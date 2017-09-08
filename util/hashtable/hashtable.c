/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

/*
 * file    : hashtable.c
 *
 *  vim: nocompatible:tabstop=8:shiftwidth=3:expandtab:smarttab:
 *  vim: cindent:cinoptions=f1s,{1s,g0,h0,t0,(0,j
 *
 *  Generic hash table implementation
 */

#include <string.h>
#include "omrcfg.h"
#include "hashtable_internal.h"
#include "ut_hashtable.h"
#include "avl_api.h"
#include "omrutilbase.h"
#include "omrutil.h"

#undef HASHTABLE_DEBUG
#define HASHTABLE_ENABLE_ASSERTS

#if defined(HASHTABLE_ENABLE_ASSERTS)
#define HASHTABLE_ASSERT(p) Assert_hashTable_true(p)
#else
#define HASHTABLE_ASSERT(p)
#endif

#ifdef HASHTABLE_DEBUG
#define hashTable_printf omrtty_printf
#define HASHTABLE_DEBUG_PORT(_portLibrary) OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary)
#else
#define hashTable_printf(...)
#define HASHTABLE_DEBUG_PORT(_portLibrary)
#endif

#define HASH_TABLE_SIZE_MIN 17
#define HASH_TABLE_SIZE_MAX 2200103

/**
 * Node macros
 */
#define NEXT(p) *((void **)((uint8_t *)p + table->listNodeSize - sizeof(uintptr_t)))

#define AVL_TREE_TAG_BIT ((uintptr_t)0x00000001)
#define AVL_TREE_TAGGED(p) (((uintptr_t)(p)) & AVL_TREE_TAG_BIT)
#define AVL_TREE_TAG(p) ((J9AVLTree *)(((uintptr_t)(p)) | AVL_TREE_TAG_BIT))
#define AVL_TREE_UNTAG(p) ((J9AVLTree *)(((uintptr_t)(p)) & (~AVL_TREE_TAG_BIT)))

/**
 * Stolen from gc_base/gcutils.h
 */
#define ROUND_TO_SIZEOF_UDATA(number) (((number) + (sizeof(uintptr_t) - 1)) & (~(sizeof(uintptr_t) - 1)))

static uint32_t hashTableNextSize(uint32_t size);
static uintptr_t hashTableGrow(J9HashTable *table);
static J9HashTable *hashTableNewImpl(OMRPortLibrary *portLibrary, const char *tableName,
	uint32_t tableSize, uint32_t entrySize, uint32_t entryAlignment, uint32_t flags, uint32_t memoryCategory, uint32_t listToTreeThreshold,
	J9HashTableHashFn hashFn, J9HashTableEqualFn hashEqualFn, J9HashTableComparatorFn comparatorFn, J9HashTablePrintFn printFn,
	void *functionUserData);
static void **hashTableFindNodeSpaceOpt(J9HashTable *table, void *entry, void **head);
static void **hashTableFindNodeInList(J9HashTable *table, void *entry, void **head);
static void *hashTableFindNodeInTree(J9HashTable *table, void *entry, void **head);
static void *hashTableAddNodeSpaceOpt(J9HashTable *table, void *entry, void **head);
static void *hashTableAddNodeInList(J9HashTable *table, void *entry, void **head);
static void *hashTableAddNodeInTree(J9HashTable *table, void *entry, void **head);
static uint32_t hashTableRemoveNodeSpaceOpt(J9HashTable *table, void *entry, void **head);
static uint32_t hashTableRemoveNodeInList(J9HashTable *table, void *entry, void **head);
static uint32_t hashTableRemoveNodeInTree(J9HashTable *table, void *entry, void **head);
static uintptr_t listToTree(J9HashTable *table, void **head, uintptr_t listLength);
static void rebuildFromPools(J9HashTable *table, uint32_t newSize, void **newNodes);
static uintptr_t hashTableGrowSpaceOpt(J9HashTable *, uint32_t newSize);
static uintptr_t hashTableGrowListNodes(J9HashTable *table, uint32_t newSize);
static uintptr_t collisionResilientHashTableGrow(J9HashTable *table, uint32_t newSize);

static const uint32_t primesTable[] = {
	17,
	37,
	73,
	149,
	293,
	587,
	811,
	1171,
	1693,
	2347,
	3511,
	4691,
	9391,
	18787,
	37573,
	75149,
	150299,
	300007,
	600071,
	1200077,
	2200103,
};


/*****************************************************************************
 *  Hash Table statistics defines
 */

#define SPACE_OPT_LIMIT primesTable[3]

static const uint32_t primesTableSize = sizeof(primesTable) / sizeof(uint32_t);

/* Builds an equality function from a comparator function */
static uintptr_t
comparatorToEqualFn(void *leftKey, void *rightKey, void *userData)
{
	J9AVLTree *tree = userData;
	J9HashTableComparatorFn comparatorFn = tree->insertionComparator;
	J9AVLTreeNode *leftNode = AVL_DATA_TO_NODE(leftKey);
	J9AVLTreeNode *rightNode = AVL_DATA_TO_NODE(rightKey);
	/* Note, swapping rightKey and leftKey here to make things consistent. avl_insert() has the search key
	 * as the second node argument. The hash table implementation on the other hand has the search key as
	 * the first node argument.
	 */
	if (0 == comparatorFn(tree, rightNode, leftNode)) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * \brief       Create a new hash table that turns list-chains into AVL trees if too many entries hash to the same slot.
 * \ingroup     hash_table
 *
 *
 * @param portLibrary  The port library
 * @param tableName     	A string giving the name of the table, recommended to use OMR_GET_CALLSITE()
 * 							within caller code, this is not a restriction. If a unique name is used, it
 * 							must be easily searchable, i.e. there should not be more than a few false hits
 * 							when searching for the name. If necessary to ensure uniqueness or find-ability,
 * 							create a list of the unique names or name conventions in this file.
 * @param tableSize   Initial number of hash table nodes (if zero, use a suitable default)
 * @param entrySize   Size of the user-data for each node
 * @param flags	Optional flags for extra options
 * @param memoryCategory  memory category for which memory allocated by hashtable should use
 * @param listToTreeThreshold  The threshold after which list chains are transformed into AVL trees
 * @param hashFn  Mandatory hashing function ptr
 * @param printFn  Optional node-print function ptr
 * @param searchComparator  Mandatory comparison function required for tree comparison
 * @param userData  Optional userData ptr to be passed to hashFn and hashEqualFn
 * @return  An initialized hash table
 *
 * In addition to the description provided by hashTableNew, this hashtable it designed to be resilient to hash collisions.
 * Specifically, if there are more than listToTreeThreshold collisions in a single bucket, the backing linked list used
 * for collision resolution will be transformed into an AVL tree.
 *
 * Note that the comparatorFn will be used for both searching and comparisons into AVL trees.
 * Note also that the trees are only transformed back into normal lists during a hash table grow (assuming they are less than or equal
 * to listToTreeThreshold).
 * Note also that lists may not be transformed into trees if there was not enough memory to allocate the nodes.  In other words lists of length greater
 * than listToTreeThreshold may exist.
 * Also note that nodes are automatically aligned to uintptr_t. (Additional alignment cannot be specified)
 *
 * In general, you should expect collisionResilientHashTable to be slower than a regular hashtable and use more memory.
 *
 *  J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION is not supported (will be ignored)
 *
 */
J9HashTable *
collisionResilientHashTableNew(
	OMRPortLibrary *portLibrary,
	const char *tableName,
	uint32_t tableSize,
	uint32_t entrySize,
	uint32_t flags,
	uint32_t memoryCategory,
	uint32_t listToTreeThreshold,
	J9HashTableHashFn hashFn,
	J9HashTableComparatorFn comparatorFn,
	J9HashTablePrintFn printFn,
	void *functionUserData)
{
	return hashTableNewImpl(portLibrary, tableName, tableSize, entrySize, sizeof(uintptr_t), flags | J9HASH_TABLE_COLLISION_RESILIENT, memoryCategory, listToTreeThreshold, hashFn, NULL, comparatorFn, printFn, functionUserData);
}

/**
 * \brief       Create a new hash table
 * \ingroup     hash_table
 *
 *
 * @param portLibrary       The port library
 * @param tableName     	A string giving the name of the table, recommended to use OMR_GET_CALLSITE()
 * 							within caller code, this is not a restriction. If a unique name is used, it
 * 							must be easily searchable, i.e. there should not be more than a few false hits
 * 							when searching for the name. If necessary to ensure uniqueness or find-ability,
 * 							create a list of the unique names or name conventions in this file.
 * @param tableSize         Initial number of hash table nodes (if zero, use a suitable default)
 * @param entrySize         Size of the user-data for each node
 * @param entryAlignment    Optional alignment required by entry data (0 for no alignment)
 * @param flags				Flags for extra options
 * @param memoryCategory	memoryCategory for which memory allocated by hashtable should use
 * @param hashFn            Mandatory hashing function ptr
 * @param hashEqualFn       Mandatory hash compare function ptr
 * @param printFn           Optional node-print function ptr
 * @param userData          Optional userData ptr to be passed to hashFn and hashEqualFn
 * @return                  An initialized hash table
 *
 *	Creates a hashtable with specified initial number of nodes. The actual number
 *  of nodes is ceiled to the nearest prime number obtained from primesTable[].
 *
 *  When J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION is defined and the entrySize fits in
 *  a uintptr_t, and the tableSize is small, a hashTable will be created without
 *  a backing pool. When the hashTable grows beyond a certain size, it will expand
 *  to use a backing pool.
 *  The following functions are not supported on space optimized hashTables:
 *  	storing NULL elements
 *  	hashTableForEachDo()
 *  	hashTableRehash()
 *  	hashTableDoRemove()
 *
 */
J9HashTable *
hashTableNew(
	OMRPortLibrary *portLibrary,
	const char *tableName,
	uint32_t tableSize,
	uint32_t entrySize,
	uint32_t entryAlignment,
	uint32_t flags,
	uint32_t memoryCategory,
	J9HashTableHashFn hashFn,
	J9HashTableEqualFn hashEqualFn,
	J9HashTablePrintFn printFn,
	void *functionUserData)
{
	return hashTableNewImpl(portLibrary, tableName, tableSize, entrySize, entryAlignment, flags, memoryCategory, U_32_MAX, hashFn, hashEqualFn, NULL, printFn, functionUserData);
}

static J9HashTable *
hashTableNewImpl(
	OMRPortLibrary *portLibrary,
	const char *tableName,
	uint32_t tableSize,
	uint32_t entrySize,
	uint32_t entryAlignment,
	uint32_t flags,
	uint32_t memoryCategory,
	uint32_t listToTreeThreshold,
	J9HashTableHashFn hashFn,
	J9HashTableEqualFn hashEqualFn,
	J9HashTableComparatorFn comparatorFn,
	J9HashTablePrintFn printFn,
	void *functionUserData)
{
	J9HashTable *hashTable = NULL;
	BOOLEAN spaceOpt = FALSE;
	HASHTABLE_DEBUG_PORT(portLibrary);

	hashTable = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9HashTable), tableName, memoryCategory);
	hashTable_printf("hashTableNew <%s>: tableSize=%d, table=%p\n", tableName, tableSize, hashTable);
	if (NULL == hashTable) {
		goto error;
	}
	memset(hashTable, 0, sizeof(J9HashTable));
	/* Set portLibrary and other fields now, so we can use omrmem_free_memory functionality in hashTableFree() if errors occur */
	hashTable->portLibrary = portLibrary;
	hashTable->tableName = tableName;
	hashTable->hashFn = hashFn;
	hashTable->printFn = printFn;
	hashTable->flags = flags;
	hashTable->numberOfNodes = 0;
	hashTable->numberOfTreeNodes = 0;
	hashTable->memoryCategory = memoryCategory;
	hashTable->listToTreeThreshold = listToTreeThreshold;
	hashTable->hashFnUserData = functionUserData;

	/* fixup tableSize to be the first prime >= to users choice */
	if (tableSize <= HASH_TABLE_SIZE_MIN) {
		hashTable->tableSize = HASH_TABLE_SIZE_MIN;
	} else if (tableSize >= HASH_TABLE_SIZE_MAX) {
		hashTable->tableSize = HASH_TABLE_SIZE_MAX;
	} else {
		hashTable->tableSize = hashTableNextSize(tableSize - 1);
	}

	hashTable->entrySize = entrySize;
	/* listNodeSize is sizeof user-data + a next-pointer */
	if (entryAlignment) {
		hashTable->listNodeSize = (((ROUND_TO_SIZEOF_UDATA(entrySize) + sizeof(uintptr_t)) + entryAlignment - 1) / entryAlignment) * entryAlignment;
		hashTable->treeNodeSize = (((ROUND_TO_SIZEOF_UDATA(entrySize) + sizeof(J9AVLTreeNode)) + entryAlignment - 1) / entryAlignment) * entryAlignment;
	} else {
		hashTable->listNodeSize = ROUND_TO_SIZEOF_UDATA(entrySize) + sizeof(uintptr_t);
		hashTable->treeNodeSize = ROUND_TO_SIZEOF_UDATA(entrySize) + sizeof(J9AVLTreeNode);
	}
	hashTable->nodeAlignment = entryAlignment;

	if (J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION == ((flags & J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION))
		&& (hashTable->listNodeSize == (2 * sizeof(uintptr_t)))
		&& (hashTable->tableSize <= SPACE_OPT_LIMIT)
#if defined(OMR_ENV_DATA64)
		&& (0 == (flags & J9HASH_TABLE_ALLOCATE_ELEMENTS_USING_MALLOC32))
#endif /* OMR_ENV_DATA64 */
		&& (!(J9HASH_TABLE_COLLISION_RESILIENT == (flags & J9HASH_TABLE_COLLISION_RESILIENT)))
	) {
		/* create a hashTable with no backing pool */
		spaceOpt = TRUE;
	}

	if (spaceOpt) {
		hashTable->listNodePool = NULL;
	} else {
#if defined(OMR_ENV_DATA64)
		if (J9HASH_TABLE_ALLOCATE_ELEMENTS_USING_MALLOC32 == (flags & J9HASH_TABLE_ALLOCATE_ELEMENTS_USING_MALLOC32)) {
			hashTable->listNodePool = pool_new(hashTable->listNodeSize, tableSize, entryAlignment, POOL_NO_ZERO, tableName, memoryCategory, POOL_FOR_PORT_PUDDLE32(portLibrary));
		} else
#endif /* OMR_ENV_DATA64 */
		{
			hashTable->listNodePool = pool_new(hashTable->listNodeSize, tableSize, entryAlignment, POOL_NO_ZERO, tableName, memoryCategory, POOL_FOR_PORT(portLibrary));
		}

		if (NULL == hashTable->listNodePool) {
			goto error;
		}
	}

	if (J9HASH_TABLE_COLLISION_RESILIENT == (flags & J9HASH_TABLE_COLLISION_RESILIENT)) {
		/* Additional initialization for capability to turn lists to trees */
		hashTable->treePool = pool_new(sizeof(J9AVLTree), 0, sizeof(uintptr_t), 0, tableName, memoryCategory, POOL_FOR_PORT(portLibrary));
		if (NULL == hashTable->treePool) {
			goto error;
		}
		/* Create an AVLTree template we need for comparatorToEqualFn.  Malloc the memory instead of using the pool, as we clear the pool on growing */
		hashTable->avlTreeTemplate = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9AVLTree), tableName, memoryCategory);
		if (NULL == hashTable->avlTreeTemplate) {
			goto error;
		}
		memset(hashTable->avlTreeTemplate, 0, sizeof(J9AVLTree));
		hashTable->avlTreeTemplate->insertionComparator = comparatorFn;
		hashTable->avlTreeTemplate->searchComparator = (intptr_t (*)(struct J9AVLTree *, uintptr_t, J9AVLTreeNode *))comparatorFn;
		hashTable->avlTreeTemplate->portLibrary = portLibrary;
		hashTable->avlTreeTemplate->userData = functionUserData;
		hashTable->avlTreeTemplate->rootNode = NULL;
		/* Need to have the avlTreeTemplate here so that the comparatorToEqualFn can access it*/
		hashTable->equalFnUserData = hashTable->avlTreeTemplate;
		hashTable->hashEqualFn = comparatorToEqualFn;

#if defined(OMR_ENV_DATA64)
		if (J9HASH_TABLE_ALLOCATE_ELEMENTS_USING_MALLOC32 == (flags & J9HASH_TABLE_ALLOCATE_ELEMENTS_USING_MALLOC32)) {
			hashTable->treeNodePool = pool_new(hashTable->treeNodeSize, 0, entryAlignment, 0, OMR_GET_CALLSITE(), memoryCategory, POOL_FOR_PORT_PUDDLE32(portLibrary));
		} else
#endif  /* OMR_ENV_DATA64 */
		{
			hashTable->treeNodePool = pool_new(hashTable->treeNodeSize, 0, entryAlignment, 0, tableName, memoryCategory, POOL_FOR_PORT(portLibrary));
		}
		if (NULL == hashTable->treeNodePool) {
			goto error;
		}
	} else {
		hashTable->equalFnUserData = functionUserData;
		hashTable->hashEqualFn = hashEqualFn;
	}

	hashTable->nodes = portLibrary->mem_allocate_memory(portLibrary, sizeof(uintptr_t) * hashTable->tableSize, tableName, memoryCategory);
	if (NULL == hashTable->nodes) {
		goto error;
	}

	/* reset all the nodes */
	memset(hashTable->nodes, 0, sizeof(uintptr_t) * hashTable->tableSize);

	return hashTable;

error:
	hashTableFree(hashTable);

	return NULL;
}

/**
 * \brief         Free a previously allocated hash-table.
 * \ingroup     hash_table
 *
 *
 * @param table
 *
 *      Frees the hash-table and all node-chains.
 */
void
hashTableFree(J9HashTable *hashTable)
{
	if (NULL != hashTable) {
		OMRPORT_ACCESS_FROM_OMRPORT(hashTable->portLibrary);
		hashTable_printf("hashTableFree <%s>: table=%p\n", hashTable->tableName, hashTable);

		if (NULL != hashTable->nodes) {
			omrmem_free_memory(hashTable->nodes);
		}
		if (NULL != hashTable->avlTreeTemplate) {
			omrmem_free_memory(hashTable->avlTreeTemplate);
		}
		if (NULL != hashTable->listNodePool) {
			pool_kill(hashTable->listNodePool);
		}
		if (NULL != hashTable->treeNodePool) {
			pool_kill(hashTable->treeNodePool);
		}
		if (NULL != hashTable->treePool) {
			pool_kill(hashTable->treePool);
		}
		omrmem_free_memory(hashTable);
	}
}

/**
 * \brief       Find an entry in the hash table.
 * \ingroup     hash_table
 *
 *
 * @param table
 * @param entry
 * @return                  NULL if entry is not present in the table; otherwise a pointer to the user-data
 *
 */
void *
hashTableFind(J9HashTable *table, void *entry)
{
	uintptr_t hash = table->hashFn(entry, table->hashFnUserData) % table->tableSize;
	void **head = &table->nodes[hash];
	void *findNode = NULL;
	HASHTABLE_DEBUG_PORT(table->portLibrary);

	hashTable_printf("hashTableFind <%s>: table=%p entry=%p\n", table->tableName, table, entry);

	if (NULL == table->listNodePool) {
		void **node = hashTableFindNodeSpaceOpt(table, entry, head);
		findNode = (NULL != *node) ? node : NULL;
	} else if (NULL == *head) {
		findNode =  NULL;
	} else if (AVL_TREE_TAGGED(*head)) {
		findNode = hashTableFindNodeInTree(table, entry, head);
	} else {
		findNode = *hashTableFindNodeInList(table, entry, head);
	}
	return findNode;
}

static void **
hashTableFindNodeSpaceOpt(J9HashTable *table, void *entry, void **head)
{
	void **node = head;
	while ((NULL != *node) && (0 == table->hashEqualFn(node, entry, table->equalFnUserData))) {
		node += 1;
		if (node == &table->nodes[table->tableSize]) {
			node = &table->nodes[0];
		}
	}
	return node;
}

static void **
hashTableFindNodeInList(J9HashTable *table, void *entry, void **head)
{
	/* Look through the list looking for the correct key */
	void **node = head;
	while ((NULL != *node) && (0 == table->hashEqualFn(*node, entry, table->equalFnUserData))) {
		node = &NEXT(*node);
	}
	return node;
}


static void *
hashTableFindNodeInTree(J9HashTable *table, void *entry, void **head)
{
	/* Fake a tree node from an entry by just moving the pointer to where the start of a tree node would be */
	J9AVLTreeNode *treeEntry = AVL_DATA_TO_NODE(entry);
	J9AVLTree *tree = AVL_TREE_UNTAG(*head);
	J9AVLTreeNode *treeNode = avl_search(tree, (uintptr_t)treeEntry);

	if (NULL != treeNode) {
		return AVL_NODE_TO_DATA(treeNode);
	} else {
		return NULL;
	}
}
/**
 * \brief       Add an entry to the hash table.
 * \ingroup     hash_table
 *
 *
 * @param table
 * @param entry
 * @return                  NULL on failure (to allocate a new node); otherwise the entry pointer
 *
 *      The call may fail on allocating new nodes to hold the entry. If the
 *      entry is already present in the hash-table, returns a pointer to it.
 *      Otherwise, returns a pointer to the newly added entry.
 */
void *
hashTableAdd(J9HashTable *table, void *entry)
{
	uintptr_t hashCode = table->hashFn(entry, table->hashFnUserData);
	void **head = &table->nodes[hashCode % table->tableSize];
	void *addNode = NULL;
	BOOLEAN growFailure = FALSE;
	HASHTABLE_DEBUG_PORT(table->portLibrary);

	hashTable_printf("hashTableAdd <%s>: table=%p entry=%p\n", table->tableName, table, entry);

	if ((table->numberOfNodes + 1) == table->tableSize) {
		if (!hashTableCanGrow(table)) {
			goto done;
		}

		if ((0 != hashTableCanRehash(table)) && (0 == hashTableGrow(table))) {
			/* Need to recalculate head as tableSize and nodes were updated */
			head = &table->nodes[hashCode % table->tableSize];
		} else {
			growFailure = TRUE;
		}
	}

	if (NULL == table->listNodePool) {
		if (growFailure) {
			goto done;
		}
		addNode = hashTableAddNodeSpaceOpt(table, entry, head);
	} else if (NULL == *head) {
		addNode = hashTableAddNodeInList(table, entry, head);
	} else if (AVL_TREE_TAGGED(*head)) {
		addNode = hashTableAddNodeInTree(table, entry, head);
	} else {
		addNode = hashTableAddNodeInList(table, entry, head);
	}
done:
	return addNode;
}

static void *
hashTableAddNodeSpaceOpt(J9HashTable *table, void *entry, void **head)
{
	void **where = hashTableFindNodeSpaceOpt(table, entry, head);
	if (NULL == *where) {
		*(uintptr_t *)where = *(uintptr_t *)entry;
		table->numberOfNodes += 1;
	}
	return where;
}

static void *
hashTableAddNodeInList(J9HashTable *table, void *entry, void **head)
{
	void **where = head;
	uintptr_t listLength = 0;
	void *newNode = NULL;

	/* Find entry if it is in the list, otherwise return a slot where we can add a pointer to it */
	while ((NULL != *where) && (0 == table->hashEqualFn(*where, entry, table->equalFnUserData))) {
		where = &NEXT(*where);
		listLength += 1;
	}

	if (NULL != *where) {
		/* found the entry in the table*/
		newNode = *where;
	} else {
		/* Reached threshold, need to turn this list into a tree.  Note that failure is okay, we'll just add
		 * the node as a list if a tree could not be allocated
		 */
		if ((listLength > table->listToTreeThreshold) && (0 == listToTree(table, head, listLength))) {
			newNode = hashTableAddNodeInTree(table, entry, head);

			/* After creating the tree, whether we succeeded to add the node or not,
			 * consider ourselves done */
		} else {
			/* Create a new list node */
			newNode = pool_newElement(table->listNodePool);
			if (NULL != newNode) {
				memcpy(newNode, entry, table->entrySize);
				NEXT(newNode) = NULL;
				if (!hashTableCanGrow(table)) {
					issueWriteBarrier();
				}
				*where = newNode;
				table->numberOfNodes += 1;
			}
		}
	}
	return newNode;
}

static void *
hashTableAddNodeInTree(J9HashTable *table, void *entry, void **head)
{
	void *nodeData = NULL;
	J9AVLTree *tree = AVL_TREE_UNTAG(*head);
	J9AVLTreeNode *newNode = pool_newElement(table->treeNodePool);

	if (NULL != newNode) {
		J9AVLTreeNode *insertNode = NULL;

		memcpy(AVL_NODE_TO_DATA(newNode), entry, table->entrySize);
		insertNode = avl_insert(tree, newNode);
		if (NULL == insertNode) {
			/* Looking at avl tree code this is currently impossible, but leaving it here to be safe.. */
			pool_removeElement(table->treeNodePool, newNode);
			nodeData = NULL;
		} else if (insertNode != newNode) {
			/* Node was in tree */
			pool_removeElement(table->treeNodePool, newNode);
			nodeData = AVL_NODE_TO_DATA(insertNode);
		} else {
			/* newNode was inserted */
			nodeData = AVL_NODE_TO_DATA(newNode);

			table->numberOfNodes += 1;
			table->numberOfTreeNodes += 1;
		}
	}

	return nodeData;
}

/* Turns a list into a tree.
 * Returns 0 on success and the tree is inserted into 'head'
 * Returns 1 on failure and the list is left untouched
 */
static uintptr_t
listToTree(J9HashTable *table, void **head, uintptr_t listLength)
{
	uintptr_t rc = 1;
	J9AVLTree *tree = pool_newElement(table->treePool);
	Trc_hashTable_listToTree_Entry(table->tableName, table, head, listLength);

	if ((0 != hashTableCanRehash(table)) && (NULL != tree)) {
		uintptr_t minimumCapacity = ((uintptr_t)table->numberOfTreeNodes) + listLength;

		/* Fill the tree contents using the template*/
		*tree = *(table->avlTreeTemplate);
		if (0 == pool_ensureCapacity(table->treeNodePool, minimumCapacity)) {
			void *nodeToCopy = *head;

			while (NULL != nodeToCopy) {
				J9AVLTreeNode *newTreeNode = (J9AVLTreeNode *) pool_newElement(table->treeNodePool);
				J9AVLTreeNode *insertNode = NULL;
				J9AVLTreeNode *nextListNode = NEXT(nodeToCopy);
				/* We ensured capacity so this can't fail */
				HASHTABLE_ASSERT(NULL != newTreeNode);

				memcpy(AVL_NODE_TO_DATA(newTreeNode), nodeToCopy, table->entrySize);
				insertNode = avl_insert(tree, newTreeNode);
				/* Node has to have been inserted */
				HASHTABLE_ASSERT(insertNode == newTreeNode);

				pool_removeElement(table->listNodePool, nodeToCopy);
				nodeToCopy = nextListNode;
				table->numberOfTreeNodes += 1;
			}
			HASHTABLE_ASSERT(((uintptr_t)table->numberOfTreeNodes) == minimumCapacity);

			/* install a tagged pointer to the tree as head*/
			*head = AVL_TREE_TAG(tree);
			rc = 0;
		} else {
			pool_removeElement(table->treePool, tree);
			rc = 1;
		}
	}

	Trc_hashTable_listToTree_Exit(rc, tree);
	return rc;
}

/**
 * \brief       Remove an entry matching given key from the hash table
 * \ingroup     hash_table
 *
 *
 * @param table         hash table
 * @param entry         removed entry return [OUT]
 * @return                  0 on success, 1 on failure
 *
 *	Removes an entry matching given key from the hash-table and frees it.
 */
uint32_t
hashTableRemove(J9HashTable *table, void *entry)
{
	uintptr_t hash = table->hashFn(entry, table->hashFnUserData) % table->tableSize;
	void **head = &table->nodes[hash];
	uint32_t rc = 1;
	HASHTABLE_DEBUG_PORT(table->portLibrary);

	hashTable_printf("hashTableRemove <%s>: table=%p, entry=%p\n", table->tableName, table, entry);

	if (NULL == table->listNodePool) {
		rc = hashTableRemoveNodeSpaceOpt(table, entry, head);
	} else if (NULL == *head) {
		rc = 1;
	} else if (AVL_TREE_TAGGED(*head)) {
		rc = hashTableRemoveNodeInTree(table, entry, head);
	} else {
		rc = hashTableRemoveNodeInList(table, entry, head);
	}

	return rc;
}

static uint32_t
hashTableRemoveNodeSpaceOpt(J9HashTable *table, void *entry, void **head)
{
	void **node = hashTableFindNodeSpaceOpt(table, entry, head);
	void **endNode = &table->nodes[table->tableSize];
	uint32_t rc = 1;

	if (NULL != *node) {
		*node = NULL;
		node += 1;
		if (node == endNode) {
			node = &table->nodes[0];
		}
		while (NULL != *node) {
			uintptr_t hash = table->hashFn(node, table->hashFnUserData) % table->tableSize;
			void **newLocation = &table->nodes[hash];
			BOOLEAN found = FALSE;
			while (NULL != *newLocation) {
				if (*newLocation == *node) {
					found = TRUE;
					break;
				}
				newLocation += 1;
				if (newLocation == endNode) {
					newLocation = &table->nodes[0];
				}
			}
			if (!found) {
				*newLocation = *node;
				*node = NULL;
			}

			node += 1;
			if (node == endNode) {
				node = &table->nodes[0];
			}
		}
		table->numberOfNodes -= 1;
		rc = 0;
	}

	return rc;
}

static uint32_t
hashTableRemoveNodeInList(J9HashTable *table, void *entry, void **head)
{
	void **node = hashTableFindNodeInList(table, entry, head);
	uint32_t rc = 1;

	if (NULL != *node) {
		void *nodeToRemove = *node;
		*node = NEXT(*node);
		pool_removeElement(table->listNodePool, nodeToRemove);
		table->numberOfNodes -= 1;
		rc = 0;
	}
	return rc;
}

static uint32_t
hashTableRemoveNodeInTree(J9HashTable *table, void *entry, void **head)
{
	uint32_t rc = 1;
	J9AVLTree *tree = AVL_TREE_UNTAG(*head);
	J9AVLTreeNode *removedNode = avl_delete(tree, AVL_DATA_TO_NODE(entry));

	if (NULL != removedNode) {
		pool_removeElement(table->treeNodePool, removedNode);
		table->numberOfNodes -= 1;
		table->numberOfTreeNodes -= 1;
		rc = 0;
	}
	return rc;
}



/**
 * \brief         Call a user-defined function for all nodes in the hash table
 * \ingroup     hash_table
 *
 * @param table
 * @param doFn           function to be called
 * @param opaque        user data to be passed to doFn
 *
 * Walks all nodes in the hash-table, calling a user-defined function on the
 * user-data of each node.  If the user-defined function returns TRUE, then the
 * node is removed from the table and freed.  Otherwise, the node is left untouched.
 */
void
hashTableForEachDo(J9HashTable *table, J9HashTableDoFn doFn, void *opaque)
{
	J9HashTableState walkState;
	void *node;
	HASHTABLE_DEBUG_PORT(table->portLibrary);

	hashTable_printf("hashTableForEachDo <%s>: table=%p\n", table->tableName, table);

	if (NULL == table->listNodePool) {
		/* space optimized hashTable, operation not supported */
		Assert_hashTable_unreachable();
	}

	node = hashTableStartDo(table,  &walkState);
	while (NULL != node) {
		if (doFn(node, opaque)) {
			hashTableDoRemove(&walkState);
		}
		node = hashTableNextDo(&walkState);
	}
}

/**
 * \brief       Rehashes an existing hash-table
 * \ingroup     hash_table
 *
 *
 * @param table
 *
 *      This routine may be used to re-evaluate the hash of already hashed
 *      entries.
 *
 */
void
hashTableRehash(J9HashTable *table)
{
	uint32_t i = 0;
	void *chain = NULL;
	void  *tail = NULL;
	uintptr_t tableSize = table->tableSize;

	if (NULL == table->listNodePool) {
		/* space optimized hashTable, operation not supported */
		Assert_hashTable_unreachable();
	}

	if (J9HASH_TABLE_COLLISION_RESILIENT == (table->flags & J9HASH_TABLE_COLLISION_RESILIENT)) {
		/* Not currently supported.
		 *
		 * Really complicated to implement as it cannot be done in-place as listNodes might become treeNodes (and vice-versa) and
		 * it's impossible to predict the required pool capacities.  One might expect this is a just a special case of collisionResilientHashTableGrow,
		 * however the problem is much harder as you cannot simply rebuild the original hash table on failure as the entries' hashcodes may
		 * have changed compared to the original.
		 */
		Assert_hashTable_unreachable();
	}

	/* connect all the node-chains into one big chain */
	for (i = 0; i < tableSize; i++) {
		if (table->nodes[i]) {
			if (NULL != chain) {
				/* walk to tail of chain */
				while (NEXT(tail)) {
					tail = NEXT(tail);
				}
				/* connect current chain to tail of other chains */
				NEXT(tail) = table->nodes[i];
			} else {
				chain = table->nodes[i];
				tail = chain;
			}
			table->nodes[i] = NULL;
		}
	}

	while (NULL != chain) {
		void *node = chain;
		uintptr_t hash = table->hashFn(node, table->hashFnUserData) % tableSize;
		/* advance to next in chain */
		chain = NEXT(chain);
		/* insert current node into node-chain at hash */
		NEXT(node) = table->nodes[hash];
		table->nodes[hash] = node;
	}
}

static uintptr_t
collisionResilientHashTableGrow(J9HashTable *table, uint32_t newSize)
{
	void **oldNodes = table->nodes;
	uintptr_t rc = 1;
	OMRPORT_ACCESS_FROM_OMRPORT(table->portLibrary);

	/* Need to ensure we have enough nodes to turn all tree nodes into list nodes */
	if (0 == pool_ensureCapacity(table->listNodePool, table->numberOfNodes)) {
		void **newNodes = (void **)table->portLibrary->mem_allocate_memory(table->portLibrary, newSize * sizeof(uintptr_t), table->tableName, table->memoryCategory);
		if (NULL != newNodes) {
			memset(newNodes, 0, newSize * sizeof(uintptr_t));
			rebuildFromPools(table, newSize, newNodes);
			/* Rebuild succeeded, so free the table's old nodes */
			omrmem_free_memory(oldNodes);
			rc = 0;
		}
	}
	return rc;
}

static void
rebuildFromPools(J9HashTable *table, uint32_t newSize, void **newNodes)
{
	uint32_t nodeCount = 0;
	uint32_t treeNodeCount = 0;
	pool_state state = {0};
	void *listNode = NULL;
	J9AVLTreeNode *treeNode = NULL;
	uintptr_t i = 0;

	/* re-add all entries into the newNodes as listNodes */
	listNode = pool_startDo(table->listNodePool, &state);
	while (NULL != listNode) {
		uintptr_t hash = table->hashFn(listNode, table->hashFnUserData) % newSize;
		NEXT(listNode) = newNodes[hash];
		newNodes[hash] = listNode;
		listNode = pool_nextDo(&state);
		nodeCount += 1;
	}

	treeNode = pool_startDo(table->treeNodePool, &state);
	while (NULL != treeNode) {
		uintptr_t hash = 0;
		void *newListNode = pool_newElement(table->listNodePool);
		/* Ensured capacity in collisionResilientHashTableGrow so this should suceed */
		HASHTABLE_ASSERT(newListNode);

		memcpy(newListNode, AVL_NODE_TO_DATA(treeNode), table->entrySize);
		hash = table->hashFn(newListNode, table->hashFnUserData) % newSize;
		NEXT(newListNode) = newNodes[hash];
		newNodes[hash] = newListNode;

		pool_removeElement(table->treeNodePool, treeNode);
		treeNode = pool_nextDo(&state);
		nodeCount += 1;
		treeNodeCount += 1;
	}
	/* Sanity checks to make sure the table has had correctly counted the right number of nodes */
	HASHTABLE_ASSERT(nodeCount == table->numberOfNodes);
	HASHTABLE_ASSERT(treeNodeCount == table->numberOfTreeNodes);

	/* Have to check if we should turn chains into trees.  Reset numberOfTreeNodes and clear all trees. ListToTree() will increment
	 * numberOfTreeNodes to the correct value as well as allocate new J9AVLTrees.
	 */
	pool_clear(table->treePool);
	table->numberOfTreeNodes = 0;
	for (i = 0; i < newSize; i++) {
		uintptr_t listLength = 0;
		void **head = &newNodes[i];
		listNode = *head;

		while (NULL != listNode) {
			listNode = NEXT(listNode);
			listLength += 1;
		}
		if (listLength > table->listToTreeThreshold) {
			/* This can fail, but that should be acceptable, we'll just have a list of length larger than listToTreeThreshold */
			listToTree(table, head, listLength);
		}
	}

	table->tableSize = newSize;
	table->nodes = newNodes;
}


/**
 * \brief       Begin an iteration over all nodes of a hash-table.
 * \ingroup     hash_table
 *
 *
 * @param table
 * @param handle used by hashTableNextDo to keep track of state
 * @return            NULL if  no more nodes; otherwise a the address of the node
 *
  *	Start of an iteration set that will return when code is to be executed.
 *	Pass in a pointer to an empty J9HashTableState and it will be filled in.
 */
void *
hashTableStartDo(J9HashTable *table,  J9HashTableState *handle)
{
	void *result = NULL;
	uint32_t numberOfListNodes = table->numberOfNodes - table->numberOfTreeNodes;
	HASHTABLE_DEBUG_PORT(table->portLibrary);

	memset(handle, 0, sizeof(J9HashTableState));
	handle->table = table;
	handle->bucketIndex = 0;
	handle->pointerToCurrentNode = &table->nodes[0];
	handle->didDeleteCurrentNode = FALSE;
	handle->iterateState = J9HASH_TABLE_ITERATE_STATE_LIST_NODES;

	if (NULL == table->listNodePool) {
		/* find the first non-empty bucket */
		while (handle->bucketIndex < table->tableSize) {
			void **node = &table->nodes[handle->bucketIndex];
			if (NULL != *node) {
				result = node;
				break;
			}
			handle->bucketIndex += 1;
		}
	} else {
		/* It would be convenient if we could iterate and perform doRemoves on avl trees, but currently that API is not supported, so we just
		 * have to iterate the treeNode pool
		 */
		if (numberOfListNodes > 0) {
			while ((handle->bucketIndex < table->tableSize)
				&& ((NULL == *handle->pointerToCurrentNode) || AVL_TREE_TAGGED(*handle->pointerToCurrentNode))
			) {
				handle->bucketIndex += 1;
				handle->pointerToCurrentNode = &table->nodes[handle->bucketIndex];
			}
			/* Had to have found a listNode in the table unless numberOfListNodes was incorrect */
			HASHTABLE_ASSERT(!(NULL == *handle->pointerToCurrentNode) || AVL_TREE_TAGGED(*handle->pointerToCurrentNode));
			result = *handle->pointerToCurrentNode;
			handle->iterateState = J9HASH_TABLE_ITERATE_STATE_LIST_NODES;
		} else if (table->numberOfTreeNodes > 0) {
			handle->pointerToCurrentNode = pool_startDo(table->treeNodePool, &handle->poolState);
			HASHTABLE_ASSERT(NULL != handle->pointerToCurrentNode);
			result = AVL_NODE_TO_DATA(handle->pointerToCurrentNode);
			handle->iterateState = J9HASH_TABLE_ITERATE_STATE_TREE_NODES;
		} else {
			result = NULL;
			handle->iterateState = J9HASH_TABLE_ITERATE_STATE_FINISHED;
		}
	}

	hashTable_printf("hashTableStartDo <%s>: handle=%p, table=%p, bucket=%u, node=%p\n", table->tableName, handle, table, handle->bucketIndex, result);

	return result;
}


/**
 * \brief       Continue an iteration over all nodes of a hash-table.
 * \ingroup     hash_table
 *
 *
 * @param lastHandle previously filled in by hashTableStartDo
 * @return                  NULL if  no more elements; otherwise a pointer to a node
 *
 * Continue an iteration set that will return when code is to be executed.
 */
void  *
hashTableNextDo(J9HashTableState *handle)
{
	J9HashTable *table = handle->table;
	void *result = NULL;
	HASHTABLE_DEBUG_PORT(table->portLibrary);

	if (NULL == table->listNodePool) {
		/* space optimized hashTable - advance to the next bucket */
		handle->bucketIndex += 1;
		while (handle->bucketIndex < table->tableSize) {
			void **node = &table->nodes[handle->bucketIndex];
			if (NULL != *node) {
				result = node;
				break;
			}
			handle->bucketIndex += 1;
		}
	} else {
		switch (handle->iterateState) {
		case J9HASH_TABLE_ITERATE_STATE_LIST_NODES:
			if (TRUE == handle->didDeleteCurrentNode) {
				/* if the current node was deleted, don't advance the pointer within the bucket, since it's already correct */
			} else {
				/* advance to the next element in the bucket */
				handle->pointerToCurrentNode = &NEXT(*(handle->pointerToCurrentNode));
			}
			handle->didDeleteCurrentNode = FALSE;

			while ((handle->bucketIndex < table->tableSize)
				&& ((NULL == *handle->pointerToCurrentNode) || AVL_TREE_TAGGED(*handle->pointerToCurrentNode))
			) {
				handle->bucketIndex += 1;
				handle->pointerToCurrentNode = &table->nodes[handle->bucketIndex];
			}
			if (handle->bucketIndex < table->tableSize) {
				result = *handle->pointerToCurrentNode;
			} else {
				if (table->numberOfTreeNodes > 0) {
					handle->pointerToCurrentNode = pool_startDo(table->treeNodePool, &handle->poolState);
					result = AVL_NODE_TO_DATA(handle->pointerToCurrentNode);
					/* iterate more tree nodes at next iteration */
					handle->iterateState = J9HASH_TABLE_ITERATE_STATE_TREE_NODES;
				} else {
					/* Nothing left to do, we're finished */
					result = NULL;
					handle->iterateState = J9HASH_TABLE_ITERATE_STATE_FINISHED;
				}
			}
			break;

		case J9HASH_TABLE_ITERATE_STATE_TREE_NODES:
			handle->pointerToCurrentNode = pool_nextDo(&handle->poolState);
			if (NULL != handle->pointerToCurrentNode) {
				result = AVL_NODE_TO_DATA(handle->pointerToCurrentNode);
			} else {
				/* Nothing left to do */
				result = NULL;
				handle->iterateState = J9HASH_TABLE_ITERATE_STATE_FINISHED;
			}
			break;

		case J9HASH_TABLE_ITERATE_STATE_FINISHED:
			result = NULL;
			break;

		default:
			Assert_hashTable_unreachable();
		}
	}

	hashTable_printf("hashTableNextDo <%s>: handle=%p, table=%p, bucket=%u, node=%p\n", table->tableName, handle, table, handle->bucketIndex, result);

	return result;
}


/**
 * \brief       Remove the current node in the hash-table iteration
 * \ingroup     hash_table
 *
 *
 * @param lastHandle previously filled in by hashTableStartDo
 * @return                  0 on success, 1 on failure
 *
 * NOTE: If you're using collisionResilientHashTable you MUST finish the iteration if you remove any elements
 */
uintptr_t
hashTableDoRemove(J9HashTableState *handle)
{
	J9HashTable *table = handle->table;
	uintptr_t rc = 1;
	HASHTABLE_DEBUG_PORT(table->portLibrary);

	/* operation not supported on a space optimized hashTable */
	if (NULL == table->listNodePool) {
		Assert_hashTable_unreachable();
	} else {
		void *currentNode = NULL;
		hashTable_printf("hashTableDoRemove <%s>: handle=%p, table=%p, bucket=%u, removedElement=%p\n", table->tableName, handle, table, handle->bucketIndex, currentNode);
		switch (handle->iterateState) {
		case J9HASH_TABLE_ITERATE_STATE_LIST_NODES:
			currentNode = *(handle->pointerToCurrentNode);

			*(handle->pointerToCurrentNode) = NEXT(currentNode);
			pool_removeElement(table->listNodePool, currentNode);
			handle->didDeleteCurrentNode = TRUE;
			table->numberOfNodes -= 1;
			rc = 0;
			break;

		case J9HASH_TABLE_ITERATE_STATE_TREE_NODES:
			rc = hashTableRemove(table, AVL_NODE_TO_DATA(handle->pointerToCurrentNode));
			Assert_hashTable_true(0 == rc);
			break;

		case J9HASH_TABLE_ITERATE_STATE_FINISHED:
			rc = 1;
			break;

		default:
			Assert_hashTable_unreachable();
		}
	}
	return rc;
}


/**
 * \brief       Return the number of entries in a hash-table
 * \ingroup     hash_table
 *
 *
 * @param table
 * @return                  Number of table entries
 *
 */
uint32_t
hashTableGetCount(J9HashTable *table)
{
	return table->numberOfNodes;
}

static uintptr_t
hashTableGrowListNodes(J9HashTable *table, uint32_t newSize)
{
	OMRPORT_ACCESS_FROM_OMRPORT(table->portLibrary);
	uintptr_t rc = 1;

	void **newNodes = table->portLibrary->mem_allocate_memory(table->portLibrary, sizeof(uintptr_t) * newSize, table->tableName, table->memoryCategory);
	if (NULL != newNodes) {
		uint32_t i = 0;
		uint32_t numberOfNodes = 0;

		/* reset all the nodes */
		memset(newNodes, 0, sizeof(uintptr_t) * newSize);

		for (i = 0; i < table->tableSize; i++) {
			void *node = table->nodes[i];
			while (NULL != node) {
				void *nextNode = NEXT(node);
				uintptr_t hash = table->hashFn(node, table->hashFnUserData) % newSize;
				NEXT(node) = newNodes[hash];
				newNodes[hash] = node;
				node = nextNode;
				numberOfNodes += 1;
			}
		}
		omrmem_free_memory(table->nodes);
		table->tableSize = newSize;
		table->nodes = newNodes;
		/* Sanity check to make sure that the old hash table had calculated the right number of nodes */
		HASHTABLE_ASSERT(numberOfNodes == table->numberOfNodes);
		rc = 0;
	}

	return rc;
}


static uintptr_t
hashTableGrowSpaceOpt(J9HashTable *table, uint32_t newSize)
{
	void **newNodes = NULL;
	uint32_t numberOfNodes = 0;
	uint32_t i = 0;
	uintptr_t hash = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(table->portLibrary);

	newNodes = table->portLibrary->mem_allocate_memory(table->portLibrary, sizeof(uintptr_t) * newSize, table->tableName, table->memoryCategory);
	if (NULL == newNodes) {
		goto error;
	}
	/* reset all the nodes */
	memset(newNodes, 0, sizeof(uintptr_t) * newSize);

	if (newSize > SPACE_OPT_LIMIT) {
		/* start using a pool */
		table->listNodePool = pool_new(table->listNodeSize, table->tableSize, table->nodeAlignment, POOL_NO_ZERO, table->tableName, table->memoryCategory, POOL_FOR_PORT(table->portLibrary));
		if (NULL == table->listNodePool) {
			goto error;
		}
		if (0 != pool_ensureCapacity(table->listNodePool, table->numberOfNodes)) {
			goto error;
		}

		for (i = 0; i < table->tableSize; i++) {
			if (NULL != table->nodes[i]) {
				/* Create a new node */
				void *newNode = pool_newElement(table->listNodePool);
				/* We ensured capacity so this couldn't happen */
				HASHTABLE_ASSERT(NULL != newNode);

				memcpy(newNode, &table->nodes[i], table->entrySize);
				hash = table->hashFn(newNode, table->hashFnUserData) % newSize;
				NEXT(newNode) = newNodes[hash];
				newNodes[hash] = newNode;
				numberOfNodes += 1;
			}
		}
	} else {
		for (i = 0; i < table->tableSize; i++) {
			if (NULL != table->nodes[i]) {
				uintptr_t index = 0;
				hash = table->hashFn(&table->nodes[i], table->hashFnUserData) % newSize;
				index = hash;

				while (NULL != newNodes[index]) {
					index += 1;
					if (index == newSize) {
						index = 0;
					}
				}
				newNodes[index] = table->nodes[i];
				numberOfNodes += 1;
			}
		}
	}

	omrmem_free_memory(table->nodes);
	table->tableSize = newSize;
	table->nodes = newNodes;
	/* Sanity check to make sure that the old hash table had calculated the right number of nodes */
	HASHTABLE_ASSERT(numberOfNodes == table->numberOfNodes);
	return 0;

error:
	if (NULL != table->listNodePool) {
		pool_kill(table->listNodePool);
	}
	if (NULL != newNodes) {
		omrmem_free_memory(newNodes);
	}
	table->listNodePool = NULL;
	return 1;
}


static uintptr_t
hashTableGrow(J9HashTable *table)
{
	uint32_t newSize = 0;
	uintptr_t rc = 1;

	/* get the new size to grow to. bail out (and dont grow) if we hit
	 * the hash table growth limit */
	newSize = hashTableNextSize(table->tableSize);

	if (0 != newSize) {
		if (NULL == table->listNodePool) {
			/* space optimized hashTable */
			rc = hashTableGrowSpaceOpt(table, newSize);
		} else {
			if (J9HASH_TABLE_COLLISION_RESILIENT == (table->flags & J9HASH_TABLE_COLLISION_RESILIENT)) {
				rc = collisionResilientHashTableGrow(table, newSize);
			} else {
				rc = hashTableGrowListNodes(table, newSize);
			}
		}
	}

	return rc;
}

static uint32_t
hashTableNextSize(uint32_t size)
{
	uint32_t i;

	for (i = 0; i < primesTableSize; i++) {
		if (primesTable[i] > size) {
			return primesTable[i];
		}
	}

	return 0;
}
