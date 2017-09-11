/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef GENERATE_INSTRUCTIONS_INCL
#define GENERATE_INSTRUCTIONS_INCL

#include "il/TreeTop.hpp"
#include "codegen/ARMInstruction.hpp"

TR::Instruction *generateInstruction(TR::CodeGenerator *cg,
                                    TR_ARMOpCodes     op,
                                    TR::Node          *node,
                                    TR::Instruction   *prev = NULL);

TR::Instruction *generateAdminInstruction(TR::CodeGenerator *cg,
                                         TR_ARMOpCodes     op,
                                         TR::Node          *node,
                                         TR::Node          *fenceNode = NULL,
                                         TR::Instruction   *prev = NULL);

TR::Instruction *generateAdminInstruction(TR::CodeGenerator                   *cg,
                                         TR_ARMOpCodes                       op,
                                         TR::Node                            *node,
                                         TR::RegisterDependencyConditions *cond,
                                         TR::Node                            *fenceNode = NULL,
                                         TR::Instruction                     *prev = NULL);

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR::RegisterDependencyConditions *cond,
                                          TR::Instruction                     *prev = NULL);

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR::Instruction                     *prev = NULL);

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR_ExternalRelocationTargetKind relocationKind,
                                          TR::Instruction                     *prev = NULL);

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR_ExternalRelocationTargetKind relocationKind,
                                          TR::SymbolReference              *sr,
                                          TR::Instruction                     *prev = NULL);

TR::Instruction *generateImmSymInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR::RegisterDependencyConditions *cond,
                                          TR::SymbolReference                 *sr,
                                          TR::Snippet                         *s = NULL,
                                          TR::Instruction                     *prev = NULL,
                                          TR_ARMConditionCode                cc = ARMConditionCodeAL);

TR::Instruction *generateMemSrc1Instruction(TR::CodeGenerator      *cg,
                                           TR_ARMOpCodes          op,
                                           TR::Node               *node,
                                           TR::MemoryReference *mf,
                                           TR::Register           *sreg,
                                           TR::Instruction        *prev = NULL);

TR::Instruction *generateTrg1MemInstruction(TR::CodeGenerator      *cg,
                                           TR_ARMOpCodes          op,
                                           TR::Node               *node,
                                           TR::Register           *treg,
                                           TR::MemoryReference *mf,
                                           TR::Instruction        *prev = NULL);

TR::Instruction *generateTrg1MemSrc1Instruction(TR::CodeGenerator      *cg,
                                               TR_ARMOpCodes          op,
                                               TR::Node               *node,
                                               TR::Register           *treg,
                                               TR::MemoryReference *mf,
                                               TR::Register           *sreg,
                                               TR::Instruction        *prev = NULL);

TR::Instruction *generateTrg1ImmInstruction(TR::CodeGenerator *cg,
                                           TR_ARMOpCodes     op,
                                           TR::Node          *node,
                                           TR::Register      *treg,
                                           uint32_t          base,
                                           uint32_t          rotate,
                                           TR::Instruction   *prev = NULL);

TR::Instruction *generateSrc1ImmInstruction(TR::CodeGenerator *cg,
                                           TR_ARMOpCodes     op,
                                           TR::Node          *node,
                                           TR::Register      *s1reg,
                                           uint32_t          base,
                                           uint32_t          rotate,
                                           TR::Instruction   *prev = NULL);

TR::Instruction *generateSrc2Instruction(TR::CodeGenerator *cg,
                                        TR_ARMOpCodes     op,
                                        TR::Node          *node,
                                        TR::Register      *s1reg,
                                        TR::Register      *s2reg,
                                        TR::Instruction   *prev = NULL);

TR::Instruction *generateTrg1Src1Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR_ARMOperand2   *s1op,
                                            TR::Instruction   *prev = NULL);

TR::Instruction *generateTrg1Src1Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR::Register      *s1reg,
                                            TR::Instruction   *prev = NULL);

TR::Instruction *generateTrg1Src1ImmInstruction(TR::CodeGenerator *cg,
                                               TR_ARMOpCodes     op,
                                               TR::Node          *node,
                                               TR::Register      *treg,
                                               TR::Register      *s1reg,
                                               uint32_t          base,
                                               uint32_t          rotate,
                                               TR::Instruction   *prev = NULL);

TR::Instruction *generateLoadStartPCInstruction(TR::CodeGenerator *cg,
                                               TR::Node          *node,
                                               TR::Register      *treg,
                                               TR::SymbolReference                 *sr,
                                               TR::Instruction   *prev = NULL);

TR::Instruction *generateTrg1Src2Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR::Register      *s1reg,
                                            TR_ARMOperand2   *s2op,
                                            TR::Instruction   *prev = NULL);

TR::Instruction *generateTrg1Src2Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR::Register      *s1reg,
                                            TR::Register      *s2reg,
                                            TR::Instruction   *prev = NULL);

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
TR::Instruction *generateTrg2Src1Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *t1reg,
                                            TR::Register      *t2reg,
                                            TR::Register      *s1reg,
                                            TR::Instruction   *prev = NULL);
#endif

TR::Instruction *generateTrg1Src2MulInstruction(TR::CodeGenerator *cg,
                                               TR_ARMOpCodes     op,
                                               TR::Node          *node,
                                               TR::Register      *treg,
                                               TR::Register      *s1reg,
                                               TR::Register      *s2reg,
                                               TR::Instruction   *prev = NULL);

TR::Instruction *generateTrg2Src2MulInstruction(TR::CodeGenerator *cg,
                                               TR_ARMOpCodes     op,
                                               TR::Node          *node,
                                               TR::Register      *tregHi,
                                               TR::Register      *tregLo,
                                               TR::Register      *s1reg,
                                               TR::Register      *s2reg,
                                               TR::Instruction   *prev = NULL);

TR::Instruction *generateShiftLeftImmediate(TR::CodeGenerator *cg,
                                           TR_ARMOperand2Type type,
                                           TR::Node          *node,
                                           TR::Register      *trgReg,
                                           TR::Register      *srcReg,
                                           int32_t           shiftAmount,
                                           TR::Instruction   *prev = NULL);

static inline TR::Instruction *generateShiftLeftImmediate(
   TR::CodeGenerator *cg, TR::Node *node, TR::Register *trgReg, TR::Register *srcReg, int32_t shiftAmount, TR::Instruction *prev = NULL)
   {
   return generateShiftLeftImmediate(cg, ARMOp2RegLSLImmed, node, trgReg, srcReg, shiftAmount, prev);
   }

TR::Instruction *generateShiftLeftByRegister(TR::CodeGenerator *cg,
                                            TR::Node          *node,
                                            TR::Register      *trgReg,
                                            TR::Register      *srcReg,
                                            TR::Register      *shiftReg,
                                            TR::Instruction   *prev = NULL);

TR::Instruction *generateShiftRightImmediate(TR::CodeGenerator *cg,
                                            TR::Node          *node,
                                            TR::Register      *trgReg,
                                            TR::Register      *srcReg,
                                            int32_t           shiftAmount,
                                            bool              isLogical,
                                            TR::Instruction   *prev = NULL);

TR::Instruction *generateShiftRightByRegister(TR::CodeGenerator *cg,
                                             TR::Node          *node,
                                             TR::Register      *trgReg,
                                             TR::Register      *srcReg,
                                             TR::Register      *shiftRegister,
                                             bool              isLogical,
                                             TR::Instruction   *prev = NULL);

static inline TR::Instruction *generateShiftRightLogicalImmediate(TR::CodeGenerator *cg,
                                                          TR::Node          *node,
                                                          TR::Register      *trgReg,
                                                          TR::Register      *srcReg,
                                                          int32_t           shiftAmount,
                                                          TR::Instruction   *prev = NULL)
   {
   return generateShiftRightImmediate(cg, node, trgReg, srcReg, shiftAmount, true, prev);
   }

inline TR::Instruction *generateShiftRightArithmeticImmediate(TR::CodeGenerator *cg,
                                                             TR::Node          *node,
                                                             TR::Register      *trgReg,
                                                             TR::Register      *srcReg,
                                                             int32_t           shiftAmount,
                                                             TR::Instruction   *prev = NULL)
   {
   return generateShiftRightImmediate(cg, node, trgReg, srcReg, shiftAmount, false, prev);
   }

inline TR::Instruction *generateShiftRightLogicalByRegister(TR::CodeGenerator *cg,
                                                           TR::Node          *node,
                                                           TR::Register      *trgReg,
                                                           TR::Register      *srcReg,
                                                           TR::Register      *shiftRegister,
                                                           TR::Instruction   *prev = NULL)
   {
   return generateShiftRightByRegister(cg, node, trgReg, srcReg, shiftRegister, true, prev);
   }

inline TR::Instruction *generateShiftRightArithmeticByRegister(TR::CodeGenerator *cg,
                                                              TR::Node          *node,
                                                              TR::Register      *trgReg,
                                                              TR::Register      *srcReg,
                                                              TR::Register      *shiftRegister,
                                                              TR::Instruction   *prev = NULL)
   {
   return generateShiftRightByRegister(cg, node, trgReg, srcReg, shiftRegister, false, prev);
   }


TR::Instruction *generateLabelInstruction(TR::CodeGenerator *cg,
                                         TR_ARMOpCodes     op,
                                         TR::Node          *node,
                                         TR::LabelSymbol    *sym,
                                         TR::Instruction   *prev = NULL,
                                         TR::Register      *trgReg = NULL,
                                         TR::Register      *src1Reg = NULL);

TR::Instruction *generateLabelInstruction(TR::CodeGenerator                   *cg,
                                         TR_ARMOpCodes                       op,
                                         TR::Node                            *node,
                                         TR::LabelSymbol                      *sym,
                                         TR::RegisterDependencyConditions *cond,
                                         TR::Instruction                     *prev = NULL);

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator    *cg,
                                                     TR::Node             *node,
                                                     TR_ARMConditionCode  cc,
                                                     TR::LabelSymbol       *sym,
                                                     TR::Instruction      *prev = NULL);

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator                   *cg,
                                                     TR::Node                            *node,
                                                     TR_ARMConditionCode                 cc,
                                                     TR::LabelSymbol                      *sym,
                                                     TR::RegisterDependencyConditions *cond,
                                                     TR::Instruction                     *prev = NULL);

TR::ARMControlFlowInstruction *generateControlFlowInstruction(TR::CodeGenerator                   *cg,
                                                             TR_ARMOpCodes                       op,
                                                             TR::Node                            *node,
                                                             TR::RegisterDependencyConditions *cond);

TR::Instruction *generatePreIncLoadInstruction(TR::CodeGenerator *cg,
                                              TR::Node          *node,
                                              TR::Register      *treg,
                                              TR::Register      *baseReg,
                                              uint32_t          stride,
                                              TR::Instruction   *prev = NULL);

TR::Instruction *generateVirtualGuardNOPInstruction(TR::CodeGenerator *cg,  TR::Node *n, TR_VirtualGuardSite *site,
   TR::RegisterDependencyConditions *cond, TR::LabelSymbol *sym, TR::Instruction *preced = NULL);

#endif
