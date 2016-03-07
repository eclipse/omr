/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 * @brief Memory Utilities
 */


/*
 * This file contains code for the portability library memory management.
 * It wraps all memory allocations for RAS tracking.
 * It handles compressed pointer logic locally, no platform specific logic.
 *
 */

/*
 * Note the asymmetry of allocate size 0 and reallocate size 0
 * - allocate returns a valid buffer of size 1 (or 2*sizeof(J9MemTag))
 * - reallocate returns a NULL memoryPointer
 *
 * Note the asymmetry of allocate32 and free32
 * - allocate32 has no special case for size 0, nor does it have a tracepoint for allocation failure
 */
#include <string.h>

#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"

#if defined(OMR_ENV_DATA64)
#include "omrmem32helpers.h"
#endif /* (OMR_ENV_DATA64) */

#include "omrmemtag_checks.h"

static void setTagSumCheck(J9MemTag *tag, uint32_t eyeCatcher);
static void *wrapBlockAndSetTags(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount, const char *callSite, const uint32_t category);
static void *unwrapBlockAndCheckTags(struct OMRPortLibrary *portLibrary, void *memoryPointer);

/* Typedefs for basic allocators */
typedef void *(*allocate_memory_func_t)(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount);
typedef void (*free_memory_func_t)(struct OMRPortLibrary *portLibrary, void *memoryPointer);
typedef void (*advise_and_free_memory_func_t)(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t memorySize);
typedef void *(*reallocate_memory_func_t)(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount);

static void
setTagSumCheck(J9MemTag *tag, uint32_t eyeCatcher)
{
	tag->sumCheck = 0;
	tag->eyeCatcher = eyeCatcher;
	tag->sumCheck = checkTagSumCheck(tag, eyeCatcher);
}

BOOLEAN
isLocatedInIgnoredRegion(struct OMRPortLibrary *portLibrary, void *memoryPointer);

/* A correctly constructed header/footer will sumcheck to zero */
static void *
wrapBlockAndSetTags(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount, const char *callSite, const uint32_t categoryCode)
{
	J9MemTag *headerTag, *footerTag;
	uint8_t *padding;
	OMRMemCategory *category;

	/* Get the tags and adjust the memoryPointer */
	headerTag = (J9MemTag *) memoryPointer;
	footerTag = (J9MemTag *)((uint8_t *) memoryPointer + ROUNDED_FOOTER_OFFSET(byteAmount));
	memoryPointer = (void *)((uint8_t *) memoryPointer + sizeof(J9MemTag));
	padding = (uint8_t *)(((uintptr_t)memoryPointer) + byteAmount);

	while ((uintptr_t)padding != (uintptr_t)footerTag) {
		*padding++ = J9MEMTAG_PADDING_BYTE;
	}

	category = omrmem_get_category(portLibrary, categoryCode);
	omrmem_categories_increment_counters(category, ROUNDED_BYTE_AMOUNT(byteAmount));

	/* Fill in the tags */
	headerTag->allocSize = byteAmount;
	headerTag->callSite = callSite;
	headerTag->category = category;
#if !defined(OMR_ENV_DATA64)
	memset(headerTag->padding, J9MEMTAG_PADDING_BYTE, sizeof(headerTag->padding));
#endif
	setTagSumCheck(headerTag, J9MEMTAG_EYECATCHER_ALLOC_HEADER);

	footerTag->allocSize = byteAmount;
	footerTag->callSite = callSite;
	footerTag->category = category;
#if !defined(OMR_ENV_DATA64)
	memset(headerTag->padding, J9MEMTAG_PADDING_BYTE, sizeof(headerTag->padding));
#endif
	setTagSumCheck(footerTag, J9MEMTAG_EYECATCHER_ALLOC_FOOTER);

	return memoryPointer;
}

static void *
unwrapBlockAndCheckTags(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	J9MemTag *headerTag, *footerTag;

	/* get the tags */
	headerTag = omrmem_get_header_tag(memoryPointer);
	footerTag = omrmem_get_footer_tag(headerTag);

	/* Check the tags and update only if not corrupted*/
	if ((checkTagSumCheck(headerTag, J9MEMTAG_EYECATCHER_ALLOC_HEADER) == 0)
		&& (checkTagSumCheck(footerTag, J9MEMTAG_EYECATCHER_ALLOC_FOOTER) == 0)
		&& (checkPadding(headerTag) == 0)) {

		omrmem_categories_decrement_counters(headerTag->category, ROUNDED_BYTE_AMOUNT(headerTag->allocSize));

		/* Optimized freed header sumCheck setting */
		headerTag->eyeCatcher = J9MEMTAG_EYECATCHER_FREED_HEADER;
		headerTag->sumCheck = headerTag->sumCheck ^ J9MEMTAG_EYECATCHER_ALLOC_HEADER ^ J9MEMTAG_EYECATCHER_FREED_HEADER;
		footerTag->eyeCatcher = J9MEMTAG_EYECATCHER_FREED_FOOTER;
		footerTag->sumCheck = footerTag->sumCheck ^ J9MEMTAG_EYECATCHER_ALLOC_FOOTER ^ J9MEMTAG_EYECATCHER_FREED_FOOTER;
	} else {
		BOOLEAN memoryCorruptionDetected = FALSE;

		portLibrary->portGlobals->corruptedMemoryBlock = memoryPointer;
		Trc_Assert_PRT_memory_corruption_detected(memoryCorruptionDetected);
	}

	return headerTag;
}

/**
 * Allocate memory.
 *
 * @param[in] portLibrary The port library
 * @param[in] byteAmount Number of bytes to allocate.
 * @param[in] callSite String describing callsite, usually file and line number.
 * @param[in] category Memory allocation category code
 *
 * @return pointer to memory on success, NULL on error.
 * @return Memory is not guaranteed to be zeroed as part of this call
 *
 * @note Even if byteAmount is 0, this function will never return NULL on
 * success. It will always return a non-NULL pointer to allocated memory.
 *
 * @internal @warning Do not call error handling code @ref omrerror upon error as
 * the error handling code uses per thread buffers to store the last error.  If memory
 * can not be allocated the result would be an infinite loop.
 */
void *
omrmem_allocate_memory(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category)
{
	void *pointer = NULL;
	uintptr_t allocationByteAmount;
	allocate_memory_func_t allocateFunction = omrmem_allocate_memory_basic;

	/* note that this monitor is protecting a larger area than strictly required but this will make the trace points sane */
	Trc_PRT_mem_omrmem_allocate_memory_Entry(byteAmount, callSite);
	allocationByteAmount = ROUNDED_BYTE_AMOUNT(byteAmount);

	pointer = allocateFunction(portLibrary, allocationByteAmount);
	if (NULL == pointer) {
		Trc_PRT_memory_alloc_returned_null_2(callSite, allocationByteAmount);
	} else {
		pointer = wrapBlockAndSetTags(portLibrary, pointer, byteAmount, callSite, category);
	}
	Trc_PRT_mem_omrmem_allocate_memory_Exit(pointer);
	return pointer;
}

/**
 * Deallocate memory.
 *
 * @param[in] portLibrary The port library
 * @param[in] memoryPointer Base address of memory to be deallocated.
 *
 * @note memoryPointer may be NULL. In that case, no action is taken.
 */
void
omrmem_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	free_memory_func_t freeFunction = omrmem_free_memory_basic;
	Trc_PRT_mem_omrmem_free_memory_Entry(memoryPointer);

	if (memoryPointer != NULL) {
		memoryPointer = unwrapBlockAndCheckTags(portLibrary, memoryPointer);
		freeFunction(portLibrary, memoryPointer);
	}
	Trc_PRT_mem_omrmem_free_memory_Exit();
}

/**
 * Advise and deallocate memory.
 *
 * @param[in] portLibrary The port library
 * @param[in] memoryPointer Base address of memory to be deallocated.
 *
 * @note memoryPointer may be NULL. In that case, no action is taken.
 */
void
omrmem_advise_and_free_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	uintptr_t memorySize = 0;
	advise_and_free_memory_func_t adviseAndFreeFunction = omrmem_advise_and_free_memory_basic;
	Trc_PRT_mem_omrmem_advise_and_free_memory_Entry(memoryPointer);

	if (memoryPointer != NULL) {
#if (defined(LINUX) || defined (AIXPPC) || defined(J9ZOS390) || defined(OSX))

		J9MemTag *headerTag = NULL;
		headerTag = omrmem_get_header_tag(memoryPointer);
		/* Check the header for signs of corruption before we use it in madvise.
		 * No error is raised here b/c one will be raised in 'unwrapBlockAndCheckTags'
		 * below.
		 */
		if ((checkTagSumCheck(headerTag, J9MEMTAG_EYECATCHER_ALLOC_HEADER) == 0) && (checkPadding(headerTag) == 0)) {
			memorySize = ROUNDED_BYTE_AMOUNT(headerTag->allocSize);
		} else {
			memorySize = 0;
		}
#endif /* (defined(LINUX) || defined (AIXPPC) || defined(J9ZOS390) || defined(OSX)) */
		memoryPointer = unwrapBlockAndCheckTags(portLibrary, memoryPointer);
		adviseAndFreeFunction(portLibrary, memoryPointer, memorySize);
	}
	Trc_PRT_mem_omrmem_advise_and_free_memory_Exit();
}

/**
 * Re-allocate memory.
 *
 * @param[in] portLibrary The port library
 * @param[in] memoryPointer Base address of memory to be re-allocated.
 * @param[in] byteAmount Number of bytes to re-allocated.
 * @param[in] callSite Allocation callsite, or NULL if the callsite from the original allocation should be inherited
 * @param[in] category Memory allocation category code
 *
 * @return pointer to memory on success, NULL on error or if byteAmount
 * is 0 and memoryPointer is non-NULL
 *
 * @note if memoryPointer is NULL, this function will behave like mem_allocate_memory
 * for the specified byteAmount
 *
 * @note If byteAmount is 0 and memoryPointer is non-NULL, this function will
 * behave like mem_free_memory for the specified memoryPointer and
 * returns NULL
 *
 * @note If this function is unable to allocate the requested memory, the original
 * memory is not freed and may still be used.
 *
 * @internal @warning Do not call error handling code @ref omrerror upon error as
 * the error handling code uses per thread buffers to store the last error.  If memory
 * can not be allocated the result would be an infinite loop.
 */
void *
omrmem_reallocate_memory(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount, const char *callSite, uint32_t category)
{
	void *pointer = NULL;
	uintptr_t allocationByteAmount;
	reallocate_memory_func_t reallocateFunction = omrmem_reallocate_memory_basic;

	Trc_PRT_mem_omrmem_reallocate_memory_Entry(memoryPointer, byteAmount, callSite, category);

	if (memoryPointer == NULL) {
		pointer = omrmem_allocate_memory(portLibrary, byteAmount, NULL == callSite ? OMR_GET_CALLSITE() : callSite, category);
	} else if (byteAmount == 0) {
		omrmem_free_memory(portLibrary, memoryPointer);
	} else {
		memoryPointer = unwrapBlockAndCheckTags(portLibrary, memoryPointer);
		if (NULL == callSite) {
			/* Inherit the callsite from the original allocation */
			callSite = ((J9MemTag *) memoryPointer)->callSite;
		}
		allocationByteAmount = ROUNDED_BYTE_AMOUNT(byteAmount);

		pointer = reallocateFunction(portLibrary, memoryPointer, allocationByteAmount);
		if (NULL != pointer) {
			pointer = wrapBlockAndSetTags(portLibrary, pointer, byteAmount, callSite, category);
		}
		if (NULL == pointer) {
			Trc_PRT_mem_omrmem_reallocate_memory_failed_2(callSite, memoryPointer, allocationByteAmount);
		}
	}

	Trc_PRT_mem_omrmem_reallocate_memory_Exit(pointer);

	return pointer;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrmem_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library
 *
 * @note Must deallocate portGlobals.
 * @note Most implementations will just deallocate portGlobals.
 */
void
omrmem_shutdown(struct OMRPortLibrary *portLibrary)
{
	omrmem_shutdown_categories(portLibrary);

#if defined(OMR_ENV_DATA64)
	shutdown_memory32(portLibrary);
#endif /* OMR_ENV_DATA64 */

	if (NULL != portLibrary->portGlobals) {
		omrmem_shutdown_basic(portLibrary);
		portLibrary->portGlobals = NULL;
	}
}

/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the memory operations may be created here.  All resources created here should be destroyed
 * in @ref omrmem_shutdown.
 *
 * @param[in] portLibrary The port library
 * @param[in] portGlobalSize Size of the global data structure to allocate
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_MEM
 *.
 * @note Must allocate portGlobals.
 * @note Most implementations will just allocate portGlobals.
 *
 * @internal @note portLibrary->portGlobals must point to an aligned structure
 */
int32_t
omrmem_startup(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize)
{
	uintptr_t categoryStartupRC = 0;
#if defined(OMR_ENV_DATA64)
	uintptr_t memory32StartupRC = 0;
#endif /* OMR_ENV_DATA64 */

	omrmem_startup_basic(portLibrary, portGlobalSize);
	if (NULL == portLibrary->portGlobals) {
		return OMRPORT_ERROR_STARTUP_MEM;
	}

	categoryStartupRC = omrmem_startup_categories(portLibrary);
	if (categoryStartupRC != 0) {
		omrmem_shutdown_basic(portLibrary);
		portLibrary->portGlobals = NULL;
		return OMRPORT_ERROR_STARTUP_MEM;
	}

#if defined(OMR_ENV_DATA64)
	memory32StartupRC = startup_memory32(portLibrary);

	if (memory32StartupRC != 0) {
		omrmem_shutdown_categories(portLibrary);
		omrmem_shutdown_basic(portLibrary);
		portLibrary->portGlobals = NULL;
		return OMRPORT_ERROR_STARTUP_MEM;
	}
#endif /* OMR_ENV_DATA64 */

	return 0;
}

/**
 * @internal Allocate memory for a portLibrary.
 *
 * @param[in] byteAmount Number of bytes to allocate.
 *
 * @return pointer to memory on success, NULL on error.
 * @note This function is called prior to the portLibrary being initialized
 * @note Must be implemented for all platforms.
 */
void *
omrmem_allocate_portLibrary(uintptr_t byteAmount)
{
	return omrmem_allocate_portLibrary_basic(byteAmount);
}

/**
 * @internal Free memory for a portLibrary.
 *
 * @param[in] memoryPointer Base address to be deallocated.
 *
 * @note Must be implemented for all platforms.
 */
void
omrmem_deallocate_portLibrary(void *memoryPointer)
{
	omrmem_deallocate_portLibrary_basic(memoryPointer);
}

/**
 * Allocate memory such that the entire allocated block resides in the low 4Gb of the address space.
 *
 * @param[in] portLibrary The port library
 * @param[in] byteAmount Number of bytes to allocate.
 * @param[in] category Memory allocation category code
 *
 * @return pointer to memory on success, NULL on error.
 * @return Memory is not guaranteed to be zeroed as part of this call
 *
 * @note Even if byteAmount is 0, this function will never return NULL on
 * success. It will always return a non-NULL pointer to allocated memory.
 *
 * @note This function relies on the implementation of omrvmem and as such memory requests will be rounded up to the default page size.
 *
 * @note A return of NULL may indicate only that memory was not available below the 32-bit bar.
 *
 * @internal @warning Do not call error handling code @ref omrerror upon error as
 * the error handling code uses per thread buffers to store the last error.  If memory
 * can not be allocated the result would be an infinite loop.
 */
void *
omrmem_allocate_memory32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite, uint32_t category)
{

	void *pointer = NULL;
#if defined(OMR_ENV_DATA64) && !defined(OSX)
	uintptr_t allocationByteAmount = byteAmount;
#endif /* defined(OMR_ENV_DATA64) && !defined(OSX) */

	Trc_PRT_mem_omrmem_allocate_memory32_Entry(byteAmount);

#if defined(OMR_ENV_DATA64) && !defined(OSX)
	allocationByteAmount = ROUNDED_BYTE_AMOUNT(byteAmount);
	pointer = allocate_memory32(portLibrary, allocationByteAmount, callSite);
	if (NULL == pointer) {
		Trc_PRT_mem_omrmem_allocate_memory32_returned_null(callSite, allocationByteAmount);
	} else {
		pointer = wrapBlockAndSetTags(portLibrary, pointer, byteAmount, callSite, category);
	}
#endif /* defined(OMR_ENV_DATA64) && !defined(OSX) */

	Trc_PRT_mem_omrmem_allocate_memory32_Exit(pointer);

	return pointer;
}

/**
 * Deallocate memory allocated by omrmem_allocate_memory32.
 *
 * @param[in] portLibrary The port library
 * @param[in] memoryPointer Base address of memory to be deallocated.
 *
 * @note memoryPointer may be NULL. In that case, no action is taken.
 */
void
omrmem_free_memory32(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{

	Trc_PRT_mem_omrmem_free_memory32_Entry(memoryPointer);

#if defined(OMR_ENV_DATA64)
	if (memoryPointer != NULL) {
		memoryPointer = unwrapBlockAndCheckTags(portLibrary, memoryPointer);
		free_memory32(portLibrary, memoryPointer);
	}
#endif /* (OMR_ENV_DATA64) */

	Trc_PRT_mem_omrmem_free_memory32_Exit();
}

/**
 * Ensure that byteAmount is available for the sub-allocator in low 4G memory and initialize a sub-allocator heap of that size
 *
 * @param[in] portLibrary The port library
 * @param[in] byteAmount The number of bytes as ensured capacity.
 *
 * @return
 * 	OMRPORT_ENSURE_CAPACITY_SUCCESS: if the sub-allocator has allocated byteAmount in low 4G and initialized a heap of that size,
 * 	OMRPORT_ENSURE_CAPACITY_NOT_REQUIRED: on systems where allocations below the 4 GB bar are not required,
 * 	OMRPORT_ENSURE_CAPACITY_FAILED: an error occurred.
 */
uintptr_t
omrmem_ensure_capacity32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount)
{
	uintptr_t retValue = OMRPORT_ENSURE_CAPACITY_FAILED;

	Trc_PRT_mem_omrmem_ensure_capacity32_Entry(byteAmount);

#if defined(OMR_ENV_DATA64)
	retValue = ensure_capacity32(portLibrary, byteAmount);
#endif /* (OMR_ENV_DATA64) */

	Trc_PRT_mem_omrmem_ensure_capacity32_Exit(retValue);

	return retValue;
}

