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

#include "optimizer/SinkStores.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/allocator.h"
#include "cs2/sparsrbit.h"
#include "env/IO.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/HashTab.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"

#define OPT_DETAILS "O^O SINK STORES: "

#define TR_USED_SYMBOLS_OPTION_1

// There are two ways to update a block with used symbols.
//
// 1. When a store is moved any savedLiveCommoned symbols that intersect with the moved stores used symbols are added to the used symbols
//    for the source block of the store
//    block 1:
//    istore
//       iload sym1  <savedLiveCommoned = (sym1,sym2) at this point, usedSymbols = (sym1) >
//
//    sink the istore to block 4
//    Update usedSymbolsInBlock[1] with (sym1,sym2) ^ (sym1) = sym1
//
// 2. Do not do the above but instead for any treetop/store that is not moved update the usedSymbolsInBlock for its block with any commoned symbols
//    block 1:
//    treetop
//       =>iload sym1
//
//    Update usedSymbolsInBlock[1] with sym1
//
//   Option 1 is the current default as it was the original scheme and is more tested.
//   Generally it is observed that with option 1 more stores are moved some distance but with option 2 less stores are moved a greater distance.

inline bool movedStorePinsCommonedSymbols()
   {
   #ifdef TR_USED_SYMBOLS_OPTION_1
   return true;
   #else
   return false;
   #endif
   }

// The bitvectors used to track commoned loads (savedLiveCommonedLoads,killedLiveCommonedLoads,myCommonedLoads etc)
// are not precise enough to deal with some cases where a particular store contains the same commoned symbol under different
// loads, for example:
//    treetop
//       iload sym1 <a>
//    treetop
//       ior
//          iload sym1 <b>
//          iconst
//    ...
//    istore
//       iand
//          =>ior
//          =>iload sym1 <a>
//
// The bitvectors alone only provides the info that the istore contains sym1 as a commoned load but not how many times it contains sym1.
// This is a problem because then detecting when a particular sym1 reference is killed by another store or when the first reference to a particular
// symbol is impossible using the bitvectors alone.
// While the bitvectors are useful as a hint to the commoned symbols a more precise mechanism is required to track exactly what commoned references
// a particular store contains.
// Each movable store will contain a list of commoned loads beneath it. The loads will be wrapped in a class to mark the load as satisfied and/or killed and
// the symIdx that the load references.
// A commoned load is satisifed when the first reference to this load has been seen (regardless of whether the store has been killed first).
// A commoned load is killed when a store to this symbol is seen before the load has been satisified.
// After visiting all the nodes every commoned load will have been satisfied and some may have been killed.
// The savedLiveCommoned bitvector can be more precisely refined to removed symbols when no active movable store contains a particular
// symbol as an unsatisfied commoned load.
// This refinement will cause fewer walks of the movable stores to occur.

// Currently under testing in trivialStoreSinking...
inline bool TR_SinkStores::enablePreciseSymbolTracking()
   {
   if (comp()->getOption(TR_EnableTrivialStoreSinking))
      return true;
   else
      return false;
   }

TR_LiveOnNotAllPaths::TR_LiveOnNotAllPaths(TR::Compilation * c, TR_Liveness * liveOnSomePaths, TR_LiveOnAllPaths * liveOnAllPaths)
   : _comp(c)
   {
   TR_LiveVariableInformation *liveVarInfo = liveOnSomePaths->getLiveVariableInfo();
   TR_ASSERT((liveVarInfo == liveOnAllPaths->getLiveVariableInfo()), "Possibly inconsistent live variable information");

   bool trace = comp()->getOption(TR_TraceLiveness);

   TR::CFG *cfg = comp()->getFlowGraph();
   _numNodes = cfg->getNextNodeNumber();
   int32_t arraySize = _numNodes * sizeof(TR_BitVector *);
   _inSetInfo =  (TR_BitVector **)trMemory()->allocateStackMemory(arraySize);
   _outSetInfo = (TR_BitVector **)trMemory()->allocateStackMemory(arraySize);
   memset(_inSetInfo, 0, arraySize);
   memset(_outSetInfo, 0, arraySize);

   _numLocals = liveVarInfo->numLocals();
   // compute LiveOnNotAllPaths = LiveOnSomePaths(Liveness) - LiveOnAllPaths
   for (TR::CFGNode *block = comp()->getFlowGraph()->getFirstNode(); block != NULL; block = block->getNext() )
      {
      int32_t b=block->getNumber();
      _inSetInfo[b] = new (trStackMemory()) TR_BitVector(_numLocals, trMemory(), stackAlloc);
      if (liveOnSomePaths->_blockAnalysisInfo[b])
         {
         *(_inSetInfo[b]) = *(liveOnSomePaths->_blockAnalysisInfo[b]);
         if (liveOnAllPaths->_blockAnalysisInfo[b])
            *(_inSetInfo[b]) -= *(liveOnAllPaths->_blockAnalysisInfo[b]);
         }

      TR_BitVector liveOnSomePathsOut(_numLocals, trMemory());
      TR_BitVector liveOnAllPathsOut(_numLocals, trMemory());
      TR_BitVector forcedLiveOnAllPathsSymbols(_numLocals, trMemory());
      liveOnAllPathsOut.setAll(_numLocals);

      // compute liveOnAllPaths at end of block
      for (auto edge = block->getSuccessors().begin(); edge != block->getSuccessors().end(); ++edge)
         {
         TR::CFGNode *toBlock = (*edge)->getTo();
         int32_t toBlockNumber = toBlock->getNumber();

         if (liveOnAllPaths->_blockAnalysisInfo[toBlockNumber])
            liveOnAllPathsOut &= *(liveOnAllPaths->_blockAnalysisInfo[toBlockNumber]);
         else
            liveOnAllPathsOut.empty();

         if (liveOnSomePaths->_blockAnalysisInfo[toBlockNumber])
            {
            liveOnSomePathsOut |= *(liveOnSomePaths->_blockAnalysisInfo[toBlockNumber]);
            int isBackEdge = (block->getForwardTraversalIndex() >= toBlock->getForwardTraversalIndex());
            if (isBackEdge)
               {
               // Ensure we don't try to move things that are only live on the back edge
               // By adding all symbols live on the back edge to the set of symbols that
               // are live on ALL paths at this block's exit, we prevent any store to those
               // symbols from moving lower than this block
               if (trace)
                  traceMsg(comp(), "    Adding backedge live vars from block_%d to LiveOnAllPaths for block_%d\n", toBlockNumber, b);
               forcedLiveOnAllPathsSymbols |= *(liveOnSomePaths->_blockAnalysisInfo[toBlockNumber]);
               }
            }
         }
      liveOnAllPathsOut |= forcedLiveOnAllPathsSymbols;
      // Adjustment 1
      // these two are more important because it should stop stores from flowing through this block
      (*liveOnAllPaths->_blockAnalysisInfo[b]) |= forcedLiveOnAllPathsSymbols;
      (*_inSetInfo[b]) -= forcedLiveOnAllPathsSymbols;

      // finally, compute LiveOnNotAllPaths at end of block
      _outSetInfo[b] = new (trStackMemory()) TR_BitVector(_numLocals, trMemory(), stackAlloc);
      *(_outSetInfo[b]) = liveOnSomePathsOut;
      *(_outSetInfo[b]) -= liveOnAllPathsOut;

      // Adjustment 2
      // for non-exception edges: recompute LOAP_in
      // for a local if LOAP_out (without exception edges) is true and LOSP_in
      // is true, then LONAP_in should be false and thus LOAP_in should be forced to true
      TR_BitVector LiveOnAllPathsNonExcIn(_numLocals, trMemory());
      LiveOnAllPathsNonExcIn = liveOnAllPathsOut;
      LiveOnAllPathsNonExcIn &= *(liveOnSomePaths->_blockAnalysisInfo[b]);
      *(_inSetInfo[b]) -= LiveOnAllPathsNonExcIn;
      *(liveOnAllPaths->_blockAnalysisInfo[b]) |= LiveOnAllPathsNonExcIn;

      if (trace)
         {
         traceMsg(comp(), "Block %d:\n", b);
         traceMsg(comp(), "  Liveness IN: ");
         liveOnSomePaths->_blockAnalysisInfo[b]->print(comp());
         traceMsg(comp(), " OUT ");
         liveOnSomePathsOut.print(comp());
         traceMsg(comp(), "\n  LiveOnAllPaths IN: ");
         liveOnAllPaths->_blockAnalysisInfo[b]->print(comp());
         traceMsg(comp(), " OUT ");
         liveOnAllPathsOut.print(comp());
         traceMsg(comp(), "\n  LiveOnNotAllPaths IN: ");
         _inSetInfo[b]->print(comp());
         traceMsg(comp(), " OUT ");
         _outSetInfo[b]->print(comp());
         traceMsg(comp(), "\n");
         }
      }
   }


ListElement<TR_BlockListEntry> *TR_OrderedBlockList::addInTraversalOrder(TR::Block * block, bool forward, TR::CFGEdge *edge)
   {
   ListElement<TR_BlockListEntry> *ptr = _pHead;
   ListElement<TR_BlockListEntry> *prevPtr = NULL;
   int32_t blockTraversalIndex = (forward) ? block->getForwardTraversalIndex() : block->getBackwardTraversalIndex();

   TR_ASSERT(edge != NULL, "edge shouldn't be null in addInTraversalOrder");
   while (ptr != NULL)
      {
      TR_BlockListEntry *ptrBlockEntry = ptr->getData();
      TR::Block *ptrBlock = ptrBlockEntry->_block;

      // if block is already in list: add pred to its list of preds, increment its count, and return it
      if (ptrBlock == block)
         {
         ptrBlockEntry->_preds.add(edge);
         ptrBlockEntry->_count++;
         return ptr;
         }

      // otherwise, see if we should put it here
      int32_t ptrTraversalIndex = (forward) ? ptrBlock->getForwardTraversalIndex() : ptrBlock->getBackwardTraversalIndex();
      //traceMsg(comp(), "addInTraversalOrder: comparing block_%d (%d) to list block_%d (%d)\n", block->getNumber(), blockTraversalIndex, ptrBlock->getNumber(), ptrTraversalIndex);
      if (blockTraversalIndex < ptrTraversalIndex)
         // block is earlier in traversal, so insert it here
         break;

      prevPtr = ptr;
      ptr = ptr->getNextElement();
      }

   return addAfter((new (getRegion()) TR_BlockListEntry(block, edge, getRegion())), prevPtr);
   }


bool TR_OrderedBlockList::removeBlockFromList(TR::Block *block, TR::CFGEdge *edge)
   {
   ListElement<TR_BlockListEntry> *ptr = _pHead;
   ListElement<TR_BlockListEntry> *prevPtr = NULL;

   while (ptr != NULL)
      {
      TR_BlockListEntry *ptrBlockEntry = ptr->getData();
      TR::Block *ptrBlock = ptrBlockEntry->_block;

      // if block is already in list: remove pred from its list of preds, decrement its count, remove it from the list if resultant count is 0
      if (ptrBlock == block &&
          ptrBlockEntry->_preds.remove(edge))
         {
         ptrBlockEntry->_count--;
         if (ptrBlockEntry->_count == 0)
            {
            removeNext(prevPtr);
            return true;
            }
         }
      prevPtr = ptr;
      ptr = ptr->getNextElement();
      }
   return false;
   }

TR_MovableStore::TR_MovableStore(TR_SinkStores *s, TR_UseOrKillInfo *useOrKillInfo,
                int32_t symIdx,
                TR_BitVector *commonedLoadsUnderTree,
                TR_BitVector *commonedLoadsAfter,
                int32_t depth,
                TR_BitVector *needTempForCommonedLoads,
                TR_BitVector *satisfiedCommonedLoads) :
                   _useOrKillInfo(useOrKillInfo),
                   _s(s),
                   _comp(s->comp()),
                   _symIdx(symIdx),
                   _commonedLoadsUnderTree(commonedLoadsUnderTree),
                   _commonedLoadsAfter(commonedLoadsAfter),
                   _commonedLoadsList(NULL),
                   _commonedLoadsCount(0),
                   _satisfiedCommonedLoadsCount(0),
                   _depth(depth),
                   _needTempForCommonedLoads(needTempForCommonedLoads),
                   _movable(true),
                   _isLoadStatic(false)
   {
   _useOrKillInfo->_movableStore = this;
   if (_s->enablePreciseSymbolTracking() && _commonedLoadsUnderTree && !_commonedLoadsUnderTree->isEmpty())
      {
      _commonedLoadsList = new (trStackMemory()) List<TR_CommonedLoad>(trMemory());
      if (_s->trace())
         traceMsg(comp(),"      calling findCommonedLoads for node %p with visitCount %d\n",_useOrKillInfo->_tt->getNode(),comp()->getVisitCount()+1);
      _commonedLoadsCount = initCommonedLoadsList(_useOrKillInfo->_tt->getNode()->getFirstChild(),comp()->incVisitCount());
      if (_s->trace())
         traceMsg(comp(),"      found %d unique commonedLoads (_commonedLoadsUnderTree->elementCount() = %d\n",_commonedLoadsCount,_commonedLoadsUnderTree->elementCount());
      // A single symbol may be found in more than one load therefore commonedLoadsFound should be greater or equal to the number of
      // symbols set in _commonedLoadsUnderTree
      // NOTE: _commonedLoadsCount < _commonedLoadsUnderTree->elementCount() when a node is commoned within the tree but not outside of it
      // istore
      //   ixor
      //      iload sym1
      //      =>iload sym1
      // In this case the futureUseCount on the node is 0 so it isn't added to the _commonedLoadsList
      //TR_ASSERT(_commonedLoadsCount >= _commonedLoadsUnderTree->elementCount(),"did not find the right number of commonedLoads (found %d, actual %d)\n",_commonedLoadsCount,_commonedLoadsUnderTree->elementCount());
      ListIterator<TR_CommonedLoad> it(_commonedLoadsList);
      TR_CommonedLoad *load;
      if (_s->trace())
         {
         traceMsg(comp(),"      for store %p found the commoned load nodes\n",_useOrKillInfo->_tt->getNode());
         for (load=it.getFirst(); load != NULL; load = it.getNext())
            {
            traceMsg(comp(),"         load = %p with symIdx %d\n",load->getNode(),_s->getSinkableSymbol(load->getNode())->getLiveLocalIndex());
            }
         }
      }
   }
int32_t
TR_MovableStore::initCommonedLoadsList(TR::Node *node, vcount_t visitCount)
   {
   int32_t increment = 0;
   vcount_t oldVisitCount = node->getVisitCount();
   if (oldVisitCount == visitCount)
      return increment;
   node->setVisitCount(visitCount);

   if (node->getOpCode().isLoadVarDirect() && node->getOpCode().hasSymbolReference())
      {
      TR::RegisterMappedSymbol *local = _s->getSinkableSymbol(node);
      //TR_ASSERT(local,"invalid local symbol\n");
      if (!local)
         return increment;
      int32_t symIdx = local->getLiveLocalIndex();

      if (node->getFutureUseCount() > 0 && symIdx != INVALID_LIVENESS_INDEX)
         {
         TR_ASSERT(_commonedLoadsUnderTree->isSet(symIdx),"symIdx %d should be set in _commonedLoadsUnderTree\n",symIdx);
         _commonedLoadsList->add(new (trStackMemory()) TR_CommonedLoad(node, symIdx));
         increment++;
         }
      }

   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      increment+=initCommonedLoadsList(node->getChild(i),visitCount);
      }
   return increment;
   }

bool
TR_MovableStore::containsKilledCommonedLoad(TR::Node *node)
   {
   if (!_commonedLoadsList)
      return false;
//   TR_ASSERT(_commonedLoadsList,"_commonedLoadsList is NULL\n");
   for (ListElement<TR_CommonedLoad> *le = _commonedLoadsList->getListHead(); le; le = le->getNextElement())
      if (node == le->getData()->getNode() && le->getData()->isKilled())
         return true;
   return false;
   }
bool
TR_MovableStore::containsSatisfiedAndNotKilledCommonedLoad(TR::Node *node)
   {
   TR_ASSERT(_commonedLoadsList,"_commonedLoadsList is NULL\n");
   for (ListElement<TR_CommonedLoad> *le = _commonedLoadsList->getListHead(); le; le = le->getNextElement())
      //if (node == le->getData()->getNode() && le->getData()->isSatisfied())
      if (node == le->getData()->getNode() && le->getData()->isSatisfied() && !le->getData()->isKilled())
         return true;
   return false;
   }
bool
TR_MovableStore::containsCommonedLoad(TR::Node *node)
   {
   if (!_commonedLoadsList)
      return false;
   for (ListElement<TR_CommonedLoad> *le = _commonedLoadsList->getListHead(); le; le = le->getNextElement())
      if (node == le->getData()->getNode())
         return true;
   return false;
   }

TR_CommonedLoad*
TR_MovableStore::getCommonedLoad(TR::Node *node)
   {
   TR_ASSERT(_commonedLoadsList,"_commonedLoadsList is NULL\n");
   for (ListElement<TR_CommonedLoad> *le = _commonedLoadsList->getListHead(); le; le = le->getNextElement())
      if (node == le->getData()->getNode())
         return le->getData();
   return NULL;
   }

bool
TR_MovableStore::satisfyCommonedLoad(TR::Node *node)
   {
   if (areAllCommonedLoadsSatisfied())
      return false;
   for (ListElement<TR_CommonedLoad> *le = _commonedLoadsList->getListHead(); le; le = le->getNextElement())
      {
      TR_CommonedLoad *commonedLoad = le->getData();
      if ((node == commonedLoad->getNode()) && !commonedLoad->isSatisfied())
         {
         if (_s->trace())
            traceMsg(comp(),"      satisfyCommonedLoad (store %p) symIdx %d setting commonedLoad %p with node %p satisfied (isKilled = %d, isSatisfied = %d)\n",this->_useOrKillInfo->_tt->getNode(),commonedLoad->getSymIdx(),commonedLoad,commonedLoad->getNode(),commonedLoad->isKilled(),commonedLoad->isSatisfied());
         commonedLoad->setIsSatisfied();
         _satisfiedCommonedLoadsCount++;
         return true;
         }
      }
   return false;
   }

bool
TR_MovableStore::areAllCommonedLoadsSatisfied()
   {
   // should never have satisfied more commoned loads then exist under the store, must have made too many calls to satisfyCommonedLoad
   TR_ASSERT(_satisfiedCommonedLoadsCount <= _commonedLoadsCount,"_satisfiedCommonedLoadsCount (%d) > _commonedLoadsCount (%d)\n",_satisfiedCommonedLoadsCount,_commonedLoadsCount);
   return (_satisfiedCommonedLoadsCount == _commonedLoadsCount);
   }

bool
TR_MovableStore::killCommonedLoadFromSymbol(int32_t symIdx)
   {
   if (areAllCommonedLoadsSatisfied())
      return false;
   bool foundSymbol = false;
   for (ListElement<TR_CommonedLoad> *le = _commonedLoadsList->getListHead(); le; le = le->getNextElement())
      {
      TR_CommonedLoad *commonedLoad = le->getData();
      if (!commonedLoad->isSatisfied() && (commonedLoad->getSymIdx() == symIdx))
         {
         if (_s->trace())
            traceMsg(comp(),"      killCommonedLoadFromSymbol (store %p) symIdx %d setting commonedLoad %p with node %p killed\n",this->_useOrKillInfo->_tt->getNode(),symIdx,commonedLoad,commonedLoad->getNode());
         commonedLoad->setIsKilled();
         foundSymbol = true;
         }
      }
   return foundSymbol;
   }

bool
TR_MovableStore::containsUnsatisfedLoadFromSymbol(int32_t symIdx)
   {
   if (areAllCommonedLoadsSatisfied())
      return false;

   for (ListElement<TR_CommonedLoad> *le = _commonedLoadsList->getListHead(); le; le = le->getNextElement())
      {
      TR_CommonedLoad *commonedLoad = le->getData();
      if (!commonedLoad->isSatisfied() && (commonedLoad->getSymIdx() == symIdx))
         {
         return true;
         }
      }
   return false;
   }

TR_SinkStores::TR_SinkStores(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
   _allEdgePlacements(trMemory()),
   _allBlockPlacements(trMemory()),
   _indirectLoadAnchors(NULL),
   _indirectLoadAnchorMap(NULL),
   _firstUseOfLoadMap(NULL)
   {
   _tempSymMap = new (trHeapMemory()) TR_HashTab(comp()->trMemory(), stackAlloc, 4);

   // tuning parameters
   _sinkAllStores = false;
   _printSinkStoreStats = false;
   _sinkThruException = false;
   _firstSinkOptTransformationIndex = -1;
   _lastSinkOptTransformationIndex = -1;

   static const char *sinkAllStoresEnv = feGetEnv("TR_SinkAllStores");
   static const char *printSinkStoreStatsEnv = feGetEnv("TR_PrintSinkStoreStats");
   static const char *sinkThruExceptionEnv = feGetEnv("TR_SinkThruException");
   static const char *firstSinkOptTransformationIndexEnv = feGetEnv("TR_FirstSinkOptTransformationIndex");
   static const char *lastSinkOptTransformationIndexEnv = feGetEnv("TR_LastSinkOptTransformationIndex");

   if (sinkAllStoresEnv)
      _sinkAllStores = true;
   if (printSinkStoreStatsEnv)
      _printSinkStoreStats = true;
   if (sinkThruExceptionEnv)
      _sinkThruException = true;
   if (firstSinkOptTransformationIndexEnv)
      _firstSinkOptTransformationIndex = atoi(firstSinkOptTransformationIndexEnv);
   if (lastSinkOptTransformationIndexEnv)
      _lastSinkOptTransformationIndex = atoi(lastSinkOptTransformationIndexEnv);
   if (manager->comp()->getOptions()->getStoreSinkingLastOpt() != -1)
      {
      _firstSinkOptTransformationIndex = 0;
      _lastSinkOptTransformationIndex = manager->comp()->getOptions()->getStoreSinkingLastOpt();
      }
   }

TR_TrivialSinkStores::TR_TrivialSinkStores(TR::OptimizationManager *manager)
   : TR_SinkStores(manager)
   {
   setUsesDataFlowAnalysis(false);
   setSinkMethodMetaDataStores(true);
   setSinkStoresWithIndirectLoads(true);
   setExceptionFlagIsSticky(false);
   setSinkStoresWithStaticLoads(false);
   _nodesClonedForSideExitDuplication = new (trStackMemory()) TR_HashTab(comp()->trMemory(), stackAlloc, comp()->getFlowGraph()->getNextNodeNumber()/4);
   }

TR_GeneralSinkStores::TR_GeneralSinkStores(TR::OptimizationManager *manager)
   : TR_SinkStores(manager)
   {
   setUsesDataFlowAnalysis(true);
   setSinkMethodMetaDataStores(false);
   setSinkStoresWithIndirectLoads(false);
   setExceptionFlagIsSticky(true);
   setSinkStoresWithStaticLoads(true);
   }

TR_EdgeInformation *TR_SinkStores::findEdgeInformation(TR::CFGEdge *edge, List<TR_EdgeInformation> & edgeList)
   {
   ListIterator<TR_EdgeInformation> edgeInfoIt(&edgeList);
   TR_EdgeInformation *edgeInfo;
   for (edgeInfo=edgeInfoIt.getFirst(); edgeInfo != NULL; edgeInfo = edgeInfoIt.getNext())
      if (edgeInfo->_edge == edge)
         break;

   return edgeInfo;
   }

void TR_SinkStores::recordPlacementForDefAlongEdge(TR_EdgeStorePlacement *edgePlacement)
   {
   TR_EdgeInformation *edgeInfo=edgePlacement->_edges.getListHead()->getData();
   TR::CFGEdge *succEdge=edgeInfo->_edge;
   int32_t toBlockNumber=succEdge->getTo()->getNumber();

   TR_StoreInformation *storeInfo = edgePlacement->_stores.getListHead()->getData();
   TR::TreeTop *tt=storeInfo->_store;
   bool copyStore=storeInfo->_copy;

   if (trace())
      traceMsg(comp(), "            RECORD placement along edge (%d->%d), for tt [" POINTER_PRINTF_FORMAT "] (copy=%d)\n", succEdge->getFrom()->getNumber(), succEdge->getTo()->getNumber(), tt, copyStore);

   // for now, just put one edge in each placement...but collect all stores that go with that edge
   // but, if we find an edge already in the list, just add this store to the list of stores for that edge
   ListElement<TR_EdgeStorePlacement> *ptr = NULL;
   if (_placementsForEdgesToBlock[toBlockNumber] != NULL)
      {
      ptr = _placementsForEdgesToBlock[toBlockNumber]->getListHead();
      while (ptr != NULL)
         {
         TR_EdgeStorePlacement *placement = ptr->getData();
         TR_EdgeInformation *placementEdgeInfo = findEdgeInformation(succEdge, placement->_edges);
         if (placementEdgeInfo != NULL)
            {
            if (trace())
               traceMsg(comp(), "                adding tt to stores on this edge\n");

            placement->_stores.add(storeInfo);
            // NOTE that since stores are encountered in backwards order and "add" just puts new stores at the beginning of the list
            // then when we ultimately place the stores on this edge, they will be stored in the correct order (i.e. a store X that
            // originally appeared before another store Y in the code will be on _placement->_stores list in the order X, Y

            // record what symbols are referenced along this edge
            (*placementEdgeInfo->_symbolsUsedOrKilled) |= (*_usedSymbolsToMove);
            (*placementEdgeInfo->_symbolsUsedOrKilled) |= (*_killedSymbolsToMove);

            break;
            }
         ptr = ptr->getNextElement();
         }
      }

   if (ptr == NULL)
      {
      // we didn't find this edge in the list already, so add it
      if (trace())
         traceMsg(comp(), "                edge isn't in list already\n");
      TR::Block *from=succEdge->getFrom()->asBlock();
      if (from->isGotoBlock(comp()))
         {
         if (trace())
            traceMsg(comp(), "                from block_%d is a goto block\n", from->getNumber());
         TR_BlockStorePlacement *newBlockPlacement = new (trStackMemory()) TR_BlockStorePlacement(storeInfo, from, trMemory());
         recordPlacementForDefInBlock(newBlockPlacement);
         }
      else
         {
         edgeInfo->_symbolsUsedOrKilled = new (trStackMemory()) TR_BitVector(_liveVarInfo->numLocals(), trMemory());
         (*edgeInfo->_symbolsUsedOrKilled) |= (*_usedSymbolsToMove);
         (*edgeInfo->_symbolsUsedOrKilled) |= (*_killedSymbolsToMove);
         _allEdgePlacements.add(edgePlacement);
	 optimizer()->setRequestOptimization(OMR::basicBlockOrdering, true);

         if (_placementsForEdgesToBlock[toBlockNumber] == NULL)
            _placementsForEdgesToBlock[toBlockNumber] = new (trStackMemory()) TR_EdgeStorePlacementList(trMemory());
         _placementsForEdgesToBlock[toBlockNumber]->add(edgePlacement);
         }
      }
   }

void TR_SinkStores::recordPlacementForDefInBlock(TR_BlockStorePlacement *blockPlacement)
   {
   TR::Block *block=blockPlacement->_block;
   int32_t blockNumber=block->getNumber();

   TR_StoreInformation *storeInfo = blockPlacement->_stores.getListHead()->getData();
   TR::TreeTop *tt=storeInfo->_store;
   bool copyStore=storeInfo->_copy;

   if (trace())
      traceMsg(comp(), "            RECORD placement at beginning of block_%d for tt [" POINTER_PRINTF_FORMAT "] (copy=%d)\n", block->getNumber(), tt, (int32_t)copyStore);

   ListElement<TR_BlockStorePlacement> *ptr=NULL;
   if (_placementsForBlock[blockNumber] != NULL)
      {
      ptr = _placementsForBlock[blockNumber]->getListHead();
      while (ptr != NULL)
         {
         TR_BlockStorePlacement *placement = ptr->getData();
         if (placement->_block == block)
            {
            placement->_stores.add(storeInfo);
            // NOTE that since stores are encountered in backwards order and "add" just puts new stores at the beginning of the list
            // then when we ultimately place the stores on this edge, they will be stored in the correct order (i.e. a store X that
            // originally appeared before another store Y in the code will be on _placement->_stores list in the order X, Y
            break;
            }
         ptr = ptr->getNextElement();
         }
      }
   else
      {
      _placementsForBlock[blockNumber] = new (trStackMemory()) TR_BlockStorePlacementList(trMemory());
      }

   if (ptr == NULL)
      {
      _allBlockPlacements.add(blockPlacement);
      _placementsForBlock[blockNumber]->add(blockPlacement);
      }

   if (usesDataFlowAnalysis())
      {
      // update {LOSP,LOAP,LONAP}_IN liveness sets because we placed the store in block
      // due to this placement, uses are now live on all paths, kills are no longer live
      (*_liveOnSomePaths->_blockAnalysisInfo[blockNumber]) -= (*_killedSymbolsToMove);
      (*_liveOnSomePaths->_blockAnalysisInfo[blockNumber]) |= (*_usedSymbolsToMove);

      (*_liveOnAllPaths->_blockAnalysisInfo[blockNumber]) -= (*_killedSymbolsToMove);
      (*_liveOnAllPaths->_blockAnalysisInfo[blockNumber]) |= (*_usedSymbolsToMove);

      (*_liveOnNotAllPaths->_inSetInfo[blockNumber]) -= (*_killedSymbolsToMove);
      }

   // add kills to block's kill set, uses to block's gen set
   // NOTE: we should already have visited these blocks, so we should only have to add these symbols to kills and uses
   //       so that later stores will not be pushed through this block
   TR_ASSERT(_symbolsKilledInBlock[blockNumber] != NULL, "_symbolsKilledInBlock[%d] should have been initialized!", blockNumber);
   //_symbolsKilledInBlock[blockNumber] = new (trStackMemory()) TR_BitVector(_liveVarInfo->numLocals(), trMemory());
   if (trace())
      {
      traceMsg(comp(),"updating symbolsKilled in recordPlacementForDefInBlock\n");
      traceMsg(comp(), "BEF  _symbolsKilledInBlock[%d]: ",blockNumber);
      _symbolsKilledInBlock[blockNumber]->print(comp());
      traceMsg(comp(), "\n");
      }
   (*_symbolsKilledInBlock[blockNumber]) |= (*_killedSymbolsToMove);

   if (trace())
      {
      traceMsg(comp(), "AFT _symbolsKilledInBlock[%d]: ",blockNumber);
      _symbolsKilledInBlock[blockNumber]->print(comp());
      traceMsg(comp(), "\n\n");
      }
   TR_ASSERT(_symbolsUsedInBlock[blockNumber] != NULL, "_symbolsUsedInBlock[%d] should have been initialized!", blockNumber);
   //_symbolsUsedInBlock[blockNumber] = new (trStackMemory()) TR_BitVector(_liveVarInfo->numLocals(), trMemory());
   if (trace())
      {
      traceMsg(comp(),"updating symbolsUsed in recordPlacementForDefInBlock\n");
      traceMsg(comp(), "BEF  _symbolsUsedInBlock[%d]: ",blockNumber);
      _symbolsUsedInBlock[blockNumber]->print(comp());
      traceMsg(comp(), "\n");
      }
   (*_symbolsUsedInBlock[blockNumber]) |= (*_usedSymbolsToMove);
   if (trace())
      {
      traceMsg(comp(), "AFT _symbolsUsedInBlock[%d]: ",blockNumber);
      _symbolsUsedInBlock[blockNumber]->print(comp());
      traceMsg(comp(), "\n\n");
      }
   }

int32_t TR_TrivialSinkStores::perform()
   {
   if (!comp()->getOption(TR_EnableTrivialStoreSinking))
      return 0;
   return performStoreSinking();
   }

int32_t TR_GeneralSinkStores::perform()
   {
   if (comp()->getOption(TR_DisableStoreSinking))
      {
      return 0;
      }
   return performStoreSinking();
   }
int32_t TR_SinkStores::performStoreSinking()
   {
   if (0 && trace())
      {
      comp()->dumpMethodTrees("Before Store Sinking");
      }

   _handlerIndex = comp()->getCurrentMethod()->numberOfExceptionHandlers();
   _numPlacements = 0;
   _numRemovedStores = 0;
   _numTemps = 0;
   _searchMarkCalls = 0;
   _searchMarkWalks = 0;
   _killMarkWalks = 0;
   _numFirstUseAnchors = 0;
   _numIndirectLoadAnchors = 0;
   _numTransformations = 0;

   TR::CFG *cfg = comp()->getFlowGraph();
   TR_Structure *rootStructure = cfg->getStructure();
   int32_t numBlocks = cfg->getNextNodeNumber();

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   // create forward and backward RPO traversal ordering to control block visitations and detect back edges
   cfg->createTraversalOrder(true, stackAlloc);
   if (0 && trace())
      {
      traceMsg(comp(), "Forward traversal:\n");
      for (int16_t i=0;i < cfg->getForwardTraversalLength();i++)
         traceMsg(comp(), "\t%d (%d)\n", cfg->getForwardTraversalElement(i)->getNumber(), cfg->getForwardTraversalElement(i)->getForwardTraversalIndex());
      }

   cfg->createTraversalOrder(false, stackAlloc);
   if (0 && trace())
      {
      traceMsg(comp(), "Backward traversal:\n");
      for (int16_t i=0;i < cfg->getBackwardTraversalLength();i++)
         traceMsg(comp(), "\t%d (%d)\n", cfg->getBackwardTraversalElement(i)->getNumber(), cfg->getBackwardTraversalElement(i)->getBackwardTraversalIndex());
      }

   // Perform liveness analysis
   //
   _liveVarInfo = new (trStackMemory()) TR_LiveVariableInformation(comp(),
                                                                   optimizer(),
                                                                   rootStructure,
                                                                   false, /* !splitLongs */
                                                                   true,  /* includeParms */
                                                                   sinkMethodMetaDataStores());

   if (_liveVarInfo->numLocals() == 0)
      {
      return 1;
      }

   if (trace() && sinkMethodMetaDataStores())
      {
      ListIterator<TR::RegisterMappedSymbol> methodMetaDataSymbols(&comp()->getMethodSymbol()->getMethodMetaDataList());
      int32_t localCount = 0;
      for (auto *m = methodMetaDataSymbols.getFirst(); m != NULL; m = methodMetaDataSymbols.getNext())
         {
         TR_ASSERT(m->isMethodMetaData(), "should be method meta data");
         // If this assume fires than the order the method meta symbols were added in LiveVariableInformation has changed and
         // the mapping of live index to symbol name given below is wrong
         TR_ASSERT(m->getLiveLocalIndex() == localCount,"Index mismatch for MethodMetaDataSymbol therefore this tracing output is wrong\n");
         traceMsg(comp(), "Local #%2d is MethodMetaData symbol at %p : %s\n",localCount++,m,m->getName());
         }
      }

   _liveVarInfo->createGenAndKillSetCaches();
   if (!enablePreciseSymbolTracking())
      _liveVarInfo->trackLiveCommonedLoads();

   if (usesDataFlowAnalysis())
      {
      _liveOnSomePaths     = new (comp()->allocator()) TR_Liveness(comp(), optimizer(), rootStructure, false, _liveVarInfo, false, true);
      _liveOnAllPaths      = new (comp()->allocator()) TR_LiveOnAllPaths(comp(), optimizer(), rootStructure, _liveVarInfo, false, true);
      _liveOnNotAllPaths   = new (comp()->allocator()) TR_LiveOnNotAllPaths(comp(), _liveOnSomePaths, _liveOnAllPaths);

      bool thereIsACandidate = false;
      _candidateBlocks = new (trStackMemory()) TR_BitVector(numBlocks, trMemory());
      for (int32_t b=0; b < numBlocks;b++)
         {
         if (_liveOnNotAllPaths->_outSetInfo[b] != NULL && !_liveOnNotAllPaths->_outSetInfo[b]->isEmpty())
            {
            _candidateBlocks->set(b);
            thereIsACandidate = true;
            }
         }
      }

   //if (!thereIsACandidate)
      //return 1;

   int32_t numLocals = _liveVarInfo->numLocals();

   // these arrays of bit vectors describe the symbols used and killed in each block in the method
   int32_t arraySize = numBlocks * sizeof(void *);
   _symbolsUsedInBlock = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);
   memset(_symbolsUsedInBlock, 0, arraySize);
   _symbolsKilledInBlock = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);
   memset(_symbolsKilledInBlock, 0, arraySize);
//   _symbolsExceptionUsedInBlock = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);
//   memset(_symbolsExceptionUsedInBlock, 0, arraySize);
//   _symbolsExceptionKilledInBlock = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);
//   memset(_symbolsExceptionKilledInBlock, 0, arraySize);

   // these arrays are used to maintain the stores placed in each block or in edges directed at each particular block
   arraySize = numBlocks * sizeof(TR_BlockStorePlacementList *);
   _placementsForBlock = (TR_BlockStorePlacementList **) trMemory()->allocateStackMemory(arraySize);
   memset(_placementsForBlock, 0, arraySize);
   arraySize = numBlocks * sizeof(TR_EdgeStorePlacementList *);
   _placementsForEdgesToBlock = (TR_EdgeStorePlacementList **) trMemory()->allocateStackMemory(arraySize);
   memset(_placementsForEdgesToBlock, 0, arraySize);

   // some extra meta data is needed if stores with indirect loads are going to be sunk
   if (sinkStoresWithIndirectLoads())
      {
      // estimate that half the blocks will have a sinkable store with an indirect child
      if (trace())
         traceMsg(comp(),"creating _indirectLoadAnchorMap with initSize = %d\n",comp()->getFlowGraph()->getNextNodeNumber()/2);
      _indirectLoadAnchorMap = new (trStackMemory()) TR_HashTab(comp()->trMemory(), stackAlloc, comp()->getFlowGraph()->getNextNodeNumber()/2);
      if (trace())
         traceMsg(comp(),"creating _firstUseOfLoadMap with initSize = %d\n",comp()->getFlowGraph()->getNextNodeNumber()/4);
      _firstUseOfLoadMap = new (trStackMemory()) TR_HashTab(comp()->trMemory(), stackAlloc, comp()->getFlowGraph()->getNextNodeNumber()/4);
      _indirectLoadAnchors = new (trStackMemory()) ListHeadAndTail<TR_IndirectLoadAnchor>(trMemory());
      }

   // grab what symbols are used/killed/exception killed from the live variable info
   //
   // The below call to initializeGenAndKillSetInfo is likely not needed at all as it looks like _symbolsUsedInBlock
   // and _symbolsKilledInBlock are always initialized and set before they are used in lookForSinkableStores
   // anyway (after the block loop is done).  Having whole block info like this is probably not needed because we are
   // always interested in just what is below the current node and not in the whole block
   //
//   if (usesDataFlowAnalysis())
//      {
//      _liveVarInfo->initializeGenAndKillSetInfo(_symbolsUsedInBlock,
//                                                _symbolsKilledInBlock,
//                                                _symbolsExceptionUsedInBlock,
//                                                _symbolsExceptionKilledInBlock);
//      }

   // Set up the block nesting depths
   //
   if (rootStructure)
      {
      TR::CFGNode *node;
      for (node = cfg->getFirstNode(); node; node = node->getNext())
         {
         TR::Block *block = toBlock(node);
         int32_t nestingDepth = 0;
         if (block->getStructureOf() != NULL)
            block->getStructureOf()->setNestingDepths(&nestingDepth);
         //if (block->getFrequency() < 0)
         //   block->setFrequency(0);
         }
      }

   // set up the future use counts for all nodes
   vcount_t futureUseVisitCount = comp()->incVisitCount();
   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      tt->getNode()->initializeFutureUseCounts(futureUseVisitCount);

   // now we have all the information we need, go look for stores to sink!
   lookForSinkableStores();

   // if we found any, transform the code now
   doSinking();

   if (0 && trace())
      {
      comp()->dumpMethodTrees("After Store Sinking");
      }

    } // scope of the stack memory region

   optimizer()->enableAllLocalOpts();

   if (trace())
      {

      traceMsg(comp(),"  Removed %d stores\n", _numRemovedStores);
      traceMsg(comp(),"  Placed  %d stores\n", _numPlacements);
      traceMsg(comp(),"  Created %d temps\n", _numTemps);
      traceMsg(comp(),"  Created %d anchors of killed loads\n",_numFirstUseAnchors);
      traceMsg(comp(),"  Created %d anchors of indirect loads\n", _numIndirectLoadAnchors);
      traceMsg(comp(),"  Performed %d kill mark walks\n", _killMarkWalks);
      traceMsg(comp(),"  Performed %d search mark walks\n", _searchMarkWalks);
      traceMsg(comp(),"  Performed %d search mark calls\n", _searchMarkCalls);
/*
      printf("  Removed %d stores\n", _numRemovedStores);
      printf("  Placed  %d stores\n", _numPlacements);
      printf("  Created %d temps\n", _numTemps);
      printf("  Created %d anchors of killed loads\n",_numFirstUseAnchors);
      printf("  Created %d anchors of indirect loads\n", _numIndirectLoadAnchors);
      printf("  Performed %d kill mark walks\n", _killMarkWalks);
      printf(" Performed %d search mark walks\n", _searchMarkWalks);
      printf("  Performed %d search mark calls\n", _searchMarkCalls);
*/
      }

   if (_numTemps > 0)
      optimizer()->setAliasSetsAreValid(false);

   return 1;
   }


void TR_SinkStores::lookForSinkableStores()
   {
   int32_t numLocals = _liveVarInfo->numLocals();
   TR::CFG *cfg = comp()->getFlowGraph();

   // visit blocks in backward RPO so that we move later stores before earlier stores
   //  so that the earlier ones don't get stuck behind another store that will be
   //  moved out of the block
   // ALSO: visit entire EBBs at a time so that commoned references can be correctly captured
   if (trace())
      traceMsg(comp(), "Starting to look for stores to sink\n");

   List<TR_UseOrKillInfo> useOrKillInfoList(trMemory());
   ListElement<TR_UseOrKillInfo> *useOrKillInfoListPtr=NULL;

   List<TR_MovableStore> potentiallyMovableStores(trMemory());
   ListElement<TR_MovableStore> *storeListPtr=NULL;

   // for keeping track of dataflow for each tree
   TR_BitVector *savedLiveCommonedLoads = new (trStackMemory()) TR_BitVector(numLocals, trMemory());
   TR_BitVector *satisfiedLiveCommonedLoads = new (trStackMemory()) TR_BitVector(numLocals, trMemory());
   TR_BitVector *killedLiveCommonedLoads = new (trStackMemory()) TR_BitVector(numLocals, trMemory());

   // treeVisitCount will be incremented for each tree in an EBB
   // when starting a new EBB, treeVisitCount will be reset fo startVisitCount
   // if treeVisitCount > highestVisitCount, comp()->incVisitCount() will be invoked to maintain consistency
   vcount_t startVisitCount=comp()->incVisitCount();
   vcount_t highestVisitCount = startVisitCount;
   vcount_t treeVisitCount = startVisitCount - 1; // will be incremented inside loop

   // this bit vector is used to collect commoned loads for every traversal
   // it will be copied into a new bit vector for a store identified as potentially movable
   TR_BitVector *treeCommonedLoads = new (trStackMemory()) TR_BitVector(numLocals, trMemory());

   int32_t *satisfiedCommonedSymCount = (int32_t *)trMemory()->allocateStackMemory(numLocals*sizeof(int32_t));
   memset(satisfiedCommonedSymCount, 0, numLocals*sizeof(int32_t));

   for (int32_t i=0;i < cfg->getBackwardTraversalLength();i++)
      {
      TR::Block *block = cfg->getBackwardTraversalElement(i)->asBlock();
      int32_t blockNumber = block->getNumber();
      bool foundException = false;
      if (trace())
         traceMsg(comp(), "  Looking for stores to sink in block_%d\n", blockNumber);

      // walk trees in reverse order to (hopefully) create opportunities for earlier trees
      if (block->getEntry() != NULL)
         {
         TR::TreeTop *tt = block->getLastRealTreeTop();

         do
            {
            // FIXME: It is legal in trivialStoreSinking to reset the foundException flag at the start of each loop so
            // return false for isExceptionFlagIsSticky() (more sinking opportunities are found)
            // Is it also legal to reset the flag given the more general control flow found in generalStoreSinking?
            if (!isExceptionFlagIsSticky())
               foundException = false;
            int32_t prevNumTemps = _numTemps;
            TR::Node *node = tt->getNode();
            if (node->getOpCodeValue() == TR::treetop)
               node = node->getFirstChild();

            treeVisitCount++;
            if (treeVisitCount > highestVisitCount)
               {
               TR_ASSERT(treeVisitCount == highestVisitCount+1, "treeVisitCount skipped a value");
               highestVisitCount = comp()->incVisitCount();
               }

            if (node && node->getOpCodeValue() == TR::BBStart)
               {
               if (trace())
                  traceMsg(comp(), "    Found BBStart, at beginning of block\n");
               break;
               }

            if (trace())
               traceMsg(comp(), "    Examining node [" POINTER_PRINTF_FORMAT "] in block_%d\n", node,blockNumber);

            if (node->exceptionsRaised() && block->hasExceptionSuccessors())
               {
               if (trace())
                  traceMsg(comp(), "    Raises an exception in try region, cannot move any earlier trees\n");
               foundException = true;
               }

            if (!enablePreciseSymbolTracking())
               (*savedLiveCommonedLoads) = (*_liveVarInfo->liveCommonedLoads());

         if (trace())
               {
               traceMsg(comp(), "      savedLiveCommonedLoads: ");
               savedLiveCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               }
            // visit the tree below this node and find all the symbols it uses
            TR_BitVector *usedSymbols = new (trStackMemory()) TR_BitVector(numLocals, trMemory());
            TR_BitVector *killedSymbols = new (trStackMemory()) TR_BitVector(numLocals, trMemory());
            // remove any commoned loads where the first reference to the load has been seen (it is 'satisfied')
            // satisfiedLiveCommonedLoads was initialized, if required, on the previous node examined
         if (enablePreciseSymbolTracking())
            {
            if (trace())
               {
               traceMsg(comp(), "      savedLiveCommonedLoads before refining: ");
               savedLiveCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               }
            if (trace())
               {
               traceMsg(comp(), "      satisfiedLiveCommonedLoads: ");
               satisfiedLiveCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               }
            (*savedLiveCommonedLoads) -= (*satisfiedLiveCommonedLoads);
            if (trace())
               {
               traceMsg(comp(), "      savedLiveCommonedLoads after refining: ");
               savedLiveCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               }
            // must or in treeCommonedLoads *after* subtracting out satisfiedLiveCommonedLoads in case this store
            // contains both a first use and a commoned load from the same symbol
            (*savedLiveCommonedLoads) |= (*treeCommonedLoads);
            satisfiedLiveCommonedLoads->empty();
            }
         else
            {
            // remember the currently live commoned loads so that we can figure out later if we can move this store
            (*savedLiveCommonedLoads) = (*_liveVarInfo->liveCommonedLoads());
            }

            treeCommonedLoads->empty();
            if (trace())
            traceMsg(comp(),"      calling findLocalUses on node %p with treeVisitCount %d\n",tt->getNode(),treeVisitCount);
            _liveVarInfo->findLocalUsesInBackwardsTreeWalk(tt->getNode(), blockNumber,
                                                           usedSymbols, killedSymbols,
                                                           treeCommonedLoads, treeVisitCount);
            if (trace())
               {
               traceMsg(comp(), "      killedSymbols: ");
               killedSymbols->print(comp());
               traceMsg(comp(), "\n");
               traceMsg(comp(), "      usedSymbols: ");
               usedSymbols->print(comp());
               traceMsg(comp(), "      treeCommonedLoads: ");
               treeCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               }
            TR::RegisterMappedSymbol *local = NULL;
            int32_t symIdx = 0;
            if (node != NULL && node->getOpCode().isStoreDirect())
               {
               local = getSinkableSymbol(node);
               }

            // killedSymbols should have the same syms as _symbolsKilledInBlock[blockNumber] at this point
            if (local != NULL)
               {
               symIdx = local->getLiveLocalIndex();
               if (symIdx != INVALID_LIVENESS_INDEX)
                  {
                  TR_ASSERT(symIdx < numLocals, "Found store to local with larger index than my liveness information can deal with, idx = %d\n",symIdx);
                  if (trace())
                     traceMsg(comp(),      "      setting %d in killedSymbols\n",symIdx);
                  killedSymbols->set(symIdx);
                  }
               else
                  {
                  local = NULL;
                  }
               }

            // TR_MethodMetaData symbols are thread local and may be killed/used by any arbitrary symRef
            else if (sinkMethodMetaDataStores() &&
                     (node->getOpCode().isCallDirect() || node->getOpCode().isStore()) &&
                     node->getOpCode().hasSymbolReference())
               {
               TR::SymbolReference *symRef = node->getSymbolReference();

               TR::SparseBitVector usedef(comp()->allocator());
               symRef->getUseDefAliases(node->getOpCode().isCallDirect()).getAliases(usedef);

               if (!usedef.IsZero())
                  {
                  if  (trace())
                     {
                     traceMsg(comp(),"       usedef:  ");
                     (*comp()) << usedef << "\n";
                     }
                  if (1)
                     {
                     TR::SparseBitVector tmp(comp()->allocator());
                     usedef &= tmp; // TODO: avoid tmp
                     if (trace())
                        {
                        traceMsg(comp(),"       symRef [%d] on node " POINTER_PRINTF_FORMAT " implicitly kills symrefs:  ",symRef->getReferenceNumber(),node);
                        (*comp()) << usedef << "\n";
                        }
                     TR::SparseBitVector::Cursor aliasesCursor(usedef);
                     for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
                        {
                        TR::SymbolReference *killedSymRef = comp()->getSymRefTab()->getSymRef(aliasesCursor);
                        TR::RegisterMappedSymbol *killedSymbol = killedSymRef->getSymbol()->getMethodMetaDataSymbol();
                        if (killedSymbol->getLiveLocalIndex() < numLocals && killedSymbol->getLiveLocalIndex() != INVALID_LIVENESS_INDEX)
                           {
                           if (trace())
                              traceMsg(comp(),"       setting symIdx %d (from symRef %d) on killedSymbols\n",killedSymbol->getLiveLocalIndex(),killedSymRef->getReferenceNumber());
                           killedSymbols->set(killedSymbol->getLiveLocalIndex());
                           if (savedLiveCommonedLoads->get(killedSymbol->getLiveLocalIndex()))
                           {
                              if (trace())
                                 traceMsg(comp(),"      updating killedLiveCommonedLoads with savedLiveCommonedLoads, setting index %d\n",
                                                       killedSymbol->getLiveLocalIndex());
                              killedLiveCommonedLoads->set(killedSymbol->getLiveLocalIndex());
                           }
                           if (node->getOpCode().isCallDirect())
                              {
                              if (trace())
                                 traceMsg(comp(),"       setting symIdx %d (from symRef %d) on usedSymbols\n",killedSymbol->getLiveLocalIndex(),killedSymRef->getReferenceNumber());
                              usedSymbols->set(killedSymbol->getLiveLocalIndex());
                              }
                           }
                        }
                     }
                  // else if {} add intersections for other TR_MethodMetaData symbols here as needed
                  }
               }
            if (local &&
               (savedLiveCommonedLoads->get(symIdx)))
               {
               // this store kills the currently live commoned loads
               if (trace())
                  traceMsg(comp(),"      updating killedLiveCommonedLoads with savedLiveCommonedLoads, setting index %d\n",symIdx);
               killedLiveCommonedLoads->set(symIdx);
               }

            if (trace())
              {
               traceMsg(comp(), "      killedSymbols: ");
               killedSymbols->print(comp());
               traceMsg(comp(), "\n");
               traceMsg(comp(), "      usedSymbols: ");
               usedSymbols->print(comp());
               traceMsg(comp(), "\n");
               traceMsg(comp(), "      treeCommonedLoads: ");
               treeCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               traceMsg(comp(), "      savedLiveCommonedLoads: ");
               savedLiveCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               traceMsg(comp(), "      killedLiveCommonedLoads: ");
               killedLiveCommonedLoads->print(comp());
               traceMsg(comp(), "\n");
               if (local)
                  traceMsg(comp(), "      is store to local %d\n", symIdx);
               }

               // check to see if we can find a store to a local that is used later in the EBB via a commoned reference
               TR_BitVectorIterator bvi(*killedLiveCommonedLoads);
               while (bvi.hasMoreElements())
                  {
                  int32_t killedSymIdx = bvi.getNextElement();
                  if (trace())
                     traceMsg(comp(), "      symbol stores to a live commoned reference, searching stores below that load this symbol via commoned reference ...\n");
                  _killMarkWalks++;

                  // find any stores that use this symbol (via a commoned reference) and mark them unmovable
                  ListElement<TR_MovableStore> *storeElement=potentiallyMovableStores.getListHead();
                  while (storeElement != NULL)
                     {
                     TR_MovableStore *store = storeElement->getData();
                     if (trace())
                        {
                        traceMsg(comp(), "            examining store [" POINTER_PRINTF_FORMAT "]\n",store->_useOrKillInfo->_tt->getNode());
                        if (store->_movable &&
                            store->_commonedLoadsUnderTree &&
                            store->_commonedLoadsUnderTree->get(killedSymIdx))
                           {
                           traceMsg(comp(),"            store->containsUnsatisfedLoadFromSymbol(%d) = %d so %s temp\n",killedSymIdx,store->containsUnsatisfedLoadFromSymbol(killedSymIdx),store->containsUnsatisfedLoadFromSymbol(killedSymIdx) ? "needs":"doesn't need");
                           }
                        }
                     if (store->_movable &&
                         store->_commonedLoadsUnderTree &&
                         store->_commonedLoadsUnderTree->get(killedSymIdx) &&
                         (!enablePreciseSymbolTracking() || store->killCommonedLoadFromSymbol(killedSymIdx)))
                        {
                        if (store->_depth >= MIN_TREE_DEPTH_TO_MOVE)
                           {
                           if (!store->_needTempForCommonedLoads)
                              store->_needTempForCommonedLoads = new (trStackMemory()) TR_BitVector(numLocals, trMemory());
                           store->_needTempForCommonedLoads->set(killedSymIdx);
                           if (!enablePreciseSymbolTracking())
                              store->_commonedLoadsUnderTree->reset(killedSymIdx);
                           }
                        else
                           store->_movable = false;

                        if (trace())
                           {
                           if (!store->_movable)
                              traceMsg(comp(), "         marking store [" POINTER_PRINTF_FORMAT "] as unmovable because its tree depth is %d\n", store->_useOrKillInfo->_tt->getNode(), store->_depth);
                           else if (store->_needTempForCommonedLoads)
                              traceMsg(comp(), "         marking store [" POINTER_PRINTF_FORMAT "] as movable with temp because it has commoned reference used sym %d\n", store->_useOrKillInfo->_tt->getNode(), killedSymIdx);
                           }
                        }
                        storeElement = storeElement->getNextElement();
                     }
                  }
            //
            //b1:
            //  treetop
            //    iload sym1 <a>
            //
            //b2:
            //  istore sym1
            //
            //b3:
            //  treetop
            //     ior
            //       iload sym1 <b>
            //b4:
            //  istore sym2
            //     =>iload sym1 <a>
            //
            //
            // The goal of the code below is to identify cases like the above where the iload sym1 in b1 is the first use the commoned
            // sym1 in b4 (and to know that the iload sym1 in b3 is *not* the first use)
            // Precise first reference locations are required in case an anchor needs be created when replicating the istore in b4
            // to a location outside the current EBB.
            // The anchored value of sym1 must be from just above the iload reference in b1 because an incorrect anchor in b3 would cause
            // the wrong value to be used in the replicated store.
            //

            if (enablePreciseSymbolTracking() && usedSymbols->intersects(*savedLiveCommonedLoads))
               {
               TR_BitVector *firstRefsToCommonedSymbols = new (trStackMemory()) TR_BitVector(*usedSymbols);
               (*firstRefsToCommonedSymbols) &= (*savedLiveCommonedLoads);
               if (trace())
                  {
                  traceMsg(comp(), "         firstRefsToCommonedSymbols: ");
                  firstRefsToCommonedSymbols->print(comp());
                  traceMsg(comp(), "\n");
                  }
               if (!firstRefsToCommonedSymbols->isEmpty())
                  {
                  ListElement<TR_MovableStore> *storeElement=potentiallyMovableStores.getListHead();
                  _searchMarkWalks++;
                  TR_BitVectorIterator firstRefsItBefore(*firstRefsToCommonedSymbols);
                  // initialize the symbols indicies we care about to 0
                  while (firstRefsItBefore.hasMoreElements())
                     satisfiedCommonedSymCount[firstRefsItBefore.getNextElement()] = 0;
                  int32_t storeCount = 0;
                  while (storeElement != NULL)
                     {
                     TR_MovableStore *store = storeElement->getData();
                     if (store->_movable &&
                         store->_commonedLoadsUnderTree &&
                         store->_commonedLoadsUnderTree->intersects(*firstRefsToCommonedSymbols))
                        {
                        if (trace())
                           traceMsg(comp(), "            examining store for first uses [" POINTER_PRINTF_FORMAT "]\n",store->_useOrKillInfo->_tt->getNode());
                        TR_BitVector *firstRefsOfCommonedSymbolsUnderThisStore = new (trStackMemory()) TR_BitVector(*store->_commonedLoadsUnderTree);
                        (*firstRefsOfCommonedSymbolsUnderThisStore) &= (*firstRefsToCommonedSymbols);
                        _searchMarkCalls++;
                        searchAndMarkFirstUses(node,
                                               tt,
                                               store,
                                               block,
                                               firstRefsOfCommonedSymbolsUnderThisStore);

                        TR_BitVectorIterator bvi(*firstRefsOfCommonedSymbolsUnderThisStore);
                        while (bvi.hasMoreElements())
                           {
                           int32_t firstRefSymIdx = bvi.getNextElement();
                           if (!store->containsUnsatisfedLoadFromSymbol(firstRefSymIdx))
                              {
                              if (trace())
                                 traceMsg(comp(),"            symIdx %d now satisfied increment satisfiedCommonedSymCount %d -> %d\n",
                                    firstRefSymIdx,satisfiedCommonedSymCount[firstRefSymIdx],satisfiedCommonedSymCount[firstRefSymIdx]+1);
                              satisfiedCommonedSymCount[firstRefSymIdx]++;
                              }
                           else if (trace())
                              {
                              traceMsg(comp(),"            symIdx %d is not yet satisfied do increment satisfiedCommonedSymCount %d\n",
                                 firstRefSymIdx,satisfiedCommonedSymCount[firstRefSymIdx]);
                              }
                           }
                        if ((*firstRefsOfCommonedSymbolsUnderThisStore) != (*firstRefsToCommonedSymbols))
                           {
                           TR_BitVector *firstRefsOfCommonedSymbolsNotUnderThisStore = new (trStackMemory()) TR_BitVector(*firstRefsToCommonedSymbols);
                           (*firstRefsOfCommonedSymbolsNotUnderThisStore) -= (*firstRefsOfCommonedSymbolsUnderThisStore);
                           TR_BitVectorIterator bvi(*firstRefsOfCommonedSymbolsNotUnderThisStore);
                           // any firstRef symbols not under this store as a commoned load are trivially satisfied
                           while (bvi.hasMoreElements())
                              {
                              int32_t firstRefSymIdx = bvi.getNextElement();
                              if (trace())
                                 traceMsg(comp(),"            symIdx %d not under this store increment satisfiedCommonedSymCount %d -> %d\n",
                                    firstRefSymIdx,satisfiedCommonedSymCount[firstRefSymIdx],satisfiedCommonedSymCount[firstRefSymIdx]+1);
                              satisfiedCommonedSymCount[firstRefSymIdx]++;
                              }
                           }
                        }
                     else
                        {
                        // any firstRef symbols not under this store as a commoned load are trivially satisfied
                        TR_BitVectorIterator bvi(*firstRefsToCommonedSymbols);
                        while (bvi.hasMoreElements())
                           {
                           int32_t firstRefSymIdx = bvi.getNextElement();
                           if (trace())
                              traceMsg(comp(),"            symIdx %d not at all under this store increment satisfiedCommonedSymCount %d -> %d\n",
                                 firstRefSymIdx,satisfiedCommonedSymCount[firstRefSymIdx],satisfiedCommonedSymCount[firstRefSymIdx]+1);
                           satisfiedCommonedSymCount[firstRefSymIdx]++;
                           }
                        }
                     storeElement = storeElement->getNextElement();
                     storeCount++;
                     } // end while
                  // A symIdx can be removed from the savedLiveCommonedLoads bitvector (using satisfiedLiveCommonedLoads)
                  // when it no longer appears as unsatisfied in *any* store
                  // searchAndMarkFirstUses increments satisfiedCommonedSymCount[symIdx] in two cases:
                  // 1. a store does not contain any commoned reference to a particular symIdx
                  // 2. a store does contain a commoned reference to a particular symIdx but does not contain
                  //    any unsatisfied commoned loads from this symIdx
                  // If, after processing all the stores, a symIdx has been found satisfied (or not found at all)
                  // then set this symIdx in satisfiedLiveCommonedLoads. Before looking at the next treetop
                  // savedLiveCommonedLoads is refined by subtracting (taking the set difference) of satisfiedLiveCommonedLoads
                  // from savedLiveCommonedLoads
                  TR_BitVectorIterator firstRefsItAfter(*firstRefsToCommonedSymbols);
                  while (firstRefsItAfter.hasMoreElements())
                     {
                     int32_t firstRefSymIdx = firstRefsItAfter.getNextElement();
                     if (satisfiedCommonedSymCount[firstRefSymIdx] == storeCount)
                        {
                        if (trace())
                           traceMsg(comp(),"            satisfiedCommonedSymCount[%d] = storeCount = %d set in satisfiedLiveCommonedLoads\n",firstRefSymIdx,storeCount);
                        satisfiedLiveCommonedLoads->set(firstRefSymIdx);
                        }
                     else if (trace())
                        {
                        traceMsg(comp(),"            satisfiedCommonedSymCount[%d] = %d and storeCount = %d do not set in satisfiedLiveCommonedLoads\n",firstRefSymIdx,satisfiedCommonedSymCount[firstRefSymIdx],storeCount);
                        }

                     }
                  }
               }

            if (!enablePreciseSymbolTracking())
               {
               // When the commoned symbols are tracked precisely it is enough to run the above code that intersects used
               // with savedLiveCommonedLoads to find the first uses of symbols
               // With this information an anchor or temp store can be created when/if it is actually needed in during
               // sinkStorePlacement()
               bool usedSymbolKilledBelow = usedSymbols->intersects(*killedLiveCommonedLoads);
               if (usedSymbolKilledBelow)
                  {
                  TR_BitVector *firstRefsToKilledSymbols = new (trStackMemory()) TR_BitVector(*usedSymbols);
                  (*firstRefsToKilledSymbols) &= (*killedLiveCommonedLoads);
                  if (trace())
                     {
                     traceMsg(comp(), "         (non-commoned) uses of killed symbols: ");
                     firstRefsToKilledSymbols->print(comp());
                     traceMsg(comp(), "\n");
                     }
                  ListElement<TR_MovableStore> *storeElement=potentiallyMovableStores.getListHead();
                  while (storeElement != NULL)
                     {
                     TR_MovableStore *store = storeElement->getData();

                     if (store->_movable &&
                         store->_needTempForCommonedLoads)
                        {
                        if (store->_needTempForCommonedLoads->intersects(*firstRefsToKilledSymbols))
                           {
                           // looking at bit vectors alone may not be enough to guarantee that a temp should actually be placed here see
                           // the comment in isCorrectCommonedLoad for more information
                           TR_BitVector *commonedLoadsUsedAboveStore = new (trStackMemory()) TR_BitVector(*(store->_needTempForCommonedLoads));
                           (*commonedLoadsUsedAboveStore) &= (*firstRefsToKilledSymbols);
                           if (trace())
                              {
                              traceMsg(comp(),"            found store [" POINTER_PRINTF_FORMAT "] below that may require a temp\n",store->_useOrKillInfo->_tt->getNode());
                              traceMsg(comp(),"               killed live commoned loads used above store:");
                              commonedLoadsUsedAboveStore->print(comp());
                              traceMsg(comp(),"\n");
                              }
                           genStoreToTempSyms(tt,
                                              node,
                                              commonedLoadsUsedAboveStore,
                                              killedLiveCommonedLoads,
                                              store->_useOrKillInfo->_tt->getNode(),
                                              potentiallyMovableStores);
                           }
                        }
                     storeElement = storeElement->getNextElement();
                     }
                  }
               }

            bool canMoveStore=false;
            TR_BitVector *moveStoreWithTemps = 0;
            uint32_t indirectLoadCount = 0; // incremented by storeIsSinkingCandidate
            int32_t treeDepth = 0;
            bool isLoadStatic = false;
            if (local != NULL &&
                node->getOpCode().isStoreDirect())
               {
               TR::RegisterMappedSymbol *sym =  node->getSymbolReference()->getSymbol()->getMethodMetaDataSymbol();
               bool sinkWithIndirectLoads = sym && comp()->getOption(TR_SinkAllStores) && sinkStoresWithIndirectLoads();
                  canMoveStore = true;
               if (comp()->getOption(TR_SinkOnlyCCStores) && sym)
                  {
                 	if (trace())
                 	   traceMsg(comp(), "         only sinking cc stores and this store is to a %s symbol\n",sym->castToMethodMetaDataSymbol()->getName());
                  canMoveStore = false;
                  }
               // TODO use aliasing to block sinking AR symbol stores
               // It is not legal to sink AR symbols as exceptions points are implicit uses of these symbols
               else if (sym && sym->isMethodMetaData() && sym->castToMethodMetaDataSymbol()->getMethodMetaDataType() == TR_MethodMetaDataType_AR)
                  {
                  if (trace())
                     traceMsg(comp(),"         not sinking the AR symbol %s\n",sym->castToMethodMetaDataSymbol()->getName());
                  canMoveStore = false;
                  }
               else if (foundException)
                  {
                 	if (trace())
                 	   traceMsg(comp(), "         can't move store with side effect\n");
                  canMoveStore = false;
                  }
               else if (!storeIsSinkingCandidate(block, node, symIdx, sinkWithIndirectLoads, indirectLoadCount, treeDepth, isLoadStatic, treeVisitCount,highestVisitCount))   // treeDepth is initialized when call return
                  {
                  if (trace())
                     traceMsg(comp(), "         can't move store because it fails sinking candidate test\n");
                  canMoveStore = false;
                  }
               else if (treeDepth > MAX_TREE_DEPTH_TO_MOVE)
                  {
                  if (trace())
                     traceMsg(comp(), "         can't move store because the tree depth (%d) is too big (MAX_DEPTH = %d)\n", treeDepth, MAX_TREE_DEPTH_TO_MOVE);
                  canMoveStore = false;
                  }

               if (trace())
                  {
                  traceMsg(comp(),"         store %s interfering and candidate tests\n", canMoveStore ? "passes some" : "fails one or more");
                  }

               // if we move this store, we will probably change the value of a symbol used below it
               // unless commoned syms are replaced by temps (saved value of the commoned load)
               // FIXME:: what if treeCommonedLoads is killed below within EBB not BB)??
               if (canMoveStore &&
                   usedSymbols->intersects(*killedLiveCommonedLoads)
                   )
                  {
                  // simple store-load tree won't be considered because of the overhead of creating temp store
                  if (treeDepth >= MIN_TREE_DEPTH_TO_MOVE)
                     {
                     moveStoreWithTemps = new (trStackMemory()) TR_BitVector(*usedSymbols);
                     (*moveStoreWithTemps) &= (*killedLiveCommonedLoads);
                     if (trace())
                        traceMsg(comp(), "         moveStoreWithTemps = %s\n", moveStoreWithTemps ? "true" : "false");
                     }
                  else
                     {
                     canMoveStore = false;
                    if (trace())
                        traceMsg(comp(), "         store will not be moved because its tree depth of %d is too small (MIN_DEPTH = %d)\n", treeDepth, MIN_TREE_DEPTH_TO_MOVE );
                     }
                  }
               }

            /*** Create TR_UseOrKillInfo and TR_MovableStore and add to list ***/
            // TR_UseOrKillInfo is used to track the symbols used and/or killed by this tree
            // Note that separate lists are maintained for potentially movable stores and all other trees that alter
            // the used and killed symbols. This is done to avoid having to go through the typically much larger
            // used and killed list when we are only interested in the potentially movable stores
            TR_UseOrKillInfo *useOrKill = NULL;

            if (!usedSymbols->isEmpty() ||
                !killedSymbols->isEmpty() ||
                (!movedStorePinsCommonedSymbols() && !treeCommonedLoads->isEmpty()))
               {
               if (trace())
                  traceMsg(comp(), "      creating use or kill info on %s node [" POINTER_PRINTF_FORMAT "] to track kills and uses\n",
                                  canMoveStore? "movable":"non-movable", tt->getNode());

               useOrKill = new (trStackMemory()) TR_UseOrKillInfo(tt,
                                                                  block,
                                                                  usedSymbols,
                                                                  movedStorePinsCommonedSymbols() ? NULL : new (trStackMemory()) TR_BitVector(*treeCommonedLoads),
                                                                  indirectLoadCount,
                                                                  killedSymbols);
               useOrKillInfoListPtr = useOrKillInfoList.addAfter(useOrKill, useOrKillInfoListPtr);
               }

            // TR_MovableStore is used to track the specific information about the potential movable store so that
            // we can decide later on whether to move this store or not
            TR_MovableStore *movableStore = NULL;
            if (canMoveStore)
               {
               if (trace())
                  traceMsg(comp(), "      store is potentially movable, collecting commoned loads and adding to list\n");
               TR_BitVector *myCommonedLoads = treeCommonedLoads->isEmpty() ? 0 : new (trStackMemory()) TR_BitVector(*treeCommonedLoads);
               if (!enablePreciseSymbolTracking() && myCommonedLoads && moveStoreWithTemps)
                  (*myCommonedLoads) -= (*moveStoreWithTemps);
               TR_BitVector *commonedLoadsAfter = 0;
               if (!savedLiveCommonedLoads->isEmpty() && movedStorePinsCommonedSymbols())
                  {
                  commonedLoadsAfter = new (trStackMemory()) TR_BitVector(*savedLiveCommonedLoads);
                  (*commonedLoadsAfter) &= (*usedSymbols);
                  }
               if (enablePreciseSymbolTracking() && myCommonedLoads)
                  {
                  if (trace())
                     {
                     traceMsg(comp(), "      myCommonedLoads: ");
                     myCommonedLoads->print(comp());
                     traceMsg(comp(), "\n");
                     traceMsg(comp(), "      myCommonedLoads->isSet(%d) = %d\n",symIdx,myCommonedLoads->isSet(symIdx));
                     }
                  if (myCommonedLoads->isSet(symIdx))
                     {
                     // The current store contains the symbol it is storing to as a commmoned load
                     // therefore it must be marked as needing a temp store

                     if (!moveStoreWithTemps)
                        moveStoreWithTemps = new (trStackMemory()) TR_BitVector(numLocals, trMemory());
                     if (trace())
                        traceMsg(comp(), "      store kills its own commoned symbol (%d), mark the store as needing a temp load\n",symIdx);
                     moveStoreWithTemps->set(symIdx);
                     killedLiveCommonedLoads->set(symIdx); // otherwise the anchor/temp store is never created when the first ref is found
                     }
                  if (myCommonedLoads && !myCommonedLoads->isEmpty())
                     {
                     // The TR_MovableStore ctor populates its commonedLoadsList when myCommonedLoads is
                     // not empty. This travesal will alter the visit counts of the nodes it visits.
                     // So the tree walk above to discover nodes does not skip any nodes that were visited by TR_MovableStore ctor
                     // increment treeVisitCount and highestVisitCount here.
                     treeVisitCount++;
                     highestVisitCount++;
                     }
                  }

               movableStore = new (trStackMemory()) TR_MovableStore(this,
                                                                    useOrKill,
                                                                    symIdx,
                                                                    myCommonedLoads,
                                                                    commonedLoadsAfter,
                                                                    treeDepth,
                                                                    moveStoreWithTemps);

               if (isLoadStatic)
                  movableStore->_isLoadStatic = true;

               storeListPtr = potentiallyMovableStores.addAfter(movableStore, storeListPtr);
               }

            // FIXME:: should do this regardless the stored is sunk or not??
            if (movableStore == NULL)
               {
               if (local &&
                   treeCommonedLoads->get(symIdx))
                  {
                  if (trace())
                     traceMsg(comp(),"      updating killedLiveCommonedLoads with treeCommonedLoads, setting index %d\n",symIdx);
                  // this store kills the currently live commoned loads
                  killedLiveCommonedLoads->set(symIdx);
                  }
               }

            // remove any used symbols from the killedLiveCommonedLoads bit vector
            //   since the kill is below this use, even if we move a store higher up, this use will preserve he old value of the local
            //TR_BitVectorIterator bi(*usedSymbols);
            //for (int32_t usedSym=bi.getFirstElement(); bi.hasMoreElements(); usedSym=bi.getNextElement())
            //   killedLiveCommonedLoads->clear(usedSym);


            tt = tt->getPrevTreeTop();

            // must skip over any temp stores that were created for this treetop so we do not attempt
            // to sink these temp stores as well.
            // The number of temp stores created for this treetop is numTemps-prevNumTemps
            for (int32_t tempCount = prevNumTemps; tempCount < _numTemps; tempCount++)
               {
               if (trace())
                  traceMsg(comp(),"    skipping created temp store [" POINTER_PRINTF_FORMAT "] in backward treetop walk\n",tt->getNode());
               tt = tt->getPrevTreeTop();
               }
            }
         while (tt != block->getEntry());
         }

      // may need to update dataflow sets here :( ??
      TR_ASSERT(_symbolsKilledInBlock[blockNumber] == NULL, "_symbolsKilledInBlock should not have been initialized for block_%d!", blockNumber);
      _symbolsKilledInBlock[blockNumber] = new (trStackMemory()) TR_BitVector(numLocals, trMemory(), stackAlloc);
      TR_ASSERT(_symbolsUsedInBlock[blockNumber] == NULL, "_symbolsUsedInBlock should not have been initialized for block_%d!", blockNumber);
      _symbolsUsedInBlock[blockNumber] = new (trStackMemory()) TR_BitVector(numLocals, trMemory(), stackAlloc);

      if (!block->isExtensionOfPreviousBlock())
         {
         if (trace())
            {
            traceMsg(comp(), "  Reached beginning of extended basic block\n");
            if (potentiallyMovableStores.isEmpty())
               traceMsg(comp(), "  No movable store candidates identified during traversal\n");
            else
               traceMsg(comp(), "  Processing potentially movable stores identified during traversal:\n");
            }

         // now go through the list of potentially movable stores in this extended basic
         // block (a subset of the useOrKillInfoList) and try to sink the ones that are still movable
         bool movedPreviousStore = false;
         while (!useOrKillInfoList.isEmpty())
            {
            TR_UseOrKillInfo *useOrKill = useOrKillInfoList.popHead();
            bool movedStore = false;
            bool isStore = (useOrKill->_movableStore != NULL);
            bool isMovableStore = isStore && (useOrKill->_movableStore->_movable);
            TR::TreeTop *tt  = useOrKill->_tt;
            TR::Block *block = useOrKill->_block;
            int32_t blockNumber = block->getNumber();

            if (trace())
               {
               traceMsg(comp(), "    Candidate treetop [" POINTER_PRINTF_FORMAT "]", tt->getNode());

               if (isMovableStore && tt->getNode()->getOpCode().hasSymbolReference())
                  {
                  TR::RegisterMappedSymbol *sym = tt->getNode()->getSymbolReference()->getSymbol()->getMethodMetaDataSymbol();
                  if (sym)
                     {
                     traceMsg(comp(), " %s", (sym->castToMethodMetaDataSymbol()->getName()));
                     traceMsg(comp(),"\n");
                     }
                  }
               }

            //_usedSymbolsToMove = useOrKill->_usedSymbols;  // this should be initialized in sinkStorePlacement and after
            _killedSymbolsToMove = useOrKill->_killedSymbols;

            //_liveVarInfo->findAllUseOfLocal(tt->getNode(), storeBlockNumber, _usedSymbolsToMove, _killedSymbolsToMove, NULL, false);

            TR_MovableStore *store = NULL;
            if (isMovableStore)
               {
               TR_ASSERT(!potentiallyMovableStores.isEmpty(), "potentiallyMovableStores should be a subset of useOrKillInfoList!");
               store = potentiallyMovableStores.popHead();

               // Example potentiallyMovableStores list and useOrKillInfoList should look like below relative to each other:
               // useOrKillInfoList             potentiallyMovableStores
               // head: UseOrKill_1             MovableStore_1
               //       UseOrKill_2             MovableStore_2
               //       MovableStore_1
               //       UseOrKill_3
               //       MovableStore_2
               // If the assume below is fired then the lists do not look like the above example lists
               TR_ASSERT(store == useOrKill->_movableStore, "mismatch in the order of nodes in potentiallyMovableStores and useOrKillInfoList!");
               TR_ASSERT(useOrKill == store->_useOrKillInfo, "mismatch useOrKillInfoList in potentiallyMovableStores!");

               if (trace())
                  traceMsg(comp(), " is still movable\n");

               if (trace())
                  traceMsg(comp(), "(Transformation #%d start - sink store)\n", _numTransformations);
                  TR::RegisterMappedSymbol *sym = tt->getNode()->getSymbolReference()->getSymbol()->getMethodMetaDataSymbol();
               if (performTransformation(comp(), "%s Finding placements for store [" POINTER_PRINTF_FORMAT "] %s with tree depth %d\n", OPT_DETAILS, tt->getNode(),
                     sym ? sym->castToMethodMetaDataSymbol()->getName() : "", store->_depth)
                   && performThisTransformation())
                  {
                  int32_t symIdx = store->_symIdx;
                  TR_ASSERT(_killedSymbolsToMove->isSet(symIdx), "_killedSymbolsToMove is inconsistent with potentiallyMovableStores entry for this store (symIdx=%d)!", symIdx);
                  if (sinkStorePlacement(store,
                                         movedPreviousStore))
                     {
                     if (trace())
                        traceMsg(comp(), "        Successfully sunk past a non-live path\n");
                     movedStore = movedPreviousStore = true;
                     _numRemovedStores++;
                     }
                  else
                     {
                     movedPreviousStore = false;
                     if (trace())
                        traceMsg(comp(), "        Didn't sink this store\n");
                     }
                  }

               if (trace())
                  traceMsg(comp(), "(Transformation #%d was %s)\n", _numTransformations, performThisTransformation() ? "performed" : "skipped");
               _numTransformations++;
               }
            else
               {
               movedPreviousStore = false;
               if (trace())
                  traceMsg(comp(), " is not movable (isStore = %s)\n", isStore ? "true" : "false");
               // if a once movable store was later marked as unmovable must remove it from the list now
               if (isStore && useOrKill->_movableStore == potentiallyMovableStores.getListHead()->getData())
                  potentiallyMovableStores.popHead();
               }

            _usedSymbolsToMove = useOrKill->_usedSymbols;

            if (movedStore)
               {
               // Even though the store is pushed outside of the Ebb, we should still set its used sym to be used if it is commoned within the Ebb.
               // For now, just set the sym to be if it is commoned below so that stores from earlier node can push thru this block.
               // Perhaps a better way to do this is to allow earlier store to be sunk along the Ebb path
               if (store->_commonedLoadsAfter)
                  {
                  if (usesDataFlowAnalysis())
                     {
                     (*_liveOnAllPaths->_blockAnalysisInfo[blockNumber]) |= (*store->_commonedLoadsAfter);
                     // remove 0 for more detail tracing
                     if (0 && trace())
                        {
                        traceMsg(comp(), "      Update for movable store due to commonedLoad: LOAP->blockInfo[%d]=", blockNumber);
                        _liveOnAllPaths->_blockAnalysisInfo[blockNumber]->print(comp());
                        traceMsg(comp(), "\n");
                        }
                     }
                   if (trace())
                      {
                      traceMsg(comp(), "        symbolsUsed in block_%d is updated due to commoned loads.  Symbols USED BEF is ", blockNumber);
                      _symbolsUsedInBlock[blockNumber]->print(comp());
                      traceMsg(comp(), "\n");
                      }
                  (*_symbolsUsedInBlock[blockNumber]) |= (*store->_commonedLoadsAfter);
                   if (trace())
                      {
                      traceMsg(comp(), "        symbolsUsed in block_%d is updated due to commoned loads.  Symbols USED AFT is ", blockNumber);
                      _symbolsUsedInBlock[blockNumber]->print(comp());
                      traceMsg(comp(), "\n");
                      }
                   }
               }
            else
               {
               // we didn't move it, so add it to block's treeKilled set and add uses to block's symbolsUsed set
               //    so that we won't move any earlier stores over it if that's not safe
               if (trace())
                  {
                  traceMsg(comp(), "      Did not move node %p so update killed and used symbols\n",tt->getNode());
                  traceMsg(comp(), "      Before use and def in block_%d: symbols KILLED ", blockNumber);
                  _symbolsKilledInBlock[blockNumber]->print(comp());
                  traceMsg(comp(), " USED ");
                  _symbolsUsedInBlock[blockNumber]->print(comp());
                  traceMsg(comp(), "\n");
                  }
               (*_symbolsUsedInBlock[blockNumber]) |= (*_usedSymbolsToMove);
               if (useOrKill->_commonedSymbols)
                  {
                  // useOrKill->_commonedSymbols should only be set if movedStorePinsCommonedSymbols == false
                  TR_ASSERT(!movedStorePinsCommonedSymbols(),"should only update with _commonedSymbols if movedStorePinsCommonedSymbols() == false\n");
                  (*_symbolsUsedInBlock[blockNumber]) |= (*useOrKill->_commonedSymbols);
                  }
               (*_symbolsKilledInBlock[blockNumber]) |= (*_killedSymbolsToMove);

               TR_ASSERT(!isStore || _symbolsKilledInBlock[blockNumber]->isSet(useOrKill->_movableStore->_symIdx),
                       "_symbolsKilledInBlock[%d] is inconsistent with for this store (symIdx=%d)!",
                       blockNumber, useOrKill->_movableStore->_symIdx);

                // need to update LOSP and LOAP as well, because the previously moved store may wiped off some locals away
                // which is not correct at *this* point and up
                if (usesDataFlowAnalysis() && (block->hasExceptionSuccessors() == false))
                  {
                  (*_liveOnSomePaths->_blockAnalysisInfo[blockNumber]) -= (*_killedSymbolsToMove);
                  (*_liveOnSomePaths->_blockAnalysisInfo[blockNumber]) |= (*_usedSymbolsToMove);
                  (*_liveOnAllPaths->_blockAnalysisInfo[blockNumber]) -= (*_killedSymbolsToMove);
                  (*_liveOnAllPaths->_blockAnalysisInfo[blockNumber]) |= (*_usedSymbolsToMove);

                  // remove 0 for more detailed tracing
                  if (0 && trace())
                     {
                     traceMsg(comp(), "      Update for the inmovable: LOSP->blockInfo[%d]=", blockNumber);
                     _liveOnSomePaths->_blockAnalysisInfo[blockNumber]->print(comp());
                     traceMsg(comp(), "\n");
                     traceMsg(comp(), "      Update for the inmovable: LOAP->blockInfo[%d]=", blockNumber);
                     _liveOnAllPaths->_blockAnalysisInfo[blockNumber]->print(comp());
                     traceMsg(comp(), "\n");
                     }
                  }

               if (trace())
                  {
                //  traceMsg(comp(), "      Did not move node %p so update killed and used symbols\n",tt->getNode());
                  traceMsg(comp(), "      Update use and def in block_%d: symbols KILLED ", blockNumber);
                  _symbolsKilledInBlock[blockNumber]->print(comp());
                  traceMsg(comp(), " USED ");
                  _symbolsUsedInBlock[blockNumber]->print(comp());
                  traceMsg(comp(), "\n\n");
                  }
               }
            }

         // clear the lists before processing next EBB
         storeListPtr = NULL;
         useOrKillInfoListPtr = NULL;

         killedLiveCommonedLoads->empty();
         _tempSymMap->reset();
         _tempSymMap->init(4);
         }
      }
   }

bool TR_SinkStores::performThisTransformation()
   {
   return (_lastSinkOptTransformationIndex == -1) ||
          (_firstSinkOptTransformationIndex <= _numTransformations &&
          _numTransformations <= _lastSinkOptTransformationIndex);

   }

void TR_SinkStores::searchAndMarkFirstUses(TR::Node *node,
                                           TR::TreeTop *tt,
                                           TR_MovableStore *movableStore,
                                           TR::Block *currentBlock,
                                            TR_BitVector *firstRefsOfCommonedSymbolsForThisStore)
   {
   if (0 && trace())
      traceMsg(comp(),"      node %p, futureUseCount %d, refCount %d\n",node,node->getFutureUseCount(),node->getReferenceCount());
   if (node->getOpCode().isLoadVarDirect() && node->getOpCode().hasSymbolReference())
      {
      TR::RegisterMappedSymbol *local = getSinkableSymbol(node);
      if (!local)
         return;
      int32_t symIdx = local->getLiveLocalIndex();

      if (symIdx != INVALID_LIVENESS_INDEX && firstRefsOfCommonedSymbolsForThisStore->isSet(symIdx))
         {
         TR_CommonedLoad *commonedLoad = movableStore->getCommonedLoad(node);
         if (trace() && commonedLoad)
            traceMsg(comp(),"      movableStore %p containsCommonedLoad (node %p, symIdx %d, isSatisfied = %d, isKilled = %d)\n",movableStore->_useOrKillInfo->_tt->getNode(),commonedLoad->getNode(),commonedLoad->getSymIdx(),commonedLoad->isSatisfied(),commonedLoad->isKilled());
         else if (trace())
            traceMsg(comp(),"      commonedLoad is NULL for node %p with symIdx %d\n",node, symIdx);
         if ((node->getFutureUseCount() == 0) &&
             movableStore->satisfyCommonedLoad(node))
            {
            if (0 && trace())
               traceMsg(comp(),"      node %p, futureUseCount %d, refCount %d\n",node,node->getFutureUseCount(),node->getReferenceCount());
            if (!findFirstUseOfLoad(node))
               {
               TR_FirstUseOfLoad *firstUse = new (trStackMemory()) TR_FirstUseOfLoad(node, tt, currentBlock->getNumber());
               TR_HashId addID = 0;
               _firstUseOfLoadMap->add(node, addID, firstUse);
               if (trace())
               traceMsg(comp(),"      searchAndMarkFirstUses creating and adding firstUse %p with node %p and anchor treetop %p to hash\n",firstUse,node,tt->getNode());
               }
            }
         }
      }

   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      // If futureUseCount > 0 then this subtree is commoned in from an earlier treetop, in which case it will not
      // contain any first uses -- that earlier treetop will be the first uses.
      //
      if (child->getFutureUseCount() == 0)
         searchAndMarkFirstUses(child, tt, movableStore, currentBlock, firstRefsOfCommonedSymbolsForThisStore);
      }
   }

void TR_SinkStores::coalesceSimilarEdgePlacements()
   {
   if (trace())
      traceMsg(comp(), "Trying to coalesce edge placements:\n");

   ListElement<TR_EdgeStorePlacement> *firstEdgePtr = _allEdgePlacements.getListHead();
   while (firstEdgePtr != NULL)
      {
      TR_EdgeStorePlacement *firstEdgePlacement = firstEdgePtr->getData();
      TR_EdgeInformation *firstEdgeInfo = firstEdgePlacement->_edges.getListHead()->getData();
      TR::CFGEdge *firstEdge = firstEdgeInfo->_edge;
      // we want to find edges that have the same "to" block and the same list of stores
      // such edges can be combined into a single placement and only one of the edges must be split (others just redirected)
      // not really expecting there to be a large number of edges on this list, so just do the stupidest N^2 thing for now
      ListElement<TR_EdgeStorePlacement> *secondEdgePtr = firstEdgePtr->getNextElement();
      TR::Block *firstToBlock = firstEdge->getTo()->asBlock();

      if (trace())
         {
         traceMsg(comp(), "  Examining edge placement (%d,%d) with stores:", firstEdge->getFrom()->getNumber(), firstToBlock->getNumber());
         ListElement<TR_StoreInformation> *firstStorePtr = firstEdgePlacement->_stores.getListHead();
         while (firstStorePtr != NULL)
            {
            traceMsg(comp(), " [" POINTER_PRINTF_FORMAT "](copy=%d)", firstStorePtr->getData()->_store->getNode(), firstStorePtr->getData()->_copy);
            firstStorePtr = firstStorePtr->getNextElement();
            }
         traceMsg(comp(), "\n");
         }

      ListElement<TR_EdgeStorePlacement> *lastPtr = firstEdgePtr;
      while (secondEdgePtr != NULL)
         {
         ListElement<TR_EdgeStorePlacement> *nextPtr = secondEdgePtr->getNextElement();
         TR_EdgeStorePlacement *secondEdgePlacement = secondEdgePtr->getData();
         TR_EdgeInformation *secondEdgeInfo = secondEdgePlacement->_edges.getListHead()->getData();
         TR::CFGEdge *secondEdge = secondEdgeInfo->_edge;
         TR::Block *secondToBlock = secondEdge->getTo()->asBlock();

         if (trace())
            {
            traceMsg(comp(), "    Comparing to edge placement (%d,%d) with stores:", secondEdge->getFrom()->getNumber(), secondToBlock->getNumber());
            ListElement<TR_StoreInformation> *secondStorePtr = secondEdgePlacement->_stores.getListHead();
            while (secondStorePtr != NULL)
               {
               traceMsg(comp(), " [" POINTER_PRINTF_FORMAT "](copy=%d)", secondStorePtr->getData()->_store->getNode(), secondStorePtr->getData()->_copy);
               secondStorePtr = secondStorePtr->getNextElement();
               }
            traceMsg(comp(), "\n");
            }

         if (secondToBlock->getNumber() == firstToBlock->getNumber())
            {
            // these two edges go to the same block, let's see if they have the same set of stores
            // the stores should be added to the lists in the same order, so first mismatch in a parallel walk is the exit test

            if (trace())
               traceMsg(comp(), "      stores have same destination block\n");

            ListElement<TR_StoreInformation> *firstStorePtr = firstEdgePlacement->_stores.getListHead();
            ListElement<TR_StoreInformation> *secondStorePtr = secondEdgePlacement->_stores.getListHead();
            while (firstStorePtr != NULL)
               {
               if (secondStorePtr == NULL)
                  break;
               if (firstStorePtr->getData()->_store != secondStorePtr->getData()->_store)
                  break;
               firstStorePtr = firstStorePtr->getNextElement();
               secondStorePtr = secondStorePtr->getNextElement();
               }

            // if both pointers are NULL, we walked the whole list so they're the same and we should coalesce them
            if (firstStorePtr == NULL && secondStorePtr == NULL)
               {
               if (trace())
                  traceMsg(comp(), "      store lists are identical so coalescing\n");
               firstEdgePlacement->_edges.add(secondEdgeInfo);
               lastPtr->setNextElement(nextPtr);
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "      store lists are different so cannot coalesce\n");
               lastPtr = secondEdgePtr;
               }
            }
         else
            {
            if (trace())
               traceMsg(comp(), "      destination blocks are different (%d,%d) so cannot coalesce\n", firstToBlock->getNumber(), secondToBlock->getNumber());
            lastPtr = secondEdgePtr;
            }

         secondEdgePtr = nextPtr;
         }

      firstEdgePtr = firstEdgePtr->getNextElement();
      }
   }

void TR_SinkStores::placeStoresInBlock(List<TR_StoreInformation> & stores, TR::Block *placementBlock)
   {
   // place stores into placementBlock
   // NOTE: this list should be properly ordered so that if a store X was earlier than another store Y in the original code,
   //       then the stores will be placed in this block in the order X, Y
   //       ALSO SEE comments in recordPlacementForDefAlongEdge and recordPlacementForDefInBlock
   TR::TreeTop *treeBeforeNextPlacement=placementBlock->getEntry();
   ListElement<TR_StoreInformation> *storePtr=stores.getListHead();
   //if (storePtr != NULL)
   //   requestOpt(localCSE, true, placementBlock);

   while (storePtr != NULL)
      {
      TR::TreeTop *newStore=NULL;
      TR::TreeTop *store = storePtr->getData()->_store;
      bool copyStore = storePtr->getData()->_copy;
      if (copyStore)
         {
         TR::TreeTop *storeTemp = storePtr->getData()->_storeTemp;
         if (storePtr->getData()->_needsDuplication)  // the tree may have already been duplicated in sinkStorePlacement
            newStore = storeTemp->duplicateTree();
         else
            newStore = storeTemp;
         requestOpt(OMR::localCSE, true, placementBlock);
         }
      else
         {
         // just moving the tree, so first disconnect it from where it's sitting now
         TR::TreeTop *prevTT = store->getPrevTreeTop();
         TR::TreeTop *nextTT = store->getNextTreeTop();
         prevTT->setNextTreeTop(nextTT);
         nextTT->setPrevTreeTop(prevTT);
         newStore = store;
         }

      if (0 && trace())
         traceMsg(comp(), "        PLACE new store [" POINTER_PRINTF_FORMAT "] (original store [" POINTER_PRINTF_FORMAT "]) at beginning of block_%d\n", newStore->getNode(), store->getNode(), placementBlock->getNumber());
      TR::TreeTop::insertTreeTops(comp(), treeBeforeNextPlacement, newStore, newStore);
      treeBeforeNextPlacement = newStore;
      _numPlacements++;

      storePtr = storePtr->getNextElement();
      }
   }

// All the edges in placement's edge list should go to the same block
// Either that block is an exception handler, or it isn't
//  if it is, then create a new catch block and redirect all the edges to the new catch block
//  if it's not, then split the first edge and then redirect all the edges to the new split block
void TR_SinkStores::placeStoresAlongEdges(List<TR_StoreInformation> & stores, List<TR_EdgeInformation> & edges)
   {
   TR::CFG *cfg=comp()->getFlowGraph();

   // don't want to update structure
   cfg->setStructure(NULL);

   ListIterator<TR_EdgeInformation> edgeInfoIt(&edges);
   TR::Block *placementBlock=NULL;

   // split first edge by creating new block
   TR_EdgeInformation *edgeInfo=edgeInfoIt.getFirst();
   TR::CFGEdge *edge = edgeInfo->_edge;
   TR::Block *from=edge->getFrom()->asBlock();
   TR::Block *to=edge->getTo()->asBlock();
   if (!to->getExceptionPredecessors().empty())
      {
      // all these edges are exception edges
      // SO: create a new catch block with the original to block as its handler
      //     store will be placed into the new catch block
      //     rethrow the exception at end of the new catch block
      //     and redirect all the edges to the new catch block
      if (trace())
         traceMsg(comp(), "    block_%d is an exception handler, so creating new catch block\n", to->getNumber());

      TR_ASSERT(_sinkThruException, "do not expect to sink store along this exception edge\n");
      TR_ASSERT(stores.getListHead()->getData()->_copy, "block cannot be extended across an exception edge");

      TR::TreeTop *firstStore=stores.getListHead()->getData()->_store;
      placementBlock = TR::Block::createEmptyBlock(firstStore->getNode(), comp(), to->getFrequency(), to);
      //placementBlock->setHandlerInfo(0, comp()->getInlineDepth(), 0, comp()->getCurrentMethod()); wrong
      //placementBlock->setHandlerInfo(to->getCatchType(), to->getInlineDepth(), to->getHandlerIndex(), to->getOwningMethod()); works on trychk2
      //placementBlock->setHandlerInfo(0, to->getInlineDepth(), 0, to->getOwningMethod()); wrong
      //placementBlock->setHandlerInfo(0, to->getInlineDepth(), to->getHandlerIndex(), to->getOwningMethod()); wrong
      //placementBlock->setHandlerInfo(to->getCatchType(), to->getInlineDepth(), 0, to->getOwningMethod()); fail modena 113380
      placementBlock->setHandlerInfo(to->getCatchType(), to->getInlineDepth(), genHandlerIndex(), to->getOwningMethod(), comp());

//      // fix that is not working
//      placementBlock = new (to->trHeapMemory()) TR::Block(*to,
//                                    TR::TreeTop::create(comp(), TR::Node::create(firstStore->getNode(), TR::BBStart)),
//                                    TR::TreeTop::create(comp(), TR::Node::create(firstStore->getNode(), TR::BBEnd)));
//      placementBlock->getEntry()->join(placementBlock->getExit());

      cfg->addNode(placementBlock);
      comp()->getMethodSymbol()->getLastTreeTop()->join(placementBlock->getEntry());

      if (trace())
         traceMsg(comp(), "      created new catch block_%d\n", placementBlock->getNumber());

      TR::SymbolReferenceTable * symRefTab = comp()->getSymRefTab();
      TR::Node * excpNode = TR::Node::createWithSymRef(firstStore->getNode(), TR::aload, 0, symRefTab->findOrCreateExcpSymbolRef());
      TR::Node * newAthrow = TR::Node::createWithSymRef(TR::athrow, 1, 1, excpNode, symRefTab->findOrCreateAThrowSymbolRef(comp()->getMethodSymbol()));
      placementBlock->append(TR::TreeTop::create(comp(), newAthrow));

      if (trace())
         {
         traceMsg(comp(), "      created new ATHROW [" POINTER_PRINTF_FORMAT "]\n", newAthrow);
         traceMsg(comp(), "      splitting exception edge (%d,%d)", from->getNumber(), to->getNumber());
         traceMsg(comp(), " into (%d,%d)", from->getNumber(), placementBlock->getNumber());
         traceMsg(comp(), " and (%d,%d)\n", placementBlock->getNumber(), to->getNumber());
         }

      // !!order of the following 3 lines are important
      // - need to addEdge to the "to" block first, otherwise "to" block will be removed accidentally
      // - remove the edge next so that the exception edge from->placement can be added properly later
      cfg->addExceptionEdge(placementBlock, to);
      cfg->removeEdge(from, to);
      cfg->addExceptionEdge(from, placementBlock);

      // now redirect all the edges to placementBlock
      for (edgeInfo = edgeInfoIt.getNext(); edgeInfo != NULL; edgeInfo = edgeInfoIt.getNext())
         {
         TR::CFGEdge *edge = edgeInfo->_edge;
         TR::Block *from=edge->getFrom()->asBlock();
         TR::Block *thisTo=edge->getTo()->asBlock();
         TR_ASSERT(thisTo == to, "all edges on edgeList should be to same destination block");

         if (trace())
            traceMsg(comp(), "      changing exception edge (%d,%d) to (%d,%d)\n", from->getNumber(), to->getNumber(), from->getNumber(), placementBlock->getNumber());

         cfg->removeEdge(from, to);
         cfg->addExceptionEdge(from, placementBlock);
         }
      }
   else
      {
      // all edges are normal flow edges
      // SO: split the first edge
      //     store will be placed into the new split block
      //     redirect all other edges to the new split block
//      TR::TreeTop *insertionExit = findOptimalInsertion(from, to);
//      placementBlock = from->splitEdge(from, to, comp(), 0, insertionExit);
      placementBlock = from->splitEdge(from, to, comp());
      if (trace())
         traceMsg(comp(), "    Split edge from %d to %d to create new split block_%d\n", from->getNumber(), to->getNumber(), placementBlock->getNumber());

      // now redirect all the other edges to splitBlock
      edgeInfo = edgeInfoIt.getNext();
      if (edgeInfo == NULL)
         {
         if (from->getExit()->getNextTreeTop() == placementBlock->getEntry() &&
             from->canFallThroughToNextBlock() && !from->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets())
         placementBlock->setIsExtensionOfPreviousBlock();
         }
      else
         {
         for (; edgeInfo != NULL; edgeInfo = edgeInfoIt.getNext())
            {
            TR::CFGEdge *edge=edgeInfo->_edge;
            TR::Block *from=edge->getFrom()->asBlock();
            TR::Block *thisTo=edge->getTo()->asBlock();
            TR_ASSERT(thisTo == to, "all edges on edgeList should be to same destination block");
            if (trace())
               traceMsg(comp(), "    changing normal edge (%d,%d) to (%d,%d)\n", from->getNumber(), to->getNumber(), from->getNumber(), placementBlock->getNumber());

            if ((placementBlock->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto) &&
                !placementBlock->isExtensionOfPreviousBlock() &&
                (from == thisTo->getPrevBlock()) &&
                from->getLastRealTreeTop()->getNode()->getOpCode().isIf())
               {
               TR::TreeTop *prevExit = placementBlock->getEntry()->getPrevTreeTop();
	       TR::TreeTop *nextEntry = placementBlock->getExit()->getNextTreeTop();
               TR::TreeTop *newExit = from->getExit();
               TR::TreeTop *newEntry = newExit->getNextTreeTop();
               prevExit->join(nextEntry);
               newExit->join(placementBlock->getEntry());
               placementBlock->getExit()->join(newEntry);
               TR::TreeTop *lastTree = placementBlock->getLastRealTreeTop();
               TR::TreeTop *prevLast = lastTree->getPrevTreeTop();
               TR::TreeTop *nextLast = lastTree->getNextTreeTop();
               prevLast->join(nextLast);

               if (!from->hasSuccessor(placementBlock))
                  comp()->getFlowGraph()->addEdge(from, placementBlock);
               comp()->getFlowGraph()->removeEdge(from, thisTo);
               }
            else
               TR::Block::redirectFlowToNewDestination(comp(), edge, placementBlock, true);

            comp()->getFlowGraph()->setStructure(NULL);
            }
         }
      }

   placeStoresInBlock(stores, placementBlock);

   //comp()->dumpMethodTrees("After splitting an edge");
   }

void TR_SinkStores::doSinking()
   {
   TR::TreeTop *notYetRemovedStore=NULL;
   TR::TreeTop *skipStore=NULL;

   // try to combine some edge placements to reduce the number of new blocks we need to create
   coalesceSimilarEdgePlacements();

   // collect the original stores here
   List<TR::TreeTop> storesToRemove(trMemory());
   List<TR::TreeTop> movedStores(trMemory());

   // do all the edge placements
   if (trace())
      traceMsg(comp(), "Now performing store placements:\n");

   if (_indirectLoadAnchors)
      {
      ListIterator<TR_IndirectLoadAnchor> it(_indirectLoadAnchors);
      for (TR_IndirectLoadAnchor *loadAnchor = it.getFirst(); loadAnchor; loadAnchor = it.getNext())
         {
         TR::Node *anchoredNode = loadAnchor->getNode();
         if (anchoredNode->decFutureUseCount() == 0)
            {
            // Insert the anchoredNode at the start of the specified block
            // This is correctly placed as it is:
            //    - low enough that the child of the indirect load (the translateAddress node) has already been evaluated
            //      (as the translateAddress would have ended the previous block)
            //    - high enough that any subsequent uses will get naturally commoned
            TR::Block *anchorBlock = loadAnchor->getAnchorBlock();
            TR::TreeTop *anchorTree = loadAnchor->getAnchorTree();
            if (trace())
               traceMsg(comp(),   "anchoring indirect load %p after node %p at start of block_%d anchor tt node is %p\n",anchoredNode,anchorBlock->getEntry()->getNode(),anchorBlock->getNumber(),anchorTree->getNode());
            anchorTree->insertBefore(TR::TreeTop::create(comp(), anchoredNode));
            _numIndirectLoadAnchors++;
            }
         }
      }

   while (!_allEdgePlacements.isEmpty())
      {
      TR_EdgeStorePlacement *placement=_allEdgePlacements.popHead();
      if (!placement->_stores.isEmpty())
         {
         placeStoresAlongEdges(placement->_stores, placement->_edges);
         while (!placement->_stores.isEmpty())
            {
            TR_StoreInformation *storeInfo=placement->_stores.popHead();
            TR::TreeTop *store=storeInfo->_store;
            if (storeInfo->_copy)
               {
               if (!storesToRemove.find(store))
                  storesToRemove.add(store);
               }
            else
               {
//               if (trace())
  //                traceMsg(comp(), "    adding store [" POINTER_PRINTF_FORMAT "] to movedStores (edge placement)\n", store);
               TR_ASSERT(!movedStores.find(store), "shouldn't find a moved store more than once");
               movedStores.add(store);
               }
            }
         }
      }

   // do any block placements
   while (!_allBlockPlacements.isEmpty())
      {
      TR_BlockStorePlacement *placement=_allBlockPlacements.popHead();
      if (!placement->_stores.isEmpty())
         {
         placeStoresInBlock(placement->_stores, placement->_block);
         while (!placement->_stores.isEmpty())
            {
            TR_StoreInformation *storeInfo=placement->_stores.popHead();
            TR::TreeTop *store=storeInfo->_store;
            if (storeInfo->_copy)
               {
               if (!storesToRemove.find(store))
                  storesToRemove.add(store);
               }
            else
               {
               if (trace())
                  traceMsg(comp(), "    adding store [" POINTER_PRINTF_FORMAT "] to movedStores (block placement)\n", store);
               TR_ASSERT(!movedStores.find(store), "shouldn't find a moved store more than once");
               movedStores.add(store);
               }
            }
         }
      }

   // now remove the original stores
   while (!storesToRemove.isEmpty())
      {
      TR::TreeTop *originalStore = storesToRemove.popHead();
      if (trace())
         traceMsg(comp(), "Removing original store [" POINTER_PRINTF_FORMAT "]\n", originalStore->getNode());

      if (movedStores.find(originalStore))
         {
         if (trace())
            traceMsg(comp(), "  this store has been moved already, so no need to remove it\n");
         }
      else
         {
         // unlink tt since we've now sunk it to other places
         //TR::TreeTop *originalPrev = originalStore->getPrevTreeTop();
         //TR::TreeTop *originalNext = originalStore->getNextTreeTop();

         //originalStore->getPrevTreeTop()->setNextTreeTop(originalNext);
         //originalStore->getNextTreeTop()->setPrevTreeTop(originalPrev);
         //originalStore->getNode()->recursivelyDecReferenceCount();
         TR::Node::recreate(originalStore->getNode(), TR::treetop);
         //requestOpt(deadTreesElimination, true, originalStore->getEnclosingBlock()->startOfExtendedBlock());
         }
      }

   }

TR::RegisterMappedSymbol *
TR_SinkStores::getSinkableSymbol(TR::Node *node)
   {
   TR::Symbol *symbol = node->getSymbolReference()->getSymbol();
   if (symbol->isAutoOrParm() ||
      (sinkMethodMetaDataStores() && symbol->isMethodMetaData()))
      return symbol->castToRegisterMappedSymbol();
   else
      return NULL;
   }
bool TR_SinkStores::treeIsSinkableStore(TR::Node *node, bool sinkIndirectLoads, uint32_t &indirectLoadCount, int32_t &depth, bool &isLoadStatic, vcount_t visitCount)
   {
   if (enablePreciseSymbolTracking())
      {
      if (node->getVisitCount() == visitCount)
         return true;
      node->setVisitCount(visitCount);
      }
   else if (depth > 8)
      {
      // if expression tree is too deep, not likely we'll be able to move it
      //      but we'll waste a lot of time looking at it
      return false;
      }

   int32_t numChildren = node->getNumChildren();
   static bool underCommonedNode;

   /* initialization upon first entry */
   if (depth == 0)
      {
      underCommonedNode = false;
      }

   if (numChildren == 0)
      {
      if (!(node->getOpCode().isLoadConst() || node->getOpCode().isLoadVarDirect())) //|| node->getOpCodeValue() == TR::loadaddr))
         {
         if (trace())
            traceMsg(comp(), "      *not a load const or direct load*\n");
         return false;
         }
      if (node->getOpCode().isLoadVarDirect())
         {
         TR::RegisterMappedSymbol *local = getSinkableSymbol(node);
         if (local == NULL)
            {
            if (!sinkStoresWithStaticLoads())
               {
               if (trace())
                  traceMsg(comp(), "      *no local found on direct load*\n");
               return false;
               }
            else if (!node->getSymbolReference()->getSymbol()->isStatic())
               {
               if (trace())
                  traceMsg(comp(), "      *no local found on direct load and not a static load*\n");
               return false;
               }

            isLoadStatic = true;
            }
         }
      }
   else
      {
      if (node->getOpCode().isLoadIndirect())
         {
         if (sinkIndirectLoads)
            {
            indirectLoadCount++;
            if (trace())
               traceMsg(comp(), "      found %s at %p so do not search children indirectLoadCount = %d\n", "indirect load" ,node,indirectLoadCount);
            return true; // indirectLoads are anchored in the mainline path so no need to search below them
            }
         else
            {
            if (trace())
               {
               if (sinkStoresWithIndirectLoads())
                  traceMsg(comp(), "      *found an indirect load and store is not for a condition code*\n");
               else
                  traceMsg(comp(), "      *found an indirect load*\n");
               }
            return false;
            }
         }
      if (node->getOpCode().isCall() || node->exceptionsRaised())
         {
         if (trace())
            {
            if (node->getOpCodeValue() == TR::arraycmp)
               traceMsg(comp(),"      *arraycmp is a call %d, raises exceptions %d*\n",node->getOpCode().isCall(),node->exceptionsRaised());
            else if (node->getOpCodeValue() == TR::arraycopy)
               traceMsg(comp(),"      *arraycopy is a call %d, raises exceptions %d*\n",node->getOpCode().isCall(),node->exceptionsRaised());
            traceMsg(comp(), "      *store is a call or an excepting node*\n");
            }
         return false;
         }

      if (node->getOpCode().isStoreDirect() && node->isPrivatizedInlinerArg())
         {
         if (trace())
            traceMsg(comp(), "         store is privatized inliner argument, not safe to move it\n");
         return false;
         }

      if (node->getOpCode().hasSymbolReference() && node->getSymbol()->holdsMonitoredObject())
         {
         if (trace())
            traceMsg(comp(), "        store holds monitored object, not safe to move it\n");
         return false;
         }

      if (node->getOpCode().isStore() &&
          (node->dontEliminateStores() ||
          (node->getSymbolReference()->getSymbol()->isAuto() &&
             (/*node->dontEliminateStores(comp()) ||*/
              node->getSymbolReference()->getSymbol()->castToAutoSymbol()->isInternalPointer() /*||
              node->getSymbolReference()->getUseonlyAliases(comp()->getSymRefTab())*/))
          ))
         {
         if (trace())
            traceMsg(comp(), "         can't move store of pinning array reference or with UseOnlyAliases\n");
         return false;
         }
      }

   if (!comp()->cg()->getSupportsJavaFloatSemantics() &&
       node->getOpCode().isFloatingPoint() &&
       (underCommonedNode || node->getReferenceCount() > 1))
      {
      if (trace())
         traceMsg(comp(), "         fp store failure\n");
      return false;
      }

   if (numChildren == 0 &&
       node->getOpCode().isLoadVarDirect() &&
       node->getSymbolReference()->getSymbol()->isStatic() &&
       (underCommonedNode || node->getReferenceCount() > 1))
       {
       if (trace())
         traceMsg(comp(), "         commoned static load store failure: %p\n", node);
       return false;
       }

   int32_t currentDepth = ++depth;
   bool    previouslyCommoned = underCommonedNode;
   if (node->getReferenceCount() > 1)
      underCommonedNode = true;
   for (int32_t c=0;c < numChildren;c++)
      {
      int32_t childDepth = currentDepth;
      TR::Node *child = node->getChild(c);
      if (!treeIsSinkableStore(child, sinkIndirectLoads, indirectLoadCount, childDepth, isLoadStatic, visitCount))
         return false;
      if (childDepth > depth)
         depth = childDepth;
      }
   underCommonedNode = previouslyCommoned;
   return true;
   }

bool TR_TrivialSinkStores::storeIsSinkingCandidate(TR::Block *block, TR::Node *node, int32_t symIdx, bool sinkIndirectLoads, uint32_t &indirectLoadCount, int32_t &depth, bool &isLoadStatic, vcount_t &treeVisitCount, vcount_t &highVisitCount)
   {
   vcount_t newVisitCount =  comp()->incVisitCount();
   treeVisitCount++;
   highVisitCount++;
   int32_t b = block->getNumber();
   comp()->setCurrentBlock(block);

   return (symIdx >= 0 &&
           block->getNextBlock() &&
           block->getNextBlock()->isExtensionOfPreviousBlock() &&
           treeIsSinkableStore(node, sinkIndirectLoads, indirectLoadCount, depth, isLoadStatic, newVisitCount));
   }

bool TR_GeneralSinkStores::storeIsSinkingCandidate(TR::Block *block, TR::Node *node, int32_t symIdx, bool sinkIndirectLoads, uint32_t &indirectLoadCount, int32_t &depth , bool &isLoadStatic, vcount_t &treeVisitCount, vcount_t &highVisitCount)
   {
   int32_t b = block->getNumber();
   // FIXME::
   // need to disable for privatized stores for virtual guards and versioning tests
   // for now, just try to push any store we can...in future, might want to limit candidates somewhat

   // treeIsSinkable store checks the visitCount as part of the enablePreciseSymbolTracking work so when this is enabled then
   // the call below must pass in comp()->incVisitCount() instead of comp()->getVisitCount() and treeVisitCount and highVisitCount
   // must be incremented
   if (enablePreciseSymbolTracking())
      {
      treeVisitCount++;
      highVisitCount++;
      }
   TR_ASSERT(!enablePreciseSymbolTracking(),"visitCount arg treeIsSinkableStore is ignored, should use incVisitCount() instead\n");
   comp()->setCurrentBlock(block);
   return (symIdx >= 0 &&
           _liveOnNotAllPaths->_outSetInfo[b]->get(symIdx) &&
           treeIsSinkableStore(node, sinkIndirectLoads, indirectLoadCount, depth, isLoadStatic, enablePreciseSymbolTracking() ? comp()->incVisitCount() : comp()->getVisitCount()));
   }

const char *
TR_GeneralSinkStores::optDetailString() const throw()
   {
   return "O^O GENERAL SINK STORES: ";
   }

// return true if it is safe to propagate the store thru the moved stores in the edge placement
bool TR_SinkStores::isSafeToSinkThruEdgePlacement(int symIdx, TR::CFGNode *block, TR::CFGNode *succBlock, TR_BitVector *allEdgeInfoUsedOrKilledSymbols)
   {
   int32_t succBlockNumber = succBlock->getNumber();
   bool isSafe = true;
   if (_placementsForEdgesToBlock[succBlockNumber] != NULL)
      {
      ListIterator<TR_EdgeStorePlacement> edgeIt(_placementsForEdgesToBlock[succBlockNumber]);
      for (TR_EdgeStorePlacement *placement=edgeIt.getFirst(); placement != NULL; placement = edgeIt.getNext())
         {
         // all edge placements should have exactly one edge at this point, so no need to iterate over edges
         TR_ASSERT(placement->_edges.getListHead()->getNextElement() == NULL, "edge placement list should only have one edge!");

         TR_EdgeInformation *edgeInfo = placement->_edges.getListHead()->getData();
         TR::CFGEdge *edge = edgeInfo->_edge;
         TR::CFGNode *from = edge->getFrom();

         if (from == block)
            (*allEdgeInfoUsedOrKilledSymbols) |= (*edgeInfo->_symbolsUsedOrKilled);

         if (isSafe &&
             from == block &&
             (edgeInfo->_symbolsUsedOrKilled->intersects(*_usedSymbolsToMove) ||
              edgeInfo->_symbolsUsedOrKilled->intersects(*_killedSymbolsToMove)))
            isSafe = false;
         }
      }
   return isSafe;
   }

// return true if symIndex is used in store recoreded in block->succBlock edge.
bool TR_SinkStores::isSymUsedInEdgePlacement(TR::CFGNode *block, TR::CFGNode *succBlock)
   {
   int32_t succBlockNumber = succBlock->getNumber();
   if (_placementsForEdgesToBlock[succBlockNumber] != NULL)
      {
      ListIterator<TR_EdgeStorePlacement> edgeIt(_placementsForEdgesToBlock[succBlockNumber]);
      for (TR_EdgeStorePlacement *placement=edgeIt.getFirst(); placement != NULL; placement = edgeIt.getNext())
         {
         // all edge placements should have exactly one edge at this point, so no need to iterate over edges
         TR_ASSERT(placement->_edges.getListHead()->getNextElement() == NULL, "edge placement list should only have one edge!");

         TR_EdgeInformation *edgeInfo = placement->_edges.getListHead()->getData();
         TR::CFGEdge *edge = edgeInfo->_edge;
         TR::CFGNode *from = edge->getFrom();

         if (from == block)
            {
            if (edgeInfo->_symbolsUsedOrKilled->intersects(*_killedSymbolsToMove))
               {
               if (trace())
                  {
                  traceMsg(comp(),"              symbolsKilled in current store\t");
                  _killedSymbolsToMove->print(comp());
                  traceMsg(comp(), "\n");
                  traceMsg(comp(),"              symbolsKilledUsed along edge\t");
                  edgeInfo->_symbolsUsedOrKilled->print(comp());
                  traceMsg(comp(), "\n");
                  traceMsg(comp(), "              Killed symbols used in store placement along edge (%d->%d)\n", block->getNumber(), succBlockNumber);
                  }
               return true;
               }
            }
         }
      }
   return false;
   }

bool nodeContainsCall(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);

   // to be more conservative when we sink statics
   // stop when any of the following conditions is met:
   // 1) a call
   // 2) a monent
   // 3) a monexit
   // 4) a store to a static
   // 5) a symref that is unresolved
   // 6) a symbol that is volatile
   if (node->getOpCode().isCall() ||
         node->getOpCodeValue() == TR::monent ||
         node->getOpCodeValue() == TR::monexit ||
         (node->getOpCode().isStore() && node->getSymbolReference()->getSymbol()->isStatic()) ||
         (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->isUnresolved()) ||
         (node->getOpCode().hasSymbolReference() && node->getSymbol()->isVolatile()))
      return true;

   int32_t i;
   for (i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      if (nodeContainsCall(child, visitCount))
         return true;
      }

   return false;
   }

bool blockContainsCall(TR::Block *block, TR::Compilation *comp)
   {
   vcount_t visitCount = comp->incVisitCount();
   for (TR::TreeTop *tt = block->getFirstRealTreeTop();
        tt != block->getExit();
        tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (nodeContainsCall(node, visitCount))
         return true;
      }

   return false;
   }

// return true if store can be pushed into the destination of the edge, otherwise false.
bool TR_SinkStores::shouldSinkStoreAlongEdge(int symIdx, TR::CFGNode *block, TR::CFGNode *succBlock, int32_t sourceBlockFrequency, bool isLoadStatic, vcount_t visitCount, TR_BitVector *allEdgeInfoUsedOrKilledSymbols)
   {
   int32_t maxFrequency = (sourceBlockFrequency * 110 ) / 100;   // store can be sunk along BBs with 10% tolerance in hotness

   if (succBlock->getVisitCount() == visitCount)
      // if successor has already been visited, bad things are happening: place def on edge
      return false;

   // any freq below 50 is consider noise
   if (succBlock->asBlock()->getFrequency() > 50 && maxFrequency > 50)
      {
      if (succBlock->asBlock()->getFrequency() > maxFrequency)
         // if succBlock has higher frequency than store's original location, don't push it any further
         return false;
      }

   if (isLoadStatic && blockContainsCall(succBlock->asBlock(), comp()))
      {
      if (trace())
         traceMsg(comp(), "            Can't push sym %d to successor block_%d (static load)\n", symIdx, succBlock->getNumber());
      return false;
      }

   // for now, don't allow a def to move into a loop
   // MISSED OPPORTUNITY: def above loop, not used in loop but use(s) exist on some paths below the loop
   TR_Structure *containingLoop = succBlock->asBlock()->getStructureOf()->getContainingLoop();
   if (containingLoop != NULL && containingLoop->getEntryBlock() == succBlock)
      {
      // make one exception: try to sink thru one-bb loop
      if (containingLoop->asRegion()->numSubNodes() > 1 ||
          !storeCanMoveThroughBlock(_symbolsKilledInBlock[succBlock->getNumber()], _symbolsUsedInBlock[succBlock->getNumber()], symIdx))
         {
         return false;
         }
      }

   // finally, check that the symbol isn't referenced by some store placed along this edge
   return
      (isSafeToSinkThruEdgePlacement(symIdx, block, succBlock, allEdgeInfoUsedOrKilledSymbols) &&
      !(allEdgeInfoUsedOrKilledSymbols->intersects(*_usedSymbolsToMove) ||
        allEdgeInfoUsedOrKilledSymbols->intersects(*_killedSymbolsToMove)));
   }

bool TR_SinkStores::checkLiveMergingPaths(TR_BlockListEntry *blockEntry, int32_t symIdx)
   {
   TR_ASSERT(usesDataFlowAnalysis(), "checkLiveMergingPaths requires data flow analysis");

   TR::Block *block = blockEntry->_block;

   if (trace())
      traceMsg(comp(), "            Counting LONAP predecessors to compare to propagation count %d\n", blockEntry->_count);

   // first, if block is a merge point, we need to make sure that the def propagated here along all paths
   int32_t numPredsLONAP=0;
   int32_t numPreds=0;
   TR_PredecessorIterator inEdges(block);
   for (auto predEdge = inEdges.getFirst(); predEdge; predEdge = inEdges.getNext())
      {
      int32_t predBlockNumber = predEdge->getFrom()->getNumber();
      numPreds++;
      if (_liveOnNotAllPaths->_outSetInfo[predBlockNumber]->get(symIdx))
         {
         if (trace())
            traceMsg(comp(), "              found LONAP predecessor %d\n", predBlockNumber);
         numPredsLONAP++;
         }
      }

   //if it has more LONAP predecessors, then there are other definitions from other predecessor edges
   //TR_ASSERT(numPredsLONAP <= blockEntry->_count, "block has fewer LONAP predecessors than edges def was propagated along");

   // NOT-SO-SAFE check for joins:
   //return (numPredsLONAP == blockEntry->_count);

   // SAFER check for joins:
   return (numPreds == blockEntry->_count);
   }


// isCorrectCommonedLoad is needed because a first use is difficult to determine from the bit vectors alone
// without descending each and every tree.
// For example if we have a commoned land node that has a symbol ref below it then by using the bit vectors
// alone it will appear that a use of the symbol ref itself (below the land use) *is* the first use.
// If we place the store here then it may not be high enough if there are other commmoned uses above.
// The bit vectors do provide a conservative hint that we may have found the first use to minimize calls
// to isCorrectCommonedLoad.
//
// land
//   lload <sym1>    This is the real first use
// ...
// lstore <sym1>
//   ==> land        If we placed the sym1 temp store after the lload below then this commoned load would not see it.
//                   This also means that the problem is not limited to MethodMetaData symbols as this store can inhibit commoning of sym1
// ...
// lload <sym1>      By using BVs alone this will look like the first use of the killed live commoned sym1
// ...
// lstore <sym1>
// ...
// lstore <sym3>
//   ==>land

bool
TR_SinkStores::isCorrectCommonedLoad(TR::Node *commonedLoad, TR::Node *searchNode)
   {
   if (commonedLoad == searchNode)
      {
      if (trace())
         traceMsg(comp(),"           found commonedLoad = " POINTER_PRINTF_FORMAT "\n", commonedLoad);
      return true;
      }
   for (int32_t i = searchNode->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = searchNode->getChild(i);
      if (isCorrectCommonedLoad(commonedLoad, child))
         return true;
      }
   return false;
   }

TR::SymbolReference*
TR_SinkStores::findTempSym(TR::Node *load)
   {
   TR_HashId id;
   if (_tempSymMap->locate(load, id))
      return (TR::SymbolReference*) _tempSymMap->getData(id);
   else
      return 0;
   }

TR_FirstUseOfLoad*
TR_SinkStores::findFirstUseOfLoad(TR::Node *load)
   {
   TR_HashId id;
   if (_firstUseOfLoadMap->locate(load, id))
      return (TR_FirstUseOfLoad*) _firstUseOfLoadMap->getData(id);
   else
      return NULL;
   }

void
TR_SinkStores::genStoreToTempSyms(TR::TreeTop *storeLocation,
                                  TR::Node *node,
                                  TR_BitVector *commonedLoadsUnderStore,
                                  TR_BitVector *killedLiveCommonedLoads,
                                  TR::Node *store,
                                  List<TR_MovableStore> &potentiallyMovableStores)
   {
   if (node->getOpCode().isLoadVarDirect() && node->getOpCode().hasSymbolReference())
      {
      TR::RegisterMappedSymbol *local = getSinkableSymbol(node);
      //TR_ASSERT(local,"invalid local symbol\n");
      if (!local)
         return;
      int32_t symIdx = local->getLiveLocalIndex();

      if (symIdx != INVALID_LIVENESS_INDEX &&
          commonedLoadsUnderStore->get(symIdx) &&
          !findTempSym(node) &&
          isCorrectCommonedLoad(node, store->getFirstChild()))
//          (!local->isMethodMetaData() || isCorrectCommonedLoad(node, store->getFirstChild())))
// FIXME: does isCorrectCommonedLoad need to be called for all symbols or just TR_MethodMetaData symbols
         {
         if (trace())
            traceMsg(comp(), "(Transformation #%d start - create temp store)\n", _numTransformations);
         if (performTransformation(comp(),
               "%s Create new temp store node for commoned loads sym %d and place above store [" POINTER_PRINTF_FORMAT "]\n",OPT_DETAILS, symIdx, storeLocation->getNode())
             && performThisTransformation())
            {
            // found first ref so reset killed commoned loads for this sym
            killedLiveCommonedLoads->reset(symIdx);
            TR::SymbolReference *symRef0 = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), node->getDataType());
            TR::Node *store0 = TR::Node::createStore(symRef0, node);
            TR::TreeTop *store0_tt = TR::TreeTop::create(comp(), store0);
            storeLocation->insertBefore(store0_tt); // insert before as the current treetop may be the one killing the sym
            TR_HashId hashIndex = 0;  // not used, initialized here to avoid compiler warning
            _tempSymMap->add(node, hashIndex, symRef0);
            _numTemps++;
            }
         else
            {
            // one or more potentially movable stores below may require that this temp store is created.
            // Find and mark these dependent movable stores as unmovable.
            ListElement<TR_MovableStore> *storeElement=potentiallyMovableStores.getListHead();
            while (storeElement != NULL)
               {
               TR_MovableStore *store = storeElement->getData();

               if (store->_movable &&
                   store->_needTempForCommonedLoads &&
                   isCorrectCommonedLoad(node,store->_useOrKillInfo->_tt->getNode()->getFirstChild()))
                  {
                  store->_movable = false;
                  if (trace())
                     traceMsg(comp(),"\tmarking store candidate [" POINTER_PRINTF_FORMAT "] as unmovable because dependent temp store transformation #%d was skipped\n", store->_useOrKillInfo->_tt->getNode(), _numTransformations);
                  }
               storeElement = storeElement->getNextElement();
               }
            }
         if (trace())
            traceMsg(comp(), "(Transformation #%d was %s)\n", _numTransformations,performThisTransformation() ? "performed" : "skipped");
         _numTransformations++;
         }
      }

   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      genStoreToTempSyms(storeLocation, child, commonedLoadsUnderStore, killedLiveCommonedLoads, store, potentiallyMovableStores);
      }
   }

// Given a dup tree, replace the symRef for all commoned loads (as in the original store) with the temp symRef
void
TR_SinkStores::replaceLoadsWithTempSym(TR::Node *newNode, TR::Node *origNode, TR_BitVector *needTempForCommonedLoads)
   {
   if (newNode->getOpCode().isLoadVarDirect() && newNode->getOpCode().hasSymbolReference() &&
       !newNode->getSymbolReference()->getSymbol()->isStatic())
      {
      TR::RegisterMappedSymbol *local = getSinkableSymbol(newNode);
      TR_ASSERT(local,"invalid local symbol\n");
      int32_t symIdx = local->getLiveLocalIndex();

      if (symIdx != INVALID_LIVENESS_INDEX && needTempForCommonedLoads->get(symIdx))
         {
         // hash is done on the original node
         TR::SymbolReference *symRef0 = (TR::SymbolReference*) findTempSym(origNode);
         if (symRef0)
            {
            if (trace())
               traceMsg(comp(),"         replacing symRef on duplicate node " POINTER_PRINTF_FORMAT " (of original node " POINTER_PRINTF_FORMAT ") with temp symRef " POINTER_PRINTF_FORMAT "\n", newNode, origNode, symRef0);
            newNode->setSymbolReference(symRef0);
            }
         }
      }

   for (int32_t i = newNode->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *newChild  = newNode->getChild(i);
      TR::Node *origChild = origNode->getChild(i);
      replaceLoadsWithTempSym(newChild, origChild, needTempForCommonedLoads);
      }
   }


// A single store that is being replicated to a side exit may have 0 or more nodes replaced with anchored nodes for different reasons
// insertAnchoredNodes will take care of all possible node replacements so only a single recursive walk of a store is required
uint32_t
TR_SinkStores::insertAnchoredNodes(TR_MovableStore *store,
                                   TR::Node *nodeCopy,
                                   TR::Node *nodeOrig,
                                   TR::Node *nodeParentCopy,
                                   int32_t childNum,
                                   TR_BitVector *needTempForCommonedLoads,
                                   TR_BitVector *blockingUsedSymbols,
                                   TR_BitVector *blockingCommonedSymbols,
                                   TR::Block *currentBlock,
                                   List<TR_IndirectLoadAnchor> *indirectLoadAnchorsForThisStore,
                                   vcount_t visitCount)
   {
   uint32_t increment = 0;

   // insertAnchoredNodes is called from two places
   // 1. When creating the side exit store tree this function will replace the duplicated tree with references to
   //    anchored nodes. In this case, nodeParentCopy != NULL and all nodes must be visited so duplicated references
   //    within the tree are all replaced. To avoid very deep recursion MAX_TREE_DEPTH_TO_DUP is checked before calling
   //    this function.
   // 2. When creating anchors for the fall through store. In this case, nodeParentCopy == NULL and all nodes need only be
   //    visited once because only a single anchor has to be created even if the fall through store tree has multiple
   //    references to the same node (the multiple references will naturally refer to the single anchored copy)
   if (nodeParentCopy == NULL)
      {
      vcount_t oldVisitCount = nodeOrig->getVisitCount();
      if (oldVisitCount == visitCount)
         {
         if (0 && trace())
            traceMsg(comp(),"         oldVisitCount == visitCount == %d so do not recurse\n",visitCount);
         return increment;
         }
      else
         {
         if (0 && trace())
            traceMsg(comp(),"         oldVisitCount (%d) != visitCount (%d) so do recurse\n",oldVisitCount,visitCount);
         nodeOrig->setVisitCount(visitCount);
         }
      }
      // only recurse each parent once
   else
      {
      vcount_t oldVisitCount = nodeParentCopy->getVisitCount();
      if (oldVisitCount == visitCount)
         {
         return increment;
         }
      }

   // In this case only anchors for blockingUsedSymbols and blockingCommonedSymbols need to be created as indirect load nodes
   // are naturally commoned correctly for the fall through sunk store
   if (indirectLoadAnchorsForThisStore && nodeOrig->getOpCode().isLoadIndirect())
      {
      TR_HashId locateID;
      TR::Node *anchoredNode = NULL;
      if (trace())
         traceMsg(comp(),"      insertAnchoredNodes found indirectLoad at %p, increment %d -> %d\n",nodeOrig,increment,increment+1);
      if (_indirectLoadAnchorMap->locate(nodeOrig, locateID))
         {
         anchoredNode = (TR::Node*)_indirectLoadAnchorMap->getData(locateID);
         anchoredNode->incFutureUseCount();
         }
      else
         {
         anchoredNode = TR::Node::create(TR::treetop, 1, nodeOrig);
         TR_ASSERT(false /*cg()->useBroaderEBBDefinition()*/, "expected EnableBroaderEBBDefinition to be set");
         anchoredNode->setFutureUseCount(1);
         TR_HashId addID = 0;
         _indirectLoadAnchorMap->add(nodeOrig, addID, anchoredNode);
         }
      if (trace())
         traceMsg(comp(),"create loadAnchored for node %p at currentBlock %d with nodeOrig %p for store node %p\n",anchoredNode,currentBlock->getNumber(),nodeOrig,store->_useOrKillInfo->_tt->getNode());
      indirectLoadAnchorsForThisStore->add(new (trStackMemory()) TR_IndirectLoadAnchor(anchoredNode, currentBlock, store->_useOrKillInfo->_tt));
      if (trace())
         traceMsg(comp(),"      indirectCase  nodeParentCopy %p setting childnum %d with nodeOrig %p\n",nodeParentCopy,childNum,nodeOrig);
      increment++;

      TR::SymbolReference* symRef = nodeOrig->getSymbolReference();

      if (symRef)
         {
         TR::SparseBitVector indirectMethodMetaUses(comp()->allocator());
         symRef->getUseDefAliases(false).getAliases(indirectMethodMetaUses);

         bool updated = false;
         if (trace() && !indirectMethodMetaUses.IsZero())
            {
            traceMsg(comp(),"           indirectMethodMetaUses:\n");
            (*comp()) << indirectMethodMetaUses << "\n";
            }
         updated = !indirectMethodMetaUses.IsZero();
         if (updated && trace())
            {
            traceMsg(comp(),"updating symbolsUsed in create loadAnchored\n");
            traceMsg(comp(), "BEF  _symbolsUsedInBlock[%d]: ",currentBlock->getNumber());
            _symbolsUsedInBlock[currentBlock->getNumber()]->print(comp());
            traceMsg(comp(), "\n");
            }
         TR::SparseBitVector::Cursor aliasesCursor(indirectMethodMetaUses);
         for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
            {
            int32_t usedSymbolIndex;
            TR::SymbolReference *usedSymRef = comp()->getSymRefTab()->getSymRef(aliasesCursor);
            TR::RegisterMappedSymbol *usedSymbol = usedSymRef->getSymbol()->getMethodMetaDataSymbol();
            if (usedSymbol)
               {
               usedSymbolIndex = usedSymbol->getLiveLocalIndex();
               if (usedSymbolIndex != INVALID_LIVENESS_INDEX && usedSymbolIndex < _liveVarInfo->numLocals())
                  {
                  if (trace())
                     {
                     traceMsg(comp(),"               update _symbolsUsedInBlock[%d] for anchor set symIdx %d\n",currentBlock->getNumber(), usedSymbolIndex);
                     }
                  _symbolsUsedInBlock[currentBlock->getNumber()]->set(usedSymbolIndex);
                  }
               }
            }
         if (updated && trace())
            {
            traceMsg(comp(), "AFT  _symbolsUsedInBlock[%d]: ",currentBlock->getNumber());
            _symbolsUsedInBlock[currentBlock->getNumber()]->print(comp());
            traceMsg(comp(), "\n");
            }
         }

      if (nodeParentCopy)
         {
         // if the node is being replaced then there is no need to search below because the side exit tree being created
         // can now not reference anything below this point
         // in the fall through case (where the node is not being replaced) then the children of this indirect load must be
         // searched in case there are first uses of commoned or blocked symbols that should be anchored
         nodeParentCopy->setAndIncChild(childNum, nodeOrig);
         return increment;
         }
      }
/*
   else if (nodeOrig->getOpCode().isLoadConst() && !nodeParentCopy)
      {
      // Must uncommon any const children as these may not be evaluated into registers at the anchor point
      // If they are only first evaluated below the anchor then the register may be commoned and this would be illegal
      // as the evaluation point may no longer be on a dominating path
      // iand    // anchor point
      //   iload
      //   iconst // not evaluated into a register
      // ifbucmpne -> sideExit1
      // ...
      // bustore sym1
      //    iand
      //       =>iload
      //         iconst // evaluated into a register now
      // ...
      // sideExit1:
      // bustore sym1
      //    iand
      //       =>iload
      //       =>iconst // illegally commoned from a path that does not dominate
      TR::Node *nodeClone = TR::Node::copy(nodeOrig, comp());
      nodeOrig->decReferenceCount();
      nodeParentOrig->setAndIncChild(childNum,nodeClone);
      }
*/
   else if (nodeOrig->getOpCode().isLoadVarDirect() && nodeOrig->getOpCode().hasSymbolReference() &&
            !nodeOrig->getSymbolReference()->getSymbol()->isStatic() && getSinkableSymbol(nodeOrig)->getLiveLocalIndex() != INVALID_LIVENESS_INDEX)
      {
      TR::RegisterMappedSymbol *local = getSinkableSymbol(nodeOrig);
      TR_ASSERT(local,"invalid local symbol\n");
      int32_t symIdx = local->getLiveLocalIndex();
      TR_FirstUseOfLoad *firstUseOfLoad = NULL;
      // Only need an anchor for killedCommoned loads when calling this function for sideExit store trees (when nodeCopy == NULL)
      // In the case where the current store is the one being moved to the lowest fall through path we are not duplicating and therefore no uncommoning
      // takes place and therefore no temp anchor is required as the original, naturally anchored, load will be used.
      if (nodeCopy && needTempForCommonedLoads && needTempForCommonedLoads->isSet(symIdx) && store->containsKilledCommonedLoad(nodeOrig))
         {
         firstUseOfLoad = findFirstUseOfLoad(nodeOrig);
         TR_ASSERT(firstUseOfLoad,"killedCommoned case: firstUseOfLoad should already exist\n");
         if (trace())
            traceMsg(comp(),"      insertAnchoredNodes needTemp for symIdx %d found firstUse %p with node %p and anchor treetop %p\n",symIdx,firstUseOfLoad,nodeOrig,firstUseOfLoad->getAnchorLocation());
         }
      else if (blockingCommonedSymbols && blockingCommonedSymbols->isSet(symIdx) && store->containsSatisfiedAndNotKilledCommonedLoad(nodeOrig))
         {
         firstUseOfLoad = findFirstUseOfLoad(nodeOrig);
         TR_ASSERT(firstUseOfLoad,"commonedBlocked case: firstUseOfLoad should already exist\n");
         if (trace())
            traceMsg(comp(),"      insertAnchoredNodes commonedBlocked for symIdx %d found firstUse %p with node %p and anchor treetop %p\n",symIdx,firstUseOfLoad,nodeOrig,firstUseOfLoad->getAnchorLocation());
         }
      else if (blockingUsedSymbols && blockingUsedSymbols->isSet(symIdx) && !store->containsCommonedLoad(nodeOrig))
         {
         firstUseOfLoad = findFirstUseOfLoad(nodeOrig);
         // A first use may already exist for a blocking used symbol load if this node itself is commoned somewhere below
         if (!firstUseOfLoad)
            {
            firstUseOfLoad = new (trStackMemory()) TR_FirstUseOfLoad(nodeOrig, store->_useOrKillInfo->_tt, store->_useOrKillInfo->_block->getNumber());
            TR_HashId addID = 0;
            _firstUseOfLoadMap->add(nodeOrig, addID, firstUseOfLoad);
            if (trace())
               traceMsg(comp(),"      insertAnchoredNodes usedBlocked create/add for symIdx %d  firstUse %p with node %p and anchor treetop\n",symIdx,firstUseOfLoad,nodeOrig,store->_useOrKillInfo->_tt->getNode());
            }
         else
            {
            if (trace())
               traceMsg(comp(),"      insertAnchoredNodes usedBlocked found for symIdx %d firstUse %p with node %p and anchor treetop %p\n",symIdx,firstUseOfLoad,nodeOrig,firstUseOfLoad->getAnchorLocation());
            }
         }
      TR_ASSERT(!firstUseOfLoad || (firstUseOfLoad->getNode() == nodeOrig),"nodes should match, firstUseOfLoad->getNode() = %p, nodeOrig = %p\n",firstUseOfLoad->getNode(),nodeOrig);
      if (firstUseOfLoad)
         {
         if (!firstUseOfLoad->isAnchored())
            {
            TR::Node *anchoredLoad =  TR::Node::create(TR::treetop, 1, nodeOrig);
            TR_ASSERT(false /*cg()->useBroaderEBBDefinition()*/, "expected EnableBroaderEBBDefinition to be set");
            TR::TreeTop *anchoredTT = TR::TreeTop::create(comp(), anchoredLoad);
            firstUseOfLoad->getAnchorLocation()->insertBefore(anchoredTT);
            // This anchor creates a new use of symIdx in the anchoring block so update _symbolsUsedInBlock so
            // earlier stores are not incorrectly sunk past this anchor
            _symbolsUsedInBlock[firstUseOfLoad->getAnchorBlockNumber()]->set(symIdx);
            firstUseOfLoad->setIsAnchored();
            if (trace())
            {
            traceMsg(comp(),"            anchor firstUse %p of load %p before %p\n",firstUseOfLoad,nodeOrig,anchoredTT->getNode());
            traceMsg(comp(),"               update _symbolsUsedInBlock[%d] for anchor set symIdx %d\n",firstUseOfLoad->getAnchorBlockNumber(),symIdx);
            traceMsg(comp(),"               _symbolsUsedInBlock[%d] ",firstUseOfLoad->getAnchorBlockNumber());
            _symbolsUsedInBlock[firstUseOfLoad->getAnchorBlockNumber()]->print(comp());
            traceMsg(comp(), "\n");
            }

            _numFirstUseAnchors++;
            }
         if (nodeCopy)
            {
            TR_ASSERT(nodeParentCopy,"if nodeCopy is not NULL then nodeParentCopy should be not NULL too\n");
            if (trace())
               traceMsg(comp(),"            replace nodeCopy with nodeOrig %p on dup\n",nodeOrig);
            nodeCopy->recursivelyDecReferenceCount();
            nodeParentCopy->setAndIncChild(childNum, nodeOrig);
            nodeCopy = nodeOrig;
            }
         }
      }

   for (int32_t i = nodeOrig->getNumChildren()-1; i >= 0; i--)
      {
      increment+=insertAnchoredNodes(store, nodeCopy ? nodeCopy->getChild(i) : NULL,
                                     nodeOrig->getChild(i),
                                     nodeCopy,
                                     i,
                                     needTempForCommonedLoads,
                                     blockingUsedSymbols,
                                     blockingCommonedSymbols,
                                     currentBlock,
                                     indirectLoadAnchorsForThisStore,
                                     visitCount);
      }
   if (nodeCopy)
      nodeCopy->setVisitCount(visitCount);
   return increment;
   }

// TR_TrivialSinkStores is less restrictive in its conditions below as it allows a used symbol to interfere with a killed
// symbol below. In this an anchor to the symbol will be created above its first use.
bool TR_TrivialSinkStores::storeCanMoveThroughBlock(TR_BitVector *blockKilledSet, TR_BitVector *blockUsedSet, int symIdx)
   {
   return (blockKilledSet == NULL || !blockKilledSet->get(symIdx))
          &&
          (blockUsedSet == NULL || (!blockUsedSet->intersects(*_killedSymbolsToMove) && !blockUsedSet->get(symIdx)));
   }

bool TR_SinkStores::storeCanMoveThroughBlock(TR_BitVector *blockKilledSet, TR_BitVector *blockUsedSet, int symIdx, TR_BitVector *allBlockUsedSymbols, TR_BitVector *allBlockKilledSymbols)
   {
   bool canMove =
      (blockKilledSet == NULL || (!blockKilledSet->intersects(*_usedSymbolsToMove) && !blockKilledSet->get(symIdx)))
      &&
      (blockUsedSet == NULL || (!blockUsedSet->intersects(*_killedSymbolsToMove) && !blockUsedSet->get(symIdx)));

   if (canMove)
      {
      if (allBlockUsedSymbols != NULL)
         (*allBlockUsedSymbols) |= (*blockUsedSet);
      if (allBlockKilledSymbols != NULL)
         (*allBlockKilledSymbols) |= (*blockKilledSet);
      }

   return canMove;
   }


TR::TreeTop *TR_TrivialSinkStores::genSideExitTree(TR::TreeTop *storeTree, TR::Block *exitBlock, bool isFirstGen)
   {
   TR::Node *storeNode = storeTree->getNode();
   TR_ASSERT(storeNode->getFirstChild()->getOpCodeValue() == TR::computeCC,"only TR::computeCC currently supported by genSideExitTree\n");
   TR::Node *opNode = storeNode->getFirstChild()->getFirstChild();
   if (isFirstGen)
      {
      // Anchor only the first time a sideExitTree is being generated for a particular store
      // It is necessary to anchor the grandchildren of the computeCC instead of just the child because lowerTrees will uncommon
      // the computeCC node anyway as cc computations require that the opNode operation is done right before the cc sequence is
      // generated. Therefore the grandchildren must be anchored.
      // bustore      <- storeNode
      //   computeCC
      //     opNode (iadd/isub/iand... etc)
      //       child1   <- anchor this node (except for const nodes, these are uncommoned -- duplicated in the sideExitTree)
      //       child2   <- anchor this node (except for const nodes, these are uncommoned -- duplicated in the sideExitTree)
      for (int32_t i = 0; i < opNode->getNumChildren(); i++)
         {
         if (!opNode->getChild(i)->getOpCode().isLoadConst())
            {
            TR::Node *anchoredNode =  TR::Node::create(TR::treetop, 1, opNode->getChild(i));
            if (trace())
              traceMsg(comp(),"      genSideExitTree anchoring computeCC grandchild %p in new node %p before node %p\n",opNode->getChild(i),anchoredNode,storeTree->getNode());
            TR_ASSERT(false /*cg()->useBroaderEBBDefinition()*/, "expected EnableBroaderEBBDefinition to be set");
            TR::TreeTop *anchoredTT = TR::TreeTop::create(comp(), anchoredNode);
            storeTree->insertBefore(anchoredTT);
            }
         else if (trace())
            {
            traceMsg(comp(),"      genSideExitTree not anchoring const computeCC grandchild %p\n",opNode->getChild(i));
            }
         }
      }

   TR::Node *opNodeClone = TR::Node::copy(opNode);
   for (int32_t i = 0; i < opNode->getNumChildren(); i++)
      {
      TR::Node *child = opNode->getChild(i);
      if (child->getOpCode().isLoadConst())
         {
         TR::Node *constClone = TR::Node::copy(child);
         opNodeClone->setChild(i,constClone);
         constClone->setReferenceCount(1);
         }
      else
         {
         opNodeClone->setAndIncChild(i, opNode->getChild(i));
         }
      }
   TR::Node *computeCCNode = TR::Node::create(TR::computeCC, 1, opNodeClone);
   opNodeClone->setReferenceCount(1);
   if (trace())
      traceMsg(comp(),"      genSideExitTree creating opNodeClone %p (refCount = %d) from opNode %p (refCount = %d)\n",opNodeClone,opNodeClone->getReferenceCount(),opNode,opNode->getReferenceCount());
   return TR::TreeTop::create(comp(),  exitBlock->getEntry(), TR::Node::createWithSymRef(storeNode->getOpCodeValue(), 1, 1, computeCCNode, storeNode->getSymbolReference()));
   }

bool TR_TrivialSinkStores::passesAnchoringTest(TR_MovableStore *store, bool storeChildIsAnchored, bool nextStoreWasMoved)
   {
   // The test below is trying to catch the very common case:
   // bustore cc
   //   computeCC
   //     opNode (iadd/isub/iand... etc)
   //       iload sym1
   //       =>iiload
   // istore sym1
   //   =>opNode
   // In this case the istore sym1 will not be sunk but we do want to sink the bustore cc. Because the bustore contains both an indirectLoad and
   // a used sym that is killed below (sym1) two anchors are required.
   // Instead we might as well just anchor the two children of the opNode to better deal with the case where they are more complex expressions.
   // Blindly duplicating the entire tree and just inserting the minimally required anchors will cause lots of uncommoning to occur as a result
   // of the duplication.
   TR::TreeTop *storeTree = store->_useOrKillInfo->_tt;
   TR::Node *storeNode = storeTree->getNode();

   if (!comp()->getOption(TR_DisableStoreAnchoring) &&
       store->_useOrKillInfo->_indirectLoadCount &&                           // if it contains an indirectLoad will need at least one anchor
       !storeChildIsAnchored &&                                               // only do this the once for a particular store
       !nextStoreWasMoved &&                                                  // is next treetop anchored?
       storeTree->getNextTreeTop()->getNode()->getOpCode().isStoreDirect() &&
       storeNode->getFirstChild()->getOpCodeValue() == TR::computeCC &&
       storeTree->getNextTreeTop()->getNode()->containsNode(storeNode->getFirstChild()->getFirstChild(),comp()->incVisitCount()))
      {
      return true;
      }
   else
      {
      return false;
      }
   }

TR::TreeTop *
TR_TrivialSinkStores::duplicateTreeForSideExit(TR::TreeTop *tree)
   {
   _nodesClonedForSideExitDuplication->clear();
   return TR::TreeTop::create(comp(),
                             duplicateNodeForSideExit(tree->getNode()),
                             tree->getPrevTreeTop(),
                             tree->getNextTreeTop());
   }

TR::Node *
TR_TrivialSinkStores::duplicateNodeForSideExit(TR::Node *node)
   {
   if (0 && trace())
      traceMsg(comp(),"dupTree node %p\n",node);

   TR_HashId id;
   if (_nodesClonedForSideExitDuplication->locate(node, id))
   {
      if (trace())
         traceMsg(comp(),"  found node cloned already %p\n",_nodesClonedForSideExitDuplication->getData(id));
      return (TR::Node*) _nodesClonedForSideExitDuplication->getData(id);
   }

   int32_t numChildren = node->getNumChildren();

   TR::Node * newRoot = TR::Node::copy(node);
   _nodesClonedForSideExitDuplication->add(node, id, newRoot);

   if (node->getOpCode().hasSymbolReference())
     newRoot->setSymbolReference(node->getSymbolReference());
   newRoot->setReferenceCount(0);

   // indirect loads will always be anchored on the mainline path so no point in duplicating below them
   // In all cases for a sideExitStoreTree insertAnchoredNodes will replace newRoot with a reference to an anchored mainline node
   if (node->getOpCode().isLoadIndirect())
      {
      if (0 && trace())
         traceMsg(comp(),"  found indirect load return newRoot %p\n",newRoot);
      return newRoot;
      }

   for (int i = 0; i < numChildren; i++)
      {
      TR::Node* child = node->getChild(i);
      if (child)
         {
         if (0 && trace())
            traceMsg(comp(),"  no indirect load found so recurse on child %p\n",child);
         TR::Node * newChild = duplicateNodeForSideExit(child);
         newRoot->setAndIncChild(i, newChild);
         }
      }
   if (0 && trace())
      traceMsg(comp(),"  base case return newRoot %p\n",newRoot);
   return newRoot;
   }

bool TR_TrivialSinkStores::sinkStorePlacement(TR_MovableStore *store,
                                              bool nextStoreWasMoved)

   {
   TR::Block *sourceBlock = store->_useOrKillInfo->_block;
   if (trace() || comp()->getOption(TR_TraceOptDetails))
      traceMsg(comp(), "            Start block is %d\n", sourceBlock->getNumber());
   int32_t sourceBlockNumber = sourceBlock->getNumber();
   TR::Block *currentBlock = sourceBlock;
   int32_t currentBlockNumber = sourceBlockNumber;
   TR::Block *fallThroughBlock = currentBlock->getNextBlock();

   bool sunkStore = false;
   bool canMoveToAllPaths = false;

   vcount_t visitCount = comp()->incVisitCount();
   TR_BitVector *allUsedSymbols;

   // allUsedSymbols will include the ones that are commoned if temp is not used; this is used for dataflow analysis
   // when the store is attempted to be sunk out of ebb
   TR_BitVector *usedSymbols = store->_useOrKillInfo->_usedSymbols;
   TR_BitVector *commonedSymbols = store->_commonedLoadsUnderTree;
   if (commonedSymbols)
      {
      if (trace())
         traceMsg(comp(),"         commonedSymbols = true so set allUsedSymbols to usedSymbols | commonedSymbols\n");
      allUsedSymbols = new (trStackMemory()) TR_BitVector(*usedSymbols);
      (*allUsedSymbols) |= (*commonedSymbols);
      }
   else
      {
      if (trace())
         traceMsg(comp(),"         commonedSymbols = false so set allUsedSymbols to usedSymbols\n");
      allUsedSymbols = usedSymbols;
      }

   sourceBlock->setVisitCount(visitCount);
   _usedSymbolsToMove = allUsedSymbols;
   TR_BitVector *killedCommonedSymbols = NULL;
   if (commonedSymbols && store->_needTempForCommonedLoads && !store->_needTempForCommonedLoads->isEmpty())
      killedCommonedSymbols = store->_needTempForCommonedLoads;
   if (trace())
      {
      traceMsg(comp(),"         usedSymbols:  ");
      usedSymbols->print(comp());
      traceMsg(comp(),"\n");
      traceMsg(comp(),"         allUsedSymbols:  ");
      allUsedSymbols->print(comp());
      traceMsg(comp(),"\n");
      traceMsg(comp(),"         _usedSymbolsToMove:  ");
      _usedSymbolsToMove->print(comp());
      traceMsg(comp(),"\n");
      traceMsg(comp(),"         commonedSymbols:  ");
      if (commonedSymbols) commonedSymbols->print(comp());
      traceMsg(comp(),"\n");
      traceMsg(comp(),"         killedCommonedSymbols:  ");
      if (killedCommonedSymbols) killedCommonedSymbols->print(comp());
      traceMsg(comp(),"\n");
      traceMsg(comp(),"         _killedSymbolsToMove:  ");
      _killedSymbolsToMove->print(comp());
      traceMsg(comp(),"\n");
      }

   if (trace())
      {
      traceMsg(comp(),"         source blockNumber %d symbols killed:  ",sourceBlockNumber);
      _symbolsKilledInBlock[sourceBlockNumber]->print(comp());
      traceMsg(comp(),"\n");
      traceMsg(comp(),"         source blockNumber %d symbols used:  ",sourceBlockNumber);
      _symbolsUsedInBlock[sourceBlockNumber]->print(comp());
      traceMsg(comp(),"\n");
      }

   bool isFirstMoveOfThisStore = true;
   TR_BlockStorePlacementList blockPlacementsForThisStore(trMemory());
   List<TR_IndirectLoadAnchor> *indirectLoadAnchorsForThisStore = new (trStackMemory()) List<TR_IndirectLoadAnchor>(trMemory());
   bool storeChildIsAnchored = false;
   bool anchoredByBranch = false;
   TR::Node *anchoredByBranchNode = NULL;
   bool anchoredByStore = false;
   int32_t symIdx = store->_symIdx;
   TR_BitVector *blockingCommonedSymbols = NULL;
   TR_BitVector *blockingUsedSymbols = NULL;
   TR::TreeTop *storeTree = store->_useOrKillInfo->_tt;
   uint32_t indirectLoadCount = store->_useOrKillInfo->_indirectLoadCount;
   while (fallThroughBlock && fallThroughBlock->isExtensionOfPreviousBlock())
      {
      int32_t currentBlockNumber = currentBlock->getNumber();
      if (trace())
         {
         traceMsg(comp(),"         blockNumber %d symbols killed:  ",currentBlockNumber);
         _symbolsKilledInBlock[currentBlockNumber]->print(comp());
         traceMsg(comp(),"\n");
         traceMsg(comp(),"         blockNumber %d symbols used:  ",currentBlockNumber);
         _symbolsUsedInBlock[currentBlockNumber]->print(comp());
         traceMsg(comp(),"\n");
         traceMsg(comp(),"         blockKilledSet->intersects(*usedSymbols) = %d\n",_symbolsKilledInBlock[currentBlockNumber]->intersects(*usedSymbols));
         if (commonedSymbols)
            traceMsg(comp(),"         blockKilledSet->intersects(*commonedSymbols) = %d\n",_symbolsKilledInBlock[currentBlockNumber]->intersects(*commonedSymbols));
         else
            traceMsg(comp(),"         commonedSymbols is NULL\n");
         traceMsg(comp(),"         Four storeCanMoveThroughBlock conditions, if any are true then the store cannot be sunk:\n");
         if (_symbolsKilledInBlock[currentBlockNumber])
            {
            traceMsg(comp(),"         blockKilledSet->intersects(*_usedSymbolsToMove) = %d\n",_symbolsKilledInBlock[currentBlockNumber]->intersects(*_usedSymbolsToMove));
            traceMsg(comp(),"         blockKilledSet->get(%d) = %d\n",symIdx,_symbolsKilledInBlock[currentBlockNumber]->get(symIdx));
            }
         else
            traceMsg(comp(),"         blockKilledSet is NULL\n");
         if (_symbolsUsedInBlock[currentBlockNumber])
            {
            traceMsg(comp(),"         blockUsedSet->intersects(*_killedSymbolsToMove) = %d\n",_symbolsUsedInBlock[currentBlockNumber]->intersects(*_killedSymbolsToMove));
            traceMsg(comp(),"         blockUsedSet->get(%d) = %d\n",symIdx,_symbolsUsedInBlock[currentBlockNumber]->get(symIdx));
            }
         else
            traceMsg(comp(),"         blockUsedSet is NULL\n");
         }
      if (storeCanMoveThroughBlock(_symbolsKilledInBlock[currentBlockNumber], _symbolsUsedInBlock[currentBlockNumber], symIdx))
         {
         TR::Node *storeNode  = storeTree->getNode();
         TR::RegisterMappedSymbol *sym = storeNode->getSymbolReference()->getSymbol()->getRegisterMappedSymbol();
         if (_symbolsKilledInBlock[currentBlockNumber]->intersects(*usedSymbols))
            {
            //if (comp()->getOption(TR_SinkAllStores) || comp()->getOption(TR_SinkAllBlockedStores))
            if (comp()->getOption(TR_SinkAllStores) || 1)
               {
               TR_BitVector *blockingUsedSymbolsForThisBlock = new (trStackMemory()) TR_BitVector(*_symbolsKilledInBlock[currentBlockNumber]);
               (*blockingUsedSymbolsForThisBlock) &= (*usedSymbols);
               if (blockingUsedSymbols)
                  (*blockingUsedSymbols) |= (*blockingUsedSymbolsForThisBlock);
               else
                  blockingUsedSymbols = new (trStackMemory()) TR_BitVector(*blockingUsedSymbolsForThisBlock);
             if (trace())
                  {
                  traceMsg(comp(), "         blockingUsedSymbols: ");
                  blockingUsedSymbols->print(comp());
                  traceMsg(comp(), "\n");
                  }
               }
            else
               {
               if (trace())
                  traceMsg(comp(),"         not sinking store %p any further as it is not a cc and there are blocking used symbols\n",storeNode);
               break;
               }
            }

         if (commonedSymbols && _symbolsKilledInBlock[currentBlockNumber]->intersects(*commonedSymbols))
            {
            if (comp()->getOption(TR_SinkAllStores) || comp()->getOption(TR_SinkAllBlockedStores))
            //if (comp()->getOption(TR_SinkAllStores) || 1)
               {
               TR_BitVector *blockingCommonedSymbolsForThisBlock = new (trStackMemory()) TR_BitVector(*_symbolsKilledInBlock[currentBlockNumber]);
               (*blockingCommonedSymbolsForThisBlock) &= (*commonedSymbols);
               if (blockingCommonedSymbols)
                  (*blockingCommonedSymbols) |= (*blockingCommonedSymbolsForThisBlock);
               else
                  blockingCommonedSymbols = new (trStackMemory()) TR_BitVector(*blockingCommonedSymbolsForThisBlock);
             if (trace())
                  {
                  traceMsg(comp(), "         blockingCommonedSymbols: ");
                  blockingCommonedSymbols->print(comp());
                  traceMsg(comp(), "\n");
                  }
               }
            else
               {
               if (trace())
                  traceMsg(comp(),"         not sinking store %p any further as it is not a cc and there are blocking commoned symbols\n",store);
               break;
               }
            }

         if (trace() || comp()->getOption(TR_TraceOptDetails))
            traceMsg(comp(), "            Pushed sym %d to end of current block_%d\n", symIdx, currentBlockNumber);
         TR::TreeTop *lastTree = currentBlock->getLastRealTreeTop();
         TR::Block *exitBlock = NULL;
         bool copyStore = true;  // when the store is going sunk to the side exit should it be copied first and then sunk or just sunk
         TR::TreeTop *sideExitStoreTree = NULL;

         if (!(currentBlock->getSuccessors().size() == 1))
            {
            exitBlock = lastTree->getNode()->getBranchDestination()->getNode()->getBlock();
            if (lastTree->getNode()->getOpCode().isBranch())
               {
               if (trace())
                  traceMsg(comp(),"               found branch node [" POINTER_PRINTF_FORMAT "] as last node in block_%d\n",lastTree->getNode(),currentBlock->getNumber());
               // test !anchoredByBranch below instead of !storeChildIsAnchored to catch this case
               // S
               // ST
               // BC
               // if !storeChildIsAnchored is used then the cc computation is duplicated on the BC side exit instead of being anchored above the branch
               if (passesAnchoringTest(store, storeChildIsAnchored, nextStoreWasMoved))
                  {
                  copyStore = false;
                  canMoveToAllPaths = true;
                  storeChildIsAnchored = anchoredByStore = true;
                  sideExitStoreTree = genSideExitTree(storeTree, exitBlock, true);
                  if (trace())
                     traceMsg(comp(), "                  store anchor branch first - create new store %p with commoned store->firstGrandChild %p on sideExit block_%d\n",
                        sideExitStoreTree->getNode(),storeNode->getFirstChild()->getFirstChild(),exitBlock->getNumber());
                  }
               else if (!storeChildIsAnchored && indirectLoadCount)
                  {
                  sideExitStoreTree = storeTree;
                  canMoveToAllPaths = true;
                  if (trace())
                     traceMsg(comp(), "                  store has %d indirect load%s\n", indirectLoadCount, indirectLoadCount > 1 ? "s":"");
                  }
               else
                  {
                  canMoveToAllPaths = true;
                  if (trace())
                     traceMsg(comp(), "               store child %p is not anchored under the branch or a subsequent store\n",storeNode->getFirstChild());
                  if (anchoredByBranch)
                     {
                     if (trace())
                        traceMsg(comp(), "                  cc child already anchored\n");
                     copyStore = false;
                     TR_ASSERT(anchoredByBranchNode,"anchoredByBranchNode should have been created already when anchoredByBranch was set to true\n");
                     sideExitStoreTree = TR::TreeTop::create(comp(),  exitBlock->getEntry(), TR::Node::createWithSymRef(storeNode->getOpCodeValue(), 1, 1, storeNode->getFirstChild(), storeNode->getSymbolReference()));
                     if (trace())
                        traceMsg(comp(), "                  branch anchor next - create new store %p with commoned store->firstChild %p on sideExit block_%d\n",
                              sideExitStoreTree->getNode(),storeNode->getFirstChild(),exitBlock->getNumber());
                     }
                  else if(anchoredByStore)
                     {
                     if (trace())
                        traceMsg(comp(), "                  cc grand child already anchored\n");
                     copyStore = false;
                     sideExitStoreTree = genSideExitTree(storeTree, exitBlock, false);
                     if (trace())
                        traceMsg(comp(), "                  store anchor branch next - create new store %p with commoned store->firstChild %p on sideExit block_%d\n",
                           sideExitStoreTree->getNode(),storeNode->getFirstChild(),exitBlock->getNumber());
                     }
                  else
                     {
                     sideExitStoreTree = storeTree;
                     if (trace())
                        {
                        traceMsg(comp(), "                  cc child is not already anchored and should not be anchored here\n");
                        traceMsg(comp(), "                  create copy of storeTree %p on side exit\n",storeTree->getNode());
                        }
                     }
                  }
               }
            else
               {
               // add support for other block ending opcodes as needed
               }
            if ((trace() || comp()->getOption(TR_TraceOptDetails)) && canMoveToAllPaths)
               traceMsg(comp(), "               Replicate to branch side exit block_%d\n", exitBlock->getNumber());
            }
         else if (!currentBlock->getExceptionSuccessors().empty())
            {
            if (trace())
               traceMsg(comp(),"               found exception node [" POINTER_PRINTF_FORMAT "] as last node in block_%d\n",lastTree->getNode(),currentBlock->getNumber());
            // For now just support a single exception successor.
            if (currentBlock->getExceptionSuccessors().size() == 1)
               {
               // If the last node of an exception throwing block is not the one and only node that raises the exception
               // then we will have to split the block as the store may not be live on all exception paths out of the block
               // For now just require that an exception raising node must end a block. This is the excepted behaviour in zEmulator
               // control flow graphs.
               TR::TreeTop *tt = currentBlock->getFirstRealTreeTop();
               int32_t exceptingNodes = 0;
               for (; tt != lastTree->getNextTreeTop(); tt = tt->getNextRealTreeTop())
                  {
                  if (tt->getNode()->exceptionsRaised())
                     exceptingNodes++;
                  }
               if (exceptingNodes == 1)
                  {
                  exitBlock = currentBlock->getExceptionSuccessors().front()->getTo()->asBlock();
                  if (passesAnchoringTest(store, storeChildIsAnchored, nextStoreWasMoved))
                     {
                     copyStore = false;
                     canMoveToAllPaths = true;
                     storeChildIsAnchored = anchoredByStore = true;
                     sideExitStoreTree = genSideExitTree(storeTree, exitBlock, true);

                     if (trace())
                        traceMsg(comp(), "                  store anchor exception first - create new store %p with commoned store->firstGrandChild %p on sideExit block_%d\n",
                           sideExitStoreTree->getNode(),storeNode->getFirstChild()->getFirstChild(),exitBlock->getNumber());
                     }
                  else if (!storeChildIsAnchored && indirectLoadCount)
                     {
                     sideExitStoreTree = storeTree;
                     canMoveToAllPaths = true;
                     if (trace())
                        traceMsg(comp(), "                  store has %d indirect load%s\n", indirectLoadCount, indirectLoadCount > 1 ? "s":"");
                      }
                  else
                     {
                     canMoveToAllPaths = true;
                     if (anchoredByBranch)
                        {
                        if (trace())
                           traceMsg(comp(), "                  cc child already anchored\n");
                        copyStore = false;
                        TR_ASSERT(anchoredByBranchNode,"anchoredByBranchNode should have been created already when anchoredByBranch was set to true\n");
                        sideExitStoreTree = TR::TreeTop::create(comp(),
                                                               exitBlock->getEntry(),
                                                               TR::Node::createWithSymRef(storeNode->getOpCodeValue(), 1, 1,
                                                                               storeNode->getFirstChild(), storeNode->getSymbolReference()));
                        if (trace())
                           traceMsg(comp(), "                  create new store %p with commoned store->firstChild %p on sideExit block_%d\n",
                              sideExitStoreTree->getNode(),storeNode->getFirstChild(),exitBlock->getNumber());
                        }
                     else if (anchoredByStore)
                        {
                        if (trace())
                           traceMsg(comp(), "                  cc grand child already anchored\n");
                        copyStore = false;
                        sideExitStoreTree = genSideExitTree(storeTree, exitBlock, false);
                        if (trace())
                           traceMsg(comp(), "                  store anchor exception next - create new store %p with commoned store->firstChild %p on sideExit block_%d\n",
                                   sideExitStoreTree->getNode(),storeNode->getFirstChild(),exitBlock->getNumber());
                        }
                     else
                        {
                        sideExitStoreTree = storeTree;
                        if (trace())
                           {
                           traceMsg(comp(), "                  cc child is not already anchored and should not be anchored here\n");
                           traceMsg(comp(), "                  create copy of storeTree %p on side exit\n",storeTree->getNode());
                           }
                        }
                     }
                  }
               else
                  {
                  if (trace() || comp()->getOption(TR_TraceOptDetails))
                     traceMsg(comp(), "            missed a store sinking opportunity as a current block_%d with a single exception succ does not have exactly 1 excepting node (%d excepting nodes)\n",currentBlock->getNumber(),exceptingNodes);
                  }
               if ((trace() || comp()->getOption(TR_TraceOptDetails)) && canMoveToAllPaths)
                  traceMsg(comp(), "               Replicate to exception side exit block_%d\n", exitBlock->getNumber());
               }
            else
               {
               if (trace() || comp()->getOption(TR_TraceOptDetails))
                  traceMsg(comp(),"            not sinking store as there is > 1 exception successor for current block_%d\n",currentBlock->getNumber());
               }
            }
         else
            {
            canMoveToAllPaths = true; // can always move to only successor block (the fallThroughBlock)
            }

         // must be able to copy/move to all paths in order to move to any of them
         if (canMoveToAllPaths)
            {
            if (trace())
               traceMsg(comp(), "            Can sink this store [" POINTER_PRINTF_FORMAT "] !\n", storeTree->getNode());
            if (sideExitStoreTree)
               {
               TR_StoreInformation *storeInfo = new (trStackMemory()) TR_StoreInformation(sideExitStoreTree, copyStore);
               if (copyStore && store->_depth > MAX_TREE_DEPTH_TO_DUP)
                  {
                  if (trace())
                     traceMsg(comp(),            "no anchor found and depth %d is > than max duplication depth of %d so do not sink this store\n",store->_depth,MAX_TREE_DEPTH_TO_DUP);
                  break;
                  }
               if (copyStore &&
                   (indirectLoadCount || killedCommonedSymbols || blockingUsedSymbols || blockingCommonedSymbols))
                  {
                  storeInfo->_storeTemp = duplicateTreeForSideExit(sideExitStoreTree);
                  storeInfo->_needsDuplication = false;
                  if (trace())
                     traceMsg(comp(),"                  call insertAnchoredNodes dupTree is %p, copyStore = %d, indirectLoadCount = %d, killedCommonedSymbols = %p, blockingUsedSymbols = %p, blockingCommonedSymbols = %p\n",
                             storeInfo->_storeTemp->getNode(),copyStore,indirectLoadCount,killedCommonedSymbols,blockingUsedSymbols,blockingCommonedSymbols);
                  uint32_t indirectLoadsReplaced = insertAnchoredNodes(store, storeInfo->_storeTemp->getNode()->getFirstChild(), // copy
                                                                       sideExitStoreTree->getNode()->getFirstChild(),     // original
                                                                       storeInfo->_storeTemp->getNode(),                  // parent node of copy
                                                                       0,                                                 // child index of copy
                                                                       killedCommonedSymbols,
                                                                       blockingUsedSymbols,
                                                                       blockingCommonedSymbols,
                                                                       currentBlock,
                                                                       indirectLoadAnchorsForThisStore,
                                                                       comp()->incVisitCount());

                   // replaced must be >= count because indirectLoadCount is the unique count of indirect loads but the tree
                   // might contain multiple references to the same indirect load and they all must be replaced
                   TR_ASSERT(indirectLoadsReplaced >= indirectLoadCount,"did not find and replace all the indirect loads (replaced %d, found %d)\n",indirectLoadsReplaced,indirectLoadCount);
                  }
               else
                  {
                  storeInfo->_storeTemp = sideExitStoreTree;
                  if (trace())
                     traceMsg(comp(),"                  do not call insertAnchoredNodes dupTree is %p, copyStore = %d, indirectLoadCount = %d, killedCommonedSymbols = %p, blockingUsedSymbols = %p, blockingCommonedSymbols = %p\n",storeInfo->_storeTemp->getNode(),copyStore,indirectLoadCount,killedCommonedSymbols,blockingUsedSymbols,blockingCommonedSymbols);
                  }
               // exitBlock is NULL when the currentBlock does not contain any fork due to a branch or exception
               if (exitBlock)
                  {
                  TR_BlockStorePlacement *newBlockPlacement = new (trStackMemory()) TR_BlockStorePlacement(storeInfo, exitBlock, trMemory());
                  blockPlacementsForThisStore.add(newBlockPlacement);
                  }
               }
            currentBlock = fallThroughBlock;
            fallThroughBlock = currentBlock->getNextBlock();
            isFirstMoveOfThisStore = false;
            sunkStore = true;
            }
         else
            {
            fallThroughBlock = 0;
            }
         }
      else
         {
         if (trace() || comp()->getOption(TR_TraceOptDetails))
            traceMsg(comp(), "            Can't push sym %d to end of current block_%d\n", symIdx, currentBlockNumber);
         fallThroughBlock = 0;
         }
      } // end while

      if ((trace() || comp()->getOption(TR_TraceOptDetails)) && fallThroughBlock &&  !fallThroughBlock->isExtensionOfPreviousBlock())
         traceMsg(comp(),"            fallThroughBlock %d is not part of same superblock as currentBlock %d so will not try to sink further\n",fallThroughBlock->getNumber(),currentBlock->getNumber());

      // have we made any progress in pushing this block
      if (sourceBlock != currentBlock)
         {
         if ((trace() || comp()->getOption(TR_TraceOptDetails)) && canMoveToAllPaths)
            traceMsg(comp(), "               Move to lowest fall through block_%d\n", currentBlock->getNumber());
         TR_StoreInformation *storeInfo = new (trStackMemory()) TR_StoreInformation(storeTree, false /*!copyStore*/);
         storeInfo->_storeTemp = storeTree;
         bool anchorIndirectLoads = (indirectLoadCount != 0);
         // indirect loads have to be anchored right at their original location otherwise if the parent node is swung down
         // then the indirect load may get moved passed a store to the same memory location (or loads will get reordered which
         // is also not allowed). This happens, among other places, in the IL for TS where the iiload under the bustore cc gets
         // swung below the iistore to the same memory location.
         if (anchorIndirectLoads || blockingUsedSymbols || blockingCommonedSymbols)
            {
            if (trace())
               traceMsg(comp(),"                  call insertAnchoredNodes !storeChildisAnchored = %d, indirectLoadCount = %d, killedCommonedSymbols = %p, blockingUsedSymbols = %p, blockingCommonedSymbols = %p\n",
                       !storeChildIsAnchored,indirectLoadCount,killedCommonedSymbols,blockingUsedSymbols,blockingCommonedSymbols);
            // The fall through store only needs to have anchors created for blocked symbols (and
            // not for indirect loads) and the store tree itself does not need to be modified
            // so pass in NULL for the nodeCopy and nodeParentCopy
            uint32_t indirectLoadsReplaced = insertAnchoredNodes(store,
                                                                 NULL,                                      // copy
                                                                 storeTree->getNode()->getFirstChild(),     // original
                                                                 NULL,
                                                                 0,                                         // child index of copy
                                                                 killedCommonedSymbols,
                                                                 blockingUsedSymbols,
                                                                 blockingCommonedSymbols,
                                                                 currentBlock,
                                                                 anchorIndirectLoads ? indirectLoadAnchorsForThisStore : NULL,
                                                                 comp()->incVisitCount());

            // replaced must be >= count because indirectLoadCount is the unique count of indirect loads but the tree
            // might contain multiple references to the same indirect load and they all must be replaced
            if (anchorIndirectLoads)
               TR_ASSERT(indirectLoadsReplaced >= indirectLoadCount,"did not find and replace all the indirect loads (replaced %d, found %d)\n",indirectLoadsReplaced,indirectLoadCount);
            else
               TR_ASSERT(indirectLoadsReplaced == 0,"the fall through store should not replace any indirect loads\n");
            }
         else
            {
            if (trace())
               traceMsg(comp(),"                  do not call insertAnchoredNodes !storeChildisAnchored = %d, indirectLoadCount = %d, killedCommonedSymbols = %p, blockingUsedSymbols = %p, blockingCommonedSymbols = %p\n",
                       !storeChildIsAnchored,indirectLoadCount,killedCommonedSymbols,blockingUsedSymbols,blockingCommonedSymbols);
            }
         TR_BlockStorePlacement *newBlockPlacement = new (trStackMemory()) TR_BlockStorePlacement(storeInfo, currentBlock, trMemory());
         blockPlacementsForThisStore.add(newBlockPlacement);
         sunkStore = true;
         }

      // transfer our block placements to _placementsInBlock and _allBlockPlacements
      ListIterator<TR_BlockStorePlacement> blockIt(&blockPlacementsForThisStore);
      for (TR_BlockStorePlacement *blockPlacement=blockIt.getFirst();
           blockPlacement != NULL;
           blockPlacement = blockIt.getNext())
         {
         recordPlacementForDefInBlock(blockPlacement);
         }

      // The stores are examined in reverse order but the exit blocks are encountered in forward order as the store is pushed
      // down. So for the program order stores (S) and exits (E)
      // S_a
      // E_1a
      // E_2a
      // E_3a
      // S_b
      // E_1b
      // E_2b
      // E_3b
      // The stores are visited in the order S_b, S_a but for each store the exit blocks are visited in the order E_1,E_2,E_3.
      // So the futureUseCounts will work correctly so the indirect load is anchored before its first use we want the master list of
      // _indirectLoadAnchors to be in reverse order (the first loadAnchor in program order is the last one dealt with in doSinking)
      //
      // This ordering is accomplished by the following algorithm:
      // The indirectLoadAnchorsForThisStore are added to the *head* of the list so at the end it is in this order
      // where L_1a is the reference to an anchored load for the pair S_a and E_1a.
      // for S_a
      // L_3a,L_2a,L_1a
      // and for S_b
      // L_3b,L_2b,L_1b
      //
      // The loop below *appends* indirectLoadAnchorsForThisStore to the master list.
      // Before the indirectLoadAnchorsForThisStore for S_b are added the master list is empty and after they are appended the master list is:
      // L_3b,L_2b,L_1b
      // And after the indirectLoadAnchorsForThisStore for S_a are appended to the master list:
      // L_3b,L_2b,L_1b,L_3a,L_2a,L_1a
      // and this list is now in the reverse order we need for the futureUseCounts to work correctly in doSinking.
      //
      // How doSinking uses the futureUseCounts:
      // Assuming each side exit tree references the same indirect load anchor then after all the exits have been seen the futureUseCount will
      // be 6. When doSinking processes the list it will decrement the futureUseCount each time it encounters a regStore and then
      // finally anchor it when the futureUseCount has reached zero.  Because L_1a shows up last in the list doSinking will correctly
      // anchor the indirect load before this location

      while (!indirectLoadAnchorsForThisStore->isEmpty())
         _indirectLoadAnchors->append(indirectLoadAnchorsForThisStore->popHead());

      return sunkStore;
   }

const char *
TR_TrivialSinkStores::optDetailString() const throw()
   {
   return "O^O TRIVIAL SINK STORES: ";
   }


bool TR_GeneralSinkStores::sinkStorePlacement(TR_MovableStore *movableStore,
                                              bool nextStoreWasMoved)
   {
   TR::Block *sourceBlock = movableStore->_useOrKillInfo->_block;
   TR::TreeTop *tt = movableStore->_useOrKillInfo->_tt;
   int32_t symIdx = movableStore->_symIdx;
   TR_BitVector *usedSymbols = movableStore->_useOrKillInfo->_usedSymbols;
   uint32_t indirectLoadCount = movableStore->_useOrKillInfo->_indirectLoadCount;
   TR_BitVector *commonedSymbols = movableStore->_commonedLoadsUnderTree;
   TR_BitVector *needTempForCommonedLoads = movableStore->_needTempForCommonedLoads;

   int32_t sourceBlockNumber=sourceBlock->getNumber();
   int32_t sourceBlockFrequency = sourceBlock->getFrequency();
   TR_OrderedBlockList worklist(trMemory());
   vcount_t visitCount = comp()->incVisitCount();
   TR_BitVector *allUsedSymbols;

   TR_BitVector *allEdgeInfoUsedOrKilledSymbols = new (trStackMemory()) TR_BitVector(*usedSymbols);
   allEdgeInfoUsedOrKilledSymbols->empty();

   TR_BitVector *allBlockUsedSymbols = new (trStackMemory()) TR_BitVector(*usedSymbols);
   allBlockUsedSymbols->empty();
   TR_BitVector *allBlockKilledSymbols = new (trStackMemory()) TR_BitVector(*usedSymbols);
   allBlockKilledSymbols->empty();

   // allUsedSymbols will include the ones that are commoned if temp is not used; this is used for dataflow analysis
   // when the store is attempted to be sunk out of ebb
   if (commonedSymbols)
      {
      allUsedSymbols = new (trStackMemory()) TR_BitVector(*usedSymbols);
      (*allUsedSymbols) |= (*commonedSymbols);

      usedSymbols = allUsedSymbols;
      }
   else
      allUsedSymbols = usedSymbols;

   // remove 0 for more detail tracing
   if (0 && trace())
      {
      traceMsg(comp(), "            usedSymbols = ");
      usedSymbols->print(comp());
      if (commonedSymbols)
         {
         traceMsg(comp(), ", allUsedSymbols = ");
         allUsedSymbols->print(comp());
         }
      traceMsg(comp(), "\n");
      }

   sourceBlock->setVisitCount(visitCount);
   _usedSymbolsToMove = usedSymbols;

   // can this store be pushed to the end of the block?
   if (movableStore->_isLoadStatic && blockContainsCall(sourceBlock->asBlock(), comp()))
      {
      if (trace())
         traceMsg(comp(), "            TT[" POINTER_PRINTF_FORMAT "] Can't push sym %d to end of source block_%d (static load)\n", tt->getNode(), symIdx, sourceBlock->getNumber());
      return false;
      }
   if (!storeCanMoveThroughBlock(_symbolsKilledInBlock[sourceBlockNumber], _symbolsUsedInBlock[sourceBlockNumber], symIdx, allBlockUsedSymbols, allBlockKilledSymbols))
      {
      if (trace())
         traceMsg(comp(), "            TT[" POINTER_PRINTF_FORMAT "] Can't push sym %d to end of source block_%d\n", tt->getNode(), symIdx, sourceBlock->getNumber());
      return false;
      }
   else if (!(_liveOnNotAllPaths->_outSetInfo[sourceBlockNumber]->get(symIdx)))
      {
      if (trace())
         traceMsg(comp(), "            TT[" POINTER_PRINTF_FORMAT "] LONAP is false for sym %d at the end of source block_%d\n", tt->getNode(), symIdx, sourceBlock->getNumber());
      return false;
      }

   if (trace())
      traceMsg(comp(), "            TT[" POINTER_PRINTF_FORMAT "] Pushed sym %d to end of source block_%d\n", tt->getNode(), symIdx, sourceBlock->getNumber());

   // this flag will be used to see if sinking is useful
   bool sunkPastNonLivePath = false;
   bool canSinkOutOfEBB = !commonedSymbols || !commonedSymbols->intersects(*_symbolsKilledInBlock[sourceBlockNumber]);
   bool placeDefInBlock = false;

   // these lists will hold the list of placements for this store
   // if we decide not to sink this store, then these placements will be discarded
   // otherwise, they will be added to the per-block lists, the per-edge lists, and the complete list of placements
   TR_BlockStorePlacementList blockPlacementsForThisStore(trMemory());
   TR_EdgeStorePlacementList  edgePlacementsForThisStore(trMemory());

   // now try to push it along sourceBlock's successor edges
   TR_SuccessorIterator outEdges(sourceBlock);
   for (auto succEdge = outEdges.getFirst(); succEdge; succEdge = outEdges.getNext())
      {
      TR::CFGNode *succBlock = succEdge->getTo();
      bool isSuccBlockInEBB = (succBlock->asBlock()->startOfExtendedBlock() == sourceBlock->startOfExtendedBlock());
      _usedSymbolsToMove = isSuccBlockInEBB ? usedSymbols : allUsedSymbols;

      if (trace())
         {
         traceMsg(comp(), "            trying to push to block_%d\n", succBlock->getNumber());
         traceMsg(comp(), "              LOSP->blockInfo[%d]: ", succBlock->getNumber());
         _liveOnSomePaths->_blockAnalysisInfo[succBlock->getNumber()]->print(comp());
         traceMsg(comp(), "\n");
         }

      if (_liveOnSomePaths->_blockAnalysisInfo[succBlock->getNumber()]->get(symIdx)
          // catch the case if the store tree interfere any moved store in the edge placement.  If so,
          // we should add this store to the edge placement list.
          //|| !isSafeToSinkThruEdgePlacement(symIdx, sourceBlock, succBlock)
         )
         {
         if (!canSinkOutOfEBB && !isSuccBlockInEBB)
            {
            if (trace())
               traceMsg(comp(), "              attempt to sink out of Ebb but commoned syms are killed in the Ebb => don't sink\n");
            placeDefInBlock = true;
            break;
            }
         if (!_sinkThruException && succBlock->asBlock()->isCatchBlock()
             //&& !checkLiveMergingPaths(succBlock->asBlock(), symIdx)  invalid check
            )
            {
            if (trace())
               traceMsg(comp(), "              attempt to sink at the catch block => don't sink\n");
            placeDefInBlock = true;
            break;
            }
         if (shouldSinkStoreAlongEdge(symIdx, sourceBlock, succBlock, sourceBlockFrequency, movableStore->_isLoadStatic, visitCount, allEdgeInfoUsedOrKilledSymbols))
            {
            if (trace())
               traceMsg(comp(), "              added %d to worklist\n", succBlock->getNumber());
            worklist.addInTraversalOrder(succBlock->asBlock(), true, succEdge);
            }
         else
            {
            bool copyStore=!isSuccBlockInEBB;
            //recordPlacementForDefAlongEdge(tt, succEdge, true); //copyStore);
            TR_StoreInformation *storeInfo = new (trStackMemory()) TR_StoreInformation(tt, copyStore);
            // init _storeTemp field
            if (!needTempForCommonedLoads)
               storeInfo->_storeTemp = tt;
            else
               {
               storeInfo->_storeTemp = tt->duplicateTree();
               replaceLoadsWithTempSym(storeInfo->_storeTemp->getNode(), tt->getNode(), needTempForCommonedLoads);
               }

            TR_BitVector *symsReferenced = new (trStackMemory()) TR_BitVector(*_killedSymbolsToMove);
            (*symsReferenced) |= (*_usedSymbolsToMove);
            TR_EdgeInformation *edgeInfo = new (trStackMemory()) TR_EdgeInformation(succEdge, symsReferenced);
            TR_EdgeStorePlacement *newEdgePlacement = new (trStackMemory()) TR_EdgeStorePlacement(storeInfo, edgeInfo, trMemory());
            edgePlacementsForThisStore.add(newEdgePlacement);
            }
         }
      else
         {
         sunkPastNonLivePath = true;
         if (trace())
            traceMsg(comp(), "              symbol not live in this successor\n");

         // Though symbol is not live on this path, it could be used in previous sink store recorded on this edge.
         // need check if symbol is used in this edge.
         if (isSymUsedInEdgePlacement(sourceBlock, succBlock))
            {
            placeDefInBlock = true;
            if (trace())
               traceMsg(comp(), "              symbol is used in early edge store placement\n");
            break;
            }
         }
      }

   // since only the successors of the sourceBlock is considered, so placeDefInBlock really means don't sink
   if (placeDefInBlock)
      return false;

   // this list will be used to update liveness info if we decide to sink the store
   List<TR::Block> blocksVisitedWhileSinking(trMemory());
   blocksVisitedWhileSinking.add(sourceBlock);

   // try to sink the store lower than all blocks where store is LONAP
   while (!worklist.isEmpty())
      {
      TR_BlockListEntry *blockEntry = worklist.popHead();
      TR::Block *block = blockEntry->_block;

      TR_ASSERT(block->getVisitCount() < visitCount, "A visited block should never be found on the worklist");

      int32_t blockNumber = block->getNumber();
      if (trace())
         traceMsg(comp(), "            TT[" POINTER_PRINTF_FORMAT "] trying to sink into block_%d\n", tt->getNode(), blockNumber);

      TR::Block *placementBlock = block;
      block->setVisitCount(visitCount);

      bool isCurrentBlockInEBB = (block->startOfExtendedBlock() == sourceBlock->startOfExtendedBlock());
      _usedSymbolsToMove = isCurrentBlockInEBB ? usedSymbols : allUsedSymbols;
      if (isCurrentBlockInEBB)
         canSinkOutOfEBB = canSinkOutOfEBB && (!commonedSymbols || !commonedSymbols->intersects(*_symbolsKilledInBlock[blockNumber]));

      if (!checkLiveMergingPaths(blockEntry, symIdx))
         {
         // def could not be propagated along some incoming LONAP edge, so propagating def to this block will cause def
         //    to occur more than once along some incoming edges (which may be unsafe)
         // therefore, need to place def along all pred edges from which we propagated
         // NOTE: def should go along edge (not at end of pred) because each pred may have other successors to which def was
         //       safely propagated...so if we put it at end of pred, then def may occur more than once along those edges
         //       (which may be unsafe)
         if (trace())
            traceMsg(comp(), "              didn't reach from all live predecessors, splitting edges it did reach on\n");
         ListIterator<TR::CFGEdge> predItLONAP(&blockEntry->_preds);
         for (TR::CFGEdge *predEdgeLONAP = predItLONAP.getFirst(); predEdgeLONAP != NULL; predEdgeLONAP = predItLONAP.getNext())
            {
            TR::CFGNode *predBlock = predEdgeLONAP->getFrom();
            bool areAllBlocksInEBB = isCurrentBlockInEBB &&
                                     predBlock->asBlock()->startOfExtendedBlock() == sourceBlock->startOfExtendedBlock();
            _usedSymbolsToMove = areAllBlocksInEBB ? usedSymbols : allUsedSymbols;
            bool copyStore= !areAllBlocksInEBB;

            //recordPlacementForDefAlongEdge(tt, predEdgeLONAP, true); // block can't be an extension of source block so copy
            TR_StoreInformation *storeInfo = new (trStackMemory()) TR_StoreInformation(tt, copyStore);
            // init _storeTemp field
            if (!needTempForCommonedLoads)
               storeInfo->_storeTemp = tt;
            else
               {
               storeInfo->_storeTemp = tt->duplicateTree();
               replaceLoadsWithTempSym(storeInfo->_storeTemp->getNode(), tt->getNode(), needTempForCommonedLoads);
               }
            TR_BitVector *symsReferenced = new (trStackMemory()) TR_BitVector(*_killedSymbolsToMove);
            (*symsReferenced) |= (*_usedSymbolsToMove);
            TR_EdgeInformation *edgeInfo = new (trStackMemory()) TR_EdgeInformation(predEdgeLONAP, symsReferenced);
            TR_EdgeStorePlacement *newEdgePlacement = new (trStackMemory()) TR_EdgeStorePlacement(storeInfo, edgeInfo, trMemory());
            edgePlacementsForThisStore.add(newEdgePlacement);
            }
         }
      else
         {
         // if we reach here, we know it's safe to propagate def to beginning of this block
         if (trace())
            {
            traceMsg(comp(), "              store sunk to beginning of block\n");
            traceMsg(comp(), "              liveOnNotAllPaths.in[sym=%d] == %s\n", symIdx, _liveOnNotAllPaths->_inSetInfo[blockNumber]->get(symIdx) ? "true" : "false");
            //traceMsg(comp(), "              symbolsKilled == NULL? %s\n", (_symbolsKilledInBlock[blockNumber] == NULL) ? "true" : "false");
            if (_symbolsKilledInBlock[blockNumber])
               {
               // remove 0 for more detail tracing
               if (0)
                  {
                  traceMsg(comp(), "            symbolsKilled[%d] now is ", blockNumber);
                  _symbolsKilledInBlock[blockNumber]->print(comp());
                  traceMsg(comp(), "\n");
                  traceMsg(comp(), "            used is ");
                  _usedSymbolsToMove->print(comp());
                  traceMsg(comp(), "\n");
               }
               traceMsg(comp(), "              intersection of used with symbolsKilled[%d] is %s\n", blockNumber, _symbolsKilledInBlock[blockNumber]->intersects(*_usedSymbolsToMove) ? "true" : "false");
               traceMsg(comp(), "              symbol %d in symbolsKilled is %s\n", symIdx, _symbolsKilledInBlock[blockNumber]->get(symIdx) ? "true" : "false");
               }
            traceMsg(comp(), "              symbolUseds == NULL? %s\n", (_symbolsUsedInBlock[blockNumber] == NULL) ? "true" : "false");
            if (_symbolsUsedInBlock[blockNumber])
               {
               // remove 0 for more detail tracing
               if (0)
                  {
                  traceMsg(comp(), "            symbolsUsed[%d] now is ", blockNumber);
                  _symbolsUsedInBlock[blockNumber]->print(comp());
                  traceMsg(comp(), "\n");
                  traceMsg(comp(), "            kill is ");
                  _killedSymbolsToMove->print(comp());
                  traceMsg(comp(), "\n");
                  }
               traceMsg(comp(), "              intersection of killed with symbolsUsed[%d] is %s\n", blockNumber, _symbolsUsedInBlock[blockNumber]->intersects(*_killedSymbolsToMove) ? "true" : "false");
               traceMsg(comp(), "              symbol %d in symbolsUsed is %s\n", symIdx, _symbolsUsedInBlock[blockNumber]->get(symIdx) ? "true" : "false");
               }
            }

         placeDefInBlock = true;

         // and if we've reached this block, then any symbols used by the store will be live at the beginning of this block
         // the liveness of the symbols used by the store can be determined from the liveness of the symbol(s) killed by the store
         if (_liveOnSomePaths->_blockAnalysisInfo[blockNumber]->intersects(*_killedSymbolsToMove))
            (*_liveOnSomePaths->_blockAnalysisInfo[blockNumber]) |= (*_usedSymbolsToMove);
         if (_liveOnAllPaths->_blockAnalysisInfo[blockNumber]->intersects(*_killedSymbolsToMove))
            (*_liveOnAllPaths->_blockAnalysisInfo[blockNumber]) |= (*_usedSymbolsToMove);
         if (_liveOnNotAllPaths->_inSetInfo[blockNumber]->intersects(*_killedSymbolsToMove))
            (*_liveOnNotAllPaths->_inSetInfo[blockNumber]) |= (*_usedSymbolsToMove);
         if (trace())
            {
            traceMsg(comp(), "              LONAP->blockInfo[%d]: ", blockNumber);
            _liveOnNotAllPaths->_inSetInfo[blockNumber]->print(comp());
            traceMsg(comp(), "\n");
            }

         // NOTE: we check that none of the symbols used or killed by the code to move are used or killed in the block
         //       this has an important sublety: it means that, if we allow the store to move through this block, then
         //        it is guaranteed that the store can move to all exception successors regardless of where in the block
         //        the exception flow occurs.  Otherwise, a store might be live at an exception successor from some exception
         //        points within the block but not others, which would force us to split the block to safely push the store to
         //       splitting the block sounds unpleasant, so just prevent it from happening
         if (_liveOnNotAllPaths->_inSetInfo[blockNumber]->get(symIdx) &&
             storeCanMoveThroughBlock(_symbolsKilledInBlock[blockNumber], _symbolsUsedInBlock[blockNumber], symIdx, allBlockUsedSymbols, allBlockKilledSymbols) &&
             (isCurrentBlockInEBB || (!allBlockKilledSymbols->intersects(*_usedSymbolsToMove) &&
                                      !allBlockKilledSymbols->get(symIdx) &&
                                      !allBlockUsedSymbols->intersects(*_killedSymbolsToMove) &&
                                      !allBlockUsedSymbols->get(symIdx))))
            {
            // 1) the store is not live on all paths at the beginning of block
            // 2) it's safe to push this store to the end of block and the store is live on som successor paths (but not all)
            // so: push the store down to those successors where it is live
            // if we can't push it all the way to the beginning of the successor block, then place def along the edge
            if (trace())
               traceMsg(comp(), "              store sunk to end of block\n");

            blocksVisitedWhileSinking.add(block);

            // we've pushed the uses down to the end of the block
            (*_liveOnNotAllPaths->_outSetInfo[blockNumber]) |= (*_usedSymbolsToMove);

            int32_t numOfEdgePlacements = 0;
            ListElement<TR_BlockListEntry> *lastAddedEntry = 0;
            placeDefInBlock = false;
            TR_SuccessorIterator outEdges(block);
            for (auto succEdge = outEdges.getFirst(); succEdge; succEdge = outEdges.getNext())
               {
               TR::CFGNode *succBlock = succEdge->getTo();
               int32_t succBlockNumber = succBlock->getNumber();
               bool isSuccBlockInEBB = isCurrentBlockInEBB &&
                                       (succBlock->asBlock()->startOfExtendedBlock() == sourceBlock->startOfExtendedBlock());
               _usedSymbolsToMove = isSuccBlockInEBB ? usedSymbols : allUsedSymbols;

               if (trace())
                  traceMsg(comp(), "            trying to push to block_%d\n", succBlockNumber);

               if (_liveOnSomePaths->_blockAnalysisInfo[succBlockNumber]->get(symIdx)
                   || !isSafeToSinkThruEdgePlacement(symIdx, block, succBlock, allEdgeInfoUsedOrKilledSymbols)   // FIXME:: without this sanity will fail?
                  )
                  {
                  if (!canSinkOutOfEBB && !isSuccBlockInEBB)
                     {
                     if (trace())
                        traceMsg(comp(), "              attempt to sink out of Ebb but commoned syms are killed in the Ebb => don't sink\n");
                     placeDefInBlock = true;
                     break;
                     }
                  if (!_sinkThruException && succBlock->asBlock()->isCatchBlock()
                      //&& !checkLiveMergingPaths(succBlock->asBlock(), symIdx)   invalid check
                     )
                     {
                     if (trace())
                        traceMsg(comp(), "              attempt to sink at the catch block => don't sink\n");
                     placeDefInBlock = true;
                     break;
                     }
                  if (shouldSinkStoreAlongEdge(symIdx, block, succBlock, sourceBlockFrequency, movableStore->_isLoadStatic, visitCount, allEdgeInfoUsedOrKilledSymbols))
                     {
                     if (trace())
                        traceMsg(comp(), "              added %d to worklist\n", succBlock->getNumber());
                     lastAddedEntry = worklist.addInTraversalOrder(succBlock->asBlock(), true, succEdge);
                     }
                  /* at this point, the store should be placed somewhere ... */
                  //else if (block->getStructureOf()->isLoopInvariantBlock())
                  //   {
                  //   // rather than creating the edge store placement from the pre-header, just place it in the pre-header
                  //   if (trace())
                  //      traceMsg(comp(), "              attempt to sink out of Ebb but commoned syms are killed in the Ebb => don't sink\n");
                  //   placeDefInBlock = true;
                  //
                  //   // clean up all the mess that we have made: remove the edge placements and worklist blocks
                  //   // previously added for successors
                  //   TR_ASSERT(numOfEdgePlacements==0, "pre-header has 2 outgoing edges??");
                  //   break;
                  //   }
                  else
                     {
                     bool copyStore=!isSuccBlockInEBB;
                     //recordPlacementForDefAlongEdge(tt, succEdge, true);   //copyStore);
                     TR_StoreInformation *storeInfo = new (trStackMemory()) TR_StoreInformation(tt, copyStore);
                     // init _storeTemp field
                     if (!needTempForCommonedLoads)
                        storeInfo->_storeTemp = tt;
                     else
                        {
                        storeInfo->_storeTemp = tt->duplicateTree();
                        replaceLoadsWithTempSym(storeInfo->_storeTemp->getNode(), tt->getNode(), needTempForCommonedLoads);
                        }
                     TR_BitVector *symsReferenced = new (trStackMemory()) TR_BitVector(*_killedSymbolsToMove);
                     (*symsReferenced) |= (*_usedSymbolsToMove);
                     TR_EdgeInformation *edgeInfo = new (trStackMemory()) TR_EdgeInformation(succEdge, symsReferenced);
                     TR_EdgeStorePlacement *newEdgePlacement = new (trStackMemory()) TR_EdgeStorePlacement(storeInfo, edgeInfo, trMemory());
                     edgePlacementsForThisStore.add(newEdgePlacement);
                     numOfEdgePlacements++;
                     }
                  }
               else
                  {
                  sunkPastNonLivePath = true;
                  if (trace())
                     traceMsg(comp(), "              symbol not live in this successor\n");
                  }
               }

            // clean up all the mess that we have made: remove the edge placements and worklist blocks
            // previously added for successors
            if (placeDefInBlock)
               {
               if (numOfEdgePlacements)
                  {
                  if (trace())
                     traceMsg(comp(), "              all previously added edge store placements from this block will be removed\n");
                  while (numOfEdgePlacements--)
                     edgePlacementsForThisStore.popHead();
                  }
               if (lastAddedEntry)
                  {
                  //worklist.remove(lastAddedEntry->getData());
                  for (auto tempSuccEdge = outEdges.getFirst(); tempSuccEdge; tempSuccEdge = outEdges.getNext())
                     worklist.removeBlockFromList(tempSuccEdge->getTo()->asBlock(), tempSuccEdge);
                  }
               }
            }

         if (placeDefInBlock)
            {
            // otherwise, just place def at the beginning of this block because
            // 1) store can't move thru this block, or
            // 2) store can move thru this block but the store has commoned load which is killed in this block and
            //    we attempt to sink along a succ path which is out of this Ebb
            if (trace())
               traceMsg(comp(), "              store can't go through block\n");

            // be careful if sourceBlock and block are in the same extended block: there may be commoned references
            //   in the store's tree that will get moved down if we make a copy of the store and remove the original
            //   tree
            bool copyStore=!isCurrentBlockInEBB;
            //recordPlacementForDefInBlock(tt, block, copyStore);
            TR_StoreInformation *storeInfo = new (trStackMemory()) TR_StoreInformation(tt, copyStore);
            // init _storeTemp field
            if (!needTempForCommonedLoads)
                storeInfo->_storeTemp = tt;
            else
               { // FIXME:: not a good idea if reach here?
               storeInfo->_storeTemp = tt->duplicateTree();
               replaceLoadsWithTempSym(storeInfo->_storeTemp->getNode(), tt->getNode(), needTempForCommonedLoads);
               }
            TR_BlockStorePlacement *newBlockPlacement = new (trStackMemory()) TR_BlockStorePlacement(storeInfo, block, trMemory());
            blockPlacementsForThisStore.add(newBlockPlacement);
            }
         }
      }

   if (sunkPastNonLivePath || _usedSymbolsToMove->isEmpty() || _sinkAllStores)
      {
      // looks like a good idea to sink this store, so:
      //  1) transfer our list of placements to the appropriate lists
      //  2) update gen and kill sets for sourceBlock and placement blocks
      //  2) update liveness information for this store

      // transfer our edge placements to _placementsOnEdgesToBlock and _allEdgePlacements
      ListIterator<TR_EdgeStorePlacement> edgeIt(&edgePlacementsForThisStore);
      for (TR_EdgeStorePlacement *edgePlacement=edgeIt.getFirst();
           edgePlacement != NULL;
           edgePlacement = edgeIt.getNext())
         {
         recordPlacementForDefAlongEdge(edgePlacement);
         }

      // transfer our block placements to _placementsInBlock and _allBlockPlacements
      // also, OR _killedSymbolsToMove into KILL and _usedSymbolsToMove into GEN for block
      // finally, add _usedSymbolsToMove into {LOSP,LOAP}_IN for block and remove _killedSymbolsToMove from {LOSP,LOAP,LONAP}_IN
      // (all happens in recordPlacementForDefInBlock)
      ListIterator<TR_BlockStorePlacement> blockIt(&blockPlacementsForThisStore);
      for (TR_BlockStorePlacement *blockPlacement=blockIt.getFirst();
           blockPlacement != NULL;
           blockPlacement = blockIt.getNext())
         {
         recordPlacementForDefInBlock(blockPlacement);
         }

      // now update liveness...for each block on blocksVisitedWhileSinking:
      //    1) {LOSP,LOAP}_{IN},LONAP_{IN,OUT} -= _killedSymbolsToMove
      //    2) {LOSP,LOAP}_{IN},LONAP_{IN,OUT} |= _usedSymbolsToMove
      // because we moved the store across all these blocks, so liveness of lhs went away and liveness of rhs entered
      // NOTE: LONAP,LOAP, and LOSP will no longer be consistent after this, but all we need to ensure is that
      //       no other store to any of those symbols will be sunk into any of these blocks
      //       It's true that we could miss an opportunity to move a store that should be LONAP because we moved *this* store,
      //       but let's leave that case on the table until there's a need to catch it
      while (!blocksVisitedWhileSinking.isEmpty())
         {
         TR::Block *visitedBlock = blocksVisitedWhileSinking.popHead();
         if (!visitedBlock->getExceptionSuccessors().empty())
            continue;
         int32_t visitedBlockNumber = visitedBlock->getNumber();
         bool isVisitedBlockInEBB = (visitedBlock->startOfExtendedBlock() == sourceBlock->startOfExtendedBlock());
         _usedSymbolsToMove = isVisitedBlockInEBB ? usedSymbols : allUsedSymbols;

         (*_liveOnSomePaths->_blockAnalysisInfo[visitedBlockNumber]) -= (*_killedSymbolsToMove);
         (*_liveOnSomePaths->_blockAnalysisInfo[visitedBlockNumber]) |= (*_usedSymbolsToMove);

         (*_liveOnAllPaths->_blockAnalysisInfo[visitedBlockNumber]) -= (*_killedSymbolsToMove);
         (*_liveOnAllPaths->_blockAnalysisInfo[visitedBlockNumber]) |= (*_usedSymbolsToMove);

         (*_liveOnNotAllPaths->_inSetInfo[visitedBlockNumber]) -= (*_killedSymbolsToMove);
         (*_liveOnNotAllPaths->_inSetInfo[visitedBlockNumber]) |= (*_usedSymbolsToMove);

         (*_liveOnNotAllPaths->_outSetInfo[visitedBlockNumber]) -= (*_killedSymbolsToMove);
         (*_liveOnNotAllPaths->_outSetInfo[visitedBlockNumber]) |= (*_usedSymbolsToMove);

         // remove 0 for more detail tracing
         if (0 && trace())
            {
            traceMsg(comp(), "            Update for the movable store: LOSP->blockInfo[%d]", visitedBlockNumber);
            _liveOnSomePaths->_blockAnalysisInfo[visitedBlockNumber]->print(comp());
            traceMsg(comp(), "\n");
            traceMsg(comp(), "            Update for the movable store: LOAP->blockInfo[%d]", visitedBlockNumber);
            _liveOnAllPaths->_blockAnalysisInfo[visitedBlockNumber]->print(comp());
            traceMsg(comp(), "\n");
            }
         }

      // also need to do the same thing for LONAP_OUT for sourceBlock (we only moved store OUT of sourceBlock,
      // not THROUGH it, so we shouldn't change {LOSP,LOAP,LONAP}_IN
      if (sourceBlock->hasExceptionSuccessors() == false)
         {
         _usedSymbolsToMove = usedSymbols;
         (*_liveOnNotAllPaths->_outSetInfo[sourceBlockNumber]) -= (*_killedSymbolsToMove);
         (*_liveOnNotAllPaths->_outSetInfo[sourceBlockNumber]) |= (*_usedSymbolsToMove);
         }

      return true;
      }
   else
      {
      // discard our placements
      }

   return false;
   }
