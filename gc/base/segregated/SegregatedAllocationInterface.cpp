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

#include "ModronAssertions.h"
#include "objectdescription.h"

#include "AllocateDescription.hpp"
#include "AllocationContextSegregated.hpp"
#include "AllocationStats.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "EnvironmentBase.hpp"
#include "FrequentObjectsStats.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "SizeClasses.hpp"
#include "ObjectHeapIteratorSegregated.hpp"

#include "SegregatedAllocationInterface.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * Create and return a new instance of MM_SegregatedAllocationInterface.
 * 
 * @return the new instance, or NULL on failure.
 */
MM_SegregatedAllocationInterface*
MM_SegregatedAllocationInterface::newInstance(MM_EnvironmentBase *env)
{
	MM_SegregatedAllocationInterface *allocationInterface;
	
	allocationInterface = (MM_SegregatedAllocationInterface *)env->getForge()->allocate(sizeof(MM_SegregatedAllocationInterface), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(NULL != allocationInterface) {
		new(allocationInterface) MM_SegregatedAllocationInterface(env);
		if(!allocationInterface->initialize(env)) {
			allocationInterface->kill(env);
			return NULL;
		}
	}
	return allocationInterface;
}

/**
 * Clean up all state and resources held by the receiver.
 */
void
MM_SegregatedAllocationInterface::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * One time initialization of the receivers state.
 * @return true on successful initialization, false otherwise.
 */
bool
MM_SegregatedAllocationInterface::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	bool result = true;

	Assert_MM_true(NULL == _frequentObjectsStats);

	if (extensions->doFrequentObjectAllocationSampling){
		_frequentObjectsStats = MM_FrequentObjectsStats::newInstance(env);
		result = (NULL != _frequentObjectsStats);
	}
	
	if (result) {
		_allocationCache = _languageAllocationCache.getLanguageSegregatedAllocationCacheStruct(env);
		_sizeClasses = extensions->defaultSizeClasses;
		_cachedAllocationsEnabled = true;

		memset(_allocationCache, 0, sizeof(LanguageSegregatedAllocationCache));
		memset(&_allocationCacheStats, 0, sizeof(_allocationCacheStats));
		for (uintptr_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
			_replenishSizes[sizeClass] = extensions->allocationCacheInitialSize;
		}
	}
	
	return result;
}

/**
 * Internal clean up of the receivers state and resources.
 */
void
MM_SegregatedAllocationInterface::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _frequentObjectsStats) {
		_frequentObjectsStats->kill(env);
		_frequentObjectsStats = NULL;
	}
}

/**
 * Allocates a cell from the cache of the size class of the given sizeInBytes.
 * @return The carved off cell, ready for allocation or NULL if the cache for the size class is empty.
 */

void*
MM_SegregatedAllocationInterface::allocateFromCache(MM_EnvironmentBase* env, uintptr_t sizeInBytes)
{
	/* Note we DO NOT keep stats about individual allocations since we can't guarantee all allocations from
	 * the cache will be done by the GC; the JIT may inline the allocations when the cache isn't empty.
	 */
	uintptr_t sizeClass = _sizeClasses->getSizeClass(sizeInBytes);
	uintptr_t cellSize = _sizeClasses->getCellSize(sizeClass);
	
	uintptr_t* cellCurrent = _allocationCache[sizeClass].current;
	
	/* Not doing check as (uintptr_t)cellCurrent + cellSize <= (uintptr_t)_allocationCache[sizeClass].top
	 * to avoid overflow.  See CMVC 194852 for more info.
	 */
	if (cellSize <= ((uintptr_t)_allocationCache[sizeClass].top) - ((uintptr_t) cellCurrent)) {
		_allocationCache[sizeClass].current = (uintptr_t *)((uintptr_t)cellCurrent + cellSize);
	} else {
		return NULL;
	}
	return cellCurrent;
}

void*
MM_SegregatedAllocationInterface::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	void* cell = NULL;
	uintptr_t sizeInBytes = allocateDescription->getBytesRequested();
	/* Record the memory space from which the allocation takes place in the AD */
	allocateDescription->setMemorySpace(memorySpace);
	
	if (shouldCollectOnFailure) {
		allocateDescription->setObjectFlags(memorySpace->getDefaultMemorySubSpace()->getObjectFlags());
		
		/* Ensure we're allocating from the heap (not immortal or scopes) and that the allocation will be from a small region. */
		if (memorySpace == env->getExtensions()->heap->getDefaultMemorySpace() && (sizeInBytes <= OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES)) {
			cell = allocateFromCache(env, sizeInBytes);
			if (NULL == cell) {
				MM_AllocationContextSegregated *ac = (MM_AllocationContextSegregated *) env->getAllocationContext();
				if (ac != NULL) {
					cell = ac->preAllocateSmall(env, sizeInBytes);
				}
			}
		}
		
		if (NULL == cell) {
			cell = memorySpace->getDefaultMemorySubSpace()->allocateObject(env, allocateDescription, NULL, NULL, shouldCollectOnFailure);
		}
	} else {
		allocateDescription->setObjectFlags(0);
		
		if (memorySpace != env->getExtensions()->heap->getDefaultMemorySpace()) {
			cell = memorySpace->getDefaultMemorySubSpace()->allocateObject(env, allocateDescription, NULL, NULL, shouldCollectOnFailure);
		} else if (sizeInBytes <= OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES) {
			cell = allocateFromCache(env, sizeInBytes);
			if (NULL == cell) {
				MM_AllocationContextSegregated *ac = (MM_AllocationContextSegregated *) env->getAllocationContext();
				if (ac != NULL) {
					cell = ac->preAllocateSmall(env, sizeInBytes);
				}
			}
		}
	}

	if ((NULL != cell) && !allocateDescription->isCompletedFromTlh()) {
		_stats._allocationBytes += allocateDescription->getContiguousBytes();
		++_stats._allocationCount;
	}

	return cell;
}

void*
MM_SegregatedAllocationInterface::allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	void *result = NULL;
#if defined(OMR_GC_ARRAYLETS)
	MM_MemorySubSpace *subSpace = memorySpace->getDefaultMemorySubSpace();
	
	result = subSpace->allocateObject(env, allocateDescription, NULL, NULL, shouldCollectOnFailure);

	if ((NULL != result) && !allocateDescription->isCompletedFromTlh()) {
		_stats._allocationBytes += allocateDescription->getContiguousBytes();
		++_stats._allocationCount;
	}

#else /* OMR_GC_ARRAYLETS */
	result = allocateObject(env, allocateDescription, memorySpace, shouldCollectOnFailure);
#endif /* OMR_GC_ARRAYLETS */
	return result;
}

#if defined(OMR_GC_ARRAYLETS)
/**
 * Allocate the arraylet spine.
 */
void *
MM_SegregatedAllocationInterface::allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	void *result = NULL;
	MM_MemorySubSpace *subSpace = memorySpace->getDefaultMemorySubSpace();
	
	result = subSpace->allocateObject(env, allocateDescription, NULL, NULL, shouldCollectOnFailure);

	if ((NULL != result) && !allocateDescription->isCompletedFromTlh()) {
		_stats._allocationBytes += allocateDescription->getContiguousBytes();
		++_stats._allocationCount;
	}

	return result;
}

/**
 * Allocate an arraylet leaf.
 */
void *
MM_SegregatedAllocationInterface::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	void *result = NULL;
	MM_MemorySubSpace *subSpace = memorySpace->getDefaultMemorySubSpace();
	
	result = subSpace->allocateArrayletLeaf(env, allocateDescription, NULL, NULL, true);

	if ((NULL != result) && !allocateDescription->isCompletedFromTlh()) {
		_stats._allocationBytes += allocateDescription->getContiguousBytes();
		++_stats._allocationCount;
	}

	return result;
}
#endif /* OMR_GC_ARRAYLETS */

/**
 * Flush the allocation cache.
 */
void
MM_SegregatedAllocationInterface::flushCache(MM_EnvironmentBase *env)
{
	/* make the current caches walkable */
	for (uintptr_t sizeClass = 0; sizeClass < OMR_SIZECLASSES_NUM_SMALL+1; sizeClass++) {
		if (_allocationCache[sizeClass].current < _allocationCache[sizeClass].top) {
			MM_HeapLinkedFreeHeader *chunk = MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(_allocationCache[sizeClass].current);
			chunk->setSize((uintptr_t)_allocationCache[sizeClass].top - (uintptr_t)_allocationCache[sizeClass].current);
			/* next pointer value is irrelevant, it just needs to be low bit tagged, to make it non-object */
			chunk->setNext(NULL);
		}
	}
	memset(_allocationCache, 0, sizeof(LanguageSegregatedAllocationCache));
	env->getExtensions()->allocationStats.merge(&_stats);
	_stats.clear();
}

/**
 * Reconnect the allocation cache.
 */
void
MM_SegregatedAllocationInterface::reconnectCache(MM_EnvironmentBase *env)
{
	
}

/**
 * This will be called periodically (typically every GC cycle) so the cache can adjust its hungriness
 * based on its usage for the current period.
 */
void
MM_SegregatedAllocationInterface::restartCache(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	/* Backout policy:
	 * - if no allocations happened since the last restart, reset the replenish size to the minimum size
	 * - if only 1 replenish occurred since the last restart (meaning we didn't use up the entire cache), halve the replenish size
	 * - if less cells were pre-allocated than the preceding replenish size (will happen if we need multiple refreshes to reach the
	 *   current replenish size because the AC has fairly full regions), halve the replenish size
	 */
	for (uintptr_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
		/* Only enforce a backout policy if the desired sizes index was actually moved forward. */
		if (extensions->allocationCacheInitialSize != _replenishSizes[sizeClass]) {
			if (0 == _allocationCacheStats.replenishesSinceRestart[sizeClass]) {
				_replenishSizes[sizeClass] = extensions->allocationCacheInitialSize;
			} else if (1 == _allocationCacheStats.replenishesSinceRestart[sizeClass]) {
				_replenishSizes[sizeClass] /= 2;
			} else if (_allocationCacheStats.bytesPreAllocatedSinceRestart[sizeClass] < (_replenishSizes[sizeClass] - extensions->allocationCacheIncrementSize)) {
				_replenishSizes[sizeClass] /= 2;
			}
		}
	}
	
	memset(&(_allocationCacheStats.bytesPreAllocatedSinceRestart), 0, sizeof(_allocationCacheStats.bytesPreAllocatedSinceRestart));
	memset(&(_allocationCacheStats.replenishesSinceRestart), 0, sizeof(_allocationCacheStats.replenishesSinceRestart));
}

/**
 * Replenishes the cache for the given size class. The cache for the given size class must be empty.
 * @param sizeInBytes The size in bytes of a single cell (ie: not the total of bytes in the cache)
 * @param cacheMemory The head of the new cell linked free list to use as a cache
 * @param cacheSize The total size of allocatable memory contained in cacheMemory
 */
void
MM_SegregatedAllocationInterface::replenishCache(MM_EnvironmentBase* env, uintptr_t sizeInBytes, void* cacheMemory, uintptr_t cacheSize)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	uintptr_t* cellLink = (uintptr_t*)cacheMemory;
	uintptr_t sizeClass = _sizeClasses->getSizeClass(sizeInBytes);

	/* The allocation cache for the size class being replenished must be empty, otherwise we'd have
	 * to append the cellLink to the end, which would require traversing the list. There should be no
	 * reason to replenish a non-empty cache.
	 */
	Assert_MM_true(_allocationCache[sizeClass].current == _allocationCache[sizeClass].top);
	if (extensions->doFrequentObjectAllocationSampling) {
		updateFrequentObjectsStats(env, sizeClass);
	}

	_allocationCache[sizeClass].current = cellLink;
	_allocationCacheBases[sizeClass] = cellLink;
	_allocationCache[sizeClass].top = (uintptr_t *)((uintptr_t)cellLink + cacheSize);
	
	if (_cachedAllocationsEnabled) {
		/* Update the allocation stats. */
		_allocationCacheStats.bytesPreAllocatedTotal[sizeClass] += cacheSize;
		_allocationCacheStats.replenishesTotal[sizeClass] += 1;
		_allocationCacheStats.bytesPreAllocatedSinceRestart[sizeClass] += cacheSize;
		_allocationCacheStats.replenishesSinceRestart[sizeClass] += 1;
		
		/* Based on the new allocation stats, determine if we should bump up the desired amount of pre-allocated cells. */
		if ((_allocationCacheStats.bytesPreAllocatedSinceRestart[sizeClass] >= _replenishSizes[sizeClass])
			&& (_replenishSizes[sizeClass] < extensions->allocationCacheMaximumSize)
		) {
			
			_replenishSizes[sizeClass] += extensions->allocationCacheIncrementSize;
		}
	}
}

uintptr_t
MM_SegregatedAllocationInterface::getReplenishSize(MM_EnvironmentBase* env, uintptr_t sizeInBytes)
{
	/* If cached allocations are disabled, we only allow a replenish the size of the requested allocation. */
	if (_cachedAllocationsEnabled) {
		uintptr_t sizeClass = _sizeClasses->getSizeClass(sizeInBytes);
		return _replenishSizes[sizeClass];
	} else {
		return sizeInBytes;
	}
}

void
MM_SegregatedAllocationInterface::enableCachedAllocations(MM_EnvironmentBase* env)
{
	if (!_cachedAllocationsEnabled) {
		_cachedAllocationsEnabled = true;
		restartCache(env);
	}
}

void
MM_SegregatedAllocationInterface::disableCachedAllocations(MM_EnvironmentBase* env)
{
	if (_cachedAllocationsEnabled) {
		_cachedAllocationsEnabled = false;
		flushCache(env);
		restartCache(env);
	}
}

void
MM_SegregatedAllocationInterface::updateFrequentObjectsStats(MM_EnvironmentBase *env, uintptr_t sizeClass)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_AllocationContextSegregated *ac = (MM_AllocationContextSegregated *) env->getAllocationContext();
	omrobjectptr_t base = (omrobjectptr_t) _allocationCacheBases[sizeClass];
	omrobjectptr_t top = (omrobjectptr_t) _allocationCache[sizeClass].top;

	if((NULL != _frequentObjectsStats) && (NULL != base) && (NULL != top)){
		uintptr_t cellSize = _sizeClasses->getCellSize(sizeClass);

		GC_ObjectHeapIteratorSegregated objectHeapIterator(extensions, base, top, ac->_smallRegions[sizeClass]->getRegionType(), cellSize, false, false);
		omrobjectptr_t object = NULL;
		uintptr_t limit = (((uintptr_t) top - (uintptr_t) base)*extensions->frequentObjectAllocationSamplingRate)/100 + (uintptr_t) base;

		while(NULL != (object = objectHeapIterator.nextObject())){
			if( ((uintptr_t) object) > limit){
				break;
			}
			_frequentObjectsStats->update(env, object);
		}
	}
}

#endif /* OMR_GC_SEGREGATED_HEAP */
