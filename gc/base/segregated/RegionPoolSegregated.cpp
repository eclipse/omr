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
 * @ingroup GC_Modron_Realtime
 */


#include "EnvironmentBase.hpp"
#include "FreeHeapRegionList.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionManager.hpp"
#include "LockingFreeHeapRegionList.hpp"
#include "LockingHeapRegionQueue.hpp"
#include "MemoryPoolAggregatedCellList.hpp"
#include "OMR_VMThread.hpp"
#include "OMRVMThreadListIterator.hpp"
#include "SegregatedAllocationInterface.hpp"

#include "RegionPoolSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

uintptr_t defragBucketThresholds[NUM_DEFRAG_BUCKETS] = DEFRAG_BUCKET_THRESHOLDS;

/**
 * Create and initialize a new instance of the receiver.
 */
MM_RegionPoolSegregated *
MM_RegionPoolSegregated::newInstance(MM_EnvironmentBase *env, MM_HeapRegionManager *heapRegionManager)
{
	MM_RegionPoolSegregated *regionPool;

	regionPool = (MM_RegionPoolSegregated *)env->getForge()->allocate(sizeof(MM_RegionPoolSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != regionPool) {
		regionPool = new(regionPool) MM_RegionPoolSegregated(env, heapRegionManager);
		if (!regionPool->initialize(env)) {
			regionPool->kill(env);
			regionPool = NULL;
		}
	}
	return regionPool;
}

/**
 * Initialization.
 */
bool
MM_RegionPoolSegregated::initialize(MM_EnvironmentBase *env)
{
	uint32_t szClass;
	
	for (szClass=0; szClass<=OMR_SIZECLASSES_MAX_SMALL; szClass++) {
		for (uint32_t i=0; i<NUM_DEFRAG_BUCKETS; i++) {
			_smallAvailableRegions[szClass][i] = NULL;
		}
		_smallFullRegions[szClass] = NULL;
		_smallSweepRegions[szClass] = NULL;
	}

	_singleFreeList = MM_RegionPoolSegregated::allocateFreeHeapRegionList(env, MM_HeapRegionList::HRL_KIND_FREE, true);
	_multiFreeList = MM_RegionPoolSegregated::allocateFreeHeapRegionList(env, MM_HeapRegionList::HRL_KIND_MULTI_FREE, false);
	_coalesceFreeList = MM_RegionPoolSegregated::allocateFreeHeapRegionList(env, MM_HeapRegionList::HRL_KIND_COALESCE, false);
	if ((_singleFreeList == NULL) || (_multiFreeList == NULL) || (_coalesceFreeList == NULL)) {
		return false;
	}
	_splitAvailableListSplitCount = env->getExtensions()->splitAvailableListSplitAmount;
	Assert_MM_true(0 < _splitAvailableListSplitCount);
	for (szClass=OMR_SIZECLASSES_MIN_SMALL; szClass<=OMR_SIZECLASSES_MAX_SMALL; szClass++) {
		for (int32_t i=0; i<NUM_DEFRAG_BUCKETS; i++) {
			uintptr_t splitAvailableListsSize = sizeof(MM_LockingHeapRegionQueue) * _splitAvailableListSplitCount;
			_smallAvailableRegions[szClass][i] = (MM_LockingHeapRegionQueue *)env->getForge()->allocate(splitAvailableListsSize, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
			if (NULL == _smallAvailableRegions[szClass][i]) {
				return false;
			}
			MM_LockingHeapRegionQueue *regionQueue = _smallAvailableRegions[szClass][i];
			for (uintptr_t j=0; j<_splitAvailableListSplitCount; j++) {
				/* The available lists should track the free bytes in their regions (4th param = true) */
				new (&regionQueue[j]) MM_LockingHeapRegionQueue(MM_HeapRegionList::HRL_KIND_AVAILABLE, true, true, true);
				if (!(&regionQueue[j])->initialize(env)) {
					return false;
				}
			}
		}
		_smallFullRegions[szClass] = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_FULL, true, true, false);
		_smallSweepRegions[szClass] = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_SWEEP, true, true, false);
		if (NULL == _smallFullRegions[szClass] || NULL == _smallSweepRegions[szClass]) {
			return false;
		}
		_smallOccupancy[szClass] = 0.5;		
	}
	
	/* The available lists should track the free bytes in their regions (4th param = true) */
	_arrayletAvailableRegions = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_AVAILABLE, true, true, true);
	_arrayletFullRegions = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_FULL, true, true, false);
	_arrayletSweepRegions = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_SWEEP, true, true, false);
	if ((NULL == _arrayletAvailableRegions) || (NULL == _arrayletFullRegions) || (NULL == _arrayletSweepRegions)) {
		return false;
	}
	
	_largeFullRegions = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_FULL, false, true, false);
	_largeSweepRegions = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_SWEEP, false, true, false);
	if ((NULL == _largeFullRegions) || (NULL == _largeSweepRegions)) {
		return false;
	}	

	resetSkipAvailableRegionForAllocation();

	return true;
}


MM_HeapRegionQueue*
MM_RegionPoolSegregated::allocateHeapRegionQueue(MM_EnvironmentBase *env, MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly, bool concurrentAccess, bool trackFreeBytes)
{
	return MM_LockingHeapRegionQueue::newInstance(env, regionListKind, singleRegionsOnly, concurrentAccess, trackFreeBytes);
}

MM_FreeHeapRegionList*
MM_RegionPoolSegregated::allocateFreeHeapRegionList(MM_EnvironmentBase *env, MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly)
{
	return MM_LockingFreeHeapRegionList::newInstance(env, regionListKind, singleRegionsOnly);
}

void
MM_RegionPoolSegregated::tearDown(MM_EnvironmentBase *env)
{
	if (_singleFreeList) {
		_singleFreeList->kill(env);
		_singleFreeList = NULL;
	}
	
	if (_multiFreeList) {
		_multiFreeList->kill(env);
		_multiFreeList = NULL;
	}
	
	if (_coalesceFreeList) {
		_coalesceFreeList->kill(env);
		_coalesceFreeList = NULL;
	}
	
	if (_largeFullRegions) {
		_largeFullRegions->kill(env);
		_largeFullRegions = NULL;
	}
	if (_largeSweepRegions) {
		_largeSweepRegions->kill(env);
		_largeSweepRegions = NULL;
	}
	
	if (_arrayletAvailableRegions) {
		_arrayletAvailableRegions->kill(env);
		_arrayletAvailableRegions = NULL;
	}
	if (_arrayletFullRegions) {
		_arrayletFullRegions->kill(env);
		_arrayletFullRegions = NULL;
	}
	if (_arrayletSweepRegions) {
		_arrayletSweepRegions->kill(env);
		_arrayletSweepRegions = NULL;
	}
	
	for (int32_t szClass=OMR_SIZECLASSES_MIN_SMALL; szClass <= OMR_SIZECLASSES_MAX_SMALL; szClass++) {
		for (uintptr_t i=0; i<NUM_DEFRAG_BUCKETS; i++) {
			MM_LockingHeapRegionQueue *regionQueueArray = _smallAvailableRegions[szClass][i];
			if (NULL != regionQueueArray) {
				for (uintptr_t j=0; j<_splitAvailableListSplitCount; j++) {
					(&regionQueueArray[j])->tearDown(env);
				}
				env->getForge()->free(regionQueueArray);
			}
		}
		if (_smallFullRegions[szClass]) {
			_smallFullRegions[szClass]->kill(env);
			_smallFullRegions[szClass] = NULL;
		}
		if (_smallSweepRegions[szClass]) {
			_smallSweepRegions[szClass]->kill(env);
			_smallSweepRegions[szClass] = NULL;
		}
	}
}

void
MM_RegionPoolSegregated::addSingleFree(MM_EnvironmentBase *env, MM_HeapRegionQueue *regionQueue)
{ 
	uintptr_t returnedRegions = regionQueue->length();
	decrementRegionsInUse(returnedRegions);
	_singleFreeList->push(regionQueue);
}

void
MM_RegionPoolSegregated::moveInUseToSweep(MM_EnvironmentBase *env)
{
	_currentTotalCountOfSweepRegions = 0;
	for (int32_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
		_darkMatterCellCount[sizeClass] = 0;
		_smallSweepRegions[sizeClass]->enqueue(_smallFullRegions[sizeClass]);
		for (int32_t i=0; i<NUM_DEFRAG_BUCKETS; i++) {
			MM_LockingHeapRegionQueue *regionQueue = _smallAvailableRegions[sizeClass][i];
			for (uintptr_t j=0; j<_splitAvailableListSplitCount; j++) {
				_smallSweepRegions[sizeClass]->enqueue(&regionQueue[j]);
			}
		}
		_initialCountOfSweepRegions[sizeClass] = _currentCountOfSweepRegions[sizeClass] = _smallSweepRegions[sizeClass]->getTotalRegions();
		_initialTotalCountOfSweepRegions = _currentTotalCountOfSweepRegions += _initialCountOfSweepRegions[sizeClass];
	}

	_largeSweepRegions->enqueue(_largeFullRegions);

	_arrayletSweepRegions->enqueue(_arrayletFullRegions);
	_arrayletSweepRegions->enqueue(_arrayletAvailableRegions);
}

void
MM_RegionPoolSegregated::countFreeRegions(uintptr_t *singleFree, uintptr_t *multiFree, uintptr_t *maxMultiFree, uintptr_t *coalesceFree)
{
	*singleFree = _singleFreeList->getTotalRegions();
	*multiFree = _multiFreeList->getTotalRegions();
	*maxMultiFree = _multiFreeList->getMaxRegions();
	*coalesceFree = _coalesceFreeList->getTotalRegions();
}

/* enqueue the region using the split region list indexed by splitListIndex for size class sizeClass */
void
MM_RegionPoolSegregated::enqueueAvailable(MM_HeapRegionDescriptorSegregated *region, uintptr_t sizeClass, uintptr_t occupancy, uintptr_t splitListIndex)
{
	for (int32_t i = 0; i < NUM_DEFRAG_BUCKETS; i++) {
		if (occupancy >= defragBucketThresholds[i]) {
			(&(_smallAvailableRegions[sizeClass][i])[splitListIndex])->enqueue(region);
			break;
		}
	}
}

void
MM_RegionPoolSegregated::addFreeRange(void *lowAddress, void *highAddress)
{
	MM_HeapRegionDescriptorSegregated *firstInRange = (MM_HeapRegionDescriptorSegregated *)_heapRegionManager->tableDescriptorForAddress(lowAddress);
	uintptr_t range = ((uintptr_t)highAddress - (uintptr_t)lowAddress) / firstInRange->getSize();

	if (1 < range) {
		firstInRange->setRange(firstInRange->getRegionType(), range);
		_multiFreeList->push(firstInRange);
	} else if (1 == range) {
		_singleFreeList->push(firstInRange);
	} else {
		/* empty range */
	}

	Assert_MM_true(0 == range || (lowAddress == firstInRange->getLowAddress() && highAddress == firstInRange->getHighAddress()));
}

void
MM_RegionPoolSegregated::addFreeRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region, bool alreadyFree)
{
	uintptr_t size = region->getRange();
	if (!alreadyFree) {
		region->setFree(size);
		decrementRegionsInUse(size);
	}

	if (size == 1) {
		_singleFreeList->push(region);
	} else {
		_multiFreeList->push(region);
	}
}

MM_HeapRegionDescriptorSegregated *
MM_RegionPoolSegregated::allocateFromRegionPool(MM_EnvironmentBase *env, uintptr_t numRegions, uintptr_t szClass, uintptr_t maxExcess)
{
	MM_HeapRegionDescriptorSegregated *region = NULL;

	if (numRegions == 1) {
		region = _singleFreeList->allocate(env, szClass);
	}
	
	if (region == NULL) {
		region = _multiFreeList->allocate(env, szClass, numRegions, maxExcess);
		
		if (region == NULL) {
			region = _coalesceFreeList->allocate(env, szClass, numRegions, maxExcess);
		}
	}
	
	
	if (region != NULL) {
		incrementRegionsInUse(region->getRange()); /* we must add here because we will return remainder later */
		
		/* We must notify the allocation tracker that a fresh region has been allocated, it will know how to
		 * account for bytes lost to internal fragmentation and will account for all the memory allocated
		 * in the case of large region allocation.
		 */
		region->emptyRegionAllocated(env);
	}
	
	return region;
}

/* join the lists for each buckets per size class, per split index */
void
MM_RegionPoolSegregated::joinBucketListsForSplitIndex(MM_EnvironmentBase *env)
{
	uintptr_t splitIndex = env->getSlaveID() % _splitAvailableListSplitCount;
	for (int32_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
		MM_LockingHeapRegionQueue *primaryQueue = &(_smallAvailableRegions[sizeClass][PRIMARY_BUCKET])[splitIndex];
		for (int32_t i=1; i<NUM_DEFRAG_BUCKETS; i++) {
			primaryQueue->enqueue(&(_smallAvailableRegions[sizeClass][i])[splitIndex]);
		}
	}
}

/**
 * Attempt to allocate a region from the given size classes available list.
 * If there are no available regions in this size class, return null.
 */
MM_HeapRegionDescriptorSegregated *
MM_RegionPoolSegregated::allocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	MM_HeapRegionDescriptorSegregated *region = NULL;
	/* skip searching the available queue for this size class if we've already exhausted all available regions */
	if (SKIP_AVAILABLE_REGION_FOR_ALLOCATION == _skipAvailableRegionForAllocation[sizeClass]) {
		return region;
	}

	/* try bucket 0, i.e. primary bucket first */
	uintptr_t startList = env->getEnvironmentId() % _splitAvailableListSplitCount;
	MM_LockingHeapRegionQueue *primaryQueueArray = _smallAvailableRegions[sizeClass][PRIMARY_BUCKET];
	MM_LockingHeapRegionQueue *allocationQueue = &primaryQueueArray[startList];
	region = allocationQueue->dequeueIfNonEmpty();
	if (region != NULL) {
		return region;
	}

	/* if primary bucket fails, try the other split queues, starting from the current thread's split index */
	for (uintptr_t j=startList+1; j<startList+_splitAvailableListSplitCount; j++) {
		allocationQueue = &primaryQueueArray[j%_splitAvailableListSplitCount];
		region = allocationQueue->dequeueIfNonEmpty();
		if (region != NULL) {
			return region;
		}
	}

	/* if all split lists in the primary bucket fail, try the remaining buckets */
	if (_isSweepingSmall) {
		for (int32_t i=1; i<NUM_DEFRAG_BUCKETS; i++) {
			MM_LockingHeapRegionQueue *queueArray = _smallAvailableRegions[sizeClass][i];
			for (uintptr_t j=startList; j<startList+_splitAvailableListSplitCount; j++) {
				allocationQueue = &queueArray[j%_splitAvailableListSplitCount];
				region = allocationQueue->dequeueIfNonEmpty();
				if (region != NULL) {
					return region;
				}
			}
		}
	} else {
		_skipAvailableRegionForAllocation[sizeClass] = SKIP_AVAILABLE_REGION_FOR_ALLOCATION;
	}
	return region;
}

/**
 * Attempt to allocate a region from the arraylet available list.
 * If there are no available arraylet regions, return null.
 */
MM_HeapRegionDescriptorSegregated *
MM_RegionPoolSegregated::allocateRegionFromArrayletSizeClass(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptorSegregated *region = _arrayletAvailableRegions->dequeue();
	if (region != NULL) {
		return region;
	}
	return NULL;
}

/**
 * Attempt to satisfy a region allocation request by sweeping a region to make
 * it eligible for further allocation.
 */
MM_HeapRegionDescriptorSegregated *
MM_RegionPoolSegregated::sweepAndAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	MM_HeapRegionDescriptorSegregated *region = _smallSweepRegions[sizeClass]->dequeue();

	if (region != NULL) {
		_sweepScheme->sweepRegion(env, region);
		/* Keep maintaining the occupancy info even while doing nondeterministic sweeps */
		_smallOccupancy[sizeClass] = (_smallOccupancy[sizeClass] * 0.9f) + (region->getMemoryPoolACL()->getMarkCount() / region->getNumCells() * 0.1f );
		decrementCurrentCountOfSweepRegions(sizeClass, 1);
		decrementCurrentTotalCountOfSweepRegions(1);
		_smallFullRegions[sizeClass]->enqueue(region);
	}
	return region;
}

void
MM_RegionPoolSegregated::updateOccupancy (uintptr_t sizeClass, uintptr_t occupancy)
{
	assume(occupancy <= 100 && occupancy >= 0, "invalid occupancy estimate");
	_smallOccupancy[sizeClass] = _smallOccupancy[sizeClass] * 0.9f + occupancy * 0.001f;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
