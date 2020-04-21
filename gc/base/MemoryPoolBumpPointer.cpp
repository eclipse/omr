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

#include "omrcfg.h"
#include "ModronAssertions.h"

#include "MemoryPoolBumpPointer.hpp"

#include "AllocateDescription.hpp"
#include "Bits.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "HeapRegionDescriptor.hpp"
#include "MemorySubSpace.hpp"

/**
 * Create and initialize a new instance of the receiver.
 */
MM_MemoryPoolBumpPointer *
MM_MemoryPoolBumpPointer::newInstance(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize)
{
	MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)env->getForge()->allocate(sizeof(MM_MemoryPoolBumpPointer), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != memoryPool) {
		memoryPool = new(memoryPool) MM_MemoryPoolBumpPointer(env, minimumFreeEntrySize);
		if (!memoryPool->initialize(env)) {
			memoryPool->kill(env);
			memoryPool = NULL;
		}
	}
	return memoryPool;
}

bool
MM_MemoryPoolBumpPointer::initialize(MM_EnvironmentBase *env)
{
	if(!MM_MemoryPool::initialize(env)) {
		return false;
	}

	/*
	 * Create Sweep Pool State for MPAOL
	 */
	MM_Collector* globalCollector = _extensions->getGlobalCollector();
	Assert_MM_true(NULL != globalCollector);

	_sweepPoolState = static_cast<MM_SweepPoolState*>(globalCollector->createSweepPoolState(env, this));
	if (NULL == _sweepPoolState) {
		return false;
	}

	/* Get the shared _sweepPoolManager instance for MemoryPoolBumpPointer */
	_sweepPoolManager = env->getExtensions()->sweepPoolManagerBumpPointer;

	return true;
}

void
MM_MemoryPoolBumpPointer::tearDown(MM_EnvironmentBase *env)
{
	MM_MemoryPool::tearDown(env);

	if (NULL != _sweepPoolState) {
		MM_Collector* globalCollector = _extensions->getGlobalCollector();
		Assert_MM_true(NULL != globalCollector);
		globalCollector->deleteSweepPoolState(env, _sweepPoolState);
	}
}

/****************************************
 * Allocation
 ****************************************
 */
MMINLINE void *
MM_MemoryPoolBumpPointer::internalAllocate(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired)
{
	Assert_MM_objectAligned(env, sizeInBytesRequired);
	void *result = NULL;
	uintptr_t spaceRemaining = (uintptr_t)_topPointer - (uintptr_t)_allocatePointer;
	if (sizeInBytesRequired <= spaceRemaining) {
		result = _allocatePointer;
		_allocatePointer = (void *)((uintptr_t)_allocatePointer + sizeInBytesRequired);
		uintptr_t newFreeSpace = spaceRemaining - sizeInBytesRequired;
		if (0 == newFreeSpace) {
			_freeEntryCount = 0;
		} else {
			_freeEntryCount = 1;
		}
		_largestFreeEntry = newFreeSpace;
		Assert_MM_true(_allocatePointer <= _topPointer);
	}
	return result;
}

void *
MM_MemoryPoolBumpPointer::allocateObject(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription)
{
	void * addr = internalAllocate(env, allocDescription->getContiguousBytes());

	if (addr != NULL) {
#if defined(OMR_GC_ALLOCATION_TAX)
		if(env->getExtensions()->payAllocationTax) {
			allocDescription->setAllocationTaxSize(allocDescription->getBytesRequested());
		}
#endif  /* OMR_GC_ALLOCATION_TAX */
		allocDescription->setTLHAllocation(false);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return addr;
}

MMINLINE bool
MM_MemoryPoolBumpPointer::internalAllocateTLH(MM_EnvironmentBase *env, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop)
{
	bool success = false;
	uintptr_t spaceRemaining = (uintptr_t)_topPointer - (uintptr_t)_allocatePointer;
	if (_minimumFreeEntrySize <= spaceRemaining) {
		addrBase = _allocatePointer;
		uintptr_t sizeToAllocate = 0;
		if (maximumSizeInBytesRequired < spaceRemaining) {
			sizeToAllocate = maximumSizeInBytesRequired;
		} else {
			sizeToAllocate = spaceRemaining;
		}
		_allocatePointer = (void *)((uintptr_t)_allocatePointer + sizeToAllocate);
		addrTop = _allocatePointer;
		uintptr_t newFreeSpace = spaceRemaining - sizeToAllocate;
		if (newFreeSpace < _minimumFreeEntrySize) {
			/* We are trying to allocate a TLH which is permitted to be larger than the requested size.  If we were going to discard
			 * the remainder of the region, anyway, return it as part of the TLH.
			 */
			addrTop = _topPointer;
			newFreeSpace = 0;
			_freeEntryCount = 0;
			_allocatePointer = _topPointer;
		} else {
			_freeEntryCount = 1;
		}
		_largestFreeEntry = newFreeSpace;

		success = true;
	}
	return success;
}

void *
MM_MemoryPoolBumpPointer::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription,
											uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop)
{
	void *tlhBase = NULL;

	if (internalAllocateTLH(env, maximumSizeInBytesRequired, addrBase, addrTop)) {
		tlhBase = addrBase;
	}

	if (NULL != tlhBase) {
#if defined(OMR_GC_ALLOCATION_TAX)
		if(env->getExtensions()->payAllocationTax) {
			allocDescription->setAllocationTaxSize((uint8_t *)addrTop - (uint8_t *)addrBase);
		}
#endif  /* OMR_GC_ALLOCATION_TAX */

		allocDescription->setTLHAllocation(true);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return tlhBase;
}

void *
MM_MemoryPoolBumpPointer::collectorAllocate(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription, bool lockingRequired)
{
	void * addr = internalAllocate(env, allocDescription->getContiguousBytes());

	if (NULL != addr) {
		allocDescription->setTLHAllocation(false);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return addr;
}

void *
MM_MemoryPoolBumpPointer::collectorAllocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired)
{
	void *tlhBase = NULL;

	if (internalAllocateTLH(env, maximumSizeInBytesRequired, addrBase, addrTop)) {
		tlhBase = addrBase;
	}

	if (NULL != tlhBase) {
		allocDescription->setTLHAllocation(true);
		allocDescription->setMemoryPool(this);
	}

	return tlhBase;
}

/****************************************
 * Free list building
 ****************************************
 */

void
MM_MemoryPoolBumpPointer::reset(Cause cause)
{
	/* Call superclass first .. */
	MM_MemoryPool::reset(cause);

	_heapFreeList = (MM_HeapLinkedFreeHeader *)NULL;
	_scannableBytes = 0;
	_nonScannableBytes = 0;

	_allocatePointer = _topPointer;
	_lastFreeEntry = NULL;
}

/**
 * As opposed to reset, which will empty out, this will fill out as if everything is free.
 * Returns the freelist entry created at the end of the given region
 */
MM_HeapLinkedFreeHeader *
MM_MemoryPoolBumpPointer::rebuildFreeListInRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, MM_HeapLinkedFreeHeader *previousFreeEntry)
{
	Assert_MM_true(0 == _darkMatterBytes);
	Assert_MM_true(0 == _scannableBytes);
	Assert_MM_true(0 == _nonScannableBytes);

	_allocatePointer = region->getLowAddress();
	uintptr_t newFreeSpace = (uintptr_t)_topPointer - (uintptr_t)_allocatePointer;
	_freeMemorySize = newFreeSpace;
	_freeEntryCount = 1;
	_largestFreeEntry = newFreeSpace;
	return (MM_HeapLinkedFreeHeader *)_allocatePointer;
}

void
MM_MemoryPoolBumpPointer::setAllocationPointer(MM_EnvironmentBase *env, void *allocatePointer)
{
	_allocatePointer = allocatePointer;
}

/**
 * Add the range of memory to the free list of the receiver.
 *
 * @param expandSize Number of bytes to remove from the memory pool
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 *
 * @todo check if this routine can eliminate any of the other similar routines.
 *
 */
void
MM_MemoryPoolBumpPointer::expandWithRange(MM_EnvironmentBase *env, uintptr_t expandSize, void *lowAddress, void *highAddress, bool canCoalesce)
{
	_allocatePointer = lowAddress;
	_topPointer = highAddress;
	uintptr_t newFreeSpace = (uintptr_t)_topPointer - (uintptr_t)_allocatePointer;
	/* we are currently assuming that we are expanding by full regions at a time */
	Assert_MM_true(env->getExtensions()->regionSize == newFreeSpace);
	_freeMemorySize = newFreeSpace;
	_freeEntryCount = 1;
	_largestFreeEntry = newFreeSpace;
}

/**
 * Remove the range of memory to the free list of the receiver.
 *
 * @param expandSize Number of bytes to remove from the memory pool
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 *
 * @note The expectation is that the range consists ONLY of free elements (no live data appears).
 *
 */
void *
MM_MemoryPoolBumpPointer::contractWithRange(MM_EnvironmentBase *env, uintptr_t contractSize, void *lowAddress, void *highAddress)
{
	return NULL;
}

bool
MM_MemoryPoolBumpPointer::abandonHeapChunk(void *addrBase, void *addrTop)
{
	Assert_MM_true(addrTop >= addrBase);
	internalRecycleHeapChunk(addrBase, addrTop, NULL);
	/* this memory pool doesn't maintain a free list, so always return false */
	return false;
}

MMINLINE bool
MM_MemoryPoolBumpPointer::internalRecycleHeapChunk(void *addrBase, void *addrTop, MM_HeapLinkedFreeHeader *next)
{
	/* Determine if the heap chunk belongs in the free list */
	bool const compressed = compressObjectReferences();
	uintptr_t freeEntrySize = ((uintptr_t)addrTop) - ((uintptr_t)addrBase);
	Assert_MM_true((uintptr_t)addrTop >= (uintptr_t)addrBase);

	MM_HeapLinkedFreeHeader *freeEntry = MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, freeEntrySize, compressed);
	if ((NULL != freeEntry) && (freeEntrySize >= _minimumFreeEntrySize)) {
		Assert_MM_true(freeEntry == addrBase);
		Assert_MM_true((NULL == next) || (freeEntry < next));
		freeEntry->setNext(next, compressed);
		return true;
	} else {
		return false;
	}
}

MM_SweepPoolManager *
MM_MemoryPoolBumpPointer::getSweepPoolManager()
{
	/*
	 * This function must be called for leaf pools only
	 * The failure at this line means that superpool has been used as leaf pool
	 * or leaf pool has problem with creation (like early initialization)
	 */
	Assert_MM_true(NULL != _sweepPoolManager);
	return _sweepPoolManager;
}

void 
MM_MemoryPoolBumpPointer::recalculateMemoryPoolStatistics(MM_EnvironmentBase *env)
{
	uintptr_t freeBytes = (uintptr_t)_topPointer - (uintptr_t)_allocatePointer;
	setFreeMemorySize(freeBytes);
	setFreeEntryCount(1);
	setLargestFreeEntry(freeBytes);
}

void
MM_MemoryPoolBumpPointer::rewindAllocationPointerTo(void *pointer)
{
	Assert_MM_true(pointer < _allocatePointer);
	_allocatePointer = pointer;
}

void
MM_MemoryPoolBumpPointer::alignAllocationPointer(uintptr_t alignmentMultiple)
{
	if (_allocatePointer < _topPointer) {
		Assert_MM_true(1 == MM_Bits::populationCount(alignmentMultiple));
		uintptr_t alignmentMask = alignmentMultiple - 1;
		uintptr_t newSum = (uintptr_t)_allocatePointer + alignmentMask;
		uintptr_t alignedValue = newSum & ~alignmentMask;
		void* newAllocatePointer = OMR_MIN((void *)alignedValue, _topPointer);
		_allocatePointer = newAllocatePointer;
	}
}

/* (non-doxygen)
 * @see MM_MemoryPool::createFreeEntry()
 */
bool
MM_MemoryPoolBumpPointer::createFreeEntry(MM_EnvironmentBase* env, void* addrBase, void* addrTop,
													  MM_HeapLinkedFreeHeader* previousFreeEntry, MM_HeapLinkedFreeHeader* nextFreeEntry)
{
	bool const compressed = compressObjectReferences();

	if (internalRecycleHeapChunk(addrBase, addrTop, nextFreeEntry)) {

		/* The range is big enough for the free list, so link the previous to it */
		if (previousFreeEntry) {
			previousFreeEntry->setNext((MM_HeapLinkedFreeHeader*)addrBase, compressed);
		}

		if ((uintptr_t) addrBase > (uintptr_t) getLastFreeEntry()) {
			setLastFreeEntry(addrBase);
		}
		return true;
	}

	/* The range was not big enough for the free list, so link previous to next */
	if (previousFreeEntry) {
		previousFreeEntry->setNext(nextFreeEntry, compressed);
	}
	return false;
}




/* (non-doxygen)
 * @see MM_MemoryPool::createFreeEntry()
 */
bool
MM_MemoryPoolBumpPointer::createFreeEntry(MM_EnvironmentBase* env, void* addrBase, void* addrTop)
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
MM_MemoryPoolBumpPointer::connectInnerMemoryToPool(MM_EnvironmentBase* env, void* address, uintptr_t size, void* previousFreeEntry)
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
MM_MemoryPoolBumpPointer::connectOuterMemoryToPool(MM_EnvironmentBase* env, void* address, uintptr_t size, void* nextFreeEntry)
{
	Assert_MM_true((NULL == nextFreeEntry) || (address < nextFreeEntry));
	Assert_MM_true((NULL == address) || (size >= getMinimumFreeEntrySize()));

	/* Build the free header */
	createFreeEntry(env, (MM_HeapLinkedFreeHeader*)address, (uint8_t*)address + size, NULL, (MM_HeapLinkedFreeHeader*)nextFreeEntry);

	if (NULL == _heapFreeList) {
		_heapFreeList = (MM_HeapLinkedFreeHeader*)nextFreeEntry;
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
MM_MemoryPoolBumpPointer::connectFinalMemoryToPool(MM_EnvironmentBase* env, void* address, uintptr_t size)
{
	Assert_MM_true((NULL == address) || (size >= getMinimumFreeEntrySize()));

	/* Build the free header */
	createFreeEntry(env, (MM_HeapLinkedFreeHeader*)address, (uint8_t*)address + size);
}

