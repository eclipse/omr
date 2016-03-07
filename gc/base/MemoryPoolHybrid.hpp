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

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(MEMORYPOOLHYBRID_HPP_)
#define MEMORYPOOLHYBRID_HPP_
#include "MemoryPoolSplitAddressOrderedListBase.hpp"

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_MemoryPoolHybrid : public MM_MemoryPoolSplitAddressOrderedListBase {
	/* 
	 * Data members
	 */
private:
protected:
public:
	/*
	 * Function members
	 */
private:
	/**
	 *  Search the free entry from the free list (only for first pass iterating)
	 *
	 * @param[in]      env
	 * @param[in]      sizeInBytesRequired
	 * @param[in]      curFreeList the index of the current freeList
	 * @param[in/out]  previousFreeEntry  a pointer to previousFreeEntry
	 * @param[in/out]  largestFreeEntry   a pointer to largestFreeEntry
	 * @return a free entry, NULL if not find a free entry
	 */
	MM_HeapLinkedFreeHeader* internalAllocateFromList(MM_EnvironmentBase* env, uintptr_t sizeInBytesRequired, uintptr_t curFreeList, MM_HeapLinkedFreeHeader** previousFreeEntry, uintptr_t* largestFreeEntry);

	MMINLINE J9ModronFreeList* getReservedFreeList()
	{
		return &_heapFreeLists[_heapFreeListCount];
	}

	void appendToReservedFreeList(MM_EnvironmentBase* env, void* address, uintptr_t size);

	/* helper methods for expand/contract/redistribute memory pool start */

	/**
	 *  append newfreeEntry at the end of freeList
	 *
	 * @param[in]      env
	 * @param[in]      freeList
	 * @param[in]      freeListTail last FreeEntry in the list
	 * @param[in]	   newFreeEntry
	 */
	void appendToFreeList(MM_EnvironmentBase* env, J9ModronFreeList* freeList, MM_HeapLinkedFreeHeader* freeListTail, MM_HeapLinkedFreeHeader* newFreeEntry);

	/**
	 *  append the range of free memory at the end of freeList
	 *
	 * @param[in]      env
	 * @param[in]      freeList
	 * @param[in]      freeListTail last FreeEntry in the list
	 * @param[in]	   lowAddress low address of the range
	 * @param[in] 	   size the size of the range
	 */
	void appendToFreeList(MM_EnvironmentBase* env, J9ModronFreeList* freeList, MM_HeapLinkedFreeHeader* freeListTail, void* lowAddress, uintptr_t size);

	/**
	 *  coalesce the range of free memory with last freeEntry of the freelist
	 *
	 * @param[in]      lastFreeEntry	the last freeEntry of the freelist
	 * @param[in]      freeList
	 * @param[in]	   lowAddress low address of the range
	 * @param[in] 	   expandSize the size of the range
	 * @return		   expandedSize if can coalesce, the size of the range if can not coalesce
	 */
	uintptr_t coalesceExpandRangeWithLastFreeEntry(MM_HeapLinkedFreeHeader* lastFreeEntry, J9ModronFreeList* freelist, void* lowAddress, uintptr_t expandSize);

	/**
	 *  coalesce the new freeEntry with last freeEntry of the freelist
	 *
	 * @param[in]      lastFreeEntry	the last freeEntry of the freelist
	 * @param[in]      freeList
	 * @param[in]	   newFreeEntry
	 * @return		   expandedSize if can coalesce, the size of newFreeEntry if can not coalesce
	 */
	uintptr_t coalesceNewFreeEntryWithLastFreeEntry(MM_HeapLinkedFreeHeader* lastFreeEntry, J9ModronFreeList* freelist, MM_HeapLinkedFreeHeader* newFreeEntry);

	/**
	 *  move the last FreeEntry to reserved FreeList
	 *
	 * @param[in]      env
	 * @param[in]	   fromFreeList
	 * @param[in]      previousFreeEntry	the pervious of last freeEntry
	 * @param[in]      freeEntry			last freeEntry of fromFree
	 * @param[in]      toFreeList
	 * @param[in]	   toFreeListTail
	 */
	void moveLastFreeEntryToReservedFreeList(MM_EnvironmentBase* env, J9ModronFreeList* fromFreeList, MM_HeapLinkedFreeHeader* previousFreeEntry, MM_HeapLinkedFreeHeader* freeEntry, J9ModronFreeList* toFreeList, MM_HeapLinkedFreeHeader* toFreeListTail);
	/**
	 * this one will try to contract range of free memory from last free entry in the freelist. If the range is larger than last free entry size, contract is ignored,
	 * If the range is smaller than last free Entry, create last freeEntry from the remainder.
	 *
	 * @param[in]      env
	 * @param[in]	   freelist
	 * @param[in]      contractSize			from lowAddress to highAddress
	 * @param[in]      lowAddress			lowAddress of contract range
	 * @param[in]      lowAddress			lowAddress of contract range
	 * @return 			true if can contract, otherwise false
	 */
	bool tryContractWithRangeInFreelist(MM_EnvironmentBase* env, J9ModronFreeList* freeList, uintptr_t contractSize, void* lowAddress, void* highAddress);
	/* helper methods for expand/contract/redistribute memory pool end */

protected:
	virtual void *internalAllocate(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats);
	virtual bool internalAllocateTLH(MM_EnvironmentBase *env, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats);
public:
	static MM_MemoryPoolHybrid* newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit);
	static MM_MemoryPoolHybrid* newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit, const char* name);

	virtual void postProcess(MM_EnvironmentBase* env, Cause cause);

	/* for MemoryPoolHybrid, expand/contract/redistribute only happened at the end of memoryPool(high address),
	 * in order to simplify the code, the below four methods (addFreeEntries, removeFreeEntriesWithinRange, expandWithRange, contractWithRange) only handle expand/contract/redistribute at the end of the memory pool.
	 */
	virtual void addFreeEntries(MM_EnvironmentBase* env, MM_HeapLinkedFreeHeader*& freeListHead, MM_HeapLinkedFreeHeader*& freeListTail,
								uintptr_t freeListMemoryCount, uintptr_t freeListMemorySize);

	virtual bool removeFreeEntriesWithinRange(MM_EnvironmentBase* env, void* lowAddress, void* highAddress, uintptr_t minimumSize,
											  MM_HeapLinkedFreeHeader*& retListHead, MM_HeapLinkedFreeHeader*& retListTail,
											  uintptr_t& retListMemoryCount, uintptr_t& retListMemorySize);

	virtual void expandWithRange(MM_EnvironmentBase* env, uintptr_t expandSize, void* lowAddress, void* highAddress, bool canCoalesce);
	virtual void* contractWithRange(MM_EnvironmentBase* env, uintptr_t contractSize, void* lowAddress, void* highAddress);

	/**
	 * Create a MemoryPoolAddressOrderedList object.
	 */
	MM_MemoryPoolHybrid(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t splitAmount)
		: MM_MemoryPoolSplitAddressOrderedListBase(env, minimumFreeEntrySize, splitAmount)
	{
		_heapFreeListCountExtended = _heapFreeListCount + 1;
		_typeId = __FUNCTION__;
	}

	MM_MemoryPoolHybrid(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t splitAmount, const char* name)
		: MM_MemoryPoolSplitAddressOrderedListBase(env, minimumFreeEntrySize, splitAmount, name)
	{
		_heapFreeListCountExtended = _heapFreeListCount + 1;
		_typeId = __FUNCTION__;
	}

	/*
	 * Friends
	 */
	friend class MM_ConcurrentSweepScheme;
	friend class MM_SweepPoolManagerHybrid;
};


#endif /* MEMORYPOOLHYBRID_HPP_ */
