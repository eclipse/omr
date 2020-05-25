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

#ifndef FREEENTRYSIZECLASSSTATS_HPP_
#define FREEENTRYSIZECLASSSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "spacesaving.h"

#include "Base.hpp"
#include "LightweightNonReentrantLock.hpp"

class MM_LargeObjectAllocateStats;

/** 
 * Keeps track of freeEntry size distribution in heap (per pool) 
 */
class MM_FreeEntrySizeClassStats {
public:
	struct FrequentAllocation {
		uintptr_t _size;  /**< frequent allocation size or a multiple of it */
		FrequentAllocation *_nextInSizeClass;
		/*TODO: there would be minus case for _count, make it intptr_t later */
		uintptr_t _count;
	};

private:
	/*TODO: there would be minus case for _count, make it intptr_t later */
	uintptr_t *_count;			/**< base of an array of counters for each sizeClass */
	FrequentAllocation **_frequentAllocationHead; /**< for each size class, there is a list of ascending (per size, not per count) frequent allocation (exact) sizes) */
	uintptr_t _maxSizeClasses;  /**< size of the above array */

	FrequentAllocation *_frequentAllocation; /**< an array of FrequentAllocation structs of size _maxFrequentAllocateSizes (current size _frequentAllocateSizeCounters) */
	FrequentAllocation *_veryLargeEntryPool; /**< an array of FrequentAllocation structs for veryLargeEntries */
	FrequentAllocation *_freeHeadVeryLargeEntry; /**< the head of linklist of free veryLargeEntries */
	float	*_fractionFrequentAllocation; /**< fraction Array for cumulating the fraction of frequentAllocation during estimating fragmentation */
	uintptr_t _maxFrequentAllocateSizes; /**< max size of _frequentAllocation array */
	uintptr_t _maxVeryLargeEntrySizes; /**< max size of _veryLargeEntryPool array */
#define VERY_LARGE_ENTRY_POOL_SIZE_FOR_THREAD 3
	uintptr_t _veryLargeEntrySizeClass; /**< index of sizeClass for minimum veryLargeEntry, the cache value of GCExtensions.largeObjectAllocationProfilingVeryLargeObjectSizeClass */
#define MAX_FREE_ENTRY_COUNTERS_PER_FREQ_ALLOC_SIZE 5
	uintptr_t _frequentAllocateSizeCounters; /**< current size of _frequentAllocation array */

	MM_LightweightNonReentrantLock _lock;  /**< lock used during merge of thread local stats */
	bool guarantyEnoughPoolSizeForVeryLargeEntry; /**< true for all memory pool, false for thread base */
private:
	/**
	 * Take a snapshot of this structure stats. 
	 * @param[out] stats structure where the snapshot is stored
	 * @param[in] sizes of all sizeclasses used to calculate total free memory returned
	 * @return total free memory represented by this structure
	 */
	uintptr_t copyTo(MM_FreeEntrySizeClassStats *stats, const uintptr_t sizeClassSizes[]);

	FrequentAllocation* copyVeryLargeEntry(FrequentAllocation* entry);
public:
	/* return count of 'regular' free entires (not those that are frequent allocates for a given size class */
	uintptr_t getCount(uintptr_t sizeClassIndex) { return _count[sizeClassIndex]; }
	uintptr_t getVeryLargeEntrySizeClass() { return _veryLargeEntrySizeClass; }

	/* return count of all frequent allocate free entires */
	uintptr_t getFrequentAllocCount(uintptr_t sizeClassIndex);
	/* return the head of the lisf of all frequent allocate entries for a give size class */
	FrequentAllocation *getFrequentAllocationHead(uintptr_t sizeClassIndex) { return _frequentAllocationHead[sizeClassIndex]; }
	/* return total free memory represented by this structure */
	uintptr_t getFreeMemory(const uintptr_t sizeClassSizes[]);
	/* return the 'average' number of pages which can be freed */
	uintptr_t getPageAlignedFreeMemory(const uintptr_t sizeClassSizes[], uintptr_t pageSize);

	/**< @param factorVeryLargeEntryPool : multiple factor for _maxVeryLargeEntrySizes, default = 1, double for splitFreeList case 
	 *   @param simulation : if true, generate _fractionFrequentAllocation array for cumulating fraction of frequentAllocation during estimating fragmentation, default = false
	 */
	bool initialize(MM_EnvironmentBase *env, uintptr_t maxAllocateSizes, uintptr_t maxSizeClasses, uintptr_t veryLargeObjectThreshold, uintptr_t factorVeryLargeEntryPool=1, bool simulation=false);
	void tearDown(MM_EnvironmentBase *env);

	/**< Reset counts for size class entries and for frequent allocations */
	void resetCounts();
	/**< Remove any frequent allocation sizes associated with size class entries, 
	 * they will be rebuilt soon by initializeFrequentAllocation
	 */
	void clearFrequentAllocation();

	/**< Merge the count of sizeClass with negative count of Very Large Entries
	 *  correct inconsistent count, which is generated by the lack of size of VeryLargeObjectPool.
	 */
	void mergeCountForVeryLargeEntries();

	/**< Merge provided stats with this free entry sizeClass stats */
	void merge(MM_FreeEntrySizeClassStats *stats);
	/**
	 * Merge provided stats with this free entry sizeClass stats
	 * Safe in multi-threaded environment.
	 * Provided stats cleared (thus ready for further gathering of current stats
	 */
	void mergeLocked(MM_FreeEntrySizeClassStats *stats) {
		_lock.acquire();
		merge(stats);
		_lock.release();

		stats->resetCounts();
	}

	/**< build the list of frequent allocation exact sizes for each size class */
	void initializeFrequentAllocation(MM_LargeObjectAllocateStats *largeObjectAllocateStats);
	/**< initialize veryLargeEntryPool -- link free veryLargeEntries in _freeHeadVeryLargeEntry list */
	void initializeVeryLargeEntryPool();

	friend class MM_LargeObjectAllocateStats;

	MM_FreeEntrySizeClassStats() :
		_count(NULL),
		_frequentAllocationHead(NULL),
		_maxSizeClasses(0),
		_frequentAllocation(NULL),
		_veryLargeEntryPool(NULL),
		_fractionFrequentAllocation(NULL),
		_maxFrequentAllocateSizes(0),
		_maxVeryLargeEntrySizes(0),
		_veryLargeEntrySizeClass(0),
		_frequentAllocateSizeCounters(0)
	{
	}
};

#endif /* FREEENTRYSIZECLASSSTATS_HPP_ */
