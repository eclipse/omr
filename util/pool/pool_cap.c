/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 * @brief Pool-capacity functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pool_internal.h"
#include "ut_pool.h"

/**
 * Ensures that the pool is large enough for newCapacity elements.
 * This has the side effect of setting the POOL_NEVER_FREE_PUDDLES flag.
 * Without this, the pool could shrink back down to its original size.
 * Note that this does not take into account the number of elements already
 * used in the pool.
 *
 * @param[in] aPool The pool
 * @param[in] newCapacity The desired new-size of the pool
 *
 * @return 0 on success
 * @return -1 on failure
 *
 */
uintptr_t
pool_ensureCapacity(J9Pool *aPool, uintptr_t newCapacity)
{
	uintptr_t numElements;
	uintptr_t result = 0;

	Trc_pool_ensureCapacity_Entry(aPool, newCapacity);

	numElements = pool_capacity(aPool);

	/* mark each pool as POOL_NEVER_FREE_PUDDLES */
	aPool->flags |= POOL_NEVER_FREE_PUDDLES;

	if (newCapacity > numElements) {
		J9PoolPuddleList *puddleList;
		J9PoolPuddle *newPuddle, *lastPuddle;
		uintptr_t newSize = newCapacity - numElements;

		puddleList = J9POOL_PUDDLELIST(aPool);
		lastPuddle = J9POOLPUDDLELIST_NEXTPUDDLE(puddleList);
		for (;;) {
			if (lastPuddle->nextPuddle == 0) {
				break;
			}
			lastPuddle = J9POOLPUDDLE_NEXTPUDDLE(lastPuddle);
		}
		while (newSize > 0) {
			J9PoolPuddle *puddle;

			if (newSize < aPool->elementsPerPuddle) {
				newSize = aPool->elementsPerPuddle;
			}

			newPuddle = poolPuddle_new(aPool);
			if (newPuddle == 0) {
				Trc_pool_ensureCapacity_OutOfMemory(newCapacity);
				result = -1;
			}

			/* Stick it at the end of the list. */
			NNWSRP_SET(lastPuddle->nextPuddle, newPuddle);
			NNWSRP_SET(newPuddle->prevPuddle, lastPuddle);
			/* And also at the top of the available puddle list. */
			puddle = WSRP_GET(puddleList->nextAvailablePuddle, J9PoolPuddle *);
			if (puddle) {
				NNWSRP_SET(newPuddle->nextAvailablePuddle, puddle);
			}
			NNWSRP_SET(puddleList->nextAvailablePuddle, newPuddle);

			lastPuddle = newPuddle;
			newSize -= aPool->elementsPerPuddle;
		}
	}

	Trc_pool_ensureCapacity_Exit(result);
	return result;
}


/**
 * Returns the total capacity of a pool
 *
 * @param[in] aPool The pool
 *
 * @return 0 on error
 * @return numElements in aPool otherwise
 *
 */
uintptr_t
pool_capacity(J9Pool *aPool)
{
	uintptr_t numElements = 0;

	Trc_pool_capacity_Entry(aPool);

	if (aPool) {
		J9PoolPuddleList *puddleList = J9POOL_PUDDLELIST(aPool);
		J9PoolPuddle *walk = J9POOLPUDDLELIST_NEXTPUDDLE(puddleList);

		while (walk) {
			numElements += aPool->elementsPerPuddle;
			walk = J9POOLPUDDLE_NEXTPUDDLE(walk);
		}
	}

	Trc_pool_capacity_Exit(numElements);

	return numElements;
}


