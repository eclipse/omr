/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
     _compilation(c),
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
      methodSymRefs().set(refNum);
   }

TR_BitVector *
OMR::AliasBuilder::methodAliases(TR::SymbolReference *symRef)
   {
   if (comp()->getOption(TR_TraceAliases))
      traceMsg(comp(), "For method sym %d default aliases\n", symRef->getReferenceNumber());

   return &defaultMethodDefAliases();
   }

void
OMR::AliasBuilder::setVeryRefinedCallAliasSets(TR::ResolvedMethodSymbol * m, TR_BitVector * bc)
   {
   _callAliases.add(new (trHeapMemory()) CallAliases(bc, m));
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
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   addressShadowSymRefs().pack();
   genericIntShadowSymRefs().pack();
   genericIntArrayShadowSymRefs().pack();
   genericIntNonArrayShadowSymRefs().pack();
   intShadowSymRefs().pack();
   nonIntPrimitiveShadowSymRefs().pack();
   addressStaticSymRefs().pack();
   intStaticSymRefs().pack();
   nonIntPrimitiveStaticSymRefs().pack();
   methodSymRefs().pack();
   unsafeSymRefNumbers().pack();
   unsafeArrayElementSymRefs().pack();
   gcSafePointSymRefNumbers().pack();

   setCatchLocalUseSymRefs();

   defaultMethodDefAliases().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   defaultMethodDefAliases() |= addressShadowSymRefs();
   defaultMethodDefAliases() |= intShadowSymRefs();
   defaultMethodDefAliases() |= nonIntPrimitiveShadowSymRefs();
   defaultMethodDefAliases() |= arrayElementSymRefs();
   defaultMethodDefAliases() |= arrayletElementSymRefs();
   defaultMethodDefAliases() |= addressStaticSymRefs();
   defaultMethodDefAliases() |= intStaticSymRefs();
   defaultMethodDefAliases() |= nonIntPrimitiveStaticSymRefs();
   defaultMethodDefAliases() |= unsafeSymRefNumbers();
   defaultMethodDefAliases() |= gcSafePointSymRefNumbers();

   defaultMethodDefAliasesWithoutImmutable().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   defaultMethodDefAliasesWithoutUserField().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);

   defaultMethodDefAliasesWithoutUserField() |= defaultMethodDefAliases();

   defaultMethodDefAliasesWithoutImmutable() |= defaultMethodDefAliases();

   defaultMethodUseAliases().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   defaultMethodUseAliases() |= defaultMethodDefAliases();
   defaultMethodUseAliases() |= catchLocalUseSymRefs();

   if (symRefTab()->element(TR::SymbolReferenceTable::contiguousArraySizeSymbol))
      defaultMethodUseAliases().set(symRefTab()->element(TR::SymbolReferenceTable::contiguousArraySizeSymbol)->getReferenceNumber());
   if (symRefTab()->element(TR::SymbolReferenceTable::discontiguousArraySizeSymbol))
      defaultMethodUseAliases().set(symRefTab()->element(TR::SymbolReferenceTable::discontiguousArraySizeSymbol)->getReferenceNumber());
   if (symRefTab()->element(TR::SymbolReferenceTable::vftSymbol))
      defaultMethodUseAliases().set(symRefTab()->element(TR::SymbolReferenceTable::vftSymbol)->getReferenceNumber());

   methodsThatMayThrow().init(symRefTab()->getNumSymRefs(), comp()->trMemory(), heapAlloc, growable);
   methodsThatMayThrow() |= methodSymRefs();

   for (CallAliases * callAliases = _callAliases.getFirst(); callAliases; callAliases = callAliases->getNext())
      callAliases->_methodSymbol->setHasVeryRefinedAliasSets(false);
   _callAliases.setFirst(0);

   if (comp()->getOption(TR_TraceAliases))
      {
      comp()->getDebug()->printAliasInfo(comp()->getOutFile(), symRefTab());
      }

   }

void
OMR::AliasBuilder::setCatchLocalUseSymRefs()
   {
   catchLocalUseSymRefs().init(symRefTab()->getNumSymRefs(), trMemory());
   notOsrCatchLocalUseSymRefs().init(symRefTab()->getNumSymRefs(), trMemory());

   vcount_t visitCount = comp()->incVisitCount();

   for (TR::CFGNode * node = comp()->getFlowGraph()->getFirstNode(); node; node = node->getNext())
      {
      if (!node->getExceptionPredecessors().empty())
         {
         bool isOSRCatch = false;
         if (node->asBlock()->isOSRCatchBlock())
            isOSRCatch = true;

         if (!isOSRCatch)
            {
            gatherLocalUseInfo(toBlock(node), isOSRCatch);
            }
         }
      }

   if (comp()->getOption(TR_EnableOSR))
      {
      visitCount = comp()->incVisitCount();

      for (TR::CFGNode *node = comp()->getFlowGraph()->getFirstNode(); node; node = node->getNext())
         {
         if (!node->getExceptionPredecessors().empty())
            {
            bool isOSRCatch = false;
            if (node->asBlock()->isOSRCatchBlock())
               isOSRCatch = true;

            if (isOSRCatch)
               {
               gatherLocalUseInfo(toBlock(node), isOSRCatch);
               }
            }
         }
      }
   }

void
OMR::AliasBuilder::gatherLocalUseInfo(TR::Block * block, TR_BitVector & storeVector, TR_ScratchList<TR_Pair<TR::Block, TR_BitVector> > *seenBlockInfos, vcount_t visitCount, bool isOSRCatch)
   {
   for (TR::TreeTop * tt = block->getEntry(); tt != block->getExit(); tt = tt->getNextTreeTop())
      gatherLocalUseInfo(tt->getNode(), storeVector, visitCount, isOSRCatch);

   TR_SuccessorIterator edges(block);
   for (TR::CFGEdge * edge = edges.getFirst(); edge; edge = edges.getNext())
      {
      TR_BitVector *predBitVector = NULL;
      if (((edge->getTo()->getPredecessors().size() == 1) && edge->getTo()->getExceptionPredecessors().empty()) /* ||
                                                                                                                     (edge->getTo()->getExceptionPredecessors().isSingleton() && edge->getTo()->getPredecessors().empty()) */)
         {
         predBitVector = new (comp()->trStackMemory()) TR_BitVector(symRefTab()->getNumSymRefs(), comp()->trMemory(), stackAlloc);
         *predBitVector = storeVector;
         }

      TR_Pair<TR::Block, TR_BitVector> *pair = new (trStackMemory()) TR_Pair<TR::Block, TR_BitVector> (toBlock(edge->getTo()), predBitVector);
      seenBlockInfos->add(pair);
      }
   }

void
OMR::AliasBuilder::gatherLocalUseInfo(TR::Block * catchBlock, bool isOSRCatch)
   {
   vcount_t visitCount = comp()->getVisitCount();
   TR_ScratchList<TR_Pair<TR::Block, TR_BitVector> > seenBlockInfos(trMemory());

   TR_Pair<TR::Block, TR_BitVector> *p = new (trStackMemory()) TR_Pair<TR::Block, TR_BitVector> (catchBlock, NULL);
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
         gatherLocalUseInfo(block, *predStoreVector, &seenBlockInfos, visitCount, isOSRCatch);
      else
         {
         TR_BitVector storeVector(symRefTab()->getNumSymRefs(), comp()->trMemory(), stackAlloc);
         gatherLocalUseInfo(block, storeVector, &seenBlockInfos, visitCount, isOSRCatch);
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
      gatherLocalUseInfo(node->getChild(i), storeVector, visitCount, isOSRCatch);

   TR::SymbolReference * symRef = node->getOpCode().hasSymbolReference() ? node->getSymbolReference() : 0;

   if (symRef && symRef->getSymbol()->isAutoOrParm())
      {
      int32_t refNumber = symRef->getReferenceNumber();
      if (node->getOpCode().isStoreDirect())
         storeVector.set(refNumber);
      else if (!storeVector.get(refNumber))
         {
         catchLocalUseSymRefs().set(refNumber);
         if (!isOSRCatch)
            notOsrCatchLocalUseSymRefs().set(refNumber);
         }
      }
   }

bool
OMR::AliasBuilder::hasUseonlyAliasesOnlyDueToOSRCatchBlocks(TR::SymbolReference *symRef)
   {
   if (notOsrCatchLocalUseSymRefs().get(symRef->getReferenceNumber()))
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
      *aliases |= refinedNonIntPrimitiveArrayShadows();

   aliases->set(symRefTab()->getArrayShadowIndex(TR::Int8));
   aliases->set(symRefTab()->getArrayShadowIndex(TR::Int16));
   aliases->set(symRefTab()->getArrayShadowIndex(TR::Int32));
   aliases->set(symRefTab()->getArrayShadowIndex(TR::Int64));
   aliases->set(symRefTab()->getArrayShadowIndex(TR::Float));
   aliases->set(symRefTab()->getArrayShadowIndex(TR::Double));
   }

void
OMR::AliasBuilder::addAddressArrayShadows(TR_BitVector *aliases)
   {
   if(supportArrayRefinement)
      *aliases |= refinedAddressArrayShadows();

   aliases->set(symRefTab()->getArrayShadowIndex(TR::Address));
   }

void
OMR::AliasBuilder::addIntArrayShadows(TR_BitVector *aliases)
   {
   if(supportArrayRefinement)
      *aliases |= refinedIntArrayShadows();

   aliases->set(symRefTab()->getArrayShadowIndex(TR::Int32));
   }
