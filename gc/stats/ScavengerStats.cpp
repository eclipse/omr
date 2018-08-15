/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include <string.h>

#include "ModronAssertions.h"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"

#include "ScavengerStats.hpp"

MM_ScavengerStats::MM_ScavengerStats()
	:
	_gcCount(UDATA_MAX)
	,_rememberedSetOverflow(0)
	,_causedRememberedSetOverflow(0)
	,_scanCacheOverflow(0)
	,_scanCacheAllocationFromHeap(0)
	,_scanCacheAllocationDurationDuringSavenger(0)
	,_backout(0)
	,_failedTenureCount(0)
	,_failedTenureBytes(0)
	,_failedTenureLargest(0)
	,_failedFlipCount(0)
	,_failedFlipBytes(0)
	,_tenureAge(0)
	,_startTime(0)
	,_endTime(0)
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	,_releaseScanListCount(0)
	,_acquireFreeListCount(0)
	,_releaseFreeListCount(0)
	,_acquireScanListCount(0)
	,_acquireListLockCount(0)
	,_aliasToCopyCacheCount(0)
	,_arraySplitCount(0)
	,_arraySplitAmount(0)
	,_workStallCount(0)
	,_completeStallCount(0)
	,_syncStallCount(0)
	,_workStallTime(0)
	,_completeStallTime(0)
	,_syncStallTime(0)
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	,_avgInitialFree(0)
	,_avgTenureBytes(0)
	,_tiltRatio(0)
	,_nextScavengeWillPercolate(false)
#if defined(OMR_GC_LARGE_OBJECT_AREA)	
	,_avgTenureLOABytes(0)
	,_avgTenureSOABytes(0)
#endif /* OMR_GC_LARGE_OBJECT_AREA */
	,_flipDiscardBytes(0)
	,_tenureDiscardBytes(0)
	,_survivorTLHRemainderCount(0)
	,_tenureTLHRemainderCount(0)
	,_semiSpaceAllocBytesAcumulation(0)
	,_tenureSpaceAllocBytesAcumulation(0)
	,_semiSpaceAllocationCountLarge(0)
	,_semiSpaceAllocationCountSmall(0)
	,_tenureSpaceAllocationCountLarge(0)
	,_tenureSpaceAllocationCountSmall(0)
	,_tenureExpandedBytes(0)
	,_tenureExpandedCount(0)
	,_tenureExpandedTime(0)
	,_leafObjectCount(0)
	,_copy_cachesize_sum(0)
	,_slotsCopied(0)
	,_slotsScanned(0)

	,_flipHistoryNewIndex(0)
{
	memset(_flipHistory, 0, sizeof(_flipHistory));
	memset(_copy_distance_counts, 0, sizeof(_copy_distance_counts));
	memset(_copy_cachesize_counts, 0, sizeof(_copy_cachesize_counts));
	memset(_small_object_counts, 0, sizeof(_small_object_counts));
	memset(_large_object_counts, 0, sizeof(_large_object_counts));
}

struct MM_ScavengerStats::FlipHistory*
MM_ScavengerStats::getFlipHistory(uintptr_t lookback)
{
	MM_ScavengerStats::FlipHistory* flipHistory = NULL;
	if (SCAVENGER_FLIP_HISTORY_SIZE <= lookback) {
		flipHistory = NULL;
	} else {
		uintptr_t historyIndex = (SCAVENGER_FLIP_HISTORY_SIZE + _flipHistoryNewIndex - lookback) % SCAVENGER_FLIP_HISTORY_SIZE;
		flipHistory = &_flipHistory[historyIndex];
	}
	return flipHistory;
}

void 
MM_ScavengerStats::clear(bool firstIncrement)
{
	if (firstIncrement) {
		/* clear() can be called several times per a cycle (in Concurrent Scavenger), but some stats/params must be reset/updated only once per a cycle */

		/* Increment the histogram offset and loop if necessary */
		/* TODO: this does not properly work for Master GC threads, which is implicit (a random mutator thread), for standard (non CS) Scavenger.
		 * Flip history stats (or complete ScavengerStats) should be one place (in GcExtensions or Scavenger), not scattered among mutator threads.
		 */
		_flipHistoryNewIndex = (_flipHistoryNewIndex + 1) % SCAVENGER_FLIP_HISTORY_SIZE;
	}

	/* Clear the new histogram row */
	memset(&_flipHistory[_flipHistoryNewIndex], 0, sizeof(_flipHistory[_flipHistoryNewIndex]));

	/* _gcCount is not cleared as the value must persist across cycles */
	
	_rememberedSetOverflow = 0;
	_causedRememberedSetOverflow = 0;
	_scanCacheOverflow = 0;
	_scanCacheAllocationFromHeap = 0;
	_scanCacheAllocationDurationDuringSavenger = 0;
	_backout = 0;
	_flipCount = 0;
	_flipBytes = 0;
	_tenureAggregateCount = 0;
	_tenureAggregateBytes = 0;
#if defined(OMR_GC_LARGE_OBJECT_AREA)	
	_tenureLOACount = 0;
	_tenureLOABytes = 0;
#endif /* OMR_GC_LARGE_OBJECT_AREA */			
	_failedTenureCount = 0;
	_failedTenureBytes = 0;
	_failedTenureLargest = 0;
	_failedFlipCount = 0;
	_failedFlipBytes = 0;
	_tenureAge = 0;
	_nextScavengeWillPercolate = false;
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	_releaseScanListCount = 0;
	_acquireFreeListCount = 0;
	_releaseFreeListCount = 0;
	_acquireScanListCount = 0;
	_acquireListLockCount = 0;
	_aliasToCopyCacheCount = 0;
	_workStallCount = 0;
	_completeStallCount = 0;
	_syncStallCount = 0;
	_workStallTime = 0;
	_completeStallTime = 0;
	_syncStallTime = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	/* NOTE: _startTime and _endTime are also not cleared
	 * as they are recorded before/after all stat clearing/gathering.
	 */
	_flipDiscardBytes = 0;
	_tenureDiscardBytes = 0;

	_survivorTLHRemainderCount = 0;
	_tenureTLHRemainderCount = 0;

	_semiSpaceAllocationCountLarge = 0;
	_semiSpaceAllocationCountSmall = 0;
	_tenureSpaceAllocationCountLarge = 0;
	_tenureSpaceAllocationCountSmall = 0;

	_tenureExpandedBytes = 0;
	_tenureExpandedCount = 0;
	_tenureExpandedTime = 0;

	_slotsCopied = 0;
	_slotsScanned = 0;
	_leafObjectCount = 0;
	_copy_cachesize_sum = 0;
	memset(_copy_distance_counts, 0, sizeof(_copy_distance_counts));
	memset(_copy_cachesize_counts, 0, sizeof(_copy_cachesize_counts));
	memset(_small_object_counts, 0, sizeof(_small_object_counts));
	memset(_large_object_counts, 0, sizeof(_large_object_counts));
};
