/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2009, 2016
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


#include <stdlib.h>
#include <string.h>
#include "avl_api.h"
#include "hashtable_api.h"
#include "omrport.h"
/*
 * Testing the following functions of J9HashTable using the J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION flag:
 * 		hashTableAdd()
 * 		hashTableGetCount()
 * 		hashTableFind()
 * 		hashTableStartDo()
 * 		hashTableNextDo()
 * 		hashTableRemove()
 */

static uintptr_t hashFn(void *key, void *userData);
static uintptr_t hashEqualFn(void *leftKey, void *rightKey, void *userData);
static uintptr_t dataOffset(uintptr_t startOffset, uintptr_t dataLength, uintptr_t i);
static BOOLEAN checkIntegrity(OMRPortLibrary *portLib, char *id, J9HashTable *table, uintptr_t *data, uintptr_t dataLength, uintptr_t removeOffset, uintptr_t i);
static BOOLEAN runTests(OMRPortLibrary *portLib, char *id, J9HashTable *table, uintptr_t *data, uintptr_t dataLength, uintptr_t reverseRemove);
static void testHashtable(OMRPortLibrary *portLib, char *id, uintptr_t *data, uintptr_t dataLength, uintptr_t *passCount, uintptr_t *failCount, BOOLEAN forceCollisions);
static void testCollisionResilientHashTable(OMRPortLibrary *portLib, char *id, uintptr_t *data, uintptr_t dataLength, uintptr_t *passCount, uintptr_t *failCount, BOOLEAN forceCollisions, uint32_t listToTreeThreshold);
static void printRandomData(OMRPortLibrary *portLib, uintptr_t *randData, uintptr_t randSize);

extern const uint8_t RandomValues[256];

#define FORWARD 0
#define REVERSE -1

static uintptr_t
hashEqualFn(void *leftKey, void *rightKey, void *userData)
{
	return *(uintptr_t *)leftKey == *(uintptr_t *)rightKey;
}

static uintptr_t
hashFn(void *key, void *userData)
{
	BOOLEAN forceCollisions = (BOOLEAN)((uintptr_t)userData);
	if (forceCollisions) {
		return (*(uintptr_t *)key) & 0x1;
	} else {
		return *(uintptr_t *)key;
	}
}



static intptr_t
hashComparatorFn(struct J9AVLTree *tree, struct J9AVLTreeNode *leftNode, struct J9AVLTreeNode *rightNode)
{
	uintptr_t leftKey = *((uintptr_t *)(leftNode + 1));
	uintptr_t rightKey = *((uintptr_t *)(rightNode + 1));

	return (intptr_t)leftKey - (intptr_t)rightKey;
}

static uintptr_t dataOffset(uintptr_t startOffset, uintptr_t dataLength, uintptr_t i)
{
	uintptr_t result;
	if (startOffset == REVERSE) {
		return dataLength - (i + 1);
	}
	result = startOffset + i;
	if (result >= dataLength) {
		result -= dataLength;
	}
	return result;
}

static BOOLEAN
checkIntegrity(OMRPortLibrary *portLib, char *id, J9HashTable *table, uintptr_t *data, uintptr_t dataLength, uintptr_t removeOffset, uintptr_t i)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	uintptr_t count, j;
	uintptr_t dup[256];
	J9HashTableState walkState;
	uintptr_t *next;

	count = hashTableGetCount(table);
	if (count != dataLength - (i + 1)) {
		omrtty_printf("Hashtable %s count failure: count %d expected %d\n", id, count, dataLength - (i + 1));
		return FALSE;
	}

	/* walk all the elements, checking for duplicates and ensuring the elements are valid */
	memset(dup, 0, sizeof(dup));
	count = 0;
	next = hashTableStartDo(table, &walkState);
	while (next != NULL) {
		uintptr_t entry;
		uintptr_t *node;
		BOOLEAN found = FALSE;
		count++;
		if (*next >= sizeof(dup) / sizeof(uintptr_t)) {
			omrtty_printf("Hashtable %s dup detection, values must be < %d\n", id, sizeof(dup) / sizeof(uintptr_t));
			return FALSE;
		}
		if (dup[*next]) {
			omrtty_printf("Hashtable %s walk duplicate failure\n", id);
			return FALSE;
		}
		dup[*next] = 1;
		for (j = i + 1; j < dataLength; j++) {
			if (*next == data[dataOffset(removeOffset, dataLength, j)]) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			omrtty_printf("Hashtable %s walk element failure\n", id);
			return FALSE;
		}

		entry = *next;
		node = hashTableFind(table, &entry);
		if (node == NULL || *node != entry) {
			if (node == NULL) {
				omrtty_printf("Hashtable %s walk find failure: search %d\n", id, entry);
			} else {
				omrtty_printf("Hashtable %s walk find failure: search %d found %d\n", id, entry, *node);
			}
			return FALSE;
		}

		next = hashTableNextDo(&walkState);
	}
	if (count != dataLength - (i + 1)) {
		omrtty_printf("Hashtable %s walk count failure\n", id);
		return FALSE;
	}

	for (j = i + 1; j < dataLength; j++) {
		uintptr_t *node;
		uintptr_t entry = data[dataOffset(removeOffset, dataLength, j)];
		node = hashTableFind(table, &entry);
		if (node == NULL || *node != entry) {
			if (node == NULL) {
				omrtty_printf("Hashtable %s find failure: search %d\n", id, entry);
			} else {
				omrtty_printf("Hashtable %s find failure: search %d found %d\n", id, entry, *node);
			}
			return FALSE;
		}
	}

	return TRUE;
}

static BOOLEAN
runTests(OMRPortLibrary *portLib, char *id, J9HashTable *table, uintptr_t *data, uintptr_t dataLength, uintptr_t removeOffset)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	uintptr_t i, entry;
	char *operation = "";
	char buf[64];

	/* add all the data elements */
	for (i = 0; i < dataLength; i++) {
		entry = data[i];
		if (hashTableAdd(table, &entry) == NULL) {
			/* Failure to allocate a new node */
			omrtty_printf("Hashtable %s add failure\n", id);
			return FALSE;
		}
	}

	/* ensure the count is correct */
	if (hashTableGetCount(table) != dataLength) {
		omrtty_printf("Hashtable %s count failure\n", id);
		return FALSE;
	}

	/* find all the elements */
	for (i = 0; i < dataLength; i++) {
		uintptr_t *node;
		entry = data[i];
		node = hashTableFind(table, &entry);
		if (node == NULL || *node != entry) {
			if (node == NULL) {
				omrtty_printf("Hashtable %s find failure: search %d\n", id, entry);
			} else {
				omrtty_printf("Hashtable %s find failure: search %d found %d\n", id, entry, *node);
			}
			return FALSE;
		}
	}

	if (checkIntegrity(portLib, id, table, data, dataLength, 0, -1) == FALSE) {
		return FALSE;
	}

	/* remove all elements verifying the integrity */
	if (removeOffset == REVERSE) {
		omrstr_printf(buf, sizeof(buf), "%s reverse remove", id);
		operation = buf;
	} else if (removeOffset > 0) {
		omrstr_printf(buf, sizeof(buf), "%s offset %d remove", id, removeOffset);
		operation = buf;
	}
	for (i = 0; i < dataLength; i++) {
		entry = data[dataOffset(removeOffset, dataLength, i)];
		if (hashTableRemove(table, &entry) != 0) {
			omrtty_printf("Hashtable %s failure\n", operation);
			return FALSE;
		}
		if (checkIntegrity(portLib, operation, table, data, dataLength, removeOffset, i) == FALSE) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
testHashtable(OMRPortLibrary *portLib, char *id, uintptr_t *data, uintptr_t dataLength, uintptr_t *passCount, uintptr_t *failCount, BOOLEAN forceCollisions)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	J9HashTable *table;
	uintptr_t i;

	if ((table = hashTableNew(portLib, "testTable", 17, sizeof(uintptr_t), sizeof(char *), J9HASH_TABLE_ALLOW_SIZE_OPTIMIZATION, OMRMEM_CATEGORY_VM, hashFn, hashEqualFn, NULL, (void *)(uintptr_t)forceCollisions)) == NULL) {
		omrtty_printf("Hashtable %s creation failure\n", id);
		goto fail;
	}

	if (runTests(portLib, id, table, data, dataLength, REVERSE) == FALSE) {
		goto fail;
	}
	for (i = 0; i < dataLength; i++) {
		if (runTests(portLib, id, table, data, dataLength, i) == FALSE) {
			goto fail;
		}
	}

	hashTableFree(table);
	(*passCount)++;
	return;

fail:
	(*failCount)++;
	hashTableFree(table);
}

static void
testCollisionResilientHashTable(OMRPortLibrary *portLib, char *id, uintptr_t *data, uintptr_t dataLength, uintptr_t *passCount, uintptr_t *failCount, BOOLEAN forceCollisions, uint32_t listToTreeThreshold)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	J9HashTable *table = NULL;

	uintptr_t i = 0;
	if ((table = collisionResilientHashTableNew(portLib, "collisionResilient testTable", 17, sizeof(uintptr_t), 0, OMRMEM_CATEGORY_VM, listToTreeThreshold, hashFn, hashComparatorFn, NULL, (void *)(uintptr_t)forceCollisions)) == NULL) {
		omrtty_printf("Hashtable %s creation failure\n", id);
		goto fail;
	}

	if (runTests(portLib, id, table, data, dataLength, REVERSE) == FALSE) {
		goto fail;
	}
	for (i = 0; i < dataLength; i++) {
		if (runTests(portLib, id, table, data, dataLength, i) == FALSE) {
			goto fail;
		}
	}

	hashTableFree(table);
	(*passCount)++;
	return;

fail:
	(*failCount)++;
	hashTableFree(table);
}

static void
printRandomData(OMRPortLibrary *portLib, uintptr_t *randData, uintptr_t randSize)
{
	uintptr_t i = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	omrtty_printf("random data:\n");
	for (i = 0; i < randSize; i++) {
		omrtty_printf("%03d ", randData[i]);
		if ((i + 1) % 16 == 0) {
			omrtty_printf("\n");
		}
	}
	omrtty_printf("\n");
}

int32_t
verifyHashtable(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	int32_t rc = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	uintptr_t start, end;
	U_64 testSetStart = 0;
	U_64 testSetEnd = 0;
	U_64 testDelta = 0;
	uintptr_t randSize, i, orgFail;
	uintptr_t data1[] = {1, 2, 3, 4, 5, 6, 7, 17, 18, 19, 20, 21, 22, 23, 24, 25};
	uintptr_t data2[] = {1, 3, 5, 7, 18, 20, 22, 23, 24, 35, 36, 37};
	uintptr_t data3[] = {25, 24, 23, 22, 21, 20, 19, 18, 17, 7, 6, 5, 4, 3, 2, 1};
	uintptr_t data4[] = {37, 36, 35, 24, 23, 22, 20, 18, 7, 5, 3, 1};
	uintptr_t data5[] = {17, 14, 8, 73, 23, 38, 91, 84, 31, 25, 11, 41, 26, 87, 35, 6};
	uintptr_t data6[] = {17, 14, 8, 73, 23, 38, 91, 84, 31, 25, 11, 41, 26, 87, 35, 6, 13, 61, 10, 20, 92};
	uintptr_t randData[256];
	uint32_t listToTreeThresholdIndex = 0;
	uint32_t listToTreeThresholdValues[] = {0, 1, 5, 10, 100};

	omrtty_printf("Testing hashtable functions...\n");

	start = portLib->time_usec_clock(portLib);

	testSetStart = omrtime_hires_clock();
	testHashtable(portLib, "Standard NoForce test1", data1, sizeof(data1) / sizeof(uintptr_t), passCount, failCount, FALSE);
	testHashtable(portLib, "Standard NoForce test2", data2, sizeof(data2) / sizeof(uintptr_t), passCount, failCount, FALSE);
	testHashtable(portLib, "Standard NoForce test3", data3, sizeof(data3) / sizeof(uintptr_t), passCount, failCount, FALSE);
	testHashtable(portLib, "Standard NoForce test4", data4, sizeof(data4) / sizeof(uintptr_t), passCount, failCount, FALSE);
	testHashtable(portLib, "Standard NoForce test5", data5, sizeof(data5) / sizeof(uintptr_t), passCount, failCount, FALSE);
	testHashtable(portLib, "Standard NoForce test6", data6, sizeof(data6) / sizeof(uintptr_t), passCount, failCount, FALSE);
	testSetEnd = omrtime_hires_clock();
	testDelta = omrtime_hires_delta(testSetStart, testSetEnd, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	omrtty_printf("Standard NoForce data tests: Elapsed Time=%llu.%03.3llums \n", testDelta / 1000, testDelta % 1000);

	testSetStart = omrtime_hires_clock();
	testHashtable(portLib, "Standard Force test1", data1, sizeof(data1) / sizeof(uintptr_t), passCount, failCount, TRUE);
	testHashtable(portLib, "Standard Force test2", data2, sizeof(data2) / sizeof(uintptr_t), passCount, failCount, TRUE);
	testHashtable(portLib, "Standard Force test3", data3, sizeof(data3) / sizeof(uintptr_t), passCount, failCount, TRUE);
	testHashtable(portLib, "Standard Force test4", data4, sizeof(data4) / sizeof(uintptr_t), passCount, failCount, TRUE);
	testHashtable(portLib, "Standard Force test5", data5, sizeof(data5) / sizeof(uintptr_t), passCount, failCount, TRUE);
	testHashtable(portLib, "Standard Force test6", data6, sizeof(data6) / sizeof(uintptr_t), passCount, failCount, TRUE);
	testSetEnd = omrtime_hires_clock();
	testDelta = omrtime_hires_delta(testSetStart, testSetEnd, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	omrtty_printf("Standard Force data tests: Elapsed Time=%llu.%03.3llums \n", testDelta / 1000, testDelta % 1000);


	for (listToTreeThresholdIndex = 0; listToTreeThresholdIndex < sizeof(listToTreeThresholdValues) / sizeof(uint32_t); listToTreeThresholdIndex++) {
		uint32_t listToTreeThreshold = listToTreeThresholdValues[listToTreeThresholdIndex];
		testSetStart = omrtime_hires_clock();
		testCollisionResilientHashTable(portLib, "CollisionResilient NoForce test1", data1, sizeof(data1) / sizeof(uintptr_t), passCount, failCount, FALSE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient NoForce test2", data2, sizeof(data2) / sizeof(uintptr_t), passCount, failCount, FALSE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient NoForce test3", data3, sizeof(data3) / sizeof(uintptr_t), passCount, failCount, FALSE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient NoForce test4", data4, sizeof(data4) / sizeof(uintptr_t), passCount, failCount, FALSE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient NoForce test5", data5, sizeof(data5) / sizeof(uintptr_t), passCount, failCount, FALSE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient NoForce test6", data6, sizeof(data6) / sizeof(uintptr_t), passCount, failCount, FALSE, listToTreeThreshold);
		testSetEnd = omrtime_hires_clock();
		testDelta = omrtime_hires_delta(testSetStart, testSetEnd, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
		omrtty_printf("CollisionResilient NoForce data tests: listToThreshold=%zu, Elapsed Time=%llu.%03.3llums \n", (uintptr_t)listToTreeThreshold, testDelta / 1000, testDelta % 1000);
	}

	for (listToTreeThresholdIndex = 0; listToTreeThresholdIndex < sizeof(listToTreeThresholdValues) / sizeof(uint32_t); listToTreeThresholdIndex++) {
		uint32_t listToTreeThreshold = listToTreeThresholdValues[listToTreeThresholdIndex];
		testSetStart = omrtime_hires_clock();
		testCollisionResilientHashTable(portLib, "CollisionResilient Force test1", data1, sizeof(data1) / sizeof(uintptr_t), passCount, failCount, TRUE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient Force test2", data2, sizeof(data2) / sizeof(uintptr_t), passCount, failCount, TRUE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient Force test3", data3, sizeof(data3) / sizeof(uintptr_t), passCount, failCount, TRUE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient Force test4", data4, sizeof(data4) / sizeof(uintptr_t), passCount, failCount, TRUE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient Force test5", data5, sizeof(data5) / sizeof(uintptr_t), passCount, failCount, TRUE, listToTreeThreshold);
		testCollisionResilientHashTable(portLib, "CollisionResilient Force test6", data6, sizeof(data6) / sizeof(uintptr_t), passCount, failCount, TRUE, listToTreeThreshold);
		testSetEnd = omrtime_hires_clock();
		testDelta = omrtime_hires_delta(testSetStart, testSetEnd, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
		omrtty_printf("CollisionResilient Force data tests: listToThreshold=%zu, Elapsed Time=%llu.%03.3llums \n", (uintptr_t)listToTreeThreshold, testDelta / 1000, testDelta % 1000);
	}

	for (i = 0; i < sizeof(RandomValues); i++) {
		uintptr_t j;
		uintptr_t offset = i;
		char name[256];

		if (RandomValues[i] == 0) {
			continue;
		}
		randSize = RandomValues[offset++] % 38;
		for (j = 0; j < randSize; j++) {
			if (offset == 256) {
				offset = 0;
			}
			randData[j] = RandomValues[offset++];
			if (randData[j] == 0) {
				if (offset == 256) {
					offset = 0;
				}
				randData[j] = RandomValues[offset++];
			}
		}
		orgFail = *failCount;
		omrstr_printf(name, sizeof(name), "HashTable randomData%d", i);
		testHashtable(portLib, name, randData, randSize, passCount, failCount, FALSE);
		if (orgFail != *failCount) {
			printRandomData(portLib, randData, randSize);
		}

		for (listToTreeThresholdIndex = 0; listToTreeThresholdIndex < sizeof(listToTreeThresholdValues) / sizeof(uint32_t); listToTreeThresholdIndex++) {
			uint32_t listToTreeThreshold = listToTreeThresholdValues[listToTreeThresholdIndex];
			orgFail = *failCount;
			omrstr_printf(name, sizeof(name), "CollisionResilientHashTable NoForce randomData%d", i);
			testCollisionResilientHashTable(portLib, name, randData, randSize, passCount, failCount, FALSE, listToTreeThreshold);
			if (orgFail != *failCount) {
				printRandomData(portLib, randData, randSize);
			}
		}

		for (listToTreeThresholdIndex = 0; listToTreeThresholdIndex < sizeof(listToTreeThresholdValues) / sizeof(uint32_t); listToTreeThresholdIndex++) {
			uint32_t listToTreeThreshold = listToTreeThresholdValues[listToTreeThresholdIndex];
			orgFail = *failCount;
			omrstr_printf(name, sizeof(name), "CollisionResilientHashTable Force randomData%d", i);
			testCollisionResilientHashTable(portLib, name, randData, randSize, passCount, failCount, TRUE, listToTreeThreshold);
			if (orgFail != *failCount) {
				printRandomData(portLib, randData, randSize);
			}
		}
	}

	end = portLib->time_usec_clock(portLib);
	omrtty_printf("Finished testing hashtable functions.\n");
	omrtty_printf("Hashtable functions execution time was %d (usec).\n", (end - start));

	return rc;
}
