/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include <string.h>

#include "MemoryPoolAddressOrderedList.hpp"

#include "AllocateDescription.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Collector.hpp"
#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"
//#include "mmhook_internal.h"
#include "HeapRegionDescriptor.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "Heap.hpp"
#include "Math.hpp"

#if defined(OMR_VALGRIND_MEMCHECK)
#include "MemcheckWrapper.hpp"
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

#include "HeapRegionManager.hpp"
#include "HeapRegionDescriptor.hpp"

/**
 * Called when SATB barrier is enabled/disabled. We use this to set the TLH alignment base.
 */
void
concurrentSATBToggled(J9HookInterface **hook, uintptr_t eventNum, void *eventData, void *userData)
{
	MM_ConcurrentSATBEvent *event = (MM_ConcurrentSATBEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_MemoryPoolAddressOrderedList *)userData)->setParallelGCAlignment(env, event->satbEnabled);
}

/**
 * Create and initialize a new instance of the receiver.
 */
MM_MemoryPoolAddressOrderedList *
MM_MemoryPoolAddressOrderedList::newInstance(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize)
{
	return newInstance(env, minimumFreeEntrySize, "Unknown");
}

/**
 * Create and initialize a new instance of the receiver.
 */
MM_MemoryPoolAddressOrderedList *
MM_MemoryPoolAddressOrderedList::newInstance(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize, const char *name)
{
	MM_MemoryPoolAddressOrderedList *memoryPool;

	memoryPool = (MM_MemoryPoolAddressOrderedList *)env->getForge()->allocate(sizeof(MM_MemoryPoolAddressOrderedList), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memoryPool) {
		memoryPool = new(memoryPool) MM_MemoryPoolAddressOrderedList(env, minimumFreeEntrySize, name);
		if (!memoryPool->initialize(env)) {
			memoryPool->kill(env);
			memoryPool = NULL;
		}
	}
	return memoryPool;
}

bool
MM_MemoryPoolAddressOrderedList::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *ext = env->getExtensions();
	J9ModronAllocateHint *inactiveHint, *previousInactiveHint;
	uintptr_t inactiveCount;

	Assert_MM_true(_minimumFreeEntrySize >= CARD_SIZE);

	if(!MM_MemoryPool::initialize(env)) {
		return false;
	}

	if(!_extensions->_lazyCollectorInit) {
		if(!initializeSweepPool(env)) {
			return false;
		}
	}

	_referenceHeapFreeList = &_heapFreeList;

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	/* this memoryPool can be used by scavenger, maximum tlh size should be max(_extensions->tlhMaximumSize, _extensions->scavengerScanCacheMaximumSize) */
#if defined(OMR_GC_MODRON_SCAVENGER)
	uintptr_t tlhMaximumSize = OMR_MAX(_extensions->getTlhMaximumSize(), _extensions->scavengerScanCacheMaximumSize);
#else /* OMR_GC_MODRON_SCAVENGER */
	uintptr_t tlhMaximumSize = _extensions->getTlhMaximumSize();
#endif /* OMR_GC_MODRON_SCAVENGER */
	_largeObjectAllocateStats = MM_LargeObjectAllocateStats::newInstance(env, (uint16_t)ext->largeObjectAllocationProfilingTopK, ext->largeObjectAllocationProfilingThreshold, ext->largeObjectAllocationProfilingVeryLargeObjectThreshold, (float)ext->largeObjectAllocationProfilingSizeClassRatio / (float)100.0,
			_extensions->heap->getMaximumMemorySize(), tlhMaximumSize + _minimumFreeEntrySize, _extensions->tlhMinimumSize);
#else
	_largeObjectAllocateStats = MM_LargeObjectAllocateStats::newInstance(env, (uint16_t)ext->largeObjectAllocationProfilingTopK, ext->largeObjectAllocationProfilingThreshold, ext->largeObjectAllocationProfilingVeryLargeObjectThreshold, (float)ext->largeObjectAllocationProfilingSizeClassRatio / (float)100.0,
			_extensions->heap->getMaximumMemorySize(), 0, 0);
#endif
	if (NULL == _largeObjectAllocateStats) {
		return false;
	} 

	/* At this moment we do not know who is creator of this pool, so we do not set _largeObjectCollectorAllocateStats yet.
	 * Tenure SubSpace for Gencon will set _largeObjectCollectorAllocateStats to _largeObjectAllocateStats (we append collector stats to mutator stats)
	 * SemiSpace will leave _largeObjectCollectorAllocateStats at NULL (no interest in Collector stats)
	 * Tenure SubSpace for Flat will leave _largeObjectCollectorAllocateStats at NULL (no interest in Colletor stats)
	 */
	 
	if (!_heapLock.initialize(env, &ext->lnrlOptions, "MM_MemoryPoolAddressOrderedList:_heapLock")) {
		return false;
	}

	if (!_resetLock.initialize(env, &ext->lnrlOptions, "MM_MemoryPoolAddressOrderedList:_resetLock")) {
		return false;
	}

	_hintActive = NULL;
	_hintLru = 0;

	/* Link up the inactive hints */
	inactiveHint = (J9ModronAllocateHint *)_hintStorage;
	previousInactiveHint = NULL;
	inactiveCount = HINT_ELEMENT_COUNT;
	while(inactiveCount) {
		inactiveHint->next = previousInactiveHint;
		previousInactiveHint = inactiveHint;
		inactiveHint += 1;
		inactiveCount -= 1;
	}
	_hintInactive = previousInactiveHint;

	return true;
}

bool
MM_MemoryPoolAddressOrderedList::initializeSweepPool(MM_EnvironmentBase *env)
{
	if (NULL == _sweepPoolState) {
		/*
		 * Create Sweep Pool State for MPAOL
		 */
		MM_Collector* globalCollector = (MM_Collector*) _extensions->getGlobalCollector();
		Assert_MM_true(NULL != globalCollector);

		_sweepPoolState = static_cast<MM_SweepPoolState*>(globalCollector->createSweepPoolState(env, this));
		if (NULL == _sweepPoolState) {
			return false;
		}

		/*
		 * Get Sweep Pool Manager for MPAOL
		 * For platforms it doesn't required still be NULL
		 */
		_sweepPoolManager = (MM_SweepPoolManagerAddressOrderedListBase*) env->getExtensions()->sweepPoolManagerAddressOrderedList;
	}
	return true;
}

void
MM_MemoryPoolAddressOrderedList::tearDown(MM_EnvironmentBase *env)
{
	if (MEMORY_TYPE_OLD == _memorySubSpace->getTypeFlags()) {
		J9HookInterface **mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);
		(*mmPrivateHooks)->J9HookUnregister(mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_SATB_TOGGLED, concurrentSATBToggled, (void *)this);
	}

	MM_MemoryPool::tearDown(env);

	if (NULL != _sweepPoolState) {
		MM_Collector* globalCollector = (MM_Collector*) _extensions->getGlobalCollector();
		Assert_MM_true(NULL != globalCollector);
		globalCollector->deleteSweepPoolState(env, _sweepPoolState);
	}

	if (NULL != _largeObjectAllocateStats) {
		_largeObjectAllocateStats->kill(env);
	}
	
	_largeObjectCollectorAllocateStats = NULL;

	_heapLock.tearDown();
	_resetLock.tearDown();
}

/****************************************
 * Hint Functionality
 ****************************************
 */
void
MM_MemoryPoolAddressOrderedList::clearHints()
{
	J9ModronAllocateHint *activeHint, *nextActiveHint, *inactiveHint;

	activeHint = _hintActive;
	inactiveHint = _hintInactive;

	while(activeHint) {
		nextActiveHint = activeHint->next;
		activeHint->next = inactiveHint;
		inactiveHint = activeHint;
		activeHint = nextActiveHint;
	}
	_hintInactive = inactiveHint;
	_hintActive = NULL;
	_hintLru = 1;
}

MMINLINE void
MM_MemoryPoolAddressOrderedList::addHint(MM_HeapLinkedFreeHeader *freeEntry, uintptr_t lookupSize)
{
	/* Travel the list removing any hints that this new hint will override */
	J9ModronAllocateHint *previousActiveHint, *currentActiveHint, *nextActiveHint;
	previousActiveHint = NULL;
	currentActiveHint = _hintActive;
	while(currentActiveHint) {
		/* Check what address range the free header to be added lies in (below, equal, above) */
		if(freeEntry < currentActiveHint->heapFreeHeader) {
			if(lookupSize >= currentActiveHint->size) {
				/* remove this hint */
				goto removeHint;
			}
		} else {
			if(freeEntry == currentActiveHint->heapFreeHeader) {
				if(lookupSize < currentActiveHint->size) {
					/* remove this hint */
					goto removeHint;
				} else {
					/* The hint to be added is redundant */
					return ;
				}
			} else {
				/* freeEntry > currentActiveHint->heapFreeHeader <--- implied */
				if(lookupSize <= currentActiveHint->size) {
					/* remove this hint */
					goto removeHint;
				}
			}
		}

		/* Do nothing to this hint - move to the next one */
		previousActiveHint = currentActiveHint;
		currentActiveHint = currentActiveHint->next;

		continue;

removeHint:
		/* Remove the currentActiveHint */
		nextActiveHint = currentActiveHint->next;
		if(previousActiveHint) {
			previousActiveHint->next = nextActiveHint;
		} else {
			_hintActive = nextActiveHint;
		}
		currentActiveHint->next = _hintInactive;
		_hintInactive = currentActiveHint;
		currentActiveHint = nextActiveHint;
	}


	/* Get a hint from the list */
	J9ModronAllocateHint *hint;
	if(_hintInactive) {
		/* Consume the hint from the inactive list */
		hint = _hintInactive;
		_hintInactive = hint->next;
		/* Place new hint in active list */
		hint->next = _hintActive;
		_hintActive = hint;
	} else {
		J9ModronAllocateHint *currentHint;
		/* Find the smallest LRU value and recycle the hint */
		hint = _hintActive;
		currentHint = hint->next;
		while(currentHint) {
			if(hint->lru > currentHint->lru) {
				hint = currentHint;
			}
			currentHint = currentHint->next;
		}
	}

	/* Update the global and local LRU */
	hint->lru = _hintLru++;

	/* Initialize the hint info */
	hint->size = lookupSize;
	hint->heapFreeHeader = freeEntry;
}

MMINLINE J9ModronAllocateHint *
MM_MemoryPoolAddressOrderedList::findHint(uintptr_t lookupSize)
{
	J9ModronAllocateHint *hint = NULL, *candidateHint, *previousHint;

	/* Be sure to remove stale hints as we search the list (stale hints are addresses < the heap free head) */

	candidateHint = _hintActive;
	previousHint = NULL;

	while(candidateHint) {
		if(!_heapFreeList || (candidateHint->heapFreeHeader < _heapFreeList)) {
			/* Hint is stale - remove */
			J9ModronAllocateHint *nextHint;

			if(previousHint) {
				previousHint->next = candidateHint->next;
			} else {
				_hintActive = candidateHint->next;
			}

			nextHint = candidateHint->next;
			candidateHint->next = _hintInactive;
			_hintInactive = candidateHint;
			candidateHint = nextHint;
		} else {
			if(candidateHint->size < lookupSize) {
				if(hint) {
					if(candidateHint->size > hint->size) {
						hint = candidateHint;
					}
				} else {
					hint = candidateHint;
				}
			}
			previousHint = candidateHint;
			candidateHint = candidateHint->next;
		}
	}

	if(hint) {
		/* Update the global and local LRU */
		hint->lru = _hintLru++;
	}

	return hint;
}

MMINLINE void
MM_MemoryPoolAddressOrderedList::removeHint(MM_HeapLinkedFreeHeader *freeEntry)
{
	J9ModronAllocateHint *hint, *previousHint, *nextHint;

	previousHint = NULL;
	hint = _hintActive;

	while(hint) {
		if(hint->heapFreeHeader == freeEntry) {
			nextHint = hint->next;
			/* Add the hint to the free list */
			hint->next = _hintInactive;
			_hintInactive = hint;
			/* Glue the previous hint to the next hint */
			if(previousHint) {
				previousHint->next = nextHint;
			} else {
				/* no previous hint - must be the head */
				_hintActive = nextHint;
			}
			hint = nextHint;
		} else {
			/* Move to the next hint */
			previousHint = hint;
			hint = hint->next;
		}
	}
}

MMINLINE void
MM_MemoryPoolAddressOrderedList::updateHint(MM_HeapLinkedFreeHeader *oldFreeEntry, MM_HeapLinkedFreeHeader *newFreeEntry)
{
	J9ModronAllocateHint *hint = _hintActive;

	while(hint) {
		if(hint->heapFreeHeader == oldFreeEntry) {
			hint->heapFreeHeader = newFreeEntry;
			/*
			 * we can not break here!!!
			 * It is possible that we have more that one hint for this address
			 * All of them should be updated
			 */
		} else {
			/* Move to the next hint */
			hint = hint->next;
		}
	}
}

/**
 * Update all hints to point no further than the given free entry.
 * For all active hints, find any which have a pointer greater than the given free entry.  If found, reset
 * the pointer to be the free entry.  This is used when free entries are added to the middle of a free list;
 * hints may point beyond valid free entries in the list.
 */
void
MM_MemoryPoolAddressOrderedList::updateHintsBeyondEntry(MM_HeapLinkedFreeHeader *freeEntry)
{
	J9ModronAllocateHint *hint = _hintActive;

	while(hint) {
		if (hint->heapFreeHeader > freeEntry) {
			hint->heapFreeHeader = freeEntry;
		}
		/* Move to the next hint */
		hint = hint->next;
	}
}

/****************************************
 * Allocation
 ****************************************
 */
MMINLINE void *
MM_MemoryPoolAddressOrderedList::internalAllocate(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader  *currentFreeEntry, *previousFreeEntry, *recycleEntry;
	uintptr_t candidateHintSize;
	uintptr_t recycleEntrySize;
	uintptr_t walkCount;
	J9ModronAllocateHint *allocateHintUsed;
	void *addrBase;
	uintptr_t largestFreeEntry = 0;
	
	if (lockingRequired) {
		_heapLock.acquire();
	}

#if defined(OMR_GC_CONCURRENT_SWEEP)
retry:
#endif /* OMR_GC_CONCURRENT_SWEEP */

	currentFreeEntry = _heapFreeList;
	previousFreeEntry = NULL;
	walkCount = 0;
	allocateHintUsed = NULL;
	candidateHintSize = 0;

	/* Large object - use a hint if it is available */
	allocateHintUsed = findHint(sizeInBytesRequired);
	if(allocateHintUsed) {
		currentFreeEntry = allocateHintUsed->heapFreeHeader;
		candidateHintSize = allocateHintUsed->size;
	}


	while(currentFreeEntry) {
		if (doesNeedCardAlignment(env, currentFreeEntry)) {
			currentFreeEntry = doFreeEntryCardAlignmentUpTo(env, currentFreeEntry);
			if (NULL == currentFreeEntry) {
				currentFreeEntry = (FREE_ENTRY_END == _firstCardUnalignedFreeEntry) ? NULL : _firstCardUnalignedFreeEntry;
				previousFreeEntry = (FREE_ENTRY_END == _prevCardUnalignedFreeEntry) ? NULL : _prevCardUnalignedFreeEntry;
				walkCount += 1;
				continue;
			}
		}
		uintptr_t currentFreeEntrySize = currentFreeEntry->getSize();
		/* while we are walking, keep track of the largest free entry.  We will need this in the case of allocation failure to update the pool's largest free */
		if (currentFreeEntrySize > largestFreeEntry) {
			largestFreeEntry = currentFreeEntrySize;
		}
		
		if(sizeInBytesRequired <= currentFreeEntrySize) {
			break;
		}

		if(candidateHintSize < currentFreeEntrySize) {
			candidateHintSize = currentFreeEntrySize;
		}

		walkCount += 1;

		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext(compressed);
		Assert_MM_true((NULL == currentFreeEntry) || (currentFreeEntry > previousFreeEntry));
	}

	/* Check if an entry was found */
	if(!currentFreeEntry) {
#if defined(OMR_GC_CONCURRENT_SWEEP)
		if(_memorySubSpace->replenishPoolForAllocate(env, this, sizeInBytesRequired)) {
			goto retry;
		}
#endif /* OMR_GC_CONCURRENT_SWEEP */
		goto fail_allocate;
	}

	_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
	if((walkCount >= J9MODRON_ALLOCATION_MANAGER_HINT_MAX_WALK) || ((walkCount > 1) && allocateHintUsed)) {
		addHint(previousFreeEntry, candidateHintSize);
	}

	/* Adjust the free memory size */
	_freeMemorySize -= sizeInBytesRequired;

	/* Update allocation statistics */
	_allocCount += 1;
	_allocBytes += sizeInBytesRequired;
	_allocSearchCount += walkCount;

	/* Determine what to do with the recycled portion of the free entry */
	recycleEntrySize = currentFreeEntry->getSize() - sizeInBytesRequired;

	addrBase = (void *)currentFreeEntry;
	recycleEntry = (MM_HeapLinkedFreeHeader *)(((uint8_t *)currentFreeEntry) + sizeInBytesRequired);

	if (recycleHeapChunk(recycleEntry, ((uint8_t *)recycleEntry) + recycleEntrySize, previousFreeEntry, currentFreeEntry->getNext(compressed))) {
		updatePrevCardUnalignedFreeEntry(currentFreeEntry->getNext(compressed), recycleEntry);
		updateHint(currentFreeEntry, recycleEntry);
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(recycleEntrySize);
	} else {
		updatePrevCardUnalignedFreeEntry(currentFreeEntry->getNext(compressed), previousFreeEntry);
		/* Adjust the free memory size and count */
		_freeMemorySize -= recycleEntrySize;
		_freeEntryCount -= 1;

		/* Update discard bytes if necessary */
		_allocDiscardedBytes += recycleEntrySize;

		/* Removed from the free list - Kill the hint if necessary */
		removeHint(currentFreeEntry);
	}
	
	/* Collector object allocate stats for Survivor are not interesting (_largeObjectCollectorAllocateStats is null for Survivor) */	
	if (NULL != largeObjectAllocateStats) {
		largeObjectAllocateStats->allocateObject(sizeInBytesRequired);
	}

	if(lockingRequired) {
			_heapLock.release();
		}
	
	Assert_MM_true(NULL != addrBase);
	
	return addrBase;

fail_allocate:
	/* Since we failed to allocate, update the largest free entry so that outside callers will be able to skip this pool, next time, in Tarok configurations */
	setLargestFreeEntry(largestFreeEntry);
	if (lockingRequired) {
		_heapLock.release();
	}
	return NULL;
}

void *
MM_MemoryPoolAddressOrderedList::allocateObject(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription)
{
	void * addr = internalAllocate(env, allocDescription->getContiguousBytes(), true, _largeObjectAllocateStats);

	if (addr != NULL) {
#if defined(OMR_GC_ALLOCATION_TAX)
		if(env->getExtensions()->payAllocationTax) {
			allocDescription->setAllocationTaxSize(allocDescription->getBytesRequested());
		}
#endif  /* OMR_GC_ALLOCATION_TAX */
		allocDescription->setTLHAllocation(false);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return addr;
}

void *
MM_MemoryPoolAddressOrderedList::collectorAllocate(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool lockingRequired)
{
	void *addr = internalAllocate(env, allocDescription->getContiguousBytes(), lockingRequired, _largeObjectCollectorAllocateStats);

	if (addr != NULL) {
		allocDescription->setTLHAllocation(false);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return addr;
}

bool
MM_MemoryPoolAddressOrderedList::alignTLHForParallelGC(MM_EnvironmentBase *env,  MM_HeapLinkedFreeHeader *freeEntry, uintptr_t *consumedSize)
{
	/* Determine the boundaries (based on alignment params) that contain the tlh being inited. */
	void *tlhBase = (void *)freeEntry;

	Assert_MM_true(tlhBase >= _parallelGCAlignmentBase);

	uintptr_t remainder = ((uintptr_t)tlhBase - (uintptr_t)_parallelGCAlignmentBase) % _parallelGCAlignmentSize;
	void *baseBoundary = (void *)((uintptr_t)tlhBase - remainder);
	/* topBoundary may not be the real top boundary for the expected chunk, it's not wrapped around pool/region boundaries, which is fine. */
	void *topBoundary = (void *)((uintptr_t)baseBoundary + _parallelGCAlignmentSize);

	void *tlhTopProjection = (void *)(((uintptr_t)freeEntry) + *consumedSize);

	if (tlhTopProjection >  topBoundary) {
		uintptr_t offset = ((uintptr_t)tlhTopProjection - (uintptr_t)topBoundary);
		*consumedSize -= offset;
	}

	/* TODO: Consider topBoundary as the TLH base and clip the tlh from the base for alignment. */
	if (*consumedSize < _minimumFreeEntrySize) {
		return false;
	}

	return true;
}

MMINLINE bool
MM_MemoryPoolAddressOrderedList::internalAllocateTLH(MM_EnvironmentBase *env, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats)
{
	bool const compressed = compressObjectReferences();
	uintptr_t freeEntrySize = 0;
	void *topOfRecycledChunk = NULL;
	MM_HeapLinkedFreeHeader *entryNext = NULL;
	MM_HeapLinkedFreeHeader *freeEntry = NULL;
	uintptr_t consumedSize = 0;
	uintptr_t recycleEntrySize = 0;
	
	if (lockingRequired) {
		_heapLock.acquire();
	}

retry:
	freeEntry = _heapFreeList;
#if defined(OMR_GC_CONCURRENT_SWEEP)

	/* Check if an entry was found */
	if(!freeEntry) {
		if(_memorySubSpace->replenishPoolForAllocate(env, this, _minimumFreeEntrySize)) {
			goto retry;
		}
		goto fail_allocate;
	}
#else /* OMR_GC_CONCURRENT_SWEEP */

	/* Check if an entry was found */
	if(!freeEntry) {
		goto fail_allocate;
	}
#endif /* OMR_GC_CONCURRENT_SWEEP */

	if (doesNeedCardAlignment(env, freeEntry)) {
		freeEntry = doFreeEntryCardAlignmentUpTo(env, freeEntry);
		if (NULL == freeEntry) {
			goto retry;
		}
	}

	freeEntrySize = freeEntry->getSize();

	_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(freeEntrySize);

	if (0 == (consumedSize = getConsumedSizeForTLH(env, freeEntry, maximumSizeInBytesRequired))) {
		goto retry;
	}

	/* If the leftover chunk is smaller than the minimum size, hand it out. Handing out the leftover chunk may break alignment,
	 * ensure we don't require alignment first. */
	recycleEntrySize = freeEntrySize - consumedSize;
	if (recycleEntrySize && (recycleEntrySize < _minimumFreeEntrySize) && !isAlignmentForParallelGCRequired()) {
		consumedSize += recycleEntrySize;
		recycleEntrySize = 0;
	}

	/* Adjust the free memory size */
	_freeMemorySize -= consumedSize;

	_allocCount += 1;
	_allocBytes += consumedSize;
	/* Collector TLH allocate stats for Survivor are not interesting (_largeObjectCollectorAllocateStats is null for Survivor) */	
	if (NULL != largeObjectAllocateStats) {
		largeObjectAllocateStats->incrementTlhAllocSizeClassStats(consumedSize);
	}

	addrBase = (void *)freeEntry;
	addrTop = (void *) (((uint8_t *)addrBase) + consumedSize);
	entryNext = freeEntry->getNext(compressed);

	if (recycleEntrySize > 0) {
		topOfRecycledChunk = ((uint8_t *)addrTop) + recycleEntrySize;
		/* Recycle the remaining entry back onto the free list (if applicable) */
		if (recycleHeapChunk(addrTop, topOfRecycledChunk, NULL, entryNext)) {
			updatePrevCardUnalignedFreeEntry(entryNext, (MM_HeapLinkedFreeHeader *)addrTop);
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(recycleEntrySize);
		} else {
			updatePrevCardUnalignedFreeEntry(entryNext, FREE_ENTRY_END);
			/* Adjust the free memory size and count */
			_freeMemorySize -= recycleEntrySize;
			_freeEntryCount -= 1;

			_allocDiscardedBytes += recycleEntrySize;
		}
	} else {
		updatePrevCardUnalignedFreeEntry(entryNext, FREE_ENTRY_END);
		/* If not recycling just update the free list pointer to the next free entry */
		_heapFreeList = entryNext;
		/* also update the freeEntryCount as recycleHeapChunk would do this */
		_freeEntryCount -= 1;
	}

	if (lockingRequired) {
		_heapLock.release();
	}

	return true;

fail_allocate:
	/* if we failed to allocate a TLH, this pool is either full or so heavily fragmented that it is effectively full */
	_largestFreeEntry = 0;
	if(lockingRequired) {
		_heapLock.release();
	}
	return false;
}

uintptr_t
MM_MemoryPoolAddressOrderedList::getConsumedSizeForTLH(MM_EnvironmentBase *env, MM_HeapLinkedFreeHeader *freeEntry, uintptr_t maximumSizeInBytesRequired) {
	bool const compressed = compressObjectReferences();

	/* Consume the bytes and set the return pointer values */
	uintptr_t freeEntrySize = freeEntry->getSize();
	Assert_MM_true(freeEntrySize >= _minimumFreeEntrySize);
	uintptr_t consumedSize = (maximumSizeInBytesRequired > freeEntrySize) ? freeEntrySize : maximumSizeInBytesRequired;

	if (isAlignmentForParallelGCRequired()) {
		if (!alignTLHForParallelGC(env, freeEntry, &consumedSize)) {
			/* If alignment was required and it failed (i.e resulted in consumedSize < minEntrySize), abandon entry and retry */
			abandonHeapChunk((void *)freeEntry, (void *)(((uintptr_t)freeEntry) + freeEntrySize));

			_freeMemorySize -= freeEntrySize;
			_allocDiscardedBytes += freeEntrySize;

			MM_HeapLinkedFreeHeader *entryNext = freeEntry->getNext(compressed);

			updatePrevCardUnalignedFreeEntry(entryNext, FREE_ENTRY_END);

			_heapFreeList = entryNext;
			_freeEntryCount -= 1;

			consumedSize = 0;
		}
	}

	return consumedSize;
}

void *
MM_MemoryPoolAddressOrderedList::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription,
											uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop)
{
	void *tlhBase = NULL;

	if (internalAllocateTLH(env, maximumSizeInBytesRequired, addrBase, addrTop, true, _largeObjectAllocateStats)) {
		tlhBase = addrBase;
	}

	if (NULL != tlhBase) {
#if defined(OMR_GC_ALLOCATION_TAX)
		if(env->getExtensions()->payAllocationTax) {
			allocDescription->setAllocationTaxSize((uint8_t *)addrTop - (uint8_t *)addrBase);
		}
#endif  /* OMR_GC_ALLOCATION_TAX */

		allocDescription->setTLHAllocation(true);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}

	return tlhBase;
}

void *
MM_MemoryPoolAddressOrderedList::collectorAllocateTLH(MM_EnvironmentBase *env,
													 MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired,
													 void * &addrBase, void * &addrTop, bool lockingRequired)
{
	void *base = NULL;
	if (internalAllocateTLH(env, maximumSizeInBytesRequired, addrBase, addrTop, lockingRequired, _largeObjectCollectorAllocateStats)) {
		base = addrBase;
		allocDescription->setTLHAllocation(true);
		allocDescription->setNurseryAllocation((_memorySubSpace->getTypeFlags() == MEMORY_TYPE_NEW) ? true : false);
		allocDescription->setMemoryPool(this);
	}
	return base;
}

/****************************************
 * Free list building
 ****************************************
 */

void
MM_MemoryPoolAddressOrderedList::reset(Cause cause)
{
	/* Call superclass first .. */
	MM_MemoryPool::reset(cause);

	clearHints();
	_heapFreeList = (MM_HeapLinkedFreeHeader *)NULL;
	_scannableBytes = 0;
	_nonScannableBytes = 0;
	_firstCardUnalignedFreeEntry = FREE_ENTRY_END;
	_prevCardUnalignedFreeEntry = FREE_ENTRY_END;


	_lastFreeEntry = NULL;
	_adjustedBytesForCardAlignment = 0;
	resetFreeEntryAllocateStats(_largeObjectAllocateStats);
	resetLargeObjectAllocateStats();
}

/**
 * As opposed to reset, which will empty out, this will fill out as if everything is free.
 * Returns the freelist entry created at the end of the given region
 */
MM_HeapLinkedFreeHeader *
MM_MemoryPoolAddressOrderedList::rebuildFreeListInRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, MM_HeapLinkedFreeHeader *previousFreeEntry)
{
	MM_HeapLinkedFreeHeader *newFreeEntry = NULL;
	void* rangeBase = region->getLowAddress();
	void* rangeTop = region->getHighAddress();
	uintptr_t rangeSize = region->getSize();

	/* This may be called while VM running (indirectly from JCL
	 * to create RTJ Scoped Memory, so we need proper locking
	 */
	acquireResetLock(env);
	lock(env);
	reset();

	/* TODO 108399 Determine whether this is still necessary (OMR_SCAVENGER_DEBUG is defined in Scqavenger.cpp and is not accessible here) */
#if defined(OMR_SCAVENGER_DEBUG)
	/* Fill the new space with a bogus value */
	memset(rangeBase, 0xFA, rangeSize);
#endif /* OMR_SCAVENGER_DEBUG */

	/* Segment list is already address ordered */
	if (createFreeEntry(env, rangeBase, rangeTop, previousFreeEntry, NULL)) {
		newFreeEntry = (MM_HeapLinkedFreeHeader *)rangeBase;

		/* Adjust the free memory data */
		_freeMemorySize = rangeSize;
		_freeEntryCount = 1;

		_heapFreeList = newFreeEntry;
		/* we already did reset, so it's safe to call increment */
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(rangeSize);

		TRIGGER_J9HOOK_MM_PRIVATE_REBUILD_FREE_LIST(env->getExtensions()->privateHookInterface, env->getOmrVMThread(), rangeBase, rangeTop);
	}
	unlock(env);
	releaseResetLock(env);

	return newFreeEntry;
}


#if defined(DEBUG)
bool
MM_MemoryPoolAddressOrderedList::isValidListOrdering()
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader *walk = _heapFreeList;
	while (NULL != walk) {
		if ((NULL != walk->getNext(compressed)) && (walk >= walk->getNext(compressed))) {
			return false;
		}
		walk = walk->getNext(compressed);
	}
	return true;
}
#endif

/**
 * Add the range of memory to the free list of the receiver.
 *
 * @param expandSize Number of bytes to remove from the memory pool
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 *
 * @todo check if this routine can eliminate any of the other similar routines.
 *
 */
void
MM_MemoryPoolAddressOrderedList::expandWithRange(MM_EnvironmentBase *env, uintptr_t expandSize, void *lowAddress, void *highAddress, bool canCoalesce)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader *previousFreeEntry, *nextFreeEntry;

	if(0 == expandSize) {
		return ;
	}

	/* Handle the entries that are too small to make the free list */
	if(expandSize < _minimumFreeEntrySize) {
		abandonHeapChunk(lowAddress, highAddress);
		return ;
	}

	/* Find the free entries in the list the appear before/after the range being added */
	previousFreeEntry = NULL;
	nextFreeEntry = _heapFreeList;
	while(nextFreeEntry) {
		if(lowAddress < nextFreeEntry)  {
			break;
		}
		previousFreeEntry = nextFreeEntry;
		nextFreeEntry = nextFreeEntry->getNext(compressed);
	}

	/* Check if the range can be coalesced with either the surrounding free entries */
	if(canCoalesce) {
		/* Check if the range can be fused to the tail previous free entry */
		if(previousFreeEntry && (lowAddress == (void *) (((uintptr_t)previousFreeEntry) + previousFreeEntry->getSize()))) {
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(previousFreeEntry->getSize());
			previousFreeEntry->expandSize(expandSize);

			/* Update the free list information */
			_freeMemorySize += expandSize;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(previousFreeEntry->getSize());

			assume0(isMemoryPoolValid(env, true));
			return ;
		}
		/* Check if the range can be fused to the head of the next free entry */
		if(nextFreeEntry && (highAddress == (void *)nextFreeEntry)) {
			MM_HeapLinkedFreeHeader *newFreeEntry = (MM_HeapLinkedFreeHeader *)lowAddress;
			assume0((NULL == nextFreeEntry->getNext(compressed)) || (newFreeEntry < nextFreeEntry->getNext(compressed)));

			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(nextFreeEntry->getSize());

			newFreeEntry->setNext(nextFreeEntry->getNext(compressed), compressed);
			newFreeEntry->setSize(expandSize + nextFreeEntry->getSize());

			/* The previous free entry next pointer must be updated */
			if(previousFreeEntry) {
				assume0(newFreeEntry > previousFreeEntry);
				previousFreeEntry->setNext(newFreeEntry, compressed);
			} else {
				_heapFreeList = newFreeEntry;
			}

			/* Update the free list information */
			_freeMemorySize += expandSize;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(newFreeEntry->getSize());

			assume0(isMemoryPoolValid(env, true));
			return ;
		}
	}

	/* No coalescing available - build a free entry from the range that will be inserted into the list */
	MM_HeapLinkedFreeHeader *freeEntry = (MM_HeapLinkedFreeHeader *)lowAddress;

#if defined(OMR_VALGRIND_MEMCHECK)
	valgrindMakeMemUndefined((uintptr_t) freeEntry, sizeof(MM_HeapLinkedFreeHeader));
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

	assume0((NULL == nextFreeEntry) || (nextFreeEntry > freeEntry));
	freeEntry->setNext(nextFreeEntry, compressed);
	freeEntry->setSize(expandSize);

	/* Insert the free entry into the list (the next entry was handled above) */
	if(previousFreeEntry) {
		assume0(previousFreeEntry < freeEntry);
		previousFreeEntry->setNext(freeEntry, compressed);
	} else {
		_heapFreeList = freeEntry;
	}

	/* Update the free list information */
	_freeMemorySize += expandSize;
	_freeEntryCount += 1;

	_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(expandSize);
	
	if (freeEntry->getSize() > _largestFreeEntry) {
		_largestFreeEntry = freeEntry->getSize(); 
	}

	assume0(isMemoryPoolValid(env, true));
}

/**
 * Remove the range of memory to the free list of the receiver.
 *
 * @param expandSize Number of bytes to remove from the memory pool
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 *
 * @note The expectation is that the range consists ONLY of free elements (no live data appears).
 *
 */
void *
MM_MemoryPoolAddressOrderedList::contractWithRange(MM_EnvironmentBase *env, uintptr_t contractSize, void *lowAddress, void *highAddress)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader *currentFreeEntry, *currentFreeEntryTop, *previousFreeEntry, *nextFreeEntry;
	uintptr_t totalContractSize;
	intptr_t contractCount;

	if(0 == contractSize) {
		return NULL;
	}

	/* Find the free entry that encompasses the range to contract */
	/* TODO: Could we use hints to find a better starting address?  Are hints still valid? */
	previousFreeEntry = NULL;
	currentFreeEntry = _heapFreeList;
	while(currentFreeEntry) {
		if( (lowAddress >= currentFreeEntry) && (highAddress <= (void *) (((uintptr_t)currentFreeEntry) + currentFreeEntry->getSize())) ) {
			break;
		}
		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	assume0(NULL != currentFreeEntry);  /* Can't contract what doesn't exist */

	totalContractSize = contractSize;
	contractCount = 1;
	_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

	/* Record the next free entry after the current one which we are going to contract */
	nextFreeEntry = currentFreeEntry->getNext(compressed);

	/* Glue any new free entries that have been split from the original free entry into the free list */
	/* Start back to front, attaching the current entry to the next entry, and the current entry taking
	 * the place of the next entry */

	/* Determine what to do with any trailing bytes to the free entry that are not being contracted. */
	currentFreeEntryTop = (MM_HeapLinkedFreeHeader *) (((uintptr_t)currentFreeEntry) + currentFreeEntry->getSize());
	if(currentFreeEntryTop != (MM_HeapLinkedFreeHeader *)highAddress) {
		/* Space at the tail that is not being contracted - is it a valid free entry? */
		if (createFreeEntry(env, highAddress, currentFreeEntryTop, NULL, nextFreeEntry)) {
			/* The entry is a free list candidate */
			nextFreeEntry = (MM_HeapLinkedFreeHeader *)highAddress;
			contractCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(nextFreeEntry->getSize());
		} else {
			/* The entry was not large enough for the free list */
			uintptr_t trailingSize = ((uintptr_t)currentFreeEntryTop) - ((uintptr_t)highAddress);
			totalContractSize += trailingSize;
		}
	}

	/* Determine what to do with any leading bytes to the free entry that are not being contracted. */
	if(currentFreeEntry != (MM_HeapLinkedFreeHeader *)lowAddress) {
		if (createFreeEntry(env, currentFreeEntry, lowAddress, NULL, nextFreeEntry)) {
			nextFreeEntry = currentFreeEntry;
			contractCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
		} else {
			/* Abandon the leading entry - it is too small to be on the free list */
			uintptr_t leadingSize = ((uintptr_t)lowAddress) - ((uintptr_t)currentFreeEntry);
			totalContractSize += leadingSize;
		}
	}

	/* Attach our findings to the previous entry, and assume the head of the list if there was none */
	if(previousFreeEntry) {
		assume0((NULL == nextFreeEntry) || (previousFreeEntry < nextFreeEntry));
		previousFreeEntry->setNext(nextFreeEntry, compressed);
	} else {
		_heapFreeList = nextFreeEntry;
	}

	/* Adjust the free memory data */
	_freeMemorySize -= totalContractSize;
	_freeEntryCount -= contractCount;

	assume0(isMemoryPoolValid(env, true));
	
	return lowAddress;
}

/**
 * Add contents of an address ordered list of all free entries to this pool
 *
 * @param freeListHead Head of list of free entries to be added
 * @param freeListTail Tail of list of free entries to be added
 *
 */
void
MM_MemoryPoolAddressOrderedList::addFreeEntries(MM_EnvironmentBase *env,
												MM_HeapLinkedFreeHeader* &freeListHead, MM_HeapLinkedFreeHeader* &freeListTail,
												uintptr_t freeListMemoryCount, uintptr_t freeListMemorySize)
{
	bool const compressed = compressObjectReferences();
	uintptr_t localFreeListMemoryCount = freeListMemoryCount;

	MM_HeapLinkedFreeHeader *currentFreeEntry = freeListHead;

	while (currentFreeEntry != NULL) {
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	/* Find the first free entry, if any, within specified range */
	MM_HeapLinkedFreeHeader *previousFreeEntry = NULL;
	currentFreeEntry = _heapFreeList;

	assume0(isValidListOrdering());

	while(NULL != currentFreeEntry) {
		if(currentFreeEntry > freeListHead) {
			/* we need to insert our chunks before currentfreeEntry */
			break;
		}

		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	/* Appending at start of list ? */
	if (previousFreeEntry == NULL) {
		assume0(_heapFreeList == NULL || freeListTail < _heapFreeList);

		/* Do we need to coalesce ?*/
		if ((uint8_t*)freeListTail->afterEnd() == (uint8_t*)_heapFreeList) {
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(_heapFreeList->getSize());
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(freeListTail->getSize());
			freeListTail->expandSize(_heapFreeList->getSize());
			assume0((NULL == _heapFreeList->getNext(compressed)) || (freeListTail < _heapFreeList->getNext(compressed)));
			freeListTail->setNext(_heapFreeList->getNext(compressed), compressed);
			localFreeListMemoryCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(freeListTail->getSize());
		} else {
			assume0((NULL == _heapFreeList) || (freeListTail < _heapFreeList));
			freeListTail->setNext(_heapFreeList, compressed);
		}

		_heapFreeList = freeListHead;
	} else {
		assume0((NULL == previousFreeEntry->getNext(compressed)) || (freeListTail < previousFreeEntry->getNext(compressed)));
		freeListTail->setNext(previousFreeEntry->getNext(compressed), compressed);
		/* Do we need to coalesce ?*/
		if ((uint8_t*)previousFreeEntry->afterEnd() == (uint8_t*)freeListHead) {
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(freeListHead->getSize());
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(previousFreeEntry->getSize());
			previousFreeEntry->expandSize(freeListHead->getSize());
			assume0((NULL == freeListHead->getNext(compressed)) || (previousFreeEntry < freeListHead->getNext(compressed)));
			previousFreeEntry->setNext(freeListHead->getNext(compressed), compressed);
			localFreeListMemoryCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(previousFreeEntry->getSize());
		} else {
			assume0((NULL == freeListHead) || (previousFreeEntry < freeListHead));
			previousFreeEntry->setNext(freeListHead, compressed);
		}
	}

	/* Adjust the free memory data */
	_freeMemorySize += freeListMemorySize;
	_freeEntryCount += localFreeListMemoryCount;
}

#if defined(OMR_GC_LARGE_OBJECT_AREA)
/**
 * Remove all free entries in a specified range from pool and return to caller
 * as an address ordered list.
 *
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 * @param minimumSize Minimum size of free entry we are intrested in
 * @param retListHead Head of list of free entries being returned
 * @param retListTail Tail of list of free entries being returned
 * @param retListMemoryCount Number of free chunks on returned list
 * @param retListmemorySize Total size of all free chunks on returned list
 *
 * @return TRUE if at least one chunk in specified range found; FALSE otherwise
 */

bool
MM_MemoryPoolAddressOrderedList::removeFreeEntriesWithinRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress,
														uintptr_t minimumSize,
														 MM_HeapLinkedFreeHeader* &retListHead, MM_HeapLinkedFreeHeader* &retListTail,
														 uintptr_t &retListMemoryCount, uintptr_t &retListMemorySize)
{
	bool const compressed = compressObjectReferences();
	uintptr_t removeSize = 0;
	intptr_t removeCount = 0;
	uintptr_t trailingSize, leadingSize;

	void *currentFreeEntryTop, *baseAddr, *topAddr;
	MM_HeapLinkedFreeHeader *currentFreeEntry, *previousFreeEntry, *nextFreeEntry, *tailFreeEntry;

	retListHead = NULL;
	retListTail = NULL;
	retListMemoryCount = 0;
	retListMemorySize = 0;

	/* Find the first free entry, if any, within specified range */
	previousFreeEntry = NULL;
	currentFreeEntry = _heapFreeList;

	while(currentFreeEntry) {
		currentFreeEntryTop = (void *)currentFreeEntry->afterEnd();

		/* Does this chunk fall within range ? */
		if(currentFreeEntry >= lowAddress || currentFreeEntryTop > lowAddress) {
			break;
		}

		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	/* If we got to end of list or to an entry greater than high address */
	if ( currentFreeEntry == NULL || currentFreeEntry >= highAddress ) {
		/* ..no entries found within the specified range so we are done*/
		return false;
	}

	/* Remember the next free entry after the current one which we are going to consume at least part of */
	nextFreeEntry = currentFreeEntry->getNext(compressed);

	/* ...and top of current entry */
	currentFreeEntryTop = (void *)currentFreeEntry->afterEnd();

	/*Assume for now we will remove from pool all of this free chunk */
	removeSize = currentFreeEntry->getSize();
	removeCount ++;
	_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

	baseAddr = (void *)currentFreeEntry;
	topAddr = currentFreeEntryTop;

	/* Determine what to do with any leading bytes to the free entry that are not being returned */
	if(currentFreeEntry < (MM_HeapLinkedFreeHeader *)lowAddress) {
		/* Space at the head that is not being returned - is it a valid free entry? */
		if (createFreeEntry(env, currentFreeEntry, lowAddress, previousFreeEntry, NULL)) {
			leadingSize = ((uintptr_t)lowAddress) - ((uintptr_t)currentFreeEntry);
			if (NULL == previousFreeEntry) {
				_heapFreeList = currentFreeEntry;
			} else {
				assume0(previousFreeEntry < currentFreeEntry);
				previousFreeEntry->setNext(currentFreeEntry, compressed);
			}
			previousFreeEntry = currentFreeEntry;
			removeSize -= leadingSize;
			removeCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(leadingSize);
		}

		baseAddr = lowAddress;
	}

	/* Determine what to do with any trailing bytes to the free entry that are not returned to caller */
	if(currentFreeEntryTop > (MM_HeapLinkedFreeHeader *)highAddress) {
		/* Space at the tail that is not being returned - is it a valid free entry for this pool ? */
		if (createFreeEntry(env, highAddress, currentFreeEntryTop, previousFreeEntry, NULL)) {
			trailingSize = ((uintptr_t)currentFreeEntryTop) - ((uintptr_t)highAddress);
			if (NULL == previousFreeEntry) {
				_heapFreeList = (MM_HeapLinkedFreeHeader *)highAddress;
			} else {
				assume0(previousFreeEntry < highAddress);
				previousFreeEntry->setNext((MM_HeapLinkedFreeHeader *)highAddress, compressed);
			}

			previousFreeEntry = (MM_HeapLinkedFreeHeader *)highAddress;
			removeSize -= trailingSize;
			removeCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(trailingSize);
		}

		topAddr = highAddress;
	}


	/* Append whats left, if big enough to list to be returned. if not big enough it is abandoned */
	if (appendToList(env, baseAddr, topAddr, minimumSize, retListHead, retListTail)) {
		retListMemoryCount++;
		retListMemorySize += ((uint8_t*)topAddr - (uint8_t*) baseAddr);
	}

	currentFreeEntry = nextFreeEntry;
	tailFreeEntry = nextFreeEntry;

	/* Now append any whole chunks to list which fall within specified range */
	while (currentFreeEntry &&  (((uint8_t*)currentFreeEntry->afterEnd()) <= highAddress)) {
		tailFreeEntry = currentFreeEntry->getNext(compressed);

		if (appendToList(env, (void *)currentFreeEntry, (void *)currentFreeEntry->afterEnd(),
							 minimumSize, retListHead, retListTail)) {
			retListMemoryCount++;
			retListMemorySize += currentFreeEntry->getSize();
		}
		removeSize += currentFreeEntry->getSize();
		removeCount++;
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
		currentFreeEntry = tailFreeEntry;
	}

	/* Finally deal with any leading chunk which falls within specified range */
	if (currentFreeEntry && currentFreeEntry < highAddress) {
		/*Assume for now we will remove from pool all of this free chunk */
		removeSize += currentFreeEntry->getSize();
		removeCount++;
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
		tailFreeEntry = currentFreeEntry->getNext(compressed);

		currentFreeEntryTop = (void *)currentFreeEntry->afterEnd();
		/* Space at the tail that is not being returned - is it a valid free entry for this pool ? */
		if (createFreeEntry(env, highAddress, currentFreeEntryTop, previousFreeEntry, tailFreeEntry)) {
			trailingSize = ((uintptr_t)currentFreeEntryTop) - ((uintptr_t)highAddress);

			if (!previousFreeEntry) {
				_heapFreeList = (MM_HeapLinkedFreeHeader *)highAddress;
			}

			/* Adjust tail back to address retained portion */
			tailFreeEntry = (MM_HeapLinkedFreeHeader *)highAddress;
			removeSize -= trailingSize;
			removeCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(trailingSize);
		}

		if (appendToList(env, currentFreeEntry, highAddress, minimumSize, retListHead, retListTail)) {
			retListMemoryCount++;
			retListMemorySize += (uint8_t*)highAddress - (uint8_t*) currentFreeEntry;
		}
	}

	/* Attach the remaining tail to the previous entry, and assume the head of the list if there was none */
	if (previousFreeEntry) {
		assume0((NULL == tailFreeEntry) || (previousFreeEntry < tailFreeEntry));
		previousFreeEntry->setNext(tailFreeEntry, compressed);
	} else {
		_heapFreeList = tailFreeEntry;
	}

	/* Adjust the free memory data */
	_freeMemorySize -= removeSize;
	_freeEntryCount -= removeCount;

	return true;
}

/**
 * Determine the address in this memoryPool where there is at least sizeRequired free bytes in free entries.
 * The free entries must be of size minimumSize to be counted.
 *
 * @param sizeRequired Amount of free memory to be returned
 * @param minimumSize Minimum size of free entry we are intrested in
 *
 * @return address of memory location which has sizeRequired free bytes available in this memoryPool.
 * @return NULL if this memory pool can not satisfy sizeRequired bytes.
 */
void *
MM_MemoryPoolAddressOrderedList::findAddressAfterFreeSize(MM_EnvironmentBase *env, uintptr_t sizeRequired, uintptr_t minimumSize)
{
	bool const compressed = compressObjectReferences();
	uintptr_t remainingBytesNeeded = sizeRequired;
	MM_HeapLinkedFreeHeader *currentFreeEntry = _heapFreeList;

	/* Count full free entries until an entry needs to be split. */
	while (NULL != currentFreeEntry) {
		/* Removing entries has left us needing less than the minimum size of the entries.
		 * To ensure we get the size needed, bump up the remainingBytesNeeded.
		 */
		if (remainingBytesNeeded < minimumSize) {
			remainingBytesNeeded = minimumSize;
		}

		if (remainingBytesNeeded >= currentFreeEntry->getSize()) {
			/* Make sure the entry is big enough to include */
			if (minimumSize <= currentFreeEntry->getSize()) {
				remainingBytesNeeded -= currentFreeEntry->getSize();

				/* Did the last free entry satisfied required memory exactly? */
				if (0 == remainingBytesNeeded) {
					return (void *)currentFreeEntry->afterEnd();
				}
			}
		} else {
			/* This entry will be split */
			if (currentFreeEntry->getSize() - remainingBytesNeeded < _minimumFreeEntrySize) {
				/* The rest of the entry is too small to meet minimum size (for this pool), include it too */
				return (void *)currentFreeEntry->afterEnd();
			}
			return (void *)((uint8_t *)currentFreeEntry + remainingBytesNeeded);
		}

		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	return NULL;
}
#endif /* OMR_GC_LARGE_OBJECT_AREA */



bool
MM_MemoryPoolAddressOrderedList::recycleHeapChunk(
	void *addrBase,
	void *addrTop,
	MM_HeapLinkedFreeHeader *previousFreeEntry,
	MM_HeapLinkedFreeHeader *nextFreeEntry)
{
	bool const compressed = compressObjectReferences();
	Assert_MM_true(addrBase <= addrTop);
	Assert_MM_true((NULL == nextFreeEntry) || (addrTop <= nextFreeEntry));
#if defined(DEBUG)
	/* confirming there is no gap between previous free entry and addrBase */
	MM_HeapLinkedFreeHeader *current = NULL;
	if (previousFreeEntry) {
		current = previousFreeEntry->getNext(compressed);
	} else if (_heapFreeList) {
		current = _heapFreeList;
	}
	Assert_MM_true((current == NULL) || (((uintptr_t)current + current->getSize()) >= (uintptr_t)addrBase));
#endif
	if (internalRecycleHeapChunk(addrBase, addrTop, nextFreeEntry)) {
		if (previousFreeEntry) {
			Assert_MM_true(previousFreeEntry < addrBase);
			previousFreeEntry->setNext((MM_HeapLinkedFreeHeader*)addrBase, compressed);
		} else {
			_heapFreeList = (MM_HeapLinkedFreeHeader *)addrBase;
		}

		return true;
	}

	if (previousFreeEntry) {
		Assert_MM_true((NULL == nextFreeEntry) || (previousFreeEntry < nextFreeEntry));
		previousFreeEntry->setNext(nextFreeEntry, compressed);
	} else {
		_heapFreeList = nextFreeEntry;
	}

	return false;
}

/**
 * Find the free list entry whos end address matches the parameter.
 *
 * @param addr Address to match against the high end of a free entry.
 *
 * @return The leading address of the free entry whose top matches addr.
 */
void *
MM_MemoryPoolAddressOrderedList::findFreeEntryEndingAtAddr(MM_EnvironmentBase *env, void *addr)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader *currentFreeEntry;

	currentFreeEntry = _heapFreeList;
	while(currentFreeEntry) {
		if(((void *)currentFreeEntry->afterEnd()) == addr) {
			break;
		}
		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	return currentFreeEntry;
}

/**
 * @copydoc MM_MemoryPool::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *, MM_AllocateDescription *, void *, void *)
 */
uintptr_t
MM_MemoryPoolAddressOrderedList::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, void *lowAddr, void *highAddr)
{
	MM_HeapLinkedFreeHeader *lastFree;

	lastFree = (MM_HeapLinkedFreeHeader *)findFreeEntryEndingAtAddr(env, highAddr);

	/* If matching free entry found */
	if (NULL == lastFree) {
		/* No free entry with matching end address so return 0 */
		return 0;
	}

	uintptr_t availableContractSize = lastFree->getSize();

	/* Is the last free element a candidate to satisfy the allocation request ?
	 * If so we have to assume it is the only free element that could satisfy the
	 * request and adjust the  maximumContractSize.
	 */
	uintptr_t allocSize;
	if (NULL != allocDescription) {
		allocSize = allocDescription->getContiguousBytes();
	} else {
		allocSize = 0;
	}
	if ((allocSize != 0) && (allocSize <= availableContractSize)) {
		 availableContractSize -= allocSize;
	}

	return availableContractSize;
}


/**
 * Find the top of the free list entry whos start address matches the parameter.
 *
 * @param addr Address to match against the low end of a free entry.
 *
 * @return The trailing address of the free entry whos top matches addr.
 */
void *
MM_MemoryPoolAddressOrderedList::findFreeEntryTopStartingAtAddr(MM_EnvironmentBase *env, void *addr)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader *currentFreeEntry;

	currentFreeEntry = _heapFreeList;
	while(currentFreeEntry) {
		if((void *)currentFreeEntry == addr) {
			return (void *)currentFreeEntry->afterEnd();
		}
		/* Address ordered list - if the current entry is greater than the one we are looking for
		 * then there is no valid entry
		 */
		if((void *)currentFreeEntry > addr) {
			break;
		}

		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	return NULL;
}

/**
 * Find the address of the first entry on free list entry.
 *
 * @return The address of head of free chain
 */
void *
MM_MemoryPoolAddressOrderedList::getFirstFreeStartingAddr(MM_EnvironmentBase *env)
{
	return _heapFreeList;
}

/**
 * Find the address of the next entry on free list entry.
 *
 * @return The address of next free entry or NULL
 */
void *
MM_MemoryPoolAddressOrderedList::getNextFreeStartingAddr(MM_EnvironmentBase *env, void *currentFree)
{
	bool const compressed = compressObjectReferences();
	assume0(currentFree != NULL);
	return  ((MM_HeapLinkedFreeHeader *)currentFree)->getNext(compressed);
}

/**
 * Move a chunk of heap from one location to another within the receivers owned regions.
 * This involves fixing up any free list information that may change as a result of an address change.
 *
 * @param srcBase Start address to move.
 * @param srcTop End address to move.
 * @param dstBase Start of destination address to move into.
 *
 */
void
MM_MemoryPoolAddressOrderedList::moveHeap(MM_EnvironmentBase *env, void *srcBase, void *srcTop, void *dstBase)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader *currentFreeEntry, *previousFreeEntry;

	previousFreeEntry = NULL;
	currentFreeEntry = _heapFreeList;
	while(currentFreeEntry) {
		if(((void *)currentFreeEntry >= srcBase) && ((void *)currentFreeEntry < srcTop)) {
			MM_HeapLinkedFreeHeader *newFreeEntry;
			newFreeEntry = (MM_HeapLinkedFreeHeader *) ((((uintptr_t)currentFreeEntry) - ((uintptr_t)srcBase)) + ((uintptr_t)dstBase));

			if(previousFreeEntry) {
				assume0(previousFreeEntry < newFreeEntry);
				previousFreeEntry->setNext(newFreeEntry, compressed);
			} else {
				_heapFreeList = newFreeEntry;
			}
		}
		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext(compressed);
	}
}


/**
 * Lock any free list information from use.
 */
void
MM_MemoryPoolAddressOrderedList::lock(MM_EnvironmentBase *env)
{
	_heapLock.acquire();
}

/**
 * Unlock any free list information for use.
 */
void
MM_MemoryPoolAddressOrderedList::unlock(MM_EnvironmentBase *env)
{
	_heapLock.release();
}

/**
 * Insert/Coalesce Chunk into correct position on the free list.
 * @return true if recycle was successful, false if not.
 */
bool
MM_MemoryPoolAddressOrderedList::recycleHeapChunk(MM_EnvironmentBase *env, void *chunkBase, void *chunkTop)
{
	bool const compressed = compressObjectReferences();
	bool recycled = false;

	void *base = chunkBase;
	void *top = chunkTop;
	intptr_t freeEntryCount = 1;
	_heapLock.acquire();

	MM_HeapLinkedFreeHeader  *currentFreeEntry = _heapFreeList;
	MM_HeapLinkedFreeHeader  *nextFreeEntry = NULL;
	MM_HeapLinkedFreeHeader  *previousFreeEntry = NULL;
	MM_HeapLinkedFreeHeader  *next = NULL;
	MM_HeapLinkedFreeHeader  *prev = NULL;

	/* Find point chunk should be inserted and insert*/
	while (NULL != currentFreeEntry) {
		nextFreeEntry = currentFreeEntry->getNext(compressed);

		if ((NULL == nextFreeEntry) || ((void *)nextFreeEntry > chunkBase)) {
			break;
		}

		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = nextFreeEntry;
	}

	if ((NULL == currentFreeEntry) || (chunkBase < (void*)currentFreeEntry)) {
		prev = previousFreeEntry;
		if (chunkTop == (void *)currentFreeEntry) {
			top = (void *)((uintptr_t)top + currentFreeEntry->getSize());
			next = nextFreeEntry;
			freeEntryCount -= 1;
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
		} else {
			next = currentFreeEntry;
		}
	} else {
		if (chunkBase == (void *)((uintptr_t)currentFreeEntry + currentFreeEntry->getSize())) {
			base = (void *) currentFreeEntry;
			prev = previousFreeEntry;
			freeEntryCount -= 1;
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
		} else {
			prev = currentFreeEntry;
		}

		if (chunkTop == (void *)nextFreeEntry) {
			top = (void *)((uintptr_t)top + nextFreeEntry->getSize());
			next = nextFreeEntry->getNext(compressed);
			freeEntryCount -= 1;
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(nextFreeEntry->getSize());
		} else {
			next = nextFreeEntry;
		}
	}

	recycled = recycleHeapChunk(base, top, prev, next);
	Assert_MM_true(recycled);
	if ((NULL == prev) || (chunkTop != top)) {
		/* inserted freeEntry before _heapFreeList, it might confuse the checking for staled Hint, so clear hints for avoiding the cases.  */
		clearHints();
	}

	_largeObjectAllocateStats->incrementFreeEntrySizeClassStats((uintptr_t)top - (uintptr_t)base);
	_freeMemorySize += (uintptr_t)chunkTop - (uintptr_t)chunkBase;
	_freeEntryCount += freeEntryCount;

	_heapLock.release();

	return recycled;
}

/*
 * Debug routine to dump details of the current state of the freelist
 */
void
MM_MemoryPoolAddressOrderedList::printCurrentFreeList(MM_EnvironmentBase *env, const char *area)
{
	bool const compressed = compressObjectReferences();
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_HeapLinkedFreeHeader  *currentFreeEntry = _heapFreeList;

	omrtty_printf("Analysis of %s freelist: \n", area);

	while(currentFreeEntry) {
		omrtty_printf("Free chunk %p -> %p (%i) \n",
					currentFreeEntry,
					currentFreeEntry->afterEnd(),
					currentFreeEntry->getSize());
        currentFreeEntry = currentFreeEntry->getNext(compressed);
	}
}

#if defined(DEBUG)
/*
 * Verify that the free space statistics for this pool are correct.
 * @param largestFreValid Set if call is between collections as _largestFreEntry
 * is only expected to be valid immediately after sweep or compact as it is not
 * maintained post collection.
 * @note This routine can only be called immediately after completion of
 * sweep or compaction as _largestFreeEntry is not maintained between
 * collections.
 * @return TRUE is statistics valid; FALSE otherwise
 */
bool
MM_MemoryPoolAddressOrderedList::isMemoryPoolValid(MM_EnvironmentBase *env, bool postCollect)
{
	bool const compressed = compressObjectReferences();
	uintptr_t freeBytes = 0;
	uintptr_t freeCount = 0;
	uintptr_t largestFree = 0;

	MM_HeapLinkedFreeHeader  *currentFreeEntry = _heapFreeList;
	uintptr_t currentFreeEntrySize;
	while(currentFreeEntry) {
		currentFreeEntrySize = currentFreeEntry->getSize();

		freeBytes += currentFreeEntrySize;
		freeCount += 1;
		largestFree = OMR_MAX(largestFree, currentFreeEntrySize);

        currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	/* Do statistics match free list */
	if (freeBytes == _freeMemorySize &&
		freeCount == _freeEntryCount &&
		(postCollect || largestFree == _largestFreeEntry)) {
		return true;
	} else {
		return false;
	}
}

/*
 * Debug routine to return size of largest free entry currently on free list.
 * @return Size of largest free entry on free list
 */
uintptr_t
MM_MemoryPoolAddressOrderedList::getCurrentLargestFree(MM_EnvironmentBase *env)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader  *currentFreeEntry = _heapFreeList;
	uintptr_t largestFree = 0 ;
	while(currentFreeEntry) {
		largestFree = OMR_MAX(largestFree, currentFreeEntry->getSize());

        currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	return largestFree;
}


/*
 * Debug routine to return total all free entrys on free list
 * @return total amount of free space which should match _freeMemorySize
 */
uintptr_t
MM_MemoryPoolAddressOrderedList::getCurrentFreeMemorySize(MM_EnvironmentBase *env)
{
	bool const compressed = compressObjectReferences();
	MM_HeapLinkedFreeHeader  *currentFreeEntry = _heapFreeList;
	uintptr_t currentFree = 0 ;
	while(currentFreeEntry) {
		currentFree += currentFreeEntry->getSize();

        currentFreeEntry = currentFreeEntry->getNext(compressed);
	}

	assume0(currentFree == _freeMemorySize);

	return currentFree;
}

#endif /* DEBUG */

void 
MM_MemoryPoolAddressOrderedList::recalculateMemoryPoolStatistics(MM_EnvironmentBase *env)
{
	bool const compressed = compressObjectReferences();
	uintptr_t largestFreeEntry = 0;
	uintptr_t freeBytes = 0;
	uintptr_t freeEntryCount = 0;
	_largeObjectAllocateStats->getFreeEntrySizeClassStats()->resetCounts();

	MM_HeapLinkedFreeHeader *freeHeader = (MM_HeapLinkedFreeHeader *)getFirstFreeStartingAddr(env);
	while (NULL != freeHeader) {
		if (freeHeader->getSize() > largestFreeEntry) {
			largestFreeEntry = freeHeader->getSize();
		}
		freeBytes += freeHeader->getSize();
		freeEntryCount += 1;
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(freeHeader->getSize());
		freeHeader = freeHeader->getNext(compressed);
	}
	
	updateMemoryPoolStatistics(env, freeBytes, freeEntryCount, largestFreeEntry);
}

void
MM_MemoryPoolAddressOrderedList::appendCollectorLargeAllocateStats()
{
	_largeObjectCollectorAllocateStats = _largeObjectAllocateStats;
}

uintptr_t
MM_MemoryPoolAddressOrderedList::releaseFreeMemoryPages(MM_EnvironmentBase* env)
{
	uintptr_t releasedBytes = 0;
	_heapLock.acquire();
	releasedBytes = releaseFreeEntryMemoryPages(env, _heapFreeList);
	_heapLock.release();
	return releasedBytes;
}

MM_HeapLinkedFreeHeader *
MM_MemoryPoolAddressOrderedList::doFreeEntryCardAlignmentUpTo(MM_EnvironmentBase *env, MM_HeapLinkedFreeHeader *lastFreeEntryToAlign)
{
	MM_HeapLinkedFreeHeader *alignedLastFreeEntry = NULL;
	bool const compressed = env->compressObjectReferences();
	MM_HeapLinkedFreeHeader *currentFreeEntry = _firstCardUnalignedFreeEntry;
	MM_HeapLinkedFreeHeader *previousFreeEntry = (FREE_ENTRY_END == _prevCardUnalignedFreeEntry) ? NULL : _prevCardUnalignedFreeEntry;

	uintptr_t lostToAlignment = 0;

	uintptr_t freeBytes = _freeMemorySize;
	uintptr_t freeEntryCount = _freeEntryCount;
	while ((currentFreeEntry <= lastFreeEntryToAlign) && (NULL != currentFreeEntry)) {
		uintptr_t freeEntrySize = currentFreeEntry->getSize();
		void *endFreeEntry = (void *) ((uintptr_t)currentFreeEntry + freeEntrySize);
		void *newStartFreeEntry = (void*) MM_Math::roundToCeilingCard((uintptr_t) currentFreeEntry);
		void *newEndFreeEntry   = (void*) MM_Math::roundToFloorCard((uintptr_t) endFreeEntry);
		MM_HeapLinkedFreeHeader *nextFreeEntry = currentFreeEntry->getNext(compressed);

		if (((uintptr_t)currentFreeEntry != (uintptr_t)newStartFreeEntry) || ((uintptr_t)endFreeEntry != (uintptr_t)newEndFreeEntry)) {
			if (((uintptr_t)newEndFreeEntry - (uintptr_t)newStartFreeEntry) < _minimumFreeEntrySize) {
				/* remove currentFreeEntry */
				removeFromFreeList((void *)currentFreeEntry, endFreeEntry, previousFreeEntry, nextFreeEntry);
				removeHint(currentFreeEntry);
				lostToAlignment += freeEntrySize;
				freeEntryCount -= 1;
				freeEntrySize = 0;
				alignedLastFreeEntry = NULL;
			} else {
				if ((uintptr_t) currentFreeEntry != (uintptr_t) newStartFreeEntry) {
					fillWithHoles((void *)currentFreeEntry, newStartFreeEntry);
					updateHint(currentFreeEntry, (MM_HeapLinkedFreeHeader *)newStartFreeEntry);
				}
				if ((uintptr_t) endFreeEntry != (uintptr_t) newEndFreeEntry) {
					fillWithHoles(newEndFreeEntry, endFreeEntry);
				}
				recycleHeapChunk(newStartFreeEntry, newEndFreeEntry, previousFreeEntry, nextFreeEntry);
				previousFreeEntry = (MM_HeapLinkedFreeHeader *) newStartFreeEntry;
				lostToAlignment += freeEntrySize;
				freeEntrySize = (uintptr_t)newEndFreeEntry - (uintptr_t)newStartFreeEntry;
				lostToAlignment -= freeEntrySize;
				alignedLastFreeEntry = (MM_HeapLinkedFreeHeader *) newStartFreeEntry;
			}
		} else {
			alignedLastFreeEntry = currentFreeEntry;
			previousFreeEntry = currentFreeEntry;
		}
		currentFreeEntry = nextFreeEntry;
	}

	/* update freeBytes, freeCount, darkmatter */
	if (lostToAlignment > 0) {
		/* largestFreeEntry might not be accurate any more, largestFreeEntry would not be used for this case anyway */
		if (lostToAlignment >= _adjustedBytesForCardAlignment) {
			_adjustedBytesForCardAlignment = 0;
		} else {
			_adjustedBytesForCardAlignment -= lostToAlignment;
		}
		freeBytes -= lostToAlignment;
		_freeMemorySize = freeBytes;
		_freeEntryCount = freeEntryCount;
		_darkMatterBytes += lostToAlignment;
	}

	/* update _firstCardUnalignedFreeEntry and _prevCardUnalignedFreeEntry */
	_firstCardUnalignedFreeEntry = (NULL == currentFreeEntry) ? FREE_ENTRY_END : currentFreeEntry;
	if (FREE_ENTRY_END != _firstCardUnalignedFreeEntry) {
		_prevCardUnalignedFreeEntry = (NULL == previousFreeEntry) ? FREE_ENTRY_END : previousFreeEntry;
	} else {
		_prevCardUnalignedFreeEntry = FREE_ENTRY_END;
	}

	return alignedLastFreeEntry;
}

void
MM_MemoryPoolAddressOrderedList::setParallelGCAlignment(MM_EnvironmentBase *env, bool alignmentEnabled)
{
	if (alignmentEnabled) {
		_parallelGCAlignmentBase = _memorySubSpace->getFirstRegion()->getLowAddress();
		_parallelGCAlignmentSize = _extensions->parSweepChunkSize;
	} else {
		_parallelGCAlignmentBase = NULL;
		_parallelGCAlignmentSize = 0;
	}
}

void
MM_MemoryPoolAddressOrderedList::setSubSpace(MM_MemorySubSpace *memorySubSpace)
{
	if (MEMORY_TYPE_OLD == memorySubSpace->getTypeFlags()) {
		/* Move the registration and the hook in higher level entities. They know exactly when they create the OLD subspace/pool. */
		J9HookInterface **mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);
		(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_SATB_TOGGLED, concurrentSATBToggled, OMR_GET_CALLSITE(), (void *)this);
	}

	MM_MemoryPool::setSubSpace(memorySubSpace);
}
