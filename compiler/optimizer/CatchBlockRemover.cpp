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

#include "optimizer/CatchBlockRemover.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/StackMemoryRegion.hpp"
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"


TR_CatchBlockRemover::TR_CatchBlockRemover(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t TR_CatchBlockRemover::perform()
   {
   TR::CFG *cfg = comp()->getFlowGraph();
   if (cfg == NULL)
      {
      if (trace())
         traceMsg(comp(), "Can't do Catch Block Removal, no CFG\n");
      return 0;
      }

   if (trace())
      traceMsg(comp(), "Starting Catch Block Removal\n");

   bool thereMayBeRemovableCatchBlocks = false;

   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR::Block *block;
   ListIterator<TR::CFGEdge> edgeIterator;

   // Go through all blocks that have exception successors and see if any of them
   // are not reached. Mark each of these edges with a visit count so they can
   // be identified later.
   //
   vcount_t visitCount = comp()->incOrResetVisitCount();

   TR::CFGNode *cfgNode;
   for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
      {
      if (cfgNode->getExceptionSuccessors().empty())
         continue;

      block = toBlock(cfgNode);
      uint32_t reachedExceptions = 0;
      TR::TreeTop *treeTop;
      for (treeTop = block->getEntry(); treeTop != block->getExit(); treeTop = treeTop->getNextTreeTop())
         {
         reachedExceptions |= treeTop->getNode()->exceptionsRaised();

         if (treeTop->getNode()->getOpCodeValue() == TR::monexitfence) // for live monitor metadata
            reachedExceptions |= TR::Block::CanCatchMonitorExit;
         }

      if (reachedExceptions & TR::Block::CanCatchUserThrows)
         continue;

      for (auto edge = block->getExceptionSuccessors().begin(); edge != block->getExceptionSuccessors().end();)
         {
         TR::CFGEdge * current = *(edge++);
         TR::Block *catchBlock = toBlock(current->getTo());
         if (catchBlock->isOSRCodeBlock() || catchBlock->isOSRCatchBlock()) continue;
         if (!reachedExceptions &&
             performTransformation(comp(), "%sRemove redundant exception edge from block_%d at [%p] to catch block_%d at [%p]\n", optDetailString(), block->getNumber(), block, catchBlock->getNumber(), catchBlock))
            {
            cfg->removeEdge(block, catchBlock);
            thereMayBeRemovableCatchBlocks = true;
            }
         else
            {
            if (!catchBlock->canCatchExceptions(reachedExceptions))
               {
               current->setVisitCount(visitCount);
               thereMayBeRemovableCatchBlocks = true;
               }
            }
         }
      }

   bool edgesRemoved = false;

   // Now look to see if there are any catch blocks for which all exception
   // predecessors have the visit count set. If so, the block is unreachable and
   // can be removed.
   // If only some of the exception predecessors are marked, these edges are
   // left in place to identify the try/catch structure properly.
   //
   while (thereMayBeRemovableCatchBlocks)
      {
      thereMayBeRemovableCatchBlocks = false;
      for (cfgNode = cfg->getFirstNode(); cfgNode; cfgNode = cfgNode->getNext())
         {
         if (cfgNode->getExceptionPredecessors().empty())
            continue;
         auto edgeIt = cfgNode->getExceptionPredecessors().begin();
         for (; edgeIt != cfgNode->getExceptionPredecessors().end(); ++edgeIt)
            {
            if ((*edgeIt)->getVisitCount() != visitCount)
               break;
            }

         if (edgeIt == cfgNode->getExceptionPredecessors().end() && performTransformation(comp(), "%sRemove redundant catch block_%d at [%p]\n", optDetailString(), cfgNode->getNumber(), cfgNode))
            {
            while (!cfgNode->getExceptionPredecessors().empty())
               {
               cfg->removeEdge(cfgNode->getExceptionPredecessors().front());
               }
            edgesRemoved = true;
            thereMayBeRemovableCatchBlocks = true;
            }
         }
      }


   // Any transformations invalidate use/def and value number information
   //
   if (edgesRemoved)
      {
      optimizer()->setUseDefInfo(NULL);
      optimizer()->setValueNumberInfo(NULL);
      requestOpt(OMR::treeSimplification, true);
      }

   } // scope of the stack memory region

   if (trace())
      traceMsg(comp(), "\nEnding Catch Block Removal\n");

   return 1; // actual cost
   }

const char *
TR_CatchBlockRemover::optDetailString() const throw()
   {
   return "O^O CATCH BLOCK REMOVAL: ";
   }
