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

#ifndef OMR_INFRA_ILWALK
#define OMR_INFRA_ILWALK

#include "infra/Checklist.hpp"
#include "infra/SideTable.hpp"
#include "infra/Stack.hpp"

namespace TR {

// "Imported" classes
//
class Block;
class CFG;
class Compilation;
class MethodSymbol;
class Node;
class Optimization;
class TreeTop;

// Classes declared here
//
class AllBlockIterator;
class PostorderNodeIterator;
class PostorderNodeOccurrenceIterator;
class PreorderNodeIterator;
class PreorderNodeOccurrenceIterator;
class ReversePostorderSnapshotBlockIterator;
class TreeTopIterator;


class TreeTopIteratorImpl
   {
   // This has essentially everything from TreeTopIterator except the actual
   // decision about when to call logCurrentLocation.  Different subclasses
   // have different conventions about the meaning of the "current location",
   // so they'll want to do this in different ways.

   protected:

   TR::TreeTop *_current;

   // For logging
   //
   TR::Compilation  *_comp;
   const char       *_name; // NULL means we don't want logging

   TR::Compilation * comp() { return _comp; }

   void logCurrentLocation();

   void stepForward();
   void stepBackward();

   TreeTopIteratorImpl(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL);

   public:

   TR::TreeTop *currentTree() { return _current; }
   TR::Node    *currentNode();
   bool         isAt(TR::TreeTop *tt) { return _current == tt; }
   void         jumpTo(TR::TreeTop *tt){ _current = tt; }

   bool isAt(TreeTopIteratorImpl &other) { return isAt(other.currentTree()); }
   bool isAt(PreorderNodeIterator &other);

   public: // operators

   bool operator ==(TR::TreeTop *tt) { return isAt(tt); }
   bool operator !=(TR::TreeTop *tt) { return !isAt(tt); }
   void operator ++(){ stepForward(); }
   void operator --(){ stepBackward(); }
   };

class TreeTopIterator: public TreeTopIteratorImpl
   {
   typedef TreeTopIteratorImpl Super;

   public:

   TreeTopIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL):
      TreeTopIteratorImpl(start, comp, name){ logCurrentLocation(); }

   void stepForward()  { Super::stepForward();  logCurrentLocation(); }
   void stepBackward() { Super::stepBackward(); logCurrentLocation(); }
   };

class NodeIterator: protected TreeTopIteratorImpl // abstract
   {
   typedef TreeTopIteratorImpl Super;

   protected:

   struct WalkState
      {
      TR::Node *_node;
      int32_t   _child;
      bool      _isBetweenChildren; // Makes logging pretty

      WalkState(TR::Node *node):_node(node),_child(0),_isBetweenChildren(false){}

      bool equals(struct WalkState &other){ return _node == other._node && _child == other._child; }
      bool operator==(struct WalkState &other){ return equals(other); }
      bool operator!=(struct WalkState &other){ return !equals(other); }
      };

   TR_Stack<WalkState> _stack;
   NodeChecklist       _checklist;

   NodeIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL);
   NodeIterator(TR::TreeTop *start, TR::Compilation *comp);

   int32_t stackDepth()
      {
      // TODO: TR_Stack curently has an unsigned depth, which leads to all kinds
      // of havoc and undefined behaviour near zero.  It should have a signed
      // depth.  Until then, use this function instead of calling _stack.size()
      // directly.
      return (int32_t)_stack.size();
      }

   void logCurrentLocation();

   public:

   TR::TreeTop *currentTree()        { return Super::currentTree(); }
   TR::Node *currentNode()           { return _stack.top()._node; }
   bool isAt(TR::TreeTop *tt)        { return Super::isAt(tt); }
   bool isAt(TreeTopIterator &other) { return isAt(other.currentTree()); }
   bool isAt(PreorderNodeIterator &other);

   public: // operators

   bool operator ==(TR::TreeTop *tt) { return isAt(tt); }
   bool operator !=(TR::TreeTop *tt) { return !isAt(tt); }
   };

// Returns each node once, in the order in which they appear in the log
//
class PreorderNodeIterator: public NodeIterator
   {
   typedef class NodeIterator Super;

   bool alreadyBeenPushed(TR::Node *node);
   void push(TR::Node *node);

   public:

   PreorderNodeIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL);

   void         stepForward();

   public: // operators

   void operator ++() { stepForward(); }
   };

// Returns each node once, in postorder.
//
// This is the order in which nodes will be evaluated, except more strict.
// (Actual evaluation does not specify the order that siblings are visited.)
//
class PostorderNodeIterator: public NodeIterator
   {
   typedef class NodeIterator Super;

   bool alreadyBeenPushed(TR::Node *node);
   void push(TR::Node *node);
   void descend();

   public:

   PostorderNodeIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL);

   void         stepForward();

   public: // operators

   void operator ++() { stepForward(); }
   };

class NodeOccurrenceIterator: public NodeIterator
   {
   protected:

   NodeOccurrenceIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL):
      NodeIterator(start, comp, name){}

   void logCurrentLocation();

   public:

   TR::Node *currentNode();
   };

// Returns each node each time it appears, visiting nodes before their
// children, and visiting siblings in order.
//
// This is the order in which the nodes are listed in the logs.
//
class PreorderNodeOccurrenceIterator: public NodeOccurrenceIterator
   {
   typedef class NodeOccurrenceIterator Super;

   bool alreadyPushedChildren(TR::Node *node);
   void push(TR::Node *node);

   public:

   PreorderNodeOccurrenceIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL);

   void stepForward();

   public: // operators

   void operator ++() { stepForward(); }

   };

// Returns each node each time it appears, visiting children before parents,
// and visiting siblings in order.
//
// This is the order in which nodes will be evaluated, except more strict.
// (Actual evaluation does not specify the order that siblings are visited.)
//
class PostorderNodeOccurrenceIterator: public NodeOccurrenceIterator
   {
   typedef class NodeOccurrenceIterator Super;

   bool alreadyPushedChildren(TR::Node *node);
   void pushLeftmost(TR::Node *node);

   public:

   PostorderNodeOccurrenceIterator(TR::TreeTop *start, TR::Compilation *comp, const char *name=NULL);

   void stepForward();

   public: // operators

   void operator ++() { stepForward(); }

   };

class BlockIterator
   {
   protected:

   TR::Compilation  *_comp;
   const char       *_name;

   bool isLoggingEnabled();

   BlockIterator(TR::Compilation *comp, const char *name);
   };

// Scans blocks in reverse postorder from a given starting point, building up a
// list of blocks to visit.  Then returns each block in that order which is
// still part of the CFG at the time it is encountered.  Any block returned by
// this iterator is guaranteed to be in the CFG at the time it is returned.
//
class ReversePostorderSnapshotBlockIterator: protected BlockIterator
   {
   TR_Array<TR::Block*> _postorder;    // Note that this is opposite to the order we want, so we walk this array in reverse
   int32_t              _currentIndex; // can be 1 past the valid index range of _postorder: -1 or _postorder.lastIndex()+1

   void takeSnapshot(TR::Block *start);
   void visit(TR::Block *block, TR::BlockChecklist &alreadyVisited);

   void logCurrentLocation();

   bool isStepOperationFinished();

   public:

   ReversePostorderSnapshotBlockIterator(TR::Block *start, TR::Compilation *comp, const char *name=NULL);
   ReversePostorderSnapshotBlockIterator(TR::CFG *cfg, TR::Compilation * comp, const char *name=NULL);

   TR::Block   *currentBlock();
   bool         isAt(TR::Block *block){ return currentBlock() == block; }

   void stepForward();
   void stepBackward();

   public: // operators

   void operator ++() { stepForward();  }
   void operator --() { stepBackward(); }
   };

// Scans blocks in arbitrary order.  All blocks present in the CFG at the time
// the walk finishes will have been returned exactly once, and possibly some
// (but not necessarily all) blocks deleted during the walk.  Any block
// returned by this iterator is guaranteed to be in the CFG at the time it is
// returned.
//
// This iterator relies on the fact that CFGNodes are in a linked list into
// which new nodes are only ever prepended at the front.  Because of this, we
// can be guarantee that we will visit all nodes if we (1) start again at the
// front of the list whenever we reach the end, and (2) stop the first time we
// hit a block we've already visited.
//
class AllBlockIterator: protected BlockIterator
   {
   TR::CFG      *_cfg;
   TR::Block     *_currentBlock;
   TR::Block     *_nextBlock;
   BlockChecklist _alreadyVisited;

   void logCurrentLocation();

   public:

   AllBlockIterator(TR::CFG *cfg, TR::Compilation *comp, const char *name=NULL);

   TR::Block   *currentBlock();
   bool         isAt(TR::Block *block){ return currentBlock() == block; }

   void stepForward();

   public: // operators

   void operator ++() { stepForward(); }
   };

}

#endif
