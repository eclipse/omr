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
 
#include <math.h>
#include <stdlib.h>

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "FreeEntrySizeClassStats.hpp"
#include "GCExtensionsBase.hpp"
#include "LargeObjectAllocateStats.hpp"
#include "ModronAssertions.h"


bool
MM_FreeEntrySizeClassStats::initialize(MM_EnvironmentBase *env, uintptr_t maxAllocateSizes, uintptr_t maxSizeClasses, uintptr_t veryLargeObjectThreshold, uintptr_t factorVeryLargeEntryPool, bool simulation)
{
	_maxSizeClasses = maxSizeClasses;
	_maxFrequentAllocateSizes = maxAllocateSizes;
	_veryLargeEntrySizeClass = env->getExtensions()->largeObjectAllocationProfilingVeryLargeObjectSizeClass;
	_maxVeryLargeEntrySizes = 0;

	if (0 != _maxSizeClasses) {
		_count = (uintptr_t *)env->getForge()->allocate(sizeof(uintptr_t) * _maxSizeClasses, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

		if (NULL == _count) {
			return false;
		}

		/* _maxFrequentAllocateSizes is set to 0 when we use this structure to gather TLH allocation profile, which has no need for frequent allocations */
		if (0 != _maxFrequentAllocateSizes) {
			_frequentAllocationHead = (FrequentAllocation **)env->getForge()->allocate(sizeof(FrequentAllocation *) * _maxSizeClasses, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

			if (NULL == _frequentAllocationHead) {
				return false;
			}

			_frequentAllocation = (FrequentAllocation *)env->getForge()->allocate(sizeof(FrequentAllocation) * MAX_FREE_ENTRY_COUNTERS_PER_FREQ_ALLOC_SIZE * _maxFrequentAllocateSizes, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

			if (NULL == _frequentAllocation) {
				return false;
			}

			if (simulation) {
				_fractionFrequentAllocation = (float *)env->getForge()->allocate(sizeof(float)*_maxFrequentAllocateSizes, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
				if (NULL == _fractionFrequentAllocation) {
					return false;
				}
			}

			if (veryLargeObjectThreshold > env->getExtensions()->memoryMax) {
				_veryLargeEntryPool = NULL;
			} else {
				/* if veryLargeObjectThreshold == 0, set veryLargePool size = VERY_LARGE_ENTRY_POOL_SIZE_FOR_THREAD for thread base FreeEntrySizeClassStats */
				uintptr_t count = VERY_LARGE_ENTRY_POOL_SIZE_FOR_THREAD;
				if (0 != veryLargeObjectThreshold) {
					count = env->getExtensions()->memoryMax / veryLargeObjectThreshold * factorVeryLargeEntryPool;
					guarantyEnoughPoolSizeForVeryLargeEntry = true;
				} else {
					guarantyEnoughPoolSizeForVeryLargeEntry = false;
				}

				_veryLargeEntryPool = (FrequentAllocation *)env->getForge()->allocate(sizeof(FrequentAllocation) * count, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

				if (NULL == _veryLargeEntryPool) {
					return false;
				}
				_maxVeryLargeEntrySizes = count;
			}
		}

		clearFrequentAllocation();
		initializeVeryLargeEntryPool();
		resetCounts();

		if (!_lock.initialize(env, &env->getExtensions()->lnrlOptions, "MM_FreeEntrySizeClassStats:_lock")) {
			return false;
		}

	}

	return true;
}

void
MM_FreeEntrySizeClassStats::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _count) {
		env->getForge()->free(_count);
		_count = NULL;
	}

	if (NULL != _frequentAllocationHead) {
		env->getForge()->free(_frequentAllocationHead);
		_frequentAllocationHead = NULL;
	}

	if (NULL != _frequentAllocation) {
		env->getForge()->free(_frequentAllocation);
		_frequentAllocation = NULL;
	}

	if(NULL != _fractionFrequentAllocation) {
		env->getForge()->free(_fractionFrequentAllocation);
		_fractionFrequentAllocation = NULL;
	}

	if (NULL != _veryLargeEntryPool) {
		env->getForge()->free(_veryLargeEntryPool);
		_veryLargeEntryPool = NULL;
	}
	_lock.tearDown();
}

void
MM_FreeEntrySizeClassStats::mergeCountForVeryLargeEntries()
{
	if (NULL != _frequentAllocationHead) {
		for (uintptr_t sizeClassIndex = _veryLargeEntrySizeClass; sizeClassIndex < _maxSizeClasses; sizeClassIndex++) {
			if (NULL != _frequentAllocationHead[sizeClassIndex]) {
				FrequentAllocation* curr = _frequentAllocationHead[sizeClassIndex];
				FrequentAllocation* prev = NULL;
				FrequentAllocation* next = NULL;

				while (NULL != curr) {
					if (0 > ((intptr_t)curr->_count)) {
						_count[sizeClassIndex] += curr->_count;
						curr->_count = 0;
					}
					next = curr->_nextInSizeClass;
					if (0 == curr->_count) {
						/* remove from list */
						if (NULL == prev) {
							_frequentAllocationHead[sizeClassIndex] = next;
						} else {
							prev->_nextInSizeClass = next;
						}
						curr->_nextInSizeClass = _freeHeadVeryLargeEntry;
						_freeHeadVeryLargeEntry = curr;
					} else {
						prev = curr;
					}
					curr = next;
				}
			}
			Assert_MM_true(0 <= ((intptr_t)_count[sizeClassIndex]));
		}
	}
}

void
MM_FreeEntrySizeClassStats::merge(MM_FreeEntrySizeClassStats *stats)
{
	Assert_MM_true(stats->_maxSizeClasses <= _maxSizeClasses);
	for (uintptr_t sizeClassIndex = 0; sizeClassIndex < stats->_maxSizeClasses; sizeClassIndex++) {
		_count[sizeClassIndex] += stats->_count[sizeClassIndex];
		if (NULL != _frequentAllocationHead) {
			if (sizeClassIndex >= _veryLargeEntrySizeClass) {
				if (NULL != stats->_frequentAllocationHead[sizeClassIndex]) {
					FrequentAllocation* dest = _frequentAllocationHead[sizeClassIndex];
					FrequentAllocation* predest = NULL;
					FrequentAllocation* src = stats->_frequentAllocationHead[sizeClassIndex];
					FrequentAllocation* newEntry = NULL;
					while (NULL != src) {
						if (0 == src->_count) {
							src = src->_nextInSizeClass;
						} else if ((NULL == dest) || (dest->_size > src->_size)) {
							newEntry = copyVeryLargeEntry(src);
							newEntry->_nextInSizeClass = dest;
							if (NULL == predest) {
								_frequentAllocationHead[sizeClassIndex] = newEntry;
							} else {
								predest->_nextInSizeClass = newEntry;
							}
							predest = newEntry;
							src = src->_nextInSizeClass;
						} else if (dest->_size == src->_size) {
							FrequentAllocation* nextdest = dest->_nextInSizeClass;
							dest->_count += src->_count;
							if (0 == dest->_count) {
								if (NULL == predest) {
									_frequentAllocationHead[sizeClassIndex] = nextdest;
								} else {
									predest->_nextInSizeClass = nextdest;
								}
								dest->_nextInSizeClass = _freeHeadVeryLargeEntry;
								_freeHeadVeryLargeEntry = dest;
							} else {
								predest = dest;
							}
							dest = nextdest;
							src = src->_nextInSizeClass;

						} else {
							predest = dest;
							dest = dest->_nextInSizeClass;
						}
					}
				}
			} else {
				FrequentAllocation *curr = _frequentAllocationHead[sizeClassIndex];
				while (NULL != curr) {

					FrequentAllocation *statsCurr = stats->_frequentAllocationHead[sizeClassIndex];
					while ((NULL != statsCurr) && (curr->_size != statsCurr->_size)) {
						statsCurr = statsCurr->_nextInSizeClass;
					}

					if ((NULL != statsCurr) && (curr->_size == statsCurr->_size)) {
						curr->_count += statsCurr->_count;
					}

					curr = curr->_nextInSizeClass;
				}
			}
		}
	}
}

uintptr_t
MM_FreeEntrySizeClassStats::getFreeMemory(const uintptr_t sizeClassSizes[])
{
	uintptr_t totalFreeMemory = 0;

	for (uintptr_t sizeClassIndex = 0; sizeClassIndex < _maxSizeClasses; sizeClassIndex++) {

		/* regular sizes */
		totalFreeMemory += _count[sizeClassIndex] * sizeClassSizes[sizeClassIndex];

		/* for each size class, find frequent allocation sizes */
		if (NULL != _frequentAllocationHead) {
			MM_FreeEntrySizeClassStats::FrequentAllocation *curr = _frequentAllocationHead[sizeClassIndex];
			while (NULL != curr) {
				totalFreeMemory += curr->_count * curr->_size;
				curr = curr->_nextInSizeClass;
			}
		}
	}

	return totalFreeMemory;
}

uintptr_t 
MM_FreeEntrySizeClassStats::getPageAlignedFreeMemory(const uintptr_t sizeClassSizes[], uintptr_t pageSize) {

	uintptr_t resusableFreeMemory = 0;

	for (uintptr_t sizeClassIndex = 0; sizeClassIndex < _maxSizeClasses; sizeClassIndex++) {
		
		/* Check if the current size class is larger than the page size */
		if (pageSize < sizeClassSizes[sizeClassIndex]) {

			/* regular sizes */
			resusableFreeMemory += _count[sizeClassIndex] * (sizeClassSizes[sizeClassIndex] - pageSize);
			
			/* 
			 * We need to find how many pages of specified size, would fit into free entries. 
			 * Since we don't have exact info about the start and end of each free entry (we only have low bound size and count of each entry)
			 * we use a simple heuristic that freeEntrySize - pageSize of bytes, in average will fit. For example if free entry is 6K and page 
			 * is 4K, in average 2K will be reusable (50% chances it will be 1 full page, and 50% no page will fit).
			 */

			/* for each size class, find frequent allocation sizes */
			if (NULL != _frequentAllocationHead) {
				MM_FreeEntrySizeClassStats::FrequentAllocation *curr = _frequentAllocationHead[sizeClassIndex];
				while (NULL != curr) {
					resusableFreeMemory += curr->_count * (curr->_size - pageSize);
					curr = curr->_nextInSizeClass;
				}
			}
		}
	}	

	return resusableFreeMemory;
}

uintptr_t
MM_FreeEntrySizeClassStats::copyTo(MM_FreeEntrySizeClassStats *stats, const uintptr_t sizeClassSizes[])
{
	uintptr_t totalFreeMemory = 0;
	const uintptr_t maxFrequentAllocateSizeCounters = MAX_FREE_ENTRY_COUNTERS_PER_FREQ_ALLOC_SIZE * _maxFrequentAllocateSizes;

	Assert_MM_true(stats->_maxSizeClasses == _maxSizeClasses);
	stats->_frequentAllocateSizeCounters = 0;
	stats->_veryLargeEntrySizeClass = _veryLargeEntrySizeClass;
	stats->resetCounts();
	for (uintptr_t sizeClassIndex = 0; sizeClassIndex < _maxSizeClasses; sizeClassIndex++) {

		/* copy the _count info */
		stats->_count[sizeClassIndex] = _count[sizeClassIndex];

		totalFreeMemory += _count[sizeClassIndex] * sizeClassSizes[sizeClassIndex];

		/* for each size class, find frequent allocation sizes */
		if (NULL != _frequentAllocationHead) {
			Assert_MM_true(NULL != stats->_frequentAllocationHead);

			FrequentAllocation *curr = _frequentAllocationHead[sizeClassIndex];
			FrequentAllocation *statsPrev = NULL;

			while (NULL != curr) {
				totalFreeMemory += curr->_count * curr->_size;
				FrequentAllocation* statsCurr = NULL;

				if (sizeClassIndex >= _veryLargeEntrySizeClass) {
					Assert_MM_true(NULL != stats->_freeHeadVeryLargeEntry);
					statsCurr = stats->_freeHeadVeryLargeEntry;
					stats->_freeHeadVeryLargeEntry = statsCurr->_nextInSizeClass;
				} else {
					/* get a new FrequentAllocation from the pool */
					Assert_MM_true(stats->_frequentAllocateSizeCounters < maxFrequentAllocateSizeCounters);
					statsCurr = &stats->_frequentAllocation[stats->_frequentAllocateSizeCounters];
					stats->_frequentAllocateSizeCounters += 1;
				}
				/* connect it */
				if (curr == _frequentAllocationHead[sizeClassIndex]) {
					/* first one */
					stats->_frequentAllocationHead[sizeClassIndex] = statsCurr;
				} else {
					/* subsequent ones */
					statsPrev->_nextInSizeClass = statsCurr;
				}

				/* copy the size and count info */
				statsCurr->_size = curr->_size;
				statsCurr->_count = curr->_count;

				/* move to the next one */
				curr = curr->_nextInSizeClass;
				statsPrev = statsCurr;
			}

			/* last one copied points to null */
			if (NULL == statsPrev) {
				stats->_frequentAllocationHead[sizeClassIndex] = NULL;
			} else {
				statsPrev->_nextInSizeClass = NULL;
			}
		}
	}

	return totalFreeMemory;
}

MM_FreeEntrySizeClassStats::FrequentAllocation *
MM_FreeEntrySizeClassStats::copyVeryLargeEntry(FrequentAllocation* entry)
{
	Assert_MM_true(NULL != _freeHeadVeryLargeEntry);
	FrequentAllocation* newEntry = _freeHeadVeryLargeEntry;
	_freeHeadVeryLargeEntry = newEntry->_nextInSizeClass;
	newEntry->_size = entry->_size;
	newEntry->_count = entry->_count;
	newEntry->_nextInSizeClass = NULL;
	return newEntry;
}

void
MM_FreeEntrySizeClassStats::resetCounts() {
	for (uintptr_t sizeClassIndex = 0; sizeClassIndex < _maxSizeClasses; sizeClassIndex++) {
		_count[sizeClassIndex] = 0;
		if (0 != _maxFrequentAllocateSizes) {
			FrequentAllocation *curr = _frequentAllocationHead[sizeClassIndex];
			if (sizeClassIndex >= _veryLargeEntrySizeClass) {
				FrequentAllocation* prev = NULL;
				if (NULL != curr) {
					while (NULL != curr) {
						prev = curr;
						curr->_count = 0;
						curr = curr->_nextInSizeClass;
					}
					prev->_nextInSizeClass = _freeHeadVeryLargeEntry;
					_freeHeadVeryLargeEntry = _frequentAllocationHead[sizeClassIndex];
					_frequentAllocationHead[sizeClassIndex] = NULL;
				}
			} else {
				while (NULL != curr) {
					curr->_count = 0;
					curr = curr->_nextInSizeClass;
				}
			}
		}
	}
}

void
MM_FreeEntrySizeClassStats::clearFrequentAllocation()
{
	if (0 != _maxFrequentAllocateSizes) {
		for (uintptr_t sizeClassIndex = 0; sizeClassIndex < _maxSizeClasses; sizeClassIndex++) {
			if (sizeClassIndex < _veryLargeEntrySizeClass) {
				_frequentAllocationHead[sizeClassIndex] = NULL;
			}
		}

		/* reset the 'global pool' of FrequentAllocations */
		_frequentAllocateSizeCounters = 0;
	}
}

void
MM_FreeEntrySizeClassStats::initializeVeryLargeEntryPool()
{
	if (0 != _maxFrequentAllocateSizes) {
		for (uintptr_t sizeClassIndex = _veryLargeEntrySizeClass; sizeClassIndex < _maxSizeClasses; sizeClassIndex++) {
			_frequentAllocationHead[sizeClassIndex] = NULL;
		}
	
		_freeHeadVeryLargeEntry = NULL;
		if ((NULL != _veryLargeEntryPool) && (0 != _maxVeryLargeEntrySizes)) {
			for (uintptr_t index = 0; index < _maxVeryLargeEntrySizes; index++) {
				_veryLargeEntryPool[index]._nextInSizeClass = _freeHeadVeryLargeEntry;
				_veryLargeEntryPool[index]._count = 0;
				_veryLargeEntryPool[index]._size = 0;
				_freeHeadVeryLargeEntry = &_veryLargeEntryPool[index];
			}
		}
	}
}

void
MM_FreeEntrySizeClassStats::initializeFrequentAllocation(MM_LargeObjectAllocateStats *largeObjectAllocateStats)
{
	clearFrequentAllocation();

	const uintptr_t maxFrequentAllocateSizeCounters = MAX_FREE_ENTRY_COUNTERS_PER_FREQ_ALLOC_SIZE * _maxFrequentAllocateSizes;
	uintptr_t maxFrequentAllocateSizes = OMR_MIN(_maxFrequentAllocateSizes, spaceSavingGetCurSize(largeObjectAllocateStats->getSpaceSavingSizesAveragePercent()));
	const uintptr_t maxHeapSize = largeObjectAllocateStats->getMaxHeapSize();

	for(uintptr_t i = 0; i < maxFrequentAllocateSizes; i++ ) {
		uintptr_t frequentAllocSize = (uintptr_t)spaceSavingGetKthMostFreq(largeObjectAllocateStats->getSpaceSavingSizesAveragePercent(), i + 1);
		uintptr_t maxFactor = OMR_MIN(MAX_FREE_ENTRY_COUNTERS_PER_FREQ_ALLOC_SIZE, (maxHeapSize / frequentAllocSize));

		/* insert this size and a next few multiples (set as maxFactor) of this size */
		uintptr_t size = frequentAllocSize;
		for (uintptr_t factor = 1; factor <= maxFactor; factor++, size += frequentAllocSize) {
			/* follow the chain of frequent allocation sizes for this sizeClassIndex insert this size at appropriate place (the list is in increasing order) */

			uintptr_t sizeClassIndex = largeObjectAllocateStats->getSizeClassIndex(size);
			if (sizeClassIndex >= _veryLargeEntrySizeClass) {
				continue;
			}

			FrequentAllocation *prev = NULL;
			FrequentAllocation *curr = _frequentAllocationHead[sizeClassIndex];
			while ((NULL != curr) && (size > curr->_size)) {
				prev = curr;
				curr = curr->_nextInSizeClass;
			}

			/* already there? */
			if ((NULL == curr) || (size != curr->_size)) {
				/* no, get a new instance of FrequentAllocation from the pool */
				Assert_MM_true(_frequentAllocateSizeCounters < maxFrequentAllocateSizeCounters);
				_frequentAllocation[_frequentAllocateSizeCounters]._size = size;
				_frequentAllocation[_frequentAllocateSizeCounters]._count = 0;

				if (NULL == prev) {
					/* first one to add to the list */
					_frequentAllocationHead[sizeClassIndex] = &_frequentAllocation[_frequentAllocateSizeCounters];
				} else {
					/* somewhere in a middle (or last), set prev-to-this pointer */
					Assert_MM_true(_frequentAllocation[_frequentAllocateSizeCounters]._size > prev->_size);
					prev->_nextInSizeClass = &_frequentAllocation[_frequentAllocateSizeCounters];
				}

				if (NULL == curr) {
					/* last one in the list */
					_frequentAllocation[_frequentAllocateSizeCounters]._nextInSizeClass = NULL;
				} else {
					/* somewhere in a middle (or first), set this-to-next pointer */
					Assert_MM_true(_frequentAllocation[_frequentAllocateSizeCounters]._size < curr->_size);
					_frequentAllocation[_frequentAllocateSizeCounters]._nextInSizeClass = curr;
				}

				_frequentAllocateSizeCounters += 1;
			}
		}
	}
}

uintptr_t
MM_FreeEntrySizeClassStats::getFrequentAllocCount(uintptr_t sizeClassIndex)
{
	uintptr_t frequentAllocCount = 0;

	FrequentAllocation *frequentAllocation = _frequentAllocationHead[sizeClassIndex];

	while (NULL != frequentAllocation) {
		frequentAllocCount += frequentAllocation->_count;
		frequentAllocation = frequentAllocation->_nextInSizeClass;
	}

	return frequentAllocCount;
}
