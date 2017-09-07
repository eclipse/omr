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

#include "optimizer/VirtualGuardCoalescer.hpp"

#include <stdio.h>                                 // for printf
#include <stdlib.h>                                // for atof
#include <string.h>                                // for memset
#include "env/StackMemoryRegion.hpp"
#include "codegen/FrontEnd.hpp"                    // for feGetEnv
#include "compile/Compilation.hpp"                 // for Compilation
#include "compile/VirtualGuard.hpp"                // for TR_VirtualGuard
#include "cs2/sparsrbit.h"                         // for ASparseBitVector
#include "env/jittypes.h"                          // for TR_ByteCodeInfo, etc
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                            // for toBlock, etc
#include "il/DataTypes.hpp"
#include "il/ILOps.hpp"                            // for ILOpCode, etc
#include "il/ILOpCodes.hpp"                        // for ILOpCodes::Goto, etc
#include "il/Node.hpp"                             // for Node
#include "il/Node_inlines.hpp"                     // for Node::getChild, etc
#include "il/Symbol.hpp"                           // for Symbol
#include "il/SymbolReference.hpp"                  // for SymbolReference
#include "il/TreeTop.hpp"                          // for TreeTop
#include "il/TreeTop_inlines.hpp"                  // for TreeTop::getNode, etc
#include "infra/Assert.hpp"                        // for TR_ASSERT
#include "infra/Cfg.hpp"                           // for CFG
#include "infra/TRCfgEdge.hpp"                     // for CFGEdge
#include "infra/TRCfgNode.hpp"                     // for CFGNode
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"                 // for Optimizer
#include "optimizer/Structure.hpp"                 // for TR_BlockStructure, etc
#include "optimizer/TransformUtil.hpp"             // for TransformUtil
#include "optimizer/ValueNumberInfo.hpp"
#include "optimizer/LocalOpts.hpp"
#ifdef J9_PROJECT_SPECIFIC
#include "runtime/J9Profiler.hpp"
#include "runtime/J9ValueProfiler.hpp"
#endif

#define OPT_DETAILS "O^O VIRTUAL GUARD COALESCER: "

// A simple extension to TR_List, meant to make a List seem
// like a stack.  The only thing missing in List was a
// good top() method.
//
template <class T> class ListStack : public List<T>
   {
   public:
   ListStack(TR_Memory * m) : List<T>(m) { }

   void push(T *t) { List<T>::add(t); }
   T   *pop()      { return List<T>::popHead(); }
   T   *top()      { return List<T>::_pHead ? List<T>::_pHead->getData() : NULL;}
   };

TR_VirtualGuardTailSplitter::TR_VirtualGuardTailSplitter(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {
   _cfg = comp()->getFlowGraph();
   }


int32_t TR_VirtualGuardTailSplitter::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _splitDone = false;
   // Initialize any necessary data structures
   //
   initializeDataStructures();

   int32_t numBlocks = _cfg->getNextNodeNumber();
   int32_t nodeCount = comp()->getNodeCount();

   static char *globalSplit = feGetEnv("TR_globalSplit");
   if (globalSplit && !comp()->isProfilingCompilation() &&
       _numGuards >= 15 && numBlocks/5 < _numGuards)
      {
      // Discovered method is loaded with virtual guards!
      // Perform a global split
      //
      splitGlobal();
      printf("---$$$--- GlobalSplit %d,%d %s\n", nodeCount, comp()->getNodeCount(),
             comp()->signature());
      return 10;
      }

   _visitCount = comp()->incVisitCount();

   // Attempt to split tails occurring in a linear control flow
   //
   splitLinear(toBlock(_cfg->getStart()), toBlock(_cfg->getEnd()));

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after splitLinear");
      }

   // Change cold guards to gotos
   //
   // TODO : disabled this since we still execute *some* cold blocks
   // because of phase changes and we do not want to penalize a scenario
   // that could legitimately happen, e.g. blocks marked cold early in a run
   // may get run later on. Unless we have a better phase change detection and
   // recovery mechanism, we should avoid this optimization.
   //
   static const char* enableColdGuardElimination = feGetEnv("TR_EnableColdVirtualGuardEliminationInVGTS");
   if (enableColdGuardElimination)
      {
      eliminateColdVirtualGuards(comp()->getStartTree());
      }

   /*
   if (_splitDone)
       initializeDataStructures();
   rematerializeThis();
   */

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after eliminateColdVirtualGuards");
      }

   return 0;
   }


void TR_VirtualGuardTailSplitter::eliminateColdVirtualGuards(TR::TreeTop *treetop)
   {
   TR::Block *block = NULL;
   while (treetop)
      {
      TR::Node *node = treetop->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         block = node->getBlock();

      //this is set in case if (block->nodeIsRemoved()) fires later
      //and block goes gentle into that night
      TR::TreeTop* predBBStart = block->getPrevBlock() ? block->getPrevBlock()->getEntry() : NULL;

      VGInfo *info = getVirtualGuardInfo(block);
      TR_VirtualGuard *virtualGuard = NULL;
      if (block->getLastRealTreeTop()->getNode()->isTheVirtualGuardForAGuardedInlinedCall())
         virtualGuard = comp()->findVirtualGuardInfo(block->getLastRealTreeTop()->getNode());

      if (virtualGuard &&
          block->isCold() &&
          performTransformation(comp(), "%s remove guard from cold block_%d\n", OPT_DETAILS, block->getNumber()))
         {
         TR::Block *dest = block->getLastRealTreeTop()->getNode()->getBranchDestination()->getNode()->getBlock(); //info->getCallBlock();
         TR::Block *next = block->getNextBlock();

         _cfg->removeEdge(block, next);

         if (info)
            info->markRemoved();

         //if block was the head of an orphaned region
         //removeEdge would have completely scrubbed
         //any trace of its existence at this point.
         //running the tree transformations below would simply
         //corrupt the trees.
         if (block->nodeIsRemoved())
            {
            if (trace())
      			traceMsg(comp(), "An orphaned region starting with block_%d was removed\n", block->getNumber());
            //we cannot trust anything that block used to point to
            //so we better backtrack to block's predecessor
            //and re-run the previous iteration
            //to figure out the next successor
            //
            //The reason why we believe it is okay to backtrack to the predecessor
            //rather than start over again is that we process the blocks
            //in an IL order meaning whichever orphaned block we encounter first we
            //will treat it as a "head" of a loop which removes the rest of the loop
            //i.e. we cannot start from the block that is in the middle of the loop
            //as if there were any blocks before it we would have processed those first
            treetop = predBBStart;
            continue;
            }


         //dumpOptDetails(comp(), "%s remove guard from cold block_%d\n", OPT_DETAILS, block->getNumber());

         TR::TransformUtil::removeTree(comp(), block->getLastRealTreeTop());

         TR::Node *gotoNode = TR::Node::create(block->getLastRealTreeTop()->getNode(), TR::Goto);
         TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
         TR::TreeTop *lastTree = block->getLastRealTreeTop();
         gotoTree->join(lastTree->getNextTreeTop());
         lastTree->join(gotoTree);
         gotoNode->setBranchDestination(dest->getEntry());
         }

      if (node->getOpCodeValue() == TR::BBStart)
         treetop = block->getExit()->getNextTreeTop();
      }
   }




void TR_VirtualGuardTailSplitter::initializeDataStructures()
   {
   vcount_t visitCount = comp()->incVisitCount();

   List     <VGInfo>   allGuards(trMemory());
   List     <TR::Block> dfsList(trMemory());
   ListStack<VGInfo>   vgStack(trMemory());

   uint32_t            numGuards = 0;

   // This CFG walk is very similar to a depth first search.  We proceed
   // in a DFS manner unless we encounter a virtual guard.
   //
   // Once we are inside a virtual guard, we make sure that we have visited
   // all the nodes between the branch point and merge point of the guard
   // before we visit the merge point.
   //
   // A major assumption is that the branch point dominates a the contents of the
   // virtual guard, and the merge point post-dominates the contents.  This is
   // not true when there are exception edges in the contents.  To solve this problem
   // we refuse to recognize a guarded call with exception successors as a valid
   // virtual guarded call.
   //
   dfsList.add(toBlock(_cfg->getStart()));
   while (!dfsList.isEmpty())
      {
      TR::Block *block = dfsList.popHead();
      VGInfo   *top   = vgStack.top();

      if (top && block == top->getMergeBlock())
         {
         // make sure all predecessors of the merge point have
         // been visited
         //
         bool notDone = false;

         TR_PredecessorIterator it(block);
         for (TR::CFGEdge *edge = it.getFirst();
              edge && !notDone;
              edge = it.getNext())
            {
            if (edge->getFrom()->getVisitCount() != visitCount)
               notDone = true;
            }

         if (notDone)
            continue;

         vgStack.pop();

         // repush itself (to get to the else part below)
         dfsList.add(block);
         continue;
         }
      else            //  (this is the else the above comment is talking abt. )
         {
         if (block->getVisitCount() == visitCount)
            continue;
         block->setVisitCount(visitCount);

         VGInfo *info = recognizeVirtualGuard(block, top);

         if (info)
            {
            allGuards.add(info);
            vgStack.push (info);
            ++numGuards;
            }
         }

      TR_SuccessorIterator it(block);
      for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
         {
         TR::Block *to = toBlock(edge->getTo());
         if (!to->isOSRInduceBlock() && !to->isOSRCatchBlock() && !to->isOSRCodeBlock())
            dfsList.add(toBlock(edge->getTo()));
         }
      }


   if (trace())
      traceMsg(comp(), "Disjoint set forest:\n");

   // Put the information into the virtual guard table
   //
   _numGuards = numGuards;
   _table = (VGInfo **) trMemory()->allocateStackMemory(numGuards * sizeof(VGInfo *));
   ListIterator<VGInfo> it(&allGuards);
   uint32_t i = 0;
   for (VGInfo *info = it.getFirst(); info; info = it.getNext())
      {
      putGuard(i++, info);
      if (trace())
         traceMsg(comp(), "%d -> %d\n", info->getNumber(), info->getParent()->getNumber());
      }
   }

// Given any block, return if is a virtual guard
//
TR_VirtualGuardTailSplitter::VGInfo *TR_VirtualGuardTailSplitter::getVirtualGuardInfo(TR::Block *block)
   {
   if (!block->getExit()) return 0;
   else if (block->getLastRealTreeTop()->getNode()->isTheVirtualGuardForAGuardedInlinedCall())
      return getGuard(block);
   return 0;
   }

void TR_VirtualGuardTailSplitter::splitLinear(TR::Block *start, TR::Block *end)
   {
   List<TR::Block> stack(trMemory());
   List<VGInfo> deferred(trMemory());

   stack.add(start);
   while (!stack.isEmpty())
      {
      TR::Block *block = stack.popHead();

      if (block->getVisitCount() == _visitCount)
         continue;
      block->setVisitCount(_visitCount);

      VGInfo *info = getVirtualGuardInfo(block);
      if (info)
         {
         if (!info->isLeaf())
            deferred.add(info);

         block = lookAheadAndSplit(info, &stack);

         // If the return block is a guard, repush it and continue
         info = getVirtualGuardInfo(block);
         if (info)
            {
            stack.add(block);
            continue;
            }

         if (block->getVisitCount() == _visitCount)
            continue;
         block->setVisitCount(_visitCount);
         }

      if (block != toBlock(_cfg->getEnd()))
         {
         TR_SuccessorIterator it(block);
         for (TR::CFGEdge *edge = it.getFirst(); edge; edge = it.getNext())
            {
            stack.add(toBlock(edge->getTo()));
            }
         }
      }

   ListIterator<VGInfo> it(&deferred);
   for (VGInfo *info = it.getCurrent(); info; info = it.getNext())
      splitLinear(info->getFirstInlinedBlock(), info->getMergeBlock());
   }

// This analysis recognizes and merges virtual guards that exisit
// on a linear control flow path.  For example:
//
//       |     The restriction is that 'start' and 'end' of the
//      / \    merge must form a proper interval (start dominates everything,
//      \ /    end post-dominates everything).  This is enforced by ensuring
//       |     that all the nodes between the virtual guards have only one
//      / \    successor and only one predecessor (except merge nodes, which
//      \ /    necessarily have two predecessors)
//       |
//
// This method walks the cfg in concert with coalesceLinear.  Returns the
// node with which coalesceLinear should proceed its walk. coalesceLinear
// walks the cfg until it hits a virtual guards, at which point, it asks
// lookAheadAndMerge to attempt to discover and merge virtual guards
// from this guard onwards.  lookAheadAndMerge analyzes the control flow
// after the guard. The blocks that it encounters should be skipped by the
// callee. It returns the last block that it encounteres, and the calle
// proceeds the dfs walk from there onwards.  (apologies for the non-intuitive
// control flow)
//
TR::Block *TR_VirtualGuardTailSplitter::lookAheadAndSplit(VGInfo *guard, List<TR::Block> *stack)
   {
   VGInfo *lastInfo = 0;
   List<VGInfo> deferred(trMemory());

   TR::Block *cursor = guard->getMergeBlock();
   bool isMergeBlock = true;
   while (cursor->getExit() != NULL)
      {
        //if (!cursor->getExceptionSuccessors().empty())
        // break;

      if (isMergeBlock)
         {
         if (!(cursor->getPredecessors().size() == 2))
            break;
         }
      else
         if (!(cursor->getPredecessors().size() == 1))
            break;

      VGInfo *info = getVirtualGuardInfo(cursor);
      if (info && !info->stillExists())
         break;

      if (info)
         {
         // We should have a look at the inlined portion of the code to
         // see if there was any opportunity to optimize there.  But don't
         // do that now, add to the deferred list.
         //
         if (!info->isLeaf())
            deferred.add(info);

         lastInfo = info;
         isMergeBlock = true;
         cursor = info->getMergeBlock();
         continue;
         }

      TR::CFGEdgeList &succs = cursor->getSuccessors();
      if (!(succs.size() == 1))
         {
         TR::Block *newCursor = NULL;
         TR::TreeTop *lastTree = cursor->getLastRealTreeTop();
         if ((succs.size() == 2) &&
             !lastTree->getNode()->getOpCode().isSwitch())
            {
            TR::CFGEdge *succ1 = succs.front();
            TR::CFGEdge *succ2 = *(++succs.begin());
            TR::Block *succBlock1 = toBlock(succ1->getTo());

            if (/* succBlock1->getSuccessors().size() == 1) && */ (succBlock1 == cursor->getNextBlock()))
               {
               //TR::Block *nextBlock = toBlock(succBlock1->getSuccessors().front()->getTo());
               TR::Block *nextBlock = succBlock1;
               VGInfo *info = getVirtualGuardInfo(nextBlock);
               if (info && info->stillExists())
                  {
                  newCursor = nextBlock;
                  stack->add(toBlock(succ2->getTo()));
                  }
               }

            if (!newCursor)
               {
               TR::Block *succBlock2 = toBlock(succ2->getTo());
               if (/* succBlock2->getSuccessors().size() == 1) && */ (succBlock2 == cursor->getNextBlock()))
                  {
                  //TR::Block *nextBlock = toBlock(succBlock2->getSuccessors().front()->getTo());
                  TR::Block *nextBlock = succBlock2;
                  VGInfo *info = getVirtualGuardInfo(nextBlock);
                  if (info && info->stillExists())
                     {
                     newCursor = nextBlock;
                     stack->add(succBlock1);
                     }
                  }
               }
            }

         if (newCursor)
            cursor = newCursor;
         else
            break;
         }
      else
         cursor = toBlock(succs.front()->getTo());

      isMergeBlock = false;
      } // while(cursor->getExit() != NULL)

   if (lastInfo)
      transformLinear(guard->getBranchBlock(), lastInfo->getMergeBlock());

   ListIterator<VGInfo> it(&deferred);
   for (VGInfo *info = it.getCurrent(); info; info = it.getNext())
      {
      splitLinear(info->getFirstInlinedBlock(), info->getMergeBlock());
      }

   return cursor;
   }

// Given a proper interval marked by first and last, split
// all guards that fall between first and last, cloning any
// common blocks in between
//                                   ________
//   _/\__/\__/\_  transforms into _/ \  \   \_
//    \/  \/  \/                    \__\__\__/
//
void TR_VirtualGuardTailSplitter::transformLinear(TR::Block *first, TR::Block *last)
   {
   VGInfo *firstInfo = getVirtualGuardInfo(first);
   TR_ASSERT(firstInfo, "first should be a virtual guard");

   TR::Block *call = firstInfo->getCallBlock();   // used as a cursor (a misnomer)
   TR::Block *next = firstInfo->getMergeBlock();  // used as a cursor

   while (next != last)
      {
      TR_BlockCloner cloner(_cfg, true);
      TR::Block *clone = cloner.cloneBlocks(next, next);

      if (_cfg->getStructure())
         {
         next->getStructureOf()->getParent()->asRegion()->
            addSubNode(new (_cfg->structureRegion()) TR_StructureSubGraphNode
                       (new (_cfg->structureRegion()) TR_BlockStructure(comp(), clone->getNumber(), clone)));
         }

      if (trace())
         {
         traceMsg(comp(), "$$$ Processing guards: first %d, last %d\n", firstInfo->getNumber(), last->getNumber());
         traceMsg(comp(), "=> Call node %d, next node %d\n", call->getNumber(), next->getNumber());
         traceMsg(comp(), "=> clone block is %d\n\n", clone->getNumber());
         }


      _splitDone = true;
      _cfg->addEdge(call, clone);
      for (auto e = next->getExceptionSuccessors().begin(); e != next->getExceptionSuccessors().end(); ++e)
         _cfg->addExceptionEdge(clone, (*e)->getTo());

      TR::Block *callNext = call->getNextBlock();
      call->getExit()->join(clone->getEntry());
      if (callNext)
         clone->getExit()->join(callNext->getEntry());
      else
         clone->getExit()->setNextTreeTop(NULL);

      //if (call->getLastRealTreeTop()->getNode()->getOpCode().isBranch())
      if (call->getLastRealTreeTop()->getNode()->getOpCodeValue() == TR::Goto)
         TR::TransformUtil::removeTree(comp(), call->getLastRealTreeTop());

      VGInfo *info = getVirtualGuardInfo(next);
      if (info)
         {
         TR::Block *dest = info->getCallBlock();
         _cfg->addEdge(clone, dest);
         _cfg->removeEdge(call, next);

         TR::TransformUtil::removeTree(comp(), clone->getLastRealTreeTop());

         TR::Node *gotoNode = TR::Node::create(next->getLastRealTreeTop()->getNode(), TR::Goto);
         TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
         TR::TreeTop *lastTree = clone->getLastRealTreeTop();
         gotoTree->join(lastTree->getNextTreeTop());
         lastTree->join(gotoTree);
         gotoNode->setBranchDestination(dest->getEntry());

         info->markRemoved();
         next = info->getMergeBlock();
         call = dest;
         }
      else
         {
         TR::Block *dest = NULL;
         TR::Block *otherDest = NULL;
         TR::CFGEdgeList &succs = next->getSuccessors();
         if (succs.size() == 1)
            {
            dest = toBlock(succs.front()->getTo());
            }
         else if (succs.size() == 2)
            {
            TR::CFGEdge *succ1 = succs.front();
            TR::CFGEdge *succ2 = *(++succs.begin());
            TR::Block *succBlock1 = toBlock(succ1->getTo());
            TR::Block *succBlock2 = toBlock(succ2->getTo());

            if (/* succBlock1->getSuccessors().size() == 1) && */ (succBlock1 == next->getNextBlock()))
               {
               TR::Block *nextBlock = succBlock1;
               VGInfo *info = getVirtualGuardInfo(nextBlock);
               if (info && info->stillExists())
                  {
                  dest = succBlock1;
                  otherDest = succBlock2;
                  }
               }

            if (!dest)
               {
               if (/* succBlock2->getSuccessors().size() == 1) && */ (succBlock2 == next->getNextBlock()))
                  {
                  TR::Block *nextBlock = succBlock2;
                  VGInfo *info = getVirtualGuardInfo(nextBlock);
                  if (info && info->stillExists())
                     {
                     dest = succBlock2;
                     otherDest = succBlock1;
                     }
                  }
               }
            }

         TR_ASSERT(dest, "Should find a destination block in the sequence\n");

         _cfg->addEdge(clone, dest);
         if (otherDest)
            _cfg->addEdge(clone, otherDest);
         _cfg->removeEdge(call, next);

         TR::TreeTop *lastTree = clone->getLastRealTreeTop();
         if (lastTree->getNode()->getOpCode().isBranch())
            {
            if (lastTree->getNode()->getOpCodeValue() == TR::Goto)
               lastTree->getNode()->setBranchDestination(dest->getEntry());
            else
               {
               if (!otherDest)
                  {
                  // A special case: "if (blah) goto 3; else goto 3;"
                  // replace the if by a goto
                  //
                  TR::TransformUtil::removeTree(comp(), lastTree);
                  TR::Node *gotoNode = TR::Node::create(lastTree->getNode(), TR::Goto);
                  gotoNode->setBranchDestination(dest->getEntry());
                  clone->append(TR::TreeTop::create(comp(), gotoNode));
                  }
               }
            }
         else if (lastTree->getNode()->getOpCode().isSwitch())
            {
            // A special case, all cases go the same place
            // replace the switch by a goto
            //
            TR::TransformUtil::removeTree(comp(), lastTree);
            TR::Node *gotoNode = TR::Node::create( lastTree->getNode(), TR::Goto);
            gotoNode->setBranchDestination(dest->getEntry());
            clone->append(TR::TreeTop::create(comp(), gotoNode));
            }
         else
            {
            TR::Node *gotoNode = TR::Node::create(next->getLastRealTreeTop()->getNode(), TR::Goto);
            TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode);
            gotoTree->join(lastTree->getNextTreeTop());
            lastTree->join(gotoTree);
            gotoNode->setBranchDestination(dest->getEntry());
            }

         call = clone;
         next = dest;
         }

      }
   }

TR_VirtualGuardTailSplitter::VGInfo *TR_VirtualGuardTailSplitter::recognizeVirtualGuard
(TR::Block *block, VGInfo *parent)
   {
   if (!block->getExit())
      return 0;

   TR::Node *guardNode = block->getLastRealTreeTop()->getNode();

   if (!block->getLastRealTreeTop()->getNode()->isTheVirtualGuardForAGuardedInlinedCall())
      return 0;

   TR::CFGEdgeList &succ = block->getSuccessors();
   if (!(succ.size() == 2))
      {
      block->getLastRealTreeTop()->getNode()->setLocalIndex(MALFORMED_GUARD);
      return 0;
      }

   TR::Block *first, *second, *call=0, *inlined=0;
   first = toBlock(succ.front()->getTo());
   second = toBlock((*(++succ.begin()))->getTo());

   TR::Node *node = getFirstCallNode(first); //->getFirstRealTreeTop()->getNode()->getFirstChild();
   //TR::Node *node = first->getFirstRealTreeTop()->getNode()->getFirstChild();
   if (node && node->isTheVirtualCallNodeForAGuardedInlinedCall())
      {
      call = first;
      inlined = second;
      }
   else
      {
      node = getFirstCallNode(second); //->getFirstRealTreeTop()->getNode()->getFirstChild();
      //node = second->getFirstRealTreeTop()->getNode()->getFirstChild();
      if (node && node->isTheVirtualCallNodeForAGuardedInlinedCall())
         {
         call = second;
         inlined = first;
         }
      }

#ifdef J9_PROJECT_SPECIFIC
   if (call && guardNode->isProfiledGuard())
      {
      TR_ValueProfileInfoManager *profileManager = TR_ValueProfileInfoManager::get(comp());
      TR_AddressInfo *  valueInfo = (TR_AddressInfo *)profileManager->getValueInfo(node->getByteCodeInfo(),
                                                               comp(),
                                                               TR_ValueProfileInfoManager::allProfileInfoKinds,
                                                               NotBigDecimalOrString);


      float profiledGuardProbabilityThreshold = 0.98f;
      static char *profiledGuardSplitProbabilityThresholdStr = feGetEnv("TR_ProfiledGuardSplitProbabilityThreshold");
      if (profiledGuardSplitProbabilityThresholdStr)
         {
         profiledGuardProbabilityThreshold = ((float)atof(profiledGuardSplitProbabilityThresholdStr));
         }

      bool allowSplit = false;
      if (valueInfo)
         {
         if (valueInfo->getTopProbability() >= profiledGuardProbabilityThreshold)
            allowSplit = true;
         }

      if (!allowSplit)
         {
         guardNode->setLocalIndex(MALFORMED_GUARD);
         return 0;
         }
      }
#endif

   // the call-block must not have any exception edges.  exception edges break the
   // assumption that a virtual guard interval is a proper interval.
   //
   /////if (!call /* || !call->getExceptionSuccessors().empty() */)
   if (!call || !(call->getSuccessors().size() == 1))
      {
      block->getLastRealTreeTop()->getNode()->setLocalIndex(MALFORMED_GUARD);
      return 0;
      }

   TR_ASSERT((call->getSuccessors().size() == 1), "malformed virtual guard");
   TR::Block * merge = toBlock(call->getSuccessors().front()->getTo());

   //check if the merge block has back edges, if so exclude such guards from this opt to
   //prevent dead zone in initializeDataStructures()
   bool invalidMergeBlock = ( merge == _cfg->getEnd() || merge->getPredecessors().size() > 2);
   if (invalidMergeBlock)
      {
      block->getLastRealTreeTop()->getNode()->setLocalIndex(MALFORMED_GUARD);
      return 0;
      }

   return new (trStackMemory()) VGInfo(block, call, inlined, merge, parent);
   }


bool TR_VirtualGuardTailSplitter::isKill(TR::Block *block)
   {
   if (!block->getEntry()) return false;

   TR::TreeTop *tree = block->getFirstRealTreeTop();
   TR::TreeTop *exit = block->getExit();

   for (; tree != exit; tree = tree->getNextRealTreeTop())
      {
      if (isKill(tree->getNode()))
          return true;
      }

   return false;
   }

bool TR_VirtualGuardTailSplitter::isKill(TR::Node *node)
   {
   TR::ILOpCode &opcode = node->getOpCode();

   if (opcode.isCall())
      return true;

   if (opcode.hasSymbolReference() &&
       node->getSymbolReference()->isUnresolved())
      return true;

   for (int c = node->getNumChildren() - 1; c >= 0; --c)
      {
      if (isKill(node->getChild(c)))
         return true;
      }
   return false;
   }

void TR_VirtualGuardTailSplitter::putGuard(uint32_t index, VGInfo *info)
   {
   TR_ASSERT(index < _numGuards, "overflow");
   TR::Block *branch = info->getBranchBlock();
   TR::Node  *node   = branch->getLastRealTreeTop()->getNode();

   node->setLocalIndex(index);
   _table[index] = info;
   }

void TR_VirtualGuardTailSplitter::VGInfo::markRemoved()
   {
   _parent->removeChild(this);
   _branch = 0;
   _valid = 0;

   }


void TR_VirtualGuardTailSplitter::splitGlobal()
   {
   TR_BlockCloner *cloner=_cfg->clone();

   // Breadth First Search the graph
   //
   comp()->incVisitCount();

   TR_Queue<VGInfo> queue(trMemory());
   for (int32_t i = 0; i < _numGuards; ++i)
      {
      VGInfo *info = getGuard(i);
      if (info->isLeaf())
         queue.enqueue(info);
      }

   while (!queue.isEmpty())
      {
      VGInfo *guard = queue.dequeue();
      if (guard->stillExists() && guard->isLeaf())
         {
         remergeGuard(*cloner, guard);
         queue.enqueue(guard->getParent());
         guard->markRemoved();
         }
      }
   _cfg->removeNode(cloner->getToBlock(comp()->getStartTree()->getNode()->getBlock()));
   }

void TR_VirtualGuardTailSplitter::remergeGuard(TR_BlockCloner &cloner, VGInfo *info)
   {
   TR::Block *block = info->getBranchBlock();

   dumpOptDetails(comp(), "%sperforming global split on guard block_%d\n", OPT_DETAILS, block->getNumber());

   //        -----                -----
   //        | G |                | G'|
   //        -----                -----
   //         / \                  / \
   //        /   \                /   \
   //     ----- -----          ----- -----
   //     | A | | B |          | A'| | B'|
   //     ----- -----          ----- -----

   // Is transformed to:
   //

   //        -----                -----
   //        | G |                | G'|
   //        -----                -----
   //         /   \__________________\
   //        /                        \
   //     ----- -----          ----- -----
   //     | A | | B |          | A'| | B'|
   //     ----- -----          ----- -----

   // Block B and A' will be killed as a result,

   //        -----                -----
   //        | G |                | G'|
   //        -----                -----
   //          |\__________________ |
   //          |                   \|
   //        -----                -----
   //        | A |                | B'|
   //        -----                -----


   TR::Block *blockA = info->getFirstInlinedBlock();
   TR::Block *cloneA = cloner.getToBlock(blockA);
   TR::Block *blockB = info->getCallBlock();
   TR::Block *cloneB = cloner.getToBlock(blockB);

   TR::Block *cloneG = cloner.getToBlock(block);

   _cfg->addEdge(block, cloneB);
   _cfg->removeEdge(block, blockB);
   _cfg->removeEdge(cloneG, cloneA);

   // convert the last treetop in G' into a goto
   //
   TR::TreeTop *ifTree = cloneG->getLastRealTreeTop();
   ifTree->getNode()->removeAllChildren();
   TR::Node::recreate(ifTree->getNode(), TR::Goto);

   // Fix the if tree in G
   //
   block->getLastRealTreeTop()->getNode()->setBranchDestination(cloneB->getEntry());

#if 0
   char fileName[20];
   sprintf(fileName, "split%d.vcg", block->getNumber());
   FILE *file = fopen(fileName, "w");
   comp->getDebug()->printVCG(file, _cfg, fileName);
   fclose(file);
#endif

   if (trace())
      traceMsg(comp(), "Split Guard Block %d->(%d,%d), %d->(%d,%d)\n",
                  block ->getNumber(), blockA->getNumber(), blockB->getNumber(),
                  cloneG->getNumber(), cloneA->getNumber(), cloneB->getNumber());
   }





int32_t TR_VirtualGuardTailSplitter::rematerializeThis()
   {
   int32_t numGuardsChanged = 0;
   vcount_t visitCount = comp()->incVisitCount();
   TR::Block *block = comp()->getStartTree()->getNode()->getBlock();
   TR::SparseBitVector symbolReferencesInNode(comp()->allocator());
   List<TR_SymNodePair> symNodePairs(trMemory());
   List<TR::Block> splitBlocks(trMemory()), guardBlocks(trMemory());
   TR::Block *prevBlock = NULL, *startOfExtendedBlock = block;

   for (; block; block = block->getNextBlock())
      {
      if (prevBlock)
         {
         bool isSuccessor = false;
         for (auto edge = prevBlock->getSuccessors().begin(); edge != prevBlock->getSuccessors().end(); ++edge)
            {
            if (block == (*edge)->getTo())
              {
              isSuccessor = true;
              break;
              }
            }

         if (!isSuccessor)
            {
            symNodePairs.deleteAll();
            splitBlocks.deleteAll();
            }
         else
            startOfExtendedBlock = block;

         }
      else
         startOfExtendedBlock = block;

      prevBlock = block;
      TR::TreeTop * tt = block->getLastRealTreeTop();
      TR::Node *node = tt->getNode();
      TR::Node *thisExpr = NULL;
      TR::Node *callNode = NULL, *storeNode = NULL;
      bool canRematerialize = true;
      TR::SymbolReference *symRef = NULL;
      TR::Block *storeBlock = block;

      //if (counter > 3)
      //        break;

      if (node->isNopableInlineGuard())
         {
         TR::Block *guardBlock = block;
         guardBlocks.deleteAll();
         TR::TreeTop *prevTree = tt->getPrevTreeTop();
         TR::Node *prevNode = prevTree->getNode();
         callNode = node->getVirtualCallNodeForGuard();
         if (callNode &&
             callNode->getOpCode().isCallIndirect())
            {
            TR::Node *thisChild = callNode->getSecondChild();
            if (thisChild->getOpCode().isLoadVarDirect() &&
                thisChild->getSymbolReference()->getSymbol()->isAuto())
               {
               while (guardBlock)
                  {
                  while (prevTree &&
                         prevTree != startOfExtendedBlock->getEntry())
                     {
                     prevNode = prevTree->getNode();

                     //dumpOptDetails(comp(), "For guard %p searching currNode %p till %p(block_%d)\n", node, prevNode, startOfExtendedBlock->getEntry(), startOfExtendedBlock->getNumber());

                     if (prevNode->getOpCode().isStoreDirect() &&
                         prevNode->getSymbolReference() == thisChild->getSymbolReference())
                        {
                        dumpOptDetails(comp(), "Found store node %p for guard %p\n", prevNode, node);
                        storeNode = prevNode;
                        symRef = prevNode->getSymbolReference();
                        break;
                        }

                     if (prevNode->getOpCodeValue() == TR::BBEnd)
                        storeBlock = prevNode->getBlock();
                     else if (prevNode->isNopableInlineGuard())
                        guardBlocks.add(storeBlock);

                     prevTree = prevTree->getPrevTreeTop();
                     }

                  if (symRef &&
                      storeBlock)
                     break;

                  VGInfo *vgInfo = getVirtualGuardInfo(guardBlock);
                  VGInfo *parentInfo = NULL;
                  guardBlock = NULL;

                  if (vgInfo)
                     parentInfo = vgInfo->getParent();

                  if (!parentInfo ||
                      (parentInfo == vgInfo))
                     break;
                  else
                     {
                     guardBlock = parentInfo->getBranchBlock();
                     startOfExtendedBlock = guardBlock;
                     while (startOfExtendedBlock->isExtensionOfPreviousBlock())
                        startOfExtendedBlock = startOfExtendedBlock->getPrevBlock();
                     prevTree = guardBlock->getExit();
                     }
                  }


               if (symRef &&
                   storeBlock)
                  {
                  thisExpr = prevNode->getFirstChild();

                  if (thisExpr->getOpCode().isLoadVarDirect() &&
                      (thisExpr->getSymbolReference() == symRef))
                     {
                     TR::Block *cursorBlock = storeBlock;
                     TR::TreeTop *cursorPrev = prevTree->getPrevTreeTop();
                     while (cursorPrev &&
                            (cursorPrev != startOfExtendedBlock->getEntry()))
                        {
                        TR::Node *cursorPrevNode = cursorPrev->getNode();
                        if (cursorPrevNode->getOpCode().isStoreDirect() &&
                            (cursorPrevNode->getSymbolReference() == symRef))
                           {
                           thisExpr = cursorPrevNode->getFirstChild();
                           break;
                           }

                        cursorPrev = cursorPrev->getPrevTreeTop();
                        //if (cursorPrev == cursorBlock->getEntry())
                        //   {
                        //   if (cursorBlock->getPredecessors().find(cursorBlock->getPrevBlock()))
                        //     cursorBlock = cursorBlock->getPrevBlock();
                        //   }
                        }
                     }
                  }
               }
            }
         }

      if (thisExpr)
         {
           //dumpOptDetails(comp(), "This expr %p\n", thisExpr);
         symbolReferencesInNode.Clear();
         vcount_t visitCount = comp()->incVisitCount();
         int32_t numDeadSubNodes = 0;
         collectSymbolReferencesInNode(thisExpr, symbolReferencesInNode, &numDeadSubNodes, visitCount, comp());

         TR::TreeTop *treeTop;
         for (treeTop = storeBlock->getEntry(); treeTop != tt; treeTop = treeTop->getNextTreeTop())
            {
            TR::Node *cursorNode = treeTop->getNode();
            TR::SymbolReference *currSymRef = NULL;
            if (cursorNode->getOpCode().isResolveCheck())
               {
               // Treat unresolved references like a call, except for field references.
               // (Field references can only escape to user code if the class has to be
               //  loaded. This can only happen if the underlying object is null, and
               //  so an exception will be thrown anyway).
               //
               currSymRef = cursorNode->getSymbolReference();
               cursorNode = cursorNode->getFirstChild();
               if (cursorNode->getOpCode().isIndirect() &&
                   cursorNode->getOpCode().isLoadVarOrStore())
                  continue;
               }
            else
               {
               if (cursorNode->getOpCodeValue() == TR::treetop ||
                   cursorNode->getOpCode().isNullCheck() ||
                   cursorNode->getOpCodeValue() == TR::ArrayStoreCHK)
                  {
                  cursorNode = cursorNode->getFirstChild();
                  }

               if (!cursorNode->getOpCode().hasSymbolReference())
                  continue;

               currSymRef = cursorNode->getSymbolReference();

               if (!cursorNode->getOpCode().isLoad())
                  {
                  // For a store, just the single symbol reference is killed.
                  // Resolution of the store symbol is handled by TR::ResolveCHK
                  //
                  if (symbolReferencesInNode.ValueAt(currSymRef->getReferenceNumber()))
                     {
                       //dumpOptDetails(comp(), "0Cannot rematerialize for guard %p because of node %p\n", node, cursorNode);
                     canRematerialize = false;
                     break;
                     }

                  if (!cursorNode->getOpCode().isCall() &&
                      ((cursorNode->getOpCodeValue() != TR::monent) &&
                       (cursorNode->getOpCodeValue() != TR::monexit)))
                     continue;
                  }
               else
                  {
                  // Loads have no effect. Resolution is handled by TR::ResolveCHK
                  //
                  continue;
                  }
               }

            // Check if the definition modifies any symbol in the subtree
            //
            if (currSymRef != NULL)
               {
               if (currSymRef->getUseDefAliases().containsAny(symbolReferencesInNode, comp()))
                  {
                    //dumpOptDetails(comp(), "1Cannot rematerialize for guard %p because of node %p\n", node, cursorNode);
                  canRematerialize = false;
                  break;
                  }
               }
            }
         }
      else
         canRematerialize = false;

      if (canRematerialize)
         {
         if (thisExpr->getNumChildren() > 0)
            comp()->incVisitCount();

         canRematerialize = isLegalToClone(thisExpr, comp()->getVisitCount());
         }

      if (canRematerialize)
        {
           ///dumpOptDetails(comp(), "Success: Can rematerialize for guard %p\n", node);
         #ifdef DEBUG
         printf("Found opportunity in %s\n", comp()->signature());
         #endif

         TR::Node *origThis = callNode->getFirstChild()->getFirstChild();
         //TR::Node::recreate(origThis, thisExpr->getOpCodeValue());
         //origThis->setSymbolReference(thisExpr->getSymbolReference());

         numGuardsChanged++;
         TR_SymNodePair *symNodePair = (TR_SymNodePair *)trMemory()->allocateStackMemory(sizeof(TR_SymNodePair));
         memset(symNodePair, 0, sizeof(TR_SymNodePair));
         symNodePair->_symRef = symRef;
         symNodePair->_node = origThis;
         symNodePairs.add(symNodePair);

         TR::Node *newThis = TR::Node::createWithSymRef(thisExpr, thisExpr->getOpCodeValue(), thisExpr->getNumChildren(), thisExpr->getSymbolReference());

         //TR::TreeTop *callTree = node->getVirtualCallTreeForGuard();
         //while (callTree &&
         //        (callTree->getNode()->getOpCodeValue() != TR::BBStart))
         //  callTree = callTree->getPrevTreeTop();

         TR::Block *callBlock = node->getBranchDestination()->getNode()->getBlock();
         TR::Block *splitBlock = block->splitEdge(block, callBlock, comp());
         splitBlock->setIsCold();
         splitBlock->setFrequency(VERSIONED_COLD_BLOCK_COUNT);
         splitBlocks.add(splitBlock);

         TR::Node *duplicateStore = storeNode->duplicateTree();

         int32_t i;
         //for (i = 0; i < origThis->getNumChildren(); i++)
         //   origThis->getChild(i)->recursivelyDecReferenceCount();

         //origThis->setNumChildren(thisExpr->getNumChildren());
         if (thisExpr->getNumChildren() > 0)
            comp()->incVisitCount();
         for (i = 0; i < thisExpr->getNumChildren(); i++)
            {
            TR::Node *duplicateChild = thisExpr->getChild(i)->duplicateTree();
            canonicalizeTree(duplicateChild, symNodePairs, comp()->getVisitCount());
            newThis->setAndIncChild(i, duplicateChild);
            }

         TR::Node *newStore = TR::Node::createWithSymRef(TR::astore, 1, 1, newThis, storeNode->getSymbolReference());
         TR::TreeTop *newStoreTree = TR::TreeTop::create(comp(), newStore, NULL, NULL);
         TR::TreeTop *splitEntry = splitBlock->getEntry();
         TR::TreeTop *splitNext = splitEntry->getNextTreeTop();
         splitEntry->join(newStoreTree);
         newStoreTree->join(splitNext);

         // handle guard blocks

         /*
         if (origThis != callNode->getSecondChild())
            {
            TR::Node *secondChild = callNode->getSecondChild();
            TR::Node::recreate(secondChild, thisExpr->getOpCodeValue());
            secondChild->setSymbolReference(thisExpr->getSymbolReference());

            int32_t i;
            for (i = 0; i < secondChild->getNumChildren(); i++)
               secondChild->getChild(i)->recursivelyDecReferenceCount();

              secondChild->setNumChildren(thisExpr->getNumChildren());
            //if (thisExpr->getNumChildren() > 0)
            //   comp()->incVisitCount();
            for (i = 0; i < origThis->getNumChildren(); i++)
               {
                 //canonicalizeTree(thisExpr->getChild(i), symNodePairs, comp()->getVisitCount());
               secondChild->setAndIncChild(i, origThis->getChild(i));
               }
            }
         */
         }
      }

   //comp()->dumpMethodTrees("After rematerialize this");

   return numGuardsChanged;
   }



void TR_VirtualGuardTailSplitter::canonicalizeTree(TR::Node *node, List<TR_SymNodePair> &symNodePairs, vcount_t visitCount)
   {
     //dumpOptDetails(comp(), "Reached 0 canonicalized for node %p\n", node);

   if (node->getVisitCount() == visitCount)
      return;

   //dumpOptDetails(comp(), "Reached 1 canonicalized for node %p\n", node);
   bool canonicalized = false;
   if (node->getOpCode().isLoadVarDirect())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      ListIterator<TR_SymNodePair> symNodeIt(&symNodePairs);
      TR_SymNodePair *pair;
      for (pair = symNodeIt.getFirst(); pair != NULL; pair = symNodeIt.getNext())
         {
           //dumpOptDetails(comp(), "Looking at symRef %d(%p) this symRef %d(%p)\n", pair->_symRef->getReferenceNumber(), pair->_symRef, symRef->getReferenceNumber(), symRef);
         if (symRef == pair->_symRef)
            {
              //dumpOptDetails(comp(), "Matched for node %p\n", node);
            TR::Node::recreate(node, pair->_node->getOpCodeValue());
            node->setSymbolReference(pair->_node->getSymbolReference());
             node->setNumChildren(pair->_node->getNumChildren());
            canonicalized = true;
            if (pair->_node->getNumChildren() > 0)
               comp()->incVisitCount();

            int32_t i;
            for (i = 0; i < pair->_node->getNumChildren(); i++)
               {
               TR::Node *duplicateChild = pair->_node->getChild(i)->duplicateTree();
               //dumpOptDetails(comp(), "Called canonicalized for child %p\n", duplicateChild);
               canonicalizeTree(duplicateChild, symNodePairs, comp()->getVisitCount());
               node->setAndIncChild(i, duplicateChild);
               }

            }
         }
      }

   if (!canonicalized)
      {
      int32_t i;
      for (i = 0; i < node->getNumChildren(); i++)
         canonicalizeTree(node->getChild(i), symNodePairs, visitCount);
      }
   }




bool TR_VirtualGuardTailSplitter::isLegalToClone(TR::Node *node, vcount_t visitCount)
   {
     //dumpOptDetails(comp(), "Reached 0 canonicalized for node %p\n", node);

   if (node->getVisitCount() == visitCount)
      return true;

   node->setVisitCount(visitCount);

   //dumpOptDetails(comp(), "Reached 1 canonicalized for node %p\n", node);
   bool canonicalized = false;
   if (node->getOpCode().isCall() ||
       node->getOpCodeValue() == TR::New ||
       node->getOpCodeValue() == TR::newarray ||
       node->getOpCodeValue() == TR::anewarray ||
       node->getOpCodeValue() == TR::multianewarray ||
       node->getOpCodeValue() == TR::MergeNew)
      return false;


   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      if (!isLegalToClone(node->getChild(i), visitCount))
        return false;
      }

   return true;
   }



TR::Node *TR_VirtualGuardTailSplitter::getFirstCallNode(TR::Block *block)
   {
   TR::TreeTop *treeTop = block->getFirstRealTreeTop();
   TR::TreeTop *exitTree = block->getExit();
   while (treeTop != exitTree)
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCode().isCall())
         return node;
      else if (node->getNumChildren() > 0)
         {
         if (node->getFirstChild()->getOpCode().isCall())
            return node->getFirstChild();
         }

      treeTop = treeTop->getNextTreeTop();
      }

   return NULL;
   }

const char *
TR_VirtualGuardTailSplitter::optDetailString() const throw()
   {
   return "O^O VIRTUAL GUARD COALESCER: ";
   }

TR_InnerPreexistence::TR_InnerPreexistence(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t
TR_InnerPreexistence::perform()
   {
   // disable opt if virtual guard-noping is not enabled
   if (!comp()->performVirtualGuardNOPing())
      return 0;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   if (trace())
      comp()->dumpMethodTrees("Trees before InnerPreexistence");

   int32_t candidates = initialize();

   if (candidates > 0)
      {
      transform();
      }

   return 1;
   }

int32_t
TR_InnerPreexistence::initialize()
   {
   _numInlinedSites = comp()->getNumInlinedCallSites();
   // array of guard blocks index by the inlined call site index of the guarded method
   TR::Block **guardBlocks  = (TR::Block **) trMemory()->allocateStackMemory(_numInlinedSites * sizeof(TR::Block *));
   memset(guardBlocks, 0, _numInlinedSites * sizeof(TR::Block *));

   int32_t numGuards = 0;
   TR::TreeTop *treeTop, *exitTree;
   for (treeTop = comp()->getStartTree(); treeTop; treeTop = exitTree->getNextTreeTop())
      {
      TR::Block *block = treeTop->getNode()->getBlock();
      exitTree = block->getExit();

      TR::Node *lastNode = block->getLastRealTreeTop()->getNode();
      if (lastNode->isTheVirtualGuardForAGuardedInlinedCall())
         {
         TR_VirtualGuard *info = comp()->findVirtualGuardInfo(lastNode);

         // Both the inner guard and the outer guard must be nop-able for inner preexistence
         // to work. Ie. both the guards must give rise to assumptions
         //
         if (info->isNopable())
            {
            int16_t methodIndex = lastNode->getInlinedSiteIndex();
            guardBlocks[methodIndex] = block;
            numGuards++;
            }
         }
      }

   if (numGuards == 0)
      return 0;

   _guardTable = (GuardInfo **) trMemory()->allocateStackMemory(_numInlinedSites * sizeof(GuardInfo *));
   memset(_guardTable, 0, _numInlinedSites * sizeof(GuardInfo*));

   _vnInfo = optimizer()->getValueNumberInfo();

   int32_t numInnerGuards = 0;
   for (int32_t i = 0; i < _numInlinedSites; ++i)
      {
      TR::Block *guard = guardBlocks[i];
      if (!guard) continue;

      int16_t cursor = i;
      GuardInfo *outerGuard = 0;
      do
         {
         TR_InlinedCallSite &ics = comp()->getInlinedCallSite(cursor);
         int16_t caller = ics._byteCodeInfo.getCallerIndex();

         if (caller == -1)
            break;

         outerGuard = _guardTable[caller];

         cursor = caller;
         }
      while (cursor >= 0 && !outerGuard);

      GuardInfo *info = new (trStackMemory()) GuardInfo(comp(), guard, outerGuard, _vnInfo, _numInlinedSites);
      if (outerGuard)
         numInnerGuards++;
      _guardTable[i] = info;
      }


   return numInnerGuards;
   }

TR_InnerPreexistence::GuardInfo::GuardInfo(TR::Compilation * comp, TR::Block *block, GuardInfo *parent, TR_ValueNumberInfo *vnInfo, uint32_t numInlinedSites)
   : _block(block), _parent(parent), _hasBeenDevirtualized(false)
   {
   TR::Node *guardNode = block->getLastRealTreeTop()->getNode();
   TR::Node * callNode = guardNode->getVirtualCallNodeForGuard();
   TR_ASSERT(callNode->getOpCode().isIndirect(), "Guarded calls must be indirect");

   _argVNs = new (comp->trStackMemory()) TR_BitVector(20, comp->trMemory(), stackAlloc, growable);
   _innerSubTree = new (comp->trStackMemory()) TR_BitVector(numInlinedSites, comp->trMemory(), stackAlloc, notGrowable);

   int32_t firstArgIndex = callNode->getFirstArgumentIndex();
   _thisVN = vnInfo->getValueNumber(callNode->getChild(firstArgIndex));
   _argVNs->set(_thisVN);
   for (int32_t i = callNode->getNumChildren() - 1; i > firstArgIndex; --i)
      {
      TR::Node *child = callNode->getChild(i);
      if (child->getDataType() == TR::Address)
         _argVNs->set(vnInfo->getValueNumber(child));
      }
   }

void
TR_InnerPreexistence::transform()
   {
   int32_t i;
   for (i = _numInlinedSites - 1; i > 0; --i) // intentionally skipping i = 0 case
      {
      GuardInfo *info = _guardTable[i];
      if (!info)
         continue;

      // Find out which ancestors we can reach from here
      //
      GuardInfo *ancestor = info->getParent();
      while (ancestor)
         {
         if (ancestor->getArgVNs()->isSet(info->getThisVN()))
            {
            //info->setReachableAncestor(ancestor);
            ancestor->setVisible(i);
            }
         ancestor = ancestor->getParent();
         }
      }

   for (i = 0; i < _numInlinedSites; ++i)
      {
      GuardInfo *info = _guardTable[i];
      if (!info)
         continue;

      GuardInfo *parent = info->getParent();

      if (trace())
         {
         TR_BitVectorIterator bvi;

         traceMsg(comp(), "Site %d (block_%d, parent-block_%d): thisVN: %d, argsVNs: {",
                     i, info->getBlock()->getNumber(),
                     parent ? parent->getBlock()->getNumber() : -1,
                     info->getThisVN());
         bvi.setBitVector(*(info->getArgVNs()));
         while (bvi.hasMoreElements())
            {
            uint32_t c = bvi.getNextElement();
            traceMsg(comp(), "%d ", c);
            }
         traceMsg(comp(), "}\n\tReachable Subtree: {");

         bvi.setBitVector(*(info->getVisibleGuards()));
         while (bvi.hasMoreElements())
            {
            uint32_t c = bvi.getNextElement();
                traceMsg(comp(), "%d ", c);
            }
         traceMsg(comp(), "}\n");
         }
      }

   // FIXME: the following is a heuristic, probably we can do better
   //
   bool somethingDone = false;
   for (i = 0; i < _numInlinedSites; ++i)
      {
      GuardInfo *info = _guardTable[i];
      if (!info || info->getHasBeenDevirtualized())
         continue;

      TR_BitVectorIterator bvi(*(info->getVisibleGuards()));
      while (bvi.hasMoreElements())
         {
         uint32_t c = bvi.getNextElement();
         GuardInfo *descendant = _guardTable[c];
         TR_ASSERT(descendant, "Inner Guard Walk error.");

         if (descendant->getHasBeenDevirtualized())
            continue;

         if (!performTransformation(comp(), "%sDevirtualizing call guarded by block_%d preexisting on guard %d\n",
                                     OPT_DETAILS, descendant->getBlock()->getNumber(),
                                     info->getBlock()->getNumber()))
            continue;

         if (true)  // force block scope
            {
            TR::Node *outerCall = info->getCallNode();
            TR::Node *innerCall = descendant->getCallNode();
            int32_t thisVN = _vnInfo->getValueNumber
               (innerCall->getChild(innerCall->getFirstArgumentIndex()));

            // Find out which argument of the outer call is the same as the this of the inner call
            //
            int16_t ordinal = -1;
            for (int16_t child = outerCall->getNumChildren()-1;
                 child >= outerCall->getFirstArgumentIndex();
                 child--)
               {
               if (thisVN == _vnInfo->getValueNumber(outerCall->getChild(child)))
                  {
                  ordinal = child;
                  }
               }
            TR_ASSERT(ordinal != -1, "Fatal error running Inner Preexistence");

            TR_VirtualGuard *outerVirtualGuard = comp()->findVirtualGuardInfo(info->getGuardNode());
            TR_VirtualGuard *innerVirtualGuard = comp()->findVirtualGuardInfo(descendant->getGuardNode());
            outerVirtualGuard->addInnerAssumption(comp(), ordinal, innerVirtualGuard);
            comp()->removeVirtualGuard(innerVirtualGuard);
            }

         devirtualize(descendant);
         descendant->setHasBeenDevirtualized();
         somethingDone = true;
         }
      }

   if (somethingDone)
      {
      optimizer()->setValueNumberInfo(0);
      optimizer()->setUseDefInfo(NULL);
      }

   }

void
TR_InnerPreexistence::devirtualize(GuardInfo *info)
   {
   TR::Block *guardBlock = info->getBlock();
   TR::Node  *guardNode  = guardBlock->getLastRealTreeTop()->getNode();
   TR_ASSERT(guardNode->getOpCodeValue() == TR::ificmpne ||
          guardNode->getOpCodeValue() == TR::ifacmpne,
          "Wrong kind of if discovered for virtual guard");
   guardNode->getFirstChild()->recursivelyDecReferenceCount();
   guardNode->setAndIncChild(0, guardNode->getSecondChild());

   // Enable simplifier on the block to remove the call node completely
   //
   requestOpt(OMR::treeSimplification, true, guardBlock);
   }

const char *
TR_InnerPreexistence::optDetailString() const throw()
   {
   return "O^O INNER PREEXISTENCE: ";
   }
