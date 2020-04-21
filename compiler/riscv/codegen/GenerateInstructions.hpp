/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef GENERATE_INSTRUCTIONS_INCL
#define GENERATE_INSTRUCTIONS_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "env/jittypes.h"
#include "runtime/Runtime.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class Snippet; }
namespace TR { class SymbolReference; }

/*
 * @brief Generates simple instruction
 * @param[in] cg : CodeGenerator
 * @param[in] op : instruction opcode
 * @param[in] node : node
 * @param[in] preced : preceding instruction
 * @return generated instruction
 */
TR::Instruction *generateInstruction(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node *node,
                   TR::Instruction *preced = NULL);

/*
 * @brief Generates label instruction
 * @param[in] cg : CodeGenerator
 * @param[in] op : instruction opcode
 * @param[in] node : node
 * @param[in] sym : label symbol
 * @param[in] preced : preceding instruction
 * @return generated instruction
 */
TR::Instruction *generateLABEL(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node *node,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = NULL);

/*
 * @brief Generates label instruction with register dependency
 * @param[in] cg : CodeGenerator
 * @param[in] op : instruction opcode
 * @param[in] node : node
 * @param[in] sym : label symbol
 * @param[in] cond : register dependency condition
 * @param[in] preced : preceding instruction
 * @return generated instruction
 */
TR::Instruction *generateLABEL(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node *node,
                   TR::LabelSymbol *sym,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction *preced = NULL);

/*
 * @brief Generates admin instruction
 * @param[in] cg : CodeGenerator
 * @param[in] op : instruction opcode
 * @param[in] node : node
 * @param[in] fenceNode : fence node
 * @param[in] preced : preceding instruction
 * @return generated instruction
 */
TR::Instruction *generateADMIN(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node *node,
                   TR::Node *fenceNode = NULL,
                   TR::Instruction *preced = NULL);

/*
 * @brief Generates admin instruction with register dependency
 * @param[in] cg : CodeGenerator
 * @param[in] op : instruction opcode
 * @param[in] node : node
 * @param[in] cond : register dependency condition
 * @param[in] fenceNode : fence node
 * @param[in] preced : preceding instruction
 * @return generated instruction
 */
TR::Instruction *generateADMIN(
                   TR::CodeGenerator *cg,
                   TR::InstOpCode::Mnemonic op,
                   TR::Node *node,
                   TR::RegisterDependencyConditions *cond,
                   TR::Node *fenceNode = NULL,
                   TR::Instruction *preced = NULL);

TR::Instruction *generateRTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *treg,
                                TR::Register      *s1reg,
                                TR::Register      *s2reg,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);


TR::Instruction *generateITYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *treg,
                                TR::Register      *sreg,
                                uint32_t          imm,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateLOAD(  TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                TR::MemoryReference *memRef,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateSTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *s1reg,
                                TR::Register      *s2reg,
                                uint32_t          imm,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateSTORE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::MemoryReference *memRef,
                                TR::Register      *srcReg,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateBTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::LabelSymbol   *sym,
                                TR::Register      *src1,
                                TR::Register      *src2,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateUTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                uint32_t          imm,
                                TR::Register      *reg,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateJTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                uintptr_t        imm,
                                TR::RegisterDependencyConditions *cond,
                                TR::SymbolReference *sr,
                                TR::Snippet       *s,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateJTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                TR::LabelSymbol   *label,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

TR::Instruction *generateJTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                TR::LabelSymbol   *label,
                                TR::RegisterDependencyConditions *cond,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous = NULL);

#endif
