/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#include "arm/codegen/ARMInstruction.hpp"
#include "arm/codegen/ARMOperand2.hpp"
#include "codegen/AheadOfTimeCompile.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/ARMAOTRelocation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Bit.hpp"

TR::Register *OMR::ARM::TreeEvaluator::iconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getInt(), cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::sconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getShortInt(), cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::bconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getByte(), cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::cconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonConstEvaluator(node, node->getConst<uint16_t>(), cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::commonConstEvaluator(TR::Node *node, int32_t value, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = node->setRegister(cg->allocateRegister());
   armLoadConstant(node, value, tempReg, cg);
   return tempReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::aconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   bool isClass = node->isClassPointerConstant();
   TR_ResolvedMethod * method = comp->getCurrentMethod();

   bool isPicSite = node->isClassPointerConstant() && cg->fe()->isUnloadAssumptionRequired((TR_OpaqueClassBlock *) node->getAddress(), method);
   if (!isPicSite)
      isPicSite = node->isMethodPointerConstant() && cg->fe()->isUnloadAssumptionRequired(cg->fe()->createResolvedMethod(cg->trMemory(), (TR_OpaqueMethodBlock *) node->getAddress(), method)->classOfMethod(), method);

   bool isProfiledPointerConstant = node->isClassPointerConstant() || node->isMethodPointerConstant();

   // use data snippet only on class pointers when HCR is enabled
   int32_t address = node->getInt();
   if (isClass && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)address, node) ||
       isProfiledPointerConstant && cg->profiledPointersRequireRelocation())
      {
      TR::Register *trgReg = cg->allocateRegister();
      loadAddressConstantInSnippet(cg, node, address, trgReg, isPicSite, NULL);
      node->setRegister(trgReg);
      return trgReg;
      }
   else
      {
      TR::Register *tempReg = node->setRegister(cg->allocateRegister());
      TR::Instruction *cursor = armLoadConstant(node, address, tempReg, cg);
      if (isPicSite)
         {
         if (node->isClassPointerConstant())
            comp->getStaticPICSites()->push_front(cursor);
         else if (node->isMethodPointerConstant())
            comp->getStaticMethodPICSites()->push_front(cursor);
         }

      return tempReg;
      }
   }

TR::Register *OMR::ARM::TreeEvaluator::lconstEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register     *lowReg    = cg->allocateRegister();
   TR::Register     *highReg   = cg->allocateRegister();
   TR::RegisterPair *trgReg   = cg->allocateRegisterPair(lowReg, highReg);

   node->setRegister(trgReg);

   int32_t lowValue  = node->getLongIntLow();
   int32_t highValue = node->getLongIntHigh();
   int32_t difference;
   uint32_t base, rotate;

   difference = highValue - lowValue;
   if (constantIsImmed8r(difference, &base, &rotate))
      {
      armLoadConstant(node, lowValue, lowReg, cg);
      generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, highReg, lowReg, base, rotate);
      }
   else
      {
      armLoadConstant(node, lowValue, lowReg, cg);
      armLoadConstant(node, highValue, highReg, cg);
      }

   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::inegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg  = cg->allocateRegister();
   trgReg = cg->allocateRegister(TR_GPR);
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *sourceRegister = cg->evaluate(firstChild);
   generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, trgReg, sourceRegister, 0, 0);
   firstChild->decReferenceCount();
   return node->setRegister(trgReg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lnegEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *firstChild     = node->getFirstChild();
   TR::Register *lowReg  = cg->allocateRegister();
   TR::Register *highReg = cg->allocateRegister();
   TR::RegisterPair *trgReg = cg->allocateRegisterPair(lowReg, highReg);
   TR::Register *sourceRegister    = cg->evaluate(firstChild);
   generateTrg1Src1ImmInstruction(cg, ARMOp_rsb_r, node, lowReg, sourceRegister->getLowOrder(), 0, 0);
   generateTrg1Src1ImmInstruction(cg, ARMOp_rsc, node, highReg, sourceRegister->getHighOrder(), 0, 0);
   node->setRegister(trgReg);
   firstChild->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::iabsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *trgReg = cg->allocateRegister();
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Instruction *instr1, *instr2;

   generateSrc1ImmInstruction(cg, ARMOp_cmp, node, srcReg, 0, 0);
   instr1 = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, srcReg);
   instr1->setConditionCode(ARMConditionCodeGE);
   instr2 = generateTrg1Src1ImmInstruction(cg, ARMOp_rsb, node, trgReg, srcReg, 0, 0);
   instr2->setConditionCode(ARMConditionCodeLT);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::labsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *lowReg = cg->allocateRegister();
   TR::Register *highReg = cg->allocateRegister();
   TR::Register *trgReg = cg->allocateRegisterPair(lowReg, highReg);
   TR::Node *firstChild = node->getFirstChild();
   TR::Register *srcReg = cg->evaluate(firstChild);
   TR::Register *tmpReg = cg->allocateRegister();

   generateTrg1Src1Instruction(cg, ARMOp_mov, node, tmpReg, new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegASRImmed, srcReg->getHighOrder(), 31));
   generateTrg1Src2Instruction(cg, ARMOp_eor, node, lowReg, srcReg->getLowOrder(), tmpReg);
   generateTrg1Src2Instruction(cg, ARMOp_eor, node, highReg, srcReg->getHighOrder(), tmpReg);
   generateTrg1Src2Instruction(cg, ARMOp_sub_r, node, lowReg, lowReg, tmpReg);
   generateTrg1Src2Instruction(cg, ARMOp_sbc, node, highReg, highReg, tmpReg);
   cg->stopUsingRegister(tmpReg);

   node->setRegister(trgReg);
   cg->decReferenceCount(firstChild);
   return trgReg;
   }

//also handles i2s,l2s
TR::Register *OMR::ARM::TreeEvaluator::i2sEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, ARMOp_ldrsh, true, 16, cg);
   }

//also handles su2i
TR::Register *OMR::ARM::TreeEvaluator::i2cEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, ARMOp_ldrh, false, 16, cg);
   }

//also handles l2b, s2b
TR::Register *OMR::ARM::TreeEvaluator::i2bEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return conversionAnalyser(node, ARMOp_ldrsb, true, 8, cg);
   }


// also handles b2s, c2i, s2i
TR::Register *OMR::ARM::TreeEvaluator::b2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *trgReg = cg->gprClobberEvaluate(child);
   child->decReferenceCount();
   return node->setRegister(trgReg);
   }

//also handles s2l, i2l
TR::Register *OMR::ARM::TreeEvaluator::b2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child),
                                                       cg->allocateRegister());
   TR_ARMOperand2 *op;

   // TODO: check
   op = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegASRImmed, trgReg->getLowOrder(), 31);
   generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), op);
   node->setRegister(trgReg);
   child->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::c2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child),
                                                             cg->allocateRegister());
   generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), 0, 0);
   node->setRegister(trgReg);
   return trgReg;
   }

//also handles l2a, lu2a
TR::Register *OMR::ARM::TreeEvaluator::l2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node     *child = node->getFirstChild();
   TR::Register *trgReg;
   TR::Register *temp;

   temp = child->getRegister();
   if (child->getReferenceCount() == 1 &&
       child->getOpCode().isMemoryReference() && (temp == NULL))
      {
      trgReg = cg->allocateRegister();
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 4, cg);
      if (TR::Compiler->target.cpu.isBigEndian())
         tempMR->addToOffset(node, 4, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      temp = cg->evaluate(child);
      if (child->getReferenceCount() == 1 || !cg->useClobberEvaluate())
         {
         trgReg = temp->getLowOrder();
         }
      else
         {
         trgReg = cg->allocateRegister();
         generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, temp->getLowOrder());
         child->decReferenceCount();
         }
      }
   node->setRegister(trgReg);
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::iu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child), cg->allocateRegister());
   generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), 0, 0);
   node->setRegister(trgReg);
   child->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::su2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child), cg->allocateRegister());
   TR::Register *temp;
   temp = child->getRegister();
   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (temp == NULL))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(temp, 2, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldrh, node, trgReg->getLowOrder(), tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, cg->evaluate(child), 16));
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg->getLowOrder(), new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSRImmed, trgReg->getLowOrder(), 16));
      }
   generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), 0, 0);

   node->setRegister(trgReg);
   child->decReferenceCount();
   return trgReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::bu2iEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegister();
   TR::Register *temp;

   temp = child->getRegister();
   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (temp == NULL))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, 1, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldrb, node, trgReg, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      child->decReferenceCount();
      generateTrg1Src1ImmInstruction(cg, ARMOp_and, node, trgReg, cg->evaluate(child), 0xff, 0);
      }

   return node->setRegister(trgReg);
   }

TR::Register *OMR::ARM::TreeEvaluator::bu2lEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child  = node->getFirstChild();
   TR::Register *sourceRegister = NULL;
   TR::Register *trgReg = cg->allocateRegisterPair(cg->gprClobberEvaluate(child), cg->allocateRegister());
   TR::Register *temp;
   temp = child->getRegister();
   if (child->getReferenceCount()==1 &&
       child->getOpCode().isMemoryReference() && (temp == NULL))
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(temp, 1, cg);
      generateTrg1MemInstruction(cg, ARMOp_ldrb, node, trgReg->getLowOrder(), tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      generateTrg1Src1ImmInstruction(cg, ARMOp_and, node, trgReg->getLowOrder(), cg->evaluate(child), 0xff, 0);
      }
   generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg->getHighOrder(), 0, 0);

   node->setRegister(trgReg);
   child->decReferenceCount();
   return trgReg;
   }
