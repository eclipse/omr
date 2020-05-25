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

#include "optimizer/Optimization.hpp"


#include <stddef.h>
#include <stdint.h>
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "cs2/allocator.h"
#include "env/IO.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/Cfg.hpp"
#include "infra/CfgEdge.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/Simplifier.hpp"
#include "optimizer/TransformUtil.hpp"

#define MAX_DEPTH_FOR_SMART_ANCHORING 3

// called once before perform is executed
void
OMR::Optimization::prePerform()
   {
   self()->prePerformOnBlocks();
   }

// called once after perform is executed
void
OMR::Optimization::postPerform()
   {
   self()->postPerformOnBlocks();
   }

TR::CodeGenerator *
OMR::Optimization::cg()
   {
   return self()->comp()->cg();
   }

TR_FrontEnd *
OMR::Optimization::fe()
   {
   return self()->comp()->fe();
   }

TR_Debug *
OMR::Optimization::getDebug()
   {
   return self()->comp()->getDebug();
   }

TR::SymbolReferenceTable *
OMR::Optimization::getSymRefTab()
   {
   return self()->comp()->getSymRefTab();
   }

TR_Memory *
OMR::Optimization::trMemory()
   {
   return self()->comp()->trMemory();
   }

TR_StackMemory
OMR::Optimization::trStackMemory()
   {
   return self()->comp()->trStackMemory();
   }

TR_HeapMemory
OMR::Optimization::trHeapMemory()
   {
   return self()->comp()->trHeapMemory();
   }

TR_PersistentMemory *
OMR::Optimization::trPersistentMemory()
   {
   return self()->comp()->trPersistentMemory();
   }

TR::Allocator
OMR::Optimization::allocator()
   {
   return self()->comp()->allocator();
   }

OMR::Optimizations
OMR::Optimization::id()
   {
   return self()->manager()->id();
   }

const char *
OMR::Optimization::name()
   {
   return self()->manager()->name();
   }

void
OMR::Optimization::setTrace(bool trace)
   {
   self()->manager()->setTrace(trace);
   }

bool
OMR::Optimization::traceAny()
   {
   return (self()->trace() || self()->comp()->getOptions()->getAnyOption(TR_TraceAll));
   }

bool
OMR::Optimization::getLastRun()
   {
   return self()->manager()->getLastRun();
   }

void
OMR::Optimization::requestOpt(OMR::Optimizations optNum, bool value, TR::Block *block)
   {
   TR::OptimizationManager *manager = self()->optimizer()->getOptimization(optNum);
   if (manager)
      manager->setRequested(value, block);
   }

// useful utility functions for opts
void
OMR::Optimization::requestDeadTreesCleanup(bool value, TR::Block *block)
   {
   self()->requestOpt(OMR::deadTreesElimination, value, block);
   self()->requestOpt(OMR::trivialDeadTreeRemoval, value, block);
   }

//---------------------------------------------------------------------
// Prepare to replace a node by changing its opcode.
// Use/def information and value number information must be given a
// chance to update themselves.
// The reference counts on all children must be decremented.
//
void
OMR::Optimization::prepareToReplaceNode(TR::Node * node)
   {
   // Value number information will be invalid after this optimization
   //
   self()->optimizer()->prepareForNodeRemoval(node);
   node->removeAllChildren();
   }

void
OMR::Optimization::prepareToReplaceNode(TR::Node * node, TR::ILOpCodes opcode)
   {
   TR::Node::recreate(node, opcode);
   self()->prepareToReplaceNode(node);
   }

/**
 * Anchor all first level children of a given node, use this if you are to replace the node
 * param node       The node whose children is to be anchored
 * param anchorTree The tree before which the anchor trees are to be inserted
 */
void
OMR::Optimization::anchorAllChildren(TR::Node * node, TR::TreeTop *anchorTree)
   {
   TR_ASSERT(anchorTree != NULL, "Can't anchor children to a NULL TR::TreeTop\n");
   if (self()->trace())
      traceMsg(self()->comp(), "%sanchoring children of node [" POINTER_PRINTF_FORMAT "]\n", self()->optDetailString(), node);
   for (int i = 0; i <node->getNumChildren(); i++)
      {
      TR::TreeTop *tt = TR::TreeTop::create(self()->comp(), TR::Node::create(TR::treetop, 1, node->getChild(i)));
      if (self()->trace())
         traceMsg(self()->comp(), "TreeTop [" POINTER_PRINTF_FORMAT "] is created to anchor child [" POINTER_PRINTF_FORMAT "]\n", tt, node->getChild(i));
      anchorTree->insertBefore(tt);
      }
   }

/** anchorChildren
 *  Only anchor order dependent children
 *  \param node Root node whose children is to be anchored
 *  \param anchorTree Tree before which anchor trees will be placed
 */
void
OMR::Optimization::anchorChildren(TR::Node *node, TR::TreeTop* anchorTree, uint32_t depth, bool hasCommonedAncestor, TR::Node* replacement)
   {
   TR::Node *prevChild = NULL;

   // remaining path will be anchored to the original ancestor
   if (node == replacement)
      return;

   if (!hasCommonedAncestor)
      {
      if (self()->trace())
         traceMsg(self()->comp(),"set hasCommonedAncestor = true as %s %p has refCount %d > 1\n",
            node->getOpCode().getName(),node,node->getReferenceCount());
      hasCommonedAncestor = (node->getReferenceCount() > 1);
      }

   for (int j = node->getNumChildren()-1; j >= 0; --j)
      {
      TR::Node *child = node->getChild(j);

      if (prevChild != child) // quite common for anchor to be called with two equal children
         {
         if (self()->nodeIsOrderDependent(child, depth, hasCommonedAncestor))
            {
            dumpOptDetails(self()->comp(), "%sanchor child %s [" POINTER_PRINTF_FORMAT "] at depth %d before %s [" POINTER_PRINTF_FORMAT "]\n",
               self()->optDetailString(),child->getOpCode().getName(),child,depth,anchorTree->getNode()->getOpCode().getName(),anchorTree->getNode());

            self()->generateAnchor(child, anchorTree);
            }
         else
            {
            self()->anchorChildren(child, anchorTree, depth + 1, hasCommonedAncestor, replacement); // skipped current child, so recurse its subtree
            }
         }

      prevChild = child;
      }
   }

void
OMR::Optimization::generateAnchor(TR::Node *node, TR::TreeTop* anchorTree)
   {
   TR_ASSERT(anchorTree != NULL, "Can't anchor children to a NULL TR::TreeTop\n");
   anchorTree->insertBefore(TR::TreeTop::create(self()->comp(),
                                                     TR::Node::create(TR::treetop, 1, node)));
   }

void
OMR::Optimization::anchorNode(TR::Node *node, TR::TreeTop* anchorTree)
   {
   if (node->getOpCode().isLoadConst() && node->anchorConstChildren())
      {
      for (int32_t i=0; i < node->getNumChildren(); i++)
         {
         self()->generateAnchor(node->getChild(i), anchorTree);
         }
      }
   else if (!node->getOpCode().isLoadConst())
      {
      self()->generateAnchor(node, anchorTree);
      }
   }

extern void createGuardSiteForRemovedGuard(TR::Compilation *comp, TR::Node* ifNode);
/**
 * Folds a given if in IL. This method does NOT update CFG
 * The callers should handle any updates to CFG or call
 * conditionalToUnconditional
 */
bool
OMR::Optimization::removeOrconvertIfToGoto(TR::Node* &node, TR::Block* block, int takeBranch, TR::TreeTop* curTree, TR::TreeTop*& reachableTarget, TR::TreeTop*& unreachableTarget, const char* opt_details)
   {
   // Either change the conditional branch to an unconditional one or remove
   // it altogether.
   // In either case the CFG must be updated to reflect the change.
   //

#ifdef J9_PROJECT_SPECIFIC
   // doesn't matter taken or untaken, if it's a profiled guard we need to make sure the AOT relocation is created
   createGuardSiteForRemovedGuard(self()->comp(), node);
#endif

   if (takeBranch)
      {
      // Change the if into a goto
      //
      if (!performTransformation(self()->comp(), "%sChanging node [" POINTER_PRINTF_FORMAT "] %s into goto \n", opt_details, node, node->getOpCode().getName()))
         return false;
      self()->anchorChildren(node, curTree);
      self()->prepareToReplaceNode(node);
      TR::Node::recreate(node, TR::Goto);
      reachableTarget = node->getBranchDestination();
      unreachableTarget = block->getExit()->getNextTreeTop();
      }
   else
      {
      // Remove this node
      //
      if (!performTransformation(self()->comp(), "%sRemoving fall-through compare node [" POINTER_PRINTF_FORMAT "] %s\n", opt_details, node, node->getOpCode().getName()))
         return false;
      self()->anchorChildren(node, curTree);
      reachableTarget = block->getExit()->getNextTreeTop();
      unreachableTarget = node->getBranchDestination();
      // Don't call remove node as we can't suppress the removal with performTransformation any more
      self()->prepareToStopUsingNode(node, curTree);
      node->removeAllChildren();
      node = NULL;
      }

   return true;
}

/**
 * Folds a given if by updating IL, CFG and Structures.
 * If the removal of the edge caused the part of the CFG to become unreachable
 * (orphaned), it will remove the subgraph.
 *
 * The callers should be aware that the block containing the if tree might be
 * removed as well and handle such a scenario appropriately
 */
TR::CFGEdge*
OMR::Optimization::changeConditionalToUnconditional(TR::Node*& node, TR::Block* block, int takeBranch, TR::TreeTop* curTree, const char* opt_details)
   {

   TR::TreeTop *reachableTarget, *unreachableTarget;

   if (!self()->removeOrconvertIfToGoto(node, block, takeBranch, curTree, reachableTarget, unreachableTarget, opt_details))
      return NULL;

   // Now "unreachableTarget" contains the branch destination that is NOT being
   // taken.
   //
   TR::CFG * cfg = self()->comp()->getFlowGraph();
   TR::CFGEdge* edge = NULL;

   bool blocksWereRemoved = false;

   if (cfg)
      {
      if (unreachableTarget != reachableTarget)
         {
         TR_ASSERT(unreachableTarget->getNode()->getOpCodeValue() == TR::BBStart, "Simplifier, expected BBStart");


         TR_SuccessorIterator ei(block);
         for (edge = ei.getFirst(); edge != NULL; edge = ei.getNext())
            if (edge->getTo() == unreachableTarget->getNode()->getBlock())
               {
               blocksWereRemoved = cfg->removeEdge(edge);
               break;
               }

         }
      }
   else if (takeBranch)
      {
      // There is no CFG so we may be dealing with extended basic blocks.
      // Remove all non-fence treetops from here to the end of the block
      //
      for (TR::TreeTop * treeTop = block->getLastRealTreeTop(), * prevTreeTop;
           treeTop->getNode() != node;
           treeTop = prevTreeTop)
         {
         prevTreeTop = treeTop->getPrevRealTreeTop();
         TR::TransformUtil::removeTree(self()->comp(), treeTop);
         blocksWereRemoved = true;
         }
      }

   return edge;
   }

/**
 * Prepare to stop using a node
 */
void
OMR::Optimization::prepareToStopUsingNode(TR::Node * node, TR::TreeTop* anchorTree, bool anchorChildrenNeeded)
   {
   if (anchorChildrenNeeded && node->getOpCodeValue() != TR::treetop)
      {
      self()->anchorChildren(node, anchorTree);
      }
   if (node->getReferenceCount() <= 1)
      {
      self()->optimizer()->prepareForNodeRemoval(node);
      }
   }

/**
 * In the special case of replacing a node with its child then anchoring is not
 * needed for the replacing child (as it and any children of its own will
 * remain anchored)
 *
 * So if all of the node's other children are constants then no anchoring at
 * all is required when replacing.
 */
TR::Node *
OMR::Optimization::replaceNodeWithChild(TR::Node *node, TR::Node *child, TR::TreeTop* anchorTree, TR::Block *block, bool correctBCDPrecision)
   {
#ifdef J9_PROJECT_SPECIFIC
   if (correctBCDPrecision &&
       node->getType().isBCD() && child->getType().isBCD() &&
       node->getDecimalPrecision() != child->getDecimalPrecision())
      {
      // The extra work for BCD (packed,zoned etc) nodes on a replace node is ensuring
      // that the precision of the replacing node (child) matches the precision of the replaced
      // node (node).
      // If 'child' has a different precision than 'node' then the parent of 'node', if it is call
      // for example, will pass too many or too few digits to its callee (even digits known to be zero
      // are significant to calls to ensure the callee can demarshal the arguments correctly).
      // If the 'child' truncates then there is the danger that this truncating side-effect will be lost unless
      // a bcd modPrecOp is inserted.
      // If correctBCDPrecision=false then the caller has guaranteed that there is no danger of a child truncation
      // or widening causing any problems.
      //
      // This transformation is required for correctness so do not provide a performTransformation

      int32_t childIndex = -1;
      for (int32_t i=0; i < node->getNumChildren(); i++)
         {
         if (node->getChild(i) == child)
            {
            childIndex = i;
            }
         else if (node->getChild(i)->getOpCode().isLoadConst() && node->getChild(i)->anchorConstChildren())
            {
            for (int32_t j=0; j < node->getChild(i)->getNumChildren(); j++)
               {
               self()->anchorNode(node->getChild(i)->getChild(j), anchorTree);
               }
            }
         else if (!node->getChild(i)->getOpCode().isLoadConst() &&
                  node->getChild(i)->getOpCodeValue() != TR::loadaddr)
            {
            self()->anchorNode(node->getChild(i), anchorTree);
            }
         }
      if (childIndex == -1)
         {
         TR_ASSERT(false,"could not find child under node in replaceBCDNodeWithChild\n");
         return node;
         }
      if (node->getReferenceCount() > 1)
         {
         TR::Node *newNode = TR::Node::create(TR::ILOpCode::modifyPrecisionOpCode(child->getDataType()), 1, child);
         newNode->setDecimalPrecision(node->getDecimalPrecision());
         dumpOptDetails(self()->comp(), "%sPrecision mismatch when replacing parent %s [" POINTER_PRINTF_FORMAT "] with child %s [" POINTER_PRINTF_FORMAT "] so create new parent %s [" POINTER_PRINTF_FORMAT "]\n",
                          self()->optDetailString(),node->getOpCode().getName(),node,child->getOpCode().getName(),child,newNode->getOpCode().getName(),newNode);
         return self()->replaceNode(node, newNode, anchorTree, false); // needAnchor=false
         }
      else
         {
         dumpOptDetails(self()->comp(), "%sPrecision mismatch when replacing parent %s [" POINTER_PRINTF_FORMAT "] with child %s [" POINTER_PRINTF_FORMAT "] so change parent op to ",
                          self()->optDetailString(),node->getOpCode().getName(),node,child->getOpCode().getName(),child);
         TR_ASSERT(node->getReferenceCount() == 1,"node %p refCount should be 1 and not %d\n",node,node->getReferenceCount());
         child->incReferenceCount();
         // zd2pd    <- node - change to zdModifyPrecision (must use child type as modPrec is being applied to the child)
         //    zdX   <- child
         //
         self()->prepareToReplaceNode(node, TR::ILOpCode::modifyPrecisionOpCode(child->getDataType()));
         node->setNumChildren(1);
         node->setChild(0, child);
         dumpOptDetails(self()->comp(), "%s\n",node->getOpCode().getName());
         if (self()->id() == OMR::treeSimplification)
            return ((TR::Simplifier *) self())->simplify(node, block);
         else
            return node;
         }
      }
   else
#endif
      {
      TR_ASSERT(node->hasChild(child),"node %p is not a child of node %p\n",child,node);
      bool needAnchor = false;
      for (int32_t i = 0; i < node->getNumChildren(); i++)
         {
         // skip anchoring for 1) the input 'child' 2) constants with no children 3) loadaddrs
         if (node->getChild(i) != child &&
             (!node->getChild(i)->getOpCode().isLoadConst() || node->getChild(i)->anchorConstChildren()) &&
             node->getChild(i)->getOpCodeValue() != TR::loadaddr)
            {
            needAnchor = true;
            break;
            }
         }
      return self()->replaceNode(node, child, anchorTree, needAnchor);
      }
   }


/**
 * Common routine to replace the node by another
 *
 * This routine replaces one use of this node by adjusting reference counts and
 * returning the replacing node. It is assumed that the caller will replace the
 * reference to the original node with the replacing node. The visit count on
 * the original node is reset so that it is visited again on the next reference.
 */
TR::Node *
OMR::Optimization::replaceNode(TR::Node * node, TR::Node *other, TR::TreeTop *anchorTree, bool anchorChildren)
   {
   if (!performTransformation(self()->comp(), "%sReplace node [" POINTER_PRINTF_FORMAT "] %s by [" POINTER_PRINTF_FORMAT "] %s\n", self()->optDetailString(), node, node->getOpCode().getName(), other, other->getOpCode().getName()))
     {
     if(other->getReferenceCount() == 0)
       {
       // Because the replacement node will not be used and it does not currently exist in the compilation unit,
       // it should be destroyed.
       other->removeAllChildren();
       }
      return node;
     }


   // Increment the reference count on the replacing node, since it will be
   // getting a new reference.
   //
   other->incReferenceCount();

   // This must be done AFTER
   // incrementing the count on the replacing node in case the replacing node is
   // a child of the original node. Otherwise we can end up deleting
   // use/def info from the child (in prepareForNodeRemoval)
   // Adjust usedef and value number information if this node is not going to
   // be used any more.
   //
   self()->prepareToStopUsingNode(node, anchorTree, anchorChildren);

   // Decrement the reference count on the original node. This must be done AFTER
   // incrementing the count on the replacing node in case the replacing node is
   // a child of the original node. In this case we don't want the count on the
   // replacing node to go to zero.
   //
   node->recursivelyDecReferenceCount();

   // If the original node still has other uses make sure it is re-visited
   // so the other references to it can be fixed up too.
   //
   if (node->getReferenceCount() > 0)
      node->setVisitCount(0);

   return other;
   }

/**
 * Common routine to remove the node from the tree
 */
void
OMR::Optimization::removeNode(TR::Node * node, TR::TreeTop *anchorTree)
   {
   // Reduce the reference counts of all children.
   //
   if (performTransformation(self()->comp(), "%sRemoving redundant node [" POINTER_PRINTF_FORMAT "] %s\n", self()->optDetailString(), node, node->getOpCode().getName()))
      {
      self()->prepareToStopUsingNode(node, anchorTree);
      node->removeAllChildren();
      }
   }


bool
OMR::Optimization::nodeIsOrderDependent(TR::Node *node, uint32_t depth, bool hasCommonedAncestor)
{
   // While it may be tempting to use futureUseCount here, futureUseCount isn't well maintained in simplifier
   // and so shouldn't be used for functional items such as to anchor or not anchor a node. FutureUseCount
   // should only be used as a heuristic for optimizations.
   bool constNeedsAnchor = node->getOpCode().isLoadConst() && node->anchorConstChildren();
   return ((node->getOpCode().isLoad() && node->getOpCode().hasSymbolReference() &&
            ((node->getReferenceCount() > 1) || hasCommonedAncestor)) ||
           ((!node->getOpCode().isLoadConst() || constNeedsAnchor) && depth >= MAX_DEPTH_FOR_SMART_ANCHORING));
}
