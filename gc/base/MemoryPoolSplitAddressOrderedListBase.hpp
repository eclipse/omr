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

#if !defined(MEMORYPOOLSPLITADDRESSORDEREDLISTBASE_HPP_)
#define MEMORYPOOLSPLITADDRESSORDEREDLISTBASE_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "HeapLinkedFreeHeader.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemoryPoolAddressOrderedListBase.hpp"
#include "EnvironmentBase.hpp"

class MM_AllocateDescription;

typedef struct J9ModronHeapFreeList {
	MM_LightweightNonReentrantLock _lock;
	MM_HeapLinkedFreeHeader* _freeList;
	uintptr_t _timesLocked;

	uintptr_t _freeSize;
	uintptr_t _freeCount;

	/* Hint support */
	struct J9ModronAllocateHint* _hintActive;
	struct J9ModronAllocateHint* _hintInactive;
	struct J9ModronAllocateHint _hintStorage[HINT_ELEMENT_COUNT];
	uintptr_t _hintLru;

	bool initialize(MM_EnvironmentBase* env);
	void tearDown();

	void clearHints();
	void reset();

	MMINLINE void addHint(MM_HeapLinkedFreeHeader* freeEntry, uintptr_t lookupSize)
	{
		/* Travel the list removing any hints that this new hint will override */
		J9ModronAllocateHint* previousActiveHint = NULL;
		J9ModronAllocateHint* currentActiveHint = _hintActive;
		J9ModronAllocateHint* nextActiveHint = NULL;

		while (currentActiveHint) {
			/* Check what address range the free header to be added lies in (below, equal, above) */
			if (freeEntry < currentActiveHint->heapFreeHeader) {
				if (lookupSize >= currentActiveHint->size) {
					/* remove this hint */
					goto removeHint;
				}
			} else {
				if (freeEntry == currentActiveHint->heapFreeHeader) {
					if (lookupSize < currentActiveHint->size) {
						/* remove this hint */
						goto removeHint;
					} else {
						/* The hint to be added is redundant */
						return;
					}
				} else {
					/* freeEntry > currentActiveHint->heapFreeHeader <--- implied */
					if (lookupSize <= currentActiveHint->size) {
						/* remove this hint */
						goto removeHint;
					}
				}
			}

			/* Do nothing to this hint - move to the next one */
			previousActiveHint = currentActiveHint;
			currentActiveHint = currentActiveHint->next;

			continue;

	removeHint:
			/* Remove the currentActiveHint */
			nextActiveHint = currentActiveHint->next;
			if (previousActiveHint) {
				previousActiveHint->next = nextActiveHint;
			} else {
				_hintActive = nextActiveHint;
			}
			currentActiveHint->next = _hintInactive;
			_hintInactive = currentActiveHint;
			currentActiveHint = nextActiveHint;
		}


		/* Get a hint from the list */
		J9ModronAllocateHint* hint;
		if (_hintInactive) {
			/* Consume the hint from the inactive list */
			hint = _hintInactive;
			_hintInactive = hint->next;
			/* Place new hint in active list */
			hint->next = _hintActive;
			_hintActive = hint;
		} else {
			J9ModronAllocateHint* currentHint;
			/* Find the smallest LRU value and recycle the hint */
			hint = _hintActive;
			currentHint = hint->next;
			while (currentHint) {
				if (hint->lru > currentHint->lru) {
					hint = currentHint;
				}
				currentHint = currentHint->next;
			}
		}

		/* Update the global and local LRU */
		hint->lru = _hintLru++;

		/* Initialize the hint info */
		hint->size = lookupSize;
		hint->heapFreeHeader = freeEntry;
	}
	
	MMINLINE J9ModronAllocateHint* findHint(uintptr_t lookupSize)
	{
		J9ModronAllocateHint* hint = NULL;
		J9ModronAllocateHint* candidateHint = _hintActive;
		J9ModronAllocateHint* previousHint = NULL;

		/* Be sure to remove stale hints as we search the list (stale hints are addresses < the heap free head) */
		while (candidateHint) {
			if (!_freeList || (candidateHint->heapFreeHeader < _freeList)) {
				/* Hint is stale - remove */
				J9ModronAllocateHint* nextHint;

				if (previousHint) {
					previousHint->next = candidateHint->next;
				} else {
					_hintActive = candidateHint->next;
				}

				nextHint = candidateHint->next;
				candidateHint->next = _hintInactive;
				_hintInactive = candidateHint;
				candidateHint = nextHint;
			} else {
				if (candidateHint->size < lookupSize) {
					if (hint) {
						if (candidateHint->size > hint->size) {
							hint = candidateHint;
						}
					} else {
						hint = candidateHint;
					}
				}
				previousHint = candidateHint;
				candidateHint = candidateHint->next;
			}
		}

		if (hint) {
			/* Update the global and local LRU */
			hint->lru = _hintLru++;
		}

		return hint;
	}
	
	MMINLINE void removeHint(MM_HeapLinkedFreeHeader* freeEntry)
	{
		J9ModronAllocateHint* hint = _hintActive;
		J9ModronAllocateHint* previousHint = NULL;
		J9ModronAllocateHint* nextHint = NULL;

		while (hint) {
			if (hint->heapFreeHeader == freeEntry) {
				nextHint = hint->next;
				/* Add the hint to the free list */
				hint->next = _hintInactive;
				_hintInactive = hint;
				/* Glue the previous hint to the next hint */
				if (previousHint) {
					previousHint->next = nextHint;
				} else {
					/* no previous hint - must be the head */
					_hintActive = nextHint;
				}
				hint = nextHint;
			} else {
				/* Move to the next hint */
				previousHint = hint;
				hint = hint->next;
			}
		}
	}
	
	MMINLINE void updateHint(MM_HeapLinkedFreeHeader* oldFreeEntry, MM_HeapLinkedFreeHeader* newFreeEntry)
	{
		J9ModronAllocateHint* hint = _hintActive;
		bool found = false;

		while (hint) {
			if (hint->heapFreeHeader == oldFreeEntry) {
				/*
				 * Should be no duplicates in hints
				 * check and assert if there is more then one
				 */
				Assert_MM_true(!found);

				hint->heapFreeHeader = newFreeEntry;
				found = true;
			} else {
				/* Move to the next hint */
				hint = hint->next;
			}
		}
	}
} J9ModronFreeList;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_MemoryPoolSplitAddressOrderedListBase : public MM_MemoryPoolAddressOrderedListBase {
	/*
	 * Data members
	 */
private:
protected:
	/* Basic free list support */
	uintptr_t _heapFreeListCount;
	uintptr_t _heapFreeListCountExtended;
	uintptr_t* _currentThreadFreeList;
	J9ModronFreeList* _heapFreeLists;

	MM_LargeObjectAllocateStats* _largeObjectAllocateStatsForFreeList; /**< Approximate allocation profile for large objects. An array of stat structs for each free list */
	MM_LargeObjectAllocateStats* _largeObjectCollectorAllocateStatsForFreeList; /**< Same as _largeObjectAllocateStatsForFreeList except specifically for collector allocates */
public:
	/*
	 * Function members
	 */
private:

protected:
	virtual void *internalAllocate(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats) = 0;
	virtual bool internalAllocateTLH(MM_EnvironmentBase *env, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats) = 0;

	bool recycleHeapChunk(MM_EnvironmentBase* env, void* addrBase, void* addrTop, MM_HeapLinkedFreeHeader* previousFreeEntry, MM_HeapLinkedFreeHeader* nextFreeEntry, uintptr_t curFreeList);

	MMINLINE uintptr_t findGoodStartFreeList()
	{
		uintptr_t index = 0;
		uintptr_t timesLocked = UDATA_MAX;
		for (uintptr_t i = 0; i < _heapFreeListCount; ++i) {
			if ((NULL != _heapFreeLists[i]._freeList) && (_heapFreeLists[i]._timesLocked < timesLocked)) {
				index = i;
				timesLocked = _heapFreeLists[i]._timesLocked;
			}
		}
		return index;
	}

	/**
	 * set Next of the freeEntry with new freeEntry pointer
	 *
	 * @param[in]	freelist
	 * @param[in] 	freeEntry if freeEntry is NULL, set head of freelist with new freeEntry pointer
	 * @param[in] 	next	the new freeEntry pointer, if it is NULL, reset Next of freeEntry
	 */
	MMINLINE void setNextForFreeEntryInFreeList(J9ModronFreeList* freelist, MM_HeapLinkedFreeHeader* freeEntry, MM_HeapLinkedFreeHeader* next)
	{
		if (NULL == freeEntry) {
			freelist->_freeList = next;
		} else {
			freeEntry->setNext(next);
		}
	}

	bool printFreeListValidity(MM_EnvironmentBase* env);
public:
	virtual void* allocateObject(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription);
	virtual void* allocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop);
	virtual void* collectorAllocate(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, bool lockingRequired);
	virtual void* collectorAllocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop, bool lockingRequired);

	virtual void lock(MM_EnvironmentBase* env);
	virtual void unlock(MM_EnvironmentBase* env);

	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);

	virtual void reset(Cause cause = any);
	virtual MM_HeapLinkedFreeHeader* rebuildFreeListInRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region, MM_HeapLinkedFreeHeader* previousFreeEntry);

#if defined(DEBUG)
	virtual bool isValidListOrdering();
#endif

	virtual void* findAddressAfterFreeSize(MM_EnvironmentBase* env, uintptr_t sizeRequired, uintptr_t minimumSize);

	virtual void* findFreeEntryEndingAtAddr(MM_EnvironmentBase* env, void* addr);
	virtual uintptr_t getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, void* lowAddr, void* highAddr);
	virtual void* findFreeEntryTopStartingAtAddr(MM_EnvironmentBase* env, void* addr);
	virtual void* getFirstFreeStartingAddr(MM_EnvironmentBase* env);
	void* getFirstFreeStartingAddr(MM_EnvironmentBase* env, uintptr_t* currentFreeListReturn);
	virtual void* getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree);
	void* getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree, uintptr_t* currentFreeListIndex);

	virtual void moveHeap(MM_EnvironmentBase* env, void* srcBase, void* srcTop, void* dstBase);

	/**< Reset current (as opposed to average) large object allocate stats */
	virtual void resetLargeObjectAllocateStats();

	/**
	 * Merge the free entry stats from free lists.
	 */
	virtual void mergeFreeEntryAllocateStats();
	/**
	 * Merge the tlh allocation distribution stats from free lists.
	 */
	virtual void mergeTlhAllocateStats();
	/**
	 * Merge the current LargeObject allocation distribution stats from free lists.
	 */
	virtual void mergeLargeObjectAllocateStats();

	virtual void appendCollectorLargeAllocateStats();

	virtual void printCurrentFreeList(MM_EnvironmentBase* env, const char* area);

	/**
	 * Recalculate the memory pool statistics by actually examining the contents of the pool.
	 */
	virtual void recalculateMemoryPoolStatistics(MM_EnvironmentBase* env);

	virtual uintptr_t getActualFreeMemorySize();
	virtual uintptr_t getActualFreeEntryCount();

	/**
	 * Create a MemoryPoolAddressOrderedList object.
	 */
	MM_MemoryPoolSplitAddressOrderedListBase(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t splitAmount)
		: MM_MemoryPoolAddressOrderedListBase(env, minimumFreeEntrySize)
		, _heapFreeListCount(splitAmount)
		, _heapFreeListCountExtended(splitAmount)
		, _currentThreadFreeList(0)
		, _heapFreeLists(NULL)
		, _largeObjectAllocateStatsForFreeList(NULL)
		, _largeObjectCollectorAllocateStatsForFreeList(NULL)
	{
	};

	MM_MemoryPoolSplitAddressOrderedListBase(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t splitAmount, const char* name)
		: MM_MemoryPoolAddressOrderedListBase(env, minimumFreeEntrySize, name)
		, _heapFreeListCount(splitAmount)
		, _heapFreeListCountExtended(splitAmount)
		, _currentThreadFreeList(0)
		, _heapFreeLists(NULL)
		, _largeObjectAllocateStatsForFreeList(NULL)
		, _largeObjectCollectorAllocateStatsForFreeList(NULL)
	{
	};

	/*
	 * Friends
	 */
};


#endif /* MEMORYPOOLSPLITADDRESSORDEREDLISTBASE_HPP_ */
