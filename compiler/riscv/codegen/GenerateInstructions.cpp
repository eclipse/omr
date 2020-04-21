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

#include <stdint.h>

#include "codegen/GenerateInstructions.hpp"
#include "codegen/RVInstruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/RegisterDependency.hpp"
#include "il/DataTypes_inlines.hpp"
#include "il/Node_inlines.hpp"

namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class SymbolReference; }

TR::Instruction *generateInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::Instruction(op, node, preced, cg);
   return new (cg->trHeapMemory()) TR::Instruction(op, node, cg);
   }


TR::Instruction *generateLABEL(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::LabelSymbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::LabelInstruction(op, node, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::LabelInstruction(op, node, sym, cg);
   }

TR::Instruction *generateLABEL(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::LabelSymbol *sym, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::LabelInstruction(op, node, sym, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::LabelInstruction(op, node, sym, cond, cg);
   }

TR::Instruction *generateADMIN(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Node *fenceNode, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::AdminInstruction(op, node, fenceNode, preced, cg);
   return new (cg->trHeapMemory()) TR::AdminInstruction(op, node, fenceNode, cg);
   }

TR::Instruction *generateADMIN(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::RegisterDependencyConditions *cond, TR::Node *fenceNode, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::AdminInstruction(op, cond, node, fenceNode, preced, cg);
   return new (cg->trHeapMemory()) TR::AdminInstruction(op, cond, node, fenceNode, cg);
   }

TR::Instruction *generateRTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *treg,
                                TR::Register      *s1reg,
                                TR::Register      *s2reg,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::RtypeInstruction(op, n, treg, s1reg, s2reg, previous, cg);
   return new (cg->trHeapMemory()) TR::RtypeInstruction(op, n, treg, s1reg, s2reg, cg);
   }


TR::Instruction *generateITYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *treg,
                                TR::Register      *sreg,
                                uint32_t          imm,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::ItypeInstruction(op, n, treg, sreg, imm, previous, cg);
   return new (cg->trHeapMemory()) TR::ItypeInstruction(op, n, treg, sreg, imm, cg);
   }

TR::Instruction *generateLOAD(  TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                TR::MemoryReference *memRef,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::LoadInstruction(op, n, trgReg, memRef, previous, cg);
   return new (cg->trHeapMemory()) TR::LoadInstruction(op, n, trgReg, memRef, cg);
   }

TR::Instruction *generateSTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *s1reg,
                                TR::Register      *s2reg,
                                uint32_t          imm,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::StypeInstruction(op, n, s1reg, s2reg, imm, previous, cg);
   return new (cg->trHeapMemory()) TR::StypeInstruction(op, n, s1reg, s2reg, imm, cg);
   }

TR::Instruction *generateSTORE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::MemoryReference *memRef,
                                TR::Register      *srcReg,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::StoreInstruction(op, n, memRef, srcReg, previous, cg);
   return new (cg->trHeapMemory()) TR::StoreInstruction(op, n, memRef, srcReg, cg);
   }

TR::Instruction *generateBTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::LabelSymbol   *sym,
                                TR::Register      *src1,
                                TR::Register      *src2,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::BtypeInstruction(op, n, sym, src1, src2, previous, cg);
   return new (cg->trHeapMemory()) TR::BtypeInstruction(op, n, sym, src1, src2, cg);
   }

TR::Instruction *generateUTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                uint32_t          imm,
                                TR::Register      *reg,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::UtypeInstruction(op, n, imm, reg, previous, cg);
   return new (cg->trHeapMemory()) TR::UtypeInstruction(op, n, imm, reg, cg);
   }

TR::Instruction *generateJTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                uintptr_t        imm,
                                TR::RegisterDependencyConditions *cond,
                                TR::SymbolReference *sr,
                                TR::Snippet       *s,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::JtypeInstruction(op, n, trgReg, imm, cond, sr, s, previous, cg);
   return new (cg->trHeapMemory()) TR::JtypeInstruction(op, n, trgReg, imm, cond, sr, s, cg);
   }

TR::Instruction *generateJTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                TR::LabelSymbol   *label,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::JtypeInstruction(op, n, trgReg, label, previous, cg);
   return new (cg->trHeapMemory()) TR::JtypeInstruction(op, n, trgReg, label, cg);
   }

TR::Instruction *generateJTYPE( TR::InstOpCode::Mnemonic op,
                                TR::Node          *n,
                                TR::Register      *trgReg,
                                TR::LabelSymbol   *label,
                                TR::RegisterDependencyConditions *cond,
                                TR::CodeGenerator *cg,
                                TR::Instruction   *previous)
   {
   if (previous)
      return new (cg->trHeapMemory()) TR::JtypeInstruction(op, n, trgReg, label, cond, previous, cg);
   return new (cg->trHeapMemory()) TR::JtypeInstruction(op, n, trgReg, label, cond, cg);
   }
