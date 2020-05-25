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

#include "x/codegen/FPTreeEvaluator.hpp"

#include <stdint.h>
#include <stdio.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/FPBinaryArithmeticAnalyser.hpp"
#include "x/codegen/FPCompareAnalyser.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/RegisterRematerialization.hpp"
#include "x/codegen/X86FPConversionSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/X86Register.hpp"

namespace TR { class Instruction; }

// Only nodes that are referenced more than this number of times
// will be rematerializable.
//
#define REMATERIALIZATION_THRESHOLD 1

// Prototypes

TR::Register *OMR::X86::TreeEvaluator::coerceFPRToXMMR(TR::Node *node, TR::Register *fpRegister, TR::CodeGenerator *cg)
   {
   TR_ASSERT(fpRegister && fpRegister->getKind() == TR_X87, "incorrect register type for XMMR coercion\n");

   TR::Register *xmmRegister = cg->allocateRegister(TR_FPR);

   if (fpRegister->isSinglePrecision())
      {
      xmmRegister->setIsSinglePrecision();
      TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(TR::Float);
      generateFPMemRegInstruction(FSTMemReg, node, tempMR, fpRegister, cg);
      generateRegMemInstruction(MOVSSRegMem, node, xmmRegister, generateX86MemoryReference(*tempMR, 0, cg), cg);
      }
   else
      {
      TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(TR::Double);
      generateFPMemRegInstruction(DSTMemReg, node, tempMR, fpRegister, cg);
      generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, xmmRegister, generateX86MemoryReference(*tempMR, 0, cg), cg);
      }

   cg->stopUsingRegister(fpRegister);

   node->setRegister(xmmRegister);
   return xmmRegister;
   }


void OMR::X86::TreeEvaluator::coerceFPOperandsToXMMRs(TR::Node *node, TR::CodeGenerator *cg)
   {
   for (int i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node     *child = node->getChild(i);
      TR::Register *reg   = child->getRegister();

      if (reg && reg->getKind() == TR_X87 /* && child->getReferenceCount() > 1 */)
         {
         TR::TreeEvaluator::coerceFPRToXMMR(child, reg, cg);
         }
      }
   }


TR::Register *OMR::X86::TreeEvaluator::coerceXMMRToFPR(TR::Node *node, TR::Register *xmmRegister, TR::CodeGenerator *cg)
   {
   TR_ASSERT(xmmRegister && xmmRegister->getKind() == TR_FPR, "incorrect register type for FPR coercion\n");

   TR::Register *fpRegister;

   if (xmmRegister->isSinglePrecision())
      {
      fpRegister = cg->allocateSinglePrecisionRegister(TR_X87);
      TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(TR::Float);
      generateMemRegInstruction(MOVSSMemReg, node, tempMR, xmmRegister, cg);
      generateFPRegMemInstruction(FLDRegMem, node, fpRegister, generateX86MemoryReference(*tempMR, 0, cg), cg);
      }
   else
      {
      fpRegister = cg->allocateRegister(TR_X87);
      TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(TR::Double);
      generateMemRegInstruction(MOVSDMemReg, node, tempMR, xmmRegister, cg);
      generateFPRegMemInstruction(DLDRegMem, node, fpRegister, generateX86MemoryReference(*tempMR, 0, cg), cg);
      }

   return fpRegister;
   }

void OMR::X86::TreeEvaluator::insertPrecisionAdjustment(TR::Register      *reg,
                                                    TR::Node          *root,
                                                    TR::CodeGenerator *cg)
   {
   TR::DataType    dt;
   TR_X86OpCodes  opStore, opLoad;
   TR::Node        *node = root;

   bool            useFloatSet = true;

   if (node->getOpCode().isBooleanCompare())
      {
      node = root->getFirstChild();
      }

   if ((node->getOpCode().isDouble() && (node->getOpCodeValue() != TR::f2d)) ||
       (node->getOpCode().isBooleanCompare() && node->getFirstChild()->getDataType() != TR::Float) ||
       node->getOpCodeValue() == TR::d2i ||
       node->getOpCodeValue() == TR::d2l)
      {
      useFloatSet = false;
      }

#ifdef DEBUG
   else if (!(node->getOpCodeValue() == TR::f2d) &&
            !(node->getOpCode().isFloat()) &&
            !(node->getOpCodeValue() == TR::f2i) &&
            !(node->getOpCodeValue() == TR::f2l))
      {
      diagnostic("insertPrecisionAdjustment() ==> invalid node type for precision adjustment!");
      }
#endif

   if (useFloatSet)
      {
      opStore = FSTPMemReg;
      opLoad = FLDRegMem;
      dt = TR::Float;
      }
   else
      {
      opStore = DSTPMemReg;
      opLoad = DLDRegMem;
      dt = TR::Double;
      }

   TR::MemoryReference  *tempMR = (cg->machine())->getDummyLocalMR(dt);
   generateFPMemRegInstruction(opStore, node, tempMR, reg, cg);
   generateFPRegMemInstruction(opLoad, node, reg, tempMR, cg);
   reg->resetNeedsPrecisionAdjustment();
   reg->resetMayNeedPrecisionAdjustment();
   }


TR::Register *OMR::X86::TreeEvaluator::fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister;
   if (cg->useSSEForSinglePrecision())
      {
      targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);

      if (node->getFloatBits() == 0)
         {
         generateRegRegInstruction(XORPSRegReg, node, targetRegister, targetRegister, cg);
         }
      else
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(cg->findOrCreate4ByteConstant(node, node->getFloatBits()), cg);
         TR::Instruction *instr = generateRegMemInstruction(MOVSSRegMem, node, targetRegister, tempMR, cg);
         setDiscardableIfPossible(TR_RematerializableFloat, targetRegister, node, instr, (intptr_t)node->getFloatBits(), cg);
         }
      }
   else
      {
      targetRegister = cg->allocateSinglePrecisionRegister(TR_X87);

      if (node->getFloatBits() == FLOAT_POS_ZERO)
         {
         generateFPRegInstruction(FLD0Reg, node, targetRegister, cg);
         }
      else if (node->getFloatBits() == FLOAT_ONE)
         {
         generateFPRegInstruction(FLD1Reg, node, targetRegister, cg);
         }
      else
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(cg->findOrCreate4ByteConstant(node, node->getFloatBits()), cg);
         generateFPRegMemInstruction(FLDRegMem, node, targetRegister, tempMR, cg);
         }
      }
   node->setRegister(targetRegister);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister;
   if (cg->useSSEForDoublePrecision())
      {
      targetRegister = cg->allocateRegister(TR_FPR);

      if (node->getLongInt() == 0)
         {
         generateRegRegInstruction(XORPDRegReg, node, targetRegister, targetRegister, cg);
         }
      else
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(cg->findOrCreate8ByteConstant(node, node->getLongInt()), cg);
         generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, targetRegister, tempMR, cg);
         }
      }
   else
      {
      targetRegister = cg->allocateRegister(TR_X87);

      if (node->getLongInt() == 0) // hex for ieee double +0.0
         {
         generateFPRegInstruction(DLD0Reg, node, targetRegister, cg);
         }
      else if (node->getLongInt() == IEEE_DOUBLE_1_0) // hex for ieee double 1.0
         {
         generateFPRegInstruction(DLD1Reg, node, targetRegister, cg);
         }
      else
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(cg->findOrCreate8ByteConstant(node, node->getLongInt()), cg);
         generateFPRegMemInstruction(DLDRegMem, node, targetRegister, tempMR, cg);
         }
      }
   node->setRegister(targetRegister);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::performFload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg)
   {
   TR::Register    *targetRegister;
   TR::Instruction *instr;
   if (cg->useSSEForSinglePrecision())
      {
      if (cg->comp()->target().is64Bit() &&
          sourceMR->getSymbolReference().isUnresolved())
         {
         // The 64-bit mode XMM load instructions may be wider than 8-bytes (our patching
         // window) but we won't know that for sure until after register assignment.
         // Hence, the unresolved memory reference must be evaluated into a register
         // first.
         //
         TR::Register *memReg = cg->allocateRegister(TR_GPR);
         generateRegMemInstruction(LEA8RegMem, node, memReg, sourceMR, cg);
         sourceMR = generateX86MemoryReference(memReg, 0, cg);
         cg->stopUsingRegister(memReg);

         targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);
         instr = generateRegMemInstruction(MOVSSRegMem, node, targetRegister, sourceMR, cg);
         }
      else
         {
         targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);
         instr = generateRegMemInstruction(MOVSSRegMem, node, targetRegister, sourceMR, cg);
         setDiscardableIfPossible(TR_RematerializableFloat, targetRegister, node, instr, sourceMR, cg);
         }
      }
   else
      {
      targetRegister = cg->allocateSinglePrecisionRegister(TR_X87);
      instr = generateFPRegMemInstruction(FLDRegMem, node, targetRegister, sourceMR, cg);
      }
   if (node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);
   node->setRegister(targetRegister);
   return targetRegister;
   }

// also handles TR::floadi
TR::Register *OMR::X86::TreeEvaluator::floadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *tempMR = generateX86MemoryReference(node, cg);
   TR::Register *targetRegister = TR::TreeEvaluator::performFload(node, tempMR, cg);
   tempMR->decNodeReferenceCounts(cg);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::performDload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg)
   {
   TR::Register    *targetRegister;
   TR::Instruction *instr;
   if (cg->useSSEForDoublePrecision())
      {
      if (cg->comp()->target().is64Bit() &&
          sourceMR->getSymbolReference().isUnresolved())
         {
         // The 64-bit load instructions may be wider than 8-bytes (our patching
         // window) but we won't know that for sure until after register assignment.
         // Hence, the unresolved memory reference must be evaluated into a register
         // first.
         //
         TR::Register *memReg = cg->allocateRegister(TR_GPR);
         generateRegMemInstruction(LEA8RegMem, node, memReg, sourceMR, cg);
         sourceMR = generateX86MemoryReference(memReg, 0, cg);
         cg->stopUsingRegister(memReg);
         }

      targetRegister = cg->allocateRegister(TR_FPR);
      instr = generateRegMemInstruction(cg->getXMMDoubleLoadOpCode(), node, targetRegister, sourceMR, cg);
      }
   else
      {
      targetRegister = cg->allocateRegister(TR_X87);
      instr = generateFPRegMemInstruction(DLDRegMem, node, targetRegister, sourceMR, cg);
      }
   if (node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);
   node->setRegister(targetRegister);
   return targetRegister;
   }

// also handles TR::dloadi
TR::Register *OMR::X86::TreeEvaluator::dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *tempMR = generateX86MemoryReference(node, cg);
   TR::Register *targetRegister = TR::TreeEvaluator::performDload(node, tempMR, cg);
   tempMR->decNodeReferenceCounts(cg);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::floatingPointStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool     nodeIs64Bit    = TR::TreeEvaluator::getNodeIs64Bit(node, cg);
   bool     nodeIsIndirect = node->getOpCode().isIndirect()? 1 : 0;
   TR::Node *valueChild     = node->getChild(nodeIsIndirect);

   // Special case storing an int value into a float variable
   //
   if ((valueChild->getOpCodeValue() == TR::ibits2f || valueChild->getOpCodeValue() == TR::lbits2d) &&
       !valueChild->getRegister())
      {
      // Turn the tree into the corresponding integer store
      //
      TR::Node *integerValueChild = valueChild->getFirstChild();
      static TR::ILOpCodes integerOpCodes[2][2] = { {TR::istore, TR::lstore}, {TR::istorei, TR::lstorei} };
      TR::Node::recreate(node, integerOpCodes[nodeIsIndirect][(valueChild->getOpCodeValue() == TR::ibits2f ? 0 : 1)]);
      node->setChild(nodeIsIndirect, integerValueChild);
      integerValueChild->incReferenceCount();

      // valueChild is no longer used here
      //
      cg->recursivelyDecReferenceCount(valueChild);

      // Generate an integer store
      //
      TR::TreeEvaluator::integerStoreEvaluator(node, cg);
      return NULL;
      }

   TR::MemoryReference  *tempMR = generateX86MemoryReference(node, cg);
   TR::Instruction *exceptionPoint;

   if (valueChild->getOpCode().isLoadConst())
      {
      if (nodeIs64Bit)
         {
         if (cg->comp()->target().is64Bit())
            {
            TR::Register *floatConstReg = cg->allocateRegister(TR_GPR);
            if (valueChild->getLongInt() == 0)
               {
               generateRegRegInstruction(XOR8RegReg, node, floatConstReg, floatConstReg, cg);
               }
            else
               {
               generateRegImm64Instruction(MOV8RegImm64, node, floatConstReg, valueChild->getLongInt(), cg);
               }
            exceptionPoint = generateMemRegInstruction(S8MemReg, node, tempMR, floatConstReg, cg);
            cg->stopUsingRegister(floatConstReg);
            }
         else
            {
            exceptionPoint = generateMemImmInstruction(S4MemImm4, node, tempMR, valueChild->getLongIntLow(), cg);
            generateMemImmInstruction(S4MemImm4, node, generateX86MemoryReference(*tempMR, 4, cg), valueChild->getLongIntHigh(), cg);
            }
         }
      else
         {
         exceptionPoint = generateMemImmInstruction(S4MemImm4, node, tempMR, valueChild->getFloatBits(), cg);
         }
      TR::Register *firstChildReg = valueChild->getRegister();
      if (firstChildReg && firstChildReg->getKind() == TR_X87 && valueChild->getReferenceCount() == 1)
         generateFPSTiST0RegRegInstruction(FSTRegReg, valueChild, firstChildReg, firstChildReg, cg);
      }
   else if (debug("useGPRsForFP") &&
            (cg->getLiveRegisters(TR_GPR)->getNumberOfLiveRegisters() <
             cg->getMaximumNumbersOfAssignableGPRs() - 1) &&
            valueChild->getOpCode().isLoadVar() &&
            valueChild->getRegister() == NULL   &&
            valueChild->getReferenceCount() == 1)
      {
      TR::Register *tempRegister = cg->allocateRegister(TR_GPR);
      TR::MemoryReference  *loadMR = generateX86MemoryReference(valueChild, cg);
      generateRegMemInstruction(LRegMem(nodeIs64Bit), node, tempRegister, loadMR, cg);
      exceptionPoint = generateMemRegInstruction(SMemReg(nodeIs64Bit), node, tempMR, tempRegister, cg);
      cg->stopUsingRegister(tempRegister);
      loadMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *sourceRegister = cg->evaluate(valueChild);
      if (sourceRegister->getKind() == TR_FPR)
         {
         if (cg->comp()->target().is64Bit() &&
            tempMR->getSymbolReference().isUnresolved())
            {

            if (!tempMR->getSymbolReference().getSymbol()->isShadow() && !tempMR->getSymbolReference().getSymbol()->isClassObject() && !tempMR->getSymbolReference().getSymbol()->isConstObjectRef())
               {
               // The 64-bit static case does not require the LEA instruction as we can resolve the address in the MOV reg, imm  instruction preceeding the store.
               //
               exceptionPoint = generateMemRegInstruction(MOVSMemReg(nodeIs64Bit), node, tempMR, sourceRegister, cg);
               }
            else
               {
               // The 64-bit store instructions may be wider than 8-bytes (our patching
               // window) but we won't know that for sure until after register assignment.
               // Hence, the unresolved memory reference must be evaluated into a register
               // first.
               //
               TR::Register *memReg = cg->allocateRegister(TR_GPR);
               generateRegMemInstruction(LEA8RegMem, node, memReg, tempMR, cg);
               TR::MemoryReference *mr = generateX86MemoryReference(memReg, 0, cg);
               TR_ASSERT(nodeIs64Bit != sourceRegister->isSinglePrecision(), "Wrong operand type to floating point store\n");
               exceptionPoint = generateMemRegInstruction(MOVSMemReg(nodeIs64Bit), node, mr, sourceRegister, cg);

               tempMR->setProcessAsFPVolatile();

               if (cg->comp()->getOption(TR_X86UseMFENCE))
                  insertUnresolvedReferenceInstructionMemoryBarrier(cg, MFENCE, exceptionPoint, tempMR, sourceRegister, tempMR);
               else
                  insertUnresolvedReferenceInstructionMemoryBarrier(cg, LockOR, exceptionPoint, tempMR, sourceRegister, tempMR);

               cg->stopUsingRegister(memReg);
               }
            }
         else
            {
            TR_ASSERT(nodeIs64Bit != sourceRegister->isSinglePrecision(), "Wrong operand type to floating point store\n");
            exceptionPoint = generateMemRegInstruction(MOVSMemReg(nodeIs64Bit), node, tempMR, sourceRegister, cg);
            }
         }
      else
         {
         exceptionPoint = generateFPMemRegInstruction(FSTMemReg, node, tempMR, sourceRegister, cg);
         }
      }

   cg->decReferenceCount(valueChild);
   tempMR->decNodeReferenceCounts(cg);
   if (nodeIsIndirect)
      cg->setImplicitExceptionPoint(exceptionPoint);

   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::fpReturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());
   TR_ASSERT(returnRegister, "Return node's child should evaluate to a register");
   TR::Compilation *comp = cg->comp();

   if (cg->comp()->target().is32Bit() &&
       !cg->useSSEForDoublePrecision() &&
       returnRegister->getKind() == TR_FPR)
      {
      // TODO: Modify linkage to allow the returned value to remain in an XMMR.
      returnRegister = TR::TreeEvaluator::coerceXMMRToFPR(node->getFirstChild(), returnRegister, cg);
      }
   else if (returnRegister->mayNeedPrecisionAdjustment())
      {
      TR::TreeEvaluator::insertPrecisionAdjustment(returnRegister, node, cg);
      }

   // Restore the default FPCW if it has been forced to single precision mode.
   //
   if (comp->getJittedMethodSymbol()->usesSinglePrecisionMode() && !cg->useSSEForDoublePrecision())
      {
      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, DOUBLE_PRECISION_ROUND_TO_NEAREST), cg), cg);
      }

   const TR::X86LinkageProperties &linkageProperties = cg->getProperties();
   TR::RealRegister::RegNum machineReturnRegister =
      (returnRegister->isSinglePrecision())? linkageProperties.getFloatReturnRegister() : linkageProperties.getDoubleReturnRegister();

   TR::RegisterDependencyConditions *dependencies = NULL;
   if (machineReturnRegister != TR::RealRegister::NoReg)
      {
      dependencies = generateRegisterDependencyConditions((uint8_t)1, 0, cg);
      dependencies->addPreCondition(returnRegister, machineReturnRegister, cg);
      dependencies->stopAddingConditions();
      }

   if (linkageProperties.getCallerCleanup())
      {
      generateFPReturnInstruction(RET, node, dependencies, cg);
      }
   else
      {
      generateFPReturnImmInstruction(RETImm2, node, 0, dependencies, cg);
      }

   if (comp->getJittedMethodSymbol()->getLinkageConvention() == TR_Private)
      {
      if (cg->useSSEForDoublePrecision())
         comp->setReturnInfo((returnRegister->isSinglePrecision()) ? TR_FloatXMMReturn : TR_DoubleXMMReturn);
      else
         comp->setReturnInfo((returnRegister->isSinglePrecision()) ? TR_FloatReturn : TR_DoubleReturn);
      }

   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::fpBinaryArithmeticEvaluator(TR::Node          *node,
                                                              bool              isFloat,
                                                              TR::CodeGenerator *cg)
   {
   if (cg->useSSEForSinglePrecision() && cg->useSSEForDoublePrecision())
      {
      return TR::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator(node, cg);
      }
   else
      {
      TR_X86FPBinaryArithmeticAnalyser  temp(node, cg);
      temp.genericFPAnalyser(node);
      return node->getRegister();
      }
   }

TR::Register *OMR::X86::TreeEvaluator::fpUnaryMaskEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static uint8_t MASK_FABS[] =
      {
      0xff, 0xff, 0xff, 0x7f,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };
   static uint8_t MASK_DABS[] =
      {
      0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0x7f,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };
   static uint8_t MASK_FNEG[] =
      {
      0x00, 0x00, 0x00, 0x80,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };
   static uint8_t MASK_DNEG[] =
      {
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x80,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      };

   uint8_t*      mask;
   TR_X86OpCodes opcode;
   TR_X86OpCodes x87op;
   switch (node->getOpCodeValue())
      {
      case TR::fabs:
         mask = MASK_FABS;
         opcode = PANDRegMem;
         x87op = FABSReg;
         break;
      case TR::dabs:
         mask = MASK_DABS;
         opcode = PANDRegMem;
         x87op = DABSReg;
         break;
      case TR::fneg:
         mask = MASK_FNEG;
         opcode = PXORRegMem;
         x87op = FCHSReg;
         break;
      case TR::dneg:
         mask = MASK_DNEG;
         opcode = PXORRegMem;
         x87op = DCHSReg;
         break;
      default:
         TR_ASSERT(false, "Unsupported OpCode");
      }

   auto child = node->getFirstChild();
   auto value = cg->evaluate(child);
   auto result = child->getReferenceCount() == 1 ? value : cg->allocateRegister(value->getKind());

   if (result != value && value->isSinglePrecision())
      {
      result->setIsSinglePrecision();
      }

   if (value->getKind() != TR_FPR) // Legacy supported for X87, to be deleted
      {
      if (value->needsPrecisionAdjustment())
         TR::TreeEvaluator::insertPrecisionAdjustment(value, node, cg);
      if (value->mayNeedPrecisionAdjustment())
         result->setMayNeedPrecisionAdjustment();

      if (result != value)
         {
         generateFPST0STiRegRegInstruction(value->isSinglePrecision() ? FLDRegReg : DLDRegReg, node, result, value, cg);
         }
      generateFPRegInstruction(x87op, node, result, cg);
      }
   else
      {
      // TODO 3-OP Optimization
      if (result != value)
         {
         generateRegRegInstruction(MOVDQURegReg, node, result, value, cg);
         }
      generateRegMemInstruction(opcode, node, result, generateX86MemoryReference(cg->findOrCreate16ByteConstant(node, mask), cg), cg);
      }

   node->setRegister(result);
   cg->decReferenceCount(child);
   return result;
   }

TR::Register *OMR::X86::TreeEvaluator::fpSqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   auto value = cg->evaluate(node->getFirstChild());
   auto result = cg->allocateRegister(value->getKind());

   if (value->isSinglePrecision())
      {
      result->setIsSinglePrecision();
      }

   if (value->getKind() != TR_FPR) // Legacy supported for X87, to be deleted
      {
      auto tmp = cg->allocateRegister(TR_FPR);
      if (value->isSinglePrecision())
         {
         result->setIsSinglePrecision();
         auto tempMR = cg->machine()->getDummyLocalMR(TR::Float);
         generateFPMemRegInstruction(FSTPMemReg, node, tempMR, value, cg);
         generateRegMemInstruction(MOVSSRegMem, node, tmp, generateX86MemoryReference(*tempMR, 0, cg), cg);
         generateRegRegInstruction(SQRTSSRegReg, node, tmp, tmp, cg);
         generateMemRegInstruction(MOVSSMemReg, node, generateX86MemoryReference(*tempMR, 0, cg), tmp, cg);
         generateFPRegMemInstruction(FLDRegMem, node, result, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      else
         {
         auto tempMR = cg->machine()->getDummyLocalMR(TR::Double);
         generateFPMemRegInstruction(DSTPMemReg, node, tempMR, value, cg);
         generateRegMemInstruction(MOVSDRegMem, node, tmp, generateX86MemoryReference(*tempMR, 0, cg), cg);
         generateRegRegInstruction(SQRTSDRegReg, node, tmp, tmp, cg);
         generateMemRegInstruction(MOVSDMemReg, node, generateX86MemoryReference(*tempMR, 0, cg), tmp, cg);
         generateFPRegMemInstruction(DLDRegMem, node, result, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      cg->stopUsingRegister(tmp);
      }
   else
      {
      generateRegRegInstruction(value->isSinglePrecision() ? SQRTSSRegReg : SQRTSDRegReg, node, result, value, cg);
      }

   node->setRegister(result);
   cg->decReferenceCount(node->getFirstChild());
   return result;
   }

TR::Register *OMR::X86::TreeEvaluator::dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *operand = node->getFirstChild();
   TR::Register *opRegister = cg->evaluate(operand);
   TR::Register *targetRegister = cg->allocateRegister(TR_FPR);

   generateRegRegInstruction(SQRTSDRegReg, node, targetRegister, opRegister, cg);

   node->setRegister(targetRegister);
   cg->decReferenceCount(operand);
   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::faddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::daddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::fpBinaryArithmeticEvaluator(node, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::fpRemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool         nodeIsDouble = node->getDataType() == TR::Double;
   TR::Register *targetRegister;
   TR::Compilation *comp = cg->comp();
   const TR::X86LinkageProperties &linkageProperties = cg->getLinkage(comp->getJittedMethodSymbol()->getLinkageConvention())->getProperties();

   if (cg->useSSEForDoublePrecision()) // Note: both float and double helpers use SSE2
      {
      TR::Node *divisor = node->getSecondChild();
      TR::Node *dividend = node->getFirstChild();

      if (cg->comp()->target().is64Bit())
         {
         // TODO: We should do this for IA32 eventually
         TR::SymbolReference *helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(nodeIsDouble ? TR_AMD64doubleRemainder : TR_AMD64floatRemainder, false, false, false);
         targetRegister = TR::TreeEvaluator::performHelperCall(node, helperSymRef, nodeIsDouble ? TR::dcall : TR::fcall, false, cg);
         }
      else
         {
         TR::SymbolReference *helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(nodeIsDouble ? TR_IA32doubleRemainderSSE : TR_IA32floatRemainderSSE, false, false, false);
         targetRegister = TR::TreeEvaluator::performHelperCall(node, helperSymRef, nodeIsDouble ? TR::dcall : TR::fcall, false, cg);
         }
      }
   else
      {
      targetRegister = TR::TreeEvaluator::commonFPRemEvaluator(node, cg, nodeIsDouble);
      }


   if ((!nodeIsDouble && cg->useSSEForSinglePrecision()) ||
       (nodeIsDouble && cg->useSSEForDoublePrecision()))
      return targetRegister;

   if (nodeIsDouble &&
       (comp->getCurrentMethod()->isStrictFP() ||
        comp->getOption(TR_StrictFP)))
      {
      // Strict double op.
      //
      targetRegister->setMayNeedPrecisionAdjustment();
      targetRegister->setNeedsPrecisionAdjustment();
      }
   else if (!nodeIsDouble && !comp->getJittedMethodSymbol()->usesSinglePrecisionMode())
      {
      // Float op in a double-precision method.
      //
      targetRegister->setMayNeedPrecisionAdjustment();
      targetRegister->setNeedsPrecisionAdjustment();
      }

   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::commonFPRemEvaluator(TR::Node          *node,
                                                       TR::CodeGenerator *cg,
                                                       bool              isDouble)
   {
   TR::Node *divisor = node->getSecondChild();
   TR::Node *dividend = node->getFirstChild();
   TR::Compilation *comp = cg->comp();

   TR::Register *divisorReg = cg->evaluate( divisor);
   TR_ASSERT(divisorReg->getKind() == TR_X87, "X87 Instructions only.");

   if (divisorReg->needsPrecisionAdjustment())
      TR::TreeEvaluator::insertPrecisionAdjustment(divisorReg, divisor, cg);

   TR::Register *dividendReg = cg->evaluate( dividend);
   TR_ASSERT(dividendReg->getKind() == TR_X87, "X87 Instructions only.");

   if (dividendReg->needsPrecisionAdjustment())
      TR::TreeEvaluator::insertPrecisionAdjustment(dividendReg, dividend, cg);

   if (isDouble)
      dividendReg = cg->doubleClobberEvaluate(dividend);
   else
      dividendReg = cg->floatClobberEvaluate(dividend);

   TR::Register *accReg = cg->allocateRegister();
   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t) 0, 1, cg);
   deps->addPostCondition( accReg, TR::RealRegister::eax, cg);

   generateFPRemainderRegRegInstruction( FPREMRegReg, node, dividendReg, divisorReg, accReg, deps, cg);
   cg->stopUsingRegister(accReg);

   node->setRegister( dividendReg);
   cg->decReferenceCount( dividend);

   if (divisorReg && divisorReg->getKind() == TR_X87 && divisor->getReferenceCount() == 1)
      generateFPSTiST0RegRegInstruction(FSTRegReg, node, divisorReg, divisorReg, cg);

   cg->decReferenceCount( divisor);

   dividendReg->setMayNeedPrecisionAdjustment();

   if ((node->getOpCode().isFloat() && !comp->getJittedMethodSymbol()->usesSinglePrecisionMode()) ||
       comp->getCurrentMethod()->isStrictFP() ||
       comp->getOption(TR_StrictFP))
      {
      dividendReg->setNeedsPrecisionAdjustment();
      }

   return dividendReg;
   }

// also handles b2f, bu2f, s2f, su2f evaluators
TR::Register *OMR::X86::TreeEvaluator::i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::Register            *target;
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL &&
       child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      tempMR = generateX86MemoryReference(child, cg);
      if (cg->useSSEForSinglePrecision())
         {
         target = cg->allocateSinglePrecisionRegister(TR_FPR);
         generateRegMemInstruction(CVTSI2SSRegMem, node, target, tempMR, cg);
         }
      else
         {
         target = cg->allocateSinglePrecisionRegister(TR_X87);
         generateFPRegMemInstruction(FILDRegMem, node, target, tempMR, cg);
         target->setMayNeedPrecisionAdjustment();
         target->setNeedsPrecisionAdjustment();
         }
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);
      if (cg->useSSEForSinglePrecision())
         {
         switch (node->getOpCodeValue())
            {
            case TR::b2f:
               generateRegRegInstruction(MOVSXReg4Reg1, node, intReg, intReg, cg);
               break;
            case TR::bu2f:
               generateRegRegInstruction(MOVZXReg4Reg1, node, intReg, intReg, cg);
               break;
            case TR::s2f:
               generateRegRegInstruction(MOVSXReg4Reg2, node, intReg, intReg, cg);
               break;
            case TR::su2f:
               generateRegRegInstruction(MOVZXReg4Reg2, node, intReg, intReg, cg);
               break;
            case TR::i2f:
               break;
            default:
               TR_ASSERT(0, "INVALID OP CODE");
               break;
            }
         target = cg->allocateSinglePrecisionRegister(TR_FPR);
         generateRegRegInstruction(CVTSI2SSRegReg4, node, target, intReg, cg);
         }
      else
         {
         target = cg->allocateSinglePrecisionRegister(TR_X87);
         tempMR = generateX86MemoryReference(cg->allocateLocalTemp(), cg);
         generateMemRegInstruction(S4MemReg, node, tempMR, intReg, cg);
         generateFPRegMemInstruction(FILDRegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         target->setMayNeedPrecisionAdjustment();
         target->setNeedsPrecisionAdjustment();
         }
      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }

// also handles b2d, bu2d, s2d, su2d evaluators
TR::Register *OMR::X86::TreeEvaluator::i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target;
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 && child->getOpCode().isLoadVar())
      {
      tempMR = generateX86MemoryReference(child, cg);
      if (cg->useSSEForDoublePrecision())
         {
         target = cg->allocateRegister(TR_FPR);
         generateRegMemInstruction(CVTSI2SDRegMem, node, target, tempMR, cg);
         }
      else
         {
         target = cg->allocateRegister(TR_X87);
         generateFPRegMemInstruction(DILDRegMem, node, target, tempMR, cg);
         }
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);
      if (cg->useSSEForDoublePrecision())
         {
         switch (node->getOpCodeValue())
            {
            case TR::b2d:
               generateRegRegInstruction(MOVSXReg4Reg1, node, intReg, intReg, cg);
               break;
            case TR::bu2d:
               generateRegRegInstruction(MOVZXReg4Reg1, node, intReg, intReg, cg);
               break;
            case TR::s2d:
               generateRegRegInstruction(MOVSXReg4Reg2, node, intReg, intReg, cg);
               break;
            case TR::su2d:
               generateRegRegInstruction(MOVZXReg4Reg2, node, intReg, intReg, cg);
               break;
            case TR::i2d:
               break;
            default:
               TR_ASSERT(0, "INVALID OP CODE");
               break;
            }
         target = cg->allocateRegister(TR_FPR);
         generateRegRegInstruction(CVTSI2SDRegReg4, node, target, intReg, cg);
         }
      else
         {
         target = cg->allocateRegister(TR_X87);
         tempMR = generateX86MemoryReference(cg->allocateLocalTemp(), cg);
         generateMemRegInstruction(S4MemReg, node, tempMR, intReg, cg);
         generateFPRegMemInstruction(DILDRegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }


// General float/double convert to int
//
TR::Register *OMR::X86::TreeEvaluator::fpConvertToInt(TR::Node *node, TR::SymbolReference *helperSymRef, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 has enableSSE set, so it doesn't use this logic");

   TR::Node     *child     = node->getFirstChild();
   TR::Register *accReg    = 0;
   TR::Register *floatReg;
   TR::Register *resultReg;

   TR::MemoryReference               *tempMR;
   TR::X86RegMemInstruction             *loadInstr;
   TR::RegisterDependencyConditions  *deps;

   TR::LabelSymbol *startLabel    = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *reStartLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *snippetLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   startLabel->setStartInternalControlFlow();
   reStartLabel->setEndInternalControlFlow();

   bool optimizeF2IWithSSE = ( node->getOpCodeValue() == TR::f2i &&
                               cg->getX86ProcessorInfo().supportsSSE() );

   bool optimizeD2IWithSSE2 = ( node->getOpCodeValue() == TR::d2i &&
                                cg->getX86ProcessorInfo().supportsSSE2() );

   if (!optimizeF2IWithSSE && !optimizeD2IWithSSE2)
      {
      floatReg  = cg->evaluate(child);
      if (floatReg  && floatReg->needsPrecisionAdjustment())
         {
         TR::TreeEvaluator::insertPrecisionAdjustment(floatReg, node, cg);
         }
      }

   generateLabelInstruction(LABEL, node, startLabel, cg);

   if (!optimizeF2IWithSSE && !optimizeD2IWithSSE2)
      {
      int16_t fpcw;

      fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ?
                SINGLE_PRECISION_ROUND_TO_ZERO : DOUBLE_PRECISION_ROUND_TO_ZERO;

      fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ?
                SINGLE_PRECISION_ROUND_TO_NEAREST : DOUBLE_PRECISION_ROUND_TO_NEAREST;

      tempMR = (cg->machine())->getDummyLocalMR(TR::Int32);

      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);
      generateFPMemRegInstruction(FISTMemReg, node, tempMR, floatReg, cg);
      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);
      resultReg = cg->allocateRegister();
      loadInstr = generateRegMemInstruction(L4RegMem, node, resultReg, generateX86MemoryReference(*tempMR, 0, cg), cg);
      generateRegImmInstruction(CMP4RegImm4, node, resultReg, INT_MIN, cg);
      generateLabelInstruction(JE4, node, snippetLabel, cg);
      }
   else
      {
      if (optimizeF2IWithSSE)
         {
         if (child->getReferenceCount() == 1 &&
             child->getRegister() == 0       &&
             child->getOpCode().isMemoryReference())
            {
            tempMR = generateX86MemoryReference(child, cg);
            floatReg = cg->allocateRegister(TR_X87);
            generateFPRegMemInstruction(FLDRegMem, node, floatReg, tempMR, cg);
            resultReg = cg->allocateRegister();
            loadInstr = generateRegMemInstruction(CVTTSS2SIReg4Mem, node, resultReg,
                                                  generateX86MemoryReference(*tempMR, 0, cg), cg);
            tempMR->decNodeReferenceCounts(cg);
            }
         else
            {
            tempMR = (cg->machine())->getDummyLocalMR(TR::Float);
            floatReg  = cg->evaluate(child);
            generateFPMemRegInstruction(FSTMemReg, node, tempMR, floatReg, cg);
            resultReg = cg->allocateRegister();
            loadInstr = generateRegMemInstruction(CVTTSS2SIReg4Mem, node, resultReg,
                                                  generateX86MemoryReference(*tempMR, 0, cg), cg);
            }
         }
      else if (optimizeD2IWithSSE2)
         {
         if (child->getReferenceCount() == 1 &&
             child->getRegister() == 0       &&
             child->getOpCode().isMemoryReference())
            {
            tempMR = generateX86MemoryReference(child, cg);
            floatReg = cg->allocateRegister(TR_X87);
            generateFPRegMemInstruction(DLDRegMem, node, floatReg, tempMR, cg);
            resultReg = cg->allocateRegister();
            loadInstr = generateRegMemInstruction(CVTTSD2SIReg4Mem, node, resultReg,
                                                  generateX86MemoryReference(*tempMR, 0, cg), cg);
            tempMR->decNodeReferenceCounts(cg);
            }
         else
            {
            tempMR = (cg->machine())->getDummyLocalMR(TR::Double);
            floatReg  = cg->evaluate(child);
            generateFPMemRegInstruction(DSTMemReg, node, tempMR, floatReg, cg);
            resultReg = cg->allocateRegister();
            loadInstr = generateRegMemInstruction(CVTTSD2SIReg4Mem, node, resultReg,
                                                  generateX86MemoryReference(*tempMR, 0, cg), cg);
            }
         }
      else
         {
         floatReg = cg->evaluate(child);
         tempMR = (cg->machine())->getDummyLocalMR(TR::Int32);
         generateFPMemRegInstruction(FISTMemReg, node, tempMR, floatReg, cg);
         resultReg = cg->allocateRegister();
         loadInstr = generateRegMemInstruction(L4RegMem, node, resultReg,
                                               generateX86MemoryReference(*tempMR, 0, cg), cg);
         }

      generateRegImmInstruction(CMP4RegImm4, node, resultReg, INT_MIN, cg);
      generateLabelInstruction(JE4, node, snippetLabel, cg);

      }

   // Create the conversion snippet.
   //
   cg->addSnippet( new (cg->trHeapMemory()) TR::X86FPConvertToIntSnippet(reStartLabel,
                                                         snippetLabel,
                                                         helperSymRef,
                                                         loadInstr,
                                                         cg) );

   // Make sure the int register(s) is/are assigned to something.
   //
   if (accReg)
      {
      deps = generateRegisterDependencyConditions((uint8_t) 0, 2, cg);
      deps->addPostCondition(accReg, TR::RealRegister::eax, cg);
      deps->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);
      }
   else
      {
      deps = generateRegisterDependencyConditions((uint8_t) 0, 1, cg);
      deps->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);
      }

   generateLabelInstruction(LABEL, node, reStartLabel, deps, cg);

   // We want the floating point register to be live through the snippet, so if it is
   // not referenced again we must pop it off the stack here.
   //
   if (cg->decReferenceCount(child) == 0)
      {
      generateFPSTiST0RegRegInstruction(FSTRegReg, node, floatReg, floatReg, cg);
      }

   node->setRegister(resultReg);
   return resultReg;
   }


// General float/double convert to long
//
TR::Register *OMR::X86::TreeEvaluator::fpConvertToLong(TR::Node *node, TR::SymbolReference *helperSymRef, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 doesn't use this logic");

   TR::Node *child = node->getFirstChild();

   // TODO: Perform f2l using SSE.
   if (child->getOpCode().isDouble() && cg->useSSEForDoublePrecision())
      {
      TR::RegisterDependencyConditions  *deps;

      TR::Register        *doubleReg = cg->evaluate(child);
      TR::Register        *lowReg    = cg->allocateRegister(TR_GPR);
      TR::Register        *highReg   = cg->allocateRegister(TR_GPR);
      TR::RealRegister *espReal   = cg->machine()->getRealRegister(TR::RealRegister::esp);

      deps = generateRegisterDependencyConditions((uint8_t) 0, 3, cg);
      deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(doubleReg, TR::RealRegister::NoReg, cg);
      deps->stopAddingConditions();

      TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);   // exit routine label
      TR::LabelSymbol *CallLabel    = TR::LabelSymbol::create(cg->trHeapMemory(),cg);   // label where long (64-bit) conversion will start
      TR::LabelSymbol *StartLabel   = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      StartLabel->setStartInternalControlFlow();
      reStartLabel->setEndInternalControlFlow();

      // Attempt to convert a double in an XMM register to an integer using CVTTSD2SI.
      // If the conversion succeeds, put the integer in lowReg and sign-extend it to highReg.
      // If the conversion fails (the double is too large), call the helper.
      generateRegRegInstruction(CVTTSD2SIReg4Reg, node, lowReg, doubleReg, cg);
      generateRegImmInstruction(CMP4RegImm4, node, lowReg, 0x80000000, cg);

      generateLabelInstruction(LABEL, node, StartLabel, cg);
      generateLabelInstruction(JE4, node, CallLabel, cg);

      generateRegRegInstruction(MOV4RegReg, node, highReg ,lowReg, cg);
      generateRegImmInstruction(SAR4RegImm1, node, highReg , 31, cg);

      generateLabelInstruction(LABEL, node, reStartLabel, deps, cg);

      TR::Register *targetRegister = cg->allocateRegisterPair(lowReg, highReg);
      TR::SymbolReference *d2l = comp->getSymRefTab()->findOrCreateRuntimeHelper(TR_IA32double2LongSSE,false,false,false);
      d2l->getSymbol()->getMethodSymbol()->setLinkage(TR_Helper);
      TR::Node::recreate(node, TR::lcall);
      node->setSymbolReference(d2l);
      TR_OutlinedInstructions *outlinedHelperCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(node, TR::lcall, targetRegister, CallLabel, reStartLabel, cg);
      cg->getOutlinedInstructionsList().push_front(outlinedHelperCall);

      cg->decReferenceCount(child);
      node->setRegister(targetRegister);

      return targetRegister;
      }
   else
      {
      TR::Register *accReg    = NULL;
      TR::Register *lowReg    = cg->allocateRegister(TR_GPR);
      TR::Register *highReg   = cg->allocateRegister(TR_GPR);
      TR::Register *doubleReg = cg->evaluate(child);
      if (doubleReg->getKind() == TR_FPR)
         doubleReg = TR::TreeEvaluator::coerceXMMRToFPR(child, doubleReg, cg);

      TR::RegisterDependencyConditions  *deps;

      TR::LabelSymbol *snippetLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *startLabel   = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *reStartLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      startLabel->setStartInternalControlFlow();
      reStartLabel->setEndInternalControlFlow();

      if (doubleReg && doubleReg->needsPrecisionAdjustment())
         {
         TR::TreeEvaluator::insertPrecisionAdjustment(doubleReg, node, cg);
         }

      generateLabelInstruction(LABEL, node, startLabel, cg);

      // These instructions must be set appropriately prior to the creation
      // of the snippet near the end of this method. Also see warnings below.
      //
      TR::X86FPST0STiRegRegInstruction  *clobInstruction;  // loads a clobberable copy of the float/double
      TR::X86RegMemInstruction          *loadHighInstr;    // loads the high dword of the converted long
      TR::X86RegMemInstruction          *loadLowInstr;     // loads the low dword of the converted long

      TR::Register *tempFPR1 = child->getOpCode().isFloat() ? cg->allocateSinglePrecisionRegister(TR_X87)
                                                           : cg->allocateRegister(TR_X87);

      // WARNING:
      //
      // The following instruction is dissected in the snippet to determine the original double register.
      // If this instruction changes, or if you add any instructions that manipulate the FP stack between
      // here and the call to the snippet, you may need to change the snippet also.
      //
      clobInstruction = generateFPST0STiRegRegInstruction(FLDRegReg, node, tempFPR1, doubleReg, cg);

      // For slow conversion only, change the rounding mode on the FPU via its control word register.
      //
      int16_t fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ?
                        SINGLE_PRECISION_ROUND_TO_ZERO : DOUBLE_PRECISION_ROUND_TO_ZERO;

      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);

      TR::MemoryReference  *convertedLongMR = (cg->machine())->getDummyLocalMR(TR::Int64);
      generateFPMemRegInstruction(FLSTPMem, node, convertedLongMR, tempFPR1, cg);
      cg->stopUsingRegister(tempFPR1);

      fpcw = comp->getJittedMethodSymbol()->usesSinglePrecisionMode() ?
                SINGLE_PRECISION_ROUND_TO_NEAREST : DOUBLE_PRECISION_ROUND_TO_NEAREST;

      generateMemInstruction(LDCWMem, node, generateX86MemoryReference(cg->findOrCreate2ByteConstant(node, fpcw), cg), cg);

      // WARNING:
      //
      // The following load instructions are dissected in the snippet to determine the target registers.
      // If they or their format is changed, you may need to change the snippet also.
      //
      loadHighInstr = generateRegMemInstruction(L4RegMem, node, highReg,
                                                generateX86MemoryReference(*convertedLongMR, 4, cg), cg);

      loadLowInstr = generateRegMemInstruction(L4RegMem, node, lowReg,
                                               generateX86MemoryReference(*convertedLongMR, 0, cg), cg);

      // Jump to the snippet if the converted value is an indefinite integer; otherwise continue.
      //
      generateRegImmInstruction(CMP4RegImm4, node, highReg, INT_MIN, cg);
      generateLabelInstruction(JNE4, node, reStartLabel, cg);
      generateRegRegInstruction(TEST4RegReg, node, lowReg, lowReg, cg);
      generateLabelInstruction(JE4, node, snippetLabel, cg);

      // Create the conversion snippet.
      //
      cg->addSnippet( new (cg->trHeapMemory()) TR::X86FPConvertToLongSnippet(reStartLabel,
                                                        snippetLabel,
                                                        helperSymRef,
                                                        clobInstruction,
                                                        loadHighInstr,
                                                        loadLowInstr,
                                                        cg) );

      // Make sure the high and low long registers are assigned to something.
      //
      if (accReg)
         {
         deps = generateRegisterDependencyConditions((uint8_t)0, 3, cg);
         deps->addPostCondition(accReg, TR::RealRegister::eax, cg);
         deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
         deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
         }
      else
         {
         deps = generateRegisterDependencyConditions((uint8_t)0, 2, cg);
         deps->addPostCondition(lowReg, TR::RealRegister::NoReg, cg);
         deps->addPostCondition(highReg, TR::RealRegister::NoReg, cg);
         }
      generateLabelInstruction(LABEL, node, reStartLabel, deps, cg);

      // We want the floating point register to be live through the snippet, so if it is
      // not referenced again we must pop it off the stack here.
      //
      if ((cg->decReferenceCount(child) == 0) || (child->getRegister()->getKind() == TR_FPR))
         {
         generateFPSTiST0RegRegInstruction(FSTRegReg, node, doubleReg, doubleReg, cg);
         }

      TR::Register *targetRegister = cg->allocateRegisterPair(lowReg, highReg);
      node->setRegister(targetRegister);
      return targetRegister;
      }
   }


TR::Register *OMR::X86::TreeEvaluator::f2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->useSSEForSinglePrecision())
      {
      bool doubleSource;
      bool longTarget;
      TR_X86OpCodes cvttOpCode;
      // On AMD64, all four [fd]2[il] conversions are handled here
      // On IA32, both [fd]2i conversions are handled here
      switch (node->getOpCodeValue())
         {
         case TR::f2i:
            cvttOpCode   = CVTTSS2SIReg4Reg;
            doubleSource = false;
            longTarget   = false;
            break;
         case TR::f2l:
            cvttOpCode   = CVTTSS2SIReg8Reg;
            doubleSource = false;
            longTarget   = true;
            break;
         case TR::d2i:
            cvttOpCode   = CVTTSD2SIReg4Reg;
            doubleSource = true;
            longTarget   = false;
            break;
         case TR::d2l:
            cvttOpCode   = CVTTSD2SIReg8Reg;
            doubleSource = true;
            longTarget   = true;
            break;
         default:
            TR_ASSERT(0, "Unknown opcode value in f2iEvaluator");
            break;
         }
      TR_ASSERT(cg->comp()->target().is64Bit() || !longTarget, "Incorrect opcode value in f2iEvaluator");

      TR::TreeEvaluator::coerceFPOperandsToXMMRs(node, cg);

      TR::Node        *child          = node->getFirstChild();
      TR::Register    *sourceRegister = NULL;
      TR::Register    *targetRegister = cg->allocateRegister(TR_GPR);
      TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      TR::LabelSymbol *exceptionLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

      sourceRegister = cg->evaluate(child);
      if (sourceRegister->getKind() == TR_X87 && child->getReferenceCount() == 1)
         {
         TR_ASSERT(cg->comp()->target().is32Bit(), "assertion failure");
         TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(TR::Float);
         generateFPMemRegInstruction(FSTMemReg, node, tempMR, sourceRegister, cg);
         generateRegMemInstruction(CVTTSS2SIReg4Mem,
                                   node,
                                   targetRegister,
                                   generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      else
         {
         generateRegRegInstruction(cvttOpCode, node, targetRegister, sourceRegister, cg);
         }

      startLabel->setStartInternalControlFlow();
      endLabel->setEndInternalControlFlow();

      generateLabelInstruction(LABEL, node, startLabel, cg);

      if (longTarget)
         {
         TR_ASSERT(cg->comp()->target().is64Bit(), "We should only get here on AMD64");
         // We can't compare with 0x8000000000000000.
         // Instead, rotate left 1 bit and compare with 0x0000000000000001.
         generateRegInstruction(ROL8Reg1, node, targetRegister, cg);
         generateRegImmInstruction(CMP8RegImms, node, targetRegister, 1, cg);
         generateLabelInstruction(JE4, node, exceptionLabel, cg);
         }
      else
         {
         generateRegImmInstruction(CMP4RegImm4, node, targetRegister, INT_MIN, cg);
         generateLabelInstruction(JE4, node, exceptionLabel, cg);
         }

      //TODO: (omr issue #4969): Remove once support for spills in OOL paths is added
      TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)2, cg);
      deps->addPostCondition(targetRegister, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(sourceRegister, TR::RealRegister::NoReg, cg);
      {
      TR_OutlinedInstructionsGenerator og(exceptionLabel, node, cg);
      // at this point, target is set to -INF and there can only be THREE possible results: -INF, +INF, NaN
      // compare source with ZERO
      generateRegMemInstruction(doubleSource ? UCOMISDRegMem : UCOMISSRegMem,
                                node,
                                sourceRegister,
                                generateX86MemoryReference(doubleSource ? cg->findOrCreate8ByteConstant(node, 0) : cg->findOrCreate4ByteConstant(node, 0), cg),
                                cg);
      // load max int if source is positive, note that for long case, LLONG_MAX << 1 is loaded as it will be shifted right
      generateRegMemInstruction(CMOVARegMem(longTarget),
                                node,
                                targetRegister,
                                generateX86MemoryReference(longTarget ? cg->findOrCreate8ByteConstant(node, LLONG_MAX << 1) : cg->findOrCreate4ByteConstant(node, INT_MAX), cg),
                                cg);
      // load zero if source is NaN
      generateRegMemInstruction(CMOVPRegMem(longTarget),
                                node,
                                targetRegister,
                                generateX86MemoryReference(longTarget ? cg->findOrCreate8ByteConstant(node, 0) : cg->findOrCreate4ByteConstant(node, 0), cg),
                                cg);

      generateLabelInstruction(JMP4, node, endLabel, cg);
      og.endOutlinedInstructionSequence();
      }

      generateLabelInstruction(LABEL, node, endLabel, deps, cg);
      if (longTarget)
         {
         generateRegInstruction(ROR8Reg1, node, targetRegister, cg);
         }

      if (sourceRegister &&
          sourceRegister->getKind() == TR_X87 &&
          child->getReferenceCount() == 1)
         {
         generateFPSTiST0RegRegInstruction(FSTRegReg, node, sourceRegister, sourceRegister, cg);
         }

      node->setRegister(targetRegister);
      cg->decReferenceCount(child);
      return targetRegister;
      }
   else
      {
      TR_ASSERT(cg->comp()->target().is32Bit(), "assertion failure");
      return TR::TreeEvaluator::fpConvertToInt(node, cg->symRefTab()->findOrCreateRuntimeHelper(node->getOpCodeValue() == TR::f2i ? TR_IA32floatToInt : TR_IA32doubleToInt, false, false, false), cg);
      }
   }


TR::Register *OMR::X86::TreeEvaluator::f2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 uses f2iEvaluator for this");
   return TR::TreeEvaluator::fpConvertToLong(node, cg->symRefTab()->findOrCreateRuntimeHelper(TR_IA32floatToLong, false, false, false), cg);
   }


TR::Register *OMR::X86::TreeEvaluator::f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *targetRegister;

   if (cg->useSSEForDoublePrecision())
      {
      targetRegister = cg->floatClobberEvaluate(child);
      targetRegister->setIsSinglePrecision(false);
      generateRegRegInstruction(CVTSS2SDRegReg, node, targetRegister, targetRegister, cg);
      }
   else
      {
      TR::Register *sourceRegister = cg->evaluate(child);

      if (cg->useSSEForSinglePrecision() && sourceRegister->getKind() == TR_FPR)
         {
         TR::MemoryReference  *tempMR = cg->machine()->getDummyLocalMR(TR::Float);
         targetRegister = cg->allocateRegister(TR_X87);
         generateMemRegInstruction(MOVSSMemReg, node, tempMR, sourceRegister, cg);
         generateFPRegMemInstruction(FLDRegMem, node, targetRegister, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      else
         {
         targetRegister = cg->doubleClobberEvaluate(child);
         targetRegister->setIsSinglePrecision(false);
         if (targetRegister->needsPrecisionAdjustment())
            {
            TR::TreeEvaluator::insertPrecisionAdjustment(targetRegister, node, cg);
            }
         }
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(child);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::f2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("f2b not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::f2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("f2s not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::f2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("f2c not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->comp()->target().is32Bit(), "AMD64 uses f2iEvaluator for this");

   return TR::TreeEvaluator::fpConvertToLong(node, cg->symRefTab()->findOrCreateRuntimeHelper(TR_IA32doubleToLong, false, false, false), cg);
   }


TR::Register *OMR::X86::TreeEvaluator::d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *targetRegister;

   if (cg->useSSEForDoublePrecision())
      {
      TR::TreeEvaluator::coerceFPOperandsToXMMRs(node, cg);
      targetRegister = cg->doubleClobberEvaluate(child);
      targetRegister->setIsSinglePrecision(true);
      generateRegRegInstruction(CVTSD2SSRegReg, node, targetRegister, targetRegister, cg);
      }
   else
      {
      TR::Register *sourceRegister = cg->evaluate(child);
      if (cg->useSSEForSinglePrecision())
         {
         TR::MemoryReference  *tempMR = generateX86MemoryReference(cg->allocateLocalTemp(TR::Float), cg);
         targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);
         generateFPMemRegInstruction(FSTMemReg, node, tempMR, sourceRegister, cg);
         generateRegMemInstruction(MOVSSRegMem, node, targetRegister, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      else
         {
         targetRegister = cg->doubleClobberEvaluate(child);
         targetRegister->setMayNeedPrecisionAdjustment();
         targetRegister->setNeedsPrecisionAdjustment();
         targetRegister->setIsSinglePrecision(true);
         }
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(child);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::d2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("d2b not expected!");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::d2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("d2s not expected!");
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::d2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   diagnostic("d2s not expected!");
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::Register            *target;
   TR::MemoryReference  *tempMR;

   if (child->getRegister() == NULL &&
       child->getOpCode().isLoadVar())
      {
      // Load up the child as a float, then as an int if necessary.
      //
      // TODO: check the capabilities of the processor rather than whether SSE is enabled.
      //
      tempMR = generateX86MemoryReference(child, cg);
      if (cg->useSSEForDoublePrecision())
         {
         target = cg->allocateSinglePrecisionRegister(TR_FPR);
         generateRegMemInstruction(MOVSSRegMem, node, target, tempMR, cg);
         if (child->getReferenceCount() > 1)
            {
            TR::Register *intReg = cg->allocateRegister();
            generateRegRegInstruction(MOVDReg4Reg, node, intReg, target, cg);
            child->setRegister(intReg);
            }
         }
      else
         {
         if (cg->useSSEForSinglePrecision())
            {
            target = cg->allocateSinglePrecisionRegister(TR_FPR);
            generateRegMemInstruction(MOVSSRegMem, node, target, tempMR, cg);
            }
         else
            {
            target = cg->allocateSinglePrecisionRegister(TR_X87);
            generateFPRegMemInstruction(FLDRegMem, node, target, tempMR, cg);
            }

         if (child->getReferenceCount() > 1)
            TR::TreeEvaluator::performIload(child, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      TR::Register *intReg = cg->evaluate(child);
      if (cg->useSSEForDoublePrecision())
         {
         target = cg->allocateSinglePrecisionRegister(TR_FPR);
         generateRegRegInstruction(MOVDRegReg4, node, target, intReg, cg);
         }
      else if (cg->useSSEForSinglePrecision())
         {
         target = cg->allocateSinglePrecisionRegister(TR_FPR);
         tempMR = cg->machine()->getDummyLocalMR(TR::Int32);
         generateMemRegInstruction(S4MemReg, node, tempMR, intReg, cg);
         generateRegMemInstruction(MOVSSRegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      else
         {
         target = cg->allocateSinglePrecisionRegister(TR_X87);
         tempMR = cg->machine()->getDummyLocalMR(TR::Int32);
         generateMemRegInstruction(S4MemReg, node, tempMR, intReg, cg);
         generateFPRegMemInstruction(FLDRegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      }

   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }

TR::Register *OMR::X86::TreeEvaluator::fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = cg->allocateRegister();
   TR::MemoryReference  *tempMR;

   // Ref count == 1 check might not be reqd for statics/autos (?)
   //
   if (child->getRegister() == NULL &&
       child->getOpCode().isLoadVar() &&
       (child->getReferenceCount() == 1))
      {
      // Load up the child as an int.
      //
      tempMR = generateX86MemoryReference(child, cg);
      generateRegMemInstruction(L4RegMem, node, target, tempMR, cg);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      // Move the float value from a FPR or XMMR to a GPR.
      //
      TR::Register *floatReg = cg->evaluate(child);
      if (floatReg->getKind() == TR_FPR)
         {
         tempMR = cg->machine()->getDummyLocalMR(TR::Int32);
         generateMemRegInstruction(MOVSSMemReg, node, tempMR, floatReg, cg);
         generateRegMemInstruction(L4RegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      else
         {
         tempMR = cg->machine()->getDummyLocalMR(TR::Int32);
         generateFPMemRegInstruction(FSTMemReg, node, tempMR, floatReg, cg);
         generateRegMemInstruction(L4RegMem, node, target, generateX86MemoryReference(*tempMR, 0, cg), cg);
         }
      }

   // Check for the special case where the child is a NaN,
   // which has to be normalized to a particular NaN value.
   //
   if (node->normalizeNanValues())
      {
      static char *disableFastNormalizeNaNs = feGetEnv("TR_disableFastNormalizeNaNs");
      TR::LabelSymbol *lab0 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      if (disableFastNormalizeNaNs)
         {
         TR::LabelSymbol *lab1 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *lab2 = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         lab0->setStartInternalControlFlow();
         lab2->setEndInternalControlFlow();
         generateLabelInstruction(LABEL, node, lab0, cg);
         generateRegImmInstruction(CMP4RegImm4, node, target, FLOAT_NAN_1_LOW, cg);
         generateLabelInstruction(JGE4, node, lab1, cg);
         generateRegImmInstruction(CMP4RegImm4, node, target, FLOAT_NAN_2_LOW, cg);
         generateLabelInstruction(JB4, node, lab2, cg);
         generateLabelInstruction(LABEL, node, lab1, cg);
         generateRegImmInstruction(MOV4RegImm4, node, target, FLOAT_NAN, cg);
         TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         deps->addPostCondition(target, TR::RealRegister::NoReg, cg);
         generateLabelInstruction(LABEL, node, lab2, deps, cg);
         }
      else
         {
         // A bunch of bookkeeping
         //
         TR::Register *treg    = target;
         uint32_t nanDetector = FLOAT_NAN_2_LOW;

         TR::RegisterDependencyConditions  *internalControlFlowDeps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
         internalControlFlowDeps->addPostCondition(treg, TR::RealRegister::NoReg, cg);

         TR::RegisterDependencyConditions  *helperDeps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
         helperDeps->addPreCondition( treg, TR::RealRegister::eax, cg);
         helperDeps->addPostCondition(treg, TR::RealRegister::eax, cg);

         TR::LabelSymbol *startLabel     = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *slowPathLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *normalizeLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *endLabel       = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         startLabel->setStartInternalControlFlow();
         endLabel  ->setEndInternalControlFlow();

         // Fast path: if subtracting nanDetector leaves CF=0 or OF=1, then it
         // must be a NaN.
         //
         generateLabelInstruction(  LABEL,       node, startLabel,        cg);
         generateRegImmInstruction( CMP4RegImm4, node, treg, nanDetector, cg);
         generateLabelInstruction(  JAE4,        node, slowPathLabel,     cg);
         generateLabelInstruction(  JO4,         node, slowPathLabel,     cg);

         // Slow path
         //
         {
         TR_OutlinedInstructionsGenerator og(slowPathLabel, node, cg);
         generateRegImmInstruction( MOV4RegImm4, node, treg, FLOAT_NAN, cg);
         generateLabelInstruction(  JMP4,        node, endLabel,        cg);
         og.endOutlinedInstructionSequence();
         }

         // Merge point
         //
         generateLabelInstruction(LABEL, node, endLabel, internalControlFlowDeps, cg);
         }
      }

   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }


TR::Register *OMR::X86::TreeEvaluator::fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *globalReg = node->getRegister();
   if (globalReg == NULL)
      {
      if (cg->useSSEForSinglePrecision())
         {
         globalReg = cg->allocateSinglePrecisionRegister(TR_FPR);
         }
      else
         {
         globalReg = cg->allocateSinglePrecisionRegister(TR_X87);

         if (!comp->getJittedMethodSymbol()->usesSinglePrecisionMode() &&
              node->needsPrecisionAdjustment())
            {
            globalReg->setMayNeedPrecisionAdjustment();
            globalReg->setNeedsPrecisionAdjustment();
            }
         }
      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register *OMR::X86::TreeEvaluator::dRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *globalReg = node->getRegister();
   if (globalReg == NULL)
      {
      if (cg->useSSEForDoublePrecision())
         {
         globalReg = cg->allocateRegister(TR_FPR);
         }
      else
         {
         globalReg = cg->allocateRegister(TR_X87);
         if ((comp->getCurrentMethod()->isStrictFP() || comp->getOption(TR_StrictFP)) ||
             node->needsPrecisionAdjustment())
            {
            globalReg->setMayNeedPrecisionAdjustment();
            globalReg->setNeedsPrecisionAdjustment();
            }
         }
      node->setRegister(globalReg);
      }
   return globalReg;
   }


TR::Register *OMR::X86::TreeEvaluator::fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   int32_t globalRegNum = node->getGlobalRegisterNumber();
   TR::Machine *machine = cg->machine();
   int32_t fpStackSlot = globalRegNum - machine->getNumGlobalGPRs();
   TR::Register *childGlobalReg = cg->machine()->getFPStackRegister(fpStackSlot);
   TR::Register *globalReg = cg->evaluate(child);

   if (cg->useSSEForSinglePrecision())
      {
      if (globalReg->getKind() != TR_FPR)
         globalReg = TR::TreeEvaluator::coerceFPRToXMMR(child, globalReg, cg);

      machine->setXMMGlobalRegister(globalRegNum - machine->getNumGlobalGPRs(), globalReg);
      cg->decReferenceCount(child);
      return globalReg;
      }
   else
      {
      machine->setFPStackRegister(globalRegNum - machine->getNumGlobalGPRs(), toX86FPStackRegister(globalReg));

      if (cg->decReferenceCount(child) == 0)
         {
         generateFPSTiST0RegRegInstruction(FSTRegReg, node, globalReg, globalReg, cg);
         cg->stopUsingRegister(globalReg);
         }
      else if (childGlobalReg &&
               childGlobalReg != globalReg)
         {
            {
            int32_t i;
            for (i=0;i<TR_X86FPStackRegister::NumRegisters;i++)
               {
               if (i != fpStackSlot)
                  {
                  TR::Register *reg = cg->machine()->getCopiedFPStackRegister(i);
                  //if (!reg)
                  //   dumpOptDetails("Global reg %d Stack slot %d iglobalreg %d reg address %p childreg %s\n", globalRegNum, fpStackSlot, i+machine->getNumGlobalGPRs(), reg, getDebug()->getName(childGlobalReg));
                  //else
                  //   dumpOptDetails("Global reg %d Stack slot %d iglobalreg %d reg %s childreg %s\n", globalRegNum, fpStackSlot, i+machine->getNumGlobalGPRs(), getDebug()->getName(reg), getDebug()->getName(childGlobalReg));

                  if (reg == childGlobalReg)
                     {
                     TR::Register *popReg = cg->machine()->getFPStackRegister(i);
                     generateFPSTiST0RegRegInstruction(FSTRegReg, node, childGlobalReg, childGlobalReg, cg, true);
                     cg->stopUsingRegister(childGlobalReg);
                     cg->machine()->getFPStackRegisterNode(fpStackSlot)->setRegister(popReg);
                     break;
                     }
                  }
               }
            }
         }

      return globalReg;
      }
   }

TR::Register *OMR::X86::TreeEvaluator::dRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   int32_t globalRegNum = node->getGlobalRegisterNumber();
   TR::Machine *machine = cg->machine();
   int32_t fpStackSlot = globalRegNum - machine->getNumGlobalGPRs();
   TR::Register *childGlobalReg = cg->machine()->getFPStackRegister(fpStackSlot);
   TR::Register *globalReg = cg->evaluate(child);

   if (cg->useSSEForDoublePrecision())
      {
      if (globalReg->getKind() != TR_FPR)
         globalReg = TR::TreeEvaluator::coerceFPRToXMMR(child, globalReg, cg);

      machine->setXMMGlobalRegister(globalRegNum - machine->getNumGlobalGPRs(), globalReg);
      cg->decReferenceCount(child);
      return globalReg;
      }
   else
      {
      machine->setFPStackRegister(globalRegNum - machine->getNumGlobalGPRs(), toX86FPStackRegister(globalReg));

      if (cg->decReferenceCount(child) == 0)
         {
         generateFPSTiST0RegRegInstruction(DSTRegReg, node, globalReg, globalReg, cg);
         cg->stopUsingRegister(globalReg);
         }
      else if (childGlobalReg &&
               childGlobalReg != globalReg)
         {
         //int32_t fpStackSlot = globalRegNum - machine->getNumGlobalGPRs();
         //TR::Register *fpGlobalReg = cg->machine()->getFPStackRegister(fpStackSlot);
         //if (fpGlobalReg)
            {
            int32_t i;
            for (i=0;i<TR_X86FPStackRegister::NumRegisters;i++)
               {
               if (i != fpStackSlot)
                  {
                  TR::Register *reg = cg->machine()->getCopiedFPStackRegister(i);
                  //if (!reg)
                  //   dumpOptDetails("Global reg %d Stack slot %d iglobalreg %d reg address %p childreg %s\n", globalRegNum, fpStackSlot, i+machine->getNumGlobalGPRs(), reg, getDebug()->getName(childGlobalReg));
                  //else
                  //   dumpOptDetails("Global reg %d Stack slot %d iglobalreg %d reg %s childreg %s\n", globalRegNum, fpStackSlot, i+machine->getNumGlobalGPRs(), getDebug()->getName(reg), getDebug()->getName(childGlobalReg));

                  if (reg == childGlobalReg)
                     {
                     TR::Register *popReg = cg->machine()->getFPStackRegister(i);
                     generateFPSTiST0RegRegInstruction(DSTRegReg, node, childGlobalReg, childGlobalReg, cg, true);
                     cg->stopUsingRegister(childGlobalReg);
                     cg->machine()->getFPStackRegisterNode(fpStackSlot)->setRegister(popReg);
                     break;
                     }
                  }
               }
            }
         }


      return globalReg;
      }
   }



TR::Register *OMR::X86::TreeEvaluator::generateBranchOrSetOnFPCompare(TR::Node     *node,
                                                                  TR::Register *accRegister,
                                                                  bool         generateBranch,
                                                                  TR::CodeGenerator *cg)
   {
   List<TR::Register> popRegisters(cg->trMemory());
   TR::Register *targetRegister = NULL;
   TR::RegisterDependencyConditions *deps = NULL;

   if (generateBranch)
      {
      if (node->getNumChildren() == 3 && node->getChild(2)->getNumChildren() != 0)
         {
         TR::Node *third = node->getChild(2);
         cg->evaluate(third);
         deps = generateRegisterDependencyConditions(third, cg, 1, &popRegisters);
         deps->setMayNeedToPopFPRegisters(true);
         deps->stopAddingConditions();
         }
      }

   // If not using FCOMI/UCOMISS/UCOMISD, then we must be interpreting the FPSW in AH;
   // generate an FCMPEVAL pseudo-instruction, which will emit the interpretation code
   // at register assignment time (when we have finalized the sense of the comparison).
   //
   if (accRegister != NULL)
      {
      TR::RegisterDependencyConditions  *accRegDep = generateRegisterDependencyConditions((uint8_t) 1, 1, cg);
      accRegDep->addPreCondition(accRegister, TR::RealRegister::eax, cg);
      accRegDep->addPostCondition(accRegister, TR::RealRegister::eax, cg);
      generateFPCompareEvalInstruction(FCMPEVAL, node, accRegister, accRegDep, cg);
      cg->stopUsingRegister(accRegister);
      }

   // If using UCOMISS/UCOMISD *and* we could not avoid (if)(f|d)cmp(neu|eq),
   // generate multiple instructions. We always avoid FCOMI in these cases.
   // Otherwise, select the appropriate branch or set opcode and emit the
   // instruction.
   //
   TR::ILOpCodes cmpOp = node->getOpCodeValue();

   if (accRegister == NULL &&
       ( cmpOp == TR::iffcmpneu || cmpOp == TR::fcmpneu ||
         cmpOp == TR::ifdcmpneu || cmpOp == TR::dcmpneu ))
      {
      if (generateBranch)
         {
         // We want the pre-conditions on the first branch, and the post-conditions
         // on the last one.
         //
         TR::RegisterDependencyConditions  *deps1 = NULL;
         if (deps && deps->getPreConditions() && deps->getPreConditions()->getMayNeedToPopFPRegisters())
            {
            deps1 = deps->clone(cg);
            deps1->setNumPostConditions(0, cg->trMemory());
            deps->setNumPreConditions(0, cg->trMemory());
            }
         generateLabelInstruction(JPE4, node, node->getBranchDestination()->getNode()->getLabel(), deps1, cg);
         generateLabelInstruction(JNE4, node, node->getBranchDestination()->getNode()->getLabel(), deps, cg);
         }
      else
         {
         TR::Register *tempRegister = cg->allocateRegister();
         targetRegister = cg->allocateRegister();
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(tempRegister);
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(SETPE1Reg, node, tempRegister, cg);
         generateRegInstruction(SETNE1Reg, node, targetRegister, cg);
         generateRegRegInstruction(OR1RegReg, node, targetRegister, tempRegister, cg);
         generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
         cg->stopUsingRegister(tempRegister);
         }
      }
   else if (accRegister == NULL &&
            ( cmpOp == TR::iffcmpeq || cmpOp == TR::fcmpeq ||
              cmpOp == TR::ifdcmpeq || cmpOp == TR::dcmpeq ))
      {
      if (generateBranch)
         {
         TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
         TR::LabelSymbol *fallThroughLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

         startLabel->setStartInternalControlFlow();
         fallThroughLabel->setEndInternalControlFlow();

         TR::RegisterDependencyConditions  *deps1 = NULL;
         if (deps && deps->getPreConditions() && deps->getPreConditions()->getMayNeedToPopFPRegisters())
            {
            deps1 = deps->clone(cg);
            deps1->setNumPostConditions(0, cg->trMemory());
            deps->setNumPreConditions(0, cg->trMemory());
            }
         generateLabelInstruction(LABEL, node, startLabel, cg);
         generateLabelInstruction(JPE4, node, fallThroughLabel, deps1, cg);
         generateLabelInstruction(JE4, node, node->getBranchDestination()->getNode()->getLabel(), cg);
         generateLabelInstruction(LABEL, node, fallThroughLabel, deps, cg);
         }
      else
         {
         TR::Register *tempRegister = cg->allocateRegister();
         targetRegister = cg->allocateRegister();
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(tempRegister);
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(SETPO1Reg, node, tempRegister, cg);
         generateRegInstruction(SETE1Reg, node, targetRegister, cg);
         generateRegRegInstruction(AND1RegReg, node, targetRegister, tempRegister, cg);
         generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
         cg->stopUsingRegister(tempRegister);
         }
      }
   else
      {
      TR_X86OpCodes op = getBranchOrSetOpCodeForFPComparison(node->getOpCodeValue(), (accRegister == NULL));
      if (generateBranch)
         {
         generateLabelInstruction(op, node, node->getBranchDestination()->getNode()->getLabel(), deps, cg);
         }
      else
         {
         targetRegister = cg->allocateRegister();
         cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
         generateRegInstruction(op, node, targetRegister, cg);
         generateRegRegInstruction(MOVZXReg4Reg1, node, targetRegister, targetRegister, cg);
         }
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

   node->setRegister(targetRegister);
   return targetRegister;
   }



// Create FP compare code for the specified comparison class.
// Returns the register in which FP status word has been stored;
// NULL if EFLAGS is used.
//
TR::Register *OMR::X86::TreeEvaluator::compareFloatOrDoubleForOrder(TR::Node        *node,
                                                                TR_X86OpCodes  fpCmpRegRegOpCode,
                                                                TR_X86OpCodes  fpCmpRegMemOpCode,
                                                                TR_X86OpCodes  fpCmpiRegRegOpCode,
                                                                TR_X86OpCodes  xmmCmpRegRegOpCode,
                                                                TR_X86OpCodes  xmmCmpRegMemOpCode,
                                                                bool            useFCOMIInstructions,
                                                                TR::CodeGenerator *cg)
   {
   if (
      ((TR_X86OpCode::singleFPOp(fpCmpRegRegOpCode) && cg->useSSEForSinglePrecision()) ||
       (TR_X86OpCode::doubleFPOp(fpCmpRegRegOpCode) && cg->useSSEForDoublePrecision())))
      {
      TR_IA32XMMCompareAnalyser temp(cg);
      return temp.xmmCompareAnalyser(node, xmmCmpRegRegOpCode, xmmCmpRegMemOpCode);
      }
   else
      {
      TR_X86FPCompareAnalyser  temp(cg);
      return temp.fpCompareAnalyser(node,
                                    fpCmpRegRegOpCode,
                                    fpCmpRegMemOpCode,
                                    fpCmpiRegRegOpCode,
                                    useFCOMIInstructions);
      }
   }


TR::Register *OMR::X86::TreeEvaluator::generateFPCompareResult(TR::Node *node, TR::Register *accRegister, TR::CodeGenerator *cg)
   {
   if (accRegister != NULL)
      {
      TR::RegisterDependencyConditions  *accRegDep = generateRegisterDependencyConditions((uint8_t) 1, 1, cg);
      accRegDep->addPreCondition(accRegister, TR::RealRegister::eax, cg);
      accRegDep->addPostCondition(accRegister, TR::RealRegister::eax, cg);
      generateFPCompareEvalInstruction(FCMPEVAL, node, accRegister, accRegDep, cg);
      cg->stopUsingRegister(accRegister);
      }

   TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *doneLabel  = TR::LabelSymbol::create(cg->trHeapMemory(),cg);

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();
   generateLabelInstruction(LABEL, node, startLabel, cg);

   TR::Register *targetRegister = cg->allocateRegister(TR_GPR);
   cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(targetRegister);
   generateRegInstruction(SETA1Reg, node, targetRegister, cg);
   generateLabelInstruction(JAE4, node, doneLabel, cg);

   if (node->getOpCodeValue() == TR::fcmpg || node->getOpCodeValue() == TR::dcmpg)
      {
      generateRegInstruction(SETPE1Reg, node, targetRegister, cg);
      generateLabelInstruction(JPE4, node, doneLabel, cg);
      }

   generateRegInstruction(DEC1Reg, node, targetRegister, cg);

   TR::RegisterDependencyConditions  *deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);
   deps->addPostCondition(targetRegister, TR::RealRegister::NoReg, cg);
   generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

   generateRegRegInstruction(MOVSXReg4Reg1, node, targetRegister, targetRegister, cg);

   node->setRegister(targetRegister);
   return targetRegister;
   }


bool OMR::X86::TreeEvaluator::canUseFCOMIInstructions(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes cmpOp = node->getOpCodeValue();

   return (!cg->getX86ProcessorInfo().supportsFCOMIInstructions() ||
           cmpOp == TR::iffcmpneu ||
           cmpOp == TR::iffcmpeq  ||
           cmpOp == TR::ifdcmpneu ||
           cmpOp == TR::ifdcmpeq ||
           cmpOp == TR::fcmpneu ||
           cmpOp == TR::dcmpneu ||
           cmpOp == TR::fcmpeq ||
           cmpOp == TR::dcmpeq) ? false : true;
   }


TR::Register *OMR::X86::TreeEvaluator::compareFloatAndBranchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool useFCOMIInstructions = TR::TreeEvaluator::canUseFCOMIInstructions(node, cg);
   TR::Register *accRegister = TR::TreeEvaluator::compareFloatOrDoubleForOrder(node,
                                                           FCOMRegReg, FCOMRegMem, FCOMIRegReg,
                                                           UCOMISSRegReg, UCOMISSRegMem,
                                                           useFCOMIInstructions, cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, accRegister, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareDoubleAndBranchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool useFCOMIInstructions = TR::TreeEvaluator::canUseFCOMIInstructions(node, cg);
   TR::Register *accRegister = TR::TreeEvaluator::compareFloatOrDoubleForOrder(node,
                                                           DCOMRegReg, DCOMRegMem, DCOMIRegReg,
                                                           UCOMISDRegReg, UCOMISDRegMem,
                                                           useFCOMIInstructions, cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, accRegister, true, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareFloatAndSetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool useFCOMIInstructions = TR::TreeEvaluator::canUseFCOMIInstructions(node, cg);
   TR::Register *accRegister = TR::TreeEvaluator::compareFloatOrDoubleForOrder(node,
                                                           FCOMRegReg, FCOMRegMem, FCOMIRegReg,
                                                           UCOMISSRegReg, UCOMISSRegMem,
                                                           useFCOMIInstructions, cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, accRegister, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareDoubleAndSetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool useFCOMIInstructions = TR::TreeEvaluator::canUseFCOMIInstructions(node, cg);
   TR::Register *accRegister = TR::TreeEvaluator::compareFloatOrDoubleForOrder(node,
                                                           DCOMRegReg, DCOMRegMem, DCOMIRegReg,
                                                           UCOMISDRegReg, UCOMISDRegMem,
                                                           useFCOMIInstructions, cg);
   return TR::TreeEvaluator::generateBranchOrSetOnFPCompare(node, accRegister, false, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareFloatEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool useFCOMIInstructions = TR::TreeEvaluator::canUseFCOMIInstructions(node, cg);
   TR::Register *accRegister = TR::TreeEvaluator::compareFloatOrDoubleForOrder(node,
                                                           FCOMRegReg, FCOMRegMem, FCOMIRegReg,
                                                           UCOMISSRegReg, UCOMISSRegMem,
                                                           useFCOMIInstructions, cg);
   return TR::TreeEvaluator::generateFPCompareResult(node, accRegister, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::compareDoubleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool useFCOMIInstructions = TR::TreeEvaluator::canUseFCOMIInstructions(node, cg);
   TR::Register *accRegister = TR::TreeEvaluator::compareFloatOrDoubleForOrder(node,
                                                           DCOMRegReg, DCOMRegMem, DCOMIRegReg,
                                                           UCOMISDRegReg, UCOMISDRegMem,
                                                           useFCOMIInstructions, cg);
   return TR::TreeEvaluator::generateFPCompareResult(node, accRegister, cg);
   }
