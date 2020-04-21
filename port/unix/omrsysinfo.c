/*******************************************************************************
 * Copyright (c) 2015, 2020 IBM Corp. and others
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
#include "omrsimap.h"
#endif /* defined(J9ZOS390) */

#if defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(J9ZTPF))
#include "auxv.h"
#include <strings.h>
#endif /* defined(LINUXPPC) || (defined(S390) && defined(LINUX) && !defined(J9ZTPF)) */

#if (defined(S390))
#include "omrportpriv.h"
#include "omrportpg.h"
#endif /* defined(S390) */

#if defined(AIXPPC)
#include <fcntl.h>
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
#elif defined(OSX)
#include <sys/sysctl.h>
#endif /* defined(LINUX) && !defined(OMRZTPF) */

#include <unistd.h>

#if defined(LINUX)
#include "omrcgroup.h"
#endif /* defined(LINUX) */
#include "omrportpriv.h"
#include "omrportpg.h"
#include "omrportptb.h"
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
#include "atoe.h"

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

/* For the omrsysinfo_env_iterator */
extern char **environ;

static uintptr_t copyEnvToBuffer(struct OMRPortLibrary *portLibrary, void *args);
static uintptr_t copyEnvToBufferSignalHandler(struct OMRPortLibrary *portLib, uint32_t gpType, void *gpInfo, void *unUsed);

static void setPortableError(OMRPortLibrary *portLibrary, const char *funcName, int32_t portlibErrno, int systemErrno);

#if (defined(LINUXPPC) || defined(AIXPPC))
static OMRProcessorArchitecture omrsysinfo_map_ppc_processor(const char *processorName);
#endif /* (defined(LINUXPPC) || defined(AIXPPC)) */

#if (defined(AIXPPC) || defined(S390) || defined(J9ZOS390))
static void omrsysinfo_set_feature(OMRProcessorDesc *desc, uint32_t feature);
#endif /* defined(AIXPPC) || defined(S390) || defined(J9ZOS390) */

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

#if !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1)
/* both i 7.1 and i 7.2 do not support this function */
#if !defined(SC_TM_VER)
#define SC_TM_VER 59
#endif  /* !defined(SC_TM_VER) */

#if !defined(__power_tm)
#define __power_tm() ((long)getsystemcfg(SC_TM_VER) > 0) /* taken from AIX 7.1 sys/systemcfg.h */
#endif  /* !defined(__power_tm) */
#endif /* !defined(J9OS_I5_V7R2) && !defined(J9OS_I5_V6R1) */

#if (defined(S390) || defined(J9ZOS390) || defined(J9ZTPF))
static BOOLEAN omrsysinfo_test_stfle(struct OMRPortLibrary *portLibrary, uint64_t stfleBit);
static intptr_t omrsysinfo_get_s390_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
#endif /* defined(S390) || defined(J9ZOS390) || defined(J9ZTPF) */

#if defined(RISCV)
static intptr_t omrsysinfo_get_riscv_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
#endif

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

#define OMR_CGROUP_V1_MOUNT_POINT "/sys/fs/cgroup"
#define ROOT_CGROUP "/"
#define SYSTEMD_INIT_CGROUP "/init.scope"
#define OMR_PROC_PID_ONE_CGROUP_FILE "/proc/1/cgroup"
#define MAX_DEFAULT_VALUE_CHECK (LLONG_MAX - (1024 * 1024 * 1024)) /* subtracting the MAX page size (1GB) from LLONG_MAX to check against a value */
#define CGROUP_METRIC_FILE_CONTENT_MAX_LIMIT 1024

/* An entry in /proc/<pid>/cgroup is of following form:
 *  <hierarchy ID>:<subsystem>[,<subsystem>]*:<cgroup name>
 *
 * An example:
 *  7:cpuacct,cpu:/mycgroup
 */
#define PROC_PID_CGROUP_ENTRY_FORMAT "%d:%[^:]:%s"
#define PROC_PID_CGROUP_SYSTEMD_ENTRY_FORMAT "%d::%s"

#define SINGLE_CGROUP_METRIC 1

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

static struct OMRCgroupMetricInfoElement oomControlMetricElementList[] = {
	{ "OOM Killer Disabled", "oom_kill_disable", NULL, FALSE },
	{ "Under OOM", "under_oom", NULL, FALSE }
};

static struct OMRCgroupMetricInfoElement cpuStatMetricElementList[] = {
	{ "Period intervals elapsed count", "nr_periods", NULL, FALSE },
	{ "Throttled count", "nr_throttled", NULL, FALSE },
	{ "Total throttle time", "throttled_time", "nanoseconds", FALSE }
};

typedef struct OMRCgroupSubsystemMetricMap {
	char *metricFileName;
	OMRCgroupMetricInfoElement *metricInfoElementList;
	int32_t metricElementsCount;
} OMRCgroupSubsystemMetricMap;

static struct OMRCgroupSubsystemMetricMap omrCgroupMemoryMetricMap[] = {
	{ "memory.limit_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory Limit", NULL, "bytes", TRUE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.limit_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory + Swap Limit", NULL, "bytes", TRUE }, SINGLE_CGROUP_METRIC },
	{ "memory.usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory + Swap Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.max_usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory Max Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.max_usage_in_bytes", &(OMRCgroupMetricInfoElement){ "Memory + Swap Max Usage", NULL, "bytes", FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.failcnt", &(OMRCgroupMetricInfoElement){ "Memory limit exceeded count", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.memsw.failcnt", &(OMRCgroupMetricInfoElement){ "Memory + Swap limit exceeded count", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "memory.oom_control", &oomControlMetricElementList[0], sizeof(oomControlMetricElementList)/sizeof(oomControlMetricElementList[0]) }
};

static struct OMRCgroupSubsystemMetricMap omrCgroupCpuMetricMap[] = {
	{ "cpu.cfs_period_us", &(OMRCgroupMetricInfoElement){ "CPU Period", NULL, "microseconds", FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpu.cfs_quota_us", &(OMRCgroupMetricInfoElement){ "CPU Quota", NULL, "microseconds", TRUE }, SINGLE_CGROUP_METRIC },
	{ "cpu.shares", &(OMRCgroupMetricInfoElement){ "CPU Shares", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpu.stat", &cpuStatMetricElementList[0], sizeof(cpuStatMetricElementList)/sizeof(cpuStatMetricElementList[0]) }
};

static struct OMRCgroupSubsystemMetricMap omrCgroupCpusetMetricMap[] = {
	{ "cpuset.cpu_exclusive", &(OMRCgroupMetricInfoElement){ "CPU exclusive", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.mem_exclusive", &(OMRCgroupMetricInfoElement){ "Mem exclusive", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.cpus", &(OMRCgroupMetricInfoElement){ "CPUs", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC },
	{ "cpuset.mems", &(OMRCgroupMetricInfoElement){ "Mems", NULL, NULL, FALSE }, SINGLE_CGROUP_METRIC }
};

static uint32_t attachedPortLibraries;
static omrthread_monitor_t cgroupEntryListMonitor;
#endif /* defined(LINUX) */

static intptr_t cwdname(struct OMRPortLibrary *portLibrary, char **result);
static uint32_t getLimitSharedMemory(struct OMRPortLibrary *portLibrary, uint64_t *limit);
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
static void freeCgroupEntries(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList);
static char * getCgroupNameForSubsystem(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList, const char *subsystem);
static int32_t addCgroupEntry(struct OMRPortLibrary *portLibrary, OMRCgroupEntry **cgEntryList, int32_t hierId, const char *subsystem, const char *cgroupName, uint64_t flag);
static int32_t readCgroupFile(struct OMRPortLibrary *portLibrary, int pid, BOOLEAN inContainer, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems);
static OMRCgroupSubsystem getCgroupSubsystemFromFlag(uint64_t subsystemFlag);
static int32_t getAbsolutePathOfCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, char *fullPath, intptr_t *bufferLength);
static int32_t  getHandleOfCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, FILE **subsystemFile);
static int32_t readCgroupMetricFromFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, const char *metricKeyInFile, char **fileContent, char *value);
static int32_t readCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, int32_t numItemsToRead, const char *format, ...);
static int32_t isRunningInContainer(struct OMRPortLibrary *portLibrary, BOOLEAN *inContainer);
static int32_t getCgroupMemoryLimit(struct OMRPortLibrary *portLibrary, uint64_t *limit);
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

#if (defined(J9X86) || defined(J9HAMMER))
		rc = omrsysinfo_get_x86_description(portLibrary, desc);
#elif defined(LINUXPPC)
		rc = omrsysinfo_get_linux_ppc_description(portLibrary, desc);
#elif defined(AIXPPC)
		rc = omrsysinfo_get_aix_ppc_description(portLibrary, desc);
#elif (defined(S390) || defined(J9ZOS390))
		rc = omrsysinfo_get_s390_description(portLibrary, desc);
#elif defined(RISCV)
		rc = omrsysinfo_get_riscv_description(portLibrary, desc);
#endif
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

	if ((NULL != desc) && (feature < (OMRPORT_SYSINFO_OS_FEATURES_SIZE * 32))) {
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

#if (defined(AIXPPC) || defined(S390) || defined(J9ZOS390))
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
#endif /* defined(AIXPPC) || defined(S390) || defined(J9ZOS390) */

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

#define LAST_DOUBLE_WORD	2

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

/** @internal
 *  Check if z/OS supports the Transactional Execution Facility (TX). We use the CVTTX (0x08) and CVTTXC (0x04) bits in
 *  the CVT structure for the OS check.
 *
 *  @return TRUE if TX is supported; FALSE otherwise.
 */
static BOOLEAN
omrsysinfo_get_s390_zos_supports_transactional_execution_facility(void)
{
	/* FLCCVT is an ADDRESS off the PSA structure
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead300/PSA-map.htm */
	uint8_t* CVT = (uint8_t*)(*(uint32_t*)0x10);

	/* CVTFLAG4 is a BITSTRING off the CVT structure containing the CVTTX (0x08), CVTTXC (0x04), and CVTRI (0x02) bits
	 * https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.3.0/com.ibm.zos.v2r3.iead100/CVT-map.htm */
	uint8_t CVTFLAG4 = *(CVT + 0x17B);

	/* Note we check for both constrained and non-constrained transaction support */
	return OMR_ARE_ALL_BITS_SET(CVTFLAG4, 0x0C);
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
#elif defined(J9ZTPF)  /* defined(J9ZOS390) */
	/*
	 * z/TPF requires OS support for some of the Hardware Capabilities.
	 * Setting the auxvFeatures capabilities flag directly to mimic the query_auxv call in Linux.
	 */
	unsigned long auxvFeatures = OMR_HWCAP_S390_HIGH_GPRS|OMR_FEATURE_S390_ESAN3|OMR_HWCAP_S390_ZARCH|
			OMR_HWCAP_S390_STFLE|OMR_HWCAP_S390_MSA|OMR_HWCAP_S390_DFP|
			OMR_HWCAP_S390_LDISP|OMR_HWCAP_S390_EIMM|OMR_HWCAP_S390_ETF3EH;

#elif defined(LINUX) /* defined(J9ZTPF) */
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

			desc->processor = OMR_PROCESSOR_S390_GP6;
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
		desc->processor = OMR_PROCESSOR_S390_GP7;
	}

	/* z10 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_GENERAL_INSTRUCTIONS_EXTENSIONS)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_GENERAL_INSTRUCTIONS_EXTENSIONS);

		desc->processor = OMR_PROCESSOR_S390_GP8;
	}

	/* z196 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_HIGH_WORD)) {
#if (defined(S390) && defined(LINUX))
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_HIGH_GPRS))
#endif /* defined(S390) && defined(LINUX)*/
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_HIGH_WORD);
		}

		desc->processor = OMR_PROCESSOR_S390_GP9;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_1)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_1);

		desc->processor = OMR_PROCESSOR_S390_GP9;
	}

	/* zEC12 facility and processor detection */

	/* TE/TX hardware support */
	if (omrsysinfo_test_stfle(portLibrary, 50) && omrsysinfo_test_stfle(portLibrary, 73)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_transactional_execution_facility())
#elif defined(LINUX) /* LINUX S390 */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_TE))
#endif /* defined(J9ZOS390) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_TE);
		}
	}

	/* RI hardware support */
	if (omrsysinfo_test_stfle(portLibrary, 64)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_runtime_instrumentation_facility())
#endif /* defined(J9ZOS390) */
		{
#if !defined(J9ZTPF)
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_RI);
#endif /* !defined(J9ZTPF) */
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION);

		desc->processor = OMR_PROCESSOR_S390_GP10;
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
			desc->processor = OMR_PROCESSOR_S390_GP11;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_2)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_2);

		desc->processor = OMR_PROCESSOR_S390_GP11;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_LOAD_AND_ZERO_RIGHTMOST_BYTE)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_LOAD_AND_ZERO_RIGHTMOST_BYTE);

		desc->processor = OMR_PROCESSOR_S390_GP11;
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

			desc->processor = OMR_PROCESSOR_S390_GP12;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2);

		desc->processor = OMR_PROCESSOR_S390_GP12;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_SEMAPHORE_ASSIST)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_SEMAPHORE_ASSIST);

		desc->processor = OMR_PROCESSOR_S390_GP12;
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

			desc->processor = OMR_PROCESSOR_S390_GP12;
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

			desc->processor = OMR_PROCESSOR_S390_GP12;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MSA_EXTENSION_8)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MSA_EXTENSION_8);

		desc->processor = OMR_PROCESSOR_S390_GP12;
	}
	
    /* z15 facility and processor detection */

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3)) {
		omrsysinfo_set_feature(desc, OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3);

		desc->processor = OMR_PROCESSOR_S390_GP13;
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) && !defined(J9ZTPF) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(J9ZTPF) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2);

			desc->processor = OMR_PROCESSOR_S390_GP13;
		}
	}

	if (omrsysinfo_test_stfle(portLibrary, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY)) {
#if defined(J9ZOS390)
		if (omrsysinfo_get_s390_zos_supports_vector_extension_facility())
#elif defined(LINUX) && !defined(J9ZTPF) /* defined(J9ZOS390) */
		if (OMR_ARE_ALL_BITS_SET(auxvFeatures, OMR_HWCAP_S390_VXRS))
#endif /* defined(LINUX) && !defined(J9ZTPF) */
		{
			omrsysinfo_set_feature(desc, OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY);

			desc->processor = OMR_PROCESSOR_S390_GP13;
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

#endif /* defined(S390) || defined(J9ZOS390) */

#if defined(RISCV)
static intptr_t
omrsysinfo_get_riscv_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc)
{
#if defined(RISCV32)
	desc->processor = OMR_PROCESOR_RISCV32_UNKNOWN;
#elif defined(RISCV64)
	desc->processor = OMR_PROCESOR_RISCV64_UNKNOWN;
#elif
	desc->processor = OMR_PROCESSOR_UNDEFINED;
#endif
	desc->physicalProcessor = desc->processor;
	return 0;
}
#endif /* defined(RISCV) */

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
		int rc = -1;
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
				char myBuffer[512];  /* the file size is typically ~480 bytes */
				char *buffer = myBuffer;
				size_t len = (size_t) portLibrary->file_flength(portLibrary, plistFile);
				rc = 0;
				if (len >= sizeof(myBuffer)) { /* leave space for a terminating null */
					allocatedFileBuffer = portLibrary->mem_allocate_memory(portLibrary, len + 1, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
					buffer = allocatedFileBuffer;
				}
				if (NULL != buffer) {
					char *readPtr = buffer;
					size_t bytesRemaining = len;
					while (bytesRemaining > 0) {
						intptr_t result = portLibrary->file_read(portLibrary, plistFile, readPtr, bytesRemaining);
						if (result > 0) {
							bytesRemaining -= result;
							readPtr += result;
						} else {
							rc = -1; /* error or unexpected EOF */
							break;
						}
					}
					if (0 == rc) {
						buffer[len] = '\0';
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
#else /* defined(OSX) */

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

	portLibrary->str_printf(portLibrary, buffer, PATH_MAX, "/proc/%ld/psinfo", getpid());
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
		int32_t size = sizeof(cpuSet); /* Size in bytes */
		pid_t mainProcess = getpid();
		int32_t error = sched_getaffinity(mainProcess, size, &cpuSet);

		if (0 == error) {
			toReturn = CPU_COUNT(&cpuSet);
		} else {
			toReturn = 0;
			if (EINVAL == errno) {
				/* Too many CPUs for the fixed cpu_set_t structure */
				int32_t numCPUs = sysconf(_SC_NPROCESSORS_CONF);
				cpu_set_t *allocatedCpuSet = CPU_ALLOC(numCPUs);
				if (NULL != allocatedCpuSet) {
					size_t size = CPU_ALLOC_SIZE(numCPUs);
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
			int32_t numItemsToRead = 1; /* cpu.cfs_quota_us and cpu.cfs_period_us files each contain only one integer value */

			/* If either file read fails, ignore cgroup cpu quota limits and continue */
			rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_CPU, "cpu.cfs_quota_us", numItemsToRead, "%ld", &cpuQuota);
			if (0 == rc) {
				rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_CPU, "cpu.cfs_period_us", numItemsToRead, "%lu", &cpuPeriod);
				if (0 == rc) {
					int32_t numCpusQuota = (int32_t) (((double) cpuQuota / cpuPeriod) + 0.5);

					if ((cpuQuota > 0) && (numCpusQuota < toReturn)) {
						toReturn = numCpusQuota;
						/* If the CPU quota rounds down to 0, then just return 1 as the closest usable value */
						if (0 == toReturn) {
							toReturn = 1;
						}
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
	FILE *memStatFs = NULL;
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

	return rc;
}

#define CGROUP_MEMORY_LIMIT_IN_BYTES_FILE "memory.limit_in_bytes"
#define CGROUP_MEMORY_USAGE_IN_BYTES_FILE "memory.usage_in_bytes"
#define CGROUP_MEMORY_SWAP_LIMIT_IN_BYTES_FILE "memory.memsw.limit_in_bytes"
#define CGROUP_MEMORY_SWAP_USAGE_IN_BYTES_FILE "memory.memsw.usage_in_bytes"
#define CGROUP_MEMORY_STAT_FILE "memory.stat"

#define CGROUP_MEMORY_STAT_CACHE "cache"
#define CGROUP_MEMORY_STAT_CACHE_SZ (sizeof(CGROUP_MEMORY_STAT_CACHE)-1)

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

	Assert_PRT_true(NULL != cgroupMemInfo);

	cgroupMemInfo->memoryLimit = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->memoryUsage = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->memoryAndSwapLimit = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->memoryAndSwapUsage = OMRPORT_MEMINFO_NOT_AVAILABLE;
	cgroupMemInfo->cached = OMRPORT_MEMINFO_NOT_AVAILABLE;

	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, CGROUP_MEMORY_LIMIT_IN_BYTES_FILE, numItemsToRead, "%lu", &cgroupMemInfo->memoryLimit);
	if (0 != rc) {
		goto _exit;
	}
	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, CGROUP_MEMORY_USAGE_IN_BYTES_FILE, numItemsToRead, "%lu", &cgroupMemInfo->memoryUsage);
	if (0 != rc) {
		goto _exit;
	}
	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, CGROUP_MEMORY_SWAP_LIMIT_IN_BYTES_FILE, numItemsToRead, "%lu", &cgroupMemInfo->memoryAndSwapLimit);
	if (0 != rc) {
		if (OMRPORT_ERROR_FILE_NOENT == rc) {
			/* It is possible file memory.memsw.limit_in_bytes is not present if
			 * swap space is not configured. In such cases, set memoryAndSwapLimit to same as memoryLimit.
			 */
			cgroupMemInfo->memoryAndSwapLimit = cgroupMemInfo->memoryLimit;
			rc = 0;
		} else {
			goto _exit;
		}
	}
	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, CGROUP_MEMORY_SWAP_USAGE_IN_BYTES_FILE, numItemsToRead, "%lu", &cgroupMemInfo->memoryAndSwapUsage);
	if (0 != rc) {
		if (OMRPORT_ERROR_FILE_NOENT == rc) {
			/* It is possible file memory.memsw.usage_in_bytes is not present if
			 * swap space is not configured. In such cases, set memoryAndSwapUsage to memoryUsage.
			 */
			cgroupMemInfo->memoryAndSwapUsage = cgroupMemInfo->memoryUsage;
			rc = 0;
		} else {
			goto _exit;
		}
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
		if (0 == strncmp(tmpPtr, CGROUP_MEMORY_STAT_CACHE, CGROUP_MEMORY_STAT_CACHE_SZ)) {
			tmpPtr += CGROUP_MEMORY_STAT_CACHE_SZ;
			rc = sscanf(tmpPtr, "%" SCNu64, &cgroupMemInfo->cached);
			if (1 != rc) {
				Trc_PRT_retrieveLinuxCgroupMemoryStats_invalidValue(CGROUP_MEMORY_STAT_CACHE, CGROUP_MEMORY_STAT_FILE);
				rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_FILE_INVALID_VALUE, "invalid value for field %s in file %s", CGROUP_MEMORY_STAT_CACHE, CGROUP_MEMORY_STAT_FILE);
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
		omrthread_monitor_enter(cgroupEntryListMonitor);
		freeCgroupEntries(portLibrary, PPG_cgroupEntryList);
		PPG_cgroupEntryList = NULL;
		omrthread_monitor_exit(cgroupEntryListMonitor);
		attachedPortLibraries -= 1;
		if (0 == attachedPortLibraries) {
			omrthread_monitor_destroy(cgroupEntryListMonitor);
			cgroupEntryListMonitor = NULL;
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
	PPG_isRunningInContainer = FALSE;
	/* Obtain and cache executable name; if this fails, executable name remains NULL, but
	 * shouldn't cause failure to startup port library.  Failure will be noticed only
	 * when the omrsysinfo_get_executable_name() actually gets invoked.
	 */
	(void) find_executable_name(portLibrary, &PPG_si_executableName);

#if defined(LINUX) && !defined(OMRZTPF)
	PPG_cgroupEntryList = NULL;
	/* To handle the case where multiple port libraries are started and shutdown,
	 * as done by some fvtests (eg fvtest/porttest/j9portTest.cpp) that create fake portlibrary
	 * to test its management and lifecycle,
	 * we need to ensure globals like cgroupEntryListMonitor are initialized and destroyed only once.
	 */
	if (0 == attachedPortLibraries) {
		if (omrthread_monitor_init_with_name(&cgroupEntryListMonitor, 0, "cgroup entry list monitor")) {
			return -1;
		}
	}
	attachedPortLibraries += 1;
	isRunningInContainer(portLibrary, &PPG_isRunningInContainer);
#endif /* defined(LINUX) */
	return 0;

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

uint32_t
omrsysinfo_get_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t *limit)
{
	uint32_t hardLimitRequested = OMRPORT_LIMIT_HARD == (resourceID & OMRPORT_LIMIT_HARD);
	uint32_t resourceRequested = resourceID & ~OMRPORT_LIMIT_HARD;
	uint32_t rc = OMRPORT_LIMIT_UNKNOWN;
	struct rlimit lim = {0, 0};
	uint32_t resource = 0;

	Trc_PRT_sysinfo_get_limit_Entered(resourceID);

	if (OMRPORT_RESOURCE_ADDRESS_SPACE == resourceRequested) {
#if !defined(OMRZTPF)
		resource = RLIMIT_AS;
#else /* !defined(OMRZTPF) */
		/* z/TPF does not have virtual address support */
		*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
		Trc_PRT_sysinfo_get_limit_Exit(rc);
		return rc;
#endif /* !defined(OMRZTPF) */
	} else if (OMRPORT_RESOURCE_CORE_FILE == resourceRequested) {
#if !defined(OMRZTPF)
		resource = RLIMIT_CORE;
#else /* !defined(OMRZTPF) */
		/* z/TPF does not have virtual address support */
		*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
		Trc_PRT_sysinfo_get_limit_Exit(rc);
		return rc;
#endif /* !defined(OMRZTPF) */
	} else if (OMRPORT_RESOURCE_DATA == resourceRequested) {
#if !defined(OMRZTPF)
		resource = RLIMIT_DATA;
#else /* !defined(OMRZTPF) */
		/* Not implemented for z/TPF */
		*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
		Trc_PRT_sysinfo_get_limit_Exit(rc);
		return rc;
#endif /* !defined(OMRZTPF) */
	}

	switch (resourceRequested) {
	case OMRPORT_RESOURCE_SHARED_MEMORY:
		rc = getLimitSharedMemory(portLibrary, limit);
		break;
	case OMRPORT_RESOURCE_ADDRESS_SPACE:
		/* FALLTHROUGH */
	case OMRPORT_RESOURCE_CORE_FILE:
		/* FALLTHROUGH */
	case OMRPORT_RESOURCE_DATA: {
#if !defined(OMRZTPF)
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
#endif /* !defined(OMRZTPF) */
	}
	break;
	case OMRPORT_RESOURCE_CORE_FLAGS: {
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
	}
	break;
	/* Get the limit on maximum number of files that may be opened in any
	 * process, in the current configuration of the operating system.  This
	 * must match "ulimit -n".
	 */
	case OMRPORT_RESOURCE_FILE_DESCRIPTORS: {
#if defined(AIXPPC) || (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || defined(J9ZOS390)
		/* getrlimit(2) is a POSIX routine. */
		if (0 == getrlimit(RLIMIT_NOFILE, &lim)) {
			*limit = (uint64_t) (hardLimitRequested ? lim.rlim_max : lim.rlim_cur);
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
#else /* defined(AIXPPC) || (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || defined(J9ZOS390) */
		/* unsupported on other platforms (just in case). */
		*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
		rc = OMRPORT_LIMIT_UNKNOWN;
#endif /* defined(AIXPPC) || (defined(LINUX) && !defined(OMRZTPF)) || defined(OSX) || defined(J9ZOS390) */
	}
	break;
	default:
		Trc_PRT_sysinfo_getLimit_unrecognised_resourceID(resourceID);
		*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
		rc = OMRPORT_LIMIT_UNKNOWN;
	}

	Trc_PRT_sysinfo_get_limit_Exit(rc);
	return rc;
}

uint32_t
omrsysinfo_set_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t limit)
{
	uint32_t hardLimitRequested = OMRPORT_LIMIT_HARD == (resourceID & OMRPORT_LIMIT_HARD);
	uint32_t resourceRequested = resourceID & ~OMRPORT_LIMIT_HARD;
	struct rlimit lim = {0, 0};
	uint32_t rc = 0;
	uint32_t resource = 0;

	Trc_PRT_sysinfo_set_limit_Entered(resourceID, limit);

	switch (resourceRequested) {
	case OMRPORT_RESOURCE_FILE_DESCRIPTORS:
		resource = RLIMIT_NOFILE;
		break;
	case OMRPORT_RESOURCE_ADDRESS_SPACE:
#if !defined(OMRZTPF)
		resource = RLIMIT_AS;
#else /* !defined(OMRZTPF) */
		/* z/TPF does not have virtual address support */
		rc = -1;
#endif /* !defined(OMRZTPF) */
		break;
	case OMRPORT_RESOURCE_CORE_FILE:
#if !defined(OMRZTPF)
		resource = RLIMIT_CORE;
#else /* !defined(OMRZTPF) */
		rc = OMRPORT_LIMIT_UNKNOWN;
#endif /* !defined(OMRZTPF) */
		break;
	case OMRPORT_RESOURCE_DATA:
#if !defined(OMRZTPF)
		resource = RLIMIT_DATA;
#else /* !defined(OMRZTPF) */
		rc = OMRPORT_LIMIT_UNKNOWN;
#endif /* !defined(OMRZTPF) */
	default:
		break;
	}

	if (0 == rc) {
		switch (resourceRequested) {
		case OMRPORT_RESOURCE_FILE_DESCRIPTORS:
			/* FALLTHROUGH */
		case OMRPORT_RESOURCE_ADDRESS_SPACE:
			/* FALLTHROUGH */
		case OMRPORT_RESOURCE_CORE_FILE:
			/* FALLTHROUGH */
		case OMRPORT_RESOURCE_DATA: {
#if !defined(OMRZTPF)
			rc = getrlimit(resource, &lim);
			if (-1 == rc) {
				portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
				Trc_PRT_sysinfo_getrlimit_error(resource, findError(errno));
				break;
			}

			if (hardLimitRequested) {
				lim.rlim_max = limit;
			} else {
#if defined(OSX)
				/* MacOS doesn't allow the soft file limit to be unlimited */
				if ((OMRPORT_RESOURCE_FILE_DESCRIPTORS == resourceRequested)
						&& (RLIM_INFINITY == limit)) {
					int32_t maxFiles = 0;
					size_t resultSize = sizeof(maxFiles);
					int name[] = {CTL_KERN, KERN_MAXFILESPERPROC};
					rc = sysctl(name, 2, &maxFiles, &resultSize, NULL, 0);
					if (-1 == rc) {
						portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
						Trc_PRT_sysinfo_setrlimit_error(resource, limit, findError(errno));
					} else {
						limit = maxFiles;
					}
				}
#endif
				lim.rlim_cur = limit;
			}

			rc = setrlimit(resource, &lim);
			if (-1 == rc) {
				portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
				Trc_PRT_sysinfo_setrlimit_error(resource, limit, findError(errno));
			}
#else /* !defined(OMRZTPF) */
			rc = OMRPORT_LIMIT_UNKNOWN;
#endif /* !defined(OMRZTPF) */
			break;
		}

		case OMRPORT_RESOURCE_CORE_FLAGS: {
#if defined(AIXPPC)
			struct vario myvar;

			myvar.v.v_fullcore.value = limit;
			rc = sys_parm(SYSP_SET, SYSP_V_FULLCORE, &myvar);
			if (-1 == rc) {
				portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
				Trc_PRT_sysinfo_sysparm_error(errno);
			}
#else
			/* unsupported so return error */
			rc = OMRPORT_LIMIT_UNKNOWN;
#endif
			break;
		}

		default:
			Trc_PRT_sysinfo_setLimit_unrecognised_resourceID(resourceID);
			rc = OMRPORT_LIMIT_UNKNOWN;
		}
	}

	Trc_PRT_sysinfo_set_limit_Exit(rc);
	return rc;
}

static uint32_t
getLimitSharedMemory(struct OMRPortLibrary *portLibrary, uint64_t *limit)
{
#if defined(LINUX) && !defined(OMRZTPF)
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
#else /* defined(LINUX) && !defined(OMRZTPF) */
	Trc_PRT_sysinfo_getLimitSharedMemory_notImplemented(OMRPORT_LIMIT_UNKNOWN);
	*limit = OMRPORT_LIMIT_UNKNOWN_VALUE;
	return OMRPORT_LIMIT_UNKNOWN;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
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
		return OMRPORT_ERROR_OPFAILED;
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
		return OMRPORT_ERROR_OPFAILED;
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
	rc = statfs(OMR_CGROUP_V1_MOUNT_POINT, &buf);
	if (0 != rc) {
		int32_t osErrCode = errno;
		Trc_PRT_isCgroupV1Available_statfs_failed(OMR_CGROUP_V1_MOUNT_POINT, osErrCode);
		portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_SYSINFO_SYS_FS_CGROUP_STATFS_FAILED);
		result = FALSE;
	} else if (TMPFS_MAGIC != buf.f_type) {
		Trc_PRT_isCgroupV1Available_tmpfs_not_mounted(OMR_CGROUP_V1_MOUNT_POINT);
		portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_SYS_FS_CGROUP_TMPFS_NOT_MOUNTED, "tmpfs is not mounted on " OMR_CGROUP_V1_MOUNT_POINT);
		result = FALSE;
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
readCgroupFile(struct OMRPortLibrary *portLibrary, int pid, BOOLEAN inContainer, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems)
{
	char cgroupFilePath[PATH_MAX];
	uintptr_t requiredSize = 0;
	FILE *cgroupFile = NULL;
	OMRCgroupEntry *cgEntryList = NULL;
	uint64_t available = 0;
	int32_t rc = 0;

	Assert_PRT_true(NULL != cgroupEntryList);

	requiredSize = portLibrary->str_printf(portLibrary, NULL, (uint32_t)-1, "/proc/%d/cgroup", pid);
	Assert_PRT_true(requiredSize <= PATH_MAX);
	portLibrary->str_printf(portLibrary, cgroupFilePath, sizeof(cgroupFilePath), "/proc/%d/cgroup", pid);

	/* Even if 'inContainer' is TRUE, we need to parse the cgroup file to get the list of subsystems */
	cgroupFile = fopen(cgroupFilePath, "r");
	if (NULL == cgroupFile) {
		int32_t osErrCode = errno;
		Trc_PRT_readCgroupFile_fopen_failed(cgroupFilePath, osErrCode);
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

		if (NULL == fgets(buffer, PATH_MAX, cgroupFile)) {
			break;
		}
		if (0 != ferror(cgroupFile)) {
			int32_t osErrCode = errno;
			Trc_PRT_readCgroupFile_fgets_failed(cgroupFilePath, osErrCode);
			rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "fgets failed to read %s file stream with errno=%d", cgroupFilePath, osErrCode);
			goto _end;
		}
		rc = sscanf(buffer, PROC_PID_CGROUP_ENTRY_FORMAT, &hierId, subsystems, cgroup);

		if (EOF == rc) {
			break;
		} else if (1 == rc) {
			rc = sscanf(buffer, PROC_PID_CGROUP_SYSTEMD_ENTRY_FORMAT, &hierId, cgroup);

			if (2 != rc) {
				Trc_PRT_readCgroupFile_unexpected_format(cgroupFilePath);
				rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpected format of %s", cgroupFilePath);
				goto _end;
			}
			subsystems[0] = '\0';
		} else if (3 != rc) {
			Trc_PRT_readCgroupFile_unexpected_format(cgroupFilePath);
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
					&& !strcmp(cursor, supportedSubsystems[i].name)
				) {
					const char *cgroupToUse = cgroup;

					if (TRUE == inContainer) {
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
			Trc_PRT_readCgroupFile_available_subsystems(available);
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

	cgroup = getCgroupNameForSubsystem(portLibrary, PPG_cgroupEntryList, subsystemNames[subsystem]);
	if (NULL == cgroup) {
		/* If the subsystem is available and supported, cgroup must not be NULL */
		Trc_PRT_readCgroupSubsystemFile_missing_cgroup(subsystemFlag);
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_NAME_NOT_AVAILABLE, "cgroup name for subsystem %s is not available", subsystemNames[MEMORY]);
		Trc_PRT_Assert_ShouldNeverHappen();
		goto _end;
	}

	/* absolute path of the file to be read is: /sys/fs/cgroup/subsystemNames[subsystem]/cgroup/filenName */
	fullPathLen = portLibrary->str_printf(portLibrary, NULL, (uint32_t)-1, "%s/%s/%s/%s", OMR_CGROUP_V1_MOUNT_POINT, subsystemNames[subsystem], cgroup, fileName);
	if (fullPathLen > *bufferLength) {
		*bufferLength = fullPathLen;
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_STRING_BUFFER_TOO_SMALL, "buffer size should be %d bytes", fullPathLen);
		goto _end;
	}

	portLibrary->str_printf(portLibrary, fullPath, fullPathLen, "%s/%s/%s/%s", OMR_CGROUP_V1_MOUNT_POINT, subsystemNames[subsystem], cgroup, fileName);

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
 * Reads the Cgroup metric (if has multiple searched for metric key and reads it) and returns the value to the param `value`
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] subsystemFlag flag of type OMR_CGROUP_SUBSYSTEMS_* representing the cgroup subsystem
 * @param[in] fileName name of the file under cgroup subsystem
 * @param[in] metricKeyInFile name of the cgroup metric which is in a group of metrics in a cgroup subsystem file
 * @param[in/out] value pointer to char * which stores the value for the specified cgroup subsystem metric
 * @param[in/out] fileContent double pointer to allocate memory and write file content in case of a multiline file with more than 1 metric available in it
 *
 * @return 0 on success, negative error code on any error
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
	if (NULL == metricKeyInFile) { /* If `metricKeyInFile` is NULL then it states the file has only one Cgroup metric to be read */
		if (NULL == fgets(value, MAX_LINE_LENGTH, file)) {
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_METRIC_NOT_AVAILABLE;
		}
		goto _end;
	} else { /* Else we read the file to a buffer and pick the metrics from it later */
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
 * @param[out] inContainer pointer to BOOLEAN which on successful return indicates if
 *      the process is running in container or not.  On error it indicates FALSE.
 *
 * @return 0 on success, otherwise negative error code
 */
static int32_t
isRunningInContainer(struct OMRPortLibrary *portLibrary, BOOLEAN *inContainer)
{
	int32_t rc = 0;
	FILE *cgroupFile = NULL;

	/* Assume we are not in container */
	*inContainer = FALSE;

	if (isCgroupV1Available(portLibrary)) {
		/* Read PID 1's cgroup file /proc/1/cgroup and check cgroup name for each subsystem.
		 * If cgroup name for each subsystem points to the root cgroup "/",
		 * then the process is not running in a container.
		 * For any other cgroup name, assume we are in a container.
		 */
		cgroupFile = fopen(OMR_PROC_PID_ONE_CGROUP_FILE, "r");

		if (NULL == cgroupFile) {
			int32_t osErrCode = errno;
			Trc_PRT_isRunningInContainer_fopen_failed(OMR_PROC_PID_ONE_CGROUP_FILE, osErrCode);
			rc = portLibrary->error_set_last_error(portLibrary, osErrCode, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_FOPEN_FAILED);
			goto _end;
		}

		while (0 == feof(cgroupFile)) {
			char buffer[PATH_MAX];
			char cgroup[PATH_MAX];
			char subsystems[PATH_MAX];
			int32_t hierId = -1;

			if (NULL == fgets(buffer, PATH_MAX, cgroupFile)) {
				break;
			}
			if (0 != ferror(cgroupFile)) {
				int32_t osErrCode = errno;
				Trc_PRT_isRunningInContainer_fgets_failed(OMR_PROC_PID_ONE_CGROUP_FILE, osErrCode);
				rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "fgets failed to read %s file stream with errno=%d", OMR_PROC_PID_ONE_CGROUP_FILE, osErrCode);
				goto _end;
			}
			rc = sscanf(buffer, PROC_PID_CGROUP_ENTRY_FORMAT, &hierId, subsystems, cgroup);

			if (EOF == rc) {
				break;
			} else if (1 == rc) {
				rc = sscanf(buffer, PROC_PID_CGROUP_SYSTEMD_ENTRY_FORMAT, &hierId, cgroup);

				if (2 != rc) {
					Trc_PRT_isRunningInContainer_unexpected_format(OMR_PROC_PID_ONE_CGROUP_FILE);
					rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpected format of %s file", OMR_PROC_PID_ONE_CGROUP_FILE);
					goto _end;
				}
			} else if (3 != rc) {
				Trc_PRT_isRunningInContainer_unexpected_format(OMR_PROC_PID_ONE_CGROUP_FILE);
				rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpected format of %s file", OMR_PROC_PID_ONE_CGROUP_FILE);
				goto _end;
			}

			if ((0 != strcmp(ROOT_CGROUP, cgroup)) && (0 != strcmp(SYSTEMD_INIT_CGROUP, cgroup))) {
				*inContainer = TRUE;
				break;
			}
		}
		rc = 0;
	}
	Trc_PRT_isRunningInContainer_container_detected((uintptr_t)*inContainer);
_end:
	if (NULL != cgroupFile) {
		fclose(cgroupFile);
	}
	return rc;
}

static int32_t
getCgroupMemoryLimit(struct OMRPortLibrary *portLibrary, uint64_t *limit)
{
	uint64_t cgroupMemLimit = 0;
	uint64_t physicalMemLimit = 0;
	int32_t numItemsToRead = 1; /* memory.limit_in_bytes file contains only one integer value */
	int32_t rc = 0;

	Trc_PRT_sysinfo_cgroup_get_memlimit_Entry();

	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, "memory.limit_in_bytes", numItemsToRead, "%" SCNu64, &cgroupMemLimit);
	if (0 != rc) {
		Trc_PRT_sysinfo_cgroup_get_memlimit_memory_limit_read_failed("memory.limit_in_bytes", rc);
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

#endif /* defined(LINUX) && !defined(OMRZTPF) */

BOOLEAN
omrsysinfo_cgroup_is_system_available(struct OMRPortLibrary *portLibrary)
{
	BOOLEAN result = FALSE;
#if defined(LINUX) && !defined(OMRZTPF)
	int32_t rc = OMRPORT_ERROR_SYSINFO_CGROUP_UNSUPPORTED_PLATFORM;

	Trc_PRT_sysinfo_cgroup_is_system_available_Entry();
	if (NULL == PPG_cgroupEntryList) {
		if (isCgroupV1Available(portLibrary)) {
			BOOLEAN inContainer = FALSE;

			rc = isRunningInContainer(portLibrary, &inContainer);
			if (0 != rc) {
				goto _end;
			}
			omrthread_monitor_enter(cgroupEntryListMonitor);
			if (NULL == PPG_cgroupEntryList) {
				rc = readCgroupFile(portLibrary, getpid(), inContainer, &PPG_cgroupEntryList, &PPG_cgroupSubsystemsAvailable);
			}
			omrthread_monitor_exit(cgroupEntryListMonitor);
			if (0 != rc) {
				goto _end;
			}
		}
	} else {
		rc = 0;
	}
_end:
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
	return PPG_isRunningInContainer;
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
	switch (subsystem) {
	case OMR_CGROUP_SUBSYSTEM_MEMORY :
		state->numElements = sizeof(omrCgroupMemoryMetricMap) / sizeof(omrCgroupMemoryMetricMap[0]);
		break;
	case OMR_CGROUP_SUBSYSTEM_CPU :
		state->numElements = sizeof(omrCgroupCpuMetricMap) / sizeof(omrCgroupCpuMetricMap[0]);
		break;
	case OMR_CGROUP_SUBSYSTEM_CPUSET :
		state->numElements = sizeof(omrCgroupCpusetMetricMap) / sizeof(omrCgroupCpusetMetricMap[0]);
		break;
	default :
		goto _end;
	}
	rc = 0;

_end:
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
		switch (state->subsystemid) {
		case OMR_CGROUP_SUBSYSTEM_MEMORY:
			subsystemMetricMap = omrCgroupMemoryMetricMap;
			break;
		case OMR_CGROUP_SUBSYSTEM_CPU:
			subsystemMetricMap = omrCgroupCpuMetricMap;
			break;
		case OMR_CGROUP_SUBSYSTEM_CPUSET:
			subsystemMetricMap = omrCgroupCpusetMetricMap;
			break;
		default:
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_UNAVAILABLE;
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
	int32_t rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_UNAVAILABLE;
#if defined(LINUX) && !defined(OMRZTPF)
	struct OMRCgroupMetricInfoElement *currentElement = NULL;
	const struct OMRCgroupSubsystemMetricMap *subsystemMetricMap = NULL;
	const struct OMRCgroupSubsystemMetricMap *subsystemMetricMapElement = NULL;
	if (state->count >= state->numElements) {
		goto _end;
	}
	switch (state->subsystemid) {
	case OMR_CGROUP_SUBSYSTEM_MEMORY:
		subsystemMetricMap = omrCgroupMemoryMetricMap;
		break;
	case OMR_CGROUP_SUBSYSTEM_CPU:
		subsystemMetricMap = omrCgroupCpuMetricMap;
		break;
	case OMR_CGROUP_SUBSYSTEM_CPUSET:
		subsystemMetricMap = omrCgroupCpusetMetricMap;
		break;
	default:
		rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_UNAVAILABLE;
		state->count += 1;
		goto _end;
	}
	subsystemMetricMapElement = &subsystemMetricMap[state->count];
	currentElement = subsystemMetricMapElement->metricInfoElementList;
	if (NULL == state->fileContent) {
		rc = readCgroupMetricFromFile(portLibrary, state->subsystemid, subsystemMetricMapElement->metricFileName, currentElement->metricKeyInFile, &(state->fileContent), metricElement->value);

		/* In error condition or single metric condition we increment the counter to continue to next metric */
		if ((0 != rc) || (NULL == state->fileContent)) {
			state->fileContent = NULL;
			state->count += 1;
			goto _end;
		}

		state->fileMetricCounter = 0;
	}

	if (state->fileMetricCounter < subsystemMetricMapElement->metricElementsCount) {
		BOOLEAN isMetricFound = FALSE;
		char *current = state->fileContent;
		char *end = state->fileContent + strlen(state->fileContent) - 1;
		int32_t metricKeyLen = 0;
		currentElement += state->fileMetricCounter;
		metricKeyLen = strlen(currentElement->metricKeyInFile);
		while (current < end) {
			if (0 == strncmp(current, currentElement->metricKeyInFile, metricKeyLen)) {
				char *newLine = current;
				current += metricKeyLen;
				/* advance past any whitespace */
				while ((current != end) &&  (' ' == *current)) {
					current += 1;
				}
				/* find the newline */
				newLine = current;
				while ('\n' != *newLine) {
					newLine += 1;
				}
				strncpy(metricElement->value, current, newLine - current);
				metricElement->value[newLine - current] = '\0';
				isMetricFound = TRUE;
				rc = 0;
				break;
			}
			current += 1;
		}
		/* if we complete parse the fileContent and couldn't find the metric return error code */
		if (!isMetricFound) {
			rc = OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_METRIC_NOT_AVAILABLE;
		}

		state->fileMetricCounter += 1;
	}

	if (state->fileMetricCounter >= subsystemMetricMapElement->metricElementsCount) {
		if (NULL != state->fileContent) {
			portLibrary->mem_free_memory(portLibrary, state->fileContent);
			state->fileContent = NULL;
		}
		state->count += 1;
	}

_end:
	if (0 == rc) {
		/**
		 * 'value' may have new line at the end (fgets add '\n' at the end)
		 * which is not required so we could remove it
		 */
		size_t len = strlen(metricElement->value);
		metricElement->units = currentElement->metricUnit;
		if ((len > 0) && (metricElement->value[len-1] == '\n')) {
			metricElement->value[len-1] = '\0';
		}
		if (currentElement->isValueToBeChecked) {
			int64_t result = 0;
			sscanf(metricElement->value, "%" PRId64, &result);
			if ((result > (MAX_DEFAULT_VALUE_CHECK)) || (result < 0)) {
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
