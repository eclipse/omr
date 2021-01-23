/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#include "compile/AliasBuilder.hpp"

#include "compile/AliasBuilder.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/SymbolReference.hpp"
#include "infra/BitVector.hpp"

OMR::AliasBuilder::AliasBuilder(TR::SymbolReferenceTable *symRefTab, size_t sizeHint, TR::Compilation *c) :
     _addressShadowSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _intShadowSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _genericIntShadowSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _genericIntArrayShadowSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _genericIntNonArrayShadowSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _nonIntPrimitiveShadowSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _addressStaticSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _intStaticSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _nonIntPrimitiveStaticSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _methodSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _arrayElementSymRefs(TR::NumTypes, c->trMemory(), heapAlloc, growable),
     _arrayletElementSymRefs(TR::NumTypes, c->trMemory(), heapAlloc, growable),
     _unsafeSymRefNumbers(sizeHint, c->trMemory(), heapAlloc, growable),
     _unsafeArrayElementSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _gcSafePointSymRefNumbers(sizeHint, c->trMemory(), heapAlloc, growable),
     _cpConstantSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _cpSymRefs(sizeHint, c->trMemory(), heapAlloc, growable),
     _immutableArrayElementSymRefs(1, c->trMemory(), heapAlloc, growable),
     _refinedNonIntPrimitiveArrayShadows(1, c->trMemory(), heapAlloc, growable),
     _refinedAddressArrayShadows(1, c->trMemory(), heapAlloc, growable),
     _refinedIntArrayShadows(1, c->trMemory(), heapAlloc, growable),
     _litPoolGenericIntShadowHasBeenCreated(false),
     _userFieldMethodDefAliases(c->trMemory(), _numNonUserFieldClasses),
     _conservativeGenericIntShadowAliasingRequired(false),
     _mutableGenericIntShadowHasBeenCreated(false),
     _symRefTab(symRefTab),
     _comp(c),
     _trMemory(c->trMemory())
   {
   }

TR::AliasBuilder *
OMR::AliasBuilder::self()
   {
   return static_cast<TR::AliasBuilder *>(this);
   }

TR::SymbolReference *
OMR::AliasBuilder::getSymRefForAliasing(TR::Node *node, TR::Node *addrChild)
   {
   return node->getSymbolReference();
   }

void
OMR::AliasBuilder::updateSubSets(TR::SymbolReference *ref)
   {
   int32_t refNum = ref->getReferenceNumber();
   TR::Symbol *sym = ref->getSymbol();

   if (sym && sym->isMethod())
      _methodSymRefs.set(refNum);
   }

TR_BitVector *
OMR::AliasBuilder::methodAliases(TR::SymbolReference *symRef)
   {
   if (_comp->getOption(TR_TraceAliases))
      traceMsg(_comp, "For method sym %d default aliases\n", symRef->getReferenceNumber());

   return &_defaultMethodDefAliases;
   }

void
OMR::AliasBuilder::setVeryRefinedCallAliasSets(TR::ResolvedMethodSymbol * m, TR_BitVector * bc)
   {
   _callAliases.add(new (self()->trHeapMemory()) CallAliases(bc, m));
   }

TR_BitVector *
OMR::AliasBuilder::getVeryRefinedCallAliasSets(TR::ResolvedMethodSymbol * methodSymbol)
   {
   for (CallAliases * callAliases = _callAliases.getFirst(); callAliases; callAliases = callAliases->getNext())
      if (callAliases->_methodSymbol == methodSymbol)
         return callAliases->_callAliases;
   TR_ASSERT(0, "getVeryRefinedCallAliasSets didn't find alias info");
   return 0;
   }

void
OMR::AliasBuilder::createAliasInfo()
   {
   TR::StackMemoryRegion stackMemoryRegion(*_trMemory);

   _addressShadowSymRefs.pack();
   _genericIntShadowSymRefs.pack();
   _genericIntArrayShadowSymRefs.pack();
   _genericIntNonArrayShadowSymRefs.pack();
   _intShadowSymRefs.pack();
   _nonIntPrimitiveShadowSymRefs.pack();
   _addressStaticSymRefs.pack();
   _intStaticSymRefs.pack();
   _nonIntPrimitiveStaticSymRefs.pack();
   _methodSymRefs.pack();
   _unsafeSymRefNumbers.pack();
   _unsafeArrayElementSymRefs.pack();
   _gcSafePointSymRefNumbers.pack();

   self()->setCatchLocalUseSymRefs();

   _defaultMethodDefAliases.init(_symRefTab->getNumSymRefs(), _comp->trMemory(), heapAlloc, growable);
   _defaultMethodDefAliases |= _addressShadowSymRefs;
   _defaultMethodDefAliases |= _intShadowSymRefs;
   _defaultMethodDefAliases |= _nonIntPrimitiveShadowSymRefs;
   _defaultMethodDefAliases |= _arrayElementSymRefs;
   _defaultMethodDefAliases |= _arrayletElementSymRefs;
   _defaultMethodDefAliases |= _addressStaticSymRefs;
   _defaultMethodDefAliases |= _intStaticSymRefs;
   _defaultMethodDefAliases |= _nonIntPrimitiveStaticSymRefs;
   _defaultMethodDefAliases |= _unsafeSymRefNumbers;
   _defaultMethodDefAliases |= _gcSafePointSymRefNumbers;

   _defaultMethodDefAliasesWithoutImmutable.init(_symRefTab->getNumSymRefs(), _comp->trMemory(), heapAlloc, growable);
   _defaultMethodDefAliasesWithoutUserField.init(_symRefTab->getNumSymRefs(), _comp->trMemory(), heapAlloc, growable);

   _defaultMethodDefAliasesWithoutUserField |= _defaultMethodDefAliases;

   _defaultMethodDefAliasesWithoutImmutable |= _defaultMethodDefAliases;

   _defaultMethodUseAliases.init(_symRefTab->getNumSymRefs(), _comp->trMemory(), heapAlloc, growable);
   _defaultMethodUseAliases |= _defaultMethodDefAliases;
   _defaultMethodUseAliases |= _catchLocalUseSymRefs;

   if (_symRefTab->element(TR::SymbolReferenceTable::contiguousArraySizeSymbol))
      _defaultMethodUseAliases.set(_symRefTab->element(TR::SymbolReferenceTable::contiguousArraySizeSymbol)->getReferenceNumber());
   if (_symRefTab->element(TR::SymbolReferenceTable::discontiguousArraySizeSymbol))
      _defaultMethodUseAliases.set(_symRefTab->element(TR::SymbolReferenceTable::discontiguousArraySizeSymbol)->getReferenceNumber());
   if (_symRefTab->element(TR::SymbolReferenceTable::vftSymbol))
      _defaultMethodUseAliases.set(_symRefTab->element(TR::SymbolReferenceTable::vftSymbol)->getReferenceNumber());

   _methodsThatMayThrow.init(_symRefTab->getNumSymRefs(), _comp->trMemory(), heapAlloc, growable);
   _methodsThatMayThrow |= _methodSymRefs;

   for (CallAliases * callAliases = _callAliases.getFirst(); callAliases; callAliases = callAliases->getNext())
      callAliases->_methodSymbol->setHasVeryRefinedAliasSets(false);
   _callAliases.setFirst(0);

   if (_comp->getOption(TR_TraceAliases))
      {
      _comp->getDebug()->printAliasInfo(_comp->getOutFile(), _symRefTab);
      }

   }

void
OMR::AliasBuilder::setCatchLocalUseSymRefs()
   {
   _catchLocalUseSymRefs.init(_symRefTab->getNumSymRefs(), _trMemory);
   _notOsrCatchLocalUseSymRefs.init(_symRefTab->getNumSymRefs(), _trMemory);

   vcount_t visitCount = _comp->incVisitCount();

   for (TR::CFGNode * node = _comp->getFlowGraph()->getFirstNode(); node; node = node->getNext())
      {
      if (!node->getExceptionPredecessors().empty())
         {
         bool isOSRCatch = false;
         if (node->asBlock()->isOSRCatchBlock())
            isOSRCatch = true;

         if (!isOSRCatch)
            {
            self()->gatherLocalUseInfo(toBlock(node), isOSRCatch);
            }
         }
      }

   if (_comp->getOption(TR_EnableOSR))
      {
      visitCount = _comp->incVisitCount();

      for (TR::CFGNode *node = _comp->getFlowGraph()->getFirstNode(); node; node = node->getNext())
         {
         if (!node->getExceptionPredecessors().empty())
            {
            bool isOSRCatch = false;
            if (node->asBlock()->isOSRCatchBlock())
               isOSRCatch = true;

            if (isOSRCatch)
               {
               self()->gatherLocalUseInfo(toBlock(node), isOSRCatch);
               }
            }
         }
      }
   }

void
OMR::AliasBuilder::gatherLocalUseInfo(TR::Block * block, TR_BitVector & storeVector, TR_ScratchList<TR_Pair<TR::Block, TR_BitVector> > *seenBlockInfos, vcount_t visitCount, bool isOSRCatch)
   {
   for (TR::TreeTop * tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
      self()->gatherLocalUseInfo(tt->getNode(), storeVector, visitCount, isOSRCatch);

   TR_SuccessorIterator edges(block);
   for (TR::CFGEdge * edge = edges.getFirst(); edge; edge = edges.getNext())
      {
      TR_BitVector *predBitVector = NULL;
      if (((edge->getTo()->getPredecessors().size() == 1) && edge->getTo()->getExceptionPredecessors().empty()) /* ||
                                                                                                                     (edge->getTo()->getExceptionPredecessors().isSingleton() && edge->getTo()->getPredecessors().empty()) */)
         {
         predBitVector = new (_comp->trStackMemory()) TR_BitVector(_symRefTab->getNumSymRefs(), _comp->trMemory(), stackAlloc);
         *predBitVector = storeVector;
         }

      TR_Pair<TR::Block, TR_BitVector> *pair = new (self()->trStackMemory()) TR_Pair<TR::Block, TR_BitVector> (toBlock(edge->getTo()), predBitVector);
      seenBlockInfos->add(pair);
      }
   }

void
OMR::AliasBuilder::gatherLocalUseInfo(TR::Block * catchBlock, bool isOSRCatch)
   {
   vcount_t visitCount = _comp->getVisitCount();
   TR_ScratchList<TR_Pair<TR::Block, TR_BitVector> > seenBlockInfos(_trMemory);

   TR_Pair<TR::Block, TR_BitVector> *p = new (self()->trStackMemory()) TR_Pair<TR::Block, TR_BitVector> (catchBlock, NULL);
   seenBlockInfos.add(p);
   while (!seenBlockInfos.isEmpty())
      {
      TR_Pair<TR::Block, TR_BitVector> *blockInfo = seenBlockInfos.popHead();
      TR::Block *block = blockInfo->getKey();

      if (block->getVisitCount() == visitCount)
         continue;

      block->setVisitCount(visitCount);

      TR_BitVector *predStoreVector = blockInfo->getValue();

      if (predStoreVector)
         self()->gatherLocalUseInfo(block, *predStoreVector, &seenBlockInfos, visitCount, isOSRCatch);
      else
         {
         TR_BitVector storeVector(_symRefTab->getNumSymRefs(), _comp->trMemory(), stackAlloc);
         self()->gatherLocalUseInfo(block, storeVector, &seenBlockInfos, visitCount, isOSRCatch);
         }
      }
   }

void
OMR::AliasBuilder::gatherLocalUseInfo(TR::Node * node, TR_BitVector & storeVector, vcount_t visitCount, bool isOSRCatch)
   {
   if (node->getVisitCount() == visitCount)
      return;
   node->setVisitCount(visitCount);

   for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
      self()->gatherLocalUseInfo(node->getChild(i), storeVector, visitCount, isOSRCatch);

   TR::SymbolReference * symRef = node->getOpCode().hasSymbolReference() ? node->getSymbolReference() : 0;

   if (symRef && symRef->getSymbol()->isAutoOrParm())
      {
      int32_t refNumber = symRef->getReferenceNumber();
      if (node->getOpCode().isStoreDirect())
         storeVector.set(refNumber);
      else if (!storeVector.get(refNumber))
         {
         _catchLocalUseSymRefs.set(refNumber);
         if (!isOSRCatch)
            _notOsrCatchLocalUseSymRefs.set(refNumber);
         }
      }
   }

bool
OMR::AliasBuilder::hasUseonlyAliasesOnlyDueToOSRCatchBlocks(TR::SymbolReference *symRef)
   {
   if (_notOsrCatchLocalUseSymRefs.get(symRef->getReferenceNumber()))
      return false;
   return true;
   }

bool
OMR::AliasBuilder::conservativeGenericIntShadowAliasing()
   {
   static char *disableConservativeGenericIntShadowAliasing = feGetEnv("TR_disableConservativeGenericIntShadowAliasing");
   if (disableConservativeGenericIntShadowAliasing)
      return false;
   else
      return _conservativeGenericIntShadowAliasingRequired;
   }

static bool supportArrayRefinement = true;

void
OMR::AliasBuilder::addNonIntPrimitiveArrayShadows(TR_BitVector *aliases)
   {
   if(supportArrayRefinement)
      *aliases |= _refinedNonIntPrimitiveArrayShadows;

   aliases->set(_symRefTab->getArrayShadowIndex(TR::Int8));
   aliases->set(_symRefTab->getArrayShadowIndex(TR::Int16));
   aliases->set(_symRefTab->getArrayShadowIndex(TR::Int32));
   aliases->set(_symRefTab->getArrayShadowIndex(TR::Int64));
   aliases->set(_symRefTab->getArrayShadowIndex(TR::Float));
   aliases->set(_symRefTab->getArrayShadowIndex(TR::Double));
   }

void
OMR::AliasBuilder::addAddressArrayShadows(TR_BitVector *aliases)
   {
   if(supportArrayRefinement)
      *aliases |= _refinedAddressArrayShadows;

   aliases->set(_symRefTab->getArrayShadowIndex(TR::Address));
   }

void
OMR::AliasBuilder::addIntArrayShadows(TR_BitVector *aliases)
   {
   if(supportArrayRefinement)
      *aliases |= _refinedIntArrayShadows;

   aliases->set(_symRefTab->getArrayShadowIndex(TR::Int32));
   }
