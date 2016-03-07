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
#include "modronopt.h"
#include "ut_j9mm.h"
#include "omrcomp.h"

#include <string.h>

#include "AllocateDescription.hpp"
#include "Bits.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapMemoryPoolIterator.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "MemoryPool.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ParallelSweepChunk.hpp"
#include "ParallelSweepScheme.hpp"
#include "ParallelTask.hpp"
#include "SweepHeapSectioningSegmented.hpp"
#include "SweepPoolManagerAddressOrderedList.hpp"
#include "SweepPoolState.hpp"
#include "MarkMap.hpp"
#include "ModronAssertions.h"
#include "HeapMapWordIterator.hpp"
#include "ObjectModel.hpp"
#include "Math.hpp"

#if defined(OMR_ENV_DATA64)
#define J9MODRON_OBM_SLOT_EMPTY ((uintptr_t)0x0000000000000000)
#define J9MODRON_OBM_SLOT_FIRST_SLOT ((uintptr_t)0x0000000000000001)
#define J9MODRON_OBM_SLOT_LAST_SLOT ((uintptr_t)0x8000000000000000)
#else /* OMR_ENV_DATA64 */
#define J9MODRON_OBM_SLOT_EMPTY ((uintptr_t)0x00000000)
#define J9MODRON_OBM_SLOT_FIRST_SLOT ((uintptr_t)0x00000001)
#define J9MODRON_OBM_SLOT_LAST_SLOT ((uintptr_t)0x80000000)
#endif /* OMR_ENV_DATA64 */

/**
 * Run the sweep task.
 * Skeletal code to run the sweep task per work thread.  No actual work done.
 */
void
MM_ParallelSweepTask::run(MM_EnvironmentBase *env)
{
	_sweepScheme->internalSweep(env);
}

/**
 * Initialize sweep statistics per work thread at the beginning of the sweep task.
 */
void
MM_ParallelSweepTask::setup(MM_EnvironmentBase *env)
{
	env->_sweepStats.clear();

	/* record that this thread is participating in this cycle */
	env->_sweepStats._gcCount = env->getExtensions()->globalGCStats.gcCount;

	env->_freeEntrySizeClassStats.resetCounts();
}

/**
 * Gather sweep statistics into the global statistics counter at the end of the sweep task.
 */
void
MM_ParallelSweepTask::cleanup(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	MM_GlobalGCStats *finalGCStats = &env->getExtensions()->globalGCStats;
	finalGCStats->sweepStats.merge(&env->_sweepStats);
	
	Trc_MM_ParallelSweepTask_parallelStats(
		env->getLanguageVMThread(),
		(uint32_t)env->getSlaveID(), 
		(uint32_t)omrtime_hires_delta(0, env->_sweepStats.idleTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS), 
		env->_sweepStats.sweepChunksProcessed, 
		(uint32_t)omrtime_hires_delta(0, env->_sweepStats.mergeTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS));
}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
/**
 * Stats gathering for synchronizing threads during sweep.
 */
void
MM_ParallelSweepTask::synchronizeGCThreads(MM_EnvironmentBase *env, const char *id)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	uint64_t endTime = omrtime_hires_clock();
	env->_sweepStats.addToIdleTime(startTime, endTime);
}

/**
 * Stats gathering for synchornizing threads during sweep.
 */
bool
MM_ParallelSweepTask::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster(env, id);
	uint64_t endTime = omrtime_hires_clock();
	env->_sweepStats.addToIdleTime(startTime, endTime);

	return result;
}

bool
MM_ParallelSweepTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(env, id);
	uint64_t endTime = omrtime_hires_clock();
	env->_sweepStats.addToIdleTime(startTime, endTime);

	return result;
}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_ParallelSweepScheme *
MM_ParallelSweepScheme::newInstance(MM_EnvironmentBase *env)
{
	MM_ParallelSweepScheme *sweepScheme;
	
	sweepScheme = (MM_ParallelSweepScheme *)env->getForge()->allocate(sizeof(MM_ParallelSweepScheme), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sweepScheme) {
		new(sweepScheme) MM_ParallelSweepScheme(env);
		if (!sweepScheme->initialize(env)) { 
        	sweepScheme->kill(env);        
        	sweepScheme = NULL;            
		}                                       
	}

	return sweepScheme;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_ParallelSweepScheme::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize any internal structures.
 * @return true if initialization is successful, false otherwise.
 */
bool
MM_ParallelSweepScheme::initialize(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	extensions->sweepHeapSectioning = MM_SweepHeapSectioningSegmented::newInstance(env);
	if(NULL == extensions->sweepHeapSectioning) {
		return false;
	}
	_sweepHeapSectioning = extensions->sweepHeapSectioning;

	if (0 != omrthread_monitor_init_with_name(&_mutexSweepPoolState, 0, "SweepPoolState Monitor")) {
		return false;
	}
	
	return true;
}

/**
 * Tear down internal structures.
 */
void
MM_ParallelSweepScheme::tearDown(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	if(NULL != extensions->sweepHeapSectioning) {
		extensions->sweepHeapSectioning->kill(env);
		extensions->sweepHeapSectioning = NULL;
		_sweepHeapSectioning = NULL;
	}

	if (NULL != _poolSweepPoolState) {
		pool_kill(_poolSweepPoolState);
		_poolSweepPoolState = NULL;
	}

	if (0 != _mutexSweepPoolState) {
		omrthread_monitor_destroy(_mutexSweepPoolState);
	}
}

/**
 * Request to create sweepPoolState class for pool
 * @param  memoryPool memory pool to attach sweep state to
 * @return pointer to created class
 */
void *
MM_ParallelSweepScheme::createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	omrthread_monitor_enter(_mutexSweepPoolState);
	if (NULL == _poolSweepPoolState) {
		_poolSweepPoolState = pool_new(sizeof(MM_SweepPoolState), 0, 2 * sizeof(uintptr_t), 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(OMRPORTLIB));
		if (NULL == _poolSweepPoolState) {
			omrthread_monitor_exit(_mutexSweepPoolState);
			return NULL;
		}
	}
	omrthread_monitor_exit(_mutexSweepPoolState);
	
	return MM_SweepPoolState::newInstance(env, _poolSweepPoolState, _mutexSweepPoolState, memoryPool);
}

/**
 * Request to destroy sweepPoolState class for pool
 * @param  sweepPoolState class to destroy
 */
void
MM_ParallelSweepScheme::deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState)
{
	((MM_SweepPoolState *)sweepPoolState)->kill(env, _poolSweepPoolState, _mutexSweepPoolState);
}

/**
 * Get the sweep scheme state for the given memory pool.
 */
MM_SweepPoolState *
MM_ParallelSweepScheme::getPoolState(MM_MemoryPool *memoryPool)
{
	MM_SweepPoolManager *sweepPoolManager = memoryPool->getSweepPoolManager();
	return sweepPoolManager->getPoolState(memoryPool);
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * 
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @return true if operation completes with success (always)
 */
bool
MM_ParallelSweepScheme::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	/* this method is called too often in some configurations (ie: Tarok) so we update the sectioning table in heapReconfigured */
	return true;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * 
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 * @return true if operation completes with success (always)
 */
bool
MM_ParallelSweepScheme::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	/* this method is called too often in some configurations (ie: Tarok) so we update the sectioning table in heapReconfigured */
	return true;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * No new memory has been added to a heap reconfiguration.  This call typically is the result
 * of having segment range changes (memory redistributed between segments) or the meaning of
 * memory changed.
 * 
 */
void
MM_ParallelSweepScheme::heapReconfigured(MM_EnvironmentBase *env)
{
	_sweepHeapSectioning->update(env);
}

/**
 * Initialize all internal structures in order to perform a parallel sweep.
 * 
 * @note called by the master thread only
 */
void
MM_ParallelSweepScheme::setupForSweep(MM_EnvironmentBase *env)
{
	/* the markMap uses the heapBase() (not the activeHeapBase()) for its calculations. We must use the same base. */
	_heapBase = _extensions->heap->getHeapBase();
}


/****************************************
 * Core sweep functionality
 ****************************************
 */

MMINLINE void
MM_ParallelSweepScheme::sweepMarkMapBody(
	uintptr_t * &markMapCurrent,
	uintptr_t * &markMapChunkTop,
	uintptr_t * &markMapFreeHead,
	uintptr_t &heapSlotFreeCount,
	uintptr_t * &heapSlotFreeCurrent,
	uintptr_t * &heapSlotFreeHead )
{
	if(*markMapCurrent == J9MODRON_OBM_SLOT_EMPTY) {
		markMapFreeHead = markMapCurrent;
		heapSlotFreeHead = heapSlotFreeCurrent;

		markMapCurrent += 1;

		while(markMapCurrent < markMapChunkTop) {
			if(*markMapCurrent != J9MODRON_OBM_SLOT_EMPTY) {
				break;
			}
			markMapCurrent += 1;
		}

		/* Find the number of slots we've walked
		 * (pointer math makes this the number of slots)
		 */
		uintptr_t count = markMapCurrent - markMapFreeHead;
		heapSlotFreeCount = J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * count;
		heapSlotFreeCurrent += J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * count;
	}
}

MMINLINE void
MM_ParallelSweepScheme::sweepMarkMapHead(
	uintptr_t *markMapFreeHead,
	uintptr_t *markMapChunkBase,
	uintptr_t * &heapSlotFreeHead,
	uintptr_t &heapSlotFreeCount )
{
	uintptr_t markMapHeadValue;
	uintptr_t markMapFreeBitIndexHead;

	if(markMapFreeHead > markMapChunkBase) {
		markMapHeadValue = *(markMapFreeHead - 1);
		markMapFreeBitIndexHead = J9MODRON_HEAP_SLOTS_PER_MARK_BIT * MM_Bits::trailingZeroes(markMapHeadValue);
		if(markMapFreeBitIndexHead) {
			heapSlotFreeHead -= markMapFreeBitIndexHead;
			heapSlotFreeCount += markMapFreeBitIndexHead;
		}
	}
}

MMINLINE void
MM_ParallelSweepScheme::sweepMarkMapTail(
	uintptr_t *markMapCurrent,
	uintptr_t *markMapChunkTop,
	uintptr_t &heapSlotFreeCount )
{
	uintptr_t markMapTailValue;
	uintptr_t markMapFreeBitIndexTail;

	if(markMapCurrent < markMapChunkTop) {
		markMapTailValue = *markMapCurrent;
		markMapFreeBitIndexTail = J9MODRON_HEAP_SLOTS_PER_MARK_BIT * MM_Bits::leadingZeroes(markMapTailValue);

		if(markMapFreeBitIndexTail) {
			heapSlotFreeCount += markMapFreeBitIndexTail;
		}
	}
}

uintptr_t
MM_ParallelSweepScheme::performSamplingCalculations(MM_ParallelSweepChunk *sweepChunk, uintptr_t* markMapCurrent, uintptr_t* heapSlotFreeCurrent)
{
	const uintptr_t minimumFreeEntrySize = sweepChunk->memoryPool->getMinimumFreeEntrySize();
	uintptr_t darkMatter = 0;

	/* this word has objects in it. Sample them for dark matter */
	MM_HeapMapWordIterator markedObjectIterator(*markMapCurrent, heapSlotFreeCurrent);

	/* Hole at the beginning of the sample is not considered, since we do not know
	 * if that's part of a preceding object or part of hole.
	 */
	omrobjectptr_t prevObject = markedObjectIterator.nextObject();
	Assert_MM_true(NULL != prevObject);
	uintptr_t prevObjectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(prevObject);

	omrobjectptr_t object = NULL;
	while (NULL != (object = markedObjectIterator.nextObject())) {
		uintptr_t holeSize = (uintptr_t)object - ((uintptr_t)prevObject + prevObjectSize);
		Assert_MM_true(holeSize < minimumFreeEntrySize);
		darkMatter += holeSize;
		prevObject = object;
		prevObjectSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader(prevObject);
	}

	/* find the trailing dark matter */
	uintptr_t * endOfPrevObject = (uintptr_t*)((uintptr_t)prevObject + prevObjectSize);
	uintptr_t * startSearchAt = (uintptr_t*)MM_Math::roundToFloor(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(uintptr_t), (uintptr_t)endOfPrevObject);
	uintptr_t * endSearchAt = (uintptr_t*)MM_Math::roundToCeiling(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(uintptr_t), (uintptr_t)endOfPrevObject + minimumFreeEntrySize);
	startSearchAt = OMR_MAX(startSearchAt, heapSlotFreeCurrent + J9MODRON_HEAP_SLOTS_PER_MARK_SLOT);
	endSearchAt = OMR_MIN(endSearchAt, (uintptr_t*)sweepChunk->chunkTop);
	if (startSearchAt < endSearchAt) {
		while ( startSearchAt < endSearchAt ) {
			MM_HeapMapWordIterator nextMarkedObjectIterator(_currentMarkMap, startSearchAt);
			omrobjectptr_t nextObject = nextMarkedObjectIterator.nextObject();
			if (NULL != nextObject) {
				uintptr_t holeSize = (uintptr_t)nextObject - (uintptr_t)endOfPrevObject;
				if (holeSize < minimumFreeEntrySize) {
					darkMatter += holeSize;
				}
				break;
			}
			startSearchAt += J9MODRON_HEAP_SLOTS_PER_MARK_SLOT;
		}
	} else if (endSearchAt > endOfPrevObject) {
		darkMatter += (uintptr_t)endSearchAt - (uintptr_t)endOfPrevObject;
	}

	return darkMatter;
}

/**
 * Sweep the range of memory described by the chunk.
 * Given a properly initialized chunk (zeroed + subspace and range information) sweep the area to include
 * inner free list information as well as surrounding details (leading/trailing, projection, etc).  The routine
 * is self contained to run on an individual chunk, and does not rely on surrounding chunks having been processed.
 * @note a chunk can only be processed by a single thread (single threaded routine)
 * @return return TRUE if live objects found in chunk; FALSE otherwise
 */
bool
MM_ParallelSweepScheme::sweepChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk)
{
	uintptr_t *heapSlotFreeCurrent = NULL;  /* Current heap slot in chunk being processed */
	uintptr_t *markMapChunkBase = NULL;  /* Mark map base pointer for chunk */
	uintptr_t *markMapChunkTop = NULL;  /* Mark map top pointer for chunk */
	uintptr_t *markMapCurrent = NULL;  /* Current mark map slot being processed in chunk */
	uintptr_t *heapSlotFreeHead = NULL;  /* Heap free slot pointer for head */
	uintptr_t heapSlotFreeCount = 0;  /* Size in slots of heap free range */
	uintptr_t *markMapFreeHead = NULL;  /* Mark map free slots for head and tail */
	bool liveObjectFound = false; /* Set if we find at least one live object in chunk */
	MM_SweepPoolManager *sweepPoolManager = sweepChunk->memoryPool->getSweepPoolManager();

	/* Set up range limits */
	heapSlotFreeCurrent = (uintptr_t *)sweepChunk->chunkBase;

	markMapChunkBase = (uintptr_t *) (_currentSweepBits 
		+ (MM_Math::roundToFloor(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(uintptr_t), (uintptr_t)sweepChunk->chunkBase - (uintptr_t)_heapBase) / J9MODRON_HEAP_SLOTS_PER_MARK_SLOT) );
	markMapChunkTop = (uintptr_t *) (_currentSweepBits
		+ (MM_Math::roundToFloor(J9MODRON_HEAP_SLOTS_PER_MARK_SLOT * sizeof(uintptr_t), ((uintptr_t)sweepChunk->chunkTop) - ((uintptr_t)_heapBase)) / J9MODRON_HEAP_SLOTS_PER_MARK_SLOT) );
	markMapCurrent = markMapChunkBase;

	/* Be sure that chunk is just initialized */
	Assert_MM_true(NULL == sweepChunk->freeListTail);

	/* Process the leading free entry */
	heapSlotFreeHead = NULL;
	heapSlotFreeCount = 0;
	sweepMarkMapBody(markMapCurrent, markMapChunkTop, markMapFreeHead, heapSlotFreeCount, heapSlotFreeCurrent, heapSlotFreeHead);
	sweepMarkMapTail(markMapCurrent, markMapChunkTop, heapSlotFreeCount);
	if(heapSlotFreeCount) {
		/* Quick fixup if there was no full free map entries at the head */
		if(!heapSlotFreeHead) {
			heapSlotFreeHead = heapSlotFreeCurrent;
		}

		/* Chunk borders must be multiple of J9MODRON_HEAP_SLOTS_PER_MARK_SLOT */
		Assert_MM_true ((uintptr_t*)sweepChunk->chunkBase == heapSlotFreeHead);
		
		sweepPoolManager->addFreeMemory(env, sweepChunk, heapSlotFreeHead, heapSlotFreeCount);
	}

	/* If we get here and we have not processed the whole chunk then
	 * there must be at least one live object in the chunk.
	 */	
	if (markMapCurrent < markMapChunkTop) {
		liveObjectFound = true;
	}	

	uintptr_t darkMatterBytes = 0;
	uintptr_t darkMatterCandidates = 0;
	uintptr_t darkMatterSamples = 0;
	const UDATA darkMatterSampleRate = (0 == _extensions->darkMatterSampleRate)?UDATA_MAX:_extensions->darkMatterSampleRate;

	/* Process inner chunks */
	heapSlotFreeHead = NULL;
	heapSlotFreeCount = 0;
	while(markMapCurrent < markMapChunkTop) {
		/* Check if the map slot is part of a candidate free list entry */
		sweepMarkMapBody(markMapCurrent, markMapChunkTop, markMapFreeHead, heapSlotFreeCount, heapSlotFreeCurrent, heapSlotFreeHead);
		if (0 == heapSlotFreeCount) {
			darkMatterCandidates += 1;
			if (0 == (darkMatterCandidates % darkMatterSampleRate)) {
				darkMatterBytes += performSamplingCalculations(sweepChunk, markMapCurrent, heapSlotFreeCurrent);
				darkMatterSamples += 1;
			}
		} else {
			/* There is at least a single free slot in the mark map - check the head and tail */
			sweepMarkMapHead(markMapFreeHead, markMapChunkBase, heapSlotFreeHead, heapSlotFreeCount);
			sweepMarkMapTail(markMapCurrent, markMapChunkTop, heapSlotFreeCount);

			if (!sweepPoolManager->addFreeMemory(env, sweepChunk, heapSlotFreeHead, heapSlotFreeCount)) {
				break;
			}

			/* Reset the free entries for the next body */
			heapSlotFreeHead = NULL;
			heapSlotFreeCount = 0;
		}

		/* Proceed to the next map slot */
		heapSlotFreeCurrent += J9MODRON_HEAP_SLOTS_PER_MARK_SLOT;
		markMapCurrent += 1;
	}

	/* Process the trailing free entry - The body processing will handle trailing entries that cover a map slot or more */
	if(*(markMapCurrent - 1) != J9MODRON_OBM_SLOT_EMPTY) {
		heapSlotFreeCount = 0;
		heapSlotFreeHead = heapSlotFreeCurrent;
		sweepMarkMapHead(markMapCurrent, markMapChunkBase, heapSlotFreeHead, heapSlotFreeCount);

		/* Update the sweep chunk table entry with the trailing free information */
		sweepPoolManager->updateTrailingFreeMemory(env, sweepChunk, heapSlotFreeHead, heapSlotFreeCount);
	}
	
	if (darkMatterSamples == 0) {
		/* No samples were taken, so no dark matter was found (avoid division by zero) */
		sweepChunk->_darkMatterBytes = 0;
		sweepChunk->_darkMatterSamples =0;
	} else {
		Assert_MM_true(darkMatterCandidates >= darkMatterSamples);
		double projectionFactor = (double)darkMatterCandidates / (double)darkMatterSamples;
		UDATA projectedDarkMatter = (UDATA)((double)darkMatterBytes * projectionFactor);
		UDATA chunkSize = (UDATA)sweepChunk->chunkTop - (UDATA)sweepChunk->chunkBase;
		UDATA freeSpace = sweepChunk->freeBytes + sweepChunk->leadingFreeCandidateSize + sweepChunk->trailingFreeCandidateSize;
		Assert_MM_true(freeSpace <= chunkSize);
		sweepChunk->_darkMatterSamples = darkMatterSamples;

		if (projectedDarkMatter >= (chunkSize - freeSpace)) {
			/* We measured more than 100% dark matter in the sample.
			 * This can happen due to trailing dark matter and due to holes in some of the candidates.
			 * It usually only happens for very small samples, and will cause us to estimate more dark
			 * matter than is possible. In this case, don't apply the projection. Just record what we
			 * actually measured, since this is an accurate lower bound.
			 */
			sweepChunk->_darkMatterBytes = darkMatterBytes;
		} else {
			sweepChunk->_darkMatterBytes = projectedDarkMatter;
		}
	}

	return liveObjectFound;
}

/**
 * Prepare the chunk list for sweeping.
 * Given the current state of the heap, prepare the chunk list such that all entries are assign
 * a range of memory (and all memory has a chunk associated to it), sectioning the heap based on
 * the configuration, as well as configuration rules.
 * @return the total number of chunks in the system.
 */
uintptr_t
MM_ParallelSweepScheme::prepareAllChunks(MM_EnvironmentBase *env)
{
	return _sweepHeapSectioning->reassignChunks(env);
}

/**
 * Sweep all chunks.
 * 
 * @param totalChunkCount total number of chunks to be swept
 */
void
MM_ParallelSweepScheme::sweepAllChunks(MM_EnvironmentBase *env, uintptr_t totalChunkCount)
{
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uintptr_t chunksProcessed = 0; /* Chunks processed by this thread */
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	MM_ParallelSweepChunk *chunk = NULL;
	MM_ParallelSweepChunk *prevChunk = NULL;
	MM_SweepHeapSectioningIterator sectioningIterator(_sweepHeapSectioning);

	for (uintptr_t chunkNum = 0; chunkNum < totalChunkCount; chunkNum++) {
		
		chunk = sectioningIterator.nextChunk();
			
		Assert_MM_true (chunk != NULL);  /* Should never return NULL */
		
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)                           
			chunksProcessed += 1;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
 
 			/* if we are changing memory pool, flush the thread local stats to appropriate (previous) pool */
			if ((NULL != prevChunk) && (prevChunk->memoryPool != chunk->memoryPool)) {
				prevChunk->memoryPool->getLargeObjectAllocateStats()->getFreeEntrySizeClassStats()->mergeLocked(&env->_freeEntrySizeClassStats);
			}

 			/* if we are starting or changing memory pool, setup frequent allocation sizes in free entry stats for the pool we are about to sweep */
			if ((NULL == prevChunk) || (prevChunk->memoryPool != chunk->memoryPool)) {
				MM_MemoryPool *topLevelMemoryPool = chunk->memoryPool->getParent();
				if (NULL == topLevelMemoryPool) {
					topLevelMemoryPool = chunk->memoryPool;
				}
				env->_freeEntrySizeClassStats.initializeFrequentAllocation(topLevelMemoryPool->getLargeObjectAllocateStats());
			}
 
        	/* Sweep the chunk */
			sweepChunk(env, chunk);

			prevChunk = chunk;
		}	
	}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	env->_sweepStats.sweepChunksProcessed = chunksProcessed;
	env->_sweepStats.sweepChunksTotal = totalChunkCount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	
	/* flush the remaining stats (since the the last pool switch) */
	if (NULL != prevChunk) {
		prevChunk->memoryPool->getLargeObjectAllocateStats()->getFreeEntrySizeClassStats()->mergeLocked(&env->_freeEntrySizeClassStats);
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
MM_ParallelSweepScheme::connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk)
{
	MM_SweepPoolManager *sweepPoolManager = chunk->memoryPool->getSweepPoolManager();
	sweepPoolManager->connectChunk(env, chunk);
}

void
MM_ParallelSweepScheme::connectAllChunks(MM_EnvironmentBase *env, uintptr_t totalChunkCount)
{
	/* Initialize all sweep states for sweeping */
	initializeSweepStates(env);
	
	/* Walk the sweep chunk table connecting free lists */
	MM_ParallelSweepChunk *sweepChunk;
	MM_SweepHeapSectioningIterator sectioningIterator(_sweepHeapSectioning);

	for (uintptr_t chunkNum = 0; chunkNum < totalChunkCount; chunkNum++) {
		sweepChunk = sectioningIterator.nextChunk();
		Assert_MM_true(sweepChunk != NULL);  /* Should never return NULL */

		connectChunk(env, sweepChunk);
	}

	/* Walk all memory spaces flushing the previous free entry */
	flushAllFinalChunks(env);
}

/**
 * Initialize all global garbage collect sweep information in all memory pools
 * @note This routine should only be called once for every sweep cycle.
 */
void
MM_ParallelSweepScheme::initializeSweepStates(MM_EnvironmentBase *env)
{
	MM_MemoryPool *memoryPool;
	MM_HeapMemoryPoolIterator poolIterator(env, _extensions->heap);

	while(NULL != (memoryPool = poolIterator.nextPool())) {
		MM_SweepPoolState *sweepState;
		if(NULL != (sweepState = getPoolState(memoryPool))) {
			sweepState->initializeForSweep(env);
		}
	}
}

/**
 * Flush any unaccounted for free entries to the free list.
 */
void
MM_ParallelSweepScheme::flushFinalChunk(
	MM_EnvironmentBase *env,
	MM_MemoryPool *memoryPool)
{
	MM_SweepPoolManager *sweepPoolManager = memoryPool->getSweepPoolManager();
	sweepPoolManager->flushFinalChunk(env, memoryPool);
}

/**
 * Flush out any remaining free list data from the last chunk processed for every memory subspace.
 */
void
MM_ParallelSweepScheme::flushAllFinalChunks(MM_EnvironmentBase *env)
{
	MM_MemoryPool *memoryPool;
	MM_HeapMemoryPoolIterator poolIterator(env, _extensions->heap);

	while(NULL != (memoryPool = poolIterator.nextPool())) {
		MM_SweepPoolManager *sweepPoolManager = memoryPool->getSweepPoolManager();
		
		/* Find any unaccounted for free entries and flush them to the free list */
		sweepPoolManager->flushFinalChunk(env, memoryPool);
		sweepPoolManager->connectFinalChunk(env, memoryPool);
	}
}

void
MM_ParallelSweepScheme::allPoolsPostProcess(MM_EnvironmentBase *env)
{
	MM_MemoryPool *memoryPool;
	MM_HeapMemoryPoolIterator poolIterator(env, _extensions->heap);

	while(NULL != (memoryPool = poolIterator.nextPool())) {
		MM_SweepPoolManager *sweepPoolManager = memoryPool->getSweepPoolManager();
		
		sweepPoolManager->poolPostProcess(env, memoryPool);
	}
}

/**
 * Perform a basic sweep operation.
 * Called by all work threads from a task, performs a full sweep on all memory subspaces.
 * 
 * @note Do not call directly - used by the dispatcher for work threads.
 */
void
MM_ParallelSweepScheme::internalSweep(MM_EnvironmentBase *env)
{
	/* master thread does initialization */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		/* Reset largestFreeEntry of all subSpaces at beginning of sweep */
		_extensions->heap->resetLargestFreeEntry();
		
		_chunksPrepared = prepareAllChunks(env);
		
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	/* ..all threads now join in to do actual sweep */
	sweepAllChunks(env, _chunksPrepared);
	
	/* ..and then master thread finishes off by connecting all the chunks */
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		uint64_t mergeStartTime, mergeEndTime;
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
		
		mergeStartTime = omrtime_hires_clock();
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

		connectAllChunks(env, _chunksPrepared);

		_extensions->splitFreeListNumberChunksPrepared = _chunksPrepared;
		allPoolsPostProcess(env);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		mergeEndTime = omrtime_hires_clock();
		env->_sweepStats.addToMergeTime(mergeStartTime, mergeEndTime);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

/**
 * Perform a basic sweep operation.
 * There is no expectation for amount of work done for this routine.  The receiver is entitled to do as much or as little
 * work towards completing the sweep as it wants.  In this case, a full sweep of all memory spaces will be performed.
 * 
 * @note Expect to have the dispatcher and slave threads available for work
 * @note Expect to have exclusive access
 * @note Expect to have a valid mark map for all live objects
 */
void
MM_ParallelSweepScheme::sweep(MM_EnvironmentBase *env)
{
	setupForSweep(env);
	
	MM_ParallelSweepTask sweepTask(env, _extensions->dispatcher, this);
	_extensions->dispatcher->run(env, &sweepTask);
}

/**
 * Complete any sweep work after a basic sweep operation.
 * Completing the sweep is a noop - the basic sweep operation consists of a full sweep.
 * 
 * @note Expect to have the dispatcher and slave threads available for work
 * @note Expect to have exclusive access
 * @note Expect to have a valid mark map for all live objects
 * @note Expect basic sweep work to have been completed
 * 
 * @param Reason code to identify why completion of concurrent sweep is required.
 */
void
MM_ParallelSweepScheme::completeSweep(MM_EnvironmentBase *env, SweepCompletionReason reason)
{
	/* No work - basic sweep will have done all the work */
}

/**
 * Sweep and connect the heap until a free entry of the specified size is found.
 * Sweeping for a minimum size involves completing a full sweep (there is no incremental operation) then
 * evaluating whether the minimum free size was found.
 * 
 * @note Expects to have exclusive access
 * @note Expects to have control over the parallel GC threads (ie: able to dispatch tasks)
 * @note This only sweeps spaces that are concurrent sweepable
 * @return true if a free entry of at least the requested size was found, false otherwise.
 */
bool
MM_ParallelSweepScheme::sweepForMinimumSize(
	MM_EnvironmentBase *env,
	MM_MemorySubSpace *baseMemorySubSpace, 
	MM_AllocateDescription *allocateDescription)
{
	sweep(env);
	if (allocateDescription) {
		uintptr_t minimumFreeSize =  allocateDescription->getBytesRequested();
		return minimumFreeSize <= baseMemorySubSpace->findLargestFreeEntry(env, allocateDescription);
	} else { 
		return true;
	}	
}

#if defined(OMR_GC_CONCURRENT_SWEEP)
/**
 * Replenish a pools free lists to satisfy a given allocate.
 * The given pool was unable to satisfy an allocation request of (at least) the given size.  See if there is work
 * that can be done to increase the free stores of the pool so that the request can be met.
 * @note This call is made under the pools allocation lock (or equivalent)
 * @note The base implementation has completed all work, nothing further can be done.
 * @return True if the pool was replenished with a free entry that can satisfy the size, false otherwise.
 */
bool
MM_ParallelSweepScheme::replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, uintptr_t size)
{
	return false;
}
#endif /* OMR_GC_CONCURRENT_SWEEP */

void
MM_ParallelSweepScheme::setMarkMap(MM_MarkMap *markMap)
{
	_currentMarkMap = markMap;
	_currentSweepBits = (uint8_t *)_currentMarkMap->getMarkBits();
}
