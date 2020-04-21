/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#include "optimizer/LocalAnalysis.hpp"

#include <stdint.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "cs2/bitvectr.h"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
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
#include "infra/Cfg.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "optimizer/Optimizer.hpp"
#include "ras/Debug.hpp"


class TR_OpaqueClassBlock;

bool TR_LocalAnalysis::isSupportedNodeForFieldPrivatization(TR::Node *node, TR::Compilation *comp, TR::Node *parent)
   {
   // types of stores that may be supported by field privatization
   bool isSupportedStoreNode =
         node->getOpCode().isStore() && node->getOpCodeValue() != TR::astorei;

   return isSupportedNode(node, comp, parent, isSupportedStoreNode);
   }

bool TR_LocalAnalysis::isSupportedNode(TR::Node *node, TR::Compilation *comp, TR::Node *parent, bool isSupportedStoreNode)
   {
   return isSupportedNodeForFunctionality(node, comp, parent, isSupportedStoreNode) &&
          isSupportedNodeForPREPerformance(node, comp, parent);
   }

bool TR_LocalAnalysis::isSupportedNodeForPREPerformance(TR::Node *node, TR::Compilation *comp, TR::Node *parent)
   {
   TR::SymbolReference *symRef = node->getOpCode().hasSymbolReference()?node->getSymbolReference():NULL;
   if (node->getOpCode().isStore() && symRef &&
       symRef->getSymbol()->isAutoOrParm())
      {
      //dumpOptDetails("Returning false for store %p\n", node);
      return false;
      }

   if (node->getOpCode().isLoadConst() && !comp->cg()->isMaterialized(node))
      {
      return false;
      }

   if (node->getOpCode().hasSymbolReference() &&
       (node->getSymbolReference() == comp->getSymRefTab()->findJavaLangClassFromClassSymbolRef()))
      {
      return false;
      }

   return true;
   }


bool TR_LocalAnalysis::isSupportedNodeForFunctionality(TR::Node *node, TR::Compilation *comp, TR::Node *parent, bool isSupportedStoreNode)
   {
#if 0
   // This needs to be tested more. The idea is to check parent when making PRE decision
   if (node->getOpCode().isLoadConst() &&
       (parent->getOpCode().isStore()))
      return true;
#endif

   if (parent &&
       (parent->getOpCodeValue() == TR::Prefetch) &&
       (node->getOpCodeValue() == TR::aloadi))
      return false;

   if (node->isThisPointer() && !node->isNonNull())
      return false;

   if ((node->getOpCodeValue() == TR::a2l) || (node->getOpCodeValue() == TR::a2i))
      return false;

   if (node->getOpCode().isSpineCheck() /* &&
                                           node->getFirstChild()->getOpCode().isStore() */ )
      return false;

   if (comp->requiresSpineChecks() &&
       node->getOpCode().hasSymbolReference() &&
       node->getSymbol()->isArrayShadowSymbol())
      return false;

   if (node->getOpCode().isCall() &&
       !node->getSymbolReference()->isUnresolved() &&
       node->getSymbol()->castToMethodSymbol()->isPureFunction() &&
       (node->getDataType() != TR::NoType))
      return true;

   if (node->getOpCode().hasSymbolReference() &&
       (node->getSymbolReference()->isSideEffectInfo() ||
        node->getSymbolReference()->isOverriddenBitAddress() ||
        node->getSymbolReference()->isUnresolved()))
      return false;

   if (isSupportedOpCode(node->getOpCode(), comp) || isSupportedStoreNode || node->getOpCode().isLoadConst())
       {
       if (node->getDataType() == TR::Address)
          {
          if (!node->addressPointsAtObject())
             return false;
          }
       return true;
       }

    return false;
    }


TR_LocalAnalysisInfo::TR_LocalAnalysisInfo(TR::Compilation *c, bool t)
   : _compilation(c), _trace(t), _trMemory(c->trMemory())
   {
   _numNodes = -1;

#if 0  // somehow stops PRE from happening
   // We are going to increment visit count for every tree so can reach max
   // for big methods quickly. Perhaps can improve containsCall() in the future.
   comp()->resetVisitCounts(0);
#endif
   if (comp()->getVisitCount() > HIGH_VISIT_COUNT)
      {
      _compilation->resetVisitCounts(1);
      dumpOptDetails(comp(), "\nResetting visit counts for this method before LocalAnalysisInfo\n");
      }

   TR::CFG *cfg = comp()->getFlowGraph();
   _numBlocks = cfg->getNextNodeNumber();
   TR_ASSERT(_numBlocks > 0, "Local analysis, node numbers not assigned");

   // Allocate information on the stack. It is the responsibility of the user
   // of this class to determine the life of the information by using jitStackMark
   // and jitStackRelease.
   //
   //_blocksInfo = (TR::Block **) trMemory()->allocateStackMemory(_numBlocks*sizeof(TR::Block *));
   //memset(_blocksInfo, 0, _numBlocks*sizeof(TR::Block *));

   TR::TreeTop *currentTree = comp()->getStartTree();

   // Only do this if not done before; typically this would be done in the
   // first call to this method through LocalTransparency and would NOT
   // need to be re-done by LocalAnticipatability.
   //
   if (_numNodes < 0)
      {
      _optimizer = comp()->getOptimizer();

      int32_t numBuckets;
      int32_t numNodes = comp()->getNodeCount();
      if (numNodes < 10)
         numBuckets = 1;
      else if (numNodes < 100)
         numBuckets = 7;
      else if (numNodes < 500)
         numBuckets = 31;
      else if (numNodes < 3000)
         numBuckets = 127;
      else if (numNodes < 6000)
         numBuckets = 511;
      else
         numBuckets = 1023;

      // Allocate hash table for matching expressions
      //
      HashTable hashTable(numBuckets, comp());
      _hashTable = &hashTable;

      // Null checks are handled differently as the criterion for
      // commoning a null check is different than that used for
      // other nodes; for a null check, the null check reference is
      // important (and not the actual indirect access itself)
      //
      _numNullChecks = 0;
      while (currentTree)
         {
         if (currentTree->getNode()->getOpCodeValue() == TR::NULLCHK)
         //////if (currentTree->getNode()->getOpCode().isNullCheck())
            _numNullChecks++;

         currentTree = currentTree->getNextTreeTop();
         }

      if (_numNullChecks == 0)
         _nullCheckNodesAsArray = NULL;
      else
         {
         _nullCheckNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(_numNullChecks*sizeof(TR::Node*));
         memset(_nullCheckNodesAsArray, 0, _numNullChecks*sizeof(TR::Node*));
         }

      currentTree = comp()->getStartTree();
      int32_t symRefCount = comp()->getSymRefCount();
      _checkSymbolReferences = new (trStackMemory()) TR_BitVector(symRefCount, trMemory(), stackAlloc);

      _numNodes = 1;
      _numNullChecks = 0;

      // This loop counts all the nodes that are going to take part in PRE.
      // This is a computation intensive loop as we check if the node that
      // is syntactically equivalent to a given node has been seen before
      // and if so we use the local index of the original node (that
      // is syntactically equivalent to the given node). Could be improved
      // in complexity with value numbering at some stage.
      //
      _visitCount = comp()->incVisitCount();
      while (currentTree)
         {
         TR::Node *firstNodeInTree = currentTree->getNode();
         TR::ILOpCode *opCode = &firstNodeInTree->getOpCode();

         if (((firstNodeInTree->getOpCodeValue() == TR::treetop) ||
              (comp()->useAnchors() && firstNodeInTree->getOpCode().isAnchor())) &&
             (firstNodeInTree->getFirstChild()->getOpCode().isStore()))
            {
            firstNodeInTree->setLocalIndex(-1);
            if (comp()->useAnchors() && firstNodeInTree->getOpCode().isAnchor())
               firstNodeInTree->getSecondChild()->setLocalIndex(-1);

            firstNodeInTree = firstNodeInTree->getFirstChild();
            opCode = &firstNodeInTree->getOpCode();
            }

         // This call finds nodes with opcodes that are supported by PRE
         // in this subtree; this accounts for all opcodes other than stores/checks
         // which are handled later on below
         //
         bool firstNodeInTreeHasCallsInStoreLhs = false;
         countSupportedNodes(firstNodeInTree, NULL, firstNodeInTreeHasCallsInStoreLhs);

         if ((opCode->isStore() && !firstNodeInTree->getSymbolReference()->getSymbol()->isAutoOrParm()) ||
             opCode->isCheck())
            {
            int32_t oldExpressionOnRhs = hasOldExpressionOnRhs(firstNodeInTree);

            //
            // Return value 0 denotes that the node contains some sub-expression
            // that cannot participate in PRE; e.g. a call or a new
            //
            // Return value -1 denotes that the node can participate in PRE
            // but did not match with any existing expression seen so far
            //
            // Any other return value (should be positive always) denotes that
            // the node can participate in PRE and has been matched with a seen
            // expression having local index == return value
            //
            if (oldExpressionOnRhs == -1)
               {
               if (trace())
                  {
                  traceMsg(comp(), "\nExpression #%d is : \n", _numNodes);
                  comp()->getDebug()->print(comp()->getOutFile(), firstNodeInTree, 6, true);
                  }

               firstNodeInTree->setLocalIndex(_numNodes++);
               }
            else
               firstNodeInTree->setLocalIndex(oldExpressionOnRhs);

            if (opCode->isCheck() &&
                (firstNodeInTree->getFirstChild()->getOpCode().isStore() &&
                 !firstNodeInTree->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm()))
               {
               int oldExpressionOnRhs = hasOldExpressionOnRhs(firstNodeInTree->getFirstChild());

               if (oldExpressionOnRhs == -1)
                  {
                  if (trace())
                     {
                     traceMsg(comp(), "\nExpression #%d is : \n", _numNodes);
                     comp()->getDebug()->print(comp()->getOutFile(), firstNodeInTree->getFirstChild(), 6, true);
                     }

                  firstNodeInTree->getFirstChild()->setLocalIndex(_numNodes++);
                  }
               else
                  firstNodeInTree->getFirstChild()->setLocalIndex(oldExpressionOnRhs);
               }
            }
         else
            firstNodeInTree->setLocalIndex(-1);

         currentTree = currentTree->getNextTreeTop();
         }
      }

   _supportedNodesAsArray = (TR::Node**)trMemory()->allocateStackMemory(_numNodes*sizeof(TR::Node*));
   memset(_supportedNodesAsArray, 0, _numNodes*sizeof(TR::Node*));
   _checkExpressions = new (trStackMemory()) TR_BitVector(_numNodes, trMemory(), stackAlloc);

   //_checkExpressions.init(_numNodes, trMemory(), stackAlloc);

   // This loop goes through the trees and collects the nodes
   // that would take part in PRE. Each node has its local index set to
   // the bit position that it occupies in the bit vector analyses.
   //
   currentTree = comp()->getStartTree();
   _visitCount = comp()->incVisitCount();
   while (currentTree)
      {
      TR::Node *firstNodeInTree = currentTree->getNode();
      TR::ILOpCode *opCode = &firstNodeInTree->getOpCode();

      if (((firstNodeInTree->getOpCodeValue() == TR::treetop) ||
           (comp()->useAnchors() && firstNodeInTree->getOpCode().isAnchor())) &&
          (firstNodeInTree->getFirstChild()->getOpCode().isStore()))
         {
         firstNodeInTree = firstNodeInTree->getFirstChild();
         opCode = &firstNodeInTree->getOpCode();
         }

      collectSupportedNodes(firstNodeInTree, NULL);

      if ((opCode->isStore() && !firstNodeInTree->getSymbolReference()->getSymbol()->isAutoOrParm()) ||
          opCode->isCheck())
         {
        if (opCode->isCheck())
            {
            _checkSymbolReferences->set(firstNodeInTree->getSymbolReference()->getReferenceNumber());
            _checkExpressions->set(firstNodeInTree->getLocalIndex());
            }

         if (!_supportedNodesAsArray[firstNodeInTree->getLocalIndex()])
            _supportedNodesAsArray[firstNodeInTree->getLocalIndex()] = firstNodeInTree;

         if (opCode->isCheck() &&
             firstNodeInTree->getFirstChild()->getOpCode().isStore() &&
             !firstNodeInTree->getFirstChild()->getSymbolReference()->getSymbol()->isAutoOrParm() &&
             !_supportedNodesAsArray[firstNodeInTree->getFirstChild()->getLocalIndex()])
            _supportedNodesAsArray[firstNodeInTree->getFirstChild()->getLocalIndex()] = firstNodeInTree->getFirstChild();
         }

      currentTree = currentTree->getNextTreeTop();
      }

   //initialize(toBlock(cfg->getStart()));
   }




TR_LocalAnalysis::TR_LocalAnalysis(TR_LocalAnalysisInfo &info, bool trace)
   : _lainfo(info), _trace(trace)
   {
   _registersScarce = false; //comp()->cg()->areAssignableGPRsScarce();
   }



void TR_LocalAnalysis::initializeLocalAnalysis(bool isSparse, bool lock)
   {
   _info = (TR_LocalAnalysisInfo::LAInfo*) trMemory()->allocateStackMemory(_lainfo._numBlocks*sizeof(TR_LocalAnalysisInfo::LAInfo));
   memset(_info, 0, _lainfo._numBlocks*sizeof(TR_LocalAnalysisInfo::LAInfo));

   TR::BitVector blocksSeen(comp()->allocator());
   initializeBlocks(toBlock(comp()->getFlowGraph()->getStart()), blocksSeen);

   int32_t i;
   for (i = 0; i < _lainfo._numBlocks; i++)
      {
      _info[i]._analysisInfo = allocateContainer(getNumNodes());
      _info[i]._downwardExposedAnalysisInfo = allocateContainer(getNumNodes());
      _info[i]._downwardExposedStoreAnalysisInfo = allocateContainer(getNumNodes());
      }
   }




void TR_LocalAnalysis::initializeBlocks(TR::Block *block, TR::BitVector &blocksSeen)
   {
   _info[block->getNumber()]._block = block;
   blocksSeen[block->getNumber()] = true;

   TR::Block                *next;
   for (auto nextEdge = block->getSuccessors().begin(); nextEdge != block->getSuccessors().end(); ++nextEdge)
      {
      next = toBlock((*nextEdge)->getTo());
      if (!blocksSeen.ValueAt(next->getNumber()))
         initializeBlocks(next, blocksSeen);
      }
   for (auto nextEdge = block->getExceptionSuccessors().begin(); nextEdge != block->getExceptionSuccessors().end(); ++nextEdge)
      {
      next = toBlock((*nextEdge)->getTo());
      if (!blocksSeen.ValueAt(next->getNumber()))
         initializeBlocks(next, blocksSeen);
      }
   }





// Checks for syntactic equivalence and returns the side-table index
// of the syntactically equivalent node if it found one; else it returns
// -1 signifying that this is the first time any node similar syntactically
// to this node has been seen. Adds the node to the hash table if seen for the
// first time.
//
//
int TR_LocalAnalysisInfo::hasOldExpressionOnRhs(TR::Node *node, bool recalcContainsCall, bool storeLhsContainsCall)
   {
   //
   // Get the relevant portion of the subtree
   // for this node; this is different for a null check
   // as its null check reference is the only
   // sub-expression that matters
   //
   TR::Node *relevantSubtree = NULL;
   if (node->getOpCodeValue() == TR::NULLCHK)
      relevantSubtree = node->getNullCheckReference();
   else
      relevantSubtree = node;

   // containsCall checks whether the relevant node has some
   // sub-expression that cannot be commoned, e.g. call or a new
   //
   bool nodeContainsCall;
   if (!recalcContainsCall && (relevantSubtree == node))
      {
      // can use pre-calculated value of containsCall and storeLhsContainsCall, to avoid visitCount overflow
      nodeContainsCall = node->containsCall();
      }
   else
      {
      storeLhsContainsCall = false;
      nodeContainsCall = containsCall(relevantSubtree, storeLhsContainsCall);
      }

   if (nodeContainsCall)
      {
      //
      // If the node is not a store, a call-like sub-expression is inadmissable;
      // if the node is a store, a call-like sub-expression is allowed on the RHS
      // of the store as this does not inhibit privatization in any way as
      // the private temp store's RHS simply points at original RHS. But if a call-like
      // sub-expression is present in the LHS of the store, that is inadmissable
      //
      if (!node->getOpCode().isStore() ||
          storeLhsContainsCall)
         return 0;
      }

   bool seenIndirectStore = false;
#ifdef J9_PROJECT_SPECIFIC
   bool seenIndirectBCDStore = false;
#endif
   bool seenWriteBarrier = false;
   int32_t storeNumChildren = node->getNumChildren();


   // If node is a null check, compare the
   // null check reference only to establish syntactic equivalence
   //
   if (node->getOpCodeValue() == TR::NULLCHK)
   /////if (node->getOpCode().isNullCheck())
      {
      int32_t k;
      for (k=0;k<_numNullChecks;k++)
         {
         if (!(_nullCheckNodesAsArray[k] == NULL))
            {
            if (areSyntacticallyEquivalent(_nullCheckNodesAsArray[k]->getNullCheckReference(), node->getNullCheckReference()))
               return _nullCheckNodesAsArray[k]->getLocalIndex();
            }
         }

      _nullCheckNodesAsArray[_numNullChecks++] = node;
      }
   else
      {
      //
      // If this node is a global store, then equivalence check is different.
      // We try to give a store to field (or static) o.f the same index as
      // a load of o.f. This is so that privatization happens for fields/statics.
      // So the store's opcode value is changed temporarily to be a load before
      // syntactic equivalence is checked; this enables matching stores/loads to
      // same global symbol.
      //
      if (node->getOpCode().isStore() &&
          !node->getSymbolReference()->getSymbol()->isAutoOrParm())
         {
         if (node->getOpCode().isWrtBar())
            seenWriteBarrier = true;
#ifdef J9_PROJECT_SPECIFIC
         seenIndirectBCDStore = node->getType().isBCD();
#endif
         if (node->getOpCode().isStoreIndirect())
            {
            if (seenWriteBarrier)
               {
               TR::Node::recreate(node, _compilation->il.opCodeForIndirectArrayLoad(node->getDataType()));
               }
            else
               {
               TR::Node::recreate(node, _compilation->il.opCodeForCorrespondingIndirectStore(node->getOpCodeValue()));
               }
            node->setNumChildren(1);
            }
         else
            {
            TR::Node::recreate(node, _compilation->il.opCodeForDirectLoad(node->getDataType()));
            node->setNumChildren(0);
            }

#ifdef J9_PROJECT_SPECIFIC
         if (seenIndirectBCDStore)
            node->setBCDStoreIsTemporarilyALoad(true);
#endif

         seenIndirectStore = true;
         }

      int32_t hashValue = _hashTable->hash(node);
      HashTable::Cursor cursor(_hashTable, hashValue);
      TR::Node *other;
      for (other = cursor.firstNode(); other; other = cursor.nextNode())
         {
         // Convert other node's opcode to be a load temporarily
         // (only for syntactic equivalence check; see explanation above)
         // to enable matching global stores/loads.
         //
         bool seenOtherIndirectStore = false;
#ifdef J9_PROJECT_SPECIFIC
         bool seenOtherIndirectBCDStore = false;
#endif
         bool seenOtherWriteBarrier = false;
         int32_t otherStoreNumChildren = other->getNumChildren();
         if (other->getOpCode().isStore() &&
             !other->getSymbolReference()->getSymbol()->isAutoOrParm())
            {
            if (other->getOpCode().isWrtBar())
               seenOtherWriteBarrier = true;

#ifdef J9_PROJECT_SPECIFIC
            seenOtherIndirectBCDStore = other->getType().isBCD();
#endif
            if (other->getOpCode().isStoreIndirect())
               {
               if (seenOtherWriteBarrier)
                  {
                  TR::Node::recreate(other, _compilation->il.opCodeForIndirectArrayLoad(other->getDataType()));
                  }
               else
                  {
                  TR::Node::recreate(other, _compilation->il.opCodeForCorrespondingIndirectStore(other->getOpCodeValue()));
                  }
               other->setNumChildren(1);
               }
            else
               {
               TR::Node::recreate(other, _compilation->il.opCodeForDirectLoad(other->getDataType()));
               other->setNumChildren(0);
               }

#ifdef J9_PROJECT_SPECIFIC
            if (seenOtherIndirectBCDStore)
               other->setBCDStoreIsTemporarilyALoad(true);
#endif

            seenOtherIndirectStore = true;
            }

         bool areSame = areSyntacticallyEquivalent(node, other);

         // Restore the other node's state to what it was originally
         // (if it was a global store)
         //
         if (seenOtherWriteBarrier)
            {
            other->setNumChildren(otherStoreNumChildren);

            if (otherStoreNumChildren == 3)
               TR::Node::recreate(other, TR::awrtbari);
            else
               TR::Node::recreate(other, TR::awrtbar);
            }
         else if (seenOtherIndirectStore)
            {
            other->setNumChildren(otherStoreNumChildren);

#ifdef J9_PROJECT_SPECIFIC
            if (seenOtherIndirectBCDStore)
               other->setBCDStoreIsTemporarilyALoad(false);
#endif

            if (other->getOpCode().isIndirect())
               TR::Node::recreate(other, _compilation->il.opCodeForCorrespondingIndirectLoad(other->getOpCodeValue()));
            else
               TR::Node::recreate(other, _compilation->il.opCodeForDirectStore(other->getDataType()));
            }

         if (areSame)
            {
            if (seenWriteBarrier)
               {
               node->setNumChildren(storeNumChildren);

               if (storeNumChildren == 3)
                  TR::Node::recreate(node, TR::awrtbari);
               else
                  TR::Node::recreate(node, TR::awrtbar);
               }
            else if (seenIndirectStore)
               {
               node->setNumChildren(storeNumChildren);

#ifdef J9_PROJECT_SPECIFIC
               if (seenIndirectBCDStore)
                  node->setBCDStoreIsTemporarilyALoad(false);
#endif

               if (node->getOpCode().isIndirect())
                  TR::Node::recreate(node, _compilation->il.opCodeForCorrespondingIndirectLoad(node->getOpCodeValue()));
               else
                  TR::Node::recreate(node, _compilation->il.opCodeForDirectStore(node->getDataType()));
               }

            return other->getLocalIndex();
            }
         }

      // No match from existing nodes in the hash table;
      // add this node to the hash table.
      //
      _hashTable->add(node, hashValue);
      }

   // Restore this node's state to what it was before
   // (if it was a global store)
   //
   if (seenWriteBarrier)
      {
      node->setNumChildren(storeNumChildren);

      if (storeNumChildren == 3)
         TR::Node::recreate(node, TR::awrtbari);
      else
         TR::Node::recreate(node, TR::awrtbar);
      }
   else if (seenIndirectStore)
      {
      node->setNumChildren(storeNumChildren);

#ifdef J9_PROJECT_SPECIFIC
      if (seenIndirectBCDStore)
         node->setBCDStoreIsTemporarilyALoad(false);
#endif

      if (node->getOpCode().isIndirect())
         TR::Node::recreate(node, _compilation->il.opCodeForCorrespondingIndirectLoad(node->getOpCodeValue()));
      else
         TR::Node::recreate(node, _compilation->il.opCodeForDirectStore(node->getDataType()));
      }

   return -1;
   }

bool TR_LocalAnalysisInfo::isCallLike(TR::Node *node) {
   TR::ILOpCode &opCode = node->getOpCode();
   TR::ILOpCodes opCodeValue = opCode.getOpCodeValue();

   if ((opCode.isCall() && !node->isPureCall()) ||
       opCodeValue == TR::New ||
       opCodeValue == TR::newarray ||
       opCodeValue == TR::anewarray ||
       opCodeValue == TR::multianewarray)
      return true;

   //if (debug("preventUnresolvedPRE"))
      {
      if (node->hasUnresolvedSymbolReference())
         return true;
      }

   if (node->getOpCode().hasSymbolReference())
      {
      if (node->getSymbolReference()->getSymbol()->isVolatile() ||
          (node->getSymbolReference()->getSymbol()->isMethodMetaData() &&
           !node->getSymbolReference()->getSymbol()->isImmutableField()))
         return true;

      if (node->getSymbolReference()->isSideEffectInfo() ||
          node->getSymbolReference()->isOverriddenBitAddress())
         return true;

      if (node->isThisPointer() && !node->isNonNull())
         return true;

      if (comp()->requiresSpineChecks() &&
          //node->getOpCode().hasSymbolReference() &&
          node->getSymbol()->isArrayShadowSymbol())
         {
         //containsCallInStoreLhs = true;
         return true;
         }

      // Note that this change is only needed because javaLangClassFromClass loads are being skipped for performance (see earlier code in this file) and
      // we want to avoid strange side effects of a parent being commoned but a child is not a candidate for commoning.
      //
      if (node->getOpCode().hasSymbolReference() &&
          (node->getSymbolReference() == comp()->getSymRefTab()->findJavaLangClassFromClassSymbolRef()))
         {
         return true;
         }
      }
   return false;
}


// Check if this node contains a call or call-like opcode that
// cannot be moved (duplicated) by PRE
//
// This method iterates over the children of the call, so it needs a visit count mechanism
// to avoid processing nodes twice. However, the trees are already in a similar situation so
// the node visit counts must not be corrupted for those iterations.
// Simply incrementing for each use of containsCall will not work, for large programs the visit
// count will overflow.
// To get around this, we use two visit counts:
//    if the old visit count is the current one, the new visit count is old visit count + 2
//    otherwise, the new visit count is old visit count + 1
// At the end of processing the node tree is revisited and the visit counts reset to the old
// visit count or to zero.
// It is guaranteed that there is room for old visit count + 2, that is checked before all
// this starts.
//
bool TR_LocalAnalysisInfo::containsCall(TR::Node *node, bool &containsCallInStoreLhs)
   {
   bool result = containsCallInTree(node, containsCallInStoreLhs);
   containsCallResetVisitCounts(node);
   return result;
   }
bool TR_LocalAnalysisInfo::containsCallInTree(TR::Node *node, bool &containsCallInStoreLhs)
   {
   vcount_t visitCount = node->getVisitCount();
   if (visitCount == _visitCount+1 || visitCount == _visitCount+2)
      return false;

   if (visitCount == _visitCount)
      visitCount = 2;
   else
      visitCount = 1;
      node->setVisitCount(_visitCount + visitCount);

   if (isCallLike(node))
      return true;

   int32_t i;
   for (i = 0;i < node->getNumChildren();i++)
      {
      bool childHasCalls = containsCallInTree(node->getChild(i), containsCallInStoreLhs);
      if (childHasCalls)
         {
         if (node->getOpCode().isStoreIndirect() &&
             (i == 0))
            containsCallInStoreLhs = true;
         return true;
         }
      }

   return false;
   }
void TR_LocalAnalysisInfo::containsCallResetVisitCounts(TR::Node *node)
   {
   int32_t i;
   i = node->getVisitCount();
   if (i == _visitCount+2)
      i = _visitCount;
   else if (i == _visitCount+1)
      i = 0;
   else
      return;
   node->setVisitCount(i);

   for (i = 0; i < node->getNumChildren(); i++)
      containsCallResetVisitCounts(node->getChild(i));
   }



//
// Returns true if the two subtrees are identical syntactically; only special case
// is TR::aiadd (or TR::aladd) which may not participate in PRE; expressions containing
// the TR::aiadd (or TR::aladd) are matched correctly though by the special case code.
//
bool TR_LocalAnalysisInfo::areSyntacticallyEquivalent(TR::Node *node1, TR::Node *node2)
   {
   if (!_optimizer->areNodesEquivalent(node1,node2))
      return false;

   if (!(node1->getNumChildren() == node2->getNumChildren()))
      return false;

   if (node1 == node2)
      return true;

   int32_t i;
   for (i = 0;i < node1->getNumChildren();i++)
      {
      if (node1->getChild(i)->getLocalIndex() != node2->getChild(i)->getLocalIndex())
         return false;
      else
         {
         if (node1->getChild(i)->getLocalIndex() == MAX_SCOUNT)
            {
            if ((node1->getChild(i)->getOpCode().isLoadConst() || node1->getChild(i)->getOpCode().isLoadVarDirect()) && (node2->getChild(i)->getOpCode().isLoadConst() || node2->getChild(i)->getOpCode().isLoadVarDirect()))
               {
               if (!_optimizer->areNodesEquivalent(node1->getChild(i), node2->getChild(i)))
                  return false;
               }
            else if ((node1->getChild(i)->getOpCode().isArrayRef()) && (node2->getChild(i)->getOpCode().isArrayRef()))
               {
               for (int32_t j = 0;j < node1->getChild(i)->getNumChildren();j++)
                  {
                  TR::Node *grandChild1 = node1->getChild(i)->getChild(j);
                  TR::Node *grandChild2 = node2->getChild(i)->getChild(j);

                  if (grandChild1->getLocalIndex() != grandChild2->getLocalIndex())
                     return false;
                  else
                     {
                     if (grandChild1->getLocalIndex() == MAX_SCOUNT)
                        {
                        if ((grandChild1->getOpCode().isLoadConst() || grandChild1->getOpCode().isLoadVarDirect()) && (grandChild2->getOpCode().isLoadConst() || grandChild2->getOpCode().isLoadVarDirect()))
                           {
                           if (!_optimizer->areNodesEquivalent(grandChild1, grandChild2))
                              return false;
                           }
                        else
                           return false;
                        }
                     else if (grandChild1->getLocalIndex() == 0)
                        return false;
                     }
                  }
               }
            else
               return false;
            }
         else if (node1->getChild(i)->getLocalIndex() == 0)
            return false;
         }
      }

   return true;
   }



int32_t TR_LocalAnalysisInfo::HashTable::hash(TR::Node *node)
   {
   // Hash on the opcode and value numbers of the children
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
         h += (int32_t)(intptr_t)child->getSymbolReference()->getSymbol();
      else
         h++;

      g = h & 0xF0000000;
      h ^= g >> 24;
      }
   return (h ^ g) % _numBuckets;
   }

TR_LocalAnalysisInfo::HashTable::Cursor::Cursor(HashTable *table, int32_t bucket)
   : _table(*table), _bucket(bucket), _chunk(table->_buckets[bucket]), _index(0)
   {
   }

TR::Node *TR_LocalAnalysisInfo::HashTable::Cursor::firstNode()
   {
   _chunk = _table._buckets[_bucket];
   if (!_chunk) return NULL;
   _index = 0;
   return _chunk->_nodes[_index];
   }

TR::Node *TR_LocalAnalysisInfo::HashTable::Cursor::nextNode()
   {
   while (_chunk)
      {
      if (_index < NODES_PER_CHUNK-1)
         {
         _index++;
         TR::Node *node = _chunk->_nodes[_index];
         if (node)
            return node;
         }
      _chunk = _chunk->_next;
      _index = -1;
      }
   return NULL;
   }



// Counts nodes that involved in PRE that are not stores or checks.
// These nodes require temps.
//
bool TR_LocalAnalysisInfo::countSupportedNodes(TR::Node *node, TR::Node *parent, bool &containsCallInStoreLhs)
   {
   if (_visitCount == node->getVisitCount())
      {
      return false;
      }

   node->setVisitCount(_visitCount);
   node->setContainsCall(false);

   if (isCallLike(node))
      {
      node->setContainsCall(true);
      // would return here
      }

   bool flag = false;
   TR::ILOpCode &opCode = node->getOpCode();
   int n = node->getNumChildren();

   int32_t i;
   for (i = 0; i < n; i++)
      {
      TR::Node *child = node->getChild(i);
      bool childHasCallsInStoreLhs = false;
      if (countSupportedNodes(child, node, childHasCallsInStoreLhs))
         flag = true;

      if (childHasCallsInStoreLhs)
         containsCallInStoreLhs = true;

      if (child->containsCall())
         {
         if (node->getOpCode().isStoreIndirect() && (i == 0))
            containsCallInStoreLhs = true;
         node->setContainsCall(true);
         }
      }

   if (TR_LocalAnalysis::isSupportedNode(node, _compilation, parent))
      {
      int oldExpressionOnRhs = hasOldExpressionOnRhs(node, false, containsCallInStoreLhs);

      if (oldExpressionOnRhs == -1)
         {
         if (trace())
            {
            traceMsg(comp(), "\nExpression #%d is : \n",_numNodes);
            _compilation->getDebug()->print(_compilation->getOutFile(), node, 6, true);
            }

         flag = true;
         node->setLocalIndex(_numNodes);
         _numNodes++;
         }
      else
        node->setLocalIndex(oldExpressionOnRhs);
      }
   else
      node->setLocalIndex(-1);

   return flag;
   }


// Nodes that can not be speculated in WCode mode
bool isExceptional(TR::Compilation *comp, TR::Node * node)
   {
   if (node->getOpCode().isLoadIndirect()) return true;


   if (comp->cg()->nodeMayCauseException(node))
      {
      if (comp->cg()->traceBCDCodeGen())
         traceMsg(comp,"d^d: %s (%p) may cause on exception so do not speculate in PRE\n",node->getOpCode().getName(),node);
      return true;
      }

#if 0
   int i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      if (isExceptional(comp, node->getChild(i)))
	 return true;
      }
#endif

   return false;
   }


// Collects nodes that involved in PRE that are not stores or checks.
// These nodes require temps.
//
bool TR_LocalAnalysisInfo::collectSupportedNodes(TR::Node *node, TR::Node *parent)
   {
   if (node->getVisitCount() == _visitCount)
      return false;

   node->setVisitCount(_visitCount);

   bool flag = false;
   bool childRelevant = false;
   TR::ILOpCode &opCode = node->getOpCode();

   int32_t i;
   for (i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      if (collectSupportedNodes(child, node))
         flag = true;

      if (_checkExpressions->get(child->getLocalIndex()))
         childRelevant = true;
      }

   if (TR_LocalAnalysis::isSupportedNode(node, _compilation, parent))
      {
      _supportedNodesAsArray[node->getLocalIndex()] = node;

      bool indirectionSafe = true;
      if (opCode.isIndirect() && (opCode.isLoadVar() || opCode.isStore()))
         {
         indirectionSafe = false;
         if (node->getFirstChild()->isThisPointer() &&
             node->getFirstChild()->isNonNull())
            {
            indirectionSafe = true;
            TR::Node *firstChild = node->getFirstChild();
            TR::SymbolReference *symRef = firstChild->getSymbolReference();
            int32_t len;
            const char *sig = symRef->getTypeSignature(len);

            TR::SymbolReference *otherSymRef = node->getSymbolReference();

            TR_OpaqueClassBlock *cl = NULL;
            if (sig && (len > 0))
               cl = _compilation->fe()->getClassFromSignature(sig, len, symRef->getOwningMethod(_compilation));

            TR_OpaqueClassBlock *otherClassObject = NULL;
            int32_t otherLen;
            const char *otherSig = otherSymRef->getOwningMethod(_compilation)->classNameOfFieldOrStatic(otherSymRef->getCPIndex(), otherLen);
            if (otherSig)
               {
               otherSig = classNameToSignature(otherSig, otherLen, _compilation);
               otherClassObject = _compilation->fe()->getClassFromSignature(otherSig, otherLen, otherSymRef->getOwningMethod(_compilation));
               }

            if (!cl ||
                !otherClassObject ||
                (cl != otherClassObject))
               indirectionSafe = false;
            }
         }

      if (childRelevant ||
         (!indirectionSafe || (opCode.isArrayLength())) ||
         (node->getOpCode().isArrayRef()) ||
         (opCode.hasSymbolReference() && (node->getSymbolReference()->isUnresolved() || node->getSymbol()->isArrayShadowSymbol())) ||
         (opCode.isDiv() || opCode.isRem()))
         _checkExpressions->set(node->getLocalIndex());
      }

   return flag;
   }
