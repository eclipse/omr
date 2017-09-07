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

#include "codegen/CodeGenerator.hpp"                  // for CodeGenerator, etc
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterPair.hpp"                   // for RegisterPair
#include "codegen/TreeEvaluator.hpp"
#include "il/ILOpCodes.hpp"                           // for ILOpCodes, etc
#include "il/ILOps.hpp"                               // for ILOpCode
#include "il/Node.hpp"                                // for Node, etc
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"                           // for TR_ASSERT
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"                       // for ::LABEL, ::JE4, etc

namespace TR { class Instruction; }

static TR::MemoryReference* ConvertToPatchableMemoryReference(TR::MemoryReference* mr, TR::Node* node, TR::CodeGenerator* cg)
   {
   if (mr->getSymbolReference().isUnresolved())
      {
      // The load instructions may be wider than 8-bytes (our patching window)
      // but we won't know that for sure until after register assignment.
      // Hence, the unresolved memory reference must be evaluated into a register first.
      //
      TR::Register* tempReg = cg->allocateRegister();
      generateRegMemInstruction(LEARegMem(cg), node, tempReg, mr, cg);
      mr = generateX86MemoryReference(tempReg, 0, cg);
      cg->stopUsingRegister(tempReg);
      }
   return mr;
   }

TR::Register* OMR::X86::TreeEvaluator::SIMDRegLoadEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Register* globalReg = node->getRegister();
   if (!globalReg)
      {
      globalReg = cg->allocateRegister(TR_VRF);
      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register* OMR::X86::TreeEvaluator::SIMDRegStoreEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* child = node->getFirstChild();
   TR::Register* globalReg = cg->evaluate(child);
   cg->machine()->setXMMGlobalRegister(node->getGlobalRegisterNumber() - cg->machine()->getNumGlobalGPRs(), globalReg);
   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register* OMR::X86::TreeEvaluator::SIMDloadEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::MemoryReference* tempMR = generateX86MemoryReference(node, cg);
   tempMR = ConvertToPatchableMemoryReference(tempMR, node, cg);
   TR::Register* resultReg = cg->allocateRegister(TR_VRF);

   TR_X86OpCodes opCode = BADIA32Op;
   switch (node->getSize())
      {
      case 16:
         opCode = MOVDQURegMem;
         break;
      default:
         if (cg->comp()->getOption(TR_TraceCG))
            traceMsg(cg->comp(), "Unsupported fill size: Node = %p\n", node);
         TR_ASSERT(false, "Unsupported fill size");
         break;
      }

   TR::Instruction* instr = generateRegMemInstruction(opCode, node, resultReg, tempMR, cg);
   if (node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);
   node->setRegister(resultReg);
   tempMR->decNodeReferenceCounts(cg);
   return resultReg;
   }

TR::Register* OMR::X86::TreeEvaluator::SIMDstoreEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* valueNode = node->getChild(node->getOpCode().isIndirect() ? 1 : 0);
   TR::MemoryReference* tempMR = generateX86MemoryReference(node, cg);
   tempMR = ConvertToPatchableMemoryReference(tempMR, node, cg);
   TR::Register* valueReg = cg->evaluate(valueNode);

   TR_X86OpCodes opCode = BADIA32Op;
   switch (node->getSize())
      {
      case 16:
         opCode = MOVDQUMemReg;
         break;
      default:
         if (cg->comp()->getOption(TR_TraceCG))
            traceMsg(cg->comp(), "Unsupported fill size: Node = %p\n", node);
         TR_ASSERT(false, "Unsupported fill size");
         break;
      }

   TR::Instruction* instr = generateMemRegInstruction(opCode, node, tempMR, valueReg, cg);

   cg->decReferenceCount(valueNode);
   tempMR->decNodeReferenceCounts(cg);
   if (node->getOpCode().isIndirect())
      cg->setImplicitExceptionPoint(instr);
   return NULL;
   }

TR::Register* OMR::X86::TreeEvaluator::SIMDsplatsEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* childNode = node->getChild(0);
   TR::Register* childReg = cg->evaluate(childNode);

   TR::Register* resultReg = cg->allocateRegister(TR_VRF);
   switch (node->getDataType())
      {
      case TR::VectorInt32:
         generateRegRegInstruction(MOVDRegReg4, node, resultReg, childReg, cg);
         generateRegRegImmInstruction(PSHUFDRegRegImm1, node, resultReg, resultReg, 0x00, cg); // 00 00 00 00 shuffle xxxA to AAAA
         break;
      case TR::VectorInt64:
         if (TR::Compiler->target.is32Bit())
            {
            TR::Register* tempVectorReg = cg->allocateRegister(TR_VRF);
            generateRegRegInstruction(MOVDRegReg4, node, tempVectorReg, childReg->getHighOrder(), cg);
            generateRegImmInstruction(PSLLQRegImm1, node, tempVectorReg, 0x20, cg);
            generateRegRegInstruction(MOVDRegReg4, node, resultReg, childReg->getLowOrder(), cg);
            generateRegRegInstruction(POR, node, resultReg, tempVectorReg, cg);
            cg->stopUsingRegister(tempVectorReg);
            }
         else
            {
            generateRegRegInstruction(MOVQRegReg8, node, resultReg, childReg, cg);
            }
         generateRegRegImmInstruction(PSHUFDRegRegImm1, node, resultReg, resultReg, 0x44, cg); // 01 00 01 00 shuffle xxBA to BABA
         break;
      case TR::VectorFloat:
         generateRegRegImmInstruction(PSHUFDRegRegImm1, node, resultReg, childReg, 0x00, cg); // 00 00 00 00 shuffle xxxA to AAAA
         break;
      case TR::VectorDouble:
         generateRegRegImmInstruction(PSHUFDRegRegImm1, node, resultReg, childReg, 0x44, cg); // 01 00 01 00 shuffle xxBA to BABA
         break;
      default:
         if (cg->comp()->getOption(TR_TraceCG))
            traceMsg(cg->comp(), "Unsupported data type, Node = %p\n", node);
         TR_ASSERT(false, "Unsupported data type");
         break;
      }

   node->setRegister(resultReg);
   cg->decReferenceCount(childNode);
   return resultReg;
   }
