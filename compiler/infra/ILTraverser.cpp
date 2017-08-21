/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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
 ******************************************************************************/

#include "infra/ILTraverser.hpp"

#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/Block.hpp"
#include "il/Block_inlines.hpp"
#include "il/TreeTop.hpp"
#include "il/TreeTop_inlines.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

TR::ILTraverser::ILTraverser(TR::Compilation * comp) : _observers(ObserverAllocator(comp->trMemory()->currentStackRegion())), _visitedNodes(comp), _comp(comp)
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
