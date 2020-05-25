/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#include "codegen/CodeGenerator.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "infra/Assert.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

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
         if (cg->comp()->target().is32Bit())
            {
            TR::Register* tempVectorReg = cg->allocateRegister(TR_VRF);
            generateRegRegInstruction(MOVDRegReg4, node, tempVectorReg, childReg->getHighOrder(), cg);
            generateRegImmInstruction(PSLLQRegImm1, node, tempVectorReg, 0x20, cg);
            generateRegRegInstruction(MOVDRegReg4, node, resultReg, childReg->getLowOrder(), cg);
            generateRegRegInstruction(PORRegReg, node, resultReg, tempVectorReg, cg);
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

TR::Register* OMR::X86::TreeEvaluator::SIMDgetvelemEvaluator(TR::Node* node, TR::CodeGenerator* cg)
   {
   TR::Node* firstChild = node->getChild(0);
   TR::Node* secondChild = node->getChild(1);

   TR::Register* srcVectorReg = cg->evaluate(firstChild);
   TR::Register* resReg = 0;
   TR::Register* lowResReg = 0;
   TR::Register* highResReg = 0;

   int32_t elementCount = -1;
   switch (firstChild->getDataType())
      {
      case TR::VectorInt8:
      case TR::VectorInt16:
         TR_ASSERT(false, "unsupported vector type %s in SIMDgetvelemEvaluator.\n", firstChild->getDataType().toString());
         break;
      case TR::VectorInt32:
         elementCount = 4;
         resReg = cg->allocateRegister();
         break;
      case TR::VectorInt64:
         elementCount = 2;
         if (cg->comp()->target().is32Bit())
            {
            lowResReg = cg->allocateRegister();
            highResReg = cg->allocateRegister();
            resReg = cg->allocateRegisterPair(lowResReg, highResReg);
            }
         else
            {
            resReg = cg->allocateRegister();
            }
         break;
      case TR::VectorFloat:
         elementCount = 4;
         resReg = cg->allocateSinglePrecisionRegister(TR_FPR);
         break;
      case TR::VectorDouble:
         elementCount = 2;
         resReg = cg->allocateRegister(TR_FPR);
         break;
      default:
         TR_ASSERT(false, "unrecognized vector type %s in SIMDgetvelemEvaluator.\n", firstChild->getDataType().toString());
      }

   if (secondChild->getOpCode().isLoadConst())
      {
      int32_t elem = secondChild->getInt();

      TR_ASSERT(elem >= 0 && elem < elementCount, "Element can only be 0 to %u\n", elementCount - 1);

      uint8_t shufconst = 0x00;
      TR::Register* dstReg = 0;
      if (4 == elementCount)
         {
         /*
          * if elem = 0, access the most significant 32 bits (set shufconst to 0x03)
          * if elem = 1, access the second most significant 32 bits (set shufconst to 0x02)
          * if elem = 2, access the third most significant 32 bits (set shufconst to 0x01)
          * if elem = 3, access the least significant 32 bits (set shufconst to 0x00)
          */
         shufconst = (uint8_t)((3 - elem) & 0x03);

         /*
          * the value to be read (indicated by shufconst) from srcVectorReg is splatted into all 4 slots in the dstReg
          * this puts the value we want in the least significant bits and the other bits should never be read.
          * for float, dstReg and resReg are the same because PSHUFD can work directly with TR_FPR registers
          * for Int32, the result needs to be moved from the dstReg to a TR_GPR resReg.
          */
         if (TR::VectorInt32 == firstChild->getDataType())
            {
            dstReg = cg->allocateRegister(TR_VRF);
            }
         else //TR::VectorFloat == firstChild->getDataType()
            {
            dstReg = resReg;
            }

         /*
          * if elem = 3, the value we want is already in the least significant 32 bits
          * as a result, a mov instruction is good enough and splatting the value is unnecessary
          */
         if (3 == elem)
            {
            generateRegRegInstruction(MOVDQURegReg, node, dstReg, srcVectorReg, cg);
            }
         else
            {
            generateRegRegImmInstruction(PSHUFDRegRegImm1, node, dstReg, srcVectorReg, shufconst, cg);
            }

         if (TR::VectorInt32 == firstChild->getDataType())
            {
            generateRegRegInstruction(MOVDReg4Reg, node, resReg, dstReg, cg);
            cg->stopUsingRegister(dstReg);
            }
         }
      else //2 == elementCount
         {
         /*
          * for double, dstReg and resReg are the same because PSHUFD can work directly with TR_FPR registers
          * for Int64, the result needs to be moved from the dstReg to a TR_GPR resReg.
          */
         if (TR::VectorInt64 == firstChild->getDataType())
            {
            dstReg = cg->allocateRegister(TR_VRF);
            }
         else //TR::VectorDouble == firstChild->getDataType()
            {
            dstReg = resReg;
            }

         /*
          * the value to be read needs to be in the least significant 64 bits.
          * if elem = 0, the value we want is in the most significant 64 bits and needs to be splatted into
          * the least significant 64 bits (the other bits affected by the splat are never read)
          * if elem = 1, the value we want is already in the least significant 64 bits
          * as a result, a mov instruction is good enough and splatting the value is unnecessary
          */
         if (1 == elem)
            {
            generateRegRegInstruction(MOVDQURegReg, node, dstReg, srcVectorReg, cg);
            }
         else //0 == elem
            {
            generateRegRegImmInstruction(PSHUFDRegRegImm1, node, dstReg, srcVectorReg, 0x0e, cg);
            }

         if (TR::VectorInt64 == firstChild->getDataType())
            {
            if (cg->comp()->target().is32Bit())
               {
               generateRegRegInstruction(MOVDReg4Reg, node, lowResReg, dstReg, cg);
               generateRegRegImmInstruction(PSHUFDRegRegImm1, node, dstReg, srcVectorReg, (0 == elem) ? 0x03 : 0x01, cg);
               generateRegRegInstruction(MOVDReg4Reg, node, highResReg, dstReg, cg);
               }
            else
               {
               generateRegRegInstruction(MOVQReg8Reg, node, resReg, dstReg, cg);
               }
            cg->stopUsingRegister(dstReg);
            }
         }
      }
   else
      {
      //TODO: handle non-constant second child case
      TR_ASSERT(false, "non-const second child not currently supported in SIMDgetvelemEvaluator.\n");
      }

   node->setRegister(resReg);
   cg->decReferenceCount(firstChild);
   cg->decReferenceCount(secondChild);

   return resReg;
   }

