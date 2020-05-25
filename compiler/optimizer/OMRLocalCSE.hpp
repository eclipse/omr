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

#ifndef OMR_LOCALCSE_INCL
#define OMR_LOCALCSE_INCL

#include <map>
#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "cs2/arrayof.h"
#include "env/TypedAllocator.hpp"
#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/SymbolReference.hpp"
#include "infra/Array.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"

class TR_UseDefAliasSetInterface;
namespace TR { class Block; }
namespace TR { class LocalCSE; }
namespace TR { class OptimizationManager; }
namespace TR { class TreeTop; }

namespace OMR
{
/**
 * Class LocalCSE (Local Common Subexpression Elimination)
 * =======================================================
 *
 * LocalCSE implements an algorithm that proceeds as follows:
 *
 * Each block is examined starting from the first treetop scanning
 * down to the last treetop. Two transformations are being done at
 * the same time, namely local copy propagation and local common
 * subexpression elimination.
 *
 * 1. Local copy propagation is done first and it tries to
 * basically propagate RHS of assignments to a symbol to its uses.
 * The propagation is done in a bottom-up manner, i.e. children are propagated
 * before parents. This has the desirable effect of removing the ambiguity
 * between values that are really the same but are stored (copied) at an
 * intermediate stage. This is similar to the information we would get from
 * local value numbering.
 *
 * 2. Local CSE is done on the same subtree after local copy propagation
 * but it is done in a top down manner so as to capture any commoning
 * opportunities as high up in the subtree as possible (i.e. the
 * maximum subtree is commoned).
 */

class LocalCSE : public TR::Optimization
   {
   public:

   // Performs local common subexpression elimination within
   // a basic block.
   //
   LocalCSE(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager);

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual void postPerformOnBlocks();
   virtual const char * optDetailString() const throw();

   typedef TR::typed_allocator< std::pair< const int32_t, TR::Node* >, TR::Region & > HashTableAllocator;
   typedef std::multimap< int32_t, TR::Node*, std::less<int32_t>, HashTableAllocator > HashTable;

   protected:

   virtual bool shouldTransformBlock(TR::Block *block);
   virtual bool shouldCopyPropagateNode(TR::Node *parent, TR::Node *node, int32_t childNum, TR::Node *storeNode);
   virtual bool shouldCommonNode(TR::Node *parent, TR::Node *node);

   virtual void onNewTreeTop(TR::TreeTop *tt) { _treeBeingExamined = tt;}

   virtual bool isTreetopSafeToCommon() { return true; }
   virtual bool canAffordToIncreaseRegisterPressure(TR::Node *node = NULL) { return true; }

   virtual void prepareToCopyPropagate(TR::Node *node, TR::Node *storeDefNode) {}

   protected:

   bool doExtraPassForVolatiles();
   int32_t hash(TR::Node *parent, TR::Node *node);
   void addToHashTable(TR::Node *node, int32_t hashValue);
   void removeFromHashTable(HashTable *hashTable, int32_t hashValue);
   TR::Node *replaceCopySymbolReferenceByOriginalIn(TR::SymbolReference *,/* TR::SymbolReference *,*/ TR::Node *, TR::Node *, TR::Node *, TR::Node *, int32_t);
   void examineNode(TR::Node *, TR_BitVector &, TR::Node *, int32_t, int32_t *, bool *, int32_t);
   void commonNode(TR::Node *, int32_t, TR::Node *, TR::Node *);
   void transformBlock(TR::TreeTop *, TR::TreeTop *);
   void getNumberOfNodes(TR::Node *);
   bool allowNodeTypes(TR::Node *storeNode, TR::Node *node);
   void setIsInMemoryCopyPropFlag(TR::Node *rhsOfStoreDefNode);
   void makeNodeAvailableForCommoning(TR::Node *, TR::Node *, TR_BitVector &, bool *);
   bool canBeAvailable(TR::Node *, TR::Node *, TR_BitVector &, bool);
   bool isAvailableNullCheck(TR::Node *, TR_BitVector &);
   TR::Node *getAvailableExpression(TR::Node *parent, TR::Node *node);
   bool killExpressionsIfVolatileLoad(TR::Node *node, TR_BitVector &seenAvailableLoadedSymbolReferences, TR_UseDefAliasSetInterface &UseDefAliases);
   void killAvailableExpressionsAtGCSafePoints(TR::Node *, TR::Node *, TR_BitVector &);
   void killAllAvailableExpressions();
   void killAllDataStructures(TR_BitVector &);
   void killAvailableExpressions(int32_t);
   void killAvailableExpressionsUsingBitVector(HashTable *hashTable, TR_BitVector &vec);
   void killAvailableExpressionsUsingAliases(TR_UseDefAliasSetInterface &);
   void killAvailableExpressionsUsingAliases(TR_BitVector &);
   void killAllInternalPointersBasedOnThisPinningArray(TR::SymbolReference *symRef);

   bool areSyntacticallyEquivalent(TR::Node *, TR::Node *, bool *);
   void collectAllReplacedNodes(TR::Node *, TR::Node *);
   rcount_t recursivelyIncReferenceCount(TR::Node *);
   bool isFirstReferenceToNode(TR::Node *parent, int32_t index, TR::Node *node, vcount_t visitCount);

   bool isIfAggrToBCDOverride(TR::Node *parent, TR::Node *rhsOfStoreDefNode, TR::Node *nodeBeingReplaced);

   bool doCopyPropagationIfPossible(TR::Node *, TR::Node *, int32_t, TR::Node *, TR::SymbolReference *, vcount_t, bool &);
   void doCommoningIfAvailable(TR::Node *, TR::Node *, int32_t, bool &);
   void doCommoningAgainIfPreviouslyCommoned(TR::Node *, TR::Node *, int32_t);
   bool canCommonNodeInVolatilePass(TR::Node*);

   TR::Node *getNode(TR::Node *node);

   TR::TreeTop *_treeBeingExamined;
   typedef TR::typed_allocator<std::pair<uint32_t const, TR::Node*>, TR::Region&> StoreMapAllocator;
   typedef std::less<uint32_t> StoreMapComparator;
   typedef std::map<uint32_t, TR::Node*, StoreMapComparator, StoreMapAllocator> StoreMap;
   StoreMap *_storeMap;

   TR::Node **_nullCheckNodesAsArray;
   TR::Node **_simulatedNodesAsArray;
   TR::Node **_replacedNodesAsArray;
   TR::Node **_replacedNodesByAsArray;
   int32_t *_symReferencesTable;

   TR_BitVector _seenCallSymbolReferences;
   TR_BitVector _seenSymRefs;
   TR_BitVector _possiblyRelevantNodes;
   TR_BitVector _relevantNodes;
   TR_BitVector _parentAddedToHT;
   TR_BitVector _killedNodes;
   TR_BitVector _availableLoadExprs;
   TR_BitVector _availableCallExprs;
   TR_BitVector _availablePinningArrayExprs;
   TR_BitVector _killedPinningArrayExprs;

   HashTable *_hashTable;
   HashTable *_hashTableWithSyms;
   HashTable *_hashTableWithCalls;
   HashTable *_hashTableWithConsts;

   int32_t _numNullCheckNodes;
   int32_t _numNodes;
   int32_t _numCopyPropagations;
   vcount_t _maxVisitCount;
   int32_t _nextReplacedNode;

   bool _mayHaveRemovedChecks;
   bool _canBeAvailable;
   bool _isAvailableNullCheck;
   bool _inSubTreeOfNullCheckReference;
   bool _isTreeTopNullCheck;
   TR::Block *_curBlock;
   TR_ScratchList<TR::Node> *_arrayRefNodes;

   bool _loadaddrAsLoad;

   int32_t _volatileState;
   };

}

#endif
