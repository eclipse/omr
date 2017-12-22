/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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

#include "optimizer/IsolatedStoreElimination.hpp"

#include <stdint.h>                            // for int32_t, uint32_t, etc
#include <stdio.h>                             // for NULL, printf
#include "codegen/CodeGenerator.hpp"           // for CodeGenerator
#include "codegen/FrontEnd.hpp"                // for TR_FrontEnd
#include "compile/Compilation.hpp"             // for Compilation, etc
#include "compile/SymbolReferenceTable.hpp"    // for SymbolReferenceTable
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/bitvectr.h"                      // for ABitVector<>::Cursor, etc
#include "env/IO.hpp"                          // for POINTER_PRINTF_FORMAT
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "il/Block.hpp"                        // for Block, toBlock
#include "il/DataTypes.hpp"                    // for DataTypes::Int32, etc
#include "il/ILOpCodes.hpp"                    // for ILOpCodes::treetop, etc
#include "il/ILOps.hpp"                        // for ILOpCode, TR::ILOpCode
#include "il/Node.hpp"                         // for Node, etc
#include "il/Node_inlines.hpp"                 // for Node::getFirstChild, etc
#include "il/Symbol.hpp"                       // for Symbol
#include "il/SymbolReference.hpp"              // for SymbolReference
#include "il/TreeTop.hpp"                      // for TreeTop
#include "il/TreeTop_inlines.hpp"              // for TreeTop::getNode, etc
#include "il/symbol/RegisterMappedSymbol.hpp"  // for RegisterMappedSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"  // for ResolvedMethodSymbol
#include "infra/Array.hpp"                     // for TR_Array
#include "infra/Assert.hpp"                    // for TR_ASSERT
#include "infra/BitVector.hpp"                 // for TR_BitVector, etc
#include "infra/Cfg.hpp"                       // for CFG
#include "infra/List.hpp"                      // for List, ListIterator, etc
#include "infra/CfgEdge.hpp"                   // for CFGEdge
#include "infra/CfgNode.hpp"                   // for CFGNode
#include "optimizer/Optimization.hpp"          // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"             // for Optimizer
#include "optimizer/Structure.hpp"             // for TR_RegionStructure, etc
#include "optimizer/TransformUtil.hpp"         // for TransformUtil
#include "optimizer/UseDefInfo.hpp"            // for TR_UseDefInfo, etc
#include "optimizer/VPConstraint.hpp"          // for TR::VPConstraint
#include "ras/Debug.hpp"                       // for TR_DebugBase


TR_IsolatedStoreElimination::TR_IsolatedStoreElimination(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _mustUseUseDefInfo(false),
     _deferUseDefInfoInvalidation(false),
     _currentTree(NULL),
     _usedSymbols(NULL),
     _storeNodes(NULL),
     _defParentOfUse(NULL),
     _defStatus(NULL),
     _groupsOfStoreNodes(NULL),
     _trivialDefs(NULL)
   {
   }

struct UnsafeSubexpressionRemoval   // PR69613
   {
   TR_IsolatedStoreElimination *_ise;
   TR_BitVector _visitedNodes;
   TR_BitVector _unsafeNodes; // Nodes whose mere evaluation is not safe

   TR::Compilation *comp(){ return _ise->comp(); }
   bool           trace(){ return _ise->trace(); }

   bool isVisited(TR::Node *node) { return  _visitedNodes.isSet(node->getGlobalIndex()); }
   bool isUnsafe(TR::Node *node)
      {
      TR_ASSERT(isVisited(node), "Cannot call isUnsafe on n%dn before anchorSafeChildrenOfUnsafeNodes", node->getGlobalIndex());
      return _unsafeNodes.isSet(node->getGlobalIndex());
      }

   UnsafeSubexpressionRemoval(TR_IsolatedStoreElimination *ise):
      _ise(ise),
      _visitedNodes(ise->comp()->getNodeCount(), comp()->trMemory(), stackAlloc),
      _unsafeNodes (ise->comp()->getNodeCount(), comp()->trMemory(), stackAlloc)
      {}

   void recordDeadUse(TR::Node *node)
      {
      _visitedNodes.set(node->getGlobalIndex());
      _unsafeNodes .set(node->getGlobalIndex());
      }

   void anchorSafeChildrenOfUnsafeNodes(TR::Node *node, TR::TreeTop *anchorPoint)
      {
      if (isVisited(node))
         return;
      else
         _visitedNodes.set(node->getGlobalIndex());

      //
      // Design note: we don't decrement refcounts in here.  Conceptually,
      // anchoring and decrementing are independent of each other.  More
      // pragmatically, you end up at with a well-defined state when this
      // function finishes: if the top-level node is unsafe, the caller must
      // unhook it and call recursivelyDecReferenceCount, which will do the
      // right thing.  Otherwise, no decrementing is necessary anyway.
      //
      // A rule of thumb to remember is that this function will do nothing if
      // the node is safe.  If you call this, then find !isUnsafe(node), then
      // the trees did not change.
      //

      // Propagate unsafeness from children, unless we've already done it
      //
      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         {
         TR::Node *child = node->getChild(i);
         anchorSafeChildrenOfUnsafeNodes(child, anchorPoint);
         if (isUnsafe(child))
            {
            _unsafeNodes.set(node->getGlobalIndex());
            if (trace())
               {
               TR::Node *child = node->getChild(i);
               traceMsg(comp(), "        (Marked %s n%dn unsafe due to dead child #%d %s n%dn)\n",
                  node->getOpCode().getName(), node->getGlobalIndex(), i,
                  child->getOpCode().getName(), child->getGlobalIndex());
               }
            }
         }

      // If node is safe, all its descendant nodes must be safe.  Nothing to do.
      //
      // If node is unsafe, anchor all its safe children.  Its unsafe children,
      // at this point, must already have taken care of their own safe
      // subexpressions recursively.
      //
      // Note that this could anchor some nodes multiple times, for complex
      // subexpressions with commoning.  We could make some feeble attempts to
      // prevent that, but let's just leave it to deadTreesElimination.
      //
      if (isUnsafe(node))
         {
         for (int32_t i = 0; i < node->getNumChildren(); ++i)
            {
            TR::Node *child = node->getChild(i);
            bool didIt = anchorIfSafe(child, anchorPoint);
            if (didIt && trace())
               {
               traceMsg(comp(), "  - Anchored child #%d %s n%d of %s n%d\n",
                  i,
                  child->getOpCode().getName(), child->getGlobalIndex(),
                  node->getOpCode().getName(), node->getGlobalIndex());
               }
            }
         }
      }

   bool anchorIfSafe(TR::Node *node, TR::TreeTop *anchorPoint)
      {
      if (!isVisited(node))
         anchorSafeChildrenOfUnsafeNodes(node, anchorPoint);

      if (isUnsafe(node))
         {
         return false;
         }
      else
         {
         anchorPoint->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, node)));
         return true;
         }
      }

   };

int32_t TR_IsolatedStoreElimination::perform()
   {
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   _storeNodes  = new(trStackMemory()) TR_Array<TR::Node *>(trMemory(), 64, true, stackAlloc);

   // If there is use/def information available, use it to find isolated stores.
   // Otherwise we must munge through the trees.
   //
   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();
   int32_t cost = 0;

   _mustUseUseDefInfo = false;

   // The use of _mustUseUseDefInfo is temporary until all opts maintain proper
   // changes to use/def info
   //
   if (useDefInfo)
      {
      if (trace())
          traceMsg(comp(), "Starting Global Store Elimination (using use/def info)\n");
      cost = performWithUseDefInfo();
      }
   else
      {
      if (trace())
          traceMsg(comp(), "Starting Global Store Elimination (without using use/def info)\n");
      cost = performWithoutUseDefInfo();
      }

   // Now remove the isolated stores.
   //
   bool eliminatedStore = false;
#ifdef J9_PROJECT_SPECIFIC
   bool eliminatedBCDStore = false;
#endif
   if (useDefInfo)
      {
      // Scan for unsafe nodes, which are loads that are uses of dead stores,
      // or (recusrively) any node with at least one unsafe child.
      // Even evaluating an unsafe node could cause incorrect behaviour; for
      // example, if the unsafe node dereferences a load of a dead pointer
      // whose store got deleted, then that dereference could crash at runtime.
      //
      UnsafeSubexpressionRemoval usr(this);
      for (uint32_t groupIndex = 0; groupIndex < _groupsOfStoreNodes->size(); groupIndex++)
         {
         TR_BitVector *groupOfStores = _groupsOfStoreNodes->element(groupIndex);
         if (trace())
            traceMsg(comp(), "  Scanning store group %d for dead uses\n", groupIndex);
         if (groupOfStores)
            {
            TR_BitVectorIterator storeIter(*groupOfStores);
            while (storeIter.hasMoreElements())
               {
               int32_t defIndex = storeIter.getNextElement() + useDefInfo->getFirstDefIndex();
               if (trace())
                  {
                  TR::Node *defNode = useDefInfo->getNode(defIndex);
                  traceMsg(comp(), "    Scanning def %d %s n%dn for dead uses\n", defIndex, defNode->getOpCode().getName(), defNode->getGlobalIndex());
                  }
               TR_UseDefInfo::BitVector useIndexes(comp()->allocator());
               useDefInfo->getUsesFromDef(useIndexes, defIndex);
               TR_UseDefInfo::BitVector::Cursor useCursor(useIndexes);
               for (useCursor.SetToFirstOne(); useCursor.Valid(); useCursor.SetToNextOne())
                  {
                  int32_t useIndex = (int32_t)useCursor + useDefInfo->getFirstUseIndex();
                  TR::Node *useNode = useDefInfo->getNode(useIndex);
                  usr.recordDeadUse(useNode);
                  if (trace())
                     traceMsg(comp(), "      Marked use %d %s n%dn dead\n", useIndex, useNode->getOpCode().getName(), useNode->getGlobalIndex());
                  }
               }
            }
         }

      // Now eliminate the dead stores
      //
      for (uint32_t groupIndex = 0; groupIndex < _groupsOfStoreNodes->size(); groupIndex++)
         {
         TR_BitVector *groupOfStores = _groupsOfStoreNodes->element(groupIndex);

         if (groupOfStores && performTransformation(comp(), "%sGlobal Store Elimination eliminating group %d:\n", optDetailString(), groupIndex))
            {
            TR_BitVectorIterator bviUses(*groupOfStores);
            while (bviUses.hasMoreElements())
               {
               int32_t index = bviUses.getNextElement() + useDefInfo->getFirstDefIndex();
               TR::Node *node = useDefInfo->getNode(index);
               TR::TreeTop *treeTop = useDefInfo->getTreeTop(index);
               TR_ASSERT(node, "node should not be NULL at this point");
               dumpOptDetails(comp(), "%sRemoving %s n%dn [" POINTER_PRINTF_FORMAT "] %s\n",
                  optDetailString(), node->getOpCode().getName(), node->getGlobalIndex(), node, node->getSymbolReference()->getSymbol()->isShadow() ? "(shadow)" : "");

#ifdef J9_PROJECT_SPECIFIC
               if (node->getOpCode().isBCDStore())
                  eliminatedBCDStore = true;
#endif


               _trivialDefs->reset(index); // It's not a trivial def if we're handling it as a group.  If we try to do both, BOOM!

               // Time to delete the store.  This is more complex than it sounds.
               //
               // Generally, we want to replace the store with a treetop (ie. a
               // no-op).  However, this is complicated by several factors, in
               // order of increasing subtlety and complexity:
               //
               // 1. Stores can have multiple children -- as many as three for
               //    an iwrtbar.  If the store is turning into a nop, we must
               //    anchor all the other children.
               // 2. Stores can appear under other nodes like CHKs and
               //    compressedRefs.  Particularly for the CHKs, we must
               //    perform the appropriate checks despite eliminating the
               //    store, and so we must leave the store node in the
               //    appropriate shape so its parents will do the right thing.
               // 3. Some expressions are unsafe to evaluate after stores are
               //    eliminated, and those expressions themselves must NOT be
               //    anchored.
               //
               // In these transformations, we adopt the convention that we
               // retain the original store whenever possible, modifying it
               // in-place, and we retain its first child if possible.  These
               // conventions are sometimes required, and so we just adopt them
               // universally for the sake of consistency, efficiency, and
               // readability in the jit logs.

               // Anchor all children but the first
               //
               for (int32_t i = 1; i < node->getNumChildren(); i++)
                  {
                  TR::Node *child = node->getChild(i);
                  usr.anchorIfSafe(child, treeTop);
                  child->recursivelyDecReferenceCount();
                  }

               node->setNumChildren(1);
               //
               // Note that the node at this point could be half-baked: an indirect
               // store or write barrier with one child.  This is an intermediate
               // state, and further surgery is required for correctness.

               // Eliminate the store
               //
               if (treeTop->getNode()->getOpCode().isSpineCheck() && treeTop->getNode()->getFirstChild() == node)
                  {
                  // SpineCHKs are special. We don't want to require all the
                  // opts and codegens to do the right thing with a PassThrough
                  // child.  Hence, we'll turn the tree into a const, which is
                  // a state the opts and codegens must cope with anyway,
                  // because if this tree had been a load instead of a store,
                  // the optimizer could have legitimately turned it into a const.
                  //
                  TR::Node *child = node->getChild(0);
                  usr.anchorIfSafe(child, treeTop);
                  child->recursivelyDecReferenceCount();
                  TR::Node::recreate(node, comp()->il.opCodeForConst(node->getSymbolReference()->getSymbol()->getDataType()));
                  node->setFlags(0);
                  node->setNumChildren(0);
                  }
               else
                  {
                  // Vandalize the first child if it's unsafe to evaluate
                  //
                  TR::Node *child = node->getFirstChild();
                  usr.anchorSafeChildrenOfUnsafeNodes(child, treeTop);
                  if (usr.isUnsafe(child))
                     {
                     // Must eradicate the dead expression.  Can't leave it
                     // anchored because it's not safe to evaluate.
                     //
                     child->recursivelyDecReferenceCount();
                     TR::Node *dummyChild = node->setAndIncChild(0, TR::Node::createConstDead(child, TR::Int32, 0xbad1 /* eyecatcher */));
                     if (trace())
                        traceMsg(comp(), "  - replace unsafe child %s n%dn with dummy %s n%dn\n",
                           child->getOpCode().getName(), child->getGlobalIndex(),
                           dummyChild->getOpCode().getName(), dummyChild->getGlobalIndex());
                     }

                  // Set opcode to nop
                  //
                  if (node->getReferenceCount() >= 1)
                     TR::Node::recreate(node, TR::PassThrough);
                  else
                     TR::Node::recreate(node, TR::treetop);
                  }

               node->setFlags(0);
               eliminatedStore = true;
               }
            }
         }

      TR_BitVectorIterator bvi(*_trivialDefs);
      while (bvi.hasMoreElements())
         {
         int32_t index = bvi.getNextElement();
         int32_t useDefIndex = index + useDefInfo->getFirstDefIndex();
         TR::Node *node = useDefInfo->getNode(useDefIndex);
         if (node)
            {
            if (trace())
               {
               traceMsg(comp(), "removing trivial node %p %s n%dn udi=%d\n", node, node->getOpCode().getName(), node->getGlobalIndex(), useDefIndex);
               traceMsg(comp(), "correcting UseDefInfo:\n");
               }
            TR_UseDefInfo::BitVector defsOfRhs(comp()->allocator());
            TR_UseDefInfo::BitVector nodeUses(comp()->allocator());
            if (useDefInfo->getUseDef(defsOfRhs, node->getFirstChild()->getUseDefIndex()) && useDefInfo->getUsesFromDef(nodeUses, useDefIndex))
               {
               TR_UseDefInfo::BitVector::Cursor cursor1(defsOfRhs);
               for (cursor1.SetToFirstOne(); cursor1.Valid(); cursor1.SetToNextOne())
                  {
                  int32_t nextDef = cursor1;
                  TR_UseDefInfo::BitVector::Cursor cursor2(nodeUses);
                  for (cursor2.SetToFirstOne(); cursor2.Valid(); cursor2.SetToNextOne())
                     {
                     int32_t nextUse = (int32_t) cursor2 + useDefInfo->getFirstUseIndex();
                     useDefInfo->resetUseDef(nextUse, index);
                     useDefInfo->setUseDef(nextUse, nextDef);
                     if (trace())
                        traceMsg(comp(), "  useDefIndex %d defines %d\n", nextDef, nextUse);
                     }
                  }
               }

             useDefInfo->clearNode(node->getUseDefIndex());

             if (node->getReferenceCount() < 1)
                {
                node->setFlags(0);
                TR::Node::recreate(node, TR::treetop);
                }
             else
                TR::Node::recreate(node, TR::PassThrough);
             node->setFlags(0);
             eliminatedStore = true;
             dumpOptDetails(comp(), "%p \n",node);
             }
         }
         //if (debug("deadStructure"))
         //{
         //
         // Simplifying assumption made to avoid complications
         // arising out of commoning across extended basic blocks that
         // are partly in the loop and aprtly out of the loop.
         // In such cases, the uses may appear to be all concentrated inside
         // the loop but may not be the case because of commoning.
         //
         bool blocksHaveBeenExtended = false;
         TR::Block *block = comp()->getStartTree()->getNode()->getBlock();
         for (; block; block = block->getNextBlock())
            {
            if (block->isExtensionOfPreviousBlock())
               {
               blocksHaveBeenExtended = true;
               break;
               }
            }

         if (!blocksHaveBeenExtended && comp()->mayHaveLoops())
            performDeadStructureRemoval(optimizer()->getUseDefInfo());
         //}
      }
   else if (!_mustUseUseDefInfo)
      {
      for (int32_t i = _storeNodes->size()-1; i >= 0; --i)
         {
         TR::Node *node = _storeNodes->element(i);
         if (node && performTransformation(comp(), "%s   Global Store Elimination eliminating : %p\n", optDetailString(), node))
            {
            if (useDefInfo)
               useDefInfo->clearNode(node->getUseDefIndex());
            if (node->getReferenceCount() < 1)
               {
               node->setFlags(0);
               TR::Node::recreate(node, TR::treetop);
               }
            else
               TR::Node::recreate(node, TR::PassThrough);
            node->setFlags(0);
            eliminatedStore = true;
            }
         }
      }

   if (eliminatedStore)
      {
#ifdef J9_PROJECT_SPECIFIC
      if (eliminatedBCDStore)
         requestOpt(OMR::treeSimplification); // some of the more complex BCD simplifications work best when refCounts == 1
#endif
      requestDeadTreesCleanup();
      requestOpt(OMR::catchBlockRemoval);
      }

   if (_deferUseDefInfoInvalidation)
      {
      _deferUseDefInfoInvalidation = false;
      // useDefInfo was invalidated (deferred) so invalidate it now and release its memory
      optimizer()->setUseDefInfo(NULL);
      useDefInfo = NULL;
      }

   if (trace())
      traceMsg(comp(), "\nEnding Global Store Elimination\n");

   return cost; // Actual cost
   }

bool TR_IsolatedStoreElimination::canRemoveStoreNode(TR::Node *node)
   {
   if (_currentTree) comp()->setCurrentBlock(_currentTree->getEnclosingBlock());
   // We need to check for the special case when the RHS is a method meta data
   // type as we have observed that this could in fact be some VM dependent
   // operation that we do not want to remove.
   //

   if (node->getSymbolReference()->getSymbol()->isVolatile())
      return false;

   if (node->dontEliminateStores())
      return false;

   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();


   return true;
   }

void TR_IsolatedStoreElimination::removeRedundantSpills()
   {
   // Checking the following:
   //
   // iRegStore #123
   //       iload #123
   // ...
   //
   // istore #123        <<< can be removed
   //     iRegLoad #123
   //
   TR_UseDefInfo *useDefInfo = optimizer()->getUseDefInfo();

   for (TR::TreeTop *tt = comp()->getStartTree(); tt != 0; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      if (node->getOpCode().isStoreDirect() &&
          node->getFirstChild()->getOpCode().isLoadReg() &&
          node->getSymbolReference() == node->getFirstChild()->getRegLoadStoreSymbolReference())
         {
         uint16_t useIndex = node->getFirstChild()->getUseDefIndex();
         if (!useIndex || !useDefInfo->isUseIndex(useIndex))
            continue;

         TR_UseDefInfo::BitVector defs(comp()->allocator());
         if (useDefInfo->getUseDef(defs, useIndex))
            {
            bool redundantStore= true;
            TR_UseDefInfo::BitVector::Cursor cursor(defs);
            for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
               {
               int32_t defIndex = cursor;
               TR::Node *defNode;

               if (defIndex >= useDefInfo->getFirstRealDefIndex() &&
                   (defNode = useDefInfo->getNode(defIndex)) &&
                   defNode->getOpCode().isStoreReg() &&
                   defNode->getFirstChild()->getOpCode().isLoadVarDirect() &&
                   defNode->getFirstChild()->getSymbolReference() == node->getSymbolReference())
                  {
                  // traceMsg(comp(), "Redundant store node=%p defNode=%p \n", node, defNode);
                  }
               else
                  {
                  redundantStore = false;
                  }
               }
               if (redundantStore &&
                   performTransformation(comp(), "%s Removing redundant spill:  (%p)\n", optDetailString(), node))
                  {
                  TR::Node::recreate(node, TR::treetop);
                  node->setFlags(0);
                  }
            }
         }
      }
   }


int32_t TR_IsolatedStoreElimination::performWithUseDefInfo()
   {
   removeRedundantSpills();

   TR_UseDefInfo *info = optimizer()->getUseDefInfo();
   int32_t i;

   // Build up the set of all defs that are used by or'ing the usedef info for
   // each use. The isolated stores are the ones that are left.
   //
   const int32_t bitVectorSize = info->getNumDefOnlyNodes();

   _defParentOfUse  = new(trStackMemory()) TR_Array<int32_t>(trMemory(), info->getNumUseNodes(), true, stackAlloc);
   for (i = 0; i < info->getNumUseNodes();i++)
      _defParentOfUse->add(-1);
   //*******************************************************************************************
   //FIXME: instead of having an array of int, use 4 bitvectors of states.
   //One bitvetor will indicate if the node was visited or not.
   //For those that were visited the corresponding bit in one of the bitvectors for ("ToBeRemoved" and "ShouldNotBeRemoved") should be set.
   //The 4th bitvector is inTransit vector.
   //So "NotToBeExamine should set the bits in "Visited" and also on "ShouldNotBeRemoved".
   //After finding the group set the right bits in "ToBeRemoved" or "ShouldNotBeRemoved and update Visited vector. Also, emtpy InTransit vector
   //*******************************************************************************************

   _defStatus = new(trStackMemory()) TR_Array<defStatus>(trMemory(), info->getNumDefOnlyNodes(), true, stackAlloc);
   _defStatus->setSize(info->getNumDefOnlyNodes());

   _trivialDefs = new (trStackMemory()) TR_BitVector(info->getNumDefOnlyNodes(), trMemory(), stackAlloc);

   _groupsOfStoreNodes = new(trStackMemory()) TR_Array<TR_BitVector *>(trMemory(), 4, true, stackAlloc);

   info->buildDefUseInfo();

   for (i = info->getNumDefOnlyNodes()-1; i >= 0; --i)
      {
      int32_t useDefIndex = i + info->getFirstDefIndex();
      TR::Node *node = info->getNode(useDefIndex);
      if (node && node->getOpCode().isStore())
         {
         TR::Symbol *sym = node->getSymbolReference()->getSymbol();
         if (sym->isAuto() || sym->isParm() ||
                sym->isAutoField() || sym->isParmField() ||
                sym->isStatic()   // the ones that reach exit will have it as  a use
            )
            {
            //into array of the size of uses walking the defs
            collectDefParentInfo(useDefIndex,node,info);
            TR::Node *firstChild = node->getFirstChild();
            if (firstChild->getOpCode().isLoadVarDirect() &&
                firstChild->getReferenceCount() == 1 &&
                (firstChild->getSymbolReference() == node->getSymbolReference()))
                  {
               _trivialDefs->set(i);
                  if (trace())
                     traceMsg(comp(), "Found trivial node %p %s n%dn udi=%d\n", node, node->getOpCode().getName(), node->getGlobalIndex(), i);
                  }
            }
         else
            _defStatus->element(i) = doNotExamine;
         }
      else
         _defStatus->element(i) = doNotExamine;
      }

   TR_BitVector currentGroupOfStores(bitVectorSize, trMemory(), stackAlloc);
   for (i = info->getNumDefOnlyNodes()-1; i >= 0; --i)
      {
      if (_defStatus->element(i) == notVisited)
         {
         currentGroupOfStores.empty();

         bool foundGroupOfStores = false;
         foundGroupOfStores =  groupIsolatedStores(i,&currentGroupOfStores, optimizer()->getUseDefInfo());
         TR_BitVectorIterator bvi(currentGroupOfStores);

         while (bvi.hasMoreElements())
            {
            int32_t index = bvi.getNextElement();

            //update status
            if  (foundGroupOfStores)
               _defStatus->element(index) = toBeRemoved;
            else
               _defStatus->element(index) = notToBeRemoved;
            }

         if (foundGroupOfStores)
            {
            TR_BitVector *newGroupOfStoresToRemove = new (trStackMemory()) TR_BitVector(bitVectorSize, trMemory(), stackAlloc);
            newGroupOfStoresToRemove->setAll(bitVectorSize);
            *newGroupOfStoresToRemove &= currentGroupOfStores;
            _groupsOfStoreNodes->add(newGroupOfStoresToRemove);
            }


         }
      }

   return 0; // actual cost
   }

void TR_IsolatedStoreElimination::collectDefParentInfo(int32_t defIndex,TR::Node *node, TR_UseDefInfo *info)
   {
   if (node->getReferenceCount() > 1)
      return;

   int32_t childNum;
   for (childNum=0;childNum<node->getNumChildren();childNum++)
      {
      TR::Node *child = node->getChild(childNum);

      if ((child->getReferenceCount() == 1) &&
          child->getOpCode().isLoadVar())
        {
        int32_t index = child->getUseDefIndex();
        if (index > 0)                 //parm or auto
           {
           int32_t useIndex = index - info->getFirstUseIndex();
           _defParentOfUse->element(useIndex) = defIndex;
           if (trace())
              traceMsg(comp(), "DefParent - use %d has parent %d\n",useIndex,defIndex);
           }
        }
      collectDefParentInfo(defIndex,child,info);
      }

   return;
   }

// recusive check for node's children
static void examChildrenForValueNode(TR::TreeTop *tt, TR::Node *parent, TR::Node *valueNode, vcount_t visitCount)
   {
   if (parent->getVisitCount() == visitCount)
      return;

   parent->setVisitCount(visitCount);

   for (int32_t childCount = 0; childCount < parent->getNumChildren(); childCount++)
      {
      TR::Node *child = parent->getChild(childCount);
      if (child == valueNode)
         child->decFutureUseCount();

      examChildrenForValueNode(tt, child, valueNode, visitCount);
      }
   }


bool TR_IsolatedStoreElimination::groupIsolatedStores(int32_t defIndex,TR_BitVector *currentGroupOfStores, TR_UseDefInfo *info)
   {
   //   TR_ASSERT(defIndex < info->getNumDefOnlyNodes(),"DSE: not a real def");
   defStatus status = _defStatus->element(defIndex);
   if ( status == inTransit || status == toBeRemoved)
      {
      if (trace())
         traceMsg(comp(), "groupIsolated - DEF %d is inTransit or toBeRemoved - \n",defIndex);
      return true;
      }
   else if ( status == notToBeRemoved)
      {
      if (trace())
         traceMsg(comp(), "groupIsolated - DEF %d is notToBeRemoved - \n",defIndex);
      return false;
      }
   else if (status == notVisited)
      {
      _defStatus->element(defIndex) = inTransit;
      currentGroupOfStores->set(defIndex);
      if (trace())
         traceMsg(comp(), "groupIsolated - DEF %d is now investigated - \n", defIndex);
      }

   TR::Node *node = info->getNode(defIndex + info->getFirstDefIndex());
   TR_ASSERT(node, "assertion failure");
   TR::TreeTop *tt = info->getTreeTop(defIndex + info->getFirstDefIndex());
   if (tt)
      {
      comp()->setCurrentBlock(tt->getEnclosingBlock());
#ifdef J9_PROJECT_SPECIFIC
      // checking for killing of InMemoryLoadStore
      if (node->getValueChild()->getOpCode().isBCDLoadVar() &&
          node->getValueChild()->getOpCode().hasSymbolReference() &&
          comp()->cg()->IsInMemoryType(node->getValueChild()->getType()) &&
          node->getValueChild()->getReferenceCount() > 1)
         {
         TR::Node *valueChild = node->getValueChild();
         valueChild->setFutureUseCount(valueChild->getReferenceCount() - 1);
         vcount_t visitCount = comp()->incVisitCount();
         for (TR::TreeTop *ttTemp = tt->getNextTreeTop(); ttTemp && (valueChild->getFutureUseCount() > 0); ttTemp = ttTemp->getNextTreeTop())
            {
            TR::Node *defNode = ttTemp->getNode()->getOpCodeValue() == TR::treetop ? ttTemp->getNode()->getFirstChild() : ttTemp->getNode();
            bool defNodeHasSymRef = defNode->getOpCode().hasSymbolReference() && defNode->getSymbolReference();
            if (defNodeHasSymRef && defNode->getSymbolReference()->canKill(valueChild->getSymbolReference()))
               {
               if (trace())
                  traceMsg(comp(), "%s groupIsolated - DEF %d cannot be removed due to InMemoryLoadStoreMarking \n", optDetailString(), defIndex);
               return false;
               }
            examChildrenForValueNode(ttTemp, defNode, valueChild, visitCount);
            }
         }
#endif
      }

   if (!canRemoveStoreNode(node))
      {
      if (trace())
         traceMsg(comp(), "groupIsolated - DEF %d cannot be removed \n",defIndex);
      return false;
      }

   //Test that all uses of the def are under other store nodes (i.e. set in the defParent array).
   //If even one of the uses is not under store node, then it imposible to remove this def, and thus return.

   TR_UseDefInfo::BitVector usesOfThisDef(comp()->allocator());
   if (!info->getUsesFromDef(usesOfThisDef, defIndex+info->getFirstDefIndex()))
      {
      if (trace())
         traceMsg(comp(), "groupIsolated - DEF %d has no uses - can be removed \n", defIndex);
      return true;
      }
   else
      {
      if (trace())
         {
         (*comp()) << usesOfThisDef << "\n";
         }
      }

   TR_UseDefInfo::BitVector::Cursor cursor(usesOfThisDef);
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t useIndex = cursor;

      //if store has no uses, can be removed.
      if (_defParentOfUse->element(useIndex)== -1)
         {
         if (trace())
            traceMsg(comp(), "groupIsolated - Use %d has no def parent - \n",useIndex);
         return false;
         }
      }

   //recurse: for each use of the def, get its parent store and recurse.
   for (cursor.SetToFirstOne(); cursor.Valid(); cursor.SetToNextOne())
      {
      int32_t useIndex = cursor;
      int32_t defParentIndex = _defParentOfUse->element(useIndex);
         if (trace())
            traceMsg(comp(), "groupIsolated - recursing for Def %d (parent of %d) - \n",defParentIndex,useIndex);

      if (!groupIsolatedStores(defParentIndex,currentGroupOfStores,info))
          return false;
      }

      return true;
   }

//------------------------------------------------------------------------
//------------------------------------------------------------------------
int32_t TR_IsolatedStoreElimination::performWithoutUseDefInfo()
   {
   dumpOptDetails(comp(), "Perform without use def info\n");
   // Assign an index to each local and parameter symbol.
   //
   TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
   int32_t symRefCount                = comp()->getSymRefCount();

   int32_t i;
   int32_t numSymbols = 1;
   for (i = symRefTab->getIndexOfFirstSymRef(); i < symRefCount; i++)
      {
      TR::SymbolReference *symRef = symRefTab->getSymRef(i);
      if (symRef)
         {
         TR::Symbol *sym = symRef->getSymbol();
         if (sym)
            {
            if (sym->isParm() || sym->isAuto())
               sym->setLocalIndex(numSymbols++);
            else
               sym->setLocalIndex(0);
            }
         }
      }

   _usedSymbols = new(trStackMemory()) TR_BitVector(numSymbols, trMemory(), stackAlloc);

   // Go through the trees and
   //    1) Mark each symbol referenced by a load as used
   //    2) Remember all the stores to each symbol
   //
   vcount_t visitCount = comp()->incVisitCount();
   bool seenMultiplyReferencedParent;
   for (TR::TreeTop *treeTop = comp()->getStartTree(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      _currentTree = treeTop;
      seenMultiplyReferencedParent = false;
      examineNode(treeTop->getNode(), visitCount, seenMultiplyReferencedParent);
      }

   // Remove those stores that were not used when they were encountered in the
   // tree walk but are now used.
   //
   for (i = _storeNodes->size()-1; i >= 0; --i)
      {
      TR::Node *node = _storeNodes->element(i);
      if (node && _usedSymbols->get(node->getSymbolReference()->getSymbol()->getLocalIndex()))
         _storeNodes->element(i) = NULL;
      }

   return 1; // actual cost
   }

void TR_IsolatedStoreElimination::examineNode(TR::Node *node, vcount_t visitCount, bool seenMultiplyReferencedParent)
   {
   if (node->getVisitCount() == visitCount)
      return;
   node->setVisitCount(visitCount);

   if (node->getReferenceCount() > 1)
      seenMultiplyReferencedParent = true;

   // Process the children
   //
   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      {
      examineNode(node->getChild(i), visitCount, seenMultiplyReferencedParent);
      }

   // Now process this node. We are looking for stores or uses of a symbol to
   // which we've assigned a non-zero local index (i.e. a local or a parm)
   //
   if (!node->getOpCode().hasSymbolReference())
      return;
   TR::SymbolReference *symRef = node->getSymbolReference();
   if (symRef == NULL)
      return;
   TR::Symbol *sym = symRef->getSymbol();
   if (!sym || !sym->getLocalIndex())
      return;

   // This is a reference to a symbol we're interested in.
   //
   if (node->getOpCode().isStore())
      {
      if (!_usedSymbols->get(sym->getLocalIndex()))
         {
         // Store to a local or parm that is (as yet) unused
         //
         if (canRemoveStoreNode(node))
            _storeNodes->add(node);
         }
      }
   else
      {
      if (seenMultiplyReferencedParent ||
          (!(_currentTree->getNode()->getOpCode().isStore() &&
             (_currentTree->getNode()->getSymbolReference()->getSymbol() == sym))))
          {
          // Use of a local or parm
          //
          _usedSymbols->set(sym->getLocalIndex());
          }
      //else
      //         printf("Ignoring load in same tree as store in %s\n", signature(comp()->getCurrentMethod()));
      }
   }




#define MAX_TOTAL_NODES  50000
#define MAX_CFG_SIZE     5000

void TR_IsolatedStoreElimination::performDeadStructureRemoval(TR_UseDefInfo *info)
   {
   TR::CFG * cfg = comp()->getFlowGraph();

   if (info->getTotalNodes() > MAX_TOTAL_NODES || cfg->getNumberOfNodes() > MAX_CFG_SIZE)
      return;

   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   vcount_t visitCount = comp()->incVisitCount();
   TR_Structure *rootStructure = comp()->getFlowGraph()->getStructure();
   bool propagateRemovalToParent = false;
   TR_BitVector *nodesInStructure = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
   TR_BitVector *defsInStructure = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);

   findStructuresAndNodesUsedIn(info, rootStructure, visitCount, nodesInStructure, defsInStructure, &propagateRemovalToParent);
   }





bool
TR_IsolatedStoreElimination::findStructuresAndNodesUsedIn(TR_UseDefInfo *info, TR_Structure *structure, vcount_t visitCount, TR_BitVector *nodesInStructure, TR_BitVector *defsInStructure, bool *propagateRemovalToParent)
   {
   bool canRemoveStructure = true;
   int32_t onlyExitEdge = -1;

   if (trace())
      {
      if (structure->asRegion())
         traceMsg(comp(), "Inspecting region structure %d\n", structure->getNumber());
      else
         traceMsg(comp(), "Inspecting block structure %d\n",structure->getNumber());
      }


   if (structure->asRegion())
      {
      TR_RegionStructure *regionStructure = structure->asRegion();

      if (regionStructure->isNaturalLoop() &&
          regionStructure->numSubNodes() == 1 &&
          (regionStructure->getParent() &&
           regionStructure->getParent()->asRegion()->isCanonicalizedLoop()))
         {
         TR::Block *entryBlock = regionStructure->getEntryBlock();

         TR_ScratchList<TR::Block> blocksInLoop(trMemory());
         regionStructure->getBlocks(&blocksInLoop);
         if ((blocksInLoop.getSize() == 1) &&
             debug("deadStructure"))
            analyzeSingleBlockLoop(regionStructure, entryBlock);
         }

      TR_StructureSubGraphNode *subNode;
      TR_Structure             *subStruct = NULL;

      ListIterator<TR::CFGEdge> ei(&regionStructure->getExitEdges());
      for (TR::CFGEdge *edge = ei.getCurrent(); edge != NULL; edge = ei.getNext())
         {
         TR_StructureSubGraphNode *edgeTo = toStructureSubGraphNode(edge->getTo());
         if (onlyExitEdge == -1)
           onlyExitEdge = edgeTo->getNumber();
         else if (onlyExitEdge != edgeTo->getNumber())
            {
            onlyExitEdge = -1;
            break;
            }
         }


      if (onlyExitEdge == -1)
         {
         //try to propagate the removal if this structure to its parent.
         //If the parent can be removed then it doesn't matter how many exit edges there are for this structure.
         *propagateRemovalToParent = true;
         if (regionStructure->getExitEdges().isDoubleton())
            {
            TR::CFGNode *firstNode = regionStructure->getExitEdges().getListHead()->getData()->getTo();

            TR::CFGNode *secondNode = regionStructure->getExitEdges().getLastElement()->getData()->getTo();
            TR::CFG *cfg = comp()->getFlowGraph();
            TR::Block *firstExitBlock = NULL;
            TR::Block *secondExitBlock = NULL;
            for (TR::CFGNode *node = cfg->getFirstNode(); node; node = node->getNext())
               {
               if (node->getNumber() == firstNode->getNumber())
                  firstExitBlock = toBlock(node);
               if (node->getNumber() == secondNode->getNumber())
                  secondExitBlock = toBlock(node);

               if (firstExitBlock && secondExitBlock)
                  break;
               }


            //check if the 2 exists point one to another
            //(i.e. if A,B are the exists and A is a goto block to B).
            //then there is one target to the structure
            if (firstExitBlock->isGotoBlock(comp()) &&
                (firstExitBlock->getSuccessors().front()->getTo()->asBlock()->getNumber()==secondExitBlock->getNumber()))
               onlyExitEdge = secondExitBlock->getNumber();
            else if (secondExitBlock->isGotoBlock(comp()) &&
                (secondExitBlock->getSuccessors().front()->getTo()->asBlock()->getNumber()==firstExitBlock->getNumber()))
               onlyExitEdge = firstExitBlock->getNumber();

            //found one target, no need to propagate
            if (onlyExitEdge != -1)
               *propagateRemovalToParent = false;
            }
         }

      bool subStructureHasSideEffect = false;

      TR_BitVector *nodesInSubStructure = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
      TR_BitVector *defsInSubStructure = new (trStackMemory()) TR_BitVector(0, trMemory(), stackAlloc, growable);
      TR_RegionStructure::Cursor si(*regionStructure);
      for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
         {
         bool toPropagateRemoval = false;
         nodesInSubStructure->empty();
         defsInSubStructure->empty();
         subStruct = subNode->getStructure();
         if (findStructuresAndNodesUsedIn(info, subStruct, visitCount, nodesInSubStructure, defsInSubStructure, &toPropagateRemoval))
            {
            if (!toPropagateRemoval)
               {
               if (trace())
                  traceMsg(comp(),"returned true - subStructureHasSideEffect\n");
               subStructureHasSideEffect = true;
               }
            }

         *nodesInStructure |= *nodesInSubStructure;
         *defsInStructure |= *defsInSubStructure;
         }

      if (subStructureHasSideEffect)
         {
         *propagateRemovalToParent = false;
         if (trace())
            traceMsg(comp(),"end of region %d (hasSideEffect)\n",structure->getNumber());
         return true;
         }
      } //if structure->asRegion()
   else
      {
      TR::Block *nextBlock = structure->asBlock()->getBlock();

      if (!(nextBlock->getSuccessors().size() == 1) ||
          nextBlock->getPredecessors().empty())
         {
         if(trace())
            traceMsg(comp(),"cannot remove structure, pred is empty or succ is not singleton\n");
         canRemoveStructure = false;
         }
      else
         onlyExitEdge = nextBlock->getSuccessors().front()->getTo()->getNumber();

      bool hasSideEffects = false;
      TR::TreeTop *currentTree = nextBlock->getEntry();
      TR::TreeTop *exitTree = nextBlock->getExit();
      //traceMsg(comp(), "Examining block_%d\n", nextBlock->getNumber());
      while (currentTree != exitTree)
         {
         TR::Node *currentNode = currentTree->getNode();
         if (markNodesAndLocateSideEffectIn(currentNode, visitCount, nodesInStructure, defsInStructure))
            hasSideEffects = true;
         currentTree = currentTree->getNextRealTreeTop();
         }

      if (!nextBlock->getExceptionSuccessors().empty() ||
          !nextBlock->getExceptionPredecessors().empty())
         hasSideEffects = true;

      if (hasSideEffects)
         {
         if (trace())
            traceMsg(comp(),"block_%d hasSideEffects\n",nextBlock->getNumber());
         return true;
         }
      }

   if (canRemoveStructure)
      {
      // check for any defs whose uses appear outside this region
      // we cannot remove structure if we find any of these
      TR_BitVectorIterator defs(*defsInStructure);
      while (canRemoveStructure && defs.hasMoreElements())
         {
         int32_t defIndex = defs.getNextElement();
         if (trace())
            traceMsg(comp(), "Checking defIndex %d\n", defIndex);
         TR_UseDefInfo::BitVector uses(comp()->allocator());
         info->getUsesFromDef(uses, defIndex);
         TR_UseDefInfo::BitVector::Cursor useCursor(uses);
         for (useCursor.SetToFirstOne(); canRemoveStructure && useCursor.Valid(); useCursor.SetToNextOne())
            {
            int32_t useIndex = (int32_t)useCursor + info->getFirstUseIndex();
            TR::Node *useNode = info->getNode(useIndex);
            if (useNode && useNode->getReferenceCount() > 0 && !nodesInStructure->get(useNode->getGlobalIndex()))
               {
               if (trace())
                  {
                  if (structure->asRegion())
                     traceMsg(comp(), "Use Node %d invalidates region structure %d - will propagate removal to parent\n", useIndex, structure->getNumber());
                  else
                     traceMsg(comp(), "Use Node %d invalidates block structure %d\n", useIndex, structure->getNumber());
                  }
               canRemoveStructure = false;
               if (structure->asRegion())
                  *propagateRemovalToParent = true;
               }
            }
         }
      // if we can remove the strucutre then these defs are ok here and in all parent structures
      // so we don't need to check them again
      if (canRemoveStructure)
         defsInStructure->empty();
      }


   TR_RegionStructure *regionStructure = structure->asRegion();
   if (canRemoveStructure)
      {
      if (*propagateRemovalToParent)
         return true;

      TR_BlockStructure *loopInvariantBlock = NULL;

      if (regionStructure &&
          regionStructure->isNaturalLoop() &&
          (regionStructure->getParent() /* &&
           regionStructure->getParent()->asRegion()->isCanonicalizedLoop() */))
         {
         TR_RegionStructure *parentStructure = regionStructure->getParent()->asRegion();
         TR_StructureSubGraphNode *subNode = NULL;
         TR_RegionStructure::Cursor si(*parentStructure);
         for (subNode = si.getCurrent(); subNode != NULL; subNode = si.getNext())
            {
            if (subNode->getNumber() == regionStructure->getNumber())
               break;
            }

         if (subNode->getPredecessors().size() == 1)
            {
            TR_StructureSubGraphNode *loopInvariantNode = toStructureSubGraphNode(subNode->getPredecessors().front()->getFrom());
            if (loopInvariantNode->getStructure()->asBlock() /* &&
               loopInvariantNode->getStructure()->asBlock()->isLoopInvariantBlock() */)
               loopInvariantBlock = loopInvariantNode->getStructure()->asBlock();
            }

         if (!loopInvariantBlock)
            return true;

         TR::Block *entryBlock = regionStructure->getEntryBlock();
         TR::TreeTop *loopTestTree = NULL;
         bool isIncreasing = false;

         for (auto nextEdge = entryBlock->getPredecessors().begin(); nextEdge != entryBlock->getPredecessors().end(); ++nextEdge)
            {
            TR::Block *nextBlock = toBlock((*nextEdge)->getFrom());
            if (nextBlock != loopInvariantBlock->getBlock())
               {
               //// Back edge found
               ////
               TR::Node *loopTestNode = nextBlock->getLastRealTreeTop()->getNode();
               if (loopTestNode->getOpCode().isIf())
                  {
                  if ((loopTestNode->getOpCodeValue() == TR::ificmplt) ||
                      (loopTestNode->getOpCodeValue() == TR::iflcmplt) ||
                      (loopTestNode->getOpCodeValue() == TR::ificmple) ||
                      (loopTestNode->getOpCodeValue() == TR::iflcmple))
                     {
                     isIncreasing = true;
                     loopTestTree = nextBlock->getLastRealTreeTop();
                     }
                  else if ((loopTestNode->getOpCodeValue() == TR::ificmpgt) ||
                      (loopTestNode->getOpCodeValue() == TR::iflcmpgt) ||
                      (loopTestNode->getOpCodeValue() == TR::ificmpge) ||
                      (loopTestNode->getOpCodeValue() == TR::iflcmpge))
                     {
                     loopTestTree = nextBlock->getLastRealTreeTop();
                     }
                  }
               else
                  break;
               }
            }

         if (!loopTestTree)
            return true;

         TR::SymbolReference *symRefInCompare = NULL;
         TR::Symbol *symbolInCompare = NULL;
         TR::Node *childInCompare = loopTestTree->getNode()->getFirstChild();
         while (childInCompare->getOpCode().isConversion() || childInCompare->getOpCode().isAdd() || childInCompare->getOpCode().isSub())
            {
            if (childInCompare->getOpCode().isConversion() || childInCompare->getSecondChild()->getOpCode().isLoadConst())
               childInCompare = childInCompare->getFirstChild();
            else
               break;
             }

         if (childInCompare->getOpCode().hasSymbolReference())
            {
            symRefInCompare = childInCompare->getSymbolReference();
            symbolInCompare = symRefInCompare->getSymbol();
            if (!symbolInCompare->isAutoOrParm())
               symbolInCompare = NULL;
            }

         if (symbolInCompare)
            {
            bool foundProperIndVar = false;
            TR_InductionVariable *v;
            for (v = regionStructure->getFirstInductionVariable(); v; v = v->getNext())
               {
               if ((v->getLocal()->getDataType() == TR::Int32) ||  // I have made int checks a bit weaker.. the code below doesn't make sense for unsigned operations // it might make sense to guard this with a test that makes sure that the induction variable is being checked with an signed comparison only
                   (v->getLocal()->getDataType() == TR::Int64))
                  {
                  if ((v->getLocal() == symbolInCompare) &&
                      (((v->getLocal()->getDataType() == TR::Int32) &&
                        (v->getIncr()->getLowInt() == v->getIncr()->getHighInt())) ||
                      ((v->getLocal()->getDataType() == TR::Int64) &&
                       (v->getIncr()->getLowLong() == v->getIncr()->getHighLong()))))
                     {
                     if (isIncreasing)
                        {
                        if (((v->getLocal()->getDataType() == TR::Int32) &&
                             (v->getIncr()->getLowInt() > 0)) ||
                            ((v->getLocal()->getDataType() == TR::Int64) &&
                             (v->getIncr()->getLowLong() > 0)))
                           foundProperIndVar = true;
                        }
                     else
                        {
                        if (((v->getLocal()->getDataType() == TR::Int32) &&
                             (v->getIncr()->getLowInt() < 0)) ||
                            ((v->getLocal()->getDataType() == TR::Int64) &&
                             (v->getIncr()->getLowLong() < 0)))
                           foundProperIndVar = true;
                        }

                     break;
                     }
                  }
               }

            if (!foundProperIndVar)
               return true;
            }
         else
            return true;

         if (!performTransformation(comp(), "%s Removing dead region: %d (%p)\n", optDetailString(), regionStructure->getNumber(), regionStructure))
            return true; // has side effects - cannot be removed.

         if (trace())
            {
            traceMsg(comp(), "Region %d can be removed\n", structure->asRegion()->getNumber());
            printf("Found a removable region in %s\n", comp()->signature());
            }

         TR::CFG *cfg = comp()->getFlowGraph();
         //TR::Block *entryBlock = regionStructure->getEntryBlock();
         TR::Block *targetExitBlock = NULL;
         for (TR::CFGNode *node = cfg->getFirstNode(); node; node = node->getNext())
            {
            if (node->getNumber() == onlyExitEdge)
               {
               targetExitBlock = toBlock(node);
               break;
               }
            }

         for (TR::TreeTop *treeTop = entryBlock->getEntry()->getNextTreeTop(), *next; (treeTop != entryBlock->getExit()); treeTop = next)
            {
            next = treeTop->getNextTreeTop();
            TR::TransformUtil::removeTree(comp(), treeTop);
            if (next == entryBlock->getExit())
               break;
            }

         if (targetExitBlock->getEntry() != entryBlock->getExit()->getNextTreeTop())
            {
            TR::TreeTop *gotoBlockEntryTree = entryBlock->getEntry();
            TR::TreeTop *gotoBlockExitTree = entryBlock->getExit();
            TR::Node *gotoNode =  TR::Node::create(entryBlock->getEntry()->getNode(), TR::Goto);
            TR::TreeTop *gotoTree = TR::TreeTop::create(comp(), gotoNode, NULL, NULL);
            gotoNode->setBranchDestination(targetExitBlock->getEntry());
            gotoBlockEntryTree->join(gotoTree);
            gotoTree->join(gotoBlockExitTree);
            }

         cfg->setStructure(NULL);
         _deferUseDefInfoInvalidation = true;
         optimizer()->setValueNumberInfo(NULL);

         TR::CFGEdge *newEdge = TR::CFGEdge::createEdge(entryBlock,  targetExitBlock, trMemory());
         if (!entryBlock->hasSuccessor(targetExitBlock))
            cfg->addEdge(newEdge);

         for (auto edge = entryBlock->getSuccessors().begin(); edge != entryBlock->getSuccessors().end();)
            {
            if ((*edge) != newEdge)
              cfg->removeEdge(*(edge++));
            else
               ++edge;
            }
         }
      else
         {
           //traceMsg(comp(), "Block %d can be removed\n", structure->asBlock()->getNumber());
           //printf("Found a removable block in %s\n", signature(comp()->getCurrentMethod()));
         }
      }
   else if (regionStructure &&
            !regionStructure->isAcyclic())
      {
      if (trace())
         traceMsg(comp(),"region is not acyclic\n");
      return true;
      }

   return false;
   }

static TR::Node *getLoopIncrementStep(TR::Block *block, TR::SymbolReference *symRef, bool shouldBeSub, int32_t *loopStride)
   {
   TR::TreeTop *exitTree = block->getExit();
   TR::TreeTop *currentTree = block->getEntry();
   TR::Node *loopIncrement = NULL;
   bool seenStore = false;
   while (currentTree != exitTree)
      {
      TR::Node *currentNode = currentTree->getNode();
      if (currentNode->getOpCode().isStoreDirect() && (currentNode->getSymbolReference() == symRef))
         {
         if (seenStore)
            return NULL;

         seenStore = true;
         TR::Node *firstChild = currentNode->getFirstChild();
         if ((firstChild->getOpCode().isAdd() || firstChild->getOpCode().isSub()) &&
             firstChild->getFirstChild()->getOpCode().isLoadVarDirect() &&
             (firstChild->getFirstChild()->getSymbolReference() == symRef) &&
             (firstChild->getSecondChild()->getOpCode().isLoadConst()))
            {
            int32_t value = firstChild->getSecondChild()->getInt();
            if (firstChild->getOpCode().isAdd())
               {
               if (!shouldBeSub && (value > 0))
                  *loopStride = value;
               else if (shouldBeSub)
                  {
                  if (value < 0)
                     *loopStride = -1*value;
                  }
               loopIncrement = currentNode;
               }
            else if (firstChild->getOpCode().isSub())
               {
               if (shouldBeSub && (value > 0))
                  *loopStride = value;
               else if (!shouldBeSub)
                  {
                  if (value < 0)
                     *loopStride = -1*value;
                  }
               loopIncrement = currentNode;
               }
            }
         }
      currentTree = currentTree->getNextRealTreeTop();
      }

   return loopIncrement;
   }

void TR_IsolatedStoreElimination::analyzeSingleBlockLoop(TR_RegionStructure *regionStructure, TR::Block *block)
   {
   TR_InductionVariable *v;
   TR::SymbolReference *inductionSymRef = NULL;
   //TR::Symbol *inductionSym = NULL;

   for (v = regionStructure->getFirstInductionVariable(); v; v = v->getNext())
      {
      TR::SymbolReference *symRef = NULL;
      TR::SymbolReference *inductionSymRef = NULL;
      int32_t symRefCount = comp()->getSymRefCount();
      TR::SymbolReferenceTable *symRefTab = comp()->getSymRefTab();
      int32_t symRefNumber;

      for (symRefNumber = symRefTab->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
         {
         symRef = symRefTab->getSymRef(symRefNumber);
         if (symRef)
            {
            TR::Symbol *sym = symRef->getSymbol();
            if (v->getLocal() == sym)
               {
               inductionSymRef = symRef;
               break;
               }
            }
         }

      if (inductionSymRef)
         break;
      }

   //TR::Block *initializationBlock = NULL;
   TR::Block *loopInvariantBlock = NULL;
   //bool isPredictableLoop = false;
   TR::Node *numIterations = NULL;

   TR::Node *lastTreeInLoopHeader = block->getLastRealTreeTop()->getNode();
   TR::Node *firstChildOfLastTree = NULL;
   TR::Node *secondChildOfLastTree = NULL;

   if (lastTreeInLoopHeader->getNumChildren() > 1)
      {
      firstChildOfLastTree = lastTreeInLoopHeader->getFirstChild();
      secondChildOfLastTree = lastTreeInLoopHeader->getSecondChild();

      if (!inductionSymRef)
         {
         TR::Node *nodeWithSymRef = firstChildOfLastTree;
         if (nodeWithSymRef->getOpCode().hasSymbolReference())
            inductionSymRef = nodeWithSymRef->getSymbolReference();
         else if ((nodeWithSymRef->getNumChildren() > 0) &&
            nodeWithSymRef->getFirstChild()->getOpCode().hasSymbolReference())
         inductionSymRef = nodeWithSymRef->getFirstChild()->getSymbolReference();
         }
      }

   regionStructure->resetInvariance();

   if (((lastTreeInLoopHeader->getOpCodeValue() == TR::ificmple) ||
        (lastTreeInLoopHeader->getOpCodeValue() == TR::ificmplt) ||
        (lastTreeInLoopHeader->getOpCodeValue() == TR::ificmpgt) ||
        (lastTreeInLoopHeader->getOpCodeValue() == TR::ificmpge)) &&
        inductionSymRef &&
        (secondChildOfLastTree->getOpCode().isLoadConst() ||
         regionStructure->isExprInvariant(secondChildOfLastTree)))
      {
      //inductionSym = inductionSymRef->getSymbol();

      //if (inductionSym->isAuto() || inductionSym->isParm())
         {
         if (block->getPredecessors().size() == 2)
            {
            for (auto nextEdge = block->getPredecessors().begin(); nextEdge != block->getPredecessors().end(); ++nextEdge)
               {
               TR::Block *pred = toBlock((*nextEdge)->getFrom());
               if ((pred != block) && (pred->getSuccessors().size() == 1))
                  {
                  if (pred->getStructureOf()->isLoopInvariantBlock())
                     {
                     loopInvariantBlock = pred;
                     }

                  //if (lastRealNode->getOpCode().isStoreDirect() && (lastRealNode->getSymbolReference() == inductionSymRef))
                     {
                        {
                        bool shouldBeSub = false;
                        if ((lastTreeInLoopHeader->getOpCodeValue() == TR::ificmpge) || (lastTreeInLoopHeader->getOpCodeValue() == TR::ificmpgt))
                           shouldBeSub = true;

                        int32_t loopStride = -1;
                        TR::Node *loopIncrement = getLoopIncrementStep(block, inductionSymRef, shouldBeSub, &loopStride);

                        if (loopIncrement &&
                            (loopStride == 1) &&
                            ((loopIncrement->getFirstChild() == firstChildOfLastTree) ||
                             (firstChildOfLastTree->getOpCode().isLoadVarDirect() &&
                              ((firstChildOfLastTree->getSymbolReference()->getReferenceNumber() == inductionSymRef->getReferenceNumber())))))
                           {
                           //initializationBlock = pred;
                           //isPredictableLoop = true;
                           if ((lastTreeInLoopHeader->getOpCodeValue() == TR::ificmpgt) || (lastTreeInLoopHeader->getOpCodeValue() == TR::ificmpge))
                              {
                              numIterations = TR::Node::create(TR::isub, 2, TR::Node::createWithSymRef(secondChildOfLastTree, TR::iload, 0, inductionSymRef), secondChildOfLastTree->duplicateTree());
                              if (lastTreeInLoopHeader->getOpCodeValue() == TR::ificmpge)
                                 numIterations = TR::Node::create(TR::iadd, 2, numIterations, TR::Node::create(secondChildOfLastTree, TR::iconst, 0, 1));
                              }
                           else
                              {
                              numIterations = TR::Node::create(TR::isub, 2,  secondChildOfLastTree->duplicateTree(), TR::Node::createWithSymRef(secondChildOfLastTree, TR::iload, 0, inductionSymRef));
                              if (lastTreeInLoopHeader->getOpCodeValue() == TR::ificmple)
                                 numIterations = TR::Node::create(TR::iadd, 2, numIterations, TR::Node::create(secondChildOfLastTree, TR::iconst, 0, 1));

                              }
                           }
                        }
                     }
                  }
               }
            }
         }

      if (numIterations &&
          loopInvariantBlock)
         {
         TR::TreeTop *treeTop = block->getEntry();
         TR::TreeTop *exit = block->getExit();
         TR::TreeTop *placeHolderTree = loopInvariantBlock->getLastRealTreeTop();
         if (placeHolderTree->getNode()->getOpCode().isBranch())
            placeHolderTree = placeHolderTree->getPrevTreeTop();


         while (treeTop != exit)
            {
            TR::Node *node = treeTop->getNode();
            TR::TreeTop *next = treeTop->getNextTreeTop();

            if (node->getOpCode().isStoreDirect())
               {
               TR::SymbolReference *symRef = node->getSymbolReference();
               TR::Node *rhsOfStoreDefNode = node->getFirstChild();
               if ((rhsOfStoreDefNode->getOpCode().getOpCodeValue() == TR::ixor) &&
                   (rhsOfStoreDefNode->getReferenceCount() == 1) &&
                   rhsOfStoreDefNode->getSecondChild()->getOpCode().isLoadConst() &&
                   (rhsOfStoreDefNode->getSecondChild()->getInt() == 1) &&
                   (rhsOfStoreDefNode->getFirstChild()->getReferenceCount() == 1) &&
                   (rhsOfStoreDefNode->getFirstChild()->getOpCode().hasSymbolReference() &&
                   ((rhsOfStoreDefNode->getFirstChild()->getSymbolReference()->getReferenceNumber() == symRef->getReferenceNumber()))))
                  {
                  TR::Node *remNode = TR::Node::create(TR::irem, 2, numIterations, TR::Node::create(rhsOfStoreDefNode, TR::iconst, 0, 2));
                  remNode->setReferenceCount(1);
                  rhsOfStoreDefNode->getSecondChild()->recursivelyDecReferenceCount();
                  rhsOfStoreDefNode->setChild(1, remNode);

                  rhsOfStoreDefNode->incReferenceCount();
                  node->getFirstChild()->recursivelyDecReferenceCount();
                  node->setChild(0, rhsOfStoreDefNode);

                  TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
                  TR::TreeTop *nextTree = treeTop->getNextTreeTop();
                  prevTree->join(nextTree);
                  TR::TreeTop *placeHolderSucc = placeHolderTree->getNextTreeTop();
                  placeHolderTree->join(treeTop);
                  treeTop->join(placeHolderSucc);
                  placeHolderTree = treeTop;

                  if (trace())
                     {
                     traceMsg(comp(), "treeTop : %p\n", treeTop->getNode());
                     traceMsg(comp(), "PREDICTABLE COMPUTATION : \n");
                     comp()->getDebug()->print(comp()->getOutFile(), treeTop);
                     }
                  }
               }
            treeTop = next;
            }
         }
      }
   }

static bool
nodeHasSideEffect(TR::Node *node)
   {
   // the following are nodes that are
   // actually calls but do not raise exceptions
   // but they do have a side-effect
   //
   switch(node->getOpCodeValue())
      {
      case TR::arrayset:
      case TR::arraycmp:
      case TR::arraytranslate:
      case TR::arraytranslateAndTest:
      case TR::long2String:
      case TR::bitOpMem:
        return true;
      default:
        if (node->getOpCode().isCall() &&
               node->getSymbolReference()->getSymbol()->getResolvedMethodSymbol() &&
               !node->getSymbolReference()->getSymbol()->getResolvedMethodSymbol()->isSideEffectFree())
           return true;
      }
   return false;
   }


bool
TR_IsolatedStoreElimination::markNodesAndLocateSideEffectIn(TR::Node *node, vcount_t visitCount, TR_BitVector *nodesSeen, TR_BitVector *defsSeen)
   {
   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);
   bool toReturn = false;

   if (node->exceptionsRaised() ||
       nodeHasSideEffect(node) ||
       node->getOpCode().isReturn() ||
       node->getOpCode().isLoadReg() ||
       node->getOpCode().isStoreReg() ||
       (node->getOpCode().hasSymbolReference() && node->getSymbolReference()->getSymbol()->isVolatile()) ||
       ((node->getOpCode().isStore() ||
         (node->getOpCode().hasSymbolReference() &&
          node->getSymbolReference()->getSymbol()->isVolatile())) &&
        (node->getSymbolReference()->getSymbol()->isShadow() ||
         node->getSymbolReference()->getSymbol()->isStatic()))
      )
      {
      toReturn = true;
      }
   else if (node->getUseDefIndex())
      {
      nodesSeen->set(node->getUseDefIndex());
      }

   if (node->getOpCode().isLikeDef() && node->getUseDefIndex())
      {
      // we don't need to record defs marked as stored value is irrelevant - there will be no uses of the dead values
      if (!(node->getOpCode().isStoreDirect()
            && node->getSymbolReference()->getSymbol()->isAutoOrParm() && node->storedValueIsIrrelevant()))
         {
         if (trace())
            traceMsg(comp(), "Marking useDefIndex %d as seendef at node n%dn\n", node->getUseDefIndex(), node->getGlobalIndex());
         defsSeen->set(node->getUseDefIndex());
         }
      }

   int32_t childNum;
   for (childNum=0;childNum<node->getNumChildren();childNum++)
      {
      if (markNodesAndLocateSideEffectIn(node->getChild(childNum), visitCount, nodesSeen, defsSeen))
        toReturn = true;
      }

   return toReturn;
   }

const char *
TR_IsolatedStoreElimination::optDetailString() const throw()
   {
   return "O^O ISOLATED STORE ELIMINATION: ";
   }
