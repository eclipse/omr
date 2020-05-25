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

#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/RegisterRematerialization.hpp"
#include "x/codegen/X86Evaluator.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"
#include "x/codegen/BinaryCommutativeAnalyser.hpp"
#include "x/codegen/SubtractAnalyser.hpp"

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;

extern bool existsNextInstructionToTestFlags(TR::Instruction *startInstr,
                                             uint8_t         testMask);

TR::Register *OMR::X86::TreeEvaluator::ifxcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR_ASSERT((opCode == TR::ificmno) || (opCode == TR::ificmnno) ||
             (opCode == TR::iflcmno) || (opCode == TR::iflcmnno) ||
             (opCode == TR::ificmpo) || (opCode == TR::ificmpno) ||
             (opCode == TR::iflcmpo) || (opCode == TR::iflcmpno), "invalid opcode");

   bool nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(node->getFirstChild(), cg);
   bool reverseBranch = (opCode == TR::ificmnno) || (opCode == TR::iflcmnno) || (opCode == TR::ificmpno) || (opCode == TR::iflcmpno);

   TR::Register* rs1 = cg->evaluate(node->getFirstChild());
   TR::Register* rs2 = cg->evaluate(node->getSecondChild());

   if ((opCode == TR::ificmno) || (opCode == TR::ificmnno) ||
       (opCode == TR::iflcmno) || (opCode == TR::iflcmnno))
      {
      TR::Register* tmp = cg->allocateRegister();
      generateRegRegInstruction(MOVRegReg(nodeIs64Bit), node, tmp, rs1, cg);
      generateRegRegInstruction(ADDRegReg(nodeIs64Bit), node, tmp, rs2, cg);
      cg->stopUsingRegister(tmp);
      }
   else
      generateRegRegInstruction(CMPRegReg(nodeIs64Bit), node, rs1, rs2, cg);

   generateConditionalJumpInstruction(reverseBranch ? JNO4 : JO4, node, cg, true);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::intOrLongClobberEvaluate(TR::Node *node, bool nodeIs64Bit, TR::CodeGenerator *cg)
   {
   // TODO:AMD64: This belongs in CodeGenerator.  In fact, this whole clobberEvaluate interface needs another look.
   if (nodeIs64Bit)
      {
      TR_ASSERT(TR::TreeEvaluator::getNodeIs64Bit(node, cg), "nodeIs64Bit must be consistent with node size");
      return cg->longClobberEvaluate(node);
      }
   else
      {
      TR_ASSERT(!TR::TreeEvaluator::getNodeIs64Bit(node, cg), "nodeIs64Bit must be consistent with node size");
      return cg->intClobberEvaluate(node);
      }
   }

TR::Instruction *OMR::X86::TreeEvaluator::compareGPMemoryToImmediate(TR::Node                *node,
                                                                TR::MemoryReference  *mr,
                                                                int32_t                 value,
                                                                TR::CodeGenerator       *cg)
   {
   // On IA32, this is called to do half of an 8-byte compare, so even though
   // the node is 64 bit, we should do a 32-bit compare
   bool is64Bit = cg->comp()->target().is64Bit()? TR::TreeEvaluator::getNodeIs64Bit(node->getFirstChild(), cg) : false;
   TR_X86OpCodes cmpOp = (value >= -128 && value <= 127) ? CMPMemImms(is64Bit) : CMPMemImm4(is64Bit);
   TR::Instruction *instr = generateMemImmInstruction(cmpOp, node, mr, value, cg);
   cg->setImplicitExceptionPoint(instr);
   return instr;
   }

void OMR::X86::TreeEvaluator::compareGPRegisterToImmediate(TR::Node          *node,
                                                       TR::Register      *cmpRegister,
                                                       int32_t           value,
                                                       TR::CodeGenerator *cg)
   {
   // On IA32, this is called to do half of an 8-byte compare, so even though
   // the node is 64 bit, we should do a 32-bit compare
   bool is64Bit = cg->comp()->target().is64Bit()? TR::TreeEvaluator::getNodeIs64Bit(node->getFirstChild(), cg) : false;
   TR_X86OpCodes cmpOp = (value >= -128 && value <= 127) ? CMPRegImms(is64Bit) : CMPRegImm4(is64Bit);
   generateRegImmInstruction(cmpOp, node, cmpRegister, value, cg);
   }

void OMR::X86::TreeEvaluator::compareGPRegisterToImmediateForEquality(TR::Node          *node,
                                                                  TR::Register      *cmpRegister,
                                                                  int32_t           value,
                                                                  TR::CodeGenerator *cg)
   {
   // On IA32, this is called to do half of an 8-byte compare, so even though
   // the node is 64 bit, we should do a 32-bit compare
   bool is64Bit = cg->comp()->target().is64Bit()? TR::TreeEvaluator::getNodeIs64Bit(node->getFirstChild(), cg) : false;
   TR_X86OpCodes cmpOp = (value >= -128 && value <= 127) ? CMPRegImms(is64Bit) : CMPRegImm4(is64Bit);
   if (value==0)
      generateRegRegInstruction(TESTRegReg(is64Bit), node, cmpRegister, cmpRegister, cg);
   else
      generateRegImmInstruction(cmpOp, node, cmpRegister, value, cg);
   }

TR::Instruction *OMR::X86::TreeEvaluator::insertLoadConstant(TR::Node                  *node,
                                                        TR::Register              *target,
                                                        intptr_t                 value,
                                                        TR_RematerializableTypes  type,
                                                        TR::CodeGenerator         *cg,
                                                        TR::Instruction           *currentInstruction)
   {
   TR::Compilation *comp = cg->comp();
   static const TR_X86OpCodes ops[TR_NumRematerializableTypes+1][3] =
      //    load 0      load -1     load c
      { { BADIA32Op,  BADIA32Op,  BADIA32Op   },   // LEA; should not seen here
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Byte constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Short constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Char constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Int constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // 32-bit address constant
        { XOR4RegReg, OR8RegImms, BADIA32Op   } }; // Long address constant; MOVs handled specially

   enum { XOR = 0, OR  = 1, MOV = 2 };

   bool is64Bit = false;

   int opsRow = type;
   if (cg->comp()->target().is64Bit())
      {
      if (type == TR_RematerializableAddress)
         {
         // Treat 64-bit addresses as longs
         opsRow++;
         is64Bit = true;
         }
      else
         {
         is64Bit = (type == TR_RematerializableLong);
         }
      }
   else
      {
      TR_ASSERT(type != TR_RematerializableLong, "Longs are rematerialized as pairs of ints on IA32");
      }

   TR_ExternalRelocationTargetKind reloKind = TR_NoRelocation;
   if (cg->profiledPointersRequireRelocation() && node && node->getOpCodeValue() == TR::aconst &&
         (node->isClassPointerConstant() || node->isMethodPointerConstant()))
      {
      if (node->isClassPointerConstant())
         reloKind = TR_ClassPointer;
      else if (node->isMethodPointerConstant())
         reloKind = TR_MethodPointer;
      else
         TR_ASSERT(0, "Unexpected node, don't know how to relocate");
      }

   if (currentInstruction)
      {
      // Optimized loads inserted arbitrarily into the instruction stream must be checked
      // to ensure they don't modify any eflags needed by surrounding instructions.
      //
      if ((value == 0 || value == -1))
         {
         uint8_t EFlags = TR_X86OpCode::getModifiedEFlags(ops[opsRow][((value == 0) ? XOR : OR)]);

         if (existsNextInstructionToTestFlags(currentInstruction, EFlags) || cg->requiresCarry())
            {
            // Can't alter flags, so must use MOV.  Fall through.
            }
         else if (value == 0)
            return generateRegRegInstruction(currentInstruction, ops[opsRow][XOR], target, target, cg);
         else if (value == -1)
            return generateRegImmInstruction(currentInstruction, ops[opsRow][OR], target, (uint32_t)-1, cg);
         }

      // No luck optimizing this.  Just use a MOV
      //
      TR::Instruction *movInstruction = NULL;
      if (is64Bit)
         {
         if (cg->constantAddressesCanChangeSize(node) && node && node->getOpCodeValue() == TR::aconst &&
             (node->isClassPointerConstant() || node->isMethodPointerConstant()))
            {
            movInstruction = generateRegImm64Instruction(currentInstruction, MOV8RegImm64, target, value, cg, reloKind);
            }
         else if (IS_32BIT_UNSIGNED(value))
            {
            // zero-extended 4-byte MOV
            movInstruction = generateRegImmInstruction(currentInstruction, MOV4RegImm4, target, value, cg, reloKind);
            }
         else if (IS_32BIT_SIGNED(value)) // TODO:AMD64: Is there some way we could get RIP too?
            {
            movInstruction = generateRegImmInstruction(currentInstruction, MOV8RegImm4, target, value, cg, reloKind);
            }
         else
            {
            movInstruction = generateRegImm64Instruction(currentInstruction, MOV8RegImm64, target, value, cg, reloKind);
            }
         }
      else
         {
         movInstruction = generateRegImmInstruction(currentInstruction, ops[opsRow][MOV], target, value, cg, reloKind);
         }

      // HCR register PIC site in TR::TreeEvaluator::insertLoadConstant
      TR::Symbol *symbol = NULL;
      if (node && node->getOpCode().hasSymbolReference())
         symbol = node->getSymbol();
      bool isPICCandidate = symbol ? target && symbol->isStatic() && symbol->isClassObject() : false;
      if (isPICCandidate && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)value, node))
         comp->getStaticHCRPICSites()->push_front(movInstruction);

      if (target && node &&
          node->getOpCodeValue() == TR::aconst &&
          node->isClassPointerConstant() &&
          (cg->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock *) node->getAddress(),
                                       comp->getCurrentMethod()) ||
           cg->profiledPointersRequireRelocation()))
         {
         comp->getStaticPICSites()->push_front(movInstruction);
         }

      if (target && node &&
          node->getOpCodeValue() == TR::aconst &&
          node->isMethodPointerConstant() &&
          (cg->fe()->isUnloadAssumptionRequired(cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) node->getAddress(), comp->getCurrentMethod())->classOfMethod(), comp->getCurrentMethod()) ||
           cg->profiledPointersRequireRelocation()))
         {
         traceMsg(comp, "Adding instr %p to MethodPICSites for node %p\n", movInstruction, node);
         comp->getStaticMethodPICSites()->push_front(movInstruction);
         }

      return movInstruction;
      }
   else
      {
      // constant loads between a compare and a branch cannot clobber the EFLAGS register
      bool canClobberEFLAGS = !(cg->getCurrentEvaluationTreeTop()->getNode()->getOpCode().isIf() || cg->requiresCarry());

      if (value == 0 && canClobberEFLAGS)
         {
         return generateRegRegInstruction(ops[opsRow][XOR], node, target, target, cg);
         }
      else if (value == -1 && canClobberEFLAGS)
         {
         return generateRegImmInstruction(ops[opsRow][OR], node, target, (uint32_t)-1, cg);
         }
      else
         {
         TR::Instruction *movInstruction = NULL;
         if (is64Bit)
            {
            if (cg->constantAddressesCanChangeSize(node) && node && node->getOpCodeValue() == TR::aconst &&
                (node->isClassPointerConstant() || node->isMethodPointerConstant()))
               {
               movInstruction = generateRegImm64Instruction(MOV8RegImm64, node, target, value, cg, reloKind);
               }
            else if (IS_32BIT_UNSIGNED(value))
               {
               // zero-extended 4-byte MOV
               movInstruction = generateRegImmInstruction(MOV4RegImm4, node, target, value, cg, reloKind);
               }
            else if (IS_32BIT_SIGNED(value)) // TODO:AMD64: Is there some way we could get RIP too?
               {
               movInstruction = generateRegImmInstruction(MOV8RegImm4, node, target, value, cg, reloKind);
               }
            else
               {
               movInstruction = generateRegImm64Instruction(MOV8RegImm64, node, target, value, cg, reloKind);
               }
            }
         else
            {
            movInstruction = generateRegImmInstruction(ops[opsRow][MOV], node, target, value, cg, reloKind);
            }

         // HCR register PIC site in TR::TreeEvaluator::insertLoadConstant
         TR::Symbol *symbol = NULL;
         if (node && node->getOpCode().hasSymbolReference())
            symbol = node->getSymbol();
         bool isPICCandidate = symbol ? target && symbol->isStatic() && symbol->isClassObject() : false;
         if (isPICCandidate && comp->getOption(TR_EnableHCR))
            {
            comp->getStaticHCRPICSites()->push_front(movInstruction);
            }

         if (target &&
             node &&
             node->getOpCodeValue() == TR::aconst &&
             node->isClassPointerConstant() &&
             (cg->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock *) node->getAddress(),
                               comp->getCurrentMethod()) ||
              cg->profiledPointersRequireRelocation()))
            {
            comp->getStaticPICSites()->push_front(movInstruction);
            }

        if (target && node &&
            node->getOpCodeValue() == TR::aconst &&
            node->isMethodPointerConstant() &&
            (cg->fe()->isUnloadAssumptionRequired(cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) node->getAddress(), comp->getCurrentMethod())->classOfMethod(), comp->getCurrentMethod()) ||
             cg->profiledPointersRequireRelocation()))
            {
            traceMsg(comp, "Adding instr %p to MethodPICSites for node %p\n", movInstruction, node);
            comp->getStaticMethodPICSites()->push_front(movInstruction);
            }

         return movInstruction;
         }
      }
   }

TR::Register *OMR::X86::TreeEvaluator::loadConstant(TR::Node * node, intptr_t value, TR_RematerializableTypes type, TR::CodeGenerator *cg, TR::Register *targetRegister)
   {
   if (targetRegister == NULL)
      {
      targetRegister = cg->allocateRegister();
      }

   TR::Instruction *instr = TR::TreeEvaluator::insertLoadConstant(node, targetRegister, value, type, cg);

   if (cg->enableRematerialisation())
      {
      if (node && node->getOpCode().hasSymbolReference() && node->getSymbol() && node->getSymbol()->isClassObject())
         (TR::Compiler->om.generateCompressedObjectHeaders() || cg->comp()->target().is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;

      setDiscardableIfPossible(type, targetRegister, node, instr, value, cg);
      }

   return targetRegister;
   }

TR::Instruction *
OMR::X86::TreeEvaluator::insertLoadMemory(
      TR::Node *node,
      TR::Register *target,
      TR::MemoryReference *tempMR,
      TR_RematerializableTypes type,
      TR::CodeGenerator *cg,
      TR::Instruction *currentInstruction)
   {
   TR::Compilation *comp = cg->comp();
   static const TR_X86OpCodes ops[TR_NumRematerializableTypes] =
      {
      LEARegMem(),  // Load Effective Address
      L1RegMem,     // Byte
      L2RegMem,     // Short
      L2RegMem,     // Char
      L4RegMem,     // Int
      L4RegMem,     // Address (64-bit addresses handled specailly below)
      L8RegMem,     // Long
      };

   TR_X86OpCodes opCode = ops[type];
   if (cg->comp()->target().is64Bit())
      {
      if (type == TR_RematerializableAddress)
         {
         opCode = L8RegMem;
         if (node && node->getOpCode().hasSymbolReference() &&
               TR::Compiler->om.generateCompressedObjectHeaders())
            {
            if (node->getSymbol()->isClassObject() ||
                  (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef()))
               opCode = L4RegMem;
            }
         }
      }
   else
      TR_ASSERT(type != TR_RematerializableLong, "Longs are rematerialized as pairs of ints on IA32");

   // If we are trying to rematerialize a byte of mem into a non-byte register,
   // use movzx rather than a simple move
   //
   if (type == TR_RematerializableByte && target->getAssignedRealRegister())
      {
      TR::RealRegister::RegNum r = toRealRegister(target->getAssignedRealRegister())->getRegisterNumber();
      if (r > TR::RealRegister::Last8BitGPR)
         opCode = MOVZXReg4Mem1;
      }

   TR::Instruction *i;
   if (currentInstruction)
      {
      i = generateRegMemInstruction(currentInstruction, opCode, target, tempMR, cg);
      }
   else
      {
      i = generateRegMemInstruction(opCode, node, target, tempMR, cg);
      }

   // HCR in insertLoadMemory to do: handle unresolved data
   if (node && node->getSymbol()->isStatic() && node->getSymbol()->isClassObject() && cg->wantToPatchClassPointer(NULL, node))
      {
      // I think this has no effect; i has no immediate source operand.
      comp->getStaticHCRPICSites()->push_front(i);
      }
   return i;
   }


// If an unresolved data reference is followed immediately by an unconditional
// branch instruction then it is possible that the branch displacement could
// change after the unresolved data snippet has been generated (due to a forward
// reference).  Insert padding such that the bytes copied from the mainline
// code do not change.
//
// The padding is always inserted for certain classes of unresolved references
// because it is difficult to accurately predict when the following instruction
// will actually be one with a forward reference.  This could potentially be
// solved cleaner by running relocation processing after snippet generation.
//
// Also if the unresolved data reference is immediately followed by another patchable
// instruction (such as a virtual guard nop) with a different alignment boundary size
// then there could be an overlap between the two codes being patching, unless
// further padding is inserted.
//
void OMR::X86::TreeEvaluator::padUnresolvedDataReferences(
      TR::Node *node,
      TR::SymbolReference &symRef,
      TR::CodeGenerator *cg)
   {
   TR_ASSERT(symRef.isUnresolved(), "expecting an unresolved symbol");

   TR::Compilation *comp = cg->comp();
   uint8_t padBytes = 0;
   if (cg->comp()->target().is32Bit())
      padBytes = 2; // needs at least 2 bytes as we are patching 8 bytes at a time
   else
      {
      TR::Symbol *symbol = symRef.getSymbol();
      if (symbol && symbol->isShadow())
         {
         padBytes = 2;
         }
      }

   if (padBytes > 0)
      {
      TR::Instruction *pi = generatePaddingInstruction(padBytes, node, cg);
      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "adding %d pad bytes following unresolved data instruction %p\n", padBytes, pi->getPrev());
      }
   }


TR::Register *
OMR::X86::TreeEvaluator::loadMemory(
      TR::Node *node,
      TR::MemoryReference *sourceMR,
      TR_RematerializableTypes type,
      bool markImplicitExceptionPoint,
      TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->allocateRegister();
   TR::Instruction *instr;
   instr = TR::TreeEvaluator::insertLoadMemory(node, targetRegister, sourceMR, type, cg);

   TR::SymbolReference& symRef = sourceMR->getSymbolReference();
   if (symRef.isUnresolved())
      {
      TR::TreeEvaluator::padUnresolvedDataReferences(node, symRef, cg);
      }

   if (cg->enableRematerialisation())
      {
      if (node && node->getOpCode().hasSymbolReference() && node->getSymbol() && node->getSymbol()->isClassObject())
         (TR::Compiler->om.generateCompressedObjectHeaders() || cg->comp()->target().is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;

      setDiscardableIfPossible(type, targetRegister, node, instr, sourceMR, cg);
      }

   if (markImplicitExceptionPoint)
      cg->setImplicitExceptionPoint(instr);


   return targetRegister;
   }

void OMR::X86::TreeEvaluator::removeLiveDiscardableStatics(TR::CodeGenerator *cg)
   {
   TR_ASSERT(cg->supportsStaticMemoryRematerialization(),
          "should not be called without rematerialisation of statics\n");

   TR::Compilation *comp = cg->comp();
   for(auto iterator = cg->getLiveDiscardableRegisters().begin(); iterator != cg->getLiveDiscardableRegisters().end(); )
      {
      if ((*iterator)->getRematerializationInfo()->isRematerializableFromMemory() &&
          (*iterator)->getRematerializationInfo()->getSymbolReference()->getSymbol()->isStatic())
         {
         TR::Register* regCursor = *iterator;
         iterator = cg->getLiveDiscardableRegisters().erase(iterator);
         regCursor->resetIsDiscardable();

         if (debug("dumpRemat"))
            {
            diagnostic("\n---> Deleting static discardable candidate %s at %s instruction " POINTER_PRINTF_FORMAT,
                  regCursor->getRegisterName(comp),
                  cg->getAppendInstruction()->getOpCode().getOpCodeName(cg),
                  cg->getAppendInstruction());
            }
         }
      else
         ++iterator;
      }
   }

TR::Register *OMR::X86::TreeEvaluator::performIload(TR::Node *node, TR::MemoryReference  *sourceMR, TR::CodeGenerator *cg)
   {
   TR::Register *reg = TR::TreeEvaluator::loadMemory(node, sourceMR, TR_RematerializableInt, node->getOpCode().isIndirect(), cg);

   node->setRegister(reg);
   return reg;
   }

// also handles iaload
TR::Register *OMR::X86::TreeEvaluator::aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *sourceMR = generateX86MemoryReference(node, cg);
   TR::Register         *reg      = TR::TreeEvaluator::loadMemory(node, sourceMR, TR_RematerializableAddress, node->getOpCode().isIndirect(), cg);
   reg->setMemRef(sourceMR);
   TR::Compilation *comp = cg->comp();

   if (!node->getSymbolReference()->isUnresolved() &&
       (node->getSymbolReference()->getSymbol()->getKind() == TR::Symbol::IsShadow) &&
       (node->getSymbolReference()->getCPIndex() >= 0) &&
       (comp->getMethodHotness()>=scorching))
      {
      int32_t len;
      const char *fieldName = node->getSymbolReference()->getOwningMethod(comp)->fieldSignatureChars(
            node->getSymbolReference()->getCPIndex(), len);

      if (fieldName && strstr(fieldName, "Ljava/lang/String;"))
         {
         generateMemInstruction(PREFETCHT0, node, generateX86MemoryReference(reg, 0, cg), cg);
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   if (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
      TR::TreeEvaluator::generateVFTMaskInstruction(node, reg, cg);
#endif

   if (node->getSymbolReference()->getSymbol()->isNotCollected())
      {
      if (node->getSymbolReference()->getSymbol()->isInternalPointer())
         {
         reg->setContainsInternalPointer();
         reg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
         }
      }
   else
      {
      if (node->getSymbolReference()->getSymbol()->isInternalPointer())
         {
         reg->setContainsInternalPointer();
         reg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
         }
      else
         {
         reg->setContainsCollectedReference();
         }
      }

   node->setRegister(reg);
   sourceMR->decNodeReferenceCounts(cg);
   return reg;
   }

// generate the null test instructions if required
//
bool OMR::X86::TreeEvaluator::genNullTestSequence(TR::Node *node,
                                              TR::Register *opReg,
                                              TR::Register *targetReg,
                                              TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (comp->useCompressedPointers() &&
         node->containsCompressionSequence())
      {
      TR::Node *n = node;
      bool isNonZero = false;
      if (n->isNonZero())
         isNonZero = true;

      if (n->getOpCodeValue() == TR::ladd)
         {
         if (n->getFirstChild()->isNonZero())
            isNonZero = true;

         if (n->getFirstChild()->getOpCodeValue() == TR::iu2l ||
             n->getFirstChild()->getOpCode().isShift())
            {
            if (n->getFirstChild()->getFirstChild()->isNonZero())
               isNonZero = true;
            }
         }

      if (!isNonZero)
         {
         if (opReg != targetReg)
            generateRegRegInstruction(MOVRegReg(), node, targetReg, opReg, cg);
         TR::Register *tempReg = cg->allocateRegister();
         // addition of the negative number should result in 0
         //
         int64_t value = -(int64_t)TR::Compiler->vm.heapBaseAddress();
         generateRegImm64Instruction(MOV8RegImm64, node, tempReg, value, cg);
         if (n->getFirstChild()->getOpCode().isShift() && n->getFirstChild()->getFirstChild()->getRegister())
            {
            TR::Register * r = n->getFirstChild()->getFirstChild()->getRegister();
            generateRegRegInstruction(TESTRegReg(), node, r, r, cg);
            }
         else
            {
            generateRegRegInstruction(TESTRegReg(), node, opReg, opReg, cg);
            }
         generateRegRegInstruction(CMOVERegReg(), node, targetReg, tempReg, cg);
         cg->stopUsingRegister(tempReg);
         return true;
         }
      }
   return false;
   }


// also handles iiload
TR::Register *OMR::X86::TreeEvaluator::iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *sourceMR = generateX86MemoryReference(node, cg);
   TR::Register         *reg      = TR::TreeEvaluator::performIload(node, sourceMR, cg);
   reg->setMemRef(sourceMR);
   sourceMR->decNodeReferenceCounts(cg);
   TR::Compilation *comp = cg->comp();
   if (comp->useCompressedPointers() &&
         (node->getOpCode().hasSymbolReference() &&
           node->getSymbolReference()->getSymbol()->getDataType() == TR::Address))
      {
      if (!node->getSymbolReference()->isUnresolved() &&
          (node->getSymbolReference()->getSymbol()->getKind() == TR::Symbol::IsShadow) &&
          (node->getSymbolReference()->getCPIndex() >= 0) &&
          (comp->getMethodHotness()>=scorching))
         {
         int32_t len;
         const char *fieldName = node->getSymbolReference()->getOwningMethod(comp)->fieldSignatureChars(
                node->getSymbolReference()->getCPIndex(), len);

         if (fieldName && strstr(fieldName, "Ljava/lang/String;"))
            {
            generateMemInstruction(PREFETCHT0, node, generateX86MemoryReference(reg, 0, cg), cg);
            }
         }
      }
   return reg;
   }

// also handles ibload
TR::Register *OMR::X86::TreeEvaluator::bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *sourceMR = generateX86MemoryReference(node, cg);
   TR::Register         *reg      = TR::TreeEvaluator::loadMemory(node, sourceMR, TR_RematerializableByte, node->getOpCode().isIndirect(), cg);

   reg->setMemRef(sourceMR);
   node->setRegister(reg);

   if (cg->enableRegisterInterferences())
      cg->getLiveRegisters(TR_GPR)->setByteRegisterAssociation(reg);

   sourceMR->decNodeReferenceCounts(cg);
   return reg;
   }

// also handles cload, isload and icload
TR::Register *OMR::X86::TreeEvaluator::sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference  *sourceMR = generateX86MemoryReference(node, cg);
   TR::Register         *reg      = TR::TreeEvaluator::loadMemory(node, sourceMR, TR_RematerializableShort, node->getOpCode().isIndirect(), cg);

   reg->setMemRef(sourceMR);
   node->setRegister(reg);
   sourceMR->decNodeReferenceCounts(cg);
   return reg;
   }

TR::Register *
OMR::X86::TreeEvaluator::fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

TR::Register *
OMR::X86::TreeEvaluator::fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
   }

// cloadEvaluator handled by sloadEvaluator

// iiload handled by iloadEvaluator

// ilload handled by lloadEvaluator

// ialoadEvaluator handled by iloadEvaluator

// ibloadEvaluator handled by bloadEvaluator

// isloadEvaluator handled by sloadEvaluator

// icloadEvaluator handled by sloadEvaluator

// also used for iistore, astore and iastore
TR::Register *OMR::X86::TreeEvaluator::integerStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   TR::Node *valueChild;
   TR::Register *valueReg = NULL;
   bool genLockedOpOutOfline = true;
   TR::Compilation *comp = cg->comp();

   bool usingCompressedPointers = false;
   bool usingLowMemHeap = false;

   node->getFirstChild()->oneParentSupportsLazyClobber(comp);
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      valueChild->oneParentSupportsLazyClobber(comp);
      if (comp->useCompressedPointers() &&
            (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address))
         {
         // pattern match the sequence
         //     iistore f     iistore f         <- node
         //       aload O       aload O
         //     value           l2i
         //                       lshr
         //                         lsub        <- translatedNode
         //                           a2l
         //                             value   <- valueChild
         //                           lconst HB
         //                         iconst shftKonst
         //
         // -or- if the field is known to be null or usingLowMemHeap
         // iistore f
         //    aload O
         //    l2i
         //      a2l
         //        value  <- valueChild
         //
         TR::Node *translatedNode = valueChild;
         bool isSequence = false;
         if (translatedNode->getOpCodeValue() == TR::l2i)
            {
            translatedNode = translatedNode->getFirstChild();
            isSequence = true;
            }
         if (translatedNode->getOpCode().isRightShift())
            translatedNode = translatedNode->getFirstChild(); //optional

         if (TR::Compiler->vm.heapBaseAddress() == 0 ||
               valueChild->isNull())
            usingLowMemHeap = true;

         if (isSequence && (translatedNode->getOpCode().isSub() || usingLowMemHeap))
            usingCompressedPointers = true;

         if (usingCompressedPointers)
            {
            if (!usingLowMemHeap)
               {
               while ((valueChild->getNumChildren() > 0) &&
                        (valueChild->getOpCodeValue() != TR::a2l))
                  valueChild = valueChild->getFirstChild();
               if (valueChild->getOpCodeValue() == TR::a2l)
               valueChild = valueChild->getFirstChild();
               // this is required so that different registers are
               // allocated for the actual store and translated values
               valueChild->incReferenceCount();
               }
            }
         }
      }
   else
      valueChild = node->getFirstChild();

   int32_t                 size   = node->getOpCode().getSize();
   TR::MemoryReference  *tempMR = NULL;
   TR::Instruction      *instr = NULL;
   TR_X86OpCodes          opCode;
   TR::Node                *originalValueChild = valueChild;

   bool childIsConstant = false;

   if (valueChild->getOpCode().isLoadConst() &&
       valueChild->getRegister() == NULL &&
       !usingCompressedPointers)
      {
      childIsConstant = true;
      }

   if (childIsConstant && (size <= 4 || IS_32BIT_SIGNED(valueChild->getLongInt())))
      {
      TR::LabelSymbol * dstStored = NULL;
      TR::RegisterDependencyConditions *deps = NULL;
      int32_t konst = valueChild->getInt();

      // Note that we can use getInt() here for all sizes, since only the
      // low order "size" bytes of the int will be used by the instruction,
      // and longs only get here if the constant fits in 32 bits.
      //
      tempMR = generateX86MemoryReference(node, cg);

      if (size == 1)
         opCode = S1MemImm1;
      else if (size == 2)
         opCode = S2MemImm2;
      else if (size == 4)
         opCode = S4MemImm4;
      else
         opCode = S8MemImm4;

      instr = generateMemImmInstruction(opCode, node, tempMR, konst, cg);
      }
   else
      {
      // See if this can be a direct memory update operation.
      // If so, mark the value child so that when it is evaluated it can generate
      // the direct memory update.
      //
      if (!usingCompressedPointers && cg->isMemoryUpdate(node))
         {
         if (valueChild->getFirstChild()->getReferenceCount() == 1)
            {
            // Memory update always a win if the result value is not needed again
            //
            valueChild->setDirectMemoryUpdate(true);
            }
         else
            {
            // Memory update is a win if there is register pressure or if we are
            // optimizing for space
            //
            if (comp->getOption(TR_OptimizeForSpace))
               valueChild->setDirectMemoryUpdate(true);
            else
               {
               int32_t numRegs = cg->getLiveRegisters(TR_GPR)->getNumberOfLiveRegisters();
               if (numRegs >= TR::RealRegister::LastAssignableGPR - 2) // -1 for VM thread reg, -1 fudge
                  valueChild->setDirectMemoryUpdate(true);
               }
            }
         }

      TR::Register *translatedReg = valueReg;
      bool valueRegNeededInFuture = true;
      // try to avoid unnecessary sign-extension
      if (valueChild->getRegister()       == 0 &&
          valueChild->getReferenceCount() == 1 &&
          (valueChild->getOpCodeValue() == TR::l2i ||
           valueChild->getOpCodeValue() == TR::l2s ||
           valueChild->getOpCodeValue() == TR::l2b))
         {
         valueChild = valueChild->getFirstChild();
         if (cg->comp()->target().is64Bit())
            translatedReg = cg->evaluate(valueChild);
         else
            translatedReg = cg->evaluate(valueChild)->getLowOrder();
         }
      else
         {
         translatedReg = cg->evaluate(valueChild);
         }

      if (usingCompressedPointers && !usingLowMemHeap)
         {
         // do the translation and handle stores of nulls
         //
         valueReg = cg->evaluate(node->getSecondChild());

         // check for stored value being null
         //
         generateRegRegInstruction(TESTRegReg(), node, translatedReg, translatedReg, cg);
         generateRegRegInstruction(CMOVERegReg(), node, valueReg, translatedReg, cg);
         }
      else
         {
         valueReg = translatedReg;
         if (valueChild->getReferenceCount() == 0)
            valueRegNeededInFuture = false;
         }

      // If the evaluation of the child resulted in a NULL register value, the
      // store has already been done by the child
      //
      if (valueReg)
         {
         if (size == 1)
            opCode = S1MemReg;
         else if (size == 2)
            opCode = S2MemReg;
         else if (size == 4)
            opCode = S4MemReg;
         else
            opCode = S8MemReg;

         if (TR::Compiler->om.generateCompressedObjectHeaders() &&
               (node->getSymbol()->isClassObject() ||
                 (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())))
            opCode = S4MemReg;

         tempMR = generateX86MemoryReference(node, cg);

         // in comp->useCompressedPointers we should write 4 bytes
         // since the iastore has been changed to an iistore, size will be 4
         //
         instr = generateMemRegInstruction(opCode, node, tempMR, valueReg, cg);

         TR::SymbolReference& symRef = tempMR->getSymbolReference();
         if (symRef.isUnresolved())
            {
            TR::TreeEvaluator::padUnresolvedDataReferences(node, symRef, cg);
            }

         // Make the register being stored rematerializable from the destination of
         // the store, if possible. Only do this if the register is not already
         // discardable, otherwise the original rematerialization information will
         // be lost.
         //
         if (cg->enableRematerialisation() && !valueReg->getRematerializationInfo())
            {
            // WARNING: if the store truncates/modifies the data associated with
            // a virtual register then there will not be enough information to
            // rematerialise that virtual register
            //
            if (originalValueChild == valueChild)
               {
               TR_RematerializableTypes type;

               // This is why we should use TR::DataType in place of TR_RematerializableTypes...
               //
               switch (node->getDataType())
                  {
                  case TR::Int8:
                     type = TR_RematerializableByte;
                     break;
                  case TR::Int16:
                     type = TR_RematerializableShort;
                     break;
                  case TR::Int32:
                     type = TR_RematerializableInt;
                     break;
                  case TR::Int64:
                     type = TR_RematerializableLong;
                     break;
                  case TR::Address:
                       {
                     if (node && node->getOpCode().hasSymbolReference() && node->getSymbol() && node->getSymbol()->isClassObject())
                        (TR::Compiler->om.generateCompressedObjectHeaders() || cg->comp()->target().is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;
                     else
                        type = TR_RematerializableAddress;

                     break;
                     }
                  default:
                     TR_ASSERT(0, "Unsupported dataType for integerStoreEvaluator");
                     break;
                  }

               setDiscardableIfPossible(type, valueReg, node, instr, tempMR, cg);
               }
            }
         }
      else
         {
         TR_ASSERT(valueChild->isDirectMemoryUpdate(), "Null valueReg should only occur in direct memory upates");
         }
      }

   cg->decReferenceCount(valueChild);

   if (tempMR)
      tempMR->decNodeReferenceCounts(cg);
   else if (node->getOpCode().isIndirect() && !valueReg)
      {
      // Direct Memory Update, we need to dec the first child ref count here, since this was not done by the child evaluator in its
      // direct memory update logic.
      cg->recursivelyDecReferenceCount(node->getFirstChild());
      }

#ifdef J9_PROJECT_SPECIFIC
   if (node->getSymbolReference()->getSymbol()->isVolatile())
      {
      TR_OpaqueMethodBlock *caller = node->getOwningMethod();
      if (tempMR && caller)
         {
         TR_ResolvedMethod *m = comp->fe()->createResolvedMethod(cg->trMemory(), caller, node->getSymbolReference()->getOwningMethod(comp));
         if ((m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicInteger_lazySet) ||
             (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicReference_lazySet) ||
             (m->getRecognizedMethod() == TR::java_util_concurrent_atomic_AtomicLong_lazySet))
            {
            tempMR->setIgnoreVolatile();
            }
         }
      }
#endif

   if (instr && node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);

   if (usingCompressedPointers)
      cg->decReferenceCount(node->getSecondChild());

   if (comp->useAnchors() && node->getOpCode().isIndirect())
      node->setStoreAlreadyEvaluated(true);

   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO:AMD64: Fold this logic into integerStoreEvaluator and share it with AMD64's lstoreEvaluator.
   TR::Node *valueChild;
   TR::Compilation *comp = cg->comp();

   if (node->getOpCode().isIndirect())
      valueChild = node->getSecondChild();
   else
      valueChild = node->getFirstChild();

   // will not be taken for comp->useCompressedPointers
   // Handle special cases
   //
   if (valueChild->getRegister() == NULL &&
       valueChild->getReferenceCount() == 1)
      {
      // Special case storing a float value into an int variable
      //
      if (valueChild->getOpCodeValue() == TR::fbits2i &&
          !valueChild->normalizeNanValues())
         {
         if (node->getOpCode().isIndirect())
            {
            node->setChild(1, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::fstorei);
            TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
            node->setChild(1, valueChild);
            TR::Node::recreate(node, TR::istorei);
            }
         else
            {
            node->setChild(0, valueChild->getFirstChild());
            TR::Node::recreate(node, TR::fstore);
            TR::TreeEvaluator::floatingPointStoreEvaluator(node, cg);
            node->setChild(0, valueChild);
            TR::Node::recreate(node, TR::istore);
            }
         cg->decReferenceCount(valueChild);
         return NULL;
         }
      }

   return TR::TreeEvaluator::integerStoreEvaluator(node, cg);
   }

// astoreEvaluator handled by istoreEvaluator

// also handles ibstore
TR::Register *OMR::X86::TreeEvaluator::bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerStoreEvaluator(node, cg);
   }

// also handles isstore
TR::Register *OMR::X86::TreeEvaluator::sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerStoreEvaluator(node, cg);
   }

// also handles icstore
TR::Register *OMR::X86::TreeEvaluator::cstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::integerStoreEvaluator(node, cg);
   }

// iistore handled by istoreEvaluator

// ilstore handled by lstoreEvaluator

// iastoreEvaluator handled by istoreEvaluator

// ibstoreEvaluator handled by bstoreEvaluator

// isstoreEvaluator handled by sstoreEvaluator

// icstoreEvaluator handled by cstoreEvaluator

TR::Register *
OMR::X86::TreeEvaluator::arraycmpEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   return node->isArrayCmpLen() ? TR::TreeEvaluator::SSE2ArraycmpLenEvaluator(node, cg) : TR::TreeEvaluator::SSE2ArraycmpEvaluator(node, cg);
   }

TR::Register *OMR::X86::TreeEvaluator::SSE2ArraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *s1AddrNode = node->getChild(0);
   TR::Node *s2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *qwordLoop = generateLabelSymbol(cg);
   TR::LabelSymbol *byteStart = generateLabelSymbol(cg);
   TR::LabelSymbol *byteLoop = generateLabelSymbol(cg);
   TR::LabelSymbol *qwordUnequal = generateLabelSymbol(cg);
   TR::LabelSymbol *byteUnequal = generateLabelSymbol(cg);
   TR::LabelSymbol *lessThanLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *greaterThanLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *equalLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   TR::Register *s1Reg = cg->gprClobberEvaluate(s1AddrNode, MOVRegReg());
   TR::Register *s2Reg = cg->gprClobberEvaluate(s2AddrNode, MOVRegReg());
   TR::Register *strLenReg = cg->gprClobberEvaluate(lengthNode, MOVRegReg());
   TR::Register *deltaReg = cg->allocateRegister(TR_GPR);
   TR::Register *equalTestReg = cg->allocateRegister(TR_GPR);
   TR::Register *s2ByteVer1Reg = cg->allocateRegister(TR_GPR);
   TR::Register *s2ByteVer2Reg = cg->allocateRegister(TR_GPR);
   TR::Register *byteCounterReg = cg->allocateRegister(TR_GPR);
   TR::Register *qwordCounterReg = cg->allocateRegister(TR_GPR);
   TR::Register *resultReg = cg->allocateRegister(TR_GPR);
   TR::Register *xmm1Reg = cg->allocateRegister(TR_FPR);
   TR::Register *xmm2Reg = cg->allocateRegister(TR_FPR);
   TR::Machine *machine = cg->machine();

   generateLabelInstruction(LABEL, node, startLabel, cg);
   generateRegRegInstruction(MOVRegReg(), node, deltaReg, s1Reg, cg);
   generateRegRegInstruction(SUBRegReg(), node, deltaReg, s2Reg, cg); // delta = s1 - s2
   generateRegRegInstruction(MOVRegReg(), node, qwordCounterReg, strLenReg, cg);
   generateRegImmInstruction(SHRRegImm1(), node, qwordCounterReg, 4, cg);
   generateLabelInstruction(JE4,node, byteStart, cg);

   cg->stopUsingRegister(s1Reg);

   generateLabelInstruction(LABEL, node, qwordLoop, cg);
   generateRegMemInstruction(MOVUPSRegMem, node, xmm2Reg, generateX86MemoryReference(s2Reg, 0, cg), cg);
   generateRegMemInstruction(MOVUPSRegMem, node, xmm1Reg, generateX86MemoryReference(s2Reg, deltaReg, 0, cg), cg);
   generateRegRegInstruction(PCMPEQBRegReg, node, xmm1Reg, xmm2Reg, cg);
   generateRegRegInstruction(PMOVMSKB4RegReg, node, equalTestReg, xmm1Reg, cg);
   generateRegImmInstruction(CMPRegImm4(), node, equalTestReg, 0xffff, cg);

   cg->stopUsingRegister(xmm1Reg);
   cg->stopUsingRegister(xmm2Reg);

   generateLabelInstruction(JNE4, node, qwordUnequal, cg);
   generateRegImmInstruction(ADDRegImm4(), node, s2Reg, 16, cg);
   generateRegImmInstruction(SUBRegImm4(), node, qwordCounterReg, 1, cg);
   generateLabelInstruction(JG4, node, qwordLoop, cg);

   cg->stopUsingRegister(qwordCounterReg);

   generateLabelInstruction(LABEL, node, byteStart, cg);
   generateRegRegInstruction(MOVRegReg(), node, byteCounterReg, strLenReg, cg);
   generateRegImmInstruction(ANDRegImm4(), node, byteCounterReg, 0xf, cg);
   generateLabelInstruction(JE4, node, equalLabel, cg);

   cg->stopUsingRegister(strLenReg);

   generateLabelInstruction(LABEL, node, byteLoop, cg);
   generateRegMemInstruction(L1RegMem, node, s2ByteVer1Reg, generateX86MemoryReference(s2Reg, 0, cg), cg);
   generateMemRegInstruction(CMP1MemReg, node, generateX86MemoryReference(s2Reg, deltaReg, 0, cg), s2ByteVer1Reg, cg);
   generateLabelInstruction(JNE4, node, byteUnequal, cg);

   cg->stopUsingRegister(s2ByteVer1Reg);

   generateRegImmInstruction(ADDRegImm4(), node, s2Reg, 1, cg);
   generateRegImmInstruction(SUBRegImm4(), node, byteCounterReg, 1, cg);
   generateLabelInstruction(JG4, node, byteLoop, cg);

   cg->stopUsingRegister(byteCounterReg);

   generateLabelInstruction(JMP4, node, equalLabel, cg);

   generateLabelInstruction(LABEL, node, qwordUnequal, cg);
   generateRegInstruction(NOT2Reg, node, equalTestReg, cg);
   generateRegRegInstruction(BSF2RegReg, node, equalTestReg, equalTestReg, cg);
   generateRegRegInstruction(ADDRegReg(), node, deltaReg, equalTestReg, cg);
   generateRegMemInstruction(L1RegMem, node, s2ByteVer2Reg, generateX86MemoryReference(s2Reg, equalTestReg, 0, cg), cg);
   generateMemRegInstruction(CMP1MemReg, node, generateX86MemoryReference(s2Reg, deltaReg, 0, cg), s2ByteVer2Reg, cg);

   cg->stopUsingRegister(equalTestReg);
   cg->stopUsingRegister(s2ByteVer2Reg);
   cg->stopUsingRegister(s2Reg);
   cg->stopUsingRegister(deltaReg);

   generateLabelInstruction(LABEL, node, byteUnequal, cg);
   generateLabelInstruction(JB4, node, lessThanLabel, cg);

   generateLabelInstruction(LABEL, node, greaterThanLabel, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultReg, 2, cg);
   generateLabelInstruction(JMP4, node, doneLabel, cg);

   generateLabelInstruction(LABEL, node, lessThanLabel, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultReg, 1, cg);
   generateLabelInstruction(JMP4, node, doneLabel, cg);

   generateLabelInstruction(LABEL, node, equalLabel, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultReg, 0, cg);

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 8, cg);
   deps->addPostCondition(xmm1Reg, TR::RealRegister::xmm1, cg);
   deps->addPostCondition(xmm2Reg, TR::RealRegister::xmm2, cg);
   deps->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(s2Reg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(deltaReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(equalTestReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(s2ByteVer2Reg, TR::RealRegister::ByteReg, cg);
   deps->addPostCondition(s2ByteVer1Reg, TR::RealRegister::ByteReg, cg);

   generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

   node->setRegister(resultReg);

   cg->decReferenceCount(s1AddrNode);
   cg->decReferenceCount(s2AddrNode);
   cg->decReferenceCount(lengthNode);

   return resultReg;
   }

TR::Register *OMR::X86::TreeEvaluator::SSE2ArraycmpLenEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *s1AddrNode = node->getChild(0);
   TR::Node *s2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *qwordLoop = generateLabelSymbol(cg);
   TR::LabelSymbol *byteStart = generateLabelSymbol(cg);
   TR::LabelSymbol *byteLoop = generateLabelSymbol(cg);
   TR::LabelSymbol *qwordUnequal = generateLabelSymbol(cg);
   TR::LabelSymbol *byteUnequal = generateLabelSymbol(cg);
   TR::LabelSymbol *lessThanLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *greaterThanLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *equalLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   TR::Register *s1Reg = cg->gprClobberEvaluate(s1AddrNode, MOVRegReg());
   TR::Register *s2Reg = cg->gprClobberEvaluate(s2AddrNode, MOVRegReg());
   TR::Register *strLenReg = cg->gprClobberEvaluate(lengthNode, MOVRegReg());
   TR::Register *equalTestReg = cg->allocateRegister(TR_GPR);
   TR::Register *s2ByteReg = cg->allocateRegister(TR_GPR);
   TR::Register *byteCounterReg = cg->allocateRegister(TR_GPR);
   TR::Register *qwordCounterReg = cg->allocateRegister(TR_GPR);
   TR::Register *resultReg = cg->allocateRegister(TR_GPR);
   TR::Register *xmm1Reg = cg->allocateRegister(TR_FPR);
   TR::Register *xmm2Reg = cg->allocateRegister(TR_FPR);

   TR::Machine *machine = cg->machine();

   generateRegImmInstruction(MOVRegImm4(), node, resultReg, 0, cg);
   generateLabelInstruction(LABEL, node, startLabel, cg);
   generateRegRegInstruction(MOVRegReg(), node, qwordCounterReg, strLenReg, cg);
   generateRegImmInstruction(SHRRegImm1(), node, qwordCounterReg, 4, cg);
   generateLabelInstruction(JE4,node, byteStart, cg);

   generateLabelInstruction(LABEL, node, qwordLoop, cg);
   generateRegMemInstruction(MOVUPSRegMem, node, xmm1Reg, generateX86MemoryReference(s1Reg, resultReg, 0, cg), cg);
   generateRegMemInstruction(MOVUPSRegMem, node, xmm2Reg, generateX86MemoryReference(s2Reg, resultReg, 0, cg), cg);
   generateRegRegInstruction(PCMPEQBRegReg, node, xmm1Reg, xmm2Reg, cg);
   generateRegRegInstruction(PMOVMSKB4RegReg, node, equalTestReg, xmm1Reg, cg);
   generateRegImmInstruction(CMPRegImm4(), node, equalTestReg, 0xffff, cg);

   cg->stopUsingRegister(xmm1Reg);
   cg->stopUsingRegister(xmm2Reg);

   generateLabelInstruction(JNE4, node, qwordUnequal, cg);
   generateRegImmInstruction(ADDRegImm4(), node, resultReg, 16, cg);
   generateRegImmInstruction(SUBRegImm4(), node, qwordCounterReg, 1, cg);
   generateLabelInstruction(JG4, node, qwordLoop, cg);

   generateLabelInstruction(JMP4, node, byteStart, cg);

   generateLabelInstruction(LABEL, node, qwordUnequal, cg);
   generateRegInstruction(NOT2Reg, node, equalTestReg, cg);
   generateRegRegInstruction(BSF2RegReg, node, equalTestReg, equalTestReg, cg);
   generateRegRegInstruction(ADDRegReg(), node, resultReg, equalTestReg, cg);
   generateLabelInstruction(JMP4, node, doneLabel, cg);

   cg->stopUsingRegister(qwordCounterReg);
   cg->stopUsingRegister(equalTestReg);

   generateLabelInstruction(LABEL, node, byteStart, cg);
   generateRegRegInstruction(MOVRegReg(), node, byteCounterReg, strLenReg, cg);
   generateRegImmInstruction(ANDRegImm4(), node, byteCounterReg, 0xf, cg);
   generateLabelInstruction(JE4, node, doneLabel, cg);
   cg->stopUsingRegister(strLenReg);

   generateLabelInstruction(LABEL, node, byteLoop, cg);
   generateRegMemInstruction(L1RegMem, node, s2ByteReg, generateX86MemoryReference(s2Reg, resultReg, 0, cg), cg);
   generateMemRegInstruction(CMP1MemReg, node, generateX86MemoryReference(s1Reg, resultReg, 0, cg), s2ByteReg, cg);
   generateLabelInstruction(JNE4, node, doneLabel, cg);

   cg->stopUsingRegister(s2ByteReg);

   generateRegImmInstruction(ADDRegImm4(), node, resultReg, 1, cg);
   generateRegImmInstruction(SUBRegImm4(), node, byteCounterReg, 1, cg);
   generateLabelInstruction(JG4, node, byteLoop, cg);

   cg->stopUsingRegister(byteCounterReg);
   cg->stopUsingRegister(s1Reg);
   cg->stopUsingRegister(s2Reg);

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 8, cg);
   deps->addPostCondition(xmm1Reg, TR::RealRegister::xmm1, cg);
   deps->addPostCondition(xmm2Reg, TR::RealRegister::xmm2, cg);

   // The register pressure is 6  for above code.
   deps->addPostCondition(byteCounterReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(s2ByteReg, TR::RealRegister::ByteReg, cg);
   deps->addPostCondition(resultReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(equalTestReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(s2Reg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(s1Reg, TR::RealRegister::NoReg, cg);

   generateLabelInstruction(LABEL, node, doneLabel, deps, cg);
   node->setRegister(resultReg);

   cg->decReferenceCount(s1AddrNode);
   cg->decReferenceCount(s2AddrNode);
   cg->decReferenceCount(lengthNode);

   return resultReg;
   }


bool OMR::X86::TreeEvaluator::stopUsingCopyRegAddr(TR::Node* node, TR::Register*& reg, TR::CodeGenerator* cg)
   {
   if (node != NULL)
      {
      reg = cg->evaluate(node);
      TR::Register *copyReg = NULL;
      if (node->getReferenceCount() > 1)
         {
         if (reg->containsInternalPointer())
            {
            copyReg = cg->allocateRegister();
            copyReg->setPinningArrayPointer(reg->getPinningArrayPointer());
            copyReg->setContainsInternalPointer();
            }
         else
            copyReg = cg->allocateCollectedReferenceRegister();

         generateRegRegInstruction(MOVRegReg(), node, copyReg, reg, cg);
         reg = copyReg;
         return true;
         }
      }
   return false;
   }

bool OMR::X86::TreeEvaluator::stopUsingCopyRegInteger(TR::Node* node, TR::Register*& reg, TR::CodeGenerator* cg)
   {
   if (node != NULL)
      {
      reg = cg->evaluate(node);
      TR::Register *copyReg = NULL;
      if (node->getReferenceCount() > 1)
         {
         copyReg = cg->allocateRegister();
         generateRegRegInstruction(MOVRegReg(), node, copyReg, reg, cg);
         reg = copyReg;
         return true;
         }
      }
   return false;
   }


TR::Block *OMR::X86::TreeEvaluator::getOverflowCatchBlock(TR::Node *node, TR::CodeGenerator *cg)
   {
   //make sure the overflowCHK has a catch block first
   TR::Block *overflowCatchBlock = NULL;
   TR::CFGEdgeList &excepSucc = cg->getCurrentEvaluationTreeTop()->getEnclosingBlock()->getExceptionSuccessors();
   for (auto e = excepSucc.begin(); e != excepSucc.end(); ++e)
      {
      TR::Block *dest = toBlock((*e)->getTo());
      if (dest->getCatchBlockExtension()->_catchType == TR::Block::CanCatchOverflowCheck)
            overflowCatchBlock = dest;
      }
   TR_ASSERT(overflowCatchBlock != NULL, "OverflowChk node %p doesn't have overflow catch block\n", node);

   //the BBStartEvaluator will generate the label but in this case the catch block might not been evaluated yet
   TR::Node * bbstartNode = overflowCatchBlock->getEntry()->getNode();
   TR_ASSERT((bbstartNode->getOpCodeValue() == TR::BBStart), "catch block entry %p must be TR::BBStart\n", bbstartNode);
   if (!bbstartNode->getLabel())
      {
      TR::LabelSymbol *label = generateLabelSymbol(cg);
      bbstartNode->setLabel(label);
      }
   return overflowCatchBlock;
   }

void OMR::X86::TreeEvaluator::genArithmeticInstructionsForOverflowCHK(TR::Node *node, TR::CodeGenerator *cg)
   {
   /*
    *overflowCHK
    *   =>child1(Operation)
    *   =>child2(Operand1)
    *   =>child3(Operand2)
    */

   TR_X86OpCodes op;
   TR::Node *operationNode = node->getFirstChild();
   TR::Node *operand1 = node->getSecondChild();
   TR::Node *operand2 = node->getThirdChild();
   //it is fine that nodeIs64Bit is false for long operand on 32bits platform because
   //the analyzers below don't use *op* in this case anyways
   bool nodeIs64Bit = cg->comp()->target().is32Bit()? false: TR::TreeEvaluator::getNodeIs64Bit(operand1, cg);
   switch (node->getOverflowCheckOperation())
      {
      //add group
      case TR::ladd:
      case TR::iadd:
         op = ADDRegReg(nodeIs64Bit);
         break;
      case TR::sadd:
         op = ADD2RegReg;
         break;
      case TR::badd:
         op = ADD1RegReg;
         break;
      //sub group
      case TR::lsub:
      case TR::isub:
         op = SUBRegReg(nodeIs64Bit);
         break;
      case TR::ssub:
         op = SUB2RegReg;
         break;
      case TR::bsub:
         op = SUB1RegReg;
         break;
      //mul group
      case TR::imul:
         op = IMULRegReg(nodeIs64Bit);
         break;
      case TR::lmul:
         //TODO: leaving lmul overflowCHK on 32bit platform for later since there is no pending demand for this.
         // the 32 bits lmul needs several instructions including to multiplications between the lower and higher parts
         // of the registers and additions of the intermediate results. See TR_X86BinaryCommutativeAnalyser::longMultiplyAnalyser
         // Therefore the usual way of only detecting the OF for the last instruction of sequence won't work for this case.
         // The implementation needs to detect OF flags after all the instructions involving higher parts of the registers for
         // both operands and intermediate results.
         TR_ASSERT(cg->comp()->target().is64Bit(), "overflowCHK for lmul on 32 bits is not currently supported\n");
         op = IMULRegReg(nodeIs64Bit);
         break;
      default:
         TR_ASSERT(0 , "unsupported OverflowCHK opcode %s on node %p\n", cg->comp()->getDebug()->getName(node->getOpCode()), node);
      }

   bool operationChildEvaluatedAlready = operationNode->getRegister()? true : false;
   if (!operationChildEvaluatedAlready)
      {
      operationNode->setNodeRequiresConditionCodes(true);
      cg->evaluate(operationNode);
      cg->decReferenceCount(operand1);
      cg->decReferenceCount(operand2);
      }
   else
   // we need to do the operation again when the Operation node has been evaluated already under a different treetop
   // TODO: there is still a chance that the flags might still be available and we could detect it and avoid repeating
   // the operation
      {
      TR_X86BinaryCommutativeAnalyser  addMulAnalyser(cg);
      TR_X86SubtractAnalyser subAnalyser(cg);
      node->setNodeRequiresConditionCodes(true);
      bool needsEflags = true;
      switch (node->getOverflowCheckOperation())
         {
         // add group
         case TR::badd:
         case TR::sadd:
         case TR::iadd:
            addMulAnalyser.integerAddAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, needsEflags);
            break;
         case TR::ladd:
            cg->comp()->target().is32Bit() ? addMulAnalyser.longAddAnalyserWithExplicitOperands(node, operand1, operand2)
                                           : addMulAnalyser.integerAddAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, needsEflags);
            break;
         // sub group
         case TR::bsub:
            subAnalyser.integerSubtractAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV1RegReg, needsEflags);
            break;
         case TR::ssub:
         case TR::isub:
            subAnalyser.integerSubtractAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV4RegReg, needsEflags);
            break;
         case TR::lsub:
            cg->comp()->target().is32Bit() ? subAnalyser.longSubtractAnalyserWithExplicitOperands(node, operand1, operand2)
                                           : subAnalyser.integerSubtractAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV8RegReg, needsEflags);
            break;
         // mul group
         case TR::imul:
            addMulAnalyser.genericAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV4RegReg);
            break;
         case TR::lmul:
            addMulAnalyser.genericAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV8RegReg);
            break;
         default:
            break;
         }
      }
   }

TR::Register *OMR::X86::TreeEvaluator::overflowCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_X86OpCodes opcode;
   if (node->getOpCodeValue() == TR::OverflowCHK)
       opcode = JO4;
   else if (node->getOpCodeValue() == TR::UnsignedOverflowCHK)
       opcode = JB4;
   else
       TR_ASSERT(0, "unrecognized overflow operation in overflowCHKEvaluator");
   TR::Block *overflowCatchBlock = TR::TreeEvaluator::getOverflowCatchBlock(node, cg);
   TR::TreeEvaluator::genArithmeticInstructionsForOverflowCHK(node, cg);
   generateLabelInstruction(opcode, node, overflowCatchBlock->getEntry()->getNode()->getLabel(), cg);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

static void generateRepMovsInstruction(TR_X86OpCodes repmovs, TR::Node *node, TR::Register* sizeRegister, TR::RegisterDependencyConditions* dependencies, TR::CodeGenerator *cg)
   {
   switch (repmovs)
      {
      case REPMOVSQ:
         generateRegImmInstruction(SHRRegImm1(), node, sizeRegister, 3, cg);
         break;
      case REPMOVSD:
         generateRegImmInstruction(SHRRegImm1(), node, sizeRegister, 2, cg);
         break;
      case REPMOVSW:
         generateRegInstruction(SHRReg1(), node, sizeRegister, cg);
         break;
      case REPMOVSB:
         break;
      default:
         TR_ASSERT(false, "Invalid REP MOVS opcode [%d]", repmovs);
         break;
      }
   generateInstruction(repmovs, node, dependencies, cg);
   }

/** \brief
*    Generate instructions to do array copy for 64-bit primitives on IA-32.
*
*  \param node
*     The tree node
*
*  \param dstReg
*     The destination array address register
*
*  \param srcReg
*     The source array address register
*
*  \param sizeReg
*     The register holding the total size of elements to be copied, in bytes
*
*  \param cg
*     The code generator
*/
static void arrayCopy64BitPrimitiveOnIA32(TR::Node* node, TR::Register* dstReg, TR::Register* srcReg, TR::Register* sizeReg, TR::CodeGenerator* cg)
   {
   TR::Register* scratch = cg->allocateRegister();
   TR::Register* XMM = cg->allocateRegister(TR_FPR);

   TR::RegisterDependencyConditions* dependencies = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)5, cg);
   dependencies->addPostCondition(scratch, TR::RealRegister::ByteReg, cg);
   dependencies->addPostCondition(XMM, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(srcReg, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(dstReg, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(sizeReg, TR::RealRegister::ecx, cg);

   TR::LabelSymbol* startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* endLabel = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   TR::LabelSymbol* loopLabel = generateLabelSymbol(cg);

   generateLabelInstruction(LABEL, node, startLabel, cg);

   // Example:
   // ; obtain distance from source to destination
   //   SUB   EDI,ESI
   //
   // ; decide direction
   //   CMP   EDI,ECX
   //   LEA   EDI,[EDI+ESI]
   //   LEA   EAX,[ESI+ECX-8]
   //   CMOVB ESI,EAX
   //   SETAE AL
   //   MOVZX EAX,AL
   //   LEA   EAX,[2*EAX-1]
   //
   // ; copy
   //   SHR  ECX,3
   //   JRCXZ endLabel
   // loopLabel:
   //   MOVQ XMM0,[ESI]
   //   MOVQ [EDI+ESI],XMM0
   //   LEA  ESI,[ESI+8*EAX]
   //   LOOP loopLabel
   // endLabel:
   //   # LOOP END
   generateRegRegInstruction(SUBRegReg(), node, dstReg, srcReg, cg);
   if (node->isForwardArrayCopy())
      {
      generateRegImmInstruction(MOVRegImm4(), node, scratch, 1, cg);
      }
   else // decide direction during runtime
      {
      generateRegRegInstruction(CMPRegReg(), node, dstReg, sizeReg, cg);
      generateRegMemInstruction(LEARegMem(), node, scratch, generateX86MemoryReference(srcReg, sizeReg, 0, -8, cg), cg);
      generateRegRegInstruction(CMOVBRegReg(), node, srcReg, scratch, cg);
      generateRegInstruction(SETAE1Reg, node, scratch, cg);
      generateRegRegInstruction(MOVZXReg4Reg1, node, scratch, scratch, cg);
      generateRegMemInstruction(LEARegMem(), node, scratch, generateX86MemoryReference(NULL, scratch, 1, -1, cg), cg);
      }

   generateRegImmInstruction(SHRRegImm1(), node, sizeReg, 3, cg);
   generateLabelInstruction(JRCXZ1, node, endLabel, cg);
   generateLabelInstruction(LABEL, node, loopLabel, cg);
   generateRegMemInstruction(MOVQRegMem, node, XMM, generateX86MemoryReference(srcReg, 0, cg), cg);
   generateMemRegInstruction(MOVQMemReg, node, generateX86MemoryReference(dstReg, srcReg, 0, cg), XMM, cg);
   generateRegMemInstruction(LEARegMem(), node, srcReg, generateX86MemoryReference(srcReg, scratch, 3, cg), cg);
   generateLabelInstruction(LOOP1, node, loopLabel, cg);

   generateLabelInstruction(LABEL, node, endLabel, dependencies, cg);
   cg->stopUsingRegister(XMM);
   cg->stopUsingRegister(scratch);
   }

/** \brief
*    Generate instructions to do array copy for 16-bit primitives.
*
*  \param node
*     The tree node
*
*  \param dstReg
*     The destination array address register
*
*  \param srcReg
*     The source array address register
*
*  \param sizeReg
*     The register holding the total size of elements to be copied, in bytes
*
*  \param cg
*     The code generator
*/
static void arrayCopy16BitPrimitive(TR::Node* node, TR::Register* dstReg, TR::Register* srcReg, TR::Register* sizeReg, TR::CodeGenerator* cg)
   {
   TR::RegisterDependencyConditions* dependencies = generateRegisterDependencyConditions((uint8_t)3, (uint8_t)3, cg);
   dependencies->addPreCondition(srcReg, TR::RealRegister::esi, cg);
   dependencies->addPreCondition(dstReg, TR::RealRegister::edi, cg);
   dependencies->addPreCondition(sizeReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(srcReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(dstReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(sizeReg, TR::RealRegister::ecx, cg);

   TR::LabelSymbol* mainBegLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* mainEndLabel = generateLabelSymbol(cg);
   mainBegLabel->setStartInternalControlFlow();
   mainEndLabel->setEndInternalControlFlow();

   generateLabelInstruction(LABEL, node, mainBegLabel, cg);
   if (node->isForwardArrayCopy())
      {
      generateRegImmInstruction(SHRRegImm1(), node, sizeReg, 2, cg);
      generateInstruction(REPMOVSD, node, cg);
      generateLabelInstruction(JAE1, node, mainEndLabel, cg);
      generateRegMemInstruction(L2RegMem, node, sizeReg, generateX86MemoryReference(srcReg, 0, cg), cg);
      generateMemRegInstruction(S2MemReg, node, generateX86MemoryReference(dstReg, 0, cg), sizeReg, cg);
      }
   else // decide direction during runtime
      {
      TR::LabelSymbol* backwardLabel = generateLabelSymbol(cg);

      generateRegRegInstruction(SUBRegReg(), node, dstReg, srcReg, cg);  // dst = dst - src
      generateRegRegInstruction(CMPRegReg(), node, dstReg, sizeReg, cg); // cmp dst, size
      generateRegMemInstruction(LEARegMem(), node, dstReg, generateX86MemoryReference(dstReg, srcReg, 0, cg), cg); // dst = dst + src
      generateLabelInstruction(JB4, node, backwardLabel, cg);   // jb, skip backward copy setup

      generateRegImmInstruction(SHRRegImm1(), node, sizeReg, 2, cg);
      generateInstruction(REPMOVSD, node, cg);
      generateLabelInstruction(JAE1, node, mainEndLabel, cg);
      generateRegMemInstruction(L2RegMem, node, sizeReg, generateX86MemoryReference(srcReg, 0, cg), cg);
      generateMemRegInstruction(S2MemReg, node, generateX86MemoryReference(dstReg, 0, cg), sizeReg, cg);

      {
      TR_OutlinedInstructionsGenerator og(backwardLabel, node, cg);
      generateRegMemInstruction(LEARegMem(), node, srcReg, generateX86MemoryReference(srcReg, sizeReg, 0, -2, cg), cg);
      generateRegMemInstruction(LEARegMem(), node, dstReg, generateX86MemoryReference(dstReg, sizeReg, 0, -2, cg), cg);
      generateInstruction(STD, node, cg);
      generateRepMovsInstruction(REPMOVSW, node, sizeReg, NULL, cg);
      generateInstruction(CLD, node, cg);
      generateLabelInstruction(JMP4, node, mainEndLabel, cg);
      og.endOutlinedInstructionSequence();
      }
      }
   generateLabelInstruction(LABEL, node, mainEndLabel, dependencies, cg);
   }

/** \brief
*    Generate instructions to do array copy.
*
*  \param node
*     The tree node
*
*  \param elementSize
*     The size of an element, in bytes
*
*  \param dstReg
*     The destination array address register
*
*  \param srcReg
*     The source array address register
*
*  \param sizeReg
*     The register holding the total size of elements to be copied, in bytes
*
*  \param cg
*     The code generator
*/
static void arrayCopyDefault(TR::Node* node, uint8_t elementSize, TR::Register* dstReg, TR::Register* srcReg, TR::Register* sizeReg, TR::CodeGenerator* cg)
   {
   TR::RegisterDependencyConditions* dependencies = generateRegisterDependencyConditions((uint8_t)3, (uint8_t)3, cg);
   dependencies->addPreCondition(srcReg, TR::RealRegister::esi, cg);
   dependencies->addPreCondition(dstReg, TR::RealRegister::edi, cg);
   dependencies->addPreCondition(sizeReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(srcReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(dstReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(sizeReg, TR::RealRegister::ecx, cg);

   TR_X86OpCodes repmovs;
   switch (elementSize)
      {
      case 8:
         repmovs = REPMOVSQ;
         break;
      case 4:
         repmovs = REPMOVSD;
         break;
      case 2:
         repmovs = REPMOVSW;
         break;
      default:
         repmovs = REPMOVSB;
         break;
      }
   if (node->isForwardArrayCopy())
      {
      generateRepMovsInstruction(repmovs, node, sizeReg, dependencies, cg);
      }
   else // decide direction during runtime
      {
      TR::LabelSymbol* mainBegLabel = generateLabelSymbol(cg);
      TR::LabelSymbol* mainEndLabel = generateLabelSymbol(cg);
      TR::LabelSymbol* backwardLabel = generateLabelSymbol(cg);
      mainBegLabel->setStartInternalControlFlow();
      mainEndLabel->setEndInternalControlFlow();

      generateLabelInstruction(LABEL, node, mainBegLabel, cg);

      generateRegRegInstruction(SUBRegReg(), node, dstReg, srcReg, cg);  // dst = dst - src
      generateRegRegInstruction(CMPRegReg(), node, dstReg, sizeReg, cg); // cmp dst, size
      generateRegMemInstruction(LEARegMem(), node, dstReg, generateX86MemoryReference(dstReg, srcReg, 0, cg), cg); // dst = dst + src
      generateLabelInstruction(JB4, node, backwardLabel, cg);   // jb, skip backward copy setup
      generateRepMovsInstruction(repmovs, node, sizeReg, NULL, cg);

      {
      TR_OutlinedInstructionsGenerator og(backwardLabel, node, cg);
      generateRegMemInstruction(LEARegMem(), node, srcReg, generateX86MemoryReference(srcReg, sizeReg, 0, -(intptr_t)elementSize, cg), cg);
      generateRegMemInstruction(LEARegMem(), node, dstReg, generateX86MemoryReference(dstReg, sizeReg, 0, -(intptr_t)elementSize, cg), cg);
      generateInstruction(STD, node, cg);
      generateRepMovsInstruction(repmovs, node, sizeReg, NULL, cg);
      generateInstruction(CLD, node, cg);
      generateLabelInstruction(JMP4, node, mainEndLabel, cg);
      og.endOutlinedInstructionSequence();
      }

      generateLabelInstruction(LABEL, node, mainEndLabel, dependencies, cg);
      }
   }

/** \brief
 *    Generate a store instruction
 *
 *  \param node
 *     The tree node
 *
 *  \param addressReg
 *     The base register for the destination mem address
 *
 *  \param index
 *     The index for the destination mem address
 *
 *  \param valueReg
 *     The value that needs be stored
 *
 *  \param size
 *     The size of valueReg
 *
 *  \param cg
 *     The code generator
 *
 */
static void generateArrayElementStore(TR::Node* node, TR::Register* addressReg, int32_t index, TR::Register* valueReg, uint8_t size,  TR::CodeGenerator* cg)
   {
   TR_X86OpCodes storeOpcode;
   if (valueReg->getKind() == TR_FPR)
      {
      switch (size)
         {
         case 4:
            storeOpcode = MOVDMemReg;
            break;
         case 8:
            storeOpcode = MOVQMemReg;
            break;
         case 16:
            storeOpcode = MOVDQUMemReg;
            break;
         default:
            TR_ASSERT(0, "Unsupported size in generateArrayElementStore, size: %d", size);
            break;
         }
      }
   else if (valueReg->getKind() == TR_GPR)
      {
      switch (size)
         {
         case 1:
            storeOpcode = S1MemReg;
            break;
         case 2:
            storeOpcode = S2MemReg;
            break;
         case 4:
            storeOpcode = S4MemReg;
            break;
         case 8:
            storeOpcode = S8MemReg;
            break;
         default:
            TR_ASSERT(0, "Unsupported size in generateArrayElementStore, size: %d", size);
            break;

         }
      }
   else
      {
      TR_ASSERT(0, "Unsupported register type in generateArrayElementStore");
      }
   generateMemRegInstruction(storeOpcode, node, generateX86MemoryReference(addressReg, index, cg), valueReg, cg);
   }

/** \brief
 *    Generate a load instruction
 *
 *  \param node
 *     The tree node
 *
 *  \param valueReg
 *     The destination register
 *
 *  \param size
 *     The size of the loaded value
 *
 *  \param addressReg
 *     The base register for the source mem address
 *
 *  \param index
 *     The index for the destination mem address
 *
 *  \param cg
 *     The code generator
 */
static void generateArrayElementLoad(TR::Node* node, TR::Register* valueReg, uint8_t size, TR::Register* addressReg, int32_t index, TR::CodeGenerator* cg)
   {
   TR_X86OpCodes loadOpCode;
   if (valueReg->getKind() == TR_FPR)
      {
      switch (size)
         {
         case 4:
            loadOpCode  = MOVDRegMem;
            break;
         case 8:
            loadOpCode  = MOVQRegMem;
            break;
         case 16:
            loadOpCode  = MOVDQURegMem;
            break;
         default:
            TR_ASSERT(0, "Unsupported size in generateArrayElementLoad, size: %d", size);
            break;
         }
      }
   else if (valueReg->getKind() == TR_GPR)
      {
      switch (size)
         {
         case 1:
            loadOpCode  = L1RegMem;
            break;
         case 2:
            loadOpCode  = L2RegMem;
            break;
         case 4:
            loadOpCode  = L4RegMem;
            break;
         case 8:
            loadOpCode  = L8RegMem;
            break;
         default:
            TR_ASSERT(0, "Unsupported size in generateArrayElementLoad, size: %d", size);
            break;

         }
      }
   else
      {
      TR_ASSERT(0, "Unsupported register type in generateArrayElementLoad");
      }
   generateRegMemInstruction(loadOpCode,  node, valueReg, generateX86MemoryReference(addressReg, index, cg), cg);
   }

/** \brief
 *    Generate instructions to do arraycopy for a short constant array. We try to copy as many elements as we can every time.
 *
 *  \param node
 *     The tree node
 *
 *  \param dstReg
 *     The destination array address register
 *
 *  \param srcReg
 *     The source array address register
 *
 *  \param size
 *     The number of elements that will be copied
 *
 *  \param cg
 *     The code generator
 */

static void arraycopyForShortConstArrayWithDirection(TR::Node* node, TR::Register* dstReg, TR::Register* srcReg, uint32_t size, TR::CodeGenerator *cg)
   {
   uint32_t totalSize = size;
   static uint32_t regSize[] = {16, 8, 4, 2, 1};

   TR::Register* xmmReg = NULL;
   TR::Register* gprReg = NULL;

   int32_t i = 0;
   while (size != 0)
      {
      if (size >= regSize[i])
         {
         if (xmmReg == NULL && regSize[i] == 16)
            {
            xmmReg = cg->allocateRegister(TR_FPR);
            }
         else if (gprReg == NULL && regSize[i] < 16)
            {
            gprReg = cg->allocateRegister(TR_GPR);
            }
         TR::Register* tempReg = (regSize[i] == 16)? xmmReg : gprReg;
         generateArrayElementLoad(node, tempReg, regSize[i], srcReg, totalSize - size, cg);
         generateArrayElementStore(node, dstReg, totalSize - size, tempReg, regSize[i], cg);
         size -= regSize[i];
         }
      else
         {
         i++;
         }
      }
   if (xmmReg) cg->stopUsingRegister(xmmReg);
   if (gprReg) cg->stopUsingRegister(gprReg);
   }

/** \brief
 *    Generate instructions to do arraycopy for a short constant array. We need to copy the source array to registers
 *    then store the value back to destination array.
 *
 *  \param node
 *     The tree node
 *
 *  \param dstReg
 *     The destination array address register
 *
 *  \param srcReg
 *     The source array address register
 *
 *  \param size
 *     The number of elements that will be copied
 *
 *  \param cg
 *     The code generator
 */
static void arraycopyForShortConstArrayWithoutDirection(TR::Node* node, TR::Register* dstReg, TR::Register* srcReg, uint32_t size, TR::CodeGenerator *cg)
   {
   uint32_t totalSize = size;
   uint32_t tempTotalSize = totalSize;

   uint32_t moves[5];
   static uint32_t regSize[] = {16, 8, 4, 2, 1};

   for (uint32_t i=0; i<5; i++)
      {
      moves[i] = tempTotalSize/regSize[i];
      tempTotalSize = tempTotalSize%regSize[i];
      }

   TR::Register* xmmUsed[8] = {NULL};
   // First load as many bytes as possible into XMM
   for (uint32_t i=0; i<moves[0]; i++)
      {
      TR::Register* xmmReg = cg->allocateRegister(TR_FPR);
      generateArrayElementLoad(node, xmmReg, 16, srcReg, i*16, cg);
      xmmUsed[i] = xmmReg;
      }

   // Note: The worst case, moves = [X, 1, 1, 1, 1]
   //
   // If we can do it just using one gpr, do it
   // If we can do it with one gpr and one xmm, do it
   // If total size is >= 16, use shift window
   // Otherwise, use two gpr

   int32_t residue = totalSize%16;
   TR::Register* reg1 = NULL;
   TR::Register* reg2 = NULL;
   int32_t residueCase = 0;

   int32_t firstLoadSizeForCase4, secondLoadSizeForCase4;

   if (residue == 1 || residue == 2 || residue == 4 || residue == 8) // 1 gpr
      {
      generateArrayElementLoad(node, srcReg, residue, srcReg, totalSize - residue, cg);
      reg1 = srcReg;
      residueCase = 1;
      }
   else if (totalSize > 16 && residue > 0) // shift 1 xmm
      {
      reg1 = cg->allocateRegister(TR_FPR);
      generateArrayElementLoad(node, reg1, 16, srcReg, totalSize - 16, cg);
      residueCase = 2;
      }
   else if (residue == 3) // 2 gpr
      {
      reg1 = cg->allocateRegister(TR_GPR);
      reg2 = srcReg;
      generateArrayElementLoad(node, reg1, 1, srcReg, 0, cg);
      generateArrayElementLoad(node, reg2, 2, srcReg, 1, cg);
      residueCase = 3;
      }
   else if (residue != 0) // 1 gpr + 1 xmm
      {
      reg1 = cg->allocateRegister(TR_FPR);
      reg2 = srcReg;
      firstLoadSizeForCase4  = residue>8? 8:4;
      secondLoadSizeForCase4 = (residue - firstLoadSizeForCase4);
      if (secondLoadSizeForCase4 > 4)
         secondLoadSizeForCase4 = 8;
      else if (secondLoadSizeForCase4 == 3)
         secondLoadSizeForCase4 = 4;

      generateArrayElementLoad(node, reg1, firstLoadSizeForCase4,  srcReg, 0, cg);
      generateArrayElementLoad(node, reg2, secondLoadSizeForCase4, srcReg, residue-secondLoadSizeForCase4, cg);
      residueCase = 4;
      }

   for (int32_t i=0; i<moves[0]; i++)
      {
      generateArrayElementStore(node, dstReg, i*16, xmmUsed[i], 16, cg);
      cg->stopUsingRegister(xmmUsed[i]);
      }

   switch (residueCase)
      {
      case 0:
         break;
      case 1:
         generateArrayElementStore(node, dstReg, totalSize - residue, reg1, residue, cg);
         break;
      case 2:
         generateArrayElementStore(node, dstReg, totalSize - 16, reg1, 16, cg);
         cg->stopUsingRegister(reg1);
         break;
      case 3:
         generateArrayElementStore(node, dstReg, 0, reg1, 1, cg);
         generateArrayElementStore(node, dstReg, 1, reg2, 2, cg);
         cg->stopUsingRegister(reg1);
         break;
      case 4:
         generateArrayElementStore(node, dstReg, 0, reg1, firstLoadSizeForCase4, cg);
         generateArrayElementStore(node, dstReg, totalSize - secondLoadSizeForCase4, reg2, secondLoadSizeForCase4, cg);
         cg->stopUsingRegister(reg1);
         break;
      }
   }

TR::Register *OMR::X86::TreeEvaluator::arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // There are two variants of TR::arraycopy: one has 5 children, the other has 3 children. Details can be found from
   // compiler/il/ILOpCodeProperties.hpp
   //
   // In most, if not all, cases, the 5-child variant requires language specific semantics, which OMR is not aware of. The fact
   // that a 5-child arraycopy is generated indicates at least one of the first two children must be needed when performing the
   // copy; otherwise a 3-child arraycopy should be generated instead. Interpreting the meanings of the first two children
   // definitely requires language specific semantics. For example, the first two children may be for dealing with an arraycopy
   // where the Garbage Collector may need to be notified about the copy or something to that affect.
   //
   // Therefore, this OMR evaluator only handles the 3-child variant, is an operation equivalent to C++'s std::memmove().
   // Should a downstream project need the 5-child variant evaluator, it needs to override this evaluator in its own TreeEvaluator,
   // and delegates the 3-child variant back to this evaluator by calling OMR::TreeEvaluatorConnector::arraycopyEvaluator.

   TR_ASSERT_FATAL(node->getNumChildren() == 3, "This evaluator is for the 3-child variant of arraycopy.");

   // ALL nodes need be evaluated to comply argument evaluation order;
   // since size node is the last node, its evaluation can be delayed for further optimization
   TR::Node* srcNode  = node->getChild(0); // the address of memory to copy from
   TR::Node* dstNode  = node->getChild(1); // the address of memory to copy to
   TR::Node* sizeNode = node->getChild(2); // the size of memory to copy, in bytes

   TR::Register* srcReg = cg->gprClobberEvaluate(srcNode, MOVRegReg());
   TR::Register* dstReg = cg->gprClobberEvaluate(dstNode, MOVRegReg());

   TR::DataType dt = node->getArrayCopyElementType();
   uint32_t elementSize = 1;
   static bool useREPMOVSW = feGetEnv("TR_UseREPMOVSWForArrayCopy");
   static bool forceByteArrayElementCopy = feGetEnv("TR_ForceByteArrayElementCopy");
   if (!forceByteArrayElementCopy)
      {
      if (node->isReferenceArrayCopy() || dt == TR::Address)
         elementSize = TR::Compiler->om.sizeofReferenceField();
      else
         elementSize = TR::Symbol::convertTypeToSize(dt);
      }

   static bool optimizeForConstantLengthArrayCopy = feGetEnv("TR_OptimizeForConstantLengthArrayCopy");
   static bool ignoreDirectionForConstantLengthArrayCopy = feGetEnv("TR_IgnoreDirectionForConstantLengthArrayCopy");
#define shortConstArrayWithDirThreshold 256
#define shortConstArrayWithoutDirThreshold  16*4
   bool isShortConstArrayWithDirection = false;
   bool isShortConstArrayWithoutDirection = false;
   uint32_t size;
   if (sizeNode->getOpCode().isLoadConst() && cg->comp()->target().is64Bit() && optimizeForConstantLengthArrayCopy)
      {
      size = TR::TreeEvaluator::integerConstNodeValue(sizeNode, cg);
      if ((node->isForwardArrayCopy() || node->isBackwardArrayCopy()) && !ignoreDirectionForConstantLengthArrayCopy)
         {
         if (size <= shortConstArrayWithDirThreshold) isShortConstArrayWithDirection = true;
         }
      else
         {
         if (size <= shortConstArrayWithoutDirThreshold) isShortConstArrayWithoutDirection = true;
         }
      }

   if (isShortConstArrayWithDirection)
      {
      arraycopyForShortConstArrayWithDirection(node, dstReg, srcReg, size, cg);
      cg->recursivelyDecReferenceCount(sizeNode);
      }
   else if (isShortConstArrayWithoutDirection)
      {
      arraycopyForShortConstArrayWithoutDirection(node, dstReg, srcReg, size, cg);
      cg->recursivelyDecReferenceCount(sizeNode);
      }
   else
      {
      bool isSize64Bit = TR::TreeEvaluator::getNodeIs64Bit(sizeNode, cg);
      TR::Register* sizeReg = cg->gprClobberEvaluate(sizeNode, MOVRegReg(isSize64Bit));
      if (cg->comp()->target().is64Bit() && !isSize64Bit)
         {
         generateRegRegInstruction(MOVZXReg8Reg4, node, sizeReg, sizeReg, cg);
         }
      if (elementSize == 8 && cg->comp()->target().is32Bit())
         {
         arrayCopy64BitPrimitiveOnIA32(node, dstReg, srcReg, sizeReg, cg);
         }
      else if (elementSize == 2 && !useREPMOVSW)
         {
         arrayCopy16BitPrimitive(node, dstReg, srcReg, sizeReg, cg);
         }
      else
         {
         arrayCopyDefault(node, elementSize, dstReg, srcReg, sizeReg, cg);
         }
      cg->stopUsingRegister(sizeReg);
      cg->decReferenceCount(sizeNode);
      }

   cg->stopUsingRegister(dstReg);
   cg->stopUsingRegister(srcReg);
   cg->decReferenceCount(dstNode);
   cg->decReferenceCount(srcNode);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   //
   // tree looks as follows:
   // arraytranslate
   //    input ptr
   //    output ptr
   //    translation table (dummy)
   //    terminal character (dummy when src is byte and dest is word, otherwise, it's a mask)
   //    input length (in elements)
   //    stopping char (dummy for X)
   // Number of elements translated is returned
   //

   //sourceByte == true iff the source operand is a byte array
   bool sourceByte = node->isSourceByteArrayTranslate();

   TR::Register *srcPtrReg, *dstPtrReg, *transTableReg, *termCharReg, *lengthReg;
   bool stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(0), srcPtrReg, cg);
   bool stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(1), dstPtrReg, cg);
   bool stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyRegInteger(node->getChild(3), termCharReg, cg);
   bool stopUsingCopyReg5 = TR::TreeEvaluator::stopUsingCopyRegInteger(node->getChild(4), lengthReg, cg);
   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *dummy1 = cg->allocateRegister();
   TR::Register *dummy2 = cg->allocateRegister(TR_FPR);
   TR::Register *dummy3 = cg->allocateRegister(TR_FPR);
   TR::Register *dummy4 = cg->allocateRegister(TR_FPR);

   bool arraytranslateOT = false;
   if  (sourceByte && (node->getChild(3)->getOpCodeValue() == TR::iconst) && (node->getChild(3)->getInt() == 0))
      arraytranslateOT = true;

   int noOfDependencies = (sourceByte && !arraytranslateOT) ? 8 : 9;


   TR::RegisterDependencyConditions  *dependencies =
      generateRegisterDependencyConditions((uint8_t)0, noOfDependencies, cg);
   dependencies->addPostCondition(srcPtrReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(dstPtrReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(resultReg, TR::RealRegister::eax, cg);


   dependencies->addPostCondition(dummy1, TR::RealRegister::ebx, cg);
   dependencies->addPostCondition(dummy2, TR::RealRegister::xmm1, cg);
   dependencies->addPostCondition(dummy3, TR::RealRegister::xmm2, cg);
   dependencies->addPostCondition(dummy4, TR::RealRegister::xmm3, cg);


   TR_RuntimeHelper helper ;
   if (sourceByte)
      {

      TR_ASSERT(!node->isTargetByteArrayTranslate(), "Both source and target are byte for array translate");
      if (arraytranslateOT)
      {
         helper = cg->comp()->target().is64Bit() ? TR_AMD64arrayTranslateTROT : TR_IA32arrayTranslateTROT;
         dependencies->addPostCondition(termCharReg, TR::RealRegister::edx, cg);
      }
      else
         helper = cg->comp()->target().is64Bit() ? TR_AMD64arrayTranslateTROTNoBreak : TR_IA32arrayTranslateTROTNoBreak;
      }
   else
      {
      TR_ASSERT(node->isTargetByteArrayTranslate(), "Both source and target are word for array translate");
      helper = cg->comp()->target().is64Bit() ? TR_AMD64arrayTranslateTRTO : TR_IA32arrayTranslateTRTO;
      dependencies->addPostCondition(termCharReg, TR::RealRegister::edx, cg);
      }
   dependencies->stopAddingConditions();
   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy1);
   cg->stopUsingRegister(dummy2);
   cg->stopUsingRegister(dummy3);
   cg->stopUsingRegister(dummy4);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      cg->decReferenceCount(node->getChild(i));

   if (stopUsingCopyReg1)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(srcPtrReg);
   if (stopUsingCopyReg2)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(dstPtrReg);
   if (stopUsingCopyReg4)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(termCharReg);
   if (stopUsingCopyReg5)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(lengthReg);

   node->setRegister(resultReg);
   return resultReg;
   }


static void packUsingShift(TR::Node* node, TR::Register* tempReg, TR::Register* sourceReg, int32_t size, TR::CodeGenerator* cg)
   {
   generateRegRegInstruction(MOV8RegReg, node, tempReg, sourceReg, cg);
   generateRegImmInstruction(SHL8RegImm1, node, sourceReg, size*8, cg);
   generateRegRegInstruction(OR8RegReg, node, sourceReg, tempReg, cg);
   }

static void packXMMWithMultipleValues(TR::Node* node, TR::Register* XMMReg, TR::Register* sourceReg, int8_t size, TR::CodeGenerator* cg)
   {
   switch(size)
      {
      case 1:
      case 2:
         {
         static uint8_t MASKOFSIZEONE[] =
            {
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            };

         static uint8_t MASKOFSIZETWO[] =
            {
            0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01,
            0x00, 0x01, 0x00, 0x01,
            };

         auto snippet = generateX86MemoryReference(cg->findOrCreate16ByteConstant(node, size==1? MASKOFSIZEONE: MASKOFSIZETWO), cg);
         generateRegRegInstruction(MOVQRegReg8, node, XMMReg, sourceReg, cg);
         TR::Register* tempReg = cg->allocateRegister(TR_FPR);
         generateRegMemInstruction(MOVDQURegMem, node, tempReg, snippet, cg);
         generateRegRegInstruction(PSHUFBRegReg, node, XMMReg, tempReg, cg);
         cg->stopUsingRegister(tempReg);
         break;
         }
      case 4:
      case 8:
         generateRegRegInstruction(MOVQRegReg8, node, XMMReg, sourceReg, cg);
         generateRegRegImmInstruction(PSHUFDRegRegImm1, node, XMMReg, XMMReg, size==4? 0x00: 0x44, cg);
         break;
      default:
         TR_ASSERT(0, "Arrayset Evaluator: unsupported pack size");
         break;
      }
   }

static void arraySetToZeroForShortConstantArrays(TR::Node* node, TR::Register* addressReg, uintptr_t size, TR::CodeGenerator* cg)
   {
   // We do special optimization for zero because it happens very frequent.
   // if size < 16:
   //     zero out a GPR
   //     use a greedy approach to store
   // if size >= 16:
   //     zero out a XMM
   //     store XMM as many as we can
   //     handle the reminder using shifting window

   TR::Register* tempReg = NULL;
   if (size < 16)
      {
      tempReg = cg->allocateRegister();
      generateRegRegInstruction(XOR8RegReg, node, tempReg, tempReg, cg);
      int32_t index = 0;
      int8_t packs[4] = {8, 4, 2, 1};

      for (int32_t i=0; i<4; i++)
         {
         int32_t moves = size/packs[i];
         for (int32_t j=0; j<moves; j++)
            {
            generateArrayElementStore(node, addressReg, index, tempReg, packs[i], cg);
            index += packs[i];
            }
         size = size%packs[i];
         }
      }
   else
      {
      tempReg = cg->allocateRegister(TR_FPR);
      generateRegRegInstruction(XORPDRegReg, node, tempReg, tempReg, cg);
      int32_t moves = size/16;
      for (int32_t i=0; i<moves; i++)
         {
         generateArrayElementStore(node, addressReg, i*16, tempReg, 16, cg);
         }
      if (size%16 != 0) generateArrayElementStore(node, addressReg, size-16, tempReg, 16, cg);
      }
   cg->stopUsingRegister(tempReg);
   }

static void arraySetForShortConstantArrays(TR::Node* node, uint8_t elementSize, TR::Register* addressReg, TR::Register* valueReg, uintptr_t size, TR::CodeGenerator* cg)
   {
   const int32_t notWorthPacking = 5;
   const int32_t totalSize = elementSize*size;

   int8_t packs[5] = {16, 8, 4, 2, 1};
   int8_t moves[5] = {0};

   // Compute the number of moves of each register size e.g. 16, 8, 4, 2, 1 byte
   int32_t tempTotalSize = totalSize;
   for (int32_t i=0; i<5; i++)
      {
      moves[i] += tempTotalSize/packs[i];
      tempTotalSize = tempTotalSize%packs[i];
      }

   // If array size(# of bytes) less than 16 bytes, we do it by packing GPR.
   //    e.g. array size is 14, element size is 2 bytes, here is the psudocode:
   //    1. Pack a 4 bytes register from 2 bytes
   //    2. Move 4 bytes into memory starting at array start address
   //    3. Move 4 bytes into memory starting at array start address + 4
   //    4. Move 4 bytes into memory starting at array start address + 8
   //    5. Move 2 bytes into memory starting at array start address + 12
   //    Note here we didn't pack a 8 bytes register because there will be only
   //       one move for that 8 byte register, so the packing cost is too big for
   //       just a single move.
   // Else we pack an XMM first, and we handle the reminder using a "shifting window" method
   //    e.g. array size is 17 byte, element size is 1 byte, here is the psudocode:
   //    1. Pack an XMM register, xmmA, using PSHUFB
   //    2. Store xmmA to memory at array start address
   //    3. Store xmmA to memory at array start address + 1

   if (totalSize < 16)
      {
      for (int32_t i=1; i<5; i++)
         {
         if (packs[i] > elementSize && moves[i] <= notWorthPacking)
            {
            moves[i+1] += moves[i]*2;
            moves[i] = 0;
            }
         else
            {
            break;
            }
         }
      // Start packing
      TR::Register* currentReg = valueReg;
      for (int32_t i=1; i<5; i++)
         {
         if (moves[i] > 0 && packs[i] > elementSize)
            {
            int32_t currentSize = elementSize;
            TR::Register* tempReg = cg->allocateRegister();
            while(currentSize < packs[i])
               {
               packUsingShift(node, tempReg, currentReg, currentSize, cg);
               currentSize *= 2;
               }
            cg->stopUsingRegister(tempReg);
            break;
            }
         }

      // Start moving
      int32_t index = 0;
      for (int32_t i=1;i<5;i++)
         {
         for (int32_t j=0; j<moves[i]; j++)
            {
            generateArrayElementStore(node, addressReg, index, currentReg, packs[i], cg);
            index += packs[i];
            }
         }
      }
   else
      {
      // First check if we are able to do it by just doing equal or less than notWorthPacking movs of the valueReg
      if (size < notWorthPacking)
         {
         for (int32_t i=0; i<size; i++)
            {
            generateArrayElementStore(node, addressReg, i*elementSize, valueReg, elementSize, cg);
            }
         }
      else
         {
         TR::Register* XMM = cg->allocateRegister(TR_FPR);
         packXMMWithMultipleValues(node, XMM, valueReg, elementSize, cg);

         for (int32_t i=0; i<moves[0]; i++)
            {
            generateArrayElementStore(node, addressReg, i*16, XMM, 16, cg);
            }
         const int32_t reminder = totalSize - moves[0]*16;
         if (reminder == elementSize)
            {
            generateArrayElementStore(node, addressReg, moves[0]*16, valueReg, elementSize, cg);
            }
         else if (reminder != 0)
            {
            generateArrayElementStore(node, addressReg, totalSize-16, XMM, 16, cg);
            }
         cg->stopUsingRegister(XMM);
         }
      }
   }

static void arraySet64BitPrimitiveOnIA32(TR::Node* node, TR::Register* addressReg, TR::Register* valueReg, TR::Register* sizeReg, TR::CodeGenerator* cg)
   {
   TR::Register* XMM = cg->allocateRegister(TR_FPR);

   TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)2, cg);
   deps->addPostCondition(addressReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(sizeReg, TR::RealRegister::ecx, cg);

   TR::LabelSymbol* startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* endLabel = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();
   endLabel->setEndInternalControlFlow();

   TR::LabelSymbol* loopLabel = generateLabelSymbol(cg);

   generateLabelInstruction(LABEL, node, startLabel, cg);

   // Load value to one XMM register
   switch (valueReg->getKind())
      {
      case TR_GPR:
         generateRegRegInstruction(MOVDRegReg4, node, XMM, valueReg->getHighOrder(), cg);
         generateRegImmInstruction(PSLLQRegImm1, node, XMM, 32, cg);
         generateRegRegInstruction(MOVDRegReg4, node, XMM, valueReg->getLowOrder(), cg);
         break;
      case TR_FPR:
         generateRegRegInstruction(MOVDQURegReg, node, XMM, valueReg, cg);
         break;
      default:
         TR_ASSERT(0, "Arrayset Evaluator: unsupported register type");
         break;
      }

   // Store the XMM register to memory via a loop
   // Example:
   //   SHR RCX,3
   //   JRCXZ endLabel
   // loopLabel:
   //   MOVQ [RDI+8*RCX-8],XMM0
   //   LOOP loopLabel
   // endLable:
   //   # LOOP END
   generateRegImmInstruction(SHRRegImm1(), node, sizeReg, 3, cg);
   generateLabelInstruction(JRCXZ1, node, endLabel, cg);
   generateLabelInstruction(LABEL, node, loopLabel, cg);
   generateMemRegInstruction(MOVQMemReg, node, generateX86MemoryReference(addressReg, sizeReg, 3, -8, cg), XMM, cg);
   generateLabelInstruction(LOOP1, node, loopLabel, cg);

   generateLabelInstruction(LABEL, node, endLabel, deps, cg);
   cg->stopUsingRegister(XMM);
   }

static void arraySetDefault(TR::Node* node, uint8_t elementSize, TR::Register* addressReg, TR::Register* valueReg, TR::Register* sizeReg, TR::CodeGenerator* cg)
   {
   TR::Register* EAX = cg->allocateRegister();

   TR::RegisterDependencyConditions* stosDependencies = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)3, cg);
   stosDependencies->addPostCondition(EAX, TR::RealRegister::eax, cg);
   stosDependencies->addPostCondition(sizeReg, TR::RealRegister::ecx, cg);
   stosDependencies->addPostCondition(addressReg, TR::RealRegister::edi, cg);

   // Load value to EAX
   TR_X86OpCodes movOpcode;
   switch (valueReg->getKind())
      {
      case TR_GPR:
         movOpcode = MOVRegReg(elementSize == 8);
         break;
      case TR_FPR:
         movOpcode = elementSize == 8 ? MOVQReg8Reg : MOVDReg4Reg;
         break;
      default:
         TR_ASSERT(0, "Arrayset Evaluator: unsupported register type");
         break;
      }
   generateRegRegInstruction(movOpcode, node, EAX, valueReg, cg);

   // Store EAX into memory
   TR_X86OpCodes repOpcode;
   int32_t shiftAmount = 0;
   switch (elementSize)
      {
      case 1:
         repOpcode = REPSTOSB;
         shiftAmount = 0;
         break;
      case 2:
         repOpcode = REPSTOSW;
         shiftAmount = 1;
         break;
      case 4:
         repOpcode = REPSTOSD;
         shiftAmount = 2;
         break;
      case 8:
         repOpcode = REPSTOSQ;
         shiftAmount = 3;
         break;
      default:
         TR_ASSERT(0, "Arrayset Evaluator: unsupported fill size");
         break;
      }

   if (shiftAmount) generateRegImmInstruction(SHRRegImm1(), node, sizeReg, shiftAmount, cg);
   generateInstruction(repOpcode, node, stosDependencies, cg);
   cg->stopUsingRegister(EAX);
   }

TR::Register *OMR::X86::TreeEvaluator::arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // arrayset
   //    address
   //    fill value
   //    array size (in bytes, unsigned)

   TR::Node* addressNode  = node->getChild(0); // Address
   TR::Node* valueNode    = node->getChild(1); // Value
   TR::Node* sizeNode     = node->getChild(2); // Size

   TR::Register* addressReg = TR::TreeEvaluator::intOrLongClobberEvaluate(addressNode, cg->comp()->target().is64Bit(), cg);
   uintptr_t size;
   bool isSizeConst = false;
   bool isValueZero = false;

   bool isShortConstantArray = false;
   bool isShortConstantArrayWithZero = false;

   static bool isConstArraysetEnabled = (NULL == feGetEnv("TR_DisableConstArrayset"));
   if (isConstArraysetEnabled && cg->getX86ProcessorInfo().supportsSSSE3() && cg->comp()->target().is64Bit())
      {
      if (valueNode->getOpCode().isLoadConst() && !valueNode->getOpCode().isFloat() && !valueNode->getOpCode().isDouble())
         {
         if (0 == TR::TreeEvaluator::integerConstNodeValue(valueNode, cg)) isValueZero = true;
         }

      if (sizeNode->getOpCode().isLoadConst())
         {
         size = TR::TreeEvaluator::integerConstNodeValue(sizeNode, cg);
         isSizeConst = true;
         }

      // Prerequisites of constant length optimization
      static char* optimizedArrayLengthStr = feGetEnv("TR_ConstArraySetOptLength");
      int32_t optimizedArrayLength = optimizedArrayLengthStr? atoi(optimizedArrayLengthStr): 256;
      if (isSizeConst && size <= optimizedArrayLength)
         {
         if (isValueZero)
            isShortConstantArrayWithZero = true;
         else
            isShortConstantArray = true;
         }
      }

   const uint8_t elementSize = valueNode->getOpCode().isRef()
      ? TR::Compiler->om.sizeofReferenceField()
      : valueNode->getSize();

   if (isShortConstantArrayWithZero)
      {
      arraySetToZeroForShortConstantArrays(node, addressReg, size, cg);
      cg->recursivelyDecReferenceCount(sizeNode);
      cg->recursivelyDecReferenceCount(valueNode);
      }
   else if (isShortConstantArray)
      {
      TR::Register* valueReg;
      if (valueNode->getOpCode().isFloat() || valueNode->getOpCode().isDouble())
         {
         TR::Register* tempReg = cg->evaluate(valueNode);
         TR_ASSERT(tempReg->getKind() == TR_FPR, "Float and Double must be in an XMM register");
         valueReg = cg->allocateRegister();
         generateRegRegInstruction(elementSize == 8 ? MOVQReg8Reg : MOVDReg4Reg, node, valueReg, tempReg, cg);
         cg->stopUsingRegister(tempReg);
         }
      else
         {
         valueReg = TR::TreeEvaluator::intOrLongClobberEvaluate(valueNode, TR::TreeEvaluator::getNodeIs64Bit(valueNode, cg), cg);
         }
      arraySetForShortConstantArrays(node, elementSize, addressReg, valueReg, size / elementSize, cg);
      cg->recursivelyDecReferenceCount(sizeNode);
      cg->decReferenceCount(valueNode);
      cg->stopUsingRegister(valueReg);
      }
   else
      {
      TR::Register* sizeReg = TR::TreeEvaluator::intOrLongClobberEvaluate(sizeNode, TR::TreeEvaluator::getNodeIs64Bit(sizeNode, cg), cg);
      TR::Register* valueReg = cg->evaluate(valueNode);

      // Zero-extend array size if passed in as 32-bit on 64-bit architecture
      if (cg->comp()->target().is64Bit() && !TR::TreeEvaluator::getNodeIs64Bit(sizeNode, cg))
         {
         generateRegRegInstruction(MOVZXReg8Reg4, node, sizeReg, sizeReg, cg);
         }
      if (elementSize == 8 && cg->comp()->target().is32Bit())
         {
         arraySet64BitPrimitiveOnIA32(node, addressReg, valueReg, sizeReg, cg);
         }
      else
         {
         arraySetDefault(node, elementSize, addressReg, valueReg, sizeReg, cg);
         }
      cg->decReferenceCount(sizeNode);
      cg->decReferenceCount(valueNode);
      cg->stopUsingRegister(sizeReg);
      cg->stopUsingRegister(valueReg);
      }
   cg->stopUsingRegister(addressReg);
   cg->decReferenceCount(addressNode);
   return NULL;
   }

bool OMR::X86::TreeEvaluator::constNodeValueIs32BitSigned(TR::Node *node, intptr_t *value, TR::CodeGenerator *cg)
   {
   *value = TR::TreeEvaluator::integerConstNodeValue(node, cg);
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

bool OMR::X86::TreeEvaluator::getNodeIs64Bit(TR::Node *node, TR::CodeGenerator *cg)
   {
   /* This function is intended to allow existing 32-bit instruction selection code
    * to be reused, almost unchanged, to do the corresponding 64-bit logic on AMD64.
    * It compiles away to nothing on IA32, thus preserving performance and code size
    * on IA32, while allowing the logic to be generalized to suit AMD64.
    *
    * Don't use this function for 64-bit logic on IA32; instead, either (1) use
    * separate logic, or (2) use a different test for 64-bitness.  Usually this is
    * not a hinderance, because 64-bit code on IA32 uses register pairs and other
    * things that are totally different from their 32-bit counterparts.
    */
   TR_ASSERT(cg->comp()->target().is64Bit() || node->getSize() <= 4, "64-bit nodes on 32-bit platforms shouldn't use getNodeIs64Bit");
   return cg->comp()->target().is64Bit() && node->getSize() > 4;
   }

intptr_t OMR::X86::TreeEvaluator::integerConstNodeValue(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (TR::TreeEvaluator::getNodeIs64Bit(node, cg))
      {
      return (intptr_t)node->getLongInt(); // Cast to satisfy 32-bit compilers, even though they never take this path
      }
   else
      {
      TR_ASSERT(node->getSize() <= 4, "For efficiency on IA32, only call integerConstNodeValue for 32-bit constants");
      return node->getInt();
      }
   }


TR::Register *
OMR::X86::TreeEvaluator::performCall(
      TR::Node *node,
      bool isIndirect,
      bool spillFPRegs,
      TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol *callSymbol = symRef->getSymbol()->castToMethodSymbol();

   TR::Register *returnRegister;
   TR::Linkage *linkage = cg->getLinkage(callSymbol->getLinkageConvention());
   if (isIndirect)
      returnRegister = linkage->buildIndirectDispatch(node);
   else
      returnRegister = linkage->buildDirectDispatch(node, spillFPRegs);

   if (cg->enableRematerialisation() &&
       cg->supportsStaticMemoryRematerialization())
      TR::TreeEvaluator::removeLiveDiscardableStatics(cg);

   node->setRegister(returnRegister);
   return returnRegister;
   }

TR::Register *
OMR::X86::TreeEvaluator::performHelperCall(
      TR::Node *node,
      TR::SymbolReference *helperSymRef,
      TR::ILOpCodes helperCallOpCode,
      bool spillFPRegs,
      TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, helperCallOpCode);
   if(helperSymRef)
      node->setSymbolReference(helperSymRef);
   TR::Register *targetReg = TR::TreeEvaluator::performCall(node, false, spillFPRegs, cg);
   TR::Node::recreate(node, opCode);
   return targetReg;
   }

/**
 * generate a single precision sqrt instruction
 */
static TR::Register * inlineSinglePrecisionSQRT(TR::Node *node, TR::CodeGenerator *cg)
{

  // Sometimes the call can have 2 children, where the first one is loadaddr
  TR::Node     *operand        = NULL;
  TR::Node     *firstChild     = NULL;
  TR::Register *targetRegister = NULL;

  if (node->getNumChildren() == 1)
    {
    operand = node->getFirstChild();
    }
  else
    {
    firstChild = node->getFirstChild();
    operand    = node->getSecondChild();
    }
  TR::Register *opRegister = NULL;
  opRegister = cg->evaluate(operand);

  if (opRegister->getKind() == TR_FPR)
    {
      if (operand->getReferenceCount()==1)
   targetRegister = opRegister;
      else
   targetRegister = cg->allocateSinglePrecisionRegister(TR_FPR);

      generateRegRegInstruction(SQRTSSRegReg, node, targetRegister, opRegister, cg);
    }
  else
    {
      targetRegister = cg->floatClobberEvaluate(operand);

      if (targetRegister)
         {
         if (targetRegister->needsPrecisionAdjustment() || targetRegister->mayNeedPrecisionAdjustment())
            {
            TR::TreeEvaluator::insertPrecisionAdjustment(targetRegister, operand, cg);
            }
         targetRegister->setMayNeedPrecisionAdjustment();
         targetRegister->setNeedsPrecisionAdjustment();
         }

      generateFPRegInstruction(FSQRTReg, node, targetRegister, cg);
    }


  node->setRegister(targetRegister);
  if (firstChild)
    cg->recursivelyDecReferenceCount(firstChild);

  cg->decReferenceCount(operand);
  return node->getRegister();
}

/** \brief
 *    Generate instructions to do atomic memory update.
 *
 *  \param node
 *     The tree node
 *
 *  \param op
 *     The instruction op code to perform the memory update
 *
 *  \param cg
 *     The code generator
 */
static TR::Register* inlineAtomicMemoryUpdate(TR::Node* node, TR_X86OpCodes op, TR::CodeGenerator* cg)
   {
   TR_ASSERT((!TR_X86OpCode(op).hasLongSource() && !TR_X86OpCode(op).hasLongTarget()) || cg->comp()->target().is64Bit(), "64-bit instruction not supported on IA32");
   TR::Register* address = cg->evaluate(node->getChild(0));
   TR::Register* value   = cg->gprClobberEvaluate(node->getChild(1), MOVRegReg());

   generateMemRegInstruction(op, node, generateX86MemoryReference(address, 0, cg), value, cg);

   node->setRegister(value);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   return value;
   }

/** \brief
 *    Generate instructions to do atomic compare and 64-bit memory update on X86-32.
 *
 *  \param node
 *     The tree node
 *
 *  \param returnValue
 *     Indicate whether the result is the old memory value or the status of memory update
 *
 *  \param cg
 *     The code generator
 */
static TR::Register* inline64BitAtomicCompareAndMemoryUpdateOn32Bit(TR::Node* node, bool returnValue, TR::CodeGenerator* cg)
   {
   TR_ASSERT(cg->comp()->target().is32Bit(), "32-bit only");
   TR::Register* address  = cg->evaluate(node->getChild(0));
   TR::Register* oldvalue = cg->longClobberEvaluate(node->getChild(1));
   TR::Register* newvalue = cg->evaluate(node->getChild(2));

   TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions((uint8_t)4, (uint8_t)4, cg);
   deps->addPreCondition(oldvalue->getLowOrder(),  TR::RealRegister::eax, cg);
   deps->addPreCondition(oldvalue->getHighOrder(), TR::RealRegister::edx, cg);
   deps->addPreCondition(newvalue->getLowOrder(),  TR::RealRegister::ebx, cg);
   deps->addPreCondition(newvalue->getHighOrder(), TR::RealRegister::ecx, cg);
   deps->addPostCondition(oldvalue->getLowOrder(),  TR::RealRegister::eax, cg);
   deps->addPostCondition(oldvalue->getHighOrder(), TR::RealRegister::edx, cg);
   deps->addPostCondition(newvalue->getLowOrder(),  TR::RealRegister::ebx, cg);
   deps->addPostCondition(newvalue->getHighOrder(), TR::RealRegister::ecx, cg);

   generateMemInstruction(LCMPXCHG8BMem, node, generateX86MemoryReference(address, 0, cg), deps, cg);
   if (!returnValue)
      {
      cg->stopUsingRegister(oldvalue->getHighOrder());
      oldvalue = oldvalue->getLowOrder();
      generateRegInstruction(SETE1Reg, node, oldvalue, cg);
      generateRegRegInstruction(MOVZXReg4Reg1, node, oldvalue, oldvalue, cg);
      }

   node->setRegister(oldvalue);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   return oldvalue;
   }

/** \brief
 *    Generate instructions to do atomic compare and memory update.
 *
 *  \param node
 *     The tree node
 *
 *  \param op
 *     The instruction op code to perform the memory update
 *
 *  \param returnValue
 *     Indicate whether the result is the old memory value or the status of memory update
 *
 *  \param cg
 *     The code generator
 */
static TR::Register* inlineAtomicCompareAndMemoryUpdate(TR::Node* node, bool returnValue, TR::CodeGenerator* cg)
   {
   bool isNode64Bit = node->getChild(1)->getDataType().isInt64();
   if (cg->comp()->target().is32Bit() && isNode64Bit)
      {
      return inline64BitAtomicCompareAndMemoryUpdateOn32Bit(node, returnValue, cg);
      }

   TR::Register* address  = cg->evaluate(node->getChild(0));
   TR::Register* oldvalue = cg->gprClobberEvaluate(node->getChild(1), MOVRegReg());
   TR::Register* newvalue = cg->evaluate(node->getChild(2));

   TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
   deps->addPreCondition(oldvalue, TR::RealRegister::eax, cg);
   deps->addPostCondition(oldvalue, TR::RealRegister::eax, cg);

   generateMemRegInstruction(LCMPXCHGMemReg(isNode64Bit), node, generateX86MemoryReference(address, 0, cg), newvalue, deps, cg);
   if (!returnValue)
      {
      generateRegInstruction(SETE1Reg, node, oldvalue, cg);
      generateRegRegInstruction(MOVZXReg4Reg1, node, oldvalue, oldvalue, cg);
      }

   node->setRegister(oldvalue);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   return oldvalue;
   }

// TR::icall, TR::acall, TR::lcall, TR::fcall, TR::dcall, TR::call handled by directCallEvaluator
TR::Register *OMR::X86::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference* SymRef = node->getSymbolReference();

   if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::singlePrecisionSQRTSymbol))
      {
      return inlineSinglePrecisionSQRT(node, cg);
      }

   if (SymRef && SymRef->getSymbol()->castToMethodSymbol()->isInlinedByCG())
      {
      TR_X86OpCodes op = BADIA32Op;

      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicAddSymbol))
         {
         op = node->getChild(1)->getDataType().isInt32() ? LADD4MemReg : LADD8MemReg;
         }
      else if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicFetchAndAddSymbol))
         {
         op = node->getChild(1)->getDataType().isInt32() ? LXADD4MemReg : LXADD8MemReg;
         }
      else if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicFetchAndAdd32BitSymbol))
         {
         op = LXADD4MemReg;
         }
      else if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicFetchAndAdd64BitSymbol))
         {
         op = LXADD8MemReg;
         }
      else if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicSwapSymbol))
         {
         op = node->getChild(1)->getDataType().isInt32() ? XCHG4MemReg : XCHG8MemReg;
         }
      else if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicSwap32BitSymbol))
         {
         op = XCHG4MemReg;
         }
      else if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicSwap64BitSymbol))
         {
         op = XCHG8MemReg;
         }

      if (op != BADIA32Op)
         {
         return inlineAtomicMemoryUpdate(node, op, cg);
         }
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicCompareAndSwapReturnStatusSymbol))
         {
         return inlineAtomicCompareAndMemoryUpdate(node, false, cg);
         }
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicCompareAndSwapReturnValueSymbol))
         {
         return inlineAtomicCompareAndMemoryUpdate(node, true, cg);
         }
      }

   // If the method to be called is marked as an inline method, see if it can
   // actually be generated inline.
   //
   TR::Register *returnRegister = TR::TreeEvaluator::performCall(node, false, true, cg);

   // A strictfp caller needs to adjust double return values;
   // a float callee always returns values that have correct precision.
   //
   if (returnRegister &&
       returnRegister->needsPrecisionAdjustment() &&
       comp->getCurrentMethod()->isStrictFP())
      {
      TR::TreeEvaluator::insertPrecisionAdjustment(returnRegister, node, cg);
      }

   return returnRegister;
   }

// TR::icalli, TR::acalli, TR::lcalli, TR::fcalli, TR::dcalli, TR::calli handled by indirectCallEvaluator
TR::Register *OMR::X86::TreeEvaluator::indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // If the method to be called is marked as an inline method, see if it can
   // actually be generated inline.
   //
   TR::Compilation *comp = cg->comp();
   TR::Register     *returnRegister = NULL;
   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
   if (symbol->isVMInternalNative() || symbol->isJITInternalNative())
      {
      if (TR::TreeEvaluator::VMinlineCallEvaluator(node, true, cg))
         returnRegister = node->getRegister();
      else
         returnRegister = TR::TreeEvaluator::performCall(node, true, true, cg);
      }
   else
      returnRegister = TR::TreeEvaluator::performCall(node, true, true, cg);

   // A strictfp caller needs to adjust double return values;
   // a float callee always returns values that have correct precision.
   //
   if (returnRegister &&
       returnRegister->needsPrecisionAdjustment() &&
       comp->getCurrentMethod()->isStrictFP())
      {
      TR::TreeEvaluator::insertPrecisionAdjustment(returnRegister, node, cg);
      }

   return returnRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *tempReg = cg->evaluate(firstChild);
   cg->decReferenceCount(firstChild);
   return tempReg;
   }


void OMR::X86::TreeEvaluator::compareGPRegisterToConstantForEquality(TR::Node          *node,
                                                                 int32_t           value,
                                                                 TR::Register      *cmpRegister,
                                                                 TR::CodeGenerator *cg)
   {
   if (value == 0)
      {
      generateRegRegInstruction(TEST4RegReg, node, cmpRegister, cmpRegister, cg);
      }
   else
      {
      TR::TreeEvaluator::compareGPRegisterToImmediate(node, cmpRegister, value, cg);
      }
   }


TR::Register *OMR::X86::TreeEvaluator::fenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_X86OpCode fenceOp = BADIA32Op;
   if (node->isLoadFence() && node->isStoreFence())
      fenceOp.setOpCodeValue(MFENCE);
   else if (node->isLoadFence())
      fenceOp.setOpCodeValue(LFENCE);
   else if (node->isStoreFence())
      fenceOp.setOpCodeValue(SFENCE);

   if (fenceOp.getOpCodeValue() != BADIA32Op)
      {
      new (cg->trHeapMemory()) TR::Instruction(node, fenceOp.getOpCodeValue(), cg);
      }
   else
      {
      TR::trap();
      }
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::generateLEAForLoadAddr(TR::Node *node,
                                                          TR::MemoryReference *memRef,
                                                          TR::SymbolReference *symRef,
                                                          TR::CodeGenerator *cg,
                                                          bool isInternalPointer)
   {
   TR::Register        *targetRegister;

   TR::Compilation *comp = cg->comp();

   //memRef maybe not directly generated from symRef, for example symRef plus second child in analyzeLea
   // So we need also check if it is an internal pointer when allocate regsiter
   if (symRef->getSymbol()->isLocalObject() && !isInternalPointer)
      targetRegister = cg->allocateCollectedReferenceRegister();
   else
      targetRegister = cg->allocateRegister();

   // TODO:AMD64: This often generates mov r1,imm64 followed by lea r1,[r1] which is dumb
   TR_X86OpCodes op = LEARegMem();
   if (TR::Compiler->om.generateCompressedObjectHeaders() &&
         (node->getSymbol()->isClassObject() /*|| node->getSymbol()->isAddressOfClassObject()*/))
      op = LEA4RegMem;

   TR::Instruction *instr = generateRegMemInstruction(op, node, targetRegister, memRef, cg);
   memRef->decNodeReferenceCounts(cg);
   // HCR register PIC site in generateLEAForLoadAddr
   if (node && node->getSymbol()->isClassObject() && cg->wantToPatchClassPointer(NULL, node))
      {
      // I think this has no effect; instr has no immediate source operand.
      comp->getStaticHCRPICSites()->push_front(instr);
      }
   if (cg->enableRematerialisation())
       {
       TR_RematerializableTypes type;

       if (node && node->getOpCode().hasSymbolReference() && node->getSymbol() && node->getSymbol()->isClassObject())
          (TR::Compiler->om.generateCompressedObjectHeaders() || cg->comp()->target().is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;
       else
          type = TR_RematerializableAddress;

       setDiscardableIfPossible(type, targetRegister, node, instr, symRef, cg);
       }

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MemoryReference  *memRef = generateX86MemoryReference(symRef, cg);

   // for loadaddr, directly allocated register according to its symRef, since memRef only represents symRef
   TR::Register *targetRegister = TR::TreeEvaluator::generateLEAForLoadAddr(node, memRef, symRef, cg, false);

   if (symRef->isUnresolved() && cg->comp()->target().is32Bit())
      {
      TR::TreeEvaluator::padUnresolvedDataReferences(node, memRef->getSymbolReference(), cg);
      }

   node->setRegister(targetRegister);
   return targetRegister;
   }


TR::Register *OMR::X86::TreeEvaluator::aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();
   if (globalReg == NULL)
      {
      if (node->getRegLoadStoreSymbolReference()->getSymbol()->isNotCollected())
         {
         globalReg = cg->allocateRegister();
         if (node->getRegLoadStoreSymbolReference()->getSymbol()->isInternalPointer())
            {
            globalReg->setContainsInternalPointer();
            globalReg->setPinningArrayPointer(node->getRegLoadStoreSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }
         }
      else
         {
         if (node->getRegLoadStoreSymbolReference()->getSymbol()->isInternalPointer())
            {
            globalReg = cg->allocateRegister();
            globalReg->setContainsInternalPointer();
            globalReg->setPinningArrayPointer(node->getRegLoadStoreSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }
         else
            globalReg = cg->allocateCollectedReferenceRegister();
         }
      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register *OMR::X86::TreeEvaluator::integerRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();
   TR::Compilation *comp = cg->comp();
   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister();
      node->setRegister(globalReg);
      }

   // Everything that can put a value in a global reg (iRegStore, method parameters)
   // zeroes out the upper bits.
   //
   if (cg->comp()->target().is64Bit() && node->getOpCodeValue()==TR::iRegLoad && performTransformation(comp, "TREE EVALUATION: setUpperBitsAreZero on iRegLoad %s\n", cg->getDebug()->getName(node)))
      globalReg->setUpperBitsAreZero();

   return globalReg;
   }

// also handles TR::aRegStore
TR::Register *OMR::X86::TreeEvaluator::iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);

   if (cg->comp()->target().is64Bit() && node->getDataType() == TR::Int32)
      {
      // We disregard needsSignExtension here intentionally.  By always zero-extending
      // at iRegStores, we are free to setUpperBitsAreZero at every iRegLoad, and lots
      // of goodness results.
      //
      // In theory, this also means we must disregard skipSignExtension at i2l nodes.
      // See i2lEvaluator in AMD64TreeEvaluator.cpp for more information.
      //
      if (!globalReg->areUpperBitsZero())
         {
         generateRegRegInstruction(MOVZXReg8Reg4, node, globalReg, globalReg, cg);
         globalReg->setUpperBitsAreZero(true);
         }
      }
   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *OMR::X86::TreeEvaluator::lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *OMR::X86::TreeEvaluator::GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   for (int i = 0; i < node->getNumChildren(); ++i)
      {
      cg->evaluate(node->getChild(i));
      cg->decReferenceCount(node->getChild(i));
      }
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * child = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(child);

   if ((child->getReferenceCount() > 1 && node->getOpCodeValue() != TR::PassThrough)
       || (node->getOpCodeValue() == TR::PassThrough
           && node->isCopyToNewVirtualRegister()
           && srcReg->getKind() == TR_GPR))
      {
      TR::Register *copyReg;
      TR_RegisterKinds kind = srcReg->getKind();
      TR_ASSERT(kind == TR_GPR, "passThrough does not work for this type of register\n");

      if (srcReg->containsInternalPointer() || !srcReg->containsCollectedReference())
         {
         copyReg = cg->allocateRegister(kind);
         if (srcReg->containsInternalPointer())
            {
            copyReg->setPinningArrayPointer(srcReg->getPinningArrayPointer());
            copyReg->setContainsInternalPointer();
            }
         }
      else
         {
         copyReg = cg->allocateCollectedReferenceRegister();
         }

      if (srcReg->getRegisterPair())
         {
         TR::Register *copyRegLow = cg->allocateRegister(kind);
         generateRegRegInstruction(MOVRegReg(), node, copyReg, srcReg->getHighOrder(), cg);
         generateRegRegInstruction(MOVRegReg(), node, copyRegLow, srcReg->getLowOrder(), cg);
         copyReg = cg->allocateRegisterPair(copyRegLow, copyReg);
         }
      else
         {
         generateRegRegInstruction(MOVRegReg(), node, copyReg, srcReg, cg);
         }

      srcReg = copyReg;
      }

   node->setRegister(srcReg);
   cg->decReferenceCount(child);
   return srcReg;
   }


TR::Register *OMR::X86::TreeEvaluator::BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Block *block = node->getBlock();
   List<TR::Register> popRegisters(cg->trMemory());
   TR::Compilation *comp = cg->comp();

   cg->setCurrentBlock(block);
   if (!block->isExtensionOfPreviousBlock())
      {
      // Need to clear the register associations and reset GPR weights for each
      // extended basic block so that trailing info from previous blocks does
      // not infect the register weights of subsequent blocks.
      //
      TR::Machine *machine = cg->machine();
      machine->clearRegisterAssociations();
      machine->setGPRWeightsFromAssociations();
      machine->resetFPStackRegisters();

      machine->resetXMMGlobalRegisters();

      TR::LabelSymbol           * label = node->getLabel();
      TR::X86LabelInstruction  * inst  = 0;
      if (!label)
         {
         label = generateLabelSymbol(cg);
         node->setLabel(label);
         }

      static bool doAlign = (feGetEnv("TR_DoNotAlignLoopEntries") == NULL);
      static bool alwaysAlignLoops = (feGetEnv("TR_AlwaysAlignLoopEntries") != NULL);
      if (doAlign && !block->isCold() && block->firstBlockInLoop() &&
          (comp->getOptLevel() > warm || alwaysAlignLoops))
         {
         generateAlignmentInstruction(node, 16, cg); // TODO: Derive alignment from CPU cache information
         }

      if (node->getNumChildren() > 0)
         inst = generateLabelInstruction(LABEL, node, label, node->getFirstChild(), &popRegisters, cg);
      else
         inst = generateLabelInstruction(LABEL, node, node->getLabel(), cg);

      if (inst->getDependencyConditions())
         inst->getDependencyConditions()->setMayNeedToPopFPRegisters(true);

      inst->setNeedToClearFPStack(true);

      node->getLabel()->setInstruction(inst);
      block->setFirstInstruction(inst);

      // If this is the first BBStart of the method, its GlRegDeps determine
      // where parameters should be placed.
      //
      if (cg->getCurrentEvaluationTreeTop() == comp->getStartTree())
         cg->getLinkage()->copyGlRegDepsToParameterSymbols(node, cg);
      }

   TR::Instruction *fence =
      generateFenceInstruction(FENCE,
                               node,
                               TR::Node::createRelative32BitFenceNode(node, &block->getInstructionBoundaries()._startPC),
                               cg);

   if (!block->getFirstInstruction())
      block->setFirstInstruction(fence);

   if (comp->getOption(TR_BreakBBStart))
      {
      TR::Machine *machine = cg->machine();
      generateRegImmInstruction(TEST4RegImm4, node, machine->getRealRegister(TR::RealRegister::esp), block->getNumber(), cg);
      generateInstruction(BADIA32Op, node, cg);
      }

   cg->generateDebugCounter((node->getBlock()->isExtensionOfPreviousBlock())? "cg.blocks/extensions":"cg.blocks", 1, TR::DebugCounter::Exorbitant);

   if (!popRegisters.isEmpty())
      {
      ListIterator<TR::Register> popRegsIt(&popRegisters);
      for (TR::Register *popRegister = popRegsIt.getFirst(); popRegister != NULL; popRegister = popRegsIt.getNext())
         {
         generateFPSTiST0RegRegInstruction(FSTRegReg, node, popRegister, popRegister, cg);
         cg->stopUsingRegister(popRegister);
         }
      }

   if (block->isCatchBlock())
      {
      cg->generateCatchBlockBBStartPrologue(node, fence);
      }

   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // We need to find out whether this is the end of the extended basic block
   // this means we need to find out whether the next textual block is an
   // extension of this block.
   //
   TR::Compilation *comp = cg->comp();
   TR::TreeTop *nextTT = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();

   TR::X86FenceInstruction  *instr = generateFenceInstruction(FENCE, node, TR::Node::createRelative32BitFenceNode(node, &node->getBlock()->getInstructionBoundaries()._endPC), cg);

   node->getBlock()->setLastInstruction(instr);

   if (!nextTT || !nextTT->getNode()->getBlock()->isExtensionOfPreviousBlock())
      {
      // We need to record the final state of the register associations at the end
      // of each extended basic block so that the register states and weights
      // are initialized properly at the bottom of the block by the GP register
      // assigner.
      //
      TR::Machine *machine = cg->machine();

      if (cg->enableRegisterAssociations() &&
          cg->getAppendInstruction()->getOpCodeValue() != ASSOCREGS)
         {
         machine->createRegisterAssociationDirective(cg->getAppendInstruction());
         }

      // This label is also used by RegisterDependency to detect the end of a block.
      TR::Instruction *labelInst = NULL;
      if (node->getNumChildren() > 0)
         labelInst = generateLabelInstruction(LABEL, node, generateLabelSymbol(cg), node->getFirstChild(), NULL, cg);
      else
         labelInst = generateLabelInstruction(LABEL, node, generateLabelSymbol(cg), cg);

       node->getBlock()->setLastInstruction(labelInst);

      // Remove any surviving discardable registers.
      //
      if (cg->enableRematerialisation() &&
          !cg->getLiveDiscardableRegisters().empty())
         {
         if (debug("dumpRemat"))
            diagnostic("\n---> Deleting surviving discardable registers at BBEnd [" POINTER_PRINTF_FORMAT "]", node);

         TR::ClobberingInstruction  *clob = NULL;
         if (debug("dumpRemat"))
            diagnostic(":");
         auto iterator = cg->getLiveDiscardableRegisters().begin();
         while (iterator != cg->getLiveDiscardableRegisters().end())
            {
            TR::Register* regCursor = *iterator;
            if (!clob)
            {
               clob = new (cg->trHeapMemory()) TR::ClobberingInstruction(instr, cg->trMemory());
               cg->addClobberingInstruction(clob);
            }
            clob->addClobberedRegister(regCursor);
            iterator = cg->getLiveDiscardableRegisters().erase(iterator);
            regCursor->resetIsDiscardable();
            if (debug("dumpRemat"))
               diagnostic(" %s", regCursor->getRegisterName(comp));
            }
         }
      }

   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::conversionAnalyser(TR::Node          *node,
                                                     TR_X86OpCodes    memoryToRegisterOp,
                                                     TR_X86OpCodes    registerToRegisterOp,
                                                     TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *targetRegister = NULL;
   TR::Compilation *comp = cg->comp();
   if (child->getReferenceCount() > 1)
      {
      sourceRegister = cg->evaluate(child);

      if ( node->parentSupportsLazyClobber()
         && registerToRegisterOp == MOVZXReg8Reg4
         && sourceRegister->areUpperBitsZero()
         && performTransformation(comp, "O^O LAZY CLOBBERING: reuse register %s from %s for %s\n",
               cg->getDebug()->getName(sourceRegister),
               cg->getDebug()->getName(child),
               cg->getDebug()->getName(node)
            ))
         {
         // Other codegens consult the register's node count before clobbering
         // a register, but x86 doesn't yet, leading us to emit extra mov
         // instructions.  we'll now surgically apply the
         // better clobbering logic in cases where it is known to help the
         // removal of sign/zero-extension in array accesses in JCL methods where we don't emit bound checks.
         //
         // Without the setNeedsLazyClobbering mechanism (thereby using only
         // the node's reference count as a clobberability indicator), using
         // the same register for this i2l node AND its child is unsafe: if the
         // i2l has refcount=1 and the child has refcount >= 2, and if the
         // i2l's parent performs a clobberEvaluate, it will incorrectly
         // conclude that the i2l's register is safe to clobber.
         //
         // See also TR::TreeEvaluator::l2iEvaluator for more information.
         //
         sourceRegister->setNeedsLazyClobbering();
         targetRegister = sourceRegister;
         }
      else
         {
         targetRegister = cg->allocateRegister();
         }
      }
   else
      {
      if (child->getRegister() == NULL && child->getOpCode().isMemoryReference())
         {
         // we could have a sequence like
         // iu2l
         //   iiload  <- where this node was materialized by lowering an iaload in compressedPointers mode
         //
         if (node->getOpCodeValue() == TR::iu2l &&
               comp->useCompressedPointers() &&
               child->getOpCode().isLoadIndirect() &&
               child->getSymbolReference()->getSymbol()->getDataType() == TR::Address)
            {
            targetRegister = cg->evaluate(child);
            }
         else
            {
            TR::MemoryReference  *tempMR = generateX86MemoryReference(child, cg);
            targetRegister = cg->allocateRegister();
            generateRegMemInstruction(memoryToRegisterOp, node, targetRegister, tempMR, cg);
            tempMR->decNodeReferenceCounts(cg);
            }
         }
      else
         {
         targetRegister = sourceRegister = cg->evaluate(child);
         }
      }

   if (sourceRegister)
      {
      if (!((sourceRegister == targetRegister) && (registerToRegisterOp == MOVZXReg8Reg4) && (sourceRegister->areUpperBitsZero())))
         generateRegRegInstruction(registerToRegisterOp, node, targetRegister, sourceRegister, cg);
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(child);

   return targetRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::sbyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in byteswapEvaluator");
   TR::Node *child = node->getFirstChild();
   TR::Register *target = cg->shortClobberEvaluate(child);
   // TODO: We are using a ROR instruction to do this -- xchg al, ah would
   // be faster, but TR does not know how to encode that.
   //
   generateRegImmInstruction(ROR2RegImm1, node, target, 8, cg);
   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }


TR::Register *
OMR::X86::TreeEvaluator::icmpsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *pointer      = node->getChild(0);
   TR::Node *compareValue = node->getChild(1);
   TR::Node *replaceValue = node->getChild(2);

   // use the compareValue node to determine if node is 64 bit
   // - the current node cannot be used because both icmpset and lcmpset always return an integer sized result
   // - the pointer node cannot be used because on x64 the pointer is 64 bit for both icmpset and lcmpset but
   //   icmpset should only cmpxchg 4 bytes in memory

   bool nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(compareValue, cg);

   TR::Register *pointerReg = cg->evaluate(pointer);
   TR::MemoryReference *memRef = generateX86MemoryReference(pointerReg, 0, cg);
   TR::Register *compareReg = TR::TreeEvaluator::intOrLongClobberEvaluate(compareValue, nodeIs64Bit, cg);
   TR::Register *replaceReg = cg->evaluate(replaceValue);

   // zero the result register to avoid zero extension after the setne
   TR::Register *resultReg = cg->allocateRegister();

   // an XOR4RegReg clears the top half of the register on x64 so it can be used in all cases
   generateRegRegInstruction(XOR4RegReg, node, resultReg, resultReg, cg);

   TR::RegisterDependencyConditions *deps =
      generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
   deps->addPreCondition (compareReg, TR::RealRegister::eax, cg);
   deps->addPostCondition(compareReg, TR::RealRegister::eax, cg);
   generateMemRegInstruction(cg->comp()->target().isSMP() ? LCMPXCHGMemReg(nodeIs64Bit) : CMPXCHGMemReg(nodeIs64Bit), node, memRef, replaceReg, deps, cg);

   cg->stopUsingRegister(compareReg);

   // if equal result is 0, else result is 1
   generateRegInstruction(SETNE1Reg, node, resultReg, cg);

   node->setRegister(resultReg);
   cg->decReferenceCount(pointer);
   cg->decReferenceCount(compareValue);
   cg->decReferenceCount(replaceValue);

   return resultReg;
   }

TR::Register *
OMR::X86::TreeEvaluator::bztestnsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *pointer      = node->getChild(0);
   TR::Node *replaceValue = node->getChild(1);

   TR::Register *pointerReg = cg->evaluate(pointer);
   TR::MemoryReference *memRef = generateX86MemoryReference(pointerReg, 0, cg);
   TR::Register *replaceReg = cg->evaluate(replaceValue);

   if (replaceValue->getReferenceCount() > 1)
      {
      TR::Register* replaceRegPrev = replaceReg;
      replaceReg = cg->allocateRegister();
      generateRegRegInstruction(MOV1RegReg, node, replaceReg, replaceRegPrev, cg);
      }

   generateMemRegInstruction(XCHG1RegMem, node, memRef, replaceReg, cg);

   node->setRegister(replaceReg);
   cg->decReferenceCount(replaceValue);
   cg->decReferenceCount(pointer);

   return replaceReg;
   }


TR::Register *OMR::X86::TreeEvaluator::PrefetchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 4, "TR::Prefetch should contain 4 children nodes");

   TR::Node  *firstChild  =  node->getFirstChild();
   TR::Node  *secondChild =  node->getChild(1);
   TR::Node  *sizeChild =  node->getChild(2);
   TR::Node  *typeChild =  node->getChild(3);

   TR::Compilation *comp = cg->comp();

   TR_X86OpCode prefetchOp(BADIA32Op);

   static char * disablePrefetch = feGetEnv("TR_DisablePrefetch");
   if (comp->isOptServer() || disablePrefetch)
      {
      cg->recursivelyDecReferenceCount(firstChild);
      cg->recursivelyDecReferenceCount(secondChild);
      cg->recursivelyDecReferenceCount(sizeChild);
      cg->recursivelyDecReferenceCount(typeChild);
      return NULL;
      }

   // Size
   cg->recursivelyDecReferenceCount(sizeChild);

   // Type
   uint32_t type = typeChild->getInt();
   cg->recursivelyDecReferenceCount(typeChild);

   if (type == PrefetchLoad || type == PrefetchLoadL1)
      {
      prefetchOp = PREFETCHT0;
      }
   else if (type == PrefetchStore || type == PrefetchLoadL2)
      {
      prefetchOp = PREFETCHT1;
      }
   else if (type == PrefetchLoadNonTemporal || type == PrefetchStoreNonTemporal)

      {
      prefetchOp = PREFETCHNTA;
      }
   else if (type == PrefetchLoadL3)
      {
      prefetchOp = PREFETCHT2;
      }

   if (prefetchOp.getOpCodeValue() != BADIA32Op)
      {
      // Offset node is a constant.
      if (secondChild->getOpCode().isLoadConst())
         {
         uintptr_t offset = secondChild->getInt();
         TR::Register *addrReg = cg->evaluate(firstChild);
         generateMemInstruction(prefetchOp.getOpCodeValue(), node, generateX86MemoryReference(addrReg, offset, cg), cg);

         cg->decReferenceCount(firstChild);
         cg->recursivelyDecReferenceCount(secondChild);
         }
      else
         {
         TR::Register *addrReg = cg->evaluate(firstChild);
         TR::Register *offsetReg = cg->evaluate(secondChild);
         generateMemInstruction(prefetchOp.getOpCodeValue(), node, generateX86MemoryReference(addrReg, offsetReg, 0, cg), cg);

         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      }
   else
      {
      cg->recursivelyDecReferenceCount(firstChild);
      cg->recursivelyDecReferenceCount(secondChild);
      }

   return NULL;
   }

bool
TR_X86ComputeCC::setCarryBorrow(TR::Node *flagNode, bool invertValue, TR::CodeGenerator *cg)
   {
   TR::Register *flagReg = NULL;

   // TODO: why call this function if you do not want to set the carry flag?
   // do nothing, except evaluate child
   flagReg = cg->evaluate(flagNode);
   cg->decReferenceCount(flagNode);
   return true;
   }

TR::Register *OMR::X86::TreeEvaluator::bcmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   return node->setRegister(NULL);
   }

TR::Register *OMR::X86::TreeEvaluator::bucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareBytesForOrder(node, cg);
   return node->setRegister(NULL);
   }

TR::Register *OMR::X86::TreeEvaluator::scmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   return node->setRegister(NULL);
   }

TR::Register *OMR::X86::TreeEvaluator::sucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compare2BytesForOrder(node, cg);
   return node->setRegister(NULL);
   }

TR::Register *OMR::X86::TreeEvaluator::icmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   return node->setRegister(NULL);
   }

TR::Register *OMR::X86::TreeEvaluator::iucmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::TreeEvaluator::compareIntegersForOrder(node, cg);
   return node->setRegister(NULL);
   }

TR::Register *OMR::X86::TreeEvaluator::butestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return node->setRegister(NULL);
   }

TR::Register *OMR::X86::TreeEvaluator::sutestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return node->setRegister(NULL);
   }

// TR::ibatomicor TR::isatomicor TR::iiatomicor TR::ilatomicor
TR::Register *OMR::X86::TreeEvaluator::atomicorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getOpCode().isIndirect(),
          "so far we only deal with indirect atomic or's");

   TR::Node *addrChild  = node->getFirstChild();
   TR::Node *valueChild = node->getSecondChild();
   TR::MemoryReference *mr = generateX86MemoryReference(node, cg);
   mr->setRequiresLockPrefix();

   TR::Instruction *instr;
   if (valueChild->getOpCode().isLoadConst() &&
       !valueChild->getType().isInt64())
      {
      if (valueChild->getType().isInt8())
         {
         uint8_t konst = valueChild->getByte();
         instr = generateMemImmInstruction(OR1MemImm1, node, mr, konst, cg);
         }
      else if (valueChild->getType().isInt16())
         {
         uint16_t konst = valueChild->getShortInt();
         if (konst & 0x8000)
            instr = generateMemImmInstruction(OR2MemImm2, node, mr, konst, cg);
         else
            instr = generateMemImmInstruction(OR2MemImms, node, mr, konst, cg);
         }
      else
         {
         TR_ASSERT(valueChild->getType().isInt32(), "assertion failure");
         uint32_t konst = valueChild->getInt();
         if (konst & 0x80000000)
            instr = generateMemImmInstruction(OR4MemImm4, node, mr, konst, cg);
         else
            instr = generateMemImmInstruction(OR4MemImms, node, mr, konst, cg);
         }
      }
   else
      {
      TR::Register *valueRegister = cg->evaluate(valueChild);
      if (valueChild->getType().isInt8())
         instr = generateMemRegInstruction(OR1MemReg, node, mr, valueRegister, cg);
      else if (valueChild->getType().isInt16())
         instr = generateMemRegInstruction(OR2MemReg, node, mr, valueRegister, cg);
      else if (valueChild->getType().isInt32())
         instr = generateMemRegInstruction(OR4MemReg, node, mr, valueRegister, cg);
      else
         instr = generateMemRegInstruction(OR8MemReg, node, mr, valueRegister, cg);
      }
   cg->setImplicitExceptionPoint(instr);

   mr->decNodeReferenceCounts(cg);
   cg->decReferenceCount(valueChild);
   return NULL;
   }

TR::Register *
OMR::X86::TreeEvaluator::tstartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   /*
   .Lstart:
      xbegin       .Lfallback
      jmp          .LfallThrough
   .Lfallback:
      test         eax, 0x2
      jne          .Ltransient
      jmp          .Lpersistent
   .Lend:
   */
   TR::Node *persistentFailureNode = node->getFirstChild();
   TR::Node *transientFailureNode = node->getSecondChild();
   TR::Node *fallThroughNode = node->getThirdChild();
   TR::Node *GRANode = NULL;

   TR::LabelSymbol *startLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   startLabel->setStartInternalControlFlow();
   TR::LabelSymbol *endLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   endLabel->setEndInternalControlFlow();

   TR::LabelSymbol *fallbackLabel = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
   TR::LabelSymbol *persistentFailureLabel = persistentFailureNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol *transientFailureLabel  = transientFailureNode->getBranchDestination()->getNode()->getLabel();
   TR::LabelSymbol *fallThroughLabel = fallThroughNode->getBranchDestination()->getNode()->getLabel();

   //if LabelSymbol isn't ready yet, generate a new one and assign to the node.
   if(!fallThroughLabel){
      fallThroughLabel = generateLabelSymbol(cg);
      fallThroughNode->getBranchDestination()->getNode()->setLabel(fallThroughLabel);
   }

   if(!transientFailureLabel){
       transientFailureLabel = generateLabelSymbol(cg);
       transientFailureNode->getBranchDestination()->getNode()->setLabel(transientFailureLabel);
   }

   //in case user will make transientFailure goto persistenFailure, in which case the label will mess up at this point
   //we'd better re-generate the label and set it to persistentFailure node again.
   if(!persistentFailureLabel || persistentFailureLabel != persistentFailureNode->getBranchDestination()->getNode()->getLabel()){
      persistentFailureLabel = generateLabelSymbol(cg);
      persistentFailureNode->getBranchDestination()->getNode()->setLabel(persistentFailureLabel);
   }

   TR::Register *accReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *endLabelConditions;
   TR::RegisterDependencyConditions *fallThroughConditions = NULL;
   TR::RegisterDependencyConditions *persistentConditions = NULL;
   TR::RegisterDependencyConditions *transientConditions = NULL;

   if (fallThroughNode->getNumChildren() != 0)
      {
      GRANode = fallThroughNode->getFirstChild();
      cg->evaluate(GRANode);
      List<TR::Register> popRegisters(cg->trMemory());
      fallThroughConditions = generateRegisterDependencyConditions(GRANode, cg, 0, &popRegisters);
      cg->decReferenceCount(GRANode);
      }

   if (persistentFailureNode->getNumChildren() != 0)
      {
      GRANode = persistentFailureNode->getFirstChild();
      cg->evaluate(GRANode);
      List<TR::Register> popRegisters(cg->trMemory());
      persistentConditions = generateRegisterDependencyConditions(GRANode, cg, 0, &popRegisters);
      cg->decReferenceCount(GRANode);
      }

   if (transientFailureNode->getNumChildren() != 0)
      {
      GRANode = transientFailureNode->getFirstChild();
      cg->evaluate(GRANode);
      List<TR::Register> popRegisters(cg->trMemory());
      transientConditions = generateRegisterDependencyConditions(GRANode, cg, 0, &popRegisters);
      cg->decReferenceCount(GRANode);
      }

   //startLabel
   //add place holder register so that eax would not contain any useful value before xbegin
   TR::Register *dummyReg = cg->allocateRegister();
   dummyReg->setPlaceholderReg();
   TR::RegisterDependencyConditions *startLabelConditions = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
   startLabelConditions->addPostCondition(dummyReg, TR::RealRegister::eax, cg);
   startLabelConditions->stopAddingConditions();
   cg->stopUsingRegister(dummyReg);
   generateLabelInstruction(LABEL, node, startLabel, startLabelConditions, cg);

   //xbegin, if fallback then go to fallbackLabel
   generateLongLabelInstruction(XBEGIN4, node, fallbackLabel, cg);

   //jump to  fallThrough Path
   if (fallThroughConditions)
      generateLabelInstruction(JMP4, node, fallThroughLabel, fallThroughConditions, cg);
   else
      generateLabelInstruction(JMP4, node, fallThroughLabel, cg);

   endLabelConditions = generateRegisterDependencyConditions((uint8_t)0, 1, cg);
   endLabelConditions->addPostCondition(accReg, TR::RealRegister::eax, cg);
   endLabelConditions->stopAddingConditions();

   //Label fallback begin:
   generateLabelInstruction(LABEL, node, fallbackLabel, cg);

   //test eax, 0x2
   generateRegImmInstruction(TEST1AccImm1, node, accReg, 0x2, cg);
   cg->stopUsingRegister(accReg);

   //jne to transientFailure
   if (transientConditions)
      generateLabelInstruction(JNE4, node, transientFailureLabel, transientConditions, cg);
   else
      generateLabelInstruction(JNE4, node, transientFailureLabel, cg);

   //jmp to persistent begin:
   if (persistentConditions)
      generateLabelInstruction(JMP4, node, persistentFailureLabel, persistentConditions, cg);
   else
      generateLabelInstruction(JMP4, node, persistentFailureLabel, cg);

   //Label finish
   generateLabelInstruction(LABEL, node, endLabel, endLabelConditions, cg);

   cg->decReferenceCount(persistentFailureNode);
   cg->decReferenceCount(transientFailureNode);

   return NULL;
   }

TR::Register *
OMR::X86::TreeEvaluator::tfinishEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateInstruction(XEND, node, cg);
   return NULL;
   }

TR::Register *
OMR::X86::TreeEvaluator::tabortEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   //For now, we hardcode an abort reason as 0x04 just for simplicity.
   //TODO: Find a way to detect the real abort reason here
   generateImmInstruction(XABORT, node, 0x04, cg);
   return NULL;
   }

bool
OMR::X86::TreeEvaluator::VMinlineCallEvaluator(TR::Node*, bool, TR::CodeGenerator*)
   {
   return false;
   }

TR::Register *
OMR::X86::TreeEvaluator::ibyteswapEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in byteswapEvaluator");
   TR::Node *child = node->getFirstChild();
   bool nodeIs64Bit = TR::TreeEvaluator::getNodeIs64Bit(child, cg);
   TR::Register *target = TR::TreeEvaluator::intOrLongClobberEvaluate(child, nodeIs64Bit, cg);
   generateRegInstruction(BSWAPReg(nodeIs64Bit), node, target, cg);
   node->setRegister(target);
   cg->decReferenceCount(child);
   return target;
   }

enum BinaryArithmeticOps : uint32_t
   {
   BinaryArithmeticInvalid,
   BinaryArithmeticAdd,
   BinaryArithmeticSub,
   BinaryArithmeticMul,
   BinaryArithmeticDiv,
   BinaryArithmeticAnd,
   BinaryArithmeticOr,
   BinaryArithmeticXor,
   NumBinaryArithmeticOps
   };

static const TR_X86OpCodes BinaryArithmeticOpCodesForReg[TR::NumOMRTypes][NumBinaryArithmeticOps] =
   {
   //  Invalid,       Add,         Sub,         Mul,         Div,          And,         Or,       Xor
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // NoType
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int8
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int16
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int32
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int64
   { BADIA32Op, ADDSSRegReg, SUBSSRegReg, MULSSRegReg,  DIVSSRegReg, BADIA32Op,  BADIA32Op, BADIA32Op  }, // Float
   { BADIA32Op, ADDSDRegReg, SUBSDRegReg, MULSDRegReg,  DIVSDRegReg, BADIA32Op,  BADIA32Op, BADIA32Op  }, // Double
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Address
   { BADIA32Op, PADDBRegReg, PSUBBRegReg, BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorInt8
   { BADIA32Op, PADDWRegReg, PSUBWRegReg, PMULLWRegReg, BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorInt16
   { BADIA32Op, PADDDRegReg, PSUBDRegReg, PMULLDRegReg, BADIA32Op,   PANDRegReg, PORRegReg, PXORRegReg }, // VectorInt32
   { BADIA32Op, PADDQRegReg, PSUBQRegReg, BADIA32Op,    BADIA32Op,   PANDRegReg, PORRegReg, PXORRegReg }, // VectorInt64
   { BADIA32Op, ADDPSRegReg, SUBPSRegReg, MULPSRegReg,  DIVPSRegReg, BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorFloat
   { BADIA32Op, ADDPDRegReg, SUBPDRegReg, MULPDRegReg,  DIVPDRegReg, BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorDouble
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Aggregate
   };

static const TR_X86OpCodes BinaryArithmeticOpCodesForMem[TR::NumOMRTypes][NumBinaryArithmeticOps] =
   {
   //  Invalid,       Add,         Sub,         Mul,         Div,          And,         Or,       Xor
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // NoType
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int8
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int16
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int32
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Int64
   { BADIA32Op, ADDSSRegMem, SUBSSRegMem, MULSSRegMem,  DIVSSRegMem, BADIA32Op,  BADIA32Op, BADIA32Op  }, // Float
   { BADIA32Op, ADDSDRegMem, SUBSDRegMem, MULSDRegMem,  DIVSDRegMem, BADIA32Op,  BADIA32Op, BADIA32Op  }, // Double
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Address
   { BADIA32Op, PADDBRegMem, PSUBBRegMem, BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorInt8
   { BADIA32Op, PADDWRegMem, PSUBWRegMem, PMULLWRegMem, BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorInt16
   { BADIA32Op, PADDDRegMem, PSUBDRegMem, PMULLDRegMem, BADIA32Op,   PANDRegMem, PORRegMem, PXORRegMem }, // VectorInt32
   { BADIA32Op, PADDQRegMem, PSUBQRegMem, BADIA32Op,    BADIA32Op,   PANDRegMem, PORRegMem, PXORRegMem }, // VectorInt64
   { BADIA32Op, ADDPSRegMem, SUBPSRegMem, MULPSRegMem,  DIVPSRegMem, BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorFloat
   { BADIA32Op, ADDPDRegMem, SUBPDRegMem, MULPDRegMem,  DIVPDRegMem, BADIA32Op,  BADIA32Op, BADIA32Op  }, // VectorDouble
   { BADIA32Op, BADIA32Op,   BADIA32Op,   BADIA32Op,    BADIA32Op,   BADIA32Op,  BADIA32Op, BADIA32Op  }, // Aggregate
   };

static const TR::ILOpCodes MemoryLoadOpCodes[TR::NumOMRTypes] =
   {
   TR::BadILOp, // NoType
   TR::BadILOp, // Int8
   TR::BadILOp, // Int16
   TR::BadILOp, // Int32
   TR::BadILOp, // Int64
   TR::fload  , // Float
   TR::dload  , // Double
   TR::BadILOp, // Address
   TR::vload  , // VectorInt8
   TR::vload  , // VectorInt16
   TR::vload  , // VectorInt32
   TR::vload  , // VectorInt64
   TR::vload  , // VectorFloat
   TR::vload  , // VectorDouble
   TR::BadILOp, // Aggregate
   };

// For ILOpCode that can be translated to single SSE/AVX instructions
TR::Register* OMR::X86::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   auto arithmetic = BinaryArithmeticInvalid;

   switch (node->getOpCodeValue())
      {
      case TR::fadd:
      case TR::dadd:
      case TR::vadd:
         arithmetic = BinaryArithmeticAdd;
         break;
      case TR::fsub:
      case TR::dsub:
      case TR::vsub:
         arithmetic = BinaryArithmeticSub;
         break;
      case TR::fmul:
      case TR::dmul:
      case TR::vmul:
         arithmetic = BinaryArithmeticMul;
         break;
      case TR::fdiv:
      case TR::ddiv:
      case TR::vdiv:
         arithmetic = BinaryArithmeticDiv;
         break;
      case TR::vand:
         arithmetic = BinaryArithmeticAnd;
         break;
      case TR::vor:
         arithmetic = BinaryArithmeticOr;
         break;
      case TR::vxor:
         arithmetic = BinaryArithmeticXor;
         break;
      default:
         TR_ASSERT(false, "Unsupported OpCode");
      }

   TR::DataType type = node->getDataType();

   TR::Node* operandNode0 = node->getChild(0);
   TR::Node* operandNode1 = node->getChild(1);

   bool useRegMemForm = TR::CodeGenerator::getX86ProcessorInfo().supportsAVX();

   if (useRegMemForm)
      {
      if (operandNode1->getRegister()                               ||
          operandNode1->getReferenceCount() != 1                    ||
          operandNode1->getOpCodeValue() != MemoryLoadOpCodes[type] ||
          BinaryArithmeticOpCodesForMem[type][arithmetic] == BADIA32Op)
         {
         useRegMemForm = false;
         }
      }

   TR::Register* operandReg0 = cg->evaluate(operandNode0);
   TR::Register* operandReg1 = useRegMemForm ? NULL : cg->evaluate(operandNode1);
   TR::Register* resultReg = cg->allocateRegister(operandReg0->getKind());
   resultReg->setIsSinglePrecision(operandReg0->isSinglePrecision());

   TR_X86OpCodes opCode = useRegMemForm ? BinaryArithmeticOpCodesForMem[type][arithmetic] : BinaryArithmeticOpCodesForReg[type][arithmetic];
   TR_ASSERT(opCode != BADIA32Op, "FloatingPointAndVectorBinaryArithmeticEvaluator: unsupported data type or arithmetic.");

   if (TR::CodeGenerator::getX86ProcessorInfo().supportsAVX())
      {
      if (useRegMemForm)
         generateRegRegMemInstruction(opCode, node, resultReg, operandReg0, generateX86MemoryReference(operandNode1, cg), cg);
      else
         generateRegRegRegInstruction(opCode, node, resultReg, operandReg0, operandReg1, cg);
      }
   else
      {
      generateRegRegInstruction(MOVDQURegReg, node, resultReg, operandReg0, cg);
      generateRegRegInstruction(opCode, node, resultReg, operandReg1, cg);
      }

   node->setRegister(resultReg);
   cg->decReferenceCount(operandNode0);
   if (operandReg1)
      cg->decReferenceCount(operandNode1);
   else
      cg->recursivelyDecReferenceCount(operandNode1);
   return resultReg;
   }

TR::Register *
OMR::X86::TreeEvaluator::bitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 3, "Wrong number of children in bitpermuteEvaluator");
   TR::Node *value = node->getChild(0);
   TR::Node *addr = node->getChild(1);
   TR::Node *length = node->getChild(2);

   bool nodeIs64Bit = node->getSize() == 8;
   auto valueReg = cg->evaluate(value);
   auto addrReg = cg->evaluate(addr);

   TR::Register *tmpReg = cg->allocateRegister(TR_GPR);

   // Zero result reg
   TR::Register *resultReg = cg->allocateRegister(TR_GPR);
   generateRegRegInstruction(XORRegReg(nodeIs64Bit), node, resultReg, resultReg, cg);

   if (length->getOpCode().isLoadConst())
      {
      // Manage the constant length case
      uintptr_t arrayLen = TR::TreeEvaluator::integerConstNodeValue(length, cg);
      for (uintptr_t x = 0; x < arrayLen; ++x)
         {
         // Zero tmpReg if SET won't do it
         if (x >= 8)
            generateRegRegInstruction(XORRegReg(nodeIs64Bit), node, tmpReg, tmpReg, cg);

         TR::MemoryReference *sourceMR = generateX86MemoryReference(addrReg, x, cg);
         generateRegMemInstruction(L1RegMem, node, tmpReg, sourceMR, cg);
         generateRegRegInstruction(BTRegReg(nodeIs64Bit), node, valueReg, tmpReg, cg);

         generateRegInstruction(SETB1Reg, node, tmpReg, cg);

         // Shift to desired bit
         if (x > 0)
            generateRegImmInstruction(SHLRegImm1(nodeIs64Bit), node, tmpReg, x, cg);

         // OR with result
         TR_X86OpCodes op = (x < 8) ? OR1RegReg : ORRegReg(nodeIs64Bit);
         generateRegRegInstruction(op, node, resultReg, tmpReg, cg);
         }
      }
   else
      {
      auto lengthReg = cg->evaluate(length);

      auto indexReg = cg->allocateRegister(TR_GPR);
      TR::RegisterDependencyConditions *shiftDependencies = generateRegisterDependencyConditions((uint8_t)1, 1, cg);
      shiftDependencies->addPreCondition(indexReg, TR::RealRegister::ecx, cg);
      shiftDependencies->addPostCondition(indexReg, TR::RealRegister::ecx, cg);

      TR::RegisterDependencyConditions* deps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)2, cg);
      deps->addPostCondition(addrReg, TR::RealRegister::NoReg, cg);
      deps->addPostCondition(indexReg, TR::RealRegister::ecx, cg);

      TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *endLabel = generateLabelSymbol(cg);
      startLabel->setStartInternalControlFlow();
      endLabel->setEndInternalControlFlow();

      // Load initial index, ending if 0
      generateRegRegInstruction(MOV4RegReg, node, indexReg, lengthReg, cg);

      // Test and decrement
      generateLabelInstruction(LABEL, node, startLabel, cg);
      generateLabelInstruction(JRCXZ1, node, endLabel, cg);
      generateRegImmInstruction(SUB4RegImm4, node, indexReg, 1, cg);

      // Load the byte, test the bit and set
      generateRegRegInstruction(XORRegReg(nodeIs64Bit), node, tmpReg, tmpReg, cg);
      TR::MemoryReference *sourceMR = generateX86MemoryReference(addrReg, indexReg, 0, 0, cg);
      generateRegMemInstruction(L1RegMem, node, tmpReg, sourceMR, cg);
      generateRegRegInstruction(BTRegReg(nodeIs64Bit), node, valueReg, tmpReg, cg);
      generateRegInstruction(SETB1Reg, node, tmpReg, cg);

      // Shift and OR with result
      generateRegRegInstruction(SHLRegCL(nodeIs64Bit), node, tmpReg, indexReg, shiftDependencies, cg);
      generateRegRegInstruction(ORRegReg(nodeIs64Bit), node, resultReg, tmpReg, cg);

      // Loop
      generateLabelInstruction(JMP4, node, startLabel, cg);
      generateLabelInstruction(LABEL, node, endLabel, deps, cg);

      cg->stopUsingRegister(indexReg);
      }

   cg->stopUsingRegister(tmpReg);

   node->setRegister(resultReg);
   cg->decReferenceCount(value);
   cg->decReferenceCount(addr);
   cg->decReferenceCount(length);

   return resultReg;
   }
