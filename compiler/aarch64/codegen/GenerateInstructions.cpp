/*******************************************************************************
 * Copyright (c) 2018, 2020 IBM Corp. and others
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

#include "aarch64/codegen/GenerateInstructions.hpp"

#include <stdint.h>
#include "codegen/ARM64ConditionCode.hpp"
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
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

TR::ARM64ImmInstruction *generateImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node, uint32_t imm,
                                       TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64ImmInstruction(op, node, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64ImmInstruction(op, node, imm, cg);
   }

TR::Instruction *generateImmSymInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   uintptr_t imm, TR::RegisterDependencyConditions *cond, TR::SymbolReference *sr, TR::Snippet *s,
   TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64ImmSymInstruction(op, node, imm, cond, sr, s, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64ImmSymInstruction(op, node, imm, cond, sr, s, cg);
   }

TR::Instruction *generateLabelInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::LabelSymbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64LabelInstruction(op, node, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64LabelInstruction(op, node, sym, cg);
   }

TR::Instruction *generateLabelInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::LabelSymbol *sym, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64LabelInstruction(op, node, sym, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64LabelInstruction(op, node, sym, cond, cg);
   }

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::LabelSymbol *sym, TR::ARM64ConditionCode cc, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64ConditionalBranchInstruction(op, node, sym, cc, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64ConditionalBranchInstruction(op, node, sym, cc, cg);
   }

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::LabelSymbol *sym, TR::ARM64ConditionCode cc, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64ConditionalBranchInstruction(op, node, sym, cc, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64ConditionalBranchInstruction(op, node, sym, cc, cond, cg);
   }

TR::Instruction *generateCompareBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *sreg, TR::LabelSymbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64CompareBranchInstruction(op, node, sreg, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64CompareBranchInstruction(op, node, sreg, sym, cg);
   }

TR::Instruction *generateRegBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64RegBranchInstruction(op, node, treg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64RegBranchInstruction(op, node, treg, cg);
   }

TR::Instruction *generateRegBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64RegBranchInstruction(op, node, treg, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64RegBranchInstruction(op, node, treg, cond, cg);
   }

TR::Instruction *generateAdminInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Node *fenceNode, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64AdminInstruction(op, node, fenceNode, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64AdminInstruction(op, node, fenceNode, cg);
   }

TR::Instruction *generateAdminInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::RegisterDependencyConditions *cond, TR::Node *fenceNode, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64AdminInstruction(op, cond, node, fenceNode, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64AdminInstruction(op, cond, node, fenceNode, cg);
   }

TR::Instruction *generateTrg1ImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, uint32_t imm, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1ImmInstruction(op, node, treg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1ImmInstruction(op, node, treg, imm, cg);
   }

TR::Instruction *generateTrg1ImmSymInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, uint32_t imm, TR::Symbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1ImmSymInstruction(op, node, treg, imm, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1ImmSymInstruction(op, node, treg, imm, sym, cg);
   }

TR::Instruction *generateTrg1Src1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src1Instruction(op, node, treg, s1reg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src1Instruction(op, node, treg, s1reg, cg);
   }

TR::Instruction *generateTrg1Src1ImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, uint32_t imm, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, s1reg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node,treg, s1reg, imm, cg);
   }

TR::Instruction *generateTrg1Src2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src2Instruction(op, node, treg, s1reg, s2reg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src2Instruction(op, node, treg, s1reg, s2reg, cg);
   }

TR::Instruction *generateCondTrg1Src2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::ARM64ConditionCode cc, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64CondTrg1Src2Instruction(op, node, treg, s1reg, s2reg, cc, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64CondTrg1Src2Instruction(op, node, treg, s1reg, s2reg, cc, cg);
   }

TR::Instruction *generateCondTrg1Src2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::ARM64ConditionCode cc, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64CondTrg1Src2Instruction(op, node, treg, s1reg, s2reg, cc, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64CondTrg1Src2Instruction(op, node, treg, s1reg, s2reg, cc, cond, cg);
   }

TR::Instruction *generateTrg1Src2ShiftedInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg,
   TR::ARM64ShiftCode shiftType, uint32_t shiftAmount, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src2ShiftedInstruction(op, node, treg, s1reg, s2reg, shiftType, shiftAmount, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src2ShiftedInstruction(op, node, treg, s1reg, s2reg, shiftType, shiftAmount, cg);
   }

TR::Instruction *generateTrg1Src2ExtendtedInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg,
   TR::ARM64ExtendCode extendType, uint32_t shiftAmount, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src2ExtendedInstruction(op, node, treg, s1reg, s2reg, extendType, shiftAmount, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src2ExtendedInstruction(op, node, treg, s1reg, s2reg, extendType, shiftAmount, cg);
   }

TR::Instruction *generateTrg1Src3Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::Register *s3reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src3Instruction(op, node, treg, s1reg, s2reg, s3reg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src3Instruction(op, node, treg, s1reg, s2reg, s3reg, cg);
   }

TR::Instruction *generateTrg1Src3Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::Register *s3reg,
   TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src3Instruction(op, node, treg, s1reg, s2reg, s3reg, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src3Instruction(op, node, treg, s1reg, s2reg, s3reg, cond, cg);
   }

TR::Instruction *generateTrg1MemInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::MemoryReference *mr, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1MemInstruction(op, node, treg, mr, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1MemInstruction(op, node, treg, mr, cg);
   }

TR::Instruction *generateMemSrc1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::MemoryReference *mr, TR::Register *sreg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64MemSrc1Instruction(op, node, mr, sreg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64MemSrc1Instruction(op, node, mr, sreg, cg);
   }

TR::Instruction *generateMemSrc2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::MemoryReference *mr, TR::Register *s1reg, TR::Register *s2reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64MemSrc2Instruction(op, node, mr, s1reg, s2reg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64MemSrc2Instruction(op, node, mr, s1reg, s2reg, cg);
   }

TR::Instruction *generateTrg1MemSrc1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::MemoryReference *mr, TR::Register *sreg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1MemSrc1Instruction(op, node, treg, mr, sreg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1MemSrc1Instruction(op, node, treg, mr, sreg, cg);
   }

TR::Instruction *generateSrc1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *s1reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Src1Instruction(op, node, s1reg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Src1Instruction(op, node, s1reg, cg);
   }

TR::Instruction *generateSrc2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *s1reg, TR::Register *s2reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Src2Instruction(op, node, s1reg, s2reg, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Src2Instruction(op, node, s1reg, s2reg, cg);
   }

TR::Instruction *generateArithmeticShiftRightImmInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *treg, TR::Register *sreg, uint32_t shiftAmount, TR::Instruction *preced)
   {
   /* Alias of SBFM instruction */

   bool is64bit = node->getDataType().isInt64();
   TR_ASSERT_FATAL(shiftAmount < (is64bit ? 64 : 32), "Shift amount out of range.");

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::sbfmx : TR::InstOpCode::sbfmw;
   uint32_t imms = is64bit ? 0x3f : 0x1f;
   uint32_t immr = shiftAmount;
   uint32_t imm = (immr << 6) | imms;

   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, sreg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, sreg, imm, cg);
   }

TR::Instruction *generateLogicalShiftRightImmInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *treg, TR::Register *sreg, uint32_t shiftAmount, TR::Instruction *preced)
   {
   /* Alias of UBFM instruction */

   bool is64bit = node->getDataType().isInt64();
   TR_ASSERT_FATAL(shiftAmount < (is64bit ? 64 : 32), "Shift amount out of range.");

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::ubfmx : TR::InstOpCode::ubfmw;
   uint32_t imms = is64bit ? 0x3f : 0x1f;
   uint32_t immr = shiftAmount;
   uint32_t imm = (immr << 6) | imms;

   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, sreg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, sreg, imm, cg);
   }

TR::Instruction *generateLogicalShiftLeftImmInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *treg, TR::Register *sreg, uint32_t shiftAmount, TR::Instruction *preced)
   {
   /* Alias of UBFM instruction */

   bool is64bit = node->getDataType().isInt64();
   TR_ASSERT_FATAL(shiftAmount < (is64bit ? 64 : 32), "Shift amount out of range.");

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::Mnemonic::ubfmx : TR::InstOpCode::Mnemonic::ubfmw;
   uint32_t imms = (is64bit ? 63 : 31) - shiftAmount;
   uint32_t immr = imms + 1;
   uint32_t imm = (immr << 6) | imms;

   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, sreg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, sreg, imm, cg);
   }

TR::Instruction *generateLogicalImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op,
   TR::Node *node, TR::Register *treg, TR::Register *s1reg, bool N, uint32_t imm, TR::Instruction *preced)
   {
   TR::ARM64Trg1Src1ImmInstruction *intr;
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, treg, s1reg, N, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node,treg, s1reg, N, imm, cg);
   }

/* Use xzr as the target register */
static TR::Instruction *generateZeroSrc1ImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *sreg, int32_t imm, TR::Instruction *preced)
   {
   TR::Register *zeroReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *cond = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(cond, zeroReg, TR::RealRegister::xzr, TR_GPR, cg);

   TR::Instruction *instr =
      (preced) ?
      new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, zeroReg, sreg, imm, cond, preced, cg) :
      new (cg->trHeapMemory()) TR::ARM64Trg1Src1ImmInstruction(op, node, zeroReg, sreg, imm, cond, cg);

   cg->stopUsingRegister(zeroReg);

   return instr;
   }

TR::Instruction *generateCompareImmInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *sreg, int32_t imm, bool is64bit, TR::Instruction *preced)
   {
   /* Alias of SUBS instruction */

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::subsimmx : TR::InstOpCode::subsimmw;
   TR_ASSERT_FATAL(constantIsUnsignedImm12(imm), "Immediate value is out of range for subsimm");

   return generateZeroSrc1ImmInstruction(cg, op, node, sreg, imm, preced);
   }

TR::Instruction *generateTestImmInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *sreg, int32_t imm, bool is64bit, TR::Instruction *preced)
   {
   /* Alias of ANDS instruction */

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::andsimmx : TR::InstOpCode::andsimmw;

   return generateZeroSrc1ImmInstruction(cg, op, node, sreg, imm, preced);
   }

/* Use xzr as the target register */
static TR::Instruction *generateZeroSrc2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *s1reg, TR::Register *s2reg, TR::Instruction *preced)
   {
   TR::Register *zeroReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *cond = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(cond, zeroReg, TR::RealRegister::xzr, TR_GPR, cg);

   TR::Instruction *instr =
      (preced) ?
      new (cg->trHeapMemory()) TR::ARM64Trg1Src2Instruction(op, node, zeroReg, s1reg, s2reg, cond, preced, cg) :
      new (cg->trHeapMemory()) TR::ARM64Trg1Src2Instruction(op, node, zeroReg, s1reg, s2reg, cond, cg);

   cg->stopUsingRegister(zeroReg);

   return instr;
   }

TR::Instruction *generateCompareInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *s1reg, TR::Register *s2reg, bool is64bit, TR::Instruction *preced)
   {
   /* Alias of SUBS instruction */

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::subsx : TR::InstOpCode::subsw;

   return generateZeroSrc2Instruction(cg, op, node, s1reg, s2reg, preced);
   }

TR::Instruction *generateTestInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *s1reg, TR::Register *s2reg, bool is64bit, TR::Instruction *preced)
   {
   /* Alias of ANDS instruction */

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::andsx : TR::InstOpCode::andsw;

   return generateZeroSrc2Instruction(cg, op, node, s1reg, s2reg, preced);
   }

/* Use xzr as the first source register */
static TR::Instruction *generateTrg1ZeroSrc1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *node,
   TR::Register *treg, TR::Register *sreg, TR::Instruction *preced)
   {
   TR::Register *zeroReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *cond = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(cond, zeroReg, TR::RealRegister::xzr, TR_GPR, cg);

   TR::Instruction *instr =
      (preced) ?
      new (cg->trHeapMemory()) TR::ARM64Trg1Src2Instruction(op, node, treg, zeroReg, sreg, cond, preced, cg) :
      new (cg->trHeapMemory()) TR::ARM64Trg1Src2Instruction(op, node, treg, zeroReg, sreg, cond, cg);

   cg->stopUsingRegister(zeroReg);

   return instr;
   }

TR::Instruction *generateMovInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *treg, TR::Register *sreg, bool is64bit, TR::Instruction *preced)
   {
   /* Alias of ORR instruction */

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::orrx : TR::InstOpCode::orrw;

   return generateTrg1ZeroSrc1Instruction(cg, op, node, treg, sreg, preced);
   }

TR::Instruction *generateNegInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *treg, TR::Register *sreg, bool is64bit, TR::Instruction *preced)
   {
   /* Alias of SUB instruction */

   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::subx : TR::InstOpCode::subw;

   return generateTrg1ZeroSrc1Instruction(cg, op, node, treg, sreg, preced);
   }

TR::Instruction *generateMulInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::Instruction *preced)
   {
   /* Alias of MADD instruction */

   bool is64bit = node->getDataType().isInt64();
   TR::InstOpCode::Mnemonic op = is64bit ? TR::InstOpCode::maddx : TR::InstOpCode::maddw;

   /* Use xzr as the third source register */
   TR::Register *zeroReg = cg->allocateRegister();
   TR::RegisterDependencyConditions *cond = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(cond, zeroReg, TR::RealRegister::xzr, TR_GPR, cg);

   TR::Instruction *instr =
      (preced) ?
      new (cg->trHeapMemory()) TR::ARM64Trg1Src3Instruction(op, node, treg, s1reg, s2reg, zeroReg, cond, preced, cg) :
      new (cg->trHeapMemory()) TR::ARM64Trg1Src3Instruction(op, node, treg, s1reg, s2reg, zeroReg, cond, cg);

   cg->stopUsingRegister(zeroReg);

   return instr;
   }

TR::Instruction *generateCSetInstruction(TR::CodeGenerator *cg, TR::Node *node,
   TR::Register *treg, TR::ARM64ConditionCode cc, TR::Instruction *preced)
   {
   /* Alias of CSINC instruction with inverted condition code */
   TR::InstOpCode::Mnemonic op = TR::InstOpCode::csincx;

   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64Trg1CondInstruction(op, node, treg, cc_invert(cc), preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64Trg1CondInstruction(op, node, treg, cc_invert(cc), cg);
   }

TR::ARM64SynchronizationInstruction *generateSynchronizationInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op,
   TR::Node *node, uint32_t imm, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64SynchronizationInstruction(op, node, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64SynchronizationInstruction(op, node, imm, cg);
   }

TR::ARM64ExceptionInstruction *generateExceptionInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op,
   TR::Node *node, uint32_t imm, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64ExceptionInstruction(op, node, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64ExceptionInstruction(op, node, imm, cg);
   }

#ifdef J9_PROJECT_SPECIFIC
TR::Instruction *generateVirtualGuardNOPInstruction(TR::CodeGenerator *cg,  TR::Node *n, TR_VirtualGuardSite *site,
   TR::RegisterDependencyConditions *cond, TR::LabelSymbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARM64VirtualGuardNOPInstruction(n, site, cond, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::ARM64VirtualGuardNOPInstruction(n, site, cond, sym, cg);
   }
#endif
