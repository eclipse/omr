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

#include "optimizer/DeadTreesElimination.hpp"

#include <stddef.h>
#include <stdint.h>
#include "infra/forward_list.hpp"
#include "codegen/CodeGenerator.hpp"
#include "env/FrontEnd.hpp"
#include "compile/Compilation.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "il/AutomaticSymbol.hpp"
#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/ILOps.hpp"
#include "il/MethodSymbol.hpp"
#include "il/Node.hpp"
#include "il/NodePool.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/Symbol.hpp"
#include "il/SymbolReference.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "infra/Assert.hpp"
#include "infra/BitVector.hpp"
#include "infra/ILWalk.hpp"
#include "infra/List.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Optimizer.hpp"
#include "optimizer/TransformUtil.hpp"
#include "ras/Debug.hpp"


// Local helper functions

static OMR::TreeInfo *findOrCreateTreeInfo(TR::TreeTop *treeTop, List<OMR::TreeInfo> *targetTrees, TR::Compilation * comp)
   {
   ListIterator<OMR::TreeInfo> trees(targetTrees);
   OMR::TreeInfo *t;
   for (t = trees.getFirst(); t; t = trees.getNext())
      {
      if (t->getTreeTop() == treeTop)
         return t;
      }

   t = new (targetTrees->getRegion()) OMR::TreeInfo(treeTop, 0);
   targetTrees->add(t);
   return t;
   }

static inline bool isReadBarrierUnderTreetop(TR::Node *node)
   {
   return node->getOpCodeValue() == TR::treetop && node->getFirstChild()->getOpCode().isReadBar();
   }

bool collectSymbolReferencesInNode(TR::Node *node,
                                   TR::SparseBitVector &symbolReferencesInNode,
                                   int32_t *numDeadSubNodes, vcount_t visitCount, TR::Compilation *comp,
                                   bool *seenInternalPointer, bool *seenArraylet,
                                   bool *cantMoveUnderBranch)
   {
   // The visit count in the node must be maintained by this method.
   //
   vcount_t oldVisitCount = node->getVisitCount();
   if (oldVisitCount == visitCount || oldVisitCount == comp->getVisitCount())
      return true;
   node->setVisitCount(comp->getVisitCount());

   //diagnostic("Walking node %p, height=%d, oldVisitCount=%d, visitCount=%d, compVisitCount=%d\n", node, *height, oldVisitCount, visitCount,comp->getVisitCount());

   // For all other subtrees collect all symbols that could be killed between
   // here and the next reference.
   //
   for (int32_t i = node->getNumChildren()-1; i >= 0; i--)
      {
      TR::Node *child = node->getChild(i);
      if (child->getFutureUseCount() == 1 &&
          child->getReferenceCount() > 1 &&
          !child->getOpCode().isLoadConst())
         *numDeadSubNodes = (*numDeadSubNodes) + 1;

      collectSymbolReferencesInNode(child, symbolReferencesInNode, numDeadSubNodes, visitCount, comp,
            seenInternalPointer, seenArraylet, cantMoveUnderBranch);
      }

   // detect if this is a direct load that shouldn't be moved under a branch (because an update was moved past
   // this load by treeSimplification)
   if (cantMoveUnderBranch &&
       (node->getOpCode().isLoadVarDirect() || node->getOpCode().isLoadReg()) &&
       node->isDontMoveUnderBranch())
      *cantMoveUnderBranch = true;

   if (seenInternalPointer && node->isInternalPointer() && node->getReferenceCount() > 1)
      *seenInternalPointer = true;

   if (seenArraylet)
      {
      if (node->getOpCode().hasSymbolReference() &&
          node->getSymbolReference()->getSymbol()->isArrayletShadowSymbol() &&
          node->getReferenceCount() > 1)
         {
         *seenArraylet = true;
         }
      }

   // Add this node's symbol reference to the set
   if (node->getOpCode().hasSymbolReference())
      {
      symbolReferencesInNode[node->getSymbolReference()->getReferenceNumber()]=true;
      }

   return true;
   }

typedef std::pair<TR::Node* const, int32_t> LPEntry;
typedef TR::typed_allocator<LPEntry, TR::Region&> LPAlloc;
typedef std::map<TR::Node*, int32_t, std::less<TR::Node*>, LPAlloc> LongestPathMap;

static int32_t getLongestPathOfDAG(TR::Node *node, LongestPathMap &memo)
   {
   if (node->getNumChildren() == 0)
      return 0;

   auto ins = memo.insert(std::make_pair(node, 0));
   int32_t &value = ins.first->second;
   bool fresh = ins.second;
   if (!fresh)
      return value;

   int32_t maxLen = 0;
   for (int i = 0; i < node->getNumChildren(); i++)
      maxLen = std::max(maxLen, getLongestPathOfDAG(node->getChild(i), memo));

   value = maxLen + 1;
   return value;
   }

/*
 * \brief This function does 2 things:
 *        1. check if the \p nodeToSwingDown is in the subtree of \p containerNode
 *        2. only allowing volatile load being swung down across temps and constant string
 *
 * \parm nodeToSwingDown
 *    The node hanging under a treetop which can potentially be removed
 *
 * \parm containerNode
 *    The subtree to check
 */
static bool containsNode(TR::Node *containerNode, TR::Node *nodeToSwingDown, vcount_t visitCount, TR::Compilation *comp, int32_t *height, int32_t *maxHeight, bool &canMoveIfVolatile)
   {
   if (containerNode == nodeToSwingDown)
      return true;

   vcount_t oldVisitCount = containerNode->getVisitCount();
   if ((oldVisitCount == visitCount) || (oldVisitCount == comp->getVisitCount()))
      return false;
   containerNode->setVisitCount(comp->getVisitCount());

   if (containerNode->getOpCode().hasSymbolReference())
      {
      static const bool relaxedConditionForSwingingDownVolatile = feGetEnv("TR_relaxedConditionForSwingingDownVolatile") ? true: false;
      if (!relaxedConditionForSwingingDownVolatile)
         {
         if (!(containerNode->getSymbol()->isAutoOrParm() ||
               containerNode->getSymbol()->isConstString()))
            canMoveIfVolatile = false;
         }
      else if (containerNode->getSymbol()->isShadow() || containerNode->getSymbol()->isStatic())
         canMoveIfVolatile = false;
      }

   (*height)++;
   if (*height > *maxHeight)
     *maxHeight = *height;
   for (int32_t i = 0; i < containerNode->getNumChildren(); ++i)
      {
      if (containsNode(containerNode->getChild(i), nodeToSwingDown, visitCount, comp, height, maxHeight, canMoveIfVolatile))
         return true;
      }
   (*height)--;

   return false;
   }

#define MAX_ALLOWED_HEIGHT 50

static bool isSafeToReplaceNode(TR::Node *currentNode, TR::TreeTop *curTreeTop, bool *seenConditionalBranch,
      vcount_t visitCount, TR::Compilation *comp, TR::Optimization *opt, List<OMR::TreeInfo> *targetTrees, bool &cannotBeEliminated,
      LongestPathMap &longestPaths)
   {
   LexicalTimer tx("safeToReplace", comp->phaseTimer());

   TR::SparseBitVector symbolReferencesInNode(comp->allocator());

   // Collect all symbols that could be killed between here and the next reference
   //
   comp->incVisitCount();
   //////vcount_t visitCount = comp->getVisitCount();
   int32_t numDeadSubNodes = 0;
   bool cantMoveUnderBranch = false;
   bool seenInternalPointer = false;
   bool seenArraylet = false;
   int32_t curMaxHeight = getLongestPathOfDAG(currentNode, longestPaths);
   collectSymbolReferencesInNode(currentNode, symbolReferencesInNode, &numDeadSubNodes, visitCount, comp,
         &seenInternalPointer, &seenArraylet, &cantMoveUnderBranch);

   bool registersScarce = comp->cg()->areAssignableGPRsScarce();
#ifdef J9_PROJECT_SPECIFIC
   bool isBCD = currentNode->getType().isBCD();
#endif

   if (numDeadSubNodes > 1 &&
#ifdef J9_PROJECT_SPECIFIC
       !isBCD &&
#endif
       registersScarce)
      {
      return false;
      }

   OMR::TreeInfo *curTreeInfo = findOrCreateTreeInfo(curTreeTop, targetTrees, comp);
   int32_t curHeight = curTreeInfo->getHeight()+curMaxHeight;
   if (curHeight > MAX_ALLOWED_HEIGHT)
      {
      cannotBeEliminated = true;
      return false;
      }

   // TEMPORARY
   // Don't allow removal of a node containing an unresolved reference if
   // the gcOnResolve option is set
   //
   bool isUnresolvedReference = currentNode->hasUnresolvedSymbolReference();
   if (isUnresolvedReference)
      return false;

   bool mayBeVolatileReference = currentNode->mightHaveVolatileSymbolReference();
   // Do not swing down volatile nodes
   if (mayBeVolatileReference)
      return false;

   // Now scan forwards through the trees looking for the next use and checking
   // to see if any symbols in the subtree are getting modified; if so it is not
   // safe to replace the node at its next use.
   //

   comp->incVisitCount();
   for (TR::TreeTop *treeTop = curTreeTop->getNextTreeTop(); treeTop; treeTop = treeTop->getNextTreeTop())
      {
      TR::Node *node = treeTop->getNode();
      if (node->getOpCodeValue() == TR::treetop)
          node = node->getFirstChild();

      if (node->getOpCodeValue() == TR::BBStart &&
          !node->getBlock()->isExtensionOfPreviousBlock())
         return true;

      if (cantMoveUnderBranch && (node->getOpCode().isBranch()
         || node->getOpCode().isJumpWithMultipleTargets()))
         return false;

      if (node->canGCandReturn() &&
          seenInternalPointer)
         return false;

      int32_t tempHeight = 0;
      int32_t maxHeight = 0;
      bool canMoveIfVolatile = true;
      bool nodeInSubTree = containsNode(node, currentNode, visitCount, comp, &tempHeight, &maxHeight, canMoveIfVolatile);
      /*
       * Restrictions for swinging down volatile applies even when the volatile node is in the current sub-tree:
       * treetop
       *    a.volatileField
       * someOpcode someSymRef
       *    ...
       *    => xload/xloadi a.volatileField
       *    ...
       */
      //if (mayBeVolatileReference && !canMoveIfVolatile)
      //   return false;

      if (nodeInSubTree)
         {
         // TEMPORARY
         // Disable moving an unresolved reference down to the middle of a
         // JNI call, until the resolve helper is fixed properly
         //
         if (isUnresolvedReference && node->getFirstChild()->getOpCode().isCall() &&
             node->getFirstChild()->getSymbol()->castToMethodSymbol()->isJNI())
            return false;

         if (curTreeInfo)
            {
            OMR::TreeInfo *treeInfo = findOrCreateTreeInfo(treeTop, targetTrees, comp);
            int32_t height = treeInfo->getHeight();
            int32_t maxHeightUsed = maxHeight;
            if (maxHeightUsed < curMaxHeight)
               maxHeightUsed = curMaxHeight;

            if (height < curTreeInfo->getHeight())
               height = curTreeInfo->getHeight();
            height++;
            if ((height+maxHeightUsed) > MAX_ALLOWED_HEIGHT)
               {
               cannotBeEliminated = true;
               return false;
               }
            treeInfo->setHeight(height);
            }

         if (mayBeVolatileReference)
            dumpOptDetails(opt->comp(), "%sit is safe to remove volatile field load tree n%dn\n", opt->optDetailString(), currentNode->getGlobalIndex());
         return true;
         }

      if ((node->getOpCode().isBranch() &&
           (node->getOpCodeValue() != TR::Goto)) ||
           (node->getOpCode().isJumpWithMultipleTargets() && node->getOpCode().hasBranchChildren()))
        *seenConditionalBranch = true;

      if (node->getOpCodeValue() == TR::treetop ||
          node->getOpCode().isNullCheck() ||
          node->getOpCode().isResolveCheck() ||
          node->getOpCodeValue() == TR::ArrayStoreCHK ||
          node->getOpCode().isSpineCheck())
         {
         node = node->getFirstChild();
         }

      if (node->getOpCode().isStore())
         {
         // For a store, just the single symbol reference is killed.
         // Resolution of the store symbol is handled by TR::ResolveCHK
         //
         if (symbolReferencesInNode.ValueAt(node->getSymbolReference()->getReferenceNumber()))
            return false;
         }

      // Node Aliasing Changes
      // Check if the definition modifies any symbol in the subtree
      //
      if (node->mayKill(true).containsAny(symbolReferencesInNode, comp))
        return false;
      }
   return true;
   }

static void removeGlRegDep(TR::Node * parent, TR_GlobalRegisterNumber registerNum, TR::Block *containingBlock, TR::Optimization *opt)
   {
   if (parent->getNumChildren() == 0)
      return;

   TR_ASSERT(parent->getNumChildren() > 0, "expected TR::GlRegDeps %p", parent);
   TR::Node * predGlRegDeps = parent->getLastChild();

   if (predGlRegDeps->getOpCodeValue() != TR::GlRegDeps) // could be already removed
      return;

   TR_ASSERT(predGlRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");

   for (int32_t i = predGlRegDeps->getNumChildren() - 1; i >= 0; --i)
      if (predGlRegDeps->getChild(i)->getGlobalRegisterNumber() == registerNum)
         {
         dumpOptDetails(opt->comp(), "%sRemove GlRegDep : %p\n", opt->optDetailString(), predGlRegDeps->getChild(i));
         TR::Node *removedChild = predGlRegDeps->removeChild(i);
         if (removedChild->getReferenceCount() <= 1)
            {
            // The only remaining parent is the RegStore.  Another pass of
            // deadTrees may be able to eliminate that.
            //
            opt->requestOpt(OMR::deadTreesElimination, true, containingBlock);
            }
         break;
         }

   if (predGlRegDeps->getNumChildren() == 0)
      parent->removeLastChild();
   }

// Note: the future use counts are incremented with visit counts but are decremented here without
// using visit counts so they cannot be trusted for any functional purpose but only to guide heuristics.
static scount_t recursivelyDecFutureUseCount(TR::Node *node)
   {
   if (node->getFutureUseCount() > 0)
      node->decFutureUseCount();

   if (node->getReferenceCount() == 0)
      {
      for (int32_t childCount = node->getNumChildren()-1; childCount >= 0; childCount--)
         recursivelyDecFutureUseCount(node->getChild(childCount));
      }

   return node->getFutureUseCount();
   }


// DeadTreesElimination class methods

TR::Optimization *TR::DeadTreesElimination::create(TR::OptimizationManager *manager)
   {
   return new (manager->allocator()) TR::DeadTreesElimination(manager);
   }


TR::DeadTreesElimination::DeadTreesElimination(TR::OptimizationManager *manager)
   : TR::Optimization(manager)
   {
   _cannotBeEliminated = false;
   _delayedRegStores = false;
   }

int32_t TR::DeadTreesElimination::perform()
   {
   process(comp()->getStartTree(), NULL);
   return 1;
   }


int32_t TR::DeadTreesElimination::performOnBlock(TR::Block *block)
   {
   if (block->getEntry())
      process(block->getEntry(), block->getEntry()->getExtendedBlockExitTreeTop());
   return 0;
   }

typedef std::pair<ncount_t const, TR::TreeTop* > ReadBarToTreeTopMapEntry;
typedef TR::typed_allocator<ReadBarToTreeTopMapEntry, TR::Region &> ReadBarToTreeTopMapAlloc;
typedef std::map<ncount_t, TR::TreeTop *, std::less<ncount_t>, ReadBarToTreeTopMapAlloc> ReadBarToTreeTopMap;

static void findReadBarInSubTree(TR::Node *node, TR::NodeChecklist &visitedNodesInCurrentTree, TR::list<TR::Node*> &rdbarsInCurrentSubTree)
   {
   if (visitedNodesInCurrentTree.contains(node))
      return;
   visitedNodesInCurrentTree.add(node);
   if (node->getOpCode().isReadBar())
      rdbarsInCurrentSubTree.push_back(node);
   for (int i = 0; i < node->getNumChildren(); i++)
      findReadBarInSubTree(node->getChild(i), visitedNodesInCurrentTree, rdbarsInCurrentSubTree);
   }

void TR::DeadTreesElimination::prePerformOnBlocks()
   {
   _cannotBeEliminated = false;
   _delayedRegStores = false;

   /*
    * Walk through all the blocks to remove trivial dead trees in the following forms:
    *
    * case 1:
    * treetop
    *   => node
    *
    * case 2:
    * treetop
    *    xrdbari
    * anchor
    *    =>xrdbari
    *
    * The problem with these trees is in the scenario where the earlier use
    * of 'node' is also dead.  However, our analysis won't find that because
    * the reference count is > 1.
    *
    * Here are some clarification about case 2:
    * 1. Case 2 is seen very often because ilgen creates trees in the following form:
    *    NULLCHK
    *      ardbari
    *          aload
    *    anchor
    *      => ardbari
    *    And the NULLCHK can be optimized away by optimizations like value propagation and
    *    turned into a treetop.
    * 2. We do not remove the treetop node if there is any other tree whose subtree
    *    references to that rdbar before the anchor node, like the following:
    *    treetop
    *      ardbari
    *    SOMETREE (that's not anchor nor treetop)
    *      => ardbari
    *    anchor
    *      => ardbari
    *    Because those SOMETREE would need a treetop to anchor the rdbar node as the first evaluation point.
    *
    */
   vcount_t visitCount = comp()->incOrResetVisitCount();
   ReadBarToTreeTopMap rdbar2ttMap(std::less<ncount_t>(), comp()->trMemory()->currentStackRegion());

   for (TR::TreeTop *tt = comp()->getStartTree();
        tt != 0;
        tt = tt->getNextTreeTop())
      {
      bool removed = false;

      TR::Node *node = tt->getNode();
      if (node->getOpCodeValue() == TR::treetop &&
          node->getFirstChild()->getVisitCount() == visitCount &&
          performTransformation(comp(), "%sRemove trivial dead tree: %p\n", optDetailString(), node))
         {
         TR::TransformUtil::removeTree(comp(), tt);
         removed = true;
         }
      else if (node->getOpCode().isAnchor() && rdbar2ttMap.find(node->getFirstChild()->getGlobalIndex()) != rdbar2ttMap.end())
         {
         TR::TreeTop *ttToRemove = rdbar2ttMap[node->getFirstChild()->getGlobalIndex()];
         if (performTransformation(comp(), "%sRemove trivial dead tree (rdbar under treetop before compressedrefs): %p\n", optDetailString(), ttToRemove->getNode()))
            TR::TransformUtil::removeTree(comp(), ttToRemove);
         rdbar2ttMap.erase(node->getFirstChild()->getGlobalIndex());
         }
      else
         {
         if (comp()->useCompressedPointers() && !node->getOpCode().isAnchor() && !rdbar2ttMap.empty())
            {
            TR::NodeChecklist visitedNodesInCurrentTree(comp());
            TR::list<TR::Node*> rdbarsInSubTree(getTypedAllocator<TR::Node*>(comp()->allocator()));
            findReadBarInSubTree(node, visitedNodesInCurrentTree, rdbarsInSubTree);
            for (auto it = rdbarsInSubTree.begin(); it != rdbarsInSubTree.end(); it++)
               {
               TR::Node *rdbarNode = *it;
               if (rdbar2ttMap.find(rdbarNode->getGlobalIndex()) != rdbar2ttMap.end())
                  rdbar2ttMap.erase(rdbarNode->getGlobalIndex());
               }
            }

         if (node->getOpCode().isCheck() &&
             node->getFirstChild()->getOpCode().isCall() &&
             node->getFirstChild()->getReferenceCount() == 1 &&
             node->getFirstChild()->getSymbolReference()->getSymbol()->isResolvedMethod() &&
             node->getFirstChild()->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->isSideEffectFree() &&
             performTransformation(comp(), "%sRemove dead check of side-effect free call: %p\n", optDetailString(), node))
            {
            TR::TransformUtil::removeTree(comp(), tt);
            removed = true;
            }
         }

      if (removed
          && tt->getNextTreeTop()->getNode()->getOpCodeValue() == TR::Goto
          && tt->getPrevTreeTop()->getNode()->getOpCodeValue() == TR::BBStart
          && !tt->getPrevTreeTop()->getNode()->getBlock()->isExtensionOfPreviousBlock())
         {
         requestOpt(OMR::redundantGotoElimination, true, tt->getEnclosingBlock());
         }

      if (node->getVisitCount() >= visitCount)
         continue;

      if (comp()->useCompressedPointers() && !removed &&
         isReadBarrierUnderTreetop(node) &&
         node->getFirstChild()->getType() == TR::Address && node->getFirstChild()->getOpCode().isLoadIndirect())
         {
         ncount_t nodeIndex = node->getFirstChild()->getGlobalIndex();
         rdbar2ttMap[nodeIndex] = tt;
         }
      TR::TransformUtil::recursivelySetNodeVisitCount(tt->getNode(), visitCount);
      }

   // If the last use of an iRegLoad has been removed, then remove the node from
   // the BBStart and remove the corresponding dependency node from each of the block's
   // predecessors.
   //
   while (1)
      {
      bool glRegDepRemoved = false;
      for (TR::Block * b = comp()->getStartBlock(); b; b = b->getNextBlock())
         {
         TR::TreeTop * startTT = b->getEntry();
         TR::Node * startNode = startTT->getNode();
         if (startNode->getNumChildren() > 0 && !debug("disableEliminationOfGlRegDeps"))
            {
            TR::Node * glRegDeps = startNode->getFirstChild();
            TR_ASSERT(glRegDeps->getOpCodeValue() == TR::GlRegDeps, "expected TR::GlRegDeps");
            for (int32_t i = glRegDeps->getNumChildren() - 1; i >= 0; --i)
               {
               TR::Node * dep = glRegDeps->getChild(i);
               if (dep->getReferenceCount() == 1 &&
                   (!dep->getOpCode().isFloatingPoint() ||
                    cg()->getSupportsJavaFloatSemantics()) &&
                   performTransformation(comp(), "%sRemove GlRegDep : %p\n", optDetailString(), glRegDeps->getChild(i)))

                  {
                  glRegDeps->removeChild(i);
                  glRegDepRemoved = true;
                  TR_GlobalRegisterNumber registerNum = dep->getGlobalRegisterNumber();
                  for (auto e = b->getPredecessors().begin(); e != b->getPredecessors().end(); ++e)
                     {
                     TR::Block * pred = toBlock((*e)->getFrom());
                     if (pred == comp()->getFlowGraph()->getStart())
                        continue;

                     TR::Node * parent = pred->getLastRealTreeTop()->getNode();
                     if ( parent->getOpCode().isJumpWithMultipleTargets() && parent->getOpCode().hasBranchChildren())
                        {
                        for (int32_t j = parent->getCaseIndexUpperBound() - 1; j > 0; --j)
                           {
                           TR::Node * caseNode = parent->getChild(j);
                           TR_ASSERT(caseNode->getOpCode().isCase() || caseNode->getOpCodeValue() == TR::branch,
                                  "having problems navigating a switch");
                           if (caseNode->getBranchDestination() == startTT &&
                               caseNode->getNumChildren() > 0 &&
                               0) // can't do this now that all glRegDeps are hung off the default branch
                              removeGlRegDep(caseNode, registerNum, pred, this);
                           }
                        }
                     else if (!parent->getOpCode().isReturn() &&
                              parent->getOpCodeValue() != TR::igoto &&
                              !( parent->getOpCode().isJumpWithMultipleTargets() && parent->getOpCode().hasBranchChildren()) &&
                              !(parent->getOpCodeValue()==TR::treetop &&
                              parent->getFirstChild()->getOpCode().isCall() &&
                              parent->getFirstChild()->getOpCode().isIndirect()))

                        {
                        if (pred->getNextBlock() == b)
                           parent = pred->getExit()->getNode();
                        removeGlRegDep(parent, registerNum, pred, this);
                        }
                     }
                  }
               }

            if (glRegDeps->getNumChildren() == 0)
               startNode->removeChild(0);
            }
         }

      if (!glRegDepRemoved)
         break;
      }
   }

bool TR::DeadTreesElimination::fixUpTree(TR::Node *node, TR::TreeTop *treeTop, TR::NodeChecklist &visited, bool &highGlobalIndex, vcount_t evaluatedVisitCount)
   {
   if (node->getVisitCount() == evaluatedVisitCount)
      return false;

   if (visited.contains(node))
      return false;

   visited.add(node);

   bool containsFloatingPoint = false;
   bool anchorLoadaddr = true;
   bool anchorArrayCmp = true;

   // for arraycmp node, don't create its tree top anchor
   // fold it into if statment and save jump instruction
   if (node->getOpCodeValue() == TR::arraycmp &&
      comp()->target().cpu.isX86())
      {
      anchorArrayCmp = false;
      }

   if ((node->getReferenceCount() > 1) &&
       !node->getOpCode().isLoadConst() &&
       anchorLoadaddr &&
       anchorArrayCmp)
      {
      if (!comp()->getOption(TR_ProcessHugeMethods))
         {
         int32_t nodeCount = comp()->getNodeCount();
         int32_t nodeCountLimit = 3 * USHRT_MAX / 4;
         if (nodeCount > nodeCountLimit)
            {
            dumpOptDetails(comp(),
               "%snode count %d exceeds limit %d\n",
               optDetailString(), nodeCount, nodeCountLimit);
            highGlobalIndex = true;
            return containsFloatingPoint;
            }
         }

      if (node->getOpCode().isFloatingPoint())
        containsFloatingPoint = true;
      TR::TreeTop *nextTree = treeTop->getNextTreeTop();
      node->incFutureUseCount();
      TR::TreeTop *anchorTreeTop = TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, node));
      anchorTreeTop->getNode()->setFutureUseCount(0);
      treeTop->join(anchorTreeTop);
      anchorTreeTop->join(nextTree);
      }
   else
      {
      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         {
         TR::Node *child = node->getChild(i);
         if (fixUpTree(child, treeTop, visited, highGlobalIndex, evaluatedVisitCount))
            containsFloatingPoint = true;
         }
      }
   return containsFloatingPoint;
   }

namespace
   {
   struct CRAnchor
      {
      TR::TreeTop *tree;
      TR::Block *block;
      CRAnchor(TR::TreeTop *tree, TR::Block *block) : tree(tree), block(block) { }
      };
   }

/** \brief
 *       Tells whether it is possible to remove a tree node without considering actual side effect.
 *       Only the child's reference count is taken into consideration at this stage.
 *
 *  \parm node
 *       The tree node to be considered for removing
 *
 *  \note
 *       In general, anchoring nodes with its child's reference count == 1 might be removed, like
 *       compressedrefs, reg store, rdbar under a treetop. Any TR::treetop node (except rdbar
 *       under a treetop) can be considered for removing no matter what the child's reference count is.
 */
static bool treeCanPossiblyBeRemoved(TR::Node *node)
   {
   // If the tree node is not TR::treetop, it can only be removed if it's anchoring node and no
   // other uses exist
   if (node->getOpCodeValue() != TR::treetop)
      {
      return (node->getOpCode().isAnchor() && node->getFirstChild()->getReferenceCount() == 1) ||
             (node->getOpCode().isStoreReg() && node->getFirstChild()->getReferenceCount() == 1);
      }

   // rdbar under a treetop can also be removed if there are no other uses
   return (!isReadBarrierUnderTreetop(node) || node->getFirstChild()->getReferenceCount() == 1);
   }

int32_t TR::DeadTreesElimination::process(TR::TreeTop *startTree, TR::TreeTop *endTree)
   {
   TR::StackMemoryRegion stackRegion(*comp()->trMemory());

   List<OMR::TreeInfo> targetTrees(stackRegion);

   LongestPathMap longestPaths(std::less<TR::Node*>(), stackRegion);

   typedef TR::typed_allocator<CRAnchor, TR::Region&> CRAnchorAlloc;
   typedef TR::forward_list<CRAnchor, CRAnchorAlloc> CRAnchorList;
   CRAnchorList anchors(stackRegion);

   vcount_t visitCount = comp()->incOrResetVisitCount();
   TR::TreeTop *treeTop;
   for (treeTop = startTree; (treeTop != endTree); treeTop = treeTop->getNextTreeTop())
      treeTop->getNode()->initializeFutureUseCounts(visitCount);

   TR::Block *block = NULL;
   bool delayedRegStoresBeforeThisPass = _delayedRegStores;

   // Update visitCount as they are used in this optimization and need to be
   visitCount = comp()->incOrResetVisitCount();
   for (TR::TreeTopIterator iter(startTree, comp()); iter != endTree; ++iter)
      {
      TR::Node *node = iter.currentTree()->getNode();

      if (node->getOpCodeValue() == TR::BBStart)
         {
         block = node->getBlock();
         if (!block->isExtensionOfPreviousBlock())
            longestPaths.clear();
         }

      int vcountLimit = MAX_VCOUNT - 3;
      if (comp()->getVisitCount() > vcountLimit)
         {
         dumpOptDetails(comp(),
            "%sVisit count %d exceeds limit %d; stopping\n",
            optDetailString(), comp()->getVisitCount(), vcountLimit);
         return 0;
         }

      // correct at all intermediate stages
      //
      if (!treeCanPossiblyBeRemoved(node) &&
          (delayedRegStoresBeforeThisPass ||
           (iter.currentTree() == block->getLastRealTreeTop()) ||
           !node->getOpCode().isStoreReg() ||
           (node->getVisitCount() == visitCount)))
         {
         /*
          * second chance for anchoring nodes like compressedrefs
          * Given the following trees, the anchoring node can still be removed  if the first treetop is removed
          *
          * treetop
          *    xloadi #x
          * anchor
          *    =>xloadi #x
          */
         if (node->getOpCode().isAnchor() && node->getFirstChild()->getOpCode().isLoadIndirect())
            anchors.push_front(CRAnchor(iter.currentTree(), block));

         TR::TransformUtil::recursivelySetNodeVisitCount(node, visitCount);
         continue;
         }

      if (node->getOpCode().isStoreReg())
         _delayedRegStores = true;

      TR::Node *child = node->getFirstChild();
      if (child->getOpCodeValue() == TR::PassThrough)
         {
         TR::Node *newChild = child->getFirstChild();
         node->setAndIncChild(0, newChild);
         newChild->incFutureUseCount();
         if (child->getReferenceCount() <= 1)
            optimizer()->prepareForNodeRemoval(child);
         child->recursivelyDecReferenceCount();
         recursivelyDecFutureUseCount(child);
         child = newChild;
         }

      bool treeTopCanBeEliminated = false;

      // If the treetop child has been seen before then it must be anchored
      // somewhere above already; so we don't need the treetop to be anchoring
      // this node (as the computation is already done at the first reference to
      // the node).
      //

      if (visitCount == child->getVisitCount())
         {
         treeTopCanBeEliminated = true;
         }
      else
         {
         TR::ILOpCode &childOpCode = child->getOpCode();
         TR::ILOpCodes opCodeValue = childOpCode.getOpCodeValue();
         bool seenConditionalBranch = false;

         bool callWithNoSideEffects = child->getOpCode().isCall() &&
              child->getSymbolReference()->getSymbol()->isResolvedMethod() &&
              child->getSymbolReference()->getSymbol()->castToResolvedMethodSymbol()->isSideEffectFree();

         if (callWithNoSideEffects)
            {
            treeTopCanBeEliminated = true;
            }
         else if (!((childOpCode.isCall() && !callWithNoSideEffects) ||
               childOpCode.isStore() ||
               ((opCodeValue == TR::New ||
                 opCodeValue == TR::anewarray ||
                 opCodeValue == TR::newarray) &&
                 child->getReferenceCount() > 1) ||
                 opCodeValue == TR::multianewarray ||
               opCodeValue == TR::checkcast ||
               opCodeValue == TR::Prefetch ||
               opCodeValue == TR::iu2l ||
               ((childOpCode.isDiv() ||
                 childOpCode.isRem()) &&
                 child->getNumChildren() == 3)))
            {
            // Perform the rather complex check to see whether its safe
            // to disconnect the child node from the treetop
            //
            bool safeToReplaceNode = false;
            if (child->getReferenceCount() == 1)
               {
               safeToReplaceNode = true;
               if (opCodeValue == TR::loadaddr)
                  treeTopCanBeEliminated = true;
               }
            else if (!_cannotBeEliminated)
               {
               safeToReplaceNode = isSafeToReplaceNode(
                  child,
                  iter.currentTree(),
                  &seenConditionalBranch,
                  visitCount,
                  comp(),
                  this,
                  &targetTrees,
                  _cannotBeEliminated,
                  longestPaths);
               }

            if (safeToReplaceNode)
               {
               if (childOpCode.hasSymbolReference())
                  {
                  TR::SymbolReference *symRef = child->getSymbolReference();

                  if (symRef->getSymbol()->isAuto() || symRef->getSymbol()->isParm())
                     treeTopCanBeEliminated = true;
                  else
                     {
                     if (childOpCode.isLoad() ||
                         (opCodeValue == TR::loadaddr) ||
                         (opCodeValue == TR::instanceof) ||
                         (((opCodeValue == TR::New)  ||
                            (opCodeValue == TR::anewarray ||
                              opCodeValue == TR::newarray)) &&
                           child->markedAllocationCanBeRemoved()))
                        treeTopCanBeEliminated = true;
                     }
                  }
               else
                  treeTopCanBeEliminated = true;
               }
            }

         // Fix for the case when a float to non-float conversion node swings
         // down past a branch on IA32; this would cause a FP value to be commoned
         // across a branch where there was none originally; this causes pblms
         // as a value is left on the stack.
         //
         if (treeTopCanBeEliminated &&
             seenConditionalBranch)
            {
            if (!cg()->getSupportsJavaFloatSemantics())
               {
               if (child->getOpCode().isConversion() ||
                   child->getOpCode().isBooleanCompare())
                 {
                 if (child->getFirstChild()->getOpCode().isFloatingPoint() &&
                     !child->getOpCode().isFloatingPoint())
                     treeTopCanBeEliminated = false;
                 }
               }
            }

         if (treeTopCanBeEliminated)
            {
            TR::NodeChecklist visited(comp());
            bool containsFloatingPoint = false;
            for (int32_t i = 0; i < child->getNumChildren(); ++i)
               {
               // Anchor nodes with reference count > 1
               //
               bool highGlobalIndex = false;
               if (fixUpTree(child->getChild(i), iter.currentTree(), visited, highGlobalIndex, visitCount))
                  containsFloatingPoint = true;
               if (highGlobalIndex)
                  {
                  dumpOptDetails(comp(),
                     "%sGlobal index limit exceeded; stopping\n",
                     optDetailString());
                  return 0;
                  }
               }

            if (seenConditionalBranch &&
                containsFloatingPoint)
               {
               if (!cg()->getSupportsJavaFloatSemantics())
                  treeTopCanBeEliminated = false;
               }
            }
         }

      // Update visitCount as they are used in this optimization and need to be
      // correct at all intermediate stages
      //
      if (!treeTopCanBeEliminated)
         TR::TransformUtil::recursivelySetNodeVisitCount(node, visitCount);

      if (treeTopCanBeEliminated)
         {
         TR::TreeTop *prevTree = iter.currentTree()->getPrevTreeTop();
         TR::TreeTop *nextTree = iter.currentTree()->getNextTreeTop();

         if (!node->getOpCode().isStoreReg() || (node->getFirstChild()->getReferenceCount() == 1))
            {
            // Actually going to remove the treetop now
            //
            if (performTransformation(comp(), "%sRemove tree : [" POINTER_PRINTF_FORMAT "] ([" POINTER_PRINTF_FORMAT "] = %s)\n", optDetailString(), node, node->getFirstChild(), node->getFirstChild()->getOpCode().getName()))
               {
               prevTree->join(nextTree);
               optimizer()->prepareForNodeRemoval(node);
               ///child->recursivelyDecReferenceCount();
               node->recursivelyDecReferenceCount();
               recursivelyDecFutureUseCount(child);
               iter.jumpTo(prevTree);
               if (child->getReferenceCount() == 1)
                  requestOpt(OMR::treeSimplification, true, block);

               if (nextTree->getNode()->getOpCodeValue() == TR::Goto
                   && prevTree->getNode()->getOpCodeValue() == TR::BBStart
                   && !prevTree->getNode()->getBlock()->isExtensionOfPreviousBlock())
                  {
                  requestOpt(
                     OMR::redundantGotoElimination,
                     true,
                     prevTree->getNode()->getBlock());
                  }
               }
            }
         else
            {
            if (performTransformation(comp(), "%sMove tree : [" POINTER_PRINTF_FORMAT "]([" POINTER_PRINTF_FORMAT "] = %s) to end of block\n", optDetailString(), node, node->getFirstChild(), node->getFirstChild()->getOpCode().getName()))
               {
               prevTree->join(nextTree);
               node->setVisitCount(visitCount);

               TR::TreeTop *lastTree = findLastTreetop(block, prevTree);
               TR::TreeTop *prevLastTree = lastTree->getPrevTreeTop();

               TR::TreeTop *cursorTreeTop = nextTree;
               while (cursorTreeTop != lastTree)
                  {
                  if (cursorTreeTop->getNode()->getOpCode().isStoreReg() &&
                      (cursorTreeTop->getNode()->getGlobalRegisterNumber() == iter.currentTree()->getNode()->getGlobalRegisterNumber()))
                     {
                     lastTree = cursorTreeTop;
                     prevLastTree = lastTree->getPrevTreeTop();
                     break;
                     }

                  cursorTreeTop = cursorTreeTop->getNextTreeTop();
                  }

               if (lastTree->getNode()->getOpCodeValue() == TR::BBStart)
                  {
                  prevLastTree = lastTree;
                  lastTree = block->getExit();
                  }

               TR::Node *lastNode = lastTree->getNode();
               TR::Node *prevLastNode = prevLastTree->getNode();

               // If the node being moved is volatile, the order of the trees cannot
               // be changed because that is the order containsNode used to check if
               // canMoveIfVolatile should be true or not.
               // For example, if the order of the trees look like below, adjusting
               // the lastTree and prevLastTree in the following code would end up
               // mistakenly reordering the RegStore nodes whose children are volatile
               // loads.
               //
               // E.g
               // === Before adjusting lastTree & prevLastTree
               // TT1
               // TT2 (iRegStore) <--- prevLastTree
               //    iloadi (volatile)
               // TTBeingMoved (iRegStore)
               //    iloadi (volatile)
               // TT3 <--- lastTree
               //
               // === After adjusting lastTree & prevLastTree
               // TT1 <--- prevLastTree
               // TTBeingMoved (iRegStore) // Incorrect: should not be before TT2
               //    iloadi (volatile)
               // TT2 (iRegStore) <--- lastTree  // Incorrect: should not be after TTBeingMoved
               //    iloadi (volatile)
               // TT3

               if (!node->getFirstChild()->mightHaveVolatileSymbolReference() &&
                   lastNode->getOpCode().isIf() && !lastNode->getOpCode().isCompBranchOnly() &&
                   prevLastNode->getOpCode().isStoreReg() &&
                   ((prevLastNode->getFirstChild() == lastNode->getFirstChild()) ||
                    (prevLastNode->getFirstChild() == lastNode->getSecondChild())))
                  {
                  lastTree = prevLastTree;
                  prevLastTree = lastTree->getPrevTreeTop();
                  }

               prevLastTree->join(iter.currentTree());
               iter.currentTree()->join(lastTree);

               iter.jumpTo(prevTree);
               requestOpt(OMR::treeSimplification, true, block);
               }
            }
         }
      }

   for (auto it = anchors.begin(); it != anchors.end(); ++it)
      {
      TR::Node *anchor = it->tree->getNode();
      TR::Node *load = anchor->getChild(0);
      if (load->getReferenceCount() > 1)
         continue;

      // We can eliminate the indirect load immediately, but for the moment the
      // subtree providing the base object has to be anchored.

      TR::Node *heapBase = anchor->getChild(1);

      TR::Node::recreate(anchor, TR::treetop);
      anchor->setAndIncChild(0, load->getChild(0));
      anchor->setChild(1, NULL);
      anchor->setNumChildren(1);

      if (!heapBase->getOpCode().isLoadConst())
         {
         it->tree->insertAfter(
            TR::TreeTop::create(
               comp(),
               TR::Node::create(heapBase, TR::treetop, 1, heapBase)));
         }

      load->recursivelyDecReferenceCount();
      heapBase->recursivelyDecReferenceCount();

      // A later pass of dead trees can likely move (or even remove) the base
      // object expression.

      requestOpt(OMR::deadTreesElimination, true, it->block);
      }

   return 1; // actual cost
   }

TR::TreeTop *TR::DeadTreesElimination::findLastTreetop(TR::Block *block, TR::TreeTop *prevTree)
   {
   return block->getLastRealTreeTop();
   }

const char *
TR::DeadTreesElimination::optDetailString() const throw()
   {
   return "O^O DEAD TREES ELIMINATION: ";
   }
