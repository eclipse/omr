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

#if defined(OMR_GC_MODRON_STANDARD)
#include <string.h>

#include "Debug.hpp"
#include "AllocateDescription.hpp"
#include "Bits.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionManager.hpp"
#include "MemoryPool.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepPoolState.hpp"
#include "MarkMap.hpp"
#include "MemoryPoolAddressOrderedListBase.hpp"
#include "SweepPoolManagerAddressOrderedListBase.hpp"
#include "ModronAssertions.h"

/**
 * Initialize any internal structures.
 * @return true if initialization is successful, false otherwise.
 */
bool
MM_SweepPoolManagerAddressOrderedListBase::initialize(MM_EnvironmentBase *env)
{
	if (!MM_SweepPoolManager::initialize(env)) {
		return false;
	}
	return true;
}

/**
 * Get the sweep scheme state for the given memory pool.
 */
MM_SweepPoolState *
MM_SweepPoolManagerAddressOrderedListBase::getPoolState(MM_MemoryPool *memoryPool)
{
	MM_SweepPoolState *result = ((MM_MemoryPoolAddressOrderedListBase *)memoryPool)->getSweepPoolState();

	Assert_MM_true(NULL != result);
	
	return result;
}

/**
 * Calculate trailing details
 * 
 * @param sweepChunk current chunk
 * @param trailingCandidate trailing candidate address
 * @param trailingCandidateSlotCount trailing candidate size
 */
MMINLINE void
MM_SweepPoolManagerAddressOrderedListBase::calculateTrailingDetails(
	MM_ParallelSweepChunk *sweepChunk,
	uintptr_t *trailingCandidate,
	uintptr_t trailingCandidateSlotCount)
{
	/* Calculating the trailing free details implies that there is an object
	 * that is in front of the heapSlotFreeHead.  No safety checks required.
	 */
	uintptr_t projection;
	uintptr_t trailingCandidateByteCount = (uintptr_t)MM_Bits::convertSlotsToBytes(trailingCandidateSlotCount);

	projection = _extensions->objectModel.getConsumedSizeInBytesWithHeader((omrobjectptr_t)(trailingCandidate - J9MODRON_HEAP_SLOTS_PER_MARK_BIT)) - (J9MODRON_HEAP_SLOTS_PER_MARK_BIT * sizeof(uintptr_t));
	if(projection > trailingCandidateByteCount) {
		projection -= trailingCandidateByteCount;
		sweepChunk->projection = projection;
	} else {
		if(projection < trailingCandidateByteCount) {
			sweepChunk->trailingFreeCandidate = (void *)(((uintptr_t)trailingCandidate) + projection);
			sweepChunk->trailingFreeCandidateSize = trailingCandidateByteCount - projection;
		}
	}
}

/**
 * Connect a chunk into the free list.
 * Given a previously swept chunk, connect its data to the free list of the associated memory subspace.
 * This requires that all previous chunks in the memory subspace have been swept.
 * @note It is expected that the sweep state free information is updated properly.
 * @note the free list is not guaranteed to be in a consistent state at the end of the routine
 * @note a chunk can only be connected by a single thread (single threaded routine)
 * @todo add the general algorithm to the comment header.
 */
void
MM_SweepPoolManagerAddressOrderedListBase::connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk)
{
#if defined(J9MODRON_SWEEP_SCHEME_CONNECT_CHUNKS_TRACE)
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	omrtty_printf("CC: sweep chunk %p (%p %p)\n", chunk, chunk->chunkBase, chunk->chunkTop);
#endif

	/*
	 *  Memory Pool and Memory Pool State
	 */
	MM_MemoryPoolAddressOrderedListBase *memoryPool = (MM_MemoryPoolAddressOrderedListBase *)chunk->memoryPool;
	MM_SweepPoolState *sweepState = getPoolState(memoryPool);

	MM_HeapLinkedFreeHeader *previousFreeEntry = sweepState->_connectPreviousFreeEntry;
	uintptr_t previousFreeEntrySize = sweepState->_connectPreviousFreeEntrySize;
	MM_ParallelSweepChunk *previousConnectChunk = sweepState->_connectPreviousChunk;
	/* previousPreviousFreeEntry is only for SPMSAOL */
	MM_HeapLinkedFreeHeader *previousPreviousFreeEntry = sweepState->_connectPreviousPreviousFreeEntry;

	MM_HeapLinkedFreeHeader *leadingFreeEntry = (MM_HeapLinkedFreeHeader *)chunk->leadingFreeCandidate;
	uintptr_t leadingFreeEntrySize = chunk->leadingFreeCandidateSize;

	Assert_MM_true((NULL == leadingFreeEntry) || (previousFreeEntry < leadingFreeEntry));
	/* Assume no projection until we prove otherwise */
	uintptr_t projection = 0;

	/* If there is a projection from the previous chunk? It does not necessarily have to be associated to the pool for the
	 * projection to actually matter.
	 */
	if(NULL != chunk->_previous) {
		Assert_MM_true((0 == chunk->_previous->projection) || (chunk->_previous->chunkTop == chunk->chunkBase));
		projection = chunk->_previous->projection;
	}

	/* Any projection to be dealt with ? */
	if (0 != projection) {
		if(projection > (((uintptr_t)chunk->chunkTop) - ((uintptr_t)chunk->chunkBase))) {
			chunk->projection = projection - (((uintptr_t)chunk->chunkTop) - ((uintptr_t)chunk->chunkBase));
			leadingFreeEntry = (MM_HeapLinkedFreeHeader *)NULL;
			leadingFreeEntrySize = 0;
		} else {
			leadingFreeEntry = (MM_HeapLinkedFreeHeader *)(((uintptr_t)leadingFreeEntry) + projection);
			leadingFreeEntrySize -= projection;
		}
	}

	/* See if there is a connection to be made between the previous free and the leading entry */
	if((previousFreeEntry != NULL)
		&& ((((uint8_t *)previousFreeEntry) + previousFreeEntrySize) == (uint8_t *)leadingFreeEntry)
		&& (previousConnectChunk->memoryPool == memoryPool)
		&& chunk->_coalesceCandidate
	) {
		/* So should be using same MM_SweepPoolState */
		Assert_MM_true(getPoolState(previousConnectChunk->memoryPool) == sweepState);

		/* The previous free entry consumes the leading free entry */
		/* This trumps any checks on the trailing free space of the previous chunk */
#if defined(J9MODRON_SWEEP_SCHEME_CONNECT_CHUNKS_TRACE)
		omrtty_printf("CC: Previous consumes leading: %p(+%p) -> %p(+%p)\n", previousFreeEntry, previousFreeEntrySize, leadingFreeEntry, leadingFreeEntrySize);
#endif
		memoryPool->getLargeObjectAllocateStats()->decrementFreeEntrySizeClassStats(previousFreeEntrySize);
		previousFreeEntrySize += leadingFreeEntrySize;
		sweepState->_sweepFreeBytes += leadingFreeEntrySize;
 		sweepState->updateLargestFreeEntry(previousFreeEntrySize, previousPreviousFreeEntry);
		memoryPool->getLargeObjectAllocateStats()->incrementFreeEntrySizeClassStats(previousFreeEntrySize);

		/* Consume the leading entry */
		leadingFreeEntry = NULL;
	}

	/* If the leading entry wasn't consumed, check if there is trailing free space in the previous chunk
	 * that could connect with the leading entry leading free space */
	if(NULL != previousConnectChunk) {
		if((NULL != leadingFreeEntry)
			&& ((((uint8_t *)previousConnectChunk->trailingFreeCandidate) + previousConnectChunk->trailingFreeCandidateSize) == (uint8_t *)leadingFreeEntry)
			&& (previousConnectChunk->memoryPool == memoryPool)
			&& chunk->_coalesceCandidate
		) {
			/* The trailing/leading space forms a contiguous chunk - check if it belongs on the free list, abandon otherwise */

			uintptr_t jointFreeSize = leadingFreeEntrySize + previousConnectChunk->trailingFreeCandidateSize;
			if(memoryPool->canMemoryBeConnectedToPool(env,
					previousConnectChunk->trailingFreeCandidate,
					jointFreeSize)) {

				/* Free list candidate has been found - attach it to the free list */
#if defined(J9MODRON_SWEEP_SCHEME_CONNECT_CHUNKS_TRACE)
				omrtty_printf("CC: trailing/leading merged %p(+%p) + %p(+%p)\n", previousConnectChunk->trailingFreeCandidate, previousConnectChunk->trailingFreeCandidateSize, leadingFreeEntry, leadingFreeEntrySize);
#endif

				memoryPool->connectOuterMemoryToPool(env,
						previousFreeEntry,
						previousFreeEntrySize,
						previousConnectChunk->trailingFreeCandidate);

				connectChunkPostProcess(chunk, sweepState, (MM_HeapLinkedFreeHeader *)previousConnectChunk->trailingFreeCandidate, previousFreeEntry);
				/* only for SPMSAOL */
				previousPreviousFreeEntry = previousFreeEntry;
				previousFreeEntry = (MM_HeapLinkedFreeHeader *)previousConnectChunk->trailingFreeCandidate;
				previousFreeEntrySize = jointFreeSize;

				/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
				if(0 != jointFreeSize) {
					sweepState->_sweepFreeBytes += jointFreeSize;
					sweepState->_sweepFreeHoles += 1;
			 		sweepState->updateLargestFreeEntry(jointFreeSize, previousPreviousFreeEntry);

					memoryPool->getLargeObjectAllocateStats()->incrementFreeEntrySizeClassStats(jointFreeSize);
				}
			}

			/* Consume the leading entry */
			leadingFreeEntry = NULL;
		} else {
			/* The trailing space of the previous chunk has no connecting partner - check if the trailing space merits an entry in the free list */
			if(memoryPool->canMemoryBeConnectedToPool(env,
					previousConnectChunk->trailingFreeCandidate,
					previousConnectChunk->trailingFreeCandidateSize)) {

#if defined(J9MODRON_SWEEP_SCHEME_CONNECT_CHUNKS_TRACE)
				omrtty_printf("CC: trailing from previous used %p(+%p)\n", previousConnectChunk->trailingFreeCandidate, previousConnectChunk->trailingFreeCandidateSize);
#endif

				memoryPool->connectOuterMemoryToPool(env,
						previousFreeEntry,
						previousFreeEntrySize,
						previousConnectChunk->trailingFreeCandidate);

				connectChunkPostProcess(chunk, sweepState, (MM_HeapLinkedFreeHeader *)previousConnectChunk->trailingFreeCandidate, previousFreeEntry);
				/* only for SPMSAOL */
				previousPreviousFreeEntry = previousFreeEntry;
				previousFreeEntry = (MM_HeapLinkedFreeHeader *)previousConnectChunk->trailingFreeCandidate;
				previousFreeEntrySize = previousConnectChunk->trailingFreeCandidateSize;

				/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
				if(0 != previousConnectChunk->trailingFreeCandidateSize) {
					sweepState->_sweepFreeBytes += previousConnectChunk->trailingFreeCandidateSize;
					sweepState->_sweepFreeHoles += 1;
			 		sweepState->updateLargestFreeEntry(previousConnectChunk->trailingFreeCandidateSize, previousPreviousFreeEntry);

					memoryPool->getLargeObjectAllocateStats()->incrementFreeEntrySizeClassStats(previousConnectChunk->trailingFreeCandidateSize);
				}
			}
		}
	}

	/* If the leading free entry has not been consumed - check if it is now a trailing free candidate.  This scenario can occur if a leading
	 * free entry is pushed back from a projection from the previous sweep chunk.  A leading free candidate that spans the entire chunk in this
	 * case now becomes a trailing free entry.
	 */
	if(leadingFreeEntry && ((void *)(((uintptr_t)leadingFreeEntry) + leadingFreeEntrySize) == chunk->chunkTop)) {
		chunk->leadingFreeCandidate = NULL;
		chunk->leadingFreeCandidateSize = 0;
		chunk->trailingFreeCandidate = leadingFreeEntry;
		chunk->trailingFreeCandidateSize = leadingFreeEntrySize;
	} else {
		/* If the leading entry still hasn't been consumed, it touches neither the head or tail of a chunk.
		 * Check if it merits an entry in the free list
		 */
		if(NULL != leadingFreeEntry) {
			if(memoryPool->canMemoryBeConnectedToPool(env,
					leadingFreeEntry,
					leadingFreeEntrySize)) {

				Assert_MM_true(previousFreeEntry <= leadingFreeEntry);

				memoryPool->connectOuterMemoryToPool(env,
						previousFreeEntry,
						previousFreeEntrySize,
						leadingFreeEntry);

				connectChunkPostProcess(chunk, sweepState, leadingFreeEntry, previousFreeEntry);
				/* only for SPMSAOL */
				previousPreviousFreeEntry = previousFreeEntry;
				previousFreeEntry = leadingFreeEntry;
				previousFreeEntrySize = leadingFreeEntrySize;

				/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
				if(0 != leadingFreeEntrySize) {
					sweepState->_sweepFreeBytes += leadingFreeEntrySize;
					sweepState->_sweepFreeHoles += 1;
			 		sweepState->updateLargestFreeEntry(leadingFreeEntrySize, previousPreviousFreeEntry);

					memoryPool->getLargeObjectAllocateStats()->incrementFreeEntrySizeClassStats(leadingFreeEntrySize);
				}
			} else {
				/* Abandon it. We need to do this in case we encounter a free entry which
			 	 * spans 2 memory pools which is possible when LOA is enabled.
			 	 */
				memoryPool->abandonMemoryInPool(env,
						leadingFreeEntry,
						leadingFreeEntrySize);
			}
		}
	}

	/* If there is a previous free entry
	 * and a free list head in the current chunk, attach
	 * otherwise, make chunk head the free list head for the profile
	 */
	if(chunk->freeListHead) {
		Assert_MM_true(previousFreeEntry < chunk->freeListHead);

		memoryPool->connectOuterMemoryToPool(env,
				previousFreeEntry,
				previousFreeEntrySize,
				chunk->freeListHead);

		/* Set split candidate information */
		connectChunkPostProcess(chunk, sweepState, chunk->freeListHead, previousFreeEntry);

		/* Adjusted the largest free entry found in chunk for the sweep state */
		updateLargestFreeEntryInChunk(chunk, sweepState, previousFreeEntry);

		/* If there is a head, there is a tail - update the previous free entry -- only for SPMSAOL */
		if (NULL != chunk->previousFreeListTail) {
			previousPreviousFreeEntry = chunk->previousFreeListTail;
		} else {
			previousPreviousFreeEntry = previousFreeEntry;
		}
		previousFreeEntry = chunk->freeListTail;
		previousFreeEntrySize = chunk->freeListTailSize;

		/* Maintain the free bytes/holes count in the allocate profile for expansion/contraction purposes */
		if(0 != chunk->freeBytes) {
			sweepState->_sweepFreeBytes += chunk->freeBytes;
			sweepState->_sweepFreeHoles += chunk->freeHoles;
		}

#if defined(J9MODRON_SWEEP_SCHEME_CONNECT_CHUNKS_TRACE)
		omrtty_printf("CC: head + tail %p(+%p) %p(%p)\n", chunk->freeListHead, chunk->freeListHeadSize, chunk->freeListTail, chunk->freeListTailSize);
#endif
	}

	/* Update the allocate profile with the previous free entry and previous chunk */
	sweepState->_connectPreviousFreeEntry = previousFreeEntry;
	sweepState->_connectPreviousPreviousFreeEntry = previousPreviousFreeEntry;
	sweepState->_connectPreviousFreeEntrySize = previousFreeEntrySize;
	sweepState->_connectPreviousChunk = chunk;

	memoryPool->incrementDarkMatterBytes(chunk->_darkMatterBytes);
	memoryPool->incrementDarkMatterSamples(chunk->_darkMatterSamples);
}


/**
 * Flush any unaccounted for free entries to the free list.
 */
void
MM_SweepPoolManagerAddressOrderedListBase::flushFinalChunk(
	MM_EnvironmentBase *envModron,
	MM_MemoryPool *memoryPoolBase)
{
	MM_SweepPoolState *sweepState = getPoolState(memoryPoolBase);

	/* If the last chunk had trailing free space, try and add it to the free list */
	if((sweepState->_connectPreviousChunk != NULL)
	&& (sweepState->_connectPreviousChunk->trailingFreeCandidateSize > 0)) {
		MM_MemoryPoolAddressOrderedListBase *memoryPool = (MM_MemoryPoolAddressOrderedListBase *)memoryPoolBase;

		/* Check if the entry is a candidate */
		if (!memoryPool->canMemoryBeConnectedToPool(envModron,
				sweepState->_connectPreviousChunk->trailingFreeCandidate,
				sweepState->_connectPreviousChunk->trailingFreeCandidateSize)){
			/* It is not - abandon it */
			memoryPool->abandonMemoryInPool(envModron,
					sweepState->_connectPreviousChunk->trailingFreeCandidate,
					sweepState->_connectPreviousChunk->trailingFreeCandidateSize);

		} else {
			/* It is - fold it into the free list */
			memoryPool->connectOuterMemoryToPool(envModron,
					sweepState->_connectPreviousFreeEntry,
					sweepState->_connectPreviousFreeEntrySize,
					sweepState->_connectPreviousChunk->trailingFreeCandidate);

			sweepState->_connectPreviousPreviousFreeEntry = sweepState->_connectPreviousFreeEntry;
			sweepState->_connectPreviousFreeEntry = (MM_HeapLinkedFreeHeader *)sweepState->_connectPreviousChunk->trailingFreeCandidate;
			sweepState->_connectPreviousFreeEntrySize = sweepState->_connectPreviousChunk->trailingFreeCandidateSize;
			Assert_MM_true(sweepState->_connectPreviousFreeEntry != sweepState->_connectPreviousChunk->leadingFreeCandidate);

			sweepState->_sweepFreeBytes += sweepState->_connectPreviousChunk->trailingFreeCandidateSize;
			sweepState->_sweepFreeHoles += 1;
	 		sweepState->updateLargestFreeEntry(sweepState->_connectPreviousChunk->trailingFreeCandidateSize, sweepState->_connectPreviousPreviousFreeEntry);

			memoryPool->getLargeObjectAllocateStats()->incrementFreeEntrySizeClassStats(sweepState->_connectPreviousChunk->trailingFreeCandidateSize);
		}
	}
}
/**
 *  Finally if there is at least 1 entry in the subspace, the last entry should be connected to NULL in the free list
 */
void
MM_SweepPoolManagerAddressOrderedListBase::connectFinalChunk(
	MM_EnvironmentBase *envModron,
	MM_MemoryPool *memoryPool)
{
	MM_SweepPoolState *sweepState = getPoolState(memoryPool);

	if(sweepState->_connectPreviousFreeEntry) {

		((MM_MemoryPoolAddressOrderedListBase *)memoryPool)->connectFinalMemoryToPool(envModron,
				sweepState->_connectPreviousFreeEntry,
				sweepState->_connectPreviousFreeEntrySize);

 		sweepState->updateLargestFreeEntry(sweepState->_connectPreviousFreeEntrySize, sweepState->_connectPreviousPreviousFreeEntry);
	}

	/* The free entries should now be in a walkable state */
	assume0(memoryPool->isValidListOrdering());

	/* Update pool free memory statistics */
	((MM_MemoryPoolAddressOrderedListBase *)memoryPool)->updateMemoryPoolStatistics(envModron,
			sweepState->_sweepFreeBytes,
			sweepState->_sweepFreeHoles,
			sweepState->_largestFreeEntry);

	/* Validate sweeps free space accounting */
	assume0(memoryPool->isMemoryPoolValid(envModron, false));
}

/**
 * Update trailing free memory
 *
 * @param sweepChunk current chunk
 * @param trailingCandidate trailing candidate address
 * @param trailingCandidateSlotCount trailing candidate size
 */
void
MM_SweepPoolManagerAddressOrderedListBase::updateTrailingFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, uintptr_t *heapSlotFreeHead, uintptr_t heapSlotFreeCount)
{
	calculateTrailingDetails(sweepChunk, heapSlotFreeHead, heapSlotFreeCount);
}

/**
 * 	Add free memory slot to pool list
 * 
 * @param sweepChunk current memory chunk
 * @param address start address of memory slot
 * @param size size of free memory slot
 */
bool
MM_SweepPoolManagerAddressOrderedListBase::addFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, uintptr_t *address, uintptr_t size)
{
	bool result = false;
	
	/* This implementation is able to support SORTED pieces of memory ONLY!!! */
	Assert_MM_true((uintptr_t *)sweepChunk->freeListTail <= address);

	if(address == (uintptr_t *)sweepChunk->chunkBase) {
		/* Update the sweep chunk table entry with the leading free hole information */
		sweepChunk->leadingFreeCandidate = address;
		sweepChunk->leadingFreeCandidateSize = (uintptr_t)MM_Bits::convertSlotsToBytes(size);
		Assert_MM_true(sweepChunk->leadingFreeCandidate > sweepChunk->trailingFreeCandidate);

	} else if(address + size == (uintptr_t *)sweepChunk->chunkTop) {
		/* Update the sweep chunk table entry with the trailing free hole information */
		calculateTrailingDetails(sweepChunk, address, size);

	} else {
		/* Check if the hole is a free list candidate */
		uintptr_t heapFreeByteCount = MM_Bits::convertSlotsToBytes(size);
		uintptr_t objectSizeDelta = 
			_extensions->objectModel.getConsumedSizeInBytesWithHeader((omrobjectptr_t)(address - J9MODRON_HEAP_SLOTS_PER_MARK_BIT))
			- (J9MODRON_HEAP_SLOTS_PER_MARK_BIT * sizeof(uintptr_t));
		Assert_MM_true(objectSizeDelta <= heapFreeByteCount);
		heapFreeByteCount -= objectSizeDelta;
		address = (uintptr_t *) (((uintptr_t)address) + objectSizeDelta);
		MM_MemoryPoolAddressOrderedListBase *memoryPool = (MM_MemoryPoolAddressOrderedListBase *)sweepChunk->memoryPool;

		if(memoryPool->connectInnerMemoryToPool(env, address, heapFreeByteCount, sweepChunk->freeListTail)) {

#if defined(J9MODRON_ALLOCATION_MANAGER_TRACE)
			OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
			omrtty_printf("SW: Free entry %p %p\n", address, ((uint8_t *)address) + heapFreeByteCount);
#endif

			/* If the hole is first in this chunk, make it the header */
			if(NULL == sweepChunk->freeListTail) {
				sweepChunk->freeListHead = (MM_HeapLinkedFreeHeader *)address;
				sweepChunk->freeListHeadSize = heapFreeByteCount;
			}

			/* Maintain the free bytes/holes count in the chunk for heap expansion/contraction purposes (will be gathered up in the allocate profile) */
			if(0 != heapFreeByteCount) {
				sweepChunk->freeBytes += heapFreeByteCount;
				sweepChunk->freeHoles += 1;
//				sweepChunk->_largestFreeEntry = max(sweepChunk->_largestFreeEntry , heapFreeByteCount);
				sweepChunk->updateLargestFreeEntry(heapFreeByteCount, sweepChunk->freeListTail);

				/* increment thread local sizeClass stats that will later be merged */
				memoryPool->getLargeObjectAllocateStats()->incrementFreeEntrySizeClassStats(heapFreeByteCount, &env->_freeEntrySizeClassStats);
			}

			sweepChunk->previousFreeListTail = sweepChunk->freeListTail;
			sweepChunk->freeListTail = (MM_HeapLinkedFreeHeader *)address;
			sweepChunk->freeListTailSize = heapFreeByteCount;
		}
		result = true;
	}
	
	return result;
}
#endif /* defined(OMR_GC_MODRON_STANDARD) */
