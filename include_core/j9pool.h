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

#ifndef J9POOL_H
#define J9POOL_H

#ifdef __cplusplus
extern "C" {
#endif


/*		DO NOT DIRECTLY INCLUDE THIS FILE 	*/
/*		Include pool_api.h instead					*/

#include "omrcomp.h"
#include "omrsrp.h"

typedef void *(*omrmemAlloc_fptr_t)(void *, uint32_t, const char *, uint32_t, uint32_t, uint32_t *);
typedef void (*omrmemFree_fptr_t)(void *, void *, uint32_t);

typedef struct J9PoolPuddleList {
	uintptr_t numElements;
	J9WSRP nextPuddle;
	J9WSRP nextAvailablePuddle;
} J9PoolPuddleList;

/*
 * @ddr_namespace: map_to_type=J9PoolPuddle
 */

typedef struct J9PoolPuddle {
	uintptr_t usedElements;
	J9SRP firstElementAddress;
	J9SRP firstFreeSlot;
	J9WSRP prevPuddle;
	J9WSRP nextPuddle;
	J9WSRP prevAvailablePuddle;
	J9WSRP nextAvailablePuddle;
	uintptr_t userData;
	uintptr_t flags;
} J9PoolPuddle;


#define PUDDLE_KILLED  4
#define PUDDLE_ACTIVE  2

/*
 * @ddr_namespace: map_to_type=J9Pool
 */

typedef struct J9Pool {
	uintptr_t elementSize;
	uintptr_t elementsPerPuddle;
	uintptr_t puddleAllocSize;
	J9WSRP puddleList;
	void  *(*memAlloc)(void *userData, uint32_t byteAmount, const char *callsite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit);
	void (*memFree)(void *userData, void *ptr, uint32_t type);
	const char *poolCreatorCallsite;
	void *userData;
	uint16_t alignment;
	uint16_t flags;
	uint32_t memoryCategory;
} J9Pool;

#define POOL_NO_ZERO  8
#define POOL_ROUND_TO_PAGE_SIZE  16
#define POOL_USES_HOLES  32
#define POOL_NEVER_FREE_PUDDLES  2
#define POOL_ALLOC_TYPE_PUDDLE  1
#define POOL_ALWAYS_KEEP_SORTED  4
#define POOL_ALLOC_TYPE_PUDDLE_LIST  2
#define POOL_ALLOC_TYPE_POOL  0

/*
 * @ddr_namespace: map_to_type=J9PoolState
 */

typedef struct J9PoolState {
	uintptr_t leftToDo;
	struct J9Pool *thePool;
	struct J9PoolPuddle *currentPuddle;
	int32_t lastSlot;
	uintptr_t flags;
} J9PoolState;


#define POOLSTATE_FOLLOW_NEXT_POINTERS  1

#define pool_state J9PoolState

#define J9POOLPUDDLE_FIRSTFREESLOT(parm) SRP_GET((parm)->firstFreeSlot, uintptr_t*)
#define J9POOLPUDDLE_FIRSTELEMENTADDRESS(parm) NNSRP_GET((parm)->firstElementAddress, void*)
#define J9POOLPUDDLE_PREVPUDDLE(parm) WSRP_GET((parm)->prevPuddle, J9PoolPuddle*)
#define J9POOLPUDDLE_NEXTPUDDLE(parm) WSRP_GET((parm)->nextPuddle, J9PoolPuddle*)
#define J9POOLPUDDLE_NEXTAVAILABLEPUDDLE(parm) WSRP_GET((parm)->nextAvailablePuddle, J9PoolPuddle*)
#define J9POOLPUDDLE_PREVAVAILABLEPUDDLE(parm) WSRP_GET((parm)->prevAvailablePuddle, J9PoolPuddle*)
#define J9POOL_PUDDLELIST(pool) NNWSRP_GET((pool)->puddleList, J9PoolPuddleList*)
#define J9POOLPUDDLELIST_NEXTPUDDLE(parm) NNWSRP_GET((parm)->nextPuddle, J9PoolPuddle*)
#define J9POOLPUDDLELIST_NEXTAVAILABLEPUDDLE(parm) WSRP_GET((parm)->nextAvailablePuddle, J9PoolPuddle*)

#ifdef __cplusplus
}
#endif

#endif /* J9POOL_H */
