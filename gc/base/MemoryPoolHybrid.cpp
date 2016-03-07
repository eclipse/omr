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

#include "MemoryPoolHybrid.hpp"

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
MM_MemoryPoolHybrid*
MM_MemoryPoolHybrid::newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit)
{
	return newInstance(env, minimumFreeEntrySize, maxSplit, "Unknown");
}

/**
 * Create and initialize a new instance of the receiver.
 */
MM_MemoryPoolHybrid*
MM_MemoryPoolHybrid::newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit, const char* name)
{
	MM_MemoryPoolHybrid* memoryPool;

	memoryPool = (MM_MemoryPoolHybrid*)env->getForge()->allocate(sizeof(MM_MemoryPoolHybrid), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memoryPool) {
		memoryPool = new (memoryPool) MM_MemoryPoolHybrid(env, minimumFreeEntrySize, maxSplit, name);
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
MM_MemoryPoolHybrid::internalAllocateFromList(MM_EnvironmentBase* env, uintptr_t sizeInBytesRequired, uintptr_t curFreeList, MM_HeapLinkedFreeHeader** previousFreeEntry, uintptr_t* largestFreeEntry)
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
			if (((walkCountCurrentList >= J9MODRON_ALLOCATION_MANAGER_HINT_MAX_WALK) || ((walkCountCurrentList > 1) && allocateHintUsed))) {
				_heapFreeLists[curFreeList].addHint(candidateHintEntry, candidateHintSize);
			}

			break;
		}

		if (candidateHintSize < currentFreeEntrySize) {
			candidateHintSize = currentFreeEntrySize;
		}
		candidateHintEntry = currentFreeEntry;

		walkCountCurrentList += 1;

		*previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext();
		Assert_MM_true((NULL == currentFreeEntry) || (currentFreeEntry > *previousFreeEntry));
	}

	_allocSearchCount += walkCountCurrentList;

	return currentFreeEntry;
}


void*
MM_MemoryPoolHybrid::internalAllocate(MM_EnvironmentBase* env, uintptr_t sizeInBytesRequired, bool lockingRequired, MM_LargeObjectAllocateStats* largeObjectAllocateStatsForFreeList)
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
		/* tried all lists and the only thing to try is reserved free list */
		curFreeList = _heapFreeListCount;
	}
	suggestedFreeList = curFreeList;

	do {
		if (NULL != _heapFreeLists[curFreeList]._freeList) {
			if (lockingRequired) {
				_heapFreeLists[curFreeList]._lock.acquire();
				_heapFreeLists[curFreeList]._timesLocked += 1;
			}

			currentFreeEntry = internalAllocateFromList(env, sizeInBytesRequired, curFreeList, &previousFreeEntry, &largestFreeEntry);
			if (NULL != currentFreeEntry) {
				/* found a freeEntry; will release lock only after we handle the remainder */
				break;
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

	/* Check if an entry was found */
	if (NULL == currentFreeEntry) {
		if (skipReserved) {
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
		_heapFreeLists[curFreeList].updateHint(currentFreeEntry, recycleEntry);
		_largeObjectAllocateStatsForFreeList[curFreeList].incrementFreeEntrySizeClassStats(recycleEntrySize);
	} else {
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
MM_MemoryPoolHybrid::internalAllocateTLH(MM_EnvironmentBase* env, uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop, bool lockingRequired, MM_LargeObjectAllocateStats* largeObjectAllocateStatsForFreeList)
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
		/* tried all lists and the only thing to try is reserved free list */
		curFreeList = _heapFreeListCount;
	}
	suggestedFreeList = curFreeList;

	do {
		if (NULL != _heapFreeLists[curFreeList]._freeList) {
			if (lockingRequired) {
				_heapFreeLists[curFreeList]._lock.acquire();
				_heapFreeLists[curFreeList]._timesLocked += 1;
			}

			freeEntry = _heapFreeLists[curFreeList]._freeList;

			if (NULL != freeEntry) {
				freeEntrySize = freeEntry->getSize();
				break;
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

	/* Check if an entry was found */
	if (NULL == freeEntry) {
		if (skipReserved) {
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

		_allocDiscardedBytes += recycleEntrySize;
		_heapFreeLists[curFreeList].removeHint(freeEntry);
	} else {
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
MMINLINE void
MM_MemoryPoolHybrid::appendToReservedFreeList(MM_EnvironmentBase* env, void* address, uintptr_t size)
{
	MM_HeapLinkedFreeHeader* freeListTail = getReservedFreeList()->_freeList;
	while ((NULL != freeListTail) && (NULL != freeListTail->getNext())) {
		freeListTail = freeListTail->getNext();
	}

	connectInnerMemoryToPool(env, (MM_HeapLinkedFreeHeader*)address, size, (MM_HeapLinkedFreeHeader*)freeListTail);

	if (NULL == freeListTail) {
		getReservedFreeList()->_freeList = (MM_HeapLinkedFreeHeader*)address;
	}
	getReservedFreeList()->_freeSize += size;
	getReservedFreeList()->_freeCount += 1;
}

void
MM_MemoryPoolHybrid::appendToFreeList(MM_EnvironmentBase* env, J9ModronFreeList* freeList, MM_HeapLinkedFreeHeader* freeListTail, MM_HeapLinkedFreeHeader* newFreeEntry)
{
	setNextForFreeEntryInFreeList(freeList, freeListTail, newFreeEntry);
	newFreeEntry->setNext(NULL);
	uintptr_t size = newFreeEntry->getSize();
	freeList->_freeSize += size;
	freeList->_freeCount += 1;
	_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(size);
}

MMINLINE void
MM_MemoryPoolHybrid::appendToFreeList(MM_EnvironmentBase* env, J9ModronFreeList* freeList, MM_HeapLinkedFreeHeader* freeListTail, void* lowAddress, uintptr_t size)
{
	createFreeEntry(env, lowAddress, (uint8_t*)lowAddress+size);
	appendToFreeList(env, freeList, freeListTail, (MM_HeapLinkedFreeHeader*)lowAddress);
}

uintptr_t
MM_MemoryPoolHybrid::coalesceExpandRangeWithLastFreeEntry(MM_HeapLinkedFreeHeader* lastFreeEntry, J9ModronFreeList* freelist, void* lowAddress, uintptr_t expandSize)
{
	uintptr_t expandedSize = expandSize;
	if (NULL == lastFreeEntry) {
		return expandedSize;
	}
	uintptr_t size = lastFreeEntry->getSize();

	/* Check if the range can be fused to the tail previous free entry */
	if (lowAddress == (void*)(((uintptr_t)lastFreeEntry) + size)) {
		expandedSize += size;
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(size);
		lastFreeEntry->setSize(expandedSize);

		if (expandedSize > _largestFreeEntry) {
			_largestFreeEntry = expandedSize;
		}

		/* Update the free list information */
		freelist->_freeSize += expandSize;
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(expandedSize);
	}
	return expandedSize;
}

uintptr_t
MM_MemoryPoolHybrid::coalesceNewFreeEntryWithLastFreeEntry(MM_HeapLinkedFreeHeader* lastFreeEntry, J9ModronFreeList* freelist, MM_HeapLinkedFreeHeader* newFreeEntry)
{
	uintptr_t expandedSize = newFreeEntry->getSize();
	if (NULL == lastFreeEntry) {
		return expandedSize;
	}
	uintptr_t size = lastFreeEntry->getSize();

	/* Do we need to coalesce ?*/
	if ((uint8_t*)lastFreeEntry->afterEnd() == (uint8_t*)newFreeEntry) {
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(size);
		freelist->_freeSize += expandedSize;
		expandedSize += size;
		_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(expandedSize);
	}
	return expandedSize;
}

void
MM_MemoryPoolHybrid::moveLastFreeEntryToReservedFreeList(MM_EnvironmentBase* env, J9ModronFreeList* fromFreeList, MM_HeapLinkedFreeHeader* previousFreeEntry, MM_HeapLinkedFreeHeader* freeEntry, J9ModronFreeList* toFreeList, MM_HeapLinkedFreeHeader* toFreeListTail)
{
	Assert_MM_true(NULL == freeEntry->getNext());
	setNextForFreeEntryInFreeList(fromFreeList, previousFreeEntry, NULL);
	
	fromFreeList->_freeSize -= freeEntry->getSize();
	fromFreeList->_freeCount -= 1;

	setNextForFreeEntryInFreeList(toFreeList, toFreeListTail, freeEntry);
	
	toFreeList->_freeSize += freeEntry->getSize();
	toFreeList->_freeCount += 1;
}

bool
MM_MemoryPoolHybrid::tryContractWithRangeInFreelist(MM_EnvironmentBase* env, J9ModronFreeList* freeList, uintptr_t contractSize, void* lowAddress, void* highAddress)
{
	bool ret = false;
	MM_HeapLinkedFreeHeader* previousFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* currentFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* nextFreeEntry = NULL;

	if (NULL != freeList->_freeList) {
		currentFreeEntry = freeList->_freeList;
		previousFreeEntry = NULL;
		while (NULL != (nextFreeEntry = currentFreeEntry->getNext())) {
			previousFreeEntry = currentFreeEntry;
			currentFreeEntry = nextFreeEntry;
		}

		if ((currentFreeEntry + currentFreeEntry->getSize()) == highAddress) {
			uintptr_t totalContractSize = contractSize;
			uintptr_t contractCount = 1;

			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
			Assert_MM_true((MM_HeapLinkedFreeHeader*)lowAddress < currentFreeEntry);
			if (currentFreeEntry < (MM_HeapLinkedFreeHeader*)lowAddress) {
				if (createFreeEntry(env, currentFreeEntry, lowAddress, NULL, nextFreeEntry)) {
					contractCount -= 1;
					_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(currentFreeEntry->getSize());
				} else {
					/* Abandon the leading entry - it is too small to be on the free list */
					uintptr_t leadingSize = ((uintptr_t)lowAddress) - ((uintptr_t)currentFreeEntry);
					totalContractSize += leadingSize;
					currentFreeEntry = NULL;
				}

			}

			Assert_MM_true((NULL == currentFreeEntry) || (previousFreeEntry < currentFreeEntry));
			setNextForFreeEntryInFreeList(freeList, previousFreeEntry, currentFreeEntry);

			/* Adjust the free memory data */
			Assert_MM_true(freeList->_freeSize >= totalContractSize);
			freeList->_freeSize -= totalContractSize;
			freeList->_freeCount -= contractCount;
			ret = true;
		}
	}
	return ret;
}
 
void
MM_MemoryPoolHybrid::postProcess(MM_EnvironmentBase* env, Cause cause)
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

	getReservedFreeList()->reset();

	if (cause == forSweep) {

		_heapFreeLists[0]._freeSize = _sweepPoolState->_sweepFreeBytes;
		_heapFreeLists[0]._freeCount = _sweepPoolState->_sweepFreeHoles;
		freeListSplitSize = _heapFreeLists[0]._freeSize / _heapFreeListCount;

		/* remove largestFreeEntry from freeList, add it into reservedFreeList */
		if (0 < _sweepPoolState->_largestFreeEntry) {
			MM_HeapLinkedFreeHeader* largestFreeEntry;
			if (NULL == _sweepPoolState->_previousLargestFreeEntry) {
				largestFreeEntry = _heapFreeLists[0]._freeList;
				_heapFreeLists[0]._freeList = largestFreeEntry->getNext();
			} else {
				largestFreeEntry = _sweepPoolState->_previousLargestFreeEntry->getNext();
				_sweepPoolState->_previousLargestFreeEntry->setNext(largestFreeEntry->getNext());
			}
			Assert_MM_true(_sweepPoolState->_largestFreeEntry == largestFreeEntry->getSize());
			_heapFreeLists[0]._freeSize -= largestFreeEntry->getSize();
			_heapFreeLists[0]._freeCount -= 1;
			appendToReservedFreeList(env, (void*) largestFreeEntry, largestFreeEntry->getSize());
		}

		/* Free list splitting at sweep chunk granularity.
		 * Faster but less precise splits compared to free list entry granularity.
		 */
		MM_GCExtensionsBase* extensions = env->getExtensions();
		MM_ParallelSweepChunk* chunk = NULL;
		MM_SweepHeapSectioningIterator sectioningIterator(extensions->sweepHeapSectioning);
		uintptr_t freeSize = _heapFreeLists[0]._freeSize;
		uintptr_t freeCount = _heapFreeLists[0]._freeCount;
		uintptr_t accumulatedFreeSize = 0;
		uintptr_t accumulatedFreeHoles = 0;


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

				/* Set the head of the new free list. */
				currentFreeListIndex += 1;
				_heapFreeLists[currentFreeListIndex]._freeList = chunk->_splitCandidate;

				/* Update our accumulated stats. */
				accumulatedFreeSize = chunk->_accumulatedFreeSize;
				accumulatedFreeHoles = chunk->_accumulatedFreeHoles;
			}
		}

		/* Update the current free list stats. It gets the leftovers. */
		_heapFreeLists[currentFreeListIndex]._freeSize = freeSize - accumulatedFreeSize;
		_heapFreeLists[currentFreeListIndex]._freeCount = freeCount - accumulatedFreeHoles;
	} else {

	 	uintptr_t reservedFreeEntrySize = 0;
	  	MM_HeapLinkedFreeHeader* previousReservedFreeEntry = NULL;
	 	uintptr_t reservedFreeListIndex = 0;
		bool reservedFreeEntryAvaliable = false;
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

			if (currentFreeList->getSize() > reservedFreeEntrySize) {
				reservedFreeEntrySize = currentFreeList->getSize();
				reservedFreeListIndex = currentFreeListIndex;
				previousReservedFreeEntry = previousFreeList;
				reservedFreeEntryAvaliable = true;
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

		if (reservedFreeEntryAvaliable) {
			/* remove largestFreeEntry from freeList, add it into reservedFreeList */
			MM_HeapLinkedFreeHeader* largestFreeEntry;
			if (NULL == previousReservedFreeEntry) {
				largestFreeEntry = _heapFreeLists[reservedFreeListIndex]._freeList;
				_heapFreeLists[reservedFreeListIndex]._freeList = largestFreeEntry->getNext();
			} else {
				largestFreeEntry = previousReservedFreeEntry->getNext();
				previousReservedFreeEntry->setNext(largestFreeEntry->getNext());
			}
			_heapFreeLists[currentFreeListIndex]._freeSize -= largestFreeEntry->getSize();
			_heapFreeLists[currentFreeListIndex]._freeCount -= 1;
			appendToReservedFreeList(env, (void*) largestFreeEntry, largestFreeEntry->getSize());
		}

	}

	/* reset thread starting positions */
	for (currentFreeListIndex = 0; currentFreeListIndex < _heapFreeListCount; ++currentFreeListIndex) {
		_currentThreadFreeList[currentFreeListIndex] = currentFreeListIndex;
	}

#if 0
	Assert_MM_true(printFreeListValidity(env));
#endif
}

/**
 * Add the range of memory to the free list of the receiver.
 *
 * @param expandSize Number of bytes to remove from the memory pool
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 *
 */
void
MM_MemoryPoolHybrid::expandWithRange(MM_EnvironmentBase* env, uintptr_t expandSize, void* lowAddress, void* highAddress, bool canCoalesce)
{
	if (0 == expandSize) {
		return;
	}

	/* Handle the entries that are too small to make the free list */
	if (expandSize < _minimumFreeEntrySize) {
		abandonHeapChunk(lowAddress, highAddress);
		return;
	}

	MM_HeapLinkedFreeHeader* lastFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* perviousLastFreeEntry = NULL;

	MM_HeapLinkedFreeHeader* lastReservedFreeEntry = NULL;
	uintptr_t expandedSize = expandSize;
	J9ModronFreeList* reservedFreelist = getReservedFreeList();
	J9ModronFreeList* lastFreelist = NULL;

	/* this method only supports expand to higher address direction.
	 * attempt to Coalesce with the last reserved free entry first, 
	 * it doesn't connect with reserved free entry, it may connect with the last free entry,
	 * if the newly created entry is bigger than VeryLargeObjectThreshold, move or append the entry into reserved freelist.
	 * otherwise append the entry into regular free list.
	*/
	if (NULL != reservedFreelist->_freeList) {
		lastReservedFreeEntry = reservedFreelist->_freeList;
		while (lastReservedFreeEntry->getNext()) {
			lastReservedFreeEntry = lastReservedFreeEntry->getNext();
		}
		if (canCoalesce) {
			expandedSize = coalesceExpandRangeWithLastFreeEntry(lastReservedFreeEntry, reservedFreelist, lowAddress, expandSize);
		}
	}

	if (expandedSize == expandSize) {
		uintptr_t index = _heapFreeListCount-1;
		while ((NULL == _heapFreeLists[index]._freeList) && (0 != index)) {
			index--;
		}
		lastFreelist = &_heapFreeLists[index];
		if (NULL != lastFreelist->_freeList) {
			lastFreeEntry = lastFreelist->_freeList;
			while (lastFreeEntry->getNext()) {
				perviousLastFreeEntry = lastFreeEntry;
				lastFreeEntry = lastFreeEntry->getNext();
			}
				
			if (canCoalesce) {
				expandedSize = coalesceExpandRangeWithLastFreeEntry(lastFreeEntry, lastFreelist, lowAddress, expandSize);
			}
		}

		uintptr_t reservedFreeEntryThreshold = env->getExtensions()->largeObjectAllocationProfilingVeryLargeObjectThreshold;
		if (reservedFreeEntryThreshold <= expandedSize) {
			if (expandedSize != expandSize) {
				/* move coalesced free entry to reserved freelist */
				moveLastFreeEntryToReservedFreeList(env, lastFreelist, perviousLastFreeEntry, lastFreeEntry, reservedFreelist, lastReservedFreeEntry);
			} else {
				/* append to reserved freelist */
				appendToFreeList(env, reservedFreelist, lastReservedFreeEntry, lowAddress, expandSize);
			}
		} else {
			if (expandedSize == expandSize) {
				/* append to regular freelist */
				appendToFreeList(env, lastFreelist, lastFreeEntry, lowAddress, expandSize);
			}
			/* else stay in regular freelist */
		}
	}
	/* else stay in reserved freelist */

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
void*
MM_MemoryPoolHybrid::contractWithRange(MM_EnvironmentBase* env, uintptr_t contractSize, void* lowAddress, void* highAddress)
{
	if (0 == contractSize) {
		return NULL;
	}

	/* try contract in reserved freelist first, if can not find the range in reserved freellist, then try contract in the last freelist */
	if (!tryContractWithRangeInFreelist(env, getReservedFreeList(), contractSize, lowAddress, highAddress)) {
		uintptr_t index = _heapFreeListCount-1;
		while ((NULL == _heapFreeLists[index]._freeList) && (0 != index)) {
			index--;
		}
		J9ModronFreeList* freelist = &_heapFreeLists[index];
		tryContractWithRangeInFreelist(env, freelist, contractSize, lowAddress, highAddress);
	}

	assume0(isMemoryPoolValid(env, true));
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
MM_MemoryPoolHybrid::addFreeEntries(MM_EnvironmentBase* env,
													 MM_HeapLinkedFreeHeader*& freeListHead, MM_HeapLinkedFreeHeader*& freeListTail,
													 uintptr_t freeListMemoryCount, uintptr_t freeListMemorySize)
{
	uintptr_t reservedFreeEntryThreshold = env->getExtensions()->largeObjectAllocationProfilingVeryLargeObjectThreshold;
	MM_HeapLinkedFreeHeader* currentFreeEntry =NULL;
	MM_HeapLinkedFreeHeader* nextFreeEntry = freeListHead->getNext();

	MM_HeapLinkedFreeHeader* lastFreeEntry = NULL;
	MM_HeapLinkedFreeHeader* perviousLastFreeEntry = NULL;
	J9ModronFreeList* lastFreelist = NULL;

	MM_HeapLinkedFreeHeader* lastReservedFreeEntry = NULL;
	J9ModronFreeList* reservedFreelist = getReservedFreeList();
	uintptr_t expandSize = freeListHead->getSize();
	uintptr_t expandedSize = expandSize;

	uintptr_t index = _heapFreeListCount-1;
	while ((NULL == _heapFreeLists[index]._freeList) && (0 != index)) {
		index--;
	}
	lastFreelist = &_heapFreeLists[index];
	if (NULL != lastFreelist->_freeList) {
		lastFreeEntry = lastFreelist->_freeList;
		while (lastFreeEntry->getNext()) {
			perviousLastFreeEntry = lastFreeEntry;
			lastFreeEntry = lastFreeEntry->getNext();
		}
	}

	/* try to coalesce freeListHead with reservedFreelist first, then try to coalesce it with regular freelist,
	 * if the newly created entry or the rest of free entries is bigger than ReservedFreeEntryThreshold,
     * move or append it into reserved freelist, otherwise into regular freelist.
     */
	if (NULL != reservedFreelist->_freeList) {
		lastReservedFreeEntry = reservedFreelist->_freeList;
		while (lastReservedFreeEntry->getNext()) {
			lastReservedFreeEntry = lastReservedFreeEntry->getNext();
		}

		expandedSize = coalesceNewFreeEntryWithLastFreeEntry(lastReservedFreeEntry, reservedFreelist, freeListHead);
	}

	if (expandedSize == expandSize) {
		if (NULL != lastFreeEntry) {
			expandedSize = coalesceNewFreeEntryWithLastFreeEntry(lastFreeEntry, lastFreelist, freeListHead);
		}
		if (reservedFreeEntryThreshold <= expandedSize) {
			if (expandedSize != expandSize) {
				/* move coalesced free entry to reserved freelist */
				moveLastFreeEntryToReservedFreeList(env, lastFreelist, perviousLastFreeEntry, lastFreeEntry, reservedFreelist, lastReservedFreeEntry);
			} else {
				/* append to reserved freelist */
				appendToFreeList(env, reservedFreelist, lastReservedFreeEntry, freeListHead);
				lastReservedFreeEntry = freeListHead;
			}
		} else {
			if (expandedSize == expandSize) {
				/* append to regular freelist */
				appendToFreeList(env, lastFreelist, lastFreeEntry, freeListHead);
				lastFreeEntry = freeListHead;
			}
			/* else freelistHead will stay in regular freelist */
		}
	}
	/* else freelistHead will stay in reserved freelist */

	/* append the rest of freeEntries(except freelistHead) into reserved freelist or regular freelist */
	while (NULL != nextFreeEntry) {
		currentFreeEntry = nextFreeEntry;
		nextFreeEntry = currentFreeEntry->getNext();
		if (reservedFreeEntryThreshold <= currentFreeEntry->getSize()) {
			appendToFreeList(env, reservedFreelist, lastReservedFreeEntry, currentFreeEntry);
			lastReservedFreeEntry = currentFreeEntry;
		} else {
			appendToFreeList(env, lastFreelist, lastFreeEntry, currentFreeEntry);
			lastFreeEntry = currentFreeEntry;
		}
	}
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
MM_MemoryPoolHybrid::removeFreeEntriesWithinRange(MM_EnvironmentBase* env, void* lowAddress, void* highAddress,
																   uintptr_t minimumSize,
																   MM_HeapLinkedFreeHeader*& retListHead, MM_HeapLinkedFreeHeader*& retListTail,
																   uintptr_t& retListMemoryCount, uintptr_t& retListMemorySize)
{
	bool ret = false;
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
	/* remove any free entries(in regular freelists) with in range, append them to retList, then remove any reserved free entries, insert them into retList */

	/* Find the first free entry, if any, within specified range */
	uintptr_t currentFreeListIndex;
	previousFreeEntry = NULL;

	currentFreeEntry = (MM_HeapLinkedFreeHeader*)getFirstFreeStartingAddr(env, &currentFreeListIndex);
	while (currentFreeEntry) {
		currentFreeEntryTop = (void*)currentFreeEntry->afterEnd();

		/* Does this chunk fall within range ? */
		if ((currentFreeEntry >= lowAddress) || (currentFreeEntryTop > lowAddress)) {
			break;
		}

		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &currentFreeListIndex);
		if (NULL == previousFreeEntry->getNext()) {
			previousFreeEntry = NULL;
		}
	}

	if (NULL != currentFreeEntry) {
		Assert_MM_true(currentFreeEntry < highAddress);
		/* Remember the next free entry after the current one which we are going to consume at least part of */
		uintptr_t nextFreeListIndex = currentFreeListIndex;
		nextFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &nextFreeListIndex);

		/* Assume for now we will remove this entire free entry from the pool */
		Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeSize >= currentFreeEntry->getSize());
		Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeCount > 0);
		_heapFreeLists[currentFreeListIndex]._freeSize -= currentFreeEntry->getSize();
		_heapFreeLists[currentFreeListIndex]._freeCount -= 1;
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

		baseAddr = (void*)currentFreeEntry;
		topAddr = currentFreeEntryTop;


		/* Determine what to do with any leading bytes of the free entry that are not being returned */
		if (currentFreeEntry < (MM_HeapLinkedFreeHeader*)lowAddress) {
			/* Space at the head that is not being returned - is it a valid free entry? */
			if (createFreeEntry(env, currentFreeEntry, lowAddress, previousFreeEntry, NULL)) {
				uintptr_t leadingSize = ((uintptr_t)lowAddress) - ((uintptr_t)currentFreeEntry);

				previousFreeEntry = currentFreeEntry;
				_heapFreeLists[currentFreeListIndex]._freeSize += leadingSize;
				_heapFreeLists[currentFreeListIndex]._freeCount += 1;
				_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(leadingSize);
			} else {
				/* reminder is not big enough for a free entry */
				setNextForFreeEntryInFreeList(&_heapFreeLists[currentFreeListIndex], previousFreeEntry, NULL);
			}
			baseAddr = lowAddress;
		} else {
			setNextForFreeEntryInFreeList(&_heapFreeLists[currentFreeListIndex], previousFreeEntry, NULL);
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
		while (NULL != currentFreeEntry) {
			Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeSize >= currentFreeEntry->getSize());
			Assert_MM_true(_heapFreeLists[currentFreeListIndex]._freeCount > 0);
			_heapFreeLists[currentFreeListIndex]._freeSize -= currentFreeEntry->getSize();
			_heapFreeLists[currentFreeListIndex]._freeCount -= 1;
			_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

			tailFreeEntry = (MM_HeapLinkedFreeHeader*)getNextFreeStartingAddr(env, currentFreeEntry, &currentFreeListIndex);

			if (appendToList(env, (void*)currentFreeEntry, (void*)currentFreeEntry->afterEnd(),
							 minimumSize, retListHead, retListTail)) {
				++retListMemoryCount;
				retListMemorySize += currentFreeEntry->getSize();
			}
			currentFreeEntry = tailFreeEntry;
		}


		/* Kill all newly empty free lists */
		for (uintptr_t i = previousFreeListIndex + 1; (i <= currentFreeListIndex) && (i < _heapFreeListCount); ++i) {
			_heapFreeLists[i]._freeList = NULL;
			Assert_MM_true(0 == _heapFreeLists[i]._freeCount);
			Assert_MM_true(0 == _heapFreeLists[i]._freeSize);
			_heapFreeLists[i].clearHints();
		}
		ret = true;
	}

	/* check and update the reserved free list */
	J9ModronFreeList* reservedFreelist = getReservedFreeList();
	currentFreeEntry = reservedFreelist->_freeList;
	previousFreeEntry = NULL;

	while (currentFreeEntry) {
		currentFreeEntryTop = (void*)currentFreeEntry->afterEnd();

		/* Does this chunk fall within range ? */
		if (currentFreeEntry >= lowAddress || currentFreeEntryTop > lowAddress) {
			break;
		}

		previousFreeEntry = currentFreeEntry;
		currentFreeEntry = currentFreeEntry->getNext();
	}
	if (NULL == currentFreeEntry) {
		return ret;
	}
	nextFreeEntry = currentFreeEntry->getNext();
	ret = true;

	/* Assume for now we will remove this entire free entry from the pool */
	Assert_MM_true(reservedFreelist->_freeSize >= currentFreeEntry->getSize());
	Assert_MM_true(reservedFreelist->_freeCount > 0);
	reservedFreelist->_freeSize -= currentFreeEntry->getSize();
	reservedFreelist->_freeCount -= 1;
	_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

	baseAddr = (void*)currentFreeEntry;
	topAddr = currentFreeEntryTop;

	/* Determine what to do with any leading bytes of the free entry that are not being returned */
	if (currentFreeEntry < (MM_HeapLinkedFreeHeader*)lowAddress) {
		/* Space at the head that is not being returned - is it a valid free entry? */
		if (createFreeEntry(env, currentFreeEntry, lowAddress, previousFreeEntry, NULL)) {
			uintptr_t leadingSize = ((uintptr_t)lowAddress) - ((uintptr_t)currentFreeEntry);

			previousFreeEntry = currentFreeEntry;
			reservedFreelist->_freeSize += leadingSize;
			reservedFreelist->_freeCount += 1;
			_largeObjectAllocateStats->incrementFreeEntrySizeClassStats(leadingSize);
		} else {
			/* reminder is not big enough for a free entry */
			setNextForFreeEntryInFreeList(reservedFreelist, previousFreeEntry, NULL);
		}
		baseAddr = lowAddress;
	} else {
		setNextForFreeEntryInFreeList(reservedFreelist, previousFreeEntry, NULL);
	}

	/* Insert what's left if it's big enough to go on the list */
	if (insertToList(env, baseAddr, topAddr, minimumSize, retListHead, retListTail)) {
		retListMemoryCount += 1;
		retListMemorySize += ((uint8_t*)topAddr - (uint8_t*)baseAddr);
	}
	currentFreeEntry = nextFreeEntry;
	tailFreeEntry = nextFreeEntry;

	/* Now append any whole chunks to the list which fall within the specified range */
	while (NULL != currentFreeEntry) {
		Assert_MM_true(reservedFreelist->_freeSize >= currentFreeEntry->getSize());
		Assert_MM_true(reservedFreelist->_freeCount > 0);
		reservedFreelist->_freeSize -= currentFreeEntry->getSize();
		reservedFreelist->_freeCount -= 1;
		_largeObjectAllocateStats->decrementFreeEntrySizeClassStats(currentFreeEntry->getSize());

		tailFreeEntry = currentFreeEntry->getNext();

		if (insertToList(env, (void*)currentFreeEntry, (void*)currentFreeEntry->afterEnd(),
						 minimumSize, retListHead, retListTail)) {
			retListMemoryCount += 1;
			retListMemorySize += currentFreeEntry->getSize();
		}
		currentFreeEntry = tailFreeEntry;
	}

	return ret;
}
