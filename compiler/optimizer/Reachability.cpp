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

#include "optimizer/Reachability.hpp"

#include <algorithm>
#include <limits.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"

TR_ReachabilityAnalysis::TR_ReachabilityAnalysis(TR::Compilation *comp):_comp(comp){}

void
TR_ReachabilityAnalysis::perform(TR_BitVector *result)
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   int32_t numBlockIndexes = cfg->getNextNodeNumber();
   int32_t numBlocks       = cfg->getNumberOfNodes();

   _blocks = cfg->createArrayOfBlocks();

   blocknum_t *stack    = (blocknum_t*)comp()->trMemory()->allocateStackMemory(numBlockIndexes * sizeof(stack[0]));
   blocknum_t *depthMap = (blocknum_t*)comp()->trMemory()->allocateStackMemory(numBlockIndexes * sizeof(depthMap[0]));
   memset(depthMap, 0, numBlockIndexes * sizeof(depthMap[0]));

   bool trace = comp()->getOption(TR_TraceReachability);

   if (trace)
      traceMsg(comp(), "BEGIN REACHABILITY: %d blocks\n", numBlocks);

   for (TR::Block *block = comp()->getStartBlock(); block; block = block->getNextBlock())
      {
      blocknum_t blockNum = block->getNumber();
      if (trace)
         traceMsg(comp(), "Visit block_%d\n", blockNum);
      if (depthMap[blockNum] == 0)
         traverse(blockNum, 0, stack, depthMap, result);
      else
         traceMsg(comp(), "  depth is already %d; skip\n", depthMap[blockNum]);
      }

   if (comp()->getOption(TR_TraceReachability))
      {
      traceMsg(comp(), "END REACHABILITY.  Result:\n");
      result->print(comp(), comp()->getOutFile());
      traceMsg(comp(), "\n");
      }
   }

void
TR_ReachabilityAnalysis::propagateOneInput(blocknum_t inputBlockNum, blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depthMap, TR_BitVector *result)
   {
   if (inputBlockNum == blockNum)
      return;
   if (depthMap[inputBlockNum] == 0)
      traverse(inputBlockNum, depth, stack, depthMap, result);
   depthMap[blockNum] = std::min(depthMap[blockNum], depthMap[inputBlockNum]);
   if (result->isSet(inputBlockNum))
      {
      if (comp()->getOption(TR_TraceReachability))
         traceMsg(comp(), "    Propagate block_%d to block_%d\n", blockNum, inputBlockNum);
      result->set(blockNum);
      }
   else
      {
      if (comp()->getOption(TR_TraceReachability))
         traceMsg(comp(), "    No change to block_%d from block_%d\n", blockNum, inputBlockNum);
      }
   }

void
TR_ReachabilityAnalysis::traverse(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depthMap, TR_BitVector *result)
   {
   //
   // This is DeRemer and Penello's "digraph" algorithm
   //
   bool trace = comp()->getOption(TR_TraceReachability);

   // Initialize state for this block
   //
   stack[depth++] = blockNum;
   depthMap[blockNum] = depth;
   bool value = isOrigin(getBlock(blockNum));
   if (trace)
      traceMsg(comp(), "  traverse %sblock_%d depth %d\n", value? "origin ":"", blockNum, depth);
   result->setTo(blockNum, value);

   // Recursively propagate from inputs
   //
   propagateInputs(blockNum, depth, stack, depthMap, result);

   // Propagate result around the strongly connected component that includes blockNum
   //
   if (depthMap[blockNum] == depth)
      {
      while(1)
         {
         blocknum_t top = stack[--depth];
         depthMap[top] = INT_MAX;
         if (top == blockNum)
            {
            break;
            }
         else
            {
            if (trace)
               traceMsg(comp(), "    Loop: propagate block_%d to block_%d\n", blockNum, top);
            result->setTo(top, result->isSet(blockNum));
            }
         }
      }
   }

void
TR_ForwardReachability::propagateInputs(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure)
   {
   TR::Block *block = getBlock(blockNum);
   TR_PredecessorIterator bi(block);
   for (TR::CFGEdge *edge = bi.getFirst(); edge != NULL; edge = bi.getNext())
      {
      TR::Block *inputBlock = toBlock(edge->getFrom());
      propagateOneInput(inputBlock->getNumber(), blockNum, depth, stack, depth_map, closure);
      }
   }

void
TR_ForwardReachabilityWithoutExceptionEdges::propagateInputs(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure)
   {
   TR::Block *block = getBlock(blockNum);
   for (auto edge = block->getPredecessors().begin(); edge != block->getPredecessors().end(); ++edge)
      {
      TR::Block *inputBlock = toBlock((*edge)->getFrom());
      propagateOneInput(inputBlock->getNumber(), blockNum, depth, stack, depth_map, closure);
      }
   }

void
TR_BackwardReachability::propagateInputs(blocknum_t blockNum, int32_t depth, blocknum_t *stack, blocknum_t *depth_map, TR_BitVector *closure)
   {
   TR::Block *block = getBlock(blockNum);
   TR_SuccessorIterator bi(block);
   for (TR::CFGEdge *edge = bi.getFirst(); edge != NULL; edge = bi.getNext())
      {
      TR::Block *inputBlock = toBlock(edge->getTo());
      propagateOneInput(inputBlock->getNumber(), blockNum, depth, stack, depth_map, closure);
      }
   }

bool
TR_CanReachGivenBlocks::isOrigin(TR::Block *block)
   {
   return _originBlocks->isSet(block->getNumber());
   }

bool
TR_CanReachNonColdBlocks::isOrigin(TR::Block *block)
   {
   if (block->isCold())
      return false;
   else if (block->isSuperCold())
      return false;
   else if (block == comp()->getFlowGraph()->getEnd())
      return false;
   else
      return true;
   }

bool
TR_CanBeReachedFromCatchBlock::isOrigin(TR::Block *block)
   {
   return block->isCatchBlock();
   }

bool
TR_CanBeReachedWithoutExceptionEdges::isOrigin(TR::Block *block)
   {
   return block == comp()->getFlowGraph()->getStart();
   }
