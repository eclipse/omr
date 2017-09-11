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

#if !defined(SWEEPTSTATS_HPP_)
#define SWEEPTSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronopt.h"

#if defined(OMR_GC_MODRON_STANDARD) || defined(OMR_GC_REALTIME)

#include "Base.hpp"

/**
 * Storage for statistics relevant to the sweep phase of global collection
 * @ingroup GC_Stats
 */
class MM_SweepStats : public MM_Base
{
public:
	uintptr_t _gcCount; /**< The GC cycle in which these stats were collected */
	
#if defined(OMR_GC_CONCURRENT_SWEEP)
	uintptr_t sweepHeapBytesTotal;  /**< Number of heap bytes processed during the sweep phase */
#endif /* OMR_GC_CONCURRENT_SWEEP */

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	uint64_t idleTime;
	uint64_t mergeTime;
	
	uintptr_t sweepChunksTotal;
	uintptr_t sweepChunksProcessed;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	uint64_t _startTime;	/**< Sweep start time */
	uint64_t _endTime;		/**< Sweep end time */

	void clear();
	void merge(MM_SweepStats *statsToMerge);

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	void addToIdleTime(uint64_t startTime, uint64_t endTime);
	void addToMergeTime(uint64_t startTime, uint64_t endTime);
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	MM_SweepStats() :
		MM_Base()
		,_gcCount(UDATA_MAX)
		,_startTime(0)
		,_endTime(0)
	{
		clear();
	};
};

#endif /* OMR_GC_MODRON_STANDARD || OMR_GC_REALTIME */
#endif /* SWEEPTSTATS_HPP_ */
