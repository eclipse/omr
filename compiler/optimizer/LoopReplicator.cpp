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

#include "optimizer/LoopReplicator.hpp"

#include <stdio.h>                             // for fflush, printf, stdout
#include <string.h>                            // for NULL, memset
#include "codegen/FrontEnd.hpp"                // for feGetEnv
#include "compile/Compilation.hpp"             // for Compilation
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable
#include "env/TRMemory.hpp"                    // for TR_Link::operator new, TR_Memory, etc
#include "il/Block.hpp"                        // for Block, toBlock, TR_BlockCloner
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::Goto, etc
#include "il/ILOps.hpp"                        // for ILOpCode
#include "il/Node.hpp"                         // for Node
#include "il/Node_inlines.hpp"                 // for Node::getChild, etc
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, TreeTop::join, etc
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/BitVector.hpp"                 // for TR_BitVector
#include "infra/Cfg.hpp"                       // for CFG, MAX_COLD_BLOCK_COUNT
#include "infra/Stack.hpp"                     // for TR_Stack
#include "infra/CfgEdge.hpp"                   // for CFGEdge
#include "infra/CfgNode.hpp"                   // for CFGNode
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/Structure.hpp"             // for TR_RegionStructure, etc
#include "ras/Debug.hpp"                       // for TR_DebugBase

#define OPT_DETAILS "O^O LOOP REPLICATOR: "
#define MIN_BLOCKS_IN_TRACE 5

#define INITIAL_BLOCK_WEIGHT -999
#define COLOR_BLACK 0
#define COLOR_WHITE 1
#define COLOR_GRAY -1

TR_LoopReplicator::TR_LoopReplicator(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager),
     _blocksCloned(trMemory())
   {
   _nodeCount = 0;
   _nodesInCFG = 0;
   _switchTree = NULL;
   _maxNestingDepth = 0;
   }


//Add static debug counter for a given replication failure
static void countReplicationFailure(char *failureReason, int32_t regionNum)
   {
   //Assemble format string: "LoopReplicator/<failureReason>/%s/(%s)/region_%d"
   TR::DebugCounter::incStaticDebugCounter(TR::comp(), TR::DebugCounter::debugCounterName(TR::comp(),
      "LoopReplicator/%s/%s/(%s)/region_%d", failureReason,
      TR::comp()->getHotnessName(TR::comp()->getMethodHotness()),
      TR::comp()->signature(), regionNum));
   }

int32_t TR_LoopReplicator::perform()
   {
   // mainline entry

   static char *disableLR = feGetEnv("TR_NoLoopReplicate");
   if (disableLR)
      return 0;

   if (!comp()->mayHaveLoops() ||
       optimizer()->optsThatCanCreateLoopsDisabled())
      return 0;

   if (comp()->getProfilingMode() == JitProfiling)
      return 0;

   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure();
   _haveProfilingInfo = true;



   static char *testLR = feGetEnv("TR_LRTest");
   if (!_haveProfilingInfo)
      {
      dumpOptDetails(comp(), "Need profiling information in order to replicate...\n");
      if (trace())
         traceMsg(comp(), "method is %s \n", comp()->signature());
      if (!testLR)
         return 0;
      }

   _nodesInCFG = _cfg->getNextNodeNumber();
   TR_Structure *root = _rootStructure;

   // From this point on, stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _blocksInCFG = (TR::Block **) trMemory()->allocateStackMemory(_nodesInCFG * sizeof(TR::Block *));
   memset(_blocksInCFG, 0, _nodesInCFG * sizeof(TR::Block *));
   _blockWeights = (int32_t *) trMemory()->allocateStackMemory(_nodesInCFG * sizeof(int32_t));
   memset(_blockWeights, 0, _nodesInCFG * sizeof(int32_t));
   _seenBlocks = (int32_t *) trMemory()->allocateStackMemory(_nodesInCFG * sizeof(int32_t));
   memset(_seenBlocks, 0, _nodesInCFG * sizeof(int32_t));
   //
   _blocksVisited = new (trStackMemory()) TR_BitVector(_nodesInCFG, trMemory(), stackAlloc);
   for (TR::CFGNode *n = _cfg->getFirstNode(); n; n = n->getNext())
      {
      if (toBlock(n) && (n->getNumber() >= 0))
         _blocksInCFG[n->getNumber()] = toBlock(n);
      _blockWeights[n->getNumber()] = INITIAL_BLOCK_WEIGHT;
      _seenBlocks[n->getNumber()] = COLOR_WHITE;
      }

   // initialize the bitvectors
   _blocksVisited->empty();

   if (trace() && getDebug())
      {
      traceMsg(comp(), "structure before replication :\n");
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      }

   // collect info about potential
   // loops for replication
   perform(root);

   // now modify trees & CFG
   dumpOptDetails(comp(), "analysis complete...attempting to replicate\n");
   modifyLoops();

   return 0;
   }

int32_t TR_LoopReplicator::perform(TR_Structure *str)
   {
   TR_RegionStructure *region;

   if (!(region = str->asRegion()))
      return 0;

   // replicate in post-order
   TR_RegionStructure::Cursor sgnIt(*region);
   for (TR_StructureSubGraphNode *n = sgnIt.getCurrent(); n; n = sgnIt.getNext())
      perform(n->getStructure());

   if (!region->isNaturalLoop())
      {
      dumpOptDetails(comp(), "region (%d) is not a natural loop\n", region->getNumber());
      return 0;
      }

   // ignore loops known to be cold
   TR::Block *entryBlock = region->getEntryBlock();
   if (entryBlock->isCold())
      {
      countReplicationFailure("ColdLoop", region->getNumber());
      dumpOptDetails(comp(), "region (%d) is a cold loop\n", region->getNumber());
      return 0;
      }

   _blockMapper = (TR::Block **) trMemory()->allocateStackMemory(_nodesInCFG * sizeof(TR::Block *));
   memset(_blockMapper, 0, _nodesInCFG * sizeof(TR::Block *));

   // analyze 2 kinds of loops
   // (a) do-while loops
   //  contain a block that has 2 edges - one edge exits the region and another edge is the
   //  loop back-edge
   //  region
   // (b) while loops
   //  entry block (of the region) has an edge that exits the region. One of the predecessor
   //  edges of the entry is the loop back-edge
   //
   // there are 2 places where the controlling condition can be found
   //  (1) first treetop - (while loops)
   //  (2) originator of back-edge - (do-while loops)

   if (trace())
      traceMsg(comp(), "analyzing loop (%d)\n", region->getNumber());

   // case 1
   TR_StructureSubGraphNode *entryNode = region->getEntry();
   TR_BlockStructure *entry = entryNode->getStructure()->asBlock();
   if (entry)
      {
      for (auto edge = entryNode->getSuccessors().begin(); edge != entryNode->getSuccessors().end(); ++edge)
         {
         if (region->isExitEdge(*edge))
            {
            if (isWellFormedLoop(region, entry))
               {
               if (trace())
                  traceMsg(comp(), "found while loop\n");
               _loopType = whileDo;
               return replicateLoop(region, entryNode);
               }
            }
         }
      }

   // case 2
   TR_StructureSubGraphNode *branchNode = NULL;
   TR_RegionStructure::Cursor snIt(*region);
   for (TR_StructureSubGraphNode *sgNode = snIt.getCurrent();
        sgNode && (branchNode == NULL);
        sgNode = snIt.getNext())
      {
      bool hasBackEdge = false;
      bool hasExitEdge = false;
      for (auto edge = sgNode->getSuccessors().begin(); edge != sgNode->getSuccessors().end(); ++edge)
         {
         TR_StructureSubGraphNode *dest = toStructureSubGraphNode((*edge)->getTo());
         if (region->isExitEdge(*edge))
            hasExitEdge = true;
         if (dest == region->getEntry())
            hasBackEdge = true;
         if (hasBackEdge && hasExitEdge)
            {
            if (isWellFormedLoop(region, sgNode->getStructure()))
               branchNode = sgNode;
            }
         }
      }

   if (branchNode)
      {
      if (trace())
         traceMsg(comp(), "found do-while loop\n");
      _loopType = doWhile;
      return replicateLoop(region, branchNode);
      }

   countReplicationFailure("UnsupportedLoopStructure", region->getNumber());

   dumpOptDetails(comp(), "loop (%d) does not conform to required form & will not be replicated\n",
                    region->getNumber());
   return false;
   }

bool TR_LoopReplicator::isWellFormedLoop(TR_RegionStructure *region, TR_Structure *condBlock)
   {
   // FIXME:very simple checks for now
   //
   vcount_t visitCount = comp()->incVisitCount();
   // (1)
   // condition has to be in a block
   if (!condBlock->asBlock())
      return false;

   // (2)
   // iterate blocks and check for catch blocks; since they shouldnt be cloned
   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   region->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> bilIt(&blocksInLoop);
   TR::Block *b = NULL;
   int32_t numBlocks = 0;
   for (b = bilIt.getCurrent(); b; b = bilIt.getNext(), numBlocks++)
      {
      if (b->hasExceptionPredecessors()) // catch
         {
         if (trace())
            traceMsg(comp(), "block (%d) has exception predecessors - currently not supported\n", b->getNumber());
         return false;
         }
      if (b->hasExceptionSuccessors()) // try
         {
         if (trace())
            traceMsg(comp(), "block (%d) has exception successors\n", b->getNumber());
         }
      }

   // (3)
   // check if number of nodes in loop exceeds threshold
   // FIXME: decide on a threshold; disabled for now
   bilIt.reset();
   for (b = bilIt.getCurrent(); b; b = bilIt.getNext())
      {
      TR::TreeTop *entryTT = b->getEntry();
      TR::TreeTop *exitTT = b->getExit();
      for (TR::TreeTop *tt = entryTT->getNextRealTreeTop();
           tt != exitTT; tt = tt->getNextRealTreeTop())
         {
         _nodeCount += countChildren(tt->getNode(), visitCount);
         }
      }

   int32_t currentNestingDepth = 0;
   int32_t currentMaxNestingDepth = 0;
   _maxNestingDepth = region->getMaxNestingDepth(&currentNestingDepth, &currentMaxNestingDepth);

   if (trace())
      {
      // loopproperties
      traceMsg(comp(), "for loop (%d): \n", region->getNumber());
      traceMsg(comp(), "   number of nodes:   %d\n", _nodeCount);
      traceMsg(comp(), "   number of blocks:  %d\n", numBlocks);
      traceMsg(comp(), "   max nesting depth: %d\n", _maxNestingDepth);
      }

   if (_maxNestingDepth > MAX_REPLICATION_NESTING_DEPTH)
      {
      traceMsg(comp(), "for loop (%d), max nest depth thresholds exceeded\n", region->getNumber());
      return false;
      }

   //growth factor, 33% of the loop
   //
   if ((numBlocks*MAX_REPLICATION_GROWTH_FACTOR) > MAX_REPLICATION_GROWTH_SIZE)
      {
      traceMsg(comp(), "for loop (%d), loop too big, thresholds exceeded\n", region->getNumber());
      return false;
      }

#if 0
   if (_nodeCount > MAX_NODE_THRESHOLD_FOR_REPLICATION)
      {
      if (trace())
         traceMsg(comp(), "no replication, node count exceeds threshold for loop (%d)\n",
                 region->getNumber());
      return false;
      }
#endif

   return true;
   }

// restructure loop by replication
int32_t TR_LoopReplicator::replicateLoop(TR_RegionStructure *region,
                                         TR_StructureSubGraphNode *branchNode)
   {

   TR::Block *cBlock = branchNode->getStructure()->asBlock()->getBlock();
   TR::TreeTop *lastTT = cBlock->getLastRealTreeTop();
   if (!lastTT->getNode()->getOpCode().isBranch())
      {
      countReplicationFailure("NoBranchFoundInLoop", region->getNumber());
      if (trace())
         traceMsg(comp(), "no branch condition found in loop (%d)\n", region->getNumber());
      return false;
      }

   dumpOptDetails(comp(), "picking trace in loop (%d)...\n", region->getNumber());


   LoopInfo *lInfo = new (trStackMemory()) (LoopInfo);
   lInfo->_regionNumber = region->getNumber();
   lInfo->_replicated = false;
   lInfo->_region = region;
   _loopInfo.add(lInfo);
   _curLoopInfo = lInfo;

   // reset bitvector
   //_blocksVisited->empty();
   static char *pEnv = feGetEnv("TR_NewLRTracer");
   if (pEnv)
      {
      ///printf("computing weights for loop %d in method %s\n", region->getNumber(), comp()->signature());fflush(stdout);
      calculateBlockWeights(region);
      ///printf("finished computing weights for loop %d in method %s\n", region->getNumber(), comp()->signature());fflush(stdout);
      if (trace())
         {
         traceMsg(comp(), "propagated frequencies: \n");
         for(int32_t i = 0; i < _nodesInCFG; i++)
            traceMsg(comp(), "%d : %d\n", i, _blockWeights[i]);
         }
      }

   static char *testLR = feGetEnv("TR_LRTest");
   if (!testLR)
      {
      lInfo->_seedFreq = getSeedFreq(region);
      // gathers the blocks in the common path
      if (!heuristics(lInfo))
         {
         dumpOptDetails(comp(), "failed...unable to select trace inside the loop\n");
         return false;
         }
      }
   else
      {
      if (!heuristics(lInfo, true)) // testing mode
         {
         dumpOptDetails(comp(), "failed...unable to select trace inside the loop\n");
         return false;
         }
      }


   if (trace())
      traceMsg(comp(), "gathered information for loop (%d)\n", lInfo->_regionNumber);
   return true;
   }


/////////////////////////////
// helper routines
//
//

int32_t TR_LoopReplicator::countChildren(TR::Node *node, vcount_t visitCount)
   {
   // counts children not yet visited
   //
   if (!node)
      return 0;

   if (node->getVisitCount() == visitCount)
      return 0;

   node->setVisitCount(visitCount);
   int32_t nodeCount = 1;
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      nodeCount += countChildren(node->getChild(i), visitCount);
      }
   return nodeCount;
   }

static void collectNonColdLoops(TR::Compilation *c, TR_RegionStructure *region, List<TR_RegionStructure> &innerLoops);

// Find non-cold loops strictly nested within region.
static void collectNonColdInnerLoops(TR::Compilation *c, TR_RegionStructure *region, List<TR_RegionStructure> &innerLoops)
   {
   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *node = it.getFirst();
        node;
        node = it.getNext())
      {
      if (node->getStructure()->asRegion())
         collectNonColdLoops(c, node->getStructure()->asRegion(), innerLoops);
      }
   }

// Find non-cold loops within region, possibly including region itself.
static void collectNonColdLoops(TR::Compilation *c, TR_RegionStructure *region, List<TR_RegionStructure> &loops)
   {
   if (region->getEntryBlock()->isCold())
      return;

   List<TR_RegionStructure> innerLoops(c->trMemory());
   collectNonColdInnerLoops(c, region, innerLoops);

   if (region->isNaturalLoop() && innerLoops.isEmpty())
      loops.add(region);
   else
      loops.add(innerLoops);
   }

bool TR_LoopReplicator::checkInnerLoopFrequencies(TR_RegionStructure *region, LoopInfo *lInfo)
   {
   // dont bother with small traces
   //
   int32_t count = 0;
   for (BlockEntry *be = (lInfo->_nodesCommon).getFirst(); be; be = be->getNext())
      count++;
   if (count < MIN_BLOCKS_IN_TRACE)
      return true;

   if (comp()->getFlowGraph()->getMaxFrequency() <= 0)
      {
      if (trace())
         traceMsg(comp(), "no frequency info\n");
      return true; // maybe this should actually be false...
      }

   if (trace())
      traceMsg(comp(), "inspecting non-cold inner loops\n");

   List<TR_RegionStructure> innerLoops(trMemory());
   collectNonColdInnerLoops(comp(), region, innerLoops);

   if (innerLoops.isEmpty())
      {
      if (trace())
         traceMsg(comp(), "failed to find non-cold inner loops; will attempt to replicate\n");
      return true;
      }

   logTrace(lInfo);

   TR_ScratchList<TR::Block> hotInnerLoopHeaders(trMemory());
   int32_t outerLoopFrequency = region->getEntryBlock()->getFrequency();
   ListIterator<TR_RegionStructure> it(&innerLoops);
   for (TR_RegionStructure *loop = it.getFirst(); loop; loop = it.getNext())
      {
      if (trace())
         traceMsg(comp(), "\tlooking at inner loop %d\n", loop->getNumber());

      TR::Block * const innerLoopHeader = loop->getEntryBlock();
      int32_t entryBlockFrequency = innerLoopHeader->getFrequency();
      float outerLoopRelativeFrequency = entryBlockFrequency / (float)outerLoopFrequency;
      bool isInnerLoopHot = (outerLoopRelativeFrequency > 1.3f); // FIXME: const
      if (trace())
         traceMsg(comp(), "\t  outerloop relative frequency = %.3g\n", outerLoopRelativeFrequency);

      if (!isInnerLoopHot && outerLoopFrequency == 6)
         {
         isInnerLoopHot = true;
         if (trace())
            traceMsg(comp(), "\t  considered hot because outer loop has frequency 6\n");
         }

      if (isInnerLoopHot)
         {
         if (trace())
            traceMsg(comp(), "\t  this is a hot inner loop\n");
         hotInnerLoopHeaders.add(innerLoopHeader);
         if (!searchList(innerLoopHeader, common, lInfo))
            {
            countReplicationFailure("HotInnerLoopNotOnTrace", loop->getNumber());
            traceMsg(comp(), "not going to replicate loop because hot inner loop %d is not on the trace\n", loop->getNumber());
            return false;
            }
         }
      }

   return shouldReplicateWithHotInnerLoops(region, lInfo, &hotInnerLoopHeaders);
   }

bool TR_LoopReplicator::shouldReplicateWithHotInnerLoops(
   TR_RegionStructure *region,
   LoopInfo *lInfo,
   TR_ScratchList<TR::Block> *hotInnerLoopHeaders)
   {
   if (comp()->getOption(TR_DisableLoopReplicatorColdSideEntryCheck))
      return true;

   // If there are no hot inner loops, then replicate away.
   if (hotInnerLoopHeaders->isEmpty())
      return true;

   // When there are hot inner loops, we only want to replicate when we'll
   // remove confounding paths leading into those inner loops. Right now we're
   // only hoping to eliminate cold paths. Otherwise, we don't really expect
   // any benefit from replicating an outer loop.
   //
   // Removal of cold paths is helpful because they often contain things we
   // don't want to consider, such as calls, and they're often reached by
   // failing guards. Splitting the tail in this case can help later analysis,
   // e.g. VP may be able to fold guards within the hot inner loops.
   //
   // The following generates a conservative answer to whether we'll be able to
   // remove any such cold paths, by requiring that the trace encounter a
   // side-entry directly from a cold block before it hits either a hot inner
   // loop or a branch within the trace.
   //
   // If it does, then we can definitely remove cold paths from consideration.
   //
   // Otherwise, we may miss some opportunities. For instance, the side-entry
   // could occur only after a branch within the trace, or it may not come
   // directly from a cold block. It's also possible that the eliminated cold
   // path doesn't lead to every hot inner loop, but only most of them, for
   // example. The heuristic can be extended to cover such cases later as
   // necessary.
   //
   if (trace())
      traceMsg(comp(), "Loop has hot inner loops. Looking for early cold side-entry.\n");

   TR::Block * const outerLoopHeader = region->getEntryBlock();
   TR::Block *tracePrefixCursor = outerLoopHeader;
   for (;;)
      {
      // If this block has only one successor in the trace, move to that one.
      // If it has multiple, stop searching.
      TR::Block *tracePrefixNext = NULL;
      for (auto e = tracePrefixCursor->getSuccessors().begin(); e != tracePrefixCursor->getSuccessors().end(); ++e)
         {
         TR::Block * const succ = toBlock((*e)->getTo());
         if (succ == outerLoopHeader)
            continue; // Don't follow back-edges.
         if (!searchList(succ, common, lInfo))
            continue; // Don't leave the trace.
         if (tracePrefixNext != NULL)
            {
            // Stop due to branching paths within the trace.
            countReplicationFailure("HotInnerLoopHitBranchWithoutColdSideEntry", region->getNumber());
            if (trace())
               traceMsg(comp(), "Hit a branch without finding a cold side-entry. Will not replicate.\n");
            return false;
            }
         tracePrefixNext = succ;
         }

      if (tracePrefixNext == NULL)
         {
         // This is supposed to be impossible, since it means that we've
         // iterated over the entire trace without encountering a hot inner
         // loop header. But there has to be a hot inner loop header in the
         // trace, since we have at least one hot inner loop, and we verified
         // in checkInnerLoopFrequencies that each hot inner loop header is in
         // the trace.
         //
         // To see why we must have iterated over the entire trace, consider
         // that reaching this point means that we haven't returned due to
         // finding a branch within the trace. For any block on the trace,
         // there is a path to it from outerLoopHeader, and that path has to be
         // a prefix of the sequence of blocks considered by this loop.
         //
         TR_ASSERT(false, "cold side-entry detection ran out of trace\n");

         countReplicationFailure("HotInnerLoopRanOutOfTrace", region->getNumber());
         // In production, just safely return false for this case.
         if (trace())
            traceMsg(comp(), "Ran out of trace without finding a cold side-entry. Will not replicate.\n");
         return false;
         }

      tracePrefixCursor = tracePrefixNext;
      if (trace())
         traceMsg(comp(), "Checking for cold side-entries targeting block_%d\n", tracePrefixCursor->getNumber());

      // If this block is the target of a side-entry from a cold block, good!
      // Note that edges into outerLoopHeader are not side-entries, but we're
      // not looking at outerLoopHeader here.
      TR_ASSERT(tracePrefixCursor != outerLoopHeader,
         "cold side-entry detection returned to outerLoopHeader\n");

      for (auto e = tracePrefixCursor->getPredecessors().begin(); e != tracePrefixCursor->getPredecessors().end(); ++e)
         {
         TR::Block * const pred = toBlock((*e)->getFrom());
         if (pred->isCold() && !searchList(pred, common, lInfo))
            {
            if (trace())
               traceMsg(comp(), "Found a cold side-entry into block_%d from block_%d. Will replicate.\n", tracePrefixCursor->getNumber(), pred->getNumber());
            return true;
            }
         }

      // Don't search past the header of a hot inner loop.
      if (hotInnerLoopHeaders->find(tracePrefixCursor))
         {
         countReplicationFailure("HotInnerLoopNoColdSideEntry", region->getNumber());
         if (trace())
            traceMsg(comp(), "Hit a hot inner loop without finding a cold side-entry. Will not replicate.\n");
         return false;
         }
      }
   }

bool TR_LoopReplicator::heuristics(LoopInfo *lInfo)
   {
   // pick a trace in the loop based on some heuristics [basic block frequencies]
   // any algorithm can be implemented here; however it should probably have the side
   // effect of populating lInfo->_nodesCommon for a given loop
   //
   TR_RegionStructure *region = lInfo->_region;

   if (trace())
      traceMsg(comp(), "analyzing region - %d (%p)\n", region->getNumber(), region);

   TR::Block *seed = region->getEntryBlock();

   BlockEntry *be = new (trStackMemory()) BlockEntry;
   be->_block = seed;
   (lInfo->_nodesCommon).append(be);
   if (trace())
      traceMsg(comp(), "   adding loop header %d as seed\n", seed->getNumber());
   _blocksVisited->set(seed->getNumber());

   // select path of desirable successors
   TR::Block *X = seed;
   TR_Queue<TR::Block> blockq(trMemory());
   blockq.enqueue(X);
   do
      {
      X = blockq.dequeue();
      if (trace())
         traceMsg(comp(), "current candidate block : %d\n", X->getNumber());
      X = nextCandidate(X, region, true);
      if (!X || searchList(X, common, lInfo))
         continue;
      else
         {
         // add this block to the common path
         BlockEntry *be = new (trStackMemory()) BlockEntry;
         be->_block = X;
         _blocksVisited->set(X->getNumber());
         (lInfo->_nodesCommon).append(be);
         }
      blockq.enqueue(X);
      } while (!blockq.isEmpty());

   // select path of desirable predecessors
#if 0
   X = seed;
   blockq.enqueue(X);
   do
      {
      X = blockq.dequeue();
      if (trace())
         dumpOptDetails(comp(), "candidate block - %d\n", X->getNumber());
      X = nextCandidate(X, region, false);
      if (!X || searchList(X, common, lInfo))
         continue;
      else
         {
         // add this block to the common path
         BlockEntry *be = new (trStackMemory()) BlockEntry;
         be->_block = X;
         _blocksVisited->set(X->getNumber());
         (lInfo->_nodesCommon).append(be);
         }
      blockq.enqueue(X);
      } while (!blockq.isEmpty());
#endif

   // select desirable successors of all blocks
   _bStack = new (trStackMemory()) TR_Stack<TR::Block *>(trMemory(), 32, false, stackAlloc);
   BlockEntry *bE = NULL;
   for (bE = (lInfo->_nodesCommon).getFirst(); bE; bE = bE->getNext())
      _bStack->push(bE->_block);

   if (trace())
      traceMsg(comp(), "attempting to extend trace...\n");

   while(!_bStack->isEmpty())
      {
      X = _bStack->pop();
      processBlock(X, region, lInfo);
      }

   bool checkSideEntrances = true;

   if (_maxNestingDepth > 1)
      {
      checkSideEntrances = checkInnerLoopFrequencies(region, lInfo);
      }

   if (checkSideEntrances)
      lInfo->_replicated = gatherBlocksToBeCloned(lInfo);
   else
      lInfo->_replicated = false;

   logTrace(lInfo);

   if (!lInfo->_replicated)
      dumpOptDetails(comp(), "no side entrance found into trace; no replication will be performed\n");

   return lInfo->_replicated;
   }

void TR_LoopReplicator::logTrace(LoopInfo *lInfo)
   {
   if (!trace())
      return;

   traceMsg(comp(), "trace selected in loop :\n");
   traceMsg(comp(), "{ ");
   for (BlockEntry *be = (lInfo->_nodesCommon).getFirst(); be; be = be->getNext())
      traceMsg(comp(), "%d -> ", (be->_block)->getNumber());
   traceMsg(comp(), " }\n");
   }

TR::Block * TR_LoopReplicator::nextCandidate(TR::Block *X, TR_RegionStructure *region, bool doSucc)
   {
   TR::CFGEdge *edge = NULL;
   TR::Block *seed = region->getEntryBlock();
   TR::Block *cand = NULL;
   TR::Block *Y = bestSuccessor(region, X, &edge);
   if (Y)
      {
      // could be skipping over inner loops
      // in which case edge will be null
      if (!edge)
         {
         if (trace())
            traceMsg(comp(), "   candidate is %d\n", Y->getNumber());
         cand = Y;
         }
      else if (computeWeight(edge))
         {
         if (trace())
            traceMsg(comp(), "   candidate (%d) satisfied weight computation\n", Y->getNumber());
         cand = Y;
         }
      }
   return cand;
   }

// analyze a block's successors to see if they can be included
void TR_LoopReplicator::processBlock(TR::Block *X, TR_RegionStructure *region, LoopInfo *lInfo)
   {
   TR::Block *seed = region->getEntryBlock();
   for (auto e = X->getSuccessors().begin(); e != X->getSuccessors().end(); ++e)
      {
      TR::Block *dest = toBlock((*e)->getTo());
      // a successor could either be
      // 1. cold
      // 2. header of inner loop
      // 3. back-edge -> header
      // 4. loop exit -> exit of the region
      // 5. normal loop block
      if (dest->isCold())
         continue;
      else if (isBackEdgeOrLoopExit(*e, region))
         continue;
      BlockEntry *bE = searchList(dest, common, lInfo);
      if (bE && bE->_nonLoop)
         continue;
      else if (_blocksVisited->get(dest->getNumber()))
         continue;

      if (computeWeight(*e))
         {
         if (trace())
            traceMsg(comp(), "   candidate (%d) satisfied weight computation, extending trace\n", dest->getNumber());
         // favorable block found, extend the trace
         BlockEntry *bE = new (trStackMemory()) BlockEntry;
         bE->_block = dest;
         (lInfo->_nodesCommon).append(bE);
         _blocksVisited->set(dest->getNumber());
         _bStack->push(dest);
         }
      }
   }

bool TR_LoopReplicator::isBackEdgeOrLoopExit(TR::CFGEdge *e, TR_RegionStructure *region, bool testSGraph)
   {
   // check if this is the loop back-edge or an exit-edge
   //
   TR_Structure *destStructure = NULL;
   if (!testSGraph)
      destStructure = toBlock(e->getTo())->getStructureOf();
   else
      destStructure = toStructureSubGraphNode(e->getTo())->getStructure();

   bool notExitEdge = region->contains(destStructure, region->getParent());
   if (!notExitEdge || destStructure == region->getEntry()->getStructure())
      return true;
   return false;
   }


bool TR_LoopReplicator::computeWeight(TR::CFGEdge *edge)
   {
   TR::Block *X = toBlock(edge->getFrom());
   TR::Block *Y = toBlock(edge->getTo());

   int32_t Xfreq = getBlockFreq(X);
   int32_t Yfreq = getBlockFreq(Y);
   int32_t seedFreq = _curLoopInfo->_seedFreq;

   //float w1 = ((float)(edge->getFrequency()))/(float)Xfreq;

   // the criteria currently used is that the successor block
   // should be at least T% of the current block and Ts% of
   // the seed  (seed in our case is the loop entry)
   // w(x->y)/w(x) >= T && w(y)/w(s) >= Ts
   // where T = block threshold
   //      Ts = seed threshold
   //     x,y = blocks and x->y is the edge from x to y
   //
   float w1 = ((float)Yfreq)/(float)Xfreq;
   float w2 = ((float)Yfreq)/(float)seedFreq;
   if (trace())
      {
      traceMsg(comp(), "   weighing candidate : %d (Y)  predeccessor : %d (X)\n", Y->getNumber(), X->getNumber());
      traceMsg(comp(), "      w(Y): %d w(X): %d w(seed): %d w(Y)/w(X): %.4f w(Y)/w(seed): %.4f\n", Yfreq, Xfreq,
                           seedFreq, w1, w2);
      }
   return (w1 >= BLOCK_THRESHOLD && w2 >= SEED_THRESHOLD) ? true : false;
   }

// return the potential successor node in the CFG
TR::Block * TR_LoopReplicator::bestSuccessor(TR_RegionStructure *region, TR::Block *node, TR::CFGEdge **edge)
   {
   TR::Block *cand = NULL;

   if (trace())
       traceMsg(comp(), "   analyzing region %d (%p)\n", region->getNumber(), region);
   int16_t candFreq = -1;
   for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
      {
      TR::Block *dest = toBlock((*e)->getTo());
      if (trace())
         traceMsg(comp(), "   analyzing successor block : %d\n", dest->getNumber());
      TR_Structure *destStructure = dest->getStructureOf();
      TR_Structure *parentStructure = destStructure->getParent();
      if (trace())
         traceMsg(comp(), "      found parent %p  is block a direct descendent? (%s)\n", destStructure->getParent(),
                           (region->contains(parentStructure, region->getParent()) ? "yes" : "no"));
      //dumpOptDetails(comp(), "destStruct - %p\n", findNodeInHierarchy(region, destStructure->getNumber()));
      // check for exit edges & loop backedges
      bool notExitEdge = region->contains(destStructure, region->getParent());
      if (!notExitEdge || destStructure == region->getEntry()->getStructure())
         {
         if (trace())
            traceMsg(comp(), "      isRegionExit? (%s) successor structure %p\n", (!notExitEdge ? "yes" : "no"),
                              destStructure);
         continue;
         }

      // ignore cold successors
      if (dest->isCold())
         continue;

      int32_t destFreq = dest->getFrequency();
      static char *pEnv = feGetEnv("TR_NewLRTracer");
      if (pEnv)
         destFreq = _blockWeights[dest->getNumber()];
      if (destFreq > candFreq)
         {
         candFreq = destFreq;
         cand = dest;
         *edge = *e;
         }
      }
   if (cand)
      {
      // skip over loops already examined
      nextSuccessor(region, &cand, edge);
      if (trace())
         traceMsg(comp(), "   next candidate chosen : %d (Y)\n", cand->getNumber());
      }
   return cand;
   }

// if the current candidate is the start of an inner loop [which would have already been
// analyzed]; this routine skips the loop, picking the inner loop exit
void TR_LoopReplicator::nextSuccessor(TR_RegionStructure *region, TR::Block **cand, TR::CFGEdge **edge)
   {
   TR::CFGEdge *succEdge = NULL;
   TR_Structure *candStructure = (*cand)->getStructureOf();
   TR_RegionStructure *parentStructure = candStructure->getParent()->asRegion();
   if ((parentStructure != region) &&
         (parentStructure && parentStructure->isNaturalLoop()))
      {
      ListIterator<TR::CFGEdge> eIt(&parentStructure->getExitEdges());
      if (trace())
         traceMsg(comp(), "   inner loop detected : %p , exit edges are :\n", parentStructure);
      TR::CFGEdge *e = NULL;
      for (e = eIt.getFirst(); e; e = eIt.getNext())
         {
         // an exit edge could be on either the header or
         // the block containing the back edge. search the exit edges
         // to see if there is an exit to a block in the outer
         // loop [there could be edges to say blocks which throw exceptions
         // or return & these blocks will be in the parentstructure of the
         // outer loop which is an acyclic region]

         TR_Structure *dest = (_blocksInCFG[e->getTo()->getNumber()])->getStructureOf();
         TR_Structure *source = (_blocksInCFG[e->getFrom()->getNumber()])->getStructureOf();
         if (trace())
            traceMsg(comp(), "      %d (%p) -> %d (%p)\n", e->getFrom()->getNumber(), source,
                              e->getTo()->getNumber(), dest);
         if (region->contains(dest, region->getParent()))
            {
            if (trace())
               traceMsg(comp(), "   found edge to %p (%d)\n", dest, _blocksInCFG[e->getTo()->getNumber()]);
            succEdge = e;
            break;
            }
         }
      if (!succEdge)
         {
         // FIXME: this branch should not be taken
         TR_ASSERT(succEdge, "loopReplicator: no exit edge found for loop!\n");
         *cand = NULL;
         *edge = NULL;
         return;
         }
      //if (succEdge)
         {
         int32_t num = succEdge->getTo()->getNumber();
         if (trace())
            traceMsg(comp(), "      choosing candidate : %d (%p)\n", num, _blocksInCFG[num]);
         LoopInfo *lInfo = findLoopInfo(region->getNumber());
         TR_ASSERT(lInfo, "no loop info for loop?\n");
         // add all the blocks in the inner loop to the current trace
         TR_ScratchList<TR::Block> blocksInLoop(trMemory());
         parentStructure->getBlocks(&blocksInLoop);
         ListIterator<TR::Block> bIt(&blocksInLoop);
         for (TR::Block *b = bIt.getFirst(); b; b = bIt.getNext())
            {
            if (!searchList(b, common, lInfo))
               {
               BlockEntry *bE = new (trStackMemory()) BlockEntry;
               // poison the blocks
               bE->_nonLoop = true;
               bE->_block = b;
               (lInfo->_nodesCommon).append(bE);
               _blocksVisited->set(b->getNumber());
               }
            }
         *cand = _blocksInCFG[num];
         //*edge = succEdge;
         *edge = NULL;
         return;
         }
      }
   }

// analyze the trace looking for side-entrances; if any are found, then the loop
// is replicated
bool TR_LoopReplicator::gatherBlocksToBeCloned(LoopInfo *lInfo)
   {
   TR_RegionStructure *region = lInfo->_region;
   TR::Block *entry = region->getEntryBlock();

   bool sideEntrance = false;

   if (trace())
      traceMsg(comp(), "checking for side-entrances :\n");

   BlockEntry *be;
   for (be = (lInfo->_nodesCommon).getFirst(); be; be = be->getNext())
      {
      TR::Block *b = be->_block;
      // skip analyzing entry blocks
      //
      if (b == entry)
         continue;

      int32_t bNum = b->getNumber();
      for (auto e = b->getPredecessors().begin(); e != b->getPredecessors().end(); ++e)
         {
         TR::Block *source = toBlock((*e)->getFrom());

         ///dumpOptDetails(comp(), "ananlyzing %d -> %d\n", source->getNumber(), bNum);
         BlockEntry *sourceBE = searchList(source, common, lInfo);

         ///dumpOptDetails(comp(), "sourceBE %p mapper %d nonLoop %d\n", sourceBE, _blockMapper[source->getNumber()], be->_nonLoop);
         if (!sourceBE ||
               _blockMapper[source->getNumber()] ||
               be->_nonLoop)
            {
            // either found a side-entrance or pred is cloned
            // so this blocks gets cloned. if the pred is part
            // of an inner loop, then its already been added to
            // the common list, don't analyze it unless the inner
            // loop itself needs to be cloned. (this can happen if
            // one of the preds of the inner loop header is cold and
            // the inner loop is being treated as a merge point)
            //
            bool doClone = true;
            static char *enableInnerLoopChecks = feGetEnv("TR_lRInnerLoopChecks");
            if (enableInnerLoopChecks &&
                  be->_nonLoop &&
                  sourceBE &&
                  !_blockMapper[source->getNumber()])
               {
               ///dumpOptDetails(comp(), "no clone for %d -> %d\n", source->getNumber(), bNum);
               doClone = false;
               }

            if (doClone)
               {
               sideEntrance = true;
               if (trace())
                  traceMsg(comp(), "   found %d -> %d\n", source->getNumber(), bNum);
               BlockEntry *bE = new (trStackMemory()) BlockEntry;
               bE->_block = b;
               (lInfo->_blocksCloned).append(bE);
               _blockMapper[bNum] = b;
               break;
               }
            }
         }
      }

   if (sideEntrance)
      {
      if (trace())
         {
         traceMsg(comp(), "blocks to be cloned : \n");
         traceMsg(comp(), "{ ");
         for (BlockEntry *be = (lInfo->_blocksCloned).getFirst(); be; be = be->getNext())
            traceMsg(comp(), " %d ", (be->_block)->getNumber());
         traceMsg(comp(), " }\n");
         }
      return true;
      }

   // no side entrance found, but dont give up yet.
   // try one more time to see if there are
   // multiple back-edges; some of which might
   // emanate from cold paths
   ////TR_ScratchList<TR::CFGEdge> backEdges(trMemory());
   TR::CFGEdge *e = NULL;
   for(auto e = entry->getPredecessors().begin(); e != entry->getPredecessors().end(); ++e)
      {
      TR::Block *predBlock = toBlock((*e)->getFrom());
      if (!region->contains(predBlock->getStructureOf(), region->getParent()))
         continue;
      // backedge found
      ////backEdges.add(e);
      TR::Block *source = toBlock((*e)->getFrom());
      if (!searchList(source, common, lInfo))
         {
         sideEntrance = true;
         break;
         }
      }
#if 0
   TR::Block *bBE = NULL;
   if (!backEdges.isSingleton())
      {
      ListIterator<TR::CFGEdge> eIt(&backEdges);
      for (e = eIt.getFirst(); e; e = eIt.getNext())
         {
         bBE = toBlock(e->getFrom());
         if (!searchList(bBE, common, lInfo))
            {
            sideEntrance = true;
            break;
            }
         }
      }
#endif
   if (sideEntrance)
      {
      if (trace())
         traceMsg(comp(), "found a rather cooler backedge\n");
      return true;
      }

   countReplicationFailure("NoSideEntryFound", region->getNumber());
   if (trace())
      traceMsg(comp(), "   no side-entrance found\n");
   return false;
   }


/// Perform tail-duplication by cloning all blocks starting from the first
/// side-entrance into the trace
void TR_LoopReplicator::doTailDuplication(LoopInfo *lInfo)
   {
   TR_RegionStructure *region = lInfo->_region;
   TR::Block *loopHeader = region->getEntryBlock();
   // find the original last treetop
   // currently blocks are placed at the end of the method
   // hopefully, block re-ordering should re-order if necessary
   // FIXME: find the first block that does not fall into its next block
   TR::TreeTop *endTreeTop = findEndTreeTop(region);
   if (trace())
      traceMsg(comp(), "placing trees at position (%p) in method\n", endTreeTop);

   // duplicate the blocks starting with the first side entrance
   // reset the blockMapper first
   //
   memset(_blockMapper, 0, _nodesInCFG * sizeof(TR::Block *));
   TR_BlockCloner cloner(_cfg, true);
   BlockEntry *be = NULL;
      int32_t defaultFrequency = MAX_COLD_BLOCK_COUNT+1;
   for (be = (lInfo->_blocksCloned).getFirst(); be; be = be->getNext())
      {
      TR::Block *n = be->_block;
      if ((n->getNumber() < _nodesInCFG)) /*&&
            _blockMapper[n->getNumber()])*/
         {
         TR::Block *block = toBlock(n);
         TR::Block *newBlock = cloner.cloneBlocks(block, block);


         newBlock->setFrequency((int32_t)((block->getFrequency()*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
         if (newBlock->getFrequency() < defaultFrequency)
            newBlock->setFrequency(block->getFrequency());

         for (auto e = newBlock->getSuccessors().begin(); e != newBlock->getSuccessors().end(); ++e)
            {
            int32_t edgeFreq = (*e)->getFrequency();
            (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
            if ((*e)->getFrequency() < defaultFrequency)
            	(*e)->setFrequency(edgeFreq);
            }

         for (auto e = newBlock->getExceptionSuccessors().begin(); e != newBlock->getExceptionSuccessors().end(); ++e)
            {
            int32_t edgeFreq = (*e)->getFrequency();
            (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
            if ((*e)->getFrequency() < defaultFrequency)
               (*e)->setFrequency(edgeFreq);
            }

         for (auto e = newBlock->getExceptionPredecessors().begin(); e != newBlock->getExceptionPredecessors().end(); ++e)
            {
            int32_t edgeFreq = (*e)->getFrequency();
            (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
            if ((*e)->getFrequency() < defaultFrequency)
               (*e)->setFrequency(edgeFreq);
            }

         for (auto e = newBlock->getPredecessors().begin(); e != newBlock->getPredecessors().end(); ++e)
            {
            int32_t edgeFreq = (*e)->getFrequency();
            (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
            if ((*e)->getFrequency() < defaultFrequency)
               (*e)->setFrequency(edgeFreq);
            }

        _blockMapper[n->getNumber()] = newBlock;
         }
      }

   // clone the header of the loop
   TR::Block *clonedHeader = cloner.cloneBlocks(loopHeader, loopHeader);
   _blockMapper[loopHeader->getNumber()] = clonedHeader;
   int32_t origLoopHeaderFreq = loopHeader->getFrequency();
   loopHeader->setFrequency((int32_t)((origLoopHeaderFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
   if (loopHeader->getFrequency() < defaultFrequency)
      loopHeader->setFrequency(origLoopHeaderFreq);

   for (auto e = clonedHeader->getSuccessors().begin(); e != clonedHeader->getSuccessors().end(); ++e)
      {
      int32_t edgeFreq = (*e)->getFrequency();
      (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
      if ((*e)->getFrequency() < defaultFrequency)
    	 (*e)->setFrequency(edgeFreq);
      }

   for (auto e = clonedHeader->getExceptionSuccessors().begin(); e != clonedHeader->getExceptionSuccessors().end(); ++e)
      {
      int32_t edgeFreq = (*e)->getFrequency();
      (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
      if ((*e)->getFrequency() < defaultFrequency)
         (*e)->setFrequency(edgeFreq);
      }

   for (auto e = clonedHeader->getExceptionPredecessors().begin(); e != clonedHeader->getExceptionPredecessors().end(); ++e)
      {
      int32_t edgeFreq = (*e)->getFrequency();
      (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
      if ((*e)->getFrequency() < defaultFrequency)
         (*e)->setFrequency(edgeFreq);
      }

   for (auto e = clonedHeader->getPredecessors().begin(); e != clonedHeader->getPredecessors().end(); ++e)
      {
      int32_t edgeFreq = (*e)->getFrequency();
      (*e)->setFrequency((int32_t)((edgeFreq*BLOCK_THRESHOLD_PERCENTAGE)/100)); // cast explicitly
      if ((*e)->getFrequency() < defaultFrequency)
         (*e)->setFrequency(edgeFreq);
      }

   if (trace())
      traceMsg(comp(), "cloned header; %d -> %d\n", loopHeader->getNumber(),
                        _blockMapper[loopHeader->getNumber()]->getNumber());

   if (trace())
      {
      traceMsg(comp(), "cloned blocks : \n");
      traceMsg(comp(), "{\n");
      for (int32_t i = 0; i < _nodesInCFG; i++)
         {
         if (_blockMapper[i])
            traceMsg(comp(), "   %d -> %d;\n", i, _blockMapper[i]->getNumber());
         }
      traceMsg(comp(), "}\n");
      }

   // paste them in the trees
   TR::TreeTop *origEndTreeTop = endTreeTop;
   for (be = (lInfo->_blocksCloned).getFirst(); be; be = be->getNext())
      {
      TR::Block *n = be->_block;
      if (trace())
         traceMsg(comp(), "processing block : %d\n", n->getNumber());
      TR::Block *block = toBlock(n);
      TR::Block *clonedBlock = _blockMapper[n->getNumber()];
      TR::TreeTop *cbStartTree = clonedBlock->getEntry();
      TR::TreeTop *cbEndTree = clonedBlock->getExit();
      endTreeTop->join(cbStartTree);
      cbEndTree->setNextTreeTop(NULL);
      endTreeTop = cbEndTree;
      // gather edges that are from side-entrances
      if (trace())
         traceMsg(comp(), "   predecessors : {");
      for (auto e = block->getPredecessors().begin(); e != block->getPredecessors().end(); ++e)
         {
         TR::Block *source = toBlock((*e)->getFrom());
         if (trace())
            traceMsg(comp(), " %d ", source->getNumber());
         if (!searchList(source, common, lInfo))
            {
            EdgeEntry *edEntry = new (trStackMemory()) EdgeEntry;
            edEntry->_edge = *e;
            (lInfo->_removedEdges).add(edEntry);
            }
         }
      if (trace())
         traceMsg(comp(), "}\n");
      }

   if (trace())
      {
      traceMsg(comp(), "edges removed from cfg : \n");
      for (EdgeEntry *ee = (lInfo->_removedEdges).getFirst(); ee; ee = ee->getNext())
         {
         TR::CFGEdge *e = ee->_edge;
         traceMsg(comp(), "   %d -> %d ; ", e->getFrom()->getNumber(), e->getTo()->getNumber());
         }
      traceMsg(comp(), "\n");
      }

   // add the new blocks to the cfg and fix up the edges
   addBlocksAndFixEdges(lInfo);
   }

// add cloned blocks and fixup the cfg edges
void TR_LoopReplicator::addBlocksAndFixEdges(LoopInfo *lInfo)
   {
   TR_RegionStructure *region = lInfo->_region;

   TR::Block *loopHeader = region->getEntryBlock();

   if (trace())
      traceMsg(comp(), "fixing cfg in loop (%d)\n", lInfo->_regionNumber);
   // set structure to null to prevent repair
   _cfg->setStructure(NULL);

   TR::Block *clonedHeader = _blockMapper[loopHeader->getNumber()];
   // fix the loop entry so the loop backedges on the trace now goto the cloned
   // header (which is the new inner loop)
   fixUpLoopEntry(lInfo, loopHeader);

   // successors
   // fixup edges and add goto's if required; analyze blocks only on the picked trace
   // B - original block on the trace
   // C - its clone
   // OLH - original loop header (now entry of the outer loop)
   // CLH - cloned loop header (now entry of the inner loop)
   // G - goto block
   for (BlockEntry *be = (lInfo->_blocksCloned).getFirst(); be; be = be->getNext())
      {
      TR::Block *origBlock = toBlock(be->_block);
      TR::Block *newBlock = _blockMapper[origBlock->getNumber()];
      if (trace())
         traceMsg(comp(), "processing block (%d) -> clone (%d)\n", origBlock->getNumber(), newBlock->getNumber());

      _cfg->copyExceptionSuccessors(origBlock, newBlock);

      for (auto e = origBlock->getSuccessors().begin(); e != origBlock->getSuccessors().end(); ++e)
         {
         TR::Block *dest = toBlock((*e)->getTo());
         if (trace())
            traceMsg(comp(), "   edge %d -> %d\n", origBlock->getNumber(), dest->getNumber());
         // case 1
         // B->CLH
         if (dest == clonedHeader)
            {
            // do nothing
            if (trace())
               traceMsg(comp(), "      back-edge ; clonedHeader %d -> %d\n", origBlock->getNumber(),
                                 clonedHeader->getNumber());
            continue;
            }
         // case 2
         // B->OLH
         if (dest == loopHeader)
            {
            if (trace())
               traceMsg(comp(), "      back-edge ; loopHeader %d -> %d\n", origBlock->getNumber(), loopHeader->getNumber());
            // fixed; now add a backedge C->OLH
            dest = _blockMapper[origBlock->getNumber()];
            TR::Node *branchNode = dest->getExit()->getPrevRealTreeTop()->getNode();
            if (branchNode->getOpCode().isBranch())
               {
               // B->OLH via a conditional
               if (branchNode->getBranchDestination()->getNode()->getBlock() == loopHeader)
                  {
                  // OLH is the target; set the right target in C
                  branchNode->setBranchDestination(loopHeader->getEntry());
                  _cfg->addEdge(TR::CFGEdge::createEdge(dest,  loopHeader, trMemory()));
                  if (trace())
                     traceMsg(comp(), "      added back-edge %d -> %d\n", dest->getNumber(), loopHeader->getNumber());
                  }
               else
                  {
                  // OLH on the fall-through; add C->G->OLH
                  TR::Block *gotoBlock = createEmptyGoto(dest, loopHeader);
                  _cfg->addNode(gotoBlock);
                  _cfg->addEdge(TR::CFGEdge::createEdge(dest,  gotoBlock, trMemory()));
                  _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  loopHeader, trMemory()));
                  if (trace())
                     traceMsg(comp(), "      added goto-block_%d\n", gotoBlock->getNumber());
                  }
               }
            else if ( (branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren()) && branchNode->getOpCode().hasBranchChildren())
               {
               // just add an edge from the switch block back to the loopHeader
               //
               if (trace())
                  traceMsg(comp(), "      added  back-edge for jumpwithmultipletarget %d -> %d\n", newBlock->getNumber(), dest->getNumber());
               _cfg->addEdge(TR::CFGEdge::createEdge(dest,  loopHeader, trMemory()));
               }
            else
               {
               // B simply fall-throughs to OLH; add a goto node to the end of C to allow
               // it to branch to OLH and add edge C->OLH
               TR::Node *gotoNode = TR::Node::create(branchNode, TR::Goto, 0, loopHeader->getEntry());
               TR::TreeTop *gotoTT = TR::TreeTop::create(comp(), gotoNode);
               dest->getExit()->getPrevRealTreeTop()->join(gotoTT);
               gotoTT->join(dest->getExit());
               _cfg->addEdge(TR::CFGEdge::createEdge(dest,  loopHeader, trMemory()));
               if (trace())
                  traceMsg(comp(), "      added edge %d -> %d\n", dest->getNumber(), loopHeader->getNumber());
               }
            }
         // case 3
         // B1->B2 where B1 and B2 are on the trace
         // correspondingly add C1->C2 where C1 & C2 are the respective clones
         else if ((dest->getNumber() < _nodesInCFG) &&
                   _blockMapper[dest->getNumber()] && dest != loopHeader)
            {
            TR::Block *saveDest = dest;
            dest = _blockMapper[dest->getNumber()];
            TR::Node *branchNode = newBlock->getExit()->getPrevRealTreeTop()->getNode();
            if (origBlock->getNextBlock() != saveDest)
               {
               // B1 branches to B2
               if (branchNode->getOpCode().isBranch())
                  {
                  TR::Block *target = branchNode->getBranchDestination()->getNode()->getBlock();
                  if (saveDest == target)
                     {
                     // add C1->C2 and set destination
                     branchNode->setBranchDestination(dest->getEntry());
                     if (trace())
                        traceMsg(comp(), "      added  edge %d -> %d\n", newBlock->getNumber(),
                                           dest->getNumber());
                     _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
                     }
                  }
               else if (branchNode->getOpCode().isSwitch())
                  {
                  // B1 is a switch block; fix up the children in C1 so they have correct
                  // branch destination targets to C2
                  for (int32_t i = branchNode->getCaseIndexUpperBound()-1; i > 0; i--)
                     {
                     TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
                     if (target->getNumber() == saveDest->getNumber())
                        {
                        branchNode->getChild(i)->setBranchDestination(dest->getEntry());
                        if (trace())
                           traceMsg(comp(), "   fixed switch child %d -> %d\n", i, dest->getNumber());
                        }
                     }
                  if (trace())
                     traceMsg(comp(), "      added  edge %d -> %d\n", newBlock->getNumber(),
                                       dest->getNumber());
                  _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
                  }
               else if (branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren())
                  {
                  for (int32_t i = 0; i < branchNode->getNumChildren()-1; i++)
                     {
                     TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
                     if (target->getNumber() == saveDest->getNumber())
                        {
                        branchNode->getChild(i)->setBranchDestination(dest->getEntry());
                        if (trace())
                           traceMsg(comp(), "   fixed jumpwithmultipletarget child %d -> %d\n", i, dest->getNumber());
                        }
                     }
                  if (trace())
                     traceMsg(comp(), "      added  edge %d -> %d\n", newBlock->getNumber(),
                                       dest->getNumber());
                  _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));


                  }
               else if (branchNode->getOpCode().isGoto())
                  {
                  // B1 is a goto block; fix the target in C1 to C2
                  branchNode->setBranchDestination(dest->getEntry());
                  if (trace())
                     traceMsg(comp(), "      added  edge %d -> %d\n", newBlock->getNumber(),
                                       dest->getNumber());
                  _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
                  }
               }
            // B2 is on the fall-through
            // case 3a C1 falls-through to C2; but if C2 is not the next block of C1 in
            // the trees, add C1->G->C2
            else if (newBlock->getNextBlock() != dest &&
                       !branchNode->getOpCode().isJumpWithMultipleTargets() && !branchNode->getOpCode().isGoto())
               {
               // goto required
               TR::Block *gotoBlock = createEmptyGoto(newBlock, dest);
               _cfg->addNode(gotoBlock);
               _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  gotoBlock, trMemory()));
               _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  dest, trMemory()));
               if (trace())
                  traceMsg(comp(), "      added goto-block_%d->%d->%d\n", newBlock->getNumber(),
                                    gotoBlock->getNumber(), dest->getNumber());
               }
            // case 3b C2 is the next block of C1 in the trees
            else
               {
               // C1 could be a switch with target C2
               if (branchNode->getOpCode().isSwitch())
                  {
                  for (int32_t i = branchNode->getCaseIndexUpperBound()-1; i > 0; i--)
                     {
                     TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
                     if (target->getNumber() == saveDest->getNumber())
                        {
                        branchNode->getChild(i)->setBranchDestination(dest->getEntry());
                        if (trace())
                           traceMsg(comp(), "   fixed switch child %d -> %d\n", i, dest->getNumber());
                        }
                     }
                  if (trace())
                     traceMsg(comp(), "      added  edge %d -> %d\n", newBlock->getNumber(),
                                      dest->getNumber());
                  _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
                  }
               else if(branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren())
                  {
                  for (int32_t i =  0 ; i < branchNode->getNumChildren()-1; i++)
                     {
                     TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
                     if (target->getNumber() == saveDest->getNumber())
                        {
                        branchNode->getChild(i)->setBranchDestination(dest->getEntry());
                        if (trace())
                           traceMsg(comp(), "   fixed switch child %d -> %d\n", i, dest->getNumber());
                        }
                     }
                  if (trace())
                     traceMsg(comp(), "      added  edge %d -> %d\n", newBlock->getNumber(),
                                      dest->getNumber());
                  _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
                  }
               else
                  {
                  // C1 simply falls through to C2
                  _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
                  if (branchNode->getOpCode().isGoto())
                     {
                     // but if it is via a goto, fix the destination as C2
                     branchNode->setBranchDestination(dest->getEntry());
                     }
                  if (trace())
                     traceMsg(comp(), "      added edge %d -> %d\n", newBlock->getNumber(), dest->getNumber());
                  }
               }
            }
         // case 4
         // B1->B2 where B1 is on the trace and B2 is not on the trace ie. it is a trace exit
         // and B2 is the next block of B1 in the trees
         // add C1->B2 where C1 is clone of B1
         else if (/*(dest->getNumber() < _nodesInCFG) &&*/ dest == origBlock->getNextBlock() &&
                    checkForSuccessor(dest, loopHeader))
            {
            // the _nodesInCFG test should not be there as B2
            // could be a cloned block - this happens if B2 was
            // part of an outer loop that got replicated before
            // the current loop
            TR::Node *branchNode = newBlock->getExit()->getPrevRealTreeTop()->getNode();
            // if B1 is a switch or goto, then add edge C1->B2
            if ( (branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren()) || branchNode->getOpCode().isGoto())
               {
               _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
               if (trace())
                  traceMsg(comp(), "   added edge %d -> %d\n", newBlock->getNumber(), dest->getNumber());
               }
            else
               {
               // else add C1->G->B2
               TR::Block *gotoBlock = createEmptyGoto(newBlock, dest);
               _cfg->addNode(gotoBlock);
               _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  gotoBlock, trMemory()));
               _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  dest, trMemory()));
               if (trace())
                  traceMsg(comp(), "      added goto-block_%d->%d->%d\n", newBlock->getNumber(),
                                    gotoBlock->getNumber(), dest->getNumber());
               }
            }
         // case 5
         // B1->B2 where B1 is on the trace and B2 is a trace exit
         // add C1->B2
         else if (/*(dest->getNumber() < _nodesInCFG) &&*/
                     checkForSuccessor(dest, loopHeader))// && region->isExitEdge(e))
            {
            // see comment above for the _nodesInCFG test
            _cfg->addEdge(TR::CFGEdge::createEdge(newBlock,  dest, trMemory()));
            if (trace())
               traceMsg(comp(), "      added exit edge %d -> %d\n", newBlock->getNumber(), dest->getNumber());
            }
         }
      }

   // remove edges
   if (trace())
      traceMsg(comp(), "removing edges and adding predecessor edges if required\n");
   EdgeEntry *ee = NULL;
   for (ee = (lInfo->_removedEdges).getFirst(); ee; ee = ee->getNext())
      {
      TR::CFGEdge *e = ee->_edge;
      if (trace())
         traceMsg(comp(), "processing edge \n");
      if (ee->_removeOnly)
         {
         if (trace())
            traceMsg(comp(), "   removed %d -> %d\n", e->getFrom()->getNumber(), e->getTo()->getNumber());
         continue;
         }
      TR::Block *source = toBlock(e->getFrom());
      TR::Block *dest = toBlock(e->getTo());
      if (trace())
         traceMsg(comp(), "   removed & fixed up %d -> %d\n", source->getNumber(), dest->getNumber());
      TR::Block *newDest = _blockMapper[dest->getNumber()];
      //dumpOptDetails(comp(), "next block - %p (%p)\n", source->getNextBlock(), dest);

      // predecessors
      // fixup the side-entrances to the trace
      // B2->B1 where B1 is on the trace and B2 is a side-entrance
      // C1 - clone of B1
      // remove B2->B1 and add edge B2->C1

      // case 1
      // B1 is the next block of B2 in the trees
      if (source->getNextBlock() == dest)
         {
         TR::Node *branchNode = source->getExit()->getPrevRealTreeTop()->getNode();
         // B2 is a switch; adjust the targets to C1
         if (branchNode->getOpCode().isSwitch())
            {
            for (int32_t i = branchNode->getCaseIndexUpperBound()-1; i > 0; i--)
               {
               TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
               if (target->getNumber() == dest->getNumber())
                  {
                  branchNode->getChild(i)->setBranchDestination(newDest->getEntry());
                  if (trace())
                     traceMsg(comp(), "   fixed switch child %d -> %d\n", i, dest->getNumber());
                  }
               }
            _cfg->addEdge(TR::CFGEdge::createEdge(source,  newDest, trMemory()));
            if (trace())
               traceMsg(comp(), "   added edge %d -> %d\n", source->getNumber(), newDest->getNumber());
            }
         else if(branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren())
            {
            for (int32_t i = 0; i < branchNode->getNumChildren()-1; i++)
               {
               TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
               if (target->getNumber() == dest->getNumber())
                  {
                  branchNode->getChild(i)->setBranchDestination(newDest->getEntry());
                  if (trace())
                     traceMsg(comp(), "   fixed switch child %d -> %d\n", i, dest->getNumber());
                  }
               }
            _cfg->addEdge(TR::CFGEdge::createEdge(source,  newDest, trMemory()));
            if (trace())
               traceMsg(comp(), "   added edge %d -> %d\n", source->getNumber(), newDest->getNumber());
            }
         else
            {
            if ((branchNode->getOpCode().isGoto() || branchNode->getOpCode().isBranch()) &&
                (branchNode->getBranchDestination()->getNode()->getBlock()->getNumber() == dest->getNumber()))
               {
               // B2 is a goto block
               // -or- branch with target as the next block;
               // adjust targets to C1
               branchNode->setBranchDestination(newDest->getEntry());
               if (trace())
                  traceMsg(comp(), "   fixed branch target %d -> %d\n", source->getNumber(), dest->getNumber());
               _cfg->addEdge(TR::CFGEdge::createEdge(source,  newDest, trMemory()));
               if (trace())
                  traceMsg(comp(), "   added edge %d -> %d\n", source->getNumber(), newDest->getNumber());
               }
            else
               {
               //fall-through edge
               // add B2->G->C1
               TR::Block *gotoBlock = createEmptyGoto(source, dest, true);
               _cfg->addNode(gotoBlock);
               _cfg->addEdge(TR::CFGEdge::createEdge(source,  gotoBlock, trMemory()));
               _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  newDest, trMemory()));
               if (trace())
                  traceMsg(comp(), "   added goto block_%d->%d->%d\n", source->getNumber(),
                                    gotoBlock->getNumber(), newDest->getNumber());
               }
            }
         }
      // case 2
      // B2 branches to B1 via a conditional
      // add B2->C1
      else
         {
         _cfg->addEdge(TR::CFGEdge::createEdge(source,  newDest, trMemory()));
         if (trace())
            traceMsg(comp(), "   added edge %d -> %d\n", source->getNumber(), newDest->getNumber());
         TR::Node *branchNode = source->getExit()->getPrevRealTreeTop()->getNode();
         if (branchNode->getOpCode().isBranch())
            {
            // set branch destination to C1 in B2
            TR::Block *target = branchNode->getBranchDestination()->getNode()->getBlock();
            if (dest == target)
               {
               branchNode->setBranchDestination(newDest->getEntry());
               }
            }
         else if (branchNode->getOpCode().isSwitch())
            {
            // adjust targets in B2 to C1
            for (int32_t i = branchNode->getCaseIndexUpperBound()-1; i > 0; i--)
               {
               TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
               if (target->getNumber() == dest->getNumber())
                  {
                  branchNode->getChild(i)->setBranchDestination(newDest->getEntry());
                  if (trace())
                     traceMsg(comp(), "   fixed switch child %d -> %d\n", i, dest->getNumber());
                  }
               }
            }
         else if (branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren())
            {
            // adjust targets in B2 to C1
            for (int32_t i = 0 ; i < branchNode->getNumChildren()-1; i++)
               {
               TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
               if (target->getNumber() == dest->getNumber())
                  {
                  branchNode->getChild(i)->setBranchDestination(newDest->getEntry());
                  if (trace())
                     traceMsg(comp(), "   fixed switch child %d -> %d\n", i, dest->getNumber());
                  }
               }
            }
         }
      }

   // call removeEdge here as it might result in rearraging
   // trees [due to unreachable blocks]
   if (trace())
      traceMsg(comp(), "actually removing above edges\n");
   for (ee = (lInfo->_removedEdges).getFirst(); ee; ee = ee->getNext())
      {
      TR::CFGEdge *e = ee->_edge;
      _cfg->removeEdge(e);
      }
   }

bool TR_LoopReplicator::checkForSuccessor(TR::Block *dest, TR::Block *loopHeader)
   {
   // return true if dest is a block materialized by the replicator &&
   // the cloned loopheader is a successor to dest
   if (dest->getNumber() >= _nodesInCFG)
      {
      TR::Block *cH = _blockMapper[loopHeader->getNumber()];
      for (auto e = dest->getSuccessors().begin(); e != dest->getSuccessors().end(); ++e)
         if ((*e)->getTo()->getNumber() == cH->getNumber())
            return false;
      }
   return true;
   }

TR::Block * TR_LoopReplicator::createEmptyGoto(TR::Block *source, TR::Block *dest, bool redirect)
   {
   // create an empty goto block
   // its the caller's responsibility to add this block to the cfg
   // and create appropriate edges
   //
   TR::TreeTop *destEntry = dest->getEntry();
   int32_t freq = dest->getFrequency();
   if (source->getFrequency() < dest->getFrequency())
      freq = source->getFrequency();
   TR::Block *gotoBlock = TR::Block::createEmptyBlock(destEntry->getNode(), comp(), freq, source);
   if (trace())
      traceMsg(comp(), "goto block %p freq %d src freq %d dst freq %d\n", gotoBlock, freq, source->getFrequency(), dest->getFrequency());
   TR::TreeTop *gotoEntry = gotoBlock->getEntry();
   TR::TreeTop *gotoExit = gotoBlock->getExit();
   TR::TreeTop *target = NULL;
   // this bool controls whether side-entrance [which could be a fall-through]
   // should be redirected to the outer-loop via a goto
   if (redirect)
      {
      target = _blockMapper[dest->getNumber()]->getEntry();
      }
   else
      target = destEntry;
   TR::Node *gotoNode = TR::Node::create(destEntry->getNextTreeTop()->getNode(),
                                          TR::Goto, 0, target);
   TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
   gotoEntry->join(gotoTree);
   gotoTree->join(gotoExit);
   // paste the block in the trees
   if (source->getNextBlock())
      gotoExit->join(source->getNextBlock()->getEntry());
   source->getExit()->join(gotoEntry);
   gotoEntry->getNode()->setBlock(gotoBlock);
   gotoExit->getNode()->setBlock(gotoBlock);
   return gotoBlock;
   }

TR::TreeTop * TR_LoopReplicator::findEndTreeTop(TR_RegionStructure *region)
   {
   // find the treetop to which the clones should be attached
   //
   TR::TreeTop *endTreeTop = NULL;
   for (TR::TreeTop *tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      tt = tt->getNode()->getBlock()->getExit();
      endTreeTop = tt;
      }
   return endTreeTop;
   }

TR_LoopReplicator::LoopInfo * TR_LoopReplicator::findLoopInfo(int32_t regionNumber)
   {
   LoopInfo *lInfo = NULL;
   for (lInfo = _loopInfo.getFirst(); lInfo; lInfo = lInfo->getNext())
      if (lInfo->_regionNumber == regionNumber)
         break;
   return lInfo;
   }

// find element in the specified list
TR_LoopReplicator::BlockEntry * TR_LoopReplicator::searchList(TR::Block *block, listType lT, LoopInfo *lInfo)
   {
   TR_LinkHeadAndTail<BlockEntry> *head;

   // determines which list to search
   if (lT == common)
      head = &(lInfo->_nodesCommon);
   else
      head = &(lInfo->_blocksCloned);

   for (BlockEntry *b = head->getFirst(); b; b = b->getNext())
      if (b->_block == block)
         return b;
   return NULL;
   }

// final fixup routine that scans the loop info data structure
// and chooses loops to be modified
void TR_LoopReplicator::modifyLoops()
   {
   for (LoopInfo *lInfo = _loopInfo.getFirst(); lInfo; lInfo = lInfo->getNext())
      {
      // clone blocks
      if (lInfo->_replicated &&
          performTransformation(comp(), "%sreplicating loop - %d\n", OPT_DETAILS, lInfo->_regionNumber))
         {
         if (trace())
            {
            printf("--secs-- loopreplication in %s\n", comp()->signature());
            fflush(stdout);
            }
         doTailDuplication(lInfo);
         if (trace())
            {
            traceMsg(comp(), "loop (%d) replicated %d\n", lInfo->_regionNumber, lInfo->_replicated);
            comp()->dumpMethodTrees("trees after replication - ");
            }
         }
      else if (!lInfo->_replicated)
         dumpOptDetails(comp(), "loop (%d) will not be replicated\n", lInfo->_regionNumber);
      }
   }

// clone the original loop header and fixup edges
void TR_LoopReplicator::fixUpLoopEntry(LoopInfo *lInfo, TR::Block *loopHeader)
   {
   _cfg->setStructure(_rootStructure);
   TR_RegionStructure *region = lInfo->_region;
   // collect the backedges to the loop header
   TR_ScratchList<TR::CFGEdge> backEdges(trMemory());
   TR::CFGEdge *e = NULL;
   for(auto e = loopHeader->getPredecessors().begin(); e != loopHeader->getPredecessors().end(); ++e)
      {
      TR::Block *predBlock = toBlock((*e)->getFrom());
      if (!region->contains(predBlock->getStructureOf(), region->getParent()))
         continue;
      // backedge found
      backEdges.add(*e);
      }
   _cfg->setStructure(NULL);

   // attach the clone header as the next block of the originial
   // header in the trees
   TR::Block *clonedHeader = _blockMapper[loopHeader->getNumber()];
   TR::TreeTop *startTT = clonedHeader->getEntry();
   TR::TreeTop *endTT = clonedHeader->getExit();
   if (loopHeader->getNextBlock())
      endTT->join(loopHeader->getNextBlock()->getEntry());
   loopHeader->getExit()->join(startTT);

   //fix exception successors edges
   if (trace())
        traceMsg(comp(), "adding exception successors for new loop header %d\n", clonedHeader->getNumber());

   _cfg->copyExceptionSuccessors(loopHeader, clonedHeader);

   for (auto e = loopHeader->getExceptionSuccessors().begin(); e != loopHeader->getExceptionSuccessors().end(); ++e)
      {
      EdgeEntry *eE = new (trStackMemory()) EdgeEntry;
      eE->_edge = *e;
      eE->_removeOnly = true;
      (lInfo->_removedEdges).add(eE);
      }

   // the successors of the original header are now made the successors
   // of the cloned header;
   // remove the successors of the original header and add a single edge
   // from the original header to the cloned header
   if (trace())
      traceMsg(comp(), "adding successors for new loop header %d\n", clonedHeader->getNumber());
   for (auto e = loopHeader->getSuccessors().begin(); e != loopHeader->getSuccessors().end(); ++e)
      {
      EdgeEntry *eE = new (trStackMemory()) EdgeEntry;
      eE->_edge = *e;
      eE->_removeOnly = true;
      (lInfo->_removedEdges).add(eE);
      TR::Block *dest = toBlock((*e)->getTo());
      // LH has a backedge to itself
      if (dest == loopHeader)
         continue;
      _cfg->addEdge(TR::CFGEdge::createEdge(clonedHeader,  dest, trMemory()));
      if (trace())
         traceMsg(comp(), "   added edge %d -> %d\n", clonedHeader->getNumber(), dest->getNumber());
      }
   _cfg->addEdge(TR::CFGEdge::createEdge(loopHeader,  clonedHeader, trMemory()));
   if (trace())
      traceMsg(comp(), "added edge orig header(%d) -> new header(%d)\n", loopHeader->getNumber(),
                        clonedHeader->getNumber());

   // make the loop backedges that originate from the trace point to the
   // cloned header; backedges from blocks not in the trace still point
   // to the original loop header
   ListIterator<TR::CFGEdge> bIt(&backEdges);
   if (trace())
      traceMsg(comp(), "fixing back-edges for new loop header %d\n", clonedHeader->getNumber());
   for (e = bIt.getFirst(); e; e = bIt.getNext())
      {
      TR::Block *predBlock = toBlock(e->getFrom());
      // fix it to branch to the new clone if
      // pred is on the common path
      if (searchList(predBlock, common, lInfo))
         {
         EdgeEntry *eE = new (trStackMemory()) EdgeEntry;
         eE->_edge = e;
         eE->_removeOnly = true;
         (lInfo->_removedEdges).add(eE);
         if (trace())
            traceMsg(comp(), "   checking edge %d -> %d\n", predBlock->getNumber(), loopHeader->getNumber());
         // LH has a back-edge to itself, so CLH gets a backedge to itself
         if (predBlock == loopHeader)
            {
            _cfg->addEdge(TR::CFGEdge::createEdge(clonedHeader,  clonedHeader, trMemory()));
            if (trace())
               traceMsg(comp(), "   added edge %d -> %d\n", predBlock->getNumber(), clonedHeader->getNumber());
            TR::Node *branchNode = clonedHeader->getExit()->getPrevRealTreeTop()->getNode();
            if (branchNode->getOpCode().isBranch() || branchNode->getOpCode().isGoto())
               {
               branchNode->setBranchDestination(clonedHeader->getEntry());
               }
            continue;
            }
         TR::Node *branchNode = predBlock->getExit()->getPrevRealTreeTop()->getNode();
	 if (branchNode->getOpCode().isSwitch())
            {
            bool edgeFixed = false;
	    for (int32_t i = branchNode->getCaseIndexUpperBound()-1; i > 0; i--)
	       {
	       TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
	       if (target == loopHeader)
		  {
		  branchNode->getChild(i)->setBranchDestination(clonedHeader->getEntry());
		  if (trace())
		     traceMsg(comp(), "   fixed switch child %d -> %d\n", i, clonedHeader->getNumber());
		  if (!edgeFixed)
                     {
		     if (trace())
		        traceMsg(comp(), "      added  edge %d -> %d\n", predBlock->getNumber(),
                                           clonedHeader->getNumber());
                     _cfg->addEdge(TR::CFGEdge::createEdge(predBlock,  clonedHeader, trMemory()));
                     edgeFixed = true;
                     }
		  }

	       }
	    }
         else if (branchNode->getOpCode().isJumpWithMultipleTargets() && branchNode->getOpCode().hasBranchChildren())
            {
            bool edgeFixed = false;
	    for (int32_t i = 0 ; i < branchNode->getNumChildren() -1 ; i++)
	       {
	       TR::Block *target = branchNode->getChild(i)->getBranchDestination()->getNode()->getBlock();
	       if (target == loopHeader)
		  {
		  branchNode->getChild(i)->setBranchDestination(clonedHeader->getEntry());
		  if (trace())
		     traceMsg(comp(), "   fixed switch child %d -> %d\n", i, clonedHeader->getNumber());
		  if (!edgeFixed)
                     {
		     if (trace())
		        traceMsg(comp(), "      added  edge %d -> %d\n", predBlock->getNumber(),
                                           clonedHeader->getNumber());
                     _cfg->addEdge(TR::CFGEdge::createEdge(predBlock,  clonedHeader, trMemory()));
                     edgeFixed = true;
                     }
		  }
	       }
	    }
	 else if (branchNode->getOpCode().isBranch())
	    {
	    // block branches to the loopheader through a conditional
            if (branchNode->getBranchDestination()->getNode()->getBlock() == loopHeader)
               {
               // loopheader is the target of the conditional
               branchNode->setBranchDestination(clonedHeader->getEntry());
               _cfg->addEdge(TR::CFGEdge::createEdge(predBlock,  clonedHeader, trMemory()));
               if (trace())
                  traceMsg(comp(), "   added edge %d -> %d\n", predBlock->getNumber(), clonedHeader->getNumber());
               }
            else
               {
               // loop header is on the fall-through path;
               // insert a gotoblock to the cloned header
               TR::Block *gotoBlock = createEmptyGoto(predBlock, clonedHeader);
               _cfg->addNode(gotoBlock);
               _cfg->addEdge(TR::CFGEdge::createEdge(predBlock,  gotoBlock, trMemory()));
               _cfg->addEdge(TR::CFGEdge::createEdge(gotoBlock,  clonedHeader, trMemory()));
               if (trace())
                  traceMsg(comp(), "   added goto-block_%d->%d->%d\n", predBlock->getNumber(),
				  gotoBlock->getNumber(), clonedHeader->getNumber());
               }
            //else
            //   TR_ASSERT(0, "back edge destination is not the loopheader?\n");
            }
         else
            {
            // block simply falls-through into loopheader; add a goto to the cloned header
            TR::Node *gotoNode = TR::Node::create(branchNode, TR::Goto, 0, clonedHeader->getEntry());
            TR::TreeTop *gotoTT = TR::TreeTop::create(comp(), gotoNode);
            predBlock->getExit()->getPrevRealTreeTop()->join(gotoTT);
            gotoTT->join(predBlock->getExit());
            _cfg->addEdge(TR::CFGEdge::createEdge(predBlock,  clonedHeader, trMemory()));
            if (trace())
               traceMsg(comp(), "   added edge %d -> %d\n", predBlock->getNumber(), clonedHeader->getNumber());
            }
         }
      }

   TR::Node *bbstartNode = loopHeader->getEntry()->getNode();

   // rip out the trees from the original loop header
   loopHeader->getEntry()->join(loopHeader->getExit());

   // add the async tree into the loopheader
   //
   TR::TreeTop *asyncTT = TR::TreeTop::create(comp(), TR::Node::createWithSymRef(bbstartNode, TR::asynccheck, 0, comp()->getSymRefTab()->findOrCreateAsyncCheckSymbolRef(comp()->getMethodSymbol())));
   loopHeader->getEntry()->join(asyncTT);
   asyncTT->join(loopHeader->getExit());
   }

// given a number, find the corresponding sub-node in the structure graph
TR_StructureSubGraphNode * TR_LoopReplicator::findNodeInHierarchy(TR_RegionStructure *region, int32_t num)
   {
   if (!region) return NULL;
   TR_RegionStructure::Cursor sIt(*region);
   for (TR_StructureSubGraphNode *n = sIt.getFirst(); n; n = sIt.getNext())
      {
      if (n->getNumber() == num)
         return n;
      }
   return findNodeInHierarchy(region->getParent()->asRegion(), num);
   }

void TR_LoopReplicator::calculateBlockWeights(TR_RegionStructure *region)
   {
   TR_StructureSubGraphNode *node = region->getEntry();
   TR_Queue<TR_StructureSubGraphNode> blockq(trMemory());
   bool setWeights = false;
   blockq.enqueue(node);
   do
      {
      node = blockq.dequeue();
      if (predecessorsNotYetVisited(region, node))
         {
         dumpOptDetails(comp(), "predecessors not yet visited for block: %d, requeue\n", node->getNumber());
         blockq.enqueue(node);
         continue;
         }
      // pbr
      dumpOptDetails(comp(), "processing block: %d %p\n", node->getNumber(), node);
      if ((node->getStructure()->getParent()->asRegion() == region) &&
            !(node->getStructure()->asRegion() &&
               node->getStructure()->asRegion()->isNaturalLoop()))
         {
         TR::Block *curBlock = node->getStructure()->asBlock()->getBlock();
         int32_t bFreq = curBlock->getFrequency();
         if ((bFreq > 0) || (node == region->getEntry()))
            {
            dumpOptDetails(comp(), "positive non-zero frequency %d for block_%d\n", bFreq, node->getNumber());
            _blockWeights[node->getNumber()] = bFreq;
            }
         else if (curBlock->isCold())
            _blockWeights[node->getNumber()] = 0;
         else
            {
            _blockWeights[node->getNumber()] = deriveFrequencyFromPreds(node, region);
            // pbr
            dumpOptDetails(comp(), "set freq for block(%d) = %d\n", node->getNumber(), _blockWeights[node->getNumber()]);
            }
         }
      for (auto e = node->getSuccessors().begin(); e != node->getSuccessors().end(); ++e)
         {
         TR_StructureSubGraphNode *dest = toStructureSubGraphNode((*e)->getTo());
         //pbr
         dumpOptDetails(comp(), "   successor %d (%p)\n", dest->getNumber(), dest);
         if (!isBackEdgeOrLoopExit(*e, region, true))
            {
            bool innerLoop = ((_seenBlocks[dest->getNumber()] == COLOR_BLACK) &&
                              (dest->getStructure()->asRegion() &&
                                  dest->getStructure()->asRegion()->isNaturalLoop()));
            if ((_seenBlocks[dest->getNumber()] == COLOR_WHITE) || innerLoop)

               {
               // pbr
               dumpOptDetails(comp(), "   adding dest %p %d\n", dest, dest->getNumber());
               blockq.enqueue(dest);
               _seenBlocks[dest->getNumber()] = COLOR_GRAY;
               }
            }
         }
      _seenBlocks[node->getNumber()] = COLOR_BLACK;
      } while(!blockq.isEmpty());
   }

int32_t TR_LoopReplicator::deriveFrequencyFromPreds(TR_StructureSubGraphNode *curNode,
                                                    TR_RegionStructure *region)
   {
   TR::Block *curBlock = curNode->getStructure()->asBlock()->getBlock();
   TR_ScratchList<TR::Block> blocks(trMemory());
   for (auto e = curNode->getPredecessors().begin(); e != curNode->getPredecessors().end(); ++e)
      {
      TR_StructureSubGraphNode *from = toStructureSubGraphNode((*e)->getFrom());
      if (from->getStructure()->getParent()->asRegion() == region)
         {
         if (from->getStructure()->asRegion() &&
               from->getStructure()->asRegion()->isNaturalLoop())
            {
            TR_RegionStructure *r = from->getStructure()->asRegion();
            ListIterator<TR::CFGEdge> eIt(&r->getExitEdges());
            for (TR::CFGEdge *e = eIt.getFirst(); e; e = eIt.getNext())
               {
               TR_Structure *dest = toStructureSubGraphNode(e->getTo())->getStructure();
               if (region->contains(dest, region->getParent()))
                  blocks.add(dest->asBlock()->getBlock());
               }
            }
         else
            blocks.add(from->getStructure()->asBlock()->getBlock());
         // pbr
         dumpOptDetails(comp(), "adding block as preds: %d %p\n", from->getNumber(), from);
         }
      }
   ListIterator<TR::Block> bIt(&blocks);
   int32_t freq = 0;
   for (TR::Block *b = bIt.getFirst(); b; b = bIt.getNext())
      {
      int32_t f = _blockWeights[b->getNumber()];
      // pbr
      dumpOptDetails(comp(), "cumulative freq for block (%d) is : %d\n", b->getNumber(), f);
      if (!(b->getSuccessors().size() == 1))
         {
         bool scale = true;
         int32_t numSuccs = 0;
         for (auto e = b->getSuccessors().begin(); e != b->getSuccessors().end(); ++e)
            {
            numSuccs++;
            TR::Block *d = toBlock((*e)->getTo());
            if ((d == curBlock) ||
                  (!region->contains(d->getStructureOf(), region->getParent())))
               continue;

            if (d->isCold() || (d->getFrequency() > 0))
               scale = false;

            int32_t dFreq;
            if ((_seenBlocks[d->getNumber()] == COLOR_BLACK) && !scale)
               {
               dFreq = _blockWeights[d->getNumber()];
               // pbr
               dumpOptDetails(comp(), "weight of %d from array: %d\n", d->getNumber(), dFreq);
               }
            else
               dFreq = d->getFrequency();

            if (dFreq > f)
               f = dFreq - f;
            else
               f -= dFreq;
            // pbr
            dumpOptDetails(comp(), "after %d diffing dFreq (%d), f = %d\n", numSuccs, dFreq, f);
            }
         if (scale)
            {
            f = f/numSuccs;
            }
         }
      freq += f;
      }
   // pbr
   dumpOptDetails(comp(), "returned freq for block (%d): %d\n", curNode->getNumber(), freq);
   return freq;
   }

bool TR_LoopReplicator::predecessorsNotYetVisited(TR_RegionStructure *r,
                                                  TR_StructureSubGraphNode *curNode)
   {
   if (r->getEntry() == curNode)
      return false;
   for (auto e = curNode->getPredecessors().begin(); e != curNode->getPredecessors().end(); ++e)
      {
      TR_StructureSubGraphNode *from = toStructureSubGraphNode((*e)->getFrom());
      if (_seenBlocks[from->getNumber()] != COLOR_BLACK)
         {
         // pbr
         dumpOptDetails(comp(), "pred (%d) not visited %d\n", from->getNumber(), curNode->getNumber());
         return true;
         }
      }
   return false;
   }

// get seed frequency from looking at backedges
int32_t TR_LoopReplicator::getSeedFreq(TR_RegionStructure *r)
   {
   TR::Block *seed = r->getEntryBlock();
   int32_t seedFreq = seed->getFrequency();
   //if (seedFreq > 0)
   if (seedFreq)
      return seedFreq;

   TR_ScratchList<TR::Block> blocks(trMemory());
   for (auto e = seed->getPredecessors().begin(); e != seed->getPredecessors().end(); ++e)
      {
      TR::Block *source = toBlock((*e)->getFrom());
      if (!r->contains(source->getStructureOf(), r->getParent()))
         continue;
      blocks.add(source);
      }
   seedFreq = getScaledFreq(blocks, seed);
   return ((seedFreq > 0) ? seedFreq : 1);
   }

int32_t TR_LoopReplicator::getBlockFreq(TR::Block *X)
   {
   int32_t freq = X->getFrequency();
   static char *pEnv = feGetEnv("TR_NewLRTracer");
   if (pEnv)
      return _blockWeights[X->getNumber()];
   //if (freq >= 0)
   //if (freq)
   if ((freq != (MAX_COLD_BLOCK_COUNT+1)) &&
       (freq != 0))
      return freq;

   if (X == _curLoopInfo->_region->getEntryBlock())
     return _curLoopInfo->_seedFreq;

   TR_ScratchList<TR::Block> blocks(trMemory());
   for (auto e = X->getPredecessors().begin(); e != X->getPredecessors().end(); ++e)
      blocks.add(toBlock((*e)->getFrom()));

   freq = getScaledFreq(blocks, X);
   return ((freq) ? freq : 1);
   }

// guess block freq from looking at its preds
int32_t TR_LoopReplicator::getScaledFreq(TR_ScratchList<TR::Block> &blocks, TR::Block *X)
   {
   int32_t freq = 0;
   TR_RegionStructure *r = _curLoopInfo->_region;
   ListIterator<TR::Block> bIt(&blocks);
   for (TR::Block *b = bIt.getFirst(); b; b = bIt.getNext())
      {
      int32_t f = b->getFrequency();
      if (!(b->getSuccessors().size() == 1))
         {
         for (auto e = b->getSuccessors().begin(); e != b->getSuccessors().end(); ++e)
            {
            TR::Block *d = toBlock((*e)->getTo());
            if (d == X)
               continue;
            else if (!r->contains(d->getStructureOf(), r->getParent()))
               continue;
            else
               f = f - d->getFrequency();
            }
         }
      freq += f;
      }
   return freq;
   }

// overloaded heuristics; this is a dumb version for testing at count=0
bool TR_LoopReplicator::heuristics(LoopInfo *lInfo, bool dumb)
   {
   TR_RegionStructure *region = lInfo->_region;

   if (trace())
      traceMsg(comp(), "analyzing region - %p\n", region);

   TR_Queue<TR::Block> splitNodes(trMemory());

   TR::Block *entryBlock = region->getEntryBlock();
   TR::Block *current = entryBlock;

   BlockEntry *be = new (trStackMemory()) BlockEntry;
   be->_block = current;
   (lInfo->_nodesCommon).append(be);
   if (trace())
      traceMsg(comp(), "   adding loop header %d\n", current->getNumber());

   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   region->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> bilIt(&blocksInLoop);
   TR::Block *b = NULL;
   for (b = bilIt.getFirst(); b; b = bilIt.getNext())
      {
      if (trace())
         traceMsg(comp(), "   current cand - %d ", b->getNumber());
      if (!searchList(b, common, lInfo))
         {
         if (trace())
            traceMsg(comp(), "\n");
         TR::Block *cand = b;
         TR::CFGEdge *edge = NULL;
         nextSuccessor(region, &cand, &edge);
         if (cand != b)
            {
            if (trace())
               traceMsg(comp(), "   inner loop found bypassing\n");
            b = cand;
            }
         if ((b->getNumber() % 2) == 0)
            {
            if (!searchList(b, common, lInfo))
               {
               BlockEntry *be = new (trStackMemory()) BlockEntry;
               be->_block = b;
               (lInfo->_nodesCommon).append(be);
               if (trace())
                  traceMsg(comp(), "   next candidate chosen - %d\n", cand->getNumber());
               }
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "is already visited\n");
         }
      if (!((b->getSuccessors()).size() == 1) && !splitNodes.find(b))
            splitNodes.enqueue(b);
      }

   // now collect blocks to be cloned
   // and mark this loop as replicated - will be fixed up later
   lInfo->_replicated = gatherBlocksToBeCloned(lInfo);

   if (trace())
      {
      traceMsg(comp(), "trace selected in loop - \n");
      traceMsg(comp(), "            {");
      for (BlockEntry *be = (lInfo->_nodesCommon).getFirst(); be; be = be->getNext())
         traceMsg(comp(), "%d-> ", (be->_block)->getNumber());
      traceMsg(comp(), "}\n");

      traceMsg(comp(), "the control split points in the trace\n");
      ListIterator<TR::Block> nIt(&splitNodes);
      for (TR::Block *n = nIt.getCurrent(); n; n = nIt.getNext())
         traceMsg(comp(), "%d ", n->getNumber());
      traceMsg(comp(), "\n");
      if (!lInfo->_replicated)
         traceMsg(comp(), "no side entrance found into trace; no replication will be performed\n");
      }

   return lInfo->_replicated;
   }

const char *
TR_LoopReplicator::optDetailString() const throw()
   {
   return "O^O LOOP REPLICATOR: ";
   }

#undef COLOR_BLACK
#undef COLOR_WHITE
#undef COLOR_GRAY
#undef INITIAL_BLOCK_WEIGHT
