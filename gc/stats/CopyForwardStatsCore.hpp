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

#if !defined(COPYFORWARDSTATSCORE_HPP_)
#define COPYFORWARDSTATSCORE_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "modronopt.h"

#if defined(OMR_GC_VLHGC)

/**
 * Storage for statistics relevant to a copy forward collector.
 * @ingroup GC_Stats
 */
class MM_CopyForwardStatsCore
{
	/* 
	 * Data members 
	 */
public:
	uintptr_t _gcCount; /**< The GC cycle in which these stats were collected */

	uint64_t _startTime;  /**< Start timestamp for last copy forward */
	uint64_t _endTime;  /**< End timestamp for last copy forward */

	uintptr_t _freeMemoryBefore;  /**< Free memory on the heap recorded before a copy forward cycle */
	uintptr_t _totalMemoryBefore;  /**< Total memory on the heap recorded before a copy forward cycle */
	uintptr_t _freeMemoryAfter;  /**< Free memory on the heap recorded after a copy forward cycle */
	uintptr_t _totalMemoryAfter;  /**< Total memory on the heap recorded after a copy forward cycle */

	uintptr_t _copyObjectsTotal;  /**< Total count of objects copied into survivor */
	uintptr_t _copyBytesTotal;  /**< Total bytes copied into survivor */
	uintptr_t _copyDiscardBytesTotal;  /**< total bytes discarded due to copying into survivor */
	uintptr_t _scanObjectsTotal; /**< Total count of objects scanned in abort recovery */
	uintptr_t _scanBytesTotal;   /**< Total bytes scanned in abort recovery */
	
	uintptr_t _copyObjectsEden;  /**< Count of objects copied into survivor in Eden (age 0) space */
	uintptr_t _copyBytesEden;  /**< Bytes of survivor used for copy purposes in Eden (age 0) space */
	uintptr_t _copyDiscardBytesEden;  /**< Bytes of survivor discarded by copy scan cache in Eden (age 0) space */
	uintptr_t _scanObjectsEden;	/**< Objects scanned in abort recovery in Eden (age 0) space*/
	uintptr_t _scanBytesEden;	/**< Bytes scanned in abort recovery in Eden (age 0) space*/

	uintptr_t _copyObjectsNonEden;  /**< Count of objects copied into survivor in non-Eden (age > 0) space */
	uintptr_t _copyBytesNonEden;  /**< Bytes of survivor used for copy purposes in non-Eden (age > 0) space */
	uintptr_t _copyDiscardBytesNonEden;  /**< Bytes of survivor discarded by copy scan cache in non-Eden (age > 0) space */
	uintptr_t _scanObjectsNonEden;	/**< Objects scanned in abort recovery in non-Eden (age > 0) space*/
	uintptr_t _scanBytesNonEden;	/**< Bytes scanned in abort recovery in non-Eden (age > 0) space*/

	uintptr_t _objectsCardClean;	/**< Objects scanned through card cleaning (either for copy, or re-scanned for abort) */
	uintptr_t _bytesCardClean;		/**< Bytes scanned through card cleaning (either for copy, or re-scanned for abort) */

	bool _scanCacheOverflow;  /**< Flag indicating if a scan cache overflow occurred in last copy forward */

	bool _aborted;  /**< Flag indicating if the copy forward mechanism was aborted */

	uintptr_t _edenEvacuateRegionCount;	/**< Counts the number of Eden regions selected to be evacuated in this copy-forward (only counted globally - unused in per-thread stats) */
	uintptr_t _nonEdenEvacuateRegionCount;	/**< Counts the number of non-Eden selected to be evacuated in this copy-forward (only counted globally - unused in per-thread stats) */
	uintptr_t _edenSurvivorRegionCount;	/**< Counts the number of Eden regions allocated for survivor space in this copy-forward (only counted globally - unused in per-thread stats) */
	uintptr_t _nonEdenSurvivorRegionCount;	/**< Counts the number of non-Eden regions allocated (that is, not tail-fill regions) for survivor space in this copy-forward (only counted globally - unused in per-thread stats) */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uintptr_t _releaseScanListCount;
	uintptr_t _acquireFreeListCount;
	uintptr_t _releaseFreeListCount;
	uintptr_t _acquireScanListCount;
	uintptr_t _syncStallCount;		/**< The number of times the thread stalled to sync for normal work */
	uintptr_t _abortStallCount;		/**< The number of times the thread stalled to sync for normal work */
	uintptr_t _markStallCount;		/**< The number of times the thread stalled to sync for normal work */
	uintptr_t _irrsStallCount;		/**< The number of times the thread stalled to sync for InterRegionRememberedSet work */
	uintptr_t _workStallCount; 		/**< The number of times the thread stalled, and subsequently received more work */
	uintptr_t _completeStallCount; 	/**< The number of times the thread stalled, and waited for all other threads to complete working */
	uint64_t _syncStallTime;		/**< stall time (time thread being blocked), during most of copy phase */
	uint64_t _abortStallTime;		/**< stall time specifically for copy-forward to mark transition */
	uint64_t _markStallTime;		/**< stall time specifically for mark work following an abort */
	uint64_t _irrsStallTime;		/**< stall time specifically for InterRegionRememberedSet work */
	uint64_t _workStallTime; 		/**< The time, in hi-res ticks, the thread spent stalled waiting to receive more work */
	uint64_t _completeStallTime; 	/**< The time, in hi-res ticks, the thread spent stalled waiting for all other threads to complete working */
	uintptr_t _copiedArraysSplit; 	/**< The number of array chunks (not counting parts smaller than the split size) processed by this thread in copy-forward mode*/
	uintptr_t _markedArraysSplit; 	/**< The number of array chunks (not counting parts smaller than the split size) processed by this thread in mark mode*/
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	uintptr_t _heapExpandedBytes; /**< Bytes by which the heap expanded in order to complete the collection */
	uintptr_t _heapExpandedCount; /**< The number of times the heap was expanded in order to complete the collection */
	uint64_t _heapExpandedTime; /**< Time taken expanding the heap in order to complete the collection, in hi-res ticks */
	
	uintptr_t _externalCompactBytes; /**< The number of bytes selected for compaction outside of the copy-forward scheme. These would have been included in the copy-forward if there were sufficient space */

	uintptr_t _objectsScannedFromWorkPackets; /**< In depth-first copy-forward, the number of objects scanned by this thread which it found on a work packet */
	uintptr_t _objectsScannedFromOverflowedRegion; /**< In depth-first copy-forward, the number of objects scanned by this thread which it found in an overflowed region (while handling work packet overflow) */
	uintptr_t _objectsScannedFromNextInChain; /**< In depth-first copy-forward, the number of objects scanned by this thread which it found as the next child to scan from an object it was already scanning (going "down" the tree) */
	uintptr_t _objectsScannedFromDepthStack; /**< In depth-first copy-forward, the number of objects scanned by this thread which it found on its depth stack (that is, one it resumed as it was going "up" the tree) */
	uintptr_t _objectsScannedFromRoot; /**< In depth-first copy-forward, the number of objects scanned by this thread which it found in the non-card root set */

protected:
private:
	
	/* 
	 * Function members 
	 */
public:
	
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	MMINLINE void addToSyncStallTime(uint64_t startTime, uint64_t endTime)
	{
		_syncStallCount += 1;
		_syncStallTime += (endTime - startTime);
	}

	MMINLINE void addToAbortStallTime(uint64_t startTime, uint64_t endTime)
	{
		_abortStallCount += 1;
		_abortStallTime += (endTime - startTime);
	}
	
	MMINLINE void addToMarkStallTime(uint64_t startTime, uint64_t endTime)
	{
		_markStallCount += 1;
		_markStallTime += (endTime - startTime);
	}

	MMINLINE void addToInterRegionRememberedSetStallTime(uint64_t startTime, uint64_t endTime)
	{
		_irrsStallCount += 1;
		_irrsStallTime += (endTime - startTime);
	}
	
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
	 * @eturn the total stall time for the operation in hi-res ticks
	 */
	MMINLINE uint64_t 
	getStallTime() 
	{
		return _syncStallTime + _abortStallTime + _markStallTime + _irrsStallTime + _workStallTime + _completeStallTime;
	}
	
	/**
	 * @eturn the stall time for the copy-forward part of the operation in hi-res ticks
	 */
	MMINLINE uint64_t 
	getCopyForwardStallTime() 
	{
		return _syncStallTime + _workStallTime + _completeStallTime;
	}
	
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	MMINLINE void clear() {

		/* NOTE: _startTime and _endTime are also not cleared
		 * as they are recorded before/after all stat clearing/gathering.
		 */

		_freeMemoryBefore = 0;
		_totalMemoryBefore = 0;
		_freeMemoryAfter = 0;
		_totalMemoryAfter = 0;

		_copyObjectsTotal = 0;
		_copyBytesTotal = 0;
		_copyDiscardBytesTotal = 0;
		_scanObjectsTotal = 0;
		_scanBytesTotal = 0;
		
		_copyObjectsEden = 0;
		_copyBytesEden = 0;
		_copyDiscardBytesEden = 0;
		_scanObjectsEden = 0;
		_scanBytesEden = 0;

		_copyObjectsNonEden = 0;
		_copyBytesNonEden = 0;
		_copyDiscardBytesNonEden = 0;
		_scanObjectsNonEden = 0;
		_scanBytesNonEden = 0;

		_objectsCardClean = 0;
		_bytesCardClean = 0;

		_scanCacheOverflow = false;
		_aborted = false;

		_edenEvacuateRegionCount = 0;
		_nonEdenEvacuateRegionCount = 0;
		_edenSurvivorRegionCount = 0;
		_nonEdenSurvivorRegionCount = 0;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		_releaseScanListCount = 0;
		_acquireFreeListCount = 0;
		_releaseFreeListCount = 0;
		_acquireScanListCount = 0;
		_syncStallCount = 0;
		_abortStallCount = 0;
		_markStallCount = 0;
		_irrsStallCount = 0;
		_workStallCount = 0;
		_completeStallCount = 0;
		_syncStallTime = 0;
		_abortStallTime = 0;
		_markStallTime = 0;
		_irrsStallTime = 0;
		_workStallTime = 0;
		_completeStallTime = 0;
		_copiedArraysSplit = 0;
		_markedArraysSplit = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
		
		_heapExpandedBytes = 0;
		_heapExpandedCount = 0;
		_heapExpandedTime = 0;
		
		_externalCompactBytes = 0;

		_objectsScannedFromWorkPackets = 0;
		_objectsScannedFromOverflowedRegion = 0;
		_objectsScannedFromNextInChain = 0;
		_objectsScannedFromDepthStack = 0;
		_objectsScannedFromRoot = 0;
	}
	
	/**
	 * Merge the given stat structures values into the receiver.
	 * @note This method is NOT thread safe.
	 */
	void merge(MM_CopyForwardStatsCore *stats) {
		_aborted |= stats->_aborted;
		_scanCacheOverflow |= stats->_scanCacheOverflow;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		_acquireFreeListCount += stats->_acquireFreeListCount;
		_releaseFreeListCount += stats->_releaseFreeListCount;
		_acquireScanListCount += stats->_acquireScanListCount;
		_releaseScanListCount += stats->_releaseScanListCount;
		_syncStallCount += stats->_syncStallCount;
		_abortStallCount += stats->_abortStallCount;
		_markStallCount += stats->_markStallCount;
		_irrsStallCount += stats->_irrsStallCount;
		_workStallCount += stats->_workStallCount;
		_completeStallCount += stats->_completeStallCount;
		_syncStallTime += stats->_syncStallTime;
		_abortStallTime += stats->_abortStallTime;
		_markStallTime += stats->_markStallTime;
		_irrsStallTime += stats->_irrsStallTime;
		_workStallTime += stats->_workStallTime;
		_completeStallTime += stats->_completeStallTime;
		_copiedArraysSplit += stats->_copiedArraysSplit;
		_markedArraysSplit += stats->_markedArraysSplit;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

		_copyObjectsTotal += stats->_copyObjectsTotal;
		_copyBytesTotal += stats->_copyBytesTotal;
		_copyDiscardBytesTotal += stats->_copyDiscardBytesTotal;
		_scanObjectsTotal += stats->_scanObjectsTotal;
		_scanBytesTotal += stats->_scanBytesTotal;

		_copyObjectsEden += stats->_copyObjectsEden;
		_copyBytesEden += stats->_copyBytesEden;
		_copyDiscardBytesEden += stats->_copyDiscardBytesEden;
		_scanObjectsEden += stats->_scanObjectsEden;
		_scanBytesEden += stats->_scanBytesEden;

		_copyObjectsNonEden += stats->_copyObjectsNonEden;
		_copyBytesNonEden += stats->_copyBytesNonEden;
		_copyDiscardBytesNonEden += stats->_copyDiscardBytesNonEden;
		_scanObjectsNonEden += stats->_scanObjectsNonEden;
		_scanBytesNonEden += stats->_scanBytesNonEden;

		_objectsCardClean += stats->_objectsCardClean;
		_bytesCardClean += stats->_bytesCardClean;

		_heapExpandedBytes += stats->_heapExpandedBytes;
		_heapExpandedCount += stats->_heapExpandedCount;
		_heapExpandedTime += stats->_heapExpandedTime;
		
		_externalCompactBytes += stats->_externalCompactBytes;

		_objectsScannedFromWorkPackets += stats->_objectsScannedFromWorkPackets;
		_objectsScannedFromOverflowedRegion += stats->_objectsScannedFromOverflowedRegion;
		_objectsScannedFromNextInChain += stats->_objectsScannedFromNextInChain;
		_objectsScannedFromDepthStack += stats->_objectsScannedFromDepthStack;
		_objectsScannedFromRoot += stats->_objectsScannedFromRoot;
	}

	MM_CopyForwardStatsCore() :
		_gcCount(UDATA_MAX)
		,_startTime(0)
		,_endTime(0)
		,_freeMemoryBefore(0)
		,_totalMemoryBefore(0)
		,_freeMemoryAfter(0)
		,_totalMemoryAfter(0)
		,_copyObjectsTotal(0)
		,_copyBytesTotal(0)
		,_copyDiscardBytesTotal(0)
		,_scanObjectsTotal(0)
		,_scanBytesTotal(0)
		,_copyObjectsEden(0)
		,_copyBytesEden(0)
		,_copyDiscardBytesEden(0)
		,_scanObjectsEden(0)
		,_scanBytesEden(0)
		,_copyObjectsNonEden(0)
		,_copyBytesNonEden(0)
		,_copyDiscardBytesNonEden(0)
		,_scanObjectsNonEden(0)
		,_scanBytesNonEden(0)
		,_objectsCardClean(0)
		,_bytesCardClean(0)
		,_scanCacheOverflow(false)
		,_aborted(false)
		,_edenEvacuateRegionCount(0)
		,_nonEdenEvacuateRegionCount(0)
		,_edenSurvivorRegionCount(0)
		,_nonEdenSurvivorRegionCount(0)
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		,_releaseScanListCount(0)
		,_acquireFreeListCount(0)
		,_releaseFreeListCount(0)
		,_acquireScanListCount(0)
		,_syncStallCount(0)
		,_abortStallCount(0)
		,_markStallCount(0)
		,_irrsStallCount(0)
		,_workStallCount(0)
		,_completeStallCount(0)
		,_syncStallTime(0)
		,_abortStallTime(0)
		,_markStallTime(0)
		,_irrsStallTime(0)
		,_workStallTime(0)
		,_completeStallTime(0)
		,_copiedArraysSplit(0)
		,_markedArraysSplit(0)
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
		,_heapExpandedBytes(0)
		,_heapExpandedCount(0)
		,_heapExpandedTime(0)
		,_externalCompactBytes(0)
		,_objectsScannedFromWorkPackets(0)
		,_objectsScannedFromOverflowedRegion(0)
		,_objectsScannedFromNextInChain(0)
		,_objectsScannedFromDepthStack(0)
		,_objectsScannedFromRoot(0)
	{}
};

#endif /* OMR_GC_VLHGC */
#endif /* COPYFORWARDSTATSCORE_HPP_ */
