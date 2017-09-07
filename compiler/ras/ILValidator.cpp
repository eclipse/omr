/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "ras/ILValidator.hpp"

#include "il/DataTypes.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ILProps.hpp"
#include "il/ILOps.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "infra/Assert.hpp"

#include <stdarg.h>

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
#define ABORT() TR::trap()
#else
#define ABORT() comp()->failCompilation<TR::CompilationException>("Validation error")
#endif

#define FAIL() if (!feGetEnv("TR_continueAfterValidationError")) ABORT()

TR::ILValidator::ILValidator(TR::Compilation *comp)
   :_comp(comp)
   ,_isValidSoFar(true)
   ,_nodeStates(comp->trMemory())
   ,_liveNodes(_nodeStates, comp->trMemory())
   {
   }

TR::Compilation *TR::ILValidator::comp()
   {
   return _comp;
   }

void TR::ILValidator::checkSoundness(TR::TreeTop *start, TR::TreeTop *stop)
   {
   soundnessRule(start, start != NULL, "Start tree must exist");
   soundnessRule(stop, !stop || stop->getNode() != NULL, "Stop tree must have a node");

   TR::NodeChecklist treetopNodes(comp()), ancestorNodes(comp()), visitedNodes(comp());

   // Can't use iterators here, because those presuppose the IL is sound.  Walk trees the old-fashioned way.
   //
   for (TR::TreeTop *currentTree = start; currentTree != stop; currentTree = currentTree->getNextTreeTop())
      {
      soundnessRule(currentTree, currentTree->getNode() != NULL, "Tree must have a node");
      soundnessRule(currentTree, !treetopNodes.contains(currentTree->getNode()), "Treetop node n%dn encountered twice", currentTree->getNode()->getGlobalIndex());

      treetopNodes.add(currentTree->getNode());

      TR::TreeTop *next = currentTree->getNextTreeTop();
      if (next)
         {
         soundnessRule(currentTree, next->getNode() != NULL, "Tree after n%dn must have a node", currentTree->getNode()->getGlobalIndex());
         soundnessRule(currentTree, next->getPrevTreeTop() == currentTree, "Doubly-linked treetop list must be consistent: n%dn->n%dn<-n%dn", currentTree->getNode()->getGlobalIndex(), next->getNode()->getGlobalIndex(), next->getPrevTreeTop()->getNode()->getGlobalIndex());
         }
      else
         {
         soundnessRule(currentTree, stop == NULL, "Reached the end of the trees after n%dn without encountering the stop tree n%dn", currentTree->getNode()->getGlobalIndex(), stop? stop->getNode()->getGlobalIndex() : 0);
         checkNodeSoundness(currentTree, currentTree->getNode(), ancestorNodes, visitedNodes);
         }
      }
   }

void TR::ILValidator::checkNodeSoundness(TR::TreeTop *location, TR::Node *node, NodeChecklist &ancestorNodes, NodeChecklist &visitedNodes)
   {
   TR_ASSERT(node != NULL, "checkNodeSoundness requires that node is not NULL");
   if (visitedNodes.contains(node))
      return;
   visitedNodes.add(node);

   soundnessRule(location, !ancestorNodes.contains(node), "n%dn must not be its own ancestor", node->getGlobalIndex());
   ancestorNodes.add(node);

   for (int32_t i = 0; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      soundnessRule(location, child != NULL, "n%dn child %d must not be NULL", node->getGlobalIndex(), i);

      checkNodeSoundness(location, child, ancestorNodes, visitedNodes);
      }

   ancestorNodes.remove(node);
   }

void TR::ILValidator::soundnessRule(TR::TreeTop *location, bool condition, const char *formatStr, ...)
   {
   if (!condition)
      {
      if (location && location->getNode())
         printDiagnostic("*** VALIDATION ERROR: IL is unsound at n%dn ***\nMethod: %s\n", location->getNode()->getGlobalIndex(), comp()->signature());
      else
         printDiagnostic("*** VALIDATION ERROR: IL is unsound ***\nMethod: %s\n", comp()->signature());
      va_list args;
      va_start(args, formatStr);
      vprintDiagnostic(formatStr, args);
      va_end(args);
      printDiagnostic("\n");
      FAIL();
      }
   }

void TR::ILValidator::validityRule(Location &location, bool condition, const char *formatStr, ...)
   {
   if (!condition)
      {
      _isValidSoFar = false;
      TR::Node *node = location.currentNode();
      printDiagnostic("*** VALIDATION ERROR ***\nNode: %s n%dn\nMethod: %s\n", node->getOpCode().getName(), node->getGlobalIndex(), comp()->signature());
      va_list args;
      va_start(args, formatStr);
      vprintDiagnostic(formatStr, args);
      va_end(args);
      printDiagnostic("\n");
      FAIL();
      }
   }

void TR::ILValidator::updateNodeState(Location &newLocation)
   {
   TR::Node  *node = newLocation.currentNode();
   NodeState &state = _nodeStates[node];
   if (node->getReferenceCount() == state._futureReferenceCount)
      {
      // First occurrence -- do some bookkeeping
      //
      if (node->getReferenceCount() == 0)
         {
         validityRule(newLocation, node->getOpCode().isTreeTop(), "Only nodes with isTreeTop opcodes can have refcount == 0");
         }
      else
         {
         _liveNodes.add(node);
         }
      }

   if (_liveNodes.contains(node))
      {
      validityRule(newLocation, state._futureReferenceCount >= 1, "Node already has reference count 0");
      if (--state._futureReferenceCount == 0)
         {
         _liveNodes.remove(node);
         }
      }
   else
      {
      validityRule(newLocation, node->getOpCode().isTreeTop(), "Node has already gone dead");
      }

   if (isLoggingEnabled())
      {
      static const char *traceLiveNodesDuringValidation = feGetEnv("TR_traceLiveNodesDuringValidation");
      if (traceLiveNodesDuringValidation && !_liveNodes.isEmpty())
         {
         traceMsg(comp(), "    -- Live nodes: {");
         char *separator = "";
         for (LiveNodeWindow::Iterator lnwi(_liveNodes); lnwi.currentNode(); ++lnwi)
            {
            traceMsg(comp(), "%sn%dn", separator, lnwi.currentNode()->getGlobalIndex());
            separator = ", ";
            }
         traceMsg(comp(), "}\n");
         }
      }

   }

bool TR::ILValidator::treesAreValid(TR::TreeTop *start, TR::TreeTop *stop)
   {
   checkSoundness(start, stop);

   for (PostorderNodeOccurrenceIterator iter(start, _comp, "VALIDATOR"); iter != stop; ++iter)
      {
      updateNodeState(iter);

      // General node validation
      //
      validateNode(iter);

      //
      // Additional specific kinds of validation
      //

      TR::Node *node = iter.currentNode();
      if (node->getOpCodeValue() == TR::BBEnd)
         {
         // Determine whether this is the end of an extended block
         //
         bool isEndOfExtendedBlock = false;
         TR::TreeTop *nextTree = iter.currentTree()->getNextTreeTop();
         if (nextTree)
            {
            validityRule(iter, nextTree->getNode()->getOpCodeValue() == TR::BBStart, "Expected BBStart after BBEnd");
            isEndOfExtendedBlock = ! nextTree->getNode()->getBlock()->isExtensionOfPreviousBlock();
            }
         else
            {
            isEndOfExtendedBlock = true;
            }

         if (isEndOfExtendedBlock)
            validateEndOfExtendedBlock(iter);
         }

      auto opcode = node->getOpCode();
      if (opcode.expectedChildCount() != ILChildProp::UnspecifiedChildCount)
         {
         // Validate child expectations
         //

         const auto expChildCount = opcode.expectedChildCount();
         const auto actChildCount = node->getNumChildren();

         // validate child count
         if (!opcode.canHaveGlRegDeps())
            {
            // in the common case, no GlRegDeps child is expect nor present
            validityRule(iter, actChildCount == expChildCount,
                         "Child count %d does not match expected value of %d", actChildCount, expChildCount);
            }
         else if (actChildCount == (expChildCount + 1))
            {
            // adjust expected child number to account for a possible extra GlRegDeps
            // child and make sure the last child is actually a GlRegDeps
            validityRule(iter, node->getChild(actChildCount - 1)->getOpCodeValue() == TR::GlRegDeps,
                         "Child count %d does not match expected value of %d (%d without GlRegDeps) and last child is not a GlRegDeps",
                         actChildCount, expChildCount + 1, expChildCount);
            }
         else
            {
            // if expected and actual child counts don't match, then the child
            // count is just wrong, even with an expected GlRegDeps
            validityRule(iter, actChildCount == expChildCount,
                         "Child count %d matches neither expected values of %d (without GlRegDeps) nor %d (with GlRegDeps)",
                         actChildCount, expChildCount, expChildCount + 1);
            }

         // validate child types
         for (auto i = 0; i < actChildCount; ++i)
            {
            auto childOpcode = node->getChild(i)->getOpCode();
            if (childOpcode.getOpCodeValue() != TR::GlRegDeps)
               {
               const auto expChildType = opcode.expectedChildType(i);
               const auto actChildType = childOpcode.getDataType().getDataType();
               const auto expChildTypeName = expChildType == ILChildProp::UnspecifiedChildType ? "UnspecifiedChildType" : TR::DataType::getName(expChildType);
               const auto actChildTypeName = TR::DataType::getName(actChildType);
               validityRule(iter, expChildType == ILChildProp::UnspecifiedChildType || actChildType == expChildType,
                            "Child %d has unexpected type %s (expected %s)" , i, actChildTypeName, expChildTypeName);
               }
            else
               {
               // make sure the node is allowed to have a GlRegDeps child
               // and make sure that it is the last child
               validityRule(iter, opcode.canHaveGlRegDeps() && (i == actChildCount - 1), "Unexpected GlRegDeps child %d", i);
               }
            }
         }
      }

   return _isValidSoFar;
   }

void TR::ILValidator::validateNode(Location &location)
   {
   }

void TR::ILValidator::validateEndOfExtendedBlock(Location &location)
   {
   // Ensure there are no nodes live across the end of a block
   //
   for (LiveNodeWindow::Iterator lnwi(_liveNodes); lnwi.currentNode(); ++lnwi)
      validityRule(location, false, "Node cannot live across block boundary at n%dn", lnwi.currentNode()->getGlobalIndex());

   // At the end of an extended block, no node we've already seen could ever be seen again.
   // Slide the live node window to keep its bitvector compact.
   //
   if (_liveNodes.isEmpty())
      _liveNodes.startNewWindow();
   }

TR::ILValidator::LiveNodeWindow::LiveNodeWindow(NodeSideTable<NodeState> &sideTable, TR_Memory *memory)
   :_sideTable(sideTable)
   ,_basis(0)
   ,_liveOffsets(10, memory, stackAlloc, growable)
   {
   }

bool TR::ILValidator::isLoggingEnabled()
   {
   // TODO: IL validation should have its own logging option.
   return (comp()->getOption(TR_TraceILWalks));
   }

void TR::ILValidator::printDiagnostic(const char *formatStr, ...)
   {
   va_list stderr_args;
   va_start(stderr_args, formatStr);
   vfprintf(stderr, formatStr, stderr_args);
   va_end(stderr_args);
   if (comp()->getOutFile() != NULL)
      {
      va_list log_args;
      va_start(log_args, formatStr);
      comp()->diagnosticImplVA(formatStr, log_args);
      va_end(log_args);
      }
   }

void TR::ILValidator::vprintDiagnostic(const char *formatStr, va_list ap)
   {
   va_list stderr_copy;
   va_copy(stderr_copy, ap);
   vfprintf(stderr, formatStr, stderr_copy);
   va_end(stderr_copy);
   if (comp()->getOutFile() != NULL)
      {
      va_list log_copy;
      va_copy(log_copy, ap);
      comp()->diagnosticImplVA(formatStr, log_copy);
      va_end(log_copy);
      }
   }
