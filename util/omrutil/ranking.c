/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2010, 2015
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

#include "ranking.h"

static void bubbleUp(OMRRanking *ranking, uint32_t rank);
static void updateLowestNotFull(OMRRanking *ranking, void *key, uintptr_t count);

#define HASHTABLE_RATIO 2


struct hashTableEntry {
	uint32_t rank; /* used to point into rankTable */
	void *data;
};

struct rankTableEntry {
	uintptr_t count;
	hashTableEntry *hashTableEntry;
};

/*
 * Implemented using an array which has the nice property of rankings being laid out
 * in order in memory.  This allows access to the kth ranking in constant time.
 *
 * But can also implement using a heap.  Heap has advantage of log(n) bounded updates and
 * probably requires less operations than an array to update a data structure.
 *
 * In practice the difference is probably minor though.
 */

/* Convention we use is the at rankTable[ranking->size-1] is the element with the highest count and entires
 * with decreasing index have decreasing counts
 */

static uintptr_t
hashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	return ((hashTableEntry *)leftKey)->data == ((hashTableEntry *)rightKey)->data;
}

static uintptr_t
hashFn(void *key, void *userData)
{
	return (uintptr_t)((hashTableEntry *)key)->data;
}


void
rankingClear(OMRRanking *ranking)
{
	J9HashTableState state;
	hashTableEntry *entry;
	ranking->curSize = 0;

	/* todo: Find less ugly way of doing this other than walking through the whole structure
	 */
	entry = hashTableStartDo(ranking->hashTable, &state);
	while (entry != NULL) {
		hashTableDoRemove(&state);
		entry = hashTableNextDo(&state);
	}
}

/* todo: add user-defined hashfn*/
OMRRanking *
rankingNew(OMRPortLibrary *portLibrary, uint32_t size)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	OMRRanking *newRanking = omrmem_allocate_memory(sizeof(OMRRanking), OMRMEM_CATEGORY_MM);
	if (NULL == newRanking) {
		return NULL;
	}
	newRanking->size = size;
	newRanking->curSize = 0;
	newRanking->rankTable = omrmem_allocate_memory(size * sizeof(rankTableEntry), OMRMEM_CATEGORY_MM);
	if (NULL == newRanking->rankTable) {
		return NULL;
	}
	newRanking->hashTable = hashTableNew(portLibrary, OMR_GET_CALLSITE(), size * HASHTABLE_RATIO, sizeof(hashTableEntry), 0, J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, OMRMEM_CATEGORY_VM, hashFn, hashEqualFn, NULL, NULL);
	if (NULL == newRanking->hashTable) {
		return NULL;
	}
	newRanking->portLib = portLibrary;

	return newRanking;
}

void
rankingFree(OMRRanking *ranking)
{
	OMRPORT_ACCESS_FROM_OMRPORT(ranking->portLib);

	hashTableFree(ranking->hashTable);
	omrmem_free_memory(ranking->rankTable);
	omrmem_free_memory(ranking);

}


uintptr_t
rankingGetLowestCount(OMRRanking *ranking)
{
	if (ranking->curSize == 0) {
		return 0;
	} else {
		return ranking->rankTable[ranking->size - ranking->curSize].count;
	}
}

void
rankingUpdateLowest(OMRRanking *ranking, void *key, uintptr_t count)
{
	hashTableEntry newHashTableEntry;
	hashTableEntry *newHashTableEntryPtr;

	if (!(ranking->curSize < ranking->size)) {
		newHashTableEntry.data = key;
		newHashTableEntry.rank = 0;

		/* Remove entry at rank 0 in hash table*/
		hashTableRemove(ranking->hashTable, ranking->rankTable[0].hashTableEntry);

		/* add entry to the table with rank 0*/
		newHashTableEntryPtr = hashTableAdd(ranking->hashTable, &newHashTableEntry);

		/* Fix up rankingTable */
		ranking->rankTable[0].count = count;
		ranking->rankTable[0].hashTableEntry = newHashTableEntryPtr;

		bubbleUp(ranking, 0);
	} else {
		updateLowestNotFull(ranking, key, count);
	}
}

static void
bubbleUp(OMRRanking *ranking, uint32_t rank)
{
	hashTableEntry *currentHashTableEntry;
	hashTableEntry *nextHashTableEntry;
	rankTableEntry tmpRankTableEntry;

	while ((rank != ranking->size - 1) && (ranking->rankTable[rank].count > ranking->rankTable[rank + 1].count)) {
		/* Update ranks in the hash table */
		currentHashTableEntry = ranking->rankTable[rank].hashTableEntry;
		nextHashTableEntry = ranking->rankTable[rank + 1].hashTableEntry;
		currentHashTableEntry->rank++;
		nextHashTableEntry->rank--;

		/* Swap rankEntries */
		tmpRankTableEntry = ranking->rankTable[rank + 1];
		ranking->rankTable[rank + 1] = ranking->rankTable[rank];
		ranking->rankTable[rank] = tmpRankTableEntry;
		rank++;
	}
}

uintptr_t
rankingIncrementEntry(OMRRanking *ranking, void *key, uintptr_t count)
{
	/* Find */
	hashTableEntry hashTableEntryToFind;
	hashTableEntry *foundHashTableEntryPtr;
	hashTableEntryToFind.data = key;

	foundHashTableEntryPtr = hashTableFind(ranking->hashTable, &hashTableEntryToFind);
	if (foundHashTableEntryPtr != NULL) { /* found something in table */
		/*Increment count and bubbleup*/
		uint32_t rank = foundHashTableEntryPtr->rank;
		ranking->rankTable[rank].count += count;
		bubbleUp(ranking, rank);
		return TRUE;
	} else {
		return FALSE;
	}
}

static void
updateLowestNotFull(OMRRanking *ranking, void *key, uintptr_t count)
{
	uint32_t lowestRank = (ranking->size - ranking->curSize) - 1;

	/* just fill up the data structure */
	hashTableEntry newHashTableEntry;
	hashTableEntry *newHashTableEntryPtr;

	/* add it to hash table*/
	newHashTableEntry.data = key;
	newHashTableEntry.rank = lowestRank;
	newHashTableEntryPtr = hashTableAdd(ranking->hashTable, &newHashTableEntry);

	/* update ranking table */
	ranking->rankTable[lowestRank].count = count;
	ranking->rankTable[lowestRank].hashTableEntry = newHashTableEntryPtr;
	ranking->curSize++;
	bubbleUp(ranking, lowestRank);

}

void *
rankingGetKthHighest(OMRRanking *ranking, uintptr_t k)
{
	if (k <= ranking->curSize) {
		return ranking->rankTable[ranking->size - k].hashTableEntry->data;
	} else {
		return NULL;
	}

}

uintptr_t
rankingGetKthHighestCount(OMRRanking *ranking, uintptr_t k)
{
	if (k <= ranking->curSize) {
		return ranking->rankTable[ranking->size - k].count;
	} else {
		return 0;
	}
}

