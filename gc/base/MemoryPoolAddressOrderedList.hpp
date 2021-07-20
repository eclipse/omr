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

#if !defined(MEMORYPOOLADDRESSORDEREDLIST_HPP_)
#define MEMORYPOOLADDRESSORDEREDLIST_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronopt.h"

#include "HeapLinkedFreeHeader.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemoryPoolAddressOrderedListBase.hpp"
#include "HeapRegionDescriptor.hpp"
#include "EnvironmentBase.hpp"
#include "AtomicOperations.hpp"

class MM_AllocateDescription;
#if defined(OMR_GC_CONCURRENT_SWEEP)
class MM_ConcurrentSweepScheme;
#endif /* OMR_GC_CONCURRENT_SWEEP */

#define FREE_ENTRY_END ((MM_HeapLinkedFreeHeader *)OMRPORT_VMEM_MAX_ADDRESS)

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_MemoryPoolAddressOrderedList : public MM_MemoryPoolAddressOrderedListBase
{
/*
 * Data members
 */
private:
	/* Basic free list support */
	MM_LightweightNonReentrantLock _heapLock;
	MM_HeapLinkedFreeHeader *_heapFreeList;
	
	/* Hint support */
	struct J9ModronAllocateHint* _hintActive;
	struct J9ModronAllocateHint* _hintInactive;
	struct J9ModronAllocateHint _hintStorage[HINT_ELEMENT_COUNT];
	uintptr_t _hintLru;
	
	MM_LargeObjectAllocateStats *_largeObjectCollectorAllocateStats;  /**< Same as _largeObjectAllocateStats except specifically for collector allocates */

	MM_HeapLinkedFreeHeader *_firstUnalignedFreeEntry; /**< it is only for Balanced GC copyforward and non empty survivor region */
	MM_HeapLinkedFreeHeader *_prevFirstUnalignedFreeEntry;
protected:
public:
	
/*
 * Function members
 */	
private:
	void addHint(MM_HeapLinkedFreeHeader *freeEntry, uintptr_t lookupSize);
	J9ModronAllocateHint *findHint(uintptr_t lookupSize);
	void removeHint(MM_HeapLinkedFreeHeader *freeEntry);
	void updateHint(MM_HeapLinkedFreeHeader *oldFreeEntry, MM_HeapLinkedFreeHeader *newFreeEntry);
	void clearHints();
	void updateHintsBeyondEntry(MM_HeapLinkedFreeHeader *freeEntry);
	void *internalAllocate(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats);
	bool internalAllocateTLH(MM_EnvironmentBase *env, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats);

	MMINLINE bool doesNeedAlignment(MM_EnvironmentBase *env, MM_HeapLinkedFreeHeader *freeEntry)
	{
#if defined(OMR_ENV_DATA64)
		return (freeEntry >= _firstUnalignedFreeEntry);
#else
		return false;
#endif /* OMR_ENV_DATA64 */
	}

	/**
	 * Just aligns entries from _firstUnalignedFreeEntry to lastFreeEntryToAlign (Therefore, it incrementally aligns the pool, as we progress with allocation)
	 * It does not make decisions which free entry to use to satisfy the allocate (the caller does it), and therefore does not try to find an alternate free entry if the aligned variant is not big enough
	 * @param lastFreeEntryToAlign the last FreeEntry to be aligned for this call
	 * @return aligned variant of lastFreeEntryToAlign, or null if its aligned variant is not big enough
	 */
	MM_HeapLinkedFreeHeader *doFreeEntryAlignmentUpTo(MM_EnvironmentBase *env, MM_HeapLinkedFreeHeader *lastFreeEntryToAlign);

protected:
public:
	static MM_MemoryPoolAddressOrderedList *newInstance(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize); 
	static MM_MemoryPoolAddressOrderedList *newInstance(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize, const char *name);

	virtual void lock(MM_EnvironmentBase *env);
	virtual void unlock(MM_EnvironmentBase *env);
	
	virtual void *allocateObject(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription);
	virtual void *allocateTLH(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop);
	virtual void *collectorAllocate(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool lockingRequired);
	virtual void *collectorAllocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired);
		
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual void reset(Cause cause = any);
	virtual MM_HeapLinkedFreeHeader *rebuildFreeListInRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, MM_HeapLinkedFreeHeader *previousFreeEntry);

#if defined(DEBUG)
	virtual bool isValidListOrdering();
#endif

	virtual void addFreeEntries(MM_EnvironmentBase *env, MM_HeapLinkedFreeHeader* &freeListHead, MM_HeapLinkedFreeHeader* &freeListTail,
												uintptr_t freeListMemoryCount, uintptr_t freeListMemorySize);
	
#if defined(OMR_GC_LARGE_OBJECT_AREA)
	virtual bool removeFreeEntriesWithinRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress,uintptr_t minimumSize,
														 MM_HeapLinkedFreeHeader* &retListHead, MM_HeapLinkedFreeHeader* &retListTail,
														 uintptr_t &retListMemoryCount, uintptr_t &retListMemorySize);
	virtual void *findAddressAfterFreeSize(MM_EnvironmentBase *env, uintptr_t sizeRequired, uintptr_t minimumSize);
#endif	
	virtual void expandWithRange(MM_EnvironmentBase *env, uintptr_t expandSize, void *lowAddress, void *highAddress, bool canCoalesce);
	virtual void *contractWithRange(MM_EnvironmentBase *env, uintptr_t contractSize, void *lowAddress, void *highAddress);

	bool recycleHeapChunk(MM_EnvironmentBase *env, void* chunkBase, void* chunkTop);
	bool recycleHeapChunk(void *addrBase, void *addrTop, MM_HeapLinkedFreeHeader *previousFreeEntry, MM_HeapLinkedFreeHeader *nextFreeEntry);

	virtual void *findFreeEntryEndingAtAddr(MM_EnvironmentBase *env, void *addr);
	virtual uintptr_t getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, void *lowAddr, void *highAddr);
	virtual void *findFreeEntryTopStartingAtAddr(MM_EnvironmentBase *env, void *addr);
	virtual void *getFirstFreeStartingAddr(MM_EnvironmentBase *env);
	virtual void *getNextFreeStartingAddr(MM_EnvironmentBase *env, void *currentFree);

	virtual void moveHeap(MM_EnvironmentBase *env, void *srcBase, void *srcTop, void *dstBase);
	
#if defined(DEBUG)	
	bool isMemoryPoolValid(MM_EnvironmentBase *env, bool postCollect);
	uintptr_t getCurrentLargestFree(MM_EnvironmentBase *env);
	uintptr_t getCurrentFreeMemorySize(MM_EnvironmentBase *env);
#endif /* DEBUG */	

	virtual void  printCurrentFreeList(MM_EnvironmentBase *env, const char *area);
	
	virtual void appendCollectorLargeAllocateStats();

	virtual void mergeFreeEntryAllocateStats() {_largeObjectAllocateStats->getFreeEntrySizeClassStats()->mergeCountForVeryLargeEntries();}
	
	virtual bool initializeSweepPool(MM_EnvironmentBase *env);
	
	/**
	 * Recalculate the memory pool statistics by actually examining the contents of the pool.
	 */
	virtual void recalculateMemoryPoolStatistics(MM_EnvironmentBase *env);

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	virtual uintptr_t releaseFreeMemoryPages(MM_EnvironmentBase* env);
#endif

	/**
	 * remove a free entry from freelist
	 */
	void removeFromFreeList(void *addrBase, void *addrTop, MM_HeapLinkedFreeHeader *previousFreeEntry, MM_HeapLinkedFreeHeader *nextFreeEntry)
	{
		bool const compressed = compressObjectReferences();
		uintptr_t freeEntrySize = ((uintptr_t)addrTop) - ((uintptr_t)addrBase);
		MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, freeEntrySize, compressed);
		if (previousFreeEntry) {
			previousFreeEntry->setNext(nextFreeEntry, compressed);
		}else {
			_heapFreeList = nextFreeEntry;
		}
	}

	void fillWithHoles(void *addrBase, void *addrTop)
	{
		bool const compressed = compressObjectReferences();
		MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, ((uintptr_t)addrTop) - ((uintptr_t)addrBase), compressed);
	}

	MMINLINE uintptr_t getAdjustedBytesForCardAlignment()
	{
		return _adjustedBytesForCardAlignment;
	}

	MMINLINE void setAdjustedBytesForCardAlignment(uintptr_t adjustedbytes, bool needSync)
	{
		if (needSync) {
			MM_AtomicOperations::add(&(_adjustedBytesForCardAlignment), adjustedbytes);
		} else {
			_adjustedBytesForCardAlignment += adjustedbytes;
		}
	}

	MMINLINE void initialFirstUnalignedFreeEntry()
	{
		_firstUnalignedFreeEntry = (NULL == _heapFreeList) ? FREE_ENTRY_END : _heapFreeList;
		_prevFirstUnalignedFreeEntry =  FREE_ENTRY_END;
	}

	MMINLINE void resetFirstUnalignedFreeEntry()
	{
		_firstUnalignedFreeEntry =  FREE_ENTRY_END;
		_prevFirstUnalignedFreeEntry =  FREE_ENTRY_END;
	}

	MMINLINE virtual uintptr_t getDarkMatterBytes()
	{
		return _darkMatterBytes + _adjustedBytesForCardAlignment;
	}

	MMINLINE virtual uintptr_t getActualFreeMemorySize()
	{
		return _freeMemorySize - _adjustedBytesForCardAlignment;
	}

	/**
	 * Create a MemoryPoolAddressOrderedList object.
	 */
	MM_MemoryPoolAddressOrderedList(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize) :
		MM_MemoryPoolAddressOrderedListBase(env, minimumFreeEntrySize)
		,_heapFreeList(NULL)
		,_largeObjectCollectorAllocateStats(NULL)
		,_firstUnalignedFreeEntry(FREE_ENTRY_END)
		,_prevFirstUnalignedFreeEntry(FREE_ENTRY_END)
	{
		_typeId = __FUNCTION__;
	};

	MM_MemoryPoolAddressOrderedList(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize, const char *name) :
		MM_MemoryPoolAddressOrderedListBase(env, minimumFreeEntrySize, name)
		,_heapFreeList(NULL)
		,_largeObjectCollectorAllocateStats(NULL)
		,_firstUnalignedFreeEntry(FREE_ENTRY_END)
		,_prevFirstUnalignedFreeEntry(FREE_ENTRY_END)
	{
		_typeId = __FUNCTION__;
	};

#if defined(OMR_GC_CONCURRENT_SWEEP)
	friend class MM_ConcurrentSweepScheme;
#endif /* OMR_GC_CONCURRENT_SWEEP */
	
	friend class MM_SweepPoolManagerAddressOrderedList;
	friend class MM_SweepPoolManagerVLHGC;
};

#endif /* MEMORYPOOLADDRESSORDEREDLIST_HPP_ */
