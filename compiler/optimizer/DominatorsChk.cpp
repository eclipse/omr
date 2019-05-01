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

#include "optimizer/DominatorsChk.hpp"

#include <stdint.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "infra/Array.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"

TR_DominatorsChk::TR_DominatorsChk(TR::Compilation *c)
   : _compilation(c)
   {
   int32_t i,j;
   _topDfNum = 0;
   _visitCount = comp()->incOrResetVisitCount();

   TR::CFG *cfg = comp()->getFlowGraph();
   _numBlocks = cfg->getNumberOfNodes()+1;

   if (debug("traceDOM"))
      dumpOptDetails(comp(), "   Number of blocks is %d\n", _numBlocks-1);

   // Info will persist until the caller releases the stack
   //
   _info = (BBInfoChk*) trMemory()->allocateStackMemory(_numBlocks*sizeof(BBInfoChk));

   memset(_info, 0, _numBlocks*sizeof(BBInfoChk));

   int32_t nextNodeNumber = cfg->getNextNodeNumber();
   _dfNumbers = (int32_t *)trMemory()->allocateStackMemory(nextNodeNumber*sizeof(int32_t));
   memset(_dfNumbers, 0, nextNodeNumber*sizeof(int32_t));

   findDominators(toBlock(cfg->getStart()));
   findImmediateDominators();

   for (i = 2; i < _topDfNum+1; i++)
      {
      TR_BitVector *bucket = _info[i]._bucket;
      TR_BitVector *tmpbucket = _info[i]._tmpbucket;
      TR::Block *dominator;

      for(j = 0; j < _numBlocks-1; j++)
         {
         if (tmpbucket->get(j))
            {
            dominator = _info[j+1]._block;
            _info[i]._idom = &_info[j+1];
            }
         }

      TR::Block *dominated = _info[i]._block;

      if (((dominator == NULL) && (dominated == NULL)))
         {
         }

      // These two lines to be uncommented if this is the only Dominator
      // algorithm in use in the program. They set the Dominator info to
      // be used by other modules. The Dominator info is currently being
      // set by the efficient Dominator algorithm.
      //
      //      dominated->setDominator(dominator);
      //      dominator->addDominatedBlock(dominated);

      for(j = 0; j < _numBlocks-1; j++)
         {
         if (bucket->get(j))
            {
            if (debug("traceDOM"))
               {
               dumpOptDetails(comp(), "   Dominator of [%d] is [%d] \n", _info[i]._block->getNumber(), _info[j+1]._block->getNumber());
               dumpOptDetails(comp(), "   Immediate Dominator of [%d] is [%d] \n", _info[i]._block->getNumber(), dominator->getNumber());
               }
            }
         }
      }
   }



// Part 1 of the simple O(n^2) algorithm in Muchnick. Performs initialization
// and computes dominators for each node.
//
void TR_DominatorsChk::findDominators(TR::Block *start)
   {
   int32_t i,j;
   BBInfoChk *w;
   bool change = true;
   TR_BitVector *intersection = new (trStackMemory()) TR_BitVector(_numBlocks, trMemory(), stackAlloc);

   initialize(start, NULL);

   while (change)
      {
      change = false;
      for (i = 2; i < _topDfNum+1; i++)
         {
         w = _info + i;

         for (j = 0; j < _numBlocks-1; j++ )
            intersection->set(j);

         TR::CFGEdge              *pred;
         BBInfoChk               *predinfo;
         for (auto pred = w->_block->getPredecessors().begin(); pred != w->_block->getPredecessors().end(); ++pred)
            {
            predinfo = _info+(_dfNumbers[toBlock((*pred)->getFrom())->getNumber()]+1);
            (*intersection) &= (*(predinfo->_bucket));
            }
         for (auto pred = w->_block->getExceptionPredecessors().begin(); pred != w->_block->getExceptionPredecessors().end(); ++pred)
            {
            predinfo = _info+(_dfNumbers[toBlock((*pred)->getFrom())->getNumber()]+1);
            (*intersection) &= (*(predinfo->_bucket));
            }

         intersection->set(i - 1);

         if (!( (*(w->_bucket)) == *intersection))
            {
            change = true;

            // Copy intersection into bucket, except for the last bit.
            // Is this what was intended?
            int32_t lastBit = w->_bucket->get(_numBlocks-1);
            *w->_bucket = *intersection;
            if (lastBit)
               w->_bucket->set(_numBlocks-1);
            else
               w->_bucket->reset(_numBlocks-1);
            }
         }
      }
   }



// Initialization step for part 1 of the algorithm. Initializes the
// bucket structure and traverses the flow graph in Depth First Order
// to perform numbering.
//
void TR_DominatorsChk::initialize(TR::Block *start, TR::Block *nullParent)
   {
   TR_Array<StackInfo> *stack = new (trStackMemory()) TR_Array<StackInfo>(trMemory(), _numBlocks/2, false, stackAlloc);
   // Set up to start at the start block
   //
   TR::CFGEdge dummyEdge;
   TR::CFGEdgeList dummyList(comp()->region());
   dummyList.push_front(&dummyEdge);
   (*stack)[0].curIterator = dummyList.begin();
   (*stack)[0].list = &dummyList;
   TR::Block *block;
   int32_t index = 1;
   while ((--index) >= 0)
      {
      auto next = (*stack)[index].curIterator;
      auto list = (*stack)[index].list;
      if ((*next)->getTo())
         block = toBlock((*next)->getTo());
      else
         block = start;

      TR::Block *nextBlock;
      if (block->getVisitCount() == _visitCount)
         {
         // This block has already been processed. Just set up the next block
         // at this level.
         //
         ++next;
         if (next != list->end())
            {
            if (debug("traceDOM"))
               dumpOptDetails(comp(), "Insert block_%d at level %d\n", toBlock((*next)->getTo())->getNumber(), index);
            (*stack)[index++].curIterator = next;
            }
         continue;
         }

      if (debug("traceDOM"))
         dumpOptDetails(comp(), "At level %d block_%d becomes block_%d\n", index, block->getNumber(), _topDfNum);

      // Note: the BBInfoChk index is one more than the block's DF number.
      //
      block->setVisitCount(_visitCount);
      //block->setNumber(_topDfNum++);
      _dfNumbers[block->getNumber()] = _topDfNum++;
      BBInfoChk *binfo = _info + _topDfNum;
      binfo->_block = block;
      binfo->_bucket = new (trStackMemory()) TR_BitVector(_numBlocks, trMemory(), stackAlloc);

      if (block != start)
         binfo->_bucket->setAll(_numBlocks-1);
      else
         binfo->_bucket->set(_topDfNum - 1);

      // Set up the next block at this level
      //
      ++next;
      if (next != list->end())
         {
         if (debug("traceDOM"))
            dumpOptDetails(comp(), "Insert block_%d at level %d\n", toBlock((*next)->getTo())->getNumber(), index);
         (*stack)[index++].curIterator = next;
         }

      // Set up the successors to be processed
      //
      list = &(block->getExceptionSuccessors());
      next = list->begin();
      if (next != list->end())
         {
         if (debug("traceDOM"))
            dumpOptDetails(comp(), "Insert block_%d at level %d\n", toBlock((*next)->getTo())->getNumber(), index);
         (*stack)[index].list = list;
         (*stack)[index++].curIterator = next;
         }
      list = &(block->getSuccessors());
      next = list->begin();
      if (next != list->end())
         {
         if (debug("traceDOM"))
            dumpOptDetails(comp(), "Insert block_%d at level %d\n", toBlock((*next)->getTo())->getNumber(), index);
         (*stack)[index].list = list;
         (*stack)[index++].curIterator = next;
         }
      }
   }


// Part 2 of the simple O(n^2) algorithm in Muchnick. Performs initialization
// and computes immediate dominators for each node. Note that this uses the
// dominators info computed in Part 1.
//
void TR_DominatorsChk::findImmediateDominators()
   {
   int32_t i,j,k;
   BBInfoChk *w, *x;

   initialize();

   for (i=2; i < _topDfNum+1; i++)
      {
      w = _info + i;

      for (j=1; j < _topDfNum+1; j++)
         {
         x = _info + j;

         if ( w->_tmpbucket->get(j-1) )
            {
            for (k=0; k < _numBlocks-1; k++)
               {
               if (!( (j-1) == k ))
                  {
                  if ( x->_tmpbucket->get(k) )
                     w->_tmpbucket->reset(k);
                  }
               }
            }
         }
      }
   }



// Initialization step for part 2 of the algorithm. Initializes the
// tmpbucket structure.
//
void TR_DominatorsChk::initialize()
   {
   int i;
   BBInfoChk *w;

   for (i = 1; i < _topDfNum+1; i++)
      {
      w = _info + i;
      w->_tmpbucket = new (trStackMemory()) TR_BitVector(*w->_bucket);
      w->_tmpbucket->reset(i-1);
      }
   }

#endif
