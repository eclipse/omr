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


#include <string.h>
#include "omrport.h"
#include "pool_api.h"

#define ROUND_TO(granularity, number) ( (((number) % (granularity)) ? ((number) + (granularity) - ((number) % (granularity))) : (number)))

typedef struct J9NewPoolInputData {
	uint32_t structSize;
	uint32_t numberElements;
	uint32_t elementAlignment;
	uint32_t padding;
	uintptr_t poolFlags;
} J9NewPoolInputData;

typedef struct J9NewPoolResultData {
	J9Pool *poolResult;
	uint32_t poolAllocSize;
	uint32_t puddleAllocSize;
	uint32_t elementsAdded;
	uint32_t padding;
	void *lastFreeAddress;
} J9NewPoolResultData;

typedef struct J9PoolUserData {
	J9NewPoolInputData *inputData;
	J9NewPoolResultData *resultData;
	OMRPortLibrary *portLib;
	uintptr_t *passCount;
	uintptr_t *failCount;
} J9PoolUserData;

/* Table of parameters for pool_new, which will be used to test the various pool API functions. */
static J9NewPoolInputData INPUTDATA[] = {
	{1,		1,		sizeof(uintptr_t),		0,		0},					/* very small pool */
	{4,		0,		sizeof(uintptr_t),		0,		0},					/* page size pool - with 4-byte elements */
	{4,		10,		sizeof(uintptr_t),		0,		0},					/* small pool */
	{4,		10,		0,					0,		0},					/* small pool - default alignment size */
	{4,		10,		8,					0,		0},					/* small pool - 8 alignment size */
	{4,		10,		64,					0,		0},					/* small pool - large alignment size */
	{8,		0,		sizeof(uintptr_t),		0,		0},					/* page size pool - with 8-byte elements */
	{8,		100,	sizeof(uintptr_t),		0,		0},					/* medium pool */
	{8,		100,	0,					0,		0},					/* medium pool - default alignment size */
	{8,		100,	8,					0,		0},					/* medium pool - 8 alignment size */
	{8,		100,	64,					0,		0},					/* medium pool - large alignment size */
	{16,	0,		sizeof(uintptr_t),		0,		0},					/* page size pool - with 16-byte elements */
	{16,	256,	sizeof(uintptr_t),		0,		0},					/* larger pool */
	{16,	256,	0,					0,		0},					/* larger pool - default alignment size */
	{16,	256,	8,					0,		0},					/* larger pool - 8 alignment size */
	{16,	256,	64,					0,		0},					/* larger pool - large alignment size */
	{9999,	5,		sizeof(uintptr_t),		0,		0},					/* large struct, small number */
	{4,		9999,	sizeof(uintptr_t),		0,		0},					/* small struct, large number */
	{64,	0,		sizeof(uintptr_t),		0,		0},					/* zero minElements - should default to 1 */
	{64,	10,		0,					0,		0},					/* zero alignment - should default to MIN_GRANULARITY */
	{32,	10,		sizeof(uintptr_t),		0,		POOL_NO_ZERO},					/* POOL_NO_ZERO flag */
	{32,	10,		sizeof(uintptr_t),		0,		POOL_ALWAYS_KEEP_SORTED},		/* POOL_ALWAYS_KEEP_SORTED flag */
	{32,	10,		sizeof(uintptr_t),		0,		POOL_ROUND_TO_PAGE_SIZE},		/* POOL_ROUND_TO_PAGE_SIZE flag */
	{32,	10,		sizeof(uintptr_t),		0,		POOL_NEVER_FREE_PUDDLES}		/* POOL_NEVER_FREE_PUDDLES flag */
};

#define NUM_POOLS sizeof(INPUTDATA)/sizeof(INPUTDATA[0])

/* Results data for the pool tests. */
static J9NewPoolResultData RESULTDATA[NUM_POOLS] = {{0}};

/* User data to be passed into the pool's alloc and free functions. */
static J9PoolUserData POOLUSERDATA[NUM_POOLS] = {{0}};

/* Puddle list that will be shared by multiple pools in testPuddleListSharing(). */
static J9PoolPuddleList *sharedPuddleList = NULL;
static int32_t sharedPuddleListReferenceCount = 0;

#define NUM_POOLS_TO_SHARE_PUDDLE_LIST 16

#define FIRST_BYTE_MARKER 1
#define BYTE_MARKER 2
#define LAST_BYTE_MARKER 4

static intptr_t createNewPools(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);
static void testNewElement(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);
static void testWalkFunctions(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);
static void testRemoveElement(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);
static void testKill(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);
static void testClear(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);
static void testPuddleListSharing(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount);

static void *customAlloc(J9PoolUserData *userData, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit);
static void customFree(J9PoolUserData *userData, void *address, uint32_t type);
static void *sharedPuddleListAlloc(OMRPortLibrary *portLib, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit);
static void sharedPuddleListFree(OMRPortLibrary *portLib, void *address, uint32_t type);

int32_t
verifyPools(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	int32_t rc = 0;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	uintptr_t start, end;

	omrtty_printf("Testing pool functions using %d test pools...\n", NUM_POOLS);

	start = omrtime_usec_clock();
	if (createNewPools(portLib, passCount, failCount)) {
		testNewElement(portLib, passCount, failCount);
		testWalkFunctions(portLib, passCount, failCount);
		/* Clear the pools, and test new, walk, and remove again. */
		testClear(portLib, passCount, failCount);
		testNewElement(portLib, passCount, failCount);
		testWalkFunctions(portLib, passCount, failCount);
		testRemoveElement(portLib, passCount, failCount);
		testKill(portLib, passCount, failCount);
	}
	/* Test kill on full pools. */
	if (createNewPools(portLib, passCount, failCount)) {
		testNewElement(portLib, passCount, failCount);
		testKill(portLib, passCount, failCount);
	}
	testPuddleListSharing(portLib, passCount, failCount);
	end = omrtime_usec_clock();

	omrtty_printf("Finished testing pool functions.\n");
	omrtty_printf("Pool functions execution time was %d (usec).\n", (end - start));

	return rc;
}

/* Allocates memory */
static void *
customAlloc(J9PoolUserData *userData, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit)
{
	J9NewPoolResultData *resultData = userData->resultData;
	OMRPORT_ACCESS_FROM_OMRPORT(userData->portLib);

	if (type == POOL_ALLOC_TYPE_POOL && resultData->poolAllocSize == 0) {
		resultData->poolAllocSize = size;
	} else if (resultData->poolAllocSize == 0) {
		omrtty_printf("Error: Alloc called with type!=POOL_ALLOC_TYPE_POOL but poolAllocSize==0\n");
	} else if (type == POOL_ALLOC_TYPE_PUDDLE && resultData->puddleAllocSize == 0) {
		resultData->puddleAllocSize = size;
	}

	return omrmem_allocate_memory(size, OMRMEM_CATEGORY_VM);
}

/* Frees memory */
static void
customFree(J9PoolUserData *userData, void *address, uint32_t type)
{
	J9NewPoolResultData *resultData = userData->resultData;
	OMRPORT_ACCESS_FROM_OMRPORT(userData->portLib);

	if (type == POOL_ALLOC_TYPE_PUDDLE && userData->inputData->poolFlags & POOL_NEVER_FREE_PUDDLES) {
		omrtty_printf("Error: Pool should not have attempted to free puddle\n");
		(*userData->failCount)++;
	}
	resultData->lastFreeAddress = address;
	omrmem_free_memory(address);
}

static void *
sharedPuddleListAlloc(OMRPortLibrary *portLib, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit)
{
	if (type == POOL_ALLOC_TYPE_PUDDLE_LIST) {
		*doInit = 0;
		if (sharedPuddleList == NULL) {
			sharedPuddleList = portLib->mem_allocate_memory(portLib, size, callSite, OMRMEM_CATEGORY_VM);
			*doInit = 1;
		}
		if (sharedPuddleList != NULL) {
			sharedPuddleListReferenceCount++;
		}
		return sharedPuddleList;
	} else {
		return portLib->mem_allocate_memory(portLib, size, callSite, OMRMEM_CATEGORY_VM);
	}
}

static void
sharedPuddleListFree(OMRPortLibrary *portLib, void *address, uint32_t type)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	if (type == POOL_ALLOC_TYPE_PUDDLE_LIST) {
		sharedPuddleListReferenceCount--;
		if (sharedPuddleListReferenceCount == 0) {
			omrmem_free_memory(address);
			sharedPuddleList = NULL;
		}
	} else if (type == POOL_ALLOC_TYPE_PUDDLE) {
		if (sharedPuddleListReferenceCount == 1) {
			omrmem_free_memory(address);
		}
	} else {
		omrmem_free_memory(address);
	}
}

/* Create a bunch of new pools */
static intptr_t
createNewPools(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	intptr_t poolCntr;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9PoolUserData *userData = &POOLUSERDATA[poolCntr];
		J9NewPoolInputData *inputData = &INPUTDATA[poolCntr];
		J9NewPoolResultData *resultData = &RESULTDATA[poolCntr];
		uint32_t expectedNumElems = (inputData->numberElements == 0) ? 1 : inputData->numberElements;
		uint32_t expectedAlignment = (inputData->elementAlignment == 0) ? MIN_GRANULARITY : inputData->elementAlignment;

		userData->inputData = inputData;
		userData->passCount = passCount;
		userData->failCount = failCount;
		userData->portLib = portLib;
		userData->resultData = resultData;

		resultData->poolResult = pool_new(
									 inputData->structSize,
									 inputData->numberElements,
									 inputData->elementAlignment,
									 inputData->poolFlags,
									 OMR_GET_CALLSITE(),
									 OMRMEM_CATEGORY_VM,
									 (omrmemAlloc_fptr_t)customAlloc,
									 (omrmemFree_fptr_t)customFree,
									 userData);

		/* Check that creation succeeded */
		if (resultData->poolResult == NULL) {
			omrtty_printf("Error: pool_new returned NULL for pool test %d\n", poolCntr);
			(*failCount)++;
			return 0;
		}

		/* Check that sizes are all adequate */
		if (resultData->puddleAllocSize <
			((expectedNumElems * ROUND_TO(expectedAlignment, inputData->structSize)) +
			 ROUND_TO(expectedAlignment, sizeof(J9PoolPuddle)))
		) {
			omrtty_printf("Error: Puddle size too small for pool test %d\n", poolCntr);
			(*failCount)++;
		} else if ((inputData->poolFlags & POOL_ROUND_TO_PAGE_SIZE) &&
				   (resultData->puddleAllocSize < OS_PAGE_SIZE)) {
			omrtty_printf("Error: Allocation size did not respect POOL_ROUND_TO_PAGE_SIZE flag for pool test %d\n", poolCntr);
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
	return 1;
}

/* Call pool_newElement on a bunch of pools and try to use the data returned */
static void
testNewElement(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	uint32_t poolCntr, j;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9NewPoolInputData *inputData = &INPUTDATA[poolCntr];
		J9NewPoolResultData *resultData = &RESULTDATA[poolCntr];
		J9Pool *currentPool = resultData->poolResult;
		uint32_t puddleAllocSize = resultData->puddleAllocSize;
		void *element;
		uint8_t *walkBytes;
		uint32_t elementsInPuddle = 0;
		uintptr_t prevPuddleSize = 0;
		uint32_t expectedNumElems = (inputData->numberElements == 0) ? 1 : inputData->numberElements;
		uint32_t expectedAlignment = (inputData->elementAlignment == 0) ? MIN_GRANULARITY : inputData->elementAlignment;

		/* Call pool_newElement until a new puddle is allocated... and then allocate a couple of new elements into the new puddle */
		resultData->puddleAllocSize = 0;
		while (prevPuddleSize == 0) {

			/* Get a single element and check that it's the right size and that each byte can be written to */
			prevPuddleSize = resultData->puddleAllocSize;		/* puddleAllocSize will be updated when the next puddle is allocated */
			element = pool_newElement(currentPool);
			resultData->elementsAdded++;
			if (!element) {
				omrtty_printf("Error: pool_newElement return NULL for pool test %d\n", poolCntr);
				(*failCount)++;
				return;
			}

			/* Verify element alignment. Aligned elements are expected for use in AVL trees, for example. */
			if (((uintptr_t) element % expectedAlignment) != 0) {
				omrtty_printf("Error: pool_newElement returned 0x%p, which is not aligned to %d "
							  "for pool test %d\n", element, expectedAlignment, poolCntr);
				(*failCount)++;
			}

			if (inputData->poolFlags & POOL_NO_ZERO) {
				memset(element, 0, inputData->structSize);		/* Expect a crash if the element is too small */
			}

			/* Walk each byte of the returned element */
			walkBytes = (uint8_t *)element;
			for (j = 0; j < inputData->structSize; j++) {
				if (walkBytes[j] != 0) {
					omrtty_printf("Error: pool_newElement non-zero byte at offset %d in pool test %d\n", j, poolCntr);
					(*failCount)++;
				}
				/* Test that we can write to each byte */
				if (j == 0) {
					walkBytes[j] = FIRST_BYTE_MARKER;
				} else {
					walkBytes[j] = BYTE_MARKER;
				}
			}
			if (inputData->structSize > 1) {
				walkBytes[inputData->structSize - 1] = LAST_BYTE_MARKER;
			}
			if (resultData->puddleAllocSize == 0) {
				elementsInPuddle++;		/* Only increment while we're still in the first puddle */
			}
		}
		if (resultData->puddleAllocSize != puddleAllocSize) {
			omrtty_printf("Error: New puddle allocated is not the same size as the previous puddle in pool test %d\n", poolCntr);
			(*failCount)++;
		} else if (elementsInPuddle < expectedNumElems) {
			omrtty_printf("Error: Pool initialized with minElements=%d but only %d elements were allocated in puddle, in pool test %d\n", expectedNumElems, elementsInPuddle, poolCntr);
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
}

/* Walk the pools and ensure that they've been walked completely */
static void
testWalkFunctions(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	intptr_t poolCntr;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9NewPoolInputData *inputData = &INPUTDATA[poolCntr];
		J9NewPoolResultData *resultData = &RESULTDATA[poolCntr];
		J9Pool *currentPool = resultData->poolResult;
		pool_state state;
		uint8_t *element;
		uintptr_t numFailed = 0;
		uint32_t walkCount = 0;

		element = (uint8_t *)pool_startDo(currentPool, &state);
		do {
			if (element[0] != FIRST_BYTE_MARKER) {
				omrtty_printf("Error: First byte of element is corrupted in pool test %d\n", poolCntr);
				numFailed++;
			}
			if ((inputData->structSize > 1) && (element[inputData->structSize - 1] != LAST_BYTE_MARKER)) {
				omrtty_printf("Error: Last byte of element is corrupted in pool test %d\n", poolCntr);
				numFailed++;
			}
			if (!pool_includesElement(currentPool, element)) {
				omrtty_printf("Error: pool_includesElement returned false for walked element in pool test %d\n", poolCntr);
				numFailed++;
			}
			walkCount++;
			element = pool_nextDo(&state);
		} while (element != NULL);

		if (walkCount != resultData->elementsAdded) {
			omrtty_printf("Error: Walk did not cover all the pool elements in pool test %d\n", poolCntr);
			numFailed++;
		}
		if (pool_numElements(currentPool) != resultData->elementsAdded) {
			omrtty_printf("Error: pool_numElements returned the wrong value in pool test %d\n", poolCntr);
			numFailed++;
		}
		if (numFailed != 0) {
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
}

/* Walk the pools removing elements */
static void
testRemoveElement(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	intptr_t poolCntr;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9NewPoolResultData *resultData = &RESULTDATA[poolCntr];
		J9Pool *currentPool = resultData->poolResult;
		uintptr_t expectedNumElements = pool_numElements(currentPool);
		pool_state state;
		uintptr_t numFailed = 0;
		uintptr_t startCount, endCount;

		do {
			uint8_t *element = (uint8_t *)pool_startDo(currentPool, &state);

			startCount = pool_numElements(currentPool);
			do {
				expectedNumElements--;
				if (!pool_includesElement(currentPool, element)) {
					omrtty_printf("Error: pool_includesElement returned false before removing for walked element in pool test %d\n", poolCntr);
					numFailed++;
				}
				pool_removeElement(currentPool, element);
				if (pool_numElements(currentPool) != expectedNumElements) {
					omrtty_printf("Error: pool_numElements returned the wrong value while removing in pool test %d\n", poolCntr);
					numFailed++;
				}
				if (pool_includesElement(currentPool, element)) {
					omrtty_printf("Error: pool_includesElement returned true while removing for walked element in pool test %d\n", poolCntr);
					numFailed++;
				}
				/* Remove every other element each time - this deliberately creates fragmentation */
				element = pool_nextDo(&state);
				if (element) {
					element = pool_nextDo(&state);
				}
			} while (element);
			endCount = pool_numElements(currentPool);
		} while ((endCount > 0) && (startCount != endCount));

		if (expectedNumElements != 0) {
			omrtty_printf("Error: unexpected error - expectedNumElements is not zero for pool test %d\n", poolCntr);
			numFailed++;
		}

		if (numFailed != 0) {
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
}

/* Kill the pools */
static void
testKill(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	intptr_t poolCntr;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9NewPoolInputData *inputData = &INPUTDATA[poolCntr];
		J9NewPoolResultData *resultData = &RESULTDATA[poolCntr];
		J9Pool *currentPool = resultData->poolResult;

		resultData->lastFreeAddress = 0;
		inputData->poolFlags &= ~POOL_NEVER_FREE_PUDDLES;		/* Remove this flag so that the custom free functions don't complain */
		pool_kill(currentPool);

		if (resultData->lastFreeAddress != currentPool) {
			omrtty_printf("Error: pool_kill did not free expected data %d\n", poolCntr);
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
}

/* Clear the pools */
static void
testClear(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	intptr_t poolCntr;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9NewPoolResultData *resultData = &RESULTDATA[poolCntr];
		J9Pool *currentPool = resultData->poolResult;

		resultData->lastFreeAddress = 0;
		pool_clear(currentPool);

		if (pool_numElements(currentPool) != 0) {
			omrtty_printf("Error: pool_clear did not remove all the elements %d\n", poolCntr);
			(*failCount)++;
		} else {
			(*passCount)++;
			resultData->elementsAdded = 0;
		}
	}
}

static void
testPuddleListSharing(OMRPortLibrary *portLib, uintptr_t *passCount, uintptr_t *failCount)
{
	uint32_t index;
	uintptr_t numElements;
	void *element;
	J9Pool *pool[NUM_POOLS_TO_SHARE_PUDDLE_LIST];
	pool_state state[NUM_POOLS_TO_SHARE_PUDDLE_LIST];

	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	for (index = 0; index < NUM_POOLS_TO_SHARE_PUDDLE_LIST; index++) {

		pool[index] = pool_new(sizeof(U_64), 0, 0, 0,
							   OMR_GET_CALLSITE(), OMRMEM_CATEGORY_VM,
							   (omrmemAlloc_fptr_t) sharedPuddleListAlloc,
							   (omrmemFree_fptr_t) sharedPuddleListFree,
							   portLib);

		/* Create an element every time we create a pool. */
		pool_newElement(pool[index]);

		/*
		 * Since the underlying puddle list is shared, the number of elements in the pool
		 * should be the same as the 1 + index of the pool we're on.
		 */
		numElements = pool_numElements(pool[index]);
		if (numElements != (1 + index)) {
			omrtty_printf("Error: pool_numElements (=%d) did not match expected value (=%d)"
						  "in shared puddle list test\n", numElements, 1 + index);
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}

	/* Verify again that numElements is the same for all pools. */
	numElements = pool_numElements(pool[0]);
	for (index = 1; index < NUM_POOLS_TO_SHARE_PUDDLE_LIST; index++) {
		if (numElements != pool_numElements(pool[index])) {
			omrtty_printf("Error: pool_numElements did not match expected value "
						  "in shared puddle list test");
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}

	/* Iterate over the elements in each pool, and verify that they match. */
	element = pool_startDo(pool[0], &state[0]);
	/* Check the first element. */
	for (index = 1; index < NUM_POOLS_TO_SHARE_PUDDLE_LIST; index++) {
		if (element != pool_startDo(pool[index], &state[index])) {
			omrtty_printf("Error: Elements returned in different order when iterating over pools with shared puddle lists");
			(*failCount)++;
		} else {
			(*passCount)++;
		}
	}
	/* And all other elements. */
	while (element) {
		element = pool_nextDo(&state[0]);
		for (index = 1; index < NUM_POOLS_TO_SHARE_PUDDLE_LIST; index++) {
			if (element != pool_nextDo(&state[index])) {
				omrtty_printf("Error: Elements returned in different order when iterating over pools with shared puddle lists");
				(*failCount)++;
			} else {
				(*passCount)++;
			}
		}
	}

	/* Now, kill the pools. */
	for (index = 0; index < NUM_POOLS_TO_SHARE_PUDDLE_LIST; index++) {
		if (numElements != pool_numElements(pool[index])) {
			omrtty_printf("Error: pool_numElements did not match expected value "
						  "in shared puddle list test");
			(*failCount)++;
		} else {
			(*passCount)++;
		}
		pool_kill(pool[index]);
	}

	if (sharedPuddleList != NULL) {
		omrtty_printf("Error: sharedPuddleList was not NULL after all pools were killed");
		(*failCount)++;
	} else {
		(*passCount)++;
	}
}
