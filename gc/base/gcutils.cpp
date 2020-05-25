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

#include "omrcfg.h"
#include "omrcomp.h"
#include "gcutils.h"

extern "C" {
/**
 * Format output.
 * This functions updates byteSize and qualifier to the easiest human readable value
 * For example 2048 bytes will be return byteSize=2, *qualifier=K.
 * @param byteSize pointer to location containing the size in bytes.
 * @param qualifier Pointer to the location to store the qualifier.
 */
void
qualifiedSize(uintptr_t *byteSize, const char **qualifier)
{
	uintptr_t size;

	size = *byteSize;
	*qualifier = "";
	if(!(size % 1024)) {
		size /= 1024;
		*qualifier = "K";
		if(size && !(size % 1024)) {
			size /= 1024;
			*qualifier = "M";
			if(size && !(size % 1024)) {
				size /= 1024;
				*qualifier = "G";
			}
		}
	}
	*byteSize = size;
}

#if defined(OMR_GC_MODRON_COMPACTION)
/**
 * Return the reason for compaction as a string
 * @param reason reason code
 */
const char *
getCompactionReasonAsString(CompactReason reason)
{
	switch(reason) {
		case COMPACT_NONE:
			return "no compaction";
		case COMPACT_LARGE:
			return "compact to meet allocation";
		case COMPACT_AGGRESSIVE:
			return "compact on aggressive collection";	
		case COMPACT_FRAGMENTED:
			return "heap fragmented";
		case COMPACT_FORCED_GC:
			return "forced gc with compaction"; 
		case COMPACT_AVOID_DESPERATE:
			return "low free space (less than 4%)";
		case COMPACT_MEMORY_INSUFFICIENT:
			return "very low free space (less than 128kB)";
		case COMPACT_ALWAYS:
			return "forced compaction";
		case COMPACT_ABORTED_SCAVENGE:
			return "previous scavenge aborted";
		case COMPACT_CONTRACT:
			return "compact to aid heap contraction";
		case COMPACT_PAGE:
			return "page granularity fragmentation";	
		case COMPACT_MICRO_FRAG:
			return "micro fragmentation";	
		default:
			return "unknown";
	}
}

const char *
getCompactionPreventedReasonAsString(CompactPreventedReason reason)
{
	switch(reason) {
		case COMPACT_PREVENTED_NONE:
			return "compaction not prevented";
		case COMPACT_PREVENTED_CRITICAL_REGIONS:
			return "active JNI critical regions";
		default:
			return "unknown";
	}
}
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_GC_REALTIME)
/**
 * Return the reason for compaction as a string
 * @param reason reason code
 */
const char *
getGCReasonAsString(GCReason reason)
{
	switch(reason) {
		case TIME_TRIGGER:
			return "time triggered";
		case WORK_TRIGGER:
			return "work triggered";
		case OUT_OF_MEMORY_TRIGGER:
			return "out of memory";
		case SYSTEM_GC_TRIGGER:
			return "system GC";
		case VM_SHUTDOWN:
			return "VM shut down";
		case UNKOWN_REASON:
		default:
			return "unknown";
	}
}
#endif /* OMR_GC_REALTIME */

#if defined(OMR_GC_MODRON_SCAVENGER)
/**
 * Return the reason for percolation as a string
 * @param mode reason code
 */
const char *
getPercolateReasonAsString(PercolateReason mode)
{
	switch(mode) {
	case INSUFFICIENT_TENURE_SPACE:
		return "insufficient remaining tenure space";
	case FAILED_TENURE:
		return "failed tenure threshold reached";
	case MAX_SCAVENGES:
		return "maximum number of scavenges before global reached";
	case RS_OVERFLOW:
		return "RSO and heap walk unsafe";
	case UNLOADING_CLASSES:
		return "Unloading of classes requested";
	case EXPAND_FAILED:
		return "Previous scavenge failed to expand";
	case CRITICAL_REGIONS:
		return "Active JNI critical regions";
	case ABORTED_SCAVENGE:
		return "previous scavenge aborted";
	case CONCURRENT_MARK_EXHAUSTED:
		return "concurrent mark exhausted";
	case PREVENT_TENURE_EXPAND:
		return "prevent tenure expand";
	case MET_PROJECTED_TENURE_MAX_FREE:
		return "met projected tenure max free";
	case NONE_SET:
	default:
		return "unknown";
	}
}
#endif /* OMR_GC_MODRON_SCAVENGER */

/**
 * Return the reason for contraction as a string
 * @param reason reason code
 */
const char *
getContractReasonAsString(ContractReason reason)
{
	switch(reason) {
	case GC_RATIO_TOO_LOW:
		return "insufficient time being spent in gc";
	case FREE_SPACE_GREATER_MAXF:
		return "excess free space following gc";
	case SCAV_RATIO_TOO_LOW:
		return "insufficient time being spent scavenging";
	case SATISFY_EXPAND:
		return "enable expansion";
	case HEAP_RESIZE:
		return "heap reconfiguration";
	case FORCED_NURSERY_CONTRACT:
		return "forced nursery contract";
	default:
		return "unknown";
	}
}

/**
 * Return the reason for contraction as a string
 * @param reason reason code
 */
const char *
getExpandReasonAsString(ExpandReason reason)
{
	switch(reason) {
	case GC_RATIO_TOO_HIGH:
		return "excessive time being spent in gc";
	case FREE_SPACE_LESS_MINF:
		return "insufficient free space following gc";
	case SCAV_RATIO_TOO_HIGH:
		return "excessive time being spent scavenging";
	case SATISFY_COLLECTOR:
		return "continue current collection";
	case EXPAND_DESPERATE:
		return "satisfy allocation request";
	case FORCED_NURSERY_EXPAND:
		return "forced nursery expand";
	case HINT_PREVIOUS_RUNS:
		return "hint from previous runs";
	default:
		return "unknown";
	}
}

const char *
getLoaResizeReasonAsString(LoaResizeReason reason)
{
	switch(reason) {
	case LOA_EXPAND_HEAP_ALIGNMENT:
		return "expand to align heap";
	case LOA_EXPAND_FAILED_ALLOCATE:
		return "expand on failed allocate";
	case LOA_CONTRACT_AGGRESSIVE:
		return "contract on aggressive gc";
	case LOA_CONTRACT_MIN_SOA:
		return "contract to meet minimum soa";
	case LOA_CONTRACT_UNDERUTILIZED:
		return "contract underutilized loa";
	default:
		return "unknown";
	}
}

const char *
getSystemGCReasonAsString(uint32_t gcCode)
{
	switch(gcCode) {
	case J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC:
		return "explicit";
	case J9MMCONSTANT_EXPLICIT_GC_NOT_AGGRESSIVE:
		return "explicit not aggressive";
	case J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY:
		return "native out of memory";
	case J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT:
		return "rasdump";
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	case J9MMCONSTANT_EXPLICIT_GC_IDLE_GC:
		return "vm idle";
#endif
	case J9MMCONSTANT_IMPLICIT_GC_COMPLETE_CONCURRENT:
		return "complete concurrent cycle";
	default:
		return "unknown";
	}
}

} /* extern "C" */
