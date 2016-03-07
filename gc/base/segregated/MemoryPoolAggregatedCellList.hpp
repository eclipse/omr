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

#if !defined(MEMORYPOOLAGGREGATEDCELLLIST_HPP)
#define MEMORYPOOLAGGREGATEDCELLLIST_HPP

#include "omrcfg.h"
#include "sizeclasses.h"

#include "Base.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemoryPool.hpp"
#include "SegregatedAllocationTracker.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_HeapRegionDescriptorSegregated;
class MM_MarkMap;

class MM_MemoryPoolAggregatedCellList : public MM_MemoryPool
{
	/*
	 * Data members
	 */
private:
protected:
	MM_HeapLinkedFreeHeader *volatile _freeListHead; /**< Pointer to the first entry in the free list, NULL means there are no free entries in this region */
	uintptr_t *volatile _heapCurrent;
	uintptr_t *volatile _heapTop;

	MM_LightweightNonReentrantLock _lock; /**< Lock used to ensure cell list is maintained safely */	
	MM_HeapRegionDescriptorSegregated *_region;
	
	/* _freeCount + _markCount + "dark matter" = number of cells on region after the sweep */
	uintptr_t _markCount;
	uintptr_t _freeCount;

	uintptr_t _preSweepFreeBytes;

public:

	/*
	 * Function members
	 */
private:
protected:
public:

	MM_MemoryPoolAggregatedCellList(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize) :
		MM_MemoryPool(env, minimumFreeEntrySize)
		,_freeListHead(NULL)
		,_heapCurrent(NULL)
		,_heapTop(NULL)
		,_region(NULL)
		,_markCount(0)
		,_freeCount(0)
		,_preSweepFreeBytes(0)
	{
		_typeId = __FUNCTION__;
	}
	
	using MM_MemoryPool::initialize;
	bool initialize(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region);

	void tearDown(MM_EnvironmentBase *env) {
		_lock.tearDown();
	}

	void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription) { return NULL;}
	void expandWithRange(MM_EnvironmentBase *env, uintptr_t expandSize, void *lowAddress, void *highAddress, bool canCoalesce) {}
	void *contractWithRange(MM_EnvironmentBase *env, uintptr_t contractSize, void *lowAddress, void *highAddress) { return NULL; }
	bool abandonHeapChunk(void *addrBase, void *addrTop) { return true; }
	
	MMINLINE void
	addFreeChunk(uintptr_t *freeChunk, uintptr_t freeChunkSize, uintptr_t freeChunkCellCount)
	{
		MM_HeapLinkedFreeHeader *chunk = MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(freeChunk);
		chunk->setSize(freeChunkSize);
		MM_HeapLinkedFreeHeader::linkInAsHead((volatile uintptr_t *)(&_freeListHead), chunk);
		addFreeCount(freeChunkCellCount);
	}

	MMINLINE void addFreeCount(uintptr_t count) { _freeCount += count; }
	MMINLINE void incrementFreeCount() { _freeCount++;	}
	MMINLINE void resetFreeCount() { _freeCount = 0; }
	MMINLINE void setFreeCount(uintptr_t count) { _freeCount = count; }
	MMINLINE uintptr_t getFreeCount() { return _freeCount; }
	MMINLINE void resetMarkCount() { _markCount = 0; }
	MMINLINE void setMarkCount(uintptr_t markCount) { _markCount = markCount; }
	MMINLINE uintptr_t getMarkCount() { return _markCount; }
	MMINLINE void resetCounts() { resetMarkCount();	resetFreeCount(); }
	MMINLINE void resetFreeList() { _freeListHead = NULL; }

	void updateCounts(MM_EnvironmentBase *env, bool fromFlush);
	
	/**
	 * Return the cell to the free list
	 */ 
	void returnCell(MM_EnvironmentBase *env, uintptr_t *cell);
	MMINLINE bool hasCell() { return (_freeListHead != NULL) || (_heapCurrent < _heapTop); }
	uintptr_t* preAllocateCells(MM_EnvironmentBase* env, uintptr_t cellSize, uintptr_t desiredBytes, uintptr_t* preAllocatedBytesOutput);
	void addBytesAllocated(MM_EnvironmentBase* env, uintptr_t bytesAllocated);
	uintptr_t debugCountFreeBytes();
	
	MMINLINE uintptr_t getPreSweepFreeBytes() { return _preSweepFreeBytes; }
	MMINLINE void setPreSweepFreeBytes(uintptr_t preSweepFreeBytes) { _preSweepFreeBytes = preSweepFreeBytes; }
	MMINLINE void addSweepFreeBytes(MM_EnvironmentBase *env, uintptr_t sweepFreeBytes)
	{
		env->_allocationTracker->addBytesFreed(env, sweepFreeBytes);
		_preSweepFreeBytes += sweepFreeBytes;
	}


	/**
	 * Grab a new free list entry and start using that as the current heap chunk.
	 * NOTE: This function assumes the region lock is currently held!
	 */
	MMINLINE void refreshCurrentEntry()
	{
		/* NOTE: The region lock must be held by the caller! */
		if (NULL != _freeListHead) {
			_heapCurrent = (uintptr_t *)_freeListHead;
			_heapTop = (uintptr_t *)((uintptr_t)_heapCurrent + _freeListHead->getSize());
			_freeListHead = _freeListHead->getNext();
		} else {
			_heapCurrent = NULL;
			_heapTop = NULL;
		}
	}

	void resetCurrentEntry() { _heapCurrent = _heapTop = (uintptr_t *)_freeListHead; }
	using MM_MemoryPool::reset;
	uintptr_t reset(MM_EnvironmentBase *env, uintptr_t sizeClass, void *lowAddress);
	void setRegion(MM_HeapRegionDescriptorSegregated *region) {_region = region;}
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* MEMORYPOOLAGGREGATEDCELLLIST_HPP */
