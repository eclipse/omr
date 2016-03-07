/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 *******************************************************************************/

#include "omravl.h"
#include "avl_internal.h"
#include "ut_avl.h"

static J9AVLTreeNode *deleteNode(J9AVLTree *tree, J9AVLTreeNode **walkPtr, J9WSRP *walkSRPPtr, J9AVLTreeNode *node, intptr_t *heightChange);
static J9AVLTreeNode *rotate(J9AVLTree *tree, J9AVLTreeNode *walk, intptr_t direction, intptr_t *heightChange);
static J9AVLTreeNode *doubleRotate(J9AVLTree *tree, J9AVLTreeNode *walk, intptr_t direction, intptr_t *heightChange);
static void rebalance(J9AVLTree *tree, J9AVLTreeNode **walkPtr, J9WSRP *walkSRPPtr, intptr_t direction, intptr_t *heightChange);
static J9AVLTreeNode *findRightMostLeaf(J9AVLTree *tree, J9WSRP *walkSRPPtr, intptr_t *heightChange);
static J9AVLTreeNode *findNode(J9AVLTree *tree, J9AVLTreeNode *walk, uintptr_t search);
static J9AVLTreeNode *insertNode(J9AVLTree *tree, J9AVLTreeNode **walkPtr, J9WSRP *walkSRPPtr, J9AVLTreeNode *node, intptr_t *heightChange);

/**
 * Insert a node into an AVL tree
 *
 * @param[in] tree  The tree
 * @param[in] nodeToInsert  The node to insert into the tree
 *
 * @return  The node inserted or NULL in the case of error
 */
J9AVLTreeNode *
avl_insert(J9AVLTree *tree, J9AVLTreeNode *nodeToInsert)
{
	intptr_t heightChange;

	return insertNode(tree, &tree->rootNode, NULL, nodeToInsert, &heightChange);
}

/**
 * Delete a node from an AVL tree
 *
 * @param[in] tree  The tree
 * @param[in] nodeToDelete  The node to delete from the tree
 *
 * @return  The node deleted or NULL in the case of error
 */
J9AVLTreeNode *
avl_delete(J9AVLTree *tree, J9AVLTreeNode *nodeToDelete)
{
	intptr_t heightChange;

	return deleteNode(tree, &tree->rootNode, NULL, nodeToDelete, &heightChange);
}

/**
 * Search an AVL tree for a node
 *
 * @param[in] tree  The tree
 * @param[in] searchValue  The key to use in finding the node
 *
 * @return  The found node or NULL
 */
J9AVLTreeNode *
avl_search(J9AVLTree *tree, uintptr_t searchValue)
{
	return findNode(tree, tree->rootNode, searchValue);
}

/**
 * Finds the right-most leaf in an AVL tree
 *
 * @param[in] tree  The tree
 * @param[in] walkSRPPtr  A pointer to the root of the tree to search
 * @param[in|out] heightChange  The height change
 *
 * @return  The found leaf or null in the case of an error
 */
static J9AVLTreeNode *
findRightMostLeaf(J9AVLTree *tree, J9WSRP *walkSRPPtr, intptr_t *heightChange)
{
	J9AVLTreeNode *find;
	J9AVLTreeNode *walk = NULL;

	Trc_AVL_findRightMostLeaf_Entry(tree, walkSRPPtr, heightChange);

	walk = AVL_SRP_GETNODE(*walkSRPPtr);
	if (!walk) {
		Trc_AVL_findRightMostLeaf_NotFound();
		return NULL;
	}

	find = findRightMostLeaf(tree, &(walk->rightChild), heightChange);
	if (!find) {
		/* this is the last node in the list */
		find = walk;
		/* the predecessor at most can have one left child node -- or it is null */
		AVL_SRP_PTR_SETNODE(walkSRPPtr, AVL_SRP_GETNODE(find->leftChild));
		AVL_SRP_SET_TO_NULL(find->leftChild);
		*heightChange = -1;
		if (tree->genericActionHook) {
			tree->genericActionHook(tree, walk, J9AVLTREE_ACTION_REPLACE_REMOVED_PARENT);
		}
	} else {
		rebalance(tree, NULL, walkSRPPtr, 1, heightChange);
	}

	Trc_AVL_findRightMostLeaf_Exit(find);
	return find;
}

/**
 * Rotate the nodes in an AVL tree
 * Direction specifies the direction to rotate (ie: the direction away from the heavy node)
 *
 * @param[in] tree  The tree
 * @param[in] walk  The root of the tree to search
 * @param[in] direction  The direction to rotate
 * @param[in|out] heightChange  The height change
 *
 * @return  The heavy node
 */
static J9AVLTreeNode *
rotate(J9AVLTree *tree, J9AVLTreeNode *walk, intptr_t direction, intptr_t *heightChange)
{
	J9WSRP *heavyNodePtr;
	J9WSRP *graftNodePtr;
	J9AVLTreeNode *heavyNode;

	Trc_AVL_rotate_Entry(tree, walk, direction, heightChange);

	if (tree->genericActionHook) {
		tree->genericActionHook(tree, (J9AVLTreeNode *)walk, J9AVLTREE_ACTION_SINGLE_ROTATE);
	}

	if (direction < 0) {
		heavyNodePtr = &walk->rightChild;
		heavyNode    = AVL_NNSRP_GETNODE(*heavyNodePtr);
		graftNodePtr = &heavyNode->leftChild;
	} else {
		heavyNodePtr = &walk->leftChild;
		heavyNode    = AVL_NNSRP_GETNODE(*heavyNodePtr);
		graftNodePtr = &heavyNode->rightChild;
	}

	AVL_SRP_PTR_SETNODE(heavyNodePtr, AVL_SRP_GETNODE(*graftNodePtr));
	AVL_NNSRP_PTR_SETNODE(graftNodePtr, walk);

	if (AVL_GETBALANCE(heavyNode) != AVL_BALANCED) {
		/* this rotation reduces the amount the tree is growing by one */

		/* if the height was growing, it is no longer growing */
		if (*heightChange > 0) {
			*heightChange = 0;
		}

		AVL_SETBALANCE(heavyNode, AVL_BALANCED);
		AVL_SETBALANCE(walk, AVL_BALANCED);
	} else {
		/* the only way that the heavy node can be balanced is via deletion
		 * otherwise a single or double rotation will have already occured */
		*heightChange = 0;
		if (direction < 0) {
			AVL_SETBALANCE(heavyNode, AVL_LEFTHEAVY);
			AVL_SETBALANCE(walk, AVL_RIGHTHEAVY);
		} else {
			AVL_SETBALANCE(heavyNode, AVL_RIGHTHEAVY);
			AVL_SETBALANCE(walk, AVL_LEFTHEAVY);
		}
	}

	Trc_AVL_rotate_Exit(heavyNode);

	return heavyNode;
}

/**
 * Double-rotate the nodes in an AVL tree
 * Direction specifies the direction to rotate (ie: the direction away from the heavy node)
 *
 * @param[in] tree  The tree
 * @param[in] walk  The root of the tree to search
 * @param[in] direction  The direction to rotate
 * @param[in|out] heightChange  The height change
 *
 * @return  The new root node
 */
static J9AVLTreeNode *
doubleRotate(J9AVLTree *tree, J9AVLTreeNode *walk, intptr_t direction, intptr_t *heightChange)
{
	J9WSRP *heavyNodePtr;
	J9WSRP *graft1NodePtr;
	J9WSRP *graft2NodePtr;
	J9WSRP *newrootNodePtr;
	J9AVLTreeNode *heavyNode;
	J9AVLTreeNode *newrootNode;

	Trc_AVL_doubleRotate_Entry(tree, walk, direction, heightChange);

	if (tree->genericActionHook) {
		tree->genericActionHook(tree, (J9AVLTreeNode *)walk, J9AVLTREE_ACTION_DOUBLE_ROTATE);
	}

	if (direction < 0) {
		heavyNodePtr   = &walk->rightChild;
		heavyNode      = AVL_NNSRP_GETNODE(*heavyNodePtr);

		newrootNodePtr = &heavyNode->leftChild;
		newrootNode    = AVL_NNSRP_GETNODE(*newrootNodePtr);

		graft1NodePtr  = &newrootNode->rightChild;
		graft2NodePtr  = &newrootNode->leftChild;
	} else {
		heavyNodePtr   = &walk->leftChild;
		heavyNode      = AVL_NNSRP_GETNODE(*heavyNodePtr);

		newrootNodePtr = &heavyNode->rightChild;
		newrootNode    = AVL_NNSRP_GETNODE(*newrootNodePtr);

		graft1NodePtr  = &newrootNode->leftChild;
		graft2NodePtr  = &newrootNode->rightChild;
	}

	AVL_SRP_PTR_SETNODE(newrootNodePtr, AVL_SRP_GETNODE(*graft1NodePtr));
	AVL_NNSRP_PTR_SETNODE(graft1NodePtr, heavyNode);

	AVL_SRP_PTR_SETNODE(heavyNodePtr, AVL_SRP_GETNODE(*graft2NodePtr));
	AVL_NNSRP_PTR_SETNODE(graft2NodePtr, walk);

	if (AVL_GETBALANCE(newrootNode) == AVL_BALANCED) {
		AVL_SETBALANCE(heavyNode, AVL_BALANCED);
		AVL_SETBALANCE(walk, AVL_BALANCED);
	} else if (AVL_GETBALANCE(newrootNode) == AVL_LEFTHEAVY) {
		if (direction < 0) {
			AVL_SETBALANCE(heavyNode, AVL_RIGHTHEAVY);
			AVL_SETBALANCE(walk, AVL_BALANCED);
		} else {
			AVL_SETBALANCE(heavyNode, AVL_BALANCED);
			AVL_SETBALANCE(walk, AVL_RIGHTHEAVY);
		}
	} else if (direction < 0) {
		AVL_SETBALANCE(heavyNode, AVL_BALANCED);
		AVL_SETBALANCE(walk, AVL_LEFTHEAVY);
	} else {
		AVL_SETBALANCE(heavyNode, AVL_LEFTHEAVY);
		AVL_SETBALANCE(walk, AVL_BALANCED);
	}

	AVL_SETBALANCE(newrootNode, AVL_BALANCED);

	/* if the height was growing, it is no longer growing */
	if (*heightChange > 0) {
		*heightChange = 0;
	}

	Trc_AVL_doubleRotate_Exit(newrootNode);

	return newrootNode;
}

/**
 * Rebalances an AVL tree.
 * This function should be called with either walkPtr OR walkSRPPtr set.
 * Use walkPtr if you have a direct pointer to the node or walkSRPPtr if you have a pointer to a WSRP
 *
 * @param[in] tree  The tree
 * @param[in] walkPtr  A pointer to the root of the tree to balance (walkSRPPtr is NULL)
 * @param[in] walkSRPPtr  A pointer to an SRP of the root of the tree to balance (walkPtr is NULL)
 * @param[in|out] heightChange  The height change
 */
static void
rebalance(J9AVLTree *tree, J9AVLTreeNode **walkPtr, J9WSRP *walkSRPPtr, intptr_t direction, intptr_t *heightChange)
{
	J9AVLTreeNode *walk = NULL;
	uintptr_t walkBalance;

	if (!(*heightChange)) {
		return;
	}

	Trc_AVL_rebalance_Entry(tree, walkPtr, walkSRPPtr, direction, heightChange);

	/* if we shrunk nodes, flip the direction, thus direction points towards heavy direction */
	if (*heightChange < 0) {
		direction = -direction;
	}

	/* Assumption is that if walkSRPPtr is NULL, walkPtr is not */
	if (!walkSRPPtr) {
		walk = AVL_GETNODE(*walkPtr);
	} else {
		walk = AVL_NNSRP_GETNODE(*walkSRPPtr);
	}

	walkBalance = AVL_GETBALANCE(walk);

	if (walkBalance == AVL_BALANCED) {
		AVL_SETBALANCE(walk, (direction < 0) ? AVL_LEFTHEAVY : AVL_RIGHTHEAVY);
		/* if the tree was shrinking, we've just unbalanced a node .. no longer shrinking */
		if (*heightChange < 0) {
			*heightChange = 0;
		}
	} else if ((walkBalance == AVL_LEFTHEAVY) == (direction < 0)) {
		J9AVLTreeNode *rotateResult = NULL;

		/* left heavy adding to the left or right heavy adding to the right */
		if ((direction < 0) && (AVL_GETBALANCE(AVL_NNSRP_GETNODE(walk->leftChild)) == AVL_RIGHTHEAVY)) {
			/* doubleRotate */
			rotateResult = (J9AVLTreeNode *)doubleRotate(tree, walk, -direction, heightChange);
		} else if ((direction > 0) && (AVL_GETBALANCE(AVL_NNSRP_GETNODE(walk->rightChild)) == AVL_LEFTHEAVY)) {
			/* doubleRotate */
			rotateResult = (J9AVLTreeNode *)doubleRotate(tree, walk, -direction, heightChange);
		} else {
			/* single rotate */
			rotateResult = (J9AVLTreeNode *)rotate(tree, walk, -direction, heightChange);
		}

		/* Assumption is that if walkSRPPtr is NULL, walkPtr is not */
		if (!walkSRPPtr) {
			AVL_SETNODE(*walkPtr, rotateResult);
		} else {
			AVL_NNSRP_PTR_SETNODE(walkSRPPtr, rotateResult);
		}
	} else {
		AVL_SETBALANCE(walk, AVL_BALANCED);

		/* added height making node become equal -- no more growing */
		if (*heightChange > 0) {
			*heightChange = 0;
		}
	}

	Trc_AVL_rebalance_Exit(*heightChange);
}

/**
 * Inserts a node into an AVL tree.
 * This function should always be called externally with walkSRPPtr being NULL.
 * All recursive calls will have walkPtr==NULL and a value for walkSRPPtr.
 *
 * IMPORTANT: It is the responsibility of the caller to ensure that the nodes are aligned
 *
 * @param[in] tree  The tree
 * @param[in] walkPtr  A pointer to the root of the tree
 * @param[in] walkSRPPtr  Should always be NULL (only used when the function calls itself)
 * @param[in] node  The node to insert (can be a node or an SRP node)
 * @param[in|out] heightChange  The height change
 *
 * @return  The node inserted
 */
static J9AVLTreeNode *
insertNode(J9AVLTree *tree, J9AVLTreeNode **walkPtr, J9WSRP *walkSRPPtr, J9AVLTreeNode *node, intptr_t *heightChange)
{
	J9AVLTreeNode *find = NULL;
	J9AVLTreeNode *walk;
	intptr_t dir;

	Trc_AVL_insertNode_Entry(tree, walkPtr, walkSRPPtr, node, heightChange);

	if (!node) {
		goto _done;
	}

	/* Assumption is that if walkSRPPtr is NULL, walkPtr is not */
	if (!walkSRPPtr) {
		walk = AVL_GETNODE(*walkPtr);
	} else {
		walk = AVL_SRP_GETNODE(*walkSRPPtr);
	}
	if (!walk) {
		if (!walkSRPPtr) {
			AVL_SETNODE(*walkPtr, node); /* add the node */
		} else {
			AVL_NNSRP_PTR_SETNODE(walkSRPPtr, node);
		}
		*heightChange = 1; /* height of this tree increased one */
		if (tree->genericActionHook) {
			tree->genericActionHook(tree, node, J9AVLTREE_ACTION_INSERT);
		}
		Trc_AVL_insertNode_Trivial(node);
		return node;
	}

	dir = tree->insertionComparator(tree, node, walk);
	if (!dir) {
		/* node is already in the tree */
		*heightChange = 0; /* no change in tree structure */
		if (tree->genericActionHook) {
			tree->genericActionHook(tree, walk, J9AVLTREE_ACTION_INSERT_EXISTS);
		}
		Trc_AVL_insertNode_Exists(walk);
		return walk;
	}

	if (dir < 0) {
		find = insertNode(tree, NULL, &(walk->leftChild), node, heightChange);
	} else {
		find = insertNode(tree, NULL, &(walk->rightChild), node, heightChange);
	}

	/* if we added a node */
	if ((find == node) && (*heightChange)) {
		rebalance(tree, walkPtr, walkSRPPtr, dir, heightChange);
	}

_done :
	Trc_AVL_insertNode_Recursive(find);
	return find;
}

/**
 * Finds a node in an AVL tree or an SRP AVL tree.
 *
 * @param[in] tree  The tree
 * @param[in] node  The root node to search from
 * @param[in] search  The key value to search with
 *
 * @return  The node found or NULL
 */
static J9AVLTreeNode *
findNode(J9AVLTree *tree, J9AVLTreeNode *node, uintptr_t search)
{
	intptr_t dir;
	J9AVLTreeNode *walk = node;

	Trc_AVL_findNode_Entry(tree, walk, search);

	while (walk) {
		dir = tree->searchComparator(tree, search, walk);
		if (dir == 0) {
			break;
		}
		if (dir < 0) {
			walk = AVL_SRP_GETNODE(walk->leftChild);
		} else {
			walk = AVL_SRP_GETNODE(walk->rightChild);
		}
	}

	Trc_AVL_findNode_Exit(walk);

	return walk;
}

/**
 * Delete a node from an AVL tree.
 * This function should always be called externally with walkSRPPtr being NULL.
 * All recursive calls will have walkPtr==NULL and a value for walkSRPPtr.
 *
 * @param[in] tree  The tree
 * @param[in] walkPtr  A pointer to the root of the tree
 * @param[in] walkSRPPtr  Should always be NULL (only used when the function calls itself)
 * @param[in] node  The node to delete (can be a node or an SRP node)
 * @param[in|out] heightChange  The height change
 *
 * @return  The node deleted or NULL
 */
static J9AVLTreeNode *
deleteNode(J9AVLTree *tree, J9AVLTreeNode **walkPtr, J9WSRP *walkSRPPtr, J9AVLTreeNode *node, intptr_t *heightChange)
{
	J9AVLTreeNode *find;
	J9AVLTreeNode *walk;
	intptr_t dir;

	Trc_AVL_deleteNode_Entry(tree, walkPtr, walkSRPPtr, node, heightChange);

	/* Assumption is that if walkSRPPtr is NULL, walkPtr is not */
	if (!walkSRPPtr) {
		walk = AVL_GETNODE(*walkPtr);
	} else {
		walk = AVL_SRP_GETNODE(*walkSRPPtr);
	}
	if (!walk) {
		if (tree->genericActionHook) {
			tree->genericActionHook(tree, walk, J9AVLTREE_ACTION_REMOVE_NOT_IN_TREE);
		}
		Trc_AVL_deleteNode_NotInTree();
		return NULL;
	}

	dir = tree->insertionComparator(tree, node, walk);
	if (!dir) {
		J9AVLTreeNode *walkLeftChild, *walkRightChild;

		walkLeftChild = AVL_SRP_GETNODE(walk->leftChild);
		walkRightChild = AVL_SRP_GETNODE(walk->rightChild);

		if (!walkLeftChild) {
			if (!walkSRPPtr) {
				AVL_SETNODE(*walkPtr, walkRightChild);
			} else {
				AVL_SRP_PTR_SETNODE(walkSRPPtr, walkRightChild);
			}
			AVL_SRP_SET_TO_NULL(walk->rightChild);
			*heightChange = -1;
		} else if (!walkRightChild) {
			if (!walkSRPPtr) {
				AVL_SETNODE(*walkPtr, walkLeftChild);
			} else {
				AVL_SRP_PTR_SETNODE(walkSRPPtr, walkLeftChild);
			}
			AVL_SRP_SET_TO_NULL(walk->leftChild);
			*heightChange = -1;
		} else {
			J9AVLTreeNode *leaf;

			leaf = findRightMostLeaf(tree, &(walk->leftChild), heightChange);

			AVL_SRP_SETNODE(leaf->leftChild, AVL_SRP_GETNODE(walk->leftChild));
			AVL_SRP_SETNODE(leaf->rightChild, AVL_SRP_GETNODE(walk->rightChild));
			AVL_SETBALANCE(leaf, AVL_GETBALANCE(walk));

			AVL_SRP_SET_TO_NULL(walk->leftChild);
			AVL_SRP_SET_TO_NULL(walk->rightChild);

			if (!walkSRPPtr) {
				AVL_SETNODE(*walkPtr, leaf);
			} else {
				AVL_NNSRP_PTR_SETNODE(walkSRPPtr, leaf);
			}
			rebalance(tree, walkPtr, walkSRPPtr, -1, heightChange);
		}

		AVL_SETBALANCE(walk, AVL_BALANCED);

		if (tree->genericActionHook) {
			tree->genericActionHook(tree, walk, J9AVLTREE_ACTION_REMOVE);
		}
		Trc_AVL_deleteNode_Removed(walk);
		return walk;
	}

	if (dir < 0) {
		find = deleteNode(tree, NULL, &(walk->leftChild), node, heightChange);
	} else {
		find = deleteNode(tree, NULL, &(walk->rightChild), node, heightChange);
	}

	/* if we deleted a node */
	if (find) {
		rebalance(tree, walkPtr, walkSRPPtr, dir, heightChange);
	}

	Trc_AVL_deleteNode_Recursive(find);
	return find;
}
