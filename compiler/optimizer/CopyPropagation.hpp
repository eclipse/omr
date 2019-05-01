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

#ifndef COPYPROP_INCL
#define COPYPROP_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "infra/Checklist.hpp"

class TR_BitVector;
namespace TR { class SymbolReference; }
namespace TR { class Block; }
namespace TR { class Node; }
namespace TR { class TreeTop; }

// Copy Propagation
//
// Uses the information computed by ReachingDefinitions (stored in
// UseDefInfo) to perform the transformation. If a use node has a
// unique corresponding definition node (that is equivalent to a
// copy statement), then all occurrences of the copy in the use
// node can be replaced by the original value (the one that was copied).
// Note that after copy propagation is done, UseDefInfo might change
// because the program has been altered as a result of copy propagation.
//
// Dead Store Elimination might be a good optimization to perform
// after copy propagation is done; this could possibly eliminate
// the copy (definition) statement altogether in some cases.
//

class TR_CopyPropagation : public TR::Optimization
   {
   public:

   // Performs copy propagation using the
   // use/def values of relevant nodes.
   //
   TR_CopyPropagation(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_CopyPropagation(manager);
      }

   virtual int32_t perform();
   virtual const char * optDetailString() const throw();

   private :
   void replaceCopySymbolReferenceByOriginalIn(TR::SymbolReference * copySymRef, TR::Node * rhsOfStoreDefNode, TR::Node * useNode, TR::Node *defNode, TR::Node * baseAddrNode = NULL,bool baseAddrAvail = false);
   void replaceCopySymbolReferenceByOriginalRegLoadIn(TR::Node *regLoadNode, TR::Node *useNode, TR::SymbolReference *copySymbolReference, TR::Node *node, TR::Node *parent, int32_t childNum);
   void adjustUseDefInfo(TR::Node *, TR::Node *, TR_UseDefInfo *);
   bool isUniqueDefinitionOfUse(TR_BitVector *, TR_UseDefInfo *, int32_t);
   bool isLoadNodeSuitableForPropagation(TR::Node * useNode, TR::Node * storeNode, TR::Node * loadNode);
   TR::Node *areAllDefsInCorrectForm(TR::Node*, const TR_UseDefInfo::BitVector &, TR_UseDefInfo *, int32_t, TR::Node * &, int32_t &, TR::Node * &);
   bool isSafeToPropagate(TR::Node *, TR::Node *);
   TR::Node* skipTreeTopAndGetNode (TR::TreeTop* tt);
   bool isCorrectToPropagate(TR::Node *, TR::Node *, TR::list< TR::Node *> &, TR::SparseBitVector &, int32_t, const TR_UseDefInfo::BitVector &, TR_UseDefInfo *);
   bool isCorrectToReplace(TR::Node *, TR::Node *, const TR_UseDefInfo::BitVector &, TR_UseDefInfo *);
   bool isRedefinedBetweenStoreTreeAnd(TR::list< TR::Node *> & checkNodes, TR::SparseBitVector &, TR::Node * storeNode, TR::TreeTop * exitTree, int32_t regNumber, const TR_UseDefInfo::BitVector &, TR_UseDefInfo *);
   bool recursive_isRedefinedBetweenStoreTreeAnd(TR::list< TR::Node *> & checkNodes, TR::SparseBitVector &, TR::Node * storeNode, TR::TreeTop * exitTree, int32_t regNumber, const TR_UseDefInfo::BitVector &, TR_UseDefInfo *);
   bool containsNode(TR::Node * node1, TR::Node * node2, bool searchClones = true);
   TR::Node *isLoadVarWithConst(TR::Node *);
   TR::Node *isIndirectLoadFromAuto(TR::Node *);
   TR::Node *isIndirectLoadFromRegister(TR::Node *, TR::Node * &);
   TR::Node *isValidRegLoad(TR::Node *, TR::TreeTop *, int32_t &);
   TR::Node *isCheapRematerializationCandidate(TR::Node * , TR::Node *);
   bool containsLoadOfSymbol(TR::Node * node,TR::SymbolReference * symRef, TR::Node ** loadNode);
   TR::Node * isBaseAddrAvailable(TR::Node *, TR::Node *, bool &);
   bool isNodeAvailableInBlock(TR::TreeTop *, TR::Node *);

   // The treetop for a use node is often required during
   // this optimization. To reduce compile time overhead, they
   // are stashed into the _useTreeTops map. This should be respected
   // by updating this map if existing uses are moved to new treetops
   //
   void collectUseTrees(TR::TreeTop *tree, TR::Node *node, TR::NodeChecklist &checklist);
   void findUseTree(TR::Node *);

   TR::TreeTop *findAnchorTree(TR::Node *, TR::Node *);
   void rematerializeIndirectLoadsFromAutos();
   void commonIndirectLoadsFromAutos();

   bool _canMaintainUseDefs;
   bool _cleanupTemps;
   TR::TreeTop *_storeTree;
   TR::TreeTop *_useTree;

   typedef TR::typed_allocator<std::pair<TR::Node* const, TR::TreeTop*>, TR::Region&> StoreTreeMapAllocator;
   typedef std::less<TR::Node*> StoreTreeMapComparator;
   typedef std::map<TR::Node *, TR::TreeTop *, StoreTreeMapComparator, StoreTreeMapAllocator> StoreTreeMap;
   StoreTreeMap _storeTreeTops;

   typedef TR::typed_allocator<std::pair<TR::Node* const, TR::TreeTop*>, TR::Region&> UseTreeMapAllocator;
   typedef std::less<TR::Node*> UseTreeMapComparator;
   typedef std::map<TR::Node*, TR::TreeTop*, UseTreeMapComparator, UseTreeMapAllocator> UseTreeMap;
   UseTreeMap _useTreeTops;
   
   TR::Block *_storeBlock;
   bool _lookForOriginalDefs;
   bool _propagatingWholeExpression;
   };

#endif
