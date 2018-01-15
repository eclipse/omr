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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "optimizer/LoopCanonicalizer.hpp"

#include <limits.h>                              // for INT_MAX
#include <stdint.h>                              // for int32_t, int64_t, etc
#include <stdlib.h>                              // for NULL, llabs
#include "codegen/CodeGenerator.hpp"             // for CodeGenerator
#include "codegen/FrontEnd.hpp"                  // for TR_FrontEnd, etc
#include "compile/Compilation.hpp"               // for Compilation
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"                        // for ABitVector, etc
#include "cs2/sparsrbit.h"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                      // for SparseBitVector, etc
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                          // for Block, toBlock
#include "il/DataTypes.hpp"                      // for DataTypes::Int32, etc
#include "il/ILOpCodes.hpp"                      // for ILOpCodes::Goto, etc
#include "il/ILOps.hpp"                          // for ILOpCode, etc
#include "il/Node.hpp"                           // for Node, etc
#include "il/Node_inlines.hpp"                   // for Node::getFirstChild, etc
#include "il/Symbol.hpp"                         // for Symbol
#include "il/SymbolReference.hpp"                // for SymbolReference
#include "il/TreeTop.hpp"                        // for TreeTop
#include "il/TreeTop_inlines.hpp"                // for TreeTop::getNode, etc
#include "il/symbol/LabelSymbol.hpp"             // for LabelSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                      // for TR_ASSERT
#include "infra/BitVector.hpp"                   // for TR_BitVector, etc
#include "infra/Cfg.hpp"                         // for CFG, etc
#include "infra/ILWalk.hpp"                      // for PostorderNodeIterator
#include "infra/List.hpp"                        // for ListIterator, etc
#include "infra/CfgEdge.hpp"                     // for CFGEdge
#include "infra/CfgNode.hpp"                     // for CFGNode
#include "infra/Checklist.hpp"                   // for NodeChecklist
#include "optimizer/InductionVariable.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"               // for Optimizer
#include "optimizer/Structure.hpp"               // for TR_RegionStructure, etc
#include "optimizer/UseDefInfo.hpp"              // for TR_UseDefInfo, etc
#include "optimizer/VPConstraint.hpp"            // for TR::VPConstraint
#include "ras/Debug.hpp"                         // for TR_DebugBase

namespace TR { class RegisterMappedSymbol; }

#define OPT_DETAILS "O^O LOOP TRANSFORMATION: "

/**
 * Canonicalize While structures and convert them into If guarded
 * Do While structures. We also add some empty blocks to split critical
 * edges in the CFG for helping PRE (which is really the main optimization
 * that benefits from canonicalization of loops). Note that the general
 * critical edge splitter runs before structural analysis (so that it can
 * avoid the problems associated with complex structure repair and because it
 * does not really need structure for anything). There are some new CFG edges,
 * structures, and trees (actually cloned) that are created in this pass, and
 * for the part of the CFG that has changed now (the original While) we split
 * critical edges in a specific manner.
 */
TR_LoopCanonicalizer::TR_LoopCanonicalizer(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager)
   {}

int32_t TR_LoopCanonicalizer::perform()
   {
   if (!comp()->mayHaveLoops())
      return 0;

   if (comp()->hasLargeNumberOfLoops())
      {
      return 0;
      }

   _loopTestBlock = NULL;
   _storeTrees = NULL;
   _currTree = NULL;
   _insertionTreeTop = NULL;
   _loopTestTree = NULL;
   _asyncCheckTree = NULL;
   _cannotBeEliminated = NULL;

   _neverRead = NULL;
   _neverWritten = NULL;
   _writtenExactlyOnce.Clear();
   _readExactlyOnce.Clear();
   _allKilledSymRefs.Clear();
   _allSymRefs.Clear();

   _numberOfIterations = NULL;
   _constNode = NULL;
   _loadUsedInLoopIncrement = NULL;
   _startOfHeader = NULL;
   _cfg = NULL;
   _rootStructure = NULL;
   _hasPredictableExits = NULL;

   _invariantBlocks.deleteAll();
   _blocksToBeCleansed.deleteAll();
   _analysisStack.clear();

   _whileIndex = 0;
   _visitCount = 0;
   _topDfNum = 0;
   _counter = 0;
   _loopDrivingInductionVar = 0;
   _nextExpression = 0;
   _startExpressionForThisInductionVariable = 0;
   _numberOfTreesInLoop = 0;
   _isAddition = false;
   _requiresAdditionalCheckForIncrement = false;
   _doingVersioning = false;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   optimizer()->changeContinueLoopsToNestedLoops();

   TR_ScratchList<TR::CFGNode> newEmptyBlocks(trMemory());
   TR_ScratchList<TR::CFGNode> newEmptyExceptionBlocks(trMemory());
   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure();

   if (trace())
      {
      traceMsg(comp(), "Starting LoopCanonicalizer\n");
      traceMsg(comp(), "\nCFG before loop canonicalization:\n");
      getDebug()->print(comp()->getOutFile(), _cfg);
      }

   // printTrees();

   // Recursive call through the structure tree that would
   // collect all the While Structures (if any) in the method.
   // Note that only while loops with a single block as its
   // header will be detected here.
   //
   TR_ScratchList<TR_Structure> whileLoops(trMemory());
   ListAppender<TR_Structure> whileLoopsInnerFirst(&whileLoops);
   TR_ScratchList<TR_Structure> doWhileLoops(trMemory());
   ListAppender<TR_Structure> doWhileLoopsInnerFirst(&doWhileLoops);
   _nodesInCycle = new (trStackMemory()) TR_BitVector(_cfg->getNextNodeNumber(), trMemory(), stackAlloc);
   //comp()->incVisitCount();

   detectWhileLoops(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, _rootStructure, true);

   if (whileLoops.isEmpty() && doWhileLoops.isEmpty())
      {
      return false;
      }

   if (trace())
      traceMsg(comp(), "Number of WhileLoops = %d\n", whileLoops.getSize());

   // _startOfHeader originally points to the last tree in the method;
   // this variable is used as a placeholder to keep track of where
   // trees can be moved (sometimes required for efficiency purposes
   // to minimize gotos in the critical 'hot' path between the loop body
   // and the new duplicate header that drives the new Do While loop.
   //
   _startOfHeader = comp()->getMethodSymbol()->getLastTreeTop();

   _whileIndex = 0;
   _counter = 0;
   ListIterator<TR_Structure> whileLoopsIt(&whileLoops);
   TR_Structure *nextWhileLoop;
   for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt.getNext())
      {
      // Can be used for debugging; to stop a particular loop from being
      // and see if the problem persists or not.
      //
      // if (_counter == 1)
      //   break;
      // else
      //   _counter++;

      TR_RegionStructure *naturalLoop = nextWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"Loop canonicalizer, expecting natural loop");
      if (naturalLoop->getEntryBlock()->isCold())
         continue;
      canonicalizeNaturalLoop(naturalLoop);
      }

  ///comp()->dumpMethodTrees("Trees now\n");

   if (trace())
      traceMsg(comp(), "Number of cleansed blocks : %d\n", _blocksToBeCleansed.getSize());
   //
   // Move the trees around in this method so that the blocks that are currently
   // violating the CFG constraints (only 1 branch in a block) are cleansed
   //
   ListIterator<TR::Block> cleansedBlocksIt(&_blocksToBeCleansed);
   TR::Block *nextCleansedBlock = NULL;
   for (nextCleansedBlock = cleansedBlocksIt.getCurrent(); nextCleansedBlock; nextCleansedBlock = cleansedBlocksIt.getNext())
      {
      cleanseTrees(nextCleansedBlock);
      }

   // Move the trees around in this method so that the invariant blocks
   // fall through into the loop where possible
   //
   ListIterator<TR::Block> invariantBlocksIt(&_invariantBlocks);
   TR::Block *nextInvariantBlock = NULL;
   for (nextInvariantBlock = invariantBlocksIt.getCurrent(); nextInvariantBlock; nextInvariantBlock = invariantBlocksIt.getNext())
      {
      makeInvariantBlockFallThroughIfPossible(nextInvariantBlock);
      }


   if (trace())
      traceMsg(comp(), "Number of DoWhileLoops = %d\n", doWhileLoops.getSize());

   ListIterator<TR_Structure> doWhileLoopsIt(&doWhileLoops);
   TR_Structure *nextDoWhileLoop;
   for (nextDoWhileLoop = doWhileLoopsIt.getFirst(); nextDoWhileLoop != NULL; nextDoWhileLoop = doWhileLoopsIt.getNext())
      {
      // Can be used for debugging; to stop a particular loop from being
      // and see if the problem persists or not.
      //
      // if (_counter == 1)
      //   break;
      // else
      //   _counter++;
      TR_RegionStructure *naturalLoop = nextDoWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"Loop canonicalizer, expecting natural loop");
      if (naturalLoop->getEntryBlock()->isCold())
         continue;
      canonicalizeDoWhileLoop(naturalLoop);
      }
/*
   for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt.getNext())
      {
      TR_RegionStructure *naturalLoop = nextWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"Loop canonicalizer, expecting natural loop");
      if (naturalLoop->getEntryBlock()->isCold())
         continue;
      eliminateRedundantInductionVariablesFromLoop(naturalLoop);
      }

   for (nextDoWhileLoop = doWhileLoopsIt.getFirst(); nextDoWhileLoop != NULL; nextDoWhileLoop = doWhileLoopsIt.getNext())
      {
      TR_RegionStructure *naturalLoop = nextDoWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"Loop canonicalizer, expecting natural loop");
      if (naturalLoop->getEntryBlock()->isCold())
         continue;
      eliminateRedundantInductionVariablesFromLoop(naturalLoop);
      }
*/
   // Use/def info and value number info are now bad.
   //
   optimizer()->setUseDefInfo(NULL);
   optimizer()->setValueNumberInfo(NULL);
   //requestOpt(loopVersioner);
   requestOpt(OMR::loopVersionerGroup);

   if (trace())
      {
      traceMsg(comp(), "\nCFG after loop canonicalization:\n");
      getDebug()->print(comp()->getOutFile(), _cfg);
      traceMsg(comp(), "Ending LoopCanonicalizer\n");
      }

   } // scope of the stack memory region


   //if (trace())
      {
      if (trace())
         comp()->dumpMethodTrees("Trees after canonicalization\n");
      }

   return 1; // actual cost
   }





void TR_LoopTransformer::detectWhileLoopsInSubnodesInOrder(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_Structure *root, TR_StructureSubGraphNode *rootNode, TR_RegionStructure *region, vcount_t visitCount, TR_BitVector *pendingList, bool innerFirst)
   {
   if (trace())
      traceMsg(comp(), "Begin looking for canonicalizable loops in node %p numbered %d\n", root, root->getNumber());

   bool alreadyVisitedNode = false;
   //if (rootNode->getVisitCount() == visitCount)
   if (_nodesInCycle->get(rootNode->getNumber()))
      alreadyVisitedNode = true;

   //rootNode->setVisitCount(visitCount);
   _nodesInCycle->set(rootNode->getNumber());

   for (auto pred = rootNode->getPredecessors().begin(); pred != rootNode->getPredecessors().end(); ++pred)
      {
      TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) (*pred)->getFrom();
      TR_Structure *predStructure = predNode->getStructure();
      if (pendingList->get(predStructure->getNumber()) && (!alreadyVisitedNode))
         {
         detectWhileLoopsInSubnodesInOrder(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, predStructure, predNode, region, visitCount, pendingList, innerFirst);
         return;
         }
      }

   for (auto pred = rootNode->getExceptionPredecessors().begin(); pred != rootNode->getExceptionPredecessors().end(); ++pred)
      {
      TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) (*pred)->getFrom();
      TR_Structure *predStructure = predNode->getStructure();
      if (pendingList->get(predStructure->getNumber()) && (!alreadyVisitedNode))
         {
         detectWhileLoopsInSubnodesInOrder(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, predStructure, predNode, region, visitCount, pendingList, innerFirst);
         return;
         }
      }


    _nodesInCycle->empty();
    detectWhileLoops(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, root, innerFirst);
    pendingList->reset(root->getNumber());

    for (auto succ = rootNode->getSuccessors().begin(); succ != rootNode->getSuccessors().end(); ++succ)
      {
      TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) (*succ)->getTo();
      TR_Structure *succStructure = succNode->getStructure();
      bool isExitEdge = false;
      ListIterator<TR::CFGEdge> ei(&region->getExitEdges());
      TR::CFGEdge *edge;
      for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
          {
          if (edge == *succ)
             {
             isExitEdge = true;
             break;
             }
          }

      if ((!isExitEdge) && pendingList->get(succStructure->getNumber()))
         {
    //comp()->incVisitCount();
         _nodesInCycle->empty();
         detectWhileLoopsInSubnodesInOrder(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, succStructure, succNode, region, visitCount, pendingList, innerFirst);
         }
      }

   for (auto succ = rootNode->getExceptionSuccessors().begin(); succ != rootNode->getExceptionSuccessors().end(); ++succ)
      {
      TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) (*succ)->getTo();
      TR_Structure *succStructure = succNode->getStructure();
      bool isExitEdge = false;
      ListIterator<TR::CFGEdge> ei(&region->getExitEdges());
      TR::CFGEdge *edge;
      for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
          {
          if (edge == *succ)
             {
             isExitEdge = true;
             break;
             }
          }

      if ((!isExitEdge) && pendingList->get(succStructure->getNumber()))
         {
    //comp()->incVisitCount();
         _nodesInCycle->empty();
         detectWhileLoopsInSubnodesInOrder(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, succStructure, succNode, region, visitCount, pendingList, innerFirst);
         }
      }
   }






void TR_LoopTransformer::detectWhileLoopsInSubnodesInOrder(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_RegionStructure *region, vcount_t visitCount, TR_BitVector *pendingList, bool innerFirst)
   {
   while (!_analysisStack.isEmpty() &&
          _analysisStack.top()->getStructure() != region)
      {
      TR_StructureSubGraphNode *rootNode = _analysisStack.top();
      TR_Structure *root = rootNode->getStructure();

      if (!pendingList->get(root->getNumber()))
         {
         _analysisStack.pop();
         continue;
         }

      if (trace())
         traceMsg(comp(), "Begin looking for canonicalizable loops in node %p numbered %d\n", root, root->getNumber());

      if (!_nodesInCycle->get(rootNode->getNumber()))
         {
         _nodesInCycle->set(rootNode->getNumber());

         bool stopAnalyzingThisNode = false;
         TR_PredecessorIterator precedingEdges(rootNode);
         for (auto precedingEdge = precedingEdges.getFirst(); precedingEdge; precedingEdge = precedingEdges.getNext())
            {
            TR_StructureSubGraphNode *predNode = (TR_StructureSubGraphNode *) precedingEdge->getFrom();
            TR_Structure *predStructure = predNode->getStructure();
            if (pendingList->get(predStructure->getNumber()))
               {
               _analysisStack.pop();
               _analysisStack.push(predNode);
               stopAnalyzingThisNode = true;
               break;
               }
            }

         if (stopAnalyzingThisNode)
            continue;
         }

      _nodesInCycle->empty();
      detectWhileLoops(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, root, innerFirst);
      pendingList->reset(root->getNumber());
      _analysisStack.pop();

      TR_SuccessorIterator succeedingEdges(rootNode);
      for (auto succeedingEdge = succeedingEdges.getFirst(); succeedingEdge; succeedingEdge = succeedingEdges.getNext())
         {
         TR_StructureSubGraphNode *succNode = (TR_StructureSubGraphNode *) succeedingEdge->getTo();
         TR_Structure *succStructure = succNode->getStructure();
         bool isExitEdge = false;
         ListIterator<TR::CFGEdge> ei(&region->getExitEdges());
         TR::CFGEdge *edge;
         for (edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
            {
            if (edge == succeedingEdge)
               {
               isExitEdge = true;
               break;
               }
            }

         if ((!isExitEdge) && pendingList->get(succStructure->getNumber()))
            {
            _nodesInCycle->empty();
            _analysisStack.push(succNode);
            }
         }
      }
   }


void TR_LoopTransformer::createWhileLoopsList(TR_ScratchList<TR_Structure>* whileLoops)
   {
   TR_ScratchList<TR::CFGNode> newEmptyBlocks(trMemory());
   TR_ScratchList<TR::CFGNode> newEmptyExceptionBlocks(trMemory());

   // printTrees();

   // Recursive call through the structure tree that would
   // collect all the While Structures (if any) in the method.
   // Note that only while loops with a single block as its
   // header will be detected here.
   //
   ListAppender<TR_Structure> whileLoopsInnerFirst(whileLoops);
   TR_ScratchList<TR_Structure> doWhileLoops(trMemory());
   ListAppender<TR_Structure> doWhileLoopsInnerFirst(&doWhileLoops);
   //comp()->incVisitCount();
   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure();
   _nodesInCycle = new (trStackMemory()) TR_BitVector(_cfg->getNextNodeNumber(), trMemory(), stackAlloc);

   if (asLoopVersioner())
      detectWhileLoops(whileLoopsInnerFirst, *whileLoops, doWhileLoopsInnerFirst, doWhileLoops, _rootStructure, false);
   else
      detectWhileLoops(whileLoopsInnerFirst, *whileLoops, doWhileLoopsInnerFirst, doWhileLoops, _rootStructure, true);

   /*
   ListIterator<TR_Structure> whileLoopsIt(whileLoops);
   TR_Structure *nextWhileLoop;
   for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt.getNext())
      {
      dumpOptDetails(comp(), "Loop is %d\n", nextWhileLoop->getNumber());
      }

   ListIterator<TR_Structure> doWhileLoopsIt(&doWhileLoops);
   TR_Structure *nextDoWhileLoop;
   for (nextDoWhileLoop = doWhileLoopsIt.getFirst(); nextDoWhileLoop != NULL; nextDoWhileLoop = doWhileLoopsIt.getNext())
      {
      dumpOptDetails(comp(), "Do While Loop is %d\n", nextDoWhileLoop->getNumber());
      }
   */

   // _startOfHeader originally points to the last tree in the method;
   // this variable is used as a placeholder to keep track of where
   // trees can be moved (sometimes required for efficiency purposes
   // to minimize gotos in the critical 'hot' path between the loop body
   // and the new duplicate header that drives the new Do While loop.
   //
   _startOfHeader = comp()->getMethodSymbol()->getLastTreeTop();
   _whileIndex = 0;
   _counter = 0;

   }

/**
 * Detect while loops in the structure control tree
 */
void TR_LoopTransformer::detectWhileLoops(ListAppender<TR_Structure> &whileLoopsInnerFirst, List<TR_Structure> &whileLoops, ListAppender<TR_Structure> &doWhileLoopsInnerFirst, List<TR_Structure> &doWhileLoops, TR_Structure *root, bool innerFirst)
   {
   TR_RegionStructure *region = root->asRegion();
   if (!region)
      return;

   // Detect while loops in the subnodes
   //
   TR_StructureSubGraphNode *node;
   int32_t numSubNodes = 0;
   int32_t numberOfNodes = comp()->getFlowGraph()->getNextNodeNumber();
   TR_BitVector *pendingList = new (trStackMemory()) TR_BitVector(numberOfNodes, trMemory(), stackAlloc);
   pendingList->setAll(numberOfNodes);
   vcount_t visitCount = comp()->getVisitCount();

   //if (debug("nonRecursiveBVA"))
      {
      _analysisStack.push(region->getEntry());
      detectWhileLoopsInSubnodesInOrder(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, region, visitCount, pendingList, innerFirst);
      }
      /*
   else
      detectWhileLoopsInSubnodesInOrder(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, region->getEntry()->getStructure(), region->getEntry(), region, visitCount, pendingList, innerFirst);
      */

   TR_RegionStructure::Cursor si(*region);
   for (node = si.getCurrent(); node; node = si.getNext())
      numSubNodes++;

   if (!region->isNaturalLoop())
      return;

   /*
   // The loop must not have exception successors
   //
   if (region->getEntry()->hasExceptionOutEdges())
      return;
   */

   // The header of the loop must be a block.
   //
   TR_StructureSubGraphNode *entryNode = region->getEntry();

   TR_BlockStructure *loopHeader = entryNode->getStructure()->asBlock();

   bool isDoWhileLoop = false;
   bool isWhileLoop = false;
   if (asLoopCanonicalizer())
      {
      // There must be more than one subnode in the loop
      //
      /////if (numSubNodes <= 1)
      /////   return;
      /////
      if ((loopHeader && loopHeader->getBlock()->getNextBlock() && loopHeader->getBlock()->getNextBlock()->isExtensionOfPreviousBlock()) ||
          (loopHeader && loopHeader->getBlock()->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets()) ||
          (loopHeader && loopHeader->getBlock()->getLastRealTreeTop()->getNode()->getOpCode().isCompBranchOnly()))
         {
         isDoWhileLoop = true;
         }
      if (!isDoWhileLoop && loopHeader && (numSubNodes > 1))
         {
         // There must be no exception edges into or out of the header block.
         //
         TR::Block *hdrBlock = loopHeader->getBlock();
         if ((hdrBlock->hasExceptionPredecessors() == false) &&
             (hdrBlock->hasExceptionSuccessors() == false))
            {
            // There must be two edges out of the header, one internal and one external.
            //
            if (entryNode->getSuccessors().size() == 2)
               {
               auto it = entryNode->getSuccessors().begin();
               TR::CFGEdge* succ1 = *it;
               ++it;
               TR::CFGEdge* succ2 = *it;
               bool succ1Internal = region->contains(toStructureSubGraphNode(succ1->getTo())->getStructure(), region->getParent());
               bool succ2Internal = region->contains(toStructureSubGraphNode(succ2->getTo())->getStructure(), region->getParent());
               if (succ1Internal ^ succ2Internal)
                  {
                  TR::TreeTop *firstTree = hdrBlock->getFirstRealTreeTop();
                  static const bool skipTt = feGetEnv("TR_canonicalizerStopAtTreetop") == NULL;
                  while ((firstTree &&
                          (firstTree->getNode()->getOpCodeValue() == TR::asynccheck ||
                           (skipTt && firstTree->getNode()->getOpCodeValue() == TR::treetop) ||
                           firstTree->getNode()->getOpCode().isCheck())))
                    firstTree = firstTree->getNextTreeTop();

                  if (firstTree == hdrBlock->getLastRealTreeTop() ||
                      firstTree->getNextTreeTop() == hdrBlock->getLastRealTreeTop())
                     {
                     TR_ScratchList<TR::Block> exitBlocks(trMemory());
                     region->collectExitBlocks(&exitBlocks);

                     bool doNotConsiderAsWhile = false;
                     if (!exitBlocks.isSingleton())
                        {
                        TR::TreeTop *hdrBlockLoopTestTree = hdrBlock->getLastRealTreeTop();
                        bool foundInductionVariable =
                           findMatchingIVInRegion(hdrBlockLoopTestTree, region);
                        if (!foundInductionVariable)
                           {
                           ListIterator<TR::Block> blocksIt(&exitBlocks);
                           TR::Block *exitBlock;
                           for (exitBlock = blocksIt.getCurrent(); exitBlock; exitBlock = blocksIt.getNext())
                              {
                              if (exitBlock != hdrBlock)
                                 {
                                 if (exitBlock->getSuccessors().size() == 2)
                                    {
                                    auto it = exitBlock->getSuccessors().begin();
                                    TR::CFGEdge* exitSucc1 = *it;
                                    ++it;
                                    TR::CFGEdge* exitSucc2 = *it;
                                    bool successor1Internal = region->contains(toBlock(exitSucc1->getTo())->getStructureOf(), region->getParent());
                                    bool successor2Internal = region->contains(toBlock(exitSucc2->getTo())->getStructureOf(), region->getParent());
                                    if (successor1Internal ^ successor2Internal)
                                       {
                                       TR::TreeTop *exitBlockLoopTestTree = exitBlock->getLastRealTreeTop();
                                       if (exitBlockLoopTestTree->getNode()->getOpCode().isCompBranchOnly() ||
                                          exitBlockLoopTestTree->getNode()->getOpCode().isJumpWithMultipleTargets()) continue;

                                       foundInductionVariable =
                                          findMatchingIVInRegion(exitBlockLoopTestTree, region);
                                       }
                                    }
                                 }

                              if (foundInductionVariable)
                                 {
                                 doNotConsiderAsWhile = true;
                                 break;
                                 }
                              }
                           }
                        }

                     if (!doNotConsiderAsWhile)
                        isWhileLoop = true;
                     else
                        {
                        //printf("Found a fake while loop in %s\n", comp()->signature());
                        //fflush(stdout);
                        }
                     }
                  }
               }
            }
         }

      if (!isWhileLoop)
         {
         TR_StructureSubGraphNode *blockNode = entryNode;
         while (!blockNode->getStructure()->asBlock())
            blockNode = blockNode->getStructure()->asRegion()->getEntry();

         /*
         // There must be no exception edges into or out of the header block.
         //
         TR::Block *hdrBlock = blockNode->getStructure()->asBlock()->getBlock();
         if (!(hdrBlock->getExceptionPredecessors().empty() &&
               hdrBlock->getExceptionSuccessors().empty()))
           return;
         */

         isDoWhileLoop = true;
         }
      }
   else if (asLoopVersioner())
      {
      if ((loopHeader && loopHeader->getBlock()->getLastRealTreeTop()->getNode()->getOpCode().isJumpWithMultipleTargets()) ||
          (loopHeader && loopHeader->getBlock()->getLastRealTreeTop()->getNode()->getOpCode().isCompBranchOnly()))
         return;

      TR_RegionStructure *parentStructure = region->getParent()->asRegion();
      if (parentStructure)
         {
         TR_RegionStructure::Cursor si(*parentStructure);
         TR_StructureSubGraphNode *subNode;
         for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
            {
            TR_Structure *subStruct = subNode->getStructure();
            if (subStruct == region)
               {
                   /*
               if (!(subNode->getExceptionPredecessors().empty() &&
                     subNode->getExceptionSuccessors().empty()))
                   return;
                   */

               if (subNode->getPredecessors().empty())
                  return;

               TR_BlockStructure *pred = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom())->getStructure()->asBlock();
               if (!pred ||
                   !pred->isLoopInvariantBlock())
                   return;

               // This condition is required as we cannot create new catch blocks easily;
               // a catch block normally has some specific info (handler info etc.) that are
               // created by IL generator and if we try to create new catch blocks, then
               // there was a crash when generating exception table entries.
               //
               TR_ScratchList<TR::Block> blocksInRegion(trMemory());
               region->getBlocks(&blocksInRegion);
               ListIterator<TR::Block> blocksIt(&blocksInRegion);
               TR::Block *nextBlock;
               for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
                  if (!nextBlock->getExceptionPredecessors().empty())
                     return;

               break;
               }
            }
         }

      //
      //ListIterator<TR::CFGEdge> preds(&entryNode->getPredecessors());
      //TR::CFGEdge *pred1 = preds.getCurrent();
      //TR::CFGEdge *pred2 = preds.getNext();
      //if (!pred1 || !pred2 || preds.getNext())
      //   return;

      //bool pred1Internal = region->contains(toStructureSubGraphNode(pred1->getFrom())->getStructure(), region->getParent());
      //bool pred2Internal = region->contains(toStructureSubGraphNode(pred2->getFrom())->getStructure(), region->getParent());
      //if (!(pred1Internal ^ pred2Internal))
      //   return;
      }


   // This has satisfied all the criteria for a while loop that can be
   // transformed.
   //
   if (isDoWhileLoop)
      {
      if (trace())
          traceMsg(comp(), "Adding structure %d(%p) as a doWhile loop\n", region->getNumber(), region);
      if (innerFirst)
         doWhileLoopsInnerFirst.add(region);
      else
         doWhileLoops.add(region);
      }
   else
      {
      if (trace())
          traceMsg(comp(), "Adding structure %d(%p) as a While loop\n", region->getNumber(), region);

      if (innerFirst)
         whileLoopsInnerFirst.add(region);
      else
         whileLoops.add(region);
      }
   }



bool TR_LoopTransformer::findMatchingIVInRegion(TR::TreeTop* loopTestTree, TR_RegionStructure* region)
   {
   TR::Node *firstChild = loopTestTree->getNode()->getFirstChild();
   TR::SymbolReference *firstChildSymRef = NULL;
   if (firstChild->getOpCode().hasSymbolReference())
      firstChildSymRef = firstChild->getSymbolReference();
   else
      {
      if ((firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub()) &&
         (firstChild->getReferenceCount() > 1) &&
         firstChild->getSecondChild()->getOpCode().isLoadConst())
         {
         firstChild = firstChild->getFirstChild();
         }

      if (firstChild &&
         firstChild->getOpCode().hasSymbolReference())
         firstChildSymRef = firstChild->getSymbolReference();
      }
   if (!firstChildSymRef)
      return false;

   if (region->findMatchingIV(firstChildSymRef))
      return true;

   TR_PrimaryInductionVariable *piv = region->getPrimaryInductionVariable();
   if (piv && piv->getSymRef()->getSymbol() == firstChildSymRef->getSymbol())
      return true;
   ListIterator<TR_BasicInductionVariable> it(&region->getBasicInductionVariables());
   for (TR_BasicInductionVariable *biv = it.getFirst(); biv; biv = it.getNext())
      if (biv->getSymRef()->getSymbol() == firstChildSymRef->getSymbol())
         return true;
   return false;
   }


/**
 * @verbatim
 * The loop is changed from this:      to this:
 *             |                             |
 *             |                             |
 *         --->|                             |
 *        |    |                             |
 *        |  -----       -----             -----       -----
 *        | |  H  |---->|  E  |           |  H  |---->|  S1 |------
 *        |  -----       -----             -----       -----       |
 *        |    |                             |                     |
 *        |    |                           -----                   |
 *        |  -----                        |  S2 |                  |
 *        | |  B  |                        -----                   |
 *        |  -----                      ---->|                     |
 *        |    |                       |   -----                   |
 *         ----                        |  |  B  |                  |
 *                                     |   -----                   |
 *                                     |     |                     V
 *                                     |   -----       -----     -----
 *                                     |  |  H1 |---->|  G  |-->|  E  |
 *                                     |   -----       -----     -----
 *                                     |     |
 *                                      -----
 * @endverbatim
 * The new block H1 is a clone of the original header H
 * The new blocks S1 and S2 are empty critical-edge-splitting blocks
 * The new block G is optional, it may be necessary to contain an extra
 * goto.
 */
void TR_LoopCanonicalizer::canonicalizeNaturalLoop(TR_RegionStructure *whileLoop)
   {
   rewritePostToPreIncrementTestInRegion(whileLoop);

   if (!performTransformation(comp(), "%sCanonicalizing natural loop %d\n", OPT_DETAILS, whileLoop->getNumber()))
      return;

   if (trace())
      comp()->dumpMethodTrees("Trees at this stage");

   TR_StructureSubGraphNode *entryGraphNode = whileLoop->getEntry();
   TR_ASSERT(entryGraphNode->getStructure()->asBlock(), "Loop canonicalizer, header is not a block");

   // Find the basic blocks that represent the loop header and the
   // first block in the loop body.
   //
   TR::Block *loopHeader = entryGraphNode->getStructure()->asBlock()->getBlock();

   TR::Block *loopBody = NULL;
   for (auto nextSucc = loopHeader->getSuccessors().begin(); nextSucc != loopHeader->getSuccessors().end(); ++nextSucc)
      {
      TR::Block *dest = toBlock((*nextSucc)->getTo());
      if (whileLoop->contains(dest->getStructureOf(), whileLoop->getParent()))
         {
         loopBody = dest;
         break;
         }
      }

   TR_ScratchList<TR::Block> newBlocks(trMemory());
   TR_ScratchList<TR::CFGEdge> removedEdges(trMemory());

   TR::TreeTop *entryTree = loopHeader->getEntry();
   TR::TreeTop *exitTree = loopHeader->getExit();
   TR::Node *entryNode = entryTree->getNode();
   TR::Block *fallThroughBlock;
   if (!_blocksToBeCleansed.find(loopHeader))
      fallThroughBlock = exitTree->getNextTreeTop()->getNode()->getBlock();
   else
      {
      TR_ASSERT(loopHeader->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto, "Unexpected opcode instead of TR::Goto\n");
      fallThroughBlock = loopHeader->getLastRealTreeTop()->getNode()->getBranchDestination()->getNode()->getBlock();
      }

   // Create a new empty block to hold the cloned header H1.
   // It will be filled out with duplicate trees soon .
   //

   TR::Block *clonedHeader = TR::Block::createEmptyBlock(entryNode, comp(), loopHeader->getFrequency(), loopHeader);

   bool needToSetFlag = false;
   if (loopHeader->getStructureOf()->isEntryOfShortRunningLoop())
     needToSetFlag = true;

   newBlocks.add(clonedHeader);

   TR::TreeTop *newEntryTree = clonedHeader->getEntry();
   TR::TreeTop *newExitTree = clonedHeader->getExit();
   TR::TreeTop *prevTree = entryTree->getPrevTreeTop();


   // Since we will be explicitly doing structure repair later, remove the
   // structure so that repair is not attempted as we modify the CFG.
   //
   _cfg->setStructure(NULL);

   // Start fixing up the predecessors of the original loop
   // header; i.e. make all of the back edges now point to the
   // new duplicate header block; but maintain the incoming edges to the
   // original header as the new If guarded structure must still be entered
   // through the If guard (which is where the original loop header
   // is present after canonicalization).
   //
   int32_t sumPredFreq = 0;
   TR::Block *adjPred = NULL;
   for (auto edge = loopHeader->getPredecessors().begin(); edge != loopHeader->getPredecessors().end(); ++edge)
      {
      TR::Block *pred = toBlock((*edge)->getFrom());

      if ((*edge)->getCreatedByTailRecursionElimination())
         needToSetFlag = true;

      if (!whileLoop->contains(pred->getStructureOf(), whileLoop->getParent()))
         {
         sumPredFreq = sumPredFreq + (*edge)->getFrequency();
         //
         // This is NOT a back edge as the predecessor is not part of the loop.
         // The operations here are to insert gotos at the end of this
         // predecessor block as its not certain what the optimal
         // positioning of the trees in the original header would be
         // right now. Since this control flow path is guaranteed to be
         // less 'hot' than the actual loop, we are trying to remove
         // any constraints that might keep us from moving the trees
         // in the original loop header; we might want to move trees
         // in this original header in order to specifically take advantage
         // of falling through in the 'hot' path between the loop body
         // and the new duplicate header.
         //
       if (pred->getExit())
               loopHeader->getEntry()->getNode()->copyByteCodeInfo(pred->getExit()->getNode());
         //dumpOptDetails(comp(), "Block loopHeader %d gets info from pred %d\n", loopHeader->getNumber(), pred->getNumber());
         if (!(pred == _cfg->getStart()))
            {
            TR::TreeTop *lastNonFenceTree = pred->getLastRealTreeTop();
            if (_blocksToBeCleansed.find(pred))
               lastNonFenceTree = lastNonFenceTree->getPrevRealTreeTop();
            TR::Node * lastNonFenceNode = lastNonFenceTree->getNode();
            if (lastNonFenceTree->getNode()->getOpCode().isBranch())
               {
               // If the predecessor had a branch but was still falling
               // through rather than branching explicitly to the original
               // header. Note that a switch always has explicit branches;
               // so it cannot be falling through to the original header.
               //
               if (lastNonFenceTree->getNode()->getBranchDestination() != entryTree)
                  {
                  // We add a goto at the end of a block already having
                  // a branch; so we MUST fix this up by moving trees
                  // at the end. The reasoning here is that in general in
                  // a nested while loop we might have the headers
                  // falling through to each other and the body falling
                  // through to the innermost header. In such cases we
                  // want our canonicalized nested do whiles to also
                  // utilize maximum fall through like originally but
                  // with the new duplicate headers now. So two or more
                  // original header blocks (their trees) would need to be
                  // moved out of the 'hot' path first and then at the end,
                  // we patch up the trees properly and get rid of this
                  // goto we have inserted that breaks CFG constraints
                  // temporarily.
                  //
                  TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(entryNode, TR::Goto, 0, entryTree));
                  TR::TreeTop *treeTop = lastNonFenceTree->getNextTreeTop();
                  lastNonFenceTree->join(gotoTreeTop);
                  gotoTreeTop->join(treeTop);
                  _blocksToBeCleansed.add(pred);
                  }
               }
            else if (!lastNonFenceTree->getNode()->getOpCode().isJumpWithMultipleTargets() &&
                     !lastNonFenceTree->getNode()->getOpCode().isCompBranchOnly())
               {
               if (adjPred)
                  {
                  // If the predecessor used to fall through into the
                  // original header (no branch or switch)
                  //
                  TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(entryNode, TR::Goto, 0, entryTree));
                  TR::TreeTop *treeTop = lastNonFenceTree->getNextTreeTop();
                  lastNonFenceTree->join(gotoTreeTop);
                  gotoTreeTop->join(treeTop);
                  }
               else
                  {
                  adjPred = pred;
                  }
               }
            }
         }
      else
         {
         // It is a back edge to the original header; now the back edge
         // 'from' block must be made to branch to the duplicate header
         // instead.
         //
         removedEdges.add(*edge);
         TR::CFGEdge *newEdge = TR::CFGEdge::createEdge(pred,  clonedHeader, trMemory());
         _cfg->addEdge(newEdge);
         //traceMsg(comp(), "newEdge %p freq %d\n", newEdge, newEdge->getFrequency());


         if (pred != _cfg->getStart())
            {
            TR::TreeTop *lastNonFenceTree = pred->getLastRealTreeTop();
            TR::Node * lastNonFenceNode = lastNonFenceTree->getNode();
            if (_blocksToBeCleansed.find(pred))
               lastNonFenceTree = lastNonFenceTree->getPrevRealTreeTop();

            if (lastNonFenceTree->getNode()->getOpCode().isBranch() || lastNonFenceTree->getNode()->getOpCode().isJumpWithMultipleTargets())
               {
               // Insert a goto if required as the positioining of neither
               // the original header nor the duplicate header is fixed yet.
               // Reasoning similar to mentioned above in the NOT back edge
               // case.
               //
               if (!lastNonFenceTree->adjustBranchOrSwitchTreeTop(comp(), entryTree, newEntryTree))
                  {
                  TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(entryNode, TR::Goto, 0, newEntryTree));
                  TR::TreeTop *treeTop = lastNonFenceTree->getNextTreeTop();
                  lastNonFenceTree->join(gotoTreeTop);
                  gotoTreeTop->join(treeTop);
                  _blocksToBeCleansed.add(pred);
                  }
               }
            else
               {
               TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(entryNode, TR::Goto, 0, newEntryTree));
               TR::TreeTop *treeTop = lastNonFenceTree->getNextTreeTop();
               lastNonFenceTree->join(gotoTreeTop);
               gotoTreeTop->join(treeTop);
               }
            }
         }
      }

   //clonedHeader->setFrequency(sumPredFreq);
   int32_t clonedfreq = clonedHeader->getFrequency() - sumPredFreq;
   if (clonedfreq < 0)
      {
      if (loopHeader->isCold())
         clonedfreq = loopHeader->getFrequency();
      else
         clonedfreq = MAX_COLD_BLOCK_COUNT + 1;
      }

   clonedHeader->setFrequency(clonedfreq);

   // Now there is no dependence on fall throughs for either
   // the original header or the duplicate header; so we can now place
   // them freely wherever we determine to be optimal and then fix it up
   // at the end once we are done canonicalizing all loops.
   //
   // First place the new duplicate header trees immediately AFTER
   // the original header trees. This would already put this
   // duplicate header trees in the correct place if the
   // original header was falling through to the loop body.
   //
   TR::TreeTop *tempTree = exitTree->getNextTreeTop();
   exitTree->join(newEntryTree);
   newExitTree->join(tempTree);

   // If the tree succeeding the headers does not belong to the loop
   // body then it means that either the loop body used to fall through
   // into the original header or neither of them fell through into each
   // other. In either case its a good idea to flip the order of our
   // header blocks now putting the duplicate header trees BEFORE the
   // original header trees. Note that at this time we also move
   // the original header to the end of the method.
   //
   if (((tempTree != NULL) && (tempTree->getNode()->getBlock() != loopBody)) && (entryTree != comp()->getStartTree()))
      {
      TR::TreeTop *nextTreeAfterHeader = _startOfHeader->getNextTreeTop();
      if (prevTree != NULL)
         {
         prevTree->join(newEntryTree);
         }
      else
         comp()->setStartTree(newEntryTree);

      if (adjPred)
	 {
	 TR::TreeTop *predExit = adjPred->getExit();
         TR::TreeTop *predNext = predExit->getNextTreeTop();
         predExit->join(entryTree);
         exitTree->join(predNext);
         //printf("Changing layout in %s\n", comp()->signature());
         }
      else
	 {
         _startOfHeader->join(entryTree);
         exitTree->join(nextTreeAfterHeader);
	 }
      }


   // Duplicate the trees in the original header and insert them into
   // the new duplicate header. Note that the parent/child links for
   // multiply referenced nodes are also the same as in the original
   // block.
   //
   TR::TreeTop *currentTree = entryTree->getNextTreeTop();
   TR::TreeTop *newPrevTree = newEntryTree;
   TR_ScratchList<TR::Node> seenNodes(trMemory()), duplicateNodes(trMemory());
   vcount_t visitCount = comp()->incVisitCount();
   while (!(currentTree == exitTree))
      {
      TR::Node *currentNode = currentTree->getNode();
      TR::ILOpCode currentOpCode = currentNode->getOpCode();

      TR::Node *newCurrentNode = TR::Node::copy(currentTree->getNode());
      currentTree->getNode()->setVisitCount(visitCount);
      duplicateNodes.add(newCurrentNode);
      seenNodes.add(currentTree->getNode());

      for (int32_t childNum=0;childNum < currentTree->getNode()->getNumChildren(); childNum++)
         {
         TR::Node *duplicateChildNode = duplicateExact(currentTree->getNode()->getChild(childNum), &seenNodes, &duplicateNodes);
         newCurrentNode->setChild(childNum, duplicateChildNode);

         /////if (trace())
         /////   traceMsg(comp(), "Cloned a treetop \n");
         }

      TR::TreeTop *newCurrentTree = TR::TreeTop::create(comp(), newCurrentNode, NULL, NULL);
      newCurrentTree->join(newExitTree);
      newPrevTree->join(newCurrentTree);

      currentTree = currentTree->getNextTreeTop();
      newPrevTree = newCurrentTree;
      /////if (trace())
      /////   newCurrentTree->print(comp()->getOutFile());
      }



   // If the duplicate header falls through to the loop body
   // then remove that dependence on the ordering of trees as well.
   // Make the duplicate header branch explicitly to the loop body
   // (without introducing a goto); we reverse the check in the
   // duplicate header.
   //
   bool reversedBranch = false;
   /*
   if (newExitTree->getNextTreeTop() == loopBody->getEntry())
      {
      TR::TreeTop *lastNonFenceTree = clonedHeader->getLastRealTreeTop();
      if (lastNonFenceTree->getNode()->getOpCode().isBranch())
         {
         reversedBranch = true;
         TR::Node *cmpNode = lastNonFenceTree->getNode();
         cmpNode->reverseBranch(loopBody->getEntry());
         }
      }
   */

   // The resulting structure will be different depending on
   // whether the branch has been reversed or NOT. We need
   // an extra empty block if the branch was reversed, to redirect
   // to the old branch destination (which was no longer the destination
   // because of reversing the branch) through an explicit goto block.
   // extraGoto may or may not be created based on reversing.
   // splitter1 and splitter2 are critical edge splitting blocks.
   //
   loopHeader->setFrequency(sumPredFreq);
   TR::Block *splitter1 = TR::Block::createEmptyBlock(loopHeader->getEntry()->getNode(), comp(), sumPredFreq < 0 ? 0 : sumPredFreq, loopHeader);
   TR::Block *splitter2 = TR::Block::createEmptyBlock(loopHeader->getEntry()->getNode(), comp(), sumPredFreq < 0 ? 0 : sumPredFreq, loopHeader);
   TR::Block *extraGoto = NULL;
   if (reversedBranch)
      {
      extraGoto = TR::Block::createEmptyBlock(loopHeader->getEntry()->getNode(), comp(), sumPredFreq < 0 ? 0 : sumPredFreq, loopHeader);
      }


   newBlocks.add(splitter1);
   newBlocks.add(splitter2);
   if (reversedBranch)
      newBlocks.add(extraGoto);

   TR::TreeTop *newEntryTree1 = splitter1->getEntry();
   TR::TreeTop *newExitTree1 = splitter1->getExit();
   TR::TreeTop *newEntryTree2 = splitter2->getEntry();
   TR::TreeTop *newExitTree2 = splitter2->getExit();
   TR::TreeTop *newEntryTree4 = NULL;
   TR::TreeTop *newExitTree4 = NULL;
   if (reversedBranch)
      {
      newEntryTree4 = extraGoto->getEntry();
      newExitTree4 = extraGoto->getExit();
      }

   TR::Node *gotoNode1 = TR::Node::create(entryNode, TR::Goto);
   TR::TreeTop *gotoTree1 = TR::TreeTop::create(comp(), gotoNode1, NULL, NULL);
   TR::Node *gotoNode2 = TR::Node::create(entryNode, TR::Goto);
   TR::TreeTop *gotoTree2 = TR::TreeTop::create(comp(), gotoNode2, NULL, NULL);
   TR::Node *gotoNode4;
   TR::TreeTop *gotoTree4;

   if (reversedBranch)
      {
      gotoNode4 = TR::Node::create(entryNode, TR::Goto);
      gotoTree4 = TR::TreeTop::create(comp(), gotoNode4, NULL, NULL);
      }

   newEntryTree1->join(gotoTree1);
   gotoTree1->join(newExitTree1);

   newEntryTree2->join(gotoTree2);
   gotoTree2->join(newExitTree2);

   if (reversedBranch)
      {
      TR::TreeTop *treeTop1 = newExitTree->getNextTreeTop();
      newExitTree->join(newEntryTree4);
      newExitTree4->join(treeTop1);
      newEntryTree4->join(gotoTree4);
      gotoTree4->join(newExitTree4);
      }

   // The order in which these two empty blocks are inserted after
   // the original loop header id dependent on the fall through/destination
   // of the original loop header. These two blocks are always
   // inserted after the original loop header. Note that the end
   // of this set of trees becomes the new startHeader (place to add any
   // original header trees of subsequent loops).
   //
   if (loopBody == fallThroughBlock)
      {
      newExitTree2->join(newEntryTree1);
      newExitTree1->join(exitTree->getNextTreeTop());
      exitTree->join(newEntryTree2);
      if (_startOfHeader->getNextTreeTop())
         _startOfHeader = newExitTree1;
      }
   else
      {
      newExitTree1->join(newEntryTree2);
      newExitTree2->join(exitTree->getNextTreeTop());
      exitTree->join(newEntryTree1);
      if (_startOfHeader->getNextTreeTop())
         _startOfHeader = newExitTree2;
      }

   // Now start adjusting the CFG properly (add/remove edges)
   //
   TR::Block *joinBlock = NULL;
   TR::Block *predBlock = NULL;
   if (reversedBranch)
      {
      // Need to add an extra edge if extraGoto was created
      //
      TR::CFGEdge *newEdge = TR::CFGEdge::createEdge(clonedHeader,  extraGoto, trMemory());
      _cfg->addEdge(newEdge);
      predBlock = extraGoto;
      }
   else
      predBlock = clonedHeader;

   // Remove successor edges from the original loop header as its going
   // to only have the two blocks created for splitting critical edges
   // as its successors. Also add some duplicate edges.
   //
   TR::CFGEdge *newEdge;
   for (auto nextEdge = loopHeader->getSuccessors().begin(); nextEdge != loopHeader->getSuccessors().end(); ++nextEdge)
      {
      TR::Block *dest = toBlock((*nextEdge)->getTo());
      removedEdges.add(*nextEdge);
      if (dest == loopBody)
         {
         // This edge is to the loop body
         newEdge = TR::CFGEdge::createEdge(clonedHeader,  dest, trMemory());
         }
      else
         {
         // This edge is to the join block
         joinBlock = dest;
         newEdge = TR::CFGEdge::createEdge(predBlock,  dest, trMemory());
         }
      _cfg->addEdge(newEdge);
      }

   uint32_t loopHeaderFrequency = loopHeader->getFrequency();
   uint32_t loopBodyFrequency = loopBody->getFrequency();
   uint32_t exitBlockFrequency = joinBlock->getFrequency();

   uint32_t newLoopHeaderFrequency = sumPredFreq + exitBlockFrequency;
   if (newLoopHeaderFrequency > 10000)
      newLoopHeaderFrequency = 10000;

   if (trace())
      {
      traceMsg(comp(), "Setting frequency of peeled iteration block_%d to %d, s1 block_%d to %d, s2 block_%d to %d\n",
               loopHeader->getNumber(), newLoopHeaderFrequency, splitter1->getNumber(), exitBlockFrequency, splitter2->getNumber(), sumPredFreq);
      }

   loopHeader->setFrequency(newLoopHeaderFrequency);
   splitter2->setFrequency(sumPredFreq);
   if (loopBody->isCold())
      {
      traceMsg(comp(), "Setting s2 block_%d cold because loop body is cold\n", splitter2->getNumber());
      splitter2->setIsCold(true);
      }

   splitter1->setFrequency(exitBlockFrequency);
   if (joinBlock->isCold())
      {
      traceMsg(comp(), "Setting s1 block_%d cold because join block is cold\n", splitter1->getNumber());
      splitter2->setIsCold(true);
      }

   gotoTree1->getNode()->setBranchDestination(joinBlock->getEntry());
   gotoTree2->getNode()->setBranchDestination(loopBody->getEntry());
   if (reversedBranch)
      gotoTree4->getNode()->setBranchDestination(joinBlock->getEntry());

   TR::TreeTop *lastNonFenceTreeInHdr    = loopHeader->getLastRealTreeTop();
   /////TR::TreeTop *lastNonFenceTreeInDupHdr = clonedHeader->getLastRealTreeTop();

   if (_blocksToBeCleansed.find(loopHeader))
      {
      TR::TreeTop *prevNonFenceTreeInHdr = lastNonFenceTreeInHdr->getPrevRealTreeTop();
      prevNonFenceTreeInHdr->join(loopHeader->getExit());
      lastNonFenceTreeInHdr = prevNonFenceTreeInHdr;
      _blocksToBeCleansed.remove(loopHeader);
      _blocksToBeCleansed.add(clonedHeader);
      }


   if (fallThroughBlock == loopBody)
      {
      lastNonFenceTreeInHdr->getNode()->setBranchDestination(splitter1->getEntry());
      }
   else
      {
      lastNonFenceTreeInHdr->getNode()->setBranchDestination(splitter2->getEntry());
      }

   _cfg->addEdge(TR::CFGEdge::createEdge(loopHeader,  splitter1, trMemory()));
   _cfg->addEdge(TR::CFGEdge::createEdge(splitter1,  joinBlock, trMemory()));
   _cfg->addEdge(TR::CFGEdge::createEdge(loopHeader,  splitter2, trMemory()));
   _cfg->addEdge(TR::CFGEdge::createEdge(splitter2,  loopBody, trMemory()));

   //splitter1->setIsCold();
   //splitter1->setFrequency(VERSIONED_COLD_BLOCK_COUNT);

   ListIterator<TR::CFGEdge> removedEdgesIt(&removedEdges);
   for (TR::CFGEdge* nextEdge = removedEdgesIt.getFirst(); nextEdge != NULL; nextEdge = removedEdgesIt.getNext())
      {
      _cfg->removeEdge(nextEdge);
      }

   // Add the newly created CFG blocks to the CFG
   //
   ListIterator<TR::Block> newBlocksIt(&newBlocks);
   TR::Block *nextBlock;
   for (nextBlock = newBlocksIt.getFirst(); nextBlock; nextBlock = newBlocksIt.getNext())
      {
      adjustTreesInBlock(nextBlock);
      _cfg->addNode(nextBlock);
      }


   // Dump block info now that the block numbers have been assigned
   //
   if (trace())
      {
      traceMsg(comp(), "Cloning header block : %d(%p) as block_%d(%p) for canonicalization\n", loopHeader->getNumber(), loopHeader, clonedHeader->getNumber(), clonedHeader);
      traceMsg(comp(), "Creating splitter blocks %d and %d\n", splitter1->getNumber(), splitter2->getNumber());
      if (reversedBranch)
         traceMsg(comp(), "Creating extra goto block_%d\n", extraGoto->getNumber());
      }

   // Now start adjusting the structure properly; since we are changing structure
   // in a very specific manner, we didn't use structure repair.
   //
   _cfg->setStructure(_rootStructure);

   // The original natural loop is changed so that the original header is
   // replaced by the cloned header, and the first structure of the body becomes
   // the header of the natural loop (there is still a back-edge from the cloned
   // header to the body.
   //
   TR_StructureSubGraphNode *bodyNode   = NULL;
   int32_t                   joinNumber = -1;
   for (auto edge = entryGraphNode->getSuccessors().begin(); edge != entryGraphNode->getSuccessors().end(); ++edge)
      {
      if ((*edge)->getTo()->getNumber() == loopBody->getNumber())
         bodyNode = toStructureSubGraphNode((*edge)->getTo());
      else
         joinNumber = (*edge)->getTo()->getNumber();
      }
   TR_ASSERT(joinNumber >= 0, "No edge to join node found");

   // A new proper acyclic structure is created to contain the original header,
   // the splitter blocks, and the extra goto block if it was created. This
   // new region replaces the original natural loop in its parent.
   //
   TR_Structure *parentStructure = whileLoop->getParent();
   TR_RegionStructure *properRegion = new (_cfg->structureRegion()) TR_RegionStructure(comp(), loopHeader->getNumber());
   properRegion->setAsCanonicalizedLoop(true);
   parentStructure->replacePart(whileLoop, properRegion);
   whileLoop->setParent(properRegion);

   whileLoop->setEntry(bodyNode);
   whileLoop->setNumber(loopBody->getNumber());
   TR_BlockStructure *clonedHdrBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), clonedHeader->getNumber(), clonedHeader);
   if (needToSetFlag)
     {
     //clonedHdrBlockStructure->setIsEntryOfShortRunningLoop();
     TR_RegionStructure *bodyStructure = bodyNode->getStructure()->asRegion();
     if (bodyStructure)
        {
        TR::Block *entryBlock = bodyStructure->getEntryBlock();
        entryBlock->getStructureOf()->setIsEntryOfShortRunningLoop();
        }
     else
        bodyNode->getStructure()->asBlock()->setIsEntryOfShortRunningLoop();
     if (trace())
        traceMsg(comp(), "Marked block %p\n", bodyNode->getNumber());
     }

   clonedHdrBlockStructure->setWasHeaderOfCanonicalizedLoop(true);
   whileLoop->replacePart(entryGraphNode->getStructure(), clonedHdrBlockStructure);

   TR_StructureSubGraphNode *hdrNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(loopHeader->getStructureOf());
   properRegion->addSubNode(hdrNode);
   properRegion->setEntry(hdrNode);

   TR_StructureSubGraphNode *node = new (_cfg->structureRegion()) TR_StructureSubGraphNode(new (_cfg->structureRegion()) TR_BlockStructure(comp(), splitter2->getNumber(), splitter2));
   node->getStructure()->asBlock()->setAsLoopInvariantBlock(true);
   _invariantBlocks.add(splitter2);
   properRegion->addSubNode(node);
   TR::CFGEdge::createEdge(hdrNode, node, trMemory());

   TR_StructureSubGraphNode *loopNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(whileLoop);
   properRegion->addSubNode(loopNode);
   TR::CFGEdge::createEdge(node, loopNode, trMemory());

   node = new (_cfg->structureRegion()) TR_StructureSubGraphNode(new (_cfg->structureRegion()) TR_BlockStructure(comp(), splitter1->getNumber(), splitter1));
   properRegion->addSubNode(node);
   TR::CFGEdge::createEdge(hdrNode, node, trMemory());

   properRegion->addExitEdge(node, joinNumber);

   // All exit edges from the while loop except the one to the join node are
   // also exit edges from the new proper region
   //
   TR_BitVector seenExitNodes(_cfg->getNextNodeNumber(), trMemory(), stackAlloc);
   ListIterator<TR::CFGEdge> succ;
   succ.set(&whileLoop->getExitEdges());
   for (TR::CFGEdge* edge = succ.getCurrent(); edge; edge = succ.getNext())
      {
      int32_t exitNumber = edge->getTo()->getNumber();

      if (!seenExitNodes.get(exitNumber))
         {
         if (exitNumber != joinNumber)
            {
            seenExitNodes.set(exitNumber);
            bool isExceptionEdge = false;
            if (edge->getFrom()->hasExceptionSuccessor(edge->getTo()))
               isExceptionEdge = true;
            properRegion->addExitEdge(loopNode, exitNumber, isExceptionEdge);
            }
         }
      }

   if (reversedBranch)
      {
      bool naturalLoopStillExitsToJoinNode = false;
      TR_BlockStructure *extraGotoStruct = new (_cfg->structureRegion()) TR_BlockStructure(comp(), extraGoto->getNumber(), extraGoto);
      node = new (_cfg->structureRegion()) TR_StructureSubGraphNode(extraGotoStruct);
      properRegion->addSubNode(node);
      TR::CFGEdge::createEdge(loopNode, node, trMemory());

      // The exit edge from the original while loop now goes to the extra goto
      // block.
      //
      ListIterator<TR::CFGEdge> ei(&whileLoop->getExitEdges());
      TR::CFGEdge *edge;
      for (edge = ei.getCurrent(); edge; edge = ei.getNext())
         {
         TR_StructureSubGraphNode *fromNode = toStructureSubGraphNode(edge->getFrom());
         int32_t toNum = toStructureSubGraphNode(edge->getTo())->getNumber();

         if (toNum == joinNumber)
            {
            if (fromNode->getStructure() == clonedHdrBlockStructure)
               {
               edge->setTo(TR_StructureSubGraphNode::create(node->getNumber(), whileLoop));     //new (_cfg->structureRegion()) TR_StructureSubGraphNode(node->getNumber()));
               }
            else
               naturalLoopStillExitsToJoinNode = true;
            }
         }

      // If the natural loop has exit edges other than the one from the cloned
      // header to the join node (e.g. for break edges) the parent graph needs to
      // show that the natural loop exits to the join node.
      //
      if (naturalLoopStillExitsToJoinNode)
        {
        properRegion->addExitEdge(loopNode, joinNumber);
        }
      }
   else
      node = loopNode;

   properRegion->addExitEdge(node, joinNumber);

   if (trace())
      {
      traceMsg(comp(), "Structure after canonicalizing natural loop %d:\n", properRegion->getNumber());
      getDebug()->print(comp()->getOutFile(), _rootStructure, 6);
      getDebug()->print(comp()->getOutFile(), comp()->getFlowGraph());
      comp()->dumpMethodTrees("Trees at this stage");
      }
   }

bool TR_LoopCanonicalizer::isLegalToSplitEdges(TR_RegionStructure *doWhileLoop, TR::Block *blockAtHeadOfLoop)
   {
   // checking legality before loopInvariantBlock is created, so can set it to null
   int32_t sumPredFreq;
   return modifyBranchesForSplitEdges(doWhileLoop, blockAtHeadOfLoop, NULL, NULL, false, &sumPredFreq, /* isCheckOnly = */ true);
   }

/**
 * Looks at all predecessor blocks to blockAtHeadOfLoop, and if they are external to the doWhileLoop, modifies
 * any branches or direct paths on their last tree top so that their new destination is the targetBlock, instead
 * of blockAtHeadOfLoop.
 *
 * @param isCheckOnly if true (not the default) just check if legal, don't transform.
 */
bool TR_LoopCanonicalizer::modifyBranchesForSplitEdges(
      TR_RegionStructure *doWhileLoop, TR::Block *blockAtHeadOfLoop, TR::Block *loopInvariantBlock,
      TR::Block *targetBlock, bool addToEnd, int32_t *sumPredFreq, bool isCheckOnly)
   {
   TR_ASSERT((!isCheckOnly) || (isCheckOnly && (loopInvariantBlock == NULL) && (targetBlock == NULL)), "Check should be made only with loopInvariantBlock and targetBlock NULL");

   *sumPredFreq = 0;
   TR::TreeTop *blockHeadTreeTop = blockAtHeadOfLoop->getEntry();

   for (auto edge = blockAtHeadOfLoop->getPredecessors().begin(); edge != blockAtHeadOfLoop->getPredecessors().end();)
      {
      TR::Block *pred = toBlock((*edge)->getFrom());
      if ((!doWhileLoop->contains(pred->getStructureOf())) &&
          (pred != loopInvariantBlock))
         {
         *sumPredFreq = *sumPredFreq + (*edge)->getFrequency();
         if (pred != _cfg->getStart())
            {
            TR::TreeTop *lastTree = pred->getLastRealTreeTop();
            if (isCheckOnly)
               {
               if (!lastTree->isLegalToChangeBranchDestination(comp()))
                  return false;
               }
            else
               {
               if (!lastTree->adjustBranchOrSwitchTreeTop(comp(), blockHeadTreeTop, targetBlock->getEntry()))
                  {
                  if (addToEnd)
                     {
                     TR::TreeTop *predExit = pred->getExit();
                     TR::TreeTop *targetEntry = targetBlock->getEntry();
                     TR::TreeTop *targetExit = targetBlock->getExit();
                     TR::TreeTop *prevTree = targetEntry->getPrevTreeTop();
                     predExit->join(targetEntry);
                     targetExit->join(blockHeadTreeTop);
                     prevTree->setNextTreeTop(NULL);
                     if ((targetBlock->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto) &&
                           (targetBlock->getLastRealTreeTop()->getNode()->getBranchDestination() == blockHeadTreeTop))
                        targetEntry->join(targetExit);
                     }
                  }
               }
            }
         else
            {
            // it is okay to reach here if checking only, as the preparation work will not have been done
            TR_ASSERT(isCheckOnly, "Cannot reach here if preparation work for the loopInvariantBlock has been done correctly\n");
            if (!isCheckOnly)
               {
               TR::TreeTop *targetEntry = targetBlock->getEntry();
               TR::TreeTop *targetExit = targetBlock->getExit();
               TR::TreeTop *prevTree = targetEntry->getPrevTreeTop();
               targetExit->join(blockHeadTreeTop);
               prevTree->setNextTreeTop(NULL);
               comp()->setStartTree(targetEntry);
               }
            }

         if (!isCheckOnly)
            {
            _cfg->addEdge(TR::CFGEdge::createEdge(pred,  targetBlock, trMemory()));
            _cfg->removeEdge(*edge++);
            }
         else
        	 ++edge;
         }
      else
    	  ++edge;
      }
   return true;
   }

void TR_LoopCanonicalizer::canonicalizeDoWhileLoop(TR_RegionStructure *doWhileLoop)
   {
   rewritePostToPreIncrementTestInRegion(doWhileLoop);

   TR::TreeTop *endTree = comp()->getMethodSymbol()->getLastTreeTop();
   TR_RegionStructure *parentStructure = doWhileLoop->getParent()->asRegion();

   TR_StructureSubGraphNode *subNode = NULL, *doWhileNode = NULL;
   TR_Structure             *subStruct = NULL;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      subStruct = subNode->getStructure();
      if (subStruct == doWhileLoop)
         {
         doWhileNode = subNode;
         break;
         }
      }

   bool isEntry = false;
   if (parentStructure->getEntry() == doWhileNode)
      {
      isEntry = true;
      //printf("Found a do-while that is also parent's entry in method %s\n", comp()->signature());
      }

   TR_ScratchList<TR::Block> blocksInDoWhileLoop(trMemory());
   doWhileLoop->getBlocks(&blocksInDoWhileLoop);

   TR_StructureSubGraphNode *blockNode = doWhileLoop->getEntry();
   while (!blockNode->getStructure()->asBlock())
      blockNode = blockNode->getStructure()->asRegion()->getEntry();

   TR::Block *blockAtHeadOfLoop = blockNode->getStructure()->asBlock()->getBlock();

   if (!isLegalToSplitEdges(doWhileLoop, blockAtHeadOfLoop))
      return;

   if (!performTransformation(comp(), "%sCanonicalizing do while loop %d\n", OPT_DETAILS, doWhileLoop->getNumber()))
      return;

   doWhileLoop->setAsCanonicalizedLoop(true);

   TR::TreeTop *blockHeadTreeTop = blockAtHeadOfLoop->getEntry();
   TR::Node *blockHeadNode = blockHeadTreeTop->getNode();

   if (blockHeadTreeTop->getPrevTreeTop() == NULL)
      return;

   if (!comp()->getOption(TR_DisableEmptyPreHeaderCheck)) //Skip adding another pre-header if you don't need to.
      {
      for (auto iedge = blockAtHeadOfLoop->getPredecessors().begin(); iedge != blockAtHeadOfLoop->getPredecessors().end(); ++iedge)
         {
         TR::Block *ipred = toBlock((*iedge)->getFrom());
         if (ipred->getStructureOf()->isLoopInvariantBlock() && ipred->isEmptyBlock() &&
             !( ipred == ipred->getStructureOf()->getParent()->asRegion()->getEntryBlock()) )
            {
            return;     //if there is an empty pre-header which is not the entry block of a region, don't add another.
            }
         }
      }

   TR::Node *predBlockExitNode = blockAtHeadOfLoop->getPredecessors().front()->getFrom()->asBlock()->getExit()->getNode();

   TR::Block *loopInvariantBlock = TR::Block::createEmptyBlock(predBlockExitNode, comp(), blockAtHeadOfLoop->getFrequency(), blockAtHeadOfLoop);
   _cfg->addNode(loopInvariantBlock);
   TR::TreeTop *newEntry = loopInvariantBlock->getEntry();
   TR::TreeTop *newExit = loopInvariantBlock->getExit();

   TR::TreeTop *cursorTree = blockHeadTreeTop->getPrevTreeTop();

   bool addToEnd;
   if (doWhileLoop->contains(cursorTree->getNode()->getBlock()->getStructureOf()))
      {
      addToEnd = true;
      endTree->join(newEntry);
      TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(blockHeadNode, TR::Goto, 0, blockHeadTreeTop));
      newEntry->join(gotoTreeTop);
      gotoTreeTop->join(newExit);
      newExit->setNextTreeTop(NULL);
      }
   else
      {
      addToEnd = false;
      cursorTree->join(newEntry);
      newExit->join(blockHeadTreeTop);
      }

   TR_BlockStructure *invariantBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), loopInvariantBlock->getNumber(), loopInvariantBlock);
   invariantBlockStructure->setAsLoopInvariantBlock(true);

   TR::Block *dummyEntryBlock = NULL;
   TR_BlockStructure *dummyEntryBlockStructure = NULL;
   TR::Block *targetBlock = NULL;
   if (isEntry)
      {
      dummyEntryBlock = TR::Block::createEmptyBlock(predBlockExitNode, comp(), blockAtHeadOfLoop->getFrequency(), blockAtHeadOfLoop);
      _cfg->addNode(dummyEntryBlock);
      TR::TreeTop *dummyEntry = dummyEntryBlock->getEntry();
      TR::TreeTop *dummyExit = dummyEntryBlock->getExit();

      if (addToEnd)
         {
         newExit->join(dummyEntry);
         TR::TreeTop *gotoTreeTop = TR::TreeTop::create(comp(), TR::Node::create(blockHeadNode, TR::Goto, 0, newEntry));
         dummyEntry->join(gotoTreeTop);
         gotoTreeTop->join(dummyExit);
         dummyExit->setNextTreeTop(NULL);
         }
      else
         {
         cursorTree->join(dummyEntry);
         dummyExit->join(newEntry);
         }

      dummyEntryBlockStructure = new (_cfg->structureRegion()) TR_BlockStructure(comp(), dummyEntryBlock->getNumber(), dummyEntryBlock);
      targetBlock = dummyEntryBlock;
      }
   else
      targetBlock = loopInvariantBlock;

   _cfg->setStructure(NULL);
   TR::CFGEdge *loopInvariantBlock_blockAtHeadOfLoop_Edge = TR::CFGEdge::createEdge(loopInvariantBlock,  blockAtHeadOfLoop, trMemory());
   _cfg->addEdge(loopInvariantBlock_blockAtHeadOfLoop_Edge);
   TR::CFGEdge *dummyEntryBlock_loopInvariantBlock_Edge = NULL;
   if (isEntry)
      {
      dummyEntryBlock_loopInvariantBlock_Edge = TR::CFGEdge::createEdge(dummyEntryBlock,  loopInvariantBlock, trMemory());
      _cfg->addEdge(dummyEntryBlock_loopInvariantBlock_Edge);
      }

   int32_t sumPredFreq;
   modifyBranchesForSplitEdges(doWhileLoop, blockAtHeadOfLoop, loopInvariantBlock, targetBlock, addToEnd, &sumPredFreq);

   loopInvariantBlock->setFrequency(sumPredFreq);
   // This edge should have min(frequency of loopInvariantBlock, frequency of blockAtHeadOfLoop)
   // Since we just calculated the actual frequency of loopInvariantBlock, we need to apply that to the edge
   // if necessary
   if (sumPredFreq < blockAtHeadOfLoop->getFrequency())
      loopInvariantBlock_blockAtHeadOfLoop_Edge->setFrequency(sumPredFreq);

   if (dummyEntryBlock)
      {
      dummyEntryBlock->setFrequency(sumPredFreq);
      // Same as above
      if (sumPredFreq < blockAtHeadOfLoop->getFrequency())
         dummyEntryBlock_loopInvariantBlock_Edge->setFrequency(sumPredFreq);
      }

   _cfg->setStructure(_rootStructure);

   TR_StructureSubGraphNode *invariantNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(invariantBlockStructure);
   parentStructure->addSubNode(invariantNode);

   TR::CFGEdge::createEdge(invariantNode,  doWhileNode, trMemory());

   TR_StructureSubGraphNode *targetNode = NULL;
   TR_Structure *targetStructure = NULL;
   if (isEntry)
      {
      TR_StructureSubGraphNode *dummyEntryNode = new (_cfg->structureRegion()) TR_StructureSubGraphNode(dummyEntryBlockStructure);
      parentStructure->addSubNode(dummyEntryNode);

      TR::CFGEdge::createEdge(dummyEntryNode,  invariantNode, trMemory());

      parentStructure->setEntry(dummyEntryNode);
      int32_t doWhileNumber = doWhileNode->getNumber();
      doWhileNode->getStructure()->renumberRecursively(doWhileNode->getNumber(), dummyEntryBlockStructure->getNumber());
      doWhileNode->setNumber(doWhileNode->getStructure()->getNumber());
      dummyEntryBlockStructure->renumberRecursively(dummyEntryNode->getNumber(), doWhileNumber);
      dummyEntryNode->setNumber(doWhileNumber);
      targetNode = dummyEntryNode;
      targetStructure = dummyEntryBlockStructure;
      }
   else
      {
      targetNode = invariantNode;
      targetStructure = invariantBlockStructure;
      }

   for (auto predIt = doWhileNode->getPredecessors().begin(); predIt != doWhileNode->getPredecessors().end();)
      {
      if ((*predIt)->getFrom() != invariantNode)
         {
    	 /* Store a reference to current element, is removed in the removePredecessor */
    	 TR::CFGEdge *  nextPred = *(predIt++);
    	 doWhileNode->removePredecessor(nextPred);
         nextPred->setTo(targetNode);
         TR_RegionStructure *pred = toStructureSubGraphNode(nextPred->getFrom())->getStructure()->asRegion();
         if (pred && !isEntry)
            pred->replaceExitPart(doWhileNode->getNumber(), targetStructure->getNumber());
         }
      else
    	  ++predIt;
      }

   for (auto predIt = doWhileNode->getExceptionPredecessors().begin(); predIt != doWhileNode->getExceptionPredecessors().end();)
      {
      TR::CFGEdge * nextPred = *(predIt++);
      doWhileNode->removeExceptionPredecessor(nextPred);
      nextPred->setExceptionTo(targetNode);
      TR_RegionStructure *pred = toStructureSubGraphNode(nextPred->getFrom())->getStructure()->asRegion();
      if (pred  && !isEntry)
         pred->replaceExitPart(doWhileNode->getNumber(), targetStructure->getNumber());
      }

   if (trace())
      {
      traceMsg(comp(), "Structure after canonicalizing do while loop %p: number: %d\n", doWhileLoop, doWhileLoop->getNumber());
      if (comp()->getFlowGraph()->getStructure())
         getDebug()->print(comp()->getOutFile(), comp()->getFlowGraph()->getStructure(), 6);
      }

   return;
   }



void TR_LoopCanonicalizer::eliminateRedundantInductionVariablesFromLoop(TR_RegionStructure *naturalLoop)
   {
     //if (!debug("enableRednIndVarElim"))
     // return;

   static char *disableInductionVariableReplacement = feGetEnv("TR_disableIndVarOpt");

   if (disableInductionVariableReplacement)
      return;

   TR_BlockStructure *loopInvariantBlock = NULL;

   if (!naturalLoop->getParent() ||
       naturalLoop->getEntryBlock()->isCold())
      return;

   TR_RegionStructure *parentStructure = naturalLoop->getParent()->asRegion();
   TR_StructureSubGraphNode *subNode = NULL;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getNumber() == naturalLoop->getNumber())
         break;
      }

   if (subNode->getPredecessors().size() == 1)
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock() &&
          loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
         loopInvariantBlock = loopInvariantNode->getStructure()->asBlock();
      }

   if (!loopInvariantBlock)
      return;

   TR::Block *entryBlock = naturalLoop->getEntryBlock();
   TR::Block *loopTestBlock = NULL;
   TR::TreeTop *loopTestTree = NULL;

   for (auto nextEdge = entryBlock->getPredecessors().begin(); nextEdge != entryBlock->getPredecessors().end(); ++nextEdge)
      {
      TR::Block *nextBlock = toBlock((*nextEdge)->getFrom());
      if (nextBlock != loopInvariantBlock->getBlock())
         {
         //// Back edge found
         ////
         if (nextBlock->getLastRealTreeTop()->getNode()->getOpCode().isIf() && !nextBlock->getLastRealTreeTop()->getNode()->getOpCode().isCompBranchOnly())
            {
            loopTestBlock = nextBlock;
            loopTestTree = nextBlock->getLastRealTreeTop();
            }
         else
            break;
         }
      }

   if (!loopTestBlock)
      return;

   //TR_InductionVariable *primaryInductionVariable = NULL;
   TR::SymbolReference *symRefInCompare = NULL;
   //TR::Symbol *symbolInCompare = NULL;
   TR::Node *childInCompare = loopTestTree->getNode()->getFirstChild();
   while (childInCompare->getOpCode().isAdd() || childInCompare->getOpCode().isSub())
      {
      if (childInCompare->getSecondChild()->getOpCode().isLoadConst())
         childInCompare = childInCompare->getFirstChild();
      else
         break;
      }


   if (childInCompare->getOpCode().hasSymbolReference())
      {
      symRefInCompare = childInCompare->getSymbolReference();
      //symbolInCompare = symRefInCompare->getSymbol();
      //if (!symbolInCompare->isAutoOrParm())
       //  symbolInCompare = NULL;
      }
/*
   TR_ScratchList<TR_InductionVariable> derivedInductionVariables(trMemory());
   if (symbolInCompare)
      {
      TR_InductionVariable *v;
      for (v = naturalLoop->getFirstInductionVariable(); v; v = v->getNext())
         {
         if ((v->getLocal()->getDataType() == TR::Int32) ||
             (v->getLocal()->getDataType() == TR::Int64))
            {
            if ((v->getLocal() == symbolInCompare) &&
                (((v->getLocal()->getDataType() == TR::Int32) &&
                  (v->getIncr()->getLowInt() == v->getIncr()->getHighInt())) ||
                 ((v->getLocal()->getDataType() == TR::Int64) &&
                  (v->getIncr()->getLowLong() == v->getIncr()->getHighLong()))))
               {
               primaryInductionVariable = v;
               //break;
               }
            else
               {
               derivedInductionVariables.add(v);
               }
            }
         }
      }  */

   TR_PrimaryInductionVariable *primaryInductionVariable = naturalLoop->getPrimaryInductionVariable();
   ListIterator<TR_BasicInductionVariable> bivIterator(&naturalLoop->getBasicInductionVariables());
   if (primaryInductionVariable &&
       bivIterator.getFirst())
      {
      //printf("Found primary induction variable in loop %d in method %s\n", naturalLoop->getNumber(), comp()->signature());
      //fflush(stdout);
      dumpOptDetails(comp(), "Found primary induction variable in loop %d in method %s\n", naturalLoop->getNumber(), comp()->signature());

      bool alteredCode = false;
      _primaryInductionVariable = symRefInCompare;

      int64_t primaryIncr = 0;
      //if (primaryInductionVariable->getLocal()->getDataType() == TR::Int32)
      //   primaryIncr = (int64_t) primaryInductionVariable->getIncr()->getLowInt();
      //else
      primaryIncr = (int64_t) primaryInductionVariable->getIncrement();

      //ListIterator<TR_InductionVariable> derivedIt(&derivedInductionVariables);
      TR_BasicInductionVariable *nextIndVar;
      for (nextIndVar = bivIterator.getFirst(); nextIndVar != NULL;)
         {
         TR_BasicInductionVariable *tempIndVar = bivIterator.getNext();
         int64_t nextIndVarIncr = 0;
         //if (nextIndVar->getLocal()->getDataType() == TR::Int32)
         //   nextIndVarIncr = (int64_t) nextIndVar->getIncr()->getLowInt();
         //else
         nextIndVarIncr = (int64_t) nextIndVar->getIncrement();

         if ((nextIndVar->getSymRef()->getSymbol()->getDataType() == primaryInductionVariable->getSymRef()->getSymbol()->getDataType()) &&
             //(((nextIndVar->getLocal()->getDataType() == TR::Int32) &&
              //(nextIndVar->getIncr()->getLowInt() == nextIndVar->getIncr()->getHighInt())) ||
              //((nextIndVar->getLocal()->getDataType() == TR::Int64) &&
               //(nextIndVar->getIncr()->getLowLong() == nextIndVar->getIncr()->getHighLong()))) &&
               primaryIncr && (nextIndVarIncr == primaryIncr))
            {
            TR::SymbolReference *derivedInductionVar = NULL;
            int32_t symRefCount = comp()->getSymRefCount();
            TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
            int32_t symRefNumber;
            for (symRefNumber = symRefTab->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
               {
               TR::SymbolReference *symRef = symRefTab->getSymRef(symRefNumber);
               if (symRef)
                  {
                  TR::Symbol *sym = symRef->getSymbol();
                  if (nextIndVar->getSymRef()->getSymbol() == sym)
                     {
                     derivedInductionVar = symRef;
                     break;
                     }
                  }
               }

            _primaryIncrementedFirst = 0;
            _primaryInductionIncrementBlock = NULL;
            _derivedInductionIncrementBlock = NULL;
            _currentBlock = NULL;
            _entryBlock = entryBlock;
            _loopTestBlock = loopTestBlock;
            _primaryIncr = primaryIncr;
            _derivedIncr = nextIndVarIncr;
            TR_ScratchList<TR::Block> derivedInductionVarIncrementBlocks(trMemory());
            TR_ScratchList<TR::Block> primaryInductionVarIncrementBlocks(trMemory());
            //comp()->incVisitCount();
            bool incrementsInLockStep = incrementedInLockStep(naturalLoop, derivedInductionVar, symRefInCompare, nextIndVarIncr, primaryIncr, &derivedInductionVarIncrementBlocks, &primaryInductionVarIncrementBlocks);
            if (incrementsInLockStep &&
                (!derivedInductionVarIncrementBlocks.isEmpty() ||
                 !primaryInductionVarIncrementBlocks.isEmpty()))
               {
               incrementsInLockStep = checkIfOrderOfBlocksIsKnown(naturalLoop, entryBlock, loopTestBlock, &derivedInductionVarIncrementBlocks, &primaryInductionVarIncrementBlocks, _primaryIncrementedFirst);
               //if (_primaryIncrementedFirst == 1)
               //     {
               //   _primaryInductionIncrementBlock = entryBlock;
               //   _derivedInductionIncrementBlock = loopTestBlock;
               //   }
               //else
               //     {
               //   _primaryInductionIncrementBlock = loopTestBlock;
               //   _derivedInductionIncrementBlock = entryBlock;
               //     }
               }

            if (incrementsInLockStep)
               {
               _symRefBeingReplaced = derivedInductionVar;
               alteredCode = true;
               comp()->incVisitCount();
               TR::SymbolReference *newSymbolReference = NULL;
               if ((!comp()->cg()->hasComplexAddressingMode()) ||
                    checkComplexInductionVariableUse(naturalLoop))
                  {
                  if (performTransformation(comp(), "%sRemoving redundant induction variable #%d\n", OPT_DETAILS, derivedInductionVar->getReferenceNumber()))
                     replaceAllInductionVariableComputations(loopInvariantBlock->getBlock(), naturalLoop, &newSymbolReference, symRefInCompare);
                  }

               if (newSymbolReference)
                  {
                  optimizer()->setAliasSetsAreValid(false);
                  placeInitializationTreeInLoopPreHeader(loopInvariantBlock->getBlock(), loopInvariantBlock->getBlock()->getEntry()->getNode(), newSymbolReference, symRefInCompare, derivedInductionVar);
                  replaceInductionVariableComputationsInExits(naturalLoop, loopInvariantBlock->getBlock()->getEntry()->getNode(), newSymbolReference, symRefInCompare, derivedInductionVar);
                  requestOpt(OMR::globalDeadStoreElimination);
                  requestOpt(OMR::globalDeadStoreGroup);
                  }

               _symRefBeingReplaced = NULL;
               }
            //else
              // derivedInductionVariables.remove(nextIndVar);
            }
         //else
           // derivedInductionVariables.remove(nextIndVar);
         nextIndVar = tempIndVar;
         }

      if (alteredCode)
         {
         if (loopInvariantBlock->getBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() != TR::BBStart)
            {
            TR::TreeTop *treeToMoveAfterSplit;
            if (loopInvariantBlock->getBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto)
               treeToMoveAfterSplit = NULL;
            else
               treeToMoveAfterSplit = loopInvariantBlock->getBlock()->getLastRealTreeTop();

            //TR_ASSERT((loopInvariantBlock->getBlock()->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto), "Expect the loop pre-header to end in a goto for call to TR::Block::split to work correctly\n");
            TR::Block *newLoopInvariantBlock = loopInvariantBlock->getBlock()->split(loopInvariantBlock->getBlock()->getLastRealTreeTop(), comp()->getFlowGraph(), false);
            if (treeToMoveAfterSplit)
               {
               treeToMoveAfterSplit->getPrevTreeTop()->join(treeToMoveAfterSplit->getNextTreeTop());
               loopInvariantBlock->getBlock()->append(treeToMoveAfterSplit);
               }

            newLoopInvariantBlock->getStructureOf()->setAsLoopInvariantBlock(true);
            loopInvariantBlock->setAsLoopInvariantBlock(false);
            }

         _primaryInductionVariable = NULL;
         }
      }
   }




bool TR_LoopCanonicalizer::replaceInductionVariableComputationsInExits(TR_Structure *structure, TR::Node *node, TR::SymbolReference *newSymbolReference, TR::SymbolReference *primaryInductionVar, TR::SymbolReference *derivedInductionVar)
   {
   bool seenInductionVariableComputation = false;
   for (auto edge = _entryBlock->getSuccessors().begin(); edge != _entryBlock->getSuccessors().end();)
      {
      auto next = edge;
      ++next;
      TR::Block *block = (*edge)->getTo()->asBlock();
      if (!structure->contains(block->getStructureOf()))
         {
         TR::Block *newBlock = (*edge)->getFrom()->asBlock()->splitEdge((*edge)->getFrom()->asBlock(), block, comp());
         TR::DataType dataType = newSymbolReference->getSymbol()->getDataType();
         TR::Node *subNode = TR::Node::create((dataType == TR::Int32) ? TR::iadd : TR::ladd, 2,
                                                TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, newSymbolReference),
                                                TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, primaryInductionVar));
         if (_primaryIncrementedFirst != 0)
            {
            int64_t additiveConstant = _primaryIncr;
            TR::Node *adjustmentConstChild = 0;
            if(dataType == TR::Int32)
               adjustmentConstChild = TR::Node::create(node, TR::iconst, 0, 0);
            else
               adjustmentConstChild = TR::Node::create(node,TR::lconst, 0, 0);

            subNode = TR::Node::create(((dataType == TR::Int32) ? TR::iadd : TR::ladd), 2, subNode, adjustmentConstChild);

            if (_primaryInductionIncrementBlock == _entryBlock)
               {
               if (adjustmentConstChild->getDataType() == TR::Int32)
                  adjustmentConstChild->setInt((int32_t) -1*additiveConstant);
               else
                  adjustmentConstChild->setLongInt(-1*additiveConstant);
               }
            else
               {
               if (adjustmentConstChild->getDataType() == TR::Int32)
                  adjustmentConstChild->setInt((int32_t) additiveConstant);
               else
                  adjustmentConstChild->setLongInt(additiveConstant);
               }
            }

         TR::Node *storeNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(dataType), 1, 1, subNode, derivedInductionVar);
         TR::TreeTop *newStoreTree = TR::TreeTop::create(comp(), storeNode, NULL, NULL);

         TR::TreeTop *placeHolderTree = newBlock->getLastRealTreeTop();
         if (!placeHolderTree->getNode()->getOpCode().isBranch())
            placeHolderTree = newBlock->getExit();

         TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();
         prevTree->join(newStoreTree);
         newStoreTree->join(placeHolderTree);
         }
      edge = next;
      }

   for (auto edge = _loopTestBlock->getSuccessors().begin(); edge != _loopTestBlock->getSuccessors().end();)
      {
      auto next = edge;
      ++next;
      TR::Block *block = (*edge)->getTo()->asBlock();
      if (!structure->contains(block->getStructureOf()))
         {
         TR::Block *newBlock = (*edge)->getFrom()->asBlock()->splitEdge((*edge)->getFrom()->asBlock(), block, comp());
         TR::DataType dataType = newSymbolReference->getSymbol()->getDataType();
         TR::Node *subNode = TR::Node::create((dataType == TR::Int32) ? TR::iadd : TR::ladd, 2,
                                                TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, newSymbolReference),
                                                TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, primaryInductionVar));
         TR::Node *storeNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(dataType), 1, 1, subNode, derivedInductionVar);
         TR::TreeTop *newStoreTree = TR::TreeTop::create(comp(), storeNode, NULL, NULL);

         TR::TreeTop *placeHolderTree = newBlock->getLastRealTreeTop();
         if (!placeHolderTree->getNode()->getOpCode().isBranch())
            placeHolderTree = newBlock->getExit();

         TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();
         prevTree->join(newStoreTree);
         newStoreTree->join(placeHolderTree);
         }
      edge = next;
      }

   return seenInductionVariableComputation;
   }




void TR_LoopCanonicalizer::findIncrements(TR::Node *currentNode, vcount_t visitCount, TR::SymbolReference *derivedInductionVar, TR::SymbolReference *primaryInductionVar, int64_t &derivedInductionVarIncrement, int64_t &primaryInductionVarIncrement, bool &unknownIncrement)
   {
   //if (currentNode->getVisitCount() == visitCount)
   //   return;

   //currentNode->setVisitCount(visitCount);

   if (currentNode->getOpCode().isStore())
      {
      TR::Node *child = currentNode->getFirstChild();
      if (currentNode->getSymbolReference() == derivedInductionVar)
         {
         bool incrInStandardForm = false;
         if ((derivedInductionVarIncrement == 0) &&
             child->getOpCode().isAdd())
            {
            TR::Node *secondChild = child->getSecondChild();
            if (secondChild->getOpCode().isLoadConst())
               {
               TR::Node *firstChild = child->getFirstChild();
               int64_t curIncr;
               if (secondChild->getOpCodeValue() == TR::iconst)
                  curIncr = (int64_t) secondChild->getInt();
               else
                  curIncr = secondChild->getLongInt();

               if (firstChild->getOpCode().isLoadVar() &&
                   (firstChild->getSymbolReference() == derivedInductionVar))
                  {
                  incrInStandardForm = true;
                  derivedInductionVarIncrement = derivedInductionVarIncrement + curIncr;
                  }
               }
            }
         else if ((derivedInductionVarIncrement == 0) &&
                  child->getOpCode().isSub())
            {
            TR::Node *secondChild = child->getSecondChild();
            if (secondChild->getOpCode().isLoadConst())
               {
               TR::Node *firstChild = child->getFirstChild();
               int64_t curIncr;
               if (secondChild->getOpCodeValue() == TR::iconst)
                  curIncr = (int64_t) secondChild->getInt();
               else
                  curIncr = secondChild->getLongInt();

               if (firstChild->getOpCode().isLoadVar() &&
                   (firstChild->getSymbolReference() == derivedInductionVar))
                  {
                  incrInStandardForm = true;
                  derivedInductionVarIncrement = derivedInductionVarIncrement - curIncr;
                  }
               }
            }
         if (!incrInStandardForm)
            unknownIncrement = true;
         }
      else if (currentNode->getSymbolReference() == primaryInductionVar)
         {
         bool incrInStandardForm = false;
         if ((primaryInductionVarIncrement == 0) &&
             child->getOpCode().isAdd())
            {
            TR::Node *secondChild = child->getSecondChild();
            if (secondChild->getOpCode().isLoadConst())
               {
               TR::Node *firstChild = child->getFirstChild();
               int64_t curIncr;
               if (secondChild->getOpCodeValue() == TR::iconst)
                  curIncr = (int64_t) secondChild->getInt();
               else
                  curIncr = secondChild->getLongInt();

               if (firstChild->getOpCode().isLoadVar() &&
                   (firstChild->getSymbolReference() == primaryInductionVar))
                   {
                   incrInStandardForm = true;
                   primaryInductionVarIncrement = primaryInductionVarIncrement + curIncr;
                   }
               }
            }
         else if ((primaryInductionVarIncrement == 0) &&
                  child->getOpCode().isSub())
            {
            TR::Node *secondChild = child->getSecondChild();
            if (secondChild->getOpCode().isLoadConst())
               {
               TR::Node *firstChild = child->getFirstChild();
               int64_t curIncr;
               if (secondChild->getOpCodeValue() == TR::iconst)
                  curIncr = (int64_t) secondChild->getInt();
               else
                  curIncr = secondChild->getLongInt();

               if (firstChild->getOpCode().isLoadVar() &&
                   (firstChild->getSymbolReference() == primaryInductionVar))
                  {
                  incrInStandardForm = true;
                  primaryInductionVarIncrement = primaryInductionVarIncrement - curIncr;
                  }
               }
            }

         if (!incrInStandardForm)
            unknownIncrement = true;
         }
      }

   int32_t i;
   for (i=0;i<currentNode->getNumChildren();i++)
      {
      TR::Node *child = currentNode->getChild(i);
      if (child->getOpCode().isStore())
          findIncrements(child, visitCount, derivedInductionVar, primaryInductionVar, derivedInductionVarIncrement, primaryInductionVarIncrement, unknownIncrement);
      }
   }


bool TR_LoopCanonicalizer::incrementedInLockStep(TR_Structure *structure, TR::SymbolReference *derivedInductionVar, TR::SymbolReference *primaryInductionVar, int64_t derivedInductionVarLoopIncrement, int64_t primaryInductionVarLoopIncrement, TR_ScratchList<TR::Block> *derivedInductionVarIncrementBlocks, TR_ScratchList<TR::Block> *primaryInductionVarIncrementBlocks)
   {
   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();

      int64_t primaryInductionVarIncrement = 0;
      int64_t derivedInductionVarIncrement = 0;
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();
      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();
         bool unknownIncrement = false;
         findIncrements(currentNode, comp()->getVisitCount(), derivedInductionVar, primaryInductionVar, derivedInductionVarIncrement, primaryInductionVarIncrement, unknownIncrement);
         if (unknownIncrement)
       return false;
         currentTree = currentTree->getNextTreeTop();
         }

      if (primaryInductionVarIncrement == derivedInductionVarIncrement)
         return true;
      else if ((primaryInductionVarIncrement == primaryInductionVarLoopIncrement) &&
          (derivedInductionVarIncrement == 0))
         {
         primaryInductionVarIncrementBlocks->add(block);
         return true;
         }
      else if ((derivedInductionVarIncrement == derivedInductionVarLoopIncrement) &&
          (primaryInductionVarIncrement == 0))
         {
         derivedInductionVarIncrementBlocks->add(block);
         return true;
         }

      return false;
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;

      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         if (!incrementedInLockStep(node->getStructure(), derivedInductionVar, primaryInductionVar, derivedInductionVarLoopIncrement, primaryInductionVarLoopIncrement, derivedInductionVarIncrementBlocks, primaryInductionVarIncrementBlocks))
            return false;
         }
      return true;
      }

   return false;
   }


bool TR_LoopCanonicalizer::checkIfOrderOfBlocksIsKnown(TR_RegionStructure *naturalLoop, TR::Block *entryBlock, TR::Block *loopTestBlock, TR_ScratchList<TR::Block> *derivedInductionVarIncrementBlocks, TR_ScratchList<TR::Block> *primaryInductionVarIncrementBlocks, uint8_t &primaryFirst)
   {
   if (!derivedInductionVarIncrementBlocks->isSingleton())
      return false;

   if (!primaryInductionVarIncrementBlocks->isSingleton())
      return false;

   TR::Block *derivedBlock = derivedInductionVarIncrementBlocks->getListHead()->getData();
   TR::Block *primaryBlock = primaryInductionVarIncrementBlocks->getListHead()->getData();

   for (auto edge = entryBlock->getExceptionSuccessors().begin(); edge != entryBlock->getExceptionSuccessors().end(); ++edge)
      {
      TR::Block *catchBlock = (*edge)->getTo()->asBlock();
      if (naturalLoop->contains(catchBlock->getStructureOf()))
         return false;
      }

   for (auto edge = loopTestBlock->getExceptionSuccessors().begin(); edge != loopTestBlock->getExceptionSuccessors().end(); ++edge)
      {
      TR::Block *catchBlock = (*edge)->getTo()->asBlock();
      if (naturalLoop->contains(catchBlock->getStructureOf()))
         return false;
      }

   if (entryBlock == primaryBlock)
      {
      _primaryInductionIncrementBlock = entryBlock;
      if (loopTestBlock == derivedBlock)
         {
         _derivedInductionIncrementBlock = loopTestBlock;
         primaryFirst = 1;
         return true;
         }
      else if (loopTestBlock->getPredecessors().size() == 1)
         {
         TR::Block *onlyPredOfLoopTestBlock = loopTestBlock->getPredecessors().front()->getFrom()->asBlock();
         if ((onlyPredOfLoopTestBlock->getSuccessors().size() == 1) &&
             (onlyPredOfLoopTestBlock == derivedBlock))
            {
            _derivedInductionIncrementBlock = onlyPredOfLoopTestBlock;
            primaryFirst = 1;
            return true;
            }
         }
      }
   else if (entryBlock == derivedBlock)
      {
      _derivedInductionIncrementBlock = entryBlock;
      if (loopTestBlock == primaryBlock)
         {
         _primaryInductionIncrementBlock = loopTestBlock;
         primaryFirst = 2;
         return true;
         }
      else if (loopTestBlock->getPredecessors().size() == 1)
         {
         TR::Block *onlyPredOfLoopTestBlock = loopTestBlock->getPredecessors().front()->getFrom()->asBlock();
         if ((onlyPredOfLoopTestBlock->getSuccessors().size() == 1) &&
             (onlyPredOfLoopTestBlock == primaryBlock))
            {
            _primaryInductionIncrementBlock = onlyPredOfLoopTestBlock;
            primaryFirst = 2;
            return true;
            }
         }
      }

   return false;
   }

/**
 *  Checking to see if the induction variable to be removed participates in any compress
 *  address expressions, such as [base + stride*index] on x86 hardware.
 *  If the address expression contains a mul operand the address expression formed
 *  by the codegen will not have any space for a folded in variable substitution.
 *
 *  @return true, when the induction variable is used only in simple addressing modes
 *          false, when the induction variable is used in complex addressing modes
 */
bool TR_LoopCanonicalizer::checkComplexInductionVariableUse(TR_Structure *structure)
   {
   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();

      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();

         if (!checkComplexInductionVariableUseNode(currentNode, false))
            return false;

         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;

      TR_RegionStructure::Cursor si(*regionStructure);

      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         if (!checkComplexInductionVariableUse(node->getStructure()))
            return false;
         }
      }

   return true;
   }

bool TR_LoopCanonicalizer::checkComplexInductionVariableUseNode(TR::Node *node, bool addrExpression)
   {
   TR::ILOpCode &opCode = node->getOpCode();


   traceMsg(comp(), "NG: Walking node 0x%p\n",node);
   if (opCode.isStoreIndirect())
      {
      addrExpression = true;
      }
   else if (addrExpression)
      {
      if (node->getOpCodeValue() == TR::imul)
         {
         traceMsg(comp(), "Found imul node 0x%p used in address expression.\n", node);
         if (node->getFirstChild()->getOpCode().hasSymbolReference() &&
             node->getFirstChild()->getSymbolReference() == _symRefBeingReplaced)
            {
            traceMsg(comp(), "\tAvoiding induction variable replacement because of address mode complexity. Sym Ref. = %p\n", _symRefBeingReplaced);
            return false;
            }
         }
      else if (node->getOpCodeValue() == TR::lmul)
         {
         traceMsg(comp(), "Found lmul node 0x%p used in address expression.\n", node);
         if (node->getFirstChild()->getOpCodeValue() == TR::i2l &&
             node->getFirstChild()->getFirstChild()->getOpCode().hasSymbolReference() &&
             node->getFirstChild()->getFirstChild()->getSymbolReference() == _symRefBeingReplaced)
            {
            traceMsg(comp(), "\tAvoiding induction variable replacement because of address mode complexity. Sym Ref. = %p\n", _symRefBeingReplaced);
            return false;
            }
         }
      }

   for (int32_t i = 0; i<node->getNumChildren(); i++)
      {
      if (!checkComplexInductionVariableUseNode(node->getChild(i), addrExpression))
         return false;
      }

   return true;
   }



bool TR_LoopCanonicalizer::examineTreeForInductionVariableUse(TR::Block *loopInvariantBlock, TR::Node *parent, int32_t childNum, TR::Node *node, vcount_t visitCount, TR::SymbolReference **newSymbolReference)
  {
   bool seenInductionVariableComputation = false;
   bool examineChildren = true;

   if (node->getVisitCount() == visitCount)
      {
      examineChildren = false;
      return seenInductionVariableComputation;
      }

   node->setVisitCount(visitCount);

   if (node->getOpCodeValue() == TR::BBStart)
      {
      _primaryInductionVarStoreInBlock = NULL;
      _primaryInductionVarStoreSomewhereInBlock = NULL;
      _derivedInductionVarStoreInBlock = NULL;
      _currentBlock = node->getBlock();
      }

   if (node->getOpCode().hasSymbolReference() &&
       !node->getOpCode().isStore() &&
       node->getSymbolReference() == _symRefBeingReplaced &&
       comp()->getJittedMethodSymbol() && // avoid NULL pointer on non-Wcode builds
              comp()->getJittedMethodSymbol()->isNoTemps())
      {
      dumpOptDetails(comp(), "Skipping transformation under NOTEMPS\n");
      return false;
      }

   if (node->getOpCode().hasSymbolReference() &&
       !node->getOpCode().isStore())
      {
      if ((node->getSymbolReference() == _symRefBeingReplaced) &&
          performTransformation(comp(), "Replacing use %p of sym ref #%d by sym ref #%d\n", node, node->getSymbolReference()->getReferenceNumber(), _primaryInductionVariable->getReferenceNumber()))
         {
         TR::DataType dataType = node->getDataType();
         if (!(*newSymbolReference))
           *newSymbolReference = comp()->getSymRefTab()->createTemporary(comp()->getMethodSymbol(), dataType, false);

         TR::Node *loadOfPrimaryInductionVar = NULL;
         if (_primaryIncrementedFirst == 0)
            {
            //if (_currentBlock == _primaryInductionIncrementBlock)
            if (!_derivedInductionVarStoreInBlock)
               {
               if (!_primaryInductionVarStoreSomewhereInBlock)
                  {
                  TR::TreeTop *curTree = _currentBlock->getEntry();
                  TR::TreeTop *exitTree = _currentBlock->getExit();
                  while (curTree != exitTree)
                     {
                     TR::Node *curNode = curTree->getNode();
                     if (!curNode->getOpCode().isStore() &&
                         (curNode->getNumChildren() > 0))
                        curNode = curNode->getFirstChild();
                     if (curNode->getOpCode().isStore() &&
                         (curNode->getSymbolReference() == _primaryInductionVariable))
                        {
                        _primaryInductionVarStoreSomewhereInBlock = curNode;
                        break;
                        }
                     curTree = curTree->getNextTreeTop();
                     }
                  }

               if (_primaryInductionVarStoreSomewhereInBlock)
                  loadOfPrimaryInductionVar = _primaryInductionVarStoreSomewhereInBlock->getFirstChild()->getFirstChild();
               }
            }

         if (!loadOfPrimaryInductionVar)
            loadOfPrimaryInductionVar = TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, _primaryInductionVariable);

         loadOfPrimaryInductionVar = TR::Node::create(((dataType == TR::Int32) ? TR::iadd : TR::ladd), 2, loadOfPrimaryInductionVar, TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, *newSymbolReference));

         bool nodeSetUp = false;
         if (_primaryIncrementedFirst == 0)
            {
            if (_derivedInductionVarStoreInBlock)
               {
               if (!_primaryInductionVarStoreInBlock)
                  {
                  TR::Node *constChild = _derivedInductionVarStoreInBlock->getFirstChild()->getSecondChild();
                  int64_t additiveConstant;
                  if (constChild->getOpCodeValue() == TR::lconst)
                     additiveConstant = constChild->getLongInt();
                  else
                     additiveConstant = (int64_t) constChild->getInt();

                  TR::Node *adjustmentConstChild = TR::Node::create(constChild, constChild->getOpCodeValue(), 0, 0);
                  if (adjustmentConstChild->getDataType() == TR::Int32)
                     adjustmentConstChild->setInt((int32_t) additiveConstant);
                  else
                     adjustmentConstChild->setLongInt(additiveConstant);

                  nodeSetUp = true;
                  TR::Node::recreate(node, _derivedInductionVarStoreInBlock->getFirstChild()->getOpCodeValue());
                  node->setNumChildren(2);
                  node->setAndIncChild(0, loadOfPrimaryInductionVar);
                  node->setAndIncChild(1, adjustmentConstChild);
                  }
               }
            else if (0) // Do not need adjustment in this case since we have obtained the correctly commoned node in this case
               {
               if (_primaryInductionVarStoreInBlock)
                  {
                  TR::Node *constChild = _primaryInductionVarStoreInBlock->getFirstChild()->getSecondChild();
                  int64_t additiveConstant;
                  if (constChild->getOpCodeValue() == TR::lconst)
                     additiveConstant = constChild->getLongInt();
                  else
                     additiveConstant = (int64_t) constChild->getInt();

                  TR::Node *adjustmentConstChild = TR::Node::create(constChild, constChild->getOpCodeValue(), 0, 0);
                  if (adjustmentConstChild->getDataType() == TR::Int32)
                     adjustmentConstChild->setInt((int32_t) -1*additiveConstant);
                  else
                     adjustmentConstChild->setLongInt(-1*additiveConstant);

                  nodeSetUp = true;
                  TR::Node::recreate(node, _primaryInductionVarStoreInBlock->getFirstChild()->getOpCodeValue());
                  node->setNumChildren(2);
                  node->setAndIncChild(0, loadOfPrimaryInductionVar);
                  node->setAndIncChild(1, adjustmentConstChild);
                  }
               }
            }
         else
            {
            if (_primaryIncrementedFirst == 1)
               {
               if (((_primaryInductionIncrementBlock != _currentBlock) ||
                    _primaryInductionVarStoreInBlock) &&
                   ((_derivedInductionIncrementBlock != _currentBlock) ||
                    !_derivedInductionVarStoreInBlock) &&
                   ((_derivedInductionIncrementBlock == _loopTestBlock) ||
                    (_currentBlock != _loopTestBlock)))
                  {
                  int64_t additiveConstant = _primaryIncr;
                  TR::Node *adjustmentConstChild = 0;
                  if(dataType == TR::Int32)
                     adjustmentConstChild = TR::Node::create(node, TR::iconst, 0, (int32_t) -1*additiveConstant);
                  else
                     {
                     adjustmentConstChild = TR::Node::create(node,TR::lconst, 0, 0);
                     adjustmentConstChild->setLongInt(-1*additiveConstant);
                     }


                  nodeSetUp = true;
                  TR::Node::recreate(node, ((dataType == TR::Int32) ? TR::iadd : TR::ladd));
                  node->setNumChildren(2);
                  node->setAndIncChild(0, loadOfPrimaryInductionVar);
                  node->setAndIncChild(1, adjustmentConstChild);
                  }
               }
            else
               {
               if (((_primaryInductionIncrementBlock != _currentBlock) ||
                     !_primaryInductionVarStoreInBlock) &&
                   ((_derivedInductionIncrementBlock != _currentBlock) ||
                    _derivedInductionVarStoreInBlock) &&
                   ((_primaryInductionIncrementBlock == _loopTestBlock) ||
                    (_currentBlock != _loopTestBlock)))
                  {
                  int64_t additiveConstant = _derivedIncr;
                  TR::Node *adjustmentConstChild = 0;
                  if(dataType == TR::Int32)
                     adjustmentConstChild = TR::Node::create(node, TR::iconst, 0, (int32_t) additiveConstant);
                  else
                     {
                     adjustmentConstChild = TR::Node::create(node,TR::lconst, 0, 0);
                     adjustmentConstChild->setLongInt(additiveConstant);
                     }


                  nodeSetUp = true;
                  TR::Node::recreate(node, ((dataType == TR::Int32) ? TR::iadd : TR::ladd));
                  node->setNumChildren(2);
                  node->setAndIncChild(0, loadOfPrimaryInductionVar);
                  node->setAndIncChild(1, adjustmentConstChild);
                  }
               }
            }

         if (!nodeSetUp)
            {
            TR::Node::recreate(node, loadOfPrimaryInductionVar->getOpCodeValue());
            node->setNumChildren(2);
            node->setChild(0, loadOfPrimaryInductionVar->getFirstChild());
            node->setChild(1, loadOfPrimaryInductionVar->getSecondChild());
            }
         seenInductionVariableComputation = true;

         //TR::Node *addNode = TR::Node::create((dataType == TR_SInt32) ? TR::iadd : TR::ladd, 2,
         //                                   TR::Node::create(node, comp()->il.opCodeForDirectLoad(dataType), 0, _primaryInductionVariable),
         //                                   TR::Node::create(node, comp()->il.opCodeForDirectLoad(dataType), 0, *newSymbolReference));
         //parent->setAndIncChild(childNum, addNode);
         //node->recursivelyDecReferenceCount();
         //seenInductionVariableComputation = true;
         }
      }

   if (examineChildren)
      {
      int32_t i;
      for (i=0;i<node->getNumChildren();i++)
         {
         if (examineTreeForInductionVariableUse(loopInvariantBlock, node, i, node->getChild(i), visitCount, newSymbolReference))
            seenInductionVariableComputation = true;
         }
      }

   if (node->getOpCode().isStore())
      {
      if (node->getSymbolReference() == _primaryInductionVariable)
         {
         _primaryInductionVarStoreInBlock = node;
         _primaryInductionVarStoreSomewhereInBlock = node;
         }
      else if (node->getSymbolReference() == _symRefBeingReplaced)
         _derivedInductionVarStoreInBlock = node;
      }


   return seenInductionVariableComputation;
   }






void TR_LoopCanonicalizer::placeInitializationTreeInLoopPreHeader(TR::Block *b, TR::Node *node, TR::SymbolReference *newSymbolReference, TR::SymbolReference *primaryInductionVar, TR::SymbolReference *derivedInductionVar)
   {
   TR::DataType dataType = newSymbolReference->getSymbol()->getDataType();
   TR::Node *subNode = TR::Node::create((dataType == TR::Int32) ? TR::isub : TR::lsub, 2,
                                            TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, derivedInductionVar),
                                            TR::Node::createWithSymRef(node, comp()->il.opCodeForDirectLoad(dataType), 0, primaryInductionVar));
   TR::Node *storeNode = TR::Node::createWithSymRef(comp()->il.opCodeForDirectStore(dataType), 1, 1, subNode, newSymbolReference);
   TR::TreeTop *newStoreTree = TR::TreeTop::create(comp(), storeNode, NULL, NULL);

   TR::TreeTop *placeHolderTree = b->getLastRealTreeTop();
   if (!placeHolderTree->getNode()->getOpCode().isBranch())
      placeHolderTree = b->getExit();

   TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();
   prevTree->join(newStoreTree);
   newStoreTree->join(placeHolderTree);
   }

/**
 * Find tests against post-increments and rewrite them to use pre-increments.
 *
 * If a conditional test looks at the previous value of a variable, it can
 * sometimes be rewritten to look at the new value instead. Particularly for
 * loop tests against induction variables, the optimizer is better attuned to
 * tests against the new value, because those are commonly generated for
 * typical for-loops (after canonicalization).
 *
 * Four combinations of increment/decrement and comparison operator are
 * rewritten, listed below, where @c i0 is the previous value of @c i.
 *
   @verbatim
   original     original       equivalent     equivalent
     test       condition      condition         test

   i++ <  n      i0 <  n      i0 + 1 <= n      ++i <= n
   i-- <= n      i0 <= n      i0 - 1 <  n      --i <  n
   i-- >  n      i0 >  n      i0 - 1 >= n      --i >= n
   i++ >= n      i0 >= n      i0 + 1 >  n      ++i >  n
   @endverbatim
 *
 * For example, the following increment and test
 *
   @verbatim
          treetop
      n1n   iload i
          istore i
      n2n   isub (cannotOverflow)
              iload i
              iconst -1
          ificmplt --> [...]
      n1n   ==>iload i
            iload n
   @endverbatim
 *
 * will be rewritten to
 *
   @verbatim
          treetop
      n1n   iload i
          istore i
      n2n   isub (cannotOverflow)
              iload i
              iconst -1
          ificmple --> [...]
      n2n   ==>isub
            iload n
   @endverbatim
 *
 * In order to find such tests, each block in @p region is considered, except
 * blocks that are within nested cyclic regions.
 *
 * @param region The region in which to rewrite tests.
 */
void TR_LoopCanonicalizer::rewritePostToPreIncrementTestInRegion(TR_RegionStructure *region)
   {
   TR_RegionStructure::Cursor it(*region);
   for (TR_StructureSubGraphNode *sub = it.getCurrent(); sub != NULL; sub = it.getNext())
      {
      TR_Structure * const s = sub->getStructure();
      if (s->asBlock() != NULL)
         rewritePostToPreIncrementTestInBlock(s->asBlock()->getBlock());
      else if (s->asRegion()->isAcyclic()) // don't descend into nested loops
         rewritePostToPreIncrementTestInRegion(s->asRegion());
      }
   }

static bool isLoadVarDirectOf(TR::Node * const node, int32_t symrefNum)
   {
   return node->getOpCode().isLoadVarDirect()
      && node->getSymbolReference()->getReferenceNumber() == symrefNum;
   }

// Per-block implementation of rewritePostToPreIncrementTestInRegion (above).
void TR_LoopCanonicalizer::rewritePostToPreIncrementTestInBlock(
   TR::Block * const block)
   {
   TR::TreeTop * const testTree = block->getLastRealTreeTop();
   TR::Node * const test = testTree->getNode();
   if (!test->getOpCode().isIf())
      return;

   if (!test->getOpCode().isCompareForOrder())
      return;

   if (!test->getChild(0)->getOpCode().isInteger())
      return;

   if (test->getOpCode().isUnsignedCompare())
      return; // Below we rely on the (signed) cannotOverflow flag.

   TR::Node * const store = testTree->getPrevTreeTop()->getNode();
   if (!store->getOpCode().isStoreDirect())
      return;

   TR::SymbolReference * const symref = store->getSymbolReference();
   if (!symref->getSymbol()->isAutoOrParm())
      return;

   // Require one of the children of the conditional to be a load of symref
   const int32_t symrefNum = symref->getReferenceNumber();
   int i = 0;
   while (i < 2 && !isLoadVarDirectOf(test->getChild(i), symrefNum))
      i++;

   if (i == 2)
      return;

   const TR::ILOpCode origOp = test->getOpCode();
   const TR::ILOpCode ifOp = i == 0 ? origOp : origOp.getOpCodeForSwapChildren();
   TR::Node * const left = test->getChild(i);
   TR::Node * const right = test->getChild(1 - i);

   if (left->getReferenceCount() == 1)
      return;

   TR::Node * const updated = store->getChild(0);
   if (!updated->getOpCode().isAdd() && !updated->getOpCode().isSub())
      return;

   if (!updated->cannotOverflow())
      return;

   TR::Node * const updateBase = updated->getChild(0);
   if (!isLoadVarDirectOf(updateBase, symrefNum))
      return;

   TR::Node * const addendNode = updated->getChild(1);
   if (!addendNode->getOpCode().isLoadConst())
      return;

   // We have exactly one of isCompareTrueIfLess and isCompareTrueIfGreater
   // because isCompareForOrder was true for test, so include just one.
   const int ifOpIdx =
      int(ifOp.isCompareTrueIfGreater()) << 1 |
      int(ifOp.isCompareTrueIfEqual());

   //                              ifOp :    <, <=,  >, >=
   static const int8_t usableAddends[4] = { +1, -1, -1, +1 };
   const int64_t arithSign = updated->getOpCode().isAdd() ? 1 : -1;
   const int64_t addend = arithSign * addendNode->getConstValue();
   if (addend != usableAddends[ifOpIdx])
      return;

   // Transformation is possible if left has the same value as updateBase. Of
   // course they can only have different values if they are different nodes.
   if (left != updateBase)
      {
      if (trace())
         {
         traceMsg(comp(),
            "Post- to pre-increment transformation looking for store of #%d "
            "between n%un and n%un.\n\tEvaluation order:",
            symrefNum,
            left->getGlobalIndex(),
            updateBase->getGlobalIndex());
         }

      // They have the same value iff symref is not stored between them. This
      // determination relies on the fact that (left != updateBase), since
      // otherwise there would only be one point of evaluation.
      bool between = false;
      TR::TreeTop * const start = block->startOfExtendedBlock()->getEntry();
      for (TR::PostorderNodeIterator it(start, comp()); true; ++it)
         {
         if (it == testTree)
            {
            // Reaching testTree shouldn't be possible, because there is a
            // store between updateBase and testTree.
            TR_ASSERT(false, "post- to pre-increment analysis reached test tree\n");
            return; // In production, just bail out
            }

         TR::Node * const eval = it.currentNode();
         if (eval == left || eval == updateBase)
            {
            if (trace())
               traceMsg(comp(), " n%un", eval->getGlobalIndex());

            if (between)
               break; // Same value - go on to transform.
            else
               between = true;
            }

         if (between
             && eval->getOpCode().isStoreDirect()
             && eval->getSymbolReference()->getReferenceNumber() == symrefNum)
            {
            // Can't transform
            if (trace())
               {
               traceMsg(comp(),
                  " n%un\n\tBailing due to store between loads\n",
                  eval->getGlobalIndex());
               }
            return;
            }
         }
      }

   if (trace())
      traceMsg(comp(), "\n");

   // adjustedIfOp will be the same as ifOp, with inverted isCompareTrueIfEqual.
   // This works because ifOp is an inequality.
   const TR::ILOpCode ifNotOp = ifOp.getOpCodeForReverseBranch();
   const TR::ILOpCode adjustedIfOp = ifNotOp.getOpCodeForSwapChildren();

   if (!performTransformation(comp(),
         "%sChanging n%un (equivalently %s old-#%d n%un) to (%s n%un n%un)\n",
         optDetailString(),
         test->getGlobalIndex(),
         ifOp.getName(),
         symrefNum,
         right->getGlobalIndex(),
         adjustedIfOp.getName(),
         updated->getGlobalIndex(),
         right->getGlobalIndex()))
      return;

   // Success! Set both children because they may have been swapped for the
   // purpose of analysis above. Also, left does not need to be anchored
   // because it must have been evaluated strictly before testTree. (Otherwise,
   // store would be evaluated between updateBase and left.)
   TR::Node::recreate(test, adjustedIfOp.getOpCodeValue());
   test->setAndIncChild(0, updated);
   test->setAndIncChild(1, right);
   left->recursivelyDecReferenceCount();
   right->recursivelyDecReferenceCount();
   }

const char *
TR_LoopCanonicalizer::optDetailString() const throw()
   {
   return "O^O LOOP CANONICALIZER: ";
   }

bool TR_LoopTransformer::replaceAllInductionVariableComputations(TR::Block *loopInvariantBlock, TR_Structure *structure, TR::SymbolReference **newSymbolReference, TR::SymbolReference *inductionVarSymRef)
   {
   bool seenInductionVariableComputation = false;
   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();

      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();

         if (examineTreeForInductionVariableUse(loopInvariantBlock, NULL, -1, currentNode, comp()->getVisitCount(), newSymbolReference))
            seenInductionVariableComputation = true;


         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;

      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         if (replaceAllInductionVariableComputations(loopInvariantBlock, node->getStructure(), newSymbolReference, inductionVarSymRef))
            seenInductionVariableComputation = true;
         }
      }

   return seenInductionVariableComputation;
   }




// Used to clone nodes within a block where parent/child links are to be
// duplicated exactly for multiply referenced nodes
//
TR::Node *TR_LoopTransformer::duplicateExact(TR::Node *node, List<TR::Node> *seenNodes, List<TR::Node> *duplicateNodes)
   {
   vcount_t visitCount = comp()->getVisitCount();
   if (visitCount == node->getVisitCount())
      {
      // We've seen this node before - find its duplicate
      //
      ListIterator<TR::Node> seenNodesIt(seenNodes);
      ListIterator<TR::Node> duplicateNodesIt(duplicateNodes);
      TR::Node *nextDuplicateNode = duplicateNodesIt.getFirst();
      TR::Node *nextNode;
      for (nextNode = seenNodesIt.getFirst(); nextNode != NULL; nextNode = seenNodesIt.getNext())
         {
         if (nextNode == node)
            {
            nextDuplicateNode->incReferenceCount();
            return nextDuplicateNode;
            }
         nextDuplicateNode = duplicateNodesIt.getNext();
         }
      TR_ASSERT(nextNode == NULL,"Loop canonicalizer, error duplicating trees");
      }

   TR::Node *newRoot = TR::Node::copy(node);
   if (node->getOpCode().hasSymbolReference())
      {
      newRoot->setSymbolReference(node->getSymbolReference());
      }

   newRoot->setReferenceCount(1);
   node->setVisitCount(visitCount);
   if (node->getReferenceCount() > 1)
      {
      duplicateNodes->add(newRoot);
      seenNodes->add(node);
      }

   for (int i = 0; i < node->getNumChildren(); i++)
      newRoot->setChild(i, duplicateExact(node->getChild(i), seenNodes, duplicateNodes));

   return newRoot;
   }


void TR_LoopTransformer::adjustTreesInBlock(TR::Block *block)
   {
   TR::TreeTop *entryTree = block->getEntry();
   TR::TreeTop *exitTree = block->getExit();

   entryTree->getNode()->setBlock(block);
   exitTree->getNode()->setBlock(block);
   }


void TR_LoopTransformer::printTrees()
   {
   comp()->incVisitCount();
   TR::TreeTop *currentTree = comp()->getStartTree();

   while (!(currentTree == NULL))
      {
      if (trace())
         getDebug()->print(comp()->getOutFile(), currentTree);

      currentTree = currentTree->getNextTreeTop();
      }
   if (trace())
      getDebug()->print(comp()->getOutFile(), comp()->getFlowGraph());
   }

/**
 * MUST do this step if there are any blocks to be cleansed as otherwise the block
 * would violate the CFG constraint that it can contain only one branch instruction.
 * The goto inserted at the end earlier in canonicalization is removed and the target
 * block is made the fall through block.
 */
bool TR_LoopTransformer::cleanseTrees(TR::Block *block)
   {
   if (_cfg == NULL)
      return false;

   bool        treesWereCleansed = false;
   //TR::TreeTop *endTree = comp()->getMethodSymbol()->getLastTreeTop();
   TR::TreeTop *treeTop;

   TR::TreeTop *exitTreeTop = NULL;
   treeTop = block->getEntry();
   // for (treeTop = comp()->getStartTree();
   //        treeTop != NULL;
   //        treeTop = exitTreeTop->getNextTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      TR::Block *block = node->getBlock();
      exitTreeTop     = block->getExit();

      TR::TreeTop *tt1 = block->getLastRealTreeTop();

      TR::Node *node1 = tt1->getNode();
      if (node1->getOpCode().getOpCodeValue() == TR::Goto)
         {
         TR::TreeTop *targetTree = node1->getBranchDestination();
         TR::TreeTop *prevTreeOfTarget = NULL;
         // if (targetTree->getPrevTreeTop() != NULL)
            {
            TR_ASSERT(targetTree->getPrevTreeTop()->getNode()->getOpCodeValue() == TR::BBEnd, "BBStart is not preceded by a BBEnd");
            prevTreeOfTarget = targetTree->getPrevTreeTop()->getNode()->getBlock()->getLastRealTreeTop();
            }

         bool isPrevTreeOfTargetReturn = prevTreeOfTarget->getNode()->getOpCode().isReturn();
         if ((prevTreeOfTarget->getNode()->getOpCode().isBranch() &&
             ((prevTreeOfTarget->getNode()->getOpCodeValue() == TR::Goto) || (prevTreeOfTarget->getNode()->getBranchDestination() == targetTree))) ||
             isPrevTreeOfTargetReturn ||
             prevTreeOfTarget->getNode()->getOpCode().isSwitch() ||
             prevTreeOfTarget->getNode()->getOpCode().isJumpWithMultipleTargets() ||
             (prevTreeOfTarget->getNode()->getOpCodeValue() == TR::athrow) ||
             (prevTreeOfTarget->getNode()->getOpCode().isTreeTop() &&
              (prevTreeOfTarget->getNode()->getFirstChild()->getOpCodeValue() == TR::athrow)))
            {
           //if (isPrevTreeOfTargetReturn || ((prevTreeOfTarget->getNode()->getOpCode().getOpCodeValue() == TR::Goto) || (prevTreeOfTarget->getNode()->getBranchDestination() == targetTree)))
               {
               // Can move the trees between the goto and the target
               // to the end of the method if reqd and eliminate the goto.
               //
               if (exitTreeTop->getNextTreeTop() != targetTree)
                  {
                  TR::TreeTop *startOfTreesBeingMoved = exitTreeTop->getNextTreeTop();
                  TR::TreeTop *endOfTreesBeingMoved = targetTree->getPrevTreeTop();

                  TR::Block *placeToMoveTrees = targetTree->getNode()->getBlock();
                  TR::Block *prevBlock = NULL;
                  while (placeToMoveTrees && placeToMoveTrees->hasSuccessor(placeToMoveTrees->getNextBlock()))
           {
           //dumpOptDetails(comp(), "Block %d\n", placeToMoveTrees->getNumber());
           prevBlock = placeToMoveTrees;
           placeToMoveTrees = placeToMoveTrees->getNextBlock();
           }

                  if (!placeToMoveTrees)
           placeToMoveTrees = prevBlock;

                  TR::TreeTop *endTree = placeToMoveTrees->getExit();
                  TR::TreeTop *treeNextToEnd = endTree->getNextTreeTop();
                  exitTreeTop->join(targetTree);
                  endTree->join(startOfTreesBeingMoved);
                  endTree = endOfTreesBeingMoved;
                  if (treeNextToEnd)
                     endOfTreesBeingMoved->join(treeNextToEnd);
                  else
                     endOfTreesBeingMoved->setNextTreeTop(treeNextToEnd);
                  }

               treesWereCleansed = true;
               tt1->getPrevTreeTop()->join(tt1->getNextTreeTop());
               }
            }
         }
      }

   return treesWereCleansed;
   }

/**
 * Do this step to try to maximize occurrences of fall through from invariant
 * block into the loop body.
 */
bool TR_LoopTransformer::makeInvariantBlockFallThroughIfPossible(TR::Block *block)
   {
   if (_cfg == NULL)
      return false;

   bool        treesWereMoved = false;
   TR::TreeTop *treeTop;

   TR::TreeTop *exitTreeTop = NULL;
   treeTop = block->getEntry();
   // for (treeTop = comp()->getStartTree();
   //        treeTop != NULL;
   //        treeTop = exitTreeTop->getNextTreeTop())
      {
      // Get information about this block
      //
      TR::Node *node = treeTop->getNode();
      TR::Block *block = node->getBlock();
      exitTreeTop     = block->getExit();

      TR::TreeTop *tt1 = block->getLastRealTreeTop();

      TR::Node *node1 = tt1->getNode();
      if (node1->getOpCode().getOpCodeValue() == TR::Goto)
         {
         TR::TreeTop *targetTree = node1->getBranchDestination();
         TR::TreeTop *prevTreeOfTarget = NULL;
         // if (targetTree->getPrevTreeTop() != NULL)
            {
            TR_ASSERT(targetTree->getPrevTreeTop()->getNode()->getOpCodeValue() == TR::BBEnd, "BBStart is not preceded by a BBEnd");
            prevTreeOfTarget = targetTree->getPrevTreeTop()->getNode()->getBlock()->getLastRealTreeTop();
            }

         bool isPrevTreeOfTargetReturn = prevTreeOfTarget->getNode()->getOpCode().isReturn();
         if ((prevTreeOfTarget->getNode()->getOpCode().isBranch() &&
             ((prevTreeOfTarget->getNode()->getOpCodeValue() == TR::Goto) || (prevTreeOfTarget->getNode()->getBranchDestination() == targetTree))) ||
             isPrevTreeOfTargetReturn ||
             (prevTreeOfTarget->getNode()->getOpCodeValue() == TR::athrow))
            {
            //if (isPrevTreeOfTargetReturn || (prevTreeOfTarget->getNode()->getBranchDestination() != targetTree))
               {
               TR::TreeTop *prevTreeOfBlock = NULL;
               bool allPredsBranchExplicitly = true;
               if (treeTop->getPrevTreeTop())
                  {
                  prevTreeOfBlock = treeTop->getPrevTreeTop()->getNode()->getBlock()->getLastRealTreeTop();
                  TR::Node *prevNode = prevTreeOfBlock->getNode();
                  if (!prevNode->getOpCode().isReturn())
                     {
                     if (!prevNode->getOpCode().isBranch() || ((prevNode->getOpCodeValue() != TR::Goto) && (prevNode->getBranchDestination() != treeTop)))
                        allPredsBranchExplicitly = false;
                     }
                  }

               // Can move the trees between the goto and the target
               // to the end of the method if reqd and eliminate the goto.
               //
               if ((exitTreeTop->getNextTreeTop() != targetTree) && allPredsBranchExplicitly)
                  {
                  if (trace())
                     traceMsg(comp(), "Moving invariant block_%d to fall through into loop %d\n", block->getNumber(), targetTree->getNode()->getBlock()->getNumber());
                  TR::TreeTop *nextTreeAfterBlock = exitTreeTop->getNextTreeTop();
                  TR::TreeTop *prevTreeBeforeBlock = treeTop->getPrevTreeTop();
                  prevTreeBeforeBlock->join(nextTreeAfterBlock);
                  targetTree->getPrevTreeTop()->join(treeTop);
                  exitTreeTop->join(targetTree);
                  treesWereMoved = true;
                  }
               }
            }
         }
      }

   return treesWereMoved;
   }


bool TR_LoopTransformer::isStoreInRequiredForm(int32_t symRefNum, TR_Structure *loopStructure)
   {
   /* [101214] calls too can kill symRefs via loadaddr */
   if (symRefNum != 0 && _allKilledSymRefs[symRefNum] == true )
      return false;

   if (!comp()->getSymRefTab()->getSymRef(symRefNum)->getSymbol()->isAutoOrParm())
      return false;

   TR::Node *storeNode = _storeTrees[symRefNum]->getNode();
   if (!storeNode->getType().isInt32() && !storeNode->getType().isInt64())
      return false;


   TR::Node *addNode = storeNode->getFirstChild();
   if (storeNode->getFirstChild()->getOpCode().isConversion() &&
       storeNode->getFirstChild()->getFirstChild()->getOpCode().isConversion())
      {
      TR::Node *conversion1 = storeNode->getFirstChild();
      TR::Node *conversion2 = storeNode->getFirstChild()->getFirstChild();
      if ((conversion1->getOpCodeValue() == TR::s2i &&
           conversion2->getOpCodeValue() == TR::i2s) ||
          (conversion1->getOpCodeValue() == TR::b2i &&
           conversion2->getOpCodeValue() == TR::i2b) ||
          (conversion1->getOpCodeValue() == TR::su2i &&
           conversion2->getOpCodeValue() == TR::i2s))
         addNode = conversion2->getFirstChild();
      }

   _incrementInDifferentExtendedBlock = false;
   _constNode = containsOnlyInductionVariableAndAdditiveConstant(addNode, symRefNum);
   if (_constNode == NULL)
      {
       if (!_indirectInductionVariable) return false;

      _loadUsedInLoopIncrement = NULL;
      // Check: it might be an induction variable incremented indirectly
      TR_InductionVariable *v = loopStructure->asRegion()->findMatchingIV(comp()->getSymRefTab()->getSymRef(symRefNum));
      if (!v) return false;

      _isAddition = true;
      TR::VPConstraint * incr = v->getIncr();
      int64_t low;

      if (incr->asIntConst())
         {
         low = (int64_t) incr->getLowInt();
         _constNode = TR::Node::create(storeNode, TR::iconst, 0, (int32_t)low);
         }
      else if (incr->asLongConst())
         {
         low = incr->getLowLong();
         _constNode = TR::Node::create(storeNode, TR::lconst, 0, 0);
         _constNode->setLongInt(low);
         }
      else
         return false;

      if (trace())
         {
         traceMsg(comp(), "Found loop induction variable #%d incremented indirectly by %lld\n", symRefNum, low);
         }
      }
   else
      {
      TR::Node *secondChild = _constNode;
      if (secondChild->getOpCode().isLoadVarDirect())
         {
         int32_t timesWritten = 0;
         if (!isSymbolReferenceWrittenNumberOfTimesInStructure(loopStructure, secondChild->getSymbolReference()->getReferenceNumber(), &timesWritten, 0))
            return false;
         }
      else if (!secondChild->getOpCode().isLoadConst())
         return false;

      if (secondChild->getOpCode().isLoadConst() &&
          ((secondChild->getType().isInt32() && (secondChild->getInt() < 0)) ||
           (secondChild->getType().isInt64() && (secondChild->getLongInt() < 0))))
         {
         if (_isAddition)
            _isAddition = false;
         else
            _isAddition = true;
         }

      _constNode = _constNode->duplicateTree();
      _constNode->setReferenceCount(0);
      }

   _loopDrivingInductionVar = symRefNum;
   _insertionTreeTop = _storeTrees[symRefNum];

   return true;
   }

int32_t TR_LoopTransformer::getInductionSymbolReference(TR::Node *node)
   {
   if (node->getOpCode().hasSymbolReference() && node->getOpCode().isLoadVarDirect())
      {
      return node->getSymbolReference()->getReferenceNumber();
      }

   return -1;
   }

int32_t TR_LoopTransformer::checkLoopForPredictability(TR_Structure *loopStructure, TR::Block *loopInvariantBlock, TR::Node **numberOfIterations, bool returnIfNotPredictable)
   {
   int32_t doWhileNumber = loopStructure->getNumber();
   updateInfo_tables uinfo(comp()->allocator());

   TR::CFGNode *next;
   for (next = comp()->getFlowGraph()->getFirstNode(); next; next = next->getNext())
      {
      int32_t i = toBlock(next)->getNumber();

      if (i == doWhileNumber)
         break;
      }

   int32_t result = 1;
   for (auto nextEdge = next->getPredecessors().begin(); nextEdge != next->getPredecessors().end(); ++nextEdge)
      {
      TR::Block *nextBlock = toBlock((*nextEdge)->getFrom());
      if (nextBlock != loopInvariantBlock)
         {
         //// Back edge found
         ////
         _loopTestBlock = nextBlock;

         if (nextBlock->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
            _loopTestTree = nextBlock->getLastRealTreeTop();
         else
            {
            if (returnIfNotPredictable)
               return -1;
            else
               result = -1;
            }

         vcount_t visitCount = comp()->incVisitCount();
         collectSymbolsWrittenAndReadExactlyOnce(loopStructure, visitCount, uinfo);
         }
      }

   return result;
   }

void TR_LoopTransformer::updateInfo(TR::Node *node, vcount_t visitCount, updateInfo_tables &uinfo)
   {
   // traceMsg(comp(), "TR_LoopTransformer::updateInfo node=%p\n", node);
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   uint32_t refNo = 0;
   if (node->getOpCode().hasSymbolReference())
      {
      refNo = node->getSymbolReference()->getReferenceNumber();
      _allSymRefs[refNo] = true;
      }

//   traceMsg(comp(), "bf _allKilledSymRefs = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &_allKilledSymRefs);
//   traceMsg(comp(), "\n");
   TR::BitVector mayKillAliases(comp()->allocator());
   node->mayKill().getAliases(mayKillAliases);
   if(refNo != 0)
      mayKillAliases[refNo] = false;
//   traceMsg(comp(), "mayKillAliases(w/o self) = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &mayKillAliases);
//   traceMsg(comp(), "\n");
   _allKilledSymRefs.Or(mayKillAliases);

//   traceMsg(comp(), "af _allKilledSymRefs = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &_allKilledSymRefs);
//   traceMsg(comp(), "\n");

   TR::BitVector defAliases(comp()->allocator());

   if ((node->getOpCode().isStore()
         && !node->isTheVirtualCallNodeForAGuardedInlinedCall())
         || !_doingVersioning)
      {
      if (refNo == 0 || !uinfo.seenMultipleStores[refNo])
         node->mayKill().getAliases(defAliases);
      }


   // update writes
   *_neverWritten -= defAliases;

//   traceMsg(comp(), "defAliases = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &defAliases);
//   traceMsg(comp(), "\n");
//   traceMsg(comp(), "bf _writtenExactlyOnce = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &_writtenExactlyOnce);
//   traceMsg(comp(), "\n");
//   traceMsg(comp(), "bf uinfo.currentlyWrittenOnce = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &(uinfo.currentlyWrittenOnce));
//   traceMsg(comp(), "\n");
   // For most store operations
   if (refNo && node->getOpCode().isLikeDef() && defAliases.PopulationCount() > 1)
      {
      // FIXME: there seems to be an assumption that for every symbol
      // written exactly once, we'll have an entry in _storeTrees indexed
      // by the symref num.  However, since we have a single store, and
      // multiple aliases, that property gets broken.  Just be conservative
      // when there are multiple aliases.
      _writtenExactlyOnce -= defAliases;
      }
   else if (node->getOpCode().isStore())        // For all other store operations
      {
      _writtenExactlyOnce.Andc(defAliases);              // _writtenExactlyOnce -= defAliases
      TR::BitVector tmp(comp()->allocator());
      defAliases.Andc(uinfo.currentlyWrittenOnce, tmp);  // tmp = defAliases - uinfo.currentlyWrittenOnce
      _writtenExactlyOnce.Or(tmp);                       // _writtenExactlyOnce += tmp
      }
   else
      {
      TR::BitVector tmp(comp()->allocator());
      defAliases.And(uinfo.currentlyWrittenOnce, tmp);  // tmp = defAliases * uinfo.currentlyWrittenOnce
      _writtenExactlyOnce.Andc(tmp);                    // _writtenExactlyOnce -= tmp
      }

   uinfo.currentlyWrittenOnce |= defAliases;

   // use def aliases for volatiles to update reads and writes
   if (node->mightHaveVolatileSymbolReference())
      {
      *_neverRead -= defAliases;
      _readExactlyOnce -= defAliases;
      _writtenExactlyOnce -= defAliases;
      }

//   traceMsg(comp(), "af _writtenExactlyOnce = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &_writtenExactlyOnce);
//   traceMsg(comp(), "\n");
//   traceMsg(comp(), "af uinfo.currentlyWrittenOnce = ");
//   comp()->getDebug()->print(comp()->getOutFile(), &(uinfo.currentlyWrittenOnce));
//   traceMsg(comp(), "\n");

   // update reads
   TR::BitVector useAliases(comp()->allocator());

   if (refNo == 0 || !uinfo.seenMultipleLoads[refNo])
      node->mayUse().getAliases(useAliases);

   *_neverRead -= useAliases;

   if (node->getReferenceCount() > 1)
      {
      _readExactlyOnce -= useAliases;
      uinfo.currentlyReadOnce.Or(useAliases);
      }
   else
      {
      _readExactlyOnce.Andc(useAliases);

      TR::BitVector tmp(comp()->allocator());
      useAliases.Andc(uinfo.currentlyReadOnce, tmp);
      _readExactlyOnce.Or(tmp);

      uinfo.currentlyReadOnce.Or(useAliases);
      }

   // update other info
   bool heapificationStore = node->getOpCodeValue() == TR::astore
         && node->isHeapificationStore();
   if (!heapificationStore && _writtenAndNotJustForHeapification)
      *_writtenAndNotJustForHeapification |= defAliases;

   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t symRefNum = node->getSymbolReference()->getReferenceNumber();
      if (_autosAccessed && symRef->getSymbol()->isAutoOrParm())
         _autosAccessed->set(symRefNum);
      }

   if (node->getOpCode().isStore())
      {
      updateStoreInfo(node->getSymbolReference()->getReferenceNumber(), _currTree);
      }

   if (refNo)
      {
      if (node->getOpCode().isStore())
         {
         if (uinfo.seenStores[refNo])
            uinfo.seenMultipleStores[refNo] = true;
         else
            uinfo.seenStores[refNo] = true;
         }
      else
         {
         if (uinfo.seenLoads[refNo] || node->getReferenceCount() > 1)
            uinfo.seenMultipleLoads[refNo] = true;
         uinfo.seenLoads[refNo] = true;
         }
      }

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      updateInfo(node->getChild(i), visitCount, uinfo);
   }

void TR_LoopTransformer::collectSymbolsWrittenAndReadExactlyOnce(TR_Structure *structure, vcount_t visitCount)
   {
   updateInfo_tables uinfo(comp()->allocator());
   collectSymbolsWrittenAndReadExactlyOnce(structure, visitCount, uinfo);
   }

void TR_LoopTransformer::collectSymbolsWrittenAndReadExactlyOnce(TR_Structure *structure, vcount_t visitCount, updateInfo_tables &uinfo)
   {
   if (structure->asBlock() != NULL)
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();

      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         //traceMsg(comp(), "Looking at node %p in block_%d\n", currentTree->getNode(), block->getNumber());
         TR::Node *currentNode = currentTree->getNode();
         _currTree = currentTree;
         _numberOfTreesInLoop++;
         updateInfo(currentNode, visitCount, uinfo);
         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         collectSymbolsWrittenAndReadExactlyOnce(node->getStructure(), visitCount, uinfo);
         }
      }
   }

TR_LoopTransformer::TR_TransformerDefUseState TR_LoopTransformer::getSymbolDefUseStateInSubTree(TR::Node *node, TR::RegisterMappedSymbol* indVarSym)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return transformerNoReadOrWrite;

   TR_TransformerDefUseState subTreeResult;
   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      subTreeResult = getSymbolDefUseStateInSubTree(node->getChild(i), indVarSym);
      if (subTreeResult != transformerNoReadOrWrite)
         {
         return subTreeResult;
         }
      }

   node->setVisitCount(comp()->getVisitCount());

   TR::ILOpCode &opCode = node->getOpCode();
   if (opCode.isLoadVar() && opCode.hasSymbolReference())
      {
      TR::RegisterMappedSymbol *sym = node->getSymbol()->getRegisterMappedSymbol();
      if (sym == indVarSym)
         {
         return transformerReadFirst;
         }
      }
   else if (opCode.isStore() && opCode.hasSymbolReference())
      {
      TR::RegisterMappedSymbol *sym = node->getSymbol()->getRegisterMappedSymbol();
      if (sym == indVarSym)
         {
         return transformerWrittenFirst;
         }
      }

   return transformerNoReadOrWrite;
   }

TR_LoopTransformer::TR_TransformerDefUseState TR_LoopTransformer::getSymbolDefUseStateInBlock(TR::Block *block, TR::RegisterMappedSymbol* indVarSym)
   {
   for (TR::TreeTop* tree = block->getFirstRealTreeTop(); tree != block->getExit(); tree = tree->getNextTreeTop())
      {
      if (tree->getNode()->getOpCodeValue() == TR::asynccheck)
         continue;

      TR_TransformerDefUseState blockResult;
      blockResult = getSymbolDefUseStateInSubTree(tree->getNode(), indVarSym);
      if (blockResult != transformerNoReadOrWrite)
         {
         return blockResult;
         }
      }
   return transformerNoReadOrWrite;
   }

bool TR_LoopTransformer::isSymbolReferenceWrittenNumberOfTimesInStructure(TR_Structure *structure, int32_t symRefNum, int32_t *numTimesSeen, int32_t maxTimes)
   {
   if (structure->asBlock() != NULL)
      {
      if (comp()->getSymRefTab()->getSymRef(symRefNum)->getSymbol()->isVolatile())
         return false;

      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();

         TR::Node *defNode = currentNode->getStoreNode();
         if (!defNode)
            {
            defNode = currentNode;
            if (currentNode->getOpCodeValue() == TR::treetop ||
                  currentNode->getOpCode().isCheck())
               {
               defNode = currentNode->getFirstChild();
               }
            }

            {
            TR::SparseBitVector defAliases(comp()->allocator());
            defNode->mayKill().getAliases(defAliases);
            bool sameSym = (defAliases.ValueAt(symRefNum) != 0);

            if (sameSym)
               {
               (*numTimesSeen)++;
               if ((*numTimesSeen) > maxTimes)
                  return false;
               }

            if (!defNode->isTheVirtualCallNodeForAGuardedInlinedCall() ||
            //if (!storeOrCallNode->isTheVirtualGuardForAGuardedInlinedCall() ||
                !_doingVersioning)
               {
               TR::SparseBitVector::Cursor defAliasesCursor(defAliases);
               for (defAliasesCursor.SetToFirstOne(); defAliasesCursor.Valid(); defAliasesCursor.SetToNextOne())
                  {
                  int32_t nextAlias = defAliasesCursor;
                  if (nextAlias == symRefNum)
                     {
                     (*numTimesSeen)++;
                     if ((*numTimesSeen) > maxTimes)
                        return false;
                     }
                  }
               }
            }

         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         if (!isSymbolReferenceWrittenNumberOfTimesInStructure(node->getStructure(), symRefNum, numTimesSeen, maxTimes))
            return false;
         }
      }

   return true;
   }






TR::Node* TR_LoopTransformer::getCorrectNumberOfIterations(TR::Node *node, TR::Node *constNode)
   {
   TR::Node *subNode;
   switch (node->getOpCodeValue())
      {
      case TR::ificmplt:
         {
         if (!_isAddition)
            return 0;

         subNode = TR::Node::create(TR::isub, 2, node->getSecondChild()->duplicateTree(), node->getFirstChild()->duplicateTree());
         break;
         }
      case TR::ificmple:
         {
         //if (!_isAddition)
            return 0;

         //subNode = TR::Node::create(TR::isub, 2, node->getSecondChild()->duplicateTree(), node->getFirstChild()->duplicateTree());
         //subNode = TR::Node::create(TR::iadd, 2, subNode, TR::Node::create(node, TR::iconst, 0, 1));
         //break;
         }
      case TR::ificmpgt:
         {
         if (_isAddition)
            return 0;

         subNode = TR::Node::create(TR::isub, 2, node->getFirstChild()->duplicateTree(), node->getSecondChild()->duplicateTree());
         break;
         }
      case TR::ificmpge:
         {
         //if (_isAddition)
            return 0;

         //subNode = TR::Node::create(TR::isub, 2, node->getFirstChild()->duplicateTree(), node->getSecondChild()->duplicateTree());
         //subNode = TR::Node::create(TR::iadd, 2, subNode, TR::Node::create(node, TR::iconst, 0, 1));
         //break;
         }
      default:
         return NULL;
      }

   return TR::Node::create(TR::idiv, 2, subNode, constNode);
   }




TR::Node *TR_LoopTransformer::updateLoadUsedInLoopIncrement(TR::Node *node, int32_t inductionVariable)
   {
   TR::ILOpCode &opCode = node->getOpCode();
   if (_indirectInductionVariable &&
       opCode.isLoadVar() &&
       _writtenExactlyOnce.ValueAt(node->getSymbolReference()->getReferenceNumber()))
      {
      TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
      if (!useDefInfo)
         return NULL;

      uint16_t useIndex = node->getUseDefIndex();
      if (!useIndex || !useDefInfo->isUseIndex(useIndex))
         return NULL;

      TR_UseDefInfo::BitVector defs(comp()->allocator());
      if (useDefInfo->getUseDef(defs, useIndex) && (defs.PopulationCount() == 1))
         {
         TR_UseDefInfo::BitVector::Cursor cursor(defs);
         for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
            {
            int32_t defIndex = cursor;
            if (defIndex < useDefInfo->getFirstRealDefIndex())
               return NULL;

            TR::Node *defNode = useDefInfo->getNode(defIndex);
            if (!defNode->getOpCode().isStore())
               continue;

            TR::Node * constNode = containsOnlyInductionVariableAndAdditiveConstant(defNode->getFirstChild(), inductionVariable);
            if (constNode)
               {
               checkIfIncrementInDifferentExtendedBlock(useDefInfo->getTreeTop(defIndex)->getEnclosingBlock()->startOfExtendedBlock(), inductionVariable);
               }

            return constNode;
            }
         }
      return NULL;
      }

   return NULL;
   }



TR::Node* TR_LoopTransformer::containsOnlyInductionVariableAndAdditiveConstant(TR::Node *node, int32_t inductionVariable)
   {
   TR::ILOpCode &opCode = node->getOpCode();

   if (opCode.isAdd())
      {
      _isAddition = true;
      if (node->getFirstChild()->getOpCode().hasSymbolReference() &&
          (node->getFirstChild()->getSymbolReference()->getReferenceNumber() == inductionVariable))
         {
         _loadUsedInLoopIncrement = node->getFirstChild();
         return node->getSecondChild();
         }
      }
   else if (opCode.isSub())
      {
      _isAddition = false;
      if (node->getFirstChild()->getOpCode().hasSymbolReference() &&
          (node->getFirstChild()->getSymbolReference()->getReferenceNumber() == inductionVariable))
         {
         _loadUsedInLoopIncrement = node->getFirstChild();
         return node->getSecondChild();
         }
      }
   else
      return updateLoadUsedInLoopIncrement(node, inductionVariable);

   return NULL;
   }



void TR_LoopTransformer::checkIfIncrementInDifferentExtendedBlock(TR::Block *block, int32_t inductionVariable)
   {
   if (block !=
       _storeTrees[inductionVariable]->getEnclosingBlock()->startOfExtendedBlock())
      {
      _incrementInDifferentExtendedBlock = true;
      }
   }


bool TR_LoopTransformer::blockIsAlwaysExecutedInLoop(TR::Block *currentBlock, TR_RegionStructure *loopStructure, bool *atEntry)
   {
   TR::Block *entryBlock = loopStructure->asRegion()->getEntryBlock();
   if ((currentBlock == _loopTestBlock) ||
       (currentBlock == entryBlock))
      {
      if (atEntry)
         {
         if (currentBlock == entryBlock)
            *atEntry = true;
         else
            *atEntry = false;
         }

      return true;
      }

   if ((currentBlock->getSuccessors().size() == 1) &&
       (currentBlock->getSuccessors().front()->getTo() == _loopTestBlock))
      {
      if (atEntry)
         *atEntry = false;
      return true;
      }

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   loopStructure->getBlocks(&blocksInRegion);

   TR_ScratchList<TR::Block> seenBlocks(trMemory());

   bool flag = true;
   TR::Block *cursorBlock = currentBlock;
   while (flag)
      {
      flag = false;
      TR::Block *onlySuccInsideLoop = NULL;
      seenBlocks.add(cursorBlock);
      for (auto succ = cursorBlock->getSuccessors().begin(); succ != cursorBlock->getSuccessors().end(); ++succ)
         {
         TR::Block *succBlock = (*succ)->getTo()->asBlock();
         if (blocksInRegion.find(succBlock))
            {
            if (!onlySuccInsideLoop)
               onlySuccInsideLoop = succBlock;
            else
               {
               onlySuccInsideLoop = NULL;
               break;
               }
            }
         }
      if (onlySuccInsideLoop)
         {
         cursorBlock = onlySuccInsideLoop;
         flag = true;
         }

      if (flag)
         {
         if ((cursorBlock == _loopTestBlock) ||
             (cursorBlock == entryBlock))
            {
            if (atEntry)
               *atEntry = false;
            return true;
            }

         if (seenBlocks.find(cursorBlock))
            return false;
         }
      }


   seenBlocks.deleteAll();

   flag = true;
   cursorBlock = currentBlock;
   while (flag)
      {
      flag = false;
      seenBlocks.add(cursorBlock);
      TR::Block *predInsideLoop = NULL;
      for (auto pred = cursorBlock->getPredecessors().begin(); pred != cursorBlock->getPredecessors().end(); ++pred)
         {
         TR::Block *predBlock = (*pred)->getFrom()->asBlock();
         if (blocksInRegion.find(predBlock))
            {
            if (!predInsideLoop)
               predInsideLoop = predBlock;
            else
               {
               predInsideLoop = NULL;
               break;
               }
            }
         }

      if (predInsideLoop)
         {
         TR::Block *onlySuccInsideLoop = NULL;
         for (auto succ = predInsideLoop->getSuccessors().begin(); succ != predInsideLoop->getSuccessors().end(); ++succ)
            {
            TR::Block *succBlock = (*succ)->getTo()->asBlock();
            if (blocksInRegion.find(succBlock))
               {
               if (!onlySuccInsideLoop)
                  onlySuccInsideLoop = succBlock;
               else
                  {
                  onlySuccInsideLoop = NULL;
                  break;
                  }
               }
            }
         if (!onlySuccInsideLoop)
            predInsideLoop = NULL;
         }

      if (predInsideLoop)
         {
         cursorBlock = predInsideLoop;
         flag = true;
         }

      if (flag)
         {
         if ((cursorBlock == _loopTestBlock) ||
             (cursorBlock == entryBlock))
            {
            if (atEntry)
               *atEntry = false;
            return true;
            }

         if (seenBlocks.find(cursorBlock))
            return false;
         }
      }

   return false;
   }


/******************************************/

int32_t TR_RedundantInductionVarElimination::perform()
   {

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());
   _cfg = comp()->getFlowGraph();
   _rootStructure = _cfg->getStructure();

   TR_ScratchList<TR_Structure> whileLoops(trMemory());
   ListAppender<TR_Structure> whileLoopsInnerFirst(&whileLoops);
   TR_ScratchList<TR_Structure> doWhileLoops(trMemory());
   ListAppender<TR_Structure> doWhileLoopsInnerFirst(&doWhileLoops);
   _nodesInCycle = new (trStackMemory()) TR_BitVector(_cfg->getNextNodeNumber(), trMemory(), stackAlloc);

   detectWhileLoops(whileLoopsInnerFirst, whileLoops, doWhileLoopsInnerFirst, doWhileLoops, _rootStructure, true);

   if (whileLoops.isEmpty() && doWhileLoops.isEmpty())
      {
      return false;
      }

   if (trace())
      traceMsg(comp(), "Number of WhileLoops = %d\n", whileLoops.getSize());

   if (trace())
      traceMsg(comp(), "Number of DoWhileLoops = %d\n", doWhileLoops.getSize());

   ListIterator<TR_Structure> whileLoopsIt(&whileLoops);
   TR_Structure *nextWhileLoop;

   for (nextWhileLoop = whileLoopsIt.getFirst(); nextWhileLoop != NULL; nextWhileLoop = whileLoopsIt.getNext())
      {
      TR_RegionStructure *naturalLoop = nextWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"redundantInductionVarElimination, expecting natural loop");
      if (naturalLoop->getEntryBlock()->isCold())
         continue;
      eliminateRedundantInductionVariablesFromLoop(naturalLoop);
      }

   ListIterator<TR_Structure> doWhileLoopsIt(&doWhileLoops);
   TR_Structure *nextDoWhileLoop;

   for (nextDoWhileLoop = doWhileLoopsIt.getFirst(); nextDoWhileLoop != NULL; nextDoWhileLoop = doWhileLoopsIt.getNext())
      {
      TR_RegionStructure *naturalLoop = nextDoWhileLoop->asRegion();
      TR_ASSERT(naturalLoop && naturalLoop->isNaturalLoop(),"redundantInductionVarElimination, expecting natural loop");
      if (naturalLoop->getEntryBlock()->isCold())
         continue;
      eliminateRedundantInductionVariablesFromLoop(naturalLoop);
      }

   return true;
   }

const char *
TR_RedundantInductionVarElimination::optDetailString() const throw()
   {
   return "O^O REDUNDANT INDUCTION VAR ELIMINATION: ";
   }

/*******************************************/




bool TR_LoopTransformer::detectEmptyLoop(TR_Structure *structure, int32_t *numTrees)
   {
   if (structure->asBlock())
      {
      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();

      if (*numTrees > 1)
         return false;

      TR::TreeTop *currentTree = block->getEntry()->getNextTreeTop();
      TR::TreeTop *exitTree = block->getExit();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();
         TR::ILOpCode opCode = currentNode->getOpCode();

         if (!opCode.isExceptionRangeFence() && !opCode.isBranch() && (opCode.getOpCodeValue() != TR::asynccheck))
             {
             (*numTrees)++;
             if (*numTrees > 1)
                return false;
             }

         currentTree = currentTree->getNextRealTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();

      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
          {
          if (!detectEmptyLoop(node->getStructure(), numTrees))
             return false;
          }
      }

   return true;
   }




TR_LoopInverter::TR_LoopInverter(TR::OptimizationManager *manager)
   : TR_LoopTransformer(manager)
   {}


int32_t TR_LoopInverter::perform()
   {
   static char *invert = feGetEnv("TR_enableInvert");
   if (!invert)
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   //_count = 0;

   detectCanonicalizedPredictableLoops(comp()->getFlowGraph()->getStructure(), NULL, -1);

   return 2;
   }


int32_t TR_LoopInverter::detectCanonicalizedPredictableLoops(TR_Structure *loopStructure, TR_BitVector **optSetInfo, int32_t bitVectorSize)
   {
   TR_RegionStructure *regionStructure = loopStructure->asRegion();

   if (regionStructure)
      {
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         detectCanonicalizedPredictableLoops(node->getStructure(), optSetInfo, bitVectorSize);
      }

   TR_BlockStructure *loopInvariantBlock = NULL;

   if (!regionStructure ||
       !regionStructure->getParent() ||
       !regionStructure->isNaturalLoop())
      return 0;

   regionStructure->resetInvariance();

   TR_ScratchList<TR::Block> blocksInRegion(trMemory());
   regionStructure->getBlocks(&blocksInRegion);
   ListIterator<TR::Block> blocksIt(&blocksInRegion);

   TR::Block *nextBlock;
   for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
      if (!nextBlock->getExceptionPredecessors().empty())
         return 0;

   TR_RegionStructure *parentStructure = regionStructure->getParent()->asRegion();
   TR_StructureSubGraphNode *subNode = NULL;
   TR_RegionStructure::Cursor si(*parentStructure);
   for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
      {
      if (subNode->getNumber() == loopStructure->getNumber())
         break;
      }

   if (subNode->getPredecessors().size() == 1)
      {
      TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
      if (loopInvariantNode->getStructure()->asBlock() &&
          loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock())
         loopInvariantBlock = loopInvariantNode->getStructure()->asBlock();
      }

   if (!loopInvariantBlock)
      return 0;

   TR_ScratchList<TR::Block> exitBlocks(trMemory());
   loopStructure->collectExitBlocks(&exitBlocks);

   if (!exitBlocks.isSingleton())
      return 0;

   int32_t symRefCount = comp()->getSymRefCount();
   initializeSymbolsWrittenAndReadExactlyOnce(symRefCount, notGrowable);

   if (trace())
      traceMsg(comp(), "\nChecking loop %d for predictability\n", loopStructure->getNumber());

   _isAddition = false;
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();

   int32_t isPredictableLoop = checkLoopForPredictability(loopStructure, loopInvariantBlock->getBlock(), NULL, false);

   if (isPredictableLoop <= 0)
      return 0;

   if (trace())
      {
      traceMsg(comp(), "\nDetected a predictable loop %d\n", loopStructure->getNumber());
      traceMsg(comp(), "Possible new induction variable candidates :\n");
      comp()->getDebug()->print(comp()->getOutFile(), &_writtenExactlyOnce);
      traceMsg(comp(), "\n");
      }

   TR::SparseBitVector::Cursor cursor(_writtenExactlyOnce);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t nextInductionVariableNumber = cursor;

      _isAddition = false;
      _loadUsedInLoopIncrement = NULL;
      bool storeInRequiredForm = isStoreInRequiredForm(nextInductionVariableNumber, loopStructure);
      if (!storeInRequiredForm)
         continue;


      TR::SymbolReference *nextInductionSymRef = symRefTab->getSymRef(nextInductionVariableNumber);

      if (!nextInductionSymRef->getUseonlyAliases().isZero(comp()) ||
         !_storeTrees[nextInductionVariableNumber] ||
         !_loopTestTree ||
            ((_loopTestTree->getNode()->getOpCodeValue() != TR::ificmplt) &&
            (_loopTestTree->getNode()->getOpCodeValue() != TR::ificmple)) ||
         !_isAddition ||
         (exitBlocks.getListHead()->getData()->getLastRealTreeTop() != _loopTestTree) ||
         !nextInductionSymRef->getSymbol()->getType().isInt32())
         continue;


      TR::Node *loopTestNode = _loopTestTree->getNode();
      bool isSameInductionVariable = false;

      if (loopTestNode->getFirstChild()->getOpCode().hasSymbolReference())
         {
         if (nextInductionVariableNumber == loopTestNode->getFirstChild()->getSymbolReference()->getReferenceNumber())
            {
            isSameInductionVariable = true;
            }
         }
      else if (loopTestNode->getFirstChild() == _storeTrees[nextInductionVariableNumber]->getNode()->getFirstChild())
         {
         isSameInductionVariable = true;
         }

      vcount_t visitCount = comp()->incVisitCount();
      if (!isSameInductionVariable ||
         (_loopTestTree->getNode()->getNumChildren() <= 1) ||
         !loopStructure->asRegion()->isExprInvariant(_loopTestTree->getNode()->getSecondChild()))
         continue;

      int32_t entryValue = 0;
      TR::VPConstraint *incrVal = NULL;

      comp()->incVisitCount();
      bool isInvertible = false;
      if (isInvertibleLoop(nextInductionVariableNumber, loopStructure))
         {
         TR::SymbolReference *inductionSymRef = symRefTab->getSymRef(nextInductionVariableNumber);
         TR_InductionVariable *v = loopStructure->asRegion()->findMatchingIV(inductionSymRef);
         if (v)
            {
            TR::VPConstraint *entryVal = v->getEntry();
            TR::VPConstraint *exitVal = v->getExit();
            incrVal  = v->getIncr();

            if (entryVal &&
                entryVal->asIntConst() &&
                (entryVal->getLowInt() == 0 ||
                 (exitVal && exitVal->asIntConst() &&
                  (llabs((long long)(exitVal->getLowInt()) - (long long)(entryVal->getLowInt()))) < INT_MAX)) &&
                // We cannot safely guess exit value of IV if it doesn't land on comparand(RhsOfStoreNode).
                // Workaround is only allow abs(incr)==1.   This is also a hard requirement for unsigned IV to not introduce wraparound.
                // For signed case, we may allow abs(incr)>1, but exit value calc is not just RhsOfStoreNode if it doesn't land on it. todo
                incrVal->asIntConst() &&
                abs(incrVal->getLowInt()) == 1)
               {
               entryValue = entryVal->getLowInt();

               if (exitVal &&
                  exitVal->asIntConst())
                  isInvertible = true;

               if ((_loopTestTree->getNode()->getSecondChild()->getOpCode().isLoadVarDirect() ||
                    _loopTestTree->getNode()->getSecondChild()->getOpCode().isLoadConst()))
                  isInvertible = true;
               }
            }
         }

      if (trace())
         traceMsg(comp(), "Loop %d is %s invertable\n", loopStructure->getNumber(), isInvertible ? "" : "not");

      if (isInvertible &&
          performTransformation(comp(), "%sInverting loop %d\n", OPT_DETAILS, loopStructure->getNumber()))
         {

         TR_RegionStructure *parent = loopStructure->getParent()->asRegion();
         while (parent)
            {
            parent->setAsInvertible(false);
            TR_Structure *p = parent->getParent();
            if (!p)
               break;
            parent = p->asRegion();
            }

         TR::Block *b = loopInvariantBlock->getBlock();
         TR::TreeTop *placeHolderTree = b->getLastRealTreeTop();
         if (!placeHolderTree->getNode()->getOpCode().isBranch())
            placeHolderTree = b->getExit();

         TR::Node *storeNode;
         if (_loopTestTree->getNode()->getSecondChild()->getOpCode().isLoadConst())
            {
            int testValue = _loopTestTree->getNode()->getSecondChild()->get32bitIntegralValue();

            if (incrVal->asIntConst() &&
                incrVal->getLowInt() == 1 &&
                testValue < INT_MAX &&
                loopTestNode->getOpCodeValue() == TR::ificmple)
               {
               testValue += 1;
               TR::Node::recreate(loopTestNode, TR::ificmplt);
               }

            int initValue = testValue - entryValue;
            storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, TR::Node::create(loopTestNode, TR::iconst, 0, initValue), nextInductionSymRef);
            }
         else
            {
            TR::Node *subNode = TR::Node::create(TR::isub, 2);
            subNode->setAndIncChild(0, _loopTestTree->getNode()->getSecondChild()->duplicateTree());
            subNode->setAndIncChild(1, TR::Node::create(loopTestNode, TR::iconst, 0, entryValue));
            storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, subNode, nextInductionSymRef);
            }

         TR::Node *rhsOfStoreNode = storeNode->getFirstChild();
         TR::TreeTop *storeTree = TR::TreeTop::create(comp(), storeNode, NULL, NULL);
         TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();
         prevTree->join(storeTree);
         storeTree->join(placeHolderTree);

         dumpOptDetails(comp(), "Init node: %p, update node: %p, loopTest node: %p\n", storeNode, _storeTrees[nextInductionVariableNumber]->getNode(), _loopTestTree->getNode());

         storeNode = _storeTrees[nextInductionVariableNumber]->getNode();

         TR::Node *origStoreNode = storeNode;
         bool origOpCodeWasAdd = false;
         if (storeNode->getFirstChild()->getOpCode().isAdd())
            {
            origOpCodeWasAdd = true;
            TR::Node::recreate(storeNode->getFirstChild(), TR::isub);
            }
         else
            TR::Node::recreate(storeNode->getFirstChild(), TR::iadd);

         loopTestNode = _loopTestTree->getNode();
         if (loopTestNode->getOpCodeValue() == TR::ificmplt)
            TR::Node::recreate(loopTestNode, TR::ificmpgt);
         else if (loopTestNode->getOpCodeValue() == TR::ificmple)
            TR::Node::recreate(loopTestNode, TR::ificmpge);
         /*
           else if (loopTestNode->getOpCodeValue() == TR::ificmpgt)
           TR::Node::recreate(loopTestNode, TR::ificmplt);
           else if (loopTestNode->getOpCodeValue() == TR::ificmpge)
           TR::Node::recreate(loopTestNode, TR::ificmple);
         */

         loopTestNode->getSecondChild()->recursivelyDecReferenceCount();
         loopTestNode->setAndIncChild(1, TR::Node::create(loopTestNode, TR::iconst, 0, 0));

         TR_ScratchList<TR::Block> blocksInRegion(trMemory());
         loopStructure->getBlocks(&blocksInRegion);

         ListIterator<TR::Block> blocksIt(&exitBlocks);
         TR::Block *nextBlock;
         for (nextBlock = blocksIt.getCurrent(); nextBlock; nextBlock=blocksIt.getNext())
            {
            TR::Block *next;
            for (auto edge = nextBlock->getSuccessors().begin(); edge != nextBlock->getSuccessors().end(); ++edge)
               {
               next = toBlock((*edge)->getTo());
               if (!blocksInRegion.find(next))
                  {
                  TR::Block *splitBlock = nextBlock->splitEdge(nextBlock, next, comp());

                  if (loopTestNode->getOpCodeValue() == TR::ificmpgt)
                     storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, rhsOfStoreNode->duplicateTree(), nextInductionSymRef);
                  else
                     {
                     if (origOpCodeWasAdd)
                        storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, TR::Node::create(TR::iadd, 2, rhsOfStoreNode->duplicateTree(), origStoreNode->getFirstChild()->getSecondChild()->duplicateTree()), nextInductionSymRef);
                     else
                        storeNode = TR::Node::createWithSymRef(TR::istore, 1, 1, TR::Node::create(TR::isub, 2, rhsOfStoreNode->duplicateTree(), origStoreNode->getFirstChild()->getSecondChild()->duplicateTree()), nextInductionSymRef);
                     }

                  TR::TreeTop *placeHolderTree = splitBlock->getLastRealTreeTop();
                  if (!placeHolderTree->getNode()->getOpCode().isBranch())
                     placeHolderTree = splitBlock->getExit();

                  storeTree = TR::TreeTop::create(comp(), storeNode, NULL, NULL);
                  TR::TreeTop *prevTree = placeHolderTree->getPrevTreeTop();
                  prevTree->join(storeTree);
                  storeTree->join(placeHolderTree);
                  }
               else
                  {
                  // Back edge; make sure async check is removed
                  // by marking as short running
                  //
                  (*edge)->setCreatedByTailRecursionElimination(true);
                  }
               }
            }
         }
      }
   return 0;
   }




bool TR_LoopInverter::isInvertibleLoop(int32_t symRefNum, TR_Structure *structure)
   {
   if (structure->asBlock() != NULL)
      {
      if (comp()->getSymRefTab()->getSymRef(symRefNum)->getSymbol()->isVolatile())
         return false;

      TR_BlockStructure *blockStructure = structure->asBlock();
      TR::Block *block = blockStructure->getBlock();
      TR::TreeTop *exitTree = block->getExit();
      TR::TreeTop *currentTree = block->getEntry();

      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();

         TR::NodeChecklist checklist(comp());
         if (!checkIfSymbolIsReadInKnownTree(currentNode, symRefNum, currentTree, checklist) ||
             ((currentNode->getOpCodeValue() != TR::asynccheck) &&
              currentNode->canGCandReturn()))
            return false;

         currentTree = currentTree->getNextTreeTop();
         }
      }
   else
      {
      TR_RegionStructure *regionStructure = structure->asRegion();
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*regionStructure);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         {
         if (!isInvertibleLoop(symRefNum, node->getStructure()))
            return false;
         }
      }

   return true;
   }



/**
 * We want to make sure that the induction variable (symRefNum) is not read by
 * any of the trees except the loop test and the IV update trees.
 */
bool TR_LoopInverter::checkIfSymbolIsReadInKnownTree(TR::Node *node, int32_t symRefNum, TR::TreeTop *currentTree, TR::NodeChecklist &checklist)
   {

   // Do this check before the checklist.contains() check so that we don't waste time
   // traversing these two trees. NOTE that we don't want to mark the nodes in
   // these trees as visited (i.e., add them to the check list) except if the
   // tree top we start from is other than these two types of trees in this if statement.
   if(currentTree == _loopTestTree || currentTree == _storeTrees[symRefNum])
      return true;

   if (checklist.contains(node))
      return true;

   checklist.add(node);

   TR_UseDefAliasSetInterface defAliases = comp()->getSymRefTab()->getSymRef(symRefNum)->getUseDefAliases();

   if (node->getOpCode().hasSymbolReference())
      {
      int32_t aliasIndex = node->getSymbolReference()->getReferenceNumber();
      if ((aliasIndex == symRefNum) ||
          (defAliases.hasAliases() && defAliases.contains(aliasIndex, comp())))
         return false;
      }

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      {
      if (!checkIfSymbolIsReadInKnownTree(node->getChild(i), symRefNum, currentTree, checklist))
         return false;
      }

   return true;
   }

const char *
TR_LoopInverter::optDetailString() const throw()
   {
   return "O^O LOOP INVERTER: ";
   }
