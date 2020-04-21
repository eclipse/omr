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

#include "optimizer/LocalReordering.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "cs2/sparsrbit.h"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"

#define OPT_DETAILS "O^O LOCAL REORDERING: "

TR_LocalReordering::TR_LocalReordering(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {}

int32_t TR_LocalReordering::perform()
   {
   if (trace())
      traceMsg(comp(), "Starting LocalReordering\n");

   TR::TreeTop *treeTop = comp()->getStartTree();
   while (treeTop != NULL)
      {
      TR::Node *node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Local reordering detected BBStart treetop");
      TR::Block *block = node->getBlock();
      if (!containsBarriers(block))
         transformBlock(block);
      treeTop = block->getExit()->getNextTreeTop();
      }

   if (trace())
      traceMsg(comp(), "\nEnding LocalReordering\n");

   return 2;
   }



int32_t TR_LocalReordering::performOnBlock(TR::Block *block)
   {
   if (block->getEntry() &&
       !containsBarriers(block))
      transformBlock(block);
   return 0;
   }

void TR_LocalReordering::prePerformOnBlocks()
   {
   comp()->incOrResetVisitCount();

   int32_t symRefCount = comp()->getSymRefCount();
   _treeTopsAsArray = (TR::TreeTop**)trMemory()->allocateStackMemory(symRefCount*sizeof(TR::TreeTop*));
   memset(_treeTopsAsArray, 0, symRefCount*sizeof(TR::TreeTop*));
   _seenSymbols = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
   _stopNodes = new (trStackMemory()) TR_BitVector(comp()->getNodeCount(), trMemory(), stackAlloc, growable);
   _temp = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);
   _seenUnpinnedInternalPointer = false;

   _counter = 0;
   }


void TR_LocalReordering::postPerformOnBlocks()
   {
   comp()->incVisitCount();
   }




bool TR_LocalReordering::transformBlock(TR::Block *block)
   {
   int32_t symRefCount = comp()->getSymRefCount();

   TR::TreeTop *exitTree = block->getLastRealTreeTop();
   _numStoreTreeTops = 0;

   // For each symbol initialize the maximum extent to which a store to
   // that symbol can be pushed down locally (we can't push the store down
   // past the exit tree of the block).
   //
   int32_t i;
   for (i=comp()->getSymRefTab()->getIndexOfFirstSymRef();i<symRefCount;i++)
      _treeTopsAsArray[i] = exitTree;

   // Call that does the pushing down of stores to temps (created by PRE)
   // to be closer (in the trees) to uses of the temps. This is to avoid needless
   // register spills simply because the value in the register was live for too long
   //
   delayDefinitions(block);

   comp()->incOrResetVisitCount();
   for (i=comp()->getSymRefTab()->getIndexOfFirstSymRef();i<symRefCount;i++)
      _treeTopsAsArray[i] = NULL;

   TR::TreeTop *currentTree = block->getEntry();
   exitTree = block->getExit();
   _numStoreTreeTops = 0;
   while (currentTree != exitTree)
      {
      if (currentTree->getNode()->getOpCode().isStore())
         {
         TR::SymbolReference *symRef = currentTree->getNode()->getSymbolReference();
         TR::Symbol *sym = symRef->getSymbol();

         // Insert the store between the currentTree and the first use (if it is
         // within the block).
         //
         if (sym->isAuto() || sym->isParm())
            {
            _numStoreTreeTops++;
            }
         }

      currentTree = currentTree->getNextTreeTop();
      }

   _storeTreesAsArray = (TR::TreeTop**)trMemory()->allocateStackMemory(_numStoreTreeTops*sizeof(TR::TreeTop*));
   memset(_storeTreesAsArray, 0, _numStoreTreeTops*sizeof(TR::TreeTop*));

   currentTree = block->getEntry();
   exitTree = block->getExit();
   int32_t index = 0;
   while (currentTree != exitTree)
      {
      if (currentTree->getNode()->getOpCode().isStore())
         {
         TR::SymbolReference *symRef = currentTree->getNode()->getSymbolReference();
         TR::Symbol *sym = symRef->getSymbol();

         // Insert the store between the currentTree and the first use (if it is
         // within the block).
         //
         if (sym->isAuto() || sym->isParm())
            {
            _storeTreesAsArray[index++] = currentTree;
            }
         }

      currentTree = currentTree->getNextTreeTop();
      }

   collectUses(block);

   return true;
   }




void TR_LocalReordering::delayDefinitions(TR::Block *block)
   {
   ///// The first real treetop must be INCLUDED in the while
   ///// loop below.
   /////TR::TreeTop *entryTree = block->getFirstRealTreeTop();
   TR::TreeTop *entryTree = block->getFirstRealTreeTop()->getPrevTreeTop();
   TR::TreeTop *currentTree = block->getExit();
   vcount_t visitCount1 = comp()->incVisitCount();
   while (!(currentTree == entryTree))
      {
      TR::TreeTop *prevTree = currentTree->getPrevTreeTop();
      TR::Node *currentNode = currentTree->getNode();

      if (currentNode->getOpCode().isStore())
         {
         TR::SymbolReference *symRef = currentNode->getSymbolReference();
         TR::Symbol *sym = symRef->getSymbol();

         // Insert the store between the currentTree and the first use (if it is
         // within the block).
         //
         if (sym->isAuto() || sym->isParm())
            {
            TR::Node *storedValue = currentNode->getFirstChild();
            bool specialImmovableStore = false;
            if (storedValue->getOpCode().hasSymbolReference())
               {
               if (storedValue->getSymbolReference()->getSymbol()->isMethodMetaData())
                  specialImmovableStore = true;
               }

            bool subtreeCommoned = isSubtreeCommoned(storedValue);

            if (!subtreeCommoned)
               {
               if (!specialImmovableStore)
                  insertDefinitionBetween(currentTree, _treeTopsAsArray[symRef->getReferenceNumber()]);
               _counter++;
               }
            else
               _numStoreTreeTops++;
            }
         }

      // Set this tree as the maximum limit to which definitions
      // of symbols used in this tree can be delayed (can't push a def past
      // one of its uses).
      //
      setUseTreeForSymbolReferencesIn(currentTree, currentNode, visitCount1);

      currentTree = prevTree;
      if (currentTree == NULL)
         break;

      if (currentTree->getNode()->getOpCode().isBranch() || currentTree->getNode()->getOpCode().isJumpWithMultipleTargets())
         {
         int32_t symRefCount = comp()->getSymRefCount();

         for (int32_t i=comp()->getSymRefTab()->getIndexOfFirstSymRef();i<symRefCount;i++)
            _treeTopsAsArray[i] = currentTree;
         }
      }
   }





void TR_LocalReordering::collectUses(TR::Block *block)
   {
   TR::TreeTop *exitTree = block->getExit();

   int32_t currentStoreIndex = 0;
   TR::TreeTop *currentTree = block->getEntry();
   vcount_t visitCount1 = comp()->incVisitCount();
   while (!(currentTree == exitTree))
      {
      TR::Node *currentNode = currentTree->getNode();
      moveStoresEarlierIfRhsAnchored(block, currentTree, currentNode, NULL, visitCount1);
      if (currentNode->getOpCode().isStore())
         {
         TR::SymbolReference *symRef = currentNode->getSymbolReference();
         TR::Symbol *sym = symRef->getSymbol();

         if (sym->isAuto() || sym->isParm())
            {
            if (currentNode->getFirstChild()->getReferenceCount() > 1)
               {
               _storeTreesAsArray[currentStoreIndex] = NULL;
               currentStoreIndex++;
               }
            }
         }

      currentTree = currentTree->getNextTreeTop();
      }
   }






void TR_LocalReordering::moveStoresEarlierIfRhsAnchored(TR::Block *block, TR::TreeTop *treeTop, TR::Node *node, TR::Node *parent, vcount_t visitCount)
   {
   if (node->getVisitCount() >= visitCount)
      {
      node->setLocalIndex(node->getLocalIndex()-1);
      return;
      }

   node->setVisitCount(visitCount);
   node->setLocalIndex(node->getReferenceCount()-1);

   if (node->getReferenceCount() > 1)
      {
      for (int32_t i=0;i<_numStoreTreeTops;i++)
         {
         if ((_storeTreesAsArray[i] != NULL) && (_storeTreesAsArray[i] != treeTop))
            {
            if (_storeTreesAsArray[i]->getNode()->getFirstChild() == node)
               {
               ///// Try to hoist it earlier
               /////
               TR::SymbolReference *storeSymRef = _storeTreesAsArray[i]->getNode()->getSymbolReference();
               _seenSymbols->empty();
               _seenUnpinnedInternalPointer = false;
               _seenSymbols->set(storeSymRef->getReferenceNumber());

               if (storeSymRef->sharesSymbol())
                  storeSymRef->getUseDefAliasesIncludingGCSafePoint().getAliasesAndUnionWith(*_seenSymbols);

               storeSymRef->getUseonlyAliases().getAliasesAndUnionWith(*_seenSymbols);

               if (node->isInternalPointer() && !node->getPinningArrayPointer())
                  {
                  _seenUnpinnedInternalPointer = true;
                  }

               insertEarlierIfPossible(_storeTreesAsArray[i], treeTop, true);
               _storeTreesAsArray[i] = NULL;
               }
            }
         }
      }

   _stopNodes->empty();
   int32_t numDeadChildren = 0;
   for (int32_t j=0;j<node->getNumChildren();j++)
      {
      TR::Node *child = node->getChild(j);
      moveStoresEarlierIfRhsAnchored(block, treeTop, child, node, visitCount);
      if ((child->getLocalIndex() == 0) &&
          (child->getReferenceCount() > 1) &&
          (!child->getOpCode().isLoadConst()))
        {
            //if (child->getOpCode().hasSymbolReference())
            //_stopSymbols->set(child->getSymbolReference()->getReferenceNumber());
        _stopNodes->set(child->getGlobalIndex());

        numDeadChildren++;
        }
      }

   static const char *noReordering = feGetEnv("TR_noReorder");
   if ((numDeadChildren > 1) &&
       (!parent ||
        !parent->getOpCode().isCheck()) &&
       !node->getOpCode().isTreeTop() &&
       !node->getOpCode().isCall() &&
       (node->getOpCodeValue() != TR::New) &&
       (node->getOpCodeValue() != TR::newarray) &&
       (node->getOpCodeValue() != TR::anewarray) &&
       (node->getOpCodeValue() != TR::multianewarray) &&
       (node->getOpCodeValue() != TR::MergeNew) &&
       (node->getOpCodeValue() != TR::monent) &&
       (node->getOpCodeValue() != TR::monexit) &&
       !node->getOpCode().isArrayRef() &&
       (!cg()->supportsFusedMultiplyAdd() ||
        ((parent->getOpCodeValue() != TR::dadd || node->getOpCodeValue() != TR::dmul) &&
         (parent->getOpCodeValue() != TR::fadd || node->getOpCodeValue() != TR::fmul))) &&
       !noReordering)
      {
      node->setLocalIndex(node->getLocalIndex()+1);
      TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, node));
      anchorTreeTop->getNode()->setLocalIndex(0);
      TR::TreeTop *origPrev = treeTop->getPrevTreeTop();
      if (origPrev)
        origPrev->join(anchorTreeTop);
      else
        comp()->setStartTree(anchorTreeTop);

      anchorTreeTop->join(treeTop);
      _seenSymbols->empty();
      _seenUnpinnedInternalPointer = false;
      vcount_t visitCount3 = comp()->incVisitCount();
      collectSymbolsUsedAndDefinedInNode(node, visitCount3);
      //dumpOptDetails(comp(), "Trying to insert node %p earlier\n", node);
      insertEarlierIfPossible(anchorTreeTop, block->getEntry(), false);
      }
   }





bool TR_LocalReordering::insertEarlierIfPossible(TR::TreeTop *storeTree, TR::TreeTop *treeTop, bool strictCheck)
   {
   vcount_t visitCount2 = comp()->incVisitCount();
   TR::TreeTop *currentTree = storeTree->getPrevTreeTop();
   bool foundInsertionSpot = false;

   while (currentTree != treeTop)
      {
      TR::Node *currentNode = currentTree->getNode();
      bool stopHere;
      if (strictCheck)
        stopHere = isAnySymInDefinedOrUsedBy(currentNode, visitCount2);
      else
        stopHere = isAnySymInDefinedBy(currentNode, visitCount2);

      if (!stopHere)
   {
   if (currentNode->getOpCode().isCheckCast()) // Moving a store earlier than a checkcast usually results in suboptimal type info (because of the way store constraints work, they only pick up the type info when the store was encountered)
      stopHere = true;
   }

      if (stopHere)
         {
         if (performTransformation(comp(), "\n%sInserting Definition @ 1 : [%p] between %p and %p (earlier between %p and %p)\n", OPT_DETAILS, storeTree->getNode(), currentTree->getNode(), currentTree->getNextTreeTop()->getNode(), storeTree->getPrevTreeTop()->getNode(), storeTree->getNextTreeTop()->getNode()))
            {
            //c(comp(), "Stopped at tree %p\n", currentNode);
            TR::TreeTop *origPrevTree = storeTree->getPrevTreeTop();
            TR::TreeTop *origNextTree = storeTree->getNextTreeTop();
            origPrevTree->setNextTreeTop(origNextTree);
            origNextTree->setPrevTreeTop(origPrevTree);
            TR::TreeTop *nextTree = currentTree->getNextTreeTop();

            //dumpOptDetails(comp(), "\n%sInserting Definition @ 1 : [%p] between %p and %p (earlier between %p and %p)\n", OPT_DETAILS, storeTree->getNode(), currentTree->getNode(), nextTree->getNode(), origPrevTree->getNode(), origNextTree->getNode());

            currentTree->setNextTreeTop(storeTree);
            storeTree->setPrevTreeTop(currentTree);
            storeTree->setNextTreeTop(nextTree);
            nextTree->setPrevTreeTop(storeTree);
      }
         foundInsertionSpot = true;
         break;
         }
      currentTree = currentTree->getPrevTreeTop();
      }

   if (!foundInsertionSpot &&
       performTransformation(comp(), "\n%sInserting Definition @ 2 : [%p] between %p and %p (earlier between %p and %p)\n", OPT_DETAILS, storeTree->getNode(), currentTree->getNode(), currentTree->getNextTreeTop()->getNode(), storeTree->getPrevTreeTop()->getNode(), storeTree->getNextTreeTop()->getNode()))
      {
      TR::TreeTop *origPrevTree = storeTree->getPrevTreeTop();
      TR::TreeTop *origNextTree = storeTree->getNextTreeTop();
      origPrevTree->setNextTreeTop(origNextTree);
      origNextTree->setPrevTreeTop(origPrevTree);
      TR::TreeTop *nextTree = currentTree->getNextTreeTop();

      //dumpOptDetails(comp(), "\n%sInserting Definition @ 2 : [%p] between %p and %p (earlier between %p and %p)\n", OPT_DETAILS, storeTree->getNode(), currentTree->getNode(), nextTree->getNode(), origPrevTree->getNode(), origNextTree->getNode());

      currentTree->setNextTreeTop(storeTree);
      storeTree->setPrevTreeTop(currentTree);
      storeTree->setNextTreeTop(nextTree);
      nextTree->setPrevTreeTop(storeTree);
      }

   return foundInsertionSpot;
   }





// Set this as the max limit to which definitions can be delayed
// for symbols used within this node. We cannot push def of the symbol beyond
// the any use of the symbol.
//
void TR_LocalReordering::setUseTreeForSymbolReferencesIn(TR::TreeTop *treeTop, TR::Node *node, vcount_t visitCount)
   {
   if (visitCount == node->getVisitCount())
      return;

   node->setVisitCount(visitCount);

   TR::ILOpCode &opCode = node->getOpCode();

   if (opCode.hasSymbolReference())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      TR::Symbol *sym = symRef->getSymbol();

      if (opCode.isLoadVar() || (opCode.getOpCodeValue() == TR::loadaddr))
         {
         if (sym->isAuto() || sym->isParm())
            _treeTopsAsArray[symRef->getReferenceNumber()] = treeTop;
         }
      else
         {
         if (!opCode.isStore())
            {
            TR_UseOnlyAliasSetInterface useAliasesInterface = symRef->getUseonlyAliases();
            if (!useAliasesInterface.isZero(comp()))
               {
               TR::SparseBitVector useAliases(comp()->allocator());
               useAliasesInterface.getAliases(useAliases);
               TR::SparseBitVector::Cursor aliasCursor(useAliases);
               for (aliasCursor.SetToFirstOne(); aliasCursor.Valid(); aliasCursor.SetToNextOne())
                  {
                  int32_t nextAlias = aliasCursor;
                  _treeTopsAsArray[nextAlias] = treeTop;
                  }
               }
            }

         _treeTopsAsArray[symRef->getReferenceNumber()] = treeTop;
         }
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      setUseTreeForSymbolReferencesIn(treeTop, child, visitCount);
      }
   }




// To avoid reordering trees when monitors are present
//
bool TR_LocalReordering::containsBarriers(TR::Block *block)
   {
   TR::TreeTop *currentTree = block->getEntry();
   TR::TreeTop *exitTree = block->getExit();

   while (!(currentTree == exitTree))
      {
      TR::Node *node = currentTree->getNode();
      if (node->getOpCodeValue() == TR::treetop || node->getOpCode().isResolveOrNullCheck())
         node = node->getFirstChild();

      TR::ILOpCode &opCode = node->getOpCode();
      if ((opCode.getOpCodeValue() == TR::monent) || (opCode.getOpCodeValue() == TR::monexit))
         return true;
      if (opCode.isStore() && node->getSymbol()->holdsMonitoredObject())
         return true;
      currentTree = currentTree->getNextTreeTop();
      }

   return false;
   }




// Inserts the store as close to the use as possible; checks whether the symbol is
// used/defined in the intervening trees before pushing the store down.
//
void TR_LocalReordering::insertDefinitionBetween(TR::TreeTop *treeTop, TR::TreeTop *exitTree)
   {
   if (treeTop == exitTree)
      return;

   if ((treeTop->getNextTreeTop()->getNode()->getOpCodeValue() == TR::dbgFence) && (treeTop->getNextTreeTop()->getNextTreeTop() == exitTree))
      return;

   TR::TreeTop *originalTree = treeTop;
   TR::Node *originalNode = originalTree->getNode();
   TR::TreeTop *currentTree = treeTop->getNextTreeTop();
   vcount_t visitCount = comp()->incVisitCount();
   _seenSymbols->empty();
   _seenUnpinnedInternalPointer = false;
   collectSymbolsUsedAndDefinedInNode(originalNode, visitCount);
   vcount_t visitCount1 = comp()->incVisitCount();
   while (!(currentTree == exitTree))
      {
      TR::Node *currentNode = currentTree->getNode();

      if (currentNode->getOpCodeValue() == TR::treetop)
         {
         currentNode = currentNode->getFirstChild();
         }

      if (isAnySymInDefinedOrUsedBy(currentNode, visitCount1))
         {
         if (performTransformation(comp(), "\n%sInserting Definition : [%p] between %p and %p (earlier between %p and %p)\n", OPT_DETAILS, originalNode, currentTree->getPrevTreeTop()->getNode(), currentTree->getNode(), originalTree->getPrevTreeTop()->getNode(), originalTree->getNextTreeTop()->getNode()))
            {
            TR::TreeTop *origPrevTree = originalTree->getPrevTreeTop();
            TR::TreeTop *origNextTree = originalTree->getNextTreeTop();
            origPrevTree->setNextTreeTop(origNextTree);
            origNextTree->setPrevTreeTop(origPrevTree);
            TR::TreeTop *prevTree = currentTree->getPrevTreeTop();

            originalTree->setNextTreeTop(currentTree);
            originalTree->setPrevTreeTop(prevTree);
            prevTree->setNextTreeTop(originalTree);
            currentTree->setPrevTreeTop(originalTree);
            }
         return;
         }

      currentTree = currentTree->getNextTreeTop();
      }
   }






void TR_LocalReordering::collectSymbolsUsedAndDefinedInNode(TR::Node *node, vcount_t visitCount)
   {
   if (node->getVisitCount() == visitCount)
      return;

   node->setVisitCount(visitCount);

   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      int32_t symRefNum = symReference->getReferenceNumber();
      _seenSymbols->set(symRefNum);
      }

   if (node->isInternalPointer() && !node->getPinningArrayPointer())
      {
      _seenUnpinnedInternalPointer = true;
      }

   for (int32_t j=0;j<node->getNumChildren();j++)
      collectSymbolsUsedAndDefinedInNode(node->getChild(j), visitCount);
   }





// Returns true if any symbol in node is defined or used in defNode
//
bool TR_LocalReordering::isAnySymInDefinedOrUsedBy(TR::Node *node, vcount_t visitCount)
   {
   if (visitCount == node->getVisitCount())
      return false;

   node->setVisitCount(visitCount);
   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      int32_t symRefNum = symReference->getReferenceNumber();

      if (_seenSymbols->get(symRefNum))
         {
         if (node->getNumChildren())
           {
           for (int32_t i = 0; i < node->getNumChildren();i++)
              {
              node->getChild(i)->setVisitCount(visitCount);
              }
           }
         return true;
         }

      bool isCallDirect = false;
      if (node->getOpCode().isCallDirect())
         isCallDirect = true;

      if (symReference->getUseDefAliasesIncludingGCSafePoint(isCallDirect).containsAny(*_seenSymbols, comp()))
         return true;

      if ((!(node->getOpCode().isLoadVar() || node->getOpCode().isStore() || (node->getOpCodeValue() == TR::loadaddr))))
         {
         if (symReference->getUseonlyAliases().containsAny(*_seenSymbols, comp()))
            return true;
         }
      }

   if (node->canCauseGC() && _seenUnpinnedInternalPointer)
      {
      dumpOptDetails(comp(), "\n%sisAnySymInDefinedOrUsedBy : found unpinned internal pointer at GC point %p\n", OPT_DETAILS, node);
      return true;
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (isAnySymInDefinedOrUsedBy(child, visitCount))
         return true;
      }

   return false;
   }




// Returns true if any symbol in node is defined
//
bool TR_LocalReordering::isAnySymInDefinedBy(TR::Node *node, vcount_t visitCount)
   {
   if (visitCount == node->getVisitCount())
      return false;

   node->setVisitCount(visitCount);

   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symReference = node->getSymbolReference();
      int32_t symRefNum = symReference->getReferenceNumber();

      if ((!node->getOpCode().isLoadVar() ||
           node->mightHaveVolatileSymbolReference()) &&
          !node->getOpCode().isCheck())
         {
         if (_seenSymbols->get(symRefNum))
            return true;

         bool isCallDirect = false;
         if (node->getOpCode().isCallDirect())
            isCallDirect = true;

        if (symReference->getUseDefAliasesIncludingGCSafePoint(isCallDirect).containsAny(*_seenSymbols, comp()))
           return true;
         }
      }

   if (node->canCauseGC() && _seenUnpinnedInternalPointer)
      {
      dumpOptDetails(comp(), "\n%sisAnySymInDefinedBy : found unpinned internal pointer at GC point %p\n", OPT_DETAILS, node);
      return true;
      }

   if (_stopNodes->get(node->getGlobalIndex()))
      return true;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (isAnySymInDefinedBy(child, visitCount))
         return true;
      }

   return false;
   }



// Returns true iff some node in the given subtree has reference count > 1
//
bool TR_LocalReordering::isSubtreeCommoned(TR::Node *node)
   {
   if (node->getReferenceCount() > 1)
      return true;

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (isSubtreeCommoned(child))
         return true;
      }

   return false;
   }

const char *
TR_LocalReordering::optDetailString() const throw()
   {
   return "O^O LOCAL REORDERING: ";
   }
