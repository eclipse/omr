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

#include "omrcfg.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include <string.h>

#include "MemoryPoolSplitAddressOrderedList.hpp"

#include "AllocateDescription.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepHeapSectioning.hpp"
#include "SweepPoolState.hpp"

/**
 * Create and initialize a new instance of the receiver.
 */
MM_MemoryPoolSplitAddressOrderedList*
MM_MemoryPoolSplitAddressOrderedList::newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit)
{
	return newInstance(env, minimumFreeEntrySize, maxSplit, "Unknown");
}

/**
 * Create and initialize a new instance of the receiver.
 */
MM_MemoryPoolSplitAddressOrderedList*
MM_MemoryPoolSplitAddressOrderedList::newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit, const char* name)
{
	MM_MemoryPoolSplitAddressOrderedList* memoryPool;

	memoryPool = (MM_MemoryPoolSplitAddressOrderedList*)env->getForge()->allocate(sizeof(MM_MemoryPoolSplitAddressOrderedList), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memoryPool) {
		memoryPool = new (memoryPool) MM_MemoryPoolSplitAddressOrderedList(env, minimumFreeEntrySize, maxSplit, name);
		if (!memoryPool->initialize(env)) {
			memoryPool->kill(env);
			memoryPool = NULL;
		}
	}
	return memoryPool;
}

/****************************************
 * Allocation
 ****************************************
 */

MMINLINE MM_HeapLinkedFreeHeader*
MM_MemoryPoolSplitAddressOrderedList::internalAllocateFromList(MM_EnvironmentBase* env, uintptr_t sizeInBytesRequired, uintptr_t curFreeList, MM_HeapLinkedFreeHeader** previousFreeEntry, uintptr_t* largestFreeEntry)
{
	uintptr_t walkCountCurrentList = 0;
	J9ModronAllocateHint* allocateHintUsed = NULL;
	MM_HeapLinkedFreeHeader* candidateHintEntry = NULL;
	uintptr_t candidateHintSize = 0;
	uintptr_t currentFreeEntrySize = 0;

	MM_HeapLinkedFreeHeader* currentFreeEntry = _heapFreeLists[curFreeList]._freeList;
	*previousFreeEntry = NULL;

	/* Large object - use a hint if it is available */
	allocateHintUsed = _heapFreeLists[curFreeList].findHint(sizeInBytesRequired);
	if (allocateHintUsed) {
		currentFreeEntry = allocateHintUsed->heapFreeHeader;
		candidateHintSize = allocateHintUsed->size;
		Assert_MM_true(currentFreeEntry->getSize() <= allocateHintUsed->size);
		Assert_MM_true(currentFreeEntry->getSize() < sizeInBytesRequired);
	}

	while (NULL != currentFreeEntry) {
		currentFreeEntrySize = currentFreeEntry->getSize();
		/* while we are walking, keep track of the largest free entry.
		 * We will need this in the case of allocation failure to update
		 * the pool's largest free */
		if (currentFreeEntrySize > *largestFreeEntry) {
			*largestFreeEntry = currentFreeEntrySize;
		}

		if (sizeInBytesRequired <= currentFreeEntrySize) {
			/* in first pass, we ignore reserved free entry */
			if (!isPreviousReservedFreeEntry(*previousFreeEntry, curFreeList)) {

				if (((walkCountCurrentList >= J9MODRON_ALLOCATION_MANAGER_HINT_MAX_WALK) || ((walkCountCurrentList > 1) && allocateHintUsed))) {
					_heapFreeLists[curFreeList].addHint(candidateHintEntry, candidateHintSize);
				}

				break;
			}
		}

		if (!isPreviousReservedFreeEntry(*previousFreeEntry, curFreeList)) {
			/* we should never update hint with reserved entry */
			if (candidateHintSize < currentFreeEntrySize) {
				candidateHintSize = currentFreeEntrySize;
			}
			candidateHintEntry = currentFreeEntry;
		}

		walkCountCurrentList += 1;

		*previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext();
		Assert_MM_true((NULL == currentFreeEntry) || (currentFreeEntry > *previousFreeEntry));
	}
	
	_allocSearchCount += walkCountCurrentList;
	
	return currentFreeEntry;
}
 
 
void*
MM_MemoryPoolSplitAddressOrderedList::internalAllocate(MM_EnvironmentBase* env, uintptr_t sizeInBytesRequired, bool lockingRequired, MM_LargeObjectAllocateStats* largeObjectAllocateStatsForFreeList)
{
	MM_HeapLinkedFreeHeader* currentFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* previousFreeEntry = NULL;
	uintptr_t largestFreeEntry = 0;
	uintptr_t suggestedFreeList = 0;
	uintptr_t curFreeList = 0;
	MM_HeapLinkedFreeHeader* recycleEntry = NULL;
	uintptr_t recycleEntrySize = 0;
	void* addrBase = NULL;

	/* first pass iterating if skipReserved = true */
	bool skipReserved = true;
retry:

	bool firstIteration = true;
	bool jumpedToSuggested = false;
	if (skipReserved) {
		curFreeList = _currentThreadFreeList[env->getEnvironmentId() % _heapFreeListCount];
	} else {
		/* tried all lists and the only thing to try is reserved free entry */
		curFreeList = _reservedFreeListIndex;
		if (curFreeList == _heapFreeListCount) {
			goto skipSearch;
		}
	}
	suggestedFreeList = curFreeList;

	do {
		if (NULL != _heapFreeLists[curFreeList]._freeList) {
			if (lockingRequired) {
				_heapFreeLists[curFreeList]._lock.acquire();
				_heapFreeLists[curFreeList]._timesLocked += 1;
			}

			if (skipReserved) {
				/* first pass will skip reserved free entry */
				currentFreeEntry = internalAllocateFromList(env, sizeInBytesRequired, curFreeList, &previousFreeEntry, &largestFreeEntry);
				if (NULL != currentFreeEntry) {
					/* found a freeEntry; will release lock only after we handle the remainder */
					break;
				}
			} else {
				/* second pass will directly use reserved free entry */
				if (sizeInBytesRequired <= _reservedFreeEntrySize) {
					Assert_MM_true(_reservedFreeEntryAvaliable);
					currentFreeEntry = getReservedFreeEntry();
					previousFreeEntry = _previousReservedFreeEntry;
					break;
				}
			}
			Assert_MM_true(NULL == currentFreeEntry);

			if (lockingRequired) {
				_heapFreeLists[curFreeList]._lock.release();
			}
		}

		jumpedToSuggested = false;
		if (firstIteration) {
			firstIteration = false;
			suggestedFreeList = findGoodStartFreeList();
			curFreeList = suggestedFreeList;
			jumpedToSuggested = true;
		} else {
			curFreeList = (curFreeList + 1) % _heapFreeListCount;
		}
	} while ((jumpedToSuggested || (curFreeList != suggestedFreeList)) && skipReserved);

skipSearch:
	/* Check if an entry was found */
	if (NULL == currentFreeEntry) {
		if (skipReserved && (sizeInBytesRequired <= _reservedFreeEntrySize)) {
			skipReserved = false;
			goto retry;
		}
#if defined(OMR_GC_CONCURRENT_SWEEP)
		if (_memorySubSpace->replenishPoolForAllocate(env, this, sizeInBytesRequired)) {
			skipReserved = true;
			goto retry;
		}
#endif /* OMR_GC_CONCURRENT_SWEEP) */
		/* Since we failed to allocate, update the largest free entry so that outside callers will be able to skip this pool, next time, in Tarok configurations */
		setLargestFreeEntry(largestFreeEntry);
		return NULL;
	}

	/* Check if this free entry looks like a dead object. */
	Assert_MM_true(env->getExtensions()->objectModel.isDeadObject((omrobjectptr_t)currentFreeEntry));

	/* Adjust the free memory size */
	Assert_MM_true(_heapFreeLists[curFreeList]._freeSize >= sizeInBytesRequired);
	_heapFreeLists[curFreeList]._freeSize -= sizeInBytesRequired;
	_largeObjectAllocateStatsForFreeList[curFreeList].decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

	/* Update allocation statistics */
	_allocCount += 1;
	_allocBytes += sizeInBytesRequired;

	/* Determine what to do with the recycled portion of the free entry */
	recycleEntrySize = currentFreeEntry->getSize() - sizeInBytesRequired;

	addrBase = (void*)currentFreeEntry;
	recycleEntry = (MM_HeapLinkedFreeHeader*)(((uint8_t*)currentFreeEntry) + sizeInBytesRequired);

	if (recycleHeapChunk(env, recycleEntry, ((uint8_t*)recycleEntry) + recycleEntrySize, previousFreeEntry, currentFreeEntry->getNext(), curFreeList)) {
		if (!skipReserved && isPreviousReservedFreeEntry(previousFreeEntry, curFreeList)) {
			_reservedFreeEntrySize = recycleEntrySize;
		} else if (currentFreeEntry == _previousReservedFreeEntry) {
			Assert_MM_true(curFreeList == _reservedFreeListIndex);
			_previousReservedFreeEntry = recycleEntry;
		}
		_heapFreeLists[curFreeList].updateHint(currentFreeEntry, recycleEntry);
		_largeObjectAllocateStatsForFreeList[curFreeList].incrementFreeEntrySizeClassStats(recycleEntrySize);
	} else {
		if (!skipReserved && isPreviousReservedFreeEntry(previousFreeEntry, curFreeList)) {
			resetReservedFreeEntry();
		} else if (currentFreeEntry == _previousReservedFreeEntry) {
			Assert_MM_true(curFreeList == _reservedFreeListIndex);
			_previousReservedFreeEntry = previousFreeEntry;
		}

		/* Adjust the free memory size and count */
		Assert_MM_true(_heapFreeLists[curFreeList]._freeSize >= recycleEntrySize);
		Assert_MM_true(_heapFreeLists[curFreeList]._freeCount > 0);
		_heapFreeLists[curFreeList]._freeSize -= recycleEntrySize;
		_heapFreeLists[curFreeList]._freeCount -= 1;

		/* Update discard bytes if necessary */
		_allocDiscardedBytes += recycleEntrySize;

		/* Removed from the free list - Kill the hint if necessary */
		_heapFreeLists[curFreeList].removeHint(currentFreeEntry);
	}

	/* Was our initial or suggested freelist empty? If not, go back and use it more. */
	if (NULL != _heapFreeLists[suggestedFreeList]._freeList) {
		_currentThreadFreeList[env->getEnvironmentId() % _heapFreeListCount] = suggestedFreeList;
	}

	/* Collector object allocate stats for Survivor are not interesting (_largeObjectCollectorAllocateStatsForFreeList is null for Survivor) */
	if (NULL != largeObjectAllocateStatsForFreeList) {
		largeObjectAllocateStatsForFreeList[curFreeList].allocateObject(sizeInBytesRequired);
	}

	if (lockingRequired) {
		_heapFreeLists[curFreeList]._lock.release();
	}

	Assert_MM_true(NULL != addrBase);

	return addrBase;
}

bool
MM_MemoryPoolSplitAddressOrderedList::internalAllocateTLH(MM_EnvironmentBase* env, uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop, bool lockingRequired, MM_LargeObjectAllocateStats* largeObjectAllocateStatsForFreeList)
{
	uintptr_t freeEntrySize = 0;
	void* topOfRecycledChunk = NULL;
	MM_HeapLinkedFreeHeader* entryNext = NULL;
	MM_HeapLinkedFreeHeader* freeEntry = NULL;
	MM_HeapLinkedFreeHeader* previousFreeEntry = NULL;
	uintptr_t consumedSize = 0;
	uintptr_t recycleEntrySize = 0;
	uintptr_t suggestedFreeList;
	uintptr_t curFreeList;

	/* first pass iterating if skipReserved = true */
	bool skipReserved = true;
retry:

	bool firstIteration = true;
	bool jumpedToSuggested = false;


	if (skipReserved) {
		curFreeList = _currentThreadFreeList[env->getEnvironmentId() % _heapFreeListCount];
	} else {
		/* tried all lists and the only thing to try is reserved free entry */
		curFreeList = _reservedFreeListIndex;
		if (curFreeList == _heapFreeListCount) {
			goto skipSearch;
		}
	}
	suggestedFreeList = curFreeList;

	do {
		if (NULL != _heapFreeLists[curFreeList]._freeList) {
			if (lockingRequired) {
				_heapFreeLists[curFreeList]._lock.acquire();
				_heapFreeLists[curFreeList]._timesLocked += 1;
			}

			if (!skipReserved) {
				/* second pass will directly use reserved free entry */
				freeEntry = getReservedFreeEntry();
				if (NULL != freeEntry) {
					previousFreeEntry = _previousReservedFreeEntry;
				}
			} else {
				freeEntry = _heapFreeLists[curFreeList]._freeList;
			}
			
			if (NULL != freeEntry) {
				freeEntrySize = freeEntry->getSize();

				if (skipReserved && isPreviousReservedFreeEntry(previousFreeEntry, curFreeList)) {
					previousFreeEntry = freeEntry;
					freeEntry = freeEntry->getNext();
					if (NULL != freeEntry) {
						freeEntrySize = freeEntry->getSize();
						break;
					}
					previousFreeEntry = NULL;
				} else {
					break;
				}
			}

			if (lockingRequired) {
				_heapFreeLists[curFreeList]._lock.release();
			}
		}

		jumpedToSuggested = false;
		if (firstIteration) {
			firstIteration = false;
			suggestedFreeList = findGoodStartFreeList();
			curFreeList = suggestedFreeList;
			jumpedToSuggested = true;
		} else {
			curFreeList = (curFreeList + 1) % _heapFreeListCount;
		}
	} while ((jumpedToSuggested || (suggestedFreeList != curFreeList)) && skipReserved);

skipSearch:
	/* Check if an entry was found */
	if (NULL == freeEntry) {
		if (skipReserved && (0 != _reservedFreeEntrySize)) {
			skipReserved = false;
			goto retry;
		}
#if defined(OMR_GC_CONCURRENT_SWEEP)
		if (_memorySubSpace->replenishPoolForAllocate(env, this, _minimumFreeEntrySize)) {
			skipReserved = true;
			goto retry;
		}
#endif /* OMR_GC_CONCURRENT_SWEEP */
		/* if we failed to allocate a TLH, this pool is either full or so heavily fragmented that it is effectively full */
		setLargestFreeEntry(0);
		return false;
	}

	/* Check if this free entry looks like a dead object. */
	Assert_MM_true(env->getExtensions()->objectModel.isDeadObject((omrobjectptr_t)freeEntry));

	/* Update our current free list */
	_currentThreadFreeList[env->getEnvironmentId() % _heapFreeListCount] = curFreeList;

	/* Consume the bytes and set the return pointer values */
	Assert_MM_true(freeEntrySize >= _minimumFreeEntrySize);
	consumedSize = (maximumSizeInBytesRequired > freeEntrySize) ? freeEntrySize : maximumSizeInBytesRequired;
	_largeObjectAllocateStatsForFreeList[curFreeList].decrementFreeEntrySizeClassStats(freeEntrySize);

	/* If the leftover chunk is smaller than the minimum size, hand it out */
	recycleEntrySize = freeEntrySize - consumedSize;
	if (recycleEntrySize && (recycleEntrySize < _minimumFreeEntrySize)) {
		consumedSize += recycleEntrySize;
		recycleEntrySize = 0;
	}

	/* Adjust the free memory size */
	Assert_MM_true(_heapFreeLists[curFreeList]._freeSize >= consumedSize);
	_heapFreeLists[curFreeList]._freeSize -= consumedSize;

	_allocCount += 1;
	_allocBytes += consumedSize;
	/* Collector TLH allocate stats for Survivor are not interesting (_largeObjectCollectorAllocateStatsForFreeList is null for Survivor) */
	if (NULL != largeObjectAllocateStatsForFreeList) {
		largeObjectAllocateStatsForFreeList[curFreeList].incrementTlhAllocSizeClassStats(consumedSize);
	}

	addrBase = (void*)freeEntry;
	addrTop = (void*)(((uint8_t*)addrBase) + consumedSize);

	topOfRecycledChunk = ((uint8_t*)addrTop) + recycleEntrySize;
	entryNext = freeEntry->getNext();

	/* Recycle the remaining entry back onto the free list (if applicable) */
	if (!recycleHeapChunk(env, addrTop, topOfRecycledChunk, previousFreeEntry, entryNext, curFreeList)) {
		/* Adjust the free memory size and count */
		Assert_MM_true(_heapFreeLists[curFreeList]._freeSize >= recycleEntrySize);
		Assert_MM_true(_heapFreeLists[curFreeList]._freeCount > 0);
		_heapFreeLists[curFreeList]._freeSize -= recycleEntrySize;
		_heapFreeLists[curFreeList]._freeCount -= 1;

		if (!skipReserved && isPreviousReservedFreeEntry(previousFreeEntry, curFreeList)) {
			resetReservedFreeEntry();
		} else if (freeEntry == _previousReservedFreeEntry) {
			Assert_MM_true(curFreeList == _reservedFreeListIndex);
			_previousReservedFreeEntry = previousFreeEntry;
		}
		_allocDiscardedBytes += recycleEntrySize;
		_heapFreeLists[curFreeList].removeHint(freeEntry);
	} else {
		if (!skipReserved && isPreviousReservedFreeEntry(previousFreeEntry, curFreeList)) {
			_reservedFreeEntrySize = recycleEntrySize;
		} else if (freeEntry == _previousReservedFreeEntry) {
			Assert_MM_true(curFreeList == _reservedFreeListIndex);
			_previousReservedFreeEntry = (MM_HeapLinkedFreeHeader*) addrTop;
		}
		_heapFreeLists[curFreeList].updateHint(freeEntry, (MM_HeapLinkedFreeHeader*)addrTop);
		_largeObjectAllocateStatsForFreeList[curFreeList].incrementFreeEntrySizeClassStats(recycleEntrySize);
	}

	if (lockingRequired) {
		_heapFreeLists[curFreeList]._lock.release();
	}

	return true;
}

/****************************************
 * Free list building
 ****************************************
 */

void
MM_MemoryPoolSplitAddressOrderedList::reset(Cause cause)
{
	/* Call superclass first .. */
	MM_MemoryPoolSplitAddressOrderedListBase::reset(cause);
	resetReservedFreeEntry();
}

void
MM_MemoryPoolSplitAddressOrderedList::postProcess(MM_EnvironmentBase* env, Cause cause)
{
	uintptr_t lastFreeListIndex = _heapFreeListCount - 1;
	if (cause == forCompact && (lastFreeListIndex != 0)) {
		/* Move all the compact items to the beginning of the lists */
		_heapFreeLists[0]._freeList = _heapFreeLists[lastFreeListIndex]._freeList;
		_heapFreeLists[0]._freeCount = _heapFreeLists[lastFreeListIndex]._freeCount;
		_heapFreeLists[0]._freeSize = _heapFreeLists[lastFreeListIndex]._freeSize;
		_heapFreeLists[lastFreeListIndex]._freeList = NULL;
		_heapFreeLists[lastFreeListIndex]._freeCount = 0;
		_heapFreeLists[lastFreeListIndex]._freeSize = 0;
	}

	if (!_heapFreeLists[0]._freeList) {
		return;
	}

	/* Free list splitting by equal memory size */
	uintptr_t freeListSplitSize = _heapFreeLists[0]._freeSize / _heapFreeListCount;
	uintptr_t currentFreeListIndex = 0;

	resetReservedFreeEntry();

	if (cause == forSweep) {

		_heapFreeLists[0]._freeSize = _sweepPoolState->_sweepFreeBytes;
		_heapFreeLists[0]._freeCount = _sweepPoolState->_sweepFreeHoles;
		freeListSplitSize = _heapFreeLists[0]._freeSize / _heapFreeListCount;

		/* Free list splitting at sweep chunk granularity.
		 * Faster but less precise splits compared to free list entry granularity.
		 */
		if (0 < _sweepPoolState->_largestFreeEntry) {
			MM_HeapLinkedFreeHeader* largestFreeEntry;
			if (NULL == _sweepPoolState->_previousLargestFreeEntry) {
				largestFreeEntry = _heapFreeLists[0]._freeList;
			} else {
				largestFreeEntry = _sweepPoolState->_previousLargestFreeEntry->getNext();
			}
			Assert_MM_true(_sweepPoolState->_largestFreeEntry == largestFreeEntry->getSize());
		}
		MM_GCExtensionsBase* extensions = env->getExtensions();
		MM_ParallelSweepChunk* chunk = NULL;
		MM_SweepHeapSectioningIterator sectioningIterator(extensions->sweepHeapSectioning);
		uintptr_t freeSize = _heapFreeLists[0]._freeSize;
		uintptr_t freeCount = _heapFreeLists[0]._freeCount;
		uintptr_t accumulatedFreeSize = 0;
		uintptr_t accumulatedFreeHoles = 0;

		_reservedFreeEntrySize = _sweepPoolState->_largestFreeEntry;

		/* Iterate over all the sweep chunks to find split candidates. */
		uintptr_t processedChunkCount = 0;
		for (chunk = sectioningIterator.nextChunk(); NULL != chunk; chunk = sectioningIterator.nextChunk()) {
			if (processedChunkCount >= extensions->splitFreeListNumberChunksPrepared) {
				/* We have processed all sweep chunks. */
				break;
			}
			++processedChunkCount;
			if ((currentFreeListIndex + 1) >= _heapFreeListCount) {
				/* We have filled up all the free lists. */
				break;
			}
			if ((this != chunk->memoryPool) || (NULL == chunk->_splitCandidate)) {
				/* This chunk is not a split candidate for this pool. */
				continue;
			}

			/* Check if we want to split here. */
			uintptr_t currentFreeListSize = chunk->_accumulatedFreeSize - accumulatedFreeSize;
			if (currentFreeListSize >= freeListSplitSize) {
				/* Split here. */
				/* Fill in the size and holes of the current free list. */
				_heapFreeLists[currentFreeListIndex]._freeCount = chunk->_accumulatedFreeHoles - accumulatedFreeHoles;
				_heapFreeLists[currentFreeListIndex]._freeSize = currentFreeListSize;

				/* Terminate the tail of the current free list. */
				chunk->_splitCandidatePreviousEntry->setNext(NULL);
				/**
				 * Identify previous reserved entry from the previous LargestFreeEntry which is set during sweep
				 * if previousLargestFreeEntry == splitCandidatePreviousEntry, it means largestFreeEntry == splitCandidateEntry and the largestFreeEntry is in the next free list
				 * if previousLargestFreeEntry < splitCandidatePreviousEntry, it means the largestFreeEntry is in current freeList
				 */
				if ((_heapFreeListCount == _reservedFreeListIndex) && (_sweepPoolState->_previousLargestFreeEntry <= chunk->_splitCandidatePreviousEntry)) {
					if (_sweepPoolState->_previousLargestFreeEntry == chunk->_splitCandidatePreviousEntry) {
						_reservedFreeListIndex = currentFreeListIndex + 1;
						_previousReservedFreeEntry = NULL;
					} else {
						_reservedFreeListIndex = currentFreeListIndex;
						_previousReservedFreeEntry = _sweepPoolState->_previousLargestFreeEntry;
					}
					_reservedFreeEntryAvaliable = true;
				}

				/* Set the head of the new free list. */
				currentFreeListIndex += 1;
				_heapFreeLists[currentFreeListIndex]._freeList = chunk->_splitCandidate;

				/* Update our accumulated stats. */
				accumulatedFreeSize = chunk->_accumulatedFreeSize;
				accumulatedFreeHoles = chunk->_accumulatedFreeHoles;
			}
		}

		if (_heapFreeListCount == _reservedFreeListIndex) {
			_reservedFreeListIndex = currentFreeListIndex;
			_previousReservedFreeEntry = _sweepPoolState->_previousLargestFreeEntry;
			_reservedFreeEntryAvaliable = true;
		}
		/* Update the current free list stats. It gets the leftovers. */
		_heapFreeLists[currentFreeListIndex]._freeSize = freeSize - accumulatedFreeSize;
		_heapFreeLists[currentFreeListIndex]._freeCount = freeCount - accumulatedFreeHoles;
	} else {
		/* Free list splitting at free list entry granularity.
		 * Slower but necessary when you don't have valid sweep chunks.
		 */
		MM_HeapLinkedFreeHeader* previousFreeList = NULL;
		MM_HeapLinkedFreeHeader* currentFreeList = _heapFreeLists[0]._freeList;

		_heapFreeLists[0]._freeCount = 0;
		_heapFreeLists[0]._freeSize = 0;

		while (NULL != currentFreeList) {
			_heapFreeLists[currentFreeListIndex]._freeSize += currentFreeList->getSize();
			_heapFreeLists[currentFreeListIndex]._freeCount += 1;

			if (currentFreeList->getSize() > _reservedFreeEntrySize) {
				_reservedFreeEntrySize = currentFreeList->getSize();
				_reservedFreeListIndex = currentFreeListIndex;
				_previousReservedFreeEntry = previousFreeList;
				_reservedFreeEntryAvaliable = true;
			}

			previousFreeList = currentFreeList;
			currentFreeList = currentFreeList->getNext();

			if ((_heapFreeLists[currentFreeListIndex]._freeSize >= freeListSplitSize) && (currentFreeListIndex < lastFreeListIndex)) {
				previousFreeList->setNext(NULL);
				previousFreeList = NULL;
				currentFreeListIndex += 1;
				_heapFreeLists[currentFreeListIndex]._freeList = currentFreeList;
				_heapFreeLists[currentFreeListIndex]._freeSize = 0;
				_heapFreeLists[currentFreeListIndex]._freeCount = 0;
			}
		}
	}

	/* reset thread starting positions */
	for (currentFreeListIndex = 0; currentFreeListIndex < _heapFreeListCount; ++currentFreeListIndex) {
		_currentThreadFreeList[currentFreeListIndex] = currentFreeListIndex;
	}

#if 0
	Assert_MM_true(printFreeListValidity(env));
#endif
	Assert_MM_true(_reservedFreeEntryAvaliable);
	MM_HeapLinkedFreeHeader* largestFreeEntry =NULL;
	if (0 < _reservedFreeEntrySize) {
		if (NULL == _previousReservedFreeEntry) {
			largestFreeEntry = _heapFreeLists[_reservedFreeListIndex]._freeList;
		} else {
			largestFreeEntry = _previousReservedFreeEntry->getNext();
		} 
		Assert_MM_true(_reservedFreeEntrySize == largestFreeEntry->getSize());
	}
}

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
MM_MemoryPoolSplitAddressOrderedList::expandWithRange(MM_EnvironmentBase* env, uintptr_t expandSize, void* lowAddress, void* highAddress, bool canCoalesce)
{
	MM_HeapLinkedFreeHeader* previousFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* nextFreeEntry = NULL;

	if (0 == expandSize) {
		return;
	}

	/* Handle the entries that are too small to make the free list */
	if (expandSize < _minimumFreeEntrySize) {
		abandonHeapChunk(lowAddress, highAddress);
		return;
	}

	MM_HeapLinkedFreeHeader** head = NULL;
	uintptr_t curFreeListIndex = 0;
	for (curFreeListIndex = 0; curFreeListIndex < _heapFreeListCount; ++curFreeListIndex) {
		head = &_heapFreeLists[curFreeListIndex]._freeList;
		/* Find the free entries in the list the appear before/after the range being added */
		previousFreeEntry = NULL;
		nextFreeEntry = *head;
		while (nextFreeEntry) {
			if (lowAddress < nextFreeEntry) {
				break;
			}
			previousFreeEntry = nextFreeEntry;
			nextFreeEntry = nextFreeEntry->getNext();
		}

		/* Check if the range can be coalesced with either the surrounding free entries */
		if (canCoalesce) {
			/* Check if the range can be fused to the tail previous free entry */
			if (previousFreeEntry && (lowAddress == (void*)(((uintptr_t)previousFreeEntry) + previousFreeEntry->getSize()))) {

				uintptr_t expandedSize = expandSize + previousFreeEntry->getSize();
				/* if 1) we are upsizing reservedFreeEntry (in which case the address of the entry does not change) or
				 *    2) if its size is larger than size of reservedFreeEntry, but only if the entry has not been identified yet than
				 *  update the size of reservedFreeEntry
				 */
				if (isCurrentReservedFreeEntry(previousFreeEntry, curFreeListIndex)) {
					_reservedFreeEntrySize = expandedSize;
				}

				_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(previousFreeEntry->getSize());
				previousFreeEntry->expandSize(expandSize);

				if (previousFreeEntry->getSize() > _largestFreeEntry) {
					_largestFreeEntry = previousFreeEntry->getSize();
				}

				/* Update the free list information */
				_heapFreeLists[curFreeListIndex]._freeSize += expandSize;
				_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(previousFreeEntry->getSize());

				assume0(isMemoryPoolValid(env, true));
				return;
			}
			/* Check if the range can be fused to the head of the next free entry */
			if (nextFreeEntry && (highAddress == (void*)nextFreeEntry)) {
				MM_HeapLinkedFreeHeader* newFreeEntry = (MM_HeapLinkedFreeHeader*)lowAddress;
				assume0((NULL == nextFreeEntry->getNext()) || (newFreeEntry < nextFreeEntry->getNext()));
				
				uintptr_t expandedSize = expandSize + nextFreeEntry->getSize();
				/* if 1) we are upsizing reservedFreeEntry or
				 *    2) expanded size is larger than size of reservedFreeEntry, but only if the entry has not been identified yet
				 * update the size of reservedFreeEntry
				 * previousReservedFreeEntry remains unchanged
				 */
				if (isCurrentReservedFreeEntry(nextFreeEntry, curFreeListIndex)) {
					_reservedFreeEntrySize = expandedSize;
				}

				_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(nextFreeEntry->getSize());
	
				newFreeEntry->setNext(nextFreeEntry->getNext());
				newFreeEntry->setSize(expandedSize);

				if (newFreeEntry->getSize() > _largestFreeEntry) {
					_largestFreeEntry = newFreeEntry->getSize();
				}

				/* The previous free entry next pointer must be updated */
				if (previousFreeEntry) {
					assume0(newFreeEntry > previousFreeEntry);
					previousFreeEntry->setNext(newFreeEntry);
				} else {
					*head = newFreeEntry;
				}

				/* Update the free list information */
				_heapFreeLists[curFreeListIndex]._freeSize += expandSize;
				_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(newFreeEntry->getSize());

				assume0(isMemoryPoolValid(env, true));
				return;
			}
		} else {
			break;
		}
	}

	/* Check to see if we have fallen off the end of the free lists. */
	if (curFreeListIndex == _heapFreeListCount) {
		/* We have so we should still be expanding on the last free list. */
		--curFreeListIndex;
	}

	/* No coalescing available - build a free entry from the range that will be inserted into the list */
	MM_HeapLinkedFreeHeader* freeEntry;

	freeEntry = (MM_HeapLinkedFreeHeader*)lowAddress;
	assume0((NULL == nextFreeEntry) || (nextFreeEntry > freeEntry));
	freeEntry->setNext(nextFreeEntry);
	freeEntry->setSize(expandSize);

	/* Insert the free entry into the list (the next entry was handled above) */
	if (previousFreeEntry) {
		assume0(previousFreeEntry < freeEntry);
		previousFreeEntry->setNext(freeEntry);
	} else {
		*head = freeEntry;
	}

	/* Update the free list information */
	_heapFreeLists[curFreeListIndex]._freeSize += expandSize;
	_heapFreeLists[curFreeListIndex]._freeCount += 1;
	_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(expandSize);

	if (freeEntry->getSize() > _largestFreeEntry) {
		_largestFreeEntry = freeEntry->getSize();
	}

	assume0(isMemoryPoolValid(env, true));
	Assert_GC_true_with_message2(env, reservedFreeEntryConsistencyCheck(), "expandWithRange _previousReservedFreeEntry=%p, _reservedFreeEntrySize=%zu\n", _previousReservedFreeEntry, _reservedFreeEntrySize);
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
void*
MM_MemoryPoolSplitAddressOrderedList::contractWithRange(MM_EnvironmentBase* env, uintptr_t contractSize, void* lowAddress, void* highAddress)
{
	MM_HeapLinkedFreeHeader* currentFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* currentFreeEntryTop = NULL;
	MM_HeapLinkedFreeHeader* previousFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* nextFreeEntry = NULL;

	if (0 == contractSize) {
		return NULL;
	}

	/* Find the free entry that encompasses the range to contract */
	/* TODO: Could we use hints to find a better starting address?  Are hints still valid? */
	uintptr_t freeListIndex;
	for (freeListIndex = 0; freeListIndex < _heapFreeListCount; ++freeListIndex) {
		previousFreeEntry = NULL;
		currentFreeEntry = _heapFreeLists[freeListIndex]._freeList;
		while (currentFreeEntry) {
			if ((lowAddress >= currentFreeEntry) && (highAddress <= (void*)(((uintptr_t)currentFreeEntry) + currentFreeEntry->getSize()))) {
				break;
			}
			previousFreeEntry = currentFreeEntry;
			currentFreeEntry = currentFreeEntry->getNext();
		}
		if (NULL != currentFreeEntry) {
			break;
		}
	}

	Assert_MM_true(NULL != currentFreeEntry); /* Can't contract what doesn't exist */
	Assert_MM_true(currentFreeEntry->getSize() >= contractSize);

	uintptr_t totalContractSize = contractSize;
	uintptr_t contractCount = 1;
	_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

	MM_HeapLinkedFreeHeader* reservedFreeEntry = getReservedFreeEntry();
	if (NULL != reservedFreeEntry) {
		if ((reservedFreeEntry >= lowAddress) && (((void*)reservedFreeEntry->afterEnd()) <= highAddress)) {
			reservedFreeEntry = NULL;
			resetReservedFreeEntry();
		}
	}

	/* Record the next free entry after the current one which we are going to contract */
	nextFreeEntry = currentFreeEntry->getNext();

	/* Glue any new free entries that have been split from the original free entry into the free list */
	/* Start back to front, attaching the current entry to the next entry, and the current entry taking
	 * the place of the next entry */

	/* Determine what to do with any trailing bytes to the free entry that are not being contracted. */
	currentFreeEntryTop = (MM_HeapLinkedFreeHeader*)(((uintptr_t)currentFreeEntry) + currentFreeEntry->getSize());
	if (currentFreeEntryTop != (MM_HeapLinkedFreeHeader*)highAddress) {
		/* Space at the tail that is not being contracted - is it a valid free entry? */
		if (createFreeEntry(env, highAddress, currentFreeEntryTop, NULL, nextFreeEntry)) {
			/* The entry is a free list candidate */
			nextFreeEntry = (MM_HeapLinkedFreeHeader*)highAddress;
			contractCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(nextFreeEntry->getSize());
			if (reservedFreeEntry == currentFreeEntry) {
				_reservedFreeEntrySize = nextFreeEntry->getSize();
			}
		} else {
			/* The entry was not large enough for the free list */
			uintptr_t trailingSize = ((uintptr_t)currentFreeEntryTop) - ((uintptr_t)highAddress);
			totalContractSize += trailingSize;
			if (reservedFreeEntry == currentFreeEntry) {
				reservedFreeEntry = NULL;
				resetReservedFreeEntry();
			}
		}
	}

	/* Determine what to do with any leading bytes to the free entry that are not being contracted. */
	if (currentFreeEntry != (MM_HeapLinkedFreeHeader*)lowAddress) {
		if (createFreeEntry(env, currentFreeEntry, lowAddress, NULL, nextFreeEntry)) {
			nextFreeEntry = currentFreeEntry;
			contractCount--;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

			if (reservedFreeEntry == currentFreeEntry) {
				_reservedFreeEntrySize = currentFreeEntry->getSize();
			}
		} else {
			/* Abandon the leading entry - it is too small to be on the free list */
			uintptr_t leadingSize = ((uintptr_t)lowAddress) - ((uintptr_t)currentFreeEntry);
			totalContractSize += leadingSize;
			if (reservedFreeEntry == currentFreeEntry) {
				reservedFreeEntry = NULL;
				resetReservedFreeEntry();
			}
		}
	}

	/* Attach our findings to the previous entry, and assume the head of the list if there was none */
	if (previousFreeEntry) {
		assume0((NULL == nextFreeEntry) || (previousFreeEntry < nextFreeEntry));
		previousFreeEntry->setNext(nextFreeEntry);
	} else {
		_heapFreeLists[freeListIndex]._freeList = nextFreeEntry;
	}

	/* Adjust the free memory data */
	Assert_MM_true(_heapFreeLists[freeListIndex]._freeSize >= totalContractSize);
	_heapFreeLists[freeListIndex]._freeSize -= totalContractSize;
	_heapFreeLists[freeListIndex]._freeCount -= contractCount;

	assume0(isMemoryPoolValid(env, true));

	Assert_GC_true_with_message2(env, reservedFreeEntryConsistencyCheck(), "contractWithRange _previousReservedFreeEntry=%p, _reservedFreeEntrySize=%zu\n", _previousReservedFreeEntry, _reservedFreeEntrySize);
	return lowAddress;
}

/**
 * Add contents of an address ordered list of all free entries to this pool
 *
 * @param freeListHead Head of list of free entries to be added
 * @param freeListTail Tail of list of free entries to be added
 * @param freeListMemoryCount Number of free entries in the list
 * @param freeListMemorySize Total size of free entries in the list
 *
 */
void
MM_MemoryPoolSplitAddressOrderedList::addFreeEntries(MM_EnvironmentBase* env,
													 MM_HeapLinkedFreeHeader*& freeListHead, MM_HeapLinkedFreeHeader*& freeListTail,
													 uintptr_t freeListMemoryCount, uintptr_t freeListMemorySize)
{

	uintptr_t localFreeListMemoryCount = freeListMemoryCount;

	MM_HeapLinkedFreeHeader* freeEntryToAdd = freeListHead;
	while (freeEntryToAdd != NULL) {
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(freeEntryToAdd->getSize());
		freeEntryToAdd = freeEntryToAdd->getNext();
	}

	/* Find the first free entry, if any, within specified range */
	MM_HeapLinkedFreeHeader* previousFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* currentFreeEntry = NULL;
	uintptr_t currentFreeListIndex = 0;
	uintptr_t previousFreeListIndex = 0;
	currentFreeEntry = (MM_HeapLinkedFreeHeader*)getFirstFreeStartingAddr(env, &currentFreeListIndex);
	previousFreeListIndex = currentFreeListIndex;

	while (NULL != currentFreeEntry) {
		if (currentFreeEntry > freeListHead) {
			/* we need to insert our chunks before currentfreeEntry */
			break;
		}

		previousFreeEntry = currentFreeEntry;
		previousFreeListIndex = currentFreeListIndex;
		currentFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &currentFreeListIndex);
		/* Check a free list boundary */
		if ((NULL == previousFreeEntry->getNext()) && (currentFreeListIndex != _heapFreeListCount)) {
			previousFreeEntry = NULL;
		}
	}

	/* Check to see if we have fallen off the end of the free lists. */
	if (currentFreeListIndex == _heapFreeListCount) {
		/* We have so we should still be expanding on the last free list. */
		--currentFreeListIndex;
	}

	if (previousFreeEntry == NULL) {
		/* Appending at start of a list? */
		Assert_MM_true(currentFreeEntry == NULL || freeListTail < currentFreeEntry);
		Assert_MM_true(currentFreeEntry == _heapFreeLists[currentFreeListIndex]._freeList);

		/* Do we need to coalesce ?*/
		if ((uint8_t*)freeListTail->afterEnd() == (uint8_t*)currentFreeEntry) {
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(freeListTail->getSize());

			uintptr_t expandedSize = freeListTail->getSize() + currentFreeEntry->getSize();
			if (isPreviousReservedFreeEntry(previousFreeEntry, currentFreeListIndex)) {
				Assert_MM_true(currentFreeEntry->getSize() == _reservedFreeEntrySize);
				_reservedFreeEntrySize = expandedSize;

				if (freeListTail != freeListHead) {
					/* need to update _previousReservedFreeEntry to previous of freeListTail */
					MM_HeapLinkedFreeHeader* previous = freeListHead->getNext();
					while (previous->getNext() != freeListTail) {
						previous = previous->getNext();
					}
					_previousReservedFreeEntry = previous;
				}
			}

			freeListTail->expandSize(currentFreeEntry->getSize());
			freeListTail->setNext(currentFreeEntry->getNext());

			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(freeListTail->getSize());
			localFreeListMemoryCount--;
		} else {
			Assert_MM_true((NULL == currentFreeEntry) || (freeListTail < currentFreeEntry));
			freeListTail->setNext(currentFreeEntry);
		}

		_heapFreeLists[currentFreeListIndex]._freeList = freeListHead;
		_heapFreeLists[currentFreeListIndex]._freeSize += freeListMemorySize;
		_heapFreeLists[currentFreeListIndex]._freeCount += localFreeListMemoryCount;
	} else {
		freeListTail->setNext(previousFreeEntry->getNext());

		/* Do we need to coalesce ?*/
		if ((uint8_t*)previousFreeEntry->afterEnd() == (uint8_t*)freeListHead) {
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(freeListHead->getSize());
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(previousFreeEntry->getSize());

			uintptr_t expandedSize = freeListHead->getSize() + previousFreeEntry->getSize();
			if (isCurrentReservedFreeEntry(previousFreeEntry, previousFreeListIndex)) {
				_reservedFreeEntrySize = expandedSize;
			}

			previousFreeEntry->expandSize(freeListHead->getSize());
			Assert_MM_true((NULL == freeListHead->getNext()) || (previousFreeEntry < freeListHead->getNext()));
			previousFreeEntry->setNext(freeListHead->getNext());
			localFreeListMemoryCount -= 1;

			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(previousFreeEntry->getSize());
		} else {
			Assert_MM_true((NULL == freeListHead) || (previousFreeEntry < freeListHead));
			previousFreeEntry->setNext(freeListHead);
		}
		_heapFreeLists[previousFreeListIndex]._freeSize += freeListMemorySize;
		_heapFreeLists[previousFreeListIndex]._freeCount += localFreeListMemoryCount;
	}

	Assert_GC_true_with_message2(env, reservedFreeEntryConsistencyCheck(), "addFreeEntries _previousReservedFreeEntry=%p, _reservedFreeEntrySize=%zu\n", _previousReservedFreeEntry, _reservedFreeEntrySize);
}

/**
 * Remove all free entries in a specified range from pool and return to caller
 * as an address ordered list.
 *
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 * @param minimumSize Minimum size of free entry we are interested in
 * @param retListHead Head of list of free entries being returned
 * @param retListTail Tail of list of free entries being returned
 * @param retListMemoryCount Number of free chunks on returned list
 * @param retListmemorySize Total size of all free chunks on returned list
 *
 * @return TRUE if at least one chunk in specified range found; FALSE otherwise
 */
bool
MM_MemoryPoolSplitAddressOrderedList::removeFreeEntriesWithinRange(MM_EnvironmentBase* env, void* lowAddress, void* highAddress,
																   uintptr_t minimumSize,
																   MM_HeapLinkedFreeHeader*& retListHead, MM_HeapLinkedFreeHeader*& retListTail,
																   uintptr_t& retListMemoryCount, uintptr_t& retListMemorySize)
{
	void* currentFreeEntryTop = NULL;
	void* baseAddr = NULL;
	void* topAddr = NULL;
	MM_HeapLinkedFreeHeader* currentFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* previousFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* nextFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* tailFreeEntry = NULL;

	retListHead = NULL;
	retListTail = NULL;
	retListMemoryCount = 0;
	retListMemorySize = 0;

	/* Find the first free entry, if any, within specified range */
	uintptr_t currentFreeListIndex;
	previousFreeEntry = NULL;
	currentFreeEntry = (MM_HeapLinkedFreeHeader*)getFirstFreeStartingAddr(env, &currentFreeListIndex);
	while (currentFreeEntry) {
		currentFreeEntryTop = (void*)currentFreeEntry->afterEnd();

		/* Does this chunk fall within range ? */
		if (currentFreeEntry >= lowAddress || currentFreeEntryTop > lowAddress) {
			break;
		}

		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &currentFreeListIndex);
		if (NULL == previousFreeEntry->getNext()) {
			previousFreeEntry = NULL;
		}
	}


	/* If we got to the end of the free list or to an entry greater than the high address */
	if (currentFreeEntry == NULL || currentFreeEntry >= highAddress) {
		/* ...no entries found within the specified range so we are done */
		return false;
	}

	MM_HeapLinkedFreeHeader* reservedFreeEntry = getReservedFreeEntry();
	if (NULL != reservedFreeEntry) {
		if ((reservedFreeEntry >= lowAddress) && (((void*)reservedFreeEntry->afterEnd()) <= highAddress)) {
			reservedFreeEntry = NULL;
			resetReservedFreeEntry();
		}
	}
	/* Remember the next free entry after the current one which we are going to consume at least part of */
	uintptr_t nextFreeListIndex = currentFreeListIndex;
	nextFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &nextFreeListIndex);

	/* Assume for now we will remove this entire free entry from the pool */
	Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeSize >= currentFreeEntry->getSize());
	Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeCount > 0);
	_heapFreeLists[currentFreeListIndex]._freeSize -= currentFreeEntry->getSize();
	--_heapFreeLists[currentFreeListIndex]._freeCount;
	_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

	baseAddr = (void*)currentFreeEntry;
	topAddr = currentFreeEntryTop;

	/* Determine what to do with any leading bytes of the free entry that are not being returned */
	if (currentFreeEntry < (MM_HeapLinkedFreeHeader*)lowAddress) {
		/* Space at the head that is not being returned - is it a valid free entry? */
		if (createFreeEntry(env, currentFreeEntry, lowAddress, previousFreeEntry, NULL)) {
			uintptr_t leadingSize = ((uintptr_t)lowAddress) - ((uintptr_t)currentFreeEntry);
			if (reservedFreeEntry == currentFreeEntry) {
				_reservedFreeEntrySize = leadingSize;
			}
			previousFreeEntry = currentFreeEntry;
			_heapFreeLists[currentFreeListIndex]._freeSize += leadingSize;
			++_heapFreeLists[currentFreeListIndex]._freeCount;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(leadingSize);
		} else {
			/* reminder is not big enough for a free entry */
			if (reservedFreeEntry == currentFreeEntry) {
				reservedFreeEntry = NULL;
				resetReservedFreeEntry();
			}
			if (currentFreeListIndex == nextFreeListIndex) {
				if (NULL == previousFreeEntry) {
					_heapFreeLists[currentFreeListIndex]._freeList = nextFreeEntry;
				} else {
					previousFreeEntry->setNext(nextFreeEntry);
				}
			} else {
				if (NULL == previousFreeEntry) {
					_heapFreeLists[currentFreeListIndex]._freeList = NULL;
				} else {
					previousFreeEntry->setNext(NULL);
				}
			}
		}
		baseAddr = lowAddress;
	}

	/* Determine what to do with any trailing bytes to the free entry that are not returned to caller */
	if (currentFreeEntryTop > (MM_HeapLinkedFreeHeader*)highAddress) {
		/* Space at the tail that is not being returned - is it a valid free entry for this pool ? */
		if (createFreeEntry(env, highAddress, currentFreeEntryTop, previousFreeEntry, NULL)) {
			uintptr_t trailingSize = ((uintptr_t)currentFreeEntryTop) - ((uintptr_t)highAddress);
			if (NULL == previousFreeEntry) {
				_heapFreeLists[currentFreeListIndex]._freeList = (MM_HeapLinkedFreeHeader*)highAddress;
			} else {
				Assert_MM_true(previousFreeEntry < highAddress);
				previousFreeEntry->setNext((MM_HeapLinkedFreeHeader*)highAddress);
			}

			if (reservedFreeEntry == currentFreeEntry) {
				_reservedFreeEntrySize = trailingSize;
				_previousReservedFreeEntry = NULL;
			} else if (_previousReservedFreeEntry == currentFreeEntry) {
				_previousReservedFreeEntry = (MM_HeapLinkedFreeHeader*)highAddress;
			}
			previousFreeEntry = (MM_HeapLinkedFreeHeader*)highAddress;
			_heapFreeLists[currentFreeListIndex]._freeSize += trailingSize;
			++_heapFreeLists[currentFreeListIndex]._freeCount;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(trailingSize);
		} else {
			/* reminder is not big enough for a free entry */
			if (reservedFreeEntry == currentFreeEntry) {
				reservedFreeEntry = NULL;
				resetReservedFreeEntry();
			} else if (_previousReservedFreeEntry == currentFreeEntry) {
				_previousReservedFreeEntry = NULL;
			}
		}
		topAddr = highAddress;
	}

	/* Append what's left if it's big enough to go on the list */
	if (appendToList(env, baseAddr, topAddr, minimumSize, retListHead, retListTail)) {
		++retListMemoryCount;
		retListMemorySize += ((uint8_t*)topAddr - (uint8_t*)baseAddr);
	}

	uintptr_t previousFreeListIndex = currentFreeListIndex;
	currentFreeEntry = nextFreeEntry;
	currentFreeListIndex = nextFreeListIndex;
	tailFreeEntry = nextFreeEntry;

	/* Now append any whole chunks to the list which fall within the specified range */
	while (currentFreeEntry && (((uint8_t*)currentFreeEntry->afterEnd()) <= highAddress)) {
		Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeSize >= currentFreeEntry->getSize());
		Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeCount > 0);
		_heapFreeLists[currentFreeListIndex]._freeSize -= currentFreeEntry->getSize();
		--_heapFreeLists[currentFreeListIndex]._freeCount;
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

		if (_previousReservedFreeEntry == currentFreeEntry) {
			_previousReservedFreeEntry = NULL;
		}

		tailFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &currentFreeListIndex);

		if (appendToList(env, (void*)currentFreeEntry, (void*)currentFreeEntry->afterEnd(),
						 minimumSize, retListHead, retListTail)) {
			++retListMemoryCount;
			retListMemorySize += currentFreeEntry->getSize();
		}
		currentFreeEntry = tailFreeEntry;
	}

	/* Finally deal with any leading chunk which falls within specified range */
	if ((NULL != currentFreeEntry) && (currentFreeEntry < highAddress)) {
		/* Assume for now we will remove this entire free entry from the pool */
		Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeSize >= currentFreeEntry->getSize());
		Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeCount > 0);
		_heapFreeLists[currentFreeListIndex]._freeSize -= currentFreeEntry->getSize();
		_heapFreeLists[currentFreeListIndex]._freeCount -= 1;
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

		tailFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &currentFreeListIndex);

		currentFreeEntryTop = (void*)currentFreeEntry->afterEnd();
		/* Space at the tail that is not being returned - is it a valid free entry for this pool ? */
		if (createFreeEntry(env, highAddress, currentFreeEntryTop, previousFreeEntry, tailFreeEntry)) {
			uintptr_t trailingSize = ((uintptr_t)currentFreeEntryTop) - ((uintptr_t)highAddress);

			if (reservedFreeEntry == currentFreeEntry) {
				_reservedFreeEntrySize = trailingSize;
				_previousReservedFreeEntry = NULL;
			} else if (_previousReservedFreeEntry == currentFreeEntry) {
				_previousReservedFreeEntry = (MM_HeapLinkedFreeHeader*)highAddress;
			}
			/* Make the tail the newly created free entry */
			tailFreeEntry = (MM_HeapLinkedFreeHeader*)highAddress;
			_heapFreeLists[previousFreeListIndex]._freeSize += trailingSize;
			_heapFreeLists[previousFreeListIndex]._freeCount += 1;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(trailingSize);
		} else {
			/* reminder is not big enough for a free entry */
			if (reservedFreeEntry == currentFreeEntry) {
				reservedFreeEntry = NULL;
				resetReservedFreeEntry();
			} else if (_previousReservedFreeEntry == currentFreeEntry) {
				_previousReservedFreeEntry = NULL;
			}
		}

		if (appendToList(env, currentFreeEntry, highAddress, minimumSize, retListHead, retListTail)) {
			retListMemoryCount += 1;
			retListMemorySize += (uint8_t*)highAddress - (uint8_t*)currentFreeEntry;
		}
	}

	/* Attach the remaining tail to the previous entry, and set the head of the list if there was none */
	if (previousFreeEntry) {
		Assert_MM_true((NULL == tailFreeEntry) || (previousFreeEntry < tailFreeEntry));
		previousFreeEntry->setNext(tailFreeEntry);
	} else {
		_heapFreeLists[previousFreeListIndex]._freeList = tailFreeEntry;
	}

	/* Fix up size and count because we merged two lists */
	if ((previousFreeListIndex != currentFreeListIndex) && (currentFreeListIndex < _heapFreeListCount)) {
		_heapFreeLists[previousFreeListIndex]._freeCount += _heapFreeLists[currentFreeListIndex]._freeCount;
		_heapFreeLists[previousFreeListIndex]._freeSize += _heapFreeLists[currentFreeListIndex]._freeSize;
	}

	/* Kill all newly empty free lists */
	for (uintptr_t i = previousFreeListIndex + 1; (i <= currentFreeListIndex) && (i < _heapFreeListCount); ++i) {
		_heapFreeLists[i]._freeList = NULL;
		_heapFreeLists[i]._freeCount = 0;
		_heapFreeLists[i]._freeSize = 0;
		_heapFreeLists[i].clearHints();
	}

	Assert_GC_true_with_message2(env, reservedFreeEntryConsistencyCheck(), "removeFreeEntriesWithinRange _previousReservedFreeEntry=%p, _reservedFreeEntrySize=%zu\n", _previousReservedFreeEntry, _reservedFreeEntrySize);
	return true;
}
