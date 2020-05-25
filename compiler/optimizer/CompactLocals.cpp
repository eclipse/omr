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

#include "optimizer/CompactLocals.hpp"

#include <stdint.h>
#include <stdlib.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/sparsrbit.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Array.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/HashTab.hpp"
#include "infra/IGNode.hpp"
#include "infra/InterferenceGraph.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/DataFlowAnalysis.hpp"


#define MAX_NUMBER_OF_LOCALS 2000
// upper bound is determined by numChunk (defined as uint16_t) of interferenceMatrix BitVector (2800^2 / 2 / 64 < 64K)
#define MAX_NUMBER_OF_LOCALS_PLX 2800
#define GROWING_FACTOR_IG 1.5

#define COMPACT_LOCALS_COMPLEXITY_LIMIT 1000000000

// Compact Locals
//
// Reduce local stack slot usage by having non-interfering locals share the same slot.
// Uses the graph colouring mechanism.
//
TR_CompactLocals::TR_CompactLocals(TR::OptimizationManager *manager)
   : _liveVars(NULL),
     _prevLiveVars(NULL),
     _temp(NULL),
     _localIndexToIGNode(NULL),
     _localsIG(NULL),
     TR::Optimization(manager)
   {
   }

int32_t TR_CompactLocals::perform()
   {
   if (!cg()->getSupportsCompactedLocals())
      {
      return 0;
      }

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // If register maps are not implemented, spill temps must be included in
   // the liveness analysis.
   //
   // TODO: do this only if trees have not been lowered already.
   //
   if (!comp()->useRegisterMaps())
      {
      cg()->lowerTrees();
      cg()->findAndFixCommonedReferences();
      }

   int32_t/*             i,*/ numLocals = 0;
   TR::AutomaticSymbol *p;
   ListIterator<TR::AutomaticSymbol> locals(&comp()->getMethodSymbol()->getAutomaticList());
   for (p = locals.getFirst(); p != NULL; p = locals.getNext())
      {
      ++numLocals;
      }

   static char *limitc = feGetEnv("TR_CompactLocalsLimit");

   int32_t limit = 5;
   if (limitc)
      {
      limit = atoi(limitc);
      }

   // Nothing to do if there are no locals
   //
   if (numLocals < limit)
      {
      return 0; // actual cost
      }

   int32_t hotnessFactor = 1;
   if (comp()->getMethodHotness() >= hot)
      hotnessFactor = 2;
   else if (comp()->getMethodHotness() >= hot)
      hotnessFactor = 4;

   bool canAffordAnalysis = true;
   if (((int64_t) comp()->getFlowGraph()->getNumberOfNodes() * numLocals * numLocals/ hotnessFactor) > COMPACT_LOCALS_COMPLEXITY_LIMIT)
      {
      if (!comp()->getOption(TR_ProcessHugeMethods))
        canAffordAnalysis = false;
      }

   if (!canAffordAnalysis)
      {
      return 0; // actual cost
      }

   // Perform liveness analysis
   //
   TR_Liveness liveLocals(comp(), optimizer(), comp()->getFlowGraph()->getStructure());

   TR_BitVector *referenceLocals;
   TR_BitVector *nonReferenceLocals;

   // Create a local index to node table and seed the interference graph.
   //
   _localIndexToIGNode = new (trStackMemory()) TR::vector<TR_IGNode*, TR::Region&>(
      numLocals,
      static_cast<TR_IGNode*>(NULL),
      comp()->trMemory()->currentStackRegion());

   _localsIG = new (trHeapMemory()) TR_InterferenceGraph(comp(), numLocals);
   referenceLocals = new (trStackMemory()) TR_BitVector(numLocals, trMemory(), stackAlloc);
   nonReferenceLocals = new (trStackMemory()) TR_BitVector(numLocals, trMemory(), stackAlloc);

   for (p = locals.getFirst(); p != NULL; p = locals.getNext())
      {
      p->setLocalIndex(0);

      if (eligibleLocal(p))
         {
         (*_localIndexToIGNode)[p->getLiveLocalIndex()] = _localsIG->add(p, true);

         // Avoid colouring any internal pointers or
         // pinning array autos
         //
         if (!p->isInternalPointer() &&
             !p->isPinningArrayPointer() &&
             !p->holdsMonitoredObject())
            {
            if (p->isCollectedReference())
               {
               referenceLocals->set(p->getLiveLocalIndex());
               }
            else
               {
               nonReferenceLocals->set(p->getLiveLocalIndex());
               }
            }
         }
      }

   if (trace())
      {
      _localsIG->dumpIG("initial graph");

      traceMsg(comp(), "SymInterferenceSets for %d locals\n", numLocals);
      traceMsg(comp(), "   %4d : ", referenceLocals->elementCount());
      referenceLocals->print(comp());
      traceMsg(comp(), "\n");
      traceMsg(comp(), "   %4d : ", nonReferenceLocals->elementCount());
      nonReferenceLocals->print(comp());
      traceMsg(comp(), "\n");
      }

   if (!referenceLocals->isEmpty() &&
       !nonReferenceLocals->isEmpty())
      {
      createInterferenceBetween(referenceLocals, nonReferenceLocals);
      }

   if (trace())
      {
      _localsIG->dumpIG("after initial size interferences");
      traceMsg(comp(), "after initial size interferences numChunks=%d\n", _localsIG->getInterferenceMatrix()->numChunks());
      }

   // Build the live on exit sets for each block and determine interferences between the
   // live ranges of all locals.
   //
   _liveVars = new (trStackMemory()) TR_BitVector(numLocals, trMemory(), stackAlloc);
   _prevLiveVars = new (trStackMemory()) TR_BitVector(numLocals, trMemory(), stackAlloc);
   _temp = new (trStackMemory()) TR_BitVector(numLocals, trMemory(), stackAlloc);

   vcount_t visitCount = comp()->incOrResetVisitCount();

   TR::TreeTop *firstTT, *tt;
   TR::Block   *block, *bb, *lastBlock, *succ;

   block = comp()->getStartBlock();
   while (block)
      {
      firstTT = block->getEntry();

      while ((bb = block->getNextBlock()) && bb->isExtensionOfPreviousBlock())
         block = bb;

      tt = block->getExit();
      lastBlock = block;

      if (trace())
         traceMsg(comp(), "Now in block_%d\n", block->getNumber());


      bool extendedByNextBlock = false;
      for (; tt != firstTT; tt = tt->getPrevTreeTop())
         {
         if (tt->getNode()->getOpCodeValue() == TR::BBStart)
            {
            extendedByNextBlock = block->isExtensionOfPreviousBlock() ? true : false;
            block = block->getPrevBlock();

            if (trace())
                traceMsg(comp(), "Now in block_%d\n", block->getNumber());

            }
         else if (tt->getNode()->getOpCodeValue() == TR::BBEnd)
            {
            // Compose the live-on-exit vector from the union of the live-on-entry
            // vectors of this block's successors.
            //
            if (!extendedByNextBlock)
               {
               _liveVars->empty();
               _prevLiveVars->empty();
               }

            for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
               {
               succ = toBlock((*edge)->getTo());
               *_liveVars |= *liveLocals._blockAnalysisInfo[succ->getNumber()];
               }

            for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end(); ++edge)
               {
               succ = toBlock((*edge)->getTo());
               *_liveVars |= *liveLocals._blockAnalysisInfo[succ->getNumber()];
               }

            if (trace())
               {
               traceMsg(comp(), "BB_End for block_%d: live vars = ", block->getNumber());
               _liveVars->print(comp());
               traceMsg(comp(), "\n");
               }

            createInterferenceBetween(_liveVars);
            }

         processNodeInPreorder(tt->getNode(), visitCount, &liveLocals, block, true);
         }

      if (trace())
         {
         traceMsg(comp(), "Computed entry vector: ");
         _liveVars->print(comp());
         traceMsg(comp(), "\nLiveness entry vector: ");
         liveLocals._blockAnalysisInfo[block->getNumber()]->print(comp());
         traceMsg(comp(), "\n");
         }

      TR_ASSERT(_localsIG->getNumNodes()>=MAX_NUMBER_OF_LOCALS || *_liveVars == *liveLocals._blockAnalysisInfo[block->getNumber()],
             "live-on-entry info does not match\n");

      block = lastBlock->getNextBlock();
      }

   doCompactLocals();

   return 10; // actual cost
   }


void TR_CompactLocals::processNodeInPreorder(TR::Node *node,
                                             vcount_t  visitCount,
                                             TR_Liveness *liveLocals,
                                             TR::Block    *block,
                                             bool         directChildOfTreeTop)
   {
   // First time this node has been encountered.
   //
   if (node->getVisitCount() != visitCount)
      {
      node->setVisitCount(visitCount);
      node->setLocalIndex(node->getReferenceCount());
      }

   if (trace())
      {
      traceMsg(comp(), "---> visiting tt node %p\n", node);
      }

   if (node->getOpCode().isStoreDirect() /* && directChildOfTreeTop */)
      {
      TR::AutomaticSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
      if (local && eligibleLocal(local))
         {
         int32_t localIndex = local->getLiveLocalIndex();

         TR_ASSERT(localIndex >= 0, "bad local index: %d\n", localIndex);

         if (!_liveVars->isSet(localIndex))
            {
            createInterferenceBetweenLocals(localIndex);
            }

         // This local is killed only if the live range of any loads of this symbol do not overlap
         // with this store.
         //
         if (local->getLocalIndex() == 0)
            {
            _liveVars->reset(localIndex);
            if (trace())
               {
               traceMsg(comp(), "--- local index %d KILLED\n", localIndex);
               }
            }
         }
      }
   else if (node->getOpCode().isLoadVarDirect() || node->getOpCodeValue() == TR::loadaddr)
      {
      TR::AutomaticSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
      if (local && eligibleLocal(local))
         {
         int32_t localIndex = local->getLiveLocalIndex();

         TR_ASSERT(localIndex >= 0, "bad local index: %d\n", localIndex);

         // First visit to this node.
         //
         if (node->getLocalIndex() == node->getReferenceCount())
            {
            local->setLocalIndex(local->getLocalIndex() + node->getReferenceCount());
            }

         if ((node->getLocalIndex() == 1 || node->getOpCodeValue() == TR::loadaddr) && !_liveVars->isSet(localIndex))
            {
            // First evaluation point of this node or loadaddr.
            //
            createInterferenceBetweenLocals(localIndex);
            _liveVars->set(localIndex);

            if (trace())
               {
               traceMsg(comp(), "+++ local index %d LIVE\n", localIndex);
               }
            }
         else if (node->getOpCodeValue() == TR::loadaddr)
            {
            createInterferenceBetweenLocals(localIndex);

            if (trace())
               {
               traceMsg(comp(), "+++ local index %d address taken\n", localIndex);
               }
            }

         local->setLocalIndex(local->getLocalIndex()-1);
         node->setLocalIndex(node->getLocalIndex()-1);

         return;
         }
      }
   else if (node->exceptionsRaised() &&
           (node->getLocalIndex() <= 1))
      {
      TR::Block *succ;
      for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end(); ++edge)
         {
         succ = toBlock((*edge)->getTo());
         *_liveVars |= *((*liveLocals)._blockAnalysisInfo[succ->getNumber()]);
         }

      *_temp = *_liveVars;
      *_temp -= *_prevLiveVars;

      if (!_temp->isEmpty())
         createInterferenceBetween(_liveVars);
      }

   if (node->getLocalIndex() != 0)
      {
      node->setLocalIndex(node->getLocalIndex()-1);
      }

   // This is not the first evaluation point of this node.
   //
   if (node->getLocalIndex() > 0)
      {
      return;
      }

   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      {
      processNodeInPreorder(node->getChild(i), visitCount, liveLocals, block, false);
      }
   }


// Add interferences between all elements of a bit vector.
//
void
TR_CompactLocals::createInterferenceBetween(TR_BitVector *bv)
   {
   *_prevLiveVars = *bv;

   TR_BitVectorIterator bvi(*bv), wbvi;
   TR_IGNode            *ig1, *ig2;
   int32_t              i1, i2;

   TR_BitVector *workBV = new (trStackMemory()) TR_BitVector(*bv);

   while (bvi.hasMoreElements())
      {
      i1 = bvi.getNextElement();

      workBV->reset(i1);

      wbvi.setBitVector(*workBV);
      while (wbvi.hasMoreElements())
         {
         i2 = wbvi.getNextElement();

         ig1 = (*_localIndexToIGNode)[i1];
         ig2 = (*_localIndexToIGNode)[i2];

         if (ig1 && ig2)
            {
            if (trace() && !_localsIG->hasInterference(ig1, ig2))
               {
               traceMsg(comp(), "Adding interference between %d and %d\n", i1, i2);
               }
            _localsIG->addInterferenceBetween(ig1, ig2);
            }
         }
      }
   }

// Add interferences between elements in different bit vectors.
//
void
TR_CompactLocals::createInterferenceBetween(TR_BitVector *bv1,
                                            TR_BitVector *bv2)
   {
   TR_BitVectorIterator  bvi1(*bv1), bvi2;
   TR_IGNode             *ig1, *ig2;
   int32_t               i1, i2;

   while (bvi1.hasMoreElements())
      {
      i1 = bvi1.getNextElement();
      bvi2.setBitVector(*bv2);
      while (bvi2.hasMoreElements())
         {
         i2 = bvi2.getNextElement();

         ig1 = (*_localIndexToIGNode)[i1];
         ig2 = (*_localIndexToIGNode)[i2];

         if (ig1 && ig2)
            {
            if (trace() && !_localsIG->hasInterference(ig1, ig2))
               {
               traceMsg(comp(), "Adding interference between %d and %d\n", i1, i2);
               }
            _localsIG->addInterferenceBetween(ig1, ig2);
            }
         }
      }
   }


void
TR_CompactLocals::createInterferenceBetweenLocals(int32_t localIndex)
   {

   // Add an interference between localIndex and all currently live locals.
   //
   int32_t               liveLocalIndex;
   TR_BitVectorIterator  lvi(*_liveVars);
   TR_IGNode             *ig1, *ig2;

   while (lvi.hasMoreElements())
      {
      liveLocalIndex = lvi.getNextElement();

      if (liveLocalIndex == localIndex)
         continue;

      ig1 = (*_localIndexToIGNode)[liveLocalIndex];
      ig2 = (*_localIndexToIGNode)[localIndex];

      if (ig1 && ig2)
         {
         if (trace() && !_localsIG->hasInterference(ig1, ig2))
            {
            traceMsg(comp(), "Adding interference between %d and %d\n", liveLocalIndex, localIndex);
            }
         _localsIG->addInterferenceBetween(ig1, ig2);
         }
      }
   }


bool TR_CompactLocals::eligibleLocal(TR::AutomaticSymbol * localSym)
   {
   if (localSym->getLiveLocalIndex() == INVALID_LIVENESS_INDEX)
      return false;

   if (localSym->isLocalObject())
      return false;

   if (_localsIG->getNumNodes() > MAX_NUMBER_OF_LOCALS && (*_localIndexToIGNode)[localSym->getLiveLocalIndex()] == NULL)
      return false;

   return true;
   }


void TR_CompactLocals::assignColorsToSymbols(TR_BitVector *bv)
   {
   ;
   }


void
TR_CompactLocals::doCompactLocals()
   {

   if (trace())
      {
      _localsIG->dumpIG("before colouring");
      }

   // The maximum degree of all nodes in the graph is the upper bound on the
   // number of slots required.
   //
   IGNodeDegree degree = _localsIG->findMaxDegree() + 1;

   if (!_localsIG->doColouring(degree))
      {
      TR_ASSERT(0, "Could not find a colouring!\n");
      }

   if (trace())
      {
      _localsIG->dumpIG("after colouring");
      traceMsg(comp(), "\nOOOO: Original num locals=%d, max locals required=%d, %s\n",
                  _localsIG->getNumNodes(), _localsIG->getNumberOfColoursUsedToColour(), comp()->signature());
      }

   // TODO: Only do this if we reduced the number of locals in the method.
   //
   //   cg()->setHasCompactedLocals();

   cg()->setLocalsIG(_localsIG);
   }

const char *
TR_CompactLocals::optDetailString() const throw()
   {
   return "O^O COMPACT LOCALS: ";
   }
