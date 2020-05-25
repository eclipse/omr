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
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Snippet.hpp"
#include "env/jittypes.h"
#include "runtime/Runtime.hpp"

class TR_VirtualGuardSite;
namespace TR { class CodeGenerator; }
namespace TR { class LabelSymbol; }
namespace TR { class MemoryReference; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class SymbolReference; }

//Flags to be passed into generateMvFprGprInstructions to select the type of move.
enum MvFprGprMode
   {
   fpr2gprHost64,  //Move a 64-bit fpr to a 64-bit gpr under a 64-bit OS.
   fpr2gprHost32,  //Move a 64-bit fpr to a pair of 32-bit gpr's under a 32-bit OS.
   fpr2gprLow,     //Move the lower 32 bits of a 64-bit fpr to the lower 32 bits of a gpr.  With 64-bit gpr's, upper half of gpr set to zero.
   fpr2gprSp,      //Move the single precision value of an fpr to the lower 32 bits of a gpr.  With 64-bit gpr's, upper half of gpr set to zero.
   gpr2fprHost64,  //Move a 64-bit gpr to a 64-bit fpr under a 64-bit OS.
   gpr2fprHost32,  //Move a pair of 32-bit gpr's to a 64-bit fpr under a 32-bit OS.
   gprSp2fpr,      //Move a 32-bit gpr to an fpr, treating the gpr as if it contained a single precision float.
   gprLow2fpr};    //Move a 32-bit gpr to the lower 32 bits of an fpr and set the upper 32 bits of the fpr to a sign extension of the lower bits.

//Generates the optimal sequence for each microarchitecture for moving between an fpr and gpr.
TR::Instruction *generateMvFprGprInstructions(
                   TR::CodeGenerator *cg,
                   TR::Node *node,
                   MvFprGprMode mode,
                   bool nonops,
                   TR::Register *reg0,
                   TR::Register *reg1,
                   TR::Register *reg2 = NULL,
                   TR::Register *reg3 = NULL,
                   TR::Instruction *cursor = 0);

TR::Instruction *generateInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node               *n,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateAlignmentNopInstruction(
                   TR::CodeGenerator *cg,
                   TR::Node *n,
                   uint32_t alignment,
                   TR::Instruction *preced = 0);

TR::Instruction *generateImmInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node               *n,
                   uint32_t               imm,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateImmInstruction(
                   TR::CodeGenerator                *cg,
                   TR::InstOpCode::Mnemonic                   op,
                   TR::Node                         *n,
                   uint32_t                        imm,
                   TR_ExternalRelocationTargetKind relocationKind,
                   TR::Instruction                  *preced = 0);

TR::Instruction *generateImmInstruction(
                   TR::CodeGenerator                *cg,
                   TR::InstOpCode::Mnemonic                   op,
                   TR::Node                         *n,
                   uint32_t                        imm,
                   TR_ExternalRelocationTargetKind relocationKind,
                   TR::SymbolReference              *sr,
                   TR::Instruction                  *preced = 0);

TR::Instruction *generateImm2Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node               *n,
                   uint32_t               imm,
                   uint32_t               imm2,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateDepInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateMemSrc1Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node               *n,
                   TR::MemoryReference *mf,
                   TR::Register           *sreg,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateTrg1MemInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::MemoryReference *mf,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateTrg1MemInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic          op,
                   int32_t                hint,
                   TR::Node               *n,
                   TR::Register           *treg,
                   TR::MemoryReference *mf,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateMemInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic          op,
                   TR::Node               *n,
                   TR::MemoryReference *mf,
                   TR::Instruction        *preced = 0);

TR::Instruction *generateTrg1ImmInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   uint32_t        imm,
                   TR::Instruction *preced = 0);

TR::Instruction *generateLabelInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::LabelSymbol *sym,
                   TR::Instruction *preced = 0);

TR::Instruction *generateDepLabelInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   TR::LabelSymbol                      *sym,
                   TR::RegisterDependencyConditions *cond,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateConditionalBranchInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::LabelSymbol *sym,
                   TR::Register    *cr,
                   TR::Instruction *preced = 0);

TR::Instruction *generateConditionalBranchInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   bool likeliness,
                   TR::Node        *n,
                   TR::LabelSymbol *sym,
                   TR::Register    *cr,
                   TR::Instruction *preced = 0);

TR::Instruction *generateDepConditionalBranchInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic                        op,
                   TR::Node                             *n,
                   TR::LabelSymbol                       *sym,
                   TR::Register                         *cr,
                   TR::RegisterDependencyConditions  *cond,
                   TR::Instruction                      *preced = 0);

TR::Instruction *generateDepConditionalBranchInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic                        op,
                   bool likeliness,
                   TR::Node                             *n,
                   TR::LabelSymbol                       *sym,
                   TR::Register                         *cr,
                   TR::RegisterDependencyConditions  *cond,
                   TR::Instruction                      *preced = 0);

TR::Instruction *generateSrc1Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *s1reg,
                   int32_t         imm=0,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src1ImmInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   intptr_t       imm,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src1ImmInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   TR::Register    *cr0reg,
                   int32_t         imm,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src1Imm2Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   int32_t         imm1,
                   int64_t         imm2,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src1Imm2Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   TR::Register    *cr0reg,
                   int32_t         imm1,
                   int64_t         imm2,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src1Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src2Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   TR::Register    *s2reg,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src2Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   TR::Register    *s2reg,
                   TR::Register    *cr0reg,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src2ImmInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   TR::Register    *s2reg,
                   int64_t         imm,
                   TR::Instruction *preced = 0);

TR::Instruction *generateTrg1Src3Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *treg,
                   TR::Register    *s1reg,
                   TR::Register    *s2reg,
                   TR::Register    *s3reg,
                   TR::Instruction *preced = 0);

TR::Instruction *generateSrc2Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *s1reg,
                   TR::Register    *s2reg,
                   TR::Instruction *preced = 0);

TR::Instruction *generateShiftLeftImmediate(
                   TR::CodeGenerator      *cg,
                   TR::Node        *n,
                   TR::Register    *trgReg,
                   TR::Register    *srcReg,
                   int32_t         shiftAmount,
                   TR::Instruction *preced = 0);

TR::Instruction *generateShiftRightLogicalImmediate(
                   TR::CodeGenerator      *cg,
                   TR::Node        *n,
                   TR::Register    *trgReg,
                   TR::Register    *srcReg,
                   int32_t         shiftAmount,
                   TR::Instruction *preced = 0);

TR::Instruction *generateShiftLeftImmediateLong(
                   TR::CodeGenerator      *cg,
                   TR::Node        *n,
                   TR::Register    *trgReg,
                   TR::Register    *srcReg,
                   int32_t         shiftAmount,
                   TR::Instruction *preced = 0);

TR::Instruction *generateShiftRightLogicalImmediateLong(
                   TR::CodeGenerator      *cg,
                   TR::Node        *n,
                   TR::Register    *trgReg,
                   TR::Register    *srcReg,
                   int32_t         shiftAmount,
                   TR::Instruction *preced = 0);

TR::Instruction *generateControlFlowInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   TR::RegisterDependencyConditions *deps=NULL,
                   TR::Instruction                     *preced = 0,
                   bool                                useRegPairForResult = false,
                   bool                                useRegPairForCond = false);

TR::Instruction *generateAdminInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Node        *fenceNode=NULL,
                   TR::Instruction *preced = 0);

TR::Instruction *generateDepImmSymInstruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic                       op,
                   TR::Node                            *n,
                   uintptr_t                           imm,
                   TR::RegisterDependencyConditions *cond,
                   TR::SymbolReference                 *sr,
                   TR::Snippet                         *s=NULL,
                   TR::Instruction                     *preced = 0);

TR::Instruction *generateTrg1Instruction(
                   TR::CodeGenerator      *cg,
                   TR::InstOpCode::Mnemonic   op,
                   TR::Node        *n,
                   TR::Register    *trg,
                   TR::Instruction *preced = 0);

void generateZeroExtendInstruction(
                   TR::Node *node, 
                   TR::Register *trgReg, 
                   TR::Register *srcReg, 
                   int32_t bitsInTarget, 
                   TR::CodeGenerator *cg);

void generateSignExtendInstruction(
                   TR::Node *node,
                   TR::Register *trgReg,
                   TR::Register *srcReg,
                   TR::CodeGenerator *cg,
                   int32_t operandSize);

void generateSignExtendInstruction(
                   TR::Node *node,
                   TR::Register *trgReg,
                   TR::Register *srcReg,
                   TR::CodeGenerator *cg);
