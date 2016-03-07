/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016
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

#if 0
#define J9VMEM_DEBUG
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
void update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address, void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t allocator, OMRMemCategory *category);
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
#if defined(J9VMEM_DEBUG)
				printf("\t\t omrvmem_commit_memory called mprotect, returning 0x%zx\n", address);
				fflush(stdout);
#endif /* defined(J9VMEM_DEBUG) */
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

#if defined(J9VMEM_DEBUG)
	printf("\t\t omrvmem_commit_memory returning 0x%x\n", rc);
	fflush(stdout);
#endif /* defined(J9VMEM_DEBUG) */
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

	Trc_PRT_vmem_omrvmem_free_memory_Entry(address, byteAmount);

	/* CMVC 180372 - Some users store the identifier in the allocated memory.
	 * So, identifier must be cleared before memory is freed.
	 */
	update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);

	ret = (int32_t)munmap(address, (size_t)byteAmount);
	omrmem_categories_decrement_counters(category, byteAmount);

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
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
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
#if defined(J9VMEM_DEBUG)
				printf("\t\t\t NULL == memoryPointer, reverting to default pages\n");
				fflush(stdout);
#endif /* defined(J9VMEM_DEBUG) */
				uintptr_t defaultPageSize = PPG_vmem_pageSize[0];
				uintptr_t alignmentInBytes = OMR_MAX(defaultPageSize, params->alignmentInBytes);
				uintptr_t minimumGranule = OMR_MIN(defaultPageSize, params->alignmentInBytes);

				/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
				if ((0 == minimumGranule) || (0 == (alignmentInBytes % minimumGranule))) {
					memoryPointer = getMemoryInRange(portLibrary, identifier, category, params->byteAmount, params->startAddress, params->endAddress, alignmentInBytes, params->options, PPG_vmem_pageSize[0], OMRPORT_VMEM_PAGE_FLAG_NOT_USED, params->mode);
				}
			} else {
				update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
			}
		}
	} else {
		/* If the pageSize is not one of the supported page sizes, error */
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
		Trc_PRT_vmem_omrvmem_reserve_memory_unsupported_page_size(params->pageSize);
	}


#if defined(J9VMEM_DEBUG)
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

	flags |= MAP_ANON;
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

	result = mmap(address, (size_t)byteAmount, protectionFlags, flags, fd, 0);
	if (MAP_FAILED == result) {
		result = NULL;
	} else {
		/* Update identifier and commit memory if required, else return reserved memory */
		update_vmemIdentifier(identifier, result, result, byteAmount, mode, pageSize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, OMRPORT_VMEM_RESERVE_USED_MMAP, category);
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
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, 0, NULL);
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
update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address, void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, uintptr_t allocator, OMRMemCategory *category)
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

	if (NULL == memoryPointer) {
		/* Try all addresses within range */
		while ((startAddress <= currentAddress) && (endAddress >= currentAddress)) {
			memoryPointer = reserveMemory(portLibrary, currentAddress, byteAmount, identifier, mode, pageSize, pageFlags, category);

			if (NULL != memoryPointer) {
				/* stop if returned pointer is within range */
				if ((startAddress <= memoryPointer) && (endAddress >= memoryPointer)) {
					break;
				}
				if (0 != omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier)) {
					return NULL;
				}
				memoryPointer = NULL;
			}

			oldAddress = currentAddress;

			currentAddress += direction * alignmentInBytes;

			/* Protect against loop around */
			if (((1 == direction) && ((uintptr_t)oldAddress > (uintptr_t)currentAddress)) ||
				((-1 == direction) && ((uintptr_t)oldAddress < (uintptr_t)currentAddress))) {
				break;
			}
		}
	}

	/* If strict flag is not set and we did not get any memory, attempt to get memory at any address */
	if (0 == (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions) && (NULL == memoryPointer)) {
allocAnywhere:
		memoryPointer = reserveMemory(portLibrary, NULL, byteAmount, identifier, mode, pageSize, pageFlags, category);
	}

	if (NULL == memoryPointer) {
		memoryPointer = NULL;
	} else if (isStrictAndOutOfRange(memoryPointer, startAddress, endAddress, vmemOptions)) {
		/* If strict flag is set and returned pointer is not within range then fail */
		omrvmem_free_memory(portLibrary, memoryPointer, byteAmount, identifier);

		Trc_PRT_vmem_omrvmem_reserve_memory_ex_UnableToAllocateWithinSpecifiedRange(byteAmount, startAddress, endAddress);

		memoryPointer = NULL;
	}

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
