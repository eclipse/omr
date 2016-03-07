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
 ******************************************************************************/


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
	/* Sanity checks -- should never create an out of order link */
	assume0((NULL == nextFreeEntry) || (nextFreeEntry > addrTop));

	if (internalRecycleHeapChunk(addrBase, addrTop, nextFreeEntry)) {
		/* The range is big enough for the free list, so link the previous to it */
		if (previousFreeEntry) {
			assume0(previousFreeEntry < addrBase);
			previousFreeEntry->setNext((MM_HeapLinkedFreeHeader*)addrBase);
		}
		return true;
	}

	/* The range was not big enough for the free list, so link previous to next */
	if (previousFreeEntry) {
		assume0((NULL == nextFreeEntry) || (nextFreeEntry > previousFreeEntry));
		previousFreeEntry->setNext(nextFreeEntry);
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
	abandonHeapChunk((MM_HeapLinkedFreeHeader*)address, (uint8_t*)address + size);
}

