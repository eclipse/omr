/*******************************************************************************
 * Copyright IBM Corp. and others 2018
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "codegen/MemoryReference.hpp"

#include <stddef.h>
#include <stdint.h>
#include "codegen/ARM64Instruction.hpp"
#include "codegen/CodeGenerator.hpp"
#include "codegen/GenerateInstructions.hpp"
#include "codegen/Machine.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/StaticSymbol.hpp"

TR::MemoryReference *TR::MemoryReference::create(TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(cg);
   }

TR::MemoryReference *TR::MemoryReference::createWithIndexReg(TR::CodeGenerator *cg, TR::Register *baseReg, TR::Register *indexReg, uint8_t scale, TR::ARM64ExtendCode extendCode)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(baseReg, indexReg, cg);
   }

TR::MemoryReference *TR::MemoryReference::createWithDisplacement(TR::CodeGenerator *cg, TR::Register *baseReg, int64_t displacement)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(baseReg, displacement, cg);
   }

TR::MemoryReference *TR::MemoryReference::createWithRootLoadOrStore(TR::CodeGenerator *cg, TR::Node *rootLoadOrStore)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(rootLoadOrStore, cg);
   }

TR::MemoryReference *TR::MemoryReference::createWithSymRef(TR::CodeGenerator *cg, TR::Node *node, TR::SymbolReference *symRef)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(node, symRef, cg);
   }

static void loadRelocatableConstant(TR::Node *node,
                                    TR::SymbolReference *ref,
                                    TR::Register *reg,
                                    TR::MemoryReference *mr,
                                    TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR_ASSERT_FATAL(!ref->isUnresolved(), "Symbol Reference (%p) must be resolved", ref);

   TR::Symbol *symbol = ref->getSymbol();
   bool isStatic = symbol->isStatic();
   bool isStaticField = isStatic && (ref->getCPIndex() > 0) && !symbol->isClassObject();
   bool isClass = isStatic && symbol->isClassObject();

   uintptr_t addr = symbol->isStatic() ? (uintptr_t)symbol->getStaticSymbol()->getStaticAddress() : (uintptr_t)symbol->getMethodSymbol()->getMethodAddress();

   if (symbol->isStartPC())
      {
      generateTrg1ImmSymInstruction(cg, TR::InstOpCode::adr, node, reg, addr, symbol);
      return;
      }

   if (symbol->isGCRPatchPoint())
      {
      generateTrg1ImmSymInstruction(cg, TR::InstOpCode::adr, node, reg, addr, symbol);
      return;
      }

   TR::Node *GCRnode = node;
   if (!GCRnode)
      GCRnode = cg->getCurrentEvaluationTreeTop()->getNode();

   if (symbol->isCountForRecompile() && cg->needRelocationsForPersistentInfoData())
      {
      loadAddressConstant(cg, true, GCRnode, TR_CountForRecompile, reg, NULL, TR_GlobalValue);
      }
   else if (symbol->isRecompilationCounter() && cg->needRelocationsForBodyInfoData())
      {
      loadAddressConstant(cg, true, GCRnode, 1, reg, NULL, TR_BodyInfoAddressLoad);
      }
   else if (symbol->isCatchBlockCounter() && cg->needRelocationsForBodyInfoData())
      {
      loadAddressConstant(cg, true, GCRnode, 1, reg, NULL, TR_CatchBlockCounter);
      }
   else if (symbol->isCompiledMethod() && cg->needRelocationsForCurrentMethodPC())
      {
      loadAddressConstant(cg, true, GCRnode, 1, reg, NULL, TR_RamMethodSequence);
      }
   else if (isStaticField && !ref->isUnresolved() && cg->needRelocationsForStatics())
      {
      loadAddressConstant(cg, true, GCRnode, 1, reg, NULL, TR_DataAddress);
      }
   else if (isClass && !ref->isUnresolved() && cg->needClassAndMethodPointerRelocations())
      {
      loadAddressConstant(cg, true, GCRnode, (intptr_t)ref, reg, NULL, TR_ClassAddress);
      }
   else if (symbol->isEnterEventHookAddress() || symbol->isExitEventHookAddress())
      {
      loadAddressConstant(cg, true, GCRnode, 1, reg, NULL, TR_MethodEnterExitHookAddress);
      }
   else if (symbol->isCallSiteTableEntry() && !ref->isUnresolved() && comp->compileRelocatableCode())
      {
      loadAddressConstant(cg, true, GCRnode, 1, reg, NULL, TR_CallsiteTableEntryAddress);
      }
   else if (symbol->isMethodTypeTableEntry() && !ref->isUnresolved() && comp->compileRelocatableCode())
      {
      loadAddressConstant(cg, true, GCRnode, 1, reg, NULL, TR_MethodTypeTableEntryAddress);
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
   _scale(0),
   _length(0)
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
   _scale(0),
   _length(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   _offset = _symbolReference->getOffset();
   }


OMR::ARM64::MemoryReference::MemoryReference(
      TR::Register *br,
      intptr_t disp,
      TR::CodeGenerator *cg) :
   _baseRegister(br),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _scale(0),
   _offset(disp),
   _length(0)
   {
   _symbolReference = new (cg->trHeapMemory()) TR::SymbolReference(cg->comp()->getSymRefTab());
   }


OMR::ARM64::MemoryReference::MemoryReference(
      TR::Node *rootLoadOrStore,
      TR::CodeGenerator *cg) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _scale(0),
   _offset(0),
   _symbolReference(rootLoadOrStore->getSymbolReference())
   {
   TR::Compilation *comp = cg->comp();
   TR::SymbolReference *ref = rootLoadOrStore->getSymbolReference();
   TR::Symbol *symbol = ref->getSymbol();
   bool isStore = rootLoadOrStore->getOpCode().isStore();
   TR::Node *normalizeNode = rootLoadOrStore;

   _length = rootLoadOrStore->getSize();
   self()->setSymbol(symbol, cg);

   if (rootLoadOrStore->getOpCode().isIndirect())
      {
      TR::Node *base = rootLoadOrStore->getFirstChild();
      bool     isLocalObject = ((base->getOpCodeValue() == TR::loadaddr) &&
                                base->getSymbol()->isLocalObject());
      // Special case an indirect load or store off a local object. This
      // can be treated as a direct load or store off the frame pointer
      // We can't do this when the access is unresolved.
      //
      if (!ref->isUnresolved() && isLocalObject)
         {
         _baseRegister = cg->getStackPointerRegister();
         _symbolReference = base->getSymbolReference();
         _baseNode = base;
         }
      else
         {
         if (ref->isUnresolved())
            {
            // If it is an unresolved reference to a field of a local object
            // then force the localobject address to be evaluated into a register
            // otherwise the resolution may not work properly if the computation
            // is folded away into a stack pointer + offset computation
            if (isLocalObject)
               cg->evaluate(base);

            self()->setUnresolvedSnippet(new (cg->trHeapMemory()) TR::UnresolvedDataSnippet(cg, rootLoadOrStore, rootLoadOrStore->getSymbolReference(), isStore, false));
            cg->addSnippet(self()->getUnresolvedSnippet());
            }
         // if an aconst feeds an aloadi, we need to load the constant
         if (base->getOpCode().isLoadConst())
            {
            cg->evaluate(base);
            }
         if (symbol->isMethodMetaData())
            {
            _baseRegister = cg->getMethodMetaDataRegister();
            }

         self()->populateMemoryReference(rootLoadOrStore->getFirstChild(), cg);
         normalizeNode = rootLoadOrStore->getFirstChild();
         }
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
   self()->normalize(normalizeNode, cg);
   }


OMR::ARM64::MemoryReference::MemoryReference(
      TR::Node *node,
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _extraRegister(NULL),
   _unresolvedSnippet(NULL),
   _flag(0),
   _scale(0),
   _offset(0),
   _symbolReference(symRef),
   _length(0)
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
   self()->normalize(node, cg);
   }

void OMR::ARM64::MemoryReference::setSymbol(TR::Symbol *symbol, TR::CodeGenerator *cg)
   {
   TR_ASSERT(_symbolReference->getSymbol()==NULL || _symbolReference->getSymbol()==symbol, "should not write to existing symbolReference");
   _symbolReference->setSymbol(symbol);

   if (_baseRegister != NULL && _indexRegister != NULL &&
       (self()->hasDelayedOffset() || self()->getOffset(true) != 0))
      {
      self()->consolidateRegisters(NULL, cg);
      }
   }

void OMR::ARM64::MemoryReference::validateImmediateOffsetAlignment(TR::Node *node, uint32_t alignment, TR::CodeGenerator *cg)
   {
   intptr_t displacement = self()->getOffset();
   if ((displacement % alignment) != 0)
      {
      TR::Compilation *comp = cg->comp();
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp, "Validating immediate offset (%d) at node %p for alignment (%d)\n", displacement, node, alignment);
         }
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

      if (_baseRegister != newBase)
         {
         self()->decNodeReferenceCounts(cg);
         _baseNode = NULL;
         self()->setBaseModifiable();
         _baseRegister = newBase;
         }
      }
   }

void OMR::ARM64::MemoryReference::normalize(TR::Node *node, TR::CodeGenerator *cg)
   {

   if ((_indexRegister != NULL) && ((self()->getOffset() != 0) || self()->hasDelayedOffset()))
      {
      /* This ensures that _indexRegister is NULL when offset is not zero */
      self()->consolidateRegisters(node, cg);
      }
   else if (_baseRegister == NULL)
      {
      self()->moveIndexToBase(node, cg);
      }

   if (self()->getUnresolvedSnippet() != NULL)
      {
      if (_indexRegister != NULL)
         {
         /* This ensures that _indexRegister is NULL */
         self()->consolidateRegisters(node, cg);
         }
      return;
      }

   intptr_t displacement = self()->getOffset();

   if (displacement != 0)
      {
      TR_ASSERT_FATAL(_indexRegister == NULL, "_indexRegister must be NULL if displacement is not zero");

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

         if (_baseRegister != newBase)
            {
            self()->decNodeReferenceCounts(cg);
            _baseNode = NULL;
            self()->setBaseModifiable();
            _baseRegister = newBase;
            }
         }
      }
   }

void OMR::ARM64::MemoryReference::addToOffset(TR::Node *node, intptr_t amount, TR::CodeGenerator *cg)
   {
   self()->setOffset(self()->getOffset() + amount);
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

int32_t OMR::ARM64::MemoryReference::getScaleForNode(TR::Node *node, TR::CodeGenerator *cg)
   {
   int32_t scale = 0;
   if (node->getOpCodeValue() == TR::ishl || node->getOpCodeValue() == TR::lshl)
      {
      if (node->getSecondChild()->getOpCode().isLoadConst())
         {
         int32_t shiftMask = (node->getOpCodeValue() == TR::lshl) ? 63 : 31;
         int32_t shiftAmount = node->getSecondChild()->getInt() & shiftMask;
         // shiftAmount of add extended instruction must be less then or equal to 4.
         // shiftAmount allowed depends on the length of loads and stores.
         if ((shiftAmount <= 4) && ((1 << shiftAmount) == _length))
            {
            scale = shiftAmount;
            }
         else
            {
            TR::Compilation *comp = cg->comp();
            if (comp->getOption(TR_TraceCG))
               {
               traceMsg(comp, "Shift amount for index register at node %p is %d which is invalid for _length = %d\n", node, shiftAmount, _length);
               }
            }
         }
      }
   return scale;
   }

static bool checkOffset(TR::Node *node, TR::CodeGenerator *cg, uint32_t offset, uint32_t length)
   {
   if ((length > 0) && ((offset & (length - 1)) == 0))
      {
      return true;
      }
   else
      {
      TR::Compilation *comp = cg->comp();
      if (comp->getOption(TR_TraceCG))
         {
         traceMsg(comp, "offset amount at node %p is %d which is invalid for length = %d\n", node, offset, length);
         }
      return false;
      }
   }

void OMR::ARM64::MemoryReference::moveIndexToBase(TR::Node *node, TR::CodeGenerator *cg)
   {
   if ((_baseRegister != NULL) ||  self()->isIndexSignExtended() || (_scale != 0))
      {
      self()->consolidateRegisters(node, cg);
      }
   else
      {
      if (self()->isIndexModifiable())
         {
         self()->setBaseModifiable();
         }
      else
         {
         self()->clearBaseModifiable();
         }
      _baseRegister = _indexRegister;
      _baseNode = _indexNode;
      _indexRegister = NULL;
      _indexNode = NULL;
      self()->clearIndexModifiable();
      }
   }

void OMR::ARM64::MemoryReference::populateMemoryReference(TR::Node *subTree, TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   bool shiftUnderAddressNode = false;
   if (comp->useCompressedPointers())
      {
      if (subTree->getOpCodeValue() == TR::l2a && subTree->getReferenceCount() == 1 && subTree->getRegister() == NULL &&
          self()->getUnresolvedSnippet() == NULL)
         {
         /*
          * We need to avoid skipping l2a node when the memory reference has a UnresolvedDataSnippet because skipping l2a
          * makes the base register non-collected reference register.
          * When a UnresolvedDataSnippet exists, the memory reference will generate multiple instructions.
          * The first instruction is the branch to the UnresolvedDataSnippet which will be patched when the resolution
          * finishes, and the last instruction is the actual load using the base register and resolved offset.
          * The branch to the resolution helper can trigger GC, but if the base register is not a collected reference
          * register, the valule of the base register will not be updated by GC.
          * This is a tactical solution for OpenJ9 issue 14663.
          */

         cg->decReferenceCount(subTree);
         subTree = subTree->getFirstChild();

         /*
          * We do not want to use shifted index register for compressed refs address node
          * because shift amount might not be supported one by the mnemonic.
          */
         shiftUnderAddressNode = subTree->getOpCode().isShift();
         }
      }

   /*
    *  If a node has been passed into populateMemoryReference but it was
    *  never set as a base or index node then its reference count needs
    *  to be explicitly decremented if it already hasn't been.
    */
   const rcount_t refCountOnEntry = subTree->getReferenceCount();
   TR::Node * const subTreeOnEntry = subTree;

   if (subTree->getReferenceCount() > 1 || subTree->getRegister() != NULL || shiftUnderAddressNode)
      {
      if (_baseRegister != NULL)
         {
         if (_indexRegister != NULL)
            {
            self()->consolidateRegisters(subTree, cg);
            }
         _indexRegister = cg->evaluate(subTree);
         _indexNode = subTree;
         }
      else
         {
         _baseRegister = cg->evaluate(subTree);
         _baseNode = subTree;
         }
      }
   else
      {
      int32_t scale = 0;
      if (subTree->getOpCode().isArrayRef())
         {
         TR::Node *addressChild = subTree->getFirstChild();
         TR::Node *integerChild = subTree->getSecondChild();

         if (subTree->getSecondChild()->getOpCode().isLoadConst())
            {
            // array access with constant index
            self()->populateMemoryReference(addressChild, cg);
            intptr_t amount = (integerChild->getOpCodeValue() == TR::iconst) ?
                              integerChild->getInt() : integerChild->getLongInt();
            self()->addToOffset(integerChild, amount, cg);
            if (comp->getOption(TR_TraceCG))
               {
               traceMsg(comp, "Capturing array access with constant index at node %p offset = %d\n", subTree, amount);
               }
            cg->decReferenceCount(integerChild);
            }
         else if (cg->whichNodeToEvaluate(addressChild, integerChild) == 1)
            {
            self()->populateMemoryReference(integerChild, cg);

            self()->populateMemoryReference(addressChild, cg);
            }
         else
            {
            self()->populateMemoryReference(addressChild, cg);

            if (_baseRegister != NULL && _indexRegister != NULL)
               {
               self()->consolidateRegisters(subTree, cg);
               }

            self()->populateMemoryReference(integerChild, cg);
            }
         cg->decReferenceCount(subTree);
         }
      else if ((subTree->getOpCodeValue() == TR::lsub) &&
               (subTree->getSecondChild()->getOpCodeValue() == TR::lconst) &&
               checkOffset(subTree->getSecondChild(), cg, -subTree->getSecondChild()->getLongInt(), _length))
         {
         TR::Node *constChild = subTree->getSecondChild();
         intptr_t amount = -constChild->getLongInt();
         self()->populateMemoryReference(subTree->getFirstChild(), cg);

         self()->addToOffset(subTree, amount, cg);
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "Capturing lsub node with constant value at node %p offset = %d\n", subTree, amount);
            }
         cg->decReferenceCount(constChild);
         cg->decReferenceCount(subTree);
         }
      else if (subTree->getOpCodeValue() == TR::i2l)
         {
         if (_indexRegister != NULL)
            {
            self()->moveIndexToBase(subTree, cg);
            }
         TR::Node *firstChild  = subTree->getFirstChild();
         _indexRegister = cg->evaluate(firstChild);
         _indexNode = firstChild;
         self()->setIndexSignExtendedWord();
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "Capturing l2i node at %p\n", subTree);
            }
         cg->decReferenceCount(subTree);
         }
      else if (subTree->getOpCodeValue() == TR::ishl &&
               subTree->getFirstChild()->getOpCode().isLoadConst() &&
               subTree->getSecondChild()->getOpCode().isLoadConst())
         {
         self()->addToOffset(subTree, subTree->getFirstChild()->getInt() <<
                     subTree->getSecondChild()->getInt(), cg);
         cg->decReferenceCount(subTree->getFirstChild());
         cg->decReferenceCount(subTree->getSecondChild());
         cg->decReferenceCount(subTree);
         }
      else if ((scale = self()->getScaleForNode(subTree, cg)) != 0)
         {
         if (_indexRegister != NULL)
            {
            self()->moveIndexToBase(subTree, cg);
            }
         TR::Node *firstChild  = subTree->getFirstChild();
         if ((firstChild->getOpCodeValue() == TR::i2l) && (firstChild->getReferenceCount() == 1) && (firstChild->getRegister() == NULL))
            {
            TR::Node *i2lChild = firstChild->getFirstChild();
            cg->evaluate(i2lChild);
            self()->setIndexSignExtendedWord();
            if (comp->getOption(TR_TraceCG))
               {
               traceMsg(comp, "Capturing i2l node at %p which is a first child of shift node %p\n", firstChild, subTree);
               }
            cg->decReferenceCount(firstChild);
            firstChild = i2lChild;
            }
         _indexRegister = cg->evaluate(firstChild);
         _indexNode = firstChild;
         if (cg->canClobberNodesRegister(firstChild))
            {
            self()->setIndexModifiable();
            }

         TR::Node *secondChild = subTree->getSecondChild();
         _scale = scale;
         if (comp->getOption(TR_TraceCG))
            {
            traceMsg(comp, "Capturing shift node at %p, scale = %d\n", subTree, _scale);
            }
         cg->decReferenceCount(secondChild);
         cg->decReferenceCount(subTree);
         }
      else if ((subTree->getOpCodeValue() == TR::loadaddr) && !comp->compileRelocatableCode())
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
            TR::Register *tempReg = (!symbol->isMethodMetaData()) ? cg->getStackPointerRegister() : cg->getMethodMetaDataRegister();

            if (_baseRegister != NULL)
               {
               if (_indexRegister != NULL)
                  {
                  self()->consolidateRegisters(NULL, cg);
                  }
               _indexRegister = tempReg;
               }
            else
               {
               _baseRegister = tempReg;
               _baseNode = NULL;
               }
            }
         self()->addToOffset(subTree, subTree->getSymbolReference()->getOffset(), cg);
         cg->decReferenceCount(subTree); // need to decrement ref count because
                                         // nodes weren't set on memoryreference
         }
      else if (subTree->getOpCodeValue() == TR::aconst ||
               subTree->getOpCodeValue() == TR::iconst ||
               subTree->getOpCodeValue() == TR::lconst)
         {
         intptr_t amount = (subTree->getOpCodeValue() == TR::iconst) ?
                             subTree->getInt() : subTree->getLongInt();
         self()->addToOffset(subTree, amount, cg);
         cg->decReferenceCount(subTree);
         }
      else
         {
         if (_baseRegister != NULL)
            {
            if (_indexRegister != NULL)
               {
               self()->consolidateRegisters(subTree, cg);
               }
            _indexRegister = cg->evaluate(subTree);
            _indexNode = subTree;
            if (cg->canClobberNodesRegister(subTree))
               {
               self()->setIndexModifiable();
               }
            }
         else
            {
            _baseRegister = cg->evaluate(subTree);
            _baseNode = subTree;
            if (cg->canClobberNodesRegister(subTree))
               {
               self()->setBaseModifiable();
               }
            }
         }
      }

      if (refCountOnEntry == subTreeOnEntry->getReferenceCount())
         {
         if ((_indexNode != subTreeOnEntry) && (_baseNode != subTreeOnEntry))
            {
            cg->decReferenceCount(subTreeOnEntry);
            }
         }
   }


void OMR::ARM64::MemoryReference::consolidateRegisters(TR::Node *srcTree, TR::CodeGenerator *cg)
   {
   TR::Register *tempTargetRegister;

   if ((_baseRegister != NULL) && self()->isBaseModifiable())
      tempTargetRegister = _baseRegister;
   else if (((_baseRegister != NULL) && (_baseRegister->containsCollectedReference() || _baseRegister->containsInternalPointer())) ||
            _indexRegister->containsCollectedReference() || _indexRegister->containsInternalPointer())
      {
      if (srcTree != NULL && srcTree->isInternalPointer() &&
            srcTree->getPinningArrayPointer())
         {
         tempTargetRegister = cg->allocateRegister();
         tempTargetRegister->setContainsInternalPointer();
         tempTargetRegister->setPinningArrayPointer(srcTree->getPinningArrayPointer());
         }
      else
         {
         tempTargetRegister = cg->allocateCollectedReferenceRegister();
         }
      }
   else
      tempTargetRegister = cg->allocateRegister();

   if (_baseRegister != NULL)
      {
      if (self()->isIndexSignExtended())
         {
         generateTrg1Src2ExtendedInstruction(cg, TR::InstOpCode::addextx, srcTree, tempTargetRegister, _baseRegister, _indexRegister, self()->getIndexExtendCode(), _scale);
         }
      else
         {
         generateTrg1Src2ShiftedInstruction(cg, TR::InstOpCode::addx, srcTree, tempTargetRegister, _baseRegister, _indexRegister, TR::SH_LSL, _scale);
         }
      }
   else if (_scale != 0)
      {
      generateLogicalShiftLeftImmInstruction(cg, srcTree, tempTargetRegister, _indexRegister, _scale, true);
      if (self()->isIndexSignExtended())
         {
         uint32_t imm = (self()->isIndexSignExtendedWord() ? 31 : (self()->isIndexSignExtendedHalf() ? 15 : 7));
         generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmx, srcTree, tempTargetRegister, tempTargetRegister, imm);
         }
      }
   else if (self()->isIndexSignExtended())
      {
      uint32_t imm = (self()->isIndexSignExtendedWord() ? 31 : (self()->isIndexSignExtendedHalf() ? 15 : 7));
      generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::sbfmx, srcTree, tempTargetRegister, _indexRegister, imm);
      }
   else
      {
      TR_ASSERT_FATAL(false, "consolidateRegister() expects (_baseRegister != NULL) || (_scale != 0) || isIndexSignExtended()");
      }

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
   _indexNode = NULL;
   _indexRegister = NULL;
   _scale = 0;
   self()->clearIndexModifiable();
   self()->clearIndexSignExtended();
   }


void OMR::ARM64::MemoryReference::bookKeepingRegisterUses(TR::Instruction *instr, TR::CodeGenerator *cg)
   {
   if (_baseRegister != NULL)
      {
      instr->useRegister(_baseRegister);
      }
   if (_indexRegister != NULL)
      {
      instr->useRegister(_indexRegister);
      }
   if (_extraRegister != NULL)
      {
      instr->useRegister(_extraRegister);
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
            if ((assignedBaseRegister = machine->findBestFreeRegister(currentInstruction, TR_GPR, true, _baseRegister)) == NULL)
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
            if ((assignedIndexRegister = machine->findBestFreeRegister(currentInstruction, TR_GPR, false, _indexRegister)) == NULL)
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
      machine->decFutureUseCountAndUnlatch(currentInstruction, _baseRegister);
      _baseRegister = assignedBaseRegister;
      }

   if (_indexRegister != NULL)
      {
      machine->decFutureUseCountAndUnlatch(currentInstruction, _indexRegister);
      _indexRegister = assignedIndexRegister;
      }
   if (self()->getUnresolvedSnippet() != NULL)
      {
      currentInstruction->ARM64NeedsGCMap(cg, 0xFFFFFFFF);
      }
   }

TR::InstOpCode::Mnemonic OMR::ARM64::MemoryReference::mapOpCode(TR::InstOpCode::Mnemonic mnemonic)
   {
   if (self()->getIndexRegister() != NULL)
      {
      switch (mnemonic)
         {
         case TR::InstOpCode::ldrbimm:
            return TR::InstOpCode::ldrboff;
         case TR::InstOpCode::ldrsbimmw:
            return TR::InstOpCode::ldrsboffw;
         case TR::InstOpCode::ldrsbimmx:
            return TR::InstOpCode::ldrsboffx;
         case TR::InstOpCode::ldrhimm:
            return TR::InstOpCode::ldrhoff;
         case TR::InstOpCode::ldrshimmw:
            return TR::InstOpCode::ldrshoffw;
         case TR::InstOpCode::ldrshimmx:
            return TR::InstOpCode::ldrshoffx;
         case TR::InstOpCode::ldrimmw:
            return TR::InstOpCode::ldroffw;
         case TR::InstOpCode::ldrswimm:
            return TR::InstOpCode::ldrswoff;
         case TR::InstOpCode::ldrimmx:
            return TR::InstOpCode::ldroffx;
         case TR::InstOpCode::vldrimmb:
            return TR::InstOpCode::vldroffb;
         case TR::InstOpCode::vldrimmh:
            return TR::InstOpCode::vldroffh;
         case TR::InstOpCode::vldrimms:
            return TR::InstOpCode::vldroffs;
         case TR::InstOpCode::vldrimmd:
            return TR::InstOpCode::vldroffd;
         case TR::InstOpCode::vldrimmq:
            return TR::InstOpCode::vldroffq;
         case TR::InstOpCode::strbimm:
            return TR::InstOpCode::strboff;
         case TR::InstOpCode::strhimm:
            return TR::InstOpCode::strhoff;
         case TR::InstOpCode::strimmw:
            return TR::InstOpCode::stroffw;
         case TR::InstOpCode::strimmx:
            return TR::InstOpCode::stroffx;
         case TR::InstOpCode::vstrimmb:
            return TR::InstOpCode::vstroffb;
         case TR::InstOpCode::vstrimmh:
            return TR::InstOpCode::vstroffh;
         case TR::InstOpCode::vstrimms:
            return TR::InstOpCode::vstroffs;
         case TR::InstOpCode::vstrimmd:
            return TR::InstOpCode::vstroffd;
         case TR::InstOpCode::vstrimmq:
            return TR::InstOpCode::vstroffq;
         default:
            break;
         }
      }
      return mnemonic;
   }

/**
 * @brief Answers whether the scale is valid for the mnemonic
 *
 * @param[in] scale: scale applied for index register
 * @param[in] mnemonic: mnemonic
 * @return true if the scale is valid
 */
static bool isValidScale(uint8_t scale, TR::InstOpCode::Mnemonic mnemonic)
   {
   return (((scale == 1) && ((mnemonic == TR::InstOpCode::ldrhoff) || (mnemonic == TR::InstOpCode::ldrshoffw) ||
                             (mnemonic == TR::InstOpCode::ldrshoffx) || (mnemonic == TR::InstOpCode::strhoff) ||
                             (mnemonic == TR::InstOpCode::vldroffh) || (mnemonic == TR::InstOpCode::vstroffh))) ||
           ((scale == 2) && ((mnemonic == TR::InstOpCode::ldroffw) || (mnemonic == TR::InstOpCode::ldrswoff) ||
                             (mnemonic == TR::InstOpCode::stroffw) ||
                             (mnemonic == TR::InstOpCode::vldroffs) || (mnemonic == TR::InstOpCode::vstroffs))) ||
           ((scale == 3) && ((mnemonic == TR::InstOpCode::ldroffx) || (mnemonic == TR::InstOpCode::stroffx) ||
                             (mnemonic == TR::InstOpCode::vldroffd) || (mnemonic == TR::InstOpCode::vstroffd))) ||
           ((scale == 4) && ((mnemonic == TR::InstOpCode::vldroffq) || (mnemonic == TR::InstOpCode::vstroffq))));
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

/* unscaled immediate offset */
static bool isImm9UnscaledOffsetInstruction(uint32_t enc)
   {
   return ((enc & 0x3b200C00) == 0x38000000);
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
/* stp/ldp FPR */
static bool isImm7OffsetFPRInstruction(uint32_t enc)
   {
   return ((enc & 0x3e000000) == 0x2C000000);
   }
/* load/store exclusive */
static bool isExclusiveMemAccessInstruction(uint32_t enc)
   {
   return ((enc & 0x3f000000) == 0x08000000);
   }

/* atomic operation */
static bool isAtomicOperationInstruction(uint32_t enc)
   {
   return ((enc & 0x3b200c00) == 0x38200000);
   }

/* vector memory access: vstrimmq or vldrimmq */
static bool isImm12VectorMemoryAccess(uint32_t enc)
   {
   return ((enc & 0xffb00000) == 0x3d800000);
   }

static TR::InstOpCode::Mnemonic getEquivalentRegisterOffsetMnemonic(TR::InstOpCode::Mnemonic op)
   {
   switch (op)
      {
      case TR::InstOpCode::ldurb:
      case TR::InstOpCode::ldrbimm:
         return TR::InstOpCode::ldrboff;
      case TR::InstOpCode::ldursbw:
      case TR::InstOpCode::ldrsbimmw:
         return TR::InstOpCode::ldrsboffw;
      case TR::InstOpCode::ldursbx:
      case TR::InstOpCode::ldrsbimmx:
         return TR::InstOpCode::ldrsboffx;
      case TR::InstOpCode::ldurh:
      case TR::InstOpCode::ldrhimm:
         return TR::InstOpCode::ldrhoff;
      case TR::InstOpCode::ldurshw:
      case TR::InstOpCode::ldrshimmw:
         return TR::InstOpCode::ldrshoffw;
      case TR::InstOpCode::ldurshx:
      case TR::InstOpCode::ldrshimmx:
         return TR::InstOpCode::ldrshoffx;
      case TR::InstOpCode::ldurw:
      case TR::InstOpCode::ldrimmw:
         return TR::InstOpCode::ldroffw;
      case TR::InstOpCode::ldurx:
      case TR::InstOpCode::ldrimmx:
         return TR::InstOpCode::ldroffx;
      case TR::InstOpCode::vldurb:
      case TR::InstOpCode::vldrimmb:
         return TR::InstOpCode::vldroffb;
      case TR::InstOpCode::vldurh:
      case TR::InstOpCode::vldrimmh:
         return TR::InstOpCode::vldroffh;
      case TR::InstOpCode::vldurs:
      case TR::InstOpCode::vldrimms:
         return TR::InstOpCode::vldroffs;
      case TR::InstOpCode::vldurd:
      case TR::InstOpCode::vldrimmd:
         return TR::InstOpCode::vldroffd;
      case TR::InstOpCode::vldurq:
      case TR::InstOpCode::vldrimmq:
         return TR::InstOpCode::vldroffq;

      case TR::InstOpCode::sturb:
      case TR::InstOpCode::strbimm:
         return TR::InstOpCode::strboff;
      case TR::InstOpCode::sturh:
      case TR::InstOpCode::strhimm:
         return TR::InstOpCode::strhoff;
      case TR::InstOpCode::sturw:
      case TR::InstOpCode::strimmw:
         return TR::InstOpCode::stroffw;
      case TR::InstOpCode::sturx:
      case TR::InstOpCode::strimmx:
         return TR::InstOpCode::stroffx;
      case TR::InstOpCode::vsturb:
      case TR::InstOpCode::vstrimmb:
         return TR::InstOpCode::vstroffb;
      case TR::InstOpCode::vsturh:
      case TR::InstOpCode::vstrimmh:
         return TR::InstOpCode::vstroffh;
      case TR::InstOpCode::vsturs:
      case TR::InstOpCode::vstrimms:
         return TR::InstOpCode::vstroffs;
      case TR::InstOpCode::vsturd:
      case TR::InstOpCode::vstrimmd:
         return TR::InstOpCode::vstroffd;
      case TR::InstOpCode::vsturq:
      case TR::InstOpCode::vstrimmq:
         return TR::InstOpCode::vstroffq;
      default:
         return TR::InstOpCode::bad;
      }
   }

static TR::InstOpCode::Mnemonic getEquivalentUnscaledOffsetMnemonic(TR::InstOpCode::Mnemonic op)
   {
   switch (op)
      {
      case TR::InstOpCode::ldrbimm:
         return TR::InstOpCode::ldurb;
      case TR::InstOpCode::ldrsbimmw:
         return TR::InstOpCode::ldursbw;
      case TR::InstOpCode::ldrsbimmx:
         return TR::InstOpCode::ldursbx;
      case TR::InstOpCode::ldrhimm:
         return TR::InstOpCode::ldurh;
      case TR::InstOpCode::ldrshimmw:
         return TR::InstOpCode::ldurshw;
      case TR::InstOpCode::ldrshimmx:
         return TR::InstOpCode::ldurshx;
      case TR::InstOpCode::ldrimmw:
         return TR::InstOpCode::ldurw;
      case TR::InstOpCode::ldrimmx:
         return TR::InstOpCode::ldurx;
      case TR::InstOpCode::vldrimmb:
         return TR::InstOpCode::vldurb;
      case TR::InstOpCode::vldrimmh:
         return TR::InstOpCode::vldurh;
      case TR::InstOpCode::vldrimms:
         return TR::InstOpCode::vldurs;
      case TR::InstOpCode::vldrimmd:
         return TR::InstOpCode::vldurd;
      case TR::InstOpCode::vldrimmq:
         return TR::InstOpCode::vldurq;

      case TR::InstOpCode::strbimm:
         return TR::InstOpCode::sturb;
      case TR::InstOpCode::strhimm:
         return TR::InstOpCode::sturh;
      case TR::InstOpCode::strimmw:
         return TR::InstOpCode::sturw;
      case TR::InstOpCode::strimmx:
         return TR::InstOpCode::sturx;
      case TR::InstOpCode::vstrimmh:
         return TR::InstOpCode::vsturh;
      case TR::InstOpCode::vstrimms:
         return TR::InstOpCode::vsturs;
      case TR::InstOpCode::vstrimmd:
         return TR::InstOpCode::vsturd;
      case TR::InstOpCode::vstrimmq:
         return TR::InstOpCode::vsturq;
      default:
         return TR::InstOpCode::bad;
      }
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
      TR_ASSERT_FATAL(self()->isDelayedOffsetDone(), "delayed offset must be done before generateBinaryEncoding");
      /* delayed offset has been already applied to internal offset */
      int32_t displacement = self()->getOffset(false);
      TR_ASSERT_FATAL(!((base != NULL) && (index != NULL) && (displacement != 0)), "AArch64 does not support [base + index + offset] form of memory access");

      TR::InstOpCode op = currentInstruction->getOpCode();

      if (op.getMnemonic() != TR::InstOpCode::addimmx)
         {
         // load/store instruction
         uint32_t enc = (uint32_t)op.getOpCodeBinaryEncoding();

         if (index)
            {
            TR_ASSERT(displacement == 0, "Non-zero offset with index register.");
            TR_ASSERT_FATAL(!(self()->isIndexSignExtendedByte() || self()->isIndexSignExtendedHalf()), "Extend code for memory access must not be SXTB or SXTH.");

            if (isRegisterOffsetInstruction(enc))
               {
               base->setRegisterFieldRN(wcursor);
               index->setRegisterFieldRM(wcursor);

               if (self()->isIndexSignExtendedWord())
                  {
                  // SXTW
                  *wcursor |= 0x6 << 13;
                  }
               else
                  {
                  // LSL
                  *wcursor |= 0x3 << 13;
                  }
               uint8_t scale = self()->getScale();
               if (scale != 0)
                  {
                  TR_ASSERT_FATAL(isValidScale(scale, op.getMnemonic()), "Invalid scale value %d for mnemonic", scale);
                  // set scale bit
                  *wcursor |= (1 << 12);
                  }
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
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isImm12OffsetInstruction(enc))
               {
               uint32_t size = (enc >> 30) & 3; /* b=0, h=1, w=2, x=3, q=0 */
               uint32_t bitsToShift = isImm12VectorMemoryAccess(enc) ? 4 : size;
               uint32_t shifted = displacement >> bitsToShift;

               if (size > 0)
                  {
                  TR_ASSERT((displacement & ((1 << size) - 1)) == 0, "Non-aligned offset in 2/4/8-byte memory access.");
                  }

               if (constantIsUnsignedImm12(shifted))
                  {
                  *wcursor |= (shifted & 0xfff) << 10; /* imm12 */
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
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
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isImm7OffsetFPRInstruction(enc))
               {
               uint32_t opc = ((enc >> 30) & 3); /* 32bit: 00, 64bit: 01, 128bit: 10 */
               uint32_t size = opc + 2;
               uint32_t shifted = displacement >> size;

               TR_ASSERT_FATAL((displacement & ((1 << size) - 1)) == 0, "displacement must be 4/8/16-byte alligned");

               if (constantIsImm7(shifted))
                  {
                  *wcursor |= (shifted & 0x7f) << 15; /* imm7 */
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (!(isExclusiveMemAccessInstruction(enc) || isAtomicOperationInstruction(enc)))
               {
               TR_ASSERT_FATAL(false, "enc = 0x%x", enc);

               /* Register pair, literal instructions to be supported */
               TR_UNIMPLEMENTED();
               }
            }
            cursor += ARM64_INSTRUCTION_LENGTH;
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

TR::Instruction *OMR::ARM64::MemoryReference::expandInstruction(TR::Instruction *currentInstruction, TR::CodeGenerator *cg)
   {
   // Due to the way the generate*Instruction helpers work, there's no way to use them to generate an instruction at the
   // start of the instruction stream at the moment. As a result, we cannot peform expansion if the first instruction is
   // a memory instruction.
   TR_ASSERT_FATAL(currentInstruction->getPrev(), "The first instruction cannot be a memory instruction");

   int32_t displacement = self()->getOffset(true);
   /* Apply delayed offset to internal offset. */
   self()->setOffset(displacement);
   self()->setDelayedOffsetDone();

   if (self()->getUnresolvedSnippet() == NULL)
      {
      TR::Compilation *comp = cg->comp();
      TR_Debug *debugObj = cg->getDebug();

      TR::InstOpCode op = currentInstruction->getOpCode();
      if (op.getMnemonic() != TR::InstOpCode::addimmx)
         {
         // load/store instruction
         if (self()->getIndexRegister())
            {
            return currentInstruction;
            }
         else
            {
            /* no index register */
            uint32_t enc = (uint32_t)op.getOpCodeBinaryEncoding();

            if (isImm9OffsetInstruction(enc))
               {
               if (constantIsImm9(displacement))
                  {
                  return currentInstruction;
                  }
               else
                  {
                  if (isImm9UnscaledOffsetInstruction(enc))
                     {
                     if (isBaseModifiable() && constantIsUnsignedImm12(displacement))
                        {
                        TR::Instruction *prev = currentInstruction->getPrev();
                        generateTrg1Src1ImmInstruction(cg, TR::InstOpCode::addimmw, currentInstruction->getNode(), self()->getBaseRegister(), self()->getBaseRegister(), displacement, prev);
                        self()->setOffset(0);
                        return currentInstruction;
                        }
                     else
                        {
                        /* X16 and X17 are IP0 and IP1, intra-procedure-call temporary registers, which are used for trampolines. */
                        TR::RealRegister *x16 = cg->machine()->getRealRegister(TR::RealRegister::x16);
                        TR::Instruction *prev = currentInstruction->getPrev();
                        TR::Instruction *tmp = loadConstant32(cg, currentInstruction->getNode(), displacement, x16, prev);
                        TR::InstOpCode::Mnemonic newOp = getEquivalentRegisterOffsetMnemonic(op.getMnemonic());

                        if (comp->getOption(TR_TraceCG) && debugObj)
                           {
                           TR::InstOpCode newOpCode(newOp);
                           traceMsg(comp, "Replacing opcode of instruction %p from %s to %s\n", currentInstruction, debugObj->getOpCodeName(&op), debugObj->getOpCodeName(&newOpCode));
                           }
                        currentInstruction->setOpCodeValue(newOp);
                        self()->setIndexRegister(x16);
                        self()->setOffset(0);
                        return currentInstruction;
                        }
                     }
                  else
                     {
                     /* Giving up for pre-index or post-index instructions */
                     TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                     }
                  }
               }
            else if (isImm12OffsetInstruction(enc))
               {
               uint32_t size = (enc >> 30) & 3; /* b=0, h=1, w=2, x=3, q=0 */
               uint32_t bitsToShift = isImm12VectorMemoryAccess(enc) ? 4 : size;
               uint32_t shifted = displacement >> bitsToShift;

               if (size > 0)
                  {
                  TR_ASSERT((displacement & ((1 << size) - 1)) == 0, "Non-aligned offset in 2/4/8-byte memory access.");
                  }

               if (constantIsUnsignedImm12(shifted))
                  {
                  return currentInstruction;
                  }
               else
                  {
                  if (displacement < 0 && constantIsImm9(displacement))
                     {
                     /* rewrite the instruction ldrimm -> ldur  */
                     TR::InstOpCode::Mnemonic newOp = getEquivalentUnscaledOffsetMnemonic(op.getMnemonic());

                     if (comp->getOption(TR_TraceCG) && debugObj)
                        {
                        TR::InstOpCode newOpCode(newOp);
                        traceMsg(comp, "Replacing opcode of instruction %p from %s to %s\n", currentInstruction, debugObj->getOpCodeName(&op), debugObj->getOpCodeName(&newOpCode));
                        }
                     currentInstruction->setOpCodeValue(newOp);
                     return currentInstruction;
                     }
                  else
                     {
                     /* X16 and X17 are IP0 and IP1, intra-procedure-call temporary registers, which are used for trampolines. */
                     TR::RealRegister *x16 = cg->machine()->getRealRegister(TR::RealRegister::x16);
                     TR::Instruction *prev = currentInstruction->getPrev();
                     TR::Instruction *tmp = loadConstant32(cg, currentInstruction->getNode(), displacement, x16, prev);
                     TR::InstOpCode::Mnemonic newOp = getEquivalentRegisterOffsetMnemonic(op.getMnemonic());

                     if (comp->getOption(TR_TraceCG) && debugObj)
                        {
                        TR::InstOpCode newOpCode(newOp);
                        traceMsg(comp, "Replacing opcode of instruction %p from %s to %s\n", currentInstruction, debugObj->getOpCodeName(&op), debugObj->getOpCodeName(&newOpCode));
                        }
                     currentInstruction->setOpCodeValue(newOp);
                     self()->setIndexRegister(x16);
                     self()->setOffset(0);
                     return currentInstruction;
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
                  return currentInstruction;
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isImm7OffsetFPRInstruction(enc))
               {
               uint32_t opc = ((enc >> 30) & 3); /* 32bit: 00, 64bit: 01, 128bit: 10 */
               uint32_t size = opc + 2;
               uint32_t shifted = displacement >> size;

               TR_ASSERT_FATAL((displacement & ((1 << size) - 1)) == 0, "displacement must be 4/8/16-byte alligned");

               if (constantIsImm7(shifted))
                  {
                  return currentInstruction;
                  }
               else
                  {
                  TR_ASSERT_FATAL(false, "Offset is too large for specified instruction.");
                  }
               }
            else if (isExclusiveMemAccessInstruction(enc))
               {
               TR_ASSERT_FATAL_WITH_NODE(currentInstruction->getNode(), displacement == 0, "displacement must be zero for load/store exclusive instructions");
               return currentInstruction;
               }
            else if (isAtomicOperationInstruction(enc))
               {
               TR_ASSERT_FATAL_WITH_NODE(currentInstruction->getNode(), displacement == 0, "displacement must be zero for atomic instructions");
               return currentInstruction;
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
         // TODO: addimmx instruction
         return currentInstruction;
         }
      }

   return currentInstruction;
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
         return ARM64_INSTRUCTION_LENGTH;
         }
      else
         {
         // addimmx instruction
         TR_ASSERT(self()->getIndexRegister() == NULL, "MemoryReference with unexpected indexed form");

         int32_t displacement = self()->getOffset(false);
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
