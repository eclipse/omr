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
