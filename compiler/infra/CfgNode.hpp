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

#ifndef TR_CFGNODE_INCL
#define TR_CFGNODE_INCL

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/forward_list.hpp"
class TR_StructureSubGraphNode;
namespace TR { class Block; }
namespace TR { class Compilation; }

namespace TR
{
typedef TR::typed_allocator< CFGEdge*, TR::Region& > CFGEdgeListAllocator;
typedef TR::forward_list< CFGEdge*, CFGEdgeListAllocator > CFGEdgeList;

class CFGNode : public ::TR_Link1<CFGNode>
   {
   // NOTE: CFGNodes form a linked list with each other.  AllBlockIterator
   // assumes nodes are always added to the front of the list.  If this becomes
   // false, please double check that AllBlockIterator will still satisfy its
   // guarantee that it will not be considered finished until there are no
   // un-visited blocks in the CFG.

   public:
   TR_ALLOC(TR_Memory::CFGNode)

   CFGNode(TR_Memory * m);
   CFGNode(int32_t n, TR_Memory * m);

   protected:
   CFGNode(TR::Region &region);
   CFGNode(int32_t n, TR::Region &region);

   public:
   TR::CFGEdgeList& getSuccessors()            {return _successors;}
   TR::CFGEdgeList& getPredecessors()          {return _predecessors;}
   TR::CFGEdgeList& getExceptionSuccessors()   {return _exceptionSuccessors;}
   TR::CFGEdgeList& getExceptionPredecessors() {return _exceptionPredecessors;}

   //getEdge looks in both getSuccessors() and getExceptionSuccessors()
   //use getSuccessorEdge or getExceptionEdge if a search in a particular list is required
   CFGEdge *getEdge(CFGNode *succ);

   void addSuccessor(CFGEdge *p)                                         {_successors.push_front(p);}
   void addSuccessor(CFGEdge *p, TR_AllocationKind allocKind)            {_successors.push_front(p);}

   void addPredecessor(CFGEdge *p)                                       {_predecessors.push_front(p);}
   void addPredecessor(CFGEdge *p, TR_AllocationKind allocKind)          {_predecessors.push_front(p);}

   void addExceptionSuccessor(CFGEdge *p)                                {_exceptionSuccessors.push_front(p);}
   void addExceptionSuccessor(CFGEdge *p, TR_AllocationKind allocKind)   {_exceptionSuccessors.push_front(p);}

   void addExceptionPredecessor(CFGEdge *p)                              {_exceptionPredecessors.push_front(p);}
   void addExceptionPredecessor(CFGEdge *p, TR_AllocationKind allocKind) {_exceptionPredecessors.push_front(p);}

   void removeSuccessor(CFGEdge *p)  { _successors.remove(p);}
   void removePredecessor(CFGEdge *p){ _predecessors.remove(p);}
   void removeExceptionSuccessor(CFGEdge *p)  { _exceptionSuccessors.remove(p);}
   void removeExceptionPredecessor(CFGEdge *p){ _exceptionPredecessors.remove(p);}

   bool hasSuccessor(CFGNode * n);
   bool hasExceptionSuccessor(CFGNode * n);

   bool hasPredecessor(CFGNode * n);
   bool hasExceptionPredecessor(CFGNode * n);

   CFGEdge * getSuccessorEdge(CFGNode * n);
   CFGEdge * getExceptionSuccessorEdge(CFGNode * n);

   CFGEdge * getPredecessorEdge(CFGNode * n);
   CFGEdge * getExceptionPredecessorEdge(CFGNode * n);

   void moveSuccessors(CFGNode * to);
   void movePredecessors(CFGNode * to);

   bool isUnreachable() { return _predecessors.empty() && _exceptionPredecessors.empty(); }

   int32_t getNumber()          {return _nodeNumber;}
   void    setNumber(int32_t n) {_nodeNumber = n;}

   void    removeNode()         {return setValid(false);}
   bool    nodeIsRemoved()      {return (!isValid());}

   vcount_t    getVisitCount()            {return _visitCount;}
   vcount_t    setVisitCount(vcount_t vc) {return (_visitCount = vc);}
   vcount_t    incVisitCount()            {return ++_visitCount;}

   int32_t   getForwardTraversalIndex()              { return _forwardTraversalIndex; }
   void      setForwardTraversalIndex(int32_t idx)   { _forwardTraversalIndex=idx; }
   int32_t   getBackwardTraversalIndex()             { return _backwardTraversalIndex; }
   void      setBackwardTraversalIndex(int32_t idx)  { _backwardTraversalIndex=idx; }


   int16_t    getFrequency()          { return _frequency; }
   void       setFrequency(int32_t f)
      {
      if (f >= SHRT_MAX)
        f = SHRT_MAX-1;

      _frequency = f;
      }

   static int32_t normalizedFrequency  (int32_t rawFrequency,        int32_t maxFrequency);
   static int32_t denormalizedFrequency(int32_t normalizedFrequency, int32_t maxFrequency);

   void       normalizeFrequency(int32_t maxFrequency);
   void       normalizeFrequency(int32_t frequency, int32_t maxFrequency);
   int32_t    denormalizeFrequency(int32_t maxFrequency);

   // Perform node-specific processing when the node is being removed from the
   // CFG.
   //
   virtual void removeFromCFG(TR::Compilation *);

   // Downcasts
   //
   virtual TR::Block                 *asBlock()                 {return NULL;}
   virtual TR_StructureSubGraphNode *asStructureSubGraphNode() {return NULL;}

   private:
   template <typename FUNC>
   CFGEdge * getEdgeMatchingNodeInAList (CFGNode * n, TR::CFGEdgeList& list, FUNC blockGetter);
   static CFGNode * fromBlockGetter (CFGEdge * e) {return e->getFrom();};
   static CFGNode * toBlockGetter (CFGEdge * e)   {return e->getTo();};

   protected:
   TR::Region      &_region;

   private:
   TR::CFGEdgeList _successors;
   TR::CFGEdgeList _predecessors;
   TR::CFGEdgeList _exceptionSuccessors;
   TR::CFGEdgeList _exceptionPredecessors;

   int32_t          _nodeNumber;
   vcount_t         _visitCount;
   int16_t          _frequency;
   int32_t         _forwardTraversalIndex;
   int32_t         _backwardTraversalIndex;
   };

}

#endif
