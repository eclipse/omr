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

#include "HeapSplit.hpp"

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "HeapRegionManager.hpp"
#include "HeapVirtualMemory.hpp"
#include "ModronAssertions.h"
#include "PhysicalArena.hpp"
#include "PhysicalArenaVirtualMemory.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

class MM_MemorySubSpace;

/**
 * @param heapAlignment size in bytes the heap should be aligned to
 * @param lowExtentSize the <i>desired</i> size of the low extent of the heap
 * @param highExtentSize the <i>desired</i> size of the high extent of the heap
 * @param regionManager the global region manager
 * 
 * @note The actual heap size might be smaller than the requested size, in order to satisfy
 * alignment requirements. Use getMaximumSize() to get the actual allocation size.
 */
MM_HeapSplit *
MM_HeapSplit::newInstance(MM_EnvironmentBase *env, uintptr_t heapAlignment, uintptr_t lowExtentSize, uintptr_t highExtentSize, MM_HeapRegionManager *regionManager)
{
	MM_HeapSplit *heap = (MM_HeapSplit *)env->getForge()->allocate(sizeof(MM_HeapSplit), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	
	if (NULL != heap) {
		new(heap) MM_HeapSplit(env, lowExtentSize, highExtentSize, regionManager);
		if (!heap->initialize(env, heapAlignment, lowExtentSize, highExtentSize, regionManager)) {
			heap->kill(env);
			heap = NULL;
		}
	}
	return heap;
}


bool
MM_HeapSplit::initialize(MM_EnvironmentBase *env, uintptr_t heapAlignment, uintptr_t lowExtentSize, uintptr_t highExtentSize, MM_HeapRegionManager *regionManager)
{
	bool success = MM_Heap::initialize(env);
	
	if (success) {
		MM_GCExtensionsBase *extensions = env->getExtensions();

		/* only the high extent can require overflow rounding (this will fail in ways which aren't helpful for debugging on the low extent)
		 * so ensure that we only set that extension flag when initializing the high extent (of course, only do this if it was already set)
		 */
		bool shouldSetFVTestOverflowRounding = extensions->fvtest_alwaysApplyOverflowRounding;

		/* Attempt to allocate Tenure */
		/* This section is going to be allocated bottom-up */
		extensions->splitHeapSection = MM_GCExtensionsBase::HEAP_INITIALIZATION_SPLIT_HEAP_TENURE;
		/* temporary disable overflow rounding for Tenure if it is requested explicitly  */
		extensions->fvtest_alwaysApplyOverflowRounding = false;
		_lowExtent = MM_HeapVirtualMemory::newInstance(env, heapAlignment, lowExtentSize, regionManager);
		/* restore the overflow rounding flag */
		extensions->fvtest_alwaysApplyOverflowRounding = shouldSetFVTestOverflowRounding;

		/* Attempt to allocate Nursery */
		/* This section is going to be allocated top-down */
		extensions->splitHeapSection = MM_GCExtensionsBase::HEAP_INITIALIZATION_SPLIT_HEAP_NURSERY;
		_highExtent = MM_HeapVirtualMemory::newInstance(env, heapAlignment, highExtentSize, regionManager);

		/* Set neutral state again just to prevent wrong usage */
		extensions->splitHeapSection = MM_GCExtensionsBase::HEAP_INITIALIZATION_SPLIT_HEAP_UNKNOWN;

		/* successful initialization of split heaps is defined as success in initializing both heap extents and them being in the right order */
		success = (NULL != _lowExtent) && (NULL != _highExtent) && (_lowExtent->getHeapBase() < _highExtent->getHeapBase());

		if (!success) {
			/* we failed again just decide what kind of verbose message to use (don't forget to pass these through NLS) */
			if (NULL == _lowExtent) {
				extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_SPLIT_HEAP_OLD_SPACE;
			} else if (NULL == _highExtent) {
				extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_SPLIT_HEAP_NEW_SPACE;
			} else {
				extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_SPLIT_HEAP_GEOMETRY;
			}

			/* release memory back if necessary */
			if (NULL != _lowExtent) {
				_lowExtent->kill(env);
				_lowExtent = NULL;
			}

			if (NULL != _highExtent) {
				_highExtent->kill(env);
				_highExtent = NULL;
			}
		}
	}
	return success;
}


void
MM_HeapSplit::tearDown(MM_EnvironmentBase *env)
{
	MM_HeapRegionManager *manager = getHeapRegionManager();
	if (NULL != manager) {
		manager->destroyRegionTable(env);
	}

	if (NULL != _lowExtent) {
		_lowExtent->kill(env);
		_lowExtent = NULL;
	}
	if (NULL != _highExtent) {
		_highExtent->kill(env);
		_highExtent = NULL;
	}
	MM_Heap::tearDown(env);
}


void
MM_HeapSplit::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}


/**
 * Answer the lowest possible address for the heap that will ever be possible.
 * @return Lowest address possible for the heap.
 */
void *
MM_HeapSplit::getHeapBase()
{
	return _lowExtent->getHeapBase();
}


/**
 * Answer the highest possible address for the heap that will ever be possible.
 * @return Highest address possible for the heap.
 */
void *
MM_HeapSplit::getHeapTop()
{
	return _highExtent->getHeapTop();
}

uintptr_t
MM_HeapSplit::getPageSize()
{
	return (_lowExtent->getPageSize() < _highExtent->getPageSize()) ? _lowExtent->getPageSize() : _highExtent->getPageSize();
}

uintptr_t
MM_HeapSplit::getPageFlags()
{
	return (_lowExtent->getPageSize() < _highExtent->getPageSize()) ? _lowExtent->getPageFlags() : _highExtent->getPageFlags();
}

/**
 * Answer the largest size the heap will ever consume.
 * The value returned represents the difference between the lowest and highest possible address range
 * the heap can ever occupy.  This value includes any memory that may never be used by the heap (e.g.,
 * in a segmented heap scenario).
 * @return Maximum size that the heap will ever span.
 */
uintptr_t
MM_HeapSplit::getMaximumPhysicalRange()
{
	return (uintptr_t)getHeapTop() - (uintptr_t)getHeapBase();
}


/**
 * Remove a physical arena from the receiver.
 */
void
MM_HeapSplit::detachArena(MM_EnvironmentBase *env, MM_PhysicalArena *arena)
{
	/* Set the arena state to no longer being attached */
	arena->setAttached(false);
}


/**
 * Attach a physical arena of the specified size to the receiver.
 * This reserves the address space within the receiver for the arena, and connects the arena to the list
 * of those associated to the receiver (in address order).
 * 
 * @return true if the arena was attached successfully, false otherwise.
 * @note The memory reseved is not commited.
 */
bool
MM_HeapSplit::attachArena(MM_EnvironmentBase *env, MM_PhysicalArena *arena, uintptr_t size)
{
	/* Since we don't presently support any notion of dynamic resizing of the heap, use this assertion to make sure that we don't accidentally try to use that functionality */
	Assert_MM_true(size == (_lowExtent->getMaximumPhysicalRange() + _highExtent->getMaximumPhysicalRange()));
	((MM_PhysicalArenaVirtualMemory *)arena)->setLowAddress(getHeapBase());
	/* due to overflow rounding, we may ask for less than the entire physical region so ensure that we account for the gap when calculating the high address. */
	uintptr_t gapSize = ((uintptr_t)_highExtent->getHeapBase() - (uintptr_t)_lowExtent->getHeapTop());
	((MM_PhysicalArenaVirtualMemory *)arena)->setHighAddress((uint8_t *)getHeapBase() + gapSize + size);
	arena->setAttached(true);
	return true;
}


/**
 * Commit the address range into physical memory.
 * @return true if successful, false otherwise.
 * @note This is a bit of a strange function to have as public API.  Should it be removed?
 */
bool
MM_HeapSplit::commitMemory(void *address, uintptr_t size)
{
	bool success = false;
	
	/* these need to be a perfect fit so assert that the committed size is equal to the total size of the region at that address (in both cases) */
	if (_lowExtent->getHeapBase() == address) {
		Assert_MM_true(_lowExtent->getMaximumPhysicalRange() == size);
		success = _lowExtent->commitMemory(address, size);
	} else if (_highExtent->getHeapBase() == address) {
		Assert_MM_true(_highExtent->getMaximumPhysicalRange() == size);
		success = _highExtent->commitMemory(address, size);
	} else {
		/* This is neither range so fail */
		Assert_MM_true(false);
	}
	return success;
}


/**
 * Decommit the address range from physical memory.
 * @return true if successful, false otherwise.
 * @note This is a bit of a strange function to have as public API.  Should it be removed?
 */
bool
MM_HeapSplit::decommitMemory(void *address, uintptr_t size, void *lowValidAddress, void *highValidAddress)
{
	bool success = false;
	
	if (_lowExtent->getHeapBase() == address) {
		Assert_MM_true(_lowExtent->getMaximumPhysicalRange() == size);
		success = _lowExtent->decommitMemory(address, size, lowValidAddress, highValidAddress);
	} else if (_highExtent->getHeapBase() == address) {
		Assert_MM_true(_highExtent->getMaximumPhysicalRange() == size);
		success = _highExtent->decommitMemory(address, size, lowValidAddress, highValidAddress);
	} else {
		/* This is neither range so fail */
		Assert_MM_true(false);
	}
	return success;
}


/**
 * Calculate the offset of an address from the base of the heap.
 * @param The address which require the offset for.
 * @return The offset from heap base.
 */
uintptr_t
MM_HeapSplit::calculateOffsetFromHeapBase(void *address)
{
	return _lowExtent->calculateOffsetFromHeapBase(address);
}

bool
MM_HeapSplit::objectIsInGap(void *object)
{
	return (object > _lowExtent->getHeapTop()) && (object < _highExtent->getHeapBase());
}


bool
MM_HeapSplit::initializeHeapRegionManager(MM_EnvironmentBase *env, MM_HeapRegionManager *manager)
{
	if (!manager->setContiguousHeapRange(env, _lowExtent->getHeapBase(), _highExtent->getHeapTop())) {
		return false;
	}

	if (!manager->enableRegionsInTable(env, &_lowExtent->_vmemHandle)) {
		return false;
	}

	if (!manager->enableRegionsInTable(env, &_highExtent->_vmemHandle)) {
		return false;
	}

	return true;
}

#endif /* OMR_GC_MODRON_SCAVENGER */
