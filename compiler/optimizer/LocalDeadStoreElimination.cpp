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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "optimizer/LocalDeadStoreElimination.hpp"

#include <limits.h>                                      // for INT_MAX
#include <stddef.h>                                      // for NULL
#include <stdint.h>                                      // for int32_t, etc
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"                          // for TR_FrontEnd
#include "compile/Compilation.hpp"                       // for Compilation
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"                   // for TR::Options
#include "env/CompilerEnv.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/DataTypes.hpp"                              // for TR::DataType
#include "il/Block.hpp"                                  // for Block
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                                  // for ILOpCode, etc
#include "il/Node.hpp"                                   // for Node, etc
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"                                 // for Symbol
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"                                // for TreeTop
#include "il/TreeTop_inlines.hpp"
#include "il/symbol/AutomaticSymbol.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/BitVector.hpp"
#include "infra/Checklist.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"                       // for Optimizer


TR::Optimization *TR::LocalDeadStoreElimination::create(TR::OptimizationManager *manager)
   {
   return new (manager->allocator()) TR::LocalDeadStoreElimination(manager);
   }

TR::LocalDeadStoreElimination::LocalDeadStoreElimination(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

void
TR::LocalDeadStoreElimination::setIsFirstReferenceToNode(TR::Node *parent, int32_t index, TR::Node *node)
   {
   parent ? node->setLocalIndex(1) : node->setLocalIndex(0);
   }

bool
TR::LocalDeadStoreElimination::isFirstReferenceToNode(TR::Node *parent, int32_t index, TR::Node *node)
   {
   return (node->getLocalIndex() <= 1);
   }

int32_t TR::LocalDeadStoreElimination::perform()
   {
   if (trace())
      traceMsg(comp(), "Starting LocalDeadStoreElimination\n");

   TR::TreeTop *tt, *exitTreeTop;
   for (tt = comp()->getStartTree(); tt; tt = exitTreeTop->getNextTreeTop())
      {
      exitTreeTop = tt->getExtendedBlockExitTreeTop();
      transformBlock(tt, exitTreeTop);
      }

   eliminateDeadObjectInitializations();

   if (_treesChanged)
      requestDeadTreesCleanup();

   if (trace())
      traceMsg(comp(), "\nEnding LocalDeadStoreElimination\n");

   return 1;
   }

int32_t TR::LocalDeadStoreElimination::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      {
      transformBlock(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
      postPerformOnBlocks();
      }
   return 0;
   }

void TR::LocalDeadStoreElimination::prePerformOnBlocks()
   {
   TR::TreeTop *currentTree = comp()->getStartTree();
   comp()->incVisitCount();
   //
   // Local index is set to reference count below
   // As we walk backwards in the main analysis, the local index will keep getting decremented
   // giving us an idea about how many more references exist to a particular node
   //
   while (currentTree)
      {
      setupReferenceCounts(currentTree->getNode());
      currentTree = currentTree->getNextTreeTop();
      }

   comp()->incVisitCount();
   _treesChanged = false;
   }

void TR::LocalDeadStoreElimination::postPerformOnBlocks()
   {
   }

bool TR::LocalDeadStoreElimination::isNonRemovableStore(TR::Node *storeNode, bool &seenIdentityStore)
   {
   TR::Node *currentNode = _curTree->getNode();

   TR::SymbolReference *symRef = storeNode->getSymbolReference();
   int32_t symRefNum = symRef->getReferenceNumber();

   bool nonRemovableStore = false;

   /*
    * Stores that are children of resolve checks cannot be removed, since
    * the check still has to be done.
    */
   if (currentNode->getOpCode().isResolveCheck())
      nonRemovableStore = true;

   // NOTE: The sym->usedInNestedProc() check for LocalDeadStoreElimination is too conservative as the
   // seenIdenticalStore check can legally eliminate even nested symbols
   // i.e.
   // istore "a"  <- even with nested "a" this store can be eliminated due to the redefine of "a" below
   //    x
   // istore "a"
   //    y
   // The usedInNestedProc check is to prevent a auto/parm store to a nested symbol in a nested routine being
   // removed when there is only a return and no uses from the store to the end of the block:
   // istore "b"  // not legal to remove this store as the auto/parm 'escapes' to the parent (owning) routine as they share a stack
   //    z
   // return
   if (storeNode->dontEliminateStores(true))
      nonRemovableStore = true;

   seenIdentityStore = isIdentityStore(storeNode);

   // although it's ok to remove 2 consecutive same stores no matter what
   // eg. pinning array stores from GRA live range splitting
   // except for placeholder stores for monexit, we need to see those in order to pop the live monitor stack the correct number of times
   if (!storeNode->getSymbolReference()->getSymbol()->holdsMonitoredObject())
      {
      TR::Node *prevStoreNode = _curTree->getPrevTreeTop()->getNode()->getStoreNode();
      if (prevStoreNode &&
            !storeNode->getOpCode().isIndirect() && !prevStoreNode->getOpCode().isIndirect() &&
            storeNode->getFirstChild() == prevStoreNode->getFirstChild() &&
            storeNode->getSymbolReference() == prevStoreNode->getSymbolReference())
         {
         seenIdentityStore = true;
         nonRemovableStore = false;
         }
      }

   return nonRemovableStore;
   }

void TR::LocalDeadStoreElimination::transformBlock(TR::TreeTop * entryTree, TR::TreeTop * exitTree)
   {
   _curTree = entryTree;
   _blockContainsReturn = false;
   _treesAnchored = false;

   comp()->setCurrentBlock(entryTree->getEnclosingBlock());

   /**
    * Count the number of stores to allocate the right size data structures
    */
   int32_t numStores = 0;
   while (!(_curTree == exitTree))
      {

      if (_curTree->getNode()->getStoreNode())
         numStores++;
      _curTree = _curTree->getNextTreeTop();
      }

   StoreNodeTable storeNodeTable(0, comp()->trMemory()->currentStackRegion());
   TR_BitVector deadSymbolReferences(comp()->trMemory()->currentStackRegion());
   _storeNodes = &storeNodeTable;

   int32_t symRefCount = comp()->getSymRefCount();

   comp()->incOrResetVisitCount();
   int32_t i;
   TR::Node *currentNode;
   _curTree = exitTree;
   while (!(_curTree == entryTree))
      {
      currentNode = _curTree->getNode();
      if (comp()->getVisitCount() == MAX_VCOUNT - 2) // Inc visit count will assert at MAX_VCOUNT - 1, so this gives us
                                                     // the one extra incVisitCount that might occur if we remove a store tree.
         {
         if (trace())
            traceMsg(comp(), "Bailing out of local deadstore to avoid visit count overflow\n");
         break;
         }

      /*
       * Special case for when the last instruction in the block is a return.
       * In this case we can simply get rid of any trailing stores to local
       * variables in this block as they cannot be accessed outside
       * this method.
       */
      if (currentNode->getOpCode().isReturn())
        {
          _blockContainsReturn = true;
          for (int32_t symRefNumber = comp()->getSymRefTab()->getIndexOfFirstSymRef(); symRefNumber < symRefCount; symRefNumber++)
            {
              TR::SymbolReference *symRef = comp()->getSymRefTab()->getSymRef(symRefNumber);
              TR::Symbol *sym = symRef ? symRef->getSymbol() : 0;
              if (sym && (sym->isAuto() || sym->isParm())) {
                deadSymbolReferences.set(symRefNumber);
              }
            }
        }

      bool removedTree = false;
      TR::Node *storeNode = currentNode->getStoreNode();
      if (storeNode)
         {
         bool seenIdentityStore = isIdentityStore(storeNode);
         bool nonRemovableStore = isNonRemovableStore(storeNode, seenIdentityStore);

         TR::SymbolReference *symRef = storeNode->getSymbolReference();
         int32_t symRefNum = symRef->getReferenceNumber();

         // the logic below is:
         // when nonRemovableStore=false then removal of the store can happen if one of the following conditions is true:
         // 1) seenIdentityStore=true
         // 2) haven't seen a use and not volatile and one of the following is also true
         //    a) the block contains a return and not nested and sym is an auto or parm (i.e. val doesn't escape)
         //    b) an identical store has been seen
            if (!nonRemovableStore &&
                (seenIdentityStore ||                                 // 1)
                 (deadSymbolReferences.get(symRefNum) &&          // 2)
                  !symRef->getSymbol()->isVolatile()  &&
                  ((_blockContainsReturn              &&              //   a)
                    (symRef->getSymbol()->isAuto() ||
                     symRef->getSymbol()->isParm())) ||
                   seenIdenticalStore(storeNode)))))                 //   b)
            {
            removedTree = true;
            _curTree = removeStoreTree(_curTree);
            }
         else
            {
            adjustStoresInfo(storeNode, deadSymbolReferences);
            }
         }
      else if (currentNode->getNumChildren() > 0 &&
               (currentNode->getFirstChild()->getOpCode().isCall() ||
                currentNode->getFirstChild()->getOpCode().isStore()))
         {
         adjustStoresInfo(currentNode->getFirstChild(), deadSymbolReferences);
         }

      // Change made to accommodate branches in mid (extended) basic blocks
      // Cannot remove stores based on a store that appears after a branch
      //
      if (currentNode->getOpCode().isBranch() ||
          currentNode->getOpCode().isJumpWithMultipleTargets() ||
          (currentNode->getOpCodeValue() == TR::treetop && currentNode->getFirstChild()->getOpCode().isJumpWithMultipleTargets()))
         {
         _blockContainsReturn = false;
         deadSymbolReferences.empty();
         _storeNodes->clear();
         }

      if (!removedTree)
         {
         comp()->incVisitCount();
         examineNode(NULL, 0, currentNode, deadSymbolReferences);
         }

      _curTree = _curTree->getPrevTreeTop();
      } // end while (!(_curTree == entryTree))

   if (_treesAnchored)
      {
      _curTree = entryTree;
      while (!(_curTree == exitTree))
         {
         currentNode = _curTree->getNode();
         TR::Node *firstChild = currentNode->getOpCodeValue() == TR::treetop ? currentNode->getFirstChild() : 0;

         bool callWithSideEffects=false;
         if(firstChild && firstChild->getOpCode().isCall())
            {
            callWithSideEffects=true;
            if (firstChild->getSymbolReference()->getSymbol()->isResolvedMethod() &&
                firstChild->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->isSideEffectFree())
               callWithSideEffects = false;
            }


         if (firstChild &&
             !(callWithSideEffects ||
               firstChild->getOpCodeValue() == TR::New ||
               firstChild->getOpCodeValue() == TR::newarray ||
               firstChild->getOpCodeValue() == TR::anewarray ||
               firstChild->getOpCodeValue() == TR::multianewarray ||
               firstChild->getOpCodeValue() == TR::MergeNew ||
               ///firstChild->getOpCodeValue() == TR::checkcast ||
               firstChild->getOpCode().isCheckCast() ||
               firstChild->getOpCodeValue() == TR::instanceof ||
               firstChild->getOpCodeValue() == TR::monent ||
               firstChild->getOpCodeValue() == TR::monexit))
            {
            if (firstChild->getReferenceCount() == 0 &&
                performTransformation(comp(), "%sRemoving Dead Anchor : %s [0x%p]\n", optDetailString(), currentNode->getOpCode().getName(), currentNode))
               {
               _treesChanged = true;
               optimizer()->prepareForTreeRemoval(_curTree);

               TR::TreeTop *nextTree = _curTree->getNextTreeTop();
               TR::TreeTop *prevTree = _curTree->getPrevTreeTop();
               prevTree->setNextTreeTop(nextTree);
               nextTree->setPrevTreeTop(prevTree);
               _curTree = nextTree;
               continue;
               }
            }
            _curTree = _curTree->getNextTreeTop();
         }
      }
   }

TR::TreeTop *TR::LocalDeadStoreElimination::removeStoreTree(TR::TreeTop *treeTop)
   {
   // If all the conditions for a store are met then
   // get rid of the store operation
   //
   _treesChanged = true;
   comp()->incVisitCount();

   TR::Node *nodeInTreeTop = treeTop->getNode();
   TR::Node *storeNode = nodeInTreeTop->getStoreNode();

   if ((storeNode != nodeInTreeTop) && (nodeInTreeTop->getOpCodeValue() == TR::NULLCHK))
      {
      TR::TreeTop *nullCheckTree = TR::TreeTop::create(comp(), nodeInTreeTop);
      TR::Node *referenceNode = nodeInTreeTop->getNullCheckReference();
      TR::Node *passThroughNode = TR::Node::create(TR::PassThrough, 1, referenceNode);
      nullCheckTree->getNode()->setChild(0, passThroughNode);
      nullCheckTree->getNode()->setReferenceCount(0);
      nullCheckTree->getNode()->setNumChildren(1);
      passThroughNode->setReferenceCount(1);
      setIsFirstReferenceToNode(NULL, 0, nullCheckTree->getNode());
      setIsFirstReferenceToNode(nullCheckTree->getNode(), 0, passThroughNode);
      treeTop->getPrevTreeTop()->join(nullCheckTree);
      nullCheckTree->join(treeTop);
      }
   else if ((storeNode != nodeInTreeTop) && (nodeInTreeTop->getOpCodeValue() == TR::BNDCHKwithSpineCHK))
      {
      TR::TreeTop *spineCheckTree = TR::TreeTop::create(comp(), nodeInTreeTop);
      TR::Node *arrayAccessNode = nodeInTreeTop->getFirstChild();
      TR::Node *replacementNode = TR::Node::createConstZeroValue(storeNode, storeNode->getDataType());
      spineCheckTree->getNode()->setAndIncChild(0, replacementNode);
      spineCheckTree->getNode()->setReferenceCount(0);
      setIsFirstReferenceToNode(NULL, 0, spineCheckTree->getNode());
      setIsFirstReferenceToNode(spineCheckTree->getNode(), 0, replacementNode);
      treeTop->getPrevTreeTop()->join(spineCheckTree);
      spineCheckTree->join(treeTop);
      }

   // Entire node may not be removable as nodes may have values
   // that would change if the node is allowed to swing down to its
   // next use (if any symbols in the value are written in the
   // intervening treetops).
   //
   if (isEntireNodeRemovable(storeNode))
      {
      if (performTransformation(comp(), "%sRemoving Dead Store : %s [0x%p]\n", optDetailString(), storeNode->getOpCode().getName(), storeNode))
         {
         storeNode->setReferenceCount(1);
         optimizer()->prepareForNodeRemoval(storeNode);
         storeNode->recursivelyDecReferenceCount();
         TR::TreeTop *nextTree = treeTop->getNextTreeTop();
         TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
         prevTree->setNextTreeTop(nextTree);
         nextTree->setPrevTreeTop(prevTree);
         treeTop = nextTree;
         }
      }
   else
      {
      if (performTransformation(comp(), "%sAnchoring rhs of store : %s [0x%p] in a treetop\n", optDetailString(), storeNode->getOpCode().getName(), storeNode))
         {
         bool removedTree = false;
         TR::TreeTop *curTree = NULL;

         // if the store is dead, remove the corresponding compressRefs node as well
         //
         if (comp()->useAnchors())
            {
            // FIXME: perhaps need to walk down the extended block
            // looking for the corresponding anchor node
            //
            curTree = treeTop->getNextTreeTop();
            TR::Node *translateNode = NULL;
            for (; curTree->getNode()->getOpCodeValue() != TR::BBEnd; curTree = curTree->getNextTreeTop())
               if (curTree->getNode()->getOpCode().isAnchor() &&
                     curTree->getNode()->getFirstChild() == storeNode)
                  {
                  translateNode = curTree->getNode();
                  break;
                  }

            if (translateNode)
               {
               dumpOptDetails(comp(), "removing corresponding translation [%p] for [%p]\n", translateNode, storeNode);
               if (translateNode->getFirstChild()->getReferenceCount() > 1)
                  {
                  removedTree = true;
                  translateNode->recursivelyDecReferenceCount();
                  //translateNode->getFirstChild()->decReferenceCount();
                  TR::TreeTop *nextTree = curTree->getNextTreeTop();
                  TR::TreeTop *previousTree = curTree->getPrevTreeTop();
                  previousTree->setNextTreeTop(nextTree);
                  nextTree->setPrevTreeTop(previousTree);
                  }
               else
                  {
                  translateNode->decReferenceCount();
                  translateNode->getSecondChild()->decReferenceCount();
                  curTree->setNode(storeNode);
                  }
               }
            }

         TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
         TR::NodeChecklist visited(comp());

         for (int32_t j=0;j<storeNode->getNumChildren();j++)
            {
            getAnchorNode(storeNode, j, storeNode->getChild(j), treeTop, visited);
            }

         optimizer()->prepareForNodeRemoval(storeNode);
         _treesAnchored = true;
         TR::TreeTop *nextTree = treeTop->getNextTreeTop();

          if ((curTree != treeTop) ||
              !removedTree)
             {
             if (nodeInTreeTop->getOpCode().isAnchor()||
                (nodeInTreeTop->getOpCode().isCheck() &&
                 !nodeInTreeTop->getOpCode().isNullCheck()))
                 treeTop->getNode()->recursivelyDecReferenceCount();
             else
                storeNode->recursivelyDecReferenceCount();
             TR::TreeTop *previousTree = treeTop->getPrevTreeTop();
             previousTree->setNextTreeTop(nextTree);
             nextTree->setPrevTreeTop(previousTree);
             //treeTop = prevTree->getNextTreeTop();
             }

         treeTop = nextTree;
         }
      }
   return treeTop;
   }

// Look for identity stores, i.e. stores whose value to be stored is a load of
// the same object.
//
bool TR::LocalDeadStoreElimination::isIdentityStore(TR::Node *storeNode)
   {
   // Now see if this is an identity store. If it is there are two cases:
   //    1) This is the first use of the load node - the store can be removed
   //       immediately.
   //    2) There is a previous use of the load node - the store must be added
   //       to the list of pending stores until we can prove there is no
   //       intervening store that kills the load.
   //
   TR::Node *storedValue = NULL;
   int32_t storedValueChildIndex = -1;
   if (storeNode->getOpCode().isIndirect())
      {
      storedValueChildIndex = 1;
      storedValue = storeNode->getSecondChild();
      }
   else
      {
      storedValueChildIndex = 0;
      storedValue = storeNode->getFirstChild();
      }

   if (!storedValue->getOpCode().hasSymbolReference())
      return false;
   if (storeNode->getSymbolReference() == NULL)
      return false;
   if (storedValue->getSymbolReference() == NULL)
      return false;
   if (storedValue->getSymbol() != storeNode->getSymbol())
      return false;
   if ((storeNode->getOpCode().isIndirect() && !storedValue->getOpCode().isIndirect()) ||
       (!storeNode->getOpCode().isIndirect() && storedValue->getOpCode().isIndirect()))
      return false;
   if (storedValue->getSymbol()->isVolatile())
      return false;
   if (!storedValue->getOpCode().isLoadVar())
      return false;
   if (storeNode->getOpCode().isIndirect() &&
       storeNode->getFirstChild() != storedValue->getFirstChild())
      return false;
   if (storeNode->getSymbolReference()->getOffset() != storedValue->getSymbolReference()->getOffset())
      return false;
#ifdef J9_PROJECT_SPECIFIC
   if (storeNode->getType().isBCD() && !storeNode->isDecimalSizeAndShapeEquivalent(storedValue))
      return false;
#endif

   // This is an identity store
   //
   if ((storedValue->getReferenceCount() == 1) || isFirstReferenceToNode(storeNode, storedValueChildIndex, storedValue))
      return true;

   bool doChecksForAnchors = false;
   TR::ILOpCodes anchorOp = TR::BadILOp;
   if (comp()->useCompressedPointers() &&
         (storedValue->getOpCodeValue() == TR::aloadi) &&
         storedValue->getReferenceCount() == 2)
      {
      doChecksForAnchors = true;
      anchorOp = TR::compressedRefs;
      }

   if (doChecksForAnchors)
      {
      // search the prev treetop for the anchor, give up if not found
      //
      // compressedRefs
      //    load
      // store
      //  ==> load
      //
      TR::TreeTop *prevTree = _curTree->getPrevTreeTop();
      if ((prevTree->getNode()->getOpCodeValue() == anchorOp) &&
            (prevTree->getNode()->getFirstChild() == storedValue))
         return true;
      }

   return false;
   }

void TR::LocalDeadStoreElimination::examineNode(TR::Node *parent, int32_t childNum, TR::Node *node, TR_BitVector &deadSymbolReferences)
   {
   if (!isFirstReferenceToNode(parent, childNum, node))
      {
      node->setLocalIndex(node->getLocalIndex()-1);
      return;
      }

   // Process children
   //
   for (int32_t i = 0 ; i < node->getNumChildren(); i++)
      {
      examineNode(node, i, node->getChild(i), deadSymbolReferences);
      }

   if (!node->getOpCode().hasSymbolReference())
      return;
   TR::SymbolReference *symRef = node->getSymbolReference();

   if (node->getOpCode().isLoadVar() ||
      (node->getOpCodeValue() == TR::loadaddr))
      {
      deadSymbolReferences.reset(symRef->getReferenceNumber());
      if (symRef->sharesSymbol())
         {
         symRef->getUseDefAliases().getAliasesAndSubtractFrom(deadSymbolReferences);
         }

      killStoreNodes(node);
      }


   if (node->getOpCode().isCall() ||
       node->getOpCode().isCheck() ||
       node->getOpCodeValue() == TR::New ||
       node->getOpCodeValue() == TR::newarray ||
       node->getOpCodeValue() == TR::anewarray ||
       node->getOpCodeValue() == TR::multianewarray ||
       ///node->getOpCodeValue() == TR::checkcast ||
       node->getOpCode().isCheckCast() ||
       node->getOpCodeValue() == TR::instanceof ||
       node->getOpCodeValue() == TR::monent ||
       node->getOpCodeValue() == TR::monexit ||
       node->mightHaveVolatileSymbolReference())
      {
      int32_t symRefNum = symRef->getReferenceNumber();
      deadSymbolReferences.reset(symRefNum);
      symRef->getUseonlyAliases().getAliasesAndSubtractFrom(deadSymbolReferences);
      killStoreNodes(node);
      bool isCallDirect = node->getOpCode().isCallDirect();
      if (symRef->sharesSymbol())
         {
         symRef->getUseDefAliases(isCallDirect).getAliasesAndSubtractFrom(deadSymbolReferences);
         }
      }
   }

// Simple check to see if a node is removable or not
// Only checks the reference counts of the children (> 1 or not)
// An enhancement must be made here to check if the reference count > 1
// actually can have side effects; i.e. check if it can be allowed
// to swing down to its next use regardless. Thats more expensive and
// I have added a later cleanup pass in LocalOpts that would in fact
// clean up such anchored values created by any opt in general
//
// enhancement made to see if reference counts of children arise out of this node only
// count of edges will equal the accumulated reference count
// using externalReferenceCount = (accumulated reference count) - (edge count traversing tree from this edge)
// node is removable if externalReferenceCount is zero
//
bool TR::LocalDeadStoreElimination::isEntireNodeRemovable(TR::Node *storeNode)
   {
   if (storeNode->getReferenceCount() > 1)
      return false;

   TR::Node *storeValue = storeNode->getFirstChild();
   rcount_t externalReferenceCount = 0;
   setExternalReferenceCountToTree(storeValue, &externalReferenceCount);

   return (externalReferenceCount == 0);
   }

void TR::LocalDeadStoreElimination::setExternalReferenceCountToTree(TR::Node *node, rcount_t *externalReferenceCount)
   {
   vcount_t visitCount = comp()->getVisitCount();

   // accumulate edge counts
   (*externalReferenceCount)--;

   if (visitCount == node->getVisitCount())
      return;

   node->setVisitCount(visitCount);

   // accumulate reference counts
   (*externalReferenceCount) += node->getReferenceCount();
   // Process children
   //
   for (int32_t i = 0 ; i < node->getNumChildren(); i++)
      setExternalReferenceCountToTree(node->getChild(i), externalReferenceCount);

   return;
   }

bool TR::LocalDeadStoreElimination::seenIdenticalStore(TR::Node *node)
   {
   for (auto it = _storeNodes->rbegin(); it != _storeNodes->rend(); ++it) {
      TR::Node *storeNode = *it;
      if (!storeNode) continue;

      // the same store node could be commoned in a previous tree
      // e.g.
      // store
      // treetop
      //   ==> store
      // or
      // store
      // compressedRefs
      //   ==> store
      // or
      // store
      // fieldAccess
      //   ==> store
      // in this case, the store is not dead and cannot be removed
      //
      if (storeNode == node)
         {
         if (trace())
            traceMsg(comp(), "seenIdentical nodes %p and %p\n", node, storeNode);
         return false;
         }
      if (areLhsOfStoresSyntacticallyEquivalent(storeNode, node))
         return true;
      else if ((node->getSymbolReference()->getReferenceNumber() == storeNode->getSymbolReference()->getReferenceNumber()))
         return false;
      }

   return false;
   }

bool TR::LocalDeadStoreElimination::areLhsOfStoresSyntacticallyEquivalent(TR::Node *node1, TR::Node *node2)
   {
   int32_t childAdjust;
   int32_t firstNonCheckChildOfFirstNode = 0;
   if (node1->getNumChildren() > 0)
      {
      childAdjust = (node1->getOpCode().isWrtBar()) ? 2 : 1;
      firstNonCheckChildOfFirstNode = node1->getNumChildren() - childAdjust;
      }

   int32_t firstNonCheckChildOfSecondNode = 0;
   if (node2->getNumChildren() > 0)
      {
      childAdjust = (node2->getOpCode().isWrtBar()) ? 2 : 1;
      firstNonCheckChildOfSecondNode = node2->getNumChildren() - childAdjust;
      }

   if (!(firstNonCheckChildOfFirstNode == firstNonCheckChildOfSecondNode))
      return false;

   TR::ILOpCode &opCode1 = node1->getOpCode();
   if (opCode1.hasSymbolReference())
      {
      if (!((node1->getOpCodeValue() == node2->getOpCodeValue()
             && node1->getSymbolReference()->getReferenceNumber() == node2->getSymbolReference()->getReferenceNumber())))
         return false;
      }

   for (int32_t i = 0; i < firstNonCheckChildOfFirstNode; i++)
      {
      if (node1->getChild(i) != node2->getChild(i))
         return false;
      }

   return true;
   }

void TR::LocalDeadStoreElimination::adjustStoresInfo(TR::Node *node, TR_BitVector &deadSymbolReferences)
   {
   // Gen the information for this node if it is a store; mark the symbol
   // being stored into as unused so that any dead store encountered above
   // can be removed if it does not feed a use.
   //
   if (node->getOpCode().isStore())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *sym = symRef->getSymbol();
      deadSymbolReferences.set(symRef->getReferenceNumber());
      _storeNodes->push_back(node);
      }
   else if (node->getOpCode().isCall() ||
            node->getOpCodeValue() == TR::monent ||
            node->getOpCodeValue() == TR::monexit ||
            (node->isGCSafePointWithSymRef() && comp()->getOptions()->realTimeGC()) ||
            node->mightHaveVolatileSymbolReference())
      {
      bool isCallDirect = false;
      if (node->getOpCode().isCallDirect())
         isCallDirect = true;

      if (node->getSymbolReference()->sharesSymbol())
         {
         node->getSymbolReference()->getUseDefAliases(isCallDirect).getAliasesAndSubtractFrom(deadSymbolReferences);
         }

      killStoreNodes(node);
      }
   }


// Create treetops for each of the nodes having reference counts > 0
// and add them in to the list of treetops
//
TR::Node* TR::LocalDeadStoreElimination::getAnchorNode(TR::Node *parentNode, int32_t nodeIndex, TR::Node *node, TR::TreeTop *treeTop, TR::NodeChecklist& visited)
   {
   if (!visited.contains(node))
      visited.add(node);

   if (node->getReferenceCount() > 1)
      {
      TR::TreeTop *prevTree = treeTop->getPrevTreeTop();
      TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp(), TR::Node::create(node, TR::treetop, 1));
      anchorTreeTop->getNode()->setAndIncChild(0, node);
      setIsFirstReferenceToNode(NULL, 0, anchorTreeTop->getNode());
      // first refs may be tracked by parent nodes so we must transfer the first ref flag from
      // the original parent (parentNode) to the newly created treetop parent node
      if (isFirstReferenceToNode(parentNode, nodeIndex, node))
         setIsFirstReferenceToNode(anchorTreeTop->getNode(), 0, node);
      anchorTreeTop->join(treeTop);
      prevTree->join(anchorTreeTop);
      return node;
      }
   else
      {
      // TR_FirstNodeReferenceTracker (when used) will mark the earliest reference of a tree commoned node (> 1 ref in same tree)
      // as the first overall reference therefore visit children nodes from first (0) to last (node->getNumChildren()-1)
      // so the first reference flag is propagated correctly with setIsFirstReferenceToNode in the code just above.
      // When not using TR_FirstNodeReferenceTracker the future use count is decremented below to handle this case.
      // NOTE: TR_FirstNodeReferenceTracker is now permanently disabled.
      for (int i = 0; i < node->getNumChildren(); i++)   // visit from 0->numChildren-1 see above comment
         {
         TR::Node *child = node->getChild(i);
         if (!visited.contains(child))
            {
            getAnchorNode(node, i, child, treeTop, visited);
            }
         else
            {
            //  child->decReferenceCount();
            if (child->getLocalIndex() > 1)
               {
               // A tree commoned node (> 1 ref in same tree) must have its future use count decremented here
               // so the anchored node previously created in this function will be correctly recognized as a first
               // reference in examineNode()
               child->decLocalIndex();
               }
            }
         }
      }

   return NULL;
   }

void TR::LocalDeadStoreElimination::setupReferenceCounts(TR::Node *node)
   {
   node->setVisitCount(comp()->getVisitCount());
   node->setLocalIndex(node->getReferenceCount());

   for (int i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (child->getVisitCount() != comp()->getVisitCount())
         {
         setupReferenceCounts(child);
         }
      }
   }

void TR::LocalDeadStoreElimination::eliminateDeadObjectInitializations()
   {
   //
   // Assign an index to each local allocation symbol.
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
            if (sym->isLocalObject() &&
                sym->getLocalObjectSymbol()->getKind() == TR::New)
               sym->setLocalIndex(numSymbols++);
            else
               sym->setLocalIndex(0);
            }
         }
      }

   //if (numSymbols == 1)
   //  return;

   LDSBitVector usedLocalObjectSymbols(comp()->trMemory()->currentStackRegion());

   TR::TreeTop *tt;
   // Go through the trees and find locally allocated objects
   // used only for initialization
   //
   vcount_t visitCount = comp()->incVisitCount();
   for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      findLocallyAllocatedObjectUses(usedLocalObjectSymbols, NULL, -1, tt->getNode(), visitCount);

   visitCount = comp()->incVisitCount();
   //TR::Node *currentNew = NULL;
   TR_ScratchList<TR::Node> currentNews(trMemory()), removedNews(trMemory());
   for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      TR::Node *storeNode = node->getStoreNode();
      bool removableZeroStore = false;
      bool removableLocalObjectStore = false;
      TR::Node *currentNewNode = NULL;

      if (storeNode &&
          storeNode->getOpCode().isStoreIndirect() &&
          !storeNode->getSymbolReference()->isUnresolved())
         {
         if (storeNode->getFirstChild()->getOpCode().hasSymbolReference() &&
             storeNode->getFirstChild()->getSymbolReference()->getSymbol()->isLocalObject() &&
             (storeNode->getFirstChild()->getSymbolReference()->getSymbol()->getLocalObjectSymbol()->getKind() == TR::New) &&
             !usedLocalObjectSymbols.get(storeNode->getFirstChild()->getSymbolReference()->getSymbol()->getLocalIndex()))
            removableLocalObjectStore = true;
         else if (currentNews.find(storeNode->getFirstChild()) ||
                  (storeNode->getFirstChild()->getOpCode().isArrayRef() &&
                   currentNews.find(storeNode->getFirstChild()->getFirstChild()) &&
                   storeNode->getFirstChild()->getSecondChild()->getOpCode().isLoadConst()))
            {
            currentNewNode = storeNode->getFirstChild();
            if (currentNewNode->getOpCode().isArrayRef())
                currentNewNode = currentNewNode->getFirstChild();

            TR::Node *valueNode = storeNode->getSecondChild();

            if(valueNode->isConstZeroValue())
               {
               //dumpOptDetails(comp(), "Removable zero store %p\n", storeNode);
               removableZeroStore = true;
               }
            else
              {
              //dumpOptDetails(comp(), "Removing new %p because of cause node %p\n", currentNewNode, node);
              currentNews.remove(currentNewNode);
              if (!removedNews.find(currentNewNode))
                removedNews.add(currentNewNode);
              }
            }
         }

      if (examineNewUsesForKill(node, storeNode, &currentNews, &removedNews, NULL, -1, visitCount))
         {
         if (!removableZeroStore)
            {
            currentNews.remove(currentNewNode);
            if (!removedNews.find(currentNewNode))
                removedNews.add(currentNewNode);
            //dumpOptDetails(comp(), "Removing new %p because of cause node %p\n", currentNewNode, node);
            }
         }


       if (removableZeroStore)
          {
          TR::SymbolReference *symRef = currentNewNode->getSymbolReference();
          TR_ExtraInfoForNew *initInfo = symRef->getExtraInfo();
          if (initInfo)
             {
             if (initInfo->zeroInitSlots ||
                 (initInfo->numZeroInitSlots > 0))
                {
                int32_t offset = -1;
                if (storeNode->getFirstChild()->getOpCode().isArrayRef())
                   {
                   if (TR::Compiler->target.is64Bit())
                      {
                      if (storeNode->getFirstChild()->getSecondChild()->getLongInt() > INT_MAX)
                         removableZeroStore = false;
                      else
                         offset = storeNode->getSymbolReference()->getOffset() +
                                  (int32_t)storeNode->getFirstChild()->getSecondChild()->getLongInt() -
                                  TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
                      }
                   else
                      offset = storeNode->getSymbolReference()->getOffset() +
                               storeNode->getFirstChild()->getSecondChild()->getInt() -
                               TR::Compiler->om.contiguousArrayHeaderSizeInBytes();
                   }
                else
                   offset = storeNode->getSymbolReference()->getOffset() - fe()->getObjectHeaderSizeInBytes();

                if (removableZeroStore)
                   {
                   if (initInfo->zeroInitSlots &&
                       !initInfo->zeroInitSlots->get(offset/4))
                      {
                      initInfo->zeroInitSlots->set(offset/4);
                      initInfo->numZeroInitSlots++;

                      if (storeNode->getOpCode().getSize() > 4)
                         {
                         initInfo->zeroInitSlots->set((offset/4)+1);
                         initInfo->numZeroInitSlots++;
                         }
                      }
                   }
                }
             else
                removableZeroStore = false;
             }
          else
             removableZeroStore = false;
          }


      if ((removableZeroStore || removableLocalObjectStore) &&
          performTransformation(comp(), "%sRemoving Dead Store : %s [0x%p]\n", optDetailString(), tt->getNode()->getOpCode().getName(), tt->getNode()))
         {
         tt->getNode()->recursivelyDecReferenceCount();
         tt->getPrevTreeTop()->join(tt->getNextTreeTop());
         }
      }
   }

void TR::LocalDeadStoreElimination::findLocallyAllocatedObjectUses(LDSBitVector &usedLocalObjectSymbols, TR::Node *parent, int32_t childNum, TR::Node *node, vcount_t visitCount)
   {
   if (node->getOpCode().hasSymbolReference() &&
       (node->getSymbolReference()->getSymbol()->isLocalObject() &&
        node->getSymbolReference()->getSymbol()->getLocalObjectSymbol()->getKind() == TR::New) &&
       !(parent->getOpCode().isStoreIndirect() && childNum == 0 &&
         ( (uint32_t) parent->getSymbolReference()->getOffset() < fe()->getObjectHeaderSizeInBytes())))
       usedLocalObjectSymbols.set(node->getSymbolReference()->getSymbol()->getLocalIndex());

   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   // Process the children
   //
   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      findLocallyAllocatedObjectUses(usedLocalObjectSymbols, node, i, node->getChild(i), visitCount);
   }

bool TR::LocalDeadStoreElimination::examineNewUsesForKill(TR::Node *node, TR::Node *storeNode, List<TR::Node> *currentNews, List<TR::Node> *removedNews, TR::Node *parent, int32_t childNum, vcount_t visitCount)
   {
   TR::Node *newNode = NULL;
   TR::Node *origNode = node;
   if (node->getOpCode().isArrayRef())
      node = node->getFirstChild();

   if (node->getOpCodeValue() == TR::New ||
        node->getOpCodeValue() == TR::newarray ||
        node->getOpCodeValue() == TR::anewarray)
     newNode = node;

   node = origNode;

   if (currentNews->find(newNode) &&
       ((((parent->getOpCode().isIndirect() ||
        parent->getOpCode().isArrayLength())) ||
        parent->getOpCode().isCall())))
       {
       if (trace())
          traceMsg(comp(), "going to remove new %p at node %p\n", newNode, node);
       if ((childNum == 0) && (storeNode == parent))
          return true;
       else
          {
          if (trace())
             traceMsg(comp(), "removing new %p at node %p\n", newNode, node);
          currentNews->remove(newNode);
          if (!removedNews->find(newNode))
             removedNews->add(newNode);
          //dumpOptDetails(comp(), "Removing new %p because of cause node %p\n", newNode, node);
          }
       }

   if (node->getVisitCount() == visitCount)
      return false;

   node->setVisitCount(visitCount);

   if (newNode &&
       !removedNews->find(newNode))
      {
      currentNews->add(newNode);
      //dumpOptDetails(comp(), "Adding new %p because of cause node %p\n", newNode, node);
      }

   bool result = false;
   // Process the children
   //
   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      {
      if (examineNewUsesForKill(node->getChild(i), storeNode, currentNews, removedNews, node, i, visitCount))
        result = true;
      }
   return result;
   }

void TR::LocalDeadStoreElimination::killStoreNodes(TR::Node *node)
   {
   for (auto it = _storeNodes->begin(); it != _storeNodes->end(); ++it)
      {
      TR::Node *storeNode = *it;

      if (storeNode && node->getSymbolReference()->sharesSymbol())
         {
         TR::SymbolReference *storeSymRef=storeNode->getSymbolReference();

         // TODO: improve by not killing stores that are definitely disjoint
         if (node->getSymbolReference()->getUseDefAliases().contains(storeSymRef, comp()))
        *it = NULL;
         }
      }
   }

const char *
TR::LocalDeadStoreElimination::optDetailString() const throw()
   {
   return "O^O LOCAL DEAD STORE ELIMINATION: ";
   }
