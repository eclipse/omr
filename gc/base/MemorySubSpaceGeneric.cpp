/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
#include "modronopt.h"

#include "MemorySubSpaceGeneric.hpp"

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "HeapStats.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceRegionIterator.hpp"
#include "ObjectAllocationInterface.hpp"
#include "RegionPool.hpp"

/**
 * Return the memory pool associated to the receiver.
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemorySubSpaceGeneric::getMemoryPool()
{
	return _memoryPool;
}

/**
 * Return the number of memory pools associated to the receiver.
 * @return count of number of memory pools 
 */
uintptr_t
MM_MemorySubSpaceGeneric::getMemoryPoolCount()
{
	return _memoryPool->getMemoryPoolCount();
}

/**
 * Return the number of active memory pools associated to the receiver.
 * @return count of number of memory pools 
 */
uintptr_t
MM_MemorySubSpaceGeneric::getActiveMemoryPoolCount()
{
	return _memoryPool->getActiveMemoryPoolCount();
}

/**
 * Return the memory pool associated with a given storage location 
 * @param Address of storage location 
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemorySubSpaceGeneric::getMemoryPool(void* addr)
{
	return _memoryPool->getMemoryPool(addr);
}

/**
 * Return the memory pool associated with a given allocation size
 * @param Size of allocation request
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemorySubSpaceGeneric::getMemoryPool(uintptr_t size)
{
	return _memoryPool->getMemoryPool(size);
}

/**
 * Return the memory pool associated with a specified range of storage locations. 
 * 
 * @param addrBase Low address in specified range  
 * @param addrTop High address in  specified range 
 * @param highAddr If range spans end of memory pool set to address of first byte
 * which does not belong in returned pool.
 * @return MM_MemoryPool for storage location addrBase
 */
MM_MemoryPool*
MM_MemorySubSpaceGeneric::getMemoryPool(MM_EnvironmentBase* env, void* addrBase, void* addrTop, void*& highAddr)
{
	return _memoryPool->getMemoryPool(env, addrBase, addrTop, highAddr);
}


/* ***************************************
 * Allocation
 * ***************************************
 */

/**
 * @copydoc MM_MemorySubSpace::getActualFreeMemorySize()
 */
uintptr_t
MM_MemorySubSpaceGeneric::getActualFreeMemorySize()
{
	if (isActive()) {
		return _memoryPool->getActualFreeMemorySize();
	} else {
		return 0;
	}
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateFreeMemorySize()
 */
uintptr_t
MM_MemorySubSpaceGeneric::getApproximateFreeMemorySize()
{
	if (isActive()) {
		return _memoryPool->getApproximateFreeMemorySize();
	} else {
		return 0;
	}
}

uintptr_t
MM_MemorySubSpaceGeneric::getActiveMemorySize()
{
	return getCurrentSize();
}

uintptr_t
MM_MemorySubSpaceGeneric::getActiveMemorySize(uintptr_t includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType) {
		return getCurrentSize();
	} else {
		return 0;
	}
}

uintptr_t
MM_MemorySubSpaceGeneric::getActiveLOAMemorySize(uintptr_t includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType) {
		return _memoryPool->getCurrentLOASize();
	} else {
		return 0;
	}
}

/**
 * @copydoc MM_MemorySubSpace::getActualActiveFreeMemorySize()
 */
uintptr_t
MM_MemorySubSpaceGeneric::getActualActiveFreeMemorySize()
{
	return _memoryPool->getActualFreeMemorySize();
}

/**
 * @copydoc MM_MemorySubSpace::getActualActiveFreeMemorySize(uintptr_t)
 */
uintptr_t
MM_MemorySubSpaceGeneric::getActualActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType) {
		return _memoryPool->getActualFreeMemorySize();
	} else {
		return 0;
	}
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeMemorySize()
 */
uintptr_t
MM_MemorySubSpaceGeneric::getApproximateActiveFreeMemorySize()
{
	return _memoryPool->getApproximateFreeMemorySize();
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeMemorySize(uintptr_t)
 */
uintptr_t
MM_MemorySubSpaceGeneric::getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType) {
		return _memoryPool->getApproximateFreeMemorySize();
	} else {
		return 0;
	}
}


/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeLOAMemorySize()
 */
uintptr_t
MM_MemorySubSpaceGeneric::getApproximateActiveFreeLOAMemorySize()
{
	return _memoryPool->getApproximateFreeLOAMemorySize();
}

/**
 * @copydoc MM_MemorySubSpace::getApproximateActiveFreeLOAMemorySize(uintptr_t)
 */
uintptr_t
MM_MemorySubSpaceGeneric::getApproximateActiveFreeLOAMemorySize(uintptr_t includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType) {
		return _memoryPool->getApproximateFreeLOAMemorySize();
	} else {
		return 0;
	}
}

void
MM_MemorySubSpaceGeneric::mergeHeapStats(MM_HeapStats* heapStats)
{
	_memoryPool->mergeHeapStats(heapStats, isActive());
}

void
MM_MemorySubSpaceGeneric::mergeHeapStats(MM_HeapStats* heapStats, uintptr_t includeMemoryType)
{
	if (getTypeFlags() & includeMemoryType) {
		_memoryPool->mergeHeapStats(heapStats, isActive());
	}
}

void
MM_MemorySubSpaceGeneric::resetHeapStatistics(bool globalCollect)
{
	if ((getTypeFlags() & MEMORY_TYPE_OLD) && !globalCollect) {
		_memoryPool->resetHeapStatistics(false);
	} else {
		_memoryPool->resetHeapStatistics(true);
	}
}

/**
 * Return the allocation failure stats for this subSpace.
 */
MM_AllocationFailureStats*
MM_MemorySubSpaceGeneric::getAllocationFailureStats()
{
	return _parent->getAllocationFailureStats();
}

/****************************************
 * Allocation
 ****************************************
 */

void*
MM_MemorySubSpaceGeneric::allocateObject(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure)
{
	Trc_MM_MSSGeneric_allocate_entry(env->getLanguageVMThread(), "Object", allocDescription->getBytesRequested(), this, getName(), baseSubSpace, previousSubSpace, (uintptr_t)_allocateAtSafePointOnly, (uintptr_t)shouldCollectOnFailure, (uintptr_t)_isAllocatable);

	void *result = NULL;

	/* Typically, JIT stack frame is not built (shouldCollectOnFailure is false) and concurrent mark is not running or not complete (_allocateAtSafePointOnly is false),
	 * so we most often proceed with allocate.
	 * If we are notified that concurrent mark is complete ( _allocateAtSafePointOnly is true, and typically the stack frame is not built yet,
	 * we should not proceed with allocate, but return NULL and force JIT to call us again with the frame built
	 * Next time when the frame is built (shouldCollectOnFailure is true, while _allocateAtSafePointOnly is still true) we proceed with allocate (otherwise risk OOM),
	 * but on the way out of this allocate, when we pay tax, concurrent mark will trigger the final phase. */
	if (!_allocateAtSafePointOnly || shouldCollectOnFailure) {
		if (_isAllocatable) {
			result = _memoryPool->allocateObject(env, allocDescription);
		}

		if(NULL != result) {
			/* Allocate succeeded */
			allocDescription->setMemorySubSpace(this);
			allocDescription->setObjectFlags(getObjectFlags());
		} else {
			/* Allocate failed - try the parent */
			if (shouldCollectOnFailure) {
				Trc_MM_MSSGeneric_allocate(env->getLanguageVMThread(), "Object", allocDescription->getBytesRequested(), 1, this, _parent);
				result = _parent->allocationRequestFailed(env, allocDescription, ALLOCATION_TYPE_OBJECT, NULL, this, this);
			} else {
				Trc_MM_MSSGeneric_allocate(env->getLanguageVMThread(), "Object", allocDescription->getBytesRequested(), 2, this, _parent);
				result = _parent->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
			}
		}
	}

	Trc_MM_MSSGeneric_allocate_exit(env->getLanguageVMThread(), "Object", allocDescription->getBytesRequested(), this, result);

	return result;
}

/**
 * Allocate the arraylet spine in immortal or scoped memory.
 */
void*
MM_MemorySubSpaceGeneric::allocateArrayletLeaf(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure)
{
	void *result = NULL;

	if (!_allocateAtSafePointOnly || shouldCollectOnFailure) {
		if (_isAllocatable) {
			result = _memoryPool->allocateArrayletLeaf(env, allocDescription);
		}

		if(NULL == result) {
			/* Allocate failed - try the parent */
			if (shouldCollectOnFailure) {
				result = _parent->allocationRequestFailed(env, allocDescription, ALLOCATION_TYPE_LEAF, NULL, this, this);
			} else {
				result = _parent->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
			}
		}
	}

	return result;
}

void*
MM_MemorySubSpaceGeneric::allocationRequestFailed(MM_EnvironmentBase* env, MM_AllocateDescription* allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace)
{
	void* result = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, this);

	if ((ALLOCATION_TYPE_OBJECT == allocationType) && (NULL != result)) {
		/* Allocate succeeded */
		allocateDescription->setMemorySubSpace(this);
		allocateDescription->setObjectFlags(getObjectFlags());
	}

	return result;
}

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
void*
MM_MemorySubSpaceGeneric::allocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure)
{
	Trc_MM_MSSGeneric_allocate_entry(env->getLanguageVMThread(), "TLH", allocDescription->getBytesRequested(), this, getName(), baseSubSpace, previousSubSpace, (uintptr_t)_allocateAtSafePointOnly, (uintptr_t)shouldCollectOnFailure, (uintptr_t)_isAllocatable);

	void *result = NULL;

	/* Typically, JIT stack frame is not built (shouldCollectOnFailure is false) and concurrent mark is not running or not complete (_allocateAtSafePointOnly is false),
	 * so we most often proceed with allocate.
	 * If we are notified that concurrent mark is complete ( _allocateAtSafePointOnly is true), and typically the stack frame is not built yet,
	 * we should not proceed with allocate, but return NULL and force JIT to call us again with the frame built.
	 * Next time when the frame is built (shouldCollectOnFailure is true, while _allocateAtSafePointOnly is still true) we proceed with allocate (otherwise risk OOM),
	 * but on the way out of this allocate, when we pay tax, concurrent mark will trigger the final phase. */
	if (!_allocateAtSafePointOnly || shouldCollectOnFailure) {
		if (_isAllocatable) {
			result = objectAllocationInterface->allocateTLH(env, allocDescription, this, _memoryPool);
		}

		if (NULL == result) {
			if (shouldCollectOnFailure) {
				Trc_MM_MSSGeneric_allocate3(env->getLanguageVMThread(), "TLH", allocDescription->getBytesRequested(), this, _parent, (uintptr_t)allocDescription->shouldCollectAndClimb());
				if (allocDescription->shouldCollectAndClimb()) {
					result = _parent->allocationRequestFailed(env, allocDescription, ALLOCATION_TYPE_TLH, objectAllocationInterface, this, this);
				}
			} else {
				Trc_MM_MSSGeneric_allocate(env->getLanguageVMThread(), "TLH", allocDescription->getBytesRequested(), 2, this, _parent);
				result = _parent->allocateTLH(env, allocDescription, objectAllocationInterface, baseSubSpace, this, false);
			}
		}
	}

	Trc_MM_MSSGeneric_allocate_exit(env->getLanguageVMThread(), "TLH", allocDescription->getBytesRequested(), this, result);

	return result;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

/****************************************
 * Internal Allocation
 ****************************************
 */
void*
MM_MemorySubSpaceGeneric::collectorAllocate(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription)
{
	void* result;

	result = _memoryPool->collectorAllocate(env, allocDescription, true);
	if (NULL == result) {
		_memoryPool->lock(env);
		result = _memoryPool->collectorAllocate(env, allocDescription, false);
		if ((NULL == result) && allocDescription->isCollectorAllocateExpandOnFailure()) {
			if (0 != collectorExpand(env, requestCollector, allocDescription)) {
				allocDescription->setCollectorAllocateSatisfyAnywhere(true);
				result = _memoryPool->collectorAllocate(env, allocDescription, false);
			}
		}
		_memoryPool->unlock(env);
	}

	return result;
}

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
void*
MM_MemorySubSpaceGeneric::collectorAllocateTLH(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription,
											   uintptr_t maximumBytesRequired, void*& addrBase, void*& addrTop)
{
	void* result = NULL;

	result = _memoryPool->collectorAllocateTLH(env, allocDescription, maximumBytesRequired, addrBase, addrTop, true);
	if (NULL == result) {
		_memoryPool->lock(env);
		result = _memoryPool->collectorAllocateTLH(env, allocDescription, maximumBytesRequired, addrBase, addrTop, false);
		if ((NULL == result) && allocDescription->isCollectorAllocateExpandOnFailure()) {
			if (0 != collectorExpand(env, requestCollector, allocDescription)) {
				allocDescription->setCollectorAllocateSatisfyAnywhere(true);
				result = _memoryPool->collectorAllocateTLH(env, allocDescription, maximumBytesRequired, addrBase, addrTop, false);
			}
		}
		_memoryPool->unlock(env);
	}

	return result;
}

#endif /* OMR_GC_THREAD_LOCAL_HEAP */

void
MM_MemorySubSpaceGeneric::abandonHeapChunk(void* addrBase, void* addrTop)
{
	/* OMRTODO turn tarokEnableExpensiveAssertions into a generic (non tarok specific option) */
	if (_extensions->tarokEnableExpensiveAssertions) {
		MM_HeapRegionDescriptor* region = NULL;
		GC_MemorySubSpaceRegionIterator regionIterator(this);
		while (NULL != (region = regionIterator.nextRegion())) {
			if ((region->getLowAddress() <= addrBase) && (addrTop <= region->getHighAddress())) {
				break;
			}
		}

		Assert_MM_true(NULL != region);
	}

	_memoryPool->abandonHeapChunk(addrBase, addrTop);
}

/**
 * Expand the heap during a collection by the amount specified.
 * Generational style collectors, which move objects from one memory subspace to another
 * during collection, may require that the destination area expand during the collection 
 * cycle.  An example of this would be the tenure area in a 2-generational system, if the
 * tenure area memory has been exhausted.
 * @note This call is not protected by any locking mechanism.
 */
uintptr_t
MM_MemorySubSpaceGeneric::collectorExpand(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription)
{
	return _parent->collectorExpand(env, requestCollector, allocDescription);
}

/****************************************
 *	Sub Space Categorization
 ****************************************
 */
MM_MemorySubSpace*
MM_MemorySubSpaceGeneric::getDefaultMemorySubSpace()
{
	return _parent->getDefaultMemorySubSpace();
}

MM_MemorySubSpace*
MM_MemorySubSpaceGeneric::getTenureMemorySubSpace()
{
	return _parent->getTenureMemorySubSpace();
}

bool
MM_MemorySubSpaceGeneric::isActive()
{
	if (_parent) {
		return _parent->isChildActive(this);
	}
	return true;
}

/**
 * Ask memory pools if a complete rebuild of freelist is required
 */
bool
MM_MemorySubSpaceGeneric::completeFreelistRebuildRequired(MM_EnvironmentBase* env)
{
	return _memoryPool->completeFreelistRebuildRequired(env);
}

/****************************************
 * Free list building
 ****************************************
 */

void
MM_MemorySubSpaceGeneric::reset()
{
	/* TODO: Call the superclass reset() here as well (for children)? */
	_memoryPool->reset();
}

/**
 * As opposed to reset, which will empty out, this will fill out as if everything is free
 */
void
MM_MemorySubSpaceGeneric::rebuildFreeList(MM_EnvironmentBase* env)
{
	if (env->getExtensions()->isVLHGC()) {
		/* TODO we need a better way of defining rebuildFreeListInRegion.
		 * The previousFreeEntry parameter does not apply to Tarok world, as
		 * we don't want to link the sub memory pool together as one free list.
		 * Will be revisited in the next step for Design 1945
		 */
		_memoryPool->rebuildFreeListInRegion(env, NULL, NULL);
	} else {
		MM_HeapRegionDescriptor* region = NULL;
		lockRegionList();

		GC_MemorySubSpaceRegionIterator regionIterator(this);
		MM_HeapLinkedFreeHeader* freeListEntry = NULL;
		while (NULL != (region = regionIterator.nextRegion())) {
			freeListEntry = _memoryPool->rebuildFreeListInRegion(env, region, freeListEntry);
		}

		unlockRegionList();
	}
}

/**
 * Initialization
 */
MM_MemorySubSpaceGeneric*
MM_MemorySubSpaceGeneric::newInstance(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool, MM_RegionPool* regionPool, bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize, uintptr_t memoryType, uint32_t objectFlags)
{
	MM_MemorySubSpaceGeneric* memorySubSpace;

	memorySubSpace = (MM_MemorySubSpaceGeneric*)env->getForge()->allocate(sizeof(MM_MemorySubSpaceGeneric), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != memorySubSpace) {
		new (memorySubSpace) MM_MemorySubSpaceGeneric(env, memoryPool, regionPool, usesGlobalCollector, minimumSize, initialSize, maximumSize, memoryType, objectFlags);
		if (!memorySubSpace->initialize(env)) {
			memorySubSpace->kill(env);
			memorySubSpace = NULL;
		}
	} else {
		/* the MSS has responsibility for freeing the memoryPool and 
		 * regionPool. Since we couldn't create one, we must free them now. 
		 */
		if (NULL != memoryPool) {
			memoryPool->kill(env);
		}
		if (NULL != regionPool) {
			regionPool->kill(env);
		}
	}
	return memorySubSpace;
}

bool
MM_MemorySubSpaceGeneric::initialize(MM_EnvironmentBase* env)
{
	if (!MM_MemorySubSpace::initialize(env)) {
		return false;
	}

	_memoryPool->setSubSpace(this);

	return true;
}

void
MM_MemorySubSpaceGeneric::tearDown(MM_EnvironmentBase* env)
{
	/* Reset the fields in extensions */
	MM_GCExtensionsBase* extensions = env->getExtensions();
	extensions->heapBaseForBarrierRange0 = 0;
	extensions->heapSizeForBarrierRange0 = 0;
	extensions->setTenureAddressRange(extensions->heapBaseForBarrierRange0, extensions->heapSizeForBarrierRange0);

	if (NULL != _memoryPool) {
		_memoryPool->kill(env);
		_memoryPool = NULL;
	}

	if (NULL != _regionPool) {
		_regionPool->kill(env);
		_regionPool = NULL;
	}

	MM_MemorySubSpace::tearDown(env);
}

/**
 * Memory described by the range has added to the heap and been made available to the subspace as free memory.
 */
bool
MM_MemorySubSpaceGeneric::expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, MM_HeapRegionDescriptor* region, bool canCoalesce)
{
	uintptr_t size = region->getSize();
	void* lowAddress = region->getLowAddress();
	void* highAddress = region->getHighAddress();

	/* Inform the sub space hierarchy of the size change */
	bool result = heapAddRange(env, this, size, lowAddress, highAddress);

	if (result) {
		/* Feed the range to the memory pool */
		_memoryPool->expandWithRange(env, size, lowAddress, highAddress, canCoalesce);
	}
	return result;
}

/**
 * Memory described by the range has added to the heap and been made available to the subspace as free memory.
 * @Note this code only exists for Phase 2 Gencon collectors.  No new code should use this API.
 */
bool
MM_MemorySubSpaceGeneric::expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress, bool canCoalesce)
{
	/* Inform the sub space hierarchy of the size change */
	bool result = heapAddRange(env, this, size, lowAddress, highAddress);

	if (result) {
		addExistingMemory(env, subArena, size, lowAddress, highAddress, canCoalesce);
	}
	return result;
}

/**
 * Memory described by the range which was already part of the heap has been made available to the subspace
 * as free memory.
 * @note Size information (current) is not updated.
 * @warn This routine is fairly hacky - is there a better way?
 */
void
MM_MemorySubSpaceGeneric::addExistingMemory(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress, bool canCoalesce)
{
	/* Feed the range to the memory pool */
	_memoryPool->expandWithRange(env, size, lowAddress, highAddress, canCoalesce);

	if (MEMORY_TYPE_OLD == (getTypeFlags() & MEMORY_TYPE_OLD)) {
		addTenureRange(env, size, lowAddress, highAddress);
	}
}

/**
 * Memory described by the range which was already part of the heap is being removed from the current memory space's
 * ownership.  Adjust accordingly.
 * @note Size information (current) is not updated.
 * @warn This routine is fairly hacky - is there a better way?
 */
void*
MM_MemorySubSpaceGeneric::removeExistingMemory(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress)
{
	if (MEMORY_TYPE_OLD == (getTypeFlags() & MEMORY_TYPE_OLD)) {
		removeTenureRange(env, size, lowAddress, highAddress);
	}

	/* have the pool remove the free range */
	return _memoryPool->contractWithRange(env, size, lowAddress, highAddress);
}

#if defined(OMR_GC_MODRON_STANDARD)
/**
 * Find the free list entry whose end address matches the parameter.
 * 
 * @param addr Address to match against the high end of a free entry.
 * 
 * @return The leading address of the free entry whos top matches addr.
 */
void*
MM_MemorySubSpaceGeneric::findFreeEntryEndingAtAddr(MM_EnvironmentBase* env, void* addr)
{
	return _memoryPool->findFreeEntryEndingAtAddr(env, addr);
}

/**
 * Find the top of the free list entry whos start address matches the parameter.
 * 
 * @param addr Address to match against the low end of a free entry.
 * 
 * @return The trailing address of the free entry whos top matches addr.
 */
void*
MM_MemorySubSpaceGeneric::findFreeEntryTopStartingAtAddr(MM_EnvironmentBase* env, void* addr)
{
	return _memoryPool->findFreeEntryTopStartingAtAddr(env, addr);
}

/**
 * Find the address of the first entry on free list entry 
 * 
 * 
 * @return The address of head of free chain 
 */
void*
MM_MemorySubSpaceGeneric::getFirstFreeStartingAddr(MM_EnvironmentBase* env)
{
	return _memoryPool->getFirstFreeStartingAddr(env);
}

/**
 * Find the address of the next entry on free list entry 
 * 
 * 
 * @return The address of next free entry or NULL 
 */
void*
MM_MemorySubSpaceGeneric::getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree)
{
	return _memoryPool->getNextFreeStartingAddr(env, currentFree);
}

/**
 * Move a chunk of heap from one location to another within the receivers owned regions.
 * 
 * @param srcBase Start address to move.
 * @param srcTop End address to move.
 * @param dstBase Start of destination address to move into.
 * 
 */
void
MM_MemorySubSpaceGeneric::moveHeap(MM_EnvironmentBase* env, void* srcBase, void* srcTop, void* dstBase)
{
	/* Fix the free list for the new destination */
	_memoryPool->moveHeap(env, srcBase, srcTop, dstBase);
}
#endif /* OMR_GC_MODRON_STANDARD */

void
MM_MemorySubSpaceGeneric::addTenureRange(MM_EnvironmentBase* env, uintptr_t size, void* low, void* high)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	Assert_MM_true((uintptr_t)high - (uintptr_t)low == size);

	if (high == extensions->heapBaseForBarrierRange0) {
		/* expanding the base so base and size need to be updated */
		extensions->heapBaseForBarrierRange0 = low;
		extensions->heapSizeForBarrierRange0 += size;
	} else if (low == (void*)((uintptr_t)extensions->heapBaseForBarrierRange0 + extensions->heapSizeForBarrierRange0)) {
		/* expanding the top so only size needs to be updated */
		extensions->heapSizeForBarrierRange0 += size;
	} else {
		/* initial inflate of the heap so both base and size need to be updated */
		Assert_MM_true((NULL == extensions->heapBaseForBarrierRange0) && (0 == extensions->heapSizeForBarrierRange0));
		extensions->heapBaseForBarrierRange0 = low;
		extensions->heapSizeForBarrierRange0 = size;
	}

	extensions->setTenureAddressRange(extensions->heapBaseForBarrierRange0, extensions->heapSizeForBarrierRange0);
}

void
MM_MemorySubSpaceGeneric::removeTenureRange(MM_EnvironmentBase* env, uintptr_t size, void* low, void* high)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	Assert_MM_true((uintptr_t)high - (uintptr_t)low == size);

	if (low == extensions->heapBaseForBarrierRange0) {
		/* contracting from low */
		extensions->heapBaseForBarrierRange0 = high;
		extensions->heapSizeForBarrierRange0 -= size;
	} else if (high == (void*)((uintptr_t)extensions->heapBaseForBarrierRange0 + extensions->heapSizeForBarrierRange0)) {
		/* contracting from high */
		extensions->heapSizeForBarrierRange0 -= size;
	} else {
		/* bad range */
		Assert_MM_unreachable();
	}

	extensions->setTenureAddressRange(extensions->heapBaseForBarrierRange0, extensions->heapSizeForBarrierRange0);
}

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
uintptr_t
MM_MemorySubSpaceGeneric::releaseFreeMemoryPages(MM_EnvironmentBase* env)
{
	return _memoryPool->releaseFreeMemoryPages(env);
}
#endif
