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

#include "SweepHeapSectioningSegmented.hpp"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "NonVirtualMemory.hpp"
#include "MemorySubSpace.hpp"
#include "ParallelSweepChunk.hpp"

/**
 * Return the expected total sweep chunks that will be used in the system.
 * Called during initialization, this routine looks at the maximum size of the heap and expected
 * configuration (generations, regions, etc) and determines the approximate maximum number of chunks
 * that will be required for a sweep at any given time.  It is safe to underestimate the number of chunks,
 * as the sweep sectioning mechnanism will compensate, but the the expectation is that by having all
 * chunk memory allocated in one go will keep the data localized and fragment system memory less.
 * @return estimated upper bound number of chunks that will be required by the system.
 */
uintptr_t
MM_SweepHeapSectioningSegmented::estimateTotalChunkCount(MM_EnvironmentBase *env)
{
	uintptr_t totalChunkCountEstimate;

	if(0 == _extensions->parSweepChunkSize) {
		/* -Xgc:sweepchunksize= has NOT been specified, so we set it heuristically.
		 *
		 *                  maxheapsize
		 * chunksize =   ----------------   (rounded up to the nearest 256k)
		 *               threadcount * 32
		 */
		_extensions->parSweepChunkSize = MM_Math::roundToCeiling(256*1024, _extensions->heap->getMaximumMemorySize() / (_extensions->dispatcher->threadCountMaximum() * 32));
	}

	totalChunkCountEstimate = MM_Math::roundToCeiling(_extensions->parSweepChunkSize, _extensions->heap->getMaximumMemorySize()) / _extensions->parSweepChunkSize;

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* Because object memory segments have not been allocated yet, we cannot get the real numbers.
	 * Assume that if the scavenger is enabled, each of the semispaces will need an extra chunk */
	/* TODO: Can we make an estimate based on the number of leaf memory subspaces allocated (if they are already allocated but not inflated)??? */
	if(_extensions->scavengerEnabled) {
		totalChunkCountEstimate += 2;
	}
#endif /* OMR_GC_MODRON_SCAVENGER */

	return totalChunkCountEstimate;
}

/**
 * Walk all segments and calculate the maximum number of chunks needed to represent the current heap.
 * The chunk calculation is done on a per segment basis (no segment can represent memory from more than 1 chunk),
 * and partial sized chunks (ie: less than the chunk size) are reserved for any remaining space at the end of a
 * segment.
 * @return number of chunks required to represent the current heap memory.
 */
uintptr_t
MM_SweepHeapSectioningSegmented::calculateActualChunkNumbers() const
{
	uintptr_t totalChunkCount = 0;

	MM_HeapRegionDescriptor *region;
	MM_Heap *heap = _extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);

	while((region = regionIterator.nextRegion()) != NULL) {
		if ((region)->isCommitted()) {
			/* TODO:  this must be rethought for Tarok since it treats all regions identically but some might require different sweep logic */
			MM_MemorySubSpace *subspace = region->getSubSpace();
			/* if this is a committed region, it requires a non-NULL subspace */
			Assert_MM_true(NULL != subspace);
			uintptr_t poolCount = subspace->getMemoryPoolCount();

			totalChunkCount += MM_Math::roundToCeiling(_extensions->parSweepChunkSize, region->getSize()) / _extensions->parSweepChunkSize;

			/* Add extra chunks if more than one memory pool */
			totalChunkCount += (poolCount - 1);
		}
	}

	return totalChunkCount;
}

/**
 * Reset and reassign each chunk to a range of heap memory.
 * Given the current updated listed of chunks and the corresponding heap memory, walk the chunk
 * list reassigning each chunk to an appropriate range of memory.  This will clear each chunk
 * structure and then assign its basic values that connect it to a range of memory (base/top,
 * pool, segment, etc).
 * @return the total number of chunks in the system.
 */
uintptr_t
MM_SweepHeapSectioningSegmented::reassignChunks(MM_EnvironmentBase *env)
{
	MM_ParallelSweepChunk *chunk; /* Sweep table chunk (global) */
	MM_ParallelSweepChunk *previousChunk;
	uintptr_t totalChunkCount;  /* Total chunks in system */

	MM_SweepHeapSectioningIterator sectioningIterator(this);

	totalChunkCount = 0;
	previousChunk = NULL;

	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;

	while (NULL != (region = regionIterator.nextRegion())) {
		if (region->isCommitted()) {
			/* TODO:  this must be rethought for Tarok since it treats all regions identically but some might require different sweep logic */
			uintptr_t *heapChunkBase = (uintptr_t *)region->getLowAddress();  /* Heap chunk base pointer */
			uintptr_t *regionHighAddress = (uintptr_t *)region->getHighAddress();

			while (heapChunkBase < regionHighAddress) {
				void *poolHighAddr;
				uintptr_t *heapChunkTop;
				MM_MemoryPool *pool;

				chunk = sectioningIterator.nextChunk();
				Assert_MM_true(chunk != NULL);  /* Should never return NULL */
				totalChunkCount += 1;

				/* Clear all data in the chunk (including sweep implementation specific information) */
				chunk->clear();

				if(((uintptr_t)regionHighAddress - (uintptr_t)heapChunkBase) < _extensions->parSweepChunkSize) {
					/* corner case - we will wrap our address range */
					heapChunkTop = regionHighAddress;
				} else {
					/* normal case - just increment by the chunk size */
					heapChunkTop = (uintptr_t *)((uintptr_t)heapChunkBase + _extensions->parSweepChunkSize);
				}

				/* Find out if the range of memory we are considering spans 2 different pools.  If it does,
				 * the current chunk can only be attributed to one, so we limit the upper range of the chunk
				 * to the first pool and will continue the assignment at the upper address range.
				 */
				pool = region->getSubSpace()->getMemoryPool(env, heapChunkBase, heapChunkTop, poolHighAddr);
				if (NULL == poolHighAddr) {
					heapChunkTop = (heapChunkTop > regionHighAddress ? regionHighAddress : heapChunkTop);
				} else {
					/* Yes ..so adjust chunk boundaries */
					assume0(poolHighAddr > heapChunkBase && poolHighAddr < heapChunkTop);
					heapChunkTop = (uintptr_t *) poolHighAddr;
				}

				/* All values for the chunk have been calculated - assign them */
				chunk->chunkBase = (void *)heapChunkBase;
				chunk->chunkTop = (void *)heapChunkTop;
				chunk->memoryPool = pool;
				chunk->_coalesceCandidate = (heapChunkBase != region->getLowAddress());
				chunk->_previous= previousChunk;
				if(NULL != previousChunk) {
					previousChunk->_next = chunk;
				}

				/* Move to the next chunk */
				heapChunkBase = heapChunkTop;

				/* and remember address of previous chunk */
				previousChunk = chunk;

				assume0((uintptr_t)heapChunkBase == MM_Math::roundToCeiling(_extensions->heapAlignment,(uintptr_t)heapChunkBase));
			}
		}
	}

	if(NULL != previousChunk) {
		previousChunk->_next = NULL;
	}

	return totalChunkCount;
}

/**
 * Allocate and initialize a new instance of the receiver.
 * @return pointer to the new instance.
 */
MM_SweepHeapSectioningSegmented *
MM_SweepHeapSectioningSegmented::newInstance(MM_EnvironmentBase *env)
{
	MM_SweepHeapSectioningSegmented *sweepHeapSectioning = (MM_SweepHeapSectioningSegmented *)env->getForge()->allocate(sizeof(MM_SweepHeapSectioningSegmented), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (sweepHeapSectioning) {
		new(sweepHeapSectioning) MM_SweepHeapSectioningSegmented(env);
		if (!sweepHeapSectioning->initialize(env)) {
			sweepHeapSectioning->kill(env);
			return NULL;
		}
	}
	return sweepHeapSectioning;
}
