/*******************************************************************************
 * Copyright (c) 2000, 2021 IBM Corp. and others
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

#ifndef HEDGETREE_INCL
#define HEDGETREE_INCL

#include <stdint.h>
#include <string.h>
#include "env/FrontEnd.hpp"     // TR_FrontEnd (ptr only)
#include "compile/Compilation.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

// Implementation of Symmetric Binary B-Trees
//    (also called "hedge trees")
//
// Hedge trees have the properties:
//    - every node has at most 2 pointers, each either to a sibling or a child
//    - there are never two consecutive sibling pointers on any search path
//    - all leaf nodes (ones with zero or one child) are at the same height
//
// Search paths are at most 2 * height.
// Tree balancing is needed less frequently than with AVL trees.

template <class T> class TR_HedgeNode
   {
   public:
   TR_ALLOC(TR_Memory::HedgeTree)

   TR_HedgeNode(int32_t key) {initialize(key);}

   void initialize(int32_t key)
      {
      _key = key; _left = NULL; _right = NULL; _parent = NULL;
      _leftSibling = false; _rightSibling = false;
      }

   int32_t getKey()              {return _key;}

   T   *left()               {return _left;}
   T   *right()              {return _right;}
   T   *&leftReference()     {return _left;}
   T   *&rightReference()    {return _right;}

   void setLeft(T *node)     {_left = node; if (node) node->setParent((T*)this);}
   void setRight(T *node)    {_right = node; if (node) node->setParent((T*)this);}

   bool isLeftSibling()         {return _leftSibling;}
   bool isRightSibling()        {return _rightSibling;}
   void setRightSibling(bool b) {_rightSibling = b;}
   void setLeftSibling(bool b)  {_leftSibling = b;}
   T   *getParent()             {return _parent;}
   void setParent(T *node)      {_parent = node;}

   bool verify(T *parent)
      {
      if (isLeftSibling() && (_left->isLeftSibling() || _left->isRightSibling()))
         {
         TR_ASSERT(0, "Double sibling chain for key %d", getKey());
         return false;
         }
      if (isRightSibling() && (_right->isLeftSibling() || _right->isRightSibling()))
         {
         TR_ASSERT(0, "Double sibling chain for key %d", getKey());
         return false;
         }
      if (_parent != parent)
         {
         TR_ASSERT(0, "Bad parent for key %d", getKey());
         return false;
         }
      if (!_left && isLeftSibling())
         {
         TR_ASSERT(0, "Null left sibling for key %d", getKey());
         return false;
         }
      if (!_right && isRightSibling())
         {
         TR_ASSERT(0, "Null right sibling for key %d", getKey());
         return false;
         }
      if (!_left)
         {
         if (_right)
            {
            if (!isRightSibling())
               {
               TR_ASSERT(0, "Non-leaf is missing child for key %d", getKey());
               return false;
               }
            return _right->verify((T*)this);
            }
         return true;
         }
      if (!_right)
         {
         if (!isLeftSibling())
            {
            TR_ASSERT(0, "Non-leaf is missing child for key %d", getKey());
            return false;
            }
         return _left->verify((T*)this);
         }
      int32_t lh = _left->height();
      int32_t rh = _right->height();
      if (!isLeftSibling()) lh++;
      if (!isRightSibling()) rh++;
      if (lh != rh)
         TR_ASSERT(0, "Lhs and rhs different heights for key %d", getKey());
      return lh == rh;
      }

   int32_t height()
      {
      int32_t h;
      if (_left)
         {
         h = _left->height();
         if (!isLeftSibling()) h++;
         }
      else if (_right)
         {
         h = _right->height();
         if (!isRightSibling()) h++;
         }
      else
         h = 1;
      return h;
      }

#if DEBUG
   void printTree(TR::FILE *outFile, int32_t indent, char *prefix, bool isLeftChild, bool isSibling)
      {
      int32_t prefixPos = isSibling ? indent : indent-3;
      if (prefixPos > 0)
         {
         if (isLeftChild)
            prefix[prefixPos] = '|';
         else
            prefix[prefixPos] = ' ';
         }
      if (_right)
         {
         if (isRightSibling())
            _right->printTree(outFile, indent, prefix, false, true);
         else
            _right->printTree(outFile, indent+5, prefix, false, false);
         }
      else
         trfprintf(outFile, "%.*s\n", prefixPos+1, prefix);
      if (isSibling)
         trfprintf(outFile, "%.*s%d\n", prefixPos, prefix, getKey());
      else
         trfprintf(outFile, "%.*s-->%d\n", prefixPos, prefix, getKey());
      if (prefixPos > 0)
         {
         if (!isLeftChild)
            prefix[prefixPos] = '|';
         else
            prefix[prefixPos] = ' ';
         }
      if (_left)
         {
         if (isLeftSibling())
            _left->printTree(outFile, indent, prefix, true, true);
         else
            _left->printTree(outFile, indent+5, prefix, true, false);
         }
      else
         trfprintf(outFile, "%.*s\n", prefixPos+1, prefix);
      prefix[prefixPos] = ' ';
      }
#else
   void printTree(TR::FILE *outFile, int32_t indent, char *prefix, bool isLeftChild, bool isSibling) {}
#endif

   private:

   T       *_left;
   T       *_right;
   T       *_parent;
   int32_t  _key;
   bool     _leftSibling;
   bool     _rightSibling;
   };

template <class T> class TR_HedgeTree
   {
   public:
   TR_ALLOC(TR_Memory::HedgeTree)

   TR_HedgeTree() : _root(NULL) {}

   T *getRoot() {return _root;}
   void setRoot(T *node) {_root = node; if (node) node->setParent(NULL);}
   T *&rootReference() {return _root;}

   bool isEmpty() {return _root == NULL;}

   bool verify()
      {
      return _root ? _root->verify(NULL) : true;
      }

   T *find(int32_t key)
   {
   // Search the tree for the node with the given key.
   //
   for (T *node = _root; node; )
      {
      if (key < node->getKey())
         node = node->left();
      else if (key > node->getKey())
         node =  node->right();
      else
         return node;
      }
   return NULL;
   }

#if DEBUG
   void print(TR::FILE *outFile)
      {
      if (outFile == NULL)
         return;
      if (!_root)
         {
         trfprintf(outFile, "     empty\n");
         return;
         }
      char prefix[80];
      memset(prefix, ' ', 80);
      _root->printTree(outFile, 3, prefix, false, false);
      trfflush(outFile);
      }
#else
   void print(TR::FILE *outFile) {}
#endif

   private:
   T *_root;
   };

template <class T> class TR_HedgeTreeIterator
   {
   public:
   TR_ALLOC(TR_Memory::HedgeTree)

   TR_HedgeTreeIterator()
      : _tree(NULL), _next(NULL) {}
   TR_HedgeTreeIterator(TR_HedgeTree<T> &tree) {reset(tree);}

   TR_HedgeTree<T>   *getBase()        {return _tree;}
   void reset(TR_HedgeTree<T> &tree) {_tree = &tree; _next = getLeftmost(tree.getRoot());}

   T *getFirst()
      {
      T *node = getLeftmost(_tree->getRoot());
      _next = getNextHigher(node);
      return node;
      }
   T *getNext()
      {
      if (!_next)
         return NULL;
      T *node = _next;
      _next = getNextHigher(node);
      return node;
      }

   private:
   T *getLeftmost(T *root)
      {
      if (root)
         while (root->left())
            root = root->left();
      return root;
      }
   T *getNextHigher(T *node)
      {
      if (!node)
         return NULL;
      if (node->right())
         return getLeftmost(node->right());
      int32_t key = node->getKey();
      for (node = node->getParent(); node && node->getKey() <= key; node = node->getParent())
         {}
      return node;
      }
   TR_HedgeTree<T> *_tree;
   T               *_next;
   };

template <class T> class TR_HedgeTreeHandler
   {
   public:
   TR_ALLOC(TR_Memory::HedgeTree)

   T *find(int32_t key, TR_HedgeTree<T> &tree)
      {
      return tree.find(key);
      }
   T *findOrCreate(int32_t key, TR_HedgeTree<T> &tree)
      {
      int32_t rebalance;
      T *result;
      if (debug("traceHedge"))
         diagnostic("\nCall findOrCreate with key %d\n", key);
      if (tree.getRoot())
         {
         _nodeToBeInserted = NULL;
         _treeChanged = false;
         result = findOrCreate(key, tree.rootReference(), rebalance);
         }
      else
         {
         result = allocate(key);
         tree.setRoot(result);
         _treeChanged = true;
         }
      if (_treeChanged && debug("traceHedge"))
         {
         diagnostic("Tree after insertion of key %d:\n", key);
         tree.print(comp()->getOutFile());
         tree.verify();
         }
      return result;
      }
   T *insert(T *newNode, TR_HedgeTree<T> &tree)
      {
      int32_t rebalance;
      T *result;
      newNode->setLeft(NULL);
      newNode->setRight(NULL);
      newNode->setLeftSibling(false);
      newNode->setRightSibling(false);
      if (debug("traceHedge"))
         diagnostic("\nCall insert with key %d\n", newNode->getKey());
      if (tree.getRoot())
         {
         _nodeToBeInserted = newNode;
         _treeChanged = false;
         result = findOrCreate(newNode->getKey(), tree.rootReference(), rebalance);
         }
      else
         {
         result = newNode;
         tree.setRoot(newNode);
         _treeChanged = true;
         }
      if (_treeChanged && debug("traceHedge"))
         {
         diagnostic("Tree after insertion of key %d:\n", newNode->getKey());
         tree.print(comp()->getOutFile());
         tree.verify();
         }
      return result;
      }
   T *remove(int32_t key, TR_HedgeTree<T> &tree)
      {
      int32_t rebalance;
      _treeChanged = false;
      if (debug("traceHedge"))
         diagnostic("\nCall remove with key %d\n", key);
      T *result = remove(key, tree.rootReference(), rebalance);
      if (_treeChanged && debug("traceHedge"))
         {
         diagnostic("Tree after removal of key %d:\n", key);
         tree.print(comp()->getOutFile());
         tree.verify();
         }
      return result;
      }
   T *copyAll(TR_HedgeTree<T> &tree)
      {
      return copySubtree(tree.getRoot());
      }
   T *copySubtree(T *node)
      {
      if (!node)
         return NULL;
      T *newNode = copy(node);
      newNode->setLeft(copySubtree(node->left()));
      newNode->setRight(copySubtree(node->right()));
      newNode->setLeftSibling(node->isLeftSibling());
      newNode->setRightSibling(node->isRightSibling());
      return newNode;
      }
   void empty(TR_HedgeTree<T> &tree)
      {
      emptySubtree(tree.rootReference());
      }
   T *getRoot(TR_HedgeTree<T> &tree) {return tree.getRoot();}
   void setRoot(TR_HedgeTree<T> &tree, T *node) {tree.setRoot(node);}

   virtual TR::Compilation *comp() = 0;

   protected:

   virtual T   *allocate(int32_t key) = 0;
   virtual void free(T *node) = 0;
   virtual T   *copy(T *node) = 0;

   private:

   inline T *findOrCreate(int32_t key, T *&node, int32_t &rebalance);
   inline T *remove(int32_t key, T *&node, int32_t &rebalance);
   inline void swapRightmost(T *&node, T *&swap);
   void emptySubtree(T *&node)
      {
      if (node)
         {
         emptySubtree(node->leftReference());
         emptySubtree(node->rightReference());
         free(node);
         node = NULL;
         }
      }

   T *_nodeToBeInserted;
   bool _treeChanged;
   };

template <class T> T *TR_HedgeTreeHandler<T>::findOrCreate(int32_t key, T *&node, int32_t &rebalance)
   {
   // Search the tree for the node with the given key. Insertion may cause the
   // slot containing the node pointer to be updated.
   // "rebalance" is a returned value with the following meaning:
   //    rebalance = 0: no change in structure of this subtree
   //    rebalance = 1: this node has acquired a sibling
   //    rebalance = 2: this subtree has increased in height
   //
   // If the search failed, create a new node and insert it
   //
   T *result;
   T *p1, *p2;

   if (key < node->getKey())
      {
      // Search the left subtree. Rebalance if necessary
      //
      if (!node->left())
         {
         if (_nodeToBeInserted)
            result = _nodeToBeInserted;
         else
            result= allocate(key);
         node->setLeft(result);
         _treeChanged = true;
         rebalance = 2;
         }
      else
         result = findOrCreate(key,node->leftReference(), rebalance);
      if (rebalance)
         {
         if (node->isLeftSibling())
            {
            // If the subtree acquired a sibling this level now has 3 nodes in
            // the search path, so we must split the level.
            // Thus the height of this subtree will always have increased.
            //
            rebalance = 2;
            p1 = node->left();
            node->setLeftSibling(false);
            if (p1->isLeftSibling())
               {
               // Left subtree is lifted to a new level above this node
               //
               node->setLeft(p1->right());
               p1->setLeftSibling(false);
               p1->setParent(node->getParent());
               p1->setRight(node);
               node = p1;
               }
            else if (p1->isRightSibling())
               {
               // Left subtree's right subtree is lifted to a new level above
               // this node
               //
               p2 = p1->right();
               p1->setRight(p2->left());
               p1->setRightSibling(false);
               node->setLeft(p2->right());
               node->setLeftSibling(false);
               p2->setLeft(p1);
               p2->setParent(node->getParent());
               p2->setRight(node);
               node = p2;
               }
            }
         else
            {
            // Left subtree is a child. If its height has increased, raise it
            // to become a sibling. Otherwise rebalancing is complete.
            //
            if (--rebalance)
               node->setLeftSibling(true);
            }
         }
      }

   else if (key > node->getKey())
      {
      // Search the right subtree. Rebalance if necessary
      //
      if (!node->right())
         {
         if (_nodeToBeInserted)
            result = _nodeToBeInserted;
         else
            result= allocate(key);
         node->setRight(result);
         _treeChanged = true;
         rebalance = 2;
         }
      else
         result = findOrCreate(key, node->rightReference(), rebalance);
      if (rebalance)
         {
         if (node->isRightSibling())
            {
            // If the subtree acquired a sibling this level now has 3 nodes in
            // the search path, so we must split the level.
            // Thus the height of this subtree will always have increased.
            //
            rebalance = 2;
            p1 = node->right();
            node->setRightSibling(false);
            if (p1->isRightSibling())
               {
               // Right subtree is lifted to a new level above this node
               //
               node->setRight(p1->left());
               p1->setRightSibling(false);
               p1->setParent(node->getParent());
               p1->setLeft(node);
               node = p1;
               }
            else if (p1->isLeftSibling())
               {
               // Right subtree's left subtree is lifted to a new level above
               // this node
               //
               p2 = p1->left();
               p1->setLeft(p2->right());
               p1->setLeftSibling(false);
               node->setRight(p2->left());
               node->setRightSibling(false);
               p2->setRight(p1);
               p2->setParent(node->getParent());
               p2->setLeft(node);
               node = p2;
               }
            }
         else
            {
            // Right subtree is a child. If its height has increased, raise it
            // to become a sibling. Otherwise rebalancing is complete.
            //
            if (--rebalance)
               node->setRightSibling(true);
            }
         }
      }

   else
      {
      rebalance = 0;
      result = node;
      }

   return result;
   }

template <class T> T *TR_HedgeTreeHandler<T>::remove(int32_t key, T *&node, int32_t &rebalance)
   {
   // Find the node with the given key and delete it from the tree
   // Deletion may cause the slot containing the node pointer to be updated.
   // "rebalance" is a returned value with the following meaning:
   //    rebalance = 0: no change in structure of this subtree
   //    rebalance = 1: this subtree has decreased in height
   //
   if (node == NULL)
      {
      // Node not found
      //
      rebalance = 0;
      return NULL;
      }

   T *result;
   T *p1, *p2, *p3;
   bool takeLeftBranch = false;

   if (key == node->getKey())
      {
      // This is the node to be deleted. If it is a leaf node just remove it
      // from the tree. Otherwise swap it with the rightmost leaf node of the
      // left subtree and continue down the left subtree. This way we end up
      // always deleting a leaf node.
      //
      result = node;
      _treeChanged = true;
      if (node->right() == NULL)
         {
         rebalance = node->isLeftSibling() ? 0 : 1;
         if (node->left())
            node->left()->setParent(node->getParent());
         node = node->left();
         return result;
         }
      if (node->left() == NULL)
         {
         rebalance = node->isRightSibling() ? 0 : 1;
         node->right()->setParent(node->getParent());
         node = node->right();
         return result;
         }
      swapRightmost(node->leftReference(), node);
      takeLeftBranch = true;
      }

   if (takeLeftBranch || key < node->getKey())
      {
      // Look in the left subtree
      //
      result = remove(key, node->leftReference(), rebalance);
      if (rebalance)
         {
         // The height of the subtree has changed. We must rebalance this node.
         // In the diagrams that follow, B is this node and A is the subtree
         // that has become shorter.
         //
         if (node->isLeftSibling())
            {
            // Left subtree is a sibling. Make it a child to bring the height
            // back up to its original value.
            //
            //         |                 |
            //         |                 |
            //      A--B       ==>       B
            //          \               / \
            //           C             A   C
            //
            node->setLeftSibling(false);
            rebalance = 0;
            }
         else
            {
            p1 = node->right();
            p2 = p1->left();
            if ((node->isRightSibling() && p2->isRightSibling()) ||
                (!node->isRightSibling() && p1->isLeftSibling()))
               {
               // Right subtree's left subtree is lifted to a new level above
               // this node. The original height is then restored.
               //
               //         |                |
               //         |                |
               //         B----C    ==>    D----C
               //        /    / \         /    / \
               //       A    D-G E       B    G   E
               //           /           / \
               //          F           A   F
               //
               //
               //         |                |
               //         |                |
               //         B----C    ==>    D----C
               //        /    / \         /    / \
               //       A  F-D-G E       B-F  G   E
               //                       /
               //                      A
               //
               //
               //         |                |
               //         |                |
               //         B         ==>    D
               //        / \              / \
               //       A D-C-E          B   C--E
               //        / \            /\  /
               //       F   G          A  F G
               //
               //
               //         |                |
               //         |                |
               //         B         ==>    D
               //        / \              / \
               //       A D-C            B   C
               //        / \ \          / \  /\
               //       F   G E        A   F G E
               //
               //
               p1->setLeft(p2->right());
               p1->setLeftSibling(false);
               p2->setRight(p1);
               node->setRight(p2->left());
               node->setRightSibling(p2->isLeftSibling());
               p2->setParent(node->getParent());
               p2->setLeft(node);
               p2->setLeftSibling(false);
               node = p2;
               rebalance = 0;
               }
            else if (node->isRightSibling() && p2->isLeftSibling())
               {
               // Right subtree's left subtree's left subtree is lifted to a
               // new level above this node. The original height is then restored.
               //
               //         |                |
               //         |                |
               //         B-----C   ==>    F-----C
               //        /     / \        /     / \
               //       A  F--D   E      B     D   E
               //         / \  \        / \   / \
               //        H   I  G      A   H I   G
               //
               p3 = p2->left();
               p2->setLeft(p3->right());
               p2->setLeftSibling(false);
               node->setRight(p3->left());
               node->setRightSibling(false);
               p3->setRight(p1);
               p3->setRightSibling(true);
               p3->setParent(node->getParent());
               p3->setLeft(node);
               node = p3;
               rebalance = 0;
               }
            else if (node->isRightSibling())
               {

               // Right subtree is lifted to a new level above this node. The
               // original height is then restored.
               //
               //         |                   |
               //         |                   |
               //         B-----C   ==>       C
               //        /     / \           / \
               //       A     D   E      B--D   E
               //            / \        / \  \
               //           F   G      A   F  G
               //
               node->setRight(p2->left());
               node->setRightSibling(false);
               p1->setParent(node->getParent());
               p2->setLeft(node);
               p2->setLeftSibling(true);
               node = p1;
               rebalance = 0;
               }
            else if (p1->isRightSibling())
               {

               // Right subtree is lifted to a new level above this node. The
               // original height is not restored.
               //
               //         |                   |
               //         |                   |
               //         B         ==>    B--C--E
               //        / \              / \
               //       A   C--E         A   D
               //          /
               //         D
               //
               node->setRight(p1->left());
               p1->setParent(node->getParent());
               p1->setLeft(node);
               p1->setLeftSibling(true);
               node = p1;
               }
            else
               {
               // Raise the right subtree to be a sibling. The resulting tree
               // is still shorter so the parent will have to rebalance.
               // If this is a sibling of the parent, the parent will make it a
               // child to restore the height, so no double-sibling chain will
               // result.
               //
               //         |                |
               //         |                |
               //         B         ==>    B--C
               //        / \              /  / \
               //       A   C            A  D   E
               //          / \
               //         D   E
               //
               node->setRightSibling(true);
               }
            }
         }
      }

   else
      {
      // Look in the right subtree
      //
      result = remove(key, node->rightReference(), rebalance);
      if (rebalance)
         {
         // The height of the subtree has changed. We must rebalance this node.
         // In the diagrams that follow, B is this node and A is the subtree
         // that has become shorter.
         //
         if (node->isRightSibling())
            {
            // Right subtree is a sibling. Make it a child to bring the height
            // back up to its original value.
            //
            //         |                 |
            //         |                 |
            //         B--A    ==>       B
            //        /                 / \
            //       C                 C   A
            //
            node->setRightSibling(false);
            rebalance = 0;
            }
         else
            {
            p1 = node->left();
            p2 = p1->right();
            if ((node->isLeftSibling() && p2->isLeftSibling()) ||
                (!node->isLeftSibling() && p1->isRightSibling()))
               {
               // Left subtree's right subtree is lifted to a new level above
               // this node. The original height is then restored.
               //
               //              |                |
               //              |                |
               //         C----B    ==>    C----D
               //        / \    \         / \    \
               //       E G-D    A       E   G    B
               //            \                   / \
               //             F                 F   A
               //
               //
               //              |                |
               //              |                |
               //         C----B    ==>    C----D
               //        / \    \         / \    \
               //       E G-D-F  A       E   G  F-B
               //                                  \
               //                                   A
               //
               //
               //         |                |
               //         |                |
               //         B         ==>    D
               //        / \              / \
               //     E-C-D A         E--C   B
               //        / \              \ / \
               //       G   F             G F  A
               //
               //
               //         |                |
               //         |                |
               //         B         ==>    D
               //        / \              / \
               //       C-D A            C   B
               //      / / \            /\  / \
               //     E G   F          E  G F  A
               //
               //
               p1->setRight(p2->left());
               p1->setRightSibling(false);
               p2->setLeft(p1);
               node->setLeft(p2->right());
               node->setLeftSibling(p2->isRightSibling());
               p2->setParent(node->getParent());
               p2->setRight(node);
               p2->setRightSibling(false);
               node = p2;
               rebalance = 0;
               }
            else if (node->isLeftSibling() && p2->isRightSibling())
               {
               // Left subtree's right subtree's right subtree is lifted to a
               // new level above this node. The original height is then restored.
               //
               //               |                |
               //               |                |
               //         C-----B   ==>    C-----F
               //        / \     \        / \     \
               //       E   D--F  A      E   D     B
               //          /  / \           / \   / \
               //         G  I   H         G   I H   A
               //
               p3 = p2->right();
               p2->setRight(p3->left());
               p2->setRightSibling(false);
               node->setLeft(p3->right());
               node->setLeftSibling(false);
               p3->setLeft(p1);
               p3->setLeftSibling(true);
               p3->setParent(node->getParent());
               p3->setRight(node);
               node = p3;
               rebalance = 0;
               }
            else if (node->isLeftSibling())
               {

               // Left subtree is lifted to a new level above this node. The
               // original height is then restored.
               //
               //               |             |
               //               |             |
               //         C-----B   ==>       C
               //        / \     \           / \
               //       E   D     A         E   D--B
               //          / \                 /  / \
               //         G   F               G  F   A
               //
               node->setLeft(p2->right());
               node->setLeftSibling(false);
               p1->setParent(node->getParent());
               p2->setRight(node);
               p2->setRightSibling(true);
               node = p1;
               rebalance = 0;
               }
            else if (p1->isLeftSibling())
               {

               // Left subtree is lifted to a new level above this node. The
               // original height is not restored.
               //
               //         |                   |
               //         |                   |
               //         B         ==>    E--C--B
               //        / \                    / \
               //    E--C   A                  D   A
               //        \
               //         D
               //
               node->setLeft(p1->right());
               p1->setParent(node->getParent());
               p1->setRight(node);
               p1->setRightSibling(true);
               node = p1;
               }
            else
               {
               // Raise the left subtree to be a sibling. The resulting tree
               // is still shorter so the parent will have to rebalance.
               // If this is a sibling of the parent, the parent will make it a
               // child to restore the height, so no double-sibling chain will
               // result.
               //
               //         |                   |
               //         |                   |
               //         B         ==>    C--B
               //        / \              / \  \
               //       C   A            E   D  A
               //      / \
               //     E   D
               //
               node->setLeftSibling(true);
               }
            }
         }
      }

   return result;
   }

template <class T> void TR_HedgeTreeHandler<T>::swapRightmost(T *&node, T *&swap)
   {
   // Find the rightmost node in this subtree and swap it with the given node
   //
   if (node->right())
      {
      swapRightmost(node->rightReference(), swap);
      }
   else
      {
      // This is it
      //
      bool b;
      b = node->isLeftSibling(); node->setLeftSibling(swap->isLeftSibling()); swap->setLeftSibling(b);
      b = node->isRightSibling(); node->setRightSibling(swap->isRightSibling()); swap->setRightSibling(b);

      T *t = swap->right(); swap->setRight(node->right()); node->setRight(t);
      T *t1 = swap->getParent();
      if (node == swap->left())
         {
         // Special case if this is the immediate child
         //
         t = node;
         swap->setLeft(t->left());
         t->setLeft(swap);
         t->setParent(t1);
         swap = t;
         }
      else
         {
         t = swap->left(); swap->setLeft(node->left()); node->setLeft(t);
         t = node;
         swap->setParent(node->getParent());
         node = swap;
         t->setParent(t1);
         swap = t;
         }
      }
   }

#endif
