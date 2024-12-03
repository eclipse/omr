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

#include "optimizer/UnsafeSubexpressionRemover.hpp"

void
OMR::UnsafeSubexpressionRemover::anchorSafeChildrenOfUnsafeNodes(TR::Node *node, TR::TreeTop *anchorPoint)
   {
   if (isVisited(node))
      return;
   else
      _visitedNodes.set(node->getGlobalIndex());

   //
   // Design note: we don't decrement refcounts in here.  Conceptually,
   // anchoring and decrementing are independent of each other.  More
   // pragmatically, you end up at with a well-defined state when this
   // function finishes: if the top-level node is unsafe, the caller must
   // unhook it and call recursivelyDecReferenceCount, which will do the
   // right thing.  Otherwise, no decrementing is necessary anyway.
   //
   // A rule of thumb to remember is that this function will do nothing if
   // the node is safe.  If you call this, then find !isUnsafe(node), then
   // the trees did not change.
   //

   // Propagate unsafeness from children, unless we've already done it
   //
   for (int32_t i = 0; i < node->getNumChildren(); ++i)
      {
      TR::Node *child = node->getChild(i);
      anchorSafeChildrenOfUnsafeNodes(child, anchorPoint);
      if (isUnsafe(child))
         {
         _unsafeNodes.set(node->getGlobalIndex());
         if (trace())
            {
            TR::Node *child = node->getChild(i);
            traceMsg(comp(), "        (Marked %s n%dn unsafe due to dead child #%d %s n%dn)\n",
               node->getOpCode().getName(), node->getGlobalIndex(), i,
               child->getOpCode().getName(), child->getGlobalIndex());
            }
         }
      }

   // If node is safe, all its descendant nodes must be safe.  Nothing to do.
   //
   // If node is unsafe, anchor all its safe children.  Its unsafe children,
   // at this point, must already have taken care of their own safe
   // subexpressions recursively.
   //
   // Note that this could anchor some nodes multiple times, for complex
   // subexpressions with commoning.  We could make some feeble attempts to
   // prevent that, but let's just leave it to deadTreesElimination.
   //
   if (isUnsafe(node))
      {
      for (int32_t i = 0; i < node->getNumChildren(); ++i)
         {
         TR::Node *child = node->getChild(i);
         bool didIt = anchorIfSafe(child, anchorPoint);
         if (didIt && trace())
            {
            traceMsg(comp(), "  - Anchored child #%d %s n%d of %s n%d\n",
               i,
               child->getOpCode().getName(), child->getGlobalIndex(),
               node->getOpCode().getName(), node->getGlobalIndex());
            }
         }
      }
   }

bool
OMR::UnsafeSubexpressionRemover::anchorIfSafe(TR::Node *node, TR::TreeTop *anchorPoint)
   {
   if (!isVisited(node))
      anchorSafeChildrenOfUnsafeNodes(node, anchorPoint);

   if (isUnsafe(node))
      {
      return false;
      }
   else
      {
      anchorPoint->insertBefore(TR::TreeTop::create(comp(), TR::Node::create(TR::treetop, 1, node)));
      return true;
      }
   }

void
OMR::UnsafeSubexpressionRemover::eliminateStore(TR::TreeTop *treeTop)
   {
   // Time to delete the store.  This is more complex than it sounds.
   //
   // Generally, we want to replace the store with a treetop (ie. a
   // no-op).  However, this is complicated by several factors, in
   // order of increasing subtlety and complexity:
   //
   // 1. Stores can have multiple children -- as many as three for
   //    an iwrtbar.  If the store is turning into a nop, we must
   //    anchor all the other children.
   // 2. Stores can appear under other nodes like CHKs and
   //    compressedRefs.  Particularly for the CHKs, we must
   //    perform the appropriate checks despite eliminating the
   //    store, and so we must leave the store node in the
   //    appropriate shape so its parents will do the right thing.
   // 3. Some expressions are unsafe to evaluate after stores are
   //    eliminated, and those expressions themselves must NOT be
   //    anchored.
   //
   // In these transformations, we adopt the convention that we
   // retain the original store whenever possible, modifying it
   // in-place, and we retain its first child if possible.  These
   // conventions are sometimes required, and so we just adopt them
   // universally for the sake of consistency, efficiency, and
   // readability in the jit logs.
   TR::Node *node = treeTop->getNode();

   // Anchor all children but the first
   //
   for (int32_t i = 1; i < node->getNumChildren(); i++)
      {
      TR::Node *child = node->getChild(i);
      anchorIfSafe(child, treeTop);
      child->recursivelyDecReferenceCount();
      }

   node->setNumChildren(1);
   //
   // Note that the node at this point could be half-baked: an indirect
   // store or write barrier with one child.  This is an intermediate
   // state, and further surgery is required for correctness.

   // Eliminate the store
   //
   if (treeTop->getNode()->getOpCode().isSpineCheck() && treeTop->getNode()->getFirstChild() == node)
      {
      // SpineCHKs are special. We don't want to require all the
      // opts and codegens to do the right thing with a PassThrough
      // child.  Hence, we'll turn the tree into a const, which is
      // a state the opts and codegens must cope with anyway,
      // because if this tree had been a load instead of a store,
      // the optimizer could have legitimately turned it into a const.
      //
      TR::Node *child = node->getChild(0);
      anchorIfSafe(child, treeTop);
      child->recursivelyDecReferenceCount();
      TR::Node::recreate(node, comp()->il.opCodeForConst(node->getSymbolReference()->getSymbol()->getDataType()));
      node->setFlags(0);
      node->setNumChildren(0);
      }
   else
      {
      // Vandalize the first child if it's unsafe to evaluate
      //
      TR::Node *child = node->getFirstChild();
      anchorSafeChildrenOfUnsafeNodes(child, treeTop);
      if (isUnsafe(child))
         {
         // Must eradicate the dead expression.  Can't leave it
         // anchored because it's not safe to evaluate.
         //
         child->recursivelyDecReferenceCount();
         TR::Node *dummyChild = node->setAndIncChild(0, TR::Node::createConstDead(child, TR::Int32, 0xbad1 /* eyecatcher */));
         if (trace())
            traceMsg(comp(), "  - replace unsafe child %s n%dn with dummy %s n%dn\n",
               child->getOpCode().getName(), child->getGlobalIndex(),
               dummyChild->getOpCode().getName(), dummyChild->getGlobalIndex());
         }

      // Set opcode to nop
      //
      if (node->getReferenceCount() >= 1)
         TR::Node::recreate(node, TR::PassThrough);
      else
         TR::Node::recreate(node, TR::treetop);
      }
   }
