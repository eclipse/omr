/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include <string.h>

#include "omrcfg.h"

#include "TLHAllocationSupport.hpp"

#include "ModronAssertions.h"
#include "objectdescription.h"
#include "omrutil.h"

#include "AllocateDescription.hpp"
#include "AllocationContext.hpp"
#include "AllocationStats.hpp"
#include "CollectorLanguageInterface.hpp"
#include "EnvironmentBase.hpp"
#include "FrequentObjectsStats.hpp"
#include "GCExtensionsBase.hpp"
#include "Math.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"

#if defined(OMR_VALGRIND_MEMCHECK)
#include "MemcheckWrapper.hpp"
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
/**
 * Report clearing of a full allocation cache
 */
void
MM_TLHAllocationSupport::reportClearCache(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemorySubSpace *subspace = env->getMemorySpace()->getDefaultMemorySubSpace();

	TRIGGER_J9HOOK_MM_PRIVATE_CACHE_CLEARED(extensions->privateHookInterface, _omrVMThread, subspace, getBase(), getAlloc(), getTop());
}

/**
 * Report allocation of a new allocation cache
 */
void
MM_TLHAllocationSupport::reportRefreshCache(MM_EnvironmentBase *env)
{
	MM_MemorySubSpace *subspace = env->getMemorySpace()->getDefaultMemorySubSpace();

	TRIGGER_J9HOOK_MM_PRIVATE_CACHE_REFRESHED(env->getExtensions()->privateHookInterface, _omrVMThread, subspace, getBase(), getTop());
}

/**
 * Purge the TLH data from the receiver.
 * Remove any ownership of a heap area from the receivers TLH, and make sure the heap is left in a safe,
 * or walkable state.
 *
 * @note The calling environment may not be the receivers owning environment.
 */
void
MM_TLHAllocationSupport::clear(MM_EnvironmentBase *env)
{
	MM_MemoryPool *memoryPool = getMemoryPool();

	/* Any previous cache to clear  ? */
	if (NULL != memoryPool) {
		memoryPool->abandonTlhHeapChunk(getRealAlloc(), getTop());
		reportClearCache(env);
	}
	wipeTLH(env);
}

/**
 * Reconnect the cache to the owning environment.
 * The environment is either newly created or has had its properties changed such that the existing cache is
 * no longer valid.  An example of this is a change of memory space.  Perform the necessary flushing and
 * re-initialization of the cache details such that new allocations will occur in the correct fashion.
 *
 * @param shouldFlush determines whether the existing cache information should be flushed back to the heap.
 */
 void
 MM_TLHAllocationSupport::reconnect(MM_EnvironmentBase *env, bool shouldFlush)
{
	 MM_GCExtensionsBase *extensions = env->getExtensions();

	if(shouldFlush) {
		_abandonedList = NULL;
		_abandonedListSize = 0;
		clear(env);
	} else {
		/* Clear current information accumulated */
		setAllZeroes();
	}

	_tlh->refreshSize = extensions->tlhInitialSize;
}

/**
 * Restart the cache from its current start to an appropriate base state.
 * Reset the cache details back to a starting state that is appropriate for where it currently is.
 * In this case, the state reset takes into account the ending refresh size and sets an appropriate
 * new starting point.
 *
 * @note The previous cache contents are expected to have been flushed back to the heap.
 */
void
MM_TLHAllocationSupport::restart(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();
	uintptr_t refreshSize;

	/* Preserve the refresh size across the zeroing */
	refreshSize = _tlh->refreshSize;

	/* Clear current information accumulated */
	setAllZeroes();

	_tlh->refreshSize = MM_Math::roundToCeiling(extensions->tlhInitialSize, refreshSize / 2);
}

/**
 * Refresh the TLH.
 */
bool
MM_TLHAllocationSupport::refresh(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool shouldCollectOnFailure)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	/* Refresh the TLH only if the allocation request will fit in half the refresh size
	 * or in the TLH minimum size.
	 */
	uintptr_t sizeInBytesRequired = allocDescription->getContiguousBytes();
	uintptr_t tlhMinimumSize = extensions->tlhMinimumSize;
	uintptr_t tlhMaximumSize = extensions->tlhActiveMaximumSize;
	uintptr_t halfRefreshSize = getRefreshSize() >> 1;
	uintptr_t abandonSize = (tlhMinimumSize > halfRefreshSize ? tlhMinimumSize : halfRefreshSize);
	if (sizeInBytesRequired > abandonSize) {
		/* increase thread hungriness if we did not refresh */
		if (getRefreshSize() < tlhMaximumSize && sizeInBytesRequired < tlhMaximumSize) {
			setRefreshSize(getRefreshSize() + extensions->tlhIncrementSize);
		}
		return false;
	}

	MM_AllocationStats *stats = _objectAllocationInterface->getAllocationStats();

	stats->_tlhDiscardedBytes += getSize();

	/* Try to cache the current TLH */
	if (NULL != getRealAlloc() && getSize() >= tlhMinimumSize) {
		/* Cache the current TLH because it is bigger than the minimum size */
		MM_HeapLinkedFreeHeaderTLH* newCache = (MM_HeapLinkedFreeHeaderTLH*)getRealAlloc();

#if defined(OMR_VALGRIND_MEMCHECK)
		valgrindMakeMemUndefined((uintptr_t)newCache, sizeof(MM_HeapLinkedFreeHeaderTLH));			
#endif /* defined(OMR_VALGRIND_MEMCHECK) */
	    newCache->setSize(getSize());
		newCache->_memoryPool = getMemoryPool();
		newCache->_memorySubSpace = getMemorySubSpace();
		newCache->setNext(_abandonedList);
		_abandonedList = newCache;
		++_abandonedListSize;
		if (_abandonedListSize > stats->_tlhMaxAbandonedListSize) {
			stats->_tlhMaxAbandonedListSize = _abandonedListSize;
		}
		wipeTLH(env);
	} else {
		clear(env);
	}

	bool didRefresh = false;
	uintptr_t sizeCurrentTLHAlloc = 0;
	/* Try allocating a TLH */
	if ((NULL != _abandonedList) && (sizeInBytesRequired <= tlhMinimumSize)) {
		/* Try to get a cached TLH */
		setupTLH(env, (void *)_abandonedList, (void *)_abandonedList->afterEnd(),
				_abandonedList->_memorySubSpace, _abandonedList->_memoryPool);
		_abandonedList = (MM_HeapLinkedFreeHeaderTLH *)_abandonedList->getNext();
		--_abandonedListSize;

#if defined(OMR_GC_BATCH_CLEAR_TLH)
		if (_zeroTLH) {
			if (0 != extensions->batchClearTLH) {
				memset(getBase(), 0, sizeof(MM_HeapLinkedFreeHeaderTLH));
			}
		}
#endif /* OMR_GC_BATCH_CLEAR_TLH */

		allocDescription->setTLHAllocation(true);
		allocDescription->setNurseryAllocation(getMemorySubSpace()->getTypeFlags() == MEMORY_TYPE_NEW);
		allocDescription->setMemoryPool(getMemoryPool());

		stats->_tlhRefreshCountReused += 1;
		stats->_tlhAllocatedReused += getSize();
		stats->_tlhDiscardedBytes -= getSize();

		didRefresh = true;
	} else {
		/* Try allocating a fresh TLH */
		MM_AllocationContext *ac = env->getAllocationContext();
		MM_MemorySpace *memorySpace = _objectAllocationInterface->getOwningEnv()->getMemorySpace();

		if (extensions->enableAllocationSampling && (extensions->allocationSamplingInterval >= tlhMinimumSize)) {
			/* Get current TLH allocation size */
			sizeCurrentTLHAlloc = (uintptr_t)getAlloc() - (uintptr_t)getBase();
		}

		if (NULL != ac) {
			/* ensure that we are allowed to use the AI in this configuration in the Tarok case */
			/* allocation contexts currently aren't supported with generational schemes */
			Assert_MM_true(memorySpace->getTenureMemorySubSpace() == memorySpace->getDefaultMemorySubSpace());
			didRefresh = (NULL != ac->allocateTLH(env, allocDescription, _objectAllocationInterface, shouldCollectOnFailure));
		} else {
			MM_MemorySubSpace *subspace = memorySpace->getDefaultMemorySubSpace();
			didRefresh = (NULL != subspace->allocateTLH(env, allocDescription, _objectAllocationInterface, NULL, NULL, shouldCollectOnFailure));
		}

		if (didRefresh) {
#if defined(OMR_GC_BATCH_CLEAR_TLH)
			if (_zeroTLH) {
				if (0 != extensions->batchClearTLH) {
					void *base = getBase();
					void *top = getTop();
					OMRZeroMemory(base, (uintptr_t)top - (uintptr_t)base);
				}
			}
#endif /* defined(OMR_GC_BATCH_CLEAR_TLH) */

			/*
			 * THL was refreshed however it might be already flushed in GC
			 * Some special features (like Prepare Heap For Walk called by GC check)
			 * might request flush of all TLHs
			 * Flushed TLH would have Base=Top=Allocated=0, so getSize returns 0
			 * Do not change stats here if TLH is flushed already
			 */
			if (0 < getSize()) {
				stats->_tlhRefreshCountFresh += 1;
				stats->_tlhAllocatedFresh += getSize();
			}
		}
	}

	if (didRefresh) {
		/*
		 * THL was refreshed however it might be already flushed in GC
		 * Some special features (like Prepare Heap For Walk called by GC check)
		 * might request flush of all TLHs
		 * Flushed TLH would have Base=Top=Allocated=0, so getSize returns 0
		 * Do not change stats here if TLH is flushed already
		 */
		if (0 < getSize()) {
			reportRefreshCache(env);
			stats->_tlhRequestedBytes += getRefreshSize();
			/* TODO VMDESIGN 1322: adjust the amount consumed by the TLH refresh since a TLH refresh
			 * may not give you the size requested */
			/* Increase thread hungriness */
			/* TODO: TLH values (max/min/inc) should be per tlh, or somewhere else? */
			if (getRefreshSize() < tlhMaximumSize) {
				setRefreshSize(getRefreshSize() + extensions->tlhIncrementSize);
			}
		}
		
		if (extensions->enableAllocationSampling && (sizeCurrentTLHAlloc > 0)) {
			MM_AtomicOperations::add(&extensions->currentAllocationRemainder, sizeCurrentTLHAlloc);
			uintptr_t allocationSamplingInterval = extensions->allocationSamplingInterval;
			uintptr_t currentAllocationRemainder = extensions->currentAllocationRemainder;
			uintptr_t currentAllocationTemp = currentAllocationRemainder + sizeInBytesRequired;
			if (currentAllocationTemp >= allocationSamplingInterval) {
				/* going to out-of-line to notify that the sampling interval has been reached. */
				env->disableInlineTLHAllocate();
			}
		}
	}

	return didRefresh;
}


/**
 * Attempt to allocate an object in this TLH.
 */
void *
MM_TLHAllocationSupport::allocateFromTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool shouldCollectOnFailure)
{
	void *memPtr = NULL;

	Assert_MM_true(!env->getExtensions()->isSegregatedHeap());
	uintptr_t sizeInBytesRequired = allocDescription->getContiguousBytes();
	/* If there's insufficient space, refresh the current TLH */
	if (sizeInBytesRequired > getSize()) {
		refresh(env, allocDescription, shouldCollectOnFailure);
	}

	/* Try to fit the allocate into the current TLH */
	if(sizeInBytesRequired <= getSize()) {
		memPtr = (void *)getAlloc();
		setAlloc((void *)((uintptr_t)getAlloc() + sizeInBytesRequired));
#if defined(OMR_GC_TLH_PREFETCH_FTA)
		if (*_pointerToTlhPrefetchFTA < (intptr_t)sizeInBytesRequired) {
			*_pointerToTlhPrefetchFTA = 0;
		} else {
			*_pointerToTlhPrefetchFTA -= (intptr_t)sizeInBytesRequired;
		}
#endif /* OMR_GC_TLH_PREFETCH_FTA */
		allocDescription->setObjectFlags(getObjectFlags());
		allocDescription->setMemorySubSpace((MM_MemorySubSpace *)_tlh->memorySubSpace);
		allocDescription->completedFromTlh();
	}

	return memPtr;
}

/**
 * Replenish the allocation interface TLH cache with new storage.
 * This is a placeholder function for all non-TLH implementing configurations until a further revision of the code finally pushes TLH
 * functionality down to the appropriate level, and not so high that all configurations must recognize it.
 * For this implementation we simply redirect back to the supplied memory pool and if successful, populate the localized TLH and supplied
 * AllocationDescription with the appropriate information.
 * @return true on successful TLH replenishment, false otherwise.
 */
void *
MM_TLHAllocationSupport::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *memorySubSpace, MM_MemoryPool *memoryPool)
{
	void *addrBase, *addrTop;

	if(memoryPool->allocateTLH(env, allocDescription, getRefreshSize(), addrBase, addrTop)) {
		setupTLH(env, addrBase, addrTop, memorySubSpace, memoryPool);
		allocDescription->setMemorySubSpace(memorySubSpace);
		allocDescription->setObjectFlags(memorySubSpace->getObjectFlags());
		return addrBase;
	}
	return NULL;
}

void
MM_TLHAllocationSupport::flushCache(MM_EnvironmentBase *env)
{
	/* Since AllocationStats have been reset, reset the base as well*/
	_abandonedList = NULL;
	_abandonedListSize = 0;
	clear(env);
}

void
MM_TLHAllocationSupport::setupTLH(MM_EnvironmentBase *env, void *addrBase, void *addrTop, MM_MemorySubSpace *memorySubSpace, MM_MemoryPool *memoryPool)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	if (extensions->doFrequentObjectAllocationSampling){
		updateFrequentObjectsStats(env);
	}

	/* Set the new TLH values */
	setBase(addrBase);
	setAlloc(addrBase);
	setTop(addrTop);
	if (NULL != memorySubSpace) {
		setObjectFlags(memorySubSpace->getObjectFlags());
	}
	setMemoryPool(memoryPool);
	setMemorySubSpace(memorySubSpace);
#if defined(OMR_GC_TLH_PREFETCH_FTA)
	*_pointerToTlhPrefetchFTA = 0;
#endif /* OMR_GC_TLH_PREFETCH_FTA */
}

void
MM_TLHAllocationSupport::updateFrequentObjectsStats(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_FrequentObjectsStats* frequentObjectsStats = _objectAllocationInterface->getFrequentObjectsStats();

	if(NULL != frequentObjectsStats){
		/* presumably first and current alloc pointer will point to an object */
		GC_ObjectHeapIteratorAddressOrderedList objectHeapIterator(extensions, (omrobjectptr_t) getBase(), (omrobjectptr_t) getAlloc(), false, false);
		omrobjectptr_t object = NULL;
		uintptr_t limit = (((uintptr_t) getAlloc() - (uintptr_t) getBase())*extensions->frequentObjectAllocationSamplingRate)/100 + (uintptr_t) getBase();

		while(NULL != (object = objectHeapIterator.nextObject())){
			if( ((uintptr_t) object) > limit){
				break;
			}
			frequentObjectsStats->update(env, object);
		}
	}
}

#if defined(OMR_GC_OBJECT_ALLOCATION_NOTIFY)
void
MM_TLHAllocationSupport::objectAllocationNotify(MM_EnvironmentBase *env, void *heapBase, void *heapTop)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_CollectorLanguageInterface * cli = extensions->collectorLanguageInterface;
	GC_ObjectHeapIteratorAddressOrderedList objectHeapIterator(extensions, (omrobjectptr_t)heapBase, heapTop, false, false);
	omrobjectptr_t object = NULL;

	while(NULL != (object = objectHeapIterator.nextObject())){
		env->objectAllocationNotify(object);
	}
}
#endif /* OMR_GC_OBJECT_ALLOCATION_NOTIFY */

#endif /* defined(OMR_GC_THREAD_LOCAL_HEAP) */
