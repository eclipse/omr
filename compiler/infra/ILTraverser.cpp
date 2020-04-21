/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#include "infra/ILTraverser.hpp"

#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"

TR::ILTraverser::ILTraverser(TR::Compilation * comp) : _observers(comp->allocator()), _visitedNodes(comp), _comp(comp)
   {
   }

void TR::ILTraverser::traverse(TR::ResolvedMethodSymbol* method)
   {
   for (auto o : _observers) o->visitingMethod(method);
   traverse(method->getFirstTreeTop(), method->getLastTreeTop());
   for (auto o : _observers) o->returnedToMethod(method);
   }

void TR::ILTraverser::traverse(TR::Block* block)
   {
   for (auto o : _observers) o->visitingBlock(block);
   traverse(block->getEntry(), block->getExit());
   for (auto o : _observers) o->returnedToBlock(block);
   }

void TR::ILTraverser::traverse(TR::TreeTop* start, TR::TreeTop* stop)
   {
   for (auto currentTree = start; currentTree != stop; currentTree = currentTree->getNextTreeTop())
      {
      for (auto o : _observers) o->visitingTreeTop(currentTree);
      traverse(currentTree->getNode());
      for (auto o : _observers) o->returnedToTreeTop(currentTree);
      }

   // visit last treetop
   for (auto o : _observers) o->visitingTreeTop(stop);
   traverse(stop->getNode());
   for (auto o : _observers) o->returnedToTreeTop(stop);
   }

void TR::ILTraverser::traverse(TR::Node* node)
   {
   for (auto o : _observers) o->visitingNode(node);

   // add node to the visited list so we don't visit its children again
   // if the node is commoned
   _visitedNodes.add(node);

   // visit all the children
   for (auto childIndex = 0; childIndex < node->getNumChildren(); ++childIndex)
      {
      auto child = node->getChild(childIndex);

      if (!_visitedNodes.contains(child))
         {
         traverse(child);
         }
      else
         {
         for (auto o : _observers) o->visitingCommonedChildNode(child);
         }

      for (auto o : _observers) o->returnedToNode(node);
      }

   for (auto o : _observers) o->visitedAllChildrenOfNode(node);
   }
