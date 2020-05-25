/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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

#include "ras/ILValidationRules.hpp"

#include "il/Block.hpp"
#include "il/DataTypes.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "infra/Checklist.hpp"
#include "infra/ILWalk.hpp"
#include "ras/ILValidationStrategies.hpp"
#include "ras/ILValidationUtils.hpp"

/**
 * The ILValidation Rules are identified by the TR::ILValidator via
 * their associated `id`.
 * Each of the ILValidationRules below must have their own unique `id`.
 *
 * NOTE: For adding a new ILValidationRule, please update ILValidationStrategies.hpp.
 *       The `id` assigned in the new ILValidationRule must then match the newly
 *       defined one in ILValidationStrategies.hpp (which is a `OMR::ILValidationRule` value).
 */

/**
 * SoundnessRule (a TR::MethodValidationRule) :
 *
 * "Soundness" comprises the criteria required to make the IL iterators
 * function properly.
 *
 * NOTE: The "stop" tree is not checked.
 */
TR::SoundnessRule::SoundnessRule(TR::Compilation *comp)
   : TR::MethodValidationRule(comp, OMR::soundnessRule)
   {
   }

void TR::SoundnessRule::validate(TR::ResolvedMethodSymbol *methodSymbol)
   {
   TR::TreeTop *start = methodSymbol->getFirstTreeTop();
   TR::TreeTop *stop = methodSymbol->getLastTreeTop();
   checkSoundnessCondition(start, start != NULL, "Start tree must exist");
   checkSoundnessCondition(stop, !stop || stop->getNode() != NULL,
                           "Stop tree must have a node");

   TR::NodeChecklist treetopNodes(comp()), ancestorNodes(comp()), visitedNodes(comp());

   /* NOTE: Can't use iterators here, because iterators presuppose that the IL is sound. */
   for (TR::TreeTop *currentTree = start; currentTree != stop;
        currentTree = currentTree->getNextTreeTop())
      {
      checkSoundnessCondition(currentTree, currentTree->getNode() != NULL,
                              "Tree must have a node");
      checkSoundnessCondition(currentTree, !treetopNodes.contains(currentTree->getNode()),
                              "Treetop node n%dn encountered twice",
                              currentTree->getNode()->getGlobalIndex());

      treetopNodes.add(currentTree->getNode());

      TR::TreeTop *next = currentTree->getNextTreeTop();
      if (next)
         {
         checkSoundnessCondition(currentTree, next->getNode() != NULL,
                                 "Tree after n%dn must have a node",
                                 currentTree->getNode()->getGlobalIndex());
         checkSoundnessCondition(currentTree, next->getPrevTreeTop() == currentTree,
                                 "Doubly-linked treetop list must be consistent: n%dn->n%dn<-n%dn",
                                 currentTree->getNode()->getGlobalIndex(),
                                 next->getNode()->getGlobalIndex(),
                                 next->getPrevTreeTop()->getNode()->getGlobalIndex());
         }
      else
         {
         checkSoundnessCondition(currentTree, stop == NULL,
                                 "Reached the end of the trees after n%dn without encountering the stop tree n%dn",
                                 currentTree->getNode()->getGlobalIndex(),
                                 stop? stop->getNode()->getGlobalIndex() : 0);
         checkNodeSoundness(currentTree, currentTree->getNode(),
                            ancestorNodes, visitedNodes);
         }
      }
   }

void TR::SoundnessRule::checkNodeSoundness(TR::TreeTop *location, TR::Node *node,
                                           TR::NodeChecklist &ancestorNodes,
                                           TR::NodeChecklist &visitedNodes)
   {
   TR_ASSERT(node != NULL, "checkNodeSoundness requires that node is not NULL");
   if (visitedNodes.contains(node))
      {
      return;
      }
   visitedNodes.add(node);
   checkSoundnessCondition(location, !ancestorNodes.contains(node),
                           "n%dn must not be its own ancestor",
                           node->getGlobalIndex());
   ancestorNodes.add(node);
   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      checkSoundnessCondition(location, child != NULL, "n%dn child %d must not be NULL",
                              node->getGlobalIndex(), i);
      checkNodeSoundness(location, child, ancestorNodes, visitedNodes);
      }
   ancestorNodes.remove(node);
   }


void TR::SoundnessRule::checkSoundnessCondition(TR::TreeTop *location, bool condition,
                                                const char *formatStr, ...)
   {
   if (!condition)
      {
      if (location && location->getNode())
         {
         TR::printILDiagnostic(comp(), "*** VALIDATION ERROR: IL is unsound at n%dn ***\nMethod: %s\n",
                               location->getNode()->getGlobalIndex(), comp()->signature());
         }
      else
         {
         TR::printILDiagnostic(comp(), "*** VALIDATION ERROR: IL is unsound ***\nMethod: %s\n",
                               comp()->signature());
         }
      va_list args;
      va_start(args, formatStr);
      TR::vprintILDiagnostic(comp(), formatStr, args);
      va_end(args);
      TR::printILDiagnostic(comp(), "\n");
      if (!comp()->getOption(TR_ContinueAfterILValidationError))
         {
         TR::trap();
         }
      }
   }

/**
 * ValidateLivenessBoundaries (a TR::MethodValidationRule):
 *
 * Validates NodeLivnessBoundaries across the entire `method`
 * by checking that no nodes are Live across block boundaries.
 *
 */

TR::ValidateLivenessBoundaries::ValidateLivenessBoundaries(TR::Compilation *comp)
   : TR::MethodValidationRule(comp, OMR::validateLivenessBoundaries)
   {
   }

void TR::ValidateLivenessBoundaries::validate(TR::ResolvedMethodSymbol *methodSymbol)
   {
   /**
    * These must be initialized at the start of every validate call,
    * since the same Rule object can be used multiple times to validate
    * the IL at different stages of the compilation.
    */
   TR::NodeSideTable<TR::NodeState> nodeStates(comp()->trMemory());
   /**
    * Similar to NodeChecklist, but more compact. Rather than track
    * node global indexes, which can be sparse, this tracks local
    * indexes, which are relatively dense.  Furthermore, the _basis field
    * allows us not to waste space on nodes we saw in prior blocks.
    * As the name suggests, used to keep track of live Nodes.
    */
   TR::LiveNodeWindow liveNodes(nodeStates, comp()->trMemory());

   TR::TreeTop *start = methodSymbol->getFirstTreeTop();
   TR::TreeTop *stop = methodSymbol->getLastTreeTop();
   for (TR::PostorderNodeOccurrenceIterator iter(start, comp(), "VALIDATE_LIVENESS_BOUNDARIES");
        iter != stop; ++iter)
      {
      TR::Node *node = iter.currentNode();
      updateNodeState(node, nodeStates, liveNodes);
      if (node->getOpCodeValue() == TR::BBEnd)
         {
         /* Determine whether this is the end of an extended block */
         bool isEndOfExtendedBlock = false;
         TR::TreeTop *nextTree = iter.currentTree()->getNextTreeTop();
         if (nextTree)
            {
            TR::checkILCondition(node, nextTree->getNode()->getOpCodeValue() == TR::BBStart,
                                 comp(), "Expected BBStart after BBEnd");
            isEndOfExtendedBlock = ! nextTree->getNode()->getBlock()->isExtensionOfPreviousBlock();
            }
         else
            {
            isEndOfExtendedBlock = true;
            }
         if (isEndOfExtendedBlock)
            {
            /* Ensure there are no nodes live across the end of a block */
            validateEndOfExtendedBlockBoundary(node, liveNodes);
            }
         }
      }
   }

void TR::ValidateLivenessBoundaries::validateEndOfExtendedBlockBoundary(TR::Node *node,
                                                                        TR::LiveNodeWindow &liveNodes)
   {
   for (LiveNodeWindow::Iterator lnwi(liveNodes); lnwi.currentNode(); ++lnwi)
      {
      TR::checkILCondition(node, false, comp(),
                           "Node cannot live across block boundary at n%dn",
                           lnwi.currentNode()->getGlobalIndex());
      }
   /**
    * At the end of an extended block, no node we've already seen could ever be seen again.
    * Slide the live node window to keep its bitvector compact.
    */
   if (liveNodes.isEmpty())
      {
      liveNodes.startNewWindow();
      }
   }

void TR::ValidateLivenessBoundaries::updateNodeState(TR::Node *node,
                                                     TR::NodeSideTable<TR::NodeState>  &nodeStates,
                                                     TR::LiveNodeWindow &liveNodes)
   {
   TR::NodeState &state = nodeStates[node];
   if (node->getReferenceCount() == state._futureReferenceCount)
      {
      /* First occurrence -- do some bookkeeping */
      if (node->getReferenceCount() == 0)
         {
         TR::checkILCondition(node, node->getOpCode().isTreeTop(), comp(),
                              "Only nodes with isTreeTop opcodes can have refcount == 0");
         }
      else
         {
         liveNodes.add(node);
         }
      }

   if (liveNodes.contains(node))
      {
      TR::checkILCondition(node, state._futureReferenceCount >= 1, comp(),
                           "Node already has reference count 0");
      if (--state._futureReferenceCount == 0)
         {
         liveNodes.remove(node);
         }
      }
   else
      {
      TR::checkILCondition(node, node->getOpCode().isTreeTop(), comp(),
                           "Node has already gone dead");
      }

   if (TR::isILValidationLoggingEnabled(comp()))
      {
      if (!liveNodes.isEmpty())
         {
         traceMsg(comp(), "    -- Live nodes: {");
         char *separator = "";
         for (TR::LiveNodeWindow::Iterator lnwi(liveNodes); lnwi.currentNode(); ++lnwi)
            {
            traceMsg(comp(), "%sn%dn", separator,
                     lnwi.currentNode()->getGlobalIndex());
            separator = ", ";
            }
         traceMsg(comp(), "}\n");
         }
      }
   }


/**
 * ValidateNodeRefCountWithinBlock (a TR::BlockValidationRule):
 *
 * Verifies the number of times a node is referenced within a block
 *
 */
TR::ValidateNodeRefCountWithinBlock::ValidateNodeRefCountWithinBlock(TR::Compilation *comp)
   : TR::BlockValidationRule(comp, OMR::validateNodeRefCountWithinBlock)
   {
   }

void TR::ValidateNodeRefCountWithinBlock::validate(TR::TreeTop *firstTreeTop,
                                                      TR::TreeTop *exitTreeTop)
   {
   _nodeChecklist.empty();
   for (TR::TreeTop *tt = firstTreeTop; tt != exitTreeTop->getNextTreeTop();
        tt = tt->getNextTreeTop())
      {
      TR::Node *node = tt->getNode();
      node->setLocalIndex(node->getReferenceCount());
      validateRefCountPass1(node);
      }

   /**
    * We start again from the start of the block, and check the localIndex to
    * make sure it is 0.
    *
    * NOTE: Walking the tree backwards causes huge stack usage in validateRefCountPass2.
    */
   _nodeChecklist.empty();
   for (TR::TreeTop *tt = firstTreeTop; tt != exitTreeTop->getNextTreeTop();
        tt = tt->getNextTreeTop())
      {
      validateRefCountPass2(tt->getNode());
      }
   }

/**
 * In pass_1(validateRefCountPass1), the Local Index (which is set to the Ref
 * Count) for each child is decremented for each visit. The second pass is to
 * make sure that the Local Index is zero by the end of the block. A non-zero
 * Local Index would indicate that the Ref count was wrong at the start
 * of the Validation Process.
 */
void TR::ValidateNodeRefCountWithinBlock::validateRefCountPass1(TR::Node *node)
   {
   /* If this is the first time through this node, verify the children. */
   if (!_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      _nodeChecklist.set(node->getGlobalIndex());
      for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
         {
         TR::Node *child = node->getChild(i);
         if (_nodeChecklist.isSet(child->getGlobalIndex()))
            {
            /* If the child has already been visited, decrement its verifyRefCount. */
            child->decLocalIndex();
            }
         else
            {
            /* If the child has not yet been visited, set its localIndex and visit it. */
            child->setLocalIndex(child->getReferenceCount() - 1);
            validateRefCountPass1(child);
            }
         }
      }
   }

void TR::ValidateNodeRefCountWithinBlock::validateRefCountPass2(TR::Node *node)
   {
   /* Pass through and make sure that the localIndex == 0 for each child. */
   if (!_nodeChecklist.isSet(node->getGlobalIndex()))
      {
      _nodeChecklist.set(node->getGlobalIndex());
      for (int32_t i = node->getNumChildren() - 1; i >= 0; --i)
         {
         validateRefCountPass2(node->getChild(i));
         }

      TR::checkILCondition(node, node->getLocalIndex() == 0, comp(),
                           "Node accessed outside of its (extended) basic block: %d time(s)",
                           node->getLocalIndex());
      }
   }


/**
 * ValidateChildCount (a TR::NodeValidationRule):
 *
 * Validates that a particular Node has the expected number of children.
 * A majority of these expectations are defined in `compiler/il/ILOpCodeProperties.hpp`.
 *
 */
TR::ValidateChildCount::ValidateChildCount(TR::Compilation *comp)
   : TR::NodeValidationRule(comp, OMR::validateChildCount)
   {
   }

void TR::ValidateChildCount::validate(TR::Node *node)
   {
   auto opcode = node->getOpCode();

   if (opcode.expectedChildCount() != ILChildProp::UnspecifiedChildCount)
      {
      const auto expChildCount = opcode.expectedChildCount();
      const auto actChildCount = node->getNumChildren();

      /* Validate child count. */
      if (!opcode.canHaveGlRegDeps())
         {
         /* In the common case, no GlRegDeps child is expected nor present. */
         TR::checkILCondition(node, actChildCount == expChildCount, comp(),
                              "Child count %d does not match expected value of %d",
                              actChildCount, expChildCount);
         }
      else if (actChildCount == (expChildCount + 1))
         {
         /**
          * Adjust expected child number to account for a possible extra GlRegDeps
          * child and make sure the last child is actually a GlRegDeps.
          */
         TR::checkILCondition(node, node->getChild(actChildCount - 1)->getOpCodeValue() == TR::GlRegDeps, comp(),
                              "Child count %d does not match expected value of %d (%d without GlRegDeps) and last child is not a GlRegDeps",
                              actChildCount, expChildCount + 1, expChildCount);
         }
      else
         {
         /**
          * If expected and actual child counts don't match, then the child
          * count is just wrong, even with an expected GlRegDeps.
          */
         TR::checkILCondition(node, actChildCount == expChildCount, comp(),
                              "Child count %d matches neither expected values of %d (without GlRegDeps) nor %d (with GlRegDeps)",
                              actChildCount, expChildCount, expChildCount + 1);
         }
      }
   }


/**
 * ValidateChildTypes (a TR::NodeValidationRule):
 *
 * Validates that a particular Node has the expected child type.
 * A majority of these  expectations are defined in
 * `compiler/il/ILOpCodeProperties.hpp`.
 */
TR::ValidateChildTypes::ValidateChildTypes(TR::Compilation *comp)
   : TR::NodeValidationRule(comp, OMR::validateChildTypes)
   {
   }

void TR::ValidateChildTypes::validate(TR::Node *node)
   {
   auto opcode = node->getOpCode();
   if (opcode.expectedChildCount() != ILChildProp::UnspecifiedChildCount)
      {
      const auto expChildCount = opcode.expectedChildCount();
      const auto actChildCount = node->getNumChildren();
      /* Validate child types. */
      for (auto i = 0; i < actChildCount; ++i)
         {
         auto childOpcode = node->getChild(i)->getOpCode();
         if (childOpcode.getOpCodeValue() != TR::GlRegDeps)
            {
            const auto expChildType = opcode.expectedChildType(i);
            const auto actChildType = childOpcode.getDataType().getDataType();
            const auto expChildTypeName = (expChildType >= TR::NumTypes) ?
                                           "UnspecifiedChildType" :
                                           TR::DataType::getName(expChildType);
            const auto actChildTypeName = TR::DataType::getName(actChildType);
            TR::checkILCondition(node, (expChildType >= TR::NumTypes || actChildType == expChildType),
                                 comp(), "Child %d has unexpected type %s (expected %s)",
                                 i, actChildTypeName, expChildTypeName);
            }
         else
            {
            /**
             * Make sure the node is allowed to have a GlRegDeps child
             * and check that it is the last child.
             */
            TR::checkILCondition(node, opcode.canHaveGlRegDeps() && (i == actChildCount - 1),
                                 comp(), "Unexpected GlRegDeps child %d", i);
            }
         }
      }
   }

/**
 * Validate_ireturnReturnType (a TR::NodeValidationRule):
 */
TR::Validate_ireturnReturnType::Validate_ireturnReturnType(TR::Compilation *comp)
   : TR::NodeValidationRule(comp, OMR::validate_ireturnReturnType)
   {
   }

void TR::Validate_ireturnReturnType::validate(TR::Node *node)
   {
   auto opcode = node->getOpCode();
   if (opcode.getOpCodeValue() == TR::ireturn)
      {
      const auto childCount = node->getNumChildren();
      for (auto i = 0; i < childCount; ++i)
         {
         auto childOpcode = node->getChild(i)->getOpCode();
         const auto actChildType = childOpcode.getDataType().getDataType();
         const auto childTypeName = TR::DataType::getName(actChildType);
         TR::checkILCondition(node, (actChildType == TR::Int32 ||
                                     actChildType == TR::Int16 ||
                                     actChildType == TR::Int8),
                              comp(),"ireturn has an invalid child type %s (expected Int{8,16,32})",
                              childTypeName);
         }
      }
   }

/**
 * Validate_axaddEnvironment (a TR::NodeValidationRule):
 */
TR::Validate_axaddEnvironment::Validate_axaddEnvironment(TR::Compilation *comp)
   : TR::NodeValidationRule(comp, OMR::validate_axaddEnvironment)
   {
   }

void TR::Validate_axaddEnvironment::validate(TR::Node *node)
   {
   const auto opcode = node->getOpCode().getOpCodeValue();
   if (opcode == TR::aiadd)
      {
      TR::checkILCondition(node, comp()->target().is32Bit(), comp(), "aiadd should not be seen on 64-bit");
      }
   if (opcode == TR::aladd)
      {
      TR::checkILCondition(node, comp()->target().is64Bit(), comp(), "aladd should not be seen on 32-bit");
      }
   }
