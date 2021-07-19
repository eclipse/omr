/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#include "Heap.hpp"

#include "j9nongenerated.h"

#include "AllocationStats.hpp"
#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "HeapRegionManager.hpp"
#include "HeapStats.hpp"
#include "MemorySpace.hpp"
#include "ModronAssertions.h"

#include "mmhook_common.h"

/****************************************
 * Initialization
 ****************************************
 */

void
MM_Heap::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_Heap::initialize(MM_EnvironmentBase* env)
{
	return true;
}

void
MM_Heap::tearDown(MM_EnvironmentBase* env)
{
}

/**
 * Reset the largest free chunk of all memorySubSpaces to 0.
 */
void
MM_Heap::resetLargestFreeEntry()
{
	MM_MemorySpace* currentMemorySpace = _memorySpaceList;
	while (currentMemorySpace) {
		currentMemorySpace->resetLargestFreeEntry();
		currentMemorySpace = currentMemorySpace->getNext();
	}
}

void
MM_Heap::registerMemorySpace(MM_MemorySpace* memorySpace)
{
	if (_memorySpaceList) {
		_memorySpaceList->setPrevious(memorySpace);
	}
	memorySpace->setNext(_memorySpaceList);
	memorySpace->setPrevious(NULL);
	_memorySpaceList = memorySpace;
}

void
MM_Heap::unregisterMemorySpace(MM_MemorySpace* memorySpace)
{
	MM_MemorySpace* previous;
	MM_MemorySpace* next;
	previous = memorySpace->getPrevious();
	next = memorySpace->getNext();

	if (previous) {
		previous->setNext(next);
	} else {
		_memorySpaceList = next;
	}

	if (next) {
		next->setPrevious(previous);
	}
}

void
MM_Heap::systemGarbageCollect(MM_EnvironmentBase* env, uint32_t gcCode)
{
	getDefaultMemorySpace()->systemGarbageCollect(env, gcCode);
}

/**
 * Calculate the total amount of memory consumed by all memory space.
 * @return Total memory consumed by all memory spaces.
 */
uintptr_t
MM_Heap::getMemorySize()
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t memorySize;

	currentMemorySpace = _memorySpaceList;
	memorySize = 0;
	while (currentMemorySpace) {
		memorySize += currentMemorySpace->getCurrentSize();
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return memorySize;
}

/**
 * Get the sum of all free memory currently available for allocation in all memory spaces.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider defered work that may be done to increase current free memory stores.
 * @see getApproximateFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_Heap::getActualFreeMemorySize()
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t freeMemory;

	currentMemorySpace = _memorySpaceList;
	freeMemory = 0;
	while (currentMemorySpace) {
		freeMemory += currentMemorySpace->getActualFreeMemorySize();
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return freeMemory;
}

/**
 * Get the approximate sum of all free memory available for allocation in all memory spaces.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * @see getActualActiveFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_Heap::getApproximateFreeMemorySize()
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t freeMemory;

	currentMemorySpace = _memorySpaceList;
	freeMemory = 0;
	while (currentMemorySpace) {
		freeMemory += currentMemorySpace->getApproximateFreeMemorySize();
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return freeMemory;
}

uintptr_t
MM_Heap::getActiveMemorySize()
{
	return getActiveMemorySize(MEMORY_TYPE_OLD | MEMORY_TYPE_NEW);
}

uintptr_t
MM_Heap::getActiveMemorySize(uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t memory;

	currentMemorySpace = _memorySpaceList;
	memory = 0;
	while (currentMemorySpace) {
		memory += currentMemorySpace->getActiveMemorySize(includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return memory;
}

uintptr_t
MM_Heap::getActiveLOAMemorySize(uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t memory;

	currentMemorySpace = _memorySpaceList;
	memory = 0;
	while (currentMemorySpace) {
		memory += currentMemorySpace->getActiveLOAMemorySize(includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return memory;
}

uintptr_t
MM_Heap::getActiveSurvivorMemorySize(uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t memory;

	currentMemorySpace = _memorySpaceList;
	memory = 0;
	while (currentMemorySpace) {
		memory += currentMemorySpace->getActiveSurvivorMemorySize(includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return memory;
}

uintptr_t
MM_Heap::getApproximateActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t memory;

	currentMemorySpace = _memorySpaceList;
	memory = 0;
	while (currentMemorySpace) {
		memory += currentMemorySpace->getApproximateActiveFreeSurvivorMemorySize(includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return memory;
}


/**
 * Get the sum of all free memory currently available for allocation in all memory spaces.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider defered work that may be done to increase current free memory stores.
 * @see getApproximateActiveFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_Heap::getActualActiveFreeMemorySize()
{
	return getActualActiveFreeMemorySize(MEMORY_TYPE_OLD | MEMORY_TYPE_NEW);
}

/**
 * Get the sum of all free memory currently available for allocation in all memory subspaces of the specified type.
 * This call will return an accurate count of the current size of all free memory of the specified type.  It will not
 * consider defered work that may be done to increase current free memory stores.
 *
 * @see getApproximateActiveFreeMemorySize(uintptr_t)
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_Heap::getActualActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t freeMemory;

	currentMemorySpace = _memorySpaceList;
	freeMemory = 0;
	while (currentMemorySpace) {
		freeMemory += currentMemorySpace->getActualActiveFreeMemorySize(includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return freeMemory;
}

/**
 * Get the approximate sum of all free memory available for allocation in all memory spaces.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * @see getActualActiveFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_Heap::getApproximateActiveFreeMemorySize()
{
	return getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD | MEMORY_TYPE_NEW);
}

/**
 * Get the approximate sum of all free memory available for allocation in all memory subspaces of the specified type.
 * This call will return an estimated count of the current size of all free memory of the specified type.  Although this
 * estimate may be accurate, it will consider defered work that may be done to increase current free memory stores.
 *
 * @see getActualActiveFreeMemorySize(uintptr_t)
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_Heap::getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t freeMemory;

	currentMemorySpace = _memorySpaceList;
	freeMemory = 0;
	while (currentMemorySpace) {
		freeMemory += currentMemorySpace->getApproximateActiveFreeMemorySize(includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return freeMemory;
}

/**
 * Get the approximate sum of all free LOA memory available for allocation in all memory subspaces of the specified type.
 * This call will return an estimated count of the current size of all free memory of the specified type.  Although this
 * estimate may be accurate, it will consider defered work that may be done to increase current free memory stores.
 *
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_Heap::getApproximateActiveFreeLOAMemorySize(uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace;
	uintptr_t freeMemory;

	currentMemorySpace = _memorySpaceList;
	freeMemory = 0;
	while (currentMemorySpace) {
		freeMemory += currentMemorySpace->getApproximateActiveFreeLOAMemorySize(includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}

	return freeMemory;
}

void
MM_Heap::mergeHeapStats(MM_HeapStats* heapStats)
{
	mergeHeapStats(heapStats, (MEMORY_TYPE_OLD | MEMORY_TYPE_NEW));
}


void
MM_Heap::mergeHeapStats(MM_HeapStats* heapStats, uintptr_t includeMemoryType)
{
	MM_MemorySpace* currentMemorySpace = _memorySpaceList;

	while (currentMemorySpace) {
		currentMemorySpace->mergeHeapStats(heapStats, includeMemoryType);
		currentMemorySpace = currentMemorySpace->getNext();
	}
}

void
MM_Heap::resetHeapStatistics(bool globalCollect)
{
	MM_MemorySpace* currentMemorySpace;

	currentMemorySpace = _memorySpaceList;
	while (currentMemorySpace) {
		currentMemorySpace->resetHeapStatistics(globalCollect);
		currentMemorySpace = currentMemorySpace->getNext();
	}
}

/**
 * Reset all memory sub space information before a garbage collection.
 */
void
MM_Heap::resetSpacesForGarbageCollect(MM_EnvironmentBase* env)
{
	MM_MemorySpace* memorySpace;

	memorySpace = _memorySpaceList;
	while (memorySpace) {
		memorySpace->reset(env);
		memorySpace = memorySpace->getNext();
	}
}

/**
 * The heap has added a range of memory associated to the receiver or one of its children.
 * @note The low address is inclusive, the high address exclusive.
 */
bool
MM_Heap::heapAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	MM_GlobalCollector* globalCollector = extensions->getGlobalCollector();

	bool result = true;
	if (NULL != globalCollector) {
		result = globalCollector->heapAddRange(env, subspace, size, lowAddress, highAddress);
	}
	return result;
}

/**
 * The heap has removed a range of memory associated to the receiver or one of its children.
 * @note The low address is inclusive, the high address exclusive.
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 *
 */
bool
MM_Heap::heapRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress, void* lowValidAddress, void* highValidAddress)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	MM_GlobalCollector* globalCollector = extensions->getGlobalCollector();

	bool result = true;
	if (NULL != globalCollector) {
		result = globalCollector->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}
	return result;
}

/**
 * The heap has had its memory shuffled between memory subspaces and/or memory pools.
 */
void
MM_Heap::heapReconfigured(MM_EnvironmentBase* env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress)
{
	MM_GlobalCollector* globalCollector = env->getExtensions()->getGlobalCollector();

	if (NULL != globalCollector) {
		globalCollector->heapReconfigured(env, reason, subspace, lowAddress, highAddress);
	}
}

bool
MM_Heap::objectIsInGap(void* object)
{
	/* This is only used by the split heap to check if the given object lies inside the gap between committed regions of the heap.
	 * Most heap implementations should just inherit this implementation which returns false since they have no gaps.
	 */
	return false;
}

/**
 * Initialize the CommonGCStartData structure in preparation for reporting a GC start
 * event. This structure contains data which will be of interest to anyone listening
 * to any of the GC start events (e.g. ALLOCATION_FAILED, CONCURRENT or SYSTEM).
 * Generally, it included information about the current state of the heap and how
 * it changed since the last GC.
 */
void
MM_Heap::initializeCommonGCStartData(MM_EnvironmentBase* env, MM_CommonGCStartData* data)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	MM_HeapStats stats;

	mergeHeapStats(&stats, MEMORY_TYPE_OLD);
	initializeCommonGCData(env, &data->commonData);

	data->exclusiveAccessTime = env->getExclusiveAccessTime();
	data->meanExclusiveAccessIdleTime = env->getMeanExclusiveAccessIdleTime();
	data->lastResponder = env->getLastExclusiveAccessResponder();
	data->haltedThreads = env->getExclusiveAccessHaltedThreads();
	data->beatenByOtherThread = env->exclusiveAccessBeatenByOtherThread();

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	data->tlhAllocCount = extensions->allocationStats._tlhRefreshCountFresh;
	data->tlhAllocBytes = extensions->allocationStats._tlhAllocatedFresh;
	data->tlhRequestedBytes = extensions->allocationStats._tlhRequestedBytes;
#else
	data->tlhAllocCount = 0;
	data->tlhAllocBytes = 0;
	data->tlhRequestedBytes = 0;
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	data->nonTlhAllocCount = extensions->allocationStats._allocationCount;
	data->nonTlhAllocBytes = extensions->allocationStats._allocationBytes;
}

/**
 * Initialize the CommonGCEndtData structure in preparation for reporting a GC end
 * event. This structure contains data which will be of interest to anyone listening
 * to any of the GC end events (e.g. ALLOCATION_FAILED, CONCURRENT or SYSTEM).
 * Generally, it included information about the state of the heap after the GC has
 * finished.
 */
void
MM_Heap::initializeCommonGCEndData(MM_EnvironmentBase* env, MM_CommonGCEndData* data)
{
	MM_HeapStats stats;
	mergeHeapStats(&stats, MEMORY_TYPE_OLD);
	initializeCommonGCData(env, &data->commonData);
}

/**
 * Initialize the CommonGCData structure for GC start and end events
 */
struct MM_CommonGCData*
MM_Heap::initializeCommonGCData(MM_EnvironmentBase* env, struct MM_CommonGCData* data)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	data->nurseryFreeBytes = getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW);
	data->nurseryTotalBytes = getActiveMemorySize(MEMORY_TYPE_NEW);
	data->tenureFreeBytes = getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
	data->tenureTotalBytes = getActiveMemorySize(MEMORY_TYPE_OLD);
	data->loaEnabled = (extensions->largeObjectArea ? 1 : 0);
	data->tenureLOAFreeBytes = (extensions->largeObjectArea ? getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0);
	data->tenureLOATotalBytes = (extensions->largeObjectArea ? getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0);
	data->rememberedSetCount = extensions->getRememberedCount();
	data->immortalFreeBytes = 0;
	data->immortalTotalBytes = 0;

	return data;
}

/**
 * Determine how much of the heap is actually adjustable.
 * @note when using GenCon we can not adjust nursery space.
 * @param env
 * @return Size of the adjustable heap memory
 */
uintptr_t
MM_Heap::getActualSoftMxSize(MM_EnvironmentBase* env, uintptr_t memoryType)
{
	uintptr_t actualSoftMX = 0;
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if ((OMR_GC_POLICY_GENCON == env->getOmrVM()->gcPolicy) && (0 != extensions->softMx)) {
		uintptr_t totalHeapSize = getHeapRegionManager()->getTotalHeapSize();
		uintptr_t tenureSize = getActiveMemorySize(MEMORY_TYPE_OLD);

		Assert_MM_true(tenureSize <= totalHeapSize);

		uintptr_t nurserySize = totalHeapSize - tenureSize;

		if (MEMORY_TYPE_OLD == memoryType && nurserySize <= extensions->softMx) {
			actualSoftMX = extensions->softMx - nurserySize;
		} else if (MEMORY_TYPE_NEW == memoryType) {
			actualSoftMX = extensions->softMx / 4;
		} else {
			actualSoftMX = 0;
		}
	} else {
		actualSoftMX = extensions->softMx;
	}
	return actualSoftMX;
}
