/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef TR_INSTRUCTION_INCL
#define TR_INSTRUCTION_INCL

#include "codegen/ARMConditionCode.hpp"
#include "codegen/OMRInstruction.hpp"

#include "codegen/RegisterDependency.hpp" // @@@@

namespace TR
{
class Instruction;

class OMR_EXTENSIBLE Instruction : public OMR::InstructionConnector
   {

   public:

   Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node = 0)
      : OMR::InstructionConnector(cg, op, node)
      {}
   Instruction(TR::CodeGenerator *cg, TR::Instruction *precedingInstruction, TR::InstOpCode::Mnemonic op, TR::Node *node = 0)
      : OMR::InstructionConnector(cg, precedingInstruction, op, node)
      {}

   Instruction(TR::Node *node, TR::CodeGenerator *cg)
      : OMR::InstructionConnector(node, cg)
      {}

   Instruction(TR_ARMOpCodes op, TR::Node *node, TR::CodeGenerator *cg)
      : OMR::InstructionConnector(op, node, cg)
      {}

   Instruction(TR::Instruction   *precedingInstruction,
               TR_ARMOpCodes     op,
               TR::Node          *node,
               TR::CodeGenerator *cg)
      : OMR::InstructionConnector(precedingInstruction, op, node, cg)
      {}

   Instruction(TR_ARMOpCodes                       op,
               TR::Node                            *node,
               TR::RegisterDependencyConditions    *cond,
               TR::CodeGenerator                   *cg)
      : OMR::InstructionConnector(op, node, cond, cg)
      {}

   Instruction(TR::Instruction                     *precedingInstruction,
               TR_ARMOpCodes                       op,
               TR::Node                            *node,
               TR::RegisterDependencyConditions    *cond,
               TR::CodeGenerator                   *cg)
      : OMR::InstructionConnector(precedingInstruction, op, node, cond, cg)
      {}

   };

}

#include "codegen/OMRInstruction_inlines.hpp"

//TODO: these downcasts everywhere need to be removed
inline uint32_t        * toARMCursor(uint8_t *i) { return (uint32_t *)i; }

#endif
