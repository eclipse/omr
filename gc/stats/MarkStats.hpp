/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#if !defined(MARKSTATS_HPP_)
#define MARKSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)

#include "Base.hpp"

/**
 * Storage for statistics relevant to the mark phase of a global collection.
 * @ingroup GC_Stats
 */
class MM_MarkStats : public MM_Base
{
/* data members */
private:
	uint64_t _scanTime; /**< The amount of time spent scanning by the owning thread (or globally) during marking, in hi-res timer resolution */

protected:
public:
	uintptr_t _gcCount;  /**< Count of the number of GC cycles that have occurred */
	uintptr_t _objectsMarked;  /**< The number of objects found through scanning during marking */
	uintptr_t _objectsScanned;  /**< The number of objects popped and scanned during marking (e.g., non-base type arrays) */
	uintptr_t _bytesScanned; /**< The number of bytes scanned by the owning thread (or globally) during marking */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uintptr_t _syncStallCount; /**< The number of times the thread stalled at a sync point */
	uint64_t _syncStallTime; /**< The time, in hi-res ticks, the thread spent stalled at a sync point */
	uintptr_t _splitArraysProcessed; /**< The number of array chunks (not counting parts smaller than the split size) processed by this thread */
	uintptr_t _splitArraysAmount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	uint64_t _startTime;	/**< Mark start time */
	uint64_t _endTime;		/**< Mark end time */

/* function members */
private:
protected:
public:
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	/** Time is stored in raw format, converted to resolution at time of output 
	 * Don't need to worry about wrap (endTime < startTime as unsigned math
	 * takes care of wrap
	 */
	MMINLINE void 
	addToSyncStallTime(uint64_t startTime, uint64_t endTime)
	{
		_syncStallCount += 1;
		_syncStallTime += (endTime - startTime);
	}
	
	/**
	 * Get the total stall time
	 * @return the time in hi-res ticks
	 */
	MMINLINE uint64_t 
	getStallTime()
	{
		return _syncStallTime;
	}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	
	void clear();
	void merge(MM_MarkStats *statsToMerge);
	
	/**
	 * Add the specified interval to the amount of time attributed to scanning.
	 * @param startTime The time scanning began, measured by omrtime_hires_clock()
	 * @param endTime The time scanning ended, measured by omrtime_hires_clock()
	 */
	MMINLINE void addToScanTime(uint64_t startTime, uint64_t endTime) {	_scanTime += (endTime - startTime);	}

	/**
	 * Get the amount of time the receiver's thread spent scanning, in hi-res timer resolution.
	 * For the global stats structure, this is the sum of time spent by all threads.
	 * @return the time spent scanning
	 */
	MMINLINE uint64_t getScanTime() { return _scanTime; }

	MM_MarkStats() :
		MM_Base()
		,_scanTime(0)
		,_gcCount(UDATA_MAX)
		,_objectsMarked(0)
		,_objectsScanned(0)
		,_bytesScanned(0)
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		,_syncStallCount(0)
		,_syncStallTime(0)
		,_splitArraysProcessed(0)
		,_splitArraysAmount(0)
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
		,_startTime(0)
		,_endTime(0)
	{
		clear();
	}
	
}; 

#endif /* OMR_GC_MODRON_STANDARD || OMR_GC_REALTIME */
#endif /* MARKSTATS_HPP_ */
