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
#include "infra/CfgEdge.hpp"                       // for CFGEdge
#include "infra/CfgNode.hpp"                       // for CFGNode
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

   _visitCount = comp()->incVisitCount();

   // Attempt to split tails occurring in a linear control flow
   //
   splitLinear(toBlock(_cfg->getStart()), toBlock(_cfg->getEnd()));

   if (trace())
      {
      comp()->dumpMethodTrees("Trees after splitLinear");
      }

   return 0;
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

            if (succBlock1 == cursor->getNextBlock())
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
               if (succBlock2 == cursor->getNextBlock())
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

            if (succBlock1 == next->getNextBlock())
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
               if (succBlock2 == next->getNextBlock())
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
      TR_AddressInfo *valueInfo = static_cast<TR_AddressInfo*>(TR_ValueProfileInfoManager::getProfiledValueInfo(node, comp(), AddressInfo));

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
