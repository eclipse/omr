/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <stddef.h>                                 // for NULL
#include <stdint.h>                                 // for int32_t, etc
#include "compile/Compilation.hpp"                  // for Compilation, etc
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/allocator.h"                          // for shared_allocator
#include "cs2/sparsrbit.h"
#include "env/TRMemory.hpp"                         // for SparseBitVector, etc
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                             // for Block
#include "il/DataTypes.hpp"                         // for TR::DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                             // for ILOpCode
#include "il/Node.hpp"                              // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                            // for Symbol
#include "il/SymbolReference.hpp"                   // for SymbolReference
#include "il/TreeTop.hpp"                           // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"            // for AutomaticSymbol
#include "il/symbol/ParameterSymbol.hpp"            // for ParameterSymbol
#include "il/symbol/RegisterMappedSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                         // for TR_ASSERT
#include "infra/BitVector.hpp"                      // for TR_BitVector
#include "infra/Cfg.hpp"                            // for CFG
#include "infra/List.hpp"                           // for ListIterator, etc
#include "infra/CfgNode.hpp"                        // for CFGNode
#include "optimizer/DataFlowAnalysis.hpp"
#include "ras/Debug.hpp"                            // for TR_DebugBase

class TR_Structure;
namespace TR { class Optimizer; }

TR_LiveVariableInformation::TR_LiveVariableInformation(TR::Compilation   *c,
			                               TR::Optimizer *optimizer,
			                               TR_Structure     *rootStructure,
			                               bool              splitLongs,
			                               bool              includeParms,
                                                       bool              includeMethodMetaDataSymbols,
                                                       bool              ignoreOSRUses)
   : _compilation(c), _trMemory(c->trMemory()), _ignoreOSRUses(ignoreOSRUses)
   {
   _traceLiveVariableInfo = comp()->getOption(TR_TraceLiveness);

   if (traceLiveVarInfo())
      traceMsg(comp(), "Collecting live variable information\n");

   // Find the number of locals and assign an index to each
   //
   _numLocals = 0;
   _includeParms = includeParms;
   _splitLongs = splitLongs;
   _includeMethodMetaDataSymbols = includeMethodMetaDataSymbols;

   if (includeParms)
      {
      TR::ParameterSymbol *p;
      ListIterator<TR::ParameterSymbol> parms(&comp()->getMethodSymbol()->getParameterList());
      for (p = parms.getFirst(); p != NULL; p = parms.getNext())
         {
         if (traceLiveVarInfo())
            traceMsg(comp(), "#%2d : is a parm symbol at %p\n", _numLocals, p);

         if (p->getType().isInt64() && splitLongs)
            {
            p->setLiveLocalIndex(_numLocals, comp()->fe());
            _numLocals += 2;
            }
         else
            p->setLiveLocalIndex(_numLocals++, comp()->fe());
         }
      }

   TR::AutomaticSymbol *p;
   ListIterator<TR::AutomaticSymbol> locals(&comp()->getMethodSymbol()->getAutomaticList());
   for (p = locals.getFirst(); p != NULL; p = locals.getNext())
      {
      if (traceLiveVarInfo())
         traceMsg(comp(), "Local #%2d is symbol at %p\n",_numLocals,p);

      if (p->getType().isInt64() && splitLongs)
         {
         p->setLiveLocalIndex(_numLocals, comp()->fe());
         _numLocals += 2;
         }
      else
         p->setLiveLocalIndex(_numLocals++, comp()->fe());
      }

   if (includeMethodMetaDataSymbols)
      {
      ListIterator<TR::RegisterMappedSymbol> methodMetaDataSymbols(&comp()->getMethodSymbol()->getMethodMetaDataList());
      for (auto m = methodMetaDataSymbols.getFirst(); m != NULL; m = methodMetaDataSymbols.getNext())
         {
         TR_ASSERT(m->isMethodMetaData(), "Should be method metadata");
         if (traceLiveVarInfo())
            traceMsg(comp(), "Local #%2d is MethodMetaData symbol at %p : %s\n",_numLocals,m,m->getName());

         if (m->getType().isInt64() && splitLongs)
            {
            m->setLiveLocalIndex(_numLocals, comp()->fe());
            _numLocals += 2;
            }
         else
            m->setLiveLocalIndex(_numLocals++, comp()->fe());
         }
      }


    if (traceLiveVarInfo())
      traceMsg(comp(), "Finished collecting live variable information: %d locals found\n", _numLocals);

   _localObjects = NULL;
   _cachedRegularGenSetInfo = NULL;
   _cachedRegularKillSetInfo = NULL;
   _cachedExceptionGenSetInfo = NULL;
   _cachedExceptionKillSetInfo = NULL;

   _haveCachedGenAndKillSets = false;
   _liveCommonedLoads = NULL;
   }

void TR_LiveVariableInformation::createGenAndKillSetCaches()
   {
   _numNodes = comp()->getFlowGraph()->getNextNodeNumber();
   int32_t arraySize = _numNodes * sizeof (TR_BitVector *);
   _cachedRegularGenSetInfo = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);
   _cachedRegularKillSetInfo = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);
   _cachedExceptionGenSetInfo = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);
   _cachedExceptionKillSetInfo = (TR_BitVector **) trMemory()->allocateStackMemory(arraySize);

   // need to allocate each block's sets here or else they may be allocated above an inner stack mark
   // and then be de-allocated before they get re-used
   for (int32_t b=0;b < _numNodes;b++)
      {
      _cachedRegularGenSetInfo[b] = new (trStackMemory()) TR_BitVector(_numLocals, trMemory());
      _cachedRegularKillSetInfo[b] = new (trStackMemory()) TR_BitVector(_numLocals, trMemory());
      _cachedExceptionGenSetInfo[b] = new (trStackMemory()) TR_BitVector(_numLocals, trMemory());
      _cachedExceptionKillSetInfo[b] = new (trStackMemory()) TR_BitVector(_numLocals, trMemory());
      }

   _haveCachedGenAndKillSets = false;
   }

void TR_LiveVariableInformation::initializeGenAndKillSetInfo(TR_BitVector **regularGenSetInfo,
                                                             TR_BitVector **regularKillSetInfo,
                                                             TR_BitVector **exceptionGenSetInfo,
                                                             TR_BitVector **exceptionKillSetInfo)
   {
   if (_haveCachedGenAndKillSets)
      {
      TR_ASSERT(comp()->getFlowGraph()->getNextNodeNumber() == _numNodes, "flow graph has changed since gen and kill sets were initialized");
      // copy our cached sets into the arrays being passed to us
      for (int32_t nodeNumber=0; nodeNumber < _numNodes; nodeNumber++)
         {
         if (_cachedRegularGenSetInfo[nodeNumber] != NULL)
            {
            regularGenSetInfo[nodeNumber] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
            *(regularGenSetInfo[nodeNumber]) = *(_cachedRegularGenSetInfo[nodeNumber]);
            }

         if (_cachedRegularKillSetInfo[nodeNumber] != NULL)
            {
            regularKillSetInfo[nodeNumber] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
            *(regularKillSetInfo[nodeNumber]) = *(_cachedRegularKillSetInfo[nodeNumber]);
            }

         if (_cachedExceptionGenSetInfo[nodeNumber] != NULL)
            {
            exceptionGenSetInfo[nodeNumber] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
            *(exceptionGenSetInfo[nodeNumber]) = *(_cachedExceptionGenSetInfo[nodeNumber]);
            }

         if (_cachedExceptionKillSetInfo[nodeNumber] != NULL)
            {
            exceptionKillSetInfo[nodeNumber] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
            *(exceptionKillSetInfo[nodeNumber]) = *(_cachedExceptionKillSetInfo[nodeNumber]);
            }
         }
      return;
      }

   // allocate bit vector to store local objects as they are found by findUseOfLocal
   _localObjects = new (trStackMemory()) TR_BitVector(_numLocals, trMemory());

   // For each block in the CFG build the gen and kill set for this analysis.
   // Go in treetop order, which guarantees that we see the correct (i.e. first)
   // evaluation point for each node.
   //
   vcount_t   visitCount = comp()->incVisitCount();
   TR::Block *block;
   int32_t   blockNum = 0;
   bool      seenException = false;

   bool ignoreBlock = false;
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::BBEnd && ignoreBlock)
         {
         ignoreBlock = false;
         continue;
         }
      if (ignoreBlock)
         continue;

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         blockNum = block->getNumber();
         seenException  = false;
         if (_ignoreOSRUses && (block->isOSRCodeBlock() || block->isOSRCatchBlock()))
            {
            ignoreBlock = true;
            continue;
            }
         if (traceLiveVarInfo())
            traceMsg(comp(), "\nNow generating liveness information for block_%d\n", blockNum);
         }



      // Find uses of locals in this tree.
      //
      findUseOfLocal(node, blockNum, regularGenSetInfo, regularKillSetInfo, NULL, true, visitCount);
      if (_cachedRegularGenSetInfo && regularGenSetInfo[blockNum] != NULL)
         {
         *(_cachedRegularGenSetInfo[blockNum]) = *(regularGenSetInfo[blockNum]);
         if (!seenException)
            {
            if (exceptionGenSetInfo[blockNum] == NULL)
               exceptionGenSetInfo[blockNum] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
            *(exceptionGenSetInfo[blockNum]) = *(regularGenSetInfo[blockNum]);
            if (_cachedExceptionGenSetInfo != NULL)
               *(_cachedExceptionGenSetInfo[blockNum]) = *(regularGenSetInfo[blockNum]);
            }
         }

      if (node->getOpCodeValue() == TR::treetop)
         node = node->getFirstChild();

      if (node->getOpCode().isStoreDirect())
         {
         TR::RegisterMappedSymbol *local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
         if (!local && _includeParms)
            local = node->getSymbolReference()->getSymbol()->getParmSymbol();
         if (!local && _includeMethodMetaDataSymbols)
            local = node->getSymbolReference()->getSymbol()->getMethodMetaDataSymbol();

         if (local)
            {
            uint16_t localIndex = local->getLiveLocalIndex();

            if (localIndex != INVALID_LIVENESS_INDEX)
               {
               if (traceLiveVarInfo())
               traceMsg(comp(), "\n Killing symbol with local index %d in block_%d\n", localIndex, blockNum);

               if (regularKillSetInfo[blockNum] == NULL)
                  regularKillSetInfo[blockNum] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
               regularKillSetInfo[blockNum]->set(localIndex);
               if (_cachedRegularKillSetInfo)
                  _cachedRegularKillSetInfo[blockNum]->set(localIndex);

               if (_splitLongs && local->getType().isInt64())
                  {
                  regularKillSetInfo[blockNum]->set(localIndex+1);
                  if (_cachedRegularKillSetInfo)
                  _cachedRegularKillSetInfo[blockNum]->set(localIndex+1);
                  }

               if (!seenException)
                  {
                  if (exceptionKillSetInfo[blockNum] == NULL)
                     exceptionKillSetInfo[blockNum] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
                  exceptionKillSetInfo[blockNum]->set(localIndex);
                  if (_cachedExceptionKillSetInfo)
                     _cachedExceptionKillSetInfo[blockNum]->set(localIndex);

                  if (_splitLongs && local->getType().isInt64())
                     {
                     exceptionKillSetInfo[blockNum]->set(localIndex+1);
                     if (_cachedExceptionKillSetInfo)
                        _cachedExceptionKillSetInfo[blockNum]->set(localIndex+1);
                     }
                  }
               }
            }
         }


      if (!seenException && node->exceptionsRaised() != 0)
         seenException = true;
      }

   // if any local objects were found, mark them as gen'd in all blocks
   if (!_localObjects->isEmpty())
      {
      for (TR::CFGNode *cfgNode = _compilation->getFlowGraph()->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
         {
         TR::Block *b = cfgNode->asBlock();
         if (b)
            {
            int32_t blockNum = b->getNumber();

            if (traceLiveVarInfo())
               traceMsg(comp(), "            Adding local objects to gen set for block_%d\n", blockNum);

            if (regularGenSetInfo[blockNum] == NULL)
               regularGenSetInfo[blockNum] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
            *regularGenSetInfo[blockNum] |= *_localObjects;
            if (_cachedRegularGenSetInfo)

               *_cachedRegularGenSetInfo[blockNum] |= *_localObjects;


            if (exceptionGenSetInfo[blockNum] == NULL)
               exceptionGenSetInfo[blockNum] = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);
            *exceptionGenSetInfo[blockNum] |= *_localObjects;
            if (_cachedExceptionGenSetInfo)
               *_cachedExceptionGenSetInfo[blockNum] |= *_localObjects;
            }
         }
      }
   _localObjects = NULL;

   if (_cachedRegularGenSetInfo)
      {
      // clear out empty sets in the cached versions
      for (int32_t b=0; b < _numNodes; b++)
         {
         if (regularGenSetInfo[b] == NULL)
            _cachedRegularGenSetInfo[b] = NULL;
         if (regularKillSetInfo[b] == NULL)
            _cachedRegularKillSetInfo[b] = NULL;
         if (exceptionGenSetInfo[b] == NULL)
            _cachedExceptionGenSetInfo[b] = NULL;
         if (exceptionKillSetInfo[b] == NULL)
            _cachedExceptionKillSetInfo[b] = NULL;
         }

      _haveCachedGenAndKillSets = true;
      }
   }

// call this method to force findUseOfLocal to track uses of live commoned loads
// i.e. if a tree contains a commoned reference to a load of a local, then
void TR_LiveVariableInformation::trackLiveCommonedLoads()
   {
   _liveCommonedLoads = new (trStackMemory()) TR_BitVector(_numLocals, trMemory());
   }

void TR_LiveVariableInformation::findCommonedLoads(TR::Node *node, TR_BitVector *commonedLoads,
                                                   bool movingForwardThroughTrees, vcount_t visitCount)
   {
   TR_ASSERT(commonedLoads, "asked to collect commoned loads, but no bit vector passed in");
   visitTreeForLocals(node, NULL, NULL, movingForwardThroughTrees, true, visitCount, commonedLoads, false);
   }

void TR_LiveVariableInformation::findAllUseOfLocal(TR::Node *node, int32_t blockNum,
                                                   TR_BitVector *genSetInfo, TR_BitVector *killSetInfo,
                                                   TR_BitVector *commonedLoads, bool movingForwardThroughTrees, vcount_t visitCount)
   {
   visitTreeForLocals(node, &genSetInfo, killSetInfo, movingForwardThroughTrees, true, visitCount, commonedLoads, false);
   }

void TR_LiveVariableInformation::findLocalUsesInBackwardsTreeWalk(TR::Node *node, int32_t blockNum,
                                                   TR_BitVector *genSetInfo, TR_BitVector *killSetInfo,
                                                   TR_BitVector *commonedLoads, vcount_t visitCount)
   {
   TR_ASSERT(commonedLoads != NULL, "can't do a backward tree walk properly unless recording commoned loads for tree");
   visitTreeForLocals(node, &genSetInfo, killSetInfo, false, true, visitCount, commonedLoads, false);
   }

void TR_LiveVariableInformation::findUseOfLocal(TR::Node *node, int32_t blockNum,
                                                TR_BitVector **genSetInfo, TR_BitVector **killSetInfo,
                                                TR_BitVector *commonedLoads, bool movingForwardThroughTrees, vcount_t visitCount)
   {
   visitTreeForLocals(node, genSetInfo + blockNum, killSetInfo[blockNum], movingForwardThroughTrees, false, visitCount, commonedLoads, false);
   }

// movingForwardThroughTrees describes how the client is walking treetops
// for walking trees forwards, use visit count to determine when nodes have been visited
// for walking trees backwards, use future use counts to determine the last reference and only look at those
//    last uses (because they're actually the first references in the block)
// if visitEntireTree is true, then the backward traversal will continue under commoned nodes but belowCommonedNode will be set
//    NOTE: forward traversals cannot rely on visitEntireTree
// belowCommonedNode is used to track when we're traversing a commoned part of a node's tree or not (on first call, pass in false)
// if commonedLoads is non-null, then any symbols loaded while belowCommonedNode is set are marked in that bit vector
// if visitCount is >= 0, then it will be used to avoid visiting nodes multiple times
//    NOTE: if a node has its visit count set to visitCount, it will not be visited unless visitEntireTree is set
//    NOTE: for backward traversals a new visit count is needed for each treetop in an EBB to visit correctly and efficiently
void TR_LiveVariableInformation::visitTreeForLocals(TR::Node *node, TR_BitVector **blockGenSetInfo, TR_BitVector *blockKillSetInfo,
                                                    bool movingForwardThroughTrees, bool visitEntireTree, vcount_t visitCount,
                                                    TR_BitVector *commonedLoads, bool belowCommonedNode)
   {
   if (movingForwardThroughTrees)
      {
      TR_ASSERT(!visitEntireTree, "can't walk trees forwards and visit entire tree");
      TR_ASSERT(visitCount >= 0, "can't walk trees forwards without a visitCount");
      }

   if (traceLiveVarInfo())
      {
      if (movingForwardThroughTrees)
         traceMsg(comp(), "         Looking forward for uses in node %p has visitCount = %d and comp() visitCount = %d\n",
		  node, node->getVisitCount(), visitCount);
      else
         traceMsg(comp(), "         Looking backward for uses in node %p has future use count = %d and reference count = %d\n",
		  node, node->getFutureUseCount(), node->getReferenceCount());
      }

   uint16_t localIndex = INVALID_LIVENESS_INDEX;
   TR::RegisterMappedSymbol *local=NULL;
   TR::SparseBitVector indirectMethodMetaUses(comp()->allocator());
   TR::SparseBitVector regStarAsmUses(comp()->allocator());

   //TR::SymbolReference *symRef = node->getSymbolReference();
   if (node->getOpCode().isLoadVarDirect() || node->getOpCodeValue() == TR::loadaddr)
      {
      local = node->getSymbolReference()->getSymbol()->getAutoSymbol();
      if (!local && _includeParms)
         local = node->getSymbolReference()->getSymbol()->getParmSymbol();
      if (!local && _includeMethodMetaDataSymbols)
         local = node->getSymbolReference()->getSymbol()->getMethodMetaDataSymbol();
      if (local)
         {
         localIndex = local->getLiveLocalIndex();
         if (localIndex == INVALID_LIVENESS_INDEX)
            local = NULL; // don't analyze this local if localIndex == INVALID_LIVENESS_INDEX
         }
      }
   else if (!belowCommonedNode &&
            _includeMethodMetaDataSymbols &&
            node->getOpCode().isLoadIndirect() &&
            node->getSymbolReference()
            // && symRef->getSymbol()->isMethodMetaData()
            )
      {
      if (traceLiveVarInfo())
         traceMsg(comp(),"           found indirect methodMetaData on node %p (belowCommonedNode = %d)\n",node,belowCommonedNode);

      node->mayKill(true).getAliases(indirectMethodMetaUses);
      if (!indirectMethodMetaUses.IsZero() && traceLiveVarInfo())
         {
         traceMsg(comp(),"           indirectMethodMetaUses:\n");
         (*comp()) << indirectMethodMetaUses << "\n";
         }
      }
   // if local != NULL at this point, then we have a load of a local with index in localIndex

   // figure out if we should be visiting this node
   if (movingForwardThroughTrees)
      {
      // if we're below a commoned node, then we must be visiting the entire tree so just keep going
      if (!belowCommonedNode)
         {
         // maybe we should end traversal here?
         if (node->getVisitCount() == visitCount)
            {
            if (visitEntireTree)
               belowCommonedNode = true;
            else
               // this node has already been visited and we're not supposed to visit the entire tree
               return;
            }
         else
            node->setVisitCount(visitCount);
         }
      }
   else
      {
      // if we're below a commoned node, then we must be visiting the entire tree so just keep going
      if (!belowCommonedNode)
         {
         if (node->getFutureUseCount() > 0)
            node->decFutureUseCount();
         if (node->getFutureUseCount() > 0)
            {
            if (traceLiveVarInfo())
               traceMsg(comp(), "            not first reference\n");

            // traversal should either end here (for regular traversal) or track that we're below a commoned node
            if (visitEntireTree)
               belowCommonedNode = true;
            else
               return;
            }
         else
            {
            if (traceLiveVarInfo())
               traceMsg(comp(), "            first reference\n");

            // if this is a load, clear it in liveCommonedLoads if we've been asked to track them
            if (_liveCommonedLoads != NULL && local != NULL)
               _liveCommonedLoads->reset(localIndex);
            }
         }

      // visit count used to detect commoned nodes WITHIN each tree: visit count must be different for each tree in an EBB
      // for this to work properly ...IF the visit count isn't different for each tree, this DOES NOT WORK PROPERLY
      if (node->getVisitCount() == visitCount && node->getFutureUseCount() > 0)
         return;
      node->setVisitCount(visitCount);
      }

   // if we get here, then we should visit this node

   // go look for symbols in node's children
   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      visitTreeForLocals(node->getChild(i), blockGenSetInfo, blockKillSetInfo, movingForwardThroughTrees, visitEntireTree, visitCount, commonedLoads, belowCommonedNode);

   // if we reference a local symbol, then record it
   if (local || !indirectMethodMetaUses.IsZero() || !regStarAsmUses.IsZero())
      {
      if (traceLiveVarInfo() && local)
         traceMsg(comp(), "            Node [%p] local [%p] idx %d\n", node, local, localIndex);

      if (belowCommonedNode)
         {
         if (commonedLoads != NULL && local)
            {
            if (traceLiveVarInfo())
               traceMsg(comp(), "              Marking as commoned load\n");
            commonedLoads->set(localIndex);
            }
         else
            {
            if (traceLiveVarInfo())
               traceMsg(comp(), "              Commoned load, but not asked to remember it\n");
            }
         if (_liveCommonedLoads != NULL)
            {
            if (traceLiveVarInfo())
               traceMsg(comp(), "              Marking %d as live commoned load\n", localIndex);
            _liveCommonedLoads->set(localIndex);
            }
         }
      if (_localObjects != NULL && local && local->isLocalObject())
         {
         // First use of a local object.
         // This local object will be live throughout the method,
         // so mark it as generated in the exit block.
         //
         TR_ASSERT(node->getOpCodeValue() == TR::loadaddr, "assertion failure");
         _localObjects->set(localIndex);
         if (traceLiveVarInfo())
            traceMsg(comp(), "            Marking local object\n");
         }
      else if (!belowCommonedNode && blockGenSetInfo)

         {
         // This local is in the gen set if we haven't already seen a kill
         // for it
         //
         if (!movingForwardThroughTrees  ||
             blockKillSetInfo == NULL ||
             (local && !blockKillSetInfo->get(localIndex)) ||
             !regStarAsmUses.IsZero() ||
             !indirectMethodMetaUses.IsZero()
             )
             //!blockKillSetInfo->get(localIndex))
            {
            if (traceLiveVarInfo() && local)
               traceMsg(comp(), "            Gening symbol with local index %d\n", localIndex);

            if (*blockGenSetInfo == NULL)
               *blockGenSetInfo = new (trStackMemory()) TR_BitVector(_numLocals,trMemory(), stackAlloc);

            if (local)
               (*blockGenSetInfo)->set(localIndex);

            if (!indirectMethodMetaUses.IsZero())
               {
               TR::SparseBitVector::Cursor aliasesCursor(indirectMethodMetaUses);
               for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
                  {
                  uint16_t usedSymbolIndex;
                  TR::SymbolReference *usedSymRef = comp()->getSymRefTab()->getSymRef(aliasesCursor);
                  TR::RegisterMappedSymbol *usedSymbol = usedSymRef->getSymbol()->getRegisterMappedSymbol();
                  if (usedSymbol && usedSymbol->getDataType() != TR::NoType)
                     {
                     usedSymbolIndex = usedSymbol->getLiveLocalIndex();
                     if (usedSymbolIndex != INVALID_LIVENESS_INDEX && usedSymbolIndex < _numLocals && (!blockKillSetInfo || !blockKillSetInfo->get(usedSymbolIndex)))
                        {
                        if (traceLiveVarInfo())
                           traceMsg(comp(),"       setting symIdx %d (from symRef %d) on usedSymbols (in liveness)\n",usedSymbolIndex,usedSymRef->getReferenceNumber());
                        (*blockGenSetInfo)->set(usedSymbolIndex);
                        }
                     }
                  }
               }
            if (!regStarAsmUses.IsZero())
               {
               TR::SparseBitVector::Cursor aliasesCursor(regStarAsmUses);
               for (aliasesCursor.SetToFirstOne(); aliasesCursor.Valid(); aliasesCursor.SetToNextOne())
                  {
                  int32_t nextElement = aliasesCursor;
                  if (comp()->getSymRefTab()->getSymRef(nextElement) == NULL)
                     continue;
                  TR::Symbol *usedSym = comp()->getSymRefTab()->getSymRef(nextElement)->getSymbol();
                  }
               }
            if (_splitLongs && local && local->getType().isInt64())
               {
               (*blockGenSetInfo)->set(localIndex+1);
               }
            }
         }
      }
   return;
   }
