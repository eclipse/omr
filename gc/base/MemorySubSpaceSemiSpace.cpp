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
#include "modronopt.h"

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceRegionIterator.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "PhysicalSubArena.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

/****************************************
 * Allocation
 ****************************************
 */

void *
MM_MemorySubSpaceSemiSpace::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	void *addr = NULL;

	if (shouldCollectOnFailure) {
		addr = _memorySubSpaceAllocate->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
	} else {
		if(previousSubSpace == _parent) {
			/* if we are coming from parent (after a Global GC), pass it down to Allocate */
			addr = _memorySubSpaceAllocate->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		} else if (previousSubSpace == this) {
			/* if retrying after a recent GC (from allocationRequestFailed), pass it down to Allocate */
			addr = _memorySubSpaceAllocate->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		} else {
			/* Allocate subspace failed after a recent GC, try parent */
			Assert_MM_true(previousSubSpace == _memorySubSpaceAllocate);
			if (allocDescription->shouldClimb()) {
				addr = _parent->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
			}
		}
		}
	
	return addr;
}

void *
MM_MemorySubSpaceSemiSpace::allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace)
{
	/* The baseSubSpace in this case isn't really valid - in the event of a scavenge, the baseSubSpace changes.  The current subSpace will become the
	 * base for any restarts
	 */
	void *addr = NULL;

	allocateDescription->saveObjects(env);
	if (!env->tryAcquireExclusiveVMAccessForGC(_collector)) {
		allocateDescription->restoreObjects(env);
		addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, _memorySubSpaceAllocate);
		if(NULL != addr) {
			return addr;
		}

		allocateDescription->saveObjects(env);
		if (!env->acquireExclusiveVMAccessForGC(_collector)) {
			allocateDescription->restoreObjects(env);
			addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, _memorySubSpaceAllocate);
			if(NULL != addr) {
				/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
				 * an event for tracing / verbose to report the occurrence.
				 */
				reportAcquiredExclusiveToSatisfyAllocate(env, allocateDescription);
				return addr;
			}
			allocateDescription->saveObjects(env);
		}
	}

	Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

	reportAllocationFailureStart(env, allocateDescription);
	allocateDescription->setAllocationType(allocationType);
	/* we will pass this as the target subspace for the restarted allocation because the subspaces will swap, if the collection is 
	 * successful, but not if we backout so we don't know which subspace will allocate until after the collect - our own allocation 
	 * routines will call the correct child subspace.
	 *
	 * we might not be the only SemiSpace around (think NUMA). if so, we want to do scavenge on whole new space.
	 * OMRTODO we might be short-cutting here. more formal approach is to pass allocationRequestFailed to parent and he decides what to do
	 */
	MM_MemorySubSpace *topLevelMemorySubSpaceNew = getTopLevelMemorySubSpace(MEMORY_TYPE_NEW);
	addr = _collector->garbageCollect(env, topLevelMemorySubSpaceNew, allocateDescription, J9MMCONSTANT_IMPLICIT_GC_DEFAULT, objectAllocationInterface, this, NULL);
	allocateDescription->restoreObjects(env);

	if(NULL != addr) {
		reportAllocationFailureEnd(env);
		return addr;
	}

	/* Potentially a more aggressive collect here */

	/* Override the baseSubSpace with this - the allocate subspace can change through collections */
	reportAllocationFailureEnd(env);
	
	/*
	 *  TLH: Should we not be checking "collect and climb" here or some such?  why are we not calling the parent? 
	 *	Calling of parent for TLH is wrong because objects allocated into it on the in-line path won't have the OLD bit set
	 *  CMVC 144061 
	 */
	if (ALLOCATION_TYPE_TLH != allocationType) {
		addr = _parent->allocationRequestFailed(env, allocateDescription, allocationType, objectAllocationInterface, this, this);
	}

	return addr;
}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
void
MM_MemorySubSpaceSemiSpace::payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *baseSubSpace, MM_AllocateDescription *allocDescription)
{
}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#if defined(OMR_GC_ARRAYLETS)
void *
MM_MemorySubSpaceSemiSpace::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	void *addr = NULL;

	if (shouldCollectOnFailure) {
		addr = _memorySubSpaceAllocate->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
	} else {
		if(previousSubSpace == _parent) {
			/* if we are coming from parent (after a Global GC), pass it down to Allocate */
			addr = _memorySubSpaceAllocate->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		} else if (previousSubSpace == this) {
			/* if retrying after a recent GC (from allocationRequestFailed), pass it down to Allocate */
			addr = _memorySubSpaceAllocate->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		} else {
			/* Allocate subspace failed after a recent GC, try parent */
			Assert_MM_true(previousSubSpace == _memorySubSpaceAllocate);
			if (allocDescription->shouldClimb()) {
				addr = _parent->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
			}
		}
		}
	
	return addr;	
}
#endif /* OMR_GC_ARRAYLETS */

void *
MM_MemorySubSpaceSemiSpace::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	/* We may receive this call from the MemorySubSpaceGeneric for which we are the parent but we shouldn't satisfy TLH allocates 
	 * in this space (since that would allow us to fill old space before collecting new space) so just return NULL.
	 * This call has been observed on Vich configurations, at the very least.
	 */
	return NULL;
}

/****************************************
 * Explicit Collection
 ****************************************
 */
void
MM_MemorySubSpaceSemiSpace::systemGarbageCollect(MM_EnvironmentBase *env, uint32_t gcCode)
{
	if(_collector) {
		env->acquireExclusiveVMAccessForGC(_collector);
		reportSystemGCStart(env, gcCode);

		_collector->garbageCollect(env, this, NULL, gcCode, NULL, NULL, NULL);
		
		reportSystemGCEnd(env);
		env->releaseExclusiveVMAccessForGC();
	}
}

/**
 * If the child subSpace is active return true, else return false. 
 */
bool
MM_MemorySubSpaceSemiSpace::isChildActive(MM_MemorySubSpace *memorySubSpace)
{
	return memorySubSpace == _memorySubSpaceAllocate;
}

/****************************************
 * Sub Space Categorization
 ****************************************
 */
MM_MemorySubSpace *
MM_MemorySubSpaceSemiSpace::getDefaultMemorySubSpace()
{
	return _memorySubSpaceAllocate;
}

bool
MM_MemorySubSpaceSemiSpace::isObjectInEvacuateMemory(omrobjectptr_t objectPtr)
{
	/* check if the object in cached allocate (from GC perspective evacuate) ranges */
	return ((void *)objectPtr >= _allocateSpaceBase) && ((void *)objectPtr < _allocateSpaceTop);
}

bool
MM_MemorySubSpaceSemiSpace::isObjectInNewSpace(omrobjectptr_t objectPtr)
{
	return ((void *)objectPtr >= _survivorSpaceBase) && ((void *)objectPtr < _survivorSpaceTop);
}

bool
MM_MemorySubSpaceSemiSpace::isObjectInNewSpace(void *objectBase, void *objectTop)
{
	return (objectBase >= _survivorSpaceBase) && (objectTop <= _survivorSpaceTop);
}

uintptr_t
MM_MemorySubSpaceSemiSpace::getMaxSpaceForObjectInEvacuateMemory(omrobjectptr_t objectPtr)
{
	uintptr_t result = 0;
	if (isObjectInEvacuateMemory(objectPtr)) {
		result = (uintptr_t)_allocateSpaceTop - (uintptr_t)objectPtr;
	}
	return result;
}

/****************************************
 * Free list building
 ****************************************
 */
uintptr_t
MM_MemorySubSpaceSemiSpace::getActiveMemorySize()
{
	return _memorySubSpaceAllocate->getActiveMemorySize();
}

uintptr_t
MM_MemorySubSpaceSemiSpace::getActiveMemorySize(uintptr_t includeMemoryType)
{
	if (includeMemoryType & MEMORY_TYPE_NEW){ 
		return _memorySubSpaceAllocate->getActiveMemorySize() + _memorySubSpaceSurvivor->getActiveMemorySize();
	} else {
		return 0;
	}		
}

/**
 * @copydoc MM_MemorySubSpace::getActualActiveFreeMemorySize()
 * The semispace implementation will only report free memory associated to the allocate memory subspace.
 */
uintptr_t
MM_MemorySubSpaceSemiSpace::getActualActiveFreeMemorySize()
{
	return _memorySubSpaceAllocate->getActualActiveFreeMemorySize();
}

/**
 * @copydoc MM_MemorySubSpace::getActualActiveFreeMemorySize(uintptr_t)
 * The semispace implementation will only report free memory associated to the allocate memory subspace.
 */
uintptr_t
MM_MemorySubSpaceSemiSpace::getActualActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	if (includeMemoryType & MEMORY_TYPE_NEW){ 
		return _memorySubSpaceAllocate->getActualActiveFreeMemorySize() + getActualActiveFreeSurvivorMemorySize(includeMemoryType);
	} else {
		return 0;
	}	
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeMemorySize()
 * The semispace implementation will only report free memory associated to the allocate memory subspace.
 */
uintptr_t
MM_MemorySubSpaceSemiSpace::getApproximateActiveFreeMemorySize()
{
	return _memorySubSpaceAllocate->getApproximateActiveFreeMemorySize();
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeMemorySize(uintptr_t)
 * The semispace implementation will only report free memory associated to the allocate memory subspace.
 */
uintptr_t
MM_MemorySubSpaceSemiSpace::getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	if (includeMemoryType & MEMORY_TYPE_NEW){ 
		return _memorySubSpaceAllocate->getApproximateActiveFreeMemorySize() + getApproximateActiveFreeSurvivorMemorySize(includeMemoryType);
	} else {
		return 0;
	}	
}

uintptr_t
MM_MemorySubSpaceSemiSpace::getActiveSurvivorMemorySize(uintptr_t includeMemoryType)
{
	if (getTypeFlags() & MEMORY_TYPE_NEW) {
		return _memorySubSpaceSurvivor->getActiveMemorySize(MEMORY_TYPE_NEW);
	} else {
		return 0;
	}
}

uintptr_t
MM_MemorySubSpaceSemiSpace::getApproximateActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType)
{
	/* If we ask directly MemorySubSpaceSurvivor, we'll get as answer: everything is free. While it is true literally,
	 * this memory is unavaliable for user allocation
	 */
	return 0;
}

uintptr_t
MM_MemorySubSpaceSemiSpace::getActualActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType)
{
	/* If we ask directly MemorySubSpaceSurvivor, we'll get as answer: everything is free. While it is true literally,
	 * this memory is unavaliable for user allocation
	 */
	return 0;
}

void
MM_MemorySubSpaceSemiSpace::abandonHeapChunk(void *addrBase, void *addrTop)
{
	Assert_MM_unreachable();
}

/**
 */
void
MM_MemorySubSpaceSemiSpace::mergeHeapStats(MM_HeapStats *heapStats)
{
	_memorySubSpaceAllocate->mergeHeapStats(heapStats);
	_memorySubSpaceSurvivor->mergeHeapStats(heapStats);
} 
 
void
MM_MemorySubSpaceSemiSpace::mergeHeapStats(MM_HeapStats *heapStats, uintptr_t includeMemoryType)
{
	if (includeMemoryType & MEMORY_TYPE_NEW){ 
		_memorySubSpaceAllocate->mergeHeapStats(heapStats);
		_memorySubSpaceSurvivor->mergeHeapStats(heapStats);
	}	
}

/**
 * Initialization
 */
MM_MemorySubSpaceSemiSpace *
MM_MemorySubSpaceSemiSpace::newInstance(MM_EnvironmentBase *env, MM_Collector *collector, MM_PhysicalSubArena *physicalSubArena, MM_MemorySubSpace *memorySubSpaceAllocate, MM_MemorySubSpace *memorySubSpaceSurvivor, bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize)
{
	MM_MemorySubSpaceSemiSpace *memorySubSpace;
	
	memorySubSpace = (MM_MemorySubSpaceSemiSpace *)env->getForge()->allocate(sizeof(MM_MemorySubSpaceSemiSpace), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memorySubSpace) {
		new(memorySubSpace) MM_MemorySubSpaceSemiSpace(env, collector, physicalSubArena, memorySubSpaceAllocate, memorySubSpaceSurvivor, usesGlobalCollector, minimumSize, initialSize, maximumSize);
		if (!memorySubSpace->initialize(env)) {
			memorySubSpace->kill(env);
			memorySubSpace = NULL;
		}
	}
	return memorySubSpace;
}

bool
MM_MemorySubSpaceSemiSpace::initialize(MM_EnvironmentBase *env)
{
	if(!MM_MemorySubSpace::initialize(env)) {
		return false;
	}

	_previousBytesFlipped = getMinimumSize() / 2;
	_tiltedAverageBytesFlipped = _previousBytesFlipped;
	_tiltedAverageBytesFlippedDelta = _previousBytesFlipped;
	
	/* register the children */
	registerMemorySubSpace(_memorySubSpaceSurvivor);
	registerMemorySubSpace(_memorySubSpaceAllocate);

	_memorySubSpaceSurvivor->isAllocatable(false);

	_largeObjectAllocateStats = MM_LargeObjectAllocateStats::newInstance(env, (uint16_t)_extensions->largeObjectAllocationProfilingTopK, _extensions->largeObjectAllocationProfilingThreshold, _extensions->largeObjectAllocationProfilingVeryLargeObjectThreshold, (float)_extensions->largeObjectAllocationProfilingSizeClassRatio / (float)100.0,
			_extensions->heap->getMaximumMemorySize(), _extensions->tlhMaximumSize + _extensions->minimumFreeEntrySize, _extensions->tlhMinimumSize);
	if (NULL == _largeObjectAllocateStats) {
		return false;
	}

	return true;
}

void
MM_MemorySubSpaceSemiSpace::tearDown(MM_EnvironmentBase *env)
{
	MM_MemorySubSpace::tearDown(env);

	if (NULL != _largeObjectAllocateStats) {
		_largeObjectAllocateStats->kill(env);
		_largeObjectAllocateStats = NULL;
	}
}

/****************************************
 * Type specific functionality
 ****************************************
 */
void
MM_MemorySubSpaceSemiSpace::flip(MM_EnvironmentBase *env, Flip_step step)
{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	bool debug = _extensions->debugTiltedScavenge;
#endif

	switch (step) {
	case set_evacuate:
		_memorySubSpaceEvacuate = _memorySubSpaceAllocate;
		_memorySubSpaceEvacuate->isAllocatable(false);
		break;
	case set_allocate:
		_memorySubSpaceAllocate = _memorySubSpaceSurvivor;
		_memorySubSpaceAllocate->isAllocatable(true);
		/* Let know MemorySpace about new default MemorySubSpace */
		getMemorySpace()->setDefaultMemorySubSpace(getDefaultMemorySubSpace());
		break;
	case disable_allocation:
		_memorySubSpaceAllocate->isAllocatable(false);
		break;
	case restore_allocation_and_set_survivor:
		_memorySubSpaceAllocate->isAllocatable(true);
		_memorySubSpaceSurvivor = _memorySubSpaceEvacuate;
		break;
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	case backout:
		/* We have objects on both sides of Nursery. We will unify the two sides and do a compacting slide (after percolate global GC)
		 * Enforce Allocate be in low address range, since compact slide in percolate global GC moves objects to low addresses.
		 * Note: _allocateSpaceBase/Top are stale (not updated in set_allocate), since they are overloaded to point to Evacuate during an active cycle
		 */
		if (_allocateSpaceBase < _survivorSpaceBase) {
			_memorySubSpaceAllocate = _memorySubSpaceEvacuate;
			_memorySubSpaceEvacuate = _memorySubSpaceSurvivor;
			getMemorySpace()->setDefaultMemorySubSpace(getDefaultMemorySubSpace());
			if(debug) {
				omrtty_printf("tilt backout _allocateSpaceBase/Top %llx/%llx _survivorSpaceBase/Top %llx/%llx tilt sizes %llx %llx\n",
						_allocateSpaceBase, _allocateSpaceTop, _survivorSpaceBase, _survivorSpaceTop,
						(uintptr_t)_allocateSpaceTop - (uintptr_t)_allocateSpaceBase +  (uintptr_t)_survivorSpaceTop - (uintptr_t)_survivorSpaceBase, (uintptr_t)0);
			}
		} else {
			_memorySubSpaceSurvivor = _memorySubSpaceEvacuate;
			cacheRanges(_memorySubSpaceAllocate, &_allocateSpaceBase, &_allocateSpaceTop);
			cacheRanges(_memorySubSpaceSurvivor, &_survivorSpaceBase, &_survivorSpaceTop);
			if(debug) {
				omrtty_printf("tilt backout forced flip _allocateSpaceBase/Top %llx/%llx _survivorSpaceBase/Top %llx/%llx tilt sizes %llx %llx\n",
						_allocateSpaceBase, _allocateSpaceTop, _survivorSpaceBase, _survivorSpaceTop,
						(uintptr_t)_allocateSpaceTop - (uintptr_t)_allocateSpaceBase +  (uintptr_t)_survivorSpaceTop - (uintptr_t)_survivorSpaceBase, (uintptr_t)0);
			}
		}

		/* Tilt 100% on PhysicalSubArena level (leave SubSpaces unchanged). Give everything to the low address (Allocate) part */
		_physicalSubArena->tilt(env, (uintptr_t)_allocateSpaceTop - (uintptr_t)_allocateSpaceBase +  (uintptr_t)_survivorSpaceTop - (uintptr_t)_survivorSpaceBase, (uintptr_t)0, false);

		/* disable potentially successful allocation (in case of forced abort) to trigger another (percolate) GC */
		_memorySubSpaceAllocate->isAllocatable(false);
		getMemorySpace()->getTenureMemorySubSpace()->isAllocatable(false);

		break;
	case restore_tilt_after_percolate:
	{
		/* Make Survivor space from the last free entry in the unified Nursery */
		uintptr_t heapAlignedLastFreeEntrySize = MM_Math::roundToFloor(_extensions->heapAlignment, getDefaultMemorySubSpace()->getMemoryPool()->getLastFreeEntry()->getSize());

		/* for now, assumed fixed size of 64 sections in Nursery */
		const uint64_t maxSectionCount = 64;
		uint64_t sectionSize = (getCurrentSize()) / maxSectionCount;

		heapAlignedLastFreeEntrySize = MM_Math::roundToFloor(sectionSize, heapAlignedLastFreeEntrySize);
		if(debug) {
			omrtty_printf("tilt restore_tilt_after_percolate heapAlignedLastFreeEntry %llx section (%llx) aligned size %llx\n",
					getDefaultMemorySubSpace()->getMemoryPool()->getLastFreeEntry(), sectionSize, heapAlignedLastFreeEntrySize);
		}

		/* allocate/survivor base/top still hold the values from before we did 100% tilt */
		uintptr_t allocateSize = (uintptr_t)_allocateSpaceTop - (uintptr_t)_allocateSpaceBase;
		uintptr_t survivorSize = (uintptr_t)_survivorSpaceTop - (uintptr_t)_survivorSpaceBase;
		if (allocateSize < survivorSize) {
			allocateSize = survivorSize;
			survivorSize = (uintptr_t)_allocateSpaceTop - (uintptr_t)_allocateSpaceBase;
		}

		if(debug) {
			omrtty_printf("tilt restore_tilt_after_percolate allocateSize %llx survivorSize %llx\n", allocateSize, survivorSize);
		}
		if (heapAlignedLastFreeEntrySize < survivorSize) {
			allocateSize += (survivorSize - heapAlignedLastFreeEntrySize);
			survivorSize = heapAlignedLastFreeEntrySize;
		}
		if(debug) {
			omrtty_printf("tilt restore_tilt_after_percolate adjusted allocateSize %llx survivorSize %llx\n", allocateSize, survivorSize);
		}

		tilt(env, allocateSize, survivorSize);

		/* Restore allocation, which we had disabled in the backout step */
		_memorySubSpaceAllocate->isAllocatable(true);
		getMemorySpace()->getTenureMemorySubSpace()->isAllocatable(true);

	}
		break;
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	default:
		Assert_MM_unreachable();
	}
}

void
MM_MemorySubSpaceSemiSpace::cacheRanges(MM_MemorySubSpace *subSpace, void **base, void **top)
{
	GC_MemorySubSpaceRegionIterator regionIterator(subSpace);
	MM_HeapRegionDescriptor* region = regionIterator.nextRegion();
	Assert_MM_true(NULL != region);
	Assert_MM_true(NULL == regionIterator.nextRegion());
	*base = region->getLowAddress();
	*top = region->getHighAddress();
}

void
MM_MemorySubSpaceSemiSpace::masterSetupForGC(MM_EnvironmentBase *env)
{
	/* cache allocate (effectively evacuate for GC) ranges */
	cacheRanges(_memorySubSpaceAllocate, &_allocateSpaceBase, &_allocateSpaceTop);
	cacheRanges(_memorySubSpaceSurvivor, &_survivorSpaceBase, &_survivorSpaceTop);

	flip(env, set_evacuate);
}

void
MM_MemorySubSpaceSemiSpace::poisonEvacuateSpace()
{
	/* poison allocate (effectively effectively evacuate for the caller, GC) */
	const uintptr_t pattern = (uintptr_t)-1;
	uintptr_t* current = (uintptr_t*) _allocateSpaceBase;
	uintptr_t* end = (uintptr_t*) _allocateSpaceTop;

	while (current < end) {
		*current = pattern;
		current += 1;
	}
}

void
MM_MemorySubSpaceSemiSpace::tilt(MM_EnvironmentBase *env, uintptr_t allocateSpaceSize, uintptr_t survivorSpaceSize)
{
	_physicalSubArena->tilt(env, allocateSpaceSize, survivorSpaceSize);
}

void
MM_MemorySubSpaceSemiSpace::tilt(MM_EnvironmentBase *env, uintptr_t survivorSpaceSizeRatioRequest)
{
	_physicalSubArena->tilt(env, survivorSpaceSizeRatioRequest);
}

void
MM_MemorySubSpaceSemiSpace::masterTeardownForSuccessfulGC(MM_EnvironmentBase *env)
{
	_memorySubSpaceEvacuate->rebuildFreeList(env);

	/* Flip the memory space allocate profile */
#if !defined(OMR_GC_CONCURRENT_SCAVENGER)
	flip(env, set_allocate);
	flip(env, disable_allocation);
#endif
	flip(env, restore_allocation_and_set_survivor);

	/* Adjust memory between the semi spaces where applicable */
	checkResize(env);
	performResize(env);
}


void
MM_MemorySubSpaceSemiSpace::masterTeardownForAbortedGC(MM_EnvironmentBase *env)
{
	/* Build free list in survivor. */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/* There might be live objects in Survivor (newly allocated one since the start of Concurrent Scavenge cycle)
	 * Sweep in percolate global will rebuild it, so we can skip it here
	 */
	flip(env, backout);
#else
	_memorySubSpaceSurvivor->rebuildFreeList(env);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

}

/**
 * Adjust the sub space memory by tilting the split between allocate and survivor space.
 */
void
MM_MemorySubSpaceSemiSpace::checkSubSpaceMemoryPostCollectTilt(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());

	if(extensions->tiltedScavenge) {
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		/* for now, support static tilting only */
		_desiredSurvivorSpaceRatio = extensions->survivorSpaceMinimumSizeRatio;
#else
		uintptr_t flipBytes;
		uintptr_t flipBytesDelta;
		bool debug = extensions->debugTiltedScavenge;
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

		/* OMRTODO consider maintaining _flipBytes separately for each NUMA SemiSpace.
		 * Currently, _flipBytes is cumulative for all SemiSpaces. To compensate, current size is obtained
		 * for all of SemiSpaces too (from top level New SubSpace). This will result in blended tilt ratio,
		 * set equally for all of SemiSpaces.
		 */
		uintptr_t currentSize = getTopLevelMemorySubSpace(MEMORY_TYPE_NEW)->getCurrentSize();

		if(debug) {
			omrtty_printf("\nTilt check:\n");
		}

		flipBytes = extensions->scavengerStats._flipBytes + extensions->scavengerStats._failedFlipBytes;

		if(debug) {
			omrtty_printf("\tBytes flip:%zu fail:%zu total:%zu\n",
				extensions->scavengerStats._flipBytes,
				extensions->scavengerStats._failedFlipBytes,
				flipBytes);
		}

		/* Determine the absolute value of the change in bytes flipped since the last scavenge */
		if(flipBytes > _previousBytesFlipped) {
			flipBytesDelta = flipBytes - _previousBytesFlipped;
		} else {
			flipBytesDelta = _previousBytesFlipped - flipBytes;
		}

		if(debug) {
			omrtty_printf("\tflip delta from last (%zu):%zu\n", _previousBytesFlipped, flipBytesDelta);
		}

		/* Current flip count becomes previous */
		_previousBytesFlipped = flipBytes;

		/* The average bytes flipped weighted average is more heavily affected if the current size is
		 * greater than the current average
		 */
		if(debug) {
			omrtty_printf("\tcurrent average bytes flipped: %zu (avg delta %zu)\n",
				_tiltedAverageBytesFlipped,
				_tiltedAverageBytesFlippedDelta);
		}

		/* Check if there was any failed to flip objects - which puts us into a bit of a panic mode */
		if(0 != extensions->scavengerStats._failedFlipCount) {
			/* Panic mode - the current numbers should heavily influence the averages */
			if(debug) {
				omrtty_printf("\tfailed flip weight\n");
			}
			_tiltedAverageBytesFlipped = (uintptr_t)MM_Math::weightedAverage((float)_tiltedAverageBytesFlipped, (float)flipBytes, 0.0f);
			_tiltedAverageBytesFlippedDelta = (uintptr_t)MM_Math::weightedAverage((float)_tiltedAverageBytesFlippedDelta, (float)flipBytesDelta, 0.0f);
		} else {
			/* No panic situation - determine the new tilt ratio through normal means */
			if(flipBytes > _tiltedAverageBytesFlipped) {
				if(debug) {
					omrtty_printf("\tincrease flip weight\n");
				}
				_tiltedAverageBytesFlipped = (uintptr_t)MM_Math::weightedAverage((float)_tiltedAverageBytesFlipped, (float)flipBytes, 0.2f);
				_tiltedAverageBytesFlippedDelta = (uintptr_t)MM_Math::weightedAverage((float)_tiltedAverageBytesFlippedDelta, (float)flipBytesDelta, 0.2f);
			} else {
				if(debug) {
					omrtty_printf("\tdecrease flip weight\n");
				}
				_tiltedAverageBytesFlipped = (uintptr_t)MM_Math::weightedAverage((float)_tiltedAverageBytesFlipped, (float)flipBytes, 0.8f);
				_tiltedAverageBytesFlippedDelta = (uintptr_t)MM_Math::weightedAverage((float)_tiltedAverageBytesFlippedDelta, (float)flipBytesDelta, 0.8f);
			}
		}

		if(debug) {
			omrtty_printf("\tnew average bytes flipped: %zu (avg delta %zu)\n",
				_tiltedAverageBytesFlipped,
				_tiltedAverageBytesFlippedDelta);
		}

		/* Calculate the desired survivor space ratio */
		
		double survivorSizeAmplification = 1.04 + extensions->dispatcher->threadCount() / 100.0;
		_desiredSurvivorSpaceRatio = (_tiltedAverageBytesFlipped + _tiltedAverageBytesFlippedDelta)
										* survivorSizeAmplification	/ currentSize;
										
		if(debug) {
			omrtty_printf("\tDesired survivor size: %zu  ratio: %zu\n",
				(uintptr_t)(currentSize * _desiredSurvivorSpaceRatio),	(uintptr_t) (_desiredSurvivorSpaceRatio * 100));
		}

		/* Sanity check for lowest possible ratio */
		if (_desiredSurvivorSpaceRatio <  extensions->survivorSpaceMinimumSizeRatio) {
			_desiredSurvivorSpaceRatio =  extensions->survivorSpaceMinimumSizeRatio;
		}

		/* Never hand out more than 50% to the survivor */
		if (_desiredSurvivorSpaceRatio > extensions->survivorSpaceMaximumSizeRatio) {
			_desiredSurvivorSpaceRatio = extensions->survivorSpaceMaximumSizeRatio;			
		}
		
		/* we have flipped already, so to get old survivor space ratio we fetch allocate size */
		double previousSurvivorSpaceRatio = (double) _memorySubSpaceAllocate->getActiveMemorySize() / currentSize;
		
		/* Do not let survivor space shrink by more than the maximum tilt increase */
		assume0(previousSurvivorSpaceRatio >= extensions->tiltedScavengeMaximumIncrease);
		if (_desiredSurvivorSpaceRatio < (previousSurvivorSpaceRatio - extensions->tiltedScavengeMaximumIncrease)) {
			_desiredSurvivorSpaceRatio = previousSurvivorSpaceRatio -  extensions->tiltedScavengeMaximumIncrease;
		}
		
		if(debug) {
			omrtty_printf("\tPrevious survivor ratio: %zu\n", (uintptr_t) (previousSurvivorSpaceRatio * 100));			
			omrtty_printf("\tAdjusted survivor size: %zu  ratio: %zu\n",(uintptr_t)(currentSize * _desiredSurvivorSpaceRatio),
				(uintptr_t) (_desiredSurvivorSpaceRatio * 100));
		}
#endif /* defined(OMR_GC_CONCURRENT_SCAVENGER) */
	}
}

/**
 * Adjust the sub space memory by resizing the allocate and survivor space (expand or contract).
 */
void
MM_MemorySubSpaceSemiSpace::checkSubSpaceMemoryPostCollectResize(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	uintptr_t regionSize = extensions->getHeap()->getHeapRegionManager()->getRegionSize();

	if(extensions->dynamicNewSpaceSizing) {
		bool doDynamicNewSpaceSizing = true;
		bool debug = extensions->debugDynamicNewSpaceSizing;
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

		if(debug) {
			omrtty_printf("New space resize check:\n");
		}

		if (extensions->scavengerStats._gcCount == 1){
			if(debug) {
				omrtty_printf("\tNo previous scavenge - ABORTING\n");
			}
			doDynamicNewSpaceSizing = false;
		}

		/* the wall clock might be shifted backwards externally */
		if (extensions->scavengerStats._startTime < _lastScavengeEndTime) {
			/* clock has been shifted backwards between scavenges */
			if(debug) {
				omrtty_printf("\tClock shifted backwards between scavenges - ABORTING\n");
			}
			doDynamicNewSpaceSizing = false;
		}

		if (extensions->scavengerStats._endTime < extensions->scavengerStats._startTime) {
			/* clock has been shifted backwards at the time of the scavenge */
			if(debug) {
				omrtty_printf("\tClock shifted backwards at the time of the scavenge - ABORTING\n");
			}
			doDynamicNewSpaceSizing = false;
		}

		uint64_t intervalTime = omrtime_hires_delta(_lastScavengeEndTime, extensions->scavengerStats._endTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS);

		if(0 == intervalTime) {
			if(debug) {
				omrtty_printf("\tInterval time 0 - ABORTING\n");
			}
			doDynamicNewSpaceSizing = false;
		}

		uint64_t scavengeTime = omrtime_hires_delta(extensions->scavengerStats._startTime, extensions->scavengerStats._endTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS );

		if(0 == scavengeTime) {
			if(debug) {
				omrtty_printf("\tScavenge time 0 - ABORTING\n");
			}
			doDynamicNewSpaceSizing = false;
		}

		_lastScavengeEndTime = extensions->scavengerStats._endTime;

		if (doDynamicNewSpaceSizing) {
			double expectedTimeRatio = (extensions->dnssExpectedTimeRatioMaximum + extensions->dnssExpectedTimeRatioMinimum) / 2;

			/* Find the ratio of time to scavenge versus the interval time since the last scavenge */
			double timeRatio = (double)((int64_t)scavengeTime) / (double)((int64_t) intervalTime);
			
			if(debug) {
				omrtty_printf("\tTime scav:%llu interval:%llu ratio:%lf\n", scavengeTime, intervalTime, timeRatio);
				omrtty_printf("\tAverage scavenge time ratio: %lf -> ", _averageScavengeTimeRatio);
			}

			/* Based on the current time ratio versus the average time ratio, determine what weight to assign when
			 * calculating the new average
			 */
			double weight;
			if(timeRatio > _averageScavengeTimeRatio) {
				if(timeRatio > expectedTimeRatio) {
					if(timeRatio > extensions->dnssExpectedTimeRatioMaximum) {
						weight = extensions->dnssWeightedTimeRatioFactorIncreaseLarge;
					} else {
						weight = extensions->dnssWeightedTimeRatioFactorIncreaseMedium;
					}
				} else {
					weight = extensions->dnssWeightedTimeRatioFactorIncreaseSmall;
				}
			} else {
				weight = extensions->dnssWeightedTimeRatioFactorDecrease;
			}

			/* Weight determined - calculate the new average */
			_averageScavengeTimeRatio = (timeRatio * weight) + (_averageScavengeTimeRatio * (1.0 - weight));

			if(debug) {
				omrtty_printf("%lf (weight %lf)\n", _averageScavengeTimeRatio, weight);
			}

			/* If the average scavenge to interval ratio is greater than the maximum, try to expand */
			if((_averageScavengeTimeRatio > extensions->dnssExpectedTimeRatioMaximum)
					&& (NULL != _physicalSubArena) && _physicalSubArena->canExpand(env) && (maxExpansionInSpace(env) != 0)) {
				double desiredExpansionFactor, adjustedExpansionFactor;

				/* Try to reach 50% of the expected time ratio through expansion */
				desiredExpansionFactor = _averageScavengeTimeRatio - (expectedTimeRatio / 2);

				if (desiredExpansionFactor > extensions->dnssMaximumExpansion) {
					adjustedExpansionFactor = extensions->dnssMaximumExpansion;
				} else if (desiredExpansionFactor < extensions->dnssMinimumExpansion) {
					adjustedExpansionFactor = extensions->dnssMinimumExpansion;
				} else {
					adjustedExpansionFactor = desiredExpansionFactor;
				}

				/* Adjust the average scavenge to interval ratio */
				_averageScavengeTimeRatio -= adjustedExpansionFactor;

				_expansionSize = MM_Math::roundToCeiling(extensions->heapAlignment, (uintptr_t)(getCurrentSize() * adjustedExpansionFactor));
				_expansionSize = MM_Math::roundToCeiling(regionSize, _expansionSize);

				if(debug) {
					omrtty_printf("\tExpand decision - expandFactor desired: %lf adjusted: %lf size: %u\n", desiredExpansionFactor, adjustedExpansionFactor, _expansionSize);
					omrtty_printf("\tExpand decision - current size: %d expanded size: %d\n", getCurrentSize(), getCurrentSize() + _expansionSize);
					omrtty_printf("\tExpand decision - new time ratio:%lf\n\n\n", _averageScavengeTimeRatio);
				}

				extensions->heap->getResizeStats()->setLastExpandReason(SCAV_RATIO_TOO_HIGH);
			}

			/* If the average scavenge to interval ratio is less than the minimum, try to contract */
			if(_averageScavengeTimeRatio < extensions->dnssExpectedTimeRatioMinimum
					&& (NULL != _physicalSubArena) && _physicalSubArena->canContract(env) && (maxContractionInSpace(env) != 0)) {
				double desiredContractionFactor, adjustedContractionFactor;

				/* Try to reach 200% of the expected minimum time ratio through contraction */
				desiredContractionFactor = OMR_MIN(extensions->dnssExpectedTimeRatioMinimum  * 2, expectedTimeRatio);
				desiredContractionFactor = desiredContractionFactor - _averageScavengeTimeRatio;

				if (desiredContractionFactor > extensions->dnssMaximumContraction) {
					adjustedContractionFactor = extensions->dnssMaximumContraction;
				} else if (desiredContractionFactor < extensions->dnssMinimumContraction) {
					adjustedContractionFactor = extensions->dnssMinimumContraction;
				} else {
					adjustedContractionFactor = desiredContractionFactor;
				}

				/* Adjust the average scavenge to interval ratio */
				_averageScavengeTimeRatio += adjustedContractionFactor;


				_contractionSize = MM_Math::roundToCeiling(extensions->heapAlignment, (uintptr_t)(getCurrentSize() * adjustedContractionFactor));
				_contractionSize = MM_Math::roundToCeiling(regionSize, _contractionSize);

				if(debug) {
					omrtty_printf("\tContract decision - contractFactor desired: %lf adjusted: %lf size: %u\n", desiredContractionFactor, adjustedContractionFactor, _contractionSize);
					omrtty_printf("\tContract decision - current size: %d contracted size: %d\n", getCurrentSize(), getCurrentSize() - _contractionSize);
					omrtty_printf("\tContract decision - new time ratio:%lf\n\n\n", _averageScavengeTimeRatio);
				}

				extensions->heap->getResizeStats()->setLastContractReason(SCAV_RATIO_TOO_LOW);
			}
		}
	}
}

/**
 * Adjust the sub space memory consumed after a collect.
 * Adjusting semi space memory consumed after a collect includes changing the tilt and/or
 * expanding/contracting.  Both options are compile and runtime selectable.
 * 
 */
void
MM_MemorySubSpaceSemiSpace::checkResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool systemGC)
{
	uintptr_t oldVMState = env->pushVMstate(J9VMSTATE_GC_CHECK_RESIZE);
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/* this we are called at the end of precolate global GC, due to aborted Concurrent Scavenge,
	 * we have to restore tilt (that has been set to 100% to do unified sliding compact of Nursery */
	if (J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_ABORTED_SCAVENGE == env->_cycleState->_gcCode.getCode()) {
		flip(env, MM_MemorySubSpaceSemiSpace::restore_tilt_after_percolate);
	} else
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	{
		checkSubSpaceMemoryPostCollectTilt(env);
		checkSubSpaceMemoryPostCollectResize(env);
	}
	env->popVMstate(oldVMState);
}

intptr_t
MM_MemorySubSpaceSemiSpace::performResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	uintptr_t oldVMState = env->pushVMstate(J9VMSTATE_GC_PERFORM_RESIZE);
	uintptr_t regionSize = env->getExtensions()->getHeap()->getHeapRegionManager()->getRegionSize();
	
	if (_desiredSurvivorSpaceRatio > 0.0) {
		uintptr_t desiredSurvivorSpaceSize = (uintptr_t)(getCurrentSize() * _desiredSurvivorSpaceRatio);

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		/* for now, assumed fixed size of 64 sections in Nursery */
		const uint64_t maxSectionCount = 64;
		uint64_t sectionSize = (getCurrentSize()) / maxSectionCount;
		desiredSurvivorSpaceSize = MM_Math::roundToCeiling(sectionSize, desiredSurvivorSpaceSize);
#else
		desiredSurvivorSpaceSize = MM_Math::roundToCeiling(regionSize, desiredSurvivorSpaceSize);
#endif
		tilt(env, desiredSurvivorSpaceSize);
		_desiredSurvivorSpaceRatio = 0.0;
	}
	
	/* If -Xgc:fvtest=forceNurseryResize is specified, then repeat a sequence of 5 expands followed by 5 contracts */	
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	if (extensions->fvtest_forceNurseryResize) {
		uintptr_t resizeAmount = 0;
		resizeAmount = 2*regionSize;
		if (5 > extensions->fvtest_nurseryResizeCounter) {
			uintptr_t expansionSize = MM_Math::roundToCeiling(extensions->heapAlignment, resizeAmount);
			expansionSize = MM_Math::roundToCeiling(regionSize, expansionSize);
			if (canExpand(env, expansionSize)) {
				extensions->heap->getResizeStats()->setLastExpandReason(FORCED_NURSERY_EXPAND);
				_contractionSize = 0;
				_expansionSize = expansionSize;
				extensions->fvtest_nurseryResizeCounter += 1;
			}
		} else if (10 > extensions->fvtest_nurseryResizeCounter) {
			uintptr_t contractionSize = MM_Math::roundToCeiling(extensions->heapAlignment, resizeAmount);
			contractionSize = MM_Math::roundToCeiling(regionSize, contractionSize);
			if (canContract(env, contractionSize)) {
				_contractionSize = contractionSize;
				extensions->heap->getResizeStats()->setLastContractReason(FORCED_NURSERY_CONTRACT);
				_expansionSize = 0;
				extensions->fvtest_nurseryResizeCounter += 1;
			}
		} 
		
		if (10 <= extensions->fvtest_nurseryResizeCounter) {
			extensions->fvtest_nurseryResizeCounter = 0;
		}
	}
	
	if (0 != _expansionSize) {
		expand(env, _expansionSize);
	} else if (0 != _contractionSize) {
		contract(env, _contractionSize);
	}

	_expansionSize = 0;
	_contractionSize = 0;
	
	env->popVMstate(oldVMState);

	return 0;
}

MM_LargeObjectAllocateStats *
MM_MemorySubSpaceSemiSpace::getLargeObjectAllocateStats()
{
	return _largeObjectAllocateStats;
}

void
MM_MemorySubSpaceSemiSpace::mergeLargeObjectAllocateStats(MM_EnvironmentBase *env)
{
	_largeObjectAllocateStats->resetCurrent();
	_memorySubSpaceAllocate->getMemoryPool()->mergeLargeObjectAllocateStats();
	_memorySubSpaceSurvivor->getMemoryPool()->mergeLargeObjectAllocateStats();
	_largeObjectAllocateStats->mergeCurrent(_memorySubSpaceAllocate->getMemoryPool()->getLargeObjectAllocateStats());
	_largeObjectAllocateStats->mergeCurrent(_memorySubSpaceSurvivor->getMemoryPool()->getLargeObjectAllocateStats());
}

#endif /* OMR_GC_MODRON_SCAVENGER */

