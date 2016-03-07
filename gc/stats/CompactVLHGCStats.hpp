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

#if !defined(COMPACTVLHGCSTATS_HPP_)
#define COMPACTVLHGCSTATS_HPP_

#include "omrcfg.h"
#include "j9modron.h"
#include "modronbase.h"

#if defined(OMR_GC_VLHGC)

#if defined(OMR_GC_MODRON_COMPACTION)
#include "Base.hpp"

/**
 * Storage for stats relevant to the compaction phase of a collection.
 * @ingroup GC_Stats
 */
class MM_CompactVLHGCStats : public MM_Base
{
public:
	CompactReason _compactReason;
	CompactPreventedReason _compactPreventedReason;

	uint64_t _startTime;  /**< Start timestamp (hires ticks) for last sweep operation */
	uint64_t _endTime;  /**< End timestamp (hires ticks) for last sweep operation */

	uintptr_t _movedObjects;
	uintptr_t _movedBytes;
	uintptr_t _fixupObjects;
	uint64_t _setupStartTime;
	uint64_t _setupEndTime;
	uint64_t _moveStartTime;
	uint64_t _moveEndTime;
	uint64_t _fixupStartTime;
	uint64_t _fixupEndTime;
	uint64_t _rootFixupStartTime;
	uint64_t _rootFixupEndTime;
	uint64_t _moveStallTime;	/**< time spent waiting for move work */
	uint64_t _rebuildStallTime; /**< time spent waiting for mark map rebuild work */
		
	uint64_t _flushStartTime;	/**< hires start time to flush remembered set into card table */
	uint64_t _flushEndTime;	/**< hires end time to flush remembered set into card table */
	uint64_t _leafTaggingStartTime;	/**< hires start time to tag arraylet leaf regions for fixup (one thread only) */
	uint64_t _leafTaggingEndTime;	/**< hires end time to tag arraylet leaf regions for fixup (one thread only) */
	uint64_t _regionCompactDataInitStartTime;	/**< hires start time to initialize compact data in the region table (one thread only) */
	uint64_t _regionCompactDataInitEndTime;	/**< hires end time to initialize compact data in the region table (one thread only) */
	uint64_t _clearMarkMapStartTime;	/**< hires start time to clear the next mark map to store data for the compact set */
	uint64_t _clearMarkMapEndTime;	/**< hires end time to clear the next mark map to store data for the compact set */
	uint64_t _rememberedSetClearingStartTime;	/**< hires start time to clear all RSCL from-references for the regions in the compact set */
	uint64_t _rememberedSetClearingEndTime;	/**< hires end time to clear all RSCL from-references for the regions in the compact set */
	uint64_t _planningStartTime;	/**< hires start time of the phase which plans the compaction operation */
	uint64_t _planningEndTime;	/**< hires end time of the phase which plans the compaction operation */
	uint64_t _reportMoveEventsStartTime;	/**< hires start time to report all object move events (master thread only and only if OBJECT_RENAME is hooked) */
	uint64_t _reportMoveEventsEndTime;	/**< hires end time to report all object move events (master thread only and only if OBJECT_RENAME is hooked) */
	uint64_t _fixupExternalPacketsStartTime;	/**< hires start time to fixup work packets for an active GMP (only done if GMP is active) */
	uint64_t _fixupExternalPacketsEndTime;	/**< hires end time to fixup work packets for an active GMP (only done if GMP is active) */
	uint64_t _fixupArrayletLeafStartTime;	/**< hires start time to fixup arraylet leaf spine pointers (one thread only) */
	uint64_t _fixupArrayletLeafEndTime;	/**< hires end time to fixup arraylet leaf spine pointers (one thread only) */
	uint64_t _recycleStartTime;	/**< hires start time to identify and recycle fully evacuated regions (one thread only) */
	uint64_t _recycleEndTime;	/**< hires end time to identify and recycle fully evacuated regions (one thread only) */
	uint64_t _rebuildMarkBitsStartTime;	/**< hires start time to rebuild the mark bits in the previous mark map */
	uint64_t _rebuildMarkBitsEndTime;	/**< hires end time to rebuild the mark bits in the previous mark map */
	uint64_t _finalClearNextMarkMapStartTime;	/**< hires start time to clear the next mark map after compact is finished */
	uint64_t _finalClearNextMarkMapEndTime;	/**< hires end time to clear the next mark map after compact is finished */
	uint64_t _rebuildNextMarkMapStartTime;	/**< hires start time to rebuild the next mark map from the next work packets (only done in GMP is active) */
	uint64_t _rebuildNextMarkMapEndTime;	/**< hires end time to rebuild the next mark map from the next work packets (only done in GMP is active) */

	void clear()
	{
		_compactReason = COMPACT_NONE;
		_compactPreventedReason = COMPACT_PREVENTED_NONE;

		_movedObjects = 0;
		_movedBytes = 0;

		_fixupObjects = 0;
		_setupStartTime = 0;
		_setupEndTime = 0;
		_moveStartTime = 0;
		_moveEndTime = 0;
		_fixupStartTime = 0;
		_fixupEndTime = 0;
		_rootFixupStartTime = 0;
		_rootFixupEndTime = 0;
		_moveStallTime = 0;
		_rebuildStallTime = 0;
	};

	MMINLINE void addToMoveStallTime(uint64_t startTime, uint64_t endTime)
	{
		_moveStallTime += (endTime - startTime);
	}

	MMINLINE void addToRebuildStallTime(uint64_t startTime, uint64_t endTime)
	{
		_rebuildStallTime += (endTime - startTime);
	}

	void merge(MM_CompactVLHGCStats *statsToMerge)
	{
		_movedObjects += statsToMerge->_movedObjects;
		_movedBytes += statsToMerge->_movedBytes;
		_fixupObjects += statsToMerge->_fixupObjects;
		/* merging time intervals is a little different than just creating a total since the sum of two time intervals, for our uses, is their union (as opposed to the sum of two time spans, which is their sum) */
		_setupStartTime = (0 == _setupStartTime) ? statsToMerge->_setupStartTime : OMR_MIN(_setupStartTime, statsToMerge->_setupStartTime);
		_setupEndTime = OMR_MAX(_setupEndTime, statsToMerge->_setupEndTime);
		_moveStartTime = (0 == _moveStartTime) ? statsToMerge->_moveStartTime : OMR_MIN(_moveStartTime, statsToMerge->_moveStartTime);
		_moveEndTime = OMR_MAX(_moveEndTime, statsToMerge->_moveEndTime);
		_fixupStartTime = (0 == _fixupStartTime) ? statsToMerge->_fixupStartTime : OMR_MIN(_fixupStartTime, statsToMerge->_fixupStartTime);
		_fixupEndTime = OMR_MAX(_fixupEndTime, statsToMerge->_fixupEndTime);
		_rootFixupStartTime = (0 == _rootFixupStartTime) ? statsToMerge->_rootFixupStartTime : OMR_MIN(_rootFixupStartTime, statsToMerge->_rootFixupStartTime);
		_rootFixupEndTime = OMR_MAX(_rootFixupEndTime, statsToMerge->_rootFixupEndTime);
		_moveStallTime += statsToMerge->_moveStallTime;
		_rebuildStallTime += statsToMerge->_rebuildStallTime;		
	};

	MM_CompactVLHGCStats() :
		_startTime(0)
		,_endTime(0)
	{
		clear();
	};

};

#endif /* OMR_GC_MODRON_COMPACTION */
#endif /* OMR_GC_VLHGC */
#endif /* COMPACTVLHGCSTATS_HPP_ */
