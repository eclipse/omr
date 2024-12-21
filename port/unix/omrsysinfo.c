/*******************************************************************************
 * Copyright IBM Corp. and others 2015
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief System information
 */

#if 0
#define ENV_DEBUG
#endif

#if defined(LINUX) && !defined(OMRZTPF)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#elif defined(OSX)
#define _XOPEN_SOURCE
#include <libproc.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include <mach-o/dyld.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif /* defined(LINUX) && !defined(OMRZTPF) */

#if defined(OSX) || defined(OMR_OS_BSD)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif /* defined(OSX) || defined(OMR_OS_BSD) */

#if defined(OSX)
#include <crt_externs.h>
#endif /* defined(OSX) */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include "omrport.h"
#if defined(J9OS_I5)
#include "Xj9I5OSInterface.H"
#endif
#if !defined(J9ZOS390)
#include <sys/param.h>
#endif /* !defined(J9ZOS390) */
#include <sys/time.h>
#include <sys/resource.h>
#include <nl_types.h>
#include <langinfo.h>


#if defined(J9ZOS390)
#include "omrgetthent.h"
#include "omrsimap.h"
#endif /* defined(J9ZOS390) */

#if defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(OMRZTPF)) || (defined(AARCH64) && defined(LINUX))
#include "auxv.h"
#include <strings.h>
#endif /* defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(OMRZTPF)) || (defined(AARCH64) && defined(LINUX)) */

#if (defined(S390))
#include "omrportpriv.h"
#include "omrportpg.h"
#endif /* defined(S390) */

#if defined(AIXPPC)
#include <fcntl.h>
#include <procinfo.h>
#include <sys/procfs.h>
#include <sys/systemcfg.h>
#endif /* defined(AIXPPC) */

#include "omrsysinfo_helpers.h"

/* Start copy from omrfiletext.c */
/* __STDC_ISO_10646__ indicates that the platform wchar_t encoding is Unicode */
/* but older versions of libc fail to set the flag, even though they are Unicode */
#if defined(__STDC_ISO_10646__) || defined(LINUX) ||defined(OSX)
#define J9VM_USE_MBTOWC
#else /* defined(__STDC_ISO_10646__) || defined(LINUX) || defined(OSX) */
#include "omriconvhelpers.h"
#endif /* defined(__STDC_ISO_10646__) || defined(LINUX) || defined(OSX) */

/* a2e overrides nl_langinfo to return ASCII strings. We need the native EBCDIC string */
#if defined(J9ZOS390) && defined(nl_langinfo)
#undef nl_langinfo
#endif

#if defined(J9ZOS390)
#include "omrgetuserid.h"
#endif

/* End copy from omrfiletext.c */

#ifdef AIXPPC
#if defined(J9OS_I5)
/* The PASE compiler does not support libperfstat.h */
#else
#include <libperfstat.h>
#endif
#include <sys/proc.h>

/* omrsysinfo_get_number_CPUs_by_type */
#include <sys/thread.h>
#include <sys/rset.h>
#endif

#ifdef RS6000
#include <sys/systemcfg.h>
#include <sys/sysconfig.h>
#include <assert.h>

#if defined(OMR_ENV_DATA64)
#define LIBC_NAME "/usr/lib/libc.a(shr_64.o)"
#else
#define LIBC_NAME "/usr/lib/libc.a(shr.o)"
#endif
#endif

#if defined(LINUX) && !defined(OMRZTPF)
#include <linux/magic.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h>
#include <sched.h>
#elif defined(OSX) /* defined(LINUX) && !defined(OMRZTPF) */
#include <sys/sysctl.h>
#endif /* defined(LINUX) && !defined(OMRZTPF) */

#include <unistd.h>

#if defined(LINUX)
#include "omrcgroup.h"
#endif /* defined(LINUX) */
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrportptb.h"
#include "portnls.h"
#include "ut_omrport.h"

#if defined(OMRZTPF)
#include <locale.h>
#include <tpf/c_cinfc.h>
#include <tpf/c_dctist.h>
#include <tpf/tpfapi.h>
#include <tpf/sysapi.h>
#endif /* defined(OMRZTPF) */

#if defined(J9ZOS390)
#include <sys/ps.h>
#include <sys/types.h>
#if !defined(OMR_EBCDIC)
#include "atoe.h"
#endif

#if !defined(PATH_MAX)
/* This is a somewhat arbitrarily selected fixed buffer size. */
#define PATH_MAX 1024
#endif

#pragma linkage (GETNCPUS,OS)
#pragma map (Get_Number_Of_CPUs,"GETNCPUS")
uintptr_t Get_Number_Of_CPUs();

#endif

#define JIFFIES         100
#define USECS_PER_SEC   1000000
#define TICKS_TO_USEC   ((uint64_t)(USECS_PER_SEC/JIFFIES))
#define OMRPORT_SYSINFO_PROC_DIR_BUFFER_SIZE 256
#define OMRPORT_SYSINFO_NUM_SYSCTL_ARGS 4
#define OMRPORT_SYSINFO_NANOSECONDS_PER_MICROSECOND 1000ULL
#if defined(_LP64)
#define GETTHENT BPX4GTH
#else /* defined(_LP64) */
#define GETTHENT BPX1GTH
#endif /* defined(_LP64) */

static uintptr_t copyEnvToBuffer(struct OMRPortLibrary *portLibrary, void *args);
static uintptr_t copyEnvToBufferSignalHandler(struct OMRPortLibrary *portLib, uint32_t gpType, void *gpInfo, void *unUsed);

static void setPortableError(OMRPortLibrary *portLibrary, const char *funcName, int32_t portlibErrno, int systemErrno);

#if (defined(LINUXPPC) || defined(AIXPPC))
static OMRProcessorArchitecture omrsysinfo_map_ppc_processor(const char *processorName);
const char* omrsysinfo_get_ppc_processor_feature_name(uint32_t feature);
#endif /* (defined(LINUXPPC) || defined(AIXPPC)) */

#if defined(AIXPPC) || defined(S390) || defined(J9ZOS390) || (defined(AARCH64) && defined(OSX))
static void omrsysinfo_set_feature(OMRProcessorDesc *desc, uint32_t feature);
#endif /* defined(AIXPPC) || defined(S390) || defined(J9ZOS390) || (defined(AARCH64) && defined(OSX)) */

#if defined(LINUXPPC)
static intptr_t omrsysinfo_get_linux_ppc_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);

#if !defined(AT_HWCAP2)
#define AT_HWCAP2 26 /* needed until glibc 2.17 */
#endif /* !defined(AT_HWCAP2) */

#endif /* defined(LINUXPPC) */

#if defined(AIXPPC)
static intptr_t omrsysinfo_get_aix_ppc_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
#endif /* defined(AIXPPC) */

#if !defined(__power_8)
#define POWER_8 0x10000 /* Power 8 class CPU */
#define __power_8() (_system_configuration.implementation == POWER_8)
#if !defined(J9OS_I5_V6R1)
#define PPI8_1 0x4B
#define PPI8_2 0x4D
#define __phy_proc_imp_8() (_system_configuration.phys_implementation == PPI8_1 || _system_configuration.phys_implementation == PPI8_2)
#endif /* !defined(J9OS_I5_V6R1) */
#endif /* !defined(__power_8) */

#if !defined(__power_9)
#define POWER_9 0x20000 /* Power 9 class CPU */
#define __power_9() (_system_configuration.implementation == POWER_9)
#endif /* !defined(__power_9) */

#if !defined(__power_10)
#define POWER_10 0x40000 /* Power 10 class CPU */
#define __power_10() (_system_configuration.implementation == POWER_10)
#endif /* !defined(__power_10) */

#if !defined(__power_11)
#define POWER_11 0x80000 /* Power 11 class CPU */
#define __power_11() (_system_configuration.implementation == POWER_11)
#endif /* !defined(__power_11) */

/*
 * Please update the macro below to stay in sync with the latest POWER processor known to OMR,
 * ensuring CPU recognition is more robust than in the past. As the macro currently stands, any
 * later processors are recognized as at least POWER11.
 */
#define POWER11_OR_ABOVE (0xFFFFFFFF << 19)
#define __power_latestKnownAndUp() OMR_ARE_ANY_BITS_SET(_system_configuration.implementation, POWER11_OR_ABOVE)

#if defined(J9OS_I5_V6R1) /* vmx_version id only available since TL4 */
#define __power_vsx() (_system_configuration.vmx_version > 1)
#endif

#if !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1)
/* both i 7.1 and i 7.2 do not support this function */
#if !defined(SC_TM_VER)
#define SC_TM_VER 59
#endif  /* !defined(SC_TM_VER) */

#if !defined(__power_tm)
#define __power_tm() ((long)getsystemcfg(SC_TM_VER) > 0) /* taken from AIX 7.1 sys/systemcfg.h */
#endif  /* !defined(__power_tm) */
#endif /* !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1) */

#if (defined(S390) || defined(J9ZOS390) || defined(OMRZTPF))
static BOOLEAN omrsysinfo_test_stfle(struct OMRPortLibrary *portLibrary, uint64_t stfleBit);
static intptr_t omrsysinfo_get_s390_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
const char * omrsysinfo_get_s390_processor_feature_name(uint32_t feature);
#endif /* defined(S390) || defined(J9ZOS390) || defined(OMRZTPF) */

#if defined(RISCV)
static intptr_t omrsysinfo_get_riscv_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
#endif

#if defined(AARCH64)
static const char *omrsysinfo_get_aarch64_processor_feature_name(uint32_t feature);
#if defined(LINUX)
static intptr_t omrsysinfo_get_linux_aarch64_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
#elif defined(OSX) /* defined(LINUX) */
static intptr_t omrsysinfo_get_macos_aarch64_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
#endif /* defined(LINUX) */
#endif /* defined(AARCH64) */

static const char getgroupsErrorMsgPrefix[] = "getgroups : ";

typedef struct EnvListItem {
	struct EnvListItem *next;
	char *nameAndValue;
} EnvListItem;

typedef struct CopyEnvToBufferArgs {
	uintptr_t bufferSizeBytes;
	void *buffer;
	uintptr_t numElements;
} CopyEnvToBufferArgs;

/* For the omrsysinfo_limit_iterator */
struct {
	int resource;
	char *resourceName;
} limitMap[] = {
#if defined(RLIMIT_AS)
	{RLIMIT_AS, "RLIMIT_AS"},
#endif
#if defined(RLIMIT_CORE)
	{RLIMIT_CORE, "RLIMIT_CORE"},
#endif
#if defined(RLIMIT_CPU)
	{RLIMIT_CPU, "RLIMIT_CPU"},
#endif
#if defined(RLIMIT_DATA)
	{RLIMIT_DATA, "RLIMIT_DATA"},
#endif
#if defined(RLIMIT_FSIZE)
	{RLIMIT_FSIZE, "RLIMIT_FSIZE"},
#endif
#if defined(RLIMIT_LOCKS)
	{RLIMIT_LOCKS, "RLIMIT_LOCKS"},
#endif
#if defined(RLIMIT_MEMLOCK)
	{RLIMIT_MEMLOCK, "RLIMIT_MEMLOCK"},
#endif
#if defined(RLIMIT_NOFILE)
	{RLIMIT_NOFILE, "RLIMIT_NOFILE"},
#endif
#if defined(RLIMIT_THREADS)
	{RLIMIT_THREADS, "RLIMIT_THREADS"},
#endif
#if defined(RLIMIT_NPROC)
	{RLIMIT_NPROC, "RLIMIT_NPROC"},
#endif
#if defined(RLIMIT_RSS)
	{RLIMIT_RSS, "RLIMIT_RSS"},
#endif
#if defined(RLIMIT_STACK)
	{RLIMIT_STACK, "RLIMIT_STACK"},
#endif
#if defined(RLIMIT_MSGQUEUE)
	{RLIMIT_MSGQUEUE, "RLIMIT_MSGQUEUE"}, /* since Linux 2.6.8 */
#endif
#if defined(RLIMIT_NICE)
	{RLIMIT_NICE, "RLIMIT_NICE"}, /* since Linux 2.6.12*/
#endif
#if defined(RLIMIT_RTPRIO)
	{RLIMIT_RTPRIO, "RLIMIT_RTPRIO"}, /* since Linux 2.6.12 */
#endif
#if defined(RLIMIT_SIGPENDING)
	{RLIMIT_SIGPENDING, "RLIMIT_SIGPENDING"}, /* since linux 2.6.8 */
#endif
#if defined(RLIMIT_MEMLIMIT)
	{RLIMIT_MEMLIMIT, "RLIMIT_MEMLIMIT"}, /* likely z/OS only */
#endif
};

#if defined(LINUX)

#define OMR_CGROUP_MOUNT_POINT "/sys/fs/cgroup"
#define ROOT_CGROUP "/"
#define SYSTEMD_INIT_CGROUP "/init.scope"
#define OMR_PROC_PID_ONE_CGROUP_FILE "/proc/1/cgroup"
#define OMR_PROC_PID_ONE_SCHED_FILE "/proc/1/sched"
#define MAX_DEFAULT_VALUE_CHECK (LLONG_MAX - (1024 * 1024 * 1024)) /* subtracting the MAX page size (1GB) from LLONG_MAX to check against a value */
#define CGROUP_METRIC_FILE_CONTENT_MAX_LIMIT 1024

/* An entry in /proc/<pid>/cgroup is of following form:
 *  <hierarchy ID>:<subsystem>[,<subsystem>]*:<cgroup name>
 *
 * An example:
 *  7:cpuacct,cpu:/mycgroup
 */
#define PROC_PID_CGROUPV1_ENTRY_FORMAT "%d:%[^:]:%s"
#define PROC_PID_CGROUP_SYSTEMD_ENTRY_FORMAT "%d::%s"
/* For cgroup v2, the form is:
 *  0::<cgroup name>
 */
#define PROC_PID_CGROUPV2_ENTRY_FORMAT "0::%s"

#define SINGLE_CGROUP_METRIC 1
#define MAX_64BIT_INT_LENGTH 20
/* The metricKeyInFile field in an OMRCgroupMetricInfoElement being set to this
 * indicates that the file containing this element is in the space-separated format,
 * i.e.
 *  VAL0 VAL1 ...\n
 * All 'OMRCgroupMetricInfoElement's corresponding to values in a file of this format
 * must have their metricKeyInFile set to CGROUP_METRIC_NO_KEY and OMRCgroupMetricInfoElement
 * arrays must list metrics in order of appearance in their file.
 * CGROUP_METRIC_NO_KEY does not apply to newline-separated files.
 */
#define CGROUP_METRIC_NO_KEY -1

/* Used for PPG_sysinfoControlFlags global, set bits indicate cgroup v1 availability,
 * cgroup v2 availability, and process running in container, respectively.
 */
#define OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE 0x1
#define OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE 0x2

/* Different states of PPG_processInContainerState. */
#define OMRPORT_PROCESS_IN_CONTAINER_UNINITIALIZED 0x0
#define OMRPORT_PROCESS_IN_CONTAINER_TRUE 0x1
#define OMRPORT_PROCESS_IN_CONTAINER_FALSE 0x2
#define OMRPORT_PROCESS_IN_CONTAINER_ERROR 0x3

/* Cgroup v1 and v2 memory files */
#define CGROUP_MEMORY_STAT_FILE "memory.stat"
#define CGROUP_MEMORY_SWAPPINESS "memory.swappiness"

/* Cgroup v1 memory files */
#define CGROUP_MEMORY_LIMIT_IN_BYTES_FILE "memory.limit_in_bytes"
#define CGROUP_MEMORY_USAGE_IN_BYTES_FILE "memory.usage_in_bytes"
#define CGROUP_MEMORY_SWAP_LIMIT_IN_BYTES_FILE "memory.memsw.limit_in_bytes"
#define CGROUP_MEMORY_SWAP_USAGE_IN_BYTES_FILE "memory.memsw.usage_in_bytes"

#define CGROUP_MEMORY_STAT_CACHE_METRIC "cache"
#define CGROUP_MEMORY_STAT_CACHE_METRIC_SZ (sizeof(CGROUP_MEMORY_STAT_CACHE_METRIC)-1)

/* Cgroup v2 memory files */
#define CGROUP_MEMORY_MAX_FILE "memory.max"
#define CGROUP_MEMORY_CURRENT_FILE "memory.current"
#define CGROUP_MEMORY_SWAP_MAX_FILE "memory.swap.max"
#define CGROUP_MEMORY_SWAP_CURRENT_FILE "memory.swap.current"

#define CGROUP_MEMORY_STAT_FILE_METRIC "file"
#define CGROUP_MEMORY_STAT_FILE_METRIC_SZ (sizeof(CGROUP_MEMORY_STAT_FILE_METRIC)-1)

/* Cgroup v1 cpu files */
#define CGROUP_CPU_CFS_QUOTA_US_FILE "cpu.cfs_quota_us"
#define CGROUP_CPU_CFS_PERIOD_US_FILE "cpu.cfs_period_us"

/* Cgroup v2 cpu files */
#define CGROUP_CPU_MAX_FILE "cpu.max"

/* Currently 12 subsystems or resource controllers are defined.
 */
typedef enum OMRCgroupSubsystem {
	INVALID_SUBSYSTEM = -1,
	BLKIO,
	CPU,
	CPUACCT,
	CPUSET,
	DEVICES,
	FREEZER,
	HUGETLB,
	MEMORY,
	NET_CLS,
	NET_PRIO,
	PERF_EVENT,
	PIDS,
	NUM_SUBSYSTEMS
} OMRCgroupSubsystem;

const char * const subsystemNames[NUM_SUBSYSTEMS] = {
	"blkio",
	"cpu",
	"cpuacct",
	"cpuset",
	"devices",
	"freezer",
	"hugetlb",
	"memory",
	"net_cls",
	"net_prio",
	"perf_event",
	"pids",
};

struct {
	const char *name;
	uint64_t flag;
} supportedSubsystems[] = {
	{ "cpu", OMR_CGROUP_SUBSYSTEM_CPU },
	{ "memory", OMR_CGROUP_SUBSYSTEM_MEMORY },
	{ "cpuset", OMR_CGROUP_SUBSYSTEM_CPUSET}
};

typedef struct OMRCgroupMetricInfoElement {
	char *metricTag;
	char *metricKeyInFile;
	char *metricUnit;
	BOOLEAN isValueToBeChecked;
} OMRCgroupMetricInfoElement;

static struct OMRCgroupMetricInfoElement oomControlMetricElementListV1[] = {
	{ "OOM Killer Disabled", "oom_kill_disable", NULL, FALSE },
	{ "Under OOM", "under_oom", NULL, FALSE }
};

static struct OMRCgroupMetricInfoElement memEventsMetricElementListV2[] = {
	{ "Approached memory limit count", "max", NULL, FALSE },
	{ "Reached memory limit count", "oom", NULL, FALSE }
};

static struct OMRCgroupMetricInfoElement swapEventsMetricElementListV2[] = {
	{ "Approached swap limit count", "max", NULL, FALSE },
	{ "Swap alloc failed count", "fail", NULL, FALSE }
};

static struct OMRCgroupMetricInfoElement cpuStatMetricElementListV1[] = {
	{ "Period intervals elapsed count", "nr_periods", NULL, FALSE },
	{ "Throttled count", "nr_throttled", NULL, FALSE },
	{ "Total throttle time", "throttled_time", "nanoseconds", FALSE }
};

/* The cpu.max file contains two space separated values in the format
 *  $QUOTA $PERIOD
 */
static struct OMRCgroupMetricInfoElement cpuMaxMetricElementListV2[] = {
	{ "CPU Quota", (char *)CGROUP_METRIC_NO_KEY, "microseconds", TRUE },
	{ "CPU Period", (char *)CGROUP_METRIC_NO_KEY, "microseconds", FALSE }
};

static struct OMRCgroupMetricInfoElement cpuStatMetricElementListV2[] = {
	{ "Period intervals elapsed count", "nr_periods", NULL, FALSE },
	{ "Throttled count", "nr_throttled", NULL, FALSE },
	{ "Total throttle time", "throttled_usec", "microseconds", FALSE }
};

typedef struct OMRCgroupSubsystemMetricMap {
	char *metricFileName;
	OMRCgroupMetricInfoElement *metricInfoElementList;
	int32_t metricElementsCount;
} OMRCgroupSubsystemMetricMap;

static struct OMRCgroupSubsystemMetricMap omrCgroupMemoryMetricMapV1[] = {
	{ "memory.limit_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory Limit", NULL, "bytes", TRUE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.limit_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory + Swap Limit", NULL, "bytes", TRUE }, SINGLE_CGROUP_METRIC },
	{ "memory.usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory + Swap Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.max_usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory Max Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.max_usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory + Swap Max Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.failcnt", &(OMRCgroupMetricInfoElement){ "Memory limit exceeded count", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.failcnt", &(OMRCgroupMetricInfoElement){ "Memory + Swap limit exceeded count", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.oom_control", &oomControlMetricElementListV1[0], sizeof(oomControlMetricElementListV1)/sizeof(oomControlMetricElementListV1[0]) }
};
#define OMR_CGROUP_MEMORY_METRIC_MAP_V1_SIZE sizeof(omrCgroupMemoryMetricMapV1) / sizeof(omrCgroupMemoryMetricMapV1[0])

static struct OMRCgroupSubsystemMetricMap omrCgroupMemoryMetricMapV2[] = {
	{ "memory.max", &(OMRCgroupMetricInfoElement){ "Memory Limit", NULL, "bytes", TRUE }, SINGLE_CGROUP_METRIC },
	{ "memory.swap.max", &(OMRCgroupMetricInfoElement){ "Swap Limit", NULL, "bytes", TRUE }, SINGLE_CGROUP_METRIC },
	{ "memory.current", &(OMRCgroupMetricInfoElement){ "Memory Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.swap.current", &(OMRCgroupMetricInfoElement){ "Swap Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.events", &memEventsMetricElementListV2[0], sizeof(memEventsMetricElementListV2)/sizeof(memEventsMetricElementListV2[0]) },
	{ "memory.swap.events", &swapEventsMetricElementListV2[0], sizeof(swapEventsMetricElementListV2)/sizeof(swapEventsMetricElementListV2[0]) }
};
#define OMR_CGROUP_MEMORY_METRIC_MAP_V2_SIZE sizeof(omrCgroupMemoryMetricMapV2) / sizeof(omrCgroupMemoryMetricMapV2[0])

static struct OMRCgroupSubsystemMetricMap omrCgroupCpuMetricMapV1[] = {
	{ "cpu.cfs_period_us", &(OMRCgroupMetricInfoElement){ "CPU Period", NULL, "microseconds", FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpu.cfs_quota_us", &(OMRCgroupMetricInfoElement){ "CPU Quota", NULL, "microseconds", TRUE }, SINGLE_CGROUP_METRIC },
	{ "cpu.shares", &(OMRCgroupMetricInfoElement){ "CPU Shares", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpu.stat", &cpuStatMetricElementListV1[0], sizeof(cpuStatMetricElementListV1)/sizeof(cpuStatMetricElementListV1[0]) }
};
#define OMR_CGROUP_CPU_METRIC_MAP_V1_SIZE sizeof(omrCgroupCpuMetricMapV1) / sizeof(omrCgroupCpuMetricMapV1[0])

static struct OMRCgroupSubsystemMetricMap omrCgroupCpuMetricMapV2[] = {
	{ "cpu.max", &cpuMaxMetricElementListV2[0], sizeof(cpuMaxMetricElementListV2)/sizeof(cpuMaxMetricElementListV2[0]) },
	{ "cpu.weight", &(OMRCgroupMetricInfoElement){ "CPU Weight relative to procs in same cgroup", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpu.stat", &cpuStatMetricElementListV2[0], sizeof(cpuStatMetricElementListV2)/sizeof(cpuStatMetricElementListV2[0]) }
};
#define OMR_CGROUP_CPU_METRIC_MAP_V2_SIZE sizeof(omrCgroupCpuMetricMapV2) / sizeof(omrCgroupCpuMetricMapV2[0])

static struct OMRCgroupSubsystemMetricMap omrCgroupCpusetMetricMapV1[] = {
	{ "cpuset.cpu_exclusive", &(OMRCgroupMetricInfoElement){ "CPU exclusive", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.mem_exclusive", &(OMRCgroupMetricInfoElement){ "Mem exclusive", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.cpus", &(OMRCgroupMetricInfoElement){ "CPUs", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.mems", &(OMRCgroupMetricInfoElement){ "Mems", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC }
};
#define OMR_CGROUP_CPUSET_METRIC_MAP_V1_SIZE sizeof(omrCgroupCpusetMetricMapV1) / sizeof(omrCgroupCpusetMetricMapV1[0])

/* cpuset.cpus and cpuset.mems files may be empty, which indicates the same settings
 * as parent cgroup, or that all cpus are available.
 */
static struct OMRCgroupSubsystemMetricMap omrCgroupCpusetMetricMapV2[] = {
	{ "cpuset.cpus", &(OMRCgroupMetricInfoElement){ "CPUs", NULL, NULL, TRUE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.mems", &(OMRCgroupMetricInfoElement){ "Mems", NULL, NULL, TRUE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.cpus.effective", &(OMRCgroupMetricInfoElement){ "Effective CPUs", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.mems.effective", &(OMRCgroupMetricInfoElement){ "Effective Mems", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC }
};
#define OMR_CGROUP_CPUSET_METRIC_MAP_V2_SIZE sizeof(omrCgroupCpusetMetricMapV2) / sizeof(omrCgroupCpusetMetricMapV2[0])

static uint32_t attachedPortLibraries;

/* Used to enforce data consistency for
 * - PPG_cgroupEntryList, and
 * - PPG_processInContainerState.
 * Both variables are related and modified in the same code-path.
 */
static omrthread_monitor_t cgroupMonitor;
#endif /* defined(LINUX) */

static intptr_t cwdname(struct OMRPortLibrary *portLibrary, char **result);
static uint32_t getLimitSharedMemory(struct OMRPortLibrary *portLibrary, uint64_t *limit);
static uint32_t getLimitFileDescriptors(struct OMRPortLibrary *portLibrary, uint64_t *result, BOOLEAN hardLimitRequested);
static uint32_t setLimitFileDescriptors(struct OMRPortLibrary *portLibrary, uint64_t limit, BOOLEAN hardLimitRequested);
#if defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390)
static intptr_t readSymbolicLink(struct OMRPortLibrary *portLibrary, char *linkFilename, char **result);
#endif /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) */
#if defined(AIXPPC) || defined(J9ZOS390)
static BOOLEAN isSymbolicLink(struct OMRPortLibrary *portLibrary, char *filename);
static intptr_t searchSystemPath(struct OMRPortLibrary *portLibrary, char *filename, char **result);
#endif /* defined(AIXPPC) || defined(J9ZOS390) */

#if defined(J9ZOS390)
static void setOSFeature(struct OMROSDesc *desc, uint32_t feature);
static intptr_t getZOSDescription(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc);
#if defined(_LP64)
#pragma linkage(BPX4GTH,OS)
#else /* defined(_LP64) */
#pragma linkage(BPX1GTH,OS)
#endif /* defined(_LP64) */
void GETTHENT(
	unsigned int *inputSize,
	unsigned char **input,
	unsigned int *outputSize,
	unsigned char **output,
	unsigned int *ret,
	unsigned int *retCode,
	unsigned int *reasonCode);
#endif /* defined(J9ZOS390) */

#if !defined(RS6000) && !defined(J9ZOS390) && !defined(OSX) && !defined(OMRZTPF)
static uint64_t getPhysicalMemory(struct OMRPortLibrary *portLibrary);
#elif defined(OMRZTPF) /* !defined(RS6000) && !defined(J9ZOS390) && !defined(OSX)  && !defined(OMRZTPF) */
static uint64_t getPhysicalMemory();
#endif /* !defined(RS6000) && !defined(J9ZOS390) && !defined(OSX) && !defined(OMRZTPF) */

#if defined(OMRZTPF)
	uintptr_t get_IPL_IstreamCount();
	uintptr_t get_Dispatch_IstreamCount();
#endif /* defined(OMRZTPF) */

#if defined(LINUX) && !defined(OMRZTPF)
static BOOLEAN isCgroupV1Available(struct OMRPortLibrary *portLibrary);
static BOOLEAN isCgroupV2Available(void);
static void freeCgroupEntries(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList);
static char * getCgroupNameForSubsystem(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList, const char *subsystem);
static int32_t addCgroupEntry(struct OMRPortLibrary *portLibrary, OMRCgroupEntry **cgEntryList, int32_t hierId, const char *subsystem, const char *cgroupName, uint64_t flag);
static int32_t populateCgroupEntryListV1(struct OMRPortLibrary *portLibrary, int pid, BOOLEAN inContainer, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems);
static int32_t populateCgroupEntryListV2(struct OMRPortLibrary *portLibrary, int pid, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems);
static OMRCgroupSubsystem getCgroupSubsystemFromFlag(uint64_t subsystemFlag);
static int32_t getAbsolutePathOfCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, char *fullPath, intptr_t *bufferLength);
static int32_t  getHandleOfCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, FILE **subsystemFile);
static int32_t readCgroupMetricFromFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, const char *metricKeyInFile, char **fileContent, char *value);
static int32_t readCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, int32_t numItemsToRead, const char *format, ...);
static BOOLEAN isRunningInContainer(struct OMRPortLibrary *portLibrary);
static int32_t scanCgroupIntOrMax(struct OMRPortLibrary *portLibrary, const char *metricString, uint64_t *val);
static int32_t readCgroupMemoryFileIntOrMax(struct OMRPortLibrary *portLibrary, const char *fileName, uint64_t *metric);
static int32_t getCgroupMemoryLimit(struct OMRPortLibrary *portLibrary, uint64_t *limit);
static int32_t getCgroupSubsystemMetricMap(struct OMRPortLibrary *portLibrary, uint64_t subsystem, const struct OMRCgroupSubsystemMetricMap **subsystemMetricMap, uint32_t *numElements);
#endif /* defined(LINUX) */

#if defined(LINUX)
static int32_t retrieveLinuxMemoryStatsFromProcFS(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo);
static int32_t retrieveLinuxCgroupMemoryStats(struct OMRPortLibrary *portLibrary, struct OMRCgroupMemoryInfo *cgroupMemInfo);
static int32_t retrieveLinuxMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo);
#elif defined(OSX)
static int32_t retrieveOSXMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo);
#elif defined(AIXPPC)
static int32_t retrieveAIXMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo);
#endif

/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return  the (negative) portable error code
 */
static int32_t
findError(int32_t errorCode)
{
	switch (errorCode) {
	case EACCES:
		/* FALLTHROUGH */
	case EPERM:
		return OMRPORT_ERROR_FILE_NOPERMISSION;
	case EFAULT:
		return OMRPORT_ERROR_FILE_EFAULT;
	case EINVAL:
		return OMRPORT_ERROR_FILE_INVAL;
	case EBADF:
		return OMRPORT_ERROR_FILE_BADF;
	case ENOENT:
		return OMRPORT_ERROR_FILE_NOENT;
	case ENOTDIR:
		return OMRPORT_ERROR_FILE_NOTDIR;
	case ENOMEM:
		return OMRPORT_ERROR_FILE_INSUFFICIENT_BUFFER;
	case EMFILE:
		/* FALLTHROUGH */
	case ENFILE:
		return OMRPORT_ERROR_FILE_TOO_MANY_OPEN_FILES;
	default:
		return OMRPORT_ERROR_FILE_OPFAILED;
	}
}

void
omrsysinfo_set_number_user_specified_CPUs(struct OMRPortLibrary *portLibrary, uintptr_t number)
{
	Trc_PRT_sysinfo_set_number_user_specified_CPUs_Entered();
	portLibrary->portGlobals->userSpecifiedCPUs = number;
	Trc_PRT_sysinfo_set_number_user_specified_CPUs_Exit(number);
}

intptr_t
omrsysinfo_process_exists(struct OMRPortLibrary *portLibrary, uintptr_t pid)
{
	intptr_t rc = 0;
	int result = 0;

	result = kill(pid, 0);
	/*
	 * If sig is 0, then no signal is sent, but error checking is still performed.
	 * By using signal == 0, kill just checks for the existence and accessibility of the process.
	 */

	if (0 == result) {
		rc = 1; /* The process with pid exists and this process has sufficient permissions to send it a signal. */
	} else if (-1 == result) { /* note that kill() returns only 0 or -1 */
		if (errno == ESRCH) {
			/* The kill() failed because was there are no processes or process groups corresponding to pid. */
			rc = 0;
		} else if (errno == EPERM) {
			/* The target process exists but this process does not have permission to send it a signal. */
			rc = 1;
		} else {
			rc = -1;
		}
	}
	return rc;
}

const char *
omrsysinfo_get_CPU_architecture(struct OMRPortLibrary *portLibrary)
{
#if defined(RS6000) || defined(LINUXPPC)
#ifdef PPC64
#ifdef OMR_ENV_LITTLE_ENDIAN
	return OMRPORT_ARCH_PPC64LE;
#else /* OMR_ENV_LITTLE_ENDIAN */
	return OMRPORT_ARCH_PPC64;
#endif /* OMR_ENV_LITTLE_ENDIAN */
#else
	return OMRPORT_ARCH_PPC;
#endif /* PPC64*/
#elif defined(J9X86)
	return OMRPORT_ARCH_X86;
#elif defined(S390) || defined(J9ZOS390)
#if defined(S39064) || defined(J9ZOS39064)
	return OMRPORT_ARCH_S390X;
#else
	return OMRPORT_ARCH_S390;
#endif /* S39064 || J9ZOS39064 */
#elif defined(J9HAMMER)
	return OMRPORT_ARCH_HAMMER;
#elif defined(J9ARM)
	return OMRPORT_ARCH_ARM;
#elif defined(J9AARCH64)
	return OMRPORT_ARCH_AARCH64;
#elif defined(RISCV)
	return OMRPORT_ARCH_RISCV;
#else
	return "unknown";
#endif
}

intptr_t
omrsysinfo_get_processor_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
	intptr_t rc = -1;
	Trc_PRT_sysinfo_get_processor_description_Entered(desc);

	if (NULL != desc) {
		memset(desc, 0, sizeof(OMRProcessorDesc));

#if defined(J9X86) || defined(J9HAMMER)
		rc = omrsysinfo_get_x86_description(portLibrary, desc);
#elif defined(LINUXPPC) /* defined(J9X86) || defined(J9HAMMER) */
		rc = omrsysinfo_get_linux_ppc_description(portLibrary, desc);
#elif defined(AIXPPC) /* defined(LINUXPPC) */
		rc = omrsysinfo_get_aix_ppc_description(portLibrary, desc);
#elif defined(S390) || defined(J9ZOS390) /* defined(AIXPPC) */
		rc = omrsysinfo_get_s390_description(portLibrary, desc);
#elif defined(RISCV) /* defined(S390) || defined(J9ZOS390) */
		rc = omrsysinfo_get_riscv_description(portLibrary, desc);
#elif defined(AARCH64) && defined(LINUX) /* defined(RISCV) */
		rc = omrsysinfo_get_linux_aarch64_description(portLibrary, desc);
#elif defined(AARCH64) && defined(OSX) /* defined(AARCH64) && defined(LINUX) */
		rc = omrsysinfo_get_macos_aarch64_description(portLibrary, desc);
#endif /* defined(J9X86) || defined(J9HAMMER) */
	}

	Trc_PRT_sysinfo_get_processor_description_Exit(rc);
	return rc;
}

BOOLEAN
omrsysinfo_processor_has_feature(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, uint32_t feature)
{
	BOOLEAN rc = FALSE;
	Trc_PRT_sysinfo_processor_has_feature_Entered(desc, feature);

#if defined(J9OS_I5)
#if defined(J9OS_I5_V5R4)
	if ((OMR_FEATURE_PPC_HAS_VSX == feature) || (OMR_FEATURE_PPC_HAS_ALTIVEC == feature) || (OMR_FEATURE_PPC_HTM == feature)) {
		Trc_PRT_sysinfo_processor_has_feature_Exit((UDATA)rc);
		return rc;
	}
#elif defined(J9OS_I5_V6R1) || defined(J9OS_I5_V7R2)
	if (OMR_FEATURE_PPC_HTM == feature) {
		Trc_PRT_sysinfo_processor_has_feature_Exit((UDATA)rc);
		return rc;
	}
#endif
#endif

	if ((NULL != desc) && (feature < (OMRPORT_SYSINFO_FEATURES_SIZE * 32))) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		rc = OMR_ARE_ALL_BITS_SET(desc->features[featureIndex], 1u << featureShift);
	}

	Trc_PRT_sysinfo_processor_has_feature_Exit((uintptr_t)rc);
	return rc;
}

intptr_t
omrsysinfo_processor_set_feature(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, uint32_t feature, BOOLEAN enable)
{
	intptr_t rc = -1;
	Trc_PRT_sysinfo_processor_set_feature_Entered(desc, feature, enable);

	if ((NULL != desc) && (feature < (OMRPORT_SYSINFO_FEATURES_SIZE * 32))) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		if (enable) {
			desc->features[featureIndex] |= (1u << featureShift);
		}
		else {
			desc->features[featureIndex] &= ~(1u << featureShift);
		}
		rc = 0;
	}

	Trc_PRT_sysinfo_processor_set_feature_Exit(rc);
	return rc;
}

const char*
omrsysinfo_get_processor_feature_name(struct OMRPortLibrary *portLibrary, uint32_t feature)
{
	const char* rc = "null";
	Trc_PRT_sysinfo_get_processor_feature_name_Entered(feature);
#if defined(J9X86) || defined(J9HAMMER)
	rc = omrsysinfo_get_x86_processor_feature_name(feature);
#elif defined(S390) || defined(J9ZOS390) || defined(OMRZTPF) /* defined(J9X86) || defined(J9HAMMER) */
	rc = omrsysinfo_get_s390_processor_feature_name(feature);
#elif defined(AIXPPC) || defined(LINUXPPC) /* defined(S390) || defined(J9ZOS390) || defined(OMRZTPF) */
	rc = omrsysinfo_get_ppc_processor_feature_name(feature);
#elif defined(AARCH64) /* defined(AIXPPC) || defined(LINUXPPC) */
	rc = omrsysinfo_get_aarch64_processor_feature_name(feature);
#endif /* defined(J9X86) || defined(J9HAMMER) */
	Trc_PRT_sysinfo_get_processor_feature_name_Exit(rc);
	return rc;
}


/**
 * Generate the corresponding string literals for the provided OMRProcessorDesc. The buffer will be zero
 * initialized and overwritten with the processor feature output string.
 *
 * @param[in] portLibrary The port library.
 * @param[in] desc The struct that contains the list of processor features to be converted to string.
 * @param[out] buffer The processor feature output string.
 * @param[in] length The size of the buffer in number of bytes.
 *
 * @return 0 on success, -1 if output string size exceeds input length.
 */
intptr_t
omrsysinfo_get_processor_feature_string(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, char * buffer, const size_t length)
{
	BOOLEAN start = TRUE;
	size_t i = 0;
	size_t j = 0;
	size_t numberOfBits = 0;
	size_t bufferLength = 0;

	memset(buffer, 0, length);

	for (i = 0; i < OMRPORT_SYSINFO_FEATURES_SIZE; i++) {
		numberOfBits = CHAR_BIT * sizeof(desc->features[i]);
		for (j = 0; j < numberOfBits; j++) {

			if (desc->features[i] & (1 << j)) {
				uint32_t feature = (uint32_t)(i * numberOfBits + j);
				const char * featureName = omrsysinfo_get_processor_feature_name(portLibrary, feature);
				size_t featureLength = strlen(featureName);

				if (start == FALSE) {
					strncat(buffer, " ", length - bufferLength - 1);
					bufferLength += 1;
				} else {
					start = FALSE;
				}

				if (length - bufferLength - 1 < featureLength) {
					return -1;
				}

				strncat(buffer, featureName, length - bufferLength - 1);
				bufferLength += featureLength;
			}
		}
	}
	return 0;
}

#if defined(AIXPPC) || defined(S390) || defined(J9ZOS390) || (defined(AARCH64) && defined(OSX))
/**
 * @internal
 * Helper to set appropriate feature field in a OMRProcessorDesc struct.
 *
 * @param[in] desc pointer to the struct that contains the CPU type and features.
 * @param[in] feature to set
 *
 */
static void
omrsysinfo_set_feature(OMRProcessorDesc *desc, uint32_t feature)
{
	if ((NULL != desc) && (feature < (OMRPORT_SYSINFO_FEATURES_SIZE * 32))) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		desc->features[featureIndex] = (desc->features[featureIndex] | (1u << (featureShift)));
	}
}
#endif /* defined(AIXPPC) || defined(S390) || defined(J9ZOS390) || (defined(AARCH64) && defined(OSX)) */


#if (defined(LINUXPPC) || defined(AIXPPC))

const char *
omrsysinfo_get_ppc_processor_feature_name(uint32_t feature)
{
	switch (feature) {
	case OMR_FEATURE_PPC_32:
		return "mode32";
	case OMR_FEATURE_PPC_64:
		return "mode64";
	case OMR_FEATURE_PPC_601_INSTR:
		return "601";
	case OMR_FEATURE_PPC_HAS_ALTIVEC:
		return "vec";
	case OMR_FEATURE_PPC_HAS_FPU:
		return "fpu";
	case OMR_FEATURE_PPC_HAS_MMU:
		return "mmu";
	case OMR_FEATURE_PPC_HAS_4xxMAC:
		return "4xxmac";
	case OMR_FEATURE_PPC_UNIFIED_CACHE:
		return "uc";
	case OMR_FEATURE_PPC_HAS_SPE:
		return "spe";
	case OMR_FEATURE_PPC_HAS_EFP_SINGLE:
		return "efp_single";
	case OMR_FEATURE_PPC_HAS_EFP_DOUBLE:
		return "efp_double";
	case OMR_FEATURE_PPC_POWER4:
		return "p4";
	case OMR_FEATURE_PPC_POWER5:
		return "p5";
	case OMR_FEATURE_PPC_POWER5_PLUS:
		return "p5+";
	case OMR_FEATURE_PPC_CELL_BE:
		return "cell_be";
	case OMR_FEATURE_PPC_BOOKE:
		return "booke";
	case OMR_FEATURE_PPC_SMT:
		return "smt";
	case OMR_FEATURE_PPC_ICACHE_SNOOP:
		return "icache_snoop";
	case OMR_FEATURE_PPC_ARCH_2_05:
		return "arch_2_05";
	case OMR_FEATURE_PPC_PA6T:
		return "pa6t";
	case OMR_FEATURE_PPC_HAS_DFP:
		return "dfp";
	case OMR_FEATURE_PPC_POWER6_EXT:
		return "p6_ext";
	case OMR_FEATURE_PPC_ARCH_2_06:
		return "arch_2_06";
	case OMR_FEATURE_PPC_HAS_VSX:
		return "vsx";
	case OMR_FEATURE_PPC_PSERIES_PERFMON_COMPAT:
		return "perfmon_compact";
	case OMR_FEATURE_PPC_TRUE_LE:
		return "true_le";
	case OMR_FEATURE_PPC_LE:
		return "le";
	case OMR_FEATURE_PPC_ARCH_2_07:
		return "arch_2_07";
	case OMR_FEATURE_PPC_HTM:
		return "htm";
	case OMR_FEATURE_PPC_DSCR:
		return "dscr";
	case OMR_FEATURE_PPC_EBB:
		return "ebb";
	case OMR_FEATURE_PPC_ISEL:
		return "isel";
	case OMR_FEATURE_PPC_TAR:
		return "tar";
	default:
		return "null";
	}
	return "null";
}

#endif /* defined(LINUXPPC) || defined(AIXPPC) */

#if defined(LINUXPPC)
/**
 * @internal
 * Populates OMRProcessorDesc *desc on Linux PPC
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
omrsysinfo_get_linux_ppc_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
	char* platform = NULL;
	char* base_platform = NULL;

	/* initialize auxv prior to querying the auxv */
	if (prefetch_auxv() != 0) {
		goto _error;
	}

	/* Linux PPC processor */
	platform = (char *) query_auxv(AT_PLATFORM);
	if ((NULL == platform) || (((char *) -1) == platform)) {
		goto _error;
	}
	desc->processor = omrsysinfo_map_ppc_processor(platform);

	/* Linux PPC physical processor */
	base_platform = (char *) query_auxv(AT_BASE_PLATFORM);
	if ((NULL == base_platform) || (((char *) -1) == base_platform)) {
		/* AT_PLATFORM is known from call above.  Default BASE to unknown */
		desc->physicalProcessor = OMR_PROCESSOR_PPC_UNKNOWN;
	} else {
		desc->physicalProcessor = omrsysinfo_map_ppc_processor(base_platform);
	}

	/* Linux PPC features:
	 * Can't error check these calls as both 0 & -1 are valid
	 * bit fields that could be returned by this query.
	 */
	desc->features[0] = query_auxv(AT_HWCAP);
	desc->features[1] = query_auxv(AT_HWCAP2);

	return 0;

_error:
	desc->processor = OMR_PROCESSOR_PPC_UNKNOWN;
	desc->physicalProcessor = OMR_PROCESSOR_PPC_UNKNOWN;
	desc->features[0] = 0;
	desc->features[1] = 0;
	return -1;

}

/**
 * @internal
 * Maps a PPC processor string to the OMRProcessorArchitecture enum.
 *
 * @param[in] processorName
 *
 * @return An OMRProcessorArchitecture OMR_PROCESSOR_PPC_* found otherwise OMR_PROCESSOR_PPC_UNKNOWN.
 */
static OMRProcessorArchitecture
omrsysinfo_map_ppc_processor(const char *processorName)
{
	OMRProcessorArchitecture rc = OMR_PROCESSOR_PPC_UNKNOWN;

	if (0 == strncasecmp(processorName, "ppc403", 6)) {
		rc = OMR_PROCESSOR_PPC_PWR403;
	} else if (0 == strncasecmp(processorName, "ppc405", 6)) {
		rc = OMR_PROCESSOR_PPC_PWR405;
	} else if (0 == strncasecmp(processorName, "ppc440gp", 8)) {
		rc = OMR_PROCESSOR_PPC_PWR440;
	} else if (0 == strncasecmp(processorName, "ppc601", 6)) {
		rc = OMR_PROCESSOR_PPC_PWR601;
	} else if (0 == strncasecmp(processorName, "ppc603", 6)) {
		rc = OMR_PROCESSOR_PPC_PWR603;
	} else if (0 == strncasecmp(processorName, "ppc604", 6)) {
		rc = OMR_PROCESSOR_PPC_PWR604;
	} else if (0 == strncasecmp(processorName, "ppc7400", 7)) {
		rc = OMR_PROCESSOR_PPC_PWR603;
	} else if (0 == strncasecmp(processorName, "ppc750", 6)) {
		rc = OMR_PROCESSOR_PPC_7XX;
	} else if (0 == strncasecmp(processorName, "rs64", 4)) {
		rc = OMR_PROCESSOR_PPC_PULSAR;
	} else if (0 == strncasecmp(processorName, "ppc970", 6)) {
		rc = OMR_PROCESSOR_PPC_GP;
	} else if (0 == strncasecmp(processorName, "power3", 6)) {
		rc = OMR_PROCESSOR_PPC_PWR630;
	} else if (0 == strncasecmp(processorName, "power4", 6)) {
		rc = OMR_PROCESSOR_PPC_GP;
	} else if (0 == strncasecmp(processorName, "power5", 6)) {
		rc = OMR_PROCESSOR_PPC_GR;
	} else if (0 == strncasecmp(processorName, "power6", 6)) {
		rc = OMR_PROCESSOR_PPC_P6;
	} else if (0 == strncasecmp(processorName, "power7", 6)) {
		rc = OMR_PROCESSOR_PPC_P7;
	} else if (0 == strncasecmp(processorName, "power8", 6)) {
		rc = OMR_PROCESSOR_PPC_P8;
	} else if (0 == strncasecmp(processorName, "power9", 6)) {
		rc = OMR_PROCESSOR_PPC_P9;
	} else if (0 == strncasecmp(processorName, "power10", 7)) {
		rc = OMR_PROCESSOR_PPC_P10;
	} else if (0 == strncasecmp(processorName, "power11", 7)) {
		rc = OMR_PROCESSOR_PPC_P11;
	}

	return rc;
}
#endif /* defined(LINUXPPC) */

#if defined(AIXPPC)
/**
 * @internal
 * Populates OMRProcessorDesc *desc on AIX
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
omrsysinfo_get_aix_ppc_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
	/* AIX processor */
	if (__power_rs1() || __power_rsc()) {
		desc->processor = OMR_PROCESSOR_PPC_RIOS1;
	} else if (__power_rs2()) {
		desc->processor = OMR_PROCESSOR_PPC_RIOS2;
	} else if (__power_601()) {
		desc->processor = OMR_PROCESSOR_PPC_PWR601;
	} else if (__power_603()) {
		desc->processor = OMR_PROCESSOR_PPC_PWR603;
	} else if (__power_604()) {
		desc->processor = OMR_PROCESSOR_PPC_PWR604;
	} else if (__power_620()) {
		desc->processor = OMR_PROCESSOR_PPC_PWR620;
	} else if (__power_630()) {
		desc->processor = OMR_PROCESSOR_PPC_PWR630;
	} else if (__power_A35()) {
		desc->processor = OMR_PROCESSOR_PPC_NSTAR;
	} else if (__power_RS64II()) {
		desc->processor = OMR_PROCESSOR_PPC_NSTAR;
	} else if (__power_RS64III()) {
		desc->processor = OMR_PROCESSOR_PPC_PULSAR;
	} else if (__power_4()) {
		desc->processor = OMR_PROCESSOR_PPC_GP;
	} else if (__power_5()) {
		desc->processor = OMR_PROCESSOR_PPC_GR;
	} else if (__power_6()) {
		desc->processor = OMR_PROCESSOR_PPC_P6;
	} else if (__power_7()) {
		desc->processor = OMR_PROCESSOR_PPC_P7;
	} else if (__power_8()) {
		desc->processor = OMR_PROCESSOR_PPC_P8;
	} else if (__power_9()) {
		desc->processor = OMR_PROCESSOR_PPC_P9;
	} else if (__power_10()) {
		desc->processor = OMR_PROCESSOR_PPC_P10;
	} else if (__power_latestKnownAndUp()) {
		desc->processor = OMR_PROCESSOR_PPC_P11;
	} else {
		desc->processor = OMR_PROCESSOR_PPC_UNKNOWN;
	}
#if !defined(J9OS_I5_V6R1)
	/* AIX physical processor */
	if (__phy_proc_imp_4()) {
		desc->physicalProcessor = OMR_PROCESSOR_PPC_GP;
	} else if (__phy_proc_imp_5()) {
		desc->physicalProcessor = OMR_PROCESSOR_PPC_GR;
	} else if (__phy_proc_imp_6()) {
		desc->physicalProcessor = OMR_PROCESSOR_PPC_P6;
	} else if (__phy_proc_imp_7()) {
		desc->physicalProcessor = OMR_PROCESSOR_PPC_P7;
	} else if (__phy_proc_imp_8()) {
		desc->physicalProcessor = OMR_PROCESSOR_PPC_P8;
	} else {
		desc->physicalProcessor = desc->processor;
	}
#else
		desc->physicalProcessor = desc->processor;
#endif /* !defined(J9OS_I5_V6R1) */
	/* AIX Features */
	if (__power_64()) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_PPC_64);
	}
	if (__power_vmx()) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_PPC_HAS_ALTIVEC);
	}
	if (__power_dfp()) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_PPC_HAS_DFP);
	}
	if (__power_vsx()) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_PPC_HAS_VSX);
	}
#if !defined(J9OS_I5_V6R1)
	if (__phy_proc_imp_6()) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_PPC_ARCH_2_05);
	}
	if (__phy_proc_imp_4()) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_PPC_POWER4);
	}
#endif /* !defined(J9OS_I5_V6R1) */
#if !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1)
	if (__power_tm()) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_PPC_HTM);
	}
#endif /* !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1) */

	return 0;
}

#endif /* defined(AIXPPC) */

#if (defined(S390) || defined(J9ZOS390))

#define LAST_DOUBLE_WORD	3

/**
 * @internal
 * Check if a specific bit is set from STFLE instruction on z/OS and zLinux.
 * STORE FACILITY LIST EXTENDED stores a variable number of doublewords containing facility bits.
 *  see z/Architecture Principles of Operation 4-69
 *
 * @param[in] stfleBit bit to check
 *
 * @return TRUE if bit is 1, FALSE otherwise.
 */
static BOOLEAN
omrsysinfo_test_stfle(struct OMRPortLibrary *portLibrary, uint64_t stfleBit)
{
	BOOLEAN rc = FALSE;

	OMRSTFLEFacilities *mem = &(PPG_stfleCache.facilities);
	uintptr_t *stfleRead = &(PPG_stfleCache.lastDoubleWord);

	/* If it is the first time, read stfle and cache it */
	if (0 == *stfleRead) {
		*stfleRead = getstfle(LAST_DOUBLE_WORD, (uint64_t*)mem);
	}

	if (stfleBit < 64 && *stfleRead >= 0) {
		rc = (0 != (mem->dw1 & (((uint64_t)1) << (63 - stfleBit))));
	} else if (stfleBit < 128 && *stfleRead >= 1) {
		rc = (0 != (mem->dw2 & (((uint64_t)1) << (127 - stfleBit))));
	} else if (stfleBit < 192 && *stfleRead >= 2) {
		rc = (0 != (mem->dw3 & (((uint64_t)1) << (191 - stfleBit))));
	} else if (stfleBit < 256 && *stfleRead >= 3) {
		rc = (0 != (mem->dw4 & (((uint64_t)1) << (255 - stfleBit))));
	}

	return rc;
}

#ifdef J9ZOS390
#ifdef _LP64
typedef struct pcb_t
{
	char pcbeye[8];					/* pcbeye = "CEEPCB" */
	char dummy[336];				/* Ignore the rest to get to flag6 field */
	unsigned char ceepcb_flags6;
} pcb_t;
typedef struct ceecaa_t
{
	char dummy[912];				/* pcb is at offset 912 in 64bit */
	pcb_t *pcb_addr;
} ceecaa_t;
#else
typedef struct pcb_t
{
	char pcbeye[8];					/* pcbeye = "CEEPCB" */
	char dummy[76];					/* Ignore the rest to get to flag6 field */
	unsigned char ceepcb_flags6;
} pcb_t;
typedef struct ceecaa_t
{
	char dummy[756];				/* pcb is at offset 756 in 32bit */
	pcb_t *pcb_addr;
} ceecaa_t;
#endif /* ifdef _LP64 */

/** @internal
 *  Check if z/OS supports the Vector Extension Facility (SIMD) by checking whether both the OS and LE support vector
 *  registers. We use the CVTVEF (0x80) bit in the CVT structure for the OS check and bit 0x08 of CEEPCB_FLAG6 field in
 *  the PCB for the LE check.
 *
 *  @return TRUE if VEF is supported; FALSE otherwise.
 */
static BOOLEAN
omrsysinfo_get_s390_zos_supports_vector_extension_facility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG5 is a BITSTRING off the CVT structure containing the CVTVEF (0x80) bit
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG5 = *(CVT + 0x0F4);

	ceecaa_t* CAA = (ceecaa_t *)_gtca();

	if (OMR_ARE_ALL_BITS_SET(CVTFLAG5, 0x80)) {
		if (NULL != CAA) {
			return OMR_ARE_ALL_BITS_SET(CAA->pcb_addr->ceepcb_flags6, 0x08);
		}
	}

	return FALSE;
}

/* The following two functions for checking if z/OS supports the constrained and non-constrained
 * transactional executional facilities rely on FLCCVT and CVTFLAG4, documented here:
 *
 * FLCCVT is an ADDRESS (of the CVT structure) off the PSA structure
 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm
 *
 * CVTFLAG4 is a BITSTRING off the CVT structure containing the CVTTX (0x08), CVTTXC (0x04), and CVTRI (0x02) bits
 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm
 */

/** @internal
 *  Check if z/OS supports the Transactional Execution Facility (TX). We use the CVTTX (0x08) bit in
 *  the CVT structure for the OS check.
 *
 *  @return TRUE if TX is supported; FALSE otherwise.
 */
static BOOLEAN
omrsysinfo_get_s390_zos_supports_transactional_execution_facility(void)
{
	uint8_t* FLCCVT = (uint8_t*)(*(uint32_t*)0x10);
	uint8_t CVTFLAG4 = *(FLCCVT + 0x17B);

	return OMR_ARE_ALL_BITS_SET(CVTFLAG4, 0x08);
}

/** @internal
 *  Check if z/OS supports the Constrained Transactional Execution Facility (TXC). We use the CVTTXC (0x04) bit in
 *  the CVT structure for the OS check.
 *
 *  @return TRUE if TXC is supported; FALSE otherwise.
 */
static BOOLEAN
omrsysinfo_get_s390_zos_supports_constrained_transactional_execution_facility(void)
{
	uint8_t* FLCCVT = (uint8_t*)(*(uint32_t*)0x10);
	uint8_t CVTFLAG4 = *(FLCCVT + 0x17B);

	return OMR_ARE_ALL_BITS_SET(CVTFLAG4, 0x04);
}

/** @internal
 *  Check if z/OS supports the Runtime Instrumentation Facility (RI). We use the CVTRI (0x02) bit in the CVT structure
 *  for the OS check.
 *
 *  @return TRUE if RI is supported; FALSE otherwise.
 */
static BOOLEAN
omrsysinfo_get_s390_zos_supports_runtime_instrumentation_facility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG4 is a BITSTRING off the CVT structure containing the CVTTX (0x08), CVTTXC (0x04), and CVTRI (0x02) bits
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG4 = *(CVT + 0x17B);

	return OMR_ARE_ALL_BITS_SET(CVTFLAG4, 0x02);
}

/** @internal
 *  Check if z/OS supports the Guarded Storage Facility (GS). We use the CVTGSF (0x01) bit in the CVT structure
 *  for the OS check.
 *
 *  @return TRUE if GS is supported; FALSE otherwise.
 */
static BOOLEAN
omrsysinfo_get_s390_zos_supports_guarded_storage_facility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG3 is a BITSTRING off the CVT structure containing the CVTGSF (0x01) bit
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG3 = *(CVT + 0x17A);

	return OMR_ARE_ALL_BITS_SET(CVTFLAG3, 0x01);
}
#endif /* ifdef J9ZOS390 */

/**
 * @internal
 * Populates OMRProcessorDesc *desc on z/OS and zLinux
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
omrsysinfo_get_s390_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
/* Check hardware and OS (z/OS only) support for GS (guarded storage), RI (runtime instrumentation) and TE (transactional memory) */
#if defined(J9ZOS390)
#define S390_STFLE_BIT (0x80000000 >> 7)
	/* s390 feature detection requires the store-facility-list-extended (STFLE) instruction which was introduced in z9
	 * Location 200 is architected such that bit 7 is ON if STFLE instruction is installed */
	if (OMR_ARE_NO_BITS_SET(*(int*) 200, S390_STFLE_BIT)) {
		return -1;
	}
#elif defined(OMRZTPF)  /* defined(J9ZOS390) */
	/*
	 * z/TPF requires OS support for some of the Hardware Capabilities.
	 * Setting the auxvFeatures capabilities flag directly to mimic the query_auxv call in Linux.
	 */
	unsigned long auxvFeatures = OMR_HWCAP_S390_HIGH_GPRS|OMR_FEATURE_S390_ESAN3|OMR_HWCAP_S390_ZARCH|
			OMR_HWCAP_S390_STFLE|OMR_HWCAP_S390_MSA|OMR_HWCAP_S390_DFP|
			OMR_HWCAP_S390_LDISP|OMR_HWCAP_S390_EIMM|OMR_HWCAP_S390_ETF3EH;

#elif defined(LINUX) /* defined(OMRZTPF) */
	/* Some s390 features require OS support on Linux, querying auxv for AT_HWCAP bit-mask of processor capabilities. */
	unsigned long auxvFeatures = query_auxv(AT_HWCAP);
#endif /* defined(LINUX) */

#if (defined(S390) && defined(LINUX))
	/* OS Support of HPAGE on Linux on Z */
	if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_HPAGE)){
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_HPAGE);
	}
#endif /* defined(S390) && defined(LINUX) */

	/* Miscellaneous facility detection */

	if (omrsysinfo_test_stfle(portLibrary, 0)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_ESAN3))
#endif /* defined(S390) && defined(LINUX)*/
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_ESAN3);
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, 2)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_ZARCH))
#endif /* defined(S390) && defined(LINUX)*/
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_ZARCH);
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, 7)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_STFLE))
#endif /* defined(S390) && defined(LINUX)*/
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_STFLE);
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, 17)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_MSA))
#endif /* defined(S390) && defined(LINUX)*/
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MSA);
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, 42) && omrsysinfo_test_stfle(portLibrary, 44)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_DFP))
#endif /* defined(S390) && defined(LINUX) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_DFP);
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, 32)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_COMPARE_AND_SWAP_AND_STORE);
	}

	if (omrsysinfo_test_stfle(portLibrary, 33)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_COMPARE_AND_SWAP_AND_STORE2);
	}

	if (omrsysinfo_test_stfle(portLibrary, 35)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_EXECUTE_EXTENSIONS);
	}

	if (omrsysinfo_test_stfle(portLibrary, 41)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_FPE);
	}

	if (omrsysinfo_test_stfle(portLibrary, 49)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION);
	}

	if (omrsysinfo_test_stfle(portLibrary, 76)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MSA_EXTENSION3);
	}

	if (omrsysinfo_test_stfle(portLibrary, 77)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MSA_EXTENSION4);
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MSA_EXTENSION_5)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MSA_EXTENSION_5);
	}

	/* Assume an unknown processor ID unless we determine otherwise */
	desc->processor = OMR_PROCESSOR_S390_UNKNOWN;

	/* z990 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_LONG_DISPLACEMENT)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_LDISP))
#endif /* defined(S390) && defined(LINUX) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_LONG_DISPLACEMENT);

			desc->processor = OMR_PROCESSOR_S390_Z990;
		}
	}

	/* z9 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_EXTENDED_IMMEDIATE)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_EIMM))
#endif /* defined(S390) && defined(LINUX) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_EXTENDED_IMMEDIATE);
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_EXTENDED_TRANSLATION_3)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_EXTENDED_TRANSLATION_3);
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_ETF3_ENHANCEMENT)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_ETF3EH))
#endif /* defined(S390) && defined(LINUX) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_ETF3_ENHANCEMENT);
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_EXTENDED_IMMEDIATE) &&
		 omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_EXTENDED_TRANSLATION_3) &&
		 omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_ETF3_ENHANCEMENT)) {
		desc->processor = OMR_PROCESSOR_S390_Z9;
	}

	/* z10 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_GENERAL_INSTRUCTIONS_EXTENSIONS)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_GENERAL_INSTRUCTIONS_EXTENSIONS);

		desc->processor = OMR_PROCESSOR_S390_Z10;
	}

	/* z196 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_HIGH_WORD)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_HIGH_GPRS))
#endif /* defined(S390) && defined(LINUX)*/
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_HIGH_WORD);
		}

		desc->processor = OMR_PROCESSOR_S390_Z196;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_1)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_1);

		desc->processor = OMR_PROCESSOR_S390_Z196;
	}

	/* zEC12 facility and processor detection */

	/* TE/TX hardware support */
	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_transactional_execution_facility())
#elif defined(LINUX) /* LINUX S390 */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_TE))
#endif /* defined(J9ZOS390) */
		{
			if (!omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_INEFFECTIVE_NONCONSTRAINED_TRANSACTION_FACILITY))
			{
				omrsysinfo_set_feature(desc, OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY);
			}

			if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY)) {
#if defined(J9ZOS390)
				if (omrsysinfo_get_s390_zos_supports_constrained_transactional_execution_facility())
#endif /* defined(J9ZOS390) */
				{
					omrsysinfo_set_feature(desc, OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY);
				}
			}
		}
	}

	/* RI hardware support */
	if (omrsysinfo_test_stfle(portLibrary, 64)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_runtime_instrumentation_facility())
#endif /* defined(J9ZOS390) */
		{
#if !defined(OMRZTPF)
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_RI);
#endif /* !defined(OMRZTPF) */
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION);

		desc->processor = OMR_PROCESSOR_S390_ZEC12;
	}

	/* z13 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, 129)) {
#if defined(J9ZOS390)
		/* Vector facility requires hardware and OS support */
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) /* LINUX S390 */
		/* Vector facility requires hardware and OS support */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_FACILITY);
			desc->processor = OMR_PROCESSOR_S390_Z13;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_2)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_2);

		desc->processor = OMR_PROCESSOR_S390_Z13;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_LOAD_AND_ZERO_RIGHTMOST_BYTE)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_LOAD_AND_ZERO_RIGHTMOST_BYTE);

		desc->processor = OMR_PROCESSOR_S390_Z13;
	}

	/* z14 facility and processor detection */

	/* GS hardware support */
	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_GUARDED_STORAGE)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_guarded_storage_facility())
#elif defined(LINUX) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_GS))
#endif /* defined(LINUX) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_GUARDED_STORAGE);

			desc->processor = OMR_PROCESSOR_S390_Z14;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2);

		desc->processor = OMR_PROCESSOR_S390_Z14;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_SEMAPHORE_ASSIST)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_SEMAPHORE_ASSIST);

		desc->processor = OMR_PROCESSOR_S390_Z14;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL)) {
#if defined(J9ZOS390)
		/* Vector packed decimal requires hardware and OS support (for OS, checking for VEF is sufficient) */
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif (defined(S390) && defined(LINUX)) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS_BCD))
#endif /* defined(S390) && defined(LINUX) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL);

			desc->processor = OMR_PROCESSOR_S390_Z14;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1)) {
#if defined(J9ZOS390)
		/* Vector facility enhancement 1 requires hardware and OS support (for OS, checking for VEF is sufficient) */
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif (defined(S390) && defined(LINUX)) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS_EXT))
#endif /* defined(S390) && defined(LINUX) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1);

			desc->processor = OMR_PROCESSOR_S390_Z14;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MSA_EXTENSION_8)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MSA_EXTENSION_8);

		desc->processor = OMR_PROCESSOR_S390_Z14;
	}

    /* z15 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3);

		desc->processor = OMR_PROCESSOR_S390_Z15;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) && !defined(OMRZTPF) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(OMRZTPF) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2);

			desc->processor = OMR_PROCESSOR_S390_Z15;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) && !defined(OMRZTPF) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(OMRZTPF) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY);

			desc->processor = OMR_PROCESSOR_S390_Z15;
		}
	}

   /* z16 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_2)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) && !defined(OMRZTPF) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(OMRZTPF) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_2);

			desc->processor = OMR_PROCESSOR_S390_Z16;
		}
	}

   /* zNext facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_4)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_4);

		desc->processor = OMR_PROCESSOR_S390_ZNEXT;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_3)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) && !defined(J9ZTPF) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(J9ZTPF) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_3);

			desc->processor = OMR_PROCESSOR_S390_ZNEXT;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_PLO_EXTENSION)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_PLO_EXTENSION);

		desc->processor = OMR_PROCESSOR_S390_ZNEXT;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_3)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) && !defined(J9ZTPF) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(J9ZTPF) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_3);

			desc->processor = OMR_PROCESSOR_S390_ZNEXT;
		}
	}

	/* Set Side Effect Facility without setting GP12. This is because
	 * this GP12-only STFLE bit can also be enabled on zEC12 (GP10)
	 */
	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_SIDE_EFFECT_ACCESS)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_SIDE_EFFECT_ACCESS);
	}

	desc->physicalProcessor = desc->processor;

	return 0;
}

const char *
omrsysinfo_get_s390_processor_feature_name(uint32_t feature)
{
	switch (feature) {
	case OMR_FEATURE_S390_ESAN3:
		return "esan3";
	case OMR_FEATURE_S390_ZARCH:
		return "zarch";
	case OMR_FEATURE_S390_STFLE:
		return "stfle";
	case OMR_FEATURE_S390_MSA:
		return "msa";
	case OMR_FEATURE_S390_DFP:
		return "dfp";
	case OMR_FEATURE_S390_HPAGE:
		return "hpage";
	case OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY:
		return "tx";
	case OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY:
		return "txc";
	case OMR_FEATURE_S390_MSA_EXTENSION3:
		return "msa_e3";
	case OMR_FEATURE_S390_MSA_EXTENSION4:
		return "msa_e4";
	case OMR_FEATURE_S390_COMPARE_AND_SWAP_AND_STORE:
		return "css";
	case OMR_FEATURE_S390_COMPARE_AND_SWAP_AND_STORE2:
		return "css2";
	case OMR_FEATURE_S390_EXECUTE_EXTENSIONS:
		return "ee";
	case OMR_FEATURE_S390_FPE:
		return "fpe";
	case OMR_FEATURE_S390_RI:
		return "ri";
	case OMR_FEATURE_S390_LONG_DISPLACEMENT:
		return "ld";
	case OMR_FEATURE_S390_EXTENDED_IMMEDIATE:
		return "ei";
	case OMR_FEATURE_S390_EXTENDED_TRANSLATION_3:
		return "et3";
	case OMR_FEATURE_S390_ETF3_ENHANCEMENT:
		return "etf3";
	case OMR_FEATURE_S390_GENERAL_INSTRUCTIONS_EXTENSIONS:
		return "gi";
	case OMR_FEATURE_S390_HIGH_WORD:
		return "hw";
	case OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION:
		return "mi";
	case OMR_FEATURE_S390_LOAD_AND_ZERO_RIGHTMOST_BYTE:
		return "lzrb";
	case OMR_FEATURE_S390_VECTOR_FACILITY:
		return "vec";
	case OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2:
		return "mi_e2";
	case OMR_FEATURE_S390_SEMAPHORE_ASSIST:
		return "sema";
	case OMR_FEATURE_S390_SIDE_EFFECT_ACCESS:
		return "sea";
	case OMR_FEATURE_S390_GUARDED_STORAGE:
		return "gs";
	case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL:
		return "vec_pd";
	case OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1:
		return "vec_e1";
	case OMR_FEATURE_S390_MSA_EXTENSION_8:
		return "msa_e8";
	case OMR_FEATURE_S390_MSA_EXTENSION_5:
		return "msa_e5";
	case OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3:
		return "mi_e3";
	case OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2:
		return "vec_e2";
	case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY:
		return "vec_pde";
	case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_2:
		return "vec_pde2";
	case OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_4:
		return "mi_e4";
	case OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_3:
		return "vec_e4";
	case OMR_FEATURE_S390_PLO_EXTENSION:
		return "plo";
	case OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_3:
		return "vec_pde3";
	default:
		return "null";
	}
	return "null";
}

#endif /* defined(S390) || defined(J9ZOS390) */

#if defined(RISCV)
static intptr_t
omrsysinfo_get_riscv_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
#if defined(RISCV32)
	desc->processor = OMR_PROCESSOR_RISCV32_UNKNOWN;
#elif defined(RISCV64)
	desc->processor = OMR_PROCESSOR_RISCV64_UNKNOWN;
#elif
	desc->processor = OMR_PROCESSOR_UNDEFINED;
#endif
	desc->physicalProcessor = desc->processor;
	return 0;
}
#endif /* defined(RISCV) */

#if defined(AARCH64)
static const char *
omrsysinfo_get_aarch64_processor_feature_name(uint32_t feature)
{
	switch (feature) {
	case OMR_FEATURE_ARM64_FP:
		return "fp";
	case OMR_FEATURE_ARM64_ASIMD:
		return "asimd";
	case OMR_FEATURE_ARM64_EVTSTRM:
		return "evtstrm";
	case OMR_FEATURE_ARM64_AES:
		return "aes";
	case OMR_FEATURE_ARM64_PMULL:
		return "pmull";
	case OMR_FEATURE_ARM64_SHA1:
		return "sha1";
	case OMR_FEATURE_ARM64_SHA256:
		return "sha2";
	case OMR_FEATURE_ARM64_CRC32:
		return "crc32";
	case OMR_FEATURE_ARM64_LSE:
		return "atomics";
	case OMR_FEATURE_ARM64_FP16:
		return "fphp";
	case OMR_FEATURE_ARM64_ASIMDHP:
		return "asimdhp";
	case OMR_FEATURE_ARM64_CPUID:
		return "cpuid";
	case OMR_FEATURE_ARM64_RDM:
		return "asimdrdm";
	case OMR_FEATURE_ARM64_JSCVT:
		return "jscvt";
	case OMR_FEATURE_ARM64_FCMA:
		return "fcma";
	case OMR_FEATURE_ARM64_LRCPC:
		return "lrcpc";
	case OMR_FEATURE_ARM64_DPB:
		return "dcpop";
	case OMR_FEATURE_ARM64_SHA3:
		return "sha3";
	case OMR_FEATURE_ARM64_SM3:
		return "sm3";
	case OMR_FEATURE_ARM64_SM4:
		return "sm4";
	case OMR_FEATURE_ARM64_DOTPROD:
		return "asimddp"; /* Advanced SIMD Dot Product */
	case OMR_FEATURE_ARM64_SHA512:
		return "sha512";
	case OMR_FEATURE_ARM64_SVE:
		return "sve";
	case OMR_FEATURE_ARM64_FHM:
		return "asimdfhm";
	case OMR_FEATURE_ARM64_DIT:
		return "dit";
	case OMR_FEATURE_ARM64_LSE2:
		return "uscat"; /* unaligned single copy atomicity */
	case OMR_FEATURE_ARM64_LRCPC2:
		return "ilrcpc"; /* immediate variants of LRCPC */
	case OMR_FEATURE_ARM64_FLAGM:
		return "flagm";
	case OMR_FEATURE_ARM64_SSBS:
		return "ssbs";
	case OMR_FEATURE_ARM64_SB:
		return "sb";
	case OMR_FEATURE_ARM64_PAUTH:
		return "paca"; /* pointer authentication code to the address */
	case OMR_FEATURE_ARM64_PACG:
		return "pacg"; /* pointer authentication code generic */
	case OMR_FEATURE_ARM64_DPB2:
		return "dcpodp"; /* data cache point of deep persistence */
	case OMR_FEATURE_ARM64_SVE2:
		return "sve2";
	case OMR_FEATURE_ARM64_SVE_AES:
		return "sveaes";
	case OMR_FEATURE_ARM64_SVE_PMULL128:
		return "svepmull";
	case OMR_FEATURE_ARM64_SVE_BITPERM:
		return "svebitperm";
	case OMR_FEATURE_ARM64_SVE_SHA3:
		return "svesha3";
	case OMR_FEATURE_ARM64_SVE_SM4:
		return "svesm4";
	case OMR_FEATURE_ARM64_FLAGM2:
		return "flagm2";
	case OMR_FEATURE_ARM64_FRINTTS:
		return "frint";
	case OMR_FEATURE_ARM64_SVE_I8MM:
		return "svei8mm";
	case OMR_FEATURE_ARM64_F32MM:
		return "svef32mm";
	case OMR_FEATURE_ARM64_F64MM:
		return "svef64mm";
	case OMR_FEATURE_ARM64_SVE_BF16:
		return "svebf16";
	case OMR_FEATURE_ARM64_I8MM:
		return "i8mm";
	case OMR_FEATURE_ARM64_BF16:
		return "bf16";
	case OMR_FEATURE_ARM64_DGH:
		return "dgh";
	case OMR_FEATURE_ARM64_RNG:
		return "rng";
	case OMR_FEATURE_ARM64_BTI:
		return "bti";
	case OMR_FEATURE_ARM64_MTE2:
		return "mte";
	default:
		return "null";
	}
	return "null";
}

#if defined(LINUX)
/**
 * @internal
 * Populates OMRProcessorDesc *desc on Linux ARM64
 *
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
omrsysinfo_get_linux_aarch64_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
	/* initialize auxv prior to querying the auxv */
	if (prefetch_auxv() != 0) {
		desc->processor = OMR_PROCESSOR_ARM64_V8_A;
		desc->physicalProcessor = OMR_PROCESSOR_ARM64_V8_A;
		desc->features[0] = 0;
		desc->features[1] = 0;
		return -1;
	}

	desc->processor = OMR_PROCESSOR_ARM64_V8_A;
	desc->physicalProcessor = desc->processor;
	/* Linux ARM64 features:
	 * Can't error check these calls as both 0 & -1 are valid
	 * bit fields that could be returned by this query.
	 */
	desc->features[0] = query_auxv(AT_HWCAP);
	desc->features[1] = query_auxv(AT_HWCAP2);

	return 0;
}
#elif defined(OSX) /* defined(LINUX) */
/**
 * @internal
 * Populates OMRProcessorDesc *desc on macOS ARM64.
 *
 * @param[in] portLibrary The port library.
 * @param[in] desc pointer to the struct that will contain the CPU type and features.
 *
 * @return 0 on success.
 */
static intptr_t
omrsysinfo_get_macos_aarch64_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
	int64_t val = 0;
	size_t size = sizeof(val);

	desc->processor = OMR_PROCESSOR_ARM64_V8_A;
	desc->physicalProcessor = desc->processor;
	desc->features[0] = 0;
	desc->features[1] = 0;

	/*
	 * These features are standard on Apple Silicon
	 * No need to check
	 */
	omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_FP);
	omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_ASIMD);

	/*
	 * Advanced SIMD and Floating Point Capabilities
	 */
	if ((0 == sysctlbyname("hw.optional.arm.FEAT_BF16", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_BF16);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_DotProd", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_DOTPROD);
	}

	/* Attribute name changed in macOS 12. */
	if (((0 == sysctlbyname("hw.optional.arm.FEAT_FCMA", &val, &size, NULL, 0))
	|| (0 == sysctlbyname("hw.optional.armv8_3_compnum", &val, &size, NULL, 0)))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_FCMA);
	}

	/* Attribute name changed in macOS 12. */
	if (((0 == sysctlbyname("hw.optional.arm.FEAT_FHM", &val, &size, NULL, 0))
	|| (0 == sysctlbyname("hw.optional.armv8_2_fhm", &val, &size, NULL, 0)))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_FHM);
	}

	/* Attribute name changed in macOS 12. */
	if (((0 == sysctlbyname("hw.optional.arm.FEAT_FP16", &val, &size, NULL, 0))
	|| (0 == sysctlbyname("hw.optional.neon_fp16", &val, &size, NULL, 0)))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_FP16);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_FRINTTS", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_FRINTTS);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_I8MM", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_I8MM);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_JSCVT", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_JSCVT);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_RDM", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_RDM);
	}

	/*
	 * Integer Capabilities
	 */
	if ((0 == sysctlbyname("hw.optional.arm.FEAT_FlagM", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_FLAGM);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_FlagM2", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_FLAGM2);
	}

	if ((0 == sysctlbyname("hw.optional.armv8_crc32", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_CRC32);
	}

	/*
	 * Atomic and Memory Ordering Instruction Capabilities
	 */
	if ((0 == sysctlbyname("hw.optional.arm.FEAT_LRCPC", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_LRCPC);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_LRCPC2", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_LRCPC2);
	}

	/* Attribute name changed in macOS 12. */
	if (((0 == sysctlbyname("hw.optional.arm.FEAT_LSE", &val, &size, NULL, 0))
	|| (0 == sysctlbyname("hw.optional.armv8_1_atomics", &val, &size, NULL, 0)))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_LSE);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_LSE2", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_LSE2);
	}

	/*
	 * Encryption Capabilities
	 */
	if ((0 == sysctlbyname("hw.optional.arm.FEAT_AES", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_AES);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_PMULL", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_PMULL);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_SHA1", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_SHA1);
	}

	if ((0 == sysctlbyname("hw.optional.arm.FEAT_SHA256", &val, &size, NULL, 0))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_SHA256);
	}

	/* Attribute name changed in macOS 12. */
	if (((0 == sysctlbyname("hw.optional.arm.FEAT_SHA512", &val, &size, NULL, 0))
	|| (0 == sysctlbyname("hw.optional.armv8_2_sha512", &val, &size, NULL, 0)))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_SHA512);
	}

	/* Attribute name changed in macOS 12. */
	if (((0 == sysctlbyname("hw.optional.arm.FEAT_SHA3", &val, &size, NULL, 0))
	|| (0 == sysctlbyname("hw.optional.armv8_2_sha3", &val, &size, NULL, 0)))
	&& (0 != val)
	) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_ARM64_SHA3);
	}

	return 0;
}
#endif /* defined(LINUX) */
#endif /* defined(AARCH64) */

intptr_t
omrsysinfo_get_env(struct OMRPortLibrary *portLibrary, const char *envVar, char *infoString, uintptr_t bufSize)
{
	char *value = (char *)getenv(envVar);
	uintptr_t len = 0;

	if (NULL == value) {
		return -1;
	} else if ((len = strlen(value)) >= bufSize) {
		return len + 1;
	} else {
		strcpy(infoString, value);
		return 0;
	}
}

const char *
omrsysinfo_get_OS_type(struct OMRPortLibrary *portLibrary)
{
#if defined(J9OS_I5)
	/* JCL on IBM i expects to see "OS/400" */
	return "OS/400";
#elif defined(OSX)
	return "Mac OS X";
#else
	if (NULL == PPG_si_osType) {
		int rc = 0;
		struct utsname sysinfo;

#ifdef J9ZOS390
		rc = __osname(&sysinfo);
#else
		rc = uname(&sysinfo);
#endif
		if (rc >= 0) {
			int len = strlen(sysinfo.sysname);
			char *buffer = portLibrary->mem_allocate_memory(portLibrary, len + 1, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == buffer) {
				return NULL;
			}
			/* copy and NUL-terminate */
			memcpy(buffer, sysinfo.sysname, len);
			buffer[len] = '\0';
			PPG_si_osType = buffer;
		}
	}
	return PPG_si_osType;
#endif
}

#if defined(OSX)
#define COMPAT_PLIST_LINK "/System/Library/CoreServices/.SystemVersionPlatform.plist"
#define KERN_OSPRODUCTVERSION "kern.osproductversion"
#define PLIST_FILE "/System/Library/CoreServices/SystemVersion.plist"
#define PRODUCT_VERSION_ELEMENT "<key>ProductVersion</key>"
#define STRING_OPEN_TAG "<string>"
#define STRING_CLOSE_TAG "</string>"
#endif

#if defined(J9OS_I5)
 /* 'V', 'R', 'M', '0', & null */
#define RELEASE_OVERHEAD 5
#define FORMAT_STRING "V%sR%sM0"
#else
/* "." and terminating null character */
#define RELEASE_OVERHEAD 2
#define FORMAT_STRING "%s.%s"
#endif /* defined(J9OS_I5) */

const char *
omrsysinfo_get_OS_version(struct OMRPortLibrary *portLibrary)
{
	if (NULL == PPG_si_osVersion) {
		int64_t rc = -1;
#if defined(OSX)
		char *versionString = NULL;
		char *allocatedFileBuffer = NULL;
#else
		struct utsname sysinfo;
#endif

#ifdef J9ZOS390
		rc = __osname(&sysinfo);
#elif defined(OSX)
		/* uname() on MacOS returns the version of the Darwin kernel, not the operating system product name.
		 * For compatibility with the reference implementation,
		 * we need the operating system product name.
		 * On newer versions of MacOS, the OS name is provided by sysctlbyname().
		 * On older versions of MacOS, the OS name is provided by a property list file.
		 * The relevant portion is:
		 *      <key>ProductVersion</key>
		 *      <string>10.13.6</string>
		 */
		size_t resultSize = 0;
		rc = sysctlbyname(KERN_OSPRODUCTVERSION, NULL, &resultSize, NULL, 0);
		if (rc < 0) { /* older version of MacOS.  Fall back to parsing the plist. */
			intptr_t plistFile = portLibrary->file_open(portLibrary, PLIST_FILE, EsOpenRead, 0444);
			if (plistFile >= 0) {
				char myBuffer[1024];  /* the file size is typically ~500 bytes */
				char *buffer = NULL;
				size_t fileLen = 0;
				rc = portLibrary->file_flength(portLibrary, plistFile);
				if (rc >= 0) {
					fileLen = (size_t) rc;
					if (fileLen >= sizeof(myBuffer)) { /* leave space for a terminating null */
						allocatedFileBuffer = portLibrary->mem_allocate_memory(portLibrary, fileLen + 1,
							OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
						buffer = allocatedFileBuffer;
					} else {
						buffer = myBuffer;
					}
				}
				rc = 0;
				if (NULL != buffer) {
					char *readPtr = buffer;
					size_t bytesRemaining = fileLen;
					while (bytesRemaining > 0) {
						intptr_t result = portLibrary->file_read(portLibrary, plistFile, readPtr, bytesRemaining);
						if (result > 0) {
							bytesRemaining -= (size_t)result;
							readPtr += result;
						} else {
							rc = -1; /* error or unexpected EOF */
							break;
						}
					}
					if (0 == rc) {
						buffer[fileLen] = '\0';
						readPtr = strstr(buffer, PRODUCT_VERSION_ELEMENT);
						if (NULL != readPtr) {
							readPtr = strstr(readPtr + sizeof(PRODUCT_VERSION_ELEMENT) - 1, STRING_OPEN_TAG);
						}
						if (NULL != readPtr) {
							versionString = readPtr + sizeof(STRING_OPEN_TAG) - 1;
							readPtr = strstr(versionString, STRING_CLOSE_TAG);
						}
						if (NULL != readPtr) {
							*readPtr = '\0';
							resultSize = readPtr - versionString;
						} else {
							rc = -1; /* parse error */
						}
					}
				}
				portLibrary->file_close(portLibrary, plistFile);
				if (rc < 0) {
					/* free the buffer if it was allocated */
					portLibrary->mem_free_memory(portLibrary, allocatedFileBuffer);
					allocatedFileBuffer = NULL;
				}
			}
		}
#else
		rc = uname(&sysinfo);
#endif /* defined(OSX) */

		if (rc >= 0) {
			int len;
			char *buffer = NULL;
#if defined(RS6000) || defined(J9ZOS390)
			len = strlen(sysinfo.version) + strlen(sysinfo.release) + RELEASE_OVERHEAD;
			buffer = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL != buffer) {
				portLibrary->str_printf(portLibrary, buffer, len, FORMAT_STRING, sysinfo.version, sysinfo.release);
			}
#elif defined(OSX)
			len = resultSize + 1;
			buffer = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL != buffer) {
				if (NULL != versionString) {
					strncpy(buffer, versionString, len);
				} else {
					rc = sysctlbyname(KERN_OSPRODUCTVERSION, buffer, &resultSize, NULL, 0);
				}
				buffer[resultSize] = '\0';

				/* 10.16 is the version returned when in a backwards compatibility context in OSX versions 11+
				 * so we check the plink symlink as that bypasses the compatibility context and returns the
				 * real version info.
				 */
				if (0 == strcmp(buffer, "10.16")) {
					portLibrary->mem_free_memory(portLibrary, buffer);
					buffer = NULL;
					len = 0;
					portLibrary->mem_free_memory(portLibrary, allocatedFileBuffer);
					allocatedFileBuffer = NULL;

					intptr_t plistFile = portLibrary->file_open(portLibrary, COMPAT_PLIST_LINK, EsOpenRead, 0444);
					if (plistFile >= 0) {
						char myBuffer[1024];  /* the file size is ~500 bytes, 525 on a 11.3 */
						char *fileBuffer = myBuffer;
						size_t fileLen = 0;
						rc = portLibrary->file_flength(portLibrary, plistFile);
						if (rc > 0) {
							fileLen = (size_t) rc;
							if (fileLen >= sizeof(myBuffer)) { /* leave space for a terminating null */
								allocatedFileBuffer = portLibrary->mem_allocate_memory(portLibrary, fileLen + 1,
									OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
								fileBuffer = allocatedFileBuffer;
							} else {
								fileBuffer = myBuffer;
							}
						}

						rc = 0;
						if (NULL != fileBuffer) {
							char *readPtr = fileBuffer;
							size_t bytesRemaining = fileLen;
							while (bytesRemaining > 0) {
								intptr_t result = portLibrary->file_read(portLibrary, plistFile, readPtr, bytesRemaining);
								if (result > 0) {
									bytesRemaining -= (size_t)result;
									readPtr += result;
								} else {
									rc = -1; /* error or unexpected EOF */
									break;
								}
							}
							if (0 == rc) {
								fileBuffer[fileLen] = '\0';
								readPtr = strstr(fileBuffer, PRODUCT_VERSION_ELEMENT);
								if (NULL != readPtr) {
									readPtr = strstr(readPtr + sizeof(PRODUCT_VERSION_ELEMENT) - 1, STRING_OPEN_TAG);
								}
								if (NULL != readPtr) {
									versionString = readPtr + sizeof(STRING_OPEN_TAG) - 1;
									readPtr = strstr(versionString, STRING_CLOSE_TAG);
								}
								if (NULL != readPtr) {
									resultSize = readPtr - versionString;
									len = resultSize + 1;
									buffer = portLibrary->mem_allocate_memory(portLibrary, len,
										OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
									if (NULL != buffer) {
										memcpy(buffer, versionString, resultSize);
										buffer[resultSize] = '\0';
									}
								}
							}
						}
						portLibrary->file_close(portLibrary, plistFile);
					}
				}
			}
			/* allocatedFileBuffer is null if it was not allocated */
			portLibrary->mem_free_memory(portLibrary, allocatedFileBuffer);
#else
			len = strlen(sysinfo.release) + 1;
			buffer = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL != buffer) {
				strncpy(buffer, sysinfo.release, len);
				buffer[len - 1] = '\0';
			}
#endif /* RS6000 */
			PPG_si_osVersion = buffer;
		}
	}
	return PPG_si_osVersion;
}

uintptr_t
omrsysinfo_get_pid(struct OMRPortLibrary *portLibrary)
{
	return getpid();
}

uintptr_t
omrsysinfo_get_ppid(struct OMRPortLibrary *portLibrary)
{
	return getppid();
}

uintptr_t
omrsysinfo_get_euid(struct OMRPortLibrary *portLibrary)
{
	uintptr_t uid = geteuid();
	Trc_PRT_sysinfo_get_egid(uid);
	return uid;
}

uintptr_t
omrsysinfo_get_egid(struct OMRPortLibrary *portLibrary)
{
	uintptr_t gid = getegid();
	Trc_PRT_sysinfo_get_egid(gid);
	return gid;
}

intptr_t
omrsysinfo_get_groups(struct OMRPortLibrary *portLibrary, uint32_t **gidList, uint32_t categoryCode)
{
	intptr_t size = 0;
	gid_t *list = NULL;
	intptr_t myerror;

	Trc_PRT_sysinfo_get_groups_Entry();

	*gidList = NULL;
	/* First get the number of supplementary group IDs */
	size = getgroups(0, NULL);
	if (-1 == size) {
		/* Above call getgroups(0, ...) is not expected to fail. In case it does, return appropriate error code */
		myerror = errno;
		Trc_PRT_sysinfo_get_groups_Error_GetGroupsSize(myerror);
		setPortableError(portLibrary, getgroupsErrorMsgPrefix, OMRPORT_ERROR_SYSINFO_GETGROUPSSIZE_ERROR, myerror);
	} else {
		list = portLibrary->mem_allocate_memory(portLibrary, size * sizeof(uint32_t), OMR_GET_CALLSITE(), categoryCode);
		if (NULL == list) {
			Trc_PRT_sysinfo_get_groups_Error_ListAllocateFailed(size);
			size = -1;
		} else {
			size = getgroups(size, list);
			if (-1 == size) {
				myerror = errno;
				portLibrary->mem_free_memory(portLibrary, list);
				Trc_PRT_sysinfo_get_groups_Error_GetGroups(myerror);
				setPortableError(portLibrary, getgroupsErrorMsgPrefix, OMRPORT_ERROR_SYSINFO_GETGROUPS_ERROR, myerror);
			} else {
				*gidList = (uint32_t *)list;
			}
		}
	}
	Trc_PRT_sysinfo_get_groups_Exit(size, *gidList);
	return size;
}

/**
 * @internal Helper routine that determines the full path of the current executable
 * that launched the JVM instance.
 */
static intptr_t
find_executable_name(struct OMRPortLibrary *portLibrary, char **result)
{
#if defined(LINUX)
	intptr_t retval = readSymbolicLink(portLibrary, "/proc/self/exe", result);
	if (NULL != *result) {
		/* The code in this block is a work around for a linux bug. If /proc/self/exe is read after a
		 * call to fstat on a NFSv4 share, then the string ' (deleted)' will be appended to
		 * the file name.
		 *
		 * The work around to this problem for now is to prune off the ' (deleted)' string.
		 *
		 * Until this fix numerous 'pltest.exe' failures were occurring on 'vmfarm'. I expect over time
		 * once kernels are patch this code can be removed.
		 */
		int exeNameLen = strlen(*result);
		const char *deletedStr = " (deleted)";
		int deletedStrLen = strlen(deletedStr);

		if (exeNameLen > deletedStrLen) {
			if (strncmp(&((*result)[exeNameLen - deletedStrLen]), deletedStr, deletedStrLen) == 0) {
				(*result)[exeNameLen - deletedStrLen] = '\0';
			}
		}
	}
	return retval;
#elif defined(OSX)
	intptr_t rc = -1;
	uint32_t size = 0;

	/* First call is just to get required size. */;
	_NSGetExecutablePath(*result, &size);

	*result = portLibrary->mem_allocate_memory(portLibrary, size, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (0 == _NSGetExecutablePath(*result, &size)) {
		rc = 0;
	}
	return rc;
#elif defined(OMR_OS_BSD)
	int proc[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	size_t size = 0;
	*result = NULL;
	if (-1 == sysctl(proc, sizeof(proc) / sizeof(proc[0]), NULL, &size, NULL, 0)) {
		return -1;
	}

	*result = portLibrary->mem_allocate_memory(portLibrary, size, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == *result) {
		return -1;
	}

	if (-1 == sysctl(proc, sizeof(proc) / sizeof(proc[0]), *result, &size, NULL, 0)) {
		portLibrary->mem_free_memory(portLibrary, *result);
		*result = NULL;
		return -1;
	}
	return 0;
#else /* defined(OMR_OS_BSD) */

	intptr_t retval = -1;
	intptr_t length;
	char *p;
	char *currentName = NULL;
	char *currentPath = NULL;
	char *originalWorkingDirectory = NULL;
#ifdef J9ZOS390
	char *e2aName = NULL;
	int token = 0;
	W_PSPROC buf;
	pid_t mypid = getpid();

	memset(&buf, 0x00, sizeof(buf));
	buf.ps_pathptr   = portLibrary->mem_allocate_memory(portLibrary, buf.ps_pathlen = PS_PATHBLEN, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (buf.ps_pathptr   == NULL) {
		retval = -1;
		goto cleanup;
	}
	while ((token = w_getpsent(token, &buf, sizeof(buf))) > 0) {
		if (buf.ps_pid == mypid) {
#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
			e2aName = e2a_func(buf.ps_pathptr, strlen(buf.ps_pathptr) + 1);
#else /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */
			e2aName = buf.ps_pathptr;
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */
			break;
		}
	}

	portLibrary->mem_free_memory(portLibrary, buf.ps_pathptr);

	/* Return val of w_getpsent == -1 indicates error, == 0 indicates no more processes */
	if (token <= 0) {
		retval = -1;
		goto cleanup;
	}
	currentPath = (portLibrary->mem_allocate_memory)(portLibrary, strlen(e2aName) + 1, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (currentPath) {
		strcpy(currentPath, e2aName);
	}
#if defined(J9ZOS390) && !defined(OMR_EBCDIC)
	free(e2aName);
#endif /* defined(J9ZOS390) && !defined(OMR_EBCDIC) */
#else
	char *execName = NULL;

#if defined(AIXPPC)
	/* On AIX, "/proc" provides a way of determining argument vector (not just using
	 * argv[0] from within main()).
	 */
	struct psinfo psinfoData = {0};
	char buffer[PATH_MAX]; /* Do not initialize; large array. */
	char ***pArgv = NULL;
	int32_t fd = -1;
	uintptr_t readBytes = 0L;
	uintptr_t remainingBytes = sizeof(psinfoData);
	uintptr_t readOffset = 0L;
	uintptr_t strLen = 0L;
	int32_t portableError = 0;

	portLibrary->str_printf(portLibrary, buffer, sizeof(buffer), "/proc/%ld/psinfo", getpid());
	fd = portLibrary->file_open(portLibrary, buffer, EsOpenRead, 0);
	if (-1 == fd) {
		portableError = portLibrary->error_last_error_number(portLibrary);
		Trc_PRT_find_executable_name_failedOpeningProcFS(portableError);
		portLibrary->error_set_last_error_with_message(portLibrary,
				portableError,
				"Failed to open /proc fs");
		retval = -1;
		goto cleanup;
	}

	/* Read the process info; loop in order to ensure the complete structure is read in. */
	do {
		readBytes = portLibrary->file_read(portLibrary,
										   fd,
										   ((char *) &psinfoData) + readOffset,
										   remainingBytes);
		/* Advance readOffset by bytes read and adjust this out from remainingBytes (to be read). */
		remainingBytes -= readBytes;
		readOffset += readBytes;

		/* Check for error and EOF conditions.  Should this arise before reading ample bytes, file read
		 * has supposedly failed.
		 */
		if (-1 == readBytes) {
			/* Read failed because of some reason.  Check the portable error code. */
			portableError = portLibrary->error_last_error_number(portLibrary);
			Trc_PRT_find_executable_name_invalidRead(portableError);
			portLibrary->error_set_last_error_with_message(portLibrary, portableError, "Input/output error");
			retval = -1;
			portLibrary->file_close(portLibrary, fd);
			goto cleanup;
		} else if ((0 == readBytes) && (0 != remainingBytes)) {
			/* EOF reached but haven't read required number bytes (file simply too short)!  No error number
			 * is expected to be set here, so use OMRPORT_ERROR_FILE_IO to indicate I/O error.
			 */
			Trc_PRT_find_executable_name_invalidRead(OMRPORT_ERROR_FILE_IO);
			portLibrary->error_set_last_error_with_message(portLibrary,
					OMRPORT_ERROR_FILE_IO,
					"Input/output error: /proc file truncated");
			retval = -1;
			portLibrary->file_close(portLibrary, fd);
			goto cleanup;
		}
	} while (0 != remainingBytes); /* Read until there are no bytes remaining to fill the structure. */

	portLibrary->file_close(portLibrary, fd);

	pArgv = (char ***)psinfoData.pr_argv;
	execName = pArgv[0][0];
#endif

	strLen = strlen(execName) + 1;
	currentPath = (portLibrary->mem_allocate_memory)(portLibrary, strLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL != currentPath) {
		strncpy(currentPath, execName, strLen - 1);
		currentPath[strLen - 1] = '\0';
	}
#endif

	if (NULL == currentPath) {
		retval = -1;
		goto cleanup;
	}

	retval = cwdname(portLibrary, &originalWorkingDirectory);
	if (retval) {
		retval = -1;
		goto cleanup;
	}

gotPathName:

	/* split path into directory part and filename part. */

	p = strrchr(currentPath, '/');
	if (p) {
		*p++ = '\0';
		currentName = (portLibrary->mem_allocate_memory)(portLibrary, strlen(p) + 1, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (!currentName) {
			retval = -1;
			goto cleanup;
		}
		strcpy(currentName, p);
	} else {
		currentName = currentPath;
		currentPath = NULL;
		retval = searchSystemPath(portLibrary, currentName, &currentPath);
		if (retval) {
			retval = -1;
			goto cleanup;
		}
	}
	/* go there */
	if (currentPath) {
		if (currentPath[0]) {
			if (0 != chdir(currentPath)) {
				retval = -1;
				goto cleanup;
			}
		}
		(portLibrary->mem_free_memory)(portLibrary, currentPath);
		currentPath = NULL;
	}
	if (isSymbolicLink(portLibrary, currentName)) {
		/* try to follow the link. */
		retval = readSymbolicLink(portLibrary, currentName, &currentPath);
#ifdef DEBUG
		(portLibrary->tty_printf)(portLibrary, "read cp=%s\n", currentPath);
#endif
		if (retval) {
			retval = -1;
			goto cleanup;
		}
		(portLibrary->mem_free_memory)(portLibrary, currentName);
		currentName = NULL;
		goto gotPathName;
	}

	retval = cwdname(portLibrary, &currentPath);
	if (retval) {
		retval = -1;
		goto cleanup;
	}

	/* Put name and path back together */

	*result = (portLibrary->mem_allocate_memory)(portLibrary, strlen(currentPath) + strlen(currentName) + 2, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (!*result) {
		retval = -1;
		goto cleanup;
	}
	strcpy(*result, currentPath);
	if (currentPath[0] && (currentPath[strlen(currentPath) - 1] != '/')) {
		strcat(*result, "/");
	}
	strcat(*result, currentName);

	/* Finished. */
	retval = 0;

cleanup:

	if (originalWorkingDirectory) {
		chdir(originalWorkingDirectory);
		(portLibrary->mem_free_memory)(portLibrary, originalWorkingDirectory);
		originalWorkingDirectory = NULL;
	}

	if (currentPath) {
		(portLibrary->mem_free_memory)(portLibrary, currentPath);
		currentPath = NULL;
	}

	if (currentName) {
		(portLibrary->mem_free_memory)(portLibrary, currentName);
		currentName = NULL;
	}

	return retval;
#endif /* defined(OSX) */
}

/**
 * Determines an absolute pathname for the executable.
 *
 * @param[in] portLibrary The port library.
 * @param[in] argv0 argv[0] value
 * @param[out] result Null terminated pathname string
 *
 * @return 0 on success, -1 on error (or information is not available).
 *
 * @note Caller should /not/ de-allocate memory in the result buffer, as string containing
 * the executable name is system-owned (managed internally by the port library).
 */
intptr_t
omrsysinfo_get_executable_name(struct OMRPortLibrary *portLibrary, const char *argv0, char **result)
{
	(void) argv0; /* @args used */

	/* Clear any pending error conditions. */
	portLibrary->error_set_last_error(portLibrary, 0, 0);

	if (PPG_si_executableName) {
		*result = PPG_si_executableName;
		return 0;
	}
	*result = NULL;
	return (intptr_t)-1;
}

/**
 * @internal  Returns the current working directory.
 *
 * @return 0 on success, -1 on failure.
 *
 * @note The buffer to hold this string (including its terminating NUL) is allocated with
 * portLibrary->mem_allocate_memory.  The caller should free this memory with
 * portLibrary->mem_free_memory when it is no longer needed.
 */
static intptr_t
cwdname(struct OMRPortLibrary *portLibrary, char **result)
{
	char *cwd;
	int allocSize = 256;

doAlloc:
	cwd = (portLibrary->mem_allocate_memory)(portLibrary, allocSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (!cwd)  {
		return -1;
	}
	if (!getcwd(cwd, allocSize - 1))  {
		int32_t error = errno; /* Save the error code before re-trying with bigger buffer. */
		(portLibrary->mem_free_memory)(portLibrary, cwd);
		if (ERANGE == error) {
			allocSize += 256;
			goto doAlloc;
		}
		return -1;
	}
	*result = cwd;
	return 0;
}

#if defined(AIXPPC) || defined(J9ZOS390)
/**
 * @internal  Examines the named file to determine if it is a symbolic link.  On platforms which don't have
 * symbolic links (or where we can't tell) or if an unexpected error occurs, just answer FALSE.
 */
static BOOLEAN
isSymbolicLink(struct OMRPortLibrary *portLibrary, char *filename)
{
	struct stat statbuf;
#ifdef DEBUG
	portLibrary->tty_printf(portLibrary, "isSymbolicLink(\"%s\")\n", filename);
#endif
	if (!lstat(filename, &statbuf)) {
		if (S_ISLNK(statbuf.st_mode)) {
#ifdef DEBUG
			portLibrary->tty_printf(portLibrary, "TRUE\n");
#endif
			return TRUE;
		}
	}
#ifdef DEBUG
	portLibrary->tty_printf(portLibrary, "FALSE\n");
#endif

	return FALSE;
}
#endif /* defined(AIXPPC) || defined(J9ZOS390) */

/**
 * @internal  Attempts to read the contents of a symbolic link.  (The contents are the relative pathname of
 * the thing linked to).  A buffer large enough to hold the result (and the terminating NUL) is
 * allocated with portLibrary->mem_allocate_memory.  The caller should free this buffer with
 * portLibrary->mem_free_memory when it is no longer needed.
 * On success, returns 0.  On error, returns -1.
 */
#if defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390)
static intptr_t
readSymbolicLink(struct OMRPortLibrary *portLibrary, char *linkFilename, char **result)
{
#if defined(LINUX) || defined(AIXPPC)
	char fixedBuffer[PATH_MAX + 1];
	int size = readlink(linkFilename, fixedBuffer, sizeof(fixedBuffer) - 1);
#ifdef DEBUG
	portLibrary->tty_printf(portLibrary, "readSymbolicLink: \"%s\"\n%i\n", linkFilename, size);
#endif
	if (size <= 0) {
		return -1;
	}
	fixedBuffer[size++] = '\0';
	*result = (portLibrary->mem_allocate_memory)(portLibrary, size, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (!*result) {
		return -1;
	}
	strcpy(*result, fixedBuffer);
	return 0;

#else /* defined(LINUX) || defined(AIXPPC) */
	return -1;
#endif /* defined(LINUX) || defined(AIXPPC) */
}
#endif /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) */

#if defined(AIXPPC) || defined(J9ZOS390)
/**
 * @internal  Searches through the system PATH for the named file.  If found, it returns the path entry
 * which matched the file.  A buffer large enough to hold the proper path entry (without a
 * trailing slash, but with the terminating NUL) is allocated with portLibrary->mem_allocate_memory.
 * The caller should free this buffer with portLibrary->mem_free_memory when it is no longer
 * needed.  On success, returns 0.  On error (including if the file is not found), -1 is returned.
 */
static intptr_t
searchSystemPath(struct OMRPortLibrary *portLibrary, char *filename, char **result)
{
	char *pathCurrent;
	char *pathNext;
	int length;
	DIR *sdir = NULL;
	struct dirent *dirEntry;
	/* This should be sufficient for a single entry in the PATH var, though the var itself */
	/* could be considerably longer.. */
	char temp[PATH_MAX + 1];

	/* We use getenv() instead of the portLibrary function because getenv() doesn't require */
	/* us to allocate any memory, or guess how much to allocate. */
	if (!(pathNext = getenv("PATH"))) {
		return -1;
	}

	while (pathNext) {
		pathCurrent = pathNext;
		pathNext = strchr(pathCurrent, ':');
		if (pathNext)  {
			length = (pathNext - pathCurrent);
			pathNext += 1;
		} else  {
			length = strlen(pathCurrent);
		}
		if (length > PATH_MAX) {
			length = PATH_MAX;
		}
		memcpy(temp, pathCurrent, length);
		temp[length] = '\0';
#ifdef DEBUG
		(portLibrary->tty_printf)(portLibrary, "Searching path: \"%s\"\n", temp);
#endif
		if (!length) { /* empty path entry */
			continue;
		}
		if ((sdir = opendir(temp))) {
			while ((dirEntry = readdir(sdir))) {
#ifdef DEBUG
				(portLibrary->tty_printf)(portLibrary, "dirent: \"%s\"\n", dirEntry->d_name);
#endif
				if (!strcmp(dirEntry->d_name, filename)) {
					closedir(sdir);
					/* found! */
					*result = (portLibrary->mem_allocate_memory)(portLibrary, strlen(temp) + 1, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
					if (!result) {
						return -1;
					}
					strcpy(*result, temp);
					return 0;
				}
			}
			closedir(sdir);
		}
	}
	/* not found */
	return -1;
}

#endif /* defined(AIXPPC) || defined(J9ZOS390) */

uintptr_t
omrsysinfo_get_number_CPUs_by_type(struct OMRPortLibrary *portLibrary, uintptr_t type)
{
	uintptr_t toReturn = 0;

	Trc_PRT_sysinfo_get_number_CPUs_by_type_Entered();

	switch (type) {
	case OMRPORT_CPU_PHYSICAL:
#if (defined(LINUX) && !defined(OMRZTPF)) || defined(AIXPPC) || defined(OSX)
		toReturn = sysconf(_SC_NPROCESSORS_CONF);

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedPhysical("errno: ", errno);
		}
#elif defined(J9ZOS390)
		/* Temporarily using ONLINE for PHYSICAL on z/OS */
		toReturn = portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_ONLINE);

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedPhysical("(no errno) ", 0);
		}
#elif defined(OMRZTPF)
		toReturn = get_IPL_IstreamCount();
		if (0 == toReturn) {
						Trc_PRT_sysinfo_get_number_CPUs_by_type_failedPhysical("(no errno) ", 0);
				}
#else
		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_platformNotSupported();
		}
#endif
		break;

	case OMRPORT_CPU_ONLINE:
#ifdef RS6000
		toReturn = _system_configuration.ncpus;

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedOnline("(no errno) ", 0);
		}
#elif (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX)
		/* returns number of online(_SC_NPROCESSORS_ONLN) processors, number configured(_SC_NPROCESSORS_CONF) may  be more than online */
		toReturn = sysconf(_SC_NPROCESSORS_ONLN);

		if (0 >= toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedOnline("errno: ", errno);
		}
#elif defined(OMRZTPF)
		/* USE PHYSICAL for ONLINE on z/TPF */
		toReturn = portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_PHYSICAL);

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedOnline("(no errno) ", 0);
		}
#elif defined(J9ZOS390)
		toReturn = Get_Number_Of_CPUs();

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedOnline("(no errno) ", 0);
		}
#else
		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_platformNotSupported();
		}
#endif
		break;

	case OMRPORT_CPU_BOUND: {
#if defined(J9OS_I5)
		toReturn = 0;
		Trc_PRT_sysinfo_get_number_CPUs_by_type_invalidType();
#elif defined(AIXPPC)
		rsid_t who = { .at_tid = thread_self() };
		rsethandle_t rset = rs_alloc(RS_EMPTY);
		ra_getrset(R_THREAD, who, 0, rset);
		toReturn = rs_getinfo(rset, R_NUMPROCS, 0);
		rs_free(rset);

		if (0 >= toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedBound("errno: ", errno);
		}
#elif defined(LINUX) && !defined(OMRZTPF)
		cpu_set_t cpuSet;
		int32_t error = 0;
		size_t size = sizeof(cpuSet); /* Size in bytes */
		pid_t mainProcess = getpid();
		memset(&cpuSet, 0, size);

		error = sched_getaffinity(mainProcess, size, &cpuSet);

		if (0 == error) {
			toReturn = CPU_COUNT(&cpuSet);
		} else {
			toReturn = 0;
			if (EINVAL == errno) {
				/* Too many CPUs for the fixed cpu_set_t structure */
				int32_t numCPUs = sysconf(_SC_NPROCESSORS_CONF);
				cpu_set_t *allocatedCpuSet = CPU_ALLOC(numCPUs);
				if (NULL != allocatedCpuSet) {
					size = CPU_ALLOC_SIZE(numCPUs);
					CPU_ZERO_S(size, allocatedCpuSet);
					error = sched_getaffinity(mainProcess, size, allocatedCpuSet);
					if (0 == error) {
						toReturn = CPU_COUNT_S(size, allocatedCpuSet);
					}
					CPU_FREE(allocatedCpuSet);
				}
			}
		}

		/* If system calls failed to retrieve info, do not check cgroup at all, and give an error */
		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedBound("errno: ", errno);
		} else if (portLibrary->sysinfo_cgroup_are_subsystems_enabled(portLibrary, OMR_CGROUP_SUBSYSTEM_CPU)) {
			int32_t rc = 0;
			int64_t cpuQuota = 0;
			uint64_t cpuPeriod = 0;

			if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
				/* cpu.cfs_quota_us and cpu.cfs_period_us files each contain only one integer value. */
				int32_t numItemsToRead = 1;

				/* If either file read fails, ignore cgroup cpu quota limits and continue. */
				rc = readCgroupSubsystemFile(
						portLibrary,
						OMR_CGROUP_SUBSYSTEM_CPU,
						CGROUP_CPU_CFS_QUOTA_US_FILE,
						numItemsToRead,
						"%ld",
						&cpuQuota);
				if (0 == rc) {
					rc = readCgroupSubsystemFile(
							portLibrary,
							OMR_CGROUP_SUBSYSTEM_CPU,
							CGROUP_CPU_CFS_PERIOD_US_FILE,
							numItemsToRead,
							"%lu",
							&cpuPeriod);
				}
			} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
				/* Read cpu.max file which contains the quota and period in the format
				 * "$QUOTA $PERIOD". The value "max" for $QUOTA indicates no limit.
				 */
				int32_t numItemsToRead = 2;
				char quotaString[MAX_64BIT_INT_LENGTH];
				uint64_t quotaVal = 0;

				rc = readCgroupSubsystemFile(
						portLibrary,
						OMR_CGROUP_SUBSYSTEM_CPU,
						CGROUP_CPU_MAX_FILE,
						numItemsToRead,
						"%s %lu",
						&quotaString,
						&cpuPeriod);
				if (0 != rc) {
					Trc_PRT_sysinfo_get_number_CPUs_by_type_read_failed(CGROUP_CPU_MAX_FILE, rc);
				} else {
					rc = scanCgroupIntOrMax(portLibrary, quotaString, &quotaVal);
					if (0 == rc) {
						if (UINT64_MAX == quotaVal) {
							cpuQuota = -1;
						} else {
							cpuQuota = (int64_t)quotaVal;
						}
					}
				}
			} else {
				Trc_PRT_Assert_ShouldNeverHappen();
			}

			if (0 == rc) {
				/* numCpusQuota is calculated from the cpu quota time allocated per cpu period. */
				int32_t numCpusQuota = (int32_t)(((double)cpuQuota / cpuPeriod) + 0.5);

				/* Overwrite toReturn if cgroup cpu quota is set (cpuQuota is not -1/unlimited)
				 * or if numCpusQuota will limit the number of cpus available from the number
				 * of physical cpus.
				 */
				if ((cpuQuota > 0) && (numCpusQuota < toReturn)) {
					toReturn = numCpusQuota;
					/* If the CPU quota rounds down to 0, then just return 1 as the closest usable value. */
					if (0 == toReturn) {
						toReturn = 1;
					}
				}
			}
		}
#elif defined(OMRZTPF)
		toReturn = get_Dispatch_IstreamCount();
		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedBound("(no errno) ", 0);
		}
#elif defined(OSX)
		/* OS X does not export interfaces that identify processors or control thread
		 * placement--explicit thread to processor binding is not supported. Thus, this
		 * just returns number of CPUs.
		 */
		toReturn = sysconf(_SC_NPROCESSORS_CONF);
		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedBound("errno: ", errno);
		}
#elif defined(J9ZOS390)
		toReturn = Get_Number_Of_CPUs();

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedBound("(no errno) ", 0);
		}
#else
		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_platformNotSupported();
		}
#endif
		break;
	}
	case OMRPORT_CPU_TARGET: {
		uintptr_t specified = portLibrary->portGlobals->userSpecifiedCPUs;
		if (0 < specified) {
			toReturn = specified;
		} else {
#if defined(J9OS_I5)
			toReturn = _system_configuration.ncpus;
			if (0 == toReturn) {
				Trc_PRT_sysinfo_get_number_CPUs_by_type_failedOnline("(no errno) ", 0);
			}
#elif defined(OMRZTPF)
			toReturn = get_Dispatch_IstreamCount();
#else /* defined(J9OS_I5) */
			toReturn = portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_BOUND);
#endif /* defined(J9OS_I5) */
		}
		break;
	}
	default:
		toReturn = 0;
		Trc_PRT_sysinfo_get_number_CPUs_by_type_invalidType();
		break;
	}

	Trc_PRT_sysinfo_get_number_CPUs_by_type_Exit(type, toReturn);

	return toReturn;
}

#define MAX_LINE_LENGTH     128

/* Memory units are specified in Kilobytes. */
#define ONE_K               1024

/* Base for the decimal number system is 10. */
#define COMPUTATION_BASE    10

#if defined(LINUX)
#define MEMSTATPATH         "/proc/meminfo"

#define MEMTOTAL_PREFIX     "MemTotal:"
#define MEMTOTAL_PREFIX_SZ  (sizeof(MEMTOTAL_PREFIX) - 1)

#define MEMFREE_PREFIX      "MemFree:"
#define MEMFREE_PREFIX_SZ   (sizeof(MEMFREE_PREFIX) - 1)

#define SWAPTOTAL_PREFIX    "SwapTotal:"
#define SWAPTOTAL_PREFIX_SZ (sizeof(SWAPTOTAL_PREFIX) - 1)

#define SWAPFREE_PREFIX     "SwapFree:"
#define SWAPFREE_PREFIX_SZ  (sizeof(SWAPFREE_PREFIX) - 1)

#define CACHED_PREFIX       "Cached:"
#define CACHED_PREFIX_SZ    (sizeof(CACHED_PREFIX) - 1)

#define BUFFERS_PREFIX      "Buffers:"
#define BUFFERS_PREFIX_SZ   (sizeof(BUFFERS_PREFIX) - 1)

#define PROC_SYS_VM_SWAPPINESS "/proc/sys/vm/swappiness"

/**
 * Function collects memory usage statistics by reading /proc/meminfo on Linux platforms.
 *
 * @param[in] portLibrary The port library.
 * @param[in] memInfo A pointer to the J9MemoryInfo struct which we populate with memory usage.
 *
 * @return 0 on success and negative error code on failure.
 */
static int32_t
retrieveLinuxMemoryStatsFromProcFS(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo)
{
	int32_t rc = 0;
	int32_t rcSwappiness = 0;
	FILE *memStatFs = NULL;
	FILE *swappinessFs = NULL;
	char lineString[MAX_LINE_LENGTH] = {0};

	/* Open the memstat file on Linux for reading; this is readonly. */
	memStatFs = fopen(MEMSTATPATH, "r");
	if (NULL == memStatFs) {
		Trc_PRT_retrieveLinuxMemoryStats_failedOpeningMemFs(errno);
		rc = OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
		goto _cleanup;
	}

	Trc_PRT_retrieveLinuxMemoryStats_openedMemFs();

	while (0 == feof(memStatFs)) {
		char *endPtr = NULL;
		char *tmpPtr = NULL;

		/* read a line from MEMSTATPATH. */
		if (NULL == fgets(lineString, MAX_LINE_LENGTH, memStatFs)) {
			break;
		}
		tmpPtr = (char *)lineString;

		/* Get the various fields that we are interested in and extract the corresponding value. */
		if (0 == strncmp(tmpPtr, MEMTOTAL_PREFIX, MEMTOTAL_PREFIX_SZ)) {
			/* Now obtain the MemTotal value. */
			tmpPtr += MEMTOTAL_PREFIX_SZ;
			memInfo->totalPhysical = strtol(tmpPtr, &endPtr, COMPUTATION_BASE);
			if ((LONG_MIN == memInfo->totalPhysical) || (LONG_MAX == memInfo->totalPhysical)) {
				Trc_PRT_retrieveLinuxMemoryStats_invalidRangeFound("MemTotal");
				rc = (ERANGE == errno)
						? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE
						: OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
				goto _cleanup;
			} /* end outer-if */

			/* The values are in Kb. */
			memInfo->totalPhysical *= ONE_K;
		} else if (0 == strncmp(tmpPtr, MEMFREE_PREFIX, MEMFREE_PREFIX_SZ)) {
			/* Skip the whitespaces until we hit the MemFree value. */
			tmpPtr += MEMFREE_PREFIX_SZ;
			memInfo->availPhysical = strtol(tmpPtr, &endPtr, COMPUTATION_BASE);
			if ((LONG_MIN == memInfo->availPhysical) || (LONG_MAX == memInfo->availPhysical)) {
				Trc_PRT_retrieveLinuxMemoryStats_invalidRangeFound("MemFree");
				rc = (ERANGE == errno)
						? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE
						: OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
				goto _cleanup;
			} /* end outer-if */

			/* The values are in Kb. */
			memInfo->availPhysical *= ONE_K;
		} else if (0 == strncmp(tmpPtr, SWAPTOTAL_PREFIX, SWAPTOTAL_PREFIX_SZ)) {
			/* Skip the whitespaces until we hit the SwapTotal value. */
			tmpPtr += SWAPTOTAL_PREFIX_SZ;
			memInfo->totalSwap = strtol(tmpPtr, &endPtr, COMPUTATION_BASE);
			if ((LONG_MIN == memInfo->totalSwap) || (LONG_MAX == memInfo->totalSwap)) {
				Trc_PRT_retrieveLinuxMemoryStats_invalidRangeFound("SwapTotal");
				rc = (ERANGE == errno)
						? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE
						: OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
				goto _cleanup;
			} /* end outer-if */

			/* The values are in Kb. */
			memInfo->totalSwap *= ONE_K;
		} else if (0 == strncmp(tmpPtr, SWAPFREE_PREFIX, SWAPFREE_PREFIX_SZ)) {
			/* Skip the whitespaces until we get to the SwapFree value. */
			tmpPtr += SWAPFREE_PREFIX_SZ;
			memInfo->availSwap = strtol(tmpPtr, &endPtr, COMPUTATION_BASE);
			if ((LONG_MIN == memInfo->availSwap) || (LONG_MAX == memInfo->availSwap)) {
				Trc_PRT_retrieveLinuxMemoryStats_invalidRangeFound("SwapFree");
				rc = (ERANGE == errno)
						? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE
						: OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
				goto _cleanup;
			} /* end outer-if */

			/* The values are in Kb. */
			memInfo->availSwap *= ONE_K;
		} else if (0 == strncmp(tmpPtr, CACHED_PREFIX, CACHED_PREFIX_SZ)) {
			/* Skip the whitespaces until we hit the Cached area size. */
			tmpPtr += CACHED_PREFIX_SZ;
			memInfo->cached = strtol(tmpPtr, &endPtr, COMPUTATION_BASE);
			if ((LONG_MIN == memInfo->cached) || (LONG_MAX == memInfo->cached)) {
				Trc_PRT_retrieveLinuxMemoryStats_invalidRangeFound("Cached");
				rc = (ERANGE == errno)
						? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE
						: OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
				goto _cleanup;
			} /* end outer-if */

			/* The values are in Kb. */
			memInfo->cached *= ONE_K;
		} else if (0 == strncmp(tmpPtr, BUFFERS_PREFIX, BUFFERS_PREFIX_SZ)) {
			/* Skip the whitespaces until we hit the Buffered memory area size. */
			tmpPtr += BUFFERS_PREFIX_SZ;
			memInfo->buffered = strtol(tmpPtr, &endPtr, COMPUTATION_BASE);
			if ((LONG_MIN == memInfo->buffered) || (LONG_MAX == memInfo->buffered)) {
				Trc_PRT_retrieveLinuxMemoryStats_invalidRangeFound("Buffers");
				rc = (ERANGE == errno)
						? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE
						: OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
				goto _cleanup;
			} /* end outer-if */

			/* The values are in Kb. */
			memInfo->buffered *= ONE_K;
		} /* end if else-if */
	} /* end while() */

	swappinessFs = fopen(PROC_SYS_VM_SWAPPINESS, "r");
	if (NULL == swappinessFs) {
		Trc_PRT_retrieveLinuxMemoryStats_failedOpeningSwappinessFs(errno);
		rc = OMRPORT_ERROR_SYSINFO_ERROR_SWAPPINESS_OPEN_FAILED;
		goto _cleanup;
	}

	rcSwappiness = fscanf(swappinessFs, "%" SCNu64, &memInfo->swappiness);
	if (1 != rcSwappiness) {
		if (EOF == rcSwappiness) {
			Trc_PRT_retrieveLinuxMemoryStats_failedReadingSwappiness(errno);
		} else {
			Trc_PRT_retrieveLinuxMemoryStats_unexpectedSwappinessFormat(1, rcSwappiness);
		}
		rc = OMRPORT_ERROR_SYSINFO_ERROR_READING_SWAPPINESS;
		goto _cleanup;
	}

	/* Set hostXXX fields with memory stats from proc fs.
	 * These may be used for calculating available physical memory on the host.
	 */
	memInfo->hostAvailPhysical = memInfo->availPhysical;
	memInfo->hostCached = memInfo->cached;
	memInfo->hostBuffered = memInfo->buffered;

_cleanup:
	if (NULL != memStatFs) {
		fclose(memStatFs);
	}
	if (NULL != swappinessFs) {
		fclose(swappinessFs);
	}

	return rc;
}

#if !defined(OMRZTPF)
/**
 * Function collects memory usage statistics from the memory subsystem of the process's cgroup.
 *
 * @param[in] portLibrary The port library.
 * @param[in] cgroupMemInfo A pointer to the OMRCgroupMemoryInfo struct which will be populated with memory usage.
 *
 * @return 0 on success and negative error code on failure.
 */
static int32_t
retrieveLinuxCgroupMemoryStats(struct OMRPortLibrary *portLibrary, struct OMRCgroupMemoryInfo *cgroupMemInfo)
{
	int32_t rc = 0;
	FILE *memStatFs = NULL;
	int32_t numItemsToRead = 1;
	const char *memLimitFile = NULL;
	const char *memUsageFile = NULL;
	const char *swpLimitFile = NULL;
	const char *swpUsageFile = NULL;
	const char *cacheMetricName = NULL;
	size_t cacheMetricSize = 0;

	Assert_PRT_true(NULL != cgroupMemInfo);

	cgroupMemInfo->memoryLimit = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->memoryUsage = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->memoryAndSwapLimit = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->memoryAndSwapUsage = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->swappiness = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->cached = OMRPORT_MEMINFO_NOT_AVAILABLE;

	if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
		memLimitFile = CGROUP_MEMORY_LIMIT_IN_BYTES_FILE;
		memUsageFile = CGROUP_MEMORY_USAGE_IN_BYTES_FILE;
		swpLimitFile = CGROUP_MEMORY_SWAP_LIMIT_IN_BYTES_FILE;
		swpUsageFile = CGROUP_MEMORY_SWAP_USAGE_IN_BYTES_FILE;
		cacheMetricName = CGROUP_MEMORY_STAT_CACHE_METRIC;
		cacheMetricSize = CGROUP_MEMORY_STAT_CACHE_METRIC_SZ;
	} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
		memLimitFile = CGROUP_MEMORY_MAX_FILE;
		memUsageFile = CGROUP_MEMORY_CURRENT_FILE;
		swpLimitFile = CGROUP_MEMORY_SWAP_MAX_FILE;
		swpUsageFile = CGROUP_MEMORY_SWAP_CURRENT_FILE;
		cacheMetricName = CGROUP_MEMORY_STAT_FILE_METRIC;
		cacheMetricSize = CGROUP_MEMORY_STAT_FILE_METRIC_SZ;
	} else {
		Trc_PRT_Assert_ShouldNeverHappen();
	}

	rc = readCgroupMemoryFileIntOrMax(portLibrary, memLimitFile, &cgroupMemInfo->memoryLimit);
	if (0 != rc) {
		goto _exit;
	}
	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, memUsageFile, numItemsToRead, "%lu", &cgroupMemInfo->memoryUsage);
	if (0 != rc) {
		goto _exit;
	}
	rc = readCgroupMemoryFileIntOrMax(portLibrary, swpLimitFile, &cgroupMemInfo->memoryAndSwapLimit);
	if (0 != rc) {
		if (OMRPORT_ERROR_FILE_NOENT == rc) {
			/* It is possible that the swpLimitFile is not present if swap space is not configured.
			 * In such cases, set memoryAndSwapLimit to memoryLimit.
			 */
			cgroupMemInfo->memoryAndSwapLimit = cgroupMemInfo->memoryLimit;
			rc = 0;
		} else {
			goto _exit;
		}
	} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
		/* Cgroup v2 swap limit and usage do not include the memory limit and usage. */
		cgroupMemInfo->memoryAndSwapLimit += cgroupMemInfo->memoryLimit;
	}
	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, swpUsageFile, numItemsToRead, "%lu", &cgroupMemInfo->memoryAndSwapUsage);
	if (0 != rc) {
		if (OMRPORT_ERROR_FILE_NOENT == rc) {
			/* It is possible that the swpUsageFile is not present if swap space is not configured.
			 * In such cases, set memoryAndSwapUsage to memoryUsage.
			 */
			cgroupMemInfo->memoryAndSwapUsage = cgroupMemInfo->memoryUsage;
			rc = 0;
		} else {
			goto _exit;
		}
	} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
		/* Cgroup v2 swap limit and usage do not include the memory limit and usage. */
		cgroupMemInfo->memoryAndSwapUsage += cgroupMemInfo->memoryUsage;
	}

	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, CGROUP_MEMORY_SWAPPINESS, numItemsToRead, "%" SCNu64, &cgroupMemInfo->swappiness);
	if (0 != rc) {
		cgroupMemInfo->swappiness = OMRPORT_MEMINFO_NOT_AVAILABLE;
		/* Cgroup swappiness may not always be available (e.g. Cgroup v2),
		 * so do not treat this condition as an error
		 */
		rc = 0;
	}

	/* Read value of page cache memory from memory.stat file */
	rc = getHandleOfCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, CGROUP_MEMORY_STAT_FILE, &memStatFs);
	if (0 != rc) {
		goto _exit;
	}

	Assert_PRT_true(NULL != memStatFs);

	while (0 == feof(memStatFs)) {
		char statEntry[MAX_LINE_LENGTH] = {0};
		char *tmpPtr = NULL;

		if (NULL == fgets((char *)statEntry, MAX_LINE_LENGTH, memStatFs)) {
			break;
		}
		tmpPtr = (char *)statEntry;

		/* Extract "cache" value */
		if (0 == strncmp(tmpPtr, cacheMetricName, cacheMetricSize)) {
			tmpPtr += cacheMetricSize;
			rc = sscanf(tmpPtr, "%" SCNu64, &cgroupMemInfo->cached);
			if (1 != rc) {
				Trc_PRT_retrieveLinuxCgroupMemoryStats_invalidValue(cacheMetricName, CGROUP_MEMORY_STAT_FILE);
				rc = portLibrary->error_set_last_error_with_message_format(
						portLibrary,
						OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_FILE_INVALID_VALUE,
						"invalid value for field %s in file %s",
						cacheMetricName,
						CGROUP_MEMORY_STAT_FILE);
			} else {
				/* reset 'rc' to success code */
				rc = 0;
			}
			break;
		}
	}

_exit:
	if (NULL != memStatFs) {
		fclose(memStatFs);
	}

	return rc;
}

#endif /* !defined(OMRZTPF) */

/**
 * Function collects memory usage statistics on Linux platforms and returns the same.
 * This function takes into account cgroup limits if available.
 *
 * @param[in] portLibrary The port library.
 * @param[in] memInfo A pointer to the J9MemoryInfo struct which we populate with memory usage.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
retrieveLinuxMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo)
{
	int32_t rc = 0;
	uint64_t maxVirtual = 0;
#if !defined(OMRZTPF)
	struct OMRCgroupMemoryInfo cgroupMemInfo = {0};
	BOOLEAN isCgroupMemUsageValid = FALSE;
	BOOLEAN isCgroupMemAndSwapLimitValid = FALSE;
	uint64_t swapLimit = 0;
#endif /* !defined(OMRZTPF) */
	Trc_PRT_retrieveLinuxMemoryStats_Entered();

	rc = portLibrary->sysinfo_get_limit(portLibrary, OMRPORT_RESOURCE_ADDRESS_SPACE | OMRPORT_LIMIT_HARD, &maxVirtual);
	if ((OMRPORT_LIMIT_UNKNOWN != rc) && (RLIM_INFINITY != maxVirtual)) {
		memInfo->totalVirtual = maxVirtual;
	}

	rc = retrieveLinuxMemoryStatsFromProcFS(portLibrary, memInfo);
	if (0 != rc) {
		goto _exit;
	}

#if !defined(OMRZTPF)
	if (OMRPORT_MEMINFO_NOT_AVAILABLE == memInfo->totalPhysical) {
		goto _exit;
	}

	/* If cgroup memory subsystem is enabled, retrieve memory limits of the cgroup */
	if (!portLibrary->sysinfo_cgroup_are_subsystems_enabled(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY)) {
		goto _exit;
	}

	rc = retrieveLinuxCgroupMemoryStats(portLibrary, &cgroupMemInfo);
	if (0 != rc) {
		Trc_PRT_retrieveLinuxMemoryStats_CgroupMemoryStatsFailed(rc);
		/* If we failed to retrieve memory stats for cgroup, just use values for the host; don't consider it as failure */
		rc = 0;
		goto _exit;
	}

	/* Update memInfo based on cgroup limits */
	if (cgroupMemInfo.memoryLimit > memInfo->totalPhysical) {
		/* cgroup limit is not set; use host limits */
		goto _exit;
	}
	memInfo->totalPhysical = cgroupMemInfo.memoryLimit;

	if (cgroupMemInfo.memoryUsage <= cgroupMemInfo.memoryLimit) {
		memInfo->availPhysical = cgroupMemInfo.memoryLimit - cgroupMemInfo.memoryUsage;
		isCgroupMemUsageValid = TRUE;
	} else {
		memInfo->availPhysical = OMRPORT_MEMINFO_NOT_AVAILABLE;
	}

	swapLimit = cgroupMemInfo.memoryAndSwapLimit - cgroupMemInfo.memoryLimit;
	if (swapLimit < memInfo->totalSwap) {
		memInfo->totalSwap = swapLimit;
		isCgroupMemAndSwapLimitValid = TRUE;
	}

	if (isCgroupMemUsageValid && isCgroupMemAndSwapLimitValid) {
		if (cgroupMemInfo.memoryAndSwapUsage < cgroupMemInfo.memoryAndSwapLimit) {
			memInfo->availSwap = memInfo->totalSwap - (cgroupMemInfo.memoryAndSwapUsage - cgroupMemInfo.memoryUsage);
		} else {
			memInfo->availSwap = OMRPORT_MEMINFO_NOT_AVAILABLE;
		}
	}

	memInfo->swappiness = cgroupMemInfo.swappiness;
	memInfo->cached = cgroupMemInfo.cached;
	/* Buffered value is not available when running in a cgroup.
	 * See https://www.kernel.org/doc/Documentation/cgroup-v1/memory.txt
	 * for details of memory statistics available in cgroup.
	 */
	memInfo->buffered = OMRPORT_MEMINFO_NOT_AVAILABLE;

#endif /* !defined(OMRZTPF) */

_exit:
	Trc_PRT_retrieveLinuxMemoryStats_Exit(rc);
	return rc;
}

#elif defined(OSX)

/**
 * Function collects memory usage statistics on OSX and returns the same.
 *
 * @param[in] portLibrary The port library.
 * @param[in] memInfo A pointer to the J9MemoryInfo struct which we populate with memory usage.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
retrieveOSXMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo)
{
	int32_t ret = 0;

	/* Get total physical memory. */
	int name[2] = {CTL_HW, HW_MEMSIZE};
	uint64_t memsize = 0;
	size_t len = sizeof(memsize);

	if (0 == sysctl(name, 2, &memsize, &len, NULL, 0)) {
		memInfo->totalPhysical = memsize;
	} else {
		ret = OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	/* Get free physical memory */
	if (0 == ret) {
		vm_statistics_data_t vmStatData;
		mach_msg_type_number_t msgTypeNumber = sizeof(vmStatData) / sizeof(natural_t);
		if (KERN_SUCCESS == host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmStatData, &msgTypeNumber)) {
			memInfo->availPhysical = vm_page_size * vmStatData.free_count;
		} else {
			ret = OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
		}
	}

	/* Get total used and free swap memory. */
	if (0 == ret) {
		struct xsw_usage vmusage = {0};
		size_t size = sizeof(vmusage);
		if (0 == sysctlbyname("vm.swapusage", &vmusage, &size, NULL, 0))
		{
			memInfo->totalSwap = vmusage.xsu_total;
			memInfo->availSwap = vmusage.xsu_avail;
		} else {
			ret = OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
		}
	}

	/* Get virtual memory used by current process. */
	if (0 == ret) {
		struct task_basic_info taskInfo;
		mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

		if (KERN_SUCCESS == task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&taskInfo, &t_info_count)) {
			memInfo->totalVirtual = taskInfo.virtual_size;
			memInfo->availVirtual = taskInfo.virtual_size - taskInfo.resident_size;

		} else {
			ret = OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
		}
	}

	/* The amount of RAM used for cache memory and file buffers is not available on OSX. */
	memInfo->cached = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->buffered = OMRPORT_MEMINFO_NOT_AVAILABLE;

	memInfo->hostAvailPhysical = memInfo->availPhysical;
	memInfo->hostCached = memInfo->cached;
	memInfo->hostBuffered = memInfo->buffered;

	return ret;
}

#elif defined(AIXPPC)

/**
 * Function collects memory usage statistics on AIX and returns the same.
 *
 * @param[in] portLibrary The port library.
 * @param[in] memInfo A pointer to the J9MemoryInfo struct which we populate with memory usage.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
retrieveAIXMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo)
{
#if !defined(J9OS_I5)
	int32_t rc = 0;
	uint32_t page_size = 0;
	perfstat_memory_total_t aixMemoryInfo = {0};

	Trc_PRT_retrieveAIXMemoryStats_Entered();

	page_size = sysconf(_SC_PAGE_SIZE);

	rc = perfstat_memory_total(NULL, &aixMemoryInfo, sizeof(perfstat_memory_total_t), 1);
	if (1 != rc) {
		Trc_PRT_retrieveAIXMemoryStats_perfstatFailed(errno);
		Trc_PRT_retrieveAIXMemoryStats_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	/* AIX returns these quantities in number of regular sized (4K) pages. */
	memInfo->totalPhysical = aixMemoryInfo.real_total * page_size;
	memInfo->availPhysical = aixMemoryInfo.real_free * page_size;
	memInfo->totalSwap = aixMemoryInfo.pgsp_total * page_size;
	memInfo->availSwap = aixMemoryInfo.pgsp_free * page_size;
	memInfo->cached = aixMemoryInfo.numperm * page_size;
	/* AIX does not define buffers, so: memInfo->buffered = OMRPORT_MEMINFO_NOT_AVAILABLE; */

	memInfo->hostAvailPhysical = memInfo->availPhysical;
	memInfo->hostCached = memInfo->cached;
	memInfo->hostBuffered = memInfo->buffered;

	Trc_PRT_retrieveAIXMemoryStats_Exit(0);
#else
	memInfo->totalPhysical = 0;
	memInfo->availPhysical = 0;
	memInfo->totalSwap = 0;
	memInfo->availSwap = 0;
	memInfo->cached = 0;
#endif
	return 0;
}

#endif /* end OS specific guards */

int32_t
omrsysinfo_get_memory_info(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo, ...)
{
	int32_t rc = -1;

	Trc_PRT_sysinfo_get_memory_info_Entered();

	if (NULL == memInfo) {
		Trc_PRT_sysinfo_get_memory_info_Exit(OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED);
		return OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED;
	}

	/* Initialize to defaults. */
	memInfo->totalPhysical = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->availPhysical = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->totalVirtual = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->availVirtual = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->totalSwap = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->availSwap = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->cached = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->buffered = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->swappiness = OMRPORT_MEMINFO_NOT_AVAILABLE;

	memInfo->hostAvailPhysical = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->hostCached = OMRPORT_MEMINFO_NOT_AVAILABLE;
	memInfo->hostBuffered = OMRPORT_MEMINFO_NOT_AVAILABLE;

#if defined(LINUX)
	rc = retrieveLinuxMemoryStats(portLibrary, memInfo);
#elif defined(OSX)
	rc = retrieveOSXMemoryStats(portLibrary, memInfo);
#elif defined(AIXPPC)
	rc = retrieveAIXMemoryStats(portLibrary, memInfo);
#elif defined(J9ZOS390)
	rc = retrieveZOSMemoryStats(portLibrary, memInfo);
#endif

	memInfo->timestamp = (portLibrary->time_nano_time(portLibrary) / NANOSECS_PER_USEC);

	Trc_PRT_sysinfo_get_memory_info_Exit(rc);
	return rc;
}

uint64_t
omrsysinfo_get_addressable_physical_memory(struct OMRPortLibrary *portLibrary)
{
	uint64_t memoryLimit = 0;
	uint64_t usableMemory = portLibrary->sysinfo_get_physical_memory(portLibrary);

#if !defined(OMRZTPF)
	if (OMRPORT_LIMIT_LIMITED == portLibrary->sysinfo_get_limit(portLibrary, OMRPORT_RESOURCE_ADDRESS_SPACE, &memoryLimit)) {
		/* there is a limit on the memory we can use so take the minimum of this usable amount and the physical memory */
		usableMemory = OMR_MIN(memoryLimit, usableMemory);
	}
#else /* !defined(OMRZTPF) */
	usableMemory *= ZTPF_MEMORY_RESERVE_RATIO;
#endif /* !defined(OMRZTPF) */

	return usableMemory;
}

uint64_t
omrsysinfo_get_physical_memory(struct OMRPortLibrary *portLibrary)
{
	uint64_t result = 0;

#if defined(LINUX)
	if (portLibrary->sysinfo_cgroup_are_subsystems_enabled(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY)) {
		int32_t rc = portLibrary->sysinfo_cgroup_get_memlimit(portLibrary, &result);

		if (0 == rc) {
			return result;
		}
	}
#endif /* defined(LINUX) */

#if defined(RS6000)
	/* physmem is not a field in the system_configuration struct */
	/* on systems with 43K headers. However, this is not an issue as we only support AIX 5.2 and above only */
	result = (uint64_t) _system_configuration.physmem;
#elif defined(J9ZOS390)
	J9CVT * __ptr32 cvtp = ((J9PSA * __ptr32)0)->flccvt;
	J9RCE * __ptr32 rcep = cvtp->cvtrcep;
	result = ((U_64)rcep->rcepool * J9BYTES_PER_PAGE);
#elif defined(OSX) || defined(OMR_OS_BSD)
	{
		int name[2] = {
			CTL_HW,
#if defined(OSX)
			HW_MEMSIZE
#elif defined(OMR_OS_BSD)
			HW_PHYSMEM
#endif
};
		size_t len = sizeof(result);

		if (0 != sysctl(name, 2, &result, &len, NULL, 0)) {
			result = 0;
		}
	}
#elif defined(OMRZTPF)
	result = getPhysicalMemory();
#else /* defined(RS6000) */
	result = getPhysicalMemory(portLibrary);
#endif /* defined(RS6000) */
	return result;
}

#if !defined(RS6000) && !defined(J9ZOS390) && !defined(OSX) && !defined(OMRZTPF)

/**
 * Returns physical memory available on the system
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 *
 * @return physical memory available on the system
 */
static uint64_t
getPhysicalMemory(struct OMRPortLibrary *portLibrary)
{
	intptr_t pagesize, num_pages;

	pagesize = sysconf(_SC_PAGESIZE);
	num_pages = sysconf(_SC_PHYS_PAGES);

	if ((-1 == pagesize) || (-1 == num_pages)) {
		return 0;
	} else {
		return (uint64_t) pagesize * num_pages;
	}
}
#elif defined(OMRZTPF) /* !defined(RS6000) && !defined(J9ZOS390) && !defined(OSX) && !defined(OMRZTPF) */

static uint64_t
getPhysicalMemory( void ) {

	struct ebmaxc_parmlist newValues[] = {
			{ MAX64HEAP, MAXVALUE},
			{ 0, 0}
	};

	tpf_ebmaxc(newValues);
	uint64_t physMemory = (uint64_t) (*(uint16_t*)(cinfc_fast(CINFC_CMMMMES) + 8));

	/* Fetch available 1MB frames to system */
	int numFRM1MB  = numbc(LFRM1MB);
	/*
	 * If the available memory, i.e. number of available 1MB frames, is
	 * too low to satisfy MAX64HEAP, then the jvm cannot run. Return 0.
	 */
	if (physMemory >= (uint16_t)(numFRM1MB-1)) {
		physMemory = 0;
	}
	else {
		physMemory = physMemory << 20;
	}
	return physMemory;
}

#endif /* !defined(RS6000) && !defined(J9ZOS390) && !defined(OSX) && !defined(OMRZTPF) */

void
omrsysinfo_shutdown(struct OMRPortLibrary *portLibrary)
{
	if (NULL != portLibrary->portGlobals) {
		if (PPG_si_osVersion) {
			portLibrary->mem_free_memory(portLibrary, PPG_si_osVersion);
			PPG_si_osVersion = NULL;
		}

		if (PPG_si_osType) {
			portLibrary->mem_free_memory(portLibrary, PPG_si_osType);
			PPG_si_osType = NULL;
		}
		if (NULL != PPG_si_executableName) {
			portLibrary->mem_free_memory(portLibrary, PPG_si_executableName);
			PPG_si_executableName = NULL;
		}
#if defined(LINUX) && !defined(OMRZTPF)
		omrthread_monitor_enter(cgroupMonitor);
		freeCgroupEntries(portLibrary, PPG_cgroupEntryList);
		PPG_cgroupEntryList = NULL;
		omrthread_monitor_exit(cgroupMonitor);
		attachedPortLibraries -= 1;
		if (0 == attachedPortLibraries) {
			omrthread_monitor_destroy(cgroupMonitor);
			cgroupMonitor = NULL;
		}
#endif /* defined(LINUX) */
#if (defined(S390) || defined(J9ZOS390))
		PPG_stfleCache.lastDoubleWord = -1;
#endif
	}
}

int32_t
omrsysinfo_startup(struct OMRPortLibrary *portLibrary)
{
	int32_t rc = 0;

	PPG_criuSupportFlags = 0;
	PPG_sysinfoControlFlags = 0;
	/* Obtain and cache executable name; if this fails, executable name remains NULL, but
	 * shouldn't cause failure to startup port library.  Failure will be noticed only
	 * when the omrsysinfo_get_executable_name() actually gets invoked.
	 */
	(void) find_executable_name(portLibrary, &PPG_si_executableName);

#if defined(LINUX) && !defined(OMRZTPF)
	PPG_cgroupEntryList = NULL;
	PPG_processInContainerState = OMRPORT_PROCESS_IN_CONTAINER_UNINITIALIZED;
	/* To handle the case where multiple port libraries are started and shutdown, as done
	 * by some fvtests (eg fvtest/porttest/j9portTest.cpp) that create fake portlibrary to
	 * test its management and lifecycle, we need to ensure globals, such as cgroupMonitor,
	 * are initialized and destroyed only once.
	 */
	if (0 == attachedPortLibraries) {
		if (omrthread_monitor_init_with_name(&cgroupMonitor, 0, "cgroup monitor")) {
			rc = OMRPORT_ERROR_STARTUP_SYSINFO_MONITOR_INIT;
			goto _end;
		}
	}
	attachedPortLibraries += 1;

	if (isCgroupV1Available(portLibrary)) {
		PPG_sysinfoControlFlags |= OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE;
	} else if (isCgroupV2Available()) {
		PPG_sysinfoControlFlags |= OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE;
	}
_end:
#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return rc;

#if (defined(S390) || defined(J9ZOS390))
	PPG_stfleCache.lastDoubleWord = -1;
#endif

}

intptr_t
omrsysinfo_get_username(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length)
{
	char *remoteCopy = NULL;
	intptr_t rc = 0;
	size_t nameLen = 0;
	struct passwd *pwent = NULL;

#if defined(J9ZOS390)
	/* CMVC 182664 */
#define USERID_BUF_LEN 16
	char loginID[USERID_BUF_LEN];
	uintptr_t getuid_result = omrget_userid(loginID, USERID_BUF_LEN);
	if (0 == getuid_result) {
		remoteCopy = loginID;
	}
#endif
	if (NULL == remoteCopy) {
		uid_t uid = getuid();
		pwent = getpwuid(uid);

		if (NULL != pwent) {
			remoteCopy = pwent->pw_name;
		}
	}
	if (NULL == remoteCopy) {
		rc = -1;
	} else {
		nameLen = strlen(remoteCopy);
	}

	if ((0 == rc) && ((nameLen + 1) > length)) {
		rc = nameLen + 1;
	}

	if (0 == rc) {
		portLibrary->str_printf(portLibrary, buffer, length, "%s", remoteCopy);
	}

#if defined(_IBM_ATOE_H_)
	/* _IBM_ATOE_H_ is used instead of J9ZOS390 is to ensure that this code becomes dead code path
	 * if we ever decide to retire atoe library. This macro is defined in atoe.h, which is only included by macro
	 * J9ZOS390. Hence, there are no danger of undefined variable.
	 * j9 atoe library used malloc. Need to free any structure obtained from getpwnam and getpwuid.
	 */

	if (NULL != pwent) {
		free(pwent->pw_name);
		free(pwent->pw_dir);
		free(pwent->pw_shell);
		free(pwent);
	}
#endif

	return rc;
}

intptr_t
omrsysinfo_get_groupname(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length)
{
	char *remoteCopy = NULL;

#ifdef DEBUG
	char **member = NULL;
#endif

	gid_t gid = getgid();
	struct group *grent = getgrgid(gid);

	if (NULL != grent) {
		remoteCopy = grent->gr_name;

#ifdef DEBUG
		omrtty_printf(portLibrary, "Group name = \"%s\"\n", grent->gr_name);
		omrtty_printf(portLibrary, "Group id = %d\n", grent->gr_gid);
		omrtty_printf(portLibrary, "Start of member list:\n");
		for (member = grent->gr_mem; *member != NULL; member++) {
			omrtty_printf(portLibrary, "Member = \"%s\"\n", *member);
		}
		omrtty_printf(portLibrary, "End of member list\n");
#endif

	}

	if (NULL == remoteCopy) {
		return -1;
	} else {
		size_t nameLen = strlen(remoteCopy);

		if ((nameLen + 1) > length) {
			return nameLen + 1;
		}

		portLibrary->str_printf(portLibrary, buffer, length, "%s", remoteCopy);
	}

	return 0;
}

intptr_t
omrsysinfo_get_hostname(struct OMRPortLibrary *portLibrary, char *buffer, size_t length)
{
	if (0 != gethostname(buffer, length)) {
		int32_t err = errno;
		Trc_PRT_sysinfo_gethostname_error(findError(errno));
		return portLibrary->error_set_last_error(portLibrary, err, findError(err));
	}
	return 0;
}

/* Limits are unsupported on z/TPF */
#if defined(OMRZTPF)

uint32_t
omrsysinfo_get_limit(struct OMRPortLibrary* portLibrary, uint32_t resourceID, uint64_t* limit)
{
	int rc = OMRPORT_LIMIT_UNKNOWN;

	Trc_PRT_sysinfo_get_limit_Entered(resourceID);
	*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
	Trc_PRT_sysinfo_get_limit_Exit(rc);
	return rc;
}

uint32_t
omrsysinfo_set_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t limit)
{
	int rc = OMRPORT_LIMIT_UNKNOWN;

	Trc_PRT_sysinfo_set_limit_Entered(resourceID, limit);
	Trc_PRT_sysinfo_set_limit_Exit(rc);
	return rc;
}

#else /* defined(OMRZTPF) */

uint32_t
omrsysinfo_get_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t *limit)
{
	uint32_t resourceRequested = resourceID & ~OMRPORT_LIMIT_HARD;
	BOOLEAN hardLimitRequested = OMR_ARE_ALL_BITS_SET(resourceID, OMRPORT_LIMIT_HARD);
	uint32_t rc = OMRPORT_LIMIT_UNKNOWN;

	Trc_PRT_sysinfo_get_limit_Entered(resourceID);

	if (OMRPORT_RESOURCE_SHARED_MEMORY == resourceRequested) {
		rc = getLimitSharedMemory(portLibrary, limit);
	} else if (OMRPORT_RESOURCE_FILE_DESCRIPTORS == resourceRequested) {
		rc = getLimitFileDescriptors(portLibrary, limit, hardLimitRequested);
	} else if (OMRPORT_RESOURCE_CORE_FLAGS == resourceRequested) {
#if defined(AIXPPC)
		struct vario myvar;

		if (0 == sys_parm(SYSP_GET, SYSP_V_FULLCORE, &myvar)) {
			if (1 == myvar.v.v_fullcore.value) {
				*limit = U_64_MAX;
				rc = OMRPORT_LIMIT_UNLIMITED;
			} else {
				*limit = 0;
				rc = OMRPORT_LIMIT_LIMITED;
			}
		} else {
			*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
			portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_sysinfo_sysparm_error(errno);
			rc = OMRPORT_LIMIT_UNKNOWN;
		}
#else
		/* unsupported */
		*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
		rc = OMRPORT_LIMIT_UNKNOWN;
#endif
	} else {
		/* Other resources are handled through getrlimit() */
		struct rlimit lim = { 0, 0 };
		uint32_t resource = 0;

		switch (resourceRequested) {
		case OMRPORT_RESOURCE_ADDRESS_SPACE:
#if defined(J9ZOS39064)
			resource = RLIMIT_MEMLIMIT;
#else /* defined(J9ZOS39064) */
			resource = RLIMIT_AS;
#endif /* defined(J9ZOS39064) */
			break;
		case OMRPORT_RESOURCE_CORE_FILE:
			resource = RLIMIT_CORE;
			break;
		case OMRPORT_RESOURCE_DATA:
			resource = RLIMIT_DATA;
			break;
		default:
			Trc_PRT_sysinfo_getLimit_unrecognised_resourceID(resourceID);
			*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
			rc = OMRPORT_LIMIT_UNKNOWN;
			goto exit;
		}

		if (0 == getrlimit(resource, &lim)) {
			*limit = (uint64_t)(hardLimitRequested ? lim.rlim_max : lim.rlim_cur);
			if (RLIM_INFINITY == *limit) {
				rc = OMRPORT_LIMIT_UNLIMITED;
			} else {
				rc = OMRPORT_LIMIT_LIMITED;
			}
		} else {
			*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
			portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_sysinfo_getrlimit_error(resource, findError(errno));
			rc = OMRPORT_LIMIT_UNKNOWN;
		}
	}

exit:
	Trc_PRT_sysinfo_get_limit_Exit(rc);
	return rc;
}

uint32_t
omrsysinfo_set_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t limit)
{
	uint32_t resourceRequested = resourceID & ~OMRPORT_LIMIT_HARD;
	BOOLEAN hardLimitRequested = OMR_ARE_ALL_BITS_SET(resourceID, OMRPORT_LIMIT_HARD);
	uint32_t rc = 0;

	Trc_PRT_sysinfo_set_limit_Entered(resourceID, limit);

	if (OMRPORT_RESOURCE_FILE_DESCRIPTORS == resourceRequested) {
		rc = setLimitFileDescriptors(portLibrary, limit, hardLimitRequested);
	} else if (OMRPORT_RESOURCE_CORE_FLAGS == resourceRequested) {
#if defined(AIXPPC)
		struct vario myvar;

		myvar.v.v_fullcore.value = limit;
		rc = sys_parm(SYSP_SET, SYSP_V_FULLCORE, &myvar);
		if (-1 == rc) {
			portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_sysinfo_sysparm_error(errno);
		}
#else /* defined(AIXPPC) */
		/* unsupported so return error */
		rc = OMRPORT_LIMIT_UNKNOWN;
#endif /* defined(AIXPPC) */
	} else {
		struct rlimit lim = { 0, 0 };
		uint32_t resource = 0;

		switch (resourceRequested) {
		case OMRPORT_RESOURCE_ADDRESS_SPACE:
#if defined(J9ZOS39064)
			resource = RLIMIT_MEMLIMIT;
#else /* defined(J9ZOS39064) */
			resource = RLIMIT_AS;
#endif /* defined(J9ZOS39064) */
			break;
		case OMRPORT_RESOURCE_CORE_FILE:
			resource = RLIMIT_CORE;
			break;
		case OMRPORT_RESOURCE_DATA:
			resource = RLIMIT_DATA;
			break;
		default:
			Trc_PRT_sysinfo_setLimit_unrecognised_resourceID(resourceID);
			rc = OMRPORT_LIMIT_UNKNOWN;
			goto exit;
		}

		rc = getrlimit(resource, &lim);
		if (-1 == rc) {
			portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_sysinfo_getrlimit_error(resource, findError(errno));
			goto exit;
		}

		if (hardLimitRequested) {
			lim.rlim_max = limit;
		} else {
			lim.rlim_cur = limit;
		}
		rc = setrlimit(resource, &lim);
		if (-1 == rc) {
			portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_sysinfo_setrlimit_error(resource, limit, findError(errno));
		}
	}

exit:
	Trc_PRT_sysinfo_set_limit_Exit(rc);
	return rc;
}

static uint32_t
getLimitSharedMemory(struct OMRPortLibrary *portLibrary, uint64_t *limit)
{
#if defined(LINUX)
	int fd = 0;
	int64_t shmmax = -1;
	int bytesRead = 0;
	char buf[50];
	int32_t portableError = 0;

	Trc_PRT_sysinfo_getLimitSharedMemory_Entry();

	fd = portLibrary->file_open(portLibrary, "/proc/sys/kernel/shmmax", EsOpenRead, 0);
	if (-1 == fd) {
		portableError = portLibrary->error_last_error_number(portLibrary);
		Trc_PRT_sysinfo_getLimitSharedMemory_invalidFileHandle(portableError);
		portLibrary->error_set_last_error_with_message(portLibrary,
				portableError,
				"getLimitSharedMemory invalid return from file open");
		goto errorReturn;
	}

	bytesRead = portLibrary->file_read(portLibrary, fd, (char *)buf, sizeof(buf) - 1);
	portableError = portLibrary->error_last_error_number(portLibrary);
	Trc_PRT_sysinfo_getLimitSharedMemory_bytesRead(bytesRead, portableError);
	portLibrary->file_close(portLibrary, fd);

	if (bytesRead <= 0) {
		Trc_PRT_sysinfo_getLimitSharedMemory_invalidRead();
		goto errorReturn;
	} else {
		buf[bytesRead] = '\0';
		shmmax = atoll(buf);
		Trc_PRT_sysinfo_getLimitSharedMemory_maxSize(shmmax, buf);
	}
	*limit = (uint64_t)shmmax;
	Trc_PRT_sysinfo_getLimitSharedMemory_Exit(OMRPORT_LIMIT_LIMITED, *limit);
	return OMRPORT_LIMIT_LIMITED;

errorReturn:
	Trc_PRT_sysinfo_getLimitSharedMemory_ErrorExit(OMRPORT_LIMIT_UNKNOWN, OMRPORT_LIMIT_UNKNOWN_VALUE);
	*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
	return OMRPORT_LIMIT_UNKNOWN;
#else /* defined(LINUX) */
	Trc_PRT_sysinfo_getLimitSharedMemory_notImplemented(OMRPORT_LIMIT_UNKNOWN);
	*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
	return OMRPORT_LIMIT_UNKNOWN;
#endif /* defined(LINUX) */
}

#endif /* defined(OMRZTPF) */

static uint32_t
getLimitFileDescriptors(struct OMRPortLibrary *portLibrary, uint64_t *result, BOOLEAN hardLimitRequested)
{
	uint64_t limit = 0;
	uint32_t rc = 0;
	struct rlimit rlim = { 0, 0 };
#if defined(OSX)
	uint64_t sysctl_limit = 0;
	size_t sysctl_limit_len = sizeof(sysctl_limit);
	int sysctl_name[2] = { CTL_KERN, KERN_MAXFILESPERPROC };
#endif /* defined(OSX) */

	if (0 != getrlimit(RLIMIT_NOFILE, &rlim)) {
		goto error;
	}

	limit = (uint64_t)(hardLimitRequested ? rlim.rlim_max : rlim.rlim_cur);

#if defined(OSX)
	/* On OSX, the limit is <= KERN_MAXFILESPERPROC */
	if (0 != sysctl(sysctl_name, 2, &sysctl_limit, &sysctl_limit_len, NULL, 0)) {
		goto error;
	}
	limit = OMR_MIN(limit, sysctl_limit);
	rc = OMRPORT_LIMIT_LIMITED;
#else /* defined(OSX) */
	if (RLIM_INFINITY == limit) {
		rc = OMRPORT_LIMIT_UNLIMITED;
	} else {
		rc = OMRPORT_LIMIT_LIMITED;
	}
#endif /* defined(OSX) */

	*result = limit;
	return rc;

error:
	portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	Trc_PRT_sysinfo_getrlimit_error(RLIMIT_NOFILE, findError(errno));
	*result = OMRPORT_LIMIT_UNKNOWN_VALUE;
	return OMRPORT_LIMIT_UNKNOWN;
}

static uint32_t
setLimitFileDescriptors(struct OMRPortLibrary *portLibrary, uint64_t limit, BOOLEAN hardLimitRequested)
{
	uint32_t rc = 0;
	struct rlimit rlim = { 0, 0 };
#if defined(OSX)
	uint64_t sysctl_limit = 0;
	size_t sysctl_limit_len = sizeof(sysctl_limit);
	int sysctl_name[2] = { CTL_KERN, KERN_MAXFILESPERPROC };

	/* On OSX, we enforce that the limit is <= KERN_MAXFILESPERPROC */
	rc = sysctl(sysctl_name, 2, &sysctl_limit, &sysctl_limit_len, NULL, 0);
	if (0 != rc) {
		goto error;
	}
	if (sysctl_limit < limit) {
		rc = -1;
		goto error;
	}
#endif /* defined(OSX) */

	rc = getrlimit(RLIMIT_NOFILE, &rlim);
	if (0 != rc) {
		goto error;
	}

	if (hardLimitRequested) {
		rlim.rlim_max = limit;
	} else {
		rlim.rlim_cur = limit;
	}

	rc = setrlimit(RLIMIT_NOFILE, &rlim);
	if (0 != rc) {
		goto error;
	}

	return 0;

error:
	portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
	Trc_PRT_sysinfo_setrlimit_error(RLIMIT_NOFILE, limit, findError(errno));
	return rc;
}

intptr_t
omrsysinfo_get_load_average(struct OMRPortLibrary *portLibrary, struct J9PortSysInfoLoadData *loadAverageData)
{
#if (defined(LINUX) || defined(OSX)) && !defined(OMRZTPF)
	double loadavg[3];
	int returnValue = getloadavg(loadavg, 3);
	if (returnValue == 3) {
		loadAverageData->oneMinuteAverage = loadavg[0];
		loadAverageData->fiveMinuteAverage = loadavg[1];
		loadAverageData->fifteenMinuteAverage = loadavg[2];
		return 0;
	}
	return -1;
#elif defined(J9OS_I5)
	return -1;
#elif defined(AIXPPC)
	perfstat_cpu_total_t perfstats;
	int returnValue = perfstat_cpu_total(NULL, &perfstats, sizeof(perfstat_cpu_total_t), 1);
	if ((returnValue != EINVAL) && (returnValue != EFAULT) && (returnValue != ENOMEM)) {
		loadAverageData->oneMinuteAverage = ((double)perfstats.loadavg[0]) / (1 << SBITS);
		loadAverageData->fiveMinuteAverage = ((double)perfstats.loadavg[1]) / (1 << SBITS);
		loadAverageData->fifteenMinuteAverage = ((double)perfstats.loadavg[2]) / (1 << SBITS);
		return 0;
	}
	return -1;
#else
	return -1;
#endif
}

intptr_t
omrsysinfo_get_CPU_utilization(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTime)
{
	intptr_t status = OMRPORT_ERROR_SYSINFO_OPFAILED;
#if (defined(LINUX) && !defined(OMRZTPF)) || defined(AIXPPC) || defined(OSX)
	/* omrtime_nano_time() gives monotonically increasing times as against omrtime_hires_clock() that
	 * returns times that can (and does) decrease. Use this to compute timestamps.
	 */
	uint64_t preTimestamp = portLibrary->time_nano_time(portLibrary); /* ticks */
	uint64_t postTimestamp; /* ticks */

#if defined(LINUX)
	intptr_t bytesRead = -1;
	/*
	 * Read the first line of /proc/stat, which takes the form:
	 *      cpu <user> <nice> <system> <other data we don't use>
	 * where user, nice, and system are 64-bit numbers in units of USER_HZ (typically 10 ms).
	 *
	 * Each number will have up to 21 decimal digits.  Adding spaces and the "cpu  " prefix brings the total to
	 * ~70.  Allocate 128 bytes to give lots of margin.
	 */
	char buf[128];
	const uintptr_t CLK_HZ = sysconf(_SC_CLK_TCK); /* i.e. USER_HZ */
	const uintptr_t NS_PER_CLK = 1000000000 / CLK_HZ;
	intptr_t fd = portLibrary->file_open(portLibrary, "/proc/stat", EsOpenRead, 0);
	if (-1 == fd) {
		int32_t portableError = portLibrary->error_last_error_number(portLibrary);
		Trc_PRT_sysinfo_get_CPU_utilization_invalidFileHandle(portableError);
		return portableError;
	}

	/* leave space to put in a null */
	bytesRead = portLibrary->file_read(portLibrary, fd, (char *)buf, sizeof(buf) - 1);
	portLibrary->file_close(portLibrary, fd);

	if (bytesRead <= 0) {
		Trc_PRT_sysinfo_get_CPU_utilization_invalidRead();
		return OMRPORT_ERROR_FILE_OPFAILED;
	} else {
		int64_t userTime = 0;
		int64_t niceTime = 0;
		int64_t systemTime = 0;

		buf[bytesRead] = '\0';
		Trc_PRT_sysinfo_get_CPU_utilization_proc_stat_summary(buf);
		if (0 == sscanf(buf, "cpu  %" SCNd64 " %" SCNd64 " %" SCNd64, &userTime, &niceTime, &systemTime)) {
			return OMRPORT_ERROR_SYSINFO_GET_STATS_FAILED;
		}
		cpuTime->cpuTime = (userTime + niceTime + systemTime) * NS_PER_CLK;
		cpuTime->numberOfCpus = portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_ONLINE);
		status = 0;
	}
#elif defined(OSX)
	processor_cpu_load_info_t cpuLoadInfo;
	mach_msg_type_number_t msgTypeNumber;
	natural_t processorCount;
	natural_t i;
	kern_return_t rc;

	cpuTime->numberOfCpus = portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_ONLINE);
	cpuTime->cpuTime = 0;

	rc = host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
		&processorCount, (processor_info_array_t *)&cpuLoadInfo, &msgTypeNumber);
	if (KERN_SUCCESS == rc) {
		int64_t userTime = 0;
		int64_t niceTime = 0;
		int64_t systemTime = 0;
		for (i = 0; i < processorCount; i += 1) {
			userTime += cpuLoadInfo[i].cpu_ticks[CPU_STATE_USER];
			niceTime += cpuLoadInfo[i].cpu_ticks[CPU_STATE_NICE];
			systemTime += cpuLoadInfo[i].cpu_ticks[CPU_STATE_SYSTEM];
		}
		cpuTime->cpuTime = userTime + niceTime + systemTime;
		status = 0;
	} else {
		return OMRPORT_ERROR_FILE_OPFAILED;
	}
#elif defined(J9OS_I5)
	/*call in PASE wrapper to retrieve needed information.*/
	cpuTime->numberOfCpus = Xj9GetEntitledProcessorCapacity() / 100;
	/*Xj9GetSysCPUTime() is newly added to retrieve System CPU Time fromILE.*/
	cpuTime->cpuTime = Xj9GetSysCPUTime();
	status = 0;
#elif defined(AIXPPC) /* AIX */
	perfstat_cpu_total_t stats;
	const uintptr_t NS_PER_CPU_TICK = 10000000L;

	if (perfstat_cpu_total(NULL, &stats, sizeof(perfstat_cpu_total_t), 1) == -1) {
		Trc_PRT_sysinfo_get_CPU_utilization_perfstat_failed(errno);
		return OMRPORT_ERROR_SYSINFO_GET_STATS_FAILED;
	}

	Trc_PRT_sysinfo_get_CPU_utilization_perfstat(stats.user, stats.sys, stats.ncpus);
	cpuTime->numberOfCpus = stats.ncpus; /* get the actual number of CPUs against which the time is reported */
	cpuTime->cpuTime = (stats.user + stats.sys) * NS_PER_CPU_TICK;
	status = 0;
#endif
	postTimestamp = portLibrary->time_nano_time(portLibrary);

	if ((0 == preTimestamp) || (0 == postTimestamp) || (postTimestamp < preTimestamp)) {
		Trc_PRT_sysinfo_get_CPU_utilization_timeFailed();
		return OMRPORT_ERROR_SYSINFO_INVALID_TIME;
	}
	/* Use the average of the timestamps before and after reading processor times to reduce bias. */
	cpuTime->timestamp = (preTimestamp + postTimestamp) / 2;
	return status;
#else /* (defined(LINUX) && !defined(OMRZTPF)) || defined(AIXPPC) || defined(OSX) */
	/* Support on z/OS being temporarily removed to avoid wrong CPU stats being passed. */
	return OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED;
#endif
}

intptr_t
omrsysinfo_get_CPU_load(struct OMRPortLibrary *portLibrary, double *cpuLoad)
{
	J9SysinfoCPUTime currentCPUTime;
	J9SysinfoCPUTime *oldestCPUTime = &portLibrary->portGlobals->oldestCPUTime;
	J9SysinfoCPUTime *latestCPUTime = &portLibrary->portGlobals->latestCPUTime;

#if (defined(LINUX) && !defined(OMRZTPF)) || defined(AIXPPC) || defined(OSX)
	intptr_t portLibraryStatus = omrsysinfo_get_CPU_utilization(portLibrary, &currentCPUTime);

	if (portLibraryStatus < 0) {
		return portLibraryStatus;
	}

	if (oldestCPUTime->timestamp == 0) {
		*oldestCPUTime = currentCPUTime;
		*latestCPUTime = currentCPUTime;
		return OMRPORT_ERROR_INSUFFICIENT_DATA;
	}

	/* Calculate using the most recent value in the history */
	if (((currentCPUTime.timestamp - latestCPUTime->timestamp) >= 10000000) && (currentCPUTime.numberOfCpus != 0)) {
		*cpuLoad = OMR_MIN((currentCPUTime.cpuTime - latestCPUTime->cpuTime) / ((double)currentCPUTime.numberOfCpus * (currentCPUTime.timestamp - latestCPUTime->timestamp)), 1.0);
		if (*cpuLoad >= 0.0) {
			*oldestCPUTime = *latestCPUTime;
			*latestCPUTime = currentCPUTime;
			return 0;
		} else {
			/* Either the latest or the current time are bogus, so discard the latest value and try with the oldest value */
			*latestCPUTime = currentCPUTime;
		}
	}

	if (((currentCPUTime.timestamp - oldestCPUTime->timestamp) >= 10000000) && (currentCPUTime.numberOfCpus != 0)) {
		*cpuLoad = OMR_MIN((currentCPUTime.cpuTime - oldestCPUTime->cpuTime) / ((double)currentCPUTime.numberOfCpus * (currentCPUTime.timestamp - oldestCPUTime->timestamp)), 1.0);
		if (*cpuLoad >= 0.0) {
			return 0;
		} else {
			*oldestCPUTime = currentCPUTime;
		}
	}

	return OMRPORT_ERROR_OPFAILED;
#elif defined(J9ZOS390) /* (defined(LINUX) && !defined(OMRZTPF)) || defined(AIXPPC) || defined(OSX) */
	currentCPUTime.timestamp = portLibrary->time_nano_time(portLibrary);

	if (oldestCPUTime->timestamp == 0) {
		*oldestCPUTime = currentCPUTime;
		*latestCPUTime = currentCPUTime;
		return OMRPORT_ERROR_INSUFFICIENT_DATA;
	}

	/* Calculate using the most recent value in the history */
	if ((currentCPUTime.timestamp - latestCPUTime->timestamp) >= 10000000) {
		J9CVT* __ptr32 cvtp = ((J9PSA* __ptr32)0)->flccvt;
		J9RMCT* __ptr32 rcmtp = cvtp->cvtopctp;
		J9CCT* __ptr32 cctp = rcmtp->rmctcct;

		*cpuLoad = (double)cctp->ccvutilp / 100.0;
		*oldestCPUTime = *latestCPUTime;
		*latestCPUTime = currentCPUTime;
		return 0;
	}

	if ((currentCPUTime.timestamp - oldestCPUTime->timestamp) >= 10000000) {
		J9CVT* __ptr32 cvtp = ((J9PSA* __ptr32)0)->flccvt;
		J9RMCT* __ptr32 rcmtp = cvtp->cvtopctp;
		J9CCT* __ptr32 cctp = rcmtp->rmctcct;

		*cpuLoad = (double)cctp->ccvutilp / 100.0;
		return 0;
	}

	return OMRPORT_ERROR_OPFAILED;
#else /* (defined(LINUX) && !defined(OMRZTPF)) || defined(AIXPPC) || defined(OSX) || defined(J9ZOS390) */
	return OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED;
#endif
}

int32_t
omrsysinfo_limit_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state)
{

	if (NULL == state) {
		return OMRPORT_ERROR_INVALID_HANDLE;
	}

	state->count = 0;
	state->numElements = sizeof(limitMap) / sizeof(limitMap[0]);

	return 0;
}

BOOLEAN
omrsysinfo_limit_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state)
{
	return (state->count < state->numElements);
}

int32_t
omrsysinfo_limit_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state, J9SysinfoUserLimitElement *limitElement)
{

	struct rlimit limits;
	int getrlimitRC = -1;
	int32_t rc = OMRPORT_ERROR_SYSINFO_OPFAILED;

	limitElement->name = limitMap[state->count].resourceName;

#if !defined(OMRZTPF)
	getrlimitRC = getrlimit(limitMap[state->count].resource, &limits);

	if (0 != getrlimitRC) {

		/* getrlimit failed, but the user may want to try the next value so bump the counter*/

#ifdef DEBUG
		perror("getrlimit failure: ");
#endif
		limitElement->softValue = 0;
		limitElement->hardValue = 0;
		rc = OMRPORT_ERROR_SYSINFO_OPFAILED;

	} else {
		limitElement->name = limitMap[state->count].resourceName;

		if (RLIM_INFINITY == limits.rlim_cur) {
			limitElement->softValue = (uint64_t) OMRPORT_LIMIT_UNLIMITED;
		} else {
			limitElement->softValue = (uint64_t) limits.rlim_cur;
		}

		if (RLIM_INFINITY == limits.rlim_max) {
			limitElement->hardValue = (uint64_t) OMRPORT_LIMIT_UNLIMITED;
		} else {
			limitElement->hardValue = (uint64_t) limits.rlim_max;
		}

		rc = 0;
	}
#else /* !defined(OMRZTPF) */
	rc = OMRPORT_ERROR_SYSINFO_OPFAILED;
	limitElement->softValue = 0;
	limitElement->hardValue = 0;
#endif /* !defined(OMRZTPF) */

	state->count = state->count + 1;
	return rc;
}

static uintptr_t
copyEnvToBufferSignalHandler(struct OMRPortLibrary *portLib, uint32_t gpType, void *gpInfo, void *unUsed)
{
	return OMRPORT_SIG_EXCEPTION_RETURN;
}

#if defined(J9VM_USE_ICONV)

/* returns the number of bytes written to the output buffer, including the null terminator, or -1 if the buffer wasn't big enough */
static intptr_t
convertWithIConv(struct OMRPortLibrary *portLibrary, iconv_t *converter, char *inputBuffer, char *outputBuffer, uintptr_t bufLen)
{
	size_t inbytesleft, outbytesleft;
	char *inbuf, *outbuf;

	inbuf = (char *)inputBuffer; /* for some reason this argument isn't const */
	outbuf = outputBuffer;
	inbytesleft = strlen(inputBuffer);
	outbytesleft = bufLen - 1 /* space for null-terminator */ ;

#if defined(ENV_DEBUG)
	printf("convertWithIConv 1: %s\n", e2a_func(inputBuffer, strlen(inputBuffer)));
	fflush(stdout);
#endif

	while ((outbytesleft > 0) && (inbytesleft > 0)) {
		if ((size_t)-1 == iconv(*converter, &inbuf, &inbytesleft, &outbuf, &outbytesleft)) {
			if (errno == E2BIG) {
				return -1;
			}

			/* if we couldn't translate this character, copy one byte verbatim */
			*outbuf = *inbuf;
			outbuf++;
			inbuf++;
			inbytesleft--;
			outbytesleft--;
		}
	}

	*outbuf = '\0';

	/* outbytesleft started at (buflen -1).
	 * To find how much we wrote, subtract outbytesleft, then add 1 for the null terminator */
	return bufLen - 1 - outbytesleft + 1;
}
#endif /* J9VM_USE_ICONV */

#if defined(J9VM_USE_MBTOWC)
/* returns the number of bytes written to the output buffer, including the null terminator, or -1 if the buffer wasn't big enough */
static intptr_t
convertWithMBTOWC(struct OMRPortLibrary *portLibrary, char *inputBuffer, char *outputBuffer, uintptr_t bufLen)
{
	/* this code was taken from omrsl.c */
	char *out, *end, *walk;
	wchar_t ch;
	int ret;
	intptr_t bytesWritten = 0;

	out = outputBuffer;
	end = &outputBuffer[bufLen - 1];

	walk = inputBuffer;

#if defined(OMRZTPF)
	setlocale(LC_ALL, "C");
#endif /* defined(OMRZTPF) */

	/* reset the shift state */
	J9_IGNORE_RETURNVAL(mbtowc(NULL, NULL, 0));

	while (*walk) {
		ret = mbtowc(&ch, walk, MB_CUR_MAX);
		if (ret < 0) {
			ch = *walk++;
		} else if (ret == 0) {
			/* hit the null terminator */
			break;
		} else {
			walk += ret;
		}

		if (ch < 0x80) {
			if ((out + 1) > end) {
				/* out of space */
				return -1;
			}
			*out++ = (char) ch;
			bytesWritten = bytesWritten + 1;

		} else if (ch < 0x800) {
			if ((out + 2) > end) {
				/* out of space */
				return -1;
			}
			*out++ = (char)(0xc0 | ((ch >> 6) & 0x1f));
			*out++ = (char)(0x80 | (ch & 0x3f));
			bytesWritten = bytesWritten + 2;

		} else {
			if ((out + 3) > end) {
				/* out of space */
				return -1;
			}
			*out++ = (char)(0xe0 | ((ch >> 12) & 0x0f));
			*out++ = (char)(0x80 | ((ch >> 6) & 0x3f));
			*out++ = (char)(0x80 | (ch & 0x3f));
			bytesWritten = bytesWritten + 3;
		}
	}

	*out = '\0';
	bytesWritten = bytesWritten + 1;

	return bytesWritten;
}
#endif /* J9VM_USE_MBTOWC */

/**
 * Initializes the supplied buffer such that it can be used by the @ref omrsysinfo_env_iteraror_next() and @ref omrsysinfo_env_iterator_hasNext() APIs.
 *
 * If the caller supplies a non-NULL buffer that is not big enough to store all the name=value entries of the environment,
 *  copyEnvToBuffer will store as many entries as possible, until the buffer is used up.
 *
 * If the buffer wasn't even big enough to contain a single entry, set its entire contents to '\0'
 *
 * This call must be protected because memory in char **environ may become inaccessible while copying, resulting in a crash.
 *
 * @param[in] args pointer to a CopyEnvToBufferArgs structure
 *
 * @return
 *       - 0 if the buffer was big enough to store the entire environment.
 *       - the required size of the buffer, if it was not big enough to store the entire environment.
 *
 * @return: the number of elements available to the iterator
*/
static uintptr_t
copyEnvToBuffer(struct OMRPortLibrary *portLibrary, void *args)
{

	CopyEnvToBufferArgs *copyEnvToBufferArgs = (CopyEnvToBufferArgs *) args;
	uint8_t *buffer = copyEnvToBufferArgs->buffer;
	uintptr_t bufferSize = copyEnvToBufferArgs->bufferSizeBytes;
	intptr_t storageRequiredForEnvironment;
	uintptr_t spaceLeft;
	uint8_t *cursor;
	EnvListItem *prevItem, *currentItem = NULL;
	BOOLEAN bufferBigEnough = TRUE;
	uintptr_t i;
	uintptr_t rc;

	/* For the omrsysinfo_env_iterator */
#if defined(OSX)
	char **environ = *_NSGetEnviron();
#else /* defined(OSX) */
	extern char **environ;
#endif /* defined(OSX) */

#if defined(J9VM_USE_ICONV)
	iconv_t converter;
#endif

	memset(buffer, 0, bufferSize);

	/* How much space do we need to store the environment?
	 *  - we are converting to UTF-8, where 3 bytes is the maximum it can take to encode anything
	 *  - therefore, to keep things simple, just multiple the results of strlen by 3
	 */
#define J9_MAX_UTF8_SIZE_BYTES 3
	storageRequiredForEnvironment = 0;
	for (i = 0 ; NULL != environ[i] ; i++)  {

		storageRequiredForEnvironment = storageRequiredForEnvironment
										+ sizeof(EnvListItem)
										+ (intptr_t)(strlen(environ[i]) * J9_MAX_UTF8_SIZE_BYTES)   /* for the string itself (name=value ) */
										+ 1;    /* for the null-terminator */
	}

	if (NULL == buffer) {
		return storageRequiredForEnvironment;
	}

#if defined(J9VM_USE_ICONV)
	/* iconv_get is not an a2e function, so we need to pass it honest-to-goodness EBCDIC strings */
#ifdef J9ZOS390
#pragma convlit(suspend)
#endif

	converter = iconv_get(portLibrary, J9SYSINFO_ICONV_DESCRIPTOR, "UTF-8", nl_langinfo(CODESET));

#ifdef J9ZOS390
#pragma convlit(resume)
#endif

#endif /* defined(J9VM_USE_ICONV) */

	/* Copy each entry from environment to the user supplied buffer until we run out of space */
	cursor = buffer;
	prevItem = NULL;
	spaceLeft = bufferSize;
	copyEnvToBufferArgs->numElements = 0;

	for (i = 0 ; NULL != environ[i] ; i++)  {
		intptr_t bytesWrittenThisEntry = -1;

		if (spaceLeft > sizeof(EnvListItem)) {
			/* there's enough room to at least try to convert this env item */
			currentItem = (EnvListItem *)cursor;
			currentItem->next = NULL;
			currentItem->nameAndValue = (char *)(cursor + sizeof(EnvListItem));
		} else {
			bufferBigEnough = FALSE;
			break;
		}

#if defined(J9VM_USE_MBTOWC)

		bytesWrittenThisEntry = convertWithMBTOWC(portLibrary, environ[i], currentItem->nameAndValue, spaceLeft - sizeof(EnvListItem));

#elif defined(J9VM_USE_ICONV)

		if (J9VM_INVALID_ICONV_DESCRIPTOR == converter) {
			/* no converter available for this code set. Just dump the platform chars */
			strncpy(currentItem->nameAndValue, environ[i], spaceLeft - sizeof(EnvListItem));
			bytesWrittenThisEntry = strlen(environ[i]) + 1;
		} else {
			bytesWrittenThisEntry = convertWithIConv(portLibrary, &converter, environ[i], currentItem->nameAndValue, spaceLeft - sizeof(EnvListItem));
		}

#else
#error "platform does not define J9VM_USE_MBTOWC or J9VM_USE_ICONV"
#endif

		/* Were we able to write/convert this entry ? */
		if (bytesWrittenThisEntry < 0) {
			/* the buffer was too small */
			currentItem->nameAndValue = NULL;
			currentItem = NULL;
			bufferBigEnough = FALSE;
			break;
		}

		if (NULL != prevItem) {
			prevItem->next = currentItem;
		}
		prevItem = currentItem;

		spaceLeft = spaceLeft - (sizeof(EnvListItem) + bytesWrittenThisEntry);
		cursor = cursor + (sizeof(EnvListItem) + bytesWrittenThisEntry);
		copyEnvToBufferArgs->numElements = copyEnvToBufferArgs->numElements + 1;

#if defined(ENV_DEBUG)
		printf("\t%i stored as: %s\n", i, currentItem->nameAndValue);
		fflush(stdout);
#endif
	}

#if defined(J9VM_USE_ICONV)
	if (J9VM_INVALID_ICONV_DESCRIPTOR != converter) {
		iconv_free(portLibrary, J9SL_ICONV_DESCRIPTOR, converter);
	}
#endif

	if (bufferBigEnough) {
		rc = 0;
	} else {
		rc = storageRequiredForEnvironment;
	}
	return rc;
}

int32_t
omrsysinfo_env_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, void *buffer, uintptr_t bufferSizeBytes)
{
	int32_t rc =  OMRPORT_ERROR_SYSINFO_OPFAILED;

	CopyEnvToBufferArgs copyEnvToBufferArgs;
	uintptr_t sigProtectRc = -1;
	uintptr_t copyEnvToBufferRC;

	memset(&copyEnvToBufferArgs, 0, sizeof(copyEnvToBufferArgs));
	memset(state, 0, sizeof(*state));

	copyEnvToBufferArgs.buffer = buffer;
	copyEnvToBufferArgs.bufferSizeBytes = bufferSizeBytes;
	copyEnvToBufferArgs.numElements = 0; /* this value is returned by copyEnvToBuffer */

	sigProtectRc = portLibrary->sig_protect(portLibrary, copyEnvToBuffer, &copyEnvToBufferArgs, copyEnvToBufferSignalHandler, NULL, OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_RETURN, &copyEnvToBufferRC);

	if (0 == sigProtectRc) {
		/* copyEnvToToBuffer completed without a signal occurring so we can trust the return value contained in it's args */
		state->buffer = buffer;
		state->bufferSizeBytes = bufferSizeBytes;
		rc = (intptr_t) copyEnvToBufferRC;
	} else if (OMRPORT_SIG_EXCEPTION_OCCURRED == sigProtectRc) {
		/*  copyEnvToToBuffer triggered a signal, and copyEnvToBufferSignalHandler returned OMRPORT_SIG_EXCEPTION_RETURN */
		rc = OMRPORT_ERROR_SYSINFO_ENV_INIT_CRASHED_COPYING_BUFFER;
	} else if (OMRPORT_SIG_ERROR == sigProtectRc) {
		/* OMRPORT_SIG_ERROR, if an error occurred before fn could be executed */
		rc = OMRPORT_ERROR_SYSINFO_OPFAILED;
	} else {
		/* we shouldn't have gotten here */
	}

	if (0 == copyEnvToBufferArgs.numElements) {
		/* This is used by omrsysinfo_env_iterator_hasNext to indicate that there are no elements */
		state->current = NULL;
	} else {
		state->current = state->buffer;
	}

	return rc;
}

BOOLEAN
omrsysinfo_env_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state)
{

	if (NULL == state->current) {
		return FALSE;
	}

	return TRUE;

}

int32_t
omrsysinfo_env_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, J9SysinfoEnvElement *envElement)
{

	EnvListItem *item;

	if ((NULL == state->current)) {
		return OMRPORT_ERROR_SYSINFO_OPFAILED;
	}

	item = (EnvListItem *)(state->current);
	envElement->nameAndValue = item->nameAndValue;
	state->current = item->next;

	return 0;

}

#if defined(LINUX)

#define PROCSTATPATH    "/proc/stat"
#define PROCSTATPREFIX  "cpu"

/**
 * Function collects processor usage statistics on Linux platforms and returns the same.
 *
 * @param[in] portLibrary The port library.
 * @param[in] procInfo A pointer to J9ProcessorInfos struct that we populate with processor usage.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
retrieveLinuxProcessorStats(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo)
{
	int32_t rc = -1;
	int32_t cpuCntr = 0;

	/* Dummy variables for reading processor stats on Linux. Not worth saving, but can be thrown away
	 * once used to compute and save into other fields.
	 */
	uint64_t nice = 0;
	uint64_t irq = 0;
	uint64_t softirq = 0;
	uint64_t steal = 0;
	uint64_t guest = 0;

	char *linePtr = NULL;
	size_t lineSz = MAX_LINE_LENGTH;
	FILE *procStatFs = NULL;

	Trc_PRT_retrieveLinuxProcessorStats_Entered();

	procStatFs = fopen(PROCSTATPATH, "r");
	if (NULL == procStatFs) {
		Trc_PRT_retrieveLinuxProcessorStats_failedOpeningProcFs(errno);
		Trc_PRT_retrieveLinuxProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
	}

	Trc_PRT_retrieveLinuxProcessorStats_openedProcFs();

	/* getline() expects this line buffer to be malloc()ed so that it can re-size this according
	 * to its requirements. As such, this cannot be allocated from port-library heap allocators.
	 */
	linePtr = (char *)malloc(MAX_LINE_LENGTH * sizeof(char));
	if (NULL == linePtr) {
		fclose(procStatFs);
		Trc_PRT_retrieveLinuxProcessorStats_memAllocFailed();
		Trc_PRT_retrieveLinuxProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED);
		return OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED;
	}

	/* Loop until all records have been read-off "/proc/stat". */
	while ((-1 != getline(&linePtr, &lineSz, procStatFs))
		&& (0 == strncmp(linePtr, PROCSTATPREFIX, sizeof(PROCSTATPREFIX) - 1)))
	{
		int32_t cpuid = -1;

		/* Insufficient memory allocated since cpu count increased. Cleanup and return error. */
		if (J9_UNEXPECTED(cpuCntr > procInfo->totalProcessorCount)) {
			free(linePtr);
			fclose(procStatFs);
			Trc_PRT_retrieveLinuxProcessorStats_unexpectedCpuCount(procInfo->totalProcessorCount, cpuCntr);
			Trc_PRT_retrieveLinuxProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_UNEXPECTED_PROCESSOR_COUNT);
			return OMRPORT_ERROR_SYSINFO_UNEXPECTED_PROCESSOR_COUNT;
		}

		/* Either processor count has decreased or remained the same. In either case, parse the data. */
		if (0 == cpuCntr) {
			rc = sscanf(linePtr,
						"cpu  %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
						&procInfo->procInfoArray[cpuCntr].userTime,
						&nice,
						&procInfo->procInfoArray[cpuCntr].systemTime,
						&procInfo->procInfoArray[cpuCntr].idleTime,
						&procInfo->procInfoArray[cpuCntr].waitTime,
						&irq,
						&softirq,
						&steal,
						&guest);
		} else {
			rc = sscanf(linePtr,
						"cpu%" SCNd32 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64,
						&cpuid,
						&procInfo->procInfoArray[cpuCntr].userTime,
						&nice,
						&procInfo->procInfoArray[cpuCntr].systemTime,
						&procInfo->procInfoArray[cpuCntr].idleTime,
						&procInfo->procInfoArray[cpuCntr].waitTime,
						&irq,
						&softirq,
						&steal,
						&guest);

			/* If there are disabled SMT's, there may be processors that are online, identified by integers not
			 * in range 1 ... (N-1), [where N is the number of CPUs], i.e., exceed sysconf(_SC_NPROCESSORS_CONF).
			 */
			procInfo->procInfoArray[cpuCntr].proc_id = cpuid;
		} /* end if */

		/* Check whether for some reason operating system ended up writing a corrupt /proc/stat file or did the
		 * platform change the file format (unlikely, but even so, must check against inconsistencies in the
		 * expected /proc/stat format).
		 */
		if (J9_UNEXPECTED((0 == rc) || (EOF == rc))) {
			Trc_PRT_retrieveLinuxProcessorStats_unexpectedReadError(errno);
			free(linePtr);
			fclose(procStatFs);
			Trc_PRT_retrieveLinuxProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO);
			return OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
		}

		/* Compute the Busy times - on linux we must take in account even the nice,
		 * irq, softirq, steal, and guest times to arrive at the busy times, in addition
		 * to the user, system, and wait times.
		 */
		procInfo->procInfoArray[cpuCntr].busyTime = procInfo->procInfoArray[cpuCntr].userTime +
				procInfo->procInfoArray[cpuCntr].systemTime +
				nice +
				procInfo->procInfoArray[cpuCntr].waitTime +
				irq +
				softirq +
				steal +
				guest;

		/* Convert ticks values found in JIFFIES to microseconds. */
		procInfo->procInfoArray[cpuCntr].userTime *= TICKS_TO_USEC;
		procInfo->procInfoArray[cpuCntr].systemTime *= TICKS_TO_USEC;
		procInfo->procInfoArray[cpuCntr].idleTime *= TICKS_TO_USEC;
		procInfo->procInfoArray[cpuCntr].waitTime *= TICKS_TO_USEC;
		procInfo->procInfoArray[cpuCntr].busyTime *= TICKS_TO_USEC;

		/* Mark this cpu online using our counter rather than cpu identifier retrieved since that
		 * may exceed the cpu count.
		 */
		procInfo->procInfoArray[cpuCntr].online = OMRPORT_PROCINFO_PROC_ONLINE;
		cpuCntr++;
	} /* end while */

	free(linePtr);
	fclose(procStatFs);

	Trc_PRT_retrieveLinuxProcessorStats_Exit(0);
	return 0;
}

#elif defined(OSX)

/**
 * Function collects processor usage statistics on OSX and returns the same.
 *
 * @param[in] portLibrary The port library.
 * @param[in] procInfo A pointer to J9ProcessorInfos struct that we populate with processor usage.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
retrieveOSXProcessorStats(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo)
{
	int32_t ret = OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
	processor_cpu_load_info_t cpuLoadInfo;
	processor_basic_info_t basicInfo;
	mach_msg_type_number_t msgTypeNumber;
	natural_t processorCount;
	natural_t i;
	kern_return_t rc;
	host_t host = mach_host_self();

	rc = host_processor_info(host, PROCESSOR_CPU_LOAD_INFO,
		&processorCount, (processor_info_array_t *)&cpuLoadInfo, &msgTypeNumber);
	if (KERN_SUCCESS == rc) {
		rc = host_processor_info(host, PROCESSOR_BASIC_INFO,
			&processorCount, (processor_info_array_t *)&basicInfo, &msgTypeNumber);
	}
	if (KERN_SUCCESS == rc) {
		for (i = 0; i < processorCount; i += 1) {
			procInfo->procInfoArray[i + 1].online = basicInfo[i].running ? OMRPORT_PROCINFO_PROC_ONLINE : OMRPORT_PROCINFO_PROC_OFFLINE;
			procInfo->procInfoArray[i + 1].userTime = cpuLoadInfo[i].cpu_ticks[CPU_STATE_USER];
			procInfo->procInfoArray[i + 1].systemTime = cpuLoadInfo[i].cpu_ticks[CPU_STATE_SYSTEM];
			procInfo->procInfoArray[i + 1].idleTime = cpuLoadInfo[i].cpu_ticks[CPU_STATE_IDLE];
			procInfo->procInfoArray[i + 1].busyTime =
				cpuLoadInfo[i].cpu_ticks[CPU_STATE_USER] +
				cpuLoadInfo[i].cpu_ticks[CPU_STATE_SYSTEM] +
				cpuLoadInfo[i].cpu_ticks[CPU_STATE_NICE];

			procInfo->procInfoArray[0].userTime += procInfo->procInfoArray[i + 1].userTime;
			procInfo->procInfoArray[0].systemTime += procInfo->procInfoArray[i + 1].systemTime;
			procInfo->procInfoArray[0].idleTime += procInfo->procInfoArray[i + 1].idleTime;
			procInfo->procInfoArray[0].busyTime += procInfo->procInfoArray[i + 1].busyTime;
		}
		procInfo->procInfoArray[0].online = OMRPORT_PROCINFO_PROC_ONLINE;
		ret = 0;
	}
	return ret;
}

#elif defined(AIXPPC)

/**
 * Function collects processor usage statistics on AIX and returns the same.
 *
 * @param[in] portLibrary The port library.
 * @param[in] procInfo A pointer to J9ProcessorInfos struct that we populate with processor usage.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
retrieveAIXProcessorStats(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo)
{
#if !defined(J9OS_I5)
	int32_t rc = -1;
	int32_t cpuCntr = 0;
	intptr_t onlineProcessorCount = 0;
	perfstat_id_t firstcpu = {0};
	perfstat_cpu_t *statp = NULL;
	perfstat_cpu_total_t total_cpu_usage = {0};

	Trc_PRT_retrieveAIXProcessorStats_Entered();

	/* First, get total (system wide) processor usage statistics using perfstat_cpu_total(). */
	rc =  perfstat_cpu_total(NULL, &total_cpu_usage, sizeof(perfstat_cpu_total_t), 1);
	if (1 != rc) {
		Trc_PRT_retrieveAIXProcessorStats_perfstatFailed(errno);
		Trc_PRT_retrieveAIXProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
	}

	procInfo->procInfoArray[0].userTime = total_cpu_usage.user * TICKS_TO_USEC;
	procInfo->procInfoArray[0].systemTime = total_cpu_usage.sys * TICKS_TO_USEC;
	procInfo->procInfoArray[0].idleTime = total_cpu_usage.idle * TICKS_TO_USEC;
	procInfo->procInfoArray[0].waitTime = total_cpu_usage.wait * TICKS_TO_USEC;
	procInfo->procInfoArray[0].busyTime = procInfo->procInfoArray[0].userTime
										+ procInfo->procInfoArray[0].systemTime
										+ procInfo->procInfoArray[0].waitTime;

	/* perfstat_cpu() returns (highest_CPUID+1) that was online since last boot, but counts-in
	 * stale CPU records too. At present, we can't tell exactly which CPUs are online at this
	 * very moment. Note: AIX perfstat documentations incorrectly states this as highest_CPUID,
	 * but it actually is highest_CPUID + 1. This is likely to corrected in subsequent release
	 * notes.
	 */
	onlineProcessorCount = perfstat_cpu(NULL, NULL, sizeof(perfstat_cpu_t), 0);
	if (0 >= onlineProcessorCount) {
		/* Either perfstat_cpu() failed (returned -1) or returned 0 CPUs. 'errno' was set by the system. */
		Trc_PRT_retrieveAIXProcessorStats_perfstatFailed(errno);
		Trc_PRT_retrieveAIXProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
	} else if (onlineProcessorCount > procInfo->totalProcessorCount) {
		/* perfstat_cpu() succeeded, but by now number of processors configured has increased! */
		Trc_PRT_retrieveAIXProcessorStats_unexpectedCpuCount(procInfo->totalProcessorCount,
															 onlineProcessorCount);
		Trc_PRT_retrieveAIXProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_UNEXPECTED_PROCESSOR_COUNT);
		return OMRPORT_ERROR_SYSINFO_UNEXPECTED_PROCESSOR_COUNT;
	}

	/* Allocate as many entries perfstat_cpu_t as there are processors on the machine. */
	statp = (perfstat_cpu_t*) portLibrary->mem_allocate_memory(portLibrary,
															   onlineProcessorCount * sizeof(perfstat_cpu_t),
															   OMR_GET_CALLSITE(),
															   OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == statp) {
		Trc_PRT_retrieveAIXProcessorStats_memAllocFailed();
		Trc_PRT_retrieveAIXProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED);
		return OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED;
	}
	memset(statp, 0, onlineProcessorCount * sizeof(perfstat_cpu_t));

	/* Set name to first cpu. This is defined in <libperfstat.h>. */
	strcpy(firstcpu.name, FIRST_CPU);

	/* This time, actually obtain an array of CPU stats through perfstat. */
	rc = perfstat_cpu(&firstcpu, statp, sizeof(perfstat_cpu_t), onlineProcessorCount);
	if (0 >= rc) {
		/* Either perfstat_cpu() failed (with -1) or returned 0 CPUs. 'errno' was set by the system. */
		Trc_PRT_retrieveAIXProcessorStats_perfstatFailed(errno);
		portLibrary->mem_free_memory(portLibrary, statp);
		Trc_PRT_retrieveAIXProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_PROCESSOR_INFO;
	} else if ('\0' != firstcpu.name[0]) {
		/* After the last call to perfstat_cpu(3), the name must be "", unless more CPUs were added. */
		portLibrary->mem_free_memory(portLibrary, statp);
		Trc_PRT_retrieveAIXProcessorStats_unexpectedCpuCount(procInfo->totalProcessorCount, rc);
		Trc_PRT_retrieveAIXProcessorStats_Exit(OMRPORT_ERROR_SYSINFO_UNEXPECTED_PROCESSOR_COUNT);
		return OMRPORT_ERROR_SYSINFO_UNEXPECTED_PROCESSOR_COUNT;
	}

	for (cpuCntr = 0; cpuCntr <= onlineProcessorCount; cpuCntr++) {
		int32_t cpuid = -1;

		if (0 != cpuCntr) {
			sscanf(statp[cpuCntr - 1].name, "cpu%d", &cpuid);
			/* Convert ticks values found in JIFFIES to microseconds. */
			procInfo->procInfoArray[cpuid + 1].userTime = statp[cpuCntr - 1].user * TICKS_TO_USEC;
			procInfo->procInfoArray[cpuid + 1].systemTime = statp[cpuCntr - 1].sys * TICKS_TO_USEC;
			procInfo->procInfoArray[cpuid + 1].idleTime = statp[cpuCntr - 1].idle * TICKS_TO_USEC;
			procInfo->procInfoArray[cpuid + 1].waitTime = statp[cpuCntr - 1].wait * TICKS_TO_USEC;
			procInfo->procInfoArray[cpuid + 1].busyTime = procInfo->procInfoArray[cpuid + 1].userTime +
					procInfo->procInfoArray[cpuid + 1].systemTime +
					procInfo->procInfoArray[cpuid + 1].waitTime;
		} /* end if else */

		/* Check for the validity of the processor id. */
		Assert_PRT_true(cpuid == procInfo->procInfoArray[cpuid + 1].proc_id);

		procInfo->procInfoArray[cpuid + 1].online = OMRPORT_PROCINFO_PROC_ONLINE;
	} /* end for */

	portLibrary->mem_free_memory(portLibrary, statp);
	Trc_PRT_retrieveAIXProcessorStats_Exit(0);
	return 0;

#else /* defined(J9OS_I5) */
	return -1; /* not supported */
#endif /* defined(J9OS_I5) */
}
#endif

#define SYSINFO_MAX_RETRY_COUNT 5

int32_t
omrsysinfo_get_processor_info(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo)
{
	int32_t rc = -1;
	int32_t cpuCntr = 0;
	int32_t retries = 0;

	Trc_PRT_sysinfo_get_processor_info_Entered();

#if defined(J9ZOS390)
	/* Support on z/OS being temporarily removed to avoid wrong CPU stats being passed. */
	Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED);
	return OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED;
#endif

	if (NULL == procInfo) {
		Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED);
		return OMRPORT_ERROR_SYSINFO_NULL_OBJECT_RECEIVED;
	}

	procInfo->procInfoArray = NULL;

	/* Loop, (since we may allocate a processor array that falls short when we actually try reading processor
	 * usage statistics) until the time we hit the situation where the allocated space is sufficient for the
	 * number of holding information on the available processors.
	 */
	do {
		uintptr_t procInfoArraySize = 0;

		/* Obtain the number of processors on the machine. */
		procInfo->totalProcessorCount = portLibrary->sysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_PHYSICAL);
		Assert_PRT_true(0 < procInfo->totalProcessorCount);

		/* Allocate memory for the 'totalProcessorCount + 1' count of processors. But first check whether we are
		 * here to re-allocate, since we fell short with the allocated space or this is the first allocation.
		 */
		if (NULL != procInfo->procInfoArray) {
			portLibrary->mem_free_memory(portLibrary, procInfo->procInfoArray);
		}
		procInfoArraySize = (procInfo->totalProcessorCount + 1) * sizeof(J9ProcessorInfo);
		procInfo->procInfoArray = portLibrary->mem_allocate_memory(portLibrary,
								  procInfoArraySize,
								  OMR_GET_CALLSITE(),
								  OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL ==  procInfo->procInfoArray) {
			Trc_PRT_sysinfo_get_processor_info_memAllocFailed();
			Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED);
			return OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED;
		}
		Trc_PRT_sysinfo_get_processor_info_allocatedCPUs(procInfo->totalProcessorCount);

		/* Associate a processor id to each processor on the system. */
		for (cpuCntr = 0; cpuCntr < procInfo->totalProcessorCount + 1; cpuCntr++) {
			/* Start with the system record that is marked as -1. */
			procInfo->procInfoArray[cpuCntr].proc_id = cpuCntr - 1;
			procInfo->procInfoArray[cpuCntr].userTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].systemTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].idleTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].waitTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].busyTime = OMRPORT_PROCINFO_NOT_AVAILABLE;
			procInfo->procInfoArray[cpuCntr].online = OMRPORT_PROCINFO_PROC_OFFLINE;
		} /* end for(;;) */

		/* online field for the aggregate record needs to be -1 */
		procInfo->procInfoArray[0].online = -1;

#if defined(LINUX)
		rc = retrieveLinuxProcessorStats(portLibrary, procInfo);
#elif defined(OSX)
		rc = retrieveOSXProcessorStats(portLibrary, procInfo);
#elif defined(AIXPPC)
		rc = retrieveAIXProcessorStats(portLibrary, procInfo);
#elif defined(J9ZOS390)
		rc = retrieveZOSProcessorStats(portLibrary, procInfo);
#endif
		retries++;
		Trc_PRT_sysinfo_get_processor_info_retryCount(retries);
	} while ((OMRPORT_ERROR_SYSINFO_UNEXPECTED_PROCESSOR_COUNT == rc)
		  && (retries < SYSINFO_MAX_RETRY_COUNT));

	if (0 != rc) {
		/* The helper failed fetching stats, free up memory allocated for the CPU array. */
		portLibrary->mem_free_memory(portLibrary, procInfo->procInfoArray);
		procInfo->procInfoArray = NULL;
		/* Maximum retries exceeded. CPU count seems unstable (increasing everytime). */
		if (SYSINFO_MAX_RETRY_COUNT == retries) {
			Trc_PRT_sysinfo_get_processor_info_cpuCountUnstable();
			Trc_PRT_sysinfo_get_processor_info_Exit(OMRPORT_ERROR_SYSINFO_PROCESSOR_COUNT_UNSTABLE);
			return OMRPORT_ERROR_SYSINFO_PROCESSOR_COUNT_UNSTABLE;
		} else {
			/* A trace point in the helper already indicates the cause for failure. */
			Trc_PRT_sysinfo_get_processor_info_Exit(rc);
			return rc;
		}
	}

	procInfo->timestamp = (portLibrary->time_nano_time(portLibrary) / NANOSECS_PER_USEC);
	Trc_PRT_sysinfo_get_processor_info_Exit(0);
	return 0;
}

void
omrsysinfo_destroy_processor_info(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfos)
{
	Trc_PRT_sysinfo_destroy_processor_info_Entered();

	if (NULL != procInfos->procInfoArray) {
		portLibrary->mem_free_memory(portLibrary, procInfos->procInfoArray);
		procInfos->procInfoArray = NULL;
	}

	Trc_PRT_sysinfo_destroy_processor_info_Exit();
}

intptr_t
omrsysinfo_get_cwd(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen)
{
	intptr_t rc = -1;

	if (NULL == buf) {
		char *tmpWorkingDirectory = NULL;
		Assert_PRT_true(0 == bufLen);

		if (0 == cwdname(portLibrary, &tmpWorkingDirectory)) {
			rc = strlen(tmpWorkingDirectory) + 1;
			portLibrary->mem_free_memory(portLibrary, tmpWorkingDirectory);
		}
	} else {
		if (NULL != getcwd(buf, bufLen)) {
			rc = 0;
		} else {
			char *tmpWorkingDirectory = NULL;
			if (0 == cwdname(portLibrary, &tmpWorkingDirectory)) {
				rc = strlen(tmpWorkingDirectory) + 1;
				portLibrary->mem_free_memory(portLibrary, tmpWorkingDirectory);
			}
		}
	}
	return rc;
}

intptr_t
omrsysinfo_get_tmp(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, BOOLEAN ignoreEnvVariable)
{
	intptr_t rc = -1;

	if (NULL == buf) {
		Assert_PRT_true(0 == bufLen);
	}

	if (FALSE == ignoreEnvVariable) {
		/* try to get temporary path from environment variable */
		rc = omrsysinfo_get_env(portLibrary, "TMPDIR", buf, bufLen);
	}

	if ((-1 == rc) || (TRUE == ignoreEnvVariable)) {
		/* no environment variable, use /tmp/ */
		const char *defaultTmp = "/tmp/";
		size_t tmpLen = strlen(defaultTmp) + 1;

		if (bufLen >= tmpLen) {
			omrstr_printf(portLibrary, buf, bufLen, defaultTmp);
			rc = 0;
		} else {
			rc = tmpLen;
		}
	}
	return rc;
}

static void
setPortableError(OMRPortLibrary *portLibrary, const char *funcName, int32_t portlibErrno, int systemErrno)
{
	char *errmsgbuff = NULL;
	int32_t errmsglen = J9ERROR_DEFAULT_BUFFER_SIZE;
	int32_t portableErrno = portlibErrno;

	switch (systemErrno) {
	case EACCES:
		portableErrno += OMRPORT_ERROR_SYSINFO_ERROR_EINVAL;
		break;
	case EFAULT:
		portableErrno += OMRPORT_ERROR_SYSINFO_ERROR_EFAULT;
		break;
	}

	/*Get size of str_printf buffer (it includes null terminator)*/
	errmsglen = portLibrary->str_printf(portLibrary, NULL, 0, "%s%s", funcName, strerror(systemErrno));
	if (errmsglen <= 0) {
		/*Set the error with no message*/
		portLibrary->error_set_last_error(portLibrary, systemErrno, portableErrno);
		return;
	}

	/*Alloc the buffer*/
	errmsgbuff = portLibrary->mem_allocate_memory(portLibrary, errmsglen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == errmsgbuff) {
		/*Set the error with no message*/
		portLibrary->error_set_last_error(portLibrary, systemErrno, portableErrno);
		return;
	}

	/*Fill the buffer using str_printf*/
	portLibrary->str_printf(portLibrary, errmsgbuff, errmsglen, "%s%s", funcName, strerror(systemErrno));

	/*Set the error message*/
	portLibrary->error_set_last_error_with_message(portLibrary, portableErrno, errmsgbuff);

	/*Free the buffer*/
	portLibrary->mem_free_memory(portLibrary, errmsgbuff);

	return;
}

int32_t
omrsysinfo_get_open_file_count(struct OMRPortLibrary *portLibrary, uint64_t *count)
{
	int32_t ret = -1;
	uint64_t fdCount = 0;

	Trc_PRT_sysinfo_get_open_file_count_Entry();
	/* Check whether a valid pointee exists, to write to. */
	if (NULL == count) {
		portLibrary->error_set_last_error(portLibrary, EINVAL, findError(EINVAL));
		Trc_PRT_sysinfo_get_open_file_count_invalidArgRecvd("count");
	} else {
#if defined(LINUX) || defined(AIXPPC)
		char buffer[PATH_MAX] = {0};
		const char *procDirectory = "/proc/%d/fd/";
		DIR *dir = NULL;

		/* On Linux, "/proc/self" is as good as "/proc/<current-pid>/", but not on
		 * AIX, where the current process's PID must be specified.
		 */
		portLibrary->str_printf(portLibrary, buffer, sizeof(buffer), procDirectory, getpid());
		dir = opendir(buffer);
		if (NULL == dir) {
			int32_t rc = findError(errno);
			portLibrary->error_set_last_error(portLibrary, errno, rc);
			Trc_PRT_sysinfo_get_open_file_count_failedOpeningProcFS(rc);
		} else {
			struct dirent *dp = NULL;
			/* opendir(3) was successful; look into the directory for fds. */
			errno = 0; /* readdir(3) will set this, if an error occurs. */
			dp = readdir(dir);
			while (NULL != dp) {
				/* Skip "." and ".." */
				if (!(('.' == dp->d_name[0] && '\0' == dp->d_name[1])
					|| ('.' == dp->d_name[0] && '.' == dp->d_name[1] && '\0' == dp->d_name[2]))
				) {
					fdCount++;
				}
				dp = readdir(dir);
			}
			/* readdir(3) would eventually return NULL; check whether there was an
			 * error and readdir returned prematurely.
			 */
			if (0 != errno) {
				int32_t rc = findError(errno);
				portLibrary->error_set_last_error(portLibrary, errno, rc);
				Trc_PRT_sysinfo_get_open_file_count_failedReadingProcFS(rc);
			} else {
				/* Successfully counted up the files opened. */
				*count = fdCount;
				Trc_PRT_sysinfo_get_open_file_count_fileCount(fdCount);
				ret = 0;
			}
			closedir(dir); /* Done reading the /proc file-system. */
		}
#elif defined(OSX)
		pid_t pid = getpid();
		/* First call with null to get the size of the buffer required */
		int32_t bufferSize = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, 0, 0);
		if (bufferSize <= 0) {
			int32_t rc = findError(errno);
			portLibrary->error_set_last_error(portLibrary, errno, rc);
			Trc_PRT_sysinfo_get_open_file_count_failedReadingProcFS(rc);
		} else {
			struct proc_fdinfo *procFDInfo = (struct proc_fdinfo *) portLibrary->mem_allocate_memory(portLibrary, bufferSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == procFDInfo) {
				Trc_PRT_sysinfo_get_open_file_count_memAllocFailed();
				portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED, "memory allocation for proc_fdinfo failed");
			} else {
				/* Second call with a buffer just allocated, retrieve those available proc_fdinfo instances */
				int32_t retTmp = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, procFDInfo, bufferSize);
				if (retTmp < 0) {
					int32_t rc = findError(errno);
					portLibrary->error_set_last_error(portLibrary, errno, rc);
					Trc_PRT_sysinfo_get_open_file_count_failedReadingProcFS(rc);
				} else {
					int32_t i = 0;
					int32_t numberOfProcFDs = bufferSize / PROC_PIDLISTFD_SIZE;
					for (i = 0; i < numberOfProcFDs; i++) {
						/* A file opened */
						if (PROX_FDTYPE_VNODE == procFDInfo[i].proc_fdtype) {
							fdCount++;
						}
					}
					*count = fdCount;
					Trc_PRT_sysinfo_get_open_file_count_fileCount(fdCount);
					ret = 0;
				}
				portLibrary->mem_free_memory(portLibrary, procFDInfo);
			}
		}
#elif defined(J9ZOS390)
		/* TODO: stub where z/OS code goes in. */
		ret = OMRPORT_ERROR_SYSINFO_GET_OPEN_FILES_NOT_SUPPORTED;
#endif
	}

	Trc_PRT_sysinfo_get_open_file_count_Exit(ret);
	return ret;
}

#if defined(J9ZOS390)
/**
 * @internal
 * Helper to set appropriate feature field in a OMROSDesc struct
 *
 * @param[in] desc pointer to the struct that contains the OS features
 * @param[in] feature to set
 *
 */
static void
setOSFeature(struct OMROSDesc *desc, uint32_t feature)
{
	if ((NULL != desc) && (feature < (OMRPORT_SYSINFO_OS_FEATURES_SIZE * 32))) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		desc->features[featureIndex] = (desc->features[featureIndex] | (1u << (featureShift)));
	}
}

/**
 * @internal
 * Populates OMROSDesc *desc for zOS
 *
 * @param[in] desc pointer to the struct that will contain the OS features
 *
 * @return 0 on success, -1 on failure
 */
static intptr_t
getZOSDescription(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc)
{
	intptr_t rc = 0;

#if defined(OMR_ENV_DATA64)
	J9CVT * __ptr32 cvtp = ((J9PSA * __ptr32)0)->flccvt;
	uint8_t cvtoslvl6 = cvtp->cvtoslvl[6];
	if (OMR_ARE_ANY_BITS_SET(cvtoslvl6, 0x10)) {
		setOSFeature(desc, OMRPORT_ZOS_FEATURE_RMODE64);
	}
#endif /* defined(OMR_ENV_DATA64) */

	return rc;
}
#endif /* defined(J9ZOS390) */

intptr_t
omrsysinfo_get_os_description(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc)
{
	intptr_t rc = -1;
	Trc_PRT_sysinfo_get_os_description_Entered(desc);

	if (NULL != desc) {
		memset(desc, 0, sizeof(OMROSDesc));

#if defined(J9ZOS390)
		rc = getZOSDescription(portLibrary, desc);
#endif /* defined(J9ZOS390) */
	}

	Trc_PRT_sysinfo_get_os_description_Exit(rc);
	return rc;
}

BOOLEAN
omrsysinfo_os_has_feature(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc, uint32_t feature)
{
	BOOLEAN rc = FALSE;
	Trc_PRT_sysinfo_os_has_feature_Entered(desc, feature);

	if ((NULL != desc) && (feature < (OMRPORT_SYSINFO_OS_FEATURES_SIZE * 32))) {
		uint32_t featureIndex = feature / 32;
		uint32_t featureShift = feature % 32;

		rc = OMR_ARE_ALL_BITS_SET(desc->features[featureIndex], 1u << featureShift);
	}

	Trc_PRT_sysinfo_os_has_feature_Exit((uintptr_t)rc);
	return rc;
}

BOOLEAN
omrsysinfo_os_kernel_info(struct OMRPortLibrary *portLibrary, struct OMROSKernelInfo *kernelInfo)
{
	BOOLEAN success = FALSE;

#if defined(LINUX)
	struct utsname name = {{0}};
	if (0 == uname(&name)) {
		if (3 == sscanf(name.release, "%u.%u.%u", &kernelInfo->kernelVersion, &kernelInfo->majorRevision, &kernelInfo->minorRevision)) {
			success = TRUE;
		}
	}
#endif /* defined(LINUX) */

	return success;
}

#if defined(LINUX) && !defined(OMRZTPF)

/**
 * @internal
 * Checks if cgroup v1 system is available
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 *
 * @return TRUE if cgroup v1 system is available, FALSE otherwise
 */
static BOOLEAN
isCgroupV1Available(struct OMRPortLibrary *portLibrary)
{
	struct statfs buf = {0};
	int32_t rc = 0;
	BOOLEAN result = TRUE;

	/* If tmpfs is mounted on /sys/fs/cgroup, then it indicates cgroup v1 system is available */
	rc = statfs(OMR_CGROUP_MOUNT_POINT, &buf);
	if (0 != rc) {
		int32_t osErrCode = errno;
		Trc_PRT_isCgroupV1Available_statfs_failed(OMR_CGROUP_MOUNT_POINT, osErrCode);
		portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_SYSINFO_SYS_FS_CGROUP_STATFS_FAILED);
		result = FALSE;
	} else if (TMPFS_MAGIC != buf.f_type) {
		Trc_PRT_isCgroupV1Available_tmpfs_not_mounted(OMR_CGROUP_MOUNT_POINT);
		portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_SYS_FS_CGROUP_TMPFS_NOT_MOUNTED, "tmpfs is not mounted on " OMR_CGROUP_MOUNT_POINT);
		result = FALSE;
	}

	return result;
}

/**
 * @internal
 * Checks if cgroup v2 system is available
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 *
 * @return TRUE if cgroup v2 system is available, FALSE otherwise
 */
static BOOLEAN
isCgroupV2Available(void)
{
	BOOLEAN result = FALSE;

	/* If the cgroup.controllers file exists at the root cgroup, then v2 is available. */
	if (0 == access(OMR_CGROUP_MOUNT_POINT "/cgroup.controllers", F_OK)) {
		result = TRUE;
	}

	return result;
}

/**
 * @internal
 * Free resources allocated for OMRCgroupEntry
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] cgEntryList pointer to list of OMRCgroupEntry to be freed
 *
 * @return void
 */
static void
freeCgroupEntries(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList)
{
	OMRCgroupEntry *cgEntry = cgEntryList;
	OMRCgroupEntry *nextEntry = NULL;

	if (NULL == cgEntry) {
		return;
	}

	do {
		nextEntry = cgEntry->next;
		portLibrary->mem_free_memory(portLibrary, cgEntry);
		cgEntry = nextEntry;
	} while (cgEntry != cgEntryList);
}

/**
 * Returns cgroup name for the subsystem/resource controller using the circular
 * linked list pointed by cgEntryList.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] cgEntryList pointer to OMRCgroupEntry* which points to circular linked list
 * @param[in] subsystem The subsystem/resource controller for which cgroup is required
 *
 * @return name of the cgroup for the given subsystem if available, else NULL
 */
static char *
getCgroupNameForSubsystem(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList, const char *subsystem)
{
	OMRCgroupEntry *temp = cgEntryList;
	char *cgName = NULL;

	if (NULL == temp) {
		goto _end;
	}

	do {
		if (!strcmp(temp->subsystem, subsystem)) {
			cgName = temp->cgroup;
			break;
		}
		temp = temp->next;
	} while (temp != cgEntryList);

_end:
	return cgName;
}

/**
 * Allocates a new OMRCgroupEntry structure and adds it to the circular linked list pointed by *cgEntryList.
 * On success, *cgEntryList points to the new OMRCgroupEntry structure.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] cgEntryList pointer to OMRCgroupEntry* which points to circular linked list
 * @param[in] hierId hierarchy Id of the cgroup
 * @param[in] subsystem name of cgroup subsystem/resource controller
 * @param[in] cgroupName name of the cgroup
 *
 * @return 0 on success, negative error code on failure
 */
static int32_t
addCgroupEntry(struct OMRPortLibrary *portLibrary, OMRCgroupEntry **cgEntryList, int32_t hierId, const char *subsystem, const char *cgroupName, uint64_t flag)
{
	int32_t rc = 0;
	int32_t cgEntrySize = sizeof(OMRCgroupEntry) + strlen(subsystem) + 1 + strlen(cgroupName) + 1;
	OMRCgroupEntry *cgEntry = portLibrary->mem_allocate_memory(portLibrary, cgEntrySize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL == cgEntry) {
		Trc_PRT_addCgroupEntry_oom_for_cgroup_entry();
		rc = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED, "memory allocation for cgroup entry failed");
		goto _end;
	}
	cgEntry->hierarchyId = hierId;
	cgEntry->subsystem = (char *)(cgEntry + 1);
	strcpy(cgEntry->subsystem, subsystem);
	cgEntry->cgroup = cgEntry->subsystem + strlen(subsystem) + 1;
	strcpy(cgEntry->cgroup, cgroupName);
	cgEntry->flag = flag;

	if (NULL == *cgEntryList) {
		*cgEntryList = cgEntry;
		cgEntry->next = cgEntry; /* create a circular list */
	} else {
		OMRCgroupEntry *first = *cgEntryList;
		*cgEntryList = cgEntry;
		cgEntry->next = first->next;
		first->next = cgEntry;
	}

	Trc_PRT_addCgroupEntry_added_new_entry(cgEntry->subsystem, cgEntry->cgroup);
_end:
	return rc;
}

/**
 * Reads /proc/<pid>/cgroup to get information about the cgroups to which the given
 * process belongs.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] pid process id
 * @param[in] inContainer if set to TRUE then ignore cgroup in /proc/<pid>/cgroup and use ROOT_CGROUP instead
 * @param[out] cgroupEntryList pointer to OMRCgroupEntry *. On successful return, *cgroupEntry
 * points to a circular linked list. Each element of the list is populated based on the contents
 * of /proc/<pid>/cgroup file.
 * @param[out] availableSubsystems on successful return, contains bitwise-OR of flags of type OMR_CGROUP_SUBSYSTEMS_*
 * indicating the subsystems available for use
 *
 * returns 0 on success, negative code on error
 */
static int32_t
populateCgroupEntryListV1(struct OMRPortLibrary *portLibrary, int pid, BOOLEAN inContainer, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems)
{
	char cgroupFilePath[PATH_MAX];
	uintptr_t requiredSize = 0;
	FILE *cgroupFile = NULL;
	OMRCgroupEntry *cgEntryList = NULL;
	uint64_t available = 0;
	int32_t rc = 0;

	Assert_PRT_true(NULL != cgroupEntryList);

	requiredSize = portLibrary->str_printf(portLibrary, NULL, 0, "/proc/%d/cgroup", pid);
	Assert_PRT_true(requiredSize <= sizeof(cgroupFilePath));
	portLibrary->str_printf(portLibrary, cgroupFilePath, sizeof(cgroupFilePath), "/proc/%d/cgroup", pid);

	/* Even if 'inContainer' is TRUE, we need to parse the cgroup file to get the list of subsystems */
	cgroupFile = fopen(cgroupFilePath, "r");
	if (NULL == cgroupFile) {
		int32_t osErrCode = errno;
		Trc_PRT_populateCgroupEntryList_fopen_failed(1, cgroupFilePath, osErrCode);
		rc = portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_FOPEN_FAILED);
		goto _end;
	}

	while (0 == feof(cgroupFile)) {
		char buffer[PATH_MAX];
		char cgroup[PATH_MAX];
		/* This array should be large enough to read names of all subsystems. 1024 should be enough based on current supported subsystems. */
		char subsystems[PATH_MAX];
		char *cursor = NULL;
		char *separator = NULL;
		int32_t hierId = -1;

		if (NULL == fgets(buffer, sizeof(buffer), cgroupFile)) {
			break;
		}
		if (0 != ferror(cgroupFile)) {
			int32_t osErrCode = errno;
			Trc_PRT_populateCgroupEntryList_fgets_failed(1, cgroupFilePath, osErrCode);
			rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "fgets failed to read %s file stream with errno=%d", cgroupFilePath, osErrCode);
			goto _end;
		}
		rc = sscanf(buffer, PROC_PID_CGROUPV1_ENTRY_FORMAT, &hierId, subsystems, cgroup);

		if (EOF == rc) {
			break;
		} else if (1 == rc) {
			rc = sscanf(buffer, PROC_PID_CGROUP_SYSTEMD_ENTRY_FORMAT, &hierId, cgroup);

			if (2 != rc) {
				Trc_PRT_populateCgroupEntryList_unexpected_format(1, cgroupFilePath);
				rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpected format of %s", cgroupFilePath);
				goto _end;
			}
			subsystems[0] = '\0';
		} else if (3 != rc) {
			Trc_PRT_populateCgroupEntryList_unexpected_format(1, cgroupFilePath);
			rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpected format of %s", cgroupFilePath);
			goto _end;
		}

		cursor = subsystems;
		do {
			int32_t i = 0;

			separator = strchr(cursor, ',');
			if (NULL != separator) {
				*separator = '\0';
			}

			for (i = 0; i < sizeof(supportedSubsystems) / sizeof(supportedSubsystems[0]); i++) {
				if (OMR_ARE_NO_BITS_SET(available, supportedSubsystems[i].flag)
					&& (0 == strcmp(cursor, supportedSubsystems[i].name))
				) {
					const char *cgroupToUse = cgroup;

					if (TRUE == inContainer) {
						/* Cgroup v1 does not support namespaces; overwrite cgroup path as seen by host
						 * with '/', the actual cgroup seen from the container.
						 */
						cgroupToUse = ROOT_CGROUP;
					}
					rc = addCgroupEntry(portLibrary, &cgEntryList, hierId, cursor, cgroupToUse, supportedSubsystems[i].flag);
					if (0 != rc) {
						goto _end;
					}
					available |= supportedSubsystems[i].flag;
				}
			}
			if (NULL != separator) {
				cursor = separator + 1;
			}
		} while (NULL != separator);
	}
	rc = 0;

_end:
	if (NULL != cgroupFile) {
		fclose(cgroupFile);
	}
	if (0 != rc) {
		freeCgroupEntries(portLibrary, cgEntryList);
		cgEntryList = NULL;
	} else {
		*cgroupEntryList = cgEntryList;
		if (NULL != availableSubsystems) {
			*availableSubsystems = available;
			Trc_PRT_populateCgroupEntryList_available_subsystems(1, available);
		}
	}

	return rc;
}

/**
 * Reads /proc/<pid>/cgroup to get the name of the cgroup to which the given process
 * belongs, then the $MOUNT_POINT/cgroupName/cgroup.controllers file to get the
 * enabled subsystems.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] pid process id
 * @param[out] cgroupEntryList pointer to OMRCgroupEntry *. On successful return, *cgroupEntry
 * points to a circular linked list. Each element of the list is populated based on the contents
 * of /proc/<pid>/cgroup file.
 * @param[out] availableSubsystems on successful return, contains bitwise-OR of flags of type OMR_CGROUP_SUBSYSTEMS_*
 * indicating the subsystems available for use
 *
 * returns 0 on success, negative code on error
 */
static int32_t
populateCgroupEntryListV2(struct OMRPortLibrary *portLibrary, int pid, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems)
{
	char cgroupFilePath[PATH_MAX];
	char controllerFilePath[PATH_MAX];
	char buffer[PATH_MAX];
	uintptr_t requiredSize = 0;
	FILE *cgroupFile = NULL;
	FILE *controllerFile = NULL;
	OMRCgroupEntry *cgEntryList = NULL;
	uint64_t available = 0;
	int32_t rc = 0;

	char cgroup[PATH_MAX];
	/* This array should be large enough to read names of all subsystems.
	 * 1024 should be enough based on current supported subsystems.
	 */
	char subsystems[PATH_MAX];
	char *cursor = NULL;
	char *newline = NULL;
	char *separator = NULL;

	Assert_PRT_true(NULL != cgroupEntryList);

	requiredSize = portLibrary->str_printf(portLibrary, NULL, 0, "/proc/%d/cgroup", pid);
	Assert_PRT_true(requiredSize <= sizeof(cgroupFilePath));
	portLibrary->str_printf(portLibrary, cgroupFilePath, sizeof(cgroupFilePath), "/proc/%d/cgroup", pid);

	cgroupFile = fopen(cgroupFilePath, "r");
	if (NULL == cgroupFile) {
		int32_t osErrCode = errno;
		Trc_PRT_populateCgroupEntryList_fopen_failed(2, cgroupFilePath, osErrCode);
		rc = portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_FOPEN_FAILED);
		goto _end;
	}

	/* There can be multiple lines in the cgroup file. For v2, the line with the
	 * format '0::<cgroup>' needs to be located.
	 */
	while (0 == feof(cgroupFile)) {
		if (NULL == fgets(buffer, sizeof(buffer), cgroupFile)) {
			break;
		}

		if (0 != ferror(cgroupFile)) {
			int32_t osErrCode = errno;
			Trc_PRT_populateCgroupEntryList_fgets_failed(2, cgroupFilePath, osErrCode);
			rc = portLibrary->error_set_last_error_with_message_format(
					portLibrary,
					OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED,
					"fgets failed to read %s file stream with errno=%d",
					cgroupFilePath,
					osErrCode);
			goto _end;
		}

		rc = sscanf(buffer, PROC_PID_CGROUPV2_ENTRY_FORMAT, cgroup);
		if (1 == rc) {
			break;
		}
	}

	if (1 == rc) {
		/* The controller file consists of a single space-delimited line of controllers/subsystems. */
		requiredSize = portLibrary->str_printf(portLibrary, NULL, 0, "%s/%s/cgroup.controllers", OMR_CGROUP_MOUNT_POINT, cgroup);
		Assert_PRT_true(requiredSize <= sizeof(controllerFilePath));
		portLibrary->str_printf(portLibrary, controllerFilePath, sizeof(controllerFilePath), "%s/%s/cgroup.controllers", OMR_CGROUP_MOUNT_POINT, cgroup);

		controllerFile = fopen(controllerFilePath, "r");
		if (NULL == controllerFile) {
			int32_t osErrCode = errno;
			Trc_PRT_populateCgroupEntryList_fopen_failed(2, controllerFilePath, osErrCode);
			rc = portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_FOPEN_FAILED);
			goto _end;
		}

		if (NULL == fgets(subsystems, sizeof(subsystems), controllerFile)) {
			/* No controllers enabled. */
			rc = 0;
			goto _end;
		}
	} else {
		Trc_PRT_populateCgroupEntryList_unexpected_format(2, cgroupFilePath);
		rc = portLibrary->error_set_last_error_with_message_format(
				portLibrary,
				OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED,
				"unexpected format of %s",
				cgroupFilePath);
		goto _end;
	}

	cursor = subsystems;

	/* Strip the \n at the end (see Linux kernel source, file
	 * kernel/cgroup/cgroup.c, function cgroup_print_ss_mask()).
	 */
	newline = strchr(cursor, '\n');
	if (NULL != newline) {
		*newline = '\0';
	}

	do {
		int32_t i = 0;

		separator = strchr(cursor, ' ');
		if (NULL != separator) {
			*separator = '\0';
		}

		for (i = 0; i < sizeof(supportedSubsystems) / sizeof(supportedSubsystems[0]); i++) {
			if (OMR_ARE_NO_BITS_SET(available, supportedSubsystems[i].flag)
				&& (0 == strcmp(cursor, supportedSubsystems[i].name))
			) {
				/* In cgroup v2, all cgroups are bound to the single unified hierarchy, '0'. */
				rc = addCgroupEntry(portLibrary, &cgEntryList, 0, cursor, cgroup, supportedSubsystems[i].flag);
				if (0 != rc) {
					goto _end;
				}
				available |= supportedSubsystems[i].flag;
			}
		}
		if (NULL != separator) {
			cursor = separator + 1;
		}
	} while (NULL != separator);

	rc = 0;

_end:
	if (NULL != cgroupFile) {
		fclose(cgroupFile);
	}
	if (NULL != controllerFile) {
		fclose(controllerFile);
	}
	if (0 != rc) {
		freeCgroupEntries(portLibrary, cgEntryList);
		cgEntryList = NULL;
	} else {
		*cgroupEntryList = cgEntryList;
		if (NULL != availableSubsystems) {
			*availableSubsystems = available;
			Trc_PRT_populateCgroupEntryList_available_subsystems(2, available);
		}
	}

	return rc;
}

/**
 * Given a subsystem flag, returns enum for that subsystem
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] subsystemFlag flag of type OMR_CGROUP_SUBSYSTEM_*
 *
 * @return value of type enum OMRCgroupSubsystem
 */
static OMRCgroupSubsystem
getCgroupSubsystemFromFlag(uint64_t subsystemFlag)
{
	switch(subsystemFlag) {
	case OMR_CGROUP_SUBSYSTEM_CPU:
		return CPU;
	case OMR_CGROUP_SUBSYSTEM_MEMORY:
		return MEMORY;
	case OMR_CGROUP_SUBSYSTEM_CPUSET:
		return CPUSET;
	default:
		Trc_PRT_Assert_ShouldNeverHappen();
	}

	return INVALID_SUBSYSTEM;
}

/**
 * Returns Absolute Path for the specified file in the cgroup subsystem.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] subsystemFlag flag of type OMR_CGROUP_SUBSYSTEMS_* representing the cgroup subsystem
 * @param[in] fileName name of the file under cgroup subsystem
 * @param[in/out] fullPath Absolute Path for the specified file in the cgroup subsystem
 * @param[in/out] bufferLength to specify the size required for full path
 *
 * @return 0 on success, negative error code on any error
 */
static int32_t
getAbsolutePathOfCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, char *fullPath, intptr_t *bufferLength)
{
	char *cgroup = NULL;
	intptr_t fullPathLen = 0;
	int32_t rc = 0;
	OMRCgroupSubsystem subsystem = getCgroupSubsystemFromFlag(subsystemFlag);
	uint64_t availableSubsystem = portLibrary->sysinfo_cgroup_are_subsystems_available(portLibrary, subsystemFlag);

	if (availableSubsystem != subsystemFlag) {
		Trc_PRT_readCgroupSubsystemFile_subsystem_not_available(subsystemFlag);
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_UNAVAILABLE, "cgroup subsystem %s is not available", subsystemNames[subsystem]);
		goto _end;
	}

	if (NULL == fileName) {
		Trc_PRT_readCgroupSubsystemFile_invalid_fileName();
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_FILENAME_INVALID, "fileName is NULL");
		goto _end;
	}

	cgroup = getCgroupNameForSubsystem(portLibrary, PPG_cgroupEntryList, subsystemNames[subsystem]);
	if (NULL == cgroup) {
		/* If the subsystem is available and supported, cgroup must not be NULL */
		Trc_PRT_readCgroupSubsystemFile_missing_cgroup(subsystemFlag);
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_NAME_NOT_AVAILABLE, "cgroup name for subsystem %s is not available", subsystemNames[MEMORY]);
		Trc_PRT_Assert_ShouldNeverHappen();
		goto _end;
	}

	if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
		/* Absolute path of the file to be read is: $MOUNT_POINT/$SUBSYSTEM_NAME/$CGROUP_NAME/$FILE_NAME. */
		fullPathLen = portLibrary->str_printf(portLibrary, NULL, 0, "%s/%s/%s/%s", OMR_CGROUP_MOUNT_POINT, subsystemNames[subsystem], cgroup, fileName);
		if (fullPathLen > *bufferLength) {
			*bufferLength = fullPathLen;
			rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_STRING_BUFFER_TOO_SMALL, "buffer size should be %d bytes", fullPathLen);
			goto _end;
		}

		portLibrary->str_printf(portLibrary, fullPath, fullPathLen, "%s/%s/%s/%s", OMR_CGROUP_MOUNT_POINT, subsystemNames[subsystem], cgroup, fileName);
	} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
		/* Absolute path of the file to be read is: $MOUNT_POINT/$CGROUP_NAME/$FILE_NAME. */
		fullPathLen = portLibrary->str_printf(portLibrary, NULL, 0, "%s/%s/%s", OMR_CGROUP_MOUNT_POINT, cgroup, fileName);
		if (fullPathLen > *bufferLength) {
			*bufferLength = fullPathLen;
			rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_STRING_BUFFER_TOO_SMALL, "buffer size should be %d bytes", fullPathLen);
			goto _end;
		}

		portLibrary->str_printf(portLibrary, fullPath, fullPathLen, "%s/%s/%s", OMR_CGROUP_MOUNT_POINT, cgroup, fileName);
	} else {
		Trc_PRT_readCgroupSubsystemFile_unsupported_cgroup_version();
		rc = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_VERSION_NOT_AVAILABLE, "cgroup or its version is unsupported");
		goto _end;
	}

_end:
	return rc;
}

/**
 * Returns FILE pointer for the specified file in the cgroup subsystem.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] subsystemFlag flag of type OMR_CGROUP_SUBSYSTEMS_* representing the cgroup subsystem
 * @param[in] fileName name of the file under cgroup subsystem
 * @param[in/out] subsystemFile pointer to FILE * which stores the handle for the specified subsystem file
 *
 * @return 0 on success, negative error code on any error
 */
static int32_t
getHandleOfCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, FILE **subsystemFile)
{
	int32_t rc = 0;
	char fullPathBuf[PATH_MAX];
	char *fullPath = fullPathBuf;
	intptr_t bufferLength = PATH_MAX;
	BOOLEAN allocateMemory = FALSE;
	Assert_PRT_true(NULL != subsystemFile);
	rc = getAbsolutePathOfCgroupSubsystemFile(portLibrary, subsystemFlag, fileName, fullPath, &bufferLength);
	if (OMRPORT_ERROR_STRING_BUFFER_TOO_SMALL == rc) {
		fullPath = portLibrary->mem_allocate_memory(portLibrary, bufferLength, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == fullPath) {
			Trc_PRT_readCgroupSubsystemFile_oom_for_filename();
			rc = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED, "memory allocation for filename buffer failed");
			goto _end;
		}
		allocateMemory = TRUE;
		rc = getAbsolutePathOfCgroupSubsystemFile(portLibrary, subsystemFlag, fileName, fullPath, &bufferLength);
	}
	if (0 == rc) {
		*subsystemFile = fopen(fullPath, "r");
		if (NULL == *subsystemFile) {
			int32_t osErrCode = errno;
			Trc_PRT_readCgroupSubsystemFile_fopen_failed(fullPath, osErrCode);
			if (ENOENT == osErrCode) {
				rc = portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_FILE_NOENT);
			} else {
				rc = portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_FILE_FOPEN_FAILED);
			}
			goto _end;
		}
	}

_end:
	if (allocateMemory) {
		portLibrary->mem_free_memory(portLibrary, fullPath);
	}

	return rc;
}

/**
 * Reads the cgroup metric (if has multiple searched for metric key and reads it) and returns the
 * value to the param value.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary.
 * @param[in] subsystemFlag flag of type OMR_CGROUP_SUBSYSTEMS_* representing the cgroup subsystem.
 * @param[in] fileName name of the file under cgroup subsystem.
 * @param[in] metricKeyInFile name of the cgroup metric which is in a group of metrics in a cgroup
 * subsystem file.
 * @param[in/out] value pointer to char * which stores the value for the specified cgroup subsystem
 * metric.
 * @param[in/out] fileContent double pointer to allocate memory and write file content in case of a
 * multiline file with more than 1 metric available in it.
 *
 * @return 0 on success, negative error code on any error.
 */
static int32_t
readCgroupMetricFromFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, const char *metricKeyInFile, char **fileContent, char *value)
{
	int32_t rc = 0;
	FILE *file = NULL;

	rc = getHandleOfCgroupSubsystemFile(portLibrary, subsystemFlag, fileName, &file);
	if (0 != rc) {
		goto _end;
	}

	if (NULL == metricKeyInFile) {
		/* The file has only one cgroup metric to be read. */
		if (NULL == fgets(value, MAX_LINE_LENGTH, file)) {
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_METRIC_NOT_AVAILABLE;
		}
		goto _end;
	} else {
		/* Read the file to a buffer and pick the metrics from it later. */
		*fileContent = portLibrary->mem_allocate_memory(portLibrary, CGROUP_METRIC_FILE_CONTENT_MAX_LIMIT, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != *fileContent) {
			size_t returnvalue = fread(*fileContent, 1, CGROUP_METRIC_FILE_CONTENT_MAX_LIMIT, file);
			if (0 == returnvalue) {
				portLibrary->mem_free_memory(portLibrary, *fileContent);
				rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_METRIC_NOT_AVAILABLE;
				goto _end;
			}
		} else {
			rc = OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED;
			goto _end;
		}
	}

_end:
	if (NULL != file) {
		/* closing the file pointer */
		fclose(file);
	}
	return rc;
}

/**
 * Read a file under a subsystem in cgroup hierarchy based on the format specified
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] subsystemFlag flag of type OMR_CGROUP_SUBSYSTEMS_* representing the cgroup subsystem
 * @param[in] fileName name of the file under cgroup subsystem
 * @param[in] numItemsToRead number of items to be read from the file
 * @param[in] format format to be used for reading the file
 *
 * @return 0 on successfully reading 'numItemsToRead' items from the file, negative error code on any error
 */
static int32_t
readCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, int32_t numItemsToRead, const char *format, ...)
{
	FILE *file = NULL;
	int32_t rc = 0;
	va_list args;

	rc = getHandleOfCgroupSubsystemFile(portLibrary, subsystemFlag, fileName, &file);
	if (0 != rc) {
		goto _end;
	}

	Assert_PRT_true(NULL != file);

	va_start(args, format);
	rc = vfscanf(file, format, args);
	va_end(args);

	if (numItemsToRead != rc) {
		Trc_PRT_readCgroupSubsystemFile_unexpected_file_format(numItemsToRead, rc);
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpected format of file %s", fileName);
		goto _end;
	} else {
		rc = 0;
	}

_end:
	if (NULL != file) {
		fclose(file);
	}
	return rc;
}

/**
 * Checks if the process is running inside container
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 *
 * @return TRUE if running in a container; otherwise, return FALSE
 */
static BOOLEAN
isRunningInContainer(struct OMRPortLibrary *portLibrary)
{
	if (OMRPORT_PROCESS_IN_CONTAINER_UNINITIALIZED == PPG_processInContainerState) {
		omrthread_monitor_enter(cgroupMonitor);
		if (OMRPORT_PROCESS_IN_CONTAINER_UNINITIALIZED == PPG_processInContainerState) {
			FILE *cgroupFile = NULL;
			int32_t rc = 0;

			/* Assume we are not in a container. */
			BOOLEAN inContainer = FALSE;

			/* Check for existence of files that signify running in a container (Docker and Podman respectively).
			 * Not completely reliable as these files may not be available, but it should work for most cases.
			 */
			if ((0 == access("/.dockerenv", F_OK)) || (0 == access("/run/.containerenv", F_OK))) {
				inContainer = TRUE;
			} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
				/* Read PID 1's cgroup file /proc/1/cgroup and check cgroup name for each subsystem.
				 * If cgroup name for each subsystem points to the root cgroup "/",
				 * then the process is not running in a container.
				 * For any other cgroup name, assume we are in a container.
				 * Note that this will not work if namespaces are enabled in the container as all
				 * cgroup names will be the root cgroup.
				 */
				cgroupFile = fopen(OMR_PROC_PID_ONE_CGROUP_FILE, "r");
				if (NULL == cgroupFile) {
					int32_t osErrCode = errno;
					Trc_PRT_isRunningInContainer_fopen_failed(OMR_PROC_PID_ONE_CGROUP_FILE, osErrCode);
					rc = portLibrary->error_set_last_error_with_message_format(
							portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_FOPEN_FAILED,
							"failed to open %s file with errno=%d", OMR_PROC_PID_ONE_CGROUP_FILE, osErrCode);
					goto _end;
				}

				while (0 == feof(cgroupFile)) {
					char buffer[PATH_MAX];
					char cgroup[PATH_MAX];
					char subsystems[PATH_MAX];
					int32_t hierId = -1;

					if (NULL == fgets(buffer, sizeof(buffer), cgroupFile)) {
						break;
					}
					if (0 != ferror(cgroupFile)) {
						int32_t osErrCode = errno;
						Trc_PRT_isRunningInContainer_fgets_failed(OMR_PROC_PID_ONE_CGROUP_FILE, osErrCode);
						rc = portLibrary->error_set_last_error_with_message_format(
								portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED,
								"failed to read %s file stream with errno=%d", OMR_PROC_PID_ONE_CGROUP_FILE, osErrCode);
						goto _end;
					}
					rc = sscanf(buffer, PROC_PID_CGROUPV1_ENTRY_FORMAT, &hierId, subsystems, cgroup);

					if (EOF == rc) {
						break;
					} else if (1 == rc) {
						rc = sscanf(buffer, PROC_PID_CGROUP_SYSTEMD_ENTRY_FORMAT, &hierId, cgroup);

						if (2 != rc) {
							Trc_PRT_isRunningInContainer_unexpected_format(OMR_PROC_PID_ONE_CGROUP_FILE);
							rc = portLibrary->error_set_last_error_with_message_format(
									portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED,
									"unexpected format of %s file, error code = %d",
									OMR_PROC_PID_ONE_CGROUP_FILE, rc);
							goto _end;
						}
					} else if (3 != rc) {
						Trc_PRT_isRunningInContainer_unexpected_format(OMR_PROC_PID_ONE_CGROUP_FILE);
						rc = portLibrary->error_set_last_error_with_message_format(
								portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED,
								"unexpected format of %s file, error code = %d",
								OMR_PROC_PID_ONE_CGROUP_FILE, rc);
						goto _end;
					}

					if ((0 != strcmp(ROOT_CGROUP, cgroup)) && (0 != strcmp(SYSTEMD_INIT_CGROUP, cgroup))) {
						inContainer = TRUE;
						break;
					}
				}
				rc = 0;
			}

			/* Read the first line in /proc/1/sched if it exists.
			 * Outside a container, the initialization process is either "init" or "systemd".
			 * A containerized environment can be assumed for any other initialization process.
			 */
			if ((0 == rc) && (!inContainer) && (0 == access(OMR_PROC_PID_ONE_SCHED_FILE, F_OK))) {
				char buffer[PATH_MAX];
				FILE *schedFile = fopen(OMR_PROC_PID_ONE_SCHED_FILE, "r");

				if (NULL == schedFile) {
					int32_t osErrCode = errno;
					Trc_PRT_isRunningInContainer_fopen_failed(OMR_PROC_PID_ONE_SCHED_FILE, osErrCode);
					rc = portLibrary->error_set_last_error_with_message_format(
							portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_FOPEN_FAILED,
							"failed to open %s file with errno=%d", OMR_PROC_PID_ONE_SCHED_FILE, osErrCode);
					goto _end;
				}

				if (NULL == fgets(buffer, sizeof(buffer), schedFile)) {
					if (0 != ferror(schedFile)) {
						int32_t osErrCode = errno;
						Trc_PRT_isRunningInContainer_fgets_failed(OMR_PROC_PID_ONE_SCHED_FILE, osErrCode);
						rc = portLibrary->error_set_last_error_with_message_format(
								portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED,
								"failed to read %s file stream with errno=%d",
								OMR_PROC_PID_ONE_SCHED_FILE, osErrCode);
						goto _end;
					}
				} else {
					/* Check if the initialization process is either "init " or "systemd ".
					 * The space after systemd and init allows us to exactly check for those
					 * strings and filter any strings which either begin with systemd or init.
					 *
					 * The first line of /proc/1/sched in a non-containerized environment:
					 * - systemd (1, #threads: 1)
					 * - init (1, #threads: 1)
					 *
					 * The first line of /proc/1/sched in a containerized environment:
					 * - bash (1, #threads: 1)
					 * - sh (1, #threads: 1)
					 */
#define STARTS_WITH(string, prefix) (0 == strncmp(string, prefix, sizeof(prefix) - 1))
					if (!STARTS_WITH(buffer, "init ") && !STARTS_WITH(buffer, "systemd ")) {
						inContainer = TRUE;
					}
#undef STARTS_WITH
				}
			}

_end:
			if (NULL != cgroupFile) {
				fclose(cgroupFile);
			}

			if (0 != rc) {
				PPG_processInContainerState = OMRPORT_PROCESS_IN_CONTAINER_ERROR;
				/* Print a warning message. */
				portLibrary->nls_printf(
						portLibrary, J9NLS_WARNING, J9NLS_PORT_RUNNING_IN_CONTAINER_FAILURE,
						omrerror_last_error_message(portLibrary));
			} else if (inContainer) {
				PPG_processInContainerState = OMRPORT_PROCESS_IN_CONTAINER_TRUE;
			} else {
				PPG_processInContainerState = OMRPORT_PROCESS_IN_CONTAINER_FALSE;
			}
		}
		omrthread_monitor_exit(cgroupMonitor);
	}

	Trc_PRT_isRunningInContainer_container_detected(PPG_processInContainerState);

	return (OMRPORT_PROCESS_IN_CONTAINER_TRUE == PPG_processInContainerState) ? TRUE : FALSE;
}

/**
 * Check the metric limit obtained from a cgroup subsystem file. If it is the string
 * "max", this indicates that the metric is not limited and this function sets val
 * to UINT64_MAX. Otherwise, val is set to the integer value of the metric.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary.
 * @param[in] metricString string obtained from a cgroup subsystem file.
 * @param[out] val pointer to uint64_t which on successful return contains
 *      the value UINT64_MAX if the metric is not limited, and the integer
 *      value of the metric otherwise.
 *
 * @return 0 on success, otherwise negative error code.
 */
static int32_t
scanCgroupIntOrMax(struct OMRPortLibrary *portLibrary, const char *metricString, uint64_t *val)
{
	int32_t rc = 0;

	if (NULL == metricString || NULL == val) {
		Trc_PRT_scanCgroupIntOrMax_null_param(metricString, val);
		rc = portLibrary->error_set_last_error_with_message_format(
				portLibrary,
				OMRPORT_ERROR_SYSINFO_CGROUP_NULL_PARAM,
				"a parameter is null: metricString=%s val=%p",
				metricString,
				val);
		goto _end;
	}

	if (0 == strcmp(metricString, "max")) {
		*val = UINT64_MAX;
	} else {
		rc = sscanf(metricString, "%" SCNu64, val);
		if (1 != rc) {
			Trc_PRT_scanCgroupIntOrMax_scan_failed(metricString, rc);
			rc = portLibrary->error_set_last_error_with_message_format(
					portLibrary,
					OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED,
					"unexpected format of %s",
					metricString);
		} else {
			rc = 0;
		}
	}

_end:
	return rc;
}

/**
 * Read the cgroup memory limit from the memory subsystem file. For cgroup v1, it
 * will always be a positive integer. For cgroup v2, the value in the file is
 * either a positive integer or "max" (which indicates no limit).
 *
 * @param[in] portLibrary pointer to OMRPortLibrary.
 * @param[in] fileName name of the file under cgroup subsystem.
 * @param[out] limit pointer to uint64_t which on successful return contains
 *      the cgroup memory limit.
 *
 * @return 0 on success, otherwise negative error code.
 */
static int32_t
readCgroupMemoryFileIntOrMax(struct OMRPortLibrary *portLibrary, const char *fileName, uint64_t *limit)
{
	/* Memory limit files contain one value. */
	int32_t numItemsToRead = 1;
	int32_t rc = 0;

	if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
		rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, fileName, numItemsToRead, "%" SCNu64, limit);
		if (0 != rc) {
			Trc_PRT_readCgroupMemoryFileIntOrMax_read_failed(fileName, rc);
			goto _end;
		}
	} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
		char memLimitString[MAX_64BIT_INT_LENGTH];
		rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, fileName, numItemsToRead, "%s", memLimitString);
		if (0 != rc) {
			Trc_PRT_readCgroupMemoryFileIntOrMax_read_failed(fileName, rc);
			goto _end;
		}
		rc = scanCgroupIntOrMax(portLibrary, memLimitString, limit);
	} else {
		Trc_PRT_readCgroupMemoryFileIntOrMax_unsupported_cgroup_version();
		rc = portLibrary->error_set_last_error_with_message(
				portLibrary,
				OMRPORT_ERROR_SYSINFO_CGROUP_VERSION_NOT_AVAILABLE,
				"cgroup or its version is unsupported");
	}

_end:
	return rc;
}

/**
 * Get the cgroup memory limit from the memory subsystem file. This will either be
 * memory.limit_in_bytes for cgroup v1 or memory.max for cgroup v2.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[out] limit pointer to uint64_t which on successful return contains
 *      the cgroup memory limit
 *
 * @return 0 on success, otherwise negative error code
 */
static int32_t
getCgroupMemoryLimit(struct OMRPortLibrary *portLibrary, uint64_t *limit)
{
	uint64_t cgroupMemLimit = 0;
	uint64_t physicalMemLimit = 0;
	const char *fileName = NULL;
	int32_t rc = 0;

	Trc_PRT_sysinfo_cgroup_get_memlimit_Entry();

	if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
		fileName = "memory.limit_in_bytes";
	} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
		fileName = "memory.max";
	}

	rc = readCgroupMemoryFileIntOrMax(portLibrary, fileName, &cgroupMemLimit);
	if (0 != rc) {
		Trc_PRT_sysinfo_cgroup_get_memlimit_memory_limit_read_failed(fileName, rc);
		goto _end;
	}

	physicalMemLimit = getPhysicalMemory(portLibrary);
	/* If the cgroup is not imposing any memory limit then the value in memory.limit_in_bytes
	 * is close to max value of 64-bit integer, and is more than the physical memory in the system.
	 */
	if (cgroupMemLimit > physicalMemLimit) {
		Trc_PRT_sysinfo_cgroup_get_memlimit_unlimited();
		rc = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_MEMLIMIT_NOT_SET, "memory limit is not set");
				goto _end;
	}
	if (NULL != limit) {
		*limit = cgroupMemLimit;
	}

_end:
	Trc_PRT_sysinfo_cgroup_get_memlimit_Exit(rc);
	return rc;
}

/**
 * Get the cgroup subsystem metric map and its number of elements given the
 * subsystem flag and the cgroup version in use.
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] subsystem cgroup subsystem flag
 * @param[out] subsystemMetricMap if not null, on successful return is assigned
 *      the metric map associated with the subsystem flag
 * @param[out] numElements if not null, on successful return is assigned the
 *      number of elements of the metric map associated with the subsystem flag
 *
 * @return 0 on success, otherwise negative error code
 */
static int32_t
getCgroupSubsystemMetricMap(struct OMRPortLibrary *portLibrary, uint64_t subsystem, const struct OMRCgroupSubsystemMetricMap **subsystemMetricMap, uint32_t *numElements)
{
	const struct OMRCgroupSubsystemMetricMap *map = NULL;
	uint32_t count = 0;
	int32_t rc = 0;

	Assert_PRT_true((NULL != subsystemMetricMap) || (NULL != numElements));
	if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
		switch (subsystem) {
		case OMR_CGROUP_SUBSYSTEM_MEMORY:
			map = omrCgroupMemoryMetricMapV1;
			count = OMR_CGROUP_MEMORY_METRIC_MAP_V1_SIZE;
			break;
		case OMR_CGROUP_SUBSYSTEM_CPU:
			map = omrCgroupCpuMetricMapV1;
			count = OMR_CGROUP_CPU_METRIC_MAP_V1_SIZE;
			break;
		case OMR_CGROUP_SUBSYSTEM_CPUSET:
			map = omrCgroupCpusetMetricMapV1;
			count = OMR_CGROUP_CPUSET_METRIC_MAP_V1_SIZE;
			break;
		default:
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_UNAVAILABLE;
		}
	} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
		switch (subsystem) {
		case OMR_CGROUP_SUBSYSTEM_MEMORY:
			map = omrCgroupMemoryMetricMapV2;
			count = OMR_CGROUP_MEMORY_METRIC_MAP_V2_SIZE;
			break;
		case OMR_CGROUP_SUBSYSTEM_CPU:
			map = omrCgroupCpuMetricMapV2;
			count = OMR_CGROUP_CPU_METRIC_MAP_V2_SIZE;
			break;
		case OMR_CGROUP_SUBSYSTEM_CPUSET:
			map = omrCgroupCpusetMetricMapV2;
			count = OMR_CGROUP_CPUSET_METRIC_MAP_V2_SIZE;
			break;
		default:
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_UNAVAILABLE;
		}
	} else {
		Trc_PRT_Assert_ShouldNeverHappen();
	}

	if (NULL != subsystemMetricMap) {
		*subsystemMetricMap = map;
	}
	if (NULL != numElements) {
		*numElements = count;
	}

	return rc;
}

#endif /* defined(LINUX) && !defined(OMRZTPF) */

BOOLEAN
omrsysinfo_cgroup_is_system_available(struct OMRPortLibrary *portLibrary)
{
	BOOLEAN result = FALSE;
#if defined(LINUX) && !defined(OMRZTPF)
	int32_t rc = 0;

	Trc_PRT_sysinfo_cgroup_is_system_available_Entry();
	if (NULL == PPG_cgroupEntryList) {
		if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
			BOOLEAN inContainer = isRunningInContainer(portLibrary);

			if (NULL == PPG_cgroupEntryList) {
				omrthread_monitor_enter(cgroupMonitor);
				if (NULL == PPG_cgroupEntryList) {
					rc = populateCgroupEntryListV1(
							portLibrary, getpid(), inContainer,
							&PPG_cgroupEntryList, &PPG_cgroupSubsystemsAvailable);
				}
				omrthread_monitor_exit(cgroupMonitor);
			}
		} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
			if (NULL == PPG_cgroupEntryList) {
				omrthread_monitor_enter(cgroupMonitor);
				if (NULL == PPG_cgroupEntryList) {
					rc = populateCgroupEntryListV2(
							portLibrary, getpid(),
							&PPG_cgroupEntryList, &PPG_cgroupSubsystemsAvailable);
				}
				omrthread_monitor_exit(cgroupMonitor);
			}
		} else {
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_UNSUPPORTED_PLATFORM;
		}
	}

	if (0 == rc) {
		Trc_PRT_sysinfo_cgroup_is_system_available_subsystems(PPG_cgroupSubsystemsAvailable);
		result = TRUE;
	} else {
		PPG_cgroupSubsystemsAvailable = 0;
	}
	Trc_PRT_sysinfo_cgroup_is_system_available_Exit((uintptr_t)result);
#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return result;
}

uint64_t
omrsysinfo_cgroup_get_available_subsystems(struct OMRPortLibrary *portLibrary)
{
#if defined(LINUX) && !defined(OMRZTPF)
	Trc_PRT_sysinfo_cgroup_get_available_subsystems_Entry();
	if (NULL == PPG_cgroupEntryList) {
		portLibrary->sysinfo_cgroup_is_system_available(portLibrary);
	}
	Trc_PRT_sysinfo_cgroup_get_available_subsystems_Exit(PPG_cgroupSubsystemsAvailable);
	return PPG_cgroupSubsystemsAvailable;
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return 0;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

uint64_t
omrsysinfo_cgroup_are_subsystems_available(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlags)
{
#if defined(LINUX) && !defined(OMRZTPF)
	uint64_t available = 0;
	uint64_t rc = 0;

	Trc_PRT_sysinfo_cgroup_are_subsystems_available_Entry(subsystemFlags);

	available = portLibrary->sysinfo_cgroup_get_available_subsystems(portLibrary);
	rc = available & subsystemFlags;

	Trc_PRT_sysinfo_cgroup_are_subsystems_available_Exit(rc);

	return rc;
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return 0;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

uint64_t
omrsysinfo_cgroup_get_enabled_subsystems(struct OMRPortLibrary *portLibrary)
{
#if defined(LINUX) && !defined(OMRZTPF)
	return PPG_cgroupSubsystemsEnabled;
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return 0;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

uint64_t
omrsysinfo_cgroup_enable_subsystems(struct OMRPortLibrary *portLibrary, uint64_t requestedSubsystems)
{
#if defined(LINUX) && !defined(OMRZTPF)
	uint64_t available = 0;

	Trc_PRT_sysinfo_cgroup_enable_subsystems_Entry(requestedSubsystems);

	available = portLibrary->sysinfo_cgroup_get_available_subsystems(portLibrary);
	PPG_cgroupSubsystemsEnabled = available & requestedSubsystems;

	Trc_PRT_sysinfo_cgroup_enable_subsystems_Exit(PPG_cgroupSubsystemsEnabled);

	return PPG_cgroupSubsystemsEnabled;
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return 0;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

uint64_t
omrsysinfo_cgroup_are_subsystems_enabled(struct OMRPortLibrary *portLibrary, uint64_t subsystemsFlags)
{
#if defined(LINUX) && !defined(OMRZTPF)
	return (PPG_cgroupSubsystemsEnabled & subsystemsFlags);
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return 0;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

int32_t
omrsysinfo_cgroup_get_memlimit(struct OMRPortLibrary *portLibrary, uint64_t *limit)
{
	int32_t rc = OMRPORT_ERROR_SYSINFO_CGROUP_UNSUPPORTED_PLATFORM;

	Assert_PRT_true(NULL != limit);

#if defined(LINUX) && !defined(OMRZTPF)
	rc = getCgroupMemoryLimit(portLibrary, limit);
#endif /* defined(LINUX) && !defined(OMRZTPF) */

	return rc;
}

BOOLEAN
omrsysinfo_cgroup_is_memlimit_set(struct OMRPortLibrary *portLibrary)
{
#if defined(LINUX) && !defined(OMRZTPF)
	int32_t rc = getCgroupMemoryLimit(portLibrary, NULL);
	if (0 == rc) {
		return TRUE;
	} else {
		return FALSE;
	}
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return FALSE;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

/*
 * Returns the cgroup subsystems list
 */
struct OMRCgroupEntry *
omrsysinfo_get_cgroup_subsystem_list(struct OMRPortLibrary *portLibrary)
{
#if defined(LINUX) && !defined(OMRZTPF)
	return PPG_cgroupEntryList;
#else
	return NULL;
#endif
}

/*
 * Gets if the Runtime is running in a Container by assigning the BOOLEAN value to inContainer param
 * Returns TRUE if running inside a container and FALSE if not or if an error occurs
 */
BOOLEAN
omrsysinfo_is_running_in_container(struct OMRPortLibrary *portLibrary)
{
#if defined(LINUX) && !defined(OMRZTPF)
	return isRunningInContainer(portLibrary);
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return FALSE;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

int32_t
omrsysinfo_cgroup_subsystem_iterator_init(struct OMRPortLibrary *portLibrary, uint64_t subsystem, struct OMRCgroupMetricIteratorState *state)
{
	Assert_PRT_true(NULL != state);
	int32_t rc = OMRPORT_ERROR_SYSINFO_CGROUP_UNSUPPORTED_PLATFORM;
#if defined(LINUX) && !defined(OMRZTPF)
	state->count = 0;
	state->subsystemid = subsystem;
	state->fileMetricCounter = 0;

	rc = getCgroupSubsystemMetricMap(portLibrary, state->subsystemid, NULL, &state->numElements);
#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return rc;
}

BOOLEAN
omrsysinfo_cgroup_subsystem_iterator_hasNext(struct OMRPortLibrary *portLibrary, const struct OMRCgroupMetricIteratorState *state)
{
	BOOLEAN check = FALSE;
#if defined(LINUX) && !defined(OMRZTPF)
	check = state->count < state->numElements;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return check;
}

int32_t
omrsysinfo_cgroup_subsystem_iterator_metricKey(struct OMRPortLibrary *portLibrary, const struct OMRCgroupMetricIteratorState *state, const char **metricKey)
{
	int32_t rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_METRIC_NOT_AVAILABLE;
#if defined(LINUX) && !defined(OMRZTPF)
	if (NULL != metricKey) {
		const struct OMRCgroupSubsystemMetricMap *subsystemMetricMap = NULL;
		rc = getCgroupSubsystemMetricMap(portLibrary, state->subsystemid, &subsystemMetricMap, NULL);
		if (0 != rc) {
			goto _end;
		}
		if (state->fileMetricCounter < subsystemMetricMap[state->count].metricElementsCount){
			*metricKey = (subsystemMetricMap[state->count].metricInfoElementList + state->fileMetricCounter)->metricTag;
			rc = 0;
		} else {
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_METRIC_NOT_AVAILABLE;
		}
	}
_end:
#endif
	return rc;
}

int32_t
omrsysinfo_cgroup_subsystem_iterator_next(struct OMRPortLibrary *portLibrary, struct OMRCgroupMetricIteratorState *state, struct OMRCgroupMetricElement *metricElement)
{
	int32_t rc = OMRPORT_ERROR_SYSINFO_CGROUP_UNSUPPORTED_PLATFORM;
#if defined(LINUX) && !defined(OMRZTPF)
	struct OMRCgroupMetricInfoElement *currentElement = NULL;
	const struct OMRCgroupSubsystemMetricMap *subsystemMetricMap = NULL;
	const struct OMRCgroupSubsystemMetricMap *subsystemMetricMapElement = NULL;
	if (state->count >= state->numElements) {
		goto _end;
	}
	rc = getCgroupSubsystemMetricMap(portLibrary, state->subsystemid, &subsystemMetricMap, NULL);
	if (0 != rc) {
		state->count += 1;
		goto _end;
	}
	subsystemMetricMapElement = &subsystemMetricMap[state->count];
	currentElement = subsystemMetricMapElement->metricInfoElementList;
	if (NULL == state->fileContent) {
		Assert_PRT_true(0 == state->fileMetricCounter);
		rc = readCgroupMetricFromFile(portLibrary, state->subsystemid, subsystemMetricMapElement->metricFileName, currentElement->metricKeyInFile, &(state->fileContent), metricElement->value);

		/* In error condition or single metric condition we increment the counter to continue to next metric */
		if ((0 != rc) || (NULL == state->fileContent)) {
			state->fileContent = NULL;
			state->count += 1;
			goto _end;
		}
	}

	if (state->fileMetricCounter < subsystemMetricMapElement->metricElementsCount) {
		char *current = state->fileContent;
		currentElement += state->fileMetricCounter;

		if ((OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE))
			&& (CGROUP_METRIC_NO_KEY == (uintptr_t)currentElement->metricKeyInFile)
		) {
			/* File contains space-separated values with no keys. */
			char *valueEnd = NULL;
			int32_t i = 0;
			for (i = 0; i < state->fileMetricCounter; i++) {
				/* Skip values that were already read. */
				while (' ' != *current) {
					current += 1;
				}
				current += 1;
			}
			valueEnd = current;
			/* Find the last char of the value. */
			while ((' ' != *valueEnd) && ('\n' != *valueEnd)) {
				valueEnd += 1;
			}
			Assert_PRT_true((valueEnd - current + 1) <= sizeof(metricElement->value));
			strncpy(metricElement->value, current, valueEnd - current);
			metricElement->value[valueEnd - current] = '\0';
			rc = 0;
		} else {
			BOOLEAN isMetricFound = FALSE;
			char *end = state->fileContent + strlen(state->fileContent) - 1;
			int32_t metricKeyLen = 0;
			metricKeyLen = strlen(currentElement->metricKeyInFile);
			while (current < end) {
				if (0 == strncmp(current, currentElement->metricKeyInFile, metricKeyLen)) {
					char *newLine = current;
					current += metricKeyLen;
					/* Advance past any whitespace. */
					while ((current != end) &&  (' ' == *current)) {
						current += 1;
					}
					/* Find the newline. */
					newLine = current;
					while ('\n' != *newLine) {
						newLine += 1;
					}
					Assert_PRT_true((newLine - current + 1) <= sizeof(metricElement->value));
					strncpy(metricElement->value, current, newLine - current);
					metricElement->value[newLine - current] = '\0';
					isMetricFound = TRUE;
					rc = 0;
					break;
				}
				current += 1;
			}
			/* Return an error code if the metric is not found after completely parsing fileContent. */
			if (!isMetricFound) {
				rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_METRIC_NOT_AVAILABLE;
			}
		}

		state->fileMetricCounter += 1;
	}

	if (state->fileMetricCounter >= subsystemMetricMapElement->metricElementsCount) {
		if (NULL != state->fileContent) {
			portLibrary->mem_free_memory(portLibrary, state->fileContent);
			state->fileContent = NULL;
		}
		state->count += 1;
		/* Reset fileMetricCounter here so that a call to omrsysinfo_cgroup_subsystem_iterator_metricKey
		 * immediately after won't fail.
		 */
		state->fileMetricCounter = 0;
	}

_end:
	if (0 == rc) {
		/**
		 * 'value' may have new line at the end (fgets add '\n' at the end)
		 * which is not required so we could remove it
		 */
		size_t len = strlen(metricElement->value);
		metricElement->units = currentElement->metricUnit;
		if ((len > 1) && (metricElement->value[len-1] == '\n')) {
			metricElement->value[len-1] = '\0';
		}
		if (currentElement->isValueToBeChecked) {
			BOOLEAN notSet = FALSE;
			if ('\n' == metricElement->value[0]) {
				/* Empty file, see cpuset.cpus and cpuset.mems in omrCgroupCpusetMetricMapV2. */
				notSet = TRUE;
			} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V1_AVAILABLE)) {
				int64_t result = 0;
				rc = sscanf(metricElement->value, "%" PRId64, &result);
				if (1 != rc) {
					rc = portLibrary->error_set_last_error_with_message_format(
							portLibrary,
							OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_FILE_INVALID_VALUE,
							"invalid value %s in file %s",
							metricElement->value,
							subsystemMetricMapElement->metricFileName);
				} else {
					if ((result > MAX_DEFAULT_VALUE_CHECK) || (result < 0)) {
						notSet = TRUE;
					}
					rc = 0;
				}
			} else if (OMR_ARE_ANY_BITS_SET(PPG_sysinfoControlFlags, OMRPORT_SYSINFO_CGROUP_V2_AVAILABLE)) {
				uint64_t uresult = 0;
				rc = scanCgroupIntOrMax(portLibrary, metricElement->value, &uresult);
				if (0 != rc) {
					rc = portLibrary->error_set_last_error_with_message_format(
							portLibrary,
							OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_FILE_INVALID_VALUE,
							"invalid value %s in file %s",
							metricElement->value,
							subsystemMetricMapElement->metricFileName);
				} else if (uresult > MAX_DEFAULT_VALUE_CHECK) {
					/* Assume that the metric is effectively unlimited and can be considered "Not Set". */
					notSet = TRUE;
				}
			} else {
				Trc_PRT_Assert_ShouldNeverHappen();
			}
			/* Check that the metric is set and that it is a reasonable value. */
			if (notSet) {
				metricElement->units = NULL;
				strcpy(metricElement->value, "Not Set");
			}
		}
	}

#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return rc;
}

void
omrsysinfo_cgroup_subsystem_iterator_destroy(struct OMRPortLibrary *portLibrary, struct OMRCgroupMetricIteratorState *state)
{
	if (NULL != state->fileContent) {
		portLibrary->mem_free_memory(portLibrary, state->fileContent);
		state->fileContent = NULL;
	}
}

#if defined(OMRZTPF)
/*
 * Return the number of I-streams ("processors", as called by other
 * systems) in an unsigned integer as detected at IPL time.
 */
uintptr_t
get_IPL_IstreamCount(void) {
	struct dctist *ist = (struct dctist *)cinfc_fast(CINFC_CMMIST);
	int numberOfIStreams = ist->istactis;
	return (uintptr_t)numberOfIStreams;
}

/*
 * Return the number of I-streams ("processors", as called by other
 * systems) in an unsigned integer as detect at Process Dispatch time.
 */
uintptr_t
get_Dispatch_IstreamCount(void) {
	struct dctist *ist = (struct dctist *)cinfc_fast(CINFC_CMMIST);
	int numberOfIStreams = ist->istuseis;
	return (uintptr_t)numberOfIStreams;
}
#endif /* defined(OMRZTPF) */

int32_t
omrsysinfo_get_process_start_time(struct OMRPortLibrary *portLibrary, uintptr_t pid, uint64_t *processStartTimeInNanoseconds)
{
	int32_t rc = 0;
	uint64_t computedProcessStartTimeInNanoseconds = 0;
	Trc_PRT_sysinfo_get_process_start_time_enter(pid);
	if (0 != omrsysinfo_process_exists(portLibrary, pid)) {
#if defined(LINUX)
		char procDir[OMRPORT_SYSINFO_PROC_DIR_BUFFER_SIZE] = {0};
		struct stat st;
		snprintf(procDir, sizeof(procDir), "/proc/%" PRIuPTR, pid);
		if (-1 == stat(procDir, &st)) {
			rc = OMRPORT_ERROR_SYSINFO_ERROR_GETTING_PROCESS_START_TIME;
			goto done;
		}
		computedProcessStartTimeInNanoseconds = (uint64_t)st.st_mtime * OMRPORT_TIME_DELTA_IN_NANOSECONDS + st.st_mtim.tv_nsec;
#elif defined(OSX) /* defined(LINUX) */
		int mib[OMRPORT_SYSINFO_NUM_SYSCTL_ARGS] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
		size_t len = sizeof(struct kinfo_proc);
		struct kinfo_proc procInfo;
		if (-1 == sysctl(mib, OMRPORT_SYSINFO_NUM_SYSCTL_ARGS, &procInfo, &len, NULL, 0)) {
			rc = OMRPORT_ERROR_SYSINFO_ERROR_GETTING_PROCESS_START_TIME;
			goto done;
		}
		if (0 == len) {
			rc = OMRPORT_ERROR_SYSINFO_NONEXISTING_PROCESS;
			goto done;
		}
		computedProcessStartTimeInNanoseconds =
			((uint64_t)procInfo.kp_proc.p_starttime.tv_sec * OMRPORT_TIME_DELTA_IN_NANOSECONDS) +
			((uint64_t)procInfo.kp_proc.p_starttime.tv_usec * OMRPORT_SYSINFO_NANOSECONDS_PER_MICROSECOND);
#elif defined(AIXPPC) /* defined(OSX) */
		pid_t convertedPid = (pid_t)pid;
		struct procsinfo procInfos[] = {0};
		int ret = getprocs(procInfos, sizeof(procInfos[0]), NULL, 0, &convertedPid, sizeof(procInfos) / sizeof(procInfos[0]));
		if (-1 == ret) {
			rc = OMRPORT_ERROR_SYSINFO_ERROR_GETTING_PROCESS_START_TIME;
			goto done;
		} else if (0 == ret) {
			rc = OMRPORT_ERROR_SYSINFO_NONEXISTING_PROCESS;
			goto done;
		}
		computedProcessStartTimeInNanoseconds = (uint64_t)(procInfos[0].pi_start) * OMRPORT_TIME_DELTA_IN_NANOSECONDS;
#elif defined(J9ZOS390) /* defined(AIXPPC) */
		pgtha pgtha;
		ProcessData processData;
		pgthc *currentProcessInfo = NULL;
		uint32_t dataOffset = 0;
		uint32_t inputSize = sizeof(pgtha);
		unsigned char *input = (unsigned char *)&pgtha;
		uint32_t outputSize = sizeof(ProcessData);
		unsigned char *output = (unsigned char *)&processData;
		uint32_t ret = 0;
		uint32_t retCode = 0;
		uint32_t reasonCode = 0;
		memset(input, 0, sizeof(pgtha));
		memset(output, 0, sizeof(processData));
		pgtha.pid = pid;
		pgtha.accesspid = PGTHA_ACCESS_CURRENT;
		pgtha.flag1 = PGTHA_FLAG_PROCESS_DATA;
		GETTHENT(&inputSize, &input, &outputSize, &output, &ret, &retCode, &reasonCode);
		if (-1 == ret) {
			rc = OMRPORT_ERROR_SYSINFO_ERROR_GETTING_PROCESS_START_TIME;
			goto done;
		}
		dataOffset = *((unsigned int *)processData.pgthb.offc);
		dataOffset = (dataOffset & I_32_MAX) >> 8;
		currentProcessInfo = (pgthc *)(((char *)&processData) + dataOffset);
		computedProcessStartTimeInNanoseconds = (uint64_t)currentProcessInfo->starttime * OMRPORT_TIME_DELTA_IN_NANOSECONDS;
#else /* defined(J9ZOS390) */
		rc = OMRPORT_ERROR_NOT_SUPPORTED_ON_THIS_PLATFORM;
#endif /* defined(LINUX) */
	} else {
		rc = OMRPORT_ERROR_SYSINFO_NONEXISTING_PROCESS;
	}
done:
	*processStartTimeInNanoseconds = computedProcessStartTimeInNanoseconds;
	Trc_PRT_sysinfo_get_process_start_time_exit(pid, computedProcessStartTimeInNanoseconds, rc);
	return rc;
}

int32_t
omrsysinfo_get_number_context_switches(struct OMRPortLibrary *portLibrary, uint64_t *numSwitches)
{
#if defined(LINUX)
	char buf[128];
	int32_t rc = 0;
	intptr_t fd = portLibrary->file_open(portLibrary, "/proc/stat", EsOpenRead, 0);
	if (-1 == fd) {
		return portLibrary->error_last_error_number(portLibrary);
	}

	while (NULL != portLibrary->file_read_text(portLibrary, fd, buf, sizeof(buf))) {
		if (1 == sscanf(buf, "ctxt %" SCNu64, numSwitches)) {
			goto done;
		}
	}

	rc = OMRPORT_ERROR_SYSINFO_GET_STATS_FAILED;
done:
	portLibrary->file_close(portLibrary, fd);
	return rc;
#else /* defined(LINUX) */
	return OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED;
#endif /* defined(LINUX) */
}
