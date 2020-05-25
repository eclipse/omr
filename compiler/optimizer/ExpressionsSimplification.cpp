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

#include "optimizer/ExpressionsSimplification.hpp"

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
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
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Dominators.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "optimizer/VPConstraint.hpp"
#include "ras/Debug.hpp"

#define OPT_DETAILS "O^O EXPRESSION SIMPLIFICATION: "

TR_ExpressionsSimplification::TR_ExpressionsSimplification(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t
TR_ExpressionsSimplification::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   int32_t cost = 0;
   _supportedExpressions = NULL;

   if (trace())
      {
      comp()->dumpMethodTrees("Trees Before Performing Expression Simplification");
      }
   cost = perform(comp()->getFlowGraph()->getStructure());

   return cost;
   }


// Process the structure recursively
//
int32_t
TR_ExpressionsSimplification::perform(TR_Structure * str)
   {
   if (trace())
      traceMsg(comp(), "Analyzing root Structure : %p\n", str);

   TR_RegionStructure *region;

   // Only regions can be simplified
   //
   if (!(region = str->asRegion()))
      return 0;

   TR_RegionStructure::Cursor it(*region);

   for (TR_StructureSubGraphNode *node = it.getCurrent();
        node != 0;
        node = it.getNext())
      {
      // Too strict
      /*
      if ((node->getPredecessors().size() == 1))
         {
         TR::CFGEdge *edge = node->getPredecessors().front();
         TR_StructureSubGraphNode *pred = toStructureSubGraphNode(edge->getFrom());
         TR_BlockStructure *b = pred->getStructure()->asBlock();
         if (b && pred->getSuccessors().size() == 1))
            perform(node->getStructure());
         }
      */
      perform(node->getStructure());
      }

   // debug only
   //
   /*
   if (region->isNaturalLoop() &&
          (region->getParent()  &&
            !region->getParent()->asRegion()->isCanonicalizedLoop()))
      {
      traceMsg(comp(), "Loop not canonicalized %x\n", region);
      }
   */

   TR::Block *entryBlock = region->getEntryBlock();
   if (region->isNaturalLoop() && !entryBlock->isCold() &&
          (region->getParent() /* &&
            region->getParent()->asRegion()->isCanonicalizedLoop() */))
         {
         if (trace())
           traceMsg(comp(), "Found candidate non cold loop %p for expression elimination\n", region);

         findAndSimplifyInvariantLoopExpressions(region);
         }

   return 1;  // Need to specify the cost
   }


void
TR_ExpressionsSimplification::findAndSimplifyInvariantLoopExpressions(TR_RegionStructure * region)
   {
   _currentRegion = region;
   TR::Block *entryBlock = _currentRegion->getEntryBlock();
   if (trace())
      traceMsg(comp(), "Entry block: %p in loop region %p\n", entryBlock, region);


   // Generate a list of blocks that can be processed
   // Criteria: the block must be excucted exactly once
   //
   TR_ScratchList<TR::Block> candidateBlocksList(trMemory());
   _currentRegion->getBlocks(&candidateBlocksList);

   if (candidateBlocksList.getSize() > 1)
      {
      if (trace())
         traceMsg(comp(), "More than 1 blocks in the natural loop, need to remove uncertain blocks\n");

      removeUncertainBlocks(_currentRegion, &candidateBlocksList);

      if (candidateBlocksList.getSize() < 1)
         return;
      }

   _currentRegion->resetInvariance();
   _currentRegion->computeInvariantExpressions();

   // The rest is the transformation
   //
   // For each block that is definitely executed once
   // analyze its nodes
   //
   ListIterator<TR::Block> candidateBlocks;
   candidateBlocks.set(&candidateBlocksList);
   simplifyInvariantLoopExpressions(candidateBlocks);
   }

void TR_ExpressionsSimplification::simplifyInvariantLoopExpressions(ListIterator<TR::Block> &blocks)
   {
   // Need to locate the induction variable of the loop
   //
   LoopInfo *loopInfo = findLoopInfo(_currentRegion);

   bool canReduceSummations = false;
   if (!loopInfo)
      {
      if (trace())
         traceMsg(comp(), "Accurate loop info is not found, cannot carry out summation reduction\n");
      }
   else
      {
      if (trace())
         traceMsg(comp(), "Accurate loop info has been found, will try to carry out summation reduction\n");

      int32_t iters = loopInfo->getNumIterations();
      if (loopInfo->getBoundaryNode())
         {
         if (trace())
            traceMsg(comp(), "Variable iterations from node %p has not been handled\n",loopInfo->getBoundaryNode());
         }
      else if (iters <= 0)
         {
         if (trace())
            traceMsg(comp(), "Failed to determine iteration count\n");
         }
      else
         {
         canReduceSummations = true;
         if (trace())
            traceMsg(comp(), "Natural Loop %p will run %d times\n", _currentRegion, iters);
         }
      }

   // Initialize the list of candidates
   //
   _candidateTTs = new (trStackMemory()) TR_ScratchList<TR::TreeTop>(trMemory());

   for (TR::Block *currentBlock = blocks.getFirst(); currentBlock; currentBlock  = blocks.getNext())
      {
      if (trace())
         traceMsg(comp(), "Analyzing block #%d, which must be executed once per iteration\n", currentBlock->getNumber());


      // Scan through each node in the block
      //
      TR::TreeTop *tt = currentBlock->getEntry();
      TR::TreeTop *exitTreeTop = currentBlock->getExit();
      while (tt != exitTreeTop)
         {
         TR::Node *currentNode = tt->getNode();
         if (trace())
            traceMsg(comp(), "Analyzing tree top node %p\n", currentNode);

         if (canReduceSummations)
            {
            // requires loop info for the number of iterations
            setSummationReductionCandidates(currentNode, tt);
            }
         setStoreMotionCandidates(currentNode, tt);

         tt = tt->getNextTreeTop();
         }
      }

   // New code: without using any UDI
   // walk through the trees in the loop
   // to invalidate the candidates
   //
   if (!_supportedExpressions)
      {
      _supportedExpressions = new (trStackMemory()) TR_BitVector(comp()->getNodeCount(), trMemory(), stackAlloc, growable);
      }

   invalidateCandidates();

   ListIterator<TR::TreeTop> treeTops(_candidateTTs);
   for (TR::TreeTop *treeTop = treeTops.getFirst(); treeTop; treeTop = treeTops.getNext())
      {
      if (trace())
         traceMsg(comp(), "Candidate TreeTop: %p, Node:%p\n", treeTop, treeTop->getNode());

      bool usedCandidate = false;
      bool isPreheaderBlockInvalid = false;

      if (canReduceSummations)
         {
         usedCandidate = tranformSummationReductionCandidate(treeTop, loopInfo, &isPreheaderBlockInvalid);
         }

      if (isPreheaderBlockInvalid)
         {
         break;
         }

      if (!usedCandidate)
         {
         tranformStoreMotionCandidate(treeTop, &isPreheaderBlockInvalid);
         }
      if (isPreheaderBlockInvalid)
         {
         break;
         }
      }
   }

void TR_ExpressionsSimplification::setSummationReductionCandidates(TR::Node *node, TR::TreeTop *tt)
   {
   // Must be a store node
   //
   if (node->getOpCodeValue() != TR::istore
   /* || node->getOpCodeValue() != TR::astore */)
      {
      if (trace())
         traceMsg(comp(), "Node %p: The opcode is not istore so not a summation reduction candidate\n",node);
      return;
      }

   TR::Node *opNode = node->getFirstChild();

   if (opNode->getOpCodeValue() == TR::iadd ||
       opNode->getOpCodeValue() == TR::isub)
      {
      TR::Node *firstNode = opNode->getFirstChild();
      TR::Node *secondNode = opNode->getSecondChild();

      if (firstNode->getOpCode().hasSymbolReference() &&
            node->getSymbolReference() == firstNode->getSymbolReference() &&
            opNode->getReferenceCount() == 1 && firstNode->getReferenceCount() == 1)
         {
         // The second node must be loop invariant
         //
         if (!_currentRegion->isExprInvariant(secondNode))
            {
            if (trace())
               {
               traceMsg(comp(), "The node %p is not loop invariant\n",secondNode);

               // This can be the arithmetic series case
               // only when the node is an induction variable
               if (secondNode->getNumChildren() == 1 && secondNode->getOpCode().hasSymbolReference())
                  {
                  TR_InductionVariable *indVar = _currentRegion->findMatchingIV(secondNode->getSymbolReference());
                  if (indVar)
                     {
                     //printf("Found Candidate of arithmetic series\n" );
                     }
                  }
               }
            return;
            }

         _candidateTTs->add(tt);
         }
      else if (secondNode->getOpCode().hasSymbolReference() &&
            node->getSymbolReference() == secondNode->getSymbolReference() &&
            opNode->getReferenceCount() == 1 && secondNode->getReferenceCount() == 1 &&
            _currentRegion->isExprInvariant(firstNode))
         {
         _candidateTTs->add(tt);
         }
      }
   else if (opNode->getOpCodeValue() == TR::ixor ||
            opNode->getOpCodeValue() == TR::ineg)
      {
      if (opNode->getFirstChild()->getOpCode().hasSymbolReference() &&
            node->getSymbolReference() == opNode->getFirstChild()->getSymbolReference() &&
            opNode->getReferenceCount() == 1 && opNode->getFirstChild()->getReferenceCount() == 1 &&
            (opNode->getOpCodeValue() == TR::ineg || _currentRegion->isExprInvariant(opNode->getSecondChild())))
         _candidateTTs->add(tt);
      else if (opNode->getOpCodeValue() == TR::ixor && opNode->getSecondChild()->getOpCode().hasSymbolReference() &&
            node->getSymbolReference() == opNode->getSecondChild()->getSymbolReference() &&
            opNode->getReferenceCount() == 1 && opNode->getSecondChild()->getReferenceCount() == 1 &&
            _currentRegion->isExprInvariant(opNode->getFirstChild()))
         _candidateTTs->add(tt);
      }
   }

void TR_ExpressionsSimplification::setStoreMotionCandidates(TR::Node *node, TR::TreeTop *tt)
   {
   // Must be a store node, of any type but not holding a monitor object
   //
   if (node->getOpCode().isStore() &&
       !node->getSymbol()->isStatic() &&
       !node->getSymbol()->holdsMonitoredObject())
      {
      if (trace())
         traceMsg(comp(), "Node %p: The opcode is a non-static, non-monitor object store\n", node);

      for (int32_t i = 0; i < node->getNumChildren(); i++)
         {
         if (!_currentRegion->isExprInvariant(node->getChild(i)))
            {
            if (trace())
               traceMsg(comp(), "Node %p: The store is not loop-invariant due to child %p\n", node, node->getChild(i));
            return;
            }
         }
      // If store's operands are loop-invariant
      if (trace())
         {
         traceMsg(comp(), "Node %p: The store's operands are all loop-invariant, adding candidate\n", node);
         traceMsg(comp(), "Node %p:   - value of isExprInvariant for the store itself is %s\n", node, _currentRegion->isExprInvariant(node) ? "true" : "false");
         }
      _candidateTTs->add(tt);
      }
   }

bool TR_ExpressionsSimplification::tranformSummationReductionCandidate(TR::TreeTop *treeTop, LoopInfo *loopInfo, bool *isPreheaderBlockInvalid)
   {
   TR::Node *node = treeTop->getNode();
   TR::Node *opNode = node->getFirstChild();
   TR::Node *expNode = NULL;
   int32_t expChildNumber = 0;
   bool removeOnly = false;
   bool replaceWithNewNode = false;

   if (opNode->getOpCodeValue() == TR::iadd || opNode->getOpCodeValue() == TR::isub)
      {
      if (opNode->getSecondChild()->getOpCode().hasSymbolReference() &&
            node->getSymbolReference() == opNode->getSecondChild()->getSymbolReference())
         {
         expChildNumber = 0;
         expNode = opNode->getFirstChild();
         }
      else
         {
         expChildNumber = 1;
         expNode = opNode->getSecondChild();
         }
      expNode = iaddisubSimplifier(expNode, loopInfo);
      replaceWithNewNode = true;
      }
   else if (opNode->getOpCodeValue() == TR::ixor || opNode->getOpCodeValue() == TR::ineg)
      {
      expNode = ixorinegSimplifier(opNode, loopInfo, &removeOnly);
      }

   if (expNode)
      {
      if (trace())
         comp()->getDebug()->print(comp()->getOutFile(), expNode, 0, true);

      TR::Block *entryBlock = _currentRegion->getEntryBlock();
      TR::Block *preheaderBlock = findPredecessorBlock(entryBlock);

      if (!preheaderBlock)
         {
         if (trace())
            traceMsg(comp(), "Fail to find a place to put the hoist code in\n");
         *isPreheaderBlockInvalid = true;
         return true;
         }

      if (loopInfo->getNumIterations() > 0 ||     // make sure that the loop is going to be executed at least once
            _currentRegion->isCanonicalizedLoop())  // or that the loop is canonicalized, in which case the preheader is
         {                                        // executed in its first iteration and is protected.
         if (performTransformation(comp(), "%sMove out loop-invariant node [%p] to block_%d\n", OPT_DETAILS, node, preheaderBlock->getNumber()))
            {
            if (!(removeOnly))
               {
               TR::Node *newNode = node->duplicateTree();
               if (replaceWithNewNode)
                  newNode->getFirstChild()->setAndIncChild(expChildNumber, expNode);
               transformNode(newNode, preheaderBlock);
               }
            TR::TransformUtil::removeTree(comp(), treeTop);
            }
         }
      }
      return (expNode != NULL);
   }

void TR_ExpressionsSimplification::tranformStoreMotionCandidate(TR::TreeTop *treeTop, bool *isPreheaderBlockInvalid)
   {
   TR::Node *node = treeTop->getNode();

   TR_ASSERT(node->getOpCode().isStore() && !node->getSymbol()->isStatic() && !node->getSymbol()->holdsMonitoredObject(),
      "node %p was expected to be a non-static non-monitored object store and was not.", node);

   // this candidate should be valid, either direct or indirect

   if (trace())
      comp()->getDebug()->print(comp()->getOutFile(), node, 0, true);

   TR::Block *entryBlock = _currentRegion->getEntryBlock();
   TR::Block *preheaderBlock = findPredecessorBlock(entryBlock);

   if (!preheaderBlock)
      {
      if (trace())
         traceMsg(comp(), "Fail to find a place to put the hoist code in\n");
      *isPreheaderBlockInvalid = true;
      return;
      }

   // Earlier post-dominance test ensures that the loop is executed as least once, or is canonicalized.
   // but to be safe we still perform on canonicalized loops only.
   if (_currentRegion->isCanonicalizedLoop())  // make sure that the loop is canonicalized, in which case the preheader is
      {                                        // executed in its first iteration and is protected.
      if (performTransformation(comp(), "%sMove out loop-invariant store [%p] to block_%d\n", OPT_DETAILS, node, preheaderBlock->getNumber()))
         {
         TR::Node *newNode = node->duplicateTree();
         transformNode(newNode, preheaderBlock);
         TR::TransformUtil::removeTree(comp(), treeTop);
         }
      }
   else
      {
      if (trace())
         traceMsg(comp(), "No canonicalized loop for this candidate\n");
      }
   }

void
TR_ExpressionsSimplification::invalidateCandidates()
   {
   _visitCount = comp()->incVisitCount();

   if (trace())
      {
      traceMsg(comp(), "Checking which candidates may be invalidated\n");

      ListIterator<TR::TreeTop> treeTops(_candidateTTs);
      for (TR::TreeTop *treeTop = treeTops.getFirst(); treeTop; treeTop = treeTops.getNext())
         {
         traceMsg(comp(), "   Candidate treetop: %p node: %p\n", treeTop, treeTop->getNode());
         }
      }

   TR_ScratchList<TR::Block> blocksInLoop(trMemory());
   _currentRegion->getBlocks(&blocksInLoop);
   ListIterator<TR::Block> blocks(&blocksInLoop);

   for (TR::Block *currentBlock = blocks.getFirst(); currentBlock; currentBlock  = blocks.getNext())
      {
      TR::TreeTop *tt = currentBlock->getEntry();
      TR::TreeTop *exitTreeTop = currentBlock->getExit();
      while (tt != exitTreeTop)
         {
         TR::Node *currentNode = tt->getNode();

         if (trace())
            traceMsg(comp(), "Looking at treeTop [%p]\n", currentNode);

         removeCandidate(currentNode, tt);

         tt = tt->getNextTreeTop();
         }
      }
   removeUnsupportedCandidates();
   }

void
TR_ExpressionsSimplification::removeUnsupportedCandidates()
   {
   ListIterator<TR::TreeTop> candidateTTs(_candidateTTs);
   for (TR::TreeTop *candidateTT = candidateTTs.getFirst(); candidateTT; candidateTT = candidateTTs.getNext())
      {
      TR::Node *candidate = candidateTT->getNode();
      if (!_supportedExpressions->get(candidate->getGlobalIndex()))
         {
         if (trace())
            traceMsg(comp(), "Removing candidate %p which is unsupported or has unsupported subexpressions\n", candidate);

         _candidateTTs->remove(candidateTT);
         }
      }
   }

void
TR_ExpressionsSimplification::removeCandidate(TR::Node *node, TR::TreeTop* tt)
   {
   if (node->getVisitCount() == _visitCount)
      return;

   node->setVisitCount(_visitCount);

   if (trace())
      traceMsg(comp(), "Looking at Node [%p]\n", node);

   ListIterator<TR::TreeTop> candidateTTs(_candidateTTs);
   for (TR::TreeTop *candidateTT = candidateTTs.getFirst(); candidateTT; candidateTT = candidateTTs.getNext())
      {
      if (tt != candidateTT &&
          node->getOpCode().hasSymbolReference() &&
          candidateTT->getNode()->mayKill(true).contains(node->getSymbolReference(), comp()))
         {
         if (trace())
            traceMsg(comp(), "Removing candidate %p which has aliases in the loop\n", candidateTT->getNode());

         _candidateTTs->remove(candidateTT);
         continue;
         }
      }

   bool hasSupportedChildren = true;

   // Process the children as well
   //
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      removeCandidate(node->getChild(i), tt);
      // candidates child expressions must be invariant and supported. Here we determine if they are supported.
      if (!_supportedExpressions->get(node->getChild(i)->getGlobalIndex()))
         {
         hasSupportedChildren = false;
         }
      }

   if (hasSupportedChildren && isSupportedNodeForExpressionSimplification(node))
      {
       _supportedExpressions->set(node->getGlobalIndex());
      }
   else
      {
      if (trace())
         traceMsg(comp(), "  Node %p is unsupported expression because %s\n", node,
               !hasSupportedChildren ? "it has unsupported children" : "it is itself unsupported");
      }
   }

bool
TR_ExpressionsSimplification::isSupportedNodeForExpressionSimplification(TR::Node *node)
   {
   // types of stores that may be supported by loop invariant store motion
   bool isSupportedStoreNode = node->getOpCode().isStore();
   return TR_LocalAnalysis::isSupportedNodeForFunctionality(node, comp(), NULL, isSupportedStoreNode);
   }


static bool examineNode(TR::Node *node, intptr_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);

   if (node->getOpCode().isCall())
      return true;

   bool hasCalls = false;
   for (int32_t i = 0; !hasCalls && i < node->getNumChildren(); i++)
      {
      hasCalls = examineNode(node->getChild(i), visitCount);
      }

   return hasCalls;
   }

static bool blockHasCalls(TR::Block *block, TR::Compilation *comp)
   {
   intptr_t visitCount = comp->incVisitCount();

   TR::TreeTop *currentTree = block->getEntry();
   TR::TreeTop *exitTree = block->getExit();
   bool hasCalls = false;

   while (!hasCalls && currentTree != exitTree)
      {
      hasCalls = examineNode(currentTree->getNode(), visitCount);
      currentTree = currentTree->getNextTreeTop();
      }

   return hasCalls;
   }

void
TR_ExpressionsSimplification::removeUncertainBlocks(TR_RegionStructure* region, List<TR::Block> *candidateBlocksList)
   {
   // Examine the top region block first
   //
   TR::Block *entryBlock = _currentRegion->getEntryBlock();
   ListIterator<TR::Block> blocks;
   blocks.set(candidateBlocksList);

   if (trace())
      traceMsg(comp(), "Number of blocks %d, entry block number %d\n", candidateBlocksList->getSize(), entryBlock->getNumber());

   for (TR::Block *block = blocks.getFirst(); block; block = blocks.getNext())
      {
      TR::CFGNode *cfgNode = block;
      if (!(cfgNode->getExceptionSuccessors().empty()) || blockHasCalls(block, comp()))
         {
         if (trace())
            traceMsg(comp(), "An exception can be thrown from block_%d. Removing all the blocks, since we cannot know the number of iterations.\n", block->getNumber());
         candidateBlocksList->deleteAll();
         break;
         }
      }

   TR_PostDominators postDominators(comp());
   if (postDominators.isValid())
      {
	  postDominators.findControlDependents();
      for (TR::Block *block = blocks.getFirst(); block; block = blocks.getNext())
         {
         if (postDominators.dominates(block, entryBlock) == 0)
            {
            candidateBlocksList->remove(block);
            if (trace())
               traceMsg(comp(), "Block_%d is not guaranteed to be executed at least once. Removing it from the list.\n", block->getNumber());
            }
         }
      }
   else
      {
	  if (trace())
	     traceMsg(comp(), "There is no post dominators information. Removing all the blocks.\n");
	  for (TR::Block *block = blocks.getFirst(); block; block = blocks.getNext())
	     {
	     candidateBlocksList->remove(block);
	     if (trace())
	        traceMsg(comp(), "Block_%d is removed from the list\n", block->getNumber());
	     }
      }
   }



TR_ExpressionsSimplification::LoopInfo*
TR_ExpressionsSimplification::findLoopInfo(TR_RegionStructure* region)
   {
   ListIterator<TR::CFGEdge> exitEdges(&region->getExitEdges());

   if (region->getExitEdges().getSize() != 1)
      {
      if (trace())
         traceMsg(comp(), "Region with more than 1 exit edges can't be handled\n");
      return 0;
      }

   TR::CFGEdge *exitEdge = exitEdges.getFirst();
   TR_StructureSubGraphNode* exitNode = toStructureSubGraphNode(exitEdge->getFrom());
   int32_t exitTarget = exitEdge->getTo()->getNumber();

   if (!exitNode->getStructure()->asBlock())
      {
      if (trace())
         traceMsg(comp(), "The exit block can't be found\n");
      return 0;
      }

   TR::Block *exitBlock = exitNode->getStructure()->asBlock()->getBlock();
   TR::Node *lastTreeInExitBlock = exitBlock->getLastRealTreeTop()->getNode();

   if (trace())
      {
      traceMsg(comp(), "The exit block is %d\n", exitBlock->getNumber());
      traceMsg(comp(), "The branch node is %p\n", lastTreeInExitBlock);
      }


   if (!lastTreeInExitBlock->getOpCode().isBranch())
      {
      if (trace())
         traceMsg(comp(), "The branch node couldn't be found\n");
      return 0;
      }

   if (lastTreeInExitBlock->getNumChildren() < 2)
      {
      if (trace())
         traceMsg(comp(), "The branch node has less than 2 children\n");
      return 0;
      }

   TR::Node *firstChildOfLastTree = lastTreeInExitBlock->getFirstChild();
   TR::Node *secondChildOfLastTree = lastTreeInExitBlock->getSecondChild();

   if (!firstChildOfLastTree->getOpCode().hasSymbolReference())
      {
      if (trace())
         traceMsg(comp(), "The branch node's first child node %p - its opcode does not have a symbol reference\n", firstChildOfLastTree);
      return 0;
      }

   TR::SymbolReference *firstChildSymRef = firstChildOfLastTree->getSymbolReference();

   if (trace())
      traceMsg(comp(), "Symbol Reference: %p Symbol: %p\n", firstChildSymRef, firstChildSymRef->getSymbol());

   // Locate the induction variable that matches with the exit node symbol
   //
   TR_InductionVariable *indVar = region->findMatchingIV(firstChildSymRef);
   if (!indVar) return 0;

   if (!indVar->getIncr()->asIntConst())
      {
      if (trace())
         traceMsg(comp(), "Increment is not a constant\n");
      return 0;
      }

   int32_t increment = indVar->getIncr()->getLowInt();

   _visitCount = comp()->incVisitCount();
   bool indVarWrittenAndUsedUnexpectedly = false;
   if (firstChildOfLastTree->getReferenceCount() > 1)
      {
      TR::TreeTop *cursorTreeTopInExitBlock = exitBlock->getEntry();
      TR::TreeTop *exitTreeTopInExitBlock = exitBlock->getExit();

      bool loadSeen = false;
      while (cursorTreeTopInExitBlock != exitTreeTopInExitBlock)
         {
         TR::Node *cursorNode = cursorTreeTopInExitBlock->getNode();
         if (checkForLoad(cursorNode, firstChildOfLastTree))
            loadSeen = true;

         if (!cursorNode->getOpCode().isStore() &&
             (cursorNode->getNumChildren() > 0))
           cursorNode = cursorNode->getFirstChild();

         if (cursorNode->getOpCode().isStore() &&
             (cursorNode->getSymbolReference() == firstChildSymRef))
            {
            indVarWrittenAndUsedUnexpectedly = true;
            if ((cursorNode->getFirstChild() == firstChildOfLastTree) ||
                !loadSeen)
               indVarWrittenAndUsedUnexpectedly = false;
            else
               break;
            }

         cursorTreeTopInExitBlock = cursorTreeTopInExitBlock->getNextTreeTop();
         }
      }

   if (indVarWrittenAndUsedUnexpectedly)
      {
      return 0;
      }

   int32_t lowerBound;
   int32_t upperBound = 0;
   TR::Node *bound = 0;
   bool equals = false;

   switch(lastTreeInExitBlock->getOpCodeValue())
      {
      case TR::ificmplt:
      case TR::ificmpgt:
         equals = true;
      case TR::ificmple:
      case TR::ificmpge:
         {
         TR::TreeTop *branchDestTT = lastTreeInExitBlock->getBranchDestination();
         TR::Block *branchDest = branchDestTT->getNode()->getBlock();
         if (branchDest->getNumber() != exitTarget)
            equals = !equals;

         if (!(indVar->getEntry() && indVar->getEntry()->asIntConst()))
            {
            if (trace())
               traceMsg(comp(), "Entry value is not a constant\n");
            return 0;
            }
         lowerBound = indVar->getEntry()->getLowInt();

         if (secondChildOfLastTree->getOpCode().isLoadConst())
            {
            upperBound = secondChildOfLastTree->getInt();
            }
         else if (secondChildOfLastTree->getOpCode().isLoadVar())
            {
            bound = secondChildOfLastTree;
            }
         else
            {
            if (trace())
               traceMsg(comp(), "Second child is not a const or a load\n");
            return 0;
            }
         return new (trStackMemory()) LoopInfo(bound, lowerBound, upperBound, increment, equals);
         }

      default:
         if (trace())
            traceMsg(comp(), "The condition has not been implemeted\n");
         return 0;
      }

   return 0;
   }


bool
TR_ExpressionsSimplification::checkForLoad(TR::Node *node, TR::Node *load)
   {
   if (node->getVisitCount() == _visitCount)
      return false;

   node->setVisitCount(_visitCount);

   if (node == load)
      return true;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      if (checkForLoad(node->getChild(i), load))
         return true;
      }

   return false;
   }


TR::Node *
TR_ExpressionsSimplification::iaddisubSimplifier(TR::Node *invariantNode, LoopInfo* loopInfo)
   {
   TR_ASSERT_FATAL(
      loopInfo->getBoundaryNode() == NULL,
      "iteration count must be constant for loop %d",
      _currentRegion->getNumber());

   int32_t iters = loopInfo->getNumIterations();
   TR_ASSERT_FATAL(
     iters > 0,
     "iteration count (%d) must be known and positive for loop %d",
     iters,
     _currentRegion->getNumber());

   return TR::Node::create(
      invariantNode,
      TR::imul,
      2,
      invariantNode->duplicateTree(),
      TR::Node::create(invariantNode, TR::iconst, 0, iters));
   }


TR::Node *
TR_ExpressionsSimplification::ixorinegSimplifier(TR::Node *node, LoopInfo* loopInfo, bool *removeOnly)
   {
   TR::Node *newNode = 0;
   *removeOnly = false;

   if (loopInfo->getBoundaryNode())
      {
      if (trace())
         traceMsg(comp(), "Loop has a non constant boundary, but this case is not taken care of\n");
      }
   else
      {
      if (loopInfo->getNumIterations() > 0)
         {
         newNode = node;
         if (loopInfo->getNumIterations() % 2 == 0)
            {
            *removeOnly = true;
            }
         }
      }

   return newNode;
   }


TR::Block *
TR_ExpressionsSimplification::findPredecessorBlock(TR::CFGNode *entryNode)
   {
   if (!(entryNode->getPredecessors().size() == 2))
       return 0;


   TR::Block *block = 0;

   for (auto edge = entryNode->getPredecessors().begin(); edge != entryNode->getPredecessors().end(); ++edge)
      {
      if ((*edge)->getFrom()->getSuccessors().size() == 1)
         {
         block = toBlock((*edge)->getFrom());
         if (block->getStructureOf()->isLoopInvariantBlock())
            break;
         else
            block = 0;
         }
      }
   //traceMsg(comp(), "Commoned code will be put in block_%d\n", block->getNumber());
   return block;
   }


void
TR_ExpressionsSimplification::transformNode(TR::Node *srcNode, TR::Block *dstBlock)
   {
   TR::TreeTop *lastTree = dstBlock->getLastRealTreeTop();
   TR::TreeTop *prevTree = lastTree->getPrevTreeTop();
   TR::TreeTop *srcNodeTT = TR::TreeTop::create(comp(), srcNode);

   if (trace())
      comp()->getDebug()->print(comp()->getOutFile(),srcNode,0,true);

   if (lastTree->getNode()->getOpCode().isBranch() ||
       (lastTree->getNode()->getOpCode().isJumpWithMultipleTargets() && lastTree->getNode()->getOpCode().hasBranchChildren()))
      {
      srcNodeTT->join(lastTree);
      prevTree->join(srcNodeTT);
      }
   /*
   else if (dstBlock->getEntry()->getNode()->getOpCodeValue() == TR::BBStart)
      {
      srcNodeTT->join(dstBlock->getExit());
      dstBlock->getEntry()->join(srcNodeTT);
      }
   */
   else
      {
      srcNodeTT->join(dstBlock->getExit());
      lastTree->join(srcNodeTT);
      }
   return;
   }

const char *
TR_ExpressionsSimplification::optDetailString() const throw()
   {
   return "O^O EXPRESSION SIMPLIFICATION: ";
   }
