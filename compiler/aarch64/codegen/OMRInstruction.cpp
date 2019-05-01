/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

namespace TR { class Register; }

OMR::ARM64::Instruction::Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, op, node),
     _conditions(NULL)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }


OMR::ARM64::Instruction::Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node)
   : OMR::Instruction(cg, precedingInstruction, op, node),
     _conditions(NULL)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   }


OMR::ARM64::Instruction::Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::RegisterDependencyConditions *cond, TR::Node *node)
   : OMR::Instruction(cg, op, node),
     _conditions(cond)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }


OMR::ARM64::Instruction::Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::RegisterDependencyConditions *cond, TR::Node *node)
   : OMR::Instruction(cg, precedingInstruction, op, node),
     _conditions(cond)
   {
   self()->setBlockIndex(cg->getCurrentBlockIndex());
   if (cond)
      cond->incRegisterTotalUseCounts(cg);
   }


void
OMR::ARM64::Instruction::remove()
   {
   self()->getPrev()->setNext(self()->getNext());
   self()->getNext()->setPrev(self()->getPrev());
   }


void OMR::ARM64::Instruction::ARM64NeedsGCMap(TR::CodeGenerator *cg, uint32_t mask)
   {
   if (cg->comp()->useRegisterMaps())
      self()->setNeedsGCMap(mask);
   }


TR::Register *
OMR::ARM64::Instruction::getMemoryDataRegister()
   {
   return NULL;
   }


bool
OMR::ARM64::Instruction::refsRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::ARM64::Instruction::getDependencyConditions();
   return cond && cond->refsRegister(reg);
   }


bool
OMR::ARM64::Instruction::defsRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::ARM64::Instruction::getDependencyConditions();
   return cond && cond->defsRegister(reg);
   }


bool
OMR::ARM64::Instruction::usesRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::ARM64::Instruction::getDependencyConditions();
   return cond && cond->usesRegister(reg);
   }


bool
OMR::ARM64::Instruction::dependencyRefsRegister(TR::Register * reg)
   {
   TR::RegisterDependencyConditions *cond = OMR::ARM64::Instruction::getDependencyConditions();
   return cond && cond->refsRegister(reg);
   }


void
OMR::ARM64::Instruction::assignRegisters(TR_RegisterKinds kindToBeAssigned)
   {
   TR::RegisterDependencyConditions *cond = OMR::ARM64::Instruction::getDependencyConditions();
   if (cond)
      {
      cond->assignPostConditionRegisters(self(), kindToBeAssigned, self()->cg());
      cond->assignPreConditionRegisters(self()->getPrev(), kindToBeAssigned, self()->cg());
      }
   }
