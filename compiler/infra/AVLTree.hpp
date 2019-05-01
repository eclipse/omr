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

#ifndef AVLTREE_INCL
#define AVLTREE_INCL

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "compile/Compilation.hpp"
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"

// Implementation of AVL trees

template <class T> class TR_AVLNode
   {
   public:
   TR_ALLOC(TR_Memory::AVLTree)

   TR_AVLNode(int32_t key) {initialize(key);}

   void initialize(int32_t key)
      {
      _key = key; _left = NULL; _right = NULL; _parent = NULL;
      _balance = 0;
      }

   int32_t getKey()              {return _key;}

   T   *left()               {return _left;}
   T   *right()              {return _right;}
   T   *&leftReference()     {return _left;}
   T   *&rightReference()    {return _right;}

   void setLeft(T *node)     {_left = node; if (node) node->setParent((T*)this);}
   void setRight(T *node)    {_right = node; if (node) node->setParent((T*)this);}

   bool balance()              {return _balance;}
   void setBalance(int32_t b)  {_balance = b;}
   T   *getParent()             {return _parent;}
   void setParent(T *node)      {_parent = node;}

   bool verify(T *parent)
      {
      if (_parent != parent)
         {
         TR_ASSERT(0, "Bad parent for key %d", getKey());
         return false;
         }
      if (!_left)
         {
         if (_right)
            {
            if (_right->height() != 1 || _balance != 1)
               {
               TR_ASSERT(0, "Out of balance for key %d", getKey());
               return false;
               }
            return _right->verify((T*)this);
            }
         return true;
         }
      if (!_right)
         {
         if (_left->height() != 1 || _balance != -1)
            {
            TR_ASSERT(0, "Out of balance for key %d", getKey());
            return false;
            }
         return _left->verify((T*)this);
         }
      int32_t balance = _right->height() - _left->height();
      if (balance != _balance)
         {
         TR_ASSERT(0, "Out of balance for key %d", getKey());
         return false;
         }
      if (!_left->verify((T*)this))
         return false;
      if (!_right->verify((T*)this))
         return false;
      return true;
      }

   int32_t height()
      {
      int32_t lh = _left ? _left->height()+1 : 1;
      int32_t rh = _right ? _right->height()+1 : 1;
      return (lh >= rh) ? lh : rh;
      }

#if DEBUG
   void printTree(TR::FILE *outFile, int32_t indent, char *prefix, bool isLeftChild)
      {
      int32_t prefixPos = indent-3;
      if (prefixPos > 0)
         {
         if (isLeftChild)
            prefix[prefixPos] = '|';
         else
            prefix[prefixPos] = ' ';
         }
      if (_right)
         {
         _right->printTree(outFile, indent+3, prefix, false);
         }
      else
         fprintf(outFile, "%.*s\n", prefixPos+1, prefix);
      fprintf(outFile, "%.*s-->%d(%+d)\n", prefixPos, prefix, getKey(), _balance);
      if (prefixPos > 0)
         {
         if (!isLeftChild)
            prefix[prefixPos] = '|';
         else
            prefix[prefixPos] = ' ';
         }
      if (_left)
         {
            _left->printTree(outFile, indent+3, prefix, true);
         }
      else
         fprintf(outFile, "%.*s\n", prefixPos+1, prefix);
      prefix[prefixPos] = ' ';
      }
#else
   void printTree(TR::FILE *outFile, int32_t indent, char *prefix, bool isLeftChild) {}
#endif

   private:

   T       *_left;
   T       *_right;
   T       *_parent;
   int32_t  _key;
   int32_t  _balance; // RH height - LH height
   };

template <class T> class TR_AVLTree
   {
   public:
   TR_ALLOC(TR_Memory::AVLTree)

   TR_AVLTree() : _root(NULL) {}

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
         fprintf(outFile, "     empty\n");
         return;
         }
      char prefix[80];
      memset(prefix, ' ', 80);
      _root->printTree(outFile, 3, prefix, false);
      fflush(outFile);
      }
#else
   void print(TR::FILE *outFile) {}
#endif

   private:
   T *_root;
   };

template <class T> class TR_AVLTreeIterator
   {
   public:
   TR_ALLOC(TR_Memory::AVLTree)

   TR_AVLTreeIterator()
      : _tree(NULL), _next(NULL) {}
   TR_AVLTreeIterator(TR_AVLTree<T> &tree) {reset(tree);}

   TR_AVLTree<T>   *getBase()        {return _tree;}
   void reset(TR_AVLTree<T> &tree) {_tree = &tree; _next = getLeftmost(tree.getRoot());}

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
   TR_AVLTree<T> *_tree;
   T               *_next;
   };

template <class T> class TR_AVLTreeHandler
   {
   public:
   TR_ALLOC(TR_Memory::AVLTree)

   T *find(int32_t key, TR_AVLTree<T> &tree)
      {
      return tree.find(key);
      }
   T *findOrCreate(int32_t key, TR_AVLTree<T> &tree)
      {
      int32_t heightChanged;
      T *result;
      if (debug("traceAVL"))
         diagnostic("\nCall findOrCreate with key %d\n", key);
      if (tree.getRoot())
         {
         _nodeToBeInserted = NULL;
         _treeChanged = false;
         result = findOrCreate(key, tree.rootReference(), heightChanged);
         }
      else
         {
         result = allocate(key);
         tree.setRoot(result);
         _treeChanged = true;
         }
      if (_treeChanged && debug("traceAVL"))
         {
         diagnostic("Tree after insertion of key %d:\n", key);
         tree.print(comp()->getOutFile());
         tree.verify();
         }
      return result;
      }
   T *insert(T *newNode, TR_AVLTree<T> &tree)
      {
      int32_t heightChanged;
      T *result;
      newNode->setLeft(NULL);
      newNode->setRight(NULL);
      newNode->setBalance(0);
      if (debug("traceAVL"))
         diagnostic("\nCall insert with key %d\n", newNode->getKey());
      if (tree.getRoot())
         {
         _nodeToBeInserted = newNode;
         _treeChanged = false;
         result = findOrCreate(newNode->getKey(), tree.rootReference(), heightChanged);
         }
      else
         {
         result = newNode;
         tree.setRoot(newNode);
         _treeChanged = true;
         }
      if (_treeChanged && debug("traceAVL"))
         {
         diagnostic("Tree after insertion of key %d:\n", newNode->getKey());
         tree.print(comp()->getOutFile());
         tree.verify();
         }
      return result;
      }
   T *remove(int32_t key, TR_AVLTree<T> &tree)
      {
      int32_t heightChanged;
      _treeChanged = false;
      if (debug("traceAVL"))
         diagnostic("\nCall remove with key %d\n", key);
      T *result = remove(key, tree.rootReference(), heightChanged);
      if (_treeChanged && debug("traceAVL"))
         {
         diagnostic("Tree after removal of key %d:\n", key);
         tree.print(comp()->getOutFile());
         tree.verify();
         }
      return result;
      }
   T *copyAll(TR_AVLTree<T> &tree)
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
      newNode->setBalance(node->balance());
      return newNode;
      }
   void empty(TR_AVLTree<T> &tree)
      {
      emptySubtree(tree.rootReference());
      }
   T *getRoot(TR_AVLTree<T> &tree) {return tree.getRoot();}
   void setRoot(TR_AVLTree<T> &tree, T *node) {tree.setRoot(node);}

   virtual TR::Compilation *comp() = 0;

   protected:

   virtual T   *allocate(int32_t key) = 0;
   virtual void free(T *node) = 0;
   virtual T   *copy(T *node) = 0;

   private:

   int32_t balanceLeft(T *&node);
   int32_t balanceRight(T *&node);
   T *findOrCreate(int32_t key, T *&node, int32_t &heightChanged);
   T *remove(int32_t key, T *&node, int32_t &heightChanged);
   void swapRightmost(T *&node, T *&swap);
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

template <class T> int32_t TR_AVLTreeHandler<T>::balanceLeft(T *&node)
   {
   // Rebalance the given node which is heavy on the left.
   // Returns non-zero if the height of the tree changes.
   //
   T *p1, *p2;
   p1 = node->left();
   if (p1->balance() <= 0)
      {
      // Left subtree is lifted to a new level above this node
      //
      //         |                         |
      //         |                         |
      //     ----B(h)----         ==>  ----A(h+0,1)---
      //     |          |              |             |
      // --A(h)---  C(h-2)          X(h-1)      --B(h-0,1)--
      // |       |                              |          |
      // X(h-1) Y(h-1,2)                     Y(h-1,2)  C(h-2)
      //
      node->setLeft(p1->right());
      p1->setParent(node->getParent());
      p1->setRight(node);
      node->setBalance(-(p1->balance()+1));
      p1->setBalance(-node->balance());
      node = p1;
      return p1->balance();
      }

   // Left subtree's right subtree is lifted to a new level above
   // this node
   //
   //         |                             |
   //         |                             |
   //     ----B(h)----         ==>    ------Y(h)--------
   //     |          |                |                |
   // ----A(h)----  C(h-2)       --A(h-1)--        --B(h-1)--
   // |          |               |        |        |        |
   // X(h-2)   --Y(h-1)--        X(h-2)   P        Q     C(h-2)
   //          |        |
   //          P        Q
   //
   p2 = p1->right();
   p1->setRight(p2->left());
   node->setLeft(p2->right());
   p2->setLeft(p1);
   p2->setParent(node->getParent());
   p2->setRight(node);
   if (p2->balance() <= 0) p1->setBalance(0); else p1->setBalance(-1);
   if (p2->balance() >= 0) node->setBalance(0); else node->setBalance(1);
   p2->setBalance(0);
   node = p2;
   return 0;
   }

template <class T> int32_t TR_AVLTreeHandler<T>::balanceRight(T *&node)
   {
   // Rebalance the given node which is heavy on the right.
   // Returns non-zero if the height of the tree changes.
   //
   T *p1, *p2;
   p1 = node->right();
   if (p1->balance() >= 0)
      {
      // Right subtree is lifted to a new level above this node
      //
      node->setRight(p1->left());
      p1->setParent(node->getParent());
      p1->setLeft(node);
      node->setBalance(1-p1->balance());
      p1->setBalance(-node->balance());
      node = p1;
      return p1->balance();
      }

   // Right subtree's left subtree is lifted to a new level above
   // this node
   //
   p2 = p1->left();
   p1->setLeft(p2->right());
   node->setRight(p2->left());
   p2->setRight(p1);
   p2->setParent(node->getParent());
   p2->setLeft(node);
   if (p2->balance() >= 0) p1->setBalance(0); else p1->setBalance(1);
   if (p2->balance() <= 0) node->setBalance(0); else node->setBalance(-1);
   p2->setBalance(0);
   node = p2;
   return 0;
   }

template <class T> T *TR_AVLTreeHandler<T>::findOrCreate(int32_t key, T *&node, int32_t &heightChanged)
   {
   // Search the tree for the node with the given key. Insertion may cause the
   // slot containing the node pointer to be updated.
   // "heightChanged" is a return value, non-zero if the tree height changes
   // as a result of the rebalancing.
   //
   // If the search failed, create a new node and insert it
   //
   T *result;

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
         heightChanged = 1;
         }
      else
         result = findOrCreate(key, node->leftReference(), heightChanged);
      if (heightChanged)
         {
         heightChanged = node->balance()-1;
         node->setBalance(heightChanged);
         if (heightChanged < -1)
            heightChanged = balanceLeft(node);
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
         heightChanged = 1;
         }
      else
         result = findOrCreate(key, node->rightReference(), heightChanged);
      if (heightChanged)
         {
         heightChanged = node->balance()+1;
         node->setBalance(heightChanged);
         if (heightChanged > 1)
            heightChanged = balanceRight(node);
         }
      }

   else
      {
      heightChanged = 0;
      result = node;
      }

   return result;
   }

template <class T> T *TR_AVLTreeHandler<T>::remove(int32_t key, T *&node, int32_t &heightChanged)
   {
   // Find the node with the given key and delete it from the tree
   // Deletion may cause the slot containing the node pointer to be updated.
   // "heightChanged" is a return value, non-zero if the tree height changes
   // as a result of the rebalancing.
   //
   if (node == NULL)
      {
      // Node not found
      //
      heightChanged = 0;
      return NULL;
      }

   T *result;
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
         if (node->left())
            node->left()->setParent(node->getParent());
         heightChanged = 1;
         node = node->left();
         return result;
         }
      if (node->left() == NULL)
         {
         node->right()->setParent(node->getParent());
         heightChanged = 1;
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
      result = remove(key, node->leftReference(), heightChanged);
      if (heightChanged)
         {
         heightChanged = node->balance();
         node->setBalance(heightChanged+1);
         if (heightChanged > 0)
            heightChanged = !balanceRight(node);
         }
      }

   else
      {
      // Look in the right subtree
      //
      result = remove(key, node->rightReference(), heightChanged);
      if (heightChanged)
         {
         heightChanged = node->balance();
         node->setBalance(heightChanged-1);
         if (heightChanged < 0)
            heightChanged = !balanceLeft(node);
         }
      }

   return result;
   }

template <class T> void TR_AVLTreeHandler<T>::swapRightmost(T *&node, T *&swap)
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
      int32_t balance;
      balance = node->balance(); node->setBalance(swap->balance()); swap->setBalance(balance);

      T *t = swap->right(); swap->setRight(node->right()); node->setRight(t);
      T *t1 = swap->getParent();
      if (node == swap->left())
         {
         // Special case if this is the immediate child
         //
         t = node;
         swap->setLeft(t->left());
         t->setLeft(swap);
         }
      else
         {
         t = swap->left(); swap->setLeft(node->left()); node->setLeft(t);
         t = node;
         swap->setParent(node->getParent());
         node = swap;
         }
      t->setParent(t1);
      swap = t;
      }
   }

#endif
