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

#include "codegen/ARMInstruction.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"


TR::Instruction *generateInstruction(TR::CodeGenerator *cg,
                                    TR_ARMOpCodes     op,
                                    TR::Node          *node,
                                    TR::Instruction   *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::Instruction(prev, op, node, cg);
   else
      return new (cg->trHeapMemory()) TR::Instruction(op, node, cg);
   }

TR::Instruction *generateAdminInstruction(TR::CodeGenerator *cg,
                                         TR_ARMOpCodes     op,
                                         TR::Node          *node,
                                         TR::Node          *fenceNode,
                                         TR::Instruction   *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMAdminInstruction(prev, op, node, fenceNode, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMAdminInstruction(op, node, fenceNode, cg);
   }

TR::Instruction *generateAdminInstruction(TR::CodeGenerator                   *cg,
                                         TR_ARMOpCodes                       op,
                                         TR::Node                            *node,
                                         TR::RegisterDependencyConditions *cond,
                                         TR::Node                            *fenceNode,
                                         TR::Instruction                     *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMAdminInstruction(prev, op, node, fenceNode, cond, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMAdminInstruction(op, node, fenceNode, cond, cg);
   }

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR::RegisterDependencyConditions *cond,
                                          TR::Instruction                     *prev)

   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(prev, op, node, cond, imm, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(op, node, cond, imm, cg);
   }

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR::Instruction                     *prev)

   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(prev, op, node, imm, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(op, node, imm, cg);
   }

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR_ExternalRelocationTargetKind relocationKind,
                                          TR::Instruction                     *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(prev, op, node, imm, relocationKind, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(op, node, imm, relocationKind, cg);
   }

TR::Instruction *generateImmInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR_ExternalRelocationTargetKind relocationKind,
                                          TR::SymbolReference              *sr,
                                          TR::Instruction                     *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(prev, op, node, imm, relocationKind, sr, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMImmInstruction(op, node, imm, relocationKind, sr, cg);
   }

TR::Instruction *generateImmSymInstruction(TR::CodeGenerator                   *cg,
                                          TR_ARMOpCodes                       op,
                                          TR::Node                            *node,
                                          uint32_t                            imm,
                                          TR::RegisterDependencyConditions *cond,
                                          TR::SymbolReference                 *sr,
                                          TR::Snippet                         *s,
                                          TR::Instruction                     *prev,
                                          TR_ARMConditionCode                 cc)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMImmSymInstruction(prev, op, node, imm, cond, sr, cg, s, cc);
   else
      return new (cg->trHeapMemory()) TR::ARMImmSymInstruction(op, node, imm, cond, sr, cg, s, cc);
   }

TR::Instruction *generateMemSrc1Instruction(TR::CodeGenerator      *cg,
                                           TR_ARMOpCodes          op,
                                           TR::Node               *node,
                                           TR::MemoryReference *mf,
                                           TR::Register           *sreg,
                                           TR::Instruction        *prev)
   {
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR_ARMOpCode    opCode(op);
   if (opCode.isVFPOp())
      {
      mf->fixupVFPOffset(node, cg);
      }
#endif

   if (prev)
      return new (cg->trHeapMemory()) TR::ARMMemSrc1Instruction(prev, op, node, mf, sreg, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMMemSrc1Instruction(op, node, mf, sreg, cg);
   }

TR::Instruction *generateTrg1MemInstruction(TR::CodeGenerator      *cg,
                                           TR_ARMOpCodes          op,
                                           TR::Node               *node,
                                           TR::Register           *treg,
                                           TR::MemoryReference *mf,
                                           TR::Instruction        *prev)
   {
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   TR_ARMOpCode    opCode(op);
   if (opCode.isVFPOp())
      {
      mf->fixupVFPOffset(node, cg);
      }
#endif

   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg1MemInstruction(prev, op, node, treg, mf, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg1MemInstruction(op, node, treg, mf, cg);
   }

TR::Instruction *generateTrg1MemSrc1Instruction(TR::CodeGenerator      *cg,
                                               TR_ARMOpCodes          op,
                                               TR::Node               *node,
                                               TR::Register           *treg,
                                               TR::MemoryReference *mf,
                                               TR::Register           *sreg,
                                               TR::Instruction        *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg1MemSrc1Instruction(prev, op, node, treg, mf, sreg, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg1MemSrc1Instruction(op, node, treg, mf, sreg, cg);
   }

TR::Instruction *generateTrg1ImmInstruction(TR::CodeGenerator *cg,
                                           TR_ARMOpCodes     op,
                                           TR::Node          *node,
                                           TR::Register      *treg,
                                           uint32_t          base,
                                           uint32_t          rotate,
                                           TR::Instruction   *prev)
   {
   TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(base, rotate);
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(prev, op, node, treg, operand, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(op, node, treg, operand, cg);
   }

TR::Instruction *generateSrc1ImmInstruction(TR::CodeGenerator *cg,
                                           TR_ARMOpCodes     op,
                                           TR::Node          *node,
                                           TR::Register      *s1reg,
                                           uint32_t          base,
                                           uint32_t          rotate,
                                           TR::Instruction   *prev)
   {
   TR_ARMOpCode    opCode(op);
   TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(base, rotate);
   if (opCode.isVFPOp())
      {
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(prev, op, node, s1reg, operand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(op, node, s1reg, operand, cg);
      }
   else
      {
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMSrc2Instruction(prev, op, node, s1reg, operand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMSrc2Instruction(op, node, s1reg, operand, cg);
      }
   }

TR::Instruction *generateSrc2Instruction(TR::CodeGenerator *cg,
                                        TR_ARMOpCodes     op,
                                        TR::Node          *node,
                                        TR::Register      *s1reg,
                                        TR::Register      *s2reg,
                                        TR::Instruction   *prev)
   {
   TR_ARMOpCode    opCode(op);
   TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, s2reg);
   if (opCode.isVFPOp())
      {
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(prev, op, node, s1reg, operand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(op, node, s1reg, operand, cg);
      }
   else
      {
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMSrc2Instruction(prev, op, node, s1reg, operand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMSrc2Instruction(op, node, s1reg, operand, cg);
      }
   }

TR::Instruction *generateTrg1Src1Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR_ARMOperand2   *s1op,
                                            TR::Instruction   *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(prev, op, node, treg, s1op, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(op, node, treg, s1op, cg);
   }

TR::Instruction *generateTrg1Src1Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR::Register      *s1reg,
                                            TR::Instruction   *prev)
   {
   if (op == ARMOp_fmrs || op == ARMOp_fmsr)
      {
      TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(0, 0);
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(prev, op, node, (op==ARMOp_fmrs)?treg:s1reg, (op==ARMOp_fmrs)?s1reg:treg, operand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(op, node, (op==ARMOp_fmrs)?treg:s1reg, (op==ARMOp_fmrs)?s1reg:treg, operand, cg);
      }
   else
      {
      TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, s1reg);
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(prev, op, node, treg, operand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMTrg1Src1Instruction(op, node, treg, operand, cg);
      }
   }

TR::Instruction *generateTrg1Src1ImmInstruction(TR::CodeGenerator *cg,
                                               TR_ARMOpCodes     op,
                                               TR::Node          *node,
                                               TR::Register      *treg,
                                               TR::Register      *s1reg,
                                               uint32_t          base,
                                               uint32_t          rotate,
                                               TR::Instruction   *prev)
   {
   TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(base, rotate);
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(prev, op, node, treg, s1reg, operand, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(op, node, treg, s1reg, operand, cg);
   }

TR::Instruction *generateLoadStartPCInstruction(TR::CodeGenerator *cg,
                                               TR::Node          *node,
                                               TR::Register      *treg,
                                               TR::SymbolReference                 *sr,
                                               TR::Instruction   *prev = NULL)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMLoadStartPCInstruction(prev, node, treg, sr, cg);
   else
	   return new (cg->trHeapMemory()) TR::ARMLoadStartPCInstruction(node, treg, sr, cg);
   }

TR::Instruction *generateTrg1Src2Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR::Register      *s1reg,
                                            TR_ARMOperand2   *s2op,
                                            TR::Instruction   *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(prev, op, node, treg, s1reg, s2op, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(op, node, treg, s1reg, s2op, cg);
   }

TR::Instruction *generateTrg1Src2Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *treg,
                                            TR::Register      *s1reg,
                                            TR::Register      *s2reg,
                                            TR::Instruction   *prev)
   {
   if (op == ARMOp_fmdrr)
      {
      // fmdrr   Dm, Rd, Rn
      TR_ARMOperand2 *toperand = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, treg);
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(prev, op, node, s1reg, s2reg, toperand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(op, node, s1reg, s2reg, toperand, cg);
      }
   else
      {
      TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2Reg, s2reg);
      if (prev)
         return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(prev, op, node, treg, s1reg, operand, cg);
      else
         return new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(op, node, treg, s1reg, operand, cg);
      }
   }

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
TR::Instruction *generateTrg2Src1Instruction(TR::CodeGenerator *cg,
                                            TR_ARMOpCodes     op,
                                            TR::Node          *node,
                                            TR::Register      *t1reg,
                                            TR::Register      *t2reg,
                                            TR::Register      *sreg,
                                            TR::Instruction   *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg2Src1Instruction(prev, op, node, t1reg, t2reg, sreg, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg2Src1Instruction(op, node, t1reg, t2reg, sreg, cg);
   }
#endif

TR::Instruction *generateTrg1Src2MulInstruction(TR::CodeGenerator *cg,
                                               TR_ARMOpCodes     op,
                                               TR::Node          *node,
                                               TR::Register      *treg,
                                               TR::Register      *s1reg,
                                               TR::Register      *s2reg,
                                               TR::Instruction   *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMMulInstruction(prev, op, node, treg, s1reg, s2reg, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMMulInstruction(op, node, treg, s1reg, s2reg, cg);
   }

TR::Instruction *generateTrg2Src2MulInstruction(TR::CodeGenerator *cg,
                                               TR_ARMOpCodes     op,
                                               TR::Node          *node,
                                               TR::Register       *tregHi,
                                               TR::Register      *tregLo,
                                               TR::Register      *s1reg,
                                               TR::Register      *s2reg,
                                               TR::Instruction   *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMMulInstruction(prev, op, node, tregHi, tregLo, s1reg, s2reg, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMMulInstruction(op, node, tregHi, tregLo, s1reg, s2reg, cg);
   }

TR::Instruction *generateShiftLeftImmediate(TR::CodeGenerator  *cg,
                                           TR_ARMOperand2Type type,
                                           TR::Node           *node,
                                           TR::Register       *trgReg,
                                           TR::Register       *srcReg,
                                           int32_t            shiftAmount,
                                           TR::Instruction    *prev)
   {
   TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(type, srcReg, shiftAmount);
   return generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, operand, prev);
   }

TR::Instruction *generateShiftLeftByRegister(TR::CodeGenerator *cg,
                                            TR::Node          *node,
                                            TR::Register      *trgReg,
                                            TR::Register      *srcReg,
                                            TR::Register      *shiftRegister,
                                            TR::Instruction   *prev)
   {
   TR_ARMOperand2 *operand = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLReg, srcReg, shiftRegister);
   return generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, operand, prev);
   }

TR::Instruction *generateShiftRightImmediate(TR::CodeGenerator *cg,
                                            TR::Node          *node,
                                            TR::Register      *trgReg,
                                            TR::Register      *srcReg,
                                            int32_t           shiftAmount,
                                            bool              isLogical,
                                            TR::Instruction   *prev)
   {
   TR_ARMOperand2Type  type    = (isLogical == true ? ARMOp2RegLSRImmed : ARMOp2RegASRImmed);
   TR_ARMOperand2     *operand = new (cg->trHeapMemory()) TR_ARMOperand2(type, srcReg, shiftAmount);
   return generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, operand, prev);
   }

TR::Instruction *generateShiftRightByRegister(TR::CodeGenerator *cg,
                                             TR::Node          *node,
                                             TR::Register      *trgReg,
                                             TR::Register      *srcReg,
                                             TR::Register      *shiftRegister,
                                             bool           isLogical,
                                             TR::Instruction   *prev)
   {
   TR_ARMOperand2Type  type    = (isLogical == true ? ARMOp2RegLSRReg : ARMOp2RegASRReg);
   TR_ARMOperand2     *operand = new (cg->trHeapMemory()) TR_ARMOperand2(type, srcReg, shiftRegister);
   return generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, operand, prev);
   }

TR::Instruction *generateLabelInstruction(TR::CodeGenerator *cg,
                                         TR_ARMOpCodes     op,
                                         TR::Node          *node,
                                         TR::LabelSymbol    *sym,
                                         TR::Instruction   *prev,
                                         TR::Register      *trgReg,
                                         TR::Register      *src1Reg)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMLabelInstruction(prev, op, node, sym, cg, trgReg, src1Reg);
   else
      return new (cg->trHeapMemory()) TR::ARMLabelInstruction(op, node, sym, cg, trgReg, src1Reg);
   }

TR::Instruction *generateLabelInstruction(TR::CodeGenerator                   *cg,
                                         TR_ARMOpCodes                       op,
                                         TR::Node                            *node,
                                         TR::LabelSymbol                      *sym,
                                         TR::RegisterDependencyConditions *cond,
                                         TR::Instruction                     *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMLabelInstruction(prev, op, node, cond, sym, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMLabelInstruction(op, node, cond, sym, cg);
   }

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator    *cg,
                                                     TR::Node             *node,
                                                     TR_ARMConditionCode  cc,
                                                     TR::LabelSymbol       *sym,
                                                     TR::Instruction      *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMConditionalBranchInstruction(prev, ARMOp_b, node, sym, cc, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMConditionalBranchInstruction(ARMOp_b, node, sym, cc, cg);
   }

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator                   *cg,
                                                     TR::Node                            *node,
                                                     TR_ARMConditionCode                 cc,
                                                     TR::LabelSymbol                      *sym,
                                                     TR::RegisterDependencyConditions *cond,
                                                     TR::Instruction                     *prev)
   {
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMConditionalBranchInstruction(prev, ARMOp_b, node, cond, sym, cc, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMConditionalBranchInstruction(ARMOp_b, node, cond, sym, cc, cg);
   }

TR::ARMControlFlowInstruction *generateControlFlowInstruction(TR::CodeGenerator                   *cg,
                                                             TR_ARMOpCodes                       op,
                                                             TR::Node                            *node,
                                                             TR::RegisterDependencyConditions *cond)
   {
   if (cond)
      return new (cg->trHeapMemory()) TR::ARMControlFlowInstruction(op, node, cond, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMControlFlowInstruction(op, node, cg);
   }

TR::Instruction *generatePreIncLoadInstruction(TR::CodeGenerator *cg,
                                              TR::Node          *node,
                                              TR::Register      *treg,
                                              TR::Register      *baseReg,
                                              uint32_t          offset,
                                              TR::Instruction   *prev)
   {
   TR::MemoryReference *updateMR = new (cg->trHeapMemory()) TR::MemoryReference(baseReg, offset, cg);
   updateMR->setImmediatePreIndexed(); // write the updated EA back into baseReg
   if (prev)
      return new (cg->trHeapMemory()) TR::ARMTrg1MemInstruction(prev, ARMOp_ldr, node, treg, updateMR, cg);
   else
      return new (cg->trHeapMemory()) TR::ARMTrg1MemInstruction(ARMOp_ldr, node, treg, updateMR, cg);
   }

#ifdef J9_PROJECT_SPECIFIC
TR::Instruction *generateVirtualGuardNOPInstruction(TR::CodeGenerator *cg,  TR::Node *n, TR_VirtualGuardSite *site,
   TR::RegisterDependencyConditions *cond, TR::LabelSymbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::ARMVirtualGuardNOPInstruction(n, site, cond, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::ARMVirtualGuardNOPInstruction(n, site, cond, sym, cg);
   }
#endif
