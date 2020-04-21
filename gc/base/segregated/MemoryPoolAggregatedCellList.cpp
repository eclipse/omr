/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "GCExtensionsBase.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)
#include "HeapRegionDescriptorSegregated.hpp"
#include "SizeClasses.hpp"
#endif /* defined(OMR_GC_SEGREGATED_HEAP) */

#include "MemoryPoolAggregatedCellList.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

bool
MM_MemoryPoolAggregatedCellList::initialize(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region)
{
	if (!_lock.initialize(env, &env->getExtensions()->lnrlOptions, "MM_MemoryPoolAggregatedCellList:_lock")) {
		return false;
	}
	
	_region = region;
	
	return true;
}

/**
 * Pre allocates a list of cells within the region.
 * @param desiredBytes the desired amount of bytes to be pre-allocated
 * @param preAllocatedBytes a pointer to where the actual amount of pre-allocated bytes will be written to
 * @return the head of the pre-allocated list of cells
 */
uintptr_t*
MM_MemoryPoolAggregatedCellList::preAllocateCells(MM_EnvironmentBase* env, uintptr_t cellSize, uintptr_t desiredBytes, uintptr_t* preAllocatedBytes)
{
	uintptr_t desiredCellCount = desiredBytes / cellSize;
	uintptr_t adjustedDesiredBytes = desiredBytes;
	bool const compressed = compressObjectReferences();
	
	/* It's possible that the desiredBytes is less than the cellSize because the desiredBytes grows
	 * irrespective of the size class.
	 */
	if (0 == desiredCellCount) {
		desiredCellCount = 1;
		adjustedDesiredBytes = cellSize;
	}
	
	_lock.acquire();

	if (_heapCurrent == _heapTop) {
		/* The current chunk is empty, get the next one */
		refreshCurrentEntry();
	}
	
	uintptr_t* allocatedCellList = _heapCurrent;
	
	if ((uintptr_t)_heapTop - (uintptr_t)_heapCurrent > adjustedDesiredBytes) {
		/* Carve off the desired part */
		*preAllocatedBytes = desiredCellCount * cellSize;
		_heapCurrent = (uintptr_t *)((uintptr_t)_heapCurrent + desiredCellCount * cellSize);
		/* Make the remainder walkable */
		MM_HeapLinkedFreeHeader::fillWithHoles(_heapCurrent, (uintptr_t)_heapTop - (uintptr_t)_heapCurrent, compressed);
	} else {
		/* Take the whole free chunk */
		*preAllocatedBytes = (uintptr_t)_heapTop - (uintptr_t)_heapCurrent;
		refreshCurrentEntry();
	}
	
	addBytesAllocated(env, *preAllocatedBytes);
	_lock.release();

	return allocatedCellList;
}

/**
 * @todo Provide function documentation
 */
uintptr_t
MM_MemoryPoolAggregatedCellList::reset(MM_EnvironmentBase *env, uintptr_t sizeClass, void *lowAddress)
{
	uintptr_t numCells = env->getExtensions()->defaultSizeClasses->getNumCells(sizeClass);
	uintptr_t cellSize = env->getExtensions()->defaultSizeClasses->getCellSize(sizeClass);
	bool const compressed = compressObjectReferences();

	_freeListHead = NULL;
	MM_HeapLinkedFreeHeader *freeListEntry = MM_HeapLinkedFreeHeader::fillWithHoles((uintptr_t*)lowAddress, cellSize * numCells, compressed);
	MM_HeapLinkedFreeHeader::linkInAsHead((volatile uintptr_t *)(&_freeListHead), freeListEntry, compressed);
	resetCurrentEntry();

	return numCells;
}

void
MM_MemoryPoolAggregatedCellList::addBytesAllocated(MM_EnvironmentBase* env, uintptr_t bytesAllocated)
{
	/* Bytes allocated by large regions are counted when the regions are taken off the free region list,
	 * see emptyRegionAllocated
	 */

	/* Notify the allocation tracker of the bytes that have been allocated */
	env->_allocationTracker->addBytesAllocated(env, bytesAllocated);
	_preSweepFreeBytes -= bytesAllocated;
}

/**
 * DEBUG method which traverses the region and counts the amount of free bytes.
 * @note This is extremely inefficient but accurate.
 */
uintptr_t
MM_MemoryPoolAggregatedCellList::debugCountFreeBytes()
{
	uintptr_t freeBytes = 0;
	bool const compressed = compressObjectReferences();

	/* The region lock will prevent allocation, but not sweeping. */
	_lock.acquire();
	MM_HeapLinkedFreeHeader *chunk = _freeListHead;
	while (NULL != chunk) {
		freeBytes += chunk->getSize();
		chunk = chunk->getNext(compressed);
	}
	_lock.release();
	return freeBytes + (_heapTop - _heapCurrent);
}


void
MM_MemoryPoolAggregatedCellList::updateCounts(MM_EnvironmentBase *env, bool fromFlush)
{
	bool const compressed = compressObjectReferences();

	/* reserve the region so that nobody allocates while we flush */
	_lock.acquire();
	
	if (fromFlush && (_freeListHead == NULL && _heapCurrent == _heapTop)) {
		setFreeCount(0);
		_lock.release();
		return;
	}

	uintptr_t cellSize = _region->getCellSize();
	
	/* make the region walkable:
	 * 1) set the size of the current chunk
	 * 2) put the chunk back to the list
	 * 3) reset the current chunk pointers
	 */
	if (_heapCurrent < _heapTop) {
		MM_HeapLinkedFreeHeader *chunk = MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(_heapCurrent);
		chunk->setSize((uintptr_t)_heapTop - (uintptr_t)_heapCurrent);
		MM_HeapLinkedFreeHeader::linkInAsHead((volatile uintptr_t *)(&_freeListHead), chunk, compressed);
		_heapCurrent = _heapTop = (uintptr_t *)_freeListHead;
	}
	
	/* Update only free cell count :( */
	/* To be able to update mark/unmark count we would have to walk the whole region,
	 * which is not safe if this is called concurrently with allocation. */
	MM_HeapLinkedFreeHeader *chunk = _freeListHead;
	while (NULL != chunk) {
		addFreeCount(chunk->getSize() / cellSize);
		chunk = chunk->getNext(compressed);
	}

	_lock.release();
}

void 
MM_MemoryPoolAggregatedCellList::returnCell(MM_EnvironmentBase *env, uintptr_t *cell)
{
	bool const compressed = compressObjectReferences();

	/* reserve the region so that nobody allocates while we return this cell to the free list */
	_lock.acquire();

	MM_HeapLinkedFreeHeader *cellHeader = MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(cell);
	cellHeader->setSize(_region->getCellSize());
	MM_HeapLinkedFreeHeader::linkInAsHead((volatile uintptr_t *)(&_freeListHead), cellHeader, compressed);
	
	_lock.release();
}

#endif /* OMR_GC_SEGREGATED_HEAP */
