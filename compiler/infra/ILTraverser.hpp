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

#ifndef TR_ILTRAVERSER_HPP
#define TR_ILTRAVERSER_HPP

#include "infra/Checklist.hpp"
#include "env/TRMemory.hpp"
#include "compile/Compilation.hpp"

#include <vector>
#include <algorithm>

namespace TR {

// forward declaration of "imported" classes
class ResolvedMethodSymbol;
class Block;
class TreeTop;
class Node;

/**
 * @brief A class that abstracts aways the details of traversing Testarossa IL.
 *
 * This class uses the Observer pattern to separate the actual traversal mechanism
 * from the code that needs to do the traversal. An observer can register itself
 * with an instance of this class to be notified of the various transitions during
 * the traversal.
 *
 * This class expects the input IL to be "sound"; properly structured for traversal.
 */
class ILTraverser
   {
   public:
   explicit ILTraverser(TR::Compilation* comp);

   /**
    * @brief The base class for observers of an instance of ILTraverser
    *
    * Objects inheriting from this class can be registered with an instance of
    * ILTraverser to be notified of the various transitions done during traversal.
    *
    * All signals have an empty body by default so that derived classes need
    * only implement the signals they are interested in.
    *
    * All the events that can be listened to have the same general interface.
    * They all take as argument a pointer to the object being visited. For each
    * type of object that can be visited, there is one signal for when the object
    * is entered, and one signal for when traversal returns to the object after
    * having visited the object's children.
    *
    * Some specialized versions of these signals are also provided to accommodate
    * the traversal requirements of certain object types (e.g. there is a
    * special signal for when a commoned node is being re-visited).
    */
   struct Observer
      {
      virtual ~Observer() = default;

      // emitted when visiting methods
      virtual void visitingMethod(TR::ResolvedMethodSymbol* method) {}
      virtual void returnedToMethod(TR::ResolvedMethodSymbol* method) {}

      // emitted when visiting blocks
      virtual void visitingBlock(TR::Block* block) {}
      virtual void returnedToBlock(TR::Block* block) {}

      // emitted when visiting TreeTops
      virtual void visitingTreeTop(TR::TreeTop* treeTop) {}
      virtual void returnedToTreeTop(TR::TreeTop* treeTop) {}

      // emitted when visiting IL nodes
      virtual void visitingNode(TR::Node* node) {}
      virtual void visitingCommonedChildNode(TR::Node* node) {}
      virtual void returnedToNode(TR::Node* node) {}
      virtual void visitedAllChildrenOfNode(TR::Node* node) {}
      };

   /**
    * @brief Registers an observer object with this class instance
    * @param o is a pointer to the observer object being registered
    */
   void registerObserver(Observer* o) { _observers.push_back(o); }

   /**
    * @brief Deregisters an observer object from this class instance
    * @param o is a pointer to the observer object being deregistered
    */
   void removeObserver(Observer* o)
      {
      auto i = std::find(_observers.begin(), _observers.end(), o);
      if (i != _observers.end())
         {
         _observers.erase(i);
         }
      }

   /**
    * @brief Traverses the IL of a method
    * @param method is the method containing the IL to be traversed
    *
    * Will visit the method itself and then visit each TreeTop of the method.
    * Note that it will not visit actual blocks.
    */
   void traverse(TR::ResolvedMethodSymbol* method);

   /**
    * @brief Traverses the IL in a (basic) block
    * @param block is the TR:Block instance containing the IL to be traversed
    *
    * Will visit the block itself and then visit each TreeTop in the block.
    */
   void traverse(TR::Block* block);

   /**
    * @brief Traverses the IL between two TreeTops inclusive
    * @param start points to the TreeTop where traversal will begin (inclusive)
    * @param stop points to the Treetop where the traversal will end (inclusive)
    *
    * Will iterate over each TreeTop in the specified range inclusively, visiting
    * the nodes contained by each TreeTop in the process. It should be noted that
    * this function *will not* traverse the blocks in an extended basic block.
    */
   void traverse(TR::TreeTop* start, TR::TreeTop* stop);

   /**
    * @brief Traverses an IL node and its children
    * @param node is the node to be traversed
    *
    * Will visit the specified node, and then visit the node's children in order.
    * The signal `Observer::returnedToNode(node)` is emitted after each child
    * is visited. The signal `Observer::visitedAllChildrenOfNode(node)` is
    * emitted after all children of the node have been visited. If the node is
    * not being visited for the first time (i.e. it is a commoned node), then
    * the signal `Observer::visitingCommonedChildNode(node)` is emitted, instead
    * of the regular `Observer::visitingNode(node)`, and its children are not
    * be re-visited.
    */
   void traverse(TR::Node* node);

   TR::Compilation* comp() { return _comp; }

   private:
   typedef TR::typed_allocator<Observer*, TR::Region&> ObserverAllocator;
   std::vector<Observer*, ObserverAllocator> _observers;
   TR::NodeChecklist _visitedNodes;
   TR::Compilation* _comp;
   };

} // namespace TR

#endif // TR_ILTRAVERSER_HPP
