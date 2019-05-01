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

#include <stddef.h>
#include <stdint.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "optimizer/DataFlowAnalysis.hpp"
#include "optimizer/LocalAnalysis.hpp"
#include "optimizer/Structure.hpp"

namespace TR { class Optimizer; }

// #define MAX_BLOCKS_FOR_STACK_ALLOCATION 16

// This file contains an implementation of Latestness which
// is the fourth global bit vector analyses used by PRE. Latestness
// attempts to discover for each expression, all those points in the
// CFG that are as both optimal and as late as possible. The objective
// of Latestness is to return a placement scheme that minimizes register
// pressure while maintaining optimality. This is done with a backward
// pass over the set of possible optimal points returned by Delayedness
// choosing only those points that are latest.
//
//
TR_DataFlowAnalysis::Kind TR_Latestness::getKind()
   {
   return Latestness;
   }

TR_Latestness *TR_Latestness::asLatestness()
   {
   return this;
   }

int32_t TR_Latestness::getNumberOfBits()
   {
   return _delayedness->_numberOfBits;
   }




TR_Latestness::TR_Latestness(TR::Compilation *comp, TR::Optimizer *optimizer, TR_Structure *rootStructure, bool trace)
   : TR_BackwardIntersectionBitVectorAnalysis(comp, comp->getFlowGraph(), optimizer, trace)
   {
   _delayedness = new (comp->allocator()) TR_Delayedness(comp, optimizer, rootStructure, trace);

   _supportedNodesAsArray = _delayedness->_supportedNodesAsArray;

   if (trace)
      traceMsg(comp, "Starting Latestness\n");

   TR::CFG *cfg = comp->getFlowGraph();
   _numberOfNodes = cfg->getNextNodeNumber();
   TR_ASSERT(_numberOfNodes > 0, "Latestness, node numbers not assigned");

   _numberOfBits = getNumberOfBits();

   _inSetInfo = (ContainerType **)trMemory()->allocateStackMemory(_numberOfNodes*sizeof(ContainerType *));
   for (int32_t i=0;i<_numberOfNodes;i++)
      allocateContainer(_inSetInfo+i);

   // Allocate temp bit vectors from block info, since it is local to this analysis
   ContainerType *intersection, *negation;
   allocateBlockInfoContainer(&intersection);
   allocateBlockInfoContainer(&negation);

   TR::CFGNode *nextNode;
   for (nextNode = cfg->getFirstNode(); nextNode; nextNode = nextNode->getNext())
      {
      TR_BlockStructure *blockStructure = (toBlock(nextNode))->getStructureOf();
      if ((blockStructure == NULL) || (blockStructure->getBlock()->getSuccessors().empty() && blockStructure->getBlock()->getExceptionSuccessors().empty()))
         continue;

      /////analyzeTreeTopsInBlockStructure(blockStructure);
      /////analysisInfo->_containsExceptionTreeTop = _containsExceptionTreeTop;
      initializeInfo(intersection);
      for (auto succ = nextNode->getSuccessors().begin(); succ != nextNode->getSuccessors().end(); ++succ)
         {
         TR::CFGNode *succBlock = (*succ)->getTo();
         compose(intersection, _delayedness->_inSetInfo[succBlock->getNumber()]);
         }

      /////if (getAnalysisInfo(blockStructure)->_containsExceptionTreeTop)
         {
         for (auto succ = nextNode->getExceptionSuccessors().begin(); succ != nextNode->getExceptionSuccessors().end(); ++succ)
            {
            TR::CFGNode *succBlock = (*succ)->getTo();
            compose(intersection, _delayedness->_inSetInfo[succBlock->getNumber()]);
            }
         }

      negation->setAll(_numberOfBits);
      *negation -= *intersection;
      copyFromInto(negation, _inSetInfo[blockStructure->getNumber()]);
      *(_inSetInfo[blockStructure->getNumber()]) |= *(_delayedness->_earliestness->_globalAnticipatability->_localAnticipatability.getDownwardExposedAnalysisInfo(blockStructure->getBlock()->getNumber()));
      *(_inSetInfo[blockStructure->getNumber()]) &= *(_delayedness->_inSetInfo[blockStructure->getNumber()]);

      if (trace)
         {
         traceMsg(comp, "\nIn Set of Block : %d\n", blockStructure->getNumber());
         _inSetInfo[blockStructure->getNumber()]->print(comp);
         }
      }

   if (trace)
      traceMsg(comp, "\nEnding Latestness\n");

   // Null out info that will not be used by callers
   _delayedness->_inSetInfo = NULL;
   _blockAnalysisInfo = NULL;
   }



// Overrides the implementation in the superclass as this analysis
// is slightly different from conventional bit vector analyses.
// It uses the results from local analyses instead of examining
// each tree top for effects on the input bit vector at that tree top.
// This analysis has a trivial analyzeNode(...) method as a result.
//
//
void TR_Latestness::analyzeNode(TR::Node *, vcount_t, TR_BlockStructure *, ContainerType *)
   {
   }

void TR_Latestness::analyzeTreeTopsInBlockStructure(TR_BlockStructure *blockStructure)
   {
   TR::Block *block = blockStructure->getBlock();
   TR::TreeTop *currentTree = block->getExit();
   TR::TreeTop *entryTree = block->getEntry();
   /////copyFromInto(_regularInfo, _outSetInfo[blockStructure->getNumber()]);
   bool notSeenTreeWithChecks = true;
   _containsExceptionTreeTop = false;

   while (!(currentTree == entryTree))
      {
      if (notSeenTreeWithChecks)
         {
         bool currentTreeHasChecks = treeHasChecks(currentTree);
         if (currentTreeHasChecks)
            {
            notSeenTreeWithChecks = false;
            _containsExceptionTreeTop = true;
            /////compose(_regularInfo, _exceptionInfo);
            /////compose(_outSetInfo[blockStructure->getNumber()], _exceptionInfo);
            }
         }
      else
         break;

      if (!(currentTree == entryTree))
         currentTree = currentTree->getPrevTreeTop();
      }

   }
