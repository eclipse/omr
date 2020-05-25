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

#include "compiler/p/codegen/GenerateInstructions.hpp"

#include <algorithm>
#include <stdint.h>
#include <stdlib.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/Register.hpp"
#include "codegen/Snippet.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "runtime/Runtime.hpp"

class TR_VirtualGuardSite;
namespace TR { class LabelSymbol; }
namespace TR { class RegisterDependencyConditions; }
namespace TR { class SymbolReference; }

TR::InstOpCode::Mnemonic flipBranch(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op);
int estimateLikeliness(TR::CodeGenerator *cg, TR::Node *n);

TR::Instruction *generateMvFprGprInstructions(TR::CodeGenerator *cg, TR::Node *node, MvFprGprMode mode, bool nonops, TR::Register *reg0, TR::Register *reg1, TR::Register *reg2, TR::Register * reg3, TR::Instruction *cursor)
   {
   TR::MemoryReference *tempMRStore1, *tempMRStore2, *tempMRLoad1, *tempMRLoad2;
   static bool disableDirectMove = feGetEnv("TR_disableDirectMove") ? true : false;
   bool checkp8DirectMove = cg->comp()->target().cpu.id() >= TR_PPCp8 && !disableDirectMove && cg->comp()->target().cpu.getPPCSupportsVSX();
   bool isLittleEndian = cg->comp()->target().cpu.isLittleEndian();

   // it's fine if reg3 and reg2 are assigned in modes they are not used
   // what we want to avoid is them being NULL when we need to use them
   // i.e. in direct move gpr2fprHost32 or fpr2gprHost32
   TR_ASSERT(!(checkp8DirectMove && (reg3 == NULL || reg2 == NULL) && (mode == gpr2fprHost32 || mode == fpr2gprHost32)),
         "reg3 and reg2 cannot be null in gpr2fprHost32 or fpr2gprHost32 on POWER8 or newer");

   // POWER 8 direct move path
   if (checkp8DirectMove && mode == fpr2gprHost64)
      {
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrd, node, reg0, reg1, cursor);
      }
   else if (checkp8DirectMove && mode == fpr2gprLow)
      {
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrwz, node, reg0, reg1, cursor);
      }
   else if (checkp8DirectMove && mode == fpr2gprSp)
      {
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::xscvdpspn, node, reg1, reg1, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, reg1, reg1, 0, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrwz, node, reg0, reg1, cursor);
      }
   else if (checkp8DirectMove && mode == gpr2fprHost64)
      {
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrd, node, reg0, reg1, cursor);
      }
   else if (checkp8DirectMove && mode == gprLow2fpr)
      {
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwa, node, reg0, reg1, cursor);
      }
   else if (checkp8DirectMove && mode == gprSp2fpr)
      {
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwz, node, reg0, reg1, cursor);
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, reg0, reg0, 1, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::xscvspdp, node, reg0, reg0, cursor);
      }
   else if (checkp8DirectMove && mode == gpr2fprHost32)
      {
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwz, node, reg3, reg1, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mtvsrwz, node, reg0, reg2, cursor);
      cursor = generateTrg1Src2Instruction(cg, TR::InstOpCode::xxmrghw, node, reg3, reg3, reg0, cursor);
      cursor = generateTrg1Src2ImmInstruction(cg, TR::InstOpCode::xxpermdi, node, reg0, reg3, reg3, 2, cursor);
      }
   else if (checkp8DirectMove && mode == fpr2gprHost32)
      {
      cursor = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::xxspltw, node, reg3, reg2, 0, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrwz, node, reg0, reg3, cursor);
      cursor = generateTrg1Src1Instruction(cg, TR::InstOpCode::mfvsrwz, node, reg1, reg2, cursor);
      }
   else // otherwise, do generic load/store sequences for moving from fpr2gpr or gpr2fpr
      {
      TR_BackingStore * location;
      location = cg->allocateSpill(8, false, NULL);
      if ((mode == gprSp2fpr) || (mode == fpr2gprSp) || (mode == gprLow2fpr))
         {
         tempMRStore1 = new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 4, cg);
         tempMRLoad1 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore1, 0, 4, cg);
         }
      else if (mode == gpr2fprHost32)
         {
         if (isLittleEndian)
            {
            tempMRStore2 = new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 4, cg);
            tempMRStore1 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore2, 4, 4, cg);
            }
         else
            {
            tempMRStore1 = new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 4, cg);
            tempMRStore2 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore1, 4, 4, cg);
            }
         tempMRLoad1 = new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 8, cg);
         }
      else
         {
         tempMRStore1 = new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 8, cg);

         if (mode == fpr2gprHost32)
            tempMRLoad1 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore1, isLittleEndian ? 4 : 0, 4, cg);
         if ((mode == fpr2gprHost64) || (mode == gpr2fprHost64))
            tempMRLoad1 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore1, 0, 8, cg);
         else
            tempMRLoad2 = new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore1, isLittleEndian ? 0 : 4, 4, cg);
         }

      if ((mode == fpr2gprHost64) || (mode == fpr2gprLow))
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMRStore1, reg1, cursor);
      else if (mode == fpr2gprHost32)
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stfd, node, tempMRStore1, reg2, cursor);
      else if (mode == gpr2fprHost64)
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::std, node, tempMRStore1, reg1, cursor);
      else if (mode == gpr2fprHost32)
         {
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore1, reg1, cursor);
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore2, reg2, cursor);
         }
      else if (mode == gprSp2fpr)
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore1, reg1, cursor);
      else if (mode == fpr2gprSp)
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stfs, node, tempMRStore1, reg1, cursor);
      else if (mode == gprLow2fpr)
         cursor = generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore1, reg1, cursor);

      if ((nonops == false) && (cg->comp()->target().cpu.id() >= TR_PPCgp))
         {
    	 // Insert 3 nops to break up the load/stores into separate groupings,
    	 // thus preventing a costly stall
         cursor = cg->generateGroupEndingNop(node, cursor);
         }

      if (mode == fpr2gprHost64)
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::ld, node, reg0, tempMRLoad1, cursor);
      else if (mode == fpr2gprHost32)
         {
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, reg0, tempMRLoad1, cursor);
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, reg1, tempMRLoad2, cursor);
         }
      else if (mode == fpr2gprLow)
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, reg0, tempMRLoad2, cursor);
      else if ((mode == gpr2fprHost64) || (mode == gpr2fprHost32))
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, reg0, tempMRLoad1, cursor);
      else if (mode == gprSp2fpr)
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lfs, node, reg0, tempMRLoad1, cursor);
      else if (mode == fpr2gprSp)
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lwz, node, reg0, tempMRLoad1, cursor);
      else if (mode == gprLow2fpr)
         {
         tempMRLoad1->forceIndexedForm(node, cg);
         cursor = generateTrg1MemInstruction(cg, TR::InstOpCode::lfiwax, node, reg0, tempMRLoad1, cursor);
         tempMRLoad1->decNodeReferenceCounts(cg);
         }
      cg->freeSpill(location, 8, 0);
      }
  return cursor;
  }

TR::Instruction *generateInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::Instruction(op, n, preced, cg);
   return new (cg->trHeapMemory()) TR::Instruction(op, n, cg);
   }

TR::Instruction *generateAlignmentNopInstruction(TR::CodeGenerator *cg, TR::Node * n, uint32_t alignment, TR::Instruction *preced)
   {
   auto op = cg->comp()->target().cpu.id() >= TR_PPCp6 ? TR::InstOpCode::genop : TR::InstOpCode::nop;

   if (preced)
      return new (cg->trHeapMemory()) TR::PPCAlignmentNopInstruction(op, n, alignment, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCAlignmentNopInstruction(op, n, alignment, cg);
   }

TR::Instruction *generateImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t imm,
                                       TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCImmInstruction(op, n, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCImmInstruction(op, n, imm, cg);
   }

TR::Instruction *generateImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *n, uint32_t imm,
                                       TR_ExternalRelocationTargetKind relocationKind, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCImmInstruction(op, n, imm, relocationKind, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCImmInstruction(op, n, imm, relocationKind, cg);
   }

TR::Instruction *generateImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node *n, uint32_t imm,
                                       TR_ExternalRelocationTargetKind relocationKind, TR::SymbolReference *sr, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCImmInstruction(op, n, imm, relocationKind, sr, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCImmInstruction(op, n, imm, relocationKind, sr, cg);
   }

TR::Instruction *generateImm2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n, uint32_t imm, uint32_t imm2,
                                       TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCImm2Instruction(op, n, imm, imm2, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCImm2Instruction(op, n, imm, imm2, cg);
   }

TR::Instruction *generateDepInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCDepInstruction(op, n, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCDepInstruction(op, n, cond, cg);
   }

TR::Instruction *generateMemSrc1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::MemoryReference *mf, TR::Register *sreg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCMemSrc1Instruction(op, n, mf, sreg, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCMemSrc1Instruction(op, n, mf, sreg, cg);
   }

TR::Instruction *generateTrg1MemInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::MemoryReference *mf, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1MemInstruction(op, n, treg, mf, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1MemInstruction(op, n, treg, mf, cg);
   }

TR::Instruction *generateMemInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::MemoryReference *mf, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCMemInstruction(op, n, mf, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCMemInstruction(op, n, mf, cg);
   }

TR::Instruction *generateTrg1MemInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, int32_t hint, TR::Node * n,
   TR::Register *treg, TR::MemoryReference *mf, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1MemInstruction(op, n, treg, mf, preced, cg, hint);
   return new (cg->trHeapMemory()) TR::PPCTrg1MemInstruction(op, n, treg, mf, cg, hint);
   }

TR::Instruction *generateTrg1ImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, uint32_t imm, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1ImmInstruction(op, n, treg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1ImmInstruction(op, n, treg, imm, cg);
   }

TR::Instruction *generateLabelInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::LabelSymbol *sym, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCLabelInstruction(op, n, sym, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCLabelInstruction(op, n, sym, cg);
   }

TR::Instruction *generateDepLabelInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::LabelSymbol *sym, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCDepLabelInstruction(op, n, sym, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCDepLabelInstruction(op, n, sym, cond, cg);
   }

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, bool likeliness, TR::Node * n,
   TR::LabelSymbol *sym, TR::Register *cr, TR::Instruction *preced)
   {
   if (cr->isFlippedCCR())
      op = flipBranch(cg, op);

   if (preced)
      return new (cg->trHeapMemory()) TR::PPCConditionalBranchInstruction(op, n, sym, cr, preced, cg, likeliness);
   return new (cg->trHeapMemory()) TR::PPCConditionalBranchInstruction(op, n, sym, cr, cg, likeliness);
   }

TR::Instruction *generateDepConditionalBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op,  bool likeliness, TR::Node * n,
   TR::LabelSymbol *sym, TR::Register *cr, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (cr->isFlippedCCR())
      op = flipBranch(cg, op);

   if (preced)
      return new (cg->trHeapMemory()) TR::PPCDepConditionalBranchInstruction(op, n, sym, cr, cond, preced, cg, likeliness);
   return new (cg->trHeapMemory()) TR::PPCDepConditionalBranchInstruction(op, n, sym, cr, cond, cg, likeliness);
   }

TR::Instruction *generateConditionalBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::LabelSymbol *sym, TR::Register *cr, TR::Instruction *preced)
   {
   if (cr->isFlippedCCR())
      op = flipBranch(cg, op);

   int prediction = estimateLikeliness(cg, n);
   bool likeliness;
   if(prediction < 0)
      likeliness = false;
   else if (prediction > 0)
      likeliness = true;

   if(prediction)
      {
      if (preced)
         return new (cg->trHeapMemory()) TR::PPCConditionalBranchInstruction(op, n, sym, cr, preced, cg, likeliness);
      return new (cg->trHeapMemory()) TR::PPCConditionalBranchInstruction(op, n, sym, cr, cg, likeliness);
      }
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCConditionalBranchInstruction(op, n, sym, cr, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCConditionalBranchInstruction(op, n, sym, cr, cg);
   }

TR::Instruction *generateDepConditionalBranchInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::LabelSymbol *sym, TR::Register *cr, TR::RegisterDependencyConditions *cond, TR::Instruction *preced)
   {
   if (cr->isFlippedCCR())
      op = flipBranch(cg, op);

   int prediction = estimateLikeliness(cg, n);
   bool likeliness;
   if(prediction < 0)
      likeliness = false;
   else if (prediction > 0)
      likeliness = true;

   if(prediction)
      {
      if (preced)
         return new (cg->trHeapMemory()) TR::PPCDepConditionalBranchInstruction(op, n, sym, cr, cond, preced, cg, likeliness);
      return new (cg->trHeapMemory()) TR::PPCDepConditionalBranchInstruction(op, n, sym, cr, cond, cg, likeliness);
      }

   if (preced)
      return new (cg->trHeapMemory()) TR::PPCDepConditionalBranchInstruction(op, n, sym, cr, cond, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCDepConditionalBranchInstruction(op, n, sym, cr, cond, cg);
   }

TR::Instruction *generateTrg1Src1ImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::Register *s1reg, intptr_t imm, TR::Instruction *preced)
   {
   if (cg->comp()->target().cpu.id() == TR_PPCp6 && TR::InstOpCode(op).isCompare())
      treg->resetFlippedCCR();
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1ImmInstruction(op, n, treg, s1reg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1ImmInstruction(op, n,treg, s1reg, imm, cg);
   }

TR::Instruction *generateTrg1Src1ImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::Register *s1reg, TR::Register *cr0reg, int32_t imm, TR::Instruction *preced)
   {
   if (cg->comp()->target().cpu.id() == TR_PPCp6)
      cr0reg->resetFlippedCCR();
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1ImmInstruction(op, n,treg, s1reg, cr0reg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1ImmInstruction(op, n,treg, s1reg, cr0reg, imm, cg);
   }

TR::Instruction *generateSrc1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *s1reg, int32_t imm, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCSrc1Instruction(op, n, s1reg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCSrc1Instruction(op, n, s1reg, imm, cg);
   }

TR::Instruction *generateSrc2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *s1reg, TR::Register *s2reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCSrc2Instruction(op, n, s1reg, s2reg, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCSrc2Instruction(op, n, s1reg, s2reg, cg);
   }

TR::Instruction *generateTrg1Src1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::Register *s1reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1Instruction(op, n, treg, s1reg, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1Instruction(op, n, treg, s1reg, cg);
   }

static bool registerRecentlyWritten(TR::Register *reg, TR::Instruction *cursor)
   {
   const uint32_t windowSize = 4;
   uint32_t       window = 0;

   while (cursor && window <= windowSize)
      {
      // Walk past pseudo instructions (which have binary encoding == 0)
      if (cursor->getOpCode().getOpCodeBinaryEncoding())
         {
         if (cursor->getTargetRegister(0) == reg)
            return true;
         ++window;
         }
      cursor = cursor->getPrev();
      }
   return false;
   }

TR::Instruction *generateTrg1Src2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::Instruction *preced)
   {
   TR::Compilation * comp = cg->comp();
   static bool disableFlipCompare = feGetEnv("TR_DisableFlipCompare") != NULL;
   if (!disableFlipCompare && cg->comp()->target().cpu.id() == TR_PPCp6 &&
       TR::InstOpCode(op).isCompare() &&
       n->getOpCode().isBranch() && n->getOpCode().isBooleanCompare())
      {
      treg->resetFlippedCCR();
      if (s1reg->containsInternalPointer() ||
          (!TR::InstOpCode(op).isFloat() && registerRecentlyWritten(s1reg, preced ? preced : cg->getAppendInstruction())))
         {
         //Swap registers to avoid p6 fxu reject.
         TR::Register *temp = s1reg;
         s1reg = s2reg;
         s2reg = temp;
         treg->setFlippedCCR();
         if (cg->getDebug())
            traceMsg(comp, "Flipping CCR operands for compare generated for node [%p]\n", n);
         }
      }

   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src2Instruction(op, n, treg, s1reg, s2reg, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src2Instruction(op, n, treg, s1reg, s2reg, cg);
   }

TR::Instruction *generateTrg1Src2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::Register *cr0Reg, TR::Instruction *preced)
   {
   if (cg->comp()->target().cpu.id() == TR_PPCp6)
      cr0Reg->resetFlippedCCR();
   return new (cg->trHeapMemory()) TR::PPCTrg1Src2Instruction(op, n, treg, s1reg, s2reg, cr0Reg, preced, cg);
   }

TR::Instruction *generateTrg1Src2ImmInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, int64_t imm, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src2ImmInstruction(op, n, treg, s1reg, s2reg, imm, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src2ImmInstruction(op, n, treg, s1reg, s2reg, imm, cg);
   }

TR::Instruction *generateTrg1Src3Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *treg, TR::Register *s1reg, TR::Register *s2reg, TR::Register *s3reg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src3Instruction(op, n, treg, s1reg, s2reg, s3reg, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src3Instruction(op, n, treg, s1reg, s2reg, s3reg, cg);
   }

TR::Instruction *generateTrg1Src1Imm2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *trgReg, TR::Register *srcReg, int32_t imm1, int64_t imm2, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(op, n, trgReg, srcReg, imm1, imm2, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(op, n, trgReg, srcReg, imm1, imm2, cg);
   }

TR::Instruction *generateTrg1Src1Imm2Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *trgReg, TR::Register *srcReg, TR::Register *cr0reg, int32_t imm1, int64_t imm2, TR::Instruction *preced)
   {
   if (cg->comp()->target().cpu.id() == TR_PPCp6)
      cr0reg->resetFlippedCCR();
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(op, n, trgReg, srcReg, cr0reg, imm1, imm2, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(op, n, trgReg, srcReg, cr0reg, imm1, imm2, cg);
   }

TR::Instruction *generateShiftLeftImmediate(TR::CodeGenerator *cg, TR::Node * n,
   TR::Register *trgReg, TR::Register *srcReg, int32_t shiftAmount, TR::Instruction *preced)
   {
   int32_t temp = ((int32_t)0x80000000)>>(31-shiftAmount);
   if (shiftAmount == 1)
      {
      if (preced)
         return new (cg->trHeapMemory()) TR::PPCTrg1Src2Instruction(TR::InstOpCode::add, n, trgReg, srcReg, srcReg, preced, cg);
      return new (cg->trHeapMemory()) TR::PPCTrg1Src2Instruction(TR::InstOpCode::add, n, trgReg, srcReg, srcReg, cg);
      }
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rlwinm, n, trgReg, srcReg, shiftAmount%32, temp, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rlwinm, n, trgReg, srcReg, shiftAmount%32, temp, cg);
   }

TR::Instruction *generateShiftRightLogicalImmediate(TR::CodeGenerator *cg, TR::Node * n,
   TR::Register *trgReg, TR::Register *srcReg, int32_t shiftAmount, TR::Instruction *preced)
   {
   int32_t temp1 = 32-shiftAmount;
   int32_t temp2 = (1<<temp1)-1;
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rlwinm, n, trgReg, srcReg, temp1%32, temp2, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rlwinm, n, trgReg, srcReg, temp1%32, temp2, cg);
   }

TR::Instruction *generateShiftLeftImmediateLong(TR::CodeGenerator *cg, TR::Node * n,
   TR::Register *trgReg, TR::Register *srcReg, int32_t shiftAmount, TR::Instruction *preced)
   {
   int64_t mask;
   mask = (((int64_t)0x80000000)<<32)>>(63-shiftAmount);

   if (shiftAmount == 1)
      {
      if (preced)
         return new (cg->trHeapMemory()) TR::PPCTrg1Src2Instruction(TR::InstOpCode::add, n, trgReg, srcReg, srcReg, preced, cg);
      return new (cg->trHeapMemory()) TR::PPCTrg1Src2Instruction(TR::InstOpCode::add, n, trgReg, srcReg, srcReg, cg);
      }
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rldicr, n, trgReg, srcReg, shiftAmount%64, mask, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rldicr, n, trgReg, srcReg, shiftAmount%64, mask, cg);
   }

TR::Instruction *generateShiftRightLogicalImmediateLong(TR::CodeGenerator *cg, TR::Node * n,
   TR::Register *trgReg, TR::Register *srcReg, int32_t shiftAmount, TR::Instruction *preced)
   {
   int32_t temp1 = 64-shiftAmount;
   int64_t mask = ((uint64_t)1<<temp1)-1;
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rldicl, n, trgReg, srcReg, temp1%64, mask, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Src1Imm2Instruction(TR::InstOpCode::rldicl, n, trgReg, srcReg, temp1%64, mask, cg);
   }


TR::Instruction *generateControlFlowInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::RegisterDependencyConditions *deps, TR::Instruction *preced, bool useRegPairForResult, bool useRegPairForCond)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCControlFlowInstruction(op, n, preced, cg, deps, useRegPairForResult, useRegPairForCond);
   return new (cg->trHeapMemory()) TR::PPCControlFlowInstruction(op, n, cg, deps, useRegPairForResult, useRegPairForCond);
   }

TR::Instruction *generateAdminInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Node *fenceNode, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCAdminInstruction(op, n, fenceNode, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCAdminInstruction(op, n, fenceNode, cg);
   }

TR::Instruction *generateDepImmSymInstruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   uintptr_t imm, TR::RegisterDependencyConditions *cond, TR::SymbolReference *sr, TR::Snippet *s, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCDepImmSymInstruction(op, n, imm, cond, sr, s, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCDepImmSymInstruction(op, n, imm, cond, sr, s, cg);
   }

TR::Instruction *generateTrg1Instruction(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op, TR::Node * n,
   TR::Register *trg, TR::Instruction *preced)
   {
   if (preced)
      return new (cg->trHeapMemory()) TR::PPCTrg1Instruction(op, n, trg, preced, cg);
   return new (cg->trHeapMemory()) TR::PPCTrg1Instruction(op, n, trg, cg);
   }

TR::InstOpCode::Mnemonic flipBranch(TR::CodeGenerator *cg, TR::InstOpCode::Mnemonic op)
   {
   switch(op)
      {
      case TR::InstOpCode::bge:
         return TR::InstOpCode::ble;
      case TR::InstOpCode::bgel:
         return TR::InstOpCode::blel;
      case TR::InstOpCode::bgt:
         return TR::InstOpCode::blt;
      case TR::InstOpCode::bgtl:
         return TR::InstOpCode::bltl;
      case TR::InstOpCode::ble:
         return TR::InstOpCode::bge;
      case TR::InstOpCode::blel:
         return TR::InstOpCode::bgel;
      case TR::InstOpCode::blt:
         return TR::InstOpCode::bgt;
      case TR::InstOpCode::bltl:
         return TR::InstOpCode::bgtl;
      case TR::InstOpCode::beq:
      case TR::InstOpCode::beql:
      case TR::InstOpCode::bne:
      case TR::InstOpCode::bnel:
      case TR::InstOpCode::bdz:
      case TR::InstOpCode::bdnz:
         return op;
      default:
         TR_ASSERT(0, "Cannot use this branch opcode to branch off the comparison of an internal pointer : %d\n", (int32_t)op);
         return TR::InstOpCode::bad;
      }
   }

/*
 *returns 0 if the branch is not biased and a hint shouldn't be used.
 *returns 1 if a branch likely hint should be used.
 *returns -1 if a branch unlikely hint should be used.
 */
int estimateLikeliness(TR::CodeGenerator *cg, TR::Node *n)
   {
   TR::Compilation * comp = cg->comp();
   static char *TR_PredictBranchRatio = feGetEnv("TR_PredictBranchRatio");

   if(TR_PredictBranchRatio)
      {
      int32_t predictBranchRatio = atoi(TR_PredictBranchRatio);
      TR::TreeTop *dest = n->getBranchDestination();
      TR::Block *destBlock = dest ? dest->getNode()->getBlock() : NULL;
      TR::Block *fallThroughBlock = cg->getCurrentEvaluationTreeTop()->getEnclosingBlock()->getNextBlock();

      if (destBlock && fallThroughBlock && n->getOpCode().isIf())
         {
         float currentBlockFreq = std::max<float>(n->getBlock()->getFrequency(), 1.0f);
         float targetBlockFreq = std::max<float>(destBlock->getFrequency(), 1.0f);
         float fallThroughBlockFreq = std::max<float>(fallThroughBlock->getFrequency(), 1.0f);
         traceMsg(comp, "target block: %d, fall through block: %d\n", destBlock->getNumber(), fallThroughBlock->getNumber());
         bool biased = false;
         bool likeliness;
         traceMsg(comp, "targetBlockFreq: %f, fallThroughBlockFreq: %f\n", targetBlockFreq, fallThroughBlockFreq);

         if (fallThroughBlockFreq/targetBlockFreq > predictBranchRatio)
            return -1;
         else if (targetBlockFreq/fallThroughBlockFreq > predictBranchRatio)
            return 1;
         }
      }
   return 0;
   }

void generateZeroExtendInstruction(TR::Node *node,
                                   TR::Register *trgReg,
                                   TR::Register *srcReg,
                                   int32_t bitsInTarget,
                                   TR::CodeGenerator *cg)
   {
   TR_ASSERT((cg->comp()->target().is64Bit() && bitsInTarget > 0 && bitsInTarget < 64) ||
          (cg->comp()->target().is32Bit() && bitsInTarget > 0 && bitsInTarget < 32),
          "invalid zero extension requested");
   int64_t mask = (uint64_t)(CONSTANT64(0xffffFFFFffffFFFF)) >> (64 - bitsInTarget);
   generateTrg1Src1Imm2Instruction(cg, cg->comp()->target().is64Bit() ? TR::InstOpCode::rldicl : TR::InstOpCode::rlwinm, node, trgReg, srcReg, 0, mask);
   }

void generateSignExtendInstruction(TR::Node *node,
                                   TR::Register *trgReg,
                                   TR::Register *srcReg,
                                   TR::CodeGenerator *cg,
                                   int32_t operandSize)
   {
   TR::InstOpCode::Mnemonic signExtendOp = TR::InstOpCode::bad;
   switch (operandSize)
      {
      case 0x1:
         signExtendOp = TR::InstOpCode::extsb;
         break;
      case 0x2:
         signExtendOp = TR::InstOpCode::extsh;
         break;
      case 0x4:
         signExtendOp = TR::InstOpCode::extsw;
         break;
      default:
         TR_ASSERT(0,"unexpected operand size %d\n",operandSize);
         break;
      }
   generateTrg1Src1Instruction(cg, signExtendOp, node, trgReg, srcReg);
   }

void generateSignExtendInstruction(TR::Node *node,
                                   TR::Register *trgReg,
                                   TR::Register *srcReg,
                                   TR::CodeGenerator *cg)
   {
   generateSignExtendInstruction(node, trgReg, srcReg, cg, node->getOpCode().getSize());
   }
