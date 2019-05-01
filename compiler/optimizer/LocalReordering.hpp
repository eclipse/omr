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

#ifndef LREORDER_INCL
#define LREORDER_INCL

#include <stdint.h>
#include "il/Node.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"

class TR_BitVector;
namespace TR { class Block; }
namespace TR { class TreeTop; }

/*
 * Class TR_LocalReordering
 * ========================
 * 
 * Local reordering is an optimization that aims to reduce the (register) live 
 * ranges of RHS values of stores in two ways:
 * 
 * 1) By "delaying" a store closer to its use (if that use is in the same block) 
 * so that the RHS value will be live across a smaller range of IL trees once 
 * local copy propagation happens and the use is replaced by a commoned 
 * reference to the RHS value.
 * 
 * 2) By "moving earlier" a store whose RHS is anchored (i.e. evaluated) 
 * earlier in the block before the store. In this case, by moving the store 
 * earlier, the live range of the RHS value may be reduced (if there was 
 * no later use of the RHS value).
 *
 * This optimization's thrust is (register) live range reduction and 
 * it runs on IL trees in the common code optimizer. A more general 
 * "local live range reduction" (that is the name of the pass) optimization
 * was implemented later and it aims to reduce live ranges in even more 
 * cases (including some cases independent of stores). However local reordering 
 * was not disabled even after local live range reduction was enabled.
 */

class TR_LocalReordering  : public TR::Optimization
   {
   public:
   // Performs local reordering within
   // a basic block.
   //
   TR_LocalReordering(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LocalReordering(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual void postPerformOnBlocks();
   virtual const char * optDetailString() const throw();

   private :
   bool transformBlock(TR::Block *);
   void delayDefinitions(TR::Block *);
   void collectUses(TR::Block *);
   void setUseTreeForSymbolReferencesIn(TR::TreeTop *, TR::Node *, vcount_t);
   bool containsBarriers(TR::Block *);
   void insertDefinitionBetween(TR::TreeTop *, TR::TreeTop *);
   bool isAnySymInDefinedOrUsedBy(TR::Node *, vcount_t);
   bool isAnySymInDefinedBy(TR::Node *, vcount_t);
   void moveStoresEarlierIfRhsAnchored(TR::Block *, TR::TreeTop *, TR::Node *, TR::Node *, vcount_t);
   void collectSymbolsUsedAndDefinedInNode(TR::Node *, vcount_t);
   bool insertEarlierIfPossible(TR::TreeTop *, TR::TreeTop *, bool);
   bool isSubtreeCommoned(TR::Node *);

   TR_BitVector *_seenSymbols, *_stopNodes, *_temp;
   TR::TreeTop **_treeTopsAsArray;
   TR::TreeTop **_storeTreesAsArray;
   int32_t _numStoreTreeTops;
   bool _seenUnpinnedInternalPointer;
   int32_t _counter;
   };

#endif
