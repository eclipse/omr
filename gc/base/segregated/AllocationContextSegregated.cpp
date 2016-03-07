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

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrport.h"

#include <string.h>

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "MemoryPoolAggregatedCellList.hpp"
#include "ModronAssertions.h"
#include "RegionPoolSegregated.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SegregatedMarkingScheme.hpp"
#include "SizeClasses.hpp"

#include "AllocationContextSegregated.hpp"
#include "HeapRegionQueue.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

#define MAX_UINT ((uintptr_t) (-1))

MM_AllocationContextSegregated *
MM_AllocationContextSegregated::newInstance(MM_EnvironmentBase *env, MM_GlobalAllocationManagerSegregated *gam, MM_RegionPoolSegregated *regionPool)
{
	MM_AllocationContextSegregated *allocCtxt = (MM_AllocationContextSegregated *)env->getForge()->allocate(sizeof(MM_AllocationContextSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (allocCtxt) {
		new(allocCtxt) MM_AllocationContextSegregated(env, gam, regionPool);
		if (!allocCtxt->initialize(env)) {
			allocCtxt->kill(env);
			allocCtxt = NULL;
		}
	}
	return allocCtxt;
}

bool
MM_AllocationContextSegregated::initialize(MM_EnvironmentBase *env)
{
	memset(&_perContextSmallFullRegions[0], 0, sizeof(_perContextSmallFullRegions));

	if (!MM_AllocationContext::initialize(env)) {
		return false;
	}

	if (0 != omrthread_monitor_init_with_name(&_mutexSmallAllocations, 0, "MM_AllocationContextSegregated small allocation monitor")) {
		return false;
	}

	if (0 != omrthread_monitor_init_with_name(&_mutexArrayletAllocations, 0, "MM_AllocationContextSegregated arraylet allocation monitor")) {
		return false;
	}

	for (int32_t i = 0; i < OMR_SIZECLASSES_NUM_SMALL + 1; i++) {
		_smallRegions[i] = NULL;
		/* the small allocation lock needs to be acquired before small full region queue can be accessed, no concurrent access should be possible */
		_perContextSmallFullRegions[i] = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_FULL, true, false, false);
		if (NULL == _perContextSmallFullRegions[i]) {
			return false;
		}
	}

	/* the arraylet allocation lock needs to be acquired before arraylet full region queue can be accessed, no concurrent access should be possible */
	_perContextArrayletFullRegions = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_FULL, true, false, false);
	/* no locking done in the AC level for large allocation, large full page queue _is_ accessed concurrently */
	_perContextLargeFullRegions = MM_RegionPoolSegregated::allocateHeapRegionQueue(env, MM_HeapRegionList::HRL_KIND_FULL, false, true, false);
	if ((NULL == _perContextArrayletFullRegions ) || (NULL == _perContextLargeFullRegions)) {
		return false;
	}

	return true;
}

void
MM_AllocationContextSegregated::tearDown(MM_EnvironmentBase *env)
{
	if (_mutexSmallAllocations) {
		omrthread_monitor_destroy(_mutexSmallAllocations);
	}

	if (_mutexArrayletAllocations) {
		omrthread_monitor_destroy(_mutexArrayletAllocations);
	}

	for (int32_t i = 0; i < OMR_SIZECLASSES_NUM_SMALL + 1; i++) {
		if (NULL != _perContextSmallFullRegions[i]) {
			_perContextSmallFullRegions[i]->kill(env);
			_perContextSmallFullRegions[i] = NULL;
		}
	}

	if (NULL != _perContextArrayletFullRegions) {
		_perContextArrayletFullRegions->kill(env);
		_perContextArrayletFullRegions = NULL;
	}

	if (NULL != _perContextLargeFullRegions) {
		_perContextLargeFullRegions->kill(env);
		_perContextLargeFullRegions = NULL;
	}

	MM_AllocationContext::tearDown(env);
}

void
MM_AllocationContextSegregated::flushSmall(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	MM_HeapRegionDescriptorSegregated *region = _smallRegions[sizeClass];
	if (region != NULL) {
		uintptr_t cellSize = env->getExtensions()->defaultSizeClasses->getCellSize(sizeClass);
		flushHelper(env, region, cellSize);
	}

	_smallRegions[sizeClass] = NULL;
}

void
MM_AllocationContextSegregated::flushArraylet(MM_EnvironmentBase *env)
{
	if (NULL != _arrayletRegion) {
		flushHelper(env, _arrayletRegion, env->getOmrVM()->_arrayletLeafSize);
	}

	_arrayletRegion = NULL;
}

void
MM_AllocationContextSegregated::flush(MM_EnvironmentBase *env)
{
	lockContext();

	/* flush the per-context small full regions to sweep regions */
	for (int32_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
		flushSmall(env, sizeClass);
		_regionPool->getSmallSweepRegions(sizeClass)->enqueue(_perContextSmallFullRegions[sizeClass]);
	}

	/* flush the per-context large full region to sweep regions */
	_regionPool->getLargeSweepRegions()->enqueue(_perContextLargeFullRegions);

	/* flush the per-context arraylet region to sweep regions */
	flushArraylet(env);
	_regionPool->getArrayletSweepRegions()->enqueue(_perContextArrayletFullRegions);

	unlockContext();
}

/**
 * helper function to flush the cached full regions back to the region pool 
 * so that RegionPool::showPages can gather stats on all regions in the region pool.
 * This can be called during an allocation failure so context lock needs to be acquired.
 */
void
MM_AllocationContextSegregated::returnFullRegionsToRegionPool(MM_EnvironmentBase *env)
{
	lockContext();

	for (int32_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
		_regionPool->getSmallFullRegions(sizeClass)->enqueue(_perContextSmallFullRegions[sizeClass]);
	}
	_regionPool->getLargeFullRegions()->enqueue(_perContextLargeFullRegions);
	_regionPool->getArrayletFullRegions()->enqueue(_perContextArrayletFullRegions);

	unlockContext();
}

void
MM_AllocationContextSegregated::signalSmallRegionDepleted(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	/* No implementation */
}

bool
MM_AllocationContextSegregated::tryAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	MM_HeapRegionDescriptorSegregated *region = _regionPool->allocateRegionFromSmallSizeClass(env, sizeClass);
	bool result = false;
	if (region != NULL) {
		_smallRegions[sizeClass] = region;
		/* cache the small full region in AC */
		_perContextSmallFullRegions[sizeClass]->enqueue(region);
		result = true;
	}
	return result;
}

bool
MM_AllocationContextSegregated::trySweepAndAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass, uintptr_t *sweepCount, uint64_t *sweepStartTime)
{
	bool result = false;

	MM_HeapRegionDescriptorSegregated *region = _regionPool->sweepAndAllocateRegionFromSmallSizeClass(env, sizeClass);
	if (region != NULL) {
		MM_AtomicOperations::storeSync();
		_smallRegions[sizeClass] = region;
		/* Don't update bytesAllocated because unswept regions are still considered to be in use */
		result = true;
	}
	return result;
}

bool
MM_AllocationContextSegregated::tryAllocateFromRegionPool(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	MM_HeapRegionDescriptorSegregated *region = _regionPool->allocateFromRegionPool(env, 1, sizeClass, MAX_UINT);
	bool result = false;
	if(NULL != region) {
		/* cache the small full region in AC */
		_perContextSmallFullRegions[sizeClass]->enqueue(region);

		region->formatFresh(env, sizeClass, region->getLowAddress());

		/* A store barrier is required here since the initialization of the new region needs to write-back before
		 * we make it reachable via _smallRegions (_smallRegions is accessed from outside the lock which covers this write)
		 */
		MM_AtomicOperations::storeSync();
		_smallRegions[sizeClass] = region;
		result = true;
	}
	return result;
}

bool
MM_AllocationContextSegregated::shouldPreMarkSmallCells(MM_EnvironmentBase *env)
{
	return true;
}

/*
 * Pre allocate a list of cells, the amount to pre-allocate is retrieved from the env's allocation interface.
 * @return the carved off first cell in the list
 */
uintptr_t *
MM_AllocationContextSegregated::preAllocateSmall(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired)
{
	MM_SizeClasses *sizeClasses = env->getExtensions()->defaultSizeClasses;
	uintptr_t sizeClass = sizeClasses->getSizeClassSmall(sizeInBytesRequired);
	uintptr_t sweepCount = 0;
	uint64_t sweepStartTime = 0;
	bool done = false;
	uintptr_t *result = NULL;

	/* BEN TODO 1429: The object allocation interface base class should define all API used by this method such that casting would be unnecessary. */
	MM_SegregatedAllocationInterface* segregatedAllocationInterface = (MM_SegregatedAllocationInterface*)env->_objectAllocationInterface;
	uintptr_t replenishSize = segregatedAllocationInterface->getReplenishSize(env, sizeInBytesRequired);
	uintptr_t preAllocatedBytes = 0;

	while (!done) {

		/* If we have a region, attempt to replenish the ACL's cache */
		MM_HeapRegionDescriptorSegregated *region = _smallRegions[sizeClass];
		if (NULL != region) {
			MM_MemoryPoolAggregatedCellList *memoryPoolACL = region->getMemoryPoolACL();
			uintptr_t* cellList = memoryPoolACL->preAllocateCells(env, sizeClasses->getCellSize(sizeClass), replenishSize, &preAllocatedBytes);
			if (NULL != cellList) {
				Assert_MM_true(preAllocatedBytes > 0);
				if (shouldPreMarkSmallCells(env)) {
					_markingScheme->preMarkSmallCells(env, region, cellList, preAllocatedBytes);
				}
				segregatedAllocationInterface->replenishCache(env, sizeInBytesRequired, cellList, preAllocatedBytes);
				result = (uintptr_t *) segregatedAllocationInterface->allocateFromCache(env, sizeInBytesRequired);
				done = true;
			}
		}

		smallAllocationLock();

		/* Either we did not have a region or we failed to preAllocate from the ACL. Retry if this is no
		 * longer true */
		region = _smallRegions[sizeClass];
		if ((NULL == region) || !region->getMemoryPoolACL()->hasCell()) {

			/* This may cause the start of a GC */
			signalSmallRegionDepleted(env, sizeClass);

			flushSmall(env, sizeClass);

			/* Attempt to get a region of this size class which may already have some allocated cells */
			if (!tryAllocateRegionFromSmallSizeClass(env, sizeClass)) {
				/* Attempt to get a region by sweeping */
				if (!trySweepAndAllocateRegionFromSmallSizeClass(env, sizeClass, &sweepCount, &sweepStartTime)) {
					/* Attempt to get an unused region */
					if (!tryAllocateFromRegionPool(env, sizeClass)) {
						/* Really out of regions */
						done = true;
					}
				}
			}
		}

		smallAllocationUnlock();
	}
	return result;

}

#if defined(OMR_GC_ARRAYLETS)
uintptr_t *
MM_AllocationContextSegregated::allocateArraylet(MM_EnvironmentBase *env, omrarrayptr_t parent)
{
	arrayletAllocationLock();

retry:
	uintptr_t *arraylet = (_arrayletRegion == NULL) ? NULL : _arrayletRegion->allocateArraylet(env, parent);

	if (arraylet != NULL) {
		arrayletAllocationUnlock();

		OMRZeroMemory(arraylet, env->getOmrVM()->_arrayletLeafSize);
		return arraylet;
	}

	flushArraylet(env);

	MM_HeapRegionDescriptorSegregated *region = _regionPool->allocateRegionFromArrayletSizeClass(env);
	if (region != NULL) {
		/* cache the arraylet full region in AC */
		_perContextArrayletFullRegions->enqueue(region);
		_arrayletRegion = region;
		goto retry;
	}

	region = _regionPool->allocateFromRegionPool(env, 1, OMR_SIZECLASSES_ARRAYLET, MAX_UINT);
	if (region != NULL) {
		/* cache the small full region in AC */
		_perContextArrayletFullRegions->enqueue(region);
		_arrayletRegion = region;
		goto retry;
	}

	arrayletAllocationUnlock();

	return NULL;
}
#endif /* defined(OMR_GC_ARRAYLETS) */

/* This method is moved here so that AC can see the large full page and cache it */
uintptr_t *
MM_AllocationContextSegregated::allocateLarge(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired)
{
	uintptr_t neededRegions = _regionPool->divideUpRegion(sizeInBytesRequired);
	MM_HeapRegionDescriptorSegregated *region = NULL;
	uintptr_t excess = 0;

	while (region == NULL && excess < MAX_UINT) {
		region = _regionPool->allocateFromRegionPool(env, neededRegions, OMR_SIZECLASSES_LARGE, excess);
		excess = (2 * excess) + 1;
	}

	uintptr_t *result = (region == NULL) ? NULL : (uintptr_t *)region->getLowAddress();

	/* Flush the large page right away. */
	if (region != NULL) {
		/* cache the large full region in AC */
		_perContextLargeFullRegions->enqueue(region);

		/* reset ACL counts */
		region->getMemoryPoolACL()->resetCounts();
	}

	return result;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
