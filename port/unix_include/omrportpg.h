/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#ifndef omrportpg_h
#define omrportpg_h

/*
 * @ddr_namespace: default
 */

#include "omrcfg.h"
#if defined(LINUX)
#include "omrcgroup.h"
#endif /* defined(LINUX) */
#include "omrport.h"
#if defined(OMR_ENV_DATA64)
#include "omrportpriv.h"
#include "omrmem32struct.h"
#endif

/** Number of pageSizes supported.  There is always 1 for the default size, and 1 for the 0 terminator.
 * The number of large pages supported determines the remaining size.
 * Responsibility of the implementation of omrvmem to initialize this table correctly.
 */
#define OMRPORT_VMEM_PAGESIZE_COUNT 5

#if defined(OMR_PORT_NUMA_SUPPORT) && defined (LINUX)
#include <numa.h>
#include <sched.h>

typedef struct J9PortNodeMask {
	unsigned long mask[1024 / 8 / sizeof(unsigned long)];
} J9PortNodeMask;
#endif

typedef struct OMRPortPlatformGlobals {
	uintptr_t numa_platform_supports_numa;
	uintptr_t numa_platform_interleave_memory;
#if defined(OMR_PORT_NUMA_SUPPORT) && defined (LINUX)
	J9PortNodeMask numa_available_node_mask;
	unsigned long numa_max_node_bits;
	cpu_set_t process_affinity;
	J9PortNodeMask numa_mempolicy_node_mask;
	int numa_policy_mode; /* the NUMA policy returned by sys_get_mempolicy */
#endif
#if defined(OMR_ENV_DATA64)
	J9SubAllocateHeapMem32 subAllocHeapMem32;
#endif
	char *si_osType;
	char *si_osVersion;
	uintptr_t vmem_pageSize[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of supported page sizes */
	uintptr_t vmem_pageFlags[OMRPORT_VMEM_PAGESIZE_COUNT]; /** <0 terminated array of flags describing type of the supported page sizes */
	BOOLEAN isRunningInContainer;	
#if defined(LINUX) && defined(S390)
	int64_t last_clock_delta_update;  /** hw clock microsecond timestamp of last clock delta adjustment */
	int64_t software_msec_clock_delta; /** signed difference between hw and sw clocks in milliseconds */
#endif
	BOOLEAN loggingEnabled;
	uintptr_t systemLoggingFlags;
	BOOLEAN globalConverterEnabled;
	char *si_executableName;
#if defined(RS6000) || defined (LINUXPPC) || defined (PPC)
	int32_t mem_ppcCacheLineSize;
#endif
#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
	int32_t introspect_threadSuspendSignal;
#endif /* defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL) */
#if defined(LINUX)
	uint64_t cgroupSubsystemsAvailable; /**< cgroup subsystems available for port library to use; it is valid only when cgroupEntryList is non-null */
	uint64_t cgroupSubsystemsEnabled; /**< cgroup subsystems enabled in port library; it is valid only when cgroupEntryList is non-null */
	OMRCgroupEntry *cgroupEntryList; /**< head of the circular linked list, each element contains information about cgroup of the process for a subsystem */
	BOOLEAN syscallNotAllowed; /**< Assigned True if the mempolicy syscall is failed due to security opts (Can be seen in case of docker) */
#endif /* defined(LINUX) */
} OMRPortPlatformGlobals;


#if defined(RS6000) || defined (LINUXPPC) || defined (PPC)
#define PPG_mem_ppcCacheLineSize (portLibrary->portGlobals->platformGlobals.mem_ppcCacheLineSize)
#endif
#define PPG_numa_platform_supports_numa (portLibrary->portGlobals->platformGlobals.numa_platform_supports_numa)
#define PPG_numa_platform_interleave_memory (portLibrary->portGlobals->platformGlobals.numa_platform_interleave_memory)

#if defined(OMR_PORT_NUMA_SUPPORT) && defined (LINUX)
#define PPG_numa_available_node_mask (portLibrary->portGlobals->platformGlobals.numa_available_node_mask)
#define PPG_numa_max_node_bits (portLibrary->portGlobals->platformGlobals.numa_max_node_bits)
#define PPG_process_affinity (portLibrary->portGlobals->platformGlobals.process_affinity)
#define PPG_numa_mempolicy_node_mask (portLibrary->portGlobals->platformGlobals.numa_mempolicy_node_mask)
#define PPG_numa_policy_mode (portLibrary->portGlobals->platformGlobals.numa_policy_mode)
#endif
#define PPG_si_osType (portLibrary->portGlobals->platformGlobals.si_osType)
#define PPG_si_osVersion (portLibrary->portGlobals->platformGlobals.si_osVersion)
#define PPG_vmem_pageSize (portLibrary->portGlobals->platformGlobals.vmem_pageSize)
#define PPG_vmem_pageFlags (portLibrary->portGlobals->platformGlobals.vmem_pageFlags)
#define PPG_isRunningInContainer (portLibrary->portGlobals->platformGlobals.isRunningInContainer)
#if defined(LINUX) && defined(S390)
#define PPG_last_clock_delta_update  (portLibrary->portGlobals->platformGlobals.last_clock_delta_update)
#define PPG_software_msec_clock_delta (portLibrary->portGlobals->platformGlobals.software_msec_clock_delta)
#endif
#if defined(OMR_ENV_DATA64)
#define PPG_mem_mem32_subAllocHeapMem32 (portLibrary->portGlobals->platformGlobals.subAllocHeapMem32)
#endif

#define PPG_syslog_enabled (portLibrary->portGlobals->platformGlobals.loggingEnabled)
#define PPG_syslog_flags (portLibrary->portGlobals->platformGlobals.systemLoggingFlags)

#define PPG_global_converter_enabled (portLibrary->portGlobals->platformGlobals.globalConverterEnabled)

#define PPG_si_executableName (portLibrary->portGlobals->platformGlobals.si_executableName)

#if defined(OMR_CONFIGURABLE_SUSPEND_SIGNAL)
#define PPG_introspect_threadSuspendSignal (portLibrary->portGlobals->platformGlobals.introspect_threadSuspendSignal)
#endif

#if defined(LINUX)
/* Note that PPG_cgroupSubsystemsAvailable and PPG_cgroupSubsystemsEnabled are valid only if PPG_cgroupEntryList is not NULL */
#define PPG_cgroupSubsystemsAvailable (portLibrary->portGlobals->platformGlobals.cgroupSubsystemsAvailable)
#define PPG_cgroupSubsystemsEnabled (portLibrary->portGlobals->platformGlobals.cgroupSubsystemsEnabled)
#define PPG_cgroupEntryList (portLibrary->portGlobals->platformGlobals.cgroupEntryList)
#define PPG_numaSyscallNotAllowed (portLibrary->portGlobals->platformGlobals.syscallNotAllowed)
#endif /* defined(LINUX) */

#endif /* omrportpg_h */

