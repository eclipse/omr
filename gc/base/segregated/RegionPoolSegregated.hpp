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

#if !defined(REGIONPOOLSEGREGATED_HPP_)
#define REGIONPOOLSEGREGATED_HPP_

#include "omrcomp.h"
#include "sizeclasses.h"

#include "HeapRegionList.hpp"
#include "HeapRegionManager.hpp"
#include "LockingHeapRegionQueue.hpp"
#include "RegionPool.hpp"
#include "SweepSchemeSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_EnvironmentBase;
class MM_FreeHeapRegionList;
class MM_HeapRegionDescriptorSegregated;
class MM_HeapRegionQueue;
class MM_LockingHeapRegionQueue;

#define PRIMARY_BUCKET 0
#define SKIP_AVAILABLE_REGION_FOR_ALLOCATION 1

/*
 * Thresholds for dividing available list into buckets so that we tend to
 * defrag low occupancy regions and allocate into high occupancy ones.
 * Numbers are occupancy * 100 /numCells.  Last one must be zero.
 * The first bucket in the list is never defragmented, by design, and so is
 * also the receptacle for all pinned regions.
 */
#define NUM_DEFRAG_BUCKETS 4
#define DEFRAG_BUCKET_THRESHOLDS {50, 30, 15, 0}

/**
 * A region pool which manages lists of small objects (empty, partially used, full, sweep...), arraylet leafs and large objects regions.
 * 
 * @ingroup GC_Realtime
 */
class MM_RegionPoolSegregated : public MM_RegionPool
{
	/*
	 * Data members
	 */
private:
	MM_HeapRegionManager *_heapRegionManager; /**< The Region Manager which contains all the descriptors used for allocation (they contain the Memory Pools which manage their underlying memory) */
	MM_SweepSchemeSegregated *_sweepScheme;

	MM_FreeHeapRegionList *_singleFreeList; /**< Singleton free regions. */
	MM_FreeHeapRegionList *_multiFreeList; /**< Contiguous free regions (may include regions that are actually singletons). */
	MM_FreeHeapRegionList *_coalesceFreeList; /**< Free regions that might be coalescable, so avoid allocating from them if at all possible. */
	
	/** 
	 * @note No allocation is currently happening on the available regions.  These are roughly sorted
	 * into buckets by occupancy, with bucket 0 being the most occupied.  Evacuation for
	 * defragmentation purposes prefers the least occupied regions while allocation prefers the
	 * most occupied.
	*/
	MM_LockingHeapRegionQueue *_smallAvailableRegions[OMR_SIZECLASSES_NUM_SMALL+1][NUM_DEFRAG_BUCKETS]; /**< Regions that are available to be given out to allocation contexts and aren't entirely free. */
	
	/** 
	 * @note Some of the full regions may be attached to AllocationContexts, and thus being actively
	 * allocated into.
	 */
	MM_HeapRegionQueue *_smallFullRegions[OMR_SIZECLASSES_NUM_SMALL+1]; /**< Regions that have been allocated into during this GC cycle. */
	
	/**
	 * @note The sweep regions will contain a mix of marked (live) and unmarked (dead) objects.
	 */
	MM_HeapRegionQueue *_smallSweepRegions[OMR_SIZECLASSES_NUM_SMALL+1]; /**< Regions that are waiting to be swept during this GC cycle. */

	/** 
	 * @note No allocation is currently happening on the available regions.  These are roughly sorted
	 * into buckets by occupancy, with bucket 0 being the most occupied.  Evacuation for
	 * defragmentation purposes prefers the least occupied regions while allocation prefers the
	 * most occupied.
	*/
	MM_HeapRegionQueue *_arrayletAvailableRegions; /**< Arraylet regions that are available to be given out to allocation contexts and aren't entirely free. */
	
	/** 
	 * @note Some of the full regions may be attached to AllocationContexts, and thus being actively
	 * allocated into.
	 */
	MM_HeapRegionQueue *_arrayletFullRegions; /**< Arraylet regions that have been allocated into during this GC cycle. */
	
	/**
	 * @note The sweep regions will contain a mix of marked (live) and unmarked (dead) objects.
	 */
	MM_HeapRegionQueue *_arrayletSweepRegions; /**< Arraylet regions that are waiting to be swept during this GC cycle. */
	
	/** 
	 * @note Some of the full regions may be attached to AllocationContexts, and thus being actively
	 * allocated into.
	 */
	MM_HeapRegionQueue *_largeFullRegions; /**< Large object regions that have been allocated into during this GC cycle. */
	
	/**
	 * @note The sweep regions will contain a mix of marked (live) and unmarked (dead) objects.
	 */
	MM_HeapRegionQueue *_largeSweepRegions; /**< Large object regions that are waiting to be swept during this GC cycle. */

	volatile uintptr_t _regionsInUse; /**< Number of regions that are in use (not on a free list). */
	
	/**	
	 * @note Maintain average occupancy (used cells/total cells), calculated after sweep
	 * and needed for a heuristic in sweep-while-allocate optimization.
	 */
	float _smallOccupancy[OMR_SIZECLASSES_NUM_SMALL+1];
	volatile uintptr_t _darkMatterCellCount[OMR_SIZECLASSES_MAX_SMALL + 1];
	uintptr_t _initialCountOfSweepRegions[OMR_SIZECLASSES_NUM_SMALL + 1];
	volatile uintptr_t _currentCountOfSweepRegions[OMR_SIZECLASSES_MAX_SMALL + 1];
	uintptr_t _initialTotalCountOfSweepRegions;
	volatile uintptr_t _currentTotalCountOfSweepRegions;
	
	bool _isSweepingSmall; /**< if GC is sweeping small pages */
	uintptr_t _splitAvailableListSplitCount; /* number of split available region queues per size class per defragment bucket */
	uint8_t _skipAvailableRegionForAllocation[OMR_SIZECLASSES_NUM_SMALL+1]; /* per size class flag to indicate if there is any available regions left for allocation for that size class */


protected:
public:
	
	/*
	 * Function members
	 */
private:
	MMINLINE void 
	incrementRegionsInUse(uintptr_t value)
	{
		MM_AtomicOperations::add(&_regionsInUse, value);
	}
	MMINLINE void 
	decrementRegionsInUse(uintptr_t value)
	{
		MM_AtomicOperations::subtract(&_regionsInUse, value);
	}
	
protected:
public:
	/**
	 * Create and initialize a new instance of the receiver.
	 */
	static MM_RegionPoolSegregated *newInstance(MM_EnvironmentBase *env, MM_HeapRegionManager *heapRegionManager);
	
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	static MM_HeapRegionQueue* allocateHeapRegionQueue(MM_EnvironmentBase *env, MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly, bool concurrentAccess, bool trackFreeBytes);
	static MM_FreeHeapRegionList* allocateFreeHeapRegionList(MM_EnvironmentBase *env, MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly);
	MM_HeapRegionDescriptorSegregated *allocateFromRegionPool(MM_EnvironmentBase *env, uintptr_t numRegions, uintptr_t szClass, uintptr_t maxExcess);
	MM_HeapRegionDescriptorSegregated *allocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass);
	MM_HeapRegionDescriptorSegregated *allocateRegionFromArrayletSizeClass(MM_EnvironmentBase *env);
	MM_HeapRegionDescriptorSegregated *sweepAndAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass);
	void enqueueAvailable(MM_HeapRegionDescriptorSegregated *region, uintptr_t sizeClass, uintptr_t occupancy, uintptr_t splitListIndex);

	/**
 	 * For all size classes, move all regions in that size class from "in use"
 	 * region lists to "sweep" region lists.
 	 */
	void moveInUseToSweep(MM_EnvironmentBase *env);
	void countFreeRegions(uintptr_t *singleFree, uintptr_t *multiFree, uintptr_t *maxMultiFree, uintptr_t *coalesceFree);
	void addFreeRange(void *lowAddress, void *highAddress);
	void addFreeRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region, bool alreadyFree = false);
	void addSingleFree(MM_EnvironmentBase *env, MM_HeapRegionQueue *regionQueue);
	
	MMINLINE uintptr_t roundUpRegion(uintptr_t size) {	return (size + _heapRegionManager->getRegionSize() - 1) & (~(_heapRegionManager->getRegionSize() - 1)); }
	MMINLINE uintptr_t roundDownRegion(uintptr_t size) { return (size) & (~(_heapRegionManager->getRegionSize() - 1)); }
	MMINLINE uintptr_t divideDownRegion(uintptr_t size)	{ /* TODO: use shift operation once region log2 size is introduced */ return size / _heapRegionManager->getRegionSize(); }
	MMINLINE uintptr_t divideUpRegion(uintptr_t size)	{ /* TODO: use shift operation once region log2 size is introduced */ return (size + _heapRegionManager->getRegionSize() - 1) / _heapRegionManager->getRegionSize(); }
	
	uintptr_t getRegionsInuse() { return _regionsInUse; }	

	MMINLINE uintptr_t getInitialCountOfSweepRegions(uintptr_t sizeClass) const
	{
		assume(sizeClass >= OMR_SIZECLASSES_MIN_SMALL && sizeClass <= OMR_SIZECLASSES_MAX_SMALL, "getOccupancy: invalid sizeclass");
		return _initialCountOfSweepRegions[sizeClass];
	}	
	
	MMINLINE uintptr_t getInitialTotalCountOfSweepRegions () const
	{
		return _initialTotalCountOfSweepRegions;
	}
	
	MMINLINE uintptr_t getCurrentCountOfSweepRegions(uintptr_t sizeClass) const
	{
		assume(sizeClass >= OMR_SIZECLASSES_MIN_SMALL && sizeClass <= OMR_SIZECLASSES_MAX_SMALL, "getOccupancy: invalid sizeclass");
		return _currentCountOfSweepRegions[sizeClass];
	}
	
	MMINLINE void decrementCurrentCountOfSweepRegions(uintptr_t sizeClass, uintptr_t count)
	{
		MM_AtomicOperations::subtract(&_currentCountOfSweepRegions[sizeClass], count);
	}	
	
	MMINLINE uintptr_t getCurrentTotalCountOfSweepRegions () const
	{
		return _currentTotalCountOfSweepRegions;
	}
	
	MMINLINE void decrementCurrentTotalCountOfSweepRegions (uintptr_t count)
	{
		MM_AtomicOperations::subtract(&_currentTotalCountOfSweepRegions, count);
	}
	
	MMINLINE void addDarkMatterCellsAfterSweepForSizeClass(uintptr_t sizeClass, uintptr_t cellCount) {
		MM_AtomicOperations::add(&_darkMatterCellCount[sizeClass], cellCount);
	}	
	
	MMINLINE float getOccupancy(uintptr_t sizeClass)
	{
		assume(sizeClass >= OMR_SIZECLASSES_MIN_SMALL && sizeClass <= OMR_SIZECLASSES_MAX_SMALL, "getOccupancy: invalid sizeclass");
		return _smallOccupancy[sizeClass];
	}
	
	MMINLINE uintptr_t getSplitAvailableListSplitCount() { return _splitAvailableListSplitCount; }

	void setSweepSmallPages(bool sweepSmall) { _isSweepingSmall = sweepSmall; }
	void resetSkipAvailableRegionForAllocation() { memset(&_skipAvailableRegionForAllocation[0], 0, sizeof(_skipAvailableRegionForAllocation)); }

	void updateOccupancy (uintptr_t sizeClass, uintptr_t occupancy);
	

	MMINLINE MM_FreeHeapRegionList *getSingleFreeList() { return _singleFreeList; }
	MMINLINE MM_FreeHeapRegionList *getMultiFreeList() { return _multiFreeList; }
	MMINLINE MM_FreeHeapRegionList *getCoalesceFreeList() { return _coalesceFreeList; }
	MMINLINE MM_HeapRegionQueue *getLargeSweepRegions() { return _largeSweepRegions; }
	MMINLINE MM_HeapRegionQueue *getLargeFullRegions() { return _largeFullRegions; }
	MMINLINE MM_HeapRegionQueue *getArrayletSweepRegions() { return _arrayletSweepRegions; }
	MMINLINE MM_HeapRegionQueue *getArrayletFullRegions() { return _arrayletFullRegions; }
	MMINLINE MM_HeapRegionQueue *getArrayletAvailableRegions() { return _arrayletAvailableRegions; }
	MMINLINE MM_LockingHeapRegionQueue *getSmallAvailableRegions(uintptr_t sizeClass, uintptr_t defragBucket, uintptr_t splitList) { return &_smallAvailableRegions[sizeClass][defragBucket][splitList]; }
	MMINLINE MM_HeapRegionQueue *getSmallSweepRegions(uintptr_t sizeClass) { return _smallSweepRegions[sizeClass]; }
	MMINLINE MM_HeapRegionQueue *getSmallFullRegions(uintptr_t sizeClass) { return _smallFullRegions[sizeClass]; }
	MMINLINE uintptr_t getDarkMatterCellCount(uintptr_t sizeClass) { return _darkMatterCellCount[sizeClass]; }

	void joinBucketListsForSplitIndex(MM_EnvironmentBase *env);
	
	void setSweepScheme(MM_SweepSchemeSegregated *sweepScheme) { _sweepScheme = sweepScheme; }

	/**
	 * Create a RegionPoolSegregated object.
	 */
	MM_RegionPoolSegregated(MM_EnvironmentBase *env, MM_HeapRegionManager *heapRegionManager) :
		MM_RegionPool(env)
		, _heapRegionManager(heapRegionManager)
		, _sweepScheme(NULL)
		, _singleFreeList(NULL)
		, _multiFreeList(NULL)
		, _coalesceFreeList(NULL)
		, _arrayletAvailableRegions(NULL)
		, _arrayletFullRegions(NULL)
		, _arrayletSweepRegions(NULL)
		, _largeFullRegions(NULL)
		, _largeSweepRegions(NULL)
		, _regionsInUse(0)
		, _isSweepingSmall(false)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* REGIONPOOLSEGREGATED_HPP_ */
