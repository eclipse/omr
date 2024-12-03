/*******************************************************************************
 * Copyright IBM Corp. and others 2014
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

#ifndef UNSAFESUBEXPRESSIONREMOVER_INCL
#define UNSAFESUBEXPRESSIONREMOVER_INCL

#include "compile/Compilation.hpp"
#include "infra/BitVector.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "optimizer/Optimization.hpp"
#include "optimizer/Optimization_inlines.hpp"

namespace TR { class TreeTop; }

namespace OMR
   {
   /**
    * Utility class that will track any \ref TR::Node that must not be evaluated
    * following some transformation.  It can then restructure trees that contain
    * such nodes to anchor the parts of the tree that are still needed to preserve
    * evaluation order.
    *
    * The parent optimization that has made the transformation that results in
    * nodes that are unsafe to evaluate is responsible for marking such nodes as
    * unsafe via a call to \ref UnsafeSubexpressionRemover::recordDeadUse.
    * After all such nodes have been marked, the safe parts of the tree are
    * anchored and usafe parts vandalized via a call to
    * \ref UnsafeSubexpressionRemover::eliminateStore.
    *
    * Note that this class is only used today in situations where the problematic
    * transformation arose beneath a \TR::Node representing a store operation -
    * hence the name of the \c eliminateStore method.  That method will need to
    * be generalized if this might apply in more situations in the future.
    */
   struct UnsafeSubexpressionRemover
      {
      /**
       * Optimization that is in effect.
       */
      TR::Optimization *_opt;

      /**
       * Bit vector used to track nodes that have already been visited.
       */
      TR_BitVector _visitedNodes;

      /**
       * Bit vector used to track nodes that are considered unsafe to evaluate.
       */
      TR_BitVector _unsafeNodes; // Nodes whose mere evaluation is not safe

      /**
       * Construct an instance of \c UnsafeSubexpressionRemover
       *
       * \param[in] opt The \ref TR::Optimization that is currently running.
       */
      UnsafeSubexpressionRemover(TR::Optimization *opt):
         _opt(opt),
         _visitedNodes(opt->comp()->getNodeCount(), comp()->trMemory(), stackAlloc),
         _unsafeNodes (opt->comp()->getNodeCount(), comp()->trMemory(), stackAlloc)
         {}

      /**
       * Mark the specified \ref TR::Node as unsafe to evaluate.
       *
       * \param[in] node A \c TR::Node instance
       */
      void recordDeadUse(TR::Node *node)
         {
         _visitedNodes.set(node->getGlobalIndex());
         _unsafeNodes .set(node->getGlobalIndex());
         }

      /**
       * Eliminate a store operation anchored at the specified \ref TR::TreeTop,
       * traversing its children looking for any safe trees that will need to be
       * anchored, and eliminating any unsafe references that must not be
       * evaluated.
       *
       * \param[in] treeTop The \c TR::TreeTop that anchors a store operation
       *               that is to be eliminated.  This will be used as the
       *               anchor point before which any safe descendants of the
       *               store will themselves be anchored
       */
      void eliminateStore(TR::TreeTop *treeTop);

      private:

      /**
       * Retrieve the \ref TR::Compilation object that is currently active
       *
       * \returns The \c TR::Compilation object
       */
      TR::Compilation *comp() { return _opt->comp(); }

      /**
       * Indicates whether trace logging is enabled for the current optimization.
       *
       * \returns True if trace logging is enabled for the current optimization;
       *          false, otherwise.
       */
      bool trace() { return _opt->trace(); }

      /**
       * Test whether the specified \ref TR::Node has already been visited.
       *
       * \param[in] node A \c TR::Node instance
       *
       * \returns true if \c node has already been visited; false, otherwise
       */
      bool isVisited(TR::Node *node) { return  _visitedNodes.isSet(node->getGlobalIndex()); }

      /**
       * Test whether the specified \ref TR::Node has been marked as unsafe to evaluate.
       *
       * \param[in] node A \c TR::Node instance
       *
       * \returns true if \c node has been marked as unsafe to evaluate; false, otherwise
       */
      bool isUnsafe(TR::Node *node)
         {
         TR_ASSERT(isVisited(node), "Cannot call isUnsafe on n%dn before anchorSafeChildrenOfUnsafeNodes", node->getGlobalIndex());
         return _unsafeNodes.isSet(node->getGlobalIndex());
         }

      /**
       * Traverse a tree beginning at the specified \ref TR::Node, marking the
       * node as unsafe if any of its children were unsafe.  Then iterate over the
       * children of any node that was found to be unsafe during that reversal,
       * and anchor any child that is safe before the specified \ref TR::TreeTop.
       *
       * \param[in] node An unsafe \c TR::Node whose safe children should be anchored
       * \param[in] anchorPoint A \c TR::TreeTop before which any safe children will
       *                 be anchored
       */
      void anchorSafeChildrenOfUnsafeNodes(TR::Node *node, TR::TreeTop *anchorPoint);

      /**
       * If the specified \ref TR::Node is safe to evaluate, anchor it before the
       * anchor point specified via the \ref TR::TreeTop argument.
       *
       * \param[in] node A \c TR::Node that will be anchored if it is safe to evaluate
       * \param[in] anchorPoint A \c TR::TreeTop before which to anchor the specified
       *                 node, if necessary
       */
      bool anchorIfSafe(TR::Node *node, TR::TreeTop *anchorPoint);
      };
   }
#endif
