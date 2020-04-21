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

#if !defined(MEMORYPOOLADDRESSORDEREDLISTBASE_HPP_)
#define MEMORYPOOLADDRESSORDEREDLISTBASE_HPP_

#include "omrcfg.h"
//#include "omrcomp.h"
#include "modronopt.h"

#include "HeapLinkedFreeHeader.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemoryPool.hpp"
#include "EnvironmentBase.hpp"

#if defined(OMR_VALGRIND_MEMCHECK)
#include "MemcheckWrapper.hpp"
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

class MM_SweepPoolManager;
class MM_SweepPoolState;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_MemoryPoolAddressOrderedListBase : public MM_MemoryPool
{
/*
 * Data members
 */
private:
protected:
	/* Basic free list support */
	MM_LightweightNonReentrantLock _resetLock;

	MM_HeapLinkedFreeHeader ** _referenceHeapFreeList;
	MM_SweepPoolState* _sweepPoolState;	/**< GC Sweep Pool State */
	MM_SweepPoolManagerAddressOrderedListBase* _sweepPoolManager;		/**< pointer to SweepPoolManager class */
	MM_HeapLinkedFreeHeader *_lastFreeEntry;							/**< address of the last free entry in the pool; valid after compact; NOT maintained during allocation */ 

public:
	
/*
 * Function members
 */	
private:

protected:
/**
 * Update memory pool statistical data
 * 
 * @param freeBytes free bytes added
 * @param freeEntryCount free memory elements added
 * @param largestFreeEntry largest free memory element size 
 */
 	void updateMemoryPoolStatistics(MM_EnvironmentBase *env, uintptr_t freeBytes, uintptr_t freeEntryCount, uintptr_t largestFreeEntry)
	{
		setFreeMemorySize(freeBytes);
		setFreeEntryCount(freeEntryCount);
		setLargestFreeEntry(largestFreeEntry);
	}

	MMINLINE bool internalRecycleHeapChunk(void *addrBase, void *addrTop, MM_HeapLinkedFreeHeader *next)
	{
		/* Determine if the heap chunk belongs in the free list */
		uintptr_t freeEntrySize = ((uintptr_t)addrTop) - ((uintptr_t)addrBase);
		Assert_MM_true((uintptr_t)addrTop >= (uintptr_t)addrBase);
		bool const compressed = compressObjectReferences();

#if defined(OMR_VALGRIND_MEMCHECK)
		valgrindMakeMemUndefined((uintptr_t) addrBase, freeEntrySize);
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

		MM_HeapLinkedFreeHeader *freeEntry = MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, freeEntrySize, compressed);
		if ((NULL != freeEntry) && (freeEntrySize >= _minimumFreeEntrySize)) {
			Assert_MM_true(freeEntry == addrBase);
			Assert_MM_true((NULL == next) || (freeEntry < next));
			freeEntry->setNext(next, compressed);
			return true;
		} else {
			return false;
		}
	}
	
	/**
	 * set the address of the last free entry in the pool
	 */
	virtual void setLastFreeEntry(void * addr) { _lastFreeEntry = (MM_HeapLinkedFreeHeader *)addr; }
	
	/**
	 * get the address of the last free entry in the pool
	 */
	virtual MM_HeapLinkedFreeHeader *getLastFreeEntry() { return _lastFreeEntry; }
	
#if defined(OMR_GC_LARGE_OBJECT_AREA)
	/**
	 * Append a free entry to an address ordered list
	 *
	 * @param addrBase Low address of memory to be appended
	 * @param addrTop High address of memory non inclusive)
	 * @param minimumSize Only append entries of this minimum size
	 * @param freeListHead Current head of list of free entries
	 * @param freeListTail Current tail of list of free entries
	 *
	 * @return TRUE if free entry appended; FALSE otherwise
	 */

	MMINLINE bool appendToList(MM_EnvironmentBase* env, void* addrBase, void* addrTop, uintptr_t minimumSize, MM_HeapLinkedFreeHeader*& freeListHead, MM_HeapLinkedFreeHeader*& freeListTail)
	{
		bool const compressed = compressObjectReferences();
		uintptr_t freeEntrySize = ((uint8_t*)addrTop - (uint8_t*)addrBase);

		MM_HeapLinkedFreeHeader* freeEntry = MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, freeEntrySize, compressed);
		if ((NULL != freeEntry) && (freeEntrySize >= minimumSize)) {
			/* If the list is not empty, add the entry to the tail. */
			if (NULL != freeListHead) {
				Assert_MM_true(freeListTail < freeEntry);
				freeListTail->setNext(freeEntry, compressed);
			} else {
				freeListHead = freeEntry;
			}
			freeListTail = freeEntry;

			return true;
		} else {
			/* Abandon entry */
			return false;
		}
	}

	MMINLINE bool insertToList(MM_EnvironmentBase* env, void* addrBase, void* addrTop, uintptr_t minimumSize, MM_HeapLinkedFreeHeader*& freeListHead, MM_HeapLinkedFreeHeader*& freeListTail)
	{
		bool const compressed = compressObjectReferences();
		uintptr_t freeEntrySize = ((uint8_t*)addrTop - (uint8_t*)addrBase);

		MM_HeapLinkedFreeHeader* freeEntry = MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, freeEntrySize, compressed);
		if ((NULL != freeEntry) && (freeEntrySize >= minimumSize)) {
			/* If the list is not empty, add the entry to the tail. */
			if (NULL != freeListHead) {
				if (freeListTail < freeEntry) {
					freeListTail->setNext(freeEntry, compressed);
					freeListTail = freeEntry;
				} else {
					MM_HeapLinkedFreeHeader* cur = freeListHead;
					while (cur > freeEntry) {
						cur = cur->getNext(compressed);
					}
					freeEntry->setNext(cur->getNext(compressed), compressed);
					cur->setNext(freeEntry, compressed);
				}
			} else {
				freeListHead = freeEntry;
				freeListTail = freeEntry;
			}
			return true;
		} else {
			/* Abandon entry */
			return false;
		}

	}

#endif /* OMR_GC_LARGE_OBJECT_AREA */

	bool connectInnerMemoryToPool(MM_EnvironmentBase* env, void* address, uintptr_t size, void* previousFreeEntry);
	void connectOuterMemoryToPool(MM_EnvironmentBase *env, void *address, uintptr_t size, void *nextFreeEntry);
	void connectFinalMemoryToPool(MM_EnvironmentBase *env, void *address, uintptr_t size);
	void abandonMemoryInPool(MM_EnvironmentBase *env, void *address, uintptr_t size);


	/**
	 * Check, can free memory element be connected to memory pool
	 * 
	 * @param address free memory start address
	 * @param size free memory size in bytes
	 * @return true if free memory element would be accepted
	 */
	MMINLINE bool canMemoryBeConnectedToPool(MM_EnvironmentBase* env, void* address, uintptr_t size)
	{
		return size >= getMinimumFreeEntrySize();
	}
	
public:
	virtual void acquireResetLock(MM_EnvironmentBase* env);
	virtual void releaseResetLock(MM_EnvironmentBase* env);

	virtual bool createFreeEntry(MM_EnvironmentBase* env, void* addrBase, void* addrTop,
								 MM_HeapLinkedFreeHeader* previousFreeEntry, MM_HeapLinkedFreeHeader* nextFreeEntry);

	virtual bool createFreeEntry(MM_EnvironmentBase* env, void* addrBase, void* addrTop);



	MMINLINE bool abandonHeapChunk(void *addrBase, void *addrTop)
	{
		Assert_MM_true(addrTop >= addrBase);
		return internalRecycleHeapChunk(addrBase, addrTop, NULL);
	}
	
	MMINLINE MM_SweepPoolState * getSweepPoolState()
	{
		Assert_MM_true(NULL != _sweepPoolState);
		return _sweepPoolState;
	}
	
	/**
	 * Get access to Sweep Pool Manager
	 * @return pointer to Sweep Pool Manager associated with this pool
	 * or NULL for superpools
	 */
	MMINLINE MM_SweepPoolManager *getSweepPoolManager()
//	MMINLINE MM_SweepPoolManagerAddressOrderedListBase *getSweepPoolManager()
	{
		/*
		 * This function must be called for leaf pools only
		 * The failure at this line means that superpool has been used as leaf pool
		 * or leaf pool has problem with creation (like early initialization)
		 */
		Assert_MM_true(NULL != _sweepPoolManager);
		return (MM_SweepPoolManager*)_sweepPoolManager;
	}
	
	virtual void printCurrentFreeList(MM_EnvironmentBase* env, const char* area)=0;

	virtual void recalculateMemoryPoolStatistics(MM_EnvironmentBase* env)=0;
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	uintptr_t releaseFreeEntryMemoryPages(MM_EnvironmentBase* env, MM_HeapLinkedFreeHeader* freeEntry);
#endif
	/**
	 * Create a MemoryPoolAddressOrderedList object.
	 */
	MM_MemoryPoolAddressOrderedListBase(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize) :
		MM_MemoryPool(env, minimumFreeEntrySize)
		,_referenceHeapFreeList(NULL)
		,_sweepPoolState(NULL)
		,_sweepPoolManager(NULL)
		,_lastFreeEntry(NULL)
	{
//		_typeId = __FUNCTION__;
	};

	MM_MemoryPoolAddressOrderedListBase(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize, const char *name) :
		MM_MemoryPool(env, minimumFreeEntrySize, name)
		,_referenceHeapFreeList(NULL)
		,_sweepPoolState(NULL)
		,_sweepPoolManager(NULL)
		,_lastFreeEntry(NULL)
	{
//		_typeId = __FUNCTION__;
	};

	friend class MM_SweepPoolManagerAddressOrderedListBase;
};

#endif /* MEMORYPOOLADDRESSORDEREDLISTBASE_HPP_ */
