/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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
 * @brief Virtual memory
 */

#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"
#include "omrportasserts.h"
#include "omrvmem.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <mach/mach.h>
#include <mach/processor_info.h>
#include <mach/mach_host.h>

#if !defined(MPOL_F_MEMS_ALLOWED)
#define MPOL_F_MEMS_ALLOWED 4
#endif

#include <sys/shm.h>

#if !defined(MAP_FAILED)
#define MAP_FAILED -1
#endif

#define INVALID_KEY -1
#define FILE_NAME_SIZE 32
/* 10^12 */
#define TRUCANTE_TIME_CONST 1000000000000
/* 10^9 */
#define TRUNCATE_THREAD_CONST 1000000000

#if 0
#define OMRVMEM_DEBUG
#endif

typedef void *ADDRESS;

typedef struct AddressRange {
	ADDRESS start;
	ADDRESS end;
} AddressRange;

/* Macro to clear existing flags for page type and set it to new type */
#define SET_PAGE_TYPE(pageFlags, pageType) ((pageFlags) = (((pageFlags) & ~OMRPORT_VMEM_PAGE_FLAG_TYPE_MASK) | pageType))
#define IS_VMEM_PAGE_FLAG_NOT_USED(pageFlags)  (OMRPORT_VMEM_PAGE_FLAG_NOT_USED == (OMRPORT_VMEM_PAGE_FLAG_NOT_USED & (pageFlags)))

void addressRange_Init(AddressRange *range, ADDRESS start, ADDRESS end);
BOOLEAN addressRange_Intersect(AddressRange *a, AddressRange *b, AddressRange *result);
BOOLEAN addressRange_IsValid(AddressRange *range);
uintptr_t addressRange_Width(AddressRange *range);

static void *getMemoryInRange(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t mode);
static BOOLEAN isStrictAndOutOfRange(void *memoryPointer, void *startAddress, void *endAddress, uintptr_t vmemOptions);
static BOOLEAN rangeIsValid(struct J9PortVmemIdentifier *identifier, void *address, uintptr_t byteAmount);

void *reserveMemory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, OMRMemCategory *category);
void update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address, void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t allocator, OMRMemCategory *category, int fd);
int get_protectionBits(uintptr_t mode);

/*
 * Init the range with values
 *
 * @param AddressRange* range	[out]	Returns the range object
 * @param void* 		start	[in] 	The start address of the range
 * @param void*  		end		[in]	The end address of the range
 */
void
addressRange_Init(AddressRange *range, ADDRESS start, ADDRESS end)
{
	range->start = start;
	range->end = end;
}

/*
 * Calculate out the intersection of 2 ranges
 *
 * @param AddressRange* a		[in] 	a range object
 * @param AddressRange* b		[in] 	another range object
 * @param AddressRange* result 	[out]	the intersection of the above 2 ranges
 *
 * Returns TRUE if they have intersection.
 */
BOOLEAN
addressRange_Intersect(AddressRange *a, AddressRange *b, AddressRange *result)
{
	result->start = a->start > b->start ? a->start : b->start;
	result->end = a->end < b->end ? a->end : b->end;

	return addressRange_IsValid(result);
}


/*
 * Calculate if a range is valid.
 * A valid range should have start < its end
 *
 * @param AddressRange* range	[in] 	a range object
 *
 * Returns TRUE if range's start < end.
 */
BOOLEAN
addressRange_IsValid(AddressRange *range)
{
	return (range->end > range->start) ? TRUE : FALSE;
}

/*
 * Calculate out the with of a range
 *
 * @param AddressRange* range 	[in]	a range object
 *
 * Returns the width of the range.
 * Caller should make sure the input parameter 'range' is valid,
 * otherwise, unexpected value may be returned.
 */
uintptr_t
addressRange_Width(AddressRange *range)
{
	Assert_PRT_true(TRUE == addressRange_IsValid(range));
	return range->end - range->start;
}

void
omrvmem_shutdown(struct OMRPortLibrary *portLibrary)
{
}

int32_t
omrvmem_startup(struct OMRPortLibrary *portLibrary)
{
	/* 0 terminate the table */
	memset(PPG_vmem_pageSize, 0, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memset(PPG_vmem_pageFlags, 0, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

	/* First the default page size */
	PPG_vmem_pageSize[0] = (uintptr_t)vm_page_size;
	PPG_vmem_pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	/* Now the superpages */
	PPG_vmem_pageSize[1] = 0;
	PPG_vmem_pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_SUPERPAGE_ANY;

#if defined(VM_FLAGS_SUPERPAGE_SIZE_2MB)
	/* The OSX man pages for mmap state that mach/vm_statistics.h specifies the superpage size,
	 * but in practice it appears to always be 2MB.
	 */
	PPG_vmem_pageSize[2] = 2*1024*1024;
	PPG_vmem_pageFlags[2] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
#endif /* defined(VM_FLAGS_SUPERPAGE_SIZE_2MB) */

	/* Set default value to advise OS about vmem that is no longer needed */
	portLibrary->portGlobals->vmemAdviseOSonFree = 1;
	/* set default value to advise OS about vmem to consider for Transparent HugePage (Only for Linux) */
	portLibrary->portGlobals->vmemEnableMadvise = 0;

	return 0;
}

void *
omrvmem_commit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	void *rc = NULL;
	Trc_PRT_vmem_omrvmem_commit_memory_Entry(address, byteAmount);

	if (rangeIsValid(identifier, address, byteAmount)) {
		ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
		ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);

		/* Default page size */
		if (PPG_vmem_pageSize[0] == identifier->pageSize ||
			0 != (identifier->mode & OMRPORT_VMEM_MEMORY_MODE_EXECUTE)
		) {
			if (0 == mprotect(address, byteAmount, get_protectionBits(identifier->mode))) {
#if defined(OMRVMEM_DEBUG)
				printf("\t\t omrvmem_commit_memory called mprotect, returning 0x%zx\n", address);
				fflush(stdout);
#endif /* defined(OMRVMEM_DEBUG) */
				rc = address;
			} else {
				Trc_PRT_vmem_omrvmem_commit_memory_mprotect_failure(errno);
				portLibrary->error_set_last_error(portLibrary,  errno, OMRPORT_ERROR_VMEM_OPFAILED);
			}
		} else if (PPG_vmem_pageSize[2] == identifier->pageSize || PPG_vmem_pageFlags[1] == identifier->pageFlags) {
			rc = address;
		}
	} else {
		Trc_PRT_vmem_omrvmem_commit_memory_invalidRange(identifier->address, identifier->size, address, byteAmount);
		portLibrary->error_set_last_error(portLibrary,  -1, OMRPORT_ERROR_VMEM_INVALID_PARAMS);
	}

#if defined(OMRVMEM_DEBUG)
	printf("\t\t omrvmem_commit_memory returning 0x%x\n", rc);
	fflush(stdout);
#endif /* defined(OMRVMEM_DEBUG) */
	Trc_PRT_vmem_omrvmem_commit_memory_Exit(rc);
	return rc;
}

intptr_t
omrvmem_decommit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	intptr_t result = -1;

	Trc_PRT_vmem_omrvmem_decommit_memory_Entry(address, byteAmount);

	if (1 == portLibrary->portGlobals->vmemAdviseOSonFree) {
		if (rangeIsValid(identifier, address, byteAmount)) {
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);

			if (0 < byteAmount) {
				if (OMRPORT_VMEM_RESERVE_USED_MMAP == identifier->allocator) {
					result  = (intptr_t)madvise((void *)address, (size_t) byteAmount, MADV_DONTNEED);
				} else {
					/* OSX cannot use shmat/shmget to allocate large parges. It uses mmap to allocate superpages. */
					result = -1;
				}

				if (0 != result) {
					Trc_PRT_vmem_omrvmem_decommit_memory_failure(errno, address, byteAmount);
				}
			} else {
				/* Nothing to de-commit. */
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
	return result;
}

int32_t
omrvmem_free_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	int32_t ret = -1;
	OMRMemCategory *category = identifier->category;
	uintptr_t mode = identifier->mode;
	int fd = identifier->fd;

	Trc_PRT_vmem_omrvmem_free_memory_Entry(address, byteAmount);

	/* CMVC 180372 - Some users store the identifier in the allocated memory.
	 * So, identifier must be cleared before memory is freed.
	 */
	update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL, -1);

	ret = (int32_t)munmap(address, (size_t)byteAmount);
	omrmem_categories_decrement_counters(category, byteAmount);

	if((0 != (mode & OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN)) && (OMRPORT_INVALID_FD != fd)
			&& (VM_FLAGS_SUPERPAGE_SIZE_ANY != fd) && (VM_FLAGS_SUPERPAGE_SIZE_2MB != fd)) {
		close(fd);
	}

	Trc_PRT_vmem_omrvmem_free_memory_Exit(ret);
	return ret;
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
	void *memoryPointer = NULL;
	OMRMemCategory *category = omrmem_get_category(portLibrary, params->category);

	Trc_PRT_vmem_omrvmem_reserve_memory_Entry_replacement(params->startAddress, params->byteAmount, params->pageSize);

	Assert_PRT_true(params->startAddress <= params->endAddress);
	ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(params->byteAmount, params->pageSize);

	if (0 != (OMRPORT_VMEM_PAGE_FLAG_SUPERPAGE_ANY & params->pageFlags)) {
		memoryPointer = getMemoryInRange(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress, 0, params->options, params->pageSize, params->pageFlags, params->mode);
	} else if (0 == params->pageSize) {
		/* Invalid input */
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL, -1);
		Trc_PRT_vmem_omrvmem_reserve_memory_invalid_input();
	} else if (PPG_vmem_pageSize[0] == params->pageSize) {
		uintptr_t alignmentInBytes = OMR_MAX(params->pageSize, params->alignmentInBytes);
		uintptr_t minimumGranule = OMR_MIN(params->pageSize, params->alignmentInBytes);

		/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
		if ((0 == minimumGranule) || (0 == (alignmentInBytes % minimumGranule))) {
			memoryPointer = getMemoryInRange(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress, alignmentInBytes, params->options, params->pageSize, params->pageFlags, params->mode);
		}
	} else if (PPG_vmem_pageSize[2] == params->pageSize) {
		uintptr_t largePageAlignmentInBytes = OMR_MAX(params->pageSize, params->alignmentInBytes);
		uintptr_t largePageMinimumGranule = OMR_MIN(params->pageSize, params->alignmentInBytes);

		/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
		if ((0 == largePageMinimumGranule) || (0 == (largePageAlignmentInBytes % largePageMinimumGranule))) {
			memoryPointer = getMemoryInRange(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress, largePageAlignmentInBytes, params->options, params->pageSize, params->pageFlags, params->mode);
		}
		if (NULL == memoryPointer) {
			/* If strict page size flag is not set try again with default page size */
			if (0 == (OMRPORT_VMEM_STRICT_PAGE_SIZE & params->options)) {
#if defined(OMRVMEM_DEBUG)
				printf("\t\t\t NULL == memoryPointer, reverting to default pages\n");
				fflush(stdout);
#endif /* defined(OMRVMEM_DEBUG) */
				uintptr_t defaultPageSize = PPG_vmem_pageSize[0];
				uintptr_t alignmentInBytes = OMR_MAX(defaultPageSize, params->alignmentInBytes);
				uintptr_t minimumGranule = OMR_MIN(defaultPageSize, params->alignmentInBytes);

				/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
				if ((0 == minimumGranule) || (0 == (alignmentInBytes % minimumGranule))) {
					memoryPointer = getMemoryInRange(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress, alignmentInBytes, params->options, PPG_vmem_pageSize[0], OMRPORT_VMEM_PAGE_FLAG_NOT_USED, params->mode);
				}
			} else {
				update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL, -1);
			}
		}
	} else {
		/* If the pageSize is not one of the supported page sizes, error */
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL, -1);
		Trc_PRT_vmem_omrvmem_reserve_memory_unsupported_page_size(params->pageSize);
	}


#if defined(OMRVMEM_DEBUG)
	printf("\tomrvmem_reserve_memory_ex returning %p\n", memoryPointer);
	fflush(stdout);
#endif

	Trc_PRT_vmem_omrvmem_reserve_memory_Exit_replacement(memoryPointer, params->startAddress);
	return memoryPointer;
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

void *
reserveMemory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, OMRMemCategory *category)
{
	int fd = -1;
	int flags = MAP_PRIVATE;
	void *result = NULL;
	int protectionFlags = PROT_NONE;
	BOOLEAN useBackingSharedFile = FALSE;

	if(mode & OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN) {
		flags = MAP_SHARED;
		useBackingSharedFile = TRUE;
	}

	if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & mode)) {
		protectionFlags = get_protectionBits(mode);
	} else {
		flags |= MAP_NORESERVE;
	}

#if defined(VM_FLAGS_SUPERPAGE_SIZE_2MB)
	if (PPG_vmem_pageSize[2] == pageSize) {
		fd = VM_FLAGS_SUPERPAGE_SIZE_2MB;
	}
#endif /* defined(VM_FLAGS_SUPERPAGE_SIZE_2MB) */
	if (0 != (OMRPORT_VMEM_PAGE_FLAG_SUPERPAGE_ANY & pageFlags)) {
		fd = VM_FLAGS_SUPERPAGE_SIZE_ANY;
	}

	/* We only create shared heap if it was requested and huge pages are not available */
	if ((-1 == fd) && useBackingSharedFile) {
		Trc_PRT_vmem_reserve_doubleMapAPI_available(address, byteAmount);
		mode |= OMRPORT_VMEM_MEMORY_MODE_DOUBLE_MAP_AVAILABLE;
		int ft = -1;
		/* shm_open(2) only accepts file names up to SHM_NAME_MAX (32) chars in size (including the
		 * NULL character). To avoid overflow, we truncate omrtime_current_time_str to 12 digits
		 * and pthread_self to 9 digits (totatlling 30 chars plus NULL char), which is enough for
		 * the purposes of generating a random name. */
		char omrtime_current_time_str[FILE_NAME_SIZE];
		char pthread_self_str[FILE_NAME_SIZE];
		char filename[FILE_NAME_SIZE];
		sprintf(omrtime_current_time_str, "%zu", (uintptr_t)omrtime_current_time_millis(portLibrary) % (uintptr_t)TRUCANTE_TIME_CONST);
		sprintf(pthread_self_str, "%zu", (uintptr_t)pthread_self() % (uintptr_t)TRUNCATE_THREAD_CONST);
		sprintf(filename, "omrvmem_%.12s_%.9s", omrtime_current_time_str, pthread_self_str);

		fd = shm_open(filename, O_RDWR | O_CREAT, 0600);
		shm_unlink(filename);
		ft = ftruncate(fd, byteAmount);

		if ((OMRPORT_INVALID_FD == fd) || (OMRPORT_INVALID_FD == ft)) {
			Trc_PRT_vmem_reserve_failed(address, byteAmount);
			update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL, -1);
			Trc_PRT_vmem_reserve_exit(result, address, byteAmount);
			return result;
		}
	} else {
		flags = MAP_ANON | MAP_PRIVATE;
		useBackingSharedFile = FALSE;
	}

	result = mmap(address, (size_t)byteAmount, protectionFlags, flags, fd, 0);
	if (MAP_FAILED == result) {
		if (useBackingSharedFile && (OMRPORT_INVALID_FD != fd) && (VM_FLAGS_SUPERPAGE_SIZE_ANY != fd) && (VM_FLAGS_SUPERPAGE_SIZE_2MB != fd)) {
			close(fd);
		}
		result = NULL;
	} else {
		/* Update identifier and commit memory if required, else return reserved memory */
		update_vmemIdentifier(identifier, result, result, byteAmount, mode, pageSize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, OMRPORT_VMEM_RESERVE_USED_MMAP, category, fd);
		omrmem_categories_increment_counters(category, byteAmount);
		if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & mode)) {
			if (NULL == omrvmem_commit_memory(portLibrary, result, byteAmount, identifier)) {
				/* If the commit fails free the memory */
				omrvmem_free_memory(portLibrary, result, byteAmount, identifier);
				result = NULL;
			}
		}
	}

	if (NULL == result) {
		Trc_PRT_vmem_omrvmem_reserve_memory_failure();
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL, -1);
	}

	return result;
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
 * @param[in] pageFlags flags for describing page type
 * @param[in] allocator Constant describing how the virtual memory was allocated.
 * @param[in] category Memory allocation category
 */
void
update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address, void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t allocator, OMRMemCategory *category, int fd)
{
	identifier->address = address;
	identifier->handle = handle;
	identifier->size = byteAmount;
	identifier->pageSize = pageSize;
	identifier->pageFlags = pageFlags;
	identifier->mode = mode;
	identifier->allocator = allocator;
	identifier->category = category;
	identifier->fd = fd;
}

int
get_protectionBits(uintptr_t mode)
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

void
omrvmem_default_large_page_size_ex(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags)
{
	/* Note that the PPG_vmem_pageSize is a null-terminated list of page sizes.
	 * There will always be the 0 element (default page size),
	 * so the 1 element will be zero or the default large size where only 2 are supported
	 * (platforms with more complicated logic will use their own special process to determine the best response)
	 */
	if (NULL != pageSize) {
		*pageSize = PPG_vmem_pageSize[1];
	}
	if (NULL != pageFlags) {
		*pageFlags = PPG_vmem_pageFlags[1];
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

	Assert_PRT_true_wrapper(0 != validPageFlags);

	/* If flag OMRPORT_VMEM_PAGE_FLAG_SUPERPAGE_ANY is specified, page size is irrelevant. */
	if (OMRPORT_VMEM_PAGE_FLAG_SUPERPAGE_ANY == validPageFlags) {
		validPageSize = 0;
		goto _end;
	}
	if (0 != validPageSize) {
		uintptr_t pageIndex = 0;
		uintptr_t *supportedPageSizes = portLibrary->vmem_supported_page_sizes(portLibrary);
		uintptr_t *supportedPageFlags = portLibrary->vmem_supported_page_flags(portLibrary);

		for (pageIndex = 0; 0 != supportedPageFlags[pageIndex]; pageIndex++) {
			if (supportedPageSizes[pageIndex] == validPageSize) {
				SET_PAGE_TYPE(validPageFlags, PPG_vmem_pageFlags[pageIndex]);
				goto _end;
			}
		}
	}

	portLibrary->vmem_default_large_page_size_ex(portLibrary, mode, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		validPageSize = defaultLargePageSize;
		validPageFlags = defaultLargePageFlags;
	} else {
		validPageSize = PPG_vmem_pageSize[0];
		validPageFlags = PPG_vmem_pageFlags[0];
	}

_end:
	if (IS_VMEM_PAGE_FLAG_NOT_USED(*pageFlags)) {
		/* In this case ignore page flags when setting isSizeSupported */
		*isSizeSupported = (*pageSize == validPageSize);
	} else {
		*isSizeSupported = ((*pageSize == validPageSize) && (*pageFlags == validPageFlags));
	}
	*pageSize = validPageSize;
	*pageFlags = validPageFlags;

	return 0;
}

/**
 * Allocates memory in specified range using a best effort approach
 * (unless OMRPORT_VMEM_STRICT_ADDRESS flag is used) and returns a pointer
 * to the newly allocated memory. Returns NULL on failure.
 */
static void *
getMemoryInRange(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t mode)
{
	intptr_t direction = 1;
	void *currentAddress = startAddress;
	void *oldAddress = NULL;
	void *memoryPointer = NULL;

	/* Check allocation direction */
	if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) {
		direction = -1;
		currentAddress = endAddress;
	} else if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP)) {
		if (startAddress == NULL) {
			currentAddress += direction * alignmentInBytes;
		}
	} else if (NULL == startAddress) {
		if (OMRPORT_VMEM_MAX_ADDRESS == endAddress) {
			/* If caller specified the entire address range and does not care about the direction
			 * save time by letting OS choose where to allocate the memory
			 */
			goto allocAnywhere;
		} else {
			/* If the startAddress is NULL but the endAddress is not we
			 * need to change the startAddress so that we have a better chance
			 * of getting memory within the range because NULL means don't care
			 */
			currentAddress = (void *)alignmentInBytes;
		}
	}

#if defined(VM_FLAGS_SUPERPAGE_SIZE_2MB)
	if (PPG_vmem_pageSize[2] == pageSize) {
		/* Don't bother scanning the entire address space if the request can't be satisfied. */
		void *tmpPointer = reserveMemory(portLibrary, NULL, byteAmount, identifier, mode, pageSize, pageFlags, category);
		if (NULL == tmpPointer) {
			goto done;
		}
		omrvmem_free_memory(portLibrary, tmpPointer, byteAmount, identifier);
	}
#endif /* defined(VM_FLAGS_SUPERPAGE_SIZE_2MB) */

	/* Try all addresses within range */
	while ((startAddress <= currentAddress) && (endAddress >= currentAddress)) {
		memoryPointer = reserveMemory(portLibrary, currentAddress, byteAmount, identifier, mode, pageSize, pageFlags, category);

		if (NULL != memoryPointer) {
			/* stop if returned pointer is within range */
			if ((startAddress <= memoryPointer) && (endAddress >= memoryPointer)) {
				break;
			}
			if (0 != omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier)) {
				memoryPointer = NULL;
				goto done;
			}
			memoryPointer = NULL;
		}

		oldAddress = currentAddress;

		currentAddress += direction * alignmentInBytes;

		/* Protect against loop around */
		if (((1 == direction) && ((uintptr_t)oldAddress > (uintptr_t)currentAddress))
			|| ((-1 == direction) && ((uintptr_t)oldAddress < (uintptr_t)currentAddress))
		) {
			break;
		}
	}

	/* If strict flag is not set and we did not get any memory, attempt to get memory at any address */
	if (0 == (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions) && (NULL == memoryPointer)) {
allocAnywhere:
		memoryPointer = reserveMemory(portLibrary, NULL, byteAmount, identifier, mode, pageSize, pageFlags, category);
	}

	if ((NULL != memoryPointer) && isStrictAndOutOfRange(memoryPointer, startAddress, endAddress, vmemOptions)) {
		/* If strict flag is set and returned pointer is not within range then fail */
		omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier);

		Trc_PRT_vmem_omrvmem_reserve_memory_ex_UnableToAllocateWithinSpecifiedRange(byteAmount, startAddress, endAddress);

		memoryPointer = NULL;
	}

done:
	return memoryPointer;
}

/**
 * Returns TRUE if OMRPORT_VMEM_STRICT_ADDRESS flag is set and memoryPointer
 * is outside of range, else returns FALSE
 */
static BOOLEAN
isStrictAndOutOfRange(void *memoryPointer, void *startAddress, void *endAddress, uintptr_t vmemOptions)
{
	if ((0 != (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions)) &&
		((memoryPointer > endAddress) || (memoryPointer < startAddress))) {
		return TRUE;
	} else {
		return FALSE;
	}
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
	return OMRPORT_ERROR_VMEM_OPFAILED;
}

intptr_t
omrvmem_numa_get_node_details(struct OMRPortLibrary *portLibrary, J9MemoryNodeDetail *numaNodes, uintptr_t *nodeCount)
{
	return OMRPORT_ERROR_VMEM_OPFAILED;
}

int32_t
omrvmem_get_available_physical_memory(struct OMRPortLibrary *portLibrary, uint64_t *freePhysicalMemorySize)
{
	vm_statistics_data_t vmStatData;
	mach_msg_type_number_t msgTypeNumber = sizeof(vmStatData) / sizeof(natural_t);
	uint64_t result = 0;

	if (KERN_SUCCESS != host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmStatData, &msgTypeNumber)) {
		intptr_t sysconfError = (intptr_t)errno;
		Trc_PRT_vmem_get_available_physical_memory_failed("availablePages", sysconfError);
		return OMRPORT_ERROR_VMEM_OPFAILED;
	}
	result = vm_page_size * vmStatData.free_count;
	*freePhysicalMemorySize = result;
	Trc_PRT_vmem_get_available_physical_memory_result(result);
	return 0;
}

int32_t
omrvmem_get_process_memory_size(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize)
{
	int32_t result = OMRPORT_ERROR_VMEM_OPFAILED;
	kern_return_t rc = KERN_SUCCESS;
	mach_msg_type_number_t msgTypeNumberOut = TASK_BASIC_INFO_COUNT;
	task_basic_info_data_t taskInfoData;

	Trc_PRT_vmem_get_process_memory_enter((int32_t)queryType);
	rc = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&taskInfoData, &msgTypeNumberOut);

	if (KERN_SUCCESS == rc) {
		result = 0;
		switch (queryType) {
		case OMRPORT_VMEM_PROCESS_PHYSICAL:
			*memorySize = (uint64_t)(taskInfoData.resident_size);
			break;
		case OMRPORT_VMEM_PROCESS_PRIVATE:
			/* OSX has no API for shared/private memory usage.
			 * top is open source and has an implementation that estimates it by walking the virtual
			 * address space of the task and looking at the resident page and reference counts of
			 * mapped objects, but its broken and always returns 0..
			 */
			*memorySize = 0;
			result = OMRPORT_ERROR_VMEM_NOT_SUPPORTED;
			Trc_PRT_vmem_get_process_memory_failed("unsupported query", 0);
			break;
		case OMRPORT_VMEM_PROCESS_VIRTUAL:
			*memorySize = (uint64_t)(taskInfoData.virtual_size);
			break;
		default:
			Trc_PRT_vmem_get_process_memory_failed("invalid query", 0);
			result = OMRPORT_ERROR_VMEM_OPFAILED;
			break;
		}
	} else {
		Trc_PRT_vmem_get_process_memory_failed("task_info() failed", 0);
		result = OMRPORT_ERROR_VMEM_OPFAILED;
	}
	Trc_PRT_vmem_get_process_memory_exit(result, *memorySize);
	return result;
}

/**
 *  Restores memory region associated with double mapped region, to what it was previously
 *  If omrvmem_create_double_mapped_region was called with a NULL preferredAddress then we just
 *  call omrvmem_free_memory to free the contiguous range of memory. On the other hand, if
 *  double_map was called with a non NULL prefereAddress we call mmap to restore the memory
 *  region to what it was previously mapped (under the assumption it used mmap with flags
 *  MAP_PRIVATE | MAP_ANON). It's recommended that calls to omrvmem_create_double_mapped_region
 *  are paired with this function whenever double map is not needed anymore.
 *
 *  @param OMRPortLibrary               *portLibrary    [in] The port library object
 *  @param void                         *address        [in] Address location to free
 *  @param uintptr_t                    byteAmount      [in] Bytes to be freed
 *  @param struct J9PortVmemIdentifier  *identifier     [in] Identifier containing to be freed
 */

int32_t
omrvmem_release_double_mapped_region(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	int32_t rc = 0;
	uintptr_t allocator = identifier->allocator;
	Trc_PRT_double_map_regions_Release_Entry(address, byteAmount);

	if (OMRPORT_VMEM_RESERVE_USED_MMAP_RESTORE_MMAP == allocator) {
		uintptr_t mode = identifier->mode;
		int fd = identifier->fd;
		int protectionFlags = PROT_READ | PROT_WRITE;
		int flags = MAP_PRIVATE | MAP_FIXED;
		Trc_PRT_double_map_regions_Release_Restore_Region(address, byteAmount, fd);

		if (OMRPORT_INVALID_FD == fd) {
			flags |= MAP_ANON;
		}
		if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & mode)) {
			protectionFlags = get_protectionBits(mode);
		}
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL, -1);

		void *memPtr = mmap(
			address,
			byteAmount,
			protectionFlags,
			flags,
			fd,
			0);

		if (memPtr == MAP_FAILED) {
			Trc_PRT_double_map_regions_Release_Failure();
			portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_VMEM_OPFAILED, "Failed to map FIXED block of memory, mmap returned MAP_FAILED");
			rc = -1;
		} else if (memPtr != address) {
			Trc_PRT_double_map_regions_Release_Failure2(address, memPtr);
			portLibrary->error_set_last_error_with_message(portLibrary, OMRPORT_ERROR_VMEM_OPFAILED, "Failed to map FIXED block of memory, mapped address differ from fixed address");
			rc = -1;
		}

	} else {
		rc = omrvmem_free_memory(portLibrary, address, byteAmount, identifier);
	}

	Trc_PRT_double_map_regions_Release_Exit(rc);
	return rc;
}

/**
 *  Double maps a contiguous region of memory to discontiguous regions stored in regionAddresses[].
 *  If preferredAddress is NULL it creates a contiguous virtual representation of memory;
 *  otherwise, it uses preferredAddress as the contiguous region (under the assumption that
 *  the caller reserved such contiguous region in memory).
 *
 *  @param OMRPortLibrary               *portLibrary        [in] The port library object
 *  @param void                         *regionAddresses[]  [in] Addresses to be double mapped
 *  @param uintptr_t                    regionsCount        [in] Number of regions to be double mapped
 *  @param uintptr_t                    regionSize          [in] Size of each region
 *  @param uintptr_t                    byteAmount          [in] Total size to allocate for contiguous block of memory
 *  @param struct J9PortVmemIdentifier  *oldIdentifier      [in] Old Identifier containing file descriptor
 *  @param struct J9PortVmemIdentifier  *newIdentifier      [out] New Identifier for new block of memory. The structure to be updated
 *  @param uintptr_t                    mode                [in] Access mode
 *  @param uintptr_t                    pageSize            [in] Constant describing page size
 *  @param OMRMemCategory               *category           [in] Memory allocation category
 *  @param void                         *preferredAddress    [in] Prefered address of contiguous region to be double mapped
 *
 * @return pointer to contiguous region to which regions were double mapped into, NULL is returned if unsuccessful
 */

void *
omrvmem_create_double_mapped_region(struct OMRPortLibrary *portLibrary, void* regionAddresses[], uintptr_t regionsCount, uintptr_t regionSize, uintptr_t byteAmount, struct J9PortVmemIdentifier *oldIdentifier, struct J9PortVmemIdentifier *newIdentifier, uintptr_t mode, uintptr_t pageSize, OMRMemCategory *category, void *preferredAddress)
{
	Trc_PRT_double_map_regions_Create_Entry((void *)regionAddresses, regionsCount, regionSize, byteAmount, pageSize, preferredAddress);
	int protectionFlags = PROT_READ | PROT_WRITE;
	int flags = MAP_PRIVATE | MAP_ANON;
	int fd = -1;
	void* contiguousMap = NULL;
	BOOLEAN successfulContiguousMap = FALSE;
	BOOLEAN shouldUnmapAddr = FALSE;
	BOOLEAN shouldDecrementCategoryCounter = FALSE;
	/* All regions are fully double mapped. Partial double mapping is not supported yet. */
	Assert_PRT_true((regionsCount * regionSize) == byteAmount);

	if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & mode)) {
		protectionFlags = get_protectionBits(mode);
	}

	/* Create contiguous region if we don't have one already */
	if (NULL == preferredAddress) {
		contiguousMap = mmap(
			NULL,
			byteAmount,
			protectionFlags,
			flags,
			fd,
			0);
		Trc_PRT_double_map_regions_Create_GenerateContiguousAddress(contiguousMap);
	} else {
		contiguousMap = preferredAddress;
		Trc_PRT_double_map_regions_Create_UsingPreferredAddress(preferredAddress);
	}

	if ((contiguousMap == MAP_FAILED) || (NULL == contiguousMap)) {
		Trc_PRT_double_map_regions_Create_Failure();
		contiguousMap = NULL;
		portLibrary->error_set_last_error(portLibrary,  errno, OMRPORT_ERROR_VMEM_DOUBLE_MAP_ADRESS_RESERVE_FAILED);
	} else {
		Trc_PRT_double_map_regions_Create_Success(contiguousMap, preferredAddress);
		shouldUnmapAddr = TRUE;
		successfulContiguousMap = TRUE;
		/* If preferredAddress is not NULL, management of virtual memory is taken care by the caller */
		uintptr_t allocator = OMRPORT_VMEM_RESERVE_USED_MMAP_RESTORE_MMAP;
		if (NULL == preferredAddress) {
			allocator = OMRPORT_VMEM_RESERVE_USED_MMAP_SHM;
			omrmem_categories_increment_counters(category, byteAmount);
			shouldDecrementCategoryCounter = TRUE;
		}
		/* Update identifier and commit memory if required */
		update_vmemIdentifier(newIdentifier, contiguousMap, contiguousMap, byteAmount, mode, pageSize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, allocator, category, -1);
		if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & mode)) {
			if (NULL == omrvmem_commit_memory(portLibrary, contiguousMap, byteAmount, newIdentifier)) {
				/* If the commit fails free the memory  */
#if defined(OMRVMEM_DEBUG)
				printf("omrvmem_commit_memory failed at contiguous_region_reserve_memory.\n");
				fflush(stdout);
#endif
				successfulContiguousMap = FALSE;
			}
		}
	}

	/* Perform double mapping  */
	if (successfulContiguousMap) {
		Assert_PRT_true(NULL != contiguousMap);
		flags = MAP_SHARED | MAP_FIXED; // Must be shared, SIGSEGV otherwise
		fd = oldIdentifier->fd;
#if defined(OMRVMEM_DEBUG)
		printf("Found %zu regions.\n", regionsCount);
		for (int j = 0; j < regionsCount; j++) {
			printf("regionAddresses[%d] = %p\n", j, regionAddresses[j]);
		}
		printf("Contiguous address is: %p\n", contiguousMap);
		printf("About to double map regions. File handle got from old identifier: %d\n", fd);
		fflush(stdout);
#endif
		for (int i = 0; i < regionsCount; i++) {
			void *nextAddress = (void *)(contiguousMap + i * regionSize);
			uintptr_t addressOffset = (uintptr_t)(regionAddresses[i] - (uintptr_t)oldIdentifier->address);
			void *address = mmap(
					nextAddress,
					regionSize,
					protectionFlags,
					flags,
					fd,
					addressOffset);

			if (address == MAP_FAILED) {
				Trc_PRT_double_map_regions_Create_Failure2();
				successfulContiguousMap = FALSE;
				portLibrary->error_set_last_error(portLibrary,  errno, OMRPORT_ERROR_VMEM_DOUBLE_MAP_FAILED);
#if defined(OMRVMEM_DEBUG)
				printf("***************************** errno: %d\n", errno);
				printf("Failed to mmap address[%zu] at mmapContiguous()\n", i);
				fflush(stdout);
#endif
				break;
			} else if (nextAddress != address) {
				Trc_PRT_double_map_regions_Create_Failure3(nextAddress, address);
				successfulContiguousMap = FALSE;
				portLibrary->error_set_last_error(portLibrary,  errno, OMRPORT_ERROR_VMEM_DOUBLE_MAP_FAILED);
#if defined(OMRVMEM_DEBUG)
				printf("Map failed to provide the correct address. nextAddress %p != %p\n", nextAddress, address);
				fflush(stdout);
#endif
				break;
			}
		}
	}

	if (!successfulContiguousMap) {
		Trc_PRT_double_map_regions_Create_Failure4();
		if (shouldUnmapAddr) {
			munmap(contiguousMap, byteAmount);
		}
		contiguousMap = NULL;
		if (shouldDecrementCategoryCounter) {
			omrmem_categories_decrement_counters(category, byteAmount);
		}
	}

	Trc_PRT_double_map_regions_Create_Exit(contiguousMap);
	return contiguousMap;
}
