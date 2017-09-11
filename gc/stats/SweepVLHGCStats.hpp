/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(SWEEPVLHGCSTATS_HPP_)
#define SWEEPVLHGCSTATS_HPP_

#include "omrcfg.h"
#include "j9.h"
#include "omrport.h"
#include "j9consts.h"
#include "modronopt.h"

#if defined(OMR_GC_VLHGC)

/**
 * Storage for statistics relevant to the sweep phase of global collection
 * @ingroup GC_Stats
 */
class MM_SweepVLHGCStats
{
public:
	uintptr_t _gcCount; /**< The GC cycle in which these stats were collected */
	uint64_t _startTime;  /**< Start timestamp (hires ticks) for last sweep operation */
	uint64_t _endTime;  /**< End timestamp (hires ticks) for last sweep operation */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uint64_t idleTime;
	uint64_t mergeTime;
	
	uintptr_t sweepChunksTotal;
	uintptr_t sweepChunksProcessed;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	
	void clear()
	{
#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
		idleTime = 0;
		mergeTime = 0;
		sweepChunksProcessed = 0;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */
	}

	void merge(MM_SweepVLHGCStats *statsToMerge)
	{
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
	void addToIdleTime(uint64_t startTime, uint64_t endTime)
	{
		idleTime += (endTime - startTime);
	}

	/* Time is stored in raw format, converted to resolution at time of output
	 * Don't need to worry about wrap (endTime < startTime as unsigned math
	 * takes care of wrap
	 */
	void addToMergeTime(uint64_t startTime, uint64_t endTime)
	{
		mergeTime += (endTime - startTime);
	}
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	MM_SweepVLHGCStats() :
		_gcCount(UDATA_MAX)
		,_startTime(0)
		,_endTime(0)
	{
		clear();
	};
};

#endif /* OMR_GC_VLHGC */
#endif /* SWEEPVLHGCSTATS_HPP_ */
