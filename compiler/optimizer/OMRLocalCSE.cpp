/*******************************************************************************
 * Copyright IBM Corp. and others 2000
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "optimizer/LocalCSE.hpp"

#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/arrayof.h"
#include "cs2/sparsrbit.h"
#include "env/IO.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/AliasSetInterface.hpp"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/List.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "ras/Debug.hpp"

#define MAX_DEPTH 3000
#define MAX_COPY_PROP 400
#define REPLACE_MARKER (MAX_SCOUNT-2)
#define NUM_BUCKETS 107

#define VOLATILE_ONLY 0
#define NON_VOLATILE_ONLY 1
#define VOLATILE_AND_NON_VOLATILE 2


OMR::LocalCSE::LocalCSE(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _storeMap(NULL),
     _arrayRefNodes(NULL)
   {
   static const char *e = feGetEnv("TR_loadaddrAsLoad");
   _loadaddrAsLoad = e ? atoi(e) != 0 : 1;
   }

TR::Optimization *OMR::LocalCSE::create(TR::OptimizationManager *manager)
   {
   return new (manager->allocator()) TR::LocalCSE(manager);
   }

bool OMR::LocalCSE::shouldCopyPropagateNode(TR::Node *parent, TR::Node *node, int32_t childNum, TR::Node *storeNode)
   {
   int32_t childAdjust = storeNode->getOpCode().isWrtBar() ? 2 : 1;
   int32_t maxChild = storeNode->getNumChildren() - childAdjust;

   if (node->getNumChildren() < maxChild)
      return false;

   for (int32_t k = 0; k < maxChild; k++)
      {
      if (storeNode->getChild(k) != node->getChild(k))
         return false;
      }

   if (_numCopyPropagations >= MAX_COPY_PROP)
      {
      if (trace())
         traceMsg(comp(),"z^z : _numCopyPropagations %d >= max %d\n",_numCopyPropagations,MAX_COPY_PROP);
      return false;
      }

   return true;
   }

bool OMR::LocalCSE::shouldCommonNode(TR::Node *parent, TR::Node *node)
   {
   return isTreetopSafeToCommon();
   }

TR::Node * getRHSOfStoreDefNode(TR::Node * storeNode)
   {
   int32_t childAdjust = (storeNode->getOpCode().isWrtBar()) ? 2 : 1;
   int32_t maxChild = storeNode->getNumChildren() - childAdjust;
   return storeNode->getChild(maxChild);
   }

TR::Node *OMR::LocalCSE::getNode(TR::Node *node)
   {
   if (_volatileState != VOLATILE_ONLY)
      return node;
   if (_simulatedNodesAsArray[node->getGlobalIndex()])
      {
      TR::Node *toReturn = _simulatedNodesAsArray[node->getGlobalIndex()];
      if (trace())
         traceMsg(comp(), "Updating comparison node n%dn to n%dn due to volatile simulation\n", node->getGlobalIndex(), toReturn->getGlobalIndex());
      return toReturn;
      }
   return node;
   }


bool OMR::LocalCSE::doExtraPassForVolatiles()
   {
   if (TR::isJ9() && comp()->getMethodHotness() >= hot && !comp()->getOption(TR_DisableLocalCSEVolatileCommoning))
      return true;
   return false;
   }

int32_t OMR::LocalCSE::perform()
   {
   if (trace())
      traceMsg(comp(), "Starting LocalCommonSubexpressionElimination\n");

   TR::Region &stackRegion = comp()->trMemory()->currentStackRegion();
   _storeMap = new (stackRegion) StoreMap((StoreMapComparator()), StoreMapAllocator(stackRegion));

   TR::TreeTop *tt, *exitTreeTop;
   for (tt = comp()->getStartTree(); tt; tt = exitTreeTop->getNextTreeTop())
      {
      exitTreeTop = tt->getExtendedBlockExitTreeTop();
      _volatileState =  VOLATILE_AND_NON_VOLATILE;
      if (doExtraPassForVolatiles())
         {
         if (trace())
            traceMsg(comp(), "LocalCSE entering 2 pass mode for volatile elimination - pass 1 for volatiles ONLY\n");
         _volatileState = VOLATILE_ONLY;
         transformBlock(tt, exitTreeTop);
         if (trace())
            traceMsg(comp(), "LocalCSE volatile only pass 1 complete - pass 2 for non-volatiles ONLY\n");
         _volatileState = NON_VOLATILE_ONLY;
         transformBlock(tt, exitTreeTop);
         }
      else
         transformBlock(tt, exitTreeTop);
      }

   if (trace())
      traceMsg(comp(), "\nEnding LocalCommonSubexpressionElimination\n");

   _storeMap = NULL;
   return 1; // actual cost
   }


int32_t OMR::LocalCSE::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      {
      _volatileState = VOLATILE_AND_NON_VOLATILE;
      if (doExtraPassForVolatiles())
         {
         if (trace())
            traceMsg(comp(), "LocalCSE entering 2 pass mode for volatile elimination - pass 1 for volatiles ONLY\n");
         _volatileState = VOLATILE_ONLY;
         transformBlock(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
         if (trace())
            traceMsg(comp(), "LocalCSE volatile only pass 1 complete - pass 2 for non-volatiles ONLY\n");
         _volatileState = NON_VOLATILE_ONLY;
         transformBlock(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
         }
      else
         transformBlock(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
      }

   return 0;
   }

void OMR::LocalCSE::prePerformOnBlocks()
   {
   TR::Region &stackRegion = comp()->trMemory()->currentStackRegion();
   _storeMap = new (stackRegion) StoreMap((StoreMapComparator()), StoreMapAllocator(stackRegion));

   int32_t symRefCount = 0;//comp()->getSymRefCount();
   int32_t nodeCount = 0;//comp()->getNodeCount();
   _seenCallSymbolReferences.init(symRefCount, stackRegion, growable);
   _availableLoadExprs.init(symRefCount, stackRegion, growable);
   _availablePinningArrayExprs.init(symRefCount, stackRegion, growable);
   _availableCallExprs.init(symRefCount, stackRegion, growable);
   _seenSymRefs.init(symRefCount, stackRegion, growable);
   _possiblyRelevantNodes.init(symRefCount, stackRegion, growable);
   _relevantNodes.init(symRefCount, stackRegion, growable);
   _killedPinningArrayExprs.init(symRefCount, stackRegion, growable);
   _killedNodes.init(nodeCount, stackRegion, growable);
   _parentAddedToHT.init(nodeCount, stackRegion, growable);

   comp()->incVisitCount();
   _mayHaveRemovedChecks = false;
   manager()->setAlteredCode(false);

   _simulatedNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(comp()->getNodeCount()*sizeof(TR::Node*));
   memset(_simulatedNodesAsArray, 0, comp()->getNodeCount()*sizeof(TR::Node*));
   }


void OMR::LocalCSE::postPerformOnBlocks()
   {
   _storeMap = NULL;

   if (_mayHaveRemovedChecks)
      requestOpt(OMR::catchBlockRemoval);
   }

bool
OMR::LocalCSE::isFirstReferenceToNode(TR::Node *parent, int32_t index, TR::Node *node, vcount_t visitCount)
   {
   return (visitCount > node->getVisitCount());
   }

bool OMR::LocalCSE::shouldTransformBlock(TR::Block *block)
   {
   if (block->isCold())
      return false;

   return true;
   }

void OMR::LocalCSE::transformBlock(TR::TreeTop * entryTree, TR::TreeTop * exitTree)
   {
   if (!shouldTransformBlock(entryTree->getNode()->getBlock()))
      return;

   // From here, down, stack memory allocations will die when the function returns
   TR::StackMemoryRegion stackMemoryRegion(*trMemory());

   TR::TreeTop *currentTree = entryTree;
   int32_t numStores = 0, numNullChecks = 0;
   _numNodes = 0;
   _numNullCheckNodes = 0;
   _numCopyPropagations = 0;
   _arrayRefNodes = new (stackMemoryRegion) TR_ScratchList<TR::Node>(trMemory());

   // Count the number of stores and null checks in this method; this is
   // required to allocate data structures of the correct size. Null checks
   // need to be counted separately as they are slightly different from other
   // nodes in that equivalence of NULLCHKs is dictated by the null check
   // reference rather than the entire subtree under the NULLCHK. Stores need
   // to be counted separately as local copy propagation is done simultaneously
   // along with local CSE. Also the total number of nodes are counted for
   // allocating data structures for local commoning.
   //
   _possiblyRelevantNodes.empty();
   _relevantNodes.empty();
   _availableLoadExprs.empty();
   _availablePinningArrayExprs.empty();
   _killedPinningArrayExprs.empty();
   _availableCallExprs.empty();
   _parentAddedToHT.empty();
   _killedNodes.empty();

   // Visit counts are incremented multiple times while transforming a block.
   // For each block, make sure there is enough room in the visit count to do this.
   comp()->incOrResetVisitCount();
   for (currentTree = entryTree->getNextRealTreeTop();
        currentTree != exitTree->getNextTreeTop();
        currentTree = currentTree->getNextRealTreeTop())
      {
      TR::Node *node = currentTree->getNode();
      if (node->getStoreNode())
         numStores++;
      if (node->getOpCodeValue() == TR::NULLCHK)
         numNullChecks++;

      getNumberOfNodes(node);
      }

   _storeMap->clear();

   _nullCheckNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(numNullChecks*sizeof(TR::Node*));
   memset(_nullCheckNodesAsArray, 0, numNullChecks*sizeof(TR::Node*));

   _replacedNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(_numNodes*sizeof(TR::Node*));
   _replacedNodesByAsArray = (TR::Node**)trMemory()->allocateStackMemory(_numNodes*sizeof(TR::Node*));
   memset(_replacedNodesAsArray, 0, _numNodes*sizeof(TR::Node*));
   memset(_replacedNodesByAsArray, 0, _numNodes*sizeof(TR::Node*));

   _hashTable = new (stackMemoryRegion) HashTable(std::less<int32_t>(), stackMemoryRegion);
   _hashTableWithSyms = new (stackMemoryRegion) HashTable(std::less<int32_t>(), stackMemoryRegion);
   _hashTableWithCalls = new (stackMemoryRegion) HashTable(std::less<int32_t>(), stackMemoryRegion);
   _hashTableWithConsts = new (stackMemoryRegion) HashTable(std::less<int32_t>(), stackMemoryRegion);

   _nextReplacedNode = 0;
   TR_BitVector seenAvailableLoadedSymbolReferences(stackMemoryRegion);
   _seenCallSymbolReferences.empty();
   _seenSymRefs.empty();

   int32_t nextNodeIndex = 0;
   comp()->incVisitCount();
   _curBlock = entryTree->getNode()->getBlock();

   for (currentTree = entryTree->getNextRealTreeTop();
        currentTree != exitTree->getNextTreeTop();
        currentTree = currentTree->getNextRealTreeTop())
      {
      // set up new current treetop being examined and do any
      // processing necessary on the treetop
      onNewTreeTop(currentTree);

      _canBeAvailable = true;
      _isAvailableNullCheck = true;
      _inSubTreeOfNullCheckReference = false;
      _isTreeTopNullCheck = false;

      TR::Node *currentNode = currentTree->getNode();
      if (currentNode->getOpCodeValue() == TR::NULLCHK)
         _isTreeTopNullCheck = true;
      else if (currentNode->getOpCodeValue() == TR::BBStart)
        _curBlock = currentNode->getBlock();

      //
      // This is the call to do local commoning and local copy propagation on this tree
      //
      bool nodeCanBeAvailable = true;
      examineNode(currentNode, seenAvailableLoadedSymbolReferences, NULL, -1, &nextNodeIndex, &nodeCanBeAvailable, 0);
      }
   comp()->invalidateAliasRegion();
   }

#ifdef J9_PROJECT_SPECIFIC
void
OMR::LocalCSE::setIsInMemoryCopyPropFlag(TR::Node *rhsOfStoreDefNode)
   {
   if (_treeBeingExamined &&
       !rhsOfStoreDefNode->getOpCode().isLoadConst() &&
       cg()->IsInMemoryType(rhsOfStoreDefNode->getType()))
      {
      if (cg()->traceBCDCodeGen() && _treeBeingExamined->getNode()->chkOpsIsInMemoryCopyProp() && !_treeBeingExamined->getNode()->isInMemoryCopyProp())
         traceMsg(comp(),"\tset IsInMemoryCopyProp on %s (%p), rhsOfStoreDefNode %s (%p)\n",
            _treeBeingExamined->getNode()->getOpCode().getName(),_treeBeingExamined->getNode(),rhsOfStoreDefNode->getOpCode().getName(),rhsOfStoreDefNode);
      _treeBeingExamined->getNode()->setIsInMemoryCopyProp(true);
      }
   }
#endif

bool
OMR::LocalCSE::allowNodeTypes(TR::Node *storeNode, TR::Node *node)
   {
   if (storeNode->getDataType() == node->getDataType())
      {
      return true;
      }
   else if (storeNode->getType().isIntegral() && node->getType().isAggregate() &&
            storeNode->getSize() == node->getSize())
      {
      return true;
      }
   return false;
   }

/**
 * Method examineNode
 * ------------------
 *
 * This method performs local copy propagation and local common
 * subexpression elimination on a given node.
 */

void OMR::LocalCSE::examineNode(TR::Node *node, TR_BitVector &seenAvailableLoadedSymbolReferences, TR::Node *parent, int32_t childNum, int32_t *nextLoadIndex, bool *parentCanBeAvailable, int32_t depth)
   {
   if (depth > MAX_DEPTH)
      {
      comp()->failCompilation<TR::ExcessiveComplexity>("scratch space in local CSE");
      }

   // If register pressure is high at this point, then nothing should be commoned or copy propagated across this point
   // To achieve that, we clear all the commoning and copy propagation related information here
   if (!canAffordToIncreaseRegisterPressure(node))
      {
      killAllDataStructures(seenAvailableLoadedSymbolReferences);
      }

   TR::ILOpCodes opCodeValue = node->getOpCodeValue();
   bool nodeCanBeAvailable = true;
   vcount_t visitCount = comp()->getVisitCount();

   if (trace())
     traceMsg(comp(), "Examining node %p\n",node);

   if (!isFirstReferenceToNode(parent, childNum, node, visitCount))
      {
      if (trace())
         traceMsg(comp(), "\tNot first Reference to Node\n");

      doCommoningAgainIfPreviouslyCommoned(node, parent, childNum);
      return;
      }

   TR::Node *nullCheckReference = NULL;
   if (_isTreeTopNullCheck)
      {
      if (_treeBeingExamined->getNode()->getNullCheckReference() == node)
         {
         _inSubTreeOfNullCheckReference = true;
         nullCheckReference = node;
         }
      }

   for (int32_t i = 0 ; i < node->getNumChildren(); i++)
      examineNode(node->getChild(i), seenAvailableLoadedSymbolReferences, node, i, nextLoadIndex, &nodeCanBeAvailable, depth+1);

   node->setVisitCount(visitCount);
   bool doneCopyPropagation = false;
   bool doneCommoning = false;

   // Note : Local copy propagation has been disabled for floats and doubles
   // because of strict/non strict problems when a very large float/double value
   // is stored and then read.
   //
   TR::SymbolReference * symRef = node->getOpCode().hasSymbolReference() ? node->getSymbolReference() : 0;
   if (symRef && node->getOpCode().isLoadVar())
      {
      StoreMap::iterator result = _storeMap->find(symRef->getReferenceNumber());
      if (result != _storeMap->end() && result->second->getSymbolReference()->getReferenceNumber() == symRef->getReferenceNumber())
         {
         // Base case: where symrefs match
         doCopyPropagationIfPossible(node, parent, childNum, result->second, symRef, visitCount, doneCopyPropagation);
         }
      else if (node->getOpCode().isLoadIndirect() &&
            node->getFirstChild()->getVisitCount() >= visitCount)
        {
        // Case #2: symrefs don't match, but underlying objects match by checking sizes, types, offsets, etc.
        for (auto itr = _storeMap->begin(), end = _storeMap->end(); itr != end; ++itr)
           {
           TR::Node *storeNode = itr->second;
           if (storeNode->getOpCode().isStoreIndirect() &&
               storeNode->getSymbolReference()->getSymbol()->getSize() == symRef->getSymbol()->getSize() &&
               storeNode->getSymbolReference()->getSymbol()->getDataType() == symRef->getSymbol()->getDataType() &&
               allowNodeTypes(storeNode, node) &&
               storeNode->getSymbolReference()->getOffset() == symRef->getOffset()
#ifdef J9_PROJECT_SPECIFIC
               && (!storeNode->getType().isBCD() ||
                storeNode->getDecimalPrecision() == node->getDecimalPrecision())
#endif
               )
              {

              if (doCopyPropagationIfPossible(node, parent, childNum, storeNode, symRef, visitCount, doneCopyPropagation))
                 break;
              }
           }
        }

     }

   // This node has not been commoned up before; so examine it now.
   // The checks below isAvailableNullCheck and canBeAvailable are really
   // fast checks that might rule out the possibility of the expression (node) being
   // available. Essentially we check if ALL symbols in the expression are
   // available (this does not guarantee that the expression is available)
   // but if any symbol is unavailable, then it is impossible for the node
   // to be available.
   //
   if (!doneCopyPropagation &&
       (((node->getOpCodeValue() == TR::NULLCHK) && (isAvailableNullCheck(node, seenAvailableLoadedSymbolReferences))) ||
         (canBeAvailable(parent, node, seenAvailableLoadedSymbolReferences, nodeCanBeAvailable))
        || (!_loadaddrAsLoad && node->getOpCodeValue() == TR::loadaddr)))
     {
     doCommoningIfAvailable(node, parent, childNum, doneCommoning);
     }

   // Code below deals with how the availability and copy propagation
   // information is updated once this node is seen
   //
   TR_UseDefAliasSetInterface UseDefAliases = node->mayKill(true);
   bool hasAliases = !UseDefAliases.isZero(comp());
   bool alreadyKilledAtVolatileLoad = false;
   if (hasAliases && !doneCommoning)
      alreadyKilledAtVolatileLoad = killExpressionsIfVolatileLoad(node, seenAvailableLoadedSymbolReferences, UseDefAliases);

   // Step 1 : add this node to the data structures so that it can be considered for commoning
   //
   if (!(doneCommoning || doneCopyPropagation))
      {
      makeNodeAvailableForCommoning(parent, node, seenAvailableLoadedSymbolReferences, parentCanBeAvailable);
      }

   if (trace())
      {
      TR_BitVector tmpAliases(comp()->trMemory()->currentStackRegion());
      traceMsg(comp(), "For Node %p UseDefAliases = ",node);
      UseDefAliases.getAliasesAndUnionWith(tmpAliases);
      tmpAliases.print(comp());
      traceMsg(comp(), "\n");
      }

   // Step 2 : if this node is a potential kill point then update the fast and slow
   // commoning and copy propagation info
   //
   if (node->getOpCode().isStore() || hasAliases)
      {
      // This node is a potential kill point
      //
      if (trace())
         traceMsg(comp(), "\tnode %p isStore = %d or hasAliases = %d\n",node,node->getOpCode().isStore(),hasAliases);

      int32_t symRefNum = node->getSymbolReference()->getReferenceNumber();
      bool previouslyAvailable = false;

      if (hasAliases)
        {
        //
        // Kill slow copy propagation info
        //
        if (UseDefAliases.containsAny(_seenSymRefs, comp()))
           {

           int32_t storeNodesSize = static_cast<int32_t>(_storeMap->size());
           // If we have over 500 store nodes, get aliases and iterate over the smaller set,
           // Less than 500 nodes should not be too significant as to which set to iterate over.
           if(storeNodesSize >= 500)
              {

              // Getting aliases can be very expensive if there are alot of aliases
              // For this reason we don't always want to do this, only if storeNodes is large
              // and we can make up the cost of getting aliases by iterating the smaller set
              TR_BitVector tmpAliases(comp()->trMemory()->currentStackRegion());
              UseDefAliases.getAliasesAndUnionWith(tmpAliases);

              // Iterate over the smaller set to save compile time, this can be very significant
              if (storeNodesSize < tmpAliases.elementCount())
                 {
                 for (auto itr = _storeMap->begin(), end = _storeMap->end(); itr != end; )
                    {
                    // Kill stores that are available for copy propagation based
                    // on this definition
                    TR::Node *storeNode = itr->second;
                    int32_t storeSymRefNum = storeNode->getSymbolReference()->getReferenceNumber();

                    if ((symRef->getReferenceNumber() == storeSymRefNum) || tmpAliases.get(storeSymRefNum))
                       {
                       _storeMap->erase(itr++);
                       }
                    else
                       ++itr;
                    }
                 }
              else
                 {
                 TR_BitVectorIterator bvi(tmpAliases);
                 while (bvi.hasMoreElements())
                    {
                    int32_t sc = bvi.getNextElement();
                    StoreMap::iterator result = _storeMap->find(sc);
                    if (result != _storeMap->end())
                       _storeMap->erase(sc);
                    }
                 }
              }
           else
              {
              for (auto itr = _storeMap->begin(), end = _storeMap->end(); itr != end; )
                 {
                 TR::Node *storeNode = itr->second;
                 int32_t storeSymRefNum = storeNode->getSymbolReference()->getReferenceNumber();

                 if ((symRef->getReferenceNumber() == storeSymRefNum) || UseDefAliases.contains(storeSymRefNum, comp()))
                    {
                    _storeMap->erase(itr++);
                    }
                 else
                    ++itr;
                 }
              }
           }

        // Kill fast commoning info
        //
        TR_BitVector tmp(seenAvailableLoadedSymbolReferences);
        tmp -= _seenCallSymbolReferences;

        if (trace())
           {
           traceMsg(comp(), "For node %p tmp: ",node);
           tmp.print(comp());

           traceMsg(comp(), "\n_seenCallSymbolReferences: ");
           _seenCallSymbolReferences.print(comp());

           traceMsg(comp(), "\n seenAvailableLoadedSymbolReferences:");
           seenAvailableLoadedSymbolReferences.print(comp());

           traceMsg(comp(), "\n");
           }

        if (UseDefAliases.containsAny(tmp, comp()))
           {
           // we want to keep the last load of a particular sym ref alive but no earlier loads should survive
           // note that volatile processing already handles killing symbols when necessary hence the else
           if (alreadyKilledAtVolatileLoad)
              seenAvailableLoadedSymbolReferences.set(symRef->getReferenceNumber());
           else
              {
              previouslyAvailable = true;
              tmp = seenAvailableLoadedSymbolReferences;
              tmp &= _seenCallSymbolReferences;
              UseDefAliases.getAliasesAndSubtractFrom(seenAvailableLoadedSymbolReferences);
              seenAvailableLoadedSymbolReferences |= tmp;
              }

           if (alreadyKilledAtVolatileLoad) // we want to keep the last load of a particular sym ref alive but no earlier loads should surivive
              seenAvailableLoadedSymbolReferences.set(symRef->getReferenceNumber());
           }
        }
     else
        {
        // There are no use def aliases
        //
        int32_t symRefNumber = symRef->getReferenceNumber();

        // Kill slow copy propagation info
        //
        if (_seenSymRefs.get(symRefNumber))
           {
           for (auto itr = _storeMap->begin(), end = _storeMap->end(); itr != end; ++itr)
              {
              // Kill stores that are available for copy propagation based
              // on this definition
              TR::Node *storeNode = itr->second;
              int32_t storeSymRefNum = storeNode->getSymbolReference()->getReferenceNumber();

              if (symRefNumber == storeSymRefNum)
                 {
                 _storeMap->erase(itr++);
                 }
              }
           }

        // Kill fast availability info for commoning
        //
        if (seenAvailableLoadedSymbolReferences.get(symRef->getReferenceNumber()))
           previouslyAvailable = true;
        seenAvailableLoadedSymbolReferences.reset(symRef->getReferenceNumber());
        }

      // Internal pointers should be killed if the pinning array was killed
      //
      if (symRef && symRef->getSymbol()->isAuto() &&
         _availablePinningArrayExprs.get(symRef->getReferenceNumber()))
         {
         killAllInternalPointersBasedOnThisPinningArray(symRef);
         }

      // Only go into killing available expressions (i.e. slow availability info for commoning) if the symbol
      // is available now; otherwise there is nothing to kill.
      //
      if (previouslyAvailable)
         {
         if (hasAliases)
            killAvailableExpressionsUsingAliases(UseDefAliases);
         if (node->getOpCode().isLikeDef())
            killAvailableExpressions(symRefNum);
         }

      // Step 3 : add this node as a copy propagation candidate if it is a store
      //
      if (node->getOpCode().isStore())
         {
         (*_storeMap)[symRefNum] = node;
         }

      if (symRef && !node->getOpCode().isCall())
         _seenSymRefs.set(symRef->getReferenceNumber());
      }

   killAvailableExpressionsAtGCSafePoints(node, parent, seenAvailableLoadedSymbolReferences);

   if (_isTreeTopNullCheck)
      {
      if (nullCheckReference != NULL)
         _inSubTreeOfNullCheckReference = false;
      }

   if (node->getOpCode().isCall())
      {
      seenAvailableLoadedSymbolReferences.set(node->getSymbolReference()->getReferenceNumber());
      // pure calls can be commoned so we must observe kills
      if (!node->isPureCall())
         _seenCallSymbolReferences.set(node->getSymbolReference()->getReferenceNumber());
      }

   static char *verboseProcessing = feGetEnv("TR_VerboseLocalCSEAvailableSymRefs");
   if (verboseProcessing)
      {
      traceMsg(comp(), "  after n%dn [%p] seenAvailableLoadedSymbolReferences:", node->getGlobalIndex(), node);
      seenAvailableLoadedSymbolReferences.print(comp());
      traceMsg(comp(), "\n");
      }
   }

void OMR::LocalCSE::doCommoningAgainIfPreviouslyCommoned(TR::Node *node, TR::Node *parent, int32_t childNum)
   {
   for (int32_t i = 0; i < _nextReplacedNode;i++)
      {
      // If we have already seen this node before and commoned it up,
      // then it will be present in the _replacedNodesAsArray data structure
      // and we can common it up now again. When a parent node is commoned
      // all its children get put into _replacedNodesAsArray (as they have
      // common counterparts (children of node that the parent is commoned with)
      //
      if (_replacedNodesAsArray[i] == node &&
          shouldCommonNode(parent, node) &&
          performTransformation(comp(), "%s   Local Common Subexpression Elimination commoning node : %p again\n", optDetailString(), node))
         {
         TR::Node *replacingNode = _replacedNodesByAsArray[i];
         parent->setChild(childNum, replacingNode);
         if (replacingNode->getReferenceCount() == 0)
            recursivelyIncReferenceCount(replacingNode);
         else
            replacingNode->incReferenceCount();

         if (node->getReferenceCount() <= 1)
            optimizer()->prepareForNodeRemoval(node);
         node->recursivelyDecReferenceCount();

         if (parent->getOpCode().isResolveOrNullCheck() || ((parent->getOpCodeValue() == TR::compressedRefs) && (childNum == 0)))
            {
            TR::Node::recreate(parent, TR::treetop);
            for (int32_t index =1;index < parent->getNumChildren(); index++)
               parent->getChild(index)->recursivelyDecReferenceCount() ;
            parent->setNumChildren(1);
            }

         break;
         }
      }
   }
/**
 * We can allow auto or parms which are not global during the volatile only phase
 * as we do not expect those field to be changing. This enables up to common volatiles that are based on an
 * indirection chain of such non volatile autos or parms that are not global by definition.
 * Following query returns true if the node can be commoned in volatile only pass
 */
bool OMR::LocalCSE::canCommonNodeInVolatilePass(TR::Node *node)
   {
   return node->getOpCode().hasSymbolReference() && (node->getSymbol()->isVolatile() || node->getSymbol()->isAutoOrParm());
   }


void OMR::LocalCSE::doCommoningIfAvailable(TR::Node *node, TR::Node *parent, int32_t childNum, bool &doneCommoning)
   {
   // If the expression can be available, search for it in the
   // hash table and return the available expression; return NULL if
   // no available expression was found to match this one.
   //
   TR::Node *availableExpression = getAvailableExpression(parent, node);

   if (availableExpression && (availableExpression != node) &&
       shouldCommonNode(parent, node) &&
       performTransformation(comp(), "%s   Local Common Subexpression Elimination commoning node : %p by available node : %p\n", optDetailString(), node, availableExpression))
      {
      if (!node->getOpCode().hasSymbolReference() ||
          (_volatileState == VOLATILE_ONLY && canCommonNodeInVolatilePass(node)) ||
          (_volatileState != VOLATILE_ONLY))
         {
         TR_ASSERT(_curBlock, "_curBlock should be non-null\n");


         requestOpt(OMR::treeSimplification, true, _curBlock);
         requestOpt(OMR::localReordering, true, _curBlock);

         _mayHaveRemovedChecks = true;
         if (parent != NULL)
            {
            // This node is not a treetop type node; so just make its
            // (non NULL) parent point at the available child expression
            //
            doneCommoning = true;
            manager()->setAlteredCode(true);

            if (node->getLocalIndex() != REPLACE_MARKER)
               collectAllReplacedNodes(node, availableExpression);

            if ((!parent->getOpCode().isResolveOrNullCheck() &&
                  (parent->getOpCodeValue() != TR::DIVCHK)) &&
                  ((parent->getOpCodeValue() != TR::compressedRefs) || (childNum != 0)))
               commonNode(parent, childNum, node, availableExpression);
            else
               {
               optimizer()->prepareForNodeRemoval(parent);

               int32_t endIndex = _treeBeingExamined->getNode()->getNumChildren();
               if (parent->getOpCodeValue() == TR::compressedRefs)
                  {
                  TR::Node::recreate(parent, TR::treetop);
                  for (int32_t index =1;index < parent->getNumChildren(); index++)
                     parent->getChild(index)->recursivelyDecReferenceCount() ;
                  parent->setNumChildren(1);
                  }
               else
                  {
                  for (int32_t index =0;index < endIndex; index++)
                     _treeBeingExamined->getNode()->getChild(index)->recursivelyDecReferenceCount() ;
                  TR::TreeTop *prevTree = _treeBeingExamined->getPrevTreeTop();
                  TR::TreeTop *nextTree = _treeBeingExamined->getNextTreeTop();
                  prevTree->setNextTreeTop(nextTree);
                  nextTree->setPrevTreeTop(prevTree);
                  }
               }
            }
         else
            {
            // This node is a treetop node; we might be able to remove the
            // entire treetop if it is common (e.g. BNDCHK etc.)
            //
            TR::Node *node = _treeBeingExamined->getNode();
            if (node->getOpCode().isResolveOrNullCheck())
               {
               TR_ASSERT(node->getNumChildren() == 1, "Local CSE, NULLCHK has more than one child");

               // If the child of the nullchk is normally a treetop node, replace
               // the nullchk with that node
               //
               if (node->getFirstChild()->getOpCode().isTreeTop())
                  {
                  if ((comp()->useAnchors() && node->getFirstChild()->getOpCode().isStoreIndirect()))
                     {
                     TR::Node::recreate(node, TR::treetop);
                     }
                  else
                     {
                     TR_ASSERT(node->getFirstChild()->getReferenceCount() == 1, "Local CSE, treetop child of NULLCHK has other uses");
                     // Make sure the child doesn't get removed too
                     //
                     node->getFirstChild()->incReferenceCount();
                     optimizer()->prepareForNodeRemoval(node);
                     node->getFirstChild()->setReferenceCount(0);
                     _treeBeingExamined->setNode(node->getFirstChild());
                     }
                  }
               else
                  {
                  TR::Node::recreate(node, TR::treetop);
                  }
               }
            else
               {
               if (node->getLocalIndex() != REPLACE_MARKER)
                  collectAllReplacedNodes(node, availableExpression);

               doneCommoning = true;
               manager()->setAlteredCode(true);
               optimizer()->prepareForNodeRemoval(node);
               for (int32_t index =0;index < _treeBeingExamined->getNode()->getNumChildren(); index++)
                  _treeBeingExamined->getNode()->getChild(index)->recursivelyDecReferenceCount() ;
               TR::TreeTop *prevTree = _treeBeingExamined->getPrevTreeTop();
               TR::TreeTop *nextTree = _treeBeingExamined->getNextTreeTop();
               prevTree->setNextTreeTop(nextTree);
               nextTree->setPrevTreeTop(prevTree);
               }
            }
         }
      else
         {
         if (trace())
            traceMsg(comp(), "Simulating commoning of node n%dn with n%dn - current mode %n\n", node->getGlobalIndex(), availableExpression->getGlobalIndex(), _volatileState);
         _simulatedNodesAsArray[node->getGlobalIndex()] = availableExpression;
         }
      }
   }

bool OMR::LocalCSE::doCopyPropagationIfPossible(TR::Node *node, TR::Node *parent, int32_t childNum, TR::Node *storeNode, TR::SymbolReference *symRef, vcount_t visitCount, bool &doneCopyPropagation)
   {
   if (!shouldCopyPropagateNode(parent, node, childNum, storeNode))
      return false;

   int32_t childAdjust = storeNode->getOpCode().isWrtBar() ? 2 : 1;
   int32_t maxChild = storeNode->getNumChildren() - childAdjust;
   TR::Node *rhsOfStoreDefNode = storeNode->getChild(maxChild);

   bool isSafeToCommon = true;
   if (!shouldCommonNode(node, rhsOfStoreDefNode))
         isSafeToCommon = false;

   if ((!comp()->getOption(TR_MimicInterpreterFrameShape) ||
        !comp()->areSlotsSharedByRefAndNonRef() ||
        !symRef->getSymbol()->isAuto() ||
        !symRef->getSymbol()->castToAutoSymbol()->isSlotSharedByRefAndNonRef()) &&
       shouldCommonNode(parent, node) &&
       isSafeToCommon &&
       canAffordToIncreaseRegisterPressure() &&
       (!node->getOpCode().hasSymbolReference() ||
        node->getSymbolReference() != comp()->getSymRefTab()->findVftSymbolRef()) &&
       (symRef->storeCanBeRemoved() ||
        (!symRef->getSymbol()->isVolatile() &&
         rhsOfStoreDefNode->getDataType() == TR::Float &&
         (rhsOfStoreDefNode->getOpCode().isCall() ||
          (rhsOfStoreDefNode->getOpCode().isLoadConst()) ||
        rhsOfStoreDefNode->getOpCode().isLoadVar()))) &&
      (!parent->getOpCode().isSpineCheck() || childNum != 0) &&
      performTransformation(comp(), "%s   Local Common Subexpression Elimination propagating local #%d in node : %p PARENT : %p from node %p\n", optDetailString(), symRef->getReferenceNumber(), node, parent, storeNode))
      {
      //TR::SymbolReference *originalSymbolReference = rhsOfStoreDefNode->getSymbolReference();
      dumpOptDetails(comp(), "%s   Rhs of store def node : %p\n", optDetailString(), rhsOfStoreDefNode);
      TR_ASSERT(_curBlock, "_curBlock should be non-null\n");

      requestOpt(OMR::treeSimplification, true, _curBlock);
      requestOpt(OMR::localReordering, true, _curBlock);

#ifdef J9_PROJECT_SPECIFIC
      // Set InMemoryCopyProp flag to help codegen evaluators distinguish between potential memory overlap
      // created by the optimizer (vs present from the start) for in memory types (i.e. BCD and Aggregate)
      setIsInMemoryCopyPropFlag(rhsOfStoreDefNode);
#endif

      prepareToCopyPropagate(node, rhsOfStoreDefNode);

      TR_ASSERT(parent, "No parent for eliminated expression");

      manager()->setAlteredCode(true);
      rhsOfStoreDefNode = replaceCopySymbolReferenceByOriginalIn(symRef/*, originalSymbolReference*/, storeNode, rhsOfStoreDefNode, node, parent, childNum);
      node->setVisitCount(visitCount);
      _replacedNodesAsArray[_nextReplacedNode] = node;
      _replacedNodesByAsArray[_nextReplacedNode++] = rhsOfStoreDefNode;

      if (parent->getOpCode().isResolveOrNullCheck() || ((parent->getOpCodeValue() == TR::compressedRefs) && (childNum == 0)))
         {
         TR::Node::recreate(parent, TR::treetop);
         for (int32_t index =1;index < parent->getNumChildren(); index++)
            parent->getChild(index)->recursivelyDecReferenceCount() ;
         parent->setNumChildren(1);
         }

      doneCopyPropagation = true;
      _numCopyPropagations++;

      return true;
      }

   return false;
   }

// Adjusts the availability information for the given node; called when
// the node is being examined by commoning
//
void OMR::LocalCSE::makeNodeAvailableForCommoning(TR::Node *parent, TR::Node *node, TR_BitVector &seenAvailableLoadedSymbolReferences, bool *canBeAvailable)
   {
   if (parent &&
       (parent->getOpCodeValue() == TR::Prefetch) &&
       (parent->getFirstChild() == node))
      return;

   if (comp()->cg()->supportsLengthMinusOneForMemoryOpts() && parent && parent->getOpCode().isMemToMemOp())
      {
      if (parent->getLastChild() == node)
         return;
      }

   if (node->getOpCode().hasSymbolReference())
      {
      if (!seenAvailableLoadedSymbolReferences.get(node->getSymbolReference()->getReferenceNumber()))
         {
         *canBeAvailable = false;
         if (_inSubTreeOfNullCheckReference)
            _isAvailableNullCheck = false;

         if (node->getOpCode().isLoadVar() ||
             node->getOpCode().isCheck() ||
             node->getOpCode().isCall() ||
             node->getOpCodeValue() == TR::instanceof ||
             ((node->getOpCodeValue() == TR::loadaddr) &&
               (node->getSymbol()->isNotCollected() ||
                node->getSymbol()->isAutoOrParm())))
            {
            bool isCallDirect = false;
            if (node->getOpCode().isCallDirect())
               isCallDirect = true;

            TR::SymbolReference *symRef = node->getSymbolReference();
            seenAvailableLoadedSymbolReferences.set(symRef->getReferenceNumber());
            }
         }

      if (node->getOpCodeValue() == TR::NULLCHK)
         _nullCheckNodesAsArray[_numNullCheckNodes++] = node;
      }

   addToHashTable(node, hash(parent, node));
   }


// Make the parent point to the common (available) node
//
void OMR::LocalCSE::commonNode(TR::Node *parent, int32_t childNum, TR::Node *node, TR::Node *replacingNode)
   {
   // make sure, if original is a direct load marked dontMoveUnderBranch, that replaced node gets that flag too
   if ((node->getOpCode().isLoadVar() || node->getOpCode().isLoadReg()) && node->isDontMoveUnderBranch() && (replacingNode->getOpCode().isLoadVar() || replacingNode->getOpCode().isLoadReg()))
      {
      //dumpOptDetails(comp(), "propagating dontMoveUnderBranch flag from node [%p] to replacing node [%p]\n", node, replacingNode);
      replacingNode->setIsDontMoveUnderBranch(true);
      }
   parent->setChild(childNum, replacingNode);
   if (replacingNode->getReferenceCount() == 0)
      recursivelyIncReferenceCount(replacingNode);
   else
      replacingNode->incReferenceCount();
   if (node->getReferenceCount() <= 1)
      optimizer()->prepareForNodeRemoval(node);
   node->recursivelyDecReferenceCount();
   }


// Actually propagate the value on the rhs of the store
// Examine a load and make its parent point to the node denoting
// the value being propagated
//
TR::Node *OMR::LocalCSE::replaceCopySymbolReferenceByOriginalIn(TR::SymbolReference *copySymbolReference/*, TR::SymbolReference *originalSymbolReference*/, TR::Node *defNode, TR::Node *rhsOfStoreDefNode, TR::Node *node, TR::Node *parent, int32_t childNum)
   {
   if (node->getOpCode().hasSymbolReference())
      {
      TR::SymbolReference *symRef = node->getSymbolReference();
      int32_t overrideNodePrecision = 0;
      if (symRef->getReferenceNumber() == copySymbolReference->getReferenceNumber())
         {
         if (rhsOfStoreDefNode->getReferenceCount() == 0)
            recursivelyIncReferenceCount(rhsOfStoreDefNode);
         else
            rhsOfStoreDefNode->incReferenceCount();

         if (node->getReferenceCount() <= 1)
            {
            optimizer()->prepareForNodeRemoval(node);
            }

         node->recursivelyDecReferenceCount();

         if (
#ifdef J9_PROJECT_SPECIFIC
             !rhsOfStoreDefNode->getType().isBCD() &&
#endif
             node->getDataType() != rhsOfStoreDefNode->getDataType() &&
             node->getSize() == rhsOfStoreDefNode->getSize())
            {
            //sign mismatch
            TR::ILOpCodes convOp = TR::ILOpCode::getProperConversion(rhsOfStoreDefNode->getDataType(), node->getDataType(), false);

            TR_ASSERT(convOp != TR::BadILOp,"BadIlOp for conversion from node %p (%s) to node %p (%s)",
               rhsOfStoreDefNode,rhsOfStoreDefNode->getOpCode().getName(),node,node->getOpCode().getName());

            TR::Node *convNode = NULL;
            if (TR::ILOpCode::isVectorOpCode(convOp) &&
                TR::ILOpCode::getVectorOperation(convOp) == TR::vconv)
               convNode = TR::Node::create(TR::ILOpCode::createVectorOpCode(TR::vconv, rhsOfStoreDefNode->getDataType(), node->getDataType()), 1, rhsOfStoreDefNode);
            else
               convNode = TR::Node::create(convOp, 1, rhsOfStoreDefNode);
            rhsOfStoreDefNode->decReferenceCount();
            parent->setAndIncChild(childNum, convNode);
            }
         else
            {
#ifdef J9_PROJECT_SPECIFIC
            if (rhsOfStoreDefNode->getType().isBCD())
               {
               if (defNode &&
                   defNode->getDataType() == TR::PackedDecimal &&
                   defNode->getOpCode().isStore() &&
                   defNode->mustCleanSignInPDStoreEvaluator())
                  {
                  TR_ASSERT(rhsOfStoreDefNode->getDataType() == TR::PackedDecimal,"rhsOfStoreDefNode %p (type %s) must be pd type if defNode %p is pd\n",
                     rhsOfStoreDefNode,rhsOfStoreDefNode->getDataType().toString(),defNode);
                  // clean should only have been folded into defNode if rhsOfStoreDefNode prec <= 31
                  TR_ASSERT(rhsOfStoreDefNode->getDecimalPrecision() <= TR::DataType::getMaxPackedDecimalPrecision(),
                     "rhsOfStoreDefNode %p prec %d must be <= max %d\n", rhsOfStoreDefNode, rhsOfStoreDefNode->getDecimalPrecision(), TR::DataType::getMaxPackedDecimalPrecision());
                  TR::Node *origRhsOfStoreDefNode = rhsOfStoreDefNode;
                  rhsOfStoreDefNode = TR::Node::create(origRhsOfStoreDefNode, TR::ILOpCode::cleanOpCode(origRhsOfStoreDefNode->getDataType()), 1);
                  rhsOfStoreDefNode->setChild(0, origRhsOfStoreDefNode);  // already inc'ed above
                  rhsOfStoreDefNode->setDecimalPrecision(origRhsOfStoreDefNode->getDecimalPrecision());
                  rhsOfStoreDefNode->setReferenceCount(1);
                  dumpOptDetails(comp(), "%sPreserve pdclean side-effect of %s [" POINTER_PRINTF_FORMAT "] when propagating %s [" POINTER_PRINTF_FORMAT "] to %s [" POINTER_PRINTF_FORMAT "] so create new %s [" POINTER_PRINTF_FORMAT "]\n",
                     optDetailString(),defNode->getOpCode().getName(),defNode,
                     origRhsOfStoreDefNode->getOpCode().getName(),origRhsOfStoreDefNode,
                     parent->getOpCode().getName(),parent,
                     rhsOfStoreDefNode->getOpCode().getName(),rhsOfStoreDefNode);
                  }

               int32_t nodePrecision = 0;
               if (overrideNodePrecision != 0)
                  {
                  nodePrecision = overrideNodePrecision;
                  if (comp()->cg()->traceBCDCodeGen() || trace())
                     traceMsg(comp(),"using overrideNodePrecision %d instead of node %s (%p)\n",overrideNodePrecision,node->getOpCode().getName(),node);
                  }
               else
                  {
                  TR_ASSERT(node->getType().isBCD(),"node %s (%p) must be a BCD type if overrideNodePrecision == 0\n",node->getOpCode().getName(),node);
                  nodePrecision = node->getDecimalPrecision();
                  }

               if (rhsOfStoreDefNode->getDecimalPrecision() != nodePrecision)
                  {
                  // If the copy propagation is going to change the size of the node being replaced then a size changing pdshl must be prepended in case
                  // the new parent requires explicit widening (such as if the parent is a call).
                  TR::Node *origRhsOfStoreDefNode = rhsOfStoreDefNode;
                  TR::ILOpCodes modifyPrecisionOp = TR::ILOpCode::modifyPrecisionOpCode(origRhsOfStoreDefNode->getDataType());
                  TR_ASSERT(modifyPrecisionOp != TR::BadILOp,"no bcd modify precision opcode found\n");
                  rhsOfStoreDefNode = TR::Node::create(origRhsOfStoreDefNode, modifyPrecisionOp, 1);
                  rhsOfStoreDefNode->setChild(0, origRhsOfStoreDefNode);  // already inc'ed above
                  rhsOfStoreDefNode->setDecimalPrecision(nodePrecision);
                  rhsOfStoreDefNode->setReferenceCount(1);
                  dumpOptDetails(comp(), "%sPrecision mismatch when propagating %s [" POINTER_PRINTF_FORMAT "] to %s [" POINTER_PRINTF_FORMAT "] so create new %s [" POINTER_PRINTF_FORMAT "]\n",
                     optDetailString(),origRhsOfStoreDefNode->getOpCode().getName(),origRhsOfStoreDefNode,
                     parent->getOpCode().getName(),parent,rhsOfStoreDefNode->getOpCode().getName(),rhsOfStoreDefNode);
                  }
               }
#endif
            parent->setChild(childNum, rhsOfStoreDefNode);
            }
         }
      }
   return rhsOfStoreDefNode;
   }

void OMR::LocalCSE::getNumberOfNodes(TR::Node *node)
   {
   _numNodes++;
   if (node->getVisitCount() == comp()->getVisitCount())
      return;
   node->setVisitCount(comp()->getVisitCount());

   node->setLocalIndex(0);

   if (node->getOpCode().hasSymbolReference())
      {
      if (_possiblyRelevantNodes.get(node->getSymbolReference()->getReferenceNumber()))
         _relevantNodes.set(node->getSymbolReference()->getReferenceNumber());
      _possiblyRelevantNodes.set(node->getSymbolReference()->getReferenceNumber());
      }

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      getNumberOfNodes(child);
      }
   }

// Performs the 'fast' preliminary check to see if an expression
// could be available; this check fails if any symbol reference in the
// subtree of the node is not available
//
bool OMR::LocalCSE::canBeAvailable(TR::Node *parent, TR::Node *node, TR_BitVector &seenAvailableLoadedSymbolReferences, bool canBeAvailable)
   {
  if (!canBeAvailable)
      return false;

   if (node->getOpCode().isBranch() || (node->getOpCode().isCase()))
      return false;

   if (!shouldCommonNode(parent, node))
      return false;

   if (node->getOpCodeValue() == TR::allocationFence)
      return false;

   if (node->getOpCode().isConversion() && node->getOpCode().isRef())
      return false;

   if (node->getOpCode().isLoadReg() || node->getOpCode().isStoreReg() || (node->getOpCodeValue() == TR::PassThrough && parent->getOpCodeValue() != TR::GlRegDeps) || (node->getOpCodeValue() == TR::GlRegDeps))
      return false;

   if (node->getOpCode().hasSymbolReference())
      {
      if ((!seenAvailableLoadedSymbolReferences.get(node->getSymbolReference()->getReferenceNumber())) ||
          ((_volatileState == VOLATILE_ONLY) && !canCommonNodeInVolatilePass(node)))
         return false;

      if (comp()->getOption(TR_MimicInterpreterFrameShape) &&
          comp()->areSlotsSharedByRefAndNonRef() &&
          node->getSymbolReference()->getSymbol()->isAuto() &&
          node->getSymbolReference()->getSymbol()->castToAutoSymbol()->isSlotSharedByRefAndNonRef())
         return false;
      }


   if (parent)
      {
      if (node->getOpCode().isCall() &&
         (!node->getSymbol()->isMethod() || (node->getSymbol()->isMethod() && !node->getSymbol()->castToMethodSymbol()->isPureFunction())) &&
         (parent->getOpCodeValue() == TR::treetop ||
           parent->getOpCode().isResolveOrNullCheck()))
         return false;
      }

   if (node->getOpCodeValue() == TR::PassThrough && parent->getOpCodeValue() != TR::GlRegDeps)
      return false;


   if (comp()->cg()->supportsLengthMinusOneForMemoryOpts() && parent && parent->getOpCode().isMemToMemOp())
      {
      if (parent->getLastChild() == node)
         return false;
      }

   int32_t i = 0;
   int32_t numChildren = node->getNumChildren();
   while (i < numChildren)
      {
      if (node->getChild(i)->getReferenceCount() == 1)
         {
         if (node->getChild(i)->getOpCode().isArrayRef())
            {
            if ((node->getChild(i)->getFirstChild()->getReferenceCount() == 1) ||
                (node->getChild(i)->getSecondChild()->getReferenceCount() == 1))
               return false;
            }
         else
            return false;
         }

      if (!_parentAddedToHT.get(node->getChild(i)->getGlobalIndex()))
         return false;

      i++;
      }


   return true;
   }

// Performs 'fast' availability check for NULLCHK nodes
//
bool OMR::LocalCSE::isAvailableNullCheck(TR::Node *node, TR_BitVector &seenAvailableLoadedSymbolReferences)
   {
   if (node->getOpCode().hasSymbolReference())
      {
      if (!seenAvailableLoadedSymbolReferences.get(node->getSymbolReference()->getReferenceNumber()))
         return false;
      }

   if (!_isAvailableNullCheck)
      return false;

   return true;
   }

// Looks through the hash table and returns the least
// recently available node (i.e. earliest occurrence of the common node)
//
TR::Node* OMR::LocalCSE::getAvailableExpression(TR::Node *parent, TR::Node *node)
   {
   if (node->getOpCodeValue() == TR::NULLCHK)
      {
      for (int32_t i=0;i<_numNullCheckNodes;i++)
         {
         if (!(_nullCheckNodesAsArray[i] == NULL))
            {
            if ((_nullCheckNodesAsArray[i]->getSymbolReference()->getReferenceNumber() == node->getSymbolReference()->getReferenceNumber()) &&
                (_nullCheckNodesAsArray[i]->getNullCheckReference() == node->getNullCheckReference()))
               return _nullCheckNodesAsArray[i];
            }
         }
      return NULL;
      }

   if (trace())
      {
      traceMsg(comp(), "In getAvailableExpression _availableCallExprs = ");
      _availableCallExprs.print(comp());
      traceMsg(comp(),"\n");
      }

   HashTable *hashTable;
   if (node->getOpCode().hasSymbolReference() && ((node->getOpCodeValue() != TR::loadaddr) || _loadaddrAsLoad))
      {
      if (node->getOpCode().isCall())
         hashTable = _hashTableWithCalls;
      else
         hashTable = _hashTableWithSyms;
      }
   else if (node->getOpCode().isLoadConst())
      hashTable = _hashTableWithConsts;
   else
      hashTable = _hashTable;

   int32_t hashValue = hash(parent, node);
   auto range = hashTable->equal_range(hashValue);

   for(auto it = range.first; it != range.second;)
      {
      TR::Node *other = it->second;
      bool remove = false;
      if (areSyntacticallyEquivalent(other, node, &remove))
         {
         if (trace())
            traceMsg(comp(), "node %p is syntactically equivalent to other %p\n",node,other);
         return other;
         }

      if (remove)
         {
         if (trace())
            traceMsg(comp(), "remove is true, removing entry %p\n", other);
         auto nextIt = it;
         ++nextIt;
         hashTable->erase(it);
         it = nextIt;
         _killedNodes.set(other->getGlobalIndex());
         }
      else
         {
         ++it;
         }
      }

   if (node->hasPinningArrayPointer() &&
       node->computeIsInternalPointer() &&
       cg()->supportsInternalPointers() &&
       (node->getFirstChild()->getOpCodeValue() == TR::aload) &&
       (node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto()) &&
       !_killedPinningArrayExprs.get(node->getFirstChild()->getSymbolReference()->getReferenceNumber()))
      {
      ListIterator<TR::Node> arrayRefNodesIt(_arrayRefNodes);
      TR::Node *arrayRefNode = arrayRefNodesIt.getFirst();
      for (;arrayRefNode; arrayRefNode = arrayRefNodesIt.getNext())
         {
         if ((arrayRefNode != node) &&
             (arrayRefNode->getFirstChild() == node->getFirstChild()) &&
             (arrayRefNode->getSecondChild() == node->getSecondChild()))
            {
            arrayRefNode->setIsInternalPointer(true);

            TR::AutomaticSymbol *autoSym = node->getFirstChild()->getSymbolReference()->getSymbol()->castToAutoSymbol();
            if (!autoSym->isInternalPointer())
               {
               arrayRefNode->setPinningArrayPointer(autoSym);
               autoSym->setPinningArrayPointer();
               }
            else
               arrayRefNode->setPinningArrayPointer(autoSym->castToInternalPointerAutoSymbol()->getPinningArrayPointer());
            return arrayRefNode;
            }
         }
      }

   return NULL;
   }


// We want to keep the last load of a particular sym ref alive but no earlier loads should surivive
// This routine kills all prior volatile loads of the same (or aliased) sym ref before we add the current
// volatile load to the available expressions
//
bool OMR::LocalCSE::killExpressionsIfVolatileLoad(TR::Node *node, TR_BitVector &seenAvailableLoadedSymbolReferences, TR_UseDefAliasSetInterface &UseDefAliases)
   {
   bool isVolatileRead = false;
   if (!node->getOpCode().isLikeDef() && node->mightHaveVolatileSymbolReference())
      isVolatileRead = true;

   if (isVolatileRead)
      {
      TR_BitVector tmp(seenAvailableLoadedSymbolReferences);
      tmp -= _seenCallSymbolReferences;
      if (_volatileState != VOLATILE_ONLY)
         {
         if (node->getOpCode().hasSymbolReference() && UseDefAliases.containsAny(tmp, comp()))
            killAvailableExpressionsUsingAliases(UseDefAliases);
         }
      else
         {
         TR_BitVector aliases(comp()->trMemory()->currentStackRegion());
         TR_BitVector processedAliases(comp()->trMemory()->currentStackRegion());
         UseDefAliases.getAliasesAndUnionWith(aliases);
         TR_BitVectorIterator bvi(aliases);
         while (bvi.hasMoreElements())
            {
            int32_t c = bvi.getNextElement();
            if (comp()->getSymRefTab()->getSymRef(c)->maybeVolatile())
               processedAliases.set(c);
            }
         if (node->getOpCode().hasSymbolReference() && processedAliases.intersects(tmp))
            killAvailableExpressionsUsingAliases(processedAliases);
         }
      }

   return isVolatileRead;
   }


// Kills all nodes that are aiadd (or aladd) or contain an aiadd (or aladd) node in the subtree
// Note that bucket 0 specifically holds such values and this kill operation
// is therefore quite cheap (its done at many places as there are many unresolved
// (GC safe) points at count=0
//
void OMR::LocalCSE::killAllAvailableExpressions()
   {
 //  traceMsg(comp(), "killAllAvailableExpressions 1 setting _availableCallExprs[0] to false\n");
   removeFromHashTable(_hashTable, 0);
   removeFromHashTable(_hashTableWithSyms, 0);
   _availableLoadExprs.reset(0);
   _availablePinningArrayExprs.reset(0);
   _availableCallExprs.reset(0);
   removeFromHashTable(_hashTableWithConsts, 0);
   removeFromHashTable(_hashTableWithCalls, 0);
   }

void OMR::LocalCSE::killAllInternalPointersBasedOnThisPinningArray(TR::SymbolReference *symRef)
   {
   ListIterator<TR::Node> arrayRefNodesIt(_arrayRefNodes);
   TR::Node *arrayRefNode = arrayRefNodesIt.getFirst();
   for (;arrayRefNode; arrayRefNode = arrayRefNodesIt.getNext())
      {
      if ((arrayRefNode->getNumChildren() > 0) &&
          (arrayRefNode->getFirstChild()->getOpCodeValue() == TR::aload) &&
          (arrayRefNode->getFirstChild()->getSymbolReference()->getSymbol()->isAuto()) &&
          (arrayRefNode->getFirstChild()->getSymbolReference() == symRef))
         {
         _arrayRefNodes->remove(arrayRefNode);
         _killedPinningArrayExprs.set(symRef->getReferenceNumber());
         }
      }
   }

// Kill expressions containing this symbol
//
void OMR::LocalCSE::killAvailableExpressions(int32_t symRefNum)
   {
   removeFromHashTable(_hashTableWithSyms, symRefNum);
   _availableLoadExprs.reset(symRefNum);
   _availablePinningArrayExprs.reset(symRefNum);
   _availableCallExprs.reset(symRefNum);
   }


void OMR::LocalCSE::killAvailableExpressionsUsingBitVector(HashTable *hashTable, TR_BitVector &vec)
   {
   TR_BitVectorIterator bvi(vec);
   while (bvi.hasMoreElements())
      {
      int32_t nextSymRefNum = bvi.getNextElement();
      auto range = hashTable->equal_range(nextSymRefNum);
      if (range.first != range.second)
         {
         auto lastItem = range.second;
         --lastItem;
         _killedNodes.set(lastItem->second->getGlobalIndex());

         hashTable->erase(range.first, range.second);
         }
      }
   }


void OMR::LocalCSE::killAvailableExpressionsUsingAliases(TR_BitVector &aliases)
   {
   TR_BitVector tmp(_availableLoadExprs);
   _availableLoadExprs -= aliases;

   tmp -= _availableLoadExprs;

   killAvailableExpressionsUsingBitVector(_hashTableWithSyms, tmp);

   TR_BitVector tmp2(_availableCallExprs);
   _availableCallExprs -= aliases;
   tmp2 -= _availableCallExprs;

   killAvailableExpressionsUsingBitVector(_hashTableWithCalls, tmp2);
   }



void OMR::LocalCSE::killAvailableExpressionsUsingAliases(TR_UseDefAliasSetInterface &UseDefAliases)
   {
   TR_BitVector tmp(_availableLoadExprs);
   UseDefAliases.getAliasesAndSubtractFrom(_availableLoadExprs);
   UseDefAliases.getAliasesAndSubtractFrom(_availablePinningArrayExprs);
   tmp -= _availableLoadExprs;

   killAvailableExpressionsUsingBitVector(_hashTableWithSyms, tmp);

   TR_BitVector tmp2(_availableCallExprs);
   UseDefAliases.getAliasesAndSubtractFrom(_availableCallExprs);
   tmp2 -= _availableCallExprs;

   killAvailableExpressionsUsingBitVector(_hashTableWithCalls, tmp2);
   }


void OMR::LocalCSE::killAllDataStructures(TR_BitVector &seenAvailableLoadedSymbolReferences)
   {
   _storeMap->clear();

   seenAvailableLoadedSymbolReferences.empty();

   _availableLoadExprs.empty();
   _availablePinningArrayExprs.empty();
   _availableCallExprs.empty();

   _hashTable->clear();
   _hashTableWithSyms->clear();
   _hashTableWithConsts->clear();
   _hashTableWithCalls->clear();

   killAllAvailableExpressions();
   }

// If this is a GC safe point, kill the address type available expressions that are aload,
// aiadd (or aladd) or that contain an aiadd (or an aladd).

void OMR::LocalCSE::killAvailableExpressionsAtGCSafePoints(TR::Node *node, TR::Node *parent, TR_BitVector &seenAvailableLoadedSymbolReferences)
   {
   // A GC safe point can only occur at a treetop.
   //
   if (parent != NULL)
      return;

   if ((node->getOpCodeValue() == TR::MethodEnterHook) ||
       (node->getOpCodeValue() == TR::MethodExitHook))
      {
      // Do not want to common anything across a method enter/exit hook
      // because this might result in problems when the block containing the
      // hook is split in the codegen phase : lower trees. A new auto might
      // be created that late and since that would be AFTER global live variables
      // for GC it would not have any live local index assigned (initialized to -1)
      // causing a crash when using the live local index to index into a bit vector.
      //
      if (trace())
         traceMsg(comp(), "Node %p is detected as a method enter/exit point\n", node);

      _storeMap->clear();

      seenAvailableLoadedSymbolReferences.empty();

      _availableLoadExprs.empty();
      _availablePinningArrayExprs.empty();
      _availableCallExprs.empty();

      _hashTable->clear();
      _hashTableWithSyms->clear();
      _hashTableWithConsts->clear();
      _hashTableWithCalls->clear();
      return;
      }

   if (node->canGCandReturn())
      {
      if (trace())
         traceMsg(comp(), "Node %p is detected as a GC safe point\n", node);

      for (auto itr = _storeMap->begin(), end = _storeMap->end(); itr != end; )
         {
         TR::Node *storeNode = itr->second;
         int32_t childAdjust = (storeNode->getOpCode().isWrtBar()) ? 2 : 1;

         TR::Node *rhsOfStoreDefNode = storeNode->getChild(storeNode->getNumChildren()-childAdjust);
         if (rhsOfStoreDefNode->getOpCode().isArrayRef())
            {
            int32_t storeSymRefNum = storeNode->getSymbolReference()->getReferenceNumber();
            _storeMap->erase(itr++);
            }
         else
            ++itr;
         }
      killAllAvailableExpressions();
      }
   }


int32_t OMR::LocalCSE::hash(TR::Node *parent, TR::Node *node)
   {
   if ((node->getOpCode().isArrayRef()) ||
       (node->isGCSafePointWithSymRef() && comp()->getOptions()->realTimeGC()) ||
       (comp()->getOption(TR_EnableHCR) && ((node->getOpCodeValue() == TR::loadaddr) || ((node->getOpCodeValue() == TR::aloadi) && node->getSymbolReference()->getSymbol()->isClassObject()))))
      return 0;

   if (node->getOpCodeValue() == TR::aconst &&
       (!parent || !(parent->isTheVirtualGuardForAGuardedInlinedCall() && parent->isProfiledGuard())))
      return 0;

   if ((node->getOpCode().hasSymbolReference() &&
       node->getOpCode().isLoadVar()) || node->getOpCode().isCall())
      return node->getSymbolReference()->getReferenceNumber();

   // Hash on the opcode of the children
   //
   uint32_t h, g;
   int32_t numChildren = node->getNumChildren();
   h = (node->getOpCodeValue() << 4) + numChildren;
   g = 0;
   for (int32_t i = numChildren-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      h <<= 4;

      if (child->getOpCode().hasSymbolReference())
         h += (int32_t)(intptr_t)child->getSymbolReference()->getReferenceNumber();
      else
         h++;

      g = h & 0xF0000000;
      h ^= g >> 24;
      }

   int32_t hashValue = h ^ g;
   // This value is NUM_BUCKETS - 1 (or symRefCount - 1)
   // for historical reasons.
   // It may be possible to remove the modulo, however:
   // - 0 cannot be returned if we get to this point
   // - the semantics of LocalCSE could slightly change, which
   //   may have other effects
   int32_t modVal = NUM_BUCKETS - 1;
   if (node->getOpCode().hasSymbolReference() &&
       ((node->getOpCodeValue() != TR::loadaddr) || _loadaddrAsLoad))
      modVal = comp()->getSymRefCount() - 1;
   else if (node->getOpCode().isLoadConst())
      hashValue += (int32_t)node->getConstValue();

   return 1 + (hashValue % modVal);
   }

void OMR::LocalCSE::addToHashTable(TR::Node *node, int32_t hashValue)
   {
   if (node->getOpCode().isStore() ||
       (node->getOpCode().isVoid() && (node->getOpCodeValue() != TR::PassThrough)))
      return;

   if (node->getOpCode().hasSymbolReference() &&
       !_relevantNodes.get(node->getSymbolReference()->getReferenceNumber()))
      return;

   int32_t numChildren = node->getNumChildren();
   for (int32_t i = numChildren-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      _parentAddedToHT.set(child->getGlobalIndex());
      }

   if ((node->getOpCode().isArrayRef()) &&
       cg()->supportsInternalPointers() &&
       (node->getFirstChild()->getOpCodeValue() == TR::aload) &&
       (node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto()))
      {
      _availablePinningArrayExprs.set(node->getFirstChild()->getSymbolReference()->getReferenceNumber());
      _arrayRefNodes->add(node);
      }

   auto pair = std::make_pair(hashValue, node);

   if (node->getOpCode().hasSymbolReference() && ((node->getOpCodeValue() != TR::loadaddr) || _loadaddrAsLoad))
      {
      if (node->getOpCode().isCall())
         {
         _hashTableWithCalls->insert(pair);
         _availableCallExprs.set(node->getSymbolReference()->getReferenceNumber());
         }
      else
         {
         _hashTableWithSyms->insert(pair);
         _availableLoadExprs.set(node->getSymbolReference()->getReferenceNumber());
         }
      }
   else if (node->getOpCode().isLoadConst())
      _hashTableWithConsts->insert(pair);
   else
      _hashTable->insert(pair);
   }


void OMR::LocalCSE::removeFromHashTable(HashTable *hashTable, int32_t hashValue)
   {
   auto range = hashTable->equal_range(hashValue);
   hashTable->erase(range.first, range.second);
   }


// Returns true if the two subtrees are exactly the same syntactically
//
bool OMR::LocalCSE::areSyntacticallyEquivalent(TR::Node *node1, TR::Node *node2, bool *remove)
   {
   // traceMsg(comp(), "  Comparing node n%dn with n%dn for equivalence\n", node1->getGlobalIndex(), node2->getGlobalIndex());

   node1 = getNode(node1);
   node2 = getNode(node2);

   // WCodeLinkageFixup calls LocalCSE without a full optimizer
   // so it has to pass in an explicit compilation
   //
   // allowBCDSignPromotion=true below (if node1 has 'better' sign state then node2 then consider equivalent as well)
   if (!TR::Optimizer::areNodesEquivalent(node1,node2,comp(), true))
      return false;

   if (!(node1->getNumChildren() == node2->getNumChildren()))
      {
      if (node1->getOpCode().isDiv() ||
          node1->getOpCode().isRem())
         {
         if (node1->getNumChildren() == 3)
            return false;
         }
      else
         return false;
      }


   if (node1 == node2)
      return true;

   for (int32_t i = 0;i < node1->getNumChildren();i++)
      {
      TR::Node *child1 = getNode(node1->getChild(i));
      TR::Node *child2 = getNode(node2->getChild(i));

      if (_killedNodes.get(child1->getGlobalIndex()))
         {
         *remove = true;
         return false;
         }

      if (child1 != child2)
         {
         if (child1->getOpCode().isArrayRef() &&
             child2->getOpCode().isArrayRef())
            {
            if (child2->getReferenceCount() > 1)
               return false;

            for (int32_t j = 0;j < child1->getNumChildren();j++)
               {
               TR::Node *grandChild1 = getNode(child1->getChild(j));
               TR::Node *grandChild2 = getNode(child2->getChild(j));

               if (_killedNodes.get(grandChild1->getGlobalIndex()))
                  {
                  *remove = true;
                  return false;
                  }

               if (grandChild1 != grandChild2)
                  {
                  return false;
                  }
               }
            }
         else if (node1->getOpCodeValue() == node2->getOpCodeValue() &&
                  node1->getOpCode().isCommutative() &&
                  node1->getNumChildren() == 2 && node2->getNumChildren() == 2 &&
                  getNode(node1->getFirstChild()) == getNode(node2->getSecondChild()) &&
                  getNode(node1->getSecondChild()) == getNode(node2->getFirstChild()))
            {
            return true;
            }
         else
            {
            return false;
            }
         }
      }

   if (node1->getOpCodeValue() == TR::dbits2l && node2->getOpCodeValue() == TR::dbits2l && node1->normalizeNanValues() != node2->normalizeNanValues())
      return false;

   if (node1->getOpCodeValue() == TR::fbits2i && node2->getOpCodeValue() == TR::fbits2i && node1->normalizeNanValues() != node2->normalizeNanValues())
      return false;

   return true;
   }

// Collects the children of a node being commoned up and
// establishes replaced-relationships with the children of the available
// (expression) node. This required as children of a node being commoned up
// could have reference counts > 1 which would mean they are accessed multiple
// times and each of those accesses should now point at the appropriate
// child of the available (expression) node
//
void OMR::LocalCSE::collectAllReplacedNodes(TR::Node *node, TR::Node *replacingNode)
   {
   TR_ASSERT(!(node->getOpCode().isExceptionRangeFence()), "Unexpected fence in local CSE");
   if (node->getOpCode().isCase())
      return;

   if (node->getReferenceCount() > 1)
      {
      _replacedNodesAsArray[_nextReplacedNode] = node;
      _replacedNodesByAsArray[_nextReplacedNode++] = replacingNode;

      if (trace())
         traceMsg(comp(), "Replaced node : %p Replacing node : %p\n", node, replacingNode);

      node->setLocalIndex(REPLACE_MARKER);
      }
   }


rcount_t OMR::LocalCSE::recursivelyIncReferenceCount(TR::Node *node)
   {
   rcount_t count;
   if (node->getReferenceCount() > 0)
      {
      count = node->incReferenceCount();
      }
   else
      {
      count = node->incReferenceCount();
      for (int32_t childCount = node->getNumChildren()-1; childCount >= 0; childCount--)
         recursivelyIncReferenceCount(node->getChild(childCount));
      }
   return count;
   }

const char *
OMR::LocalCSE::optDetailString() const throw()
   {
   return "O^O LOCAL COMMON SUBEXPRESSION ELIMINATION: ";
   }
