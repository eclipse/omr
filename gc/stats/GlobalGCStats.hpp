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

#if !defined(GLOBALGCSTATS_HPP_)
#define GLOBALGCSTATS_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "ClassUnloadStats.hpp"
#if defined(OMR_GC_MODRON_COMPACTION)
#include "CompactStats.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#include "MarkStats.hpp"
#include "MetronomeStats.hpp"
#include "SweepStats.hpp"
#include "WorkPacketStats.hpp"

/**
 * Storage for statistics relevant to global garbage collections
 * @ingroup GC_Stats_Core
 */
class MM_GlobalGCStats {
public:
	uintptr_t gcCount; /**< Count of the number of GC cycles that have occurred */
	MM_WorkPacketStats workPacketStats;
	MM_SweepStats sweepStats;
#if defined(OMR_GC_MODRON_COMPACTION)
	MM_CompactStats compactStats;
#endif /* OMR_GC_MODRON_COMPACTION */

	uintptr_t fixHeapForWalkReason;
	uint64_t fixHeapForWalkTime;

	MM_MarkStats markStats;
	MM_ClassUnloadStats classUnloadStats;
	MM_MetronomeStats metronomeStats; /**< Stats collected during one GC increment (quantum) */

	uintptr_t finalizableCount; /**< count of objects pushed for finalization during one GC cycle */

	MMINLINE void clear()
	{
		/* gcCount is not cleared as the value must persist across cycles */

		workPacketStats.clear();
		sweepStats.clear();
#if defined(OMR_GC_MODRON_COMPACTION)
		compactStats.clear();
#endif /* OMR_GC_MODRON_COMPACTION */

		fixHeapForWalkReason = FIXUP_NONE;
		fixHeapForWalkTime = 0;

		markStats.clear();
		classUnloadStats.clear();
		metronomeStats.clearStart();

		finalizableCount = 0;
	};

	MM_GlobalGCStats()
		: gcCount(0)
		, workPacketStats()
		, sweepStats()
#if defined(OMR_GC_MODRON_COMPACTION)
		, compactStats()
#endif /* OMR_GC_MODRON_COMPACTION */
		, fixHeapForWalkReason(FIXUP_NONE)
		, fixHeapForWalkTime(0)
		, markStats()
		, classUnloadStats()
		, metronomeStats()
		, finalizableCount(0) {};
};

#endif /* GLOBALGCSTATS_HPP_ */
