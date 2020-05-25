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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/BackingStore.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/RegisterDependencyStruct.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "p/codegen/GenerateInstructions.hpp"

TR::Register *OMR::Power::TreeEvaluator::arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *addrNode = node->getFirstChild();
   TR::Node *fillNode = node->getSecondChild();
   TR::Node *lenNode = node->getChild(2);
   bool constLength = lenNode->getOpCode().isLoadConst();
   int32_t length = constLength ? lenNode->getInt() : 0;
   bool constFill = fillNode->getOpCode().isLoadConst();

   const uint32_t fillNodeSize = fillNode->getOpCode().isRef()
      ? TR::Compiler->om.sizeofReferenceField()
      : fillNode->getSize();

   TR::Register *addrReg = cg->gprClobberEvaluate(addrNode);
   TR::Register *fillReg = NULL;
   TR::Register *fp1Reg = NULL;
   TR::Register *lenReg   = cg->gprClobberEvaluate(lenNode);
   TR::Register *tempReg = cg->allocateRegister();
   TR::Register *condReg = cg->allocateRegister(TR_CCR);

   bool dofastPath = ((constLength && constFill && (length >= 64)) || (8 == fillNodeSize));

   if (constFill)
      {
      TR::Instruction *q[4];
      uint64_t doubleword = 0xdeadbeef;
      switch(fillNodeSize)
         {
         case 1:
            {
            uint64_t byte = (uint64_t)(fillNode->getByte() & 0xff);
            uint64_t halfword = (uint64_t)((byte << 8) | byte);
            if (dofastPath)
               {
               doubleword = (halfword << 48) | (halfword << 32) | (halfword << 16) | halfword;
               }
            fillReg = cg->allocateRegister();
            if (dofastPath && cg->comp()->target().is64Bit())
               {
               generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, fillReg, (int16_t)halfword);
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, fillReg, fillReg,  16, 0xffff0000);
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, fillReg, fillReg,  32, 0xffffffff00000000ULL);
               }
            else
               {
               generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, fillReg, (int16_t)halfword);
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, fillReg, fillReg, halfword);
               }
            }
            break;
         case 2:
            {
            uint64_t halfword = (uint64_t)(fillNode->getShortInt() & 0xffff);
            if (dofastPath)
               {
               doubleword = (halfword << 48) | (halfword << 32) | (halfword << 16) | halfword;
               }
            fillReg = cg->allocateRegister();
            if (dofastPath && cg->comp()->target().is64Bit())
               {
               generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, fillReg, (int16_t)halfword);
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, fillReg, fillReg,  16, 0xffff0000);
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, fillReg, fillReg,  32, 0xffffffff00000000ULL);
               }
            else
               {
               generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, fillReg, (int16_t)halfword);
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, fillReg, fillReg, halfword);
               }
            }
            break;
         case 4:
            {
            uint64_t word = (uint64_t)(fillNode->getOpCodeValue()==TR::fconst ? fillNode->getFloatBits() & 0xffffffff : fillNode->getUnsignedInt() & 0xffffffff);
            if (dofastPath)
               {
               doubleword = (word << 32) | word;
               }
            fillReg = cg->allocateRegister();
            loadConstant(cg, node, ((int32_t)word), fillReg);
            if (dofastPath && cg->comp()->target().is64Bit())
               generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldimi, node, fillReg, fillReg,  32, 0xffffffff00000000ULL);
            }
            break;
         case 8:
            {
            if (fillNode->getDataType() == TR::Double)
               {
               fp1Reg = fillNode->getRegister();
               if (!fp1Reg)
                  fp1Reg = cg->evaluate(fillNode);
               fillReg = cg->allocateRegister();
               }
            else if (cg->comp()->target().is64Bit())  //long: 64 bit target
               fillReg = cg->evaluate(fillNode);
            else                           //long: 32 bit target
               {
               uint64_t longInt = fillNode->getUnsignedLongInt();
               doubleword = longInt;
               fillReg = cg->allocateRegister();
               }
            }
            break;
         default:
            TR_ASSERT( false,"unrecognised address precision\n");
         }

      if (fillNode->getDataType() != TR::Double && dofastPath)
         {
         fp1Reg = cg->allocateRegister(TR_FPR);
         if (cg->comp()->target().is32Bit())
            {
            fixedSeqMemAccess(cg, node, 0, q, fp1Reg, tempReg, TR::InstOpCode::lfd, 8, NULL, tempReg);
            cg->findOrCreateFloatConstant(&doubleword, TR::Double, q[0], q[1], q[2], q[3]);
            }
         }
      }
   else //variable fill value
      {
      switch(fillNodeSize)
         {
         case 1:
            {
            fillReg = cg->gprClobberEvaluate(fillNode);
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, fillReg, fillReg,  8, 0xff00);
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, fillReg, fillReg,  16, 0xffff0000);
            }
            break;
         case 2:
            {
            fillReg = cg->gprClobberEvaluate(fillNode);
            generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwimi, node, fillReg, fillReg,  16, 0xffff0000);
            }
            break;
         case 4:
            {
            fillReg = cg->gprClobberEvaluate(fillNode);
            }
            break;
         case 8:
            {
            if (fillNode->getDataType() == TR::Double)
               {
               fp1Reg = cg->evaluate(fillNode);
               }
            else if (cg->comp()->target().is64Bit())
               {
               fp1Reg = cg->allocateRegister();
               fillReg = cg->evaluate(fillNode);
               }
            else
               {
               TR::Register *valueReg = cg->evaluate(fillNode);
               fp1Reg = cg->allocateRegister(TR_FPR);
               TR_BackingStore * location;
               location = cg->allocateSpill(8, false, NULL);
               TR::MemoryReference *tempMRStore1 = new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 4, cg);
               TR::MemoryReference *tempMRStore2 =  new (cg->trHeapMemory()) TR::MemoryReference(node, *tempMRStore1, 4, 4, cg);
               generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore1, valueReg->getHighOrder());
               generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node, tempMRStore2, valueReg->getLowOrder());
               generateInstruction(cg, TR::InstOpCode::lwsync, node);
               generateTrg1MemInstruction(cg, TR::InstOpCode::lfd, node, fp1Reg, new (cg->trHeapMemory()) TR::MemoryReference(node, location->getSymbolReference(), 8, cg));
               cg->freeSpill(location, 8, 0);
               }
            if (!fillReg)
               fillReg = cg->allocateRegister();
            }
            break;
         default:
            TR_ASSERT( false,"unrecognised address precision\n");
         }
      }

   if (dofastPath)
      {
      TR::LabelSymbol *label0 = generateLabelSymbol(cg);
      TR::LabelSymbol *label1 = generateLabelSymbol(cg);
      TR::LabelSymbol *label2 = generateLabelSymbol(cg);
      TR::LabelSymbol *label3 = generateLabelSymbol(cg);
      TR::LabelSymbol *label4 = generateLabelSymbol(cg);
      TR::LabelSymbol *label5 = generateLabelSymbol(cg);
      TR::LabelSymbol *label6 = generateLabelSymbol(cg);
      TR::LabelSymbol *label7 = generateLabelSymbol(cg);
      TR::LabelSymbol *label8 = generateLabelSymbol(cg);
      TR::LabelSymbol *label9 = generateLabelSymbol(cg);
      TR::LabelSymbol *label10 = generateLabelSymbol(cg);
      TR::LabelSymbol *donelabel = generateLabelSymbol(cg);

      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(6, 6, cg->trMemory());
      TR::addDependency(deps, addrReg, TR::RealRegister::NoReg, TR_GPR, cg);
      deps->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
      deps->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
      TR::addDependency(deps, fillReg, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, lenReg, TR::RealRegister::NoReg, TR_GPR, cg);
      deps->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0();
      deps->getPreConditions()->getRegisterDependency(2)->setExcludeGPR0();
      TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, condReg, TR::RealRegister::cr0, TR_CCR, cg);
      TR::addDependency(deps, fp1Reg, TR::RealRegister::NoReg, TR_FPR, cg);

      generateTrg1Src1ImmInstruction(cg, lenNode->getOpCode().isInt() ? TR::InstOpCode::cmpli4 : TR::InstOpCode::cmpli8, node, condReg, lenReg, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, donelabel, condReg);

      if (1 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, addrReg, 1); // 2 aligned??
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label0, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 1, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 1);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, lenReg, lenReg, -1);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label0); // L0
         }

      if (1 == fillNodeSize || 2 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, lenNode->getOpCode().isInt() ? TR::InstOpCode::cmpli4 : TR::InstOpCode::cmpli8, node, condReg, lenReg, 2);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, label1, condReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, addrReg, 2); // 4 aligned??
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label1, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 2, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 2);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, lenReg, lenReg, -2);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label1); // L1
         }

      if (1 == fillNodeSize || 2 == fillNodeSize || 4 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, lenNode->getOpCode().isInt() ? TR::InstOpCode::cmpli4 : TR::InstOpCode::cmpli8, node, condReg, lenReg, 4);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, label6, condReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, addrReg, 4); // 8 aligned??
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label6, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 4);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, lenReg, lenReg, -4);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label6); // L6
         }

      TR::InstOpCode::Mnemonic dblStoreOp = TR::InstOpCode::stfd;
      TR::Register *dblFillReg= fp1Reg;
      if (cg->comp()->target().is64Bit() && fillNode->getDataType() != TR::Double)
         {
         dblStoreOp = TR::InstOpCode::std;
         dblFillReg = fillReg;
         }

      //double word store, unroll 8 times
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi_r, node, tempReg, lenReg, 6);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label2, condReg);
      generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tempReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label3); // L3
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 8, cg), dblFillReg);
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 8, 8, cg), dblFillReg);
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 16, 8, cg), dblFillReg);
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 24, 8, cg), dblFillReg);
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 32, 8, cg), dblFillReg);
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 40, 8, cg), dblFillReg);
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 48, 8, cg), dblFillReg);
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 56, 8, cg), dblFillReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 64);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, label3, condReg);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, lenReg, lenReg, 0, 63); // Mask
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label2); // L2

      //residual double words
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi_r, node, tempReg, lenReg, 3);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label5, condReg);
      generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tempReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label7); // L7
      generateMemSrc1Instruction(cg, dblStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 8, cg), dblFillReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 8);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, label7, condReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label5); //L5

      //residual words
      if (1 == fillNodeSize || 2 == fillNodeSize || 4 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lenReg, 4);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label4, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stw, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 4);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label4); // L4
         }

      //residual half words
      if (1 == fillNodeSize || 2 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lenReg, 2);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label9, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 2, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 2);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label9); // L9
         }

      //residual bytes
      if (1 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lenReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, donelabel, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 1, cg), fillReg);
         }

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, donelabel, deps); // LdoneLabel
      cg->stopUsingRegister(dblFillReg);
      }
   else // regular word store path
      {
      TR::LabelSymbol *label1 = generateLabelSymbol(cg);
      TR::LabelSymbol *label2 = generateLabelSymbol(cg);
      TR::LabelSymbol *label3 = generateLabelSymbol(cg);
      TR::LabelSymbol *label4 = generateLabelSymbol(cg);
      TR::LabelSymbol *label5 = generateLabelSymbol(cg);
      TR::LabelSymbol *label6 = generateLabelSymbol(cg);
      TR::LabelSymbol *label9 = generateLabelSymbol(cg);
      TR::LabelSymbol *label10 = generateLabelSymbol(cg);
      TR::LabelSymbol *donelabel = generateLabelSymbol(cg);

      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(5, 5, cg->trMemory());
      TR::addDependency(deps, addrReg, TR::RealRegister::NoReg, TR_GPR, cg);
      deps->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
      deps->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
      TR::addDependency(deps, fillReg, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, lenReg, TR::RealRegister::NoReg, TR_GPR, cg);
      deps->getPostConditions()->getRegisterDependency(2)->setExcludeGPR0();
      deps->getPreConditions()->getRegisterDependency(2)->setExcludeGPR0();
      TR::addDependency(deps, tempReg, TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, condReg, TR::RealRegister::cr0, TR_CCR, cg);

      generateTrg1Src1ImmInstruction(cg, lenNode->getOpCode().isInt() ? TR::InstOpCode::cmpli4 : TR::InstOpCode::cmpli8, node, condReg, lenReg, 0);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, donelabel, condReg);

      if (1 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, addrReg, 1); // 2 aligned??
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label6, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 1, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 1);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, lenReg, lenReg, -1);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label6); // L6
         }

      if (1 == fillNodeSize || 2 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, lenNode->getOpCode().isInt() ? TR::InstOpCode::cmpli4 : TR::InstOpCode::cmpli8, node, condReg, lenReg, 2);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::blt, node, label5, condReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, addrReg, 2); // 4 aligned??
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label5, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 2, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 2);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, lenReg, lenReg, -2);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label5); // L5
         }

      TR::InstOpCode::Mnemonic wdStoreOp = TR::InstOpCode::stw;
      if (fillReg->getKind() == TR_FPR)
         {
    	 wdStoreOp = TR::InstOpCode::stfs;
         }

      //word store, unroll 8 times
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi_r, node, tempReg, lenReg, 5);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label2, condReg);
      generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tempReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label3); // L3
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, cg), fillReg);
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 4, 4, cg), fillReg);
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 8, 4, cg), fillReg);
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 12, 4, cg), fillReg);
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 16, 4, cg), fillReg);
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 20, 4, cg), fillReg);
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 24, 4, cg), fillReg);
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 28, 4, cg), fillReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 32);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, label3, condReg);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, lenReg, lenReg, 0, 31); // Mask
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label2); // L2

      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::srawi_r, node, tempReg, lenReg, 2);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label4, condReg);
      generateSrc1Instruction(cg, TR::InstOpCode::mtctr, node, tempReg);
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label10); // L10
      generateMemSrc1Instruction(cg, wdStoreOp, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 4, cg), fillReg);
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 4);
      generateConditionalBranchInstruction(cg, TR::InstOpCode::bdnz, node, label10, condReg);
      generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rlwinm, node, lenReg, lenReg, 0, 3); // Mask
      generateLabelInstruction(cg, TR::InstOpCode::label, node, label4); // L4

      if (1 == fillNodeSize || 2 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lenReg, 2);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, label9, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::sth, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 2, cg), fillReg);
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, addrReg, addrReg, 2);
         generateLabelInstruction(cg, TR::InstOpCode::label, node, label9); // L9
         }

      if (1 == fillNodeSize)
         {
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::andi_r, node, tempReg, lenReg, 1);
         generateConditionalBranchInstruction(cg, TR::InstOpCode::beq, node, donelabel, condReg);
         generateMemSrc1Instruction(cg, TR::InstOpCode::stb, node,new (cg->trHeapMemory()) TR::MemoryReference(addrReg, 0, 1, cg), fillReg);
         }

      generateDepLabelInstruction(cg, TR::InstOpCode::label, node, donelabel, deps); // LdoneLabel
      }

   cg->stopUsingRegister(fillReg);
   cg->stopUsingRegister(lenReg);
   cg->stopUsingRegister(tempReg);
   cg->stopUsingRegister(condReg);
   cg->stopUsingRegister(addrReg);
   if (dofastPath) cg->stopUsingRegister(fp1Reg);

   cg->decReferenceCount(addrNode);
   cg->decReferenceCount(fillNode);
   cg->decReferenceCount(lenNode);

   return NULL;
   }

//----------------------------------------------------------------------
// arraytranslateAndTest
//----------------------------------------------------------------------

TR::Register *OMR::Power::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }
