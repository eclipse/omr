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

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(MEMORYPOOLSPLITADDRESSORDEREDLIST_HPP_)
#define MEMORYPOOLSPLITADDRESSORDEREDLIST_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "HeapLinkedFreeHeader.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemoryPoolSplitAddressOrderedListBase.hpp"
#include "EnvironmentBase.hpp"

class MM_AllocateDescription;
class MM_ConcurrentSweepScheme;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_MemoryPoolSplitAddressOrderedList : public MM_MemoryPoolSplitAddressOrderedListBase
{
	/* 
	 * Data members
	 */
private:
 	uintptr_t _reservedFreeEntrySize;		/**< set during postSweepProcess */
  	MM_HeapLinkedFreeHeader* _previousReservedFreeEntry;	/**< combination _previousReservedFreeEntry and _reservedFreeListIndex are used to identify or update the reservedFreeEntry */
 	uintptr_t _reservedFreeListIndex;		/**< the reservedFreeEntry is initialized only once in first pass iterating after sweep, used/updated only in second pass */
	bool _reservedFreeEntryAvaliable;	/**< True if the reserved Free Entry can be used */
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

	/* helpers for maintaining reserved free entry - start */
	/**
	 * check if previousFreeEntry is the same as previousReservedFreeEntry
	 */
	MMINLINE bool isPreviousReservedFreeEntry(MM_HeapLinkedFreeHeader* previousFreeEntry, uintptr_t curFreeList)
	{
		return ((previousFreeEntry == _previousReservedFreeEntry) && (curFreeList == _reservedFreeListIndex));
	}

	MMINLINE MM_HeapLinkedFreeHeader* getReservedFreeEntry()
	{
		bool const compressed = compressObjectReferences();
		MM_HeapLinkedFreeHeader* freeEntry = NULL;
		if (_reservedFreeEntryAvaliable) {
			Assert_MM_true(_heapFreeListCount > _reservedFreeListIndex);
			Assert_MM_true((void *)UDATA_MAX != _previousReservedFreeEntry);
			if (NULL == _previousReservedFreeEntry) {
				freeEntry = _heapFreeLists[_reservedFreeListIndex]._freeList;
			} else {
				freeEntry = _previousReservedFreeEntry->getNext(compressed);
			}
			Assert_MM_true(_reservedFreeEntrySize == freeEntry->getSize());
		}
		return freeEntry;
	}

	MMINLINE  bool	reservedFreeEntryConsistencyCheck()
	{
		bool const compressed = compressObjectReferences();
		bool ret = true;
		if (_reservedFreeEntryAvaliable) {
			MM_HeapLinkedFreeHeader* freeEntry = NULL;
			if (NULL == _previousReservedFreeEntry) {
				freeEntry = _heapFreeLists[_reservedFreeListIndex]._freeList;
			} else {
				freeEntry = _previousReservedFreeEntry->getNext(compressed);
			}
			ret = (_reservedFreeEntrySize == freeEntry->getSize());
		}
		return ret;
	}
	/**
	 * check if currentFreeEntry is the same as reservedFreeEntry
	 */
	MMINLINE bool isCurrentReservedFreeEntry(MM_HeapLinkedFreeHeader* curFreeEntry, uintptr_t curFreeList)
	{
		bool const compressed = compressObjectReferences();
		bool retValue = (curFreeList == _reservedFreeListIndex);
		
		if (retValue) {
			if (NULL == _previousReservedFreeEntry) {
				retValue = (_heapFreeLists[curFreeList]._freeList == curFreeEntry);
			} else {
				retValue = (_previousReservedFreeEntry->getNext(compressed) == curFreeEntry);
			}
		}
		
		return retValue;
	}

	/**
	 * reset reservedFreeEntry
	 */
	MMINLINE void resetReservedFreeEntry()
	{
		_reservedFreeEntryAvaliable = false;
		_reservedFreeEntrySize = 0;
		/* if reserved free entry is not avaliable, previous entry and index are set to special values,
		 * which is mostly used for debugging purposes
		 */
		_previousReservedFreeEntry = (MM_HeapLinkedFreeHeader*) UDATA_MAX;
		_reservedFreeListIndex = _heapFreeListCount;
	}

	/* helpers for maintaining reserved free entry - end */
	
protected:
	virtual void *internalAllocate(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats);
	virtual bool internalAllocateTLH(MM_EnvironmentBase *env, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired, MM_LargeObjectAllocateStats *largeObjectAllocateStats);
public:
	static MM_MemoryPoolSplitAddressOrderedList* newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit);
	static MM_MemoryPoolSplitAddressOrderedList* newInstance(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t maxSplit, const char* name);

	virtual void reset(Cause cause = any);

	virtual void addFreeEntries(MM_EnvironmentBase* env, MM_HeapLinkedFreeHeader*& freeListHead, MM_HeapLinkedFreeHeader*& freeListTail,
								uintptr_t freeListMemoryCount, uintptr_t freeListMemorySize);

	virtual bool removeFreeEntriesWithinRange(MM_EnvironmentBase* env, void* lowAddress, void* highAddress, uintptr_t minimumSize,
											  MM_HeapLinkedFreeHeader*& retListHead, MM_HeapLinkedFreeHeader*& retListTail,
											  uintptr_t& retListMemoryCount, uintptr_t& retListMemorySize);

	virtual void postProcess(MM_EnvironmentBase* env, Cause cause);

	virtual void expandWithRange(MM_EnvironmentBase* env, uintptr_t expandSize, void* lowAddress, void* highAddress, bool canCoalesce);
	virtual void* contractWithRange(MM_EnvironmentBase* env, uintptr_t contractSize, void* lowAddress, void* highAddress);

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	virtual uintptr_t releaseFreeMemoryPages(MM_EnvironmentBase* env);
#endif

	/**
	 * Create a MemoryPoolAddressOrderedList object.
	 */
	MM_MemoryPoolSplitAddressOrderedList(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t splitAmount)
		: MM_MemoryPoolSplitAddressOrderedListBase(env, minimumFreeEntrySize, splitAmount)
		, _reservedFreeEntrySize(0)
		, _previousReservedFreeEntry((MM_HeapLinkedFreeHeader*) UDATA_MAX)
		, _reservedFreeListIndex(splitAmount)
		, _reservedFreeEntryAvaliable(false)
	{
		_typeId = __FUNCTION__;
	};

	MM_MemoryPoolSplitAddressOrderedList(MM_EnvironmentBase* env, uintptr_t minimumFreeEntrySize, uintptr_t splitAmount, const char* name)
		: MM_MemoryPoolSplitAddressOrderedListBase(env, minimumFreeEntrySize, splitAmount, name)
		, _reservedFreeEntrySize(0)
		, _previousReservedFreeEntry((MM_HeapLinkedFreeHeader*)UDATA_MAX)
		, _reservedFreeListIndex(splitAmount)
		, _reservedFreeEntryAvaliable(false)
	{
		_typeId = __FUNCTION__;
	};

	/*
	 * Friends
	 */
	friend class MM_ConcurrentSweepScheme;
	friend class MM_SweepPoolManagerSplitAddressOrderedList;
};


#endif /* MEMORYPOOLSPLITADDRESSORDEREDLIST_HPP_ */
