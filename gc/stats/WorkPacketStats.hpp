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
 * @ingroup GC_Stats
 */

#if !defined(WORKPACKETSTATS_HPP_)
#define WORKPACKETSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrport.h"
#include "modronbase.h"
#include "modronopt.h"
#include "AtomicOperations.hpp"

/**
 * Storage for statistics relevant to a copy forward collector.
 * @ingroup GC_Stats
 */
class MM_WorkPacketStats
{
public:
	uintptr_t _gcCount;  /**< Count of the number of GC cycles that have occurred */
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uintptr_t workPacketsAcquired;
	uintptr_t workPacketsReleased;
	uintptr_t workPacketsExchanged; /**< The number of output packets converted into input packets without being returned to the shared pool first */
	uintptr_t _workStallCount; /**< The number of times the thread stalled, and subsequently received more work */
	uintptr_t _completeStallCount; /**< The number of times the thread stalled, and waited for all other threads to complete working */
	uint64_t _workStallTime; /**< The time, in hi-res ticks, the thread spent stalled waiting to receive more work */
	uint64_t _completeStallTime; /**< The time, in hi-res ticks, the thread spent stalled waiting for all other threads to complete working */
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

protected:
private:
	uintptr_t _stwWorkStackOverflowCount;
	bool _stwWorkStackOverflowOccured;
	uintptr_t _stwWorkpacketCountAtOverflow;

public:
	void clear()
	{
		_stwWorkStackOverflowCount = 0;
		_stwWorkStackOverflowOccured = false;
		_stwWorkpacketCountAtOverflow = 0;
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		_workStallCount = 0;
		_completeStallCount = 0;
		_workStallTime = 0;
		_completeStallTime = 0;
		workPacketsAcquired = 0;
		workPacketsReleased = 0;
		workPacketsExchanged = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}

	void merge(MM_WorkPacketStats *statsToMerge)
	{
		_stwWorkStackOverflowCount += statsToMerge->_stwWorkStackOverflowCount;
		_stwWorkStackOverflowOccured = (_stwWorkStackOverflowOccured || statsToMerge->_stwWorkStackOverflowOccured);
		_stwWorkpacketCountAtOverflow = OMR_MAX(_stwWorkpacketCountAtOverflow, statsToMerge->_stwWorkpacketCountAtOverflow);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		/* It may not ever be useful to merge these stats, but do it anyways */
		_workStallCount += statsToMerge->_workStallCount;
		_completeStallCount += statsToMerge->_completeStallCount;
		_workStallTime += statsToMerge->_workStallTime;
		_completeStallTime += statsToMerge->_completeStallTime;
		workPacketsAcquired += statsToMerge->workPacketsAcquired;
		workPacketsReleased += statsToMerge->workPacketsReleased;
		workPacketsExchanged += statsToMerge->workPacketsExchanged;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	/**
	 * Add time interval to work stall time.
	 * Time is stored in raw format, converted to resolution at time of output
	 * Don't need to worry about wrap (endTime < startTime as unsigned math
	 * takes care of wrap
	 */
	MMINLINE void 
	addToWorkStallTime(uint64_t startTime, uint64_t endTime)
	{
		_workStallCount += 1;
		_workStallTime += (endTime - startTime);
	}
	
	/**
	 * Add time interval to complete stall time.
	 * Time is stored in raw format, converted to resolution at time of output
	 * Don't need to worry about wrap (endTime < startTime as unsigned math
	 * takes care of wrap
	 */
	MMINLINE void 
	addToCompleteStallTime(uint64_t startTime, uint64_t endTime)
	{
		_completeStallCount += 1;
		_completeStallTime += (endTime - startTime);
	}
	
	/**
	 * Get the total stall time
	 * @return the time in hi-res ticks
	 */
	MMINLINE uint64_t 
	getStallTime()
	{
		return _workStallTime + _completeStallTime;
	}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	MMINLINE bool getSTWWorkStackOverflowOccured()	{ return _stwWorkStackOverflowOccured; };
	MMINLINE void setSTWWorkStackOverflowOccured(bool overflow)	{ _stwWorkStackOverflowOccured = overflow; };
	MMINLINE uintptr_t getSTWWorkStackOverflowCount()	{ return _stwWorkStackOverflowCount; };
	MMINLINE uintptr_t getSTWWorkpacketCountAtOverflow() { return _stwWorkpacketCountAtOverflow; };
	MMINLINE void setSTWWorkpacketCountAtOverflow(uintptr_t workpacketCount) { _stwWorkpacketCountAtOverflow = workpacketCount; };
	MMINLINE void incrementSTWWorkStackOverflowCount()
	{
		MM_AtomicOperations::add(&_stwWorkStackOverflowCount, 1);
	}

	MM_WorkPacketStats() :
		_gcCount(UDATA_MAX)
		,workPacketsAcquired(0)
		,workPacketsReleased(0)
		,workPacketsExchanged(0)
		,_workStallCount(0)
		,_completeStallCount(0)
		,_workStallTime(0)
		,_completeStallTime(0)
		,_stwWorkStackOverflowCount(0)
		,_stwWorkStackOverflowOccured(false)
		,_stwWorkpacketCountAtOverflow(0)
	{}

protected:
private:
};

#endif /* WORKPACKETSTATS_HPP_ */
