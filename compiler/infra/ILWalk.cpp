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

#include "infra/ILWalk.hpp"

#include "compile/Compilation.hpp"
#include "il/Block.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "infra/Cfg.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"

#include <stdarg.h>

//
// Hack markers
//

// If it weren't for the fact that an already-visited block could be removed
// and re-inserted into the block list, our algorithms would be more elegant.
//
#define REMOVED_BLOCKS_CAN_BE_REINSERTED (1)


//
// TreeTopIterator and TreeTopIteratorImpl
//

TR::TreeTopIteratorImpl::TreeTopIteratorImpl(TR::TreeTop *start, TR::Compilation * comp, const char *name)
   :_current(start),_comp(comp),_name(name)
   {
   }

TR::Node *TR::TreeTopIteratorImpl::currentNode()
   {
   return currentTree()->getNode();
   }

bool TR::TreeTopIteratorImpl::isAt(PreorderNodeIterator &other)
   {
   return other.isAt(currentTree());
   }

void TR::TreeTopIteratorImpl::logCurrentLocation()
   {
   if (_name && _comp && _comp->getOption(TR_TraceILWalks))
      {
      if (currentTree())
         {
         TR::Node *node = currentTree()->getNode();
         traceMsg(_comp, "TREE  %s @ %s n%dn [%p]\n", _name, node->getOpCode().getName(), node->getGlobalIndex(), node);
         }
      else
         {
         traceMsg(_comp, "TREE  %s finished\n", _name);
         }
      }
   }

void TR::TreeTopIteratorImpl::stepForward()
   {
   TR_ASSERT(_current, "Cannot stepForward a TreeTopIterator that has already moved beyond the first or last tree of the method");
   _current = _current->getNextTreeTop();
   }

void TR::TreeTopIteratorImpl::stepBackward()
   {
   TR_ASSERT(_current, "Cannot stepBackward a TreeTopIterator that has already moved beyond the first or last tree of the method");
   _current = _current->getPrevTreeTop();
   }

//
// Node iterators
//

TR::NodeIterator::NodeIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name)
   :TreeTopIteratorImpl(start, comp, name)
   ,_checklist(comp)
   ,_stack(comp->trMemory(), 5, false, stackAlloc)
   {}

bool TR::NodeIterator::isAt(PreorderNodeIterator &other)
   {
   // One PreorderNodeIterator "is at" another if both follow the same node
   // child path from the same treetop.  Note that the checklist is explicitly
   // NOT compared.  This means that a.isAt(b) is no guarantee that a and b
   // will subsequently return the same sequence of nodes.  This is
   // intentional--isAt is not an equality test--and it could occur if the
   // iterations started at differnet points, or if there were differences in
   // timing between advancement of the iterator versus mutations in the IL.
   // This function explicitly disregards such differences.
   //
   if (currentTree() != other.currentTree())
      return false;
   if (stackDepth() != other.stackDepth())
      return false;
   for (int32_t i = 0; i < stackDepth(); i++)
      if (_stack[i] != other._stack[i])
         return false;
   return true;
   }

void TR::NodeIterator::logCurrentLocation()
   {
   if (_name && comp() && comp()->getOption(TR_TraceILWalks))
      {
      if (currentTree())
         {
         TR::Node *node = currentNode();
         traceMsg(comp(), "NODE  %s  ", _name);
         if (stackDepth() >= 2)
            {
            traceMsg(comp(), " ");
            for (int32_t i = 0; i < stackDepth()-2; i++)
               {
               if (_stack[i]._isBetweenChildren)
                  traceMsg(comp(), " |");
               else
                  traceMsg(comp(), "  ");
               }
            traceMsg(comp(), " %d: ", _stack[_stack.topIndex()-1]._child);
            }
         traceMsg(comp(), "%s n%dn [%p]\n", node->getOpCode().getName(), node->getGlobalIndex(), node);
         }
      else
         {
         // Usualy this one doesn't print, because when the iterator finishes
         // naturally, logCurrentLocation is not even called.
         //
         traceMsg(comp(), "NODE  %s finished\n", _name );
         }
      }
   }

TR::PreorderNodeIterator::PreorderNodeIterator(TR::TreeTop *start, TR::Compilation * comp, const char *name)
   :NodeIterator(start, comp, name)
   {
   push(start->getNode());
   }

bool TR::PreorderNodeIterator::alreadyBeenPushed(TR::Node *node)
   {
   return _checklist.contains(node);
   }

void TR::PreorderNodeIterator::push(TR::Node *node)
   {
   TR_ASSERT(!alreadyBeenPushed(node), "Cannot push node n%dn that was already pushed", node->getGlobalIndex());
   _stack.push(WalkState(node));
   _stack.top()._isBetweenChildren = (node->getNumChildren() >= 2);
   _checklist.add(node);
   logCurrentLocation();
   }

void TR::PreorderNodeIterator::stepForward()
   {
   // Move down into the next un-visited child
   //
   for (int32_t i = _stack.top()._child; i < _stack.top()._node->getNumChildren(); i++)
      {
      TR::Node *child = _stack.top()._node->getChild(i);
      if (!alreadyBeenPushed(child))
         {
         _stack.top()._child = i;
         if (i == _stack.top()._node->getNumChildren()-1)
            _stack.top()._isBetweenChildren = false;
         push(child);
         return;
         }
      }

   // No more un-visited children; we're done with this node
   //
   _stack.pop();
   if (_stack.isEmpty())
      {
      // Step to the next tree, if any
      //
      while (_stack.isEmpty())
         {
         TreeTopIteratorImpl::stepForward();
         if (!currentTree())
            return;
         else if (!alreadyBeenPushed(currentTree()->getNode()))
            push(currentTree()->getNode());
         }
      }
   else
      {
      _stack.top()._child++;
      stepForward();
      }
   }

TR::PostorderNodeIterator::PostorderNodeIterator(TR::TreeTop *start, TR::Compilation * comp, const char *name)
   :NodeIterator(start, comp, name)
   {
   push(start->getNode());
   descend();
   }

bool TR::PostorderNodeIterator::alreadyBeenPushed(TR::Node *node)
   {
   return _checklist.contains(node);
   }

void TR::PostorderNodeIterator::push(TR::Node *node)
   {
   TR_ASSERT(!alreadyBeenPushed(node), "Cannot push node n%dn that was already pushed", node->getGlobalIndex());
   _stack.push(WalkState(node));
   _checklist.add(node);
   }

void TR::PostorderNodeIterator::descend()
   {
   // Push frames until we find the innermost, leftmost unvisited descendant.
   TR::Node *node = _stack.top()._node;
   for (;;)
      {
      int32_t i = _stack.top()._child;
      while (i < node->getNumChildren() && alreadyBeenPushed(node->getChild(i)))
         i++;

      _stack.top()._child = i;
      if (i == node->getNumChildren())
         break;

      node = node->getChild(i);
      push(node);
      }

   logCurrentLocation();
   }

void TR::PostorderNodeIterator::stepForward()
   {
   // We're done with the current node and all its descendants. Move back up
   // to the parent and attempt to descend into later subtrees.
   _stack.pop();
   if (!_stack.isEmpty())
      {
      _stack.top()._child++;
      _stack.top()._isBetweenChildren = true;
      descend();
      return;
      }

   // Step to the next tree, if any
   do
      TreeTopIteratorImpl::stepForward();
   while (currentTree() && alreadyBeenPushed(currentTree()->getNode()));

   if (!currentTree())
     return;

   push(currentTree()->getNode());
   descend();
   }

//
// Node occurrence iterators
//

TR::Node *TR::NodeOccurrenceIterator::currentNode()
   {
   if (stackDepth() == 0)
      return currentTree()->getNode();
   else
      return _stack.top()._node->getChild(_stack.top()._child);
   }

void TR::NodeOccurrenceIterator::logCurrentLocation()
   {
   if (_name && comp() && comp()->getOption(TR_TraceILWalks))
      {
      if (currentTree())
         {
         TR::Node *node = currentNode();
         traceMsg(comp(), "WALK  %s  ", _name);
         if (stackDepth() >= 1)
            {
            traceMsg(comp(), " ");
            for (int32_t i = 0; i < stackDepth()-1; i++)
               {
               if (_stack[i]._isBetweenChildren)
                  traceMsg(comp(), " |");
               else
                  traceMsg(comp(), "  ");
               }
            traceMsg(comp(), " %d: ", _stack[_stack.topIndex()]._child);
            }
         traceMsg(comp(), "%s n%dn [%p]\n", node->getOpCode().getName(), node->getGlobalIndex(), node);
         }
      else
         {
         // Usually this one doesn't print, because when the iterator finishes
         // naturally, logCurrentLocation is not even called.
         //
         traceMsg(comp(), "WALK  %s finished\n", _name );
         }
      }
   }

TR::PreorderNodeOccurrenceIterator::PreorderNodeOccurrenceIterator(TR::TreeTop *start, TR::Compilation * comp, const char *name)
   :NodeOccurrenceIterator(start, comp, name)
   {
   logCurrentLocation();
   }

bool TR::PreorderNodeOccurrenceIterator::alreadyPushedChildren(TR::Node *node)
   {
   return _checklist.contains(node);
   }

void TR::PreorderNodeOccurrenceIterator::push(TR::Node *node)
   {
   _stack.push(WalkState(node));
   _stack.top()._isBetweenChildren = (node->getNumChildren() >= 2);
   _checklist.add(node);
   logCurrentLocation();
   }

void TR::PreorderNodeOccurrenceIterator::stepForward()
   {
   if (currentNode()->getNumChildren() >= 1 && !alreadyPushedChildren(currentNode()))
      {
      // Pushing currentNode has the effect of moving down into its children
      //
      push(currentNode());
      }
   else if (_stack.isEmpty())
      {
      // Nullary treetop node; move on to the next treetop
      //
      TreeTopIteratorImpl::stepForward();
      logCurrentLocation();
      }
   else if (++_stack.top()._child < _stack.top()._node->getNumChildren())
      {
      // Our attempt to move to the next sibling was successful
      //
      if (_stack.top()._child == _stack.top()._node->getNumChildren()-1)
         _stack.top()._isBetweenChildren = false;
      logCurrentLocation();
      }
   else
      {
      // No more siblings; try moving to parent's siblings
      //
      _stack.pop();
      stepForward();
      }
   }

TR::PostorderNodeOccurrenceIterator::PostorderNodeOccurrenceIterator(TR::TreeTop *start, TR::Compilation * comp, const char *name)
   :NodeOccurrenceIterator(start, comp, name)
   {
   pushLeftmost(currentTree()->getNode());
   }

bool TR::PostorderNodeOccurrenceIterator::alreadyPushedChildren(TR::Node *node)
   {
   return _checklist.contains(node);
   }

void TR::PostorderNodeOccurrenceIterator::pushLeftmost(TR::Node *nodeArg)
   {
   for (TR::Node *node = nodeArg; node->getNumChildren() >= 1 && !alreadyPushedChildren(node); node = node->getFirstChild())
      {
      _stack.push(WalkState(node));
      _checklist.add(node);
      }
   logCurrentLocation();
   }

void TR::PostorderNodeOccurrenceIterator::stepForward()
   {
   if (_stack.isEmpty())
      {
      // Nullary treetop node; move on to the next treetop
      //
      TreeTopIteratorImpl::stepForward();
      if (currentTree())
         pushLeftmost(currentTree()->getNode());
      }
   else if (++_stack.top()._child < _stack.top()._node->getNumChildren())
      {
      // Our attempt to move to the next sibling was successful
      //
      _stack.top()._isBetweenChildren = true;
      pushLeftmost(currentNode());
      }
   else
      {
      // No more siblings; move to parent
      //
      _stack.pop();
      logCurrentLocation();
      }
   }

//
// Block iterators
//

TR::BlockIterator::BlockIterator(TR::Compilation * comp, const char *name)
   :_comp(comp),_name(name)
   {
   }

bool TR::BlockIterator::isLoggingEnabled()
   {
   return (_name && _comp && _comp->getOption(TR_TraceILWalks));
   }

TR::ReversePostorderSnapshotBlockIterator::ReversePostorderSnapshotBlockIterator(TR::Block *start, TR::Compilation * comp, const char *name)
   :BlockIterator(comp, name),
   _postorder(comp->trMemory(), 5, false, stackAlloc)
   {
   takeSnapshot(start);
   logCurrentLocation();
   }

TR::ReversePostorderSnapshotBlockIterator::ReversePostorderSnapshotBlockIterator(TR::CFG *cfg, TR::Compilation *comp, const char *name)
   :BlockIterator(comp, name),
   _postorder(comp->trMemory(), cfg->getNumberOfNodes(), false, stackAlloc)
   {
   takeSnapshot(cfg->getStart()->asBlock());
   if (isLoggingEnabled())
      {
      traceMsg(comp, "BLOCK  %s Snapshot:", _name);
      for (int32_t i = _postorder.lastIndex(); i >= 0; --i)
         traceMsg(comp, " %d", _postorder[i]->getNumber());
      traceMsg(comp, "\n");
      }
   logCurrentLocation();
   }

void TR::ReversePostorderSnapshotBlockIterator::takeSnapshot(TR::Block *start)
   {
   // TODO: This should use an iterative algorithm with a worklist, but the recursive algo is so much easier to write...
   TR::BlockChecklist alreadyVisited(comp());
   visit(start, alreadyVisited);
   _currentIndex = _postorder.lastIndex();
   }

void TR::ReversePostorderSnapshotBlockIterator::visit(TR::Block *block, TR::BlockChecklist &alreadyVisited)
   {
   if (alreadyVisited.contains(block))
      return;
   alreadyVisited.add(block);

   TR_SuccessorIterator bi(block);
   for (TR::CFGEdge *edge = bi.getFirst(); edge != NULL; edge = bi.getNext())
      visit(edge->getTo()->asBlock(), alreadyVisited);
   if (block->getEntry()) // Don't add the special blocks with no trees to the snapshot
      _postorder.add(block);
   }

TR::Block *TR::ReversePostorderSnapshotBlockIterator::currentBlock()
   {
   if (0 <= _currentIndex && _currentIndex <= _postorder.lastIndex())
      return _postorder[_currentIndex];
   else
      return NULL;
   }

void TR::ReversePostorderSnapshotBlockIterator::stepForward()
   {
   TR_ASSERT(currentBlock(), "cannot stepForward a ReversePostorderSnapshotBlockIterator that has already finished its walk");
   do
      --_currentIndex;
   while (!isStepOperationFinished());
   logCurrentLocation();
   }

void TR::ReversePostorderSnapshotBlockIterator::stepBackward()
   {
   TR_ASSERT(currentBlock(), "cannot stepForward a ReversePostorderSnapshotBlockIterator that has already finished its walk");
   do
      ++_currentIndex;
   while (!isStepOperationFinished());
   logCurrentLocation();
   }

bool TR::ReversePostorderSnapshotBlockIterator::isStepOperationFinished()
   {
   if (!currentBlock())
      return true; // Reached the end
   else if (currentBlock()->isValid())
      return true; // Reached the next block in the walk

   if (isLoggingEnabled())
      traceMsg(comp(), "BLOCK  %s Skip block_%d removed during walk\n", _name, currentBlock()->getNumber());
   return false;
   }

void TR::ReversePostorderSnapshotBlockIterator::logCurrentLocation()
   {
   if (isLoggingEnabled())
      {
      if (currentBlock())
         traceMsg(comp(), "BLOCK  %s @ block_%d\n", _name, currentBlock()->getNumber());
      else
         traceMsg(comp(), "BLOCK  %s finished\n", _name);
      }
   }

TR::AllBlockIterator::AllBlockIterator(TR::CFG *cfg, TR::Compilation * comp, const char *name)
   :BlockIterator(comp, name),_cfg(cfg),_currentBlock(cfg->getFirstNode()->asBlock()),_nextBlock(_currentBlock->getNext()->asBlock()),_alreadyVisited(cfg->comp())
   {
   _alreadyVisited.add(_currentBlock);
   logCurrentLocation();
   }

TR::Block *TR::AllBlockIterator::currentBlock()
   {
   return _currentBlock;
   }

void TR::AllBlockIterator::stepForward()
   {
   TR_ASSERT(_currentBlock, "Cannot stepForward an AllBlockIterator that has already finished iterating");

   // INVARIANT: All visited blocks in the linked list of CFG nodes are at
   // the end of the list.  Therefore, when we hit an already-visited block,
   // we can quit scanning the list.

   // Note: it's crucially important that we use _currentBlock->getNext()->asBlock()
   // and definitely NOT _currentBlock->getNextBlock().  The latter returns
   // blocks in treetop order, which will not provide the "all blocks"
   // guarantee if new blocks are inserted physically before the current one.

   // Move forward, stopping when we hit the end of the list, or a block we've already seen
   //
   TR::CFGNode *next = _nextBlock;
   if (next == NULL || _alreadyVisited.contains(next->asBlock()))
      {
      // Start another lap of the list to catch newly added blocks we need to visit
      //
      next = _cfg->getFirstNode();
      TR_ASSERT(next != NULL, "AllBlockIterator not expecting a CFG with no blocks at all");
      if (_alreadyVisited.contains(next->asBlock()))
         {
         // Since newly added blocks always go at the front, and we never
         // insert a previously removed block, we're done as soon as we try
         // to take another lap and find that the first block in the list is
         // already visited.
         //
         next = NULL;
         }
      }

   if (REMOVED_BLOCKS_CAN_BE_REINSERTED && !next)
      {
      // Can't be certain about our "INVARIANT" after all.  Do another
      // pass looking for any blocks we haven't visited.
      //
      // This isn't the most efficient possible algorithm -- it is O(n^2) in the
      // worst case -- but it cleanly separates the cruft from the main algorithm.
      //
      for (next = _cfg->getFirstNode(); next; next = next->getNext())
         {
         if (!_alreadyVisited.contains(next->asBlock()))
            {
            // Found one!
            //
            if (isLoggingEnabled())
               traceMsg(comp(), "BLOCK  %s REMOVED_BLOCKS_CAN_BE_REINSERTED: block_%d found via extra scan\n", _name, next->asBlock()->getNumber());
            break;
            }
         }
      }

   if (next)
      {
      _currentBlock = next->asBlock();
      _nextBlock    = _currentBlock->getNext()? _currentBlock->getNext()->asBlock() : NULL;
      _alreadyVisited.add(_currentBlock);
      if (_currentBlock->getEntry())
         {
         logCurrentLocation();
         }
      else
         {
         // Skip the special blocks that have no trees.
         //
         stepForward();
         }
      }
   else
      {
      _currentBlock = NULL;
      }

   }

void TR::AllBlockIterator::logCurrentLocation()
   {
   if (isLoggingEnabled())
      {
      if (currentBlock())
         traceMsg(comp(), "BLOCK  %s @ block_%d\n", _name, currentBlock()->getNumber());
      else
         traceMsg(comp(), "BLOCK  %s finished\n", _name);
      }
   }
