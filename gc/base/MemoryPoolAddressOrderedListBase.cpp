/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#include "omrcomp.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include <string.h>

#include "MemoryPoolAddressOrderedList.hpp"

#include "AllocateDescription.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Collector.hpp"
#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"
//#include "mmhook_internal.h"
#include "HeapRegionDescriptor.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "Heap.hpp"

#if defined(OMR_VALGRIND_MEMCHECK)
#include "MemcheckWrapper.hpp"
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

/****************************************
 * Allocation
 ****************************************
 */

/****************************************
 * Free list building
 ****************************************
 */

void
MM_MemoryPoolAddressOrderedListBase::acquireResetLock(MM_EnvironmentBase* env)
{
	_resetLock.acquire();
}

void
MM_MemoryPoolAddressOrderedListBase::releaseResetLock(MM_EnvironmentBase* env)
{
	_resetLock.release();
}

/* (non-doxygen)
 * @see MM_MemoryPool::createFreeEntry()
 */
bool
MM_MemoryPoolAddressOrderedListBase::createFreeEntry(MM_EnvironmentBase* env, void* addrBase, void* addrTop,
													  MM_HeapLinkedFreeHeader* previousFreeEntry, MM_HeapLinkedFreeHeader* nextFreeEntry)
{
	bool const compressed = compressObjectReferences();

	/* Sanity checks -- should never create an out of order link */
	assume0((NULL == nextFreeEntry) || (nextFreeEntry > addrTop));

#if defined(OMR_VALGRIND_MEMCHECK)
	valgrindClearRange(env->getExtensions(),(uintptr_t) addrBase,(uintptr_t)addrTop - (uintptr_t)addrBase);
#endif /* defined(OMR_VALGRIND_MEMCHECK) */
	
	if (internalRecycleHeapChunk(addrBase, addrTop, nextFreeEntry)) {

		/* The range is big enough for the free list, so link the previous to it */
		if (previousFreeEntry) {
			assume0(previousFreeEntry < addrBase);
			previousFreeEntry->setNext((MM_HeapLinkedFreeHeader*)addrBase, compressed);
		}
		return true;
	}

	/* The range was not big enough for the free list, so link previous to next */
	if (previousFreeEntry) {
		assume0((NULL == nextFreeEntry) || (nextFreeEntry > previousFreeEntry));
		previousFreeEntry->setNext(nextFreeEntry, compressed);
	}
	return false;
}

/* (non-doxygen)
 * @see MM_MemoryPool::createFreeEntry()
 */
bool
MM_MemoryPoolAddressOrderedListBase::createFreeEntry(MM_EnvironmentBase* env, void* addrBase, void* addrTop)
{
	return createFreeEntry(env, addrBase, addrTop, NULL, NULL);
}

/**
 * Add inner chunk memory piece to pool ("Create")
 * 
 * @param address free memory start address
 * @param size free memory size in bytes
 * @param previousFreeEntry previous element of list (if any) this memory must be connected with
 * @return true if free memory element accepted
 */
bool
MM_MemoryPoolAddressOrderedListBase::connectInnerMemoryToPool(MM_EnvironmentBase* env, void* address, uintptr_t size, void* previousFreeEntry)
{
	bool result = false;

	if (size >= getMinimumFreeEntrySize()) {
		/* Build the free header */
		createFreeEntry(env, (MM_HeapLinkedFreeHeader*)address, (uint8_t*)address + size, (MM_HeapLinkedFreeHeader*)previousFreeEntry, NULL);
		result = true;
	}
	return result;
}


/**
 * Connect leading/trailing chunk free memory piece ("Connect")
 *
 * @param address free memory start address
 * @param size free memory size in bytes
 * @param nextFreeEntry next element of list (if any) this memory must be connected with
 */
void
MM_MemoryPoolAddressOrderedListBase::connectOuterMemoryToPool(MM_EnvironmentBase* env, void* address, uintptr_t size, void* nextFreeEntry)
{
	Assert_MM_true((NULL == nextFreeEntry) || (address < nextFreeEntry));
	Assert_MM_true((NULL == address) || (size >= getMinimumFreeEntrySize()));

	/* Build the free header */
	createFreeEntry(env, (MM_HeapLinkedFreeHeader*)address, (uint8_t*)address + size, NULL, (MM_HeapLinkedFreeHeader*)nextFreeEntry);

	if (NULL == *_referenceHeapFreeList) {
		*_referenceHeapFreeList = (MM_HeapLinkedFreeHeader*)nextFreeEntry;
	}
}

/**
 * Connect leading/trailing chunk free memory piece ("Finalize")
 * temporary - need be separated because of implementation of subpools
 * 
 * @param address free memory start address
 * @param size free memory size in bytes
 */
void
MM_MemoryPoolAddressOrderedListBase::connectFinalMemoryToPool(MM_EnvironmentBase* env, void* address, uintptr_t size)
{
	Assert_MM_true((NULL == address) || (size >= getMinimumFreeEntrySize()));

	/* Build the free header */
	createFreeEntry(env, (MM_HeapLinkedFreeHeader*)address, (uint8_t*)address + size);
}

/**
 * Abandon memory piece
 * 
 * @param memoryPool current memory pool
 * @param address free memory start address
 * @param size free memory size in bytes
 */
void
MM_MemoryPoolAddressOrderedListBase::abandonMemoryInPool(MM_EnvironmentBase* env, void* address, uintptr_t size)
{
#if defined(OMR_VALGRIND_MEMCHECK)
	valgrindClearRange(env->getExtensions(),(uintptr_t) address,size);
#endif /* defined(OMR_VALGRIND_MEMCHECK) */
	abandonHeapChunk((MM_HeapLinkedFreeHeader*)address, (uint8_t*)address + size);
}

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
uintptr_t
MM_MemoryPoolAddressOrderedListBase::releaseFreeEntryMemoryPages(MM_EnvironmentBase* env, MM_HeapLinkedFreeHeader* freeEntry)
{
	bool const compressed = compressObjectReferences();
	uintptr_t releasedMemory = 0;
	MM_HeapLinkedFreeHeader* currentFreeEntry = freeEntry;
	uintptr_t pageSize = env->getExtensions()->heap->getPageSize();
	while (NULL != currentFreeEntry) {
		/* skip entry less than page size */
		if (pageSize <= currentFreeEntry->getSize()) {
			uintptr_t addressBase = MM_Math::roundToCeiling(pageSize, (uintptr_t)currentFreeEntry + sizeof(MM_HeapLinkedFreeHeader));
			/* release/decommit memory after Header */
			uintptr_t totalFreePagesCount = (currentFreeEntry->getSize() - (addressBase - (uintptr_t)currentFreeEntry)) / pageSize;
			if (0 < totalFreePagesCount) {
				uintptr_t commitPagesCount = 0;
				uintptr_t decommitPagesCount = 0;
				if (0 < _extensions->idleMinimumFree) {
					commitPagesCount = totalFreePagesCount * _extensions->idleMinimumFree / 100;
				}
				decommitPagesCount = totalFreePagesCount - commitPagesCount;
				/* leave commited pages of memory aside header */
				addressBase += commitPagesCount * pageSize;
				/* now decommit pages of memory */
				if (0 < decommitPagesCount) {
					if (_extensions->heap->decommitMemory((void*)addressBase, decommitPagesCount * pageSize, NULL, currentFreeEntry->afterEnd())) {
						releasedMemory += decommitPagesCount * pageSize;
					}
				}
			}
		}
		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}
	return releasedMemory;
}
#endif
