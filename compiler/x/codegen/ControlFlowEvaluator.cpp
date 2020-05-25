/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "compile/VirtualGuard.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "env/CHTable.hpp"
#endif
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/BinaryCommutativeAnalyser.hpp"
#include "x/codegen/CompareAnalyser.hpp"
#include "x/codegen/FPTreeEvaluator.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg);

// The following functions are simple enough to inline, and are called often
// enough that we want to take advantage of inlining.  However, it is used
// across multiple translation units, preventing us from just using 'inline'
// TODO: Put in own header file shared by the various TreeEvaluator.cpp
// (but not TreeEvaluator.hpp, too many files include this).

// Right now, this function is duplicated in TreeEval, ControlFlowEval, Binary &UnaryEval.
inline bool getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg)
   {
   return cg->comp()->target().is64Bit() && node->getSize() > 4;
   }

// Right now, this is duplicated in ControlFlowEval, BinaryEval and TreeEval.
inline intptr_t integerConstNodeValue(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (getNodeIs64Bit(node, cg))
      {
      return node->getLongInt();
      }
   else
      {
      TR_ASSERT(node->getSize() <= 4, "For efficiency on IA32, only call integerConstNodeValue for 32-bit constants");
      return node->getInt();
      }
   }
// Right now, this is duplicated in ControlFlowEval, BinaryEval and TreeEval.
inline bool constNodeValueIs32BitSigned(TR::Node *node, intptr_t *value, TR::CodeGenerator *cg)
   {
   *value = integerConstNodeValue(node, cg);
   if (cg->comp()->target().is64Bit())
      {
      return IS_32BIT_SIGNED(*value);
      }
   else
      {
      TR_ASSERT(IS_32BIT_SIGNED(*value), "assertion failure");
      return true;
      }
   }


static inline TR::Instruction *generateWiderCompare(TR::Node *node, TR::Register *targetReg,
                                                   intptr_t value,
                                                   TR::CodeGenerator *cg)
   {
   // using 16bit immediate in a 32-bit compare instruction (0x66 length change prefix)
   // results in a decode penalty.
   // The instruction length decoder of the Intel Core processors cannot decode
   // the length of an LCP instruction in one cycle, therefore it initiates slow decoding,
   // which takes five extra cycles to complete.
   //
   // the recommended fix is to use a compare register with a 32-bit value
   // instead
   //
   generateRegRegInstruction(MOVSXReg4Reg2, node, targetReg, targetReg, cg);
   return generateRegImmInstruction(CMP4RegImm4, node, targetReg, value, cg);
   }

bool isConditionCodeSetForCompareToZero(TR::Node *node, bool justTestZeroFlag)
   {
   TR::Compilation *comp = TR::comp();
   // Disable.  Need to re-think how we handle overflow cases.
   //
   static char *disableNoTestEFlags = feGetEnv("TR_disableNoTestEFlags");
   if (disableNoTestEFlags)
      return false;

   // See if there is a previous instruction that has set the condition flags
   // properly for this node's register
   //
   if (!node->getRegister())
      return false;

   // This is neccessary because the branch instruction may rely on OF
   // e.g jl tests OF != SF
   // Since cmp r1,0 always unsets OF, the prevInstr cannot set OF
   //
   // Example:
   //   add r1, 1  (r1=0x7fffffff, OF set)
   //   cmp r1, 0  (OF unset, and this instr cannot be ommitted)
   //   jl address
   //
   // This check can be taken out if the branch instruction generated by
   // the caller won't test OF (e.g. js )
   //
   if (!node->cannotOverflow())
      return false;


   // Find the last instruction that either
   //    1) sets the appropriate condition flags, or
   //    2) modifies the register to be tested
   // (and that hopefully does both)
   //
   TR::Instruction     *prevInstr;
   TR::X86RegInstruction  *prevRegInstr;
   for (prevInstr = comp->cg()->getAppendInstruction();
        prevInstr;
        prevInstr = prevInstr->getPrev())
      {
      prevRegInstr = prevInstr->getX86RegInstruction();

      // The register must be equal and the node size must be equal in order to
      // insure the instruction is setting the condition code based on the
      // correct set of bits
      if (prevRegInstr && prevRegInstr->getTargetRegister() == node->getRegister() &&
          prevRegInstr->getNode() && prevRegInstr->getNode()->getSize() == node->getSize())
         {
         if (prevRegInstr->getOpCode().modifiesTarget())
            {
            // This instruction sets the register.
            //
            if (justTestZeroFlag)
               {
               if (!prevInstr->getOpCode().setsCCForTest())
                  return false;
               else if (prevInstr->getOpCode().isShiftOp())
                  {
                  // Only constant shift instructions should be considered.  For those instructions,
                  // the flags will only be set as required if the count != 0.
                  //
                  uint32_t count = 0;
                  if (prevInstr->getOpCode().hasByteImmediate())
                     {
                     count = ((TR::X86RegImmInstruction  *)prevRegInstr)->getSourceImmediate();
                     }

                  if (count == 0)
                     {
                     return false;
                     }
                  }
               }
            else
               {
               if (!prevInstr->getOpCode().setsCCForCompare())
                  return false;
               }

            return true;
            }
         }

      if (prevInstr->getOpCodeValue() == LABEL)
         {
         // This instruction is a possible branch target.
         return false;
         }

      if (prevInstr->getOpCode().modifiesSomeArithmeticFlags())
         {
         // This instruction overwrites the condition flags.
         return false;
         }
      }

   return false;
   }

static uint32_t sumOf2ConsecutivePowersOf2(uint32_t numberOfCases)
   {
   for (uint32_t i = 3; i < 0xc0000000; i <<= 1)
      {
      if (i == numberOfCases)
         {
         return ((i & (i-1)) >> 1) + 1;
         }
      }
   return 0;
   }

static void binarySearchCaseSpace(TR::Register *selectorReg,
                                  TR::Node     *lookupNode,
                                  uint32_t     lowChild,
                                  uint32_t     highChild,
                                  bool        &evaluateDefaultGlRegDeps,
                                  TR::CodeGenerator *cg)
   {
   uint32_t numCases  = highChild - lowChild + 1;
   TR_X86OpCodes opCode;
   uint32_t pivot;
   if ((pivot = sumOf2ConsecutivePowersOf2(numCases)) == 0)
      {
      pivot = lowChild + (numCases / 2) - 1;
      }
   else
      {
      pivot += lowChild - 1;
      }

   if (pivot >= lowChild)
      {
      int32_t pivotValue = lookupNode->getChild(pivot)->getCaseConstant();

      // while this is a signed comparison, it's valid for unsigned too
      // (if -128 <= (signed) pivot <= 0; then  (unsigned) pivot >= 0xFFFF FF80
      //  so sign extension is valid)
      if (pivotValue >= -128 && pivotValue <= 127)
         {
         opCode = CMP4RegImms;
         }
      else
         {
         opCode = CMP4RegImm4;
         }
      generateRegImmInstruction(opCode, lookupNode, selectorReg, pivotValue, cg);
      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *pivotLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      pivotLabel->setEndInternalControlFlow();
      generateLabelInstruction(LABEL, lookupNode, startLabel, cg);

      // We are guaranteed that the case children are sorted.
      // Image the Z32 number lines
      //                [------------------------|--------------------------)                                                     signed
      //   TR::getMinSigned<TR::Int32>()         0            TR::getMaxSigned<TR::Int32>()      TR::getMaxUnsigned<TR::Int32>()
      //                                         [--------------------------|------------------------------------)                unsigned
      // If all the cases of the lookup are on the same 'half' of the number lines, it doesn't matter
      // if we use JA or JG, so lets use JG.
      // For the signed case, we better use JG if the numbers straddle the 0 boundary; we already do.
      // For the unsigned case, we better use JA if the numbers straddle the TR::getMaxSigned<TR::Int32>() boundary. If that's
      // the case, the highVal will be less than (in the signed sense) than lowVal.
      //
      bool isUnsigned;
      int32_t lowVal  = lookupNode->getChild(lowChild)->getCaseConstant();
      int32_t highVal = lookupNode->getChild(highChild)->getCaseConstant();
      if (highVal < lowVal)
         isUnsigned = true;
      else
         isUnsigned = false;

      generateLabelInstruction(isUnsigned ? JA4 : JG4, lookupNode, pivotLabel, cg);

      if (lowChild == pivot)
         {
         generateJumpInstruction(JE4, lookupNode->getChild(lowChild), cg);
         generateJumpInstruction(JMP4, lookupNode->getChild(1), cg, false, evaluateDefaultGlRegDeps);
         evaluateDefaultGlRegDeps = false;
         }
      else
         {
         binarySearchCaseSpace(selectorReg, lookupNode, lowChild, pivot, evaluateDefaultGlRegDeps, cg);
         }
      generateLabelInstruction(LABEL, lookupNode, pivotLabel, cg);
      }
   else
      TR_ASSERT(pivot == lowChild - 1 && lowChild == highChild, "unexpected pivot value in binarySearchCaseSpace");

   if (highChild == pivot + 1)
      {
      int32_t  highValue = lookupNode->getChild(highChild)->getCaseConstant();

      // while this is a signed comparison, it's valid for unsigned too
      // (if -128 <= (signed) pivot <= 0; then  (unsigned) pivot >= 0xFFFF FF80
      //  so sign extension is valid)
      if (highValue >= -128 && highValue <= 127)
         {
         opCode = CMP4RegImms;
         }
      else
         {
         opCode = CMP4RegImm4;
         }
      generateRegImmInstruction(opCode, lookupNode, selectorReg, highValue, cg);

      generateJumpInstruction(JE4, lookupNode->getChild(highChild), cg);
      generateJumpInstruction(JMP4, lookupNode->getChild(1), cg, false, evaluateDefaultGlRegDeps);
      evaluateDefaultGlRegDeps = false;
      }
   else
      {
      binarySearchCaseSpace(selectorReg, lookupNode, pivot + 1, highChild, evaluateDefaultGlRegDeps, cg);
      }
   }


TR::Register *OMR::X86::TreeEvaluator::lookupEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register  *selectorReg = cg->evaluate(node->getFirstChild());
   bool evaluateDefaultGlRegDeps = true;
   TR::RealRegister::RegNum depsRegisterIndex = TR::RealRegister::NoReg;
   bool selectorRegInGlRegDeps = false;
   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t) 0, TR::RealRegister::MaxAssignableRegisters, cg);

   startLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);

   for (int i=1; i< node->getNumChildren(); i++)
      {
      TR::Node *caseChild = node->getChild(i);
      if (caseChild->getNumChildren() > 0)
         {
         TR::Node *firstCaseChild = caseChild->getFirstChild();
         if (firstCaseChild->getOpCodeValue() == TR::GlRegDeps)
            {
            for (int j = firstCaseChild->getNumChildren()-1; j >= 0; j--)
               {
               TR::Node                 *child        = firstCaseChild->getChild(j);
               TR::Register             *globalReg    = NULL;

               if (child->getOpCodeValue() == TR::PassThrough)
                  globalReg = child->getFirstChild()->getRegister();
               else
                  globalReg = child->getRegister();

               TR_GlobalRegisterNumber  globalRegNum = child->getGlobalRegisterNumber();
               TR_GlobalRegisterNumber  highGlobalRegNum = child->getHighGlobalRegisterNumber();

               if (globalReg->getKind() == TR_GPR && highGlobalRegNum < 0 &&
                   (globalReg == selectorReg))
                  {
                  depsRegisterIndex = (TR::RealRegister::RegNum) cg->getGlobalRegister(globalRegNum);
                  selectorRegInGlRegDeps = true;
                  }
               else if (globalReg->getKind() == TR_GPR || globalReg->getKind() == TR_FPR || globalReg->getKind() == TR_VRF)
                  {
                  TR::RegisterPair *globalRegPair = globalReg->getRegisterPair();
                  TR::RealRegister::RegNum registerIndex = (TR::RealRegister::RegNum) cg->getGlobalRegister(globalRegNum);
                  if (!deps->findPostCondition(registerIndex))
                     deps->addPostCondition(globalRegPair ? globalRegPair->getLowOrder() : globalReg , registerIndex, cg);
                  if (highGlobalRegNum >=0)
                     {
                     TR::RealRegister::RegNum highRegisterIndex = (TR::RealRegister::RegNum) cg->getGlobalRegister(highGlobalRegNum);
                     if (!deps->findPostCondition(highRegisterIndex))
                        deps->addPostCondition(globalRegPair->getHighOrder(), highRegisterIndex, cg);
                     }
                  }
               }
            }
         }
      }

   binarySearchCaseSpace(selectorReg, node, 2, node->getNumChildren() - 1, evaluateDefaultGlRegDeps, cg);
   cg->decReferenceCount(node->getFirstChild());

   deps->addPostCondition(selectorReg, depsRegisterIndex, cg);
   deps->stopAddingConditions();
   generateLabelInstruction(LABEL, node, endLabel, deps, cg);

   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::tableEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int32_t i;
   uint32_t numBranchTableEntries = node->getNumChildren() - 2;
   intptr_t *branchTable =
      (intptr_t*)cg->allocateCodeMemory(numBranchTableEntries * sizeof(branchTable[0]), cg->getCurrentEvaluationBlock()->isCold());

   TR::Register *selectorReg = cg->evaluate(node->getFirstChild());
   TR_X86OpCodes opCode;

   bool canSkipBoundTest = node->isSafeToSkipTableBoundCheck();

   TR::Node *secondChild = node->getSecondChild();

   if (!canSkipBoundTest)
      {
      if (numBranchTableEntries <= 127)
         {
         opCode = CMP4RegImms;
         }
      else
         {
         opCode = CMP4RegImm4;
         }

      generateRegImmInstruction(opCode, node, selectorReg, numBranchTableEntries, cg);

      // The glRegDep is hung off the default case statement.
      //
      generateJumpInstruction(JAE4, secondChild, cg, true);
      }
   else
      {
      if (secondChild->getNumChildren() >= 1)
         cg->evaluate(secondChild->getFirstChild()); // evaluate the glRegDeps
      }

   TR::MemoryReference *jumpMR = NULL;
   TR::Register *branchTableReg = NULL;
   if (cg->comp()->target().is64Bit() && cg->comp()->compileRelocatableCode())
      {
      // Generate position-independent code so that no (external) relocation is
      // necessary:
      //
      //    lea rBranchTable, [rip + OFFSET_TO_JUMP_TABLE]
      //    jmp qword ptr [rBranchTable + 8*rIndex]
      //
      TR::LabelSymbol *label = generateLabelSymbol(cg);
      label->setCodeLocation(reinterpret_cast<uint8_t*>(branchTable));
      TR::MemoryReference *branchTableLeaMR = generateX86MemoryReference(label, cg);
      branchTableReg = cg->allocateRegister();
      generateRegMemInstruction(LEA8RegMem, node, branchTableReg, branchTableLeaMR, cg);
      jumpMR = generateX86MemoryReference(branchTableReg, selectorReg, 3, cg);
      }
   else
      {
      jumpMR = generateX86MemoryReference(
         (TR::Register *)NULL,
         selectorReg,
         (uint8_t)(cg->comp()->target().is64Bit()? 3 : 2),
         (intptr_t)branchTable, cg);

      jumpMR->setNeedsCodeAbsoluteExternalRelocation();
      }

   TR::X86MemTableInstruction *jmpTableInstruction = NULL;
   TR::RegisterDependencyConditions *deps = NULL;

   // Add GlRegDep dependencies to indirect jump
   //
   if (secondChild->getNumChildren() > 0)
      {
      deps = generateRegisterDependencyConditions(secondChild->getFirstChild(), cg, 0, NULL);
      deps->stopAddingConditions();
      }

   if (cg->getLinkage()->getProperties().getMethodMetaDataRegister() != TR::RealRegister::NoReg)
      {
      jmpTableInstruction = generateMemTableInstruction(JMPMem, node, jumpMR, numBranchTableEntries, deps, cg);
      }
   else
      {
      generateMemInstruction(JMPMem, node, jumpMR, deps, cg);
      }

   for (i = 2; i < node->getNumChildren(); ++i)
      {
      TR::Node * caseNode = node->getChild(i);
      uint8_t * target = (uint8_t *)&branchTable[i-2];
      cg->addMetaDataForBranchTableAddress(target, caseNode, jmpTableInstruction);
      }

   for (i = 0; i < node->getNumChildren(); ++i)
      {
      cg->decReferenceCount(node->getChild(i));
      }

   if (branchTableReg != NULL)
      cg->stopUsingRegister(branchTableReg);

   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::minmaxEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_X86OpCodes CMP  = BADIA32Op;
   TR_X86OpCodes MOV  = BADIA32Op;
   TR_X86OpCodes CMOV = BADIA32Op;
   switch (node->getOpCodeValue())
      {
      case TR::imin:
         CMP = CMP4RegReg;
         MOV = MOV4RegReg;
         CMOV = CMOVG4RegReg;
         break;
      case TR::imax:
         CMP = CMP4RegReg;
         MOV = MOV4RegReg;
         CMOV = CMOVL4RegReg;
         break;
      case TR::lmin:
         CMP = CMP8RegReg;
         MOV = MOV8RegReg;
         CMOV = CMOVG8RegReg;
         break;
      case TR::lmax:
         CMP = CMP8RegReg;
         MOV = MOV8RegReg;
         CMOV = CMOVL8RegReg;
         break;
      default:
         TR_ASSERT(false, "INCORRECT IL OPCODE.");
         break;
      }

   auto operand0 = cg->evaluate(node->getChild(0));
   auto operand1 = cg->evaluate(node->getChild(1));
   auto result = cg->allocateRegister();
   generateRegRegInstruction(CMP,  node, operand0, operand1, cg);
   generateRegRegInstruction(MOV,  node, result,  operand0, cg);
   generateRegRegInstruction(CMOV, node, result,  operand1, cg);

   node->setRegister(result);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   return result;
   }


void
OMR::X86::TreeEvaluator::setupProfiledGuardRelocation(TR::X86RegImmInstruction *cmpInstruction,
                                                  TR::Node *node,
                                                  TR_ExternalRelocationTargetKind reloKind)
   {
#ifdef J9_PROJECT_SPECIFIC
   // The following makes sure that the TR_ProfiledInlinedMethod relocation for this inlined site will be created
   // later in TR::CodeGenerator::processRelocations()
   TR::Compilation *comp = TR::comp();
   TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
   TR_AOTGuardSite *site = comp->addAOTNOPSite();
   site->setLocation(NULL);
   site->setType(TR_ProfiledGuard);
   site->setGuard(virtualGuard);
   site->setNode(node);
   site->setAconstNode(node->getSecondChild());

   // If we've generated an instruction, then make sure it is marked to get the right kind of relocation
   if (cmpInstruction)
      {
      cmpInstruction->setReloKind(reloKind);
      cmpInstruction->setNode(node->getSecondChild());
      }
   traceMsg(comp, "setupProfiledGuardRelocation: site %p type %d node %p\n", site, site->getType(), node);
#endif
   }

void OMR::X86::TreeEvaluator::compareIntegersForEquality(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node  *secondChild = node->getSecondChild();
   // The opcode size of the compare node doesn't tell us whether we need a
   // 64-bit compare.  We need to check a child.
   bool is64Bit = TR::TreeEvaluator::getNodeIs64Bit(secondChild, cg);
   if (cg->profiledPointersRequireRelocation() &&
      secondChild->getOpCodeValue() == TR::aconst &&
      (secondChild->isMethodPointerConstant() || secondChild->isClassPointerConstant()))
      {
      TR_ASSERT(!(node->isNopableInlineGuard()),"Should not evaluate class or method pointer constants underneath NOPable guards as they are runtime assumptions handled by virtualGuardHelper");
      cg->evaluate(secondChild);
      }

   intptr_t constValue;
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL     &&
       (((secondChild->getSize() <= 2) && (!secondChild->isUnsigned())) ||
       (TR::TreeEvaluator::constNodeValueIs32BitSigned(secondChild, &constValue, cg) && !cg->constantAddressesCanChangeSize(secondChild))))
      {
      if(secondChild->getSize() <= 2)
         constValue = (intptr_t)secondChild->get64bitIntegralValue();

      // compare  (node)
      //    ?     (firstChild)
      //    const (secondChild and constValue)
      //
      TR::Node *firstChild = node->getFirstChild();

      // Extra goodies for branches
      //
      if (node->getOpCode().isIf() && constValue == 0)
         {
         TR::ILOpCodes op = node->getOpCodeValue();
         if (op == TR::ifacmpeq)
            {
            node->getFirstChild()->setIsNonNull(true);
            }
         else if (op == TR::ificmpeq || op == TR::iflcmpeq)
            {
            node->getFirstChild()->setIsNonZero(true);
            }
         }

      if (constValue >= -128 && constValue <= 127)
         {
         if (constValue == 0)
            // compare  (node)
            //    ?     (firstChild)
            //    0
            //
            {
            if (firstChild->getOpCode().isAnd() &&
                (firstChild->getRegister() == NULL) &&
                (firstChild->getReferenceCount() == 1))
               {
               // compare  (node)
               //    and   (firstChild)
               //       ?  (andFirstChild)
               //       ?  (andSecondChild)
               //    0
               //
               bool conversionSkipped = false;
               TR::Node *andFirstChild  = firstChild->getFirstChild();
               TR::Node *andSecondChild = firstChild->getSecondChild();

               //This code path cheats a bit and uses a 4-byte instruction to compare a 2-byte
               //value with zero.  Ordinarily, this wouldn't work because the top 2 bytes of
               //the value are garbage. To make this work, we must zero-extend the mask immediate
               //to 4 bytes, thereby ensuring the top two bytes of the masked value will be
               //zero and won't affect the subsequent compare with zero.

               //Therefore, counterintuitively, zero-entending the mask is always the correct thing
               //to do, regardless of whether the compare node claims to be a signed operation!
               uint64_t mask;
               if (andSecondChild->getOpCode().isLoadConst() &&
                   andSecondChild->getRegister() == NULL     &&
                   ((mask = andSecondChild->get64bitIntegralValueAsUnsigned()) >> 31) == 0) // this needs to be an unsigned value otherwise
                                                                                                      // <4 byte mask values will be sign extended
                                                                                                      // and may be used in a 4 byte compare
                  {
                  // compare     (node)
                  //    and      (firstChild)
                  //       ?     (andFirstChild)
                  //       const (andSecondChild)
                  //    0
                  //
                  if (andFirstChild->getRegister()       == NULL &&
                      andFirstChild->getReferenceCount() == 1    &&
                      andFirstChild->getOpCode().isLoadVar())
                     {
                     // memory case
                     TR::MemoryReference  *tempMR = generateX86MemoryReference(andFirstChild, cg);
                     TR_X86OpCodes testInstr;
                     if(((mask >> 8) == 0) || (andSecondChild->getSize() == 1))
                        generateMemImmInstruction(TEST1MemImm1, node, tempMR, mask, cg);
                     else if(andSecondChild->getSize() == 2)
                        {
                        TR::Register *tempReg = cg->allocateRegister();
                        TR::TreeEvaluator::loadConstant(node, mask, TR_RematerializableShort, cg, tempReg);
                        generateMemRegInstruction(TEST2MemReg, node, tempMR, tempReg, cg);
                        cg->stopUsingRegister(tempReg);
                        }
                     else
                        generateMemImmInstruction(TESTMemImm4(is64Bit), node, tempMR, mask, cg);
                     tempMR->decNodeReferenceCounts(cg);
                     }
                  else
                     {
                     //register case
                     // before evaluating the andFirstChild we check to see if it is an integral
                     // conversion that is redundant because of the constant value in the compare
                     TR::Register *tempReg = cg->evaluate(andFirstChild);
                     if (andFirstChild->getOpCode().isConversion()
                         && andFirstChild->getType().isIntegral()
                         && andFirstChild->getFirstChild()->getType().isIntegral()
                         && !andFirstChild->getRegister()
                         && andFirstChild->getSize() > andFirstChild->getFirstChild()->getSize()
                         && ((mask >> (8 * andFirstChild->getFirstChild()->getSize())) == 0))
                        {
                        tempReg = cg->evaluate(andFirstChild->getFirstChild());
                        conversionSkipped = true;
                        }
                     else
                        {
                        tempReg = cg->evaluate(andFirstChild);
                        }
                     TR_X86OpCodes testInstr;
                     if(((mask >> 8) == 0 && !andFirstChild->isInvalid8BitGlobalRegister()) ||
                        (andSecondChild->getSize() == 1))
                        testInstr = TEST1RegImm1;
                     else
                        testInstr = TESTRegImm4(is64Bit);
                     generateRegImmInstruction(testInstr, node, tempReg, mask, cg);
                     }
                  if (conversionSkipped)
                     {
                     cg->recursivelyDecReferenceCount(andFirstChild);
                     }
                  else
                     {
                     cg->decReferenceCount(andFirstChild);
                     }
                  cg->decReferenceCount(andSecondChild);
                  }
               else
                  {
                  TR_X86BinaryCommutativeAnalyser  temp(cg);
                  TR_X86OpCodes testRRInstr, testMRInstr, movRRInstr;
                  uint32_t size = firstChild->getSize();
                  if(size == 1)
                     {
                     testRRInstr = TEST1RegReg;
                     testMRInstr = TEST1MemReg;
                     movRRInstr = MOV1RegReg;
                     }
                  else if(size == 2)
                     {
                     testRRInstr = TEST2RegReg;
                     testMRInstr = TEST2MemReg;
                     movRRInstr = MOV2RegReg;
                     }
                  else
                     {
                     testRRInstr = TESTRegReg(is64Bit);
                     testMRInstr = TESTMemReg(is64Bit);
                     movRRInstr = MOVRegReg(is64Bit);
                     }
                  temp.genericAnalyser(firstChild, testRRInstr, testMRInstr, movRRInstr, true);
                  }
               }
            else
              {
              if ((firstChild->getRegister() == NULL) &&
                     (firstChild->getOpCode().isAnd() || firstChild->getOpCode().isOr() || firstChild->getOpCode().isXor()))
                  {
                  // child must be evaluated to a register but we can still make
                  // use of the conditions codes that it produces
                  cg->evaluate(firstChild);
                  }
               if (isConditionCodeSetForCompareToZero(firstChild, true))
                  {
                  // Nothing to do because previous instruction already set the
                  // condition code for the first child's register
                  }
               else
                  {
                  int compareSize = secondChild->getSize();
                  if (compareSize > 1 &&
                      (firstChild->getOpCodeValue() == TR::b2i || firstChild->getOpCodeValue() == TR::bu2i ||
                       firstChild->getOpCodeValue() == TR::b2s || firstChild->getOpCodeValue() == TR::bu2s) &&
                      firstChild->getRegister() == NULL &&
                      firstChild->getReferenceCount() == 1)
                     {
                     compareSize = 1;
                     cg->decReferenceCount(firstChild);
                     firstChild = firstChild->getFirstChild();
                     }
                  if (compareSize > 2 &&
                      (firstChild->getOpCodeValue() == TR::su2i || firstChild->getOpCodeValue() == TR::s2i) &&
                      firstChild->getRegister() == NULL &&
                      firstChild->getReferenceCount() == 1)
                     {
                     compareSize = 2;
                     cg->decReferenceCount(firstChild);
                     firstChild = firstChild->getFirstChild();
                     }
                  if (firstChild->getOpCode().isMemoryReference()  &&
                        firstChild->getRegister() == NULL &&
                        firstChild->getReferenceCount() == 1)
                     {
                     TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);
                     if (compareSize == 1)
                        generateMemImmInstruction(CMP1MemImm1, node, tempMR, 0, cg);
                     else if (compareSize == 2)
                        {
                        //shouldn't use Imm2 instructions
                        TR::Register *tempReg = cg->allocateRegister();
                        TR::TreeEvaluator::loadConstant(node, 0, TR_RematerializableShort, cg, tempReg);
                        generateMemRegInstruction(CMP2MemReg, node, tempMR, tempReg, cg);
                        cg->stopUsingRegister(tempReg);
                        }
                     else
                        TR::TreeEvaluator::compareGPMemoryToImmediate(node, tempMR, 0, cg);
                     tempMR->decNodeReferenceCounts(cg);
                     }
                  else
                     {
                     TR::Register *firstChildReg = cg->evaluate(firstChild);
                     if (isConditionCodeSetForCompareToZero(firstChild, true))
                        {
                        // Nothing to do because the evaluation of firstChild set the
                        // condition codes for us
                        }
                     else
                        {
                        if (compareSize == 1)
                           generateRegRegInstruction(TEST1RegReg, node, firstChildReg, firstChildReg, cg);
                        else if (compareSize == 2)
                           generateRegRegInstruction(TEST2RegReg, node, firstChildReg, firstChildReg, cg);
                        else
                           TR::TreeEvaluator::compareGPRegisterToImmediateForEquality(node, firstChildReg, 0, cg);
                        }
                     }
                  }
               }
            }
         else
            {
            int compareSize = secondChild->getSize();
            if (compareSize > 1 &&
                (firstChild->getOpCodeValue() == TR::b2i || firstChild->getOpCodeValue() == TR::bu2i ||
                 firstChild->getOpCodeValue() == TR::b2s || firstChild->getOpCodeValue() == TR::bu2s) &&
                firstChild->getRegister() == NULL &&
                firstChild->getReferenceCount() == 1)
               {
               compareSize = 1;
               cg->decReferenceCount(firstChild);
               firstChild = firstChild->getFirstChild();
               }
            if (compareSize > 2 &&
                (firstChild->getOpCodeValue() == TR::su2i || firstChild->getOpCodeValue() == TR::s2i) &&
                firstChild->getRegister() == NULL &&
                firstChild->getReferenceCount() == 1)
               {
               compareSize = 2;
               cg->decReferenceCount(firstChild);
               firstChild = firstChild->getFirstChild();
               }
            if (firstChild->getOpCode().isMemoryReference()  &&
                  firstChild->getRegister() == NULL &&
                  firstChild->getReferenceCount() == 1)
               {
               TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);
               if (compareSize == 1)
                  generateMemImmInstruction(CMP1MemImm1, node, tempMR, constValue, cg);
               else if (compareSize == 2)
                  {
                  //shouldn't use Imm2 instructions
                  TR::Register *tempReg = cg->allocateRegister();
                  TR::TreeEvaluator::loadConstant(node, constValue, TR_RematerializableShort, cg, tempReg);
                  generateMemRegInstruction(CMP2MemReg, node, tempMR, tempReg, cg);
                  cg->stopUsingRegister(tempReg);
                  }
               else
                  TR::TreeEvaluator::compareGPMemoryToImmediate(node, tempMR, constValue, cg);
               tempMR->decNodeReferenceCounts(cg);
               }
            else
               {
               TR::Register *firstChildReg = cg->evaluate(firstChild);
               if (compareSize == 1)
                  generateRegImmInstruction(CMP1RegImm1, node, firstChildReg, constValue, cg);
               else if (compareSize == 2)
                  {
                  ///generateRegImmInstruction(CMP2RegImm2, node, firstChildReg, constValue, cg);
                  generateWiderCompare(node, firstChildReg, constValue, cg);
                  }
               else
                  TR::TreeEvaluator::compareGPRegisterToImmediateForEquality(node, firstChildReg, constValue, cg);
               }

            if (secondChild->getOpCodeValue() == TR::aconst)
               {
               TR_ASSERT(!secondChild->isClassPointerConstant(), "Class pointer constant unexpected here\n");
               TR_ASSERT(!secondChild->isMethodPointerConstant(), "Method pointer constant unexpected here\n");
               }
            }
         }
      else
         {
         TR::Instruction *cmpInstruction = NULL;
         uint32_t size = secondChild->getSize();
         TR::Register *firstChildReg = cg->evaluate(firstChild);
         if(size == 1)
            cmpInstruction = generateRegImmInstruction(CMP1RegImm1, node, firstChildReg, constValue, cg);
         else if (size == 2)
            {
            ///cmpInstruction = generateRegImmInstruction(CMP2RegImm2, node, firstChildReg, constValue, cg);
            cmpInstruction = generateWiderCompare(node, firstChildReg, constValue, cg);
            }
         else
            {
            cmpInstruction = generateRegImmInstruction(CMPRegImm4(is64Bit), node, firstChildReg, constValue, cg);
            }
         TR::Symbol *symbol = NULL;
         if (node && secondChild->getOpCode().hasSymbolReference())
            symbol = secondChild->getSymbol();
         bool isPICCandidate = symbol ? symbol->isStatic() && symbol->isClassObject() : false;

         if (isPICCandidate && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)constValue, secondChild))
            comp->getStaticHCRPICSites()->push_front(cmpInstruction);

         if (secondChild->getOpCodeValue() == TR::aconst)
            {
            if (secondChild->isClassPointerConstant())
               {
               if (cg->profiledPointersRequireRelocation())
                  TR::TreeEvaluator::setupProfiledGuardRelocation((TR::X86RegImmInstruction *)cmpInstruction, node, TR_ClassPointer);

               if (cg->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock *) secondChild->getAddress(), comp->getCurrentMethod()) || cg->profiledPointersRequireRelocation())
                  comp->getStaticPICSites()->push_front(cmpInstruction);
               }

            if (secondChild->isMethodPointerConstant())
               {
               if (cg->profiledPointersRequireRelocation())
                  TR::TreeEvaluator::setupProfiledGuardRelocation((TR::X86RegImmInstruction *)cmpInstruction, node, TR_MethodPointer);

               if (cg->fe()->isUnloadAssumptionRequired(cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) secondChild->getAddress(), comp->getCurrentMethod())->classOfMethod(), comp->getCurrentMethod()) || cg->profiledPointersRequireRelocation())
                  comp->getStaticMethodPICSites()->push_front(cmpInstruction);
               }
            }
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);

      // If we are comparing class addresses, check whether the header is compressed on 64-bit
      // to do the right size of compare.
      //
      TR::Node *firstChild = node->getFirstChild();

      if (cg->comp()->target().is64Bit() && TR::Compiler->om.generateCompressedObjectHeaders())
         {
         if (   (firstChild->getOpCode().isLoadIndirect()
                 && firstChild->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
             || (secondChild->getOpCode().isLoadIndirect()
                 && secondChild->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef()))
            {
            is64Bit = false;
            }
         }

      uint32_t size = firstChild->getSize();
      TR_X86OpCodes cmpRRInstr, cmpRMInstr, cmpMRInstr;
      if(size == 1)
         {
         cmpRRInstr = CMP1RegReg;
         cmpRMInstr = CMP1RegMem;
         cmpMRInstr = CMP1MemReg;
         }
      else if(size == 2)
         {
         cmpRRInstr = CMP2RegReg;
         cmpRMInstr = CMP2RegMem;
         cmpMRInstr = CMP2MemReg;
         }
      else
         {
         cmpRRInstr = CMPRegReg(is64Bit);
         cmpRMInstr = CMPRegMem(is64Bit);
         cmpMRInstr = CMPMemReg(is64Bit);
         }
      temp.integerCompareAnalyser(node, cmpRRInstr, cmpRMInstr, cmpMRInstr);

      if (node->isProfiledGuard() && cg->profiledPointersRequireRelocation())
         {
         TR::Node *secondChild = node->getSecondChild();
         if (secondChild->getOpCodeValue() == TR::aconst)
            {
            if (secondChild->isClassPointerConstant())
               TR::TreeEvaluator::setupProfiledGuardRelocation(NULL, node, TR_ClassPointer);
            else if (secondChild->isMethodPointerConstant())
               TR::TreeEvaluator::setupProfiledGuardRelocation(NULL, node, TR_MethodPointer);
            }
         }
      }
   }


void OMR::X86::TreeEvaluator::compareIntegersForOrder(
   TR::Node          *node,
   TR::Node          *firstChild,
   TR::Node          *secondChild,
   TR::CodeGenerator *cg)
   {
   intptr_t constValue;

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL     &&
       TR::TreeEvaluator::constNodeValueIs32BitSigned(secondChild, &constValue, cg))
      {
      // If the constant is 0 and there is a previous instruction that has set the
      // condition codes for the first child's register, then we can omit the compare
      // instruction here.
      //
      if (constValue != 0 || !isConditionCodeSetForCompareToZero(firstChild, false))
         {
         // If the first child is a memory reference, is not already in a register,
         // and is only used here, do an in-memory comparison.
         //
         // Always evaluate spine check comparisons using registers to ensure the
         // children are available.
         //
         if (!node->getOpCode().isSpineCheck() &&
             firstChild->getOpCode().isMemoryReference()  &&
             firstChild->getRegister() == NULL &&
             firstChild->getReferenceCount() == 1)
            {
            TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);
            TR::TreeEvaluator::compareGPMemoryToImmediate(node, tempMR, constValue, cg);
            tempMR->decNodeReferenceCounts(cg);
            }
         else
            {
            TR::TreeEvaluator::compareGPRegisterToImmediate(node, cg->evaluate(firstChild), constValue, cg);
            }
         }

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      // The opcode size of the compare node doesn't tell us whether we need a
      // 64-bit compare -- we need to check a child.
      //
      bool is64Bit = TR::TreeEvaluator::getNodeIs64Bit(secondChild, cg);
      TR_X86CompareAnalyser temp(cg);
      temp.integerCompareAnalyser(
         node,
         firstChild,
         secondChild,
         false,
         CMPRegReg(is64Bit), CMPRegMem(is64Bit), CMPMemReg(is64Bit));
      }
   }

void OMR::X86::TreeEvaluator::compareIntegersForOrder(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, node->getFirstChild(), node->getSecondChild(), cg);
   }


void OMR::X86::TreeEvaluator::compare2BytesForOrder(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getSecondChild();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int32_t        value      = secondChild->getShortInt();
      TR::Node       *firstChild = node->getFirstChild();
      bool isByteValue = (value >= -128 && value <= 127);

      if ((firstChild->getReferenceCount() == 1) &&
          (firstChild->getRegister() == NULL)    &&
          firstChild->getOpCode().isMemoryReference())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);
         //try to avoid Imm2 instructions
         if(isByteValue)
            generateMemImmInstruction(CMP2MemImms, firstChild, tempMR, value, cg);
         else
            {
            TR::Register *tempReg = cg->allocateRegister();
            TR::TreeEvaluator::loadConstant(node, value, TR_RematerializableShort, cg, tempReg);
            generateMemRegInstruction(CMP2MemReg, node, tempMR, tempReg, cg);
            cg->stopUsingRegister(tempReg);
            }
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         generateWiderCompare(node, cg->evaluate(firstChild), value, cg);
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(node, CMP2RegReg, CMP2RegMem, CMP2MemReg);
      }
   }

void OMR::X86::TreeEvaluator::compareBytesForOrder(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getSecondChild();
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      TR::Node *firstChild = node->getFirstChild();
      if ((firstChild->getReferenceCount() == 1) &&
          (firstChild->getRegister() == NULL)    &&
          firstChild->getOpCode().isMemoryReference())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);
         generateMemImmInstruction(CMP1MemImm1, firstChild, tempMR, secondChild->getByte(), cg);
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         generateRegImmInstruction(CMP1RegImm1, node, cg->evaluate(firstChild), secondChild->getByte(), cg);
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(node, CMP1RegReg, CMP1RegMem, CMP1MemReg);
      }
   }


TR::Register *OMR::X86::TreeEvaluator::gotoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateJumpInstruction(JMP4, node, cg, false);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::igotoEvaluator(TR::Node* node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() <= 2, "igoto cannot have more than 2 children");
   TR::RegisterDependencyConditions *secondChildDeps = NULL;
   if (node->getNumChildren() == 2)
      {
      TR::Node* secondChild = node->getSecondChild();
      TR_ASSERT(secondChild->getOpCodeValue() == TR::GlRegDeps, "second child of a igoto should be a TR::GlRegDeps");
      cg->evaluate(secondChild);
      List<TR::Register> popRegisters(cg->trMemory());

      secondChildDeps = generateRegisterDependencyConditions(secondChild, cg, 0, &popRegisters);
      cg->decReferenceCount(secondChild);

      if (!popRegisters.isEmpty())
         {
         ListIterator<TR::Register> popRegsIt(&popRegisters);
         for (TR::Register *popRegister = popRegsIt.getFirst();
              popRegister != NULL;
              popRegister = popRegsIt.getNext())
            {
            generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
            cg->stopUsingRegister(popRegister);
            }
         }
      }

   TR::Register* jumpTargetReg  = cg->evaluate(node->getFirstChild());
   if (secondChildDeps)
      generateRegInstruction(JMPReg, node, jumpTargetReg, secondChildDeps, cg);
   else
      generateRegInstruction(JMPReg, node, jumpTargetReg, cg);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::integerReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   // Restore the default FPCW if it has been forced to single precision mode.
   //
   if (cg->enableSinglePrecisionMethods() &&
       comp->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, DOUBLE_PRECISION_ROUND_TO_NEAREST), cg), cg);
      }

   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *returnRegister = cg->evaluate(firstChild);
   const TR::X86LinkageProperties &linkageProperties = cg->getProperties();
   TR::RealRegister::RegNum machineReturnRegister =
      linkageProperties.getIntegerReturnRegister();

   TR::RegisterDependencyConditions  *dependencies = NULL;
   if (machineReturnRegister != TR::RealRegister::NoReg)
      {
      dependencies = generateRegisterDependencyConditions((uint8_t)1, 0, cg);
      dependencies->addPreCondition(returnRegister, machineReturnRegister, cg);
      dependencies->stopAddingConditions();
      }

   if (linkageProperties.getCallerCleanup())
      {
      generateInstruction(RET, node, dependencies, cg);
      }
   else
      {
      generateImmInstruction(RETImm2, node, 0, dependencies, cg);
      }

   if (comp->getMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      if (cg->comp()->target().is64Bit())
         {
         TR_ReturnInfo returnInfo;
         switch (node->getDataType())
            {
            default:
               TR_ASSERT(0, "Unrecognized return type");
               // fall through
            case TR::Int32:
               returnInfo = TR_IntReturn;
               break;
            case TR::Int64:
               returnInfo = TR_LongReturn;
               break;
            case TR::Address:
               returnInfo = TR_ObjectReturn;
               break;
            }
         comp->setReturnInfo(returnInfo);
         }
      else
         {
         comp->setReturnInfo(TR_IntReturn);
         }
      }

   cg->decReferenceCount(firstChild);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::returnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   // Restore the default FPCW if it has been forced to single precision mode.
   //
   if (cg->enableSinglePrecisionMethods() &&
       comp->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, DOUBLE_PRECISION_ROUND_TO_NEAREST), cg), cg);
      }

   if (cg->getProperties().getCallerCleanup())
      {
      generateInstruction(RET, node, cg);
      }
   else
      {
      generateImmInstruction(RETImm2, node, 0, cg);
      }

   if (comp->getMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      comp->setReturnInfo(TR_VoidReturn);
      }

   return NULL;
   }

// also handles lselect, aselect
TR::Register *OMR::X86::TreeEvaluator::iselectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *condition = node->getChild(0);
   TR::Node *trueVal   = node->getChild(1);
   TR::Node *falseVal  = node->getChild(2);

   TR::Register *falseReg = cg->evaluate(falseVal);
   bool trueValIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(trueVal, cg);
   TR::Register *trueReg  = TR::TreeEvaluator::intOrLongClobberEvaluate(trueVal, trueValIs64Bit, cg);
   if (!node->isNotCollected())
      trueReg->setContainsCollectedReference();

   // don't need to test if we're already using a compare eq or compare ne
   auto conditionOp = condition->getOpCode();
   bool longCompareOn32bit = (cg->comp()->target().is32Bit() && conditionOp.isBooleanCompare() &&
         condition->getFirstChild()->getOpCode().isLong());
   //if ((conditionOp == TR::icmpeq) || (conditionOp == TR::icmpne) || (conditionOp == TR::lcmpeq) || (conditionOp == TR::lcmpne))
   if (!longCompareOn32bit && conditionOp.isCompareForEquality() && condition->getFirstChild()->getOpCode().isIntegerOrAddress())
      {
      TR::TreeEvaluator::compareIntegersForEquality(condition, cg);
      //if ((conditionOp == TR::icmpeq) || (conditionOp == TR::lcmpeq))
      if (conditionOp.isCompareTrueIfEqual())
         generateRegRegInstruction(CMOVNERegReg(trueValIs64Bit), node, trueReg, falseReg, cg);
      else
         generateRegRegInstruction(CMOVERegReg(trueValIs64Bit), node, trueReg, falseReg, cg);
      }
   else if (!longCompareOn32bit && conditionOp.isCompareForOrder() && condition->getFirstChild()->getOpCode().isIntegerOrAddress())
      {
      TR::TreeEvaluator::compareIntegersForOrder(condition, cg);
      generateRegRegInstruction((conditionOp.isCompareTrueIfEqual()) ?
                   ((conditionOp.isCompareTrueIfGreater()) ? CMOVLRegReg(trueValIs64Bit) : CMOVGRegReg(trueValIs64Bit)) :
                   ((conditionOp.isCompareTrueIfGreater()) ? CMOVLERegReg(trueValIs64Bit) : CMOVGERegReg(trueValIs64Bit)), node, trueReg, falseReg, cg);
      }
   else
      {
      TR::Register *condReg  = cg->evaluate(condition);
      generateRegRegInstruction(TEST4RegReg, node, condReg, condReg, cg); // condition is always an int
      generateRegRegInstruction(CMOVERegReg(trueValIs64Bit), node, trueReg, falseReg, cg);
      }

   if (node->getOpCodeValue() == TR::bselect &&
       cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(trueReg);

   node->setRegister(trueReg);
   cg->decReferenceCount(condition);
   cg->decReferenceCount(trueVal);
   cg->decReferenceCount(falseVal);

   return trueReg;
   }

TR::Register *OMR::X86::TreeEvaluator::integerIfCmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   // Check for the special case of a BigDecimal long lookaside overflow check.
   //
   if (node->getFirstChild()->getOpCodeValue() == TR::icall &&
       node->getSecondChild()->getOpCodeValue() == TR::iconst)
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      TR::MethodSymbol *symbol = firstChild->getSymbol()->castToMethodSymbol();

      if (cg->getSupportsBDLLHardwareOverflowCheck() &&
          (symbol->getRecognizedMethod() == TR::java_math_BigDecimal_noLLOverflowAdd ||
           symbol->getRecognizedMethod() == TR::java_math_BigDecimal_noLLOverflowMul))
         {
         cg->evaluate(firstChild);
         cg->evaluate(secondChild);

         generateConditionalJumpInstruction(JO4, node, cg, true);

         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);

         traceMsg(cg->comp(), "inserting long lookaside versioning overflow check @ node %p\n", node);

         return NULL;
         }
      }
#endif

   if ( node->isTheVirtualGuardForAGuardedInlinedCall())
      {
      TR::Node *firstChild = node->getFirstChild();
      cg->evaluate(firstChild);
      }

   TR::TreeEvaluator::compareIntegersForEquality(node, cg);

   generateConditionalJumpInstruction(JE4, node, cg, true);
   return NULL;
   }

static inline void generateMergedGuardCodeIfNeeded(TR::Node *node, TR::CodeGenerator *cg, TR::Instruction *runtimeGuard)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (node->isTheVirtualGuardForAGuardedInlinedCall() && cg->getSupportsVirtualGuardNOPing())
      {
      TR_VirtualGuard *virtualGuard = cg->comp()->findVirtualGuardInfo(node);

      if (virtualGuard && (virtualGuard->mergedWithOSRGuard() || virtualGuard->mergedWithHCRGuard()))
         {
         TR_VirtualGuardSite *site = virtualGuard->addNOPSite();
         TR::LabelSymbol *label = node->getBranchDestination()->getNode()->getLabel();

         TR::Instruction *vgnopInstr;
         if (runtimeGuard->getDependencyConditions())
            vgnopInstr = generateVirtualGuardNOPInstruction(runtimeGuard->getPrev(), node, site, runtimeGuard->getDependencyConditions()->clone(cg, 0), label, cg);
         else
            vgnopInstr = generateVirtualGuardNOPInstruction(runtimeGuard->getPrev(), node, site, NULL, label, cg);
         }
      }
#endif
   }


TR::Register *OMR::X86::TreeEvaluator::integerIfCmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (virtualGuardHelper(node, cg))
      {
      return NULL;
      }
   else
      {
#ifdef J9_PROJECT_SPECIFIC
      // Check for the special case of a BigDecimal long lookaside overflow check.
      //
      if (node->getFirstChild()->getOpCodeValue() == TR::icall &&
          node->getSecondChild()->getOpCodeValue() == TR::iconst)
         {
         TR::Node *firstChild = node->getFirstChild();
         TR::Node *secondChild = node->getSecondChild();

         TR::MethodSymbol *symbol = firstChild->getSymbol()->castToMethodSymbol();

         if (cg->getSupportsBDLLHardwareOverflowCheck() &&
             (symbol->getRecognizedMethod() == TR::java_math_BigDecimal_noLLOverflowAdd ||
              symbol->getRecognizedMethod() == TR::java_math_BigDecimal_noLLOverflowMul))
            {
            cg->evaluate(firstChild);
            cg->evaluate(secondChild);

            generateConditionalJumpInstruction(JNO4, node, cg, true);

            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);

            traceMsg(comp, "inserting long lookaside versioning overflow check @ node %p\n", node);

            return NULL;
            }
         }
#endif

      if ( node->isTheVirtualGuardForAGuardedInlinedCall())
         {
         TR::Node *firstChild = node->getFirstChild();
         cg->evaluate(firstChild);
         }

     if ( node->getFirstChild()->getOpCodeValue() == TR::ishr &&
            node->getFirstChild()->getRegister() == NULL &&
            node->getFirstChild()->getReferenceCount() == 1 &&
            (node->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::iloadi || node->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::iload) &&
            node->getFirstChild()->getSecondChild()->getOpCodeValue() == TR::iconst &&
            node->getSecondChild()->getOpCodeValue() == TR::iconst && node->getSecondChild()->getInt() == 0 &&
            performTransformation(comp, "O^O SHIFT PEEPHOLE: detected shift pattern for node %p shifting so mask = %p shift amount = %d \n",
                node, (((int32_t)-1) << node->getFirstChild()->getSecondChild()->getInt()), node->getFirstChild()->getSecondChild()->getInt())
          ){
          TR::Node     *loadNode = node->getFirstChild()->getFirstChild();
          TR::Register *loadReg  = loadNode->getRegister();

          if (loadReg)
             {
             generateRegImmInstruction(TEST4RegImm4, node, loadReg, ( ((int32_t)-1) << node->getFirstChild()->getSecondChild()->getInt() ), cg);
             }
          else
             {
             TR::MemoryReference  *sourceMR = generateX86MemoryReference(loadNode, cg);
             generateMemImmInstruction(TEST4MemImm4, node, sourceMR, ( ((int32_t)-1) << node->getFirstChild()->getSecondChild()->getInt() ), cg);
             }

         TR::X86LabelInstruction *instr = generateConditionalJumpInstruction(JNE4, node, cg, true);
         generateMergedGuardCodeIfNeeded(node, cg, instr);

         cg->recursivelyDecReferenceCount(node->getFirstChild());
         cg->decReferenceCount(node->getSecondChild());
         return NULL;
         }

      TR::TreeEvaluator::compareIntegersForEquality(node, cg);

      // If this is a guard that has not been NOPed, then
      // it might need to be registered in our internal data structures
      //
      // Disabled code below because this is now being handled in compareIntegersForEquality
      // Below test was risky as the append instruction might not be the instruction we want
      //
      //if (node->isTheVirtualGuardForAGuardedInlinedCall())
      //         {
      //         TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);
      //         if (virtualGuard &&
      //             (virtualGuard->getTestType() == TR_VftTest) &&
      //             !TR::Compiler->cls.sameClassLoaders(comp, virtualGuard->getThisClass(), comp->getCurrentMethod()->classOfMethod()))
      //            {
      //            TR::Instruction *guardInstr = comp->cg()->getAppendInstruction();
      //            comp->getStaticPICSites()->add(guardInstr);
      //            }
      //         }

      TR::X86LabelInstruction *instr = generateConditionalJumpInstruction(JNE4, node, cg, true);
      generateMergedGuardCodeIfNeeded(node, cg, instr);

      return NULL;
      }
   }

bool OMR::X86::TreeEvaluator::generateIAddOrSubForOverflowCheck(TR::Node *compareNode, TR::CodeGenerator *cg)
   {
   TR_ArithmeticOverflowCheckNodes u = {0};
   bool matches = TR::TreeEvaluator::nodeIsIArithmeticOverflowCheck(compareNode, &u);
   if (matches
      && (u.operationNode->getOpCode().isAdd() || u.operationNode->getOpCode().isSub())
      && (u.leftChild->getReferenceCount() >= 1)
      && (u.rightChild->getReferenceCount() >= 1)
      && performTransformation(cg->comp(), "O^O OVERFLOW CHECK RECOGNITION: Recognizing %s\n", cg->getDebug()->getName(compareNode)))
      {
      TR::Register *rightReg = cg->evaluate(u.rightChild);
      // leftChild might appear twice in this tree, and we need a clobber evaluate only if it also appears elsewhere
      bool leftNeedsCopy = u.leftChild->getReferenceCount() > 2 || (u.leftChild->getReferenceCount() > 1 && u.operationNode->getRegister());
      TR::Register *leftReg  = leftNeedsCopy ? cg->intClobberEvaluate(u.leftChild) : cg->evaluate(u.leftChild);
      TR_X86OpCodes opCode = u.operationNode->getOpCode().isAdd()? ADD4RegReg : SUB4RegReg;
      generateRegRegInstruction(opCode, u.operationNode, leftReg, rightReg, cg);
      if (!u.operationNode->getRegister())
         {
         // We actually evaluated the operationNode here so we have to do everything its evaluator would have done
         u.operationNode->setRegister(leftReg);
         cg->decReferenceCount(u.leftChild);
         cg->decReferenceCount(u.rightChild);
         }
      else
         {
         cg->stopUsingRegister(leftReg);
         }

      cg->recursivelyDecReferenceCount(compareNode->getFirstChild());
      cg->recursivelyDecReferenceCount(compareNode->getSecondChild());
      // Note: GlRegDeps, if any, is decremented elsewhere

      return true;
      }
   return false;
   }

bool OMR::X86::TreeEvaluator::generateLAddOrSubForOverflowCheck(TR::Node *compareNode, TR::CodeGenerator *cg)
   {
   TR_ArithmeticOverflowCheckNodes u = {0};
   bool matches = TR::TreeEvaluator::nodeIsLArithmeticOverflowCheck(compareNode, &u);
   if (matches
      && (u.operationNode->getOpCode().isAdd() || u.operationNode->getOpCode().isSub())
      && (u.leftChild->getReferenceCount() >= 1)
      && (u.rightChild->getReferenceCount() >= 1)
      && performTransformation(cg->comp(), "O^O OVERFLOW CHECK RECOGNITION: Recognizing %s\n", cg->getDebug()->getName(compareNode)))
      {
      TR::Register *rightReg = cg->evaluate(u.rightChild);
      // leftChild might appear twice in this tree, and we need a clobber evaluate only if it also appears elsewhere
      bool leftNeedsCopy = u.leftChild->getReferenceCount() > 2 || (u.leftChild->getReferenceCount() > 1 && u.operationNode->getRegister());
      TR::Register *leftReg  = leftNeedsCopy? cg->longClobberEvaluate(u.leftChild) : cg->evaluate(u.leftChild);
      if (cg->comp()->target().is64Bit())
         {
         TR_X86OpCodes opCode = u.operationNode->getOpCode().isAdd()? ADD8RegReg : SUB8RegReg;
         generateRegRegInstruction(opCode, u.operationNode, leftReg, rightReg, cg);
         }
      else if (u.operationNode->getOpCode().isAdd())
         {
         generateRegRegInstruction(ADD4RegReg, u.operationNode, leftReg->getLowOrder(),  rightReg->getLowOrder(),  cg);
         generateRegRegInstruction(ADC4RegReg, u.operationNode, leftReg->getHighOrder(), rightReg->getHighOrder(), cg);
         }
      else
         {
         generateRegRegInstruction(SUB4RegReg, u.operationNode, leftReg->getLowOrder(),  rightReg->getLowOrder(),  cg);
         generateRegRegInstruction(SBB4RegReg, u.operationNode, leftReg->getHighOrder(), rightReg->getHighOrder(), cg);
         }
      if (!u.operationNode->getRegister())
         {
         // We actually evaluated the operationNode here so we have to do everything its evaluator would have done
         u.operationNode->setRegister(leftReg);
         cg->decReferenceCount(u.leftChild);
         cg->decReferenceCount(u.rightChild);
         }

      cg->recursivelyDecReferenceCount(compareNode->getFirstChild());
      cg->recursivelyDecReferenceCount(compareNode->getSecondChild());
      // Note: GlRegDeps, if any, is decremented elsewhere

      return true;
      }
   return false;
   }

TR::Register *OMR::X86::TreeEvaluator::integerIfCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (TR::TreeEvaluator::getNodeIs64Bit(node, cg)?
         TR::TreeEvaluator::generateLAddOrSubForOverflowCheck(node, cg)
       : TR::TreeEvaluator::generateIAddOrSubForOverflowCheck(node, cg))
      {
      generateConditionalJumpInstruction(JO4, node, cg, true);
      }
   else
      {
      TR::TreeEvaluator::compareIntegersForOrder(node, cg);
      generateConditionalJumpInstruction(JL4, node, cg, true);
      }
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::integerIfCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (TR::TreeEvaluator::getNodeIs64Bit(node, cg)?
         TR::TreeEvaluator::generateLAddOrSubForOverflowCheck(node, cg)
       : TR::TreeEvaluator::generateIAddOrSubForOverflowCheck(node, cg))
      {
      generateConditionalJumpInstruction(JNO4, node, cg, true);
      }
   else
      {
      TR::TreeEvaluator::compareIntegersForOrder(node, cg);
      generateConditionalJumpInstruction(JGE4, node, cg, true);
      }
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::integerIfCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   generateConditionalJumpInstruction(JG4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::integerIfCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   generateConditionalJumpInstruction(JLE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerIfCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   generateConditionalJumpInstruction(JB4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerIfCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   generateConditionalJumpInstruction(JAE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerIfCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   generateConditionalJumpInstruction(JA4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerIfCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   generateConditionalJumpInstruction(JBE4, node, cg, true);
   return NULL;
   }


// also handles ifbcmpne
TR::Register *OMR::X86::TreeEvaluator::ifbcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {


   TR::Node *secondChild = node->getSecondChild();
   bool reverseBranch = false;
   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int32_t      value            = (int32_t)secondChild->get64bitIntegralValue();
      TR::Node     *firstChild       = node->getFirstChild();
      if ((firstChild->getReferenceCount() == 1) &&
          (firstChild->getRegister() == NULL)    &&
          firstChild->getOpCode().isMemoryReference())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);
         generateMemImmInstruction(CMP1MemImm1, firstChild, tempMR, value, cg);
         tempMR->decNodeReferenceCounts(cg);
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      else if (firstChild->getOpCode().isAnd() &&
               firstChild->getReferenceCount() == 1 &&
               firstChild->getRegister() == NULL &&     // TODO: we can still do better in this case
               firstChild->getSecondChild()->getOpCode().isLoadConst() &&
               (value == 0 ||
                (value == (int32_t)firstChild->getSecondChild()->get64bitIntegralValue() &&
                 isPowerOf2(value&0xff))))
         {
         // ifXcmp[eq|ne]
         //   Xand
         //     expr
         //     Xconst k2
         //   Xconst k1
         // where either k1 is 0, or k1 == k2 and tests only a single bit (need to reverse the branch)
         TR::Node *expr = firstChild->getFirstChild();
         TR::Node *k2   = firstChild->getSecondChild();

         if (value != 0) reverseBranch = true; // k1
         value = (int32_t)k2->get64bitIntegralValue(); // k2

         if (expr->getReferenceCount() == 1 &&
             expr->getRegister() == NULL &&
             expr->getOpCode().isMemoryReference())
            {
            TR::MemoryReference *tempMR = generateX86MemoryReference(expr, cg);
            generateMemImmInstruction(TEST1MemImm1, expr, tempMR, value, cg);
            tempMR->decNodeReferenceCounts(cg);
            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
            cg->decReferenceCount(expr);
            cg->decReferenceCount(k2);
            }
         else
            {
            TR::Register *targetRegister = cg->evaluate(expr);
            generateRegImmInstruction(TEST1RegImm1, node, targetRegister, value, cg);
            cg->recursivelyDecReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
            }
         }
      else
         {
         TR::Register *targetRegister   = cg->evaluate(firstChild);
         if (value == 0)
            {
            generateRegRegInstruction(TEST1RegReg, node, targetRegister, targetRegister, cg);
            }
         else
            {
            generateRegImmInstruction(CMP1RegImm1, node, targetRegister, value, cg);
            }
         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(node, CMP1RegReg, CMP1RegMem, CMP1MemReg);
      }

   TR_X86OpCodes opCode;
   if (node->getOpCodeValue() == TR::ifbcmpeq)
      opCode = reverseBranch ? JNE4 : JE4;
   else
      opCode = reverseBranch ? JE4 : JNE4;

   generateConditionalJumpInstruction(opCode, node, cg, true);
   return NULL;
   }

// ifbcmpneEvaluator handled by ifbcmpeqEvaluator

TR::Register *OMR::X86::TreeEvaluator::ifbcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JL4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifbucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JB4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifbcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JGE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifbucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JAE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifbcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JG4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifbucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JA4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifbcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JLE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifbucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   generateConditionalJumpInstruction(JBE4, node, cg, true);
   return NULL;
   }

// also handles ifscmpne
TR::Register *OMR::X86::TreeEvaluator::ifscmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *secondChild = node->getSecondChild();
   if (secondChild->getOpCodeValue() == TR::sconst &&
       secondChild->getRegister() == NULL)
      {
      int32_t      value            = secondChild->getShortInt();
      TR::Node     *firstChild       = node->getFirstChild();

      if ((firstChild->getReferenceCount() == 1) &&
          (firstChild->getRegister() == NULL)    &&
          firstChild->getOpCode().isMemoryReference())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(firstChild, cg);

         if (value >= -128 && value <= 127)
            {
            generateMemImmInstruction(CMP2MemImms, firstChild, tempMR, value, cg);
            }
         else
            {
            //try to avoid Imm2 instructions
            TR::Register *tempReg = cg->allocateRegister();
            TR::TreeEvaluator::loadConstant(node, value, TR_RematerializableShort, cg, tempReg);
            generateMemRegInstruction(CMP2MemReg, node, tempMR, tempReg, cg);
            cg->stopUsingRegister(tempReg);
            }
         tempMR->decNodeReferenceCounts(cg);
         }
      else
         {
         TR::Register *targetRegister   = cg->evaluate(firstChild);
         if (value >= -128 && value <= 127)
            {
            if (value == 0)
               {
               generateRegRegInstruction(TEST2RegReg, node, targetRegister, targetRegister, cg);
               }
            else
               {
               generateRegImmInstruction(CMP2RegImms, node, targetRegister, value, cg);
               }
            }
         else
            {
            ///generateRegImmInstruction(CMP2RegImm2, node, targetRegister, value, cg);
            generateWiderCompare(node, targetRegister, value, cg);
            }
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(node, CMP2RegReg, CMP2RegMem, CMP2MemReg);
      }
   generateConditionalJumpInstruction(node->getOpCodeValue() == TR::ifscmpeq ? JE4 : JNE4,
                                      node, cg, true);
   return NULL;
   }

// ifscmpneEvaluator handled by ifscmpeqEvaluator

TR::Register *OMR::X86::TreeEvaluator::ifscmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JL4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifscmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JGE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifscmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JG4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifscmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JLE4, node, cg, true);
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::ifsucmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForEquality(node, cg);
   generateConditionalJumpInstruction(node->getOpCodeValue() == TR::ifsucmpeq ? JE4 : JNE4,
                                      node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifsucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JB4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifsucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JAE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifsucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JA4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ifsucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateConditionalJumpInstruction(JBE4, node, cg, true);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::integerEqualityHelper(TR::Node *node, TR_X86OpCodes setOp, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForEquality(node, cg);
   TR::Register *targetRegister = cg->allocateRegister();
   generateRegInstruction(setOp, node, targetRegister, cg);

   generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   node->setRegister(targetRegister);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::integerCmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerEqualityHelper(node, SETE1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::integerCmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerEqualityHelper(node, SETNE1Reg, cg);
   }


TR::Register *OMR::X86::TreeEvaluator::integerOrderHelper(TR::Node          *node,
                                                     TR_X86OpCodes    setOp,
                                                     TR::CodeGenerator *cg)
   {
   TR::Register  *targetRegister = cg->allocateRegister();
   node->setRegister(targetRegister);
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   generateRegInstruction(setOp, node, targetRegister, cg);

   generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::integerCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETL1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::integerCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETGE1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::integerCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETG1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::integerCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETLE1Reg, cg);
   }


TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerCmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETB1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerCmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETAE1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerCmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETA1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::unsignedIntegerCmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerOrderHelper(node, SETBE1Reg, cg);
   }



// also handles bcmpne
TR::Register *OMR::X86::TreeEvaluator::bcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Node     *secondChild    = node->getSecondChild();

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int32_t      value            = secondChild->getByte();
      TR::Node     *firstChild       = node->getFirstChild();
      TR::Register *testRegister     = cg->evaluate(firstChild);
      if (value == 0)
         {
         generateRegRegInstruction(TEST1RegReg, node, testRegister, testRegister, cg);
         }
      else
         {
         generateRegImmInstruction(CMP1RegImm1, node, testRegister, value, cg);
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(node, CMP1RegReg, CMP1RegMem, CMP1MemReg);
      }
   bool isEq = node->getOpCodeValue() == TR::bcmpeq;
   generateRegInstruction(isEq ? SETE1Reg : SETNE1Reg,
                          node, targetRegister, cg);
   generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
   node->setRegister(targetRegister);
   return targetRegister;
   }

// bcmpneEvaluator handled by bcmpeqEvaluator

TR::Register *OMR::X86::TreeEvaluator::bcmpEvaluator(TR::Node        *node,
                                                 TR_X86OpCodes  setOp,
                                                 TR::CodeGenerator *cg)
   {
   TR::Register  *targetRegister = cg->allocateRegister();
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   node->setRegister(targetRegister);
   generateRegInstruction(setOp, node, targetRegister, cg);
   generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::bcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bcmpEvaluator(node, SETL1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::bcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bcmpEvaluator(node, SETGE1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::bcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bcmpEvaluator(node, SETG1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::bcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::bcmpEvaluator(node, SETLE1Reg, cg);
   }

// also handles scmpne
TR::Register *OMR::X86::TreeEvaluator::scmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Node     *secondChild    = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int32_t      value            = secondChild->getShortInt();
      TR::Node     *firstChild       = node->getFirstChild();
      TR::Register *testRegister     = cg->evaluate(firstChild);
      if (value >= -128 && value <= 127)
         {
         if (value == 0)
            {
            generateRegRegInstruction(TEST2RegReg, node, testRegister, testRegister, cg);
            }
         else
            {
            generateRegImmInstruction(CMP2RegImms, node, testRegister, value, cg);
            }
         }
      else
         {
         ///generateRegImmInstruction(CMP2RegImm2, node, testRegister, value, cg);
         generateWiderCompare(node, testRegister, value, cg);
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(node, CMP2RegReg, CMP2RegMem, CMP2MemReg);
      }
   node->setRegister(targetRegister);

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   generateRegInstruction(node->getOpCodeValue() == TR::scmpeq ? SETE1Reg : SETNE1Reg,
                          node, targetRegister, cg);
   generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
   return targetRegister;
   }

// scmpneEvaluator handled by scmpeqEvaluator

TR::Register *OMR::X86::TreeEvaluator::cmp2BytesEvaluator(TR::Node        *node,
                                                      TR_X86OpCodes  setOp,
                                                      TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateRegister();
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   generateRegInstruction(setOp, node, targetRegister, cg);
   generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   return node->setRegister(targetRegister);;
   }

TR::Register *OMR::X86::TreeEvaluator::scmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETL1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::scmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETGE1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::scmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETG1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::scmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETLE1Reg, cg);
   }


TR::Register *OMR::X86::TreeEvaluator::sucmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Node     *secondChild    = node->getSecondChild();

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      int32_t      value            = secondChild->getShortInt();
      TR::Node     *firstChild       = node->getFirstChild();
      TR::Register *testRegister     = cg->evaluate(firstChild);
      if (value >= -128 && value <= 127)
         {
         if (value == 0)
            {
            generateRegRegInstruction(TEST2RegReg, node, testRegister, testRegister, cg);
            }
         else
            {
            generateRegImmInstruction(CMP2RegImms, node, testRegister, value, cg);
            }
         }
      else
         {
         ///generateRegImmInstruction(CMP2RegImm2, node, testRegister, value, cg);
         generateWiderCompare(node, testRegister, value, cg);
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else
      {
      TR_X86CompareAnalyser  temp(cg);
      temp.integerCompareAnalyser(node, CMP2RegReg, CMP2RegMem, CMP2MemReg);
      }
   node->setRegister(targetRegister);

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);

   generateRegInstruction(node->getOpCodeValue() == TR::sucmpeq ? SETE1Reg : SETNE1Reg, node, targetRegister, cg);
   generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::sucmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETB1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::sucmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETAE1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::sucmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETA1Reg, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::sucmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::cmp2BytesEvaluator(node, SETBE1Reg, cg);
   }

static bool virtualGuardHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC

   if (!cg->willGenerateNOPForVirtualGuard(node))
      {
      return false;
      }

   TR::Compilation *comp = cg->comp();
   TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(node);

   TR_VirtualGuardSite *site = NULL;
   if (cg->needClassAndMethodPointerRelocations())
      {
      site = (TR_VirtualGuardSite *)comp->addAOTNOPSite();
      TR_AOTGuardSite *aotSite = (TR_AOTGuardSite *)site;
      aotSite->setType(virtualGuard->getKind());
      aotSite->setNode(node);

      int32_t reloKind = virtualGuard->getKind();
      switch (reloKind)
         {
         case TR_DirectMethodGuard:
         case TR_NonoverriddenGuard:
         case TR_InterfaceGuard:
         case TR_MethodEnterExitGuard:
         case TR_HCRGuard:
         case TR_AbstractGuard:
            aotSite->setGuard(virtualGuard);
            break;

         case TR_ProfiledGuard:
            break;

         default:
            TR_ASSERT_FATAL(0, "AOT guard in node but not one of known guards supported for AOT. Guard: %d", reloKind);
            break;
         }
      }
   else if (!node->isSideEffectGuard())
      {
      site = virtualGuard->addNOPSite();
      }
   else
      {
      site = comp->addSideEffectNOPSite();
      }

   List<TR::Register> popRegisters(cg->trMemory());
   TR::RegisterDependencyConditions  *deps = 0;
   if (node->getNumChildren() == 3)
      {
      TR::Node *third = node->getChild(2);
      cg->evaluate(third);
      deps = generateRegisterDependencyConditions(third, cg, 1, &popRegisters);
      deps->stopAddingConditions();
      }

   if(virtualGuard->shouldGenerateChildrenCode())
      cg->evaluateChildrenWithMultipleRefCount(node);

   TR::LabelSymbol *label = node->getBranchDestination()->getNode()->getLabel();

   TR::Instruction *vgnopInstr = generateVirtualGuardNOPInstruction(node, site, deps, label, cg);
   TR::Instruction *patchPoint = cg->getVirtualGuardForPatching(vgnopInstr);

   // Guards patched when the threads are stopped have no issues with multithreaded patching.
   // therefore alignment is not required
   if (cg->comp()->target().isSMP() && !node->isStopTheWorldGuard())
      {
      // the compiler is now capable of generating a train of vgnops all looking to patch the
      // same point with different constraints. alignment is required before the delegated patch
      // instruction if this collaborative merging is to work
      // we only want one alignment instruction because the instructions always try to move
      // next to their target instruction and we can end up with an infinite loop if two or more
      // dual. for now we know that all patch points have the same alignment requirement so just
      // delegate the alignment generation
      if (vgnopInstr == patchPoint)
         {
         generatePatchableCodeAlignmentInstruction(TR::X86PatchableCodeAlignmentInstruction::spinLoopAtomicRegions, vgnopInstr, cg);
         }
      }

   cg->recursivelyDecReferenceCount(node->getFirstChild());
   cg->recursivelyDecReferenceCount(node->getSecondChild());

   if (deps)
      {
      deps->setMayNeedToPopFPRegisters(true);
      }

   if (!popRegisters.isEmpty())
      {
      ListIterator<TR::Register> popRegsIt(&popRegisters);
      for (TR::Register *popRegister = popRegsIt.getFirst(); popRegister != NULL; popRegister = popRegsIt.getNext())
         {
         generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
         cg->stopUsingRegister(popRegister);
         }
      }

   return true;
#else
   return false;
#endif
   }

void
OMR::X86::CodeGenerator::addMetaDataForBranchTableAddress(
      uint8_t *target,
      TR::Node *caseNode,
      TR::X86MemTableInstruction *jmpTableInstruction)
   {

   self()->addExternalRelocation(new (self()->trHeapMemory()) TR::ExternalRelocation(target, 0, TR_AbsoluteMethodAddress, self()),
                           __FILE__, __LINE__, caseNode->getBranchDestination()->getNode());

   TR::LabelSymbol *label = caseNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelRelocation *relocation = new (self()->trHeapMemory()) TR::LabelAbsoluteRelocation(target, label);
   self()->addRelocation(relocation);

   if (jmpTableInstruction)
      jmpTableInstruction->addRelocation(relocation);

   }
