/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <algorithm>                                  // for std::min
#include <assert.h>                                   // for assert
#include <stdint.h>                                   // for uint8_t, etc
#include <stdlib.h>                                   // for NULL, atoi
#include <string.h>                                   // for strstr
#include "codegen/CodeGenerator.hpp"                  // for CodeGenerator, etc
#include "codegen/FrontEnd.hpp"                       // for TR_FrontEnd, etc
#include "codegen/Instruction.hpp"                    // for Instruction
#include "codegen/Linkage.hpp"                        // for Linkage, etc
#include "codegen/LiveRegister.hpp"
#include "codegen/Machine.hpp"                        // for Machine
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"                   // for RealRegister, etc
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"                       // for Register
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterPair.hpp"                   // for RegisterPair
#include "codegen/RegisterRematerializationInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"                    // for Compilation, etc
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"                                 // for POINTER_PRINTF_FORMAT
#include "env/ObjectModel.hpp"                        // for ObjectModel
#include "env/TRMemory.hpp"                           // for TR_HeapMemory, etc
#include "env/jittypes.h"                             // for uintptrj_t, intptrj_t
#include "il/Block.hpp"                               // for Block, etc
#include "il/DataTypes.hpp"                           // for DataTypes, TR::DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                               // for ILOpCode
#include "il/Node.hpp"                                // for Node, etc
#include "il/Node_inlines.hpp"                        // for Node::getChild, etc
#include "il/Symbol.hpp"                              // for Symbol, etc
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                             // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/MethodSymbol.hpp"                 // for MethodSymbol
#include "infra/Assert.hpp"                           // for TR_ASSERT
#include "infra/Bit.hpp"                              // for leadingZeroes
#include "infra/List.hpp"                             // for List, etc
#include "infra/TRCfgEdge.hpp"                        // for CFGEdge
#include "ras/Debug.hpp"                              // for TR_DebugBase
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"         // for TR_ValueInfo
#endif
#include "x/codegen/HelperCallSnippet.hpp"
#include "x/codegen/OutlinedInstructions.hpp"
#include "x/codegen/RegisterRematerialization.hpp"
#include "x/codegen/X86Evaluator.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                       // for ::LABEL, etc
#include "x/codegen/BinaryCommutativeAnalyser.hpp"
#include "x/codegen/SubtractAnalyser.hpp"

class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;

extern bool existsNextInstructionToTestFlags(TR::Instruction *startInstr,
                                             uint8_t         testMask);

static inline bool alwaysInlineArrayCopy(TR::CodeGenerator *cg)
   {
   return false;
   }

static inline bool isByteLengthSmallEnough(uintptrj_t byteLength)
   {
   return true;
   }

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

   cg->setVMThreadRequired(true);
   generateConditionalJumpInstruction(reverseBranch ? JNO4 : JO4, node, cg, true);
   cg->setVMThreadRequired(false);

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
   bool is64Bit = TR::Compiler->target.is64Bit()? TR::TreeEvaluator::getNodeIs64Bit(node->getFirstChild(), cg) : false;
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
   bool is64Bit = TR::Compiler->target.is64Bit()? TR::TreeEvaluator::getNodeIs64Bit(node->getFirstChild(), cg) : false;
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
   bool is64Bit = TR::Compiler->target.is64Bit()? TR::TreeEvaluator::getNodeIs64Bit(node->getFirstChild(), cg) : false;
   TR_X86OpCodes cmpOp = (value >= -128 && value <= 127) ? CMPRegImms(is64Bit) : CMPRegImm4(is64Bit);
   if (value==0)
      generateRegRegInstruction(TESTRegReg(is64Bit), node, cmpRegister, cmpRegister, cg);
   else
      generateRegImmInstruction(cmpOp, node, cmpRegister, value, cg);
   }

TR::Instruction *OMR::X86::TreeEvaluator::insertLoadConstant(TR::Node                  *node,
                                                        TR::Register              *target,
                                                        intptrj_t                 value,
                                                        TR_RematerializableTypes  type,
                                                        TR::CodeGenerator         *cg,
                                                        TR::Instruction           *currentInstruction)
   {
   TR::Compilation *comp = cg->comp();
   static const TR_X86OpCodes ops[TR_NumRematerializableTypes+1][3] =
      //    load 0      load -1     load c
      { { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Byte constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Short constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Char constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // Int constant
        { XOR4RegReg, OR4RegImms, MOV4RegImm4 },   // 32-bit address constant
        { XOR4RegReg, OR8RegImms, BADIA32Op   } }; // Long address constant; MOVs handled specially

   enum { XOR = 0, OR  = 1, MOV = 2 };

   bool is64Bit = false;

   int opsRow = type;
   if (TR::Compiler->target.is64Bit())
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
            movInstruction = generateRegImm64Instruction(MOV8RegImm64, node, target, value, cg, reloKind);
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

TR::Register *OMR::X86::TreeEvaluator::loadConstant(TR::Node * node, intptrj_t value, TR_RematerializableTypes type, TR::CodeGenerator *cg, TR::Register *targetRegister)
   {
   if (targetRegister == NULL)
      {
      targetRegister = cg->allocateRegister();
      }

   TR::Instruction *instr = TR::TreeEvaluator::insertLoadConstant(node, targetRegister, value, type, cg);

   if (cg->enableRematerialisation())
      {
      if (node && node->getOpCode().hasSymbolReference() && node->getSymbol() && node->getSymbol()->isClassObject())
         (TR::Compiler->om.generateCompressedObjectHeaders() || TR::Compiler->target.is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;

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
      { L1RegMem,     // Byte
        L2RegMem,     // Short
        L2RegMem,     // Char
        L4RegMem,     // Int
        L4RegMem,     // Address (64-bit addresses handled specailly below)
        L8RegMem,     // Long
      };

   TR_X86OpCodes opCode = ops[type];
   if (TR::Compiler->target.is64Bit())
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
   if (TR::Compiler->target.is32Bit())
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
         (TR::Compiler->om.generateCompressedObjectHeaders() || TR::Compiler->target.is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;

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
                  comp->getAppendInstruction()->getOpCode().getOpCodeName(cg),
                  comp->getAppendInstruction());
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
         if (TR::Compiler->target.is64Bit())
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
                        (TR::Compiler->om.generateCompressedObjectHeaders() || TR::Compiler->target.is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;
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

static TR::Register *REPArraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   /*
    * This evaluator will utilize the REPE CMPSQ (64 bit) or CMPSD (32 bit) 
    * instruction to compare 8 (64 bit) or 4 (32 bit) bytes of an array at a 
    * time. Before compare the 8 or 4 byte chunks, the residuals will be taken 
    * care of. By right shifting the length array 3 times, the size of the 
    * residual can be determined and taken care of by using CMPSB, CMPSW and 
    * CMPSD (64 bit). For example, if the residual is 7 Byte, each shift will 
    * set the CF flag, which will result in CMPSB, CMPSW and CMPSD being used.
    * Depending on the result, equal/less than/greater than, the result register
    * will be 0/1/2 respectively
   */
   TR::Register *resultRegister = cg->allocateRegister(TR_GPR);
   TR::Node *src1AddrNode = node->getChild(0);
   TR::Node *src2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);
   
   int32_t byteLen = TR::Compiler->target.is64Bit() ? 8 : 4;

   TR::Register *src1AddrRegister = cg->gprClobberEvaluate(src1AddrNode, MOVRegReg());
   TR::Register *src2AddrRegister = cg->gprClobberEvaluate(src2AddrNode, MOVRegReg());
   TR::Register *arrayLenRegister = cg->gprClobberEvaluate(lengthNode, MOVRegReg());
   TR::Register *counterRegister = cg->allocateRegister();

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *notEqualLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *lessThanLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endResultLabel = generateLabelSymbol(cg);
   
   TR::LabelSymbol *wordCompareLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *dWordCompareLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *qWordCompareLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *equalLabel = generateLabelSymbol(cg);

   generateLabelInstruction(LABEL, node, startLabel, cg);
   startLabel->setStartInternalControlFlow();

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 5, cg);
   deps->addPostCondition(src1AddrRegister, TR::RealRegister::esi, cg);
   deps->addPostCondition(src2AddrRegister, TR::RealRegister::edi, cg);
   deps->addPostCondition(counterRegister, TR::RealRegister::ecx, cg);
   deps->addPostCondition(arrayLenRegister, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(resultRegister, TR::RealRegister::NoReg, cg);

   generateRegRegInstruction(MOVRegReg(), node, counterRegister, arrayLenRegister, cg);   
   cg->stopUsingRegister(arrayLenRegister);

   //Byte Compare
   generateRegImmInstruction(SHRRegImm1(), node, counterRegister, 1, cg);
   generateLabelInstruction(JAE4, node, wordCompareLabel, cg);
   generateInstruction(CMPSB, node, deps, cg);
   generateLabelInstruction(JNE4, node, notEqualLabel, cg);

   //Word Compare
   generateLabelInstruction(LABEL, node, wordCompareLabel, cg);
   generateRegImmInstruction(SHRRegImm1(), node, counterRegister, 1, cg);
   generateLabelInstruction(JAE4, node, dWordCompareLabel, cg);
   generateInstruction(CMPSW, node, deps, cg);
   generateLabelInstruction(JNE4, node, notEqualLabel, cg);

   if (TR::Compiler->target.is64Bit())
      {
      //DWord Compare
      generateLabelInstruction(LABEL, node, dWordCompareLabel, cg);
      generateRegImmInstruction(SHRRegImm1(), node, counterRegister, 1, cg);
      generateLabelInstruction(JAE4, node, qWordCompareLabel, cg);
      generateInstruction(CMPSD, node, deps, cg);
      generateLabelInstruction(JNE4, node, notEqualLabel, cg);

      //QWord Compare
      generateLabelInstruction(LABEL, node, qWordCompareLabel, cg);
      generateInstruction(REPECMPSQ, node, deps, cg);
      generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }
   else 
      {
      generateLabelInstruction(LABEL, node, dWordCompareLabel, cg);
      generateInstruction(REPECMPSD, node, deps, cg);
      generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }

   cg->stopUsingRegister(counterRegister);
   cg->stopUsingRegister(src1AddrRegister);
   cg->stopUsingRegister(src2AddrRegister);

   //Result 
   generateLabelInstruction(LABEL, node, equalLabel, cg);
   generateRegRegInstruction(TR::Compiler->target.is64Bit() ? XOR8RegReg : XOR4RegReg, node, resultRegister, resultRegister, cg); //Equal
   generateLabelInstruction(JMP4, node, endResultLabel, cg);

   generateLabelInstruction(LABEL, node, notEqualLabel, cg);
   generateLabelInstruction(JL4, node, lessThanLabel, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultRegister, 2, cg); //Greater Than
   generateLabelInstruction(JMP4, node, endResultLabel, cg);

   generateLabelInstruction(LABEL, node, lessThanLabel, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultRegister, 1, cg); //Less Than
   
   deps->stopAddingConditions();

   generateLabelInstruction(LABEL, node, endResultLabel, deps, cg);
   endResultLabel->setEndInternalControlFlow();

   node->setRegister(resultRegister);

   cg->decReferenceCount(src1AddrNode);
   cg->decReferenceCount(src2AddrNode );
   cg->decReferenceCount(lengthNode );

   return resultRegister;
   }

static TR::Register *REPArraycmpLenEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   /*
    * This evaluator will utilize the REPE CMPSQ (64 bit) or CMPSD (32 bit) 
    * instruction to compare 8 (64 bit) or 4 (32 bit) bytes of an array at a 
    * time. The residual bytes are compared using the REPE CMPSB instruction.
    * If an inequality was found, in order to determine the first unequal byte,
    * the ESI/EDI registers will be moved back 8 bytes (64 bit), 4 bytes (32 bit)
    * or 1 byte if the inequality was found in the residuals. After this 
    * backtracking is done, REPE CMPSB will be used to find the byte of inequality
   */
   TR::Register *resultRegister = cg->allocateRegister(TR_GPR);
   TR::Node *src1AddrNode = node->getChild(0);
   TR::Node *src2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);
   
   int32_t byteLen = TR::Compiler->target.is64Bit() ? 8 : 4;

   TR::Register *src1AddrRegister = cg->gprClobberEvaluate(src1AddrNode, MOVRegReg());
   TR::Register *src2AddrRegister = cg->gprClobberEvaluate(src2AddrNode, MOVRegReg());
   TR::Register *arrayLenRegister = cg->gprClobberEvaluate(lengthNode, MOVRegReg());

   TR::Register *counterRegister = cg->allocateRegister();

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *residualLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *notEqualLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *backtrackLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *equalLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endResultLabel = generateLabelSymbol(cg);

   generateLabelInstruction(LABEL, node, startLabel, cg);
   startLabel->setStartInternalControlFlow();

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 5, cg);
   deps->addPostCondition(src1AddrRegister, TR::RealRegister::esi, cg);
   deps->addPostCondition(src2AddrRegister, TR::RealRegister::edi, cg);
   deps->addPostCondition(counterRegister, TR::RealRegister::ecx, cg);
   deps->addPostCondition(arrayLenRegister, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(resultRegister, TR::RealRegister::NoReg, cg);

   //Setup
   generateRegRegInstruction(MOVRegReg(), node, resultRegister, src1AddrRegister, cg);   
   generateRegRegInstruction(MOVRegReg(), node, counterRegister, arrayLenRegister, cg);   


   if (TR::Compiler->target.is64Bit())
      {
      //Qword REP compare
      generateRegImmInstruction(SHRRegImm1(), node, counterRegister, 3, cg);
      generateInstruction(REPECMPSQ, node, deps, cg);
      }
   else
      {
      //Dword REP compare
      generateRegImmInstruction(SHRRegImm1(), node, counterRegister, 2, cg);
      generateInstruction(REPECMPSD, node, deps, cg);
      }
   generateLabelInstruction(JNE4, node, backtrackLabel, cg);

   //Residual
   generateLabelInstruction(LABEL, node, residualLabel, cg);
   generateRegRegInstruction(MOVRegReg(), node, counterRegister, arrayLenRegister, cg);   
   generateRegImmInstruction(ANDRegImm4(), node, counterRegister, 0x7, cg);
   generateInstruction(REPECMPSB, node, deps, cg);
   generateLabelInstruction(JNE4, node, notEqualLabel, cg);

   //Equal
   generateRegRegInstruction(MOVRegReg(), node, resultRegister, arrayLenRegister, cg);   
   generateLabelInstruction(JMP4, node, endResultLabel, cg);
   cg->stopUsingRegister(arrayLenRegister);

   //Inequality found, figure out which byte
   generateLabelInstruction(LABEL, node, backtrackLabel, cg);
   generateRegImmInstruction(SUBRegImm4(), node, src1AddrRegister, byteLen, cg);
   generateRegImmInstruction(SUBRegImm4(), node, src2AddrRegister, byteLen, cg);
   generateRegImmInstruction(MOVRegImm4(), node, counterRegister, byteLen, cg);
   generateInstruction(REPECMPSB, node, deps, cg);
   cg->stopUsingRegister(counterRegister);

   //NotEqual
   generateLabelInstruction(LABEL, node, notEqualLabel, cg);
   generateRegImmInstruction(SUBRegImm4(), node, src1AddrRegister, 1, cg);
   generateRegRegInstruction(SUBRegReg(), node, src1AddrRegister, resultRegister, cg);
   generateRegRegInstruction(MOVRegReg(), node, resultRegister, src1AddrRegister, cg);   
   generateLabelInstruction(JMP4, node, endResultLabel, cg);

   //Result 
   cg->stopUsingRegister(src1AddrRegister);
   cg->stopUsingRegister(src2AddrRegister);
   deps->stopAddingConditions();

   generateLabelInstruction(LABEL, node, endResultLabel, deps, cg);
   endResultLabel->setEndInternalControlFlow();

   node->setRegister(resultRegister);

   cg->decReferenceCount(src1AddrNode);
   cg->decReferenceCount(src2AddrNode );
   cg->decReferenceCount(lengthNode );

   return resultRegister;
   }

static TR::Register *ConstLenArraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg, uintptrj_t arrLen)
   {
   /* This evaluator will use CMPSQ (64 bit) or CMPSD (32 bit) to compare an array
    * of known length, by generating a sequence of CMPS instructions. Any residuals
    * will be dealt with CMPSB
    * The result is similar to the REPArraycmpLenEvaluator
   */
   TR::Register *resultRegister = cg->allocateRegister(TR_GPR);
   TR::Node *src1AddrNode = node->getChild(0);
   TR::Node *src2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);
   
   int32_t byteLen = TR::Compiler->target.is64Bit() ? 8 : 4;

   TR::Register *src1AddrRegister = cg->gprClobberEvaluate(src1AddrNode, MOVRegReg());
   TR::Register *src2AddrRegister = cg->gprClobberEvaluate(src2AddrNode, MOVRegReg());

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *notEqualLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *lessThanLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endResultLabel = generateLabelSymbol(cg);

   generateLabelInstruction(LABEL, node, startLabel, cg);
   startLabel->setStartInternalControlFlow();

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 3, cg);
   deps->addPostCondition(src1AddrRegister, TR::RealRegister::esi, cg);
   deps->addPostCondition(src2AddrRegister, TR::RealRegister::edi, cg);
   deps->addPostCondition(resultRegister, TR::RealRegister::NoReg, cg);

   //Deal with the residual bits first, as we want to have a length that is a multiple of 8/4 in order to use CMPSQ
   uintptrj_t residualLen = arrLen % byteLen;
   
   if (TR::Compiler->target.is64Bit() && residualLen & 4)
      {
      generateInstruction(CMPSD, node, deps, cg);
      generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }
   if (residualLen & 2)
      {
      generateInstruction(CMPSW, node, deps, cg);
      generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }
   if (residualLen & 1)
      {
      generateInstruction(CMPSB, node, deps, cg);
      generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }

   arrLen -= residualLen;

   //Perform CMPSQ, for the rest of the array, which should be a multiple of 8.
   if (TR::Compiler->target.is64Bit())
      TR_ASSERT(arrLen % 8 == 0, "64 Bit: arrLen should be a multiple of 8 at this point, the length is %d", arrLen);
   else
      TR_ASSERT(arrLen % 4 == 0, "32 Bit: arrLen should be a multiple of 4 at this point, the length is %d", arrLen);

   while (arrLen > 0)
      {
         arrLen -= byteLen;
         generateInstruction(TR::Compiler->target.is64Bit() ? CMPSQ : CMPSD, node, deps, cg);
         generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }

   //Result 
   cg->stopUsingRegister(src1AddrRegister);
   cg->stopUsingRegister(src2AddrRegister);
   generateRegRegInstruction(TR::Compiler->target.is64Bit() ? XOR8RegReg : XOR4RegReg, node, resultRegister, resultRegister, cg); //Equal
   generateLabelInstruction(JMP4, node, endResultLabel, cg);

   generateLabelInstruction(LABEL, node, notEqualLabel, cg);
   generateLabelInstruction(JL4, node, lessThanLabel, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultRegister, 1, cg);
   generateLabelInstruction(JMP4, node, endResultLabel, cg);

   generateLabelInstruction(LABEL, node, lessThanLabel, cg);
   generateRegImmInstruction(MOVRegImm4(), node, resultRegister, 2, cg);
   
   deps->stopAddingConditions();

   generateLabelInstruction(LABEL, node, endResultLabel, deps, cg);
   endResultLabel->setEndInternalControlFlow();

   node->setRegister(resultRegister);

   cg->decReferenceCount(src1AddrNode);
   cg->decReferenceCount(src2AddrNode);
   cg->recursivelyDecReferenceCount(lengthNode);

   return resultRegister;
   }

static TR::Register *ConstLenArraycmpLenEvaluator(TR::Node *node, TR::CodeGenerator *cg, uintptrj_t arrLen)
   {
   /* This evaluator will use CMPSQ (64 bit) or CMPSD (32 bit) to compare an array
    * of known length, by generating a sequence of CMPS instructions. Any residuals
    * will be dealt with CMPSD (64 bit), CMPSW and CMPSB
    * The result is similar to the REPArraycmpEvaluator
   */
   TR::Register *resultRegister = cg->allocateRegister(TR_GPR);
   TR::Node *src1AddrNode = node->getChild(0);
   TR::Node *src2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);
   
   int32_t byteLen = TR::Compiler->target.is64Bit() ? 8 : 4;

   TR::Register *src1AddrRegister = cg->gprClobberEvaluate(src1AddrNode, MOVRegReg());
   TR::Register *src2AddrRegister = cg->gprClobberEvaluate(src2AddrNode, MOVRegReg());
   TR::Register *arrayLenRegister = cg->gprClobberEvaluate(lengthNode, MOVRegReg());

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *residualLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *notEqualLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *backtrackLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *equalLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *endResultLabel = generateLabelSymbol(cg);

   generateLabelInstruction(LABEL, node, startLabel, cg);
   startLabel->setStartInternalControlFlow();

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 4, cg);
   deps->addPostCondition(src1AddrRegister, TR::RealRegister::esi, cg);
   deps->addPostCondition(src2AddrRegister, TR::RealRegister::edi, cg);
   deps->addPostCondition(arrayLenRegister, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(resultRegister, TR::RealRegister::NoReg, cg);

   //Setup
   generateRegRegInstruction(MOVRegReg(), node, resultRegister, src1AddrRegister, cg);   

   //Use CMPSQ/CMPSD to compare the first N bytes of the array that are a multiple of 8/4
   while (arrLen >= byteLen )
      {
      arrLen -= byteLen ;
      generateInstruction(TR::Compiler->target.is64Bit() ? CMPSQ : CMPSD, node, deps, cg);
      generateLabelInstruction(JNE4, node, backtrackLabel, cg);
      }

   //Residual
   while (arrLen > 0)
      {
         arrLen--;
         generateInstruction(CMPSB, node, deps, cg);
         generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }

   //Equal
   generateRegRegInstruction(MOVRegReg(), node, resultRegister, arrayLenRegister, cg);   
   generateLabelInstruction(JMP4, node, endResultLabel, cg);

   //Inequality found, figure out which byte
   generateLabelInstruction(LABEL, node, backtrackLabel, cg);
   generateRegImmInstruction(SUBRegImm4(), node, src1AddrRegister, byteLen , cg);
   generateRegImmInstruction(SUBRegImm4(), node, src2AddrRegister, byteLen , cg);

   arrLen = byteLen ;
   while (arrLen > 0)
      {
         arrLen--;
         generateInstruction(CMPSB, node, deps, cg);
         generateLabelInstruction(JNE4, node, notEqualLabel, cg);
      }

   //NotEqual
   generateLabelInstruction(LABEL, node, notEqualLabel, cg);
   generateRegInstruction(TR::Compiler->target.is64Bit() ? DEC8Reg : DEC4Reg, node, src1AddrRegister, cg);
   generateRegRegInstruction(SUBRegReg(), node, src1AddrRegister, resultRegister, cg);
   generateRegRegInstruction(MOVRegReg(), node, resultRegister, src1AddrRegister, cg);   
   generateLabelInstruction(JMP4, node, endResultLabel, cg);

   //Result 
   cg->stopUsingRegister(src1AddrRegister);
   cg->stopUsingRegister(src2AddrRegister);
   cg->stopUsingRegister(arrayLenRegister);
   deps->stopAddingConditions();

   generateLabelInstruction(LABEL, node, endResultLabel, deps, cg);
   endResultLabel->setEndInternalControlFlow();

   node->setRegister(resultRegister);

   cg->decReferenceCount(src1AddrNode);
   cg->decReferenceCount(src2AddrNode );
   cg->decReferenceCount(lengthNode );

   return resultRegister;
   }

TR::Register *
OMR::X86::TreeEvaluator::arraycmpEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Register *resultRegister = NULL;

   TR::Node *byteLenNode = node->getChild(2);

   static bool repArrayCmpOpt = (feGetEnv("TR_NewArrayCompareOpt") != NULL);
   static bool constLenArrayCmpOpt = (feGetEnv("TR_NoConstLenArrayCompareOpt") == NULL);
   static char* maxLengthOfConstArrayStr = feGetEnv("TR_MaxLengthOfConstArray");
   int32_t maxLengthOfConstArray = maxLengthOfConstArrayStr ? atoi(maxLengthOfConstArrayStr) : 32;
   uintptrj_t arrLen;

   //Array Rep
   //
   if (repArrayCmpOpt)
      {
      if (byteLenNode->getOpCode().isLoadConst() && constLenArrayCmpOpt)
         {
         arrLen = TR::TreeEvaluator::integerConstNodeValue(byteLenNode, cg);
         if (arrLen <= maxLengthOfConstArray)
            return node->isArrayCmpLen() ?  ConstLenArraycmpLenEvaluator(node, cg, arrLen) : ConstLenArraycmpEvaluator(node, cg, arrLen);
         }
      return node->isArrayCmpLen() ? REPArraycmpLenEvaluator(node, cg) : REPArraycmpEvaluator(node, cg);
      }
   // Exploit SSE4.2 SIMD instructions if they're available.
   //
   if (cg->getX86ProcessorInfo().supportsSSE2())
      {
      return node->isArrayCmpLen() ? TR::TreeEvaluator::SSE2ArraycmpLenEvaluator(node, cg) : TR::TreeEvaluator::SSE2ArraycmpEvaluator(node, cg);
      }

   TR::Node *src1AddrNode = node->getChild(0);
   TR::Node *src2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   TR::Register *byteLenRegister = NULL;
   TR::Register *condReg = NULL;
   TR::Register *byteLenRemainingRegister = NULL;
   TR::Register *condResultReg = NULL;

   TR::LabelSymbol *startLabel = NULL;
   TR::LabelSymbol *loopStartLabel = NULL;
   TR::LabelSymbol *residueStartLabel = NULL;
   TR::LabelSymbol *residueLoopStartLabel = NULL;
   TR::LabelSymbol *residueEndLabel = NULL;
   TR::LabelSymbol *resultLabel = NULL;
   TR::LabelSymbol *result2Label = NULL;
   TR::LabelSymbol *result3Label = NULL;

   int32_t byteLen = 4;
   if (TR::Compiler->target.is64Bit())
      byteLen = 8;

   TR::Register *src1AddrReg = TR::TreeEvaluator::intOrLongClobberEvaluate(src1AddrNode, TR::TreeEvaluator::getNodeIs64Bit(src1AddrNode, cg), cg);
   TR::Register *src2AddrReg = TR::TreeEvaluator::intOrLongClobberEvaluate(src2AddrNode, TR::TreeEvaluator::getNodeIs64Bit(src2AddrNode, cg), cg);
   byteLenRegister  = TR::TreeEvaluator::intOrLongClobberEvaluate(byteLenNode,  TR::TreeEvaluator::getNodeIs64Bit(byteLenNode, cg), cg);

   byteLenRemainingRegister = cg->allocateRegister(TR_GPR);

   generateRegRegInstruction(MOVRegReg(), node, byteLenRemainingRegister, byteLenRegister, cg);

   startLabel = generateLabelSymbol(cg);
   generateLabelInstruction(LABEL, node, startLabel, cg);

   residueStartLabel = generateLabelSymbol(cg);
   residueLoopStartLabel = generateLabelSymbol(cg);

   generateRegRegInstruction(SUBRegReg(), node, src2AddrReg, src1AddrReg, cg);

   generateRegImmInstruction(CMPRegImms(), node, byteLenRemainingRegister, byteLen, cg);
   generateLabelInstruction(JL4, node, residueLoopStartLabel, cg);

   generateRegImmInstruction(ADDRegImms(), node, byteLenRemainingRegister, -byteLen, cg);

   loopStartLabel = generateLabelSymbol(cg);
   generateLabelInstruction(LABEL, node, loopStartLabel, cg);

   residueEndLabel = generateLabelSymbol(cg);
   resultLabel = generateLabelSymbol(cg);
   result2Label = generateLabelSymbol(cg);
   result3Label = generateLabelSymbol(cg);
   startLabel->setStartInternalControlFlow();

   TR::Register *src1Reg = cg->allocateRegister(TR_GPR);
   TR::Register *src2Reg = cg->allocateRegister(TR_GPR);

   generateRegMemInstruction((byteLen == 4) ? L4RegMem : L8RegMem, node, src1Reg,
                             generateX86MemoryReference(src1AddrReg, 0, cg), cg);

   generateRegMemInstruction(CMPRegMem(), node, src1Reg, generateX86MemoryReference(src1AddrReg, src2AddrReg, 0, 0, cg), cg);
   generateLabelInstruction(JNE4, node, residueStartLabel, cg);

   generateRegImmInstruction(ADDRegImms(), node, src1AddrReg, byteLen, cg);
   generateRegImmInstruction(ADDRegImms(), node, byteLenRemainingRegister, -byteLen, cg);

   generateLabelInstruction(JGE4, node, loopStartLabel, cg);

   generateLabelInstruction(LABEL, node, residueStartLabel, cg);
   generateRegImmInstruction(ADDRegImms(), node, byteLenRemainingRegister, byteLen, cg);

   generateRegImmInstruction(CMPRegImms(), node, byteLenRemainingRegister, 0, cg);
   generateLabelInstruction(JE4, node, result3Label, cg);

   generateLabelInstruction(LABEL, node, residueLoopStartLabel, cg);

   generateRegMemInstruction((byteLen == 4) ? MOVZXReg4Mem1 : MOVZXReg8Mem1, node, src1Reg,
                          generateX86MemoryReference(src1AddrReg, 0, cg), cg);
   generateRegMemInstruction((byteLen == 4) ? MOVZXReg4Mem1 : MOVZXReg8Mem1, node, src2Reg,
                          generateX86MemoryReference(src1AddrReg, src2AddrReg, 0, 0, cg), cg);

   generateRegRegInstruction(CMPRegReg(), node, src1Reg, src2Reg, cg);
   generateLabelInstruction(JNE4, node, result3Label, cg);

   generateRegImmInstruction(ADDRegImms(), node, src1AddrReg, 1, cg);
   generateRegImmInstruction(ADDRegImms(), node, byteLenRemainingRegister, -1, cg);
   generateLabelInstruction(JG4, node, residueLoopStartLabel, cg);

   generateLabelInstruction(LABEL, node, result3Label, cg);

   generateRegRegInstruction(ADDRegReg(), node, src2AddrReg, src1AddrReg, cg);

   generateLabelInstruction(LABEL, node, resultLabel, cg);

   if (node->isArrayCmpLen())
      {
      generateRegRegInstruction(MOVRegReg(), node, src1Reg, byteLenRegister, cg);
      generateRegRegInstruction(SUBRegReg(), node, src1Reg, byteLenRemainingRegister, cg);
      resultRegister = src1Reg;
      }
   else
      {
      generateRegImmInstruction(CMPRegImms(), node, byteLenRemainingRegister, 0, cg);
      generateLabelInstruction(JNE4, node, result2Label, cg);
      generateRegImmInstruction(MOVRegImm4(), node, byteLenRemainingRegister, 0, cg);
      generateLabelInstruction(JMP4, node, residueEndLabel, cg);
      generateLabelInstruction(LABEL, node, result2Label, cg);
      generateRegImmInstruction(MOVRegImm4(), node, byteLenRemainingRegister, 1, cg);
      generateRegRegInstruction(CMPRegReg(), node, src1Reg, src2Reg, cg);
      generateLabelInstruction(JL4, node, residueEndLabel, cg);
      generateRegImmInstruction(MOVRegImm4(), node, byteLenRemainingRegister, 2, cg);
      resultRegister = byteLenRemainingRegister;
      }

   TR::RegisterDependencyConditions *dependencies = generateRegisterDependencyConditions((uint8_t)0, 6, cg);
   dependencies->addPostCondition(src1Reg, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(src2Reg, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(src1AddrReg, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(src2AddrReg, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(byteLenRemainingRegister, TR::RealRegister::NoReg, cg);
   dependencies->addPostCondition(byteLenRegister, TR::RealRegister::NoReg, cg);

   generateLabelInstruction(LABEL, node, residueEndLabel, dependencies, cg);
   residueEndLabel->setEndInternalControlFlow();

   node->setRegister(resultRegister);

   if (src1Reg != resultRegister)
      {
      cg->stopUsingRegister(src1Reg);
      }

   cg->stopUsingRegister(src2Reg);
   if ((byteLenRemainingRegister) && (byteLenRemainingRegister != resultRegister))
      {
      cg->stopUsingRegister(byteLenRemainingRegister);
      }

   cg->decReferenceCount(src1AddrNode);
   cg->decReferenceCount(src2AddrNode);
   cg->decReferenceCount(lengthNode);

   cg->stopUsingRegister(src1AddrReg);
   cg->stopUsingRegister(src2AddrReg);
   cg->stopUsingRegister(byteLenRegister);

   return resultRegister;
   }

TR::Register *OMR::X86::TreeEvaluator::SSE2IfArraycmpEvaluator(TR::Node *node, TR::LabelSymbol *targetLabel, bool branchonEqual, TR::CodeGenerator *cg)
   {
   TR::Node *s1AddrNode = node->getChild(0);
   TR::Node *s2AddrNode = node->getChild(1);
   TR::Node *lengthNode = node->getChild(2);

   TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *qwordLoop = generateLabelSymbol(cg);
   TR::LabelSymbol *qwordUnequal = generateLabelSymbol(cg);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);

   TR::Register *s1Reg = cg->gprClobberEvaluate(s1AddrNode, MOVRegReg());
   TR::Register *s2Reg = cg->gprClobberEvaluate(s2AddrNode, MOVRegReg());
   TR::Register *strLenReg = cg->gprClobberEvaluate(lengthNode, MOVRegReg());
   TR::Register *equalTestReg = cg->allocateRegister(TR_GPR);
   TR::Register *xmm1Reg = cg->allocateRegister(TR_FPR);
   TR::Register *xmm2Reg = cg->allocateRegister(TR_FPR);
   TR::Machine *machine = cg->machine();

   startLabel->setStartInternalControlFlow();
   doneLabel->setEndInternalControlFlow();

   // s2 = s2-1
   generateLabelInstruction(LABEL, node, startLabel, cg);
   generateRegRegInstruction(SUBRegReg(), node, s2Reg, s1Reg, cg); // s2 = s2 - s1

   // SSE loop body
   generateLabelInstruction(LABEL, node, qwordLoop, cg);
   generateRegMemInstruction(MOVUPSRegMem, node, xmm2Reg, generateX86MemoryReference(s1Reg, 0, cg), cg);
   generateRegMemInstruction(MOVUPSRegMem, node, xmm1Reg, generateX86MemoryReference(s1Reg, s2Reg, 0, cg), cg);
   generateRegRegInstruction(PCMPEQBRegReg, node, xmm1Reg, xmm2Reg, cg);
   generateRegRegInstruction(PMOVMSKB4RegReg, node, equalTestReg, xmm1Reg, cg);
   generateRegImmInstruction(XOR2RegImm2, node, equalTestReg, 0xffff, cg);

   cg->stopUsingRegister(xmm1Reg);
   cg->stopUsingRegister(xmm2Reg);

   generateLabelInstruction(JNE4, node, qwordUnequal, cg);
   generateRegImmInstruction(SUBRegImms(), node, strLenReg, 0x10, cg);
   generateRegMemInstruction(LEARegMem(), node, s1Reg, generateX86MemoryReference(s1Reg, 0x10, cg), cg);

   cg->stopUsingRegister(s1Reg);
   cg->stopUsingRegister(s2Reg);

   generateLabelInstruction(JG4, node, qwordLoop, cg);
   if (branchonEqual)
      generateLabelInstruction(JMP4, node, targetLabel, cg);
   else
      generateLabelInstruction(JMP4, node, doneLabel, cg);

   // check compare results
   generateLabelInstruction(LABEL, node, qwordUnequal, cg);
   generateRegRegInstruction(BSF2RegReg, node, equalTestReg, equalTestReg, cg);
   generateRegRegInstruction(CMPRegReg(), node, equalTestReg, strLenReg, cg);

   cg->stopUsingRegister(strLenReg);
   cg->stopUsingRegister(equalTestReg);

   if (branchonEqual)
      generateLabelInstruction(JGE4, node, targetLabel, cg);
   else
      generateLabelInstruction(JL4, node, targetLabel, cg);

   TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t) 0, 6, cg);
   deps->addPostCondition(xmm1Reg, TR::RealRegister::xmm1, cg);
   deps->addPostCondition(xmm2Reg, TR::RealRegister::xmm2, cg);
   deps->addPostCondition(s2Reg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(s1Reg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(strLenReg, TR::RealRegister::NoReg, cg);
   deps->addPostCondition(equalTestReg, TR::RealRegister::NoReg, cg);


   generateLabelInstruction(LABEL, node, doneLabel, deps, cg);

   cg->decReferenceCount(s1AddrNode);
   cg->decReferenceCount(s2AddrNode);
   cg->decReferenceCount(lengthNode);

   return NULL;
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

void genCodeToPerformLeftToRightAndBlockConcurrentOpIfNeeded(
   TR::Node *node,
   TR::MemoryReference *memRef,
   TR::Register *vReg,
   TR::Register *tempReg,
   TR::Register *tempReg1,
   TR::Register *tempReg2,
   TR::LabelSymbol * nonLockedOpLabel,
   TR::LabelSymbol *&opDoneLabel,
   TR::RegisterDependencyConditions *&deps,
   uint8_t size,
   TR::CodeGenerator* cg,
   bool isLoad,
   bool genOutOfline,
   bool keepValueRegAlive,
   TR::LabelSymbol *startControlFlowLabel)
   {
   }

void constLengthArrayCopy(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Register *byteSrcReg,
      TR::Register *byteDstReg,
      TR::Node *byteLenNode,
      bool preserveSrcPointer,
      bool preserveDstPointer)
   {

   TR::LabelSymbol *endControlFlowLabel = NULL;
   TR::LabelSymbol *startControlFlowLabel = NULL;
   TR::Register    *temp = NULL, *tempReg2 = NULL;
   TR::Register    *lenReg = NULL;
   TR::Register    *valueReg  = NULL;
   static char    *useSSECopy = feGetEnv("TR_SSECopy");
   static char    *disableConstArrayCopyLoop = feGetEnv("TR_DisableConstArrayCopyLoop");

   uintptrj_t byteLength = TR::TreeEvaluator::integerConstNodeValue(byteLenNode, cg);
   TR::Compilation *comp = cg->comp();
   TR::RegisterDependencyConditions  *deps = NULL;
   TR::LabelSymbol *overlapCopyLabel = NULL;

   // 160-byte copies occur in console workloads dealing with 80-character strings, so
   // we want them to get in here.  For less than 64 bytes, the long-latency
   // strategies will stall a lot.
   //
   if (node->isForwardArrayCopy() && byteLength >= 64 && byteLength <= 160)
      {
      struct TR_FancyConstLengthStrategy
         {
         const char      *_name;
         TR_X86OpCodes    _loadOpCode, _storeOpCode;
         int8_t           _stride;      // log2(size of copy)
         TR_RegisterKinds _registerKind;
         int8_t           _pentium4Latency, _pentiumMLatency, _core2Latency, _opteronLatency;

         int8_t getLatency(TR_X86ProcessorInfo &p)
            {
            return (p.isIntelCore2() ||
                    p.isIntelNehalem() ||
                    p.isIntelWestmere() ||
                    p.isIntelSandyBridge() ||
                    p.isAMD15h()) ? _core2Latency : p.isAMDOpteron()? _opteronLatency : _pentium4Latency;
            }
         };
      static TR_FancyConstLengthStrategy fancyConstLengthStrategies[] =
         {
         { "none" },
         { "4-byte mov", L4RegMem,     S4MemReg,     2, TR_GPR, 3, 3, 3, 4 },
         { "8-byte mov", L8RegMem,     S8MemReg,     3, TR_GPR, 3, 3, 3, 4 }, // Only valid when is64BitTarget
         { "movq",       MOVQRegMem,   MOVQMemReg,   3, TR_FPR, 4, 4, 4, 4 }, // Not the latencies from the proc manuals... beyond 4 regs it doesn't seem to make much difference
         { "movups",     MOVUPSRegMem, MOVUPSMemReg, 4, TR_FPR, 4, 4, 4, 4 },
         };

      // Pick a strategy
      //
      TR_X86ProcessorInfo &p = cg->getX86ProcessorInfo();
      diagnostic("isIntelCore2: %d\n", p.isIntelCore2());
      uint32_t stratNum;
      TR_LiveRegisters *liveGPRs = cg->getLiveRegisters(TR_GPR);
      if (p.isIntelSandyBridge() || p.isIntelWestmere() || p.isAMD15h())
         stratNum = 4;
      else if (TR::Compiler->target.is64Bit() && liveGPRs
         && (liveGPRs->getNumberOfLiveRegisters() < (15-fancyConstLengthStrategies[2].getLatency(p))))
         stratNum = 2;
      else if (p.isIntelCore2() || p.isAMDOpteron() || p.isIntelNehalem())
         stratNum = 3;
      else
         stratNum = 0;

      static char *stratNumStr = feGetEnv("TR_FancyConstLengthStrategy");
      if (stratNumStr)
         stratNum = atoi(stratNumStr);

      TR_FancyConstLengthStrategy *strat = fancyConstLengthStrategies + stratNum;

      int32_t strideMask = (1<<strat->_stride) - 1;
      if ((stratNum > 0) && ((byteLength & strideMask) == 0)
         && performTransformation(comp, "O^O FANCY CONST LENGTH ARRAYCOPY: Copy %d bytes\n", byteLength))
         {
         int32_t numCopies     = byteLength >> strat->_stride;
         int32_t numRegs       = strat->getLatency(p);
         static char *numRegsStr = feGetEnv("TR_FancyConstLengthRegs");
         if (numRegsStr)
            numRegs = atoi(numRegsStr);
         int32_t ramp          = std::min(numCopies, numRegs);
         int32_t numIterations = numCopies / numRegs; // An "iteration" is a sequence of loads and stores using all the registers.  Has nothing to do with loops (though it is true that a loop, if generated, does exactly one iteration).
         int32_t residue       = numCopies - numIterations * numRegs;

         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "   strategy:%d (%s) stride:%d bytes:%d architecture:%x\n", stratNum, strat->_name, strat->_stride, 1 << strat->_stride, p.getX86Architecture());
            traceMsg(comp, "   copies:%d regs:%d ramp:%d iterations:%d residue:%d\n", numCopies, numRegs, ramp, numIterations, residue);
            }

         // Allocate registers
         //
         int32_t regIndex;
         TR::Register *regs[16];
         TR_ASSERT(numRegs <= sizeof(regs)/sizeof(regs[0]), "assertion failure");
         for (regIndex = 0; regIndex < numRegs; regIndex++)
            regs[regIndex] = cg->allocateRegister(strat->_registerKind);

         // Ramp up with loads
         //
         int32_t copyNum;
         for (copyNum = 0; copyNum < ramp; copyNum++)
            {
            regIndex = copyNum % numRegs;
            generateRegMemInstruction(strat->_loadOpCode, node,
               regs[regIndex],
               generateX86MemoryReference(byteSrcReg, copyNum << strat->_stride, cg),
               cg);
            }

         // Start of loop (if we need one)
         //
         // DISABLED becuase it doesn't handle the internal control flow properly yet
         //
         TR::LabelSymbol *loopStart    = NULL;
         TR::Register    *inductionReg = NULL;
         int32_t         inductionIncrement = 0;
         int32_t         inductionEnd = 0;

         // Steady state: store a reg then reload it
         //
         for (copyNum = ramp; copyNum < numCopies; copyNum++)
            {
            regIndex = copyNum % numRegs;
            generateMemRegInstruction(strat->_storeOpCode, node,
               generateX86MemoryReference(byteDstReg, inductionReg, 0, (copyNum-numRegs) << strat->_stride, cg),
               regs[regIndex],
               cg);
            generateRegMemInstruction(strat->_loadOpCode, node,
               regs[regIndex],
               generateX86MemoryReference(byteSrcReg, inductionReg, 0, copyNum << strat->_stride, cg),
               cg);
            }

         // Loop back-edge (if we need one)
         //
         if (inductionReg)
            {
            TR_ASSERT(loopStart, "assertion failure");
            TR_ASSERT(inductionIncrement > 0, "assertion failure");
            TR_X86OpCodes opCode;
            opCode = (inductionIncrement <= 127)? ADDRegImms() : ADDRegImm4();
            generateRegImmInstruction(opCode, node, inductionReg, inductionIncrement, cg);
            opCode = (inductionEnd <= 127)? CMPRegImms() : CMPRegImm4();
            generateRegImmInstruction(opCode, node, inductionReg, inductionEnd, cg);
            cg->stopUsingRegister(inductionReg);
            generateLabelInstruction(JLE4, node, loopStart, cg);
            }

         // Ramp down with stores
         //
         if (ramp < numRegs)
            {
            // Ramp-up was incomplete -- it didn't need all the registers.
            // Just do the stores corresponding to the loads we actually did in the partial ramp-up.
            //
            for (copyNum = 0; copyNum < ramp; copyNum++)
               {
               regIndex = copyNum % numRegs;
               generateMemRegInstruction(strat->_storeOpCode, node,
                  generateX86MemoryReference(byteDstReg, copyNum << strat->_stride, cg),
                  regs[regIndex],
                  cg);
               }
            }
         else
            {
            // We completed the ramp-up, then did zero or more iterations.
            // The idea here is to pretend we're going to keep on copying, but only do the stores.
            //
            for (copyNum = numCopies; copyNum < numCopies+ramp; copyNum++)
               {
               regIndex = copyNum % numRegs;
               generateMemRegInstruction(strat->_storeOpCode, node,
                  generateX86MemoryReference(byteDstReg, inductionReg, 0, (copyNum-numRegs) << strat->_stride, cg),
                  regs[regIndex],
                  cg);
               }
            }

         // Clean up
         for (regIndex = 0; regIndex < numRegs; regIndex++)
            cg->stopUsingRegister(regs[regIndex]);

         cg->decReferenceCount(byteLenNode);

         return;
         }
      }

   if (useSSECopy && byteLength > 48)
      {
      // Check for mutual buffer alignment.
      //
      bool nodeIs64Bit = TR::Compiler->target.is64Bit();
      TR_ASSERT(!nodeIs64Bit, "TODO: adapt for 64-bit");
      TR::MemoryReference  *leaMR = generateX86MemoryReference(byteDstReg, byteSrcReg, 0, cg);
      generateRegRegInstruction(SUBRegReg(nodeIs64Bit), node, byteDstReg, byteSrcReg, cg);
      generateRegImmInstruction(TESTRegImm4(nodeIs64Bit), node, byteDstReg, 0xf, cg);

      // Restore contents of byteDstReg without modifying any flags.
      generateRegMemInstruction(LEARegMem(nodeIs64Bit), node, byteDstReg, leaMR, cg);

      if (!startControlFlowLabel)
         {
         startControlFlowLabel = generateLabelSymbol(cg);
         startControlFlowLabel->setStartInternalControlFlow();
         generateLabelInstruction(LABEL, node, startControlFlowLabel, cg);
         }

      TR::LabelSymbol *stringMoveLabel = generateLabelSymbol(cg);
      generateLabelInstruction(JNE4, node, stringMoveLabel, cg);

      lenReg = cg->evaluate(byteLenNode);

      TR::RegisterDependencyConditions  *sseDeps;
      sseDeps = generateRegisterDependencyConditions((uint8_t)0, 3, cg);
      sseDeps->addPostCondition(byteSrcReg, TR::RealRegister::esi, cg);
      sseDeps->addPostCondition(byteDstReg, TR::RealRegister::edi, cg);
      sseDeps->addPostCondition(lenReg, TR::RealRegister::ecx, cg);
      sseDeps->stopAddingConditions();
      TR::X86ImmSymInstruction  *callInstr =
         generateHelperCallInstruction(node, TR_IA32forwardSSEArrayCopyNoAlignCheck, sseDeps, cg);
      if (!endControlFlowLabel)
         {
         endControlFlowLabel = generateLabelSymbol(cg);
         endControlFlowLabel->setEndInternalControlFlow();
         }
      generateLabelInstruction(JMP4, node, endControlFlowLabel, cg);

      generateLabelInstruction(LABEL, node, stringMoveLabel, cg);
      }

   // we have to be really careful with ESP... modifying it is very dangerous
   TR::Register *esp = cg->machine()->getX86RealRegister(TR::RealRegister::esp);
   bool         srcIsEsp = (byteSrcReg == esp);
   bool         dstIsEsp = (byteDstReg == esp);
   TR_ASSERT(!(srcIsEsp && dstIsEsp), "Copying to ESP from ESP is not supported");

   int32_t constWordSize = TR::Compiler->target.is64Bit() ? 8 : 4;
   int64_t wordCount     = (byteLength / constWordSize);
   int32_t residue       = byteLength % constWordSize;
   int64_t numBytesMoved = wordCount * constWordSize;
   bool    srcPtrChanged = true; // assume that the source and destination pointers get changed
   bool    dstPtrChanged = true;

   static char *reportConstArrayCopy = feGetEnv("TR_ReportConstArryCopy");

   if (wordCount <= 3)
      {
      int32_t originalWordCount = wordCount;
      if (!lenReg)
         lenReg = cg->allocateRegister();

      while (wordCount--)
         {
         TR::MemoryReference  * tempSrcMR = generateX86MemoryReference(byteSrcReg, constWordSize*(originalWordCount - wordCount) - constWordSize, cg);
         generateRegMemInstruction(LRegMem(), node, lenReg, tempSrcMR, cg);
         TR::MemoryReference  * tempDstMR = generateX86MemoryReference(byteDstReg, constWordSize*(originalWordCount - wordCount) - constWordSize, cg);
         generateMemRegInstruction(SMemReg(), node, tempDstMR, lenReg, cg);
         }

      srcPtrChanged = false;  // in this case, we didn't change the pointers
      dstPtrChanged = false;
      if (reportConstArrayCopy)
         diagnostic("constArrayCopy: found one inline with bytelength = %d in method %s\n", byteLength, comp->signature());
      }
   else if (!disableConstArrayCopyLoop && wordCount < 64 && (byteSrcReg != byteDstReg)) // word count must be > 1 or pipelined loop below is bad
      {
      TR_ASSERT(wordCount > 1, "Const Arraycopy tried to do pipelined loop with word count <= 1");
      lenReg = TR::TreeEvaluator::loadConstant(byteLenNode, wordCount, TR_RematerializableInt, cg, lenReg);
      if (!temp)
         temp = cg->allocateRegister();

      if (!deps)
         {
         TR::Machine *machine = cg->machine();
         deps = generateRegisterDependencyConditions((uint8_t)0, 4, cg);
         deps->addPostCondition(byteSrcReg, TR::RealRegister::NoReg, cg);
         deps->addPostCondition(byteDstReg, TR::RealRegister::NoReg, cg);
         deps->addPostCondition(lenReg, TR::RealRegister::NoReg, cg);
         deps->addPostCondition(temp, TR::RealRegister::NoReg, cg);
         deps->stopAddingConditions();
         }

      // Basic algorithm:
      // - change dst register to be the offset from the src to the dst
      // - use base+offset to access the destination
      // - after the loop, add the base to the offset to restore the dst pointer

      // ESP is a special case:
      // - We want ESP to always contain a valid stack pointer.
      //   So, if it's the dst register, then we use the dst as the base, and the
      //   source as the offset
      const bool srcIsOffset = (dstIsEsp) ? true : false;

      // calculate the offset
      if (srcIsOffset)
         generateRegRegInstruction(SUBRegReg(), node, byteSrcReg, byteDstReg, cg);
      else
         generateRegRegInstruction(SUBRegReg(), node, byteDstReg, byteSrcReg, cg);

      TR::LabelSymbol * loopHead = generateLabelSymbol(cg);

      if (!startControlFlowLabel)
         loopHead->setStartInternalControlFlow();

      generateAlignmentInstruction(node, 16, cg); // TODO: Derive alignment from CPU cache information
      generateLabelInstruction(LABEL, node, loopHead, cg);

      // get the word from src memory
      TR::MemoryReference  *tempSrcMR;
      if (srcIsOffset)
         tempSrcMR = generateX86MemoryReference(byteDstReg, byteSrcReg, 0, cg);
      else
         tempSrcMR = generateX86MemoryReference(byteSrcReg, 0, cg);
      generateRegMemInstruction(LRegMem(), node, temp, tempSrcMR, cg);

      // and send the word to dst memory
      TR::MemoryReference  *tempDstMR;
      if (srcIsOffset)
         tempDstMR = generateX86MemoryReference(byteDstReg, 0, cg);
      else
         tempDstMR = generateX86MemoryReference(byteDstReg, byteSrcReg, 0, cg);
      generateMemRegInstruction(SMemReg(), node, tempDstMR, temp, cg);

      // increment base ptr, and decrement count
      generateRegImmInstruction(ADDRegImms(), node, (srcIsOffset) ? byteDstReg : byteSrcReg, constWordSize, cg);
      generateRegImmInstruction(SUBRegImms(), node, lenReg, 1, cg);
      generateLabelInstruction(JNE4, node, loopHead,  cg);

      if (!endControlFlowLabel)
         {
         endControlFlowLabel = generateLabelSymbol(cg);
         endControlFlowLabel->setEndInternalControlFlow();
         }

      generateLabelInstruction(LABEL, node, endControlFlowLabel, deps, cg);

      // add base to offset, so that both src and dst point to the end of their arrays
      // and, if we need to preserve the pointer, subtract the # bytes moved
      if (srcIsOffset && preserveSrcPointer)
         {
         generateRegMemInstruction(LEARegMem(), node, byteSrcReg, generateX86MemoryReference(byteDstReg, byteSrcReg, 0, -numBytesMoved, cg), cg);
         srcPtrChanged = false;
         }
      else if (srcIsOffset)
         {
         generateRegRegInstruction(ADDRegReg(), node, byteSrcReg, byteDstReg, cg);
         }
      else if (preserveDstPointer) // dstIsOffset
         {
         generateRegMemInstruction(LEARegMem(), node, byteDstReg, generateX86MemoryReference(byteDstReg, byteSrcReg, 0, -numBytesMoved, cg), cg);
         dstPtrChanged = false;
         }
      else
         {
         generateRegRegInstruction(ADDRegReg(), node, byteDstReg, byteSrcReg, cg);
         }

      if (reportConstArrayCopy)
         diagnostic("constArrayCopy: found one loop with bytelength = %d in method %s\n", byteLength, comp->signature());
      }
   else
      {
      TR_RematerializableTypes type =
         byteLenNode->getOpCode().isLong() ? TR_RematerializableLong : TR_RematerializableInt;
      lenReg = TR::TreeEvaluator::loadConstant(byteLenNode, wordCount, type, cg, lenReg);

      // we can't modify the value of esp. Unless, of course, you want _weird_ data corruption.
      if (srcIsEsp)
         {
         byteSrcReg = cg->allocateRegister();
         generateRegRegInstruction(MOVRegReg(), node, byteSrcReg, esp, cg);
         }
      if (dstIsEsp)
         {
         byteDstReg = cg->allocateRegister();
         generateRegRegInstruction(MOVRegReg(), node, byteDstReg, esp, cg);
         }

      if (!deps)
         {
         deps = generateRegisterDependencyConditions((uint8_t)0, 3, cg);
         deps->addPostCondition(byteSrcReg, TR::RealRegister::esi, cg);
         deps->addPostCondition(byteDstReg, TR::RealRegister::edi, cg);
         deps->addPostCondition(lenReg, TR::RealRegister::ecx, cg);
         deps->stopAddingConditions();
         }

      TR_X86OpCodes op = TR::Compiler->target.is64Bit() ? REPMOVSQ : REPMOVSD;
      generateInstruction(op, node, deps, cg);
      if (srcIsEsp)
         {
         cg->stopUsingRegister(byteSrcReg);
         byteSrcReg = esp;
         srcPtrChanged = false; // we modified the copy, not esp
         }

      if (dstIsEsp)
         {
         cg->stopUsingRegister(byteDstReg);
         byteDstReg = esp;
         dstPtrChanged = false; // we modified the copy, not esp
         }

      if (reportConstArrayCopy)
         diagnostic("constArrayCopy: found one repmovs%s with bytelength = %d in method %s\n",
                     TR::Compiler->target.is64Bit() ? "q" : "d",
                     byteLength, comp->signature());
      }

   cg->decReferenceCount(byteLenNode);

   if (preserveSrcPointer && srcPtrChanged)
      {
      TR_X86OpCodes opCode = (numBytesMoved < 127) ? SUBRegImms() : SUBRegImm4();
      generateRegImmInstruction(opCode, node, byteSrcReg, numBytesMoved, cg);
      srcPtrChanged = false;
      }

   if (preserveDstPointer && dstPtrChanged)
      {
      TR_X86OpCodes opCode = (numBytesMoved < 127) ? SUBRegImms() : SUBRegImm4();
      generateRegImmInstruction(opCode, node, byteDstReg, numBytesMoved, cg);
      dstPtrChanged = false;
      }

   int64_t srcOffset = (srcPtrChanged) ? 0 : numBytesMoved;
   int64_t dstOffset = (dstPtrChanged) ? 0 : numBytesMoved;

   TR::MemoryReference  *memRef;
   TR_ASSERT(residue < 8, "Residue (%d) should be < 8, otherwise its not residue", residue);

   if (residue >= 4)
      {
      if (!lenReg)
         lenReg = cg->allocateRegister();

      TR::MemoryReference  *loadRef = generateX86MemoryReference(byteSrcReg, srcOffset, cg);
      generateRegMemInstruction(L4RegMem, node, lenReg, loadRef, cg);
      TR::MemoryReference  *storeRef = generateX86MemoryReference(byteDstReg, dstOffset, cg);
      generateMemRegInstruction(S4MemReg, node, storeRef, lenReg, cg);

      srcOffset += 4;
      dstOffset += 4;
      residue -= 4;
      }

   if (residue >= 2)
      {
      // 2: mov reg_w, word ptr [esi]
      //    mov word ptr [edi], reg_w
      //
      if (!lenReg)
         lenReg = cg->allocateRegister();

      TR::MemoryReference  *loadRef = generateX86MemoryReference(byteSrcReg, srcOffset, cg);
      generateRegMemInstruction(L2RegMem, node, lenReg, loadRef, cg);
      TR::MemoryReference  *storeRef = generateX86MemoryReference(byteDstReg, dstOffset, cg);
      generateMemRegInstruction(S2MemReg, node, storeRef, lenReg, cg);

      srcOffset += 2;
      dstOffset += 2;
      residue -= 2;
      }

   if (residue >= 1)
      {
      // 1: mov reg_b, byte ptr [esi]
      //    mov byte ptr [edi], reg_b
      //
      if (!lenReg)
         lenReg = cg->allocateRegister();

      TR::MemoryReference  *loadRef = generateX86MemoryReference(byteSrcReg, srcOffset, cg);
      generateRegMemInstruction(L1RegMem, node, lenReg, loadRef, cg);

      TR::MemoryReference  *storeRef = generateX86MemoryReference(byteDstReg, dstOffset, cg);
      generateMemRegInstruction(S1MemReg, node, storeRef, lenReg, cg);

      srcOffset++;
      dstOffset++;
      residue--;
      }

   TR_ASSERT(residue == 0, "Residue should be 0 by now, not %d", residue);

   if (lenReg)
      cg->stopUsingRegister(lenReg);

   if (temp)
      {
      cg->stopUsingRegister(temp);
      }
   if (tempReg2)
      {
      cg->stopUsingRegister(tempReg2);
      }
   }

TR::Register *
OMR::X86::TreeEvaluator::constLengthArrayCopyEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   TR::Node *byteSrcNode, *byteDstNode, *byteLenNode;
   TR::Compilation *comp = cg->comp();
   bool isPrimitiveCopy = (node->getNumChildren() == 3);
   if (isPrimitiveCopy)
      {
      byteSrcNode = node->getChild(0);
      byteDstNode = node->getChild(1);
      byteLenNode = node->getChild(2);
      }
   else
      {
      TR_ASSERT(node->isNoArrayStoreCheckArrayCopy(), "expecting arraycopy not requiring an array store check");
      cg->recursivelyDecReferenceCount(node->getChild(0));

      // We can't avoid evaluating the destination object node if a write
      // barrier is required.
      //
      if (!comp->getOptions()->needWriteBarriers())
         cg->recursivelyDecReferenceCount(node->getChild(1));

      byteSrcNode = node->getChild(2);
      byteDstNode = node->getChild(3);
      byteLenNode = node->getChild(4);
      }

   TR_ASSERT(node->isForwardArrayCopy(), "expecting forward arraycopy");
   TR_ASSERT(byteLenNode->getOpCode().isLoadConst(), "expecting constant length arraycopy");
   uintptrj_t byteLength = TR::TreeEvaluator::integerConstNodeValue(byteLenNode, cg);

   TR::Register *byteSrcReg = cg->evaluate(byteSrcNode);
   TR::Register *byteDstReg = cg->evaluate(byteDstNode);

   bool preserveSrcPointer = (byteSrcNode->getReferenceCount() > 1);
   bool preserveDstPointer = (byteDstNode->getReferenceCount() > 1);

   constLengthArrayCopy(node, cg, byteSrcReg, byteDstReg, byteLenNode, preserveSrcPointer, preserveDstPointer);

   cg->decReferenceCount(byteSrcNode);
   cg->decReferenceCount(byteDstNode);

   return NULL;
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


TR::Register *
OMR::X86::TreeEvaluator::compressStringEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool japaneseMethod)
   {
   TR::Node *srcObjNode, *dstObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg, *dstObjReg, *lengthReg, *startReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4;

   srcObjNode = node->getChild(0);
   dstObjNode = node->getChild(1);
   startNode = node->getChild(2);
   lengthNode = node->getChild(3);

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegAddr(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyRegInteger(startNode, startReg, cg);
   stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyRegInteger(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateRegImmInstruction(ADDRegImms(), node, srcObjReg, hdrSize, cg);
   generateRegImmInstruction(ADDRegImms(), node, dstObjReg, hdrSize, cg);


   // Now that we have all the registers, set up the dependencies
   TR::RegisterDependencyConditions  *dependencies =
      generateRegisterDependencyConditions((uint8_t)0, 6, cg);
   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *dummy = cg->allocateRegister();
   dependencies->addPostCondition(srcObjReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(dstObjReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(startReg, TR::RealRegister::eax, cg);
   dependencies->addPostCondition(resultReg, TR::RealRegister::edx, cg);
   dependencies->addPostCondition(dummy, TR::RealRegister::ebx, cg);
   dependencies->stopAddingConditions();

   TR_RuntimeHelper helper;
   if (TR::Compiler->target.is64Bit())
      helper = japaneseMethod ? TR_AMD64compressStringJ : TR_AMD64compressString;
   else
      helper = japaneseMethod ? TR_IA32compressStringJ : TR_IA32compressString;
   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
     cg->decReferenceCount(node->getChild(i));

   if (stopUsingCopyReg1)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(srcObjReg);
   if (stopUsingCopyReg2)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(dstObjReg);
   if (stopUsingCopyReg3)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(startReg);
   if (stopUsingCopyReg4)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(lengthReg);
   node->setRegister(resultReg);
   return resultReg;
   }


TR::Register *
OMR::X86::TreeEvaluator::compressStringNoCheckEvaluator(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool japaneseMethod)
   {
   TR::Node *srcObjNode, *dstObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg, *dstObjReg, *lengthReg, *startReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4;

   srcObjNode = node->getChild(0);
   dstObjNode = node->getChild(1);
   startNode = node->getChild(2);
   lengthNode = node->getChild(3);

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegAddr(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyRegInteger(startNode, startReg, cg);
   stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyRegInteger(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateRegImmInstruction(ADDRegImms(), node, srcObjReg, hdrSize, cg);
   generateRegImmInstruction(ADDRegImms(), node, dstObjReg, hdrSize, cg);


   // Now that we have all the registers, set up the dependencies
   TR::RegisterDependencyConditions  *dependencies =
      generateRegisterDependencyConditions((uint8_t)0, 5, cg);
   dependencies->addPostCondition(srcObjReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(dstObjReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(startReg, TR::RealRegister::eax, cg);
   TR::Register *dummy = cg->allocateRegister();
   dependencies->addPostCondition(dummy, TR::RealRegister::ebx, cg);
   dependencies->stopAddingConditions();

   TR_RuntimeHelper helper;
   if (TR::Compiler->target.is64Bit())
      helper = japaneseMethod ? TR_AMD64compressStringNoCheckJ : TR_AMD64compressStringNoCheck;
   else
      helper = japaneseMethod ? TR_IA32compressStringNoCheckJ : TR_IA32compressStringNoCheck;

   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
     cg->decReferenceCount(node->getChild(i));

   if (stopUsingCopyReg1)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(srcObjReg);
   if (stopUsingCopyReg2)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(dstObjReg);
   if (stopUsingCopyReg3)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(startReg);
   if (stopUsingCopyReg4)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(lengthReg);
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::andORStringEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *srcObjNode, *startNode, *lengthNode;
   TR::Register *srcObjReg, *lengthReg, *startReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3;

   srcObjNode = node->getChild(0);
   startNode = node->getChild(1);
   lengthNode = node->getChild(2);

   stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegInteger(startNode, startReg, cg);
   stopUsingCopyReg3 = TR::TreeEvaluator::stopUsingCopyRegInteger(lengthNode, lengthReg, cg);

   uintptrj_t hdrSize = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateRegImmInstruction(ADDRegImms(), node, srcObjReg, hdrSize, cg);

   // Now that we have all the registers, set up the dependencies
   TR::RegisterDependencyConditions  *dependencies =
      generateRegisterDependencyConditions((uint8_t)0, 5, cg);
   TR::Register *resultReg = cg->allocateRegister();
   dependencies->addPostCondition(srcObjReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(lengthReg, TR::RealRegister::ecx, cg);
   dependencies->addPostCondition(startReg, TR::RealRegister::eax, cg);
   dependencies->addPostCondition(resultReg, TR::RealRegister::edx, cg);
   TR::Register *dummy = cg->allocateRegister();
   dependencies->addPostCondition(dummy, TR::RealRegister::ebx, cg);
   dependencies->stopAddingConditions();

   TR_RuntimeHelper helper =
      TR::Compiler->target.is64Bit() ? TR_AMD64andORString : TR_IA32andORString;
   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
     cg->decReferenceCount(node->getChild(i));

   if (stopUsingCopyReg1)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(srcObjReg);
   if (stopUsingCopyReg2)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(startReg);
   if (stopUsingCopyReg3)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(lengthReg);
   node->setRegister(resultReg);
   return resultReg;
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
   bool nodeIs64Bit = TR::Compiler->target.is32Bit()? false: TR::TreeEvaluator::getNodeIs64Bit(operand1, cg);
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
         TR_ASSERT(TR::Compiler->target.is64Bit(), "overflowCHK for lmul on 32 bits is not currently supported\n");
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
   // TODO: there is still a chance that the flags might still be avaiable and we could detect it and avoid repeating
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
            TR::Compiler->target.is32Bit() ? addMulAnalyser.longAddAnalyserWithExplicitOperands(node, operand1, operand2) 
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
            TR::Compiler->target.is32Bit() ? subAnalyser.longSubtractAnalyserWithExplicitOperands(node, operand1, operand2) 
                                           : subAnalyser.integerSubtractAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV8RegReg, needsEflags);
            break;
         // mul group
         case TR::imul:
            addMulAnalyser.genericAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV4RegReg);
            break;
         case TR::lmul:
            addMulAnalyser.genericAnalyserWithExplicitOperands(node, operand1, operand2, op, BADIA32Op, MOV8RegReg);
            break;
         }
      }

   cg->setVMThreadRequired(true);
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
   cg->setVMThreadRequired(false);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

extern "C" void *fwdHalfWordCopyTable;
static TR::Register * deprecated_arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Instruction *instr;
   TR::LabelSymbol *snippetLabel;
   TR::RegisterDependencyConditions *dependencies;

   // There are two cases.
   // In the first case we know that a simple memmove or memcpy operation can
   // be done. For this case there are 3 children:
   //    1) The byte source pointer
   //    2) The byte destination pointer
   //    3) The byte count
   //
   // In the second case we must generate run-time tests to see if a simple
   // byte copy can be done or if an element-by-element copy is needed.
   // For this case there are 5 children:
   //    1) The original source object reference
   //    2) The original destination object reference
   //    3) The byte source pointer
   //    4) The byte destination pointer
   //    5) The byte count
   //
   TR::Node *srcNode;
   TR::Node *dstNode;
   TR::Node *byteSrcNode;
   TR::Node *byteDstNode;
   TR::Node *byteLenNode;

   TR::Register *srcReg;
   TR::Register *dstReg;
   TR::Register *byteSrcReg;
   TR::Register *byteDstReg;
   TR::Register *byteLenReg = NULL;

   bool isPrimitiveCopy = (node->getNumChildren() == 3);
   if (isPrimitiveCopy)
      {
      srcNode     = NULL;
      dstNode     = NULL;
      byteSrcNode = node->getChild(0);
      byteDstNode = node->getChild(1);
      byteLenNode = node->getChild(2);
      if (byteSrcNode == byteDstNode)
         {
         // nothing to do
         cg->recursivelyDecReferenceCount(byteSrcNode);
         cg->recursivelyDecReferenceCount(byteSrcNode);
         cg->recursivelyDecReferenceCount(byteLenNode);
         return NULL;
         }
      }
   else
      {
      srcNode     = node->getChild(0);
      dstNode     = node->getChild(1);
      byteSrcNode = node->getChild(2);
      byteDstNode = node->getChild(3);
      byteLenNode = node->getChild(4);
      }

   bool isArrayStoreCheckUnnecessary = isPrimitiveCopy || node->isNoArrayStoreCheckArrayCopy();
   bool shortCopy = true;

   static char *disableConstantLengthArrayCopy = feGetEnv("TR_disableConstantLengthArrayCopy");

   uintptrj_t constantByteLength = TR::TreeEvaluator::integerConstNodeValue(byteLenNode, cg);
   TR::Compilation *comp = cg->comp();

   if (isArrayStoreCheckUnnecessary &&
       (alwaysInlineArrayCopy(cg) || node->isForwardArrayCopy()) &&
       byteLenNode->getOpCode().isLoadConst() &&
       isByteLengthSmallEnough(constantByteLength) &&
       !disableConstantLengthArrayCopy)
      {
      static char * p = feGetEnv("TR_OldConstArrayCopy");
      if (constantByteLength > 28 || !p)
         {
         TR::TreeEvaluator::constLengthArrayCopyEvaluator(node, cg);
         if (!isPrimitiveCopy)
            {
#ifdef J9_PROJECT_SPECIFIC
            TR::TreeEvaluator::generateWrtbarForArrayCopy(node, cg);
#endif

            if (comp->getOptions()->needWriteBarriers())
               cg->decReferenceCount(dstNode);
            }

         return NULL;
         }
      }

   static char *useSSECopyEnv = feGetEnv("TR_SSECopy");
   static char *disableOptSeverCheckForRepeatMove = feGetEnv("TR_disableOptSeverCheckForRepeatMove");
   bool useSSECopy = false;

   if (useSSECopyEnv)
      useSSECopy = true;


   // Use SSE copy for following methods
   TR_OpaqueMethodBlock *caller = node->getOwningMethod();
   TR_ResolvedMethod *m = NULL;
   if (caller)
      {
      m = comp->fe()->createResolvedMethod(cg->trMemory(), caller, node->getSymbolReference()->getOwningMethod(comp));
      }

#ifdef J9_PROJECT_SPECIFIC
   if (m &&
       (m->getRecognizedMethod() == TR::java_util_Arrays_copyOf_byte ||
        m->getRecognizedMethod() == TR::java_io_ByteArrayOutputStream_write ||
        m->getRecognizedMethod() == TR::java_io_ObjectInputStream_BlockDataInputStream_read))
      {
      useSSECopy = true;
      }
#endif

   // disable SSE arraycopy for AMD64 but use it for aggressive liberty mode and for hot compiles
   if (TR::Compiler->target.is64Bit())
      {
      if (comp->getOptLevel() >= hot || disableOptSeverCheckForRepeatMove || !comp->isOptServer())
         useSSECopy = true;
      }
   else if (cg->useSSEForDoublePrecision())
      useSSECopy = true;
   else // 32 bit and useSSEForDoublePrecision is false
      useSSECopy = false;

#ifdef J9_PROJECT_SPECIFIC
   if (isArrayStoreCheckUnnecessary &&
       !useSSECopy &&
       comp->getRecompilationInfo() &&
       (node->isHalfWordElementArrayCopy() || node->isWordElementArrayCopy()))
      {
      TR_ValueInfo *valueInfo = (TR_ValueInfo *)TR_ValueProfiler::getProfiledValueInfo(node, comp);
      if (valueInfo)
         {
         if ((valueInfo->_totalFrequency > 0) &&
             (valueInfo->_frequency1 > (valueInfo->_totalFrequency/2)) &&
             (valueInfo->_value1 > 24))
            {
            shortCopy = false;
            }
         }
      }
#endif

   if (!useSSECopy && (comp->isOptServer() && !disableOptSeverCheckForRepeatMove))
      {
      shortCopy = false;
      }

   // If the byte length node is a mul (converting elements to bytes), then
   // skip the mul, and just use the length in words directly.
   //
   bool byteLenInDWords    = false;
   bool byteLenInWords     = false;
   bool byteLenInHalfWords = false;

   if (isArrayStoreCheckUnnecessary &&
       !shortCopy &&
       node->isForwardArrayCopy() &&
       (node->isWordElementArrayCopy() || node->isHalfWordElementArrayCopy()) &&
       !useSSECopy)
      {
      intptrj_t size = 0;
      if (byteLenNode &&
          byteLenNode->getRegister() == NULL &&
          byteLenNode->getOpCode().isMul() &&
          byteLenNode->getSecondChild()->getOpCode().isLoadConst())
         {
         TR::Node *constNode = byteLenNode->getSecondChild();
         size = TR::TreeEvaluator::integerConstNodeValue(constNode, cg);
         }

      if (size == 8 || size == 4 || size == 2)
         {
         byteLenNode->getFirstChild()->incReferenceCount();
         cg->recursivelyDecReferenceCount(byteLenNode);
         byteLenNode = byteLenNode->getFirstChild();

         if (size == 8)
            byteLenInDWords = true;
         else if (size == 4)
            byteLenInWords = true;
         else
            byteLenInHalfWords = true;
         }
      }

   if (!isArrayStoreCheckUnnecessary)
      {
      return TR::TreeEvaluator::VMarrayStoreCheckArrayCopyEvaluator(node, cg);
      }

   byteLenReg = cg->evaluate(byteLenNode);

   int32_t argSize = 0;

   // Evaluate the derived arguments.
   //
   // Since the derived argument registers are going to be killed over the call,
   // move them into preserved registers if they still have more uses.
   //
   TR::Register *copyByteLenReg = NULL;
   if (byteLenNode->getReferenceCount() > 1)
      {
      copyByteLenReg = cg->allocateRegister();
      generateRegRegInstruction(MOVRegReg(), node, copyByteLenReg, byteLenReg, cg);

      // Kill the copy rather than the original to allow better rematerialisation
      // when the original register comes from a load.
      //
      byteLenReg = copyByteLenReg;
      }

   byteSrcReg = cg->evaluate(byteSrcNode);
   TR::Register *copyByteSrcReg = NULL;
   if (byteSrcNode->getReferenceCount() > 1)
      {
      if (byteSrcReg->containsInternalPointer())
         {
         copyByteSrcReg = cg->allocateRegister();
         copyByteSrcReg->setPinningArrayPointer(byteSrcReg->getPinningArrayPointer());
         copyByteSrcReg->setContainsInternalPointer();
         }
      else
         copyByteSrcReg = cg->allocateCollectedReferenceRegister();

      generateRegRegInstruction(MOVRegReg(), node, copyByteSrcReg, byteSrcReg, cg);

      // Kill the copy rather than the original to allow better rematerialisation
      // when the original register comes from a load.
      //
      byteSrcReg = copyByteSrcReg;
      }

   byteDstReg = cg->evaluate(byteDstNode);
   TR::Register *copyByteDstReg = NULL;
   if (byteDstNode->getReferenceCount() > 1)
      {
      if (byteDstReg->containsInternalPointer())
         {
         copyByteDstReg = cg->allocateRegister();
         copyByteDstReg->setPinningArrayPointer(byteDstReg->getPinningArrayPointer());
         copyByteDstReg->setContainsInternalPointer();
         }
      else
         copyByteDstReg = cg->allocateCollectedReferenceRegister();

      generateRegRegInstruction(MOVRegReg(), node, copyByteDstReg, byteDstReg, cg);

      // Kill the copy rather than the original to allow better rematerialisation
      // when the original register comes from a load.
      //
      byteDstReg = copyByteDstReg;
      }

   // Now that we have all the registers, set up the dependencies
   //
   int32_t numPostDeps = TR::Compiler->target.is64Bit() ? 6 : 4;

   TR_ASSERT(isArrayStoreCheckUnnecessary, "arraystorechk must be necessary");

   if ((shortCopy && node->isForwardArrayCopy() &&
        !node->isWordElementArrayCopy() && cg->useSSEForDoublePrecision()))
      {
      // XMM scratch registers
      //
      numPostDeps += 1;
      }

   if (useSSECopy)
      numPostDeps += 5;

   if (node->isHalfWordElementArrayCopy() && shortCopy && TR::Compiler->target.is64Bit())
      {
      // "hidden" address reg in 64-bit memrefs
      //
      numPostDeps += 1;
      }

   dependencies = generateRegisterDependencyConditions((uint8_t)0, numPostDeps, cg);
   dependencies->addPostCondition(byteSrcReg, TR::RealRegister::esi, cg);
   dependencies->addPostCondition(byteDstReg, TR::RealRegister::edi, cg);
   dependencies->addPostCondition(byteLenReg, TR::RealRegister::ecx, cg);

   TR::Register *xmmScratchReg1 = NULL;
   if (shortCopy && node->isForwardArrayCopy() &&
       !node->isWordElementArrayCopy() && cg->useSSEForDoublePrecision())
      {
      xmmScratchReg1 = cg->allocateRegister(TR_FPR);
      dependencies->addPostCondition(xmmScratchReg1, TR::RealRegister::xmm7, cg);
      }

   // Call the appropriate helper entry point
   //
   TR::DataType dt = node->getArrayCopyElementType();
   uint32_t elementSize;
   if (dt == TR::Address)
      elementSize = TR::Compiler->om.sizeofReferenceField();
   else
      elementSize = TR::Symbol::convertTypeToSize(dt);

   if (node->isForwardArrayCopy())
      {
      TR_RuntimeHelper helper;

      bool generateInlineJumpTable = false;

      if (useSSECopy)
         {
         TR::Register *tempReg = cg->allocateRegister();
         dependencies->addPostCondition(tempReg, TR::RealRegister::eax, cg);

         TR::Register *xmm0Register = cg->allocateRegister(TR_FPR);
         TR::Register *xmm1Register = cg->allocateRegister(TR_FPR);
         TR::Register *xmm2Register = cg->allocateRegister(TR_FPR);
         TR::Register *xmm3Register = cg->allocateRegister(TR_FPR);
         dependencies->addPostCondition(xmm0Register, TR::RealRegister::xmm0, cg);
         dependencies->addPostCondition(xmm1Register, TR::RealRegister::xmm1, cg);
         dependencies->addPostCondition(xmm2Register, TR::RealRegister::xmm2, cg);
         dependencies->addPostCondition(xmm3Register, TR::RealRegister::xmm3, cg);

         dependencies->stopAddingConditions();
         if (TR::Compiler->target.is64Bit())
             generateHelperCallInstruction(node, TR_AMD64SSEforwardArrayCopyAggressive, dependencies, cg);
         else
            {
               generateHelperCallInstruction(node, TR_IA32SSEforwardArrayCopyAggressive, dependencies, cg);
            }
         cg->stopUsingRegister(tempReg);
         cg->stopUsingRegister(xmm0Register);
         cg->stopUsingRegister(xmm1Register);
         cg->stopUsingRegister(xmm2Register);
         cg->stopUsingRegister(xmm3Register);
         }
      else if (elementSize == 8)
         {
         dependencies->stopAddingConditions();
         if (!shortCopy && TR::Compiler->target.is64Bit())
            {
            if (byteLenInDWords == false)
               generateRegImmInstruction(SHRRegImm1(), node, byteLenReg, 3, cg);
            generateInstruction(REPMOVSQ, node, dependencies, cg);
            }
         else if (TR::Compiler->target.is64Bit())
            helper = TR_AMD64forwardWordArrayCopy;
         else
            helper = TR_IA32forwardWordArrayCopy;
         }
      else if (elementSize == 4)
         {
         dependencies->stopAddingConditions();
         if (!shortCopy)
            {
            if (byteLenInWords == false)
               generateRegImmInstruction(SHRRegImm1(), node, byteLenReg, 2, cg);
            generateInstruction(REPMOVSD, node, dependencies, cg);
            }
         else if (TR::Compiler->target.is64Bit())
            helper = TR_AMD64forwardWordArrayCopy;
         else
            helper = TR_IA32forwardWordArrayCopy;
         }
       else if (elementSize == 2)
         {
         if (!shortCopy)
            {
            dependencies->stopAddingConditions();
            if (byteLenInHalfWords == false)
               generateRegImmInstruction(SHRRegImm1(), node, byteLenReg, 1, cg);
            generateInstruction(REPMOVSW, node, dependencies, cg);
            }
         else if (TR::Compiler->target.is64Bit())
            {
            // TODO: need an absolute relocation on the table base address and then AOT will work
            //
            if (!cg->needRelocationsForStatics())
               {
               generateInlineJumpTable = true;
               }

            helper = TR_AMD64forwardHalfWordArrayCopy;
            }
         else if (cg->useSSEForDoublePrecision())
            {
            // TODO: need an absolute relocation on the table base address and then AOT will work
            //
            if (!cg->needRelocationsForStatics())
               {
               generateInlineJumpTable = true;
               }

            helper = TR_IA32SSEforwardHalfWordArrayCopy;
            }
         else
            helper = TR_IA32forwardHalfWordArrayCopy;

         if (!generateInlineJumpTable && shortCopy)
            {
            dependencies->stopAddingConditions();
            }
         }
      else
         { // doing byte array copy or 8 byte reference copy on 64bit
         dependencies->stopAddingConditions();
         TR_X86ProcessorInfo p = cg->getX86ProcessorInfo();
         //TR_ASSERT(shortCopy, "can't use long copy assumption here, code was invalid for 64bit reference copies");
         if (!shortCopy)
            {
            generateInstruction(REPMOVSB, node, dependencies, cg);
            }
         else if (TR::Compiler->target.is64Bit())
            {
            if (p.isAMDOpteron() || p.isAMD15h())
               {
               helper = TR_AMD64forwardArrayCopyAMDOpteron;
               }
            else
               {
               helper = TR_AMD64forwardArrayCopy;
               }
            }
         else if (cg->useSSEForDoublePrecision())
            helper = (p.isAMDOpteron() || p.isAMD15h()) ? TR_IA32SSEforwardArrayCopyAMDOpteron : TR_IA32SSEforwardArrayCopy;
         else
            helper = TR_IA32forwardArrayCopy;
         }

      if (shortCopy && !generateInlineJumpTable && !useSSECopy)
         {
         generateHelperCallInstruction(node, helper, dependencies, cg)->setAdjustsFramePointerBy(-argSize);
         }
      else if (generateInlineJumpTable)
         {
         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "Generating inline halfword jump table : %s\n", comp->signature());

         TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
         TR::LabelSymbol *snippetLabel = generateLabelSymbol(cg);
         TR::LabelSymbol *restartLabel = generateLabelSymbol(cg);

         startLabel->setStartInternalControlFlow();
         restartLabel->setEndInternalControlFlow();

         generateLabelInstruction(LABEL, node, startLabel, cg);

         TR::SymbolReference *helperSymRef = cg->symRefTab()->findOrCreateRuntimeHelper(helper, false, false, false);

         TR::X86HelperCallSnippet *snippet = new (cg->trHeapMemory())
            TR::X86HelperCallSnippet(cg, node, restartLabel, snippetLabel, helperSymRef);

         cg->addSnippet(snippet);

         generateRegImmInstruction(CMP4RegImm4, node, byteLenReg, 48, cg);
         generateLabelInstruction(JG4, node, snippetLabel, cg);

         generateRegMemInstruction(LEARegMem(), node,
                                   byteDstReg,
                                   generateX86MemoryReference(byteDstReg, byteLenReg, 0, cg),
                                   cg);

         generateRegMemInstruction(LEARegMem(), node,
                                   byteSrcReg,
                                   generateX86MemoryReference(byteSrcReg, byteLenReg, 0, cg),
                                   cg);

         int32_t scale = TR::Compiler->target.is64Bit() ? 3 : 2;

         // TODO: need an absolute relocation on the table base address
         //
         TR::MemoryReference *mr = generateX86MemoryReference(NULL, byteLenReg, scale, (intptrj_t)&fwdHalfWordCopyTable, cg);

         if (TR::Compiler->target.is64Bit())
            {
            TR::Register *addressReg = mr->getAddressRegister();
            if (addressReg)
               {
               dependencies->addPostCondition(addressReg, TR::RealRegister::NoReg, cg);
               }
            }

         dependencies->stopAddingConditions();

         generateMemInstruction(CALLMem, node, mr, cg);
         generateLabelInstruction(LABEL, node, restartLabel, dependencies, cg);
         }
      }
   else
      {
      TR::Register *xmm0Register = NULL, *xmm1Register = NULL, *xmm2Register = NULL, *xmm3Register = NULL;
      if (useSSECopy)
         {
         xmm0Register = cg->allocateRegister(TR_FPR);
         xmm1Register = cg->allocateRegister(TR_FPR);
         xmm2Register = cg->allocateRegister(TR_FPR);
         xmm3Register = cg->allocateRegister(TR_FPR);
         dependencies->addPostCondition(xmm0Register, TR::RealRegister::xmm0, cg);
         dependencies->addPostCondition(xmm1Register, TR::RealRegister::xmm1, cg);
         dependencies->addPostCondition(xmm2Register, TR::RealRegister::xmm2, cg);
         dependencies->addPostCondition(xmm3Register, TR::RealRegister::xmm3, cg);
         }

      dependencies->stopAddingConditions();

      TR_RuntimeHelper helper;
      TR::LabelSymbol *helperCallLabel = NULL;
      TR::LabelSymbol *endRepetLabe = NULL;
      if (!shortCopy && !(elementSize == 8 && TR::Compiler->target.is32Bit()))
         {
         // dst = dst - src
         // cmp dst, rcx_in_bytes (always in bytes)
         // lea dst, dst + src
         // jb helper call
         TR::LabelSymbol *startLabel = generateLabelSymbol(cg);
         startLabel->setStartInternalControlFlow();
         generateLabelInstruction(LABEL, node, startLabel, cg);
         generateRegRegInstruction(SUBRegReg(), node, byteDstReg, byteSrcReg, cg);
         generateRegRegInstruction(CMPRegReg(), node, byteDstReg, byteLenReg, cg);
         generateRegMemInstruction(LEARegMem(), node, byteDstReg, generateX86MemoryReference(byteDstReg, byteSrcReg, 0, cg), cg);
         helperCallLabel = generateLabelSymbol(cg);
         endRepetLabe = generateLabelSymbol(cg);
         endRepetLabe->setEndInternalControlFlow();
         generateLabelInstruction(JB4,  node, helperCallLabel, cg);
         }

      if (elementSize == 8)
         {
         if (!shortCopy && TR::Compiler->target.is64Bit())
            {
            generateRegImmInstruction(SHRRegImm1(), node, byteLenReg, 3, cg);
            generateInstruction(REPMOVSQ, node, dependencies, cg);
            }
         helper = TR::Compiler->target.is64Bit() ?
              (useSSECopy) ? TR_AMD64wordArrayCopyAggressive : TR_AMD64wordArrayCopy
              : (useSSECopy) ? TR_IA32wordArrayCopyAggressive : TR_IA32wordArrayCopy;
         }
      else if (elementSize == 4)
         {
         if (!shortCopy)
            {
            generateRegImmInstruction(SHRRegImm1(), node, byteLenReg, 2, cg);
            generateInstruction(REPMOVSD, node, dependencies, cg);
            }
         helper = TR::Compiler->target.is64Bit() ?
              (useSSECopy) ? TR_AMD64wordArrayCopyAggressive : TR_AMD64wordArrayCopy
              : (useSSECopy) ? TR_IA32wordArrayCopyAggressive : TR_IA32wordArrayCopy;
         }
      else if (elementSize == 2)
         {
         if (!shortCopy)
            {
            generateRegImmInstruction(SHRRegImm1(), node, byteLenReg, 1, cg);
            generateInstruction(REPMOVSW, node, dependencies, cg);
            }
         helper = TR::Compiler->target.is64Bit() ? (useSSECopy) ? TR_AMD64halfWordArrayCopyAggressive : TR_AMD64halfWordArrayCopy
              : (useSSECopy) ? TR_IA32halfWordArrayCopyAggressive : TR_IA32halfWordArrayCopy;
         }
      else
         {
         if (!shortCopy)
            {
            generateInstruction(REPMOVSB, node, dependencies, cg);
            }
         helper = TR::Compiler->target.is64Bit() ? (false ? TR_AMD64BCarrayCopy :
                                         (useSSECopy) ? TR_AMD64arrayCopyAggressive : TR_AMD64arrayCopy)
                                      :  (useSSECopy) ? TR_IA32arrayCopyAggressive : TR_IA32arrayCopy;
         }

      TR_ASSERT((helperCallLabel != NULL) == (!shortCopy && !(elementSize == 8 && TR::Compiler->target.is32Bit())),
                "arraycopy evaluator control flow disaster!");
      TR_OutlinedInstructions *outlinedCall = NULL;
      if (helperCallLabel != NULL)
         {
         outlinedCall = new (cg->trHeapMemory()) TR_OutlinedInstructions(helperCallLabel, cg);
         cg->getOutlinedInstructionsList().push_front(outlinedCall);
         outlinedCall->swapInstructionListsWithCompilation();
         generateLabelInstruction(LABEL, node, helperCallLabel, cg);
         }

      generateHelperCallInstruction(node, helper, dependencies, cg)->setAdjustsFramePointerBy(-argSize);


      if (helperCallLabel != NULL)
         {
         generateLabelInstruction(JMP4, node, endRepetLabe, cg);
         outlinedCall->swapInstructionListsWithCompilation();
         generateLabelInstruction(LABEL,  node, endRepetLabe, dependencies, cg);
         }

      if (useSSECopy)
         {
         cg->stopUsingRegister(xmm0Register);
         cg->stopUsingRegister(xmm1Register);
         cg->stopUsingRegister(xmm2Register);
         cg->stopUsingRegister(xmm3Register);
         }
      }

   if (!isPrimitiveCopy)
      {
#ifdef J9_PROJECT_SPECIFIC
      TR::TreeEvaluator::generateWrtbarForArrayCopy(node, cg);
#endif
      cg->decReferenceCount(srcNode);
      cg->decReferenceCount(dstNode);
      }

   cg->decReferenceCount(byteSrcNode);
   cg->decReferenceCount(byteDstNode);
   cg->decReferenceCount(byteLenNode);

   // If we made copies of the derived registers, these copies are now dead. At this point,
   // copy{ByteSrc,ByteDst,ByteLen,Dst}Reg == {byteSrc,byteDst,byteLen,dst}Reg, respectively.
   //
   if (copyByteSrcReg)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(byteSrcReg);
   if (copyByteDstReg)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(byteDstReg);
   if (copyByteLenReg)
      cg->getLiveRegisters(TR_GPR)->registerIsDead(byteLenReg);

   if (xmmScratchReg1)
      cg->getLiveRegisters(TR_FPR)->registerIsDead(xmmScratchReg1);

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

      TR_OutlinedInstructions* backwardPath = new (cg->trHeapMemory()) TR_OutlinedInstructions(backwardLabel, cg);
      cg->getOutlinedInstructionsList().push_front(backwardPath);
      backwardPath->swapInstructionListsWithCompilation();
      generateLabelInstruction(LABEL, node, backwardLabel, cg);
      generateRegMemInstruction(LEARegMem(), node, srcReg, generateX86MemoryReference(srcReg, sizeReg, 0, -(intptr_t)elementSize, cg), cg);
      generateRegMemInstruction(LEARegMem(), node, dstReg, generateX86MemoryReference(dstReg, sizeReg, 0, -(intptr_t)elementSize, cg), cg);
      generateInstruction(STD, node, cg);
      generateRepMovsInstruction(repmovs, node, sizeReg, NULL, cg);
      generateInstruction(CLD, node, cg);
      generateLabelInstruction(JMP4, node, mainEndLabel, cg);
      backwardPath->swapInstructionListsWithCompilation();

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
 *     The srouce array address register
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
 *     The srouce array address register
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
   static bool useNewArraycopy = feGetEnv("TR_UseNewArraycopy");
   if (useNewArraycopy == NULL)
      {
      return deprecated_arraycopyEvaluator(node, cg);
      }
   if (node->isReferenceArrayCopy() && !node->isNoArrayStoreCheckArrayCopy())
      {
      return TR::TreeEvaluator::VMarrayStoreCheckArrayCopyEvaluator(node, cg);
      }
   // There are two cases.
   // In the first case we know that a simple memmove or memcpy operation can
   // be done. For this case there are 3 children:
   //    1) The byte source pointer
   //    2) The byte destination pointer
   //    3) The byte count
   //
   // In the second case we must generate run-time tests to see if a simple
   // byte copy can be done or if an element-by-element copy is needed.
   // For this case there are 5 children:
   //    1) The original source object reference
   //    2) The original destination object reference
   //    3) The byte source pointer
   //    4) The byte destination pointer
   //    5) The byte count
   //

   // ALL nodes need be evaluated to comply argument evaluation order;
   // since size node is the last node, its evaluation can be delayed for further optimization
   TR::Node* sizeNode = node->getLastChild();
   for (int32_t i = 0; i < node->getNumChildren()-1; i++)
      {
      cg->evaluate(node->getChild(i));
      }

   TR::Register* srcReg = cg->allocateRegister();
   TR::Register* dstReg = cg->allocateRegister();
   generateRegRegInstruction(MOVRegReg(), node, srcReg, node->getChild(node->getNumChildren() - 3)->getRegister(), cg);
   generateRegRegInstruction(MOVRegReg(), node, dstReg, node->getChild(node->getNumChildren() - 2)->getRegister(), cg);

   TR::DataType dt = node->getArrayCopyElementType();
   uint32_t elementSize = 1;
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
   if (sizeNode->getOpCode().isLoadConst() && TR::Compiler->target.is64Bit() && optimizeForConstantLengthArrayCopy)
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
      TR::Register* sizeReg = TR::TreeEvaluator::intOrLongClobberEvaluate(sizeNode, TR::TreeEvaluator::getNodeIs64Bit(sizeNode, cg), cg);
      if (TR::Compiler->target.is64Bit() && !TR::TreeEvaluator::getNodeIs64Bit(sizeNode, cg))
         {
         generateRegRegInstruction(MOVZXReg8Reg4, node, sizeReg, sizeReg, cg);
         }
      if (elementSize == 8 && TR::Compiler->target.is32Bit())
         {
         arrayCopy64BitPrimitiveOnIA32(node, dstReg, srcReg, sizeReg, cg);
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
#ifdef J9_PROJECT_SPECIFIC
   if (node->isReferenceArrayCopy())
      {
      TR::TreeEvaluator::generateWrtbarForArrayCopy(node, cg);
      }
#endif
   for (int32_t i = 0; i < node->getNumChildren()-1; i++)
      {
      cg->decReferenceCount(node->getChild(i));
      }
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::reverseLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(0, "reverseLoad not implemented yet for this platform");
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::reverseStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(0, "reverseStore not implemented yet for this platform");
   return NULL;
   }


TR::Register *OMR::X86::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(0, "arraytranslateAndTest not implemented yet for this platform");
   return NULL;
   }

TR::Register *OMR::X86::TreeEvaluator::arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   //
   // tree looks as follows:
   // arraytranslate
   //    input ptr
   //    output ptr
   //    translation table (not used)
   //    terminal character (not used when src is byte and dest is word, otherwise, it's a mask)
   //    input length (in elements)
   //    stopping char (not used for X)
   // Number of elements translated is returned
   //

   //sourceByte == true iff the source operand is a byte array
   bool sourceByte = node->isSourceByteArrayTranslate();

   TR::Register *srcPtrReg, *dstPtrReg, *transTableReg, *termCharReg, *lengthReg;
   bool stopUsingCopyReg1 = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(0), srcPtrReg, cg);
   bool stopUsingCopyReg2 = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(1), dstPtrReg, cg);
   bool stopUsingCopyReg4 = false;
   if (!sourceByte)
      {
      stopUsingCopyReg4 = TR::TreeEvaluator::stopUsingCopyRegInteger(node->getChild(3), termCharReg, cg);
      }
   
   bool stopUsingCopyReg5 = TR::TreeEvaluator::stopUsingCopyRegInteger(node->getChild(4), lengthReg, cg);
   TR::Register *resultReg = cg->allocateRegister();
   TR::Register *dummy1 = cg->allocateRegister();
   TR::Register *dummy2 = cg->allocateRegister(TR_FPR);
   TR::Register *dummy3 = cg->allocateRegister(TR_FPR);
   TR::Register *dummy4 = cg->allocateRegister(TR_FPR);

   bool arraytranslateOT = false;
   if (sourceByte && (node->getChild(3)->getOpCodeValue() == TR::iconst) && (node->getChild(3)->getInt() == 0))
      arraytranslateOT = true;

   int noOfDependencies = (sourceByte)? 8 : 9;

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
         helper = TR::Compiler->target.is64Bit() ? TR_AMD64arrayTranslateTROT : TR_IA32arrayTranslateTROT;
      else
         helper = TR::Compiler->target.is64Bit() ? TR_AMD64arrayTranslateTROTNoBreak : TR_IA32arrayTranslateTROTNoBreak;
      }
   else
      {
      TR_ASSERT(node->isTargetByteArrayTranslate(), "Both source and target are word for array translate");
      helper = TR::Compiler->target.is64Bit() ? TR_AMD64arrayTranslateTRTO : TR_IA32arrayTranslateTRTO;
      dependencies->addPostCondition(termCharReg, TR::RealRegister::edx, cg);
      }
   dependencies->stopAddingConditions();
   generateHelperCallInstruction(node, helper, dependencies, cg);
   cg->stopUsingRegister(dummy1);
   cg->stopUsingRegister(dummy2);
   cg->stopUsingRegister(dummy3);
   cg->stopUsingRegister(dummy4);

   cg->decReferenceCount(node->getChild(0));               // Input
   cg->decReferenceCount(node->getChild(1));               // Output
   cg->recursivelyDecReferenceCount(node->getChild(2));    // Translate table (not used)
   if (sourceByte)
      cg->recursivelyDecReferenceCount(node->getChild(3)); // Terminal char (not used)
   else
      cg->decReferenceCount(node->getChild(3));            // Terminal char (used)
   cg->decReferenceCount(node->getChild(4));               // Length
   cg->recursivelyDecReferenceCount(node->getChild(5));    // Stopping char (not used)

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

#ifdef J9_PROJECT_SPECIFIC
TR::Register *OMR::X86::TreeEvaluator::encodeUTF16Evaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // tree looks like:
   // icall com.ibm.jit.JITHelpers.encodeUTF16{Big,Little}()
   //    input ptr
   //    output ptr
   //    input length (in elements)
   // Number of elements translated is returned

   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();
   bool bigEndian = symbol->getRecognizedMethod() == TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big;

   // Set up register dependencies
   const int gprClobberCount = 2;
   const int maxFprClobberCount = 5;
   const int fprClobberCount = bigEndian ? 5 : 4; // xmm4 only needed for big-endian
   TR::Register *srcPtrReg, *dstPtrReg, *lengthReg, *resultReg;
   TR::Register *gprClobbers[gprClobberCount], *fprClobbers[maxFprClobberCount];
   bool killSrc = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(0), srcPtrReg, cg);
   bool killDst = TR::TreeEvaluator::stopUsingCopyRegAddr(node->getChild(1), dstPtrReg, cg);
   bool killLen = TR::TreeEvaluator::stopUsingCopyRegInteger(node->getChild(2), lengthReg, cg);
   resultReg = cg->allocateRegister();
   for (int i = 0; i < gprClobberCount; i++)
      gprClobbers[i] = cg->allocateRegister();
   for (int i = 0; i < fprClobberCount; i++)
      fprClobbers[i] = cg->allocateRegister(TR_FPR);

   int depCount = 11;
   TR::RegisterDependencyConditions *deps =
      generateRegisterDependencyConditions((uint8_t)0, depCount, cg);

   deps->addPostCondition(srcPtrReg, TR::RealRegister::esi, cg);
   deps->addPostCondition(dstPtrReg, TR::RealRegister::edi, cg);
   deps->addPostCondition(lengthReg, TR::RealRegister::edx, cg);
   deps->addPostCondition(resultReg, TR::RealRegister::eax, cg);

   deps->addPostCondition(gprClobbers[0], TR::RealRegister::ecx, cg);
   deps->addPostCondition(gprClobbers[1], TR::RealRegister::ebx, cg);

   deps->addPostCondition(fprClobbers[0], TR::RealRegister::xmm0, cg);
   deps->addPostCondition(fprClobbers[1], TR::RealRegister::xmm1, cg);
   deps->addPostCondition(fprClobbers[2], TR::RealRegister::xmm2, cg);
   deps->addPostCondition(fprClobbers[3], TR::RealRegister::xmm3, cg);
   if (bigEndian)
      deps->addPostCondition(fprClobbers[4], TR::RealRegister::xmm4, cg);

   deps->stopAddingConditions();

   // Generate helper call
   TR_RuntimeHelper helper;
   if (TR::Compiler->target.is64Bit())
      helper = bigEndian ? TR_AMD64encodeUTF16Big : TR_AMD64encodeUTF16Little;
   else
      helper = bigEndian ? TR_IA32encodeUTF16Big : TR_IA32encodeUTF16Little;

   generateHelperCallInstruction(node, helper, deps, cg);

   // Free up registers
   for (int i = 0; i < gprClobberCount; i++)
      cg->stopUsingRegister(gprClobbers[i]);
   for (int i = 0; i < fprClobberCount; i++)
      cg->stopUsingRegister(fprClobbers[i]);

   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      cg->decReferenceCount(node->getChild(i));

   TR_LiveRegisters *liveRegs = cg->getLiveRegisters(TR_GPR);
   if (killSrc)
      liveRegs->registerIsDead(srcPtrReg);
   if (killDst)
      liveRegs->registerIsDead(dstPtrReg);
   if (killLen)
      liveRegs->registerIsDead(lengthReg);

   node->setRegister(resultReg);
   return resultReg;
   }
#endif

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

static void arraySetToZeroForShortConstantArrays(TR::Node* node, TR::Register* addressReg, uintptrj_t size, TR::CodeGenerator* cg)
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

static void arraySetForShortConstantArrays(TR::Node* node, uint8_t elementSize, TR::Register* addressReg, TR::Register* valueReg, uintptrj_t size, TR::CodeGenerator* cg)
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

   TR::Register* addressReg = TR::TreeEvaluator::intOrLongClobberEvaluate(addressNode, TR::Compiler->target.is64Bit(), cg);
   uintptrj_t size;
   bool isSizeConst = false;
   bool isValueZero = false;

   bool isShortConstantArray = false;
   bool isShortConstantArrayWithZero = false;

   static bool isConstArraysetEnabled = (NULL == feGetEnv("TR_DisableConstArrayset"));
   if (isConstArraysetEnabled && cg->getX86ProcessorInfo().supportsSSSE3() && TR::Compiler->target.is64Bit())
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
      if (TR::Compiler->target.is64Bit() && !TR::TreeEvaluator::getNodeIs64Bit(sizeNode, cg))
         {
         generateRegRegInstruction(MOVZXReg8Reg4, node, sizeReg, sizeReg, cg);
         }
      if (elementSize == 8 && TR::Compiler->target.is32Bit())
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

bool OMR::X86::TreeEvaluator::constNodeValueIs32BitSigned(TR::Node *node, intptrj_t *value, TR::CodeGenerator *cg)
   {
   *value = TR::TreeEvaluator::integerConstNodeValue(node, cg);
   if (TR::Compiler->target.is64Bit())
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
   TR_ASSERT(TR::Compiler->target.is64Bit() || node->getSize() <= 4, "64-bit nodes on 32-bit platforms shouldn't use getNodeIs64Bit");
   return TR::Compiler->target.is64Bit() && node->getSize() > 4;
   }

intptrj_t OMR::X86::TreeEvaluator::integerConstNodeValue(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (TR::TreeEvaluator::getNodeIs64Bit(node, cg))
      {
      return (intptrj_t)node->getLongInt(); // Cast to satisfy 32-bit compilers, even though they never take this path
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

      generateRegRegInstruction(SQRTSFRegReg, node, targetRegister, opRegister, cg);
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

TR::Register* OMR::X86::TreeEvaluator::performSimpleAtomicMemoryUpdate(TR::Node* node, int8_t size, TR_X86OpCodes op, TR::CodeGenerator* cg)
   {
   // There are two versions of native exchange helpers:
   //     2 nodes: exchange with [address]
   //     3 nodes: exchange with [address+offset]
   TR::Register* object = cg->evaluate(node->getFirstChild());
   TR::Register* offset = node->getNumChildren() == 2 ? NULL : cg->evaluate(node->getSecondChild());
   TR::Register* value = TR::TreeEvaluator::intOrLongClobberEvaluate(node->getLastChild(), size == 8, cg);
   // Assume that the offset is positive and not pathologically large (i.e., > 2^31).
   if (offset && TR::Compiler->target.is32Bit() && size == 8)
      {
      offset = offset->getLowOrder();
      }

   generateMemRegInstruction(op, node, generateX86MemoryReference(object, offset, 0, cg), value, cg);

   node->setRegister(value);

   // Clean up children nodes
   for (uint16_t i = 0; i < node->getNumChildren(); i++)
      {
      cg->decReferenceCount(node->getChild(i));
      }
   return value;
   }

// TR::icall, TR::acall, TR::lcall, TR::fcall, TR::dcall, TR::call handled by directCallEvaluator
TR::Register *OMR::X86::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   static bool useJapaneseCompression = (feGetEnv("TR_JapaneseComp") != NULL);
   static bool disableECCP256Multiply = (feGetEnv("TR_disableECCP256Multiply") != NULL);
   static bool disableECCP256Mod = (feGetEnv("TR_disableECCP256Mod") != NULL);
   static bool disableECCP256AESCBC = (feGetEnv("TR_disableECCP256AESCBC") != NULL);

   TR::Compilation *comp = cg->comp();
   TR::SymbolReference* SymRef = node->getSymbolReference();
   if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::singlePrecisionSQRTSymbol))
      {
      return inlineSinglePrecisionSQRT(node, cg);
      }
   if (SymRef && SymRef->getSymbol()->castToMethodSymbol()->isInlinedByCG())
      {
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicAdd32BitSymbol))
         {
         return TR::TreeEvaluator::performSimpleAtomicMemoryUpdate(node, 4, LADD4MemReg, cg);
         }
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicAdd64BitSymbol))
         {
         return TR::TreeEvaluator::performSimpleAtomicMemoryUpdate(node, 8, LADD8MemReg, cg);
         }
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicFetchAndAdd32BitSymbol))
         {
         return TR::TreeEvaluator::performSimpleAtomicMemoryUpdate(node, 4, LXADD4MemReg, cg);
         }
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicFetchAndAdd64BitSymbol))
         {
         return TR::TreeEvaluator::performSimpleAtomicMemoryUpdate(node, 8, LXADD8MemReg, cg);
         }
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicSwap32BitSymbol))
         {
         return TR::TreeEvaluator::performSimpleAtomicMemoryUpdate(node, 4, XCHG4MemReg, cg);
         }
      if (comp->getSymRefTab()->isNonHelper(SymRef, TR::SymbolReferenceTable::atomicSwap64BitSymbol))
         {
         return TR::TreeEvaluator::performSimpleAtomicMemoryUpdate(node, 8, XCHG8MemReg, cg);
         }
      }

   // If the method to be called is marked as an inline method, see if it can
   // actually be generated inline.
   //
   TR::Register     *returnRegister = NULL;
   TR::MethodSymbol *symbol = node->getSymbol()->castToMethodSymbol();

   switch (symbol->getRecognizedMethod())
      {
#ifdef J9_PROJECT_SPECIFIC
      case TR::java_nio_Bits_keepAlive:
         {
         TR_ASSERT(node->getNumChildren() == 1, "keepAlive is assumed to have just one argument");

         // The only purpose of keepAlive is to prevent an otherwise
         // unreachable object from being garbage collected, because we don't
         // want its finalizer to be called too early.  There's no need to
         // generate a full-blown call site just for this purpose.

         TR::Register *valueToKeepAlive = cg->evaluate(node->getFirstChild());

         // In theory, a value could be kept alive on the stack, rather than in
         // a register.  It is unfortunate that the following deps will force
         // the value into a register for no reason.  However, in many common
         // cases, this label will have no effect on the generated code, and
         // will only affect GC maps.
         //
         TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions((uint8_t)1, (uint8_t)1, cg);
         deps->addPreCondition  (valueToKeepAlive, TR::RealRegister::NoReg, cg);
         deps->addPostCondition (valueToKeepAlive, TR::RealRegister::NoReg, cg);
         new (cg->trHeapMemory()) TR::X86LabelInstruction(LABEL, node, generateLabelSymbol(cg), deps, cg);
         cg->decReferenceCount(node->getFirstChild());

         return NULL; // keepAlive has no return value
         }
#endif
      default:
         break;
      }

#ifdef J9_PROJECT_SPECIFIC
   if (cg->enableAESInHardwareTransformations() &&
       (symbol->getRecognizedMethod() == TR::com_ibm_jit_crypto_JITAESCryptInHardware_doAESInHardware ||
        symbol->getRecognizedMethod() == TR::com_ibm_jit_crypto_JITAESCryptInHardware_expandAESKeyInHardware))
      {
      return TR::TreeEvaluator::VMAESHelperEvaluator(node, cg);
      }

   if (symbol->getMandatoryRecognizedMethod() == TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Big ||
       symbol->getMandatoryRecognizedMethod() == TR::com_ibm_jit_JITHelpers_transformedEncodeUTF16Little)
      {
      return TR::TreeEvaluator::encodeUTF16Evaluator(node, cg);
      }

   if (cg->getSupportsBDLLHardwareOverflowCheck() &&
       (symbol->getRecognizedMethod() == TR::java_math_BigDecimal_noLLOverflowAdd ||
        symbol->getRecognizedMethod() == TR::java_math_BigDecimal_noLLOverflowMul))
      {
      // Eat this call as its only here to anchor where a long lookaside overflow check
      // needs to be done.  There should be a TR::icmpeq node following
      // this one where the real overflow check will be inserted.
      //
      cg->recursivelyDecReferenceCount(node->getFirstChild());
      cg->recursivelyDecReferenceCount(node->getSecondChild());
      cg->evaluate(node->getChild(2));
      cg->decReferenceCount(node->getChild(2));
      returnRegister = cg->allocateRegister();
      node->setRegister(returnRegister);
      return returnRegister;
      }

   // If the method to be called is marked as an inline method, see if it can
   // actually be generated inline.
   //
   else if (symbol->isVMInternalNative() || symbol->isJITInternalNative() ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_rotateLeft)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_sqrt)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_StrictMath_sqrt)  ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_max_I) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_min_I) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_max_L) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_min_L) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_L) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_D) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_F) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Math_abs_I) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Long_reverseBytes) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Integer_reverseBytes) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Short_reverseBytes) ||
       (symbol->getRecognizedMethod()==TR::java_lang_Class_isAssignableFrom) ||
       (symbol->getRecognizedMethod()==TR::java_lang_System_nanoTime) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCAS) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCAS) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSet) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSet) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordCASSupported) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordCASSupported) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicMarkableReference_doubleWordSetSupported) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_AtomicStampedReference_doubleWordSetSupported) ||

       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_Fences_orderAccesses) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_Fences_orderReads) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_Fences_orderWrites) ||
       (symbol->getRecognizedMethod()==TR::java_util_concurrent_atomic_Fences_reachabilityFence) ||

       (symbol->getRecognizedMethod()==TR::sun_nio_ch_NativeThread_current) ||
       (symbol->getRecognizedMethod()==TR::sun_misc_Unsafe_copyMemory) ||
       (symbol->getRecognizedMethod()==TR::java_lang_String_hashCodeImplCompressed) ||
       (symbol->getRecognizedMethod()==TR::java_lang_String_hashCodeImplDecompressed)
      )
      {
      if (TR::TreeEvaluator::VMinlineCallEvaluator(node, false, cg))
         returnRegister = node->getRegister();
      else
         returnRegister = TR::TreeEvaluator::performCall(node, false, true, cg);
      }
   else if (symbol->getRecognizedMethod() == TR::java_lang_String_compress)
      {
      return TR::TreeEvaluator::compressStringEvaluator(node, cg, useJapaneseCompression);
      }
   else if (symbol->getRecognizedMethod() == TR::java_lang_String_compressNoCheck)
      {
      return TR::TreeEvaluator::compressStringNoCheckEvaluator(node, cg, useJapaneseCompression);
      }
   else if (symbol->getRecognizedMethod() == TR::java_lang_String_andOR)
      {
      return TR::TreeEvaluator::andORStringEvaluator(node, cg);
      }
   else if (symbol->getRecognizedMethod() == TR::com_ibm_crypto_provider_P256PrimeField_multiply
         && !disableECCP256Multiply && !TR::Compiler->om.canGenerateArraylets())
      {
       return TR::TreeEvaluator::VMP256MultiplyEvaluator(node, cg);
      }
   else if (symbol->getRecognizedMethod() == TR::com_ibm_crypto_provider_P256PrimeField_mod
         && !disableECCP256Mod && !TR::Compiler->om.canGenerateArraylets())
      {
       return TR::TreeEvaluator::VMP256ModEvaluator(node, cg);
      }
   else if ((symbol->getRecognizedMethod() == TR::com_ibm_crypto_provider_AEScryptInHardware_cbcEncrypt)
         && cg->enableAESInHardwareTransformations() && !disableECCP256AESCBC
         && !TR::Compiler->om.canGenerateArraylets() && TR::Compiler->target.is64Bit())
      {
      return TR::TreeEvaluator::VMP256AESCBCEncryptionEvaluator(node,cg);
      }
   else if ((symbol->getRecognizedMethod() == TR::com_ibm_crypto_provider_AEScryptInHardware_cbcDecrypt)
         && cg->enableAESInHardwareTransformations() && !disableECCP256AESCBC
         && !TR::Compiler->om.canGenerateArraylets() && TR::Compiler->target.is64Bit())
      {
       return TR::TreeEvaluator::VMP256AESCBCDecryptionEvaluator(node, cg);
      }
   else
#endif
      {
      returnRegister = TR::TreeEvaluator::performCall(node, false, true, cg);
      }

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

// TR::icalli, TR::acalli, TR::lcalli, TR::fcalli, TR::dcalli, TR::calli handled by directCallEvaluator
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
      assert(0);
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
          (TR::Compiler->om.generateCompressedObjectHeaders() || TR::Compiler->target.is32Bit()) ? type = TR_RematerializableInt : type = TR_RematerializableLong;
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

   if (symRef->isUnresolved() && TR::Compiler->target.is32Bit())
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
   if (TR::Compiler->target.is64Bit() && node->getOpCodeValue()==TR::iRegLoad && performTransformation(comp, "TREE EVALUATION: setUpperBitsAreZero on iRegLoad %s\n", cg->getDebug()->getName(node)))
      globalReg->setUpperBitsAreZero();

   return globalReg;
   }

// also handles TR::aRegStore
TR::Register *OMR::X86::TreeEvaluator::iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);

   if (TR::Compiler->target.is64Bit() && node->getDataType() == TR::Int32)
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

      cg->setVMThreadRequired(true);

      static bool doAlign = (feGetEnv("TR_DoNotAlignLoopEntries") == NULL);
      static bool alwaysAlignLoops = (feGetEnv("TR_AlwaysAlignLoopEntries") != NULL);
      if (doAlign && !block->isCold() && block->firstBlockInLoop() &&
          (comp->getOptLevel() > warm || alwaysAlignLoops))
         {
         generateAlignmentInstruction(node, 16, cg); // TODO: Derive alignment from CPU cache information
         }

      if (comp->getOption(TR_DisableLateEdgeSplitting))
         {
         // Check the cfg edge for the mustRestoreVMThreadRegister attribute. If markked then
         // there can be only one incoming edge.
         TR::CFGEdge *edge = (!block->getPredecessors().empty()) ? block->getPredecessors().front() : NULL;
         if (cg->getSupportsVMThreadGRA() &&
             edge && edge->mustRestoreVMThreadRegister())
            {
            // Add a dummy register dependency on ebp to force a vmThread reload.
            TR::RegisterDependencyConditions  *glRegDeps;
            if (node->getNumChildren() > 0)
               {
               cg->evaluate(node->getFirstChild());
               glRegDeps = generateRegisterDependencyConditions(node->getFirstChild(), cg, 1, &popRegisters);
               }
            else
               glRegDeps = generateRegisterDependencyConditions((uint8_t)0, (uint8_t)1, cg);

            TR::Register *dummyReg = cg->allocateRegister();
            glRegDeps->addPostCondition(dummyReg, TR::RealRegister::ebp, cg);
            glRegDeps->stopAddingConditions();
            inst = new (cg->trHeapMemory()) TR::X86LabelInstruction(LABEL, node, label, glRegDeps, cg);
            cg->stopUsingRegister(dummyReg);
            }
         else
            {
            if (node->getNumChildren() > 0)
               inst = generateLabelInstruction(LABEL, node, label, node->getFirstChild(), &popRegisters, true, cg);
            else
               inst = generateLabelInstruction(LABEL, node, node->getLabel(), true, cg);
            }
         }
      else
         {
         bool needVMThreadDep =
            !performTransformation(comp, "O^O LATE EDGE SPLITTING: Omit ebp dependency for %s node %s\n",
                                     node->getOpCode().getName(), cg->getDebug()->getName(node));
         if (node->getNumChildren() > 0)
            inst = generateLabelInstruction(LABEL, node, label, node->getFirstChild(), &popRegisters, needVMThreadDep, cg);
         else
            inst = generateLabelInstruction(LABEL, node, node->getLabel(), needVMThreadDep, cg);
         }

      if (inst->getDependencyConditions())
         inst->getDependencyConditions()->setMayNeedToPopFPRegisters(true);

      inst->setNeedToClearFPStack(true);

      node->getLabel()->setInstruction(inst);
      block->setFirstInstruction(inst);
      cg->setVMThreadRequired(false);

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
      generateRegImmInstruction(TEST4RegImm4, node, machine->getX86RealRegister(TR::RealRegister::esp), block->getNumber(), cg);
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
      // are initialised properly at the bottom of the block by the GP register
      // assigner.
      //
      TR::Machine *machine = cg->machine();

      if (cg->enableRegisterAssociations() &&
          comp->getAppendInstruction()->getOpCodeValue() != ASSOCREGS)
         {
         machine->createRegisterAssociationDirective(comp->getAppendInstruction());
         }

      bool needVMThreadDep =
         comp->getOption(TR_DisableLateEdgeSplitting) ||
         !performTransformation(comp, "O^O LATE EDGE SPLITTING: Omit ebp dependency for %s node %s\n", node->getOpCode().getName(), cg->getDebug()->getName(node));

      // This label is also used by RegisterDependency to detect the end of a block.
      TR::Instruction *labelInst = NULL;
      if (node->getNumChildren() > 0)
         labelInst = generateLabelInstruction(LABEL, node, generateLabelSymbol(cg), node->getFirstChild(), NULL, needVMThreadDep, cg);
      else
         labelInst = generateLabelInstruction(LABEL, node, generateLabelSymbol(cg), needVMThreadDep, cg);

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
            targetRegister = TR::TreeEvaluator::iloadEvaluator(child, cg);
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
   generateMemRegInstruction(TR::Compiler->target.isSMP() ? LCMPXCHGMemReg(nodeIs64Bit) : CMPXCHGMemReg(nodeIs64Bit), node, memRef, replaceReg, deps, cg);

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
         uintptrj_t offset = secondChild->getInt();
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


void
TR_X86ComputeCC::bitwise32(TR::Node *node, TR::Register *ccReg, TR::Register *target,
                             TR::CodeGenerator *cg)
   {
   generateRegInstruction(SETNE1Reg, node, ccReg, cg);
   target->setCCRegister(ccReg);
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

TR::Register *
OMR::X86::TreeEvaluator::VMarrayStoreCheckArrayCopyEvaluator(TR::Node*, TR::CodeGenerator*)
   {
   return 0;
   }

TR::Register *
OMR::X86::TreeEvaluator::VMAESHelperEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return 0;
   }

bool
OMR::X86::TreeEvaluator::VMinlineCallEvaluator(TR::Node*, bool, TR::CodeGenerator*)
   {
   return false;
   }

TR::Register *
OMR::X86::TreeEvaluator::VMifInstanceOfEvaluator(TR::Node*, TR::CodeGenerator*)
   {
   return 0;
   }

TR::Register *
OMR::X86::TreeEvaluator::VMifArrayCmpEvaluator(TR::Node*, TR::CodeGenerator*)
   {
   return 0;
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
   NumBinaryArithmeticOps
   };
static const TR_X86OpCodes BinaryArithmeticOpCodes[TR::NumOMRTypes][NumBinaryArithmeticOps] =
   {
   //  Invalid,       Add,         Sub,       Mul,         Div
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // NoType
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Int8
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Int16
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Int32
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Int64
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Float
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Double
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Address
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // VectorInt8
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // VectorInt16
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // VectorInt32
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // VectorInt64
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // VectorFloat
   { BADIA32Op, ADDPDRegReg, BADIA32Op, MULPDRegReg, BADIA32Op }, // VectorDouble
   { BADIA32Op, BADIA32Op,   BADIA32Op, BADIA32Op,   BADIA32Op }, // Aggregate
   };
// For ILOpCode that can be translated to single SSE/AVX instructions
TR::Register* OMR::X86::TreeEvaluator::FloatingPointAndVectorBinaryArithmeticEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   auto arithmetic = BinaryArithmeticInvalid;
   switch (node->getOpCodeValue())
      {
      case TR::vadd:
         arithmetic = BinaryArithmeticAdd;
         break;
      case TR::vmul:
         arithmetic = BinaryArithmeticMul;
         break;
      default:
         TR_ASSERT(false, "Unsupported OpCode");
      }
   TR::Node* operandNode0 = node->getChild(0);
   TR::Node* operandNode1 = node->getChild(1);
   TR::Register* operandReg0 = cg->evaluate(operandNode0);
   TR::Register* operandReg1 = cg->evaluate(operandNode1);

   TR::Register* resultReg = cg->allocateRegister(operandReg0->getKind());
   generateRegRegInstruction(MOVDQURegReg, node, resultReg, operandReg0, cg);

   TR_X86OpCodes opCode = BinaryArithmeticOpCodes[node->getDataType()][arithmetic];
   TR_ASSERT(opCode != BADIA32Op, "FloatingPointAndVectorBinaryArithmeticEvaluator: unsupported data type or arithmetic.");
   generateRegRegInstruction(opCode, node, resultReg, operandReg1, cg);

   node->setRegister(resultReg);
   cg->decReferenceCount(operandNode0);
   cg->decReferenceCount(operandNode1);
   return resultReg;
   }
