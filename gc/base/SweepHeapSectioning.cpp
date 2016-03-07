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

#include "SweepHeapSectioning.hpp"

#include "EnvironmentBase.hpp"
#include "MemoryManager.hpp"
#include "MemorySubSpace.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepHeapSectioning.hpp"

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
				_array = (MM_ParallelSweepChunk*)env->getForge()->allocate(_size * sizeof(MM_ParallelSweepChunk), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
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

	array = (MM_ParallelSweepChunkArray*)env->getForge()->allocate(sizeof(MM_ParallelSweepChunkArray), MM_AllocationCategory::OTHER, OMR_GET_CALLSITE());
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
