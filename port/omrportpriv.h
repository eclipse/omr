/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#ifndef omrportlibraryprivatedefines_h
#define omrportlibraryprivatedefines_h

/*
 * @ddr_namespace: default
 */

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrmutex.h"
#include "omrportpg.h"
#include "omrport.h"
#include <signal.h>

#define FLAG_IS_SET(flag,bitmap)		(0 != (flag & bitmap))
#define FLAG_IS_NOT_SET(flag,bitmap) 	(0 == (flag & bitmap))

/**
 * The "std" file descriptors (fds), stderr,stdout and stdin, have special attributes on z/OS.
 * For example, close() and seek() cannot be called on them.
 * FD_BIAS is used to distinguish the "std" fds on z/OS from other fds.
 * CMVC 99667: Introducing FD-biasing to fix MVS started-tasks and POSIX file-descriptors
 */
#ifdef J9ZOS390
#define FD_BIAS 1000
#else
#define FD_BIAS 0
#endif

typedef struct J9PortControlData {
	uintptr_t sig_flags;
	OMRMemCategorySet language_memory_categories;
	OMRMemCategorySet omr_memory_categories;
#if defined(AIXPPC)
	uintptr_t aix_proc_attr;
#endif
} J9PortControlData;

#define J9NLS_NUM_HASH_BUCKETS  0x100

typedef struct J9NLSDataCache {
	char *baseCatalogPaths[4];
	uintptr_t nPaths;
	uintptr_t isDisabled;
	char *baseCatalogName;
	char *baseCatalogExtension;
	char *catalog;
	char language[4];
	char region[4];
	char variant[32];
	struct J9ThreadMonitor *monitor;
	struct J9NLSHashEntry *hash_buckets[J9NLS_NUM_HASH_BUCKETS];
	struct J9NLSHashEntry *old_hashEntries;
} J9NLSDataCache;

typedef struct J9NLSHashEntry {
	uint32_t module_name;
	uint32_t message_num;
	struct J9NLSHashEntry *next;
	char message[8];
} J9NLSHashEntry;

#if defined(OMR_OPT_CUDA)
/**
 * Structure for internal data required by omrcuda.
 */
typedef struct J9CudaGlobalData {
	/** The key for omrcuda thread-local storage. */
	omrthread_tls_key_t tlsKey;
	/** The internal state of the omrcuda layer. */
	uint32_t state;
	/** The CUDA driver version in use. */
	uint32_t driverVersion;
	/** The CUDA runtime version in use. */
	uint32_t runtimeVersion;
	/** The number of devices detected at initialization time. */
	uint32_t deviceCount;
	/** Protects state changes. */
	MUTEX stateMutex;
	/** The handle for the driver shared library. */
	uintptr_t driverHandle;
	/** The handle for the runtime shared library. */
	uintptr_t runtimeHandle;
	/** Pointers to driver and runtime functions. */
	void *functionTable[63];
} J9CudaGlobalData;
#endif /* OMR_OPT_CUDA */

/* these port library globals are initialized to zero in omrmem_startup_basic */
typedef struct OMRPortLibraryGlobalData {
	void *corruptedMemoryBlock;
	struct J9PortControlData control;
	struct J9NLSDataCache nls_data;
	omrthread_tls_key_t tls_key;
#if defined(OMR_PORT_SOCKET_SUPPORT)
	omrthread_tls_key_t socketTlsKey;
#endif /* defined(OMR_PORT_SOCKET_SUPPORT) */
	MUTEX tls_mutex;
	void *buffer_list;
	void *procSelfMap;
	struct OMRPortPlatformGlobals platformGlobals;
	OMRMemCategory unknownMemoryCategory;
	OMRMemCategory portLibraryMemoryCategory;
	uintptr_t disableEnsureCap32;
#if defined(OMR_ENV_DATA64)
	OMRMemCategory unusedAllocate32HeapRegionsMemoryCategory;
#endif
	uintptr_t vmemAdviseOSonFree;					/** For softmx to determine whether OS should be advised of freed vmem */
	uintptr_t vectorRegsSupportOn;				/* Turn on vector regs support */
	uintptr_t userSpecifiedCPUs;						/* Number of user-specified CPUs */
#if defined(OMR_OPT_CUDA)
	J9CudaGlobalData cudaGlobals;
#endif /* OMR_OPT_CUDA */
	uintptr_t vmemEnableMadvise;					/* madvise to use Transparent HugePage (THP) for Virtual memory allocated by mmap */
	J9SysinfoCPUTime oldestCPUTime;
	J9SysinfoCPUTime latestCPUTime;
} OMRPortLibraryGlobalData;

/* J9SourceJ9CPUControl*/
extern J9_CFUNC void
omrcpu_flush_icache(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount);
extern J9_CFUNC void
omrcpu_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrcpu_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrcpu_get_cache_line_size(struct OMRPortLibrary *portLibrary, int32_t *lineSize);

/* J9SourceJ9Error*/
extern J9_CFUNC const char *
omrerror_last_error_message(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrerror_set_last_error(struct OMRPortLibrary *portLibrary,  int32_t platformCode, int32_t portableCode);
extern J9_CFUNC int32_t
omrerror_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrerror_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrerror_set_last_error_with_message(struct OMRPortLibrary *portLibrary, int32_t portableCode, const char *errorMessage);
extern J9_CFUNC int32_t
omrerror_set_last_error_with_message_format(struct OMRPortLibrary *portLibrary, int32_t portableCode, const char *format, ...);
extern J9_CFUNC int32_t
omrerror_last_error_number(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9ErrorHelpers*/
extern J9_CFUNC const char *
errorMessage(struct OMRPortLibrary *portLibrary,  int32_t errorCode);

/* J9SourceJ9Exit*/
extern J9_CFUNC int32_t
omrexit_get_exit_code(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrexit_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrexit_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrexit_shutdown_and_exit(struct OMRPortLibrary *portLibrary, int32_t exitCode);

/* J9SourceJ9File*/
extern J9_CFUNC int32_t
omrfile_unlink(struct OMRPortLibrary *portLibrary, const char *path);
extern J9_CFUNC int32_t
omrfile_close(struct OMRPortLibrary *portLibrary, intptr_t fd);
extern J9_CFUNC int32_t
omrfile_mkdir(struct OMRPortLibrary *portLibrary, const char *path);
extern J9_CFUNC void
omrfile_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrfile_set_length(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength);
extern J9_CFUNC int32_t
omrfile_findnext(struct OMRPortLibrary *portLibrary, uintptr_t findhandle, char *resultbuf);
extern J9_CFUNC
int32_t omrfile_sync(struct OMRPortLibrary *portLibrary, intptr_t fd);
extern J9_CFUNC
int32_t omrfile_fstat(struct OMRPortLibrary *portLibrary, intptr_t fd, struct J9FileStat *buf);
extern J9_CFUNC
int32_t omrfile_stat(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStat *buf);
extern J9_CFUNC
int32_t omrfile_stat_filesystem(struct OMRPortLibrary *portLibrary, const char *path, uint32_t flags, struct J9FileStatFilesystem *buf);
extern J9_CFUNC void
omrfile_vprintf(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, va_list args);
extern J9_CFUNC int32_t
omrfile_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrfile_printf(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *format, ...);
extern J9_CFUNC int32_t
omrfile_unlinkdir(struct OMRPortLibrary *portLibrary, const char *path);
extern J9_CFUNC intptr_t
omrfile_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode);
extern J9_CFUNC uintptr_t
omrfile_findfirst(struct OMRPortLibrary *portLibrary, const char *path, char *resultbuf);
extern J9_CFUNC int32_t
omrfile_move(struct OMRPortLibrary *portLibrary, const char *pathExist, const char *pathNew);
extern J9_CFUNC int32_t
omrfile_attr(struct OMRPortLibrary *portLibrary, const char *path);
extern J9_CFUNC int32_t
omrfile_chmod(struct OMRPortLibrary *portLibrary, const char *path, int32_t mode);
extern J9_CFUNC intptr_t
omrfile_chown(struct OMRPortLibrary *portLibrary, const char *path, uintptr_t owner, uintptr_t group);
extern J9_CFUNC void
omrfile_findclose(struct OMRPortLibrary *portLibrary, uintptr_t findhandle);
extern J9_CFUNC intptr_t
omrfile_read(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes);
extern J9_CFUNC intptr_t
omrfile_write(struct OMRPortLibrary *portLibrary, intptr_t fd, const void *buf, intptr_t nbytes);
extern J9_CFUNC const char *
omrfile_error_message(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int64_t
omrfile_seek(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t offset, int32_t whence);
extern J9_CFUNC int64_t
omrfile_length(struct OMRPortLibrary *portLibrary, const char *path);
extern J9_CFUNC int64_t
omrfile_flength(struct OMRPortLibrary *portLibrary, intptr_t fd);
extern J9_CFUNC int64_t
omrfile_lastmod(struct OMRPortLibrary *portLibrary, const char *path);
extern J9_CFUNC int32_t
omrfile_lock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length);
extern J9_CFUNC int32_t
omrfile_unlock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length);
extern J9_CFUNC intptr_t
omrfile_convert_native_fd_to_omrfile_fd(struct OMRPortLibrary *portLibrary, intptr_t nativeFD);
extern J9_CFUNC intptr_t
omrfile_convert_omrfile_fd_to_native_fd(struct OMRPortLibrary *portLibrary, intptr_t nativeFD);

/* J9SourceJ9File_BlockingAsyncText*/
extern J9_CFUNC int32_t
omrfile_blockingasync_close(struct OMRPortLibrary *portLibrary, intptr_t fd);
extern J9_CFUNC intptr_t
omrfile_blockingasync_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode);
extern J9_CFUNC intptr_t
omrfile_blockingasync_read(struct OMRPortLibrary *portLibrary, intptr_t fd, void *buf, intptr_t nbytes);
extern J9_CFUNC intptr_t
omrfile_blockingasync_write(struct OMRPortLibrary *portLibrary, intptr_t fd, const void *buf, intptr_t nbytes);
extern J9_CFUNC int32_t
omrfile_blockingasync_lock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t lockFlags, uint64_t offset, uint64_t length);
extern J9_CFUNC int32_t
omrfile_blockingasync_unlock_bytes(struct OMRPortLibrary *portLibrary, intptr_t fd, uint64_t offset, uint64_t length);
extern J9_CFUNC int32_t
omrfile_blockingasync_set_length(struct OMRPortLibrary *portLibrary, intptr_t fd, int64_t newLength);
extern J9_CFUNC int64_t
omrfile_blockingasync_flength(struct OMRPortLibrary *portLibrary, intptr_t fd);
extern J9_CFUNC int32_t
omrfile_blockingasync_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrfile_blockingasync_shutdown(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9FileStream */
extern J9_CFUNC int32_t
omrfilestream_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrfilestream_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC OMRFileStream *
omrfilestream_open(struct OMRPortLibrary *portLibrary, const char *path, int32_t flags, int32_t mode);
extern J9_CFUNC int32_t
omrfilestream_close(struct OMRPortLibrary *portLibrary,  OMRFileStream *fileStream);
extern J9_CFUNC intptr_t
omrfilestream_write(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const void *buf, intptr_t nbytes);
extern J9_CFUNC intptr_t
omrfilestream_write_text(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *buf, intptr_t nbytes, int32_t toCode);
extern J9_CFUNC void
omrfilestream_vprintf(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, const char *format, va_list args);
extern J9_CFUNC void
omrfilestream_printf(struct OMRPortLibrary *portLibrary, OMRFileStream *filestream, const char *format, ...);
extern J9_CFUNC int32_t
omrfilestream_sync(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream);
extern J9_CFUNC int32_t
omrfilestream_setbuffer(struct OMRPortLibrary *portLibrary, OMRFileStream *fileStream, char *buffer, int32_t mode, uintptr_t size);
extern J9_CFUNC OMRFileStream *
omrfilestream_fdopen(struct OMRPortLibrary *portLibrary, intptr_t fd, int32_t flags);
extern J9_CFUNC intptr_t
omrfilestream_fileno(struct OMRPortLibrary *portLibrary, OMRFileStream *stream);

/* J9SourceJ9FileText*/
extern J9_CFUNC intptr_t
omrfile_write_text(struct OMRPortLibrary *portLibrary, intptr_t fd, const char *buf, intptr_t nbytes);
extern J9_CFUNC char *
omrfile_read_text(struct OMRPortLibrary *portLibrary, intptr_t fd, char *buf, intptr_t nbytes);
extern J9_CFUNC int32_t
omrfile_get_text_encoding(struct OMRPortLibrary *portLibrary, char *charsetName, uintptr_t nbytes);

/* J9SourceJ9Heap*/
extern J9_CFUNC struct J9Heap *
omrheap_create(struct OMRPortLibrary *portLibrary, void *heapBase, uintptr_t heapSize, uint32_t heapFlags);
extern J9_CFUNC void *
omrheap_allocate(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, uintptr_t byteAmount);
extern J9_CFUNC void
omrheap_free(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address);
extern J9_CFUNC void *
omrheap_reallocate(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address, uintptr_t byteAmount);
extern J9_CFUNC uintptr_t
omrheap_query_size(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, void *address);
extern J9_CFUNC BOOLEAN
omrheap_grow(struct OMRPortLibrary *portLibrary, struct J9Heap *heap, uintptr_t growAmount);


/* J9SourceJ9Mem*/
extern J9_CFUNC void
omrmem_deallocate_portLibrary_basic(void *memoryPointer);
extern J9_CFUNC void *
omrmem_allocate_memory_basic(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount);
extern J9_CFUNC void
omrmem_startup_basic(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize);
extern J9_CFUNC void *
omrmem_allocate_portLibrary_basic(uintptr_t byteAmount);
extern J9_CFUNC void *
omrmem_reallocate_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount);
extern J9_CFUNC void
omrmem_shutdown_basic(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrmem_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer);
extern J9_CFUNC void
omrmem_advise_and_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t memorySize);

/* J9SourceJ9MemTag*/
extern J9_CFUNC void
omrmem_deallocate_portLibrary(void *memoryPointer);
extern J9_CFUNC void *
omrmem_allocate_memory(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category);
extern J9_CFUNC int32_t
omrmem_startup(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize);
extern J9_CFUNC void *
omrmem_allocate_portLibrary(uintptr_t byteAmount);
extern J9_CFUNC void *
omrmem_reallocate_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount, const char *callsite, uint32_t category);
extern J9_CFUNC void
omrmem_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrmem_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer);
extern J9_CFUNC void
omrmem_advise_and_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer);
extern J9_CFUNC void *
omrmem_allocate_memory32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category);
extern J9_CFUNC void
omrmem_free_memory32(struct OMRPortLibrary *portLibrary, void *memoryPointer);
extern J9_CFUNC uintptr_t
omrmem_ensure_capacity32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount);

/* omrmemcategories.c */
extern J9_CFUNC OMRMemCategory *
omrmem_get_category(struct OMRPortLibrary *portLibrary, uint32_t categoryCode);
extern J9_CFUNC void
omrmem_walk_categories(struct OMRPortLibrary *portLibrary, OMRMemCategoryWalkState *state);
extern J9_CFUNC int32_t
omrmem_startup_categories(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrmem_shutdown_categories(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrmem_categories_increment_counters(OMRMemCategory *category, uintptr_t size);
extern J9_CFUNC void
omrmem_categories_decrement_counters(OMRMemCategory *category, uintptr_t size);
extern J9_CFUNC void
omrmem_categories_increment_bytes(OMRMemCategory *category, uintptr_t size);
extern J9_CFUNC void
omrmem_categories_decrement_bytes(OMRMemCategory *category, uintptr_t size);

/* J9SourceJ9MemoryMap*/
extern J9_CFUNC void
omrmmap_unmap_file(struct OMRPortLibrary *portLibrary, J9MmapHandle *handle);
extern J9_CFUNC int32_t
omrmmap_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrmmap_capabilities(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC J9MmapHandle *
omrmmap_map_file(struct OMRPortLibrary *portLibrary, intptr_t file, uint64_t offset, uintptr_t size, const char *mappingName, uint32_t flags, uint32_t category);
extern J9_CFUNC void
omrmmap_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrmmap_msync(struct OMRPortLibrary *portLibrary, void *start, uintptr_t length, uint32_t flags);
extern J9_CFUNC intptr_t
omrmmap_protect(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags);
extern J9_CFUNC uintptr_t
omrmmap_get_region_granularity(struct OMRPortLibrary *portLibrary, void *address);
extern J9_CFUNC void
omrmmap_dont_need(struct OMRPortLibrary *portLibrary, const void *startAddress, size_t length);

/* J9SourceJ9NLS*/
extern J9_CFUNC const char *
j9nls_get_language(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC const char *
j9nls_get_variant(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
j9nls_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
j9nls_vprintf(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, va_list args);
extern J9_CFUNC void
j9nls_set_catalog(struct OMRPortLibrary *portLibrary, const char **paths, const int nPaths, const char *baseName, const char *extension);
extern J9_CFUNC void
j9nls_set_locale(struct OMRPortLibrary *portLibrary, const char *lang, const char *region, const char *variant);
extern J9_CFUNC int32_t
j9nls_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
j9nls_free_cached_data(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
j9nls_printf(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, ...);
extern J9_CFUNC const char *
j9nls_lookup_message(struct OMRPortLibrary *portLibrary, uintptr_t flags, uint32_t module_name, uint32_t message_num, const char *default_string);
extern J9_CFUNC const char *
j9nls_get_region(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9NLSHelpers*/
extern J9_CFUNC void
nls_determine_locale(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9OSDump*/
extern J9_CFUNC uintptr_t
omrdump_create(struct OMRPortLibrary *portLibrary, char *filename, char *dumpType, void *userData);
extern J9_CFUNC int32_t
omrdump_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrdump_shutdown(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9PortControl*/
extern J9_CFUNC int32_t
omrport_control(struct OMRPortLibrary *portLibrary, const char *key, uintptr_t value);

/* J9SourceJ9PortTLSHelpers*/
extern J9_CFUNC void
omrport_tls_free(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrport_tls_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void *
omrport_tls_peek(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void *
omrport_tls_get(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrport_tls_shutdown(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9SI*/
extern J9_CFUNC intptr_t
omrsysinfo_process_exists(struct OMRPortLibrary *portLibrary, uintptr_t pid);
extern J9_CFUNC uintptr_t
omrsysinfo_get_egid(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrsysinfo_get_executable_name(struct OMRPortLibrary *portLibrary, const char *argv0, char **result);
extern J9_CFUNC uintptr_t
omrsysinfo_get_euid(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrsysinfo_get_groups(struct OMRPortLibrary *portLibrary, uint32_t **gidList, uint32_t categoryCode);
extern J9_CFUNC const char *
omrsysinfo_get_OS_type(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrsysinfo_get_memory_info(struct OMRPortLibrary *portLibrary, struct J9MemoryInfo *memInfo, ...);
extern J9_CFUNC int32_t
omrsysinfo_get_processor_info(struct OMRPortLibrary *portLibrary, J9ProcessorInfos *procInfo);
extern J9_CFUNC void
omrsysinfo_destroy_processor_info(struct OMRPortLibrary *portLibrary, J9ProcessorInfos *procInfos);
extern J9_CFUNC uint64_t
omrsysinfo_get_addressable_physical_memory(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint64_t
omrsysinfo_get_physical_memory(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint32_t
omrsysinfo_get_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t *limit);
extern J9_CFUNC uint32_t
omrsysinfo_set_limit(struct OMRPortLibrary *portLibrary, uint32_t resourceID, uint64_t limit);
extern J9_CFUNC uintptr_t
omrsysinfo_get_number_CPUs_by_type(struct OMRPortLibrary *portLibrary, uintptr_t type);
extern J9_CFUNC const char *
omrsysinfo_get_CPU_architecture(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrsysinfo_get_processor_description(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc);
extern J9_CFUNC BOOLEAN
omrsysinfo_processor_has_feature(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, uint32_t feature);
extern J9_CFUNC intptr_t
omrsysinfo_processor_set_feature(struct OMRPortLibrary *portLibrary, OMRProcessorDesc *desc, uint32_t feature, BOOLEAN enable);
extern J9_CFUNC const char *
omrsysinfo_get_OS_version(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrsysinfo_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrsysinfo_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrsysinfo_get_groupname(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length);
extern J9_CFUNC intptr_t
omrsysinfo_get_hostname(struct OMRPortLibrary *portLibrary, char *buffer, size_t length);
extern J9_CFUNC uintptr_t
omrsysinfo_get_pid(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uintptr_t
omrsysinfo_get_ppid(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrsysinfo_get_username(struct OMRPortLibrary *portLibrary, char *buffer, uintptr_t length);
extern J9_CFUNC intptr_t
omrsysinfo_get_env(struct OMRPortLibrary *portLibrary, const char *envVar, char *infoString, uintptr_t bufSize);
extern J9_CFUNC intptr_t
omrsysinfo_get_load_average(struct OMRPortLibrary *portLibrary, struct J9PortSysInfoLoadData *loadAverageData);
extern J9_CFUNC int32_t
omrsysinfo_limit_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state);
extern J9_CFUNC BOOLEAN
omrsysinfo_limit_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state);
extern J9_CFUNC int32_t
omrsysinfo_limit_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoLimitIteratorState *state, J9SysinfoUserLimitElement *limitElement);
extern J9_CFUNC int32_t
omrsysinfo_env_iterator_init(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, void *buffer, uintptr_t bufferSizeBytes);
extern J9_CFUNC BOOLEAN
omrsysinfo_env_iterator_hasNext(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state);
extern J9_CFUNC int32_t
omrsysinfo_env_iterator_next(struct OMRPortLibrary *portLibrary, J9SysinfoEnvIteratorState *state, J9SysinfoEnvElement *envElement);
extern J9_CFUNC intptr_t
omrsysinfo_get_CPU_utilization(struct OMRPortLibrary *portLibrary, struct J9SysinfoCPUTime *cpuTime);
extern J9_CFUNC intptr_t
omrsysinfo_get_CPU_load(struct OMRPortLibrary *portLibrary, double *cpuLoad);
extern J9_CFUNC void
omrsysinfo_set_number_user_specified_CPUs(struct OMRPortLibrary *portLibrary, uintptr_t number);
extern J9_CFUNC intptr_t
omrsysinfo_get_cwd(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen);
extern J9_CFUNC intptr_t
omrsysinfo_get_tmp(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, BOOLEAN ignoreEnvVariable);
extern J9_CFUNC int32_t 
omrsysinfo_get_open_file_count(struct OMRPortLibrary *portLibrary, uint64_t *count);
extern J9_CFUNC intptr_t
omrsysinfo_get_os_description(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc);
extern J9_CFUNC BOOLEAN
omrsysinfo_os_has_feature(struct OMRPortLibrary *portLibrary, struct OMROSDesc *desc, uint32_t feature);
extern J9_CFUNC BOOLEAN
omrsysinfo_os_kernel_info(struct OMRPortLibrary *portLibrary, struct OMROSKernelInfo *kernelInfo);
extern J9_CFUNC BOOLEAN 
omrsysinfo_cgroup_is_system_available(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint64_t
omrsysinfo_cgroup_get_available_subsystems(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint64_t 
omrsysinfo_cgroup_are_subsystems_available(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlags);
extern J9_CFUNC uint64_t
omrsysinfo_cgroup_get_enabled_subsystems(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint64_t
omrsysinfo_cgroup_enable_subsystems(struct OMRPortLibrary *portLibrary, uint64_t requestedSubsystems);
extern J9_CFUNC uint64_t
omrsysinfo_cgroup_are_subsystems_enabled(struct OMRPortLibrary *portLibrary, uint64_t subsystemFlags);
extern J9_CFUNC int32_t 
omrsysinfo_cgroup_get_memlimit(struct OMRPortLibrary *portLibrary, uint64_t *limit);
extern J9_CFUNC BOOLEAN
omrsysinfo_cgroup_is_memlimit_set(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC struct OMRCgroupEntry *
omrsysinfo_get_cgroup_subsystem_list(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC BOOLEAN
omrsysinfo_is_running_in_container(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrsysinfo_cgroup_subsystem_iterator_init(struct OMRPortLibrary *portLibrary, uint64_t subsystem, struct OMRCgroupMetricIteratorState *state);
extern J9_CFUNC BOOLEAN
omrsysinfo_cgroup_subsystem_iterator_hasNext(struct OMRPortLibrary *portLibrary, const struct OMRCgroupMetricIteratorState *state);
extern J9_CFUNC int32_t
omrsysinfo_cgroup_subsystem_iterator_metricKey(struct OMRPortLibrary *portLibrary, const struct OMRCgroupMetricIteratorState *state, const char **metricKey);
extern J9_CFUNC int32_t
omrsysinfo_cgroup_subsystem_iterator_next(struct OMRPortLibrary *portLibrary, struct OMRCgroupMetricIteratorState *state, struct OMRCgroupMetricElement *metricElement);
extern J9_CFUNC void
omrsysinfo_cgroup_subsystem_iterator_destroy(struct OMRPortLibrary *portLibrary, struct OMRCgroupMetricIteratorState *state);

/* J9SourceJ9Signal*/
extern J9_CFUNC int32_t
omrsig_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint32_t
omrsig_get_options(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags);
extern J9_CFUNC void
omrsig_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint32_t
omrsig_info_count(struct OMRPortLibrary *portLibrary, void *info, uint32_t category);
extern J9_CFUNC int32_t
omrsig_set_options(struct OMRPortLibrary *portLibrary, uint32_t options);
extern J9_CFUNC int32_t
omrsig_protect(struct OMRPortLibrary *portLibrary,  omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result);
extern J9_CFUNC int32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags);
extern J9_CFUNC int32_t
omrsig_set_single_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t portlibSignalFlag, void **oldOSHandler);
extern J9_CFUNC uint32_t
omrsig_map_os_signal_to_portlib_signal(struct OMRPortLibrary *portLibrary, uint32_t osSignalValue);
extern J9_CFUNC int32_t
omrsig_map_portlib_signal_to_os_signal(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag);
extern J9_CFUNC int32_t
omrsig_register_os_handler(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, void *newOSHandler, void **oldOSHandler);
extern J9_CFUNC BOOLEAN
omrsig_is_master_signal_handler(struct OMRPortLibrary *portLibrary, void *osHandler);
extern J9_CFUNC int32_t
omrsig_is_signal_ignored(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, BOOLEAN *isSignalIgnored);
extern J9_CFUNC uint32_t
omrsig_info(struct OMRPortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value);
extern J9_CFUNC int32_t
omrsig_set_reporter_priority(struct OMRPortLibrary *portLibrary, uintptr_t priority);
extern J9_CFUNC intptr_t
omrsig_get_current_signal(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9SL*/
extern J9_CFUNC uintptr_t
omrsl_lookup_name(struct OMRPortLibrary *portLibrary, uintptr_t descriptor, char *name, uintptr_t *func, const char *argSignature);
extern J9_CFUNC uintptr_t
omrsl_close_shared_library(struct OMRPortLibrary *portLibrary, uintptr_t descriptor);
extern J9_CFUNC int32_t
omrsl_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uintptr_t
omrsl_open_shared_library(struct OMRPortLibrary *portLibrary, char *name, uintptr_t *descriptor, uintptr_t flags);
extern J9_CFUNC void
omrsl_shutdown(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9Sock*/
#if defined(OMR_PORT_SOCKET_SUPPORT)
extern J9_CFUNC int32_t
omrsock_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrsock_getaddrinfo_create_hints(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t *hints, int32_t family, int32_t socktype, int32_t protocol, int32_t flags);
extern J9_CFUNC int32_t
omrsock_getaddrinfo(struct OMRPortLibrary *portLibrary, const char *node, const char *service, omrsock_addrinfo_t hints, omrsock_addrinfo_t result);
extern J9_CFUNC int32_t
omrsock_addrinfo_length(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t *result);
extern J9_CFUNC int32_t
omrsock_addrinfo_family(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index,int32_t *result);
extern J9_CFUNC int32_t
omrsock_addrinfo_socktype(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, int32_t *result);
extern J9_CFUNC int32_t
omrsock_addrinfo_protocol(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index,int32_t *result);
extern J9_CFUNC int32_t
omrsock_addrinfo_address(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle, uint32_t index, omrsock_sockaddr_t result);
extern J9_CFUNC int32_t
omrsock_freeaddrinfo(struct OMRPortLibrary *portLibrary, omrsock_addrinfo_t handle);
extern J9_CFUNC int32_t
omrsock_sockaddr_init(struct OMRPortLibrary *portLibrary, omrsock_sockaddr_t handle, int32_t family, uint8_t *addrNetworkOrder, uint16_t portNetworkOrder);
extern J9_CFUNC int32_t
omrsock_sockaddr_init6(struct OMRPortLibrary *portLibrary, omrsock_sockaddr_t handle, int32_t family, uint8_t *addrNetworkOrder, uint16_t portNetworkOrder, uint32_t flowinfo, uint32_t scope_id);
extern J9_CFUNC int32_t
omrsock_socket(struct OMRPortLibrary *portLibrary, omrsock_socket_t *sock, int32_t family, int32_t socktype, int32_t protocol);
extern J9_CFUNC int32_t
omrsock_bind(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr);
extern J9_CFUNC int32_t
omrsock_listen(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, int32_t backlog);
extern J9_CFUNC int32_t
omrsock_connect(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, omrsock_sockaddr_t addr);
extern J9_CFUNC int32_t
omrsock_accept(struct OMRPortLibrary *portLibrary, omrsock_socket_t serverSock, omrsock_sockaddr_t addrHandle, omrsock_socket_t *sockHandle);
extern J9_CFUNC int32_t
omrsock_send(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags);
extern J9_CFUNC int32_t
omrsock_sendto(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle);
extern J9_CFUNC int32_t
omrsock_recv(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags);
extern J9_CFUNC int32_t
omrsock_recvfrom(struct OMRPortLibrary *portLibrary, omrsock_socket_t sock, uint8_t *buf, int32_t nbyte, int32_t flags, omrsock_sockaddr_t addrHandle);
extern J9_CFUNC int32_t
omrsock_close(struct OMRPortLibrary *portLibrary, omrsock_socket_t *sock);
extern J9_CFUNC int32_t
omrsock_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint16_t
omrsock_htons(struct OMRPortLibrary *portLibrary, uint16_t val);
extern J9_CFUNC uint32_t
omrsock_htonl(struct OMRPortLibrary *portLibrary, uint32_t val);
extern J9_CFUNC int32_t
omrsock_inet_pton(struct OMRPortLibrary *portLibrary, int32_t addrFamily, const char *addr, uint8_t *addrNetworkOrder);
#endif /* defined(OMR_PORT_SOCKET_SUPPORT) */

/* J9SourceJ9Str*/
extern J9_CFUNC uintptr_t
omrstr_vprintf(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, va_list args);
extern J9_CFUNC uintptr_t
omrstr_printf(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, ...);
extern J9_CFUNC struct J9StringTokens *
omrstr_create_tokens(struct OMRPortLibrary *portLibrary, int64_t timeMillis);
extern J9_CFUNC intptr_t
omrstr_set_token(struct OMRPortLibrary *portLibrary, struct J9StringTokens *tokens, const char *key, const char *format, ...) ;
extern J9_CFUNC uintptr_t
omrstr_subst_tokens(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, struct J9StringTokens *tokens) ;
extern J9_CFUNC void
omrstr_free_tokens(struct OMRPortLibrary *portLibrary, struct J9StringTokens *tokens) ;
extern J9_CFUNC void
omrstr_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrstr_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrstr_set_time_tokens(struct OMRPortLibrary *portLibrary, struct J9StringTokens *tokens, int64_t timeMillis) ;
extern J9_CFUNC int32_t
omrstr_convert(struct OMRPortLibrary *portLibrary, int32_t fromCode, int32_t toCode, const char *inBuffer, uintptr_t inBufferSize, char *outBuffer, uintptr_t outBufferSize) ;

/* J9SourceJ9StrFTime*/
extern J9_CFUNC uintptr_t
omrstr_ftime(struct OMRPortLibrary *portLibrary, char *buf, uintptr_t bufLen, const char *format, int64_t timeMillis);

/* J9SourceJ9Time*/
extern J9_CFUNC uintptr_t
omrtime_usec_clock(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint64_t
omrtime_hires_clock(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint64_t
omrtime_hires_delta(struct OMRPortLibrary *portLibrary, uint64_t startTime, uint64_t endTime, uint64_t requiredResolution);
extern J9_CFUNC uint64_t
omrtime_hires_frequency(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrtime_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int64_t
omrtime_current_time_millis(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int64_t
omrtime_nano_time(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrtime_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uintptr_t
omrtime_msec_clock(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uint64_t
omrtime_current_time_nanos(struct OMRPortLibrary *portLibrary, uintptr_t *success);

/* J9SourceJ9TTY*/
extern J9_CFUNC void
omrtty_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrtty_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrtty_err_printf(struct OMRPortLibrary *portLibrary, const char *format, ...);
extern J9_CFUNC intptr_t
omrtty_get_chars(struct OMRPortLibrary *portLibrary, char *s, uintptr_t length);
extern J9_CFUNC void
omrtty_printf(struct OMRPortLibrary *portLibrary, const char *format, ...);
extern J9_CFUNC intptr_t
omrtty_available(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrtty_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args);
extern J9_CFUNC void
omrtty_err_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args);
extern J9_CFUNC void
omrtty_daemonize(struct OMRPortLibrary *portLibrary);

/* J9SourceJ9VMem*/
extern J9_CFUNC void
omrvmem_default_large_page_size_ex(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags);
extern J9_CFUNC intptr_t
omrvmem_find_valid_page_size(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags, BOOLEAN *isSizeSupported);
extern J9_CFUNC void *
omrvmem_commit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier);
extern J9_CFUNC intptr_t
omrvmem_decommit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier);
extern J9_CFUNC int32_t
omrvmem_vmem_params_init(struct OMRPortLibrary *portLibrary, struct J9PortVmemParams *params);
extern J9_CFUNC void *
omrvmem_reserve_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, uint32_t category);
extern J9_CFUNC void *
omrvmem_reserve_memory_ex(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, struct J9PortVmemParams *params);
extern J9_CFUNC void *
omrvmem_get_contiguous_region_memory(struct OMRPortLibrary *portLibrary, void* addresses[], uintptr_t addressesCount, uintptr_t addressSize, uintptr_t byteAmount, struct J9PortVmemIdentifier *oldIdentifier, struct J9PortVmemIdentifier *newIdentifier, uintptr_t mode, uintptr_t pageSize, OMRMemCategory *category);
extern J9_CFUNC uintptr_t
omrvmem_get_page_size(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier);
extern J9_CFUNC uintptr_t
omrvmem_get_page_flags(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier);
extern J9_CFUNC void
omrvmem_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrvmem_free_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier);
extern J9_CFUNC uintptr_t *
omrvmem_supported_page_sizes(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uintptr_t *
omrvmem_supported_page_flags(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrvmem_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC intptr_t
omrvmem_numa_set_affinity(struct OMRPortLibrary *portLibrary, uintptr_t numaNode, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier);
extern J9_CFUNC intptr_t
omrvmem_numa_get_node_details(struct OMRPortLibrary *portLibrary, struct J9MemoryNodeDetail *numaNodes, uintptr_t *nodeCount);
extern J9_CFUNC int32_t
omrvmem_get_available_physical_memory(struct OMRPortLibrary *portLibrary, uint64_t *freePhysicalMemorySize);
extern J9_CFUNC int32_t
omrvmem_get_process_memory_size(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize);

/* J9SourcePort*/
extern J9_CFUNC int32_t
omrport_shutdown_library(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC uintptr_t
omrport_getSize(void);
extern J9_CFUNC int32_t
omrport_allocate_library(struct OMRPortLibrary **portLibrary);
extern J9_CFUNC int32_t
omrport_isFunctionOverridden(struct OMRPortLibrary *portLibrary, uintptr_t offset);
extern J9_CFUNC int32_t
omrport_init_library(struct OMRPortLibrary *portLibrary, uintptr_t size);
extern J9_CFUNC int32_t
omrport_startup_library(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrport_create_library(struct OMRPortLibrary *portLibrary, uintptr_t size);

/* J9Syslog */
extern J9_CFUNC uintptr_t
omrsyslog_write(struct OMRPortLibrary *portLibrary, uintptr_t flags, const char *message);
extern J9_CFUNC uintptr_t
omrsyslog_query(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrsyslog_set(struct OMRPortLibrary *portLibrary, uintptr_t options);

/* J9Introspect */
extern J9_CFUNC int32_t
omrintrospect_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrintrospect_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrintrospect_set_suspend_signal_offset(struct OMRPortLibrary *portLibrary, int32_t signalOffset);
extern J9_CFUNC struct J9PlatformThread *
omrintrospect_threads_startDo(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state);
extern J9_CFUNC struct J9PlatformThread *
omrintrospect_threads_startDo_with_signal(struct OMRPortLibrary *portLibrary, J9Heap *heap, J9ThreadWalkState *state, void *signal_info);
extern J9_CFUNC struct J9PlatformThread *
omrintrospect_threads_nextDo(J9ThreadWalkState *state);
extern J9_CFUNC struct J9PlatformThread *
omrintrospect_threads_nextDo(J9ThreadWalkState *state);
extern J9_CFUNC uintptr_t
omrintrospect_backtrace_thread(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap, void *signalInfo);
extern J9_CFUNC uintptr_t
omrintrospect_backtrace_symbols(struct OMRPortLibrary *portLibrary, J9PlatformThread *threadInfo, J9Heap *heap);

/* omrcuda */
#if defined(OMR_OPT_CUDA)
extern J9_CFUNC int32_t
omrcuda_startup(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC void
omrcuda_shutdown(struct OMRPortLibrary *portLibrary);
extern J9_CFUNC int32_t
omrcuda_deviceAlloc(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uintptr_t size, void **deviceAddressOut);
extern J9_CFUNC int32_t
omrcuda_deviceCanAccessPeer(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId, BOOLEAN *canAccessPeerOut);
extern J9_CFUNC int32_t
omrcuda_deviceDisablePeerAccess(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId);
extern J9_CFUNC int32_t
omrcuda_deviceEnablePeerAccess(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId);
extern J9_CFUNC int32_t
omrcuda_deviceFree(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *devicePointer);
extern J9_CFUNC int32_t
omrcuda_deviceGetAttribute(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceAttribute attribute, int32_t *valueOut);
extern J9_CFUNC int32_t
omrcuda_deviceGetCacheConfig(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaCacheConfig *config);
extern J9_CFUNC int32_t
omrcuda_deviceGetCount(struct OMRPortLibrary *portLibrary, uint32_t *deviceCountOut);
extern J9_CFUNC int32_t
omrcuda_deviceGetLimit(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceLimit limit, uintptr_t *valueOut);
extern J9_CFUNC int32_t
omrcuda_deviceGetMemInfo(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uintptr_t *free, uintptr_t *total);
extern J9_CFUNC int32_t
omrcuda_deviceGetName(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t nameSize, char *nameOut);
extern J9_CFUNC int32_t
omrcuda_deviceGetSharedMemConfig(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaSharedMemConfig *config);
extern J9_CFUNC int32_t
omrcuda_deviceGetStreamPriorityRange(struct OMRPortLibrary *portLibrary, uint32_t deviceId, int32_t *leastPriority, int32_t *greatestPriority);
extern J9_CFUNC int32_t
omrcuda_deviceReset(struct OMRPortLibrary *portLibrary, uint32_t deviceId);
extern J9_CFUNC int32_t
omrcuda_deviceSetCacheConfig(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaCacheConfig config);
extern J9_CFUNC int32_t
omrcuda_deviceSetLimit(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceLimit limit, uintptr_t value);
extern J9_CFUNC int32_t
omrcuda_deviceSetSharedMemConfig(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaSharedMemConfig config);
extern J9_CFUNC int32_t
omrcuda_deviceSynchronize(struct OMRPortLibrary *portLibrary, uint32_t deviceId);
extern J9_CFUNC int32_t
omrcuda_driverGetVersion(struct OMRPortLibrary *portLibrary, uint32_t *versionOut);
extern J9_CFUNC int32_t
omrcuda_eventCreate(struct OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t flags, J9CudaEvent *event);
extern J9_CFUNC int32_t
omrcuda_eventDestroy(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaEvent event);
extern J9_CFUNC int32_t
omrcuda_eventElapsedTime(struct OMRPortLibrary *portLibrary, J9CudaEvent start, J9CudaEvent end, float *elapsedMillisOuts);
extern J9_CFUNC int32_t
omrcuda_eventQuery(struct OMRPortLibrary *portLibrary, J9CudaEvent event);
extern J9_CFUNC int32_t
omrcuda_eventRecord(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaEvent event, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_eventSynchronize(struct OMRPortLibrary *portLibrary, J9CudaEvent event);
extern J9_CFUNC int32_t
omrcuda_funcGetAttribute(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaFunctionAttribute attribute, int32_t *valueOut);
extern J9_CFUNC int32_t
omrcuda_funcMaxActiveBlocksPerMultiprocessor(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, uint32_t blockSize, uint32_t dynamicSharedMemorySize, uint32_t flags, uint32_t *valueOut);
extern J9_CFUNC int32_t
omrcuda_funcMaxPotentialBlockSize(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaBlockToDynamicSharedMemorySize dynamicSharedMemoryFunction, uintptr_t userData, uint32_t blockSizeLimit, uint32_t flags, uint32_t *minGridSizeOut, uint32_t *maxBlockSizeOut);
extern J9_CFUNC int32_t
omrcuda_funcSetCacheConfig(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaCacheConfig config);
extern J9_CFUNC int32_t
omrcuda_funcSetSharedMemConfig(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaSharedMemConfig config);
extern J9_CFUNC const char *
omrcuda_getErrorString(struct OMRPortLibrary *portLibrary, int32_t error);
extern J9_CFUNC int32_t
omrcuda_hostAlloc(struct OMRPortLibrary *portLibrary, uintptr_t size, uint32_t flags, void **hostAddressOut);
extern J9_CFUNC int32_t
omrcuda_hostFree(struct OMRPortLibrary *portLibrary, void *hostAddress);
extern J9_CFUNC int32_t
omrcuda_launchKernel(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, uint32_t gridDimX, uint32_t gridDimY, uint32_t gridDimZ, uint32_t blockDimX, uint32_t blockDimY, uint32_t blockDimZ, uint32_t sharedMemBytes, J9CudaStream stream, void **kernelParms);
extern J9_CFUNC int32_t
omrcuda_linkerAddData(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker, J9CudaJitInputType type, void *data, uintptr_t size, const char *name, J9CudaJitOptions *options);
extern J9_CFUNC int32_t
omrcuda_linkerComplete(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker, void **cubinOut, uintptr_t *sizeOut);
extern J9_CFUNC int32_t
omrcuda_linkerCreate(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaJitOptions *options, J9CudaLinker *linkerOut);
extern J9_CFUNC int32_t
omrcuda_linkerDestroy(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker);
extern J9_CFUNC int32_t
omrcuda_memcpy2DDeviceToDevice(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height);
extern J9_CFUNC int32_t
omrcuda_memcpy2DDeviceToDeviceAsync(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memcpy2DDeviceToHost(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height);
extern J9_CFUNC int32_t
omrcuda_memcpy2DDeviceToHostAsync(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memcpy2DHostToDevice(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height);
extern J9_CFUNC int32_t
omrcuda_memcpy2DHostToDeviceAsync(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memcpyDeviceToDevice(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount);
extern J9_CFUNC int32_t
omrcuda_memcpyDeviceToDeviceAsync(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memcpyDeviceToHost(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount);
extern J9_CFUNC int32_t
omrcuda_memcpyDeviceToHostAsync(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memcpyHostToDevice(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount);
extern J9_CFUNC int32_t
omrcuda_memcpyHostToDeviceAsync(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memcpyPeer(struct OMRPortLibrary *portLibrary, uint32_t targetDeviceId, void *targetAddress, uint32_t sourceDeviceId, const void *sourceAddress, uintptr_t byteCount);
extern J9_CFUNC int32_t
omrcuda_memcpyPeerAsync(struct OMRPortLibrary *portLibrary, uint32_t targetDeviceId, void *targetAddress, uint32_t sourceDeviceId, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memset8Async(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, U_8 value, uintptr_t count, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memset16Async(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, U_16 value, uintptr_t count, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_memset32Async(struct OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, uint32_t value, uintptr_t count, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_moduleGetFunction(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, J9CudaFunction *functionOut);
extern J9_CFUNC int32_t
omrcuda_moduleGetGlobal(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *addressOut, uintptr_t *sizeOut);
extern J9_CFUNC int32_t
omrcuda_moduleGetSurfaceRef(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *surfRefOut);
extern J9_CFUNC int32_t
omrcuda_moduleGetTextureRef(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *texRefOut);
extern J9_CFUNC int32_t
omrcuda_moduleLoad(struct OMRPortLibrary *portLibrary, uint32_t deviceId, const void *image, J9CudaJitOptions *options, J9CudaModule *moduleOut);
extern J9_CFUNC int32_t
omrcuda_moduleUnload(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module);
extern J9_CFUNC int32_t
omrcuda_runtimeGetVersion(struct OMRPortLibrary *portLibrary, uint32_t *runtimeVersion);
extern J9_CFUNC int32_t
omrcuda_streamAddCallback(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, J9CudaStreamCallback callback, uintptr_t userData);
extern J9_CFUNC int32_t
omrcuda_streamCreate(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream *streamOut);
extern J9_CFUNC int32_t
omrcuda_streamCreateWithPriority(struct OMRPortLibrary *portLibrary, uint32_t deviceId, int32_t priority, uint32_t flags, J9CudaStream *streamOut);
extern J9_CFUNC int32_t
omrcuda_streamDestroy(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_streamGetFlags(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, uint32_t *flags);
extern J9_CFUNC int32_t
omrcuda_streamGetPriority(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, int32_t *priority);
extern J9_CFUNC int32_t
omrcuda_streamQuery(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_streamSynchronize(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream);
extern J9_CFUNC int32_t
omrcuda_streamWaitEvent(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, J9CudaEvent event);
#endif /* OMR_OPT_CUDA */

#endif /* omrportlibraryprivatedefines_h */
