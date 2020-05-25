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
#include "codegen/AheadOfTimeCompile.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGeneratorUtils.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Linkage.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/RegisterDependency.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/Processors.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/List.hpp"
#include "p/codegen/GenerateInstructions.hpp"
#include "p/codegen/PPCAOTRelocation.hpp"
#include "p/codegen/PPCInstruction.hpp"
#include "p/codegen/PPCOpsDefines.hpp"
#include "p/codegen/PPCTableOfConstants.hpp"
#include "runtime/Runtime.hpp"

class TR_OpaqueClassBlock;

static TR::RealRegister::RegNum choose_rX(TR::Instruction *, TR::RealRegister *);


OMR::Power::MemoryReference::MemoryReference(
      TR::CodeGenerator *cg) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _modBase(NULL), _length(0),
   _staticRelocation(NULL),
   _unresolvedSnippet(NULL),
   _conditions(NULL),
   _flag(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _offset = _symbolReference->getOffset();
   }

OMR::Power::MemoryReference::MemoryReference(
      TR::Register *br,
      TR::Register *ir,
      uint8_t len,
      TR::CodeGenerator *cg) :
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(ir),
   _indexNode(NULL),
   _modBase(NULL),
   _unresolvedSnippet(NULL),
   _staticRelocation(NULL),
   _conditions(NULL),
   _length(len),
   _flag(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _offset = _symbolReference->getOffset();
   }

OMR::Power::MemoryReference::MemoryReference(
      TR::Register *br,
      int32_t disp,
      uint8_t len,
      TR::CodeGenerator *cg) :
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _modBase(NULL),
   _unresolvedSnippet(NULL),
   _staticRelocation(NULL),
   _conditions(NULL),
   _length(len),
   _offset(disp),
   _flag(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   }

//
// constructor called to generate a memory reference for all kinds of
// stores and non-constant(or address) loads
//

OMR::Power::MemoryReference::MemoryReference(TR::Node *rootLoadOrStore, uint32_t len, TR::CodeGenerator *cg)
   : _baseRegister(NULL), _indexRegister(NULL), _unresolvedSnippet(NULL),
     _modBase(NULL), _flag(0), _baseNode(NULL), _indexNode(NULL),
     _symbolReference(rootLoadOrStore->getSymbolReference()), _offset(0),
     _conditions(NULL), _length(len), _staticRelocation(NULL)
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *ref = rootLoadOrStore->getSymbolReference();
   TR::Symbol   *symbol = ref->getSymbol();
   bool        isStore = rootLoadOrStore->getOpCode().isStore();
   self()->checkRegisters(cg);

   TR::MethodSymbol * methodSymbol = rootLoadOrStore->getSymbol()->getMethodSymbol();
   if (rootLoadOrStore->getOpCode().isIndirect())
      {
      TR::Node *base = rootLoadOrStore->getFirstChild();
      bool     isLocalObject = ((base->getOpCodeValue() == TR::loadaddr) &&
                                base->getSymbol()->isLocalObject());

      if (!ref->isUnresolved() && isLocalObject)
         {
         _baseRegister = cg->getStackPointerRegister();
         _symbolReference = base->getSymbolReference();
         self()->checkRegisters(cg);
         _baseNode = base;
         }
      else
         {
         if (ref->isUnresolved())
            {
            // Stack late mapping forces us to evaluate local object's base into a register
            // rather than getting the stack pointer as the base which is not correct.
            if (isLocalObject)
               cg->evaluate(base);

            self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, rootLoadOrStore, ref, isStore, false));
            cg->addSnippet(self()->getUnresolvedSnippet());
            }
         // if an aconst feeds a iaload, we need to load the constant
         if (base->getOpCode().isLoadConst())
            {
            cg->evaluate(base);
            }
         if (symbol->isMethodMetaData())
            {
            _baseRegister = cg->getMethodMetaDataRegister();
            }
         self()->populateMemoryReference(base, cg);
         }
      }
   else
      {
      if (symbol->isStatic())
         {
         self()->accessStaticItem(rootLoadOrStore, ref, isStore, cg);
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

   if (cg->comp()->target().is32Bit() || !symbol->isConstObjectRef())
      self()->addToOffset(rootLoadOrStore, ref->getOffset(), cg);
   if (self()->getUnresolvedSnippet() != NULL)
      self()->adjustForResolution(cg);
   // TODO: aliasing sets?
   }

OMR::Power::MemoryReference::MemoryReference(TR::Node *node, TR::SymbolReference *symRef, uint32_t len, TR::CodeGenerator *cg)
   : _baseRegister(NULL), _indexRegister(NULL), _unresolvedSnippet(NULL),
     _modBase(NULL), _flag(0), _baseNode(NULL), _indexNode(NULL),
     _symbolReference(symRef), _offset(0),
     _conditions(NULL), _length(len), _staticRelocation(NULL)
   {
   TR::Symbol *symbol = symRef->getSymbol();

   if (symbol->isStatic())
      {
      self()->accessStaticItem(node, symRef, false, cg);
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

   if (cg->comp()->target().is32Bit() || !symbol->isConstObjectRef())
      self()->addToOffset(0, symRef->getOffset(), cg);
   if (self()->getUnresolvedSnippet() != NULL)
      self()->adjustForResolution(cg);
   // TODO: aliasing sets?
   }


OMR::Power::MemoryReference::MemoryReference(TR::Node *node, TR::MemoryReference& mr, int32_t displacement, uint32_t len, TR::CodeGenerator *cg)
   {
   _baseRegister      = mr._baseRegister;
   _baseNode          = NULL;
   _indexRegister     = mr._indexRegister;
   _indexNode         = NULL;
   _modBase           = mr._modBase;
   _symbolReference   = mr._symbolReference;
   _offset            = mr.getOffset();
   _flag              = 0;
   _length            = len;
   _staticRelocation  = NULL;
   _conditions        = NULL;
   mr._flag           = 0;

   TR_ASSERT(mr.getStaticRelocation() == NULL, "Relocated memory reference should not be re-used.");

   if (mr.getUnresolvedSnippet() != NULL)
      {
      _unresolvedSnippet = new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, _symbolReference, mr.getUnresolvedSnippet()->isUnresolvedStore(), false);
      cg->addSnippet(_unresolvedSnippet);

      self()->adjustForResolution(cg);
      }
   else
      {
      _unresolvedSnippet = NULL;
      }
   self()->addToOffset(0, displacement, cg);
   }

bool OMR::Power::MemoryReference::useIndexedForm()
   {
   return((_indexRegister==NULL && !self()->isUsingDelayedIndexedForm()) ? false : true );
   }

bool OMR::Power::MemoryReference::isTOCAccess()
   {
   // TODO: add checking for other type of TOC usages.
   return(self()->isUsingStaticTOC());
   }

int32_t OMR::Power::MemoryReference::getTOCOffset()
   {
   // TODO: add checking for other type of TOC usages.
   if (self()->isUsingStaticTOC())
      {
      return _symbolReference->getSymbol()->getStaticSymbol()->getTOCIndex() * sizeof(intptr_t);
      }
   return 0;
   }

void OMR::Power::MemoryReference::addToOffset(TR::Node * node, intptr_t amount, TR::CodeGenerator *cg)
   {
   // in compressedRefs mode, amount maybe heapBase constant, which in
   // most cases is quite large
   //
   TR::Compilation *comp = cg->comp();
   if (self()->getUnresolvedSnippet() != NULL &&
         (amount > LOWER_IMMED && amount < UPPER_IMMED))
      {
      self()->setOffset(self()->getOffset() + amount);
      return;
      }

   if (amount == 0)
      return;

   if (_baseRegister!=NULL && _indexRegister!=NULL)
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   intptr_t displacement = self()->getOffset() + amount;
   if (displacement<LOWER_IMMED || displacement>UPPER_IMMED)
      {
      TR::Register  *newBase;
      intptr_t     upper, lower;

      self()->setOffset(0);
      lower = (intptr_t)(int16_t)displacement;
      upper = displacement >> 16;
      if (lower < 0)
         upper++;
      if (_baseRegister!=NULL && self()->isBaseModifiable())
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
         if (node->getOpCode().isLoadConst() && node->getRegister())
            {
            generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, newBase, _baseRegister, node->getRegister());
            }
         else
            {
            if (cg->comp()->target().is64Bit() && (upper<LOWER_IMMED || upper>UPPER_IMMED))
               {
               TR::Register *tempReg = cg->allocateRegister();
               loadActualConstant(cg, node, upper<<16, tempReg);
               generateTrg1Src2Instruction(cg, TR::InstOpCode::add, node, newBase, _baseRegister, tempReg);
               cg->stopUsingRegister(tempReg);
               }
            else
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, newBase, _baseRegister, upper);
            if (lower != 0)
               generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi2, node, newBase, newBase, lower);
            }
         }
      else
         loadActualConstant(cg, node, displacement, newBase);

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

void OMR::Power::MemoryReference::forceIndexedForm(TR::Node * node, TR::CodeGenerator *cg, TR::Instruction *cursor)
   {
   if (self()->useIndexedForm())
      return;  // already X-form

   TR_ASSERT(!self()->isTOCAccess(), "X-form TOC access not supported");

   if (self()->getUnresolvedSnippet() != NULL)
      {
      // nothing to do except force an X-form at binary encoding time
      self()->setUsingDelayedIndexedForm();
      return;
      }

   if (self()->hasDelayedOffset())
      {
      self()->setUsingDelayedIndexedForm();
      if (!self()->isBaseModifiable())
         {
         _indexRegister = cg->allocateRegister();  // for encoding time use
         self()->setIndexModifiable();
         }
      return;
      }

   // true displacement available now
   intptr_t displacement = self()->getOffset();

   if (displacement == 0)
      {
      // force use of gr0 as RA in addressing
      _indexRegister = _baseRegister;
      _indexNode = _baseNode;
      if (self()->isBaseModifiable())
         {
         self()->setIndexModifiable();
         self()->clearBaseModifiable();
         }
      _baseRegister = NULL;
      _baseNode = NULL;
      return;
      }

   // load displacement into a register, make it the index
   self()->setOffset(0);
   TR::Register  *newIndex = cg->allocateRegister();
   if (!cursor)
      cursor = cg->getAppendInstruction();
   if (!node)
      node = cursor->getNode();
   loadActualConstant(cg, node, displacement, newIndex, cursor);
   _indexRegister = newIndex;
   _indexNode = NULL;
   self()->setIndexModifiable();
   cg->stopUsingRegister(newIndex);
   }

void OMR::Power::MemoryReference::adjustForResolution(TR::CodeGenerator *cg)
   {
   _modBase = cg->allocateRegister();
   _conditions = new (cg->trHeapMemory()) TR::RegisterDependencyConditions(1, 1, cg->trMemory());
   TR::addDependency(_conditions, _modBase, TR::RealRegister::gr11, TR_GPR, cg);
   }

void OMR::Power::MemoryReference::decNodeReferenceCounts(TR::CodeGenerator *cg)
   {
   if (_baseRegister != NULL)
      {
      if (_baseNode != NULL)
         {
         cg->decReferenceCount(_baseNode);
         }
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

   if (_modBase != NULL)
      cg->stopUsingRegister(_modBase);
   }

void OMR::Power::MemoryReference::bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg)
   {
   if (_baseRegister != NULL)
      {
      instr->useRegister(_baseRegister);
      }
   if (_indexRegister != NULL)
      {
      instr->useRegister(_indexRegister);
      }
   if (_modBase != NULL)
      {
      instr->useRegister(_modBase);
      _conditions->bookKeepingRegisterUses(instr, cg);
      }
   }

static bool isLoadConstAndShift(TR::Node *subTree, TR::CodeGenerator *cg)
   {
   if (cg->comp()->target().is64Bit())
      {
      if (subTree->getOpCodeValue() == TR::lshl)
         {
         // lshl
         //    lconst
         //    iconst
         if (subTree->getFirstChild()->getOpCode().isLoadConst() &&
             subTree->getSecondChild()->getOpCode().isLoadConst())
            return true;

         // lshl
         //    i2l
         //       iconst
         //    iconst
         else if (subTree->getFirstChild()->getOpCodeValue() == TR::i2l &&
                  subTree->getFirstChild()->getFirstChild()->getOpCode().isLoadConst() &&
                  subTree->getSecondChild()->getOpCode().isLoadConst())
            return true;
         else
            return false;
         }
      else
         return false;
      }
   else // 32-bit
      {
      if (subTree->getOpCodeValue() == TR::ishl &&
          subTree->getFirstChild()->getOpCode().isLoadConst() &&
          subTree->getSecondChild()->getOpCode().isLoadConst())
         return true;
      else
         return false;
      }
   }


void OMR::Power::MemoryReference::populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg)
   {
   if (cg->comp()->useCompressedPointers())
      {
      if (subTree->getOpCodeValue() == TR::l2a && subTree->getReferenceCount() == 1 && subTree->getRegister() == NULL)
         {
         cg->decReferenceCount(subTree);
         subTree = subTree->getFirstChild();
         }
      }

   // Skip sign extension if the offset is known to be >= 0; this is only safe to do if the child is the result of a load, otherwise the upper 32 bits may be undefined
   if (subTree->getOpCodeValue() == TR::i2l && subTree->isNonNegative() && subTree->getFirstChild()->getOpCode().isLoadVar() && subTree->getReferenceCount() == 1 && subTree->getRegister() == NULL)
      {
      if (performTransformation(cg->comp(), "O^O PPC Evaluator: Skip unecessary sign extension on index [%p].\n", subTree))
         {
         static bool dontSkipIndexSignExt = feGetEnv("TR_dontSkipIndexSignExt") != NULL;
         if (!dontSkipIndexSignExt)
            {
            cg->decReferenceCount(subTree);
            subTree = subTree->getFirstChild();
            }
         }
      }

   if (subTree->getReferenceCount()>1 || subTree->getRegister()!=NULL)
      {
      if (_baseRegister != NULL)
         {
         self()->consolidateRegisters(cg->evaluate(subTree), subTree, false, cg);
         }
      else
         {
         _baseRegister  = cg->evaluate(subTree);
         _baseNode      = subTree;
         }
      }
   else
      {
      if (subTree->getOpCode().isArrayRef() ||
          (subTree->getOpCodeValue() == TR::iadd || subTree->getOpCodeValue() == TR::ladd))
         {
         TR::Node     *addressChild    = subTree->getFirstChild();
         TR::Node     *integerChild    = subTree->getSecondChild();

         if (integerChild->getOpCode().isLoadConst())
            {
            self()->populateMemoryReference(addressChild, cg);
            if (cg->comp()->target().is64Bit())
               {
               intptr_t amount = (integerChild->getOpCodeValue() == TR::iconst) ?
                                   integerChild->getInt() :
                                   integerChild->getLongInt();
               self()->addToOffset(integerChild, amount, cg);
               }
            else
               self()->addToOffset(subTree, integerChild->getInt(), cg);
            cg->decReferenceCount(integerChild);
            }
         else if (integerChild->getEvaluationPriority(cg)>addressChild->getEvaluationPriority(cg) &&
                  !(subTree->getOpCode().isArrayRef() && cg->comp()->target().cpu.id()==TR_PPCp6))
            {
            self()->populateMemoryReference(integerChild, cg);
            self()->populateMemoryReference(addressChild, cg);
            }
         else
            {
            self()->populateMemoryReference(addressChild, cg);
            self()->populateMemoryReference(integerChild, cg);
            }
         cg->decReferenceCount(subTree);
         }
      else if (isLoadConstAndShift(subTree, cg))
         {
         if (cg->comp()->target().is64Bit())
            { // 64-bit
            intptr_t amount = (subTree->getSecondChild()->getOpCodeValue() == TR::iconst) ?
                                subTree->getSecondChild()->getInt() :
                                subTree->getSecondChild()->getLongInt();
            // lshl
            //    lconst
            //    iconst(lconst)
            if (subTree->getFirstChild()->getOpCode().isLoadConst())
               self()->addToOffset(subTree, subTree->getFirstChild()->getLongInt() << amount, cg);
            else
            // lshl
            //    i2l
            //       iconst
            //    iconst(lconst)
               {
               self()->addToOffset(subTree, subTree->getFirstChild()->getFirstChild()->getInt() << amount,
                           cg);
               cg->decReferenceCount(subTree->getFirstChild()->getFirstChild());
               }
            }
         else
            self()->addToOffset(subTree, subTree->getFirstChild()->getInt() <<
                        subTree->getSecondChild()->getInt(), cg);
         cg->decReferenceCount(subTree->getFirstChild());
         cg->decReferenceCount(subTree->getSecondChild());
         }
      else if ((subTree->getOpCodeValue() == TR::loadaddr) && !cg->comp()->compileRelocatableCode())
         {
         TR::SymbolReference *ref = subTree->getSymbolReference();
         TR::Symbol *symbol = ref->getSymbol();
         bool       isStore = subTree->getOpCode().isStore();
         _symbolReference = ref;
         self()->checkRegisters(cg);

         if (symbol->isStatic())
            {
            self()->accessStaticItem(subTree, ref, isStore, cg);
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
         if (cg->comp()->target().is32Bit() || !symbol->isConstObjectRef())
            self()->addToOffset(subTree, subTree->getSymbolReference()->getOffset(), cg);
         // TODO: aliasing sets?
         cg->decReferenceCount(subTree); // need to decrement ref count because
                                              // nodes weren't set on memoryreference
         }
      else if (subTree->getOpCodeValue() == TR::aconst ||
               subTree->getOpCodeValue() == TR::iconst || // subTree->getOpCode().isLoadConst ?
               subTree->getOpCodeValue() == TR::lconst)
         {
         intptr_t amount = (subTree->getOpCodeValue() == TR::iconst) ?
                             subTree->getInt() : subTree->getLongInt();
         self()->addToOffset(subTree, amount, cg);
         }
      else
         {
         if (_baseRegister != NULL)
            {
            self()->consolidateRegisters(cg->evaluate(subTree), subTree, cg->canClobberNodesRegister(subTree), cg);
            }
         else
            {
            _baseRegister  = cg->evaluate(subTree);
            _baseNode      = subTree;
            if (cg->canClobberNodesRegister(subTree)) self()->setBaseModifiable();
            }
         }
      }
   }


void OMR::Power::MemoryReference::consolidateRegisters(TR::Register *srcReg, TR::Node *srcTree, bool srcModifiable, TR::CodeGenerator *cg)
   {
   TR::Register *tempTargetRegister;
   TR::Compilation *comp = cg->comp();

       TR::Node *tempNode = (srcTree==NULL)?cg->getAppendInstruction()->getNode():srcTree;

   if (self()->getUnresolvedSnippet() != NULL)
      {
      if (srcReg == NULL)
         return;
      if (_indexRegister != NULL)
         {
         if (self()->isIndexModifiable())
            tempTargetRegister = _indexRegister;
         else if ((srcReg && (srcReg->containsCollectedReference() || srcReg->containsInternalPointer())) ||
                  (_indexRegister && (_indexRegister->containsCollectedReference() || _indexRegister->containsInternalPointer())))
           {
           if (srcTree!=NULL && srcTree->isInternalPointer() &&
               srcTree->getPinningArrayPointer())
              {
              tempTargetRegister = cg->allocateRegister();
              tempTargetRegister->setContainsInternalPointer();
              tempTargetRegister->setPinningArrayPointer(srcTree->getPinningArrayPointer());
              }
           else
              tempTargetRegister = cg->allocateCollectedReferenceRegister();
           }
        else
           {
           tempTargetRegister = cg->allocateRegister();
           }

         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, tempNode, tempTargetRegister, _indexRegister, srcReg);
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
         else if ((_baseRegister && (_baseRegister->containsCollectedReference() || _baseRegister->containsInternalPointer())) ||
                  (_indexRegister && (_indexRegister->containsCollectedReference() || _indexRegister->containsInternalPointer())))
            {
            if (srcTree!=NULL && srcTree->isInternalPointer() &&
                srcTree->getPinningArrayPointer())
               {
               tempTargetRegister = cg->allocateRegister();
               tempTargetRegister->setContainsInternalPointer();
               tempTargetRegister->setPinningArrayPointer(srcTree->getPinningArrayPointer());
               }
            else
               tempTargetRegister = cg->allocateCollectedReferenceRegister();
            }
         else
            {
            tempTargetRegister = cg->allocateRegister();
            }

         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, tempNode, tempTargetRegister, _baseRegister, _indexRegister);
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
         _baseRegister  = tempTargetRegister;
         self()->setBaseModifiable();
         }
      else if (srcReg!=NULL && (self()->getOffset(*comp)!=0 || self()->hasDelayedOffset()))
         {
         if (self()->isBaseModifiable())
            tempTargetRegister = _baseRegister;
         else if (srcModifiable)
            tempTargetRegister = srcReg;
         else if ((srcReg && (srcReg->containsCollectedReference() || srcReg->containsInternalPointer())) ||
                  (_baseRegister && (_baseRegister->containsCollectedReference() || _baseRegister->containsInternalPointer())))
            {
            if (srcTree!=NULL && srcTree->isInternalPointer() &&
               srcTree->getPinningArrayPointer())
               {
               tempTargetRegister = cg->allocateRegister();
               tempTargetRegister->setContainsInternalPointer();
               tempTargetRegister->setPinningArrayPointer(srcTree->getPinningArrayPointer());
               }
            else
               tempTargetRegister = cg->allocateCollectedReferenceRegister();
            }
         else
            {
            tempTargetRegister = cg->allocateRegister();
            }

         generateTrg1Src2Instruction(cg, TR::InstOpCode::add, tempNode, tempTargetRegister, _baseRegister, srcReg);
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



void OMR::Power::MemoryReference::assignRegisters(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   TR::Machine *machine = cg->machine();
   TR::RealRegister *assignedBaseRegister;
   TR::RealRegister *assignedIndexRegister;

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

      _conditions->assignPostConditionRegisters(currentInstruction, _modBase->getKind(), cg);
      _conditions->assignPreConditionRegisters(currentInstruction->getPrev(), _modBase->getKind(), cg);

      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      if (_indexRegister != NULL)
         {
         _indexRegister->unblock();
         }
      }

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

      if ((assignedBaseRegister != NULL) &&
          (toRealRegister(assignedBaseRegister)->getRegisterNumber() ==
           TR::RealRegister::gr0))
         {
         TR::RealRegister     *alternativeRegister;
         if ((alternativeRegister = machine->findBestFreeRegister(currentInstruction, TR_GPR, true, false, _baseRegister)) == NULL)
            {
            alternativeRegister = machine->freeBestRegister(currentInstruction, _baseRegister, NULL, true);
            }
         machine->coerceRegisterAssignment(currentInstruction, _baseRegister, toRealRegister(alternativeRegister)->getRegisterNumber());
         assignedBaseRegister = alternativeRegister;
         }
      else
         {
         if (assignedBaseRegister == NULL)
            {
            assignedBaseRegister = machine->assignOneRegister(currentInstruction, _baseRegister, true);
            }
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
         assignedIndexRegister = machine->assignOneRegister(currentInstruction, _indexRegister, false);
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

   //////////////////////////////////////
   // _modBase has to be freed outside.//
   //////////////////////////////////////

   if (_modBase != NULL)
      {
      _modBase = _modBase->getAssignedRealRegister();
      }

   if (_baseRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(_baseRegister);
      _baseRegister = assignedBaseRegister;
      }

   if (_indexRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(_indexRegister);
      _indexRegister = assignedIndexRegister;
      }

   if (self()->getUnresolvedSnippet() != NULL)
      {
      currentInstruction->PPCNeedsGCMap(0xFFFFFFFF);
      }
   }

void OMR::Power::MemoryReference::mapOpCode(TR::Instruction *currentInstruction)
   {
   TR::InstOpCode::Mnemonic op = currentInstruction->getOpCodeValue();
   TR::Register  *index = self()->getIndexRegister();

   if (index == NULL && !self()->isUsingDelayedIndexedForm())
      return;

   switch (op)
      {
      case TR::InstOpCode::lbz:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lbzx);
         break;
      case TR::InstOpCode::ld:
         currentInstruction->setOpCodeValue(TR::InstOpCode::ldx);
         break;
      case TR::InstOpCode::lfd:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfdx);
         break;
      case TR::InstOpCode::lfdu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfdux);
         break;
      case TR::InstOpCode::lfs:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfsx);
         break;
      case TR::InstOpCode::lfsu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lfsux);
         break;
      case TR::InstOpCode::lha:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhax);
         break;
      case TR::InstOpCode::lhau:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhaux);
         break;
      case TR::InstOpCode::lhz:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhzx);
         break;
      case TR::InstOpCode::lhzu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lhzux);
         break;
      case TR::InstOpCode::lwa:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lwax);
         break;
      case TR::InstOpCode::lwz:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lwzx);
         break;
      case TR::InstOpCode::lwzu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::lwzux);
         break;
      case TR::InstOpCode::stb:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stbx);
         break;
      case TR::InstOpCode::stbu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stbux);
         break;
      case TR::InstOpCode::std:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stdx);
         break;
      case TR::InstOpCode::stdu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stdux);
         break;
      case TR::InstOpCode::stfd:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfdx);
         break;
      case TR::InstOpCode::stfdu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfdux);
         break;
      case TR::InstOpCode::stfs:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfsx);
         break;
      case TR::InstOpCode::stfsu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stfsux);
         break;
      case TR::InstOpCode::sth:
         currentInstruction->setOpCodeValue(TR::InstOpCode::sthx);
         break;
      case TR::InstOpCode::sthu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::sthux);
         break;
      case TR::InstOpCode::stw:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stwx);
         break;
      case TR::InstOpCode::stwu:
         currentInstruction->setOpCodeValue(TR::InstOpCode::stwux);
         break;
      default:
         break;
      }
   }

TR::Instruction *OMR::Power::MemoryReference::expandForUnresolvedSnippet(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
#ifdef J9_PROJECT_SPECIFIC
   TR::Compilation *comp = cg->comp();
   TR::Node *node = currentInstruction->getNode();
   TR::Instruction *prevInstruction = currentInstruction->getPrev();
   TR::UnresolvedDataSnippet *snippet = self()->getUnresolvedSnippet();
   TR::MemoryReference *newMR = NULL;

   TR::RealRegister *base = toRealRegister(self()->getBaseRegister());
   TR::RealRegister *index = toRealRegister(self()->getIndexRegister());
   TR::RealRegister *data = toRealRegister(currentInstruction->getMemoryDataRegister());
   int32_t displacement = self()->getOffset(*comp);

   currentInstruction->remove();

   TR::Instruction *firstInstruction = generateLabelInstruction(cg, TR::InstOpCode::bl, node, getUnresolvedSnippet()->getSnippetLabel(), prevInstruction);
   prevInstruction = firstInstruction;

   if (comp->target().is64Bit() && self()->isTOCAccess())
      {
      displacement = self()->getTOCOffset();

      if (displacement == PTOC_FULL_INDEX)
         {
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, data, data, 0, prevInstruction);
         prevInstruction = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, data, data, 32, 0xffffffff00000000ULL, prevInstruction);
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::oris, node, data, data, 0, prevInstruction);
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, data, data, 0, prevInstruction);
         }
      else if (displacement < LOWER_IMMED || displacement > UPPER_IMMED)
         {
         newMR = new (cg->trHeapMemory()) TR::MemoryReference(self()->getModBase(), LO_VALUE(displacement), self()->getLength(), cg);
         }
      }
   else if (index != NULL || isUsingDelayedIndexedForm())
      {
      if (index == NULL)
         {
         newMR = new (cg->trHeapMemory()) TR::MemoryReference(cg->machine()->getRealRegister(TR::RealRegister::gr0), base, self()->getLength(), cg);
         }
      else
         {
         newMR = new (cg->trHeapMemory()) TR::MemoryReference(base, index, self()->getLength(), cg);
         }

      prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, base, base, 0, prevInstruction);
      }
   else
      {
      newMR = new (cg->trHeapMemory()) TR::MemoryReference(self()->getModBase(), 0, self()->getLength(), cg);
      }

   // Since J9UnresolvedDataSnippet will look at this memory reference's registers, modifying them in place to be
   // correct for binary encoding will cause that to break. Instead, we leave the current memory reference alone and
   // simply replace the current instruction with an entirely new one.
   if (newMR != NULL)
      {
      switch (currentInstruction->getKind())
         {
         case TR::Instruction::IsMem:
            prevInstruction = generateMemInstruction(
               cg,
               currentInstruction->getOpCodeValue(),
               currentInstruction->getNode(),
               newMR,
               prevInstruction
            );
            break;
         case TR::Instruction::IsMemSrc1:
            prevInstruction = generateMemSrc1Instruction(
               cg,
               currentInstruction->getOpCodeValue(),
               currentInstruction->getNode(),
               newMR,
               static_cast<TR::PPCMemSrc1Instruction*>(currentInstruction)->getSourceRegister(),
               prevInstruction
            );
            break;
         case TR::Instruction::IsTrg1Mem:
            prevInstruction = generateTrg1MemInstruction(
               cg,
               currentInstruction->getOpCodeValue(),
               currentInstruction->getNode(),
               static_cast<TR::PPCTrg1MemInstruction*>(currentInstruction)->getTargetRegister(),
               newMR,
               prevInstruction
            );
            break;
         default:
            TR_ASSERT_FATAL_WITH_INSTRUCTION(currentInstruction, false, "Unrecognized instruction kind %d for memory instruction", currentInstruction->getKind());
         }
      }

   if (currentInstruction->needsGCMap())
      {
      firstInstruction->setNeedsGCMap(currentInstruction->getGCRegisterMask());
      firstInstruction->setGCMap(currentInstruction->getGCMap());
      }

   snippet->setDataReferenceInstruction(firstInstruction);
   snippet->setMemoryReference(self());
   snippet->setDataRegister(data);

   return prevInstruction;
#else
   TR_UNIMPLEMENTED();
#endif
   }

TR::Instruction *OMR::Power::MemoryReference::expandInstruction(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   // Due to the way the generate*Instruction helpers work, there's no way to use them to generate an instruction at the
   // start of the instruction stream at the moment. As a result, we cannot peform expansion if the first instruction is
   // a memory instruction.
   TR_ASSERT_FATAL(currentInstruction->getPrev(), "The first instruction cannot be a memory instruction");

   self()->mapOpCode(currentInstruction);

   if (self()->getUnresolvedSnippet() != NULL)
      return self()->expandForUnresolvedSnippet(currentInstruction, cg);

   if (!self()->getBaseRegister())
      {
      if (self()->getModBase())
         self()->setBaseRegister(self()->getModBase());
      else
         self()->setBaseRegister(cg->machine()->getRealRegister(TR::RealRegister::gr0));
      }
   else
      {
      TR_ASSERT_FATAL_WITH_INSTRUCTION(currentInstruction, !self()->getModBase(), "Cannot have mod base and base register");
      }

   TR::Compilation *comp = cg->comp();
   TR::Node *node = currentInstruction->getNode();
   TR::Instruction *prevInstruction = currentInstruction->getPrev();

   TR::RealRegister *base = toRealRegister(self()->getBaseRegister());
   TR::RealRegister *index = toRealRegister(self()->getIndexRegister());
   TR::RealRegister *data = toRealRegister(currentInstruction->getMemoryDataRegister());
   int32_t displacement = self()->getOffset(*comp);

   self()->setOffset(displacement);
   self()->setDelayedOffsetDone();

   TR_ASSERT_FATAL_WITH_INSTRUCTION(
      currentInstruction,
      index == NULL || displacement == 0 || self()->isUsingDelayedIndexedForm(),
      "Cannot have an index register and a displacement unless using delayed indexed form"
   );

   if (self()->getStaticRelocation() != NULL)
      {
      prevInstruction = generateLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg), prevInstruction);
      self()->getStaticRelocation()->setSource2Instruction(prevInstruction);
      }

   if (comp->target().is64Bit() && self()->isTOCAccess())
      {
      int32_t displacement = self()->getTOCOffset();
      TR::SymbolReference *symRef = self()->getSymbolReference();
      TR::Symbol *symbol = symRef->getSymbol();
      uint64_t addr;

      if (symbol->isStatic())
         addr = (uint64_t) symbol->getStaticSymbol()->getStaticAddress();
      else if (symbol->isMethod())
         addr = (uint64_t) symbol->getMethodSymbol()->getMethodAddress();
      else
         TR_ASSERT_FATAL_WITH_INSTRUCTION(currentInstruction, false, "Symbol %s has unrecognized type for PTOC access", comp->getDebug()->getName(symbol));

      if (displacement != PTOC_FULL_INDEX)
         {
         if (!self()->getSymbolReference()->isUnresolved() && TR_PPCTableOfConstants::getTOCSlot(displacement) == 0)
            TR_PPCTableOfConstants::setTOCSlot(displacement, addr);

         if (displacement < LOWER_IMMED || displacement > UPPER_IMMED)
            {
            generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, data, base, cg->hiValue(displacement), prevInstruction);
            self()->setBaseRegister(data);
            }

         self()->setOffset(LO_VALUE(displacement));
         }
      else
         {
         if (cg->comp()->compileRelocatableCode() && symbol->isStatic() && symbol->isClassObject())
            {
            prevInstruction = generateLabelInstruction(cg, TR::InstOpCode::label, node, generateLabelSymbol(cg), prevInstruction);

            TR_RelocationRecordInformation *recordInfo = (TR_RelocationRecordInformation*)cg->trMemory()->allocateHeapMemory(sizeof(TR_RelocationRecordInformation));

            if (comp->getOption(TR_UseSymbolValidationManager))
               {
               recordInfo->data1 = (uintptr_t)symbol->getStaticSymbol()->getStaticAddress();
               recordInfo->data2 = (uintptr_t)TR::SymbolType::typeClass;
               recordInfo->data3 = fixedSequence1;
               cg->addExternalRelocation(
                  new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                     prevInstruction,
                     (uint8_t*)recordInfo,
                     TR_DiscontiguousSymbolFromManager,
                     cg
                  ),
                  __FILE__,
                  __LINE__,
                  node
               );
               }
            else
               {
               recordInfo->data1 = (uintptr_t)symRef;
               recordInfo->data2 = node == NULL ? -1 : node->getInlinedSiteIndex();
               recordInfo->data3 = fixedSequence1;
               cg->addExternalRelocation(
                  new (cg->trHeapMemory()) TR::BeforeBinaryEncodingExternalRelocation(
                     prevInstruction,
                     (uint8_t*)recordInfo,
                     TR_ClassAddress,
                     cg
                  ),
                  __FILE__,
                  __LINE__,
                  node
               );
               }

            addr = 0;
            }

         TR::Instruction *newInstruction = prevInstruction = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, data, (int16_t)(addr >> 48), prevInstruction);
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, data, data, (addr >> 32) & 0xffff, prevInstruction);
         prevInstruction = generateTrg1Src1Imm2Instruction(cg, TR::InstOpCode::rldicr, node, data, data, 32, 0xffffffff00000000ULL, prevInstruction);
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::oris, node, data, data, (addr >> 16) & 0xffff, prevInstruction);
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, data, data, addr & 0xffff, prevInstruction);

         currentInstruction->remove();
         currentInstruction = newInstruction;
         }
      }
   else if (self()->isUsingDelayedIndexedForm())
      {
      if (index != NULL)
         {
         if (displacement < LOWER_IMMED || displacement > UPPER_IMMED)
            {
            prevInstruction = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, index, (int16_t)(displacement >> 16), prevInstruction);
            prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::ori, node, index, index, displacement & 0xffff, prevInstruction);
            }
         else if (displacement != 0)
            {
            prevInstruction = generateTrg1ImmInstruction(cg, TR::InstOpCode::li, node, index, displacement, prevInstruction);
            }
         else
            {
            // To implicitly use a displacement of 0, put the base register into the index position and use r0 as the
            // base register.
            self()->setIndexRegister(base);
            self()->setBaseRegister(cg->machine()->getRealRegister(TR::RealRegister::gr0));
            }
         }
      else
         {
         TR_ASSERT_FATAL_WITH_INSTRUCTION(
            currentInstruction,
            self()->isBaseModifiable(),
            "Delayed index form without index register requires isBaseModifiable"
         );

         if (displacement < LOWER_IMMED || displacement > UPPER_IMMED)
            {
            prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, base, base, cg->hiValue(displacement), prevInstruction);
            displacement = LO_VALUE(displacement);
            }

         if (displacement != 0)
            prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addi, node, base, base, displacement, prevInstruction);

         self()->setIndexRegister(base);
         self()->setBaseRegister(cg->machine()->getRealRegister(TR::RealRegister::gr0));
         }

      self()->setOffset(0);
      }
   else if (displacement < LOWER_IMMED || displacement > UPPER_IMMED)
      {
      if (self()->isBaseModifiable())
         {
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, base, base, cg->hiValue(displacement), prevInstruction);
         self()->setOffset(LO_VALUE(displacement));
         }
      else
         {
         TR::RealRegister *stackPtr = cg->getStackPointerRegister();
         TR::RealRegister *rX = cg->machine()->getRealRegister(choose_rX(currentInstruction, base));
         int32_t saveLen = comp->target().is64Bit() ? 8 : 4;

         prevInstruction = generateMemSrc1Instruction(
            cg,
            TR::InstOpCode::Op_st,
            node,
            new (comp->trHeapMemory()) TR::MemoryReference(stackPtr, -saveLen, saveLen, cg),
            rX,
            prevInstruction
         );
         prevInstruction = generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addis, node, rX, base, cg->hiValue(displacement), prevInstruction);

         self()->setBaseRegister(rX);
         self()->setOffset(LO_VALUE(displacement));

         currentInstruction = generateTrg1MemInstruction(
            cg,
            TR::InstOpCode::Op_load,
            node,
            rX,
            new (comp->trHeapMemory()) TR::MemoryReference(stackPtr, -saveLen, saveLen, cg),
            currentInstruction
         );
         }
      }

   return currentInstruction;
   }

static TR::RealRegister::RegNum choose_rX(TR::Instruction *currentInstruction, TR::RealRegister *base)
   {
   TR::RealRegister::RegNum TSnum = toRealRegister(currentInstruction->getMemoryDataRegister())->getRegisterNumber();
   TR::RealRegister::RegNum basenum = base->getRegisterNumber();
   int32_t  exclude = ((TSnum == TR::RealRegister::gr10)?1:0)|
                      ((TSnum == TR::RealRegister::gr9)?2:0)|
                      ((basenum == TR::RealRegister::gr10)?1:0)|
                      ((basenum == TR::RealRegister::gr9)?2:0);
   switch (exclude)
      {
      case 0:
      case 2:
         return(TR::RealRegister::gr10);
      case 1:
         return(TR::RealRegister::gr9);
      case 3:
         return(TR::RealRegister::gr8);
      }
   return(TR::RealRegister::gr10);
   }

extern "C" void voom(){}
void OMR::Power::MemoryReference::accessStaticItem(TR::Node *node, TR::SymbolReference *ref, bool isStore, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Symbol                       *symbol;
   symbol = ref->getSymbol();
   TR_ASSERT(symbol->isStatic() || symbol->isMethod(), "accessStaticItem:problem with new symbol hierarchy");

   TR_ASSERT(_baseRegister==NULL, "32Bit JIT has relocation problem for AOT");

   bool isStatic = symbol->isStatic() && !ref->isUnresolved();
   bool isStaticField = isStatic && (ref->getCPIndex() > 0) && !symbol->isClassObject();
   bool isClass = isStatic && symbol->isClassObject();
   bool isPicSite = isClass;
   if (isPicSite && !cg->comp()->compileRelocatableCode()
       && cg->wantToPatchClassPointer((TR_OpaqueClassBlock*)symbol->getStaticSymbol()->getStaticAddress(), node))
      {
      TR_ASSERT(!comp->getOption(TR_AOT), "HCR: AOT is currently no supported");
      TR::Register *reg = _baseRegister = cg->allocateRegister();
      intptr_t address = (intptr_t)symbol->getStaticSymbol()->getStaticAddress();
      loadAddressConstantInSnippet(cg, node ? node : cg->getCurrentEvaluationTreeTop()->getNode(), address, reg, NULL, isStore?TR::InstOpCode::Op_st :TR::InstOpCode::Op_load, false, NULL);
      return;
      }

   bool useUnresSnippetToAvoidRelo = cg->comp()->compileRelocatableCode();
   if (useUnresSnippetToAvoidRelo)
      {
      // The unresolved snippet can't handle fabricated symrefs, so we'll have
      // to do a relocation. For now, keep using the unresolved snippet for
      // typical class symrefs even though they could generate relocations too.
      bool reloIsRequired =
         symbol->isStatic()
         && symbol->isClassObject()
         && ref->getCPIndex() == -1;

      if (reloIsRequired)
         useUnresSnippetToAvoidRelo = false;
      }

   if (cg->comp()->target().is64Bit())
      {
      TR::Node *topNode = cg->getCurrentEvaluationTreeTop()->getNode();
      TR::UnresolvedDataSnippet *snippet = NULL;
      int32_t tocIndex = symbol->getStaticSymbol()->getTOCIndex();
 
      /* don't want to trash node prematurely by the code for handling other symbols */
      TR::Node *nodeForSymbol = node;
      if (!nodeForSymbol)
         nodeForSymbol = cg->getCurrentEvaluationTreeTop()->getNode();

      if (symbol->isDebugCounter() && cg->comp()->compileRelocatableCode())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, true, nodeForSymbol, 1, reg, NULL, false, TR_DebugCounter);
         return;
         }
      if (symbol->isCountForRecompile() && cg->needRelocationsForPersistentInfoData())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, true, nodeForSymbol, TR_CountForRecompile, reg, NULL, false, TR_GlobalValue);
         return;
         }
      else if (symbol->isRecompilationCounter() && cg->needRelocationsForBodyInfoData())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, true, nodeForSymbol, 1, reg, NULL, false, TR_BodyInfoAddressLoad);
         return;
         }
      else if (symbol->isCompiledMethod() && cg->needRelocationsForCurrentMethodPC())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, true, nodeForSymbol, 1, reg, NULL, false, TR_RamMethodSequence);
         return;
         }
      else if (symbol->isStartPC())
         {
         // use inSnippet, as the relocation mechanism is already set up there
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstantInSnippet(cg, nodeForSymbol, 0, reg, NULL, TR::InstOpCode::addi, false, NULL);
         return;
         }
      else if (isStaticField && !ref->isUnresolved() && cg->needRelocationsForStatics())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, true, nodeForSymbol, 1, reg, NULL, false, TR_DataAddress);
         return;
         }
      else if (isClass && !ref->isUnresolved() && comp->getOption(TR_UseSymbolValidationManager) && cg->needClassAndMethodPointerRelocations())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, true, nodeForSymbol, (intptr_t)ref, reg, NULL, false, TR_ClassAddress);
         return;
         }
      else
         {
         TR_ASSERT_FATAL(!comp->getOption(TR_UseSymbolValidationManager) || ref->isUnresolved(), "SVM relocation unhandled");
         }

      // TODO: find a better default uninitialized value --- 0x80000000
      //       Deciding particular static references through TOC or not
      // startPC symbols should not be placed in the TOC as their value is not known at this time
      // and they are different from method to method
      if (tocIndex == 0 && !symbol->isStartPC())
         {
         tocIndex = TR_PPCTableOfConstants::lookUp(ref, cg);
         symbol->getStaticSymbol()->setTOCIndex(tocIndex);
         }

      // We need a snippet for unresolved or AOT if:
      //  1. the load hasn't been resolved yet
      //  (otherwise optimizer will remove the ResolveCHK and we can be sure pTOC will contain the resolved address),
      //  2. we don't have a PTOC slot, we must always take the slow path

      if ((ref->isUnresolved() || useUnresSnippetToAvoidRelo) &&
          (topNode->getOpCodeValue() == TR::ResolveCHK || tocIndex == PTOC_FULL_INDEX))
         {
         snippet = new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, ref, isStore, false);
         cg->addSnippet(snippet);
         }

      // TODO: Improve the code sequence for cases when we know pTOC is full.
      TR::MemoryReference *tocRef = new (cg->trHeapMemory()) TR::MemoryReference(cg->getTOCBaseRegister(), 0, sizeof(uintptr_t), cg);
      tocRef->setSymbol(symbol, cg);
      tocRef->getSymbolReference()->copyFlags(ref);
      tocRef->setUsingStaticTOC();
      if (snippet != NULL)
         {
         tocRef->setUnresolvedSnippet(snippet);
         tocRef->adjustForResolution(cg);
         }

      TR::Register *addrReg = cg->allocateRegister();
      TR::InstOpCode::Mnemonic loadOp = TR::InstOpCode::ld;

      generateTrg1MemInstruction(cg, loadOp, node==NULL?topNode:node, addrReg, tocRef);
      if (snippet != NULL)
         cg->stopUsingRegister(tocRef->getModBase());
      _baseRegister = addrReg;
      self()->setBaseModifiable();
      }
   else
      {
      // don't want to trash node prematurely by the code for handling other symbols */
      TR::Node *nodeForSymbol = node;
      if (!nodeForSymbol)
         nodeForSymbol = cg->getCurrentEvaluationTreeTop()->getNode();
      bool refIsUnresolved = ref->isUnresolved();

      if ((refIsUnresolved || comp->compileRelocatableCode()) && symbol->isDebugCounter())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, comp->compileRelocatableCode(), nodeForSymbol, 1, reg, NULL, false, TR_DebugCounter);
         return;
         }
      else if ((refIsUnresolved || cg->needRelocationsForPersistentInfoData()) && symbol->isCountForRecompile())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, cg->needRelocationsForPersistentInfoData(), nodeForSymbol, TR_CountForRecompile, reg, NULL, false, TR_GlobalValue);
         return;
         }
      else if ((refIsUnresolved || cg->needRelocationsForBodyInfoData()) && symbol->isRecompilationCounter())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, cg->needRelocationsForBodyInfoData(), nodeForSymbol, 0, reg, NULL, false, TR_BodyInfoAddressLoad);
         return;
         }
      else if (symbol->isCompiledMethod() && (ref->isUnresolved() || cg->needRelocationsForCurrentMethodPC()))
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, cg->needRelocationsForCurrentMethodPC(), nodeForSymbol, 0, reg, NULL, false, TR_RamMethodSequence);
         return;
         }
      else if (symbol->isStartPC() && (ref->isUnresolved() || cg->needRelocationsForCurrentMethodPC()))
         {
         // use inSnippet, as the relocation mechanism is already set up there
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstantInSnippet(cg, nodeForSymbol, 0, reg, NULL, TR::InstOpCode::addi, false, NULL);
         return;
         }
      else if (isStaticField && !refIsUnresolved && cg->needRelocationsForStatics())
         {
         TR::Register *reg = _baseRegister = cg->allocateRegister();
         loadAddressConstant(cg, true, nodeForSymbol, 1, reg, NULL, false, TR_DataAddress);
         return;
         }
      else if (refIsUnresolved || useUnresSnippetToAvoidRelo)
         {
         self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, node, ref, isStore, false));
         cg->addSnippet(self()->getUnresolvedSnippet());
         return;
         }

      TR::Instruction                 *rel1;
      TR::PPCPairedRelocation         *staticRelocation;
      TR::Register                    *reg = _baseRegister = cg->allocateRegister();
      TR_ExternalRelocationTargetKind relocationKind;
      intptr_t                        addr;

      addr = symbol->isStatic() ? (intptr_t)symbol->getStaticSymbol()->getStaticAddress() :
                                  (intptr_t)symbol->getMethodSymbol()->getMethodAddress();

      if (!node) node = cg->getCurrentEvaluationTreeTop()->getNode();

      if (symbol->isStartPC())
         {
         loadAddressConstantInSnippet(cg, node, 0, reg, NULL, TR::InstOpCode::addi, false, NULL);
         return;
         }

      self()->setBaseModifiable();
      rel1 = generateTrg1ImmInstruction(cg, TR::InstOpCode::lis, node, reg, cg->hiValue(addr));
      self()->addToOffset(node, LO_VALUE(addr), cg);

      if (cg->needClassAndMethodPointerRelocations())
         {
         TR_ASSERT_FATAL(
            symbol->isClassObject(),
            "expected a class symbol for symref #%d\n",
            ref->getReferenceNumber());

         TR_RelocationRecordInformation *recordInfo = ( TR_RelocationRecordInformation *)comp->trMemory()->allocateMemory(sizeof( TR_RelocationRecordInformation), heapAlloc);
         recordInfo->data1 = (uintptr_t)ref;
         recordInfo->data2 = node ? (uintptr_t)node->getInlinedSiteIndex() : (uintptr_t)-1;
         recordInfo->data3 = orderedPairSequence1;
         staticRelocation = new (cg->trHeapMemory()) TR::PPCPairedRelocation(rel1, NULL, (uint8_t *)recordInfo, TR_ClassAddress, node);
         cg->getAheadOfTimeCompile()->getRelocationList().push_front(staticRelocation);
         self()->setStaticRelocation(staticRelocation);
         }
      }
   }

void
OMR::Power::MemoryReference::setOffsetRequiresWordAlignment(
      TR::Node *node,
      TR::CodeGenerator *cg,
      TR::Instruction *cursor)
   {
   _flag |=  TR_PPCMemoryReferenceControl_OffsetRequiresWordAlignment;
   if (self()->getOffset() & 0x3)
      {
      self()->forceIndexedForm(node, cg, cursor);
      }
   }

void
OMR::Power::MemoryReference::setSymbol(
      TR::Symbol *symbol,
      TR::CodeGenerator *cg)
   {
   TR_ASSERT(_symbolReference->getSymbol()==NULL || _symbolReference->getSymbol()==symbol, "should not write to existing symbolReference");
   _symbolReference->setSymbol(symbol);

   if (_baseRegister!=NULL && _indexRegister!=NULL &&
       (self()->hasDelayedOffset() || self()->getOffset(*cg->comp())!=0))
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   }

void
OMR::Power::MemoryReference::checkRegisters(TR::CodeGenerator *cg)
   {
   if (_baseRegister!=NULL && _indexRegister!=NULL &&
       (self()->hasDelayedOffset() || self()->getOffset(*cg->comp())!=0))
      {
      self()->consolidateRegisters(NULL, NULL, false, cg);
      }
   }

int32_t
OMR::Power::MemoryReference::getOffset(TR::Compilation& comp)
   {
   int32_t displacement = _offset;
   if (self()->hasDelayedOffset() && !self()->isDelayedOffsetDone())
      displacement += _symbolReference->getSymbol()->getOffset();

   return(displacement);
   }
