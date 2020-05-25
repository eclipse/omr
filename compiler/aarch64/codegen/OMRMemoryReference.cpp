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

#include "codegen/MemoryReference.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"


static void loadRelocatableConstant(TR::Node *node,
                                    TR::SymbolReference *ref,
                                    TR::Register *reg,
                                    TR::MemoryReference *mr,
                                    TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   TR::Symbol *symbol = ref->getSymbol();
   bool isStatic = symbol->isStatic();
   bool isStaticField = isStatic && (ref->getCPIndex() > 0) && !symbol->isClassObject();
   bool isClass = isStatic && symbol->isClassObject();
   bool isPicSite = isClass;

   if (isPicSite && !cg->comp()->compileRelocatableCode()
       && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)symbol->getStaticSymbol()->getStaticAddress(), node))
      {
      TR_UNIMPLEMENTED();
      return;
      }

   uintptr_t addr = symbol->isStatic() ? (uintptr_t)symbol->getStaticSymbol()->getStaticAddress() : (uintptr_t)symbol->getMethodSymbol()->getMethodAddress();

   if (symbol->isStartPC())
      {
      generateTrg1ImmSymInstruction(cg, TR::InstOpCode::adr, node, reg, addr, symbol);
      return;
      }

   if (ref->isUnresolved() || comp->compileRelocatableCode())
      {
      TR::Node *GCRnode = node;
      if (!GCRnode)
         GCRnode = cg->getCurrentEvaluationTreeTop()->getNode();

      if (symbol->isCountForRecompile())
         {
         loadAddressConstant(cg, GCRnode, TR_CountForRecompile, reg, NULL, false, TR_GlobalValue);
         }
      else if (symbol->isRecompilationCounter())
         {
         loadAddressConstant(cg, GCRnode, 1, reg, NULL, false, TR_BodyInfoAddressLoad);
         }
      else if (symbol->isCompiledMethod())
         {
         loadAddressConstant(cg, GCRnode, 1, reg, NULL, false, TR_RamMethodSequence);
         }
      else if (isStaticField && !ref->isUnresolved())
         {
         loadAddressConstant(cg, GCRnode, 1, reg, NULL, false, TR_DataAddress);
         }
      else if (isClass && !ref->isUnresolved())
         {
         loadAddressConstant(cg, GCRnode, (intptr_t)ref, reg, NULL, false, TR_ClassAddress);
         }
      else
         {
         mr->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, ref, node->getOpCode().isStore(), false));
         cg->addSnippet(mr->getUnresolvedSnippet());
         }
      }
   else
      {
      loadConstant64(cg, node, addr, reg);
      }
   }

OMR::ARM64::MemoryReference::MemoryReference(
      TR::CodeGenerator *cg) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _length(0),
   _scale(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _offset = _symbolReference->getOffset();
   }


OMR::ARM64::MemoryReference::MemoryReference(
      TR::Register *br,
      TR::Register *ir,
      TR::CodeGenerator *cg) :
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(ir),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _length(0),
   _scale(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _offset = _symbolReference->getOffset();
   }


OMR::ARM64::MemoryReference::MemoryReference(
      TR::Register *br,
      int32_t disp,
      TR::CodeGenerator *cg) :
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _length(0),
   _scale(0),
   _offset(disp)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   }


OMR::ARM64::MemoryReference::MemoryReference(
      TR::Node *rootLoadOrStore,
      uint32_t len,
      TR::CodeGenerator *cg) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _length(len),
   _scale(0),
   _offset(0),
   _symbolReference(rootLoadOrStore->getSymbolReference())
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *ref = rootLoadOrStore->getSymbolReference();
   TR::Symbol *symbol = ref->getSymbol();
   bool isStore = rootLoadOrStore->getOpCode().isStore();

   self()->setSymbol(symbol, cg);

   if (rootLoadOrStore->getOpCode().isIndirect())
      {
      if (ref->isUnresolved())
         {
         self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, rootLoadOrStore, rootLoadOrStore->getSymbolReference(), isStore, false));
         cg->addSnippet(self()->getUnresolvedSnippet());
         }
      self()->populateMemoryReference(rootLoadOrStore->getFirstChild(), cg);
      }
   else
      {
      if (symbol->isStatic())
         {
         if (ref->isUnresolved())
            {
            self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, rootLoadOrStore, rootLoadOrStore->getSymbolReference(), isStore, false));
            cg->addSnippet(self()->getUnresolvedSnippet());
            }
         else
            {
            _baseRegister = cg->allocateRegister();
            self()->setBaseModifiable();
            loadRelocatableConstant(rootLoadOrStore, ref, _baseRegister, self(), cg);
            }
         }
      else
         {
         if (!symbol->isMethodMetaData())
            { // must be either auto or parm or error.
            _baseRegister = cg->getStackPointerRegister();
            }
         else
            {
            _baseRegister = cg->getMethodMetaDataRegister();
            }
         }
      }
   self()->addToOffset(rootLoadOrStore, ref->getOffset(), cg);
   }


OMR::ARM64::MemoryReference::MemoryReference(
      TR::Node *node,
      TR::SymbolReference *symRef,
      uint32_t len,
      TR::CodeGenerator *cg) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _length(len),
   _scale(0),
   _offset(0),
   _symbolReference(symRef)
   {
   TR::Symbol *symbol = symRef->getSymbol();

   if (symbol->isStatic())
      {
      if (symRef->isUnresolved())
         {
         self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, symRef, false, false));
         cg->addSnippet(self()->getUnresolvedSnippet());
         }
      else
         {
         _baseRegister = cg->allocateRegister();
         self()->setBaseModifiable();
         loadRelocatableConstant(node, symRef, _baseRegister, self(), cg);
         }
      }

   if (symbol->isRegisterMappedSymbol())
      {
      if (!symbol->isMethodMetaData())
         { // must be either auto or parm or error.
         _baseRegister = cg->getStackPointerRegister();
         }
      else
         {
         _baseRegister = cg->getMethodMetaDataRegister();
         }
      }

   self()->setSymbol(symbol, cg);
   self()->addToOffset(0, symRef->getOffset(), cg);
   }


void OMR::ARM64::MemoryReference::setSymbol(TR::Symbol *symbol, TR::CodeGenerator *cg)
   {
   TR_ASSERT(_symbolReference->getSymbol()==NULL || _symbolReference->getSymbol()==symbol, "should not write to existing symbolReference");
   _symbolReference->setSymbol(symbol);

   if (_baseRegister != NULL && _indexRegister != NULL &&
       (self()->hasDelayedOffset() || self()->getOffset(true) != 0))
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   }


void OMR::ARM64::MemoryReference::addToOffset(TR::Node *node, intptr_t amount, TR::CodeGenerator *cg)
   {
   if (self()->getUnresolvedSnippet() != NULL)
      {
      self()->setOffset(self()->getOffset() + amount);
      return;
      }

   if (amount == 0)
      {
      return;
      }

   if (_baseRegister != NULL && _indexRegister != NULL)
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }

   intptr_t displacement = self()->getOffset() + amount;
   if (!constantIsImm9(displacement))
      {
      TR::Register *newBase;

      self()->setOffset(0);

      if (_baseRegister && self()->isBaseModifiable())
         newBase = _baseRegister;
      else
         {
         newBase = cg->allocateRegister();

         if (_baseRegister && _baseRegister->containsInternalPointer())
            {
            newBase->setContainsInternalPointer();
            newBase->setPinningArrayPointer(_baseRegister->getPinningArrayPointer());
            }
         }

      if (!node)
         node = cg->getAppendInstruction()->getNode();

      if (_baseRegister != NULL)
         {
         if (constantIsUnsignedImm12(displacement))
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmx, node, newBase, _baseRegister, displacement);
            }
         else if (node->getOpCode().isLoadConst() && node->getRegister() && (node->getLongInt() == displacement))
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, newBase, _baseRegister, node->getRegister());
            }
         else
            {
            TR::Register *tempReg = cg->allocateRegister();
            loadConstant64(cg, node, displacement, tempReg);
            generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, node, newBase, _baseRegister, tempReg);
            cg->stopUsingRegister(tempReg);
            }
         }
      else
         {
         loadConstant64(cg, node, displacement, newBase);
         }

      // the following "if" is just to avoid stopUsingRegister being called
      // for newBase by decNodeReferenceCounts(cg);
      if (_baseRegister == newBase && _baseNode == NULL) _baseRegister = NULL;

      self()->decNodeReferenceCounts(cg);
      _baseRegister = newBase;
      _baseNode = NULL;
      self()->setBaseModifiable();
      }
   else
      self()->setOffset(displacement);
   }


void OMR::ARM64::MemoryReference::decNodeReferenceCounts(TR::CodeGenerator *cg)
   {
   if (_baseRegister != NULL)
      {
      if (_baseNode != NULL)
         cg->decReferenceCount(_baseNode);
      else
         cg->stopUsingRegister(_baseRegister);
      }

   if (_indexRegister != NULL)
      {
      if (_indexNode != NULL)
         cg->decReferenceCount(_indexNode);
      else
         cg->stopUsingRegister(_indexRegister);
      }

   if (_extraRegister != NULL)
      {
      cg->stopUsingRegister(_extraRegister);
      }
   }


void OMR::ARM64::MemoryReference::populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg)
   {
   if (cg->comp()->useCompressedPointers())
      {
      if (subTree->getOpCodeValue() == TR::l2a && subTree->getReferenceCount() == 1 && subTree->getRegister() == NULL)
         {
         cg->decReferenceCount(subTree);
         subTree = subTree->getFirstChild();
         }
      }

   if (subTree->getReferenceCount() > 1 || subTree->getRegister() != NULL)
      {
      if (_baseRegister != NULL)
         {
         self()->consolidateRegisters(cg->evaluate(subTree), subTree, false, cg);
         }
      else
         {
         _baseRegister = cg->evaluate(subTree);
         _baseNode = subTree;
         }
      }
   else
      {
      if (subTree->getOpCodeValue() == TR::aiadd ||
          subTree->getOpCodeValue() == TR::iadd)
         {
         TR::Node *addressChild = subTree->getFirstChild();
         TR::Node *integerChild = subTree->getSecondChild();

         if (integerChild->getOpCode().isLoadConst())
            {
            self()->populateMemoryReference(addressChild, cg);
            intptr_t amount = (integerChild->getOpCodeValue() == TR::iconst) ?
                                integerChild->getInt() : integerChild->getLongInt();
            self()->addToOffset(integerChild, amount, cg);
            integerChild->decReferenceCount();
            }
         else if (integerChild->getEvaluationPriority(cg) > addressChild->getEvaluationPriority(cg))
            {
            self()->populateMemoryReference(integerChild, cg);
            self()->populateMemoryReference(addressChild, cg);
            }
         else
            {
            self()->populateMemoryReference(addressChild, cg);
            self()->populateMemoryReference(integerChild, cg);
            }
         }
      else if (subTree->getOpCodeValue() == TR::ishl &&
               subTree->getFirstChild()->getOpCode().isLoadConst() &&
               subTree->getSecondChild()->getOpCode().isLoadConst())
         {
         self()->addToOffset(subTree, subTree->getFirstChild()->getInt() <<
                     subTree->getSecondChild()->getInt(), cg);
         subTree->getFirstChild()->decReferenceCount();
         subTree->getSecondChild()->decReferenceCount();
         }
      else if ((subTree->getOpCodeValue() == TR::loadaddr) && !cg->comp()->compileRelocatableCode())
         {
         TR::SymbolReference *ref = subTree->getSymbolReference();
         TR::Symbol *symbol = ref->getSymbol();
         _symbolReference = ref;

         if (symbol->isStatic())
            {
            if (ref->isUnresolved())
               {
               self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, subTree, ref, subTree->getOpCode().isStore(), false));
               cg->addSnippet(self()->getUnresolvedSnippet());
               }
            else
               {
               _baseRegister = cg->allocateRegister();
               _baseNode = NULL;
               self()->setBaseModifiable();
               loadRelocatableConstant(subTree, ref, _baseRegister, self(), cg);
               }
            }
         if (symbol->isRegisterMappedSymbol())
            {
            if (_baseRegister != NULL)
               {
               TR::Register *tempReg;
               if (!symbol->isMethodMetaData())
                  { // must be either auto or parm or error.
                  tempReg = cg->getStackPointerRegister();
                  }
               else
                  {
                  tempReg = cg->getMethodMetaDataRegister();
                  }
               self()->consolidateRegisters(tempReg, NULL, false, cg);
               }
            else
               {
               if (!symbol->isMethodMetaData())
                  { // must be either auto or parm or error.
                  _baseRegister = cg->getStackPointerRegister();
                  }
               else
                  {
                  _baseRegister = cg->getMethodMetaDataRegister();
                  }
               _baseNode = NULL;
               }
            }
         self()->addToOffset(subTree, subTree->getSymbolReference()->getOffset(), cg);
         subTree->decReferenceCount(); // need to decrement ref count because
                                       // nodes weren't set on memoryreference
         }
      else if (subTree->getOpCodeValue() == TR::aconst ||
               subTree->getOpCodeValue() == TR::iconst ||
               subTree->getOpCodeValue() == TR::lconst)
         {
         intptr_t amount = (subTree->getOpCodeValue() == TR::iconst) ?
                             subTree->getInt() : subTree->getLongInt();
         if (_baseRegister != NULL)
            {
            self()->addToOffset(subTree, amount, cg);
            }
         else
            {
            _baseRegister = cg->allocateRegister();
            _baseNode = subTree;
            self()->setBaseModifiable();
            loadConstant64(cg, subTree, amount, _baseRegister);
            }
         }
      else
         {
         if (_baseRegister != NULL)
            {
            self()->consolidateRegisters(cg->evaluate(subTree), subTree, true, cg);
            }
         else
            {
            _baseRegister = cg->evaluate(subTree);
            _baseNode = subTree;
            self()->setBaseModifiable();
            }
         }
      }
   }


void OMR::ARM64::MemoryReference::consolidateRegisters(TR::Register *srcReg, TR::Node *srcTree, bool srcModifiable, TR::CodeGenerator *cg)
   {
   TR::Register *tempTargetRegister;

   if (self()->getUnresolvedSnippet() != NULL)
      {
      TR_UNIMPLEMENTED();
      }
   else
      {
      if (_indexRegister != NULL)
         {
         if (self()->isBaseModifiable())
            tempTargetRegister = _baseRegister;
         else if (_baseRegister->containsCollectedReference() ||
                  _indexRegister->containsCollectedReference())
            tempTargetRegister = cg->allocateCollectedReferenceRegister();
         else
            tempTargetRegister = cg->allocateRegister();

         generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, srcTree, tempTargetRegister, _baseRegister, _indexRegister);

         if (_baseRegister != tempTargetRegister)
            {
            self()->decNodeReferenceCounts(cg);
            _baseNode = NULL;
            }
         else
            {
            if (_indexNode != NULL)
               cg->decReferenceCount(_indexNode);
            else
               cg->stopUsingRegister(_indexRegister);
            }
         _baseRegister = tempTargetRegister;
         self()->setBaseModifiable();
         }
      else if (srcReg != NULL && (self()->getOffset(true) != 0 || self()->hasDelayedOffset()))
         {
         if (self()->isBaseModifiable())
            tempTargetRegister = _baseRegister;
         else if (srcModifiable)
            tempTargetRegister = srcReg;
         else if (srcReg->containsCollectedReference() ||
                  _baseRegister->containsCollectedReference())
            tempTargetRegister = cg->allocateCollectedReferenceRegister();
         else
            tempTargetRegister = cg->allocateRegister();

         generateTrg1Src2Instruction(cg, TR::InstOpCode::addx, srcTree, tempTargetRegister, _baseRegister, srcReg);

         if (_baseRegister != tempTargetRegister)
            {
            self()->decNodeReferenceCounts(cg);
            _baseNode = NULL;
            }
         if (srcReg == tempTargetRegister)
            {
            self()->decNodeReferenceCounts(cg);
            _baseNode = srcTree;
            }
         else
            {
            if (srcTree != NULL)
               cg->decReferenceCount(srcTree);
            else
               cg->stopUsingRegister(srcReg);
            }
         _baseRegister = tempTargetRegister;
         self()->setBaseModifiable();
         srcReg = NULL;
         srcTree = NULL;
         srcModifiable=false;
         }
      _indexRegister = srcReg;
      _indexNode = srcTree;
      if (srcModifiable)
         self()->setIndexModifiable();
      else
         self()->clearIndexModifiable();
      }
   }


void OMR::ARM64::MemoryReference::incRegisterTotalUseCounts(TR::CodeGenerator * cg)
   {
   if (_baseRegister != NULL)
      {
      _baseRegister->incTotalUseCount();
      }
   if (_indexRegister != NULL)
      {
      _indexRegister->incTotalUseCount();
      }
   if (_extraRegister != NULL)
      {
      _extraRegister->incTotalUseCount();
      }
   }


void OMR::ARM64::MemoryReference::assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   TR::Machine *machine = cg->machine();
   TR::RealRegister *assignedBaseRegister;
   TR::RealRegister *assignedIndexRegister;

   if (_baseRegister != NULL)
      {
      assignedBaseRegister = _baseRegister->getAssignedRealRegister();
      if (_indexRegister != NULL)
         {
         _indexRegister->block();
         }

      if (assignedBaseRegister == NULL)
         {
         if (_baseRegister->getTotalUseCount() == _baseRegister->getFutureUseCount())
            {
            if ((assignedBaseRegister = machine->findBestFreeRegister(TR_GPR, true)) == NULL)
               {
               assignedBaseRegister = machine->freeBestRegister(currentInstruction, _baseRegister);
               }
            }
         else
            {
            assignedBaseRegister = machine->reverseSpillState(currentInstruction, _baseRegister);
            }
         _baseRegister->setAssignedRegister(assignedBaseRegister);
         assignedBaseRegister->setAssignedRegister(_baseRegister);
         assignedBaseRegister->setState(TR::RealRegister::Assigned);
         }

      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }
      }

   if (_indexRegister != NULL)
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->block();
         }

      assignedIndexRegister = _indexRegister->getAssignedRealRegister();
      if (assignedIndexRegister == NULL)
         {
         if (_indexRegister->getTotalUseCount() == _indexRegister->getFutureUseCount())
            {
            if ((assignedIndexRegister = machine->findBestFreeRegister(TR_GPR, false)) == NULL)
               {
               assignedIndexRegister = machine->freeBestRegister(currentInstruction, _indexRegister);
               }
            }
         else
            {
            assignedIndexRegister = machine->reverseSpillState(currentInstruction, _indexRegister);
            }
         _indexRegister->setAssignedRegister(assignedIndexRegister);
         assignedIndexRegister->setAssignedRegister(_indexRegister);
         assignedIndexRegister->setState(TR::RealRegister::Assigned);
         }

      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      }

   if (_baseRegister != NULL)
      {
      if (_baseRegister->decFutureUseCount() == 0)
         {
         _baseRegister->setAssignedRegister(NULL);
         assignedBaseRegister->setState(TR::RealRegister::Unlatched);
         }
      _baseRegister = assignedBaseRegister;
      }

   if (_indexRegister != NULL)
      {
      if (_indexRegister->decFutureUseCount() == 0)
         {
         _indexRegister->setAssignedRegister(NULL);
         assignedIndexRegister->setState(TR::RealRegister::Unlatched);
         }
      _indexRegister = assignedIndexRegister;
      }
   if (self()->getUnresolvedSnippet() != NULL)
      {
      currentInstruction->ARM64NeedsGCMap(cg, 0xFFFFFFFF);
      }
   }


/* register offset */
static bool isRegisterOffsetInstruction(uint32_t enc)
   {
   return ((enc & 0x3b200c00) == 0x38200800);
   }


/* post-index/pre-index/unscaled immediate offset */
static bool isImm9OffsetInstruction(uint32_t enc)
   {
   return ((enc & 0x3b200000) == 0x38000000);
   }


/* unsigned immediate offset */
static bool isImm12OffsetInstruction(uint32_t enc)
   {
   return ((enc & 0x3b200000) == 0x39000000);
   }

/* stp/ldp GPR */
static bool isImm7OffsetGPRInstruction(uint32_t enc)
   {
   return ((enc & 0x3e000000) == 0x28000000);
   }

/* load/store exclusive */
static bool isExclusiveMemAccessInstruction(TR::InstOpCode::Mnemonic op)
   {
   return (op == TR::InstOpCode::ldxrx || op == TR::InstOpCode::ldxrw ||
           op == TR::InstOpCode::stxrx || op == TR::InstOpCode::stxrw);
   }


uint8_t *OMR::ARM64::MemoryReference::generateBinaryEncoding(TR::Instruction *currentInstruction, uint8_t *cursor, TR::CodeGenerator *cg)
   {
   uint32_t *wcursor = (uint32_t *)cursor;
   TR::RealRegister *base = self()->getBaseRegister() ? toRealRegister(self()->getBaseRegister()) : NULL;
   TR::RealRegister *index = self()->getIndexRegister() ? toRealRegister(self()->getIndexRegister()) : NULL;

   if (self()->getUnresolvedSnippet())
      {
      TR_UNIMPLEMENTED();
      }
   else
      {
      int32_t displacement = self()->getOffset(true);

      TR::InstOpCode op = currentInstruction->getOpCode();

      if (op.getMnemonic() != TR::InstOpCode::addimmx)
         {
         // load/store instruction
         uint32_t enc = (uint32_t)op.getOpCodeBinaryEncoding();

         if (index)
            {
            TR_ASSERT(displacement == 0, "Non-zero offset with index register.");

            if (isRegisterOffsetInstruction(enc))
               {
               base->setRegisterFieldRN(wcursor);
               index->setRegisterFieldRM(wcursor);

               if (self()->getScale() == 0)
                  {
                  // default: LSL #0
                  *wcursor |= 0x6 << 12;
                  }
               else
                  {
                  // Eclipse OMR Issue #4227 tracks this
                  TR_UNIMPLEMENTED();
                  }

               cursor += ARM64_INSTRUCTION_LENGTH;
               }
            else
               {
               TR_ASSERT_FATAL(false, "Unsupported instruction type.");
               }
            }
         else
            {
            /* no index register */
            base->setRegisterFieldRN(wcursor);

            if (isImm9OffsetInstruction(enc))
               {
               if (constantIsImm9(displacement))
                  {
                  *wcursor |= (displacement & 0x1ff) << 12; /* imm9 */
                  cursor += ARM64_INSTRUCTION_LENGTH;
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isImm12OffsetInstruction(enc))
               {
               uint32_t size = (enc >> 30) & 3; /* b=0, h=1, w=2, x=3 */
               uint32_t shifted = displacement >> size;

               if (size > 0)
                  {
                  TR_ASSERT((displacement & ((1 << size) - 1)) == 0, "Non-aligned offset in 2/4/8-byte memory access.");
                  }

               if (constantIsUnsignedImm12(shifted))
                  {
                  *wcursor |= (shifted & 0xfff) << 10; /* imm12 */
                  cursor += ARM64_INSTRUCTION_LENGTH;
                  }
               else
                  {
                  if (op.getMnemonic() == TR::InstOpCode::ldrimmw && displacement < 0 && constantIsImm9(displacement))
                     {
                     *wcursor &= 0xFEFFFFFF; /* rewrite the instruction ldrimmw -> ldurw */
                     *wcursor |= (displacement & 0x1ff) << 12; /* imm9 */
                     cursor += ARM64_INSTRUCTION_LENGTH;
                     }
                  else
                     {
                     TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                     }
                  }
               }
            else if (isImm7OffsetGPRInstruction(enc))
               {
               uint32_t opc = ((enc >> 30) & 3); /* 32bit: 00, 64bit: 10 */
               uint32_t size = ((opc >> 1) + 2);
               uint32_t shifted = displacement >> size;

               TR_ASSERT((displacement & ((1 << size) - 1)) == 0, "displacement must be 4/8-byte alligned");

               if (constantIsImm7(shifted))
                  {
                  *wcursor |= (shifted & 0x7f) << 15; /* imm7 */
                  cursor += ARM64_INSTRUCTION_LENGTH;
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isExclusiveMemAccessInstruction(op.getMnemonic()))
               {
               TR_ASSERT(displacement == 0, "Offset must be zero for specified instruction.");
               cursor += ARM64_INSTRUCTION_LENGTH;
               }
            else
               {
               /* Register pair, literal instructions to be supported */
               TR_UNIMPLEMENTED();
               }
            }
         }
      else
         {
         // loadaddrEvaluator() uses addimmx in generateTrg1MemInstruction
         TR_ASSERT(index == NULL, "MemoryReference with unexpected indexed form");

         if (constantIsUnsignedImm12(displacement))
            {
            *wcursor |= (displacement & 0xfff) << 10; /* imm12 */
            base->setRegisterFieldRN(wcursor);
            cursor += ARM64_INSTRUCTION_LENGTH;
            }
         else
            {
            TR_ASSERT(currentInstruction->getKind() == OMR::Instruction::IsTrg1Mem, "unexpected instruction kind");
            TR::RealRegister *treg = toRealRegister(((TR::ARM64Trg1MemInstruction *)currentInstruction)->getTargetRegister());
            uint32_t lower = displacement & 0xffff;
            uint32_t upper = (displacement >> 16) & 0xffff;
            bool needSpill = (treg->getRegisterNumber() == base->getRegisterNumber());
            TR::RealRegister *immreg;
            TR::RealRegister *stackPtr;

            if (needSpill)
               {
               immreg = cg->machine()->getRealRegister(
                           (base->getRegisterNumber() == TR::RealRegister::x12) ? TR::RealRegister::x11 : TR::RealRegister::x12
                        );
               stackPtr = cg->getStackPointerRegister();
               // stur immreg, [sp, -8]
               *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::sturx) | (0x1F8 << 12);
               immreg->setRegisterFieldRT(wcursor);
               stackPtr->setRegisterFieldRN(wcursor);
               wcursor++;
               cursor += ARM64_INSTRUCTION_LENGTH;
               }
            else
               {
               immreg = treg;
               }

            // movzw immreg, low16bit
            // movkw immreg, high16bit, LSL #16
            // addx treg, basereg, immreg, SXTW
            *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movzw) | (lower << 5);
            immreg->setRegisterFieldRD(wcursor);
            wcursor++;
            *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::movkw) | ((upper | TR::MOV_LSL16) << 5);
            immreg->setRegisterFieldRD(wcursor);
            wcursor++;
            *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::addextx) | (TR::EXT_SXTW << 13);
            base->setRegisterFieldRN(wcursor);
            immreg->setRegisterFieldRM(wcursor);
            treg->setRegisterFieldRD(wcursor);
            wcursor++;
            cursor += ARM64_INSTRUCTION_LENGTH * 3;

            if (needSpill)
               {
               // ldur immreg, [sp, -8]
               *wcursor = TR::InstOpCode::getOpCodeBinaryEncoding(TR::InstOpCode::ldurx) | (0x1F8 << 12);
               immreg->setRegisterFieldRT(wcursor);
               stackPtr->setRegisterFieldRN(wcursor);
               wcursor++;
               cursor += ARM64_INSTRUCTION_LENGTH;
               }
            }
         }
      }

   return cursor;
   }


uint32_t OMR::ARM64::MemoryReference::estimateBinaryLength(TR::InstOpCode op)
   {
   if (self()->getUnresolvedSnippet() != NULL)
      {
      TR_UNIMPLEMENTED();
      }
   else
      {
      if (op.getMnemonic() != TR::InstOpCode::addimmx)
         {
         // load/store instruction
         if (self()->getIndexRegister())
            {
            return ARM64_INSTRUCTION_LENGTH;
            }
         else
            {
            /* no index register */
            int32_t displacement = self()->getOffset(true);
            uint32_t enc = (uint32_t)op.getOpCodeBinaryEncoding();

            if (isImm9OffsetInstruction(enc))
               {
               if (constantIsImm9(displacement))
                  {
                  return ARM64_INSTRUCTION_LENGTH;
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isImm12OffsetInstruction(enc))
               {
               uint32_t size = (enc >> 30) & 3; /* b=0, h=1, w=2, x=3 */
               uint32_t shifted = displacement >> size;

               if (size > 0)
                  {
                  TR_ASSERT((displacement & ((1 << size) - 1)) == 0, "Non-aligned offset in 2/4/8-byte memory access.");
                  }

               if (constantIsUnsignedImm12(shifted))
                  {
                  return ARM64_INSTRUCTION_LENGTH;
                  }
               else
                  {
                  if (op.getMnemonic() == TR::InstOpCode::ldrimmw && displacement < 0 && constantIsImm9(displacement))
                     {
                     /* rewrite the instruction ldrimmw -> ldurw in generateBinaryEncoding() */
                     return ARM64_INSTRUCTION_LENGTH;
                     }
                  else
                     {
                     TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                     }
                  }
               }
            else if (isImm7OffsetGPRInstruction(enc))
               {
               uint32_t opc = ((enc >> 30) & 3); /* 32bit: 00, 64bit: 10 */
               uint32_t size = ((opc >> 1) + 2);
               uint32_t shifted = displacement >> size;

               TR_ASSERT((displacement & ((1 << size) - 1)) == 0, "displacement must be 4/8-byte alligned");

               if (constantIsImm7(shifted))
                  {
                  return ARM64_INSTRUCTION_LENGTH;
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isExclusiveMemAccessInstruction(op.getMnemonic()))
               {
               return ARM64_INSTRUCTION_LENGTH;
               }
            else
               {
               /* Register pair, literal instructions to be supported */
               TR_UNIMPLEMENTED();
               }
            }
         }
      else
         {
         // addimmx instruction
         TR_ASSERT(self()->getIndexRegister() == NULL, "MemoryReference with unexpected indexed form");

         int32_t displacement = self()->getOffset(true);
         if (constantIsUnsignedImm12(displacement))
            {
            return ARM64_INSTRUCTION_LENGTH;
            }
         else
            {
            return ARM64_INSTRUCTION_LENGTH*5;
            }
         }
      }

   return 0;
   }
