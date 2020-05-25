/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "GCCode.hpp"

#include "ModronAssertions.h"

/**
 * Determine if the GC is implicit or explicit (i.e. triggered externally).
 * @return true if the gc code indicates an explicit GC
 */
bool 
MM_GCCode::isExplicitGC() const
{
	bool explicitGC = false;

	switch (_gcCode) {
	case J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_DEFAULT:
	case J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES:
	case J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT:
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_ABORTED_SCAVENGE:
#endif
		explicitGC = false;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	case J9MMCONSTANT_EXPLICIT_GC_IDLE_GC:
#endif
		explicitGC = true;
		break;
	default:
		Assert_MM_unreachable();
	}
	
	return explicitGC;
}

/**
 * Determine if the GC should aggressively try to compact the heap.
 * @return true if heap should be compacted aggressively
 */
bool 
MM_GCCode::shouldAggressivelyCompact() const
{
	bool aggressivelyCompact = true;

	switch (_gcCode) {
	case J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE:
		aggressivelyCompact = true;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
	case J9MMCONSTANT_IMPLICIT_GC_DEFAULT:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES:
	case J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT:
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_ABORTED_SCAVENGE:
#endif
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	case J9MMCONSTANT_EXPLICIT_GC_IDLE_GC:
#endif
		aggressivelyCompact = false;
		break;
	default:
		Assert_MM_unreachable();
	}
	
	return aggressivelyCompact;
}

/**
 * Determine if the GC is going to throw OOM if enough memory is not collected.
 * @return true if OOM can be thrown at the end of this GC
 */
bool 
MM_GCCode::isOutOfMemoryGC() const
{
	bool OOM = true;

	switch (_gcCode) {
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
	case J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE:
		OOM = true;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
	case J9MMCONSTANT_IMPLICIT_GC_DEFAULT:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES:
	case J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT:
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_ABORTED_SCAVENGE:
#endif
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	case J9MMCONSTANT_EXPLICIT_GC_IDLE_GC:
#endif
		OOM = false;
		break;
	default:
		Assert_MM_unreachable();
	}
	
	return OOM;
}

/**
 * Determine if the GC should be aggressive.
 * @return true if the gc code indicates an aggressive GC
 */
bool 
MM_GCCode::isAggressiveGC() const
{
	bool aggressiveGC = true;

	switch (_gcCode) {
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
	case J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE:
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	case J9MMCONSTANT_EXPLICIT_GC_IDLE_GC:
#endif
		aggressiveGC = true;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_DEFAULT:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES:
	case J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT:
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_ABORTED_SCAVENGE:
#endif
		aggressiveGC = false;
		break;
	default:
		Assert_MM_unreachable();
	}
	
	return aggressiveGC;
}

/**
 * Determine if it is a percolate GC call.
 * @param gcCode requested GC code
 * @return true if it is a percolate call
 */
bool 
MM_GCCode::isPercolateGC() const
{
	bool percolateGC = false;

	switch (_gcCode) {
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES:
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_ABORTED_SCAVENGE:
#endif
		percolateGC = true;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
	case J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_DEFAULT:
	case J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT:
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	case J9MMCONSTANT_EXPLICIT_GC_IDLE_GC:
#endif
		percolateGC = false;
		break;
	default:
		Assert_MM_unreachable();
	}
	
	return percolateGC;
}

/**
 * Determine if it is a GC request from a RAS dump agent.
 * @return true if it is a RAS dump call
 */	
bool 
MM_GCCode::isRASDumpGC() const
{
	return J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT == _gcCode;
}
