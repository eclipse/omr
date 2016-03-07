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

#include "HeapVirtualMemory.hpp"

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"
#include "Collector.hpp"
#include "HeapRegionManager.hpp"
#include "Math.hpp"
#include "MemoryManager.hpp"
#include "MemorySubSpace.hpp"
#include "PhysicalArena.hpp"

#define HIGH_ADDRESS UDATA_MAX
#define OVERFLOW_ROUNDING ((uintptr_t)16 * 1024)

/**
 * @param heapAlignment size in bytes the heap should be aligned to
 * @param size the <i>desired</i> heap size
 * @param regionManager the regionManager for the heap
 * 
 * @note The actual heap size might be smaller than the requested size, in order to satisfy
 * alignment requirements. Use getMaximumSize() to get the actual allocation size.
 */
MM_HeapVirtualMemory*
MM_HeapVirtualMemory::newInstance(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t size, MM_HeapRegionManager* regionManager)
{
	MM_HeapVirtualMemory* heap;

	heap = (MM_HeapVirtualMemory*)env->getForge()->allocate(sizeof(MM_HeapVirtualMemory), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (heap) {
		new (heap) MM_HeapVirtualMemory(env, heapAlignment, size, regionManager);
		if (!heap->initialize(env, size)) {
			heap->kill(env);
			heap = NULL;
		}
	}
	return heap;
}

bool
MM_HeapVirtualMemory::initialize(MM_EnvironmentBase* env, uintptr_t size)
{
	/* call the superclass to inialize before we do any work */
	if (!MM_Heap::initialize(env)) {
		return false;
	}

	MM_GCExtensionsBase* extensions = env->getExtensions();
	uintptr_t padding = extensions->heapTailPadding;

	uintptr_t effectiveHeapAlignment = _heapAlignment;
	/* we need to ensure that we allocate the heap with region alignment since the region table requires that */
	MM_HeapRegionManager* manager = getHeapRegionManager();
	effectiveHeapAlignment = MM_Math::roundToCeiling(manager->getRegionSize(), effectiveHeapAlignment);

	MM_MemoryManager* memoryManager = extensions->memoryManager;
	bool created = false;
	bool forcedOverflowProtection = false;

#if defined(J9ZOS390) || defined(S390)
	/* 390 JIT codegen requires a uintptr_t-worth of padding at end of heap -- see CMVC 95853 */
	if (padding < sizeof(uintptr_t)) {
		padding = sizeof(uintptr_t);
	}
#endif /* defined(J9ZOS390) || defined(S390) */

	/* Under -Xaggressive ensure a full page of padding -- see JAZZ103 45254 */
	if (extensions->padToPageSize) {
#if (defined(AIXPPC) && !defined(PPC64))
		/*
		 * An attempt to allocate heap with top at 0xffffffff
		 * In this case extra padding is not required because of overflow protection padding can be used instead
		 */
		uintptr_t effectiveSize = MM_Math::roundToCeiling(manager->getRegionSize(), size);
		void *preferredHeapBase = (void *)((uintptr_t)0 - effectiveSize);

		created = memoryManager->createVirtualMemoryForHeap(env, &_vmemHandle, effectiveHeapAlignment, size, padding, preferredHeapBase, (void *)(extensions->heapCeiling));
		if (created) {
			/* overflow protection must be there to play role of padding even top is not so close to the end of the memory */
			forcedOverflowProtection = true;
		} else
#endif /* (defined(AIXPPC) && !defined(PPC64)) */
		{
#if defined(OMR_GC_COMPRESSED_POINTERS) && !(defined(J9ZOS390) || defined(S390))
			/*
			 * For all compressed references platforms (except 390)
			 * ignore extra full page padding if page size is too large (hard coded here for 1G or larger)
			 */
#define ONE_GB ((uintptr_t)1 * 1024 * 1024 * 1024)
			if (extensions->requestedPageSize < ONE_GB)
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && !(defined(J9ZOS390) || defined(S390)) */
			{
				if (padding < extensions->requestedPageSize) {
					padding = extensions->requestedPageSize;
				}
			}
		}
	}

	if (!created && !memoryManager->createVirtualMemoryForHeap(env, &_vmemHandle, effectiveHeapAlignment, size, padding, (void*)(extensions->preferredHeapBase), (void*)(extensions->heapCeiling))) {
		return false;
	}

	/* Check we haven't overflowed the address range */
	if (forcedOverflowProtection || (HIGH_ADDRESS - ((uintptr_t)memoryManager->getHeapTop(&_vmemHandle)) < (OVERFLOW_ROUNDING)) || extensions->fvtest_alwaysApplyOverflowRounding) {
		/* Address range overflow */
		memoryManager->roundDownTop(&_vmemHandle, OVERFLOW_ROUNDING);
	}
	extensions->overflowSafeAllocSize = ((HIGH_ADDRESS - (uintptr_t)(memoryManager->getHeapTop(&_vmemHandle))) + 1);

	/* The memory returned might be less than we asked for -- get the actual size */
	_maximumMemorySize = memoryManager->getMaximumSize(&_vmemHandle);

	return true;
}

void
MM_HeapVirtualMemory::tearDown(MM_EnvironmentBase* env)
{
	MM_MemoryManager* memoryManager = env->getExtensions()->memoryManager;
	MM_HeapRegionManager* manager = getHeapRegionManager();

	if (NULL != manager) {
		manager->destroyRegionTable(env);
	}

	memoryManager->destroyVirtualMemory(env, &_vmemHandle);

	MM_Heap::tearDown(env);
}

void
MM_HeapVirtualMemory::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Answer the lowest possible address for the heap that will ever be possible.
 * @return Lowest address possible for the heap.
 */
void*
MM_HeapVirtualMemory::getHeapBase()
{
	MM_MemoryManager* memoryManager = MM_GCExtensionsBase::getExtensions(_omrVM)->memoryManager;
	return memoryManager->getHeapBase(&_vmemHandle);
}

/**
 * Answer the highest possible address for the heap that will ever be possible.
 * @return Highest address possible for the heap.
 */
void*
MM_HeapVirtualMemory::getHeapTop()
{
	MM_MemoryManager* memoryManager = MM_GCExtensionsBase::getExtensions(_omrVM)->memoryManager;
	return memoryManager->getHeapTop(&_vmemHandle);
}

uintptr_t
MM_HeapVirtualMemory::getPageSize()
{
	MM_MemoryManager* memoryManager = MM_GCExtensionsBase::getExtensions(_omrVM)->memoryManager;
	return memoryManager->getPageSize(&_vmemHandle);
}

uintptr_t
MM_HeapVirtualMemory::getPageFlags()
{
	MM_MemoryManager* memoryManager = MM_GCExtensionsBase::getExtensions(_omrVM)->memoryManager;
	return memoryManager->getPageFlags(&_vmemHandle);
}

/**
 * Answer the largest size the heap will ever consume.
 * The value returned represents the difference between the lowest and highest possible address range
 * the heap can ever occupy.  This value includes any memory that may never be used by the heap (e.g.,
 * in a segmented heap scenario).
 * @return Maximum size that the heap will ever span.
 */
uintptr_t
MM_HeapVirtualMemory::getMaximumPhysicalRange()
{
	MM_MemoryManager* memoryManager = MM_GCExtensionsBase::getExtensions(_omrVM)->memoryManager;
	return ((uintptr_t)memoryManager->getMaximumSize(&_vmemHandle));
}

/**
 * Remove a physical arena from the receiver.
 */
void
MM_HeapVirtualMemory::detachArena(MM_EnvironmentBase* env, MM_PhysicalArena* arena)
{
	MM_PhysicalArena* currentArena = arena;
	MM_PhysicalArena* previous = currentArena->getPreviousArena();
	MM_PhysicalArena* next = currentArena->getNextArena();

	if (previous) {
		previous->setNextArena(next);
	} else {
		_physicalArena = next;
	}

	if (next) {
		next->setPreviousArena(previous);
	}

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
MM_HeapVirtualMemory::attachArena(MM_EnvironmentBase* env, MM_PhysicalArena* arena, uintptr_t size)
{
	/* Sanity check of the size */
	if (getMaximumMemorySize() < size) {
		return false;
	}

	MM_GCExtensionsBase* extensions = env->getExtensions();
	MM_MemoryManager* memoryManager = extensions->memoryManager;

	/* Find the insertion point for the currentArena */
	void* candidateBase = memoryManager->getHeapBase(&_vmemHandle);
	MM_PhysicalArena* insertionHead = NULL;
	MM_PhysicalArena* insertionTail = _physicalArena;
	MM_PhysicalArena* currentArena = arena;

	while (insertionTail) {
		if ((((uintptr_t)insertionTail->getLowAddress()) - ((uintptr_t)candidateBase)) >= size) {
			break;
		}

		candidateBase = insertionTail->getHighAddress();

		insertionHead = insertionTail;
		insertionTail = insertionTail->getNextArena();
	}

	/* If we have reached the end of the currentArena list, check if there is room between the candidateBase
	 * and the end of virtual memory */
	if (!insertionTail) {
		if ((memoryManager->calculateOffsetToHeapTop(&_vmemHandle, candidateBase)) < size) {
			return false;
		}
	}

	/* Connect the physical currentArena into the list at the appropriate point */
	currentArena->setPreviousArena(insertionHead);
	currentArena->setNextArena(insertionTail);

	if (insertionTail) {
		insertionTail->setPreviousArena(currentArena);
	}

	if (insertionHead) {
		insertionHead->setNextArena(currentArena);
	} else {
		_physicalArena = currentArena;
	}

	currentArena->setLowAddress(candidateBase);
	currentArena->setHighAddress((void*)(((uint8_t*)candidateBase) + size));

	/* Set the arena state to being attached */
	arena->setAttached(true);

	return true;
}

/**
 * Commit the address range into physical memory.
 * @return true if successful, false otherwise.
 * @note This is a bit of a strange function to have as public API.  Should it be removed?
 */
bool
MM_HeapVirtualMemory::commitMemory(void* address, uintptr_t size)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	MM_MemoryManager* memoryManager = extensions->memoryManager;
	return memoryManager->commitMemory(&_vmemHandle, address, size);
}

/**
 * Decommit the address range from physical memory.
 * @return true if successful, false otherwise.
 * @note This is a bit of a strange function to have as public API.  Should it be removed?
 */
bool
MM_HeapVirtualMemory::decommitMemory(void* address, uintptr_t size, void* lowValidAddress, void* highValidAddress)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	MM_MemoryManager* memoryManager = extensions->memoryManager;
	return memoryManager->decommitMemory(&_vmemHandle, address, size, lowValidAddress, highValidAddress);
}

/**
 * Calculate the offset of an address from the base of the heap.
 * @param The address which require the offset for.
 * @return The offset from heap base.
 */
uintptr_t
MM_HeapVirtualMemory::calculateOffsetFromHeapBase(void* address)
{
	MM_GCExtensionsBase* extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	MM_MemoryManager* memoryManager = extensions->memoryManager;
	return memoryManager->calculateOffsetFromHeapBase(&_vmemHandle, address);
}

/**
 * The heap has added a range of memory associated to the receiver or one of its children.
 * @note The low address is inclusive, the high address exclusive.
 */

bool
MM_HeapVirtualMemory::heapAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress)
{
	MM_Collector* globalCollector = env->getExtensions()->getGlobalCollector();

	bool result = true;
	if (NULL != globalCollector) {
		result = globalCollector->heapAddRange(env, subspace, size, lowAddress, highAddress);
	}
	
	env->getExtensions()->identityHashDataAddRange(env, subspace, size, lowAddress, highAddress);

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
MM_HeapVirtualMemory::heapRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress, void* lowValidAddress, void* highValidAddress)
{
	MM_Collector* globalCollector = env->getExtensions()->getGlobalCollector();

	bool result = true;
	if (NULL != globalCollector) {
		result = globalCollector->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}

	env->getExtensions()->identityHashDataRemoveRange(env, subspace, size, lowAddress, highAddress);

	return result;
}

bool
MM_HeapVirtualMemory::initializeHeapRegionManager(MM_EnvironmentBase* env, MM_HeapRegionManager* manager)
{
	bool result = false;

	/* since this kind of heap is backed by contiguous memory, tell the heap region manager (which was just
	 * initialized by super) that we want to enable this range of regions for later use.
	 */
	MM_MemoryManager* memoryManager = MM_GCExtensionsBase::getExtensions(_omrVM)->memoryManager;
	void* heapBase = memoryManager->getHeapBase(&_vmemHandle);
	void* heapTop = memoryManager->getHeapTop(&_vmemHandle);

	if (manager->setContiguousHeapRange(env, heapBase, heapTop)) {
		result = manager->enableRegionsInTable(env, &_vmemHandle);
	}

	return result;
}
