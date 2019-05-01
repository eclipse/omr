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

#ifndef TR_LOCALDEADSTOREELIMINATION_INCL
#define TR_LOCALDEADSTOREELIMINATION_INCL

#include <stdint.h>
#include "env/TRMemory.hpp"
#include "il/Node.hpp"
#include "infra/deque.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

namespace TR { class Block; }
namespace TR { class NodeChecklist; }
namespace TR { class TreeTop; }
template <class T> class List;

namespace TR
{

/**
 * Class LocalDeadStoreElimination
 * ===============================
 *
 * LocalDeadStoreElimination implements an algorithm that proceeds as follows:
 *
 * Scan through an extended basic block starting from the last treetop
 * and moving upwards through the block. As a store is seen, mark the symbol
 * being stored as unused and if another store is encountered above to
 * the same symbol, then it can be removed if the symbol being stored into
 * is still marked as unused. Symbols are marked as used when a load is
 * seen or they are aliased to a symbol (a call for example) that might
 * use it implicitly (through aliases).
 */

class LocalDeadStoreElimination : public TR::Optimization
   {
   public:
   // Performs local dead store elimination within
   // a basic block.
   //
   LocalDeadStoreElimination(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager);

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual void postPerformOnBlocks();
   virtual const char * optDetailString() const throw();

   protected:

   virtual bool isNonRemovableStore(TR::Node *storeNode, bool &seenIdentityStore);

   private:
   typedef TR::Node *StoreNode;

   typedef TR::deque<StoreNode, TR::Region&> StoreNodeTable;
   typedef TR_BitVector LDSBitVector;

   inline TR::LocalDeadStoreElimination *self();

   bool isIdentityStore(TR::Node *);
   void examineNode(TR::Node *, int32_t, TR::Node *, TR_BitVector &);
   void transformBlock(TR::TreeTop *, TR::TreeTop *);
   bool isEntireNodeRemovable(TR::Node *);
   void setExternalReferenceCountToTree(TR::Node *node, rcount_t *externalReferenceCount);
    bool seenIdenticalStore(TR::Node *);
   bool areLhsOfStoresSyntacticallyEquivalent(TR::Node *, TR::Node *);
   void adjustStoresInfo(TR::Node *, TR_BitVector &);
   TR::Node *getAnchorNode(TR::Node *parentNode, int32_t nodeIndex, TR::Node *, TR::TreeTop *, TR::NodeChecklist& visited);

   void setupReferenceCounts(TR::Node *);
   void eliminateDeadObjectInitializations();
   void findLocallyAllocatedObjectUses(LDSBitVector &, TR::Node *, int32_t, TR::Node *, vcount_t);
   bool examineNewUsesForKill(TR::Node *, TR::Node *, List<TR::Node> *, List<TR::Node> *, TR::Node *, int32_t, vcount_t);
   void killStoreNodes(TR::Node *);

   TR::TreeTop *removeStoreTree(TR::TreeTop *treeTop);

   bool isFirstReferenceToNode(TR::Node *parent, int32_t index, TR::Node *node);
   void setIsFirstReferenceToNode(TR::Node *parent, int32_t index, TR::Node *node);

   protected:

   TR::TreeTop                      *_curTree;

   private:
   StoreNodeTable                   *_storeNodes;

   bool                              _blockContainsReturn;
   bool                              _treesChanged;
   bool                              _treesAnchored;
   };

}

#endif
