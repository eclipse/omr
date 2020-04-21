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

#include "optimizer/Simplifier.hpp"

#include "optimizer/OMRSimplifierHelpers.hpp"
#include "optimizer/OMRSimplifierHandlers.hpp"
#include "optimizer/SimplifierTable.hpp"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "codegen/StorageInfo.hpp"
#include "codegen/TreeEvaluator.hpp"
#include "compile/Compilation.hpp"
#include "compile/ResolvedMethod.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "cs2/sparsrbit.h"
#include "env/IO.hpp"
#include "env/ObjectModel.hpp"
#include "env/TRMemory.hpp"
#include "il/AliasSetInterface.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/LabelSymbol.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/StaticSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Bit.hpp"
#include "infra/BitVector.hpp"
#include "infra/Cfg.hpp"
#include "infra/Link.hpp"
#include "infra/List.hpp"
#include "infra/CfgEdge.hpp"
#include "infra/CfgNode.hpp"
#include "compiler/il/ILOpCodes.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"

extern const SimplifierPtr simplifierOpts[];

static_assert(TR::NumIlOps ==
              (sizeof(simplifierOpts) / sizeof(simplifierOpts[0])),
              "simplifierOpts is not the correct size");


/*
 * Local helper functions
 */

static void resetBlockVisitFlags(TR::Compilation *comp)
   {
   for (TR::Block *block = comp->getStartBlock(); block != NULL; block = block->getNextBlock())
      {
      block->setHasBeenVisited(false);
      }
   }

static TR::TreeTop *findNextLegalTreeTop(TR::Compilation *comp, TR::Block *block)
   {
   vcount_t startVisitCount = comp->getStartTree()->getNode()->getVisitCount();
   TR::TreeTop * tt = NULL;
   for (tt = comp->getStartTree(); tt; tt = tt->getNextTreeTop())
      {
      if (tt->getNode()->getVisitCount() < startVisitCount)
         break;
      if (tt->getNode()->getOpCodeValue() == TR::BBStart)
         tt = tt->getNode()->getBlock()->getExit();
      }
   return tt;
   }

static void countNodes(CS2::ABitVector< TR::Allocator > & mark,
                       TR::Node * n,
                       size_t & numNodes)
   {
   if (mark.ValueAt(n->getGlobalIndex()))
      {
      return;
      }

   mark[n->getGlobalIndex()] = true;
   numNodes += 1;

   for (size_t i = 0; i < n->getNumChildren(); i++)
      {
      countNodes(mark, n->getChild(i), numNodes);
      }
   }

static void computeInvarianceOfAllStructures(TR::Compilation *comp, TR_Structure * s)
   {
   TR_RegionStructure *region = s->asRegion();

   if (region)
      {
      TR_StructureSubGraphNode *node;
      TR_RegionStructure::Cursor si(*region);
      for (node = si.getCurrent(); node != NULL; node = si.getNext())
         computeInvarianceOfAllStructures(comp, node->getStructure());

      region->resetInvariance();
      if (region->isNaturalLoop() /*|| region->containsInternalCycles() */)
         {
         region->computeInvariantExpressions();
         }
      }
   }


/*
 * Simplifier class functions
 */

// Simplify all blocks
//
OMR::Simplifier::Simplifier(TR::OptimizationManager *manager)
   : TR::Optimization(manager),
     _hashTable(manager->trMemory(), stackAlloc),
     _ccHashTab(manager->trMemory(), stackAlloc),
     _performLowerTreeNodePairs(getTypedAllocator<std::pair<TR::TreeTop*, TR::Node*>>(self()->allocator()))
   {
   _invalidateUseDefInfo      = false;
   _alteredBlock = false;
   _blockRemoved = false;

   _useDefInfo = optimizer()->getUseDefInfo();
   _valueNumberInfo = optimizer()->getValueNumberInfo();

   _reassociate = comp()->getOption(TR_EnableReassociation);

   _containingStructure = NULL;
   }

TR::Optimization *OMR::Simplifier::create(TR::OptimizationManager *manager)
   {
   return new (manager->allocator()) TR::Simplifier(manager);
   }

void
OMR::Simplifier::prePerformOnBlocks()
   {
   _invalidateUseDefInfo      = false;
   _alteredBlock = false;
   _blockRemoved = false;

   _useDefInfo = optimizer()->getUseDefInfo();
   _valueNumberInfo = optimizer()->getValueNumberInfo();
   _containingStructure = NULL;


   if (_reassociate)
      {
      _hashTable.reset();
      _hashTable.init(1000, true);

      TR_ASSERT(comp()->getFlowGraph()->getStructure(), "assertion failure");
      computeInvarianceOfAllStructures(comp(), comp()->getFlowGraph()->getStructure());
      }
      _ccHashTab.reset();
      _ccHashTab.init(64, true);

   if (trace())
      {
      comp()->dumpMethodTrees("Trees before simplification");
      }
   }

void
OMR::Simplifier::postPerformOnBlocks()
   {
   if (trace())
      comp()->dumpMethodTrees("Trees after simplification");

#ifdef DEBUG
   resetBlockVisitFlags(comp());
#endif

   // Invalidate usedef and value number information if necessary
   //
   if (_useDefInfo && _invalidateUseDefInfo)
      optimizer()->setUseDefInfo(NULL);
   if (_valueNumberInfo && _invalidateValueNumberInfo)
      optimizer()->setValueNumberInfo(NULL);
   }

int32_t
OMR::Simplifier::perform()
   {

   vcount_t visitCount = comp()->incOrResetVisitCount();
   TR::TreeTop * tt;
   for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      tt->getNode()->initializeFutureUseCounts(visitCount);

   comp()->incVisitCount();
   for (tt = comp()->getStartTree(); tt; tt = tt->getNextTreeTop())
      cleanupFlags(tt->getNode());

   visitCount = comp()->incVisitCount();
   tt = comp()->getStartTree();
   while (tt)
      tt = simplifyExtendedBlock(tt);

   comp()->getFlowGraph()->removeUnreachableBlocks();

   if (manager()->numPassesCompleted() == 0)
      manager()->incNumPassesCompleted();

   return 1;
   }


int32_t
OMR::Simplifier::performOnBlock(TR::Block * block)
   {
   if (block->getEntry())
      {
      TR::TreeTop *extendedExitTree = block->getEntry()->getExtendedBlockExitTreeTop();
      vcount_t visitCount = comp()->incOrResetVisitCount();
      for (TR::TreeTop * tt = block->getEntry(); tt; tt = tt->getNextTreeTop())
         {
         tt->getNode()->initializeFutureUseCounts(visitCount);
         if (tt == extendedExitTree)
            break;
         }

      comp()->incVisitCount();
      simplifyExtendedBlock(block->getEntry());
      }
   return 0;
   }

// Pre-order traversal, remove the flags on the way in.
// On the way out set the flags to true as appropriate:
//     nodeRequiresConditionCodes on the first child of computeCC nodes.
//     adjunct                    on the third child of dual high nodes.
//
void
OMR::Simplifier::cleanupFlags(TR::Node *node)
   {
   if (node->getVisitCount() == comp()->getVisitCount())
      return;
   node->setVisitCount(comp()->getVisitCount());

   if (node->nodeRequiresConditionCodes())
      node->setNodeRequiresConditionCodes(false);

   if (node->isAdjunct())
      node->setIsAdjunct(false);

   for (int32_t i = node->getNumChildren()-1; i >= 0; --i)
      cleanupFlags(node->getChild(i));

   if (node->getOpCodeValue() == TR::computeCC)
      node->getFirstChild()->setNodeRequiresConditionCodes(true);

   if (node->isDualHigh())
      node->getChild(2)->setIsAdjunct(true);
   }

void
OMR::Simplifier::setCC(TR::Node *node, OMR::TR_ConditionCodeNumber cc)
   {
   TR_ASSERT(node->nodeRequiresConditionCodes(), "assertion failure");
   TR_ASSERT(cc <= OMR::ConditionCodeLast, "illegal condition code setting");

   TR_HashId index = 0;
//   FIXME: is this assume actually needed? It seems harmless if the node is already present
//   TR_ASSERT(!_ccHashTab.locate(node->getGlobalIndex(), index),
//   "node already present in cc hashtable");
   _ccHashTab.add(node->getGlobalIndex(), index, (void*)cc);
   }

OMR::TR_ConditionCodeNumber
OMR::Simplifier::getCC(TR::Node *node)
   {
   TR_HashId index;
   if (!_ccHashTab.locate(node->getGlobalIndex(), index))
      return OMR::ConditionCodeInvalid;
   return (OMR::TR_ConditionCodeNumber)(uintptr_t) _ccHashTab.getData(index);
   }

TR::TreeTop *
OMR::Simplifier::simplifyExtendedBlock(TR::TreeTop * treeTop)
   {
   TR::Block * block = 0;

   _containingStructure = NULL;
   _blockRemoved = false;

   for (; treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node * node = treeTop->getNode();
      TR_ASSERT(node->getOpCodeValue() == TR::BBStart, "Simplification, expected BBStart treetop");

      TR::Block * b = node->getBlock();
      if (block && !b->isExtensionOfPreviousBlock())
         break;

      if (b->isOSRCodeBlock() || b->isOSRCatchBlock())
         {
         b->setHasBeenVisited();
         treeTop = b->getExit();
         continue;
         }

#ifdef DEBUG
      if (block != b)
         b->setHasBeenVisited();
#endif

      if (!block && _reassociate &&
          comp()->getFlowGraph()->getStructure() != NULL         // [99391] getStructureOf() only valid if structure isn't invalidated
         )
         { // b is first block in the extended block
         TR_BlockStructure *blockStructure = b->getStructureOf();

         if(blockStructure)
            {
            TR_Structure *parent = blockStructure->getParent();
            while (parent)
               {
               TR_RegionStructure *region = parent->asRegion();
               if (region->isNaturalLoop() /* || region->containsInternalCycles() */)
                  {
                  _containingStructure = region;
                  break;
                  }
               parent = parent->getParent();
               }
            }
         }

      block = b;

      if (trace())
         traceMsg(comp(), "simplifying block_%d\n", block->getNumber());

      simplify(block);

      for(auto cursor = _performLowerTreeNodePairs.begin(); cursor != _performLowerTreeNodePairs.end(); ++cursor)
         {
         auto treeNodePair = *cursor;
         if (trace())
            traceMsg(comp(), "process _performLowerTreeNodePairs treetop %p node %p\n", treeNodePair.first, treeNodePair.second);
         TR::Node *performLowerNode = postWalkLowerTreeSimplifier(treeNodePair.first, treeNodePair.second, block, (TR::Simplifier *) this);
         treeNodePair.first->setNode(performLowerNode);
         }

      while (!_performLowerTreeNodePairs.empty())
         _performLowerTreeNodePairs.pop_back();

      // If the block itself was removed from the CFG during simplification, find
      // the next 'legitimate' block to be simplified
      //
      //if (comp()->getFlowGraph()->getRemovedNodes().find(block))
        if(block->nodeIsRemoved())
         {
         TR::TreeTop * tt = findNextLegalTreeTop(comp(), block);
         // in certain cases the removed block might be the last one we haven't
         // visited and therefore we won't be able to find a treetop to continue
         // in such cases we exit the loop
         //
         treeTop = tt ? tt->getPrevTreeTop() : 0;
         if (!treeTop)
            break;
         }
      else
         {
         treeTop = block->getExit();
         }
      }

   // now remove any unreachable blocks
   //
   if (_blockRemoved)
      {
      // if the next block to be processed has been removed,
      // find the next valid block to process
      //
      if (treeTop)
         {
         TR::Block *b = treeTop->getNode()->getBlock();
         //if (comp()->getFlowGraph()->getRemovedNodes().find(b))
           if(b->nodeIsRemoved())
            treeTop = findNextLegalTreeTop(comp(), b);
         }
      }

   return treeTop;
   }

void
OMR::Simplifier::simplify(TR::Block * block)
   {
   _alteredBlock = false;

   TR::TreeTop * tt, * next;

   //vcount_t visitCount = comp()->incVisitCount();
   //for (tt = block->getEntry(); tt; tt = tt->getNextTreeTop())
    //  tt->getNode()->initializeFutureUseCounts(visitCount);

   for (tt = block->getEntry(); tt; tt = next)
      {
      next = simplify(tt, block);

      // NOTE: simplification can change the exit for a block, so don't move
      // this getExit call out of the loop.
      //
      if (tt == block->getExit())
         break;
      }

   if (_alteredBlock)
      {
      _invalidateValueNumberInfo = true;
      requestOpt(OMR::localCSE, true, block);
      }
   }

// Simplify a complete expression tree.
// Returns the next treetop to be processed.
//
TR::TreeTop *
OMR::Simplifier::simplify(TR::TreeTop * treeTop, TR::Block * block)
   {
   TR::Node * node = treeTop->getNode();
   if (node->getVisitCount() == comp()->getVisitCount())
      return treeTop->getNextTreeTop();

   // Note that this call to simplify may cause the treetops before or after
   // this treetop to be removed, so we can't hold the previous or next
   // treetop locally across this call.
   //
   _curTree = treeTop;
   node = simplify(node, block);
   treeTop->setNode(node);

   // Grab the next treetop AFTER simplification of the current treetop, since
   // the next may be affected by simplification.
   //
   TR::TreeTop * next = _curTree->getNextTreeTop();

   // If the node is null, this treetop can be removed
   //
   if (node == NULL &&
       (!block->getPredecessors().empty() ||
        !block->getExceptionPredecessors().empty()))
      TR::TransformUtil::removeTree(comp(), treeTop);

   return next;
   }

// Simplify a sub-tree.
// Returns the replaced root of the sub-tree, which may be null if the sub-tree
// has been removed.
//
TR::Node *
OMR::Simplifier::simplify(TR::Node * node, TR::Block * block)
   {
   // Set the visit count for this node to prevent recursion into it
   //
   vcount_t visitCount = comp()->getVisitCount();
   node->setVisitCount(visitCount);

   if (node->nodeRequiresConditionCodes())
      {
      // On Java, nodes that require condition codes must not be simplified.
      dftSimplifier(node, block, (TR::Simplifier *) this);
      return node;
      }

   // Simplify this node.
   // Note that the processing routine for the node is responsible for
   // simplifying its children.
   //
   TR::Node * newNode = simplifierOpts[node->getOpCodeValue()](node, block, (TR::Simplifier *) this);
   if ((node != newNode) ||
       (newNode &&
        ((newNode->getOpCodeValue() != node->getOpCodeValue()) ||
         (newNode->getNumChildren() != node->getNumChildren()))))
        requestOpt(OMR::localCSE, true, block);

   return newNode;
   }

TR::Node *
OMR::Simplifier::unaryCancelOutWithChild(TR::Node * node, TR::Node * firstChild, TR::TreeTop *anchorTree, TR::ILOpCodes opcode, bool anchorChildren)
   {
   if (!isLegalToUnaryCancel(node, firstChild, opcode))
      return NULL;

   if (firstChild->getOpCodeValue() == opcode &&
       (node->getType().isAggregate() || firstChild->getType().isAggregate()) &&
       (node->getSize() > firstChild->getSize() || node->getSize() != firstChild->getFirstChild()->getSize()))
      {
      // ensure a truncation side-effect of a conversion is not lost
      // o2a size=3
      //   a2o size=3 // conversion truncates in addition to type cast so cannot be removed
      //     loadaddr size=4
      // This restriction could be loosened to only disallow intermediate truncations (see BCD case above) but then would require a node
      // op that would just correct for size (e.g. addrSizeMod size=3 to replace the o2a/a2o pair)
      //
      // Do allow cases when all three sizes are the same and when the middle node widens but the top and bottom node have the same size, e.g.
      //
      // i2o size=3
      //   o2i size=4
      //     oload size=3
      //
      // Also allow the special case where the grandchild is not really truncated as the 'truncated' bytes are known to be zero
      // (i.e. there really isn't an intermediate truncation of 4->3 even though it appears that way from looking at the sizes alone)
      // o2i
      //   i2o size=3
      //     iushr
      //       x
      //       iconst 8
      bool disallow = true;
      TR::Node *grandChild = firstChild->getFirstChild();
      size_t nodeSize = node->getSize();
      if (node->getType().isIntegral() &&
          nodeSize == grandChild->getSize() &&
          nodeSize > firstChild->getSize())
         {
         size_t truncatedBits = (nodeSize - firstChild->getSize()) * 8;
         if (grandChild->getOpCode().isRightShift() && grandChild->getOpCode().isShiftLogical() &&
             grandChild->getSecondChild()->getOpCode().isLoadConst() &&
             (grandChild->getSecondChild()->get64bitIntegralValue() == truncatedBits))
            {
            disallow = false;
            if (trace())
               traceMsg(comp(),"do allow unaryCancel of node %s (%p) and firstChild %s (%p) as grandChild %s (%p) zeros the %d truncated bytes\n",
                       node->getOpCode().getName(),node,firstChild->getOpCode().getName(),firstChild,
                       grandChild->getOpCode().getName(),grandChild,truncatedBits/8);
            }
         }

      if (disallow)
         {
         if (trace())
            traceMsg(comp(),"disallow unaryCancel of node %s (%p) and firstChild %s (%p) due to unequal sizes (nodeSize %d, firstChildSize %d, firstChild->childSize %d)\n",
                    node->getOpCode().getName(),node,firstChild->getOpCode().getName(),firstChild,
                    node->getSize(),firstChild->getSize(),firstChild->getFirstChild()->getSize());
         return NULL;
         }
      }

   if (firstChild->getOpCodeValue() == opcode &&
       performTransformation(comp(), "%sRemoving node [" POINTER_PRINTF_FORMAT "] %s and its child [" POINTER_PRINTF_FORMAT "] %s\n",
             optDetailString(), node, node->getOpCode().getName(), firstChild, firstChild->getOpCode().getName()))
      {
      TR::Node *grandChild = firstChild->getFirstChild();
      grandChild->incReferenceCount();
      bool anchorChildrenNeeded = anchorChildren &&
         (node->getNumChildren() > 1 ||
          firstChild->getNumChildren() > 1 ||
          node->getOpCode().hasSymbolReference() ||
          firstChild->getOpCode().hasSymbolReference());
      prepareToStopUsingNode(node, anchorTree, anchorChildrenNeeded);
      node->recursivelyDecReferenceCount();
      node->setVisitCount(0);
      return grandChild;
      }

   return NULL;
   }

//---------------------------------------------------------------------
// Common routine to change a conditional branch into an unconditional one.
// Change the node to be the unconditional branch or NULL if no branch taken.
// Return true if blocks were removed as a result of the change
//
bool
OMR::Simplifier::conditionalToUnconditional(TR::Node *&node, TR::Block * block, int takeBranch)
   {
   if (!performTransformation(comp(), "%s change conditional to unconditional n%in\n",
         optDetailString(), node->getGlobalIndex()))
      {
      return false;
      }

   TR::CFGEdge* removedEdge = changeConditionalToUnconditional(node, block, takeBranch, _curTree, optDetailString());
   bool blocksWereRemoved = removedEdge ? removedEdge->getTo()->nodeIsRemoved() : false;


   if (takeBranch)
      {
      TR_ASSERT(node->getOpCodeValue() == TR::Goto, "expecting the node to have been converted to a goto");
      node = simplify(node, block);
      }

   if (blocksWereRemoved)
      {
      _invalidateUseDefInfo = true;
      _alteredBlock = true;
      _blockRemoved = true;
      }
   return blocksWereRemoved;
   }

void
OMR::Simplifier::prepareToReplaceNode(TR::Node * node)
   {
   OMR::Optimization::prepareToReplaceNode(node);
   _alteredBlock = true;
   }

void
OMR::Simplifier::anchorOrderDependentNodesInSubtree(TR::Node *node, TR::Node *replacement, TR::TreeTop* anchorTree)
{
   if (node == replacement)
      return;

   if (nodeIsOrderDependent(node, 0, false))
      {
      if (trace())
         traceMsg(comp(), "anchor detached node %p\n", node);

      generateAnchor(node, anchorTree);
      }
   else
      anchorChildren(node, anchorTree, 0, node->getReferenceCount() > 1, replacement);
}

bool
OMR::Simplifier::isBoundDefinitelyGELength(TR::Node *boundChild, TR::Node *lengthChild)
   {
   TR::ILOpCodes boundOp = boundChild->getOpCodeValue();
   if (boundOp == TR::iadd)
      {
      TR::Node *first  = boundChild->getFirstChild();
      TR::Node *second = boundChild->getSecondChild();
      if (first == lengthChild)
         {
         TR::ILOpCodes secondOp = second->getOpCodeValue();
         if (second->getOpCode().isArrayLength()                          ||
             secondOp == TR::bu2i                                          ||
             secondOp == TR::su2i                                          ||

             (secondOp == TR::iconst &&
              second->getInt() >= 0)                                      ||

             (secondOp == TR::iand                                     &&
              second->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (second->getSecondChild()->getInt() & 80000000) == 0)       ||

             (secondOp == TR::iushr                                    &&
              second->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (second->getSecondChild()->getInt() & 0x1f) > 0))
            {
            return true;
            }
         }
      else if (second == lengthChild)
         {
         TR::ILOpCodes firstOp = first->getOpCodeValue();
         if (first->getOpCode().isArrayLength()                          ||
             firstOp == TR::bu2i                                          ||
             firstOp == TR::su2i                                          ||

             (firstOp == TR::iand                                     &&
              first->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (first->getSecondChild()->getInt() & 80000000) == 0)       ||

             (firstOp == TR::iushr &&
              first->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (first->getSecondChild()->getInt() & 0x1f) > 0))
            {
            return true;
            }
         }
      }
   else if (boundOp == TR::isub)
      {
      TR::Node *first  = boundChild->getFirstChild();
      TR::Node *second = boundChild->getSecondChild();
      if (first  == lengthChild)
         {
         TR::ILOpCodes secondOp = second->getOpCodeValue();
         if ((secondOp == TR::iconst &&
              second->getInt() < 0)                                      ||

             (secondOp == TR::ior                                      &&
              second->getSecondChild()->getOpCodeValue() == TR::iconst &&
              (second->getSecondChild()->getInt() & 0x80000000) != 0))
            {
            return true;
            }
         }
      }

   return false;
   }

const char *
OMR::Simplifier::optDetailString() const throw()
   {
   return "O^O TREE SIMPLIFICATION: ";
   }
