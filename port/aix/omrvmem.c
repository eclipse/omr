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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Virtual memory
 */

#include "omrcfg.h"
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"
#include "omrportasserts.h"
#include "omrvmem.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <string.h>

#include <sys/shm.h>
#include <sys/vminfo.h>
#include <unistd.h>

#include <sys/rset.h>
#include <dlfcn.h>
#include <procinfo.h>

#define INVALID_KEY -1

#if 0
#define J9VMEM_DEBUG
#endif

/* LIR 1394: Search for 64K page support and set it as default if it exists for AIX PPC. */
#define PREFERRED_DEFAULT_PAGE_SIZE SIXTY_FOUR_K

static void *getNextPossibleAddress(uintptr_t alignment, void *currentAddress, intptr_t direction);
static void *getMemoryInRangeForLargePages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, key_t addressKey, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions);
static void *getMemoryInRangeForDefaultPages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode);
static void *getMemoryInRangeForDefaultPagesUsingMmap(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode);
static void *allocateMemoryForLargePages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, void *currentAddress, key_t addressKey, OMRMemCategory *category, uintptr_t byteAmount);
static void *attemptToReserveInLargePages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t pageSize, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode);
static void *attemptToReserveInDefaultPages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t pageSize, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode);
static void *reserveLargePages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t pageSize, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode);
static BOOLEAN detectAndRecordPageSize(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, key_t addressKey, void *memoryPointer, OMRMemCategory *category, uintptr_t pageSize, uintptr_t byteAmount, uintptr_t vmemOptions, uintptr_t mode);
static uintptr_t getDataSegmentPageSize(struct OMRPortLibrary *portLibrary);

/* This ifdef is specifically provided to allow compilation on AIX 5.3 of features which we can only use on 6.1 */
#if !defined(__ENHANCED_AFFINITY)
#include <sys/systemcfg.h>
#define __ENHANCED_AFFINITY_MASK    0x1000
#define __ENHANCED_AFFINITY() \
           (_system_configuration.kernel & __ENHANCED_AFFINITY_MASK)

/* sys/processor.h */
typedef short  sradid_t;

/* sys/rset.h */
#define R_SRADSDL       R_MCMSDL
#define RS_SRADID_LOADAVG   2
#define R_PROCMEM       14
#define R_SRADID        13
#define R_MIGRATE_ASYNC 0x00000200
typedef struct loadavg_info {
	int load_average;
	int cpu_count;
} loadavg_info_t;

#ifndef _AIX61
/* create rsid_t_MODIFIED since rsid_t is already defined in 5.3 but is missing the at_sradid field */
typedef union {
	pid_t at_pid;           /* Process id (for R_PROCESS and R_PROCMEM */
	tid_t at_tid;           /* Kernel thread id (for R_THREAD) */
	int at_shmid;           /* Shared memory id (for R_SHM) */
	int at_fd;              /* File descriptor (for R_FILDES) */
	rsethandle_t at_rset;   /* Resource set handle (for R_RSET) */
	subrange_t *at_subrange;  /* Memory ranges (for R_SUBRANGE) */
	sradid_t at_sradid;     /* SRAD id (for R_SRADID) */
#ifdef _KERNEL
	ulong_t at_raw_val;     /* raw value: used to avoid copy typecasting */
#endif
} rsid_t_MODIFIED;
#endif
extern sradid_t rs_get_homesrad(void);
extern int rs_info(void *out, int command, long arg1, long arg2);
#ifndef _AIX61
extern int ra_attach(rstype_t, rsid_t, rstype_t, rsid_t_MODIFIED, uint_t);
#else
extern int ra_attach(rstype_t, rsid_t, rstype_t, rsid_t, uint_t);
#endif
/* sys/vminfo.h */
struct vm_srad_meminfo {
	int vmsrad_in_size;
	int vmsrad_out_size;
	size64_t vmsrad_total;
	size64_t vmsrad_free;
	size64_t vmsrad_file;
	int vmsrad_aff_priv_pct;
	int vmsrad_aff_avail_pct;
};
#define VM_SRAD_MEMINFO        106

#endif /* !defined(__ENHANCED_AFFINITY) */

void *default_pageSize_reserve_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, OMRMemCategory *category);
int get_default_pageSize_protectionBits(uintptr_t mode);
void update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address,  void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t allocator, OMRMemCategory *category);
static BOOLEAN rangeIsValid(struct J9PortVmemIdentifier *identifier, void *address, uintptr_t byteAmount);

void
omrvmem_shutdown(struct OMRPortLibrary *portLibrary)
{
	PPG_numa_platform_supports_numa = 0;
}

int32_t
omrvmem_startup(struct OMRPortLibrary *portLibrary)
{
	int num_psizes = 0;
	uint64_t physicalMemory = 0;

	/* 0 terminate the table */
	memset(PPG_vmem_pageSize, 0, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memset(PPG_vmem_pageFlags, 0, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

	/* First the default page size */
	PPG_vmem_pageSize[0] = (uintptr_t)getpagesize();
	PPG_vmem_pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
	num_psizes = -1;  /* vmgetinfo not supported by PASE yet */
#else
	/* try the new AIX 5.3E+ interface to get all supported page sizes */
	num_psizes = vmgetinfo(NULL, VMINFO_GETPSIZES, 0);
#endif

	if (num_psizes == -1) {
		/* assume OS does not support VMINFO_GETPSIZES */

		/* Now any large page sizes */
		if (-1 != sysconf(_SC_LARGE_PAGESIZE)) {
			PPG_vmem_pageSize[1] = (uintptr_t)sysconf(_SC_LARGE_PAGESIZE);
			PPG_vmem_pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		}
	} else {
		int counter = 0;
		psize_t psizes[OMRPORT_VMEM_PAGESIZE_COUNT];
		/* assert(num_psizes < OMRPORT_VMEM_PAGESIZE_COUNT); We don't want to do anything dangerous if this happens as it may, some day.
		 * We will use the earliest values in the set since they are the ones we should understand (if the OS adds new sizes at the beginning of the list
		 * there isn't much that we can do
		 */
		if (num_psizes >= OMRPORT_VMEM_PAGESIZE_COUNT) {
			num_psizes = OMRPORT_VMEM_PAGESIZE_COUNT - 1;
		}
#if !(defined(J9OS_I5) && defined(J9OS_I5_V5R4))
		vmgetinfo(psizes, VMINFO_GETPSIZES, num_psizes);
#endif
		physicalMemory = portLibrary->sysinfo_get_physical_memory(portLibrary);
		for (counter = 0;  counter < num_psizes; counter++) {
			if (psizes[counter] > physicalMemory) {
				/* Don't advertise pagesizes that exceed the physical memory on the machine. */
				Trc_PRT_vmem_omrvmem_startup_pagessize_exceeds_physical_memory(psizes[counter], physicalMemory);
				break;
			}
			PPG_vmem_pageSize[counter] = psizes[counter];
			PPG_vmem_pageFlags[counter] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		}
	}

	/* set default value to advise OS about vmem that is no longer needed */
	portLibrary->portGlobals->vmemAdviseOSonFree = 1;

	return 0;
}

void *
omrvmem_commit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	void *ptr = NULL;

	Trc_PRT_vmem_omrvmem_commit_memory_Entry(address, byteAmount);

	if (rangeIsValid(identifier, address, byteAmount)) {
		if (0 == (identifier->mode & OMRPORT_VMEM_MEMORY_MODE_EXECUTE)) {
			/* Assert that address and byteAmount are page aligned. These restrictions are not required for executable pages
			 * because when using default page size they are allocated using malloc()
			 * which may not return page aligned address and any attempt to align it to page boundary would waste one page of memory.
			 */
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);
		}

		if (PPG_vmem_pageSize[0] == identifier->pageSize) {
			if (0 == mprotect(address, byteAmount, get_default_pageSize_protectionBits(identifier->mode))) {
				ptr = address;
			} else {
				ptr = NULL;
			}
		} else {
			ptr = address;
		}
	} else {
		Trc_PRT_vmem_omrvmem_commit_memory_invalidRange(identifier->address, identifier->size, address, byteAmount);
		portLibrary->error_set_last_error(portLibrary,  -1, OMRPORT_ERROR_VMEM_INVALID_PARAMS);
	}

	Trc_PRT_vmem_omrvmem_commit_memory_Exit(ptr);
	return ptr;

	/*	return memset(address, 0, byteAmount); */
	/*
	 * CMVC 70354 change
	 * DANGER: memory is already zeroed when it gets faulted in so no memset is required.
	 * However, if the memory cannot actually be faulted in when needed, the VM will crash
	 * during execution in a much less obvious way.  Adding a VM flag to force the init is
	 * a RAS TODO.
	 */
}

intptr_t
omrvmem_decommit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	intptr_t result = -1;

	Trc_PRT_vmem_omrvmem_decommit_memory_Entry(address, byteAmount);

	if (portLibrary->portGlobals->vmemAdviseOSonFree == 1) {
		if (rangeIsValid(identifier, address, byteAmount)) {
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);

			if (byteAmount > 0) {
				if ((OMRPORT_VMEM_RESERVE_USED_J9MEM_ALLOCATE_MEMORY == identifier->allocator) || (OMRPORT_VMEM_RESERVE_USED_SHM == identifier->allocator)) {
					if ((FOUR_K == identifier->pageSize) || (SIXTY_FOUR_K == identifier->pageSize)) {
						unsigned long flags = DISCLAIM_ZEROMEM;

						Trc_PRT_vmem_decommit_memory_calling_disclaim(flags);
						result = (intptr_t)disclaim64((void *)address, (size_t) byteAmount, flags);
					} else {
						/* noop: both 16M and 16G pages are pinned in memory so don't disclaim them */
						result = 0;
					}
				} else if (identifier->allocator == OMRPORT_VMEM_RESERVE_USED_MMAP) {
					int flags = MS_INVALIDATE;

					Trc_PRT_vmem_decommit_memory_calling_msync(flags);
					result = (intptr_t)msync((void *)address, (size_t) byteAmount, flags);
				} else {
					/* should not get here */
					Trc_PRT_Assert_ShouldNeverHappen();
				}

				if (0 != result) {
					Trc_PRT_vmem_omrvmem_decommit_memory_failure(errno, address, byteAmount);
				}
			} else {
				/* nothing to de-commit. */
				Trc_PRT_vmem_decommit_memory_zero_pages();
				result = 0;
			}
		} else {
			result = -1;
			Trc_PRT_vmem_omrvmem_decommit_memory_invalidRange(identifier->address, identifier->size, address, byteAmount);
			portLibrary->error_set_last_error(portLibrary,  result, OMRPORT_ERROR_VMEM_INVALID_PARAMS);
		}
	} else {
		if (!rangeIsValid(identifier, address, byteAmount)) {
			result = -1;
			Trc_PRT_vmem_omrvmem_decommit_memory_invalidRange(identifier->address, identifier->size, address, byteAmount);
			portLibrary->error_set_last_error(portLibrary,  result, OMRPORT_ERROR_VMEM_INVALID_PARAMS);
		} else {
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);

			/* JVM is not allowed to decommit, just return success */
			Trc_PRT_vmem_decommit_memory_not_allowed(portLibrary->portGlobals->vmemAdviseOSonFree);
			result = 0;
		}
	}
	Trc_PRT_vmem_omrvmem_decommit_memory_Exit(result);
	return (intptr_t)result;
}

int32_t
omrvmem_free_memory(struct OMRPortLibrary *portLibrary, void *userAddress, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	int32_t result = 0;
	Trc_PRT_vmem_omrvmem_free_memory_Entry(userAddress, byteAmount);

	if (OMRPORT_VMEM_RESERVE_USED_J9MEM_ALLOCATE_MEMORY == identifier->allocator) {
		portLibrary->mem_free_memory(portLibrary, identifier->address);
		/* omrmem_categories_decrement_counters will be done by mem_free_memory */
	} else if (OMRPORT_VMEM_RESERVE_USED_MMAP == identifier->allocator) {
		result = (int32_t)munmap(identifier->address, byteAmount);
		omrmem_categories_decrement_counters(identifier->category, identifier->size);
	} else if (OMRPORT_VMEM_RESERVE_USED_SHM == identifier->allocator) {
		result = (int32_t)shmdt(identifier->address);
		omrmem_categories_decrement_counters(identifier->category, identifier->size);
	} else {
		result = (int32_t)OMRPORT_ERROR_VMEM_INVALID_PARAMS;
	}

	update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
	Trc_PRT_vmem_omrvmem_free_memory_Exit(result);
	return result;
}

int32_t
omrvmem_vmem_params_init(struct OMRPortLibrary *portLibrary, struct J9PortVmemParams *params)
{
	memset(params, 0, sizeof(struct J9PortVmemParams));
	params->startAddress = NULL;
	params->endAddress = OMRPORT_VMEM_MAX_ADDRESS;
	params->byteAmount = 0;
	params->pageSize = PPG_vmem_pageSize[0];
	params->pageFlags = PPG_vmem_pageFlags[0];
	params->mode = OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE;
	params->options = 0;
	params->category = OMRMEM_CATEGORY_UNKNOWN;
	params->alignmentInBytes = 0;
	return 0;
}

void *
omrvmem_reserve_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, uint32_t category)
{
	struct J9PortVmemParams params;
	omrvmem_vmem_params_init(portLibrary, &params);
	if (NULL != address) {
		params.startAddress = address;
		params.endAddress = address;
	}
	params.byteAmount = byteAmount;
	params.mode = mode;
	params.pageSize = pageSize;
	params.pageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	params.options = 0;
	params.category = category;
	return portLibrary->vmem_reserve_memory_ex(portLibrary, identifier, &params);
}

void *
omrvmem_reserve_memory_ex(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, struct J9PortVmemParams *params)
{
	/* Under AIX 5.2, a 32-bit process can request up to 3.25 GB with each shmget call (essentially all of the available memory), and
	 * a 64-bit process can request at least 1 TB.  We will consider 1 TB to be an acceptable restriction, and not resort to the additional
	 * complexity of multiple shmget calls.
	 */
	int pageSizeCheckIndex = 0;
	void *memoryPointer = NULL;
	/* this use of "localParams" is only in place until we implement native reserve alignment (this is just the implementation we use in callers) */
	OMRMemCategory *category = omrmem_get_category(portLibrary, params->category);

	Trc_PRT_vmem_omrvmem_reserve_memory_Entry_replacement(params->startAddress, params->byteAmount, params->pageSize);

#if defined(J9VMEM_DEBUG)
	printf("\n\tomrvmem_reserve_memory_ex byteAmount: %p, startAddress: %p, endAddress: %p, pageSize: 0x%zX, %s, %s, %s\n ",
		   params->byteAmount, params->startAddress, params->endAddress, params->pageSize,
		   (OMRPORT_VMEM_STRICT_PAGE_SIZE & params->options) ? "OMRPORT_VMEM_STRICT_PAGE_SIZE" : "\t",
		   (OMRPORT_VMEM_STRICT_ADDRESS & params->options) ? "OMRPORT_VMEM_STRICT_ADDRESS" : "\t",
		   (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & params->mode) ? "OMRPORT_VMEM_MEMORY_MODE_EXECUTE" : "\t");
#endif

	Assert_PRT_true(params->startAddress <= params->endAddress);
	ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(params->byteAmount, params->pageSize);

	if (0 == params->pageSize) {
		/* Invalid input */
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
		Trc_PRT_vmem_omrvmem_reserve_memory_invalid_input();
	} else if (PPG_vmem_pageSize[0] == params->pageSize) {
		/* Handle default page size differently, don't use shmget */
		uintptr_t alignment = OMR_MAX(params->pageSize, params->alignmentInBytes);
		uintptr_t minimumGranule = OMR_MIN(params->pageSize, params->alignmentInBytes);

		/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
		if ((0 == minimumGranule) || (0 == (alignment % minimumGranule))) {
			memoryPointer = getMemoryInRangeForDefaultPages(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress, alignment, params->options, params->mode);
		}
	} else {
		BOOLEAN requestedPageSizeIsSupported = FALSE;
		BOOLEAN preferedDefaultPageSizeIsSupported = FALSE;
		BOOLEAN preferedDefaultPageSizeIsRequested = (PREFERRED_DEFAULT_PAGE_SIZE == params->pageSize);

		/* If we were not called with the default page size, we must ensure that we were called with one of our understood sizes.
		 * AIX offers a total of 4 page sizes:
		 * 	4 kB (default)
		 *	64 kB (5.3E or later on POWER5+)
		 *	16 MB (legacy large page)
		 *	16 GB (very large page in 5.3E or later on POWER5+)
		 */
		/* make sure that this is valid page size for this configuration
		 * check that PREFERRED_DEFAULT_PAGE_SIZE is in list as well - it will be used if original requested allocation is not succeed
		 */
		while (0 != PPG_vmem_pageSize[pageSizeCheckIndex]) {
			if (params->pageSize == PPG_vmem_pageSize[pageSizeCheckIndex]) {
				/* this is requested page size - found */
				requestedPageSizeIsSupported = TRUE;
			}
			if (PREFERRED_DEFAULT_PAGE_SIZE == PPG_vmem_pageSize[pageSizeCheckIndex]) {
				/* this is default prefered page size - found */
				preferedDefaultPageSizeIsSupported = TRUE;
			}
			if (requestedPageSizeIsSupported && preferedDefaultPageSizeIsSupported) {
				/* both found - no reason to continue */
				break;
			}
			pageSizeCheckIndex++;
		}

		if (requestedPageSizeIsSupported) {
			/* try to allocate in requested pages first */
			memoryPointer = attemptToReserveInLargePages(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress,
							params->pageSize, params->alignmentInBytes, params->options | OMRPORT_VMEM_STRICT_PAGE_SIZE, params->mode);

			if ((NULL == memoryPointer) && (0 == (OMRPORT_VMEM_STRICT_PAGE_SIZE & params->options))) {
				if (preferedDefaultPageSizeIsSupported && !preferedDefaultPageSizeIsRequested) {
					/*
					 * If requested page is not supported or memory can not be allocated with such page size
					 * repeat an allocation with PREFERRED_DEFAULT_PAGE_SIZE
					 */
					memoryPointer = attemptToReserveInLargePages(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress,
									PREFERRED_DEFAULT_PAGE_SIZE, params->alignmentInBytes, params->options | OMRPORT_VMEM_STRICT_PAGE_SIZE, params->mode);
				}

				if (NULL == memoryPointer) {
					/* use shmat allocation type for default page size */
					memoryPointer = attemptToReserveInLargePages(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress,
									PPG_vmem_pageSize[0], params->alignmentInBytes, params->options, params->mode);
				}

				if (NULL == memoryPointer) {
					/*
					 * Try again with default size pages using another allocation methods
					 * Do it only if known page size allocation was requested - for compatibility with old code
					 */
#if defined(J9VMEM_DEBUG)
					printf("\t\t\t NULL == memoryPointer, reverting to default pages\n");
					fflush(stdout);
#endif
					memoryPointer = attemptToReserveInDefaultPages(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress,
									PPG_vmem_pageSize[0], params->alignmentInBytes, params->options, params->mode);
				}
			}
		}

		if (NULL == memoryPointer) {
			/* we failed to find the page size before hitting the null terminator so use the unsupported page size trace point and fail */
			update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
			if (!requestedPageSizeIsSupported) {
				Trc_PRT_vmem_omrvmem_reserve_memory_unsupported_page_size(params->pageSize);
			}
		}
	}

#if defined(J9VMEM_DEBUG)
	printf("\t\t omrvmem_reserve_memory_ex returning address: %p\n", memoryPointer);
#endif
	Trc_PRT_vmem_omrvmem_reserve_memory_Exit_replacement(memoryPointer, params->startAddress);
	return memoryPointer;
}

static void *
attemptToReserveInLargePages(struct OMRPortLibrary *portLibrary,
							 struct J9PortVmemIdentifier *identifier,
							 OMRMemCategory *category,
							 uintptr_t byteAmount,
							 void *startAddress,
							 void *endAddress,
							 uintptr_t pageSize,
							 uintptr_t alignmentInBytes,
							 uintptr_t vmemOptions,
							 uintptr_t mode)
{
	void *memoryPointer = NULL;

	/* large pages so we have to use the shm API */
	uintptr_t alignment = OMR_MAX(pageSize, alignmentInBytes);
	uintptr_t minimumGranule = OMR_MIN(pageSize, alignmentInBytes);

	/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
	if ((0 == minimumGranule) || (0 == (alignment % minimumGranule))) {
		memoryPointer = reserveLargePages(portLibrary, identifier, category, byteAmount, startAddress, endAddress, pageSize, alignment, vmemOptions, mode);
	}

	return memoryPointer;
}

static void *
attemptToReserveInDefaultPages(struct OMRPortLibrary *portLibrary,
							   struct J9PortVmemIdentifier *identifier,
							   OMRMemCategory *category,
							   uintptr_t byteAmount,
							   void *startAddress,
							   void *endAddress,
							   uintptr_t pageSize,
							   uintptr_t alignmentInBytes,
							   uintptr_t vmemOptions,
							   uintptr_t mode)
{
	void *memoryPointer = NULL;

	uintptr_t alignment = OMR_MAX(pageSize, alignmentInBytes);
	uintptr_t minimumGranule = OMR_MIN(pageSize, alignmentInBytes);

	/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
	if ((0 == minimumGranule) || (0 == (alignment % minimumGranule))) {
		memoryPointer = getMemoryInRangeForDefaultPages(portLibrary, identifier, category, byteAmount, startAddress, endAddress, alignment, vmemOptions, mode);
	}

	return memoryPointer;
}

static void *
reserveLargePages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t pageSize, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode)
{
	void *memoryPointer = NULL;
	key_t addressKey;
	int shmgetFlags = IPC_CREAT;

	/* Determine shmget flags */
	/* by this point, we know that we want to use large pages */

	/* set the access permissions */
	if (0 != (OMRPORT_VMEM_MEMORY_MODE_READ & mode)) {
		shmgetFlags |= S_IRUSR;
	}
	if (0 != (OMRPORT_VMEM_MEMORY_MODE_WRITE & mode)) {
		shmgetFlags |= S_IWUSR;
	}
	if (0 != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) {
		shmgetFlags |= S_IXUSR;
	}

	/* Reserve and attach memory */
	addressKey = shmget(IPC_PRIVATE, (size_t)byteAmount, shmgetFlags);
#if defined(J9VMEM_DEBUG)
	printf("\t\t omrvmem_reserve_memory_ex shmget(byteAmount 0x%zX, shmgetFlags = 0x%x) returning addressKey: 0x%zx\n", byteAmount, shmgetFlags, addressKey);
#endif
	if (-1 == addressKey) {
		Trc_PRT_vmem_omrvmem_reserve_memory_shmget_failed(byteAmount, shmgetFlags);
	} else {
		struct shmid_ds mutate;

		memset(&mutate, 0x0, sizeof(struct shmid_ds));
		mutate.shm_pagesize = pageSize;

		if (-1 == shmctl(addressKey, SHM_PAGESIZE, (struct shmid_ds *)&mutate)) {
			/* Failed to set page size, this will happen if the env var EXTSHM=ON is set
			 * It is safe to continue with the memory request.
			 * detectAndRecordPageSize() (below) will determine if the page size we get is acceptable */
#if defined(J9VMEM_DEBUG)
			printf("\t\t shmctl() failed\n");
#endif
		}

		memoryPointer = getMemoryInRangeForLargePages(portLibrary, identifier, addressKey, category, byteAmount, startAddress, endAddress, alignmentInBytes, vmemOptions);

		/* release when complete, protect from ^C or crash */
		if (0 == shmctl(addressKey, IPC_RMID, NULL)) {
			if (NULL != memoryPointer) {
				/* Detect and record the page size in the vmemIdentifier */
				if (FALSE == detectAndRecordPageSize(portLibrary, identifier, addressKey, memoryPointer, category, pageSize, byteAmount, vmemOptions, mode)) {
					omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier);
					memoryPointer = NULL;
				} else {
					if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & mode)) {
						if (NULL == omrvmem_commit_memory(portLibrary, memoryPointer, byteAmount, identifier)) {
							/* If the commit fails free the memory */
							omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier);
							memoryPointer = NULL;
						}
					}
				}
			}
		} else {
			/* if releasing addressKey fails detach memory to avoid leaks and fail */
			if (NULL != memoryPointer) {
				if (0 != shmdt(memoryPointer)) {
					Trc_PRT_vmem_omrvmem_reserve_memory_shmdt_failed(memoryPointer);
				}
			}
			Trc_PRT_vmem_omrvmem_reserve_memory_failure();
			memoryPointer = NULL;
		}
	}

	if (NULL == memoryPointer) {
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
	}

	Trc_PRT_vmem_omrvmem_reserve_memory_Exit_replacement(memoryPointer, startAddress);
#if defined(J9VMEM_DEBUG)
	printf("\t\t reserveLargePages returning address: 0x%zX\n", memoryPointer);
#endif
	return memoryPointer;
}

/**
 * Detects the page size of the memory pointed to by memoryPointer
 * and records it in the identifier.
 * @return 	TRUE upon success, FALSE if an error occurs or if the detected
 * 			page size is different from the requested page size and
 * 			OMRPORT_VMEM_STRICT_PAGE_SIZE is set
 */
static BOOLEAN
detectAndRecordPageSize(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, key_t addressKey, void *memoryPointer, OMRMemCategory *category, uintptr_t pageSize, uintptr_t byteAmount, uintptr_t vmemOptions, uintptr_t mode)
{
	struct vm_page_info page_info;
	int vmgetinfoResult;
	BOOLEAN vmgetinfoFailedFirstTime = FALSE;
#if !(defined(J9OS_I5) && defined(J9OS_I5_V5R4))
	/* Ensure we got back the correct page size if OMRPORT_VMEM_STRICT_PAGE_SIZE is set */
	page_info.addr = (uint64_t) memoryPointer;
	vmgetinfoResult = vmgetinfo(&page_info, VM_PAGE_INFO, sizeof(struct vm_page_info));

	if (0 != vmgetinfoResult) {
		vmgetinfoFailedFirstTime = TRUE;
		page_info.addr = ((uint64_t) memoryPointer) + byteAmount - 1;
		/* TODO: Find out why original author of code thought that vmgetinfo
		 * may sometimes fail
		 */
		vmgetinfoResult = vmgetinfo(&page_info, VM_PAGE_INFO, sizeof(struct vm_page_info));
	}

	/* Update identifier and commit memory if required, else return reserved memory */
	update_vmemIdentifier(identifier, memoryPointer, (void *)addressKey, byteAmount, mode, (uintptr_t)page_info.pagesize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, OMRPORT_VMEM_RESERVE_USED_SHM, category);

	if (0 != vmgetinfoResult) {
		Trc_PRT_vmem_omrvmem_reserve_memory_ex_aix_vmgetinfo_failed(page_info.addr);
		return FALSE;
	} else if (page_info.pagesize != pageSize) {
		/* if we got here then one of the calls to vmgetinfo succeeded */

#if defined(J9VMEM_DEBUG)
		if (!vmgetinfoFailedFirstTime) {
			portLibrary->tty_printf(portLibrary, "Warning: large pages not available\n");
		} else {
			portLibrary->tty_printf(portLibrary, "Warning: not enough large pages to satisfy entire allocation\n");
		}
#endif

		if (0 != (OMRPORT_VMEM_STRICT_PAGE_SIZE & vmemOptions)) {
			return FALSE;
		}
	}
#else
	/* Update identifier and commit memory if required, else return reserved memory */
	update_vmemIdentifier(identifier, memoryPointer, (void *)addressKey, byteAmount, mode, pageSize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, OMRPORT_VMEM_RESERVE_USED_SHM, category);
#endif /* !(defined(J9OS_I5) && defined(J9OS_I5_V5R4)) */
	return TRUE;
}

uintptr_t
omrvmem_get_page_size(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier)
{
	return identifier->pageSize;
}

uintptr_t
omrvmem_get_page_flags(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier)
{
	return identifier->pageFlags;
}

uintptr_t *
omrvmem_supported_page_sizes(struct OMRPortLibrary *portLibrary)
{
	return PPG_vmem_pageSize;
}

uintptr_t *
omrvmem_supported_page_flags(struct OMRPortLibrary *portLibrary)
{
	return PPG_vmem_pageFlags;
}

/**
 * @internal
 * Update J9PortVmIdentifier structure
 *
 * @param[in] identifier The structure to be updated
 * @param[in] address Base address
 * @param[in] handle Platform specific handle for reserved memory
 * @param[in] byteAmount Size of allocated area
 * @param[in] mode Access Mode
 * @param[in] pageSize Constant describing pageSize
 * @param[in] allocator Constant describing how the virtual memory was allocated.
 * @param[in] category Pointer to memory category
 */
void update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address,  void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t allocator, OMRMemCategory *category)
{
	identifier->address = address;
	identifier->handle = handle;
	identifier->size = byteAmount;
	identifier->pageSize = pageSize;
	identifier->pageFlags = pageFlags;
	identifier->mode = mode;
	identifier->allocator = allocator;
	identifier->category = category;
}

/**
 * @internal
 * @see omrvmem_reserve_memory
 *
 * @param[in] portLibrary The port library.
 * @param[in] address The starting address of the memory to be reserved.
 * @param[in] byteAmount The number of bytes to be reserved.
 * @param[in] identifier Descriptor for virtual memory block.
 * @param[in] mode Bitmap indicating how memory is to be reserved.  Expected values combination of:
 * @param[in] category Memory category
 *
 * @return pointer to the reserved memory on success, NULL on failure.
 *
 * @internal @warning Do not call error handling code @ref omrerror upon error as
 * the error handling code uses per thread buffers to store the last error.  If memory
 * can not be allocated the result would be an infinite loop.
 */
void *
default_pageSize_reserve_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, OMRMemCategory *category)
{
	int fd = -1;
	int flags = MAP_SHARED;
	int protMask;
	void *result = NULL;

	Trc_PRT_vmem_default_reserve_entry(address, byteAmount);

	if (0 != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) {
		/* Allocate code memory  */
		result = portLibrary->mem_allocate_memory(portLibrary, byteAmount, OMR_GET_CALLSITE(), category->categoryCode);
#if defined(J9VMEM_DEBUG)
		printf("\t\t mem_allocate_memory(byteAmount = 0x%zx) returned 0x%zx \n", byteAmount, result);
#endif
		if (NULL == result) {
			Trc_PRT_vmem_default_reserve_failed(address, byteAmount);
			update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
			Trc_PRT_vmem_default_reserve_exit(result, address, byteAmount);
			return NULL;
		}
		/* mem_allocate_memory will have called omrmem_category_increment_counters() - we don't need to do so */
		update_vmemIdentifier(identifier, result, result, byteAmount, mode, pageSize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, OMRPORT_VMEM_RESERVE_USED_J9MEM_ALLOCATE_MEMORY, category);
		Trc_PRT_vmem_default_reserve_exit(result, address, byteAmount);
		return result;
	}

#if defined(MAP_ANONYMOUS)
	flags |= MAP_ANONYMOUS;
#else
	fd = portLibrary->file_open(portLibrary, "/dev/zero", EsOpenRead | EsOpenWrite, 0);
	if (-1 != fd)
#endif
	{
		/* Attributes requested */
		protMask = get_default_pageSize_protectionBits(mode);

		if (address != NULL) {
			flags |= MAP_FIXED;
		}

		/* mmap and protect the memory */
		result = mmap(address, (size_t)byteAmount, protMask, flags, fd, 0);
#if defined(J9VMEM_DEBUG)
		printf("\t\t mmap(address = 0x%xz, byteAmount = 0x%zx) returned 0x%zx \n", address, byteAmount, result);
#endif

#if !defined(MAP_ANONYMOUS)
		portLibrary->file_close(portLibrary, fd);
#endif

		if (MAP_FAILED == result) {
			result = NULL;
		} else {
			omrmem_categories_increment_counters(category, byteAmount);
			update_vmemIdentifier(identifier, result, result, byteAmount, mode, pageSize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, OMRPORT_VMEM_RESERVE_USED_MMAP, category);
			if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & mode)) {
				if (NULL == omrvmem_commit_memory(portLibrary, result, byteAmount, identifier)) {
					/* If the commit fails free the memory */
					omrvmem_free_memory(portLibrary, result, byteAmount, identifier);
					result = NULL;
				}
			}
		}
	}

	if (NULL == result) {
		Trc_PRT_vmem_default_reserve_failed(address, byteAmount);
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
	}

	Trc_PRT_vmem_default_reserve_exit(result, address, byteAmount);
	return result;
}

/**
 * Get protection bits when reserving memory with the default page size
 *
 * @param[in] mode Bitmap indicating how memory is to be reserved.  Expected values combination of:
 * \arg OMRPORT_VMEM_MEMORY_MODE_READ memory is readable
 * \arg OMRPORT_VMEM_MEMORY_MODE_WRITE memory is writable
 * \arg OMRPORT_VMEM_MEMORY_MODE_EXECUTE memory is executable
 * \arg OMRPORT_VMEM_MEMORY_MODE_COMMIT commits memory as part of the reserve
 *
 * @return the protection flags required by the functions used to reserve memory for default page size
 */
int
get_default_pageSize_protectionBits(uintptr_t mode)
{
	int protectionFlags = 0;

	if (0 != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) {
		protectionFlags |= PROT_EXEC;
	}
	if (0 != (OMRPORT_VMEM_MEMORY_MODE_READ & mode)) {
		protectionFlags |= PROT_READ;
	}
	if (0 != (OMRPORT_VMEM_MEMORY_MODE_WRITE & mode)) {
		protectionFlags |= PROT_WRITE;
	}
	if (0 == protectionFlags) {
		protectionFlags = PROT_NONE;
	}

	return protectionFlags;
}

/**
 * Helper routine to get page size of data segment.
 * To get data segment page size, retrieve size of the page backing portLibrary->portGlobals
 * which is allocated using omrmem_allocate_memory() in port/unix/omrmem.c
 *
 * @param[in] portLibrary The port library.
 *
 * @return page size of data segment.
 */
static uintptr_t
getDataSegmentPageSize(struct OMRPortLibrary *portLibrary)
{
	intptr_t rc = -1;
	uintptr_t pageSize = 0;
	struct vm_page_info pageInfo;

	pageInfo.addr = (uint64_t) portLibrary->portGlobals;
	rc = vmgetinfo(&pageInfo, VM_PAGE_INFO, sizeof(struct vm_page_info));
	if (0 == rc) {
		pageSize = pageInfo.pagesize;
	} else {
		/* vmgetinfo() can fail with VM_PAGE_INFO only if vm_page_info.addr is invalid,
		 * which should never happen here as portLibrary->portGlobals is valid until VM shutdown.
		 */
		Assert_PRT_ShouldNeverHappen_wrapper();
	}
	return pageSize;
}

void
omrvmem_default_large_page_size_ex(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags)
{
	if (NULL != pageSize) {
		if (OMRPORT_VMEM_MEMORY_MODE_EXECUTE != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) {
			/* AIX 5.3E (or later) on POWER5+ supports 4 kiB, 64 kiB, 16 MiB, 16 GiB page sizes (in that order).
			 * To be compatible with how the -Xlp option has historically worked, 16 MiB is the default page size.
			 * If, for some reason, that page size can't be found, we search for 64KB page size.
			 * If that is also not found, we will return the page size at [1] since failing is
			 * probably not a reasonable answer as it would break our support for that legacy option (and that
			 * situation should never occur).
			 */
			uintptr_t preferredPageSize = PPG_vmem_pageSize[1];
			uint32_t index = 0;

			while (0 != PPG_vmem_pageSize[index]) {
				if (SIXTEEN_M == PPG_vmem_pageSize[index]) {
					preferredPageSize = SIXTEEN_M;
					break;
				}
				if (SIXTY_FOUR_K == PPG_vmem_pageSize[index]) {
					preferredPageSize = SIXTY_FOUR_K;
					/* We don't break here because preferred page size is 16M. So keep looking for 16M. */
				}
				index++;
			}
			*pageSize = preferredPageSize;
		} else {
			/* Return the page size of data segment.
			 * This is the same page size as used by JIT code cache when allocated using omrmem_allocate_memory().
			 */
			uintptr_t dataSegmentPageSize = getDataSegmentPageSize(portLibrary);
			*pageSize = (FOUR_K != dataSegmentPageSize)? dataSegmentPageSize : 0;
		}

		if (NULL != pageFlags) {
			*pageFlags = (0 == *pageSize) ? 0 : OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		}
	} else {
		if (NULL != pageFlags) {
			*pageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		}
	}

	return;
}

intptr_t
omrvmem_find_valid_page_size(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags, BOOLEAN *isSizeSupported)
{
	uintptr_t validPageSize = *pageSize;
	uintptr_t validPageFlags = *pageFlags;
	uintptr_t defaultLargePageSize = 0;
	uintptr_t defaultLargePageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	Assert_PRT_true_wrapper(OMRPORT_VMEM_PAGE_FLAG_NOT_USED == validPageFlags);

	if (0 != validPageSize) {
		/* For executable pages search through the list of supported page sizes only if
		 * - request is for 16M pages, and
		 * - 64 bit system OR (32 bit system AND CodeCacheConsolidation is enabled)
		 */
#if defined(OMR_ENV_DATA64)
		if ((OMRPORT_VMEM_MEMORY_MODE_EXECUTE != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) ||
			(SIXTEEN_M == validPageSize))
#else
		BOOLEAN codeCacheConsolidationEnabled = FALSE;

		/* Check if the TR_ppcCodeCacheConsolidationEnabled env variable is set.
		 * TR_ppcCodeCacheConsolidationEnabled is a manually set env variable used to indicate
		 * that Code Cache Consolidation is enabled in the JIT.
		 */
		if (OMRPORT_VMEM_MEMORY_MODE_EXECUTE == (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) {
			if (portLibrary->sysinfo_get_env(portLibrary, "TR_ppcCodeCacheConsolidationEnabled", NULL, 0) != -1) {
				codeCacheConsolidationEnabled = TRUE;
			}
		}
		if ((OMRPORT_VMEM_MEMORY_MODE_EXECUTE != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) ||
			((TRUE == codeCacheConsolidationEnabled) && (SIXTEEN_M == validPageSize)))
#endif /* defined(OMR_ENV_DATA64) */
		{
			uintptr_t pageIndex = 0;
			uintptr_t *supportedPageSizes = portLibrary->vmem_supported_page_sizes(portLibrary);
			uintptr_t *supportedPageFlags = portLibrary->vmem_supported_page_flags(portLibrary);

			for (pageIndex = 0; 0 != supportedPageSizes[pageIndex]; pageIndex++) {
				if ((supportedPageSizes[pageIndex] == validPageSize) &&
					(supportedPageFlags[pageIndex] == validPageFlags)
				) {
					goto _end;
				}
			}
		}
	}

	portLibrary->vmem_default_large_page_size_ex(portLibrary, mode, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		validPageSize = defaultLargePageSize;
		validPageFlags = defaultLargePageFlags;
	} else {
		if (OMRPORT_VMEM_MEMORY_MODE_EXECUTE == (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) {
			validPageSize = getDataSegmentPageSize(portLibrary);
			validPageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
		} else {
			validPageSize = PPG_vmem_pageSize[0];
			validPageFlags = PPG_vmem_pageFlags[0];
		}
	}

_end:
	/* Since page type is not used, ignore pageFlags when setting isSizeSupported. */
	*isSizeSupported = (*pageSize == validPageSize);
	*pageSize = validPageSize;
	*pageFlags = validPageFlags;

	return 0;
}

/**
 * This function returns the next possible address at which memory may
 * be allocated as documented on aix publib article General Programming Concepts:
 * Writing and Debugging Programs Understanding Memory Mapping. URL =>
 * http://publib.boulder.ibm.com/infocenter/systems/index.jsp?topic=/com.ibm.aix.genprogc/doc/genprogc/understanding_mem_mapping.htm
 *
 * @param[in] alignment The required alignment of the address to return (always a multiple of the page size being used)
 * @param[in] currentAddress	The last address at which we attempted to allocate memory
 * @param[in] direction	The direction to follow when scanning through the address range
 * 						1 = bottom-up
 * 						-1 = top-down
 */
static void *
getNextPossibleAddress(uintptr_t alignment, void *currentAddress, intptr_t direction)
{
	void *oldAddress = currentAddress;

#if defined(OMR_ENV_DATA64)
	uintptr_t ranges[][2] = {{0x30000000, 0x07FFFFFFFFFFFFFF}};
	int numRanges = sizeof(ranges) / (sizeof(uintptr_t) * 2);

	/* Check to see if we will not be within a valid range,
	 * if not either move to a valid range or fail
	 */
	if (direction == 1) {
		/* Bottom-Up */
		if ((uintptr_t)currentAddress < ranges[0][0]) {
			/* We are below the lowest possible address, jump up
			 * We also need to subtract the pageSize because it will
			 * be added later on in the function
			 */
			currentAddress = (void *)(ranges[0][0] - alignment);
		} else if ((uintptr_t)currentAddress > ranges[numRanges - 1][1]) {
			/* We are above the highest possible address, fail */
			currentAddress = NULL;
		} else {
			/* We are somewhere in between */
			int i;
			for (i = 0 ; i < (numRanges - 1); i++) {
				if (((uintptr_t)currentAddress > ranges[i][1]) && ((uintptr_t)currentAddress < ranges[i + 1][0])) {
					/* We are between two valid ranges, jump UP to the next range
					 * We also need to subtract the pageSize because it will
					 * be added later on in the function
					 */
					currentAddress = (void *)(ranges[i + 1][0] - alignment);
					break;
				}
			}

		}
	} else {
		/* Top-Down */
		if ((uintptr_t)currentAddress < (ranges[0][0] + alignment)) {
			/* We are below the lowest possible address, fail */
			currentAddress = NULL;
		} else if ((uintptr_t)currentAddress > ranges[numRanges - 1][1]) {
			/* We are above the highest possible address, jump down */
			currentAddress = (void *) ranges[numRanges - 1][1];
		} else {
			/* We are somewhere in between */
			int i;
			for (i = 0 ; i < (numRanges - 1); i++) {
				if (((uintptr_t)currentAddress > ranges[i][1]) && ((uintptr_t)currentAddress <= ranges[i + 1][0])) {
					/* We are between two valid ranges, jump DOWN to the next range */
					currentAddress = (void *) ranges[i][1];
					break;
				}
			}
		}
	}


	if (currentAddress != NULL) {
		currentAddress = (void *)((uintptr_t)currentAddress & ~(alignment - 1)); /* align */
		currentAddress += direction * alignment;
	}
#else
	currentAddress = (void *)((uintptr_t)currentAddress & ~(alignment - 1)); /* align */
	currentAddress += direction * alignment;
#endif /* OMR_ENV_DATA64 */

	/* protect against loop around */
	if (((1 == direction) && ((uintptr_t)oldAddress > (uintptr_t)currentAddress)) ||
		((-1 == direction) && ((uintptr_t)oldAddress < (uintptr_t)currentAddress))
	) {
		currentAddress = NULL;
	}
	return currentAddress;
}

/**
 * Allocates memory in specified range using a best effort approach
 * (unless OMRPORT_VMEM_STRICT_ADDRESS flag is used) and returns a pointer
 * to the newly allocated memory. Returns NULL on failure.
 */
static void *
getMemoryInRangeForLargePages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, key_t addressKey, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions)
{
	intptr_t direction = 1;
	void *currentAddress = startAddress;
	void *memoryPointer = NULL;

	/* the max is a multiple of the min so we can safely proceed */
	/* check allocation direction */
	if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) {
		direction = -1;
		/* minimally, the first request must be aligned */
		currentAddress = (void *)((uintptr_t)endAddress & ~(alignmentInBytes - 1)); /* align */
	} else if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP)) {
		if (startAddress == NULL) {
			currentAddress = getNextPossibleAddress(alignmentInBytes, NULL, direction);
		}
	} else if (NULL == startAddress) {
		if (OMRPORT_VMEM_MAX_ADDRESS == endAddress) {
			/* if caller specified the entire address range and does not care about the direction
			 * save time by letting OS choose where to allocate the memory
			 */
			goto allocAnywhere;
		} else {
			/* if the startAddress is NULL but the endAddress is not we
			 * need to change the startAddress so that we have a better chance
			 * of getting memory within the range because NULL means don't care
			 */
			currentAddress = (void *)alignmentInBytes;
		}
	}

	/* prevent while loop from attempting to free memory on first entry */
	memoryPointer = MAP_FAILED;

	/* try all addresses within range */
	while ((startAddress <= currentAddress) && (endAddress >= currentAddress)) {
		/* free previously attached memory and attempt to get new memory */
		if (MAP_FAILED != memoryPointer) {
			if (0 != omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier)) {
				return NULL;
			}
		}

		memoryPointer = allocateMemoryForLargePages(portLibrary, identifier, currentAddress, addressKey, category, byteAmount);

		/* stop if returned pointer is within range */
		if ((MAP_FAILED != memoryPointer) && (startAddress <= memoryPointer) && (endAddress >= memoryPointer)) {
			break;
		}

		currentAddress = getNextPossibleAddress(alignmentInBytes, currentAddress, direction);

		/* stop if we exit the range of possible allocation addresses on the system */
		if (currentAddress == NULL) {
			break;
		}
	}

	/* if strict flag is not set and we did not get any memory, attempt to get memory at any address */
	if (0 == (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions) && (MAP_FAILED == memoryPointer)) {
allocAnywhere:
		memoryPointer = allocateMemoryForLargePages(portLibrary, identifier, NULL, addressKey, category, byteAmount);
	}

	if (MAP_FAILED == memoryPointer) {
		Trc_PRT_vmem_omrvmem_reserve_memory_shmat_failed2(byteAmount, startAddress, endAddress);

		memoryPointer = NULL;
	} else if (0 != (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions)) {
		/* if strict flag is set and returned pointer is not within range then fail */
		if ((memoryPointer > endAddress) || (memoryPointer < startAddress)) {
			omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier);

			Trc_PRT_vmem_omrvmem_reserve_memory_ex_UnableToAllocateWithinSpecifiedRange(byteAmount, startAddress, endAddress);

			memoryPointer = NULL;
		}
	}
	return memoryPointer;
}

static void *
getMemoryInRangeForDefaultPages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode)
{
#if !defined(OMR_ENV_DATA64)
	/**
	 * Using shmat to allocate vmem on AIX32 reduces the number of segments which can be used for thread stacks.  Thus, explicitly use the mmap
	 * allocator for default-sized pages (large pages still must use shmat).
	 * The only reasons to use shmat, other than unification of the code paths, are to support large pages or NUMA.  NUMA is only supported on
	 * the Balanced policy which does not yet run on 32-bit and large page allocations come through a different path.
	 *
	 * See CMVC 178983 for more detail regarding this limitation.
	 */
	return getMemoryInRangeForDefaultPagesUsingMmap(portLibrary, identifier, category, byteAmount, startAddress, endAddress, alignmentInBytes, vmemOptions, mode);
#else /* !defined(OMR_ENV_DATA64) */
	/*
	 * Balanced GC requires that omrvmem_numa_set_affinity() can be used on memory returned by omrvmem_reserve_memory(). This is
	 * not supported for memory returned by default_pageSize_reserve_memory(), as it uses the mmap() API for historical reasons.
	 *
	 * To support omrvmem_numa_set_affinity() under balanced, we redefine getMemoryInRangeForDefaultPages() to simply call
	 * reserveLargePages(), which uses the shmat()/shmget() APIs which do not have the same problem.
	 *
	 * Note that there is a limitation in older versions of AIX (pre-6.1) where shmem may be unable to allocate as much memory as
	 * mmap (up to one segment is lost - 256M).  However, the NUMA API is not supported prior to AIX 6.1 so these requirements
	 * never appear together.  Thus, if NUMA is supported on the system, use shmem.  Otherwise, use mmap (as we will not be trying
	 * to use the NUMA API).
	 * See CMVC 178946 for more detail regarding this limitation
	 *
	 * Also note, do not use shmem for OMRPORT_VMEM_MEMORY_MODE_EXECUTE memory (JIT codecache) as it causes the following problems:
	 * 		- 2MB of codeCache will waste a segment of 256MB virtual memory
	 * 		- Upon JIT start, there are 4 such codecaches created spanning more than 1GB of virtual space.
	 * 			- Calls from one codeCache to another will need trampolines to jump more than 32MB of distance
	 * 		- puts pressure on hardware facility of virtual memory translations,
	 * 			- i.e. SLB(segment lookaside buffer), TLB(table lookaside buffer), and ERAT(effective address to real address table). In particular, each 256MB requires an SLB entry.
	 *
	 */
	if (__ENHANCED_AFFINITY() && (OMRPORT_VMEM_MEMORY_MODE_EXECUTE != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode))) {
		/* If we have __ENHANCED_AFFINITY() and we're not looking for executable memory */
		return reserveLargePages(portLibrary, identifier, category, byteAmount, startAddress, endAddress, PPG_vmem_pageSize[0], alignmentInBytes, vmemOptions, mode);
	} else {
		return getMemoryInRangeForDefaultPagesUsingMmap(portLibrary, identifier, category, byteAmount, startAddress, endAddress, alignmentInBytes, vmemOptions, mode);
	}
#endif /* !defined(OMR_ENV_DATA64) */
}

static void *
getMemoryInRangeForDefaultPagesUsingMmap(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode)
{
	intptr_t direction = 1;
	void *currentAddress = startAddress;
	void *memoryPointer = NULL;

	/* check allocation direction */
	if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) {
		direction = -1;
		currentAddress = endAddress;
	} else if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP)) {
		if (startAddress == NULL) {
			currentAddress = getNextPossibleAddress(alignmentInBytes, NULL, direction);
		}
	} else if (NULL == startAddress) {
		if (OMRPORT_VMEM_MAX_ADDRESS == endAddress) {
			/* if caller specified the entire address range and does not care about the direction
			 * save time by letting OS choose where to allocate the memory
			 */
			goto allocAnywhere;
		} else {
			/* if the startAddress is NULL but the endAddress is not we
			 * need to change the startAddress so that we have a better chance
			 * of getting memory within the range because NULL means don't care
			 */
			currentAddress = (void *)alignmentInBytes;
		}
	}

	/* try all addresses within range */
	while ((startAddress <= currentAddress) && (endAddress >= currentAddress)) {
		/* free previously attached memory and attempt to get new memory */
		if (NULL != memoryPointer) {
			if (0 != omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier)) {
				return NULL;
			}
		}

		memoryPointer = default_pageSize_reserve_memory(portLibrary, currentAddress, byteAmount, identifier, mode, PPG_vmem_pageSize[0], category);

		/* stop if returned pointer is within range */
		if ((NULL != memoryPointer) && (startAddress <= memoryPointer) && (endAddress >= memoryPointer)) {
			break;
		}

		currentAddress = getNextPossibleAddress(alignmentInBytes, currentAddress, direction);

		/* stop if we exit the range of possible allocation addresses on the system */
		if (currentAddress == NULL) {
			break;
		}
	}

	/* if strict flag is not set and we did not get any memory, attempt to get memory at any address */
	if (0 == (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions) && (NULL == memoryPointer)) {
allocAnywhere:
		memoryPointer = default_pageSize_reserve_memory(portLibrary, NULL, byteAmount, identifier, mode, PPG_vmem_pageSize[0], category);
	}

	if (NULL == memoryPointer) {
		memoryPointer = NULL;
	} else if (0 != (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions)) {
		/* if strict flag is set and returned pointer is not within range then fail */
		if ((memoryPointer > endAddress) || (memoryPointer < startAddress)) {
			omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier);

			Trc_PRT_vmem_omrvmem_reserve_memory_ex_UnableToAllocateWithinSpecifiedRange(byteAmount, startAddress, endAddress);

			memoryPointer = NULL;
		}
	}

	return memoryPointer;
}

/**
 * Attempts to allocate memory at currentAddress by delegating to the appropriate
 * function depending on whether or not the default page size is being used.
 * Returns pointer to newly allocated memory upon success and if default page size
 * is being used NULL upon failure else -1 upon failure.
 */
static void *
allocateMemoryForLargePages(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, void *currentAddress, key_t addressKey, OMRMemCategory *category, uintptr_t byteAmount)
{
	void *ptr = shmat(addressKey,  currentAddress, 0);

	/* We are not updating the entire vmemIdentifier for the sake of speed
	 * because on this platform we need to detect the page size which was
	 * returned which takes a bit more time
	 */
	identifier->allocator = OMRPORT_VMEM_RESERVE_USED_SHM;
	identifier->category = category;

	if (MAP_FAILED != ptr) {
		identifier->address = ptr;
		identifier->size = byteAmount;
		omrmem_categories_increment_counters(category, byteAmount);
	}
#if defined(J9VMEM_DEBUG)
	printf("\t\t shmat(currentAddress = 0x%zx) returned 0x%zx\n", currentAddress, ptr);
#endif
	return ptr;
}


/**
 * Ensures that the requested address and (address + byteAmount) fall
 * within the reserved identifier->address and (identifier->address + identifier->size)
 * @returns TRUE if the range is valid, FALSE otherwise
 */
static BOOLEAN
rangeIsValid(struct J9PortVmemIdentifier *identifier, void *address, uintptr_t byteAmount)
{
	BOOLEAN isValid = FALSE;
	uintptr_t requestedUpperLimit = (uintptr_t)address + byteAmount - 1;

	if (requestedUpperLimit + 1 >= byteAmount) {
		/* Requested range does not wrap around */
		uintptr_t realUpperLimit = (uintptr_t)identifier->address + identifier->size - 1;

		if (((uintptr_t)address >= (uintptr_t)identifier->address) &&
			(requestedUpperLimit <= realUpperLimit)
		) {
			isValid = TRUE;
		}
	}

	return isValid;
}

intptr_t
omrvmem_numa_set_affinity(struct OMRPortLibrary *portLibrary, uintptr_t numaNode, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	int result = -1;
#if defined(OMR_ENV_DATA64)
	/**
	 * NUMA only supported on AIX64 since Balanced does not yet exist on AIX32 and using shmem for vmem, which NUMA requires,
	 * reduces the number of thread stacks available.
	 *
	 * See CMVC 178983 for more detail regarding this limitation.
	 */
#ifndef _AIX61
	int (*PTR_ra_attach)(rstype_t, rsid_t, rstype_t, rsid_t_MODIFIED, uint_t) = (int (*)(rstype_t, rsid_t, rstype_t, rsid_t_MODIFIED, uint_t))dlsym(RTLD_DEFAULT, "ra_attach");
	rsid_t_MODIFIED targetSRADResourceID;
#else
	int (*PTR_ra_attach)(rstype_t, rsid_t, rstype_t, rsid_t, uint_t) = (int (*)(rstype_t, rsid_t, rstype_t, rsid_t, uint_t))dlsym(RTLD_DEFAULT, "ra_attach");
	rsid_t targetSRADResourceID;
#endif
	rsid_t thisSubrangeResourceID;
	/* don't forget to subtract one from the numaNode to shift it into the 0-indexed numbering scheme used by the system */
	sradid_t desiredSRADID = numaNode - 1;
	subrange_t subrange;

	subrange.su_offset = (long long)address;
	subrange.su_length = byteAmount;
	/* the subrange is in this process's address space */
	subrange.su_rstype = R_PROCMEM;
	/* use this process's ID */
	subrange.su_rsid.at_pid = RS_MYSELF;
	/* subrange policy is ignored in favour of the ra_attach policy so set it to 0 */
	subrange.su_policy = 0;
	/* point the resource we are binding at this memory range structure */
	thisSubrangeResourceID.at_subrange = &subrange;
	targetSRADResourceID.at_sradid = desiredSRADID;

	/* attach the specified subrange to the SRAD (allow the memory to be asynchronously migrated to the specified SRAD) */
	result = PTR_ra_attach(R_SUBRANGE, thisSubrangeResourceID, R_SRADID, targetSRADResourceID, R_MIGRATE_ASYNC);
#endif /* defined(OMR_ENV_DATA64) */
	return (0 == result) ? 0 : OMRPORT_ERROR_VMEM_OPFAILED;
}

intptr_t
omrvmem_numa_get_node_details(struct OMRPortLibrary *portLibrary, J9MemoryNodeDetail *numaNodes, uintptr_t *nodeCount)
{
	intptr_t result = OMRPORT_ERROR_VMEM_OPFAILED;
#if defined(OMR_ENV_DATA64)
	/**
	 * NUMA only supported on AIX64 since Balanced does not yet exist on AIX32 and using shmem for vmem, which NUMA requires,
	 * reduces the number of thread stacks available.
	 *
	 * See CMVC 178983 for more detail regarding this limitation.
	 */
	sradid_t (*PTR_rs_get_homesrad)(void);
	int (*PTR_rs_info)(void *out, int command, long arg1, long arg2);

	PTR_rs_get_homesrad = (sradid_t (*)(void))dlsym(RTLD_DEFAULT, "rs_get_homesrad");
	PTR_rs_info = (int (*)(void *out, int command, long arg1, long arg2))dlsym(RTLD_DEFAULT, "rs_info");
	if ((0 != __ENHANCED_AFFINITY()) && (NULL != PTR_rs_get_homesrad) && (NULL != PTR_rs_info)) {
		sradid_t sid = PTR_rs_get_homesrad();
		rsethandle_t rs_all =  rs_alloc(RS_ALL);
		int srad_sdl =  rs_getinfo(NULL, R_SRADSDL, 0);
		int num_srads =  rs_numrads(rs_all, srad_sdl, 0);
		int sradid = 0;
		uintptr_t nextNodeIndex = 0;

		for (sradid = 0; sradid < num_srads; sradid++) {
			loadavg_info_t load_avg;
			struct vm_srad_meminfo srad_meminfo;

			int infoRetCode = PTR_rs_info(&load_avg, RS_SRADID_LOADAVG, sradid, sizeof(load_avg));
			int vmRetCode = 0;
			srad_meminfo.vmsrad_in_size = sizeof(srad_meminfo);
			vmRetCode = vmgetinfo(&srad_meminfo, VM_SRAD_MEMINFO, sradid);
			if ((-1 != infoRetCode) && (-1 != vmRetCode)) {
				if (nextNodeIndex < *nodeCount) {
					J9MemoryState policy = J9NUMA_DENIED;
					if (srad_meminfo.vmsrad_total > 0) {
						policy = J9NUMA_ALLOWED;
					}
					numaNodes[nextNodeIndex].j9NodeNumber = sradid + 1;
					numaNodes[nextNodeIndex].memoryPolicy = policy;
					numaNodes[nextNodeIndex].computationalResourcesAvailable = load_avg.cpu_count;
				}
				nextNodeIndex += 1;
			}
		}
		rs_free(rs_all);
		*nodeCount = nextNodeIndex;
		PPG_numa_platform_supports_numa = 1;
		result = 0;
	}
#endif /* defined(OMR_ENV_DATA64) */
	return result;
}

int32_t
omrvmem_get_available_physical_memory(struct OMRPortLibrary *portLibrary, uint64_t *freePhysicalMemorySize)
{
	uint64_t result = 0;
	uint32_t pageIndex = 0;
	uintptr_t *pageSizes = PPG_vmem_pageSize;
	for (pageIndex = 0; 0 != pageSizes[pageIndex]; ++pageIndex) {
		struct vminfo_psize pageSize;
		pageSize.psize = pageSizes[pageIndex];
		if (0 == vmgetinfo(&pageSize, VMINFO_PSIZE, sizeof(pageSize))) {
			result += pageSize.psize * pageSize.numfrb;
		} else {
			intptr_t vmgetinfoError = (intptr_t)errno;
			Trc_PRT_vmem_get_available_physical_memory_failed("vmgetinfo", vmgetinfoError);
			return OMRPORT_ERROR_VMEM_OPFAILED;
		}
	}
	*freePhysicalMemorySize = result;
	Trc_PRT_vmem_get_available_physical_memory_result(result);
	return 0;
}

int32_t
omrvmem_get_process_memory_size(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize)
{
	int32_t result = OMRPORT_ERROR_VMEM_OPFAILED;
	Trc_PRT_vmem_get_process_memory_enter((int32_t)queryType);
	switch (queryType) {
	case OMRPORT_VMEM_PROCESS_PRIVATE: {
		struct procentry64 pe;
		pid_t pid = getpid();

		if (1 == getprocs64(&pe, sizeof(pe), NULL, 0, &pid, 1)) {
			/* NOTE: pi_dvm is always in 4K units regardless of pi_data_l2psize. */
			result = 0;
			int64_t size = pe.pi_tsize + (pe.pi_dvm * 4096);
			if (size > 0) {
				*memorySize = size;
			} else {
				Trc_PRT_vmem_get_process_memory_failed("negative size from getprocs64", 0);
				result = OMRPORT_ERROR_VMEM_NOT_SUPPORTED;
			}
		} else {
			Trc_PRT_vmem_get_process_memory_failed("getprocs64 failed", 0);
			result = OMRPORT_ERROR_VMEM_NOT_SUPPORTED;
		}
		break;
	}
	case OMRPORT_VMEM_PROCESS_PHYSICAL: /* FALLTHROUGH */
	case OMRPORT_VMEM_PROCESS_VIRTUAL:
		/* There is no API on AIX to get shared memory usage for the process. If such an
		 * API existed, we could return getProcessPrivateMemorySize() + sharedSize here.
		 *
		 * Note: Iterating through /proc/<pid>/map and looking at the pages that are
		 * not marked MA_SHARED does not account for shared code pages when in fact
		 * command-line AIX utilities (such as svmon) do show that pages are shared.
		 */
		Trc_PRT_vmem_get_process_memory_failed("unsupported query", 0);
		result = OMRPORT_ERROR_VMEM_NOT_SUPPORTED;
		break;
	default:
		Trc_PRT_vmem_get_process_memory_failed("invalid query", 0);
		result = OMRPORT_ERROR_VMEM_OPFAILED;
		break;
	}
	Trc_PRT_vmem_get_process_memory_exit(result, *memorySize);
	return result;
}
