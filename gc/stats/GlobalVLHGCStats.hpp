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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Stats
 */

#if !defined(GLOBALVLHGCSTATS_HPP_)
#define GLOBALVLHGCSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#if defined(OMR_GC_VLHGC)

/**
 * Storage for statistics relevant to global garbage collections
 * @ingroup GC_Stats
 */
class MM_GlobalVLHGCStats
{
public:
	uintptr_t gcCount;  /**< Count of the number of GC cycles that have occurred */
	uintptr_t incrementCount;	/**< The number of incremental taxation entry point collection operations which have been completed since the VM started */

	/* Statistics which are used in heap sizing logic. Contains information that the heap needs to know about for purposes of heap resizing */

	uintptr_t _previousPgcPerGmpCount; /**< The number of PGC's that happened between the most recent GMP cycle, and the second most recent GMP cycle*/

	struct MM_HeapSizingData {
		uint64_t gmpTime;
		uint64_t avgPgcTimeUs;
		uint64_t avgPgcIntervalUs;
		uint64_t pgcCountSinceGMPEnd;
		uint64_t reservedSize;
		uint64_t freeTenure;
		intptr_t edenRegionChange;
		bool readyToResizeAtGlobalEnd;

		MM_HeapSizingData() :
			gmpTime(0),
			avgPgcTimeUs(0),
			avgPgcIntervalUs(0),
			pgcCountSinceGMPEnd(1),
			reservedSize(0),
			freeTenure(0),
			edenRegionChange(0), 
			readyToResizeAtGlobalEnd(true)
		{
		}
	} _heapSizingData; /**< A collection of data that is required by the total heap sizing logic */

	/* 
	 * When a GMP recently occured, GMP should be weighted according to how many PGC's occured before the GMP (historically) - NOT how many we currently observe.
	 *	If we saw 200 Pgc's before the recent GMP cycle, then we assume that we will still have around 200 PGC's, that is, until we are informed that this count is higher
	 *	If only had 2 PGC's happened before the last GMP, then GMP indeed has significant weight, and reading from _previousPgcPerGmpCount, will inform us of that	 
	 *  @return A PGC count which is representative of what we will likely observe until the next GMP
	 */ 
	uintptr_t getRepresentativePgcPerGmpCount(){
		return (uintptr_t)OMR_MAX(_previousPgcPerGmpCount, _heapSizingData.pgcCountSinceGMPEnd);
	}

	MM_GlobalVLHGCStats() :
		gcCount(0)
		,incrementCount(0)
		,_previousPgcPerGmpCount(1)
		,_heapSizingData()
	{};
};

#endif /* OMR_GC_VLHGC */
#endif /* GLOBALVLHGCSTATS_HPP_ */
