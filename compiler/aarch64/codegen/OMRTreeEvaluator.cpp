/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#include "codegen/ARM64ShiftCode.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/LabelSymbol.hpp"
#include "il/symbol/ParameterSymbol.hpp"

TR::Instruction *loadConstant32(TR::CodeGenerator *cg, TR::Node *node, int32_t value, TR::Register *trgReg, TR::Instruction *cursor)
   {
   TR::Instruction *insertingInstructions = cursor;
   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   TR::InstOpCode::Mnemonic op = TR::InstOpCode::bad;
   uint32_t imm;

   if (value >= 0 && value <= 65535)
      {
      op = TR::InstOpCode::movzw;
      imm = value & 0xFFFF;
      }
   else if (value >= -65535 && value < 0)
      {
      op = TR::InstOpCode::movnw;
      imm = ~value & 0xFFFF;
      }
   else if ((value & 0xFFFF) == 0)
      {
      op = TR::InstOpCode::movzw;
      imm = ((value >> 16) & 0xFFFF) | TR::MOV_LSL16;
      }
   else if ((value & 0xFFFF) == 0xFFFF)
      {
      op = TR::InstOpCode::movnw;
      imm = ((~value >> 16) & 0xFFFF) | TR::MOV_LSL16;
      }

   if (op != TR::InstOpCode::bad)
      {
      cursor = generateTrg1ImmInstruction(cg, op, node, trgReg, imm, cursor);
      }
   else
      {
      // need two instructions
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::movzw, node, trgReg,
                                          (value & 0xFFFF), cursor);
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::movkw, node, trgReg,
                                          (((value >> 16) & 0xFFFF) | TR::MOV_LSL16), cursor);
      }

   if (!insertingInstructions)
      cg->setAppendInstruction(cursor);

   return cursor;
   }

TR::Instruction *loadConstant64(TR::CodeGenerator *cg, TR::Node *node, int64_t value, TR::Register *trgReg, TR::Instruction *cursor)
   {
   TR::Instruction *insertingInstructions = cursor;
   if (cursor == NULL)
      cursor = cg->getAppendInstruction();

   if (value == 0LL)
      {
      // 0
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::movzx, node, trgReg, 0, cursor);
      }
   else if (~value == 0LL)
      {
      // -1
      cursor = generateTrg1ImmInstruction(cg, TR::InstOpCode::movnx, node, trgReg, 0, cursor);
      }
   else
      {
      uint16_t h[4];
      int32_t count0000 = 0, countFFFF = 0;
      int32_t use_movz;
      int32_t i;

      for (i = 0; i < 4; i++)
         {
         h[i] = (value >> (i * 16)) & 0xFFFF;
         if (h[i] == 0)
            {
            count0000++;
            }
         else if (h[i] == 0xFFFF)
            {
            countFFFF++;
            }
         }
      use_movz = (count0000 >= countFFFF);

      TR::Instruction *start = cursor;

      for (i = 0; i < 4; i++)
         {
         uint32_t shift = TR::MOV_LSL16 * i;
         TR::InstOpCode::Mnemonic op = TR::InstOpCode::bad;
         uint32_t imm;

         if (use_movz && (h[i] != 0))
            {
            imm = h[i] | shift;
            if (cursor != start)
               {
               op = TR::InstOpCode::movkx;
               }
            else
               {
               op = TR::InstOpCode::movzx;
               }
            }
         else if (!use_movz && (h[i] != 0xFFFF))
            {
            if (cursor != start)
               {
               op = TR::InstOpCode::movkx;
               imm = h[i] | shift;
               }
            else
               {
               op = TR::InstOpCode::movnx;
               imm = (~h[i] & 0xFFFF) | shift;
               }
            }

         if (op != TR::InstOpCode::bad)
            {
            cursor = generateTrg1ImmInstruction(cg, op, node, trgReg, imm, cursor);
            }
         else
            {
            // generate no instruction here
            }
         }
      }

   if (!insertingInstructions)
      cg->setAppendInstruction(cursor);

   return cursor;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::unImpOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	TR_ASSERT_FATAL(false, "Opcode %s is not implemented\n", node->getOpCode().getName());
	return NULL;
	}

TR::Register *
OMR::ARM64::TreeEvaluator::badILOpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::badILOpEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *commonLoadEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, int32_t memSize, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg;
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP());

   if (op == TR::InstOpCode::vldrimms)
      {
      tempReg = cg->allocateSinglePrecisionRegister();
      }
   else if (op == TR::InstOpCode::vldrimmd)
      {
      tempReg = cg->allocateRegister(TR_FPR);
      }
   else
      {
      tempReg = cg->allocateRegister();
      }
   node->setRegister(tempReg);
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, memSize, cg);
   generateTrg1MemInstruction(cg, op, node, tempReg, tempMR);

   /*
    * Enable this part when dmb instruction becomes available
   if (needSync)
      {
      generateInstruction(cg, TR::InstOpCode::dmb, node);
      }
    */
   tempMR->decNodeReferenceCounts(cg);

   return tempReg;
   }

// also handles iloadi
TR::Register *
OMR::ARM64::TreeEvaluator::iloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::ldrimmw, 4, cg);
   }

// also handles aloadi
TR::Register *
OMR::ARM64::TreeEvaluator::aloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Register *tempReg;

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

   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, 8, cg);
   generateTrg1MemInstruction(cg, TR::InstOpCode::ldrimmx, node, tempReg, tempMR);

   /*
    * Enable this part when dmb instruction becomes available
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP());
   if (needSync)
      {
      generateInstruction(cg, TR::InstOpCode::dmb, node);
      }
    */
   tempMR->decNodeReferenceCounts(cg);

   return tempReg;
   }

// also handles lloadi
TR::Register *
OMR::ARM64::TreeEvaluator::lloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::ldrimmx, 8, cg);
   }

// also handles bloadi
TR::Register *
OMR::ARM64::TreeEvaluator::bloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::ldrsbimmx, 1, cg);
   }

// also handles sloadi
TR::Register *
OMR::ARM64::TreeEvaluator::sloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::ldrshimmx, 2, cg);
   }

// also handles cloadi
TR::Register *
OMR::ARM64::TreeEvaluator::cloadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonLoadEvaluator(node, TR::InstOpCode::ldrhimm, 2, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::awrtbarEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::awrtbarEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *commonStoreEvaluator(TR::Node *node, TR::InstOpCode::Mnemonic op, int32_t memSize, TR::CodeGenerator *cg)
   {
   TR::MemoryReference *tempMR = new (cg->trHeapMemory()) TR::MemoryReference(node, memSize, cg);
   bool needSync = (node->getSymbolReference()->getSymbol()->isSyncVolatile() && TR::Compiler->target.isSMP());
   TR::Node *valueChild;

   if (node->getOpCode().isIndirect())
      {
      valueChild = node->getSecondChild();
      }
   else
      {
      valueChild = node->getFirstChild();
      }

   /*
    * Enable this part when dmb instruction becomes available
   if (needSync)
      {
      generateInstruction(cg, TR::InstOpCode::dmb, node);
      }
    */
   generateMemSrc1Instruction(cg, op, node, tempMR, cg->evaluate(valueChild));
   /*
    * Enable this part when dmb instruction becomes available
   if (needSync)
      {
      generateInstruction(cg, TR::InstOpCode::dmb, node);
      }
    */

   valueChild->decReferenceCount();
   tempMR->decNodeReferenceCounts(cg);

   return NULL;
   }

// also handles lstorei, astore, astorei
TR::Register *
OMR::ARM64::TreeEvaluator::lstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::strimmx, 8, cg);
   }

// also handles bstorei
TR::Register *
OMR::ARM64::TreeEvaluator::bstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::strbimm, 1, cg);
   }

// also handles sstorei, cstore, cstorei
TR::Register *
OMR::ARM64::TreeEvaluator::sstoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::strhimm, 2, cg);
   }

// also handles istorei
TR::Register *
OMR::ARM64::TreeEvaluator::istoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   return commonStoreEvaluator(node, TR::InstOpCode::strimmw, 4, cg);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::monentEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::monentEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::monexitEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::monexitEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::monexitfenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::monexitfenceEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::arraytranslateAndTestEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::arraytranslateAndTestEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::arraytranslateEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::arraytranslateEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::arraysetEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::arraysetEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::arraycmpEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::arraycmpEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::arraycopyEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::arraycopyEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::asynccheckEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::asynccheckEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::instanceofEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::instanceofEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::checkcastEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::checkcastEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::checkcastAndNULLCHKEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::checkcastAndNULLCHKEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

// handles call, icall, lcall, fcall, dcall, acall
TR::Register *
OMR::ARM64::TreeEvaluator::directCallEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::SymbolReference *symRef = node->getSymbolReference();
   TR::MethodSymbol *callee = symRef->getSymbol()->castToMethodSymbol();
   TR::Linkage *linkage = cg->getLinkage(callee->getLinkageConvention());

   return linkage->buildDirectDispatch(node);
   }

TR::Register *
OMR::ARM64::TreeEvaluator::treetopEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *tempReg = cg->evaluate(node->getFirstChild());
   cg->decReferenceCount(node->getFirstChild());
   return tempReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::exceptionRangeFenceEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::exceptionRangeFenceEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::loadaddrEvaluator(TR::Node *node, TR::CodeGenerator *cg)
	{
	// TODO:ARM64: Enable TR::TreeEvaluator::loadaddrEvaluator in compiler/aarch64/codegen/TreeEvaluatorTable.hpp when Implemented.
	return OMR::ARM64::TreeEvaluator::unImpOpEvaluator(node, cg);
	}

TR::Register *
OMR::ARM64::TreeEvaluator::aRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
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
         {
         globalReg = cg->allocateCollectedReferenceRegister();
         }

      node->setRegister(globalReg);
      }
   return globalReg;
   }

// Also handles sRegLoad, bRegLoad, and lRegLoad
TR::Register *
OMR::ARM64::TreeEvaluator::iRegLoadEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Register *globalReg = node->getRegister();

   if (globalReg == NULL)
      {
      globalReg = cg->allocateRegister();
      node->setRegister(globalReg);
      }
   return(globalReg);
   }

// Also handles sRegStore, bRegStore, lRegStore, and aRegStore
TR::Register *
OMR::ARM64::TreeEvaluator::iRegStoreEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *globalReg = cg->evaluate(child);
   cg->decReferenceCount(child);
   return globalReg;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::GlRegDepsEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   int32_t i;

   for (i = 0; i < node->getNumChildren(); i++)
      {
      cg->evaluate(node->getChild(i));
      cg->decReferenceCount(node->getChild(i));
      }
   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::BBStartEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Block *block = node->getBlock();
   cg->setCurrentBlock(block);

   TR::RegisterDependencyConditions *deps = NULL;

   if (!block->isExtensionOfPreviousBlock() && node->getNumChildren()>0)
      {
      int32_t i;
      TR::Node *child = node->getFirstChild();
      cg->evaluate(child);
      deps = generateRegisterDependencyConditions(cg, child, 0);
      if (cg->getCurrentEvaluationTreeTop() == comp->getStartTree())
         {
         for (i=0; i<child->getNumChildren(); i++)
            {
            TR::ParameterSymbol *sym = child->getChild(i)->getSymbol()->getParmSymbol();
            if (sym != NULL)
               {
               sym->setAllocatedIndex(cg->getGlobalRegister(child->getChild(i)->getGlobalRegisterNumber()));
               }
            }
         }
      child->decReferenceCount();
      }

   if (node->getLabel() != NULL)
      {
      node->getLabel()->setInstruction(generateLabelInstruction(cg, TR::InstOpCode::label, node, node->getLabel(), deps));
      }

   TR::Node *fenceNode = TR::Node::createRelative32BitFenceNode(node, &block->getInstructionBoundaries()._startPC);
   TR::Instruction *fence = generateAdminInstruction(cg, TR::InstOpCode::fence, node, fenceNode);

   if (block->isCatchBlock())
      {
      cg->generateCatchBlockBBStartPrologue(node, fence);
      }

   return NULL;
   }

TR::Register *
OMR::ARM64::TreeEvaluator::BBEndEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Block *block = node->getBlock();
   TR::Compilation *comp = cg->comp();
   TR::Node *fenceNode = TR::Node::createRelative32BitFenceNode(node, &node->getBlock()->getInstructionBoundaries()._endPC);

   if (NULL == block->getNextBlock())
      {
      TR::Instruction *lastInstruction = cg->getAppendInstruction();
      if (lastInstruction->getOpCodeValue() == TR::InstOpCode::bl
              && lastInstruction->getNode()->getSymbolReference()->getReferenceNumber() == TR_aThrow)
         {
         lastInstruction = generateInstruction(cg, TR::InstOpCode::bad, node, lastInstruction);
         }
      }

   TR::TreeTop *nextTT = cg->getCurrentEvaluationTreeTop()->getNextTreeTop();

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
   generateAdminInstruction(cg, TR::InstOpCode::fence, node, deps, fenceNode);

   return NULL;
   }

// handles l2a, lu2a, a2l
TR::Register *
OMR::ARM64::TreeEvaluator::passThroughEvaluator(TR::Node *node, TR::CodeGenerator *cg)
   {
   TR::Node *child = node->getFirstChild();
   TR::Register *trgReg = cg->evaluate(child);
   child->decReferenceCount();
   node->setRegister(trgReg);
   return trgReg;
   }
