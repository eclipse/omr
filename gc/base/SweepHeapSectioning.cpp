/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#include "SweepHeapSectioning.hpp"

#include "EnvironmentBase.hpp"
#include "MemoryManager.hpp"
#include "MemorySubSpace.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepHeapSectioning.hpp"

#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "MemoryPool.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "SweepPoolManager.hpp"
#include "ParallelDispatcher.hpp"

/**
 * Internal storage memory pool for heap sectioning chunks.
 * 
 * @ingroup GC_Base
 */
class MM_ParallelSweepChunkArray : public MM_BaseVirtual {
private:
	MM_ParallelSweepChunk* _array; /**< backing store for chunks */
	uintptr_t _used; /**< number of array elements used */
	uintptr_t _size; /**< total array elements available */
	MM_ParallelSweepChunkArray* _next; /**< next pointer in array list */
	MM_MemoryHandle _memoryHandle; /**< memory handle for array backing store */
	bool _useVmem; /**< if true the Virtual Memory instance is allocated */

protected:
	bool initialize(MM_EnvironmentBase* env, bool useVmem);
	void tearDown(MM_EnvironmentBase* env);

public:
	static MM_ParallelSweepChunkArray* newInstance(MM_EnvironmentBase* env, uintptr_t size, bool useVmem);
	void kill(MM_EnvironmentBase* env);

	MM_ParallelSweepChunkArray(uintptr_t size)
		: MM_BaseVirtual()
		, _array(NULL)
		, _used(0)
		, _size(size)
		, _next(NULL)
		, _memoryHandle()
		, _useVmem(false)
	{
		_typeId = __FUNCTION__;
	}

	friend class MM_SweepHeapSectioning;
	friend class MM_SweepHeapSectioningIterator;
};

/**
 * Allocate and initialize the receivers internal structures.
 * @return true on success, false on failure.
 */
bool
MM_ParallelSweepChunkArray::initialize(MM_EnvironmentBase* env, bool useVmem)
{
	bool result = false;
	MM_GCExtensionsBase* extensions = env->getExtensions();

	_useVmem = useVmem;

	if (extensions->isFvtestForceSweepChunkArrayCommitFailure()) {
		Trc_MM_SweepHeapSectioning_parallelSweepChunkArrayCommitFailureForced(env->getLanguageVMThread());
	} else {
		if (useVmem) {
			MM_MemoryManager* memoryManager = extensions->memoryManager;
			if (memoryManager->createVirtualMemoryForMetadata(env, &_memoryHandle, extensions->heapAlignment, _size * sizeof(MM_ParallelSweepChunk))) {
				void* base = memoryManager->getHeapBase(&_memoryHandle);
				result = memoryManager->commitMemory(&_memoryHandle, base, _size * sizeof(MM_ParallelSweepChunk));
				if (!result) {
					Trc_MM_SweepHeapSectioning_parallelSweepChunkArrayCommitFailed(env->getLanguageVMThread(), base, _size * sizeof(MM_ParallelSweepChunk));
				}
				_array = (MM_ParallelSweepChunk*)base;
			}
		} else {
			if (0 != _size) {
				_array = (MM_ParallelSweepChunk*)env->getForge()->allocate(_size * sizeof(MM_ParallelSweepChunk), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
				result = (NULL != _array);
			} else {
				result = true;
			}
		}
	}
	return result;
}

/**
 * Free the receivers internal structures.
 */
void
MM_ParallelSweepChunkArray::tearDown(MM_EnvironmentBase* env)
{
	if (_useVmem) {
		MM_GCExtensionsBase* extensions = env->getExtensions();
		MM_MemoryManager* memoryManager = extensions->memoryManager;
		memoryManager->destroyVirtualMemory(env, &_memoryHandle);
	} else {
		env->getForge()->free((void*)_array);
	}
	_array = (MM_ParallelSweepChunk*)NULL;
}

/**
 * Create a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_ParallelSweepChunkArray*
MM_ParallelSweepChunkArray::newInstance(MM_EnvironmentBase* env, uintptr_t size, bool useVmem)
{
	MM_ParallelSweepChunkArray* array;

	array = (MM_ParallelSweepChunkArray*)env->getForge()->allocate(sizeof(MM_ParallelSweepChunkArray), OMR::GC::AllocationCategory::OTHER, OMR_GET_CALLSITE());
	if (NULL != array) {
		new (array) MM_ParallelSweepChunkArray(size);
		if (!array->initialize(env, useVmem)) {
			array->kill(env);
			return NULL;
		}
	}
	return array;
}

/**
 * Free all memory associated to the receiver.
 */
void
MM_ParallelSweepChunkArray::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Get the next chunk from the sectioning iterator.
 * @return a chunk, or NULL if the end of the chunk list has been reached.
 */
MM_ParallelSweepChunk*
MM_SweepHeapSectioningIterator::nextChunk()
{
	while (NULL != _currentArray) {
		if (_currentIndex < _currentArray->_used) {
			return _currentArray->_array + _currentIndex++;
		}

		/* End of the current array - move to the next one */
		_currentArray = _currentArray->_next;
		_currentIndex = 0;
	}

	return NULL;
}

/**
 * Free all memory associated to the receiver.
 */
void
MM_SweepHeapSectioning::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Allocate and initialize the receiver's internal structures.
 * @return true on success, false on failure.
 */
bool
MM_SweepHeapSectioning::initialize(MM_EnvironmentBase* env)
{
	uintptr_t totalChunkCountEstimate;

	totalChunkCountEstimate = estimateTotalChunkCount(env);

	/* Allocate the lead array to see if the initial backing store can be allocated */
	_head = MM_ParallelSweepChunkArray::newInstance(env, totalChunkCountEstimate, true);
	if (NULL == _head) {
		return false;
	}

	/* Save away the initial array for other uses (routines that need its backing store, such as compact) */
	_baseArray = _head;

	/* Make sure we record the total size currently allocated */
	_totalSize = totalChunkCountEstimate;

	return true;
}

/**
 * Free the receivers internal structures.
 */
void
MM_SweepHeapSectioning::tearDown(MM_EnvironmentBase* env)
{
	MM_ParallelSweepChunkArray* array, *nextArray;

	array = _head;
	while (array) {
		nextArray = array->_next;
		array->kill(env);
		array = nextArray;
	}
	_head = NULL;
}

/**
 * Reserve the given number of chunks.
 * Walk all arrays in the receiver reserving the requested number of chunks.  Any arrays in excess will have their
 * used count set to 0.
 * @param chunkCount Number of chunks to be reserved in the receiver.
 * @return true if the receiver successfully reserved the chunks, false otherwise.
 */
bool
MM_SweepHeapSectioning::initArrays(uintptr_t chunkCount)
{
	/* Note that we can't use the iterator here since its used counts are incorrect (may skip arrays if they
	 * are currently empty
	 */
	uintptr_t remainingChunkCount = chunkCount;
	MM_ParallelSweepChunkArray* array;

	array = _head;
	while (0 != remainingChunkCount) {
		/* trying to reserve too much? */
		if (NULL == array) {
			return false;
		}

		/* set the actual length of the array to its max, unless it is the last array */
		if (remainingChunkCount > array->_size) {
			array->_used = array->_size;
		} else {
			/* last array */
			array->_used = remainingChunkCount;
		}

		remainingChunkCount -= array->_used;
		array = array->_next;
	}

	/* If there are any remaining arrays in current allocated by the sectioning iterator, set their
	 * used sizes to NULL
	 */
	while (NULL != array) {
		array->_used = 0;
		array = array->_next;
	}

	return true;
}

/**
 * Update the sectioning data to reflect the current heap size and shape.
 * @return true if the receiver successfully reserved enough chunks to represent the heap, false otherwise.
 */
bool
MM_SweepHeapSectioning::update(MM_EnvironmentBase* env)
{
	uintptr_t totalChunkCount;

	totalChunkCount = calculateActualChunkNumbers();

	/* Check if we've exceeded our current physical capacity to reserve chunks */
	if (totalChunkCount > _totalSize) {
		/* Insufficient room - reserve more memory for chunks */
		MM_ParallelSweepChunkArray* newArray;

		/* TODO: Do we want to round the number of chunks allocated to something sane? */
		newArray = MM_ParallelSweepChunkArray::newInstance(env, totalChunkCount - _totalSize, false);
		if (NULL == newArray) {
			return false;
		}

		/* clear chunks */
		MM_ParallelSweepChunk* chunk;
		for(uintptr_t count=0; count<newArray->_size; count++) {
			chunk = newArray->_array + count;
			chunk->clear();
		}

		/* link the new array into the list of arrays */
		newArray->_next = _head;
		_head = newArray;

		/* set the actual number of chunks used */
		_totalUsed = totalChunkCount;
		_totalSize = totalChunkCount;

	} else {
		/* Sufficient room - reserve the chunks */
		_totalUsed = totalChunkCount;
	}

	/* Walk the arrays initializing their used lengths to account for new totals */
	return initArrays(totalChunkCount);
}

/**
 * Find and return the backing store addresses base.
 * This routine uses the backing store of the base array and uses this memory as the return value.
 * @return base address of the backing store.
 */
void*
MM_SweepHeapSectioning::getBackingStoreAddress()
{
	MM_MemoryManager* memoryManager = _extensions->memoryManager;
	return (void*)memoryManager->getHeapBase(&_baseArray->_memoryHandle);
}

/**
 * Find and return the size of the backing store.
 * This routine uses the backing store of the base array and uses this memory as the return value.
 * @return size of the backing store.
 */
uintptr_t
MM_SweepHeapSectioning::getBackingStoreSize()
{
	return _baseArray->_used * sizeof(MM_ParallelSweepChunk);
}

/**
 * Return the expected total sweep chunks that will be used in the system.
 * Called during initialization, this routine looks at the maximum size of the heap and expected
 * configuration (generations, regions, etc) and determines the approximate maximum number of chunks
 * that will be required for a sweep at any given time.  It is safe to underestimate the number of chunks,
 * as the sweep sectioning mechanism will compensate, but the expectation is that by having all
 * chunk memory allocated in one go will keep the data localized and fragment system memory less.
 * @return estimated upper bound number of chunks that will be required by the system.
 */
uintptr_t
MM_SweepHeapSectioning::estimateTotalChunkCount(MM_EnvironmentBase *env)
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

	return totalChunkCountEstimate;
}

uintptr_t
MM_SweepHeapSectioning::reassignChunks(MM_EnvironmentBase *env)
{
	MM_ParallelSweepChunk *chunk; /* Sweep table chunk (global) */
	MM_ParallelSweepChunk *previousChunk = NULL;
	uintptr_t totalChunkCount = 0;  /* Total chunks in system */

	MM_SweepHeapSectioningIterator sectioningIterator(this);

	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	MM_HeapRegionDescriptor *region = NULL;

	while (NULL != (region = regionIterator.nextRegion())) {
		if (isReadyToSweep(env, region)) {
			uintptr_t *heapChunkBase = (uintptr_t *)region->getLowAddress();  /* Heap chunk base pointer */
			uintptr_t *regionHighAddress = (uintptr_t *)region->getHighAddress();

			while (heapChunkBase < regionHighAddress) {
				void *poolHighAddr = NULL;
				uintptr_t *heapChunkTop = NULL;

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
				MM_MemoryPool *pool = region->getSubSpace()->getMemoryPoolForSweep(env, heapChunkBase, heapChunkTop, poolHighAddr);
				if (NULL == poolHighAddr) {
					heapChunkTop = (heapChunkTop > regionHighAddress ? regionHighAddress : heapChunkTop);
				} else {
					/* Yes ..so adjust chunk boundaries */
					Assert_MM_true(poolHighAddr > heapChunkBase && poolHighAddr < heapChunkTop);
					heapChunkTop = (uintptr_t *) poolHighAddr;
				}

				/* All values for the chunk have been calculated - assign them */
				chunk->chunkBase = (void *)heapChunkBase;
				chunk->chunkTop = (void *)heapChunkTop;
				chunk->memoryPool = pool;
				Assert_MM_true(NULL != pool);
				/* Some memory pools, like the one in LOA, may have larger min free size then in the rest of the heap being swept */
				chunk->_minFreeSize = OMR_MAX(pool->getMinimumFreeEntrySize(), pool->getSweepPoolManager()->getMinimumFreeSize());

				chunk->_coalesceCandidate = (heapChunkBase != region->getLowAddress());
				chunk->_previous= previousChunk;
				if(NULL != previousChunk) {
					previousChunk->_next = chunk;
				}

				/* Move to the next chunk */
				heapChunkBase = heapChunkTop;

				/* and remember address of previous chunk */
				previousChunk = chunk;

				Assert_MM_true((uintptr_t)heapChunkBase == MM_Math::roundToCeiling(_extensions->heapAlignment,(uintptr_t)heapChunkBase));
			}
		}
	}

	if(NULL != previousChunk) {
		previousChunk->_next = NULL;
	}

	return totalChunkCount;
}

/* For a given address determine the sweep chunk boundaries that contains it. This is the expected chunk that's assigned during heap sectioning (reassignChunks).*/
void
MM_SweepHeapSectioning::getChunkBoundariesForAddr(MM_EnvironmentBase *env,  void* addrToCheck, void * &sweepChunkBaseBoundary, void * &sweepChunkTopBoundary)
{
	MM_HeapRegionManager *regionManager = _extensions->getHeap()->getHeapRegionManager();
	MM_HeapRegionDescriptor *region = regionManager->regionDescriptorForAddress(addrToCheck);;

	sweepChunkBaseBoundary = NULL;
	sweepChunkTopBoundary = NULL;

	uintptr_t *regionBase = (uintptr_t *)region->getLowAddress();
	uintptr_t *regionTop = (uintptr_t *)region->getHighAddress();

	Assert_MM_true(addrToCheck >= regionBase);
	Assert_MM_true(addrToCheck < regionTop);

	uintptr_t remainder = ((uintptr_t)addrToCheck - (uintptr_t)regionBase) % _extensions->parSweepChunkSize;
	sweepChunkBaseBoundary = (void *) ((uintptr_t) addrToCheck - remainder);
	sweepChunkTopBoundary = (void *) ((uintptr_t) sweepChunkBaseBoundary + _extensions->parSweepChunkSize);

	/* Find out if the range of memory we are considering spans 2 different pools.  If it does,
	 * the current chunk can only be attributed to one, so we limit the upper range of the chunk.
	 */
	void *poolHighAddr = NULL;
	region->getSubSpace()->getMemoryPool(env, sweepChunkBaseBoundary, sweepChunkTopBoundary, poolHighAddr);
	if (NULL == poolHighAddr) {
		sweepChunkTopBoundary = (sweepChunkTopBoundary > regionTop ? regionTop : sweepChunkTopBoundary);
	} else {
		Assert_MM_true(poolHighAddr > sweepChunkBaseBoundary && poolHighAddr < sweepChunkTopBoundary);
		sweepChunkTopBoundary = (uintptr_t *) poolHighAddr;
	}
}
