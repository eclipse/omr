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

#if !defined(EXCESSIVEGCSTATS_HPP_)
#define EXCESSIVEGCSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "Base.hpp"

/**
 * Storage for statistics tracking all collections that have occurred in the system.
 * @note This class is intended to have a single global instance
 * @ingroup GC_Stats
 */
class MM_ExcessiveGCStats : public MM_Base
{
public:
	uintptr_t gcCount; /**< Total number of GCs of all types that have occurred */
	
	/**
	 * Stats for determining excessive GC
	 * @{
	 */
	uint64_t startGCTimeStamp; /**< Raw timestamp (no units) of the start of the last GC */
	uint64_t endGCTimeStamp; /**< Raw timestamp (no units) of the end of the last GC */
	uintptr_t freeMemorySizeBefore; /**< The amount of free memory before the last GC */
	uintptr_t freeMemorySizeAfter; /**< The amount of free memory after the last GC */
	float newGCToUserTimeRatio; /**< Time spent in GC version vs. time in user code */
	float avgGCToUserTimeRatio; /**< Weighted average of time spent in GC version vs. time in user code */
	uint64_t totalGCTime; /**< Time (in microseconds) of all the GCs since the last global GC.  This includes all the local GCs (ie: new space scavenges) and the current global GC */
	uint64_t lastEndGlobalGCTimeStamp; /**< Raw timestamp (no units) of the end of the last global GC */
	/** @} */

	MM_ExcessiveGCStats() :
		MM_Base(),
		gcCount(0),
		startGCTimeStamp(0),
		endGCTimeStamp(0),
		freeMemorySizeBefore(0),
		freeMemorySizeAfter(0),
		newGCToUserTimeRatio(0.0),
		avgGCToUserTimeRatio(0.0),
		totalGCTime(0),
		lastEndGlobalGCTimeStamp(0)
	{}
}; 

#endif /* EXCESSIVEGCSTATS_HPP_ */
