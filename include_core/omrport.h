/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#if !defined(OMRPORT_H_)
#define OMRPORT_H_

/*
 * @ddr_namespace: map_to_type=J9PortLibrary
 */

#include "omrcfg.h"

/* NOTE:  omrportlib.h include is at the bottom of this file until its dependencies on this file can be relaxed */

/* fix for linux s390 32bit stdint vs unistd.h definition of intptr_t (see CMVC 73850) */
#if defined(LINUX) && defined(S390)
#include <stdint.h>
#endif

#include <stdarg.h>	/* for va_list */
#include <stdio.h> /* For FILE */
#include <signal.h>
#include "omrcomp.h"
#include "omrthread.h"
#include "omrmemcategories.h"
#include "omrporterror.h"
#include "omrportsock.h"
#if defined(OMR_OPT_CUDA)
#include "omrcuda.h"
#endif /* OMR_OPT_CUDA */
#if defined(OMRZTPF)
#include "omrgcconsts.h"
#endif /* defined(OMRZTPF) */

#if !defined(OMR_OS_WINDOWS)
#include <unistd.h>
#endif /* !defined(OMR_OS_WINDOWS) */

#if defined(J9ZOS390)
#define PORT_ABEND_CODE	0xDED
#define PORT_ABEND_REASON_CODE 20
#define PORT_ABEND_CLEANUP_CODE 1 	/* allow for normal enclave termination/cleanup processing */
#endif

/**
 * @name Port library access
 * @anchor PortAccess
 * Macros for accessing port library.
 * @{
 */
#define OMRPORT_ACCESS_FROM_OMRPORT(_omrPortLib) OMRPortLibrary *privateOmrPortLibrary = (_omrPortLib)
/** @} */

#define OMRPORTLIB privateOmrPortLibrary

/**
 * @name File Operations
 * @anchor PortFileOperations
 * File operation modifiers.
 * @{
 */
#ifdef		SEEK_SET
#define	EsSeekSet	SEEK_SET	/* Values for EsFileSeek */
#else
#define	EsSeekSet	0
#endif
#ifdef 		SEEK_CUR
#define	EsSeekCur	SEEK_CUR
#else
#define	EsSeekCur	1
#endif
#ifdef		SEEK_END
#define	EsSeekEnd	SEEK_END
#else
#define	EsSeekEnd	2
#endif

#define	EsOpenRead			0x1	/* Values for EsFileOpen */
#define	EsOpenWrite			0x2
#define	EsOpenCreate		0x4
#define	EsOpenTruncate		0x8
#define	EsOpenAppend		0x10
#define	EsOpenText			0x20
#define	EsOpenCreateNew 	0x40		/* Use this flag with EsOpenCreate, if this flag is specified then trying to create an existing file will fail */
#define	EsOpenSync			0x80
#define EsOpenForMapping	0x100 /* Required for WinCE for file memory mapping, ignored on other platforms. WINCE is not supported so this flag is obsolete now. */
#define	EsOpenForInherit 	0x200	/* Use this flag such that returned handle can be inherited by child processes */
#define	EsOpenCreateAlways 	0x400	/* Always creates a new file, an existing file will be overwritten */
#define EsOpenCreateNoTag 	0x800	/* Used for zOS only, to disable USS file tagging on JVM-generated files */
#define EsOpenShareDelete 	0x1000  /* used only for windows to allow a file to be renamed while it is still open */
#define EsOpenAsynchronous 	0x2000  /* used only for windows to allow a file to be opened asynchronously */

#define EsIsDir 	0	/* Return values for EsFileAttr */
#define EsIsFile 	1

/* Invalid file descriptor */
#define OMRPORT_INVALID_FD	-1

/* Filestream Buffering modes */
#define OMRPORT_FILESTREAM_FULL_BUFFERING _IOFBF
#define OMRPORT_FILESTREAM_LINE_BUFFERING _IOLBF
#define OMRPORT_FILESTREAM_NO_BUFFERING _IONBF

/** EsMaxPath was chosen from unix MAXPATHLEN.  Override in platform
  * specific omrfile implementations if needed.
  */
#define EsMaxPath 	1024
/** @} */

/**
 * @name Sysinfo get limit success flags
 * @anchor PortSharedMemorySuccessFlags
 * Return codes related to sysinfo get limit operations.
 * @{
 * @internal OMRPORT_LIMIT* range from at 120 to 129 to avoid overlap
 */
#define OMRPORT_LIMIT_BASE 120
#define OMRPORT_LIMIT_UNLIMITED (OMRPORT_LIMIT_BASE)
#define OMRPORT_LIMIT_UNKNOWN (OMRPORT_LIMIT_BASE+1)
#define OMRPORT_LIMIT_LIMITED (OMRPORT_LIMIT_BASE+2)
/** @} */

/**
 * @name Sysinfo Limits
 * Flags used to indicate type of operation for omrsysinfo_get_limit
 * @{
 */
#define OMRPORT_LIMIT_SOFT ((uintptr_t) 0x0)
#define OMRPORT_LIMIT_HARD ((uintptr_t) 0x80000000)
#define OMRPORT_RESOURCE_SHARED_MEMORY ((uintptr_t) 1)
#define OMRPORT_RESOURCE_ADDRESS_SPACE ((uintptr_t) 2)
#define OMRPORT_RESOURCE_CORE_FILE ((uintptr_t) 3)
#define OMRPORT_RESOURCE_CORE_FLAGS ((uintptr_t) 4)
#define OMRPORT_RESOURCE_FILE_DESCRIPTORS ((uintptr_t) 5)
#define OMRPORT_RESOURCE_DATA ((uintptr_t) 6)
/** @} */

/**
 * @name Sysinfo Limits - return values
 * These values are returned by omrsysinfo_get_limit in the limit parameter for the corresponding return codes.
 * If a value has been determined for a limit, it is the value reurned in the limit parameter.
 * @{
 */
#define OMRPORT_LIMIT_UNLIMITED_VALUE (J9CONST64(0xffffffffffffffff))
#define OMRPORT_LIMIT_UNKNOWN_VALUE (J9CONST64(0xffffffffffffffff))
/** @} */

/**
 * @name OMRSIG support (optional)
 * OMRSIG
 * @{
 */
#if defined(OMRPORT_OMRSIG_SUPPORT)
#define OMRSIG_SIGNAL(signum, handler) omrsig_primary_signal(signum, handler)
#define OMRSIG_SIGACTION(signum, act, oldact) omrsig_primary_sigaction(signum, act, oldact)
#else /* defined(OMRPORT_OMRSIG_SUPPORT) */
#define OMRSIG_SIGNAL(signum, handler) signal(signum, handler)
#define OMRSIG_SIGACTION(signum, act, oldact) sigaction(signum, act, oldact)
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */
/** @} */

/**
 * @deprecated
 *
 * @name Native Language Support
 * Native Language Support
 * @{
 * @internal standards require that all VM messages be prefixed with JVM.
 */
#define J9NLS_COMMON_PREFIX "JVM"
#define J9NLS_ERROR_PREFIX ""
#define J9NLS_WARNING_PREFIX ""
#define J9NLS_INFO_PREFIX ""
#define J9NLS_ERROR_SUFFIX "E"
#define J9NLS_WARNING_SUFFIX "W"
#define J9NLS_INFO_SUFFIX "I"

/** @internal these macros construct in string literals from message ids. */
#define J9NLS_MESSAGE(id, message) ("" J9NLS_COMMON_PREFIX "" id##__PREFIX " " message)
#define J9NLS_ERROR_MESSAGE(id, message) ("" J9NLS_ERROR_PREFIX "" J9NLS_COMMON_PREFIX "" id##__PREFIX "" J9NLS_ERROR_SUFFIX " " message)
#define J9NLS_INFO_MESSAGE(id, message) ("" J9NLS_INFO_PREFIX "" J9NLS_COMMON_PREFIX "" id##__PREFIX "" J9NLS_INFO_SUFFIX " " message)
#define J9NLS_WARNING_MESSAGE(id, message) ("" J9NLS_WARNING_PREFIX "" J9NLS_COMMON_PREFIX "" id##__PREFIX "" J9NLS_WARNING_SUFFIX " " message)
/** @} */

/**
 * @name Virtual Memory Access
 * Flags used to describe type of the page for the virtual memory
 * @{
 */
#define OMRPORT_VMEM_PAGE_FLAG_NOT_USED 0x1
#define OMRPORT_VMEM_PAGE_FLAG_FIXED 0x2
#define OMRPORT_VMEM_PAGE_FLAG_PAGEABLE 0x4
#define OMRPORT_VMEM_PAGE_FLAG_SUPERPAGE_ANY 0x8
#define OMRPORT_VMEM_PAGE_FLAG_PAGEABLE_PREFERABLE 0x10

#define OMRPORT_VMEM_PAGE_FLAG_TYPE_MASK 0x1F
/** @} */

/**
 * @name Virtual Memory Access
 * Flags used to create bitmap indicating memory access
 * @{
 */
#define OMRPORT_VMEM_MEMORY_MODE_READ 0x00000001
#define OMRPORT_VMEM_MEMORY_MODE_WRITE 0x00000002
#define OMRPORT_VMEM_MEMORY_MODE_EXECUTE 0x00000004
#define OMRPORT_VMEM_MEMORY_MODE_COMMIT 0x00000008
#define OMRPORT_VMEM_MEMORY_MODE_VIRTUAL 0x00000010
#define OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN 0x000000200
#define OMRPORT_VMEM_MEMORY_MODE_MMAP_HUGE_PAGES 0x000000400
#define OMRPORT_VMEM_MEMORY_MODE_DOUBLE_MAP_AVAILABLE 0x000000800
/* If OMRPORT_VMEM_MEMORY_MODE_SHARE_TMP_FILE_OPEN is set,
 * then OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN must be set as well.
 */
#define OMRPORT_VMEM_MEMORY_MODE_SHARE_TMP_FILE_OPEN 0x000001000
#define OMRPORT_VMEM_ALLOCATE_TOP_DOWN 0x00000020
#define OMRPORT_VMEM_ALLOCATE_PERSIST 0x00000040
#define OMRPORT_VMEM_NO_AFFINITY 0x00000080
/** @} */

/**
 * @name Timer Resolution
 * @anchor timerResolution
 * Define resolution requested in @ref omrtime::omrtime_hires_delta
 * @{
 */
#define OMRPORT_TIME_DELTA_IN_SECONDS ((uint64_t) 1)
#define OMRPORT_TIME_DELTA_IN_MILLISECONDS ((uint64_t) 1000)
#define OMRPORT_TIME_DELTA_IN_MICROSECONDS ((uint64_t) 1000000)
#define OMRPORT_TIME_DELTA_IN_NANOSECONDS ((uint64_t) 1000000000)
/** @} */

#if defined(S390) || defined(J9ZOS390)
/**
 * @name Constants to calculate time from high-resolution timer
 * @anchor hiresConstants
 * High-resolution timer can be converted into millisecond timer or nanosecond timer through appropriate
 * division or multiplication (respectively). For example, 390 high-resolution timer provides accuracy to
 * 1/2048 of a microsecond so, to convert hires timer to
 * - nanoseconds, multiply it by 125/256
 * - microseconds, divide it by 2,048
 * - milliseconds, divide it by 2,048,000
 * @{
 */
#define OMRPORT_TIME_HIRES_NANOTIME_NUMERATOR ((uint64_t) 125)
#define OMRPORT_TIME_HIRES_NANOTIME_DENOMINATOR ((uint64_t) 256)
#define OMRPORT_TIME_HIRES_MICROTIME_DIVISOR ((uint64_t) 2048)
#define OMRPORT_TIME_HIRES_MILLITIME_DIVISOR ((uint64_t) 2048000)
#define OMRTIME_HIRES_CLOCK_FREQUENCY ((uint64_t) 2048000000) /* Frequency is microseconds / second */

#else /* defined(S390) || defined(J9ZOS390) */

#define OMRPORT_TIME_HIRES_NANOTIME_NUMERATOR ((uint64_t) 0)
#define OMRPORT_TIME_HIRES_NANOTIME_DENOMINATOR ((uint64_t) 0)
#define OMRPORT_TIME_HIRES_MICROTIME_DIVISOR ((uint64_t) 0)
#define OMRPORT_TIME_HIRES_MILLITIME_DIVISOR ((uint64_t) 0)

#endif /* defined(S390) || defined(J9ZOS390) */
/** @} */

#if defined(LINUX) && !defined(OMRZTPF)
#define OMR_CONFIGURABLE_SUSPEND_SIGNAL
#endif /* defined(LINUX) */

/**
 * @name Time Unit Conversion
 * Constants used to convert between units of time.
 * @{
 */
#define OMRPORT_TIME_NS_PER_MS ((uint64_t) 1000000)	/* nanoseconds per millisecond */
#define OMRPORT_TIME_US_PER_SEC ((uint64_t) 1000000) /* microseconds per second */
/** @} */

/**
 * @name Shared Semaphore
 * Flags used to indicate type of operation for omrshsem_post/omrshsem_wait
 * @{
 */
#define OMRPORT_SHSEM_MODE_DEFAULT ((uintptr_t) 0)
#define OMRPORT_SHSEM_MODE_UNDO ((uintptr_t) 1)
#define OMRPORT_SHSEM_MODE_NOWAIT ((uintptr_t) 2)
/** @} */

/**
 * @name Shared Semaphore Success flags
 * @anchor PortSharedSemaphoreSuccessFlags
 * Success codes related to shared semaphore  operations.
 * @{
 * @internal OMRPORT_INFO_SHSEM* range from at 100 to 109 to avoid overlap
 */
#define OMRPORT_INFO_SHSEM_BASE 100
#define OMRPORT_INFO_SHSEM_CREATED (OMRPORT_INFO_SHSEM_BASE)
#define OMRPORT_INFO_SHSEM_OPENED (OMRPORT_INFO_SHSEM_BASE+1)
#define OMRPORT_INFO_SHSEM_OPEN_UNLINKED (OMRPORT_INFO_SHSEM_BASE+2)
#define OMRPORT_INFO_SHSEM_OPENED_STALE (OMRPORT_INFO_SHSEM_BASE+3)
#define OMRPORT_INFO_SHSEM_PARTIAL (OMRPORT_INFO_SHSEM_BASE+4)
#define OMRPORT_INFO_SHSEM_STAT_PASSED (OMRPORT_INFO_SHSEM_BASE+5)

/** @} */

#define OMRSH_MAXPATH EsMaxPath

#define OMRSH_SEMAPHORE_ID "_semaphore_"
#define OMRSH_MEMORY_ID "_memory_"

#define OMRSH_DIRPERM_ABSENT ((uintptr_t)-2)
#define OMRSH_DIRPERM (0777)
#define OMRSH_PARENTDIRPERM (01777)
#define OMRSH_DIRPERM_DEFAULT (0000)
#define OMRSH_DIRPERM_DEFAULT_WITH_STICKYBIT (01000)
#define OMRSH_BASEFILEPERM (0644)
#define OMRSH_BASEFILEPERM_GROUP_RW_ACCESS (0664)

#define OMRSH_SHMEM_PERM_READ (0444)
#define OMRSH_SHMEM_PERM_READ_WRITE (0644)

#define OMRSH_SYSV_REGULAR_CONTROL_FILE 0
#define OMRSH_SYSV_OLDER_CONTROL_FILE 1
#define OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE 2

/* Flags passed to "flag" argument of omrshsem_deprecated_open(). */
#define OMRSHSEM_NO_FLAGS 0x0
#define OMRSHSEM_OPEN_FOR_STATS 0x1
#define OMRSHSEM_OPEN_FOR_DESTROY 0x2
#define OMRSHSEM_OPEN_DO_NOT_CREATE 0x4
/*
 * Flags passed to "flag" argument of omrshmem_open(). Should be of type uintptr_t.
 * High order 4 bits are reserved for passing the storage key testing value to omrshmem.
 */
#define OMRSHMEM_NO_FLAGS 0x0
#define OMRSHMEM_OPEN_FOR_STATS 0x1
#define OMRSHMEM_OPEN_FOR_DESTROY 0x2
#define OMRSHMEM_PRINT_STORAGE_KEY_WARNING 0x4
#define OMRSHMEM_STORAGE_KEY_TESTING 0x8
#define OMRSHMEM_OPEN_DO_NOT_CREATE 0x10

#define OMRSHMEM_STORAGE_KEY_TESTING_SHIFT ((sizeof(uintptr_t)*8)-4)
#define OMRSHMEM_STORAGE_KEY_TESTING_MASK 0xF

/* Flags passed to "flags" argument of omrshmem_getDir(). */
#define OMRSHMEM_GETDIR_APPEND_BASEDIR 0x1
#define OMRSHMEM_GETDIR_USE_USERHOME 0x2

#ifdef WIN32
#define OMRSH_BASEDIR "omrsharedresources\\"
#else
#define OMRSH_BASEDIR "omrsharedresources/"
#endif

/**
 * @name Shared Memory Success flags
 * @anchor PortSharedMemorySuccessFlags
 * Success codes related to shared memory semaphore operations.
 * @{
 * @internal OMRPORT_INFO_SHMEM* range from at 110 to 119 to avoid overlap
 */
#define OMRPORT_INFO_SHMEM_BASE 110
#define OMRPORT_INFO_SHMEM_CREATED (OMRPORT_INFO_SHMEM_BASE)
#define OMRPORT_INFO_SHMEM_OPENED (OMRPORT_INFO_SHMEM_BASE+1)
#define OMRPORT_INFO_SHMEM_OPEN_UNLINKED (OMRPORT_INFO_SHMEM_BASE+2)
#define OMRPORT_INFO_SHMEM_OPENED_STALE (OMRPORT_INFO_SHMEM_BASE+3)
#define OMRPORT_INFO_SHMEM_PARTIAL (OMRPORT_INFO_SHMEM_BASE+4)
#define OMRPORT_INFO_SHMEM_STAT_PASSED (OMRPORT_INFO_SHMEM_BASE+5)

/** @} */

/**
 * @name Shared Memory Eyecatcher
 * @anchor PortSharedMemoryEyecatcher
 * Eyecatcher written to start of a shared classes cache to identify the shared memory segment as such a cache
 * @{
 */

#define OMRPORT_SHMEM_EYECATCHER "OMRSC"
#define OMRPORT_SHMEM_EYECATCHER_LENGTH 5
/** @} */

#define ROUND_UP_TO_POWEROF2(value, powerof2) (((value) + ((powerof2) - 1)) & (UDATA)~((powerof2) - 1))
#define ROUND_DOWN_TO_POWEROF2(value, powerof2) ((value) & (UDATA)~((powerof2) - 1))

typedef struct J9Permission {
	uint32_t isUserWriteable : 1;
	uint32_t isUserReadable : 1;
	uint32_t isGroupWriteable : 1;
	uint32_t isGroupReadable : 1;
	uint32_t isOtherWriteable : 1;
	uint32_t isOtherReadable : 1;
	uint32_t : 26; /* future use */
} J9Permission;

typedef struct OMRPortShmemStatistic {
	uintptr_t shmid;
	uintptr_t nattach;
	uintptr_t key;
	uintptr_t ouid;
	uintptr_t ogid;
	uintptr_t cuid;
	uintptr_t cgid;
	char* file;
	uintptr_t size;
	int64_t lastAttachTime;
	int64_t lastDetachTime;
	int64_t lastChangeTime;
	char* controlDir;
	J9Permission perm;
} OMRPortShmemStatistic;

typedef struct OMRPortShsemStatistic {
	uintptr_t semid;
	uintptr_t ouid;
	uintptr_t ogid;
	uintptr_t cuid;
	uintptr_t cgid;
	int64_t lastOpTime;
	int64_t lastChangeTime;
	int32_t nsems;
	J9Permission perm;
} OMRPortShsemStatistic;

/**
 * Holds properties relating to a file. Can be added to in the future
 * if needed.
 * Note that the ownerUid and ownerGid fields are 0 on Windows.
 */
typedef struct J9FileStat {
	uint32_t isFile : 1;
	uint32_t isDir : 1;
	uint32_t isFixed : 1;
	uint32_t isRemote : 1;
	uint32_t isRemovable : 1;
	uint32_t : 27; /* future use */
	J9Permission perm;
	uintptr_t ownerUid;
	uintptr_t ownerGid;
} J9FileStat;

/**
 * Holds properties relating to a file system.
 */
typedef struct J9FileStatFilesystem {
	uint64_t freeSizeBytes;
	uint64_t totalSizeBytes;
} J9FileStatFilesystem;

/**
 * A handle to a filestream.
 * Private, platform specific implementation.
 */
typedef FILE OMRFileStream;

/* It is the responsibility of the user to create the storage for J9PortVMemParams.
 * The structure is only needed for the lifetime of the call to omrvmem_reserve_memory_ex
 * This structure must be initialized using @ref omrvmem_vmem_params_init
 */
typedef struct J9PortVmemParams {
	/* Upon success we will attempt to return a pointer within [startAddress, endAddress]
	 * in a best effort manner unless the OMRPORT_VMEM_STRICT_ADDRESS flag is specified
	 * endAddress is the last address at which the user wishes the returned pointer to
	 * be assigned regardless of the byteAmount
	 * startAddress must be <= endAddress or omrvmem_reserve_memory_ex will fail
	 * and cause the generation of a trace assertion
	 */
	void *startAddress;
	void *endAddress;

	/* This value must be aligned to pageSize when passing this structure to omrvmem_reserve_memory_ex() */
	uintptr_t byteAmount;

	/* Size of the page requested, a value returned by @ref omrvmem_supported_page_sizes */
	uintptr_t pageSize;

	/* Flags describing type of the page requested */
	uintptr_t pageFlags;

	/* @mode Bitmap indicating how memory is to be reserved.  Expected values combination of:
	 * \arg OMRPORT_VMEM_MEMORY_MODE_READ memory is readable
	 * \arg OMRPORT_VMEM_MEMORY_MODE_WRITE memory is writable
	 * \arg OMRPORT_VMEM_MEMORY_MODE_EXECUTE memory is executable
	 * \arg OMRPORT_VMEM_MEMORY_MODE_COMMIT commits memory as part of the reserve
	 * \arg OMRPORT_VMEM_MEMORY_MODE_VIRTUAL used only on z/OS
	 *			- used to allocate memory in 4K pages using system macros instead of malloc() or __malloc31() routines
	 *			- on 64-bit, this mode rounds up byteAmount to be aligned to 1M boundary.*
	 */
	uintptr_t mode;

	/* [Optional] Bitmap indicating direction to follow when trying to allocate memory in a range.
	 * \arg OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP start at lower address and proceed towards higher one
	 * \arg OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN start at higher address and proceed towards lower one
	 * \arg OMRPORT_VMEM_STRICT_ADDRESS fail if requested address is unavailable
	 * \arg OMRPORT_VMEM_STRICT_PAGE_SIZE fail if requested page size is unavailable
	 * \arg OMRPORT_VMEM_ZOS_USE2TO32G_AREA
	 * 			- applies to z/OS only, ignored on all other platforms
	 * 			- use allocator that exclusively requests memory in 2to32G region if set
	 * 			- do not use allocator that requests memory exclusively in 2to32G region if not set
	 * 			- if this flag is set and the 2to32G support is not there omrvmem_reserve_memory_ex will return failure
	 * \arg OMRPORT_VMEM_ALLOC_QUICK
	 *  		- enabled for Linux only
	 *  		- If set, information from /proc/self/maps is used to decide quickly if a request
	 *  		  can be satisfied. If not, NULL is returned without doing a linear search.
	 *  		- If not set, do a linear search for a memory block.
	 * \arg OMRPORT_VMEM_ADDRESS_HINT
	 *		- enabled for Linux and default page allocations only (has no effect on large page allocations)
	 *		- If not set, search memory in linear scan method
	 *		- If set, return whatever mmap gives us (only one allocation attempt)
	 *		- this option is based on the observation that mmap would take the given address as a hint about where to place the mapping
	 *		- this option does not apply to large page allocations as the allocation is done with shmat instead of mmap
	 */
	uintptr_t options;

	/* Memory allocation category for storage */
	uint32_t category;

	/* the lowest common multiple of alignmentInBytes and pageSize should be used to determine the base address */
	uintptr_t alignmentInBytes;
} J9PortVmemParams;

typedef enum J9VMemMemoryQuery {
	OMRPORT_VMEM_PROCESS_PHYSICAL,
	OMRPORT_VMEM_PROCESS_PRIVATE,
	OMRPORT_VMEM_PROCESS_VIRTUAL,
	OMRPORT_VMEM_PROCESS_EnsureWideEnum = 0x1000000
} J9VMemMemoryQuery;

#if defined(LINUX)

typedef struct OMRCgroupEntry {
	int32_t hierarchyId; /**< cgroup hierarch ID*/
	char *subsystem; /**< name of the subsystem*/
	char *cgroup; /**< name of the cgroup*/
	uint64_t flag; /**< a bit-wise flag of type OMR_CGROUP_SUBSYSTEM_* representing the cgroup*/
	struct OMRCgroupEntry *next; /**< pointer to next OMRCgroupEntry*/
} OMRCgroupEntry;

#endif /* defined(LINUX) */

typedef struct OMRCgroupMetricElement {
	const char *units;
	char value[128];
} OMRCgroupMetricElement;

typedef struct OMRCgroupMetricIteratorState {
	uint32_t count;
	uint32_t numElements;
	uint64_t subsystemid;
	int32_t fileMetricCounter;
	char *fileContent;
} OMRCgroupMetricIteratorState;



/**
 * @name Virtual Memory Options
 * Flags used to create bitmap indicating vmem options
 * See J9PortVmemParams.options
 *
 */
#define OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP	1
#define OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN	2
#define OMRPORT_VMEM_STRICT_ADDRESS		4
#define OMRPORT_VMEM_STRICT_PAGE_SIZE	8
#define OMRPORT_VMEM_ZOS_USE2TO32G_AREA 16
#define OMRPORT_VMEM_ALLOC_QUICK 		32
#define OMRPORT_VMEM_ZTPF_USE_31BIT_MALLOC 64
#define OMRPORT_VMEM_ADDRESS_HINT 128

/**
 * @name Virtual Memory Address
 * highest memory address on platform
 *
 */
#if defined(J9ZOS390) && !defined(OMR_ENV_DATA64)
/* z/OS 31-bit uses signed pointer comparison so this UDATA_MAX maximum address becomes -1 which is less than the minimum address of 0 so use IDATA_MAX instead */
#define OMRPORT_VMEM_MAX_ADDRESS ((void *) IDATA_MAX)
#else /* defined(J9ZOS390) && !defined(OMR_ENV_DATA64) */
#define OMRPORT_VMEM_MAX_ADDRESS ((void *) UDATA_MAX)
#endif /* defined(J9ZOS390) && !defined(OMR_ENV_DATA64) */

/**
 * @name Memory Tagging
 * Eye catcher and header/footer used for tagging omrmem allocation blocks
 *
 */
#define J9MEMTAG_TAG_CORRUPTION						0xFFFFFFFF

#define J9MEMTAG_VERSION							0
#define J9MEMTAG_EYECATCHER_ALLOC_HEADER			0xB1234567
#define J9MEMTAG_EYECATCHER_ALLOC_FOOTER			0xB7654321
#define J9MEMTAG_EYECATCHER_FREED_HEADER			0xBADBAD67
#define J9MEMTAG_EYECATCHER_FREED_FOOTER			0xBADBAD21
#define J9MEMTAG_PADDING_BYTE						0xDD

typedef struct J9MemTag {
	uint32_t eyeCatcher;
	uint32_t sumCheck;
	uintptr_t allocSize;
	const char *callSite;
	OMRMemCategory *category;
#if !defined(OMR_ENV_DATA64)
	/* omrmem_allocate_memory should return addresses aligned to 8 bytes for
	 * performance reasons. On 32 bit platforms we have to pad to achieve this.
	 */
	uint8_t padding[4];
#endif
} J9MemTag;

#if defined(LINUX) || defined(OSX)
/**
 * @name Linux OS Dump Eyecatcher
 *
 */

#define J9OSDUMP_EYECATCHER	0x19810924
#if defined(OMR_ENV_DATA64)
#define J9OSDUMP_SIZE	(192 * 1024)
#else
#define J9OSDUMP_SIZE	(128 * 1024)
#endif
#endif /* defined(LINUX) || defined(OSX) */

/* omrfile_chown takes unsigned arguments for user/group IDs, but uses -1 to indicate that group/user id are not to be changed */
#define OMRPORT_FILE_IGNORE_ID UDATA_MAX

/* Used by omrsysinfo_limit_iterator_init/next functions */
typedef struct J9SysinfoUserLimitElement {

	/* Null terminated string  */
	const char *name;

	uint64_t softValue;
	uint64_t hardValue;
	/** 1. If the OS indicates a limit is set to unlimited, the corresponding soft/hard value will be set to
	 *  	OMRPORT_LIMIT_UNLIMITED
	 *  2. There may not be the notion of a hard value on some platforms (for example Windows), in which case
	 * 		the hard value will be set to OMRPORT_LIMIT_UNLIMITED.
	 */

} J9SysinfoUserLimitElement;

typedef struct J9SysinfoLimitIteratorState {
	uint32_t count;
	uint32_t numElements;
} J9SysinfoLimitIteratorState;

/* Used by omrsysinfo_env_iterator_init/next functions */

typedef struct J9SysinfoEnvElement {
	/* Null terminated string in format "name=value" */
	const char *nameAndValue;
} J9SysinfoEnvElement;

typedef struct J9SysinfoEnvIteratorState {
	void *current;		/* to be used exclusively by the port library */
	void *buffer; 		/* caller-allocated buffer. This can be freed by the caller once they have finished using the iterator. */
	uintptr_t bufferSizeBytes;  /* size of @ref buffer */
} J9SysinfoEnvIteratorState;

/* Used by omrsysinfo_get_CPU_utilization() */
typedef struct J9SysinfoCPUTime {
	int64_t timestamp; /* time in nanoseconds from a fixed but arbitrary point in time */
	int64_t cpuTime; /* cumulative CPU utilization (sum of system and user time in nanoseconds) of all CPUs on the system. */
	int32_t numberOfCpus; /* number of CPUs as reported by the operating system */
} J9SysinfoCPUTime;

/* Key memory categories are copied here for DDR access */
/* Special memory category for memory allocated for unknown categories */
#define OMRMEM_CATEGORY_UNKNOWN 0x80000000
/* Special memory category for memory allocated for the port library itself */
#define OMRMEM_CATEGORY_PORT_LIBRARY 0x80000001
/* Special memory category for *unused* sections of regions allocated for <32bit allocations on 64 bit.
 * The used sections will be accounted for under the categories they are used by. */
#define OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS 0x80000009

#define J9MEM_CATEGORIES_KEEP_ITERATING 0
#define J9MEM_CATEGORIES_STOP_ITERATING 1

/**
* State data for memory category walk
* @see omrmem_walk_categories(
*/
typedef struct OMRMemCategoryWalkState {
	/**
	* Callback function called from omrmemory_walk_categories with memory category data.
	*
	* @param [in] categoryCode               Code identifying memory category
	* @param [in] categoryName               Name of category
	* @param [in] liveBytes                  Bytes outstanding (allocated but not freed) for this category
	* @param [in] liveAllocations            Number of allocations outstanding (not freed) for this category
	* @param [in] isRoot                     True if this node is a root (i.e. does not have a parent)
	* @param [in] parentCategoryCode         Code identifying the parent of this category. Ignore if isRoot==TRUE
	* @param [in] state                      Walk state record.
	* @return J9MEM_CATEGORIES_KEEP_INTERATING or J9MEM_CATEGORIES_STOP_ITERATING
	*/
	uintptr_t (*walkFunction)(uint32_t categoryCode, const char *categoryName, uintptr_t liveBytes, uintptr_t liveAllocations, BOOLEAN isRoot, uint32_t parentCategoryCode, struct OMRMemCategoryWalkState *state);

	void *userData1;
	void *userData2;
} OMRMemCategoryWalkState;

typedef enum J9MemoryState {J9NUMA_PREFERRED, J9NUMA_ALLOWED, J9NUMA_DENIED} J9MemoryState;

typedef struct J9MemoryNodeDetail {
	uintptr_t j9NodeNumber;  /**< The 1-indexed number used outside of Port and Thread to describe this node */
	J9MemoryState memoryPolicy;  /**< Whether the memory on this node is preferred, allowed, or denied use by this process under the current NUMA policy */
	uintptr_t computationalResourcesAvailable;  /**< The number of computational resources on this node which are currently available for use under the current NUMA policy */
} J9MemoryNodeDetail;

/* Stores memory usage statistics snapshot sampled at a time 'timestamp'.
 *
 * @see omrsysinfo_get_memory_info
 *
 * If one of these parameters is not available on a particular platform, this is set to the
 * default OMRPORT_MEMINFO_NOT_AVAILABLE.
 */
typedef struct J9MemoryInfo {
	uint64_t totalPhysical;		/* Total physical memory in the system (in bytes). */
	uint64_t availPhysical;		/* Available physical memory in the system (in bytes). */
	uint64_t totalVirtual;		/* Total virtual memory addressable by the process (in bytes). */
	uint64_t availVirtual;		/* Virtual memory available to the process (in bytes). */
	uint64_t totalSwap;			/* Total swap memory (in bytes). */
	uint64_t availSwap;			/* Total swap memory free (in bytes). */
	uint64_t cached;			/* The physical RAM used as cache memory (in bytes). */
	uint64_t buffered;			/* The physical RAM used for file buffers (in bytes). */
	int64_t timestamp;			/* Sampling timestamp (in microseconds). */
	/* Available physical memory on the host (in bytes) when process is in a cgroup.
	 * When not in a cgroup, this will be identical to 'availPhysical' field above.
	 */
	uint64_t hostAvailPhysical;
	/* The physical RAM used as cache memory (in bytes) when process is in a cgroup.
	 * When not in a cgroup, this will be identical to 'cached' field above.
	 */
	uint64_t hostCached;
	/* The physical RAM used for file buffers (in bytes) when process is in a cgroup.
	 * When not in a cgroup, this will be identical to 'buffered' field above.
	 */
	uint64_t hostBuffered;
	/* The default setting for this kernel parameter of swappiness is 60. Value of 0 disables swap.
	 * Lower swappiness values would keep more pages in memory instead of putting them in swap space.
	 * Higher values will provide more I/O cache and lower values will wait longer to swap out idle application.
	 */
	uint64_t swappiness;
} J9MemoryInfo;

#define OMRPORT_MEMINFO_NOT_AVAILABLE ((uint64_t) -1)

/**
 * Stores usage information on a per-processor basis. These parameters are the ones that generic
 * whereas, operating system specific parameters are not saved here. If one of these parameters is
 * not available on a particular platform, this is set to OMRPORT_PROCINFO_NOT_AVAILABLE.
 */
typedef struct J9ProcessorInfo {
	uint64_t userTime;			/* Time spent in user mode (in microseconds). */
	uint64_t systemTime;		/* Time spent in system mode (in microseconds). */
	uint64_t idleTime;			/* Time spent sitting idle (in microseconds). */
	uint64_t waitTime;			/* Time spent over IO wait (in microseconds). */
	uint64_t busyTime;			/* Time spent over useful work (in microseconds). */
	int32_t proc_id;			/* This processor's id. */
	/* Was current CPU online when last sampled (OMRPORT_PROCINFO_PROC_ONLINE if online,
	 * OMRPORT_PROCINFO_PROC_OFFLINE if not).
	 */
	int32_t online;
} J9ProcessorInfo;

/**
 * Structure collects the usage statistics for each of the processors that is online at the
 * time of sampling.
 *
 * @see omrsysinfo_get_processor_info, omrsysinfo_destroy_processor_info
 *
 * The array holds an entry for each logical processor on the system plus a one for the aggregates.
 * However, the particular record shall hold valid usage samples only if the processor were online,
 * else the individual fields shall be set to OMRPORT_PROCINFO_NOT_AVAILABLE.
 */
typedef struct J9ProcessorInfos {
	int32_t totalProcessorCount;			/* Number of logical processors on the machine. */
	J9ProcessorInfo *procInfoArray;		/* Array of processors, of 'totalProcessorCount + 1'. */
	int64_t timestamp;						/* Sampling timestamp (in microseconds). */
} J9ProcessorInfos;

#define OMRPORT_PROCINFO_NOT_AVAILABLE ((uint64_t) -1)

/* Processor status. */
#define OMRPORT_PROCINFO_PROC_OFFLINE ((int32_t)0)
#define OMRPORT_PROCINFO_PROC_ONLINE ((int32_t)1)

#define NANOSECS_PER_USEC 1000

#define OMRPORT_ENABLE_ENSURE_CAP32 0
#define OMRPORT_DISABLE_ENSURE_CAP32 1

#define FLG_ACQUIRE_LOCK_WAIT		0x00000001
#define FLG_ACQUIRE_LOCK_NOWAIT		0x00000002

/* Lock Status values - the current status of a lock.  */
#define LS_UNINITIALIZED                   0
#define LS_INITIALIZED                     1
#define LS_INITIALIZING                    2
#define LS_LOCKED                          3

/* omrsysinfo_get_number_CPUs_by_type flags */
#define OMRPORT_CPU_PHYSICAL 1
#define OMRPORT_CPU_ONLINE 2
#define OMRPORT_CPU_BOUND 3
#define OMRPORT_CPU_TARGET 4

#define OMRPORT_SL_FOUND  0
#define OMRPORT_SL_NOT_FOUND  1
#define OMRPORT_SL_INVALID  2
#define OMRPORT_SL_UNSUPPORTED  3
#define OMRPORT_SL_UNKNOWN 4					/* Unknown Shared Library related error. */

#define OMRPORT_SLOPEN_DECORATE  1 			/* Note this value must remain 1, in order for legacy callers using TRUE and FALSE to control decoration */
#if !defined(OMRZTPF)
#define OMRPORT_SLOPEN_LAZY  2
#else /* !defined(OMRZTPF) */
#define OMRPORT_SLOPEN_LAZY  0
#endif /* defined(OMRZTPF) */
#define OMRPORT_SLOPEN_NO_LOOKUP_MSG_FOR_NOT_FOUND  4
#define OMRPORT_SLOPEN_OPEN_EXECUTABLE 8     /* Can be ORed without affecting existing flags. */

#if defined(J9ZOS39064)
#define OMRPORT_SLOPEN_ATTEMPT_31BIT_OPEN 16 /* Attempt 31-bit DLL load from 64-bit. */
#define OMRPORT_SL_ZOS_31BIT_TARGET_HIGHTAG 0x3100000000000000l /* High-tag to signify 31-bit target handles and addresses. */
#define OMRPORT_SL_ZOS_31BIT_TARGET_MASK 0xFFFFFFFF /* Mask used to convert 31-bit tagged handles/addresses to proper values. */
#endif /* defined(J9ZOS39064) */

/* Supported on Linux/OSX/Unix/Windows platforms. */
#define OMRPORT_SLOPEN_NO_LOAD 32

#define OMRPORT_ARCH_X86       "x86"
#define OMRPORT_ARCH_PPC       "ppc" 				/* in line with IBM JDK 1.22 and above for AIX and Linux/PPC */
#define OMRPORT_ARCH_PPC64     "ppc64"
#define OMRPORT_ARCH_PPC64LE   "ppc64le"
#define OMRPORT_ARCH_S390      "s390"
#define OMRPORT_ARCH_S390X     "s390x"
#define OMRPORT_ARCH_HAMMER    "amd64"
#define OMRPORT_ARCH_ARM       "arm"
#define OMRPORT_ARCH_AARCH64   "aarch64"
#define OMRPORT_ARCH_RISCV     "riscv"

#define OMRPORT_TTY_IN  0
#define OMRPORT_TTY_OUT  1
#define OMRPORT_TTY_ERR  2

#define OMRPORT_STREAM_IN  stdin
#define OMRPORT_STREAM_OUT stdout
#define OMRPORT_STREAM_ERR stderr

#define OMRPORT_CTLDATA_SIG_FLAGS  "SIG_FLAGS"
#define OMRPORT_CTLDATA_TRACE_START  "TRACE_START"
#define OMRPORT_CTLDATA_TRACE_STOP  "TRACE_STOP"
#define OMRPORT_CTLDATA_VMEM_NUMA_IN_USE  "VMEM_NUMA_IN_USE"
#define OMRPORT_CTLDATA_VMEM_NUMA_ENABLE  "VMEM_NUMA_IN_ENABLE"
#define OMRPORT_CTLDATA_VMEM_NUMA_INTERLEAVE_MEM "VMEM_NUMA_INTERLEAVE"
#define OMRPORT_CTLDATA_SYSLOG_OPEN  "SYSLOG_OPEN"
#define OMRPORT_CTLDATA_SYSLOG_CLOSE  "SYSLOG_CLOSE"
#define OMRPORT_CTLDATA_NOIPT  "NOIPT"
#define OMRPORT_CTLDATA_TIME_CLEAR_TICK_TOCK  "TIME_CLEAR_TICK_TOCK"
#define OMRPORT_CTLDATA_MEM_CATEGORIES_SET  "MEM_CATEGORIES_SET"
#define OMRPORT_CTLDATA_AIX_PROC_ATTR  "AIX_PROC_ATTR"
#define OMRPORT_CTLDATA_ALLOCATE32_COMMIT_SIZE  "ALLOCATE32_COMMIT_SIZE"
#define OMRPORT_CTLDATA_NOSUBALLOC32BITMEM  "NOSUBALLOC32BITMEM"
#define OMRPORT_CTLDATA_VMEM_ADVISE_OS_ONFREE  "VMEM_ADVISE_OS_ONFREE"
#define OMRPORT_CTLDATA_VECTOR_REGS_SUPPORT_ON  "VECTOR_REGS_SUPPORT_ON"
#define OMRPORT_CTLDATA_NLS_DISABLE "NLS_DISABLE"
#define OMRPORT_CTLDATA_VMEM_ADVISE_HUGEPAGE  "VMEM_ADVISE_HUGEPAGE"
#define OMRPORT_CTLDATA_VMEM_PERFORM_FULL_MEMORY_SEARCH  "VMEM_PERFORM_FULL_SEARCH"
#define OMRPORT_CTLDATA_VMEM_HUGE_PAGES_MMAP_ENABLED "VMEM_HUGE_PAGES_MMAP_ENABLED"
#define OMRPORT_CTLDATA_CRIU_SUPPORT_FLAGS "CRIU_SUPPORT_FLAGS"
#define OMRPORT_CTLDATA_MEM_32BIT "MEM_32BIT_FLAGS"

/* OMRPORT_CTLDATA_MEM_32BIT Flags */
#define OMRPORT_MEM_32BIT_FLAGS_TMP_FILE_BACKED_VMEM 0x1

/* CRIU support is enabled, a checkpoint could be taken
 * if current VM is not from a final restoration.
 */
#define OMRPORT_CRIU_SUPPORT_ENABLED  0x1
/* Current VM is from a final restoration,
 * i.e., no more checkpoint is allowed.
 */
#define OMRPORT_CRIU_SUPPORT_FINAL_RESTORE  0x2

#define OMRPORT_FILE_READ_LOCK  1
#define OMRPORT_FILE_WRITE_LOCK  2
#define OMRPORT_FILE_WAIT_FOR_LOCK  4
#define OMRPORT_FILE_NOWAIT_FOR_LOCK  8

#define OMRPORT_MMAP_CAPABILITY_COPYONWRITE  1
#define OMRPORT_MMAP_CAPABILITY_READ  2
#define OMRPORT_MMAP_CAPABILITY_WRITE  4
#define OMRPORT_MMAP_CAPABILITY_UMAP_REQUIRES_SIZE  8
#define OMRPORT_MMAP_CAPABILITY_MSYNC  16
#define OMRPORT_MMAP_CAPABILITY_PROTECT  32
#define OMRPORT_MMAP_FLAG_CREATE_FILE  1
#define OMRPORT_MMAP_FLAG_READ  2
#define OMRPORT_MMAP_FLAG_WRITE  4
#define OMRPORT_MMAP_FLAG_COPYONWRITE  8
#define OMRPORT_MMAP_FLAG_EXECUTABLE  16
#define OMRPORT_MMAP_FLAG_SHARED  32
#define OMRPORT_MMAP_FLAG_PRIVATE  64
#define OMRPORT_MMAP_SYNC_WAIT  0x80
#define OMRPORT_MMAP_SYNC_ASYNC  0x100
#define OMRPORT_MMAP_SYNC_INVALIDATE  0x200
#if defined(J9ZOS39064)
#define OMRPORT_MMAP_FLAG_ZOS_64BIT  0x400
#endif /* defined(J9ZOS39064) */
#if defined(J9ZOS390)
#define OMRPORT_MMAP_FLAG_ZOS_READ_MAPFILE  0x800
#endif /* defined(J9ZOS390) */

/* Signal classification bits. */
#define OMRPORT_SIG_FLAG_MAY_RETURN             ((uint32_t)0x01)
#define OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION ((uint32_t)0x02)
#define OMRPORT_SIG_FLAG_IS_ASYNC               ((uint32_t)0x04)
#define OMRPORT_SIG_FLAG_IS_SYNC                ((uint32_t)0x08)
#define OMRPORT_SIG_FLAG_CONTROL_BITS_MASK      ((uint32_t)0x0F)


#if defined(OMR_OS_WINDOWS) || defined(OMRZTPF)
/* The below macros support the [win32|win64amd|ztpf]/omrsignal.c implementations, which are used on Windows and z/TPF. */

#define OMRPORT_SIG_SMALLEST_SIGNAL_FLAG         0x4
#define OMRPORT_SIG_FLAG_SIGSEGV                 0x4
#define OMRPORT_SIG_FLAG_SIGBUS                  0x8
#define OMRPORT_SIG_FLAG_SIGILL                  0x10
#define OMRPORT_SIG_FLAG_SIGFPE                  0x20
#define OMRPORT_SIG_FLAG_SIGTRAP                 0x40
#define OMRPORT_SIG_FLAG_SIGABEND                0x80
#define OMRPORT_SIG_FLAG_SIGPIPE                 0x100
#define OMRPORT_SIG_FLAG_SIGALRM                 0x200
#define OMRPORT_SIG_FLAG_SIGQUIT                 0x400
#define OMRPORT_SIG_FLAG_SIGABRT                 0x800
#define OMRPORT_SIG_FLAG_SIGTERM                 0x1000
#define OMRPORT_SIG_FLAG_SIGRECONFIG             0x2000
#define OMRPORT_SIG_FLAG_SIGINT                  0x4000
#define OMRPORT_SIG_FLAG_SIGXFSZ                 0x8000
#define OMRPORT_SIG_FLAG_SIGCHLD                 0x10000
#define OMRPORT_SIG_FLAG_SIGTSTP                 0x20000
#define OMRPORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO      (OMRPORT_SIG_FLAG_SIGFPE | 0x40000)
#define OMRPORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO  (OMRPORT_SIG_FLAG_SIGFPE | 0x80000)
#define OMRPORT_SIG_FLAG_SIGFPE_INT_OVERFLOW     (OMRPORT_SIG_FLAG_SIGFPE | 0x100000)
#define OMRPORT_SIG_FLAG_SIGIO                   0x200000
#define OMRPORT_SIG_FLAG_SIGHUP                  0x400000
#define OMRPORT_SIG_FLAG_SIGCONT                 0x800000
#define OMRPORT_SIG_FLAG_SIGWINCH                0x1000000
#define OMRPORT_SIG_FLAG_SIGUSR1                 0x2000000
#define OMRPORT_SIG_FLAG_SIGUSR2                 0x4000000
#define OMRPORT_SIG_FLAG_SIGURG                  0x8000000
#define OMRPORT_SIG_FLAG_SIGXCPU                 0x10000000
#define OMRPORT_SIG_FLAG_SIGVTALRM               0x20000000
#define OMRPORT_SIG_FLAG_SIGPROF                 0x40000000
#define OMRPORT_SIG_FLAG_SIGSYS                  0x80000000

#define OMRPORT_SIG_FLAG_SIGALLASYNC \
	( OMRPORT_SIG_FLAG_SIGQUIT  | OMRPORT_SIG_FLAG_SIGABRT   | OMRPORT_SIG_FLAG_SIGTERM | OMRPORT_SIG_FLAG_SIGRECONFIG \
	| OMRPORT_SIG_FLAG_SIGXFSZ  | OMRPORT_SIG_FLAG_SIGINT    | OMRPORT_SIG_FLAG_SIGHUP  | OMRPORT_SIG_FLAG_SIGCONT \
	| OMRPORT_SIG_FLAG_SIGWINCH | OMRPORT_SIG_FLAG_SIGPIPE   | OMRPORT_SIG_FLAG_SIGALRM | OMRPORT_SIG_FLAG_SIGCHLD \
	| OMRPORT_SIG_FLAG_SIGTSTP  | OMRPORT_SIG_FLAG_SIGUSR1   | OMRPORT_SIG_FLAG_SIGUSR2 | OMRPORT_SIG_FLAG_SIGURG \
	| OMRPORT_SIG_FLAG_SIGXCPU  | OMRPORT_SIG_FLAG_SIGVTALRM | OMRPORT_SIG_FLAG_SIGPROF | OMRPORT_SIG_FLAG_SIGIO \
	| OMRPORT_SIG_FLAG_SIGSYS )
#else /* defined(OMR_OS_WINDOWS) || defined(OMRZTPF) */
/* The below macros support the unix/omrsignal.c implementation, used everywhere but windows and z/TPF. */

/* All signal codes include exactly one of OMRPORT_SIG_FLAG_IS_SYNC or OMRPORT_SIG_FLAG_IS_ASYNC
 * plus a multiple of this.
 */
#define OMRPORT_SIG_SMALLEST_SIGNAL_FLAG        ((uint32_t)0x10)

/* The API can accommodate up to 19 more synchronous signals and up to 7 more asynchronous signals:
 *
 * #define OMRPORT_SIG_FLAG_MAX_SYNC   ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 27) | OMRPORT_SIG_FLAG_IS_SYNC)
 * #define OMRPORT_SIG_FLAG_MAX_ASYNC  ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 27) | OMRPORT_SIG_FLAG_IS_ASYNC)
 */

/* Synchronous signals. */
#define OMRPORT_SIG_FLAG_SIGSEGV                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  0) | OMRPORT_SIG_FLAG_IS_SYNC)
#define OMRPORT_SIG_FLAG_SIGBUS                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  1) | OMRPORT_SIG_FLAG_IS_SYNC)
#define OMRPORT_SIG_FLAG_SIGILL                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  2) | OMRPORT_SIG_FLAG_IS_SYNC)
#define OMRPORT_SIG_FLAG_SIGFPE                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  3) | OMRPORT_SIG_FLAG_IS_SYNC)
#define OMRPORT_SIG_FLAG_SIGTRAP                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  4) | OMRPORT_SIG_FLAG_IS_SYNC)
#define OMRPORT_SIG_FLAG_SIGABEND               ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  5) | OMRPORT_SIG_FLAG_IS_SYNC)

#define OMRPORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO     ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  6) | OMRPORT_SIG_FLAG_SIGFPE)
#define OMRPORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  7) | OMRPORT_SIG_FLAG_SIGFPE)
#define OMRPORT_SIG_FLAG_SIGFPE_INT_OVERFLOW    ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  8) | OMRPORT_SIG_FLAG_SIGFPE)

/* Asynchronous signals. */
#define OMRPORT_SIG_FLAG_SIGALRM                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  0) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGQUIT                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  1) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGABRT                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  2) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGTERM                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  3) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGRECONFIG            ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  4) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGINT                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  5) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGXFSZ                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  6) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGCHLD                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  7) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGTSTP                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  8) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGIO                  ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG <<  9) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGHUP                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 10) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGCONT                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 11) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGWINCH               ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 12) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGUSR1                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 13) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGUSR2                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 14) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGURG                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 15) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGXCPU                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 16) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGVTALRM              ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 17) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGPROF                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 18) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGPIPE                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 19) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGSYS                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 20) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGTTIN                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 21) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGTTOU                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 22) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGINFO                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 23) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGIOT                 ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 24) | OMRPORT_SIG_FLAG_IS_ASYNC)
#define OMRPORT_SIG_FLAG_SIGPOLL                ((OMRPORT_SIG_SMALLEST_SIGNAL_FLAG << 25) | OMRPORT_SIG_FLAG_IS_ASYNC)

#define OMRPORT_SIG_FLAG_SIGALLASYNC \
	( OMRPORT_SIG_FLAG_SIGALRM     | OMRPORT_SIG_FLAG_SIGQUIT   | OMRPORT_SIG_FLAG_SIGABRT | OMRPORT_SIG_FLAG_SIGTERM \
	| OMRPORT_SIG_FLAG_SIGRECONFIG | OMRPORT_SIG_FLAG_SIGINT    | OMRPORT_SIG_FLAG_SIGXFSZ | OMRPORT_SIG_FLAG_SIGCHLD \
	| OMRPORT_SIG_FLAG_SIGTSTP     | OMRPORT_SIG_FLAG_SIGIO     | OMRPORT_SIG_FLAG_SIGHUP  | OMRPORT_SIG_FLAG_SIGCONT \
	| OMRPORT_SIG_FLAG_SIGWINCH    | OMRPORT_SIG_FLAG_SIGUSR1   | OMRPORT_SIG_FLAG_SIGUSR2 | OMRPORT_SIG_FLAG_SIGURG  \
	| OMRPORT_SIG_FLAG_SIGXCPU     | OMRPORT_SIG_FLAG_SIGVTALRM | OMRPORT_SIG_FLAG_SIGPROF | OMRPORT_SIG_FLAG_SIGPIPE \
	| OMRPORT_SIG_FLAG_SIGSYS      | OMRPORT_SIG_FLAG_SIGTTIN   | OMRPORT_SIG_FLAG_SIGTTOU | OMRPORT_SIG_FLAG_SIGINFO \
	| OMRPORT_SIG_FLAG_SIGIOT      | OMRPORT_SIG_FLAG_SIGPOLL)
#endif /* defined(OMR_OS_WINDOWS) || defined(OMRZTPF) */

#if defined(J9ZOS390)
#define OMRPORT_SIG_FLAG_SIGALLSYNC \
	( OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_SIGBUS | OMRPORT_SIG_FLAG_SIGILL | OMRPORT_SIG_FLAG_SIGFPE \
	| OMRPORT_SIG_FLAG_SIGTRAP | OMRPORT_SIG_FLAG_SIGABEND )
#else /* defined(J9ZOS390) */
#define OMRPORT_SIG_FLAG_SIGALLSYNC \
	( OMRPORT_SIG_FLAG_SIGSEGV | OMRPORT_SIG_FLAG_SIGBUS | OMRPORT_SIG_FLAG_SIGILL | OMRPORT_SIG_FLAG_SIGFPE \
	| OMRPORT_SIG_FLAG_SIGTRAP )
#endif /* defined(J9ZOS390) */

#define OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH  0
#define OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION  1
#define OMRPORT_SIG_EXCEPTION_RETURN  2
#define OMRPORT_SIG_EXCEPTION_COOPERATIVE_SHUTDOWN  3

#define OMRPORT_SIG_NO_EXCEPTION 0
#define OMRPORT_SIG_EXCEPTION_OCCURRED  1
#define OMRPORT_SIG_ERROR  -1

#define OMRPORT_SIG_SIGNAL  0
#define OMRPORT_SIG_GPR  1
#define OMRPORT_SIG_OTHER  2
#define OMRPORT_SIG_CONTROL  3
#define OMRPORT_SIG_FPR  4
#define OMRPORT_SIG_MODULE  5
#define OMRPORT_SIG_VR	6
#define OMRPORT_SIG_NUM_CATEGORIES  (OMRPORT_SIG_VR + 1)

#define OMRPORT_SIG_SIGNAL_TYPE  -1
#define OMRPORT_SIG_SIGNAL_CODE  -2
#define OMRPORT_SIG_SIGNAL_ERROR_VALUE  -3
#define OMRPORT_SIG_CONTROL_PC  -4
#define OMRPORT_SIG_CONTROL_SP  -5
#define OMRPORT_SIG_CONTROL_BP  -6
#define OMRPORT_SIG_GPR_X86_EDI  -7
#define OMRPORT_SIG_GPR_X86_ESI  -8
#define OMRPORT_SIG_GPR_X86_EAX  -9
#define OMRPORT_SIG_GPR_X86_EBX  -10
#define OMRPORT_SIG_GPR_X86_ECX  -11
#define OMRPORT_SIG_GPR_X86_EDX  -12
#define OMRPORT_SIG_MODULE_NAME  -13
#define OMRPORT_SIG_SIGNAL_ADDRESS  -14
#define OMRPORT_SIG_SIGNAL_HANDLER  -15
#define OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE  -16
#define OMRPORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS  -17
#define OMRPORT_SIG_GPR_AMD64_RDI  -18
#define OMRPORT_SIG_GPR_AMD64_RSI  -19
#define OMRPORT_SIG_GPR_AMD64_RAX  -20
#define OMRPORT_SIG_GPR_AMD64_RBX  -21
#define OMRPORT_SIG_GPR_AMD64_RCX  -22
#define OMRPORT_SIG_GPR_AMD64_RDX  -23
#define OMRPORT_SIG_GPR_AMD64_R8  -24
#define OMRPORT_SIG_GPR_AMD64_R9  -25
#define OMRPORT_SIG_GPR_AMD64_R10  -26
#define OMRPORT_SIG_GPR_AMD64_R11  -27
#define OMRPORT_SIG_GPR_AMD64_R12  -28
#define OMRPORT_SIG_GPR_AMD64_R13  -29
#define OMRPORT_SIG_GPR_AMD64_R14  -30
#define OMRPORT_SIG_GPR_AMD64_R15  -31
#define OMRPORT_SIG_CONTROL_POWERPC_LR  -32
#define OMRPORT_SIG_CONTROL_POWERPC_MSR  -33
#define OMRPORT_SIG_CONTROL_POWERPC_CTR  -34
#define OMRPORT_SIG_CONTROL_POWERPC_CR  -35
#define OMRPORT_SIG_CONTROL_POWERPC_FPSCR  -36
#define OMRPORT_SIG_CONTROL_POWERPC_XER  -37
#define OMRPORT_SIG_CONTROL_POWERPC_MQ  -38
#define OMRPORT_SIG_CONTROL_POWERPC_DAR  -39
#define OMRPORT_SIG_CONTROL_POWERPC_DSIR  -40
#define OMRPORT_SIG_CONTROL_S390_FPC  -41
#define OMRPORT_SIG_CONTROL_S390_GPR7  -42
#define OMRPORT_SIG_CONTROL_X86_EFLAGS  -43
#define OMRPORT_SIG_SIGNAL_ZOS_CONDITION_INFORMATION_BLOCK  -44
#define OMRPORT_SIG_SIGNAL_ZOS_CONDITION_FACILITY_ID  -45
#define OMRPORT_SIG_SIGNAL_ZOS_CONDITION_MESSAGE_NUMBER  -46
#define OMRPORT_SIG_SIGNAL_ZOS_CONDITION_FEEDBACK_TOKEN  -47
#define OMRPORT_SIG_SIGNAL_ZOS_CONDITION_SEVERITY  -48
#define OMRPORT_SIG_MODULE_FUNCTION_NAME  -49
#define OMRPORT_SIG_WINDOWS_DEFER_TRY_EXCEPT_HANDLER  -50
#define OMRPORT_SIG_CONTROL_S390_BEA  -51
#define OMRPORT_SIG_GPR_ARM_R0   -52
#define OMRPORT_SIG_GPR_ARM_R1   -53
#define OMRPORT_SIG_GPR_ARM_R2   -54
#define OMRPORT_SIG_GPR_ARM_R3   -55
#define OMRPORT_SIG_GPR_ARM_R4   -56
#define OMRPORT_SIG_GPR_ARM_R5   -57
#define OMRPORT_SIG_GPR_ARM_R6   -58
#define OMRPORT_SIG_GPR_ARM_R7   -59
#define OMRPORT_SIG_GPR_ARM_R8   -60
#define OMRPORT_SIG_GPR_ARM_R9   -61
#define OMRPORT_SIG_GPR_ARM_R10  -62

#define OMRPORT_SIG_VALUE_UNDEFINED  1
#define OMRPORT_SIG_VALUE_STRING  2
#define OMRPORT_SIG_VALUE_ADDRESS  3
#define OMRPORT_SIG_VALUE_32  4
#define OMRPORT_SIG_VALUE_64  5
#define OMRPORT_SIG_VALUE_FLOAT_64  6
#define OMRPORT_SIG_VALUE_16  7
#define OMRPORT_SIG_VALUE_128 8

#define OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN  1
#define OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS  2
#define OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS  4
#define OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR  8
#define OMRPORT_SIG_OPTIONS_COOPERATIVE_SHUTDOWN  16
#define OMRPORT_SIG_OPTIONS_SIGXFSZ  32

#define OMRPORT_PAGE_PROTECT_NOT_SUPPORTED  -2
#define OMRPORT_PAGE_PROTECT_NONE  1
#define OMRPORT_PAGE_PROTECT_READ  2
#define OMRPORT_PAGE_PROTECT_WRITE  4
#define OMRPORT_PAGE_PROTECT_EXEC  8

#define OMRPORT_VMEM_RESERVE_USED_INVALID  0
#define OMRPORT_VMEM_RESERVE_USED_J9MEM_ALLOCATE_MEMORY  1
#define OMRPORT_VMEM_RESERVE_USED_MMAP  2
#define OMRPORT_VMEM_RESERVE_USED_SHM  3
#define OMRPORT_VMEM_RESERVE_USED_MALLOC31  4
#define OMRPORT_VMEM_RESERVE_USED_J9ALLOCATE_LARGE_FIXED_PAGES_ABOVE_BAR  5
#define OMRPORT_VMEM_RESERVE_USED_J9ALLOCATE_LARGE_PAGEABLE_PAGES_ABOVE_BAR  6
#define OMRPORT_VMEM_RESERVE_USED_J9ALLOCATE_LARGE_PAGES_BELOW_BAR  7
#define OMRPORT_VMEM_RESERVE_USED_J9ALLOCATE_4K_PAGES_IN_2TO32G_AREA  8
#define OMRPORT_VMEM_RESERVE_USED_J9ALLOCATE_4K_PAGES_ABOVE_BAR 9
#define OMRPORT_VMEM_RESERVE_USED_J9ALLOCATE_4K_PAGES_BELOW_BAR 10
#define OMRPORT_VMEM_RESERVE_USED_MOSERVICES 11
#define OMRPORT_VMEM_RESERVE_USED_MMAP_SHM 12
#define OMRPORT_VMEM_RESERVE_USED_MMAP_RESTORE_MMAP 13

#define OMRPORT_ENSURE_CAPACITY_FAILED  0
#define OMRPORT_ENSURE_CAPACITY_SUCCESS  1
#define OMRPORT_ENSURE_CAPACITY_NOT_REQUIRED  2

/* omrstr_convert encodings. */
/* character set currently in effect */
#define J9STR_CODE_PLATFORM_RAW 1
/* modified UTF-8 */
#define J9STR_CODE_MUTF8 2
/* UTF-16 */
#define J9STR_CODE_WIDE 3
/* EBCDIC */
#define J9STR_CODE_EBCDIC 4
/* orthodox UTF-8 */
#define J9STR_CODE_UTF8 5
/* orthodox UTF-8 */
#define J9STR_CODE_LATIN1 6
/* Windows default ANSI code page */
#define J9STR_CODE_WINDEFAULTACP 7
/* Windows current thread ANSI code page */
#define J9STR_CODE_WINTHREADACP 8

/* flags for omrstr_ftime_ex */
#define OMRSTR_FTIME_FLAG_LOCAL 0 /* result should be produced for local timezone */
#define OMRSTR_FTIME_FLAG_UTC   1 /* result should be produced for UTC timezone */

#if defined(J9ZOS390)
/*
 * OMR on z/OS translates the output of certain system calls such as getenv to ASCII using functions in atoe.c; see stdlib.h for a list.
 * Use J9STR_CODE_PLATFORM_OMR_INTERNAL to when processing the output of these calls.  Otherwise, use J9STR_CODE_PLATFORM_RAW.
 */
#define J9STR_CODE_PLATFORM_OMR_INTERNAL J9STR_CODE_LATIN1
#elif defined(OMR_OS_WINDOWS)
/*
 * Most system calls on Windows use the "wide" versions which return UTF-16, which OMR then converts to UTF-8.
 */
#define J9STR_CODE_PLATFORM_OMR_INTERNAL J9STR_CODE_UTF8
#else /* defined(OMR_OS_WINDOWS) */
/* on other platforms the internal encoding is the actual operating system encoding */
#define J9STR_CODE_PLATFORM_OMR_INTERNAL J9STR_CODE_PLATFORM_RAW
#endif /* defined(J9ZOS390) */

#define UNICODE_REPLACEMENT_CHARACTER 0xFFFD
#define MAX_STRING_TERMINATOR_LENGTH 4
/* worst case terminating sequence: wchar or UTF-32 */

/* Constants from J9NLSConstants */
#define J9NLS_BEGIN_MULTI_LINE 0x100
#define J9NLS_CONFIG 0x800
#define J9NLS_DO_NOT_APPEND_NEWLINE 0x10
#define J9NLS_DO_NOT_PRINT_MESSAGE_TAG 0x1
#define J9NLS_END_MULTI_LINE 0x400
#define J9NLS_ERROR 0x2
#define J9NLS_INFO 0x8
#define J9NLS_MULTI_LINE 0x200
#define J9NLS_STDERR 0x40
#define J9NLS_STDOUT 0x20
#define J9NLS_VITAL 0x1000
#define J9NLS_WARNING 0x4

typedef struct J9PortVmemIdentifier {
	void *address;
	void *handle;
	uintptr_t size;
	uintptr_t pageSize;
	uintptr_t pageFlags;
	uintptr_t mode;
	uintptr_t allocator;
	int fd;
	OMRMemCategory *category;
} J9PortVmemIdentifier;

typedef struct J9MmapHandle {
	void *pointer;
	uintptr_t size;
	void *allocPointer;
	OMRMemCategory *category;
} J9MmapHandle;

#if !defined(OMR_OS_WINDOWS)
#if defined(OSX)
#define _XOPEN_SOURCE
#endif /* defined(OSX) */
#include <ucontext.h>
#if defined(OSX)
#undef _XOPEN_SOURCE
#endif /* OSX */
#endif /* !OMR_OS_WINDOWS */

#if defined(J9ZOS390)
struct __mcontext;
#endif /* J9ZOS390 */

typedef struct J9PlatformStackFrame {
	uintptr_t stack_pointer;
	uintptr_t base_pointer;
	uintptr_t instruction_pointer;
	uintptr_t register1;
	uintptr_t register2;
	uintptr_t register3;
	char *symbol;
	struct J9PlatformStackFrame *parent_frame;
} J9PlatformStackFrame;

typedef struct J9PlatformThread {
	uintptr_t thread_id;
	uintptr_t process_id;
	uintptr_t stack_base;
	uintptr_t stack_end;
	uintptr_t priority;
#if defined(OMR_OS_WINDOWS)
	void *context;
#elif defined(J9ZOS390)
	/* This should really be 'struct __mcontext*' however DDR cannot parse the zos system header
	 * that we carry as part of portlib. DDR runs on a linux machine and the defines required by the edcwccwi.h
	 * header are set by both VAC and other zos system headers that DDR has no access to.
	 */
	void *context;
#else
	ucontext_t *context;
#endif
	struct J9PlatformStackFrame *callstack;
#if defined(OMR_OS_WINDOWS)
	void *sigmask;
#else /* OMR_OS_WINDOWS */
	sigset_t *sigmask;
#endif /* OMR_OS_WINDOWS */
	intptr_t error;
	void *dsa;
	uintptr_t dsa_format;
	void *caa;
} J9PlatformThread;

typedef struct J9ThreadWalkState {
	struct OMRPortLibrary *portLibrary;
	struct J9PlatformThread *current_thread;
	int64_t deadline1;
	int64_t deadline2;
	struct J9Heap *heap;
	void *platform_data;
	intptr_t error;
	uintptr_t error_detail;
	const char *error_string;
	uint32_t options;
} J9ThreadWalkState;

/*
 * Possible options in J9ThreadWalkState.
 */

/*
 * Don't resolve symbols in stack frames in introspect_threads_XXX();
 * caller will use introspect_backtrace_symbols_ex() as necessary.
 */
#define OMR_INTROSPECT_NO_SYMBOLS 1

/*
 * Possible options in calls to introspect_backtrace_symbols_ex().
 */

/*
 * Do only basic, low cost symbol resolution. Currently, this is only
 * relevant on Linux where full symbol resolution can be costly.
 */
#define OMR_BACKTRACE_SYMBOLS_BASIC 1

typedef struct J9PortSysInfoLoadData {
	double oneMinuteAverage;
	double fiveMinuteAverage;
	double fifteenMinuteAverage;
} J9PortSysInfoLoadData;

typedef struct J9StringTokens {
	void *table;
} J9StringTokens;

/* Holds OS features used with omrsysinfo_get_os_description and omrsysinfo_os_has_feature */
#define OMRPORT_SYSINFO_OS_FEATURES_SIZE 1
typedef struct OMROSDesc {
	uint32_t features[OMRPORT_SYSINFO_OS_FEATURES_SIZE];
} OMROSDesc;

/* zOS features */
#define OMRPORT_ZOS_FEATURE_RMODE64 31 /* RMODE64. */

typedef struct OMROSKernelInfo {
	uint32_t kernelVersion;
	uint32_t majorRevision;
	uint32_t minorRevision;
} OMROSKernelInfo;

/* bitwise flags indicating cgroup subsystems supported by portlibrary */
#define OMR_CGROUP_SUBSYSTEM_CPU ((uint64_t)0x1)
#define OMR_CGROUP_SUBSYSTEM_MEMORY ((uint64_t)0x2)
#define OMR_CGROUP_SUBSYSTEM_CPUSET ((uint64_t)0x4)
#define OMR_CGROUP_SUBSYSTEM_ALL (OMR_CGROUP_SUBSYSTEM_CPU | OMR_CGROUP_SUBSYSTEM_MEMORY | OMR_CGROUP_SUBSYSTEM_CPUSET)


/* List of all processors that are currently supported by OMR's processor detection */

typedef enum OMRProcessorArchitecture {

	OMR_PROCESSOR_UNDEFINED,

	// 390 Processors
	OMR_PROCESSOR_S390_FIRST,
	OMR_PROCESSOR_S390_UNKNOWN = OMR_PROCESSOR_S390_FIRST,
	OMR_PROCESSOR_S390_Z900,
 	OMR_PROCESSOR_S390_Z990,
 	OMR_PROCESSOR_S390_Z9,
	OMR_PROCESSOR_S390_Z10,
	OMR_PROCESSOR_S390_Z196,
	OMR_PROCESSOR_S390_ZEC12,
	OMR_PROCESSOR_S390_Z13,
	OMR_PROCESSOR_S390_Z14,
	OMR_PROCESSOR_S390_Z15,
	OMR_PROCESSOR_S390_Z16,
	OMR_PROCESSOR_S390_ZNEXT,
	OMR_PROCESSOR_S390_LAST = OMR_PROCESSOR_S390_ZNEXT,

	// ARM Processors
	OMR_PROCESSOR_ARM_FIRST,
	OMR_PROCESSOR_ARM_UNKNOWN = OMR_PROCESSOR_ARM_FIRST,
	OMR_PROCESSOR_ARM_V6,
	OMR_PROCESSOR_ARM_V7,
	OMR_PROCESSOR_ARM_LAST = OMR_PROCESSOR_ARM_V7,

	// ARM64 / AARCH64 Processors
	OMR_PROCESSOR_ARM64_FIRST,
	OMR_PROCESSOR_ARM64_UNKNOWN = OMR_PROCESSOR_ARM64_FIRST,
	OMR_PROCESSOR_ARM64_V8_A,
	OMR_PROCESSOR_ARM64_LAST = OMR_PROCESSOR_ARM64_V8_A,

	// PPC Processors
	OMR_PROCESSOR_PPC_FIRST,
	OMR_PROCESSOR_PPC_UNKNOWN = OMR_PROCESSOR_PPC_FIRST,
	OMR_PROCESSOR_PPC_RIOS1,
	OMR_PROCESSOR_PPC_PWR403,
	OMR_PROCESSOR_PPC_PWR405,
	OMR_PROCESSOR_PPC_PWR440,
	OMR_PROCESSOR_PPC_PWR601,
	OMR_PROCESSOR_PPC_PWR602,
	OMR_PROCESSOR_PPC_PWR603,
	OMR_PROCESSOR_PPC_82XX,
	OMR_PROCESSOR_PPC_7XX,
	OMR_PROCESSOR_PPC_PWR604,
	// The following processors support SQRT in hardware
	OMR_PROCESSOR_PPC_HW_SQRT_FIRST,
	OMR_PROCESSOR_PPC_RIOS2 = OMR_PROCESSOR_PPC_HW_SQRT_FIRST,
	OMR_PROCESSOR_PPC_PWR2S,
	// The following processors are 64-bit implementations
	OMR_PROCESSOR_PPC_64BIT_FIRST,
	OMR_PROCESSOR_PPC_PWR620 = OMR_PROCESSOR_PPC_64BIT_FIRST,
	OMR_PROCESSOR_PPC_PWR630,
	OMR_PROCESSOR_PPC_NSTAR,
	OMR_PROCESSOR_PPC_PULSAR,
	// The following processors support the PowerPC AS architecture
	// PPC AS includes the new branch hint 'a' and 't' bits
	OMR_PROCESSOR_PPC_AS_FIRST,
	OMR_PROCESSOR_PPC_GP = OMR_PROCESSOR_PPC_AS_FIRST,
	OMR_PROCESSOR_PPC_GR,
	// The following processors support VMX
	OMR_PROCESSOR_PPC_VMX_FIRST,
	OMR_PROCESSOR_PPC_GPUL = OMR_PROCESSOR_PPC_VMX_FIRST,
	OMR_PROCESSOR_PPC_HW_ROUND_FIRST,
	OMR_PROCESSOR_PPC_HW_COPY_SIGN_FIRST = OMR_PROCESSOR_PPC_HW_ROUND_FIRST,
	OMR_PROCESSOR_PPC_P6 = OMR_PROCESSOR_PPC_HW_COPY_SIGN_FIRST,
	OMR_PROCESSOR_PPC_ATLAS,
	OMR_PROCESSOR_PPC_BALANCED,
	OMR_PROCESSOR_PPC_CELLPX,
	// The following processors support VSX
	OMR_PROCESSOR_PPC_VSX_FIRST,
	OMR_PROCESSOR_PPC_P7 = OMR_PROCESSOR_PPC_VSX_FIRST,
	OMR_PROCESSOR_PPC_P8,
	OMR_PROCESSOR_PPC_P9,
	OMR_PROCESSOR_PPC_P10,
	OMR_PROCESSOR_PPC_LAST = OMR_PROCESSOR_PPC_P10,

	// X86 Processors
	//
	// These processors should be ordered by ascending microarchitectural release
	OMR_PROCESSOR_X86_FIRST,
	OMR_PROCESSOR_X86_UNKNOWN = OMR_PROCESSOR_X86_FIRST,
	OMR_PROCESSOR_X86_INTEL_FIRST,
	OMR_PROCESSOR_X86_INTEL_PENTIUM = OMR_PROCESSOR_X86_INTEL_FIRST,
	OMR_PROCESSOR_X86_INTEL_P6,
	OMR_PROCESSOR_X86_INTEL_PENTIUM4,
	OMR_PROCESSOR_X86_INTEL_CORE2,
	OMR_PROCESSOR_X86_INTEL_TULSA,
	OMR_PROCESSOR_X86_INTEL_NEHALEM,
	OMR_PROCESSOR_X86_INTEL_WESTMERE,
	OMR_PROCESSOR_X86_INTEL_SANDYBRIDGE,
	OMR_PROCESSOR_X86_INTEL_IVYBRIDGE,
	OMR_PROCESSOR_X86_INTEL_HASWELL,
	OMR_PROCESSOR_X86_INTEL_BROADWELL,
	OMR_PROCESSOR_X86_INTEL_SKYLAKE,
	OMR_PROCESSOR_X86_INTEL_CASCADELAKE,
	OMR_PROCESSOR_X86_INTEL_ICELAKE,
	OMR_PROCESSOR_X86_INTEL_SAPPHIRERAPIDS,
	OMR_PROCESSOR_X86_INTEL_EMERALDRAPIDS,
	OMR_PROCESSOR_X86_INTEL_LAST = OMR_PROCESSOR_X86_INTEL_EMERALDRAPIDS,
	OMR_PROCESSOR_X86_AMD_FIRST,
	OMR_PROCESSOR_X86_AMD_K5 = OMR_PROCESSOR_X86_AMD_FIRST,
	OMR_PROCESSOR_X86_AMD_K6,
	OMR_PROCESSOR_X86_AMD_ATHLONDURON,
	OMR_PROCESSOR_X86_AMD_OPTERON,
	OMR_PROCESSOR_X86_AMD_FAMILY15H,
	OMR_PROCESSOR_X86_AMD_LAST = OMR_PROCESSOR_X86_AMD_FAMILY15H,

	/**
	 * Keep the original macro definitions until all known downstream projects
	 * complete the switch to the new names
	 */
	// <ORIGINAL MACROS>
	OMR_PROCESSOR_X86_INTELPENTIUM = OMR_PROCESSOR_X86_INTEL_PENTIUM,
	OMR_PROCESSOR_X86_INTELP6 = OMR_PROCESSOR_X86_INTEL_P6,
	OMR_PROCESSOR_X86_INTELPENTIUM4 = OMR_PROCESSOR_X86_INTEL_PENTIUM4,
	OMR_PROCESSOR_X86_INTELCORE2 = OMR_PROCESSOR_X86_INTEL_CORE2,
	OMR_PROCESSOR_X86_INTELTULSA = OMR_PROCESSOR_X86_INTEL_TULSA,
	OMR_PROCESSOR_X86_INTELNEHALEM = OMR_PROCESSOR_X86_INTEL_NEHALEM,
	OMR_PROCESSOR_X86_INTELWESTMERE = OMR_PROCESSOR_X86_INTEL_WESTMERE,
	OMR_PROCESSOR_X86_INTELSANDYBRIDGE = OMR_PROCESSOR_X86_INTEL_SANDYBRIDGE,
	OMR_PROCESSOR_X86_INTELIVYBRIDGE = OMR_PROCESSOR_X86_INTEL_IVYBRIDGE,
	OMR_PROCESSOR_X86_INTELHASWELL = OMR_PROCESSOR_X86_INTEL_HASWELL,
	OMR_PROCESSOR_X86_INTELBROADWELL = OMR_PROCESSOR_X86_INTEL_BROADWELL,
	OMR_PROCESSOR_X86_INTELSKYLAKE = OMR_PROCESSOR_X86_INTEL_SKYLAKE,
	OMR_PROCESSOR_X86_AMDATHLONDURON = OMR_PROCESSOR_X86_AMD_ATHLONDURON,
	OMR_PROCESSOR_X86_AMDOPTERON = OMR_PROCESSOR_X86_AMD_OPTERON,
	OMR_PROCESSOR_X86_AMDFAMILY15H = OMR_PROCESSOR_X86_AMD_FAMILY15H,
	// </ORIGINAL MACROS>

	OMR_PROCESSOR_X86_LAST = OMR_PROCESSOR_X86_AMD_FAMILY15H,

	OMR_PROCESSOR_RISCV32_UNKNOWN,
	OMR_PROCESSOR_RISCV64_UNKNOWN,

	OMR_PROCESSOR_DUMMY = 0x40000000 /* force wide enums */

} OMRProcessorArchitecture;

/* Holds processor type and features used with omrsysinfo_get_processor_description,
 * omrsysinfo_processor_has_feature and omrsysinfo_processor_set_feature
 */
#define OMRPORT_SYSINFO_FEATURES_SIZE 7
typedef struct OMRProcessorDesc {
	OMRProcessorArchitecture processor;
	OMRProcessorArchitecture physicalProcessor;
	uint32_t features[OMRPORT_SYSINFO_FEATURES_SIZE];
} OMRProcessorDesc;

/* PowerPC features
 * Auxiliary Vector Hardware Capability (AT_HWCAP) features for PowerPC.
 */
#define OMR_FEATURE_PPC_32                    31 /* 32-bit mode.  */
#define OMR_FEATURE_PPC_64                    30 /* 64-bit mode.  */
#define OMR_FEATURE_PPC_601_INSTR             29 /* 601 chip, Old POWER ISA.  */
#define OMR_FEATURE_PPC_HAS_ALTIVEC           28 /* SIMD/Vector Unit.  */
#define OMR_FEATURE_PPC_HAS_FPU               27 /* Floating Point Unit.  */
#define OMR_FEATURE_PPC_HAS_MMU               26 /* Memory Management Unit.  */
#define OMR_FEATURE_PPC_HAS_4xxMAC            25 /* 4xx Multiply Accumulator.  */
#define OMR_FEATURE_PPC_UNIFIED_CACHE         24 /* Unified I/D cache.  */
#define OMR_FEATURE_PPC_HAS_SPE               23 /* Signal Processing ext.  */
#define OMR_FEATURE_PPC_HAS_EFP_SINGLE        22 /* SPE Float.  */
#define OMR_FEATURE_PPC_HAS_EFP_DOUBLE        21 /* SPE Double.  */
#define OMR_FEATURE_PPC_NO_TB                 20 /* 601/403gx have no timebase.  */
#define OMR_FEATURE_PPC_POWER4                19 /* POWER4 ISA 2.01.  */
#define OMR_FEATURE_PPC_POWER5                18 /* POWER5 ISA 2.02.  */
#define OMR_FEATURE_PPC_POWER5_PLUS           17 /* POWER5+ ISA 2.03.  */
#define OMR_FEATURE_PPC_CELL_BE               16 /* CELL Broadband Engine */
#define OMR_FEATURE_PPC_BOOKE                 15 /* ISA Embedded Category.  */
#define OMR_FEATURE_PPC_SMT                   14 /* Simultaneous Multi-Threading.  */
#define OMR_FEATURE_PPC_ICACHE_SNOOP          13
#define OMR_FEATURE_PPC_ARCH_2_05             12 /* ISA 2.05.  */
#define OMR_FEATURE_PPC_PA6T                  11 /* PA Semi 6T Core.  */
#define OMR_FEATURE_PPC_HAS_DFP               10 /* Decimal FP Unit.  */
#define OMR_FEATURE_PPC_POWER6_EXT             9 /* P6 + mffgpr/mftgpr.  */
#define OMR_FEATURE_PPC_ARCH_2_06              8 /* ISA 2.06.  */
#define OMR_FEATURE_PPC_HAS_VSX                7 /* P7 Vector Scalar Extension.  */
#define OMR_FEATURE_PPC_PSERIES_PERFMON_COMPAT 6 /* Has ISA >= 2.05 PMU basic subset support.  */
#define OMR_FEATURE_PPC_TRUE_LE                1 /* Processor in true Little Endian mode.  */
#define OMR_FEATURE_PPC_LE                     0 /* Processor emulates Little Endian Mode.  */

#define OMR_FEATURE_PPC_ARCH_2_07             32 + 31
#define OMR_FEATURE_PPC_HTM                   32 + 30
#define OMR_FEATURE_PPC_DSCR                  32 + 29
#define OMR_FEATURE_PPC_EBB                   32 + 28
#define OMR_FEATURE_PPC_ISEL                  32 + 27
#define OMR_FEATURE_PPC_TAR                   32 + 26

/* s390 features
 * z/Architecture Principles of Operation 4-69
 * STORE FACILITY LIST EXTENDED (STFLE)
 */
#define OMR_FEATURE_S390_ESAN3      0 /* STFLE bit 0 */
#define OMR_FEATURE_S390_ZARCH      1 /* STFLE bit 2 */
#define OMR_FEATURE_S390_STFLE      2 /* STFLE bit 7 */
#define OMR_FEATURE_S390_MSA        3 /* STFLE bit 17 */
#define OMR_FEATURE_S390_DFP        6 /* STFLE bit 42 & 44 */
#define OMR_FEATURE_S390_HPAGE      7
#define OMR_FEATURE_S390_MSA_EXTENSION3                      11 /* STFLE bit 76 */
#define OMR_FEATURE_S390_MSA_EXTENSION4                      12 /* STFLE bit 77 */

#define OMR_FEATURE_S390_COMPARE_AND_SWAP_AND_STORE          32 + 0  /* STFLE bit 32 */
#define OMR_FEATURE_S390_COMPARE_AND_SWAP_AND_STORE2         32 + 1  /* STFLE bit 33 */
#define OMR_FEATURE_S390_EXECUTE_EXTENSIONS                  32 + 3  /* STFLE bit 35 */
#define OMR_FEATURE_S390_FPE                                 32 + 9  /* STFLE bit 41 */

#define OMR_FEATURE_S390_RI            64 + 0 /* STFLE bit 64 */

/* z990 facilities */

/* STFLE bit 19 - Long-displacement facility */
#define OMR_FEATURE_S390_LONG_DISPLACEMENT 19

/* z9 facilities */

/* STFLE bit 21 - Extended-immediate facility */
#define OMR_FEATURE_S390_EXTENDED_IMMEDIATE 21

/* STFLE bit 22 - Extended-translation facility 3 */
#define OMR_FEATURE_S390_EXTENDED_TRANSLATION_3 22

/* STFLE bit 30 - ETF3-enhancement facility */
#define OMR_FEATURE_S390_ETF3_ENHANCEMENT 30

/* z10 facilities */

/* STFLE bit 34 - General-instructions-extension facility */
#define OMR_FEATURE_S390_GENERAL_INSTRUCTIONS_EXTENSIONS 34

/* z196 facilities */

/* STFLE bit 45 - High-word facility */
#define OMR_FEATURE_S390_HIGH_WORD 45

/* STFLE bit 45 - Load/store-on-condition facility 1 */
#define OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_1 45

/* zEC12 facilities */

/* STFLE bit 49 - Miscellaneous-instruction-extension facility */
#define OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION 49

/* STFLE bit 50 - Constrained transactional-execution facility */
#define OMR_FEATURE_S390_CONSTRAINED_TRANSACTIONAL_EXECUTION_FACILITY 50
/* STFLE bit 73 - Transactional-execution facility */
#define OMR_FEATURE_S390_TRANSACTIONAL_EXECUTION_FACILITY 73

/* z13 facilities */

/* STFLE bit 53 - Load/store-on-condition facility 2 */
#define OMR_FEATURE_S390_LOAD_STORE_ON_CONDITION_2 53

/* STFLE bit 53 - Load-and-zero-rightmost-byte facility */
#define OMR_FEATURE_S390_LOAD_AND_ZERO_RIGHTMOST_BYTE 53

/* STFLE bit 129 - Vector facility */
#define OMR_FEATURE_S390_VECTOR_FACILITY 129

/* z14 facilities */

/* STFLE bit 58 - Miscellaneous-instruction-extensions facility 2 */
#define OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_2 58

/* STFLE bit 59 - Semaphore-assist facility */
#define OMR_FEATURE_S390_SEMAPHORE_ASSIST 59

/* STFLE bit 131 - Side-effect-access facility */
#define OMR_FEATURE_S390_SIDE_EFFECT_ACCESS 131

/* STFLE bit 133 - Guarded-storage facility */
#define OMR_FEATURE_S390_GUARDED_STORAGE 133

/* STFLE bit 134 - Vector packed decimal facility */
#define OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL 134

/* STFLE bit 135 - Vector enhancements facility 1 */
#define OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_1 135

/* STFLE bit 146 - Message-security-assist-extension-8 facility */
#define OMR_FEATURE_S390_MSA_EXTENSION_8 146

/* STFLE bit 57 - Message-security-assist-extension-5 facility */
#define OMR_FEATURE_S390_MSA_EXTENSION_5 57

/* z15 facilities */

/* STFLE bit 61 - Miscellaneous-instruction-extensions facility 3 */
#define OMR_FEATURE_S390_MISCELLANEOUS_INSTRUCTION_EXTENSION_3 61

/* STFLE bit 148 - Vector enhancements facility 2 */
#define OMR_FEATURE_S390_VECTOR_FACILITY_ENHANCEMENT_2 148

/* STFLE bit 152 - Vector packed decimal enhancement facility */
#define OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY 152

/* z16 facilities */

/* STFLE bit 192 - Vector-Packed-Decimal-Enhancement Facility 2 */
#define OMR_FEATURE_S390_VECTOR_PACKED_DECIMAL_ENHANCEMENT_FACILITY_2 192

/*  Linux on Z features
 *  Auxiliary Vector Hardware Capability (AT_HWCAP) features for Linux on Z.
 *  Obtained from: https://github.com/torvalds/linux/blob/050cdc6c9501abcd64720b8cc3e7941efee9547d/arch/s390/include/asm/elf.h#L94-L109.
 *  If new facility support is required, then it must be defined there (and here), before we can check for it consistently.
 *
 *  The linux kernel will use the defines in the above link to set HWCAP features. This is done inside "setup_hwcaps(void)" routine found
 *  in arch/s390/kernel/setup.c in the linux kernel source tree.
 */
#define OMR_HWCAP_S390_ESAN3     0x1
#define OMR_HWCAP_S390_ZARCH     0x2
#define OMR_HWCAP_S390_STFLE     0x4
#define OMR_HWCAP_S390_MSA       0x8
#define OMR_HWCAP_S390_LDISP     0x10
#define OMR_HWCAP_S390_EIMM      0x20
#define OMR_HWCAP_S390_DFP       0x40
#define OMR_HWCAP_S390_HPAGE     0x80
#define OMR_HWCAP_S390_ETF3EH    0x100
#define OMR_HWCAP_S390_HIGH_GPRS 0x200
#define OMR_HWCAP_S390_TE        0x400
#define OMR_HWCAP_S390_VXRS      0x800
#define OMR_HWCAP_S390_VXRS_BCD  0x1000
#define OMR_HWCAP_S390_VXRS_EXT  0x2000
#define OMR_HWCAP_S390_GS        0x4000

/* x86 features
 * INTEL INSTRUCTION SET REFERENCE, A-M
 * 3-170 Vol. 2A Table 3-21. More on Feature Information Returned in the EDX Register
 */
#define OMR_FEATURE_X86_FPU     0 /* Floating Point Unit On-Chip. */
#define OMR_FEATURE_X86_VME     1 /* Virtual 8086 Mode Enhancements. */
#define OMR_FEATURE_X86_DE      2 /* DE Debugging Extensions. */
#define OMR_FEATURE_X86_PSE     3 /* Page Size Extension. */
#define OMR_FEATURE_X86_TSC     4 /* Time Stamp Counter. */
#define OMR_FEATURE_X86_MSR     5 /* Model Specific Registers RDMSR and WRMSR Instructions. */
#define OMR_FEATURE_X86_PAE     6 /* Physical Address Extension. */
#define OMR_FEATURE_X86_MCE     7 /* Machine Check Exception. */
#define OMR_FEATURE_X86_CX8     8 /* Compare-and-exchange 8 bytes (64 bits) instruction */
#define OMR_FEATURE_X86_APIC    9 /* APIC On-Chip. */
#define OMR_FEATURE_X86_0_10   10 /* Reserved */
#define OMR_FEATURE_X86_SEP    11 /* SYSENTER and SYSEXIT Instructions. */
#define OMR_FEATURE_X86_MTRR   12 /* Memory Type Range Registers. */
#define OMR_FEATURE_X86_PGE    13 /* Page Global Bit. */
#define OMR_FEATURE_X86_MCA    14 /* Machine Check Architecture. */
#define OMR_FEATURE_X86_CMOV   15 /* Conditional Move Instructions. */
#define OMR_FEATURE_X86_PAT    16 /* Page Attribute Table. */
#define OMR_FEATURE_X86_PSE_36 17 /* 36-Bit Page Size Extension. */
#define OMR_FEATURE_X86_PSN    18 /* Processor Serial Number. */
#define OMR_FEATURE_X86_CLFSH  19 /* CLFLUSH Instruction. */
#define OMR_FEATURE_X86_0_20   20 /* Reserved */
#define OMR_FEATURE_X86_DS     21 /* Debug Store. */
#define OMR_FEATURE_X86_ACPI   22 /* Thermal Monitor and Software Controlled Clock Facilities. */
#define OMR_FEATURE_X86_MMX    23 /* Intel MMX Technology. */
#define OMR_FEATURE_X86_FXSR   24 /* FXSAVE and FXRSTOR Instructions. */
#define OMR_FEATURE_X86_SSE    25 /* The processor supports the SSE extensions. */
#define OMR_FEATURE_X86_SSE2   26 /* The processor supports the SSE2 extensions. */
#define OMR_FEATURE_X86_SS     27 /* Self Snoop. */
#define OMR_FEATURE_X86_HTT    28 /* Hyper Threading. */
#define OMR_FEATURE_X86_TM     29 /* Thermal Monitor. */
#define OMR_FEATURE_X86_0_30   30 /* Reserved */
#define OMR_FEATURE_X86_PBE    31 /* Pending Break Enable. */

/* INTEL INSTRUCTION SET REFERENCE, A-M
 * Vol. 2A 3-167 Table 3-20. Feature Information Returned in the ECX Register
 */
#define OMR_FEATURE_X86_SSE3         32 + 0 /* Streaming SIMD Extensions 3 */
#define OMR_FEATURE_X86_PCLMULQDQ    32 + 1 /* PCLMULQDQ. */
#define OMR_FEATURE_X86_DTES64       32 + 2 /* 64-bit DS Area. */
#define OMR_FEATURE_X86_MONITOR      32 + 3 /* MONITOR/MWAIT. */
#define OMR_FEATURE_X86_DS_CPL       32 + 4 /* CPL Qualified Debug Store. */
#define OMR_FEATURE_X86_VMX          32 + 5 /* Virtual Machine Extensions. */
#define OMR_FEATURE_X86_SMX          32 + 6 /* Safer Mode Extensions. */
#define OMR_FEATURE_X86_EIST         32 + 7 /* Enhanced Intel SpeedStep technology. */
#define OMR_FEATURE_X86_TM2          32 + 8 /* Thermal Monitor 2. */
#define OMR_FEATURE_X86_SSSE3        32 + 9 /* Supplemental Streaming SIMD Extensions 3 */
#define OMR_FEATURE_X86_CNXT_ID      32 + 10 /* L1 Context ID. */
#define OMR_FEATURE_X86_SDBG         32 + 11 /* Processor supports IA32_DEBUG_INTERFACE MSR for silicon debug. */
#define OMR_FEATURE_X86_FMA          32 + 12 /* FMA extensions using YMM state. */
#define OMR_FEATURE_X86_CMPXCHG16B   32 + 13 /* CMPXCHG16B Available. */
#define OMR_FEATURE_X86_XTPR         32 + 14 /* xTPR Update Control. */
#define OMR_FEATURE_X86_PDCM         32 + 15 /* Perfmon and Debug Capability. */
#define OMR_FEATURE_X86_1_16         32 + 16 /* Reserved. */
#define OMR_FEATURE_X86_PCID         32 + 17 /* Process-context identifiers. */
#define OMR_FEATURE_X86_DCA          32 + 18 /* Processor supports the ability to prefetch data from a memory mapped device. */
#define OMR_FEATURE_X86_SSE4_1       32 + 19 /* Processor supports SSE4.1. */
#define OMR_FEATURE_X86_SSE4_2       32 + 20 /* Processor supports SSE4.2. */
#define OMR_FEATURE_X86_X2APIC       32 + 21 /* Processor supports x2APIC feature. */
#define OMR_FEATURE_X86_MOVBE        32 + 22 /* Processor supports MOVBE instruction. */
#define OMR_FEATURE_X86_POPCNT       32 + 23 /* Processor supports the POPCNT instruction. */
#define OMR_FEATURE_X86_TSC_DEADLINE 32 + 24 /* Processor's local APIC timer supports one-shot operation using a TSC deadline value. */
#define OMR_FEATURE_X86_AESNI        32 + 25 /* Processor supports the AESNI instruction extensions. */
#define OMR_FEATURE_X86_XSAVE        32 + 26 /* Processor supports the XSAVE/XRSTOR processor extended states. */
#define OMR_FEATURE_X86_OSXSAVE      32 + 27 /* OS has enabled XSETBV/XGETBV instructions to access XCR0, and support for processor extended state management using XSAVE/XRSTOR. */
#define OMR_FEATURE_X86_AVX          32 + 28 /* Processor supports the AVX instruction extensions. */
#define OMR_FEATURE_X86_F16C         32 + 29 /* 16-bit floating-point conversion instructions. */
#define OMR_FEATURE_X86_RDRAND       32 + 30 /* Processor supports RDRAND instruction. */
#define OMR_FEATURE_X86_1_31         32 + 31 /* Not used */

/* INTEL INSTRUCTION SET REFERENCE, A-L May 2019
 * Vol. 2 3-197 Table 3-8. Structured Feature Information Returned in the EBX Register by CPUID instruction
 */
#define OMR_FEATURE_X86_FSGSBASE            96 + 0 /* fsgsbase instructions support */
#define OMR_FEATURE_X86_IA32_TSC_ADJUST     96 + 1 /* IA32_TSC_ADJUST MSR support */
#define OMR_FEATURE_X86_SGX                 96 + 2 /* Intel Software Guard Extensions */
#define OMR_FEATURE_X86_BMI1                96 + 3 /* Bit Manipulation Instructions 1 */
#define OMR_FEATURE_X86_HLE                 96 + 4 /* Hardware Lock Elison */
#define OMR_FEATURE_X86_AVX2                96 + 5 /* AVX2 support */
#define OMR_FEATURE_X86_FDP_EXCPTN_ONLY     96 + 6 /* x87 FPU data pointer updated only on exceptions */
#define OMR_FEATURE_X86_SMEP                96 + 7 /* Supervsior-Mode Execution Prevention */
#define OMR_FEATURE_X86_BMI2                96 + 8 /* Bit Manipulation Instructions 2 */
#define OMR_FEATURE_X86_ERMSB               96 + 9 /* Enhanced REP MOVSB/STOSB */
#define OMR_FEATURE_X86_INVPCID             96 + 10 /* Invalidate Process-Context Identifier instruction */
#define OMR_FEATURE_X86_RTM                 96 + 11 /* Restricted Transactional Memory */
#define OMR_FEATURE_X86_RDT_M               96 + 12 /* Intel RDT Monitoring */
#define OMR_FEATURE_X86_DEPRECATE_FPUCS     96 + 13 /* Deprecates FPU CS and FPU DS when set */
#define OMR_FEATURE_X86_MPX                 96 + 14 /* Intel Memory Protextion Extensions */
#define OMR_FEATURE_X86_RDT_A               96 + 15 /* Intel RDT Allocation */
#define OMR_FEATURE_X86_AVX512F             96 + 16 /* AVX512 Foundation */
#define OMR_FEATURE_X86_AVX512DQ            96 + 17 /* AVX512 Doubleword & Quadword */
#define OMR_FEATURE_X86_RDSEED              96 + 18 /* RDSEED instruction support */
#define OMR_FEATURE_X86_ADX                 96 + 19 /* Intel ADX (multi-precision arithmetic) */
#define OMR_FEATURE_X86_SMAP                96 + 20 /* Supervisor-Mode Access Prevention */
#define OMR_FEATURE_X86_AVX512_IFMA         96 + 21 /* AVX512 Integer Fused Multiply Add */
#define OMR_FEATURE_X86_3_22                96 + 22 /* reserved */
#define OMR_FEATURE_X86_CLFLUSHOPT          96 + 23 /* cache flush optimized */
#define OMR_FEATURE_X86_CLWB                96 + 24 /* cache line write back */
#define OMR_FEATURE_X86_IPT                 96 + 25 /* Intel Processor Trace */
#define OMR_FEATURE_X86_AVX512PF            96 + 26 /* AVX512 Prefetch */
#define OMR_FEATURE_X86_AVX512ER            96 + 27 /* AVX512 Exponential and Reciprocal */
#define OMR_FEATURE_X86_AVX512CD            96 + 28 /* AVX512 Conflict Detection */
#define OMR_FEATURE_X86_SHA                 96 + 29 /* Intel SHA Extensions */
#define OMR_FEATURE_X86_AVX512BW            96 + 30 /* AVX512 Byte and Word */
#define OMR_FEATURE_X86_AVX512VL            96 + 31 /* AVX512 Vector Length */

/*
 * Structured Feature Information Returned in the ECX Register by CPUID instruction when EAX = 7, ECX = 0
 */
#define OMR_FEATURE_X86_PREFETCHWT1         128 + 0
#define OMR_FEATURE_X86_AVX512_VBMI         128 + 1
#define OMR_FEATURE_X86_UMIP                128 + 2
#define OMR_FEATURE_X86_PKU                 128 + 3
#define OMR_FEATURE_X86_OSPKE               128 + 4
#define OMR_FEATURE_X86_WAITPKG             128 + 5
#define OMR_FEATURE_X86_AVX512_VBMI2        128 + 6
#define OMR_FEATURE_X86_CET_SS              128 + 7
#define OMR_FEATURE_X86_GFNI                128 + 8
#define OMR_FEATURE_X86_VAES                128 + 9
#define OMR_FEATURE_X86_VPCLMULQDQ          128 + 10
#define OMR_FEATURE_X86_AVX512_VNNI         128 + 11
#define OMR_FEATURE_X86_AVX512_BITALG       128 + 12
#define OMR_FEATURE_X86_TME_EN              128 + 13
#define OMR_FEATURE_X86_AVX512_VPOPCNTDQ    128 + 14
#define OMR_FEATURE_X86_4_15                128 + 15 /* Reserved */
#define OMR_FEATURE_X86_LA57                128 + 16
#define OMR_FEATURE_X86_MAWAU_0             128 + 17
#define OMR_FEATURE_X86_MAWAU_1             128 + 18
#define OMR_FEATURE_X86_MAWAU_2             128 + 19
#define OMR_FEATURE_X86_MAWAU_3             128 + 20
#define OMR_FEATURE_X86_MAWAU_4             128 + 21
#define OMR_FEATURE_X86_RDPID               128 + 22
#define OMR_FEATURE_X86_KL                  128 + 23
#define OMR_FEATURE_X86_BUS_LOCK_DETECT     128 + 24
#define OMR_FEATURE_X86_CLDEMOTE            128 + 25
#define OMR_FEATURE_X86_4_26                128 + 26 /* Reserved */
#define OMR_FEATURE_X86_MOVDIRI             128 + 27
#define OMR_FEATURE_X86_MOVDIR64B           128 + 28
#define OMR_FEATURE_X86_ENQCMD              128 + 29
#define OMR_FEATURE_X86_SGX_LC              128 + 30
#define OMR_FEATURE_X86_PKS                 128 + 31

/*  AArch64 Linux features
 *  See https://www.kernel.org/doc/html/latest/arm64/elf_hwcaps.html.
 */
#define OMR_FEATURE_ARM64_FP                0 /* Floating Point without FP16 */
#define OMR_FEATURE_ARM64_ASIMD             1 /* Advanced SIMD without FP16 */
#define OMR_FEATURE_ARM64_EVTSTRM           2 /* Event Stream */
#define OMR_FEATURE_ARM64_AES               3 /* FEAT_AES */
#define OMR_FEATURE_ARM64_PMULL             4 /* FEAT_PMULL */
#define OMR_FEATURE_ARM64_SHA1              5 /* FEAT_SHA1 */
#define OMR_FEATURE_ARM64_SHA256            6 /* FEAT_SHA256 */
#define OMR_FEATURE_ARM64_CRC32             7 /* FEAT_CRC32 */
#define OMR_FEATURE_ARM64_LSE               8 /* FEAT_LSE */
#define OMR_FEATURE_ARM64_FP16              9 /* FEAT_FP16 */
#define OMR_FEATURE_ARM64_ASIMDHP          10 /* Advanced SIMD with FP16 */
#define OMR_FEATURE_ARM64_CPUID            11 /* CPU ID/feature register */
#define OMR_FEATURE_ARM64_RDM              12 /* FEAT_RDM */
#define OMR_FEATURE_ARM64_JSCVT            13 /* FEAT_JSCVT */
#define OMR_FEATURE_ARM64_FCMA             14 /* FEAT_FCMA */
#define OMR_FEATURE_ARM64_LRCPC            15 /* FEAT_LRCPC */
#define OMR_FEATURE_ARM64_DPB              16 /* FEAT_DPB */
#define OMR_FEATURE_ARM64_SHA3             17 /* FEAT_SHA3 */
#define OMR_FEATURE_ARM64_SM3              18 /* FEAT_SM3 */
#define OMR_FEATURE_ARM64_SM4              19 /* FEAT_SM4 */
#define OMR_FEATURE_ARM64_DOTPROD          20 /* FEAT_DotProd */
#define OMR_FEATURE_ARM64_SHA512           21 /* FEAT_SHA512 */
#define OMR_FEATURE_ARM64_SVE              22 /* FEAT_SVE */
#define OMR_FEATURE_ARM64_FHM              23 /* FEAT_FHM */
#define OMR_FEATURE_ARM64_DIT              24 /* FEAT_DIT */
#define OMR_FEATURE_ARM64_LSE2             25 /* FEAT_LSE2 */
#define OMR_FEATURE_ARM64_LRCPC2           26 /* FEAT_LRCPC2 */
#define OMR_FEATURE_ARM64_FLAGM            27 /* FEAT_FlagM */
#define OMR_FEATURE_ARM64_SSBS             28 /* FEAT_SSBS */
#define OMR_FEATURE_ARM64_SB               29 /* FEAT_SB */
#define OMR_FEATURE_ARM64_PAUTH            30 /* FEAT_PAuth */
#define OMR_FEATURE_ARM64_PACG             31 /* PACGA instruction */

#define OMR_FEATURE_ARM64_DPB2             32 /* FEAT_DPB2 */
#define OMR_FEATURE_ARM64_SVE2             33 /* FEAT_SVE2 */
#define OMR_FEATURE_ARM64_SVE_AES          34 /* FEAT_SVE_AES */
#define OMR_FEATURE_ARM64_SVE_PMULL128     35 /* FEAT_SVE_PMULL128 */
#define OMR_FEATURE_ARM64_SVE_BITPERM      36 /* FEAT_SVE_BitPerm */
#define OMR_FEATURE_ARM64_SVE_SHA3         37 /* FEAT_SVE_SHA3 */
#define OMR_FEATURE_ARM64_SVE_SM4          38 /* FEAT_SVE_SM4 */
#define OMR_FEATURE_ARM64_FLAGM2           39 /* FEAT_FlagM2 */
#define OMR_FEATURE_ARM64_FRINTTS          40 /* FEAT_FRINTTS */
#define OMR_FEATURE_ARM64_SVE_I8MM         41 /* FEAT_I8MM */
#define OMR_FEATURE_ARM64_F32MM            42 /* FEAT_F32MM */
#define OMR_FEATURE_ARM64_F64MM            43 /* FEAT_F64MM */
#define OMR_FEATURE_ARM64_SVE_BF16         44 /* FEAT_BF16 */
#define OMR_FEATURE_ARM64_I8MM             45 /* FEAT_I8MM */
#define OMR_FEATURE_ARM64_BF16             46 /* FEAT_BF16 */
#define OMR_FEATURE_ARM64_DGH              47 /* FEAT_DGH */
#define OMR_FEATURE_ARM64_RNG              48 /* FEAT_RNG */
#define OMR_FEATURE_ARM64_BTI              49 /* FEAT_BTI */
#define OMR_FEATURE_ARM64_MTE2             50 /* FEAT_MTE2 */

struct OMRControlFileStatus;
struct OMRPortShSemParameters;

struct omrshsem_handle;
struct omrshmem_handle;

struct OMRPortLibrary;
typedef struct J9Heap J9Heap;

typedef uintptr_t (*omrsig_protected_fn)(struct OMRPortLibrary *portLib, void *handler_arg);
typedef uintptr_t (*omrsig_handler_fn)(struct OMRPortLibrary *portLib, uint32_t gpType, void *gpInfo, void *handler_arg);

typedef struct OMRPortLibrary {
	/** portGlobals*/
	struct OMRPortLibraryGlobalData *portGlobals;
	/** see @ref omrport.c::omrport_shutdown_library "omrport_shutdown_library"*/
	int32_t (*port_shutdown_library)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrport.c::omrport_isFunctionOverridden "omrport_isFunctionOverridden"*/
	int32_t (*port_isFunctionOverridden)(struct OMRPortLibrary *portLibrary, uintptr_t offset) ;
	/** see @ref omrport.c::omrport_tls_free "omrport_tls_free"*/
	void (*port_tls_free)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrerror.c::omrerror_startup "omrerror_startup"*/
	int32_t (*error_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrerror.c::omrerror_shutdown "omrerror_shutdown"*/
	void (*error_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrerror.c::omrerror_last_error_number "omrerror_last_error_number"*/
	int32_t (*error_last_error_number)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrerror.c::omrerror_last_error_message "omrerror_last_error_message"*/
	const char *(*error_last_error_message)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrerror.c::omrerror_set_last_error "omrerror_set_last_error"*/
	int32_t (*error_set_last_error)(struct OMRPortLibrary *portLibrary,  int32_t platformCode, int32_t portableCode) ;
	/** see @ref omrerror.c::omrerror_set_last_error_with_message "omrerror_set_last_error_with_message"*/
	int32_t (*error_set_last_error_with_message)(struct OMRPortLibrary *portLibrary, int32_t portableCode, const char *errorMessage) ;
	/** see @ref omrerror.c::omrerror_set_last_error_with_message_format "omrerror_set_last_error_with_message_format"*/
	int32_t (*error_set_last_error_with_message_format)(struct OMRPortLibrary *portLibrary, int32_t portableCode, const char *format, ...) ;
	/** see @ref omrtime.c::omrtime_startup "omrtime_startup"*/
	int32_t (*time_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_shutdown "omrtime_shutdown"*/
	void (*time_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_msec_clock "omrtime_msec_clock"*/
	uintptr_t (*time_msec_clock)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_usec_clock "omrtime_usec_clock"*/
	uintptr_t (*time_usec_clock)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_current_time_nanos "omrtime_current_time_nanos"*/
	uint64_t (*time_current_time_nanos)(struct OMRPortLibrary *portLibrary, uintptr_t *success) ;
	/** see @ref omrtime.c::omrtime_current_time_millis "omrtime_current_time_millis"*/
	int64_t (*time_current_time_millis)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_nano_time "omrtime_nano_time"*/
	int64_t (*time_nano_time)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_hires_clock "omrtime_hires_clock"*/
	uint64_t (*time_hires_clock)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_hires_frequency "omrtime_hires_frequency"*/
	uint64_t (*time_hires_frequency)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtime.c::omrtime_hires_delta "omrtime_hires_delta"*/
	uint64_t (*time_hires_delta)(struct OMRPortLibrary *portLibrary, uint64_t startTime, uint64_t endTime, uint64_t requiredResolution) ;
	/** see @ref omrsysinfo.c::omrsysinfo_startup "omrsysinfo_startup"*/
	int32_t (*sysinfo_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_shutdown "omrsysinfo_shutdown"*/
	void (*sysinfo_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_process_exists "omrsysinfo_process_exists"*/
	intptr_t (*sysinfo_process_exists)(struct OMRPortLibrary *portLibrary, uintptr_t pid)  ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_egid "omrsysinfo_get_egid"*/
	uintptr_t (*sysinfo_get_egid)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_euid "omrsysinfo_get_euid"*/
	uintptr_t (*sysinfo_get_euid)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_groups "omrsysinfo_get_groups"*/
	intptr_t (*sysinfo_get_groups)(struct OMRPortLibrary *portLibrary, uint32_t **gidList, uint32_t categoryCode) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_pid "omrsysinfo_get_pid"*/
	uintptr_t (*sysinfo_get_pid)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_ppid "omrsysinfo_get_ppid"*/
	uintptr_t (*sysinfo_get_ppid)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_memory_info "omrsysinfo_get_memory_info"*/
	int32_t (*sysinfo_get_memory_info)(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo, ...) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_processor_info "omrsysinfo_get_processor_info"*/
	int32_t (*sysinfo_get_processor_info)(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfo) ;
	/** see @ref omrsysinfo.c::omrsysinfo_destroy_processor_info "omrsysinfo_destroy_processor_info"*/
	void (*sysinfo_destroy_processor_info)(struct OMRPortLibrary *portLibrary, struct J9ProcessorInfos *procInfos) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_addressable_physical_memory "omrsysinfo_get_addressable_physical_memory"*/
	uint64_t (*sysinfo_get_addressable_physical_memory)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_physical_memory "omrsysinfo_get_physical_memory"*/
	uint64_t (*sysinfo_get_physical_memory)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_OS_version "omrsysinfo_get_OS_version"*/
	const char  *(*sysinfo_get_OS_version)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_env "omrsysinfo_get_env"*/
	intptr_t (*sysinfo_get_env)(struct OMRPortLibrary *portLibrary, const char *envVar, char *infoString, uintptr_t bufSize) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_CPU_architecture "omrsysinfo_get_CPU_architecture"*/
	const char *(*sysinfo_get_CPU_architecture)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_processor_description "omrsysinfo_get_processor_description"*/
	intptr_t  ( *sysinfo_get_processor_description)(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc) ;
	/** see @ref omrsysinfo.c::omrsysinfo_processor_has_feature "omrsysinfo_processor_has_feature"*/
	BOOLEAN  ( *sysinfo_processor_has_feature)(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, uint32_t feature) ;
	/** see @ref omrsysinfo.c::omrsysinfo_processor_set_feature "omrsysinfo_processor_set_feature"*/
	intptr_t  ( *sysinfo_processor_set_feature)(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, uint32_t feature, BOOLEAN enable) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_processor_feature_name "omrsysinfo_get_processor_feature_name"*/
	const char  *( *sysinfo_get_processor_feature_name)(struct OMRPortLibrary *portLibrary, uint32_t feature) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_processor_feature_string "omrsysinfo_get_processor_feature_string"*/
	intptr_t ( *sysinfo_get_processor_feature_string)(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, char * buffer, const size_t length) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_OS_type "omrsysinfo_get_OS_type"*/
	const char *(*sysinfo_get_OS_type)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_executable_name "omrsysinfo_get_executable_name"*/
	intptr_t (*sysinfo_get_executable_name)(struct OMRPortLibrary *portLibrary, const char *argv0, char **result) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_username "omrsysinfo_get_username"*/
	intptr_t (*sysinfo_get_username)(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_groupname "omrsysinfo_get_groupname"*/
	intptr_t (*sysinfo_get_groupname)(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_hostname "omrsysinfo_get_hostname"*/
	intptr_t (*sysinfo_get_hostname)(struct OMRPortLibrary *portLibrary, char *buffer, size_t length) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_load_average "omrsysinfo_get_load_average"*/
	intptr_t (*sysinfo_get_load_average)(struct OMRPortLibrary *portLibrary, struct J9PortSysInfoLoadData *loadAverageData) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_CPU_utilization "omrsysinfo_get_CPU_utilization"*/
	intptr_t (*sysinfo_get_CPU_utilization)(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTime) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_CPU_load "omrsysinfo_get_CPU_utilization"*/
	intptr_t (*sysinfo_get_CPU_load)(struct OMRPortLibrary *portLibrary, double *load) ;
	/** see @ref omrsysinfo.c::omrsysinfo_limit_iterator_init "omrsysinfo_limit_iterator_init"*/
	int32_t (*sysinfo_limit_iterator_init)(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state) ;
	/** see @ref omrsysinfo.c::omrsysinfo_limit_iterator_hasNext "omrsysinfo_limit_iterator_hasNext"*/
	BOOLEAN (*sysinfo_limit_iterator_hasNext)(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state) ;
	/** see @ref omrsysinfo.c::omrsysinfo_limit_iterator_next "omrsysinfo_limit_iterator_next"*/
	int32_t (*sysinfo_limit_iterator_next)(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state, J9SysinfoUserLimitElement *limitElement) ;
	/** see @ref omrsysinfo.c::omrsysinfo_env_iterator_init "omrsysinfo_env_iterator_init"*/
	int32_t (*sysinfo_env_iterator_init)(struct OMRPortLibrary *portLibrary, struct J9SysinfoEnvIteratorState *state, void *buffer, uintptr_t bufferSizeBytes) ;
	/** see @ref omrsysinfo.c::omrsysinfo_env_iterator_hasNext "omrsysinfo_env_iterator_hasNext"*/
	BOOLEAN (*sysinfo_env_iterator_hasNext)(struct OMRPortLibrary *portLibrary, struct J9SysinfoEnvIteratorState *state) ;
	/** see @ref omrsysinfo.c::omrsysinfo_env_iterator_next "omrsysinfo_env_iterator_next"*/
	int32_t (*sysinfo_env_iterator_next)(struct OMRPortLibrary *portLibrary, struct J9SysinfoEnvIteratorState *state, J9SysinfoEnvElement *envElement) ;
	/** see @ref omrfile.c::omrfile_startup "omrfile_startup"*/
	int32_t (*file_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrfile.c::omrfile_shutdown "omrfile_shutdown"*/
	void (*file_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrfile.c::omrfile_write "omrfile_write"*/
	intptr_t (*file_write)(struct OMRPortLibrary *portLibrary, intptr_t fd, const void *buf, intptr_t nbytes) ;
	/** see @ref omrfile.c::omrfile_write_text "omrfile_write_text"*/
	intptr_t (*file_write_text)(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes) ;
	/** see @ref omrfile.c::omrfile_get_text_encoding "omrfile_get_text_encoding"*/
	int32_t (*file_get_text_encoding)(struct OMRPortLibrary *portLibrary, char *charsetName, uintptr_t nbytes) ;
	/** see @ref omrfile.c::omrfile_vprintf "omrfile_vprintf"*/
	void (*file_vprintf)(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, va_list args) ;
	/** see @ref omrfile.c::omrfile_printf "omrfile_printf"*/
	void (*file_printf)(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, ...) ;
	/** see @ref omrfile.c::omrfile_open "omrfile_open"*/
	intptr_t (*file_open)(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode) ;
	/** see @ref omrfile.c::omrfile_close "omrfile_close"*/
	int32_t (*file_close)(struct OMRPortLibrary *portLibrary, intptr_t fd) ;
	/** see @ref omrfile.c::omrfile_seek "omrfile_seek"*/
	int64_t (*file_seek)(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t offset, int32_t whence) ;
	/** see @ref omrfile.c::omrfile_read "omrfile_read"*/
	intptr_t (*file_read)(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes) ;
	/** see @ref omrfile.c::omrfile_unlink "omrfile_unlink"*/
	int32_t (*file_unlink)(struct OMRPortLibrary *portLibrary, const char *path) ;
	/** see @ref omrfile.c::omrfile_attr "omrfile_attr"*/
	int32_t (*file_attr)(struct OMRPortLibrary *portLibrary, const char *path) ;
	/** see @ref omrfile.c::omrfile_chmod "omrfile_chmod"*/
	int32_t (*file_chmod)(struct OMRPortLibrary *portLibrary, const char *path, int32_t mode) ;
	/** see @ref omrfile.c::omrfile_chown "omrfile_chown"*/
	intptr_t (*file_chown)(struct OMRPortLibrary *portLibrary, const char *path, uintptr_t owner, uintptr_t group) ;
	/** see @ref omrfile.c::omrfile_lastmod "omrfile_lastmod"*/
	int64_t (*file_lastmod)(struct OMRPortLibrary *portLibrary, const char *path) ;
	/** see @ref omrfile.c::omrfile_length "omrfile_length"*/
	int64_t (*file_length)(struct OMRPortLibrary *portLibrary, const char *path) ;
	/** see @ref omrfile.c::omrfile_flength "omrfile_flength"*/
	int64_t (*file_flength)(struct OMRPortLibrary *portLibrary, intptr_t fd) ;
	/** see @ref omrfile.c::omrfile_set_length "omrfile_set_length"*/
	int32_t (*file_set_length)(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength) ;
	/** see @ref omrfile.c::omrfile_sync "omrfile_sync"*/
	int32_t (*file_sync)(struct OMRPortLibrary *portLibrary, intptr_t fd) ;
	/** see @ref omrfile.c::omrfile_fstat "omrfile_fstat"*/
	int32_t (*file_fstat)(struct OMRPortLibrary *portLibrary, intptr_t fd, struct J9FileStat *buf) ;
	/** see @ref omrfile.c::omrfile_stat "omrfile_stat"*/
	int32_t (*file_stat)(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStat *buf) ;
	/** see @ref omrfile.c::omrfile_stat_filesystem "omrfile_stat_filesystem"*/
	int32_t (*file_stat_filesystem)(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStatFilesystem *buf) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_open "omrfile_blockingasync_open"*/
	intptr_t (*file_blockingasync_open)(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_close "omrfile_blockingasync_close"*/
	int32_t (*file_blockingasync_close)(struct OMRPortLibrary *portLibrary, intptr_t fd) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_read "omrfile_blockingasync_read"*/
	intptr_t (*file_blockingasync_read)(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_write "omrfile_blockingasync_write"*/
	intptr_t (*file_blockingasync_write)(struct OMRPortLibrary *portLibrary, intptr_t fd, const void *buf, intptr_t nbytes) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_set_length "omrfile_blockingasync_set_length"*/
	int32_t (*file_blockingasync_set_length)(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_flength "omrfile_blockingasync_flength"*/
	int64_t (*file_blockingasync_flength)(struct OMRPortLibrary *portLibrary, intptr_t fd) ;
	/** see @ref omrfile.c::omrfile_blockingasync_startup "omrfile_blockingasync_startup"*/
	int32_t (*file_blockingasync_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrfile.c::omrfile_blockingasync_shutdown "omrfile_blockingasync_shutdown"*/
	void (*file_blockingasync_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrfilestream::omrfilestream_startup "filestream_startup"*/
	int32_t ( *filestream_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrfilestream::omrfilestream_shutdown "filestream_shutdown"*/
	void ( *filestream_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrfilestream::omrfilestreamm_open "filestream_open"*/
	OMRFileStream *( *filestream_open)(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode) ;
	/** see @ref omrfilestream::omrfilestream_close "filestream_close"*/
	int32_t ( *filestream_close)(struct OMRPortLibrary *portLibrary,  OMRFileStream *fileStream) ;
	/** see @ref omrfilestream::omrfilestream_write "filestream_write"*/
	intptr_t ( *filestream_write)(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const void *buf, intptr_t nbytes) ;
	/** see @ref omrfilestream::omrfilestream_write_text "filestream_write_text"*/
	intptr_t ( *filestream_write_text)(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *buf, intptr_t nbytes, int32_t toCode) ;
	/** see @ref omrfilestream::omrfilestream_vprintf "filestream_vprintf"*/
	void ( *filestream_vprintf)(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *format, va_list args) ;
	/** see @ref omrfilestream::omrfilestream_printf "filestream_printf"*/
	void ( *filestream_printf)(struct OMRPortLibrary *portLibrary, OMRFileStream *filestream, const char *format, ...) ;
	/** see @ref omrfilestream::omrfilestream_sync "filestream_sync"*/
	int32_t ( *filestream_sync)(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream) ;
	/** see @ref omrfilestream::omrfilestream_setbuffer "filestream_setbuffer"*/
	int32_t ( *filestream_setbuffer)(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, char *buffer, int32_t mode, uintptr_t size) ;
	/** see @ref omrfilestream::omrfilestream_fdopen "filestream_fdopen"*/
	OMRFileStream *( *filestream_fdopen)(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t flags) ;
	/** see @ref omrfilestream::omrfilestream_fileno "filestream_fileno"*/
	intptr_t ( *filestream_fileno)(struct OMRPortLibrary *portLibrary, OMRFileStream *stream) ;
	/** see @ref omrsl.c::omrsl_startup "omrsl_startup"*/
	int32_t (*sl_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsl.c::omrsl_shutdown "omrsl_shutdown"*/
	void (*sl_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsl.c::omrsl_close_shared_library "omrsl_close_shared_library"*/
	uintptr_t (*sl_close_shared_library)(struct OMRPortLibrary *portLibrary, uintptr_t descriptor) ;
	/** see @ref omrsl.c::omrsl_open_shared_library "omrsl_open_shared_library"*/
	uintptr_t (*sl_open_shared_library)(struct OMRPortLibrary *portLibrary, char *name, uintptr_t *descriptor, uintptr_t flags) ;
	/** see @ref omrsl.c::omrsl_lookup_name "omrsl_lookup_name"*/
	uintptr_t (*sl_lookup_name)(struct OMRPortLibrary *portLibrary, uintptr_t descriptor, char *name, uintptr_t *func, const char *argSignature) ;
	/** see @ref omrtty.c::omrtty_startup "omrtty_startup"*/
	int32_t (*tty_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtty.c::omrtty_shutdown "omrtty_shutdown"*/
	void (*tty_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtty.c::omrtty_printf "omrtty_printf"*/
	void (*tty_printf)(struct OMRPortLibrary *portLibrary, const char *format, ...) ;
	/** see @ref omrtty.c::omrtty_vprintf "omrtty_vprintf"*/
	void (*tty_vprintf)(struct OMRPortLibrary *portLibrary, const char *format, va_list args) ;
	/** see @ref omrtty.c::omrtty_get_chars "omrtty_get_chars"*/
	intptr_t (*tty_get_chars)(struct OMRPortLibrary *portLibrary, char *s, uintptr_t length) ;
	/** see @ref omrtty.c::omrtty_err_printf "omrtty_err_printf"*/
	void (*tty_err_printf)(struct OMRPortLibrary *portLibrary, const char *format, ...) ;
	/** see @ref omrtty.c::omrtty_err_vprintf "omrtty_err_vprintf"*/
	void (*tty_err_vprintf)(struct OMRPortLibrary *portLibrary, const char *format, va_list args) ;
	/** see @ref omrtty.c::omrtty_available "omrtty_available"*/
	intptr_t (*tty_available)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrtty.c::omrtty_daemonize "omrtty_daemonize"*/
	void (*tty_daemonize)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrheap.c::omrheap_create "omrheap_create"*/
	J9Heap *(*heap_create)(struct OMRPortLibrary *portLibrary, void *heapBase, uintptr_t heapSize, uint32_t heapFlags) ;
	/** see @ref omrheap.c::omrheap_allocate "omrheap_allocate"*/
	void *(*heap_allocate)(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, uintptr_t byteAmount) ;
	/** see @ref omrheap.c::omrheap_free "omrheap_free"*/
	void (*heap_free)(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address) ;
	/** see @ref omrheap.c::omrheap_reallocate "omrheap_reallocate"*/
	void *(*heap_reallocate)(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address, uintptr_t byteAmount) ;
	/** see @ref omrmem.c::omrmem_startup "omrmem_startup"*/
	int32_t (*mem_startup)(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize) ;
	/** see @ref omrmem.c::omrmem_shutdown "omrmem_shutdown"*/
	void (*mem_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrmem.c::omrmem_allocate_memory "omrmem_allocate_memory"*/
	void  *(*mem_allocate_memory)(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category) ;
	/** see @ref omrmem.c::omrmem_free_memory "omrmem_free_memory"*/
	void (*mem_free_memory)(struct OMRPortLibrary *portLibrary, void *memoryPointer) ;
	/** see @ref omrmem.c::omrmem_advise_and_free_memory "omrmem_advise_and_free_memory"*/
	void (*mem_advise_and_free_memory)(struct OMRPortLibrary *portLibrary, void *memoryPointer) ;
	/** see @ref omrmem.c::omrmem_reallocate_memory "omrmem_reallocate_memory"*/
	void *(*mem_reallocate_memory)(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount, const char *callSite, uint32_t category) ;
	/** see @ref omrmem.c::omrmem_allocate_memory32 "omrmem_allocate_memory32"*/
	void *(*mem_allocate_memory32)(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite,  uint32_t category) ;
	/** see @ref omrmem.c::omrmem_free_memory32 "omrmem_free_memory32"*/
	void (*mem_free_memory32)(struct OMRPortLibrary *portLibrary, void *memoryPointer) ;
	/** see @ref omrmem.c::omrmem_ensure_capacity32 "omrmem_ensure_capacity32"*/
	uintptr_t (*mem_ensure_capacity32)(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount) ;
	/** see @ref omrcpu.c::omrcpu_startup "omrcpu_startup"*/
	int32_t (*cpu_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrcpu.c::omrcpu_shutdown "omrcpu_shutdown"*/
	void (*cpu_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrcpu.c::omrcpu_flush_icache "omrcpu_flush_icache"*/
	void (*cpu_flush_icache)(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount) ;
	/** see @ref omrcpu.c::omrcpu_get_cache_line_size "cpu_get_cache_line_size"  */
	int32_t (*cpu_get_cache_line_size)(struct OMRPortLibrary *portLibrary, int32_t *lineSize) ;
	/** see @ref omrvmem.c::omrvmem_startup "omrvmem_startup"*/
	int32_t (*vmem_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrvmem.c::omrvmem_shutdown "omrvmem_shutdown"*/
	void (*vmem_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrvmem.c::omrvmem_commit_memory "omrvmem_commit_memory"*/
	void *(*vmem_commit_memory)(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier) ;
	/** see @ref omrvmem.c::omrvmem_decommit_memory "omrvmem_decommit_memory"*/
	intptr_t (*vmem_decommit_memory)(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier) ;
	/** see @ref omrvmem.c::omrvmem_free_memory "omrvmem_free_memory"*/
	int32_t (*vmem_free_memory)(struct OMRPortLibrary *portLibrary, void *userAddress, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier) ;
	/** see @ref omrvmem.c::omrvmem_vmem_params_init "omrvmem_vmem_params_init"*/
	int32_t (*vmem_vmem_params_init)(struct OMRPortLibrary *portLibrary, struct J9PortVmemParams *params) ;
	/** see @ref omrvmem.c::omrvmem_reserve_memory "omrvmem_reserve_memory"*/
	void *(*vmem_reserve_memory)(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize,  uint32_t category) ;
	/** see @ref omrvmem.c::omrvmem_reserve_memory_ex "omrvmem_reserve_memory_ex"*/
	void *(*vmem_reserve_memory_ex)(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, struct J9PortVmemParams *params) ;
	/** see @ref omrvmem.c::omrvmem_get_contiguous_region_memory "omrvmem_get_contiguous_region_memory"*/
	void *(*vmem_get_contiguous_region_memory)(struct OMRPortLibrary *portLibrary, void* addresses[], uintptr_t addressesCount, uintptr_t addressSize, uintptr_t byteAmount, struct J9PortVmemIdentifier *oldIdentifier, struct J9PortVmemIdentifier *newIdentifier, uintptr_t mode, uintptr_t pageSize, OMRMemCategory *category);
	/** see @ref omrvmem.c::omrvmem_create_double_mapped_region "omrvmem_create_double_mapped_region"*/
	void *(*vmem_create_double_mapped_region)(struct OMRPortLibrary *portLibrary, void* regionAddresses[], uintptr_t regionsCount, uintptr_t regionSize, uintptr_t byteAmount, struct J9PortVmemIdentifier *oldIdentifier, struct J9PortVmemIdentifier *newIdentifier, uintptr_t mode, uintptr_t pageSize, OMRMemCategory *category, void *preferredAddress);
	/** see @ref omrvmem.c::omrvmem_release_double_mapped_region "omrvmem_release_double_mapped_region"*/
	int32_t (*vmem_release_double_mapped_region)(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier);
	/** see @ref omrvmem.c::omrvmem_get_page_size "omrvmem_get_page_size"*/
	uintptr_t (*vmem_get_page_size)(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier) ;
	/** see @ref omrvmem.c::omrvmem_get_page_flags "omrvmem_get_page_flags"*/
	uintptr_t (*vmem_get_page_flags)(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier) ;
	/** see @ref omrvmem.c::omrvmem_supported_page_sizes "omrvmem_supported_page_sizes"*/
	uintptr_t *(*vmem_supported_page_sizes)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrvmem.c::omrvmem_supported_page_flags "omrvmem_supported_page_flags"*/
	uintptr_t *(*vmem_supported_page_flags)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrvmem.c::omrvmem_default_large_page_size_ex "omrvmem_default_large_page_size_ex"*/
	void (*vmem_default_large_page_size_ex)(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags) ;
	/** see @ref omrvmem.c::omrvmem_find_valid_page_size "omrvmem_find_valid_page_size"*/
	intptr_t (*vmem_find_valid_page_size)(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags, BOOLEAN *isSizeSupported) ;
	/** see @ref omrvmem.c::omrvmem_numa_set_affinity "omrvmem_numa_set_affinity"*/
	intptr_t (*vmem_numa_set_affinity)(struct OMRPortLibrary *portLibrary, uintptr_t numaNode, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier) ;
	/** see @ref omrvmem.c::omrvmem_numa_get_node_details "omrvmem_numa_get_node_details"*/
	intptr_t (*vmem_numa_get_node_details)(struct OMRPortLibrary *portLibrary, struct J9MemoryNodeDetail *numaNodes, uintptr_t *nodeCount) ;
	/** see @ref omrvmem.c::omrvmem_get_available_physical_memory "omrvmem_get_available_physical_memory"*/
	int32_t (*vmem_get_available_physical_memory)(struct OMRPortLibrary *portLibrary, uint64_t *freePhysicalMemorySize);
	/** see @ref omrvmem.c::omrvmem_get_process_memory_size "omrvmem_get_process_memory_size"*/
	int32_t (*vmem_get_process_memory_size)(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize);
	/** see @ref omrstr.c::omrstr_startup "omrstr_startup"*/
	int32_t (*str_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrstr.c::omrstr_shutdown "omrstr_shutdown"*/
	void (*str_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrstr.c::omrstr_printf "omrstr_printf"*/
	uintptr_t (*str_printf)(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, ...) ;
	/** see @ref omrstr.c::omrstr_vprintf "omrstr_vprintf"*/
	uintptr_t (*str_vprintf)(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, va_list args) ;
	/** see @ref omrstr.c::omrstr_create_tokens "omrstr_create_tokens"*/
	J9StringTokens *(*str_create_tokens)(struct OMRPortLibrary *portLibrary, int64_t timeMillis) ;
	/** see @ref omrstr.c::omrstr_set_token "omrstr_set_token"*/
	intptr_t (*str_set_token)(struct OMRPortLibrary *portLibrary, struct J9StringTokens *tokens, const char *key, const char *format, ...) ;
	/** see @ref omrstr.c::omrstr_subst_tokens "omrstr_subst_tokens"*/
	uintptr_t (*str_subst_tokens)(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, struct J9StringTokens *tokens) ;
	/** see @ref omrstr.c::omrstr_free_tokens "omrstr_free_tokens"*/
	void (*str_free_tokens)(struct OMRPortLibrary *portLibrary, struct J9StringTokens *tokens) ;
	/** see @ref omrstr.c::omrstr_set_time_tokens "omrstr_set_time_tokens"*/
	intptr_t (*str_set_time_tokens)(struct OMRPortLibrary *portLibrary, struct J9StringTokens *tokens, int64_t timeMillis) ;
	/** see @ref omrstr.c::omrstr_convert "omrstr_convert"*/
	int32_t (*str_convert)(struct OMRPortLibrary *portLibrary, int32_t fromCode, int32_t toCode, const char *inBuffer, uintptr_t inBufferSize, char *outBuffer, uintptr_t outBufferSize) ;
	/** see @ref omrexit.c::omrexit_startup "omrexit_startup"*/
	int32_t (*exit_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrexit.c::omrexit_shutdown "omrexit_shutdown"*/
	void (*exit_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrexit.c::omrexit_get_exit_code "omrexit_get_exit_code"*/
	int32_t (*exit_get_exit_code)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrexit.c::omrexit_shutdown_and_exit "omrexit_shutdown_and_exit"*/
	void (*exit_shutdown_and_exit)(struct OMRPortLibrary *portLibrary, int32_t exitCode) ;
	/** self_handle*/
	void *self_handle;
	/** see @ref omrosdump.c::omrdump_create "omrdump_create" */
	uintptr_t (*dump_create)(struct OMRPortLibrary *portLibrary, char *filename, char *dumpType, void *userData) ;
	/** see @ref omrosdump.c::omrdump_startup "omrdump_startup" */
	int32_t (*dump_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrosdump.c::omrdump_shutdown "omrdump_shutdown" */
	void (*dump_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref j9nls.c::j9nls_startup "j9nls_startup" Deprecated*/
	int32_t (*nls_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref j9nls.c::j9nls_free_cached_data "j9nls_free_cached_data" Deprecated*/
	void (*nls_free_cached_data)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref j9nls.c::j9nls_shutdown "j9nls_shutdown" Deprecated*/
	void (*nls_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref j9nls.c::j9nls_set_catalog "j9nls_set_catalog" Deprecated*/
	void (*nls_set_catalog)(struct OMRPortLibrary *portLibrary, const char **paths, const int nPaths, const char *baseName, const char *extension) ;
	/** see @ref j9nls.c::j9nls_set_locale "j9nls_set_locale" Deprecated*/
	void (*nls_set_locale)(struct OMRPortLibrary *portLibrary, const char *lang, const char *region, const char *variant) ;
	/** see @ref j9nls.c::j9nls_get_language "j9nls_get_language" Deprecated*/
	const char *(*nls_get_language)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref j9nls.c::j9nls_get_region "j9nls_get_region" Deprecated*/
	const char *(*nls_get_region)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref j9nls.c::j9nls_get_variant "j9nls_get_variant" Deprecated*/
	const char *(*nls_get_variant)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref j9nls.c::j9nls_printf "j9nls_printf" Deprecated*/
	void (*nls_printf)(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, ...) ;
	/** see @ref j9nls.c::j9nls_vprintf "j9nls_vprintf" Deprecated*/
	void (*nls_vprintf)(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, va_list args) ;
	/** see @ref j9nls.c::j9nls_lookup_message "j9nls_lookup_message" Deprecated*/
	const char *(*nls_lookup_message)(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, const char *default_string) ;
	/** see @ref omrportcontrol.c::omrport_control "omrport_control"*/
	int32_t (*port_control)(struct OMRPortLibrary *portLibrary, const char *key, uintptr_t value) ;
	/** see @ref omrsignal.c::omrsig_startup "omrsig_startup"*/
	int32_t (*sig_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsignal.c::omrsig_shutdown "omrsig_shutdown"*/
	void (*sig_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsignal.c::omrsig_protect "omrsig_protect"*/
	int32_t (*sig_protect)(struct OMRPortLibrary *portLibrary,  omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result) ;
	/** see @ref omrsignal.c::omrsig_can_protect "omrsig_can_protect"*/
	int32_t (*sig_can_protect)(struct OMRPortLibrary *portLibrary,  uint32_t flags) ;
	/** see @ref omrsignal.c::omrsig_set_async_signal_handler "omrsig_set_async_signal_handler"*/
	int32_t (*sig_set_async_signal_handler)(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags) ;
	/** see @ref omrsignal.c::omrsig_set_single_async_signal_handler "omrsig_set_single_async_signal_handler"*/
	int32_t (*sig_set_single_async_signal_handler)(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t portlibSignalFlag, void **oldOSHandler) ;
	/** see @ref omrsignal.c::omrsig_map_os_signal_to_portlib_signal "omrsig_map_os_signal_to_portlib_signal"*/
	uint32_t (*sig_map_os_signal_to_portlib_signal)(struct OMRPortLibrary *portLibrary, uint32_t osSignalValue) ;
	/** see @ref omrsignal.c::omrsig_map_portlib_signal_to_os_signal "omrsig_map_portlib_signal_to_os_signal"*/
	int32_t (*sig_map_portlib_signal_to_os_signal)(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag) ;
	/** see @ref omrsignal.c::omrsig_register_os_handler "omrsig_register_os_handler"*/
	int32_t (*sig_register_os_handler)(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, void *newOSHandler, void **oldOSHandler) ;
	/** see @ref omrsignal.c::omrsig_is_main_signal_handler "omrsig_is_main_signal_handler"*/
	BOOLEAN (*sig_is_main_signal_handler)(struct OMRPortLibrary *portLibrary, void *osHandler) ;
	/** see @ref omrsignal.c::omrsig_is_signal_ignored "omrsig_is_signal_ignored"*/
	int32_t (*sig_is_signal_ignored)(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, BOOLEAN *isSignalIgnored) ;
	/** see @ref omrsignal.c::omrsig_info "omrsig_info"*/
	uint32_t (*sig_info)(struct OMRPortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value) ;
	/** see @ref omrsignal.c::omrsig_info_count "omrsig_info_count"*/
	uint32_t (*sig_info_count)(struct OMRPortLibrary *portLibrary, void *info, uint32_t category) ;
	/** see @ref omrsignal.c::omrsig_set_options "omrsig_set_options"*/
	int32_t (*sig_set_options)(struct OMRPortLibrary *portLibrary, uint32_t options) ;
	/** see @ref omrsignal.c::omrsig_get_options "omrsig_get_options"*/
	uint32_t (*sig_get_options)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsignal.c::omrsig_get_current_signal "omrsig_get_current_signal"*/
	intptr_t (*sig_get_current_signal)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsignal.c::omrsig_set_reporter_priority "omrsig_set_reporter_priority"*/
	int32_t (*sig_set_reporter_priority)(struct OMRPortLibrary *portLibrary, uintptr_t priority) ;
	/** see @ref omrfile.c::omrfile_read_text "omrfile_read_text"*/
	char  *(*file_read_text)(struct OMRPortLibrary *portLibrary, intptr_t fd, char *buf, intptr_t nbytes) ;
	/** see @ref omrfile.c::omrfile_mkdir "omrfile_mkdir"*/
	int32_t (*file_mkdir)(struct OMRPortLibrary *portLibrary, const char *path) ;
	/** see @ref omrfile.c::omrfile_move "omrfile_move"*/
	int32_t (*file_move)(struct OMRPortLibrary *portLibrary, const char *pathExist, const char *pathNew) ;
	/** see @ref omrfile.c::omrfile_unlinkdir "omrfile_unlinkdir"*/
	int32_t (*file_unlinkdir)(struct OMRPortLibrary *portLibrary, const char *path) ;
	/** see @ref omrfile.c::omrfile_findfirst "omrfile_findfirst"*/
	uintptr_t (*file_findfirst)(struct OMRPortLibrary *portLibrary, const char *path, char *resultbuf) ;
	/** see @ref omrfile.c::omrfile_findnext "omrfile_findnext"*/
	int32_t (*file_findnext)(struct OMRPortLibrary *portLibrary, uintptr_t findhandle, char *resultbuf) ;
	/** see @ref omrfile.c::omrfile_findclose "omrfile_findclose"*/
	void (*file_findclose)(struct OMRPortLibrary *portLibrary, uintptr_t findhandle) ;
	/** see @ref omrfile.c::omrfile_error_message "omrfile_error_message"*/
	const char  *(*file_error_message)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrfile.c::omrfile_unlock_bytes "omrfile_unlock_bytes"*/
	int32_t (*file_unlock_bytes)(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length) ;
	/** see @ref omrfile.c::omrfile_lock_bytes "omrfile_lock_bytes"*/
	int32_t (*file_lock_bytes)(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length) ;
	/** see @ref omrfile.c::omrfile_convert_native_fd_to_omrfile_fd "omrfile_convert_native_fd_to_omrfile_fd"*/
	intptr_t (*file_convert_native_fd_to_omrfile_fd)(struct OMRPortLibrary *portLibrary, intptr_t nativeFD) ;
	/** see @ref omrfile.c::omrfile_convert_omrfile_fd_to_native_fd "omrfile_convert_omrfile_fd_to_native_fd"*/
	intptr_t (*file_convert_omrfile_fd_to_native_fd)(struct OMRPortLibrary *portLibrary, intptr_t omrfileFD) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_unlock_bytes "omrfile_blockingasync_unlock_bytes"*/
	int32_t (*file_blockingasync_unlock_bytes)(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length) ;
	/** see @ref omrfile_blockingasync.c::omrfile_blockingasync_lock_bytes "omrfile_blockingasync_lock_bytes"*/
	int32_t (*file_blockingasync_lock_bytes)(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length) ;
	/** see @ref omrstr.c::omrstr_ftime "omrstr_ftime"*/
	uintptr_t (*str_ftime)(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, int64_t timeMillis) ;
	/** see @ref omrstr.c::omrstr_ftime_ex "omrstr_ftime_ex"*/
	uintptr_t (*str_ftime_ex)(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, int64_t timeMillis, uint32_t flags);
	/** see @ref omrstr.c::omrstr_current_time_zone "omrstr_current_time_zone"*/
	int32_t (*str_current_time_zone)(struct OMRPortLibrary *portLibrary, int32_t *secondsEast, char *zoneNameBuffer, size_t zoneNameBufferLen) ;
	/** see @ref omrmmap.c::omrmmap_startup "omrmmap_startup"*/
	int32_t (*mmap_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrmmap.c::omrmmap_shutdown "omrmmap_shutdown"*/
	void (*mmap_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrmmap.c::omrmmap_capabilities "omrmmap_capabilities"*/
	int32_t (*mmap_capabilities)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrmmap.c::omrmmap_map_file "omrmmap_map_file"*/
	struct J9MmapHandle *(*mmap_map_file)(struct OMRPortLibrary *portLibrary, intptr_t file, uint64_t offset, uintptr_t size, const char *mappingName, uint32_t flags,  uint32_t category) ;
	/** see @ref omrmmap.c::omrmmap_unmap_file "omrmmap_unmap_file"*/
	void (*mmap_unmap_file)(struct OMRPortLibrary *portLibrary, J9MmapHandle *handle) ;
	/** see @ref omrmmap.c::omrmmap_msync "omrmmap_msync"*/
	intptr_t (*mmap_msync)(struct OMRPortLibrary *portLibrary, void *start, uintptr_t length, uint32_t flags) ;
	/** see @ref omrmmap.c::omrmmap_protect "omrmmap_protect"*/
	intptr_t (*mmap_protect)(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags) ;
	/** see @ref omrmmap.c::omrmmap_get_region_granularity "omrmmap_get_region_granularity"*/
	uintptr_t (*mmap_get_region_granularity)(struct OMRPortLibrary *portLibrary, void *address) ;
	/** see @ref omrmmap.c::omrmmap_dont_need "omrmmap_dont_need"*/
	void (*mmap_dont_need)(struct OMRPortLibrary *portLibrary, const void *startAddress, size_t length) ;
#if !defined(OMR_OS_WINDOWS)
	/** see @ref omrshsem.c::omrshsem_params_init "omrshsem_params_init"*/
	int32_t  ( *shsem_params_init)(struct OMRPortLibrary *portLibrary, struct OMRPortShSemParameters *params) ;
	/** see @ref omrshsem.c::omrshsem_startup "omrshsem_startup"*/
	int32_t  ( *shsem_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrshsem.c::omrshsem_shutdown "omrshsem_shutdown"*/
	void  ( *shsem_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrshsem.c::omrshsem_open "omrshsem_open"*/
	intptr_t  ( *shsem_open)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle, const struct OMRPortShSemParameters *params) ;
	/** see @ref omrshsem.c::omrshsem_post "omrshsem_post"*/
	intptr_t  ( *shsem_post)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref omrshsem.c::omrshsem_wait "omrshsem_wait"*/
	intptr_t  ( *shsem_wait)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref omrshsem.c::omrshsem_getVal "omrshsem_getVal"*/
	intptr_t  ( *shsem_getVal)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset) ;
	/** see @ref omrshsem.c::omrshsem_setVal "omrshsem_setVal"*/
	intptr_t  ( *shsem_setVal)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, intptr_t value) ;
	/** see @ref omrshsem.c::omrshsem_close "omrshsem_close"*/
	void  ( *shsem_close)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle) ;
	/** see @ref omrshsem.c::omrshsem_destroy "omrshsem_destroy"*/
	intptr_t  ( *shsem_destroy)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_startup "omrshsem_deprecated_startup"*/
	int32_t  ( *shsem_deprecated_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_shutdown "omrshsem_deprecated_shutdown"*/
	void  ( *shsem_deprecated_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_open "omrshsem_deprecated_open"*/
	intptr_t  ( *shsem_deprecated_open)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshsem_handle** handle, const char* semname, int setSize, int permission, uintptr_t flags, struct OMRControlFileStatus *controlFileStatus) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_openDeprecated "omrshsem_deprecated_openDeprecated"*/
	intptr_t  ( *shsem_deprecated_openDeprecated)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshsem_handle** handle, const char* semname, uintptr_t cacheFileType) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_post "omrshsem_deprecated_post"*/
	intptr_t  ( *shsem_deprecated_post)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_wait "omrshsem_deprecated_wait"*/
	intptr_t  ( *shsem_deprecated_wait)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, uintptr_t flag) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_getVal "omrshsem_deprecated_getVal"*/
	intptr_t  ( *shsem_deprecated_getVal)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_setVal "omrshsem_deprecated_setVal"*/
	intptr_t  ( *shsem_deprecated_setVal)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle, uintptr_t semset, intptr_t value) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_handle_stat "omrshsem_deprecated_handle_stat"*/
	intptr_t  ( *shsem_deprecated_handle_stat)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle *handle, struct OMRPortShsemStatistic *statbuf);
	/** see @ref omrshsem.c::omrshsem_deprecated_close "omrshsem_deprecated_close"*/
	void  ( *shsem_deprecated_close)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_destroy "omrshsem_deprecated_destroy"*/
	intptr_t  ( *shsem_deprecated_destroy)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_destroyDeprecated "omrshsem_deprecated_destroyDeprecated"*/
	intptr_t  ( *shsem_deprecated_destroyDeprecated)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle **handle, uintptr_t cacheFileType) ;
	/** see @ref omrshsem.c::omrshsem_deprecated_getid "omrshsem_deprecated_getid"*/
	int32_t  ( *shsem_deprecated_getid)(struct OMRPortLibrary *portLibrary, struct omrshsem_handle* handle) ;
	/** see @ref omrshmem.c::omrshmem_startup "omrshmem_startup"*/
	int32_t  ( *shmem_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrshmem.c::omrshmem_shutdown "omrshmem_shutdown"*/
	void  ( *shmem_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrshmem.c::omrshmem_open "omrshmem_open"*/
	intptr_t  ( *shmem_open)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle, const char* rootname, uintptr_t size, uint32_t perm, uint32_t category, uintptr_t flags, struct OMRControlFileStatus *controlFileStatus) ;
	/** see @ref omrshmem.c::omrshmem_openDeprecated "omrshmem_openDeprecated"*/
	intptr_t  ( *shmem_openDeprecated)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle, const char* rootname, uint32_t perm, uintptr_t cacheFileType, uint32_t category) ;
	/** see @ref omrshmem.c::omrshmem_attach "omrshmem_attach"*/
	void*  ( *shmem_attach)(struct OMRPortLibrary *portLibrary, struct omrshmem_handle* handle,  uint32_t category) ;
	/** see @ref omrshmem.c::omrshmem_detach "omrshmem_detach"*/
	intptr_t  ( *shmem_detach)(struct OMRPortLibrary *portLibrary, struct omrshmem_handle **handle) ;
	/** see @ref omrshmem.c::omrshmem_close "omrshmem_close"*/
	void  ( *shmem_close)(struct OMRPortLibrary *portLibrary, struct omrshmem_handle **handle) ;
	/** see @ref omrshmem.c::omrshmem_destroy "omrshmem_destroy"*/
	intptr_t  ( *shmem_destroy)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle) ;
	/** see @ref omrshmem.c::omrshmem_destroyDeprecated "omrshmem_destroyDeprecated"*/
	intptr_t  ( *shmem_destroyDeprecated)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, struct omrshmem_handle **handle, uintptr_t cacheFileType) ;
	/** see @ref omrshmem.c::omrshmem_findfirst "omrshmem_findfirst"*/
	uintptr_t  ( *shmem_findfirst)(struct OMRPortLibrary *portLibrary, char *cacheDirName, char *resultbuf) ;
	/** see @ref omrshmem.c::omrshmem_findnext "omrshmem_findnext"*/
	int32_t  ( *shmem_findnext)(struct OMRPortLibrary *portLibrary, uintptr_t findhandle, char *resultbuf) ;
	/** see @ref omrshmem.c::omrshmem_findclose "omrshmem_findclose"*/
	void  ( *shmem_findclose)(struct OMRPortLibrary *portLibrary, uintptr_t findhandle) ;
	/** see @ref omrshmem.c::omrshmem_stat "omrshmem_stat"*/
	uintptr_t  ( *shmem_stat)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct OMRPortShmemStatistic* statbuf) ;
	/** see @ref omrshmem.c::omrshmem_statDeprecated "omrshmem_statDeprecated"*/
	uintptr_t  ( *shmem_statDeprecated)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, const char* name, struct OMRPortShmemStatistic* statbuf, uintptr_t cacheFileType) ;
	/** see @ref omrshmem.c::omrshmem_handle_stat "omrshmem_handle_stat"*/
	intptr_t  ( *shmem_handle_stat)(struct OMRPortLibrary *portLibrary, struct omrshmem_handle *handle, struct OMRPortShmemStatistic *statbuf);
	/** see @ref omrshmem.c::omrshmem_getDir "omrshmem_getDir"*/
	intptr_t  ( *shmem_getDir)(struct OMRPortLibrary* portLibrary, const char* ctrlDirName, uint32_t flags, char* buffer, uintptr_t length) ;
	/** see @ref omrshmem.c::omrshmem_createDir "omrshmem_createDir"*/
	intptr_t  ( *shmem_createDir)(struct OMRPortLibrary *portLibrary, char* cacheDirName, uintptr_t cacheDirPerm, BOOLEAN cleanMemorySegments) ;
	/** see @ref omrshmem.c::omrshmem_getFilepath "omrshmem_getFilepath"*/
	intptr_t  ( *shmem_getFilepath)(struct OMRPortLibrary* portLibrary, char* cacheDirName, char* buffer, uintptr_t length, const char* cachename) ;
	/** see @ref omrshmem.c::omrshmem_protect "omrshmem_protect"*/
	intptr_t  ( *shmem_protect)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void* address, uintptr_t length, uintptr_t flags) ;
	/** see @ref omrshmem.c::omrshmem_get_region_granularity "omrshmem_get_region_granularity"*/
	uintptr_t  ( *shmem_get_region_granularity)(struct OMRPortLibrary *portLibrary, const char* cacheDirName, uintptr_t groupPerm, void *address) ;
	/** see @ref omrshmem.c::omrshmem_getid "omrshmem_getid"*/
	int32_t  ( *shmem_getid)(struct OMRPortLibrary *portLibrary, struct omrshmem_handle* handle);
#endif /* !defined(OMR_OS_WINDOWS) */
	/** see @ref omrsysinfo.c::omrsysinfo_get_limit "omrsysinfo_get_limit"*/
	uint32_t (*sysinfo_get_limit)(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t *limit) ;
	/** see @ref omrsysinfo.c::omrsysinfo_set_limit "omrsysinfo_set_limit"*/
	uint32_t (*sysinfo_set_limit)(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t limit) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_number_CPUs_by_type "omrsysinfo_get_number_CPUs_by_type"*/
	uintptr_t (*sysinfo_get_number_CPUs_by_type)(struct OMRPortLibrary *portLibrary, uintptr_t type) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_cwd "omrsysinfo_get_cwd"*/
	intptr_t (*sysinfo_get_cwd)(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_tmp "omrsysinfo_get_tmp"*/
	intptr_t (*sysinfo_get_tmp)(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, BOOLEAN ignoreEnvVariable) ;
	/** see @ref omrsysinfo.c::omrsysinfo_set_number_user_specified_CPUs "omrsysinfo_set_number_user_specified_CPUs"*/
	void (*sysinfo_set_number_user_specified_CPUs)(struct OMRPortLibrary *portLibrary, uintptr_t number) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_open_file_count "omrsysinfo_get_open_file_count"*/
	int32_t (*sysinfo_get_open_file_count)(struct OMRPortLibrary *portLibrary, uint64_t *count) ;
	/** see @ref omrsysinfo.c::omrsysinfo_get_os_description "omrsysinfo_get_os_description"*/
	intptr_t  ( *sysinfo_get_os_description)(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc) ;
	/** see @ref omrsysinfo.c::omrsysinfo_os_has_feature "omrsysinfo_os_has_feature"*/
	BOOLEAN  ( *sysinfo_os_has_feature)(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc, uint32_t feature) ;
	/** see @ref omrsysinfo.c::omrsysinfo_os_kernel_info "omrsysinfo_os_kernel_info"*/
	BOOLEAN  ( *sysinfo_os_kernel_info)(struct OMRPortLibrary *portLibrary, struct OMROSKernelInfo *kernelInfo) ;
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_is_system_available "omrsysinfo_cgroup_is_system_available"*/
	BOOLEAN  ( *sysinfo_cgroup_is_system_available)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_get_available_subsystems "omrsysinfo_cgroup_get_available_subsystems"*/
	uint64_t ( *sysinfo_cgroup_get_available_subsystems)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_are_subsystems_available "omrsysinfo_cgroup_are_subsystems_available"*/
	uint64_t ( *sysinfo_cgroup_are_subsystems_available)(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlags);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_get_enabled_subsystems "omrsysinfo_cgroup_get_enabled_subsystems"*/
	uint64_t ( *sysinfo_cgroup_get_enabled_subsystems)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrsysinfo_cgroup_enable_subsystems "omrsysinfo_cgroup_enable_subsystems"*/
	uint64_t ( *sysinfo_cgroup_enable_subsystems)(struct OMRPortLibrary *portLibrary, uint64_t requestedSubsystems);
	/** see @ref omrsysinfo_cgroup_are_subsystems_enabled "omrsysinfo_cgroup_are_subsystems_enabled"*/
	uint64_t ( *sysinfo_cgroup_are_subsystems_enabled)(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlags);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_get_memlimit "omrsysinfo_cgroup_get_memlimit"*/
	int32_t (*sysinfo_cgroup_get_memlimit)(struct OMRPortLibrary *portLibrary, uint64_t *limit);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_is_memlimit_set "omrsysinfo_cgroup_is_memlimit_set"*/
	BOOLEAN (*sysinfo_cgroup_is_memlimit_set)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrsysinfo.c::omrsysinfo_get_cgroup_subsystem_list "omrsysinfo_get_cgroup_subsystem_list"*/
	struct OMRCgroupEntry *(*sysinfo_get_cgroup_subsystem_list)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrsysinfo.c::omrsysinfo_is_running_in_container "omrsysinfo_is_running_in_container"*/
	BOOLEAN (*sysinfo_is_running_in_container)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_subsystem_iterator_init "omrsysinfo_cgroup_subsystem_iterator_init"*/
	int32_t (*sysinfo_cgroup_subsystem_iterator_init)(struct OMRPortLibrary *portLibrary, uint64_t subsystem, struct OMRCgroupMetricIteratorState *state);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_subsystem_iterator_hasNext "omrsysinfo_cgroup_subsystem_iterator_hasNext"*/
	BOOLEAN (*sysinfo_cgroup_subsystem_iterator_hasNext)(struct OMRPortLibrary *portLibrary, const struct OMRCgroupMetricIteratorState *state);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_subsystem_iterator_metricKey "omrsysinfo_cgroup_subsystem_iterator_metricKey"*/
	int32_t (*sysinfo_cgroup_subsystem_iterator_metricKey)(struct OMRPortLibrary *portLibrary, const struct OMRCgroupMetricIteratorState *state, const char **metricKey);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_subsystem_iterator_next "omrsysinfo_cgroup_subsystem_iterator_next"*/
	int32_t (*sysinfo_cgroup_subsystem_iterator_next)(struct OMRPortLibrary *portLibrary, struct OMRCgroupMetricIteratorState *state, struct OMRCgroupMetricElement *metricElement);
	/** see @ref omrsysinfo.c::omrsysinfo_cgroup_subsystem_iterator_destroy "omrsysinfo_cgroup_subsystem_iterator_destroy"*/
	void (*sysinfo_cgroup_subsystem_iterator_destroy)(struct OMRPortLibrary *portLibrary, struct OMRCgroupMetricIteratorState *state);
	/** see @ref omrsysinfo.c::omrsysinfo_get_process_start_time "omrsysinfo_get_process_start_time"*/
	int32_t (*sysinfo_get_process_start_time)(struct OMRPortLibrary *portLibrary, uintptr_t pid, uint64_t *processStartTimeInNanoseconds);
	/** see @ref omrport.c::omrport_init_library "omrport_init_library"*/
	int32_t (*port_init_library)(struct OMRPortLibrary *portLibrary, uintptr_t size) ;
	/** see @ref omrport.c::omrport_startup_library "omrport_startup_library"*/
	int32_t (*port_startup_library)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrport.c::omrport_create_library "omrport_create_library"*/
	int32_t (*port_create_library)(struct OMRPortLibrary *portLibrary, uintptr_t size) ;
	/** see @ref omrsyslog.c::omrsyslog_write "omrsyslog_write"*/
	uintptr_t (*syslog_write)(struct OMRPortLibrary *portLibrary, uintptr_t flags, const char *message) ;
	/** see @ref omrintrospect.c::omrintrospect_startup "omrintrospect_startup"*/
	int32_t (*introspect_startup)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrintrospect.c::omrintrospect_shutdown "omrintrospect_shutdown"*/
	void (*introspect_shutdown)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrintrospect.c::omrintrospect_set_suspend_signal_offset "omrintrospect_set_suspend_signal_offset"*/
	int32_t (*introspect_set_suspend_signal_offset)(struct OMRPortLibrary *portLibrary, int32_t signalOffset);
	/** see @ref omrintrospect.c::omrintrospect_threads_startDo "omrintrospect_threads_startDo"*/
	struct J9PlatformThread  *(*introspect_threads_startDo)(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state) ;
	/** see @ref omrintrospect.c::omrintrospect_threads_startDo_with_signal "omrintrospect_threads_startDo_with_signal"*/
	struct J9PlatformThread  *(*introspect_threads_startDo_with_signal)(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state, void *signal_info) ;
	/** see @ref omrintrospect.c::omrintrospect_threads_nextDo "omrintrospect_threads_nextDo"*/
	struct J9PlatformThread  *(*introspect_threads_nextDo)(J9ThreadWalkState *state) ;
	/** see @ref omrintrospect.c::omrintrospect_backtrace_thread "omrintrospect_backtrace_thread"*/
	uintptr_t (*introspect_backtrace_thread)(struct OMRPortLibrary *portLibrary, J9PlatformThread *thread, J9Heap *heap, void *signalInfo) ;
	/** see @ref omrintrospect.c::omrintrospect_backtrace_symbols "omrintrospect_backtrace_symbols"*/
	uintptr_t (*introspect_backtrace_symbols)(struct OMRPortLibrary *portLibrary, J9PlatformThread *thread, J9Heap *heap) ;
	/** see @ref omrintrospect.c::omrintrospect_backtrace_symbols_ex "omrintrospect_backtrace_symbols_ex"*/
	uintptr_t (*introspect_backtrace_symbols_ex)(struct OMRPortLibrary *portLibrary, J9PlatformThread *thread, J9Heap *heap, uint32_t options);
	/** see @ref omrsyslog.c::omrsyslog_query "omrsyslog_query"*/
	uintptr_t (*syslog_query)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsyslog.c::omrsyslog_set "omrsyslog_set"*/
	void (*syslog_set)(struct OMRPortLibrary *portLibrary, uintptr_t options) ;
	/** see @ref omrmem.c::omrmem_walk_categories "omrmem_walk_categories"*/
	void (*mem_walk_categories)(struct OMRPortLibrary *portLibrary, OMRMemCategoryWalkState *state) ;
	/** see @ref omrmemcategories.c::omrmem_get_category "omrmem_get_category"*/
	OMRMemCategory *(*mem_get_category)(struct OMRPortLibrary *portLibrary, uint32_t categoryCode);
	/** see @ref omrmem32helpers.c::omrmem_categories_increment_counters "omrmem_categories_increment_counters"*/
	void (*mem_categories_increment_counters)(OMRMemCategory *category, uintptr_t size);
	/** see @ref omrmem32helpers.c::omrmem_categories_decrement_counters "omrmem_categories_decrement_counters"*/
	void (*mem_categories_decrement_counters)(OMRMemCategory *category, uintptr_t size);
	/** see @ref omrheap.c::omrheap_query_size "omrheap_query_size"*/
	uintptr_t (*heap_query_size)(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address) ;
	/** see @ref omrheap.c::omrheap_grow "omrheap_grow"*/
	BOOLEAN (*heap_grow)(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, uintptr_t growAmount) ;
	/** see @ref omrsock.c::omrsock_startup "omrsock_startup"*/
	int32_t (*sock_startup)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsock.c::omrsock_getaddrinfo_create_hints "omrsock_getaddrinfo_create_hints"*/
	int32_t (*sock_getaddrinfo_create_hints)(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t *hints, int32_t family, int32_t socktype, int32_t protocol, int32_t flags) ;
	/** see @ref omrsock.c::omrsock_getaddrinfo "omrsock_getaddrinfo"*/
	int32_t (*sock_getaddrinfo)(struct OMRPortLibrary *portLibrary, const char *node, const char *service, omrsock_addrinfo_t hints, omrsock_addrinfo_t result) ;
	/** see @ref omrsock.c::omrsock_addrinfo_length "omrsock_addrinfo_length"*/
	int32_t (*sock_addrinfo_length)(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t *result) ;
	/** see @ref omrsock.c::omrsock_addrinfo_family "omrsock_addrinfo_family"*/
	int32_t (*sock_addrinfo_family)(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, int32_t *result) ;
	/** see @ref omrsock.c::omrsock_addrinfo_socktype "omrsock_addrinfo_socktype"*/
	int32_t (*sock_addrinfo_socktype)(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, int32_t *result) ;
	/** see @ref omrsock.c::omrsock_addrinfo_protocol "omrsock_addrinfo_protocol"*/
	int32_t (*sock_addrinfo_protocol)(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, int32_t *result) ;
	/** see @ref omrsock.c::omrsock_addrinfo_address "omrsock_addrinfo_address"*/
	int32_t (*sock_addrinfo_address)(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, omrsock_sockaddr_t result) ;
	/** see @ref omrsock.c::omrsock_freeaddrinfo "omrsock_freeaddrinfo"*/
	int32_t (*sock_freeaddrinfo)(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle) ;
	/** see @ref omrsock.c::omrsock_sockaddr_init "omrsock_sockaddr_init"*/
	int32_t (*sock_sockaddr_init)(struct OMRPortLibrary *portLibrary, omrsock_sockaddr_t handle, int32_t family, uint8_t *addrNetworkOrder, uint16_t portNetworkOrder) ;
	/** see @ref omrsock.c::omrsock_sockaddr_init6 "omrsock_sockaddr_init6"*/
	int32_t (*sock_sockaddr_init6)(struct OMRPortLibrary *portLibrary, omrsock_sockaddr_t handle, int32_t family, uint8_t *addrNetworkOrder, uint16_t portNetworkOrder, uint32_t flowinfo, uint32_t scope_id) ;
	/** see @ref omrsock.c::omrsock_socket_getfd "omrsock_socket_getfd"*/
	int32_t (*sock_socket_getfd)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock) ;
	/** see @ref omrsock.c::omrsock_socket "omrsock_socket"*/
	int32_t (*sock_socket)(struct OMRPortLibrary *portLibrary, omrsock_socket_t *sock, int32_t family, int32_t socktype, int32_t protocol) ;
	/** see @ref omrsock.c::omrsock_bind "omrsock_bind"*/
	int32_t (*sock_bind)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr) ;
	/** see @ref omrsock.c::omrsock_listen "omrsock_listen"*/
	int32_t (*sock_listen)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t backlog) ;
	/** see @ref omrsock.c::omrsock_connect "omrsock_connect"*/
	int32_t (*sock_connect)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr) ;
	/** see @ref omrsock.c::omrsock_accept "omrsock_accept"*/
	int32_t (*sock_accept)(struct OMRPortLibrary *portLibrary, omrsock_socket_t serverSock, omrsock_sockaddr_t addrHandle, omrsock_socket_t *sockHandle) ;
	/** see @ref omrsock.c::omrsock_send "omrsock_send"*/
	int32_t (*sock_send)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags) ;
	/** see @ref omrsock.c::omrsock_sendto "omrsock_sendto"*/
	int32_t (*sock_sendto)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle) ;
	/** see @ref omrsock.c::omrsock_recv "omrsock_recv"*/
	int32_t (*sock_recv)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags) ;
	/** see @ref omrsock.c::omrsock_recvfrom "omrsock_recvfrom"*/
	int32_t (*sock_recvfrom)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle) ;
	/** see @ref omrsock.c::omrsock_pollfd_init "omrsock_pollfd_init"*/
	int32_t (*sock_pollfd_init)(struct OMRPortLibrary *portLibrary, omrsock_pollfd_t handle, omrsock_socket_t sock, int16_t events) ;
	/** see @ref omrsock.c::omrsock_pollpd_info "omrsock_pollfd_info"*/
	int32_t (*sock_get_pollfd_info)(struct OMRPortLibrary *portLibrary, omrsock_pollfd_t handle, omrsock_socket_t *sock, int16_t *revents) ;
	/** see @ref omrsock.c::omrsock_poll "omrsock_poll"*/
	int32_t (*sock_poll)(struct OMRPortLibrary *portLibrary, omrsock_pollfd_t fds, uint32_t nfds, int32_t timeoutMs) ;
	/** see @ref omrsock.c::omrsock_fdset_zero "omrsock_fdset_zero"*/
	void (*sock_fdset_zero)(struct OMRPortLibrary *portLibrary, omrsock_fdset_t fdset) ;
	/** see @ref omrsock.c::omrsock_fdset_set "omrsock_fdset_set"*/
	void (*sock_fdset_set)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_fdset_t fdset) ;
	/** see @ref omrsock.c::omrsock_fdset_clr "omrsock_fdset_clr"*/
	void (*sock_fdset_clr)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_fdset_t fdset) ;
	/** see @ref omrsock.c::omrsock_fdset_isset "omrsock_fdset_isset"*/
	BOOLEAN (*sock_fdset_isset)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_fdset_t fdset) ;
	/** see @ref omrsock.c::omrsock_select "omrsock_select"*/
	int32_t (*sock_select)(struct OMRPortLibrary *portLibrary, omrsock_fdset_t readfds, omrsock_fdset_t writefds, omrsock_fdset_t exceptfds, omrsock_timeval_t timeout) ;
	/** see @ref omrsock.c::omrsock_close "omrsock_close"*/
	int32_t (*sock_close)(struct OMRPortLibrary *portLibrary, omrsock_socket_t *sock) ;
	/** see @ref omrsock.c::omrsock_shutdown "omrsock_shutdown"*/
	int32_t (*sock_shutdown)(struct OMRPortLibrary *portLibrary) ;
	/** see @ref omrsock.c::omrsock_htons "omrsock_htons"*/
	uint16_t (*sock_htons)(struct OMRPortLibrary *portLibrary, uint16_t val) ;
	/** see @ref omrsock.c::omrsock_htonl "omrsock_htonl"*/
	uint32_t (*sock_htonl)(struct OMRPortLibrary *portLibrary, uint32_t val) ;
	/** see @ref omrsock.c::omrsock_inet_pton "omrsock_inet_pton"*/
	int32_t (*sock_inet_pton)(struct OMRPortLibrary *portLibrary, int32_t addrFamily, const char *addr, uint8_t *result) ;
	/** see @ref omrsock.c::omrsock_fcntl "omrsock_fcntl"*/
	int32_t (*sock_fcntl)(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t arg) ;
	/** see @ref omrsock.c::omrsock_timeval_init "omrsock_timeval_init"*/
	int32_t (*sock_timeval_init)(struct OMRPortLibrary *portLibrary, omrsock_timeval_t handle, uint32_t secTime, uint32_t uSecTime) ;
	/** see @ref omrsock.c::omrsock_linger_init "omrsock_linger_init"*/
	int32_t (*sock_linger_init)(struct OMRPortLibrary *portLibrary, omrsock_linger_t handle, int32_t enabled, uint16_t timeout) ;
	/** see @ref omrsock.c::omrsock_setsockopt_int "omrsock_setsockopt_int"*/
	int32_t (*sock_setsockopt_int)(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, int32_t *optval) ;
	/** see @ref omrsock.c::omrsock_setsockopt_linger "omrsock_setsockopt_linger"*/
	int32_t (*sock_setsockopt_linger)(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_linger_t optval) ;
	/** see @ref omrsock.c::omrsock_setsockopt_timeval "omrsock_setsockopt_timeval"*/
	int32_t (*sock_setsockopt_timeval)(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_timeval_t optval) ;
	/** see @ref omrsock.c::omrsock_getsockopt_int "omrsock_getsockopt_int"*/
	int32_t (*sock_getsockopt_int)(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, int32_t *optval) ;
	/** see @ref omrsock.c::omrsock_getsockopt_linger "omrsock_getsockopt_linger"*/
	int32_t (*sock_getsockopt_linger)(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_linger_t optval) ;
	/** see @ref omrsock.c::omrsock_getsockopt_timeval "omrsock_getsockopt_timeval"*/
	int32_t (*sock_getsockopt_timeval)(struct OMRPortLibrary *portLibrary, omrsock_socket_t handle, int32_t optlevel, int32_t optname, omrsock_timeval_t optval) ;
#if defined(OMR_OPT_CUDA)
	/** CUDA configuration data */
	J9CudaConfig *cuda_configData;
	/** see @ref omrcuda.cpp::omrcuda_startup "omrcuda_startup" */
	int32_t (*cuda_startup)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrcuda.cpp::omrcuda_shutdown "omrcuda_shutdown" */
	void (*cuda_shutdown)(struct OMRPortLibrary *portLibrary);
	/** see @ref omrcuda.cpp::omrcuda_deviceAlloc "omrcuda_deviceAlloc" */
	int32_t (*cuda_deviceAlloc)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uintptr_t size, void **deviceAddressOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceCanAccessPeer "omrcuda_deviceCanAccessPeer" */
	int32_t (*cuda_deviceCanAccessPeer)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId, BOOLEAN *canAccessPeerOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceDisablePeerAccess "omrcuda_deviceDisablePeerAccess" */
	int32_t (*cuda_deviceDisablePeerAccess)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId);
	/** see @ref omrcuda.cpp::omrcuda_deviceEnablePeerAccess "omrcuda_deviceEnablePeerAccess" */
	int32_t (*cuda_deviceEnablePeerAccess)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId);
	/** see @ref omrcuda.cpp::omrcuda_deviceFree "omrcuda_deviceFree" */
	int32_t (*cuda_deviceFree)(struct OMRPortLibrary *OMRPortLibrary, uint32_t deviceId, void *devicePointer);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetAttribute "omrcuda_deviceGetAttribute" */
	int32_t (*cuda_deviceGetAttribute)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceAttribute attribute, int32_t *valueOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetCacheConfig "omrcuda_deviceGetCacheConfig" */
	int32_t (*cuda_deviceGetCacheConfig)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaCacheConfig *configOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetCount "omrcuda_deviceGetCount" */
	int32_t (*cuda_deviceGetCount)(struct OMRPortLibrary *portLibrary, uint32_t *countOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetLimit "omrcuda_deviceGetLimit" */
	int32_t (*cuda_deviceGetLimit)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceLimit limit, uintptr_t *valueOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetMemInfo "omrcuda_deviceGetMemInfo" */
	int32_t (*cuda_deviceGetMemInfo)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uintptr_t *freeOut, uintptr_t *totalOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetName "omrcuda_deviceGetName" */
	int32_t (*cuda_deviceGetName)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t nameSize, char *nameOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetSharedMemConfig "omrcuda_deviceGetSharedMemConfig" */
	int32_t (*cuda_deviceGetSharedMemConfig)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaSharedMemConfig *configOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceGetStreamPriorityRange "omrcuda_deviceGetStreamPriorityRange" */
	int32_t (*cuda_deviceGetStreamPriorityRange)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, int32_t *leastPriorityOut, int32_t *greatestPriorityOut);
	/** see @ref omrcuda.cpp::omrcuda_deviceReset "omrcuda_deviceReset" */
	int32_t (*cuda_deviceReset)(struct OMRPortLibrary *portLibrary, uint32_t deviceId);
	/** see @ref omrcuda.cpp::omrcuda_deviceSetCacheConfig "omrcuda_deviceSetCacheConfig" */
	int32_t (*cuda_deviceSetCacheConfig)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaCacheConfig config);
	/** see @ref omrcuda.cpp::omrcuda_deviceSetLimit "omrcuda_deviceSetLimit" */
	int32_t (*cuda_deviceSetLimit)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceLimit limit, uintptr_t value);
	/** see @ref omrcuda.cpp::omrcuda_deviceSetSharedMemConfig "omrcuda_deviceSetSharedMemConfig" */
	int32_t (*cuda_deviceSetSharedMemConfig)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaSharedMemConfig config);
	/** see @ref omrcuda.cpp::omrcuda_deviceSynchronize "omrcuda_deviceSynchronize" */
	int32_t (*cuda_deviceSynchronize)(struct OMRPortLibrary *portLibrary, uint32_t deviceId);
	/** see @ref omrcuda.cpp::omrcuda_driverGetVersion "omrcuda_driverGetVersion" */
	int32_t (*cuda_driverGetVersion)(struct OMRPortLibrary *portLibrary, uint32_t *versionOut);
	/** see @ref omrcuda.cpp::omrcuda_eventCreate "omrcuda_eventCreate" */
	int32_t (*cuda_eventCreate)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t flags, J9CudaEvent *eventOut);
	/** see @ref omrcuda.cpp::omrcuda_eventDestroy "omrcuda_eventDestroy" */
	int32_t (*cuda_eventDestroy)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaEvent event);
	/** see @ref omrcuda.cpp::omrcuda_eventElapsedTime "omrcuda_eventElapsedTime" */
	int32_t (*cuda_eventElapsedTime)(struct OMRPortLibrary *portLibrary, J9CudaEvent start, J9CudaEvent end, float *elapsedMillisOut);
	/** see @ref omrcuda.cpp::omrcuda_eventQuery "omrcuda_eventQuery" */
	int32_t (*cuda_eventQuery)(struct OMRPortLibrary *portLibrary, J9CudaEvent event);
	/** see @ref omrcuda.cpp::omrcuda_eventRecord "omrcuda_eventRecord" */
	int32_t (*cuda_eventRecord)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaEvent event, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_eventSynchronize "omrcuda_eventSynchronize" */
	int32_t (*cuda_eventSynchronize)(struct OMRPortLibrary *portLibrary, J9CudaEvent event);
	/** see @ref omrcuda.cpp::omrcuda_funcGetAttribute "omrcuda_funcGetAttribute" */
	int32_t (*cuda_funcGetAttribute)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaFunctionAttribute attribute, int32_t *valueOut);
	/** see @ref omrcuda.cpp::omrcuda_funcMaxActiveBlocksPerMultiprocessor "omrcuda_funcMaxActiveBlocksPerMultiprocessor" */
	int32_t (*cuda_funcMaxActiveBlocksPerMultiprocessor)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, uint32_t blockSize, uint32_t dynamicSharedMemorySize, uint32_t flags, uint32_t *valueOut);
	/** see @ref omrcuda.cpp::omrcuda_funcMaxPotentialBlockSize "omrcuda_funcMaxPotentialBlockSize" */
	int32_t (*cuda_funcMaxPotentialBlockSize)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaBlockToDynamicSharedMemorySize dynamicSharedMemoryFunction, uintptr_t userData, uint32_t blockSizeLimit, uint32_t flags, uint32_t *minGridSizeOut, uint32_t *maxBlockSizeOut);
	/** see @ref omrcuda.cpp::omrcuda_funcSetCacheConfig "omrcuda_funcSetCacheConfig" */
	int32_t (*cuda_funcSetCacheConfig)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaCacheConfig config);
	/** see @ref omrcuda.cpp::omrcuda_funcSetSharedMemConfig "omrcuda_funcSetSharedMemConfig" */
	int32_t (*cuda_funcSetSharedMemConfig)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaSharedMemConfig config);
	/** see @ref omrcuda.cpp::omrcuda_getErrorString "omrcuda_getErrorString" */
	const char *(*cuda_getErrorString)(struct OMRPortLibrary *portLibrary, int32_t error);
	/** see @ref omrcuda.cpp::omrcuda_hostAlloc "omrcuda_hostAlloc" */
	int32_t (*cuda_hostAlloc)(struct OMRPortLibrary *portLibrary, uintptr_t size, uint32_t flags, void **hostAddressOut);
	/** see @ref omrcuda.cpp::omrcuda_hostFree "omrcuda_hostFree" */
	int32_t (*cuda_hostFree)(struct OMRPortLibrary *portLibrary, void *hostAddress);
	/** see @ref omrcuda.cpp::omrcuda_launchKernel "omrcuda_launchKernel" */
	int32_t (*cuda_launchKernel)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, uint32_t gridDimX, uint32_t gridDimY, uint32_t gridDimZ, uint32_t blockDimX, uint32_t blockDimY, uint32_t blockDimZ, uint32_t sharedMemBytes, J9CudaStream stream, void **kernelParms);
	/** see @ref omrcuda.cpp::omrcuda_linkerAddData "omrcuda_linkerAddData" */
	int32_t (*cuda_linkerAddData)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker, J9CudaJitInputType type, void *data, uintptr_t size, const char *name, J9CudaJitOptions *options);
	/** see @ref omrcuda.cpp::omrcuda_linkerComplete "omrcuda_linkerComplete" */
	int32_t (*cuda_linkerComplete)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker, void **cubinOut, uintptr_t *sizeOut);
	/** see @ref omrcuda.cpp::omrcuda_linkerCreate "omrcuda_linkerCreate" */
	int32_t (*cuda_linkerCreate)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaJitOptions *options, J9CudaLinker *linkerOut);
	/** see @ref omrcuda.cpp::omrcuda_linkerDestroy "omrcuda_linkerDestroy" */
	int32_t (*cuda_linkerDestroy)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker);
	/** see @ref omrcuda.cpp::omrcuda_memcpy2DDeviceToDevice "omrcuda_memcpy2DDeviceToDevice" */
	int32_t (*cuda_memcpy2DDeviceToDevice)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height);
	/** see @ref omrcuda.cpp::omrcuda_memcpy2DDeviceToDeviceAsync "omrcuda_memcpy2DDeviceToDeviceAsync" */
	int32_t (*cuda_memcpy2DDeviceToDeviceAsync)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memcpy2DDeviceToHost "omrcuda_memcpy2DDeviceToHost" */
	int32_t (*cuda_memcpy2DDeviceToHost)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height);
	/** see @ref omrcuda.cpp::omrcuda_memcpy2DDeviceToHostAsync "omrcuda_memcpy2DDeviceToHostAsync" */
	int32_t (*cuda_memcpy2DDeviceToHostAsync)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memcpy2DHostToDevice "omrcuda_memcpy2DHostToDevice" */
	int32_t (*cuda_memcpy2DHostToDevice)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height);
	/** see @ref omrcuda.cpp::omrcuda_memcpy2DHostToDeviceAsync "omrcuda_memcpy2DHostToDeviceAsync" */
	int32_t (*cuda_memcpy2DHostToDeviceAsync)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memcpyDeviceToDevice "omrcuda_memcpyDeviceToDevice" */
	int32_t (*cuda_memcpyDeviceToDevice)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount);
	/** see @ref omrcuda.cpp::omrcuda_memcpyDeviceToDeviceAsync "omrcuda_memcpyDeviceToDeviceAsync" */
	int32_t (*cuda_memcpyDeviceToDeviceAsync)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memcpyDeviceToHost "omrcuda_memcpyDeviceToHost" */
	int32_t (*cuda_memcpyDeviceToHost)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount);
	/** see @ref omrcuda.cpp::omrcuda_memcpyDeviceToHostAsync "omrcuda_memcpyDeviceToHostAsync" */
	int32_t (*cuda_memcpyDeviceToHostAsync)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memcpyHostToDevice "omrcuda_memcpyHostToDevice" */
	int32_t (*cuda_memcpyHostToDevice)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount);
	/** see @ref omrcuda.cpp::omrcuda_memcpyHostToDeviceAsync "omrcuda_memcpyHostToDeviceAsync" */
	int32_t (*cuda_memcpyHostToDeviceAsync)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memcpyPeer "omrcuda_memcpyPeer" */
	int32_t (*cuda_memcpyPeer)(struct OMRPortLibrary *portLibrary, uint32_t targetDeviceId, void *targetAddress, uint32_t sourceDeviceId, const void *sourceAddress, uintptr_t byteCount);
	/** see @ref omrcuda.cpp::omrcuda_memcpyPeerAsync "omrcuda_memcpyPeerAsync" */
	int32_t (*cuda_memcpyPeerAsync)(struct OMRPortLibrary *portLibrary, uint32_t targetDeviceId, void *targetAddress, uint32_t sourceDeviceId, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memset8Async "omrcuda_memset8Async" */
	int32_t (*cuda_memset8Async)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, uint8_t value, uintptr_t count, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memset16Async "omrcuda_memset16Async" */
	int32_t (*cuda_memset16Async)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, uint16_t value, uintptr_t count, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_memset32Async "omrcuda_memset32Async" */
	int32_t (*cuda_memset32Async)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, uint32_t value, uintptr_t count, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_moduleGetFunction "omrcuda_moduleGetFunction" */
	int32_t (*cuda_moduleGetFunction)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, J9CudaFunction *functionOut);
	/** see @ref omrcuda.cpp::omrcuda_moduleGetGlobal "omrcuda_moduleGetGlobal" */
	int32_t (*cuda_moduleGetGlobal)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *addressOut, uintptr_t *sizeOut);
	/** see @ref omrcuda.cpp::omrcuda_moduleGetSurfaceRef "omrcuda_moduleGetSurfaceRef" */
	int32_t (*cuda_moduleGetSurfaceRef)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *surfRefOut);
	/** see @ref omrcuda.cpp::omrcuda_moduleGetTextureRef "omrcuda_moduleGetTextureRef" */
	int32_t (*cuda_moduleGetTextureRef)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *texRefOut);
	/** see @ref omrcuda.cpp::omrcuda_moduleLoad "omrcuda_moduleLoad" */
	int32_t (*cuda_moduleLoad)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, const void *image, J9CudaJitOptions *options, J9CudaModule *moduleOut);
	/** see @ref omrcuda.cpp::omrcuda_moduleUnload "omrcuda_moduleUnload" */
	int32_t (*cuda_moduleUnload)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module);
	/** see @ref omrcuda.cpp::omrcuda_runtimeGetVersion "omrcuda_runtimeGetVersion" */
	int32_t (*cuda_runtimeGetVersion)(struct OMRPortLibrary *portLibrary, uint32_t *versionOut);
	/** see @ref omrcuda.cpp::omrcuda_streamAddCallback "omrcuda_streamAddCallback" */
	int32_t (*cuda_streamAddCallback)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, J9CudaStreamCallback callback, uintptr_t userData);
	/** see @ref omrcuda.cpp::omrcuda_streamCreate "omrcuda_streamCreate" */
	int32_t (*cuda_streamCreate)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream *streamOut);
	/** see @ref omrcuda.cpp::omrcuda_streamCreateWithPriority "omrcuda_streamCreateWithPriority" */
	int32_t (*cuda_streamCreateWithPriority)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, int32_t priority, uint32_t flags, J9CudaStream *streamOut);
	/** see @ref omrcuda.cpp::omrcuda_streamDestroy "omrcuda_streamDestroy" */
	int32_t (*cuda_streamDestroy)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_streamGetFlags "omrcuda_streamGetFlags" */
	int32_t (*cuda_streamGetFlags)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, uint32_t *flagsOut);
	/** see @ref omrcuda.cpp::omrcuda_streamGetPriority "omrcuda_streamGetPriority" */
	int32_t (*cuda_streamGetPriority)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, int32_t *priorityOut);
	/** see @ref omrcuda.cpp::omrcuda_streamQuery "omrcuda_streamQuery" */
	int32_t (*cuda_streamQuery)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_streamSynchronize "omrcuda_streamSynchronize" */
	int32_t (*cuda_streamSynchronize)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream);
	/** see @ref omrcuda.cpp::omrcuda_streamWaitEvent "omrcuda_streamWaitEvent" */
	int32_t (*cuda_streamWaitEvent)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, J9CudaEvent event);
#endif /* OMR_OPT_CUDA */
} OMRPortLibrary;

/**
 * Stores information about status of control file used by omrshmem_open() or omrshsem_deprecated_open().
 */
typedef struct OMRControlFileStatus {
	uintptr_t status;
	int32_t errorCode;
	char *errorMsg;
} OMRControlFileStatus;

/**
 * @name OMRPortShSemParameters
 * The caller is responsible creating storage for OMRPortShSemParameters.
 * The structure is only needed for the lifetime of the call to @ref omrshsem_open
 * This structure must be initialized using @ref omrshsem_params_init
 */
typedef struct OMRPortShSemParameters {
	const char *semName; /* Unique identifier of the semaphore. */
	uint32_t setSize; /* number of semaphores to be created in this set */
	uint32_t permission; /* Posix-style file permissions */
	const char* controlFileDir; /* Directory in which to create control files (SysV semaphores only) */
	uint8_t proj_id; /* parameter used with semName to generate semaphore key */
	uint32_t deleteBasefile : 1; /* delete the base file (used to generate the semaphore key) when destroying the semaphore */
} OMRPortShSemParameters;

/**
 * @name Control file unlink status
 * Flags used to indicate unlink status of control files used by semaphore set or shared memory
 * These flags are used to store value in J9ControlFileStatus.status
 * @{
 */
#define OMRPORT_INFO_CONTROL_FILE_NOT_UNLINKED 0
#define OMRPORT_INFO_CONTROL_FILE_UNLINK_FAILED 1
#define OMRPORT_INFO_CONTROL_FILE_UNLINKED 2
/** @} */

/**
 * @name Port library startup and shutdown functions
 * @anchor PortStartup
 * Create, initialize, startup and shutdow the port library
 * @{
 */
/** Standard startup and shutdown (port library allocated on stack or by application)  */
extern J9_CFUNC int32_t omrport_create_library(struct OMRPortLibrary *portLibrary, uintptr_t size);
extern J9_CFUNC int32_t omrport_init_library(struct OMRPortLibrary *portLibrary, uintptr_t size);
extern J9_CFUNC int32_t omrport_shutdown_library(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t omrport_startup_library(struct OMRPortLibrary *portLibrary);

/** Port library self allocation routines */
extern J9_CFUNC int32_t omrport_allocate_library(struct OMRPortLibrary **portLibrary);
/** @} */

/**
 * @name Port library version and compatability queries
 * @anchor PortVersionControl
 * Determine port library compatability and version.
 * @{
 */
extern J9_CFUNC uintptr_t omrport_getSize(void);
extern J9_CFUNC int32_t omrport_getVersion(struct OMRPortLibrary *portLibrary);
/** @} */

/**
 * @name PortLibrary Access functions
 * Users should call port library functions through these macros.
 * @code
 * OMRPORT_ACCESS_FROM_OMRVM(omrVM);
 * memoryPointer = omrmem_allocate_memory(1024);
 * @endcode
 * @{
 */
#if !defined(OMRPORT_LIBRARY_DEFINE)
#define omrport_shutdown_library() privateOmrPortLibrary->port_shutdown_library(privateOmrPortLibrary)
#define omrport_isFunctionOverridden(param1) privateOmrPortLibrary->port_isFunctionOverridden(privateOmrPortLibrary, (param1))
#define omrport_tls_free() privateOmrPortLibrary->port_tls_free(privateOmrPortLibrary)
#define omrerror_startup() privateOmrPortLibrary->error_startup(privateOmrPortLibrary)
#define omrerror_shutdown() privateOmrPortLibrary->error_shutdown(privateOmrPortLibrary)
#define omrerror_last_error_number() privateOmrPortLibrary->error_last_error_number(privateOmrPortLibrary)
#define omrerror_last_error_message() privateOmrPortLibrary->error_last_error_message(privateOmrPortLibrary)
#define omrerror_set_last_error(param1,param2) privateOmrPortLibrary->error_set_last_error(privateOmrPortLibrary, (param1), (param2))
#define omrerror_set_last_error_with_message(param1,param2) privateOmrPortLibrary->error_set_last_error_with_message(privateOmrPortLibrary, (param1), (param2))
#define omrerror_set_last_error_with_message_format(...) privateOmrPortLibrary->error_set_last_error_with_message_format(privateOmrPortLibrary, __VA_ARGS__)
#define omrtime_startup() privateOmrPortLibrary->time_startup(privateOmrPortLibrary)
#define omrtime_shutdown() privateOmrPortLibrary->time_shutdown(privateOmrPortLibrary)
#define omrtime_msec_clock() privateOmrPortLibrary->time_msec_clock(privateOmrPortLibrary)
#define omrtime_usec_clock() privateOmrPortLibrary->time_usec_clock(privateOmrPortLibrary)
#define omrtime_current_time_millis() privateOmrPortLibrary->time_current_time_millis(privateOmrPortLibrary)
#define omrtime_current_time_nanos(param1) privateOmrPortLibrary->time_current_time_nanos(privateOmrPortLibrary, (param1))
#define omrtime_nano_time() privateOmrPortLibrary->time_nano_time(privateOmrPortLibrary)
#define omrtime_hires_clock() privateOmrPortLibrary->time_hires_clock(privateOmrPortLibrary)
#define omrtime_hires_frequency() privateOmrPortLibrary->time_hires_frequency(privateOmrPortLibrary)
#define omrtime_hires_delta(param1,param2,param3) privateOmrPortLibrary->time_hires_delta(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsysinfo_startup() privateOmrPortLibrary->sysinfo_startup(privateOmrPortLibrary)
#define omrsysinfo_shutdown() privateOmrPortLibrary->sysinfo_shutdown(privateOmrPortLibrary)
#define omrsysinfo_process_exists(param1) privateOmrPortLibrary->sysinfo_process_exists(privateOmrPortLibrary, (param1))
#define omrsysinfo_get_egid() privateOmrPortLibrary->sysinfo_get_egid(privateOmrPortLibrary)
#define omrsysinfo_get_euid() privateOmrPortLibrary->sysinfo_get_euid(privateOmrPortLibrary)
#define omrsysinfo_get_groups(param1,param2) privateOmrPortLibrary->sysinfo_get_groups(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_pid() privateOmrPortLibrary->sysinfo_get_pid(privateOmrPortLibrary)
#define omrsysinfo_get_ppid() privateOmrPortLibrary->sysinfo_get_ppid(privateOmrPortLibrary)
#define omrsysinfo_get_memory_info(param1) privateOmrPortLibrary->sysinfo_get_memory_info(privateOmrPortLibrary, (param1))
#define omrsysinfo_get_memory_info_with_flags(param1,param2) privateOmrPortLibrary->sysinfo_get_memory_info(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_processor_info(param1) privateOmrPortLibrary->sysinfo_get_processor_info(privateOmrPortLibrary, (param1))
#define omrsysinfo_destroy_processor_info(param1) privateOmrPortLibrary->sysinfo_destroy_processor_info(privateOmrPortLibrary, (param1))
#define omrsysinfo_get_addressable_physical_memory() privateOmrPortLibrary->sysinfo_get_addressable_physical_memory(privateOmrPortLibrary)
#define omrsysinfo_get_physical_memory() privateOmrPortLibrary->sysinfo_get_physical_memory(privateOmrPortLibrary)
#define omrsysinfo_get_OS_version() privateOmrPortLibrary->sysinfo_get_OS_version(privateOmrPortLibrary)
#define omrsysinfo_get_env(param1,param2,param3) privateOmrPortLibrary->sysinfo_get_env(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsysinfo_get_CPU_architecture() privateOmrPortLibrary->sysinfo_get_CPU_architecture(privateOmrPortLibrary)
#define omrsysinfo_get_processor_description(param1) privateOmrPortLibrary->sysinfo_get_processor_description(privateOmrPortLibrary,param1)
#define omrsysinfo_processor_has_feature(param1,param2) privateOmrPortLibrary->sysinfo_processor_has_feature(privateOmrPortLibrary,param1,param2)
#define omrsysinfo_processor_set_feature(param1,param2,param3) privateOmrPortLibrary->sysinfo_processor_set_feature(privateOmrPortLibrary,param1,param2,param3)
#define omrsysinfo_get_processor_feature_name(param1) privateOmrPortLibrary->sysinfo_get_processor_feature_name(privateOmrPortLibrary,param1)
#define omrsysinfo_get_processor_feature_string(param1,param2,param3) privateOmrPortLibrary->sysinfo_get_processor_feature_string(privateOmrPortLibrary,param1,param2,param3)
#define omrsysinfo_get_OS_type() privateOmrPortLibrary->sysinfo_get_OS_type(privateOmrPortLibrary)
#define omrsysinfo_get_executable_name(param1,param2) privateOmrPortLibrary->sysinfo_get_executable_name(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_username(param1,param2) privateOmrPortLibrary->sysinfo_get_username(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_groupname(param1,param2) privateOmrPortLibrary->sysinfo_get_groupname(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_hostname(param1,param2) privateOmrPortLibrary->sysinfo_get_hostname(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_load_average(param1) privateOmrPortLibrary->sysinfo_get_load_average(privateOmrPortLibrary, (param1))
#define omrsysinfo_get_CPU_utilization(param1) privateOmrPortLibrary->sysinfo_get_CPU_utilization(privateOmrPortLibrary, (param1))
#define omrsysinfo_get_CPU_load(param1) privateOmrPortLibrary->sysinfo_get_CPU_load(privateOmrPortLibrary, (param1))
#define omrsysinfo_limit_iterator_init(param1) privateOmrPortLibrary->sysinfo_limit_iterator_init(privateOmrPortLibrary, (param1))
#define omrsysinfo_limit_iterator_hasNext(param1) privateOmrPortLibrary->sysinfo_limit_iterator_hasNext(privateOmrPortLibrary, (param1))
#define omrsysinfo_limit_iterator_next(param1,param2) privateOmrPortLibrary->sysinfo_limit_iterator_next(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_env_iterator_init(param1,param2,param3) privateOmrPortLibrary->sysinfo_env_iterator_init(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsysinfo_env_iterator_hasNext(param1) privateOmrPortLibrary->sysinfo_env_iterator_hasNext(privateOmrPortLibrary, (param1))
#define omrsysinfo_env_iterator_next(param1,param2) privateOmrPortLibrary->sysinfo_env_iterator_next(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_set_number_user_specified_CPUs(param1) privateOmrPortLibrary->sysinfo_set_number_user_specified_CPUs(privateOmrPortLibrary,(param1))
#define omrfile_startup() privateOmrPortLibrary->file_startup(privateOmrPortLibrary)
#define omrfile_shutdown() privateOmrPortLibrary->file_shutdown(privateOmrPortLibrary)
#define omrfile_write(param1,param2,param3) privateOmrPortLibrary->file_write(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_write_text(param1,param2,param3) privateOmrPortLibrary->file_write_text(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_get_text_encoding(param1,param2) privateOmrPortLibrary->file_get_text_encoding(privateOmrPortLibrary, (param1), (param2))
#define omrfile_vprintf(param1,param2,param3) privateOmrPortLibrary->file_vprintf(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_printf(...) privateOmrPortLibrary->file_printf(privateOmrPortLibrary, __VA_ARGS__)
#define omrfile_open(param1,param2,param3) privateOmrPortLibrary->file_open(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_close(param1) privateOmrPortLibrary->file_close(privateOmrPortLibrary, (param1))
#define omrfile_seek(param1,param2,param3) privateOmrPortLibrary->file_seek(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_read(param1,param2,param3) privateOmrPortLibrary->file_read(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_unlink(param1) privateOmrPortLibrary->file_unlink(privateOmrPortLibrary, (param1))
#define omrfile_attr(param1) privateOmrPortLibrary->file_attr(privateOmrPortLibrary, (param1))
#define omrfile_chmod(param1,param2) privateOmrPortLibrary->file_chmod(privateOmrPortLibrary, (param1), (param2))
#define omrfile_chown(param1,param2,param3) privateOmrPortLibrary->file_chown(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_lastmod(param1) privateOmrPortLibrary->file_lastmod(privateOmrPortLibrary, (param1))
#define omrfile_length(param1) privateOmrPortLibrary->file_length(privateOmrPortLibrary, (param1))
#define omrfile_flength(param1) privateOmrPortLibrary->file_flength(privateOmrPortLibrary, (param1))
#define omrfile_set_length(param1,param2) privateOmrPortLibrary->file_set_length(privateOmrPortLibrary, (param1), (param2))
#define omrfile_sync(param1) privateOmrPortLibrary->file_sync(privateOmrPortLibrary, (param1))
#define omrfile_fstat(param1,param2) privateOmrPortLibrary->file_fstat(privateOmrPortLibrary, (param1), (param2))
#define omrfile_stat(param1,param2,param3) privateOmrPortLibrary->file_stat(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_stat_filesystem(param1,param2,param3) privateOmrPortLibrary->file_stat_filesystem(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_blockingasync_open(param1,param2,param3) privateOmrPortLibrary->file_blockingasync_open(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_blockingasync_close(param1) privateOmrPortLibrary->file_blockingasync_close(privateOmrPortLibrary, (param1))
#define omrfile_blockingasync_read(param1,param2,param3) privateOmrPortLibrary->file_blockingasync_read(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_blockingasync_write(param1,param2,param3) privateOmrPortLibrary->file_blockingasync_write(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_blockingasync_unlock_bytes(param1,param2,param3) privateOmrPortLibrary->file_blockingasync_unlock_bytes(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_blockingasync_lock_bytes(param1,param2,param3,param4) privateOmrPortLibrary->file_blockingasync_lock_bytes(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrfile_blockingasync_set_length(param1,param2) privateOmrPortLibrary->file_blockingasync_set_length(privateOmrPortLibrary, (param1), (param2))
#define omrfile_blockingasync_flength(param1) privateOmrPortLibrary->file_blockingasync_flength(privateOmrPortLibrary, (param1))
#define omrfilestream_startup() privateOmrPortLibrary->filestream_startup(privatePortLibrary)
#define omrfilestream_shutdown() privateOmrPortLibrary->filestream_shutdown(privatePortLibrary)
#define omrfilestream_open(param1, param2, param3) privateOmrPortLibrary->filestream_open(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfilestream_close(param1) privateOmrPortLibrary->filestream_close(privateOmrPortLibrary, (param1))
#define omrfilestream_write(param1, param2, param3) privateOmrPortLibrary->filestream_write(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfilestream_write_text(param1, param2, param3, param4) privateOmrPortLibrary->filestream_write_text(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrfilestream_vprintf(param1, param2, param3) privateOmrPortLibrary->filestream_vprintf(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfilestream_printf(...) privateOmrPortLibrary->filestream_printf(privateOmrPortLibrary, __VA_ARGS__)
#define omrfilestream_sync(param1) privateOmrPortLibrary->filestream_sync(privateOmrPortLibrary, param1)
#define omrfilestream_setbuffer(param1, param2, param3, param4) privateOmrPortLibrary->filestream_setbuffer(privateOmrPortLibrary, param1, param2, param3, param4)
#define omrfilestream_fdopen(param1, param2) privateOmrPortLibrary->filestream_fdopen(privateOmrPortLibrary, param1, param2)
#define omrfilestream_fileno(param1) privateOmrPortLibrary->filestream_fileno(privateOmrPortLibrary, param1)
#define omrsl_startup() privateOmrPortLibrary->sl_startup(privateOmrPortLibrary)
#define omrsl_shutdown() privateOmrPortLibrary->sl_shutdown(privateOmrPortLibrary)
#define omrsl_close_shared_library(param1) privateOmrPortLibrary->sl_close_shared_library(privateOmrPortLibrary, (param1))
#define omrsl_open_shared_library(param1,param2,param3) privateOmrPortLibrary->sl_open_shared_library(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsl_lookup_name(param1,param2,param3,param4) privateOmrPortLibrary->sl_lookup_name(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrtty_startup() privateOmrPortLibrary->tty_startup(privateOmrPortLibrary)
#define omrtty_shutdown() privateOmrPortLibrary->tty_shutdown(privateOmrPortLibrary)
#define omrtty_printf(...) privateOmrPortLibrary->tty_printf(privateOmrPortLibrary, __VA_ARGS__)
#define omrtty_vprintf(param1,param2) privateOmrPortLibrary->tty_vprintf(privateOmrPortLibrary, (param1), (param2))
#define omrtty_get_chars(param1,param2) privateOmrPortLibrary->tty_get_chars(privateOmrPortLibrary, (param1), (param2))
#define omrtty_err_printf(...) privateOmrPortLibrary->tty_err_printf(privateOmrPortLibrary, __VA_ARGS__)
#define omrtty_err_vprintf(param1,param2) privateOmrPortLibrary->tty_err_vprintf(privateOmrPortLibrary, (param1), (param2))
#define omrtty_available() privateOmrPortLibrary->tty_available(privateOmrPortLibrary)
#define omrtty_daemonize() privateOmrPortLibrary->tty_daemonize(privateOmrPortLibrary)
#define omrheap_create(param1,param2,param3) privateOmrPortLibrary->heap_create(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrheap_allocate(param1,param2) privateOmrPortLibrary->heap_allocate(privateOmrPortLibrary, (param1), (param2))
#define omrheap_free(param1,param2) privateOmrPortLibrary->heap_free(privateOmrPortLibrary, (param1), (param2))
#define omrheap_reallocate(param1,param2,param3) privateOmrPortLibrary->heap_reallocate(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrmem_startup(param1) privateOmrPortLibrary->mem_startup(privateOmrPortLibrary, (param1))
#define omrmem_shutdown() privateOmrPortLibrary->mem_shutdown(privateOmrPortLibrary)
#define omrmem_allocate_memory(param1, category) privateOmrPortLibrary->mem_allocate_memory(privateOmrPortLibrary, (param1), OMR_GET_CALLSITE(), (category))
#define omrmem_free_memory(param1) privateOmrPortLibrary->mem_free_memory(privateOmrPortLibrary, (param1))
#define omrmem_advise_and_free_memory(param1) privateOmrPortLibrary->mem_advise_and_free_memory(privateOmrPortLibrary, (param1))
#define omrmem_reallocate_memory(param1, param2, category) privateOmrPortLibrary->mem_reallocate_memory(privateOmrPortLibrary, (param1), (param2), OMR_GET_CALLSITE(), (category))
#define omrmem_allocate_memory32(param1, category) privateOmrPortLibrary->mem_allocate_memory32(privateOmrPortLibrary, (param1), OMR_GET_CALLSITE(), (category))
#define omrmem_free_memory32(param1) privateOmrPortLibrary->mem_free_memory32(privateOmrPortLibrary, (param1))
#define omrmem_ensure_capacity32(param1) privateOmrPortLibrary->mem_ensure_capacity32(privateOmrPortLibrary, (param1))
#define omrcpu_startup() privateOmrPortLibrary->cpu_startup(privateOmrPortLibrary)
#define omrcpu_shutdown() privateOmrPortLibrary->cpu_shutdown(privateOmrPortLibrary)
#define omrcpu_flush_icache(param1,param2) privateOmrPortLibrary->cpu_flush_icache(privateOmrPortLibrary, (param1), (param2))
#define omrcpu_get_cache_line_size(param1) privateOmrPortLibrary->cpu_get_cache_line_size(privateOmrPortLibrary, (param1))
#define omrvmem_startup() privateOmrPortLibrary->vmem_startup(privateOmrPortLibrary)
#define omrvmem_shutdown() privateOmrPortLibrary->vmem_shutdown(privateOmrPortLibrary)
#define omrvmem_commit_memory(param1,param2,param3) privateOmrPortLibrary->vmem_commit_memory(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrvmem_decommit_memory(param1,param2,param3) privateOmrPortLibrary->vmem_decommit_memory(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrvmem_free_memory(param1,param2,param3) privateOmrPortLibrary->vmem_free_memory(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrvmem_vmem_params_init(param1) privateOmrPortLibrary->vmem_vmem_params_init(privateOmrPortLibrary, (param1))
#define omrvmem_reserve_memory(param1,param2,param3,param4,param5,param6) privateOmrPortLibrary->vmem_reserve_memory(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5), (param6))
#define omrvmem_reserve_memory_ex(param1,param2) privateOmrPortLibrary->vmem_reserve_memory_ex(privateOmrPortLibrary, (param1), (param2))
#define omrvmem_get_contiguous_region_memory(param1, param2, param3, param4, param5, param6, param7, param8, param9) privateOmrPortLibrary->vmem_get_contiguous_region_memory(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5), (param6), (param7), (param8), (param9))
#define omrvmem_create_double_mapped_region(param1, param2, param3, param4, param5, param6, param7, param8, param9, param10) privateOmrPortLibrary->vmem_create_double_mapped_region(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5), (param6), (param7), (param8), (param9), (param10))
#define omrvmem_release_double_mapped_region(param1, param2, param3) privateOmrPortLibrary->vmem_release_double_mapped_region(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrvmem_get_page_size(param1) privateOmrPortLibrary->vmem_get_page_size(privateOmrPortLibrary, (param1))
#define omrvmem_get_page_flags(param1) privateOmrPortLibrary->vmem_get_page_flags(privateOmrPortLibrary, (param1))
#define omrvmem_supported_page_sizes() privateOmrPortLibrary->vmem_supported_page_sizes(privateOmrPortLibrary)
#define omrvmem_supported_page_flags() privateOmrPortLibrary->vmem_supported_page_flags(privateOmrPortLibrary)
#define omrvmem_default_large_page_size_ex(param1,param2,param3) privateOmrPortLibrary->vmem_default_large_page_size_ex(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrvmem_find_valid_page_size(param1,param2,param3,param4) privateOmrPortLibrary->vmem_find_valid_page_size(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrvmem_numa_set_affinity(param1,param2,param3,param4) privateOmrPortLibrary->vmem_numa_set_affinity(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrvmem_numa_get_node_details(param1,param2) privateOmrPortLibrary->vmem_numa_get_node_details(privateOmrPortLibrary, (param1), (param2))
#define omrvmem_get_available_physical_memory(param1) privateOmrPortLibrary->vmem_get_available_physical_memory(privateOmrPortLibrary, (param1))
#define omrvmem_get_process_memory_size(param1,param2) privateOmrPortLibrary->vmem_get_process_memory_size(privateOmrPortLibrary, (param1), (param2))
#define omrstr_startup() privateOmrPortLibrary->str_startup(privateOmrPortLibrary)
#define omrstr_shutdown() privateOmrPortLibrary->str_shutdown(privateOmrPortLibrary)
#define omrstr_printf(...) privateOmrPortLibrary->str_printf(privateOmrPortLibrary, __VA_ARGS__)
#define omrstr_vprintf(param1,param2,param3,param4) privateOmrPortLibrary->str_vprintf(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrstr_create_tokens(param1) privateOmrPortLibrary->str_create_tokens(privateOmrPortLibrary, (param1))
#define omrstr_set_token(...) privateOmrPortLibrary->str_set_token(privateOmrPortLibrary, __VA_ARGS__)
#define omrstr_subst_tokens(param1,param2,param3,param4) privateOmrPortLibrary->str_subst_tokens(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrstr_free_tokens(param1) privateOmrPortLibrary->str_free_tokens(privateOmrPortLibrary, (param1))
#define omrstr_set_time_tokens(param1,param2) privateOmrPortLibrary->str_set_time_tokens(privateOmrPortLibrary, (param1), (param2))
#define omrstr_convert(param1,param2,param3,param4,param5,param6) privateOmrPortLibrary->str_convert(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5), (param6))
#define omrexit_startup() privateOmrPortLibrary->exit_startup(privateOmrPortLibrary)
#define omrexit_shutdown() privateOmrPortLibrary->exit_shutdown(privateOmrPortLibrary)
#define omrexit_get_exit_code() privateOmrPortLibrary->exit_get_exit_code(privateOmrPortLibrary)
#define omrexit_shutdown_and_exit(param1) privateOmrPortLibrary->exit_shutdown_and_exit(privateOmrPortLibrary, (param1))
#define omrdump_create(param1,param2,param3) privateOmrPortLibrary->dump_create(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrdump_startup() privateOmrPortLibrary->dump_startup(privateOmrPortLibrary)
#define omrdump_shutdown() privateOmrPortLibrary->dump_shutdown(privateOmrPortLibrary)
#define omrnls_startup() privateOmrPortLibrary->nls_startup(privateOmrPortLibrary)
#define omrnls_free_cached_data() privateOmrPortLibrary->nls_free_cached_data(privateOmrPortLibrary)
#define omrnls_shutdown() privateOmrPortLibrary->nls_shutdown(privateOmrPortLibrary)
#define omrnls_set_catalog(param1,param2,param3,param4) privateOmrPortLibrary->nls_set_catalog(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrnls_set_locale(param1,param2,param3) privateOmrPortLibrary->nls_set_locale(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrnls_get_language() privateOmrPortLibrary->nls_get_language(privateOmrPortLibrary)
#define omrnls_get_region() privateOmrPortLibrary->nls_get_region(privateOmrPortLibrary)
#define omrnls_get_variant() privateOmrPortLibrary->nls_get_variant(privateOmrPortLibrary)
#define omrnls_printf(...) privateOmrPortLibrary->nls_printf(privateOmrPortLibrary, __VA_ARGS__)
#define omrnls_vprintf(param1,param2,param3) privateOmrPortLibrary->nls_vprintf(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrnls_lookup_message(param1,param2,param3,param4) privateOmrPortLibrary->nls_lookup_message(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrport_control(param1,param2) privateOmrPortLibrary->port_control(privateOmrPortLibrary, (param1), (param2))
#define omrsig_startup() privateOmrPortLibrary->sig_startup(privateOmrPortLibrary)
#define omrsig_shutdown() privateOmrPortLibrary->sig_shutdown(privateOmrPortLibrary)
#define omrsig_protect(param1,param2,param3,param4,param5,param6) privateOmrPortLibrary->sig_protect(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5), (param6))
#define omrsig_can_protect(param1) privateOmrPortLibrary->sig_can_protect(privateOmrPortLibrary, (param1))
#define omrsig_set_async_signal_handler(param1,param2,param3) privateOmrPortLibrary->sig_set_async_signal_handler(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsig_set_single_async_signal_handler(param1,param2,param3,param4) privateOmrPortLibrary->sig_set_single_async_signal_handler(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsig_map_os_signal_to_portlib_signal(param1) privateOmrPortLibrary->sig_map_os_signal_to_portlib_signal(privateOmrPortLibrary, (param1))
#define omrsig_map_portlib_signal_to_os_signal(param1) privateOmrPortLibrary->sig_map_portlib_signal_to_os_signal(privateOmrPortLibrary, (param1))
#define omrsig_register_os_handler(param1,param2,param3) privateOmrPortLibrary->sig_register_os_handler(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsig_is_main_signal_handler(param1) privateOmrPortLibrary->sig_is_main_signal_handler(privateOmrPortLibrary, (param1))
#define omrsig_is_signal_ignored(param1, param2) privateOmrPortLibrary->sig_is_signal_ignored(privateOmrPortLibrary, (param1), (param2))
#define omrsig_info(param1,param2,param3,param4,param5) privateOmrPortLibrary->sig_info(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5))
#define omrsig_info_count(param1,param2) privateOmrPortLibrary->sig_info_count(privateOmrPortLibrary, (param1), (param2))
#define omrsig_set_options(param1) privateOmrPortLibrary->sig_set_options(privateOmrPortLibrary, (param1))
#define omrsig_get_options() privateOmrPortLibrary->sig_get_options(privateOmrPortLibrary)
#define omrsig_get_current_signal() privateOmrPortLibrary->sig_get_current_signal(privateOmrPortLibrary)
#define omrsig_set_reporter_priority(param1) privateOmrPortLibrary->sig_set_reporter_priority(privateOmrPortLibrary, (param1))
#define omrfile_read_text(param1,param2,param3) privateOmrPortLibrary->file_read_text(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_mkdir(param1) privateOmrPortLibrary->file_mkdir(privateOmrPortLibrary, (param1))
#define omrfile_move(param1,param2) privateOmrPortLibrary->file_move(privateOmrPortLibrary, (param1), (param2))
#define omrfile_unlinkdir(param1) privateOmrPortLibrary->file_unlinkdir(privateOmrPortLibrary, (param1))
#define omrfile_findfirst(param1,param2) privateOmrPortLibrary->file_findfirst(privateOmrPortLibrary, (param1), (param2))
#define omrfile_findnext(param1,param2) privateOmrPortLibrary->file_findnext(privateOmrPortLibrary, (param1), (param2))
#define omrfile_findclose(param1) privateOmrPortLibrary->file_findclose(privateOmrPortLibrary, (param1))
#define omrfile_error_message() privateOmrPortLibrary->file_error_message(privateOmrPortLibrary)
#define omrfile_unlock_bytes(param1,param2,param3) privateOmrPortLibrary->file_unlock_bytes(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrfile_lock_bytes(param1,param2,param3,param4) privateOmrPortLibrary->file_lock_bytes(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrfile_convert_native_fd_to_omrfile_fd(param1) privateOmrPortLibrary->file_convert_native_fd_to_omrfile_fd(privateOmrPortLibrary, (param1))
#define omrfile_convert_omrfile_fd_to_native_fd(param1) privateOmrPortLibrary->file_convert_omrfile_fd_to_native_fd(privateOmrPortLibrary,param1)
#define omrstr_ftime(param1,param2,param3,param4) privateOmrPortLibrary->str_ftime_ex(privateOmrPortLibrary, (param1), (param2), (param3), (param4), OMRSTR_FTIME_FLAG_LOCAL)
#define omrstr_ftime_ex(param1,param2,param3,param4,param5) privateOmrPortLibrary->str_ftime_ex(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5))
#define omrstr_current_time_zone(param1,param2,param3) privateOmrPortLibrary->str_current_time_zone(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrmmap_startup() privateOmrPortLibrary->mmap_startup(privateOmrPortLibrary)
#define omrmmap_shutdown() privateOmrPortLibrary->mmap_shutdown(privateOmrPortLibrary)
#define omrmmap_capabilities() privateOmrPortLibrary->mmap_capabilities(privateOmrPortLibrary)
#define omrmmap_map_file(param1,param2,param3,param4,param5,param6) privateOmrPortLibrary->mmap_map_file(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5), (param6))
#define omrmmap_unmap_file(param1) privateOmrPortLibrary->mmap_unmap_file(privateOmrPortLibrary, (param1))
#define omrmmap_msync(param1,param2,param3) privateOmrPortLibrary->mmap_msync(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrmmap_protect(param1,param2,param3) privateOmrPortLibrary->mmap_protect(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrmmap_get_region_granularity(param1) privateOmrPortLibrary->mmap_get_region_granularity(privateOmrPortLibrary, (param1))
#define omrmmap_dont_need(param1, param2) privateOmrPortLibrary->mmap_dont_need(privateOmrPortLibrary, (param1), param2)
#if !defined(OMR_OS_WINDOWS)
#define omrshsem_params_init(param1) privateOmrPortLibrary->shsem_params_init(privateOmrPortLibrary,param1)
#define omrshsem_startup() privateOmrPortLibrary->shsem_startup(privateOmrPortLibrary)
#define omrshsem_shutdown() privateOmrPortLibrary->shsem_shutdown(privateOmrPortLibrary)
#define omrshsem_open(param1,param2) privateOmrPortLibrary->shsem_open(privateOmrPortLibrary,param1,param2)
#define omrshsem_post(param1,param2,param3) privateOmrPortLibrary->shsem_post(privateOmrPortLibrary,param1,param2,param3)
#define omrshsem_wait(param1,param2,param3) privateOmrPortLibrary->shsem_wait(privateOmrPortLibrary,param1,param2,param3)
#define omrshsem_getVal(param1,param2) privateOmrPortLibrary->shsem_getVal(privateOmrPortLibrary,param1,param2)
#define omrshsem_setVal(param1,param2,param3) privateOmrPortLibrary->shsem_setVal(privateOmrPortLibrary,param1,param2,param3)
#define omrshsem_close(param1) privateOmrPortLibrary->shsem_close(privateOmrPortLibrary,param1)
#define omrshsem_destroy(param1) privateOmrPortLibrary->shsem_destroy(privateOmrPortLibrary,param1)
#define omrshsem_deprecated_startup() privateOmrPortLibrary->shsem_deprecated_startup(privateOmrPortLibrary)
#define omrshsem_deprecated_shutdown() privateOmrPortLibrary->shsem_deprecated_shutdown(privateOmrPortLibrary)
#define omrshsem_deprecated_open(param1,param2,param3,param4,param5,param6,param7,param8) privateOmrPortLibrary->shsem_deprecated_open(privateOmrPortLibrary,param1,param2,param3,param4,param5,param6,param7,param8)
#define omrshsem_deprecated_openDeprecated(param1,param2,param3,param4,param5) privateOmrPortLibrary->shsem_deprecated_openDeprecated(privateOmrPortLibrary,param1,param2,param3,param4,param5)
#define omrshsem_deprecated_post(param1,param2,param3) privateOmrPortLibrary->shsem_deprecated_post(privateOmrPortLibrary,param1,param2,param3)
#define omrshsem_deprecated_wait(param1,param2,param3) privateOmrPortLibrary->shsem_deprecated_wait(privateOmrPortLibrary,param1,param2,param3)
#define omrshsem_deprecated_getVal(param1,param2) privateOmrPortLibrary->shsem_deprecated_getVal(privateOmrPortLibrary,param1,param2)
#define omrshsem_deprecated_setVal(param1,param2,param3) privateOmrPortLibrary->shsem_deprecated_setVal(privateOmrPortLibrary,param1,param2,param3)
#define omrshsem_deprecated_handle_stat(param1,param2) privateOmrPortLibrary->shsem_deprecated_handle_stat(privateOmrPortLibrary,param1,param2)
#define omrshsem_deprecated_close(param1) privateOmrPortLibrary->shsem_deprecated_close(privateOmrPortLibrary,param1)
#define omrshsem_deprecated_destroy(param1) privateOmrPortLibrary->shsem_deprecated_destroy(privateOmrPortLibrary,param1)
#define omrshsem_deprecated_destroyDeprecated(param1,param2) privateOmrPortLibrary->shsem_deprecated_destroyDeprecated(privateOmrPortLibrary,param1,param2)
#define omrshsem_deprecated_getid(param1) privateOmrPortLibrary->shsem_deprecated_getid(privateOmrPortLibrary,param1)
#define omrshmem_startup() privateOmrPortLibrary->shmem_startup(privateOmrPortLibrary)
#define omrshmem_shutdown() privateOmrPortLibrary->shmem_shutdown(privateOmrPortLibrary)
#define omrshmem_open(param1,param2,param3,param4,param5,param6,param7,param8,param9) privateOmrPortLibrary->shmem_open(privateOmrPortLibrary,param1,param2,param3,param4,param5,param6,param7,param8,param9)
#define omrshmem_openDeprecated(param1,param2,param3,param4,param5,param6,param7) privateOmrPortLibrary->shmem_openDeprecated(privateOmrPortLibrary,param1,param2,param3,param4,param5,param6,param7)
#define omrshmem_attach(param1,param2) privateOmrPortLibrary->shmem_attach(privateOmrPortLibrary,param1,param2)
#define omrshmem_detach(param1) privateOmrPortLibrary->shmem_detach(privateOmrPortLibrary,param1)
#define omrshmem_close(param1) privateOmrPortLibrary->shmem_close(privateOmrPortLibrary,param1)
#define omrshmem_destroy(param1,param2,param3) privateOmrPortLibrary->shmem_destroy(privateOmrPortLibrary,param1,param2,param3)
#define omrshmem_destroyDeprecated(param1,param2,param3,param4) privateOmrPortLibrary->shmem_destroyDeprecated(privateOmrPortLibrary,param1,param2,param3,param4)
#define omrshmem_findfirst(param1,param2) privateOmrPortLibrary->shmem_findfirst(privateOmrPortLibrary,param1,param2)
#define omrshmem_findnext(param1,param2) privateOmrPortLibrary->shmem_findnext(privateOmrPortLibrary,param1,param2)
#define omrshmem_findclose(param1) privateOmrPortLibrary->shmem_findclose(privateOmrPortLibrary,param1)
#define omrshmem_stat(param1,param2,param3,param4) privateOmrPortLibrary->shmem_stat(privateOmrPortLibrary,param1,param2,param3,param4)
#define omrshmem_statDeprecated(param1,param2,param3,param4,param5) privateOmrPortLibrary->shmem_statDeprecated(privateOmrPortLibrary,param1,param2,param3,param4,param5)
#define omrshmem_handle_stat(param1,param2) privateOmrPortLibrary->shmem_handle_stat(privateOmrPortLibrary,param1,param2)
#define omrshmem_getDir(param1,param2,param3,param4) privateOmrPortLibrary->shmem_getDir(privateOmrPortLibrary,param1,param2,param3,param4)
#define omrshmem_createDir(param1,param2,param3) privateOmrPortLibrary->shmem_createDir(privateOmrPortLibrary,param1,param2,param3)
#define omrshmem_getFilepath(param1,param2,param3,param4) privateOmrPortLibrary->shmem_getFilepath(privateOmrPortLibrary,param1,param2,param3,param4)
#define omrshmem_protect(param1,param2,param3,param4,param5) privateOmrPortLibrary->shmem_protect(privateOmrPortLibrary,param1,param2,param3,param4,param5)
#define omrshmem_get_region_granularity(param1,param2,param3) privateOmrPortLibrary->shmem_get_region_granularity(privateOmrPortLibrary,param1,param2,param3)
#define omrshmem_getid(param1) privateOmrPortLibrary->shmem_getid(privateOmrPortLibrary,param1)
#endif /* !defined(OMR_OS_WINDOWS) */
#define omrsysinfo_get_limit(param1,param2) privateOmrPortLibrary->sysinfo_get_limit(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_set_limit(param1,param2) privateOmrPortLibrary->sysinfo_set_limit(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_number_CPUs_by_type(param1) privateOmrPortLibrary->sysinfo_get_number_CPUs_by_type(privateOmrPortLibrary, (param1))
#define omrsysinfo_get_cwd(param1,param2) privateOmrPortLibrary->sysinfo_get_cwd(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_get_tmp(param1,param2,param3) privateOmrPortLibrary->sysinfo_get_tmp(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsysinfo_get_open_file_count(param1) privateOmrPortLibrary->sysinfo_get_open_file_count(privateOmrPortLibrary, (param1))
#define omrsysinfo_get_os_description(param1) privateOmrPortLibrary->sysinfo_get_os_description(privateOmrPortLibrary, (param1))
#define omrsysinfo_os_has_feature(param1,param2) privateOmrPortLibrary->sysinfo_os_has_feature(privateOmrPortLibrary, (param1), (param2))
#define omrsysinfo_os_kernel_info(param1) privateOmrPortLibrary->sysinfo_os_kernel_info(privateOmrPortLibrary, (param1))
#define omrsysinfo_cgroup_is_system_available() privateOmrPortLibrary->sysinfo_cgroup_is_system_available(privateOmrPortLibrary)
#define omrsysinfo_cgroup_get_available_subsystems() privateOmrPortLibrary->sysinfo_cgroup_get_available_subsystems(privateOmrPortLibrary)
#define omrsysinfo_cgroup_are_subsystems_available(param1) privateOmrPortLibrary->sysinfo_cgroup_are_subsystems_available(privateOmrPortLibrary, param1)
#define omrsysinfo_cgroup_get_enabled_subsystems() privateOmrPortLibrary->sysinfo_cgroup_get_enabled_subsystems(privateOmrPortLibrary)
#define omrsysinfo_cgroup_enable_subsystems(param1) privateOmrPortLibrary->sysinfo_cgroup_enable_subsystems(privateOmrPortLibrary, param1)
#define omrsysinfo_cgroup_are_subsystems_enabled(param1) privateOmrPortLibrary->sysinfo_cgroup_are_subsystems_enabled(privateOmrPortLibrary, param1)
#define omrsysinfo_cgroup_get_memlimit(param1) privateOmrPortLibrary->sysinfo_cgroup_get_memlimit(privateOmrPortLibrary, param1)
#define omrsysinfo_cgroup_is_memlimit_set() privateOmrPortLibrary->sysinfo_cgroup_is_memlimit_set(privateOmrPortLibrary)
#define omrsysinfo_get_cgroup_subsystem_list() privateOmrPortLibrary->sysinfo_get_cgroup_subsystem_list(privateOmrPortLibrary)
#define omrsysinfo_is_running_in_container() privateOmrPortLibrary->sysinfo_is_running_in_container(privateOmrPortLibrary)
#define omrsysinfo_cgroup_subsystem_iterator_init(param1, param2) privateOmrPortLibrary->sysinfo_cgroup_subsystem_iterator_init(privateOmrPortLibrary, param1, param2)
#define omrsysinfo_cgroup_subsystem_iterator_hasNext(param1) privateOmrPortLibrary->sysinfo_cgroup_subsystem_iterator_hasNext(privateOmrPortLibrary, param1)
#define omrsysinfo_cgroup_subsystem_iterator_metricKey(param1, param2) privateOmrPortLibrary->sysinfo_cgroup_subsystem_iterator_metricKey(privateOmrPortLibrary, param1, param2)
#define omrsysinfo_cgroup_subsystem_iterator_next(param1, param2) privateOmrPortLibrary->sysinfo_cgroup_subsystem_iterator_next(privateOmrPortLibrary, param1, param2)
#define omrsysinfo_cgroup_subsystem_iterator_destroy(param1) privateOmrPortLibrary->sysinfo_cgroup_subsystem_iterator_destroy(privateOmrPortLibrary, param1)
#define omrsysinfo_get_process_start_time(param1, param2) privateOmrPortLibrary->sysinfo_get_process_start_time(privateOmrPortLibrary, param1, param2)
#define omrintrospect_startup() privateOmrPortLibrary->introspect_startup(privateOmrPortLibrary)
#define omrintrospect_shutdown() privateOmrPortLibrary->introspect_shutdown(privateOmrPortLibrary)
#define omrintrospect_set_suspend_signal_offset(param1) privateOmrPortLibrary->introspect_set_suspend_signal_offset(privateOmrPortLibrary, param1)
#define omrintrospect_threads_startDo(param1,param2) privateOmrPortLibrary->introspect_threads_startDo(privateOmrPortLibrary, (param1), (param2))
#define omrintrospect_threads_startDo_with_signal(param1,param2,param3) privateOmrPortLibrary->introspect_threads_startDo_with_signal(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrintrospect_threads_nextDo(param1) privateOmrPortLibrary->introspect_threads_nextDo(param1)
#define omrintrospect_backtrace_thread(param1,param2,param3) privateOmrPortLibrary->introspect_backtrace_thread(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrintrospect_backtrace_symbols(param1,param2) privateOmrPortLibrary->introspect_backtrace_symbols_ex(privateOmrPortLibrary, (param1), (param2), 0)
#define omrintrospect_backtrace_symbols_ex(param1,param2,param3) privateOmrPortLibrary->introspect_backtrace_symbols_ex(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsyslog_query() privateOmrPortLibrary->syslog_query(privateOmrPortLibrary)
#define omrsyslog_set(param1) privateOmrPortLibrary->syslog_set(privateOmrPortLibrary, (param1))
#define omrmem_walk_categories(param1) privateOmrPortLibrary->mem_walk_categories(privateOmrPortLibrary, (param1))
#define omrmem_get_category(param1) privateOmrPortLibrary->mem_get_category(privateOmrPortLibrary, (param1))
#define omrmem_categories_increment_counters(param1,param2) privateOmrPortLibrary->mem_categories_increment_counters((param1), (param2))
#define omrmem_categories_decrement_counters(param1,param2) privateOmrPortLibrary->mem_categories_decrement_counters((param1), (param2))
#define omrheap_query_size(param1,param2) privateOmrPortLibrary->heap_query_size(privateOmrPortLibrary, (param1), (param2))
#define omrheap_grow(param1,param2) privateOmrPortLibrary->heap_grow(privateOmrPortLibrary, (param1), (param2))
#define omrsock_startup() privateOmrPortLibrary->sock_startup(privateOmrPortLibrary)
#define omrsock_getaddrinfo_create_hints(param1,param2,param3,param4,param5) privateOmrPortLibrary->sock_getaddrinfo_create_hints(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5))
#define omrsock_getaddrinfo(param1,param2,param3,param4) privateOmrPortLibrary->sock_getaddrinfo(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_addrinfo_length(param1,param2) privateOmrPortLibrary->sock_addrinfo_length(privateOmrPortLibrary, (param1), (param2))
#define omrsock_addrinfo_family(param1,param2,param3) privateOmrPortLibrary->sock_addrinfo_family(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_addrinfo_socktype(param1,param2,param3) privateOmrPortLibrary->sock_addrinfo_socktype(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_addrinfo_protocol(param1,param2,param3) privateOmrPortLibrary->sock_addrinfo_protocol(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_addrinfo_address(param1, param2, param3) privateOmrPortLibrary->sock_addrinfo_address(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_freeaddrinfo(param1) privateOmrPortLibrary->sock_freeaddrinfo(privateOmrPortLibrary, (param1))
#define omrsock_sockaddr_init(param1, param2, param3, param4) privateOmrPortLibrary->sock_sockaddr_init(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_sockaddr_init6(param1, param2, param3, param4, param5, param6) privateOmrPortLibrary->sock_sockaddr_init6(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5), (param6))
#define omrsock_socket_getfd(param1) privateOmrPortLibrary->sock_socket_getfd(privateOmrPortLibrary, (param1))
#define omrsock_socket(param1, param2, param3, param4) privateOmrPortLibrary->sock_socket(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_bind(param1,param2) privateOmrPortLibrary->sock_bind(privateOmrPortLibrary, (param1), (param2))
#define omrsock_listen(param1,param2) privateOmrPortLibrary->sock_listen(privateOmrPortLibrary, (param1), (param2))
#define omrsock_connect(param1,param2) privateOmrPortLibrary->sock_connect(privateOmrPortLibrary, (param1), (param2))
#define omrsock_accept(param1,param2,param3) privateOmrPortLibrary->sock_accept(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_send(param1,param2,param3,param4) privateOmrPortLibrary->sock_send(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_sendto(param1,param2,param3,param4,param5) privateOmrPortLibrary->sock_sendto(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5))
#define omrsock_recv(param1,param2,param3,param4) privateOmrPortLibrary->sock_recv(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_recvfrom(param1,param2,param3,param4,param5) privateOmrPortLibrary->sock_recvfrom(privateOmrPortLibrary, (param1), (param2), (param3), (param4), (param5))
#define omrsock_pollfd_init(param1,param2,param3) privateOmrPortLibrary->sock_pollfd_init(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_get_pollfd_info(param1,param2,param3) privateOmrPortLibrary->sock_get_pollfd_info(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_poll(param1,param2,param3) privateOmrPortLibrary->sock_poll(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_fdset_zero(param1) privateOmrPortLibrary->sock_fdset_zero(privateOmrPortLibrary, (param1))
#define omrsock_fdset_set(param1,param2) privateOmrPortLibrary->sock_fdset_set(privateOmrPortLibrary, (param1), (param2))
#define omrsock_fdset_clr(param1,param2) privateOmrPortLibrary->sock_fdset_clr(privateOmrPortLibrary, (param1), (param2))
#define omrsock_fdset_isset(param1,param2) privateOmrPortLibrary->sock_fdset_isset(privateOmrPortLibrary, (param1), (param2))
#define omrsock_select(param1,param2,param3,param4) privateOmrPortLibrary->sock_select(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_close(param1) privateOmrPortLibrary->sock_close(privateOmrPortLibrary, (param1))
#define omrsock_shutdown() privateOmrPortLibrary->sock_shutdown(privateOmrPortLibrary)
#define omrsock_htons(param1) privateOmrPortLibrary->sock_htons(privateOmrPortLibrary, (param1))
#define omrsock_htonl(param1) privateOmrPortLibrary->sock_htonl(privateOmrPortLibrary, (param1))
#define omrsock_inet_pton(param1,param2,param3) privateOmrPortLibrary->sock_inet_pton(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_fcntl(param1,param2) privateOmrPortLibrary->sock_fcntl(privateOmrPortLibrary, (param1), (param2))
#define omrsock_timeval_init(param1,param2,param3) privateOmrPortLibrary->sock_timeval_init(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_linger_init(param1,param2,param3) privateOmrPortLibrary->sock_linger_init(privateOmrPortLibrary, (param1), (param2), (param3))
#define omrsock_setsockopt_int(param1,param2,param3,param4) privateOmrPortLibrary->sock_setsockopt_int(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_setsockopt_linger(param1,param2,param3,param4) privateOmrPortLibrary->sock_setsockopt_linger(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_setsockopt_timeval(param1,param2,param3,param4) privateOmrPortLibrary->sock_setsockopt_timeval(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_getsockopt_int(param1,param2,param3,param4) privateOmrPortLibrary->sock_getsockopt_int(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_getsockopt_linger(param1,param2,param3,param4) privateOmrPortLibrary->sock_getsockopt_linger(privateOmrPortLibrary, (param1), (param2), (param3), (param4))
#define omrsock_getsockopt_timeval(param1,param2,param3,param4) privateOmrPortLibrary->sock_getsockopt_timeval(privateOmrPortLibrary, (param1), (param2), (param3), (param4))

#if defined(OMR_OPT_CUDA)
#define omrcuda_startup() \
				privateOmrPortLibrary->cuda_startup(privateOmrPortLibrary)
#define omrcuda_shutdown() \
				privateOmrPortLibrary->cuda_shutdown(privateOmrPortLibrary)
#define omrcuda_deviceAlloc(deviceId, size, deviceAddressOut) \
				privateOmrPortLibrary->cuda_deviceAlloc(privateOmrPortLibrary, (deviceId), (size), (deviceAddressOut))
#define omrcuda_deviceCanAccessPeer(deviceId, peerDeviceId, canAccessPeerOut) \
				privateOmrPortLibrary->cuda_deviceCanAccessPeer(privateOmrPortLibrary, (deviceId), (peerDeviceId), (canAccessPeerOut))
#define omrcuda_deviceDisablePeerAccess(deviceId, peerDeviceId) \
				privateOmrPortLibrary->cuda_deviceDisablePeerAccess(privateOmrPortLibrary, (deviceId), (peerDeviceId))
#define omrcuda_deviceEnablePeerAccess(deviceId, peerDeviceId) \
				privateOmrPortLibrary->cuda_deviceEnablePeerAccess(privateOmrPortLibrary, (deviceId), (peerDeviceId))
#define omrcuda_deviceFree(deviceId, devicePointer) \
				privateOmrPortLibrary->cuda_deviceFree(privateOmrPortLibrary, (deviceId), (devicePointer))
#define omrcuda_deviceGetAttribute(deviceId, attribute, valueOut) \
				privateOmrPortLibrary->cuda_deviceGetAttribute(privateOmrPortLibrary, (deviceId), (attribute), (valueOut))
#define omrcuda_deviceGetCacheConfig(deviceId, configOut) \
				privateOmrPortLibrary->cuda_deviceGetCacheConfig(privateOmrPortLibrary, (deviceId), (configOut))
#define omrcuda_deviceGetCount(countOut) \
				privateOmrPortLibrary->cuda_deviceGetCount(privateOmrPortLibrary, (countOut))
#define omrcuda_deviceGetLimit(deviceId, limit, valueOut) \
				privateOmrPortLibrary->cuda_deviceGetLimit(privateOmrPortLibrary, (deviceId), (limit), (valueOut))
#define omrcuda_deviceGetMemInfo(deviceId, freeOut, totalOut) \
				privateOmrPortLibrary->cuda_deviceGetMemInfo(privateOmrPortLibrary, (deviceId), (freeOut), (totalOut))
#define omrcuda_deviceGetName(deviceId, nameSize, nameOut) \
				privateOmrPortLibrary->cuda_deviceGetName(privateOmrPortLibrary, (deviceId), (nameSize), (nameOut))
#define omrcuda_deviceGetSharedMemConfig(deviceId, configOut) \
				privateOmrPortLibrary->cuda_deviceGetSharedMemConfig(privateOmrPortLibrary, (deviceId), (configOut))
#define omrcuda_deviceGetStreamPriorityRange(deviceId, leastPriorityOut, greatestPriorityOut) \
				privateOmrPortLibrary->cuda_deviceGetStreamPriorityRange(privateOmrPortLibrary, (deviceId), (leastPriorityOut), (greatestPriorityOut))
#define omrcuda_deviceReset(deviceId) \
				privateOmrPortLibrary->cuda_deviceReset(privateOmrPortLibrary, (deviceId))
#define omrcuda_deviceSetCacheConfig(deviceId, config) \
				privateOmrPortLibrary->cuda_deviceSetCacheConfig(privateOmrPortLibrary, (deviceId), (config))
#define omrcuda_deviceSetLimit(deviceId, limit, value) \
				privateOmrPortLibrary->cuda_deviceSetLimit(privateOmrPortLibrary, (deviceId), (limit), (value))
#define omrcuda_deviceSetSharedMemConfig(deviceId, config) \
				privateOmrPortLibrary->cuda_deviceSetSharedMemConfig(privateOmrPortLibrary, (deviceId), (config))
#define omrcuda_deviceSynchronize(deviceId) \
				privateOmrPortLibrary->cuda_deviceSynchronize(privateOmrPortLibrary, (deviceId))
#define omrcuda_driverGetVersion(versionOut) \
				privateOmrPortLibrary->cuda_driverGetVersion(privateOmrPortLibrary, (versionOut))
#define omrcuda_eventCreate(deviceId, flags, eventOut) \
				privateOmrPortLibrary->cuda_eventCreate(privateOmrPortLibrary, (deviceId), (flags), (eventOut))
#define omrcuda_eventDestroy(deviceId, event) \
				privateOmrPortLibrary->cuda_eventDestroy(privateOmrPortLibrary, (deviceId), (event))
#define omrcuda_eventElapsedTime(start, end, elapsedMillisOut) \
				privateOmrPortLibrary->cuda_eventElapsedTime(privateOmrPortLibrary, (start), (end), (elapsedMillisOut))
#define omrcuda_eventQuery(event) \
				privateOmrPortLibrary->cuda_eventQuery(privateOmrPortLibrary, (event))
#define omrcuda_eventRecord(deviceId, event, stream) \
				privateOmrPortLibrary->cuda_eventRecord(privateOmrPortLibrary, (deviceId), (event), (stream))
#define omrcuda_eventSynchronize(event) \
				privateOmrPortLibrary->cuda_eventSynchronize(privateOmrPortLibrary, (event))
#define omrcuda_funcGetAttribute(deviceId, function, attribute, valueOut) \
				privateOmrPortLibrary->cuda_funcGetAttribute(privateOmrPortLibrary, (deviceId), (function), (attribute), (valueOut))
#define omrcuda_funcMaxActiveBlocksPerMultiprocessor(deviceId, function, blockSize, dynamicSharedMemorySize, flags, valueOut) \
				privateOmrPortLibrary->cuda_funcMaxActiveBlocksPerMultiprocessor(privateOmrPortLibrary, (deviceId), (function), (blockSize), (dynamicSharedMemorySize), (flags), (valueOut))
#define omrcuda_funcMaxPotentialBlockSize(deviceId, function, dynamicSharedMemoryFunction, userData, blockSizeLimit, flags, minGridSizeOut, maxBlockSizeOut) \
				privateOmrPortLibrary->cuda_funcMaxPotentialBlockSize(privateOmrPortLibrary, (deviceId), (function), (dynamicSharedMemoryFunction), (userData), (blockSizeLimit), (flags), (minGridSizeOut), (maxBlockSizeOut))
#define omrcuda_funcSetCacheConfig(deviceId, function, config) \
				privateOmrPortLibrary->cuda_funcSetCacheConfig(privateOmrPortLibrary, (deviceId), (function), (config))
#define omrcuda_funcSetSharedMemConfig(deviceId, function, config) \
				privateOmrPortLibrary->cuda_funcSetSharedMemConfig(privateOmrPortLibrary, (deviceId), (function), (config))
#define omrcuda_getErrorString(error) \
				privateOmrPortLibrary->cuda_getErrorString(privateOmrPortLibrary, (error))
#define omrcuda_hostAlloc(size, flags, hostAddressOut) \
				privateOmrPortLibrary->cuda_hostAlloc(privateOmrPortLibrary, (size), (flags), (hostAddressOut))
#define omrcuda_hostFree(hostAddress) \
				privateOmrPortLibrary->cuda_hostFree(privateOmrPortLibrary, (hostAddress))
#define omrcuda_launchKernel(deviceId, function, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, stream, kernelParms) \
				privateOmrPortLibrary->cuda_launchKernel(privateOmrPortLibrary, (deviceId), (function), (gridDimX), (gridDimY), (gridDimZ), (blockDimX), (blockDimY), (blockDimZ), (sharedMemBytes), (stream), (kernelParms))
#define omrcuda_linkerAddData(deviceId, linker, type, data, size, name, options) \
				privateOmrPortLibrary->cuda_linkerAddData(privateOmrPortLibrary, (deviceId), (linker), (type), (data), (size), (name), options)
#define omrcuda_linkerComplete(deviceId, linker, cubinOut, sizeOut) \
				privateOmrPortLibrary->cuda_linkerComplete(privateOmrPortLibrary, (deviceId), (linker), (cubinOut), (sizeOut))
#define omrcuda_linkerCreate(deviceId, options, linkerOut) \
				privateOmrPortLibrary->cuda_linkerCreate(privateOmrPortLibrary, (deviceId), (options), (linkerOut))
#define omrcuda_linkerDestroy(deviceId, linker) \
				privateOmrPortLibrary->cuda_linkerDestroy(privateOmrPortLibrary, (deviceId), (linker))
#define omrcuda_memcpy2DDeviceToDevice(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height) \
				privateOmrPortLibrary->cuda_memcpy2DDeviceToDevice(privateOmrPortLibrary, (deviceId), (targetAddress), (targetPitch), (sourceAddress), (sourcePitch), (width), (height))
#define omrcuda_memcpy2DDeviceToDeviceAsync(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream) \
				privateOmrPortLibrary->cuda_memcpy2DDeviceToDeviceAsync(privateOmrPortLibrary, (deviceId), (targetAddress), (targetPitch), (sourceAddress), (sourcePitch), (width), (height), (stream))
#define omrcuda_memcpy2DDeviceToHost(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height) \
				privateOmrPortLibrary->cuda_memcpy2DDeviceToHost(privateOmrPortLibrary, (deviceId), (targetAddress), (targetPitch), (sourceAddress), (sourcePitch), (width), (height))
#define omrcuda_memcpy2DDeviceToHostAsync(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream) \
				privateOmrPortLibrary->cuda_memcpy2DDeviceToHostAsync(privateOmrPortLibrary, (deviceId), (targetAddress), (targetPitch), (sourceAddress), (sourcePitch), (width), (height), (stream))
#define omrcuda_memcpy2DHostToDevice(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height) \
				privateOmrPortLibrary->cuda_memcpy2DHostToDevice(privateOmrPortLibrary, (deviceId), (targetAddress), (targetPitch), (sourceAddress), (sourcePitch), (width), (height))
#define omrcuda_memcpy2DHostToDeviceAsync(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, stream) \
				privateOmrPortLibrary->cuda_memcpy2DHostToDeviceAsync(privateOmrPortLibrary, (deviceId), (targetAddress), (targetPitch), (sourceAddress), (sourcePitch), (width), (height), (stream))
#define omrcuda_memcpyDeviceToDevice(deviceId, targetAddress, sourceAddress, byteCount) \
				privateOmrPortLibrary->cuda_memcpyDeviceToDevice(privateOmrPortLibrary, (deviceId), (targetAddress), (sourceAddress), (byteCount))
#define omrcuda_memcpyDeviceToDeviceAsync(deviceId, targetAddress, sourceAddress, byteCount, stream) \
				privateOmrPortLibrary->cuda_memcpyDeviceToDeviceAsync(privateOmrPortLibrary, (deviceId), (targetAddress), (sourceAddress), (byteCount), (stream))
#define omrcuda_memcpyDeviceToHost(deviceId, targetAddress, sourceAddress, byteCount) \
				privateOmrPortLibrary->cuda_memcpyDeviceToHost(privateOmrPortLibrary, (deviceId), (targetAddress), (sourceAddress), (byteCount))
#define omrcuda_memcpyDeviceToHostAsync(deviceId, targetAddress, sourceAddress, byteCount, stream) \
				privateOmrPortLibrary->cuda_memcpyDeviceToHostAsync(privateOmrPortLibrary, (deviceId), (targetAddress), (sourceAddress), (byteCount), (stream))
#define omrcuda_memcpyHostToDevice(deviceId, targetAddress, sourceAddress, byteCount) \
				privateOmrPortLibrary->cuda_memcpyHostToDevice(privateOmrPortLibrary, (deviceId), (targetAddress), (sourceAddress), (byteCount))
#define omrcuda_memcpyHostToDeviceAsync(deviceId, targetAddress, sourceAddress, byteCount, stream) \
				privateOmrPortLibrary->cuda_memcpyHostToDeviceAsync(privateOmrPortLibrary, (deviceId), (targetAddress), (sourceAddress), (byteCount), (stream))
#define omrcuda_memcpyPeer(targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount) \
				privateOmrPortLibrary->cuda_memcpyPeer(privateOmrPortLibrary, (targetDeviceId), (targetAddress), (sourceDeviceId), (sourceAddress), (byteCount))
#define omrcuda_memcpyPeerAsync(targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount, stream) \
				privateOmrPortLibrary->cuda_memcpyPeerAsync(privateOmrPortLibrary, (targetDeviceId), (targetAddress), (sourceDeviceId), (sourceAddress), (byteCount), (stream))
#define omrcuda_memset8(deviceId, deviceAddress, value, count) \
				omrcuda_memset8Async((deviceId), (deviceAddress), (value), (count), NULL)
#define omrcuda_memset8Async(deviceId, deviceAddress, value, count, stream) \
				privateOmrPortLibrary->cuda_memset8Async(privateOmrPortLibrary, (deviceId), (deviceAddress), (value), (count), (stream))
#define omrcuda_memset16(deviceId, deviceAddress, value, count) \
				omrcuda_memset16Async((deviceId), (deviceAddress), (value), (count), NULL)
#define omrcuda_memset16Async(deviceId, deviceAddress, value, count, stream) \
				privateOmrPortLibrary->cuda_memset16Async(privateOmrPortLibrary, (deviceId), (deviceAddress), (value), (count), (stream))
#define omrcuda_memset32(deviceId, deviceAddress, value, count) \
				omrcuda_memset32Async((deviceId), (deviceAddress), (value), (count), NULL)
#define omrcuda_memset32Async(deviceId, deviceAddress, value, count, stream) \
				privateOmrPortLibrary->cuda_memset32Async(privateOmrPortLibrary, (deviceId), (deviceAddress), (value), (count), (stream))
#define omrcuda_moduleGetFunction(deviceId, module, name, functionOut) \
				privateOmrPortLibrary->cuda_moduleGetFunction(privateOmrPortLibrary, (deviceId), (module), (name), (functionOut))
#define omrcuda_moduleGetGlobal(deviceId, module, name, addressOut, sizeOut) \
				privateOmrPortLibrary->cuda_moduleGetGlobal(privateOmrPortLibrary, (deviceId), (module), (name), (addressOut), (sizeOut))
#define omrcuda_moduleGetSurfaceRef(deviceId, module, name, surfRefOut) \
				privateOmrPortLibrary->cuda_moduleGetSurfaceRef(privateOmrPortLibrary, (deviceId), (module), (name), (surfRefOut))
#define omrcuda_moduleGetTextureRef(deviceId, module, name, texRefOut) \
				privateOmrPortLibrary->cuda_moduleGetTextureRef(privateOmrPortLibrary, (deviceId), (module), (name), (texRefOut))
#define omrcuda_moduleLoad(deviceId, image, options, moduleOut) \
				privateOmrPortLibrary->cuda_moduleLoad(privateOmrPortLibrary, (deviceId), (image), (options), (moduleOut))
#define omrcuda_moduleUnload(deviceId, module) \
				privateOmrPortLibrary->cuda_moduleUnload(privateOmrPortLibrary, (deviceId), (module))
#define omrcuda_runtimeGetVersion(versionOut) \
				privateOmrPortLibrary->cuda_runtimeGetVersion(privateOmrPortLibrary, (versionOut))
#define omrcuda_streamAddCallback(deviceId, stream, callback, userData) \
				privateOmrPortLibrary->cuda_streamAddCallback(privateOmrPortLibrary, (deviceId), (stream), (callback), (userData))
#define omrcuda_streamCreate(deviceId, streamOut) \
				privateOmrPortLibrary->cuda_streamCreate(privateOmrPortLibrary, (deviceId), (streamOut))
#define omrcuda_streamCreateWithPriority(deviceId, priority, flags, streamOut) \
				privateOmrPortLibrary->cuda_streamCreateWithPriority(privateOmrPortLibrary, (deviceId), (priority), (flags), (streamOut))
#define omrcuda_streamDestroy(deviceId, stream) \
				privateOmrPortLibrary->cuda_streamDestroy(privateOmrPortLibrary, (deviceId), (stream))
#define omrcuda_streamGetFlags(deviceId, stream, flagsOut) \
				privateOmrPortLibrary->cuda_streamGetFlags(privateOmrPortLibrary, (deviceId), (stream), (flagsOut))
#define omrcuda_streamGetPriority(deviceId, stream, priorityOut) \
				privateOmrPortLibrary->cuda_streamGetPriority(privateOmrPortLibrary, (deviceId), (stream), (priorityOut))
#define omrcuda_streamQuery(deviceId, stream) \
				privateOmrPortLibrary->cuda_streamQuery(privateOmrPortLibrary, (deviceId), (stream))
#define omrcuda_streamSynchronize(deviceId, stream) \
				privateOmrPortLibrary->cuda_streamSynchronize(privateOmrPortLibrary, (deviceId), (stream))
#define omrcuda_streamWaitEvent(deviceId, stream, event) \
				privateOmrPortLibrary->cuda_streamWaitEvent(privateOmrPortLibrary, (deviceId), (stream), (event))
#endif /* OMR_OPT_CUDA */

#endif /* !defined(OMRPORT_LIBRARY_DEFINE) */

/** @} */

#endif /* !defined(OMRPORT_H_) */
