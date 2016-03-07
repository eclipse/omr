/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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
#if !defined(RANKING_H_)
#define RANKING_H_
#include "hashtable_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rankTableEntry rankTableEntry;
typedef struct OMRRanking OMRRanking;
typedef struct hashTableEntry hashTableEntry;

struct OMRRanking {
	uint32_t size;
	uint32_t curSize;
	rankTableEntry *rankTable;
	OMRPortLibrary *portLib;
	J9HashTable *hashTable;
};

/*
 * Create new ranking data structure which organizes keys based on some count value.  The keys
 * are organized by some sorting based on their rank in such a way that we can access that key's count
 * in O(1) time.   Updates of count values are also quick (increments of count values should not require
 * much time).
 *
 * TODO: rewrite so we can accept something other than pointer values as keys
 *
 * @param portLibrary the port library
 * @param size number of entries to have in the ranking table
 * @return pointer to new ranking data structure
 */
OMRRanking *rankingNew(OMRPortLibrary *portLibrary, uint32_t size);
void rankingFree(OMRRanking *ranking);

/* resets all entries in the ranking data structure */
void rankingClear(OMRRanking *ranking);

/* Get the lowest count value in the data structure*/
uintptr_t rankingGetLowestCount(OMRRanking *ranking);

/* Replace the entry with the lowest count value with the given key and count value
 * @param key to replace lowest entry with
 * @param count count to replace lowest entry wtih
 */
void rankingUpdateLowest(OMRRanking *ranking, void *key, uintptr_t count);

/* Increment an entry by count
 * @param key the entry to increment
 * @param count how much to increment by
 * @return returns TRUE if an entry with the given key exists(who's count is then increment), false otherwise
 * */
uintptr_t rankingIncrementEntry(OMRRanking *ranking, void *key, uintptr_t count);

/* get the key at rank k
 * @param k the kth highest count we're enquiring about
 * @return the key at rank k
 */
void *rankingGetKthHighest(OMRRanking *ranking, uintptr_t k);

/* get the count at rank k
 * @param k the kth highest count we're enquiring about
 * @return the count at rank k
 */
uintptr_t rankingGetKthHighestCount(OMRRanking *ranking, uintptr_t k);

#ifdef __cplusplus
}
#endif

#endif
