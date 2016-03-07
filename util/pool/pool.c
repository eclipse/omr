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

/**
 * @file
 * @ingroup Pool
 * @brief Pool primitives (creation, iteration, deletion, etc.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pool_internal.h"
#include "ut_pool.h"

#define ROUND_TO(granularity, number) ( (((number) % (granularity)) ? ((number) + (granularity) - ((number) % (granularity))) : (number)))
#define NEXT_FREE_SLOT(slot) SRP_PTR_GET((uintptr_t *)slot, uintptr_t*)
#define LINK_TO_FREE_LIST(prev, toAdd) SRP_PTR_SET((uintptr_t *)prev, toAdd)
#define LINK_TO_NULL(prev) SRP_PTR_SET_TO_NULL(prev)

#define PUDDLE_BITS(puddle) ((uint32_t *) ((J9PoolPuddle *) (puddle) + 1))
#define POOL_PUDDLE_BITS_LEN(pool) (((pool)->elementsPerPuddle+31) / 32)
#define PUDDLE_SLOT_FREE(puddle, sindex) (*(PUDDLE_BITS(puddle) + (((uint32_t)(sindex)) >> 5)) & (1 << (31 - (((uint32_t)(sindex)) & 31))))
#define MARK_SLOT_FREE(puddle, sindex) do { *(PUDDLE_BITS(puddle) + (((uint32_t)(sindex)) >> 5)) |=  (1 << (31 - (((uint32_t)(sindex)) & 31))); } while (0)
#define MARK_SLOT_USED(puddle, sindex) do { *(PUDDLE_BITS(puddle) + (((uint32_t)(sindex)) >> 5)) &= ~(1 << (31 - (((uint32_t)(sindex)) & 31))); } while (0)

#define COMPUTE_FIRST_ELEMENT(align, puddle, bitlength) (ROUND_TO((align), ((uintptr_t) (puddle)) + sizeof(J9PoolPuddle) + ((bitlength)*sizeof(uint32_t))))

/* HOLE_FREQUENCY defines how often a hole appears - there is a hole every HOLE_FREQUENCY elements. Must be power of two. */
#define HOLE_FREQUENCY	16
#define ELEMENT_IS_HOLE(pool, element) (((pool)->flags & POOL_USES_HOLES) && ((uintptr_t) (element) % ((pool)->elementSize*HOLE_FREQUENCY) == 0))

/**
 * Get a pointer to the SRP to the puddle, given a puddle element.
 *
 * @param[in] pool    The pool containing the element.
 * @param[in] element The valid element for which the puddle SRP pointer is being queried.
 *
 * @return A pointer to the SRP to the puddle containing the specified element.
 */
static J9SRP *
pool_getElementPuddleSRP(J9Pool *pool, void *element)
{
	J9SRP *puddleSRP;

	if (pool->flags & POOL_USES_HOLES) {
		/* (pool->elementSize*HOLE_FREQUENCY - 1) evaluates to the bits that must be masked off. */
		puddleSRP = (J9SRP *)((uintptr_t) element & ~(pool->elementSize * HOLE_FREQUENCY - 1));
	} else {
		puddleSRP = (J9SRP *)((uintptr_t) element + pool->elementSize - sizeof(J9SRP));
	}

	return puddleSRP;
}

/**
 * Get the slot (index) of the specified element in the specified puddle. It is not
 * necessary for the element to be currently allocated (ie: it could be a free slot).
 *
 * @param[in] pool    The pool containing the specified puddle and element.
 * @param[in] puddle  The puddle containing the specified element.
 * @param[in] element The element whose slot index is being queried.
 *
 * @return The element's index in the puddle, or -1 if the element is not in the puddle.
 */
static int32_t
pool_getElementPuddleSlot(J9Pool *pool, J9PoolPuddle *puddle, void *element)
{
	int32_t returnValue = -1;
	uintptr_t firstElementAddress = (uintptr_t) J9POOLPUDDLE_FIRSTELEMENTADDRESS(puddle);
	uintptr_t diff = ((uintptr_t) element - firstElementAddress);

	if ((diff % pool->elementSize) == 0) {
		uintptr_t slot = diff / pool->elementSize;

		if ((slot < pool->elementsPerPuddle) && ((int32_t) slot >= 0)) {
			returnValue = (int32_t) slot;
		}
	}

	return returnValue;
}

/**
 * Common code to initialise a puddle header. Used when creating
 * a new puddle, and when clearing the pool.
 *
 * The puddle's user data and its next, previous, next available,
 * and previous available SRP's are left unaltered.
 *
 * @param[in] pool    The pool containing the parameters to use for the puddle.
 * @param[in] puddle  The puddle that will be initialised.
 *
 * @return none
 */
static void
poolPuddle_init(J9Pool *pool, J9PoolPuddle *puddle)
{
	uintptr_t *nextLocation;
	uintptr_t *freeLocation;
	uintptr_t *lastLocation;
	uintptr_t bitlength;
	uint32_t *bits;
	uintptr_t firstElementAlignment = pool->alignment;

	if (pool->flags & POOL_USES_HOLES) {
		firstElementAlignment = pool->elementSize * HOLE_FREQUENCY;
	}

	bitlength = POOL_PUDDLE_BITS_LEN(pool);
	NNSRP_SET(puddle->firstElementAddress, COMPUTE_FIRST_ELEMENT(firstElementAlignment, puddle, bitlength));
	puddle->usedElements = 0;

	/* Mark all slots as free. */
	bits = PUDDLE_BITS(puddle);
	memset(bits, -1, bitlength * sizeof(uint32_t));

	/* Build the free list, containing all element slots. */
	if (pool->flags & POOL_USES_HOLES) {
		freeLocation = (uintptr_t *)((uintptr_t)J9POOLPUDDLE_FIRSTELEMENTADDRESS(puddle) + pool->elementSize);
	} else {
		freeLocation = (uintptr_t *) J9POOLPUDDLE_FIRSTELEMENTADDRESS(puddle);
	}
	NNSRP_SET(puddle->firstFreeSlot, freeLocation);
	nextLocation = freeLocation;
	lastLocation = (uintptr_t *)((uintptr_t)freeLocation + pool->elementSize * (pool->elementsPerPuddle - 1));
	while (nextLocation < lastLocation) {
		nextLocation = (uintptr_t *)((uintptr_t)nextLocation + pool->elementSize);
		if (!ELEMENT_IS_HOLE(pool, nextLocation)) {
			LINK_TO_FREE_LIST(freeLocation, nextLocation);
			freeLocation = nextLocation;
		}
	}
	LINK_TO_NULL(freeLocation);
}

/**
 * Allocate a new J9PoolPuddle to be used with the specified pool.
 *
 * This does not link the puddle into the pool, but simply allocates and initializes it.
 *
 * @param[in] pool  The pool containing the parameters to use for the puddle.
 *
 * @return pointer to a new J9PoolPuddle or NULL in the case of an error.
 */
J9PoolPuddle *
poolPuddle_new(J9Pool *pool)
{
	J9PoolPuddle *puddle;
	uint32_t doInit = 1;

	Trc_poolPuddle_new_Entry(pool);

	puddle = pool->memAlloc(pool->userData, (uint32_t)pool->puddleAllocSize, pool->poolCreatorCallsite, pool->memoryCategory, POOL_ALLOC_TYPE_PUDDLE, &doInit);

	if (NULL != puddle) {

		puddle->userData = 0;
		puddle->flags = 0;

		WSRP_SET(puddle->nextAvailablePuddle, NULL);
		WSRP_SET(puddle->prevAvailablePuddle, NULL);
		WSRP_SET(puddle->nextPuddle, NULL);
		WSRP_SET(puddle->prevPuddle, NULL);

		poolPuddle_init(pool, puddle);
	}

	Trc_poolPuddle_new_Exit(puddle);

	return puddle;
}

/**
 * Delete the given J9PoolPuddle from the specified pool.
 *
 * It is assumed that this puddle is empty of any elements.
 *
 * If the puddle is the last one in the pool, the puddle will
 * not be removed.
 *
 * @param[in] pool    The pool containing the puddle.
 * @param[in] puddle  The puddle that will be removed.
 *
 * @return none
 */
static void
poolPuddle_delete(J9Pool *pool, J9PoolPuddle *puddle)
{
	J9PoolPuddleList *puddleList = J9POOL_PUDDLELIST(pool);
	J9PoolPuddle *next = J9POOLPUDDLE_NEXTPUDDLE(puddle);
	J9PoolPuddle *prev = J9POOLPUDDLE_PREVPUDDLE(puddle);

	if (NULL != prev) {
		WSRP_SET(prev->nextPuddle, next);
		if (NULL != next) {
			NNWSRP_SET(next->prevPuddle, prev);
		}
	} else if (NULL == next) {
		/* Don't remove the puddle if it's the last one in the pool. */
		puddle = NULL;
	} else {
		/* Assume puddle is the first puddle in the pool. */
		WSRP_SET(puddleList->nextPuddle, next);
		WSRP_SET(next->prevPuddle, NULL);
	}

	if (NULL != puddle) {
		/* Remove it from the available puddles list also, if it is in it. */
		J9PoolPuddle *poolNext = J9POOLPUDDLELIST_NEXTAVAILABLEPUDDLE(puddleList);

		next = J9POOLPUDDLE_NEXTAVAILABLEPUDDLE(puddle);
		prev = J9POOLPUDDLE_PREVAVAILABLEPUDDLE(puddle);

		if (poolNext == puddle) {
			WSRP_SET(puddleList->nextAvailablePuddle, next);
		} else if (NULL != prev) {
			WSRP_SET(prev->nextAvailablePuddle, next);
		}

		if (NULL != next) {
			WSRP_SET(next->prevAvailablePuddle, prev);
		}

		/* And free the memory. */
		pool->memFree(pool->userData, puddle, POOL_ALLOC_TYPE_PUDDLE);
	}

}

/**
 *	Returns a handle to a variable sized pool of structures.
 *	This handle should be passed into all other pool functions.
 *
 * @param[in] structSize Size of the pool-elements
 * @param[in] minNumberElelements If zero, will default to 1
 * @param[in] elementAlignment If zero will default to MIN_GRANULARITY
 * @param[in] poolFlags
 * @param[in] creatorCallSite location of the function creating the pool
 * @param[in] memoryCategory Memory category for the function creating the pool
 * @param[in] memAlloc Allocate function pointer for J9Pools
 * @param[in] memFree  Free function pointer for J9Pools
 * @param[in] userData Passed as first parameter into allocation and free calls
 *
 * @return pointer to a new pool, or NULL if the pool could not be created.
 *
 */
J9Pool *
pool_new(uintptr_t structSizeArg,
		 uintptr_t numberElementsArg,
		 uintptr_t elementAlignmentArg,
		 uintptr_t poolFlags,
		 const char *poolCreatorCallsite,
		 uint32_t memoryCategory,
		 omrmemAlloc_fptr_t memAlloc,
		 omrmemFree_fptr_t memFree,
		 void *userData)
{
	uint32_t doInit;
	uint64_t tempAllocSize, puddleAllocSize;
	uint32_t finalNumberOfElements, minNumberElements;
	uint32_t roundedStructSize, puddleHeaderAllocSize, puddleBitsSize, newPuddleBitsSize;
	J9Pool *pool;
	uint32_t firstElementAlignment;
	uint32_t structSize = (uint32_t)structSizeArg;
	uint32_t numberElements = (uint32_t)numberElementsArg;
	uint32_t elementAlignment = (uint32_t)elementAlignmentArg;

	Trc_pool_new_Entry(structSize, numberElements, elementAlignment, poolFlags, memAlloc, memFree, userData);

	if (((uintptr_t)structSize != structSizeArg) || ((uintptr_t)numberElements != numberElementsArg) || ((uintptr_t)elementAlignment != elementAlignmentArg)) {
		/* one of the uintptr_t arguments is too large to fit in a uint32_t. Don't even try to allocate this pool. */
		Trc_pool_new_ArgumentTooLargeExit(structSizeArg, numberElementsArg, elementAlignmentArg);
		return NULL;
	}

	if (elementAlignment == 0) {
		elementAlignment = MIN_GRANULARITY;
	}

	if (numberElements == 0) {
		minNumberElements = 1;
		poolFlags |= POOL_ROUND_TO_PAGE_SIZE;
	} else {
		minNumberElements = numberElements;
	}

	roundedStructSize = ROUND_TO(elementAlignment, structSize);

	poolFlags &= ~POOL_USES_HOLES;

	switch (roundedStructSize) {
	case 4:
	case 8:
	case 16:
		/* Use holes when all of the following are true:
		 *  1. element size is either 4, 8, or 16 (must be powers of 2)
		 *  2. rounded element size doesn't give us space for a J9SRP "for free"
		 *  3. element alignment is divisible by MIN_GRANULARITY (which is a power of 2)
		 */
		if ((roundedStructSize - structSize) < sizeof(J9SRP) &&
			(elementAlignment % MIN_GRANULARITY) == 0
		) {
			poolFlags |= POOL_USES_HOLES;
		}
		break;
	default:
		break;
	}

	if (poolFlags & POOL_USES_HOLES) {
		/* Note: roundedStructSize must be a power of 2, so that firstElementAlignment will be also. */
		firstElementAlignment = roundedStructSize * HOLE_FREQUENCY;
	} else {
		/* Round up the ( structSize + sizeof(puddle SRP) ) to the closest value divisible by elementAlignment. */
		if ((roundedStructSize - structSize) < sizeof(J9SRP)) {
			roundedStructSize = ROUND_TO(elementAlignment, structSize + sizeof(J9SRP));
		}
		firstElementAlignment = elementAlignment;
	}

	/* Header size is as follows (if not using holes):
	 *
	 * +--------------+-------------+--------------------+   +-------------+------------+-------------+------------+-----+
	 * | J9PoolPuddle | puddle bits | alignment overhead |   | 1st element | puddle SRP | 2nd element | puddle SRP | ... |
	 * +--------------+-------------+--------------------+   +-------------+------------+-------------+------------+-----+
	 *
	 * Note: With POOL_USES_HOLES, puddleSRPs are instead stored once in every HOLE_FREQUENCY slots.
	 */
	newPuddleBitsSize = ((minNumberElements + 31) >> 3);
	do {
		puddleBitsSize = newPuddleBitsSize;
		puddleHeaderAllocSize = ROUND_TO(elementAlignment, sizeof(J9PoolPuddle) + puddleBitsSize) + (firstElementAlignment - MALLOC_ALIGNMENT);
		if (poolFlags & POOL_USES_HOLES) {
			/* Every sector has HOLE_FREQUENCY (16) slots (one of which is the "hole"). */
			uint32_t sectorSize = roundedStructSize * HOLE_FREQUENCY;
			uint32_t numberOfSectors = (minNumberElements + HOLE_FREQUENCY - 2) / (HOLE_FREQUENCY - 1);
			tempAllocSize = sectorSize * numberOfSectors + puddleHeaderAllocSize;
			puddleAllocSize = (poolFlags & POOL_ROUND_TO_PAGE_SIZE) ? ROUND_TO(OS_PAGE_SIZE, tempAllocSize) : tempAllocSize;
			numberOfSectors += (uint32_t)((puddleAllocSize - tempAllocSize) / sectorSize);
			finalNumberOfElements = numberOfSectors * HOLE_FREQUENCY;
		} else {
			tempAllocSize = roundedStructSize * minNumberElements + puddleHeaderAllocSize;
			puddleAllocSize = (poolFlags & POOL_ROUND_TO_PAGE_SIZE) ? ROUND_TO(OS_PAGE_SIZE, tempAllocSize) : tempAllocSize;
			finalNumberOfElements = minNumberElements;
			finalNumberOfElements += (uint32_t)((puddleAllocSize - tempAllocSize) / roundedStructSize);
		}
		newPuddleBitsSize = ((finalNumberOfElements + 31) >> 3);
	} while (newPuddleBitsSize != puddleBitsSize);

	/*
	 * puddleAllocSize is a uint64_t so that we can detect pool sizes which overflow 32-bits.
	 * See CMVC defects 84003 and 90099
	 */
	if (puddleAllocSize > (uint64_t)0x7FFFFFFF) {
		Trc_pool_new_TooLargeExit(puddleAllocSize);
		return NULL;
	}

	pool = memAlloc(userData, sizeof(J9Pool), poolCreatorCallsite, memoryCategory, POOL_ALLOC_TYPE_POOL, &doInit);

	if (NULL != pool) {
		J9PoolPuddleList *puddleList;

		pool->elementSize = (uintptr_t)roundedStructSize;
		pool->alignment = (uint16_t)elementAlignment;	/* we assume no alignment is > 64k */
		pool->puddleAllocSize = (uintptr_t)puddleAllocSize;
		pool->flags = (uint16_t)poolFlags;
		pool->poolCreatorCallsite = poolCreatorCallsite;
		pool->elementsPerPuddle = finalNumberOfElements;
		pool->memAlloc = memAlloc;
		pool->memFree = memFree;
		pool->userData = userData;
		pool->memoryCategory = memoryCategory;

		doInit = 1;
		puddleList = memAlloc(userData, sizeof(J9PoolPuddleList), poolCreatorCallsite, memoryCategory, POOL_ALLOC_TYPE_PUDDLE_LIST, &doInit);

		if (NULL != puddleList) {
			NNWSRP_SET(pool->puddleList, puddleList);

			if (doInit) {
				J9PoolPuddle *firstPuddle = poolPuddle_new(pool);
				if (NULL != firstPuddle) {
					puddleList->numElements = 0;
					NNWSRP_SET(puddleList->nextPuddle, firstPuddle);
					NNWSRP_SET(puddleList->nextAvailablePuddle, firstPuddle);
				} else {
					memFree(userData, puddleList, POOL_ALLOC_TYPE_PUDDLE_LIST);
					memFree(userData, pool, POOL_ALLOC_TYPE_POOL);
					pool = NULL;
				}
			}
		} else {
			memFree(userData, pool, POOL_ALLOC_TYPE_POOL);
			pool = NULL;
		}
	}

	Trc_pool_new_Exit(pool);
	return pool;
}


/**
 *	Deallocates all memory associated with a pool.
 *
 * @param[in] pool Pool to be deallocated
 *
 * @return none
 *
 */
void
pool_kill(J9Pool *pool)
{
	Trc_pool_kill_Entry(pool);

	if (NULL != pool) {
		J9PoolPuddleList *puddleList = J9POOL_PUDDLELIST(pool);
		J9PoolPuddle *walk = J9POOLPUDDLELIST_NEXTPUDDLE(puddleList);
		J9PoolPuddle *puddle;

		while (NULL != walk) {
			puddle = walk;
			walk = J9POOLPUDDLE_NEXTPUDDLE(puddle);
			pool->memFree(pool->userData, puddle, POOL_ALLOC_TYPE_PUDDLE);
		}

		pool->memFree(pool->userData, puddleList, POOL_ALLOC_TYPE_PUDDLE_LIST);
		pool->memFree(pool->userData, pool, POOL_ALLOC_TYPE_POOL);
	}

	Trc_pool_kill_Exit();
}

/**
 *	Asks for the address of a new pool element.
 *
 *	If it succeeds, the address returned will have space for
 *	one element of the correct structure size.
 *
 *	The contents of the element will be set to 0's unless the
 *  POOL_NO_ZERO flag is set on the pool, in which case the
 *  contents are undefined.
 *
 *	If all puddles in the pool are full, a new puddle will be
 *  grafted onto the end of the pool's puddle chain and the
 *  element returned will come from this puddle.
 *
 * @param[in] pool
 *
 * @return NULL on error
 * @return pointer to a new element otherwise
 *
 */
void *
pool_newElement(J9Pool *pool)
{
	int32_t slot;
	void *newElement;
	void *nextFreeElement;
	J9SRP *puddleSRP;
	J9PoolPuddle *puddle;
	J9PoolPuddleList *puddleList;

	Trc_pool_newElement_Entry(pool);

	if (NULL == pool) {
		Trc_pool_newElement_ExitNoop();
		return NULL;
	}

	/* Check if there is a puddle with free slots - if so use it. */
	puddleList = J9POOL_PUDDLELIST(pool);

	puddle = J9POOLPUDDLELIST_NEXTAVAILABLEPUDDLE(puddleList);
	if (NULL == puddle) {
		J9PoolPuddle *head;

		/* No available puddles. Allocate a new one. */
		puddle = poolPuddle_new(pool);
		if (NULL == puddle) {
			Trc_pool_newElement_Exit(NULL);
			return NULL;
		}

		/* Stick it at the top of the list. */
		head = J9POOLPUDDLELIST_NEXTPUDDLE(puddleList);
		NNWSRP_SET(puddleList->nextPuddle, puddle);
		NNWSRP_SET(puddle->nextPuddle, head);
		NNWSRP_SET(head->prevPuddle, puddle);
		/* And point to it as the next (and the only, right now) available puddle on the pool. */
		NNWSRP_SET(puddleList->nextAvailablePuddle, puddle);
	}

	newElement = J9POOLPUDDLE_FIRSTFREESLOT(puddle);
	nextFreeElement = NEXT_FREE_SLOT(newElement);

	SRP_SET(puddle->firstFreeSlot, nextFreeElement);
	slot = pool_getElementPuddleSlot(pool, puddle, newElement);
	MARK_SLOT_USED(puddle, slot);
	puddle->usedElements++;
	puddleList->numElements++;
	if (!(pool->flags & POOL_NO_ZERO)) {
		memset(newElement, 0, pool->elementSize);
	}
	puddleSRP = pool_getElementPuddleSRP(pool, newElement);
	NNSRP_SET(*puddleSRP, puddle);

	/* If the puddle is full, remove it from the list of available puddles. */
	if (NULL == nextFreeElement) {
		J9PoolPuddle *next = J9POOLPUDDLE_NEXTAVAILABLEPUDDLE(puddle);
		J9PoolPuddle *prev = J9POOLPUDDLE_PREVAVAILABLEPUDDLE(puddle);

		if (NULL != prev) {
			WSRP_SET(prev->nextAvailablePuddle, next);
		} else {
			/* Assume it is the first one in the pool. */
			WSRP_SET(puddleList->nextAvailablePuddle, next);
		}

		if (NULL != next) {
			WSRP_SET(next->prevAvailablePuddle, prev);
		}

		WSRP_SET(puddle->nextAvailablePuddle, NULL);
		WSRP_SET(puddle->prevAvailablePuddle, NULL);
	}

	Trc_pool_newElement_Exit(newElement);

	return newElement;
}

/**
 *	Deallocates an element from a pool.
 *
 * It is safe to call pool_removeElement() while looping over the
 * pool with @ref pool_startDo / @ref pool_nextDo on the element
 * returned by those calls.
 *
 * @param[in] pool
 * @param[in] anElement Pointer to the element to be removed
 *
 * @return none
 *
 */
void
pool_removeElement(J9Pool *pool, void *anElement)
{
	J9SRP *puddleSRP;
	int32_t slot;
	J9PoolPuddle *puddle;
	J9PoolPuddleList *puddleList;
	void *freeLocation;

	Trc_pool_removeElement_Entry(pool, anElement);

	if (!(pool && anElement)) {
		Trc_pool_removeElement_ExitNoop();
		return;
	}

	puddleList = J9POOL_PUDDLELIST(pool);
	puddleSRP = pool_getElementPuddleSRP(pool, anElement);
	puddle = NNSRP_GET(*puddleSRP, J9PoolPuddle *);
	slot = pool_getElementPuddleSlot(pool, puddle, anElement);
	if (slot < 0) {
		Trc_pool_removeElement_NotFound(anElement, J9POOLPUDDLELIST_NEXTPUDDLE(puddleList));
		Trc_pool_removeElement_Exit();
		return;		/* this is an error...  we were passed a bogus data pointer. */
	}

	if (PUDDLE_SLOT_FREE(puddle, slot)) {
		Trc_pool_removeElement_NotFound(anElement, puddle);
		Trc_pool_removeElement_Exit();
		return;		/* this is an error... the slot was already free. */
	}

	MARK_SLOT_FREE(puddle, slot);
	puddle->usedElements--;
	puddleList->numElements--;
	freeLocation = (void *) J9POOLPUDDLE_FIRSTFREESLOT(puddle);
	SRP_SET(puddle->firstFreeSlot, anElement);
	LINK_TO_FREE_LIST(anElement, freeLocation);

	/* If the puddle's empty, and we're allowed to free it, then remove it. */
	if ((puddle->usedElements == 0) && !(pool->flags & POOL_NEVER_FREE_PUDDLES)) {
		poolPuddle_delete(pool, puddle);
	} else if (NULL == freeLocation) {
		/* It was full before - but not anymore - add it to the top of the available puddles list. */
		J9PoolPuddle *next = J9POOLPUDDLELIST_NEXTAVAILABLEPUDDLE(puddleList);

		WSRP_SET(puddleList->nextAvailablePuddle, puddle);
		WSRP_SET(puddle->prevAvailablePuddle, NULL);
		WSRP_SET(puddle->nextAvailablePuddle, next);
		if (NULL != next) {
			WSRP_SET(next->prevAvailablePuddle, puddle);
		}
	}

	Trc_pool_removeElement_Exit();
}

/**
 *	Calls a user provided function for each element in the list.
 *
 * @param[in] pool The pool to "do" things to
 * @param[in] doFunction Pointer to function which will "do" things to the elements of pool
 * @param[in] userData Pointer to data to be passed to "do" function, along with each pool-element
 *
 * @return none
 *
 * @see pool_startDo, pool_nextDo
 *
 */
void
pool_do(J9Pool *pool, void (*doFunction)(void *anElement, void *userData), void *userData)
{
	void *anElement;
	pool_state aState;

	Trc_pool_do_Entry(pool, doFunction, userData);

	anElement = pool_startDo(pool, &aState);

	while (anElement) {
		(doFunction)(anElement, userData);
		anElement = pool_nextDo(&aState);
	}

	Trc_pool_do_Exit();
}

/**
 *	Returns the number of elements in a given pool.
 *
 * @param[in] pool
 *
 * @return 0 on error
 * @return the number of elements in the pool otherwise
 *
 */
uintptr_t
pool_numElements(J9Pool *pool)
{
	uintptr_t numElements;
	J9PoolPuddleList *puddleList;

	Trc_pool_numElements_Entry(pool);

	puddleList = J9POOL_PUDDLELIST(pool);
	numElements = puddleList->numElements;

	Trc_pool_numElements_Exit(numElements);

	return numElements;
}

/**
 *	Start of an iteration set that will return when code is to be executed.
 *	This is different to pool_startDo because it allows a specific puddle to be
 *  specified as the start point and also provides the option to only walk that puddle.
 *	Pass in a pointer to an empty pool_state and it will be filled in.
 *
 * @param[in] pool The pool to "do" things to
 * @param[in] currentPuddle The puddle to start the walk from
 * @param[in] state The pool_state to be used for this iteration.
 * @param[in] followNextPointers If zero, prevents the walk of other puddles
 *
 * @return NULL
 * @return pointer to element otherwise
 *
 * @see pool_do, pool_nextDo
 *
 */
void *
poolPuddle_startDo(J9Pool *pool, J9PoolPuddle *currentPuddle, pool_state *state, uintptr_t followNextPointers)
{
	int32_t slot = 0;
	uintptr_t *currAddr;

	Trc_poolPuddle_startDo_Entry(pool, currentPuddle, state, followNextPointers);

	if (!(pool && currentPuddle)) {
		Trc_poolPuddle_startDo_ExitNullPoolPuddleExit();
		return NULL;
	}

	if (0 == currentPuddle->usedElements) {	/* this puddle is empty */
		Trc_poolPuddle_startDo_EmptyExit();
		if ((currentPuddle->nextPuddle != 0) && (followNextPointers != 0)) {
			return poolPuddle_startDo(pool, J9POOLPUDDLE_NEXTPUDDLE(currentPuddle), state, followNextPointers);
		} else {
			return NULL;		/* totally empty */
		}
	}

	/* Note: Under "hole" implementation, "hole" slots are marked as free. */
	while (PUDDLE_SLOT_FREE(currentPuddle, slot)) {
		slot++;
	}
	currAddr = (uintptr_t *)((uintptr_t) J9POOLPUDDLE_FIRSTELEMENTADDRESS(currentPuddle) + pool->elementSize * slot);

	state->thePool = pool;
	state->currentPuddle = currentPuddle;
	state->lastSlot = slot;
	state->leftToDo = currentPuddle->usedElements - 1;
	state->flags = 0;
	if (followNextPointers) {
		state->flags |= POOLSTATE_FOLLOW_NEXT_POINTERS;
	}

	if (state->leftToDo == 0) {
		/* Pre-fetch the next puddle in case this element is deleted and the puddle is freed. */
		if (followNextPointers) {
			state->currentPuddle = J9POOLPUDDLE_NEXTPUDDLE(state->currentPuddle);
			state->lastSlot = -1; /* In order to start at slot 0 on the next puddle. */
		} else {
			/* If not following next pointers, we are done with this pool. */
			state->currentPuddle = NULL;
		}
	}

	Trc_poolPuddle_startDo_Exit(currAddr);

	return (void *) currAddr;
}

/**
 *	Start of an iteration set that will return when code is to be executed.
 *
 *	Pass in a pointer to an empty pool_state and it will be filled in.
 *
 * @param[in] pool  The pool to "do" things to
 * @param[in] state The pool_state to be used for this iteration.
 *
 * @return NULL
 * @return pointer to element otherwise
 *
 * @see pool_do, pool_nextDo
 *
 */
void *
pool_startDo(J9Pool *pool, pool_state *state)
{
	void *result = NULL;

	Trc_pool_startDo_Entry(pool, state);

	if (pool) {
		J9PoolPuddleList *puddleList = J9POOL_PUDDLELIST(pool);
		result = poolPuddle_startDo(pool, J9POOLPUDDLELIST_NEXTPUDDLE(puddleList), state, TRUE);
	}

	Trc_pool_startDo_Exit(result);

	return result;
}

/**
 *	Continue an iteration based on state passed in by lastHandle.
 *	It is safe to stop an iteration midway through.
 *
 * @param[in] state Pointer for current iteration state.
 *
 * @return NULL nothing more to be done
 * @return pointer to next element to be processed otherwise
 *
 * @see pool_do, pool_startDo
 *
 */
void *
pool_nextDo(pool_state *state)
{
	int32_t slot = 1 + state->lastSlot;
	uintptr_t *currAddr;

	Trc_pool_nextDo_Entry(state);

	if (state->leftToDo == 0) {		/* no more used elements, stop this pool. */
		/* if leftToDo is 0, then currentPuddle will already have been advanced to point to the next puddle */
		if (state->currentPuddle) {		/* check the next one. */
			Trc_pool_nextDo_NextPuddle();
			return poolPuddle_startDo(state->thePool, state->currentPuddle, state, TRUE);
		} else {
			Trc_pool_nextDo_Finished();
			return NULL;
		}
	}

	/* there's at least one more used element */

	/* Note: Under "hole" implementation, "hole" slots are marked as free. */
	while (PUDDLE_SLOT_FREE(state->currentPuddle, slot)) {
		slot++;
	}
	currAddr = (uintptr_t *)((uintptr_t) J9POOLPUDDLE_FIRSTELEMENTADDRESS(state->currentPuddle)
							 + state->thePool->elementSize * slot);
	state->lastSlot = slot;
	state->leftToDo--;

	/* Is this the last element in the puddle? */
	if (state->leftToDo == 0) {
		if (state->flags & POOLSTATE_FOLLOW_NEXT_POINTERS) {
			/* Pre-fetch the next puddle in case this element is deleted and the puddle is freed. */
			state->currentPuddle = J9POOLPUDDLE_NEXTPUDDLE(state->currentPuddle);
			state->lastSlot = -1; /* In order to start at slot 0 on the next puddle. */
		} else {
			/* If not following next pointers, we are done with this pool. */
			state->currentPuddle = NULL;
		}
	}

	Trc_pool_nextDo_Exit(currAddr);

	return (void *) currAddr;
}

/**
 * Clear the contents of a pool, but do not de-allocate the puddles or the pool.
 *
 * @note Make no assumptions about the contents of the pool after invoking this method (it currently does not zero the memory)
 *
 * @param[in] pool The pool to clear
 *
 * @return none
 *
 */
void
pool_clear(J9Pool *pool)
{
	Trc_pool_clear_Entry(pool);

	if (pool) {
		J9PoolPuddleList *puddleList = J9POOL_PUDDLELIST(pool);
		J9PoolPuddle *walk = J9POOLPUDDLELIST_NEXTPUDDLE(puddleList);

		NNWSRP_SET(puddleList->nextAvailablePuddle, walk);
		while (walk) {
			J9PoolPuddle *next, *prev;
			poolPuddle_init(pool, walk);
			prev = J9POOLPUDDLE_PREVPUDDLE(walk);
			next = J9POOLPUDDLE_NEXTPUDDLE(walk);
			WSRP_SET(walk->prevAvailablePuddle, prev);
			WSRP_SET(walk->nextAvailablePuddle, next);
			walk = next;
		}

		puddleList->numElements = 0;
	}

	Trc_pool_clear_Exit();
}

/**
 * See if an element is currently allocated from a pool.
 *
 * @param[in] pool The pool to check
 * @param[in] anElement The element to check
 *
 * @return uintptr_t - TRUE if the element is allocated, FALSE otherwise
 *
 */
uintptr_t
pool_includesElement(J9Pool *pool, void *anElement)
{
	J9PoolPuddleList *puddleList;
	J9PoolPuddle *walk;

	Trc_pool_includesElement_Entry(pool, anElement);

	if (!(pool && anElement)) {
		Trc_pool_includesElement_ExitNoop();
		return FALSE;
	}

	puddleList = J9POOL_PUDDLELIST(pool);
	walk = J9POOLPUDDLELIST_NEXTPUDDLE(puddleList);

	while (walk != NULL) {
		/* Conveniently, -1 is returned if the element is not a slot in the puddle. */
		int32_t slot = pool_getElementPuddleSlot(pool, walk, anElement);
		if (slot >= 0) {
			if (PUDDLE_SLOT_FREE(walk, slot)) {
				Trc_pool_includesElement_ExitFoundFree();
				return FALSE;
			} else {
				Trc_pool_includesElement_ExitSuccess();
				return TRUE;
			}
		}
		walk = J9POOLPUDDLE_NEXTPUDDLE(walk);
	}

	Trc_pool_includesElement_ExitOutOfScope();
	return FALSE;
}

#if defined(J9ZOS390)
/* Temporary hack to resolve ZOS linking problems.
 * Functions in pool_cap.c are not getting short-names properly without this fix.  */
void
refToPoolCap(J9Pool *aPool)
{
	pool_ensureCapacity(aPool, 0);
}
#endif /* J9ZOS390 */
