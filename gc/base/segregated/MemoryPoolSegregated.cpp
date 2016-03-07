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

#include "omrcfg.h"
#include "ModronAssertions.h"
#include "modronopt.h"
#include "objectdescription.h"
#include "sizeclasses.h"

#include <string.h>

#include "AllocateDescription.hpp"
#if defined(OMR_GC_ARRAYLETS)
#include "ArrayletObjectModel.hpp"
#endif /* OMR_GC_ARRAYLETS */
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "MemoryPool.hpp"
#include "MemoryPoolAggregatedCellList.hpp"
#include "ObjectHeapIteratorSegregated.hpp"
#include "OMRVMThreadListIterator.hpp"
#include "RegionPoolSegregated.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SegregatedAllocationTracker.hpp"
#include "SizeClasses.hpp"
#include "SlotObject.hpp"

#include "MemoryPoolSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * Create an instance of MPS and initialize it
 */
MM_MemoryPoolSegregated *
MM_MemoryPoolSegregated::newInstance(MM_EnvironmentBase *env, MM_RegionPoolSegregated *regionPool, uintptr_t minimumFreeEntrySize, MM_GlobalAllocationManagerSegregated *gam)
{
	MM_MemoryPoolSegregated *memoryPool = (MM_MemoryPoolSegregated *)env->getForge()->allocate(sizeof(MM_MemoryPoolSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memoryPool) {
		memoryPool = new(memoryPool) MM_MemoryPoolSegregated(env, regionPool, minimumFreeEntrySize, gam);
		if (!memoryPool->initialize(env)) {
			memoryPool->kill(env);
			memoryPool = NULL;
		}
	}
	return memoryPool;
}

/**
 * Initialize MPS
 */
bool
MM_MemoryPoolSegregated::initialize(MM_EnvironmentBase *env)
{
	if(!MM_MemoryPool::initialize(env)) {
		return false;
	}

	_extensions = env->getExtensions();
	MM_SegregatedAllocationTracker::initializeGlobalAllocationTrackerValues(env);

	return true;
}

/**
 * Tear down an MPS instance
 */
void
MM_MemoryPoolSegregated::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _regionPool) {
		_regionPool->kill(env);
		_regionPool = NULL;
	}
	
	MM_MemoryPool::tearDown(env);
}

MM_SegregatedAllocationTracker *
MM_MemoryPoolSegregated::createAllocationTracker(MM_EnvironmentBase* env)
{
	return MM_SegregatedAllocationTracker::newInstance(env, &_bytesInUse, _extensions->allocationTrackerFlushThreshold);
}


void
MM_MemoryPoolSegregated::flushCachedFullRegions(MM_EnvironmentBase *env)
{
	/* delegate to GAM to perform the flushing of per-context full regions to the region pool */
	_globalAllocationManager->flushCachedFullRegions(env);
}

void
MM_MemoryPoolSegregated::moveInUseToSweep(MM_EnvironmentBase *env)
{
	assume(env->isMasterThread(), "only can be called by master thread");
	/* Must flush allocation contexts as part of transfering inUseToSweep.
	 * Allocation contexts allocate into in use regions, and we can't let them be
	 * put on a sweep list while mutators are still allocating into them.
	 */
	_globalAllocationManager->flushAllocationContexts(env);
	_regionPool->moveInUseToSweep(env);
}

#if defined(OMR_GC_ARRAYLETS)
void*
MM_MemoryPoolSegregated::allocateChunkedArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocDesc, MM_AllocationContextSegregated *ac)
{
	const uintptr_t spineBytes = allocDesc->getContiguousBytes();
	const uintptr_t totalBytes = allocDesc->getBytesRequested();
	const uintptr_t numberArraylets = allocDesc->getNumArraylets(); 

	omrarrayptr_t spine = (omrarrayptr_t)allocateContiguous(env, allocDesc, ac);

	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	const uintptr_t arrayletLeafLogSize = env->getOmrVM()->_arrayletLeafLogSize;
	const uintptr_t arrayletLeafSize = env->getOmrVM()->_arrayletLeafSize;
	
	if (spine) {
		memset((void *)((uint8_t *)spine), 0, spineBytes);
		fomrobject_t *arrayoidPtr = _extensions->indexableObjectModel.getArrayoidPointer(spine);
		Assert_MM_true(totalBytes >= spineBytes);
		uintptr_t bytesRemaining = totalBytes - spineBytes;
		for (uintptr_t i=0; i<numberArraylets; i++) {
			uintptr_t* arraylet = NULL;
			if (0 < bytesRemaining) {
				arraylet = ac->allocateArraylet(env, spine);
				if (arraylet == NULL) {
					/* allocation failed; release all storage include spine. */
					env->getAllocationContext()->flush(env);
	
					for (uintptr_t j=0; j<i; j++) {
						GC_SlotObject slotObject(env->getOmrVM(), &arrayoidPtr[j]);
						arraylet = (uintptr_t*)slotObject.readReferenceFromSlot();
						
						MM_HeapRegionDescriptorSegregated *region = (MM_HeapRegionDescriptorSegregated *)regionManager->tableDescriptorForAddress(arraylet);
						region->clearArraylet(region->whichArraylet(arraylet, arrayletLeafLogSize));
						/* Arraylet backout means arraylets may be re-used before the next cycle, so we need to correct for 
						 * their un-allocation
						 */
						region->addBytesFreedToArrayletBackout(env);
					}
					MM_HeapRegionDescriptorSegregated *region = (MM_HeapRegionDescriptorSegregated *)regionManager->tableDescriptorForAddress((uintptr_t *)spine);
					if (region->isSmall()) {
						region->getMemoryPoolACL()->returnCell(env, (uintptr_t *)spine);
						/* Small spine backout means the cell may be re-used before the next cycle, so we need to correct for
						 * its un-allocation
						 */
						region->addBytesFreedToSmallSpineBackout(env);
					} else {
						/* It is illegal to call addFree without detach.  
						 * However, detaching causes meta-data corruption.
						 * Fortunately, it is legal to simply let the space be reclaimed at the next GC.
						 * _regionPool->addFreeRegion(env, region); */
					}
					return NULL;
				}
			} else {
				/* There can be one extra arraylet leaf pointer at the end, which we set to NULL.
				 * This allows us to calculate an address for the after-last element without reading
				 * beyond allocated memory. This is required for things like arraycopy.
				 */
				Assert_MM_true(i == numberArraylets - 1);
			}
			GC_SlotObject slotObject(env->getOmrVM(), &arrayoidPtr[i]);
			slotObject.writeReferenceToSlot((omrobjectptr_t)arraylet);
			bytesRemaining = MM_Math::saturatingSubtract(bytesRemaining, arrayletLeafSize);
		}
	}
	return spine;
}

/**
 * Allocate an arraylet leaf.
 */
void *
MM_MemoryPoolSegregated::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDesc)
{
	MM_AllocationContextSegregated *allocationContext = (MM_AllocationContextSegregated *)env->getAllocationContext();
	/* we must have already allocated the parent spine, which is stored in the AllocateDescriptionCore */
	omrarrayptr_t spine = allocDesc->getSpine();
	
	return allocationContext->allocateArraylet(env, spine);
}
#endif /* OMR_GC_ARRAYLETS */

/**
 * @todo Provide function documentation
 */
void *
MM_MemoryPoolSegregated::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDesc)
{
	void *result = NULL;
	MM_AllocationContextSegregated *allocationContext = (MM_AllocationContextSegregated *)env->getAllocationContext();

#if defined(OMR_GC_ARRAYLETS)
	if (allocDesc->isArrayletSpine()) {
		result = (void *) allocateContiguous(env, allocDesc, allocationContext);
	} else if (allocDesc->isChunkedArray()) {
		result = allocateChunkedArray(env, allocDesc, allocationContext);
	} else {
		result = (void *) allocateContiguous(env, allocDesc, allocationContext);
	}
#else /* defined(OMR_GC_ARRAYLETS) */
	result = (void *) allocateContiguous(env, allocDesc, allocationContext);
#endif /* defined(OMR_GC_ARRAYLETS) */
	return result;
}



uintptr_t *
MM_MemoryPoolSegregated::allocateContiguous(MM_EnvironmentBase *env,  
										MM_AllocateDescription *allocDesc,
										MM_AllocationContextSegregated *ac)
{
	const uintptr_t sizeInBytesRequired = allocDesc->getContiguousBytes();
	const uintptr_t sizeClass = _extensions->defaultSizeClasses->getSizeClass(sizeInBytesRequired);
	uintptr_t *result = NULL;

	if (sizeClass == OMR_SIZECLASSES_LARGE) {
		/* allocating large also goes through AC so that AC can see and cache the large full page */
		result = ac->allocateLarge(env, sizeInBytesRequired);
	} else {
		result = (uintptr_t*)(MM_SegregatedAllocationInterface::getObjectAllocationInterface(env)->allocateFromCache(env, sizeInBytesRequired));
		if (NULL == result) {
			result = ac->preAllocateSmall(env, sizeInBytesRequired);
		}
	}
	
	return result;
}

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
/**
 * There is currently no TLH in this memory pool.
 */
void *
MM_MemoryPoolSegregated::allocateTLH(MM_EnvironmentBase *env,  uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop)
{
	return NULL;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

void
MM_MemoryPoolSegregated::reset(Cause cause)
{
	Assert_MM_unreachable();
}


void
MM_MemoryPoolSegregated::addRange(MM_EnvironmentBase *env,  void *previousFreeEntry, uintptr_t previousFreeEntrySize, void *currentFreeEntry, uintptr_t currentFreeEntrySize)
{
	/* Used by the sweep routine to add free chunks to the list - ignored here */
	return ;
}

void
MM_MemoryPoolSegregated::insertRange(MM_EnvironmentBase *env, 
	void *previousFreeListEntry, uintptr_t previousFreeListEntrySize,
	void *expandRangeBase, void *expandRangeTop,
	void *nextFreeListEntry, uintptr_t nextFreeListEntrySize)
{
	Assert_MM_unreachable();
}

void *
MM_MemoryPoolSegregated::contractWithRange(MM_EnvironmentBase *env, uintptr_t expandSize, void *lowAddress, void *highAddress)
{
	Assert_MM_unreachable();
	return NULL;
}

void
MM_MemoryPoolSegregated::expandWithRange(MM_EnvironmentBase *env,  uintptr_t expandSize, void *base, void *top, bool canCoalesce)
{
	Assert_MM_unreachable();
}

void
MM_MemoryPoolSegregated::buildRange(MM_EnvironmentBase *env,  void *expandRangeBase, void *expandRangeTop)
{
	abandonHeapChunk(expandRangeBase, expandRangeTop);
}

bool
MM_MemoryPoolSegregated::abandonHeapChunk(void *addrBase, void *addrTop) 
{
	Assert_MM_unreachable();
	return false;
}

uintptr_t
MM_MemoryPoolSegregated::getApproximateFreeMemorySize()
{
	return (_extensions->getHeap()->getHeapRegionManager()->getHeapSize() - getBytesInUse());
}

uintptr_t
MM_MemoryPoolSegregated::getApproximateActiveFreeMemorySize()
{
	return (_extensions->getHeap()->getActiveMemorySize() - getBytesInUse());
}

uintptr_t
MM_MemoryPoolSegregated::getActualFreeMemorySize()
{
	uintptr_t single = 0;
	uintptr_t multi = 0;
	uintptr_t maxMulti = 0;
	uintptr_t coalesce = 0;
	
	_regionPool->countFreeRegions(&single, &multi, &maxMulti, &coalesce);
	
	return (single + multi + coalesce) * _extensions->getHeap()->getHeapRegionManager()->getRegionSize();
}

uintptr_t
MM_MemoryPoolSegregated::getActualFreeEntryCount()
{
	Assert_MM_unreachable();
	return 0;
}

/**
 * Walks all threads to get the unflushed bytes allocated count
 */
uintptr_t
MM_MemoryPoolSegregated::debugGetActualFreeMemorySize()
{
	GC_OMRVMThreadListIterator vmThreadListIterator(_extensions->getOmrVM());
	OMR_VMThread *walkThread;
	uintptr_t totalBytesInUse = _bytesInUse;
	
	while (NULL != (walkThread = vmThreadListIterator.nextOMRVMThread())) {
		MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkThread);
		totalBytesInUse += walkEnv->_allocationTracker->getUnflushedBytesAllocated(walkEnv);
	}
	
	return totalBytesInUse;
}

void
MM_MemoryPoolSegregated::setFreeMemorySize(uintptr_t freeMemorySize)
{
}

void
MM_MemoryPoolSegregated::setFreeEntryCount(uintptr_t entryCount)
{
}

#endif /* OMR_GC_SEGREGATED_HEAP */
