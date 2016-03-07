/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

/**
 * @file
 * @ingroup GC_Stats_Core
 */

#if !defined(MARKVLHGCSTATSCORE_HPP_)
#define MARKVLHGCSTATSCORE_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrport.h"
#include "modronbase.h"
#include "modronopt.h"

#if defined(OMR_GC_VLHGC)

#include "Base.hpp"
#include "AtomicOperations.hpp" 
/**
 * Storage for statistics relevant to the mark phase of a global collection.
 * @ingroup GC_Stats
 */
class MM_MarkVLHGCStatsCore : public MM_Base
{
/* data members */
private:
	uint64_t _scanTime; /**< The amount of time spent scanning by the owning thread (or globally) during marking, in hi-res timer resolution */

protected:
public:
	uintptr_t _gcCount; /**< The GC cycle in which these stats were collected */

	uint64_t _startTime;  /**< Start timestamp for last mark operation */
	uint64_t _endTime;  /**< End timestamp for last mark operation */

	uintptr_t _objectsMarked;  /**< The number of objects found through scanning during marking */
	uintptr_t _objectsScanned;  /**< The number of objects popped and scanned during marking (e.g., non-base type arrays) */
	uintptr_t _bytesScanned; /**< The number of bytes scanned by the owning thread (or globally) during marking */

	uintptr_t _objectsCardClean;	/**< Objects scanned through card cleaning */
	uintptr_t _bytesCardClean;		/**< Bytes scanned through card cleaning */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uintptr_t _splitArraysProcessed; /**< The number of array chunks (not counting parts smaller than the split size) processed by this thread */
	uintptr_t _syncStallCount; /**< The number of times the thread stalled at a sync point */
	uint64_t _syncStallTime; /**< The time, in hi-res ticks, the thread spent stalled at a sync point */
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

/* function members */
private:
protected:
public:

	void clear()
	{
		_scanTime = 0;

		_objectsMarked = 0;
		_objectsScanned = 0;
		_bytesScanned = 0;

		_objectsCardClean = 0;
		_bytesCardClean = 0;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		_syncStallCount = 0;
		_syncStallTime = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}

	void merge(MM_MarkVLHGCStatsCore *statsToMerge)
	{
		_scanTime += statsToMerge->_scanTime;

		_objectsMarked += statsToMerge->_objectsMarked;
		_objectsScanned += statsToMerge->_objectsScanned;
		_bytesScanned += statsToMerge->_bytesScanned;

		_objectsCardClean += statsToMerge->_objectsCardClean;
		_bytesCardClean += statsToMerge->_bytesCardClean;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		/* It may not ever be useful to merge these stats, but do it anyways */
		_syncStallCount += statsToMerge->_syncStallCount;
		_syncStallTime += statsToMerge->_syncStallTime;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	/* Time is stored in raw format, converted to resolution at time of output
	 * Don't need to worry about wrap (endTime < startTime as unsigned math
	 * takes care of wrap
	 */
	void addToSyncStallTime(uint64_t startTime, uint64_t endTime)
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
	
	MM_MarkVLHGCStatsCore() :
		MM_Base()
		,_scanTime(0)
		,_gcCount(0)
		,_startTime(0)
		,_endTime(0)
		,_objectsMarked(0)
		,_objectsScanned(0)
		,_bytesScanned(0)
		,_objectsCardClean(0)
		,_bytesCardClean(0)
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		,_splitArraysProcessed(0)
		,_syncStallCount(0)
		,_syncStallTime(0)
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	{
	}
	
}; 

#endif /* OMR_GC_VLHGC */
#endif /* MARKVLHGCSTATSCORE_HPP_ */
