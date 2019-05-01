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

#if DEBUG

#include "optimizer/DominatorVerifier.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/StackMemoryRegion.hpp"
#include "compile/Compilation.hpp"
#include "cs2/arrayof.h"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "optimizer/DominatorsChk.hpp"
#include "optimizer/Dominators.hpp"
#include "ras/Debug.hpp"

TR_DominatorVerifier::TR_DominatorVerifier(TR_Dominators &findDominators)
   : _compilation(findDominators.comp())
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _dominators = &findDominators;

   TR::CFG *cfg = comp()->getFlowGraph();
   _visitCount = comp()->incVisitCount();
   _numBlocks = cfg->getNumberOfNodes()+1;

   if (debug("traceVER"))
      {
      dumpOptDetails(comp(), "Printing out the TreeTops from DominatorVerifier\n");

      TR::TreeTop *currentTree = comp()->getStartTree();

      while (!(currentTree == NULL))
         {
         comp()->getDebug()->print(comp()->getOutFile(), currentTree);
         currentTree = currentTree->getNextTreeTop();
         }

      dumpOptDetails(comp(), "Printing out the CFG from DominatorVerifier\n");
      if (cfg != NULL)
         comp()->getDebug()->print(comp()->getOutFile(), cfg);
      }

   TR_DominatorsChk expensiveAlgorithm(comp());
   expensiveAlgorithmCorrect = isExpensiveAlgorithmCorrect(expensiveAlgorithm);

   if (expensiveAlgorithmCorrect)
      {
      if (debug("traceVER"))
         dumpOptDetails(comp(), "Dominators computed by the expensive algorithm are correct\n");
      }
   else
      {
      if (debug("traceVER"))
         dumpOptDetails(comp(), "Dominators computed by the expensive algorithm are NOT correct\n");
      TR_ASSERT(0, "Dominators computed by the expensive algorithm are NOT correct\n");
      }


   bothImplementationsConsistent = areBothImplementationsConsistent(expensiveAlgorithm, findDominators);

   if (bothImplementationsConsistent)
      {
      if (debug("traceVER"))
         dumpOptDetails(comp(), "Dominators computed by the two implementations are consistent\n");
      }
   else
      {
      if (debug("traceVER"))
         dumpOptDetails(comp(), "Dominators computed by the two implementations are NOT consistent\n");
      TR_ASSERT(0, "Dominators computed by the two implementations are NOT consistent\n");
      }
   }



bool TR_DominatorVerifier::areBothImplementationsConsistent(TR_DominatorsChk &findDominatorsChk, TR_Dominators &findDominators)
   {
   int i;

   _dominatorsChkInfo = findDominatorsChk.getDominatorsChkInfo();

   for (i = 2; i < _numBlocks-1; i++)
      {
      TR::Block *conservativeDominator = _dominatorsChkInfo[i]._idom->_block;
      TR::Block *efficientDominator = findDominators.getDominator(_dominatorsChkInfo[i]._block);

      if (!(efficientDominator == conservativeDominator))
         {
         if (debug("traceVER"))
            {
            dumpOptDetails(comp(), "Inconsistency \n");
            dumpOptDetails(comp(), "   Dominator computed by expensive algorithm for [%p] is [%p]\n", _dominatorsChkInfo[i]._block, _dominatorsChkInfo[i]._idom->_block);
            dumpOptDetails(comp(), "   Dominator computed by efficient algorithm for [%p] is [%p]\n", _dominatorsChkInfo[i]._block, efficientDominator);
            }

         return false;
         }
     }

   return true;
   }



bool TR_DominatorVerifier::isExpensiveAlgorithmCorrect(TR_DominatorsChk &expensiveAlgorithm)
   {
   int32_t i,j;
   _nodesSeenOnEveryPath = new (trStackMemory()) TR_BitVector(_numBlocks,trMemory(), stackAlloc);
   _nodesSeenOnCurrentPath = new (trStackMemory()) TR_BitVector(_numBlocks,trMemory(), stackAlloc);
   _dominatorsChkInfo = expensiveAlgorithm.getDominatorsChkInfo();

   for (i = 2; i < _numBlocks-1; i++)
      {
      TR_BitVector *bucket = _dominatorsChkInfo[i]._tmpbucket;

      for (j = 0; j < _numBlocks-1; j++)
         {
         if (bucket->get(j)) // dominator according to algorithm
            {
            // Initializing these BitVectors before checking
            // dominators for the next block.
            // The last bit is not changed for either bit vector - is that what
            // was intended?
            //
            _nodesSeenOnEveryPath->setAll(_numBlocks-1);
            int32_t lastBit = _nodesSeenOnCurrentPath->get(_numBlocks-1);
            _nodesSeenOnCurrentPath->empty();
            if (lastBit)
               _nodesSeenOnCurrentPath->set(_numBlocks-1);

            if ( ! dominates(_dominatorsChkInfo[j+1]._block,_dominatorsChkInfo[i]._block) )  // dominator according to the CFG
               {
               if (debug("traceVER"))
                  {
                  dumpOptDetails(comp(), "   Dominator info for expensive algorithm is incorrect \n");
                  dumpOptDetails(comp(), "   Dominator of [%p] is [%p] as per the algorithm\n", _dominatorsChkInfo[i]._block, _dominatorsChkInfo[j+1]._block);
                  dumpOptDetails(comp(), "   But [%p] is not an the dominator of [%p] as per the Control Flow Graph", _dominatorsChkInfo[j+1]._block, _dominatorsChkInfo[i]._block);
                  }
               return false;
               }
            }
         }
      }

   return true;
   }


// Returns true if the dominator block dominates the dominated block
// as per the Control Flow Graph. Examines each (backward) control
// flow path from the dominated block to the dominator block. Intersection
// of the nodes seen along each of the backward control flow paths (until
// the dominator block) from the dominated block must be a singleton set,
// containing only the dominator block.
//
bool TR_DominatorVerifier::dominates(TR::Block *dominator, TR::Block *dominated)
   {
   for (auto pred = dominated->getPredecessors().begin(); pred != dominated->getPredecessors().end(); ++pred)
       {
       _visitCount = comp()->incVisitCount();
       compareWithPredsOf(dominator, toBlock((*pred)->getFrom()));
       *_nodesSeenOnEveryPath &= *_nodesSeenOnCurrentPath;

       // Reset all bits except the last - is this what is intended?
       int32_t lastBit = _nodesSeenOnCurrentPath->get(_numBlocks-1);
       _nodesSeenOnCurrentPath->empty();
       if (lastBit)
          _nodesSeenOnCurrentPath->set(_numBlocks-1);
       }

   for (auto pred = dominated->getExceptionPredecessors().begin(); pred != dominated->getExceptionPredecessors().end(); ++pred)
       {
       _visitCount = comp()->incVisitCount();
       compareWithPredsOf(dominator, toBlock((*pred)->getFrom()));
       *_nodesSeenOnEveryPath &= *_nodesSeenOnCurrentPath;
       _nodesSeenOnCurrentPath->empty();
       }

   for (int32_t i=0;i<_numBlocks-1;i++)
      {
      if (i==_dominators->_dfNumbers[dominator->getNumber()])
         {
         if (!_nodesSeenOnEveryPath->get(i))
            return false;
         }
         else
         {
         if (_nodesSeenOnEveryPath->get(i))
            return false;
         }
      }

      return true;
   }



// Visits the Predecessors of the dominated block recursively.
//
void TR_DominatorVerifier::compareWithPredsOf(TR::Block *dominator, TR::Block *dominated)
   {
   dominated->setVisitCount(_visitCount);
   _nodesSeenOnCurrentPath->set(_dominators->_dfNumbers[dominator->getNumber()]);

   if (dominator == dominated)
      return;

   for (auto pred = dominated->getPredecessors().begin(); pred != dominated->getPredecessors().end(); ++pred)
       {
       if (toBlock((*pred)->getFrom())->getVisitCount() != _visitCount)
          compareWithPredsOf(dominator, toBlock((*pred)->getFrom()));
       }

   for (auto pred = dominated->getExceptionPredecessors().begin(); pred != dominated->getExceptionPredecessors().end(); ++pred)
       {
       if (toBlock((*pred)->getFrom())->getVisitCount() != _visitCount)
          compareWithPredsOf(dominator, toBlock((*pred)->getFrom()));
       }

   }

#endif
