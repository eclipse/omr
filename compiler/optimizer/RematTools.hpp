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
 *******************************************************************************/
#include <vector>                                         // for std::vector
#include "compile/Compilation.hpp"                        // for Compilation
#include "cs2/sparsrbit.h"
#include "il/Node.hpp"                                    // for Node, etc
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                                 // for TreeTop

#ifndef REMAT_SAFETY_TOOLS
#define REMAT_SAFETY_TOOLS

class RematSafetyInformation
   {
   std::vector<TR::SparseBitVector, TR::typed_allocator<TR::SparseBitVector, TR::Allocator> > dependentSymRefs;
   std::vector<TR::TreeTop *, TR::typed_allocator<TR::TreeTop * , TR::Allocator> > argumentTreeTops;
   std::vector<TR::TreeTop *, TR::typed_allocator<TR::TreeTop * , TR::Allocator> > rematTreeTops;
   TR::Compilation *comp;
   public:
   RematSafetyInformation(TR::Compilation *comp) :
      dependentSymRefs(getTypedAllocator<TR::SparseBitVector>(comp->allocator())),
      argumentTreeTops(getTypedAllocator<TR::TreeTop*>(comp->allocator())),
      rematTreeTops(getTypedAllocator<TR::TreeTop*>(comp->allocator())),
      comp(comp)
      {
      }

   void add(TR::TreeTop *argStore, TR::SparseBitVector &symRefDependencies);
   void add(TR::TreeTop *argStore, TR::TreeTop *rematStore);
   uint32_t size() { return dependentSymRefs.size(); }
   TR::SparseBitVector &symRefDependencies(uint32_t idx) { return dependentSymRefs[idx]; }
   TR::TreeTop *argStore(uint32_t idx) { return argumentTreeTops[idx]; }
   TR::TreeTop *rematTreeTop(uint32_t idx) { return rematTreeTops[idx]; }
   void dumpInfo(TR::Compilation *comp);
   };

class RematTools
   {
   static TR_YesNoMaybe gatherNodesToCheck(TR::Compilation *comp,
      TR::Node *privArg, TR::Node *currentNode, TR::SparseBitVector &scanTargets,
      TR::SparseBitVector &symRefsToCheck, bool trace, TR::SparseBitVector &visitedNodes);
   static void walkNodesCalculatingRematSafety(TR::Compilation *comp,
      TR::Node *currentNode, TR::SparseBitVector &scanTargets, 
      TR::SparseBitVector &enabledSymRefs, TR::SparseBitVector &unsafeSymRefs,
      bool trace, TR::SparseBitVector &visitedNodes);
   static bool getNextTreeTop(TR::TreeTop* &start, TR_BitVector *blocksToFollow,
      TR::Block *startBlock);
   public:
   static TR_YesNoMaybe gatherNodesToCheck(TR::Compilation *comp,
      TR::Node *privArg, TR::Node *currentNode, TR::SparseBitVector &scanTargets,
      TR::SparseBitVector &symRefsToCheck, bool trace);

   static bool walkTreesCalculatingRematSafety(TR::Compilation *comp,
      TR::TreeTop *start, TR::TreeTop *end, TR::SparseBitVector &scanTargets, 
      TR::SparseBitVector &unsafeSymRefs, TR_BitVector *blocksToVisit, bool trace);
   static void walkTreesCalculatingRematSafety(TR::Compilation *comp,
      TR::TreeTop *start, TR::TreeTop *end, TR::SparseBitVector &scanTargets, 
      TR::SparseBitVector &unsafeSymRefs, bool trace);

   static bool walkTreeTopsCalculatingRematFailureAlternatives(TR::Compilation *comp,
      TR::TreeTop *start, TR::TreeTop *end, TR::list<TR::TreeTop*> &failedArgs,
      TR::SparseBitVector &scanTargets, RematSafetyInformation &rematInfo,
      TR_BitVector *blocksToVisit, bool trace);
   static void walkTreeTopsCalculatingRematFailureAlternatives(TR::Compilation *comp,
      TR::TreeTop *start, TR::TreeTop *end, TR::list<TR::TreeTop*> &failedArgs,
      TR::SparseBitVector &scanTargets, RematSafetyInformation &rematInfo, bool trace);
   };
#endif
