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

class MM_AllocateDescription;
#if defined(OMR_GC_CONCURRENT_SWEEP)
class MM_ConcurrentSweepScheme;
#endif /* OMR_GC_CONCURRENT_SWEEP */

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

	bool recycleHeapChunk(void *addrBase, void *addrTop, MM_HeapLinkedFreeHeader *previousFreeEntry, MM_HeapLinkedFreeHeader *nextFreeEntry);	
	
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

	bool recycleHeapChunk(void* chunkBase, void* chunkTop);

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

	/**
	 * Create a MemoryPoolAddressOrderedList object.
	 */
	MM_MemoryPoolAddressOrderedList(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize) :
		MM_MemoryPoolAddressOrderedListBase(env, minimumFreeEntrySize)
		,_heapFreeList(NULL)
		,_largeObjectCollectorAllocateStats(NULL)
	{
		_typeId = __FUNCTION__;
	};

	MM_MemoryPoolAddressOrderedList(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize, const char *name) :
		MM_MemoryPoolAddressOrderedListBase(env, minimumFreeEntrySize, name)
		,_heapFreeList(NULL)
		,_largeObjectCollectorAllocateStats(NULL)
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
