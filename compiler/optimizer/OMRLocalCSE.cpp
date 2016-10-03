/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include "optimizer/LocalCSE.hpp"

#include <stdlib.h>                             // for atoi
#include <string.h>                             // for NULL, memset
#include "codegen/CodeGenerator.hpp"            // for CodeGenerator
#include "codegen/FrontEnd.hpp"                 // for TR_FrontEnd, etc
#include "compile/Compilation.hpp"              // for Compilation
#include "compile/Method.hpp"                   // for MAX_SCOUNT
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"          // for TR::Options, etc
#include "cs2/arrayof.h"                        // for ArrayOf
#include "cs2/sparsrbit.h"                      // for ASparseBitVector, etc
#include "cs2/tableof.h"                        // for TableOf<>::Cursor, etc
#include "env/IO.hpp"                           // for POINTER_PRINTF_FORMAT
#include "env/StackMemoryRegion.hpp"
#include "env/TRMemory.hpp"                     // for BitVector, etc
#include "env/jittypes.h"                       // for intptrj_t
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"                         // for Block
#include "il/DataTypes.hpp"                     // for DataType, etc
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"                         // for ILOpCode, etc
#include "il/Node.hpp"                          // for Node, etc
#include "il/Node_inlines.hpp"                  // for Node::getDataType, etc
#include "il/Symbol.hpp"                        // for Symbol
#include "il/SymbolReference.hpp"               // for SymbolReference, etc
#include "il/TreeTop.hpp"                       // for TreeTop
#include "il/TreeTop_inlines.hpp"               // for Node::getChild, etc
#include "il/symbol/AutomaticSymbol.hpp"        // for AutomaticSymbol
#include "il/symbol/MethodSymbol.hpp"           // for MethodSymbol
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "infra/Assert.hpp"                     // for TR_ASSERT
#include "infra/BitVector.hpp"                  // for TR_BitVector
#include "infra/List.hpp"                       // for TR_ScratchList, etc
#include "infra/SimpleRegex.hpp"
#include "optimizer/Optimization.hpp"           // for Optimization
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"              // for Optimizer
#include "ras/Debug.hpp"                        // for TR_Debug

#define MAX_DEPTH 3000
#define MAX_COPY_PROP 400
#define REPLACE_MARKER (MAX_SCOUNT-2)

#define VOLATILE_ONLY 0
#define NON_VOLATILE_ONLY 1
#define VOLATILE_AND_NON_VOLATILE 2


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
 * 2. Local CSE is done on the same subtree after local copy propagation
 * but it is done in a top down manner so as to capture any commoning
 * opportunities as high up in the subtree as possible (i.e. the
 * maximum subtree is commoned).
 */


OMR::LocalCSE::LocalCSE(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _storeNodes(comp()->allocator()),
     _storeNodesIndex(comp()->allocator(), 0),
     _seenCallSymbolReferences(comp()->allocator()),
     _seenKilledSymbolReferences(comp()->allocator()),
     _seenSymRefs(comp()->allocator()),
     _possiblyRelevantNodes(comp()->allocator()),
     _relevantNodes(comp()->allocator()),
     _parentAddedToHT(comp()->allocator()),
     _killedNodes(comp()->allocator()),
     _availableLoadExprs(comp()->allocator()),
     _availablePinningArrayExprs(comp()->allocator()),
     _killedPinningArrayExprs(comp()->allocator()),
     _availableCallExprs(comp()->allocator()),
     _arrayRefNodes(trMemory())
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

   if (node->getOpCode().isReverseLoadOrStore() || storeNode->getOpCode().isReverseLoadOrStore())
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
   if (cg()->disableCommoningOfVolatiles())
      {
      TR::Node * nodeToCheck = node;
      while (nodeToCheck->getOpCode().isConversion())
         nodeToCheck = nodeToCheck->getFirstChild();
      if (nodeToCheck->mightHaveVolatileSymbolReference())
         return false;
      }

   if (!isTreetopSafeToCommon())
      return false;

   if (node->getType().isAggregate())
      return true;

   return node->canEvaluate();
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
   comp()->incVisitCount();
   _mayHaveRemovedChecks = false;
   manager()->setAlteredCode(false);

   _simulatedNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(comp()->getNodeCount()*sizeof(TR::Node*));
   memset(_simulatedNodesAsArray, 0, comp()->getNodeCount()*sizeof(TR::Node*));

   memset (_previousNodeConversionsTable, 0, TR_PNC_BUCKETS * sizeof (OMR::PreviousNodeConversion *));
   }


void OMR::LocalCSE::postPerformOnBlocks()
   {
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
   _arrayRefNodes.deleteAll();

   // Count the number of stores and null checks in this method; this is
   // required to allocate data structures of the correct size. Null checks
   // need to be counted separately as they are slightly different from other
   // nodes in that equivalence of NULLCHKs is dictated by the null check
   // reference rather than the entire subtree under the NULLCHK. Stores need
   // to be counted separately as local copy propagation is done simultaneously
   // along with local CSE. Also the total number of nodes are counted for
   // allocating data structures for local commoning.
   //
   int32_t symRefCount = comp()->getSymRefCount();
   _possiblyRelevantNodes.Clear();
   _relevantNodes.Clear();
   _availableLoadExprs.Clear();
   _availablePinningArrayExprs.Clear();
   _killedPinningArrayExprs.Clear();
   _availableCallExprs.Clear();
   _parentAddedToHT.Clear();
   _killedNodes.Clear();

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

   _storeNodes.MakeEmpty();
   _storeNodesIndex.ShrinkTo(0);

   _nullCheckNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(numNullChecks*sizeof(TR::Node*));
   memset(_nullCheckNodesAsArray, 0, numNullChecks*sizeof(TR::Node*));

   _replacedNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(_numNodes*sizeof(TR::Node*));
   _replacedNodesByAsArray = (TR::Node**)trMemory()->allocateStackMemory(_numNodes*sizeof(TR::Node*));
   memset(_replacedNodesAsArray, 0, _numNodes*sizeof(TR::Node*));
   memset(_replacedNodesByAsArray, 0, _numNodes*sizeof(TR::Node*));

   _hashTable._numBuckets = 107;
   _hashTable._buckets = (HashTableEntry**)trMemory()->allocateStackMemory(_hashTable._numBuckets*sizeof(HashTableEntry*));
   memset(_hashTable._buckets, 0, _hashTable._numBuckets*sizeof(HashTableEntry*));

   _hashTableWithSyms._numBuckets = symRefCount;
   _hashTableWithSyms._buckets = (HashTableEntry**)trMemory()->allocateStackMemory(_hashTableWithSyms._numBuckets*sizeof(HashTableEntry*));
   memset(_hashTableWithSyms._buckets, 0, _hashTableWithSyms._numBuckets*sizeof(HashTableEntry*));

   _hashTableWithCalls._numBuckets = symRefCount;
   _hashTableWithCalls._buckets = (HashTableEntry**)trMemory()->allocateStackMemory(_hashTableWithCalls._numBuckets*sizeof(HashTableEntry*));
   memset(_hashTableWithCalls._buckets, 0, _hashTableWithCalls._numBuckets*sizeof(HashTableEntry*));

   _hashTableWithConsts._numBuckets = 107;
   _hashTableWithConsts._buckets = (HashTableEntry**)trMemory()->allocateStackMemory(_hashTableWithConsts._numBuckets*sizeof(HashTableEntry*));
   memset(_hashTableWithConsts._buckets, 0, _hashTableWithConsts._numBuckets*sizeof(HashTableEntry*));


   _nextReplacedNode = 0;
   SharedSparseBitVector seenAvailableLoadedSymbolReferences(comp()->allocator());
   _seenCallSymbolReferences.Clear();
   _seenSymRefs.Clear();

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

void OMR::LocalCSE::examineNode(TR::Node *node, SharedSparseBitVector &seenAvailableLoadedSymbolReferences, TR::Node *parent, int32_t childNum, int32_t *nextLoadIndex, bool *parentCanBeAvailable, int32_t depth)
   {
   if (depth > MAX_DEPTH)
      {
      comp()->setErrorCode(COMPILATION_MAX_DEPTH_EXCEEDED);
      fe()->outOfMemory(comp(), "scratch space in local CSE");
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
      int32_t i = _storeNodesIndex[symRef->getReferenceNumber()];
      if (i && _storeNodes.Exists(i) &&
          _storeNodes[i]->getSymbolReference()->getReferenceNumber() == symRef->getReferenceNumber())
         {
         // Base case: where symrefs match
         doCopyPropagationIfPossible(node, parent, childNum, _storeNodes[i], symRef, visitCount, doneCopyPropagation);
         }
      else if (node->getOpCode().isLoadIndirect() &&
            node->getFirstChild()->getVisitCount() >= visitCount)
        {
        // Case #2: symrefs don't match, but underlying objects match by checking sizes, types, offsets, etc.
        CS2::TableOf<TR::Node *, TR::Allocator>::Cursor nc(_storeNodes);
        for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
           {
           if (_storeNodes[nc]->getOpCode().isStoreIndirect() &&
               _storeNodes[nc]->getSymbolReference()->getSymbol()->getSize() == symRef->getSymbol()->getSize() &&
               _storeNodes[nc]->getSymbolReference()->getSymbol()->getDataType() == symRef->getSymbol()->getDataType() &&
               allowNodeTypes(_storeNodes[nc], node) &&
               _storeNodes[nc]->getSymbolReference()->getOffset() == symRef->getOffset()
#ifdef J9_PROJECT_SPECIFIC
               && (!_storeNodes[nc]->getType().isBCD() ||
                _storeNodes[nc]->getDecimalPrecision() == node->getDecimalPrecision())
#endif
               )
              {

              if (doCopyPropagationIfPossible(node, parent, childNum, _storeNodes[nc], symRef, visitCount, doneCopyPropagation))
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
   TR_NodeKillAliasSetInterface UseDefAliases = node->mayKill(true);
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
      SharedSparseBitVector tmpAliases(comp()->allocator());
      traceMsg(comp(), "For Node %p UseDefAliases = ",node);
      UseDefAliases.getAliases(tmpAliases);
      *(comp()) << tmpAliases;
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

           uint32_t storeNodesSize = _storeNodes.NumberOfElements();
           // If we have over 500 store nodes, get aliases and iterate over the smaller set,
           // Less than 500 nodes should not be too significant as to which set to iterate over.
           if(storeNodesSize >= 500)
              {

              // Getting aliases can be very expensive if there are alot of aliases
              // For this reason we don't always want to do this, only if storeNodes is large
              // and we can make up the cost of getting aliases by iterating the smaller set
              SharedSparseBitVector tmpAliases(comp()->allocator());
              UseDefAliases.getAliases(tmpAliases);

              // Iterate over the smaller set to save compile time, this can be very significant
              if (storeNodesSize < tmpAliases.PopulationCount())
                 {
                 CS2::TableOf<TR::Node *, TR::Allocator>::Cursor nc(_storeNodes);
                 for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
                    {
                    // Kill stores that are available for copy propagation based
                    // on this definition
                    TR::Node *storeNode = _storeNodes[nc];
                    int32_t storeSymRefNum = storeNode->getSymbolReference()->getReferenceNumber();

                    if ((symRef->getReferenceNumber() == storeSymRefNum) || tmpAliases[storeSymRefNum])
                       {
                       _storeNodes.RemoveEntry(nc);
                       _storeNodesIndex[storeSymRefNum]=0;
                       }
                    }
                 }
              else
                 {
                 SharedSparseBitVector::Cursor sc(tmpAliases);
                 for (sc.SetToFirstOne(); sc.Valid(); sc.SetToNextOne())
                    {
                    if (_storeNodesIndex[sc] != 0)
                       {
                       uint32_t index = _storeNodesIndex[sc];
                       _storeNodes.RemoveEntry(index);
                       _storeNodesIndex[sc] = 0;
                       }
                    }
                 }
              }
           else
              {
              CS2::TableOf<TR::Node *, TR::Allocator>::Cursor nc(_storeNodes);
              for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
                 {
                 // Kill stores that are available for copy propagation based
                 // on this definition
                 TR::Node *storeNode = _storeNodes[nc];
                 int32_t storeSymRefNum = storeNode->getSymbolReference()->getReferenceNumber();

                 if ((symRef->getReferenceNumber() == storeSymRefNum) || UseDefAliases.contains(storeSymRefNum, comp()))
                    {
                    _storeNodes.RemoveEntry(nc);
                    _storeNodesIndex[storeSymRefNum]=0;
                    }
                 }
              }
           }

        // Kill fast commoning info
        //
        SharedSparseBitVector tmp(comp()->allocator());
        seenAvailableLoadedSymbolReferences.Andc(_seenCallSymbolReferences, tmp);

        if (trace())
           {
           traceMsg(comp(), "For node %p tmp: ",node);
           *(comp()) << tmp;

           traceMsg(comp(), "\n_seenCallSymbolReferences: ");
           *(comp()) << _seenCallSymbolReferences;

           traceMsg(comp(), "\n seenAvailableLoadedSymbolReferences:");
           *(comp()) << seenAvailableLoadedSymbolReferences;

           traceMsg(comp(), "\n");
           }

        if (UseDefAliases.containsAny(tmp, comp()))
           {
           // we want to keep the last load of a particular sym ref alive but no earlier loads should survive
           // note that volatile processing already handles killing symbols when necessary hence the else
           if (alreadyKilledAtVolatileLoad)
              seenAvailableLoadedSymbolReferences[symRef->getReferenceNumber()]=true;
           else
              {
              previouslyAvailable = true;
              seenAvailableLoadedSymbolReferences.And(_seenCallSymbolReferences,tmp);
              UseDefAliases.getAliasesAndSubtractFrom(seenAvailableLoadedSymbolReferences);
              seenAvailableLoadedSymbolReferences |= tmp;
              }

           if (alreadyKilledAtVolatileLoad) // we want to keep the last load of a particular sym ref alive but no earlier loads should surivive
              seenAvailableLoadedSymbolReferences[symRef->getReferenceNumber()]=true;
           }
        }
     else
        {
        // There are no use def aliases
        //
        int32_t symRefNumber = symRef->getReferenceNumber();

        // Kill slow copy propagation info
        //
        if (_seenSymRefs.ValueAt(symRefNumber))
           {
           CS2::TableOf<TR::Node *, TR::Allocator>::Cursor nc(_storeNodes);
           for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
              {
              int32_t storeSymRefNum = _storeNodes[nc]->getSymbolReference()->getReferenceNumber();
              if (symRefNumber == storeSymRefNum)
                 {
                 _storeNodes.RemoveEntry(nc);
                 _storeNodesIndex[storeSymRefNum]=0;
                 }
              }
           }

        // Kill fast availability info for commoning
        //
        if (seenAvailableLoadedSymbolReferences.ValueAt(symRef->getReferenceNumber()))
           previouslyAvailable = true;
        seenAvailableLoadedSymbolReferences[symRef->getReferenceNumber()]=false;
        }

      // Internal pointers should be killed if the pinning array was killed
      //
      if (symRef && symRef->getSymbol()->isAuto() &&
         _availablePinningArrayExprs.ValueAt(symRef->getReferenceNumber()))
         {
         killAllInternalPointersBasedOnThisPinningArray(symRef);
         }

      // Only go into killing available expressions (i.e. slow availability info for commoning) if the symbol
      // is available now; otherwise there is nothing to kill.
      //
      if (previouslyAvailable)
         {
         if (hasAliases)
            killAvailableExpressionsUsingAliases(symRefNum, UseDefAliases);
         if (node->getOpCode().isLikeDef())
            killAvailableExpressions(symRefNum);
         }

      // Step 3 : add this node as a copy propagation candidate if it is a store
      //
      if (node->getOpCode().isStore())
         {
         uint32_t storeIndex = _storeNodesIndex[symRefNum];
         if (storeIndex)
            _storeNodes[storeIndex] = node;
         else
            _storeNodesIndex[symRefNum] = _storeNodes.AddEntry(node);
         }

      if (symRef && !node->getOpCode().isCall())
         _seenSymRefs[symRef->getReferenceNumber()]=true;
      }

   killAvailableExpressionsAtGCSafePoints(node, parent, seenAvailableLoadedSymbolReferences);

   if (_isTreeTopNullCheck)
      {
      if (nullCheckReference != NULL)
         _inSubTreeOfNullCheckReference = false;
      }

   if (node->getOpCode().isCall())
      {
      seenAvailableLoadedSymbolReferences[node->getSymbolReference()->getReferenceNumber()]=true;
      // pure calls can be commoned so we must observe kills
      if (!node->isPureCall())
         _seenCallSymbolReferences[node->getSymbolReference()->getReferenceNumber()]=true;;
      }

   static char *verboseProcessing = feGetEnv("TR_VerboseLocalCSEAvailableSymRefs");
   if (verboseProcessing)
      {
      traceMsg(comp(), "  after n%dn [%p] seenAvailableLoadedSymbolReferences:", node->getGlobalIndex(), node);
      *(comp()) << seenAvailableLoadedSymbolReferences;
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
          (_volatileState == VOLATILE_ONLY && node->getSymbol()->isVolatile()) ||
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

            if (node->getSideTableIndex() != REPLACE_MARKER)
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
               if (node->getSideTableIndex() != REPLACE_MARKER)
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
         if ((parent != NULL || !node->getOpCode().isResolveOrNullCheck()) &&
             !_simulatedNodesAsArray[node->getGlobalIndex()] &&
             !node->getOpCode().isCase() && node->getReferenceCount() > 1)
            {
            _replacedNodesAsArray[_nextReplacedNode] = node;
            _replacedNodesByAsArray[_nextReplacedNode++] = availableExpression;
            // if (trace())
            //    traceMsg(comp(), "Replaced node : %p Replacing node : %p\n", node, availableExpression);
            doneCommoning = true;
            }

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
void OMR::LocalCSE::makeNodeAvailableForCommoning(TR::Node *parent, TR::Node *node, SharedSparseBitVector &seenAvailableLoadedSymbolReferences, bool *canBeAvailable)
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
      if (!seenAvailableLoadedSymbolReferences.ValueAt(node->getSymbolReference()->getReferenceNumber()))
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
            seenAvailableLoadedSymbolReferences[symRef->getReferenceNumber()]=true;
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
            if (convOp == TR::v2v)
               convNode = TR::Node::createVectorConversion(rhsOfStoreDefNode, node->getDataType());
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
                  TR_ASSERT(rhsOfStoreDefNode->getDataType() == TR::PackedDecimal,"rhsOfStoreDefNode %p (type %d) must be pd type if defNode %p is pd\n",
                     rhsOfStoreDefNode,rhsOfStoreDefNode->getDataType(),defNode);
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

   node->setSideTableIndex(0);

   if (node->getOpCode().hasSymbolReference())
      {
      if (_possiblyRelevantNodes.ValueAt(node->getSymbolReference()->getReferenceNumber()))
         _relevantNodes[node->getSymbolReference()->getReferenceNumber()]=true;
      _possiblyRelevantNodes[node->getSymbolReference()->getReferenceNumber()]=true;
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
bool OMR::LocalCSE::canBeAvailable(TR::Node *parent, TR::Node *node, SharedSparseBitVector &seenAvailableLoadedSymbolReferences, bool canBeAvailable)
   {
  if (!canBeAvailable)
      return false;

   if (node->getOpCode().isBranch() || (node->getOpCode().isCase()))
      return false;

   if (!shouldCommonNode(parent, node))
      return false;

   if (node->getOpCodeValue() == TR::allocationFence)
      return false;

   if (node->getOpCode().isLoadReg() || node->getOpCode().isStoreReg() || (node->getOpCodeValue() == TR::PassThrough && parent->getOpCodeValue() != TR::GlRegDeps) || (node->getOpCodeValue() == TR::GlRegDeps))
      return false;

   if (node->getOpCode().hasSymbolReference())
      {
      // We can allow final non volatile fields to be commoned even during the volatile only phase
      // This is because those are the only fields we can rely on not changing and therefore we can
      // common volatiles that are based on an indirection chain of such final non volatiles (or autos or parms that are not global by definition)
      // Any non volatile field that could change is a problem since it could be proven that a later volatile load
      // based on an indirection chain involving a non final, non volatile variable must be different than an
      // earlier volatile load based on the same indirection chain.
      //
      if ((!seenAvailableLoadedSymbolReferences.ValueAt(node->getSymbolReference()->getReferenceNumber())) ||
          ((_volatileState == VOLATILE_ONLY) && !node->getSymbol()->isAutoOrParm() && !node->getSymbol()->isVolatile() && !node->getSymbol()->isFinal()))
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

      if (!_parentAddedToHT.ValueAt(node->getChild(i)->getGlobalIndex()))
         return false;

      i++;
      }


   return true;
   }

// Performs 'fast' availability check for NULLCHK nodes
//
bool OMR::LocalCSE::isAvailableNullCheck(TR::Node *node, SharedSparseBitVector &seenAvailableLoadedSymbolReferences)
   {
   if (node->getOpCode().hasSymbolReference())
      {
      if (!seenAvailableLoadedSymbolReferences.ValueAt(node->getSymbolReference()->getReferenceNumber()))
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
      *(comp()) << _availableCallExprs;
      traceMsg(comp(),"\n");
      }

   int32_t hashValue = hash(parent, node);
   HashTableEntry *firstEntry;
   if (node->getOpCode().hasSymbolReference() && ((node->getOpCodeValue() != TR::loadaddr) || _loadaddrAsLoad))
      {
      if (node->getOpCode().isCall())
         {
         firstEntry = _hashTableWithCalls._buckets[hashValue];
         }
      else
         {
         firstEntry = _hashTableWithSyms._buckets[hashValue];
         }
      }
   else if (node->getOpCode().isLoadConst())
      firstEntry = _hashTableWithConsts._buckets[hashValue];
   else
      firstEntry = _hashTable._buckets[hashValue];

   if (!(firstEntry == NULL))
      {
      HashTableEntry *entry;
      HashTableEntry *prevEntry = firstEntry;
      for (entry = firstEntry->_next; entry != firstEntry; entry = entry->_next)
         {
         TR::Node *other = entry->_node;
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
               traceMsg(comp(), "remove is true, removing entry %p\n",entry->_node);
            prevEntry->_next = entry->_next;
            _killedNodes[other->getGlobalIndex()]=true;
            }
         else
            {
            prevEntry = entry;
            }
         }

      bool remove = false;
      if (areSyntacticallyEquivalent(entry->_node, node, &remove))
         {
         if (trace())
            traceMsg(comp(), "spot 2: node %p is syntactically equivalent to other %p\n",node,entry->_node);
         return entry->_node;
         }
      if (trace())
         traceMsg(comp(), "spot 2: remove = %d\n",remove);
      }

   if ((node->getOpCode().isArrayRef()) &&
       cg()->supportsInternalPointers() &&
       (node->getFirstChild()->getOpCodeValue() == TR::aload) &&
       (node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto()) &&
       !_killedPinningArrayExprs[node->getFirstChild()->getSymbolReference()->getReferenceNumber()])
      {
      ListIterator<TR::Node> arrayRefNodesIt(&_arrayRefNodes);
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
bool OMR::LocalCSE::killExpressionsIfVolatileLoad(TR::Node *node, SharedSparseBitVector &seenAvailableLoadedSymbolReferences, TR_NodeKillAliasSetInterface &UseDefAliases)
   {
   bool isVolatileRead = false;
   if (!node->getOpCode().isLikeDef() && node->mightHaveVolatileSymbolReference())
      isVolatileRead = true;

   if (isVolatileRead)
      {
      SharedSparseBitVector tmp(comp()->allocator());
      seenAvailableLoadedSymbolReferences.Andc(_seenCallSymbolReferences,tmp);
      if (_volatileState != VOLATILE_ONLY)
         {
         if (node->getOpCode().hasSymbolReference() && UseDefAliases.containsAny(tmp, comp()))
            killAvailableExpressionsUsingAliases(node->getSymbolReference()->getReferenceNumber(), UseDefAliases);
         }
      else
         {
         SharedSparseBitVector aliases(comp()->allocator());
         SharedSparseBitVector processedAliases(comp()->allocator());
         UseDefAliases.getAliases(aliases);
         SharedSparseBitVector::Cursor c(aliases);
         for (c.SetToFirstOne(); c.Valid(); c.SetToNextOne())
            {
            if (comp()->getSymRefTab()->getSymRef(c)->maybeVolatile())
               processedAliases[c] = 1;
            }
         if (node->getOpCode().hasSymbolReference() && processedAliases.Intersects(tmp))
            killAvailableExpressionsUsingAliases(node->getSymbolReference()->getReferenceNumber(), processedAliases);
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
   _hashTable._buckets[0] = NULL;
   _hashTableWithSyms._buckets[0] = NULL;
   _availableLoadExprs[0]=false;
   _availablePinningArrayExprs[0]=false;
   _availableCallExprs[0]=false;
   _hashTableWithConsts._buckets[0] = NULL;
   _hashTableWithCalls._buckets[0] = NULL;
   }

void OMR::LocalCSE::killAllInternalPointersBasedOnThisPinningArray(TR::SymbolReference *symRef)
   {
   ListIterator<TR::Node> arrayRefNodesIt(&_arrayRefNodes);
   TR::Node *arrayRefNode = arrayRefNodesIt.getFirst();
   for (;arrayRefNode; arrayRefNode = arrayRefNodesIt.getNext())
      {
      if ((arrayRefNode->getNumChildren() > 0) &&
          (arrayRefNode->getFirstChild()->getOpCodeValue() == TR::aload) &&
          (arrayRefNode->getFirstChild()->getSymbolReference()->getSymbol()->isAuto()) &&
          (arrayRefNode->getFirstChild()->getSymbolReference() == symRef))
         {
         _arrayRefNodes.remove(arrayRefNode);
         _killedPinningArrayExprs[symRef->getReferenceNumber()] = true;
         }
      }
   }

// Kill expressions containing this symbol
//
void OMR::LocalCSE::killAvailableExpressions(int32_t symRefNum)
   {
   _hashTableWithSyms._buckets[symRefNum] = NULL;
   _availableLoadExprs[symRefNum]=false;
   _availablePinningArrayExprs[symRefNum]=false;
   _availableCallExprs[symRefNum]=false;
   }


void OMR::LocalCSE::killAvailableExpressionsUsingAliases(int32_t symRefNum, SharedSparseBitVector &aliases)
   {
   SharedSparseBitVector tmp(_availableLoadExprs);
   _availableLoadExprs -= aliases;

   tmp.Andc(_availableLoadExprs);

   SharedSparseBitVector::Cursor c(tmp);
   for (c.SetToFirstOne(); c.Valid(); c.SetToNextOne())
      {
      int32_t nextSymRefNum = c;

      if (_hashTableWithSyms._buckets[nextSymRefNum] != NULL)
         {
         _killedNodes[_hashTableWithSyms._buckets[nextSymRefNum]->_node->getGlobalIndex()]=true;
         _hashTableWithSyms._buckets[nextSymRefNum] = NULL;
         }
      }

   SharedSparseBitVector tmp2(_availableCallExprs);
   _availableCallExprs -= aliases;
   tmp2.Andc(_availableCallExprs);
   SharedSparseBitVector::Cursor c2(tmp2);
    for(c2.SetToFirstOne(); c2.Valid(); c2.SetToNextOne())
       {
       int32_t nextSymRefNum = c2;

       if (_hashTableWithCalls._buckets[nextSymRefNum] != NULL)
          {
          _killedNodes[_hashTableWithCalls._buckets[nextSymRefNum]->_node->getGlobalIndex()]=true;
          _hashTableWithCalls._buckets[nextSymRefNum] = NULL;
          }
       }
   }



void OMR::LocalCSE::killAvailableExpressionsUsingAliases(int32_t symRefNum, TR_NodeKillAliasSetInterface &UseDefAliases)
   {
   SharedSparseBitVector tmp(_availableLoadExprs);
   UseDefAliases.getAliasesAndSubtractFrom(_availableLoadExprs);
   UseDefAliases.getAliasesAndSubtractFrom(_availablePinningArrayExprs);
   tmp.Andc(_availableLoadExprs);

   SharedSparseBitVector::Cursor c(tmp);
   for(c.SetToFirstOne(); c.Valid(); c.SetToNextOne()) {
      int32_t nextSymRefNum = c;
      if (_hashTableWithSyms._buckets[nextSymRefNum] != NULL){
         _killedNodes[_hashTableWithSyms._buckets[nextSymRefNum]->_node->getGlobalIndex()]=true;
         _hashTableWithSyms._buckets[nextSymRefNum] = NULL;
      }
   }

   SharedSparseBitVector tmp2(_availableCallExprs);
   UseDefAliases.getAliasesAndSubtractFrom(_availableCallExprs);
   tmp2.Andc(_availableCallExprs);

   SharedSparseBitVector::Cursor c2(tmp2);
    for(c2.SetToFirstOne(); c2.Valid(); c2.SetToNextOne())
       {
       int32_t nextSymRefNum = c2;
       if (_hashTableWithCalls._buckets[nextSymRefNum] != NULL)
          {
          _killedNodes[_hashTableWithCalls._buckets[nextSymRefNum]->_node->getGlobalIndex()]=true;
          _hashTableWithCalls._buckets[nextSymRefNum] = NULL;
          }
      }
   }


void OMR::LocalCSE::killAllDataStructures(SharedSparseBitVector &seenAvailableLoadedSymbolReferences)
   {
   _storeNodes.MakeEmpty();
   _storeNodesIndex.ShrinkTo(0);

   seenAvailableLoadedSymbolReferences.Truncate();

   int32_t i;
   for (i=0;i<_hashTable._numBuckets;i++)
      _hashTable._buckets[i] = NULL;

   _availableLoadExprs.Clear();
   _availablePinningArrayExprs.Clear();
   _availableCallExprs.Clear();

   for (i=0;i<_hashTableWithSyms._numBuckets;i++)
      _hashTableWithSyms._buckets[i] = NULL;

   for (i=0;i<_hashTableWithConsts._numBuckets;i++)
      _hashTableWithConsts._buckets[i] = NULL;

   for (i=0;i<_hashTableWithCalls._numBuckets;i++)
      _hashTableWithCalls._buckets[i] = NULL;

   killAllAvailableExpressions();
   }

// If this is a GC safe point, kill the address type available expressions that are aload,
// aiadd (or aladd) or that contain an aiadd (or an aladd).

void OMR::LocalCSE::killAvailableExpressionsAtGCSafePoints(TR::Node *node, TR::Node *parent, SharedSparseBitVector &seenAvailableLoadedSymbolReferences)
   {
   // A GC safe point can only occur at a treetop.
   //
   if (parent != NULL)
      return;

#ifdef J9_PROJECT_SPECIFIC

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

      int32_t i;
      _storeNodes.MakeEmpty();
      _storeNodesIndex.ShrinkTo(0);

      seenAvailableLoadedSymbolReferences.Truncate();

      for (i=0;i<_hashTable._numBuckets;i++)
         _hashTable._buckets[i] = NULL;

      _availableLoadExprs.Clear();
      _availablePinningArrayExprs.Clear();
      _availableCallExprs.Clear();

      for (i=0;i<_hashTableWithSyms._numBuckets;i++)
         _hashTableWithSyms._buckets[i] = NULL;

      for (i=0;i<_hashTableWithConsts._numBuckets;i++)
         _hashTableWithConsts._buckets[i] = NULL;

      for (i=0;i<_hashTableWithCalls._numBuckets;i++)
         _hashTableWithCalls._buckets[i] = NULL;
      return;
      }

#endif

   if (node->canGCandReturn())
      {
      if (trace())
         traceMsg(comp(), "Node %p is detected as a GC safe point\n", node);

      CS2::TableOf<TR::Node *, TR::Allocator>::Cursor nc(_storeNodes);

      for (nc.SetToFirst(); nc.Valid(); nc.SetToNext())
         {
         TR::Node *storeNode = _storeNodes[nc];
         int32_t childAdjust = (storeNode->getOpCode().isWrtBar()) ? 2 : 1;

         TR::Node *rhsOfStoreDefNode = storeNode->getChild(storeNode->getNumChildren()-childAdjust);
         if (rhsOfStoreDefNode->getOpCode().isArrayRef())
            {
            int32_t storeSymRefNum = storeNode->getSymbolReference()->getReferenceNumber();
            _storeNodes.RemoveEntry(nc);
            _storeNodesIndex[storeSymRefNum] = 0;
            }
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

   if (node->getOpCode().hasSymbolReference() &&
       node->getOpCode().isLoadVar() || node->getOpCode().isCall())
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
         h += (int32_t)(intptrj_t)child->getSymbolReference()->getReferenceNumber();
      else
         h++;

      g = h & 0xF0000000;
      h ^= g >> 24;
      }

   int32_t hashValue;
      {
      if (node->getOpCode().hasSymbolReference() &&
          ((node->getOpCodeValue() != TR::loadaddr) || _loadaddrAsLoad))
         {
         if (node->getOpCode().isCall())
            {
            hashValue = 1 + ((h ^ g) % (_hashTableWithCalls._numBuckets-1));
            TR_ASSERT(hashValue >= 0 && hashValue < _hashTableWithCalls._numBuckets, "LocalCSE hash problem - hashValue %d on node %p %s outside of _hashTableWithCalls size %d\n",hashValue,node,node->getOpCode().getName(),_hashTableWithCalls._numBuckets);
            }
         else
            {
            hashValue = 1 + ((h ^ g) % (_hashTableWithSyms._numBuckets-1));
            TR_ASSERT(hashValue >= 0 && hashValue < _hashTableWithSyms._numBuckets, "LocalCSE hash problem - hashValue %d on node %p %s outside of _hashTableWithSyms size %d\n",hashValue,node,node->getOpCode().getName(),_hashTableWithSyms._numBuckets);
            }
         }
      else if (node->getOpCode().isLoadConst())
         {
         hashValue = 1 + (( (h ^ g) + (int32_t)node->getConstValue()) % (_hashTableWithConsts._numBuckets-1));
         TR_ASSERT(hashValue >= 0 && hashValue < _hashTableWithConsts._numBuckets, "LocalCSE hash problem - hashValue %d on node %p %s outside of _hashTableWithConsts size %d\n",hashValue,node,node->getOpCode().getName(),_hashTableWithConsts._numBuckets);
         }
      else
         {
         hashValue = 1 + ((h ^ g) % (_hashTable._numBuckets-1));
         TR_ASSERT(hashValue >= 0 && hashValue < _hashTable._numBuckets, "LocalCSE hash problem - hashValue %d on node %p %s outside of _hashTable size %d\n",hashValue,node,node->getOpCode().getName(),_hashTable._numBuckets);
         }
      }

   return hashValue;
   }

void OMR::LocalCSE::addToHashTable(TR::Node *node, int32_t hashValue)
   {
   if (node->getOpCode().isStore() ||
       (node->getOpCode().isVoid() && (node->getOpCodeValue() != TR::PassThrough)))
      return;

   if (node->getOpCode().hasSymbolReference() &&
       !_relevantNodes.ValueAt(node->getSymbolReference()->getReferenceNumber()))
      return;

   int32_t numChildren = node->getNumChildren();
   for (int32_t i = numChildren-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      _parentAddedToHT[child->getGlobalIndex()]=1;
      }

   if ((node->getOpCode().isArrayRef()) &&
       cg()->supportsInternalPointers() &&
       (node->getFirstChild()->getOpCodeValue() == TR::aload) &&
       (node->getFirstChild()->getSymbolReference()->getSymbol()->isAuto()))
      {
      _availablePinningArrayExprs[node->getFirstChild()->getSymbolReference()->getReferenceNumber()] = true;
      _arrayRefNodes.add(node);
      }

   HashTableEntry *entry = (HashTableEntry*)trMemory()->allocateStackMemory(sizeof(HashTableEntry));
   entry->_node = node;

   HashTableEntry *prevEntry;
   bool hasSymRef = false;
   if (node->getOpCode().hasSymbolReference())
      hasSymRef = true;

   if (hasSymRef && ((node->getOpCodeValue() != TR::loadaddr) || _loadaddrAsLoad))
      {
      if (node->getOpCode().isCall())
         {
         prevEntry = _hashTableWithCalls._buckets[hashValue];
         _availableCallExprs[node->getSymbolReference()->getReferenceNumber()]=true;
         }
      else
         {
         prevEntry = _hashTableWithSyms._buckets[hashValue];
         _availableLoadExprs[node->getSymbolReference()->getReferenceNumber()]=true;
         }
      }
   else if (node->getOpCode().isLoadConst())
      prevEntry = _hashTableWithConsts._buckets[hashValue];
   else
      prevEntry = _hashTable._buckets[hashValue];

   if (!(prevEntry == NULL))
      {
      entry->_next = prevEntry->_next;
      prevEntry->_next = entry;
      }
   else
      entry->_next = entry;

   if (hasSymRef && ((node->getOpCodeValue() != TR::loadaddr) || _loadaddrAsLoad))
      {
      if (node->getOpCode().isCall())
         {
         _hashTableWithCalls._buckets[hashValue] = entry;
         _availableCallExprs[node->getSymbolReference()->getReferenceNumber()]=true;
         }
      else
         {
         _hashTableWithSyms._buckets[hashValue] = entry;
         _availableLoadExprs[node->getSymbolReference()->getReferenceNumber()]=true;
         }
      }
   else if (node->getOpCode().isLoadConst())
      _hashTableWithConsts._buckets[hashValue] = entry;
   else
      _hashTable._buckets[hashValue] = entry;
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

      if (_killedNodes.ValueAt(child1->getGlobalIndex()))
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

               if (_killedNodes.ValueAt(grandChild1->getGlobalIndex()))
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

      node->setSideTableIndex(REPLACE_MARKER);
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


void OMR::LocalCSE::setPreviousConversion(TR::Node *storeNode, TR::Node *convertedNode, TR::SymbolReference *symRef)
   {
   uint32_t hash = TR_PNC_HASH(storeNode) & (TR_PNC_BUCKETS - 1);

   OMR::PreviousNodeConversion *cursor = _previousNodeConversionsTable[hash];
   OMR::PreviousNodeConversion *prev = 0;

   for (; cursor; prev = cursor, cursor = cursor->next)
      {
      if (cursor->getConvertedStoreNode() == storeNode)
         {
         cursor->addConvertedNode(convertedNode, symRef);
         }
      }

   if (!cursor)
      {
      cursor = new (trHeapMemory()) OMR::PreviousNodeConversion(trMemory(), storeNode);
      cursor->addConvertedNode(convertedNode, symRef);
      cursor->next = 0;
      if (prev)
        prev->next = cursor;
      else
        _previousNodeConversionsTable[hash] = cursor;
      }
   }

TR::Node *OMR::LocalCSE::getPreviousConversion(TR::Node *storeNode, TR::SymbolReference *symRef)
   {
   uint32_t hash = TR_PNC_HASH(storeNode) & (TR_PNC_BUCKETS - 1);

   OMR::PreviousNodeConversion *cursor = _previousNodeConversionsTable[hash];
   TR::Node *ret = NULL;

   for (; cursor; cursor = cursor->next)
      {
      if (cursor->getConvertedStoreNode() == storeNode)
         {
         ret = cursor->findConvertedNodeForSymRef(symRef);
         break;
         }
      }

   return ret;
   }
