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

#include <vector>
#include "compile/Compilation.hpp"
#include "cs2/sparsrbit.h"
#include "il/Node.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"

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
