/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMRHASHTABLE_H
#define OMRHASHTABLE_H

/*
 * @ddr_namespace: default
 */

#ifdef __cplusplus
extern "C" {
#endif


/* DO NOT DIRECTLY INCLUDE THIS FILE! */
/* Include hashtable_api.h instead */


#include "omravl.h"
#include "omrcomp.h"
#include "omrport.h"
#include "pool_api.h"

/**
 * Hash table flags
 */
#define J9HASH_TABLE_DO_NOT_GROW	0x00000001	/*!< Do not grow & rehash the table while set */
#define J9HASH_TABLE_COLLISION_RESILIENT	0x00000002	/*!< Use hash table using avl trees for collision resolution instead of lists */
#define J9HASH_TABLE_ALLOCATE_ELEMENTS_USING_MALLOC32	0x00000004	/*!< Allocate table elements using the malloc32 function */
#define J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION	0x00000008	/*!< Allow space optimized hashTable, some functions not supported */
#define J9HASH_TABLE_DO_NOT_REHASH	0x00000010	/*!< Do not rehash the table while set */

#define J9HASH_TABLE_AVL_TREE_TAG_BIT ((uintptr_t)0x00000001) /*!< Bit to indicate that hastable slot contains a pointer to an AVL tree */

/**
 * Hash Table state constants for iteration
 */
#define J9HASH_TABLE_ITERATE_STATE_LIST_NODES 0
#define J9HASH_TABLE_ITERATE_STATE_TREE_NODES 1
#define J9HASH_TABLE_ITERATE_STATE_FINISHED  2

/**
 * Macros for getting at data directly from AVLTreeNodes
 */
#define AVL_NODE_TO_DATA(p) ((void *)((uint8_t *)(p) + sizeof(J9AVLTreeNode)))
#define AVL_DATA_TO_NODE(p) (((J9AVLTreeNode *)((uint8_t *)(p) - sizeof(J9AVLTreeNode))))

/**
 * Hash table flag macros
 */
#define hashTableCanGrow(table) (((table)->flags & J9HASH_TABLE_DO_NOT_GROW) ? 0 : 1)
#define hashTableCanRehash(table) (((table)->flags & J9HASH_TABLE_DO_NOT_REHASH) ? 0 : 1)
#define hashTableSetFlag(table,flag) ((table)->flags |= (flag))
#define hashTableResetFlag(table,flag) ((table)->flags &= ~(flag))

/**
* Hash table state queries
*/
#define hashTableIsSpaceOptimized(table) (NULL == table->listNodePool)


struct J9HashTable; /* Forward struct declaration */
struct J9AVLTreeNode; /* Forward struct declaration */
typedef uintptr_t (*J9HashTableHashFn)(void *entry, void *userData);  /* Forward struct declaration */
typedef uintptr_t (*J9HashTableEqualFn)(void *leftEntry, void *rightEntry, void *userData);  /* Forward struct declaration */
typedef intptr_t (*J9HashTableComparatorFn)(struct J9AVLTree *tree, struct J9AVLTreeNode *leftNode, struct J9AVLTreeNode *rightNode);  /* Forward struct declaration */
typedef void (*J9HashTablePrintFn)(OMRPortLibrary *portLibrary, void *entry, void *userData);  /* Forward struct declaration */
typedef uintptr_t (*J9HashTableDoFn)(void *entry, void *userData);  /* Forward struct declaration */
typedef struct J9HashTable {
	const char *tableName;
	uint32_t tableSize;
	uint32_t numberOfNodes;
	uint32_t numberOfTreeNodes;
	uint32_t entrySize;
	uint32_t listNodeSize;
	uint32_t treeNodeSize;
	uint32_t nodeAlignment;
	uint32_t flags;
	uint32_t memoryCategory;
	uint32_t listToTreeThreshold;
	void **nodes;
	struct J9Pool *listNodePool;
	struct J9Pool *treeNodePool;
	struct J9Pool *treePool;
	struct J9AVLTree *avlTreeTemplate;
	uintptr_t (*hashFn)(void *key, void *userData) ;
	uintptr_t (*hashEqualFn)(void *leftKey, void *rightKey, void *userData) ;
	void (*printFn)(OMRPortLibrary *portLibrary, void *key, void *userData) ;
	struct OMRPortLibrary *portLibrary;
	void *equalFnUserData;
	void *hashFnUserData;
	struct J9HashTable *previous;
} J9HashTable;

typedef struct J9HashTableState {
	struct J9HashTable *table;
	uint32_t bucketIndex;
	uint32_t didDeleteCurrentNode;
	void **pointerToCurrentNode;
	uintptr_t iterateState;
	struct J9PoolState poolState;
} J9HashTableState;

#ifdef __cplusplus
}
#endif

#endif /* OMRHASHTABLE_H */
