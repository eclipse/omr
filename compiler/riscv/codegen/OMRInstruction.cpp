/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include <stdint.h>
#include <stdlib.h>

#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RVInstruction.hpp"

#define RISCV_INSTRUCTION_LENGTH 4

namespace TR { class Register; }

OMR::RV::Instruction::Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, op, node),
     _conditions(NULL)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }


OMR::RV::Instruction::Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, precedingInstruction, op, node),
     _conditions(NULL)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }


OMR::RV::Instruction::Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::RegisterDependencyConditions *cond, TR::Node *node)
   : OMR::Instruction(cg, op, node),
     _conditions(cond)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }


OMR::RV::Instruction::Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::RegisterDependencyConditions *cond, TR::Node *node)
   : OMR::Instruction(cg, precedingInstruction, op, node),
     _conditions(cond)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }


void
OMR::RV::Instruction::remove()
   {
   self()->getPrev()->setNext(self()->getNext());
   self()->getNext()->setPrev(self()->getPrev());
   }


void OMR::RV::Instruction::RVNeedsGCMap(TR::CodeGenerator *cg, uint32_t mask)
   {
   if (cg->comp()->useRegisterMaps())
      self()->setNeedsGCMap(mask);
   }

bool
OMR::RV::Instruction::refsRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::RV::Instruction::getDependencyConditions();
   return cond && cond->refsRegister(reg);
   }


bool
OMR::RV::Instruction::defsRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::RV::Instruction::getDependencyConditions();
   return cond && cond->defsRegister(reg);
   }


bool
OMR::RV::Instruction::usesRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::RV::Instruction::getDependencyConditions();
   return cond && cond->usesRegister(reg);
   }


bool
OMR::RV::Instruction::dependencyRefsRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::RV::Instruction::getDependencyConditions();
   return cond && cond->refsRegister(reg);
   }


void
OMR::RV::Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::RegisterDependencyConditions *cond = OMR::RV::Instruction::getDependencyConditions();
   if (cond)
      {
      cond->assignPostConditionRegisters(self(), kindToBeAssigned, self()->cg());
      cond->assignPreConditionRegisters(self()->getPrev(), kindToBeAssigned, self()->cg());
      }
   }

uint8_t *OMR::RV::Instruction::generateBinaryEncoding()
   {
   TR_ASSERT(false, "generateBinaryEncoding() must be overloaded in subclasses");
   return NULL;
   }

int32_t OMR::RV::Instruction::estimateBinaryLength(int32_t currentEstimate)
   {
   setEstimatedBinaryLength(RISCV_INSTRUCTION_LENGTH);
   return currentEstimate + self()->getEstimatedBinaryLength();
   }

TR::BtypeInstruction *OMR::RV::Instruction::getBtypeInstruction()
   {
   return NULL;
   }
