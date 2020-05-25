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

//On zOS XLC linker can't handle files with same name at link time
//This workaround with pragma is needed. What this does is essentially
//give a different name to the codesection (csect) for this file. So it
//doesn't conflict with another file with same name.
#pragma csect(CODE,"TRZTreeEvalBase#C")
#pragma csect(STATIC,"TRZTreeEvalBase#S")
#pragma csect(TEST,"TRZTreeEvalBase#T")

#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/ConstantDataSnippet.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
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
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/StorageInfo.hpp"
#include "codegen/SystemLinkage.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/S390Evaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/ObjectModel.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/defines.h"
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
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/HashTab.hpp"
#include "infra/List.hpp"
#include "infra/Random.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"
#include "ras/DebugCounter.hpp"
#include "runtime/Runtime.hpp"
#include "z/codegen/BinaryCommutativeAnalyser.hpp"
#include "z/codegen/OpMemToMem.hpp"
#include "z/codegen/S390GenerateInstructions.hpp"
#include "z/codegen/S390HelperCallSnippet.hpp"
#include "z/codegen/S390Instruction.hpp"
#include "z/codegen/S390OutOfLineCodeSection.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "z/codegen/S390Register.hpp"
#endif
#include "z/codegen/TranslateEvaluator.hpp"


#ifdef J9_PROJECT_SPECIFIC
TR::Instruction *
generateS390PackedCompareAndBranchOps(TR::Node * node,
                                      TR::CodeGenerator * cg,
                                      TR::InstOpCode::S390BranchCondition fBranchOpCond,
                                      TR::InstOpCode::S390BranchCondition rBranchOpCond,
                                      TR::InstOpCode::S390BranchCondition &retBranchOpCond,
                                      TR::LabelSymbol *branchTarget = NULL);
#endif

TR::Instruction*
generateLoad32BitConstant(TR::CodeGenerator* cg, TR::Node* node, int32_t value, TR::Register* targetRegister, bool canSetConditionCode, TR::Instruction* cursor, TR::RegisterDependencyConditions* dependencies, TR::Register* literalPoolRegister)
   {
   TR::Symbol *sym = NULL;
   if (node->getOpCode().hasSymbolReference())
      sym = node->getSymbol();
   bool load64bit = node->getType().isInt64() || node->isExtendedTo64BitAtSource();

   if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
      {
      if (load64bit)
         {
         return generateRIInstruction(cg, TR::InstOpCode::LGHI, node, targetRegister, value, cursor);
         }
      else
         {
         // Prefer to generate XR over LHI if possible to save 2 bytes of icache
         if (value == 0 && canSetConditionCode && !cg->getConditionalMovesEvaluationMode())
            {
            return generateRRInstruction(cg, TR::InstOpCode::XR, node, targetRegister, targetRegister, cursor);
            }
         else
            {
            return generateRIInstruction(cg, TR::InstOpCode::LHI, node, targetRegister, value, cursor);
            }
         }
      }
   else if (load64bit && value >= 0 && value <= 65535)
      {
      // Zero-extended halfword
      return generateRIInstruction(cg, TR::InstOpCode::LLILL, node, targetRegister, value, cursor);
      }
   else
      {
      if (sym)
         {
         if (cg->comp()->compileRelocatableCode() && sym->isDebugCounter())
            return generateRegLitRefInstruction(cg, TR::InstOpCode::L, node, targetRegister, value, TR_DebugCounter, dependencies, cursor, literalPoolRegister);
         if (sym->isStatic() && !sym->isClassObject() && !sym->isNotDataAddress())
            return generateRegLitRefInstruction(cg, TR::InstOpCode::L, node, targetRegister, value, TR_DataAddress, dependencies, cursor, literalPoolRegister);
         if (sym->isCountForRecompile())
            return generateRegLitRefInstruction(cg, TR::InstOpCode::L, node, targetRegister, value, TR_GlobalValue, dependencies, cursor, literalPoolRegister);
         }
      }

   if (load64bit)
      {
      // Sign-extended 32-bit
      return generateRILInstruction(cg, TR::InstOpCode::LGFI, node, targetRegister, value, cursor);
      }
   else
      {
      return generateRILInstruction(cg, TR::InstOpCode::IILF, node, targetRegister, value, cursor);
      }
   }

/**
 * Generate code to load a long constant
 */
TR::Instruction *
genLoadLongConstant(TR::CodeGenerator * cg, TR::Node * node, int64_t value, TR::Register * targetRegister, TR::Instruction * cursor,
   TR::RegisterDependencyConditions * cond, TR::Register * base)
   {
   TR::Compilation *comp = cg->comp();

   TR::Symbol *sym = NULL;
   if (node->getOpCode().hasSymbolReference())
      sym = node->getSymbol();

   if (cg->comp()->compileRelocatableCode() && sym && sym->isDebugCounter())
      {
      cursor = generateRegLitRefInstruction(cg, TR::InstOpCode::LG, node, targetRegister, value, TR_DebugCounter, cond, cursor, base);
      }
   else if (comp->compileRelocatableCode() && sym && (sym->isCountForRecompile()))
      {
      TR::Instruction * temp = cursor;
      cursor = generateRegLitRefInstruction(cg, TR::InstOpCode::LG, node, targetRegister, value, TR_GlobalValue, cond, cursor, base);
      }
   else if (comp->compileRelocatableCode() && sym && (sym->isRecompilationCounter()))
      {
      TR::Instruction * temp = cursor;
      cursor = generateRegLitRefInstruction(cg, TR::InstOpCode::LG, node, targetRegister, value, TR_BodyInfoAddress, cond, cursor, base);
      }
   else if (comp->compileRelocatableCode() && sym && sym->isStatic() && !sym->isClassObject() && !sym->isNotDataAddress())
      {
      TR::Instruction * temp = cursor;
      cursor = generateRegLitRefInstruction(cg, TR::InstOpCode::LG, node, targetRegister, value, TR_DataAddress, cond, cursor, base);
      }
   else if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL && !(comp->compileRelocatableCode()))
      {
      cursor = generateRIInstruction(cg, TR::InstOpCode::LGHI, node, targetRegister, value, cursor);
      }
   else if (value >= GE_MIN_IMMEDIATE_VAL && value <= GE_MAX_IMMEDIATE_VAL) // value is signed 32bit - use sign extension
      {
      // If within Golden Eagle min and max immediate value, then use Load immediate.
      cursor = generateRILInstruction(cg, TR::InstOpCode::LGFI, node, targetRegister, static_cast<int32_t>(value), cursor);
      }
   else if (!(value & CONSTANT64(0x00000000FFFFFFFF)))
      {
      // Value is of 0x********00000000, we can use Golden Eagle LLIHF instr to load
      // the high 32 bits
      int32_t value32 = value >> 32;
      cursor = generateRILInstruction(cg, TR::InstOpCode::LLIHF, node, targetRegister, value32, cursor);
      }
   else if (!(value & CONSTANT64(0xFFFFFFFF00000000))) // 2^33 < value <= 2^32 ; value is unsigned 32bit so use load logical
      {
      cursor = generateRILInstruction(cg, TR::InstOpCode::LLILF, node, targetRegister, static_cast<int32_t>(value), cursor);
      }
   //disable LARL for AOT for easier relocation
   //cannot safely generate LARL for longs on 31-bit
   else if (cg->canUseRelativeLongInstructions(value) &&
            comp->target().is64Bit() && !(comp->compileRelocatableCode()))
      {
      cursor = generateRILInstruction(cg, TR::InstOpCode::LARL, node, targetRegister, reinterpret_cast<void*>(value), cursor);
      }
   else
      {
      int32_t high32 = value >> 32;

      cursor = generateRILInstruction(cg, TR::InstOpCode::LLIHF, node, targetRegister, high32, cursor);
      cursor = generateRILInstruction(cg, TR::InstOpCode::IILF, node, targetRegister, static_cast<int32_t>(value), cursor);
      }

   return cursor;
   }


/**
 * Generate code to load an address constant
 */
TR::Instruction *
genLoadAddressConstant(TR::CodeGenerator * cg, TR::Node * node, uintptr_t value, TR::Register * targetRegister,
   TR::Instruction * cursor, TR::RegisterDependencyConditions * cond, TR::Register * base)
   {
   if (cg->profiledPointersRequireRelocation() &&
            node->getOpCodeValue() == TR::aconst &&
            (node->isMethodPointerConstant() || node->isClassPointerConstant()))
      {
      TR_ExternalRelocationTargetKind reloKind = TR_NoRelocation;
      if (node->isMethodPointerConstant())
         {
         reloKind = TR_MethodPointer;
         if (node->getInlinedSiteIndex() == -1)
            reloKind = TR_RamMethod;
         }
      else if (node->isClassPointerConstant())
         reloKind = TR_ClassPointer;

      TR_ASSERT(reloKind != TR_NoRelocation, "relocation kind shouldn't be TR_NoRelocation");
      return generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, targetRegister, (uintptr_t) value, reloKind, cond, cursor, base);
      }

   TR::Symbol *symbol = NULL;
   TR::Compilation *comp = cg->comp();

   if (node && node->getOpCode().hasSymbolReference())
      symbol = node->getSymbol();
   bool isPICCandidate = symbol ? symbol->isStatic() && symbol->isClassObject() : false;
   if (isPICCandidate && !cg->comp()->compileRelocatableCode()
       && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)value, node))
      return genLoadAddressConstantInSnippet(cg, node, (uintptr_t)value, targetRegister, cursor, cond, base, true);

   if (node->isClassUnloadingConst())
      {
      uintptr_t value = node->getAddress();
      TR::Instruction *unloadableConstInstr = generateRILInstruction(cg, TR::InstOpCode::LARL, node, targetRegister, reinterpret_cast<void*>(value));
      TR_OpaqueClassBlock* unloadableClass = NULL;
      if (node->isMethodPointerConstant())
         {
         comp->getStaticMethodPICSites()->push_front(unloadableConstInstr);
         }
      else
         {
         comp->getStaticPICSites()->push_front(unloadableConstInstr);
         }
      return unloadableConstInstr;
      }

   if (cg->comp()->target().is64Bit())
      {
      return genLoadLongConstant(cg, node, (uint64_t) value, targetRegister, cursor, cond, base);
      }
   else
      {
      // TODO: We should update this API as well to accept a "canSetConditionCode" and forward that argument here rather than passing false blindly.
      return generateLoad32BitConstant(cg, node, (int32_t) value, targetRegister, false, cursor, cond, base);
      }
   }

TR::Instruction *
genLoadAddressConstantInSnippet(TR::CodeGenerator * cg, TR::Node * node, uintptr_t value, TR::Register * targetRegister,
   TR::Instruction * cursor, TR::RegisterDependencyConditions * cond, TR::Register * base, bool isPICCandidate)
   {
   return generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, targetRegister, value, cond, NULL, base, isPICCandidate);
   }

static TR::Register *
generateLoad32BitConstant(TR::CodeGenerator * cg, TR::Node * constExpr)
   {
   TR::Register * tempReg = NULL;
   switch (constExpr->getDataType())
      {
      case TR::Int8:
         tempReg = cg->allocateRegister();
         generateLoad32BitConstant(cg, constExpr, constExpr->getByte(), tempReg, false);
         break;
      case TR::Int16:
         tempReg = cg->allocateRegister();
         generateLoad32BitConstant(cg, constExpr, constExpr->getShortInt(), tempReg, false);
         break;
      case TR::Int32:
         tempReg = cg->allocateRegister();
         generateLoad32BitConstant(cg, constExpr, constExpr->getInt(), tempReg, false);
         break;
      case TR::Address:
         tempReg = cg->allocateRegister();
         if (cg->comp()->target().is64Bit())
            {
            genLoadLongConstant(cg, constExpr, constExpr->getLongInt(), tempReg);
            }
         else
            {
            generateLoad32BitConstant(cg, constExpr, constExpr->getInt(), tempReg, false);
            }

         break;
      case TR::Int64:
         {
         tempReg = cg->allocateRegister();
         genLoadLongConstant(cg, constExpr, constExpr->getLongInt(), tempReg);
         break;
         }
      case TR::Float:
         tempReg = cg->allocateRegister(TR_FPR);
         generateRegLitRefInstruction(cg, TR::InstOpCode::LE, constExpr, tempReg, constExpr->getFloat());
         break;
      case TR::Double:
         tempReg = cg->allocateRegister(TR_FPR);
         generateRegLitRefInstruction(cg, TR::InstOpCode::LD, constExpr, tempReg, constExpr->getDouble());
         break;
      default:
         TR_ASSERT( 0, "Unexpected datatype encountered on genLoad");
         break;
      }

   return tempReg;
   }

int64_t
getIntegralValue(TR::Node * node)
   {
   int64_t value = 0;
   if (node->getOpCode().isLoadConst())
      {
      switch (node->getDataType())
         {
         case TR::Int8:
            value = node->getByte();
            break;
         case TR::Int16:
            value = node->getShortInt();
            break;
         case TR::Int32:
            value = node->getInt();
            break;
         case TR::Address:
            value = node->getAddress();
            break;
         case TR::Int64:
            value = node->getLongInt();
            break;
         default:
            TR_ASSERT(0, "Unexpected element load datatype encountered");
            break;
         }
      }
   return value;
   }
TR::Instruction *multiply31Reduction(TR::CodeGenerator * cg, TR::Node * node,
   TR::Register * sourceRegister, TR::Register * targetRegister, int32_t value,
   TR::Instruction * preced = NULL)
   {
   TR::Compilation *comp = cg->comp();
   TR::Instruction * cursor = NULL;
   int32_t absValue = (value < 0) ? -value : value;
   int32_t shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo(absValue);

   if (value == -1)
      {
      cursor = generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, sourceRegister, preced);
      return cursor;
      }


   if (shiftValue >= 0 && shiftValue < 32)
      {
      int32_t numOperations = 0;
      if (shiftValue != 0)
         {
         numOperations += 1;
         }
      if (value < 0)
         {
         numOperations += 1;
         }

      if (!cg->mulDecompositionCostIsJustified(numOperations, 0, 0, (int64_t)value))
         {
         return NULL;
         }

      if (shiftValue != 0)
         {
         if (targetRegister != sourceRegister)
            {
            if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
               {
               cursor = generateRSInstruction(cg, TR::InstOpCode::SLLK, node, targetRegister, sourceRegister, shiftValue, preced);
               }
            else
               {
               cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, sourceRegister, shiftValue, preced);
               }
            }
         else
            {
            cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, targetRegister, shiftValue, preced);
            }
         }

      if (value < 0)
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, targetRegister, (cursor != NULL) ? cursor : preced);
         }

      return cursor;
      }

   // if it is Trex or Danu, then we want to do a multiply instead of a shift/subtract.
   // But z6 multiply is slow, so we still want to do the decomposition
   // We also need to disable the optimization in the tree simplifier -- see overloaded
   // method, TR_S390CodeGenerator::mulDecompositionCostIsJustified()

   if (!cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
      {
      return cursor;
      }

   // find bracketing powers of 2 and use this to see if an easy shift/add or shift/subtract
   // sequence can be done. also - optimize for the case where it is (2**i -1) or (2**i + 1)
   // since this can be done more efficiently (the shift degrades into a no-op since it is a shift
   // of 0

   int32_t before = 31;
   for (int32_t i = 1; i < 32; i++)
      {
      if (0x1 << i > absValue)
         {
         before = i - 1; break;
         }
      }
   int32_t after = before + 1;
   int32_t posDifference = absValue - (1 << before);
   int32_t negDifference = (int32_t) ((((int64_t) 1) << after) - absValue);
   shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo(posDifference); // i.e. 3, 5, 9, 17, 6, 10, 18, 20

   if (shiftValue >= 0 && shiftValue < 32)
      {
      int32_t numOperations =
         ((sourceRegister != targetRegister) ? 1 : 0) + // first LR
         1 + // second LR
         ((before != 0) ? 1 : 0) + // first SLL
         ((shiftValue != 0) ? 1 : 0) + // second SLL
         1 + // AR
         ((value < 0) ? 1 : 0); // LCR

      if (!cg->mulDecompositionCostIsJustified(numOperations, 0, 0, (int64_t)value))
         {
         return NULL;
         }

      TR::Register * tempRegister = cg->allocateRegister();
      if (sourceRegister != targetRegister)
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, targetRegister, sourceRegister, preced);
         }
      cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegister, sourceRegister, (cursor != NULL) ? cursor : preced);
      if (before != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, targetRegister, before, cursor);
         }
      if (shiftValue != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tempRegister, shiftValue, cursor);
         }
      cursor = generateRRInstruction(cg, TR::InstOpCode::AR, node, targetRegister, tempRegister, cursor);
      if (value < 0)
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, targetRegister);
         }
      cg->stopUsingRegister(tempRegister);

      return cursor;
      }

  shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo(negDifference); // i.e. 7, 12, 14, 15,
  if (shiftValue >= 0 && shiftValue < 32)
     {
      int32_t numOperations =
         ((sourceRegister != targetRegister) ? 1 : 0) + // first LR
         1 + // second LR
         ((after != 0) ? 1 : 0) + // first SLL
         ((shiftValue != 0) ? 1 : 0) + // second SLL
         1 + // SR
         ((value < 0) ? 1 : 0); // LCR

      if (!cg->mulDecompositionCostIsJustified(numOperations, 0, 0, (int64_t)value))
         {
         return NULL;
         }

     TR::Register * tempRegister = cg->allocateRegister();
     if (targetRegister != sourceRegister)
        {
        cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, targetRegister, sourceRegister, preced);
        }
     cursor = generateRRInstruction(cg, TR::InstOpCode::LR, node, tempRegister, sourceRegister, (cursor != NULL) ? cursor : preced);
     if (after != 0)
        {
        cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, targetRegister, after, cursor);
        }
     if (shiftValue != 0)
        {
        cursor = generateRSInstruction(cg, TR::InstOpCode::SLL, node, tempRegister, shiftValue, cursor);
        }
     cursor = generateRRInstruction(cg, TR::InstOpCode::SR, node, targetRegister, tempRegister, cursor);
     if (value < 0)
        {
        cursor = generateRRInstruction(cg, TR::InstOpCode::LCR, node, targetRegister, targetRegister, cursor);
        }
     cg->stopUsingRegister(tempRegister);

     return cursor;
     }

   return NULL;
   }

/**
 * Reduce multiply High when the value is power of 2 in 64-bit.
 * This function returns the high word of the multiplication result.
 */
TR::Instruction *multiplyHigh64Reduction(TR::CodeGenerator * cg, TR::Node * node,
   TR::Register * sourceRegister, TR::Register * targetRegister, int64_t  value)
   {
   TR::Instruction * cursor = NULL;
   int64_t absValue = (value < 0) ? -value : value;
   int64_t shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo(absValue);

   if (shiftValue >=0 && shiftValue < 64)
      {
      // Shift left by 'shiftValue' amount == shift right by 64-shiftValue
      cursor = generateRSInstruction(cg, TR::InstOpCode::SRLG, node, targetRegister, sourceRegister, 64-shiftValue);
      return cursor;
      }
   return NULL;
   }

TR::Instruction *multiply64Reduction(TR::CodeGenerator * cg, TR::Node * node,
   TR::Register * sourceRegister, TR::Register * targetRegister, int64_t  value, TR::Instruction * preced = NULL)
   {
   TR::Instruction * cursor = NULL;
   int64_t absValue = (value < 0) ? -value : value;
   int64_t shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo(absValue);

   if (value == -1)
      {
      cursor = generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegister, sourceRegister, preced);
      return cursor;
      }

   if (shiftValue >= 0 && shiftValue < 64)
      {
      int32_t numOperations =
         ((shiftValue != 0) ? 1 : 0) + // SLLG
         ((value < 0) ? 1 : 0); // LCGR

      if (!cg->mulDecompositionCostIsJustified(numOperations, 0, 0, value))
         {
         return NULL;
         }

      if (shiftValue != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, sourceRegister, shiftValue, preced);
         }
      if (value < 0)
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegister, targetRegister,
         (cursor != NULL) ? cursor : preced);
         }
      return cursor;
      }

   // find bracketing powers of 2 and use this to see if an easy shift/add or shift/subtract
   // sequence can be done. also - optimize for the case where it is (2**i -1) or (2**i + 1)
   // since this can be done more efficiently (the shift degrades into a no-op since it is a shift
   // of 0.  1st and 2nd orders of Booth's algorithm

   int32_t before = 63;
   for (int32_t i = 1; i < 64; i++)
      {
      if (0x1 << i > absValue)
         {
         before = i - 1; break;
         }
      }
   int32_t after = before + 1;
   int64_t posDifference = absValue - (1 << before);
   int64_t negDifference = ((((int64_t) 1) << after) - absValue);
   shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo(posDifference); // i.e. 3, 5, 9, 17, 6, 10, 18, 20

   if (shiftValue >= 0 && shiftValue < 64)
      {
      int32_t numOperations =
         1 + // LGR
         ((shiftValue != 0) ? 1 : 0) + // first SLLG
         ((before != 0) ? 1 : 0) + // second SLLG
         1 + // AGR
         ((value < 0) ? 1 : 0); // LCGR

      if (!cg->mulDecompositionCostIsJustified(numOperations, 0, 0, value))
         {
         return NULL;
         }

      TR::Register * tempRegister = cg->allocateRegister();
      cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, tempRegister, sourceRegister, preced);
      if (shiftValue != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempRegister, sourceRegister, shiftValue, cursor);
         }
      if (before != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, sourceRegister, before, cursor);
         }
      cursor = generateRRInstruction(cg, TR::InstOpCode::AGR, node, targetRegister, tempRegister, cursor);
      if (value < 0)
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegister, targetRegister, cursor);
         }
      cg->stopUsingRegister(tempRegister);

      return cursor;
      }

   shiftValue = TR::TreeEvaluator::checkNonNegativePowerOfTwo(negDifference); // i.e. 7, 12, 14, 15,
   if (shiftValue >= 0 && shiftValue < 64)
      {
      int32_t numOperations =
         1 + // LGR
         ((shiftValue != 0) ? 1 : 0) + // first SLLG
         ((after != 0) ? 1 : 0) + // second SLLG
         1 + // SGR
         ((value < 0) ? 1 : 0); // LCGR

      if (!cg->mulDecompositionCostIsJustified(numOperations, 0, 0, value))
         {
         return NULL;
         }

      TR::Register * tempRegister = cg->allocateRegister();
      cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, tempRegister, sourceRegister, preced);
      if (shiftValue != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempRegister, sourceRegister, shiftValue, cursor);
         }
      if (after != 0)
         {
         cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, sourceRegister, after, cursor);
         }
      cursor = generateRRInstruction(cg, TR::InstOpCode::SGR, node, targetRegister, tempRegister, cursor);
      if (value < 0)
         {
         cursor = generateRRInstruction(cg, TR::InstOpCode::LCGR, node, targetRegister, targetRegister, cursor);
         }
      cg->stopUsingRegister(tempRegister);

      return cursor;
      }
   return NULL;
   }

TR::Instruction *
generateS390ImmOp(TR::CodeGenerator * cg,  TR::InstOpCode::Mnemonic memOp, TR::Node * node,
   TR::Register * sourceRegister, TR::Register * targetRegister, int32_t value, TR::RegisterDependencyConditions * cond,
   TR::Register * base, TR::Instruction * preced)
   {
   TR::Compilation *comp = cg->comp();
   TR::Instruction * cursor = NULL, *resultMultReduction = NULL;
   TR::InstOpCode::Mnemonic immOp = TR::InstOpCode::BAD;

   // LL: Store Golden Eagle extended immediate instruction - 6 bytes long
   TR::InstOpCode::Mnemonic ei_immOp = TR::InstOpCode::BAD;

   switch (memOp)
      {
      case TR::InstOpCode::A:
         if (value == 0) return cursor;

         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::AHI;
            }
         else if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Add Immediate with max 32-bit value.
            ei_immOp = TR::InstOpCode::AFI;
            }
         break;
      case TR::InstOpCode::AG:
         if (value == 0) return cursor;

         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::AGHI;
            }
         else if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Add Immediate with max 32-bit value.
            ei_immOp = TR::InstOpCode::AGFI;
            }
         memOp = TR::InstOpCode::AGF;
         break;

      case TR::InstOpCode::CG:
         if (value == 0)
            {
            if ((sourceRegister == targetRegister) && (cg->isActiveArithmeticCC(sourceRegister) || cg->isActiveLogicalCC(node, sourceRegister)))
               {
               return cursor;
               }
            else
               {
               cursor = generateRRInstruction(cg, TR::InstOpCode::LTGR, node, sourceRegister, sourceRegister, preced);
               return cursor;
               }
            }
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::CGHI;
            }
         else if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Compare Immediate with max 32-bit value.
            ei_immOp = TR::InstOpCode::CGFI;
            }
         memOp = TR::InstOpCode::CGF;
         break;
      case TR::InstOpCode::C:
         if (value == 0)
            {
            if ((sourceRegister == targetRegister) && (cg->isActiveArithmeticCC(sourceRegister) || cg->isActiveLogicalCC(node, sourceRegister)))
               {
               return cursor;
               }
            else
               {
               cursor = generateRRInstruction(cg, TR::InstOpCode::LTR, node, sourceRegister, sourceRegister, preced);
               return cursor;
               }
            }
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::CHI;
            }
         else if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Compare Immediate with max 32-bit value.
            ei_immOp = TR::InstOpCode::CFI;
            }
         break;

      case TR::InstOpCode::CL:
         // Comparing to NULL, we can get away with LTR
         if ((value == 0)&&(TR::ILOpCode::isEqualCmp(node->getOpCodeValue()) || TR::ILOpCode::isNotEqualCmp(node->getOpCodeValue())))
            {
            if (!targetRegister)
               {
               targetRegister = sourceRegister;
               }

            if (cg->isActiveArithmeticCC(sourceRegister) || cg->isActiveLogicalCC(node, sourceRegister))
               {
               return cursor;
               }
            else
               {
               cursor = generateRRInstruction(cg, TR::InstOpCode::LTR, node, sourceRegister, sourceRegister, preced);
               return cursor;
               }
            }
         else
            {
            if (cg->canUseGoldenEagleImmediateInstruction(value))
               {
               // LL: If Golden Eagle - can use Compare Logical Immediate with max 32-bit value.
               ei_immOp = TR::InstOpCode::CLFI;
               targetRegister = sourceRegister;
               }
            // Number fits into LHI
            else if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
               {
               TR::Register * constReg = NULL;
               if (targetRegister != NULL && targetRegister != sourceRegister)
                  {
                  constReg = targetRegister;
                  }
               else
                  {
                  // testing register pressure
                  if (!targetRegister)
                     targetRegister = sourceRegister;
                  break;
                  }

               if (cond)
                  {
                  cond->addPostConditionIfNotAlreadyInserted(constReg, TR::RealRegister::AssignAny);
                  }
               cursor = generateLoad32BitConstant(cg, node, value, constReg, true, preced, cond, NULL);
               cursor = generateRRInstruction(cg, TR::InstOpCode::CLR, node, sourceRegister, constReg, cursor);
               cg->stopUsingRegister(constReg);
               return cursor;
               }
            // Big ugly number, go to lit pool
            else
               {
               if (!targetRegister)
                  {
                  targetRegister = sourceRegister;
                  }
               }
            }
         break;

      case TR::InstOpCode::CLG:
      case TR::InstOpCode::CLGF:
         if ((value == 0)&&(TR::ILOpCode::isEqualCmp(node->getOpCodeValue()) || TR::ILOpCode::isNotEqualCmp(node->getOpCodeValue())))
            {
            if ((sourceRegister == targetRegister) && (cg->isActiveArithmeticCC(sourceRegister) || cg->isActiveLogicalCC(node, sourceRegister)))
               {
               return cursor;
               }
            else
               {
               cursor = generateRRInstruction(cg, TR::InstOpCode::LTGR, node, targetRegister, sourceRegister, preced);
               return cursor;
               }
            }
         if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Compare Logical Immediate with max 32-bit value.
            ei_immOp = TR::InstOpCode::CLGFI;
            break;
            }
         memOp = TR::InstOpCode::CLGF;
         break;

      case TR::InstOpCode::AL:
         TR_ASSERT( sourceRegister != targetRegister, "generateS390ImmOp: AL requires two regs\n");
         if (value == 0)  return cursor;
         if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Add Logical Immediate with max 32-bit value.
            ei_immOp = TR::InstOpCode::ALFI;
            }
         else if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, sourceRegister, value, preced);
            cursor = generateRRInstruction(cg, TR::InstOpCode::ALR, node, targetRegister, sourceRegister, cursor);

            return cursor;
            }
         sourceRegister = targetRegister;
         break;

      case TR::InstOpCode::ALC:
         TR_ASSERT( sourceRegister != targetRegister, "generateS390ImmOp: ALC requires two regs\n");
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, sourceRegister, value, preced);
            cursor = generateRRInstruction(cg, TR::InstOpCode::ALCR, node, targetRegister, sourceRegister, cursor);

            return cursor;
            }
         else if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            cursor = generateRILInstruction(cg, TR::InstOpCode::IILF, node, sourceRegister, value, preced);
            cursor = generateRRInstruction(cg, TR::InstOpCode::ALCR, node, targetRegister, sourceRegister, cursor);

            return cursor;
            }
         else
            {
            sourceRegister = targetRegister;
            }
         break;

      case TR::InstOpCode::SL:
         if (value == 0)  return cursor;
         if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Add Logical Immediate with max 32-bit value.
            ei_immOp=TR::InstOpCode::SLFI;
            }
         else if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            TR_ASSERT( sourceRegister != targetRegister, "generateS390ImmOp: SL requires two regs\n");

            cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, sourceRegister, value, preced);
            cursor = generateRRInstruction(cg, TR::InstOpCode::SLR, node, targetRegister, sourceRegister, cursor);

            return cursor;
            }
         sourceRegister = targetRegister;
         break;

      case TR::InstOpCode::SLB:
         TR_ASSERT( sourceRegister != targetRegister, "generateS390ImmOp: SLB requires two regs\n");
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, sourceRegister, value, preced);
            cursor = generateRRInstruction(cg, TR::InstOpCode::SLBR, node, targetRegister, sourceRegister, cursor);

            return cursor;
            }
         else if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            cursor = generateRILInstruction(cg, TR::InstOpCode::IILF, node, sourceRegister, value, preced);
            cursor = generateRRInstruction(cg, TR::InstOpCode::SLBR, node, targetRegister, sourceRegister, cursor);

            return cursor;
            }
         else
            {
            sourceRegister = targetRegister;
            }
         break;

      case TR::InstOpCode::ALG:
         if (value == 0) return cursor;
         if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            // LL: If Golden Eagle - can use Add Logical Immediate with max 32-bit value.
            ei_immOp = TR::InstOpCode::ALGFI;
            break;
            }
         memOp=TR::InstOpCode::ALGF;
         break;

      case TR::InstOpCode::X:
         {
         if (value == 0)
            {
            return cursor;
            }

         ei_immOp = TR::InstOpCode::XILF;
         }
         break;

      case TR::InstOpCode::O:
         {
         // Avoid generating useless instructions such as N R1, 0x00000000
         if (value == 0)
            {
            return cursor;
            }

         if (value == -1)
            {
            cursor = generateRIInstruction(cg, TR::InstOpCode::LHI, node, targetRegister, -1, preced);

            return cursor;
            }

         ei_immOp = TR::InstOpCode::OILF;

         // Prefer shorter instruction encodings if possible
         if (!(value & 0x0000FFFF))
            {
            // value = 0x****0000
            value = value >> 16;
            immOp = TR::InstOpCode::OILH;
            ei_immOp = TR::InstOpCode::BAD; // Reset so we don't generate OILF.
            }
         else if (!(value & 0xFFFF0000))
            {
            // value = 0x0000****
            immOp = TR::InstOpCode::OILL;
            ei_immOp = TR::InstOpCode::BAD; // Reset so we don't generate OILF.
            }
         }
         break;

      case TR::InstOpCode::N:
         {
         // Avoid generating useless instructions such as N R1, 0xFFFFFFFF
         if (value == -1)
            {
            return cursor;
            }

         if (value == 0)
            {
            cursor = generateRRInstruction(cg, TR::InstOpCode::XR, node, targetRegister, sourceRegister, preced);

            return cursor;
            }

         ei_immOp = TR::InstOpCode::NILF;

         // Prefer shorter instruction encodings if possible
         if ((value & 0xFFFF0000) == 0xFFFF0000)
            {
            value = value & 0x0000FFFF;
            immOp = TR::InstOpCode::NILL;
            ei_immOp = TR::InstOpCode::BAD; // Reset so we don't generate NILF.
            }
         else if ((value & 0x0000FFFF) == 0x0000FFFF)
            {
            value = value >> 16;
            immOp = TR::InstOpCode::NILH;
            ei_immOp = TR::InstOpCode::BAD; // Reset so we don't generate NILF.
            }
         }
         break;

      case TR::InstOpCode::MSG:
         if (value==0)
            {
            cursor = generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetRegister, sourceRegister, preced);
            return cursor;
            }
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::MGHI;
            }
         else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && value >= GE_MIN_IMMEDIATE_VAL && value <= GE_MAX_IMMEDIATE_VAL)
            {
            ei_immOp = TR::InstOpCode::MSGFI;
            }

         // don't want to overwrite cursor in case multiply64Reduction returns null

         resultMultReduction = multiply64Reduction(cg, node, sourceRegister, targetRegister, value, preced);
         if(resultMultReduction != NULL)
            {
            cursor = resultMultReduction;
            return cursor;
            }

         memOp=TR::InstOpCode::MSGF;
         break;

      case TR::InstOpCode::MS:
      case TR::InstOpCode::M:
         {
         TR::Register* targetRegisterNoPair = targetRegister->getRegisterPair() ? targetRegister->getHighOrder() : targetRegister;

         if (value == 0)
            {
            cursor = generateRRInstruction(cg, TR::InstOpCode::XR, node, targetRegisterNoPair, targetRegisterNoPair, preced);
            return cursor;
            }
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::MHI;
            }
         else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && memOp == TR::InstOpCode::MS)
            {
            ei_immOp = TR::InstOpCode::MSFI;
            }

         // Don't overwrite the cursor in case multiply31Reduction returns null
         resultMultReduction=multiply31Reduction(cg, node, sourceRegister, targetRegisterNoPair, value, preced);

         if (resultMultReduction!=NULL)
            {
            cursor = resultMultReduction;

            return cursor;
            }
         }
         break;
      case TR::InstOpCode::AE:
      case TR::InstOpCode::SE:
      case TR::InstOpCode::AEB:
      case TR::InstOpCode::SEB:
         // These should go to litpool
         break;
      default:
         TR_ASSERT( 0, "Unsupported opcode in  generateS390ImmOp !\n");
         break;
      }

   TR::Register* targetRegisterNoPair = targetRegister->getRegisterPair() ? targetRegister->getHighOrder() : targetRegister;

   if (targetRegister != sourceRegister)
      {
      cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCodeFromNode(cg, node), node, targetRegisterNoPair, sourceRegister, (cursor != NULL) ? cursor : preced);
      }

   if (ei_immOp != TR::InstOpCode::BAD)
      {
      cursor = generateRILInstruction(cg, ei_immOp, node, targetRegisterNoPair, value, (cursor != NULL) ? cursor : preced);
      return cursor;
      }
   else if (immOp != TR::InstOpCode::BAD)
      {
      cursor = generateRIInstruction(cg, immOp, node, targetRegisterNoPair, value, (cursor != NULL) ? cursor : preced);
      return cursor;
      }
   else
      {
      // We cannot load this value easily with ops, we must use the lit pool
      cursor = generateRegLitRefInstruction(cg, memOp, node, targetRegister, value, cond, NULL, base, (cursor != NULL) ? cursor : preced);
      return cursor;
      }
   }

TR::Instruction *
generateS390ImmOp(TR::CodeGenerator * cg,
   TR::InstOpCode::Mnemonic memOp, TR::Node * node, TR::Register * sourceRegister, TR::Register * targetRegister, int64_t value,
   TR::RegisterDependencyConditions * cond, TR::Register * base, TR::Instruction * preced)
   {
   TR_ASSERT( preced == NULL, "Support has not yet been adding for preced instruction");
   TR::Instruction * cursor = NULL;
   TR::InstOpCode::Mnemonic immOp=TR::InstOpCode::BAD;

   // LL: Store Golden Eagle extended immediate instruction - 6 bytes long
   TR::InstOpCode::Mnemonic ei_immOp=TR::InstOpCode::BAD;

   const int64_t hhMask = CONSTANT64(0x0000FFFFFFFFFFFF);
   const int64_t hlMask = CONSTANT64(0xFFFF0000FFFFFFFF);
   const int64_t lhMask = CONSTANT64(0xFFFFFFFF0000FFFF);
   const int64_t llMask = CONSTANT64(0xFFFFFFFFFFFF0000);
   // LL: Mask for high and low 32 bits
   const int64_t highMask = CONSTANT64(0x00000000FFFFFFFF);
   const int64_t lowMask  = CONSTANT64(0xFFFFFFFF00000000);

   // LL: If it is bitwise instruction for long constant in either 64 or 32 bits target,
   //     we can handle them here (i.e. if opcode is N, NG, O, OG, X, or XG.)
   //     For all other opcodes, if long value is a 32-bit value or it is not TR::InstOpCode::ADB in 32bit,
   //     then call generateS390ImmOp with the equivalent 32-bit value constant.
   if (memOp != TR::InstOpCode::N  && memOp != TR::InstOpCode::O  && memOp != TR::InstOpCode::X &&
       memOp != TR::InstOpCode::NG && memOp != TR::InstOpCode::OG && memOp != TR::InstOpCode::XG)
      {
      bool isUnsigned = node->getOpCode().isUnsigned();
      TR::InstOpCode tempOpCode(memOp);
      bool isNotSpecialOpCode = memOp!=TR::InstOpCode::ADB && memOp!=TR::InstOpCode::CDB && memOp!=TR::InstOpCode::SDB &&
                                memOp!=TR::InstOpCode::AD && memOp!=TR::InstOpCode::CD && memOp!=TR::InstOpCode::SD &&
                                memOp!=TR::InstOpCode::CG && memOp!=TR::InstOpCode::CLG && memOp!= TR::InstOpCode::LG && memOp!=TR::InstOpCode::MLG;
      if ( (!isUnsigned && value == (int32_t) value) ||
           (isUnsigned && value == (uint32_t) value) ||
           (!tempOpCode.is64bit() && isNotSpecialOpCode))
         {
         return  generateS390ImmOp(cg, memOp, node, sourceRegister, targetRegister, (int32_t) value, cond, base);
         }
      }

   // LL: Verify we have a register pair for bitwise immediate op for long constant
   // in 32bit target.  Currently we only handle long constant for 32bit target for
   // bitwise instructions.
   // NOTE: You can only get in here if bitwiseOpNeedsLiteralFromPool() return false.
   if (cg->comp()->target().is32Bit() &&
        (memOp == TR::InstOpCode::N || memOp == TR::InstOpCode::O || memOp == TR::InstOpCode::X))
      {
      TR_ASSERT( sourceRegister->getRegisterPair(), "Long value in 32-bit target must be in a register pair : OpCode %s\n", memOp);
      }

   switch (memOp)
      {
      // generate some bitwise immediate ops if on zarch and bit pattern is suitable
      case TR::InstOpCode::O:
      case TR::InstOpCode::OG:
         if (value == 0) return NULL;  // do not generate useless instructions, such as O r1, 0

         // Prefer shorter instruction encodings if possible
         if (!(value & hhMask))
            {  // value = 0x****000000000000
            value = value >> 48;
            immOp = TR::InstOpCode::OIHH;
            }
         else if (!(value & hlMask))
            {  // value = 0x0000****00000000
            value = (value & CONSTANT64(0x0000FFFF00000000)) >> 32;
            immOp = TR::InstOpCode::OIHL;
            }
         else if (!(value & lhMask))
            { // value = 0x00000000****0000
            value = (value & CONSTANT64(0x00000000FFFF0000)) >> 16;
            immOp = TR::InstOpCode::OILH;
            }
         else if (!(value & llMask))
            { // value = 0x000000000000****
            value = value;
            immOp = TR::InstOpCode::OILL;
            }
         else
            {
            if (!(value & highMask))
               {
               // value = 0x********00000000
               value = value >> 32;
               ei_immOp = TR::InstOpCode::OIHF;
               }
            else if (!(value & lowMask))
               {
               // value = 0x00000000********
               ei_immOp= TR::InstOpCode::OILF;
               }
            else
               {
               int32_t h_value = (int32_t)(value>>32);
               int32_t l_value = (int32_t)value;

               generateRILInstruction(cg, TR::InstOpCode::OILF, node, targetRegister, l_value);
               cursor = generateRILInstruction(cg, TR::InstOpCode::OIHF, node, targetRegister, h_value);

               return cursor;
               }
            }
         break;

      case TR::InstOpCode::N:
      case TR::InstOpCode::NG:
         // Avoid generating useless instructions such as N R1, 0x00000000
         if (value == -1)
            {
            return NULL;
            }

         // Prefer shorter instruction encodings if possible
         if ((value & hhMask) == hhMask)
            {
            // value = 0x****FFFFFFFFFFFF
            value = value >> 48;
            immOp = TR::InstOpCode::NIHH;
            }
         else if ((value & hlMask) == hlMask)
            {
            // value = 0xFFFF****FFFFFFFF
            value = (value & CONSTANT64(0x0000FFFF00000000)) >> 32;
            immOp = TR::InstOpCode::NIHL;
            }
         else if ((value & lhMask) == lhMask)
            {
            // value = 0xFFFFFFFF****FFFF
            value = (value & CONSTANT64(0x00000000FFFF0000)) >> 16;
            immOp = TR::InstOpCode::NILH;
            }
         else if ((value & llMask) == llMask)
            {
            // value = 0xFFFFFFFFFFFF****
            immOp = TR::InstOpCode::NILL;
            }
         else
            {
            // LL: If Golden Eagle - can use generate fast AND-Immediate on the 32 bits value depending on the bits pattern.
            if ((value & highMask) == highMask)
               {  // value = 0x********FFFFFFFF
               value = value >> 32;
               ei_immOp = TR::InstOpCode::NIHF;
               }
            else if ((value & lowMask) == lowMask)
               {  // value = 0xFFFFFFFF********
               ei_immOp = TR::InstOpCode::NILF;
               }
            else
               {
               int32_t h_value = (int32_t)(value >> 32);
               int32_t l_value = (int32_t)value;

               generateRILInstruction(cg, TR::InstOpCode::NILF, node, targetRegister, l_value);
               cursor = generateRILInstruction(cg, TR::InstOpCode::NIHF, node, targetRegister, h_value);

               return cursor;
               }
            }
         break;

      case TR::InstOpCode::X:
      case TR::InstOpCode::XG:
         if (!(value & highMask))
            {
            // value = 0x********00000000
            value = value >> 32;
            ei_immOp = TR::InstOpCode::XIHF;
            }
         else if (!(value & lowMask))
            {
            // value = 0x00000000********
            ei_immOp = TR::InstOpCode::XILF;
            }
         else
            {
            int32_t h_value = (int32_t)(value >> 32);
            int32_t l_value = (int32_t)value;

            generateRILInstruction(cg, TR::InstOpCode::XILF, node, targetRegister, l_value);
            cursor = generateRILInstruction(cg, TR::InstOpCode::XIHF, node, targetRegister, h_value);

            return cursor;
            }
         break;

      case TR::InstOpCode::MSG:
         if (value == 0)
            {
            return generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetRegister, sourceRegister);
            }
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::MGHI;
            }
         else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && value >= GE_MIN_IMMEDIATE_VAL && value <= GE_MAX_IMMEDIATE_VAL)
            {
            ei_immOp = TR::InstOpCode::MSGFI;
            }

         // TODO: Do not overwrite cursor in case the value returned is null?
         cursor = multiply64Reduction(cg, node, sourceRegister, targetRegister, value);
         if (cursor != NULL)
            {
            return cursor;
            }

         break;

      case TR::InstOpCode::MLG:
         if (value == 0)
            {
            generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetRegister->getHighOrder(), sourceRegister->getHighOrder());
            return generateRRInstruction(cg, TR::InstOpCode::XGR, node, targetRegister->getLowOrder(), sourceRegister->getLowOrder());
            }
         // Check if we can reduce the multiplication and get the high word when multiplied by power of 2
         cursor = multiplyHigh64Reduction(cg, node, sourceRegister->getLowOrder(), targetRegister->getHighOrder(), value);
         if (cursor != NULL) return cursor;
         break;

      case TR::InstOpCode::CG:
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::CGHI;
            }
         else if (cg->canUseGoldenEagleImmediateInstruction(value))
            {
            ei_immOp = TR::InstOpCode::CGFI;
            }
         else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
            {
            memOp = TR::InstOpCode::CGRL;
            }
         break;
      case TR::InstOpCode::CLG:
         if (cg->canUseGoldenEagleUnsignedImmediateInstruction(value))
            {
            ei_immOp = TR::InstOpCode::CLGFI;
            }
         else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
            {
            memOp = TR::InstOpCode::CLGRL;
            }
         break;
      case TR::InstOpCode::LG:
         if (value >= MIN_IMMEDIATE_VAL && value <= MAX_IMMEDIATE_VAL)
            {
            immOp = TR::InstOpCode::LGHI;
            }
         else if (cg->canUseGoldenEagleUnsignedImmediateInstruction(value))
            {
            ei_immOp = TR::InstOpCode::LLILF;
            }
         else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
            {
            memOp = TR::InstOpCode::LGRL;
            }
         else
            {
            // Didn't fit in IILF, so we need two instructions to build the immediate
            // Note... there is a tradeoff here between going to the lit-pool (extra reg and
            // potential d-cache) vs the extra cycle for 1 extra instr.   The latter would
            // seem to be more likely to be a win.
            //
            generateRILInstruction(cg, TR::InstOpCode::LLIHF, node, targetRegister, static_cast<int32_t>(value));

            ei_immOp = TR::InstOpCode::IILF;
            }
         break;
      case TR::InstOpCode::AG:
         {
         // This case is to handle adding unsigned operands, and the immediate operand is 32-bit.
         if (cg->canUseGoldenEagleUnsignedImmediateInstruction(value))
            {
            ei_immOp = TR::InstOpCode::ALGFI;
            }
         //This case is to convert the addition to a subtraction of 32-bit immediate operand.
         else if ((value & lowMask) == lowMask && (value & highMask) > 0)
            {
            ei_immOp = TR::InstOpCode::SLGFI;
            value = 1 + ~value;
            }
         }
         break;
      case TR::InstOpCode::ALG:
      case TR::InstOpCode::ADB:
      case TR::InstOpCode::CDB:
      case TR::InstOpCode::SDB:
      case TR::InstOpCode::AD:
      case TR::InstOpCode::CD:
      case TR::InstOpCode::SD:
      case TR::InstOpCode::SG:
         break;
      default:
         TR_ASSERT(0, "Unsupported opcode in  generateS390ImmOp !\n");
         break;

      }  // end switch

   if (targetRegister != sourceRegister)
      {
      cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCodeFromNode(cg, node), node, targetRegister, sourceRegister);
      }

   // LL: Golden Eagle extended immediate instructions
   if (ei_immOp != TR::InstOpCode::BAD)
      {
      return generateRILInstruction(cg, ei_immOp, node, targetRegister, static_cast<int32_t>(value));
      }
   else if (immOp != TR::InstOpCode::BAD)
      {
      return generateRIInstruction(cg, immOp, node, targetRegister, (int16_t)value);
      }
   else
      {
      if (cg->comp()->target().is32Bit() && memOp != TR::InstOpCode::MLG)
         {
         TR_ASSERT(!targetRegister->getRegisterPair(), "This instruction is for 64bit target only, it should not take a register pair.\n");
         }
      return generateRegLitRefInstruction(cg, memOp, node, targetRegister, value, cond, NULL, base);
      }
   }

TR::Instruction *
generateS390ImmOp(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic memOp, TR::Node * node, TR::Register * targetRegister, float value,
   TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   TR_ASSERT( preced == NULL, "Support has not yet been adding for preced instruction");
   return generateRegLitRefInstruction(cg, memOp, node, targetRegister, value);
   }

TR::Instruction *
generateS390ImmOp(TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic memOp, TR::Node * node, TR::Register * targetRegister, double value,
   TR::RegisterDependencyConditions * cond, TR::Instruction * preced)
   {
   TR_ASSERT( preced == NULL, "Support has not yet been adding for preced instruction");
   return generateRegLitRefInstruction(cg, memOp, node, targetRegister, value);
   }

void
updateReferenceNode(TR::Node * node, TR::Register * reg)
   {
   TR::Symbol * sym = node->getSymbolReference()->getSymbol();
   if ((node->getOpCodeValue() == TR::aload || node->getOpCodeValue() == TR::aloadi) &&
        sym->isCollectedReference() &&
        !node->getSymbolReference()->getSymbol()->isAddressOfClassObject())
      {
      if (sym->isInternalPointer())
         {
         reg->setContainsInternalPointer();
         reg->setPinningArrayPointer(sym->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
         }
      else
         {
         reg->setContainsCollectedReference();
         }
      }
   else
      {
      if (sym->isInternalPointer())
         {
         reg->setContainsInternalPointer();
         reg->setPinningArrayPointer(sym->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
         }
      }
   }

/**
 * Manufacture immediate value to a register
 * Do NOT use the lit pool.
 */
TR::Instruction *
generateS390ImmToRegister(TR::CodeGenerator * cg, TR::Node * node, TR::Register * targetRegister, intptr_t value, TR::Instruction * cursor)
   {
   // We use a low word of 15bits to avoid sign problems.
   intptr_t high = (value >> 15) & 0xffff;
   intptr_t low  = (value & 0x7fff);
   TR::Compilation *comp = cg->comp();

   TR_ASSERT(value>=(int32_t)0xc0000000 && value<=(int32_t)0x30000000,"generateS390ImmToRegister: Value 0x%x not handled\n", value);

   if (high)
      {
      cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, targetRegister, high, cursor);
      cursor = generateRSInstruction(cg, TR::InstOpCode::SLLG, node, targetRegister, targetRegister, 15, cursor);

      if (low)
         {
         cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, targetRegister, low, cursor);
         }
      }
   else
      {
      cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, targetRegister, low, cursor);
      }

   return cursor;
   }

TR::Instruction *
generateS390FloatCompareAndBranchOps(TR::Node *node,
                                     TR::CodeGenerator *cg,
                                     TR::InstOpCode::S390BranchCondition fBranchOpCond,
                                     TR::InstOpCode::S390BranchCondition rBranchOpCond,
                                     TR::InstOpCode::S390BranchCondition &retBranchOpCond,
                                     TR::LabelSymbol *branchTarget)
   {
   TR::DataType dataType = node->getFirstChild()->getDataType();
   TR_ASSERT(dataType == TR::Float ||
           dataType == TR::Double,
           "only floats are supported by this function");

   TR::Instruction *returnInstruction = 0;
   TR::Compilation *comp = cg->comp();
   bool isDouble = (dataType == TR::Double);

   if ((node->getOpCodeValue() == TR::ifdcmpneu ||
        node->getOpCodeValue() == TR::ifdcmpeq ||
        node->getOpCodeValue() == TR::dcmpeq ||
        node->getOpCodeValue() == TR::dcmpneu) &&
       node->getSecondChild()->getOpCode().isLoadConst() &&
       node->getSecondChild()->getDouble() == 0.0)
      {
      TR::Register* testRegister = cg->evaluate(node->getFirstChild());
      generateRRInstruction(cg, TR::InstOpCode::LTDBR, node, testRegister, testRegister);
      if (branchTarget)
         returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, fBranchOpCond, node, branchTarget);
      retBranchOpCond = fBranchOpCond;
      cg->decReferenceCount(node->getFirstChild());
      cg->decReferenceCount(node->getSecondChild());
      return returnInstruction;
      }

   TR::InstOpCode::Mnemonic regRegOpCode = isDouble ? TR::InstOpCode::CDBR : TR::InstOpCode::CEBR;

   TR::InstOpCode::Mnemonic regMemOpCode = isDouble ? TR::InstOpCode::CDB : TR::InstOpCode::CEB;

   TR::InstOpCode::Mnemonic regCopyOpCode = isDouble ? TR::InstOpCode::LDR : TR::InstOpCode::LER;

   TR_S390BinaryCommutativeAnalyser analyser(cg);
   analyser.genericAnalyser(node, regRegOpCode, regMemOpCode, regCopyOpCode, true);

   bool isForward = (analyser.getReversedOperands() == false);

   // There should be no register attached to the compare node, otherwise
   // the register is kept live longer than it should.
   node->unsetRegister();

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   TR::InstOpCode::S390BranchCondition returnCond = (isForward? fBranchOpCond : rBranchOpCond);


   // Generate the branch if required.
   if (branchTarget != NULL)
      {
      returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, returnCond, node, branchTarget);
      }

   // Set the return Branch Condition
   retBranchOpCond = returnCond;

   return returnInstruction;
   }

#ifdef J9_PROJECT_SPECIFIC
/**
 * Generate compare and branch code for packed decimal
 * If branchTarget label is set, the code will generate a compare and branch instruction(s) and return that instruction.
 * retBranchOpCond (reference parameter) returns the appropriate fBranchOpCond or rBranchOpCond depending on direction of compare instruction that is generated
 */
TR::Instruction *
generateS390PackedCompareAndBranchOps(TR::Node * node,
                                      TR::CodeGenerator * cg,
                                      TR::InstOpCode::S390BranchCondition fBranchOpCond,
                                      TR::InstOpCode::S390BranchCondition rBranchOpCond,
                                      TR::InstOpCode::S390BranchCondition &retBranchOpCond,
                                      TR::LabelSymbol *branchTarget)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR_PseudoRegister *firstReg = cg->evaluateBCDNode(firstChild);

   TR::Node *secondChild = node->getSecondChild();
   TR_PseudoRegister *secondReg = cg->evaluateBCDNode(secondChild);
   TR::Compilation *comp = cg->comp();

   bool useCLC = false;
   int32_t clcSize = 0;
   if (cg->traceBCDCodeGen())
      traceMsg(comp,"pdcompare node %p attempt to gen CLC with firstReg %s (symRef #%d) and secondReg %s (symRef #%d)\n",
         node,cg->getDebug()->getName(firstReg),firstReg->getStorageReference()->getReferenceNumber(),cg->getDebug()->getName(secondReg),secondReg->getStorageReference()->getReferenceNumber());
   bool signsAndDataAreValid = firstReg->hasKnownValidSignAndData() && secondReg->hasKnownValidSignAndData();
   if (signsAndDataAreValid)
      {
      bool knownSigns = firstReg->hasKnownOrAssumedSignCode() &&
                        secondReg->hasKnownOrAssumedSignCode() &&
                        firstReg->knownOrAssumedSignCodeIs(secondReg->getKnownOrAssumedSignCode());
      bool cleanSigns = firstReg->hasKnownOrAssumedCleanSign() &&
                        secondReg->hasKnownOrAssumedCleanSign();
      if (knownSigns || cleanSigns)
         {
         if (cg->traceBCDCodeGen())
            {
            traceMsg(comp,"\t+validateDecimalSignAndData=%s (firstReg (validData=%s validSign=%s) and secondReg (validData=%s validSign=%s))\n",
               true?"yes":"no",
               firstReg->hasKnownValidData()?"yes":"no",firstReg->hasKnownValidSign()?"yes":"no",
               secondReg->hasKnownValidData()?"yes":"no",secondReg->hasKnownValidSign()?"yes":"no");
            if (knownSigns)
               traceMsg(comp,"\t+firstReg and secondReg have matching known or assumed sign codes (both are 0x%x)\n",secondReg->getKnownOrAssumedSignCode());
            if (cleanSigns)
               traceMsg(comp,"\t+firstReg and secondReg have known or assumed clean signs\n");
            }

         bool knownSignsArePositive = knownSigns && firstReg->hasKnownOrAssumedPositiveSignCode();
         if (knownSignsArePositive || fBranchOpCond == TR::InstOpCode::COND_BE || fBranchOpCond == TR::InstOpCode::COND_BNE)
            {
            if (cg->traceBCDCodeGen())
               traceMsg(comp,"\t+branchCond %s is allowed (knownSignsArePositive = %s) so check sizes\n",node->getOpCode().getName(),knownSignsArePositive?"yes":"no");
            if (firstReg->getSize() == secondReg->getSize())
               {
               useCLC = true;
               clcSize = firstReg->getSize();
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\t+regSizes match (firstRegSize = secondRegSize = %d) so do gen CLC\n",firstReg->getSize());
               }
            else
               {
               if (firstReg->getSize() < secondReg->getSize())
                  {
                  if (cg->traceBCDCodeGen())
                     traceMsg(comp,"\t+firstRegSize < secondRegSize (%d < %d) so check upper bytes on firstReg\n",firstReg->getSize(),secondReg->getSize());
                  if (firstReg->getLiveSymbolSize() >= secondReg->getSize())
                     {
                     if (cg->traceBCDCodeGen())
                        traceMsg(comp,"\t+firstReg->liveSymSize() >= secondReg->getSize() (%d >= %d)\n",firstReg->getLiveSymbolSize(),secondReg->getSize());
                     if (firstReg->getBytesToClear(firstReg->getSize(), secondReg->getSize()) == 0)
                        {
                        if (cg->traceBCDCodeGen())
                           traceMsg(comp,"\t+upper bytes (byte %d to %d) are clear so do gen CLC with clcSize = secondReg->getSize() = %d\n",firstReg->getSize(),secondReg->getSize(),secondReg->getSize());
                        useCLC = true;
                        clcSize = secondReg->getSize();
                        }
                     else if (cg->traceBCDCodeGen())
                        {
                        traceMsg(comp,"\t-upper bytes (byte %d to %d) are not clear so do not gen CLC\n",firstReg->getSize(),secondReg->getSize());
                        }
                     }
                  else if (cg->traceBCDCodeGen())
                     {
                     traceMsg(comp,"\t-firstReg->liveSymSize() < secondReg->getSize() (%d < %d) so do not gen CLC\n",firstReg->getLiveSymbolSize(),secondReg->getSize());
                     }
                  }
               else  // firstReg->getSize() > secondReg->getSize()
                  {
                  if (cg->traceBCDCodeGen())
                     traceMsg(comp,"\t+firstRegSize > secondRegSize (%d > %d) so check upper bytes on secondReg\n",firstReg->getSize(),secondReg->getSize());
                  if (secondReg->getLiveSymbolSize() >= firstReg->getSize())
                     {
                     if (cg->traceBCDCodeGen())
                        traceMsg(comp,"\t+secondReg->liveSymSize() >= firstReg->getSize() (%d >= %d)\n",secondReg->getLiveSymbolSize(),firstReg->getSize());
                     if (secondReg->getBytesToClear(secondReg->getSize(), firstReg->getSize()) == 0)
                        {
                        if (cg->traceBCDCodeGen())
                           traceMsg(comp,"\t+upper bytes (byte %d to %d) are clear so do gen CLC with clcSize = firstReg->getSize() = %d\n",secondReg->getSize(),firstReg->getSize(),firstReg->getSize());
                        useCLC = true;
                        clcSize = firstReg->getSize();
                        }
                     else if (cg->traceBCDCodeGen())
                        {
                        traceMsg(comp,"\t-upper bytes (byte %d to %d) are not clear so do not gen CLC\n",secondReg->getSize(),firstReg->getSize());
                        }
                     }
                  else if (cg->traceBCDCodeGen())
                     {
                     traceMsg(comp,"\t-secondReg->liveSymSize() < firstReg->getSize() (%d < %d) so do not gen CLC\n",secondReg->getLiveSymbolSize(),firstReg->getSize());
                     }
                  }
               }
            }
         else if (cg->traceBCDCodeGen())
            {
            traceMsg(comp,"\t-branchCond %s is not allowed (knownSignsArePositive = %s) so do not gen CLC\n",node->getOpCode().getName(),knownSignsArePositive?"yes":"no");
            }
         }
      else if (cg->traceBCDCodeGen())
         {
         traceMsg(comp,"\t-firstReg (clean=%s known=0x%x) and secondReg (clean=%s known=0x%x) signs are not compatible so do not gen CLC\n",
            firstReg->hasKnownOrAssumedCleanSign()?"yes":"no",firstReg->hasKnownOrAssumedSignCode()?firstReg->getKnownOrAssumedSignCode():0,
            secondReg->hasKnownOrAssumedCleanSign()?"yes":"no",secondReg->hasKnownOrAssumedSignCode()?secondReg->getKnownOrAssumedSignCode():0);
         }
      }
   else if (cg->traceBCDCodeGen())
      {
      traceMsg(comp,"\t-firstReg (validData=%s validSign=%s) and secondReg (validData=%s validSign=%s) signs/data are not valid so do not gen CLC\n",
         firstReg->hasKnownValidData()?"yes":"no",firstReg->hasKnownValidSign()?"yes":"no",
         secondReg->hasKnownValidData()?"yes":"no",secondReg->hasKnownValidSign()?"yes":"no");
      }

   TR::Instruction *inst = NULL;
   if (useCLC)
      {
      TR_ASSERT(clcSize > 0,"clcSize (%d) must be set at this point\n",clcSize);
      if (cg->traceBCDCodeGen())
         traceMsg(comp,"gen CLC with size %d\n",clcSize);
      inst = generateSS1Instruction(cg, TR::InstOpCode::CLC, node,
                                    clcSize-1,
                                    generateS390RightAlignedMemoryReference(firstChild, firstReg->getStorageReference(), cg),
                                    generateS390RightAlignedMemoryReference(secondChild, secondReg->getStorageReference(), cg));
      }
   else
      {
      TR::MemoryReference *firstMR = generateS390RightAlignedMemoryReference(firstChild, firstReg->getStorageReference(), cg);
      TR::MemoryReference *secondMR = generateS390RightAlignedMemoryReference(secondChild, secondReg->getStorageReference(), cg);
      cg->correctBadSign(firstChild, firstReg, firstReg->getSize(), firstMR);
      cg->correctBadSign(secondChild, secondReg, secondReg->getSize(), secondMR);
      inst = generateSS2Instruction(cg, TR::InstOpCode::CP, node,
                                    firstReg->getSize()-1,
                                    firstMR,
                                    secondReg->getSize()-1,
                                    secondMR);
      firstReg->setHasKnownValidSignAndData();
      secondReg->setHasKnownValidSignAndData();
      }

   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps,
         "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      cg->decReferenceCount(thirdChild);
      }

   TR::InstOpCode::S390BranchCondition branchCond = fBranchOpCond;
   retBranchOpCond = branchCond;
   if (branchTarget)
      inst = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, branchCond, node, branchTarget, deps);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return inst;
   }

/**
 * Generate compare and branch code for long double and DFP
 * If branchTarget label is set, the code will generate a compare and branch instruction(s) and return that instruction.
 * retBranchOpCond (reference parameter) returns the appropriate fBranchOpCond or rBranchOpCond depending on direction of compare instruction that is generated
 */
TR::Instruction *
generateS390DFPLongDoubleCompareAndBranchOps(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond, TR::InstOpCode::S390BranchCondition &retBranchOpCond, TR::LabelSymbol *branchTarget = NULL)
   {
   bool isForward = true;
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   bool isUnsignedCmp = node->getOpCode().isUnsignedCompare();
   TR::InstOpCode::S390BranchCondition newBranchOpCond = TR::InstOpCode::COND_NOP;
   TR::Instruction * returnInstruction = NULL;
   TR::Compilation *comp = cg->comp();
   bool isBranchGenerated = false;

   TR::DataType dataType = node->getFirstChild()->getDataType();
   TR::InstOpCode::Mnemonic cmpOp = TR::InstOpCode::BAD;
   if (dataType == TR::DecimalDouble)
      cmpOp = TR::InstOpCode::CDTR;
   else if (dataType == TR::DecimalLongDouble)
      cmpOp = TR::InstOpCode::CXTR;

   if (cg->whichChildToEvaluate(node) == 0)
      {
      firstChild = node->getFirstChild();
      secondChild = node->getSecondChild();
      }
   else
      {
      isForward = false;
      firstChild = node->getSecondChild();
      secondChild = node->getFirstChild();
      }

   TR::Register * firstRegister = firstChild->getRegister();
   TR::Register * secondRegister = secondChild->getRegister();

   if (firstRegister==NULL)
      firstRegister = cg->evaluate(firstChild);
   if (secondRegister==NULL)
      secondRegister = cg->evaluate(secondChild);


   generateRRInstruction(cg, cmpOp, node, firstRegister, secondRegister);

   // There should be no register attached to the compare node, otherwise
   // the register is kept live longer than it should.
   node->unsetRegister();
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   TR::InstOpCode::S390BranchCondition returnCond;

   if (newBranchOpCond!=TR::InstOpCode::COND_NOP)
      returnCond = (newBranchOpCond);
   else
      returnCond = (isForward? fBranchOpCond : rBranchOpCond);

   // Generate the branch if required.
   if (branchTarget != NULL && !isBranchGenerated)
      {
      returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, returnCond, node, branchTarget);
      }

   // Set the return Branch Condition
   retBranchOpCond = returnCond;

   return returnInstruction;
   }
#endif

static bool
memoryReferenceMightNeedLargeOffset(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getOpCode().isMemoryReference(),
           "node must be a memory reference");

   TR::Compilation *comp = cg->comp();

   // This is yet another heuristic to detect large offsets
   // (non-Java where stack frames large)
   //
   if (node->getOpCode().hasSymbolReference() &&
       ((uintptr_t)node->getSymbolReference()->getOffset() > MAX_12_RELOCATION_VAL)) // unsigned checks for neg too
      return true;

   if (node->getOpCode().isIndirect())
      {
      TR::Node *address = node->getFirstChild();
      if (address->getOpCode().isArrayRef() &&
          address->getSecondChild()->getOpCode().isIntegralConst())
         {
         int64_t offset = address->getSecondChild()->get64bitIntegralValue();

         // This is leading to worse codegen when the offset is too large
         // to fit wide displacement instructions. There, we'll have to do some fixup
         // anyway (eg. a LA and AFI to compute the address), so we can still use
         // small-offset instructions.
         if ((offset > MAX_12_RELOCATION_VAL && offset <= MAXLONGDISP) ||
             offset < 0)
            {
            //offset is too large to fit in 12 bits
            return true;
            }
         }
      }

   return false;
   }

static bool
memoryReferenceMightNeedAnIndexRegister(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getOpCode().isMemoryReference(),
           "node must be a memory reference");

   // Heuristic: Are we likely to need index regs? indirect loads
   // can be compilated; if there is an aladd or a loadaddr involved
   // then we become pessimistic
   //
   if (node->getOpCode().isLoadIndirect())
      {
      TR::Node *addrChild = node->getFirstChild();

      if (addrChild->getOpCode().isLoadAddr() ||
          addrChild->getNumChildren() > 1)
         {
         if (addrChild->getOpCode().isAdd() &&
             !addrChild->getFirstChild()->getOpCode().isAdd()  &&
             addrChild->getSecondChild()->getOpCode().isLoadConst())
            {
            return false;
            }
         return true;
         }
      }

   return false;
   }


/**
 * Tries to generate SI form comparisons
 * e.g. CLI,                   ; logical compare  8-bit unsigned imm
 *      CLHHSI, CLFHSI, CLGHSI ; logical compare 16-bit unsigned imm
 *      CHHSI,  CHSI,   CGHSI  ;  signed compare 16-bit   signed imm
 * Returns false if its not a good idea to use these instructions.
 * If isOnlyTest is true, returns true if these instructions can be used.
 * If isOnlyTest is false, generates instruction and then returns true.
 * It is the responsibility of the caller to decrement the children.
 */
static bool
tryGenerateSIComparisons(TR::Node *node, TR::Node *constNode, TR::Node *otherNode, TR::CodeGenerator *cg, bool isOnlyTest)
   {
   TR_ASSERT(constNode->getOpCode().isLoadConst(),
           "constNode must be a constant");

   TR::Node *operand = otherNode;
   TR::Compilation *comp = cg->comp();

#ifdef J9_PROJECT_SPECIFIC
   if ((operand->getOpCode().isConversion() && (operand->getFirstChild()->getType().isBCD() || operand->getFirstChild()->getType().isFloatingPoint())) || constNode->getType().isBCD())
      {
      return 0;
      }
#else
   if (operand->getOpCode().isConversion() && operand->getFirstChild()->getType().isFloatingPoint())
      {
      return 0;
      }
#endif

   // Conversions might be involved. Deal with them.
   //
   if (otherNode->getOpCode().isConversion())
      {
      operand = otherNode->getFirstChild();

      // The conversion must not be a truncation
      //
      if (otherNode->getSize() < operand->getSize())
         return 0;

      // The conversion must not be in a register or want to be in a register
      //
      if (!otherNode->isSingleRefUnevaluated())
         return 0;
      }

   // Operand must be a memory reference
   //
   if (!operand->getOpCode().isMemoryReference())
      return 0;

   // The operand must not be in a register, or want to be in a register
   //
   if (!operand->isSingleRefUnevaluated())
      return 0;

   uint8_t operandSize = operand->getSize();

   bool isUnsignedCmp = node->getOpCode().isUnsignedCompare();

   TR::Instruction *i;
   if (operandSize == 1) // we can use CLI
      {
      // The value must be a representable as an unsigned byte
      // consider the the following trees:
      // ifbcmpeq          ificmp                                       ificmp
      //   bload             b2i                                          b2i
      //   bconst              bload                                        bload
      //  CLI - good         iconst 255                                   iconst 16
      //                      CLI - bad, i.e. FFFFFFFF != 000000FF         CLI - good
      uint64_t value = constNode->get64bitIntegralValueAsUnsigned();
      if (isUnsignedCmp && 0 != (value >> 8) || // only the bottom byte
          !isUnsignedCmp && (operand == otherNode && 0 != (value >> 8) || // have not skipped conversion
                             operand != otherNode && 0 != (value >> 7))) // guaranteed to be positive byte if interpreted as logical or signed
         {
         return 0;
         }

      // Gotta be careful with signed comparisons for order
      //
      if (!isUnsignedCmp &&
          node->getOpCode().isCompareForOrder())
         {
         // We can still use a CLI if the non-const child is a zero
         // extended value
         //
         if (!otherNode->isZeroExtension())
            {
            return 0;
            }
         }

      // Are we likely to use an index register for the memory reference? avoid the CLI
      //
      if (memoryReferenceMightNeedAnIndexRegister(operand, cg))
         {
         return 0;
         }

      // We don't care about large offsets because there is a Y form (CLIY) available
      // that can deal with them.

      if (isOnlyTest) return true;

      TR::MemoryReference *memRef = generateS390MemoryReference(operand, cg);

      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "CLI-Success\n");

      // Generate the CLI
      //
      i = generateSIInstruction(cg, TR::InstOpCode::CLI, node, memRef, value);
      }
   else if (operandSize == 2 || operandSize == 4 || operandSize == 8) // we can use the SIL form compares
      {
      // 16-bit immediates and these instructions are only avaialble on z10 and up
      if (!cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
         {
         return 0;
         }

      int64_t  svalue = getIntegralValue(constNode);
      uint64_t uvalue = constNode->get64bitIntegralValueAsUnsigned();

      // For singed comparisons, if the immediate is a zero-extendable 16-bit
      // value, we can convert to unsigned comparisons in some cases. This is preferable
      // because we get an extra bit in the immediate.
      //
      if (!isUnsignedCmp &&
          (0 == (uvalue >> 16)))
         {
         // Check if we can convert an signed equality comparison to a
         // unsigned comparison.
         // E.g.
         //   ificmpne(iload x, iconst 0xc3e4)
         // This would be incorrect if emitted as:
         //   CHSI  [x], 0xc3e4  ; sign extend immediate
         //   BNE
         // However, the following is perfectly valid:
         //   CLHSI [x], 0xc3e4
         //   BNE
         //
         // If we are going to skip a conversion, we will be doing
         // a smaller compare. We can only use unsigned if the conversion
         // is a zero extension
         // e.g.
         //  ificmpeq
         //    s2i
         //      sconst x
         //    iconst 0xc3e4
         // This cannot be converted to use an unsigned comparison.
         //
         if (node->getOpCode().isCompareForEquality() &&
             (!otherNode->getOpCode().isConversion() ||
              otherNode->isZeroExtension()))
            isUnsignedCmp = true;

         // Check if we can convert a compare for order into an unsigned
         // compare for order
         // E.g.
         //   iflcmpgt(iu2l(iload x), lconst 0xc3e4)
         // This would be incorret if emitted as:
         //   CGHSI  [x],0xc3e4 ; gets sign extended to 0xfff..fc3e4
         // However, the following is perfectly valid
         //   CLGHSI [x],0xc3e4
         //
         if (node->getOpCode().isCompareForOrder() &&
             otherNode->isZeroExtension()) // order compare, but zero-extension involved
            isUnsignedCmp = true;

         // Another way of expressing the above logic would be:
         // if there is a zero-extension involved, used an unsigned compare.
         // otherwise, if if no conversions are invovled and its an equality
         // compare, we can use unsigned compare.
         }

      // Make sure the immediate is compatiable with the type of comparison
      if (isUnsignedCmp)
         {
         // The value must be a unsigned 16-bit value
         //
         if (0 != (uvalue >> 16))
            {
            return 0;
            }
         }
      else
         {
         // The value must be a signed 16-bit value
         //
         if (svalue > MAX_IMMEDIATE_VAL ||
             svalue < MIN_IMMEDIATE_VAL)
            {
            return 0;
            }
         }

      // Are we likely to need an indexReg? Shouldn't use SI form
      //
      if (memoryReferenceMightNeedAnIndexRegister(operand, cg))
         {
         return 0;
         }

      // Are we likely to need a large offset in the memRefs? shouldn't use SI form
      //
      if (memoryReferenceMightNeedLargeOffset(operand, cg))
         {
         return 0;
         }

      if (isOnlyTest) return true;

      TR::MemoryReference *memRef = generateS390MemoryReference(operand, cg);

      TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::BAD;
      if (operandSize == 8)
         opCode = (isUnsignedCmp ? TR::InstOpCode::CLGHSI : TR::InstOpCode::CGHSI);
      else if (operandSize == 4)
         opCode = (isUnsignedCmp ? TR::InstOpCode::CLFHSI : TR::InstOpCode::CHSI);
      else if (operandSize == 2)
         opCode = (isUnsignedCmp ? TR::InstOpCode::CLHHSI : TR::InstOpCode::CHHSI);
      else
         TR_ASSERT( 0 == 1, "un-expected type");

      i = generateSILInstruction(cg, opCode, node, memRef, svalue); // doesn't matter if we use svalue or uvalue, only 16 bits are needed

      if (comp->getOption(TR_TraceCG))
         traceMsg(comp, "SI-Success\n");

      // FIXME: is this necessary?
      memRef->stopUsingMemRefRegister(cg);
      }

   else // of (operandSize == ...
      {
      // Odd size? probably an i[o]load
      return 0;
      }


   // if we skipped a level, decrement the grand-children.
   if (operand != otherNode)
      cg->decReferenceCount(operand);

      // The caller is responsible for decrementing the reference count of the immediate children

   return true;
   }


/**
 * Tries to generate a CLC for the comparison node provided.
 * return null if unsucessful
 * doesn't decrement the refcnt of the immediate children (caller's responsible for that)
 * may decrement refcnt of grandchildren
 */
static TR::Instruction *
tryGenerateCLCForComparison(TR::Node *node, TR::CodeGenerator *cg)
   {
   // CLC can only be used for equality or unsigned comparisons
   if (!node->getOpCode().isUnsignedCompare() &&
       !node->getOpCode().isCompareForEquality())
      return 0;

   // Can't deal with floats
   // TODO: Perhaps for equality comparisons we could?
   //
   if (node->getDataType() == TR::Float ||
       node->getDataType() == TR::Double)
      return 0;

   TR::Compilation *comp = cg->comp();
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *operand1    = firstChild;
   TR::Node *operand2    = secondChild;

#ifdef J9_PROJECT_SPECIFIC
   if ((operand1->getOpCode().isConversion() && (operand1->getFirstChild()->getType().isBCD() || operand1->getFirstChild()->getType().isFloatingPoint())) ||
       (operand2->getOpCode().isConversion() && (operand2->getFirstChild()->getType().isBCD() || operand2->getFirstChild()->getType().isFloatingPoint())))
      {
      return 0;
      }
#endif

   // See through conversions
   //
   if (firstChild->getOpCode().isConversion())
      {
      // Both children must be conversions
      if (!secondChild->getOpCode().isConversion())
         return 0;

      // Either both conversions cause sign externsion or
      // neither cause sign extension
      //
      if (firstChild->getType().isIntegral() &&
        (firstChild->getOpCode().isSignExtension() !=
         secondChild->getOpCode().isSignExtension()))
         return 0;

      // The conversions must not be in registers, or want to
      // be in registers
      //
      if (!firstChild ->isSingleRefUnevaluated() ||
          !secondChild->isSingleRefUnevaluated())
         return 0;

      operand1 = firstChild ->getFirstChild();
      operand2 = secondChild->getFirstChild();

      // The operands must be the same size
      //
      if (operand1->getSize() != operand2->getSize())
         return 0;

      // The conversion must not be a truncation
      //
      if (firstChild->getSize() < operand1->getSize())
         return 0;
      }

   // Make sure that neither operand is in a register, or wants to be a in
   // a register.
   //
   if (!operand1->isSingleRefUnevaluated() ||
       !operand2->isSingleRefUnevaluated())
      return 0;

   // the operands must both be memory references
   //
   if (!operand1->getOpCode().isMemoryReference() ||
       !operand2->getOpCode().isMemoryReference())
      return 0;

   // Check the sizes of both operands in memory
   if (operand1->getSymbolReference()->getSymbol()->getSize() !=
       operand2->getSymbolReference()->getSymbol()->getSize())
      return 0;

   // Are we likely to need a large offset in the memRefs? shouldn't use CLC
   //
   if (memoryReferenceMightNeedLargeOffset(operand1, cg) ||
       memoryReferenceMightNeedLargeOffset(operand2, cg))
      return 0;

   // Are we likely to need index regs? shouldn't use CLC
   //
   if (memoryReferenceMightNeedAnIndexRegister(operand1, cg) ||
       memoryReferenceMightNeedAnIndexRegister(operand2, cg))
      return 0;


   // If only one of operands is a VFT Symbol, then the other one is probably loaded (and masked) in a temp slot.
   // If this is the case, we won't use CLC on compressed refs, because the temp slot is 8 bytes whereas VFT Symbol
   // is 4 bytes and CLC needs a slightly modified displacement to point to the correct bytes.
   //
   // ============================================================
   // ; Live regs: GPR=2 FPR=0 VRF=0 GPR64=0 AR=0 {&GPR_5107, &GPR_5106}
   // ------------------------------
   //  n17006n  (  0)  astore  <temp slot 57>[#1624  Auto] [0x000003ff9f029290]
   //  n11644n  (  2)    aloadi  <vft-symbol>[#490  Shadow] [0x000003ff9b1116f0]
   //  n19707n  ( 11)      ==>aRegLoad (in &GPR_5107) ...
   // ------------------------------
   // ------------------------------
   //  n17006n  (  0)  astore  <temp slot 57>[#1624  Auto] [0x000003ff9f029290]
   //  n11644n  (  1)    aloadi  <vft-symbol>[#490  Shadow] (in GPR_5139) [0x000003ff9b1116f0]
   //  n19707n  ( 10)      ==>aRegLoad (in &GPR_5107)
   // ------------------------------
   //
   //  [0x000003ff9e54a730]   LLGF    GPR_5139, Shadow[<vft-symbol>] 0(&GPR_5107)
   //  [0x000003ff9e54a800]   NILL    GPR_5139,0xff00
   //  [0x000003ff9e54a9f0]   STG     GPR_5139, Auto[<temp slot 57>] ?+0(GPR5)
   //
   // ============================================================
   // ; Live regs: GPR=3 FPR=0 VRF=0 GPR64=0 AR=0 {&GPR_6161, &GPR_6146, &GPR_6145}
   // ------------------------------
   //  n11687n  (  0)  ifacmpne --> block_731 BBStart at n11656n [0x000003ff9b112460]
   //  n11679n  (  1)    aload  <temp slot 57>[#1624  Auto] [0x000003ff9b1121e0]
   //  n11683n  (  1)    aloadi  <vft-symbol>[#490  Shadow] [0x000003ff9b112320]
   //  n10888n  (  2)      ==>aload (in &GPR_6161)
   //  n19872n  (  1)    GlRegDeps [0x000003ff9ef76560]
   //  n19868n  ( 13)      ==>aRegLoad (in &GPR_6145)
   //  n19869n  ( 14)      ==>aRegLoad (in &GPR_6146)
   // ------------------------------
   // CLC-Success (size=3), node ifacmpne (000003FF9B112460)
   // ------------------------------
   //  n11687n  (  0)  ifacmpne --> block_731 BBStart at n11656n [0x000003ff9b112460]
   //  n11679n  (  0)    aload  <temp slot 57>[#1624  Auto] [0x000003ff9b1121e0]
   //  n11683n  (  0)    aloadi  <vft-symbol>[#490  Shadow] [0x000003ff9b112320]
   //  n10888n  (  1)      ==>aload (in &GPR_6161)
   //  n19872n  (  0)    GlRegDeps [0x000003ff9ef76560]
   //  n19868n  ( 12)      ==>aRegLoad (in &GPR_6145)
   //  n19869n  ( 13)      ==>aRegLoad (in &GPR_6146)
   // ------------------------------
   //
   //  [0x000003ff9e44be48]   CLC      Auto[<temp slot 57>] ?+0(3,GPR5), Shadow[<vft-symbol>] 0(&GPR_6161)
   //  [0x000003ff9e44c190]   ASSOCREGS
   //  [0x000003ff9e44bf88]   BRC     MASK3(0x6), Label L0175
   //
   // \\ gnu/testlet/java/text/DateFormat/Test.test(Lgnu/testlet/TestHarness;)V
   //  \\  344 JBaload0
   // 0x000003ffdf6ca49a 0000269e [0x000003ff9e54a730] e3 60 c0 00 00 16          LLGF    GPR6, Shadow[<vft-symbol>] 0(GPR12)
   // 0x000003ffdf6ca4a0 000026a4 [0x000003ff9e54a800] a5 67 ff 00                NILL    GPR6,0xff00
   // 0x000003ffdf6ca4a4 000026a8 [0x000003ff9e54a9f0] e3 60 50 78 00 24          STG     GPR6, Auto[<temp slot 57>] 120(GPR5)
   // \\ gnu/testlet/java/text/DateFormat/Test.test(Lgnu/testlet/TestHarness;)V
   //  \\  354 JBinvokevirtual 52 gnu/testlet/java/text/DateFormat/Test.equals(Ljava/lang/Object;)Z
   //  \\      19 JBifacmpeq 24
   // 0x000003ffdf6cb13a 0000333e [0x000003ff9e44be48] d5 02 50 78 70 00          CLC      Auto[<temp slot 57>] 120(3,GPR5), Shadow[<vft-symbol>] 0(GPR7)
   // 0x000003ffdf6cb140 00003344 [0x000003ff9e44c190]                            ASSOCREGS
   // 0x000003ffdf6cb140 00003344 [0x000003ff9e44bf88] a7 64 12 fc                BRC     MASK3(0x6), Label L0175, labelTargetAddr=0x000003FFDF6CD738

   bool operand1IsVFTSymbol = operand1->getOpCode().hasSymbolReference() && (operand1->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef());
   bool operand2IsVFTSymbol = operand2->getOpCode().hasSymbolReference() && (operand2->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef());

   if (comp->useCompressedPointers() &&
      ((!operand1IsVFTSymbol && operand2IsVFTSymbol) || (operand1IsVFTSymbol && !operand2IsVFTSymbol)))
      {
      return 0;
      }

   uint16_t numOfBytesToCompare = operand1->getSize();
   // CLC is slower than CL when the length is 4 bytes
   if (numOfBytesToCompare == 4)
      {
      return 0;
      }

   // TODO: We can still use CLC if only one of operands is a VFT symbol when we are using
   // compressed refs. But we have to adjust/increase the displacement of the operand that is a temp
   // slot so that CLC gets to compare the correct bytes.

   TR::MemoryReference *memRef1 = generateS390MemoryReference(operand1, cg);
   TR::MemoryReference *memRef2 = generateS390MemoryReference(operand2, cg);

   // If we have a pair of class pointers, we have to mask the rightmost byte (flags).
   // Instead, we ignore that byte and do not use it in our comparison. Since the operands
   // must be the same size (or we would have returned earlier), it does not matter form
   // which operand we get the size.
   // We use OR operator in case we have already loaded (and masked) one of the operands
   // in a temp slot.

   if (operand1IsVFTSymbol || operand2IsVFTSymbol)
      {
      if (cg->comp()->target().is64Bit() && (cg->isCompressedClassPointerOfObjectHeader(operand1) || cg->isCompressedClassPointerOfObjectHeader(operand2)))
         numOfBytesToCompare = 4; //getSize() currently return wrong value in compressedrefs

      numOfBytesToCompare--;
      }

   // Generate the CLC
   TR::Instruction *i = generateSS1Instruction(cg, TR::InstOpCode::CLC, node, numOfBytesToCompare-1, memRef1, memRef2);

   if (comp->getOption(TR_TraceCG))
      traceMsg(comp, "CLC-Success (size=%d), node %s (%p)\n", numOfBytesToCompare, node->getOpCode().getName(),node); // size = numOfBytesToCompare+1 since CLC is 0-based

   // If we skipped a level, decrement the grand-children
   //
   if (operand1 != firstChild)
      cg->decReferenceCount(operand1);
   if (operand2 != secondChild)
      cg->decReferenceCount(operand2);

   // its the job of the caller to decrement the immediate children

   return i;
   }

/**
 * Tries to use the auto-extending RX form instructions to do comparisons
 * E.g. CH, CGH, CGF  ; signed comparisons
 *               CLGF ; unsigned comparisons
 * TODO: we need to improve the binary commutative analyser to be able to do this
 * however given the time budget, this wasn't possible. There is a fair amount of
 * intelligence in the binary commutative analyser that we dont' try to duplicate
 * here to catch the other RX form opportunities.
 *
 * Note too the similarities between this code and tryGenerateCLCForComparison
 * and tryGenerateSIComparison
 *
 * It is a good idea to try and use the SI and CLC comparisons first; they would be superior
 * if they can be used. Only use the CH, CGH, CGF, CLGF instructions when we know (looking through
 * conversions) that one operand is bigger than the other and we know that it is going to
 * be in a register. The other operand must be a single reference, unevaluated memory reference
 * of a narrower kind.
 * If these conditions don't match we are going to fall back to the binary commutative analyser
 * and let it decide how to generate the code.
 */
static TR::Instruction *
tryGenerateConversionRXComparison(TR::Node *node, TR::CodeGenerator *cg, bool *isForward)
   {
   const bool ENABLE_CONVERSION_RX_COMPARE = true;
   TR::Compilation *comp = cg->comp();
   if (!ENABLE_CONVERSION_RX_COMPARE)
      return 0;

   // Can't deal with floats
   TR_ASSERT(node->getDataType() != TR::Float &&
           node->getDataType() != TR::Double,
           "floating point types should not reach here");

   TR::Node *operand1 = node->getFirstChild();
   TR::Node *operand2 = node->getSecondChild();

#ifdef J9_PROJECT_SPECIFIC
   if ((operand1->getOpCode().isConversion() && (operand1->getFirstChild()->getType().isBCD() || operand1->getFirstChild()->getType().isFloatingPoint())) ||
       (operand2->getOpCode().isConversion() && (operand2->getFirstChild()->getType().isBCD() || operand2->getFirstChild()->getType().isFloatingPoint())))
      {
      return 0;
      }
#endif

   // At least one of the nodes must be a conversion otherwise we can't do any better
   // than the commutative analyser
   //
   if (!operand1->getOpCode().isConversion() &&
       !operand2->getOpCode().isConversion())
      {
      return 0;
      }

   bool op1InReg = false;
   if (operand1->getRegister()                      ||
       (operand1->getOpCode().isConversion() &&
        (operand1->getFirstChild()->getRegister()
       )))
      {
      op1InReg = true;
      }

   bool op2InReg = false;
   if (operand2->getRegister()                      ||
       (operand2->getOpCode().isConversion() &&
        (operand2->getFirstChild()->getRegister()
      )))
      {
      op2InReg = true;
      }

   // If both are in registers, give up and let the binary commutative
   // analyser handle things.
   //
   if (op1InReg && op2InReg)
      {
      return 0;
      }
   // if neither is in a register, we'll have to nominate on to be the
   // reg child
   //
   else if (!op1InReg && !op2InReg)
      {
      if (operand2->getOpCode().isMemoryReference())
         op1InReg = true;
      else if (operand1->getOpCode().isMemoryReference() ||
          (operand1->getOpCode().isConversion() &&
           operand1->getFirstChild()->getOpCode().isMemoryReference()))
         op2InReg = true;
      else
         op1InReg = true;
      }

   TR::Node *regNode = op1InReg ? operand1 : operand2; // the node we'll eval to a reg
   TR::Node *memNode = op1InReg ? operand2 : operand1; // the node we'll use a memref for
   bool     forward = op1InReg ? true     : false;
   TR::Node *conversion = 0;
   bool isUnsignedCmp = node->getOpCode().isUnsignedCompare();

   if(!isUnsignedCmp)
      {
      switch(node->getOpCodeValue())
         {
         case TR::ifacmple:
         case TR::ifacmplt:
         case TR::ifacmpge:
         case TR::ifacmpgt:
            isUnsignedCmp = true;
//            traceMsg(cg->comp(), "Setting isUnsignedCmp to true for address compare\n");
            break;
         default:
            break;
         }
      }

   bool convertToSignExtension = false;
   if (memNode->getOpCode().isConversion())
      {
      conversion = memNode;
      memNode = memNode->getFirstChild();

      // This better be an extension and not a truncation
      //
      if (conversion->getSize() < memNode->getSize())
         {
         return 0;
         }

      // For equality compares, use the type of compares as specified by the
      // extension
      //
      if (node->getOpCode().isCompareForEquality())
         isUnsignedCmp = (conversion->isZeroExtension());

      // The conversion must match the polarity of the compare
      //
      if (conversion->isZeroExtension() &&
          !isUnsignedCmp)
         {
         return 0;
         }
      if (conversion->getOpCode().isSignExtension() &&
          isUnsignedCmp)
         {
         return 0;
         }

      // The conversion must not be in a register, or want to be in a
      // register
      //
      if (!conversion->isSingleRefUnevaluated())
         {
         return 0;
         }

      // This is a very special case to deal with the assymetry in the
      // logical versions of the half-word rx form compare opcodes.
      // Specificaly, we don't have logical versions of CH and
      // CGH. However we can do some tricky business to use a signed
      // comparison in some cases (only if the comparison is for equality)
      //
      if (memNode->getSize() == 2 &&        // only short memref
          conversion->isZeroExtension() &&  // being zero extended
          isUnsignedCmp &&
          node->getOpCode().isCompareForEquality())
         {
         // we have a unsigned comparison for equality with a memref
         // that is 2-bytes. We need to use CLH or CLGH, however, they
         // don't exist (at the time of this writing).
         //
         // HOWEVER, if the other child is a zero-extension too, then the
         // larger comparison is only comparing the bottom 16-bits. In that case
         // it woudl be okay if we instead caused both children to be sign extended
         // instead!! I.e. do sign extension of the other child, and use CH (which
         // does sign extension too). Et Voila!
         //
         // An example for an important DB2 loop (dsnionx2) is:
         // ificmpne
         //   su2i
         //     memRef
         //   su2i
         //     => sRegLoad
         //
         //
         // have to delay actually doing the transformation until it is guaranteed that a CH will be generated
         // as there are still more bailout conditions to be checked
         // otherwise an su2i will be changed to an s2i and then a CR could be used and this would be incorrect
         if (regNode->isZeroExtension()               &&
             regNode->getFirstChild()->getSize() == 2 &&
             regNode->isSingleRefUnevaluated() &&
             performTransformation(comp,
                                    "O^O Convert zero extension node [%p] to sign extension so that we can use CH/CGH\n",
                                     regNode))
            {
            convertToSignExtension = true;
            }
         }
      }

   // its the memNode child isn't a mem ref, give up
   //
   if (!memNode->getOpCode().isMemoryReference())
      {
      return 0;
      }

   // Make sure the mem ref node isn't in a register, and doesn't
   // need to be in a register
   //
   if (!memNode->isSingleRefUnevaluated())
      {
      return 0;
      }

   uint8_t regSize = regNode->getSize();
   uint8_t memSize = memNode->getSize();

   if (regSize != 4 && regSize != 8)
      {
      // What is this? an aggregate?
      return 0;
      }

   if (memSize != 2 && memSize != 4 && memSize != 8)
      {
      // What is this? an aggregate?
      return 0;
      }

   static const TR::InstOpCode::Mnemonic choices[2/*signedness*/][2/*regSize*/][3/*memSize*/] =
      {
      // Signed
      // memSize                    regSize
      // 2       4       8             v
      {{ TR::InstOpCode::CH,  TR::InstOpCode::C,   TR::InstOpCode::BAD },  // 4
       { TR::InstOpCode::BAD, TR::InstOpCode::CGF, TR::InstOpCode::CG  }}, // 8 // FIXME: CGH missing because it doesn't exist in s390ops

      // Unsigned
      // memSize                     regSize
      // 2       4        8             v
      {{ TR::InstOpCode::BAD, TR::InstOpCode::CL,   TR::InstOpCode::BAD },  // 4
       { TR::InstOpCode::BAD, TR::InstOpCode::CLGF, TR::InstOpCode::CLG }}, // 8
      };

   // FIXME: the above table needs to be intersected with the architecture.

   uint8_t memSizeCoord = (memSize == 2 ? 0 : (memSize == 4 ? 1 : 2));
   // forcing a signedCmp (e.g. CH) if convertToSignExtension is true
   TR::InstOpCode::Mnemonic op = choices[convertToSignExtension ? false : isUnsignedCmp][regSize == 4 ? 0 : 1][memSizeCoord];

   if (op == TR::InstOpCode::BAD)
      {
      return 0;
      }

   if (convertToSignExtension)
      {
      TR::Node::recreate(regNode, TR::s2i);
      regNode->setUnneededConversion(false); // it is definitely needed now
      }

   TR::Register *reg = cg->evaluate(regNode);
   TR::MemoryReference *memRef = generateS390MemoryReference(memNode, cg);

   TR::Instruction *i = generateRXInstruction(cg, op, node, reg, memRef);

   if (comp->getOption(TR_TraceCG))
      traceMsg(comp, "Conversion RX-Success\n");

   // We skipped a conversion, we must decrement the grandchild
   //
   if (conversion)
      cg->decReferenceCount(memNode);

   // its the job of the caller to decrement the immediate children

   *isForward = forward;
   return i;
   }

/**
 * Generate compare and branch code depending on the types to be compared.
 * If branchTarget label is set, the code will generate a compare and branch instruction(s) and return that instruction.
 * retBranchOpCond (reference parameter) returns the appropriate fBranchOpCond or rBranchOpCond depending on direction of compare instruction that is generated
 */
TR::Instruction *
generateS390CompareAndBranchOpsHelper(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::S390BranchCondition fBranchOpCond,
      TR::InstOpCode::S390BranchCondition rBranchOpCond, TR::InstOpCode::S390BranchCondition &retBranchOpCond, TR::LabelSymbol *branchTarget = NULL)
   {
   TR::Compilation *comp = cg->comp();
   bool isForward = true;
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * firstChild = node->getFirstChild();
   TR::InstOpCode::S390BranchCondition newBranchOpCond = TR::InstOpCode::COND_NOP;
   TR::Instruction * returnInstruction = NULL;
   bool isBranchGenerated = false;

   TR::DataType dataType = firstChild->getDataType();

   if (TR::Float == dataType || TR::Double == dataType)
      return generateS390FloatCompareAndBranchOps(node, cg, fBranchOpCond, rBranchOpCond, retBranchOpCond, branchTarget);
#ifdef J9_PROJECT_SPECIFIC
   else if (dataType == TR::DecimalDouble || dataType == TR::DecimalLongDouble)
      return generateS390DFPLongDoubleCompareAndBranchOps(node, cg, fBranchOpCond, rBranchOpCond, retBranchOpCond, branchTarget);
   else if (TR::PackedDecimal == dataType)
      return generateS390PackedCompareAndBranchOps(node, cg, fBranchOpCond, rBranchOpCond, retBranchOpCond, branchTarget);
#endif
   if (dataType == TR::Aggregate)
      {
      TR_ASSERT( 0, "unexpected data size for aggregate compare\n");
      }

   bool isUnsignedCmp = node->getOpCode().isUnsignedCompare() || dataType == TR::Address;
   bool isAOTGuard = false;
   // If second node is aconst and it asks for relocation record for AOT, we should provide it even if not guarded.
   if (cg->profiledPointersRequireRelocation() &&
       secondChild->getOpCodeValue() == TR::aconst &&
       (secondChild->isMethodPointerConstant() || secondChild->isClassPointerConstant()))
      {
      TR_ASSERT(!(node->isNopableInlineGuard()),"Should not evaluate class or method pointer constants underneath NOPable guards as they are runtime assumptions handled by virtualGuardHelper");
      // make sure aconst has been evaluated so relocation record already been created for it
      if (secondChild->getRegister() != NULL)
         isAOTGuard = true;

      /* This is to prevent this clobbering
       *
       * LGRL      R8,LitPool = 0xffffffffffffffff << Disabled Guard
       * LLGF      R8,0(,ReceiverObject)           << Guard Clobered
       * NILL      R8,0xFF00
       * LG        R8,VFTMethodOffset(,R8)
       * CLGFI     R8,J9Method
       * BNE
       *
       * For the moment, will disable generation of CLGFI for this case. Proper fix is to
       * create a relocation record on the immediate in the CLGFI. Proper fix also needs to
       * take out the forced evaluation of second child in generateS390CompareBranch (hint:
       * search for profiledPointersRequireRelocation)
       */
      }


   // If one of the children is a constant, we can potentially use the SI and the RI form
   // instructions
   //
   if ((secondChild->getOpCode().isLoadConst() || firstChild->getOpCode().isLoadConst()) &&
         /*FIXME: remove */!isAOTGuard)
      {
      TR::Register * testRegister;
      TR::Node * constNode;
      TR::Node * nonConstNode;

      if (firstChild->getOpCode().isLoadConst())
         {
         constNode = firstChild;
         nonConstNode = secondChild;
         isForward = false;
         }
      else
         {
         constNode = secondChild;
         nonConstNode = firstChild;
         isForward = true;
         }

      TR::DataType constType = constNode->getDataType();

      bool byteAddress = false;
      bool is64BitData = dataType == TR::Int64;
      bool isIntConst =  (constNode->getOpCode().isInt() || constNode->getOpCode().isLong());

      bool loadCCForArraycmp = false;

      // Try to see if we can use store-immediate form instructions
      // These are a good idea if one child is a memory reference that
      // doesn't require long-displacement or index registers
      //
      if (tryGenerateSIComparisons(node, constNode, nonConstNode, cg, false))
         {
         // yay! success.. nothing else to do
         }

      // ifxcmpyy
      //   xload        ; unevalauted int32 or int64, not restricted to register
      //   xconst 0
      // Try to generate Load-and-Test, or ICM style opcodes.
      //
      else if (nonConstNode->getOpCode().isLoadVar() &&
          nonConstNode->getRegister() == NULL &&
          !nonConstNode->getOpCode().isFloat() &&
          !nonConstNode->getOpCode().isByte() &&
          !nonConstNode->getOpCode().isShort() &&
          !isUnsignedCmp &&
          !nonConstNode->getOpCode().isDouble() &&
          getIntegralValue(constNode) == 0 &&
          (!nonConstNode->isExtendedTo64BitAtSource() || cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) ))
         {
         TR::SymbolReference * symRef = nonConstNode->getSymbolReference();
         TR::Symbol * symbol = symRef->getSymbol();
         bool useLTG = (cg->comp()->target().is64Bit() && (constType == TR::Int64 || constType == TR::Address)) ||
                       (cg->comp()->target().is32Bit() && (constType == TR::Int64));

         if (nonConstNode->getDataType() == TR::Address &&
             !symbol->isInternalPointer() &&
             !symbol->isNotCollected()    &&
             !symbol->isAddressOfClassObject())
            {
            testRegister = cg->allocateCollectedReferenceRegister();
            }
         else
            {
            testRegister = cg->allocateRegister();
            }

         TR::MemoryReference * tempMR = generateS390MemoryReference(nonConstNode, cg);
         bool mustExtend = !useLTG && nonConstNode->isExtendedTo64BitAtSource();

         if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && mustExtend)
            {
            if (isUnsignedCmp)
               {
               generateRXInstruction(cg, TR::InstOpCode::LLGF, nonConstNode, testRegister, tempMR);  // 64 bits zero extended
               generateRRInstruction(cg, TR::InstOpCode::LTGR, node, testRegister, testRegister);     // 64 bit reg test
               if (nonConstNode->isSignExtendedTo64BitAtSource())
                  {
                  generateRRInstruction(cg, TR::InstOpCode::LGFR, node, testRegister, testRegister);  // 64 bits sign extended
                  }
               }
            else
               {
               if (nonConstNode->isSignExtendedTo64BitAtSource())
                  {
                  generateRXInstruction(cg, TR::InstOpCode::LTGF, nonConstNode, testRegister, tempMR); // 64 bits sign extended and test
                  }
               else
                  {
                  generateRXInstruction(cg, TR::InstOpCode::LLGF, nonConstNode, testRegister, tempMR); // 64 bits zero extended
                  generateRRInstruction(cg, TR::InstOpCode::LTR, node, testRegister, testRegister);     // 32 bits compare
                  }
               }
            }
         // Can replace L/CHI instructions with LT (31bit) or LTG (64bit)
         else if (!mustExtend)
            {
            // Use LT if constant is 31bit, and only use LTG if constant is 64 bit
            generateRXInstruction(cg, (useLTG) ? TR::InstOpCode::LTG : TR::InstOpCode::LT, nonConstNode, testRegister, tempMR);
            }
         else
            {
            TR_ASSERT(  cg->comp()->target().is32Bit(), "ICM can be used for 32bit code-gen only!");
            if (cg->comp()->target().is32Bit() && (constType == TR::Int64))
               {
               generateRXInstruction(cg, TR::InstOpCode::LG, nonConstNode, testRegister, tempMR);
               if (branchTarget != NULL)
                  {
                  returnInstruction = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, testRegister, (int32_t)0, isForward?fBranchOpCond:rBranchOpCond, branchTarget, false, true);
                  isBranchGenerated = true;
                  }
               else
                  {
                  generateRIInstruction(cg, TR::InstOpCode::CGHI, node, testRegister, (int32_t) 0);
                  }
               }
            else if (tempMR->getIndexRegister())
               {
               TR::InstOpCode::Mnemonic load = TR::InstOpCode::L;

               if (nonConstNode->isSignExtendedTo64BitAtSource())
                  {
                  load = TR::InstOpCode::LGF;
                  }

               if (nonConstNode->isZeroExtendedTo64BitAtSource())
                  {
                  load = TR::InstOpCode::LLGF;
                  }

               generateRXInstruction(cg, load, nonConstNode, testRegister, tempMR);

               if (branchTarget != NULL)
                  {
                  returnInstruction = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, testRegister, (int32_t)0, isForward?fBranchOpCond:rBranchOpCond, branchTarget, false, true);
                  isBranchGenerated = true;
                  }
               else
                  {
                  generateRIInstruction(cg, TR::InstOpCode::CHI, node, testRegister, (int32_t) 0);
                  }
               }
            else
               {
               generateRSInstruction(cg, TR::InstOpCode::ICM, nonConstNode, testRegister, (uint32_t) 0xF, tempMR);
               }
            }

         updateReferenceNode(nonConstNode, testRegister);
         nonConstNode->setRegister(testRegister);
         tempMR->stopUsingMemRefRegister(cg);
         }
      else
         {
         if (nonConstNode->getRegister() != NULL)
            {
            testRegister = nonConstNode->getRegister();
            }
         else
            {
            //If this is equality (inequality) compare and the compare value is 0
            //and arraycmp or arraycmpwithPad is another child, then no need to do that
            //1. load CC to result register and
            //2. Compare the result register with the compare value.
            //The CC from arraycmp is the CC desired.

            if ((nonConstNode->getOpCodeValue() == TR::arraycmp || nonConstNode->getOpCodeValue() == TR::arraycmpWithPad) &&
                  (fBranchOpCond == TR::InstOpCode::COND_BE || fBranchOpCond == TR::InstOpCode::COND_BNE ) &&
                  (constNode->getIntegerNodeValue<int64_t>() == 0))
               {
               bool isEqualCmp = fBranchOpCond == TR::InstOpCode::COND_BE;
               //int compareVal = 0;

               if (nonConstNode->getOpCodeValue() == TR::arraycmp)
                  {
                  TR::TreeEvaluator::arraycmpHelper(nonConstNode,
                        cg,
                        false,      //isWideChar
                        isEqualCmp, //isEqualCmp
                        0,          //cmpValue.
                        NULL,       //compareTarget
                        NULL,       //ificmpNode
                        false,      //needResultReg
                        true,       //return102
                        &newBranchOpCond);

                  }
               else
                  {
                  arraycmpWithPadHelper helper(nonConstNode,
                        cg,
                        false,   //isUTF16
                        false,   //isFoldedIf
                        node,    //ificmpNode
                        false,   //needResultReg
                        &newBranchOpCond);
                  helper.generate();
                  }
               testRegister = NULL;
               loadCCForArraycmp = true;
               }
            else
               {
               testRegister = cg->evaluate(nonConstNode);
               }
            }

         bool isAddress = constType == TR::Address;
         if (constType == TR::Address)
            {
            constType = (cg->comp()->target().is64Bit() ? TR::Int64 : TR::Int32);
            isUnsignedCmp = true;
            }

         TR::InstOpCode::Mnemonic compareOpCode = TR::InstOpCode::BAD;
         int64_t constValue64 = 0;
         int32_t constValue32 = 0;
         bool useConstValue64 = false;
         switch (constType)
            {
            case TR::Int64:
            case TR::Address:
               {
               uint64_t uconstValue = static_cast<uint64_t>(constNode->getLongInt());
               constValue64 = static_cast<int64_t>(uconstValue);
               useConstValue64 = true;
               compareOpCode = isUnsignedCmp ? TR::InstOpCode::CLG : TR::InstOpCode::CG;
               break;
               }
            case TR::Int32:
               constValue32 = isAddress ? static_cast<uintptr_t>(constNode->getAddress()) : constNode->getInt();
               compareOpCode = isUnsignedCmp ? TR::InstOpCode::CL : TR::InstOpCode::C;
               break;

            case TR::Int16:
            case TR::Int8:
               constValue32 = getIntegralValue(constNode);
               compareOpCode = isUnsignedCmp ? TR::InstOpCode::CL : TR::InstOpCode::C;
               break;
            default:
               TR_ASSERT(0, "generateS390CompareOps: Unexpected Type\n");
               break;
            }

         if (loadCCForArraycmp)
            {
            //Need to do the same to avoid redundant comparison at other places in the enclosing switch block.
            }
         else if (branchTarget != NULL)
            {
            if (useConstValue64)
               returnInstruction = generateS390CompareAndBranchInstruction(cg, compareOpCode, node, testRegister, constValue64, isForward?fBranchOpCond:rBranchOpCond, branchTarget, false, true);
            else
               returnInstruction = generateS390CompareAndBranchInstruction(cg, compareOpCode, node, testRegister, constValue32, isForward?fBranchOpCond:rBranchOpCond, branchTarget, false, true);
            isBranchGenerated = true;
            }
         else
            {
            if ((constType == TR::Int64 || constType == TR::Address) && constNode->getRegister())
               generateRRInstruction(cg, isUnsignedCmp ? TR::InstOpCode::CLGR : TR::InstOpCode::CGR, node, testRegister, constNode->getRegister());
            else if (useConstValue64)
               generateS390ImmOp(cg, compareOpCode, node, testRegister, testRegister, constValue64);
            else
               generateS390ImmOp(cg, compareOpCode, node, testRegister, testRegister, constValue32);
            }
         }
      }

   // Try and see if we can use a CLC to do the comparison. They are nice because
   // we save registers for the operands. However they should not be used if
   // index registers are needed for the memory reference or the memrefs may
   // require long displacements.
   //
   else if (tryGenerateCLCForComparison(node, cg))
      {
      // yay! success.. nothing else to do
      }

   else if (tryGenerateConversionRXComparison(node, cg, &isForward))
      {
      // yay! success.. nothing else to do
      }

   // ificmpyy
   //   c2i        ; refcnt-1, unevaluated
   //     icload   ; evaluated
   //   c2i        ; refcnt-1, unevaluated
   //     icload   ; evaluated
   //
   // Try to generate a CR.
   // FIXME: can't the binary commutative analyser handle this? that's where this should be done
   //            what about su2i? s2i? mixing of these? bytes? ints? longs?
   //
   else if (firstChild->getOpCodeValue()==TR::su2i && firstChild->getRegister()==NULL && firstChild->getReferenceCount()==1 &&
            firstChild->getFirstChild()->getOpCodeValue()==TR::cloadi && firstChild->getFirstChild()->getRegister() &&
            secondChild->getOpCodeValue()==TR::su2i && secondChild->getRegister()==NULL && secondChild->getReferenceCount()==1 &&
            secondChild->getFirstChild()->getOpCodeValue()==TR::cloadi && secondChild->getFirstChild()->getRegister())
      {
      if (branchTarget != NULL)
         {
         returnInstruction = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, firstChild->getFirstChild()->getRegister(), secondChild->getFirstChild()->getRegister(), isForward?fBranchOpCond:rBranchOpCond, branchTarget, false, true);
         isBranchGenerated = true;
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::CR, node, firstChild->getFirstChild()->getRegister(), secondChild->getFirstChild()->getRegister());
         }
      firstChild->setRegister(firstChild->getFirstChild()->getRegister());
      secondChild->setRegister(secondChild->getFirstChild()->getRegister());

      cg->decReferenceCount(firstChild->getFirstChild());
      cg->decReferenceCount(secondChild->getFirstChild());
      }

   // ificmpyy
   //   bu2i               ; refcnt-1, unevaluated
   //     ibuload/iRegLoad ; evaluated
   //   bu2i               ; refcnt-1, unevaluated
   //     ibuload/iRegLoad ; evaluated
   //
   // Try to use a CR.
   // FIXME: can't the binary commutative analyser handle this? that's where this should be done
   //
   else if (firstChild->getOpCodeValue()==TR::bu2i && firstChild->getRegister()==NULL && firstChild->getReferenceCount()==1 &&
            (firstChild->getFirstChild()->getOpCodeValue()==TR::buloadi   ||
             firstChild->getFirstChild()->getOpCodeValue()==TR::iRegLoad)
            && firstChild->getFirstChild()->getRegister() &&
            secondChild->getOpCodeValue()==TR::bu2i && secondChild->getRegister()==NULL && secondChild->getReferenceCount()==1 &&
            (secondChild->getFirstChild()->getOpCodeValue()==TR::buloadi  ||
             secondChild->getFirstChild()->getOpCodeValue()==TR::iRegLoad)
            && secondChild->getFirstChild()->getRegister())
      {
      if (branchTarget != NULL)
         {
         returnInstruction = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CR, node, firstChild->getFirstChild()->getRegister(), secondChild->getFirstChild()->getRegister(), isForward?fBranchOpCond:rBranchOpCond, branchTarget, false, true);
         isBranchGenerated = true;
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::CR, node, firstChild->getFirstChild()->getRegister(), secondChild->getFirstChild()->getRegister());
         }
      firstChild->setRegister(firstChild->getFirstChild()->getRegister());
      secondChild->setRegister(secondChild->getFirstChild()->getRegister());

      cg->decReferenceCount(firstChild->getFirstChild());
      cg->decReferenceCount(secondChild->getFirstChild());
      }

   // Else of all the special cases above. Try to use the Binary Commutative Analyser
   // to generate the opcode for us.
   //
   else
      {
      TR_S390BinaryCommutativeAnalyser temp(cg);
      TR_ASSERT_FATAL(firstChild->getDataType() == secondChild->getDataType(), "Data type of firstChild (%s) and secondChild (%s) should match for generating compare and branch", firstChild->getDataType().toString(), secondChild->getDataType().toString());

      // On 64-Bit platforms with compressed references it is possible that one (or both) of the children of the 
      // compare is loading a class from the object (VFT symbol). This symbol is specially treated within the JIT at
      // the moment because it is an `aloadi` which loads a 32-bit value (64-bit compressed references) and zero
      // extends it to a 64-bit address. Unfortunately the generic analyzers used below are unaware of this fact
      // and could end up generating an incorrect instruction to evaluate such a VFT load.
      //
      // To prevent such logic leaking into the generic analyzers (which should remain generic) we force the evaluation
      // of such VFT nodes here before calling out to the analyzer.
      if (TR::Compiler->target.is64Bit())
         {
         if (firstChild->getDataType() == TR::Address && cg->isCompressedClassPointerOfObjectHeader(firstChild))
            cg->evaluate(firstChild);
         if (secondChild->getDataType() == TR::Address && cg->isCompressedClassPointerOfObjectHeader(secondChild))
            cg->evaluate(secondChild);
         }

      switch (TR::DataType::getSize(firstChild->getDataType()))
         {
         case 8:
            temp.genericAnalyser (node, isUnsignedCmp ? TR::InstOpCode::CLGR : TR::InstOpCode::CGR,
                                 isUnsignedCmp ? TR::InstOpCode::CLG : TR::InstOpCode::CG,
                                 TR::InstOpCode::LGR, true, branchTarget, fBranchOpCond, rBranchOpCond);
            returnInstruction = cg->getAppendInstruction();
            isBranchGenerated = true;
            break;
         case 4:
            temp.genericAnalyser (node, isUnsignedCmp ? TR::InstOpCode::CLR : TR::InstOpCode::CR,
                                 isUnsignedCmp ? TR::InstOpCode::CL : TR::InstOpCode::C,
                                 TR::InstOpCode::LR, true, branchTarget, fBranchOpCond, rBranchOpCond);
            returnInstruction = cg->getAppendInstruction();
            isBranchGenerated = true;
            break;
         case 2:
            temp.genericAnalyser(node, isUnsignedCmp ? TR::InstOpCode::CLR : TR::InstOpCode::CR,
                                 isUnsignedCmp ? TR::InstOpCode::CL : TR::InstOpCode::CH,
                                 TR::InstOpCode::LR, true, branchTarget, fBranchOpCond, rBranchOpCond);
            returnInstruction = cg->getAppendInstruction();
            isBranchGenerated = true;
            break;
         default:
            TR_ASSERT_FATAL( 0, "generateS390CompareOps: Unexpected child type (%s) with size = %d\n", firstChild->getDataType().toString(), TR::DataType::getSize(firstChild->getDataType()));
            break;
         }

         isForward = (temp.getReversedOperands() == false);
         // There should be no register attached to the compare node, otherwise
         // the register is kept live longer than it should.
         node->unsetRegister();
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   TR::InstOpCode::S390BranchCondition returnCond;

   if (newBranchOpCond!=TR::InstOpCode::COND_NOP)
      returnCond = (newBranchOpCond);
   else
      returnCond = (isForward? fBranchOpCond : rBranchOpCond);

   // logical instruction condition code reused, need to adjust the condition mask
   if (cg->hasCCInfo() && cg->ccInstruction()->getOpCode().setsCarryFlag())
      {
      TR_ASSERT( (returnCond == TR::InstOpCode::COND_BE || returnCond == TR::InstOpCode::COND_BNE), "Unexpected branch condition when reusing logical CC: %d\n", returnCond);
      if (returnCond == TR::InstOpCode::COND_BE)
         returnCond = TR::InstOpCode::COND_MASK10;
      else if (returnCond == TR::InstOpCode::COND_BNE)
         returnCond = TR::InstOpCode::COND_MASK5;
      }

   // Generate the branch if required.
   if (branchTarget != NULL && !isBranchGenerated)
      {
      returnInstruction = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, returnCond, node, branchTarget);
      }

   // Set the return Branch Condition
   retBranchOpCond = returnCond;

   return returnInstruction;
   }



TR::InstOpCode::S390BranchCondition
generateS390CompareOps(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond)
   {
   TR::InstOpCode::S390BranchCondition returnCond;
   generateS390CompareAndBranchOpsHelper(node, cg, fBranchOpCond, rBranchOpCond, returnCond);
   return returnCond;
   }

TR::Instruction *
generateS390CompareOps(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond, TR::LabelSymbol * targetLabel)
   {
   TR::InstOpCode::S390BranchCondition returnCond;
   TR::Instruction * cursor = generateS390CompareAndBranchOpsHelper(node, cg, fBranchOpCond, rBranchOpCond, returnCond, targetLabel);

   return cursor;
   }

/**
 * Generate code to perform a comparison that returns 1 or 0
 * The comparisson type is determined by the choice of CMP operators:
 *   - fBranchOp:  Operator used for forward operation ->  A fCmp B
 *   - rBranchOp:  Operator user for reverse operation ->  B rCmp A <=> A fCmp B
 */
TR::Register *
generateS390CompareBool(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond,
   bool isUnorderedOK)
   {
   TR::Instruction * cursor = NULL;

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);


   // Create a label
   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
   // Create a register
   TR::Register * targetRegister = cg->allocateRegister();


   // Assume the answer is TRUE
   generateLoad32BitConstant(cg, node, 1, targetRegister, true);

   // Generate compare code, find out if ops were reversed


   if (isUnorderedOK)
      {
      TR::InstOpCode::S390BranchCondition branchOpCond =
         generateS390CompareOps(node, cg, fBranchOpCond, rBranchOpCond);
      uint8_t branchMask = getMaskForBranchCondition(branchOpCond);
      branchMask += 0x01;
      branchOpCond = getBranchConditionForMask(branchMask);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      generateS390BranchInstruction(cg, branchOp, branchOpCond, node, cFlowRegionEnd);
      }
   else
      {
      cursor = generateS390CompareOps(node, cg, fBranchOpCond, rBranchOpCond, cFlowRegionEnd);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, cursor->getPrev());
      // generateS390CompareOps may generate separate compare and branch instruction.
      // In this case the cursor points to the branch instruction, and the compare is
      // placed outside of ICF.
      if(cursor->isCompare())
         {
         // CompareAndBranch instruction is generated and comparison is inside the ICF.
         // So the postconditions should be inserted.
         // We expect children to return a register (getRegister()) or be constant node (CIJ,CGIJ).
         if(node->getFirstChild()->getRegister())
            deps->addPostConditionIfNotAlreadyInserted(node->getFirstChild()->getRegister(), TR::RealRegister::AssignAny);
         else
            TR_ASSERT(node->getFirstChild()->getOpCode().isLoadConst(),"Expect an evaluated node with a register output or a constant node");

         if(node->getSecondChild()->getRegister())
            deps->addPostConditionIfNotAlreadyInserted(node->getSecondChild()->getRegister(), TR::RealRegister::AssignAny);
         else
            TR_ASSERT(node->getSecondChild()->getOpCode().isLoadConst(),"Expect an evaluated node with a register output or a constant node");
         }
      }

   cFlowRegionStart->setStartInternalControlFlow();

#ifdef J9_PROJECT_SPECIFIC
   //DAA related:
   //in the case of packed decimal comparision, we also need to add th targetRegister
   //to the label we generate at the very end of BCDCHK. The reason is that between the point of
   //TR::InstOpCode::CP and the return point of OOL path, targetRegister is used. And we are potentially
   //jumping from the point of TR::InstOpCode::CP to the end label of BCDCHK.
   //
   //we check 2 criteria: 1. it must under BCDCHK
   //and                  2. it must be a pdcmpxx.
   TR::Node * BCDCHKNode;
   if (((BCDCHKNode = cg->getCurrentCheckNodeBeingEvaluated())
       && BCDCHKNode->getOpCodeValue() == TR::BCDCHK) &&
       node->getFirstChild()->getDataType() == TR::PackedDecimal)
      {
      cg->getCurrentCheckNodeRegDeps()->addPostConditionIfNotAlreadyInserted(targetRegister,
                                                                                     TR::RealRegister::AssignAny);
      }
#endif

   deps->addPostConditionIfNotAlreadyInserted(targetRegister, TR::RealRegister::AssignAny);

   // Set the answer to FALSE
   generateLoad32BitConstant(cg, node, 0, targetRegister, true, NULL, deps, NULL);

   // DONE
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
   cFlowRegionEnd->setEndInternalControlFlow();

   node->setRegister(targetRegister);
   return targetRegister;

   }

TR::InstOpCode::Mnemonic
getOpCodeIfSuitableForCompareAndBranch(TR::CodeGenerator * cg, TR::Node * node, TR::DataType dataType, bool canUseImm8 )
   {
   // be pessimistic and signal we can't use compare and branch until we
   // determine otherwise.
   TR::InstOpCode::Mnemonic opCodeToUse = TR::InstOpCode::BAD;

   bool isUnsignedCmp = node->getOpCode().isUnsignedCompare();
   if (dataType == TR::Address)
      {
      dataType = (cg->comp()->target().is64Bit() ? TR::Int64 : TR::Int32);
      isUnsignedCmp = true;
      }

   switch (dataType)
      {
      case TR::Int64:
         if (isUnsignedCmp)
            {
            opCodeToUse = canUseImm8 ? TR::InstOpCode::CLGIJ : TR::InstOpCode::CLGRJ;
            }
         else
            {
            opCodeToUse = canUseImm8 ? TR::InstOpCode::CGIJ : TR::InstOpCode::CGRJ;
            }
         break;
      case TR::Int32:
      case TR::Int16:
      case TR::Int8:
         if (isUnsignedCmp)
            {
            opCodeToUse = canUseImm8 ? TR::InstOpCode::CLIJ : TR::InstOpCode::CLRJ;
            }
         else
            {
            opCodeToUse = canUseImm8 ? TR::InstOpCode::CIJ : TR::InstOpCode::CRJ;
            }
         break;
      case TR::Float:
      case TR::Double:
#ifdef J9_PROJECT_SPECIFIC
      case TR::DecimalFloat:
      case TR::DecimalDouble:
      case TR::DecimalLongDouble:
#endif
      case TR::Aggregate:
         break;
     default:
         TR_ASSERT( 0, "genCompareAndBranchIfPossible: Unexpected Type\n");
         break;
      }

   return opCodeToUse;
   }


/**
 * This method generates z6 compare and branch instruction if it can, otherwise returns false
 */
TR::Instruction *
genCompareAndBranchInstructionIfPossible(TR::CodeGenerator * cg, TR::Node * node, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond, TR::RegisterDependencyConditions *deps)
   {
   static char * disableCompareAndBranch = feGetEnv("TR_DISABLES390CompareAndBranch");
   TR::Compilation *comp = cg->comp();

   if (comp->getOption(TR_DisableCompareAndBranchInstruction) || disableCompareAndBranch)
        return NULL;

   // be pessimistic and signal we can't use compare and branch until we
   // determine otherwise.
   TR::InstOpCode::Mnemonic opCodeToUse = TR::InstOpCode::BAD;

   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * thirdChild = (node->getNumChildren() == 3)? node->getChild(2) : NULL;
   bool isUnsignedCmp = node->getOpCode().isUnsignedCompare();
   bool canUseImm8 = (secondChild->getOpCode().isLoadConst() || firstChild->getOpCode().isLoadConst());
   bool isForward = (secondChild->getOpCode().isLoadConst());
   TR::Node * constNode = isForward ? secondChild : firstChild ;
   TR::Node * nonConstNode = isForward ? firstChild : secondChild;
   TR::InstOpCode::S390BranchCondition branchCond = isForward ? fBranchOpCond : rBranchOpCond ;
   TR::Instruction * cursor = NULL;
   int64_t value ;

   // get a label for our destination
   TR::LabelSymbol * dest = node->getBranchDestination()->getNode()->getLabel();

   bool isBranchNodeInColdBlock = cg->getCurrentEvaluationTreeTop()->getEnclosingBlock()->isCold();
   bool isBranchDestinationNodeInColdBlock = node->getBranchDestination()->getNode()->getBlock()->isCold();

   bool isBranchToNodeWithDifferentHotnessLevel = ( isBranchNodeInColdBlock && !isBranchDestinationNodeInColdBlock ) ||
                                                   (!isBranchNodeInColdBlock && isBranchDestinationNodeInColdBlock ) ;
   // Don't bother if branch is from hot to cold or from cold to hot
   // As the 16 bit relative offset will not be enough , and we will end up replacing compare and branch with
   // explicit compare followed by branch
   // As warm cache can contain cold blocks
   // what we really need is a way to know if the two blocks are in same warm/cold cache
   // but there is no mechanism currently to find cache residency for the block.
   if (isBranchToNodeWithDifferentHotnessLevel)
      return NULL;

   TR::DataType dataType = constNode->getDataType();
   if (canUseImm8)
        {

        //traceMsg(comp,"canUseImm8 is true.  isIntegral for constNode %p is %d  isAddress = %d\n",constNode,constNode->getType().isIntegral(),constNode->getType().isAddress());

        if (constNode->getType().isIntegral())
           {
           value = getIntegralValue(constNode);

           if (isUnsignedCmp)
               canUseImm8 = ((uint64_t)value <= TR::getMaxUnsigned<TR::Int8>());
           else
               canUseImm8 = ((value >= MIN_IMMEDIATE_BYTE_VAL) && (value <= MAX_IMMEDIATE_BYTE_VAL));
           }
        else if (constNode->getType().isAddress() && constNode->getAddress()== 0)
           {
           value = 0;
           }
        else
           canUseImm8 = false;
        }

   if (constNode->getOpCode().isIntegralConst() &&
       tryGenerateSIComparisons(node, constNode, nonConstNode, cg, true))
      {
      // Where possible, using SI is better than using compare and branch
      return NULL;
      }

   opCodeToUse = getOpCodeIfSuitableForCompareAndBranch(cg, node, dataType, canUseImm8 );

   if (opCodeToUse == TR::InstOpCode::BAD)
      {
      return NULL;
      }
   else if (canUseImm8)
      {
      TR::Register * targetReg = cg->evaluate(nonConstNode);

      //Evaluate GlRegDeps before branch
      if (thirdChild)
         {
         cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(cg, thirdChild, 1);
         cg->decReferenceCount(thirdChild);

         // We need to add the regs used by the comp-and-branch inst to the
         // glregdeps as the live ranges could interfere causing the constraints
         // imposed by glRegDeps to be undone
         //
         deps->addPostConditionIfNotAlreadyInserted(targetReg, TR::RealRegister::AssignAny);
         }

      cursor = generateRIEInstruction(cg, opCodeToUse, node, targetReg, (int8_t)value, dest, branchCond);
      }
   else
      {
      TR::Register * srcReg = constNode->getRegister();
      if (srcReg == NULL )
         {
         if (constNode->getReferenceCount() > 1 || !constNode->getOpCode().isLoad())
            srcReg = cg->evaluate(constNode);
         }
      if (srcReg != NULL)
         {
         TR::Register * targetReg = cg->evaluate(nonConstNode);

         //Evaluate GlRegDeps before branch
         if (thirdChild)
            {
            cg->evaluate(thirdChild);
            deps = generateRegisterDependencyConditions(cg, thirdChild, 2);
            cg->decReferenceCount(thirdChild);

            // We need to add the regs used by the comp-and-branch inst to the
            // glregdeps as the live ranges could interfere causing the constraints
            // imposed by glRegDeps to be undone
            //
            deps->addPostConditionIfNotAlreadyInserted(srcReg, TR::RealRegister::AssignAny);
            deps->addPostConditionIfNotAlreadyInserted(targetReg, TR::RealRegister::AssignAny);
            }

         cursor = generateRIEInstruction(cg, opCodeToUse, node, targetReg, srcReg, dest, branchCond);
         }
      else
         {
         return NULL;
         }
      }

   if (cursor)
      {
      if (deps)
         {
         cursor->setDependencyConditions(deps);
         }
      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }

   return cursor;
   }

/** \brief
 *    Converts a comparison branch condition to the correct branch condition for the test under mask instruction
 *    being generated.
 *
 *  \param constNode
 *    The constant the comparison branch condition is comparing against.
 *
 *  \param brCond
 *    The comparison branch condition to be converted.
 *
 *  \return newCond
 *    The branch condition to be used in generated test under mask instruction.
 */
TR::InstOpCode::S390BranchCondition convertComparisonBranchConditionToTestUnderMaskBranchCondition(TR::Node * constNode, TR::InstOpCode::S390BranchCondition brCond)
   {

   TR_ASSERT((constNode->getOpCode().isLoadConst()), "Expected constNode %p to be constant\n", constNode);

   TR::InstOpCode::S390BranchCondition newCond = brCond;
   if (getIntegralValue(constNode) == 0)
      {
      if (getBranchConditionForMask(getMaskForBranchCondition(brCond)) == TR::InstOpCode::COND_MASK8)
         {
         newCond = TR::InstOpCode::COND_BZ;
         }
      else
         {
         newCond = TR::InstOpCode::COND_MASK7;
         }
      }
   else if (getBranchConditionForMask(getMaskForBranchCondition(brCond)) == TR::InstOpCode::COND_MASK8)
      {
      newCond = TR::InstOpCode::COND_BO;
      }
   else
      {
      newCond = TR::InstOpCode::COND_BNO;
      }

   return newCond;

   }

TR::InstOpCode::S390BranchCondition
generateTestUnderMaskIfPossible(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * constNode = NULL;
   TR::Node * nonConstNode = NULL;
   TR::Node * memRefNode = NULL;
   TR::InstOpCode::S390BranchCondition newBranchOpCond = TR::InstOpCode::COND_NOP;
   bool isForward;

   if (secondChild->getOpCode().isLoadConst() || firstChild->getOpCode().isLoadConst())
      {
      if (firstChild->getOpCode().isLoadConst())
         {
         constNode = firstChild;
         nonConstNode = secondChild;
         isForward = false;
         }
      else
         {
         constNode = secondChild;
         nonConstNode = firstChild;
         isForward = true;
         }
      }

   /* ======= Variables for Pattern Matching for Byte Testing ======= */

   // Number of nodes from the nonConstNode to reach a memory reference
   int32_t depth = 0;

   /* Reference count should be 0 for all the nodes from nonConstNode
    * until the memory reference in order to evaluate them together*/
   bool refCountZero = false;

   uint64_t nodeVal = 0; //For nonConstNode->getSecondChild()
   uint32_t byteNum = 0; //Byte number to test
   bool byteCheckForTM = true; //Test if only one byte needs to be checked

   if (nonConstNode && nonConstNode->getNumChildren() > 0)
      {
      depth++;
      if (nonConstNode->getFirstChild()->getOpCode().isMemoryReference() &&
            nonConstNode->getFirstChild()->getReferenceCount() == 1)
         {
         refCountZero = true;
         memRefNode = nonConstNode->getFirstChild();
         }
      else if ((nonConstNode->getFirstChild()->getOpCodeValue() == TR::bu2i ||
            nonConstNode->getFirstChild()->getOpCodeValue() == TR::s2i ||
            nonConstNode->getFirstChild()->getOpCodeValue() == TR::b2i ||
            nonConstNode->getFirstChild()->getOpCodeValue() == TR::i2l)
            && nonConstNode->getFirstChild()->getReferenceCount() == 1)
         {
         depth++;
         if (nonConstNode->getFirstChild()->getFirstChild()->getOpCode().isMemoryReference() &&
               nonConstNode->getFirstChild()->getFirstChild()->getReferenceCount() == 1)
            {
            refCountZero = true;
            memRefNode = nonConstNode->getFirstChild()->getFirstChild();
            }
         else if ((nonConstNode->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::s2i ||
               nonConstNode->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::b2i ||
               nonConstNode->getFirstChild()->getFirstChild()->getOpCodeValue() == TR::i2l)
               && nonConstNode->getFirstChild()->getFirstChild()->getReferenceCount() == 1)
            {
            depth++;
            if (nonConstNode->getFirstChild()->getFirstChild()->getFirstChild()->getOpCode().isMemoryReference() &&
                  nonConstNode->getFirstChild()->getFirstChild()->getFirstChild()->getReferenceCount() == 1)
               {
               refCountZero = true;
               memRefNode = nonConstNode->getFirstChild()->getFirstChild()->getFirstChild();
               }
            }
         }
      }

   if (nonConstNode != NULL && nonConstNode->getOpCode().isAnd() && nonConstNode->getSecondChild()->getOpCode().isLoadConst())
      {
      nodeVal = nonConstNode->getSecondChild()->get64bitIntegralValueAsUnsigned();

      /* Pattern Matching for Byte Testing: Identifying if we're just testing a single byte. If so, we can use
       * in-memory TM + BRC sequence to do that. Below, we're trying to find the byte to test
       * which would be used later in the TM instruction. */

      if ((nodeVal & 0xFFFFFFFFFFFFFF00ULL) == 0)
         {
         byteNum = 0;
         }
      else if ((nodeVal & 0xFFFFFFFFFFFF00FFULL) == 0)
         {
         byteNum = 1;
         }
      else if ((nodeVal & 0xFFFFFFFFFF00FFFFULL) == 0)
         {
         byteNum = 2;
         }
      else if ((nodeVal & 0xFFFFFFFF00FFFFFFULL) == 0)
         {
         byteNum = 3;
         }
      else if ((nodeVal & 0xFFFFFF00FFFFFFFFULL) == 0)
         {
         byteNum = 4;
         }
      else if ((nodeVal & 0xFFFF00FFFFFFFFFFULL) == 0)
         {
         byteNum = 5;
         }
      else if ((nodeVal & 0xFF00FFFFFFFFFFFFULL) == 0)
         {
         byteNum = 6;
         }
      else if ((nodeVal & 0x00FFFFFFFFFFFFFFULL) == 0)
         {
         byteNum = 7;
         }
      else
         {
         byteCheckForTM = false;
         }
      }

   if (constNode &&
            (node->getOpCodeValue() == TR::ificmpeq /*|| node->getOpCodeValue() == TR::ificmpne*/) &&
            getIntegralValue(constNode) == 0                                                 &&
            nonConstNode->getNumChildren() == 1                                              &&
            nonConstNode->getReferenceCount() == 1 && nonConstNode->getRegister() == NULL    &&
            nonConstNode->getFirstChild()->getReferenceCount() == 1                          &&
            nonConstNode->getFirstChild()->getRegister() == NULL                             &&
            memRefNode != NULL                                                               &&
            (nonConstNode->getFirstChild()->getOpCodeValue() == TR::buload ||
             nonConstNode->getFirstChild()->getOpCodeValue() == TR::bload  ||
             nonConstNode->getFirstChild()->getOpCodeValue() == TR::bloadi ||
             nonConstNode->getFirstChild()->getOpCodeValue() == TR::buloadi    )                   )
      {
      TR::MemoryReference * tempMR2 = NULL;
      TR::MemoryReference * tempMR = generateS390MemoryReference(nonConstNode->getFirstChild(), cg);

      if (tempMR->getIndexRegister() == NULL && tempMR->getIndexNode() == NULL)
         {
         tempMR2 = tempMR;
         }
      else
         {
         TR::Register * tempReg2 = cg->allocateRegister();
         tempMR2 = generateS390MemoryReference(tempReg2, 0, cg);
         generateRXInstruction(cg, TR::InstOpCode::LA, node, tempReg2, tempMR);
         cg->stopUsingRegister(tempReg2);
         }

      generateSIInstruction(cg, TR::InstOpCode::TM, node, tempMR2, (uint32_t) 0xFF);

      cg->decReferenceCount(nonConstNode->getFirstChild());

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      newBranchOpCond = (isForward? fBranchOpCond : rBranchOpCond);

      // change condition code to reflect comparing to zero
      newBranchOpCond = convertComparisonBranchConditionToTestUnderMaskBranchCondition(constNode, newBranchOpCond);
      }
   //Case: Pattern Matching for Byte Testing
   else if (node->getOpCode().isBooleanCompare() &&
           (TR::ILOpCode::isEqualCmp(node->getOpCode().getOpCodeValue()) ||
           TR::ILOpCode::isNotEqualCmp(node->getOpCode().getOpCodeValue())) &&
           constNode != NULL && nonConstNode != NULL &&
           nonConstNode->getOpCode().isAnd() &&
           byteCheckForTM && nonConstNode->getReferenceCount() == 1 &&
           nonConstNode->getRegister() == NULL                             &&
           nonConstNode->getSecondChild()->getOpCode().isLoadConst()       &&
           (getIntegralValue(constNode) == getIntegralValue(nonConstNode->getSecondChild()) ||
            getIntegralValue(constNode) ==0) && memRefNode && refCountZero &&
            memRefNode->getRegister() == NULL &&
            nonConstNode->getFirstChild()->getRegister() == NULL           &&
           fBranchOpCond == rBranchOpCond                                  &&
           (fBranchOpCond == TR::InstOpCode::COND_BE || fBranchOpCond == TR::InstOpCode::COND_BER ||
           fBranchOpCond == TR::InstOpCode::COND_BNE) || fBranchOpCond == TR::InstOpCode::COND_BNER)
          {
          /* ====== Some examples of Pattern Matching for Byte Testing ===== */

            /* Depth: Number of nodes from the nonConstNode to reach a memory reference */
            /* Depth of 1:

              iflcmpne/ificmpne
                 land/iand
                    lload/iload
                    lconst/iconst 4
                 lconst/iconst 0/4
            */

            /* Depth of 2:

              ificmpne
                 iand
                    s2i/b2i
                       sloadi/bloadi
                    iconst 4
                 lconst 0/4
            */

            /* Depth of 3:

               iflcmpne
                  land
                     i2l
                        s2i/b2i
                           sloadi/bloadi
                     lconst 4
                  lconst 0/4
            */

          TR::DebugCounter::incStaticDebugCounter(cg->comp(), "Z/optimization/ifcmpne/bittest");

          nodeVal = nodeVal >> (byteNum * 8);
          nodeVal = nodeVal & 0xff;
          int32_t lastByteInAddress = 0;

          switch(depth)
             {
          case 1:
             lastByteInAddress = nonConstNode->getFirstChild()->getSize() - 1;
             break;
          case 2:
             lastByteInAddress = nonConstNode->getFirstChild()->getFirstChild()->getSize() - 1;
             break;
          case 3:
             lastByteInAddress = nonConstNode->getFirstChild()->getFirstChild()->getFirstChild()->getSize() - 1;
             break;
          default:
             TR_ASSERT(false, "unsupported depth for pattern matching %d\n", depth);
             }

          byteNum = lastByteInAddress - byteNum;

          TR::MemoryReference * tempMR = generateS390MemoryReference(memRefNode, cg);
          TR::MemoryReference * tempMR2 = generateS390MemoryReference(*tempMR, byteNum, cg);
          generateSIInstruction(cg, TR::InstOpCode::TM, node, tempMR2, nodeVal);

          switch(depth)
             {
          case 1:
             cg->decReferenceCount(nonConstNode->getFirstChild());
             break;
          case 2:
             cg->decReferenceCount(nonConstNode->getFirstChild()->getFirstChild());
             cg->decReferenceCount(nonConstNode->getFirstChild());
             break;
          case 3:
             cg->decReferenceCount(nonConstNode->getFirstChild()->getFirstChild()->getFirstChild());
             cg->decReferenceCount(nonConstNode->getFirstChild()->getFirstChild());
             cg->decReferenceCount(nonConstNode->getFirstChild());
             break;
          default:
             TR_ASSERT(false, "unsupported depth for pattern matching %d\n", depth);
             }

          cg->decReferenceCount(nonConstNode->getSecondChild());
          cg->decReferenceCount(firstChild);
          cg->decReferenceCount(secondChild);

          newBranchOpCond = convertComparisonBranchConditionToTestUnderMaskBranchCondition(constNode, fBranchOpCond);
          }
   else if (constNode &&
         (node->getOpCodeValue()==TR::iflcmpeq ||
          node->getOpCodeValue()==TR::iflcmpne) &&
         constNode->getLongInt() == 0 &&
         nonConstNode->getOpCodeValue()==TR::land &&
         nonConstNode->getReferenceCount() == 1 && nonConstNode->getRegister()==NULL &&
         nonConstNode->getSecondChild()->getOpCode().isLoadConst() &&
        ((nonConstNode->getSecondChild()->getLongInt() & 0xFFFFFFFFFFFF0000) == 0 ||
         (nonConstNode->getSecondChild()->getLongInt() & 0xFFFFFFFF0000FFFF) == 0 ||
         (nonConstNode->getSecondChild()->getLongInt() & 0xFFFF0000FFFFFFFF) == 0 ||
         (nonConstNode->getSecondChild()->getLongInt() & 0x0000FFFFFFFFFFFF) == 0)
         )
      {
      //  (0)  iflcmpeq
      //  (1)    land
      //  (x)      nonConstNode
      //  (x)      lconst 1
      //  (x)    lconst 0

      TR::Register * tempReg = cg->evaluate(nonConstNode->getFirstChild());

      if ((nonConstNode->getSecondChild()->getLongInt() & 0xFFFFFFFFFFFF0000) == 0)
         generateRIInstruction(cg, TR::InstOpCode::TMLL, node, tempReg, nonConstNode->getSecondChild()->getLongInt()&0xFFFF);
      else if ((nonConstNode->getSecondChild()->getLongInt() & 0xFFFFFFFF0000FFFF) == 0)
         generateRIInstruction(cg, TR::InstOpCode::TMLH, node, tempReg, (nonConstNode->getSecondChild()->getLongInt()&0xFFFF0000)>>16);
      else if ((nonConstNode->getSecondChild()->getLongInt() & 0xFFFF0000FFFFFFFF) == 0)
         generateRIInstruction(cg, TR::InstOpCode::TMHL, node, tempReg, (nonConstNode->getSecondChild()->getLongInt()&0xFFFF00000000)>>32);
      else
         generateRIInstruction(cg, TR::InstOpCode::TMHH, node, tempReg, (nonConstNode->getSecondChild()->getLongInt()&0xFFFF000000000000)>>48);

      if (node->getOpCodeValue()==TR::iflcmpne)
         {
         newBranchOpCond = TR::InstOpCode::COND_MASK7;
         }
       else
         {
         newBranchOpCond = (isForward? fBranchOpCond : rBranchOpCond);

         // change condition code to reflect comparing to zero
         newBranchOpCond = convertComparisonBranchConditionToTestUnderMaskBranchCondition(constNode, newBranchOpCond);
         }

      cg->decReferenceCount(nonConstNode->getFirstChild());
      cg->decReferenceCount(nonConstNode->getSecondChild());

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else if (constNode &&
         (node->getOpCodeValue()==TR::ificmpeq ||
          node->getOpCodeValue()==TR::ificmpne) &&
         constNode->getInt() == 0 &&
         nonConstNode->getOpCodeValue()==TR::iand &&
         nonConstNode->getReferenceCount() == 1 && nonConstNode->getRegister()==NULL &&
         nonConstNode->getSecondChild()->getOpCode().isLoadConst() &&
         ((nonConstNode->getSecondChild()->getInt() & 0xFFFF0000) == 0 ||
          (nonConstNode->getSecondChild()->getInt() & 0x0000FFFF) == 0)
         )
      {
      //  (0)  ificmpeq
      //  (1)    iand
      //  (x)      nonConstNode
      //  (x)      iconst 15

      TR::Register * tempReg = cg->evaluate(nonConstNode->getFirstChild());

      if ((nonConstNode->getSecondChild()->getInt() & 0xFFFF0000) == 0)
          generateRIInstruction(cg, TR::InstOpCode::TMLL, node, tempReg, nonConstNode->getSecondChild()->getInt()&0xFFFF);
      else
          generateRIInstruction(cg, TR::InstOpCode::TMLH, node, tempReg, (nonConstNode->getSecondChild()->getInt()&0xFFFF0000)>>16);

      if (node->getOpCodeValue()==TR::ificmpne)
         {
         newBranchOpCond = TR::InstOpCode::COND_MASK7;
         }
       else
         {
         newBranchOpCond = (isForward? fBranchOpCond : rBranchOpCond);

         // change condition code to reflect comparing to zero
         newBranchOpCond = convertComparisonBranchConditionToTestUnderMaskBranchCondition(constNode, newBranchOpCond);
         }

      cg->decReferenceCount(nonConstNode->getFirstChild());
      cg->decReferenceCount(nonConstNode->getSecondChild());

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      }
   else if ((node->getOpCodeValue()==TR::ifbcmpeq ||
               node->getOpCodeValue()==TR::ifbcmpne) &&
          constNode != NULL && nonConstNode != NULL                       &&
          nonConstNode->getOpCodeValue() == TR::band                      &&
          nonConstNode->getReferenceCount() == 1                          &&
          nonConstNode->getRegister() == NULL                             &&
          nonConstNode->getSecondChild()->getOpCode().isLoadConst()       &&
         (constNode->getByte() == nonConstNode->getSecondChild()->getByte() ||
           getIntegralValue(constNode) ==0) &&
          fBranchOpCond == rBranchOpCond  &&
          (fBranchOpCond == TR::InstOpCode::COND_BE || fBranchOpCond == TR::InstOpCode::COND_BER ||
           fBranchOpCond == TR::InstOpCode::COND_BNE || fBranchOpCond == TR::InstOpCode::COND_BNER))
         {
         if (nonConstNode->getOpCodeValue() == TR::band)
            {
            TR::Register* tempReg = cg->evaluate(nonConstNode->getFirstChild());
            generateRIInstruction(cg, TR::InstOpCode::TMLL, node, tempReg, nonConstNode->getSecondChild()->getUnsignedByte());
            cg->decReferenceCount(nonConstNode->getFirstChild());
            }

         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         cg->decReferenceCount(nonConstNode->getSecondChild());

         newBranchOpCond = convertComparisonBranchConditionToTestUnderMaskBranchCondition(constNode, fBranchOpCond);
         }
   return newBranchOpCond;
   }

static bool
containsValidOpCodesForConditionalMoves(TR::Node* node, TR::CodeGenerator* cg)
   {

   // Conditional load/stores only supports simple variants (i.e. no array) due to no index register.
   if (node->getOpCode().isLoadConst())
      return true;
   // Requires additonal work to handle GlRegDep moves
   else if (node->getOpCodeValue() == TR::GlRegDeps)
      return false;
   // Array calculations are require index and base.  Will introduce additional instructions in stream.
   else if (node->getOpCodeValue() == TR::aladd || node->getOpCodeValue() == TR::aiadd)
      {
      // We can handle as long as it's folded into LA or memRef
      }
   else if (node->getOpCodeValue() == TR::aloadi)
      return false;
   else if (node->getOpCode().isLoad() || node->getOpCode().isStore())
      {
      // There are only 32-bit and 64-bit load/store on condition.
      // On 32-bit platforms, we should not generate a STM/LM on condition.
      if (!node->getType().isInt64() && !node->getType().isAddress() && !node->getType().isInt32())
         return false;
      if (cg->comp()->target().is32Bit() && node->getType().isInt64())
         return false;
      if (node->useSignExtensionMode() || (node->getOpCode().isLoad() && node->isLoadAndTest()))
         return false;

      TR::SymbolReference * symRef = node->getSymbolReference();
      TR::Symbol * symbol = symRef->getSymbol();

      // Indirect nodes are not safe.
      if (node->getOpCode().isIndirect() || node->getOpCode().isArrayLength())
         return false;

      if (symbol->isStatic() || symRef->isUnresolved() || symbol->isShadow())
         return false;

      if (node->getNumChildren() == 0)
         return true;
      }
   // We can handle logical shifts as these operations do not override CC.
   // Arithmetic ones (SRA/SRAG) kills CC.
   else if (node->getOpCode().isShiftLogical())
      {
      }
   else
      {
      // Unknown Node... be conservative.
      return false;
      }

   // Recursively check children nodes
   for (int i =0; i < node->getNumChildren(); i++)
      {
      if (!containsValidOpCodesForConditionalMoves(node->getChild(i),cg))
          return false;
      }
   return true;
   }

static TR::Block *
checkForCandidateBlockForConditionalLoadAndStores(TR::Node* node, TR::CodeGenerator* cg, TR::Block * currentBlock, bool *isLoadOrStoreOnConditionCandidateFallthrough)
   {
   if (cg->comp()->compileRelocatableCode())
      return NULL;

   if (node->getBranchDestination() == NULL)
      return NULL;

   // Check to see if the fall-through and taken blocks will remerge at the subsequent block
   TR::Block * targetBlock = node->getBranchDestination()->getEnclosingBlock();
   TR::Block * fallthroughBlock = currentBlock->getNextBlock();

   if (fallthroughBlock == NULL)
      return NULL;

   TR::Block * candidateBlock = NULL;

   TR::CFGEdgeList & fallthroughSuccessors = fallthroughBlock->getSuccessors();
   TR::CFGEdgeList & fallthroughExceptionSuccessors = fallthroughBlock->getExceptionSuccessors();

   TR::CFGEdgeList & targetSuccessors = targetBlock->getSuccessors();
   TR::CFGEdgeList & targetExceptionSuccessors = targetBlock->getExceptionSuccessors();

   // We only consider one target of fallthrough and no exception raised
   // To catch the following scenario.
   //   CurrentBlock:
   //         ifXcmpXX -> targetBlock
   //   FallThroughBlock:
   //        ..
   //   TargetBlock:
   if ((fallthroughSuccessors.size() == 1) && fallthroughExceptionSuccessors.empty())
      {
      TR::Block *fallthroughSuccessorBlock = fallthroughSuccessors.front()->getTo()->asBlock();
      if (targetBlock == fallthroughSuccessorBlock)
         {
         candidateBlock = fallthroughBlock;
         *isLoadOrStoreOnConditionCandidateFallthrough = true;
         }
      }
   // To catch the following scenario.
   //   CurrentBlock:
   //         ifXcmpXX -> targetBlock
   //   FallThroughBlock:
   //        ..
   //
   //   TargetBlock:
   //        ...
   //        br -> FallThroughBlock
   else if ((targetSuccessors.size() == 1) && targetExceptionSuccessors.empty())
      {
      TR::Block *targetSuccessorBlock = targetSuccessors.front()->getTo()->asBlock();

      if (fallthroughBlock == targetSuccessorBlock)
         {
         candidateBlock = targetBlock;
         *isLoadOrStoreOnConditionCandidateFallthrough = false;
         }
      }

   if (candidateBlock == NULL)
      return NULL;

   // Verify that the candidate block only has one predecessor
   if (!(candidateBlock->getPredecessors().size() == 1))
      return NULL;

   TR::Compilation *comp = cg->comp();
   bool noSTOC = comp->getOption(TR_DisableStoreOnCondition);

   // Check the nodes within the candidate block to ensure they can all be changed to conditoional loads and stores
   for (TR::TreeTop *tt = candidateBlock->getEntry(); tt != candidateBlock->getExit(); tt = tt->getNextTreeTop())
      {
      TR::Node* node = tt->getNode();
      TR::ILOpCodes opCodeValue = node->getOpCodeValue();
      if (opCodeValue == TR::BBStart || opCodeValue == TR::BBEnd)
         {
         // GlRegDeps moves that RA generates will be required to be "conditionalized"
         }
      else if (node->getOpCode().isLoad())
         {
         // Loads
         if (!containsValidOpCodesForConditionalMoves(node, cg))
            return NULL;
         }
      else if (node->getOpCode().isStore())
         {
         // Stores
         if (noSTOC || !containsValidOpCodesForConditionalMoves(node, cg))
            return NULL;
         }
      else
         {
         return NULL;
         }
      }

   // Use Random Generator to determine whether to ignore the frequency based heuristics below.
   bool ignoreFrequencyHeuristic = false;
   if (comp->getOption(TR_Randomize))
      {
      ignoreFrequencyHeuristic = cg->randomizer.randomBoolean();
      }

   // We should only generate conditional load/stores for poorly predicted branches.
   double conditionalMoveFrequencyThreshold = 0.3;
   if (!ignoreFrequencyHeuristic &&
         (candidateBlock->getFrequency() <= 6 ||
          currentBlock->getFrequency() <= 6 ||
          candidateBlock->getFrequency() < conditionalMoveFrequencyThreshold * currentBlock->getFrequency() ||
          candidateBlock->getFrequency() > (1 - conditionalMoveFrequencyThreshold) * currentBlock->getFrequency()))
      return NULL;


   if (performTransformation(comp, "O^O Reducing block: %d using conditional instructions\n", candidateBlock->getNumber()))
       return candidateBlock;
   else
       return NULL;
   }

bool isBranchOnCountAddSub(TR::CodeGenerator *cg, TR::Node *node)
   {
   if (!node->getType().isIntegral())
      return false;
   if (!node->getOpCode().isAdd() && !node->getOpCode().isSub())
      return false;
   if (!node->getOpCode().canUseBranchOnCount() || !node->isUseBranchOnCount())
      return false;

   TR::Node *regLoad = node->getFirstChild();
   TR::Node *delta = node->getSecondChild();

   // Check the constant
   if (!delta->getOpCode().isIntegralConst())
      return false;
   else if (node->getOpCode().isAdd() && delta->getIntegerNodeValue<int32_t>() != -1)
      return false;
   else if (node->getOpCode().isSub() && delta->getIntegerNodeValue<int32_t>() != 1)
      return false;

   if (regLoad->getOpCodeValue() != TR::iRegLoad)
      return false;

   return true;
   }

/**
 * Generate code to perform a comparision and branch
 * This routine is used mostly by ifcmp and cmp evaluator.
 *
 * The comparisson type is determined by the choice of CMP operators:
 *   - fBranchOp:  Operator used for forward operation ->  A fCmp B
 *   - rBranchOp:  Operator user for reverse operation ->  B rCmp A <=> A fCmp B
 */
TR::Register *
generateS390CompareBranch(TR::Node * node, TR::CodeGenerator * cg, TR::InstOpCode::Mnemonic branchOp, TR::InstOpCode::S390BranchCondition fBranchOpCond, TR::InstOpCode::S390BranchCondition rBranchOpCond, bool isUnorderedOK)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Node * thirdChild = NULL;
   TR::RegisterDependencyConditions *deps = NULL;
   TR::InstOpCode::S390BranchCondition opBranchCond = TR::InstOpCode::COND_NOP;
   TR::Compilation *comp = cg->comp();

//   traceMsg(comp,"In generateS390CompareBranch for node %p child1 = %p child2 = %p  child2->GetFloat = %f\n",node,firstChild,secondChild,secondChild->getOpCodeValue() == TR::fconst ? secondChild->getFloat() : -1);

   if (node->getNumChildren() == 3)
      {
      thirdChild = node->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps,
         "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");
      }
   // When we need relocation records to be generated and second child is conatant class pointer or method pointer,
   // We need both child to be evaluated
   if (cg->profiledPointersRequireRelocation() &&
       secondChild->getOpCodeValue() == TR::aconst &&
       (secondChild->isMethodPointerConstant() || secondChild->isClassPointerConstant()))
      {
      TR_ASSERT(!(node->isNopableInlineGuard()),"Should not evaluate class or method pointer constants underneath NOPable guards as they are runtime assumptions handled by virtualGuardHelper");
      // make sure aconst is evaluated explicitly so relocation record can be created for it
      if (secondChild->getRegister() == NULL)
         {
         cg->evaluate(firstChild);
         cg->evaluate(secondChild);
         }
      }

   TR::Instruction * cmpBranchInstr = NULL;

   bool isCmpGT = node->getOpCodeValue() == TR::ificmpgt || node->getOpCodeValue() == TR::iflcmpgt;

   TR::Block *canadidateLoadStoreConditionalBlock = NULL;
   bool isLoadOrStoreOnConditionCandidate = false;
   bool isLoadOrStoreOnConditionCandidateFallthrough  = false;

   // Determine if successor blocks maybe candidates for conditional load/stores/moves
   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
      {
      canadidateLoadStoreConditionalBlock = checkForCandidateBlockForConditionalLoadAndStores(node, cg, cg->getCurrentBlock(), &isLoadOrStoreOnConditionCandidateFallthrough);
      if (canadidateLoadStoreConditionalBlock)
         {
         // Currently only support if cndidateblock is on fallthrough path
         if (isLoadOrStoreOnConditionCandidateFallthrough)
            isLoadOrStoreOnConditionCandidate = true;
         }
      }

   bool useBranchOnCount = false;

   // If the child and node both have the branch on count flags set and the child (add or sub) hasn't been evaluated yet
   // (eg. nothing else references it), we can proceed. The tree should look like this:
   // ificmpgt
   //   => iadd
   //     iRegLoad GRX
   //     iconst -1
   //   iconst 0
   //   GlRegDeps
   //     PassThrough GRX
   //       => iadd
   if (isCmpGT &&
       isBranchOnCountAddSub(cg, firstChild) &&
       node->getOpCode().canUseBranchOnCount() &&
       node->isUseBranchOnCount() &&
       !firstChild->getRegister() &&
       !isUnorderedOK)
      {
      TR_ASSERT(!isLoadOrStoreOnConditionCandidate, "Unexpected Branch on Count candidate with conditional load/stores\n");

      // If no one but the iregLoad else uses the register, it's safe to generate the branch on count code
      cg->evaluate(firstChild->getFirstChild());
      if (cg->canClobberNodesRegister(firstChild->getFirstChild()))
         {
         useBranchOnCount = true;
         firstChild->setRegister(firstChild->getFirstChild()->getRegister());
         cg->decReferenceCount(firstChild->getFirstChild());
         cg->decReferenceCount(firstChild->getSecondChild());
         }
      }

   if (!isCmpGT || !useBranchOnCount || !isLoadOrStoreOnConditionCandidate)
       opBranchCond = generateTestUnderMaskIfPossible(node, cg, fBranchOpCond, rBranchOpCond);

   if (opBranchCond == TR::InstOpCode::COND_NOP &&
      (!isCmpGT || !useBranchOnCount) && !isLoadOrStoreOnConditionCandidate)
      {
      cmpBranchInstr = genCompareAndBranchInstructionIfPossible(cg, node, fBranchOpCond, rBranchOpCond, deps);
      }
   if (cmpBranchInstr == NULL)
      {

      //traceMsg(comp, "Couldn't use z6/z10 compare and branch instructions, so we'll generate this the old fashioned way\n");

      // couldn't use z6/z10 compare and branch instructions, so we'll generate this the old fashioned way

      // Generate compare code, find out if ops were reversed
      bool isForward;
      if(isCmpGT && useBranchOnCount)
         {
         opBranchCond = fBranchOpCond;
         }
      else if (opBranchCond == TR::InstOpCode::COND_NOP)
         {
         opBranchCond = generateS390CompareOps(node, cg, fBranchOpCond, rBranchOpCond);
         }
      TR::DataType dataType = node->getFirstChild()->getDataType();

      // take care of global reg deps if we have them
      if (thirdChild)
         {
         cg->evaluate(thirdChild);
         deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
         cg->decReferenceCount(thirdChild);
         }

      // We'll skip emitting the branch for LoadOrStoreOnCondition target blocks.
      if (isLoadOrStoreOnConditionCandidate)
         {
         if(comp->getOption(TR_TraceCG))
            traceMsg(comp, "isLoadOrStoreOnConditionCandidate is true\n");
         // We need to evaluate the end of this block for the GLRegDeps
         TR::TreeTop *blockEndTT = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
         TR_ASSERT( blockEndTT->getNode()->getOpCodeValue() == TR::BBEnd, "Unexpected next TT after compareAndBranch");

         if (blockEndTT->getNextTreeTop() == canadidateLoadStoreConditionalBlock->getEntry())
            {
            cg->setImplicitExceptionPoint(NULL);
            cg->setConditionalMovesEvaluationMode(true);
            cg->setFCondMoveBranchOpCond(opBranchCond);

            // Start evaluation of conditional loads and stores.
            TR::TreeTop *tt = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();

            // BBEnd node may have a global reg deps node
            TR::Node * bbEndNode = tt->getNode();
            cg->setCurrentEvaluationTreeTop(tt);
            cg->evaluate(bbEndNode);

            do
               {
               tt = tt->getNextTreeTop();  // Effectively starts at blockEndTT, but this allows us to evaluate the last Exit block of canadidateLoadStoreConditionalBlock

               cg->setCurrentEvaluationBlock(canadidateLoadStoreConditionalBlock);
               cg->setCurrentBlockIndex(canadidateLoadStoreConditionalBlock->getNumber());
               cg->setCurrentEvaluationTreeTop(tt);
               cg->resetMethodModifiedByRA();

               TR::Node* candidateBlockNode = tt->getNode();

               // We skip evaluation of goto, since we've merged the blocks together.
               if (candidateBlockNode->getOpCodeValue() == TR::Goto)
                  continue;

               if (comp->getOption(TR_TraceCG))
                  traceMsg(comp, "Evaluating node %p",candidateBlockNode);
               cg->evaluate(candidateBlockNode);
               }
            while (tt != canadidateLoadStoreConditionalBlock->getExit());

            // ILGRA can transform follow on block as extension because there are no GlRegDeps on entry exits that could get evaluated wrong
            TR::Block *nextBlock=canadidateLoadStoreConditionalBlock->getNextBlock();
            cg->setConditionalMovesEvaluationMode(false);
            return NULL;
            }
         }
      if (!isUnorderedOK)
         {
         // traceMsg(comp, "in !isunorderedOK statement\n");
         if(isCmpGT && useBranchOnCount)
            {
            if(TR::ificmpgt == node->getOpCodeValue())
               {
               generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, firstChild->getRegister(), deps, node->getBranchDestination()->getNode()->getLabel());
               }
            else
               {
               generateS390BranchInstruction(cg, TR::InstOpCode::BRCTG, node, firstChild->getRegister(), deps, node->getBranchDestination()->getNode()->getLabel());
               }

            cg->decReferenceCount(firstChild);
            cg->decReferenceCount(secondChild);
            }
         else
            {
            generateS390BranchInstruction(cg, branchOp, opBranchCond, node, node->getBranchDestination()->getNode()->getLabel(), deps);
            }
         }
      else
         {
         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "in else statement\n");
         uint8_t branchMask = getMaskForBranchCondition(opBranchCond);
         branchMask += 0x01;
         opBranchCond = getBranchConditionForMask(branchMask);

         generateS390BranchInstruction(cg, branchOp, opBranchCond, node, node->getBranchDestination()->getNode()->getLabel(),deps);
         }
      }
   return NULL;
   }

/**
 *            Reg-Reg           Mem-Reg
 *    Logical     Signed      Logical     Signed
 *    32    64   32     64   32    64    32      64
 * 8  LLCR  LLGCR   LBR   LGBR  LLC   LLGC  LB      LGB
 * 16 LLHR  LLGHR   LHR   LGHR  LLH   LLGH  LH,LHY  LGH
 * 32 LR    LLGFR   LR    LGFR  L,LY  LLGF  L,LY    LGF
 * 64 ----- LGR     ----- LGR   ----- LG    -----   LG
 */
static const TR::InstOpCode::Mnemonic loadInstrs[2/*Form*/][4/*numberOfBits*/][2/*isSigned*/][2/*numberOfExtendBits*/] =
   {
         /* ----- Logical -------   ----- Signed -------*/
         /*   32          64          32          64    */
/*RegReg*/{
/* 8*/   { { TR::InstOpCode::LLCR,  TR::InstOpCode::LLGCR }, { TR::InstOpCode::LBR,   TR::InstOpCode::LGBR } },
/*16*/   { { TR::InstOpCode::LLHR,  TR::InstOpCode::LLGHR }, { TR::InstOpCode::LHR,   TR::InstOpCode::LGHR } },
/*32*/   { { TR::InstOpCode::LR,    TR::InstOpCode::LLGFR }, { TR::InstOpCode::LR,    TR::InstOpCode::LGFR } },
/*64*/   { { TR::InstOpCode::BAD,   TR::InstOpCode::LGR   }, { TR::InstOpCode::BAD,   TR::InstOpCode::LGR  } }
      },
/*MemReg*/{
/* 8*/   { { TR::InstOpCode::LLC,   TR::InstOpCode::LLGC  }, { TR::InstOpCode::LB,    TR::InstOpCode::LGB } },
/*16*/   { { TR::InstOpCode::LLH,   TR::InstOpCode::LLGH  }, { TR::InstOpCode::LH,    TR::InstOpCode::LGH } },
/*32*/   { { TR::InstOpCode::L,     TR::InstOpCode::LLGF  }, { TR::InstOpCode::L,     TR::InstOpCode::LGF } },
/*64*/   { { TR::InstOpCode::BAD,   TR::InstOpCode::LG    }, { TR::InstOpCode::BAD,   TR::InstOpCode::LG  } }
      }
   };

template <uint32_t numberOfBits>
TR::Register *
genericLoad(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister)
   {
   bool nodeSigned = node->getType().isInt64() || !node->isZeroExtendedAtSource();

   if (node->getType().isAddress() || node->getType().isAggregate())
      nodeSigned = false;

   if (numberOfBits > 32 || numberOfBits == 31 || node->isExtendedTo64BitAtSource())
      return genericLoadHelper<numberOfBits, 64, MemReg>(node, cg, tempMR, srcRegister, nodeSigned, false);
   else
      return genericLoadHelper<numberOfBits, 32, MemReg>(node, cg, tempMR, srcRegister, nodeSigned, false);
   }

/**
 * This function may be used directly. However, there are couple of known wrappers for the big consumers
 *   - genericLoad -- wrapper for b-,s-,i-,l-... loadEvaluator
 *   - extendCastEvaluator -- wrapper for the conversions that convert from small to big (i.e. s2l)
 *   - narrowCastEvaluator -- wrapper for the conversions that convert from big to small (i.e. s2b)
 * In general, those wrappers check for the flags set on nodes. genericLoadHelper is agnostic to the settings of the flags.
 *
 * TODO: This function may further be extended for many other loads available on zOS, like load-and-test, Load-high, etc. through the Form tparm.
 *
 * @tparam numberOfBits       specifies how many bits to load from storage
 * @tparam numberOfExtendBits specifies what register type to use 32 or 64. 64 includes register pairs
 * @tparam form               Now either, Memory-to-Register or Register-to-Register load (extensible)
 *                            Should/could skip manual extension using two shifts.
 * @param node                for RegReg, the childNode (for checking if cloberable), not too important for MemReg (used for generateXXInstruction())
 * @param tempMR              used only for MemReg form now. Address pointer
 * @param srcRegister         is an allocated register. In RegReg form, will be careful not to clobber.
 * @param isSourceSigned      specifies the preference for logical (zero-extend, unsigned) or arithmetic (MSB extend, signed) loads
 * @param couldIgnoreExtend,  optimization for ARCH<5 (before GoldenEagle) as some load instructions have not become available then
 * @return                    result of the conversion. Will only clobber srcRegister if allowed by canClobberNodesRegister()
 */
template <uint32_t numberOfBits, uint32_t numberOfExtendBits, enum LoadForm form>
TR::Register *
genericLoadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister, bool isSourceSigned, bool couldIgnoreExtend_OR_canClobberSrcReg)
   {
   TR_ASSERT_FATAL((numberOfBits & (numberOfBits - 1)) == 0, "Invalid input numberOfBits (%d) is not a multiple of two", numberOfBits);
   TR_ASSERT(numberOfExtendBits>=numberOfBits, "Cannot put %d bits into register of size %d. Verify calling convention", numberOfBits, numberOfExtendBits);
   const static int numberOfBytes = numberOfBits/8;
   TR::Register * targetRegister = srcRegister;
   const bool canClobberSrcReg = form==RegReg && couldIgnoreExtend_OR_canClobberSrcReg;

   // Compile-time constant-folding variable
   //  8 1 0001  0 ->  0 0000
   // 16 2 0010  1 ->  1 0001
   // 32 4 0100  3 ->  2 0010
   // 64 8 1000  7 ->  3 0011
   const static int numberOfBytesLog2 = (bool)(numberOfBytes&0xA)+2*(bool)(numberOfBytes&0xC);

   TR::InstOpCode::Mnemonic load = loadInstrs[form][numberOfBytesLog2][isSourceSigned][numberOfExtendBits/32-1];

   if (form == RegReg)
      {
      if (!canClobberSrcReg)
         {
         targetRegister = cg->allocateRegister();
         }

      if (TR::InstOpCode::getInstructionFormat(load) == RR_FORMAT)
         generateRRInstruction(cg, load, node, targetRegister, srcRegister);
      else
         generateRREInstruction(cg, load, node, targetRegister, srcRegister);
      }
   else
      {
      // TODO: I don't think we need to be doing this. We should track where these things are allocated and prevent
      // the allocation at the source. That is, this API needs to be properly defined and explicitly state whether
      // srcRegister is NULL or not.
      if (srcRegister)
         {
         cg->stopUsingRegister(srcRegister);
         }

      targetRegister = cg->allocateRegister();
      generateRXInstruction(cg, load, node, targetRegister, tempMR);
      }

   return targetRegister;
   }

/**
 * This function is a wrapper for generiLoadHelper. It is intended to handle all the conversions that narrow data.
 * Narrowing conversions are done by either zero- or sign-extending; which of the two, depends only on the signnedness of the
 * destination type. Note that on 32 bit AMODE, narrowing will always be into a 32bit register. (i.e. there is no
 * case that narrows into a 64bit register yet, unless this function were to support o-types -- it does not)
 *
 * It is possible for this evaluator to be skipped, when the isUnneededConversion() flag is set.
 * The flags are set elsewhere (i.e. CodeGenPrep.cpp). Any patterns to be detected, should be detected elsewhere.
 * Try hard not to do any pattern matching here.
 *
 * @tparam isDstTypeSigned signedness of the destination type
 * @tparam newSize         size in bits, of the destination type (8,16,32,40,48,56)
 * @return
 */
template<bool isDstTypeSigned, uint32_t newSize>
TR::Register *
OMR::Z::TreeEvaluator::narrowCastEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   TR::Compilation *comp = cg->comp();

   targetRegister = cg->evaluate(firstChild);
   if (newSize <= 32 && targetRegister->getRegisterPair() != NULL)
      // if were using the register pairs, just throw away the top
      targetRegister = targetRegister->getLowOrder();
   else
     {
      targetRegister = TR::TreeEvaluator::passThroughEvaluator(node, cg);
      if(targetRegister->getStartOfRangeNode() == firstChild)
        targetRegister->setStartOfRangeNode(node);
      return targetRegister;
     }

   node->setRegister(targetRegister);
   if(targetRegister->getStartOfRangeNode() == firstChild)
     targetRegister->setStartOfRangeNode(node);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }

/**
 * This function is a wrapper for genericLoadHelper. It is intended to handle all the conversions that grow/extend data.
 * Extension conversions are done by either zero- or sign-extending; which of the two, depends only on the signnedness of the
 * source type.
 *
 * It is possible for this evaluator to be skipped, when the isUnneededConversion() flag is set.
 * The flags are set elsewhere (i.e. CodeGenPrep.cpp). Any patterns to be detected, should be detected elsewhere.
 * Try hard not to do any pattern matching here.
 *
 * @tparam isSourceTypeSigned Signedness of the source type
 * @tparam srcSize            bit-size of the source (8,16,31,32,64)
 * @tparam numberOfExtendBits Valid values are 32 or 64.
 * @return
 */
template<bool isSourceTypeSigned, uint32_t srcSize, uint32_t numberOfExtendBits>
TR::Register *
OMR::Z::TreeEvaluator::extendCastEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register * targetRegister = NULL;
   TR::Register * childRegister;
   TR::Compilation *comp = cg->comp();

   childRegister = targetRegister = cg->evaluate(firstChild);
   if (numberOfExtendBits == 32 && targetRegister->getRegisterPair() != NULL) //&& cg->comp()->target().is32Bit()
      // if were using the register pairs (i.e. force64bit() might have been set), just throw away the top
      targetRegister = targetRegister->getLowOrder();

   bool canClobberSrc = cg->canClobberNodesRegister(firstChild);

   /**
   * A load (for proper type conversion) is required if the node is unsigned, not sign extended and
   * not marked as unneeded conversion.
   *
   * For example, codegen can encounter the following tree:
   *
   * n4188n   (  0)  iRegStore GPR6  (Unsigned NeedsSignExt privatizedInlinerArg )
   * n4109n   (  3)    l2i
   * n19339n  (  1)      ==>lRegLoad (in GPR_3616) (highWordZero X>=0 cannotOverflow SeenRealReference )
   *  .....
   * n4182n   (  0)      iu2l (in GPR_3616) (highWordZero X>=0 )
   * n4109n   (  0)        ==>l2i (in GPR_3616)
   *
   * In this case, a load is needed to zero the high half of the long to corretly convert iu to l.
   */
   if (!node->isUnneededConversion() && !(isSourceTypeSigned && childRegister->alreadySignExtended()))
      {
      targetRegister = genericLoadHelper<srcSize, numberOfExtendBits, RegReg> (firstChild, cg, NULL, targetRegister, isSourceTypeSigned, canClobberSrc);
      }

   if (numberOfExtendBits == 64)
      {
      // TODO: Think about whether or not this is actually needed. My current thinking is that setting this flag is
      // redundant because if any extension happened it must have used a 64-bit instruction to do so in which case
      // the register should be marked as a 64-bit register already.
      targetRegister->setIs64BitReg();
      }

   node->setRegister(targetRegister);
   if(targetRegister == childRegister)
     targetRegister->setStartOfRangeNode(node);
   cg->decReferenceCount(firstChild);
   return targetRegister;
   }


/**
 * This function is a rather silly wrapper for address type conversion (either to- or from-). Address conversions behave
 * exactly like the code from extendCastEvaluator() or narrowCastEvaluator(). However, those templates need to know
 * the precision at compile time, meanwhile AMODE is unlike other types, for which bit-size is not constant.
 *
 * To avoid creating __YET__ another wrapper, depending on the direction of the conversion (to- or from- an address), the
 * usage is multiplexed via the dirToAddr parm
 *
 * @tparm otherSize  the bit-size of the type, other then the address (i.e. a2b, otherSize=8)
 * @tparm dirToAddr  the direction of conversion, true for to address (i.e. x2a, dirToAddr=true)
 * @return
 */
template<uint32_t otherSize, bool dirToAddr>
TR::Register *
OMR::Z::TreeEvaluator::addressCastEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();

   // use getSize instead of getAddressPrecision since for some cases the address type node symbol size will be different than the address precesion value (RTC: 34855)
   uint8_t addrSize = dirToAddr ? 8 * node->getSize()
                                : 8 * node->getFirstChild()->getSize();

   if (otherSize == addrSize)
      {
      return TR::TreeEvaluator::passThroughEvaluator(node, cg);
      }
   else if (dirToAddr && otherSize > addrSize) //i.e. (otherSize)2a
      {
      switch (addrSize)
         {
      case 56: return TR::TreeEvaluator::extendCastEvaluator<false, 56, 64>(node, cg); break;
      case 48: return TR::TreeEvaluator::extendCastEvaluator<false, 48, 64>(node, cg); break;
      case 40: return TR::TreeEvaluator::extendCastEvaluator<false, 40, 64>(node, cg); break;
      case 32: return TR::TreeEvaluator::narrowCastEvaluator<false, 32>(node, cg); break;
      case 24: return TR::TreeEvaluator::narrowCastEvaluator<false, 24>(node, cg); break;
      case 16: return TR::TreeEvaluator::narrowCastEvaluator<false, 16>(node, cg); break;
      case 8:  return TR::TreeEvaluator::narrowCastEvaluator<false,  8>(node, cg); break;
      default:
         TR_ASSERT(0, "Invalid Address Precision (%d)\n", addrSize);
         break;
         }
      }
   else if (!dirToAddr && addrSize > otherSize) //i.e. a2(otherSize)
      {
      return narrowCastEvaluator<false, otherSize>(node,cg);
      }
   else if (dirToAddr && otherSize < addrSize) //i.e. (otherSize)2a
      {
      if (addrSize>32)
         {
         TR::Node* child = node->getFirstChild();

         // We need to generate LLGT (Load 31 bit into 64)
         // iu2a <addr size = 8>
         //   aload
         if (child->getOpCode().isLoadVar() && child->getType().isAddress())
            {
            child->setZeroExtendTo64BitAtSource(true);
            //if (child->getRegister() == NULL)
            //   node->setUnneededConversion(true);
            }
         return extendCastEvaluator<false, otherSize, 64>(node, cg);
         }
      else
         {
         return extendCastEvaluator<false, otherSize, 32>(node, cg);
         }
      }
   else //if (!dirToAddr && addrSize < otherSize) //i.e. a2(otherSize)
      {
      switch (addrSize)
         {
      case 56:         return TR::TreeEvaluator::extendCastEvaluator<false, 56, 64>(node, cg);   break;
      case 48:         return TR::TreeEvaluator::extendCastEvaluator<false, 48, 64>(node, cg);   break;
      case 40:         return TR::TreeEvaluator::extendCastEvaluator<false, 40, 64>(node, cg);   break;
                       // NOTE 31, on purpose!
      case 32:         return TR::TreeEvaluator::extendCastEvaluator<false, 31, 64>(node, cg); break;
      case 24:         return TR::TreeEvaluator::extendCastEvaluator<false, 24, (otherSize>32) ? 64:32>(node, cg);       break;
      case 16:         return TR::TreeEvaluator::extendCastEvaluator<false, 16, (otherSize>32) ? 64:32>(node, cg);       break;
      case 8:          return TR::TreeEvaluator::extendCastEvaluator<false,  8, (otherSize>32) ? 64:32>(node, cg);       break;
      default:
         TR_ASSERT(0, "Invalid Address Precision (%d)\n", addrSize);
         break;
         }
      }
   TR_ASSERT(0, "Should never reach here");
   return NULL; //stop compiler from complaining
   }

namespace OMR
{
namespace Z
{
// Some invalid template instantiations. Should never be called.
template <> TR::Register *
OMR::Z::TreeEvaluator::extendCastEvaluator<false,  8,  8>(TR::Node * node, TR::CodeGenerator * cg) { TR_ASSERT(0, "Should not be called"); return NULL; }
template <> TR::Register *
OMR::Z::TreeEvaluator::extendCastEvaluator<false, 16,  8>(TR::Node * node, TR::CodeGenerator * cg) { TR_ASSERT(0, "Should not be called"); return NULL; }
template <> TR::Register *
OMR::Z::TreeEvaluator::extendCastEvaluator<false, 24,  8>(TR::Node * node, TR::CodeGenerator * cg) { TR_ASSERT(0, "Should not be called"); return NULL; }
template <> TR::Register *
OMR::Z::TreeEvaluator::extendCastEvaluator<false,  8, 16>(TR::Node * node, TR::CodeGenerator * cg) { TR_ASSERT(0, "Should not be called"); return NULL; }
template <> TR::Register *
OMR::Z::TreeEvaluator::extendCastEvaluator<false, 16, 16>(TR::Node * node, TR::CodeGenerator * cg) { TR_ASSERT(0, "Should not be called"); return NULL; }
template <> TR::Register *
OMR::Z::TreeEvaluator::extendCastEvaluator<false, 24, 16>(TR::Node * node, TR::CodeGenerator * cg) { TR_ASSERT(0, "Should not be called"); return NULL; }

// As the templates are not defined in the headers, but here, must instantiate all callable variations in this compilation units,
// to make it available anywhere else in the program.
template TR::Register * OMR::Z::TreeEvaluator::narrowCastEvaluator<true,   8>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::narrowCastEvaluator<false,  8>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::narrowCastEvaluator<true,  16>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::narrowCastEvaluator<false, 16>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::narrowCastEvaluator<true,  32>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::narrowCastEvaluator<false, 32>(TR::Node * node, TR::CodeGenerator * cg);

template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<true,   8, 32>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<true,   8, 64>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<false,  8, 32>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<false,  8, 64>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<true,  16, 32>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<true,  16, 64>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<false, 16, 32>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<false, 16, 64>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<true,  32, 32>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<true,  32, 64>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<false, 32, 32>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<false, 32, 64>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<true,  64, 64>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::extendCastEvaluator<false, 64, 64>(TR::Node * node, TR::CodeGenerator * cg);

template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator< 8, true >(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator< 8, false>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator<16, true >(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator<16, false>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator<32, true >(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator<32, false>(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator<64, true >(TR::Node * node, TR::CodeGenerator * cg);
template TR::Register * OMR::Z::TreeEvaluator::addressCastEvaluator<64, false>(TR::Node * node, TR::CodeGenerator * cg);
}
}

template TR::Register * genericLoadHelper<32, 32, MemReg>
      (TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister, bool isSourceSigned, bool couldIgnoreExtend);
template TR::Register * genericLoadHelper<64, 64, MemReg>
      (TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, TR::Register * srcRegister, bool isSourceSigned, bool couldIgnoreExtend);

TR::Register *
sloadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, bool isReversed)
   {
   TR::Compilation *comp = cg->comp();
   //TR_ASSERTC( !isReversed,comp, "need to write logic for reverse load still");

   TR::Register * tempReg = cg->allocateRegister();

   if (tempMR == NULL)
      {
      tempMR = generateS390MemoryReference(node, cg);
      }

   if (isReversed)
      {
      generateRXInstruction(cg, TR::InstOpCode::LRVH, node, tempReg, tempMR);
      //sign or clear highOrderBits: TODO: Clearing/Extending is probably not needed
      if (node->isExtendedTo64BitAtSource())
         tempReg = genericLoadHelper<16, 64, RegReg>(node, cg, NULL, tempReg, !node->isZeroExtendedAtSource(), true);
      else
         tempReg = genericLoadHelper<16, 32, RegReg>(node, cg, NULL, tempReg, !node->isZeroExtendedAtSource(), true);
      }
   else
      {
      tempReg = genericLoad<16>(node, cg, tempMR, tempReg);
      }

   node->setRegister(tempReg);
   tempMR->stopUsingMemRefRegister(cg);
   return tempReg;
   }

bool relativeLongLoadHelper(TR::CodeGenerator * cg, TR::Node * node, TR::Register* tReg)
   {
   static char * disableFORCELRL = feGetEnv("TR_DISABLEFORCELRL");

   if(!node->getOpCode().hasSymbolReference())
      return false;

   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::Symbol * symbol = symRef->getSymbol();

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) &&
       symbol->isStatic() &&
       !symRef->isUnresolved() &&
       !cg->comp()->compileRelocatableCode() &&
       !node->getOpCode().isIndirect() &&
       !cg->getConditionalMovesEvaluationMode()
      )
      {
      uintptr_t staticAddress = (uintptr_t)symRef->getSymbol()->getStaticSymbol()->getStaticAddress();

      TR::InstOpCode::Mnemonic op = TR::InstOpCode::BAD;
      if (node->getType().isInt32() || (!(cg->comp()->target().is64Bit()) && node->getType().isAddress() ))
         {
         op = TR::InstOpCode::LRL;

         if (node->isSignExtendedTo64BitAtSource())
            {
            op = TR::InstOpCode::LGFRL;
            }

         if (node->isZeroExtendedTo64BitAtSource())
            {
            op = TR::InstOpCode::LLGFRL;
            }
         }
      else
         {
         op = TR::InstOpCode::LGRL;
         }

      TR_ASSERT(op != TR::InstOpCode::BAD, "Bad opcode selection in relative load helper!\n");

      if ((!disableFORCELRL || cg->canUseRelativeLongInstructions(staticAddress)) &&
           ((cg->comp()->target().is32Bit() && (staticAddress&0x3) == 0)  ||
             (cg->comp()->target().is64Bit() && (staticAddress&0x7) == 0)
           )
         )
         {
         if (node->useSignExtensionMode())
            {
            generateRILInstruction(cg, TR::InstOpCode::LGFRL, node, tReg, symRef, reinterpret_cast<void*>(staticAddress));
            }
         else
            {
            if (cg->comp()->target().is64Bit())
               {
               // On 64bit we could end up having to transform the LGRL into LLILF/LG.
               // We would not be able to handle GRP0 in this case
               //
               tReg->setIsUsedInMemRef();
               }
            generateRILInstruction(cg, op, node, tReg, symRef, reinterpret_cast<void*>(staticAddress));
            }
         return true;
         }
      }

   return false;
   }

TR::Register *
iloadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR, bool isReversed)
   {
   //TR_ASSERTC( !isReversed,cg->comp(), "need to write logic for reverse load still");

   TR::Register * tempReg = cg->allocateRegister();

   if (relativeLongLoadHelper(cg, node, tempReg))
      {
      node->setRegister(tempReg);
      return tempReg;
      }

   if (tempMR == NULL)
      {
      tempMR = generateS390MemoryReference(node, cg);
      }

   if (isReversed)
      {
      generateRXInstruction(cg, TR::InstOpCode::LRV, node, tempReg, tempMR);

      if (node->isExtendedTo64BitAtSource())
         {
         tempReg = genericLoadHelper<32, 64, RegReg>(node, cg, NULL, tempReg, !node->isZeroExtendedAtSource(), true);
         }
      }
   else if (cg->getConditionalMovesEvaluationMode())
      {
      generateRSInstruction(cg, TR::InstOpCode::LOC, node, tempReg, cg->getRCondMoveBranchOpCond(), tempMR);

      if (node->isSignExtendedTo64BitAtSource())
         {
         generateRRInstruction(cg, TR::InstOpCode::LGFR, node, tempReg, tempReg);
         }

      if (node->isZeroExtendedTo64BitAtSource())
         {
         generateRRInstruction(cg, TR::InstOpCode::LLGFR, node, tempReg, tempReg);
         }
      }
   else if ((node->getOpCodeValue() == TR::iload || node->getOpCodeValue() == TR::iloadi) && node->isLoadAndTest())
      {
      bool mustExtend = node->isExtendedTo64BitAtSource();

      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && mustExtend)
         {
         if (node->isZeroExtendedTo64BitAtSource())
            {
            generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tempReg, tempMR);
            generateRRInstruction(cg, TR::InstOpCode::LTGR, node, tempReg, tempReg);
            }
         else
            {
            generateRXInstruction(cg, TR::InstOpCode::LTGF, node, tempReg, tempMR);
            }

         }
      else if (!mustExtend)
         {
         generateRXInstruction(cg, TR::InstOpCode::LT, node, tempReg, tempMR);
         }
      else
         {
         tempReg = genericLoad<32>(node, cg, tempMR, tempReg);
         generateRRInstruction(cg, TR::InstOpCode::LTR, node, tempReg, tempReg);
         }
      }
   else
      tempReg = genericLoad<32>(node, cg, tempMR, tempReg);
   node->setRegister(tempReg);
   tempMR->stopUsingMemRefRegister(cg);
   return tempReg;
   }

TR::Register *
lloadHelper64(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * highMR, bool isReversed)
   {
   TR::Register * longRegister = cg->allocateRegister();

   if (relativeLongLoadHelper(cg, node, longRegister))
      {
      node->setRegister(longRegister);
      return longRegister;
      }

   // Force Memref to get a new reg.  Using the pair screws it up.
   node->setRegister(NULL);

   if (highMR == NULL)
      {
      highMR = generateS390MemoryReference(node, cg);
      }

   if (node->isLoadAndTest())
      {
      generateRXInstruction(cg, TR::InstOpCode::LTG, node, longRegister, highMR);
      }
   else
      {
      if (node->getOpCode().hasSymbolReference())
         {
         if (isReversed)
            generateRXInstruction(cg, TR::InstOpCode::LRVG, node, longRegister, highMR);
         else if (cg->getConditionalMovesEvaluationMode())
            generateRSInstruction(cg, TR::InstOpCode::LOCG, node, longRegister, cg->getRCondMoveBranchOpCond(), highMR);
         else
            longRegister = genericLoad<64>(node, cg, highMR, longRegister);
         }
      else if (isReversed)
         generateRXInstruction(cg, TR::InstOpCode::LRVG, node, longRegister, highMR);
      else if (cg->getConditionalMovesEvaluationMode())
         generateRSInstruction(cg, TR::InstOpCode::LOCG, node, longRegister, cg->getRCondMoveBranchOpCond(), highMR);
      else
         longRegister = genericLoad<64>(node, cg, highMR, longRegister);
      }

   node->setRegister(longRegister);
   highMR->stopUsingMemRefRegister(cg);
   return longRegister;
   }

TR::Register *
aloadHelper(TR::Node * node, TR::CodeGenerator * cg, TR::MemoryReference * tempMR)
   {
   TR::Compilation *comp = cg->comp();

   TR::Register * tempReg = NULL; // msf - ia32 does not check for not internal pointer...

   // Evaluation of aloadi cannot be skipped if any unevaluated node in its subtree contains a symbol reference
   // TODO: Currently we exclude aloadi nodes from the skipping list conservatively. The logic should be modified
   // to skip evaluation of unneeded aloadi nodes which do not contain any symbol reference.
   if (node->isUnneededIALoad() &&
           (node->getFirstChild()->getNumChildren() == 0 || node->getFirstChild()->getRegister() != NULL))
      {
      traceMsg (comp, "This iaload is not needed: %p\n", node);

      tempReg= cg->allocateRegister();
      node->setRegister(tempReg);
      cg->recursivelyDecReferenceCount(node->getFirstChild());
      cg->stopUsingRegister(tempReg);
      return tempReg;
      }

   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::Symbol * symbol = symRef->getSymbol();
   TR::Node *constNode = reinterpret_cast<TR::Node *>(node->getSymbolReference()->getOffset());
   bool dynLitPoolLoad = false;
   tempReg = TR::TreeEvaluator::checkAndAllocateReferenceRegister(node, cg, dynLitPoolLoad);

   bool isStatic = symbol->isStatic() && !symRef->isUnresolved();
   bool isPICCandidate = isStatic && symbol->isClassObject();
   if (isPICCandidate
           && !cg->comp()->compileRelocatableCode() // AOT Class Address are loaded via snippets already
           && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)symbol->getStaticSymbol()->getStaticAddress(), node))
      {
      if (tempMR == NULL)
         {
         uintptr_t value = (uintptr_t) symbol->getStaticSymbol()->getStaticAddress();
         genLoadAddressConstantInSnippet(cg, node, value, tempReg, NULL, NULL, NULL, true);
         }
      // HCR to do for compressed references
      else
         {
         generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg, tempMR);
         }

      updateReferenceNode(node, tempReg);
      cg->decReferenceCount(node->getFirstChild());
      }
   else if ((symRef->isLiteralPoolAddress()) && (node->getOpCodeValue() == TR::aload))
      {
      // the imm. operand will be patched when the actual address of
      // the literal pool is known
      generateLoadLiteralPoolAddress(cg, node, tempReg);
      }
   else if ((symRef->isLiteralPoolAddress())
            && (node->getOpCodeValue() == TR::aloadi)
            && constNode->isClassUnloadingConst())
      {
      uintptr_t value = constNode->getAddress();
      TR::Instruction *unloadableConstInstr = generateRILInstruction(cg, TR::InstOpCode::LARL, node, tempReg, reinterpret_cast<void*>(value));
      TR_OpaqueClassBlock* unloadableClass = NULL;
      if (constNode->isMethodPointerConstant())
         {
         unloadableClass = (TR_OpaqueClassBlock *) cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) value,
            comp->getCurrentMethod())->classOfMethod();
         if (cg->fe()->isUnloadAssumptionRequired(unloadableClass, comp->getCurrentMethod())
                 || cg->profiledPointersRequireRelocation())
            {
            comp->getStaticMethodPICSites()->push_front(unloadableConstInstr);
            }
         }
      else
         {
         unloadableClass = (TR_OpaqueClassBlock *) value;
         if (cg->fe()->isUnloadAssumptionRequired(unloadableClass, comp->getCurrentMethod()) ||
             cg->profiledPointersRequireRelocation())
            {
            comp->getStaticPICSites()->push_front(unloadableConstInstr);
            }
         }
      }
   else if(relativeLongLoadHelper(cg, node, tempReg))
      {
      }
   else
      {
      if (tempMR == NULL)
         {
         tempMR = generateS390MemoryReference(node, cg);
         TR::TreeEvaluator::checkAndSetMemRefDataSnippetRelocationType(node, cg, tempMR);
         }

      // check if J9 classes take 4 bytes (class offsets)
      //
      if (cg->isCompressedClassPointerOfObjectHeader(node) && !dynLitPoolLoad)
         {
         if (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
            TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, tempReg, tempMR, NULL);
         else
            TR::TreeEvaluator::genLoadForObjectHeaders(cg, node, tempReg, tempMR, NULL);
         }
      else
         {
         if (node->isLoadAndTest())
            {
            TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::getLoadTestOpCode();

            TR_ASSERT_FATAL(!node->isSignExtendedAtSource(), "Cannot force sign extension at source for address loads\n");

            if (node->isZeroExtendedTo64BitAtSource())
               {
               generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tempReg, tempMR);
               generateRRInstruction(cg, TR::InstOpCode::LTGR, node, tempReg, tempReg);
               }
            else
               {
               generateRXInstruction(cg, opCode, node, tempReg, tempMR);
               }

#ifdef J9_PROJECT_SPECIFIC
            // Load and Mask not supported for Load and Test, so must mask explicitly
            if (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
               TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
#endif
            }
         else
            {
            if (cg->getConditionalMovesEvaluationMode())
               {
               generateRSInstruction(cg, TR::InstOpCode::getLoadOpCode() == TR::InstOpCode::L ? TR::InstOpCode::LOC : TR::InstOpCode::LOCG, node, tempReg, cg->getRCondMoveBranchOpCond(), tempMR);

#ifdef J9_PROJECT_SPECIFIC
               // Load and Mask not supported for conditional moves, so must mask explicitly
               if (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
                  TR::TreeEvaluator::generateVFTMaskInstruction(node, tempReg, cg);
#endif
               }
            else
               {
               if (node->getSymbolReference() == comp->getSymRefTab()->findVftSymbolRef())
                  TR::TreeEvaluator::genLoadForObjectHeadersMasked(cg, node, tempReg, tempMR, NULL);
               else if (node->getSymbol()->getSize() == 4 && node->isExtendedTo64BitAtSource())
                  generateRXInstruction(cg, TR::InstOpCode::LLGF, node, tempReg, tempMR);
               else
                  generateRXInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, tempReg, tempMR);
               }
            }
         }

      updateReferenceNode(node, tempReg);
      }

   node->setRegister(tempReg);
   return tempReg;
   }

bool relativeLongStoreHelper(TR::CodeGenerator * cg, TR::Node * node, TR::Node * valueChild)
   {
   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::Symbol * symbol = symRef->getSymbol();

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) &&
       symbol->isStatic() &&
       !symRef->isUnresolved() &&
       !cg->comp()->compileRelocatableCode() &&
       !node->getOpCode().isIndirect() &&
       !cg->getConditionalMovesEvaluationMode()
      )
      {
      uintptr_t staticAddress = (uintptr_t)symRef->getSymbol()->getStaticSymbol()->getStaticAddress();
      TR::InstOpCode::Mnemonic op = node->getSize() == 8 ? TR::InstOpCode::STGRL : TR::InstOpCode::STRL;

      TR::Register * sourceRegister = cg->evaluate(valueChild);

      if ((cg->comp()->target().is32Bit() && (staticAddress&0x3) == 0)  ||
           (cg->comp()->target().is64Bit() && (staticAddress&0x7) == 0)
         )
         {
         generateRILInstruction(cg, op, node, sourceRegister, symRef, reinterpret_cast<void*>(staticAddress));

         return true;
         }
      }

   return false;
   }

bool directToMemoryAddHelper(TR::CodeGenerator * cg, TR::Node * node, TR::Node * valueChild, TR::InstOpCode::Mnemonic op)
   {
   TR_ASSERT( op == TR::InstOpCode::ASI || op == TR::InstOpCode::AGSI, "directToMemoryAddHelper: Should be only used for ASI or ASGI\n");

   // Try to exploit direct memory operation if all we are doing is incrementing
   // a counter in memory.
   //
   if (cg->isAddMemoryUpdate(node, valueChild))
      {
      TR::MemoryReference * tempMR = generateS390MemoryReference(node, cg);
      int32_t value = 0;

      switch (valueChild->getSecondChild()->getDataType())
         {
         case TR::Int64:
            value = (int32_t) valueChild->getSecondChild()->getLongInt();
            break;
         case TR::Int32:
            value = (int32_t)(valueChild->getSecondChild()->getInt());
            break;
         default:
            TR_ASSERT( 0,"unexpected data type for Add immediate supported");
         }

      if (valueChild->getOpCodeValue() == TR::isub || valueChild->getOpCodeValue() == TR::lsub)
         {
         value = -value;
         }
      if (valueChild->getFirstChild()->getReferenceCount()==1)
         {
         generateSIInstruction(cg, op, node, tempMR, value);

         cg->recursivelyDecReferenceCount(valueChild);
         }
      else
         {
         //  The isub/iadd child must get into a register, so evaluate it.
         //
         cg->evaluate(valueChild->getFirstChild());

         generateSIInstruction(cg, op, node, tempMR, value);

         cg->decReferenceCount(valueChild);
         cg->decReferenceCount(valueChild->getFirstChild());
         cg->decReferenceCount(valueChild->getSecondChild());
         }

      return true;
      }

   return false;
   }

/**
 * This method generates instructions for the storeHelper methods as 32 bit values, if it can
 * find an immediate in the 16 bit range, and the correct hardware prerequisites are met.
 * Otherwise it returns a value to indicate another method must be used to move one value from
 * memory location to another.
 */
bool storeHelperImmediateInstruction(TR::Node * valueChild, TR::CodeGenerator * cg, bool isReversed, TR::InstOpCode::Mnemonic op, TR::Node * node, TR::MemoryReference * mf)
   {
   int32_t imm;

   TR::Compilation *comp = cg->comp();

   // All the immediates that use this require non reversed instructions
   if (!cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) || isReversed || cg->getConditionalMovesEvaluationMode())
      {
      return true;
      }

   if (valueChild->getOpCode().isLoadConst() && (mf->getIndexRegister() == NULL))
      {
      if (mf->getBaseRegister() == NULL)
         {
         return true;
         }
      switch (valueChild->getDataType())
         {
         int64_t tmp;
         case TR::Address:
            if (cg->comp()->target().is32Bit())
               {
               imm = (uintptr_t)valueChild->getAddress();
               if ((imm <= MIN_IMMEDIATE_VAL) || (imm >= MAX_IMMEDIATE_VAL))
                  return true;
               break;
               }
            // 64-bit address case falls through.
         case TR::Int64:
            tmp = (int64_t) valueChild->getLongInt();
            if ((tmp <= MIN_IMMEDIATE_VAL) || (tmp >= MAX_IMMEDIATE_VAL))
               return true;

            imm = (int32_t) tmp;
            break;
         case TR::Int32:
            imm = valueChild->getInt();
            if ((imm <= MIN_IMMEDIATE_VAL) || (imm >= MAX_IMMEDIATE_VAL))
               return true;
            break;
         case TR::Int16:
            imm = (int32_t) valueChild->getShortInt();
            break;
         case TR::Int8:
            imm = (int32_t) valueChild->getByte();
            break;
         default:
            return true;
            break;
         }
      }
   else
      {
      return true;
      }
   generateSILInstruction(cg, op, node, mf, imm);
   return false;
   }

/** \brief
 *     Attempts to perform a direct memory-memory copy while evaluating a store node.
 *
 *  \details
 *     The source value node must be resolved for now. An unresolved field requires an UnresolvedDataSnippet
 *     to patch the instruction's displacement field. This snippet is designed to patch most
 *     load instructions' displacement field, which is bit 20-31.
 *     MVC instruction has both a source and a destination displacement field. Its source is
 *     at bit 36-47 and can't be handled by the current UnresolvedDataSnippet; but the destination
 *     displacement can still , in theory, be patchable.
 *
 *  \param cg
 *     The code generator used to generate the instructions.
 *
 *  \param storeNode
 *     The store node to attempt to reduce to a memory-memory copy.
 *
 *  \return
 *     true if the store node was reduced to a memory-memory copy; false otherwise.
 *
 *  \note
 *     If this transformation succeeds the helper will decrement the reference counts of the storeNode subtree.
 *     If this transformation fails the state of the storeNode subtree remains unchanged.
 */
bool directMemoryStoreHelper(TR::CodeGenerator* cg, TR::Node* storeNode)
   {
   if (!cg->getConditionalMovesEvaluationMode())
      {
      if (!storeNode->getOpCode().isReverseLoadOrStore()
              && storeNode->getType().isIntegral()
              && !(storeNode->getOpCode().isIndirect() && storeNode->hasUnresolvedSymbolReference()))
         {
         TR::Node* valueNode = storeNode->getOpCode().isIndirect() ? storeNode->getChild(1) : storeNode->getChild(0);

         if (valueNode->getOpCode().isLoadVar() && !valueNode->getOpCode().isReverseLoadOrStore () && valueNode->isSingleRefUnevaluated() && !valueNode->hasUnresolvedSymbolReference())
            {
            // Pattern match the following trees:
            //
            // (0) xstorei b
            // (?)   A
            // (1)   xloadi y
            // (?)     X
            //
            // (0) xstorei b
            // (?)   A
            // (1)   xload y
            //
            // And reduce it to a memory-memory copy.

            TR::DebugCounter::incStaticDebugCounter(cg->comp(), "z/optimization/directMemoryStore/indirect");

            // Force the memory references to not use an index register because MVC is an SS instruction
            // After generating a memory reference, Enforce it to generate LA instruction.
            // This will avoid scenarios when we have common base/index between destination and source
            // And when generating the source memory reference, it clobber evaluates one of the node shared between
            // target memory reference as well.
            TR::MemoryReference* targetMemRef = generateS390MemoryReference(storeNode, cg, false);
            targetMemRef->enforceSSFormatLimits(storeNode, cg, NULL);

            TR::MemoryReference* sourceMemRef = generateS390MemoryReference(valueNode, cg, false);
            sourceMemRef->enforceSSFormatLimits(storeNode, cg, NULL);

            generateSS1Instruction(cg, TR::InstOpCode::MVC, storeNode, storeNode->getSize() - 1, targetMemRef, sourceMemRef);

            cg->decReferenceCount(valueNode);

            return true;
            }
         else if (valueNode->getOpCode().isConversion() && valueNode->isSingleRefUnevaluated() && !valueNode->hasUnresolvedSymbolReference())
            {
            TR::Node* conversionNode = valueNode;

            TR::Node* valueNode = conversionNode->getChild(0);

            // Make sure this is an integral truncation conversion
            if (valueNode->getOpCode().isIntegralLoadVar()
                    && !valueNode->getOpCode().isReverseLoadOrStore()
                    && valueNode->isSingleRefUnevaluated()
                    && !valueNode->hasUnresolvedSymbolReference())
               {
               if (valueNode->getSize() > storeNode->getSize())
                  {
                  // Pattern match the following trees:
                  //
                  // (0) xstorei b
                  // (?)   A
                  // (1)   z2x
                  // (1)     zloadi y
                  // (?)       X
                  //
                  // (0) xstorei b
                  // (?)   A
                  // (1)   z2x
                  // (1)     zload y
                  //
                  // And reduce it to a memory-memory copy.

                  TR::DebugCounter::incStaticDebugCounter(cg->comp(), "z/optimization/directMemoryStore/conversion/indirect");

                  // Force the memory references to not use an index register because MVC is an SS instruction
                  TR::MemoryReference* targetMemRef = generateS390MemoryReference(storeNode, cg, false);
                  TR::MemoryReference* sourceMemRef = generateS390MemoryReference(valueNode, cg, false);

                  // Adjust the offset to reference the truncated value
                  sourceMemRef->setOffset(sourceMemRef->getOffset() + valueNode->getSize() - storeNode->getSize());

                  generateSS1Instruction(cg, TR::InstOpCode::MVC, storeNode, storeNode->getSize() - 1, targetMemRef, sourceMemRef);

                  cg->decReferenceCount(conversionNode);
                  cg->decReferenceCount(valueNode);

                  return true;
                  }
               }
            }
         }
      }

   return false;
   }

TR::MemoryReference *
sstoreHelper(TR::Node * node, TR::CodeGenerator * cg, bool isReversed)
   {
   if (directMemoryStoreHelper(cg, node))
      {
      return NULL;
      }

   TR::Node * valueChild;
   TR::Register * sourceRegister = NULL;
   bool srcClobbered = false;
   TR::Compilation *comp = cg->comp();

   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   TR::MemoryReference * tempMR = NULL;
   bool longWay = false;
   if (valueChild->getOpCode().isLoadConst())
      {
      tempMR = generateS390MemoryReference(node, cg);
      longWay = storeHelperImmediateInstruction(valueChild, cg, isReversed, TR::InstOpCode::MVHHI, node, tempMR);
      if (longWay)
         {
         // We don't need to do casting op if we know that the only use of the cast
         // byte will be the STH
         sourceRegister = cg->evaluate(valueChild);
         }
      }
   else
      {
      longWay = true;
      if (valueChild->getDataType() != TR::Aggregate)
         {
         sourceRegister = cg->evaluate(valueChild);
         }
      else
         {
         sourceRegister = cg->gprClobberEvaluate(valueChild);
         if (cg->comp()->target().is64Bit())
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, sourceRegister, sourceRegister, 48);
         else
            generateRSInstruction(cg, TR::InstOpCode::SRL, node, sourceRegister, 16);
         srcClobbered = true;
         }
      tempMR = generateS390MemoryReference(node, cg);
      }
   if (longWay)
      {
      if (isReversed)
         {
         generateRXInstruction(cg, TR::InstOpCode::STRVH, node, sourceRegister, tempMR);
         }
      else
         {
         generateRXInstruction(cg, TR::InstOpCode::STH, node, sourceRegister, tempMR);
         }
      }

   if (srcClobbered)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(valueChild);

   tempMR->stopUsingMemRefRegister(cg);

   return tempMR;
   }


// TODO: The return value does not make sense here. There is no one that consumes it so it should be void. Similar
//       occurrences can be found on other store helpers. These should be fixed as well.
TR::MemoryReference *
istoreHelper(TR::Node * node, TR::CodeGenerator * cg, bool isReversed)
   {
   TR::Node * valueChild;
   TR::Node * addrChild = NULL;

   // (0)  istore #322[0x000000008028871c]  Static (profilingCode )/>
   // (0)    isub
   // (0)      iload #322[0x000000008028871c]  Static
   // (0)      iconst -1
   //
   if (node->isProfilingCode())
      {
      // Mark the load noce as profiling so that we can
      // insert an STCMH on it
      //
      //      node->getFirstChild()->getFirstChild()->setIsProfilingCode(cg->comp());
      }


   bool usingCompressedPointers = false;
   TR::Node *unCompressedValue = NULL;
   TR::Compilation *comp = cg->comp();
   TR::Node *translatedNode = NULL;

   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      addrChild = node->getFirstChild();
      if (comp->useCompressedPointers() &&
            (node->getSymbolReference()->getSymbol()->getDataType() == TR::Address))
         {
         // pattern match the sequence
         //     iistore f     iistore f         <- node
         //       aload O       aload O         <- addrChild
         //     value           l2i
         //                       lshr
         //                         lsub        <- translatedNode
         //                           a2l
         //                             value   <- valueChild
         //                           lconst HB
         //                         iconst shftKonst
         //
         // -or- if the field is known to be null
         // iistore f
         //    aload O
         //    l2i
         //      a2l
         //        value  <- valueChild
         //
         translatedNode = valueChild;
         if (translatedNode->getOpCodeValue() == TR::l2i)
            translatedNode = translatedNode->getFirstChild();
         if (translatedNode->getOpCode().isRightShift()) // optional
            translatedNode = translatedNode->getFirstChild();

         bool usingLowMemHeap = false;
         if (TR::Compiler->vm.heapBaseAddress() == 0 ||
               valueChild->isNull())
            usingLowMemHeap = true;

         if (translatedNode->getOpCode().isSub() || usingLowMemHeap)
            usingCompressedPointers = true;
         // valueChild is not updated, the compression is taken care of
         // by the lsub (which is already tagged with a nodeflag)
         //
         if (usingCompressedPointers && !usingLowMemHeap)
            {
            unCompressedValue = valueChild;
            while (unCompressedValue->getNumChildren() > 0 &&
                     unCompressedValue->getOpCodeValue() != TR::a2l)
               unCompressedValue = unCompressedValue->getFirstChild();
            if (unCompressedValue->getOpCodeValue() == TR::a2l)
               unCompressedValue = unCompressedValue->getFirstChild();
            // this allows separate registers to be allocated if the base
            // of the indirect access is the same as the value thats being
            // compressed (e.g. O.f = O)
            //
            if (unCompressedValue == addrChild)
               unCompressedValue->incReferenceCount();
            else
               unCompressedValue = NULL;
            }
         }
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   TR::MemoryReference * tempMR = NULL;

   // Try to exploit direct memory operation if all we are doing is incrementing
   // a counter in memory.
   //
   if (directToMemoryAddHelper(cg, node, valueChild, TR::InstOpCode::ASI))
      {
      return tempMR;
      }
   else if (relativeLongStoreHelper(cg, node, valueChild))
      {
      tempMR = NULL;
      }
   else if (directMemoryStoreHelper(cg, node))
      {
      return tempMR;
      }
   else
      {
      TR::Register * sourceRegister = NULL;
      bool srcClobbered = false;

      if (valueChild->getOpCode().isLoadConst())
         {
         if (storeHelperImmediateInstruction(valueChild, cg, isReversed, TR::InstOpCode::MVHI, node,
              (tempMR = generateS390MemoryReference(node, cg))))
            {
            sourceRegister = cg->evaluate(valueChild);
            }
         }
      else
         {
         if (valueChild->getDataType() != TR::Aggregate || cg->comp()->target().is32Bit())
            sourceRegister = cg->evaluate(valueChild);
         else
            {
            sourceRegister = cg->gprClobberEvaluate(valueChild);
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, sourceRegister, sourceRegister, 32);
            srcClobbered = true;
            }
         tempMR = generateS390MemoryReference(node, cg);
         }
      if (sourceRegister != NULL)
         {
         if (isReversed)
            {
            generateRXInstruction(cg, TR::InstOpCode::STRV, node, sourceRegister, tempMR);
            }
         else if (cg->getConditionalMovesEvaluationMode())
            {
            generateRSInstruction(cg, TR::InstOpCode::STOC, node, sourceRegister, cg->getRCondMoveBranchOpCond(), tempMR);
            }
         else
            {
            generateRXInstruction(cg, TR::InstOpCode::ST, node, sourceRegister, tempMR);
            }
         }
      if (srcClobbered)
         cg->stopUsingRegister(sourceRegister);
      }
   cg->decReferenceCount(valueChild);
   if (unCompressedValue)
      cg->decReferenceCount(unCompressedValue);

   return tempMR;
   }

/**
 * 64 bit version of lstore
 */
TR::MemoryReference *
lstoreHelper64(TR::Node * node, TR::CodeGenerator * cg, bool isReversed)
   {
   TR::Node * valueChild;
   TR::Node * addrChild = NULL;

   if (node->getOpCode().isIndirect())
      {
      addrChild = node->getFirstChild();
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   TR::MemoryReference * longMR = NULL;

   // Try to exploit direct memory operation if all we are doing is incrementing
   // a counter in memory.
   //
   if (directToMemoryAddHelper(cg, node, valueChild, TR::InstOpCode::AGSI))
      {
      return longMR;
      }
   else if(relativeLongStoreHelper(cg, node, valueChild))
      {
      longMR = NULL;
      }
   else if (directMemoryStoreHelper(cg, node))
      {
      return longMR;
      }
   else
      {
      TR::Register * valueReg = NULL;
      if (valueChild->getOpCode().isLoadConst())
         {
         if(storeHelperImmediateInstruction(valueChild, cg, isReversed, TR::InstOpCode::MVGHI, node,
           (longMR = generateS390MemoryReference(node,cg)) ))
            {
            valueReg = cg->evaluate(valueChild);
            }
         }
      // lload is the child, then don't evaluate the child, generate MVC to move directly among memory
      else if(!node->getOpCode().isIndirect() &&
              valueChild->getOpCodeValue() == TR::lload &&
              valueChild->getReferenceCount() == 1  &&
              valueChild->getRegister() == NULL &&
              generateS390MemoryReference(node, cg)->getIndexRegister() == NULL &&
              !cg->getConditionalMovesEvaluationMode())
         {
         TR::MemoryReference * tempMRSource = generateS390MemoryReference(valueChild, cg);
         TR::MemoryReference * tempMRDest = generateS390MemoryReference(node, cg);
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node, int16_t(node->getSize()-1),
                                tempMRDest, tempMRSource );
         cg->decReferenceCount(valueChild);
         return tempMRDest;
         }
      else
         {
         valueReg = cg->evaluate(valueChild);
         longMR = generateS390MemoryReference(node,cg);
         }
      if (valueReg != NULL)
         {
         if (isReversed)
            {
            generateRXInstruction(cg, TR::InstOpCode::STRVG, node, valueReg, longMR);
            }
         else
            {
            if (cg->getConditionalMovesEvaluationMode())
               generateRSInstruction(cg, TR::InstOpCode::STOCG, node, valueReg, cg->getRCondMoveBranchOpCond(), longMR);
            else
               generateRXInstruction(cg, TR::InstOpCode::STG, node, valueReg, longMR);
            }
         }
      }

   cg->decReferenceCount(valueChild);

   return longMR;
   }

TR::MemoryReference *
astoreHelper(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * valueChild;
   TR::Node * addrChild = NULL;
   TR::Compilation *comp = cg->comp();

   if (node->getOpCode().isIndirect())
      {
      addrChild = node->getFirstChild();
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   TR::MemoryReference * tempMR = NULL;

   if (!relativeLongStoreHelper(cg, node, valueChild))
      {
      // Determine the Store OpCode.
      TR::InstOpCode::Mnemonic storeOp = TR::InstOpCode::getStoreOpCode();

      if (cg->isCompressedClassPointerOfObjectHeader(node))
         {
         storeOp = TR::InstOpCode::ST;
         }

      TR::InstOpCode::Mnemonic mvhiOp = TR::InstOpCode::getMoveHalfWordImmOpCodeFromStoreOpCode(storeOp);

      TR::Register* sourceRegister = NULL;

      // Try Move HalfWord Immediate instructions first
      if (mvhiOp != TR::InstOpCode::BAD &&
          valueChild->getOpCode().isLoadConst() &&
          !cg->getConditionalMovesEvaluationMode())
         {
         tempMR = generateS390MemoryReference(node, cg);
         if (storeHelperImmediateInstruction(valueChild, cg, false, mvhiOp, node, tempMR))
            {
            // Move Halfword Immediate Instruction failed.  Evaluate valueChild now.
            // Typically, we should evaluate valueChild first before
            // generating the memory reference. However, given the valueChild
            // is a constant, we can safely reverse the order.
            sourceRegister = cg->evaluate(valueChild);
            }
         else
            {
            // Successfully generated Move Halfword Immediate instruction
            // Set storeOp to TR::InstOpCode::BAD, so we do not explicitly generate a
            // store instruction later.
            storeOp = TR::InstOpCode::BAD;
            }
         }
      // aload is the child, then don't evaluate the child, generate MVC to move directly among memory
      else if(!node->getOpCode().isIndirect() &&
              valueChild->getOpCodeValue() == TR::aload &&
              valueChild->getReferenceCount() == 1 &&
              valueChild->getRegister() == NULL &&
              generateS390MemoryReference(node, cg)->getIndexRegister() == NULL &&
              !valueChild->getSymbolReference()->isLiteralPoolAddress() &&
        node->getSymbol()->getSize() == node->getSize() &&
        node->getSymbol()->getSize() == valueChild->getSymbol()->getSize() &&
        !cg->getConditionalMovesEvaluationMode())
         {
         int16_t length = 0;
         int nodeSymbolSize = node->getSymbol()->getSize();
         if (nodeSymbolSize == 8)
            length = 8;
         else if (nodeSymbolSize == 1)
            length = 1;
         else if (nodeSymbolSize == 2)
            length = 2;
         else if (nodeSymbolSize == 3)
            length = 3;
         else
            length = 4;
         TR::MemoryReference * tempMRSource = generateS390MemoryReference(valueChild, cg);
         tempMR = generateS390MemoryReference(node, cg);
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node, length-1, tempMR, tempMRSource);
         cg->decReferenceCount(valueChild);
         return tempMR;
         }
      else
         {
         // Normal non-constant path
         sourceRegister = cg->evaluate(valueChild);
         tempMR = generateS390MemoryReference(node, cg);
         }

      // Generate the Store instruction unless storeOp is TR::InstOpCode::BAD (i.e. Move
      // Halfword Immediate instruction was generated).
      if (storeOp == TR::InstOpCode::STCM)
         generateRSInstruction(cg, TR::InstOpCode::STCM, node, sourceRegister, (uint32_t) 0x7, tempMR);
      else if (storeOp != TR::InstOpCode::BAD)
         {
         if (cg->getConditionalMovesEvaluationMode())
            generateRSInstruction(cg, (storeOp == TR::InstOpCode::STG)? TR::InstOpCode::STOCG : TR::InstOpCode::STOC, node, sourceRegister, cg->getRCondMoveBranchOpCond(), tempMR);
         else
            generateRXInstruction(cg, storeOp, node, sourceRegister, tempMR);
         }
      }

   cg->decReferenceCount(valueChild);

   return tempMR;
   }

/**
 * aload Evaluator: load address
 *   - also handles aload and iaload
 */
TR::Register *
OMR::Z::TreeEvaluator::aloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return aloadHelper(node, cg, NULL);
   }

TR::Register *
OMR::Z::TreeEvaluator::aiaddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return OMR::Z::TreeEvaluator::axaddEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::aladdEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return OMR::Z::TreeEvaluator::axaddEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::axaddEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register * targetRegister = cg->allocateRegister();
   TR::Node * firstChild = node->getFirstChild();
   TR::MemoryReference * axaddMR = generateS390MemoryReference(cg);

   axaddMR->populateAddTree(node, cg);
   axaddMR->eliminateNegativeDisplacement(node, cg);
   axaddMR->enforceDisplacementLimit(node, cg, NULL);

   if (node->isInternalPointer())
      {
      if (node->getPinningArrayPointer())
         {
         targetRegister->setContainsInternalPointer();
         targetRegister->setPinningArrayPointer(node->getPinningArrayPointer());
         }
      else if (firstChild->getOpCodeValue() == TR::aload &&
         firstChild->getSymbolReference()->getSymbol()->isAuto() &&
         firstChild->getSymbolReference()->getSymbol()->isPinningArrayPointer())
         {
         targetRegister->setContainsInternalPointer();
         if (!firstChild->getSymbolReference()->getSymbol()->isInternalPointer())
            {
            targetRegister->setPinningArrayPointer(firstChild->getSymbolReference()->getSymbol()->castToAutoSymbol());
            }
         else
            {
            targetRegister->setPinningArrayPointer(firstChild->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }
         }
      else if (firstChild->getRegister() != NULL && firstChild->getRegister()->containsInternalPointer())
         {
         targetRegister->setContainsInternalPointer();
         targetRegister->setPinningArrayPointer(firstChild->getRegister()->getPinningArrayPointer());
         }
      }

   generateRXInstruction(cg, TR::InstOpCode::LA, node, targetRegister, axaddMR);
   node->setRegister(targetRegister);

   return targetRegister;
   }

/**
 * Indirect Load Evaluators:
 *   iiload handled by iloadEvaluator
 *   ilload handled by lloadEvaluator
 *   ialoadEvaluator handled by aloadEvaluator
 *   ibloadEvaluator handled by bloadEvaluator
 *   isloadEvaluator handled by sloadEvaluator
 *   icloadEvaluator handled by sloadEvaluator
 *
 * riload Evaluator: load integer reversed
 * handles ruiload too
 */
TR::Register *
OMR::Z::TreeEvaluator::riloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return iloadHelper(node, cg, NULL, true);
   }

/**
 * rlload Evaluator: load long integer reversed
 * handles rulload too
 */
TR::Register *
OMR::Z::TreeEvaluator::rlloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return lloadHelper64(node, cg, NULL, true);
   }

/**
 * rsload Evaluator: load short integer reversed
 * handles rcload too
 */
TR::Register *
OMR::Z::TreeEvaluator::rsloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return sloadHelper(node, cg, NULL, true);
   }

/**
 * iload Evaluator: load integer
 *   - also handles iiload
 */
TR::Register *
OMR::Z::TreeEvaluator::iloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return iloadHelper(node, cg, NULL);
   }


/**
 * lload Evaluator: load long integer
 *   - also handles ilload
 */
TR::Register *
OMR::Z::TreeEvaluator::lloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return lloadHelper64(node, cg, NULL);
   }

/**
 * sload Evaluator: load short integer
 *   - also handles cload, isload and icload
 */
TR::Register *
OMR::Z::TreeEvaluator::sloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return sloadHelper(node, cg, NULL);
   }

/**
 * bload Evaluator: load byte
 *   - also handles ibload
 */
TR::Register *
OMR::Z::TreeEvaluator::bloadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();

   TR::Register * tempReg;

   tempReg = cg->allocateRegister();
   TR::MemoryReference * tempMR;

   tempMR = generateS390MemoryReference(node, cg);
   tempReg = genericLoad<8>(node, cg, tempMR, tempReg);
   node->setRegister(tempReg);

   tempMR->stopUsingMemRefRegister(cg);
   return tempReg;
   }

/**
 * Indirect Store Evaluators:
 *  ilstore handled by lstoreEvaluator
 *  iistore handled by istoreEvaluator
 *  iastoreEvaluator handled by istoreEvaluator
 *  ibstoreEvaluator handled by bstoreEvaluator
 *  isstoreEvaluator handled by sstoreEvaluator
 *  icstoreEvaluator handled by cstoreEvaluator
 */
/**
 * ristoreEvaluator - store integer reversed
 */
TR::Register *
OMR::Z::TreeEvaluator::ristoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   istoreHelper(node, cg, true);
   return NULL;
   }

/**
 * rlstoreEvaluator - store long integer reversed
 */
TR::Register *
OMR::Z::TreeEvaluator::rlstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   lstoreHelper64(node, cg, true);
   return NULL;
   }

/**
 * rsstoreEvaluator - store short integer reversed
 */
TR::Register *
OMR::Z::TreeEvaluator::rsstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   sstoreHelper(node, cg, true);
   return NULL;
   }

/**
 * istoreEvaluator - store integer
 *  - also used for istorei
 */
TR::Register *
OMR::Z::TreeEvaluator::istoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   bool adjustRefCnt = false;

   istoreHelper(node, cg);
   if (adjustRefCnt)
      node->getFirstChild()->decReferenceCount();
   if (comp->useCompressedPointers() && node->getOpCode().isIndirect())
      node->setStoreAlreadyEvaluated(true);

   return NULL;
   }

/**
 * lstoreEvaluator - store long integer
 */
TR::Register *
OMR::Z::TreeEvaluator::lstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   lstoreHelper64(node, cg);

   return NULL;
   }

/**
 * sstoreEvaluator - store short integer
 *  - also handles isstore
 */
TR::Register *
OMR::Z::TreeEvaluator::sstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   sstoreHelper(node, cg);
   return NULL;
   }

/**
 * cstoreEvaluator - store unsigned short integer
 *  - also handles icstore
 */
TR::Register *
OMR::Z::TreeEvaluator::cstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   sstoreHelper(node, cg);
   return NULL;
   }

/**
 * astoreEvaluator - store address
 *  - also used for iastore
 */
TR::Register *
OMR::Z::TreeEvaluator::astoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   astoreHelper(node, cg);
   return NULL;
   }

/**
 * bstoreEvaluator - load byte
 *  - also handles ibstore
 */
TR::Register *
OMR::Z::TreeEvaluator::bstoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   if (directMemoryStoreHelper(cg, node))
      {
      return NULL;
      }

   TR::Compilation *comp = cg->comp();
   TR::Node * valueChild, *memRefChild;
   TR::Register * sourceRegister = NULL;
   bool decSubFirstChild = false;
   bool srcClobbered = false;

   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      memRefChild = node->getFirstChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      memRefChild = NULL;
      }

   // Use MVI when we can
   if (valueChild->getRegister()==NULL &&
      valueChild->getOpCodeValue() == TR::bconst)
      {
      TR::MemoryReference * tempMR = generateS390MemoryReference(node, cg);
      generateSIInstruction(cg, TR::InstOpCode::MVI, node, tempMR, valueChild->getByte()&0xFF);

      cg->decReferenceCount(valueChild);
      tempMR->stopUsingMemRefRegister(cg);

      return NULL;
      }
   // Check for bit op pattern
   // ibstore
   //   addr
   //   bor/band/bxor
   //     ibload
   //       =>addr
   //     bconst
   else if (((valueChild->getOpCodeValue() == TR::bor) ||
             (valueChild->getOpCodeValue() == TR::band) ||
             (valueChild->getOpCodeValue() == TR::bxor)) &&
            cg->isInMemoryInstructionCandidate(node) &&
            // trying to use OI, NI, XI, so second child of RX op code must be load const
            valueChild->getSecondChild()->getOpCode().isLoadConst() &&
            performTransformation(comp, "O^O for bstore [%p], emitting single in-memory instruction OI/NI/XI\n", node))
      {
      // Attempt using only one instruction
      TR::InstOpCode::Mnemonic instrOpCode = TR::InstOpCode::NOP;

      switch (valueChild->getOpCodeValue())
         {
         case TR::bor:
            instrOpCode = TR::InstOpCode::OI;
            break;
         case TR::band:
            instrOpCode = TR::InstOpCode::NI;
            break;
         case TR::bxor:
            instrOpCode = TR::InstOpCode::XI;
            break;
         default:
            TR_ASSERT(false, "unexpected bitOpMem op on node %p\n", node);
            break;
         }

      int8_t byteConst = valueChild->getSecondChild()->getByte();
      TR::MemoryReference * tmp_mr = generateS390MemoryReference(node, cg);
      generateSIInstruction(cg, instrOpCode, node, tmp_mr, byteConst);

      // cg->decReferenceCount(node->getChild(0)); // addr
      cg->recursivelyDecReferenceCount(node->getChild(1)); // value child

      return NULL;
      }
   else if ( valueChild->getRegister() != NULL )
      {
      sourceRegister = cg->evaluate(valueChild);
      }
   // We don't need to do casting op if we know that the only use of the cast
   // byte will be the STC
   else if ( (valueChild->getOpCodeValue() == TR::l2b  ||
              valueChild->getOpCodeValue() == TR::i2b  ||
              valueChild->getOpCodeValue() == TR::s2b) &&
             (valueChild->getReferenceCount() == 1))
      {
      // if the x2b child was not evaluated, it would have decremented its 1st child when it
      // was actually evaluated.
      sourceRegister = cg->evaluate(valueChild->getFirstChild());
      TR::RegisterPair *regPair = sourceRegister->getRegisterPair();
      if (regPair)
         {
         sourceRegister = regPair->getLowOrder();
         }

      if (valueChild->getRegister() == NULL)
         {
         decSubFirstChild = true;
         valueChild->setRegister(sourceRegister);
         }
      }
   // If the only consumer is the bstore, then don't bother extending
   //
   else if (valueChild->getReferenceCount() == 1 && valueChild->getRegister() == NULL &&
             (valueChild->getOpCodeValue() == TR::buload  ||
              valueChild->getOpCodeValue() == TR::bload   ||
              valueChild->getOpCodeValue() == TR::buloadi ||
              valueChild->getOpCodeValue() == TR::buload      ))
      {
      sourceRegister = cg->allocateRegister();
      TR::MemoryReference * tempMR2 = generateS390MemoryReference(valueChild, cg);
      generateRXInstruction(cg, TR::InstOpCode::IC, valueChild, sourceRegister, tempMR2);

      valueChild->setRegister(sourceRegister);
      cg->stopUsingRegister(sourceRegister);
      }
   else
      {
      if (valueChild->getDataType() != TR::Aggregate)
         {
         sourceRegister = cg->evaluate(valueChild);
         }
      else
         {
         sourceRegister = cg->gprClobberEvaluate(valueChild);
         if (cg->comp()->target().is64Bit())
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, sourceRegister, sourceRegister, 56);
         else
            generateRSInstruction(cg, TR::InstOpCode::SRL, node, sourceRegister, 24);
         srcClobbered = true;
         }
      }

   TR::MemoryReference * tempMR = generateS390MemoryReference(node, cg);
   generateRXInstruction(cg, TR::InstOpCode::STC, node, sourceRegister, tempMR);

   if (srcClobbered)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(valueChild);
   if (decSubFirstChild)
      {
      cg->decReferenceCount(valueChild->getFirstChild());
      }
   tempMR->stopUsingMemRefRegister(cg);

   return NULL;
   }

TR::Register *
OMR::Z::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::astoreEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::astoreEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::fwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::fstoreEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::fwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::fstoreEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::dwrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getSecondChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::dwrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The wrtbar IL op represents a store with side effects.
   // Currently we don't use the side effect node. So just evaluate it and decrement the reference count.
   TR::Node *sideEffectNode = node->getThirdChild();
   cg->evaluate(sideEffectNode);
   cg->decReferenceCount(sideEffectNode);
   // Delegate the evaluation of the remaining children and the store operation to the storeEvaluator.
   return TR::TreeEvaluator::dstoreEvaluator(node, cg);
   }

/**
 * performCall - build dispatch method
 */
TR::Register *
OMR::Z::TreeEvaluator::performCall(TR::Node * node, bool isIndirect, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::MethodSymbol * callSymbol = symRef->getSymbol()->castToMethodSymbol();

   TR::Register * returnRegister;
   if (isIndirect)
      {
      returnRegister = (cg->getLinkage(callSymbol->getLinkageConvention()))->buildIndirectDispatch(node);
      }
   else
      {
      //Generate Fast JNI direct call
      if (callSymbol->isJNI() && node->isPreparedForDirectJNI())
         returnRegister = (cg->getLinkage(TR_J9JNILinkage))->buildDirectDispatch(node);
      else
         returnRegister = (cg->getLinkage(callSymbol->getLinkageConvention()))->buildDirectDispatch(node);
      }

   return returnRegister;
   }

void
OMR::Z::TreeEvaluator::checkAndSetMemRefDataSnippetRelocationType(TR::Node * node,
                                                                  TR::CodeGenerator * cg,
                                                                  TR::MemoryReference* tempMR)
   {
   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::Symbol * symbol = symRef->getSymbol();
   bool isStatic = symbol->isStatic() && !symRef->isUnresolved();

   if (cg->comp()->compileRelocatableCode())
      {
      int32_t reloType;
      if (node->getSymbol()->isDebugCounter())
         reloType = TR_DebugCounter;
      else if (node->getSymbol()->isConst())
         reloType = TR_ConstantPool;
      else if (node->getSymbol()->isClassObject())
         {
         reloType = TR_ClassAddress;
         }
      else if (node->getSymbol()->isMethod())
         reloType = TR_MethodObject;
      else if (isStatic && !node->getSymbol()->isNotDataAddress())
         reloType = TR_DataAddress;
      else
         reloType = 0;

      if (reloType != 0)
         {
         if (tempMR->getConstantDataSnippet())
            {
            tempMR->getConstantDataSnippet()->setSymbolReference(tempMR->getSymbolReference());
            tempMR->getConstantDataSnippet()->setReloType(reloType);
            }
         }
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::checkAndAllocateReferenceRegister(TR::Node * node,
                                                         TR::CodeGenerator * cg,
                                                         bool& dynLitPoolLoad)
   {
   TR::Register* tempReg = NULL;
   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::Symbol * symbol = symRef->getSymbol();
   TR::Node *constNode = (TR::Node *) (node->getSymbolReference()->getOffset());

   // Dynamic literal pool can change loadaddr of static sym ref to aloadi
   // we have to detect this case and allocate collectable/non-collectable
   // register based on loadaddr logic
   // the aloadi that came from loadaddr will have the following structure
   //
   //          aloadi <static sym ref>
   //             aload (literal pool base)
   if ((node->getOpCodeValue() == TR::aloadi) &&
         symbol->isStatic() &&
         node->getSymbolReference()->isFromLiteralPool() &&
         (node->getNumChildren() > 0 && node->getFirstChild()->getOpCode().hasSymbolReference() &&
         (node->getFirstChild()->getSymbolReference()->isLiteralPoolAddress() ||
          (!node->getFirstChild()->getSymbol()->isStatic() &&
           node->getFirstChild()->getSymbolReference()->isFromLiteralPool()))))
      {
      dynLitPoolLoad = true;

      if (symbol->isLocalObject())
         {
         tempReg = cg->allocateCollectedReferenceRegister();
         }
      else
         {
         tempReg = cg->allocateRegister();
         }
      }
   else    //regular-born aload/aloadi
      {
      tempReg = cg->allocateRegister();

      if (!symbol->isInternalPointer()  &&
          !symbol->isNotCollected()     &&
          !symbol->isAddressOfClassObject() &&
          // LowerTrees transforms unloadable aconsts to iaload <gen. int shadow> / aconst NULL.  All aconsts are never collectable.
          !(symRef->isLiteralPoolAddress() && (node->getOpCodeValue() == TR::aloadi) && constNode->isClassUnloadingConst()))
         {
         tempReg->setContainsCollectedReference();
         }
      }

   traceMsg(cg->comp(), "aload reg contains ref: %d\n", tempReg->containsCollectedReference());
   return tempReg;
   }

/**
 * Helper to generate instructions for z990 pop count
 */
TR::Register *
OMR::Z::TreeEvaluator::z990PopCountHelper(TR::Node *node, TR::CodeGenerator *cg, TR::Register *inReg, TR::Register *outReg)
   {
   TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);

   TR::InstOpCode::Mnemonic subOp = cg->comp()->target().is64Bit() ? TR::InstOpCode::SGR : TR::InstOpCode::SR;
   TR::InstOpCode::Mnemonic addOp = cg->comp()->target().is64Bit() ? TR::InstOpCode::AGR : TR::InstOpCode::AR;

   // check if 0 to skip below instructions
   generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, outReg, outReg);
   generateRIInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::CGHI : TR::InstOpCode::CHI, node, inReg, 0);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRE, node, cFlowRegionEnd);
   TR::RegisterDependencyConditions *regDeps =
         new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 7, cg);

   regDeps->addPostCondition(inReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(outReg, TR::RealRegister::AssignAny);

   // x -= (x >> 1) & 0x555555555
   generateRSInstruction(cg, TR::InstOpCode::SRLG, node, outReg, inReg, 1);
   if (cg->comp()->target().is64Bit())
     generateS390ImmOp(cg, TR::InstOpCode::N, node, outReg, outReg, (int64_t) 0x5555555555555555, regDeps);
   else
     generateS390ImmOp(cg, TR::InstOpCode::N, node, outReg, outReg, (int32_t) 0x55555555, regDeps);
   generateRRInstruction(cg, subOp, node, inReg, outReg);

   // x = (x & 0x33333333) + ((x >> 2) & 0x33333333)
   generateRSInstruction(cg, TR::InstOpCode::SRLG, node, outReg, inReg, 2);
   if (cg->comp()->target().is64Bit())
      {
      generateS390ImmOp(cg, TR::InstOpCode::N, node, outReg, outReg, (int64_t) 0x3333333333333333, regDeps);
      generateS390ImmOp(cg, TR::InstOpCode::N, node, inReg, inReg, (int64_t) 0x3333333333333333, regDeps);
      }
   else
      {
      generateS390ImmOp(cg, TR::InstOpCode::N, node, outReg, outReg, (int32_t) 0x33333333, regDeps);
      generateS390ImmOp(cg, TR::InstOpCode::N, node, inReg, inReg, (int32_t) 0x33333333, regDeps);
      }
   generateRRInstruction(cg, addOp, node, inReg, outReg);

   // x = (x + (x >> 4)) & 0x0f0f0f0f
   generateRSInstruction(cg, TR::InstOpCode::SRLG, node, outReg, inReg, 4);
   generateRRInstruction(cg, addOp, node, inReg, outReg);
   if (cg->comp()->target().is64Bit())
     generateS390ImmOp(cg, TR::InstOpCode::N, node, inReg, inReg, (int64_t) 0x0f0f0f0f0f0f0f0f, regDeps);
   else
     generateS390ImmOp(cg, TR::InstOpCode::N, node, inReg, inReg, (int32_t) 0x0f0f0f0f, regDeps);

   // x += x >> 8
   generateRSInstruction(cg, TR::InstOpCode::SRLG, node, outReg, inReg, 8);
   generateRRInstruction(cg, addOp, node, inReg, outReg);

   // x += x >> 16
   generateRSInstruction(cg, TR::InstOpCode::SRLG, node, outReg, inReg, 16);
   generateRRInstruction(cg, addOp, node, inReg, outReg);

   // x += x > 32
   generateRSInstruction(cg, TR::InstOpCode::SRLG, node, outReg, inReg, 32);
   generateRRInstruction(cg, addOp, node, outReg, inReg);

   // x = x & 0x7f
   if (cg->comp()->target().is64Bit())
     generateS390ImmOp(cg, TR::InstOpCode::N, node, outReg, outReg, (int64_t) 0x000000000000007f, regDeps);
   else
     generateS390ImmOp(cg, TR::InstOpCode::N, node, outReg, outReg, (int32_t) 0x0000007f, regDeps);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, regDeps);
   cFlowRegionEnd->setEndInternalControlFlow();

   return outReg;
   }

/**
 * Used when inlining Integer|Long.numberOfTrailingZeros()
 * or builtin_ctz/builtin_ctzll
 */
extern  TR::Register *
inlineNumberOfTrailingZeros(
      TR::Node *node,
      TR::CodeGenerator *cg,
      int32_t subfconst)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineNumberOfTrailingZeros");
   TR::Node *argNode = node->getFirstChild();
   TR::Register *argReg = cg->evaluate(argNode);
   TR::Register *tempReg = cg->allocateRegister();
   TR::Register *returnReg = NULL;
   bool isLong = (subfconst == 64);

   // Use identity x & -x to isolate the rightmost 1-bit (eg. 10110100 => 00000100)
   // generate x & -x
   generateRRInstruction(cg, TR::InstOpCode::LCGR, node, tempReg, argReg);
   generateRRInstruction(cg, TR::InstOpCode::NGR, node, tempReg, argReg);

   // Use FLOGR to count leading zeros, and xor from 63 to get trailing zeroes (FLOGR counts on the full 64 bit register)
   TR::Register *highReg = cg->allocateRegister();
   TR::Register *flogrRegPair = cg->allocateConsecutiveRegisterPair(tempReg, highReg);

   // Java needs to return 64/32 if no one bit found
   // This helps that case
   if (!isLong)
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempReg, tempReg, 32);

   generateRIInstruction(cg, TR::InstOpCode::AGHI, node, tempReg, -1);

   // CLZ
   generateRREInstruction(cg, TR::InstOpCode::FLOGR, node, flogrRegPair, tempReg);

   generateRIInstruction(cg, TR::InstOpCode::AGHI, node, highReg, -subfconst);
   generateRRInstruction(cg, TR::InstOpCode::LCR, node, highReg, highReg);

   node->setRegister(highReg);
   returnReg = highReg;
   cg->stopUsingRegister(flogrRegPair);

   cg->decReferenceCount(argNode);
   cg->stopUsingRegister(argReg);
   return returnReg;
   }

/**
 * Used when inlining Integer|Long.highestOneBit()
 */
extern TR::Register *
inlineHighestOneBit(
      TR::Node *node,
      TR::CodeGenerator *cg,
      bool isLong)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineHighestOneBit");

   TR::Node * firstChild = node->getFirstChild();

   /**
    Need to make RA aware that we'll be requiring 64-bit registers in 32-bit mode.
    For this, we place all required registers in a dependency, and hang it off
    the last instruction in this inlined sequence.
    **/

   // need to clobber evaluate all cases except when in 64-bit long
   TR::Register * srcReg = NULL;
   bool srcClobbered = false;
   if (isLong)
      srcReg = cg->evaluate(firstChild);
   else
      {
      srcClobbered = true;
      srcReg = cg->gprClobberEvaluate(firstChild);
      }
   cg->decReferenceCount(firstChild);

   TR::Instruction * cursor = NULL;

   // Prepare the result register pair
   TR::Register * evenReg = cg->allocateRegister();
   TR::Register * oddReg = cg->allocateRegister();
   TR::Register * pairReg = cg->allocateConsecutiveRegisterPair(oddReg, evenReg);

   // for Integer APIs:
   //  AND out high order to get proper FLOGR val for a 32-bit sint
   if (!isLong)
      {
      cursor = generateRILInstruction(cg, TR::InstOpCode::NIHF, node, srcReg, 0x0);
      }

   cursor = generateRRInstruction(cg, TR::InstOpCode::FLOGR, node, pairReg, srcReg);

   // perform XOR for find highest one bit
   TR::Register * retReg = cg->allocateRegister();
   cursor = generateRRInstruction(cg, TR::InstOpCode::LGR, node, retReg, oddReg);
   cursor = generateRRInstruction(cg, TR::InstOpCode::XGR, node, retReg, srcReg);

   TR::RegisterDependencyConditions * flogrDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
   flogrDeps->addPostCondition(srcReg, TR::RealRegister::AssignAny);

   // these are required regardless
   flogrDeps->addPostCondition(pairReg, TR::RealRegister::EvenOddPair);
   flogrDeps->addPostCondition(evenReg, TR::RealRegister::LegalEvenOfPair);
   flogrDeps->addPostCondition(oddReg, TR::RealRegister::LegalOddOfPair);
   flogrDeps->addPostCondition(retReg, TR::RealRegister::AssignAny);

   cursor->setDependencyConditions(flogrDeps);

   node->setRegister(retReg);
   cg->stopUsingRegister(pairReg);
   if (srcClobbered)
      cg->stopUsingRegister(srcReg);
   return retReg;
   }

/**
 * Used when inlining Integer|Long.numberOfLeadingZeros
 * or builtin_clz/builtin_clzll
 */
extern TR::Register *
inlineNumberOfLeadingZeros(
      TR::Node *node,
      TR::CodeGenerator * cg,
      bool isLong)
   {
   TR_ASSERT(node->getNumChildren() == 1, "Wrong number of children in inlineNumberOfLeadingZeros");
   TR::Node *argNode = node->getChild(0);
   TR::Register *argReg = cg->gprClobberEvaluate(argNode);
   TR::Register *returnReg = NULL;

   if (!isLong)
      {
      TR::Register *tempReg = cg->allocateRegister();

      // 32 bit number: shift left by 32 bits and count from there
      generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempReg, argReg, 32);

      // Java methods need to return 32 if no one bit is found
      // Hack by setting the 32nd bit to 1, which doesn't affect anything except when the input is 0
      generateRIInstruction(cg, TR::InstOpCode::OILH, node, tempReg, 0x8000);

      cg->stopUsingRegister(argReg);
      argReg = tempReg;
      }

   TR::Register *lowReg = cg->allocateRegister();
   TR::Register *flogrReg = cg->allocateConsecutiveRegisterPair(lowReg, argReg);
   generateRREInstruction(cg, TR::InstOpCode::FLOGR, node, flogrReg, argReg);

   returnReg = flogrReg->getHighOrder();
   node->setRegister(flogrReg->getHighOrder());
   cg->stopUsingRegister(flogrReg);

   cg->stopUsingRegister(argReg);
   cg->decReferenceCount(argNode);
   return returnReg;
   }

TR::Register *getConditionCode(TR::Node *node, TR::CodeGenerator *cg, TR::Register *programRegister)
   {
   if (!programRegister)
      {
      programRegister = cg->allocateRegister();
      }

   generateRRInstruction(cg, TR::InstOpCode::IPM, node, programRegister, programRegister);
   if (cg->comp()->target().is64Bit())
      {
      generateRRInstruction(cg, TR::InstOpCode::LLGTR, node, programRegister, programRegister);
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, programRegister, programRegister, 28);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, programRegister, 28);
      }
   return programRegister;
   }

TR::RegisterDependencyConditions *getGLRegDepsDependenciesFromIfNode(TR::CodeGenerator *cg,
                                                                               TR::Node* ificmpNode)
   {
   TR::RegisterDependencyConditions *deps = NULL;
   if (ificmpNode != NULL && ificmpNode->getNumChildren() == 3)
      {
      TR::Node *thirdChild = ificmpNode->getChild(2);
      TR_ASSERT(thirdChild->getOpCodeValue() == TR::GlRegDeps,
             "The third child of a compare is assumed to be a TR::GlRegDeps, but wasn't");

      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      cg->decReferenceCount(thirdChild);
      }
   return deps;
   }

/**
 *
 * @param node
 * @param cg
 * @param lowlowC
 * @param lowhighC
 * @param highlowC
 * @param highhighC
 * @param lowA       will be clobered
 * @param highA      will be clobered
 * @param lowB
 * @param highB
 */
void
inline128Multiply(TR::Node * node, TR::CodeGenerator * cg,
      TR::Register * lowlowC, TR::Register * lowhighC, TR::Register * highlowC, TR::Register * highhighC,
      TR::Register * lowA, TR::Register * highA,
      TR::Register * lowB, TR::Register * highB)
   {
   // There are alot more registers here then really needed, but since this was intentionally written
   // not to have internal control flow, RA can do its job properly, so mostly try not to reuse registers
   TR::Register *lowZ0 = lowlowC;
   TR::Register *highZ0 = lowhighC;
   TR::Register *lowZ1 = lowA;
   TR::Register *highZ1 = cg->allocateRegister();
   TR::Register *lowZ2 = highlowC;
   TR::Register *highZ2 = highhighC;
   TR::Register *lowZ1part = highA;
   TR::Register *highZ1part = cg->allocateRegister();
   TR::Register *Sum_high = cg->allocateRegister();

   TR::RegisterPair * Z0 = cg->allocateConsecutiveRegisterPair(lowZ0, highZ0);
   TR::RegisterPair * Z1 = cg->allocateConsecutiveRegisterPair(lowZ1, highZ1);
   TR::RegisterPair * Z1part = cg->allocateConsecutiveRegisterPair(lowZ1part, highZ1part);
   TR::RegisterPair * Z2 = cg->allocateConsecutiveRegisterPair(lowZ2, highZ2);

   /**
    * Method parameters:
    *
    *  A:
    *                                  +-----------------------------+
    *                                  |    highA     |     lowA     |
    *                                  +-----------------------------+
    *  B:
    *                                  +-----------------------------+
    *                                  |    highB     |     lowB     |
    *                                  +-----------------------------+
    *  C:
    *    +--------------+--------------+--------------+--------------+
    *    |  highhighC   |   highlowC   |   lowhighC   |   lowlowC    |
    *    +--------------+--------------+--------------+--------------+
    */

   /**
    * [highZ0,lowZ0] = lowA * lowB    // 64-bit multiply
    * [highZ2,lowZ2] = highA * highB    // 64-bit multiply
    *
    * [highZ1,lowZ1] = lowA * highB + highA * lowB // two 64-bit multiply
    *
    * Then interpolate:
    *  +-----------------------------+
    *  |    highZ2    |     lowZ2    |
    *  +-----------------------------+
    *                 +-----------------------------+
    *               C<|    highZ1    |     lowZ1    |
    *                 +-----------------------------+
    *                                +-----------------------------+
    *                                |    highZ0    |     lowZ0    |
    *                                +-----------------------------+
    *
    * Breaking down above formula for [highZ1,lowZ1]:
    *
    *    [highZ1',lowZ1']       = A0 * B1    // 64-bit multiply
    *    [highZ1part,lowZ1part] = A1 * B0    // 64-bit multiply
    *
    *    [Sum_high,highZ1,lowZ1] = [highZ1',lowZ1'] + [highZ1part,lowZ1part]
    *
    * Full break down with alignment:
    *
    *  +--------------------------------------------+    \
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |     |             +-----------------------------+
    *  +--------------------------------------------+      \   Sum_high<|    highZ1    |     lowZ1    |
    *                 +-----------------------------+      /            +-----------------------------+
    *                 |    highZ2    |     lowZ2    |     |
    *                 +-----------------------------+    /
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *
    */

   // Multiply [highZ0,lowZ0] = lowA * lowB
   TR::Instruction *cursor =
   generateRRInstruction(cg, TR::InstOpCode::LGR, node, lowZ0, lowA); // Destructive MUL
   generateRRInstruction(cg, TR::InstOpCode::MLGR, node, Z0, lowB);

   // Multiply [highZ1,lowZ1] = lowA * highB
   generateRRInstruction(cg, TR::InstOpCode::LGR, node, lowZ2, highA); // Destructive MUL
   generateRRInstruction(cg, TR::InstOpCode::MLGR, node, Z2, highB);

   // Multiply [highZ1part,lowZ1part] = highA * lowB
   //generateRRInstruction(cg, TR::InstOpCode::LGR, node, lowZ1part, highA); // Destructive MUL
   generateRRInstruction(cg, TR::InstOpCode::MLGR, node, Z1part, lowB);

   // Multiply [highZ1,lowZ1] = lowA * highB
   //generateRRInstruction(cg, TR::InstOpCode::LGR, node, lowZ1, lowA); // Destructive MUL
   generateRRInstruction(cg, TR::InstOpCode::MLGR, node, Z1, highB);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "128bit multiplications");

   /**
    * Finish calculating Z1:
    *  +--------------------------------------------+
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |
    *  +--------------------------------------------+
    *                 +-----------------------------+
    *                 |  highZ1part  | lowZ1part    |
    *                 +-----------------------------+
    */

   cursor =
   generateRRInstruction(cg, TR::InstOpCode::ALGR, node, lowZ1, lowZ1part);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, highZ1, highZ1part);
   generateRIInstruction(cg, TR::InstOpCode::LGHI, node, Sum_high, 0);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Sum_high, Sum_high);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "128bit Finish calculating Z1");

   /**
    * Final addition:
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *  +--------------------------------------------+
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |
    *  +--------------------------------------------+
    *
    *  Equals to:
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *
    */

   cursor =
   generateRRInstruction(cg, TR::InstOpCode::ALGR, node, highZ0, lowZ1);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, lowZ2, highZ1);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, highZ2, Sum_high);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "128bit Final addition");

   cg->stopUsingRegister(highZ1);
   cg->stopUsingRegister(highZ1part);
   cg->stopUsingRegister(Sum_high);

   cg->stopUsingRegister(Z0);
   cg->stopUsingRegister(Z1);
   cg->stopUsingRegister(Z1part);
   cg->stopUsingRegister(Z2);
   }

/**
 * Helper routine to calculate 128bit*128bit wide multiply. Result will be in two registers (256 bit)
 *
 * @param lowC       allocated TR_VRF register, will contain low half of result
 * @param highC      allocated TR_VRF register, will contain high half of result
 * @param Aarr       allocated TR_GPR register, base pointer to multiplicand 1
 * @param Aoff       allocated TR_GPR register, may be NULL, offset pointer to multiplicand 1
 * @param AimmOff    integral offset... (does include the array header??)
 * @param Barr       allocated TR_GPR register, base pointer to multiplicand 2
 * @param Boff       allocated TR_GPR register, may be NULL, offset pointer to multiplicand 2
 * @param BimmOff    integral offset... (does include the array header??)
 * @param vecImm4    can be NULL, passed by reference, and to be reused again in repeated calls. caller must call cg->stopUsingRegister
 * @param vecShift   can be NULL, passed by reference, and to be reused again in repeated calls. caller must call cg->stopUsingRegister
 */
void
inlineSIMD128Multiply(TR::Node * node, TR::CodeGenerator * cg,
      TR::Register * lowC, TR::Register * highC, TR::Register * A, TR::Register * B,
      TR::Register *& vecImm4, TR::Register *& vecShift)
   {
   TR::Instruction *cursor;

   /**
    *  VLEIB  V6,0x20,7 "4" in bits 1-4 of element 7, Will shift by 4 with VSRLB
    *  VLEIB  0x0c0d0e0f101112131415161718191a1b Lit. pool? Double register shift right
    */
   if (NULL == vecImm4)
      {
      vecImm4 = cg->allocateRegister(TR_VRF);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, vecImm4, 0x20, 7);
      }

   if (NULL == vecShift)
      {
      vecShift = cg->allocateRegister(TR_VRF);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x1a1b, 7);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x1819, 6);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x1617, 5);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x1415, 4);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x1213, 3);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x1011, 2);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x0e0f, 1);
      generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vecShift, 0x0c0d, 0);
      }

   /**
    *  VREPF  V5,B,3
    *  VREPF  V4,B,2
    *  VREPF  V3,B,1
    *  VREPF  Vtemp1,B,0
    */

   TR::Register * V5 = cg->allocateRegister(TR_VRF);
   TR::Register * V4 = cg->allocateRegister(TR_VRF);
   TR::Register * V3 = cg->allocateRegister(TR_VRF);
   TR::Register * Vtemp1 = cg->allocateRegister(TR_VRF);

   cursor =
   generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, V5, B, 3, 2);
   generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, V4, B, 2, 2);
   generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, V3, B, 1, 2);
   generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, Vtemp1, B, 0, 2);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# Load inputs");

   /**
    *  # First row
    *  VMLF   V9,A,V5
    *  VMLHF  highC,A,V5
    */

   TR::Register *V9 = cg->allocateRegister(TR_VRF);
   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VML, node, V9, A, V5, 2);
   generateVRRcInstruction(cg, TR::InstOpCode::VMLH, node, highC, A, V5, 2);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# First row");

   /**
    *  # Second row + first row high
    *  VMALF  V10,A,V4,highC
    *  VMALHF highC,A,V4,highC
    */

   TR::Register *V10 = cg->allocateRegister(TR_VRF);
   cursor = generateVRRdInstruction(cg, TR::InstOpCode::VMAL, node, V10, A, V4, highC, 0, 2);
   generateVRRdInstruction(cg, TR::InstOpCode::VMALH, node, highC, A, V4, highC, 0, 2);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# Second row + first row high");

   /**
    *  # Third row + second row high
    *  VMALF  V11,A,V3,highC
    *  VMALHF highC,A,V3,highC
    */

   TR::Register *V11 = cg->allocateRegister(TR_VRF);
   cursor = generateVRRdInstruction(cg, TR::InstOpCode::VMAL, node, V11, A, V3, highC, 0, 2);
   generateVRRdInstruction(cg, TR::InstOpCode::VMALH, node, highC, A, V3, highC, 0, 2);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# Third row + second row high");

   /**
    *  # Forth row + third row high
    *  VMALF  V12,A,Vtemp1,highC
    *  VMALHF highC,A,Vtemp1,highC
    */

   TR::Register *V12 = cg->allocateRegister(TR_VRF);
   cursor = generateVRRdInstruction(cg, TR::InstOpCode::VMAL, node, V12, A, Vtemp1, highC, 0, 2);
   generateVRRdInstruction(cg, TR::InstOpCode::VMALH, node, highC, A, Vtemp1, highC, 0, 2);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# Forth row + third row high");

   /**
    *  # least significant digit
    *  VPERM  lowC,V9,lowC,vecShift
    *  VSRLB  V9,V9,V6
    */

   cursor = generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, lowC, V9, lowC, vecShift);
   generateVRRcInstruction(cg, TR::InstOpCode::VSRLB, node, V9, V9, vecImm4, 2);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# least significant digit");

   /**
    *  # First row low + second row low; next digit
    *  VAQ    Vtemp1,V10,V9
    *  VACCQ  V3,V10,V9
    *  VPERM  lowC,Vtemp1,lowC,vecShift
    *  VPERM  V9,V3,Vtemp1,vecShift
    */

   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VA, node, Vtemp1, V10, V9, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, V3, V10, V9, 4);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, lowC, Vtemp1, lowC, vecShift);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, V9, V3, Vtemp1, vecShift);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# First row low + second row low; next digit");

   /**
    *  # Accumulator + third row low; next digit
    *  VAQ    Vtemp1,V11,V9
    *  VACCQ  V3,V11,V9
    *  VPERM  lowC,Vtemp1,lowC,vecShift
    *  VPERM  V9,V3,Vtemp1,vecShift
    */

   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VA, node, Vtemp1, V11, V9, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, V3, V11, V9, 4);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, lowC, Vtemp1, lowC, vecShift);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, V9, V3, Vtemp1, vecShift);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# Accumulator + third row low; next digit");

   /**
    *  # Accumulator + forth row low; next digit
    *  VAQ    Vtemp1,V12,V9
    *  VACCQ  V3,V12,V9
    *  VPERM  lowC,Vtemp1,lowC,vecShift
    *  VPERM  V9,V3,Vtemp1,vecShift
    */

   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VA, node, Vtemp1, V12, V9, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, V3, V12, V9, 4);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, lowC, Vtemp1, lowC, vecShift);
   generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, V9, V3, Vtemp1, vecShift);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# Accumulator + forth row low; next digit");

   /**
    *  # Accumulator + forth row high; high quad
    *  VAQ    highC,highC,V9
    *  VST    highC, 0(c, c_off)
    */

   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VA, node, highC, highC, V9, 4);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "# Accumulator + forth row high; high quad");

   cg->stopUsingRegister(Vtemp1);
   cg->stopUsingRegister(V3);
   cg->stopUsingRegister(V4);
   cg->stopUsingRegister(V5);
   cg->stopUsingRegister(V9);
   cg->stopUsingRegister(V10);
   cg->stopUsingRegister(V11);
   cg->stopUsingRegister(V12);
   }

TR::Register *
inline256Multiply(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction *cursor;
   cg->decReferenceCount(node->getFirstChild());

   TR::Register * Carr = cg->evaluate(node->getChild(1));
   TR::Register * Aarr = cg->evaluate(node->getChild(2));
   TR::Register * Barr = cg->evaluate(node->getChild(3));

   // There are alot more registers here then really needed, but since this was intentionally written
   // not to have internal control flow, RA can do its job properly, so mostly try not to reuse registers

   TR::Register *A0 = cg->allocateRegister();
   TR::Register *A1 = cg->allocateRegister();
   TR::Register *A2 = cg->allocateRegister();
   TR::Register *A3 = cg->allocateRegister();
   TR::Register *AA0 = cg->allocateRegister();
   TR::Register *AA1 = cg->allocateRegister();
   TR::Register *AA2 = cg->allocateRegister();
   TR::Register *AA3 = cg->allocateRegister();
   TR::Register *B0 = cg->allocateRegister();
   TR::Register *B1 = cg->allocateRegister();
   TR::Register *B2 = cg->allocateRegister();
   TR::Register *B3 = cg->allocateRegister();
   TR::Register *Z00 = cg->allocateRegister();
   TR::Register *Z01 = cg->allocateRegister();
   TR::Register *Z02 = cg->allocateRegister();
   TR::Register *Z03 = cg->allocateRegister();
   TR::Register *rZ10 = cg->allocateRegister();  // unique name to avoid const conflict in J9 VM
   TR::Register *Z11 = cg->allocateRegister();
   TR::Register *Z12 = cg->allocateRegister();
   TR::Register *Z13 = cg->allocateRegister();
   TR::Register *Z1p0 = cg->allocateRegister();
   TR::Register *Z1p1 = cg->allocateRegister();
   TR::Register *Z1p2 = cg->allocateRegister();
   TR::Register *Z1p3 = cg->allocateRegister();
   TR::Register *Z20 = cg->allocateRegister();
   TR::Register *Z21 = cg->allocateRegister();
   TR::Register *Z22 = cg->allocateRegister();
   TR::Register *Z23 = cg->allocateRegister();

   TR::Register *carry = cg->allocateRegister();

   TR::MemoryReference * Aref56  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+56, cg);
   TR::MemoryReference * Aref48  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+48, cg);
   TR::MemoryReference * Aref40  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+40, cg);
   TR::MemoryReference * Aref32  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);
   TR::MemoryReference * AAref56 = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+56, cg);
   TR::MemoryReference * AAref48 = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+48, cg);
   TR::MemoryReference * AAref40 = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+40, cg);
   TR::MemoryReference * AAref32 = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);
   TR::MemoryReference * Bref56  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+56, cg);
   TR::MemoryReference * Bref48  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+48, cg);
   TR::MemoryReference * Bref40  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+40, cg);
   TR::MemoryReference * Bref32  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);

   TR::MemoryReference * Cref56  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+56, cg);
   TR::MemoryReference * Cref48  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+48, cg);
   TR::MemoryReference * Cref40  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+40, cg);
   TR::MemoryReference * Cref32  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);
   TR::MemoryReference * Cref24  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+24, cg);
   TR::MemoryReference * Cref16  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);
   TR::MemoryReference * Cref8   = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+8, cg);
   TR::MemoryReference * Cref0   = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+0, cg);

   /**
    * Method parameters:
    *
    *  A:
    *    +--------------+-------------------------------+---------------+---------------+
    *    | Arr. Header  |    xxxx       |     xxxx      |  A3   |  A2   |  A1   |  A0   |
    *    +--------------+-------------------------------+---------------+---------------+
    *                   0              16             32               48
    *  B:
    *    +--------------+-------------------------------+---------------+---------------+
    *    | Arr. Header  |    xxxx       |     xxxx      |  B3   |  B2   |  B1   |  B0   |
    *    +--------------+-------------------------------+---------------+---------------+
    *                   0              16              32              48
    *  C:
    *    +--------------+-------+-------+-------+-------+-------+-------+-------+-------+
    *    | Arr. Header  |  C7   |  C6   |  C5   |  C4   |  C3   |  C2   |  C1   |  C0   |
    *    +--------------+-------+-------+-------+-------+-------+-------+-------+-------+
    *                   0              16              32              48
    *
    * [Z03,Z02,Z01,Z00] = [A1,A0] * [B1,B0]    // full 128-bit multiply
    * [Z23,Z22,Z21,Z20] = [A3,A2] * [B3,B2]    // full 128-bit multiply
    *
    * [Z13,Z12,Z11,Z10] = [A1,A0] * [B3,B2] + [A3,A2] * [B1,B0] // two full 128-bit multiply
    *
    * Then interpolate:
    *  +---------------+---------------+
    *  |  Z23  |  Z22  |  Z21  |  Z20  |
    *  +---------------+---------------+
    *                  +---------------+---------------+
    *                C<|  Z13  |  Z12  |  Z11  |  Z10  |
    *                  +---------------+---------------+
    *                                  +---------------+---------------+
    *                                  |  Z03  |  Z02  |  Z01  |  Z00  |
    *                                  +---------------+---------------+
    *
    * Breaking down above formula for [highZ1,lowZ1]:
    *
    *    [Z13,Z12,Z11,Z10]     = [A1,A0] * [B3,B2]    // full 128-bit multiply
    *    [Z1p3,Z1p2,Z1p1,Z1p0] = [A3,A2] * [B1,B0]    // full 128-bit multiply
    *
    *    [Sum_high,highZ1,lowZ1] = [highZ1',lowZ1'] + [highZ1part,lowZ1part]
    *
    * Full break down with alignment:
    *
    *  +--------------------------------------------+    \
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |     |             +-----------------------------+
    *  +--------------------------------------------+      \   Sum_high<|    highZ1    |     lowZ1    |
    *                 +-----------------------------+      /            +-----------------------------+
    *                 |    highZ2    |     lowZ2    |     |
    *                 +-----------------------------+    /
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *
    */

   generateRXInstruction(cg, TR::InstOpCode::LG, node, A0, Aref56);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, A1, Aref48);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, B2, Bref40);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, B3, Bref32);

   // Multiply [Z13,Z12,Z11,Z10]     = [A1,A0] * [B3,B2]
   inline128Multiply(node, cg,
         rZ10, Z11, Z12, Z13,
         A0, A1, B2, B3);

   generateRXInstruction(cg, TR::InstOpCode::LG, node, A2, Aref40);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, A3, Aref32);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, B0, Bref56);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, B1, Bref48);

   // Multiply [Z1p3,Z1p2,Z1p1,Z1p0] = [A3,A2] * [B1,B0]
   inline128Multiply(node, cg,
         Z1p0, Z1p1, Z1p2, Z1p3,
         A2, A3, B0, B1);

   /**
    * Finish calculating Z1:
    *  +-----+---------------+---------------+
    *  |carry|  Z13  |  Z12  |  Z11  |  Z10  |
    *  +-----+---------------+---------------+
    *        +---------------+---------------+
    *        | Z1p3  | Z1p2  | Z1p1  | Z1p0  |
    *        +---------------+---------------+
    */

   cursor =
   generateRRInstruction(cg,  TR::InstOpCode::ALGR, node, rZ10, Z1p0);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z11, Z1p1);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z12, Z1p2);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z13, Z1p3);
   generateRIInstruction(cg, TR::InstOpCode::LGHI, node, carry, 0);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, carry, carry);

   if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Finish calculating Z1");

   generateRXInstruction(cg, TR::InstOpCode::LG, node, AA0, AAref56);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, AA1, AAref48);

   // Multiply [Z03,Z02,Z01,Z00] = [A1,A0] * [B1,B0]
   inline128Multiply(node, cg,
         Z00, Z01, Z02, Z03,
         AA0, AA1, B0, B1);

   // Store out lowest two digits
   generateRXInstruction(cg, TR::InstOpCode::STG, node, Z00, Cref56);
   generateRXInstruction(cg, TR::InstOpCode::STG, node, Z01, Cref48);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, AA2, AAref40);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, AA3, AAref32);

   // Multiply [Z23,Z22,Z21,Z20] = [A3,A2] * [B3,B2]
   inline128Multiply(node, cg,
         Z20, Z21, Z22, Z23,
         AA2, AA3, B2, B3);

   /**
    * Final addition:
    *  +---------------+---------------+---------------+---------------+
    *  |  Z23  |  Z22  |  Z21  |  Z20  |  Z03  |  Z02  |  Z01  |  Z00  |
    *  +---------------+---------------+---------------+---------------+
    *            +-----+---------------+---------------+
    *            |carry|  Z13  |  Z12  |  Z11  |  Z10  |
    *            +-----+---------------+---------------+
    *
    *  Equals to:
    *  +---------------+---------------+---------------+---------------+
    *  |  Z23  |  Z22  |  Z21  |  Z20  |  Z03  |  Z02  |  Z01  |  Z00  |
    *  +---------------+---------------+---------------+---------------+
    *
    */

   cursor =
   generateRRInstruction(cg,  TR::InstOpCode::ALGR,  node, Z02, rZ10);
   generateRXInstruction(cg,  TR::InstOpCode::STG,   node, Z02, Cref40);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z03, Z11);
   generateRXInstruction(cg,  TR::InstOpCode::STG,   node, Z03, Cref32);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z20, Z12);
   generateRXInstruction(cg,  TR::InstOpCode::STG,   node, Z20, Cref24);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z21, Z13);
   generateRXInstruction(cg,  TR::InstOpCode::STG,   node, Z21, Cref16);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z22, carry);
   generateRXInstruction(cg,  TR::InstOpCode::STG,   node, Z22, Cref8);
   generateRIInstruction(cg,  TR::InstOpCode::LGHI,  node, carry, 0);
   generateRREInstruction(cg, TR::InstOpCode::ALCGR, node, Z23, carry);
   generateRXInstruction(cg,  TR::InstOpCode::STG,   node, Z23, Cref0);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "Final addition");


   cg->stopUsingRegister(A0);
   cg->stopUsingRegister(A1);
   cg->stopUsingRegister(A2);
   cg->stopUsingRegister(A3);
   cg->stopUsingRegister(AA0);
   cg->stopUsingRegister(AA1);
   cg->stopUsingRegister(AA2);
   cg->stopUsingRegister(AA3);
   cg->stopUsingRegister(B0);
   cg->stopUsingRegister(B1);
   cg->stopUsingRegister(B2);
   cg->stopUsingRegister(B3);
   cg->stopUsingRegister(Z00);
   cg->stopUsingRegister(Z01);
   cg->stopUsingRegister(Z02);
   cg->stopUsingRegister(Z03);
   cg->stopUsingRegister(rZ10);
   cg->stopUsingRegister(Z11);
   cg->stopUsingRegister(Z12);
   cg->stopUsingRegister(Z13);
   cg->stopUsingRegister(Z1p0);
   cg->stopUsingRegister(Z1p1);
   cg->stopUsingRegister(Z1p2);
   cg->stopUsingRegister(Z1p3);
   cg->stopUsingRegister(Z20);
   cg->stopUsingRegister(Z21);
   cg->stopUsingRegister(Z22);
   cg->stopUsingRegister(Z23);

   cg->stopUsingRegister(carry);

   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   return NULL;
   }

TR::Register *
inlineSIMDP256KaratsubaMultiply
(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction *cursor;
   cg->decReferenceCount(node->getFirstChild());

   TR::Register * Carr = cg->evaluate(node->getChild(1));
   TR::Register * Aarr = cg->evaluate(node->getChild(2));
   TR::Register * Barr = cg->evaluate(node->getChild(3));

   // There are alot more registers here then really needed, but since this was intentionally written
   // not to have internal control flow, RA can do its job properly, so mostly try not to reuse registers
   TR::Register *lowZ0 = cg->allocateRegister(TR_VRF);
   TR::Register *highZ0 = cg->allocateRegister(TR_VRF);
   TR::Register *lowZ1 = cg->allocateRegister(TR_VRF);
   TR::Register *highZ1 = cg->allocateRegister(TR_VRF);
   TR::Register *lowZ2 = cg->allocateRegister(TR_VRF);
   TR::Register *highZ2 = cg->allocateRegister(TR_VRF);
   TR::Register *A0 = cg->allocateRegister(TR_VRF);
   TR::Register *A1 = cg->allocateRegister(TR_VRF);
   TR::Register *B0 = cg->allocateRegister(TR_VRF);
   TR::Register *B1 = cg->allocateRegister(TR_VRF);
   TR::Register *Asum_low = cg->allocateRegister(TR_VRF);
   TR::Register *Asum_high = cg->allocateRegister(TR_VRF);
   TR::Register *Bsum_low = cg->allocateRegister(TR_VRF);
   TR::Register *Bsum_high = cg->allocateRegister(TR_VRF);
   TR::Register *Sum_high = cg->allocateRegister(TR_VRF);
   TR::Register *zero = cg->allocateRegister(TR_VRF);
   TR::Register *carry1 = cg->allocateRegister(TR_VRF);
   TR::Register *carry2 = cg->allocateRegister(TR_VRF);

   TR::Register *vecImm4 = NULL;
   TR::Register *vecShift = NULL;

   TR::MemoryReference * Aref48  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16+32, cg);
   TR::MemoryReference * Aref32   = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);
   TR::MemoryReference * Bref48  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16+32, cg);
   TR::MemoryReference * Bref32   = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);

   /**
    * Method parameters:
    *
    *  A:
    *    +--------------+-----------------------------+-----------------------------+
    *    | Arr. Header  |    xxxx      |     xxxx     |      A1      |      A0      |
    *    +--------------+-----------------------------+-----------------------------+
    *                   0             16             32             48
    *  B:
    *    +--------------+-----------------------------+-----------------------------+
    *    | Arr. Header  |    xxxx      |     xxxx     |      B1      |      B0      |
    *    +--------------+-----------------------------+-----------------------------+
    *                   0             16             32             48
    *  C:
    *    +--------------+-----------------------------+-----------------------------+
    *    | Arr. Header  |     C3       |      C2      |      C1      |      C0      |
    *    +--------------+-----------------------------+-----------------------------+
    *                   0             16             32             48
    */
   // Load inputs
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, A0, Aref48);
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, A1, Aref32);
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, B0, Bref48);
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, B1, Bref32);

   /**
    * KARATSUBA Algorithm
    *
    * [Asum_high,Asum_low] = A0 + A1   // Asum_high is a boolean carry, 1-bit
    * [Bsum_high,Bsum_low] = B0 + B1   // Bsum_high is a boolean carry, 1-bit
    *
    * [highZ0,lowZ0] = A0 * B0    // full 128-bit multiply
    * [highZ2,lowZ2] = A1 * B1    // full 128-bit multiply
    *
    * [highZ1,lowZ1] = (A0+A1)*(B0+B1) - [highZ0,lowZ0] - [highZ2,lowZ2]
    *                = [Asum_high,Asum_low]*[Bsum_high,Bsum_low] - [highZ0,lowZ0] - [highZ2,lowZ2]
    *                = Asum_high*Bsum_high    // 1 bit with correct shift (2 vector shift)
    *                  + Asum_high*Bsum_low   // Conditional add  (1 vector shift)
    *                  + Bsum_high*Asum_low   // Conditional add  (1 vector shift)
    *                  + Asum_low*Bsum_low    // full 128-bit multiply
    *                  - [highZ0,lowZ0] - [highZ2,lowZ2]
    *
    * Then interpolate:
    *  +-----------------------------+
    *  |    highZ2    |     lowZ2    |
    *  +-----------------------------+
    *                 +-----------------------------+
    *               C<|    highZ1    |     lowZ1    |
    *                 +-----------------------------+
    *                                +-----------------------------+
    *                                |    highZ0    |     lowZ0    |
    *                                +-----------------------------+
    *
    * Breaking down above formula for [highZ1,lowZ1]:
    *
    *    [highZ1',lowZ1'] = Asum_low*Bsum_low    // full 128-bit multiply
    *    Sum_high = Asum_high*Bsum_high         // 1 bit with correct shift
    *             = Asum_high & Bsum_high
    *    Asum_high' = Asum_high*Bsum_low
    *               = (zero - Asum_high) & Bsum_low  // Bitwise instead of branch
    *    Bsum_high' = Bsum_high*Asum_low
    *               = (zero - Bsum_high) & Asum_low  // Bitwise instead of branch
    *
    * Full break down with alignment:
    *
    *  +--------------------------------------------+    \
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |     \
    *  +--------------------------------------------+      |
    *                 +--------------+                     |
    *                 |   Asum_high' |                     |
    *                 +--------------+                     |
    *                 +--------------+                     |      +-----------------------------+
    *                 |   Bsum_high' |                      \   C<|    highZ1    |     lowZ1    |
    *                 +--------------+                      /     +-----------------------------+
    *                    -------- (Subtract)               |
    *                 +-----------------------------+      |
    *                 |    highZ0    |     lowZ0    |      |
    *                 +-----------------------------+      |
    *                    -------- (Subtract)               |
    *                 +-----------------------------+      |
    *                 |    highZ2    |     lowZ2    |     /
    *                 +-----------------------------+    /
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *
    */
   // Compute middle terms, 129 bits each
   generateVRRcInstruction(cg, TR::InstOpCode::VA,   node, Asum_low,  A0, A1, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, Asum_high, A0, A1, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VA,   node, Bsum_low,  B0, B1, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, Bsum_high, B0, B1, 4);
   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VN,   node, Sum_high,  Bsum_high, Asum_high, 0);

   inlineSIMD128Multiply(node, cg,
         lowZ0, highZ0, A0, B0,
         vecImm4, vecShift);

   inlineSIMD128Multiply(node, cg,
         lowZ1, highZ1, Asum_low, Bsum_low,
         vecImm4, vecShift);

   inlineSIMD128Multiply(node, cg,
         lowZ2, highZ2, A1, B1,
         vecImm4, vecShift);

   // Create masks from the carry bit of the middle term, *sum_low is to be added if carry was true
   cursor = generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, zero, 0, 0);

   generateVRRcInstruction(cg, TR::InstOpCode::VS, node, Asum_high, zero, Asum_high, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VS, node, Bsum_high, zero, Bsum_high, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VN, node, Asum_high, Asum_high, Bsum_low, 0);
   generateVRRcInstruction(cg, TR::InstOpCode::VN, node, Bsum_high, Bsum_high, Asum_low, 0);

   /**
    * Finish calculating Z1:
    *  +--------------------------------------------+    \
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |     \
    *  +--------------------------------------------+      |
    *                 +--------------+                     |
    *                 |   Asum_high' |                     |
    *                 +--------------+                     |
    *                 +--------------+                     |             +-----------------------------+
    *                 |   Bsum_high' |                      \   Sum_high<|    highZ1    |     lowZ1    |
    *                 +--------------+                      /            +-----------------------------+
    *                    -------- (Subtract)               |
    *                 +-----------------------------+      |
    *                 |    highZ0    |     lowZ0    |      |
    *                 +-----------------------------+      |
    *                    -------- (Subtract)               |
    *                 +-----------------------------+      |
    *                 |    highZ2    |     lowZ2    |     /
    *                 +-----------------------------+    /
    */
   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, carry1,  highZ1, Asum_high, 4);

   generateVRRcInstruction(cg, TR::InstOpCode::VA,   node, highZ1,  highZ1, Asum_high, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VA,   node, Sum_high,  Sum_high, carry1, 4);

   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, carry1,  highZ1, Bsum_high, 4);

   generateVRRcInstruction(cg, TR::InstOpCode::VA,   node, highZ1,  highZ1, Bsum_high, 4);
   generateVRRcInstruction(cg, TR::InstOpCode::VA,   node, Sum_high,  Sum_high, carry1, 4);

   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VSCBI,  node, carry1,   lowZ1,  lowZ0,  4);

   generateVRRcInstruction(cg, TR::InstOpCode::VS,     node, lowZ1,    lowZ1,  lowZ0,  4);
   generateVRRdInstruction(cg, TR::InstOpCode::VSBCBI, node, carry2,   highZ1, highZ0,  carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VSBI,   node, highZ1,   highZ1, highZ0,  carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VSBI,   node, Sum_high, Sum_high, zero,  carry2, 0, 4);

   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VSCBI,  node, carry1,  lowZ1,  lowZ2,  4);

   generateVRRcInstruction(cg, TR::InstOpCode::VS,     node, lowZ1,   lowZ1,  lowZ2,  4);
   generateVRRdInstruction(cg, TR::InstOpCode::VSBCBI, node, carry2,  highZ1, highZ2, carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VSBI,   node, highZ1,  highZ1, highZ2, carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VSBI,   node, Sum_high, Sum_high, zero, carry2, 0, 4);

   /**
    * Final addition:
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *  +--------------------------------------------+
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |
    *  +--------------------------------------------+
    *
    *  Equals to:
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *
    */
   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VACC,  node, carry1,  highZ0, lowZ1, 4);

   generateVRRcInstruction(cg, TR::InstOpCode::VA,    node, highZ0,  highZ0, lowZ1, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VACCC, node, carry2,  lowZ2,  highZ1,   carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VAC,   node, lowZ2,   lowZ2,  highZ1,   carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VAC,   node, highZ2,  highZ2, Sum_high, carry2, 0, 4);

   TR::MemoryReference * Cref48 = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+48, cg);
   TR::MemoryReference * Cref32 = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);
   TR::MemoryReference * Cref16 = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);
   TR::MemoryReference * Cref0  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+0, cg);

   // Store out
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, highZ2, Cref0, 3);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, lowZ2, Cref16, 3);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, highZ0, Cref32, 3);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, lowZ0, Cref48, 3);

   cg->stopUsingRegister(lowZ0);
   cg->stopUsingRegister(highZ0);
   cg->stopUsingRegister(lowZ1);
   cg->stopUsingRegister(highZ1);
   cg->stopUsingRegister(lowZ2);
   cg->stopUsingRegister(highZ2);
   cg->stopUsingRegister(A0);
   cg->stopUsingRegister(A1);
   cg->stopUsingRegister(B0);
   cg->stopUsingRegister(B1);
   cg->stopUsingRegister(Asum_low);
   cg->stopUsingRegister(Asum_high);
   cg->stopUsingRegister(Bsum_low);
   cg->stopUsingRegister(Bsum_high);
   cg->stopUsingRegister(Sum_high);
   cg->stopUsingRegister(zero);
   cg->stopUsingRegister(carry1);
   cg->stopUsingRegister(carry2);
   cg->stopUsingRegister(vecImm4);
   cg->stopUsingRegister(vecShift);

   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   return NULL;
   }

/**
 * Simpler version of TR::Register *inlineSIMDP256KaratsubaMultiply
 */
TR::Register *
inlineSIMDP256Multiply
(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction *cursor;
   cg->decReferenceCount(node->getFirstChild());

   TR::Register * Carr = cg->evaluate(node->getChild(1));
   TR::Register * Aarr = cg->evaluate(node->getChild(2));
   TR::Register * Barr = cg->evaluate(node->getChild(3));

   // There are alot more registers here then really needed, but since this was intentionally written
   // not to have internal control flow, RA can do its job properly, so mostly try not to reuse registers
   TR::Register *lowZ0 = cg->allocateRegister(TR_VRF);
   TR::Register *highZ0 = cg->allocateRegister(TR_VRF);
   TR::Register *lowZ1 = cg->allocateRegister(TR_VRF);
   TR::Register *highZ1 = cg->allocateRegister(TR_VRF);
   TR::Register *lowZ2 = cg->allocateRegister(TR_VRF);
   TR::Register *highZ2 = cg->allocateRegister(TR_VRF);
   TR::Register *lowZ1part = cg->allocateRegister(TR_VRF);
   TR::Register *highZ1part = cg->allocateRegister(TR_VRF);
   TR::Register *A0 = cg->allocateRegister(TR_VRF);
   TR::Register *A1 = cg->allocateRegister(TR_VRF);
   TR::Register *B0 = cg->allocateRegister(TR_VRF);
   TR::Register *B1 = cg->allocateRegister(TR_VRF);
   TR::Register *Sum_high = cg->allocateRegister(TR_VRF);
   TR::Register *carry1 = cg->allocateRegister(TR_VRF);
   TR::Register *carry2 = cg->allocateRegister(TR_VRF);

   TR::Register *vecImm4 = NULL;
   TR::Register *vecShift = NULL;

   TR::MemoryReference * Aref48  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16+32, cg);
   TR::MemoryReference * Aref32  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);
   TR::MemoryReference * Bref48  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16+32, cg);
   TR::MemoryReference * Bref32  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);

   /**
    * Method parameters:
    *
    *  A:
    *    +--------------+-----------------------------+-----------------------------+
    *    | Arr. Header  |    xxxx      |     xxxx     |      A1      |      A0      |
    *    +--------------+-----------------------------+-----------------------------+
    *                   0             16             32             48
    *  B:
    *    +--------------+-----------------------------+-----------------------------+
    *    | Arr. Header  |    xxxx      |     xxxx     |      B1      |      B0      |
    *    +--------------+-----------------------------+-----------------------------+
    *                   0             16             32             48
    *  C:
    *    +--------------+-----------------------------+-----------------------------+
    *    | Arr. Header  |     C3       |      C2      |      C1      |      C0      |
    *    +--------------+-----------------------------+-----------------------------+
    *                   0             16             32             48
    */
   // Load inputs
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, A0, Aref48);
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, A1, Aref32);
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, B0, Bref48);
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, B1, Bref32);

   /**
    *
    * [Asum_high,Asum_low] = A0 + A1   // Asum_high is a boolean carry, 1-bit
    * [Bsum_high,Bsum_low] = B0 + B1   // Bsum_high is a boolean carry, 1-bit
    *
    * [highZ0,lowZ0] = A0 * B0    // full 128-bit multiply
    * [highZ2,lowZ2] = A1 * B1    // full 128-bit multiply
    *
    * [highZ1,lowZ1] = A0 * B1 + A1 * B0 // two full 128-bit multiply
    *
    * Then interpolate:
    *  +-----------------------------+
    *  |    highZ2    |     lowZ2    |
    *  +-----------------------------+
    *                 +-----------------------------+
    *               C<|    highZ1    |     lowZ1    |
    *                 +-----------------------------+
    *                                +-----------------------------+
    *                                |    highZ0    |     lowZ0    |
    *                                +-----------------------------+
    *
    * Breaking down above formula for [highZ1,lowZ1]:
    *
    *    [highZ1',lowZ1']       = A0 * B1    // full 128-bit multiply
    *    [highZ1part,lowZ1part] = A1 * B0    // full 128-bit multiply
    *
    *    [Sum_high,highZ1,lowZ1] = [highZ1',lowZ1'] + [highZ1part,lowZ1part]
    *
    * Full break down with alignment:
    *
    *  +--------------------------------------------+    \
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |     |             +-----------------------------+
    *  +--------------------------------------------+      \   Sum_high<|    highZ1    |     lowZ1    |
    *                 +-----------------------------+      /            +-----------------------------+
    *                 |    highZ2    |     lowZ2    |     |
    *                 +-----------------------------+    /
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *
    */

   inlineSIMD128Multiply(node, cg,
         lowZ0, highZ0, A0, B0,
         vecImm4, vecShift);

   inlineSIMD128Multiply(node, cg,
         lowZ1, highZ1, A0, B1,
         vecImm4, vecShift);

   inlineSIMD128Multiply(node, cg,
         lowZ1part, highZ1part, A1, B0,
         vecImm4, vecShift);

   inlineSIMD128Multiply(node, cg,
         lowZ2, highZ2, A1, B1,
         vecImm4, vecShift);

   /**
    * Finish calculating Z1:
    *  +--------------------------------------------+
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |
    *  +--------------------------------------------+
    *                 +-----------------------------+
    *                 |  highZ1part  | lowZ1part    |
    *                 +-----------------------------+
    */
   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VACC, node, carry1,  lowZ1, lowZ1part, 4);

   generateVRRcInstruction(cg, TR::InstOpCode::VA,    node, lowZ1,    lowZ1,  lowZ1part, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VACCC, node, Sum_high, highZ1, highZ1part,   carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VAC,   node, highZ1,   highZ1, highZ1part,   carry1, 0, 4);

   /**
    * Final addition:
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *  +--------------------------------------------+
    *  |   Sum_high   |    highZ1'   |     lowZ1'   |
    *  +--------------------------------------------+
    *
    *  Equals to:
    *  +-----------------------------+-----------------------------+
    *  |    highZ2    |     lowZ2    |    highZ0    |     lowZ0    |
    *  +-----------------------------+-----------------------------+
    *
    */
   cursor = generateVRRcInstruction(cg, TR::InstOpCode::VACC,  node, carry1,  highZ0, lowZ1, 4);

   generateVRRcInstruction(cg, TR::InstOpCode::VA,    node, highZ0,  highZ0, lowZ1, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VACCC, node, carry2,  lowZ2,  highZ1,   carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VAC,   node, lowZ2,   lowZ2,  highZ1,   carry1, 0, 4);
   generateVRRdInstruction(cg, TR::InstOpCode::VAC,   node, highZ2,  highZ2, Sum_high, carry2, 0, 4);

   TR::MemoryReference * Cref48 = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+48, cg);
   TR::MemoryReference * Cref32 = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+32, cg);
   TR::MemoryReference * Cref16 = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);
   TR::MemoryReference * Cref0  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+0, cg);

   // Store out
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, highZ2, Cref0, 3);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, lowZ2, Cref16, 3);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, highZ0, Cref32, 3);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, lowZ0, Cref48, 3);

   cg->stopUsingRegister(lowZ0);
   cg->stopUsingRegister(highZ0);
   cg->stopUsingRegister(lowZ1);
   cg->stopUsingRegister(highZ1);
   cg->stopUsingRegister(lowZ2);
   cg->stopUsingRegister(highZ2);
   cg->stopUsingRegister(lowZ1part);
   cg->stopUsingRegister(highZ1part);
   cg->stopUsingRegister(A0);
   cg->stopUsingRegister(A1);
   cg->stopUsingRegister(B0);
   cg->stopUsingRegister(B1);
   cg->stopUsingRegister(Sum_high);
   cg->stopUsingRegister(carry1);
   cg->stopUsingRegister(carry2);

   cg->stopUsingRegister(vecImm4);
   cg->stopUsingRegister(vecShift);

   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   return NULL;
   }
/**
 * Multiplies two 256b integer using 'VECTOR MULTIPLY SUM LOGICAL' instruction
 */
TR::Register * inlineVMSL256Multiply(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction *cursor;
   cg->decReferenceCount(node->getFirstChild());

   TR::Register * Carr = cg->evaluate(node->getChild(1));
   TR::Register * Aarr = cg->evaluate(node->getChild(2));
   TR::Register * Barr = cg->evaluate(node->getChild(3));

   //Allocating Registers
   TR::Register *A[5];
   TR::Register *C[9];
   TR::Register *B         = cg->allocateRegister(TR_VRF);
   TR::Register *VPERMA    = cg->allocateRegister(TR_VRF);
   TR::Register *VPERMB    = cg->allocateRegister(TR_VRF);
   TR::Register *vZero     = cg->allocateRegister(TR_VRF);
   TR::Register *rSeven    = cg->allocateRegister();
   TR::Register *rFourteen = cg->allocateRegister();
   TR::Register *rResidue  = cg->allocateRegister();
   TR::Register *VR;
   for (int i = 0 ; i < 5 ; i++)
      A[i] = cg->allocateRegister(TR_VRF);
   for (int i = 0 ; i < 9 ; i++)
      C[i] = cg->allocateRegister(TR_VRF);
   VR = C[0];

   // Initialization of vZero and VPERM constants
   int32_t VPERMAValue[4] = {0x0F000102, 0x03040506, 0x0F070809, 0x0A0B0C0D};
   int32_t VPERMBValue[4] = {0x0F070809, 0x0A0B0C0D, 0x0F000102, 0x03040506};
   int32_t VPERMAResidueValue[4] = {0x0F0F0F0F, 0x00010203, 0x0F040506, 0x0708090A};
   TR::MemoryReference* VPERMAMR = generateS390MemoryReference(cg->findOrCreateConstant(node, VPERMAValue, 16), cg, 0, node);
   TR::MemoryReference* VPERMBMR = generateS390MemoryReference(cg->findOrCreateConstant(node, VPERMBValue, 16), cg, 0, node);
   TR::MemoryReference* VPERMAResidueMR = generateS390MemoryReference(cg->findOrCreateConstant(node, VPERMAResidueValue, 16), cg, 0, node);
   generateVRIaInstruction (cg, TR::InstOpCode::VGBM, node, vZero, 0, 0);
   generateVRXInstruction  (cg, TR::InstOpCode::VL,   node, VPERMA, VPERMAMR);
   generateVRXInstruction  (cg, TR::InstOpCode::VL,   node, VPERMB, VPERMBMR);
   generateRIInstruction   (cg, TR::InstOpCode::LHI,  node, rSeven, 6);
   generateRIInstruction   (cg, TR::InstOpCode::LHI,  node, rFourteen, 13);
   generateRIInstruction   (cg, TR::InstOpCode::LHI,  node, rResidue, 10);
   // Load A to vector registers
   /**
    * Method parameters:
    *
    *  A:
    *    +------------+----------------------------------------------------------------+
    *    |Arr. Header |      xxxx      |      xxxx     | A4|  A3  |  A2  |  A1  |  A0  |
    *    +------------+----------------------------------------------------------------+
    *                 0               16              32   36    43     50     57
    *  B:
    *    +------------+----------------------------------------------------------------+
    *    |Arr. Header |      xxxx      |      xxxx     | B4|  B3  |  B2  |  B1  |  B0  |
    *    +------------+----------------------------------------------------------------+
    *                 0               16              32   36    43     50     57
    *  C:
    *    +------------+----------------------------------------------------------------+
    *    |Arr. Header |   C8   |  C7  |  C6  |  C5  |  C4  |  C3  |  C2  |  C1  |  C0  |
    *    +------------+----------------------------------------------------------------+
    *                 0        8     15     22     29      36    43     50     57
    */

   /*
    *  A[0] = {A1,A0}
    *  A[1] = {A2,A1}
    *  A[2] = {A3,A2}
    *  A[3] = {A4,A3}
    *  A[4] = {V0,A4}
    */
   int startIndex = TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, A[0], rFourteen, generateS390MemoryReference(Aarr, startIndex + 50, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, A[0], A[0], vZero, VPERMA);
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, A[1], rFourteen, generateS390MemoryReference(Aarr, startIndex + 43, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, A[1], A[1], vZero, VPERMA);
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, A[2], rFourteen, generateS390MemoryReference(Aarr, startIndex + 36, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, A[2], A[2], vZero, VPERMA);
   generateVRXInstruction  (cg, TR::InstOpCode::VL,   node, VPERMA, VPERMAResidueMR);
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, A[3], rResidue, generateS390MemoryReference(Aarr, startIndex + 32, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, A[3], A[3], vZero, VPERMA);
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, A[4], vZero, A[3], 8, 0);

   /*
    *               |    Ops in Step 1   |      Ops in Step2     | Ops in Step 3
    *------------------------------------------------------------------------
    * Step 1 | C0 = | A0 * B0            |                       |
    *        | C1 = | A1 * B0  +  A0 * B1|                       |
    *------------------------------------------------------------------------
    * Step 2 | C2 = | A2 * B0  +  A1 * B1| +  A0 * B2            |
    *        | C3 = | A3 * B0  +  A2 * B1| +  A1 * B2  +  A0 * B3|
    *------------------------------------------------------------------------
    * Step 3 | C4 = | A4 * B0  +  A3 * B1| +  A2 * B2  +  A1 * B3| +  A0 * B4
    *------------------------------------------------------------------------
    *        | C5 = |             A4 * B1| +  A3 * B2  +  A2 * B3| +  A1 * B4
    * Store  | C6 = |                    |    A4 * B2  +  A3 * B3| +  A2 * B4
    *        | C7 = |                    |                A4 * B3| +  A3 * B4
    *        | C8 = |                    |                       |    A4 * B4
    */

   // Step 1
   // B = {V0,B0}
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, B, rSeven, generateS390MemoryReference(Barr, startIndex + 57, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, B, B, vZero, VPERMB);
   generateVRRdInstruction (cg, TR::InstOpCode::VMSL, node, VR, A[0], B, vZero, 0, 3);
   generateVSIInstruction  (cg, TR::InstOpCode::VSTRL,node, VR, generateS390MemoryReference(Carr, startIndex + 57, cg), 6);
   // B = {B0,B1}
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, VR, vZero, VR, 9, 0);
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, B, rFourteen, generateS390MemoryReference(Barr, startIndex + 50, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, B, B, vZero, VPERMB);
   for (int i = 0 ; i < 5 ; i++)
      {
      generateVRRdInstruction(cg, TR::InstOpCode::VMSL, node, C[i+1], A[i], B, vZero, 0, 3);
      }
   generateVRRcInstruction (cg, TR::InstOpCode::VA,   node, VR, VR, C[1], 4);
   generateVSIInstruction  (cg, TR::InstOpCode::VSTRL,node, VR, generateS390MemoryReference(Carr, startIndex + 50, cg), 6);
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, VR, vZero, VR, 9, 0);

   // Step 2
   // B = {V0,B2}
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, B, rSeven, generateS390MemoryReference(Barr, startIndex + 43, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, B, B, vZero, VPERMB);
   generateVRRdInstruction (cg, TR::InstOpCode::VMSL, node, C[2], A[0], B, C[2], 0, 3);
   generateVRRcInstruction (cg, TR::InstOpCode::VA,   node, VR, VR, C[2], 4);
   generateVSIInstruction  (cg, TR::InstOpCode::VSTRL,node, VR, generateS390MemoryReference(Carr, startIndex + 43, cg), 6);
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, VR, vZero, VR, 9, 0);
   // B = {B2,B3}
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, B, rFourteen, generateS390MemoryReference(Barr, startIndex + 36, cg));
   generateVRReInstruction (cg, TR::InstOpCode::VPERM,node, B, B, vZero, VPERMB);

   for (int i = 0 ; i < 3 ; i++)
      {
      generateVRRdInstruction(cg, TR::InstOpCode::VMSL, node, C[i+3], A[i], B, C[i+3], 0, 3);
      }
   generateVRRdInstruction (cg, TR::InstOpCode::VMSL, node, C[6], A[3], B, vZero, 0, 3);
   generateVRRdInstruction (cg, TR::InstOpCode::VMSL, node, C[7], A[4], B, vZero, 0, 3);

   generateVRRcInstruction (cg, TR::InstOpCode::VA,   node, VR, VR, C[3], 4);
   generateVSIInstruction  (cg, TR::InstOpCode::VSTRL,node, VR, generateS390MemoryReference(Carr, startIndex + 36, cg), 6);
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, VR, vZero, VR, 9, 0);

   // Step 3
   // B = {V0,B4}
   generateVRSbInstruction (cg, TR::InstOpCode::VLL,  node, B, rResidue, generateS390MemoryReference(Barr, startIndex + 32, cg));
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, B, vZero, B, 4, 0);
   generateVRRdInstruction (cg, TR::InstOpCode::VMSL, node, C[4], A[0], B, C[4], 0, 3);
   generateVRRcInstruction (cg, TR::InstOpCode::VA,   node, VR, VR, C[4], 4);
   generateVSIInstruction  (cg, TR::InstOpCode::VSTRL,node, VR, generateS390MemoryReference(Carr, startIndex + 29, cg), 6);
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, VR, vZero, VR, 9, 0);
   // B = {B4,V0}
   generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, B, B, vZero, 8, 0);

   for (int i = 0 ; i < 3 ; i++)
      {
      generateVRRdInstruction(cg, TR::InstOpCode::VMSL, node, C[i+5], A[i], B, C[i+5], 0, 3);
      }
   generateVRRdInstruction(cg, TR::InstOpCode::VMSL, node, C[8], A[3], B, vZero, 0, 3);
   // Store Step
   for (int i = 5 ; i < 8 ; i++)
      {
      generateVRRcInstruction (cg, TR::InstOpCode::VA,   node, VR, VR, C[i], 4);
      generateVSIInstruction  (cg, TR::InstOpCode::VSTRL,node, VR, generateS390MemoryReference(Carr, startIndex + 64 - 7 - i*7, cg), 6);
      generateVRIdInstruction (cg, TR::InstOpCode::VSLDB,node, VR, vZero, VR, 9, 0);
      }
   generateVRRcInstruction (cg, TR::InstOpCode::VA,   node, VR, VR, C[8], 4);
   generateVSIInstruction  (cg, TR::InstOpCode::VSTRL,node, VR, generateS390MemoryReference(Carr, startIndex + 0, cg), 7);
   for (int i = 0 ; i < 5 ; i++)
      cg->stopUsingRegister(A[i]);
   for (int i = 0 ; i < 9 ; i++)
      cg->stopUsingRegister(C[i]);

   cg->stopUsingRegister(B);
   cg->stopUsingRegister(VPERMA);
   cg->stopUsingRegister(VPERMB);
   cg->stopUsingRegister(vZero);
   cg->stopUsingRegister(rSeven);
   cg->stopUsingRegister(rFourteen);
   cg->stopUsingRegister(rResidue);

   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   return NULL;
   }

TR::Register *
inlineP256Multiply(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   static const char * disableECCSIMD = feGetEnv("TR_disableECCSIMD");
   static const char * disableECCMLGR = feGetEnv("TR_disableECCMLGR");
   static const char * disableECCKarat = feGetEnv("TR_disableECCKarat");
   static const char * disableVMSL = feGetEnv("TR_disableVMSL");

   bool disableSIMDP256 = NULL != disableECCSIMD || comp->getOption(TR_Randomize) && cg->randomizer.randomBoolean();
   bool disableMLGRP256 = NULL != disableECCMLGR || comp->getOption(TR_Randomize) && cg->randomizer.randomBoolean();

   if (!disableVMSL && cg->comp()->target().cpu.getSupportsArch(TR::CPU::z14) && cg->getSupportsVectorRegisters())
      return inlineVMSL256Multiply(node, cg);
   if (disableECCKarat==NULL && cg->getSupportsVectorRegisters())
      return inlineSIMDP256Multiply(node, cg);
   if (!disableECCSIMD && cg->getSupportsVectorRegisters())
      return inlineSIMDP256Multiply(node, cg);
   else if (!disableMLGRP256) {
      return inline256Multiply(node, cg);
   }

//   private void multiply(int[] C, int[] A, int[] B)

   cg->decReferenceCount(node->getFirstChild());
   TR::Register * Carr = cg->evaluate(node->getChild(1));
   TR::Register * Aarr = cg->evaluate(node->getChild(2));
   TR::Register * Barr = cg->evaluate(node->getChild(3));

//        int R0 = 0;
//        int R1 = 0;


   TR::Register * zero = cg->allocateRegister();
   TR::Register * R0 = cg->allocateRegister();
   TR::Register * R1 = cg->allocateRegister();
   TR::Register * R2;

   generateRIInstruction(cg, TR::InstOpCode::LHI, node, R0, 0);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, R1, 0);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, zero, 0);

   //LSB Triangle
   for (int k = 15; k>=8; k--) {
//      R2 = 0;
      R2 = cg->allocateRegister();
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, R2, 0);

      for (int i = k, j = 15; i<16; j--, i++) {
//         long UL = (A[i] & 0xFFFFFFFFL)*(B[j] & 0xFFFFFFFFL);
//         int L = (int) UL;
//         int U = (int)(UL>>32);

            TR::Register * A = cg->allocateRegister();
            TR::Register * B = cg->allocateRegister();
            TR::Register * U = cg->allocateRegister();
            TR::Register * L = A; // Destructive MUL
            TR::RegisterPair * UL = cg->allocateConsecutiveRegisterPair(L, U);

            TR::MemoryReference * Aref = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+i*4, cg);
            TR::MemoryReference * Bref = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+j*4, cg);

            generateRXInstruction(cg, TR::InstOpCode::L, node, A, Aref);
            generateRXInstruction(cg, TR::InstOpCode::L, node, B, Bref);

            generateRRInstruction(cg, TR::InstOpCode::MLR, node, UL, B);
            cg->stopUsingRegister(B);

//         long carry;
//         carry = (R0 & 0xFFFFFFFFL) + (L & 0xFFFFFFFFL); R0 = (int) carry; carry>>=32;
            generateRRInstruction(cg, TR::InstOpCode::ALR, node, R0, L);

//         carry+= (R1 & 0xFFFFFFFFL) + (U & 0xFFFFFFFFL); R1 = (int) carry; carry>>=32;
            generateRREInstruction(cg, TR::InstOpCode::ALCR, node, R1, U);

//         R2 += carry;
            generateRREInstruction(cg, TR::InstOpCode::ALCR, node, R2, zero);

            cg->stopUsingRegister(A);
            cg->stopUsingRegister(U);
            cg->stopUsingRegister(UL);
      }

//      C[k] = R0;
      TR::MemoryReference * Cref = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+k*4, cg);
      TR::Instruction *cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, R0, Cref);

      if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Store %d least significant digit of final product", k);

      cg->stopUsingRegister(R0);
      R0 = R1;
      R1 = R2;
   }

   //MSB Triangle
   for (int k = 7; k>=0; k--) {
      //      R2 = 0;
      R2 = cg->allocateRegister();
      generateRIInstruction(cg, TR::InstOpCode::LHI, node, R2, 0);

      for (int i = 7, j = k+8; j>=8; j--, i++) {
         //                long UL = (A[i] & 0xFFFFFFFFL)*(B[j] & 0xFFFFFFFFL);
         //                int L = (int) UL;
         //                int U = (int)(UL>>32);

         TR::Register * A = cg->allocateRegister();
         TR::Register * B = cg->allocateRegister();
         TR::Register * U = cg->allocateRegister();
         TR::Register * L = A; // Destructive MUL
         TR::RegisterPair * UL = cg->allocateConsecutiveRegisterPair(L, U);

         TR::MemoryReference * Aref = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+i*4, cg);
         TR::MemoryReference * Bref = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+j*4, cg);

         generateRXInstruction(cg, TR::InstOpCode::L, node, A, Aref);
         generateRXInstruction(cg, TR::InstOpCode::L, node, B, Bref);

         generateRRInstruction(cg, TR::InstOpCode::MLR, node, UL, B);
         cg->stopUsingRegister(B);

         //         long carry;
         //         carry = (R0 & 0xFFFFFFFFL) + (L & 0xFFFFFFFFL); R0 = (int) carry; carry>>=32;
         generateRRInstruction(cg, TR::InstOpCode::ALR, node, R0, L);

         //         carry+= (R1 & 0xFFFFFFFFL) + (U & 0xFFFFFFFFL); R1 = (int) carry; carry>>=32;
         generateRREInstruction(cg, TR::InstOpCode::ALCR, node, R1, U);

         //         R2 += carry;
         generateRREInstruction(cg, TR::InstOpCode::ALCR, node, R2, zero);

         cg->stopUsingRegister(A);
         cg->stopUsingRegister(U);
         cg->stopUsingRegister(UL);
      }

      //      C[k] = R0;
      TR::MemoryReference * Cref = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+k*4, cg);
      TR::Instruction *cursor = generateRXInstruction(cg, TR::InstOpCode::ST, node, R0, Cref);

      if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Store %d least significant digit of final product", k);

      cg->stopUsingRegister(R0);
      R0 = R1;
      R1 = R2;
   }

   cg->stopUsingRegister(zero);
   cg->stopUsingRegister(R0);
   cg->stopUsingRegister(R1);
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));
   return NULL;
   }

#if 0 // Used for testing, will be used for RSA in future
TR::Register *
inline128Multiply(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction *cursor;
   cg->decReferenceCount(node->getFirstChild());

   TR::Register * Carr = cg->evaluate(node->getChild(1));
   TR::Register * Aarr = cg->evaluate(node->getChild(2));
   TR::Register * Barr = cg->evaluate(node->getChild(3));

   // There are alot more registers here then really needed, but since this was intentionally written
   // not to have internal control flow, RA can do its job properly, so mostly try not to reuse registers

   TR::Register *A0 = cg->allocateRegister();
   TR::Register *A1 = cg->allocateRegister();
   TR::Register *B0 = cg->allocateRegister();
   TR::Register *B1 = cg->allocateRegister();
   TR::Register *Z00 = cg->allocateRegister();
   TR::Register *Z01 = cg->allocateRegister();
   TR::Register *Z02 = cg->allocateRegister();
   TR::Register *Z03 = cg->allocateRegister();

   TR::MemoryReference * Aref24  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+24, cg);
   TR::MemoryReference * Aref16  = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);
   TR::MemoryReference * Bref24  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+24, cg);
   TR::MemoryReference * Bref16  = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);

   TR::MemoryReference * Cref24  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+24, cg);
   TR::MemoryReference * Cref16  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);
   TR::MemoryReference * Cref8   = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+8, cg);
   TR::MemoryReference * Cref0   = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+0, cg);

   cursor =
   generateRXInstruction(cg, TR::InstOpCode::LG, node, A0, Aref24);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, A1, Aref16);

   generateRXInstruction(cg, TR::InstOpCode::LG, node, B0, Bref24);
   generateRXInstruction(cg, TR::InstOpCode::LG, node, B1, Bref16);

   if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Load inputs");

   // Multiply [Z03,Z02,Z01,Z00] = [A1,A0] * [B1,B0]
   inline128Multiply(node, cg,
         Z00, Z01, Z02, Z03,
         A0, A1, B0, B1);

   /**
    *  Equals to:
    *  +---------------+---------------+---------------+---------------+
    *  |  Z23  |  Z22  |  Z21  |  Z20  |  Z03  |  Z02  |  Z01  |  Z00  |
    *  +---------------+---------------+---------------+---------------+
    *
    */

   cursor =
   generateRXInstruction(cg, TR::InstOpCode::STG, node, Z00, Cref24);
   generateRXInstruction(cg, TR::InstOpCode::STG, node, Z01, Cref16);
   generateRXInstruction(cg, TR::InstOpCode::STG, node, Z02, Cref8);
   generateRXInstruction(cg, TR::InstOpCode::STG, node, Z03, Cref0);

   if(cg->getDebug())
      cg->getDebug()->addInstructionComment(cursor, "Finish storing out result");

   cg->stopUsingRegister(A0);
   cg->stopUsingRegister(A1);
   cg->stopUsingRegister(B0);
   cg->stopUsingRegister(B1);

   cg->stopUsingRegister(Z00);
   cg->stopUsingRegister(Z01);
   cg->stopUsingRegister(Z02);
   cg->stopUsingRegister(Z03);

   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   return NULL;
   }

TR::Register *
inlineSIMD128Multiply(TR::Node * node, TR::CodeGenerator * cg)
   {
   return inline128Multiply(node, cg);

   TR::Instruction *cursor;
   cg->decReferenceCount(node->getFirstChild());

   TR::Register * Carr = cg->evaluate(node->getChild(1));
   TR::Register * Aarr = cg->evaluate(node->getChild(2));
   TR::Register * Barr = cg->evaluate(node->getChild(3));

   TR::Register *A = cg->allocateRegister(TR_VRF);
   TR::Register *B = cg->allocateRegister(TR_VRF);
   TR::Register *lowC = cg->allocateRegister(TR_VRF);
   TR::Register *highC = cg->allocateRegister(TR_VRF);

   TR::Register *vecImm4 = NULL;
   TR::Register *vecShift = NULL;

   TR::MemoryReference * Aref   = generateS390MemoryReference(Aarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);
   TR::MemoryReference * Bref   = generateS390MemoryReference(Barr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);

   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, A, Aref);
   generateVRXInstruction(cg, TR::InstOpCode::VL,    node, B, Bref);

   inlineSIMD128Multiply(node, cg,
         lowC, highC, A, B,
         vecImm4, vecShift);

   TR::MemoryReference * Cref16 = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+16, cg);
   TR::MemoryReference * Cref0  = generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+0, cg);

   generateVRXInstruction(cg, TR::InstOpCode::VST, node, highC, Cref0, 3);
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, lowC, Cref16, 3);


   cg->stopUsingRegister(vecImm4);
   cg->stopUsingRegister(vecShift);
   cg->stopUsingRegister(A);
   cg->stopUsingRegister(B);
   cg->stopUsingRegister(lowC);
   cg->stopUsingRegister(highC);

   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));

   return NULL;
   }
#endif

TR::Register *
inlineP256Mod(TR::Node * node, TR::CodeGenerator * cg)
   {
   cg->decReferenceCount(node->getFirstChild());
   TR::Register * RESarr = cg->evaluate(node->getChild(1));
   TR::Register * Carr = cg->evaluate(node->getChild(2));

   TR::Register* RESregs[9];
   for (int i = 0; i<9; i++)
      {
      RESregs[i] = cg->allocateRegister();
      }

#define p256digit(x) generateS390MemoryReference(Carr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+4*(15-x), cg)

   TR::MemoryReference* partialOperands[10][8] =
      {
  // Add the following (2*S3 + 2*S2 + S1 + S4 + S5)
 /*S3*/ {         NULL,          NULL,          NULL, p256digit(12), p256digit(13), p256digit(14), p256digit(15),          NULL},
 /*S2*/ {         NULL,          NULL,          NULL, p256digit(11), p256digit(12), p256digit(13), p256digit(14), p256digit(15)},
 /*S2*/ {         NULL,          NULL,          NULL, p256digit(11), p256digit(12), p256digit(13), p256digit(14), p256digit(15)},
 /*S1*/ {p256digit( 0), p256digit( 1), p256digit( 2), p256digit( 3), p256digit( 4), p256digit( 5), p256digit( 6), p256digit( 7)},
 /*S4*/ {p256digit( 8), p256digit( 9), p256digit(10),          NULL,          NULL,          NULL, p256digit(14), p256digit(15)},
 /*S5*/ {p256digit( 9), p256digit(10), p256digit(11), p256digit(13), p256digit(14), p256digit(15), p256digit(13), p256digit( 8)},
  // Subtract the following
 /*S6*/ {p256digit(11), p256digit(12), p256digit(13),          NULL,          NULL,          NULL, p256digit( 8), p256digit(10)},
 /*S7*/ {p256digit(12), p256digit(13), p256digit(14), p256digit(15),          NULL,          NULL, p256digit( 9), p256digit(11)},
 /*S8*/ {p256digit(13), p256digit(14), p256digit(15), p256digit( 8), p256digit( 9), p256digit(10),          NULL, p256digit(12)},
 /*S9*/ {p256digit(14), p256digit(15),          NULL, p256digit( 9), p256digit(10), p256digit(11),          NULL, p256digit(13)}
      };
#undef p256digit

   TR::Register * zero = cg->allocateRegister();

   generateRIInstruction(cg, TR::InstOpCode::LHI, node, zero, 0);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, RESregs[0], 0);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, RESregs[1], 0);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, RESregs[2], 0);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, RESregs[7], 0);
   generateRIInstruction(cg, TR::InstOpCode::LHI, node, RESregs[8], 0);

//   addNoMod(res, s3, s3);

   generateRXInstruction(cg, TR::InstOpCode::L, node, RESregs[3], partialOperands[0][3]);
   generateRRInstruction(cg, TR::InstOpCode::ALR, node, RESregs[3], RESregs[3]);
   for (int i=4; i<7; i++) // i = { 4, 5, 6}
      {
      generateRXInstruction(cg, TR::InstOpCode::L, node, RESregs[i], partialOperands[0][i]);
      generateRREInstruction(cg, TR::InstOpCode::ALCR, node, RESregs[i], RESregs[i]);
      }
   TR::Instruction *cursor = generateRREInstruction(cg, TR::InstOpCode::ALCR, node, RESregs[7], zero);
   if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Added S3 + S3");

//   addNoMod(res, res, s2);
//   addNoMod(res, res, s2);
   for (int j = 1; j<=2; j++) // j = {1, 2}
      {
      generateRXInstruction(cg, TR::InstOpCode::AL, node, RESregs[3], partialOperands[j][3]);
      for (int i=4; i<8; i++) // i = { 4, 5, 6, 7}
         {
         generateRXInstruction(cg, TR::InstOpCode::ALC, node, RESregs[i], partialOperands[j][i]);
         }
      cursor = generateRREInstruction(cg, TR::InstOpCode::ALCR, node, RESregs[8], zero);
      if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Added +S2");
      }

//   addNoMod(res, res, s1);
//   addNoMod(res, res, s4);
//   addNoMod(res, res, s5);
   for (int j = 3; j<=5; j++) // j = {3, 4, 5}
      {
      generateRXInstruction(cg, TR::InstOpCode::AL, node, RESregs[0], partialOperands[j][0]);
      for (int i = 1; i<8; i++) // i = {1, 2, 3, 4, 5, 6, 7}
         {
         if (0 == partialOperands[j][i])
            generateRREInstruction(cg, TR::InstOpCode::ALCR, node, RESregs[i], zero);
         else
            generateRXInstruction(cg, TR::InstOpCode::ALC, node, RESregs[i], partialOperands[j][i]);
         }
      cursor = generateRREInstruction(cg, TR::InstOpCode::ALCR, node, RESregs[8], zero);
      if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Added +S%%d", j==3?1:j);
      }

//   subNoMod(res, res, s6);
//   subNoMod(res, res, s7);
//   subNoMod(res, res, s8);
//   subNoMod(res, res, s9);

   for (int j = 6; j<=9; j++)
      {
      generateRXInstruction(cg, TR::InstOpCode::SL, node, RESregs[0], partialOperands[j][0]);
      for (int i = 1; i<8; i++) // i = {1, 2, 3, 4, 5, 6, 7}
         {
         if (0 == partialOperands[j][i])
            generateRREInstruction(cg, TR::InstOpCode::SLBR, node, RESregs[i], zero);
         else
            generateRXInstruction(cg, TR::InstOpCode::SLB, node, RESregs[i], partialOperands[j][i]);
         }
      cursor = generateRREInstruction(cg, TR::InstOpCode::SLBR, node, RESregs[8], zero);
      if(cg->getDebug())
         cg->getDebug()->addInstructionComment(cursor, "Subtracted -S%%d", j);
      }

   // Store out the result
   for (int i = 0, j = 15; i<9; i++, j--)
      {
   TR::MemoryReference * RESref = generateS390MemoryReference(RESarr, TR::Compiler->om.contiguousArrayHeaderSizeInBytes()+j*4, cg);
      generateRXInstruction(cg, TR::InstOpCode::ST, node, RESregs[i], RESref);
      cg->stopUsingRegister(RESregs[i]);
      }

   cg->stopUsingRegister(zero);
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));

   return NULL;
   }

TR::Register *inlineShortReverseBytes(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineShortReverseBytes");
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   cg->decReferenceCount(firstChild);
   TR::Register *trgReg = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::LRVR, node, trgReg, srcReg);
   generateRSInstruction(cg, TR::InstOpCode::SRA, node, trgReg, 16);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *inlineIntegerReverseBytes(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineIntegerReverseBytes");
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   cg->decReferenceCount(firstChild);
   TR::Register *trgReg = cg->allocateRegister();
   generateRRInstruction(cg, TR::InstOpCode::LRVR, node, trgReg, srcReg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *inlineLongReverseBytes(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren()==1, "Wrong number of children in inlineIntegerReverseBytes");
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   cg->decReferenceCount(firstChild);
   TR::Register *trgReg =cg->allocateRegister();
   generateRREInstruction(cg, TR::InstOpCode::LRVGR, node, trgReg, srcReg);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::evaluateLengthMinusOneForMemoryCmp(TR::Node *node, TR::CodeGenerator *cg, bool clobberEvaluate, bool &lenMinusOne)
   {
   TR::Register *reg;
   if (((node->getOpCodeValue()==TR::iadd && node->getSecondChild()->getOpCodeValue()==TR::iconst && node->getSecondChild()->getInt() == 1)   ||
        (node->getOpCodeValue()==TR::isub && node->getSecondChild()->getOpCodeValue()==TR::iconst && node->getSecondChild()->getInt() == -1)) &&
       (node->getRegister() == NULL))
       {
       if (clobberEvaluate)
          reg = cg->gprClobberEvaluate(node->getFirstChild());
       else
          reg = cg->evaluate(node->getFirstChild());
       lenMinusOne=true;

       if (node->getReferenceCount() > 1)
         {
         // set the register for the parent node to keep it alive if it is going to be used again
         node->setRegister(reg);
         }

       cg->decReferenceCount(node->getFirstChild());
       cg->decReferenceCount(node->getSecondChild());
       }
   else
      {
       if (clobberEvaluate)
          reg = cg->gprClobberEvaluate(node);
       else
          reg = cg->evaluate(node);
      }
   return reg;
   }

/**
 * Obtain the right condition code for ificmpXX
 */
TR::InstOpCode::S390BranchCondition getStandardIfBranchConditionForArraycmp(TR::Node * ifxcmpXXNode, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::S390BranchCondition brCond = TR::InstOpCode::COND_NOP;

   TR::ILOpCodes opCode = ifxcmpXXNode->getOpCodeValue();

   bool isIficmp = opCode == TR::ificmpeq ||
                   opCode == TR::ificmpne ||
                   opCode == TR::ificmplt ||
                   opCode == TR::ificmpgt ||
                   opCode == TR::ificmple ||
                   opCode == TR::ificmpge;

   //This routine only handles ificmpXX
   TR_ASSERT(isIficmp,  "Other opCodes are not handled!");

   TR::Node *opNode = ifxcmpXXNode->getChild(0);
   TR::Node *constNode = ifxcmpXXNode->getChild(1);

   bool isArraycmp = isIficmp && (opNode->getOpCodeValue() == TR::arraycmp);

   //Only handle Arraycmp if it is under Ificmp.
   TR_ASSERT(!isIficmp || isArraycmp,  "Only handle Memcmp or Arraycmp under ificmppXX!");


   int64_t compareVal = constNode->getIntegerNodeValue<int64_t>();
   return getStandardIfBranchCondition(opCode, compareVal);
   }

void setStartInternalControlFlow(TR::CodeGenerator * cg, TR::Node * node, TR::Instruction * cursor, bool& isStartInternalControlFlowSet)
   {
   TR_ASSERT(cursor && cursor->getPrev(), "cannot set ICF without previous instruction");
   if (!isStartInternalControlFlowSet)
      {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart, cursor->getPrev());
      cFlowRegionStart->setStartInternalControlFlow();
      isStartInternalControlFlowSet = true;
      }
   }

/**
 * ArraycmpHelper is extended based on inlineMemcmpEvaluator to handle both memcmp and arraycmp.
 * The differences are
 * 1. return102: false by default - return -1/0/1 for memcmpEvaluator, otherwise 1/0/2 for arraycmpEvaluator.
 * 2. Allow no result register returned and no branch target taken if compareTarget is NULL and needResultReg is false.
 * Notice that
 * 1. it handles neither arraycmpSign nor arraycmpLen
 * 2. Whether it is if-folding is considered first, then whether or not it needs a result register.
 * 3. support ificmpXX.
 */
TR::Register *
OMR::Z::TreeEvaluator::arraycmpHelper(TR::Node *node,
                                         TR::CodeGenerator *cg,
                                         bool isWideChar,
                                         bool isEqualCmp,
                                         int32_t cmpValue,
                                         TR::LabelSymbol *compareTarget,
                                         TR::Node *ificmpNode,
                                         bool needResultReg,
                                         bool return102,
                                         TR::InstOpCode::S390BranchCondition *returnCond)
   {
   TR::Compilation *comp = cg->comp();

   if (TR::isJ9() && !comp->getOption(TR_DisableSIMDArrayCompare) && cg->getSupportsVectorRegisters() && !isWideChar)
      {
      // An empirical study has showed that CLC is faster for all array sizes if the number of bytes to copy is known to be constant
      if (!node->getChild(2)->getOpCode().isLoadConst())
          return TR::TreeEvaluator::arraycmpSIMDHelper(node, cg, compareTarget, ificmpNode, needResultReg, return102);
      }

   TR::Node *source1Node = node->getChild(0);
   TR::Node *source2Node = node->getChild(1);
   TR::Node *lengthNode  = node->getChild(2);

   bool isFoldedIf = compareTarget != NULL;

   bool ordinal = ificmpNode && ificmpNode->getOpCode().isCompareForOrder();

   TR::InstOpCode::S390BranchCondition ifxcmpBrCond = TR::InstOpCode::COND_NOP;
   if (isFoldedIf)
      {
      //compareTarget/isEqualCmp are really redundant.
      TR_ASSERT(ificmpNode && compareTarget, "Both ificmpNode && compareTarget are required for now");
      ifxcmpBrCond = getStandardIfBranchConditionForArraycmp(ificmpNode, cg);
      }

   bool isIfxcmpBrCondContainEqual = getMaskForBranchCondition(ifxcmpBrCond) & 0x08;
   bool isStartInternalControlFlowSet = false;
   bool isCondLabelNeeded = true;
   bool isEndLabelNeeded = false;

   uint64_t length = 0;
   bool isConstLength = false;

   if (lengthNode->getOpCode().isLoadConst())
      {
      length = lengthNode->getIntegerNodeValue<uint64_t>();
      if(isWideChar)
         length *= 2;
      if(length < 4096)
         isConstLength = true;
      }

   TR::MemoryReference *baseSource1Ref = NULL;
   TR::MemoryReference *baseSource2Ref = NULL;
   TR::Register *retValReg = NULL;
   TR::Instruction *cursor = NULL;

   if(!isFoldedIf && needResultReg)
      retValReg = cg->allocateRegister();

   if ((isConstLength && (length == 0)) || (source1Node == source2Node))
      {
      TR::RegisterDependencyConditions *deps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode);
      if(isFoldedIf)
         {
         if (isIfxcmpBrCondContainEqual) //Branch if it is equal.
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, compareTarget, deps);
            }
         }
      else
         {
         if (needResultReg)
            generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, retValReg, retValReg);
         }

      cg->recursivelyDecReferenceCount(lengthNode);
      cg->processUnusedNodeDuringEvaluation(source1Node);
      cg->processUnusedNodeDuringEvaluation(source2Node);
      if (returnCond!=NULL)
         {
         if (isEqualCmp)
            *returnCond = TR::InstOpCode::COND_MASK15;
         else
            *returnCond = TR::InstOpCode::COND_MASK0;
         }
      }
   else if (isConstLength)
      {
      if (node->getOpCode().hasSymbolReference() && node->getSymbolReference())
         {
         baseSource1Ref = generateS390MemoryReference(cg, node, source1Node, 0, true); // forSS=true
         baseSource2Ref = generateS390MemoryReference(cg, node, source2Node, 0, true); // forSS=true
         }
      else
         {
         // memcmp2 lacks a symbol reference
         baseSource1Ref = generateS390MemoryReference(cg->evaluate(source1Node), 0, cg);
         baseSource2Ref = generateS390MemoryReference(cg->evaluate(source2Node), 0, cg);
         cg->decReferenceCount(source1Node);
         cg->decReferenceCount(source2Node);
         }

      uint32_t loopIters = (uint32_t)(length / TR_MAX_CLC_SIZE);
      TR::Register *baseReg1Temp = NULL;
      TR::Register *baseReg2Temp = NULL;

         {
         // if any of the to be generated CLC instructions will need a large disp then generate an LA up front
         if (loopIters * TR_MAX_CLC_SIZE + baseSource1Ref->getOffset() > TR_MAX_SS_DISP)
            {
            baseReg1Temp = cg->allocateRegister();
            cg->genLoadAddressToRegister(baseReg1Temp, baseSource1Ref, source1Node);
            baseSource1Ref = generateS390MemoryReference(baseReg1Temp, 0, cg);
            }

         if (loopIters * TR_MAX_CLC_SIZE + baseSource2Ref->getOffset() > TR_MAX_SS_DISP)
            {
            baseReg2Temp = cg->allocateRegister();
            cg->genLoadAddressToRegister(baseReg2Temp, baseSource2Ref, source2Node);
            baseSource2Ref = generateS390MemoryReference(baseReg2Temp, 0, cg);
            }
         }

      TR::RegisterDependencyConditions *deps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode);
      TR::RegisterDependencyConditions *regDeps = NULL;

      // Counter for the number of bytes already compared
      uint32_t compared = 0;

      bool earlyCLCEnabled = TR::isJ9();

      uint32_t earlyCLCValue = 8;

      // Threshold for when to generate a preemptive CLC to avoid pulling in 2 cache lines if the arrays differ towards the beginning
      uint32_t earlyCLCThreshold = 32;

      isEndLabelNeeded = loopIters || needResultReg || length > (earlyCLCEnabled ? earlyCLCThreshold : TR_MAX_CLC_SIZE);

      if (length <= (earlyCLCEnabled ? earlyCLCThreshold : TR_MAX_CLC_SIZE) && (isFoldedIf || !needResultReg))
         {
         // these are the only cases where no regDeps are needed as code will be just CLC/BRC with no internal control flow
         regDeps = deps;
         isCondLabelNeeded = false;
         }
      else
         {
         uint8_t numDeps = 0;
         // typically only 1 dep is needed but up to 4 could be needed so pre-compute to save space in most cases as regdep memory must be pre-allocated
         if (length > (earlyCLCEnabled ? earlyCLCThreshold : TR_MAX_CLC_SIZE)) // NOTE: similar logic below when actually adding to regDeps
            {
            if (baseSource1Ref->getIndexRegister())
                  numDeps++;
            if (baseSource1Ref->getBaseRegister())
                  numDeps++;
            if (baseSource2Ref->getIndexRegister() && (baseSource2Ref->getIndexRegister() != baseSource1Ref->getIndexRegister()))
                  numDeps++;
            if (baseSource2Ref->getBaseRegister() && (baseSource2Ref->getBaseRegister() != baseSource1Ref->getBaseRegister()))
                  numDeps++;
            }

         if (isFoldedIf)
            {
            if (deps == NULL)
               regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, cg);
            else
               regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(deps, 0, numDeps, cg);
            }
         else
            {
            if (needResultReg)
               {
               numDeps++; // for retValReg
               regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numDeps, cg);
               regDeps->addPostCondition(retValReg, TR::RealRegister::AssignAny);
               }
            }

         if (length > (earlyCLCEnabled ? earlyCLCThreshold : TR_MAX_CLC_SIZE))
            {
            if (baseSource1Ref->getIndexRegister())
               regDeps->addPostConditionIfNotAlreadyInserted(baseSource1Ref->getIndexRegister(), TR::RealRegister::AssignAny);

            if (baseSource1Ref->getBaseRegister())
               regDeps->addPostConditionIfNotAlreadyInserted(baseSource1Ref->getBaseRegister(), TR::RealRegister::AssignAny);

            if (baseSource2Ref->getIndexRegister() && (baseSource2Ref->getIndexRegister() != baseSource1Ref->getIndexRegister()))
               regDeps->addPostConditionIfNotAlreadyInserted(baseSource2Ref->getIndexRegister(), TR::RealRegister::AssignAny);

            if (baseSource2Ref->getBaseRegister() && (baseSource2Ref->getBaseRegister() != baseSource1Ref->getBaseRegister()))
               regDeps->addPostConditionIfNotAlreadyInserted(baseSource2Ref->getBaseRegister(), TR::RealRegister::AssignAny);
            }
         }

      TR::LabelSymbol *condLabel = isCondLabelNeeded ? generateLabelSymbol(cg) : NULL;

      TR::LabelSymbol *endLabel = isEndLabelNeeded ? generateLabelSymbol(cg) : NULL;

      if (!isFoldedIf && needResultReg)
         generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, retValReg, retValReg);

      if (earlyCLCEnabled && length > earlyCLCThreshold)
         {
         TR::MemoryReference *source1Ref = generateS390MemoryReference(*baseSource1Ref, compared, cg);
         TR::MemoryReference *source2Ref = generateS390MemoryReference(*baseSource2Ref, compared, cg);

         compared = earlyCLCValue;

         generateSS1Instruction(cg, TR::InstOpCode::CLC, node, compared - 1, source1Ref, source2Ref);

         // Recompute the number of loop iterations as we just compared 8 bytes
         loopIters = (uint32_t)((length - compared) / TR_MAX_CLC_SIZE);

         if (isFoldedIf)
            {
            if (ifxcmpBrCond == TR::InstOpCode::COND_MASK8)
               setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, endLabel),
                                           isStartInternalControlFlowSet);
            else
               setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, condLabel),
                                           isStartInternalControlFlowSet);
            }
         else
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, condLabel);
         }

      bool isPerfectMultiple = ((length - compared) % TR_MAX_CLC_SIZE) == 0;

      for (uint32_t i = 0; i < loopIters; ++i, compared += TR_MAX_CLC_SIZE)
         {
         TR::MemoryReference *source1Ref = generateS390MemoryReference(*baseSource1Ref, compared, cg);
         TR::MemoryReference *source2Ref = generateS390MemoryReference(*baseSource2Ref, compared, cg);

         generateSS1Instruction(cg, TR::InstOpCode::CLC, node, TR_MAX_CLC_SIZE - 1, source1Ref, source2Ref);

         if (isPerfectMultiple && (i == loopIters - 1))
            {
            if (isFoldedIf)
               {
                //generateS390BranchInstruction(cg, TR::InstOpCode::BRC, ifxcmpBrCond, node, compareTarget);
               }
            else
               setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, endLabel),
                                           isStartInternalControlFlowSet);
            }
         else
            {
            if (isFoldedIf)
               {
               if (ifxcmpBrCond == TR::InstOpCode::COND_MASK8)
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, endLabel),
                                              isStartInternalControlFlowSet);
               else
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, condLabel),
                                              isStartInternalControlFlowSet);
               }
            else
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, condLabel);
            }
         }

      if (!isPerfectMultiple)
         {
         TR::MemoryReference *source1Ref = generateS390MemoryReference(*baseSource1Ref, compared, cg);
         TR::MemoryReference *source2Ref = generateS390MemoryReference(*baseSource2Ref, compared, cg);

         generateSS1Instruction(cg, TR::InstOpCode::CLC, node, length - compared - 1, source1Ref, source2Ref);

         if (!isFoldedIf && needResultReg)
            setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, endLabel),
                                        isStartInternalControlFlowSet);
         }

      if (isCondLabelNeeded)
         {
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, condLabel);
         if (regDeps)
            cursor->setDependencyConditions(regDeps);
         }

      if (isFoldedIf)
         {
         cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, ifxcmpBrCond, node, compareTarget);
         }
      else
         {
         if (needResultReg)
            {
            if (return102)
               {
               generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, 2);

               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK2, node, endLabel);
               cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, 1);
               }
            else
               {
               generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, 1);

               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK2, node, endLabel);
               cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, -1);
               }
            }
         }

      TR::LabelSymbol * cFlowRegionEnd;

      if (endLabel)
         {
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, endLabel);
         cFlowRegionEnd = endLabel;
         }
      else
         {
         cFlowRegionEnd = generateLabelSymbol(cg);
         cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, cursor);
         }

      if (regDeps)
         {
         cursor->setDependencyConditions(regDeps);
         }

      if (isStartInternalControlFlowSet)
         {
         cFlowRegionEnd->setEndInternalControlFlow();
         }

      if (baseReg1Temp)
         cg->stopUsingRegister(baseReg1Temp);
      if (baseReg2Temp)
         cg->stopUsingRegister(baseReg2Temp);

      cg->decReferenceCount(lengthNode);
      }
   else
      {
      //general case
      TR::Register *source1Reg = NULL;
      TR::Register *source2Reg = NULL;
      TR::Register *lengthReg = NULL;

      // If it can be proven that this operation will operate
      // within 256 bytes, we can elide the generation of the
      // residue loop. Currently, we don't have a way to determine
      // this, however the codegen below does understand how
      // to do this.
      bool maxLenIn256 = false;


      bool lengthCanBeZero = true;
      bool lenMinusOne = false;
      bool needs64BitOpCode = false;

      TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);

      TR::Register *loopCountReg = NULL;

      TR::Register *lengthCopyReg = NULL;
      TR::RegisterPair *source1Pair = NULL;
      TR::RegisterPair *source2Pair = NULL;

      if (lengthNode)
         {
         needs64BitOpCode = lengthNode->getSize() > 4;
         }
      else
         {
         needs64BitOpCode =  cg->comp()->target().is64Bit();
         }

      if (maxLenIn256)
         {
         source1Reg = cg->evaluate(source1Node);
         source2Reg = cg->evaluate(source2Node);
         lengthReg = TR::TreeEvaluator::evaluateLengthMinusOneForMemoryCmp(lengthNode, cg, true, lenMinusOne);

         if (isWideChar)
            {
            if(needs64BitOpCode)
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lengthReg, lengthReg, 1);
            else
               generateRSInstruction(cg, TR::InstOpCode::SLL, node, lengthReg, 1);
            }
         }
      else
         {
         if (isWideChar)
            {
            source1Reg = cg->gprClobberEvaluate(source1Node, true);
            source2Reg = cg->gprClobberEvaluate(source2Node, true);
            lengthReg = cg->gprClobberEvaluate(lengthNode, true);
            }
         else
            {
            source1Reg = cg->gprClobberEvaluate(source1Node);
            source2Reg = cg->gprClobberEvaluate(source2Node);
            lengthReg = cg->evaluateLengthMinusOneForMemoryOps(lengthNode, true, lenMinusOne);
            }
         }

      TR::RegisterDependencyConditions *deps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode);

      TR::RegisterDependencyConditions *branchDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(deps, 0,
                                                                                                                     maxLenIn256 ? 5 : (isWideChar ? (isFoldedIf ? 10 : 11) : 5 + 4),
                                                                                                                     cg);

      //We could do better job in future for the known length (>=4k)
      //Currently there are some unnecessary test/load instructions generated.

      if (maxLenIn256)
         {
         if (!isFoldedIf && needResultReg)
            {
            branchDeps->addPostCondition(retValReg, TR::RealRegister::AssignAny);
            generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, retValReg, retValReg);
            }

         if (!lenMinusOne)
            {
            generateRIInstruction(cg, (needs64BitOpCode) ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, node, lengthReg, -1);
            }

         if (lengthCanBeZero)
            {
            if (lenMinusOne)
               generateRRInstruction(cg, (needs64BitOpCode) ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, lengthReg, lengthReg);

            //Strings of 0 length are considered equal
            if (isFoldedIf)
               {
               if (isIfxcmpBrCondContainEqual)
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, compareTarget),
                                              isStartInternalControlFlowSet);
               else
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, cFlowRegionEnd),
                                              isStartInternalControlFlowSet);
               }
            else
               setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, cFlowRegionEnd),
                                            isStartInternalControlFlowSet);
            }

         TR::MemoryReference *source1MemRef = generateS390MemoryReference(source1Reg, 0, cg);
         TR::MemoryReference *source2MemRef = generateS390MemoryReference(source2Reg, 0, cg);

         cursor = cg->getAppendInstruction();
         cursor = generateSS1Instruction(cg, TR::InstOpCode::CLC, node, 0, source1MemRef, source2MemRef, cursor);

         cursor = generateEXDispatch(node, cg, lengthReg, cursor, 0, branchDeps);
         }
      else
         {
         if (isWideChar)
            {
            //This part is the orginal code that can handle all general cases including isWideChar via CLCLE/CLCLU
            //Now it is used only for WideChar as CLC loop is prefered on other general cases
            //Later we can use maxlength to determine a threshhold to do CLCLE

            lengthCopyReg = cg->allocateRegister();

            source1Pair = cg->allocateConsecutiveRegisterPair(lengthReg, source1Reg);

            source2Pair = cg->allocateConsecutiveRegisterPair(lengthCopyReg, source2Reg);

            branchDeps = cg->createDepsForRRMemoryInstructions(node, source1Pair, source2Pair);

            //compareDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, isFoldedIf ? 6 : 7, cg);

            if(!isFoldedIf && needResultReg)
               {
               branchDeps->addPostCondition(retValReg, TR::RealRegister::AssignAny);
               generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, retValReg, retValReg);
               }

            //cFlowRegionEnd = generateLabelSymbol(cg);

            generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LTGR : TR::InstOpCode::LTR, node, lengthReg, lengthReg);

            //The following test may not be necessary as TR::InstOpCode::CLCLU/TR::InstOpCode::CLCLE does not care zero legth case?
            if (isFoldedIf)
               {
               if (isIfxcmpBrCondContainEqual)
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, compareTarget),
                                               isStartInternalControlFlowSet);
               else
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, cFlowRegionEnd),
                                               isStartInternalControlFlowSet);
               }
            else
               setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, cFlowRegionEnd),
                                               isStartInternalControlFlowSet);

            if(isWideChar)
               {
               if(cg->comp()->target().is64Bit())
                  generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lengthReg, lengthReg, 1);
               else
                  generateRSInstruction(cg, TR::InstOpCode::SLL, node, lengthReg, 1);
               }
            generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, lengthCopyReg, lengthReg);

            TR::LabelSymbol *continueLabel = generateLabelSymbol(cg);
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, continueLabel);
            cursor = generateRSWithImplicitPairStoresInstruction(cg, isWideChar ? TR::InstOpCode::CLCLU : TR::InstOpCode::CLCLE, node, source1Pair, source2Pair, generateS390MemoryReference(NULL, 64, cg));

            //cursor->setDependencyConditions(compareDeps);
            cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK1, node, continueLabel);
            //cursor->setDependencyConditions(branchDeps);


            }
         else
            {
            //This part uses CLC loop to do the compare
            //CLC loop is considered superior to CLCLE -- task 33860

            if (!isFoldedIf && needResultReg)
               {
               branchDeps->addPostCondition(retValReg, TR::RealRegister::AssignAny);
               generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, retValReg, retValReg);
               }

            loopCountReg = cg->allocateRegister();

            if (!lenMinusOne)
               {
               generateRIInstruction(cg, (needs64BitOpCode) ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, node, lengthReg, -1);
               }

            if (lengthCanBeZero)
               {
               if (lenMinusOne)
                  generateRRInstruction(cg, TR::InstOpCode::LTR, node, lengthReg, lengthReg); //The LengthminuxOne uses "TR::iadd length, -1"

               //Strings of 0 length are considered equal
               if (isFoldedIf)
                  {
                  if (isIfxcmpBrCondContainEqual)
                    // generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, compareTarget);
                     setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, compareTarget),
                                                 isStartInternalControlFlowSet);
                  else
                     setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, cFlowRegionEnd),
                                                 isStartInternalControlFlowSet);
                  }
               else
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, cFlowRegionEnd),
                                              isStartInternalControlFlowSet);
               }

            branchDeps->addPostConditionIfNotAlreadyInserted(source1Reg, TR::RealRegister::AssignAny);
            branchDeps->addPostConditionIfNotAlreadyInserted(source2Reg, TR::RealRegister::AssignAny);
            branchDeps->addPostConditionIfNotAlreadyInserted(lengthReg, TR::RealRegister::AssignAny);
            branchDeps->addPostConditionIfNotAlreadyInserted(loopCountReg, TR::RealRegister::AssignAny);

            TR::LabelSymbol *topOfLoop    = generateLabelSymbol(cg);
            TR::LabelSymbol *bottomOfLoop = generateLabelSymbol(cg);
            TR::LabelSymbol *outOfCompare = generateLabelSymbol(cg);

           if (needs64BitOpCode)
              generateRSInstruction(cg, TR::InstOpCode::SRAG, node, loopCountReg, lengthReg, 8);
           else
              {
              generateRRInstruction(cg, TR::InstOpCode::LR, node, loopCountReg, lengthReg);
              generateRSInstruction(cg, TR::InstOpCode::SRA, node, loopCountReg, 8);
              }

            setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, bottomOfLoop),
                                        isStartInternalControlFlowSet);

            TR::MemoryReference *source1MemRef = generateS390MemoryReference(source1Reg, 0, cg);
            TR::MemoryReference *source2MemRef = generateS390MemoryReference(source2Reg, 0, cg);

            TR::LabelSymbol * EXTargetLabel;

            if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && !comp->getOption(TR_DisableInlineEXTarget))
               {
               // Inline the EXRL target so it is always in the instruction cache
               generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, EXTargetLabel = generateLabelSymbol(cg));

               generateSS1Instruction(cg, TR::InstOpCode::CLC, node, 0, source1MemRef, source2MemRef);

               source1MemRef = generateS390MemoryReference(source1Reg, 0, cg);
               source2MemRef = generateS390MemoryReference(source2Reg, 0, cg);
               }

            //Loop
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, topOfLoop);
            generateSS1Instruction(cg, TR::InstOpCode::CLC, node, 255, source1MemRef, source2MemRef);

            if (isFoldedIf)
               {
               if (ifxcmpBrCond == TR::InstOpCode::COND_MASK8)
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, cFlowRegionEnd),
                                              isStartInternalControlFlowSet);
               else
                  setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, outOfCompare),
                                              isStartInternalControlFlowSet);
               }
            else
               setStartInternalControlFlow(cg, node, generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, outOfCompare),
                                           isStartInternalControlFlowSet);

            generateRXInstruction(cg, TR::InstOpCode::LA, node, source1Reg, generateS390MemoryReference(source1Reg, 256, cg));
            generateRXInstruction(cg, TR::InstOpCode::LA, node, source2Reg, generateS390MemoryReference(source2Reg, 256, cg));
            generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, loopCountReg, topOfLoop);
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, bottomOfLoop);

            //Remainder
            source1MemRef= generateS390MemoryReference(source1Reg, 0, cg);
            source2MemRef= generateS390MemoryReference(source2Reg, 0, cg);

            if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && !comp->getOption(TR_DisableInlineEXTarget))
               {
               cursor = new (cg->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::EXRL, node, lengthReg, EXTargetLabel, cg);
               }
            else
               {
               cursor = generateSS1Instruction(cg, TR::InstOpCode::CLC, node, 0, source1MemRef, source2MemRef);
               cursor = generateEXDispatch(node, cg, lengthReg, cursor, 0, branchDeps);
               }

            //Setup result
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, outOfCompare);
            }
         }

      if(isFoldedIf)
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, ifxcmpBrCond, node, compareTarget);
         }
      else
         {
         if (needResultReg)
            {
            if (return102)
               {
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, cFlowRegionEnd);

               generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, 2);

               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK2, node, cFlowRegionEnd);

               generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, 1);
               }
            else
               {
               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK8, node, cFlowRegionEnd);

               generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, 1);

               generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK2, node, cFlowRegionEnd);

               generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, retValReg, -1);
               }
            }
         }


      TR::RegisterDependencyConditions *regDepsCopy = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(branchDeps, 0, 0, cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, regDepsCopy);

      if (isStartInternalControlFlowSet)
         {
         cFlowRegionEnd->setEndInternalControlFlow();
         }

      cg->decReferenceCount(lengthNode);
      cg->decReferenceCount(source1Node);
      cg->decReferenceCount(source2Node);

      cg->stopUsingRegister(source1Reg);
      cg->stopUsingRegister(source2Reg);
      cg->stopUsingRegister(lengthReg);

      if (!maxLenIn256)
         {
         if (isWideChar)
            {
            cg->stopUsingRegister(lengthCopyReg);
            cg->stopUsingRegister(source1Pair);
            cg->stopUsingRegister(source2Pair);
            }
         else
            {
            cg->stopUsingRegister(loopCountReg);
            }

         }
      }

   if (isFoldedIf)
      {
      cg->decReferenceCount(node);
      return NULL;
      }
   else
      {
      if (needResultReg)
         {
         node->setRegister(retValReg);
         return retValReg;
         }
      else
         {
         return NULL;
         }
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::inlineIfArraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg, bool& inlined)
   {
   TR::Node *opNode = node->getChild(0);
   TR::Node *constNode = node->getChild(1);
   TR::Node *thirdChild = NULL;

   inlined = false;

   //Check if it can handle TR::arraycmp or TR::arraycmpWithPad
   if ((opNode->getOpCodeValue() != TR::arraycmp && opNode->getOpCodeValue() != TR::arraycmpWithPad) ||
       opNode->getRegister() != NULL ||
       opNode->getReferenceCount() > 1 ||
       node->getReferenceCount() > 1 ||
       !constNode->getOpCode().isLoadConst() ||
       (constNode->getInt() != 0 && constNode->getInt() != 1 && constNode->getInt() != 2))
      {
      return NULL;
      }

   TR::RegisterDependencyConditions *deps = NULL;
   int compareVal = constNode->getIntegerNodeValue<int>();
   bool isEqualCmp = (node->getOpCodeValue() == TR::ificmpeq);
   TR::LabelSymbol *compareTarget = node->getBranchDestination()->getNode()->getLabel();
   TR_ASSERT( compareTarget != NULL, "NULL compare target found in inlineIfArraycmpEvaluator");
   bool isWide = false;

   TR::Register *ret;
   if (opNode->getOpCodeValue() == TR::arraycmp)
      {
      ret = TR::TreeEvaluator::arraycmpHelper(opNode,
                                              cg,
                                              isWide,        //isWideChar
                                              isEqualCmp,    //isEqualCmp
                                              compareVal,    //cmpValue
                                              compareTarget, //compareTarget
                                              node,          //ificmpNode
                                              false,         //needResultReg
                                              true);         //return102
      }
   else
      {
      arraycmpWithPadHelper helper(opNode,
                                   cg,
                                   isWide,  //isUTF16
                                   true,    //isFoldedIf
                                   node,    //ificmpNode
                                   false);  //needResultReg
      ret = helper.generate();
      cg->decReferenceCount(opNode);
      }

   inlined = true;

   cg->decReferenceCount(constNode);
   return ret;
   }

/**
 * directCallEvaluator - direct call
 * TR::icall, TR::acall, TR::lcall, TR::fcall, TR::dcall, TR::call handled by directCallEvaluator
 */
TR::Register *
OMR::Z::TreeEvaluator::directCallEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register *resultReg = NULL;

   if (!cg->inlineDirectCall(node, resultReg))
      {
      TR::SymbolReference* symRef = node->getSymbolReference();

      if (symRef != NULL && symRef->getSymbol()->castToMethodSymbol()->isInlinedByCG())
         {
         TR::Compilation* comp = cg->comp();

         if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicAddSymbol))
            {
            resultReg = TR::TreeEvaluator::intrinsicAtomicAdd(node, cg);
            }
         else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicFetchAndAddSymbol))
            {
            resultReg = TR::TreeEvaluator::intrinsicAtomicFetchAndAdd(node, cg);
            }
         else if (comp->getSymRefTab()->isNonHelper(symRef, TR::SymbolReferenceTable::atomicSwapSymbol))
            {
            resultReg = TR::TreeEvaluator::intrinsicAtomicSwap(node, cg);
            }
         }

      if (resultReg == NULL)
         {
         resultReg = TR::TreeEvaluator::performCall(node, false, cg);
         }

      // TODO: This should likely get done at the point where we allocate the return register
      if (node->getOpCodeValue() == TR::lcall)
         {
         resultReg->setIs64BitReg(true);
         }
      }

   return resultReg;
   }

/**
 * indirectCallEvaluator - indirect call
 * TR::icalli, TR::acalli, TR::lcalli, TR::fcalli, TR::dcalli, TR::calli handled by indirectCallEvaluator
 */
TR::Register *
OMR::Z::TreeEvaluator::indirectCallEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   // If the method to be called is marked as an inline method, see if it can
   // actually be generated inline.
   //
   TR::MethodSymbol * symbol = node->getSymbol()->castToMethodSymbol();
   if ((symbol->isVMInternalNative() || symbol->isJITInternalNative()))
      {
#ifdef J9_PROJECT_SPECIFIC
      if (TR::TreeEvaluator::VMinlineCallEvaluator(node, true, cg))
         {
         return node->getRegister();
         }
#endif
      }

   TR::Register * returnRegister = TR::TreeEvaluator::performCall(node, true, cg);

   // TODO: This should likely get done at the point where we allocate the return register
   if (node->getOpCodeValue() == TR::lcalli)
      {
      returnRegister->setIs64BitReg(true);
      }

   return returnRegister;
   }

/**
 * treetopEvaluator - tree top to anchor subtrees with side-effects
 */
TR::Register *
OMR::Z::TreeEvaluator::treetopEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();

   if (node->getFirstChild()->getReferenceCount() == 1)
      {
      switch (node->getFirstChild()->getOpCodeValue())
         {
         case TR::aiadd:
            {
            if (comp->getOption(TR_TraceCG))
               {
               traceMsg(comp, " found %s [%p] with ref count 1 under treetop, avoiding evaluation into register.\n",
                        node->getFirstChild()->getOpCode().getName(), node->getFirstChild());
               }

            TR::MemoryReference * mr = generateS390MemoryReference(cg);
            mr->setForceFoldingIfAdvantageous(cg, node->getFirstChild());
            mr->populateMemoryReference(node->getFirstChild(), cg);
            cg->decReferenceCount(node->getFirstChild());

            return NULL;
            }
         }
      }

   TR::Register * tempReg = cg->evaluate(node->getFirstChild());
   cg->decReferenceCount(node->getFirstChild());
#ifdef J9_PROJECT_SPECIFIC
   if (tempReg && tempReg->getOpaquePseudoRegister())
      {
      TR_StorageReference *storageReference = tempReg->getOpaquePseudoRegister()->getStorageReference();
      // hints should only be used when the for store treetop nodes
      TR_ASSERT( !storageReference->isNodeBasedHint(),"should not have a node based hint under a treetop\n");

      if (storageReference->isNodeBased())
         {
         storageReference->decrementNodeReferenceCount();
         if (storageReference->getNode()->getOpCode().isIndirect() ||
             (storageReference->isConstantNodeBased() && storageReference->getNode()->getNumChildren() > 0))
            {
            TR::Node *addressChild = storageReference->getNode()->getFirstChild();
            if (storageReference->getNodeReferenceCount() == 0 &&
                addressChild->safeToDoRecursiveDecrement())
               {
               cg->recursivelyDecReferenceCount(addressChild);
               }
            else
               {
               cg->evaluate(addressChild);
               if (storageReference->getNodeReferenceCount() == 0)
                  {
                  if (cg->traceBCDCodeGen())
                     traceMsg(comp,"storageReference->nodeRefCount == 0 so dec addr child %p refCount %d->%d\n",
                        storageReference->getNode()->getFirstChild(),storageReference->getNode()->getFirstChild()->getReferenceCount(),storageReference->getNode()->getFirstChild()->getReferenceCount()-1);
                  cg->decReferenceCount(addressChild);
                  }
               }
            }
         }
      }
#endif

   return tempReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::fenceEvaluator(TR::Node * node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

/**
 * loadaddrEvaluator - load address of non-heap storage item (Auto, Parm, Static or Method).
 * Note that newInstancePrototype - a special function - loads up it's parm with
 * a loadaddr -but- the parameter is not a complete object yet. SO, it is important
 * that you do not mark a newInstancePrototype parm as 'collected'.
 */
TR::Register *
OMR::Z::TreeEvaluator::loadaddrEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::SymbolReference * symRef = node->getSymbolReference();
   TR::Compilation *comp = cg->comp();

   if (symRef->getSymbol()->isLocalObject())
      {
      targetRegister = cg->allocateCollectedReferenceRegister();
      }
   else
      {
      if (node->getSize() > 4)
         targetRegister = cg->allocateRegister();
      else
         targetRegister = cg->allocateRegister();
      }

   node->setRegister(targetRegister);

   if (symRef->isUnresolved())
      {
#ifdef J9_PROJECT_SPECIFIC
      TR::UnresolvedDataSnippet * uds = new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, symRef, false, false);
      cg->addSnippet(uds);

      // Generate branch to the unresolved data snippet
      TR::Instruction * cursor = generateRegUnresolvedSym(cg, TR::InstOpCode::getLoadOpCode(), node, targetRegister, symRef, uds);
      uds->setBranchInstruction(cursor);

      // Create a patchable data in litpool
      TR::S390WritableDataSnippet * litpool = cg->CreateWritableConstant(node);
      litpool->setUnresolvedDataSnippet(uds);

      TR::S390RILInstruction * lrlInstr = NULL;
      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
         {
         lrlInstr = static_cast<TR::S390RILInstruction *>(generateRILInstruction(cg, TR::InstOpCode::getLoadRelativeLongOpCode(), node, targetRegister, reinterpret_cast<void*>(0xBABE), 0));
         uds->setDataReferenceInstruction(lrlInstr);
         lrlInstr->setSymbolReference(uds->getDataSymbolReference());
         lrlInstr->setTargetSnippet(litpool);
         lrlInstr->setTargetSymbol(uds->getDataSymbol());
         }
      else
         {
         lrlInstr = static_cast<TR::S390RILInstruction *>(generateRILInstruction(cg, TR::InstOpCode::LARL, node, targetRegister, reinterpret_cast<void*>(0xBABE), 0));
         lrlInstr->setSymbolReference(uds->getDataSymbolReference());
         lrlInstr->setTargetSnippet(litpool);
         lrlInstr->setTargetSymbol(uds->getDataSymbol());

         cursor = generateRXInstruction(cg, TR::InstOpCode::LG, node, targetRegister,
                                generateS390MemoryReference(targetRegister, 0, cg));

         uds->setDataReferenceInstruction(cursor);
         }

      TR_ASSERT_FATAL(lrlInstr->getKind() == OMR::Instruction::Kind::IsRIL, "Unexpected load instruction format");

      TR_Debug * debugObj = cg->getDebug();
      if (debugObj)
         {
         debugObj->addInstructionComment(lrlInstr, "LoadLitPoolEntry");
         }
#endif
      }
   else
      {
      if (node->getSymbol()->isStatic())
         {
         TR::Instruction * cursor;

         // A static symbol may either contain a static address or require a register (i.e. DLT MetaData)
         if (comp->getOption(TR_EnableHCR) && node->getSymbol()->isMethod() && !cg->comp()->compileRelocatableCode()) // AOT Class Address are loaded via snippets already
            cursor = genLoadAddressConstantInSnippet(cg, node, (uintptr_t) node->getSymbol()->getStaticSymbol()->getStaticAddress(), targetRegister, NULL, NULL, NULL, true);
         else
            {
            // Generate Static Address into a register
            TR::StaticSymbol *sym =  node->getSymbol()->getStaticSymbol();
            if (sym && sym->isStartPC())
               {
               cursor = generateRILInstruction(cg, TR::InstOpCode::LARL, node, targetRegister, node->getSymbolReference(),
                                                reinterpret_cast<void*>(sym->getStaticAddress()),  NULL);
               }
            else if (comp->compileRelocatableCode())
               {
               int32_t reloType;
               if (node->getSymbol()->isDebugCounter())
                  reloType = TR_DebugCounter;
               else if (node->getSymbol()->isConst())
                  reloType = TR_ConstantPool;
               else if (node->getSymbol()->isClassObject())
                  reloType = TR_ClassAddress;
               else if (node->getSymbol()->isMethod())
                  reloType = TR_MethodObject;
               else if (sym && sym->isRecompilationCounter())
                  reloType = TR_BodyInfoAddress;
               else if (sym && sym->isCompiledMethod())
                  reloType = TR_RamMethod;
               else if (sym && sym->isStartPC())
                  reloType = TR_AbsoluteMethodAddress;
               else if (sym && sym->isStatic() && !sym->isNotDataAddress())
                  reloType = TR_DataAddress;
               else
                  reloType = 0;

               cursor = generateRegLitRefInstruction(cg, TR::InstOpCode::getLoadOpCode(), node, targetRegister,
                                                     (uintptr_t) node->getSymbol()->getStaticSymbol()->getStaticAddress(),
                                                     reloType, NULL, NULL, NULL);
               }
            else
               {
               cursor = genLoadAddressConstant(cg, node, (uintptr_t) node->getSymbol()->getStaticSymbol()->getStaticAddress(), targetRegister);
               }
            }
         }
      else if ((node->getSymbol()->isAuto()) || (node->getSymbol()->isParm()) || (node->getSymbol()->isShadow()) || (node->getSymbol()->isRegisterMappedSymbol()))
         {
         // We could dump a const in auto's and ref it from somewhere else.
         TR::MemoryReference * mref = generateS390MemoryReference(node, symRef, cg);
         TR::Node * treetopNode = cg->getCurrentEvaluationTreeTop()->getNode();
         generateRXInstruction(cg, TR::InstOpCode::LA, treetopNode, targetRegister, mref);

         mref->setBaseNode(node);
         mref->stopUsingMemRefRegister(cg);
         }
      else if (node->getSymbol()->isLabel() || node->getSymbol()->isMethod())
         {
         new (cg->trHeapMemory()) TR::S390RILInstruction(TR::InstOpCode::LARL, node, targetRegister, symRef->getSymbol(), symRef, cg);
         }
      else
         {
         // We probably have not yet implemented all possible cases here.
         TR_UNIMPLEMENTED();
         }
      }
   return targetRegister;
   }

TR::Register *
OMR::Z::TreeEvaluator::passThroughEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * targetRegister = NULL;
   TR::Compilation *comp = cg->comp();
   if ((node->getOpCodeValue() != TR::PassThrough && cg->useClobberEvaluate()))
      {
      targetRegister =  cg->gprClobberEvaluate(node->getFirstChild());
      }
   else
      {
      targetRegister = cg->evaluate(node->getFirstChild());
      }

   if (!targetRegister->getRegisterPair() &&
       node->getReferenceCount() > 1 &&
       node->getOpCodeValue() == TR::PassThrough &&
       node->getFirstChild()->getOpCode().isLoadReg() &&
       node->getGlobalRegisterNumber() != node->getFirstChild()->getGlobalRegisterNumber())
      {
      TR::Register * reg = targetRegister;
      TR::Register * copyReg = NULL;
      TR::InstOpCode::Mnemonic opCode;
      TR_RegisterKinds kind = reg->getKind();
      TR_Debug * debugObj = cg->getDebug();
      TR::Node * child = node->getFirstChild();

      TR::RealRegister::RegNum regNum = (TR::RealRegister::RegNum)
         cg->getGlobalRegister(child->getGlobalRegisterNumber());

      bool containsInternalPointer = false;
      if (reg->getPinningArrayPointer())
         {
         containsInternalPointer = true;
         }

      copyReg = (targetRegister->containsCollectedReference() && !containsInternalPointer) ?
      cg->allocateCollectedReferenceRegister() :
      cg->allocateRegister(kind);
      if (containsInternalPointer)
         {
         copyReg->setContainsInternalPointer();
         copyReg->setPinningArrayPointer(reg->getPinningArrayPointer());
         }

      switch (kind)
         {
         case TR_GPR:
            if (reg->is64BitReg())
               {
               opCode = TR::InstOpCode::LGR;
               }
            else
               {
               opCode = TR::InstOpCode::LR;
               }
            break;
         case TR_FPR:
            opCode = TR::InstOpCode::LDR;
            break;
         case TR_VRF:
            opCode = TR::InstOpCode::VLR;
            break;
         default:
            TR_ASSERT( 0, "Invalid register kind.");
         }

      TR::Instruction * iCursor = opCode == TR::InstOpCode::VLR ? generateVRRaInstruction(cg, opCode, node, copyReg, reg) :
                                                        generateRRInstruction  (cg, opCode, node, copyReg, reg);

      if (debugObj)
         {
         if (opCode == TR::InstOpCode::VLR)
            debugObj->addInstructionComment(toS390VRRaInstruction(iCursor), "VLR=Reg_deps");
         else
            debugObj->addInstructionComment(toS390RRInstruction(iCursor), "LR=Reg_deps");
         }

      targetRegister = copyReg;
      }

   node->setRegister(targetRegister);
   cg->decReferenceCount(node->getFirstChild());

   return targetRegister;
   }

/**
 * BBStartEvaluator - Start of Basic Block
 */
TR::Register *
OMR::Z::TreeEvaluator::BBStartEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Instruction * firstInstr = NULL;
   TR::Block * block = node->getBlock();
   cg->setCurrentBlockIndex(block->getNumber());
   TR::Compilation *comp = cg->comp();

   bool generateFence = true;

   TR::RegisterDependencyConditions * deps = NULL;

   if (node->getLabel() != NULL
      )
      {
      TR::Instruction * labelInstr = NULL;
      cg->setHasCCInfo(false);

      bool unneededLabel = cg->supportsUnneededLabelRemoval()? block->doesNotNeedLabelAtStart() : false;

      if (!unneededLabel)
         {
         labelInstr = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, node->getLabel());
         if (!firstInstr)
            firstInstr = labelInstr;
      // BB_Start has a label ==> We have an unique block number.
         labelInstr->setBlockIndex(node->getBlock()->getNumber());
         }
      // BB_Start has a label ==> We have an unique block number.
      // Decrement Next Available Block Index since we didn't need to use the one automatically
      // assigned to the above TR::InstOpCode::LABEL.

      //keeping proper track of block indexes by codegen, can comment out following
      //cg->setNextAvailableBlockIndex(cg->getNextAvailableBlockIndex() - 1);
      // Update current block index with BB_Start's block number.

      //keeping proper track of block indexes by codegen, can comment out following
      //cg->setCurrentBlockIndex(node->getBlock()->getNumber());
      node->getLabel()->setInstruction(labelInstr);
      }


   cg->setCurrentBlock(block);

   if (!block->isExtensionOfPreviousBlock() && node->getNumChildren() > 0)
      {
      int32_t i;
      TR::Node * child = node->getFirstChild();
      TR::Machine *machine = cg->machine();
      machine->clearRegisterAssociations();

      cg->evaluate(child);

      // REG ASSOC
      machine->setRegisterWeightsFromAssociations();

      // GRA
      deps = generateRegisterDependencyConditions(cg, child, 0);

      if (cg->getCurrentEvaluationTreeTop() == comp->getStartTree())
         {
         for (i = 0; i < child->getNumChildren(); i++)
            {
            TR::ParameterSymbol * sym = child->getChild(i)->getSymbol()->getParmSymbol();
            if (sym != NULL)
               {
               TR::DataType dt = sym->getDataType();
               TR::DataType type = dt;

               if (
#ifdef J9_PROJECT_SPECIFIC
                   !type.isLongDouble() &&
#endif
                   true)
                  {
                  sym->setAssignedGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getGlobalRegisterNumber()));
                  }
               else
                  {
                  sym->setAssignedHighGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getHighGlobalRegisterNumber()));
                  sym->setAssignedLowGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getLowGlobalRegisterNumber()));
                  }
               }
            }
         }

      TR::Instruction * cursor = generateS390PseudoInstruction(cg, TR::InstOpCode::DEPEND, node, deps);
      if (!firstInstr)
        firstInstr = cursor;

      cg->decReferenceCount(child);
      }

   TR::Instruction * fence = NULL;

   if (!generateFence)
      {
      // Even though we dont generate fences for every BB, catch blocks require it.
      if (block->isCatchBlock()) fence = generateS390PseudoInstruction(cg, TR::InstOpCode::FENCE, node, (TR::Node *) NULL);
      }
   else
      {
      fence = generateS390PseudoInstruction(cg, TR::InstOpCode::FENCE, node,
                TR::Node::createRelative32BitFenceNode(node, &block->getInstructionBoundaries()._startPC));

      // Save the first instruction of the block.
      block->setFirstInstruction((firstInstr != NULL)?firstInstr:fence);
      }
   if (!firstInstr)
      firstInstr = fence;

   if (block->isCatchBlock())
      {
      cg->generateCatchBlockBBStartPrologue(node, fence);
      }

   return NULL;
   }

/**
 * BBEndEvaluator - End of Basic Block
 */
TR::Register *
OMR::Z::TreeEvaluator::BBEndEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Block * block = node->getBlock();
   TR::Instruction* lastInstr = NULL;

   TR::Block *nextBlock = block->getNextBlock();

   TR::TreeTop * nextTT = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   TR::RegisterDependencyConditions * deps = NULL;

   lastInstr = generateS390PseudoInstruction(cg, TR::InstOpCode::FENCE, node, deps,
   TR::Node::createRelative32BitFenceNode(node, &block->getInstructionBoundaries()._endPC));

   if (!nextTT || !nextTT->getNode()->getBlock()->isExtensionOfPreviousBlock())
      {
      if (cg->enableRegisterAssociations() &&
          cg->getAppendInstruction()->getOpCodeValue() != TR::InstOpCode::ASSOCREGS)
         {
         cg->machine()->createRegisterAssociationDirective(cg->getAppendInstruction());
         }

      if (node->getNumChildren() > 0)
         {
         TR::Node * child = node->getFirstChild();
         cg->evaluate(child);

         // GRA
         lastInstr =  generateS390PseudoInstruction(cg, TR::InstOpCode::DEPEND, node, generateRegisterDependencyConditions(cg, child, 0));

         cg->decReferenceCount(child);
         }
      }

   // Save the last TR::Instruction for this block.
   block->setLastInstruction(lastInstr);

   return NULL;
   }

#define ClobberRegisterForLoops(evaluateChildren,op,baseAddr,baseReg) \
         { if (evaluateChildren)                                      \
            {                                                         \
            if (op.needsLoop())                                       \
               baseReg = cg->gprClobberEvaluate(baseAddr);    \
            else                                                      \
               baseReg = cg->evaluate(baseAddr);              \
            }                                                         \
         }

#define AlwaysClobberRegisterForLoops(evaluateChildren,baseAddr,baseReg) \
         { if (evaluateChildren)                                      \
            {                                                         \
            baseReg = cg->gprClobberEvaluate(baseAddr);               \
            }                                                         \
         }

///////////////////////////////////////////////////////////////////////////////////////

TR::Register *
OMR::Z::TreeEvaluator::arraycmpEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();

   if (TR::isJ9() && !comp->getOption(TR_DisableSIMDArrayCompare) && cg->getSupportsVectorRegisters())
      {
      // An empirical study has showed that CLC is faster for all array sizes if the number of bytes to copy is known to be constant
      if (!node->getChild(2)->getOpCode().isLoadConst())
         return TR::TreeEvaluator::arraycmpSIMDHelper(node, cg, NULL, NULL, true, !node->isArrayCmpSign()/*return102*/);
      }

   TR::Node * firstBaseAddr = node->getFirstChild();
   TR::Node * secondBaseAddr = node->getSecondChild();
   TR::Node * elemsExpr = node->getChild(2);

   TR::Register * firstBaseReg = NULL;
   TR::Register * secondBaseReg = NULL;
   bool lenMinusOne=false;
   TR_ASSERT(!node->isArrayCmpLen() || !node->isArrayCmpSign(), "Invalid arraycmp node");
   if (!node->isArrayCmpLen())
      {
      // use CLC
      TR::Register * resultReg;

      if (elemsExpr->getOpCode().isLoadConst())
         {
         //TR_ASSERTC( elemsExpr->getSize() <=4,comp, "If this is not true why are we truncating?");
         // remove the assertion for now
         int32_t elems = (int32_t) getIntegralValue(elemsExpr); //get number of elements (in bytes)
         bool    clobber = (comp->getOption(TR_DisableSSOpts) || elems>256 || elems==0 || node->isArrayCmpSign());
         if (!node->isArrayCmpSign())
            {
            resultReg = TR::TreeEvaluator::arraycmpHelper(
                            node,
                            cg,
                            false, //isWideChar
                            true,  //isEqualCmp
                            0,     //cmpValue. It means it is equivalent to do "arraycmp(A,B) == 0"
                            NULL,  //compareTarget
                            NULL,  //ificmpNode
                            true,  //needResultReg
                            true); //return102
            // node->setRegister(resultReg);
            return resultReg;
            }
         else
            {
            MemCmpConstLenSignMacroOp op(node, firstBaseAddr, secondBaseAddr, cg, elems);
            ClobberRegisterForLoops(clobber,op,firstBaseAddr,firstBaseReg);
            ClobberRegisterForLoops(clobber,op,secondBaseAddr,secondBaseReg);
            op.generate(firstBaseReg, secondBaseReg);
            resultReg = op.resultReg();
            }
         }
      else
         {
         TR::Register * elemsReg;

         if (!node->isArrayCmpSign())
            {
            resultReg = TR::TreeEvaluator::arraycmpHelper(
                          node,
                          cg,
                          false, //isWideChar
                          true,  //isEqualCmp
                          0,     //cmpValue. It means it is equivalent to do "arraycmp(A,B) == 0"
                          NULL,  //compareTarget
                          NULL,  //ificmpNode
                          true,  //needResultReg
                          true); //return102

            // node->setRegister(resultReg);
            return resultReg;
            }
         else
            {
            elemsReg = cg->evaluateLengthMinusOneForMemoryOps(elemsExpr, true, lenMinusOne);
            firstBaseReg = cg->gprClobberEvaluate(firstBaseAddr);
            secondBaseReg = cg->gprClobberEvaluate(secondBaseAddr);

            MemCmpVarLenSignMacroOp op(node, firstBaseAddr, secondBaseAddr, cg, elemsReg, elemsExpr);
            op.generate(firstBaseReg, secondBaseReg);
            resultReg = op.resultReg();
            cg->stopUsingRegister(elemsReg);
            }
         }

      cg->decReferenceCount(elemsExpr);
      if (firstBaseReg!=NULL) cg->decReferenceCount(firstBaseAddr);
      if (secondBaseReg!=NULL) cg->decReferenceCount(secondBaseAddr);

      if (firstBaseReg!=NULL) cg->stopUsingRegister(firstBaseReg);
      if (secondBaseReg!=NULL) cg->stopUsingRegister(secondBaseReg);


      TR_ASSERT( resultReg!=firstBaseReg && resultReg!=secondBaseReg, "arraycmpEvaluator -- result reg should be a new reg\n");

      node->setRegister(resultReg);
      return resultReg;
      }
   else
      {
      // use CLCL instruction
      firstBaseReg = cg->gprClobberEvaluate(firstBaseAddr);
      secondBaseReg = cg->gprClobberEvaluate(secondBaseAddr);

      TR::Register * orgLen = cg->gprClobberEvaluate(elemsExpr);
      TR::Register * firstLen = cg->allocateRegister();
      TR::Register * secondLen = cg->allocateRegister();
      TR::RegisterPair * firstPair = cg->allocateConsecutiveRegisterPair(firstLen, firstBaseReg);
      TR::RegisterPair * secondPair = cg->allocateConsecutiveRegisterPair(secondLen, secondBaseReg);
      TR::Register * resultReg = cg->allocateRegister();
      TR::Instruction * cursor;

      TR::RegisterDependencyConditions * dependencies = cg->createDepsForRRMemoryInstructions(node, firstPair, secondPair);

      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, firstLen, orgLen);
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, secondLen, orgLen);
      cursor = generateRRInstruction(cg, TR::InstOpCode::CLCL, node, firstPair, secondPair);

      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, resultReg, orgLen);
      cursor = generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, resultReg, firstLen);


      cg->stopUsingRegister(firstPair);
      cg->stopUsingRegister(secondPair);
      cg->stopUsingRegister(firstBaseReg);
      cg->stopUsingRegister(secondBaseReg);
      cg->stopUsingRegister(firstLen);
      cg->stopUsingRegister(secondLen);
      cg->stopUsingRegister(orgLen);


      cg->decReferenceCount(elemsExpr);
      cg->decReferenceCount(firstBaseAddr);
      cg->decReferenceCount(secondBaseAddr);
      cursor->setDependencyConditions(dependencies);


      node->setRegister(resultReg);
      return resultReg;

      }
   }

TR::Register *
OMR::Z::TreeEvaluator::arraycmpEvaluatorWithPad(TR::Node * node, TR::CodeGenerator * cg)
   {
   arraycmpWithPadHelper helper(node,
                                cg,
                                false,  //isUTF16
                                false,  //isFoldedIf
                                NULL,   //ificmpNode
                                true);  //needResultReg
   return helper.generate();
   }


#define TRTSIZE 256
TR::Register *
OMR::Z::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Instruction * cursor;
   if (!node->isArrayTRT())
      {
      // not findBytes
      // tree looks as follows:
      // arraytranslateAndTest
      //    input node
      //    stop byte
      //    input length (in bytes)
      //
      // Right now, this is only 1 style of tree - just for a simple 'test'
      //  that consists of only one byte
      // This type of check is mapped to a loop of SRST. The intent is that this
      //  will also support a table of bytes to stop on that can be mapped to
      //  translate-and-test instruction (TRT) which maps to TRT.
      //
      TR::Register * inputReg = cg->gprClobberEvaluate(node->getChild(0));
      TR::Register * stopCharReg = cg->gprClobberEvaluate(node->getChild(1));
      TR::Register * endReg = cg->gprClobberEvaluate(node->getChild(2));

      TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);
      dependencies->addPostCondition(stopCharReg, TR::RealRegister::GPR0);
      dependencies->addPostCondition(inputReg, TR::RealRegister::AssignAny);
      dependencies->addPostCondition(endReg, TR::RealRegister::AssignAny);

      generateRILInstruction(cg, TR::InstOpCode::NILF, node, stopCharReg, 0x000000FF);

      TR::InstOpCode::Mnemonic opCode;
      TR::Register * startReg = cg->allocateRegister();

      TR::MemoryReference * startMR = generateS390MemoryReference(inputReg, 0, cg);
      TR::MemoryReference * endMR = generateS390MemoryReference(inputReg, endReg, 0, cg);
      generateRXInstruction(cg, TR::InstOpCode::LA, node, endReg, endMR);
      generateRXInstruction(cg, TR::InstOpCode::LA, node, startReg, startMR);

      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
      TR::LabelSymbol * topOfLoop = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, topOfLoop, dependencies);
      topOfLoop->setStartInternalControlFlow();

      generateRRInstruction(cg, TR::InstOpCode::SRST, node, endReg, inputReg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, topOfLoop); // repeat if CPU-defined limit hit
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
      cFlowRegionEnd->setEndInternalControlFlow();

      generateRRInstruction(cg, TR::InstOpCode::SR, node, endReg, startReg);


      node->setRegister(endReg);
      cg->stopUsingRegister(startReg);
      cg->stopUsingRegister(inputReg);
      cg->stopUsingRegister(stopCharReg);
      startMR->stopUsingMemRefRegister(cg);
      endMR->stopUsingMemRefRegister(cg);

      cg->decReferenceCount(node->getChild(0));
      cg->decReferenceCount(node->getChild(1));
      cg->decReferenceCount(node->getChild(2));

      return endReg;
      }
   else // Use the new loopReducer - CISC Transfomer.
      {
      // CISCTransformer generates arrayTranslateAndTest for both TRT (translate and test) and SRST (search string)
      //
      // For TRT, the IL tree is:
      //    arraytranslateAndTest
      //       input array base
      //       initial array index
      //       lookup table base (used by TRT) (TR::Address)
      //       array length
      //       search length ( optional )   Default search length = array length
      //
      // For SRST / SRSTU, the IL tree is:
      //    arraytranslateAndTest
      //       input array base
      //       initial array index
      //       search character to look up (iconst)
      //       array length
      //       search length ( optional )   Default search length = array length
      //
      //  With optional search length, we still need to maintain array length to perform bounds check.

      TR::Register * baseReg = cg->evaluate(node->getChild(0));
      TR::Register * indexReg = cg->gprClobberEvaluate(node->getChild(1));
      TR::Node * tableNode = node->getChild(2);
      TR::Register * alenReg = cg->gprClobberEvaluate(node->getChild(3));

      bool isUsingTRT = (tableNode->getDataType() == TR::Address);

      // Use clobberEvalute for SRST and SRSTU
      // because we need to mask out the GPR0 (tableNode contains the search character)
      TR::Register * tableReg = NULL;
      if (isUsingTRT)
         tableReg = cg->evaluate(tableNode);
      else
         tableReg = cg->gprClobberEvaluate(tableNode);

      // 5th child, search length, is optional.  Default is array length.
      bool isAdditionalLen = (node->getNumChildren() == 5);
      TR::Register * lenReg = (isAdditionalLen)?cg->gprClobberEvaluate(node->getChild(4)):alenReg;

      // minReg contains the actual range to translate/search: min (array length , optional search length).
      TR::Register * minReg = (isAdditionalLen)?cg->allocateRegister():alenReg;

      bool elementChar = node->isCharArrayTRT();
      bool emulateSRSTUusingSRST = false;

      int32_t numRegister = 7;

      if (isUsingTRT || emulateSRSTUusingSRST)
         numRegister++;

      if (isAdditionalLen)
         numRegister+=2;

      TR::RegisterDependencyConditions * dependencies;
      TR::Register * ptrReg = cg->allocateRegister();
      TR::Register * tmpReg = cg->allocateRegister();
      TR::LabelSymbol * label256Top = generateLabelSymbol(cg);
      TR::LabelSymbol * labelFind = generateLabelSymbol(cg);
      TR::LabelSymbol * labelEnd = generateLabelSymbol(cg);
      TR::LabelSymbol * boundCheckFailureLabel = generateLabelSymbol(cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
      dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numRegister, cg);

      // For chars, scale the index and array/search length by 2 (size of char) to convert into bytes.
      if (elementChar)
         {
         if (cg->comp()->target().is64Bit())
            {
            generateRSInstruction(cg, TR::InstOpCode::SLLG, node, indexReg, indexReg, 1);
            generateRSInstruction(cg, TR::InstOpCode::SLLG, node, alenReg, alenReg, 1);
            if (isAdditionalLen)
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, lenReg, lenReg, 1);
            }
         else
            {
            generateRSInstruction(cg, TR::InstOpCode::SLL, node, indexReg, 1);
            generateRSInstruction(cg, TR::InstOpCode::SLL, node, alenReg, 1);
            if (isAdditionalLen)
               generateRSInstruction(cg, TR::InstOpCode::SLL, node, lenReg, 1);
            }
         }

      // If an optional translate/search length was specified, determine the minimum
      // length we need to search by taking the smaller of the two values - array length
      // and translate/search length.
      if (isAdditionalLen)
         {
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
         TR::LabelSymbol * labelSkip = generateLabelSymbol(cg);

         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, minReg, alenReg);
         generateRRInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, minReg, lenReg);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNH, node, labelSkip);
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, minReg, lenReg);
         TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
         deps->addPostCondition(minReg, TR::RealRegister::AssignAny);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelSkip, deps);
         labelSkip->setEndInternalControlFlow();

         dependencies->addPostCondition(lenReg, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(minReg, TR::RealRegister::AssignAny);
         }

      // Memory reference pointing to beginning of source array: index + base + arrayHeaderSize.
      TR::MemoryReference * ptrMR = generateS390MemoryReference(baseReg, indexReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
      dependencies->addPostCondition(baseReg, TR::RealRegister::AssignAny);
      dependencies->addPostCondition(indexReg, TR::RealRegister::AssignAny);
      dependencies->addPostCondition(alenReg, TR::RealRegister::AssignAny);

      if (isUsingTRT)
         {
         dependencies->addPostCondition(tableReg, TR::RealRegister::GPR3);  // For helper
         dependencies->addPostCondition(ptrReg, TR::RealRegister::GPR1);  // GPR1 is used/clobbered in TRT
         dependencies->addPostCondition(tmpReg, TR::RealRegister::GPR2); // GPR2 is implicitly clobbered in TRT

         TR::Register * raReg = cg->allocateRegister();
         dependencies->addPostCondition(raReg, cg->getReturnAddressRegister());

         // At first, load up the branch address into raReg, because
         // to ensure that no weird spilling happens if the code decides it needs
         // to allocate a register at this point for the literal pool base address.
         intptr_t helper = (intptr_t) cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390arrayTranslateAndTestHelper, false, false, false)->getMethodAddress();

         TR::LabelSymbol * labelEntryElementChar = generateLabelSymbol(cg);
         TR::Instruction * cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEntryElementChar);

         genLoadAddressConstant(cg, node, helper, raReg, cursor, dependencies);

         // Generate internal pointer to beginning of source array :  index + base + arrayHeaderSize.
         generateRXInstruction(cg, TR::InstOpCode::LA, node, ptrReg, ptrMR);
         // Increment index by 256 bytes.
         generateRIInstruction(cg, TR::InstOpCode::AHI, node, indexReg, TRTSIZE);

         // Label for the helper code to handle TRT < 256 bytes.
         TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
         TR::MemoryReference * inputTRTMR = generateS390MemoryReference(ptrReg, 0, cg);
         TR::MemoryReference * tableMR = generateS390MemoryReference(tableReg, 0, cg);


         // TRT loops can only perform 256 bytes blocks.  Any blocks < 256 bytes will
         // go to helper table.  CLR checks if current index + 256 (indexReg) < length (minReg)
         cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CLR, node, indexReg, minReg, TR::InstOpCode::COND_BHR, cFlowRegionEnd);

         //
         // Beginning of TRT 256 bytes loop
         //

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label256Top);
         label256Top->setStartInternalControlFlow();
         // Perform the TRT - GPR1 contains the address of the last translated unit.
         // If TRT terminates early, we have found the test character.
         generateSS1Instruction(cg, TR::InstOpCode::TRT, node, TRTSIZE-1, inputTRTMR, tableMR);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNZ, node, labelFind);     // Found test char!

         // Increment internal pointer by 256 bytes.
         generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, ptrReg, TRTSIZE);

         // Increment index by 256 byte
         generateRIInstruction(cg, TR::InstOpCode::AHI, node, indexReg, TRTSIZE);

         generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CLR, node, indexReg, minReg, TR::InstOpCode::COND_BNHR, label256Top);

         TR::RegisterDependencyConditions *loopDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
         loopDeps->addPostCondition(indexReg, TR::RealRegister::AssignAny);
         loopDeps->addPostCondition(minReg, TR::RealRegister::AssignAny);
         loopDeps->addPostCondition(ptrReg, TR::RealRegister::AssignAny);
         loopDeps->addPostCondition(tableReg, TR::RealRegister::AssignAny);

         //
         // Helper code to handle TRT < 256 bytes.
         //
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, loopDeps);
         cFlowRegionEnd->setEndInternalControlFlow();
         // Calculate the remaining bytes left to translate:
         //    remaining bytes =  length - indexReg - 256
         //                 (indexReg is 256 bytes beyond real index)
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, tmpReg, minReg);
         generateRRInstruction(cg, TR::InstOpCode::SR, node, tmpReg, indexReg);
         generateRIInstruction(cg, TR::InstOpCode::AHI, node, tmpReg, TRTSIZE);

         // Index into helper table - Each entry in table is 16 bytes.
         //   Actual index to helper table is the remaining length (8-byte) * 16.
         if (cg->comp()->target().is64Bit())
            generateShiftThenKeepSelected64Bit(node, cg, tmpReg, tmpReg, 52, 59, 4);
         else
            generateShiftThenKeepSelected31Bit(node, cg, tmpReg, tmpReg, 20, 27, 4);

         // Helper table address is stored in raReg.  Branch to helper to execute TRT and return.
         TR::MemoryReference * targetMR = new (cg->trHeapMemory()) TR::MemoryReference(raReg, tmpReg, 0, cg);
         generateRXInstruction(cg, TR::InstOpCode::BAS, node, raReg, targetMR);

         cg->stopUsingRegister(raReg);
         cg->stopUsingRegister(tableReg);

         //
         // TRT Helper Call does not find the target characters.
         //
         // Bounds Checking

         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();

         if (isAdditionalLen)
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNZ, node, labelFind);  // Find !!
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, indexReg, minReg);
            cg->stopUsingRegister(minReg);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CLR, node, indexReg, alenReg, TR::InstOpCode::COND_BLR, labelEnd);

            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CLR, node, lenReg, alenReg, TR::InstOpCode::COND_BNER, boundCheckFailureLabel);
            cg->stopUsingRegister(lenReg);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelEnd);
            }
         else
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, boundCheckFailureLabel); // not find !!
            }
         cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, boundCheckFailureLabel,
                                                     comp->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp->getMethodSymbol())));
         cg->stopUsingRegister(alenReg);

         //
         // Found a test character
         //
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelFind);

         // Compute the actual index.
         //   Last translated character pointer - base register of array - array header size.
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, indexReg, ptrReg);
         generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, indexReg, baseReg);
         generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, indexReg, -((int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes()));

         if (elementChar)
            {
            TR::LabelSymbol * label2ByteOk = generateLabelSymbol(cg);
            TR::MemoryReference * ptr0MR = generateS390MemoryReference(ptrReg, 0, 0, cg);

            generateRIInstruction(cg, TR::InstOpCode::NILL, node, ptrReg, 0xFFFE);
            generateRIInstruction(cg, TR::InstOpCode::NILL, node, indexReg, 0xFFFE);
            generateRXInstruction(cg, TR::InstOpCode::LLGH, node, tmpReg, ptr0MR);

            generateRIInstruction(cg, TR::InstOpCode::CHI, node, tmpReg, 256);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, label2ByteOk);
            generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, ptrReg, 2);
            generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, indexReg, 2);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelEntryElementChar);
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label2ByteOk);
            ptr0MR->stopUsingMemRefRegister(cg);
            }

         cg->stopUsingRegister(ptrReg);
         cg->stopUsingRegister(baseReg);

         inputTRTMR->stopUsingMemRefRegister(cg);
         tableMR->stopUsingMemRefRegister(cg);
         targetMR->stopUsingMemRefRegister(cg);
         }
      else
         {
         // Using SRST (byte) or SRSTU (2-byte)

         // SRST/SRSTU R1, R2 expects:  ('character' = byte for SRST and two-bytes for SRSTU)
         //    GPR0         - The character for which the search occurs.
         //    R1 (tmpReg)  - The location of the character after the end of search range.
         //    R2 (ptrReg)  - The location of the first character to search.
         //
         // SRST/SRSTU can terminate with 3 scenarios:
         //    CC 1:  character specified in GPR0 is found, address of character found is set in R1
         //    CC 2:  address of next character == address in R1.  End of search.
         //    CC 3:  CPU defined limit (at least 256 characters searched).  R2 is updated to point to next character.
         dependencies->addPostCondition(tableReg, TR::RealRegister::GPR0);
         dependencies->addPostCondition(tmpReg, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(ptrReg, TR::RealRegister::AssignAny);
         TR::Register *endReg = NULL;

         // SRSTU is only available on z9.
         bool useSRSTU = (elementChar && !emulateSRSTUusingSRST);

         // Need to mask out bits 32-55 in GPR0 when using SRST instruction and bits 32-47 in GPR0 when using SRSTU
         if (useSRSTU)
            {
            generateRILInstruction(cg, TR::InstOpCode::NILF, node, tableReg, 0x0000FFFF);
            }
         else
            {
            generateRILInstruction(cg, TR::InstOpCode::NILF, node, tableReg, 0x000000FF);
            }

         // Pointer to the end of search range : base + length + array header size
         TR::MemoryReference * endMR = generateS390MemoryReference(baseReg, minReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
         // Generate internal pointer to end of search range.
         generateRXInstruction(cg, TR::InstOpCode::LA, node, tmpReg, endMR);

         // Generate internal pointer to beginning of source array :  index + base + arrayHeaderSize.
         generateRXInstruction(cg, TR::InstOpCode::LA, node, ptrReg, ptrMR);

         // If we are using SRST to search 2-byte arrays, save the end of search range
         // in case of false match.
         if (emulateSRSTUusingSRST)
            {
            endReg = cg->allocateRegister();
            dependencies->addPostCondition(endReg, TR::RealRegister::AssignAny);
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, endReg, tmpReg);
            }

         // SRST / SRSTU loop
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label256Top);
         label256Top->setStartInternalControlFlow();
         generateRRInstruction(cg, (useSRSTU) ? TR::InstOpCode::SRSTU : TR::InstOpCode::SRST, node, tmpReg, ptrReg);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, label256Top); // repeat if CPU-defined limit hit

         // Bounds Checking
         //     - either we found character, or we reached end of search range.
         if (isAdditionalLen)
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BM, node, labelFind);   // if the searching byte is found
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, indexReg, minReg);
            cg->stopUsingRegister(minReg);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CLR, node, indexReg, alenReg, TR::InstOpCode::COND_BLR, labelEnd);
            generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CLR, node, lenReg, alenReg, TR::InstOpCode::COND_BNER, boundCheckFailureLabel);
            cg->stopUsingRegister(lenReg);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, labelEnd);
            }
         else
            {
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BP, node, boundCheckFailureLabel);   // if the searching byte is NOT found
            }
         cg->addSnippet(new (cg->trHeapMemory()) TR::S390HelperCallSnippet(cg, node, boundCheckFailureLabel,
                                                     comp->getSymRefTab()->findOrCreateArrayBoundsCheckSymbolRef(comp->getMethodSymbol())));
         cg->stopUsingRegister(alenReg);

         //
         // Search character was found.
         //
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelFind);

         // If we are using SRST to search 2-byte array, we have to check
         // whether we have a real match.  We could potentially get a false positive
         // if we match the top byte of a 2-byte array.  i.e. delimiter = 0xAA, SRST
         // will match the character 0xFFFF 00AA.  Solution is to load the 2-byte value
         // and compare to make sure it's within 1-255 range.
         if (emulateSRSTUusingSRST)
            {
            TR::MemoryReference * ptr0MR = generateS390MemoryReference(tmpReg, 0, 0, cg);
            TR::LabelSymbol * label2ByteOk = generateLabelSymbol(cg);

            // Align the pointer to nearest halfword.
            generateRIInstruction(cg, TR::InstOpCode::NILL, node, tmpReg, 0xFFFE);
            // Load the halfword
            generateRXInstruction(cg, TR::InstOpCode::LLGH, node, ptrReg, ptr0MR);

            // If halfword value is < 256, we are okay.
            generateRIInstruction(cg, TR::InstOpCode::CHI, node, ptrReg, 256);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BL, node, label2ByteOk);

            // If haflword value >= 256, we got the wrong value, loop and try again.
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, ptrReg, tmpReg);
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, tmpReg, endReg);
            generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, ptrReg, 2);
            generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, label256Top);
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, label2ByteOk);
            }
         generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, tmpReg, baseReg);
         generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, tmpReg, -((int32_t)TR::Compiler->om.contiguousArrayHeaderSizeInBytes()));
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, indexReg, tmpReg);

         cg->stopUsingRegister(tableReg);
         cg->stopUsingRegister(ptrReg);
         cg->stopUsingRegister(baseReg);
         if (endReg != NULL)
            cg->stopUsingRegister(endReg);
         endMR->stopUsingMemRefRegister(cg);
         }
      //
      // End
      //
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelEnd);

      if (elementChar)
         {
         // Scale the result index from bytes to char.
         if (cg->comp()->target().is64Bit())
            {
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, indexReg, indexReg, 1);
            }
         else
            {
            generateRSInstruction(cg, TR::InstOpCode::SRL, node, indexReg, 1);
            }
         }

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
      cFlowRegionEnd->setEndInternalControlFlow();

      node->setRegister(indexReg);
      cg->stopUsingRegister(tmpReg);
      cg->stopUsingRegister(indexReg);
      ptrMR->stopUsingMemRefRegister(cg);

      cg->decReferenceCount(node->getChild(0));
      cg->decReferenceCount(node->getChild(1));
      cg->decReferenceCount(tableNode);
      cg->decReferenceCount(node->getChild(3));
      if (isAdditionalLen)
         cg->decReferenceCount(node->getChild(4));
      return indexReg;
      }
   }
#undef TRTSIZE

TR::Register *
OMR::Z::TreeEvaluator::arraytranslateEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   //
   // tree looks as follows:
   // arraytranslate
   //    input ptr
   //    output ptr
   //    translation table
   //    termCharNode i.e stop character for table based methods
   //    input length (in elements)
   //    stoppingNode for SIMD/non-table based methods. Can take on values from -1 to 0xFFFF
   // Number of elements translated is returned
   //
   TR::Compilation *comp = cg->comp();
   if (!comp->getOption(TR_DisableSIMDArrayTranslate) && cg->getSupportsVectorRegisters() &&  node->getChild(5)->get32bitIntegralValue() != -1)
     {
     if (node->isCharToByteTranslate()) return OMR::Z::TreeEvaluator::arraytranslateEncodeSIMDEvaluator(node, cg, CharToByte);
     if (node->isByteToCharTranslate()) return OMR::Z::TreeEvaluator::arraytranslateDecodeSIMDEvaluator(node, cg, ByteToChar);
     }

   TR::Register * inputReg = cg->gprClobberEvaluate(node->getChild(0));
   TR::Register * outputReg = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register * tableReg = cg->evaluate(node->getChild(2));
   TR::Register * termCharReg = cg->gprClobberEvaluate(node->getChild(3));
   TR::Register * resultReg = cg->allocateRegister();

   // We can special case a constant length.
   TR::Node *inputLengthNode = node->getChild(4);
   bool isLengthConstant = inputLengthNode->getOpCode().isLoadConst();
   TR::Register * inputLenReg = NULL;

   if (isLengthConstant)
      {
      inputLenReg = cg->allocateRegister();
      int64_t length = getIntegralValue(inputLengthNode);
      if (node->isCharToByteTranslate() || node->isCharToCharTranslate())
         generateLoad32BitConstant(cg, inputLengthNode, length * 2, inputLenReg, true);
      else
         generateLoad32BitConstant(cg, inputLengthNode, length, inputLenReg, true);
      generateLoad32BitConstant(cg, inputLengthNode, length, resultReg, true);
      }
   else
      {
      inputLenReg = cg->gprClobberEvaluate(inputLengthNode);
      }

   TR::RegisterPair * outputPair = cg->allocateConsecutiveRegisterPair(inputLenReg, outputReg);

   // If it's constant, the result and outLenReg are already initialized properly.
   if (!isLengthConstant)
      {
      // We need to copy the final length to resultReg, so that we can calculate
      // number of elements translated after TRXX.
      if (cg->comp()->target().is64Bit() && !(inputLengthNode->getOpCode().isLong()))
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, resultReg, inputLenReg);
      else
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, resultReg, inputLenReg);
      }

   TR::InstOpCode::Mnemonic opCode;
   if (node->isByteToByteTranslate() || node->isByteToCharTranslate())
      {
      opCode = (node->isByteToByteTranslate())?TR::InstOpCode::TROO:TR::InstOpCode::TROT;
      if (!isLengthConstant && cg->comp()->target().is64Bit() && !(inputLengthNode->getOpCode().isLong())) // need to ensure this value is sign extended as well
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, inputLenReg, resultReg);
      }
   else
      {
      TR_ASSERT( (node->isCharToByteTranslate() || node->isCharToCharTranslate()), "array translation type is unexpected");
      opCode = (node->isCharToByteTranslate())?TR::InstOpCode::TRTO:TR::InstOpCode::TRTT;

      if (!isLengthConstant)
         {
         if (cg->comp()->target().is64Bit())
            {
            if (!(inputLengthNode->getOpCode().isLong()))
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, inputLenReg, resultReg, 1);  // resultReg is sign extended above.
            else
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, inputLenReg, inputLenReg, 1);  // outLen is already 64-bit.. use it instead for better dependencies.
            }
         else
            {
            generateRSInstruction(cg, TR::InstOpCode::SLL, node, inputLenReg, 1);
            }
         }
      }

   if (!node->getTableBackedByRawStorage())
      {
      TR::Register* tableStorageReg = cg->allocateRegister();
      TR::MemoryReference * tableMR = generateS390MemoryReference(tableReg, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg);
      generateRXInstruction(cg, TR::InstOpCode::LA, node, tableStorageReg, tableMR);
      tableMR->stopUsingMemRefRegister(cg);

      tableReg = tableStorageReg;
      }

   TR::LabelSymbol * topOfLoop = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, topOfLoop);
   topOfLoop->setStartInternalControlFlow();


   new (cg->trHeapMemory()) TR::S390TranslateInstruction(opCode, node, outputPair, inputReg, tableReg, termCharReg, 0, cg, node->getTermCharNodeIsHint() && (opCode != TR::InstOpCode::TRTO) ? 1 : 0 );

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BO, node, topOfLoop); // repeat if CPU-defined limit hit
   // LL: Only have to do this if not Golden Eagle when termination character is a guess.

   if (node->getTermCharNodeIsHint() && (opCode == TR::InstOpCode::TRTO))
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, cFlowRegionEnd); // skip after loop if all chars processed
      if (node->getSourceCellIsTermChar())
         {
         TR_ASSERT( opCode == TR::InstOpCode::TRTO, "translation type is not TRTO");
         TR::MemoryReference * inMR = generateS390MemoryReference(inputReg, 0, cg);
         generateRXInstruction(cg, TR::InstOpCode::CH, node, termCharReg, inMR);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, cFlowRegionEnd); // skip after loop if the source cell is not equal to stopChar
         inMR->stopUsingMemRefRegister(cg);
         }
      TR::MemoryReference * outMR = generateS390MemoryReference(outputReg, 0, cg);
      TR::MemoryReference * nextAdr;

      if (opCode == TR::InstOpCode::TROT || opCode == TR::InstOpCode::TRTT)
         {
         generateRXInstruction(cg, TR::InstOpCode::STH, node, termCharReg, outMR);
         nextAdr = generateS390MemoryReference(outputReg, 2, cg);
         }
      else
         {
         generateRXInstruction(cg, TR::InstOpCode::STC, node, termCharReg, outMR);
         nextAdr = generateS390MemoryReference(outputReg, 1, cg);
         }

      // Advance output register address by y amount (TRxy)
      generateRXInstruction(cg, TR::InstOpCode::LA, node, outputReg, nextAdr);

      if (opCode == TR::InstOpCode::TROT || opCode == TR::InstOpCode::TROO )
         {
         nextAdr = generateS390MemoryReference(inputReg, 1, cg);
         // Advance input register address by y amount (TRxy)
         generateRXInstruction(cg, TR::InstOpCode::LA, node, inputReg, nextAdr );
         // Decrement length by x amount (TRxy)
         generateRIInstruction(cg, TR::InstOpCode::AHI, node, inputLenReg, -1 );
         }
      else
         {
         nextAdr = generateS390MemoryReference(inputReg, 2, cg);
         generateRXInstruction(cg, TR::InstOpCode::LA, node, inputReg, nextAdr );
         generateRIInstruction(cg, TR::InstOpCode::AHI, node, inputLenReg, -2 );
         }

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, topOfLoop); // repeat loop after character copies
      outMR->stopUsingMemRefRegister(cg);
      }

   // Create Dependencies for the TRXX instruction
   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);
   dependencies->addPostCondition(outputReg, TR::RealRegister::LegalEvenOfPair);
   dependencies->addPostCondition(inputLenReg, TR::RealRegister::LegalOddOfPair);
   dependencies->addPostCondition(outputPair, TR::RealRegister::EvenOddPair);
   dependencies->addPostCondition(termCharReg, TR::RealRegister::GPR0);
   dependencies->addPostCondition(tableReg, TR::RealRegister::GPR1);
   dependencies->addPostCondition(inputReg, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   if (opCode == TR::InstOpCode::TRTO || opCode == TR::InstOpCode::TRTT)
      {
      if (cg->comp()->target().is64Bit())
         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, inputLenReg, inputLenReg, 1);
      else
         generateRSInstruction(cg, TR::InstOpCode::SRL, node, inputLenReg, 1);
      }

   // The return value is the number of translated characters.
   // (num of chars left to translate + 1) - (num of chars required for translation)
   generateRRInstruction(cg, TR::InstOpCode::SR, node, resultReg, inputLenReg);

   node->setRegister(resultReg);

   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));
   cg->decReferenceCount(node->getChild(4));
   cg->decReferenceCount(node->getChild(5));
   cg->stopUsingRegister(inputReg);
   if (!node->getTableBackedByRawStorage())
      cg->stopUsingRegister(tableReg);
   cg->stopUsingRegister(outputPair);
   cg->stopUsingRegister(termCharReg);
   if (isLengthConstant)
      cg->stopUsingRegister(inputLenReg);

   return resultReg;
   }

  //To fully utilize MVC's capacity, do this within 4k!
  #define LARGEARRAYSET_PADDING_SIZE 256
  #define LARGEARRAYSET_MAX_SIZE LARGEARRAYSET_PADDING_SIZE * 16

TR::Register *getLitPoolBaseReg(TR::Node *node, TR::CodeGenerator * cg)
   {
   TR::Register *litPoolBaseReg = NULL;
   if (cg->isLiteralPoolOnDemandOn())
      {
      litPoolBaseReg = cg->allocateRegister();
      generateLoadLiteralPoolAddress(cg, node, litPoolBaseReg);
      }
   else
      {
      litPoolBaseReg = cg->getLitPoolRealRegister();
      }
   return litPoolBaseReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::arraysetEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * baseAddr = node->getChild(0);
   TR::Node * constExpr = node->getChild(1);
   TR::Node * elemsExpr = node->getChild(2);
   bool lenMinusOne = false;
   TR::Compilation *comp = cg->comp();

   TR::Register * baseReg = NULL;
   bool evaluateChildren=true;

   TR::Register * constExprRegister = NULL;
   TR::Register * elemsRegister = NULL;
   bool mvcCopy = false;
   bool mvcluCopy = false;
   bool stgCopy = false;
   bool varLength = false;
   uint32_t bv;
   int32_t shiftAmount;
   bool isZero = false;
   bool alloc = false;

   // arrayset of zero bytes
   if (elemsExpr->getOpCode().isLoadConst() &&
       elemsExpr->getUnsignedLongInt() == 0)
      {
      cg->decReferenceCount(elemsExpr);
      if (baseAddr->getRegister() == NULL) cg->evaluate(baseAddr);
      cg->decReferenceCount(baseAddr);
      if (constExpr->getRegister() == NULL) cg->evaluate(constExpr);
      cg->decReferenceCount(constExpr);

      return NULL;
      }

   TR::DataType constType = constExpr->getDataType();


   if (constType == TR::Address)
      {
      bool refs64 = cg->comp()->target().is64Bit() && !comp->useCompressedPointers();
      constType = (refs64 ? TR::Int64 : TR::Int32);
      }

   if (constExpr->getOpCode().isLoadConst())
      {
      switch (constType)
         {
         case TR::Int8:
            mvcCopy = true;
            bv = constExpr->getByte();
            shiftAmount = 0;
            break;
         case TR::Int16:
               {
               shiftAmount = 1;
               uint32_t value = (uint32_t) constExpr->getShortInt();
               bv = (value & 0xFF);
               if (bv == ((value & 0xFF00) >> 8))
                  {
                  // most likely case is 0
                  mvcCopy = true;
                  }

               //Java needs to use STG to keep atomicity on reading the padding (2/4 byes).
               if (!comp->getOption(TR_DisableZArraySetUnroll) &&
                        ((elemsExpr->getOpCode().isLoadConst() && elemsExpr->getUnsignedLongInt() >= 64) ||
                         (!elemsExpr->getOpCode().isLoadConst())))
                  {
                  stgCopy = true;
                  if (!elemsExpr->getOpCode().isLoadConst())
                     varLength = true;
                  }
               break;
               }
         case TR::Int32:
         case TR::Float:
               {
               shiftAmount = 2;
               uint32_t value = (constType == TR::Float) ? constExpr->getFloatBits() : constExpr->getUnsignedInt();
               bv = (value & 0xFF);
               if (bv == ((value & 0xFF00) >> 8) && bv == ((value & 0xFF0000) >> 16) && bv == ((value & 0xFF000000) >> 24))
                  {
                  // most likely case is 0
                  mvcCopy = true;
                  }
               if (!comp->getOption(TR_DisableZArraySetUnroll) &&
                   elemsExpr->getOpCode().isLoadConst() && elemsExpr->getUnsignedLongInt() >= 64 && constType ==  TR::Int32)
                  {
                  stgCopy = true;
                  }
               break;
               }
         case TR::Int64:
         case TR::Double:
               {
               shiftAmount = 3;
               uint32_t value = constExpr->getUnsignedLongIntHigh();
               bv = (value & 0xFF);
               if (bv == ((value & 0xFF00) >> 8) && bv == ((value & 0xFF0000) >> 16) && bv == ((value & 0xFF000000) >> 24))
                  {
                  uint32_t value = (uint32_t) constExpr->getLongIntLow();
                  uint32_t bv2 = (value & 0xFF);
                  if (bv == bv2 &&
                     (((value & 0xFF00) >> 8) && bv == ((value & 0xFF0000) >> 16) && bv == ((value & 0xFF000000) >> 24)))
                     {
                     // most likely case is 0
                     mvcCopy = true;
                     }
                  }
               break;
               }

         default:
            TR_ASSERT(0, "Unexpected copy datatype %s encountered on arrayset", constType.toString());
            break;
         }
      }

   uint64_t elems=0;
   bool useMVI = false;
   if (elemsExpr->getOpCode().isLoadConst())
      {
      elems = elemsExpr->getIntegerNodeValue<uint64_t>();
      }


   // Use an MVI instead of an LHI and STC pair (LHI here, STC in MemInitConstLenMacroOp), but only if the constant isn't
   // already in a register and the value doesn't need to remain in a register after the array is initialized.
   if (constType == TR::Int8 && mvcCopy && bv != 0 && constExpr->getRegister() == NULL && constExpr->getReferenceCount() == 1)
      useMVI = true;

   if (constExpr->getOpCode().isLoadConst() && !useMVI)
      {
      if (mvcCopy && (bv == 0))
         {
         constExprRegister = cg->allocateRegister();
         isZero = true;
         }
      else
         {
         if (mvcCopy && (bv != 0))
            {
            if (constType == TR::Int8)
               {
               constExprRegister = cg->evaluate(constExpr);
               }
            else
               {
               constExprRegister = cg->allocateRegister();
               generateLoad32BitConstant(cg, node, bv, constExprRegister, true);
               }
            }

         else if (mvcluCopy)
            {
            uint16_t value = (uint16_t) constExpr->getShortInt();
            constExprRegister = cg->allocateRegister();
            generateLoad32BitConstant(cg, node, value, constExprRegister, true);
            }

         else if (stgCopy)
            {
            constExprRegister = cg->allocateRegister();
            if (constType == TR::Int16)
               {
               // pack in 4 half words in constExprRegister
               uint64_t value = (uint64_t) constExpr->getShortInt();
               value &= 0xFFFF; //clear high order bits
               value += (value <<16);
               value += (value <<32);
               genLoadLongConstant(cg, node, value, constExprRegister, NULL, NULL, NULL);
               //printf ("\n STG for short generated in %s", comp->signature());fflush(stdout);
               }
            else
               {
               // pack in 2 words in constExprRegister
               uint64_t value = (uint64_t) constExpr->getInt();
               value &= 0xFFFFFFFF; //clear high order bits
               value += (value <<32);
               genLoadLongConstant(cg, node, value, constExprRegister, NULL, NULL, NULL);
               //printf ("\n STG for int generated in %s", comp->signature());fflush(stdout);
               }
            }
         else
            {
            constExprRegister = generateLoad32BitConstant(cg, constExpr);
            }
         }
      }
   else if (!useMVI)
      {
      constExprRegister = cg->gprClobberEvaluate(constExpr);

      if (constExpr->getDataType() == TR::Int8)
         {
         mvcCopy = true;
         }
      }
   cg->decReferenceCount(constExpr);

   if (elemsExpr->getOpCode().isLoadConst())
      {
      if (!comp->getOption(TR_DisableSSOpts) &&
          mvcCopy && elems <=256 && elems!=0)
         {
         evaluateChildren=false;
         }
      }
   else
      {
      elemsRegister = cg->evaluateLengthMinusOneForMemoryOps(elemsExpr, true, lenMinusOne);
      }


   // If we knew the node operation was happening on
   // <= 256 bytes, we could avoid evaluating
   // children. However, we do not have this analysis,
   // and so we universally evaluate the children.
   //
   // if (node operation <= 256 bytes)
   evaluateChildren=true;

   //if (evaluateChildren)
   //     baseReg = cg->gprClobberEvaluate(baseAddr);

   cg->decReferenceCount(elemsExpr);
   //TR_ASSERTC( elemsExpr->getSize() <=4,comp, "If this is not true we should not be truncating");
   //someone should investigate why this assertion is not being hit

   if (mvcCopy)
      {
      if (elemsExpr->getOpCode().isLoadConst()) // length known at compe-time
         {
         if (isZero)
            {
            MemClearConstLenMacroOp op(node, baseAddr, cg, elems);
            ClobberRegisterForLoops(evaluateChildren,op,baseAddr,baseReg);
            op.generate(baseReg);
            }
         else
            {
            if (useMVI)
               {
               MemInitConstLenMacroOp op(node, baseAddr, cg, elems, constExpr->getByte());
               ClobberRegisterForLoops(evaluateChildren, op, baseAddr, baseReg);
               op.generate(baseReg);
               }
            else
               {
               MemInitConstLenMacroOp op(node, baseAddr, cg, elems, constExprRegister);
               ClobberRegisterForLoops(evaluateChildren, op, baseAddr, baseReg);
               op.generate(baseReg);
               }
            }
         }
      else // length unknown at compile-time
         {
         if (isZero)
            {
            MemClearVarLenMacroOp op(node, baseAddr, cg, elemsRegister, elemsExpr, lenMinusOne);
            AlwaysClobberRegisterForLoops(evaluateChildren, baseAddr, baseReg);
            op.setUseEXForRemainder(true);
            op.generate(baseReg);
            }
         else
            {
            if (useMVI)
               {
               MemInitVarLenMacroOp op(node, baseAddr, cg, elemsRegister, constExpr->getByte(), elemsExpr, lenMinusOne);
               AlwaysClobberRegisterForLoops(evaluateChildren, baseAddr, baseReg);
               op.setUseEXForRemainder(true);
               op.generate(baseReg);
               }
            else
               {
               MemInitVarLenMacroOp op(node, baseAddr, cg, elemsRegister, constExprRegister, elemsExpr, lenMinusOne);
               AlwaysClobberRegisterForLoops(evaluateChildren, baseAddr, baseReg);
               op.setUseEXForRemainder(true);
               op.generate(baseReg);
               }
            }
         }
      }

   else if (mvcluCopy)
      {
      TR::Node *targetNode = baseAddr;
      TR::Node *targetLenNode = elemsExpr ; //elemsExpr is a const

      TR::Register *targetReg = cg->gprClobberEvaluate(targetNode);
      TR::Register *targetLenReg = generateLoad32BitConstant(cg, targetLenNode);

      TR::Register *sourceReg =  cg->allocateRegister(); //Reuse the target's base address
      generateRRInstruction(cg, TR::InstOpCode::LR, node, sourceReg, targetReg);

      //Set the source length register to 0
      TR::Register *sourceLenReg =  cg->allocateRegister();
      generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, sourceLenReg, sourceLenReg);

      uint16_t disp = (uint16_t) constExpr->getShortInt();
      TR::MemoryReference *paddingRef = generateS390MemoryReference(NULL, disp, cg);

      TR::RegisterPair *sourcePairReg = cg->allocateConsecutiveRegisterPair(sourceLenReg, sourceReg);
      TR::RegisterPair *targetPairReg = cg->allocateConsecutiveRegisterPair(targetLenReg, targetReg);

      TR::RegisterDependencyConditions * regDeps = cg->createDepsForRRMemoryInstructions(node, sourcePairReg, targetPairReg);

      //MVCLU
      TR::LabelSymbol *continueLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, continueLabel);
      continueLabel->setStartInternalControlFlow();

      generateRSInstruction(cg, TR::InstOpCode::MVCLU, node, targetPairReg, sourcePairReg, paddingRef);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK1, node, continueLabel);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, regDeps);
      cFlowRegionEnd->setEndInternalControlFlow();


      cg->stopUsingRegister(targetPairReg);
      cg->stopUsingRegister(targetReg);
      cg->stopUsingRegister(targetLenReg);

      cg->stopUsingRegister(sourcePairReg);
      cg->stopUsingRegister(sourceReg);
      cg->stopUsingRegister(sourceLenReg);
      }

   else if (stgCopy)
      {
      if (evaluateChildren)
          baseReg = cg->gprClobberEvaluate(baseAddr);

      int iter =  elems >> 6; // unroll 8 times, each STG = 8 bytes, so divide by 64
      int remainder = elems & 0x0000003F;
      uint16_t value = (uint16_t) constExpr->getShortInt();
      TR::Register* itersReg = cg->allocateRegister();
      TR::Register* indexReg = cg->allocateRegister();
      TR::LabelSymbol * topOfLoop = generateLabelSymbol(cg);
      TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
      TR::LabelSymbol * endOfLoop = NULL;
      TR::RegisterDependencyConditions * dependencies = NULL;

      generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, indexReg, indexReg);

      if (varLength)
         {
         /*
           SRAG  GPR_Iteration, GPR_elemsReg, 6 ; unroll=8, 8 bytes per each STG
           BRC  eq, Label L0097; start of internal control flow
         */
         endOfLoop = generateLabelSymbol(cg);

         if (elemsExpr->getDataType() == TR::Int64)
            {
            generateRSInstruction(cg, TR::InstOpCode::SRAG, node, itersReg, elemsRegister, 6);
            }
         else
            {
            if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196))
               {
               generateRSInstruction(cg, TR::InstOpCode::SRAK, node, itersReg, elemsRegister, 6);
               }
            else
               {
               generateRRInstruction(cg, TR::InstOpCode::LR, node, itersReg, elemsRegister);
               generateRSInstruction(cg, TR::InstOpCode::SRA, node, itersReg, 6);
               }
            }

         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, endOfLoop); // repeat loop after character copies
         }
      else
         {
         generateLoad32BitConstant(cg, node, iter, itersReg, true);
         }
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, topOfLoop);
      topOfLoop->setStartInternalControlFlow();

      for (int i=0; i < 8; i++)
         {
         generateRXInstruction(cg, TR::InstOpCode::STG, node, constExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, i*8, cg));
         }
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, indexReg, 64);
      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
      deps->addPostCondition(indexReg, TR::RealRegister::AssignAny);
      deps->addPostCondition(baseReg, TR::RealRegister::AssignAny);
      deps->addPostCondition(constExprRegister, TR::RealRegister::AssignAny);
      deps->addPostCondition(itersReg, TR::RealRegister::AssignAny);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, itersReg, topOfLoop);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
      cFlowRegionEnd->setEndInternalControlFlow();

      if (varLength)
         {
         TR::LabelSymbol * topOfLoop1 = generateLabelSymbol(cg);
         TR::LabelSymbol * endOfLoop1 = generateLabelSymbol(cg);
         TR::Register* residueReg = cg->allocateRegister();
         dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);
         dependencies->addPostCondition(itersReg, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(indexReg, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(elemsRegister, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(constExprRegister, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(residueReg, TR::RealRegister::AssignAny);
         dependencies->addPostCondition(baseReg, TR::RealRegister::AssignAny);

         /*
           LGR  GPR_Residue, GPR_elemsReg
           NILF GPR_Residue, 0x3F
           SRA  GPR_Residue, 1; for 2-bytes shorts
           BRC  eq, Label L0099
           Label L0098:
           STH  GPR_0352,#587 0(GPR_0355,GPR_0353)
           BRCT GPR_Residue, Label L0098
           Label L0099:
           POST:{GPR_Residue:assignAny},{GPR_0352:assignAny},{GPR_0353:assignAny},{GPR_0355:assignAny}
         */
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, endOfLoop);
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, residueReg, elemsRegister);
         generateRILInstruction(cg, TR::InstOpCode::NILF, node, residueReg, 0x3f);
         generateRSInstruction(cg, TR::InstOpCode::SRA, node, residueReg, 1);
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, endOfLoop1);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, topOfLoop1);
         topOfLoop1->setStartInternalControlFlow();
         generateRXInstruction(cg, TR::InstOpCode::STH, node, constExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, 0, cg));
         generateRXInstruction(cg, TR::InstOpCode::LA, node, indexReg, generateS390MemoryReference(indexReg, 2, cg));
         generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, residueReg, topOfLoop1);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, endOfLoop1, dependencies);
         endOfLoop1->setEndInternalControlFlow();

         cg->stopUsingRegister(residueReg);
         cg->stopUsingRegister(itersReg);
         cg->stopUsingRegister(indexReg);

         }
      else
         {
         // how many double words left?
         int iterDwords = remainder >> 3;
         int remainderDwords = remainder & 7;
         int offset = 0;
         for (int i=0; i< iterDwords; i++)
            {
            generateRXInstruction(cg, TR::InstOpCode::STG, node, constExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, offset, cg));
            offset += 8;
            }

         // how many words left?
         int iterWords = remainderDwords >> 2;
         int remainderWords = remainderDwords & 3;
         TR::Register *tmpConstExprRegister = cg->allocateRegister();
         generateRSInstruction(cg, TR::InstOpCode::SRLG, node, tmpConstExprRegister, constExprRegister, 32);

         for (int i=0; i< iterWords; i++)
            {
            generateRXInstruction(cg, TR::InstOpCode::ST, node, tmpConstExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, offset, cg));
            offset += 4;
            }

         switch(remainderWords)
            {
            case 1:
               generateRSInstruction(cg, TR::InstOpCode::SRL, node, tmpConstExprRegister, 24);
               generateRXInstruction(cg, TR::InstOpCode::STC, node, tmpConstExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, offset, cg));
               offset++;
               break;
            case 2:
               generateRSInstruction(cg, TR::InstOpCode::SRL, node, tmpConstExprRegister, 16);
               generateRXInstruction(cg, TR::InstOpCode::STH, node, tmpConstExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, offset, cg));
               offset += 2;
               break;
            case 3:
               {
               TR::Register *tmpByteConstReg = cg->allocateRegister();
               generateShiftThenKeepSelected31Bit(node, cg, tmpByteConstReg, tmpConstExprRegister, 24, 31, -8);
               generateRSInstruction(cg, TR::InstOpCode::SRL, node, tmpConstExprRegister, 16); //0x0000aabb
               generateRXInstruction(cg, TR::InstOpCode::STH, node, tmpConstExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, offset, cg));
               offset += 2;
               generateRXInstruction(cg, TR::InstOpCode::STC, node, tmpByteConstReg, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, offset, cg));
               offset++;
               cg->stopUsingRegister(tmpByteConstReg);
               break;
               }
            default:
               break;
            }

         cg->stopUsingRegister(itersReg);
         cg->stopUsingRegister(indexReg);
         cg->stopUsingRegister(tmpConstExprRegister);
         /*
         // this code sequence is extremely slow due to 2 bytes MVC copy
         generateRXInstruction(cg, TR::InstOpCode::STH, node, constExprRegister, new (cg->trHeapMemory()) TR::MemoryReference(baseReg, 0, cg));
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node,
         elems-2,
         generateS390MemoryReference(baseReg, 2, cg),
         generateS390MemoryReference(baseReg, 0, cg));
         */
         }
      }
   else
      {
      if (evaluateChildren)
          baseReg = cg->gprClobberEvaluate(baseAddr);

      // copy elements over one element at a time
      if (elemsExpr->getOpCode().isLoadConst())
         {
         elemsRegister = cg->allocateRegister();
         generateLoad32BitConstant(cg, elemsExpr, elems, elemsRegister, true);
         }

      TR::Register * strideRegister = cg->allocateRegister();

      MemInitVarLenTypedMacroOp vtOp(node, baseAddr, cg, constType, elemsRegister, constExprRegister, elemsExpr);

      vtOp.generate(baseReg, strideRegister);

      cg->stopUsingRegister(strideRegister);
      }


   if (elemsRegister)
      cg->stopUsingRegister(elemsRegister);
   if (!useMVI)
      cg->stopUsingRegister(constExprRegister);
   if (evaluateChildren)
      {
      cg->stopUsingRegister(baseReg);
      cg->decReferenceCount(baseAddr);
      }

   return NULL;
   }

void
OMR::Z::TreeEvaluator::generateLoadAndStoreForArrayCopy(TR::Node *node, TR::CodeGenerator *cg, TR::MemoryReference *srcMemRef, TR::MemoryReference *dstMemRef, TR_S390ScratchRegisterManager *srm, TR::DataType elenmentType, bool needsGuardedLoad, TR::RegisterDependencyConditions* deps)
   {
   TR::Register *workReg = srm->findOrCreateScratchRegister();
   switch (elenmentType)
      {
      case TR::Int8:
         generateRXInstruction(cg, TR::InstOpCode::IC, node, workReg, srcMemRef);
         generateRXInstruction(cg, TR::InstOpCode::STC, node, workReg, dstMemRef);
         break;
      case TR::Int16:
         generateRXInstruction(cg, TR::InstOpCode::LH, node, workReg, srcMemRef);
         generateRXInstruction(cg, TR::InstOpCode::STH, node, workReg, dstMemRef);
         break;
      case TR::Int32:
      case TR::Float:
         generateRXInstruction(cg, TR::InstOpCode::L, node, workReg, srcMemRef);
         generateRXInstruction(cg, TR::InstOpCode::ST, node, workReg, dstMemRef);
         break;
      case TR::Int64:
      case TR::Double:
            generateRXInstruction(cg, TR::InstOpCode::LG, node, workReg, srcMemRef);
            generateRXInstruction(cg, TR::InstOpCode::STG, node, workReg, dstMemRef);
         break;
      case TR::Address:
         if (cg->comp()->target().is64Bit() && !cg->comp()->useCompressedPointers())
            {
            if (needsGuardedLoad)
               {
               generateRXInstruction(cg, TR::InstOpCode::LGG, node, workReg, srcMemRef);
               }
            else
               {
               generateRXInstruction(cg, TR::InstOpCode::LG, node, workReg, srcMemRef);
               }
            generateRXInstruction(cg, TR::InstOpCode::STG, node, workReg, dstMemRef);
            }
         else
            {
            if (needsGuardedLoad)
               {
               int32_t shiftAmount = TR::Compiler->om.compressedReferenceShift();
               generateRXInstruction(cg, TR::InstOpCode::LLGFSG, node, workReg, srcMemRef);
               if (shiftAmount != 0)
                  {
                  generateRSInstruction(cg, TR::InstOpCode::SRLG, node, workReg, workReg, shiftAmount);
                  }
               }
            else
               {
               generateRXInstruction(cg, TR::InstOpCode::L, node, workReg, srcMemRef);
               }
            generateRXInstruction(cg, TR::InstOpCode::ST, node, workReg, dstMemRef);
            }
         break;
      default:
         TR_ASSERT_FATAL(false, "elementType of invalid type\n");
         break;
      }
   srm->reclaimScratchRegister(workReg);
   }


TR::RegisterDependencyConditions*
OMR::Z::TreeEvaluator::generateMemToMemElementCopy(TR::Node *node, TR::CodeGenerator *cg, TR::Register *byteSrcReg, TR::Register *byteDstReg, TR::Register *byteLenReg, TR_S390ScratchRegisterManager *srm, bool isForward, bool needsGuardedLoad, bool genStartICFLabel, TR::RegisterDependencyConditions* deps)
   {
   // This function uses a BRXHG/BRXLG instruction for updating index and compare and branch which requires register pair.
   // Scratch Register Manager does not provide any means for use to allocate EvenOdd register pair. So this function
   // Allocates those registers separately and adds it to register dependency condition and return that which caller needs to combine with it's dependency conditions
   TR::Register *incRegister = cg->allocateRegister();
   TR::Register *endReg = cg->allocateRegister();
   TR::RegisterPair *brxReg = cg->allocateConsecutiveRegisterPair(endReg, incRegister);

   if (deps == NULL)
      {
      deps = generateRegisterDependencyConditions(0, 3, cg);
      }
   deps->addPostCondition(incRegister, TR::RealRegister::LegalEvenOfPair);
   deps->addPostCondition(endReg, TR::RealRegister::LegalOddOfPair);
   deps->addPostCondition(brxReg, TR::RealRegister::EvenOddPair);
   TR::DataType elementType = node->getArrayCopyElementType();
   int32_t elementSize = (elementType == TR::Address && cg->comp()->target().is64Bit() && cg->comp()->useCompressedPointers()) ? 4 : TR::DataType::getSize(elementType);
   TR::Instruction *cursor = NULL;
   TR_Debug *debug = cg->getDebug();
#define iComment(str) if (debug) debug->addInstructionComment(cursor, (str));

   if (isForward)
      {
      cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, incRegister, elementSize);
      iComment("Load increment Value");
      cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, endReg, byteDstReg);
      cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, endReg, -1 * elementSize);
      cursor = generateRRInstruction(cg, TR::InstOpCode::getAddRegWidenOpCode(), node, endReg, byteLenReg);
      iComment("Pointer to last element in destination");
      TR::MemoryReference *srcMemRef = generateS390MemoryReference(byteSrcReg, 0, cg);
      TR::MemoryReference *dstMemRef = generateS390MemoryReference(byteDstReg, 0, cg);
      TR::LabelSymbol *topOfLoop = generateLabelSymbol(cg);
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, topOfLoop);
      iComment("topOfLoop");
      // needsGuardedLoad means we are generating MemToMemCopy sequence in OOL where ICF starts here
      if (needsGuardedLoad)
         topOfLoop->setStartInternalControlFlow();
      TR::TreeEvaluator::generateLoadAndStoreForArrayCopy(node, cg, srcMemRef, dstMemRef, srm, elementType, needsGuardedLoad, deps);
      cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, byteSrcReg, generateS390MemoryReference(byteSrcReg, elementSize, cg));
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::getBranchRelIndexEqOrLowOpCode(), node, brxReg, byteDstReg, topOfLoop);
      iComment("byteDst+=elementSize ; if (byteDst <= lastElement) GoTo topOfLoop");
      }
   else
      {
      cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, incRegister, -1 * elementSize);
      iComment("Load increment Value");
      cursor = generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, endReg, byteDstReg);
      cursor = generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, endReg, incRegister);
      iComment("exitCond = Pointer to one element before last element to copy in destination");
      cursor = generateRRInstruction(cg, TR::InstOpCode::getAddRegWidenOpCode(), node, byteLenReg, incRegister);
      cursor = generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, byteSrcReg, byteLenReg);
      cursor = generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, byteDstReg, byteLenReg);
      TR::MemoryReference *srcMemRef = generateS390MemoryReference(byteSrcReg, 0, cg);
      TR::MemoryReference *dstMemRef = generateS390MemoryReference(byteDstReg, 0, cg);
      TR::LabelSymbol *topOfLoop = generateLabelSymbol(cg);
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, topOfLoop);
      // needsGuardedLoad means we are generating MemToMemCopy sequence in OOL where ICF starts here
      // For backward array copy, we need to set start of ICF in case node is known to be of backward direction and number of bytes are constant
      if (needsGuardedLoad || genStartICFLabel)
         topOfLoop->setStartInternalControlFlow();
      TR::TreeEvaluator::generateLoadAndStoreForArrayCopy(node, cg, srcMemRef, dstMemRef, srm, elementType, needsGuardedLoad, deps);
      cursor = generateRXInstruction(cg, TR::InstOpCode::LAY, node, byteSrcReg,
         generateS390MemoryReference(byteSrcReg, -1 * elementSize, cg));
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::getBranchRelIndexHighOpCode(), node, brxReg, byteDstReg, topOfLoop);
      iComment("byteDst-=elementSize; if (byteDst > exitCond) GoTo topOfLoop");
      }
   return deps;
#undef iComment
   }

void
OMR::Z::TreeEvaluator::genLoopForwardArrayCopy(TR::Node *node, TR::CodeGenerator *cg, TR::Register *byteSrcReg, TR::Register *byteDstReg, TR::Register *loopIterReg, bool isConstantLength)
   {
   TR::LabelSymbol *processingLoop = generateLabelSymbol(cg);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processingLoop);
   generateSS1Instruction(cg, TR::InstOpCode::MVC, node, 255,
      generateS390MemoryReference(byteDstReg, 0, cg),
      generateS390MemoryReference(byteSrcReg, 0, cg));
   generateRXInstruction(cg, TR::InstOpCode::LA, node, byteSrcReg, generateS390MemoryReference(byteSrcReg, 256, cg));
   generateRXInstruction(cg, TR::InstOpCode::LA, node, byteDstReg, generateS390MemoryReference(byteDstReg, 256, cg));
   generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, loopIterReg, processingLoop);
   // If it is known at compile time that this is forward array copy and also length is known at compile time then internal control flow starts here.
   if (node->isForwardArrayCopy() && isConstantLength)
      processingLoop->setStartInternalControlFlow();
   }


void
OMR::Z::TreeEvaluator::forwardArrayCopySequenceGenerator(TR::Node *node, TR::CodeGenerator *cg, TR::Register *byteSrcReg, TR::Register *byteDstReg, TR::Register *byteLenReg, TR::Node *byteLenNode, TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *mergeLabel)
   {
   if (byteLenNode->getOpCode().isLoadConst())
      {
      // TODO: IMPLEMENT MVCL VERSION OF FORWARD ARRAY COPY
      // If the length is at the THRESHOLD=77825 i.e., 4096*19+1, MVCL becomes a better choice compared to MVC in loop.
      // In general, MVCL shows performance gain over LOOP with MVCs when the length is
      // within [4K*i+1, 4K*i+4089], i=19,20,...,4095.
      // Notice that within the small range [4K*i-7, 4K*i], MVCL is significantly degraded. (3 times slower than normal detected)
      // In order to use MVCL, we have also to make sure that the use is safe. i.e., src and dst are NOT aliased.
      int64_t byteLen = byteLenNode->getConst<int64_t>();
      if (byteLen > 256)
         {
         TR::Register *loopIterReg = srm->findOrCreateScratchRegister();
         int64_t loopIter = byteLenNode->getConst<int64_t>() >> 8;
         genLoadLongConstant(cg, node, loopIter, loopIterReg);
         TR::TreeEvaluator::genLoopForwardArrayCopy(node, cg, byteSrcReg, byteDstReg, loopIterReg, true);
         srm->reclaimScratchRegister(loopIterReg);
         }
      // In following cases following MVC instruction should be generated.
      // 1. byteLength <= 256
      // 2. byteLength > 256 && byteLength is not multiple of 256
      uint8_t residueLength = static_cast<uint8_t>(byteLen & 0xFF);
      if (byteLen <= 256 || residueLength != 0)
         generateSS1Instruction(cg, TR::InstOpCode::MVC, node, static_cast<uint8_t>(residueLength-1),
            generateS390MemoryReference(byteDstReg, 0, cg),
            generateS390MemoryReference(byteSrcReg, 0, cg));
      }
   else
      {
      TR::Register *loopIterReg = srm->findOrCreateScratchRegister();
      generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, byteLenReg, -1);
      generateShiftRightImmediate(cg, node, loopIterReg, byteLenReg, 8);
      TR::LabelSymbol *exrlInstructionLabel = generateLabelSymbol(cg);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, exrlInstructionLabel);
      TR::TreeEvaluator::genLoopForwardArrayCopy(node, cg, byteSrcReg, byteDstReg, loopIterReg, false);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, exrlInstructionLabel);
      TR::LabelSymbol *exrlTargetLabel = generateLabelSymbol(cg);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, exrlTargetLabel);
      TR::Instruction *residueMVC = generateSS1Instruction(cg, TR::InstOpCode::MVC, node, 0,
                                       generateS390MemoryReference(byteDstReg, 0, cg),
                                       generateS390MemoryReference(byteSrcReg, 0, cg));
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, exrlInstructionLabel);
      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) && !cg->comp()->getOption(TR_DisableInlineEXTarget))
         {
         generateRILInstruction(cg, TR::InstOpCode::EXRL, node, byteLenReg, exrlTargetLabel);
         }
      else
         {
         TR::Register *regForResidueMVCTarget = srm->findOrCreateScratchRegister();
         generateEXDispatch(node, cg, byteLenReg, regForResidueMVCTarget, residueMVC);
         srm->reclaimScratchRegister(regForResidueMVCTarget);
         }
      srm->reclaimScratchRegister(loopIterReg);
      }
   }

TR::RegisterDependencyConditions *
OMR::Z::TreeEvaluator::backwardArrayCopySequenceGenerator(TR::Node *node, TR::CodeGenerator *cg, TR::Register *byteSrcReg, TR::Register *byteDstReg, TR::Register *byteLenReg, TR::Node *byteLenNode, TR_S390ScratchRegisterManager *srm, TR::LabelSymbol *mergeLabel)
   {
   TR_Debug *debug = cg->getDebug();
#define iComment(str) if (debug) debug->addInstructionComment(cursor, (str));
   TR::Instruction *cursor = NULL;
   TR::RegisterDependencyConditions *deps = NULL;
   bool genStartICFLabel = node->isBackwardArrayCopy() && byteLenNode->getOpCode().isLoadConst();

   if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z15))
      {
      TR::Register* loopIterReg = srm->findOrCreateScratchRegister();

      cursor = generateShiftRightImmediate(cg, node, loopIterReg, byteLenReg, 8);
      iComment("Calculate loop iterations");

      if (genStartICFLabel)
         {
         TR::LabelSymbol* cFlowRegionStart = generateLabelSymbol(cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         }

      TR::LabelSymbol* handleResidueLabel = generateLabelSymbol(cg);

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, handleResidueLabel);
      iComment("If length < 256 then jump to handle residue");

      // loadBytesReg is used to store number of bytes used by MVCRL instruction
      TR::Register* loadBytesReg = cg->allocateRegister();

      deps = generateRegisterDependencyConditions(0, 1, cg);
      deps->addPostCondition(loadBytesReg, TR::RealRegister::GPR0);

      cursor = generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, loadBytesReg, 255);
      iComment("GPR0 is the length in bytes that will be loaded in one iteration of loop");

      cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, byteSrcReg, generateS390MemoryReference(byteSrcReg, byteLenReg, 0, cg));
      cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, byteDstReg, generateS390MemoryReference(byteDstReg, byteLenReg, 0, cg));

      TR::LabelSymbol* loopLabel = generateLabelSymbol(cg);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopLabel);

      generateRXInstruction(cg, TR::InstOpCode::LA, node, byteSrcReg, generateS390MemoryReference(byteSrcReg, -256, cg));
      generateRXInstruction(cg, TR::InstOpCode::LA, node, byteDstReg, generateS390MemoryReference(byteDstReg, -256, cg));

      cursor = generateSSEInstruction(cg, TR::InstOpCode::MVCRL, node,
         generateS390MemoryReference(byteDstReg, 0, cg),
         generateS390MemoryReference(byteSrcReg, 0, cg));

      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, loopIterReg, loopLabel);

      cursor = generateRILInstruction(cg, TR::InstOpCode::NILF, node, byteLenReg, 0xFF);

      TR::LabelSymbol *skipResidueLabel = generateLabelSymbol(cg);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, skipResidueLabel);

      cursor = generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, byteSrcReg, byteLenReg);
      cursor = generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, byteDstReg, byteLenReg);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, handleResidueLabel);
      iComment("Handle residue");

      cursor = generateRIEInstruction(cg, TR::InstOpCode::getAddHalfWordImmDistinctOperandOpCode(), node, loadBytesReg, byteLenReg, -1, cursor);
      cursor = generateSSEInstruction(cg, TR::InstOpCode::MVCRL, node,
         generateS390MemoryReference(byteDstReg, 0, cg),
         generateS390MemoryReference(byteSrcReg, 0, cg));

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipResidueLabel);

      srm->reclaimScratchRegister(loopIterReg);
      }
   else if (cg->getSupportsVectorRegisters())
      {
      TR::Register *loopIterReg = srm->findOrCreateScratchRegister();
      cursor = generateShiftRightImmediate(cg, node, loopIterReg, byteLenReg, 4);
      iComment("Calculate loop iterations");

      TR::LabelSymbol *handleResidueLabel = generateLabelSymbol(cg);
      if (genStartICFLabel)
         {
         TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         }
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BZ, node, handleResidueLabel);
      iComment("if length <= 16, jump to handling residue");

      TR::LabelSymbol *loopLabel = generateLabelSymbol(cg);
      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopLabel);
      iComment("Vector Load and Strore Loop");
      cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, byteLenReg, -16);

      TR::Register *vectorBuffer = srm->findOrCreateScratchRegister(TR_VRF);
      generateVRXInstruction(cg, TR::InstOpCode::VL , node, vectorBuffer, generateS390MemoryReference(byteSrcReg, byteLenReg, 0, cg));
      generateVRXInstruction(cg, TR::InstOpCode::VST, node, vectorBuffer, generateS390MemoryReference(byteDstReg, byteLenReg, 0, cg), 0);
      generateS390BranchInstruction(cg, TR::InstOpCode::BRCT, node, loopIterReg, loopLabel);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, handleResidueLabel);
      iComment("Handle residue");

      cursor = generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, byteLenReg, -1);
      iComment("Adjust remaining bytes for vectore load/store length instruction");
      TR::LabelSymbol *skipResidueLabel = generateLabelSymbol(cg);
    /** TODO: Exploit constant length in vectorized implementation
      *  For non vectorized version, we are going to copy bytes as per data type so there is no advantage of exploiting constant length unless we unroll the loop
      *  Current vectorized implementation for backward array copy uses byteLen as index to copy bytes from source to destination in backward direction.
      *  So even if length is known at compile time we can use it to avoid generating residue control logic.
      */
      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, byteLenReg, 0, TR::InstOpCode::COND_BL, skipResidueLabel, false);
      generateVRSbInstruction(cg, TR::InstOpCode::VLL , node, vectorBuffer, byteLenReg, generateS390MemoryReference(byteSrcReg, 0, cg));
      generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vectorBuffer, byteLenReg, generateS390MemoryReference(byteDstReg, 0, cg), 0);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, skipResidueLabel);
      srm->reclaimScratchRegister(loopIterReg);
      srm->reclaimScratchRegister(vectorBuffer);
      }
   else
      {
      deps = TR::TreeEvaluator::generateMemToMemElementCopy(node, cg, byteSrcReg, byteDstReg, byteLenReg, srm, false, false, genStartICFLabel);
      }
   return deps;
   #undef iComment
   }

TR::Register*
OMR::Z::TreeEvaluator::arraycopyEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* byteSrcNode = node->getChild(0);
   TR::Node* byteDstNode = node->getChild(1);
   TR::Node* byteLenNode = node->getChild(2);

   TR::TreeEvaluator::primitiveArraycopyEvaluator(node, cg, byteSrcNode, byteDstNode, byteLenNode);

   return NULL;
   }

void
OMR::Z::TreeEvaluator::primitiveArraycopyEvaluator(TR::Node* node, TR::CodeGenerator* cg, TR::Node* byteSrcNode, TR::Node* byteDstNode, TR::Node* byteLenNode)
   {
   TR::Register* byteSrcReg = NULL;
   TR::Register* byteDstReg = NULL;
   TR::Register* byteLenReg = NULL;
   bool isConstantByteLen = byteLenNode->getOpCode().isLoadConst();
   // Check for array copy of zero bytes or negative number of bytes
   if (isConstantByteLen && byteLenNode->getConst<int64_t>() <= 0)
      {
      cg->evaluate(byteSrcNode);
      cg->evaluate(byteDstNode);

      cg->decReferenceCount(byteSrcNode);
      cg->decReferenceCount(byteDstNode);
      cg->decReferenceCount(byteLenNode);

      return;
      }

   byteSrcReg = cg->gprClobberEvaluate(byteSrcNode);
   byteDstReg = cg->gprClobberEvaluate(byteDstNode);
   TR::LabelSymbol *mergeLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
   TR_S390ScratchRegisterManager *srm = cg->generateScratchRegisterManager(3);
   TR::RegisterDependencyConditions *deps = NULL;
   TR_Debug *debug = cg->getDebug();
   TR::Instruction *cursor = NULL;
#define iComment(str) if (debug) debug->addInstructionComment(cursor, (str));
   // If it is variable length load then Internal Control Flow starts where on runtime we check if length of bytes to copy is greater than zero.
   if (!isConstantByteLen)
      {
      byteLenReg = cg->gprClobberEvaluate(byteLenNode);
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpOpCode(), node, byteLenReg, 0, TR::InstOpCode::COND_BNH, mergeLabel, false);
      iComment("byteLen <= 0 goto mergeLabel");
      }

   if (node->isForwardArrayCopy())
      {
      TR::TreeEvaluator::forwardArrayCopySequenceGenerator(node, cg, byteSrcReg, byteDstReg, byteLenReg, byteLenNode, srm, mergeLabel);
      }
   else if (node->isBackwardArrayCopy())
      {
      if (byteLenReg == NULL)
         byteLenReg = cg->gprClobberEvaluate(byteLenNode);
      deps = TR::TreeEvaluator::backwardArrayCopySequenceGenerator(node, cg, byteSrcReg, byteDstReg, byteLenReg, byteLenNode, srm, mergeLabel);
      }
   else
      {
      // We need to decide direction of array copy at runtime.
      if (isConstantByteLen)
         {
         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         }
      TR::LabelSymbol *forwardArrayCopyLabel = generateLabelSymbol(cg);
      cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, byteSrcReg, byteDstReg, TR::InstOpCode::COND_BNL, forwardArrayCopyLabel, false);
      iComment("if byteSrcPointer >= byteDstPointer then GoTo forwardArrayCopy");


      TR::Register *checkBoundReg = srm->findOrCreateScratchRegister();
      if (byteLenReg == NULL)
         {
         TR_ASSERT_FATAL(isConstantByteLen, "byteLenNode can be not evaluated only when we have constant length");
         byteLenReg = cg->allocateRegister();
         genLoadLongConstant(cg, node, byteLenNode->getConst<int64_t>(), byteLenReg);
         }
      cursor = generateRXInstruction(cg, TR::InstOpCode::LA, node, checkBoundReg, generateS390MemoryReference(byteSrcReg, byteLenReg, 0, cg));
      iComment("nextPointerToLastElement=byteSrcPointer+lengthInBytes");

      TR::LabelSymbol *backwardArrayCopyLabel = generateLabelSymbol(cg);
      cursor = generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, checkBoundReg, byteDstReg, TR::InstOpCode::COND_BH, backwardArrayCopyLabel, false);
      iComment("lastElementToCopy > byteDstPointer then GoTo backwardArrayCopy");
      srm->reclaimScratchRegister(checkBoundReg);

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, forwardArrayCopyLabel);
      iComment("Forward Array Copy Sequence");
      TR::TreeEvaluator::forwardArrayCopySequenceGenerator(node, cg, byteSrcReg, byteDstReg, byteLenReg, byteLenNode, srm, mergeLabel);
      cursor = generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, mergeLabel);
      iComment("Jump to MergeLabel");

      cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, backwardArrayCopyLabel);
      iComment("Backward Array Copy Sequence");
      deps = TR::TreeEvaluator::backwardArrayCopySequenceGenerator(node, cg, byteSrcReg, byteDstReg, byteLenReg, byteLenNode, srm, mergeLabel);
      }
   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, mergeLabel);
   if (!(isConstantByteLen && node->isForwardArrayCopy() && byteLenNode->getConst<int64_t>() <= 256))
      {
      deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(deps, 0, 3+srm->numAvailableRegisters(), cg);
      deps->addPostCondition(byteSrcReg, TR::RealRegister::AssignAny);
      deps->addPostCondition(byteDstReg, TR::RealRegister::AssignAny);
      if (byteLenReg != NULL)
         {
         deps->addPostCondition(byteLenReg, TR::RealRegister::AssignAny);
         }
      srm->addScratchRegistersToDependencyList(deps);
      cursor->setDependencyConditions(deps);
      mergeLabel->setEndInternalControlFlow();
      }

   cg->decReferenceCount(byteDstNode);
   cg->decReferenceCount(byteSrcNode);
   cg->decReferenceCount(byteLenNode);

   // All registers used in this evaluator are added to the Register Dependency Condition deps.
   if (deps != NULL)
      {
      deps->stopUsingDepRegs(cg);
      }
   else
      {
      cg->stopUsingRegister(byteSrcReg);
      cg->stopUsingRegister(byteDstReg);
      cg->stopUsingRegister(byteLenReg);
      srm->stopUsingRegisters();
      }
#undef iComment
   }

/**
 * Tactical GRA Support
 */
TR::Register *
OMR::Z::TreeEvaluator::aRegLoadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * globalReg = node->getRegister();
   TR_GlobalRegisterNumber globalRegNum;
   TR::Compilation *comp = cg->comp();

   if (globalReg == NULL)
      {
      globalRegNum = node->getGlobalRegisterNumber();
      }

   if (globalReg == NULL)
      {
      if (node->getOpCodeValue()==TR::aRegLoad  && node->getRegLoadStoreSymbolReference())
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
              {
              globalReg = cg->allocateCollectedReferenceRegister();
              }
           }
         }
      else
         {
         globalReg = cg->allocateCollectedReferenceRegister();
         }
      node->setRegister(globalReg);
      }

   TR::Machine *machine = cg->machine();

   // GRA needs to tell LRA about the register type
   if (cg->comp()->target().is64Bit())
      {
      globalReg->setIs64BitReg(true);
      }

   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::iRegLoadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * globalReg = node->getRegister();
   TR_GlobalRegisterNumber globalRegNum;
   TR::Compilation *comp = cg->comp();

   if(globalReg == NULL)
      {
      globalRegNum = node->getGlobalRegisterNumber();
      }

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister();

      if (cg->getExtendedToInt64GlobalRegisters().ValueAt(node->getGlobalRegisterNumber()))
         {
         // getExtendedToInt64GlobalRegisters is set by TR_LoadExtensions and it means a larger larger virtual register must be used here
         // so any instructions generated by local RA are the correct size to preserve the upper bits (e.g. use LGR vs LR)
         globalReg->setIs64BitReg();
         }

      node->setRegister(globalReg);
      }

   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::lRegLoadEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Register * globalReg = node->getRegister();
   TR_GlobalRegisterNumber globalRegNum;
   TR::Compilation *comp = cg->comp();
   TR::RegisterPair *globalPair=NULL;
   TR::Register *globalRegLow;
   TR::Register *globalRegHigh;

   if(globalReg == NULL)
     {
     globalRegNum = node->getGlobalRegisterNumber();
     }

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister();

      node->setRegister(globalReg);
      }

   // GRA needs to tell LRA about the register type
   globalReg->setIs64BitReg(true);

   return globalReg;
   }

/**
 * Also handles TR::aRegStore
 */
TR::Register *
OMR::Z::TreeEvaluator::iRegStoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node* value = node->getFirstChild();

   TR::Register* globalReg = NULL;

   // If the child is an add/sub that could be used in a branch on count op, don't evaluate it here
   if (isBranchOnCountAddSub(cg, value))
      {
      if (value->getFirstChild()->getGlobalRegisterNumber() == node->getGlobalRegisterNumber())
         {
         cg->decReferenceCount(value);

         return globalReg;
         }
      }

   bool needsLGFR = node->needsSignExtension();
   bool useLGHI = false;

   if (needsLGFR && value->getOpCode().isLoadConst() &&
       getIntegralValue(value) < MAX_IMMEDIATE_VAL &&
       getIntegralValue(value) > MIN_IMMEDIATE_VAL &&
       !cg->comp()->compileRelocatableCode())
      {
      needsLGFR = false;
      if (cg->comp()->target().is64Bit())
         {
         useLGHI = true;
         }
      }

   bool noLGFgenerated = true;

   if (needsLGFR && value->getOpCode().isLoadVar() && value->getRegister() == NULL && value->getType().isInt32())
      {
      value->setSignExtendTo64BitAtSource(true);
      value->setUseSignExtensionMode(true);
      noLGFgenerated =false;
      }

   if (!useLGHI)
      {
      globalReg = cg->evaluate(value);
      }
   else
      {
      globalReg = value->getRegister();
      if (globalReg == NULL)
         {
         globalReg = value->setRegister(cg->allocateRegister());
         }
      globalReg->setIs64BitReg(true);
      genLoadLongConstant(cg, value, getIntegralValue(value), globalReg);
      }

   // Without extensive evaluation of children & context, assume that we might have swapped signs
   globalReg->resetAlreadySignExtended();

   if (needsLGFR)
      {
      if (noLGFgenerated)
         {
         generateRRInstruction(cg, TR::InstOpCode::LGFR, node, globalReg, globalReg);
         }

      globalReg->setAlreadySignExtended();
      }

   cg->decReferenceCount(value);

   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::lRegStoreEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node* value = node->getFirstChild();

   TR::Register* globalReg = cg->evaluate(value);

   // GRA needs to tell LRA about the register type
   globalReg->setIs64BitReg(true);

   cg->decReferenceCount(value);

   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::GlRegDepsEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   int32_t i;

   for (i = 0; i < node->getNumChildren(); i++)
     {
     cg->evaluate(node->getChild(i));
     cg->decReferenceCount(node->getChild(i));
     }

   return NULL;
   }

/**
 * NOPEvaluator - generic NOP evaluator for opcode where no action is needed.
 */
TR::Register *
OMR::Z::TreeEvaluator::NOPEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   return NULL;
   }

/**
 * barrierFenceEvaluator - evaluator for various memory barriers
 */
TR::Register *
OMR::Z::TreeEvaluator::barrierFenceEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();

   TR::Instruction * fenceInstruction;
   if (opCode == TR::loadFence)
      {
      // create fence nop
      fenceInstruction = generateS390PseudoInstruction(cg, TR::InstOpCode::FENCE, node, (TR::Node *) NULL);
      }
   else if (opCode == TR::storeFence)
      {
      // create fence nop
      fenceInstruction = generateS390PseudoInstruction(cg, TR::InstOpCode::FENCE, node, (TR::Node *) NULL);
      }
   else if (opCode == TR::fullFence)
      {
      if (node->canOmitSync())
         {
         // create fence nop
         fenceInstruction = generateS390PseudoInstruction(cg, TR::InstOpCode::FENCE, node, (TR::Node *) NULL);
         }
      else
         {
         // generate serialization instruction
         fenceInstruction = generateSerializationInstruction(cg, node);
         }
      }
   return NULL;
   }

/**
 * long2StringEvaluator - Convert integer/long value to String
 */
TR::Register *
OMR::Z::TreeEvaluator::long2StringEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR::Node * inputNode = node->getChild(0);
   bool isLong = (inputNode->getDataType() == TR::Int64);
   TR::Register * inputReg = cg->evaluate(inputNode);
   TR::Register * baseReg = cg->evaluate(node->getChild(1));
   TR::Register * countReg = cg->gprClobberEvaluate(node->getChild(2));
   TR::Register * workReg = cg->evaluate(node->getChild(3));
   TR::Register * raReg = cg->allocateRegister();
   TR_ASSERT( inputNode->getDataType() == TR::Int64 || inputNode->getDataType() == TR::Int32, "error");
   TR_ASSERT( !isLong || cg->comp()->target().is64Bit(), "Not supported");

   TR::Instruction *cursor;

   TR::RegisterDependencyConditions * dependencies;
   dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg);
   dependencies->addPostCondition(inputReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(baseReg, TR::RealRegister::GPR2);  // for helper
   dependencies->addPostCondition(workReg, TR::RealRegister::GPR1);  // for helper
   dependencies->addPostCondition(countReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(raReg, cg->getReturnAddressRegister());

   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
   TR::LabelSymbol * labelDigit1 = generateLabelSymbol(cg);

   generateRIInstruction(cg, TR::InstOpCode::CHI, node, countReg, (int32_t) 1);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BE, node, labelDigit1);

   // At first, load up the branch address into raReg, because
   // to ensure that no weird spilling happens if the code decides it needs
   // to allocate a register at this point for the literal pool base address.
   intptr_t helper = (intptr_t) cg->symRefTab()->findOrCreateRuntimeHelper(TR_S390long2StringHelper,false,false,false)->getMethodAddress();
   genLoadAddressConstant(cg, node, helper, raReg);

   TR::MemoryReference * workTopMR = generateS390MemoryReference(workReg, 0, cg);
   if (isLong)
      {
      generateRXInstruction(cg, TR::InstOpCode::CVDG, node, inputReg, workTopMR);
      }
   else
      {
      TR::MemoryReference * workTopMR2 = generateS390MemoryReference(workReg, 0, cg);
      TR::MemoryReference * work8MR = generateS390MemoryReference(workReg, 8, cg);
      generateSS1Instruction(cg, TR::InstOpCode::XC, node, 7, workTopMR, workTopMR2);
      generateRXInstruction(cg, TR::InstOpCode::CVD, node, inputReg, work8MR);
      }

   generateRSInstruction(cg, TR::InstOpCode::SLL, node, countReg, 4);

   // call helper (UNPKU)
   TR::MemoryReference * targetMR = new (cg->trHeapMemory()) TR::MemoryReference(raReg, countReg, 0, cg);
   generateRXInstruction(cg, TR::InstOpCode::BAS, node, raReg, targetMR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);

   //
   // Special case
   //
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelDigit1);
   TR::MemoryReference * outMR = generateS390MemoryReference(baseReg, 0, cg);
   generateRIInstruction(cg, TR::InstOpCode::getLoadHalfWordImmOpCode(), node, countReg, 48);
   generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, countReg, inputReg);
   generateRXInstruction(cg, TR::InstOpCode::STH, node, countReg, outMR);
   outMR->stopUsingMemRefRegister(cg);
   // end

   cg->stopUsingRegister(inputReg);
   cg->stopUsingRegister(baseReg);
   cg->stopUsingRegister(workReg);
   cg->stopUsingRegister(countReg);
   targetMR->stopUsingMemRefRegister(cg);

   //
   // End
   //
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   cg->decReferenceCount(inputNode);
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));
   cg->decReferenceCount(node->getChild(3));
   return NULL;
   }

bool isIdentityOp(uint8_t byteValue, TR::InstOpCode::Mnemonic SI_opcode)
   {
   if (SI_opcode == TR::InstOpCode::NI && byteValue == 255)
      return true;
   else if ((SI_opcode == TR::InstOpCode::XI || SI_opcode == TR::InstOpCode::OI) && byteValue == 0)
      return true;
   else
      return false;
   }

bool isClearingOp(uint8_t byteValue, TR::InstOpCode::Mnemonic SI_opcode)
   {
   if (SI_opcode == TR::InstOpCode::NI && byteValue == 0)
      return true;
   else
      return false;
   }

bool isSettingOp(uint8_t byteValue, TR::InstOpCode::Mnemonic SI_opcode)
   {
   if (SI_opcode == TR::InstOpCode::OI && byteValue == 255)
      return true;
   else
      return false;
   }

TR::Register *
OMR::Z::TreeEvaluator::bitpermuteEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *value = node->getChild(0);
   TR::Node *addr = node->getChild(1);
   TR::Node *length = node->getChild(2);

   auto valueReg = cg->evaluate(value);
   auto addrReg = cg->evaluate(addr);

   TR::Register *tmpReg = cg->allocateRegister(TR_GPR);

   // The bitpermuteEvaluator uses loop unrolling to do the vector bit permute operation required by the IL
   // However, the bitPermuteConstantUnrollThreshold constant defined below defines the cap for this technique
   // (i.e. the maximum array size for which this implementation can be used).
   //
   // Currently it is set at 4. The reasons are as follows:
   //
   // 1. This technique scales linearly as the array size grows and is also expensive for the iCache.
   // 2. Additionally, on z14 and newer hardware, the vector implementation performs better than the loop unrolling
   //    technique for arrays of size > 4. Moreover, if a slightly modified version of the IL was available (better
   //    suited for BigEndian architectures) then the Vector implementation is on par or better than the loop
   //    unrolling technique for all array sizes.
   //
   // More info on the measurements is available here: https://github.com/eclipse/omr/pull/2330#issuecomment-378380538
   // The differences between the generated code for the different
   // implementations can be seen here: https://github.com/eclipse/omr/pull/2330#issuecomment-378387516
   static const int8_t bitPermuteConstantUnrollThreshold = 4;

   bool isLoadConst = length->getOpCode().isLoadConst();
   int32_t arrayLen = isLoadConst ? length->getIntegerNodeValue<int32_t>() : 0;

   TR::Register *resultReg = cg->allocateRegister(TR_GPR);

   if (isLoadConst && arrayLen <= bitPermuteConstantUnrollThreshold)
      {
      // Manage the constant length case
      generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, resultReg, resultReg);

      for (int32_t x = 0; x < arrayLen; ++x)
         {
         // Load the shift amount into tmpReg
         TR::MemoryReference *sourceMR = generateS390MemoryReference(addrReg, x, cg, NULL);
         generateRXInstruction(cg, TR::InstOpCode::LGB, node, tmpReg, sourceMR);

         // Create memory reference using tmpReg (which holds the shift amount), then shift valueReg by the shift amount
         TR::MemoryReference *shiftAmountMR = generateS390MemoryReference(tmpReg, 0, cg);
         generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, tmpReg, valueReg, shiftAmountMR);
         if (node->getDataType() == TR::Int64)
            {
            // This will generate a RISBG instruction (if it's supported, otherwise two shift instructions).
            // A RISBG instruction is equivalent to doing a `(tmpReg & 0x1) << x`. But for a 64-bit value we would have to use
            // two AND immediate instructions and a shift instruction to do this. So instead we use a single RISBG instruction.
            generateShiftThenKeepSelected64Bit(node, cg, tmpReg, tmpReg, 63 - x, 63 - x, x);
            }
         else
            {
            // Same as above, but generate a RISBLG instead of RISBG for 32, 16, and 8-bit integers
            generateShiftThenKeepSelected31Bit(node, cg, tmpReg, tmpReg, 31 - x, 31 - x, x);
            }

         // Now OR the result into the resultReg
         generateRRInstruction(cg, TR::InstOpCode::getOrRegOpCode(), node, resultReg, tmpReg);
         }
      }
   // Use z14's VBPERM instruction if possible. (Note: VBPERM supports permutation on arrays
   // of up to size 16, and beats the performance of the loop unrolling technique used above
   // for arrays with size greater than the bitPermuteConstantUnrollThreshold constant defined above)
   else if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z14) &&
         isLoadConst  &&
         arrayLen <= 16 )
      {
      char mask[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
      char inverse[16] = {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63};

      TR::Register *vectorIndices = cg->allocateRegister(TR_VRF);
      TR::Register *vectorSource = cg->allocateRegister(TR_VRF);
      TR::Register *tmpVector = cg->allocateRegister(TR_VRF);

      // load the value to permute into a vector register
      generateRILInstruction(cg, TR::InstOpCode::LGFI, node, resultReg, 0, NULL);
      TR::MemoryReference *vectorSourceMR = generateS390MemoryReference(resultReg, 0, cg, NULL);
      generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, vectorSource, valueReg, vectorSourceMR, 3);

      // load the array of indices into a vector register
      TR::MemoryReference *vectorIndicesMR = generateS390MemoryReference(addrReg, 0, cg, NULL);
      generateVSIInstruction(cg, TR::InstOpCode::VLRL, node, vectorIndices, vectorIndicesMR, 15);

      // Since we are on BigEndian architecture, bit positions in a register are numbered left to right. Hence
      // the most significant byte in the array is loaded into the right most byte (i.e least significant byte in the register)
      // The VPERM instruction below uses a mask to reverse the order of the array so we can do the computation correctly.
      TR::MemoryReference *maskAddrMR = generateS390MemoryReference(cg->findOrCreateConstant(node, mask, 16), cg, 0, node);
      generateVSIInstruction(cg, TR::InstOpCode::VLRL, node, tmpVector, maskAddrMR, 15);
      generateVRReInstruction(cg, TR::InstOpCode::VPERM, node, vectorIndices, vectorIndices, vectorIndices, tmpVector, 0, 0);

      // Registers on the Z hardware have their bit and byte positions numbered from left to right in registers. Thus, a bit position
      // of 2 specified by the bitPermute IL is equivalent to 63 - 2 = 61 on Z. Thus the VS instruction below does a "packed subtraction" to
      // subtract all bit indices by 63.
      TR::MemoryReference *inverseAddrMR = generateS390MemoryReference(cg->findOrCreateConstant(node, inverse, 16), cg, 0, node);
      generateVSIInstruction(cg, TR::InstOpCode::VLRL, node, tmpVector, inverseAddrMR, 15);
      generateVRRcInstruction(cg, TR::InstOpCode::VS, node, vectorIndices, tmpVector, vectorIndices, 0);

      // The VBPERM instruction now maps perfectly to the IL, so do the operation.
      generateVRRcInstruction(cg, TR::InstOpCode::VBPERM, node, vectorSource, vectorSource, vectorIndices, 0);

      generateRILInstruction(cg, TR::InstOpCode::LGFI, node, resultReg, 3, NULL);

      // load result back into GPR
      TR::MemoryReference *loadBackToGPRMR = generateS390MemoryReference(resultReg, 0, cg, NULL);
      generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, resultReg, vectorSource, loadBackToGPRMR, 1);

      // The result of the VBPERM op is now in resultReg's 48-63 bit locations. Extract the bits you want via a mask
      // and store the final result in resultReg. This is necessary because VBPERM operates on 16 bit positions at a time.
      // If the array specified by the bitPermute IL was smaller than 16, then invalid bits can be selected.
      int32_t resultMask = (1 << arrayLen) - 1;

      generateRIInstruction(cg, TR::InstOpCode::NILL, node, resultReg, resultMask);

      cg->stopUsingRegister(vectorIndices);
      cg->stopUsingRegister(vectorSource);
      cg->stopUsingRegister(tmpVector);
      }
   else
      {
      auto lengthReg = cg->evaluate(length);
      auto indexReg = cg->allocateRegister(TR_GPR);

      TR::RegisterDependencyConditions *deps = generateRegisterDependencyConditions(0, 4, cg);
      deps->addPostCondition(addrReg, TR::RealRegister::AssignAny);
      deps->addPostCondition(indexReg, TR::RealRegister::AssignAny);
      deps->addPostCondition(valueReg, TR::RealRegister::AssignAny);
      deps->addPostCondition(tmpReg, TR::RealRegister::AssignAny);

      TR::LabelSymbol *cFlowRegionStart = generateLabelSymbol(cg);
      TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);

      generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, indexReg, indexReg);
      generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, resultReg, resultReg);

      // Load array length into index and test to see if it's already zero by checking the Condition Code (CC)
      // CC=0 if register value is 0, CC=1 if value < 1, CC=2 if value>0
      generateRRInstruction(cg, TR::InstOpCode::LTR, node, indexReg, lengthReg);

      // Start of internal control flow
      generateS390LabelInstruction(cg,TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();

      // Now subtract 1 from indexReg
      generateRIInstruction(cg, TR::InstOpCode::AHI, node, indexReg, -1);
      // Conditionally jump to end of control flow if index is less than 0.
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, cFlowRegionEnd);

      // Load the bit index into tmpReg
      TR::MemoryReference *sourceMR = generateS390MemoryReference(addrReg, indexReg, 0, cg);
      generateRXInstruction(cg, TR::InstOpCode::LGB, node, tmpReg, sourceMR);

      // Shift value reg by location in shiftAmountMR and store in tmpReg
      TR::MemoryReference *shiftAmountMR = generateS390MemoryReference(tmpReg, 0, cg);
      generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, tmpReg, valueReg, shiftAmountMR);

      if (node->getDataType() == TR::Int64)
         {
         // This will generate a RISBG instruction (if supported).
         // This is equivalent to doing a `tmpReg & 0x1`. But on 64-bit we would have to use
         // two AND immediate instructions. So instead we use a single RISBG instruction.
         generateShiftThenKeepSelected64Bit(node, cg, tmpReg, tmpReg, 63, 63, 0);
         }
      else
         {
         // Now AND the value in tmpReg by 1 to get the relevant bit
         generateRILInstruction(cg, TR::InstOpCode::NILF, node, tmpReg,0x1);
         }
      // Now shift your bit to the appropriate position beforing ORing it to the result register
      // and conditionally jumping back up to the start of internal control flow
      shiftAmountMR = generateS390MemoryReference(indexReg, 0, cg);
      generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, tmpReg, tmpReg, shiftAmountMR);

      generateRRInstruction(cg, TR::InstOpCode::getOrRegOpCode(), node, resultReg, tmpReg);

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, cFlowRegionStart);

      // Generate cFlowRegionEnd Instruction here:
      generateS390LabelInstruction(cg,TR::InstOpCode::LABEL, node, cFlowRegionEnd, deps);
      cFlowRegionEnd->setEndInternalControlFlow();

      cg->stopUsingRegister(indexReg);
      }

   cg->stopUsingRegister(tmpReg);

   node->setRegister(resultReg);
   cg->decReferenceCount(value);
   cg->decReferenceCount(addr);
   cg->decReferenceCount(length);

   return resultReg;
   }

/**
 * bitOpMemEvaluator - bit operations (OR, AND, XOR) for memory to memory
 *
 * This evaluator will generate the following code (e.g. bitOp = OR "|").
 * if (node->getNumChildren() == 3)
 *    {
 *    // Dest, Src1, Len ========>  Dest |= Src1
 *    OC    Dest, Src1, Len
 *    }
 * else
 *    {
 *    // Dest, Src1, Src2, Len ==>  Dest = Src1 | Src2
 *    // IL transformer guarantees the destination NEVER overlaps to src1 or src2.
 *    MVC   Dest, Src2, Len
 *    OC    Dest, Src1, Len
 *    }
 * handles bitOpMem BIFs
 */
TR::Register *OMR::Z::TreeEvaluator::bitOpMemEvaluator(TR::Node * node, TR::CodeGenerator * cg)
   {
   TR_FoldType foldType = TR_FoldNone;
   TR::Node *parent = NULL;

   TR::Node * byteSrc1Node = NULL;
   TR::Node * byteSrc2Node = NULL;
   TR::Node * byteDstNode = NULL;
   TR::Node * byteLenNode = NULL;

   TR::Register * byteSrc1Reg = NULL;
   TR::Register * byteSrc2Reg = NULL;
   TR::Register * byteDstReg = NULL;
   TR::Register * byteLenReg = NULL;

   TR::Register *backupDstReg = NULL;
   TR::Register *backupLenReg = NULL;
   bool evaluateChildren = true;
   bool isSimpleBitOpMem = node->getNumChildren() == 3;
   bool lenMinusOne=false;

   TR::Node *aggrChild1 = NULL;
   TR::Node *aggrChild2 = NULL;
   TR::Compilation *comp = cg->comp();

   byteDstNode = node->getChild(0);
   byteSrc1Node = node->getChild(1);
   if (isSimpleBitOpMem)
      {
      byteLenNode = node->getChild(2);
      }
   else
      {
      byteSrc2Node = node->getChild(2);
      byteLenNode = node->getChild(3);

      byteSrc2Reg = cg->gprClobberEvaluate(byteSrc2Node);
      }

   bool isAnd  =  node->isAndBitOpMem();
   bool isOr   =  node->isOrBitOpMem();
   bool isXor  =  node->isXorBitOpMem();

   int64_t constantByteLength = 0;
   if (byteLenNode->getOpCode().isLoadConst())
      constantByteLength = byteLenNode->get64bitIntegralValue();

   // see if we can generate immediate instructions
   TR::InstOpCode::Mnemonic SI_opcode = isXor ? TR::InstOpCode::XI :
                              isAnd ? TR::InstOpCode::NI :
                              isOr  ? TR::InstOpCode::OI : TR::InstOpCode::BAD;
   bool useSIFormat = false;
   char *value = NULL;

   TR::MemoryReference *aggrDestMR = NULL;
   TR::MemoryReference *aggr2ndMR = NULL;
   if (byteLenNode->getOpCode().isLoadConst())
      {
      if (!comp->getOption(TR_DisableSSOpts) &&
          (constantByteLength <= 256))
         {
         evaluateChildren = false;
         }
      }

   if (useSIFormat)
      {
      evaluateChildren = false;
      // generate immediate instructions byte by byte

      TR::MemoryReference * tempMR =  generateS390MemoryReference(cg, node, byteDstNode, 0, true);

      for (int32_t i = 0; i < constantByteLength; i++)
         {
         uint8_t byteValue=*(uint8_t*)(value+i);
         // do not OR or XOR 0000's or AND FFFF's
         if (!isIdentityOp(byteValue, SI_opcode))
            {
            TR::MemoryReference * tempMRbyte;
            if (i==0)
               tempMRbyte=tempMR;
            else
               tempMRbyte=generateS390MemoryReference(*tempMR, i, cg);
            TR::InstOpCode::Mnemonic opcode = SI_opcode;
            if (isClearingOp(byteValue, SI_opcode))
               {
               opcode = TR::InstOpCode::MVI;
               byteValue = 0;
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tuse MVI 0 for clearing op %s (%p): value[%d] = 0x%x\n",node->getOpCode().getName(),node,i,value[i]);
               }
            else if (isSettingOp(byteValue, SI_opcode))
               {
               opcode = TR::InstOpCode::MVI;
               byteValue = 0xFF;
               if (cg->traceBCDCodeGen())
                  traceMsg(comp,"\tuse MVI 0xFF for setting op %s (%p): value[%d] = 0x%x\n",node->getOpCode().getName(),node,i,value[i]);
               }
            TR::Instruction * cursor = generateSIInstruction(cg, opcode, node, tempMRbyte, byteValue);
            }
         }
      if (byteSrc1Node)
         cg->decReferenceCount(byteSrc1Node);
      }
   else
      {
      if (evaluateChildren)
         byteDstReg = cg->evaluate(byteDstNode);
      // Backup some registers and execute MVC if isSimpleBitOpMem is false
      if (isSimpleBitOpMem)
         {
         if (!byteLenNode->getOpCode().isLoadConst())
            {
            byteLenReg =cg->evaluateLengthMinusOneForMemoryOps(byteLenNode, true, lenMinusOne);
            }

         if (evaluateChildren && !cg->canClobberNodesRegister(byteDstNode))
            {
            backupDstReg = cg->allocateClobberableRegister(byteDstReg);
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, backupDstReg, byteDstReg);
            }
         }
      else
         {
         // MVC part
         // Copy from Src2 to Dest
         // IL transformer guarantees the destination NEVER overlaps to src1 or src2.
         backupDstReg = cg->allocateClobberableRegister(byteDstReg);
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, backupDstReg, byteDstReg);
         if (!byteLenNode->getOpCode().isLoadConst())
            {
            byteLenReg = cg->evaluate(byteLenNode);
            backupLenReg = cg->allocateClobberableRegister(byteLenReg);

            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, backupLenReg, byteLenReg);
            MemCpyVarLenMacroOp op(node, byteDstNode, byteSrc2Node, cg, byteLenReg, byteLenNode);
            op.generate(byteDstReg, byteSrc2Reg);
            generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, byteLenReg, backupLenReg);
            }
         else
            {
            MemCpyConstLenMacroOp op(node, byteDstNode, byteSrc2Node, cg, constantByteLength);
            op.generate(byteDstReg, byteSrc2Reg);
            }
         TR_ASSERT( backupDstReg, "error");
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, byteDstReg, backupDstReg);
         }

         // bitOpMem part
         if (evaluateChildren)
            byteSrc1Reg = cg->gprClobberEvaluate(byteSrc1Node);
         TR::InstOpCode::Mnemonic opcode = isXor ? TR::InstOpCode::XC :
                                 isAnd ? TR::InstOpCode::NC :
                                 isOr  ? TR::InstOpCode::OC : TR::InstOpCode::BAD;

         if (byteLenReg == NULL)
            {
            BitOpMemConstLenMacroOp op(node, byteDstNode, byteSrc1Node, cg, opcode, constantByteLength);
            op.generate(byteDstReg, byteSrc1Reg);
            }
         else
            {
            BitOpMemVarLenMacroOp op(node, byteDstNode, byteSrc1Node, cg, opcode, byteLenReg, byteLenNode, lenMinusOne);
            op.setUseEXForRemainder(true);
            op.generate(byteDstReg, byteSrc1Reg);
            }
      }
   // Restore registers
   if (backupDstReg)
      {
      if (!cg->canClobberNodesRegister(byteDstNode))
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, byteDstReg, backupDstReg);
      cg->stopUsingRegister(backupDstReg);
      }
   if (backupLenReg)
      {
      if (!cg->canClobberNodesRegister(byteLenNode))
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, byteLenReg, backupLenReg);
      cg->stopUsingRegister(backupLenReg);
      }

   if (byteLenReg) cg->stopUsingRegister(byteLenReg);
   if (byteSrc1Reg) cg->stopUsingRegister(byteSrc1Reg);
   if (byteDstReg)cg->stopUsingRegister(byteDstReg);
   if (!isSimpleBitOpMem)
      {
      cg->stopUsingRegister(byteSrc2Reg);

      cg->decReferenceCount(byteSrc2Node);
      }

   if (byteSrc1Reg!=NULL) cg->decReferenceCount(byteSrc1Node);
   if (byteDstReg!=NULL) cg->decReferenceCount(byteDstNode);
   if (byteLenNode) cg->decReferenceCount(byteLenNode);

   return TR::TreeEvaluator::getConditionCodeOrFold(node, cg, foldType, parent);
   }

TR::Register *OMR::Z::TreeEvaluator::libmFuncEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (node->getOpCode().isFloat())
      TR::Node::recreate(node, TR::fcall);
   else if (node->getOpCode().isDouble())
      TR::Node::recreate(node, TR::dcall);
   else
      TR_ASSERT(false, "Invalid type for libmFuncEvaluator.\n");

   TR::Register *trgReg = TR::TreeEvaluator::directCallEvaluator(node, cg);
   return trgReg;
   }

TR::Register *OMR::Z::TreeEvaluator::PrefetchEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT(node->getNumChildren() == 4, "TR::Prefetch should contain 4 children nodes");

   TR::Node  *firstChild  =  node->getFirstChild();
   TR::Node  *secondChild =  node->getChild(1);
   TR::Node  *sizeChild =  node->getChild(2);
   TR::Node  *typeChild =  node->getChild(3);

   TR::Compilation *comp = cg->comp();

   static char * disablePrefetch = feGetEnv("TR_DisablePrefetch");
   if (!cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10) || disablePrefetch)
      {
      cg->recursivelyDecReferenceCount(firstChild);
      cg->recursivelyDecReferenceCount(secondChild);
      cg->recursivelyDecReferenceCount(sizeChild);
      cg->recursivelyDecReferenceCount(typeChild);
      return NULL;
      }

   cg->recursivelyDecReferenceCount(sizeChild);

   // Type
   uint32_t type = typeChild->getInt();
   cg->recursivelyDecReferenceCount(typeChild);

   uint8_t memAccessMode = 0;

   if (type == PrefetchLoad)
      {
      memAccessMode = 1;
      }
   else if (type == PrefetchStore)
      {
      memAccessMode = 2;
      }
   else if (type == ReleaseStore)
      {
      memAccessMode = 6;
      }
   else if (type == ReleaseAll)
      {
      memAccessMode = 7;
      }
   else
      {
      traceMsg(comp,"Prefetching for type %d not implemented/supported on 390.\n",type);
      }

   if (memAccessMode)
      {
      TR::MemoryReference * memRef = NULL;

      if (secondChild->getOpCode().isLoadConst())
         {
         // Offset Node is a constant.
         TR::Register * firstChildReg = cg->evaluate(firstChild);
         uint32_t offset = secondChild->getInt();
         memRef = generateS390MemoryReference(firstChildReg, offset, cg);

         cg->decReferenceCount(firstChild);
         cg->recursivelyDecReferenceCount(secondChild);
         }
      else
         {
         TR::Register *baseReg = cg->evaluate(firstChild);
         TR::Register *indexReg = cg->evaluate(secondChild);
         memRef = generateS390MemoryReference(baseReg, indexReg, 0, cg);

         cg->decReferenceCount(firstChild);
         cg->decReferenceCount(secondChild);
         }
      generateRXInstruction(cg, TR::InstOpCode::PFD, node, memAccessMode, memRef);
      }
   else
      {
      cg->recursivelyDecReferenceCount(firstChild);
      cg->recursivelyDecReferenceCount(secondChild);
      }

   return NULL;
   }

bool
TR_S390ComputeCC::setCarryBorrow(TR::Node *flagNode, bool invertValue, TR::CodeGenerator *cg)
   {
   TR::Register *flagReg = NULL;

   // do nothing, except evaluate child
   flagReg = cg->evaluate(flagNode);
   cg->decReferenceCount(flagNode);
   return true;
   }

void
TR_S390ComputeCC::computeCC(TR::Node *node, TR::Register *ccReg, TR::CodeGenerator *cg)
   {
   getConditionCode(node, cg, ccReg);
   }

void
TR_S390ComputeCC::computeCCLogical(TR::Node *node, TR::Register *ccReg, TR::Register *targetReg, TR::CodeGenerator *cg, bool is64Bit)
   {
   generateRRInstruction(cg, TR::InstOpCode::XR, node, ccReg, ccReg);
   generateRRInstruction(cg, !is64Bit ? TR::InstOpCode::CLR : TR::InstOpCode::CLGFR, node, targetReg, ccReg);
   generateRRInstruction(cg, TR::InstOpCode::ALCR, node, ccReg, ccReg);
   }

void
TR_S390ComputeCC::saveHostCC(TR::Node *node, TR::Register *ccReg, TR::CodeGenerator *cg)
   {
   generateRRInstruction(cg, TR::InstOpCode::IPM, node, ccReg, ccReg);
   generateRIEInstruction(cg, TR::InstOpCode::RISBG, node, ccReg, ccReg, 60, 63|0x80, 36);
   }

TR::Register *OMR::Z::TreeEvaluator::loadAutoOffsetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *reg = cg->allocateRegister();
   TR::MemoryReference *mr = generateS390MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::GPR0), 0, cg);
   mr->setSymbolReference(node->getSymbolReference());
   generateRXInstruction(cg, TR::InstOpCode::LA, node, reg, mr);
   cg->stopUsingRegister(reg);
   return reg;
   }

/**
 * Determines the appropriate branch mask to use for a BRC following a TM/TMLL
 * resulting from a butest.
 *
 * The boolean is important, because in the source executable we expect a TM
 * which produces condition codes 0,1,3.  However, we may end up generating
 * a TMLL, which produces condition codes 0,1,2,3.  From the POPS:
 *
 *   CondCode          TM                      TMLL
 *      0         All bits 0               All bits 0
 *      1         Bits mixed 0/1           Bits mixed 0/1 and leftmost bit 0
 *      2             N/A                  Bits mixed 0/1 and leftmost bit 1
 *      3         All bits 1               All bits 1
 *
 * As you can see, this function needs to make sure that whatever branch test
 * that *would have* been acceptable give TM style condition codes correctly
 * captures the (previously impossible) possibility of a 2 CC from TMLL.
 *
 * E.g.  If we had a TM that wanted to branch iff all bits were 1, it is possible that
 *       we got as input:    TM   D1(B1), <mask>
 *                           BRC  3, Label
 *
 * corresponding to il:
 *                ificmpgt
 *                    butest
 *                    iconst 1
 *
 * BUT TMLL could produce CC=2, which would cause us to naively branch.  Catastrophe!
 */
TR::InstOpCode::S390BranchCondition getButestBranchCondition(TR::ILOpCodes opCode, int32_t cmpValue)
   {

   TR::InstOpCode::S390BranchCondition TMtoTMLLBranchMaskTable[6][4] =
         {
                                 /* Condition Code Value */
    /* branch opcode*/  /*        0            1            2            3       */
    /* TR::ifiucmpeq */     { TR::InstOpCode::COND_MASK8 , TR::InstOpCode::COND_MASK6 , TR::InstOpCode::COND_MASK0 ,  TR::InstOpCode::COND_MASK1  },
    /* TR::ifiucmpne */     { TR::InstOpCode::COND_MASK7 , TR::InstOpCode::COND_MASK9 , TR::InstOpCode::COND_MASK15,  TR::InstOpCode::COND_MASK14 },
    /* TR::ifiucmpgt */     { TR::InstOpCode::COND_MASK7 , TR::InstOpCode::COND_MASK1 , TR::InstOpCode::COND_MASK1 ,  TR::InstOpCode::COND_MASK0  },
    /* TR::ifiucmpge */     { TR::InstOpCode::COND_MASK15, TR::InstOpCode::COND_MASK7 , TR::InstOpCode::COND_MASK1 ,  TR::InstOpCode::COND_MASK1  },
    /* TR::ifiucmplt */     { TR::InstOpCode::COND_MASK0 , TR::InstOpCode::COND_MASK8 , TR::InstOpCode::COND_MASK14,  TR::InstOpCode::COND_MASK14 },
    /* TR::ifiucmple */     { TR::InstOpCode::COND_MASK8 , TR::InstOpCode::COND_MASK14, TR::InstOpCode::COND_MASK14,  TR::InstOpCode::COND_MASK15 }
         };

   int32_t rowNum = -1;
   TR_ASSERT(cmpValue >= 0 && cmpValue <= 3, "Unexpected butest cc test value %d\n", cmpValue);
   switch(opCode)
         {
         case TR::ificmpeq: rowNum = 0; break;
         case TR::ificmpne: rowNum = 1; break;
         case TR::ifiucmpgt: rowNum = 2; break;
         case TR::ifiucmpge: rowNum = 3; break;
         case TR::ifiucmplt: rowNum = 4; break;
         case TR::ifiucmple: rowNum = 5; break;
         default:
            TR_ASSERT(0, "Unexpected opcode [%d] for butestBranchCondition\n", opCode); break;
         }

   return  TMtoTMLLBranchMaskTable[rowNum][cmpValue];
   }

TR::InstOpCode::S390BranchCondition getStandardIfBranchCondition(TR::ILOpCodes opCode, int64_t compareVal)
   {
   TR::InstOpCode::S390BranchCondition brCond = TR::InstOpCode::COND_NOP;
   switch(opCode)
      {
      case TR::ificmpeq:
         switch(compareVal)
            {
            case 0: brCond = TR::InstOpCode::COND_MASK8; break;
            case 1: brCond = TR::InstOpCode::COND_MASK4; break;
            case 2: brCond = TR::InstOpCode::COND_MASK2; break;
            case 3: brCond = TR::InstOpCode::COND_MASK1; break;
            default: break;
            }
         break;
      case TR::ificmpne:
         switch(compareVal)
            {
            case 0: brCond = TR::InstOpCode::COND_MASK7; break;
            case 1: brCond = TR::InstOpCode::COND_MASK11; break;
            case 2: brCond = TR::InstOpCode::COND_MASK13; break;
            case 3: brCond = TR::InstOpCode::COND_MASK14; break;
            default: break;
            }
         break;
      case TR::ificmpgt:
         switch(compareVal)
            {
            case 0: brCond = TR::InstOpCode::COND_MASK7; break;
            case 1: brCond = TR::InstOpCode::COND_MASK3; break;
            case 2: brCond = TR::InstOpCode::COND_MASK1; break;
            case 3: brCond = TR::InstOpCode::COND_MASK0; break;
            default: break;
            }
         break;
      case TR::ificmpge:
         switch(compareVal)
            {
            case 0: brCond = TR::InstOpCode::COND_MASK15; break;
            case 1: brCond = TR::InstOpCode::COND_MASK7; break;
            case 2: brCond = TR::InstOpCode::COND_MASK3; break;
            case 3: brCond = TR::InstOpCode::COND_MASK1; break;
            default: break;
            }
         break;
      case TR::ificmplt:
         switch(compareVal)
            {
            case 0: brCond = TR::InstOpCode::COND_MASK0; break;
            case 1: brCond = TR::InstOpCode::COND_MASK8; break;
            case 2: brCond = TR::InstOpCode::COND_MASK12; break;
            case 3: brCond = TR::InstOpCode::COND_MASK14; break;
            default: break;
            }
         break;
      case TR::ificmple:
         switch(compareVal)
            {
            case 0: brCond = TR::InstOpCode::COND_MASK8; break;
            case 1: brCond = TR::InstOpCode::COND_MASK12; break;
            case 2: brCond = TR::InstOpCode::COND_MASK14; break;
            case 3: brCond = TR::InstOpCode::COND_MASK15; break;
            default: break;
            }
         break;
      default: break;
      }
   return brCond;
   }

#define MAX_PADDING_SIZE  256

/**
 * arraycmpWithPadHelper handles MemcmpWithPad and arraycmpWithPad
 */
arraycmpWithPadHelper::arraycmpWithPadHelper(TR::Node *n,
                                             TR::CodeGenerator *codegen,
                                             bool utf,
                                             bool isFoldedIf_p,
                                             TR::Node *ificmpNode,
                                             bool needResultReg,
                                             TR::InstOpCode::S390BranchCondition *returnCond_p) :
   PADDING_THRESHOLD(496), CLC_THRESHOLD(4096), CLCL_THRESHOLD(16777216)
   {
   isUTF16 = utf;
   node = n;
   cg = codegen;
   isFoldedIf = isFoldedIf_p;
   ificmpNode_ = ificmpNode;
   returnCond = returnCond_p;

   //genResultReg is in effect only if isFoldedIf is false.
   genResultReg = !isFoldedIf_p && needResultReg;

   if(isFoldedIf)
      {
      cmpVal = ificmpNode->getChild(1)->getIntegerNodeValue<int>();

      cmpTargetLabel = ificmpNode->getBranchDestination()->getNode()->getLabel();
      }

   source1Node = node->getChild(0);
   source1LenNode = node->getChild(1);
   source2Node = node->getChild(2);
   source2LenNode = node->getChild(3);
   paddingNode = node->getChild(4);

   if (node->getOpCodeValue() == TR::arraycmpWithPad)
      {
      maxLen = INT_MAX;
      isEqual = 0;
      if ((source1LenNode->getOpCode().isLoadConst() &&
           source2LenNode->getOpCode().isLoadConst() &&
           source1LenNode->getIntegerNodeValue<int64_t>() ==  source2LenNode->getIntegerNodeValue<int64_t>()) ||
          (source1LenNode == source2LenNode))
         {
         isEqual = 1;
         }
      }
   else //it is MemcmpWithPad
      {
      TR::Node *maxLenNode = node->getChild(5);
      TR::Node *equalNode = node->getChild(6);

      maxLen = maxLenNode->getIntegerNodeValue<int32_t>();
      cg->decReferenceCount(maxLenNode);
      isEqual = equalNode->getIntegerNodeValue<int32_t>();
      cg->decReferenceCount(equalNode);
      }

   source1Reg = NULL;
   source2Reg = NULL;
   source1LenReg = NULL;
   source2LenReg = NULL;
   paddingReg = NULL;
   source1PairReg = NULL;
   source2PairReg = NULL;
   retValReg = NULL;
   litPoolBaseReg = NULL;
   newLitPoolReg = NULL;
   paddingSrcReg = NULL;
   loopCountReg = NULL;
   countReg = NULL;
   paddingPosReg = NULL;
   paddingLenReg = NULL;
   paddingUnequalRetValReg = NULL;

   regDeps = NULL;
   branchDeps = NULL;
   unequalLabel = NULL;
   doneLabel = NULL;
   remainEndLabel = NULL;
   isStartInternalControlFlowSet = false;

   isLen1Const = false;
   isLen2Const = false;
   isAddr1Const = false;
   isAddr2Const = false;
   isPaddingSrcAddrConst = false;
   isConst = false;
   forceCLCL = false;
   constSpacePadding = false;
   CLCLpaddingChar = '\0';

   isSingleTarget = false;
   skipUnequal = false;
   brCond = TR::InstOpCode::COND_NOP;
   finalBrCond = TR::InstOpCode::COND_NOP;

   recDecSource1Node = false;
   recDecSource2Node = false;
   }

void arraycmpWithPadHelper::setupCLCLandCLCLE()
   {
   source1LenReg = cg->gprClobberEvaluate(source1LenNode);
   if ((source1LenNode->getSize()) <= 4 && cg->comp()->target().is64Bit())
      {
      // Cleanup the high order part of the reg
      generateRRInstruction(cg, TR::InstOpCode::LLGFR, node, source1LenReg, source1LenReg);
      }

   source2LenReg = cg->gprClobberEvaluate(source2LenNode);
   if ((source2LenNode->getSize()) <= 4 && cg->comp()->target().is64Bit())
      {
      // Cleanup the high order part of the reg
      generateRRInstruction(cg, TR::InstOpCode::LLGFR, node, source2LenReg, source2LenReg);
      }

   paddingReg = cg->gprClobberEvaluate(paddingNode);

   source1PairReg = cg->allocateConsecutiveRegisterPair(source1LenReg, source1Reg);
   source2PairReg = cg->allocateConsecutiveRegisterPair(source2LenReg, source2Reg);

   regDeps = cg->createDepsForRRMemoryInstructions(node, source1PairReg, source2PairReg, 1); // extraDeps=1

   regDeps->addPostCondition(paddingReg, TR::RealRegister::AssignAny);

   branchDeps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode_);
   }

void arraycmpWithPadHelper::setupConstCLCL()
   {
   source1LenReg = cg->gprClobberEvaluate(source1LenNode);
   source2LenReg = cg->allocateRegister();
   source2Len = source2LenNode->getIntegerNodeValue<int>();
   CLCLpaddingChar = paddingNode->getIntegerNodeValue<char>();

   source1PairReg = cg->allocateConsecutiveRegisterPair(source1LenReg, source1Reg);
   source2PairReg = cg->allocateConsecutiveRegisterPair(source2LenReg, source2Reg);

   regDeps = cg->createDepsForRRMemoryInstructions(node, source1PairReg, source2PairReg);

   branchDeps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode_);
   }

TR::Register *arraycmpWithPadHelper::generateConstCLCL()
   {
   uint32_t lenPad = ((uint32_t)source2Len & 0x00FFFFFF) | ((uint32_t)CLCLpaddingChar << 24);
   if(cg->comp()->target().is64Bit())
      genLoadLongConstant(cg, node, lenPad, source2LenReg);
   else
      generateLoad32BitConstant(cg, node, lenPad, source2LenReg, true);

   TR::Instruction *cursor = NULL;
   cursor = generateRRInstruction(cg, TR::InstOpCode::CLCL, node, source1PairReg, source2PairReg);
   cursor->setDependencyConditions(regDeps);

   // This is paranoid ... should never be NULL
   if (branchDeps == NULL)
      {
      branchDeps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode_);
      }
   if(isFoldedIf)
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node, cmpTargetLabel, branchDeps);

      return NULL;
      }
   else
      {
      if (genResultReg)
         return getConditionCode(node, cg);
      else
         return NULL;
      }
   }

TR::Register *arraycmpWithPadHelper::generateCLCL()
   {
   if (cg->comp()->target().is64Bit())
      {
      if (cg->comp()->target().cpu.getSupportsArch(TR::CPU::z10))
         {
         generateRIEInstruction(cg, TR::InstOpCode::ROSBG, node, source2LenReg, paddingReg, 32, 39, 24);
         }
      else
         {
         generateRSInstruction(cg, TR::InstOpCode::SLLG, node, paddingReg, paddingReg, 24);
         generateRRInstruction(cg, TR::InstOpCode::OGR, node, source2LenReg, paddingReg);
         }
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SLL, node, paddingReg, 24);
      generateRRInstruction(cg, TR::InstOpCode::OR, node, source2LenReg, paddingReg);
      }
   cg->stopUsingRegister(paddingReg);

   TR::Instruction *cursor = NULL;
   cursor = generateRRInstruction(cg, TR::InstOpCode::CLCL, node, source1PairReg, source2PairReg);
   cursor->setDependencyConditions(regDeps);

   // This is paranoid ... should never be NULL
   if (branchDeps == NULL)
      {
      branchDeps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode_);
      }
   if(isFoldedIf)
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node, cmpTargetLabel, branchDeps);

      return NULL;
      }
   else
      {
      if (genResultReg)
         return getConditionCode(node, cg);
      else
         return NULL;
      }
   }

TR::Register *arraycmpWithPadHelper::generateCLCLE()
   {
   //CLCLE/CLCLU
   TR::MemoryReference *paddingRef = generateS390MemoryReference(paddingReg, 0, cg);

   TR::LabelSymbol *continueLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *cFlowRegionEnd = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, continueLabel, regDeps);
   continueLabel->setStartInternalControlFlow();

   generateRSInstruction(cg, isUTF16 ? TR::InstOpCode::CLCLU : TR::InstOpCode::CLCLE, node, source1PairReg, source2PairReg, paddingRef);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK1, node, continueLabel);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, regDeps);
   cFlowRegionEnd->setEndInternalControlFlow();

   cg->stopUsingRegister(paddingReg);

      // This is paranoid ... should never be NULL
   if (branchDeps == NULL)
      {
      branchDeps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode_);
      }
   if(isFoldedIf)
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node, cmpTargetLabel, branchDeps);

      return NULL;
      }
   else
      {
      if (genResultReg)
         return getConditionCode(node, cg);
      else
         return NULL;
      }
   }

void arraycmpWithPadHelper::teardownCLCLandCLCLE()
   {
   cg->stopUsingRegister(source1PairReg);
   cg->stopUsingRegister(source1Reg);
   cg->stopUsingRegister(source1LenReg);
   cg->stopUsingRegister(source2PairReg);
   cg->stopUsingRegister(source2Reg);
   cg->stopUsingRegister(source2LenReg);

   if (recDecSource1Node)
      cg->recursivelyDecReferenceCount(source1Node);
   else
      cg->decReferenceCount(source1Node);

   if (recDecSource2Node)
      cg->recursivelyDecReferenceCount(source2Node);
   else
      cg->decReferenceCount(source2Node);

   cg->decReferenceCount(source1LenNode);
   cg->decReferenceCount(source2LenNode);
   cg->decReferenceCount(paddingNode);
   }

void arraycmpWithPadHelper::generateConstCLCSetup()
   {
   int32_t dependencyCount = 0;

   source1Len = source1LenNode->getIntegerNodeValue<int>();
   cg->decReferenceCount(source1LenNode);
   source2Len = source2LenNode->getIntegerNodeValue<int>();
   cg->decReferenceCount(source2LenNode);
   count = (source1Len < source2Len) ? source1Len : source2Len;

   if (source1Len == 0)
      {
      if (source1Node->safeToDoRecursiveDecrement())
         recDecSource1Node = true;
      else
         cg->evaluate(source1Node);
      }
   else
      {
      dependencyCount += 1;
      }

   if (source2Len == 0)
      {
      if (source2Node->safeToDoRecursiveDecrement())
         recDecSource2Node = true;
      else
         cg->evaluate(source2Node);
      }
   else
      {
      dependencyCount += 1;
      }

   // This is paranoid ... should never be NULL
   if (branchDeps == NULL)
      {
      branchDeps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode_);
      }

   if(!isEqual && !constSpacePadding)
      {
      paddingReg = cg->gprClobberEvaluate(paddingNode);
      retValReg = cg->allocateRegister();
      }

   if(!isEqual)
      {
      if(constSpacePadding)
         {
         if(newLitPoolReg)
            dependencyCount += 1;
         }
      else
         dependencyCount += 2;
      }

   //May need two more regDeps for later use: in generateConstCLCRemainder and in generateCLCUnequal
   dependencyCount += 2;

   regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(branchDeps, 0, dependencyCount, cg);
   if(source1Len != 0 && source1Reg)
      regDeps->addPostCondition(source1Reg, TR::RealRegister::AssignAny);
   if(source2Len != 0 && source2Reg)
      regDeps->addPostCondition(source2Reg, TR::RealRegister::AssignAny);
   if(!isEqual)
      {
      if(constSpacePadding)
         {
         if(newLitPoolReg)
            regDeps->addPostCondition(litPoolBaseReg, TR::RealRegister::AssignAny);
         }
      else
         {
         regDeps->addPostCondition(retValReg, TR::RealRegister::AssignAny);
         regDeps->addPostCondition(paddingReg, TR::RealRegister::AssignAny);
         }
      }

   unequalLabel = generateLabelSymbol(cg);
   if(!isFoldedIf)
      brCond = TR::InstOpCode::COND_MASK6;
   }

void arraycmpWithPadHelper::generateVarCLCSetup()
   {
   source1LenReg = cg->gprClobberEvaluate(source1LenNode);
   source2LenReg = cg->gprClobberEvaluate(source2LenNode);
   // This is paranoid ... should never be NULL
   if (branchDeps == NULL)
      {
      branchDeps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode_);
      }
   if(isEqual)
      {
      countReg = source1LenReg;
      }
   else
      {
      paddingReg = cg->gprClobberEvaluate(paddingNode);

      //padding calculation
      countReg = cg->allocateRegister();
      paddingPosReg = cg->allocateRegister();
      paddingUnequalRetValReg = cg->allocateRegister();

      TR::RegisterDependencyConditions *regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 6, cg);
      regDeps->addPostCondition(source1LenReg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(source2LenReg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(source1Reg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(source2Reg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(countReg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(paddingPosReg, TR::RealRegister::AssignAny);

      generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, countReg, source1LenReg);
      generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, paddingPosReg, source2Reg);
      if (cg->comp()->target().is64Bit())
         genLoadLongConstant(cg, node, 2, paddingUnequalRetValReg);
      else
         generateLoad32BitConstant(cg, node, 2, paddingUnequalRetValReg, true);

      TR::LabelSymbol *doneAssignLabel = generateLabelSymbol(cg);
      generateS390CompareAndBranchInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::CGR : TR::InstOpCode::CR, node, source2LenReg, source1LenReg, TR::InstOpCode::COND_BHR, doneAssignLabel);

      generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, countReg, source2LenReg);
      generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, paddingPosReg, source1Reg);
      if(cg->comp()->target().is64Bit())
         genLoadLongConstant(cg, node, 1, paddingUnequalRetValReg);
      else
         generateLoad32BitConstant(cg, node, 1, paddingUnequalRetValReg, true);

      TR::Instruction *cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneAssignLabel);
      cursor->setDependencyConditions(regDeps);

      paddingLenReg = cg->allocateRegister();
      generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::SLGR : TR::InstOpCode::SLR, node, source1LenReg, source2LenReg);
      generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LPGR : TR::InstOpCode::LPR, node, paddingLenReg, source1LenReg);
      if(isUTF16)
         {
         if(cg->comp()->target().is64Bit())
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, paddingLenReg, paddingLenReg, 1);
         else
            generateRSInstruction(cg, TR::InstOpCode::SRL, node, paddingLenReg, 1);
         }
      generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::ALGR : TR::InstOpCode::ALR, node, paddingPosReg, countReg);

      retValReg = cg->allocateRegister();
      if(cg->comp()->target().is64Bit())
         genLoadLongConstant(cg, node, 0, retValReg);
      else
         generateLoad32BitConstant(cg, node, 0, retValReg, true);
      }

   loopCountReg = cg->allocateRegister();
   if (cg->comp()->target().is64Bit())
      {
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, loopCountReg, countReg, 8);
      }
   else
      {
      generateRRInstruction(cg, TR::InstOpCode::LR, node, loopCountReg, countReg);
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, loopCountReg, 8);
      }
   generateRXInstruction(cg, TR::InstOpCode::LA, node, loopCountReg, generateS390MemoryReference(loopCountReg, 1, cg));

   //int32_t dependencyCount = isEqual ? 4 : 9;
   //May need two more regDeps for later use: in generateConstCLCRemainder and in generateCLCUnequal
   int32_t dependencyCount = isEqual ? 6 : 11;

   regDeps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, dependencyCount, cg);
   regDeps->addPostCondition(source1Reg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(source2Reg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(loopCountReg, TR::RealRegister::AssignAny);
   regDeps->addPostCondition(countReg, TR::RealRegister::AssignAny);
   if(!isEqual)
      {
      regDeps->addPostCondition(retValReg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(paddingReg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(paddingPosReg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(paddingLenReg, TR::RealRegister::AssignAny);
      regDeps->addPostCondition(paddingUnequalRetValReg, TR::RealRegister::AssignAny);
      }
   unequalLabel = generateLabelSymbol(cg);
   }

void arraycmpWithPadHelper::generateConstCLCMainLoop()
   {
   int32_t loopCount = count >> 8;

   //check for multiple of 256
   bool isPerfectMultiple = ((count & 0xFF) == 0);

   for(int32_t i = 0, offset = 0; i < loopCount; ++i, offset += 256)
      {
      TR::MemoryReference *pos1Ref;
      if (!isAddr1Const)
         {
         pos1Ref = generateS390MemoryReference(source1Reg, offset, cg);
         }
      else
         {
         TR_ASSERT( offset + addr1Const < 4096 , "arraycmpWithPadHelper: memRef offset too large, we will generate wrong code\n");
         pos1Ref = generateS390MemoryReference(NULL, offset + addr1Const, cg);
         }

      TR::MemoryReference *pos2Ref;
      if (!isAddr2Const)
         {
         pos2Ref = generateS390MemoryReference(source2Reg, offset, cg);
         }
      else
         {
         TR_ASSERT( offset + addr2Const < 4096 ,"arraycmpWithPadHelper: memRef offset too large, we will generate wrong code\n");
         pos2Ref = generateS390MemoryReference(NULL, offset + addr2Const, cg);
         }

      generateSS1Instruction(cg, TR::InstOpCode::CLC, node, 255, pos1Ref, pos2Ref);

      if (!(isPerfectMultiple && (i == loopCount - 1) && isEqual))
         {
         TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

         generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
         cFlowRegionStart->setStartInternalControlFlow();
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node,
                                                                   skipUnequal ? cmpDoneLabel : unequalLabel,
                                                                   branchDeps); //brCond is always TR::InstOpCode::COND_MASK6
         }
      }
   }

void arraycmpWithPadHelper::generateVarCLCMainLoop()
   {
   TR::LabelSymbol *loopStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *loopTopLabel = generateLabelSymbol(cg);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, loopStartLabel);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopTopLabel);

   TR::MemoryReference *pos1Ref = generateS390MemoryReference(source1Reg, 0, cg);
   TR::MemoryReference *pos2Ref = generateS390MemoryReference(source2Reg, 0, cg);

   generateSS1Instruction(cg, TR::InstOpCode::CLC, node, 255, pos1Ref, pos2Ref);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node, skipUnequal ? cmpDoneLabel : unequalLabel, branchDeps);

   generateRXInstruction(cg, TR::InstOpCode::LA, node, source1Reg, generateS390MemoryReference(source1Reg, 256, cg));
   generateRXInstruction(cg, TR::InstOpCode::LA, node, source2Reg, generateS390MemoryReference(source2Reg, 256, cg));

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopStartLabel);

   generateS390BranchInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::BRCTG : TR::InstOpCode::BRCT, node, loopCountReg, loopTopLabel);
   cg->stopUsingRegister(loopCountReg);
   }

void arraycmpWithPadHelper::generateConstCLCRemainder()
   {
   int32_t remain = count & 0xFF;
   if (remain)
      {
      TR::MemoryReference *pos1Ref;
      if (!isAddr1Const)
         {
         pos1Ref = generateS390MemoryReference(source1Reg, count & 0xFFFFFF00, cg);
         }
      else
         {
         TR_ASSERT( (count & 0xFFFFFF00) + addr1Const < 4096 , "arraycmpWithPadHelper: memRef offset too large, we will generate wrong code\n");
         pos1Ref = generateS390MemoryReference(NULL, (count & 0xFFFFFF00) + addr1Const, cg);
         }

      TR::MemoryReference *pos2Ref;
      if (!isAddr2Const)
         {
         pos2Ref = generateS390MemoryReference(source2Reg, count & 0xFFFFFF00, cg);
         }
      else
         {
         TR_ASSERT( (count & 0xFFFFFF00) + addr2Const < 4096 ,"arraycmpWithPadHelper: memRef offset too large, we will generate wrong code\n");
         pos2Ref = generateS390MemoryReference(NULL, (count & 0xFFFFFF00) + addr2Const, cg);
         }

      generateSS1Instruction(cg, TR::InstOpCode::CLC, node, remain - 1, pos1Ref, pos2Ref);
      }
   }

void arraycmpWithPadHelper::generateVarCLCRemainder()
   {
   //remainder check

   // this compare and branch will work because we'd only get here if the CC was 0
   // if we skip, we're setting it to 0 again (no change)
   // otherwise we're setting it properly with a CLC
   if (cg->comp()->target().is64Bit())
      {
      generateS390ImmOp(cg, TR::InstOpCode::NG, node, countReg, countReg, (int64_t)0xFF);

      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::CG, node, countReg, (int32_t)0, TR::InstOpCode::COND_BE, isFoldedIf ? unequalLabel : cmpDoneLabel, false, false);
      }
   else
      {
      generateS390ImmOp(cg, TR::InstOpCode::N, node, countReg, countReg, (int32_t)0xFF);

      generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::C, node, countReg, (int32_t)0, TR::InstOpCode::COND_BE, isFoldedIf ? unequalLabel : cmpDoneLabel, false, false);
      }

   generateRIInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::AGHI : TR::InstOpCode::AHI, node, countReg, -1);

   TR::MemoryReference *pos1Ref;
   if (!isAddr1Const)
      {
      pos1Ref = generateS390MemoryReference(source1Reg, 0, cg);
      }
   else
      {
      TR_ASSERT( addr1Const < 4096 , "arraycmpWithPadHelper: memRef offset too large, we will generate wrong code\n");
      pos1Ref = generateS390MemoryReference(NULL, addr1Const, cg);
      }

   TR::MemoryReference *pos2Ref;
   if (!isAddr2Const)
      {
      pos2Ref = generateS390MemoryReference(source2Reg, 0, cg);
      }
   else
      {
      TR_ASSERT( addr2Const < 4096 , "arraycmpWithPadHelper: memRef offset too large, we will generate wrong code\n");
      pos2Ref = generateS390MemoryReference(NULL, addr2Const, cg);
      }
   TR::Instruction *cursor = generateSS1Instruction(cg, TR::InstOpCode::CLC, node, 0, pos1Ref, pos2Ref);

   generateEXDispatch(node, cg, countReg, cursor);
   }

TR::Register* arraycmpWithPadHelper::generateCLCUnequal()
   {
   if((isEqual || constSpacePadding) && !isConst)
      {
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, unequalLabel);
      }
   else
      {
      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, unequalLabel, regDeps);
      }

   if (!retValReg)
      {
      retValReg = cg->allocateRegister();
      regDeps->addPostCondition(retValReg, TR::RealRegister::AssignAny);
      }

   generateRRInstruction(cg, TR::InstOpCode::IPM, node, retValReg, retValReg);
   if(cg->comp()->target().is64Bit())
      {
      generateRRInstruction(cg, TR::InstOpCode::LLGTR, node, retValReg, retValReg);
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, retValReg, retValReg, 28);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, retValReg, 28);
      }

   //if(!isConst)
   //   {
   //   cursor = generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, doneLabel);
   //   cursor->setDependencyConditions(regDeps);
   //   }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cmpDoneLabel, regDeps);

   if (isStartInternalControlFlowSet)
      {
      cmpDoneLabel->setEndInternalControlFlow();
      }

   return retValReg;
   }

void arraycmpWithPadHelper::setupConstCLCPadding()
   {
   //if there's padding then I need to do the final check if we did any comparison in the first place

   if ((source1Len != 0) && (source2Len != 0))
      {
      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node,
                                                                   skipUnequal ? cmpDoneLabel : unequalLabel,
                                                                   branchDeps);
      }

   //calculate padding information
   paddingLen = source1Len - source2Len;
   paddingOffset = 0;
   paddingRetCode = 0;
   paddingSrcReg = NULL;
   if(source1Len > source2Len)
      {
      paddingOffset = source2Len;
      paddingSrcReg = source1Reg;
      if (isAddr1Const)
         {
         isPaddingSrcAddrConst = true;
         paddingSrcAddr = addr1Const;
         }
      paddingRetCode = 1;
      }
   else
      {
      paddingLen = -paddingLen;
      paddingOffset = source1Len;
      paddingSrcReg = source2Reg;
      if (isAddr2Const)
         {
         isPaddingSrcAddrConst = true;
         paddingSrcAddr = addr2Const;
         }
      paddingRetCode = 2;
      }
   }

void arraycmpWithPadHelper::setupVarCLCPadding()
   {
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, unequalLabel);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, remainEndLabel);
   }

TR::MemoryReference *
arraycmpWithPadHelper::convertToShortDispMemRef(TR::Node * node, TR::MemoryReference * largeDispMemRef, TR::Register * &largeDispReg, TR::RegisterDependencyConditions *regDeps, TR::CodeGenerator * cg)
   {
   //Long disp memory reference causes extra LA to load addr to fit into CLC etc.
   //Early loading addr to the explicit reg can thus avoid the LA generated from the later "generateSS1Instruction(cg, TR::InstOpCode::CLC,...)"
   //More importantly, we have an explicit reg which is easily added to regDeps.

   int64_t disp = largeDispMemRef->getOffset() ? largeDispMemRef->getOffset()
                                               : largeDispMemRef->getSymbolReference()->getOffset(); //For ConstantDataSnippet

   TR::MemoryReference * ret = largeDispMemRef;

   if (!largeDispMemRef)
      return ret;

   //if (cg->isDispInRange(disp))
   //if (disp < MAXDISP)
   if (disp <= TR_MAX_SS_DISP)
      {
      //Nothing. Satisfied.
      }
   else
      {
      largeDispReg = cg->allocateRegister();
      regDeps->addPostCondition(largeDispReg, TR::RealRegister::AssignAny);

      generateRXInstruction(cg, TR::InstOpCode::LA, node, largeDispReg, largeDispMemRef);
      ret = generateS390MemoryReference(largeDispReg, 0, cg);
      }

    return ret;
   }

void arraycmpWithPadHelper::generateConstCLCSpacePaddingLoop()
   {
   TR::Compilation *comp = cg->comp();
   // use space characters in the literal pool so we can use CLC

   if (newLitPoolReg)
      generateLoadLiteralPoolAddress(cg, node, litPoolBaseReg);

   // main padding loop
   int32_t paddingLoopCount = paddingLen / MAX_PADDING_SIZE;

   //remaining padding
   int32_t remain = paddingLen % MAX_PADDING_SIZE;

   TR::Register * largeDispReg = NULL;
   TR::Register * tempReg = NULL;
   TR::Register * tempReg2 = NULL;

   TR::MemoryReference *paddingRef = generateS390MemoryReference(litPoolBaseReg, paddingLitPoolOffset, cg);

   if (paddingLoopCount || remain > 1 || (remain == 1) && (paddingRetCode == 2))
      {
      paddingRef = convertToShortDispMemRef(node, paddingRef, largeDispReg, regDeps, cg);
      paddingRef = preemptivelyAdjustLongDisplacementMemoryReference(paddingRef);
      }
   for (int32_t i = 0; i < paddingLoopCount; ++i)
      {
      TR::MemoryReference *posRef = generateS390MemoryReference(paddingSrcReg, paddingOffset + MAX_PADDING_SIZE * i, cg);
      paddingRef = reuseS390MemoryReference(paddingRef, 0, node, cg, false);

      if (paddingRetCode == 1)
         generateSS1Instruction(cg, TR::InstOpCode::CLC, node, MAX_PADDING_SIZE - 1, posRef, paddingRef);
      else
         generateSS1Instruction(cg, TR::InstOpCode::CLC, node, MAX_PADDING_SIZE - 1, paddingRef, posRef);

      TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);

      generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
      cFlowRegionStart->setStartInternalControlFlow();
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node,
                                                                   skipUnequal ? cmpDoneLabel : unequalLabel,
                                                                   branchDeps);
      }

   if (remain)
      {
      //if src1leng < src2len and remain == 1, CLI forces a reverse compare order which leads an additional BRC following CLI. i.e.,
      //CLI
      //TR::InstOpCode::S390BranchCondition brCond = getBranchConditionForMask(getReverseBranchMask((getMaskForBranchCondition(finalBrCond)) & 0x0E));
      ////Do not use --> TR::InstOpCode::S390BranchCondition brCond = getReverseBranchCondition(finalBrCond);
      //BRC on brCond
      //Not worth doing that.

      //Special handling for 1 byte of remain and src1leng > src2len
      if ((remain == 1) && (paddingRetCode == 1))
         {
         TR::MemoryReference *posRef  = generateS390MemoryReference(paddingSrcReg, paddingOffset + (paddingLen - remain), cg);
         uint64_t paddingValue = paddingValue = paddingNode->getIntegerNodeValue<uint8_t>(); //The remain for UTF16 must be an even number, So it wont be here

         generateSIInstruction(cg, TR::InstOpCode::CLI, node, posRef, paddingValue);
         }
      else
         {
         TR::MemoryReference *posRef;
         if (isPaddingSrcAddrConst)
            {
            posRef = generateS390MemoryReference(NULL, paddingSrcAddr + paddingOffset + (paddingLen - remain), cg);
            }
         else
            {
            TR_ASSERT( paddingOffset + (paddingLen - remain) < 4096 ,"arraycmpWithPadHelper: memRef offset too large, we will generate wrong code\n");
            posRef = generateS390MemoryReference(paddingSrcReg, paddingOffset + (paddingLen - remain), cg);
            }
         posRef = preemptivelyAdjustLongDisplacementMemoryReference(posRef);
         TR::MemoryReference *oldPaddingRef = paddingRef;
         paddingRef = reuseS390MemoryReference(paddingRef, 0, node, cg, false);
         if (oldPaddingRef != paddingRef)
            paddingRef = preemptivelyAdjustLongDisplacementMemoryReference(paddingRef);

         if (paddingRetCode == 1)
            generateSS1Instruction(cg, TR::InstOpCode::CLC, node, remain - 1, posRef, paddingRef);
         else
            generateSS1Instruction(cg, TR::InstOpCode::CLC, node, remain - 1, paddingRef, posRef);
         }
      }

   if (largeDispReg)
      {
      cg->stopUsingRegister(largeDispReg);
      }
   if (newLitPoolReg)
      cg->stopUsingRegister(litPoolBaseReg);
   if (tempReg)
      cg->stopUsingRegister(tempReg);
   if (tempReg2)
      cg->stopUsingRegister(tempReg2);
   }

void arraycmpWithPadHelper::generateConstCLCPaddingLoop()
   {
   // use EX->CLI to compare one character at a time
   TR::LabelSymbol *paddingUnequalLabel = generateLabelSymbol(cg);
   doneLabel = generateLabelSymbol(cg);
   if(isUTF16)
      paddingLen /= 2;

   for(int32_t i = 0; i < paddingLen; ++i)
      {
      if(isUTF16)
         generateRXInstruction(cg, TR::InstOpCode::CH, node, paddingReg, generateS390MemoryReference(paddingSrcReg, paddingOffset + (2 * i), cg));
      else
         {
         TR::Instruction *cursor = generateSIInstruction(cg, TR::InstOpCode::CLI, node, generateS390MemoryReference(paddingSrcReg, paddingOffset + i, cg), 0);

         generateEXDispatch(node, cg, paddingReg, cursor);
         }

      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, paddingUnequalLabel);
      }

   if(cg->comp()->target().is64Bit())
      genLoadLongConstant(cg, node, 0, retValReg);
   else
      generateLoad32BitConstant(cg, node, 0, retValReg, true);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, doneLabel);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, paddingUnequalLabel);
   //IPM
   generateRRInstruction(cg, TR::InstOpCode::IPM, node, retValReg, retValReg);
   //SRL(G)
   if(cg->comp()->target().is64Bit())
      {
      generateRRInstruction(cg, TR::InstOpCode::LLGTR, node, retValReg, retValReg);
      generateRSInstruction(cg, TR::InstOpCode::SRLG, node, retValReg, retValReg, 28);
      }
   else
      {
      generateRSInstruction(cg, TR::InstOpCode::SRL, node, retValReg, 28);
      }
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, doneLabel);

   cg->stopUsingRegister(paddingReg);
   }

void arraycmpWithPadHelper::generateVarCLCPaddingLoop()
   {
   TR::LabelSymbol *loopTopLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *loopStartLabel = generateLabelSymbol(cg);
   TR::LabelSymbol *paddingUnequalLabel = generateLabelSymbol(cg);

   generateRXInstruction(cg, TR::InstOpCode::LA, node, paddingLenReg, generateS390MemoryReference(paddingLenReg, 1, cg));

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, loopStartLabel);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopTopLabel);

   if(isUTF16)
      {
      generateRXInstruction(cg, TR::InstOpCode::CH, node, paddingReg, generateS390MemoryReference(paddingPosReg, 0, cg));
      }
   else
      {
      TR::Instruction *cursor = generateSIInstruction(cg, TR::InstOpCode::CLI, node, generateS390MemoryReference(paddingPosReg, 0, cg), 0);

      generateEXDispatch(node, cg, paddingReg, cursor);
      }

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK6, node, paddingUnequalLabel);

   generateRXInstruction(cg, TR::InstOpCode::LA, node, paddingPosReg, generateS390MemoryReference(paddingPosReg, isUTF16 ? 2 : 1, cg));

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopStartLabel);

   generateS390BranchInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::BRCTG : TR::InstOpCode::BRCT, node, paddingLenReg, loopTopLabel);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, doneLabel); //if we finish the loop, then everything is equal and we're done

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, paddingUnequalLabel);
   generateRRInstruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, retValReg, paddingUnequalRetValReg);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, doneLabel);

   cg->stopUsingRegister(paddingReg);
   cg->stopUsingRegister(countReg);
   cg->stopUsingRegister(paddingPosReg);
   cg->stopUsingRegister(paddingLenReg);
   cg->stopUsingRegister(paddingUnequalRetValReg);
   }

void arraycmpWithPadHelper::teardownCLC()
   {
   if (!isConst)
      {
      cg->stopUsingRegister(source1LenReg);
      cg->stopUsingRegister(source2LenReg);
      cg->decReferenceCount(source1LenNode);
      cg->decReferenceCount(source2LenNode);
      }
   cg->stopUsingRegister(source1Reg);
   cg->stopUsingRegister(source2Reg);

   if (recDecSource1Node)
      cg->recursivelyDecReferenceCount(source1Node);
   else
      cg->decReferenceCount(source1Node);

   if (recDecSource2Node)
      cg->recursivelyDecReferenceCount(source2Node);
   else
      cg->decReferenceCount(source2Node);

   cg->recursivelyDecReferenceCount(paddingNode);
   }

void arraycmpWithPadHelper::generateCLCLitPoolPadding()
   {
   TR::Compilation *comp = cg->comp();
   bool constPadding = paddingNode->getOpCode().isLoadConst();
   int32_t paddingByteLength = (source1Len > source2Len) ? (source1Len - source2Len) : (source2Len - source1Len);
   uint8_t *paddingValue = (uint8_t *)comp->trMemory()->allocateHeapMemory(sizeof(uint8_t) * paddingByteLength);

   if (constPadding)
      {
      if (isUTF16)
         {
         uint16_t paddingChar = paddingNode->getIntegerNodeValue<uint16_t>();
         uint16_t *paddingArr = (uint16_t *)paddingValue;

         for (int32_t i = 0; i < paddingByteLength/2; ++i)
            paddingArr[i] = paddingChar;
         }
      else
         {
         uint8_t paddingChar = paddingNode->getIntegerNodeValue<uint8_t>();
         memset(paddingValue, paddingChar, paddingByteLength);
         }

      constSpacePadding = true;

      paddingLitPoolOffset = cg->findOrCreateLiteral(paddingValue, paddingByteLength);
      newLitPoolReg = cg->isLiteralPoolOnDemandOn();
      if (newLitPoolReg)
         litPoolBaseReg = cg->allocateRegister();
      else
         litPoolBaseReg = cg->getLitPoolRealRegister();
      }
   }

void arraycmpWithPadHelper::generateCLCFoldedIfResult()
   {
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, unequalLabel);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, finalBrCond, node, cmpTargetLabel, branchDeps);
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cmpDoneLabel, regDeps);

   if (isStartInternalControlFlowSet)
      {
      cmpDoneLabel->setEndInternalControlFlow();
      }
   }

void arraycmpWithPadHelper::chooseCLCBranchConditions()
   {
   isSingleTarget = false;
   brCond = TR::InstOpCode::COND_MASK6;
   if (finalBrCond == TR::InstOpCode::COND_NOP)
      {
      finalBrCond = getStandardIfBranchCondition(ificmpNode_->getOpCodeValue(), cmpVal);
      if(finalBrCond == TR::InstOpCode::COND_MASK8) skipUnequal = true;
      }
   }

/**
 * Sometimes during arraycmpwithPadHelper instructions will be automatically created as part of the memRef evaluation,
 * specifically when fixup for long displacement occurs.
 * This is a common occurrance, but in this situation it's problematic because it occurs inside Internal Control Flow, and
 * the result is that registers used are not properly assigned afterwards.  Furthermore, since these register uses occur
 * 'down' in the instruction creation process it is not possible to add them to the reg deps at that point.
 *
 * The solution is to perform the long displacement fixup before trying to create instructions.  This function
 * can (or should?) be called whenever a memory reference is created inside internal control flow.
 *
 */
TR::MemoryReference * arraycmpWithPadHelper::preemptivelyAdjustLongDisplacementMemoryReference(TR::MemoryReference * memRef)
   {
     return memRef;
   }

TR::Register* arraycmpWithPadHelper::generate()
   {
   TR::Register *retValReg = NULL;
   if(source1LenNode->getOpCode().isLoadConst())
      {
      source1Len = source1LenNode->getIntegerNodeValue<int>();
      isLen1Const = true;
      }

   if(source2LenNode->getOpCode().isLoadConst())
      {
      source2Len = source2LenNode->getIntegerNodeValue<int>();
      isLen2Const = true;
      }

   if (source1LenNode->getOpCode().isLoadConst() && source2LenNode->getOpCode().isLoadConst())
      {
      isConst = true;

      //can get accurate answers for these now
      isEqual = (source1Len == source2Len);
      maxLen = (source1Len > source2Len) ? source1Len : source2Len;

      if (abs(source1Len - source2Len) > PADDING_THRESHOLD)
         forceCLCL = true;

      if (source1Len == 0 && source2Len == 0)
         {
         if (ificmpNode_ && ificmpNode_->getOpCode().isCompareTrueIfEqual())
            {
            finalBrCond = TR::InstOpCode::COND_MASK15;
            if (returnCond!=NULL) *returnCond = TR::InstOpCode::COND_MASK15;
            }
         else
            {
            finalBrCond = TR::InstOpCode::COND_MASK0;
            if (returnCond!=NULL) *returnCond = TR::InstOpCode::COND_MASK0;
            }
         }
      }
   //CLCL becomes better than EX->CLI after 19 bytes on z9 and 21 bytes on z10, so if
   //we're unsure if there's padding (due to variable lengths), or we're not sure what
   //the padding is, best to just use CLCL
   else if(!isEqual)
      forceCLCL = true;
   if(!paddingNode->getOpCode().isLoadConst())
      forceCLCL = true;
      // Force a CLCL if we want compact code

      if (source1Node->getOpCode().isLoadConst())
         {
         addr1Const = source1Node->getIntegerNodeValue<int>();
         if (isLen1Const && (addr1Const + source1Len < 4096))
            {
            isAddr1Const = true;
            }
         }

      if (!isAddr1Const)
         {
         source1Reg = cg->gprClobberEvaluate(source1Node);
         }

      if(source2Node->getOpCode().isLoadConst())
         {
         addr2Const = source2Node->getIntegerNodeValue<int>();
         if (isLen2Const && (addr2Const + source2Len < 4096))
            {
            isAddr2Const = true;
            }
         }

      if (!isAddr2Const)
         {
         source2Reg = cg->gprClobberEvaluate(source2Node);
         }



   //Notice that when (maxLen == 0) && !isConst both their real lengths and maximum lengths are unknown.
   if ((maxLen < CLC_THRESHOLD) && (maxLen != 0 || isConst) && !forceCLCL)
      {
      cmpDoneLabel = generateLabelSymbol(cg);

      //choose the branch conditions
      if (isFoldedIf)
         chooseCLCBranchConditions();

      // check for const space padding
      if (!isEqual && isConst)
         generateCLCLitPoolPadding();

      //generate main setup
      if (isConst)
         generateConstCLCSetup();
      else
         generateVarCLCSetup();

      //generate main loop
      if (isConst)
         generateConstCLCMainLoop();
      else
         generateVarCLCMainLoop();

      //generate main remainder
      if (isConst)
         generateConstCLCRemainder();
      else
         generateVarCLCRemainder();

      //padding section
      if (!isEqual)
         {
         //generate padding loop and remainder
         if (isConst)
            {
            setupConstCLCPadding();
            if (constSpacePadding)
               generateConstCLCSpacePaddingLoop();
            else
               generateConstCLCPaddingLoop(); // NOTE: This can't happen right now, check the note below for details.
            }
         else
            {
            // NOTE: This path is currently disabled as CLCL is faster.
            // If this path ever becomes faster, it will need to be modified to handle folded if statements.
            setupVarCLCPadding();
            generateVarCLCPaddingLoop();
            }
         }

      if (isFoldedIf)
         generateCLCFoldedIfResult();
      else
         {
         if (genResultReg)
            retValReg = generateCLCUnequal();
         else
            {
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, unequalLabel);
            generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cmpDoneLabel, regDeps);

            if (isStartInternalControlFlowSet)
               {
               cmpDoneLabel->setEndInternalControlFlow();
               }
            }
         }

      //cleanup
      teardownCLC();
      }
   else
      {
      if(isFoldedIf)
         brCond = getStandardIfBranchCondition(ificmpNode_->getOpCodeValue(), cmpVal);


   if (isAddr1Const)
      {
      if(cg->comp()->target().is64Bit())
         {
         source1Reg = cg->allocateRegister();
         genLoadLongConstant(cg, node, addr1Const, source1Reg);
         }
      else
         {
         source1Reg = cg->allocateRegister();
         generateLoad32BitConstant(cg, node, addr1Const, source1Reg, true);
         }
      }

   if (isAddr2Const)
      {
      if(cg->comp()->target().is64Bit())
         {
         source2Reg = cg->allocateRegister();
         genLoadLongConstant(cg, node, addr2Const, source2Reg);
         }
      else
         {
         source2Reg = cg->allocateRegister();
         generateLoad32BitConstant(cg, node, addr2Const, source2Reg, true);
         }
      }



      if(source2LenNode->getOpCode().isLoadConst() && paddingNode->getOpCode().isLoadConst() && (maxLen < CLCL_THRESHOLD) && (maxLen != 0 || isConst) && !isUTF16)
         {
         setupConstCLCL();
         retValReg = generateConstCLCL();
         }
      else
         {
         //No internal control flow is necessary for CLCL/CLCLE

         setupCLCLandCLCLE();

         if((maxLen < CLCL_THRESHOLD) && (maxLen != 0 || isConst) && !isUTF16)
            retValReg = generateCLCL();
         else
            retValReg = generateCLCLE();
         }

      teardownCLCLandCLCLE();
      }

   if (isAddr1Const && source1Reg)
      cg->stopUsingRegister(source1Reg);
   if (isAddr2Const && source2Reg)
      cg->stopUsingRegister(source2Reg);

   //!isFoldedIf && genResultReg == genResultReg
   if(!isFoldedIf && genResultReg)
      node->setRegister(retValReg);
   return retValReg;
   }

TR::Register *OMR::Z::TreeEvaluator::trtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineTrtEvaluator(node, cg, TR::InstOpCode::TRT, false, false, NULL);
   }

TR::Register *OMR::Z::TreeEvaluator::integerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineHighestOneBit(node, cg, false);
   }

TR::Register *OMR::Z::TreeEvaluator::integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineNumberOfLeadingZeros(node, cg, false);
   }

TR::Register *OMR::Z::TreeEvaluator::integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineNumberOfTrailingZeros(node, cg, 32);
   }

TR::Register *OMR::Z::TreeEvaluator::integerBitCount(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_ASSERT_FATAL(cg->comp()->target().cpu.getSupportsArch(TR::CPU::z15), "TR::popcnt IL only supported on z15+");

   TR::Register* resultReg = cg->allocateRegister();
   TR::Register* valueReg = cg->gprClobberEvaluate(node->getChild(0));

   generateRRInstruction(cg, TR::InstOpCode::LLGFR, node, valueReg, valueReg);
   generateRRFInstruction(cg, TR::InstOpCode::POPCNT, node, resultReg, valueReg, static_cast<uint8_t>(0x8), static_cast<uint8_t>(0x0), NULL);

   node->setRegister(resultReg);

   cg->decReferenceCount(node->getChild(0));

   return resultReg;
   }

TR::Register *OMR::Z::TreeEvaluator::longHighestOneBit(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineHighestOneBit(node, cg, true);
   }

TR::Register *OMR::Z::TreeEvaluator::longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineNumberOfLeadingZeros(node, cg, true);
   }

TR::Register *OMR::Z::TreeEvaluator::longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg)
   {
   return inlineNumberOfTrailingZeros(node, cg, 64);
   }

TR::Register *OMR::Z::TreeEvaluator::longBitCount(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register* resultReg = cg->allocateRegister();
   TR::Register* valueReg = cg->evaluate(node->getChild(0));

   generateRRFInstruction(cg, TR::InstOpCode::POPCNT, node, resultReg, valueReg, (uint8_t)0x8, (uint8_t)0, NULL);
   node->setRegister(resultReg);

   cg->decReferenceCount(node->getChild(0));
   return resultReg;
   }

TR::Register *OMR::Z::TreeEvaluator::inlineIfTestDataClassHelper(TR::Node *node, TR::CodeGenerator *cg, bool &inlined)
   {
   inlined = false;
   return NULL;
   }

/**
 * Retrieving and Folding Condition Code
 * To prevent generation of unused CC for a BIF, add checks for the following cases:
 * FoldNone:    Default case, called through evaluator
 * FoldIf:      depends on whether nodes returning CC are uncommoned (moved out of treetop) via TR::CodeGenerator::uncommonIfBifs()
 *              To add, include check in uncommonIfBifs() and isIfFoldable().
 * FoldNoIPM:   set by calling through treetopEvaluator(). Include a check there.
 * FoldLookUp:  set by calling through lookupEvaluator(). Include a check there.
 * For icall BIF, returnsConditionCodeOrBool flag should be set by including the BIF in idIsBIFReturningConditionCode() during ILGen
 */
TR::Register *OMR::Z::TreeEvaluator::getConditionCodeOrFold(TR::Node *node, TR::CodeGenerator *cg, TR_FoldType foldType, TR::Node *parent){
   switch(foldType){
      case TR_FoldNone: return TR::TreeEvaluator::getConditionCodeOrFoldIf(node, cg, false, NULL);
      case TR_FoldIf: return TR::TreeEvaluator::getConditionCodeOrFoldIf(node, cg, true, parent);
      case TR_FoldLookup: return TR::TreeEvaluator::foldLookup(cg, parent);
      case TR_FoldNoIPM: return NULL;
      default: TR_ASSERT(false, "unexpected folding type\n");
   }
   return NULL;//never reached, suppress warning
}
/**
 * @see getConditionCodeOrFold
 */
TR::Register *OMR::Z::TreeEvaluator::getConditionCodeOrFoldIf(TR::Node *node, TR::CodeGenerator *cg, bool isFoldedIf, TR::Node *ificmpNode)
   {
   if(isFoldedIf)
      {
      TR_ASSERT(ificmpNode, "Trying to get condition code from node (0x%p) but given NULL parent\n", node);
      //figure out branch condition
      int compareVal = ificmpNode->getChild(1)->getIntegerNodeValue<int>();
      cg->decReferenceCount(ificmpNode->getChild(1));
      TR::InstOpCode::S390BranchCondition brCond = getStandardIfBranchCondition(ificmpNode->getOpCodeValue(), compareVal);
      //get destination
      TR::LabelSymbol *compareTarget = ificmpNode->getBranchDestination()->getNode()->getLabel();
      //get dependencies
      TR::RegisterDependencyConditions *deps = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode);
      //generate and return
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, node, compareTarget, deps);
      return NULL;
      }
   else
      {
      TR::Register *conditionCodeReg = getConditionCode(node, cg);
      node->setRegister(conditionCodeReg);
      return conditionCodeReg;
      }
   }

TR::Register *OMR::Z::TreeEvaluator::foldLookup(TR::CodeGenerator *cg, TR::Node *lookupNode){
   //handle nondefault cases
   uint16_t upperBound = lookupNode->getCaseIndexUpperBound();
   for(int32_t i=2; i<upperBound; ++i)
      {
      TR::Node* caseNode = lookupNode->getChild(i);
      //figure out branch condition
      CASECONST_TYPE compareVal = caseNode->getCaseConstant();
      TR::InstOpCode::S390BranchCondition brCond = getStandardIfBranchCondition(TR::ificmpeq, compareVal);
      //get destination
      TR::LabelSymbol *compareTarget = caseNode->getBranchDestination()->getNode()->getLabel();
      //generate
      if(caseNode->getNumChildren() > 0)
         {
         //GRA
         cg->evaluate(caseNode->getFirstChild());
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, lookupNode, compareTarget,
               generateRegisterDependencyConditions(cg, caseNode->getFirstChild(), 0));
         }
      else
         {
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, brCond, lookupNode, compareTarget);
         }
      }
   //handle default case
   TR::Node * defaultChild = lookupNode->getSecondChild();
   if (defaultChild->getNumChildren() > 0)
      {
      // GRA
      cg->evaluate(defaultChild->getFirstChild());
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, lookupNode, defaultChild->getBranchDestination()->getNode()->getLabel(),
            generateRegisterDependencyConditions(cg, defaultChild->getFirstChild(), 0));
      }
   else
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, lookupNode, defaultChild->getBranchDestination()->getNode()->getLabel());
      }

   cg->decReferenceCount(lookupNode->getFirstChild());
   return NULL;
}

TR::Register *
OMR::Z::TreeEvaluator::ifxcmpoEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR_ASSERT((opCode == TR::ificmno) || (opCode == TR::ificmnno) ||
             (opCode == TR::iflcmno) || (opCode == TR::iflcmnno) ||
             (opCode == TR::ificmpo) || (opCode == TR::ificmpno) ||
             (opCode == TR::iflcmpo) || (opCode == TR::iflcmpno), "invalid opcode");

   bool nodeIs64Bit = node->getFirstChild()->getDataType() == TR::Int64;
   bool reverseBranch = (opCode == TR::ificmnno) || (opCode == TR::iflcmnno) || (opCode == TR::ificmpno) || (opCode == TR::iflcmpno);

   // TODO:
   // - use clobber evaluates / analyser and avoid evaluating children to registers unnecessarily
   // - if we can use non-destructive instructions, we can avoid the temp register

   TR::Register* rs1 = cg->evaluate(node->getFirstChild());
   TR::Register* rs2 = cg->evaluate(node->getSecondChild());
   TR::Register *tmp = cg->allocateRegister();

   generateRRInstruction(cg, nodeIs64Bit? TR::InstOpCode::LGR : TR::InstOpCode::LR, node, tmp, rs1);

   if ((opCode == TR::ificmno) || (opCode == TR::ificmnno) ||
       (opCode == TR::iflcmno) || (opCode == TR::iflcmnno))
      generateRRInstruction(cg, nodeIs64Bit? TR::InstOpCode::AGR : TR::InstOpCode::AR, node, tmp, rs2);
   else
      generateRRInstruction(cg, nodeIs64Bit? TR::InstOpCode::SGR : TR::InstOpCode::SR, node, tmp, rs2);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, reverseBranch ? TR::InstOpCode::COND_BNO : TR::InstOpCode::COND_BO, node,
                                 node->getBranchDestination()->getNode()->getLabel());

   cg->stopUsingRegister(tmp);
   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());

   return NULL;
   }

// anchor instruction to assist in debugging
TR::Instruction * breakInst = NULL;

/**
 * @return a vector register, trying to reuse children registers if possible
 */
TR::Register *
OMR::Z::TreeEvaluator::tryToReuseInputVectorRegs(TR::Node * node, TR::CodeGenerator *cg)
   {
   int32_t numChildren = node->getNumChildren();
   TR::Register *returnReg = NULL;

   for (int32_t i = 0; i < numChildren; i++)
      {
      TR::Node *child = node->getChild(i);
      if (child->getOpCode().isVector() && child->getReferenceCount() <= 1)
         {
         returnReg = cg->gprClobberEvaluate(child);
         break;
         }
      }

   if (returnReg == NULL)
      returnReg = cg->allocateRegister(TR_VRF);

   return returnReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::inlineVectorUnaryOp(TR::Node * node,
                                          TR::CodeGenerator *cg,
                                          TR::InstOpCode::Mnemonic op)
   {
   TR_ASSERT(node->getNumChildren() <= 1, "Unary node must only contain 1 or less children");

   TR::Node   *firstChild = node->getFirstChild();
   TR::Register *returnReg = TR::TreeEvaluator::tryToReuseInputVectorRegs(node, cg);
   TR::Register *sourceReg1 = cg->evaluate(firstChild);

   switch (op)
      {
      case TR::InstOpCode::VCDG:
         generateVRRaInstruction(cg, op, node, returnReg, sourceReg1, 0, 0, 3);
         break;
      case TR::InstOpCode::VLC:
         generateVRRaInstruction(cg, op, node, returnReg, sourceReg1, 0, 0, getVectorElementSizeMask(node));
         break;
      case TR::InstOpCode::VFPSO:
         breakInst = generateVRRaInstruction(cg, op, node, returnReg, sourceReg1, 0 /* invert sign */, 0, getVectorElementSizeMask(node));
         break;
      default:
         TR_ASSERT(false, "Unary Vector IL evaluation unimplemented for node : %s\n", cg->getDebug()->getName(node));
         break;
      }

   node->setRegister(returnReg);
   cg->decReferenceCount(firstChild);
   return returnReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::inlineVectorBinaryOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op)
   {
   TR_ASSERT(node->getNumChildren() <= 2,"Binary Node must only contain 2 or less children");

   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();

   TR::Register *targetReg  = TR::TreeEvaluator::tryToReuseInputVectorRegs(node, cg);
   TR::Register *sourceReg1 = cg->evaluate(firstChild);
   TR::Register *sourceReg2 = cg->evaluate(secondChild);

   // !!! Masks change per instruction. *Ref to zPoP for masks* !!!
   uint8_t mask4 = 0;

   switch (op)
      {
      // These don't use mask
      case TR::InstOpCode::VN:
      case TR::InstOpCode::VO:
      case TR::InstOpCode::VX:
         breakInst = generateVRRcInstruction(cg, op, node, targetReg, sourceReg1, sourceReg2, 0, 0, 0);
         break;
      // These use mask4 to set element size to operate on
      case TR::InstOpCode::VA:
      case TR::InstOpCode::VS:
      case TR::InstOpCode::VML:
      case TR::InstOpCode::VCEQ:
         mask4 = getVectorElementSizeMask(node);
         breakInst = generateVRRcInstruction(cg, op, node, targetReg, sourceReg1, sourceReg2, 0, 0, mask4);
         break;
      /*
       * Before z14, these required mask4 = 0x3 and other values were illegal
       * This was because they only operated on long BFP elements.
       * From z14 onward, mask4 is variable and sets the element size to operate on
       */
      case TR::InstOpCode::VFA:
      case TR::InstOpCode::VFS:
      case TR::InstOpCode::VFM:
      case TR::InstOpCode::VFD:
         mask4 = getVectorElementSizeMask(node);
         breakInst = generateVRRcInstruction(cg, op, node, targetReg, sourceReg1, sourceReg2, 0, 0, mask4);
         break;
      case TR::InstOpCode::VFCE:
      case TR::InstOpCode::VFCH:
      case TR::InstOpCode::VFCHE:
         mask4 = 3;
         breakInst = generateVRRcInstruction(cg, op, node, targetReg, sourceReg1, sourceReg2, 0, 0, mask4);
         break;
      // These are VRRb
      case TR::InstOpCode::VCH:
      case TR::InstOpCode::VCHL:
         mask4 = getVectorElementSizeMask(node);
         breakInst = generateVRRbInstruction(cg, op, node, targetReg, sourceReg1, sourceReg2, 0, mask4);
         break;
      default:
         TR_ASSERT(false, "Binary Vector IL evaluation unimplemented for node : %s", cg->getDebug()->getName(node));
      }

   node->setRegister(targetReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return targetReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::inlineVectorBitSelectOp(TR::Node * node, TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op)
   {
   TR_ASSERT(node->getNumChildren() >= 3,"Select Node must contain 3 or more children");
   // TODO Bullet proof this code better and handle more children better.
   TR::Node *firstChild  = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild  = node->getThirdChild();

   TR::Register *targetReg  = TR::TreeEvaluator::tryToReuseInputVectorRegs(node, cg);

   TR::Register *sourceReg1 = cg->evaluate(firstChild);
   TR::Register *sourceReg2 = cg->evaluate(secondChild);
   TR::Register *sourceReg3 = cg->evaluate(thirdChild);

   // !!! Masks change per instruction. *Ref to zPoP for masks* !!!
   uint8_t mask3 = 0;
   uint8_t mask4 = 0;
   uint8_t mask5 = 0;

   node->setRegister(targetReg);

   switch (op)
      {
      case TR::InstOpCode::VMAL:
      case TR::InstOpCode::VMAH:
         mask5 = 0x2;   // mask5 = 0x2 --> word element size (mul makes it double-word)
         // RA does allow for source registers 1 & 2 to be re-used as target
         breakInst = generateVRRdInstruction(cg, op, node, targetReg, sourceReg1, sourceReg2, sourceReg3, 0, mask5);
         break;
      case TR::InstOpCode::VFMA:
         breakInst = generateVRReInstruction(cg, op, node, targetReg, sourceReg1, sourceReg2, sourceReg3, 3, mask5);
         break;
      default:
         TR_ASSERT(false, "Select Vector IL evaluation unimplemented for node : %s", cg->getDebug()->getName(node));
      }

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(thirdChild);

   return targetReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::vloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic opcode = TR::InstOpCode::BAD;

   if (node->getOpCodeValue() == TR::vload ||
       node->getOpCodeValue() == TR::vloadi)
      {
      opcode = TR::InstOpCode::VL;
      }
   else
      {
      TR_ASSERT(false, "Unknown vector load IL\n");
      }

   TR::Register * targetReg = cg->allocateRegister(TR_VRF);
   node->setRegister(targetReg);

   TR::MemoryReference * srcMemRef = new (cg->trHeapMemory()) TR::MemoryReference(node, cg);
   breakInst = generateVRXInstruction(cg, TR::InstOpCode::VL, node, targetReg, srcMemRef);
   srcMemRef->stopUsingMemRefRegister(cg);

   return targetReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register * globalReg = node->getRegister();
   TR::Compilation *comp = cg->comp();

   if (globalReg == NULL)
     {
     globalReg = cg->allocateRegister(TR_VRF);
     node->setRegister(globalReg);
     }
   else
     {
     TR_ASSERT(globalReg->getKind() == TR_VRF, "virtual reg type from adjacent reg store was improperly set");
     }

    return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::vRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * child = node->getFirstChild();
   TR_GlobalRegisterNumber globalRegNum;
   TR::Compilation *comp = cg->comp();
   TR::Register * globalReg;
   bool childEvaluatedPreviously = child->getRegister() != NULL;

   globalReg = cg->evaluate(child);

   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::vstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic opcode = TR::InstOpCode::BAD;

   if (node->getOpCodeValue() == TR::vstore ||
       node->getOpCodeValue() == TR::vstorei)
      {
      opcode = TR::InstOpCode::VST;
      }
   else
      {
      TR_ASSERT(false, "Unknown vector store IL\n");
      }

   TR::Node * valueChild = node->getOpCode().isStoreDirect() ? node->getFirstChild() : node->getSecondChild();
   TR::Register * valueReg = cg->evaluate(valueChild);

   TR::MemoryReference * srcMemRef = new (cg->trHeapMemory()) TR::MemoryReference(node, cg);

   breakInst = generateVRXInstruction(cg, opcode, node, valueReg, srcMemRef);

   //cg->stopUsingRegister(valueReg);
   cg->decReferenceCount(valueChild);
   srcMemRef->stopUsingMemRefRegister(cg);

   return NULL;
   }

TR::Register *
OMR::Z::TreeEvaluator::vdremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::BAD);
   }

TR::Register *
OMR::Z::TreeEvaluator::vdmaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBitSelectOp(node, cg, TR::InstOpCode::VFMA);
   }

TR::Register *
OMR::Z::TreeEvaluator::vselEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstVecNode  = node->getFirstChild();
   TR::Node *secondVecNode = node->getSecondChild();
   TR::Node *vecSelectNode = node->getThirdChild();

   TR::Register *firstVecReg  = cg->evaluate(firstVecNode);
   TR::Register *secondVecReg = cg->evaluate(secondVecNode);
   TR::Register *vecSelectReg = cg->evaluate(vecSelectNode);
   TR::Register *returnVecReg = cg->allocateRegister(TR_VRF);

   generateVRReInstruction(cg, TR::InstOpCode::VSEL, node, returnVecReg, firstVecReg, secondVecReg, vecSelectReg, 0, 0);

   cg->stopUsingRegister(firstVecReg);
   cg->stopUsingRegister(secondVecReg);
   cg->stopUsingRegister(vecSelectReg);

   cg->decReferenceCount(firstVecNode);
   cg->decReferenceCount(secondVecNode);
   cg->decReferenceCount(vecSelectNode);

   node->setRegister(returnVecReg);
   return returnVecReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::arraytranslateDecodeSIMDEvaluator(TR::Node * node, TR::CodeGenerator * cg, ArrayTranslateFlavor convType)
   {
   switch(convType)
      {
      // Find opportunities/uses
      case ByteToChar: cg->generateDebugCounter("z13/simd/ByteToChar", 1, TR::DebugCounter::Free); break;
      default: break;
      }

   //
   // tree looks as follows:
   // arraytranslate
   //    input ptr
   //    output ptr
   //    translation table
   //    terminating character
   //    input length (in characters)
   //    limit (stop) character
   //
   // Number of characters translated is returned
   //
   TR::Compilation* comp = cg->comp();

   // Create the necessary registers
   TR::Register* output = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register* input  = cg->gprClobberEvaluate(node->getChild(0));

   TR::Register* inputLen;
   TR::Register* inputLen16     = cg->allocateRegister();
   TR::Register* inputLenMinus1 = inputLen16;

   // The number of characters currently translated
   TR::Register* translated = cg->allocateRegister();

   TR::Node* inputLenNode = node->getChild(4);

   // Optimize the constant length case
   bool isLenConstant = inputLenNode->getOpCode().isLoadConst() && performTransformation(comp, "O^O [%p] Reduce input length to constant.\n", inputLenNode);

   // Initialize the number of translated characters to 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, translated, translated);

   if (isLenConstant)
      {
      inputLen = cg->allocateRegister();

      generateLoad32BitConstant(cg, inputLenNode, (getIntegralValue(inputLenNode)), inputLen, true);
      generateLoad32BitConstant(cg, inputLenNode, (getIntegralValue(inputLenNode) >> 4) << 4, inputLen16, true);
      }
   else
      {
      inputLen = cg->gprClobberEvaluate(inputLenNode, true);

      // Sign extend the value if needed
      if (cg->comp()->target().is64Bit() && !(inputLenNode->getOpCode().isLong()))
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen,   inputLen);
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen16, inputLen);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, inputLen16, inputLen);
         }

      // Truncate the 4 right most bits
      generateRIInstruction(cg, TR::InstOpCode::NILL, node, inputLen16, static_cast <int16_t> (0xFFF0));
      }

   // Create the necessary labels
   TR::LabelSymbol * processMultiple16Bytes    = generateLabelSymbol(cg);
   TR::LabelSymbol * processMultiple16BytesEnd = generateLabelSymbol(cg);

   TR::LabelSymbol * processUnder16Bytes    = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnder16BytesEnd = generateLabelSymbol(cg);

   TR::LabelSymbol * processSaturated         = generateLabelSymbol(cg);
   TR::LabelSymbol * processSaturatedEnd      = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnSaturatedInput1 = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnSaturatedInput2 = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd           = generateLabelSymbol(cg);

   // Create the necessary vector registers
   TR::Register* vOutput1   = cg->allocateRegister(TR_VRF); // 1st 16 bytes of output (8 chars)
   TR::Register* vOutput2   = cg->allocateRegister(TR_VRF); // 2nd 16 bytes of output (8 chars)
   TR::Register* vInput     = cg->allocateRegister(TR_VRF); // Input buffer
   TR::Register* vSaturated = cg->allocateRegister(TR_VRF); // Track index of first saturated char

   TR::Register* vRange        = cg->allocateRegister(TR_VRF);
   TR::Register* vRangeControl = cg->allocateRegister(TR_VRF);

   uint32_t saturatedRange        = getIntegralValue(node->getChild(5));
   uint16_t saturatedRangeControl = 0x20; // > comparison

   TR_ASSERT((convType == CharToByte || convType == CharToChar) ? saturatedRange <= 0xFFFF : saturatedRange <= 0xFF, "Value should be less be less than 16 (8) bit. Current value is %d", saturatedRange);

   // Replicate the limit character and comparison controller into vector registers
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vRange,        saturatedRange,        0);
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vRangeControl, saturatedRangeControl, 0);

   // Branch to the slow path if input is less than 16 bytes as we can only process multiples of 16 in vector registers
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen16, 0, TR::InstOpCode::COND_MASK8, processMultiple16BytesEnd, false, false);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processMultiple16Bytes);
   processMultiple16Bytes->setStartInternalControlFlow();

   // Load 16 bytes into a vector register and check for saturation
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, vInput, generateS390MemoryReference(input, translated, 0, cg));

   // Check for vector saturation and branch to copy the unsaturated bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSaturated, vInput, vRange, vRangeControl, 0x1, 0);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturated);

   // Unpack the 1st and 2nd halves of the input vector
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, vOutput1, vInput, 0, 0, 0);
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, vOutput2, vInput, 0, 0, 0);

   // Store the result and advance the output pointer
   generateVRSaInstruction(cg, TR::InstOpCode::VSTM, node, vOutput1, vOutput2, generateS390MemoryReference(output, 0, cg), 0);

   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, output, generateS390MemoryReference(output, 32, cg));
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, translated, generateS390MemoryReference(translated, 16, cg));

   // ed : perf : Use BRXLE (can't directly use BRCT since VL uses translated to iterate through input)
   // Loop back if there is at least 16 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, translated, inputLen16, TR::InstOpCode::COND_BL, processMultiple16Bytes, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processMultiple16BytesEnd);
   processMultiple16BytesEnd->setEndInternalControlFlow();

   // ----------------- Incoming branch -----------------

   // Slow path in the event of fewer than 16 bytes left to unpack
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder16Bytes);
   processUnder16Bytes->setStartInternalControlFlow();

   // Calculate the number of residue bytes available
   generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, inputLen, translated);

   // Branch to the end if there is no residue
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, cFlowRegionEnd);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   // Zero out the input register to avoid invalid VSTRC result
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vInput, 0, 0 /*unused*/);

   // VLL instruction can only handle memory references of type D(B), so increment the base input address
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, input, translated);

   // Load residue bytes into vector register
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, vInput, inputLenMinus1, generateS390MemoryReference(input, 0, cg));

   // Check for vector saturation and branch to copy the unsaturated bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSaturated, vInput, vRange, vRangeControl, 0x1, 0);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturated);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processSaturatedEnd);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder16BytesEnd);
   processUnder16BytesEnd->setEndInternalControlFlow();

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSaturated);
   processSaturated->setStartInternalControlFlow();

   // Extract the index of the first saturated byte
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, inputLen, vSaturated, generateS390MemoryReference(7, cg), 0);

   // Return in the case of saturation at index 0
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen, 0, TR::InstOpCode::COND_CC0, cFlowRegionEnd, false, false);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSaturatedEnd);

   // Update the result value
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, translated, inputLen);

   // Multiply by 2 as the unit length of each element in the vector register is 2 bytes
   generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, inputLenMinus1, inputLenMinus1, 1);

   // (x - 1) * 2 = 2x - 2 but we want 2x - 1
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, inputLenMinus1, 1);

   // Branch to copy the unsaturated results from the 1st and 2nd halves
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLenMinus1, 15, TR::InstOpCode::COND_CC2, processUnSaturatedInput2, false, false);

   // ----------------- Incoming branch -----------------

   // Case 1: The 1st half of the input vector contains a saturated byte
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnSaturatedInput1);

   // Unpack the 1st half of the input vector
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, vOutput1, vInput, 0, 0, 0);

   // Copy only the unsaturated results from the 1st vector using the index we calculated earlier
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vOutput1, inputLenMinus1, generateS390MemoryReference(output, 0, cg), 0);

   // Result has already been updated, so return
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, cFlowRegionEnd);

   // ----------------- Incoming branch -----------------

   // Case 2: The 2nd half of the input vector contains a saturated byte
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnSaturatedInput2);

   // Unpack the 1st and 2nd halves of the input vector
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, vOutput1, vInput, 0, 0, 0);
   generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, vOutput2, vInput, 0, 0, 0);

   // The 1st output vector contains all unsaturated bytes so we can copy them all
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, vOutput1, generateS390MemoryReference(output, 0, cg));

   // VSTL instruction can only handle memory references of type D(B), so increment the base output address
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, output, 16);

   // Adjust the unsaturated index as we copied 16 bytes already
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, inputLenMinus1, -16);

   // Copy only the unsaturated results from the 2nd vector using the index we calculated earlier
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vOutput2, inputLenMinus1, generateS390MemoryReference(output, 0, cg), 0);

   // Set up the proper register dependencies
   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 11, cg);

   dependencies->addPostCondition(input,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen,   TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen16, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(output,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(translated, TR::RealRegister::AssignAny);

   // These two need to be adjacent since we use VSTM
   dependencies->addPostCondition(vOutput1,   TR::RealRegister::VRF16);
   dependencies->addPostCondition(vOutput2,   TR::RealRegister::VRF17);
   dependencies->addPostCondition(vInput,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vSaturated, TR::RealRegister::AssignAny);

   dependencies->addPostCondition(vRange, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vRangeControl, TR::RealRegister::AssignAny);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   // Cleanup nodes before returning
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(4));

   cg->recursivelyDecReferenceCount(node->getChild(2)); // Unevaluated
   cg->recursivelyDecReferenceCount(node->getChild(3)); // Unevaluated
   cg->recursivelyDecReferenceCount(node->getChild(5)); // Unevaluated

   // Cleanup registers before returning
   cg->stopUsingRegister(output);
   cg->stopUsingRegister(input);
   cg->stopUsingRegister(inputLen);
   cg->stopUsingRegister(inputLen16);

   cg->stopUsingRegister(vOutput1);
   cg->stopUsingRegister(vOutput2);
   cg->stopUsingRegister(vInput);
   cg->stopUsingRegister(vSaturated);
   cg->stopUsingRegister(vRange);
   cg->stopUsingRegister(vRangeControl);

   return node->setRegister(translated);
   }

TR::Register *
OMR::Z::TreeEvaluator::arraytranslateEncodeSIMDEvaluator(TR::Node * node, TR::CodeGenerator * cg, ArrayTranslateFlavor convType)
   {
   switch(convType)
      {
      // Find opportunities/uses
      case CharToByte: cg->generateDebugCounter("z13/simd/CharToByte", 1, TR::DebugCounter::Free); break;
      default: break;
      }

   //
   // tree looks as follows:
   // arraytranslate
   //    input ptr
   //    output ptr
   //    translation table
   //    terminating character
   //    input length (in characters)
   //    limit (stop) character
   //
   // Number of characters translated is returned
   //
   TR::Compilation* comp = cg->comp();

   // Create the necessary registers
   TR::Register* output = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register* input  = cg->gprClobberEvaluate(node->getChild(0));

   TR::Register* inputLen;
   TR::Register* inputLen16     = cg->allocateRegister();
   TR::Register* inputLenMinus1 = inputLen16;

   // Number of characters currently translated
   TR::Register* translated = cg->allocateRegister();

   TR::Node* inputLenNode = node->getChild(4);

   // Optimize the constant length case
   bool isLenConstant = inputLenNode->getOpCode().isLoadConst() && performTransformation(comp, "O^O [%p] Reduce input length to constant.\n", inputLenNode);

   // Initialize the number of translated characters to 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, translated, translated);

   if (isLenConstant)
      {
      inputLen = cg->allocateRegister();

      generateLoad32BitConstant(cg, inputLenNode, (getIntegralValue(inputLenNode)), inputLen, true);
      generateLoad32BitConstant(cg, inputLenNode, (getIntegralValue(inputLenNode) >> 4) << 4, inputLen16, true);
      }
   else
      {
      inputLen = cg->gprClobberEvaluate(inputLenNode, true);

      // Sign extend the value if needed
      if (cg->comp()->target().is64Bit() && !(inputLenNode->getOpCode().isLong()))
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen,   inputLen);
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen16, inputLen);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, inputLen16, inputLen);
         }

      // Truncate the 4 right most bits
      generateRIInstruction(cg, TR::InstOpCode::NILL, node, inputLen16, static_cast <int16_t> (0xFFF0));
      }

   // Create the necessary labels
   TR::LabelSymbol * processMultiple16Chars    = generateLabelSymbol(cg);
   TR::LabelSymbol * processMultiple16CharsEnd = generateLabelSymbol(cg);

   TR::LabelSymbol * processUnder16Chars    = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnder16CharsEnd = generateLabelSymbol(cg);

   TR::LabelSymbol * processUnder8Chars    = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnder8CharsEnd = generateLabelSymbol(cg);

   TR::LabelSymbol * processSaturatedInput1 = generateLabelSymbol(cg);
   TR::LabelSymbol * processSaturatedInput2 = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnSaturated     = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd         = generateLabelSymbol(cg);

   // Create the necessary vector registers
   TR::Register* vInput1    = cg->allocateRegister(TR_VRF); // 1st 16 bytes of input (8 chars)
   TR::Register* vInput2    = cg->allocateRegister(TR_VRF); // 2nd 16 bytes of input (8 chars)
   TR::Register* vOutput    = cg->allocateRegister(TR_VRF); // Output buffer
   TR::Register* vSaturated = cg->allocateRegister(TR_VRF); // Track index of first saturated char

   TR::Register* vRange        = cg->allocateRegister(TR_VRF);
   TR::Register* vRangeControl = cg->allocateRegister(TR_VRF);

   uint32_t saturatedRange        = getIntegralValue(node->getChild(5));
   uint16_t saturatedRangeControl = 0x2000; // > comparison

   TR_ASSERT((convType == CharToByte || convType == CharToChar) ? saturatedRange <= 0xFFFF : saturatedRange <= 0xFF, "Value should be less be less than 16 (8) bit. Current value is %d", saturatedRange);

   // Replicate the limit character and comparison controller into vector registers
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vRange,        saturatedRange,        1);
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vRangeControl, saturatedRangeControl, 1);

   // Branch to the end if there are no more multiples of 16 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen16, 0, TR::InstOpCode::COND_MASK8, processMultiple16CharsEnd, false, false);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processMultiple16Chars);
   processMultiple16Chars->setStartInternalControlFlow();

   // Load 32 bytes (16 chars) into vector registers and check for saturation
   generateVRSaInstruction(cg, TR::InstOpCode::VLM, node, vInput1, vInput2, generateS390MemoryReference(input, 0, cg), 0);

   // Check for vector saturation and branch to copy the unsaturated bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSaturated, vInput1, vRange, vRangeControl, 0x1, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturatedInput1);
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSaturated, vInput2, vRange, vRangeControl, 0x1, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturatedInput2);

   // Pack the 2 byte characters into 1 byte
   generateVRRbInstruction(cg, TR::InstOpCode::VPKLS, node, vOutput, vInput1, vInput2, 0x0, 1);

   // Store the result and advance the input pointer
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, vOutput, generateS390MemoryReference(output, translated, 0, cg));

   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, input, generateS390MemoryReference(input, 32, cg));
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, translated, generateS390MemoryReference(translated, 16, cg));

   // ed : perf : Use BRXLE and get rid of the LA translated
   // Loop back if there is at least 16 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, translated, inputLen16, TR::InstOpCode::COND_BL, processMultiple16Chars, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processMultiple16CharsEnd);
   processMultiple16CharsEnd->setEndInternalControlFlow();

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder16Chars);
   processUnder16Chars->setStartInternalControlFlow();

   // Calculate the number of residue chars available
   generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, inputLen, translated);

   // Branch to the end if there is no residue
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, cFlowRegionEnd);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLenMinus1, 7, TR::InstOpCode::COND_CC2, processUnder8CharsEnd, false, false);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder8Chars);
   processUnder8Chars->setStartInternalControlFlow();

   // Zero out the input register to avoid invalid VSTRC result
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vInput1, 0, 0 /*unused*/);

   // Convert input length in number of characters to number of bytes
   generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, inputLenMinus1, inputLenMinus1, 1);

   // (x - 1) * 2 = 2x - 2 but we want 2x - 1
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, inputLenMinus1, 1);

   // Load residue bytes and check for saturation
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, vInput1, inputLenMinus1, generateS390MemoryReference(input, 0, cg));

   // Check for vector saturation and branch to copy the unsaturated bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSaturated, vInput1, vRange, vRangeControl, 0x1, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturatedInput1);

   // Convert back
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, inputLenMinus1, inputLenMinus1, 1);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processUnSaturated);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder8CharsEnd);
   processUnder8CharsEnd->setEndInternalControlFlow();

   // Load first 8 chars and check for saturation
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, vInput1, generateS390MemoryReference(input, 0, cg));

   // Check for vector saturation and branch to copy the unsaturated bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSaturated, vInput1, vRange, vRangeControl, 0x1, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturatedInput1);

   // Subtract 8 from total residue count
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, inputLenMinus1, -8);

   // Zero out the input register to avoid invalid VSTRC result
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vInput2, 0, 0 /*unused*/);

   // Convert input length in number of characters to number of bytes
   generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, inputLenMinus1, inputLenMinus1, 1);

   // (x - 1) * 2 = 2x - 2 but we want 2x - 1
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, inputLenMinus1,  generateS390MemoryReference(inputLenMinus1 , 1, cg));

   // Load residue bytes and check for saturation
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, vInput2, inputLenMinus1, generateS390MemoryReference(input, 16, cg));

   // Check for vector saturation and branch to copy the unsaturated bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSaturated, vInput2, vRange, vRangeControl, 0x1, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSaturatedInput2);

   // Convert back
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, inputLenMinus1, inputLenMinus1, 1);

   // Add 8 to the total residue count
   generateRIInstruction (cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, inputLenMinus1, 8);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processUnSaturated);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder16CharsEnd);
   processUnder16CharsEnd->setEndInternalControlFlow();

   // ----------------- Incoming branch -----------------

   // Case 1: The 1st vector register contained a saturated char
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSaturatedInput1);

   // Extract the index of the first saturated char
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, inputLen, vSaturated, generateS390MemoryReference(7, cg), 0);

   // Return in the case of saturation at index 0
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen, 0, TR::InstOpCode::COND_CC0, cFlowRegionEnd, false, false);

   // Divide by 2 as the unit length of each element in the vector register is 2 bytes
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, inputLen, inputLen, 1);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   // Branch to copy the unsaturated results
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processUnSaturated);

   // ----------------- Incoming branch -----------------

   // Case 2: The 2nd vector register contained a saturated char
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSaturatedInput2);

   // Extract the index of the first saturated char in the 2nd vector register
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, inputLen, vSaturated, generateS390MemoryReference(7, cg), 0);

   // Divide by 2 as the unit length of each element in the vector register is 2 bytes
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, inputLen, inputLen, 1);

   // VLL and VSTL work on indices so we must subtract 1 (add 8 as saturation happened in the second input vector)
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, 7);

   // If we end up in this ICF block, then it means that all 8 characters in the first input vector (vInput1) can be
   // succesfully translated and some portion of the 8 chars in vInput2 can also be translated. Currently inputLen only
   // indicates how many chars in vInput2 will be successfully translated. We add 8 to inputLen here to account for
   // the 8 chars that will be successfully translated in vInput1.
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, inputLen, 8);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnSaturated);
   processUnSaturated->setStartInternalControlFlow();

   // Pack the chars regardless of saturation
   generateVRRbInstruction(cg, TR::InstOpCode::VPKLS, node, vOutput, vInput1, vInput2, 0x0, 1);

   // VSTL instruction can only handle memory references of type D(B), so increment the base output address
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, output, translated);

   // Copy only the unsaturated results using the index we calculated earlier
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vOutput, inputLenMinus1, generateS390MemoryReference(output, 0, cg), 0);

   // inputLen now holds the number of characters we were able to translate before encountering the saturated
   // character. Add it to translated register here to update the result value.
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, translated, inputLen);

   // Set up the proper register dependencies
   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 11, cg);

   dependencies->addPostCondition(input,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen,   TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen16, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(output,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(translated, TR::RealRegister::AssignAny);

   // These two need to be adjacent since we use VLM
   dependencies->addPostCondition(vInput1,       TR::RealRegister::VRF16);
   dependencies->addPostCondition(vInput2,       TR::RealRegister::VRF17);
   dependencies->addPostCondition(vOutput,       TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vSaturated,    TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vRange,        TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vRangeControl, TR::RealRegister::AssignAny);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   // Cleanup nodes before returning
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(4));

   cg->recursivelyDecReferenceCount(node->getChild(2)); // Unevaluated
   cg->recursivelyDecReferenceCount(node->getChild(3)); // Unevaluated
   cg->recursivelyDecReferenceCount(node->getChild(5)); // Unevaluated

   // Cleanup registers before returning
   cg->stopUsingRegister(output);
   cg->stopUsingRegister(input);
   cg->stopUsingRegister(inputLen);
   cg->stopUsingRegister(inputLen16);

   cg->stopUsingRegister(vInput1);
   cg->stopUsingRegister(vInput2);
   cg->stopUsingRegister(vOutput);
   cg->stopUsingRegister(vSaturated);
   cg->stopUsingRegister(vRange);
   cg->stopUsingRegister(vRangeControl);

   return node->setRegister(translated);
   }

/*This Evaluator generates the SIMD routine for methods
 * java/lang/String.hashCodeImplCompressed and
 * java/lang/String.hashCodeImplDecompressed depending on the "isCompressed"
 * parameter passed to it.
 */
TR::Register *
inlineStringHashCode(TR::Node* node, TR::CodeGenerator* cg, bool isCompressed)
   {
   TR::Compilation* comp = cg->comp();
   //stringSize = Number of bytes to load to process 4 characters in SIMD loop
   //terminateVal = SIMD loop cotroller allowing characters in multiple of 4 to be processes by loop
   //VLLEZ instruction will load word(compressed String) or double word (decompressed String), elementSize is used for that
   const short stringSize = (isCompressed ? 4 : 8);
   const short terminateVal = (isCompressed ? 3 : 6);
   const short elementSize = (isCompressed ? 2 : 3);

   TR::Node* nodeValue = node->getChild(0);
   TR::Node* nodeIndex = node->getChild(1);
   TR::Node* nodeCount = node->getChild(2);

   // Create the necessary labels
   TR::LabelSymbol * cFlowRegionStart  = generateLabelSymbol(cg);

   TR::LabelSymbol * labelVector       = generateLabelSymbol(cg);
   TR::LabelSymbol * labelVectorLoop   = generateLabelSymbol(cg);
   TR::LabelSymbol * labelVectorReduce = generateLabelSymbol(cg);

   TR::LabelSymbol * labelSerial       = generateLabelSymbol(cg);

   TR::LabelSymbol * labelSerialLoop   = generateLabelSymbol(cg);

   TR::LabelSymbol * cFlowRegionEnd    = generateLabelSymbol(cg);

   // Create the necessary registers
   TR::Register* registerHash = cg->allocateRegister();

   TR::Register* registerValue = cg->evaluate(nodeValue);
   TR::Register* registerIndex = cg->gprClobberEvaluate(nodeIndex);
   TR::Register* registerCount = cg->gprClobberEvaluate(nodeCount);

   if (cg->comp()->target().is64Bit())
      {
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, registerIndex, registerIndex);
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, registerCount, registerCount);
      }

   TR::Register* registerVA = cg->allocateRegister(TR_VRF);
   TR::Register* registerVB = cg->allocateRegister(TR_VRF);
   TR::Register* registerVC = cg->allocateRegister(TR_VRF);

   TR::Register* registerEnd = cg->allocateRegister(TR_GPR);

   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 12, cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();

   if(!isCompressed)
      {
      // registerIndex *= 2 and registerCount *= 2
      generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, registerIndex, registerIndex, 1);
      generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, registerCount, registerCount, 1);
      }

   // registerEnd = registerIndex + registerCount
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerEnd, generateS390MemoryReference(registerIndex, registerCount, 0, cg));

   // registerHash = 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, registerHash, registerHash);

   // Branch to labelSerial if registerCount < stringSize
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, registerCount, static_cast<int32_t>(stringSize), TR::InstOpCode::COND_MASK4, labelSerial, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelVector);

   // registerEnd -= terminateVal
   generateRILInstruction(cg, TR::InstOpCode::getSubtractLogicalImmOpCode(), node, registerEnd, terminateVal);

   // snippetData1 = [31^4, 31^4, 31^4, 31^4]
   int32_t snippetData1[4] = {923521, 923521, 923521, 923521};

   TR::MemoryReference* memrefSnippet1 = generateS390MemoryReference(cg->findOrCreateConstant(node, snippetData1, 16), cg, 0, node);

   dependencies->addAssignAnyPostCondOnMemRef(memrefSnippet1);

   // registerVA = snippetData1
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, registerVA, memrefSnippet1);

   // registerVB = 0
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, registerVB, 0, 0 /*unused*/);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelVectorLoop);

   // registerVC = 4 consecutive chars (16 bit shorts or 8 bit bytes depending on String Compression) at the current index
   generateVRXInstruction(cg, TR::InstOpCode::VLLEZ, node, registerVC, generateS390MemoryReference(registerValue, registerIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg), elementSize);

   if (!isCompressed)
      {
      // registerVC = unpack 4 (16 bit) short elements into 4 (32 bit) int elements
      generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, registerVC, registerVC, 0, 0, 1);
      }
   else
      {
      // registerVC = unpack 4 (8 bit) byte elements into 4 (32 bit) int elements
      generateVRRaInstruction(cg, TR::InstOpCode::VUPLH, node, registerVC, registerVC, 0, 0, 0);
      generateVRRaInstruction(cg, TR::InstOpCode::VUPLL, node, registerVC, registerVC, 0, 0, 1);
      }

   // registerVB = registerVB * registerVA + registerVC
   generateVRRdInstruction(cg, TR::InstOpCode::VMAL, node, registerVB, registerVB, registerVA, registerVC, 0, 2);

   // registerIndex += stringSize
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerIndex, generateS390MemoryReference(registerIndex, stringSize, cg));

   // Branch to labelVectorLoop if registerIndex < registerEnd
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, registerIndex, registerEnd, TR::InstOpCode::COND_MASK4, labelVectorLoop, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelVectorReduce);

   // snippetData2 = [31^3, 31^2, 31^1, 31^0]
   int32_t snippetData2[4] = {29791, 961, 31, 1};

   TR::MemoryReference* memrefSnippet2 = generateS390MemoryReference(cg->findOrCreateConstant(node, snippetData2, 16), cg, 0, node);

   dependencies->addAssignAnyPostCondOnMemRef(memrefSnippet2);

   // registerVA = snippetData2
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, registerVA, memrefSnippet2);

   // registerVB = registerVB * registerVA
   generateVRRcInstruction(cg, TR::InstOpCode::VML, node, registerVB, registerVB, registerVA, 2);

   // registerVA = 0
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, registerVA, 0, 0 /*unused*/);

   // registerVA = sum of 4 (32 bit) int elements
   generateVRRcInstruction(cg, TR::InstOpCode::VSUMQ, node, registerVA, registerVB, registerVA, 0, 0, 2);

   // registerEnd += terminateVal
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerEnd, generateS390MemoryReference(registerEnd, terminateVal, cg));

   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, registerHash, registerVA, generateS390MemoryReference(3, cg), 2);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelSerial);
   labelSerial->setEndInternalControlFlow();

   // Branch to labelEnd if registerIndex >= registerEnd
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, registerIndex, registerEnd, TR::InstOpCode::COND_MASK10, cFlowRegionEnd, false, false);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, labelSerialLoop);
   labelSerialLoop->setStartInternalControlFlow();

   TR::Register* registerTemp = registerCount;

   // registerTemp = registerHash << 5
   generateRSInstruction(cg, TR::InstOpCode::SLLK, node, registerTemp, registerHash, 5);

   // registerTemp -= registerHash
   generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, registerTemp, registerHash);

   // registerHash = char at registerIndex
   if(isCompressed)
      generateRXInstruction(cg, TR::InstOpCode::LLGC, node, registerHash, generateS390MemoryReference(registerValue, registerIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));
   else
      generateRXInstruction(cg, TR::InstOpCode::LLH, node, registerHash, generateS390MemoryReference(registerValue, registerIndex, TR::Compiler->om.contiguousArrayHeaderSizeInBytes(), cg));

   if(isCompressed) //registerIndex += 1
      generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerIndex, generateS390MemoryReference(registerIndex, 1, cg));
   else //registerIndex += 2
      generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, registerIndex, generateS390MemoryReference(registerIndex, 2, cg));


   // registerHash += registerTemp
   generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, registerHash, registerTemp);

   // Branch to labelSerialLoop if registerIndex < registerEnd
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalRegOpCode(), node, registerIndex, registerEnd, TR::InstOpCode::COND_MASK4, labelSerialLoop, false, false);

   // Set up the proper register dependencies
   dependencies->addPostConditionIfNotAlreadyInserted(registerValue, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerIndex, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerCount, TR::RealRegister::AssignAny);

   dependencies->addPostConditionIfNotAlreadyInserted(registerHash, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerEnd, TR::RealRegister::AssignAny);

   dependencies->addPostConditionIfNotAlreadyInserted(registerVA, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerVB, TR::RealRegister::AssignAny);
   dependencies->addPostConditionIfNotAlreadyInserted(registerVC, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   // Cleanup nodes before returning
   cg->decReferenceCount(nodeValue);
   cg->decReferenceCount(nodeIndex);
   cg->decReferenceCount(nodeCount);

   // Cleanup registers before returning
   cg->stopUsingRegister(registerValue);
   cg->stopUsingRegister(registerIndex);
   cg->stopUsingRegister(registerCount);

   cg->stopUsingRegister(registerEnd);

   cg->stopUsingRegister(registerVA);
   cg->stopUsingRegister(registerVB);
   cg->stopUsingRegister(registerVC);

   return node->setRegister(registerHash);
   }

TR::Register *
inlineUTF16BEEncodeSIMD(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation* comp = cg->comp();

   // Create the necessary registers
   TR::Register* output = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register* input  = cg->gprClobberEvaluate(node->getChild(0));

   TR::Register* inputLen;
   TR::Register* inputLen16 = cg->allocateRegister();
   TR::Register* inputLenMinus1 = inputLen16;

   // Number of characters currently translated
   TR::Register* translated = cg->allocateRegister();

   // Initialize the number of translated characters to 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, translated, translated);

   TR::Node* inputLenNode = node->getChild(2);

   // Optimize the constant length case
   bool isLenConstant = inputLenNode->getOpCode().isLoadConst() && performTransformation(comp, "O^O [%p] Reduce input length to constant.\n", inputLenNode);

   if (isLenConstant)
      {
      inputLen = cg->allocateRegister();

      // Convert input length in number of characters to number of bytes
      generateLoad32BitConstant(cg, inputLenNode, ((getIntegralValue(inputLenNode) * 2)), inputLen, true);
      generateLoad32BitConstant(cg, inputLenNode, ((getIntegralValue(inputLenNode) * 2) >> 4) << 4, inputLen16, true);
      }
   else
      {
      inputLen = cg->gprClobberEvaluate(inputLenNode, true);

      // Convert input length in number of characters to number of bytes
      generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, inputLen, inputLen, 1);

      // Sign extend the value if needed
      if (cg->comp()->target().is64Bit() && !(inputLenNode->getOpCode().isLong()))
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen,   inputLen);
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegWidenOpCode(), node, inputLen16, inputLen);
         }
      else
         {
         generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, inputLen16, inputLen);
         }

      // Truncate the 4 right most bits
      generateRIInstruction(cg, TR::InstOpCode::NILL, node, inputLen16, static_cast <int16_t> (0xFFF0));
      }

   // Create the necessary vector registers
   TR::Register* vInput     = cg->allocateRegister(TR_VRF);
   TR::Register* vSurrogate = cg->allocateRegister(TR_VRF); // Track index of first surrogate char

   TR::Register* vRange        = cg->allocateRegister(TR_VRF);
   TR::Register* vRangeControl = cg->allocateRegister(TR_VRF);

   // Initialize the vector registers
   uint16_t surrogateRange1 = 0xD800;
   uint16_t surrogateRange2 = 0xDFFF;

   uint16_t surrogateControl1 = 0xA000; // >= comparison
   uint16_t surrogateControl2 = 0xC000; // <= comparison

   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vRange, 0, 0 /*unused*/);
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vRangeControl, 0, 0 /*unused*/);

   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRange, surrogateRange1, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRange, surrogateRange2, 1);

   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRangeControl, surrogateControl1, 0);
   generateVRIaInstruction(cg, TR::InstOpCode::VLEIH, node, vRangeControl, surrogateControl2, 1);

   // Create the necessary labels
   TR::LabelSymbol * process8Chars         = generateLabelSymbol(cg);
   TR::LabelSymbol * process8CharsEnd      = generateLabelSymbol(cg);

   TR::LabelSymbol * processUnder8Chars    = generateLabelSymbol(cg);
   TR::LabelSymbol * processUnder8CharsEnd = generateLabelSymbol(cg);

   TR::LabelSymbol * processSurrogate      = generateLabelSymbol(cg);
   TR::LabelSymbol * processSurrogateEnd   = generateLabelSymbol(cg);

   // Branch to the end if there are no more multiples of 8 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen16, 0, TR::InstOpCode::COND_MASK8, process8CharsEnd, false, false);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, process8Chars);
   process8Chars->setStartInternalControlFlow();

   // Load 16 bytes (8 chars) into vector register
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, vInput, generateS390MemoryReference(input, translated, 0, cg));

   // Check for vector surrogates and branch to copy the non-surrogate bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSurrogate, vInput, vRange, vRangeControl, 0x1, 1);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processSurrogate);

   // Store the result
   generateVRXInstruction(cg, TR::InstOpCode::VST, node, vInput, generateS390MemoryReference(output, translated, 0, cg));

   // Advance the stride register
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, translated, 16);

   // Loop back if there is at least 8 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, translated, inputLen16, TR::InstOpCode::COND_BL, process8Chars, false, false);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, process8CharsEnd);
   process8CharsEnd->setEndInternalControlFlow();

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder8Chars);
   processUnder8Chars->setStartInternalControlFlow();

   // Calculate the number of residue bytes available
   generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, inputLen, translated);

   // Branch to the end if there is no residue
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC0, node, processUnder8CharsEnd);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   // Zero out the input register to avoid invalid VSTRC result
   generateVRIaInstruction(cg, TR::InstOpCode::VGBM, node, vInput, 0, 0 /*unused*/);

   // VLL instruction can only handle memory references of type D(B), so increment the base input address
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, input, translated);

   // Load residue bytes into vector register
   generateVRSbInstruction(cg, TR::InstOpCode::VLL, node, vInput, inputLenMinus1, generateS390MemoryReference(input, 0, cg));

   // Check for vector surrogates and branch to copy the non-surrogate bytes
   generateVRRdInstruction(cg, TR::InstOpCode::VSTRC, node, vSurrogate, vInput, vRange, vRangeControl, 0x1, 1);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC3, node, processSurrogateEnd);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSurrogate);

   // Extract the index of the first surrogate char
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, inputLen, vSurrogate, generateS390MemoryReference(7, cg), 0);

   // Return in the case of saturation at index 0
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen, 0, TR::InstOpCode::COND_CC0, processUnder8CharsEnd, false, false);

   // VLL and VSTL work on indices so we must subtract 1
   generateRIEInstruction(cg, TR::InstOpCode::getAddLogicalRegRegImmediateOpCode(), node, inputLenMinus1, inputLen, -1);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processSurrogateEnd);

   // VSTL instruction can only handle memory references of type D(B), so increment the base output address
   generateRRInstruction (cg, TR::InstOpCode::getAddRegOpCode(), node, output, translated);

   // Store the result
   generateVRSbInstruction(cg, TR::InstOpCode::VSTL, node, vInput, inputLenMinus1, generateS390MemoryReference(output, 0, cg), 0);

   // Advance the stride register
   generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, translated, inputLen);

   // Set up the proper register dependencies
   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 9, cg);

   dependencies->addPostCondition(input,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen,   TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen16, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(output,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(translated, TR::RealRegister::AssignAny);

   dependencies->addPostCondition(vInput,        TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vSurrogate,    TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vRange,        TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vRangeControl, TR::RealRegister::AssignAny);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processUnder8CharsEnd, dependencies);
   processUnder8CharsEnd->setEndInternalControlFlow();

   // Convert translated length in number of bytes to number of characters
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, translated, translated, 1);

   // Cleanup nodes before returning
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));

   // Cleanup registers before returning
   cg->stopUsingRegister(input);
   cg->stopUsingRegister(inputLen);
   cg->stopUsingRegister(inputLen16);
   cg->stopUsingRegister(output);

   cg->stopUsingRegister(vInput);
   cg->stopUsingRegister(vSurrogate);
   cg->stopUsingRegister(vRange);
   cg->stopUsingRegister(vRangeControl);

   return node->setRegister(translated);
   }

TR::Register*
inlineUTF16BEEncode(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation* comp = cg->comp();

   // Create the necessary registers
   TR::Register* output    = cg->gprClobberEvaluate(node->getChild(1));
   TR::Register* input     = cg->gprClobberEvaluate(node->getChild(0));

   TR::Register* inputLen  = cg->gprClobberEvaluate(node->getChild(2));
   TR::Register* inputLen8 = cg->allocateRegister();

   TR::Register* temp1 = cg->allocateRegister();
   TR::Register* temp2 = cg->allocateRegister();

   // Number of bytes currently translated (also used as a stride register)
   TR::Register* translated = cg->allocateRegister();

   // Convert input length in number of characters to number of bytes
   generateRSInstruction(cg, TR::InstOpCode::getShiftLeftLogicalSingleOpCode(), node, inputLen, inputLen, 1);

   // Calculate inputLen8 = inputLen / 8
   generateRSInstruction(cg, TR::InstOpCode::SRLK, node, inputLen8, inputLen, 3);

   // Initialize the number of translated bytes to 0
   generateRREInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, translated, translated);

   // Create the necessary labels
   TR::LabelSymbol * processChar4     = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar4End  = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar1     = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar1End  = generateLabelSymbol( cg);
   TR::LabelSymbol * processChar1Copy = generateLabelSymbol( cg);

   const uint16_t surrogateRange1 = 0xD800;
   const uint16_t surrogateRange2 = 0xDFFF;

   const uint32_t surrogateMaskAND = 0xF800F800;
   const uint32_t surrogateMaskXOR = 0xD800D800;

   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 7, cg);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processChar4);
   processChar4->setStartInternalControlFlow();

   // Branch to the end if there are no more multiples of 4 chars left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpLogicalOpCode(), node, inputLen8, 0, TR::InstOpCode::COND_MASK8, processChar4End, false, false, NULL, dependencies);

   // Load 4 input characters from memory and make a copy
   generateRXInstruction(cg, TR::InstOpCode::LG,  node, temp1, generateS390MemoryReference(input, translated, 0, cg));
   generateRREInstruction(cg, TR::InstOpCode::LGR, node, temp2, temp1);

   // AND temp2 by the surrogate mask
   generateRILInstruction(cg, TR::InstOpCode::NIHF, node, temp2, surrogateMaskAND);
   generateRILInstruction(cg, TR::InstOpCode::NILF, node, temp2, surrogateMaskAND);

   // XOR temp2 by the surrogate mask and branch if CC = 1 (meaning there is a surrogate)
   generateRILInstruction(cg, TR::InstOpCode::XIHF, node, temp2, surrogateMaskXOR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processChar4End);
   generateRILInstruction(cg, TR::InstOpCode::XILF, node, temp2, surrogateMaskXOR);
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_CC1, node, processChar4End);

   generateRXInstruction(cg, TR::InstOpCode::STG, node, temp1, generateS390MemoryReference(output, translated, 0, cg));

   // Advance the number of bytes processed
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, translated, 8);

   // Branch back to the start of the loop
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processChar4);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processChar4End);
   processChar4End->setEndInternalControlFlow();
   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processChar1);
   processChar1->setStartInternalControlFlow();

   // Branch to the end if there are no more characters left to process
   generateS390CompareAndBranchInstruction(cg, TR::InstOpCode::getCmpRegOpCode(), node, translated, inputLen, TR::InstOpCode::COND_BNL, processChar1End, false, false);

   // Load an input character from memory
   generateRXInstruction(cg, TR::InstOpCode::LLH, node, temp1, generateS390MemoryReference(input, translated, 0, cg));

   // Compare the input character against the lower bound surrogate character range
   generateRILInstruction(cg, TR::InstOpCode::getCmpImmOpCode(), node, temp1, surrogateRange1);

   // Branch if < (non-surrogate char)
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK4, node, processChar1Copy);

   // Compare the input character against the upper bound surrogate character range
   generateRILInstruction(cg, TR::InstOpCode::getCmpImmOpCode(), node, temp1, surrogateRange2);

   // Branch if > (non-surrogate char)
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK2, node, processChar1Copy);

   // If we get here it must be a surrogate char
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processChar1End);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processChar1Copy);

   // Store the lower byte of the character into the output buffer
   generateRXInstruction (cg, TR::InstOpCode::STH, node, temp1, generateS390MemoryReference(output, translated, 0, cg));

   // Advance the number of bytes processed
   generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, translated, 2);

   // Branch back to the start of the loop
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK15, node, processChar1);

   // Set up the proper register dependencies
   dependencies->addPostCondition(input,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen,   TR::RealRegister::AssignAny);
   dependencies->addPostCondition(inputLen8,  TR::RealRegister::AssignAny);
   dependencies->addPostCondition(temp1,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(temp2,      TR::RealRegister::AssignAny);
   dependencies->addPostCondition(output,     TR::RealRegister::AssignAny);
   dependencies->addPostCondition(translated, TR::RealRegister::AssignAny);

   // ----------------- Incoming branch -----------------

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, processChar1End, dependencies);
   processChar1End->setEndInternalControlFlow();

   // Convert translated length in number of bytes to number of characters
   generateRSInstruction(cg, TR::InstOpCode::getShiftRightLogicalSingleOpCode(), node, translated, translated, 1);

   // Cleanup nodes before returning
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   cg->decReferenceCount(node->getChild(2));

   // Cleanup registers before returning
   cg->stopUsingRegister(input);
   cg->stopUsingRegister(inputLen);
   cg->stopUsingRegister(inputLen8);
   cg->stopUsingRegister(temp1);
   cg->stopUsingRegister(temp2);
   cg->stopUsingRegister(output);

   return node->setRegister(translated);
   }

TR::Register *
OMR::Z::TreeEvaluator::arraycmpSIMDHelper(TR::Node *node,
      TR::CodeGenerator *cg,
      TR::LabelSymbol *compareTarget,
      TR::Node *ificmpNode,
      bool needResultReg,
      bool return102)
   {
   // Similar to arraycmpHelper, except it uses vector instructions and supports arraycmpsign and arraycmplen
   // Does not currently support aggregates or wide chars

   cg->generateDebugCounter("z13/simd/arraycmp", 1, TR::DebugCounter::Free);

   // Swap the order as then the condition codes of VFENE can be easily converted to -1/0/1
   TR::Node * firstAddrNode = return102 ? node->getFirstChild() : node->getSecondChild();
   TR::Node * secondAddrNode = return102 ? node->getSecondChild() : node->getFirstChild();
   TR::Node * elemsExpr = node->getChild(2);
   bool isFoldedIf = compareTarget != NULL;
   bool isArrayCmp = node->getOpCodeValue() == TR::arraycmp;
   TR::Compilation *comp = cg->comp();

   TR_ASSERT( !(isArrayCmp && node->isArrayCmpLen() && node->isArrayCmpSign()), "Invalid arraycmp node");

   TR::InstOpCode::S390BranchCondition ifxcmpBrCond = TR::InstOpCode::COND_NOP;
   if (isFoldedIf)
      {
      TR_ASSERT(ificmpNode, "ificmpNode is required for now");
      ifxcmpBrCond = getStandardIfBranchConditionForArraycmp(ificmpNode, cg);
      needResultReg = false;// Result register is not applicable for the isFoldedIf case
      }

   bool isIfxcmpBrCondContainEqual = getMaskForBranchCondition(ifxcmpBrCond) & 0x08;

   TR::Register * firstAddrReg = cg->gprClobberEvaluate(firstAddrNode);
   TR::Register * secondAddrReg = cg->gprClobberEvaluate(secondAddrNode);
   TR::Register * lastByteIndexReg = cg->gprClobberEvaluate(elemsExpr);
   TR::Register * vectorFirstInputReg = cg->allocateRegister(TR_VRF);
   TR::Register * vectorSecondInputReg = cg->allocateRegister(TR_VRF);
   TR::Register * vectorOutputReg = cg->allocateRegister(TR_VRF);
   TR::Register * resultReg = needResultReg ? cg->allocateRegister() : NULL;

   // VLL uses lastByteIndexReg as the highest 0-based index to load, which is length - 1
   generateRILInstruction(cg, TR::InstOpCode::getSubtractLogicalImmOpCode(), node, lastByteIndexReg, 1);
   if(needResultReg && isArrayCmp && node->isArrayCmpLen())
      generateRRInstruction(cg, TR::InstOpCode::getLoadRegOpCode(), node, resultReg, lastByteIndexReg);

   TR::LabelSymbol * cFlowRegionStart = generateLabelSymbol(cg);
   TR::LabelSymbol * cFlowRegionEnd = generateLabelSymbol(cg);
   TR::LabelSymbol * mismatch = generateLabelSymbol(cg);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionStart);
   cFlowRegionStart->setStartInternalControlFlow();

   // Vector is filled with data in memory in [addr,addr+min(15,numBytesLeft)]
   generateVRSbInstruction(cg, TR::InstOpCode::VLL  , node, vectorFirstInputReg , lastByteIndexReg, generateS390MemoryReference(firstAddrReg , 0, cg));
   generateVRSbInstruction(cg, TR::InstOpCode::VLL  , node, vectorSecondInputReg, lastByteIndexReg, generateS390MemoryReference(secondAddrReg, 0, cg));
   generateVRRbInstruction(cg, TR::InstOpCode::VFENE, node, vectorOutputReg, vectorFirstInputReg, vectorSecondInputReg, 1/*SET COND CODE*/, 0/*BYTE*/);

   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BNE, node, mismatch);

   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, firstAddrReg , generateS390MemoryReference(firstAddrReg , 16, cg));
   generateRXInstruction(cg, TR::InstOpCode::getLoadAddressOpCode(), node, secondAddrReg, generateS390MemoryReference(secondAddrReg, 16, cg));

   // eds : perf : Can use BRXLE with negative increment for this
   generateRILInstruction(cg, TR::InstOpCode::getSubtractLogicalImmOpCode(), node, lastByteIndexReg, 16);

   // ed : perf : replace these 2 with BRXLE
   //Branch if the number of bytes left is not negative
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_MASK3, node, cFlowRegionStart);

   // In certain cases we can branch directly so we must set up the correct global register dependencies
   TR::RegisterDependencyConditions* compareTargetRDC = getGLRegDepsDependenciesFromIfNode(cg, ificmpNode);

   // Arrays are equal
   if(isFoldedIf)
      {
      if(isIfxcmpBrCondContainEqual)
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, compareTarget, compareTargetRDC);
      else
         generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);
      }
   else if(needResultReg)
      {
      if(isArrayCmp && node->isArrayCmpLen())
         generateRIInstruction(cg, TR::InstOpCode::getAddHalfWordImmOpCode(), node, resultReg, 1);//Return length of the arrays, which is resultReg += 1
      else
         generateRRInstruction(cg, TR::InstOpCode::getXORRegOpCode(), node, resultReg, resultReg);//Return zero to indicate equal
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, TR::InstOpCode::COND_BRC, node, cFlowRegionEnd);
      }

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, mismatch);

   if(isFoldedIf)
      {
      generateS390BranchInstruction(cg, TR::InstOpCode::BRC, ifxcmpBrCond, node, compareTarget, compareTargetRDC);
      }
   else if(needResultReg)
      {
      if(isArrayCmp && node->isArrayCmpLen())
         {
         // Return 0-based index of first non-matching element
         // resultReg - lastByteIndexReg = number of elements compared before the last loop
         // vectorOutputReg contains the 0-based index of the first non-matching element (index is its position in the vector)
         generateRRInstruction(cg, TR::InstOpCode::getSubstractRegOpCode(), node, resultReg, lastByteIndexReg);
         generateVRScInstruction(cg, TR::InstOpCode::VLGV/*B*/, node, lastByteIndexReg, vectorOutputReg, generateS390MemoryReference(7, cg), 0);
         generateRRInstruction(cg, TR::InstOpCode::getAddRegOpCode(), node, resultReg, lastByteIndexReg);
         }
      else
         {
         if(return102)
            {
            // Return the condition code of VFENE
            getConditionCode(node, cg, resultReg);
            }
         else
            {
            // Return -1 or 1 based off the condition code of VFENE
            generateRRInstruction(cg, TR::InstOpCode::IPM, node, resultReg, resultReg);
            if (cg->comp()->target().is64Bit())
               {
               generateRSInstruction(cg, TR::InstOpCode::SLLG, node, resultReg, resultReg, 34);
               generateRSInstruction(cg, TR::InstOpCode::SRAG, node, resultReg, resultReg, 64-2);
               }
            else
               {
               generateRSInstruction(cg, TR::InstOpCode::SLL, node, resultReg, 34-32);
               generateRSInstruction(cg, TR::InstOpCode::SRA, node, resultReg, (64-2)-32);
               }
            }
         }
      }


   int numConditions = 8;
   TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, numConditions, cg);
   dependencies->addPostCondition(firstAddrReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(secondAddrReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(lastByteIndexReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vectorFirstInputReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vectorSecondInputReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(vectorOutputReg, TR::RealRegister::AssignAny);
   if (needResultReg)
      dependencies->addPostCondition(resultReg, TR::RealRegister::AssignAny);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   cg->stopUsingRegister(vectorFirstInputReg);
   cg->stopUsingRegister(vectorSecondInputReg);
   cg->stopUsingRegister(vectorOutputReg);
   cg->stopUsingRegister(firstAddrReg);
   cg->stopUsingRegister(secondAddrReg);
   cg->stopUsingRegister(lastByteIndexReg);

   cg->decReferenceCount(elemsExpr);
   cg->decReferenceCount(firstAddrNode);
   cg->decReferenceCount(secondAddrNode);

   if(isFoldedIf)
      cg->decReferenceCount(node);

   if (needResultReg)
      {
      node->setRegister(resultReg);
      return resultReg;
      }
   return NULL;
   }

int32_t getVectorElementSize(TR::Node *node)
   {
   switch(node->getDataType())
      {
      case TR::VectorInt8: return 1;
      case TR::VectorInt16: return 2;
      case TR::VectorInt32:
      case TR::VectorFloat: return 4;
      case TR::VectorInt64:
      case TR::VectorDouble: return 8;
      default: TR_ASSERT(false, "Unknown vector node type %s for element size\n", node->getDataType().toString()); return 0;
      }
   }

int32_t getVectorElementSizeMask(TR::Node *node)
   {
   switch(node->getDataType())
      {
      case TR::VectorInt8: return 0;
      case TR::VectorInt16: return 1;
      case TR::VectorInt32:
      case TR::VectorFloat: return 2;
      case TR::VectorInt64:
      case TR::VectorDouble: return 3;
      default: TR_ASSERT(false, "Unknown vector node type %s for Element Size Control Mask\n", node->getDataType().toString()); return 0;
      }
   }

int32_t getVectorElementSizeMask(int8_t size)
   {
   switch(size)
      {
      case 8: return 3;
      case 4: return 2;
      case 2: return 1;
      case 1: return 0;
      default: TR_ASSERT(false, "Unknown vector size %i for Element Size Control Mask\n", size); return 0;
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vincEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *vecSrcReg = cg->evaluate(node->getChild(0));
   TR::Register *vecSrc2Reg = cg->allocateRegister(TR_VRF);
   TR::Register *vecTrgReg = cg->allocateRegister(TR_VRF);
   TR::Node *secondChild = node->getChild(1);
   generateReplicateNodeInVectorReg(node, cg, vecSrc2Reg, secondChild, getVectorElementSize(node));
   generateVRRcInstruction(cg, TR::InstOpCode::VA, node, vecTrgReg, vecSrcReg, vecSrc2Reg, getVectorElementSizeMask(node));
   node->setRegister(vecTrgReg);
   cg->stopUsingRegister(vecSrc2Reg);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::vdecEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *vecSrcReg = cg->evaluate(node->getChild(0));
   TR::Register *vecSrc2Reg = cg->allocateRegister(TR_VRF);
   TR::Register *vecTrgReg = cg->allocateRegister(TR_VRF);
   TR::Node *secondChild = node->getChild(1);
   generateReplicateNodeInVectorReg(node, cg, vecSrc2Reg, secondChild, getVectorElementSize(node));
   generateVRRcInstruction(cg, TR::InstOpCode::VS, node, vecTrgReg, vecSrcReg, vecSrc2Reg, getVectorElementSizeMask(node));
   node->setRegister(vecTrgReg);
   cg->stopUsingRegister(vecSrc2Reg);
   cg->decReferenceCount(node->getChild(0));
   cg->decReferenceCount(node->getChild(1));
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::vnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::BAD;
   switch (node->getDataType())
      {
      case TR::VectorInt8:
      case TR::VectorInt16:
      case TR::VectorInt32:
      case TR::VectorInt64:
         opCode = TR::InstOpCode::VLC;
         break;
      case TR::VectorFloat:
      case TR::VectorDouble:
         opCode = TR::InstOpCode::VFPSO;
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString());
         return NULL;
      }

   return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, opCode);
   }

TR::Register *
OMR::Z::TreeEvaluator::vl2vdEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorUnaryOp(node, cg, TR::InstOpCode::VCDG);
   }

TR::Register *
OMR::Z::TreeEvaluator::vcomEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *vecOneReg = cg->allocateRegister(TR_VRF);
   TR::Register *srcVecReg = cg->evaluate(node->getChild(0));
   generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, vecOneReg, 0xFFFF, 1);
   generateVRRcInstruction(cg, TR::InstOpCode::VNC, node, vecOneReg, vecOneReg, srcVecReg, 0);
   node->setRegister(vecOneReg);
   cg->decReferenceCount(node->getChild(0));
   return node->getRegister();
   }

bool canUseNodeForFusedMultiply(TR::Node *node)
   {
   if (node->getOpCode().isMul() && node->getRegister() == NULL && node->getReferenceCount() < 2)
      return true;
   else
      return false;
   }

/**
 * Generate a fused multiply add from the tree (A * B) + C, where addNode is the + node
 * and mulNode the * subtree.
 * If negateOp is provided, generates a fused multiply sub from the tree (A - B*C), where addNode is the + node
 * and mulNode the * subtree.
 * Negates one operand of the multiply and generates a fused multiply add to perform (A - B*C)
 */
bool
generateFusedMultiplyAddIfPossible(TR::CodeGenerator *cg, TR::Node *addNode, TR::InstOpCode::Mnemonic op, TR::InstOpCode::Mnemonic negateOp)
   {
   TR::Compilation *comp = cg->comp();

   TR::Node     *mulNode  = addNode->getFirstChild();
   TR::Node     *addChild = addNode->getSecondChild();

   //  Check for opposite order
   if(!canUseNodeForFusedMultiply(mulNode))
      {
      addChild = addNode->getFirstChild();
      mulNode  = addNode->getSecondChild();
      }

   // if the FloatMAF option is not set and the node is not FP strict, we can't generate FMA operations
   if ((addNode->getType().isFloatingPoint() || TR::VectorFloat == addNode->getType() || TR::VectorDouble == addNode->getType())
       && !comp->getOption(TR_FloatMAF) && !mulNode->isFPStrictCompliant())
      return false;

   if (!performTransformation(comp, "O^O Changing [%p] to fused multiply and add (FMA) operation\n", addNode))
      return false;

   TR_ASSERT(mulNode->getOpCode().isMul(),"Unexpected op!=mul %p\n",mulNode);

   TR::Register *addReg      = (addNode->getType().isFloatingPoint()) ? cg->fprClobberEvaluate(addChild) : cg->gprClobberEvaluate(addChild);
   TR::Register *mulLeftReg  = cg->evaluate(mulNode->getFirstChild());
   TR::Register *mulRightReg = cg->evaluate(mulNode->getSecondChild());

   switch (op)
      {
      case TR::InstOpCode::VFMA:
      case TR::InstOpCode::VFMS:
         generateVRReInstruction(cg, op, addNode, addReg, mulLeftReg, mulRightReg, addReg, getVectorElementSizeMask(addNode), 0);
         break;
      case TR::InstOpCode::MAER:
      case TR::InstOpCode::MAEBR:
      case TR::InstOpCode::MADR:
      case TR::InstOpCode::MADBR:
      case TR::InstOpCode::MSER:
      case TR::InstOpCode::MSEBR:
      case TR::InstOpCode::MSDR:
      case TR::InstOpCode::MSDBR:
         if (negateOp != TR::InstOpCode::BAD)
            {
            TR::Register *tempReg = cg->allocateRegister(TR_FPR);
            generateRRInstruction(cg, negateOp, addNode, tempReg, mulLeftReg); // negate one operand of the multiply
            generateRRDInstruction(cg, op, addNode, addReg, tempReg, mulRightReg);
            cg->stopUsingRegister(tempReg);
            }
         else
            generateRRDInstruction(cg, op, addNode, addReg, mulLeftReg, mulRightReg);
         break;
      default:
         TR_ASSERT(false, "unhandled opcode %s in generateFusedMultiplyAddIfPossible\n", ((TR::InstOpCode *)op)->getMnemonicName());
         break;
      }

   addNode->setRegister(addReg);

   cg->decReferenceCount(mulNode->getFirstChild());
   cg->decReferenceCount(mulNode->getSecondChild());
   cg->decReferenceCount(mulNode); // don't forget this guy!
   cg->decReferenceCount(addChild);

   cg->stopUsingRegister(mulLeftReg);
   cg->stopUsingRegister(mulRightReg);

   return true;
   }

TR::Register *
OMR::Z::TreeEvaluator::vaddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if ((node->getDataType() == TR::VectorDouble || node->getDataType() == TR::VectorFloat) &&
      (canUseNodeForFusedMultiply(node->getFirstChild()) || canUseNodeForFusedMultiply(node->getSecondChild())) &&
      generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::VFMA))
      {
      if (cg->comp()->getOption(TR_TraceCG))
         traceMsg(cg->comp(), "Successfully changed vadd with vmul child to fused multiply and add operation\n");

      return node->getRegister();
      }
   else
      {
      TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::BAD;
      switch (node->getDataType())
         {
         case TR::VectorInt8:
         case TR::VectorInt16:
         case TR::VectorInt32:
         case TR::VectorInt64:
            opCode = TR::InstOpCode::VA;
            break;
         case TR::VectorFloat:
         case TR::VectorDouble:
            opCode = TR::InstOpCode::VFA;
            break;
         default:
            TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString());
            return NULL;
         }
      return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, opCode);
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if ((node->getDataType() == TR::VectorDouble || node->getDataType() == TR::VectorFloat) &&
      canUseNodeForFusedMultiply(node->getFirstChild()) &&
      generateFusedMultiplyAddIfPossible(cg, node, TR::InstOpCode::VFMS))
      {
      if (cg->comp()->getOption(TR_TraceCG))
         traceMsg(cg->comp(), "Successfully changed vsub with vmul child to fused multiply and sub operation\n");

      return node->getRegister();
      }
   else
      {
      TR::InstOpCode::Mnemonic opCode = TR::InstOpCode::BAD;
      switch (node->getDataType())
         {
         case TR::VectorInt8:
         case TR::VectorInt16:
         case TR::VectorInt32:
         case TR::VectorInt64:
            opCode = TR::InstOpCode::VS;
            break;
         case TR::VectorFloat:
         case TR::VectorDouble:
            opCode = TR::InstOpCode::VFS;
            break;
         default:
            TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString());
            return NULL;
         }
      return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, opCode);
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   switch(node->getDataType())
      {
      case TR::VectorInt8:
      case TR::VectorInt16:
      case TR::VectorInt32: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VML);
      case TR::VectorInt64:
         {
         // emulated, no Z instruction available

         // a * b
         // ah = high 32 of a, al = low 32 of a
         // bh = high 32 of b, bl = low 32 of b

         // low 32 = low 32 of (al * bl)
         // high 32 = (high 32 of al*bl) + (ah*bl)<<32 + (al*bh)<<32

         TR::Register *firstChildReg = cg->evaluate(node->getChild(0));
         TR::Register *secondChildReg = cg->evaluate(node->getChild(1));

         TR::Register *multLow = cg->allocateRegister(TR_VRF);
         TR::Register *multHigh = cg->allocateRegister(TR_VRF);
         TR::Register *tempReg = cg->allocateRegister(TR_VRF);
         TR::Register *returnReg = cg->allocateRegister(TR_VRF);

         generateVRRcInstruction(cg, TR::InstOpCode::VML, node, multLow, firstChildReg, secondChildReg, 2);
         generateVRRcInstruction(cg, TR::InstOpCode::VMLH, node, multHigh, firstChildReg, secondChildReg, 2);

         generateVRIbInstruction(cg, TR::InstOpCode::VGM, node, tempReg, 32, 64, 3);
         generateVRRcInstruction(cg, TR::InstOpCode::VN, node, multLow, multLow, tempReg, 0);

         generateVRSaInstruction(cg, TR::InstOpCode::VESL, node, multHigh, multHigh, generateS390MemoryReference(32, cg), 3);
         generateVRRcInstruction(cg, TR::InstOpCode::VA, node, returnReg, multLow, multHigh, 3);

         generateVRSaInstruction(cg, TR::InstOpCode::VESL, node, tempReg, firstChildReg, generateS390MemoryReference(32, cg), 3);
         generateVRRcInstruction(cg, TR::InstOpCode::VML, node, multLow, tempReg, secondChildReg, 2);
         generateVRRcInstruction(cg, TR::InstOpCode::VA, node, returnReg, multLow, returnReg, 3);

         generateVRSaInstruction(cg, TR::InstOpCode::VESL, node, tempReg, secondChildReg, generateS390MemoryReference(32, cg), 3);
         generateVRRcInstruction(cg, TR::InstOpCode::VML, node, multLow, tempReg, firstChildReg, 2);
         generateVRRcInstruction(cg, TR::InstOpCode::VA, node, returnReg, multLow, returnReg, 3);

         node->setRegister(returnReg);

         cg->stopUsingRegister(multHigh);
         cg->stopUsingRegister(multLow);
         cg->stopUsingRegister(tempReg);

         cg->decReferenceCount(node->getChild(0));
         cg->decReferenceCount(node->getChild(1));
         return returnReg;
         }
      case TR::VectorFloat:
      case TR::VectorDouble:
         return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VFM);
      default: TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vDivOrRemHelper(TR::Node *node, TR::CodeGenerator *cg, bool isDivision)
   {
   TR::Node *dividend = node->getChild(0);
   TR::Node *divisor = node->getChild(1);
//   if(dividend->getOpCode().isLoadConst && divisor->getOpCode().isLoadConst())
//      {
//      //todo: pull data from lit pool, constant fold?
//      }
//   else
   TR::Register *resultVRF = cg->allocateRegister(TR_VRF);
   uint8_t mask4 = getVectorElementSizeMask(node);

   // return 1 (div) or 0 (rem)
   if (dividend == divisor)
      {
      generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, resultVRF, isDivision ? 1 : 0, mask4);

      node->setRegister(resultVRF);

      // not evaluating, so recursively decrement reference count
      cg->recursivelyDecReferenceCount(dividend);
      cg->recursivelyDecReferenceCount(divisor);
      }
   else
      {
      TR::Register *dividendGPRHigh = cg->allocateRegister();
      TR::Register *dividendGPRLow = cg->allocateRegister();
      TR::Register *dividendGPR = cg->allocateConsecutiveRegisterPair(dividendGPRLow, dividendGPRHigh);
      TR::Register *divisorGPR = cg->allocateRegister();
      TR::Register *dividendVRF = cg->evaluate(dividend);
      TR::Register *divisorVRF = cg->evaluate(divisor);

      TR::RegisterDependencyConditions * dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg);
      dependencies->addPostCondition(dividendGPR, TR::RealRegister::EvenOddPair);
      dependencies->addPostCondition(dividendGPRHigh, TR::RealRegister::LegalEvenOfPair);
      dependencies->addPostCondition(dividendGPRLow, TR::RealRegister::LegalOddOfPair);
      dependencies->addPostCondition(divisorGPR, TR::RealRegister::AssignAny);

      bool isUnsigned = divisor->isUnsigned();
      bool is64Bit = (node->getDataType() == TR::VectorInt64);
      TR::InstOpCode::Mnemonic divOp = TR::InstOpCode::BAD;
      if (is64Bit)
         divOp = isUnsigned ? TR::InstOpCode::DLGR : TR::InstOpCode::DSGR;
      else
         divOp = isUnsigned ? TR::InstOpCode::DLR : TR::InstOpCode::DR;

      for(int i = 0; i < (16 / getVectorElementSize(node)); i++)
         {
         //Load into GPR from VR element
         generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, is64Bit ? dividendGPRLow : dividendGPRHigh, dividendVRF,
                                 generateS390MemoryReference(i, cg), mask4);
         generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, divisorGPR, divisorVRF,
                                 generateS390MemoryReference(i, cg), mask4);

         // clear out high order
         if (is64Bit && isUnsigned)
            generateRRInstruction(cg, TR::InstOpCode::XGR, node, dividendGPRHigh, dividendGPRHigh);

         // generate shift for non 64 bit values
         if (!is64Bit)
            generateRSInstruction(cg, isUnsigned ? TR::InstOpCode::SRDL : TR::InstOpCode::SRDA, node, dividendGPR, 32);

         generateRRInstruction(cg, divOp, node, dividendGPR, divisorGPR);
         generateS390PseudoInstruction(cg, InstOpCode::DEPEND, node, dependencies);

         //Store result GPR into VRF
         generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, resultVRF, isDivision ? dividendGPRLow : dividendGPRHigh,
                                 generateS390MemoryReference(i, cg), mask4);
         }

      node->setRegister(resultVRF);

      cg->stopUsingRegister(dividendGPR);
      cg->stopUsingRegister(divisorGPR);
      cg->stopUsingRegister(dividendVRF);
      cg->stopUsingRegister(divisorVRF);

      cg->decReferenceCount(dividend);
      cg->decReferenceCount(divisor);
      }

   return resultVRF;
   }

TR::Register *
OMR::Z::TreeEvaluator::vdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   switch(node->getDataType())
      {
      case TR::VectorInt8:
      case TR::VectorInt16:
      case TR::VectorInt32:
      case TR::VectorInt64: return TR::TreeEvaluator::vDivOrRemHelper(node, cg, true);
      case TR::VectorFloat:
      case TR::VectorDouble: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VFD);
      default: TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::vDivOrRemHelper(node, cg, false);
   }

TR::Register *
OMR::Z::TreeEvaluator::vandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VN);
   }

TR::Register *
OMR::Z::TreeEvaluator::vorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VO);
   }

TR::Register *
OMR::Z::TreeEvaluator::vxorEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VX);
   }

/**
 * Vector Shift
 * children: source vector and 1) shift vector or 2) shift scalar
 */
TR::Register *
OMR::Z::TreeEvaluator::vectorElementShiftHelper(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic opcode;
   TR::Node *firstOperand = node->getFirstChild();
   TR::Node *secondOperand = node->getSecondChild();

   TR::Register *vecSrcReg = cg->evaluate(firstOperand);
   TR::Register *returnReg = cg->allocateRegister(TR_VRF);

   if (secondOperand->getOpCode().isLoadConst() && !secondOperand->getOpCode().isVector())
      {
      switch (node->getOpCodeValue())
         {
         case TR::vshl: opcode = TR::InstOpCode::VESL; break;
         case TR::vushr: opcode = TR::InstOpCode::VESRL; break;
         case TR::vshr: opcode = TR::InstOpCode::VESRA; break;
         default: TR_ASSERT(false, "Unknown TR vector shift IL\n"); return NULL;
         }

      int64_t constVal = secondOperand->getIntegerNodeValue<int64_t>();
      generateVRSaInstruction(cg, opcode, node, returnReg, vecSrcReg, generateS390MemoryReference(constVal, cg), getVectorElementSizeMask(firstOperand));
      }
   else
      {
      switch (node->getOpCodeValue())
         {
         case TR::vshl: opcode = TR::InstOpCode::VESLV; break;
         case TR::vushr: opcode = TR::InstOpCode::VESRLV; break;
         case TR::vshr: opcode = TR::InstOpCode::VESRAV; break;
         default: TR_ASSERT(false, "Unknown TR vector shift IL\n"); return NULL;
         }

      uint8_t mask = getVectorElementSizeMask(firstOperand);

      TR::Register *shiftReg = cg->evaluate(secondOperand);
      if (!secondOperand->getOpCode().isVector())
         {
         TR::Register *tempReg = cg->allocateRegister(TR_VRF);

      // On 31-bit an 8-byte sized child may come in a register pair so we have to handle this case specially
      if (shiftReg->getRegisterPair() != NULL)
         {
         if (mask == 3)
            {
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, tempReg, shiftReg->getHighOrder(), generateS390MemoryReference(0, cg), 2);
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, tempReg, shiftReg->getLowOrder(), generateS390MemoryReference(1, cg), 2);
            }
         else
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, tempReg, shiftReg->getLowOrder(), generateS390MemoryReference(0, cg), mask);
         }
      else
         generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, tempReg, shiftReg, generateS390MemoryReference(0, cg), mask);

         generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, tempReg, tempReg, 0, mask);
         cg->stopUsingRegister(shiftReg);
         shiftReg = tempReg;
         }

      generateVRRcInstruction(cg, opcode, node, returnReg, vecSrcReg, shiftReg, mask);

      cg->stopUsingRegister(shiftReg);
      }

   node->setRegister(returnReg);

   cg->decReferenceCount(firstOperand);
   cg->decReferenceCount(secondOperand);

   return returnReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::vcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   switch(node->getFirstChild()->getDataType())
      {
      case TR::VectorInt8:
      case TR::VectorInt16:
      case TR::VectorInt32:
      case TR::VectorInt64: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VCEQ);
      case TR::VectorDouble: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VFCE);
      default: TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetReg;
   switch(node->getFirstChild()->getDataType())
      {
      case TR::VectorInt8:
      case TR::VectorInt16:
      case TR::VectorInt32:
      case TR::VectorInt64: targetReg = TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VCEQ); break;
      case TR::VectorDouble: targetReg = TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VFCE); break;
      default: TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); break;
      }

   // vector nor with zero vector
   TR::Register *vecZeroReg = cg->allocateRegister(TR_VRF);
   generateZeroVector(node, cg, vecZeroReg);
   generateVRRcInstruction(cg, TR::InstOpCode::VNO, node, targetReg, targetReg, vecZeroReg, 0, 0, 0);
   cg->stopUsingRegister(vecZeroReg);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::vcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   node->swapChildren();
   return TR::TreeEvaluator::vcmpgtEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::vcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic op = node->getOpCode().isUnsignedCompare() ? TR::InstOpCode::VCHL : TR::InstOpCode::VCH;
   switch(node->getFirstChild()->getDataType())
      {
      case TR::VectorInt8:
      case TR::VectorInt16:
      case TR::VectorInt32:
      case TR::VectorInt64: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, op);
      case TR::VectorDouble: return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VFCH);
      default: TR_ASSERT(false, "unrecognized vector type %s\n", node->getDataType().toString()); return NULL;
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   node->swapChildren();
   return TR::TreeEvaluator::vcmpgeEvaluator(node, cg);
   }

TR::Register *
OMR::Z::TreeEvaluator::vcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::InstOpCode::Mnemonic op = node->getOpCode().isUnsignedCompare() ? TR::InstOpCode::VCHL : TR::InstOpCode::VCH;

   if (node->getFirstChild()->getDataType() == TR::VectorDouble)
      return TR::TreeEvaluator::inlineVectorBinaryOp(node, cg, TR::InstOpCode::VFCHE);
   else
      {
      TR::Node *firstChild = node->getFirstChild();
      TR::Node *secondChild = node->getSecondChild();

      TR::Register *firstReg = cg->evaluate(firstChild);
      TR::Register *secondReg = cg->evaluate(secondChild);
      TR::Register *targetReg = cg->allocateRegister(TR_VRF);
      TR::Register *equalReg = cg->allocateRegister(TR_VRF);

      int32_t mask4 = getVectorElementSizeMask(node);
      // vector int types need compare equal part
      generateVRRcInstruction(cg, TR::InstOpCode::VCEQ, node, equalReg, firstReg, secondReg, 0, 0, mask4);
      generateVRRbInstruction(cg, op, node, targetReg, firstReg, secondReg, 0, mask4);
      generateVRRcInstruction(cg, TR::InstOpCode::VO, node, targetReg, targetReg, equalReg, 0, 0, 0);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);

      cg->stopUsingRegister(equalReg);
      cg->stopUsingRegister(firstReg);
      cg->stopUsingRegister(secondReg);

      node->setRegister(targetReg);
      return targetReg;
      }
   }

TR::Register *
OMR::Z::TreeEvaluator::vrandEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *srcVecReg = cg->evaluate(node->getChild(0));
   TR::Register *workReg = cg->allocateRegister(TR_VRF);
   TR::Register *workReg2 = cg->allocateRegister(TR_VRF);
   TR::Register *shiftReg = cg->allocateRegister(TR_VRF);

   int mask4 = getVectorElementSizeMask(node->getChild(0));   //0 - byte, 3 - doubleword

   // copy source to work reg
   generateVRRaInstruction(cg, TR::InstOpCode::VLR, node, workReg, srcVecReg);

   // Perform vector reduction AND
   // Strategy used here is shift by incremental powers of 2 and AND. Next iteration works on previous shift-and-AND result
   switch(mask4)
      {
      case 0:   // byte
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, shiftReg, 1 << 4, 7);                      // load shiftReg
         generateVRRcInstruction(cg, TR::InstOpCode::VSRLB, node, workReg2, workReg, shiftReg, 0, 0, 0);     // shift right logical by byte
         generateVRRcInstruction(cg, TR::InstOpCode::VN, node, workReg, workReg, workReg2, 0, 0, 0);         // AND
      case 1:   // halfword
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, shiftReg, 1 << 5, 7);                      // load shiftReg
         generateVRRcInstruction(cg, TR::InstOpCode::VSRLB, node, workReg2, workReg, shiftReg, 0, 0, 0);     // shift right logical by byte
         generateVRRcInstruction(cg, TR::InstOpCode::VN, node, workReg, workReg, workReg2, 0, 0, 0);         // AND
      case 2:   // word
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, shiftReg, 1 << 6, 7);                      // load shiftReg
         generateVRRcInstruction(cg, TR::InstOpCode::VSRLB, node, workReg2, workReg, shiftReg, 0, 0, 0);     // shift right logical by byte
         generateVRRcInstruction(cg, TR::InstOpCode::VN, node, workReg, workReg, workReg2, 0, 0, 0);         // AND
      case 3:   // doubleword
         generateVRIaInstruction(cg, TR::InstOpCode::VLEIB, node, shiftReg, 1 << 7, 7);                      // load shiftReg
         generateVRRcInstruction(cg, TR::InstOpCode::VSRLB, node, workReg2, workReg, shiftReg, 0, 0, 0);     // shift right logical by byte
         generateVRRcInstruction(cg, TR::InstOpCode::VN, node, workReg, workReg, workReg2, 0, 0, 0);         // AND
         break;
      default: TR_ASSERT(false, "wrong mask value found: %i\n", mask4); break;
      }

   // final result is in the last element. store it into resultReg. result will be in GPR
   // last element index. mask4=0,1,2,3 -> last element index=15,7,3,1
   TR::MemoryReference *extractIndexMR = generateS390MemoryReference((1 << (4 - mask4)) - 1, cg);

   TR::Register *resultReg = (cg->comp()->target().is32Bit() && mask4 == 3) ? cg->allocateRegister() : cg->allocateRegister();
   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, resultReg, workReg, extractIndexMR, mask4);

   cg->stopUsingRegister(workReg);
   cg->stopUsingRegister(workReg2);
   cg->stopUsingRegister(shiftReg);
   node->setRegister(resultReg);
   cg->decReferenceCount(node->getChild(0));
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::vreturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Linkage * linkage = cg->getLinkage();
   TR::Register * returnAddressReg = cg->allocateRegister();
   TR::Register * returnValRegister = NULL;

   returnValRegister = cg->evaluate(node->getFirstChild());

   TR::RegisterDependencyConditions * dependencies = NULL ;
   dependencies= new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 1, cg);
   dependencies->addPostCondition(returnValRegister, linkage->getVectorReturnRegister());
   TR::Instruction * inst = generateS390PseudoInstruction(cg, TR::InstOpCode::RET, node, dependencies);
   cg->stopUsingRegister(returnAddressReg);
   cg->decReferenceCount(node->getFirstChild());
   return NULL ;
   }

TR::Register *
OMR::Z::TreeEvaluator::vconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *litReg = cg->allocateRegister();
   generateLoadLiteralPoolAddress(cg, node, litReg);
   size_t offset = node->getLiteralPoolOffset();
   TR::Compilation *comp = cg->comp();
   TR::Register *vecReg = cg->allocateRegister(TR_VRF);
   generateVRXInstruction(cg, TR::InstOpCode::VL, node, vecReg, generateS390MemoryReference(litReg, offset, cg), 0);
   cg->stopUsingRegister(litReg);
   node->setRegister(vecReg);
   return node->getRegister();
   }

TR::Register *
OMR::Z::TreeEvaluator::vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *returnReg = cg->allocateRegister(TR_VRF);
   uint8_t ESMask = getVectorElementSizeMask(firstChild->getSize());
   bool inRegister = !firstChild->isSingleRefUnevaluated();

   if (firstChild->getOpCode().isLoadConst() &&
       firstChild->getOpCode().isInteger() &&
       MIN_IMMEDIATE_VAL <= firstChild->getLongInt() && firstChild->getLongInt() <= MAX_IMMEDIATE_VAL)
      {
      generateVRIaInstruction(cg, TR::InstOpCode::VREPI, node, returnReg, firstChild->getLongInt(), ESMask);
      cg->recursivelyDecReferenceCount(firstChild);
      }
   else if (!inRegister &&
            firstChild->getOpCode().isMemoryReference() &&
            firstChild->getReferenceCount() < 2)  // Just load from mem and skip evaluation if low refcount
      {
      auto mr = generateS390MemoryReference(firstChild, cg);
      generateVRXInstruction(cg, TR::InstOpCode::VLREP, node, returnReg, mr, ESMask);
      cg->decReferenceCount(firstChild);
      }
   else if (!inRegister &&
            firstChild->getOpCode().isLoadConst() &&
            firstChild->getReferenceCount() < 2)  // Just load from mem and skip evaluation if low refcount
      {
      TR::MemoryReference* mr = NULL;
      if (firstChild->getOpCode().isDouble())
         {
         double value = firstChild->getDouble();
         mr = generateS390MemoryReference(cg->findOrCreateConstant(node, &value, 8), cg, 0, node);
         }
      else if (firstChild->getOpCode().isFloat())
         {
         float value = firstChild->getFloat();
         mr = generateS390MemoryReference(cg->findOrCreateConstant(node, &value, 4), cg, 0, node);
         }
      else
         {
         switch (firstChild->getSize())
            {
            case 1:
               {
               uint8_t value = firstChild->getIntegerNodeValue<uint8_t>();
               mr = generateS390MemoryReference(cg->findOrCreateConstant(node, &value, 1), cg, 0, node);
               break;
               }
            case 2:
               {
               uint16_t value = firstChild->getIntegerNodeValue<uint16_t>();
               mr = generateS390MemoryReference(cg->findOrCreateConstant(node, &value, 2), cg, 0, node);
               break;
               }
            case 4:
               {
               uint32_t value = firstChild->getIntegerNodeValue<uint32_t>();
               mr = generateS390MemoryReference(cg->findOrCreateConstant(node, &value, 4), cg, 0, node);
               break;
               }
            case 8:
               {
               uint64_t value = firstChild->getIntegerNodeValue<uint64_t>();
               mr = generateS390MemoryReference(cg->findOrCreateConstant(node, &value, 8), cg, 0, node);
               break;
               }
            default: TR_ASSERT(false, "unhandled integral splat child size\n"); break;
            }
         }
      generateVRXInstruction(cg, TR::InstOpCode::VLREP, node, returnReg, mr, ESMask);
      cg->recursivelyDecReferenceCount(firstChild);
      }
   else if (firstChild->getOpCode().isDouble() || firstChild->getOpCode().isFloat())
      {
      // Use the same VRF as the one FPR overlaps, to avoid general 'else' path of moving to GPR
      auto firstReg = cg->evaluate(firstChild);
      generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, returnReg, firstReg, 0, ESMask);
      cg->decReferenceCount(firstChild);
      cg->stopUsingRegister(firstReg);
      }
   else
      {
      auto firstReg = cg->evaluate(firstChild);

      if (firstChild->getSize() == 8)
         {
         // On 31-bit an 8-byte sized child may come in a register pair so we have to handle this case specially
         if (firstReg->getRegisterPair())
            {
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, returnReg, firstReg->getHighOrder(), generateS390MemoryReference(0, cg), 2);
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, returnReg, firstReg->getLowOrder(), generateS390MemoryReference(1, cg), 2);
            generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, returnReg, returnReg, 0, ESMask);
            }
         else
            {
            generateVRRfInstruction(cg, TR::InstOpCode::VLVGP, node, returnReg, firstReg, firstReg);
            }
         }
      else
         {
         if (firstReg->getRegisterPair())
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, returnReg, firstReg->getLowOrder(), generateS390MemoryReference(NULL, 0, cg), ESMask);
         else
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, returnReg, firstReg, generateS390MemoryReference(NULL, 0, cg), ESMask);

         generateVRIcInstruction(cg, TR::InstOpCode::VREP, node, returnReg, returnReg, 0, ESMask);
         }

      cg->decReferenceCount(firstChild);
      cg->stopUsingRegister(firstReg);
      }

   node->setRegister(returnReg);
   return returnReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::getvelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *vectorChild = node->getFirstChild();
   TR::Node *elementChild = node->getSecondChild();

   TR::Register *vectorReg = cg->evaluate(vectorChild);
   TR::Register *returnReg = cg->allocateRegister();

   /*
    * Using pair regs to return 64 bit data in 32 bit registers is a special case.
    * To read 64 bit element 0, it will read 32 bit elements 0 and 1 instead.
    * To read 64 bit element 1, it will read 32 bit elements 2 and 3 instead.
    * The index provided to VLGV is adjusted accordingly
    */
   TR::MemoryReference *memRef = NULL;
   if (elementChild->getOpCode().isLoadConst())
      {
      memRef = generateS390MemoryReference(elementChild->get64bitIntegralValue(), cg);
      }
   else
      {
      TR::Register *elementReg = cg->evaluate(elementChild);
      memRef = generateS390MemoryReference(elementReg, 0 , cg);
      cg->stopUsingRegister(elementReg);
      }

   generateVRScInstruction(cg, TR::InstOpCode::VLGV, node, returnReg, vectorReg, memRef, getVectorElementSizeMask(vectorChild));

   TR::DataType dt = vectorChild->getDataType();
   bool isUnsigned = (!node->getType().isInt64() && node->isUnsigned());
   if (dt == TR::VectorDouble || dt == TR::VectorFloat)
      {
      TR::Register *tempReg = returnReg;
      returnReg = cg->allocateRegister(TR_FPR);
      if (dt == TR::VectorFloat)
         {
         generateRSInstruction(cg, TR::InstOpCode::SLLG, node, tempReg, 32); //floats are stored in the high half of the floating point register
         }
      generateRRInstruction(cg, TR::InstOpCode::LDGR, node, returnReg, tempReg);
      cg->stopUsingRegister(tempReg);
      }
   else if (dt == TR::VectorInt16 && !isUnsigned)
      {
      generateRRInstruction(cg, (cg->comp()->target().is64Bit()) ? TR::InstOpCode::LGHR : TR::InstOpCode::LHR, node, returnReg, returnReg);
      }
   else if (dt == TR::VectorInt8 && !isUnsigned)
      {
      generateRRInstruction(cg, (cg->comp()->target().is64Bit()) ? TR::InstOpCode::LGBR : TR::InstOpCode::LBR, node, returnReg, returnReg);
      }

   cg->stopUsingRegister(vectorReg);

   node->setRegister(returnReg);

   cg->decReferenceCount(vectorChild);
   cg->decReferenceCount(elementChild);

   return returnReg;
   }

TR::Register *
OMR::Z::TreeEvaluator::vsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *vectorNode = node->getFirstChild();
   TR::Node *elementNode = node->getSecondChild();
   TR::Node *valueNode = node->getThirdChild();

   TR::Register *vectorReg = cg->gprClobberEvaluate(vectorNode);

   // if elementNode is constant then we can use the Vector Load Element version of the instructions
   // but only if valueNode is a constant or a memory reference
   // otherwise we have to use the Vector Load GR From VR Element version
   bool inRegister = !valueNode->isSingleRefUnevaluated();

   int32_t size = valueNode->getSize();

   if (elementNode->getOpCode().isLoadConst() && !inRegister &&
      (valueNode->getOpCode().isMemoryReference() || valueNode->getOpCode().isLoadConst()))
      {
      uint8_t m3 = elementNode->getLongInt() % (16/size);
      TR::InstOpCode::Mnemonic op = TR::InstOpCode::BAD;

      if (valueNode->getOpCode().isLoadConst() && valueNode->getOpCode().isInteger() &&
          valueNode->getLongInt() >= MIN_IMMEDIATE_VAL && valueNode->getLongInt() <= MAX_IMMEDIATE_VAL)
         {
         switch (size)
            {
            case 8: op = TR::InstOpCode::VLEIG; break;
            case 4: op = TR::InstOpCode::VLEIF; break;
            case 2: op = TR::InstOpCode::VLEIH; break;
            case 1: op = TR::InstOpCode::VLEIB; break;
            }

         generateVRIaInstruction(cg, op, node, vectorReg, valueNode->getLongInt(), m3);
         }
      else
         {
         TR::MemoryReference *memRef = NULL;

         switch (size)
            {
            case 8: op = TR::InstOpCode::VLEG; break;
            case 4: op = TR::InstOpCode::VLEF; break;
            case 2: op = TR::InstOpCode::VLEH; break;
            case 1: op = TR::InstOpCode::VLEB; break;
            }

         if (valueNode->getOpCode().isLoadConst())
            {
            TR::Register *litReg = getLitPoolBaseReg(valueNode, cg);
            size_t offset = 0;

            if (size == 1)
               {
               uint8_t value = valueNode->getIntegerNodeValue<uint8_t>();
               offset = cg->findOrCreateLiteral(&value, size);
               }
            else if (size == 2)
               {
               uint16_t value = valueNode->getIntegerNodeValue<uint16_t>();
               offset = cg->findOrCreateLiteral(&value, size);
               }
            else if (size == 4)
               {
               if (valueNode->getOpCode().isFloat())
                  {
                  float value = valueNode->getFloat();
                  offset = cg->findOrCreateLiteral(&value, size);
                  }
               else
                  {
                  uint32_t value = valueNode->getIntegerNodeValue<uint32_t>();
                  offset = cg->findOrCreateLiteral(&value, size);
                  }
               }
            else if (size == 8)
               {
               if (valueNode->getOpCode().isDouble())
                  {
                  double value = valueNode->getDouble();
                  offset = cg->findOrCreateLiteral(&value, size);
                  }
               else
                  {
                  uint64_t value = valueNode->getIntegerNodeValue<uint64_t>();
                  offset = cg->findOrCreateLiteral(&value, size);
                  }
               }

            memRef = new (cg->trHeapMemory()) TR::MemoryReference(litReg, offset, cg);
            cg->stopUsingRegister(litReg);
            }
         else
            memRef = generateS390MemoryReference(valueNode, cg);

         generateVRXInstruction(cg, op, node, vectorReg, memRef, m3);
         }
      }
   else
      {
      TR::Register *valueReg = cg->evaluate(valueNode);

      bool usePairReg = valueReg->getRegisterPair() != NULL;

      TR::MemoryReference *memRef = NULL;
      if (elementNode->getOpCode().isLoadConst())
         {
         if (usePairReg)
            {
            memRef = generateS390MemoryReference(elementNode->get64bitIntegralValue() * 2, cg);
            }
         else
            {
            memRef = generateS390MemoryReference(elementNode->get64bitIntegralValue(), cg);
            }
         }
      else
         {
         TR::Register *elementReg = cg->evaluate(elementNode);
         if (usePairReg)
            {
            generateRSInstruction(cg, TR::InstOpCode::SLL, node, elementReg, 1); //shift left by 1 to perform multiplication by 2
            }
         memRef = generateS390MemoryReference(elementReg, 0 , cg);
         cg->stopUsingRegister(elementReg);
         }

      if (valueNode->getDataType() == TR::Double || valueNode->getDataType() == TR::Float)
         {
         TR::Register *fpReg = valueReg;
         valueReg = cg->allocateRegister();
         generateRRInstruction(cg, TR::InstOpCode::LGDR, node, valueReg, fpReg);
         if (valueNode->getDataType() == TR::Float)
            {
            generateRSInstruction(cg, TR::InstOpCode::SRLG, node, valueReg, 32); //floats are stored in the high half of the floating point register whild VLVG reads for the low half of the register
            }
         cg->stopUsingRegister(fpReg);
         }

      // On 31-bit an 8-byte sized child may come in a register pair so we have to handle this case specially
      if (usePairReg)
         {
         if (getVectorElementSizeMask(size) == 3)
            {
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, vectorReg, valueReg->getHighOrder(), memRef, 2);
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, vectorReg, valueReg->getLowOrder(),  generateS390MemoryReference(*memRef, 1, cg), 2);
            }
         else
            generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, vectorReg, valueReg->getLowOrder(),  memRef, getVectorElementSizeMask(size));
         }
      else
         generateVRSbInstruction(cg, TR::InstOpCode::VLVG, node, vectorReg, valueReg, memRef, getVectorElementSizeMask(size));

      cg->stopUsingRegister(valueReg);
      }

   node->setRegister(vectorReg);
   cg->decReferenceCount(vectorNode);
   cg->decReferenceCount(elementNode);
   cg->decReferenceCount(valueNode);

   return vectorReg;
   }

TR::Instruction *
OMR::Z::TreeEvaluator::genLoadForObjectHeaders      (TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor)
   {
   return iCursor;
   }

TR::Instruction *
OMR::Z::TreeEvaluator::genLoadForObjectHeadersMasked(TR::CodeGenerator *cg, TR::Node *node, TR::Register *reg, TR::MemoryReference *tempMR, TR::Instruction *iCursor)
   {
   return iCursor;
   }

TR::Register*
OMR::Z::TreeEvaluator::intrinsicAtomicAdd(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR_ASSERT_FATAL(cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196), "Atomic add intrinsics are only supported z196+");

   TR::Node* addressNode = node->getChild(0);
   TR::Node* valueNode = node->getChild(1);

   TR::Register* addressReg = cg->evaluate(addressNode);
   TR::Register* valueReg = cg->evaluate(valueNode);

   // Used to hold the return value of LAA instruction but will be discarded
   TR::Register* tempReg = cg->allocateRegister();

   TR::MemoryReference* addressMemRef = generateS390MemoryReference(addressReg, 0, cg);

   auto mnemonic =
      valueNode->getDataType().isInt32() ?
         TR::InstOpCode::LAA :
         TR::InstOpCode::LAAG;

   generateRSInstruction(cg, mnemonic, node, tempReg, valueReg, addressMemRef);

   node->setRegister(valueReg);
   cg->stopUsingRegister(tempReg);
   cg->decReferenceCount(addressNode);
   cg->decReferenceCount(valueNode);

   return valueReg;
   }

TR::Register*
OMR::Z::TreeEvaluator::intrinsicAtomicFetchAndAdd(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR_ASSERT_FATAL(cg->comp()->target().cpu.getSupportsArch(TR::CPU::z196), "Atomic add intrinsics are only supported z196+");

   TR::Node* addressNode = node->getChild(0);
   TR::Node* valueNode = node->getChild(1);

   TR::Register* addressReg = cg->evaluate(addressNode);
   TR::Register* valueReg = cg->gprClobberEvaluate(valueNode);

   TR::MemoryReference* addressMemRef = generateS390MemoryReference(addressReg, 0, cg);

   auto mnemonic =
      valueNode->getDataType().isInt32() ?
         TR::InstOpCode::LAA :
         TR::InstOpCode::LAAG;

   generateRSInstruction(cg, mnemonic, node, valueReg, valueReg, addressMemRef);

   node->setRegister(valueReg);
   cg->decReferenceCount(addressNode);
   cg->decReferenceCount(valueNode);

   return valueReg;
   }

TR::Register*
OMR::Z::TreeEvaluator::intrinsicAtomicSwap(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* addressNode = node->getChild(0);
   TR::Node* valueNode = node->getChild(1);

   TR::Register* addressReg = cg->evaluate(addressNode);
   TR::Register* valueReg = cg->evaluate(valueNode);
   TR::Register* returnReg = cg->allocateRegister();

   TR::RegisterDependencyConditions* dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 3, cg);

   dependencies->addPostCondition(addressReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(valueReg, TR::RealRegister::AssignAny);
   dependencies->addPostCondition(returnReg, TR::RealRegister::AssignAny);

   TR::LabelSymbol* loopLabel = generateLabelSymbol(cg);
   TR::LabelSymbol* cFlowRegionEnd = generateLabelSymbol(cg);

   TR::MemoryReference* addressMemRef = generateS390MemoryReference(addressReg, 0, cg);

   auto mnemonic =
      valueNode->getDataType().isInt32() ?
         TR::InstOpCode::L :
         TR::InstOpCode::LG;

   // Load the original value
   generateRXInstruction(cg, mnemonic, node, returnReg, addressMemRef);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, loopLabel);
   loopLabel->setStartInternalControlFlow();

   addressMemRef = generateS390MemoryReference(*addressMemRef, 0, cg);

   mnemonic =
      valueNode->getDataType().isInt32() ?
         TR::InstOpCode::CS :
         TR::InstOpCode::CSG;

   // Compare and swap against the original value
   generateRSInstruction(cg, mnemonic, node, returnReg, valueReg, addressMemRef);

   // Branch if the compare and swap failed and try again
   generateS390BranchInstruction(cg, TR::InstOpCode::BRC,TR::InstOpCode::COND_CC1, node, loopLabel);

   generateS390LabelInstruction(cg, TR::InstOpCode::LABEL, node, cFlowRegionEnd, dependencies);
   cFlowRegionEnd->setEndInternalControlFlow();

   node->setRegister(returnReg);
   cg->decReferenceCount(addressNode);
   cg->decReferenceCount(valueNode);

   return returnReg;
   }

///// SIMD Evaluators End /////////////


#ifdef _MSC_VER
// Because these templates are not defined in headers, but the cpp TreeEvaluator.cpp, suppress implicit instantiation
// using these extern definitions. Unfortunately, Visual Studio likes doing things differently, so for VS, this chunk
// is copied from TreeEvaluator.hpp here.
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<true,   8>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<false,  8>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<true,  16>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<false, 16>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<true,  32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::narrowCastEvaluator<false, 32>(TR::Node * node, TR::CodeGenerator * cg);

extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,   8, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,   8, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false,  8, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false,  8, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  16, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  16, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 16, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 16, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  32, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  32, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 32, 32>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 32, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<true,  64, 64>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::extendCastEvaluator<false, 64, 64>(TR::Node * node, TR::CodeGenerator * cg);

extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator< 8, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator< 8, false>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<16, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<16, false>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<32, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<32, false>(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<64, true >(TR::Node * node, TR::CodeGenerator * cg);
extern template TR::Register * TR::TreeEvaluator::addressCastEvaluator<64, false>(TR::Node * node, TR::CodeGenerator * cg);
#endif
