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

#include "omrcfg.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)

#include "SweepStats.hpp"

void
MM_SweepStats::clear()
{
#if defined(OMR_GC_CONCURRENT_SWEEP)
	sweepHeapBytesTotal = 0;
#endif /* OMR_GC_CONCURRENT_SWEEP */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	idleTime = 0;
	mergeTime = 0;
	sweepChunksProcessed = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */			
}
	
void
MM_SweepStats::merge(MM_SweepStats *statsToMerge)
{
#if defined(OMR_GC_CONCURRENT_SWEEP)
	sweepHeapBytesTotal += statsToMerge->sweepHeapBytesTotal;
#endif /* OMR_GC_CONCURRENT_SWEEP */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	/* It may not ever be useful to merge these stats, but do it anyways */
	idleTime += statsToMerge->idleTime;
	mergeTime += statsToMerge->mergeTime;
	sweepChunksProcessed += statsToMerge->sweepChunksProcessed;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
/* Time is stored in raw format, converted to resolution at time of output 
 * Don't need to worry about wrap (endTime < startTime as unsigned math
 * takes care of wrap
 */
void
MM_SweepStats::addToIdleTime(uint64_t startTime, uint64_t endTime)
{
	idleTime += (endTime - startTime);	
}

/* Time is stored in raw format, converted to resolution at time of output 
 * Don't need to worry about wrap (endTime < startTime as unsigned math
 * takes care of wrap
 */
void
MM_SweepStats::addToMergeTime(uint64_t startTime, uint64_t endTime)
{
	mergeTime += (endTime - startTime);
}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
 
#endif /* OMR_GC_MODRON_STANDARD || OMR_GC_REALTIME */
