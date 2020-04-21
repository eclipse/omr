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


/**
 * @file
 * @ingroup GC_Base_Core
 */

#include "MemoryPoolSplitAddressOrderedListBase.hpp"

#include "omrcfg.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include <string.h>

#include "MemoryPoolSplitAddressOrderedList.hpp"

#include "AllocateDescription.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepHeapSectioning.hpp"
#include "SweepPoolState.hpp"

bool
J9ModronFreeList::initialize(MM_EnvironmentBase* env)
{
	if (!_lock.initialize(env, &env->getExtensions()->lnrlOptions, "J9ModronFreeList:_lock")) {
		return false;
	}

	_freeList = NULL;

	/* Link up the inactive hints */
	J9ModronAllocateHint* inactiveHint = (J9ModronAllocateHint*)_hintStorage;
	J9ModronAllocateHint* previousInactiveHint = NULL;
	uintptr_t inactiveCount = HINT_ELEMENT_COUNT;

	while (inactiveCount) {
		inactiveHint->next = previousInactiveHint;
		previousInactiveHint = inactiveHint;
		inactiveHint += 1;
		inactiveCount -= 1;
	}
	_hintInactive = previousInactiveHint;

	return true;
}

void
J9ModronFreeList::tearDown()
{
	_lock.tearDown();
}

/****************************************
 * Hint Functionality
 ****************************************
 */
void
J9ModronFreeList::clearHints()
{
	J9ModronAllocateHint* activeHint = _hintActive;
	J9ModronAllocateHint* nextActiveHint = NULL;
	J9ModronAllocateHint* inactiveHint = _hintInactive;

	while (activeHint) {
		nextActiveHint = activeHint->next;
		activeHint->next = inactiveHint;
		inactiveHint = activeHint;
		activeHint = nextActiveHint;
	}
	_hintInactive = inactiveHint;
	_hintActive = NULL;
	_hintLru = 1;
}

void J9ModronFreeList::reset()
{
	_freeList = NULL;
	_freeSize = 0;
	_freeCount = 0;
	_timesLocked = 0;
	clearHints();
}

bool
MM_MemoryPoolSplitAddressOrderedListBase::initialize(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if (!MM_MemoryPool::initialize(env)) {
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

	/*
	 * Get Sweep Pool Manager for MPAOL
	 * For platforms it doesn't required still be NULL
	 */
	_sweepPoolManager = extensions->sweepPoolManagerSmallObjectArea;

	_currentThreadFreeList = (uintptr_t*)extensions->getForge()->allocate(sizeof(uintptr_t) * _heapFreeListCount, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _currentThreadFreeList) {
		return false;
	} else {
		for (uintptr_t i = 0; i < _heapFreeListCount; ++i) {
			_currentThreadFreeList[i] = 0;
		}
	}

	_heapFreeLists = (J9ModronFreeList*)extensions->getForge()->allocate(sizeof(J9ModronFreeList) * _heapFreeListCountExtended, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _heapFreeLists) {
		return false;
	} else {
		for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
			_heapFreeLists[i] = J9ModronFreeList();
			if (!_heapFreeLists[i].initialize(env)) {
				return false;
			}
		}
		_referenceHeapFreeList = &(_heapFreeLists[0]._freeList);
	}

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* this memoryPool can be used by scavenger, maximum tlh size should be max(_extensions->tlhMaximumSize, _extensions->scavengerScanCacheMaximumSize) */
	uintptr_t tlhMaximumSize = OMR_MAX(_extensions->tlhMaximumSize, _extensions->scavengerScanCacheMaximumSize);
#else /* OMR_GC_MODRON_SCAVENGER */
	uintptr_t tlhMaximumSize = _extensions->tlhMaximumSize;
#endif /* OMR_GC_MODRON_SCAVENGER */
	/* set multiple factor = 2 for doubling _maxVeryLargeEntrySizes to avoid run out of _veryLargeEntryPool (minus count during decrement) */
	_largeObjectAllocateStats = MM_LargeObjectAllocateStats::newInstance(env, (uint16_t)extensions->largeObjectAllocationProfilingTopK, extensions->largeObjectAllocationProfilingThreshold, extensions->largeObjectAllocationProfilingVeryLargeObjectThreshold, (float)extensions->largeObjectAllocationProfilingSizeClassRatio / (float)100.0,
																		 _extensions->heap->getMaximumMemorySize(), tlhMaximumSize + _minimumFreeEntrySize, _extensions->tlhMinimumSize, 2);

	if (NULL == _largeObjectAllocateStats) {
		return false;
	}

	/* If we ever subclass MM_LargeObjectAllocateStats we will have to allocate this as an array of pointers to MM_LargeObjectAllocateStats and invoke newInstance instead of just initialize
	 * Than, we may also need to virtualize initialize() and tearDown().
	 */
	_largeObjectAllocateStatsForFreeList = (MM_LargeObjectAllocateStats*)extensions->getForge()->allocate(sizeof(MM_LargeObjectAllocateStats) * _heapFreeListCountExtended, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

	if (NULL == _largeObjectAllocateStatsForFreeList) {
		return false;
	} else {
		for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
			new (&_largeObjectAllocateStatsForFreeList[i]) MM_LargeObjectAllocateStats(env);

			/* set multiple factor = 2 for doubling _maxVeryLargeEntrySizes to avoid run out of _veryLargeEntryPool (minus count during decrement) */
			if (!_largeObjectAllocateStatsForFreeList[i].initialize(env, (uint16_t)extensions->largeObjectAllocationProfilingTopK, extensions->largeObjectAllocationProfilingThreshold, extensions->largeObjectAllocationProfilingVeryLargeObjectThreshold, (float)extensions->largeObjectAllocationProfilingSizeClassRatio / (float)100.0,
																	_extensions->heap->getMaximumMemorySize(), tlhMaximumSize + _minimumFreeEntrySize, _extensions->tlhMinimumSize, 2)) {
				return false;
			}
		}
	}

	/* At this moment we do not know who is creator of this pool, so we do not set _largeObjectCollectorAllocateStatsForFreeList yet.
	 * Tenure SubSpace for Gencon will set _largeObjectCollectorAllocateStatsForFreeList to _largeObjectCollectorAllocateStatsForFreeList (we append collector stats to mutator stats)
	 * Tenure SubSpace for Flat will leave _largeObjectCollectorAllocateStatsForFreeList at NULL (no interest in collector stats)
	 * We do not even need _largeObjectCollectorAllocateStats here (as we do in plain MPAOL) - we'll just merge whatever _largeObjectCollectorAllocateStatsForFreeList contatins (appended Collector stats or not)
	 */

	if (!_resetLock.initialize(env, &extensions->lnrlOptions, "MM_MemoryPoolSplitAddressOrderedList:_resetLock")) {
		return false;
	}

	return true;
}

void
MM_MemoryPoolSplitAddressOrderedListBase::tearDown(MM_EnvironmentBase* env)
{
	MM_MemoryPool::tearDown(env);

	if (NULL != _sweepPoolState) {
		MM_Collector* globalCollector = _extensions->getGlobalCollector();
		Assert_MM_true(NULL != globalCollector);
		globalCollector->deleteSweepPoolState(env, _sweepPoolState);
	}

	if (NULL != _heapFreeLists) {
		for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
			_heapFreeLists[i].tearDown();
		}
	}

	MM_GCExtensionsBase* extensions = env->getExtensions();
	extensions->getForge()->free(_heapFreeLists);
	extensions->getForge()->free(_currentThreadFreeList);

	if (NULL != _largeObjectAllocateStats) {
		_largeObjectAllocateStats->kill(env);
		_largeObjectAllocateStats = NULL;
	}

	if (NULL != _largeObjectAllocateStatsForFreeList) {
		for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
			_largeObjectAllocateStatsForFreeList[i].tearDown(env);
		}
		extensions->getForge()->free(_largeObjectAllocateStatsForFreeList);
		_largeObjectAllocateStatsForFreeList = NULL;
	}

	_largeObjectCollectorAllocateStatsForFreeList = NULL;

	_resetLock.tearDown();
}

void*
MM_MemoryPoolSplitAddressOrderedListBase::allocateObject(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	void* addr = internalAllocate(env, allocDescription->getContiguousBytes(), true, _largeObjectAllocateStatsForFreeList);

	if (addr != NULL) {
		if (env->getExtensions()->payAllocationTax) {
			allocDescription->setAllocationTaxSize(allocDescription->getBytesRequested());
		}
		allocDescription->setTLHAllocation(false);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return addr;
}

void*
MM_MemoryPoolSplitAddressOrderedListBase::collectorAllocate(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, bool lockingRequired)
{
	void* addr = internalAllocate(env, allocDescription->getContiguousBytes(), lockingRequired, _largeObjectCollectorAllocateStatsForFreeList);

	if (addr != NULL) {
		allocDescription->setTLHAllocation(false);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return addr;
}

void*
MM_MemoryPoolSplitAddressOrderedListBase::allocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription,
												  uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop)
{
	void* tlhBase = NULL;

	if (internalAllocateTLH(env, maximumSizeInBytesRequired, addrBase, addrTop, true, _largeObjectAllocateStatsForFreeList)) {
		tlhBase = addrBase;
	}

	if (NULL != tlhBase) {
		if (env->getExtensions()->payAllocationTax) {
			allocDescription->setAllocationTaxSize((uint8_t*)addrTop - (uint8_t*)addrBase);
		}

		allocDescription->setTLHAllocation(true);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return tlhBase;
}

void*
MM_MemoryPoolSplitAddressOrderedListBase::collectorAllocateTLH(MM_EnvironmentBase* env,
														   MM_AllocateDescription* allocDescription, uintptr_t maximumSizeInBytesRequired,
														   void*& addrBase, void*& addrTop, bool lockingRequired)
{
	void* base = NULL;
	if (internalAllocateTLH(env, maximumSizeInBytesRequired, addrBase, addrTop, lockingRequired, _largeObjectCollectorAllocateStatsForFreeList)) {
		base = addrBase;
		allocDescription->setTLHAllocation(true);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}
	return base;
}

/****************************************
 * Free list building
 ****************************************
 */

void
MM_MemoryPoolSplitAddressOrderedListBase::reset(Cause cause)
{
	/* Call superclass first .. */
	MM_MemoryPool::reset(cause);

	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		_heapFreeLists[i].reset();

		/* initialize frequent allocates for each free list based on cummulative large object stats for the pool */
		resetFreeEntryAllocateStats(&_largeObjectAllocateStatsForFreeList[i]);
	}
	_lastFreeEntry = NULL;
	resetFreeEntryAllocateStats(_largeObjectAllocateStats);
	resetLargeObjectAllocateStats();
}

/**
 * As opposed to reset, which will empty out, this will fill out as if everything is free.
 * Returns the freelist entry created at the end of the given region
 */
MM_HeapLinkedFreeHeader*
MM_MemoryPoolSplitAddressOrderedListBase::rebuildFreeListInRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region, MM_HeapLinkedFreeHeader* previousFreeEntry)
{
	MM_HeapLinkedFreeHeader* newFreeEntry = NULL;
	void* rangeBase = region->getLowAddress();
	void* rangeTop = region->getHighAddress();
	uintptr_t rangeSize = region->getSize();

	/* This may be called while VM running (indirectly from JCL
	 * to create RTJ Scoped Memory, so we need proper locking
	 */
	acquireResetLock(env);
	lock(env);
	reset();

	/* TODO 108399 Determine whether this is still necessary (OMR_SCAVENGER_DEBUG is defined in Scavenger.cpp and is not accessible here) */
#if defined(OMR_SCAVENGER_DEBUG)
	/* Fill the new space with a bogus value */
	memset(rangeBase, 0xFA, rangeSize);
#endif /* OMR_SCAVENGER_DEBUG */

	/* Segment list is already address ordered */
	if (createFreeEntry(env, rangeBase, rangeTop, previousFreeEntry, NULL)) {
		newFreeEntry = (MM_HeapLinkedFreeHeader*)rangeBase;

		/* Adjust the free memory data */
		_heapFreeLists[0]._freeSize = rangeSize;
		_heapFreeLists[0]._freeCount = 1;
		_heapFreeLists[0]._freeList = newFreeEntry;
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(rangeSize);

		TRIGGER_J9HOOK_MM_PRIVATE_REBUILD_FREE_LIST(env->getExtensions()->privateHookInterface, env->getOmrVMThread(), rangeBase, rangeTop);
	}
	unlock(env);
	releaseResetLock(env);

	return newFreeEntry;
}


bool
MM_MemoryPoolSplitAddressOrderedListBase::printFreeListValidity(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	bool result = true;
	bool const compressed = compressObjectReferences();

	omrtty_printf("----- START SPLIT FREE LIST VALIDITY FOR 0x%p -----\n", this);
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		if ( _heapFreeListCount == i ) {
			omrtty_printf("--- Reserved Free List ---\n");
		}
		bool listIsSane = true;
		uintptr_t calculatedSize = 0;
		uintptr_t calculatedHoles = 0;
		MM_HeapLinkedFreeHeader* currentFreeEntry = _heapFreeLists[i]._freeList;
		MM_HeapLinkedFreeHeader* previousFreeEntry = currentFreeEntry;
		while (NULL != currentFreeEntry) {
			listIsSane = listIsSane && ((NULL == currentFreeEntry->getNext(compressed)) || (currentFreeEntry < currentFreeEntry->getNext(compressed)));
			calculatedSize += currentFreeEntry->getSize();
			++calculatedHoles;
			previousFreeEntry = currentFreeEntry;
			currentFreeEntry = currentFreeEntry->getNext(compressed);
		}
		omrtty_printf("  -- Free List %4zu (head: 0x%p, tail: 0x%p, expected size: %16zu, expected holes: %16zu): ", i, _heapFreeLists[i]._freeList, previousFreeEntry, _heapFreeLists[i]._freeSize, _heapFreeLists[i]._freeCount);
		listIsSane = listIsSane && (calculatedSize == _heapFreeLists[i]._freeSize) && (calculatedHoles == _heapFreeLists[i]._freeCount);
		if (listIsSane) {
			omrtty_printf("VALID\n");
		} else {
			omrtty_printf("INVALID (calculated size: %16zu, calculated holes: %16zu)\n", calculatedSize, calculatedHoles);
		}
		result = result && listIsSane;
	}
	omrtty_printf("----- END SPLIT FREE LIST VALIDITY FOR 0x%p: %s -----\n", this, (result ? "VALID" : "INVALID"));

	return result;
}

#if defined(DEBUG)
bool
MM_MemoryPoolSplitAddressOrderedListBase::isValidListOrdering()
{
	bool const compressed = compressObjectReferences();
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		MM_HeapLinkedFreeHeader* walk = _heapFreeLists[i]._freeList;
		while (NULL != walk) {
			if ((NULL != walk->getNext(compressed)) && (walk >= walk->getNext(compressed))) {
				return false;
			}
			walk = walk->getNext(compressed);
		}
	}
	return true;
}
#endif

/**
 * Determine the address in this memoryPool where there is at least sizeRequired free bytes in free entries.
 * The free entries must be of size minimumSize to be counted.
 *
 * @param sizeRequired Amount of free memory to be returned
 * @param minimumSize Minimum size of free entry we are interested in
 *
 * @return address of memory location which has sizeRequired free bytes available in this memoryPool.
 * @return NULL if this memory pool can not satisfy sizeRequired bytes.
 */
void*
MM_MemoryPoolSplitAddressOrderedListBase::findAddressAfterFreeSize(MM_EnvironmentBase* env, uintptr_t sizeRequired, uintptr_t minimumSize)
{
	uintptr_t remainingBytesNeeded = sizeRequired;
	uintptr_t currentFreeListIndex;
	MM_HeapLinkedFreeHeader* currentFreeEntry = (MM_HeapLinkedFreeHeader*)getFirstFreeStartingAddr(env, &currentFreeListIndex);

	/* Count full free entries until an entry needs to be split. */
	while (NULL != currentFreeEntry) {
		/* Removing entries has left us needing less than the minimum size of the entries.
		 * To ensure we get the size needed, bump up the remainingBytesNeeded.
		 */
		if (remainingBytesNeeded < minimumSize) {
			remainingBytesNeeded = minimumSize;
		}

		if (remainingBytesNeeded >= currentFreeEntry->getSize()) {
			/* Make sure the entry is big enough to include */
			if (minimumSize <= currentFreeEntry->getSize()) {
				remainingBytesNeeded -= currentFreeEntry->getSize();

				/* Did the last free entry satisfied required memory exactly? */
				if (0 == remainingBytesNeeded) {
					return (void*)currentFreeEntry->afterEnd();
				}
			}
		} else {
			/* This entry will be split */
			if (currentFreeEntry->getSize() - remainingBytesNeeded < _minimumFreeEntrySize) {
				/* The rest of the entry is too small to meet minimum size (for this pool), include it too */
				return (void*)currentFreeEntry->afterEnd();
			}
			return (void*)((uint8_t*)currentFreeEntry + remainingBytesNeeded);
		}

		currentFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &currentFreeListIndex);
	}

	return NULL;
}

bool
MM_MemoryPoolSplitAddressOrderedListBase::recycleHeapChunk(MM_EnvironmentBase* env, void* addrBase, void* addrTop,
													   MM_HeapLinkedFreeHeader* previousFreeEntry, MM_HeapLinkedFreeHeader* nextFreeEntry, uintptr_t curFreeList)
{
	bool const compressed = compressObjectReferences();
	Assert_MM_true(addrBase <= addrTop);
	Assert_MM_true((NULL == nextFreeEntry) || (addrTop <= nextFreeEntry));
	if (internalRecycleHeapChunk(addrBase, addrTop, nextFreeEntry)) {
		if (previousFreeEntry) {
			Assert_MM_true(previousFreeEntry < addrBase);
			previousFreeEntry->setNext((MM_HeapLinkedFreeHeader*)addrBase, compressed);
		} else {
			_heapFreeLists[curFreeList]._freeList = (MM_HeapLinkedFreeHeader*)addrBase;
		}

		return true;
	}

	if (previousFreeEntry) {
		Assert_MM_true((NULL == nextFreeEntry) || (previousFreeEntry < nextFreeEntry));
		previousFreeEntry->setNext(nextFreeEntry, compressed);
	} else {
		_heapFreeLists[curFreeList]._freeList = nextFreeEntry;
	}

	return false;
}

/**
 * @copydoc MM_MemoryPool::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *, MM_AllocateDescription *, void *, void *)
 */
uintptr_t
MM_MemoryPoolSplitAddressOrderedListBase::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, void* lowAddr, void* highAddr)
{
	MM_HeapLinkedFreeHeader* lastFree;

	lastFree = (MM_HeapLinkedFreeHeader*)findFreeEntryEndingAtAddr(env, highAddr);

	/* If matching free entry found */
	if (NULL == lastFree) {
		/* No free entry with matching end address so return 0 */
		return 0;
	}

	uintptr_t availableContractSize = lastFree->getSize();

	/* Is the last free element a candidate to satisfy the allocation request ?
	 * If so we have to assume it is the only free element that could satisfy the
	 * request and adjust the  maximumContractSize.
	 */
	uintptr_t allocSize;
	if (NULL != allocDescription) {
		allocSize = allocDescription->getContiguousBytes();
	} else {
		allocSize = 0;
	}
	if ((allocSize != 0) && (allocSize <= availableContractSize)) {
		availableContractSize -= allocSize;
	}

	return availableContractSize;
}

/**
 * Find the free list entry whos end address matches the parameter.
 *
 * @param addr Address to match against the high end of a free entry.
 *
 * @return The leading address of the free entry whose top matches addr.
 */
void*
MM_MemoryPoolSplitAddressOrderedListBase::findFreeEntryEndingAtAddr(MM_EnvironmentBase* env, void* addr)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader* currentFreeEntry;

	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		currentFreeEntry = _heapFreeLists[i]._freeList;

		while (currentFreeEntry) {
			if (((void*)currentFreeEntry->afterEnd()) == addr) {
				return currentFreeEntry;
			}

			/* Address ordered list - if the current entry is greater than the one we are looking for
			 * then there is no valid entry
			 */
			if ((void*)currentFreeEntry > addr) {
				break;
			}

			currentFreeEntry = currentFreeEntry->getNext(compressed);
		}
	}

	return NULL;
}

/**
 * Find the top of the free list entry whos start address matches the parameter.
 *
 * @param addr Address to match against the low end of a free entry.
 *
 * @return The trailing address of the free entry whos top matches addr.
 */
void*
MM_MemoryPoolSplitAddressOrderedListBase::findFreeEntryTopStartingAtAddr(MM_EnvironmentBase* env, void* addr)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader* currentFreeEntry;

	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		currentFreeEntry = _heapFreeLists[i]._freeList;

		while (currentFreeEntry) {
			if ((void*)currentFreeEntry == addr) {
				return (void*)currentFreeEntry->afterEnd();
			}

			/* Address ordered list - if the current entry is greater than the one we are looking for
			 * then there is no valid entry
			 */
			if ((void*)currentFreeEntry > addr) {
				break;
			}

			currentFreeEntry = currentFreeEntry->getNext(compressed);
		}
	}

	return NULL;
}

/**
 * Find the address of the first entry on free list entry.
 *
 * @return The address of head of free chain
 */
void*
MM_MemoryPoolSplitAddressOrderedListBase::getFirstFreeStartingAddr(MM_EnvironmentBase* env, uintptr_t* currentFreeListReturn)
{
	for (uintptr_t i = 0; i < _heapFreeListCount; ++i) {
		if (NULL != _heapFreeLists[i]._freeList) {
			if (NULL != currentFreeListReturn) {
				*currentFreeListReturn = i;
			}
			return _heapFreeLists[i]._freeList;
		}
	}

	if (NULL != currentFreeListReturn) {
		*currentFreeListReturn = _heapFreeListCount;
	}

	return NULL;
}

/**
 * Find the address of the first entry on free list entry.
 *
 * @return The address of head of free chain
 */
void*
MM_MemoryPoolSplitAddressOrderedListBase::getFirstFreeStartingAddr(MM_EnvironmentBase* env)
{
	return getFirstFreeStartingAddr(env, NULL);
}

/**
 * Find the address of the next entry on free list entry.
 * @param currentFreeListIndex will return the index of the free list containing the next free starting address but
 * ONLY if the free list index has changed from the current free address to the next free address.
 *
 * @return The address of next free entry or NULL
 */
void*
MM_MemoryPoolSplitAddressOrderedListBase::getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree, uintptr_t* currentFreeListIndex)
{
	Assert_MM_true(currentFree != NULL);
	bool const compressed = compressObjectReferences();
	if (NULL != ((MM_HeapLinkedFreeHeader*)currentFree)->getNext(compressed)) {
		return ((MM_HeapLinkedFreeHeader*)currentFree)->getNext(compressed);
	}
	uintptr_t startFreeListIndex = 0;
	if (currentFreeListIndex != NULL) {
		startFreeListIndex = *currentFreeListIndex;
		if (startFreeListIndex >= _heapFreeListCount) {
			startFreeListIndex = 0;
		} else if (_heapFreeLists[startFreeListIndex]._freeList > currentFree) {
			startFreeListIndex = 0;
		}
	}
	for (uintptr_t i = startFreeListIndex; i < _heapFreeListCount; ++i) {
		if ((uintptr_t)_heapFreeLists[i]._freeList > (uintptr_t)currentFree) {
			if (NULL != currentFreeListIndex) {
				*currentFreeListIndex = i;
			}
			return _heapFreeLists[i]._freeList;
		}
	}

	if (NULL != currentFreeListIndex) {
		*currentFreeListIndex = _heapFreeListCount;
	}

	return NULL;
}

/**
 * Find the address of the next entry on free list entry.
 *
 * @return The address of next free entry or NULL
 */
void*
MM_MemoryPoolSplitAddressOrderedListBase::getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree)
{
	return getNextFreeStartingAddr(env, currentFree, NULL);
}

/**
 * Move a chunk of heap from one location to another within the receivers owned regions.
 * This involves fixing up any free list information that may change as a result of an address change.
 *
 * @param srcBase Start address to move.
 * @param srcTop End address to move.
 * @param dstBase Start of destination address to move into.
 *
 */
void
MM_MemoryPoolSplitAddressOrderedListBase::moveHeap(MM_EnvironmentBase* env, void* srcBase, void* srcTop, void* dstBase)
{
	bool const compressed = compressObjectReferences();
	for (uintptr_t i = 0; i < _heapFreeListCount; ++i) {
		MM_HeapLinkedFreeHeader* currentFreeEntry, *previousFreeEntry;

		previousFreeEntry = NULL;
		currentFreeEntry = _heapFreeLists[i]._freeList;
		while (NULL != currentFreeEntry) {
			if (((void*)currentFreeEntry >= srcBase) && ((void*)currentFreeEntry < srcTop)) {
				MM_HeapLinkedFreeHeader* newFreeEntry;
				newFreeEntry = (MM_HeapLinkedFreeHeader*)((((uintptr_t)currentFreeEntry) - ((uintptr_t)srcBase)) + ((uintptr_t)dstBase));

				if (previousFreeEntry) {
					assume0(previousFreeEntry < newFreeEntry);
					previousFreeEntry->setNext(newFreeEntry, compressed);
				} else {
					_heapFreeLists[i]._freeList = newFreeEntry;
				}
			}
			previousFreeEntry = currentFreeEntry;
			currentFreeEntry = currentFreeEntry->getNext(compressed);
		}
	}
}


/**
 * Lock any free list information from use.
 */
void
MM_MemoryPoolSplitAddressOrderedListBase::lock(MM_EnvironmentBase* env)
{
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		_heapFreeLists[i]._lock.acquire();
	}
}

/**
 * Unlock any free list information for use.
 */
void
MM_MemoryPoolSplitAddressOrderedListBase::unlock(MM_EnvironmentBase* env)
{
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		_heapFreeLists[i]._lock.release();
	}
}

/*
 * Debug routine to dump details of the current state of the freelist
 */
void
MM_MemoryPoolSplitAddressOrderedListBase::printCurrentFreeList(MM_EnvironmentBase* env, const char* area)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	omrtty_printf("Analysis of %s freelist: \n", area);
	bool const compressed = compressObjectReferences();

	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		MM_HeapLinkedFreeHeader* currentFreeEntry = _heapFreeLists[i]._freeList;
		while (currentFreeEntry) {
			const char * msg = "Free chunk %p -> %p (%i) \n";
			if (_heapFreeListCount == i) {
				msg = "Reserved chunk %p -> %p (%i) \n";
			}
			omrtty_printf(msg,
						 currentFreeEntry,
						 currentFreeEntry->afterEnd(),
						 currentFreeEntry->getSize());
			currentFreeEntry = currentFreeEntry->getNext(compressed);
		}
	}
}

void
MM_MemoryPoolSplitAddressOrderedListBase::recalculateMemoryPoolStatistics(MM_EnvironmentBase* env)
{
	uintptr_t largestFreeEntry = 0;
	uintptr_t freeBytes = 0;
	uintptr_t freeEntryCount = 0;
	bool const compressed = compressObjectReferences();

	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		MM_HeapLinkedFreeHeader* freeHeader = _heapFreeLists[i]._freeList;
		while (NULL != freeHeader) {
			if (freeHeader->getSize() > largestFreeEntry) {
				largestFreeEntry = freeHeader->getSize();
			}
			freeBytes += freeHeader->getSize();
			freeEntryCount += 1;
			freeHeader = freeHeader->getNext(compressed);
		}
	}

	updateMemoryPoolStatistics(env, freeBytes, freeEntryCount, largestFreeEntry);
}

uintptr_t
MM_MemoryPoolSplitAddressOrderedListBase::getActualFreeMemorySize()
{
	uintptr_t result = 0;
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		result += _heapFreeLists[i]._freeSize;
	}
	return result;
}

uintptr_t
MM_MemoryPoolSplitAddressOrderedListBase::getActualFreeEntryCount()
{
	uintptr_t result = 0;
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		result += _heapFreeLists[i]._freeCount;
	}
	return result;
}

void
MM_MemoryPoolSplitAddressOrderedListBase::resetLargeObjectAllocateStats()
{
	_largeObjectAllocateStats->resetCurrent();
	_largeObjectAllocateStats->getTlhAllocSizeClassStats()->resetCounts();

	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		_largeObjectAllocateStatsForFreeList[i].resetCurrent();
		_largeObjectAllocateStatsForFreeList[i].getTlhAllocSizeClassStats()->resetCounts();
	}
}

void
MM_MemoryPoolSplitAddressOrderedListBase::mergeFreeEntryAllocateStats()
{
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		_largeObjectAllocateStats->getFreeEntrySizeClassStats()->merge(_largeObjectAllocateStatsForFreeList[i].getFreeEntrySizeClassStats());
		_largeObjectAllocateStatsForFreeList[i].getFreeEntrySizeClassStats()->resetCounts();
	}
	_largeObjectAllocateStats->getFreeEntrySizeClassStats()->mergeCountForVeryLargeEntries();
}

void
MM_MemoryPoolSplitAddressOrderedListBase::mergeTlhAllocateStats()
{
	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		_largeObjectAllocateStats->getTlhAllocSizeClassStats()->merge(_largeObjectAllocateStatsForFreeList[i].getTlhAllocSizeClassStats());
		_largeObjectAllocateStatsForFreeList[i].getTlhAllocSizeClassStats()->resetCounts();
	}
}

void
MM_MemoryPoolSplitAddressOrderedListBase::mergeLargeObjectAllocateStats()
{
	_largeObjectAllocateStats->resetCurrent();

	for (uintptr_t i = 0; i < _heapFreeListCountExtended; ++i) {
		_largeObjectAllocateStats->mergeCurrent(&_largeObjectAllocateStatsForFreeList[i]);
	}
}

void
MM_MemoryPoolSplitAddressOrderedListBase::appendCollectorLargeAllocateStats()
{
	_largeObjectCollectorAllocateStatsForFreeList = _largeObjectAllocateStatsForFreeList;
}
