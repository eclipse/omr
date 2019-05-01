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

#ifndef ISOLSTOR_INCL
#define ISOLSTOR_INCL

#include <stdint.h>
#include "il/Node.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_BitVector;
class TR_RegionStructure;
class TR_Structure;
class TR_UseDefInfo;
namespace TR { class SymbolReference; }
namespace TR { class Block; }
namespace TR { class TreeTop; }
template <class T> class TR_Array;

// Isolated Store Elimination
//
// Extends local dead store elimination; removes stores to any
// locals that are never used in the entire method. Such local
// stores might be created after commoning (local and global) and
// local dead store elimination may not be able to catch all the cases.
//
class TR_IsolatedStoreElimination : public TR::Optimization
   {
   public:
   // Performs isolated store elimination
   //
   TR_IsolatedStoreElimination(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_IsolatedStoreElimination(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   protected:

   char *_optDetailsMessage;
   bool _mustUseUseDefInfo; // temporary

   private :

   bool _deferUseDefInfoInvalidation;  // true if useDefInfo is invalidated during performWithUseDefInfo() but useDefInfo is still used
                                       // (conservatively) later on in the optimization.

   int32_t performWithUseDefInfo();
   int32_t performWithoutUseDefInfo();
   bool    canRemoveStoreNode(TR::Node *node);
   void    collectDefParentInfo(int32_t defIndex,TR::Node *, TR_UseDefInfo *);
   bool    groupIsolatedStores(int32_t defIndex,TR_BitVector *,TR_UseDefInfo *);
   void    examineNode(TR::Node *, vcount_t visitCount, bool);

   void performDeadStructureRemoval(TR_UseDefInfo *);
   bool findStructuresAndNodesUsedIn(TR_UseDefInfo *, TR_Structure *, vcount_t, TR_BitVector *, TR_BitVector *, bool *);
   bool markNodesAndLocateSideEffectIn(TR::Node *, vcount_t, TR_BitVector *, TR_BitVector *);

   void analyzeSingleBlockLoop(TR_RegionStructure *, TR::Block *);

   void removeRedundantSpills();

   enum defStatus
      {
      notVisited =0,
      inTransit,
      notToBeRemoved,
      toBeRemoved,
      doNotExamine
      };

   TR_BitVector        *_usedSymbols;
   TR_Array<TR::Node *> *_storeNodes;
   TR_Array<int32_t>  *_defParentOfUse;
   TR_Array<defStatus>  *_defStatus;
   TR_Array<TR_BitVector *> *_groupsOfStoreNodes;
   TR_BitVector       *_trivialDefs;

   TR::TreeTop *_currentTree;
   };

#endif
