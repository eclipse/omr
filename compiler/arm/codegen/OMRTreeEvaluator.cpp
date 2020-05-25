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

#include <stdint.h>
#include "arm/codegen/ARMInstruction.hpp"
#include "arm/codegen/ARMOperand2.hpp"
#include "codegen/AheadOfTimeCompile.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/ARMAOTRelocation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "codegen/CallSnippet.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterPair.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/VirtualGuard.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ParameterSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Annotations.hpp"
#include "infra/Bit.hpp"

// Used in buildJNIDispatch in ARMLinkage.cpp for calculating the magic number
uint32_t findARMLoadConstantLength(int32_t value)
   {
     intParts localVal(value);
     uint32_t bitValue    = value, notBits = ~bitValue;
     uint32_t bitTrailing = trailingZeroes(bitValue) & ~1;
     uint32_t notTrailing = trailingZeroes(notBits) & ~1;
     uint32_t base        = bitValue>>bitTrailing;
     uint32_t notBase     = notBits>>notTrailing;
     uint32_t result      = 1;

     if(((base & 0xFFFFFF00) == 0) || ((notBase & 0xFFFFFF00) == 0)) // is Immed8r || reversed immed8r
        {
        //result = 1;
        }
     else if((base & 0xFFFF0000) == 0)                               // 16bit Value
        {
        result = 2;
        }
     else if((notBase & 0xFFFF0000) == 0)                            // reversed 16bits
        {
        result = 3;
        }
     else if((base & 0xFF000000) == 0)                               // 24bit Value
        {
        result = 2;
        if(((base >> 8) & 0x000000FF) != 0)
           result = 3;
        }
     else
        {
        result = 4;
        }
     return result;
   }

// PLEASE READ:
// The code to determine the number of instructions to use is reflected in findARMLoadConstantLength
// Please change the code there as well when making a change here
TR::Instruction *armLoadConstant(TR::Node *node, int32_t value, TR::Register *trgReg, TR::CodeGenerator *cg, TR::Instruction *cursor)
   {
     intParts localVal(value);
     uint32_t bitValue    = value, notBits = ~bitValue;
     uint32_t bitTrailing = trailingZeroes(bitValue) & ~1;
     uint32_t notTrailing = trailingZeroes(notBits) & ~1;
     uint32_t base        = bitValue>>bitTrailing;
     uint32_t notBase     = notBits>>notTrailing;
     TR::Compilation *comp = cg->comp();

     if (comp->getOption(TR_TraceCG))
        {
        traceMsg(comp, "In armLoadConstant with\n");
        traceMsg(comp, "\tvalue = %d\n", value);
        traceMsg(comp, "\tnotBits = %d\n", notBits);
        traceMsg(comp, "\tbitTrailing = %d\n", bitTrailing);
        traceMsg(comp, "\tnotTrailing = %d\n", notTrailing);
        traceMsg(comp, "\tbase = %d\n", base);
        traceMsg(comp, "\tnotBase = %d\n", notBase);
        }

     TR::Instruction *insertingInstructions = cursor;
     if (cursor == NULL)
        cursor = cg->getAppendInstruction();

     // The straddled cases are not caught yet ...

     if ((base & 0xFFFFFF00) == 0)                                  // is Immed8r
        {
        if (bitTrailing == 32)
           {
           bitTrailing = 0;
           }
        cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, base & 0x000000FF, bitTrailing, cursor);
        }
     else if ((notBase & 0xFFFFFF00) == 0)                         // reversed immed8r
        {
        if (notTrailing == 32)
           {
           notTrailing = 0;
           }
        cursor = generateTrg1ImmInstruction(cg, ARMOp_mvn, node, trgReg, notBase & 0x000000FF, notTrailing, cursor);
        }
     else if ((base & 0xFFFF0000) == 0)                            // 16bit Value
        {
        cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, (base>>8) & 0x000000FF, 8 + bitTrailing, cursor);
        cursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, trgReg, trgReg, base & 0x000000FF, bitTrailing, cursor);
        }
     else if ((notBase & 0xFFFF0000) == 0)                         // reversed 16bits
        {
        cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, (notBase>>8) & 0x000000FF, 8 + notTrailing, cursor);
        cursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, trgReg, trgReg, notBase & 0x000000FF, notTrailing, cursor);
        cursor = generateTrg1Src1Instruction(cg, ARMOp_mvn, node, trgReg, trgReg, cursor);
        }
     else if ((base & 0xFF000000) == 0)                            // 24bit Value
        {
        cursor = generateTrg1ImmInstruction(cg, ARMOp_mov, node, trgReg, (base>>16) & 0x000000FF, 16 + bitTrailing, cursor);
        if (((base >> 8) & 0x000000FF) != 0)
           cursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, trgReg, trgReg, (base>>8) & 0x000000FF, 8 + bitTrailing, cursor);
        cursor = generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, trgReg, trgReg, base & 0x000000FF, bitTrailing, cursor);
        }
     else
        {
        TR_ARMOperand2 *op2_3 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte3(), 24);
        TR_ARMOperand2 *op2_2 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte2(), 16);
        TR_ARMOperand2 *op2_1 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte1(), 8);
        TR_ARMOperand2 *op2_0 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte0(), 0);
        cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, op2_3, cursor);
        cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_2, cursor);
        cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_1, cursor);
        cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_0, cursor);
        }

     if (!insertingInstructions)
        cg->setAppendInstruction(cursor);

     return(cursor);
   }

TR::Instruction *fixedSeqMemAccess(TR::CodeGenerator *cg, TR::Node *node, int32_t addr, TR::Instruction **nibbles, TR::Register *trgReg, TR::Instruction *cursor)
   {
   // Note: tempReg can be the same register as srcOrTrg. Caller needs to make sure it is right.
   TR::Instruction *cursorCopy = cursor;
   intParts localVal(addr);
   TR::Compilation *comp = cg->comp();

   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   TR_ARMOperand2 *op2_3 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte3(), 24);
   TR_ARMOperand2 *op2_2 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte2(), 16);
   TR_ARMOperand2 *op2_1 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte1(), 8);
   TR_ARMOperand2 *op2_0 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte0(), 0);

   nibbles[0] = cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, op2_3, cursor);
   nibbles[1] = cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_2, cursor);
   nibbles[2] = cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_1, cursor);
   nibbles[3] = cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_0, cursor);

   TR::MemoryReference *memRef = new (cg->trHeapMemory()) TR::MemoryReference(trgReg, 0, sizeof(addr), cg);
   nibbles[4] = cursor = generateTrg1MemInstruction(cg, ARMOp_ldr, node, trgReg, memRef, cursor);

   if (cursorCopy == NULL)
      cg->setAppendInstruction(cursor);

   return cursor;
   }

TR::Instruction *loadAddressConstantInSnippet(TR::CodeGenerator *cg, TR::Node * node, intptr_t address, TR::Register *trgReg, bool isUnloadablePicSite, TR::Instruction *cursor)
   {
   TR::Instruction *q[5];
   TR::Instruction *c = fixedSeqMemAccess(cg, node, 0, q, trgReg, cursor);
   cg->findOrCreateAddressConstant(&address, TR::Address, q[0], q[1], q[2], q[3], q[4], node, isUnloadablePicSite);
   return c;
   }

TR::Instruction *loadAddressConstantFixed(TR::CodeGenerator *cg, TR::Node * node, intptr_t value, TR::Register *trgReg, TR::Instruction *cursor, uint8_t *targetAddress, uint8_t *targetAddress2, int16_t typeAddress, bool doAOTRelocation)
   {
   TR::Compilation *comp = cg->comp();
   bool isAOT = comp->compileRelocatableCode();
   // load a 32-bit constant into a register with a fixed 4 instruction sequence
   TR::Instruction *temp = cursor;
   intParts localVal(value);

   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   TR_ARMOperand2 *op2_3 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte3(), 24);
   TR_ARMOperand2 *op2_2 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte2(), 16);
   TR_ARMOperand2 *op2_1 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte1(), 8);
   TR_ARMOperand2 *op2_0 = new (cg->trHeapMemory()) TR_ARMOperand2(localVal.getByte0(), 0);

   cursor = generateTrg1Src1Instruction(cg, ARMOp_mov, node, trgReg, op2_3, cursor);

   if (value != 0x0)
      {
#ifdef J9_PROJECT_SPECIFIC
      TR_FixedSequenceKind seqKind = fixedSequence1;//does not matter
      if (typeAddress == -1)
         {
         if (doAOTRelocation)
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                              cursor,
                              (uint8_t *)value,
                              (uint8_t *)seqKind,
                              TR_FixedSequenceAddress2, cg),
                              __FILE__,
                              __LINE__,
                              node);
         }
      else
         {
         if (typeAddress == TR_DataAddress)
            {
            if (doAOTRelocation)
               {
               TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
               recordInfo->data1 = (uintptr_t)node->getSymbolReference();
               recordInfo->data2 = (uintptr_t)node->getInlinedSiteIndex();
               recordInfo->data3 = (uintptr_t)seqKind;

               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 (uint8_t *)recordInfo,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
               }
            }
         else if ((typeAddress == TR_ClassAddress) || (typeAddress == TR_MethodObject))
            {
            if (doAOTRelocation)
               {
                TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
                recordInfo->data1 = (uintptr_t)targetAddress;
                recordInfo->data2 = (uintptr_t)targetAddress2;
                recordInfo->data3 = (uintptr_t)seqKind;

                cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                  cursor,
                                  (uint8_t *)recordInfo,
                                  (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                  __FILE__,
                                  __LINE__,
                                  node);
               }
            }
         else if (typeAddress == TR_ConstantPool)
            {
            if (doAOTRelocation)
               {
               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 targetAddress,
                                 targetAddress2,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
               }
            }
         else
            {
            if (doAOTRelocation)
               cg->addExternalRelocation(new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                                 cursor,
                                 (uint8_t *)value,
                                 (uint8_t *)seqKind,
                                 (TR_ExternalRelocationTargetKind)typeAddress, cg),
                                 __FILE__,
                                 __LINE__,
                                 node);
            }
         }
#endif
      }
   cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_2, cursor);
   cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_1, cursor);
   cursor = generateTrg1Src2Instruction(cg, ARMOp_add, node, trgReg, trgReg, op2_0, cursor);

   if (temp == NULL)
      cg->setAppendInstruction(cursor);

   return(cursor);
   }

TR::Instruction *loadAddressConstant(TR::CodeGenerator *cg, TR::Node * node, intptr_t value, TR::Register *trgReg, TR::Instruction *cursor, bool isPicSite, int16_t typeAddress, uint8_t *targetAddress, uint8_t *targetAddress2)
   {
   if (cg->comp()->compileRelocatableCode())
      return loadAddressConstantFixed(cg, node, value, trgReg, cursor, targetAddress, targetAddress2, typeAddress);

   return armLoadConstant(node, value, trgReg, cg, cursor);
   }

// also handles iiload
TR::Register *OMR::ARM::TreeEvaluator::iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   tempReg = cg->allocateRegister();
   if (node->getSymbolReference()->getSymbol()->isInternalPointer())
      {
      tempReg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      tempReg->setContainsInternalPointer();
      }

   node->setRegister(tempReg);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   generateTrg1MemInstruction(cg, ARMOp_ldr, node, tempReg, tempMR);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   tempMR->decNodeReferenceCounts();

   return tempReg;
   }

static bool nodeIsNeeded(TR::Node *checkNode, TR::Node *node)
   {
   bool result = (checkNode->getOpCode().isCall() ||
                 (checkNode != node && (checkNode->getOpCodeValue() == TR::aloadi || checkNode->getOpCodeValue() == TR::aload)));
   TR::Node *child = NULL;
   for (uint16_t i = 0; i < checkNode->getNumChildren() && !result; i++)
      {
      child = checkNode->getChild(i);
      if (child->getOpCode().isCall())
         result = true;
      // If the class ptr load is feeding anything other than a nullchk it's probably needed
      else if (child == node && !checkNode->getOpCode().isNullCheck())
         result = true;
      else if (child != node && (child->getOpCodeValue() == TR::aloadi || child->getOpCodeValue() == TR::aload))
         result = true;
      else
         result = nodeIsNeeded(child, node);
      if (result || child == node)
         break;
      }
   return result;
   }

// handles aload and iaload
TR::Register *OMR::ARM::TreeEvaluator::aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   // NEW to check for it feeding to a single vgdnop
   static bool disableLoadForVGDNOP = (feGetEnv("TR_DisableLoadForVGDNOP") != NULL);
   bool checkFeedingVGDNOP = !disableLoadForVGDNOP &&
      node->getOpCodeValue() == TR::aloadi &&
      node->getSymbolReference() &&
      node->getReferenceCount() == 2 &&
      !node->getSymbolReference()->isUnresolved() &&
      node->getSymbolReference()->getSymbol()->getKind() == TR::Symbol::IsShadow;

   if (OMR_UNLIKELY(checkFeedingVGDNOP))
      {
      TR::Node *topNode = cg->getCurrentEvaluationTreeTop()->getNode();
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t index = symRef->getReferenceNumber();
      int32_t numHelperSymbols = cg->symRefTab()->getNumHelperSymbols();
      if (!nodeIsNeeded(topNode, node) &&
         (comp->getSymRefTab()->isNonHelper(index, TR::SymbolReferenceTable::vftSymbol)) &&
          topNode->getOpCodeValue() != TR::treetop &&  // side effects free
         (!topNode->getOpCode().isNullCheck() || !topNode->getOpCode().isResolveCheck()))
         {
         // Search the current block for the feeding cmp to see
         TR::TreeTop *tt = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
         TR::Node *checkNode = NULL;
         while (tt && (tt->getNode()->getOpCodeValue() != TR::BBEnd))
            {
            checkNode = tt->getNode();
            if (cg->getSupportsVirtualGuardNOPing() &&
               (checkNode->isNopableInlineGuard() || checkNode->isHCRGuard()) &&
               (checkNode->getOpCodeValue() == TR::ificmpne || checkNode->getOpCodeValue() == TR::iflcmpne ||
                checkNode->getOpCodeValue() == TR::ifacmpne) &&
                checkNode->getFirstChild() == node)
               {
               // Try get the virtual guard
               TR_VirtualGuard *virtualGuard = comp->findVirtualGuardInfo(checkNode);
               if (!((comp->performVirtualGuardNOPing() || node->isHCRGuard()) &&
                   comp->isVirtualGuardNOPingRequired(virtualGuard)) &&
                   virtualGuard->canBeRemoved())
                  {
                  break;
                  }
               else
                  {
                  if (!topNode->getOpCode().isNullCheck() && !topNode->getOpCode().isResolveCheck())
                     node->decReferenceCount();
                  cg->recursivelyDecReferenceCount(node->getFirstChild());
                  TR::Register *tempReg = cg->allocateRegister(); // Empty register
                  node->setRegister(tempReg);
                  return tempReg;
                  }
               }
            tt = tt->getNextTreeTop();
            }
         }
      }

   TR::Register *tempReg;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (!node->getSymbolReference()->getSymbol()->isInternalPointer())
      {
      if (node->getSymbolReference()->getSymbol()->isNotCollected())
         tempReg = cg->allocateRegister();
      else
         tempReg = cg->allocateCollectedReferenceRegister();
      }
   else
      {
      tempReg = cg->allocateRegister();
      tempReg->setPinningArrayPointer(node->getSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
      tempReg->setContainsInternalPointer();
      }

   node->setRegister(tempReg);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   generateTrg1MemInstruction(cg, ARMOp_ldr, node, tempReg, tempMR);

#ifdef J9_PROJECT_SPECIFIC
   if (node->getSymbolReference() == cg->comp()->getSymRefTab()->findVftSymbolRef())
      {
      generateVFTMaskInstruction(cg, node, tempReg);
      }
#endif

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   tempMR->decNodeReferenceCounts();

   return tempReg;
   }

// also handles ilload
TR::Register *OMR::ARM::TreeEvaluator::lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register           *lowReg  = cg->allocateRegister();
   TR::Register           *highReg = cg->allocateRegister();
   TR::RegisterPair       *trgReg = cg->allocateRegisterPair(lowReg, highReg);
   TR::Compilation *comp = cg->comp();
   bool bigEndian = cg->comp()->target().cpu.isBigEndian();
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
      TR::SymbolReference *vrlRef = comp->getSymRefTab()->findOrCreateVolatileReadLongSymbolRef(comp->getMethodSymbol());

      generateTrg1MemInstruction(cg, ARMOp_add, node, bigEndian ? highReg: lowReg, tempMR);
      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(2, 2, cg->trMemory());
      TR::addDependency(deps, lowReg, bigEndian ? TR::RealRegister::gr1 : TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(deps, highReg, bigEndian ? TR::RealRegister::gr0 : TR::RealRegister::gr1, TR_GPR, cg);

      generateImmSymInstruction(cg, ARMOp_bl,
                                node, (uintptr_t)vrlRef->getMethodAddress(),
                                deps,
                                vrlRef);
      tempMR->decNodeReferenceCounts();
      //deps->stopUsingDepRegs(cg, lowReg, highReg);
      cg->machine()->setLinkRegisterKilled(true);
      }
   else
      {
      TR::MemoryReference *lowMR        = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
      TR::MemoryReference *highMR       = new (cg->trHeapMemory()) TR::MemoryReference(*lowMR, 4, 4, cg);

      generateTrg1MemInstruction(cg, ARMOp_ldr, node, bigEndian ? highReg : lowReg, lowMR);
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, bigEndian ? lowReg : highReg, highMR);
      lowMR->decNodeReferenceCounts();
      }
   node->setRegister(trgReg);
   return trgReg;
   }

// aloadEvaluator handled by iloadEvaluator


// also handles ibload
TR::Register *OMR::ARM::TreeEvaluator::bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, ARMOp_ldrsb, 1, cg);
   }

// also handles isload
TR::Register *OMR::ARM::TreeEvaluator::sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, ARMOp_ldrsh, 2, cg);
   }

// also handles icload
TR::Register *OMR::ARM::TreeEvaluator::cloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, ARMOp_ldrh, 2, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::commonLoadEvaluator(TR::Node *node,  TR_ARMOpCodes memToRegOp, int32_t memSize, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = node->setRegister(cg->allocateRegister());
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, memSize, cg);
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   generateTrg1MemInstruction(cg, memToRegOp, node, tempReg, tempMR);
   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   tempMR->decNodeReferenceCounts();
   return tempReg;
   }

#if J9_PROJECT_SPECIFIC
TR::Register *OMR::ARM::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference *tempMR              = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   TR::Register            *destinationRegister = cg->evaluate(node->getSecondChild());
   TR::Node                *firstChild = node->getFirstChild();
   TR::Register            *sourceRegister;
   bool killSource = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   if (firstChild->getReferenceCount() > 1 && firstChild->getRegister() != NULL)
      {
      if (!firstChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(firstChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, sourceRegister, firstChild->getRegister());
      killSource = true;
      }
   else
      sourceRegister = cg->evaluate(firstChild);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }

   generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, sourceRegister);

   VMwrtbarEvaluator(node, sourceRegister, destinationRegister, NULL, true, cg);

   cg->machine()->setLinkRegisterKilled(true);

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getFirstChild());
   cg->decReferenceCount(node->getSecondChild());
   tempMR->decNodeReferenceCounts();

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::awrtbariEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::MemoryReference *tempMR              = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
   TR::Register            *destinationRegister = cg->evaluate(node->getChild(2));
   TR::Node                *secondChild = node->getSecondChild();
   TR::Register            *sourceRegister;
   bool killSource = false;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());

   /* comp->useCompressedPointers() is false for 32bit environment, leaving the compressed pointer support unimplemented. */
   if (secondChild->getReferenceCount() > 1 && secondChild->getRegister() != NULL)
      {
      if (!secondChild->getRegister()->containsInternalPointer())
         sourceRegister = cg->allocateCollectedReferenceRegister();
      else
         {
         sourceRegister = cg->allocateRegister();
         sourceRegister->setPinningArrayPointer(secondChild->getRegister()->getPinningArrayPointer());
         sourceRegister->setContainsInternalPointer();
         }
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, sourceRegister, secondChild->getRegister());
      killSource = true;
      }
   else
      sourceRegister = cg->evaluate(secondChild);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }

   generateMemSrc1Instruction(cg, ARMOp_str, node, tempMR, sourceRegister);

   VMwrtbarEvaluator(node, sourceRegister, destinationRegister, NULL, true, cg);

   cg->machine()->setLinkRegisterKilled(true);

   if (killSource)
      cg->stopUsingRegister(sourceRegister);

   cg->decReferenceCount(node->getSecondChild());
   cg->decReferenceCount(node->getChild(2));
   tempMR->decNodeReferenceCounts();

   return NULL;
   }
#endif

// also handles ilstore
TR::Register *OMR::ARM::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *valueChild;
   TR::Compilation *comp = cg->comp();
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }
   bool bigEndian = cg->comp()->target().cpu.isBigEndian();
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   TR::Register *valueReg = cg->evaluate(valueChild);

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      TR::Register           *addrReg  = cg->allocateRegister();
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
      TR::SymbolReference *vwlRef = comp->getSymRefTab()->findOrCreateVolatileWriteLongSymbolRef(comp->getMethodSymbol());

      generateTrg1MemInstruction(cg, ARMOp_add, node, addrReg, tempMR);
      TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3, 3, cg->trMemory());
      TR::addDependency(deps, addrReg, TR::RealRegister::gr0, TR_GPR, cg);
      TR::addDependency(deps, valueReg->getLowOrder(), TR::RealRegister::gr1, TR_GPR, cg);
      TR::addDependency(deps, valueReg->getHighOrder(), TR::RealRegister::gr2, TR_GPR, cg);

      generateImmSymInstruction(cg, ARMOp_bl,
                                node, (uintptr_t)vwlRef->getMethodAddress(),
                                deps,
                                vwlRef);
      tempMR->decNodeReferenceCounts();
      //deps->stopUsingDepRegs(cg, valueReg->getLowOrder(), valueReg->getHighOrder());
      cg->machine()->setLinkRegisterKilled(true);
      }
   else
      {
      TR::MemoryReference *lowMR  = new (cg->trHeapMemory()) TR::MemoryReference(node, 4, cg);
      TR::MemoryReference *highMR = new (cg->trHeapMemory()) TR::MemoryReference(*lowMR, 4, 4, cg);

      if (bigEndian)
         {
         generateMemSrc1Instruction(cg, ARMOp_str, node, lowMR, valueReg->getHighOrder());
         generateMemSrc1Instruction(cg, ARMOp_str, node, highMR, valueReg->getLowOrder());
         }
      else
         {
         generateMemSrc1Instruction(cg, ARMOp_str, node, lowMR, valueReg->getLowOrder());
         generateMemSrc1Instruction(cg, ARMOp_str, node, highMR, valueReg->getHighOrder());
         }
      lowMR->decNodeReferenceCounts();
      }
   valueChild->decReferenceCount();
   return NULL;
   }

// also handles ibstore
TR::Register *OMR::ARM::TreeEvaluator::bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, ARMOp_strb, 1, cg);
   }

// also handles isstore, icstore, cstore,isstore,icstore
TR::Register *OMR::ARM::TreeEvaluator::sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, ARMOp_strh, 2, cg);
   }

// also handles astore, iastore, iistore
TR::Register *OMR::ARM::TreeEvaluator::istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, ARMOp_str, 4, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::commonStoreEvaluator(TR::Node *node, TR_ARMOpCodes memToRegOp, int32_t memSize, TR::CodeGenerator *cg)
   {
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, memSize, cg);
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && cg->comp()->target().isSMP());
   TR::Node *valueChild;
   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb_st, node);
      }
   generateMemSrc1Instruction(cg, memToRegOp, node, tempMR, cg->evaluate(valueChild));
   if (needSync && cg->comp()->target().cpu.id() != TR_DefaultARMProcessor)
      {
      generateInstruction(cg, (cg->comp()->target().cpu.id() == TR_ARMv6) ? ARMOp_dmb_v6 : ARMOp_dmb, node);
      }
   valueChild->decReferenceCount();
   tempMR->decNodeReferenceCounts();
   return NULL;
   }

#ifdef J9_PROJECT_SPECIFIC
TR::Register *OMR::ARM::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return VMmonentEvaluator(node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return VMmonexitEvaluator(node, cg);
   }
#endif

TR::Register *OMR::ARM::TreeEvaluator::integerHighestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerLowestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::integerBitCount(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longHighestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longLowestOneBit(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longNumberOfLeadingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longNumberOfTrailingZeros(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }
TR::Register *OMR::ARM::TreeEvaluator::longBitCount(TR::Node *node, TR::CodeGenerator *cg) { TR_UNIMPLEMENTED(); return NULL; }

TR::Register *OMR::ARM::TreeEvaluator::reverseLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::reverseStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR_UNIMPLEMENTED();
   return NULL;
   }

bool OMR::ARM::TreeEvaluator::stopUsingCopyReg(
      TR::Node* node,
      TR::Register*& reg,
      TR::CodeGenerator* cg)
   {
   if (node != NULL)
      {
      reg = cg->evaluate(node);
      if (!cg->canClobberNodesRegister(node))
         {
         TR::Register *copyReg;
         if (reg->containsInternalPointer() ||
             !reg->containsCollectedReference())
            {
            copyReg = cg->allocateRegister();
            if (reg->containsInternalPointer())
               {
               copyReg->setPinningArrayPointer(reg->getPinningArrayPointer());
               copyReg->setContainsInternalPointer();
               }
            }
         else
            copyReg = cg->allocateCollectedReferenceRegister();

         generateTrg1Src1Instruction(cg, ARMOp_mov, node, copyReg, reg);
         reg = copyReg;
         return true;
         }
      }

   return false;
   }

static void constLengthArrayCopyEvaluator(TR::Node *node, int32_t byteLen, TR::Register *src, TR::Register *dst, TR::CodeGenerator *cg)
   {
   int32_t groups = byteLen >> 4;
   int32_t residual=byteLen & 0x0000000F;
   int32_t ri_x = 0, ri;
   TR::Register                        *regs[4] = {NULL, NULL, NULL, NULL};

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(7,7, cg->trMemory());

   TR::addDependency(deps, src, TR::RealRegister::NoReg, TR_GPR, cg);
   deps->getPreConditions()->getRegisterDependency(0)->setExcludeGPR0();
   deps->getPostConditions()->getRegisterDependency(0)->setExcludeGPR0();
   TR::addDependency(deps, dst, TR::RealRegister::NoReg, TR_GPR, cg);
   deps->getPreConditions()->getRegisterDependency(1)->setExcludeGPR0();
   deps->getPostConditions()->getRegisterDependency(1)->setExcludeGPR0();


   if (byteLen == 0)
      {
      return;
      }

   regs[0] = cg->allocateRegister(TR_GPR); //as byteLen > 0 here, will need at least one register
   TR::addDependency(deps, regs[0], TR::RealRegister::NoReg, TR_GPR, cg);
   if (groups != 0)
      {
      regs[1] = cg->allocateRegister(TR_GPR);
      regs[2] = cg->allocateRegister(TR_GPR);
      regs[3] = cg->allocateRegister(TR_GPR);
      TR::addDependency(deps, regs[1], TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, regs[2], TR::RealRegister::NoReg, TR_GPR, cg);
      TR::addDependency(deps, regs[3], TR::RealRegister::NoReg, TR_GPR, cg);

      TR::Register *countReg = cg->allocateRegister();
      if (groups <= UPPER_IMMED12)
         generateTrg1ImmInstruction(cg, ARMOp_mov, node, countReg, groups, 0);


      else
         armLoadConstant(node, groups,countReg,cg);

      TR::LabelSymbol *loopStart = TR::LabelSymbol::create(cg->trHeapMemory(),cg);
      generateLabelInstruction(cg, ARMOp_label, node, loopStart);

      for( ri = 0; ri < 4; ri++)
         {
         TR::MemoryReference *mr = new (cg->trHeapMemory()) TR::MemoryReference(src, 4*ri, cg);
       //  mr->setImmediatePreIndexed();  //store the base + offset back to the base reg
         generateTrg1MemInstruction(cg, ARMOp_ldr, node, regs[ri], mr);
         }
      for( ri = 0; ri < 4; ri++)
         {
         TR::MemoryReference *mr = new (cg->trHeapMemory()) TR::MemoryReference(dst, 4*ri, cg);
       //  mr->setImmediatePreIndexed();
         generateMemSrc1Instruction(cg, ARMOp_str, node, mr, regs[ri]);
         }

      generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, src, src, 16, 0);
      generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, dst, dst, 16, 0);
      generateTrg1Src1ImmInstruction(cg, ARMOp_sub_r, node, countReg, countReg, 1, 0);
      generateConditionalBranchInstruction(cg, node, ARMConditionCodeNE, loopStart);

    }

   for ( ri = 0; ri < residual>>2; ri++)
      {
      //TR::Register *oneReg = regs[ri];
      generateTrg1MemInstruction(cg, ARMOp_ldr, node, regs[0], new (cg->trHeapMemory()) TR::MemoryReference(src, 4*ri, cg));
      generateMemSrc1Instruction(cg, ARMOp_str, node, new (cg->trHeapMemory()) TR::MemoryReference(dst, 4*ri, cg), regs[0]);
      }
   if(residual & 2)
      {
      generateTrg1MemInstruction(cg, ARMOp_ldrh, node, regs[0], new (cg->trHeapMemory()) TR::MemoryReference(src, 4*ri, cg));
      generateMemSrc1Instruction(cg, ARMOp_strh, node, new (cg->trHeapMemory()) TR::MemoryReference(dst, 4*ri, cg), regs[0]);
      ri++;
      ri_x = 2;
      }
   if(residual & 1)
      {
      generateTrg1MemInstruction(cg, ARMOp_ldrb, node, regs[0], new (cg->trHeapMemory()) TR::MemoryReference(src, (4*ri)+ri_x, cg));
      generateMemSrc1Instruction(cg, ARMOp_strb, node, new (cg->trHeapMemory()) TR::MemoryReference(dst, (4*ri)+ri_x, cg), regs[0]);
      }

   return;

   }

TR::Register *OMR::ARM::TreeEvaluator::arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {

   TR::Node             *srcObjNode, *dstObjNode, *srcAddrNode, *dstAddrNode, *lengthNode;
   TR::Register         *srcObjReg=NULL, *dstObjReg=NULL, *srcAddrReg, *dstAddrReg, *lengthReg;
   bool stopUsingCopyReg1, stopUsingCopyReg2, stopUsingCopyReg3, stopUsingCopyReg4, stopUsingCopyReg5 = false;
   TR::SymbolReference *arrayCopyHelper;
   FILE *outFile;

   bool isSimpleCopy = (node->getNumChildren() == 3);

   if (isSimpleCopy)
      {
      srcObjNode = NULL;
      dstObjNode = NULL;
      srcAddrNode = node->getChild(0);
      dstAddrNode = node->getChild(1);
      lengthNode = node->getChild(2);
      }
   else
      {
      srcObjNode = node->getChild(0);
      dstObjNode = node->getChild(1);
      srcAddrNode = node->getChild(2);
      dstAddrNode = node->getChild(3);
      lengthNode = node->getChild(4);
      }

   stopUsingCopyReg1 = stopUsingCopyReg(srcObjNode, srcObjReg, cg);
   stopUsingCopyReg2 = stopUsingCopyReg(dstObjNode, dstObjReg, cg);
   stopUsingCopyReg3 = stopUsingCopyReg(srcAddrNode, srcAddrReg, cg);
   stopUsingCopyReg4 = stopUsingCopyReg(dstAddrNode, dstAddrReg, cg);

   lengthReg = cg->evaluate(lengthNode);
   if (!cg->canClobberNodesRegister(lengthNode))
      {
      TR::Register *lenCopyReg = cg->allocateRegister();
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, lenCopyReg, lengthReg);
      lengthReg = lenCopyReg;
      stopUsingCopyReg5 = true;
      }

   TR::RegisterDependencyConditions *deps = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(3,3, cg->trMemory());
   // Build up the dependency conditions
   // r0 is length in bytes, r1 is srcAddr, r2 is dstAddr
   TR::addDependency(deps, lengthReg, TR::RealRegister::gr0, TR_GPR, cg);
   TR::addDependency(deps, srcAddrReg, TR::RealRegister::gr1, TR_GPR, cg);
   TR::addDependency(deps, dstAddrReg, TR::RealRegister::gr2, TR_GPR, cg);

   arrayCopyHelper = cg->symRefTab()->findOrCreateRuntimeHelper(TR_ARMarrayCopy, false, false, false);

   generateImmSymInstruction(cg, ARMOp_bl,
                                   node, (uintptr_t)arrayCopyHelper->getMethodAddress(),
                                   deps,
                                   arrayCopyHelper);

#ifdef J9_PROJECT_SPECIFIC
   if (!isSimpleCopy)
      {
      TR::TreeEvaluator::genWrtbarForArrayCopy(node, srcObjReg, dstObjReg, cg);
      }
#endif

   if (srcObjNode != NULL)
      cg->decReferenceCount(srcObjNode);
   if (dstObjNode != NULL)
      cg->decReferenceCount(dstObjNode);
   cg->decReferenceCount(srcAddrNode);
   cg->decReferenceCount(dstAddrNode);
   cg->decReferenceCount(lengthNode);
   if (stopUsingCopyReg1)
      cg->stopUsingRegister(srcObjReg);
   if (stopUsingCopyReg2)
      cg->stopUsingRegister(dstObjReg);
   if (stopUsingCopyReg3)
      cg->stopUsingRegister(srcAddrReg);
   if (stopUsingCopyReg4)
      cg->stopUsingRegister(dstAddrReg);
   cg->machine()->setLinkRegisterKilled(true);
   cg->setHasCall();
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // The child contains an inline test. If it succeeds, the helper is called.
   // The address of the helper is contained as an int in this node.
   //
   TR::Node     *testNode = node->getFirstChild();
   TR::Node     *firstChild       = testNode->getFirstChild();
   TR::Register *src1Reg   = cg->evaluate(firstChild);
   TR::Node *secondChild = testNode->getSecondChild();
   TR::SymbolReference *asynccheckHelper;

   bool biggerCheck = true;

   if (secondChild->getOpCode().isLoadConst() &&
       secondChild->getRegister() == NULL)
      {
      bool negated;
      uint32_t base, rotate;
      int32_t tmpChild = secondChild->getInt();
      negated = tmpChild < 0 && tmpChild != 0x80000000; //the negative of the maximum negative int is itself so can not use cmn here

      if(constantIsImmed8r(negated ? -tmpChild : tmpChild, &base, &rotate))
         {
         generateSrc1ImmInstruction(cg, negated ? ARMOp_cmn : ARMOp_cmp, node, src1Reg, base, rotate);
         biggerCheck = false;
         }
      }

   if(biggerCheck)
      {
      TR::Register *src2Reg   = cg->evaluate(secondChild);
      generateSrc2Instruction(cg, ARMOp_cmp, node, src2Reg, src1Reg);
      }

   TR::Instruction *gcPoint;
   TR_ASSERT(testNode->getOpCodeValue() == TR::icmpeq, "opcode not icmpeq - find new condition");

   asynccheckHelper = node->getSymbolReference();
   gcPoint = generateImmSymInstruction(cg, ARMOp_bl, node, (uintptr_t)asynccheckHelper->getMethodAddress(), NULL, asynccheckHelper, NULL, NULL, ARMConditionCodeEQ);
   gcPoint->ARMNeedsGCMap(0xFFFFFFFF);
   cg->machine()->setLinkRegisterKilled(true);

   firstChild->decReferenceCount();
   secondChild->decReferenceCount();
   testNode->decReferenceCount();
   return NULL;
   }

#if J9_PROJECT_SPECIFIC
TR::Register *OMR::ARM::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return VMinstanceOfEvaluator(node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return OMR::ARM::TreeEvaluator::VMcheckcastEvaluator(node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return OMR::ARM::TreeEvaluator::VMcheckcastEvaluator(node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::newObjectEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return VMnewEvaluator(node, cg);
   }

TR::Register *OMR::ARM::TreeEvaluator::newArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      {
      return VMnewEvaluator(node, cg);
      }
   }

TR::Register *OMR::ARM::TreeEvaluator::anewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   if (cg->comp()->suppressAllocationInlining())
      {
      TR::ILOpCodes opCode = node->getOpCodeValue();
      TR::Node::recreate(node, TR::acall);
      TR::Register *targetRegister = directCallEvaluator(node, cg);
      TR::Node::recreate(node, opCode);
      return targetRegister;
      }
   else
      {
      return VMnewEvaluator(node, cg);
      }

   }
#endif

TR::Register *OMR::ARM::TreeEvaluator::multianewArrayEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::ILOpCodes opCode = node->getOpCodeValue();
   TR::Node::recreate(node, TR::acall);
   TR::Register *targetRegister = directCallEvaluator(node, cg);
   TR::Node::recreate(node, opCode);
   return targetRegister;
   }

// handles: TR::call, TR::acall, TR::icall, TR::lcall, TR::fcall, TR::dcall
TR::Register *OMR::ARM::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage = cg->getLinkage(callee->getLinkageConvention());

   return linkage->buildDirectDispatch(node);
   }

TR::Register *OMR::ARM::TreeEvaluator::performCall(TR::Node *node, bool isIndirect, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef     = node->getSymbolReference();
   TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage = cg->getLinkage(callee->getLinkageConvention());
   TR::Register *returnRegister;

   if (isIndirect)
      returnRegister = linkage->buildIndirectDispatch(node);
   else
      returnRegister = linkage->buildDirectDispatch(node);

   return returnRegister;

   }

// handles: TR::icalli, TR::acalli, TR::fcalli, TR::dcalli, TR::lcalli, TR::calli
TR::Register *OMR::ARM::TreeEvaluator::indirectCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol    *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage      *linkage = cg->getLinkage(callee->getLinkageConvention());

   return linkage->buildIndirectDispatch(node);
   }

TR::Register *OMR::ARM::TreeEvaluator::treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->evaluate(node->getFirstChild());
   node->getFirstChild()->decReferenceCount();
   return tempReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   generateAdminInstruction(cg, ARMOp_fence, node, node);
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   // TODO: DANGER: the following needs rewriting...
   TR::Register            *resultReg;
   TR::Symbol *sym = node->getSymbol();
   TR::MemoryReference  *mref = new (cg->trHeapMemory()) TR::MemoryReference(node, node->getSymbolReference(), 4, cg);

   if (mref->getUnresolvedSnippet() != NULL)
      {
      resultReg = sym->isLocalObject() ?  cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
      if (mref->useIndexedForm())
         {
         cg->comp()->failCompilation<TR::CompilationException>("implement unresolved loadAddr indexed");
         generateTrg1MemInstruction(cg, ARMOp_add, node, resultReg, mref);
         }
      else
         {
         // the following is a HACK that assumes that the resolve will
         // have the addr in a register and fix up the next instruction
         // to do the right thing.
         generateTrg1MemInstruction(cg, ARMOp_mov, node, resultReg, mref);
         }
      }
   else
      {
      if (mref->useIndexedForm())
         {
         resultReg = sym->isLocalObject() ?  cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
         generateTrg1Src2Instruction(cg, ARMOp_add, node, resultReg, mref->getBaseRegister(), mref->getIndexRegister());
         }
      else
         {
         int32_t offset = mref->getOffset();
         if (mref->hasDelayedOffset() || offset != 0)
            {
            resultReg = sym->isLocalObject() ?  cg->allocateCollectedReferenceRegister() : cg->allocateRegister();
            if (mref->hasDelayedOffset())
               {
               generateTrg1MemInstruction(cg, ARMOp_add, node, resultReg, mref);
               }
            else
               {
               uint32_t base, rotate;

               if (constantIsImmed8r(offset, &base, &rotate))
                  {
                  generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, resultReg, mref->getBaseRegister(), base, rotate);
                  }
               else
                  {
                  armLoadConstant(node, offset, resultReg, cg, NULL);
                  generateTrg1Src2Instruction(cg, ARMOp_add, node, resultReg, mref->getBaseRegister(), resultReg);
                  }
               }
            }
         else
            {
            resultReg = mref->getBaseRegister();
            if (resultReg == cg->getMethodMetaDataRegister())
               {
               resultReg = cg->allocateRegister();
               generateTrg1Src1Instruction(cg, ARMOp_mov, node, resultReg, mref->getBaseRegister());
               }
            }
         }
      }
   node->setRegister(resultReg);
   return resultReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      if (node->getRegLoadStoreSymbolReference()->getSymbol()->isNotCollected() ||
          node->getRegLoadStoreSymbolReference()->getSymbol()->isInternalPointer())
         {
         globalReg = cg->allocateRegister();
         if (node->getRegLoadStoreSymbolReference()->getSymbol()->isInternalPointer())
            {
            globalReg->setContainsInternalPointer();
            globalReg->setPinningArrayPointer(node->getRegLoadStoreSymbolReference()->getSymbol()->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            }
         }
      else
         globalReg = cg->allocateCollectedReferenceRegister();

      node->setRegister(globalReg);
      }
   return globalReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister();
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

TR::Register *OMR::ARM::TreeEvaluator::lRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegisterPair(cg->allocateRegister(),
                                                cg->allocateRegister());
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

// Also handles TR::aRegStore
TR::Register *OMR::ARM::TreeEvaluator::iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   child->decReferenceCount();
   return globalReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::lRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   child->decReferenceCount();
   return globalReg;
   }

TR::Register *OMR::ARM::TreeEvaluator::GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int i;

   for (i=0; i<node->getNumChildren(); i++)
      {
      cg->evaluate(node->getChild(i));
      node->getChild(i)->decReferenceCount();
      }
   return(NULL);
   }


//also handles i2a, iu2a, a2i, a2l
TR::Register *OMR::ARM::TreeEvaluator::passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *targetRegister = cg->evaluate(node->getFirstChild());
   node->setRegister(targetRegister);
   node->getFirstChild()->decReferenceCount();
   return targetRegister;
   }

TR::Register *OMR::ARM::TreeEvaluator::BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Block *block = node->getBlock();
   TR::RegisterDependencyConditions *deps = NULL;

   if (!block->isExtensionOfPreviousBlock() && node->getNumChildren()>0)
      {
      int32_t     i;
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      deps = generateRegisterDependencyConditions(cg, child, 0);
      if (cg->getCurrentEvaluationTreeTop() == comp->getStartTree())
         {
         for (i=0; i<child->getNumChildren(); i++)
            {
            TR::ParameterSymbol *sym = child->getChild(i)->getSymbolReference()->getSymbol()->getParmSymbol();
            if (sym != NULL)
               {
               sym->setAssignedGlobalRegisterIndex(cg->getGlobalRegister(child->getChild(i)->getGlobalRegisterNumber()));
               }
            }
         }
      child->decReferenceCount();
      }

   if (node->getLabel() != NULL)
      {
      node->getLabel()->setInstruction(generateLabelInstruction(cg, ARMOp_label, node, node->getLabel(), deps));
      deps = NULL; // put the dependencies (if any) on either the label or the fence
      }

   TR::Node * fenceNode = TR::Node::createRelative32BitFenceNode(node, &block->getInstructionBoundaries()._startPC);
   TR::Instruction * fence = generateAdminInstruction(cg, ARMOp_fence, node, deps, fenceNode);

   if (block->isCatchBlock())
      {
      cg->generateCatchBlockBBStartPrologue(node, fence);
      }

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Block * block = node->getBlock();
   TR::Compilation *comp = cg->comp();
   TR::TreeTop *nextTT    = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();
   TR::Node    *fenceNode = TR::Node::createRelative32BitFenceNode(node, &node->getBlock()->getInstructionBoundaries()._endPC);

   if (NULL == block->getNextBlock())
      {
      // PR108736 If bl jitThrowException is the last instruction, jitGetExceptionTableFromPC fails to find the method.
      TR::Instruction *lastInstruction = cg->getAppendInstruction();
      if (lastInstruction->getOpCodeValue() == ARMOp_bl
              && lastInstruction->getNode()->getSymbolReference()->getReferenceNumber() == TR_aThrow)
         {
         lastInstruction = generateInstruction(cg, ARMOp_bad, node, lastInstruction);
         }
      }
   TR::RegisterDependencyConditions *deps = NULL;
   if (node->getNumChildren() > 0 &&
       (!nextTT || !nextTT->getNode()->getBlock()->isExtensionOfPreviousBlock()))
      {
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      deps = generateRegisterDependencyConditions(cg, child, 0);
      child->decReferenceCount();
      }

   // put the dependencies (if any) on the fence
   generateAdminInstruction(cg, ARMOp_fence, node, deps, fenceNode);

   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::NOPEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::badILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }

TR::Register *OMR::ARM::TreeEvaluator::unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return NULL;
   }


TR::Register *OMR::ARM::TreeEvaluator::conversionAnalyser(TR::Node          *node,
                                                     TR_ARMOpCodes    memoryToRegisterOp,
                                                     bool needSignExtend,
                                                     int dstBits,
                                                     TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *sourceRegister;
   TR::Register *targetRegister;

   if (child->getReferenceCount() == 1 && child->getRegister() == NULL && child->getOpCode().isMemoryReference())
      {
      TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(child, dstBits >> 3, cg);
      if (cg->comp()->target().cpu.isBigEndian() && node->getSize() < child->getSize())
         {
         tempMR->addToOffset(node, child->getSize() - (dstBits>>3), cg);
         }
      targetRegister = cg->allocateRegister();
      generateTrg1MemInstruction(cg, memoryToRegisterOp, node, targetRegister, tempMR);
      tempMR->decNodeReferenceCounts();
      }
   else
      {
      targetRegister = cg->allocateRegister();
      sourceRegister = child->getOpCode().isLong() ? cg->evaluate(child)->getLowOrder() : cg->evaluate(child);
      generateSignOrZeroExtend(node, targetRegister, sourceRegister, needSignExtend, dstBits, cg);
      }
   node->setRegister(targetRegister);
   child->decReferenceCount();

   return targetRegister;
   }


static void generateSignOrZeroExtend(TR::Node *node, TR::Register *dst, TR::Register *src, bool needSignExtend, int32_t bitsInDst, TR::CodeGenerator *cg)
   {
   TR_ARMOpCodes opcode = ARMOp_bad;

   if (cg->comp()->target().cpu.id() >= TR_ARMv6)
      {
      // sxtb/sxth/uxtb/uxth instructions are unavailable in ARMv5 and older
      if (needSignExtend)
         {
         if (bitsInDst == 8) // byte
            {
            opcode = ARMOp_sxtb;
            }
         else if (bitsInDst == 16) // short
            {
            opcode = ARMOp_sxth;
            }
         }
      else
         {
         if (bitsInDst == 8) // byte
            {
            opcode = ARMOp_uxtb;
            }
         else if (bitsInDst == 16) // short
            {
            opcode = ARMOp_uxth;
            }
         }
      }

   if (opcode != ARMOp_bad)
      {
      generateTrg1Src1Instruction(cg, opcode, node, dst, src);
      }
   else
      {
      TR_ARMOperand2 *op;

      op = new (cg->trHeapMemory()) TR_ARMOperand2(ARMOp2RegLSLImmed, src, 32 - bitsInDst);
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dst, op);
      op = new (cg->trHeapMemory()) TR_ARMOperand2((needSignExtend ? ARMOp2RegASRImmed : ARMOp2RegLSRImmed), dst, 32 - bitsInDst);
      generateTrg1Src1Instruction(cg, ARMOp_mov, node, dst, op);
      }
   }
