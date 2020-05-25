/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(MEMORYPOOLLARGEOBJECTS_HPP_)
#define MEMORYPOOLLARGEOBJECTS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronopt.h"

#include "AtomicOperations.hpp"
#include "Base.hpp"
#include "GCExtensionsBase.hpp"
#include "MemoryPool.hpp"
#include "MemoryPoolAddressOrderedListBase.hpp"
#include "ModronAssertions.h"
class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_LargeObjectAllocateStats;
class MM_MemorySubSpace;

/* We expand/contract by LOA_RESIZE_AMOUNT_NORMAL when 
 * current ratio greater than LOA_MINIMUM_SIZE1 
 */
#define LOA_MINIMUM_SIZE1 ((double)0.01)
#define LOA_RESIZE_AMOUNT_NORMAL ((double)0.01)

/* We expand/contracxt by LOA_RESIZE_AMOUNT_SMALL when 
 * current ratio less than LOA_MINIMUM_SIZE1 and 
 * greater than LOA_MINIMUM_SIZE2
 */
#define LOA_MINIMUM_SIZE2 ((double)0.001)
#define LOA_RESIZE_AMOUNT_SMALL ((double)0.001)


#define LOA_CONTRACT_TRIGGER1 ((double)0.70)
#define LOA_CONTRACT_TRIGGER2 ((double)0.90)

#define LOA_EXPAND_TRIGGER1 ((double)0.30)
#define LOA_EXPAND_TRIGGER2 ((double)0.50)
#define LOA_EXPAND_TRGGER3 5

#if defined(J9ZOS390)
#if defined(OMR_ENV_DATA64)
#define LOA_EMPTY ((void*)0x7FFFFFFFFFFFFFFF)
#else /* !OMR_ENV_DATA64 */
#define LOA_EMPTY ((void*)0x7FFFFFFF)
#endif /* OMR_ENV_DATA64 */
#else /* !defined(J9ZOS390)*/
#define LOA_EMPTY ((void*)-1)
#endif /* !defined(J9ZOS390) */

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_MemoryPoolLargeObjects : public MM_MemoryPool {
	/*
	 * Data members
	 */
private:
	OMR_VM* _omrVM;

	uintptr_t _currentOldAreaSize;
	void* _currentLOABase;

	MM_MemoryPoolAddressOrderedListBase* _memoryPoolSmallObjects;
	MM_MemoryPoolAddressOrderedListBase* _memoryPoolLargeObjects;

	uintptr_t _loaSize;
	uintptr_t _soaSize;
	double _currentLOARatio;
	double _minLOAFreeRatio;
	double *_loaFreeRatioHistory; /**<history of LOA free/total size ratio  for a past few global GCs */


	uintptr_t _soaObjectSizeLWM;

	uintptr_t _soaFreeBytesAfterLastGC;

protected:
public:
	/*
	 * Function members
	 */
private:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);

	uintptr_t* determineLOABase(MM_EnvironmentBase* env, uintptr_t soaSize);
	double calculateTargetLOARatio(MM_EnvironmentBase*, uintptr_t bytesRequested);
	double resetTargetLOARatio(MM_EnvironmentBase*);
	void resetLOASize(MM_EnvironmentBase*, double newLOARatio);
	void redistributeFreeMemory(MM_EnvironmentBase*, uintptr_t newOldAreaSize);

	/**
	 * Check if new LOA size is larger than largeObjectMinimumSize. If it is not large enough,
	 * set the LOA size to 0.  After, set the SOA size to be the remainder of the heap.
	 * @return true If the newLOASize is large enough for the LOA
	 */
	MMINLINE bool
	checkAndSetSizeForLOA(MM_EnvironmentBase* env, uintptr_t newLOASize, double newLOARatio, void* newLOABase=NULL) {
		bool ret = newLOASize >= _extensions->largeObjectMinimumSize;
		uintptr_t oldAreaSize = _memorySubSpace->getActiveMemorySize();
		if (!ret) {
			/* No.. make LOA empty as not even big enough for one free chunk */
			_loaSize = 0;
			_soaSize = oldAreaSize;
			_currentLOARatio = 0;
			_currentLOABase = LOA_EMPTY;
		} else {
			_loaSize = newLOASize;
			/* ..and SOA is whats left after LOA allocation */
			_soaSize = oldAreaSize - _loaSize;
			if (0 != newLOARatio) {
				_currentLOARatio = newLOARatio;
			} else {
				Assert_MM_true((_loaSize + _soaSize) == oldAreaSize);
				_currentLOARatio = ((double)_loaSize) / oldAreaSize;
				/* Rounding during float operations may result in a new LOA ratio less than
				 * minimum so fix up if necessary
				 */
				if (_currentLOARatio < _extensions->largeObjectAreaMinimumRatio) {
					_currentLOARatio = _extensions->largeObjectAreaMinimumRatio;
				}
				Assert_MM_true(0 != _currentLOARatio);
			}
			if (NULL != newLOABase) {
				_currentLOABase = newLOABase;
			} else {
				_currentLOABase = determineLOABase(env, _soaSize);
			}
		}
		return ret;
	}

protected:
public:
	static MM_MemoryPoolLargeObjects* newInstance(MM_EnvironmentBase* env, MM_MemoryPoolAddressOrderedListBase* largeObjectArea, MM_MemoryPoolAddressOrderedListBase* smallObjectArea);

	virtual void lock(MM_EnvironmentBase* env);
	virtual void unlock(MM_EnvironmentBase* env);

	void preCollect(MM_EnvironmentBase* env, bool systemGC, bool aggressive, uintptr_t bytesRequested);
	virtual void resizeLOA(MM_EnvironmentBase* env);
	virtual bool completeFreelistRebuildRequired(MM_EnvironmentBase* env);

	virtual MM_MemoryPool* getMemoryPool(void* addr);
	virtual MM_MemoryPool* getMemoryPool(uintptr_t size);
	virtual MM_MemoryPool* getMemoryPool(MM_EnvironmentBase* env, void* addrBase, void* addrTop, void*& highAddr);

	virtual uintptr_t getMemoryPoolCount()
	{
		return 2;
	}

	virtual uintptr_t getActiveMemoryPoolCount()
	{
		return _loaSize > 0 ? 2 : 1;
	}

	virtual void resetHeapStatistics(bool memoryPoolCollected);
	virtual void mergeHeapStats(MM_HeapStats* heapStats, bool active);

	virtual void* allocateObject(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription);
	virtual void* collectorAllocate(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, bool lockingRequired);

	virtual void* allocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop);
	virtual void* collectorAllocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop, bool lockingRequired);

	virtual void reset(Cause cause = any);

	virtual void expandWithRange(MM_EnvironmentBase* env, uintptr_t expandSize, void* lowAddress, void* highAddress, bool canCoalesce);
	virtual void* contractWithRange(MM_EnvironmentBase* env, uintptr_t expandSize, void* lowAddress, void* highAddress);
	virtual bool abandonHeapChunk(void* addrBase, void* addrTop);

	virtual void* findFreeEntryEndingAtAddr(MM_EnvironmentBase* env, void* addr);
	virtual uintptr_t getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, void* lowAddr, void* highAddr);
	virtual void* findFreeEntryTopStartingAtAddr(MM_EnvironmentBase* env, void* addr);
	virtual void* getFirstFreeStartingAddr(MM_EnvironmentBase* env);
	virtual void* getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree);

	virtual uintptr_t getActualFreeMemorySize();
	virtual uintptr_t getApproximateFreeMemorySize();
	virtual uintptr_t getActualFreeEntryCount();

	MMINLINE virtual uintptr_t getCurrentLOASize()
	{
		return _loaSize;
	}

	MMINLINE virtual uintptr_t getApproximateFreeLOAMemorySize()
	{
		return _memoryPoolLargeObjects->getApproximateFreeMemorySize();
	}

	virtual void resetLargestFreeEntry();
	virtual uintptr_t getLargestFreeEntry();

	virtual void moveHeap(MM_EnvironmentBase* env, void* srcBase, void* srcTop, void* dstBase);

	/**
	 * Merge the free entry stats from both children pools.
	 */
	virtual void mergeFreeEntryAllocateStats();
	/**
	 * Merge the tlh allocation distribution stats from both children pools(include merging children pools).
	 */
	virtual void mergeTlhAllocateStats();
	/**
	 * Merge the current LargeObject allocation distribution stats from both children pools(include merging children pools).
	 */
	virtual void mergeLargeObjectAllocateStats();
	/**
	 * Merge Average allocation distribution stats from both children pools(include averaging children pools).
	 */
	virtual void averageLargeObjectAllocateStats(MM_EnvironmentBase* env, uintptr_t bytesAllocatedThisRound);
	/**
	 * Reset current LargeObject allocation stats and both children pools
	 */
	virtual void resetLargeObjectAllocateStats();

	/**
	 * @return the recorded estimate of dark matter in the receiver
	 */
	MMINLINE virtual uintptr_t getDarkMatterBytes()
	{
		return (_memoryPoolSmallObjects->getDarkMatterBytes() + _memoryPoolLargeObjects->getDarkMatterBytes());
	}

	/**
	 * @return the ratio of Large Object Area
	 */
	MMINLINE double
	getLOARatio()
	{
		return _currentLOARatio;
	}

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	virtual uintptr_t releaseFreeMemoryPages(MM_EnvironmentBase* env);
#endif

	/**
	 * Create a MemoryPoolLargeObjects object.
	 */
	MM_MemoryPoolLargeObjects(MM_EnvironmentBase* env, MM_MemoryPoolAddressOrderedListBase* largeObjectArea, MM_MemoryPoolAddressOrderedListBase* smallObjectArea)
		: MM_MemoryPool(env, 0)
		, _omrVM(env->getOmrVM())
		, _currentOldAreaSize(0)
		, _currentLOABase(NULL)
		, _memoryPoolSmallObjects(smallObjectArea)
		, _memoryPoolLargeObjects(largeObjectArea)
		, _loaSize(0)
		, _soaSize(0)
		, _currentLOARatio(_extensions->largeObjectAreaInitialRatio)
		, _minLOAFreeRatio(0)
		, _loaFreeRatioHistory(NULL)
		, _soaObjectSizeLWM(UDATA_MAX)
		, _soaFreeBytesAfterLastGC(0)
	{
		_typeId = __FUNCTION__;
	}
};


#endif /* MEMORYPOOLLARGEOBJECTS_HPP_ */
