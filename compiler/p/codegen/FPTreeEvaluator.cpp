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
#include "codegen/CodeGeneratorUtils.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
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
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/Runtime.hpp"

static void ifFloatEvaluator( TR::Node *node, TR::InstOpCode::Mnemonic branchOp1, TR::InstOpCode::Mnemonic branchOp2, TR::CodeGenerator *cg);
static TR::Register *compareFloatAndSetOrderedBoolean(TR::InstOpCode::Mnemonic branchOp1, TR::InstOpCode::Mnemonic branchOp2, int64_t imm, TR::Node *node, TR::CodeGenerator *cg);
static TR::Register *singlePrecisionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic OpCode, TR::CodeGenerator *cg);
static TR::Register *doublePrecisionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic OpCode, TR::CodeGenerator *cg);

TR::Register *OMR::Power::TreeEvaluator::ibits2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child = node->getFirstChild();
   TR::Register            *target = cg->allocateSinglePrecisionRegister();

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
#ifdef J9_PROJECT_SPECIFIC
      if (node->getFirstChild()->getOpCodeValue() == TR::iriload)
         {
         tempMR->forceIndexedForm(child, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwbrx, node, target, tempMR);
         }
      else
#endif
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfs, node, target, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      generateMvFprGprInstructions(cg, node, gprSp2fpr, cg->comp()->target().is64Bit(),target, cg->evaluate(child));
      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }

TR::Register *OMR::Power::TreeEvaluator::fbits2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = cg->allocateRegister();
   TR::Register            *floatReg;
   bool                   childEval = true;


   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      childEval = false;
      floatReg = cg->allocateSinglePrecisionRegister();
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfs, node, floatReg, new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 0, 4, cg));
      generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, target, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      floatReg = cg->comp()->target().cpu.id() >= TR_PPCp8 ? cg->gprClobberEvaluate(child) : cg->evaluate(child);
      generateMvFprGprInstructions(cg, node, fpr2gprSp, cg->comp()->target().is64Bit(),target, floatReg);
      childEval = floatReg == child->getRegister();
      cg->decReferenceCount(child);
      }

   // There's a special-case check for NaN values, which have to be
   // normalized to a particular NaN value.
   //
   if (node->normalizeNanValues())
      {
      TR::Register *condReg = cg->allocateRegister(TR_CCR);

      TR::LabelSymbol *nanNormalizeStartLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *nanNormalizeEndLabel = generateLabelSymbol(cg);

      nanNormalizeStartLabel->setStartInternalControlFlow();
      nanNormalizeEndLabel->setEndInternalControlFlow();

      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());
      deps->addPostCondition(condReg, TR::RealRegister::NoReg);
      deps->addPostCondition(target, TR::RealRegister::NoReg);

      generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, floatReg, floatReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, nanNormalizeStartLabel);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchLikely, node, nanNormalizeEndLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, target, 0x7fc0);
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, nanNormalizeEndLabel, deps);

      cg->stopUsingRegister(condReg);
      }

   if (!childEval)
      cg->stopUsingRegister(floatReg);

   node->setRegister(target);
   return target;
   }

TR::Register *OMR::Power::TreeEvaluator::lbits2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child  = node->getFirstChild();
   TR::Register            *target = cg->allocateRegister(TR_FPR);

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 8, cg);
#ifdef J9_PROJECT_SPECIFIC
      if (child->getOpCodeValue() == TR::irlload && cg->comp()->target().is64Bit())        // 64-bit only
         {
         TR::Register     *tmpReg = cg->allocateRegister();
         tempMR->forceIndexedForm(child, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::ldbrx, node, tmpReg, tempMR);
         generateMvFprGprInstructions(cg, node, gpr2fprHost64, true, target, tmpReg);
         cg->stopUsingRegister(tmpReg);
         }
      else if (child->getOpCodeValue() == TR::irlload && cg->comp()->target().is32Bit())   // 32-bit
         {
         TR::Register     *highReg = cg->allocateRegister();
         TR::Register     *lowReg = cg->allocateRegister();
         TR::MemoryReference *tempMRLoad1 = new (cg->trHeapMemory()) TR::MemoryReference(child, *tempMR, 0, 4, cg);
         TR::MemoryReference *tempMRLoad2 = new (cg->trHeapMemory()) TR::MemoryReference(child, *tempMR, 4, 4, cg);

         tempMRLoad1->forceIndexedForm(child, cg);
         tempMRLoad2->forceIndexedForm(child, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwbrx, node, lowReg, tempMRLoad1);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwbrx, node, highReg, tempMRLoad2);
         generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, target, highReg, lowReg);

         tempMRLoad1->decNodeReferenceCounts(cg);
         tempMRLoad2->decNodeReferenceCounts(cg);
         cg->stopUsingRegister(highReg);
         cg->stopUsingRegister(lowReg);
         cg->decReferenceCount(child);
         }
      else
#endif
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, target, tempMR);
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      if (cg->comp()->target().is64Bit())
         generateMvFprGprInstructions(cg, node, gpr2fprHost64, true, target, cg->evaluate(child));
      else
         {
         TR::Register *longReg = cg->evaluate(child);
         if (cg->comp()->target().cpu.id() >= TR_PPCp8)
            {
            TR::Register * tmp1 = cg->allocateRegister(TR_FPR);
            generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, target, longReg->getHighOrder(), longReg->getLowOrder(), tmp1);
            cg->stopUsingRegister(tmp1);
            }
         else
            generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, target, longReg->getHighOrder(), longReg->getLowOrder());
         }
      cg->decReferenceCount(child);
      }

   node->setRegister(target);
   return target;
   }

TR::Register *OMR::Power::TreeEvaluator::dbits2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node                *child   = node->getFirstChild();
   TR::Register            *lowReg  = NULL;
   TR::Register            *highReg = NULL;
   TR::Register            *lReg = NULL;
   TR::Register            *doubleReg;
   bool                    childEval = true;

   if (child->getRegister() == NULL && child->getReferenceCount() == 1 &&
       child->getOpCode().isLoadVar())
      {
      childEval = false;
      TR::MemoryReference  *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 8, cg);
      doubleReg = cg->allocateRegister(TR_FPR);
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, doubleReg, tempMR);

      if (cg->comp()->target().is64Bit())
         {
         lReg  = cg->allocateRegister();
         TR::MemoryReference  *tempMR2 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 0, 8, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, lReg, tempMR2);
         tempMR2->decNodeReferenceCounts(cg);
         }
      else
         {
         lowReg  = cg->allocateRegister();
         highReg = cg->allocateRegister();

         TR::MemoryReference *highMem, *lowMem;
         highMem = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 0, 4, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, highReg, highMem);
         lowMem = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 4, 4, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, lowReg, lowMem);

         highMem->decNodeReferenceCounts(cg);
         lowMem->decNodeReferenceCounts(cg);
         }
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      doubleReg = cg->evaluate(child);

      if (cg->comp()->target().is64Bit())
         {
         lReg  = cg->allocateRegister();
         generateMvFprGprInstructions(cg, node, fpr2gprHost64, true, lReg, doubleReg);
         }
      else
         {
         highReg = cg->allocateRegister();
         lowReg = cg->allocateRegister();
         if (cg->comp()->target().cpu.id() >= TR_PPCp8)
            {
            TR::Register * tmp1 = cg->allocateRegister(TR_FPR);
            generateMvFprGprInstructions(cg, node, fpr2gprHost32, false, highReg, lowReg, doubleReg, tmp1);
            cg->stopUsingRegister(tmp1);
            }
         else
            generateMvFprGprInstructions(cg, node, fpr2gprHost32, false, highReg, lowReg, doubleReg);
         }
      cg->decReferenceCount(child);
      }

   // There's a special-case check for NaN values, which have to be
   // normalized to a particular NaN value.
   //
   if (node->normalizeNanValues())
      {
      TR::Register *condReg = cg->allocateRegister(TR_CCR);

      TR::LabelSymbol *nanNormalizeStartLabel = generateLabelSymbol(cg);
      TR::LabelSymbol *nanNormalizeEndLabel = generateLabelSymbol(cg);

      uint16_t numDeps = cg->comp()->target().is64Bit() ? 2 : 3;
      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(numDeps, numDeps, cg->trMemory());
      TR::addDependency(deps, condReg, TR::RealRegister::NoReg, TR_CCR, cg);
      if (cg->comp()->target().is64Bit())
         {
         TR::addDependency(deps, lReg, TR::RealRegister::NoReg, TR_GPR, cg);
         }
      else
         {
         TR::addDependency(deps, lowReg, TR::RealRegister::NoReg, TR_GPR, cg);
         TR::addDependency(deps, highReg, TR::RealRegister::NoReg, TR_GPR, cg);
         }

      generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, doubleReg, doubleReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, nanNormalizeStartLabel);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, PPCOpProp_BranchLikely, node, nanNormalizeEndLabel, condReg);
      if (cg->comp()->target().is64Bit())
         {
         loadConstant(cg, node, (int64_t)CONSTANT64(0x7ff8000000000000), lReg);
         }
      else
         {
         generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, highReg, 0x7ff8);
         generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, lowReg, 0);
         }
      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, nanNormalizeEndLabel, deps);

      cg->stopUsingRegister(condReg);
      }

   if (!childEval)
      cg->stopUsingRegister(doubleReg);

   if (cg->comp()->target().is64Bit())
      {
      node->setRegister(lReg);
      return lReg;
      }
   else
      {
      TR::RegisterPair *target = cg->allocateRegisterPair(lowReg, highReg);
      node->setRegister(target);
      return target;
      }
   }

static TR::Register *fconstHandler(TR::Node *node, TR::CodeGenerator *cg, float value)
   {
   TR::Register *trgRegister = cg->allocateSinglePrecisionRegister();
   TR::Register *srcRegister, *tempReg=NULL;
   TR::Instruction *q[4];
   int32_t      offset;
   TR::Compilation *comp = cg->comp();


   if (cg->comp()->target().is64Bit())
      {
      offset = cg->findOrCreateFloatConstant(&value, TR::Float, NULL, NULL, NULL, NULL);
      if (offset != PTOC_FULL_INDEX)
         {
         if (offset<LOWER_IMMED || offset>UPPER_IMMED)
            {
            srcRegister = cg->allocateRegister();
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, srcRegister, cg->getTOCBaseRegister(), HI_VALUE(offset));
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfs, node, trgRegister, new (cg->trHeapMemory()) TR::MemoryReference(srcRegister, LO_VALUE(offset), 4, cg));
            cg->stopUsingRegister(srcRegister);
            }
         else
            {
            generateTrg1MemInstruction(cg, TR::InstOpCode::lfs, node, trgRegister, new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), offset, 4, cg));
            }
         }
      }

   if (cg->comp()->target().is32Bit() || offset==PTOC_FULL_INDEX)
      {
      srcRegister = cg->allocateRegister();
      if (cg->comp()->target().is64Bit())
         tempReg = cg->allocateRegister();
      fixedSeqMemAccess(cg, node, 0, q, trgRegister, srcRegister, TR::InstOpCode::lfs, 4, NULL, tempReg);
      cg->findOrCreateFloatConstant(&value, TR::Float, q[0], q[1], q[2], q[3]);
      cg->stopUsingRegister(srcRegister);
      if (cg->comp()->target().is64Bit())
         cg->stopUsingRegister(tempReg);
      }
   node->setRegister(trgRegister);
   return trgRegister;
   }

TR::Register *OMR::Power::TreeEvaluator::fconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return fconstHandler(node, cg, node->getFloat());
   }

TR::Register *OMR::Power::TreeEvaluator::dconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   double            value = node->getDouble();
   TR::InstOpCode::Mnemonic     opcode = TR::InstOpCode::lfd;
   bool              splats = false;
   TR::Compilation *comp = cg->comp();

   if (node->getOpCodeValue() == TR::vsplats)
      {
      splats = true;
      }

   if (splats)
      {
      value = node->getFirstChild()->getDouble();
      opcode = TR::InstOpCode::lxvdsx;
      }


   if (!splats)
      {
      if (value == ((double) ((float) value)))
         return fconstHandler(node, cg, (float) value);
      }

   TR::Register *trgRegister;

   if (splats)
      trgRegister = cg->allocateRegister(TR_VSX_VECTOR);
   else
      trgRegister = cg->allocateRegister(TR_FPR);

   TR::Register       *srcRegister, *tempReg=NULL;
   TR::Instruction *q[4];
   int32_t            offset;

   if (cg->comp()->target().is64Bit())
      {
      offset = cg->findOrCreateFloatConstant(&value, TR::Double, NULL, NULL, NULL, NULL);
      if (offset != PTOC_FULL_INDEX)
         {
         if (offset<LOWER_IMMED || offset>UPPER_IMMED)
            {
            srcRegister = cg->allocateRegister();
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, srcRegister, cg->getTOCBaseRegister(), HI_VALUE(offset));

            TR::MemoryReference *memRef = new (cg->trHeapMemory()) TR::MemoryReference(srcRegister, LO_VALUE(offset), 8, cg);
            if (splats)
               memRef->forceIndexedForm(node, cg);

            generateTrg1MemInstruction(cg, opcode, node, trgRegister, memRef);
            cg->stopUsingRegister(srcRegister);
            }
         else
            {
            TR::MemoryReference *memRef = new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), offset, 8, cg);
            if (splats)
               memRef->forceIndexedForm(node, cg);

            generateTrg1MemInstruction(cg, opcode, node, trgRegister, memRef);
            }
         }
      }

   if (cg->comp()->target().is32Bit() || offset==PTOC_FULL_INDEX)
      {
      srcRegister = cg->allocateRegister(TR_GPR);
      if (cg->comp()->target().is64Bit())
         tempReg = cg->allocateRegister(TR_GPR);
      fixedSeqMemAccess(cg, node, 0, q, trgRegister, srcRegister, opcode, 8, NULL, tempReg);
      cg->stopUsingRegister(srcRegister);
      if (cg->comp()->target().is64Bit())
         cg->stopUsingRegister(tempReg);
      cg->findOrCreateFloatConstant(&value, TR::Double, q[0], q[1], q[2], q[3]);
      }
   node->setRegister(trgRegister);
   return trgRegister;
   }

// also handles TR::floadi
TR::Register *OMR::Power::TreeEvaluator::floadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = node->setRegister(cg->allocateSinglePrecisionRegister());
   TR::MemoryReference *tempMR;
   bool needSync= node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP();

   // If the reference is volatile or potentially volatile, the layout needs to be
   // fixed for patching if it turns out to be not a volatile atfer all.
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);

   generateTrg1MemInstruction(cg, TR::InstOpCode::lfs, node, tempReg, tempMR);
   if (needSync)
      {
      TR::TreeEvaluator::postSyncConditions(node, cg, tempReg, tempMR, TR::InstOpCode::isync);
      }
   tempMR->decNodeReferenceCounts(cg);

   return tempReg;
   }


TR::Register *OMR::Power::TreeEvaluator::dloadHelper(TR::Node *node, TR::CodeGenerator *cg, TR::Register *tempReg, TR::InstOpCode::Mnemonic opcode)
   {
   TR::Compilation *comp = cg->comp();
   TR::MemoryReference *tempMR;
   bool needSync= node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP();
   if (cg->comp()->target().is32Bit() && needSync && !cg->is64BitProcessor())
      {
      TR::Register *addrReg = cg->allocateRegister();
      TR::SymbolReference *vrlRef = comp->getSymRefTab()->findOrCreateVolatileReadDoubleSymbolRef(comp->getMethodSymbol());

      if (node->getSymbolReference()->isUnresolved())
         {
#ifdef J9_PROJECT_SPECIFIC
         // NOTE NOTE -- If the reference is volatile or potentially volatile, the layout
         // needs to be fixed for patching if it turns out to be not a volatile after all.
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);

         if (opcode == TR::InstOpCode::lxvdsx)
            tempMR->forceIndexedForm(node, cg);

         generateTrg1MemInstruction(cg, opcode, node, tempReg, tempMR);
         tempMR->getUnresolvedSnippet()->setIsSpecialDouble();
         tempMR->getUnresolvedSnippet()->setInSyncSequence();
#endif
         }
      else
         {
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
         if (tempMR->getIndexRegister() != NULL)
            generateTrg1Src2Instruction (cg, TR::InstOpCode::add, node, addrReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         else
            generateTrg1MemInstruction (cg, TR::InstOpCode::addi2, node, addrReg, tempMR);
         }
      TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
      TR::addDependency(dependencies, tempReg, TR::RealRegister::fp0, TR_FPR, cg);
      TR::addDependency(dependencies, addrReg, TR::RealRegister::gr3, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
      if (node->getSymbolReference()->isUnresolved())
         {
         if (tempMR->getBaseRegister() != NULL)
            {
            TR::addDependency(dependencies, tempMR->getBaseRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
            dependencies->getPreConditions()->getRegisterDependency(3)->setExcludeGPR0();
            dependencies->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0();
            }
         if (tempMR->getIndexRegister() != NULL)
            TR::addDependency(dependencies, tempMR->getIndexRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
         }
      generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node,
                                   (uintptr_t)vrlRef->getSymbol()->castToMethodSymbol()->getMethodAddress(),
                                   dependencies, vrlRef);

      // tempMR registers possibly repeatedly declared dead but no harm.
      dependencies->stopUsingDepRegs(cg, tempReg);
      cg->machine()->setLinkRegisterKilled(true);
      }
   else
      {
      // If the reference is volatile or potentially volatile, the layout needs to be
      // fixed for patching if it turns out to be not a volatile after all.
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);

      if (opcode == TR::InstOpCode::lxvdsx || opcode == TR::InstOpCode::lxsdx)
         {
         if (tempMR->hasDelayedOffset())
            {
            TR::Register *tmpReg = cg->allocateRegister();
            generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, tmpReg, tempMR);
            tempMR->decNodeReferenceCounts(cg);

            tempMR = new (cg->trHeapMemory()) TR::MemoryReference(NULL, tmpReg, 16, cg);
            }
         else
            {
            tempMR->forceIndexedForm(node, cg);
            }
         }
      generateTrg1MemInstruction(cg, opcode, node, tempReg, tempMR);
      if (needSync)
         {
         TR::TreeEvaluator::postSyncConditions(node, cg, tempReg, tempMR, TR::InstOpCode::isync);
         }
      }

   tempMR->decNodeReferenceCounts(cg);

   return tempReg;
   }

// also handles TR::dloadi
TR::Register *OMR::Power::TreeEvaluator::dloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = node->setRegister(cg->allocateRegister(TR_FPR));
   return TR::TreeEvaluator::dloadHelper(node, cg, tempReg, TR::InstOpCode::lfd);
   }

TR::Register *OMR::Power::TreeEvaluator::vsplatsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   static bool disableDirectMove = feGetEnv("TR_disableDirectMove") ? true : false;

   if (node->getDataType() == TR::VectorInt8)
      {
      TR::SymbolReference    *temp    = cg->allocateLocalTemp(TR::VectorInt8);
      TR::MemoryReference *tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(node, temp, 1, cg);
      TR::Register *srcReg = cg->evaluate(child);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node, tempMR, srcReg);

      TR::Register *tmpReg = cg->allocateRegister();

      generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, tmpReg, tempMR);
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(NULL, tmpReg, 16, cg);
      cg->stopUsingRegister(tmpReg);

      TR::Register *trgReg = cg->allocateRegister(TR_VRF);

      generateTrg1MemInstruction(cg, TR::InstOpCode::lxvd2x, node, trgReg, tempMR);
#if defined(__LITTLE_ENDIAN__)
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::vspltb, node, trgReg, trgReg, 7);
#else
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::vspltb, node, trgReg, trgReg, 0);
#endif
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   else if (node->getDataType() == TR::VectorInt16)
      {
      TR::SymbolReference    *temp    = cg->allocateLocalTemp(TR::VectorInt16);
      TR::MemoryReference *tempMR  = new (cg->trHeapMemory()) TR::MemoryReference(node, temp, 2, cg);
      TR::Register *srcReg = cg->evaluate(child);
      generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node, tempMR, srcReg);

      TR::Register *tmpReg = cg->allocateRegister();

      generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, tmpReg, tempMR);
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(NULL, tmpReg, 16, cg);
      cg->stopUsingRegister(tmpReg);

      TR::Register *trgReg = cg->allocateRegister(TR_VRF);

      generateTrg1MemInstruction(cg, TR::InstOpCode::lxvd2x, node, trgReg, tempMR);
#if defined(__LITTLE_ENDIAN__)
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::vsplth, node, trgReg, trgReg, 3);
#else
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::vsplth, node, trgReg, trgReg, 0);
#endif
      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   else if (node->getDataType() == TR::VectorInt32)
      {
      TR::Register *tempReg = cg->evaluate(child);
      TR::Register *resReg = cg->allocateRegister(TR_VRF);

      if (!disableDirectMove && cg->comp()->target().cpu.id() >= TR_PPCp8 && cg->comp()->target().cpu.getPPCSupportsVSX())
         {
         generateMvFprGprInstructions(cg, node, gprLow2fpr, false, resReg, tempReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, resReg, resReg, 0x1);
         }
      else
         {
         TR::SymbolReference    *localTemp = cg->allocateLocalTemp(TR::Int32);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, new (cg->trHeapMemory()) TR::MemoryReference(node, localTemp, 4, cg), tempReg);

         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, localTemp, 4, cg);
         tempMR->forceIndexedForm(node, cg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lxsdx, node, resReg, tempMR);
         tempMR->decNodeReferenceCounts(cg);
#if defined(__LITTLE_ENDIAN__)
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, resReg, resReg, 1);
#else
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, resReg, resReg, 0);
#endif
         }

      cg->stopUsingRegister(tempReg);
      node->setRegister(resReg);
      cg->decReferenceCount(child);

      return resReg;
      }
   else if (node->getDataType() == TR::VectorInt64)
      {
      TR::Register *srcReg = cg->evaluate(child);
      TR::Register *trgReg = cg->allocateRegister(TR_VRF);

      if (!disableDirectMove && cg->comp()->target().cpu.id() >= TR_PPCp8 && cg->comp()->target().cpu.getPPCSupportsVSX())
         {
         if (cg->comp()->target().is64Bit())
            {
            generateMvFprGprInstructions(cg, node, gpr2fprHost64, false, trgReg, srcReg);
            generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, trgReg, trgReg, trgReg, 0x0);
            }
         else
            {
            TR::Register *tempVectorReg = cg->allocateRegister(TR_VSX_VECTOR);
            generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, trgReg, srcReg->getHighOrder(), srcReg->getLowOrder(), tempVectorReg);
            generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, trgReg, trgReg, trgReg, 0x0);
            cg->stopUsingRegister(tempVectorReg);
            }
         }
      else
         {
         TR::SymbolReference *temp   = cg->allocateLocalTemp(TR::Int64);
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, temp, 8, cg);

         if (cg->comp()->target().is64Bit())
            {
            generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempMR, srcReg);
            }
         else
            {
            TR::MemoryReference *tempMR2 =  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMR, 4, 4, cg);
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR, srcReg->getHighOrder());
            generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMR2, srcReg->getLowOrder());
            }

         TR::Register *tmpReg = cg->allocateRegister();
         generateTrg1MemInstruction(cg, TR::InstOpCode::addi2, node, tmpReg, tempMR);
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(NULL, tmpReg, 16, cg);
         cg->stopUsingRegister(tmpReg);
         generateTrg1MemInstruction(cg, TR::InstOpCode::lxvdsx, node, trgReg, tempMR);
         }

      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }
   else if (node->getDataType() == TR::VectorFloat)
      {
      TR::Register   *srcReg = cg->evaluate(child);
      TR::Register   *trgReg  = cg->allocateRegister(TR_VRF);

      generateTrg1Src1Instruction(cg, TR::InstOpCode::xscvdpsp, node, trgReg, srcReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, trgReg, trgReg, 0);

      node->setRegister(trgReg);
      cg->decReferenceCount(child);
      return trgReg;
      }

   TR_ASSERT(node->getDataType() == TR::VectorDouble, "unsupported splats type");

   if (child->getOpCode().isLoadConst())
      {
      TR::Register *resReg = TR::TreeEvaluator::dconstEvaluator(node, cg);
      cg->decReferenceCount(child);
      return resReg;
      }
   else if (child->getRegister() == NULL &&
            child->getOpCode().isLoadVar())
      {
      if (child->getReferenceCount() > 1)
         {
         TR::Node *newNode = child->duplicateTree(false);

         cg->evaluate(child);
         cg->decReferenceCount(child);
         child = newNode;
         }

      TR::Register *resReg = node->setRegister(cg->allocateRegister(TR_VSX_VECTOR));
      return TR::TreeEvaluator::dloadHelper(child, cg, resReg, TR::InstOpCode::lxvdsx);
      }
   else
      {
      TR::Register *srcReg = cg->evaluate(child);
      TR::Register *resReg = node->setRegister(cg->allocateRegister(TR_VSX_VECTOR));
      generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, resReg, srcReg, srcReg, 0x0);

      cg->decReferenceCount(child);
      return resReg;
      }

   return NULL;
   }


TR::Register *OMR::Power::TreeEvaluator::vdgetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Register *resReg = node->setRegister(cg->allocateRegister(TR_FPR));

   if (secondChild->getOpCode().isLoadConst())
      {
      int elem = secondChild->getInt();
      TR_ASSERT(elem == 0 || elem == 1, "Element can only be 0 or 1\n");

      TR::Register *srcReg = cg->evaluate(firstChild);

      if (elem == 1)
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxsldwi, node, resReg, srcReg, srcReg, 0x2);
      else
         generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, resReg, srcReg, srcReg);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(secondChild);
      return resReg;
      }

   TR::Register *vectorReg = cg->evaluate(firstChild);
   TR::Register *idxReg = cg->evaluate(secondChild);
   TR::Register    *condReg = cg->allocateRegister(TR_CCR);
   TR::LabelSymbol *jumpLabel = generateLabelSymbol(cg);

   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, idxReg, 0);

   // copy vector register to result register
   generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, resReg, vectorReg, vectorReg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, jumpLabel, condReg);
   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxsldwi, node, resReg, vectorReg, vectorReg, 0x2);

   TR::RegisterDependencyConditions *dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 4, cg->trMemory());
   dep->addPostCondition(vectorReg, TR::RealRegister::NoReg);
   dep->addPostCondition(idxReg, TR::RealRegister::NoReg);
   dep->addPostCondition(resReg, TR::RealRegister::NoReg);
   dep->addPostCondition(condReg, TR::RealRegister::NoReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, jumpLabel, dep);

   cg->stopUsingRegister(condReg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return resReg;

   }


TR::Register *OMR::Power::TreeEvaluator::vdsetelemEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Node *secondChild = node->getSecondChild();
   TR::Node *thirdChild = node->getThirdChild();
   TR::Register *vectorReg = cg->evaluate(firstChild);
   TR::Register *resReg = node->setRegister(cg->allocateRegister(TR_VSX_VECTOR));

   if (thirdChild->getOpCode().isLoadConst())
      {
      int elem = thirdChild->getInt();
      TR_ASSERT(elem == 0 || elem == 1, "Element can only be 0 or 1\n");

      if (secondChild->getRegister() == NULL &&
         secondChild->getOpCode().isLoadVar())
         {
         if (secondChild->getReferenceCount() > 1)
            {
            TR::Node *newNode = secondChild->duplicateTree(false);
             cg->evaluate(secondChild);
            cg->decReferenceCount(secondChild);
            secondChild = newNode;
            }

         TR::TreeEvaluator::dloadHelper(secondChild, cg, resReg, TR::InstOpCode::lxsdx);
	      }
         else
	      {
         TR::Register *fprReg = cg->evaluate(secondChild);
         generateTrg1Src2Instruction(cg, TR::InstOpCode::xxlor, node, resReg, fprReg, fprReg);

         cg->decReferenceCount(secondChild);
         }

      if (elem == 0)
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, resReg, resReg, vectorReg, 0x1);
      else
         generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, resReg, vectorReg, resReg, 0x0);

      cg->decReferenceCount(firstChild);
      cg->decReferenceCount(thirdChild);

      return resReg;
      }

   TR::Register *idxReg = cg->evaluate(secondChild);
   TR::Register *valueReg = cg->evaluate(thirdChild);
   TR::Register    *condReg = cg->allocateRegister(TR_CCR);
   TR::LabelSymbol *doneLabel = generateLabelSymbol(cg);
   // cmpwi index, 0
   // xxpermdi result, value, vector, 1
   // beq 1f
   // xxmrghd  result, vector, value
   // 1:
   generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::cmpi4, node, condReg, idxReg, 0);

   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, resReg, valueReg, vectorReg, 0x1);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
   generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, resReg, vectorReg, valueReg, 0x0);

   TR::RegisterDependencyConditions *dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg->trMemory());
   dep->addPostCondition(vectorReg, TR::RealRegister::NoReg);
   dep->addPostCondition(idxReg, TR::RealRegister::NoReg);
   dep->addPostCondition(valueReg, TR::RealRegister::NoReg);
   dep->addPostCondition(resReg, TR::RealRegister::NoReg);
   dep->addPostCondition(condReg, TR::RealRegister::NoReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, dep);

   cg->stopUsingRegister(condReg);

   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->decReferenceCount(thirdChild);

   return resReg;
   }


//handles both fstore and ifstore
TR::Register *OMR::Power::TreeEvaluator::fstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool indirect = node->getOpCode().isIndirect();
   int childIndex = indirect?1:0;
   TR::Node *child = node->getChild(childIndex);

   // Special case storing an int value into a float variable
   //
   if (child->getRegister()==NULL && child->getOpCodeValue() == TR::ibits2f)
      {
      if (child->getReferenceCount()>1)
         node->setAndIncChild(childIndex, child->getFirstChild());
      else
         node->setChild(childIndex, child->getFirstChild());
      TR::Node::recreate(node, indirect?TR::istorei:TR::istore);
      TR::TreeEvaluator::istoreEvaluator(node, cg);
      node->setChild(childIndex, child);
      TR::Node::recreate(node, indirect?TR::fstorei:TR::fstore);
      cg->decReferenceCount(child);
      return NULL;
      }

   TR::Register *valueReg = cg->evaluate(child);
   TR::MemoryReference *tempMR;
   bool    needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   // If the reference is volatile or potentially volatile, the layout needs to be
   // fixed for patching if it turns out to be not a volatile after all.
   tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);

   if (needSync)
      generateInstruction(cg, TR::InstOpCode::lwsync, node);
   generateMemSrc1Instruction(cg, TR::InstOpCode::stfs, node, tempMR, valueReg);
   if (needSync)
      {
      TR::TreeEvaluator::postSyncConditions(node, cg, valueReg, tempMR, TR::InstOpCode::sync);
      }
   cg->decReferenceCount(child);
   tempMR->decNodeReferenceCounts(cg);

   return NULL;
   }

//handles both dstore and idstore
TR::Register* OMR::Power::TreeEvaluator::dstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool indirect = node->getOpCode().isIndirect();
   int childIndex = indirect?1:0;
   TR::Node *child = node->getChild(childIndex);
   bool  isUnresolved = node->getSymbolReference()->isUnresolved();
   TR::Compilation *comp = cg->comp();

   // Special case storing a long value into a double variable
   //
   if (child->getRegister()==NULL && child->getOpCodeValue() == TR::lbits2d)
      {
      if (child->getReferenceCount()>1)
         node->setAndIncChild(childIndex, child->getFirstChild());
      else
         node->setChild(childIndex, child->getFirstChild());
      TR::Node::recreate(node, indirect?TR::lstorei:TR::lstore);
      TR::TreeEvaluator::lstoreEvaluator(node, cg);
      node->setChild(childIndex, child);
      TR::Node::recreate(node, indirect?TR::dstorei:TR::dstore);
      cg->decReferenceCount(child);
      return NULL;
      }


   TR::Register *valueReg = cg->evaluate(child);
   bool needSync= node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP();
   TR::MemoryReference *tempMR;
   if (cg->comp()->target().is32Bit() && needSync && !cg->is64BitProcessor())
      {
      TR::Register *addrReg = cg->allocateRegister();
      TR::SymbolReference    *vrlRef = comp->getSymRefTab()->findOrCreateVolatileWriteDoubleSymbolRef(comp->getMethodSymbol());
      TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
      if (isUnresolved)
         {
#ifdef J9_PROJECT_SPECIFIC
         // NOTE NOTE -- If the reference is volatile or potentially volatile, the layout
         // needs to be fixed for patching if it turns out to be not a volatile after all.
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, valueReg);
         tempMR->getUnresolvedSnippet()->setIsSpecialDouble();
         tempMR->getUnresolvedSnippet()->setInSyncSequence();
#endif
         }
      else
         {
         tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
         if (tempMR->getIndexRegister() != NULL)
             generateTrg1Src2Instruction (cg, TR::InstOpCode::add, node, addrReg, tempMR->getBaseRegister(), tempMR->getIndexRegister());
         else
             generateTrg1MemInstruction (cg, TR::InstOpCode::addi2, node, addrReg, tempMR);
         }
      TR::addDependency(dependencies, valueReg, TR::RealRegister::fp0, TR_FPR, cg);
      TR::addDependency(dependencies, addrReg, TR::RealRegister::gr3, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
      if (isUnresolved)
         {
         if (tempMR->getBaseRegister() != NULL)
            {
            TR::addDependency(dependencies, tempMR->getBaseRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
            dependencies->getPreConditions()->getRegisterDependency(3)->setExcludeGPR0();
            dependencies->getPostConditions()->getRegisterDependency(3)->setExcludeGPR0();
            }
         if (tempMR->getIndexRegister() != NULL)
            TR::addDependency(dependencies, tempMR->getIndexRegister(), TR::RealRegister::NoReg, TR_GPR, cg);
         }
      generateDepImmSymInstruction(cg, TR::InstOpCode::bl, node,
         (uintptr_t)vrlRef->getSymbol()->castToMethodSymbol()->getMethodAddress(),
         dependencies, vrlRef);

      // tempMR registers may be repeatedly declared dead but no harm.
      dependencies->stopUsingDepRegs(cg);

      cg->machine()->setLinkRegisterKilled(true);
      }
   else
      {
      // If the reference is volatile or potentially volatile, the layout needs to be
      // fixed for patching if it turns out to be not a volatile after all.
      tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
      if (needSync)
         generateInstruction(cg, TR::InstOpCode::lwsync, node);
      generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMR, valueReg);
      if (needSync)
         {
         TR::TreeEvaluator::postSyncConditions(node, cg, valueReg, tempMR, TR::InstOpCode::sync);
         }
      }
   tempMR->decNodeReferenceCounts(cg);
   cg->decReferenceCount(child);
   return NULL;
   }

//Handles both freturn and dreturn
TR::Register *OMR::Power::TreeEvaluator::freturnEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool isDouble = node->getOpCodeValue() == TR::dreturn;
   TR::Register *returnRegister = cg->evaluate(node->getFirstChild());
   const TR::PPCLinkageProperties &linkageProperties = cg->getProperties();
   TR::RealRegister::RegNum machineReturnRegister =
                linkageProperties.getFloatReturnRegister();
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 0, cg->trMemory());
   dependencies->addPreCondition(returnRegister, machineReturnRegister);
   generateAdminInstruction(cg, TR::InstOpCode::ret, node);
   generateDepInstruction(cg, TR::InstOpCode::blr, node, dependencies);
   cg->decReferenceCount(node->getFirstChild());
   return NULL;
   }

// return true if mul node is marked appropriately and not shared.
//
static bool isFPStrictMul(TR::Node *node, TR::Compilation * comp)
   {
   if(!node->getOpCode().isMul() ||
      !node->isFPStrictCompliant() ||
      node->getRegister())
   return false;

   if( node->getReferenceCount() < 2 && node->getRegister() == NULL)
     return true;

   node->setIsFPStrictCompliant(false);// need to set this otherwise children get incorrectly bumped
   return false;
   }

/** Generate a fused multiply add from the tree (A * B) + C, where addNode is the + node
 * and mulNode the * subtree.
 */
static TR::Register *generateFusedMultiplyAdd(TR::Node *addNode,TR::InstOpCode::Mnemonic OpCode, TR::CodeGenerator *cg)
   {
   TR::Node     *mulNode = addNode->getFirstChild();
   TR::Node     *addChild    = addNode->getSecondChild();

   if (!isFPStrictMul(mulNode, cg->comp()))
      {
      addChild = addNode->getFirstChild();
      mulNode  = addNode->getSecondChild();
      }

   TR_ASSERT(mulNode->getReferenceCount() < 2,"Mul node 0x%p reference count %d >= 2\n",mulNode,mulNode->getReferenceCount());
   TR_ASSERT(mulNode->getOpCode().isMul(),"Unexpected op!=mul %p\n",mulNode);
   TR_ASSERT(mulNode->isFPStrictCompliant(),"mul node %p is not fpStrict Compliant\n",mulNode);
   TR::Register *source1Register = cg->evaluate(mulNode->getFirstChild());
   TR::Register *source2Register = cg->evaluate(mulNode->getSecondChild());
   TR::Register *source3Register = cg->evaluate(addChild);

   TR::Register *trgRegister;
   if(addNode->getDataType() == TR::Float)
      trgRegister  = cg->allocateSinglePrecisionRegister();
   else
      trgRegister  = cg->allocateRegister(TR_FPR);  // TR::Double

   generateTrg1Src3Instruction(cg, OpCode, addNode, trgRegister, source1Register, source2Register,source3Register);
   addNode->setRegister(trgRegister);

   cg->decReferenceCount(mulNode->getFirstChild());
   cg->decReferenceCount(mulNode->getSecondChild());
   cg->decReferenceCount(mulNode); // don't forget this guy!
   cg->decReferenceCount(addChild);

   return trgRegister;
   }

TR::Register *OMR::Power::TreeEvaluator::faddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *result=NULL;
   if((isFPStrictMul(node->getFirstChild(), comp) ||
       isFPStrictMul(node->getSecondChild(), comp)) &&
       performTransformation(comp, "O^O Changing [%p] to fmadd\n",node))
      result = generateFusedMultiplyAdd(node, TR::InstOpCode::fmadds, cg);
   else
      result = singlePrecisionEvaluator(node, TR::InstOpCode::fadds, cg);
   return result;
   }
TR::Register *OMR::Power::TreeEvaluator::daddEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *result=NULL;
   if((isFPStrictMul(node->getFirstChild(), comp) ||
      isFPStrictMul(node->getSecondChild(), comp)) &&
       performTransformation(comp, "O^O Changing [%p] to dmadd\n",node))
      result = generateFusedMultiplyAdd(node, TR::InstOpCode::fmadd, cg);
   else
      result = doublePrecisionEvaluator(node, TR::InstOpCode::fadd, cg);
   return result;
   }

TR::Register *OMR::Power::TreeEvaluator::dsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *result=NULL;
   if(isFPStrictMul(node->getFirstChild(), comp) &&
      performTransformation(comp, "O^O Changing [%p] to fmsub\n",node))
      result = generateFusedMultiplyAdd(node, TR::InstOpCode::fmsub, cg);
   else
      {
      if (isFPStrictMul(node->getSecondChild(), comp) &&
          performTransformation(comp, "O^O Changing [%p] to fnmsub\n",node))
         {
         result = generateFusedMultiplyAdd(node, TR::InstOpCode::fnmsub, cg);
         }
      else
         result = doublePrecisionEvaluator(node, TR::InstOpCode::fsub, cg);
      }

   return result;
   }

TR::Register *OMR::Power::TreeEvaluator::fsubEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *result=NULL;
   if(isFPStrictMul(node->getFirstChild(), comp)&&
       performTransformation(comp, "O^O Changing [%p] to fmsub\n",node))
      result = generateFusedMultiplyAdd(node, TR::InstOpCode::fmsubs, cg);
   else
      {
      if (isFPStrictMul(node->getSecondChild(), comp) &&
          performTransformation(comp, "O^O Changing [%p] to fnmsub\n",node))
         {
         result = generateFusedMultiplyAdd(node, TR::InstOpCode::fnmsubs, cg);
         }
      else
        result = singlePrecisionEvaluator(node, TR::InstOpCode::fsubs, cg);
      }
   return result;
   }

TR::Register *OMR::Power::TreeEvaluator::fmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::fmuls, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::dmulEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::fmul, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fdivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return singlePrecisionEvaluator(node, TR::InstOpCode::fdivs, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::ddivEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return doublePrecisionEvaluator(node, TR::InstOpCode::fdiv, cg);
   }

TR::Register *OMR::Power::TreeEvaluator::fremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg      = cg->allocateSinglePrecisionRegister();
   TR::Node     *child1      = node->getFirstChild();
   TR::Node     *child2      = node->getSecondChild();
   TR::Register *source1Reg  = cg->evaluate(child1);
   TR::Register *source2Reg  = cg->evaluate(child2);
   TR::Register *copyReg;
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(16, 16, cg->trMemory());

   if (!cg->canClobberNodesRegister(child1))
      {
      copyReg = cg->allocateSinglePrecisionRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, copyReg, source1Reg);
      source1Reg = copyReg;
      }
   if (!cg->canClobberNodesRegister(child2))
      {
      copyReg = cg->allocateSinglePrecisionRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, copyReg, source2Reg);
      source2Reg = copyReg;
      }

   TR::addDependency(dependencies, source1Reg, TR::RealRegister::fp0, TR_FPR, cg);
   TR::addDependency(dependencies, source2Reg, TR::RealRegister::fp1, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr3, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr4, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr8, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr0, TR_CCR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr1, TR_CCR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr6, TR_CCR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp2, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp3, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp4, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp5, TR_FPR, cg);
   TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCdoubleRemainder, node, dependencies, cg);

   // all registers on dep are now not used any longer, except source1
   dependencies->stopUsingDepRegs(cg, source1Reg);

   generateTrg1Src1Instruction(cg, TR::InstOpCode::frsp, node, trgReg, source1Reg);

   cg->decReferenceCount(child1);
   cg->decReferenceCount(child2);
   cg->stopUsingRegister(source1Reg);

   node->setRegister(trgReg);
   cg->machine()->setLinkRegisterKilled(true);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::dremEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child1      = node->getFirstChild();
   TR::Node     *child2      = node->getSecondChild();
   TR::Register *source1Reg  = cg->evaluate(child1);
   TR::Register *source2Reg  = cg->evaluate(child2);
   TR::Register *copyReg;
   TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(16, 16, cg->trMemory());

   if (!cg->canClobberNodesRegister(child1))
      {
      copyReg = cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, copyReg, source1Reg);
      source1Reg = copyReg;
      }
   if (!cg->canClobberNodesRegister(child2))
      {
      copyReg = cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, copyReg, source2Reg);
      source2Reg = copyReg;
      }

   TR::addDependency(dependencies, source1Reg, TR::RealRegister::fp0, TR_FPR, cg);
   TR::addDependency(dependencies, source2Reg, TR::RealRegister::fp1, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr3, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr4, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr8, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr0, TR_CCR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr1, TR_CCR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::cr6, TR_CCR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp2, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp3, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp4, TR_FPR, cg);
   TR::addDependency(dependencies, NULL, TR::RealRegister::fp5, TR_FPR, cg);
   TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCdoubleRemainder, node, dependencies, cg);

   node->setRegister(source1Reg);
   cg->decReferenceCount(child1);
   cg->decReferenceCount(child2);

   if (source1Reg->isSinglePrecision())
      source1Reg->setIsSinglePrecision(false);

   // all regsiters(except source1) on dep are now not used any longer.
   dependencies->stopUsingDepRegs(cg, source1Reg);

   cg->machine()->setLinkRegisterKilled(true);
   return source1Reg;
   }

/**
 * regular neg simplification, but if child is fmadd/fmsub, and node not
 * shared, generate fnmadd/fnmsub
 * handles both fneg and dneg
 */
TR::Register *OMR::Power::TreeEvaluator::fnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   bool noFMA=true;
   TR::Register *trgRegister;
   TR::Compilation *comp = cg->comp();
   TR::Node     *firstChild = node->getFirstChild();
   bool isAddOrSub = firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub();
   if(firstChild->getReferenceCount() < 2 && !firstChild->getRegister() && isAddOrSub)
      {
      TR::Register *result=NULL;
      bool isAdd = firstChild->getOpCode().isAdd();

      if((isFPStrictMul(firstChild->getFirstChild(), comp) ||
            (isAdd && isFPStrictMul(firstChild->getSecondChild(), comp))) &&
         performTransformation(comp, "O^O Changing [%p] to fnmadd/sub\n",node))
         {
         TR::InstOpCode::Mnemonic opCode =
            node->getOpCode().isFloat()
            ? (isAdd ? TR::InstOpCode::fnmadds:TR::InstOpCode::fnmsubs)
            : (isAdd ? TR::InstOpCode::fnmadd :TR::InstOpCode::fnmsub);
         noFMA=false;
         trgRegister = generateFusedMultiplyAdd(firstChild, opCode, cg);
         firstChild->unsetRegister();//unset as the first child isn't the result node
         }
      }
   if(noFMA)
      {
      TR::Register *sourceRegister = NULL;
      trgRegister  = node->getOpCode().isFloat() ? cg->allocateSinglePrecisionRegister() : cg->allocateRegister(TR_FPR);
      sourceRegister = cg->evaluate(firstChild);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fneg, node, trgRegister, sourceRegister);
      }
   cg->decReferenceCount(firstChild);
   node->setRegister(trgRegister);
   return trgRegister;
   }

TR::Register *OMR::Power::TreeEvaluator::int2dbl(TR::Node * node, TR::Register *srcReg, bool canClobber, TR::CodeGenerator *cg)
   {
   if (node->getOpCodeValue() == TR::b2f || node->getOpCodeValue() == TR::b2d)
      {
      TR::Register *tmpReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, TR::InstOpCode::extsb, node, tmpReg, srcReg);
      srcReg = tmpReg;
      cg->stopUsingRegister(tmpReg);
      }
   else if (node->getOpCodeValue() == TR::bu2f || node->getOpCodeValue() == TR::bu2d)
      {
      TR::Register *tmpReg = cg->allocateRegister();
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, tmpReg, srcReg, 0, 0xff);
      srcReg = tmpReg;
      cg->stopUsingRegister(tmpReg);
      }

   TR::Register *trgReg     = cg->allocateRegister(TR_FPR);
   TR::Register *tempReg;

   if (cg->is64BitProcessor() || (cg->comp()->compileRelocatableCode() && cg->comp()->target().is64Bit()))
      {
      if (cg->comp()->target().is64Bit())
         {
         if (node->getOpCodeValue() == TR::iu2f || node->getOpCodeValue() == TR::iu2d)
            {
            tempReg = cg->allocateRegister();
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicl, node, tempReg, srcReg, 0, CONSTANT64(0x00000000ffffffff));
            generateMvFprGprInstructions(cg, node, gpr2fprHost64, false, trgReg, tempReg);
            cg->stopUsingRegister(tempReg);
            }
         else if (node->getOpCodeValue() == TR::i2f || node->getOpCodeValue() == TR::i2d)
            {
            tempReg = cg->allocateRegister();
            generateTrg1Src1Instruction(cg, TR::InstOpCode::extsw, node, tempReg, srcReg);
            generateMvFprGprInstructions(cg, node, gpr2fprHost64, false, trgReg, tempReg);
            cg->stopUsingRegister(tempReg);
            }
         else
            generateMvFprGprInstructions(cg, node, gpr2fprHost64, false, trgReg, srcReg);
         }
      else
         {
         if (cg->comp()->target().cpu.id() >= TR_PPCp6 && node->getOpCodeValue() != TR::iu2f && node->getOpCodeValue() != TR::iu2d)
            generateMvFprGprInstructions(cg, node, gprLow2fpr, false, trgReg, srcReg);
         else
            {
            tempReg = cg->allocateRegister();

            if (node->getOpCodeValue() == TR::iu2f || node->getOpCodeValue() == TR::iu2d)
               generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, tempReg, 0);
            else
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi, node, tempReg, srcReg, 31);

            if (cg->comp()->target().cpu.id() >= TR_PPCp8)
               {
               TR::Register * tmp1 = cg->allocateRegister(TR_FPR);
               generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, trgReg, tempReg, srcReg, tmp1);
               cg->stopUsingRegister(tmp1);
               }
            else
               generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, trgReg, tempReg, srcReg);
            cg->stopUsingRegister(tempReg);
            }
         }
      if ((cg->comp()->target().cpu.id() >= TR_PPCp7) &&
          (node->getOpCodeValue() == TR::i2f || node->getOpCodeValue() == TR::iu2f))
         {
         // Generate the code to produce the float result here, setting the register flag is done afterwards
         if (node->getOpCodeValue() == TR::i2f)
            generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfids, node, trgReg, trgReg);
         else
            generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfidus, node, trgReg, trgReg);
         }
      else
         {
         generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, trgReg, trgReg);
         if (node->getOpCodeValue() == TR::i2f || node->getOpCodeValue() == TR::iu2f)
            generateTrg1Src1Instruction(cg, TR::InstOpCode::frsp, node, trgReg, trgReg);
         }
      }
   else
      {
      TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());
      if (!canClobber)
         {
         tempReg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, tempReg, srcReg);
         srcReg = tempReg;
         }
      TR::addDependency(dependencies, srcReg, TR::RealRegister::gr3, TR_GPR, cg);
      TR::addDependency(dependencies, trgReg, TR::RealRegister::fp0, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr4, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp1, TR_FPR, cg);
      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCinteger2Double, node, dependencies, cg);
      if (node->getOpCodeValue() == TR::i2f || node->getOpCodeValue() == TR::iu2f)
         generateTrg1Src1Instruction(cg, TR::InstOpCode::frsp, node, trgReg, trgReg);

      dependencies->stopUsingDepRegs(cg, trgReg);
      cg->machine()->setLinkRegisterKilled(true);
      }
   return trgReg;
   }

// also handles iu2f
TR::Register *OMR::Power::TreeEvaluator::i2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child      = node->getFirstChild();
   TR::Register *tempReg;
   TR::Register *trgReg;

   if (((cg->comp()->target().cpu.id() >= TR_PPCp7 &&
       (node->getOpCodeValue() == TR::iu2f && (child->getOpCodeValue() == TR::iuload || child->getOpCodeValue() == TR::iuloadi))) ||
       (cg->comp()->target().cpu.id() >= TR_PPCp6 &&
       (node->getOpCodeValue() == TR::i2f && (child->getOpCodeValue() == TR::iload || child->getOpCodeValue() == TR::iloadi)))) &&
       child->getReferenceCount() == 1 && child->getRegister() == NULL &&
       !(child->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP()))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      tempMR->forceIndexedForm(node, cg);
      tempReg = cg->allocateRegister(TR_FPR); // This one is 64bit
      trgReg = cg->allocateSinglePrecisionRegister(); // Allocate here
      if (node->getOpCodeValue() == TR::i2f)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, tempReg, tempMR);
         if (cg->comp()->target().cpu.id() >= TR_PPCp7)
            {
            generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfids, node, trgReg, tempReg);
            }
         else
            {
            generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, tempReg, tempReg);
            generateTrg1Src1Instruction(cg, TR::InstOpCode::frsp, node, trgReg, tempReg);
            }
         }
      else
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwzx, node, tempReg, tempMR);  // TR::iu2f, must be P7
         generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfidus, node, trgReg, tempReg);
         }
      tempMR->decNodeReferenceCounts(cg);
      cg->stopUsingRegister(tempReg);
      }
   else
      {
      trgReg = TR::TreeEvaluator::int2dbl(node, cg->evaluate(child), cg->canClobberNodesRegister(child), cg);
      trgReg->setIsSinglePrecision();
      cg->decReferenceCount(child);
      }
   node->setRegister(trgReg);
   return trgReg;
   }

// also handles iu2d, b2f, b2d, bu2f, bu2d, s2f, s2d, su2f, su2d
TR::Register *OMR::Power::TreeEvaluator::i2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child      = node->getFirstChild();
   TR::Register *trgReg;

   if (((cg->comp()->target().cpu.id() >= TR_PPCp7 &&
       (node->getOpCodeValue() == TR::iu2d && (child->getOpCodeValue() == TR::iuload || child->getOpCodeValue() == TR::iuloadi))) ||
       (cg->comp()->target().cpu.id() >= TR_PPCp6 &&
       node->getOpCodeValue() == TR::i2d && (child->getOpCodeValue() == TR::iload || child->getOpCodeValue() == TR::iloadi))) &&
       child->getReferenceCount()==1 &&
       child->getRegister() == NULL &&
       !(child->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP()))
      {
      // possible TODO: if refcount > 1, do both load and lfiwax?
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      tempMR->forceIndexedForm(node, cg);
      trgReg = cg->allocateRegister(TR_FPR);
      if (node->getOpCodeValue() == TR::i2d)
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, trgReg, tempMR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, trgReg, trgReg);
         }
      else
         {
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwzx, node, trgReg, tempMR); // iu2d
         generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfidu, node, trgReg, trgReg);
         }
      tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      trgReg = TR::TreeEvaluator::int2dbl(node, cg->evaluate(child), cg->canClobberNodesRegister(child), cg);
      cg->decReferenceCount(child);
      }
   node->setRegister(trgReg);
   return trgReg;
   }


TR::Register *OMR::Power::TreeEvaluator::long2dbl(TR::Node *node, TR::CodeGenerator *cg)
{
   TR::Node     *child      = node->getFirstChild();
   TR::Register *srcReg  = cg->evaluate(child);
   TR::Register *trgReg     = cg->allocateRegister(TR_FPR);


   if (cg->is64BitProcessor() || (cg->comp()->compileRelocatableCode() && cg->comp()->target().is64Bit()))
      {
      if (cg->comp()->target().is64Bit())
         generateMvFprGprInstructions(cg, node, gpr2fprHost64, false, trgReg, srcReg);
      else
         {
         if (cg->comp()->target().cpu.id() >= TR_PPCp8)
            {
            TR::Register * tmp1 = cg->allocateRegister(TR_FPR);
            generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, trgReg, srcReg->getHighOrder(), srcReg->getLowOrder(), tmp1);
            cg->stopUsingRegister(tmp1);
            }
         else
            generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, trgReg, srcReg->getHighOrder(), srcReg->getLowOrder());
         }
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, trgReg, trgReg);
      }
   else
      {
      TR::Register *srcLow, *srcHigh;
      TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(8, 8, cg->trMemory());

      if (!cg->canClobberNodesRegister(child))
         {
         srcLow = cg->allocateRegister();
         srcHigh = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, srcHigh, srcReg->getHighOrder());
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, srcLow, srcReg->getLowOrder());
         }
      else
         {
         srcLow = srcReg->getLowOrder();
         srcHigh = srcReg->getHighOrder();
         }
      TR::addDependency(dependencies, trgReg, TR::RealRegister::fp0, TR_FPR, cg);
      TR::addDependency(dependencies, srcHigh, TR::RealRegister::gr3, TR_GPR, cg);
      TR::addDependency(dependencies, srcLow, TR::RealRegister::gr4, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr5, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp1, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp2, TR_FPR, cg);

      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPClong2Double, node, dependencies, cg);

      dependencies->stopUsingDepRegs(cg, trgReg);

      cg->machine()->setLinkRegisterKilled(true);
      }

   cg->decReferenceCount(child);
   return trgReg;
}

TR::Register *OMR::Power::TreeEvaluator::long2float(TR::Node *node, TR::CodeGenerator *cg)
{
   TR::Node     *child      = node->getFirstChild();
   TR::Register *srcReg  = cg->evaluate(child);
   TR::Register *trgReg     = cg->allocateSinglePrecisionRegister(TR_FPR);

   if (cg->comp()->target().cpu.id() >= TR_PPCp7 &&
      (cg->is64BitProcessor() || (cg->comp()->compileRelocatableCode() && cg->comp()->target().is64Bit())))
      {
      if (cg->comp()->target().is64Bit())
         generateMvFprGprInstructions(cg, node, gpr2fprHost64, false, trgReg, srcReg);
      else
         {
         if (cg->comp()->target().cpu.id() >= TR_PPCp8)
            {
            TR::Register * tmp1 = cg->allocateRegister(TR_FPR);
            generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, trgReg, srcReg->getHighOrder(), srcReg->getLowOrder(), tmp1);
            cg->stopUsingRegister(tmp1);
            }
         else
            generateMvFprGprInstructions(cg, node, gpr2fprHost32, false, trgReg, srcReg->getHighOrder(), srcReg->getLowOrder());
         }
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfids, node, trgReg, trgReg);
      }
   else if (cg->comp()->target().is64Bit())
      {
      TR::Register *src;
      TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(8, 8, cg->trMemory());

      if (!cg->canClobberNodesRegister(child))
         {
         src = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, src, srcReg);
         }
      else
         {
         src = srcReg;
         }
      TR::addDependency(dependencies, trgReg, TR::RealRegister::fp0, TR_FPR, cg);
      TR::addDependency(dependencies, src, TR::RealRegister::gr3, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr4, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);

      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPClong2Float, node, dependencies, cg);

      dependencies->stopUsingDepRegs(cg, trgReg);

      cg->machine()->setLinkRegisterKilled(true);

      }
   else
      {
      TR::Register *srcLow, *srcHigh;
      TR::RegisterDependencyConditions *dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(10, 10, cg->trMemory());

      if (!cg->canClobberNodesRegister(child))
         {
         srcLow = cg->allocateRegister();
         srcHigh = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, srcHigh, srcReg->getHighOrder());
         generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, srcLow, srcReg->getLowOrder());
         }
      else
         {
         srcLow = srcReg->getLowOrder();
         srcHigh = srcReg->getHighOrder();
         }
      TR::addDependency(dependencies, trgReg, TR::RealRegister::fp0, TR_FPR, cg);
      TR::addDependency(dependencies, srcHigh, TR::RealRegister::gr3, TR_GPR, cg);
      TR::addDependency(dependencies, srcLow, TR::RealRegister::gr4, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr5, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr6, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr7, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp1, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp2, TR_FPR, cg);

      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPClong2Float, node, dependencies, cg);

      dependencies->stopUsingDepRegs(cg, trgReg);

      cg->machine()->setLinkRegisterKilled(true);
   }

   cg->decReferenceCount(child);
   return trgReg;
}

TR::Register *OMR::Power::TreeEvaluator::l2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;
   TR::Node     *child      = node->getFirstChild();
   if (cg->comp()->target().cpu.id() >= TR_PPCp7 &&
       node->getOpCodeValue() == TR::l2f &&
       (child->getOpCodeValue() == TR::lload || child->getOpCodeValue() == TR::lloadi) &&
       child->getReferenceCount()==1 &&
       child->getRegister() == NULL &&
       !(child->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP()))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      tempMR->forceIndexedForm(node, cg);
      TR::Register *tempReg = cg->allocateRegister(TR_FPR); // Double
      trgReg = cg->allocateSinglePrecisionRegister(TR_FPR); // Single
      generateTrg1MemInstruction(cg, TR::InstOpCode::lfdx, node, tempReg, tempMR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfids, node, trgReg, tempReg);
      tempMR->decNodeReferenceCounts(cg);
      cg->stopUsingRegister(tempReg);
      }
   else
      {
      trgReg = TR::TreeEvaluator::long2float(node, cg);
      }
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::l2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg;
   TR::Node     *child      = node->getFirstChild();
   if (
       cg->comp()->target().cpu.id() >= TR_PPCp7 &&
       node->getOpCodeValue() == TR::l2d &&
       (child->getOpCodeValue() == TR::lload || child->getOpCodeValue() == TR::lloadi) &&
       child->getReferenceCount()==1 &&
       child->getRegister() == NULL &&
       !(child->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP()))
      {
         TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
         tempMR->forceIndexedForm(node, cg);
         trgReg = cg->allocateRegister(TR_FPR); // Double
         generateTrg1MemInstruction(cg, TR::InstOpCode::lfdx, node, trgReg, tempMR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, trgReg, trgReg);
         tempMR->decNodeReferenceCounts(cg);
      }
   else
      {
      trgReg = TR::TreeEvaluator::long2dbl(node, cg);
      }
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::f2dEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *firstChild = node->getFirstChild();
   TR::Register  *resReg = cg->evaluate(firstChild);

   if (firstChild->getReferenceCount()>1 && cg->useClobberEvaluate())
      {
      TR::Register *tempReg = cg->allocateRegister(TR_FPR);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, tempReg, resReg);
      resReg = tempReg;
      }

   node->setRegister(resReg);
   cg->decReferenceCount(firstChild);
   return node->getRegister();
   }

// also handles f2b, f2s, f2c, f2i, d2b, d2s, d2c
//
TR::Register *OMR::Power::TreeEvaluator::d2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::ILOpCodes opcode     = node->getOpCodeValue();

   TR::Node     *child      = node->getFirstChild();
   TR::Register *sourceReg  = cg->evaluate(child);
   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   TR::Register *trgReg     = cg->allocateRegister();
   TR::Register *tempReg = (opcode == TR::f2b || opcode == TR::f2s  || opcode == TR::f2c || opcode == TR::f2i)
                          ? cg->allocateSinglePrecisionRegister() : cg->allocateRegister(TR_FPR);

   // Eventually the following test should be whether there is a register allocator that
   // can handle registers being alive across basic block boundaries.
   // For now we just generate pessimistic code.
   if (true)
      {
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
         generateControlFlowInstruction(cg, TR::InstOpCode::d2i, node);
      cfop->addTargetRegister(condReg);
      cfop->addTargetRegister(tempReg);
      cfop->addTargetRegister(trgReg);
      cfop->addSourceRegister(sourceReg);
      }
   else
      {
      TR::LabelSymbol         *doneLabel = generateLabelSymbol(cg);

      generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, sourceReg, sourceReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bne, node, doneLabel, condReg);
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fctiwz, node, tempReg, sourceReg);
      generateMvFprGprInstructions(cg, node, fpr2gprLow, false, trgReg, tempReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
      }

   if (opcode == TR::f2s || opcode == TR::d2s) generateTrg1Src1Instruction(cg, TR::InstOpCode::extsh, node, trgReg, trgReg);
   if (opcode == TR::f2c || opcode == TR::d2c) generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 0, 0xffff);

   cg->decReferenceCount(child);

   cg->stopUsingRegister(condReg);
   cg->stopUsingRegister(tempReg);

   node->setRegister(trgReg);
   return trgReg;
   }

// also handles f2bu, f2c, f2iu, d2bu, d2c
TR::Register *OMR::Power::TreeEvaluator::d2iuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Node     *child      = node->getFirstChild();
   TR::Register *sourceReg  = cg->evaluate(child);
   TR::Register *trgReg     = cg->allocateRegister();
   TR::ILOpCodes  opcode    = node->getOpCodeValue();
   TR::Register *tempReg0, *tempReg1, *tempReg2, *tempReg3;

   if (opcode == TR::f2bu || opcode == TR::f2c || opcode == TR::f2iu)
      {
      tempReg0 = cg->allocateSinglePrecisionRegister();
      tempReg1 = cg->allocateSinglePrecisionRegister();
      tempReg2 = cg->allocateSinglePrecisionRegister();
      tempReg3 = cg->allocateSinglePrecisionRegister();
      }
   else
      {
      tempReg0 = cg->allocateRegister(TR_FPR);
      tempReg1 = cg->allocateRegister(TR_FPR);
      tempReg2 = cg->allocateRegister(TR_FPR);
      tempReg3 = cg->allocateRegister(TR_FPR);
      }

      {
      TR_ASSERT(false, "32 bit version of d2iu is not implemented yet\n");
      }

   if (opcode == TR::f2bu || opcode == TR::d2bu) generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 0, 0xff);
   if (opcode == TR::f2c || opcode == TR::d2c) generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, trgReg, trgReg, 0, 0xffff);

   return trgReg;
   }

// also handles f2l
TR::Register *OMR::Power::TreeEvaluator::d2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child      = node->getFirstChild();
   TR::Register *sourceReg  = cg->evaluate(child);
   TR::Register *trgReg;

   if (cg->comp()->target().is64Bit())
      trgReg     = cg->allocateRegister();
   else
      trgReg     = cg->allocateRegisterPair(cg->allocateRegister(),
                                                 cg->allocateRegister());

   if (cg->is64BitProcessor() || (cg->comp()->compileRelocatableCode() && cg->comp()->target().is64Bit()))
      {
      TR::Register *condReg = cg->allocateRegister(TR_CCR);
      TR::Register *tempReg = (node->getOpCodeValue() == TR::f2l) ? cg->allocateSinglePrecisionRegister() :  cg->allocateRegister(TR_FPR);
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
         generateControlFlowInstruction(cg, TR::InstOpCode::d2l, node);
      cfop->addTargetRegister(condReg);
      cfop->addTargetRegister(tempReg);
      if (cg->comp()->target().is64Bit())
         cfop->addTargetRegister(trgReg);
      else
         {
         cfop->addTargetRegister(trgReg->getHighOrder());
         cfop->addTargetRegister(trgReg->getLowOrder());
    }
      cfop->addSourceRegister(sourceReg);
      cg->stopUsingRegister(condReg);
      cg->stopUsingRegister(tempReg);
      }
   else
      {
      TR::Register *copyReg;
      TR::RegisterDependencyConditions *dependencies;

      if (!cg->canClobberNodesRegister(child))
         {
         if (node->getOpCodeValue() == TR::f2l)
            copyReg = cg->allocateSinglePrecisionRegister();
         else
            copyReg = cg->allocateRegister(TR_FPR);
         generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, copyReg, sourceReg);
         sourceReg = copyReg;
         }

      dependencies = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(17, 17, cg->trMemory());
      TR::addDependency(dependencies, sourceReg, TR::RealRegister::fp0, TR_FPR, cg);
      TR::addDependency(dependencies, trgReg->getHighOrder(), TR::RealRegister::gr3, TR_GPR, cg);
      TR::addDependency(dependencies, trgReg->getLowOrder(), TR::RealRegister::gr4, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr5, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr6, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::gr11, TR_GPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::cr0, TR_CCR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::cr1, TR_CCR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::cr6, TR_CCR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp1, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp2, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp3, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp4, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp5, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp6, TR_FPR, cg);
      TR::addDependency(dependencies, NULL, TR::RealRegister::fp7, TR_FPR, cg);

      TR::TreeEvaluator::generateHelperBranchAndLinkInstruction(TR_PPCdouble2Long, node, dependencies, cg);

      dependencies->stopUsingDepRegs(cg, trgReg->getHighOrder(), trgReg->getLowOrder());
      cg->machine()->setLinkRegisterKilled(true);
      }

   cg->decReferenceCount(child);
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::d2fEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg        = cg->allocateSinglePrecisionRegister();
   TR::Node *child = node->getFirstChild();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::frsp, node, trgReg, cg->evaluate(child));
   cg->decReferenceCount(child);
   node->setRegister(trgReg);
   return trgReg;
   }

// also handles TR::iffcmpeq
TR::Register *OMR::Power::TreeEvaluator::ifdcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::beq, TR::InstOpCode::bad, cg);
   return NULL;
   }

// also handles TR::iffcmpeq
TR::Register *OMR::Power::TreeEvaluator::ifdcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::bne, TR::InstOpCode::bad, cg);
   return NULL;
   }

// also handles TR::iffcmpeq
TR::Register *OMR::Power::TreeEvaluator::ifdcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::blt, TR::InstOpCode::bad, cg);
   return NULL;
   }

// also handles TR::iffcmpeq
TR::Register *OMR::Power::TreeEvaluator::ifdcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::bgt, TR::InstOpCode::beq, cg);
   return NULL;
   }

// also handles TR::iffcmpgt
TR::Register *OMR::Power::TreeEvaluator::ifdcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::bgt, TR::InstOpCode::bad, cg);
   return NULL;
   }

// also handles TR::iffcmple
TR::Register *OMR::Power::TreeEvaluator::ifdcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::blt, TR::InstOpCode::beq, cg);
   return NULL;
   }

// also handles TR::iffcmpequ
TR::Register *OMR::Power::TreeEvaluator::ifdcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::beq, TR::InstOpCode::bun, cg);
   return NULL;
   }

// also handles TR::iffcmpltu
TR::Register *OMR::Power::TreeEvaluator::ifdcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::blt, TR::InstOpCode::bun, cg);
   return NULL;
   }

// also handles TR::iffcmpgeu
TR::Register *OMR::Power::TreeEvaluator::ifdcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TR::InstOpCode::bge is really not less than which is equivalent to greater than equal to or unordered
   ifFloatEvaluator(node, TR::InstOpCode::bge, TR::InstOpCode::bad, cg);
   return NULL;
   }

// also handles TR::iffcmpgtu
TR::Register *OMR::Power::TreeEvaluator::ifdcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   ifFloatEvaluator(node, TR::InstOpCode::bgt, TR::InstOpCode::bun, cg);
   return NULL;
   }

// also handles TR::iffcmpleu
TR::Register *OMR::Power::TreeEvaluator::ifdcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TR::InstOpCode::ble is really not greater than which is equivalent to less than equal to or unordered
   ifFloatEvaluator(node, TR::InstOpCode::ble, TR::InstOpCode::bad, cg);
   return NULL;
   }

// also handles TR::fcmpeq
TR::Register *OMR::Power::TreeEvaluator::dcmpeqEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::beq, TR::InstOpCode::bad, 0, node, cg);
   }

// also handles TR::fcmpne
TR::Register *OMR::Power::TreeEvaluator::dcmpneEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::bne, TR::InstOpCode::bad, 0, node, cg);
   }

// also handles TR::fcmplt
TR::Register *OMR::Power::TreeEvaluator::dcmpltEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int64_t imm = 0;
   if (cg->comp()->target().cpu.id() >= TR_PPCp9)
      {
      imm = (TR::RealRegister::CRCC_GT <<  TR::RealRegister::pos_RT | TR::RealRegister::CRCC_LT <<  TR::RealRegister::pos_RA | TR::RealRegister::CRCC_LT << TR::RealRegister::pos_RB);
      }
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::blt, TR::InstOpCode::bad, imm, node, cg);
   }

// also handles TR::fcmpge
TR::Register *OMR::Power::TreeEvaluator::dcmpgeEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::bgt, TR::InstOpCode::beq, 0, node, cg);
   }

// also handles TR::fcmpgt
TR::Register *OMR::Power::TreeEvaluator::dcmpgtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int64_t imm = 0;
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::bgt, TR::InstOpCode::bad, imm, node, cg);
   }

// also handles TR::fcmple
TR::Register *OMR::Power::TreeEvaluator::dcmpleEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::blt, TR::InstOpCode::beq, 0, node, cg);
   }

// also handles TR::fcmpequ
TR::Register *OMR::Power::TreeEvaluator::dcmpequEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::beq, TR::InstOpCode::bun, 0, node, cg);
   }

// also handles TR::fcmpltu
TR::Register *OMR::Power::TreeEvaluator::dcmpltuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::blt, TR::InstOpCode::bun, 0, node, cg);
   }

// also handles TR::fcmpgeu
TR::Register *OMR::Power::TreeEvaluator::dcmpgeuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::bge, TR::InstOpCode::bad, 0, node, cg);
   }

// also handles TR::fcmpgtu
TR::Register *OMR::Power::TreeEvaluator::dcmpgtuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::bgt, TR::InstOpCode::bun, 0, node, cg);
   }

// also handles TR::fcmpleu
TR::Register *OMR::Power::TreeEvaluator::dcmpleuEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TR::InstOpCode::ble is really not greater than which is equivalent to less than equal to or unordered
   return compareFloatAndSetOrderedBoolean(TR::InstOpCode::ble, TR::InstOpCode::bad, 0, node, cg);
   }

// also handles TR::fcmpl
TR::Register *OMR::Power::TreeEvaluator::dcmplEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *src2Reg      = cg->evaluate(secondChild);
   TR::Register *trgReg    = cg->allocateRegister();
   TR::Register    *condReg = cg->allocateRegister(TR_CCR);

   // Eventually the following test should be whether there is a register allocator that
   // can handle registers being alive across basic block boundaries.
   // For now we just generate pessimistic code.
   if (true)
      {
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
         generateControlFlowInstruction(cg, TR::InstOpCode::flcmpl, node);
      cfop->addTargetRegister(condReg);
      cfop->addTargetRegister(trgReg);
      cfop->addSourceRegister(src1Reg);
      cfop->addSourceRegister(src2Reg);
      }
   else
      {
      TR::LabelSymbol *doneLabel         = generateLabelSymbol(cg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, src1Reg, src2Reg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bgt, node, doneLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, -1);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(condReg);
   return trgReg;
   }

// also handles TR::fcmpg
TR::Register *OMR::Power::TreeEvaluator::dcmpgEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *src2Reg      = cg->evaluate(secondChild);
   TR::Register *trgReg    = cg->allocateRegister();
   TR::Register    *condReg = cg->allocateRegister(TR_CCR);

   // Eventually the following test should be whether there is a register allocator that
   // can handle registers being alive across basic block boundaries.
   // For now we just generate pessimistic code.
   if (true)
      {
      TR::PPCControlFlowInstruction *cfop = (TR::PPCControlFlowInstruction *)
         generateControlFlowInstruction(cg, TR::InstOpCode::flcmpg, node);
      cfop->addTargetRegister(condReg);
      cfop->addTargetRegister(trgReg);
      cfop->addSourceRegister(src1Reg);
      cfop->addSourceRegister(src2Reg);
      }
   else
      {
      TR::LabelSymbol *doneLabel         = generateLabelSymbol(cg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, src1Reg, src2Reg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, -1);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, doneLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, doneLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(condReg);
   return trgReg;
   }

static TR::Register *singlePrecisionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic OpCode, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild = node->getFirstChild();
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Register *source1Register = cg->evaluate(firstChild);
   TR::Register *source2Register = cg->evaluate(secondChild);
   TR::Register *trgRegister  = cg->allocateSinglePrecisionRegister();
   generateTrg1Src2Instruction(cg, OpCode, node, trgRegister, source1Register, source2Register);
   node->setRegister(trgRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgRegister;
   }

static TR::Register *doublePrecisionEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic OpCode, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild = node->getFirstChild();
   TR::Node     *secondChild    = node->getSecondChild();
   TR::Register *source1Register = cg->evaluate(firstChild);
   TR::Register *source2Register = cg->evaluate(secondChild);
   TR::Register *trgRegister  = cg->allocateRegister(TR_FPR);
   generateTrg1Src2Instruction(cg, OpCode, node, trgRegister, source1Register, source2Register);
   node->setRegister(trgRegister);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgRegister;
   }


static void ifFloatEvaluator( TR::Node       *node,
                              TR::InstOpCode::Mnemonic branchOp1,
                              TR::InstOpCode::Mnemonic branchOp2,
                              TR::CodeGenerator *cg)
   {
   TR::Register *conditionRegister = cg->allocateRegister(TR_CCR);
   TR::Node     *firstChild = node->getFirstChild();
   TR::Node     *secondChild = node->getSecondChild();
   TR::Register *src1Register   = cg->evaluate(firstChild);
   TR::Register *src2Register   = cg->evaluate(secondChild);
   TR::LabelSymbol *label = node->getBranchDestination()->getNode()->getLabel();
   TR::RegisterDependencyConditions *deps = NULL;

   generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, conditionRegister, src1Register, src2Register);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   if (node->getNumChildren() == 3)
      {
      TR::Node *thirdChild = node->getChild(2);
      cg->evaluate(thirdChild);
      deps = generateRegisterDependencyConditions(cg, thirdChild, 0);
      cg->decReferenceCount(thirdChild);
      }

   if (branchOp2 == TR::InstOpCode::bad && deps != NULL)
      {
      generateDepConditionalBranchInstruction(cg, branchOp1, node, label,
         conditionRegister, deps);

      }
   else
      {
      generateConditionalBranchInstruction(cg, branchOp1, node,label, conditionRegister);
      }

   if (branchOp2 != TR::InstOpCode::bad)
      {
      if (deps == NULL)
    {
         generateConditionalBranchInstruction(cg, branchOp2, node, label,
            conditionRegister);
    }
      else
    {
         generateDepConditionalBranchInstruction(cg, branchOp2, node, label,
            conditionRegister, deps);
    }
      }

   cg->stopUsingRegister(conditionRegister);
   }


static TR::Register *compareFloatAndSetOrderedBoolean(TR::InstOpCode::Mnemonic branchOp1, TR::InstOpCode::Mnemonic branchOp2, int64_t imm, TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild   = node->getFirstChild();
   TR::Node     *secondChild  = node->getSecondChild();
   TR::Register *src1Reg      = cg->evaluate(firstChild);
   TR::Register *src2Reg      = cg->evaluate(secondChild);
   TR::Register *trgReg       = cg->allocateRegister();
   TR::Register *condReg      = cg->allocateRegister(TR_CCR);

   // Eventually the following test should be whether there is a register allocator that
   // can handle registers being alive across basic block boundaries.
   // For now we just generate pessimistic code.
   if (true)
      {
      TR::PPCControlFlowInstruction *cfop;
      if (branchOp2 == TR::InstOpCode::bad)
         {
         cfop = (TR::PPCControlFlowInstruction *)
                generateControlFlowInstruction(cg, TR::InstOpCode::setbool, node);
         }
      else
         {
         cfop = (TR::PPCControlFlowInstruction *)
                generateControlFlowInstruction(cg, TR::InstOpCode::setbflt, node);
         cfop->setOpCode3Value(branchOp2);
         }
      cfop->addTargetRegister(condReg);
      cfop->addTargetRegister(trgReg);
      cfop->addSourceRegister(src1Reg);
      cfop->addSourceRegister(src2Reg);
      if (cg->comp()->target().cpu.id() >= TR_PPCp9 && branchOp2 == TR::InstOpCode::bad && imm != 0)
         {
         cfop->addSourceImmediate(imm);
         }
      cfop->setOpCode2Value(branchOp1);
      cfop->setCmpOpValue(TR::InstOpCode::fcmpu);
      }
   else
      {
      TR::LabelSymbol *doneLabel         = generateLabelSymbol(cg);
      generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, src1Reg, src2Reg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 1);
      generateConditionalBranchInstruction(cg, branchOp1, node, doneLabel, condReg);
      if (branchOp2 != TR::InstOpCode::bad)
         generateConditionalBranchInstruction(cg, branchOp2, node, doneLabel, condReg);
      generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, trgReg, 0);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel);
      }
   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   cg->stopUsingRegister(condReg);
   return trgReg;
   }


TR::Register *OMR::Power::TreeEvaluator::fRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateSinglePrecisionRegister();
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

TR::Register *OMR::Power::TreeEvaluator::dRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister(TR_FPR);
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

TR::Register *OMR::Power::TreeEvaluator::vRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      if (node->getOpCode().getOpCodeValue() == TR::vbRegLoad ||
          node->getOpCode().getOpCodeValue() == TR::vsRegLoad ||
          node->getOpCode().getOpCodeValue() == TR::viRegLoad ||
          node->getOpCode().getOpCodeValue() == TR::vlRegLoad)
         globalReg = cg->allocateRegister(TR_VRF);
      else if (node->getOpCode().getOpCodeValue() == TR::vfRegLoad ||
               node->getOpCode().getOpCodeValue() == TR::vdRegLoad)
         globalReg = cg->allocateRegister(TR_VSX_VECTOR);
      else
         TR_ASSERT(0, "unknown operation.\n");
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

// Also handles TR::dRegStore
TR::Register *OMR::Power::TreeEvaluator::fRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *OMR::Power::TreeEvaluator::iexpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

TR::Register *OMR::Power::TreeEvaluator::lexpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }


// also handles fexp
TR::Register *OMR::Power::TreeEvaluator::dexpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return 0;
   }

// also handles fxfrs
TR::Register *OMR::Power::TreeEvaluator::dxfrsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Node * secondChild = node->getSecondChild();
   TR::Register *reg1 = cg->evaluate(firstChild);
   TR::Register *reg2 = cg->evaluate(secondChild);

   TR::Node *tmp_node = TR::Node::create(node, TR::dconst, 0);
   tmp_node->setDouble(0.0);
   TR::Register *tmpReg = TR::TreeEvaluator::dconstEvaluator(tmp_node, cg);
   tmp_node->unsetRegister();

   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, tmpReg, reg2);
   cg->stopUsingRegister(tmpReg);

   TR::Register *trgReg = (node->getOpCodeValue() == TR::dxfrs) ?
                           cg->allocateRegister(TR_FPR)
                         : cg->allocateSinglePrecisionRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fabs, node, trgReg, reg1);
   TR::LabelSymbol    *doneLabel = generateLabelSymbol(cg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, doneLabel, condReg);
   cg->stopUsingRegister(condReg);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fnabs, node, trgReg, reg1);

   TR::RegisterDependencyConditions *dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 2, cg->trMemory());
   dep->addPostCondition(trgReg, TR::RealRegister::NoReg);
   dep->addPostCondition(reg1, TR::RealRegister::NoReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, dep);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);
   return trgReg;
   }

// also handles fint
TR::Register *OMR::Power::TreeEvaluator::dintEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);

   TR::Register *trgReg = (node->getOpCodeValue() == TR::dint) ?
                           cg->allocateRegister(TR_FPR)
                         : cg->allocateSinglePrecisionRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fctidz, node, trgReg, srcReg);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, trgReg, trgReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   return trgReg;
   }

// also handles fnint
TR::Register *OMR::Power::TreeEvaluator::dnintEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *trgReg = srcReg;
   bool dnint = node->getOpCodeValue() == TR::dnint;

   if (!cg->canClobberNodesRegister(firstChild))
      {
      trgReg = dnint ? cg->allocateRegister(TR_FPR) : cg->allocateSinglePrecisionRegister();
      }


   TR::Register *tmp1Reg;
   TR::Node *const_node;
   if (dnint)
      {
      const_node = TR::Node::create(node, TR::dconst, 0);
      const_node->setDouble(CONSTANT64(0x10000000000000));   // 2**52
      tmp1Reg = TR::TreeEvaluator::dconstEvaluator(const_node, cg);
      }
   else
      {
      const_node = TR::Node::create(node, TR::fconst, 0);
      const_node->setFloat(0x800000);   // 2**23
      tmp1Reg = TR::TreeEvaluator::fconstEvaluator(const_node, cg);
      }

   const_node->unsetRegister();
   TR::Register *tmp2Reg = dnint ? cg->allocateRegister(TR_FPR) : cg->allocateSinglePrecisionRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fabs, node, tmp2Reg, srcReg);

   TR::Register *condReg = cg->allocateRegister(TR_CCR);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::fcmpu, node, condReg, tmp1Reg, tmp2Reg);

   TR::LabelSymbol    *moveLabel = generateLabelSymbol(cg);
   generateConditionalBranchInstruction(cg, TR::InstOpCode::ble, node, moveLabel, condReg);
   cg->stopUsingRegister(condReg);
   cg->stopUsingRegister(tmp1Reg);
   cg->stopUsingRegister(tmp2Reg);

   const_node = TR::Node::create(node, TR::fconst, 0);
   const_node->setFloat(0.5);
   tmp1Reg = TR::TreeEvaluator::fconstEvaluator(const_node, cg);
   const_node->unsetRegister();

   const_node = TR::Node::create(node, TR::fconst, 0);
   const_node->setFloat(-0.5);
   tmp2Reg = TR::TreeEvaluator::fconstEvaluator(const_node, cg);
   const_node->unsetRegister();

   generateTrg1Src2Instruction(cg, TR::InstOpCode::fadd, node, tmp1Reg, srcReg, tmp1Reg);
   generateTrg1Src2Instruction(cg, TR::InstOpCode::fadd, node, tmp2Reg, srcReg, tmp2Reg);
   generateTrg1Src3Instruction(cg, TR::InstOpCode::fsel, node, trgReg, srcReg, tmp1Reg, tmp2Reg);
   cg->stopUsingRegister(tmp1Reg);
   cg->stopUsingRegister(tmp2Reg);

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fctidz, node, trgReg, trgReg);
   generateTrg1Src1Instruction(cg, TR::InstOpCode::fcfid, node, trgReg, trgReg);

   TR::LabelSymbol    *doneLabel = generateLabelSymbol(cg);

   if (!cg->canClobberNodesRegister(firstChild))
      {
      generateLabelInstruction(cg, TR::InstOpCode::b, node, doneLabel);
      }
   generateLabelInstruction(cg, TR::InstOpCode::label, node, moveLabel);
   if (!cg->canClobberNodesRegister(firstChild))
      {
      generateTrg1Src1Instruction(cg, TR::InstOpCode::fmr, node, trgReg, srcReg);
      }

   TR::RegisterDependencyConditions *dep = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(0, 5, cg->trMemory());
   dep->addPostCondition(trgReg, TR::RealRegister::NoReg);
   dep->addPostCondition(srcReg, TR::RealRegister::NoReg);
   dep->addPostCondition(tmp1Reg, TR::RealRegister::NoReg);
   dep->addPostCondition(tmp2Reg, TR::RealRegister::NoReg);
   dep->addPostCondition(condReg, TR::RealRegister::NoReg);

   generateDepLabelInstruction(cg, TR::InstOpCode::label, node, doneLabel, dep);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::fsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);

   TR::Register *trgReg = cg->allocateSinglePrecisionRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fsqrts, node, trgReg, srcReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::dsqrtEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);

   TR::Register *trgReg = cg->allocateRegister(TR_FPR);

   generateTrg1Src1Instruction(cg, TR::InstOpCode::fsqrt, node, trgReg, srcReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::getstackEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   const TR::PPCLinkageProperties &properties = cg->getProperties();

   TR::Register *spReg = cg->machine()->getRealRegister(properties.getNormalStackPointerRegister());
   TR::Register *trgReg = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, trgReg, spReg);

   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::Power::TreeEvaluator::deallocaEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node * firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   const TR::PPCLinkageProperties &properties = cg->getProperties();

   // TODO: restore stack chain
   TR::Register *spReg = cg->machine()->getRealRegister(properties.getNormalStackPointerRegister());

   generateTrg1Src1Instruction(cg, TR::InstOpCode::mr, node, spReg, srcReg);

   node->setRegister(NULL);
   cg->decReferenceCount(firstChild);
   return NULL;
   }

TR::Register *OMR::Power::TreeEvaluator::xfRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateFloatingPointRegisterPair(cg->allocateSinglePrecisionRegister(),
                                                        cg->allocateSinglePrecisionRegister());
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

TR::Register *OMR::Power::TreeEvaluator::xdRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateFloatingPointRegisterPair(cg->allocateRegister(TR_FPR),
                                                        cg->allocateRegister(TR_FPR));
      node->setRegister(globalReg);
      }
   return(globalReg);
   }
