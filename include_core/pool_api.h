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

#ifndef pool_api_h
#define pool_api_h

#include "omrsrp.h"

#define MIN_GRANULARITY		sizeof(uintptr_t)	/* default structure alignment */
#define MALLOC_ALIGNMENT	sizeof(uintptr_t)	/* alignment enforced by malloc() */

/* read this if a port library call becomes available that gives out the OS page size */
#if 0
#define OS_PAGE_SIZE		(EsGetAddressSpacePageSize())
#else
#define OS_PAGE_SIZE		4096
#endif

/**
* @file pool_api.h
* @brief Public API for the POOL module.
*
* This file contains public function prototypes and
* type definitions for the POOL module.
*
*/

#include "omrcomp.h"
#include "omrpool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- pool.c ---------------- */


/**
* @brief
* @param aPool
* @return void
*/
void
pool_clear(J9Pool *aPool);


/**
* @brief
* @param aPool
* @param aFunction
* @param userData
* @return void
*/
void
pool_do(J9Pool *aPool, void (*aFunction)(void *anElement, void *userData), void *userData);

/**
* @brief
* @param aPool
* @return void
*/
void
pool_kill(J9Pool *aPool);

/**
* @brief
* @param structSize
* @param minNumberElements
* @param elementAlignment
* @param poolFlags
* @param[in] creatorCallSite location of the function creating the pool
* @param[in] memoryCategory memory category
* @param void*(*memAlloc)(void*,uint32_t)
* @param void(*memFree)(void*,void*)
* @param userData
* @return J9Pool*
*/
J9Pool *
pool_new(uintptr_t structSize,
		 uintptr_t minNumberElements,
		 uintptr_t elementAlignment,
		 uintptr_t poolFlags,
		 const char *poolCreatorCallsite,
		 uint32_t memoryCategory,
		 omrmemAlloc_fptr_t memAlloc,
		 omrmemFree_fptr_t memFree,
		 void *userData);

/**
* @brief
* @param aPool
* @return void *
*/
void *
pool_newElement(J9Pool *aPool);


/**
* @brief
* @param *lastHandle
* @return void*
*/
void *
pool_nextDo(pool_state *lastHandle);


/**
* @brief
* @param *aPool
* @return uintptr_t
*/
uintptr_t
pool_numElements(J9Pool *aPool);


/**
* @brief
* @param *aPool
* @param *anElement
* @return void
*/
void
pool_removeElement(J9Pool *aPool, void *anElement);


/**
* @brief
* @param *aPool
* @return J9PoolPuddle
*/
J9PoolPuddle *
poolPuddle_new(J9Pool *aPool);

/**
* @brief
* @param *aPool
* @param *lastHandle
* @return void*
*/
void *
pool_startDo(J9Pool *aPool, pool_state *lastHandle);

/**
* @brief
* @param *aPool
* @param *currentPuddle
* @param *lastHandle
* @param followNextPointers
* @return void*
*/
void *
poolPuddle_startDo(J9Pool *aPool, J9PoolPuddle *currentPuddle, pool_state *lastHandle, uintptr_t followNextPointers);

/* ---------------- pool_cap.c ---------------- */

/**
* @brief
* @param *aPool
* @return uintptr_t
*/
uintptr_t
pool_capacity(J9Pool *aPool);


/**
* @brief
* @param *aPool
* @param newCapacity
* @return uintptr_t
*/
uintptr_t
pool_ensureCapacity(J9Pool *aPool, uintptr_t newCapacity);


/**
* @brief
* @param *aPool
* @return uintptr_t
*/
uintptr_t
pool_includesElement(J9Pool *aPool, void *anElement);

#ifdef __cplusplus
}
#endif

#endif /* pool_api_h */
