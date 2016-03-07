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

#if !defined(COMPACTSTATS_HPP_)
#define COMPACTSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#if defined(OMR_GC_MODRON_STANDARD)

#if defined(OMR_GC_MODRON_COMPACTION)
#include "Base.hpp"

/**
 * Storage for stats relevant to the compaction phase of a collection.
 * @ingroup GC_Stats
 */
class MM_CompactStats : public MM_Base 
{
public:
	CompactReason _compactReason;
	CompactPreventedReason _compactPreventedReason;

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
		
	/* Remember gc count on last compaction of heap */
	uintptr_t _lastHeapCompaction;

	uint64_t _startTime;	/**< Compact start time */
	uint64_t _endTime;		/**< Compact end time */

	void clear();
	void merge(MM_CompactStats *statsToMerge);

	MM_CompactStats() :
		MM_Base()
		,_lastHeapCompaction(0)
		,_startTime(0)
		,_endTime(0)
	{
		clear();
	};

};

#endif /* OMR_GC_MODRON_COMPACTION */
#endif /* OMR_GC_MODRON_STANDARD */
#endif /* COMPACTSTATS_HPP_ */
