/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "optimizer/Dominators.hpp"

#include <stddef.h>
#include <stdint.h>
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "ras/Debug.hpp"

TR_Dominators::TR_Dominators(TR::Compilation *c, bool post) :
   _region(c->trMemory()->heapMemoryRegion()),
   _compilation(c),
   _info(c->getFlowGraph()->getNextNodeNumber()+1, BBInfo(_region), _region),
   _dfNumbers(c->getFlowGraph()->getNextNodeNumber()+1, 0, _region),
   _dominators(c->getFlowGraph()->getNextNodeNumber()+1, static_cast<TR::Block *>(NULL), _region)
   {
   construct(c->getFlowGraph(), post);
   }

TR_Dominators::TR_Dominators(TR::Compilation *c, TR::CFG* cfg, bool acceptUnreachableBlocks, bool post) :
   _region(c->trMemory()->heapMemoryRegion()),
   _compilation(c),
   _info(cfg->getNextNodeNumber()+1, BBInfo(_region), _region),
   _dfNumbers(cfg->getNextNodeNumber()+1, 0, _region),
   _dominators(cfg->getNextNodeNumber()+1, static_cast<TR::Block *>(NULL), _region)
   {
   construct(cfg, post, acceptUnreachableBlocks);
   }

void TR_Dominators::construct(TR::CFG* cfg, bool post, bool acceptUnreachableBlocks)
   {
   LexicalTimer tlex("TR_Dominators::TR_Dominators", _compilation->phaseTimer());

   _postDominators = post;
   _isValid = true;
   _hasUnreachableBlocks = false;
   _topDfNum = 0;
   _visitCount = _compilation->incOrResetVisitCount();
   _trace = comp()->getOption(TR_TraceDominators);

   _cfg = cfg;
   _numNodes = cfg->getNumberOfNodes()+1;

   if (trace())
      {
      traceMsg(comp(), "Starting %sdominator calculation\n", _postDominators ? "post-" : "");
      traceMsg(comp(), "   Number of nodes is %d\n", _numNodes-1);
      }

   if (_postDominators)
      _dfNumbers[cfg->getStart()->getNumber()] = -1;
   else
      _dfNumbers[cfg->getEnd()->getNumber()] = -1;

   findDominators(toBlock( _postDominators ? cfg->getEnd() : cfg->getStart() ));

   int32_t i;
   for (i = _topDfNum; i > 1; i--)
      {
      BBInfo &info = getInfo(i);
      TR::Block *dominated = info._block;
      TR::Block *dominator = getInfo(info._idom)._block;
      _dominators[dominated->getNumber()] = dominator;
      if (trace())
         traceMsg(comp(), "   %sDominator of block_%d is block_%d\n", _postDominators ? "post-" : "",
                                 dominated->getNumber(), dominator->getNumber());
      }

   // The exit block may not be reachable from the entry node. In this case just
   // give the exit block the highest depth-first numbering.
   // No other blocks should be unreachable.
   //

   if (_postDominators)
      {
      if (_dfNumbers[cfg->getStart()->getNumber()] < 0)
         _dfNumbers[cfg->getStart()->getNumber()] = _topDfNum++;
      }
   else
      {
      if (_dfNumbers[cfg->getEnd()->getNumber()] < 0)
         _dfNumbers[cfg->getEnd()->getNumber()] = _topDfNum++;
      }

   // Assert that we've found every node in the cfg.
   //
   if (_topDfNum != _numNodes-1)
      {
      if (_postDominators)
         {
         _isValid = false;
         if (trace())
            traceMsg(comp(), "Some blocks are not reachable from exit. Post-dominator info is invalid.\n");
         return;
         }
      if (!acceptUnreachableBlocks)
         {
         _hasUnreachableBlocks = true;
         if (trace())
            traceMsg(comp(), "Unreachable block in the CFG %d %d\n", _topDfNum, _numNodes-1);
         return;
         }
      else
         TR_ASSERT(false, "Unreachable block in the CFG %d %d", _topDfNum, _numNodes-1);
      }

   #if DEBUG
      for (auto block = toBlock(cfg->getFirstNode()); block; block = toBlock(block->getNext()))
      {
         TR_ASSERT(_dfNumbers[block->getNumber()] >= 0, "Unreachable block in the CFG");
      }
   #endif

   if (trace())
      traceMsg(comp(), "End of %sdominator calculation\n", _postDominators ? "post-" : "");

   // Release no-longer-used data
   _info.clear();
   }

TR::Block * TR_Dominators::getDominator(TR::Block *block)
   {
   if (block->getNumber() >= _dominators.size())
      {
      return NULL;
      }
   return _dominators[block->getNumber()];
   }

int TR_Dominators::dominates(TR::Block *block, TR::Block *other)
   {

   if (other == block)
      return 1;
   for (TR::Block *d = other; d != NULL && _dfNumbers[d->getNumber()] >= _dfNumbers[block->getNumber()]; d = getDominator(d))
      {
      if (d == block)
         return 1;
      }
   return 0;
   }

void TR_Dominators::findDominators(TR::Block *start)
   {
   int32_t i;

   // Initialize the BBInfo structure for the first (dummy) entry
   //
   getInfo(0)._ancestor = 0;
   getInfo(0)._label = 0;

   if (trace())
      {
      traceMsg(comp(), "CFG before initialization:\n");
      comp()->getDebug()->print(comp()->getOutFile(), _cfg);
      }

   // Initialize the BBInfo structures for the real blocks
   //
   initialize(start, NULL);

   if (trace())
      {
	  traceMsg(comp(), "CFG after initialization:\n");
      comp()->getDebug()->print(comp()->getOutFile(), _cfg);
      traceMsg(comp(), "\nInfo after initialization:\n");
      for (i = 0; i <= _topDfNum; i++)
         getInfo(i).print(comp()->fe(), comp()->getOutFile());
      }

   for (i = _topDfNum; i > 1; i--)
      {
      BBInfo &w = getInfo(i);

      int32_t u;

      // Compute initial values for semidominators and store blocks with the
      // same semidominator in the semidominator's bucket.
      //
      TR::CFGEdge *pred, *succ;

      if (_postDominators)
         {
         TR_SuccessorIterator bi(w._block);
         for (succ = bi.getFirst(); succ != NULL; succ = bi.getNext())
            {
            u = eval(_dfNumbers[toBlock(succ->getTo())->getNumber()]+1);
            u = getInfo(u)._sdno;
            if (u < w._sdno)
               w._sdno = u;
            }
         }
      else
         {
         TR_PredecessorIterator bi(w._block);
         for (pred = bi.getFirst(); pred != NULL; pred = bi.getNext())
            {
            u = eval(_dfNumbers[toBlock(pred->getFrom())->getNumber()]+1);
            u = getInfo(u)._sdno;
            if (u < w._sdno)
               w._sdno = u;
            }
         }

      BBInfo &v = getInfo(w._sdno);
      v._bucket.set(i);
      link(w._parent, i);

      // Compute immediate dominators for nodes in the bucket of w's parent
      //
      BitVector &parentBucket = getInfo(w._parent)._bucket;
      BitVector::Cursor cursor(parentBucket);
      for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
         {
         BBInfo &sdom = getInfo(cursor);
         u = eval(cursor);
         if (getInfo(u)._sdno < sdom._sdno)
        	 sdom._idom = u;
         else
        	 sdom._idom = w._parent;
         }
      parentBucket.empty();
      }

   // Adjust immediate dominators of nodes whose current version of the
   // immediate dominator differs from the node's semidominator.
   //
   for (i = 2; i <= _topDfNum; i++)
      {
      BBInfo &w = getInfo(i);
      if (w._idom != w._sdno)
         w._idom = getInfo(w._idom)._idom;
      }
   }

void TR_Dominators::initialize(TR::Block *start, BBInfo *nullParent) {

   // Set up to start at the start block
   //
   TR::deque<StackInfo> stack(comp()->allocator());

   /*
    * Seed an initial list of successors using a dummy loop edge connecting the start to itself.
    */
   TR::CFGEdge dummyEdge;
   TR::CFGEdgeList startList(comp()->region());
   startList.push_front(&dummyEdge);

   StackInfo seed(startList, startList.begin(), -1);
   stack.push_back(seed);

   while (!stack.empty()) {
      StackInfo current(stack.back());
      stack.pop_back();
      TR_ASSERT(current.listPosition != current.list.end(), "Successor list has already been exhausted.");
      TR::CFGNode* succ = _postDominators ? (*current.listPosition)->getFrom() : (*current.listPosition)->getTo();
      TR::Block *block = succ ? toBlock(succ) : start;

      if (block->getVisitCount() == _visitCount)
         {
         // This block has already been processed. Just set up the next block
         // at this level.
         //
         StackInfo::iterator_type next(current.listPosition);
         ++next;
         if (next != current.list.end())
            {
            if (trace())
               {
               traceMsg(
                  comp(),
                  "Insert block_%d at level %d\n",
                  toBlock( _postDominators ? (*next)->getFrom() : (*next)->getTo() )->getNumber(),
                  stack.size()
                  );
               }

            stack.push_back(StackInfo(current.list, next, current.parent));
            }
         continue;
         }

      if (trace())
         traceMsg(comp(), "At level %d block_%d becomes block_%d\n", stack.size(), block->getNumber(), _topDfNum);

      block->setVisitCount(_visitCount);
      _dfNumbers[block->getNumber()] = _topDfNum++;

      // Note: the BBInfo index is one more than the block's DF number.
      //
      BBInfo &binfo = getInfo(_topDfNum);
      binfo._block = block;
      binfo._sdno = _topDfNum;
      binfo._label = _topDfNum;
      binfo._ancestor = 0;
      binfo._child = 0;
      binfo._size = 1;
      binfo._parent = current.parent;

      // Set up the next block at this level
      //
      StackInfo::iterator_type next(current.listPosition);
      ++next;
      if (next != current.list.end())
         {
         if (trace())
            {
            traceMsg(
               comp(),
               "Insert block_%d at level %d\n",
               toBlock(_postDominators ? (*next)->getFrom() : (*next)->getTo())->getNumber(),
               stack.size()
               );
            }

         stack.push_back(StackInfo(current.list, next, current.parent));
         }

      // Set up the successors to be processed
      //
      StackInfo::list_type &exceptionSuccessors = _postDominators ? block->getExceptionPredecessors() : block->getExceptionSuccessors();
      StackInfo::iterator_type firstExceptionSuccessor = exceptionSuccessors.begin();
      if (firstExceptionSuccessor != exceptionSuccessors.end())
         {
         if (trace())
            {
            traceMsg(
               comp(),
               "Insert block_%d at level %d\n",
               toBlock( _postDominators ? (*firstExceptionSuccessor)->getFrom() : (*firstExceptionSuccessor)->getTo())->getNumber(),
               stack.size()
               );
            }

         stack.push_back(StackInfo(exceptionSuccessors, firstExceptionSuccessor, _topDfNum));
         }

      StackInfo::list_type &successors = _postDominators ? block->getPredecessors() : block->getSuccessors();
      StackInfo::iterator_type firstSuccessor = successors.begin();
      if (firstSuccessor != successors.end())
         {
         if (trace())
            {
            traceMsg(
               comp(),
               "Insert block_%d at level %d\n",
               toBlock( _postDominators ? (*firstSuccessor)->getFrom() : (*firstSuccessor)->getTo() )->getNumber(),
               stack.size()
               );
            }

         stack.push_back(StackInfo(successors, firstSuccessor, _topDfNum));
         }
      }
   }

// Determine the ancestor of the block at the given index whose semidominator has the minimal depth-first
// number.
//
int32_t TR_Dominators::eval(int32_t index)
   {
   	BBInfo &bbInfo = getInfo(index);
   if (bbInfo._ancestor == 0)
      return bbInfo._label;
   compress(index);
   BBInfo &ancestor = getInfo(bbInfo._ancestor);
   if (getInfo(ancestor._label)._sdno >= getInfo(bbInfo._label)._sdno)
      return bbInfo._label;
   return ancestor._label;
   }

// Compress ancestor path of the block at the given index to the block whose label has the maximal
// semidominator number.
//
void TR_Dominators::compress(int32_t index)
   {
   	BBInfo & bbInfo = getInfo(index);
   	BBInfo &ancestor = getInfo(bbInfo._ancestor);
   if (ancestor._ancestor != 0)
      {
      compress(bbInfo._ancestor);
      if (getInfo(ancestor._label)._sdno < getInfo(bbInfo._label)._sdno)
         bbInfo._label = ancestor._label;
      bbInfo._ancestor = ancestor._ancestor;
      }
   }

// Rebalance the forest of trees maintained by the ancestor and child links
//
void TR_Dominators::link(int32_t parentIndex, int32_t childIndex)
   {
   BBInfo &binfo = getInfo(childIndex);
   int32_t sdno = getInfo(binfo._label)._sdno;
   int32_t si = childIndex;
   BBInfo *s = &binfo;
   while (sdno < getInfo(getInfo(s->_child)._label)._sdno)
      {
      BBInfo &child = getInfo(s->_child);
      if (s->_size + getInfo(child._child)._size >= 2*child._size)
         {
         child._ancestor = si;
         s->_child = child._child;
         }
      else
         {
         child._size = s->_size;
         s->_ancestor = s->_child;
         si = s->_child;
         s = &getInfo(si);
         }
      }
   s->_label = binfo._label;
   BBInfo & parent = getInfo(parentIndex);
   parent._size += binfo._size;
   if (parent._size < 2*binfo._size)
      {
      int32_t tmp = si;
      si = parent._child;
      parent._child = tmp;
      }
   while (si != 0)
      {
      s = &getInfo(si);
      s->_ancestor = parentIndex;
      si = s->_child;
      }
   }

#ifdef DEBUG
void TR_Dominators::BBInfo::print(TR_FrontEnd *fe, TR::FILE *pOutFile)
   {
   if (pOutFile == NULL)
      return;
   trfprintf(pOutFile,"BBInfo %d:\n",getIndex());
   trfprintf(pOutFile,"   _parent=%d, _idom=%d, _sdno=%d, _ancestor=%d, _label=%d, _size=%d, _child=%d\n",
             _parent,
             _idom,
             _sdno,
             _ancestor,
             _label,
             _size,
             _child);
   }
#endif


void TR_PostDominators::findControlDependents()
   {
   int32_t nextNodeNumber = _cfg->getNextNodeNumber();
   int32_t i;

   _directControlDependents = (TR_BitVector**)_region.allocate(nextNodeNumber*sizeof(TR_BitVector *));
   // Initialize the table of direct control dependents
   for (i = 0; i < nextNodeNumber; i++)
      _directControlDependents[i] = new (_region) TR_BitVector(nextNodeNumber, _region);//.AddEntry(comp()->allocator("PostDominators"));

   TR::Block * block;
   for (block = comp()->getStartBlock(); block!=NULL; block = block->getNextBlock())
      {
      BitVector &bv = *_directControlDependents[block->getNumber()];

      auto next = block->getSuccessors().begin();
      while (next != block->getSuccessors().end())
         {
         TR::Block *p;
         p = toBlock((*next)->getTo());
         while (p != getDominator(block))
	        {
            bv.set(p->getNumber());
            p = getDominator(p);
            }
         ++next;
         }
      }

   if (trace())
      {
      for (i = 0; i < nextNodeNumber; i++)
         {
         BitVector::Cursor cursor(*_directControlDependents[i]);
         cursor.SetToFirstOne();
         traceMsg(comp(), "Block %d controls blocks: {", i);
         if (cursor.Valid())
         	  {
         	  int32_t b = cursor;
         	  traceMsg(comp(),"%d", b);
         	  while(cursor.SetToNextOne())
         	     {
         	     b = cursor;
         	     traceMsg(comp(),", %d", b);
         	     }
         	  }
         traceMsg(comp(), "} \t\t%d blocks in total\n", numberOfBlocksControlled(i));
         }
      }
   }


int32_t TR_PostDominators::numberOfBlocksControlled(int32_t block)
   {
   BitVector seen(_region);
   return countBlocksControlled(block, seen);
   }

int32_t TR_PostDominators::countBlocksControlled(int32_t block, BitVector &seen)
   {
   int32_t number = 0;
   BitVector::Cursor cursor(*_directControlDependents[block]);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t i = cursor;
      if (!seen.get(i))
         {
         number++;
         seen.set(i);
         number += countBlocksControlled(i, seen);
         }
      }
   return number;
   }
