/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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

#ifndef LARGEOBJECTALLOCATESTATS_HPP_
#define LARGEOBJECTALLOCATESTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "spacesaving.h"

#include "Base.hpp"
#include "LightweightNonReentrantLock.hpp"

#include "FreeEntrySizeClassStats.hpp"

class MM_EnvironmentBase;
class MM_FreeEntrySizeClassStats;

/*
 * Keeps track of the most frequent large allocations
 */
class MM_LargeObjectAllocateStats : public MM_Base {
public:
private:
	MM_EnvironmentBase *_env;				/**< cached thread environment */
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	uintptr_t _tlhMaximumSize;				/**< cached value of _tlhMaximumSize */
	uintptr_t _tlhMinimumSize;				/**< cached value of _tlhMinimumSize */
#endif
	OMRSpaceSaving *_spaceSavingSizes;	/**< Internal top-k-frequent data structure to maintain the stats for exact sizes */
	OMRSpaceSaving *_spaceSavingSizeClasses; /**< Internal top-k-frequent data structure to maintain the stats for size-classes */
	OMRSpaceSaving *_spaceSavingSizesAveragePercent;	/**< Internal top-k-frequent data structure to maintain the stats for exact sizes */
	OMRSpaceSaving *_spaceSavingSizeClassesAveragePercent; /**< Internal top-k-frequent data structure to maintain the stats for size-classes */
	OMRSpaceSaving *_spaceSavingTemp; /**< A temp spaceSaving containter used for average calculation */

	uint16_t _maxAllocateSizes;			/**< Maximum number of different allocation sizes we keep track of */
	uintptr_t _largeObjectThreshold;	/**< Threshold to consider an object large enough to keep track of */
	uintptr_t _veryLargeEntrySizeClass; /**< index of sizeClass for minimum veryLargeEntry, the cache value of GCExtensions.largeObjectAllocationProfilingVeryLargeObjectSizeClass */
	float _sizeClassRatio;			/**< ratio of lower and upper boundary of a size class */
	float _sizeClassRatioLog;		/**< log value of the above ratio */
	uintptr_t _averageBytesAllocated;   /**< history average of total allocated bytes for each of the updates */

	MM_FreeEntrySizeClassStats _freeEntrySizeClassStats; /**< global (still per pool) statistics structure for heap free entry size (sizeClass) distribution */
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	MM_FreeEntrySizeClassStats _tlhAllocSizeClassStats;  /**< distribution/historgram of tlh sizes actually allocated */
#endif
	uintptr_t *_sizeClassSizes;                              /**< size that represents each sizeClass */

	uint64_t _timeEstimateFragmentation;					 /**< The amount of time spent for estimating Fragmentation */
	uint64_t _cpuTimeEstimateFragmentation;					 /**< The amount of cputime spent for estimating Fragmentation */
	uint64_t _timeMergeAverage;								 /**< The amount of time spent for merging and averaging LargeAllocateStats */
	uintptr_t _remainingFreeMemoryAfterEstimate;			 /**< result of estimateFragmentation */
	uintptr_t _freeMemoryBeforeEstimate;					 /**< initial free memory before estimateFragmentation */
	uintptr_t _maxHeapSize;

	uintptr_t _TLHSizeClassIndex; /**< preserved next value of sizeClassIndex on last invocation of simulateAllocateTLHs */
	uintptr_t _TLHFrequentAllocationSize;/**< preserved next value of FrequentAllocationSize on last invocation of simulateAllocateTLHs */

	MMINLINE uintptr_t getNextSizeClass(uintptr_t sizeClassIndex, uintptr_t maxSizeClasses);
	MMINLINE bool isFirstIterationCompleteForCurrentStride(uintptr_t sizeClassIndex, uintptr_t maxSizeClasses);

	/**
	 * Historic weighted averaging of current stats with existing average stats. A private call. Called twice from public call for exact sizes and sizeClasses
	 * @param spaceSavingToAverageWith new (this round) top frequent large allocation stats
	 * @param spaceSavingAveragePercent old, average top frequent large allocation stats
	 * @param bytesAllocatedThisRound total new (this round) allocates (top frequent ones, plus any other)
	 */ 
	void averageForSpaceSaving(MM_EnvironmentBase *env, OMRSpaceSaving* spaceSavingToAverageWith, OMRSpaceSaving** spaceSavingAveragePercent, uintptr_t bytesAllocatedThisRound);

	/**
	 * Large allocation stats are initially done only on out-of-line allocates (does not fit within current TLH), so we will miss to count those that fit within TLH
	 * This method extrapolate allocate stats to account even for those that fit within TLH. For given allocation size and total bytes allocated out of line,
	 * the method returns extrapolated total amount of bytes (including in-line and out-of-line) for that size.
	 * The extrapolation logic relies on distribution of TLH sizes that have been created, and calculates probability that the size fits in each of TLH size classes.
	 * @param size for which large allocation stats extrapolation is done
	 * @param bytesAllocated total amount of bytes allocated out-of-line for this size
	 * @return esitamated total amount of bytes allocated (in-line and out-of-line) for this size
	 */
	uintptr_t upSampleAllocStats(MM_EnvironmentBase *env, uintptr_t size, uintptr_t bytesAllocated);

	/**
	 * Simulate (batch) object allocation from (also simulated) free memory profile
	 * @param totalAllocBytes size of total allocates of size objectSize
	 * @param objectSize size of object that we do batch simulated allocation
	 * @param[out] currentFreeMemory free memory after allocate
	 */
	uintptr_t simulateAllocateObjects(MM_EnvironmentBase *env, uintptr_t totalAllocBytes, uintptr_t objectSize, uintptr_t *currentFreeMemory);
	/**
	 * Simulate (batch) TLH allocation from (also simulated) free memory profile
	 * @param totalAllocBytes size of total TLH allocates
	 * @param strides = 1 or more than 1, how many strides need to split
	 * @param[out] currentFreeMemory free memory after allocate
	 */	
	uintptr_t simulateAllocateTLHs(MM_EnvironmentBase *env, uintptr_t totalAllocBytes, uintptr_t *currentFreeMemory, uintptr_t strides);

	/**
	 * share code between incrementFreeEntrySizeClassStats(), decrementFreeEntrySizeClassStats() and simulateAllocateObjects(), simulateAllocateTLHs()
	 */
	MMINLINE uintptr_t updateFreeEntrySizeClassStats(uintptr_t freeEntrySize, MM_FreeEntrySizeClassStats *freeEntrySizeClassStats, intptr_t count, uintptr_t sizeClassIndex, MM_FreeEntrySizeClassStats::FrequentAllocation* prev, MM_FreeEntrySizeClassStats::FrequentAllocation* curr);

public:
	/* Based on free memory stats and allocation profile, simulate allocation and project remaining unused (fragmented) memory */
	uintptr_t estimateFragmentation(MM_EnvironmentBase *env);

	/**
	 * Invoked by allocator to notify about a successful allocation.
	 * We increment counter for appropriate size or with some probability introduce a new size in the stats
	 * @param allocateSize size that was just allocated
	 */
	void allocateObject(uintptr_t allocateSize);

	/**
	 * Merge CURRENT this/these stats with provided stats. The result is stored back into this stats
     * @param statsToMerge to be added to this stats
	 */
	void mergeCurrent(MM_LargeObjectAllocateStats *statsToMerge);
	/**
	 * Merge AVERAGE this/these stats with provided stats. The result is stored back into this stats
     * @param statsToMerge to be added to this stats
	 */
	void mergeAverage(MM_LargeObjectAllocateStats *statsToMerge);

	/**
	 * Historic weighted averaging of current stats with existing average stats
	 */
	void average(MM_EnvironmentBase *env, uintptr_t bytesAllocatedThisRound);

	/**
	 * Encode 'native' precentage of free entry stats to a format internally used by SpaceSaving structure (uintptr_t)
	 */
	uintptr_t convertPercentFloatToUDATA(float percent);
	/**
	 * Decode precentage of free entry stats from format internally used by SpaceSaving structure (uintptr_t)
	 * to 'native' pecentage
	 */
	float convertPercentUDATAToFloat(uintptr_t percent);

	/**
	 * reset current stats. done when after merging current stats with average ones
	 */
	void resetCurrent();

	/**
	 * reset average stats. done on cumulative stats when merging average stats from children
	 */
	void resetAverage();
	

	static MM_LargeObjectAllocateStats *newInstance(MM_EnvironmentBase *env, uint16_t maxAllocateSizes, uintptr_t largeObjectThreshold, uintptr_t veryLargeObjectThreshold, float sizeClassRatio, uintptr_t maxHeapSize, uintptr_t tlhMaximumSize, uintptr_t tlhMinimumSize, uintptr_t factorVeryLargeEntryPool=1);
	void kill(MM_EnvironmentBase *env);

	bool initialize(MM_EnvironmentBase *env, uint16_t maxAllocateSizes, uintptr_t largeObjectThreshold, uintptr_t veryLargeObjectThreshold, float sizeClassRatio, uintptr_t maxHeapSize, uintptr_t tlhMaximumSize, uintptr_t tlhMinimumSize, uintptr_t factorVeryLargeEntryPool=1);
	void tearDown(MM_EnvironmentBase *env);
	
	uint16_t getMaxAllocateSizes() { return _maxAllocateSizes; }
	void setMaxAllocateSizes(uint16_t maxAllocateSizes) { _maxAllocateSizes = maxAllocateSizes; }
	
	uintptr_t getLargeObjectThreshold() { return _largeObjectThreshold; }
	
	OMRSpaceSaving *getSpaceSavingSizes() { return _spaceSavingSizes; }
	OMRSpaceSaving *getSpaceSavingSizeClasses() { return _spaceSavingSizeClasses; }
	OMRSpaceSaving *getSpaceSavingSizesAveragePercent() { return _spaceSavingSizesAveragePercent; }
	OMRSpaceSaving *getSpaceSavingSizeClassesAveragePercent() { return _spaceSavingSizeClassesAveragePercent; }

	/* free-memory profile specific methods */
	uintptr_t getMaxSizeClasses() { return _freeEntrySizeClassStats._maxSizeClasses; }
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	uintptr_t getMaxTLHSizeClasses() { return _tlhAllocSizeClassStats._maxSizeClasses; }
#endif

	uintptr_t getSizeClassIndex(uintptr_t size);

	MM_FreeEntrySizeClassStats *getFreeEntrySizeClassStats() { return &_freeEntrySizeClassStats; }

	/* accumulate freeEntryCount from FreeEntrySizeClassStats and verify the count with memoryPool actualFreeEntryCount  */
	void verifyFreeEntryCount(uintptr_t actualFreeEntryCount);
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	MM_FreeEntrySizeClassStats *getTlhAllocSizeClassStats() { return &_tlhAllocSizeClassStats; }
	uintptr_t getTlhAllocSizeClassCount(uintptr_t sizeClassIndex) { return _tlhAllocSizeClassStats._count[sizeClassIndex]; }
#endif
	uintptr_t getSizeClassSizes(uintptr_t sizeClassIndex) { return _sizeClassSizes[sizeClassIndex]; }

	uintptr_t incrementFreeEntrySizeClassStats(uintptr_t freeEntrySize);
	uintptr_t incrementFreeEntrySizeClassStats(uintptr_t freeEntrySize, MM_FreeEntrySizeClassStats *freeEntrySizeClassStats) {
		 return incrementFreeEntrySizeClassStats(freeEntrySize, freeEntrySizeClassStats, 1);
	}
	uintptr_t incrementFreeEntrySizeClassStats(uintptr_t freeEntrySize, MM_FreeEntrySizeClassStats *freeEntrySizeClassStats, uintptr_t count);
	void decrementFreeEntrySizeClassStats(uintptr_t freeEntrySize);
	void decrementFreeEntrySizeClassStats(uintptr_t freeEntrySize, MM_FreeEntrySizeClassStats *inFreeEntrySizeClassStats, uintptr_t count);
	void incrementTlhAllocSizeClassStats(uintptr_t freeEntrySize);

	uint64_t getTimeEstimateFragmentation() { return _timeEstimateFragmentation; }
	uint64_t getCPUTimeEstimateFragmentation() { return _cpuTimeEstimateFragmentation; }
	uint64_t getTimeMergeAverage() { return _timeMergeAverage; }
	void setTimeMergeAverage(uint64_t time) { _timeMergeAverage = time; }
	void addTimeMergeAverage(uint64_t time) { _timeMergeAverage += time; }
	uintptr_t getRemainingFreeMemoryAfterEstimate() { return _remainingFreeMemoryAfterEstimate; }
	void resetRemainingFreeMemoryAfterEstimate() { _remainingFreeMemoryAfterEstimate= 0; }
	uintptr_t getFreeMemoryBeforeEstimate() { return _freeMemoryBeforeEstimate; }
	uintptr_t getMaxHeapSize() {return _maxHeapSize; }
	uintptr_t getFreeMemory(){return _freeEntrySizeClassStats.getFreeMemory(_sizeClassSizes);}
	uintptr_t getPageAlignedFreeMemory(uintptr_t pageSize) {return _freeEntrySizeClassStats.getPageAlignedFreeMemory(_sizeClassSizes, pageSize);}


	MM_LargeObjectAllocateStats(MM_EnvironmentBase* env) :
		_env(env),
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
		_tlhMaximumSize(0),
		_tlhMinimumSize(0),
#endif
		_spaceSavingSizes(NULL),
		_spaceSavingSizeClasses(NULL),
		_spaceSavingSizesAveragePercent(NULL),
		_spaceSavingSizeClassesAveragePercent(NULL),
		_spaceSavingTemp(NULL),
		_maxAllocateSizes(0),
		_largeObjectThreshold(0),
		_veryLargeEntrySizeClass(0),
		_sizeClassRatio(0.0),
		_sizeClassRatioLog(0.0),
		_averageBytesAllocated(0),
		_timeEstimateFragmentation(0),
		_cpuTimeEstimateFragmentation(0),
		_timeMergeAverage(0),
		_remainingFreeMemoryAfterEstimate(0),
		_freeMemoryBeforeEstimate(0),
		_maxHeapSize(0),
		_TLHSizeClassIndex(0),
		_TLHFrequentAllocationSize(0)
	{
	}

};

#endif /* LARGEOBJECTALLOCATESTATS_HPP_ */
