/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * @ingroup Port
 * @brief System information
 */

#if 0
#define ENV_DEBUG
#endif

#if defined(LINUX) && !defined(OMRZTPF)
#define _GNU_SOURCE
#elif defined(OSX)
#define _XOPEN_SOURCE
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>
#include <mach-o/dyld.h>
#include <sys/param.h>
#include <sys/mount.h>
#endif /* defined(LINUX) && !defined(OMRZTPF) */

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
#if defined(J9OS_I5)
#include "Xj9I5OSInterface.H"
#endif
#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <nl_types.h>
#include <langinfo.h>
#if !defined(USER_HZ) && !defined(OMRZTPF)
#define USER_HZ HZ
#endif /* !defined(USER_HZ) && !defined(OMRZTPF) */

#if defined(J9ZOS390)
#include "omrsimap.h"
#include "omrsysinfo_helpers.h"
#endif /* defined(J9ZOS390) */

#if defined(LINUXPPC)
#include "auxv.h"
#include <strings.h>
#endif /* defined(LINUXPPC) */

#if defined(AIXPPC)
#include <fcntl.h>
#include <sys/procfs.h>
#include <sys/systemcfg.h>
#endif /* defined(AIXPPC) */

/* Start copy from omrfiletext.c */
/* __STDC_ISO_10646__ indicates that the platform wchar_t encoding is Unicode */
/* but older versions of libc fail to set the flag, even though they are Unicode */
#if defined(__STDC_ISO_10646__) || defined(LINUX) ||defined(OSX)
#define J9VM_USE_MBTOWC
#else /* defined(__STDC_ISO_10646__) || defined(LINUX) || defined(OSX) */
#include "omriconvhelpers.h"
#endif /* defined(__STDC_ISO_10646__) || defined(LINUX) || defined(OSX) */

/* a2e overrides nl_langinfo to return ASCII strings. We need the native EBCDIC string */
#if defined(J9ZOS390) && defined (nl_langinfo)
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

#if defined( OMR_ENV_DATA64 )
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

#define JIFFIES			100
#define USECS_PER_SEC	1000000
#define TICKS_TO_USEC	((uint64_t)(USECS_PER_SEC/JIFFIES))

/* For the omrsysinfo_env_iterator */
extern char **environ;

static uintptr_t copyEnvToBuffer(struct OMRPortLibrary *portLibrary, void *args);
static uintptr_t copyEnvToBufferSignalHandler(struct OMRPortLibrary *portLib, uint32_t gpType, void *gpInfo, void *unUsed);

static void setPortableError(OMRPortLibrary *portLibrary, const char *funcName, int32_t portlibErrno, int systemErrno);

static const char *getgroupsErrorMsgPrefix = "getgroups : ";

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
struct  {
	int resource;
	char *resourceName;
} limitMap[] = {
#if defined RLIMIT_AS
	{RLIMIT_AS, "RLIMIT_AS"},
#endif
#if defined RLIMIT_CORE
	{RLIMIT_CORE, "RLIMIT_CORE"},
#endif
#if defined RLIMIT_CPU
	{RLIMIT_CPU, "RLIMIT_CPU"},
#endif
#if defined RLIMIT_DATA
	{RLIMIT_DATA, "RLIMIT_DATA"},
#endif
#if defined RLIMIT_FSIZE
	{RLIMIT_FSIZE, "RLIMIT_FSIZE"},
#endif
#if defined RLIMIT_LOCKS
	{RLIMIT_LOCKS, "RLIMIT_LOCKS"},
#endif
#if defined RLIMIT_MEMLOCK
	{RLIMIT_MEMLOCK, "RLIMIT_MEMLOCK"},
#endif
#if defined RLIMIT_NOFILE
	{RLIMIT_NOFILE, "RLIMIT_NOFILE"},
#endif
#if defined RLIMIT_THREADS
	{RLIMIT_THREADS, "RLIMIT_THREADS"},
#endif
#if defined RLIMIT_NPROC
	{RLIMIT_NPROC, "RLIMIT_NPROC"},
#endif
#if defined RLIMIT_RSS
	{RLIMIT_RSS, "RLIMIT_RSS"},
#endif
#if defined RLIMIT_STACK
	{RLIMIT_STACK, "RLIMIT_STACK"},
#endif
#if defined RLIMIT_MSGQUEUE
	{RLIMIT_MSGQUEUE, "RLIMIT_MSGQUEUE"}, /* since Linux 2.6.8 */
#endif
#if defined RLIMIT_NICE
	{RLIMIT_NICE, "RLIMIT_NICE"}, /* since Linux 2.6.12*/
#endif
#if defined RLIMIT_RTPRIO
	{RLIMIT_RTPRIO, "RLIMIT_RTPRIO"}, /* since Linux 2.6.12 */
#endif
#if defined RLIMIT_SIGPENDING
	{RLIMIT_SIGPENDING, "RLIMIT_SIGPENDING"} /* since linux 2.6.8 */
#endif
#if defined RLIMIT_MEMLIMIT
	{RLIMIT_MEMLIMIT, "RLIMIT_MEMLIMIT"} /* likely z/OS only */
#endif
};

#if defined(LINUX)

#define OMR_CGROUP_V1_MOUNT_POINT "/sys/fs/cgroup"

/* Currently 12 subsystems/resource controllers are defined.
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

const char *subsystemNames[NUM_SUBSYSTEMS] = {
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
	{ "memory", OMR_CGROUP_SUBSYSTEM_MEMORY }
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
static uint16_t getPhysicalMemory();
#endif /* !defined(RS6000) && !defined(J9ZOS390) && !defined(OSX) && !defined(OMRZTPF) */

#if defined(OMRZTPF)
	uintptr_t get_IPL_IstreamCount();
	uintptr_t get_Dispatch_IstreamCount();
#endif /* defined(OMRZTPF) */

#if defined(LINUX) && !defined(OMRZTPF)
static void freeCgroupEntries(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList);
static char * getCgroupNameForSubsystem(struct OMRPortLibrary *portLibrary, OMRCgroupEntry *cgEntryList, const char *subsystem);
static int32_t addCgroupEntry(struct OMRPortLibrary *portLibrary, OMRCgroupEntry **cgEntryList, int32_t hierId, const char *subsystem, const char *cgroupName);
static int32_t readCgroupFile(struct OMRPortLibrary *portLibrary, int pid, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems);
static OMRCgroupSubsystem getCgroupSubsystemFromFlag(uint64_t subsystemFlag);
static int32_t readCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, int32_t numItemsToRead, const char *format, ...);
#endif /* defined(LINUX) */


/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return	the (negative) portable error code
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
omrsysinfo_set_number_entitled_CPUs(struct OMRPortLibrary *portLibrary, uintptr_t number)
{
	Trc_PRT_sysinfo_set_number_entitled_CPUs_Entered();
	portLibrary->portGlobals->entitledCPUs = number;
	Trc_PRT_sysinfo_set_number_entitled_CPUs_Exit(number);
	return;
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
#else
	return "unknown";
#endif
}


intptr_t
omrsysinfo_get_env(struct OMRPortLibrary *portLibrary, const char *envVar, char *infoString, uintptr_t bufSize)
{
	char *value = (char *)getenv(envVar);
	uintptr_t len;

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
#else
	if (NULL == PPG_si_osType) {
		int rc;
		int len;
		char *buffer;
		struct utsname sysinfo;

#ifdef J9ZOS390
		rc = __osname(&sysinfo);
#else
		rc = uname(&sysinfo);
#endif
		if (rc >= 0) {
			len = strlen(sysinfo.sysname) + 1;
			buffer = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == buffer) {
				return NULL;
			}
			/* copy and null terminte (just in case) */
			strncpy(buffer, sysinfo.sysname, len - 1);
			buffer[len - 1] = '\0';
			PPG_si_osType = buffer;
		}
	}
	return PPG_si_osType;
#endif
}


const char *
omrsysinfo_get_OS_version(struct OMRPortLibrary *portLibrary)
{
	if (NULL == PPG_si_osVersion) {
		int rc;
		struct utsname sysinfo;

#ifdef J9ZOS390
		rc = __osname(&sysinfo);
#else
		rc = uname(&sysinfo);
#endif

		if (rc >= 0) {
			int len;
			char *buffer;
#if defined(RS6000) || defined(J9ZOS390)
#if defined(J9OS_I5)
			len = strlen(sysinfo.version) + strlen(sysinfo.release) + 5; /* 'V', 'R', 'M', '0', & null */
#else
			len = strlen(sysinfo.version) + strlen(sysinfo.release) + 2; /* "." and terminating null character */
#endif /* J9OS_I5 */
			buffer = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == buffer) {
				return NULL;
			}
#if defined(J9OS_I5)
			portLibrary->str_printf(portLibrary, buffer, len, "V%sR%sM0", sysinfo.version, sysinfo.release);
#else
			portLibrary->str_printf(portLibrary, buffer, len, "%s.%s", sysinfo.version, sysinfo.release);
#endif /* J9OS_I5 */
#else
			len = strlen(sysinfo.release) + 1;
			buffer = portLibrary->mem_allocate_memory(portLibrary, len, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == buffer) {
				return NULL;
			}
			strncpy(buffer, sysinfo.release, len);
			buffer[len - 1] = '\0';
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
			e2aName = e2a_func(buf.ps_pathptr, strlen(buf.ps_pathptr) + 1);
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
	free(e2aName);
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
		if (!length) {		/* empty path entry */
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
		toReturn = omrsysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_ONLINE);

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

		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedBound("errno: ", errno);
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
	case OMRPORT_CPU_ENTITLED:
#if !defined(OMRZTPF)
		toReturn = portLibrary->portGlobals->entitledCPUs;
#else
		toReturn = get_Dispatch_IstreamCount();
#endif
		break;

	case OMRPORT_CPU_TARGET: {
#if defined(J9OS_I5)
		toReturn = _system_configuration.ncpus;
		if (0 == toReturn) {
			Trc_PRT_sysinfo_get_number_CPUs_by_type_failedOnline("(no errno) ", 0);
		}
#elif defined(OMRZTPF)
		toReturn = get_Dispatch_IstreamCount();
#else /* defined(J9OS_I5) */
		uintptr_t entitled = portLibrary->portGlobals->entitledCPUs;
		uintptr_t bound = omrsysinfo_get_number_CPUs_by_type(portLibrary, OMRPORT_CPU_BOUND);
		if (entitled != 0 && entitled < bound) {
			toReturn = entitled;
		} else {
			toReturn = bound;
		}
#endif /* defined(J9OS_I5) */
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

#define MAX_LINE_LENGTH		128

/* Memory units are specified in Kilobytes. */
#define ONE_K				1024

/* Base for the decimal number system is 10. */
#define COMPUTATION_BASE	10

#if defined(LINUX)
#define MEMSTATPATH			"/proc/meminfo"

#define MEMTOTAL_PREFIX		"MemTotal:"
#define MEMTOTAL_PREFIX_SZ	(sizeof(MEMTOTAL_PREFIX) - 1)

#define MEMFREE_PREFIX		"MemFree:"
#define MEMFREE_PREFIX_SZ	(sizeof(MEMFREE_PREFIX) - 1)

#define SWAPTOTAL_PREFIX	"SwapTotal:"
#define SWAPTOTAL_PREFIX_SZ	(sizeof(SWAPTOTAL_PREFIX) - 1)

#define SWAPFREE_PREFIX		"SwapFree:"
#define SWAPFREE_PREFIX_SZ	(sizeof(SWAPFREE_PREFIX) - 1)

#define CACHED_PREFIX		"Cached:"
#define CACHED_PREFIX_SZ	(sizeof(CACHED_PREFIX) - 1)

#define BUFFERS_PREFIX		"Buffers:"
#define BUFFERS_PREFIX_SZ	(sizeof(BUFFERS_PREFIX) - 1)

/**
 * Function collects memory usage statistics on Linux platforms and returns the same.
 *
 * @param[in] portLibrary The port library.
 * @param[in] memInfo A pointer to the J9MemoryInfo struct which we populate with memory usage.
 *
 * @return 0 on success and -1 on failure.
 */
static int32_t
retrieveLinuxMemoryStats(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo)
{
	int32_t rc = -1;
	register char *tmpPtr = NULL;
	char lineString[MAX_LINE_LENGTH] = {0};
	FILE *memStatFs = NULL;
	uint64_t maxVirtual = 0;

	Trc_PRT_retrieveLinuxMemoryStats_Entered();

	/* Open the memstat file on Linux for reading; this is readonly. */
	memStatFs = fopen(MEMSTATPATH, "r");
	if (NULL == memStatFs) {
		Trc_PRT_retrieveLinuxMemoryStats_failedOpeningMemFs(errno);
		Trc_PRT_retrieveLinuxMemoryStats_Exit(OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO);
		return OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
	}

	Trc_PRT_retrieveLinuxMemoryStats_openedMemFs();

	while (0 == feof(memStatFs)) {
		char *endPtr = NULL;

		/* read off a line from MEMSTATPATH. */
		fgets((char *)lineString, MAX_LINE_LENGTH, memStatFs);
		tmpPtr = (char *)lineString;

		/* Get the various fields that we are interested in and extract the corresponding value. */
		if (0 == strncmp(tmpPtr, MEMTOTAL_PREFIX, MEMTOTAL_PREFIX_SZ)) {
			/* Now obtain the MemTotal value. */
			tmpPtr += MEMTOTAL_PREFIX_SZ;
			memInfo->totalPhysical = strtol(tmpPtr, &endPtr, COMPUTATION_BASE);
			if ((LONG_MIN == memInfo->totalPhysical) || (LONG_MAX == memInfo->totalPhysical)) {
				Trc_PRT_retrieveLinuxMemoryStats_invalidRangeFound("MemTotal");
				rc = (ERANGE == errno) ? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE :
					 OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
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
				rc = (ERANGE == errno) ? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE :
					 OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
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
				rc = (ERANGE == errno) ? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE :
					 OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
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
				rc = (ERANGE == errno) ? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE :
					 OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
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
				rc = (ERANGE == errno) ? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE :
					 OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
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
				rc = (ERANGE == errno) ? OMRPORT_ERROR_SYSINFO_PARAM_HAS_INVALID_RANGE :
					 OMRPORT_ERROR_SYSINFO_ERROR_READING_MEMORY_INFO;
				goto _cleanup;
			} /* end outer-if */

			/* The values are in Kb. */
			memInfo->buffered *= ONE_K;
		} /* end if else-if */
	} /* end while() */

	rc = portLibrary->sysinfo_get_limit(portLibrary, OMRPORT_RESOURCE_ADDRESS_SPACE | OMRPORT_LIMIT_HARD, &maxVirtual);
	if ((OMRPORT_LIMIT_UNKNOWN != rc) && (RLIM_INFINITY != maxVirtual)) {
		memInfo->totalVirtual = maxVirtual;
	}
	/* Reset return code to 0, since we are successful in retrieving memory usage. Failure
	 * conditions would have jumped over to _cleanup section and shall return an appropriately
	 * set error code.
	 */
	rc = 0;

_cleanup:
	fclose(memStatFs);
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

	Trc_PRT_retrieveAIXMemoryStats_Exit(0);
	return 0;
#else
	return -1; /* not supported */
#endif
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

#if defined (RS6000)
	/* physmem is not a field in the system_configuration struct */
	/* on systems with 43K headers. However, this is not an issue as we only support AIX 5.2 and above only */
	result = (uint64_t) _system_configuration.physmem;
#elif defined(J9ZOS390)
	J9CVT * __ptr32 cvtp = ((J9PSA * __ptr32)0)->flccvt;
	J9RCE * __ptr32 rcep = cvtp->cvtrcep;
	result = ((U_64)rcep->rcepool * J9BYTES_PER_PAGE);
#elif defined(OSX)
	{
		int name[2] = {CTL_HW, HW_MEMSIZE};
		size_t len = sizeof(result);

		if (0 != sysctl(name, 2, &result, &len, NULL, 0)) {
			result = 0;
		}
	}
#elif defined(OMRZTPF)
	/* getPhysicalMemory is returning the number of 1 MB frames */
	/* for our ECB that we can use - XMMES */
	uint64_t pmem = (uint64_t)getPhysicalMemory();
	if (pmem != 0) {
		return pmem << 20;
	} else {
		return pmem;
	}
#else /* defined (RS6000) */
	result = getPhysicalMemory(portLibrary);
#endif /* defined (RS6000) */
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

uint16_t getPhysicalMemory( void ) {

	struct ebmaxc_parmlist newValues[] = {
			{ MAX64HEAP, MAXVALUE},
			{ 0, 0}
	};

	tpf_ebmaxc(newValues);
	uint16_t physMemory = *(uint16_t*)(cinfc_fast(CINFC_CMMMMES) + 8);
	int numFRM1MB  = numbc(LFRM1MB);
	/*
	 * If the available memory, i.e. number of available 1MB frames, is
	 * too low to satisfy MAX64HEAP, then the jvm cannot run. Return 0.
	 */
	if (physMemory >= (uint16_t)(numFRM1MB-1)) {
		physMemory = 0;
	}
	else {
		physMemory = physMemory * 4 / 5;
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
	}
}


int32_t
omrsysinfo_startup(struct OMRPortLibrary *portLibrary)
{
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
#endif /* defined(LINUX) */
	return 0;
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
	}

	switch (resourceRequested) {
	case OMRPORT_RESOURCE_SHARED_MEMORY:
		rc = getLimitSharedMemory(portLibrary, limit);
		break;
	case OMRPORT_RESOURCE_ADDRESS_SPACE:
		/* FALLTHROUGH */
	case OMRPORT_RESOURCE_CORE_FILE: {
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

	if (OMRPORT_RESOURCE_ADDRESS_SPACE == resourceRequested) {
#if !defined(OMRZTPF)
		resource = RLIMIT_AS;
#else /* !defined(OMRZTPF) */
		/* z/TPF does not have virtual address support */
		rc = -1;
		Trc_PRT_sysinfo_set_limit_Exit(rc);
		return rc;
#endif /* !defined(OMRZTPF) */
	} else if (OMRPORT_RESOURCE_CORE_FILE == resourceRequested) {
#if !defined(OMRZTPF)
		resource = RLIMIT_CORE;
#else /* !defined(OMRZTPF) */
		rc = -1;
		Trc_PRT_sysinfo_set_limit_Exit(rc);
		return rc;
#endif /* !defined(OMRZTPF) */
	}

	switch (resourceRequested) {
	case OMRPORT_RESOURCE_ADDRESS_SPACE:
		/* FALLTHROUGH */
	case OMRPORT_RESOURCE_CORE_FILE: {
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
			lim.rlim_cur = limit;
		}

		rc = setrlimit(resource, &lim);
		if (-1 == rc) {
			portLibrary->error_set_last_error(portLibrary, errno, findError(errno));
			Trc_PRT_sysinfo_setrlimit_error(resource, limit, findError(errno));
		}
#else /* !defined(OMRZTPF) */
		rc = -1;
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
		rc = -1;
#endif
		break;
	}

	default:
		Trc_PRT_sysinfo_setLimit_unrecognised_resourceID(resourceID);
		rc = -1;
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
	 * 		cpu <user> <nice> <system> <other data we don't use>
	 * where user, nice, and system are 64-bit numbers in units of USER_HZ (typically 10 ms).
	 *
	 * Each number will have up to 21 decimal digits.  Adding spaces and the "cpu  " prefix brings the total to
	 * ~70.  Allocate 128 bytes to give lots of margin.
	 */
	char buf[128];
	const uintptr_t NS_PER_HZ = 1000000000 / USER_HZ;
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
		cpuTime->cpuTime = (userTime + niceTime + systemTime) * NS_PER_HZ;
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
	return OMRPORT_ERROR_SYSINFO_NOT_SUPPORTED;
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
			limitElement->softValue	= (uint64_t) OMRPORT_LIMIT_UNLIMITED;
		} else {
			limitElement->softValue = (uint64_t) limits.rlim_cur;
		}

		if (RLIM_INFINITY == limits.rlim_max) {
			limitElement->hardValue	= (uint64_t) OMRPORT_LIMIT_UNLIMITED;
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
	mbtowc(NULL, NULL, 0);

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
 * Initialises the supplied buffer such that it can be used by the @ref omrsysinfo_env_iteraror_next() and @ref omrsysinfo_env_iterator_hasNext() APIs.
 *
 * If the caller supplies a non-NULL buffer that is not big enough to store all the name=value entries of the environment,
 * 	copyEnvToBuffer will store as many entries as possible, until the buffer is used up.
 *
 * If the buffer wasn't even big enough to contain a single entry, set its entire contents to '\0'
 *
 * This call must be protected because memory in char **environ may become inaccessible while copying, resulting in a crash.
 *
 * @param[in] args pointer to a CopyEnvToBufferArgs structure
 *
 * @return
 * 		 - 0 if the buffer was big enough to store the entire environment.
 * 		 - the required size of the buffer, if it was not big enough to store the entire environment.
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
										+ (intptr_t)(strlen(environ[i]) * J9_MAX_UTF8_SIZE_BYTES)	/* for the string itself (name=value ) */
										+ 1;	/* for the null-terminator */
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

#define PROCSTATPATH	"/proc/stat"
#define PROCSTATPREFIX	"cpu"

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
	while ((-1 != getline(&linePtr, &lineSz, procStatFs)) &&
		   (0 == strncmp(linePtr, PROCSTATPREFIX, sizeof(PROCSTATPREFIX) - 1))) {
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
	procInfo->procInfoArray[0].busyTime = procInfo->procInfoArray[0].userTime +
										  procInfo->procInfoArray[0].systemTime +
										  procInfo->procInfoArray[0].waitTime;

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
	portLibrary->str_printf(portLibrary, errmsgbuff, errmsglen, "%s%s", funcName,	strerror(systemErrno));

	/*Set the error message*/
	portLibrary->error_set_last_error_with_message(portLibrary, portableErrno, errmsgbuff);

	/*Free the buffer*/
	portLibrary->mem_free_memory(portLibrary, errmsgbuff);

	return;
}

int32_t
omrsysinfo_get_open_file_count(struct OMRPortLibrary *portLibrary, uint64_t *count)
{
	int32_t ret = 0;
#if defined(LINUX) || defined(AIXPPC)
	uint64_t fdCount = 0;
	char buffer[PATH_MAX] = {0};
	const char *procDirectory = "/proc/%d/fd/";
	struct dirent *dp = NULL;
	DIR *dir = NULL;

	Trc_PRT_sysinfo_get_open_file_count_Entry();
	/* Check whether a valid pointee exists, to write to. */
	if (NULL == count) {
		portLibrary->error_set_last_error(portLibrary, EINVAL, findError(EINVAL));
		Trc_PRT_sysinfo_get_open_file_count_invalidArgRecvd("count");
		ret = -1;
		goto leave_routine;
	}
	/* On Linux, "/proc/self" is as good as "/proc/<current-pid>/", but not on
	 * AIX, where the current process's PID must be specified.
	 */
	portLibrary->str_printf(portLibrary, buffer, sizeof(buffer), procDirectory, getpid());
	dir = opendir(buffer);
	if (NULL == dir) {
		int32_t rc = findError(errno);
		portLibrary->error_set_last_error(portLibrary, errno, rc);
		Trc_PRT_sysinfo_get_open_file_count_failedOpeningProcFS(rc);
		ret = -1;
		goto leave_routine;
	}
	/* opendir(3) was successful; look into the directory for fds. */
	errno = 0; /* readdir(3) will set this, if an error occurs. */
	dp = readdir(dir);
	while (NULL != dp) {
		/* Skip "." and ".." */
		if (!(('.' == dp->d_name[0] && '\0' == dp->d_name[1])
		||    ('.' == dp->d_name[0] && '.' == dp->d_name[1] && '\0' == dp->d_name[2]))
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
		ret = -1;
	} else {
		/* Successfully counted up the files opened. */
		*count = fdCount;
		Trc_PRT_sysinfo_get_open_file_count_fileCount(fdCount);
	}
	closedir(dir); /* Done reading the /proc file-system. */
leave_routine:
	/* Clean-ups, setting-error (if any), etc, done. */
#elif defined(OSX) || defined(J9ZOS390)
	/* TODO: stub where z/OS || OSX code goes in. */
	ret = OMRPORT_ERROR_SYSINFO_GET_OPEN_FILES_NOT_SUPPORTED;
#endif
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

		desc->features[featureIndex] = (desc->features[featureIndex] | (1 << (featureShift)));
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

		rc = OMR_ARE_ALL_BITS_SET(desc->features[featureIndex], 1 << featureShift);
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

#if defined(LINUX)

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
addCgroupEntry(struct OMRPortLibrary *portLibrary, OMRCgroupEntry **cgEntryList, int32_t hierId, const char *subsystem, const char *cgroupName)
{
	int32_t rc = 0;
	int32_t cgEntrySize = sizeof(OMRCgroupEntry) + strlen(subsystem) + 1 + strlen(cgroupName) + 1;
	OMRCgroupEntry *cgEntry = portLibrary->mem_allocate_memory(portLibrary, cgEntrySize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL == cgEntry) {
		rc = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED, "memory allocation for cgroup entry failed");
		goto _end;
	}
	cgEntry->hierarchyId = hierId;
	cgEntry->subsystem = (char *)(cgEntry + 1);
	strcpy(cgEntry->subsystem, subsystem);
	cgEntry->cgroup = cgEntry->subsystem + strlen(subsystem) + 1;
	strcpy(cgEntry->cgroup, cgroupName);

	if (NULL == *cgEntryList) {
		*cgEntryList = cgEntry;
		cgEntry->next = cgEntry; /* create a circular list */
	} else {
		OMRCgroupEntry *first = *cgEntryList;
		*cgEntryList = cgEntry;
		cgEntry->next = first->next;
		first->next = cgEntry;
	}
_end:
	return rc;
}

/**
 * Reads /proc/<pid>/cgroup to get information about the cgroups to which the given
 * process belongs.
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
readCgroupFile(struct OMRPortLibrary *portLibrary, int pid, OMRCgroupEntry **cgroupEntryList, uint64_t *availableSubsystems)
{
	char cgroup[PATH_MAX];
	uintptr_t requiredSize = 0; 
	/* This array should be large enough to read names of all subsystems. 1024 should be enough. */
	char subsystems[1024];
	FILE *cgroupFile = NULL;
	OMRCgroupEntry *cgEntryList = NULL;
	uint64_t available = 0;
	int32_t rc = 0;

	Assert_PRT_true(NULL != cgroupEntryList);
	
	requiredSize = portLibrary->str_printf(portLibrary, NULL, (uint32_t)-1, "/proc/%d/cgroup", pid);
	Assert_PRT_true(requiredSize <= PATH_MAX);
	portLibrary->str_printf(portLibrary, cgroup, sizeof(cgroup), "/proc/%d/cgroup", pid);
	cgroupFile = fopen(cgroup, "r");
	if (NULL == cgroupFile) {
		rc = portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_FOPEN_FAILED);
		goto _end;
	}

	while (0 == feof(cgroupFile)) {
		char *cursor = NULL;
		char *separator = NULL;
		int32_t hierId = -1;

		/* Following is the description of /proc/<pid>/cgroup copied from 'man' page for proc:
 		 *
 		 * Each entry in /proc/<pid>/cgroup is of type:
		 * 	5:cpuacct,cpu,cpuset:/daemons
		 * The colon-separated fields are, from left to right:
		 * 	1. hierarchy ID number
		 * 	2. set of subsystems bound to the hierarchy
		 * 	3. control group in the hierarchy to which the process belongs
		 */
		rc = fscanf(cgroupFile, "%d:%[^:]:%s", &hierId, subsystems, cgroup);
		/* Ensure we didn't overflow */
		Assert_PRT_true(strlen(subsystems) < 1024);
		Assert_PRT_true(strlen(cgroup) < PATH_MAX);

		if (EOF == rc) {
			rc = 0;
			break;
		} else if (3 != rc) {
			rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpcted format of /proc/%d/cgroup file", pid);
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
					rc = addCgroupEntry(portLibrary, &cgEntryList, hierId, cursor, cgroup);
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
	default:
		Trc_PRT_Assert_ShouldNeverHappen();
	}

	return INVALID_SUBSYSTEM;
}

/**
 * Read a file under a subsystem in cgroup hierarchy based on the format specified
 *
 * @param[in] portLibrary pointer to OMRPortLibrary
 * @param[in] subsystemFlag flag bitwise-OR of flags of type OMR_CGROUP_SUBSYSTEMS_* representing the cgroup subsystem
 * @param[in] fileName name of the file under cgroup subsystem
 * @param[in] numItemsToRead number of items to be read from the file
 * @param[in] format format to be used for reading the file
 *
 * @return 0 on successfully reading 'numItemsToRead' items from the file, negative error code on any error
 */
static int32_t
readCgroupSubsystemFile(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlag, const char *fileName, int32_t numItemsToRead, const char *format, ...)
{
	char fileToRead[PATH_MAX];
	char *fileNameBuf = fileToRead;
	intptr_t fileNameLen = 0;
	char *cgroup = NULL;
	FILE *file = NULL;
	int32_t rc = 0;
	va_list args;
	OMRCgroupSubsystem subsystem = getCgroupSubsystemFromFlag(subsystemFlag);
	uint64_t availableSubsystem = portLibrary->sysinfo_cgroup_are_subsystems_available(portLibrary, subsystemFlag);

	if (availableSubsystem != subsystemFlag) {
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_SUBSYSTEM_UNAVAILABLE, "cgroup subsystem %s is not available", subsystemNames[subsystem]);
		goto _end;
	}
	
	cgroup = getCgroupNameForSubsystem(portLibrary, PPG_cgroupEntryList, subsystemNames[subsystem]);
	if (NULL == cgroup) {
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_CGROUP_NAME_NOT_AVAILABLE, "cgroup name for subsystem %s is not available", subsystemNames[MEMORY]);
		goto _end;
	}

	/* absolute path of the file to be read is: /sys/fs/cgroup/subsystemNames[subsystem]/cgroup/filenName */
	fileNameLen = portLibrary->str_printf(portLibrary, NULL, (uint32_t)-1, "%s/%s/%s/%s", OMR_CGROUP_V1_MOUNT_POINT, subsystemNames[subsystem], cgroup, fileName);
	if (fileNameLen > PATH_MAX) {
		fileNameBuf = portLibrary->mem_allocate_memory(portLibrary, fileNameLen, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == fileNameBuf) {
			rc = portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_SYSINFO_MEMORY_ALLOC_FAILED, "memory allocation for filename buffer failed");
			goto _end;
			
		}
	}
	portLibrary->str_printf(portLibrary, fileNameBuf, fileNameLen, "%s/%s/%s/%s", OMR_CGROUP_V1_MOUNT_POINT, subsystemNames[subsystem], cgroup, fileName);
	file = fopen(fileNameBuf, "r");
	if (NULL == file) {
		portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_SYSINFO_CGROUP_MEMLIMIT_FILE_FOPEN_FAILED);
		goto _end;
	}

	va_start(args, format);	
	rc = vfscanf(file, format, args);
	va_end(args);

	if (numItemsToRead != rc) {
		rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_PROCESS_CGROUP_FILE_READ_FAILED, "unexpected format of file %s", fileNameBuf);
		goto _end;
	} else {
		rc = 0;
	}

_end:
	if (fileToRead != fileNameBuf) {
		portLibrary->mem_free_memory(portLibrary, fileNameBuf);
	}
	if (NULL != file) {
		fclose(file);
	}
	return rc;
}

#endif /* defined(LINUX) */

BOOLEAN
omrsysinfo_cgroup_is_system_available(struct OMRPortLibrary *portLibrary)
{
	BOOLEAN result = FALSE;
#if defined(LINUX) && !defined(OMRZTPF)
	int32_t rc = OMRPORT_ERROR_SYSINFO_CGROUP_UNSUPPORTED_PLATFORM;

	if (NULL == PPG_cgroupEntryList) {
		struct statfs buf = {0};

		/* If tmpfs is mounted on /sys/fs/cgroup, then it indicates cgroup v1 system is available */
		rc = statfs(OMR_CGROUP_V1_MOUNT_POINT, &buf);
		if (0 != rc) {
			rc = portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_ERROR_SYSINFO_SYS_FS_CGROUP_STATFS_FAILED);
			goto _end;
		} else if (TMPFS_MAGIC != buf.f_type) {
			rc = portLibrary->error_set_last_error_with_message_format(portLibrary, OMRPORT_ERROR_SYSINFO_SYS_FS_CGROUP_TMPFS_NOT_MOUNTED, "tmpfs is not mounted on " OMR_CGROUP_V1_MOUNT_POINT);
			goto _end;
		}

		omrthread_monitor_enter(cgroupEntryListMonitor);
		if (NULL == PPG_cgroupEntryList) {
			rc = readCgroupFile(portLibrary, getpid(), &PPG_cgroupEntryList, &PPG_cgroupSubsystemsAvailable);
		}
		omrthread_monitor_exit(cgroupEntryListMonitor);
		if (0 != rc) {
			goto _end;
		}
	} else {
		rc = 0;
	}
_end:
	if (0 == rc) {
		result = TRUE;
	} else {
		PPG_cgroupSubsystemsAvailable = 0;
	}
#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return result;
}

uint64_t
omrsysinfo_cgroup_get_available_subsystems(struct OMRPortLibrary *portLibrary)
{
#if defined(LINUX) && !defined(OMRZTPF)
	if (NULL == PPG_cgroupEntryList) {
		portLibrary->sysinfo_cgroup_is_system_available(portLibrary);
	}
	return PPG_cgroupSubsystemsAvailable;
#else /* defined(LINUX) && !defined(OMRZTPF) */
	return 0;
#endif /* defined(LINUX) && !defined(OMRZTPF) */
}

uint64_t
omrsysinfo_cgroup_are_subsystems_available(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlags)
{
#if defined(LINUX) && !defined(OMRZTPF)
	uint64_t available = portLibrary->sysinfo_cgroup_get_available_subsystems(portLibrary);
	return (available & subsystemFlags);
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
	uint64_t available = portLibrary->sysinfo_cgroup_get_available_subsystems(portLibrary);
	PPG_cgroupSubsystemsEnabled = available & requestedSubsystems;
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
#if defined(LINUX) && !defined(OMRZTPF)
	uint64_t cgroupMemLimit = 0;
	uint64_t physicalMemLimit = 0;
	int32_t numItemsToRead = 1; /* memory.limit_in_bytes file contains only one integer value */

	Assert_PRT_true(NULL != limit);

	rc = readCgroupSubsystemFile(portLibrary, OMR_CGROUP_SUBSYSTEM_MEMORY, "memory.limit_in_bytes", numItemsToRead, "%lu", &cgroupMemLimit);

	if (0 != rc) {
		goto _end;
	}

	physicalMemLimit = getPhysicalMemory(portLibrary);
	/* If the cgroup is not imposing any memory limit then the value in memory.limit_in_bytes
	 * is close to max value of 64-bit integer, and is more than the physical memory in the system.
	 * In such case, just return the amount of physical memory.
	 */
	if (cgroupMemLimit > physicalMemLimit) {
		cgroupMemLimit = physicalMemLimit;
	}

	*limit = cgroupMemLimit;
	rc = 0;
_end:
#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return rc;
}

#if defined(OMRZTPF)
/*
 *	Return the number of I-streams ("processors", as called by other
 *	systems) in an unsigned integer as detected at IPL time.
 */
uintptr_t
get_IPL_IstreamCount( void ) {
	struct dctist *ist;
	ist = (struct dctist *)cinfc_fast(CINFC_CMMIST);
	int numberOfIStreams = ist->istactis;
	return (uintptr_t)numberOfIStreams;
}

 /*
 *      Return the number of I-streams ("processors", as called by other
 *      systems) in an unsigned integer as detect at Process Dispatch time.
 */
uintptr_t
get_Dispatch_IstreamCount( void ) {
	struct dctist *ist;
	ist = (struct dctist *)cinfc_fast(CINFC_CMMIST);
	int numberOfIStreams = ist->istuseis;
	return (uintptr_t)numberOfIStreams;
}
#endif /* defined(OMRZTPF) */
