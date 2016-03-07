/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 ******************************************************************************/

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
		explicitGC = false;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
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
		aggressiveGC = true;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_DEFAULT:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_CRITICAL_REGIONS:
	case J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_UNLOADING_CLASSES:
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
		percolateGC = true;
		break;
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
	case J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE:
	case J9MMCONSTANT_IMPLICIT_GC_DEFAULT:
	case J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE:
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
