/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#ifndef ILVALIDATOR_INCL
#define ILVALIDATOR_INCL

#include "infra/ILWalk.hpp"

namespace TR {

class ILValidator
   {
   TR::Compilation  *_comp;
   bool              _isValidSoFar;

   public:

   ILValidator(TR::Compilation *comp);

   bool isLoggingEnabled();

   // "Soundness" comprises the criteria required to make a IL iterators
   // function properly.  Compilation will abort if the trees are unsound
   // because we can't realistically continue when our most basic assumptions
   // are violated.
   // Note that the "stop" tree is not checked.
   //
   void checkSoundness(TR::TreeTop *start, TR::TreeTop *stop=NULL);

   // "Valid" comprises any kind of validity rules anyone might want to impose on the trees.
   //
   bool treesAreValid(TR::TreeTop *start, TR::TreeTop *stop=NULL);

   protected: // Soundness

   //
   // Soundness checks don't even try to pretend they can continue after a
   // failure.  If the trees are unsound, we don't want to try to continue
   // processing them.  This means that a soundness check can rely on all
   // previous checks succeeding.
   //

   void soundnessRule(TR::TreeTop *location, bool condition, const char *formatStr, ...); // returns isSoundSoFar for convenience
   void checkNodeSoundness(TR::TreeTop *location, TR::Node *node, NodeChecklist &ancestorNodes, NodeChecklist &visitedNodes);

   protected: // Validation

   typedef PostorderNodeOccurrenceIterator Location;

   struct NodeState
      {
      TR::Node    *_node;
      ncount_t     _futureReferenceCount;

      NodeState(TR::Node *node):_node(node),_futureReferenceCount(node->getReferenceCount()){}
      };

   class LiveNodeWindow
      {
      // This is like a NodeChecklist, but more compact.  Rather than track
      // node global indexes, which can be sparse, this tracks local
      // indexes, which are relatively dense.  Furthermore, the _basis field
      // allows us not to waste space on nodes we saw in prior blocks.

      NodeSideTable<NodeState> &_sideTable;
      int32_t                   _basis;
      TR_BitVector              _liveOffsets; // sideTable index = basis + offset

      public:

      LiveNodeWindow(NodeSideTable<NodeState> &sideTable, TR_Memory *memory);

      bool contains(TR::Node *node)
         {
         int32_t index = _sideTable.indexOf(node);
         if (index < _basis)
            return false;
         else
            return _liveOffsets.isSet(index - _basis);
         }

      void add(TR::Node *node)
         {
         int32_t index = _sideTable.indexOf(node);
         TR_ASSERT(index >= _basis, "Cannot mark node n%dn before basis %d live", node->getGlobalIndex(), _basis);
         _liveOffsets.set(index - _basis);
         }

      void remove(TR::Node *node)
         {
         int32_t index = _sideTable.indexOf(node);
         if (index >= _basis)
            _liveOffsets.reset(index - _basis);
         }

      bool isEmpty()
         {
         return _liveOffsets.isEmpty();
         }

      TR::Node *anyLiveNode()
         {
         Iterator iter(*this);
         return iter.currentNode();
         }

      void startNewWindow()
         {
         TR_ASSERT(_liveOffsets.isEmpty(), "Cannot close LiveNodeWindow when there are still live nodes");
         _basis = _sideTable.size();
         }

      public:

      class Iterator
         {
         LiveNodeWindow      &_window;
         TR_BitVectorIterator _bvi;
         int32_t              _currentOffset; // -1 means iteration is past end

         bool isPastEnd(){ return _currentOffset < 0; }

         public:

         Iterator(LiveNodeWindow &window):_window(window),_bvi(window._liveOffsets),_currentOffset(0xdead /* anything non-negative */)
            {
            stepForward();
            }

         TR::Node *currentNode()
            {
            if (isPastEnd())
               return NULL;
            else
               return _window._sideTable.getAt(_window._basis + _currentOffset)._node;
            }

         void stepForward()
            {
            TR_ASSERT(!isPastEnd(), "Can't stepForward a LiveNodeWindow::Iterator that's already past end");
            if (_bvi.hasMoreElements())
               _currentOffset = _bvi.getNextElement();
            else
               _currentOffset = -1;
            }

         public: // operators

         void operator ++() { stepForward(); }
         };
      };

   NodeSideTable<NodeState>  _nodeStates;
   LiveNodeWindow            _liveNodes;

   bool isValidSoFar(){ return _isValidSoFar; }
   void validityRule(Location &location, bool condition, const char *formatStr, ...);

   protected: // Individual validation routines

   // These return false to indicate a validity error was found.
   // Note that these are given verb names ("validateXxx") instead of adjective
   // names ("xxxIsValid") because generally they are stateful.

   // Called on every node, before all other validation routines
   //
   virtual void validateNode(Location &location);

   //
   // Additional validation hooks
   //

   virtual void validateEndOfExtendedBlock(Location &location);

   protected:

   TR::Compilation *comp();

   void updateNodeState(Location &newLocation);

   void printDiagnostic(const char *formatStr, ...);
   void vprintDiagnostic(const char *formatStr, va_list ap);
   };

} // namespace TR

#endif // ILVALIDATOR_INCL
