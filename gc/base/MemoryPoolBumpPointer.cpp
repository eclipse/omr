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
	MM_MemoryPoolBumpPointer *memoryPool = (MM_MemoryPoolBumpPointer *)env->getForge()->allocate(sizeof(MM_MemoryPoolBumpPointer), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
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

	_scannableBytes = 0;
	_nonScannableBytes = 0;

	_allocatePointer = _topPointer;
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

MMINLINE void
MM_MemoryPoolBumpPointer::internalRecycleHeapChunk(void *addrBase, void *addrTop)
{
	/* Determine if the heap chunk belongs in the free list */
	uintptr_t freeEntrySize = ((uintptr_t)addrTop) - ((uintptr_t)addrBase);
	Assert_MM_true((uintptr_t)addrTop >= (uintptr_t)addrBase);
	MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, freeEntrySize);
}

bool
MM_MemoryPoolBumpPointer::abandonHeapChunk(void *addrBase, void *addrTop)
{
	Assert_MM_true(addrTop >= addrBase);
	internalRecycleHeapChunk(addrBase, addrTop);
	/* this memory pool doesn't maintain a free list, so always return false */
	return false;
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
