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

#include "compile/Compilation.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Machine.hpp"
#include "codegen/ARMInstruction.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "infra/Bit.hpp"
#include "arm/codegen/GenerateInstructions.hpp"

#include "codegen/ARMOutOfLineCodeSection.hpp"

bool constantIsImmed8r(int32_t intValue, uint32_t *base, uint32_t *rotate)
   {
   uint32_t trailing;
   uint32_t bitPattern = (uint32_t) intValue;

   // special case 0
   if(0 == intValue)
      {
      *base = *rotate = 0;
      return true;
      }

   trailing = trailingZeroes(bitPattern);
   trailing &= 0xFFFFFFFE; // make clip the low bit off as we can only do even rotates
   if(trailing > 0)
      {
      bitPattern = bitPattern >> trailing;
      }
   if(bitPattern & 0xFFFFFF00)
      {
      return false;
      }
   *base = bitPattern;
   *rotate = trailing;
   return true;
   }


// TR::Instruction:: member functions

OMR::ARM::Instruction::Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node)
   :OMR::Instruction(cg, precedingInstruction, op, node),
                  _conditions(0),
                  _asyncBranch(false)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }

OMR::ARM::Instruction::Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node)
   :OMR::Instruction(cg, op, node),
     _conditions(0),
     _asyncBranch(false)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }

// TODO: need to fix the InstOpCode initialization
OMR::ARM::Instruction::Instruction(TR::Node *node, TR::CodeGenerator *cg)
   : OMR::InstructionConnector(cg, TR::InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(ARMOp_bad);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(NULL);
   }

OMR::ARM::Instruction::Instruction(TR_ARMOpCodes op, TR::Node *node, TR::CodeGenerator *cg)
   : OMR::InstructionConnector(cg, TR::InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(NULL);
   }

OMR::ARM::Instruction::Instruction(TR::Instruction   *precedingInstruction,
            TR_ARMOpCodes     op,
            TR::Node          *node,
            TR::CodeGenerator *cg)
   : OMR::InstructionConnector(cg, precedingInstruction, TR::InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(NULL);
   }

OMR::ARM::Instruction::Instruction(TR_ARMOpCodes                       op,
            TR::Node                            *node,
            TR::RegisterDependencyConditions    *cond,
            TR::CodeGenerator                   *cg)
   : OMR::InstructionConnector(cg, TR::InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(cond);
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }

OMR::ARM::Instruction::Instruction(TR::Instruction                     *precedingInstruction,
            TR_ARMOpCodes                       op,
            TR::Node                            *node,
            TR::RegisterDependencyConditions    *cond,
            TR::CodeGenerator                   *cg)
   : OMR::InstructionConnector(cg, precedingInstruction, TR::InstOpCode::BAD, node)
   {
   self()->setOpCodeValue(op);
   self()->setConditionCode(ARMConditionCodeAL);
   self()->setDependencyConditions(cond);
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }

void
OMR::ARM::Instruction::remove()
   {
   self()->getPrev()->setNext(self()->getNext());
   self()->getNext()->setPrev(self()->getPrev());
   }

void OMR::ARM::Instruction::ARMNeedsGCMap(uint32_t mask)
   {
   if (TR::comp()->useRegisterMaps())
      self()->setNeedsGCMap(mask);
   }

TR::Register *OMR::ARM::Instruction::getMemoryDataRegister()
   {
   return NULL;
   }

bool OMR::ARM::Instruction::refsRegister(TR::Register *reg)
   {
   return (self()->getDependencyConditions() && self()->getDependencyConditions()->refsRegister(reg));
   }

bool OMR::ARM::Instruction::defsRegister(TR::Register *reg)
   {
   return (self()->getDependencyConditions() && self()->getDependencyConditions()->defsRegister(reg));
   }

bool OMR::ARM::Instruction::defsRealRegister(TR::Register *reg)
   {
   return (self()->getDependencyConditions() && self()->getDependencyConditions()->defsRealRegister(reg));
   }

bool OMR::ARM::Instruction::usesRegister(TR::Register *reg)
   {
   return (self()->getDependencyConditions() && self()->getDependencyConditions()->usesRegister(reg));
   }

bool OMR::ARM::Instruction::dependencyRefsRegister(TR::Register *reg)
   {
   return false;
   }

TR::ARMConditionalBranchInstruction *
OMR::ARM::Instruction::getARMConditionalBranchInstruction()
   {
   return NULL;
   }

// The following safe virtual downcast method is only used in an assertion
// check within "toARMImmInstruction"
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
TR::ARMImmInstruction *OMR::ARM::Instruction::getARMImmInstruction()
   {
   return NULL;
   }
#endif // defined(DEBUG) || defined(PROD_WITH_ASSUMES)


int32_t
OMR::ARM::Instruction::getMachineOpCode()
   {
   return self()->getOpCodeValue();
   }


void OMR::ARM::Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if (self()->getDependencyConditions())
      {
      self()->getDependencyConditions()->assignPostConditionRegisters(self(), kindToBeAssigned, self()->cg());
      self()->getDependencyConditions()->assignPreConditionRegisters(self()->getPrev(), kindToBeAssigned, self()->cg());
      }
   }

// TR::ARMConditionalBranchInstruction member functions
TR::ARMConditionalBranchInstruction *TR::ARMConditionalBranchInstruction::getARMConditionalBranchInstruction()
   {
   return this;
   }

void TR::ARMConditionalBranchInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if (getDependencyConditions())
      {
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
      }
   if (getLabelSymbol()->isStartOfColdInstructionStream())
      {
      // Switch to the outlined instruction stream and assign registers.
      //
      TR_ARMOutOfLineCodeSection *oi = cg()->findOutLinedInstructionsFromLabel(getLabelSymbol());
      TR_ASSERT(oi, "Could not find ARMOutOfLineCodeSection stream from label.  instr=%p, label=%p\n", this, getLabelSymbol());
      if (cg()->getDebug())
            cg()->traceRegisterAssignment("\nOOL: Start register assignment in OOL section");
      if (!oi->hasBeenRegisterAssigned())
         oi->assignRegisters(kindToBeAssigned);
      if (cg()->getDebug())
            cg()->traceRegisterAssignment("\nOOL: Finished register assignment in OOL section");

      // Unlock the free spill list.
      //
      // TODO: live registers that are not spilled at this point should have their backing
      // storage returned to the free spill list.
      //
      cg()->unlockFreeSpillList();
      }
   }

bool TR::ARMConditionalBranchInstruction::refsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::ARMConditionalBranchInstruction::defsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::ARMConditionalBranchInstruction::defsRealRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::ARMConditionalBranchInstruction::usesRegister(TR::Register *reg)
   {
   return false;
   }

// TR::ARMImmInstruction:: member functions


// The following safe virtual downcast method is only used in an assertion
// check within "toARMImmInstruction"
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
TR::ARMImmInstruction *TR::ARMImmInstruction::getARMImmInstruction()
   {
   return this;
   }
#endif // defined(DEBUG) || defined(PROD_WITH_ASSUMES)

// TR::ARMImmSymInstruction:: member functions

TR::ARMImmSymInstruction::ARMImmSymInstruction(TR_ARMOpCodes                       op,
                                               TR::Node                            *node,
                                               uint32_t                            imm,
                                               TR::RegisterDependencyConditions *cond,
                                               TR::SymbolReference                 *sr,
                                               TR::CodeGenerator                   *cg,
                                               TR::Snippet                         *s,
                                               TR_ARMConditionCode                 cc)
   : TR::ARMImmInstruction(op, node, cond, imm, cg),
     _symbolReference(sr),
     _snippet(s)
   {
   setConditionCode(cc);
   }

TR::ARMImmSymInstruction::ARMImmSymInstruction(TR::Instruction                           *precedingInstruction,
                                               TR_ARMOpCodes                       op,
                                               TR::Node                            *node,
                                               uint32_t                            imm,
                                               TR::RegisterDependencyConditions *cond,
                                               TR::SymbolReference                 *sr,
                                               TR::CodeGenerator                   *cg,
                                               TR::Snippet                         *s,
                                               TR_ARMConditionCode                 cc)
   : TR::ARMImmInstruction(precedingInstruction, op, node, cond, imm, cg),
     _symbolReference(sr),
     _snippet(s)
   {
   setConditionCode(cc);
   }

void TR::ARMLabelInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Compilation *comp = TR::comp();
   TR::Machine  *machine        = cg()->machine();
   TR::Register    *target1Virtual = getTarget1Register();
   TR::Register    *source1Virtual = getSource1Register();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   // allocate the source register
   TR::RealRegister *assignedSource1Register;
   if (source1Virtual)
      {
      if (target1Virtual) target1Virtual->block();
      assignedSource1Register = machine->assignSingleRegister(source1Virtual, this);
      if (target1Virtual) target1Virtual->unblock();
      }

   // allocate the target register last
   TR::RealRegister *assignedTarget1Register;
   if (target1Virtual)
      {
      if (source1Virtual) source1Virtual->block();
      assignedTarget1Register = machine->assignSingleRegister(target1Virtual, this);
      if (source1Virtual) source1Virtual->unblock();
      }

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   if (target1Virtual) setTarget1Register(assignedTarget1Register);
   if (source1Virtual) setSource1Register(assignedSource1Register);
   if (isLabel() && getLabelSymbol()->isEndOfColdInstructionStream())
      {
      // A label instruction coming back from OOL.
      //
      // This label is the end of the hot instruction stream (i.e., the fallthru path).
      //
      if (comp->getOption(TR_TraceRA))
         traceMsg (comp,"\nOOL: 1. Taking register state snap shot\n");
      cg()->setIsOutOfLineHotPath(true);
      machine->takeRegisterStateSnapShot();

      // Prevent spilled registers from reclaiming their backing store if they
      // become unspilled.  This will ensure a spilled register will receive the
      // same backing store if it is spilled on either path of the control flow.
      //
      cg()->lockFreeSpillList();
      }

   if (isBranchOp() && getLabelSymbol()->isEndOfColdInstructionStream())
      {
      // This is the branch at the end of the cold instruction stream to the end of
      // the hot instruction stream (or fallthru path).
      //
      // Start RA for OOL cold path, restore register state from snap shot
      //
      if (comp->getOption(TR_TraceRA))
         traceMsg (comp, "\nOOL: 1. Restoring Register state from snap shot\n");
      cg()->setIsOutOfLineHotPath(false);
      machine->restoreRegisterStateFromSnapShot();
      }
   }

// TR::ARMTrg1Src2Instruction:: member functions
bool TR::ARMTrg1Src2Instruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTarget1Register() ||
           reg == getSource1Register() ||
           getSource2Operand()->refsRegister(reg));
   }

bool TR::ARMTrg1Src2Instruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTarget1Register());
   }

bool TR::ARMTrg1Src2Instruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTarget1Register()->getAssignedRegister());
   }

bool TR::ARMTrg1Src2Instruction::usesRegister(TR::Register *reg)
   {
   return (reg == getSource1Register() || getSource2Operand()->refsRegister(reg));
   }

void TR::ARMTrg1Src2Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine  *machine        = cg()->machine();
   TR::Register    *target1Virtual = getTarget1Register();
   TR::Register    *source1Virtual = getSource1Register();
   TR_ARMOperand2 *source2Operand = getSource2Operand();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   // allocate the operand2 first; we always have a source2Operand so no null check is needed
   if (source1Virtual) source1Virtual->block();
   if (target1Virtual) target1Virtual->block();
   TR_ARMOperand2  *assignedSource2Operand = source2Operand->assignRegisters(this, cg());
   if (target1Virtual) target1Virtual->unblock();
   if (source1Virtual) source1Virtual->unblock();

   // allocate the other source register
   TR::RealRegister *assignedSource1Register;
   if (source1Virtual)
      {
      source2Operand->block();
      if (target1Virtual) target1Virtual->block();
      assignedSource1Register = machine->assignSingleRegister(source1Virtual, this);
      if (target1Virtual) target1Virtual->unblock();
      source2Operand->unblock();
      }

   // allocate the target register last
   TR::RealRegister *assignedTarget1Register;
   if (target1Virtual)
      {
      source2Operand->block();
      if (source1Virtual) source1Virtual->block();
      assignedTarget1Register = machine->assignSingleRegister(target1Virtual, this);
      if (source1Virtual) source1Virtual->unblock();
      source2Operand->unblock();
      }

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   if (target1Virtual) setTarget1Register(assignedTarget1Register);
   if (source1Virtual) setSource1Register(assignedSource1Register);
   setSource2Operand(assignedSource2Operand);
   }

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
// TR::ARMTrg2Src1Instruction:: member functions
bool TR::ARMTrg2Src1Instruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTarget1Register() ||
           reg == getTarget2Register() ||
           reg == getSource1Register() );
   }

bool TR::ARMTrg2Src1Instruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTarget1Register() || reg == getTarget2Register());
   }

bool TR::ARMTrg2Src1Instruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTarget1Register()->getAssignedRegister() || reg == getTarget2Register()->getAssignedRegister());
   }

bool TR::ARMTrg2Src1Instruction::usesRegister(TR::Register *reg)
   {
   return (reg == getSource1Register());
   }

void TR::ARMTrg2Src1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine  *machine        = cg()->machine();
   TR::Register    *target1Virtual = getTarget1Register();
   TR::Register    *target2Virtual = getTarget2Register();
   TR::Register    *source1Virtual = getSource1Register();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   // allocate source first;
   TR::RealRegister *assignedSource1Register;
   if (source1Virtual)
      {
      if (target1Virtual) target1Virtual->block();
      if (target2Virtual) target2Virtual->block();
      assignedSource1Register = machine->assignSingleRegister(source1Virtual, this);
      if (target2Virtual) target2Virtual->unblock();
      if (target1Virtual) target1Virtual->unblock();
      }

   // allocate the target1 register
   TR::RealRegister *assignedTarget1Register;
   if (target1Virtual)
      {
      if (target2Virtual) target2Virtual->block();
      if (source1Virtual) source1Virtual->block();
      assignedTarget1Register = machine->assignSingleRegister(target1Virtual, this);
      if (source1Virtual) source1Virtual->unblock();
      if (target2Virtual) target2Virtual->unblock();
      }

   // allocate the target1 register
   TR::RealRegister *assignedTarget2Register;
   if (target2Virtual)
      {
      if (target1Virtual) target1Virtual->block();
      if (source1Virtual) source1Virtual->block();
      assignedTarget2Register = machine->assignSingleRegister(target2Virtual, this);
      if (source1Virtual) source1Virtual->unblock();
      if (target1Virtual) target1Virtual->unblock();
      }

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   if (target1Virtual) setTarget1Register(assignedTarget1Register);
   if (target2Virtual) setTarget2Register(assignedTarget2Register);
   if (source1Virtual) setSource1Register(assignedSource1Register);
   }
#endif

bool TR::ARMMulInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetLoRegister() ||
           reg == getTargetHiRegister() ||
           reg == getSource1Register()  ||
           reg == getSource2Register());
   }

bool TR::ARMMulInstruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTargetLoRegister() ||
           reg == getTargetHiRegister());
   }

bool TR::ARMMulInstruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTargetLoRegister()->getAssignedRegister() ||
           reg == getTargetHiRegister()->getAssignedRegister());
   }

bool TR::ARMMulInstruction::usesRegister(TR::Register *reg)
   {
   return (reg == getSource1Register() ||
           reg == getSource2Register());
   }

void TR::ARMMulInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine         = cg()->machine();
   TR::Register   *targetLoVirtual = getTargetLoRegister();
   TR::Register   *targetHiVirtual = getTargetHiRegister();  // will be NULL if performing mul (as opposed to mull)
   TR::Register   *source1Virtual  = getSource1Register();
   TR::Register   *source2Virtual  = getSource2Register();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   // allocate source2
   source1Virtual->block();
   targetLoVirtual->block();
   if (targetHiVirtual) targetHiVirtual->block();
   TR::RealRegister *assignedSource2Register = machine->assignSingleRegister(source2Virtual, this);
   if (targetHiVirtual) targetHiVirtual->unblock();
   targetLoVirtual->unblock();
   source1Virtual->unblock();

   // allocate source1
   source2Virtual->block();
   targetLoVirtual->block();
   if (targetHiVirtual) targetHiVirtual->block();
   TR::RealRegister *assignedSource1Register = machine->assignSingleRegister(source1Virtual, this);
   if (targetHiVirtual) targetHiVirtual->unblock();
   targetLoVirtual->unblock();
   source2Virtual->unblock();

   // allocate targetLo
   source1Virtual->block();
   source2Virtual->block();
   if (targetHiVirtual) targetHiVirtual->block();
   TR::RealRegister *assignedTargetLoRegister = machine->assignSingleRegister(targetLoVirtual, this);
   if (targetHiVirtual) targetHiVirtual->unblock();
   source2Virtual->unblock();
   source1Virtual->unblock();

   TR::RealRegister *assignedTargetHiRegister;
   if (targetHiVirtual)  // allocate targetHi if mull
      {
      source1Virtual->block();
      source2Virtual->block();
      targetLoVirtual->block();
      assignedTargetHiRegister = machine->assignSingleRegister(targetHiVirtual, this);
      targetLoVirtual->unblock();
      source2Virtual->unblock();
      source1Virtual->unblock();
      }

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());

   if (targetHiVirtual) setTargetHiRegister(assignedTargetHiRegister);
   setTargetLoRegister(assignedTargetLoRegister);
   setSource1Register(assignedSource1Register);
   setSource2Register(assignedSource2Register);
   }

TR::Register *TR::ARMMemInstruction::getMemoryDataRegister()
   {
   return getTargetRegister();
   }


bool TR::ARMMemInstruction::refsRegister(TR::Register *reg)
   {
   return getMemoryReference()->refsRegister(reg);
   }

bool TR::ARMMemInstruction::usesRegister(TR::Register *reg)
   {
   return getMemoryReference()->refsRegister(reg);
   }

void TR::ARMMemInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   getMemoryReference()->assignRegisters(this, cg());

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

// TR::ARMMemSrc1Instruction:: member functions

bool TR::ARMMemSrc1Instruction::refsRegister(TR::Register *reg)
   {
   return (getMemoryReference()->refsRegister(reg) || reg == getSourceRegister());
   }

bool TR::ARMMemSrc1Instruction::defsRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::ARMMemSrc1Instruction::defsRealRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::ARMMemSrc1Instruction::usesRegister(TR::Register *reg)
   {
   return (getMemoryReference()->refsRegister(reg) || reg == getSourceRegister());
   }

void TR::ARMMemSrc1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine *machine       = cg()->machine();
   TR::Register   *sourceVirtual = getSourceRegister();
   TR::Register   *mBaseVirtual  = getMemoryReference()->getModBase();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   sourceVirtual->block();
   getMemoryReference()->assignRegisters(this, cg());
   sourceVirtual->unblock();

   getMemoryReference()->blockRegisters();
   TR::RealRegister *assignedRegister = sourceVirtual->getAssignedRealRegister();
   if (assignedRegister == NULL)
      {
      if (sourceVirtual->getTotalUseCount() == sourceVirtual->getFutureUseCount())
         {
         TR_RegisterKinds kindOfRegister = sourceVirtual->getKind();
         if ((assignedRegister = machine->findBestFreeRegister(kindOfRegister, false, true)) == NULL)
            {
            assignedRegister = machine->freeBestRegister(this, kindOfRegister);
            }
         }
      else
         {
         assignedRegister = machine->reverseSpillState(this, sourceVirtual);
         }
      sourceVirtual->setAssignedRegister(assignedRegister);
      assignedRegister->setAssignedRegister(sourceVirtual);
      assignedRegister->setState(TR::RealRegister::Assigned);
      }
   getMemoryReference()->unblockRegisters();

   if (sourceVirtual->decFutureUseCount() == 0)
      {
      sourceVirtual->setAssignedRegister(NULL);
      assignedRegister->setState(TR::RealRegister::Unlatched);
      }

   setSourceRegister(assignedRegister);

   TR::RealRegister *mBaseReal = mBaseVirtual ? toRealRegister(getMemoryReference()->getModBase()) : NULL;
   if (mBaseVirtual && mBaseVirtual->decFutureUseCount()==0)
      {
      mBaseVirtual->setAssignedRegister(NULL);
      mBaseReal->setState(TR::RealRegister::Unlatched);
      }

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

// TR::ARMTrg1Instruction:: member functions

bool TR::ARMTrg1Instruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister());
   }

bool TR::ARMTrg1Instruction::usesRegister(TR::Register *reg)
   {
   return false;
   }

bool TR::ARMTrg1Instruction::defsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister());
   }

bool TR::ARMTrg1Instruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister()->getAssignedRegister());
   }

void TR::ARMTrg1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   TR::Machine *machine       = cg()->machine();
   TR::Register   *targetVirtual = getTargetRegister();
   setTargetRegister(machine->assignSingleRegister(targetVirtual, this));

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

// TR::ARMTrg1MemInstruction:: member functions

bool TR::ARMTrg1MemInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister() || getMemoryReference()->refsRegister(reg));
   }

bool TR::ARMTrg1MemInstruction::usesRegister(TR::Register *reg)
   {
   return getMemoryReference()->refsRegister(reg);
   }

void TR::ARMTrg1MemInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register *targetVirtual = getTargetRegister();
   TR::Register *mBaseVirtual  = getMemoryReference()->getModBase();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   getMemoryReference()->blockRegisters();
   setTargetRegister(cg()->machine()->assignSingleRegister(targetVirtual, this));
   getMemoryReference()->unblockRegisters();

   targetVirtual->block();
   getMemoryReference()->assignRegisters(this, cg());
   targetVirtual->unblock();

   TR::RealRegister *mBaseReal = mBaseVirtual ? toRealRegister(getMemoryReference()->getModBase()) : NULL;
   if (mBaseVirtual && mBaseVirtual->decFutureUseCount() == 0)
      {
      mBaseVirtual->setAssignedRegister(NULL);
      mBaseReal->setState(TR::RealRegister::Unlatched);
      }

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

// TR::ARMTrg1MemSrc1Instruction:: member functions
bool TR::ARMTrg1MemSrc1Instruction::refsRegister(TR::Register *reg)
   {
   return (reg == getTargetRegister() || getMemoryReference()->refsRegister(reg) || reg == getSourceRegister());
   }

bool TR::ARMTrg1MemSrc1Instruction::usesRegister(TR::Register *reg)
   {
   return (getMemoryReference()->refsRegister(reg) || reg == getSourceRegister());
   }

void TR::ARMTrg1MemSrc1Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Register *targetVirtual = getTargetRegister();
   TR::Register *mBaseVirtual  = getMemoryReference()->getModBase();
   TR::Register *sourceVirtual = getSourceRegister();

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   getMemoryReference()->blockRegisters();
   targetVirtual->block();
   setTargetRegister(cg()->machine()->assignSingleRegister(sourceVirtual, this));
   targetVirtual->unblock();
   getMemoryReference()->unblockRegisters();

   targetVirtual->block();
   sourceVirtual->block();
   getMemoryReference()->assignRegisters(this, cg());
   sourceVirtual->block();
   targetVirtual->unblock();

   getMemoryReference()->blockRegisters();
   sourceVirtual->block();
   setTargetRegister(cg()->machine()->assignSingleRegister(targetVirtual, this));
   sourceVirtual->unblock();
   getMemoryReference()->unblockRegisters();

   TR::RealRegister *mBaseReal = mBaseVirtual ? toRealRegister(getMemoryReference()->getModBase()) : NULL;
   if (mBaseVirtual && mBaseVirtual->decFutureUseCount() == 0)
      {
      mBaseVirtual->setAssignedRegister(NULL);
      mBaseReal->setState(TR::RealRegister::Unlatched);
      }

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

// TR::ARMControlFlowInstruction:: member functions

bool TR::ARMControlFlowInstruction::refsRegister(TR::Register *reg)
   {
   int i;
   for (i = 0; i < getNumTargets(); i++)
      {
      if (reg == getTargetRegister(i))
         return true;
      }
   for (i = 0; i < getNumSources(); i++)
      {
      if (reg == getSourceRegister(i))
         return true;
      }
   return false;
   }

bool TR::ARMControlFlowInstruction::defsRegister(TR::Register *reg)
   {
   for (int i = 0; i < getNumTargets(); i++)
      {
      if (reg == getTargetRegister(i))
         return true;
      }
   return false;
   }

bool TR::ARMControlFlowInstruction::defsRealRegister(TR::Register *reg)
   {
   for (int i = 0; i < getNumTargets(); i++)
      {
      if (reg == getTargetRegister(i)->getAssignedRegister())
         return true;
      }
   return false;
   }

bool TR::ARMControlFlowInstruction::usesRegister(TR::Register *reg)
   {
   for (int i = 0; i < getNumSources(); i++)
      {
      if (reg == getSourceRegister(i))
         return true;
      }
   return false;
   }

static TR::Instruction *expandBlongLessThan(TR::Node          *node,
                                           TR::CodeGenerator *cg,
                                           TR::Instruction   *cursor,
                                           TR::Register      *temp,
                                           TR::Register      *result,
                                           TR::Register      *aHi,
                                           TR::Register      *aLo,
                                           TR::Register      *bHi,
                                           TR::Register      *bLo,
                                           bool              trueValue)
   {
   // This helper works by subtracting the two long values, and testing the
   // resulting high word. The only interesting test on the high word is if
   // it is negative. (A zero high word does not indicate that both words
   // are zero!).
   cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, result, !trueValue, 0, cursor);
   cursor = generateTrg1Src2Instruction(cg, ARMOp_sub_r, node, temp, aLo, bLo, cursor);
   cursor = generateTrg1Src2Instruction(cg, ARMOp_sbc_r, node, temp, aHi, bHi, cursor);
   cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, result, trueValue, 0, cursor);
   cursor->setConditionCode(ARMConditionCodeLT);
   return cursor;
}

void TR::ARMControlFlowInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::Machine    *machine = cg()->machine();
   TR::Node          *currentNode = getNode();
   TR::RealRegister  *realSources[8], *realTargets[5];
   int i;

   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   for (i = 0; i< getNumSources(); i++)
      getSourceRegister(i)->block();

   for (i = 0; i< getNumTargets(); i++)
      getTargetRegister(i)->block();

   for (i = 0; i< getNumTargets(); i++)
      {
      TR::Register      *targetVirtual    = getTargetRegister(i);
      TR::RealRegister  *assignedRegister = targetVirtual->getAssignedRealRegister();
      TR_RegisterKinds  kindOfRegister   = targetVirtual->getKind();
      if (assignedRegister == NULL)
         {
         if (targetVirtual->getTotalUseCount() != targetVirtual->getFutureUseCount())
            {
            assignedRegister = machine->reverseSpillState(this, targetVirtual, NULL, false);
            }
         else
            {
            if ((assignedRegister = machine->findBestFreeRegister(kindOfRegister, false, true)) == NULL)
               {
               assignedRegister = machine->freeBestRegister(this, kindOfRegister, NULL, false);
               }
            }
         targetVirtual->setAssignedRegister(assignedRegister);
         assignedRegister->setAssignedRegister(targetVirtual);
         assignedRegister->setState(TR::RealRegister::Assigned);
         }
      realTargets[i] = assignedRegister;
      targetVirtual->block();
      }

   for (i = 0; i< getNumSources(); i++)
      {
      TR::Register      *sourceVirtual = getSourceRegister(i);
      TR_RegisterKinds  kindOfRegister = sourceVirtual->getKind();
      TR::RealRegister  *assignedRegister = sourceVirtual->getAssignedRealRegister();
      if (assignedRegister == NULL)
         {
         if (sourceVirtual->getTotalUseCount() == sourceVirtual->getFutureUseCount())
            {
            if ((assignedRegister = machine->findBestFreeRegister(kindOfRegister, false, true)) == NULL)
               {
               assignedRegister = machine->freeBestRegister(this, kindOfRegister, NULL, false);
               }
            }
         else
            {
            assignedRegister = machine->reverseSpillState(this, sourceVirtual, NULL, false);
            }
         sourceVirtual->setAssignedRegister(assignedRegister);
         assignedRegister->setAssignedRegister(sourceVirtual);
         assignedRegister->setState(TR::RealRegister::Assigned);
         }
      realSources[i] = assignedRegister;
      sourceVirtual->block();
      }

   for (i = 0; i< getNumTargets(); i++)
      {
      TR::Register *targetVirtual = getTargetRegister(i);
      // Because of OOL codegen, if the target virtual is defined at this instruction and this instruction is in the hot path of an OOL section,
      // the virtual's future use count may still be >0 at this point because of outstsanding uses in the cold path.
      // Therefore we have to check if this is the start of it's live range and unlatch it.
      targetVirtual->unblock();
      if (targetVirtual->decFutureUseCount() == 0 || targetVirtual->getStartOfRange() == this)
         {
         targetVirtual->setAssignedRegister(NULL);
         realTargets[i]->setState(TR::RealRegister::Unlatched);
         }

      setTargetRegister(i, realTargets[i]);
      }

   for (i = 0; i< getNumSources(); i++)
      {
      TR::Register *sourceVirtual = getSourceRegister(i);

      sourceVirtual->unblock();
      if (sourceVirtual->decFutureUseCount() == 0)
         {
         sourceVirtual->setAssignedRegister(NULL);
         realSources[i]->setState(TR::RealRegister::Unlatched);
         }

      setSourceRegister(i, realSources[i]);
      }
   // Now that registers have been allocated, expand the code
   TR::Instruction *cursor = this;
   TR::LabelSymbol *label2 = TR::LabelSymbol::create(cg()->trHeapMemory(),cg());
//   TR::LabelSymbol *label1;
   switch(getOpCode().getOpCodeValue())
      {
      case ARMOp_iflong:
         {
         int isGT = (getConditionCode() == ARMConditionCodeGT);
         cursor = generateSrc2Instruction(cg(), ARMOp_cmp, currentNode, getSourceRegister(0), getSourceRegister(2), cursor);
         cursor = generateConditionalBranchInstruction(cg(), currentNode, isGT ? ARMConditionCodeGT : ARMConditionCodeLT, getLabelSymbol(), cursor);
         cursor = generateConditionalBranchInstruction(cg(), currentNode, isGT ? ARMConditionCodeLT : ARMConditionCodeGT, label2, cursor);
         cursor = generateSrc2Instruction(cg(), ARMOp_cmp, currentNode, getSourceRegister(1), getSourceRegister(3), cursor);
         cursor = generateConditionalBranchInstruction(cg(), currentNode, isGT ? ARMConditionCodeHI : ARMConditionCodeLS, getLabelSymbol(), cursor);
         cursor = generateLabelInstruction(cg(), ARMOp_label, currentNode, label2, cursor);
         }
         break;
      case ARMOp_setblong:
         switch (getConditionCode())
            {
            case ARMConditionCodeLT:
               cursor = expandBlongLessThan(currentNode, cg(), cursor,
                  getTargetRegister(1), getTargetRegister(0),
                  getSourceRegister(1), getSourceRegister(0),
                  getSourceRegister(3), getSourceRegister(2),
                  1);
               break;
            case ARMConditionCodeGT:
               // A > B ==> B < A
               cursor = expandBlongLessThan(currentNode, cg(), cursor,
                  getTargetRegister(1), getTargetRegister(0),
                  getSourceRegister(3), getSourceRegister(2),
                  getSourceRegister(1), getSourceRegister(0),
                  1);
               break;
            case ARMConditionCodeGE:
               // A >= B ==> !(B < A)
               cursor = expandBlongLessThan(currentNode, cg(), cursor,
                  getTargetRegister(1), getTargetRegister(0),
                  getSourceRegister(1), getSourceRegister(0),
                  getSourceRegister(3), getSourceRegister(2),
                  0);
               break;
            case ARMConditionCodeLE:
               // A <= B ==> !(B > A)
               cursor = expandBlongLessThan(currentNode, cg(), cursor,
                  getTargetRegister(1), getTargetRegister(0),
                  getSourceRegister(3), getSourceRegister(2),
                  getSourceRegister(1), getSourceRegister(0),
                  0);
               break;
            case ARMConditionCodeEQ:
            case ARMConditionCodeNE:
               cursor = generateTrg1ImmInstruction(cg(), ARMOp_mov, currentNode, getTargetRegister(0), 0, 0, cursor);
               cursor = generateSrc2Instruction(cg(), ARMOp_cmp, currentNode, getSourceRegister(0), getSourceRegister(2), cursor);
               cursor = generateSrc2Instruction(cg(), ARMOp_cmp, currentNode, getSourceRegister(1), getSourceRegister(3), cursor);
               cursor->setConditionCode(ARMConditionCodeEQ);
               cursor = generateTrg1ImmInstruction(cg(), ARMOp_mov, currentNode, getTargetRegister(0), 1, 0, cursor);
               cursor->setConditionCode(getConditionCode());
               break;
            default:
               TR_ASSERT(0, "Unknown condition");
            }
         break;
      case ARMOp_setbool:
         TR_ASSERT(0, "implement setbool");
         /*
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, getCmpOpValue(), currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(1), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, getOpCode2Value(), currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 0, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMLabelInstruction(cursor, ARMOp_label, currentNode, label2, cg());
         break;
         */
      case ARMOp_setbflt:
         TR_ASSERT(0, "implement setbflt");
         /*
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, getCmpOpValue(), currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(1), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, getOpCode2Value(), currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, getOpCode3Value(), currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 0, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMLabelInstruction(cursor, ARMOp_label, currentNode, label2, cg());
         break;
         */
      case ARMOp_lcmp:
         /*
          * correct:
          * cmp   hi1, hi2
          * movgt res, #1
          * mvnlt res, #0
          * bne   endLcmp
          * cmp   lo1, lo2
          * movhi res, #1
          * moveq res, #0
          * mvnlo res, #0
          *
          * incorrect, due to signed cmp of low word:
          * cmp   hi1, hi2
          * cmpeq lo1, lo2
          * movgt res, #1
          * mvnlt res, #0
          * moveq res, #0
          */
         cursor = generateSrc2Instruction(cg(), ARMOp_cmp, currentNode, getSourceRegister(1), getSourceRegister(3), cursor);
         cursor = generateTrg1ImmInstruction(cg(), ARMOp_mov, currentNode, getTargetRegister(0), 1, 0, cursor);
         cursor->setConditionCode(ARMConditionCodeGT);
         cursor = generateTrg1ImmInstruction(cg(), ARMOp_mvn, currentNode, getTargetRegister(0), 0, 0, cursor);
         cursor->setConditionCode(ARMConditionCodeLT);
         cursor = generateConditionalBranchInstruction(cg(), currentNode, ARMConditionCodeNE, label2, cursor);
         cursor = generateSrc2Instruction(cg(), ARMOp_cmp, currentNode, getSourceRegister(0), getSourceRegister(2), cursor);
         cursor = generateTrg1ImmInstruction(cg(), ARMOp_mov, currentNode, getTargetRegister(0), 1, 0, cursor);
         cursor->setConditionCode(ARMConditionCodeHI);
         cursor = generateTrg1ImmInstruction(cg(), ARMOp_mov, currentNode, getTargetRegister(0), 0, 0, cursor);
         cursor->setConditionCode(ARMConditionCodeEQ);
         cursor = generateTrg1ImmInstruction(cg(), ARMOp_mvn, currentNode, getTargetRegister(0), 0, 0, cursor);
         cursor->setConditionCode(ARMConditionCodeCC);
         cursor = generateLabelInstruction(cg(), ARMOp_label, currentNode, label2, cursor);
         break;
/*
      case ARMOp_flcmpl:
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_fcmpu, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(2), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, ARMOp_bgt, currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 0, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, ARMOp_beq, currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), -1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMLabelInstruction(cursor, ARMOp_label, currentNode, label2, cg());
         break;
      case ARMOp_flcmpg:
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_fcmpu, currentNode, getTargetRegister(0), getSourceRegister(0), getSourceRegister(2), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), -1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, ARMOp_blt, currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 0, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, ARMOp_beq, currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMLabelInstruction(cursor, ARMOp_label, currentNode, label2, cg());
         break;
      case ARMOp_idiv:
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_eqv, currentNode, getTargetRegister(2), getTargetRegister(1), getSourceRegister(1), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_and, currentNode, getTargetRegister(2), getSourceRegister(2), getTargetRegister(2), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src1ImmInstruction(cursor, ARMOp_cmpi4, currentNode, getTargetRegister(0), getTargetRegister(2), -1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, ARMOp_beq, currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_divw, currentNode, getTargetRegister(1), getSourceRegister(1), getSourceRegister(2), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMLabelInstruction(cursor, ARMOp_label, currentNode, label2, cg());
         break;
      case ARMOp_irem:
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src1ImmInstruction(cursor, ARMOp_cmpi4, currentNode, getTargetRegister(0), getSourceRegister(2), -1, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1ImmInstruction(cursor, ARMOp_li, currentNode, getTargetRegister(1), 0, cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMConditionalBranchInstruction(cursor, ARMOp_beq, currentNode, label2, getTargetRegister(0), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_divw, currentNode, getTargetRegister(2), getSourceRegister(1), getSourceRegister(2), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_mullw, currentNode, getTargetRegister(2), getSourceRegister(2), getTargetRegister(2), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMTrg1Src2Instruction(cursor, ARMOp_subf, currentNode, getTargetRegister(1), getTargetRegister(2), getSourceRegister(1), cg());
         cursor = new (cg()->trHeapMemory()) TR::ARMLabelInstruction(cursor, ARMOp_label, currentNode, label2, cg());
         break;
*/
      default:
         TR_ASSERT(0, "missing evaluate");
      }
   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }

bool TR::ARMMultipleMoveInstruction::refsRegister(TR::Register *reg)
   {
   return (reg == getMemoryBaseRegister());
   }

bool TR::ARMMultipleMoveInstruction::defsRegister(TR::Register *reg)
   {
   return (reg == getMemoryBaseRegister());
   }

bool TR::ARMMultipleMoveInstruction::defsRealRegister(TR::Register *reg)
   {
   return (reg == getMemoryBaseRegister()->getAssignedRegister());
   }

bool TR::ARMMultipleMoveInstruction::usesRegister(TR::Register *reg)
   {
   return (reg == getMemoryBaseRegister());
   }

void TR::ARMMultipleMoveInstruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   if (getDependencyConditions())
      getDependencyConditions()->assignPostConditionRegisters(this, kindToBeAssigned, cg());

   TR::Machine   *machine          = cg()->machine();
   TR::Register     *targetVirtual    = getMemoryBaseRegister();
   TR::RealRegister *assignedRegister = machine->assignSingleRegister(targetVirtual, this);
   setMemoryBaseRegister(assignedRegister);

   if (getDependencyConditions())
      getDependencyConditions()->assignPreConditionRegisters(this->getPrev(), kindToBeAssigned, cg());
   }
