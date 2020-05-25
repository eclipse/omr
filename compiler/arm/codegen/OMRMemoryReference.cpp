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
#include "codegen/AheadOfTimeCompile.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "codegen/ARMAOTRelocation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#endif
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Linkage_inlines.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "env/jittypes.h"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

static void loadRelocatableConstant(TR::Node               *node,
                                    TR::SymbolReference    *ref,
                                    TR::Register           *reg,
                                    TR::MemoryReference *mr,
                                    TR::CodeGenerator      *cg);

static uint8_t *armLoadConstantInstructions(TR::RealRegister *trgReg,
                                            int32_t            value,
                                            uint8_t            *cursor,
                                            uint32_t           *wcursor,
                                            TR::CodeGenerator   *cg);

static TR::RealRegister::RegNum choose_rX(TR::Instruction    *currentInstruction,
                                                         TR::RealRegister   *base);

OMR::ARM::MemoryReference::MemoryReference(TR::Register *br, TR::Register *ir, TR::CodeGenerator *cg):
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(ir),
   _indexNode(NULL),
   _modBase(NULL),
   _unresolvedSnippet(NULL),
   _symbolReference(cg->comp()->getSymRefTab()),
   _flag(0),
   _scale(0)
   {
   }

OMR::ARM::MemoryReference::MemoryReference(TR::Register *br, TR::Register *ir, uint8_t scale, TR::CodeGenerator *cg):
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(ir),
   _indexNode(NULL),
   _modBase(NULL),
   _unresolvedSnippet(NULL),
   _symbolReference(cg->comp()->getSymRefTab()),
   _flag(0),
   _scale(scale)
   {
   }

OMR::ARM::MemoryReference::MemoryReference(TR::Register *br, int32_t disp, TR::CodeGenerator *cg):
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _modBase(NULL),
   _unresolvedSnippet(NULL),
   _symbolReference(cg->comp()->getSymRefTab()),
   _flag(0),
   _scale(0)
   {
   _symbolReference.setOffset(disp);
   }

// constructor called to generate a memory reference for all kinds of
// stores and non-constant(or address) loads
//
OMR::ARM::MemoryReference::MemoryReference(TR::Node          *rootLoadOrStore,
                                             uint32_t          len,
                                             TR::CodeGenerator *cg)
   : _baseRegister(NULL),
     _baseNode(NULL),
     _indexRegister(NULL),
     _indexNode(NULL),
     _unresolvedSnippet(NULL),
     _symbolReference(cg->comp()->getSymRefTab()),
     _modBase(NULL),
     _length(len),
     _scale(0),
     _flag(0)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *ref    = rootLoadOrStore->getSymbolReference();
   TR::Symbol           *symbol = ref->getSymbol();
   bool               isStore = rootLoadOrStore->getOpCode().isStore();

   self()->setSymbol(symbol, cg);
   _symbolReference.setOwningMethodIndex(ref->getOwningMethodIndex());
   _symbolReference.setCPIndex(ref->getCPIndex());
   _symbolReference.copyFlags(ref);
   _symbolReference.copyRefNumIfPossible(ref, comp->getSymRefTab());

   if (rootLoadOrStore->getOpCode().isIndirect())
      {
      if (ref->isUnresolved())
         {
         self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, rootLoadOrStore, rootLoadOrStore->getSymbolReference(),isStore, false));
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
            self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, rootLoadOrStore, rootLoadOrStore->getSymbolReference(),isStore, false));
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
            _baseRegister = cg->getFrameRegister();
            }
         else
            {
            _baseRegister = cg->getMethodMetaDataRegister();
            }
         }
      }
   self()->addToOffset(rootLoadOrStore, rootLoadOrStore->getSymbolReference()->getOffset(), cg);
   if (self()->getUnresolvedSnippet() != NULL)
      self()->adjustForResolution(cg);
   // TODO: aliasing sets?
   }

OMR::ARM::MemoryReference::MemoryReference(TR::Node *node, TR::SymbolReference *symRef,
                                             uint32_t            len,
                                             TR::CodeGenerator   *cg)
   : _baseRegister(NULL),
     _baseNode(NULL),
     _indexRegister(NULL),
     _indexNode(NULL),
     _unresolvedSnippet(NULL),
     _symbolReference(cg->comp()->getSymRefTab()),
     _modBase(NULL),
     _length(len),
     _scale(0),
     _flag(0)
   {
   TR::Symbol *symbol = symRef->getSymbol();
   if (symbol->isStatic())
      {
      if (symRef->isUnresolved())
         {
         self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, symRef,false, false));
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
         _baseRegister = cg->getFrameRegister();
         }
      else
         {
         _baseRegister = cg->getMethodMetaDataRegister();
         }
      }

   self()->setSymbol(symbol, cg);
   _symbolReference.copyFlags(symRef);
   _symbolReference.copyRefNumIfPossible(symRef, cg->comp()->getSymRefTab());
   self()->addToOffset(0, symRef->getOffset(), cg);
   if (self()->getUnresolvedSnippet() != NULL)
      self()->adjustForResolution(cg);
   // TODO: aliasing sets?
   }

OMR::ARM::MemoryReference::MemoryReference(TR::MemoryReference &mr,
                                             int32_t                displacement,
                                             uint32_t               len,
                                             TR::CodeGenerator      *cg)
   : _symbolReference(cg->comp()->getSymRefTab())
   {
   _baseRegister    = mr._baseRegister;
   _baseNode        = NULL;
   _indexRegister   = mr._indexRegister;
   _indexNode       = NULL;
   _modBase         = mr._modBase;
   _symbolReference = TR::SymbolReference(mr._symbolReference);
   _length          = len;
   _scale           = 0;
   _flag            = 0;
   mr._flag         = 0;
   if (mr.getUnresolvedSnippet() != NULL)
      {
#ifdef J9_PROJECT_SPECIFIC
      _unresolvedSnippet = new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, 0, &_symbolReference, mr.getUnresolvedSnippet()->resolveForStore(), false);
      cg->addSnippet(_unresolvedSnippet);
      _modBase = cg->allocateRegister();
#endif
      }
   else
      {
      _unresolvedSnippet = NULL;
      }
   self()->addToOffset(0, displacement, cg);
   }

void OMR::ARM::MemoryReference::setSymbol(TR::Symbol *symbol, TR::CodeGenerator *cg)
   {
   _symbolReference.setSymbol(symbol);
   if (_baseRegister!=NULL && _indexRegister!=NULL &&
       (self()->hasDelayedOffset() || self()->getOffset()!=0))
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   }

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
void OMR::ARM::MemoryReference::fixupVFPOffset(TR::Node *node, TR::CodeGenerator *cg)
   {
      if (self()->getUnresolvedSnippet() != NULL)
         {
         return;
         }
      int32_t displacement = _symbolReference.getOffset();
      if (!constantIsImmed10(displacement))
         {
          TR::Register *newBase;
          int32_t      foldedOffset    = displacement & 0xFFFFFC00;
          int32_t      remainingOffset = displacement - foldedOffset;   // must be within -1024 to 1023

          _symbolReference.setOffset(remainingOffset);

          if (_baseRegister != NULL && self()->isBaseModifiable())
             newBase = _baseRegister;
          else
             newBase = cg->allocateCollectedReferenceRegister();

          // For all purposes, we catch the most likely case here.
          uint32_t immValue, rotate;
          if (constantIsImmed8r(foldedOffset, &immValue, &rotate))
             {
             if (_baseRegister == NULL)
                generateTrg1ImmInstruction(cg, ARMOp_mov, node, newBase, immValue, rotate);
             else
                generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, newBase, _baseRegister, immValue, rotate);
             }
          else
             {
             bool isFirstDone = false;
             for (int i = 0; i < 3; i++)
                {
                int32_t shift = 8 * (3 - i);
                int32_t base  = (foldedOffset & (0x000000FF << shift)) >> shift;
                if (base != 0)
                   {
                   if (!isFirstDone)
                      {
                      if (_baseRegister == NULL)
                         generateTrg1ImmInstruction(cg, ARMOp_mov, node, newBase, base, shift);
                      else
                         generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, newBase, _baseRegister, base, shift);
                      isFirstDone = true;
                      }
                   else
                      generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, newBase, newBase, base, shift);
                   }
                }
             }
          self()->decNodeReferenceCounts();
          _baseRegister = newBase;
          _baseNode = NULL;
          self()->setBaseModifiable();
         }
   }
#endif

void OMR::ARM::MemoryReference::addToOffset(TR::Node *node, int32_t amount, TR::CodeGenerator *cg)
   {
   if (self()->getUnresolvedSnippet() != NULL)
      {
      _symbolReference.setOffset(_symbolReference.getOffset() + amount);
      return;
      }

   if (amount == 0)
      return;

   if (_baseRegister != NULL && _indexRegister != NULL)
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }

   int32_t displacement = _symbolReference.getOffset() + amount;

   // TODO: the following unfairly punishes unsigned indirect byte load by
   // constants > 256 and < 4096 (they can use addr2 instead of addr3)
   // It is non-trivial to fix as the memory reference doesn't know
   // what instruction it is assocated with until later.
   if ((_length == 4 && !constantIsImmed12(displacement)) || (_length < 4 && (displacement < -128 || displacement > 127)))
      {
      TR::Register *newBase;
      int32_t      foldedOffset    = displacement & 0xFFFFFF00;
      int32_t      remainingOffset = displacement - foldedOffset;   // must be within -128 to 127

      _symbolReference.setOffset(remainingOffset);

      if (_baseRegister != NULL && self()->isBaseModifiable())
         newBase = _baseRegister;
      else
         newBase = cg->allocateCollectedReferenceRegister();

      // For all purposes, we catch the most likely case here.
      uint32_t immValue, rotate;
      if (constantIsImmed8r(foldedOffset, &immValue, &rotate))
         {
         if (_baseRegister == NULL)
            generateTrg1ImmInstruction(cg, ARMOp_mov, node, newBase, immValue, rotate);
         else
            generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, newBase, _baseRegister, immValue, rotate);
         }
      else
         {
         bool isFirstDone = false;
         for (int i = 0; i < 3; i++)
            {
            int32_t shift = 8 * (3 - i);
            int32_t base  = (foldedOffset & (0x000000FF << shift)) >> shift;
            if (base != 0)
               {
               if (!isFirstDone)
                  {
                  if (_baseRegister == NULL)
                     generateTrg1ImmInstruction(cg, ARMOp_mov, node, newBase, base, shift);
                  else
                     generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, newBase, _baseRegister, base, shift);
                  isFirstDone = true;
                  }
               else
                  generateTrg1Src1ImmInstruction(cg, ARMOp_add, node, newBase, newBase, base, shift);
               }
            }
         }
      self()->decNodeReferenceCounts();
      _baseRegister = newBase;
      _baseNode = NULL;
      self()->setBaseModifiable();
      }
   else
      _symbolReference.setOffset(displacement);
   }

void OMR::ARM::MemoryReference::adjustForResolution(TR::CodeGenerator *cg)
   {
   _modBase = cg->allocateRegister();
   }

void OMR::ARM::MemoryReference::decNodeReferenceCounts()
   {
   if (_baseRegister != NULL && _baseNode != NULL)
      {
      _baseNode->decReferenceCount();
      }
   if (_indexRegister != NULL && _indexNode != NULL)
      {
      _indexNode->decReferenceCount();
      }
   }

void OMR::ARM::MemoryReference::incRegisterTotalUseCounts(TR::CodeGenerator * cg)
   {
   if (_baseRegister != NULL)
      {
      _baseRegister->incTotalUseCount();
      }
   if (_indexRegister != NULL)
      {
      _indexRegister->incTotalUseCount();
      }
   if (_modBase != NULL)
      {
      _modBase->incTotalUseCount();
      }
   }

void OMR::ARM::MemoryReference::populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg)
   {
   if (subTree->getReferenceCount() > 1 || subTree->getRegister() != NULL)
      {
      if (_baseRegister != NULL)
         {
         self()->consolidateRegisters(cg->evaluate(subTree), subTree, false, cg);
         }
      else
         {
         _baseRegister = cg->evaluate(subTree);
         _baseNode     = subTree;
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
            self()->addToOffset(integerChild, integerChild->getInt(), cg);
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
         self()->setSymbol(symbol, cg);
         _symbolReference.copyFlags(ref);
         if (symbol->isStatic())
            {
            if (ref->isUnresolved())
               {
               self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, subTree, &_symbolReference, subTree->getOpCode().isStore(), false));
               cg->addSnippet(self()->getUnresolvedSnippet());
               }
            else
               {
               if (_baseRegister != NULL)
                  {
                  self()->addToOffset(subTree, (uintptr_t)symbol->castToStaticSymbol()->getStaticAddress(), cg);
                  }
               else
                  {
                  _baseRegister = cg->allocateRegister();
                  _baseNode = NULL;
                  self()->setBaseModifiable();
                  loadRelocatableConstant(subTree, ref, _baseRegister, self(), cg);
                  }
               }
            }
         if (symbol->isRegisterMappedSymbol())
            {
            if (_baseRegister != NULL)
               {
               TR::Register *tempReg;
               if (!symbol->isMethodMetaData())
                  { // must be either auto or parm or error.
                  tempReg = cg->getFrameRegister();
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
                  _baseRegister = cg->getFrameRegister();
                  }
               else
                  {
                  _baseRegister = cg->getMethodMetaDataRegister();
                  }
               _baseNode = NULL;
               }
            }
         self()->addToOffset(subTree, subTree->getSymbolReference()->getOffset(), cg);
         // TODO: aliasing sets?
         subTree->decReferenceCount(); // need to decrement ref count because
                                       // nodes weren't set on memoryreference
         }
      else if (subTree->getOpCodeValue() == TR::aconst ||
               subTree->getOpCodeValue() == TR::iconst)
         {
         if (_baseRegister != NULL)
            {
            self()->addToOffset(subTree, subTree->getInt(), cg);
            }
         else
            {
            _baseRegister = cg->allocateRegister();
            _baseNode = subTree;
            self()->setBaseModifiable();
            armLoadConstant(subTree, subTree->getInt(), _baseRegister, cg);
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
            _baseRegister  = cg->evaluate(subTree);
            _baseNode      = subTree;
            self()->setBaseModifiable();
            }
         }
      }
   }

void OMR::ARM::MemoryReference::consolidateRegisters(TR::Register *srcReg, TR::Node *srcTree, bool srcModifiable, TR::CodeGenerator *cg)
   {
   TR::Register *tempTargetRegister;

   if (self()->getUnresolvedSnippet() != NULL)
      {
      if (srcReg == NULL)
         return;
      if (_indexRegister != NULL)
         {
         if (self()->isIndexModifiable())
            tempTargetRegister = _indexRegister;
         else if (srcReg->containsCollectedReference() ||
                  _indexRegister->containsCollectedReference())
            tempTargetRegister = cg->allocateCollectedReferenceRegister();
         else
            tempTargetRegister = cg->allocateRegister();
         new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(ARMOp_add, srcTree, tempTargetRegister, _indexRegister, srcReg, cg);
         if (_indexRegister != tempTargetRegister)
            {
            if (_indexNode != NULL)
               cg->decReferenceCount(_indexNode);
            else
               cg->stopUsingRegister(_indexRegister);
            _indexNode = NULL;
            }
         if (srcTree != NULL)
            cg->decReferenceCount(srcTree);
         else
            cg->stopUsingRegister(srcReg);
         _indexRegister = tempTargetRegister;
         self()->setIndexModifiable();
         }
      else
         {
         _indexRegister = srcReg;
         _indexNode = srcTree;
         if (srcModifiable)
            self()->setIndexModifiable();
         else
            self()->clearIndexModifiable();
         }
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
         new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(ARMOp_add, srcTree, tempTargetRegister, _baseRegister, _indexRegister, cg);
         if (_baseRegister != tempTargetRegister)
            {
            self()->decNodeReferenceCounts();
            _baseNode = NULL;
            }
         else
            {
            if (_indexNode != NULL)
               cg->decReferenceCount(_indexNode);
            else
               cg->stopUsingRegister(_indexRegister);
            }
         _baseRegister  = tempTargetRegister;
         self()->setBaseModifiable();
         }
      else if (srcReg!=NULL && (self()->getOffset()!=0 || self()->hasDelayedOffset()))
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
         new (cg->trHeapMemory()) TR::ARMTrg1Src2Instruction(ARMOp_add, srcTree, tempTargetRegister, _baseRegister, srcReg, cg);
         if (_baseRegister != tempTargetRegister)
            {
            self()->decNodeReferenceCounts();
            _baseNode = NULL;
            }
         if (srcReg == tempTargetRegister)
            {
            self()->decNodeReferenceCounts();
            _baseNode = srcTree;
            }
         else
            {
            if (srcTree != NULL)
               cg->decReferenceCount(srcTree);
            else
               cg->stopUsingRegister(srcReg);
            }
         _baseRegister  = tempTargetRegister;
         self()->setBaseModifiable();
         srcReg=NULL;
         srcTree=NULL;
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

void OMR::ARM::MemoryReference::assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   TR::Machine   *machine = cg->machine();
   TR::RealRegister *assignedBaseRegister;
   TR::RealRegister *assignedIndexRegister;
   TR::RealRegister *assignedModBaseRegister;
   if (_baseRegister != NULL)
      {
      assignedBaseRegister = _baseRegister->getAssignedRealRegister();
      if (_indexRegister != NULL)
         {
         _indexRegister->block();
         }
      if (_modBase != NULL)
         {
         _modBase->block();
         }

      if (assignedBaseRegister == NULL)
         {
         if (_baseRegister->getTotalUseCount() == _baseRegister->getFutureUseCount())
            {
            if ((assignedBaseRegister = machine->findBestFreeRegister(TR_GPR, true, true)) == NULL)
               {
               assignedBaseRegister = machine->freeBestRegister(currentInstruction, TR_GPR, NULL, true);
               }
            }
         else
            {
            assignedBaseRegister = machine->reverseSpillState(currentInstruction, _baseRegister, NULL, true);
            }
         _baseRegister->setAssignedRegister(assignedBaseRegister);
         assignedBaseRegister->setAssignedRegister(_baseRegister);
         assignedBaseRegister->setState(TR::RealRegister::Assigned);
         }

      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }
      if (_modBase != NULL)
         {
         _modBase->unblock();
         }
      }

   if (_indexRegister != NULL)
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->block();
         }
      if (_modBase != NULL)
         {
         _modBase->block();
         }

      assignedIndexRegister = _indexRegister->getAssignedRealRegister();
      if (assignedIndexRegister == NULL)
         {
         if (_indexRegister->getTotalUseCount() == _indexRegister->getFutureUseCount())
            {
            if ((assignedIndexRegister = machine->findBestFreeRegister(TR_GPR, false, true)) == NULL)
               {
               assignedIndexRegister = machine->freeBestRegister(currentInstruction, TR_GPR);
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
      if (_modBase != NULL)
         {
         _modBase->unblock();
         }
      }

   if (_modBase != NULL)
      {
      if (_baseRegister != NULL)
         {
         _baseRegister->block();
         }
      if (_indexRegister != NULL)
         {
         _indexRegister->block();
         }

      assignedModBaseRegister = _modBase->getAssignedRealRegister();
      if (assignedModBaseRegister == NULL)
         {
         if (_modBase->getTotalUseCount() == _modBase->getFutureUseCount())
            {
            if ((assignedModBaseRegister = machine->findBestFreeRegister(TR_GPR, true, true)) == NULL)
               {
               assignedModBaseRegister = machine->freeBestRegister(currentInstruction, TR_GPR, NULL, true);
               }
            }
         else
            {
            assignedModBaseRegister = machine->reverseSpillState(currentInstruction, _modBase, NULL, true);
            }
         _modBase->setAssignedRegister(assignedModBaseRegister);
         assignedModBaseRegister->setAssignedRegister(_modBase);
         assignedModBaseRegister->setState(TR::RealRegister::Assigned);
         }

      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }

      //////////////////////////////////////
      // _modBase has to be freed outside.//
      //////////////////////////////////////
      _modBase = assignedModBaseRegister;
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
      currentInstruction->ARMNeedsGCMap(0xFFFFFFFF);
      }
   }

static uint32_t splitOffset(uint32_t offset)
   {
   TR_ASSERT(offset < 256, "offset too large for halfword addressing");
   return (offset & 0xF) | ((offset & 0xF0) << 4);
   }

uint8_t *OMR::ARM::MemoryReference::generateBinaryEncoding(TR::Instruction *currentInstruction,
                                                       uint8_t           *cursor,
                                                       TR::CodeGenerator  *cg)
   {
   uint32_t           *wcursor = (uint32_t *)cursor;
   TR_ARMOpCodes       op      = currentInstruction->getOpCodeValue();
   TR::RealRegister *base    = self()->getBaseRegister() ? toRealRegister(self()->getBaseRegister()) : NULL;
   TR::RealRegister *index   = self()->getIndexRegister() ? toRealRegister(self()->getIndexRegister()) : NULL;

   if (self()->getUnresolvedSnippet())
      {
#ifdef J9_PROJECT_SPECIFIC
      uint32_t preserve = *wcursor; // original instruction
      TR::RealRegister *mBase = toRealRegister(self()->getModBase());
      self()->getUnresolvedSnippet()->setAddressOfDataReference(cursor);
      self()->getUnresolvedSnippet()->setMemoryReference(self());
      cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative24BitRelocation(cursor, self()->getUnresolvedSnippet()->getSnippetLabel()));
      *wcursor = 0xEA000000; // insert a branch to the snippet
      wcursor++;
      cursor += ARM_INSTRUCTION_LENGTH;

      if (index)
         {
         TR_ASSERT(0, "do unresolved indexed snippets");
         }
      else
         {
         // insert instructions for computing the address of the resolved field;
         // the actual immediate operands are patched in by L_mergedDataResolve
         // in PicBuilder.s

         // add r_mbase, r_mbase, <bits 8-15>
         *wcursor = 0xE2800C00;
         mBase->setRegisterFieldRD(wcursor);
         mBase->setRegisterFieldRN(wcursor);
         wcursor++;
         cursor += ARM_INSTRUCTION_LENGTH;

         // add r_mbase, r_mbase, <bits 16-23>
         *wcursor = 0xE2800800;
         mBase->setRegisterFieldRD(wcursor);
         mBase->setRegisterFieldRN(wcursor);
         wcursor++;
         cursor += ARM_INSTRUCTION_LENGTH;

         // add r_mbase, r_mbase, <bits 24-31>
         *wcursor = 0xE2800400;
         mBase->setRegisterFieldRD(wcursor);
         mBase->setRegisterFieldRN(wcursor);
         wcursor++;
         cursor += ARM_INSTRUCTION_LENGTH;

         // finally, encode the original instruction
         *wcursor = preserve;
         if (op == ARMOp_ldr   || op == ARMOp_ldrb || op == ARMOp_str   || op == ARMOp_strb ||
             op == ARMOp_ldrsb || op == ARMOp_ldrh || op == ARMOp_ldrsh || op == ARMOp_strh)
            {
            if (base)
               {
               // if the load or store had a base, add it in as an index.
               base->setRegisterFieldRN(wcursor);
               mBase->setRegisterFieldRM(wcursor);
               *wcursor |= 5 << 23; // set the I and U bits
               }
            else
               {
               mBase->setRegisterFieldRN(wcursor);
               }
            }
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         else if (op == ARMOp_flds || op == ARMOp_fldd || op == ARMOp_fsts || op == ARMOp_fstd)
            {
            if (base)
               {
               // add mBase, base, mBase
               *wcursor = 0xE0800000;
               mBase->setRegisterFieldRD(wcursor);
               base->setRegisterFieldRN(wcursor);
               mBase->setRegisterFieldRM(wcursor);
               wcursor++;
               cursor += ARM_INSTRUCTION_LENGTH;

               // Now the load/store
               *wcursor = preserve;
               mBase->setRegisterFieldRN(wcursor);
               }
            else
               {
               mBase->setRegisterFieldRN(wcursor);
               }
            }
#endif
         else if (op == ARMOp_add)
            {
            if (base)
               {
               base->setRegisterFieldRN(wcursor);
               mBase->setRegisterFieldRM(wcursor);
               }
            else
               {
               mBase->setRegisterFieldRN(wcursor);
               *wcursor |= 1 << 25; // Change it to a addi of #0
               }
            }
         else
            {
            TR_ASSERT(op == ARMOp_mov, "unknown opcode in an unresolved memory reference");
            mBase->setRegisterFieldRM(wcursor);
            }
         wcursor++;
         cursor += ARM_INSTRUCTION_LENGTH;
         }
#endif
      }
   else
      {
      int32_t displacement;
      if (!index && op != ARMOp_add)               // common encoding for all true load/store instructions with immediate offsets
         {
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         if (!(op == ARMOp_flds || op == ARMOp_fldd || op == ARMOp_fsts || op == ARMOp_fstd))
            {
#endif
            base->setRegisterFieldRN(wcursor);
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
            }
#endif
         displacement = self()->getOffset();
         if(displacement > 0)
            *wcursor |= 1 << 23;                   // set U bit for +ve
         else
            displacement = -displacement;
         TR_ASSERT(!(self()->isImmediatePreIndexed() && self()->isPostIndexed()),"write back with post-index transfer redundant");
         if (self()->isImmediatePreIndexed())
            *wcursor |= 1 << 21;                   // set W bit for write-back
         if (self()->isPostIndexed())
            *wcursor &= (uint32_t)(~(1 << 24));    // unset P for post-index
         }

      if (op == ARMOp_ldr || op == ARMOp_str || op == ARMOp_ldrb || op == ARMOp_strb)
         {
         // addressing mode 2
         if (index)
            {
            if (0 != _scale)
               {
               if (4 == _scale)
                  {
                  *wcursor |= 2 << 7;
                  }
               else
                  {
                  TR_ASSERT(0, "implement other scales");
                  }
               }
            base->setRegisterFieldRN(wcursor);
            index->setRegisterFieldRM(wcursor);
            *wcursor |= 1 << 25;                   // set I bit for index register (yes, this bit has
                                                   // different bit positions as well as reverse semantics
                                                   // in modes 2 and 3!)
            *wcursor |= 1 << 23;                   // set U bit for +ve
            }
         else
            {
            if (constantIsImmed12(displacement))
               {
               *wcursor |= displacement < 0 ? -displacement : displacement;
               }
            else
               {
               cursor = self()->encodeLargeARMConstant(wcursor, cursor, currentInstruction, cg);
               wcursor = (uint32_t *)cursor;
               }
            }
         }
      else if (op == ARMOp_ldrsb || op == ARMOp_ldrh || op == ARMOp_ldrsh || op == ARMOp_strh) // addressing mode 3
         {
         if (index)
            {
            base->setRegisterFieldRN(wcursor);
            index->setRegisterFieldRM(wcursor);
            *wcursor |= 1 << 23;                   // set U bit for +ve
            }
         else
            {
            if (constantIsUnsignedImmed8(self()->getOffset()))
               {
               *wcursor |= 1 << 22;                // set I bit for immediate index
               *wcursor |= splitOffset(displacement);
               }
            else
               {
               cursor = self()->encodeLargeARMConstant(wcursor, cursor, currentInstruction, cg);
               wcursor = (uint32_t *)cursor;
               }
            }
         }
      else if (op == ARMOp_add)                     // op is ARMOp_add for delayed offsets (see loadAddrEvaluator)
         {
         base->setRegisterFieldRN(wcursor);
         uint32_t immBase, rotate;
         if (constantIsImmed8r(self()->getOffset(), &immBase, &rotate))
            {
            *wcursor |= 1 << 25;                            // set I bit for immediate index
            *wcursor |= ((32 - rotate) << 7) & 0x00000f00;  // mask to deal with rotate == 0 case.
            *wcursor |= immBase;
            }
         else
            {
            cursor = self()->encodeLargeARMConstant(wcursor, cursor, currentInstruction, cg);
            wcursor = (uint32_t *)cursor;
            }
         }
      else if (op == ARMOp_ldfd || op == ARMOp_stfd || op == ARMOp_ldfs || op == ARMOp_stfs)
         {
         *wcursor |= (displacement >> 2);          // offset for these coprocessor instructions is in words
         }
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
      else if (op == ARMOp_flds || op == ARMOp_fldd || op == ARMOp_fsts || op == ARMOp_fstd)
         {
         if (constantIsImmed10(displacement))
            {
            base->setRegisterFieldRN(wcursor);
            *wcursor |= (displacement >> 2);  // Offset in words
            }
         else
            {
            cursor = self()->encodeLargeARMConstant(wcursor, cursor, currentInstruction, cg);
            wcursor = (uint32_t *)cursor;
            }
         }
#endif
      else if (op == ARMOp_ldrex || op == ARMOp_strex)
         {
         TR_ASSERT(displacement == 0 && index == 0, "Load/store exclusive does not have displacement or index register");
         base->setRegisterFieldRN(wcursor);
         }
      else
         {
         TR_ASSERT(0,"Unknown opcode");
         }
      wcursor++;
      cursor += ARM_INSTRUCTION_LENGTH;
      }

   return cursor;
   }

uint32_t OMR::ARM::MemoryReference::estimateBinaryLength(TR_ARMOpCodes op)
   {
   if (self()->getUnresolvedSnippet() != NULL)
      {
      if (_indexRegister != NULL)
         {
         TR_ASSERT(0, "set bin length");
         return 3*ARM_INSTRUCTION_LENGTH;
         }
      else
         {
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
         if ((op == ARMOp_flds || op == ARMOp_fldd || op == ARMOp_fsts || op == ARMOp_fstd) &&
            _baseRegister != NULL)
            return 6 * ARM_INSTRUCTION_LENGTH;
         else
#endif
         return 5 * ARM_INSTRUCTION_LENGTH;
         }
      }

   if (_indexRegister != NULL)
      {
      return ARM_INSTRUCTION_LENGTH;
      }
   else if (op == ARMOp_ldr || op == ARMOp_str || op == ARMOp_ldrb || op == ARMOp_strb || op == ARMOp_ldrex || op == ARMOp_strex)
      {
      if (constantIsImmed12(self()->getOffset()))
         {
         return ARM_INSTRUCTION_LENGTH;
         }
      else
         {
         return 7*ARM_INSTRUCTION_LENGTH;
         }
      }
   else if ( op == ARMOp_ldrsb || op == ARMOp_ldrh || op == ARMOp_ldrsh || op == ARMOp_strh )
      {
      if(constantIsUnsignedImmed8(self()->getOffset()))
         {
         return ARM_INSTRUCTION_LENGTH;
         }
      else
         {
         return 7*ARM_INSTRUCTION_LENGTH;
         }
      }
   else if( op == ARMOp_add )
      {
      uint32_t base,rotate;
      if(constantIsImmed8r(self()->getOffset(),&base,&rotate))
         {
         return ARM_INSTRUCTION_LENGTH;
         }
      else
         {
         return 7*ARM_INSTRUCTION_LENGTH;
         }
      }
   else if (op == ARMOp_ldfd || op == ARMOp_stfd || op == ARMOp_ldfs || op == ARMOp_stfs)
      {
      return ARM_INSTRUCTION_LENGTH;
      }
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   else if (op == ARMOp_flds || op == ARMOp_fldd || op == ARMOp_fsts || op == ARMOp_fstd)
      {
         if (constantIsImmed10(self()->getOffset()))
         {
         return ARM_INSTRUCTION_LENGTH;
         }
      else
         {
         return 8*ARM_INSTRUCTION_LENGTH;
         }
      }
#endif
   else
      {
      TR_ASSERT(0,"Unknown Opcode");
      return ARM_INSTRUCTION_LENGTH;
      }
   }

uint8_t *OMR::ARM::MemoryReference::encodeLargeARMConstant(uint32_t *wcursor, uint8_t *cursor, TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   bool spillNeeded;
   TR::RealRegister   *stackPtr;
   TR::RealRegister   *rX;
   uint32_t             preserve = *wcursor; // original instruction
   TR_ARMOpCodes        op       = currentInstruction->getOpCodeValue();
   TR::RealRegister   *base    = self()->getBaseRegister() ? toRealRegister(self()->getBaseRegister()) : NULL;
   int32_t              offset   = self()->getOffset();
   bool vfpInstruction = false;

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
   vfpInstruction = (op == ARMOp_flds || op == ARMOp_fldd || op == ARMOp_fsts || op == ARMOp_fstd);
#endif

   TR_ASSERT((op == ARMOp_add || op == ARMOp_ldr || op == ARMOp_ldrb || op == ARMOp_str || op == ARMOp_strb ||
           op == ARMOp_ldrsb || op == ARMOp_ldrh || op == ARMOp_ldrsh || op == ARMOp_strh || vfpInstruction),
           "unrecognized opcode");

   // if doing a ldr then can re-use the target register as the index register (if it is not also the base register),
   // stores must always spill.
   if(op == ARMOp_str || op == ARMOp_strh || op == ARMOp_strb || vfpInstruction ||
      toRealRegister(currentInstruction->getMemoryDataRegister())->getRegisterNumber() == base->getRegisterNumber())
      {
      spillNeeded = true;
      rX = cg->machine()->getRealRegister(choose_rX(currentInstruction, base));
      stackPtr = cg->machine()->getRealRegister(cg->getLinkage()->getProperties().getStackPointerRegister());
      }
   else
      {
      spillNeeded = false;
      rX = toRealRegister(currentInstruction->getMemoryDataRegister());
      }

   if (!vfpInstruction)
      {
      self()->setIndexRegister(rX);
      }
   else
      {
      self()->setBaseRegister(rX);
      }

   if(spillNeeded)
      {
      // str [sp,-4], rX
      *wcursor = 0xE5000004;
      rX->setRegisterFieldRD(wcursor);
      stackPtr->setRegisterFieldRN(wcursor);
      wcursor++;
      cursor += ARM_INSTRUCTION_LENGTH;
      }
   // now load large constant into register rX
   cursor = armLoadConstantInstructions(rX,offset < 0 ? -offset : offset,cursor,wcursor,cg);

   if (!vfpInstruction)
      {
      wcursor = (uint32_t *)cursor;
      *wcursor = preserve;                    // now encode original instruction
      rX->setRegisterFieldRM(wcursor);
      }
   else
      {
      wcursor = (uint32_t *)cursor;
      // generate "add rX, rX, rBase"
      *wcursor = 0xE0800000;
      rX->setRegisterFieldRD(wcursor);
      rX->setRegisterFieldRN(wcursor);
      base->setRegisterFieldRM(wcursor);
      wcursor++;
      cursor += ARM_INSTRUCTION_LENGTH;
      *wcursor = preserve;
      rX->setRegisterFieldRN(wcursor); // modify the original instruction to use rX as the base register
      }

   if(op == ARMOp_ldr || op == ARMOp_str || op == ARMOp_ldrb || op == ARMOp_strb)
      *wcursor |= 1 << 25; // set I bit for index register

   if(spillNeeded)
      {
      wcursor++;
      cursor += ARM_INSTRUCTION_LENGTH;
      // ldr rX, [sp,-4]
      *wcursor = 0xE5100004;
      rX->setRegisterFieldRD(wcursor);
      stackPtr->setRegisterFieldRN(wcursor);
      }
   return cursor;
}

static TR::RealRegister::RegNum choose_rX(TR::Instruction *currentInstruction, TR::RealRegister *base)
   {
   TR::RealRegister::RegNum TSnum = toRealRegister(currentInstruction->getMemoryDataRegister())->getRegisterNumber();
   TR::RealRegister::RegNum basenum = base->getRegisterNumber();
   int32_t  exclude = ((TSnum == TR::RealRegister::gr3)?1:0)|
            ((TSnum == TR::RealRegister::gr2)?2:0)|
            ((basenum == TR::RealRegister::gr3)?1:0)|
            ((basenum == TR::RealRegister::gr2)?2:0);

   switch (exclude)
      {
      case 0:
      case 2:
         return(TR::RealRegister::gr3);
      case 1:
         return(TR::RealRegister::gr2);
      case 3:
         return(TR::RealRegister::gr1);
      }
   return(TR::RealRegister::gr3);
   }

static uint8_t *armLoadConstantInstructions(TR::RealRegister *trgReg, int32_t value, uint8_t *cursor, uint32_t *wcursor, TR::CodeGenerator *cg)
   {
   intParts localVal(value);

   // if these large delayed constants start showing up more frequently it would be worth using similar logic as in armLoadConstant
   // for now just pessimistically generate four instructions

   // mov rX, 0xff000000 & displacement
   *wcursor = 0xE3A00000;
   trgReg->setRegisterFieldRD(wcursor);
   *wcursor |= 1 << 10;    // rotate getByte3 up to bits 31-24
   *wcursor |= localVal.getByte3();
   wcursor++;
   cursor += ARM_INSTRUCTION_LENGTH;

   // add rX, rX, 0x00ff0000 & displacement
   *wcursor = 0xE2800000;
   trgReg->setRegisterFieldRN(wcursor);  //source1 reg
   trgReg->setRegisterFieldRD(wcursor);  //target reg
   *wcursor |= 2 << 10;     // rotate getByte2 up to bits 23-16
   *wcursor |= localVal.getByte2();
   wcursor++;
   cursor += ARM_INSTRUCTION_LENGTH;

   // add rX, rX, 0x0000ff00 & displacement
   *wcursor = 0xE2800000;
   trgReg->setRegisterFieldRN(wcursor);  //source1 reg
   trgReg->setRegisterFieldRD(wcursor);  //target reg
   *wcursor |= 3 << 10;     // rotate getByte1 up to bits 15-8
   *wcursor |= localVal.getByte1();
   wcursor++;
   cursor += ARM_INSTRUCTION_LENGTH;

   // add rX, rX, 0x000000ff & displacement
   *wcursor = 0xE2800000;
   trgReg->setRegisterFieldRN(wcursor);  //source1 reg
   trgReg->setRegisterFieldRD(wcursor);  //target reg
   *wcursor |= localVal.getByte0();
   wcursor++;
   cursor += ARM_INSTRUCTION_LENGTH;

   return cursor;
   }

static void loadRelocatableConstant(TR::Node               *node,
                                    TR::SymbolReference    *ref,
                                    TR::Register           *reg,
                                    TR::MemoryReference *mr,
                                    TR::CodeGenerator      *cg)
   {
   uint8_t                         *relocationTarget;
   TR_ExternalRelocationTargetKind  relocationKind;
   TR::Symbol                        *symbol;
   int32_t                          addr, offset, highAddr;
   TR::Instruction                  *instr;
   TR::Compilation * comp = cg->comp();
   symbol = ref->getSymbol();
   /* This method is called only if symbol->isStatic() && !ref->isUnresolved(). */
   TR_ASSERT(symbol->isStatic() || symbol->isMethod(), "loadRelocatableConstant, problem with new symbol hierarchy");

   bool isStatic = symbol->isStatic();
   bool isStaticField = isStatic && (ref->getCPIndex() > 0) && !symbol->isClassObject();
   bool isClass = isStatic && symbol->isClassObject();
   bool isPicSite = isClass;
   if (isPicSite && !cg->comp()->compileRelocatableCode()
       && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)symbol->getStaticSymbol()->getStaticAddress(), node))
      {
      TR_ASSERT(!comp->getOption(TR_AOT), "HCR: AOT is currently no supported");
      intptr_t address = (intptr_t)symbol->getStaticSymbol()->getStaticAddress();
      loadAddressConstantInSnippet(cg, node ? node : cg->getCurrentEvaluationTreeTop()->getNode(), address, reg); // isStore ? TR::InstOpCode::Op_st : TR::InstOpCode::Op_load
      return;
      }

   addr = symbol->isStatic() ? (uintptr_t)symbol->getStaticSymbol()->getStaticAddress() : (uintptr_t)symbol->getMethodSymbol()->getMethodAddress();

   if (symbol->isStartPC())
      {
      generateLoadStartPCInstruction(cg, node, reg, ref);
      /* The constant values do not mean anything here. They will be replaced in binary encoding phase. */
      generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, reg, reg, 0xad, 16);
      generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, reg, reg, 0xbe, 8);
      generateTrg1Src1ImmInstruction(cg, ARMOp_sub, node, reg, reg, 0xef, 0);
      return;
      }

   if (ref->isUnresolved()  || comp->compileRelocatableCode())
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
      else
         {
         cg->addSnippet(mr->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, ref, node->getOpCode().isStore(), false)));
         }
      }
   else
      {
      offset = addr & 0x000003FF;      // We have good reasons to have 10bit offset
      highAddr = addr & ~0x000003FF;
      instr = armLoadConstant(node, highAddr, reg, cg);
      mr->addToOffset(node, offset, cg);
      }
   }
