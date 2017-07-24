/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#ifndef TR_CFGNODE_INCL
#define TR_CFGNODE_INCL

#include <limits.h>             // for SHRT_MAX
#include <stddef.h>             // for NULL
#include <stdint.h>             // for int32_t, int16_t
#include "env/TRMemory.hpp"     // for TR_Memory, etc
#include "il/Node.hpp"          // for vcount_t
#include "infra/Link.hpp"       // for TR_Link1
#include "infra/List.hpp"       // for List
#include "infra/TRCfgEdge.hpp"  // for CFGEdge
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
   CFGNode(TR::Region &region);
   CFGNode(int32_t n, TR::Region &region);

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
