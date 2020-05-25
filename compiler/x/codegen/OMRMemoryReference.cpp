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
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/Machine.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"
#include "codegen/Register.hpp"
#include "codegen/RegisterConstants.hpp"
#include "codegen/Relocation.hpp"
#include "codegen/UnresolvedDataSnippet.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/RegisterMappedSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "ras/Debug.hpp"
#include "runtime/Runtime.hpp"
#include "x/codegen/DataSnippet.hpp"
#include "x/codegen/X86Instruction.hpp"
#include "x/codegen/X86Ops.hpp"

class TR_OpaqueClassBlock;
class TR_ScratchRegisterManager;
namespace TR { class LabelSymbol; }
namespace TR { class MethodSymbol; }

static void rematerializeAddressAdds(TR::Node *rootLoadOrStore, TR::CodeGenerator *cg);

intptr_t
OMR::X86::MemoryReference::getDisplacement()
   {
   TR::SymbolReference &symRef = self()->getSymbolReference();
   intptr_t  displacement = symRef.getOffset();
   TR::Symbol *symbol = symRef.getSymbol();

   if (symbol != NULL)
      {
      if (symbol->isRegisterMappedSymbol())
         {
         TR_ASSERT(!symRef.isUnresolved(), "Whoops, a register mapped symbol is unresolved");
         displacement += symbol->castToRegisterMappedSymbol()->getOffset();
         }
      else if (!symRef.isUnresolved() && symbol->isStatic())
         {
         displacement += (uintptr_t)symbol->castToStaticSymbol()->getStaticAddress();
         }
      }

   return displacement;
   }


OMR::X86::MemoryReference::MemoryReference(TR::X86DataSnippet *cds, TR::CodeGenerator   *cg):
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _dataSnippet(cds),
   _label(NULL),
   _symbolReference(cg->comp()->getSymRefTab()),
   _stride(0),
   _flags(0),
   _reloKind(-1)
   {
   self()->setForceWideDisplacement();
   }

OMR::X86::MemoryReference::MemoryReference(TR::LabelSymbol *label, TR::CodeGenerator *cg):
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _dataSnippet(NULL),
   _label(label),
   _symbolReference(cg->comp()->getSymRefTab()),
   _stride(0),
   _flags(0),
   _reloKind(-1)
   {
   self()->setForceWideDisplacement();
   }

OMR::X86::MemoryReference::MemoryReference(TR::SymbolReference *symRef, TR::CodeGenerator *cg):
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _dataSnippet(NULL),
   _label(NULL),
   _symbolReference(cg->comp()->getSymRefTab()),
   _stride(0),
   _flags(0),
   _reloKind(-1)
   {
   self()->initialize(symRef, cg);
   }

OMR::X86::MemoryReference::MemoryReference(TR::SymbolReference *symRef, intptr_t displacement, TR::CodeGenerator *cg):
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _dataSnippet(NULL),
   _label(NULL),
   _symbolReference(cg->comp()->getSymRefTab()),
   _stride(0),
   _flags(0),
   _reloKind(-1)
   {
   self()->initialize(symRef, cg);
   _symbolReference.addToOffset(displacement);
   }

// Constructor called to generate a memory reference for all kinds of
// stores and non-constant (or address) loads.
//
OMR::X86::MemoryReference::MemoryReference(
      TR::Node *rootLoadOrStore,
      TR::CodeGenerator *cg,
      bool canRematerializeAddressAdds) :
   _baseRegister(NULL),
   _baseNode(NULL),
   _indexRegister(NULL),
   _indexNode(NULL),
   _dataSnippet(NULL),
   _label(NULL),
   _symbolReference(cg->comp()->getSymRefTab()),
   _stride(0),
   _flags(0),
   _reloKind(-1)
   {
   TR::SymbolReference *symRef = rootLoadOrStore->getSymbolReference();
   TR::Compilation *comp = cg->comp();

   if (symRef)
      {
      TR::Symbol *symbol = symRef->getSymbol();

      bool isStore = rootLoadOrStore->getOpCode().isStore();
      bool isUnresolved = symRef->isUnresolved();

      _symbolReference.setSymbol(symbol);
      _symbolReference.addToOffset(symRef->getOffset());
      _symbolReference.setOwningMethodIndex(symRef->getOwningMethodIndex());
      _symbolReference.setCPIndex(symRef->getCPIndex());
      _symbolReference.copyFlags(symRef);
      _symbolReference.copyRefNumIfPossible(symRef, comp->getSymRefTab());

      if (rootLoadOrStore->getOpCode().isIndirect())
         {
         // Special case an indirect load or store off a local object. This
         // can be treated as a direct load or store off the frame pointer
         // We can't do this when the access is unresolved.
         //
         TR::Node *base = rootLoadOrStore->getFirstChild();
         if (!isUnresolved &&
             base->getOpCodeValue() == TR::loadaddr &&
             base->getSymbol()->isLocalObject())
            {
            _baseRegister = cg->getFrameRegister();
            _symbolReference.setSymbol(base->getSymbol());
            _symbolReference.copyFlags(base->getSymbolReference());
            _baseNode = base;
            }
         else
            {
            if (isUnresolved)
               {
               // If it is an unresolved reference to a field of a local object
               // then force the localobject address to be evaluated into a register
               // otherwise the resolution may not work properly if the computation
               // is folded away into an esp + offset computation
               //
               if (base->getOpCodeValue() == TR::loadaddr && base->getSymbol()->isLocalObject())
                  cg->evaluate(base);

               self()->setUnresolvedDataSnippet(TR::UnresolvedDataSnippet::create(cg, rootLoadOrStore, &_symbolReference, isStore, symRef->canCauseGC()));
               cg->addSnippet(self()->getUnresolvedDataSnippet());
               }

            if (!debug("noAddressAddRemat") && canRematerializeAddressAdds)
               {
               rematerializeAddressAdds(rootLoadOrStore, cg);
               base = rootLoadOrStore->getFirstChild(); // It may have changed
               }

            if (symbol->isMethodMetaData())
               {
               _baseRegister = cg->getMethodMetaDataRegister();
               }

            rcount_t refCount = base->getReferenceCount();
            self()->populateMemoryReference(base, cg);
            self()->checkAndDecReferenceCount(base, refCount, cg);
            }
         }
      else
         {
         if (symbol->isStatic())
            {
            if (isUnresolved)
               {
               self()->setUnresolvedDataSnippet(TR::UnresolvedDataSnippet::create(cg, rootLoadOrStore, &_symbolReference, isStore, symRef->canCauseGC()));
               cg->addSnippet(self()->getUnresolvedDataSnippet());
               }
            _baseNode = rootLoadOrStore;
            }
         else
            {
            if (!symbol->isMethodMetaData())
               {
               // Must be either auto or parm or error
               //
               _baseRegister = cg->getFrameRegister();
               }
            else
               {
               _baseRegister = cg->getMethodMetaDataRegister();
               }
            _baseNode = NULL;
            }
         }

      if (isUnresolved)
         {
         // Need a wide field because we don't know how big the displacement
         // may turn out to be once resolved.
         //
         self()->setForceWideDisplacement();
         }

      }
   else
      {
      diagnostic("OMR::X86::MemoryReference::OMR::X86::MemoryReference() ==> no symbol reference");
      }

   // TODO: aliasing sets?

   }


TR::UnresolvedDataSnippet *
OMR::X86::MemoryReference::getUnresolvedDataSnippet()
   {
   return self()->hasUnresolvedDataSnippet() ? (TR::UnresolvedDataSnippet *)_dataSnippet : NULL;
   }

TR::UnresolvedDataSnippet *
OMR::X86::MemoryReference::setUnresolvedDataSnippet(TR::UnresolvedDataSnippet *s)
   {
   self()->setHasUnresolvedDataSnippet();
   return ( (TR::UnresolvedDataSnippet *) (_dataSnippet = s) );
   }

TR::X86DataSnippet*
OMR::X86::MemoryReference::getDataSnippet()
   {
   return (self()->hasUnresolvedDataSnippet() || self()->hasUnresolvedVirtualCallSnippet()) ? NULL : (TR::X86DataSnippet*)_dataSnippet;
   }


void
OMR::X86::MemoryReference::initialize(
      TR::SymbolReference *symRef,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   TR::Symbol *symbol = symRef->getSymbol();

   if (symbol->isMethodMetaData())
      {
      _baseRegister = cg->getMethodMetaDataRegister();
      }
   else if (symbol->isRegisterMappedSymbol())
      {
      // Must be either auto or parm or error
      //
      _baseRegister = cg->getFrameRegister();
      }

   _indexRegister = NULL;
   _symbolReference.setSymbol(symbol);
   _symbolReference.addToOffset(symRef->getOffset());
   _symbolReference.setOwningMethodIndex(symRef->getOwningMethodIndex());
   _symbolReference.setCPIndex(symRef->getCPIndex());
   _symbolReference.copyFlags(symRef);
   _symbolReference.copyRefNumIfPossible(symRef, comp->getSymRefTab());

   if (symRef->isUnresolved())
      {
      self()->setUnresolvedDataSnippet(TR::UnresolvedDataSnippet::create(cg, 0, &_symbolReference, false, symRef->canCauseGC()));
      cg->addSnippet(self()->getUnresolvedDataSnippet());
      self()->setForceWideDisplacement();
      }
   // TODO: aliasing sets?

   }


OMR::X86::MemoryReference::MemoryReference(
      TR::MemoryReference& mr,
      intptr_t n,
      TR::CodeGenerator *cg,
      TR_ScratchRegisterManager *srm) :
   _symbolReference(cg->comp()->getSymRefTab())
   {
   TR::Compilation *comp = cg->comp();
   _baseRegister = mr._baseRegister;
   _baseNode = mr._baseNode;
   _indexRegister = mr._indexRegister;
   _indexNode = mr._indexNode;
   _label = mr._label;
   _symbolReference = TR::SymbolReference(comp->getSymRefTab(), mr._symbolReference, n);
   _reloKind = -1;

   if (mr.getUnresolvedDataSnippet() != NULL)
      {
      _dataSnippet = TR::UnresolvedDataSnippet::create(cg, _baseNode, &_symbolReference, false, _symbolReference.canCauseGC());
      cg->addSnippet(_dataSnippet);
      }
   else if (mr.getDataSnippet() != NULL)
      {
      _dataSnippet = mr.getDataSnippet();
      }
   else
      {
      _dataSnippet = NULL;
      }
   _stride = mr._stride;
   _flags = mr._flags;
   }


TR::Register *
OMR::X86::MemoryReference::getNextRegister(TR::Register *cur)
   {
   if (cur == NULL)
      {
      return (_baseRegister != NULL) ? _baseRegister : _indexRegister;
      }

   if (cur == _baseRegister)
      {
      return _indexRegister;
      }

   return NULL;
   }


uint8_t
OMR::X86::MemoryReference::setStrideFromMultiplier(uint8_t s)
   {
   return (_stride = TR::MemoryReference::convertMultiplierToStride(s));
   }


// If a node has been passed into populateMemoryReference but it was
// never set as a base or index node then its reference count needs
// to be explicitly decremented if it already hasn't been.
//
void
OMR::X86::MemoryReference::checkAndDecReferenceCount(
   TR::Node *node,
   rcount_t refCount,
   TR::CodeGenerator *cg)
   {
   if (refCount == node->getReferenceCount())
      {
      if (_indexNode != node && _baseNode != node)
         {
         cg->decReferenceCount(node);
         }
      }
   }


void
OMR::X86::MemoryReference::decNodeReferenceCounts(TR::CodeGenerator *cg)
   {
   TR::Register *vmThreadReg = cg->getVMThreadRegister();

   if (_baseRegister != NULL)
      {
      if (_baseNode != NULL)
         cg->decReferenceCount(_baseNode);
      else if (_baseRegister != vmThreadReg)
         cg->stopUsingRegister(_baseRegister);
      }

   if (_indexRegister != NULL)
      {
      if (_indexNode != NULL)
         cg->decReferenceCount(_indexNode);
      else if (_indexRegister != vmThreadReg)
         cg->stopUsingRegister(_indexRegister);
      }
   }


void
OMR::X86::MemoryReference::stopUsingRegisters(TR::CodeGenerator *cg)
   {
   TR::Register *vmThreadReg = cg->getVMThreadRegister();
   if (_baseRegister != NULL)
      {
      if (_baseRegister != vmThreadReg)
         cg->stopUsingRegister(_baseRegister);
      }
   if (_indexRegister != NULL)
      {
      if (_indexRegister != vmThreadReg)
         cg->stopUsingRegister(_indexRegister);
      }
   }


void
OMR::X86::MemoryReference::useRegisters(
      TR::Instruction *instr,
      TR::CodeGenerator *cg)
   {
   if (_baseRegister != NULL)
      {
      instr->useRegister(_baseRegister);
      }
   if (_indexRegister != NULL)
      {
      instr->useRegister(_indexRegister);
      }
   }


// supports an aladd tree
int32_t
OMR::X86::MemoryReference::getStrideForNode(
      TR::Node *node,
      TR::CodeGenerator *cg)
   {
   int32_t stride = 0;
   if (node->getOpCodeValue() == TR::imul || node->getOpCodeValue() == TR::lmul)
      {
      if (node->getSecondChild()->getOpCode().isLoadConst())
         {
         int32_t multiplier;
         if (cg->comp()->target().is64Bit())
            multiplier = (int32_t)node->getSecondChild()->getLongInt();
         else
            multiplier = node->getSecondChild()->getInt();
         if (multiplier > 0 && multiplier <= HIGHEST_STRIDE_MULTIPLIER)
            stride = _multiplierToStrideMap[multiplier];
         }
      }
   else if (node->getOpCodeValue() == TR::ishl || node->getOpCodeValue() == TR::lshl)
      {
      if (node->getSecondChild()->getOpCode().isLoadConst())
         {
         int32_t shiftMask = (node->getOpCodeValue() == TR::lshl) ? 63 : 31;
         int32_t shiftAmount = node->getSecondChild()->getInt() & shiftMask;
         if (shiftAmount <= HIGHEST_STRIDE_SHIFT)
            stride = shiftAmount;
         }
      }
   return stride;
   }


void
OMR::X86::MemoryReference::populateMemoryReference(
      TR::Node *subTree,
      TR::CodeGenerator *cg, TR::Node *parent)
   {
   TR::Node *nodeToBeAdjusted = NULL;
   TR::Compilation *comp = cg->comp();

   if (comp->useCompressedPointers())
       {
       if ((subTree->getOpCodeValue() == TR::l2a) && (subTree->getReferenceCount() == 1) &&
             (subTree->getRegister() == NULL))
          {
          cg->decReferenceCount(subTree);
          subTree = subTree->getFirstChild();
          if (subTree->getRegister() == NULL)
             nodeToBeAdjusted = subTree;
          }
       }

   bool evalSubTree = true;
   if (subTree->getOpCodeValue() == TR::loadaddr &&
       subTree->getSymbolReference()->getSymbol()->isMethodMetaData())
      evalSubTree = false;

   // This condition was previously formatted in a misleading way given the
   //  conditional being evaluated. I believe the condition is correct (rather,
   //  I have no strong evidence that it is incorrect other than the formatting)
   //  so I added parentheses to eliminate a compiler warning and reformatted
   if ((evalSubTree && subTree->getReferenceCount() > 1)
       || (subTree->getRegister() != NULL)
       || (self()->inUpcastingMode() && !subTree->cannotOverflow()))
      {
      if (_baseRegister != NULL)
         {
         if (_indexRegister != NULL)
            {
            self()->consolidateRegisters(subTree, cg);
            }
         _indexRegister = self()->evaluate(subTree, cg, parent);
         _indexNode     = subTree;
         }
      else
         {
         _baseRegister  = self()->evaluate(subTree, cg, parent);
         _baseNode      = subTree;
         }
      }
   else
      {
      int32_t stride;
      TR::ILOpCodes subTreeOp = subTree->getOpCodeValue();

      if (subTree->getOpCode().isArrayRef() || (subTreeOp == TR::iadd || subTreeOp == TR::ladd))
         {
         TR::Node *addressChild = subTree->getFirstChild();
         TR::Node *integerChild = subTree->getSecondChild();

         if (integerChild->getOpCode().isLoadConst())
            {
            rcount_t refCount = addressChild->getReferenceCount();
            self()->populateMemoryReference(addressChild, cg);
            self()->checkAndDecReferenceCount(addressChild, refCount, cg);
            _symbolReference.addToOffset(TR::TreeEvaluator::integerConstNodeValue(integerChild, cg));
            cg->decReferenceCount(integerChild);
            }
         else if (cg->whichNodeToEvaluate(addressChild, integerChild) == 1)
            {
            rcount_t refCount = integerChild->getReferenceCount();
            self()->populateMemoryReference(integerChild, cg);
            self()->checkAndDecReferenceCount(integerChild, refCount, cg);

            refCount = addressChild->getReferenceCount();
            self()->populateMemoryReference(addressChild, cg);
            self()->checkAndDecReferenceCount(addressChild, refCount, cg);
            }
         else
            {
            rcount_t refCount = addressChild->getReferenceCount();
            self()->populateMemoryReference(addressChild, cg);
            self()->checkAndDecReferenceCount(addressChild, refCount, cg);

            if (_baseRegister != NULL && _indexRegister != NULL)
               {
               self()->consolidateRegisters(subTree, cg);
               }

            refCount = integerChild->getReferenceCount();
            self()->populateMemoryReference(integerChild, cg);
            self()->checkAndDecReferenceCount(integerChild, refCount, cg);
            }
         }
      else if ((subTreeOp == TR::isub || subTreeOp == TR::lsub) &&
               (subTree->getSecondChild()->getOpCodeValue() == TR::iconst ||
               subTree->getSecondChild()->getOpCodeValue() == TR::lconst))
         {
         TR::Node *constChild = subTree->getSecondChild();
         rcount_t refCount = subTree->getFirstChild()->getReferenceCount();
         self()->populateMemoryReference(subTree->getFirstChild(), cg);
         self()->checkAndDecReferenceCount(subTree->getFirstChild(), refCount, cg);
         _symbolReference.addToOffset(-TR::TreeEvaluator::integerConstNodeValue(constChild, cg));
         cg->decReferenceCount(constChild);
         }
      else if (subTreeOp == TR::i2l || subTreeOp == TR::s2l || subTreeOp == TR::s2i)
         {
         self()->setInUpcastingMode();

         if (comp->getOption(TR_TraceCG))
            traceMsg(comp, "Entering UpcastingNoOverflow mode at node %x\n", subTree);
         rcount_t refCount = subTree->getFirstChild()->getReferenceCount();
         self()->populateMemoryReference(subTree->getFirstChild(), cg, subTree);
         self()->checkAndDecReferenceCount(subTree->getFirstChild(), refCount, cg);

         self()->setInUpcastingMode(false);
         }
      else if ((stride = TR::MemoryReference::getStrideForNode(subTree, cg)) != 0)
         {
         if (_indexRegister != NULL)
            {
            if (_baseRegister != NULL || _stride != 0)
               {
               self()->consolidateRegisters(subTree, cg);
               }
            else
               {
               _baseRegister = _indexRegister;
               _baseNode     = _indexNode;
               }
            }
         TR::Node *firstChild  = subTree->getFirstChild();

         if (firstChild->getOpCodeValue()==TR::i2l
             && firstChild->getRegister()==NULL)
            {
            TR::Node *i2lChild = firstChild->getFirstChild();
            TR::Register *sourceReg = i2lChild->getRegister();

            if (!sourceReg)
               self()->evaluate(i2lChild, cg, parent);

            sourceReg = i2lChild->getRegister();

            if (debug("traceMemOp"))
               diagnostic("\nMEMREF ***sourceReg = %p, upperBitsZero = %d, isNonNegative = %d, skipSignExtension = %d \n",
                           sourceReg, sourceReg->areUpperBitsZero(), i2lChild->isNonNegative(), i2lChild->skipSignExtension() );
            if (sourceReg != NULL
               && (((sourceReg->areUpperBitsZero()
                     || (i2lChild->getOpCodeValue()==TR::iRegLoad))
                     && i2lChild->isNonNegative())
                     || i2lChild->skipSignExtension()))
               {
               if (firstChild->getReferenceCount()>1)
                  i2lChild->incReferenceCount();
               cg->decReferenceCount(firstChild);
               firstChild = i2lChild;
               }
            }
         _indexRegister       = self()->evaluate(firstChild, cg, parent);
         _indexNode           = firstChild;
         TR::Node *secondChild = subTree->getSecondChild();
         _stride              = stride;
         cg->decReferenceCount(secondChild);
         }
      else if (subTreeOp == TR::loadaddr
               && !(subTree->getSymbol()->isClassObject()
                    && (cg->needClassAndMethodPointerRelocations() || comp->getOption(TR_EnableHCR))))
         {
         TR::SymbolReference *symRef = subTree->getSymbolReference();
         TR::Symbol *symbol = symRef->getSymbol();
         if (symbol->isRegisterMappedSymbol())
            {
            if (_baseRegister != NULL)
               {
               if (_indexRegister != NULL)
                  {
                  self()->consolidateRegisters(subTree, cg);
                  }
               if (!symbol->isMethodMetaData())
                  {
                  // Must be either auto or parm or error; currently we cannot handle
                  // using the frame register as the index register of a memory reference
                  //
                  _indexRegister = _baseRegister;
                  _baseRegister = cg->getFrameRegister();
                  }
               else
                  {
                  _indexRegister = cg->getMethodMetaDataRegister();
                  }
               _indexNode = NULL;
               }
            else
               {
               if (!symbol->isMethodMetaData())
                  {
                  // Must be either auto or parm or error
                  //
                  _baseRegister = cg->getFrameRegister();
                  }
               else
                  {
                  _baseRegister = cg->getMethodMetaDataRegister();
                  }
               _baseNode = NULL;
               }
            }

         _symbolReference.setSymbol(symbol);
         _symbolReference.addToOffset(symRef->getOffset());
         _symbolReference.setOwningMethodIndex(symRef->getOwningMethodIndex());
         _symbolReference.setCPIndex(symRef->getCPIndex());
         _symbolReference.copyFlags(symRef);
         _symbolReference.copyRefNumIfPossible(symRef, comp->getSymRefTab());

         if (symRef->isUnresolved())
            {
            self()->setUnresolvedDataSnippet(TR::UnresolvedDataSnippet::create(cg, subTree, &_symbolReference, false, symRef->canCauseGC()));
            cg->addSnippet(self()->getUnresolvedDataSnippet());
            self()->setForceWideDisplacement();
            }

         // TODO: aliasing sets?

         // Need to decrement ref count because nodes weren't set on memoryreference.
         //
         cg->decReferenceCount(subTree);
         }
      else if (subTreeOp == TR::aconst)
         {
         _symbolReference.addToOffset(TR::TreeEvaluator::integerConstNodeValue(subTree, cg));
         }
      else
         {
         if (_baseRegister != NULL)
            {
            if (_indexRegister != NULL)
               {
               self()->consolidateRegisters(subTree, cg);
               }
            _indexRegister = self()->evaluate(subTree, cg, parent);
            _indexNode     = subTree;
            }
         else
            {
            _baseRegister  = self()->evaluate(subTree, cg);
            _baseNode      = subTree;
            }
         }
      }

   if (nodeToBeAdjusted && !nodeToBeAdjusted->getRegister())
      {
      cg->decReferenceCount(nodeToBeAdjusted);
      }

   if (comp->getOption(TR_TraceRegisterPressureDetails))
      {
      traceMsg(comp, "   populated memref on %s", cg->getDebug()->getName(subTree));
      cg->getDebug()->dumpLiveRegisters();
      traceMsg(comp, "\n");
      }

   }

TR::Register*
OMR::X86::MemoryReference::evaluate(TR::Node * node, TR::CodeGenerator * cg, TR::Node *parent)
   {

   TR::Register *reg = cg->evaluate(node);

   if (self()->inUpcastingMode())
      {
      if (node->skipSignExtension())
         {
         //We don't need to sign extend the node
         }

      else if ( (node->isNonNegative() ||
               (parent && parent->isNonNegative()))
               && reg->areUpperBitsZero())
         //else if (node->isNonNegative() && reg->areUpperBitsZero())
         {
         //Node is already positive and zero extended
         }
      else if (cg->comp()->target().is64Bit())
         {
         //Sign extension in the 64-bit case
         TR::Instruction *instr = NULL;
         if (node->getSize() == 4)
            instr = generateRegRegInstruction(MOVSXReg8Reg4, node, reg, reg, cg);
         else if (node->getSize() == 2)
            instr = generateRegRegInstruction(MOVSXReg8Reg2, node, reg, reg, cg);

         if (cg->comp()->getOption(TR_TraceCG))
            traceMsg(cg->comp(), "Add a sign extension instruction to 64-bit in Upcasting Mode %x\n", instr);
         }

      else
         {
         //Sign extension in the 32-bit case
         TR::Instruction *instr = NULL;
         if (node->getSize() == 2)
            instr = generateRegRegInstruction(MOVSXReg4Reg2, node, reg, reg, cg);

         if (cg->comp()->getOption(TR_TraceCG))
            traceMsg(cg->comp(), "Add a sign extension instruction to 32-bit in Upcasting Mode %x\n", instr);
         }

      }

   return reg;
   }

void
OMR::X86::MemoryReference::consolidateRegisters(
      TR::Node * node,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();
   if (comp->getOption(TR_TraceRegisterPressureDetails))
      {
      traceMsg(comp, "  consolidateRegisters on %s", cg->getDebug()->getName(node));
      cg->getDebug()->dumpLiveRegisters();
      traceMsg(comp, "\n");
      }

   TR::Register *tempTargetRegister;
   if ((_baseRegister && (_baseRegister->containsCollectedReference() || _baseRegister->containsInternalPointer())) ||
       (_indexRegister && (_indexRegister->containsCollectedReference() || _indexRegister->containsInternalPointer())))
      {
      if (node &&
          node->isInternalPointer() &&
          node->getPinningArrayPointer())
         {
         tempTargetRegister = cg->allocateRegister();
         tempTargetRegister->setContainsInternalPointer();
         tempTargetRegister->setPinningArrayPointer(node->getPinningArrayPointer());
         }
      else
         tempTargetRegister = cg->allocateCollectedReferenceRegister();
      }
   else
      {
      tempTargetRegister = cg->allocateRegister();
      }

   TR::MemoryReference  *interimMemoryReference = generateX86MemoryReference(_baseRegister, _indexRegister, _stride, cg);
   generateRegMemInstruction(LEARegMem(), node, tempTargetRegister, interimMemoryReference, cg);
   self()->decNodeReferenceCounts(cg);
   _baseRegister  = tempTargetRegister;
   _baseNode      = NULL;
   _indexRegister = NULL;
   _stride        = 0;
   }


void
OMR::X86::MemoryReference::assignRegisters(
      TR::Instruction *currentInstruction,
      TR::CodeGenerator *cg)
   {
   TR::RealRegister *assignedBaseRegister = NULL;
   TR::RealRegister *assignedIndexRegister;
   TR::UnresolvedDataSnippet *snippet = self()->getUnresolvedDataSnippet();

   if (_baseRegister != NULL)
      {
      if (_baseRegister == cg->machine()->getRealRegister(TR::RealRegister::vfp))
         {
         assignedBaseRegister = cg->machine()->getRealRegister(TR::RealRegister::vfp);
         }
      else
         {
         assignedBaseRegister = _baseRegister->getAssignedRealRegister();

         if (_indexRegister != NULL)
            {
            _indexRegister->block();
            }

         if (assignedBaseRegister == NULL)
            {
            // Note: a MemRef can be used only once -- if you want to reuse make a copy using
            // generateX86MemoryReference(OMR::X86::MemoryReference  &, intptr_t, TR::CodeGenerator *cg).
            TR_ASSERT(!_baseRegister->getRealRegister(),"_baseRegister is a Real Register already, are you reusing a Memory Reference?");
            assignedBaseRegister = assignGPRegister(currentInstruction, _baseRegister, TR_WordReg, cg);
            }

         if (_indexRegister != NULL)
            {
            _indexRegister->unblock();
            }
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
         assignedIndexRegister = assignGPRegister(currentInstruction, _indexRegister, TR_WordReg, cg);
         }

      if (_indexRegister->decFutureUseCount() == 0 &&
          assignedIndexRegister->getState() != TR::RealRegister::Locked)
         {
         _indexRegister->setAssignedRegister(NULL);
         assignedIndexRegister->setState(TR::RealRegister::Unlatched);
         }

      _indexRegister = assignedIndexRegister;
      if (_baseRegister != NULL)
         {
         _baseRegister->unblock();
         }
      }

   if (_baseRegister != NULL)
      {
      if (_baseRegister->decFutureUseCount() == 0 &&
          assignedBaseRegister->getState() != TR::RealRegister::Locked)
         {
         _baseRegister->setAssignedRegister(NULL);
         assignedBaseRegister->setState(TR::RealRegister::Unlatched);
         }
      _baseRegister = assignedBaseRegister;
      }
   }


uint32_t
OMR::X86::MemoryReference::estimateBinaryLength(TR::CodeGenerator *cg)
   {
   if (self()->getBaseRegister() && toRealRegister(self()->getBaseRegister())->getRegisterNumber() == TR::RealRegister::vfp)
      {
      // Rewrite VFP-relative memref in terms of an actual register
      //
      _baseRegister = cg->machine()->getRealRegister(cg->vfpState()._register);
      self()->getSymbolReference().setOffset(self()->getSymbolReference().getOffset() + cg->vfpState()._displacement);
      }

   TR::RealRegister *base = toRealRegister(self()->getBaseRegister());

   intptr_t displacement;
   uint32_t addressTypes =
      (self()->getBaseRegister() != NULL ? 1 : 0) |
      (self()->getIndexRegister() != NULL ? 2 : 0) |
      ((self()->getSymbolReference().getSymbol() != NULL ||
        self()->getSymbolReference().getOffset() != 0    ||
        self()->isForceWideDisplacement()) ? 4 : 0);
   uint32_t length = 0;

   switch (addressTypes)
      {
      case 1:
         if (base->needsDisp())
            {
            length = 1;
            }
         else if (base->needsSIB())
            {
            length = 2;
            }
         break;

      case 6:
      case 2:
         length = 5;
         break;

      case 3:
         length = 1;
         if (base->needsDisp())
            {
            // Need a sib byte with 8bit displacement field of zero to get ebp as a base register
            //
            length = 2;
            }
         break;

      case 4:
         length = 4;
         break;

      case 5:
         displacement = self()->getDisplacement();
         TR_ASSERT(IS_32BIT_SIGNED(displacement), "64-bit displacement should have been replaced in TR_AMD64MemoryReference::generateBinaryEncoding");
         if (displacement == 0 &&
             !base->needsDisp() &&
             !base->needsSIB() &&
             !self()->isForceWideDisplacement())
            {
            length = 0;
            }
         else if (displacement >= -128 &&
                  displacement <= 127  &&
                  !self()->isForceWideDisplacement())
            {
            length = 1;
            }
         else
            {
            // If there is a symbol or if the displacement will not fit in a byte,
            // then displacement will be 4 bytes
            //
            length = 4;
            }

         if (base->needsSIB() || self()->isForceSIBByte())
            {
            length += 1;
            }
         break;

      case 7:
         displacement = self()->getDisplacement();
         TR_ASSERT(IS_32BIT_SIGNED(displacement), "64-bit displacement should have been replaced in TR_AMD64MemoryReference::generateBinaryEncoding");
         if (displacement >= -128 &&
             displacement <= 127  &&
             !self()->isForceWideDisplacement())
            {
            length = 2;
            }
         else
            {
            // If there is a symbol or if the displacement will not fit in a byte,
            // then displacement will be 4 bytes.
            //
            length = 5;
            }
         break;
      }

      return length;
   }


uint32_t
OMR::X86::MemoryReference::getBinaryLengthLowerBound(TR::CodeGenerator *cg)
   {
   intptr_t displacement;
   uint32_t addressTypes =
      (self()->getBaseRegister()     != NULL ? 1 : 0) |
      (self()->getIndexRegister()    != NULL ? 2 : 0) |
      ((self()->getSymbolReference().getSymbol() != NULL ||
        self()->getSymbolReference().getOffset() != 0    ||
        self()->isForceWideDisplacement()) ? 4 : 0);

   uint32_t length = 0;
   TR::RealRegister::RegNum registerNumber = TR::RealRegister::NoReg;

   if (self()->getBaseRegister())
      {
      registerNumber = toRealRegister(self()->getBaseRegister())->getRegisterNumber();

      if (registerNumber == TR::RealRegister::vfp)
         {
         TR_ASSERT(cg->machine()->getRealRegister(registerNumber)->getAssignedRealRegister(),
                "virtual frame pointer must be assigned before estimating instruction length lower bound!\n");
         registerNumber = toRealRegister(cg->machine()->getRealRegister(registerNumber)->
                             getAssignedRealRegister())->getRegisterNumber();
         }
      }

   TR::RealRegister *base = cg->machine()->getRealRegister(registerNumber);
   switch (addressTypes)
      {
      case 1:
         if (base->needsDisp() ||
             base->needsSIB())
            length = 1;
         else
            length = 0;
         break;

      case 6:
      case 2:
         length = 5;
         break;

      case 3:
         length = 1;
         if (base->needsDisp())
            {
            length = 2;
            }
         break;

      case 4:
         length = 4;
         break;

      case 5:
         displacement = self()->getDisplacement();

         if (displacement == 0 &&
             !base->needsDisp() &&
             !base->needsSIB() &&
             !self()->isForceWideDisplacement())
            {
            length = 0;
            }
         else if (displacement >= -128 &&
                  displacement <= 127 &&
                  !self()->isForceWideDisplacement())
            {
            if (displacement != 0)
               length = 1;
            }
         else
            // If displacement is not a 32-bit value then return the lower bound
            // as 4 bytes. this can happen in compressed-references mode when a
            // memoryReference uses the heap-base constant (which may not be a 32bit
            // value)
            length = 4;

         if (base->needsSIB() || self()->isForceSIBByte())
            {
            length += 1;
            }
         break;

      case 7:
         displacement = self()->getDisplacement();
         if (!self()->isForceWideDisplacement())
            length = 2;
         else
            length = 5;
         break;
      }

   return length;
   }

OMR::X86::EnlargementResult OMR::X86::MemoryReference::enlarge(TR::CodeGenerator *cg, int32_t requestedEnlargementSize, int32_t maxEnlargementSize, bool allowPartialEnlargement)
   {
   static char* disableMemRefExpansion = feGetEnv("TR_DisableMemRefExpansion");
   if (!disableMemRefExpansion)
      return OMR::X86::EnlargementResult(0, 0);

   int32_t growth = 0;
   if (!self()->isForceWideDisplacement())
      {
      int32_t currentEncodingAllocation = self()->estimateBinaryLength(cg);
      int32_t currentPatchSize = self()->getBinaryLengthLowerBound(cg);
      _flags.set(MemRef_ForceWideDisplacement);
      int32_t potentialEncodingGrowth = self()->estimateBinaryLength(cg) - currentPatchSize;
      int32_t potentialPatchGrowth = self()->getBinaryLengthLowerBound(cg) - currentEncodingAllocation;

      if (potentialPatchGrowth > 0 &&
          (potentialPatchGrowth >= requestedEnlargementSize || allowPartialEnlargement) &&
          potentialEncodingGrowth <= maxEnlargementSize &&
          performTransformation(cg->comp(), "O^O Enlarging memory reference by %d bytes by forcing wide displacement - allowpartial was %d", potentialPatchGrowth, allowPartialEnlargement))
         {
         return OMR::X86::EnlargementResult(potentialPatchGrowth, potentialEncodingGrowth);
         }
      else
         {
         _flags.reset(MemRef_ForceWideDisplacement);
         self()->estimateBinaryLength(cg);
         return OMR::X86::EnlargementResult(0, 0);
         }
      }
   return OMR::X86::EnlargementResult(0, 0);
   }


void
OMR::X86::MemoryReference::addMetaDataForCodeAddress(
      uint32_t addressTypes,
      uint8_t *cursor,
      TR::Node *node,
      TR::CodeGenerator *cg)
   {

   switch (addressTypes)
      {
      case 6:
      case 2:
         {
         if (self()->needsCodeAbsoluteExternalRelocation())
            {
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                  0,
                                                                  TR_AbsoluteMethodAddress, cg),
                                 __FILE__,__LINE__, node);
            }
         else if (self()->getReloKind() == TR_ACTIVE_CARD_TABLE_BASE)
            {
            cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                  (uint8_t*)TR_ActiveCardTableBase,
                                                                  TR_GlobalValue, cg),
                                 __FILE__,__LINE__, node);
            }

         if (self()->getSymbolReference().getSymbol()
            && self()->getSymbolReference().getSymbol()->isClassObject()
            && cg->wantToPatchClassPointer(NULL, cursor)) // might not point to beginning of class
            {
            cg->jitAdd32BitPicToPatchOnClassRedefinition((void*)(uintptr_t)*(int32_t*)cursor, (void *) cursor, self()->getUnresolvedDataSnippet() != NULL);
            }

         break;
         }

      case 4:
         {
         TR::Symbol *symbol = self()->getSymbolReference().getSymbol();
         if (symbol)
            {
            TR::StaticSymbol * staticSym = symbol->getStaticSymbol();

            if (staticSym)
               {
               if (self()->getUnresolvedDataSnippet() == NULL)
                  {
                  if (symbol->isConst())
                     {
                     TR::Compilation *comp = cg->comp();
                     cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                            (uint8_t *)self()->getSymbolReference().getOwningMethod(comp)->constantPool(),
                                                                            node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                           TR_ConstantPool, cg),
                                          __FILE__, __LINE__, node);
                     }
                  else if (symbol->isClassObject())
                     {
                     if (cg->needClassAndMethodPointerRelocations())
                        {
                        *(int32_t *)cursor = (int32_t)(TR::Compiler->cls.persistentClassPointerFromClassPointer(cg->comp(), (TR_OpaqueClassBlock*)(self()->getSymbolReference().getOffset() + (intptr_t)staticSym->getStaticAddress())));
                        if (cg->comp()->getOption(TR_UseSymbolValidationManager))
                           {
                           cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                                                     (uint8_t *)(self()->getSymbolReference().getOffset() + (intptr_t)staticSym->getStaticAddress()),
                                                                                                     (uint8_t *)TR::SymbolType::typeClass,
                                                                                                     TR_SymbolFromManager,
                                                                                                     cg),
                                                                                           __FILE__, __LINE__, node);
                           }
                        else
                           {
                           cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *)&self()->getSymbolReference(),
                                                                                                    node ? (uint8_t *)(intptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                                                    TR_ClassAddress, cg), __FILE__, __LINE__, node);
                           }
                        }

                     if (cg->wantToPatchClassPointer(NULL, cursor)) // might not point to beginning of class
                        {
                        cg->jitAdd32BitPicToPatchOnClassRedefinition((void*)(uintptr_t)*(int32_t*)cursor, (void *) cursor, false);
                        }
                     }
                  else
                     {
                     if (staticSym->isCountForRecompile())
                        {
                        cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, (uint8_t *) TR_CountForRecompile, TR_GlobalValue, cg),
                                             __FILE__,
                                             __LINE__,
                                             node);
                        }
                     else if (staticSym->isRecompilationCounter())
                        {
                        cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor, 0, TR_BodyInfoAddress, cg),
                                             __FILE__,
                                             __LINE__,
                                             node);
                        }
                     else if (staticSym->isGCRPatchPoint())
                        {
                        TR::ExternalRelocation* r= new (cg->trHeapMemory())
                           TR::ExternalRelocation(cursor,
                                                      0,
                                                      TR_AbsoluteMethodAddress, cg);
                        cg->addExternalRelocation(r, __FILE__, __LINE__, node);
                        }
                     else if (symbol->isDebugCounter())
                        {
                        TR::DebugCounterBase *counter = cg->comp()->getCounterFromStaticAddress(&(self()->getSymbolReference()));
                        if (counter == NULL)
                           {
                           cg->comp()->failCompilation<TR::CompilationException>("Could not generate relocation for debug counter in OMR::X86::MemoryReference::addMetaDataForCodeAddress\n");
                           }
                        TR::DebugCounter::generateRelocation(cg->comp(),
                                                             cursor,
                                                             node,
                                                             counter);
                        }
                     else
                        {
                        cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                           (uint8_t *)&self()->getSymbolReference(),
                                                                           node ? (uint8_t *)(uintptr_t)node->getInlinedSiteIndex() : (uint8_t *)-1,
                                                                           TR_DataAddress, cg),
                                             __FILE__,
                                             __LINE__,
                                             node);
                        }
                     }
                  }
               else
                  {
                  if (symbol->isClassObject() && cg->wantToPatchClassPointer(NULL, cursor)) // unresolved
                     {
                     cg->jitAdd32BitPicToPatchOnClassRedefinition((void*)-1, (void *) cursor, true);
                     }
                  }
               }
            }
         else
            {
            TR::X86DataSnippet* cds = self()->getDataSnippet();
            TR::LabelSymbol *label = NULL;

            if (cds)
               label = cds->getSnippetLabel();
            else
               label = self()->getLabel();

            if (label != NULL)
               {
               if (cg->comp()->target().is64Bit())
                  {
                  // Assume the snippet is in RIP range
                  // TODO:AMD64: Would it be cleaner to have some kind of "isRelative" flag rather than "is64BitTarget"?
                  cg->addRelocation(new (cg->trHeapMemory()) TR::LabelRelative32BitRelocation(cursor, label));
                  }
               else
                  {
                  cg->addRelocation(new (cg->trHeapMemory()) TR::LabelAbsoluteRelocation(cursor, label));
                  cg->addExternalRelocation(new (cg->trHeapMemory()) TR::ExternalRelocation(cursor,
                                                                        0,
                                                                        TR_AbsoluteMethodAddress, cg),
                                       __FILE__, __LINE__, node);
                  }
               }
            }

         break;
         }

      case 5:
         {
         intptr_t displacement = self()->getDisplacement();
         TR::RealRegister *base = toRealRegister(self()->getBaseRegister());

         if (!(displacement == 0 &&
               !base->needsDisp() &&
               !base->needsDisp() &&
               !self()->isForceWideDisplacement()) &&
             !(displacement >= -128 &&
               displacement <= 127  &&
               !self()->isForceWideDisplacement()))
            {
            if (self()->getSymbolReference().getSymbol()
               && self()->getSymbolReference().getSymbol()->isClassObject()
               && cg->wantToPatchClassPointer(NULL, cursor)) // possibly unresolved, may not point to beginning of class
               {
               cg->jitAdd32BitPicToPatchOnClassRedefinition((void*)(uintptr_t)*(int32_t*)cursor, (void *) cursor, self()->getUnresolvedDataSnippet() != NULL);
               }
            }

         break;
         }

      case 7:
         {
         intptr_t displacement = self()->getDisplacement();

         if (!(displacement >= -128 &&
               displacement <= 127  &&
               !self()->isForceWideDisplacement()))
            {
            if (self()->getSymbolReference().getSymbol()
               && self()->getSymbolReference().getSymbol()->isClassObject()
               && cg->wantToPatchClassPointer(NULL, cursor)) // possibly unresolved, may not point to beginning of class
               {
               cg->jitAdd32BitPicToPatchOnClassRedefinition((void*)(uintptr_t)*(int32_t*)cursor, (void *)cursor, self()->getUnresolvedDataSnippet() != NULL);
               }
            }

         break;
         }

      }

   }


uint8_t *
OMR::X86::MemoryReference::generateBinaryEncoding(
      uint8_t *modRM,
      TR::Instruction *containingInstruction,
      TR::CodeGenerator *cg)
   {
   TR::Compilation *comp = cg->comp();

   uint32_t addressTypes =
      (self()->getBaseRegister() != NULL ? 1 : 0) |
      (self()->getIndexRegister() != NULL ? 2 : 0) |
      ((self()->getSymbolReference().getSymbol() != NULL ||
        self()->getSymbolReference().getOffset() != 0    ||
        self()->isForceWideDisplacement()) ? 4 : 0);

   intptr_t displacement;

   uint8_t *cursor = modRM;
   TR::RealRegister *base = NULL;
   TR::RealRegister *index = NULL;
   TR::RealRegister::RegNum baseRegisterNumber = TR::RealRegister::NoReg;
   TR::Symbol *symbol;
   uint8_t *immediateCursor = 0;

   switch (addressTypes)
      {
      case 1:
         {
         base = toRealRegister(self()->getBaseRegister());
         baseRegisterNumber = base->getRegisterNumber();

         if (baseRegisterNumber == TR::RealRegister::vfp)
            {
            TR_ASSERT(cg->machine()->getRealRegister(baseRegisterNumber)->getAssignedRealRegister(),
                   "virtual frame pointer must be assigned before binary encoding!\n");

            base = toRealRegister(cg->machine()->
               getRealRegister(baseRegisterNumber)->getAssignedRealRegister());
            baseRegisterNumber = base->getRegisterNumber();
            self()->setBaseRegister(base);
            }

         if (base->needsDisp())
            {
            self()->ModRM(modRM)->setBaseDisp8();
            base->setRMRegisterFieldInModRM(modRM);
            *++cursor = 0x00;
            }
         else if (base->needsSIB())
            {
            self()->ModRM(modRM)->setBase()->setHasSIB();
            *++cursor = 0x00;
            self()->SIB(cursor)->setNoIndex();
            base->setBaseRegisterFieldInSIB(cursor);
            }
         else
            {
            self()->ModRM(modRM)->setBase();
            base->setRMRegisterFieldInModRM(modRM);
            }

         ++cursor;
         break;
         }

      case 6:
      case 2:
         {
         index = toRealRegister(self()->getIndexRegister());
         self()->ModRM(modRM)->setBase()->setHasSIB();
         *++cursor = 0x00;
         self()->SIB(cursor)->setIndexDisp32();
         index->setIndexRegisterFieldInSIB(cursor);
         self()->setStrideFieldInSIB(cursor);
         cursor++;

         immediateCursor = cursor;

         *(int32_t *)cursor = (int32_t)self()->getSymbolReference().getOffset();

         if (self()->getUnresolvedDataSnippet() != NULL)
            {
            self()->getUnresolvedDataSnippet()->setAddressOfDataReference(cursor);
            }

         cursor += 4;
         break;
         }

      case 3:
         {
         base = toRealRegister(self()->getBaseRegister());
         baseRegisterNumber = base->getRegisterNumber();

         if (baseRegisterNumber == TR::RealRegister::vfp)
            {
            TR_ASSERT(cg->machine()->getRealRegister(baseRegisterNumber)->getAssignedRealRegister(),
                   "virtual frame pointer must be assigned before binary encoding!\n");

            base = toRealRegister(cg->machine()->
                   getRealRegister(baseRegisterNumber)->getAssignedRealRegister());
            baseRegisterNumber = base->getRegisterNumber();
            self()->setBaseRegister(base);
            }

         index = toRealRegister(self()->getIndexRegister());
         *++cursor = 0x00;
         base->setBaseRegisterFieldInSIB(cursor);
         index->setIndexRegisterFieldInSIB(cursor);
         self()->setStrideFieldInSIB(cursor);
         if (base->needsDisp())
            {
            // Need a sib byte with 8bit displacement field of zero to get ebp as a base register
            //
            self()->ModRM(modRM)->setBaseDisp8()->setHasSIB();
            *++cursor = 0x00;
            }
         else
            {
            self()->ModRM(modRM)->setBase()->setHasSIB();
            }
         ++cursor;
         break;
         }

      case 4:
         {
         self()->ModRM(modRM)->setIndexOnlyDisp32();
         cursor++;
         symbol = self()->getSymbolReference().getSymbol();
         immediateCursor = cursor;

         if (symbol)
            {
            TR::StaticSymbol * staticSym = symbol->getStaticSymbol();
            TR::MethodSymbol * methodSym = symbol->getMethodSymbol();

            if (staticSym)
               {

               if (self()->getUnresolvedDataSnippet() == NULL)
                  {
                  *(int32_t *)cursor = (int32_t)(self()->getSymbolReference().getOffset() + (intptr_t)staticSym->getStaticAddress());
                  }
               else
                  {
                  *(int32_t *)cursor = (int32_t)self()->getSymbolReference().getOffset();
                  }
               }
            else if (methodSym)
               {
               /* ----------------------------------------------------- */
               /* This is fake. Not supported for JIT compilation;      */
               /* just avoiding a core dump                             */
               /* ----------------------------------------------------- */
               * (int32_t *) cursor = 0;
               }
            else if (symbol->isRegisterMappedSymbol())
               {
               displacement = self()->getSymbolReference().getOffset() +
                                 symbol->getRegisterMappedSymbol()->getOffset();
               TR_ASSERT(IS_32BIT_SIGNED(displacement), "64-bit displacement should have been replaced in TR_AMD64MemoryReference::generateBinaryEncoding");
               *(int32_t *)cursor = (int32_t)displacement;
               }
            else if (symbol->isShadow())
               {
               *(int32_t *)cursor = (int32_t)self()->getSymbolReference().getOffset();
               }
            else
               TR_ASSERT(0, "generateBinaryEncoding, new symbol hierarchy problem");
            }
         else
            {
            TR::X86DataSnippet* cds = self()->getDataSnippet();
            TR_ASSERT(cds == NULL || self()->getLabel() == NULL,
                   "a memRef cannot have both a constant data snippet and a label");
            TR::LabelSymbol *label = NULL;
            if (cds)
               label = cds->getSnippetLabel();
            else
               label = self()->getLabel();

            if (label != NULL)
               {
               if (cg->comp()->target().is64Bit())
                  {
                  // This cast is ok because we only need the low 32 bits of the address
                  // *(int32_t *)cursor = -(int32_t)(intptr_t)(cursor+4);
                  // if it is X86MEMIMM instruction, offset need consider immedidate length
                  *(int32_t *)cursor = -(int32_t)(intptr_t)(cursor+4 + containingInstruction->getOpCode().info().ImmediateSize());
                  }
               else
                  {
                  *(int32_t *)cursor = (int32_t)0;
                  }
               }
            else
               {
               *(int32_t *)cursor = (int32_t)self()->getSymbolReference().getOffset();
               }
            }

         if (self()->getUnresolvedDataSnippet() != NULL)
            {
            self()->getUnresolvedDataSnippet()->setAddressOfDataReference(cursor);
            }

         cursor += 4;
         break;
         }

      case 5:
         {
         base = toRealRegister(self()->getBaseRegister());
         baseRegisterNumber = base->getRegisterNumber();

         if (baseRegisterNumber == TR::RealRegister::vfp)
            {
            TR_ASSERT(cg->machine()->getRealRegister(baseRegisterNumber)->getAssignedRealRegister(),
                   "virtual frame pointer must be assigned before binary encoding!\n");

            base = toRealRegister(cg->machine()->
                   getRealRegister(baseRegisterNumber)->getAssignedRealRegister());
            baseRegisterNumber = base->getRegisterNumber();
            self()->setBaseRegister(base);
            }

         if (base->needsSIB() || self()->isForceSIBByte())
            {
            *++cursor = 0x00;
            self()->ModRM(modRM)->setBase()->setHasSIB();
            self()->SIB(cursor)->setNoIndex();
            }

         displacement = self()->getDisplacement();
         TR_ASSERT(IS_32BIT_SIGNED(displacement), "64-bit displacement should have been replaced in TR_AMD64MemoryReference::generateBinaryEncoding");
         base->setRMRegisterFieldInModRM(cursor++);
         immediateCursor = cursor;

         if (displacement == 0 &&
             !base->needsDisp() &&
             !base->needsDisp() &&
             !self()->isForceWideDisplacement())
            {
            self()->ModRM(modRM)->setBase();
            }
         else if (displacement >= -128 &&
                  displacement <= 127  &&
                  !self()->isForceWideDisplacement())
            {
            self()->ModRM(modRM)->setBaseDisp8();
            *cursor = (uint8_t)displacement;
            ++cursor;
            }
         else
            {
            // If there is a symbol or if the displacement will not fit in a byte,
            // then displacement will be 4 bytes.
            //
            self()->ModRM(modRM)->setBaseDisp32();
            *(int32_t *)cursor = (int32_t)displacement;
            if (self()->getUnresolvedDataSnippet() != NULL)
               {
               self()->getUnresolvedDataSnippet()->setAddressOfDataReference(cursor);
               }

            cursor += 4;
            }
         break;
         }

      case 7:
         {
         base = toRealRegister(self()->getBaseRegister());
         baseRegisterNumber = base->getRegisterNumber();

         if (baseRegisterNumber == TR::RealRegister::vfp)
            {
            TR_ASSERT(cg->machine()->getRealRegister(baseRegisterNumber)->getAssignedRealRegister(),
                   "virtual frame pointer must be assigned before binary encoding!\n");

            base = toRealRegister(cg->machine()->
                      getRealRegister(baseRegisterNumber)->getAssignedRealRegister());
            baseRegisterNumber = base->getRegisterNumber();
            self()->setBaseRegister(base);
            }

         index = toRealRegister(self()->getIndexRegister());
         *++cursor = 0x00;
         base->setBaseRegisterFieldInSIB(cursor);
         index->setIndexRegisterFieldInSIB(cursor);
         self()->setStrideFieldInSIB(cursor);
         ++cursor;
         displacement = self()->getDisplacement();
         immediateCursor = cursor;

         TR_ASSERT(IS_32BIT_SIGNED(displacement), "64-bit displacement should have been replaced in TR_AMD64MemoryReference::generateBinaryEncoding");
         if (displacement >= -128 &&
             displacement <= 127  &&
             !self()->isForceWideDisplacement())
            {
            self()->ModRM(modRM)->setBaseDisp8()->setHasSIB();
            *cursor++ = (uint8_t)displacement;
            }
         else
            {
            // If there is a symbol or if the displacement will not fit in a byte,
            // then displacement will be 4 bytes.
            //
            self()->ModRM(modRM)->setBaseDisp32()->setHasSIB();
            *(int32_t *)cursor = (int32_t)displacement;
            if (self()->getUnresolvedDataSnippet() != NULL)
               {
               self()->getUnresolvedDataSnippet()->setAddressOfDataReference(cursor);
               }

            cursor += 4;
            }
         break;
         }
      }

   self()->addMetaDataForCodeAddress(addressTypes, immediateCursor, containingInstruction->getNode(), cg);

   return cursor;
   }


static
void rematerializeAddressAdds(
      TR::Node *rootLoadOrStore,
      TR::CodeGenerator *cg)
   {
   TR::Node *subTree = rootLoadOrStore->getFirstChild();

   // Only try to rematerialize addresses computation that will cause (avoidable) register
   // pressure. Use the existing register if one has been allocated for a previous use of
   // the sum address. Do nothing if the sum address will not be re-used.
   //
   if (subTree->getOpCode().isArrayRef() &&
         subTree->getRegister() == NULL && subTree->getReferenceCount() > 1)
      {
      TR::Node *baseNode = subTree->getFirstChild();
      TR::Node *indexNode = subTree->getSecondChild();

      // Avoid index computations that potentially involve multiple bases.
      //
      if (baseNode->getOpCode().isIndirect())
         return;

      // Do nothing if the index is not a constant offset from the base.
      //
      if (indexNode->getOpCode().isLoadConst())
         {
         TR::Node *tempNode = TR::Node::copy(subTree);

         tempNode->setReferenceCount(1);
         tempNode->setRegister(NULL);
         baseNode->incReferenceCount();
         indexNode->incReferenceCount();

         rootLoadOrStore->setChild(0, tempNode);

         cg->decReferenceCount(subTree);

         if (debug("traceInstructionSelection"))
            {
            if (cg->comp()->target().is64Bit())
               diagnostic("\nRematerializing aladd [" POINTER_PRINTF_FORMAT "] with [" POINTER_PRINTF_FORMAT "] at [" POINTER_PRINTF_FORMAT "]",
                        tempNode, subTree, rootLoadOrStore);
            else
               diagnostic("\nRematerializing aiadd [" POINTER_PRINTF_FORMAT "] with [" POINTER_PRINTF_FORMAT "] at [" POINTER_PRINTF_FORMAT "]",
                        tempNode, subTree, rootLoadOrStore);
            }
         }
      }
   }


const uint8_t OMR::X86::MemoryReference::_multiplierToStrideMap[HIGHEST_STRIDE_MULTIPLIER + 1] =
   {
    0,     // 0
    0,     // 1
    1,     // 2
    0,     // 3
    2,     // 4
    0,     // 5
    0,     // 6
    0,     // 7
    3      // 8
   };


#ifdef DEBUG
uint32_t OMR::X86::MemoryReference::getNumMRReferencedGPRegisters()
   {
   return (self()->getBaseRegister() ? 1 : 0) + (self()->getIndexRegister() ? 1 : 0);
   }
#endif

///////////////////////////////////////////
// Generate Routines
///////////////////////////////////////////

TR::MemoryReference  *
generateX86MemoryReference(TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(intptr_t disp, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(disp, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::Register * br, intptr_t disp, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(br, disp, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::Register * br, TR::Register * ir, uint8_t s, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(br, ir, s, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::Register * br, TR::Register * ir, uint8_t s, intptr_t disp, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(br, ir, s, disp, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::Node * node, TR::CodeGenerator *cg, bool canRematerializeAddressAdds)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(node, cg, canRematerializeAddressAdds);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::MemoryReference  & mr, intptr_t n, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(mr, n, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::MemoryReference& mr, intptr_t n, TR_ScratchRegisterManager *srm, TR::CodeGenerator *cg)
   {
   if(cg->comp()->target().is64Bit())
      return new (cg->trHeapMemory()) TR::MemoryReference(mr, n, cg, srm);
   else
      return new (cg->trHeapMemory()) TR::MemoryReference(mr, n, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::SymbolReference * sr, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(sr, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::SymbolReference * sr, intptr_t displacement, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(sr, displacement, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::X86DataSnippet* cds, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(cds, cg);
   }

TR::MemoryReference  *
generateX86MemoryReference(TR::LabelSymbol *label, TR::CodeGenerator *cg)
   {
   return new (cg->trHeapMemory()) TR::MemoryReference(label, cg);
   }
