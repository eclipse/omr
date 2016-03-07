/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2010, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thread_internal.h"
#include "omrmemcategories.h"
#include "AtomicSupport.hpp"

extern "C" {

/* Template category data to be copied into the thread library structure in omrthread_mem_init */
#if defined(OMR_THR_FORK_SUPPORT)
const uint32_t threadCategoryChildren[] = {OMRMEM_CATEGORY_THREADS_RUNTIME_STACK, OMRMEM_CATEGORY_THREADS_NATIVE_STACK, OMRMEM_CATEGORY_OSMUTEXES, OMRMEM_CATEGORY_OSCONDVARS};
const OMRMemCategory threadCategoryTemplate = { "Threads", OMRMEM_CATEGORY_THREADS, 0, 0, 0, 0, 4, threadCategoryChildren };
const OMRMemCategory mutexCategoryTemplate = { "OS Mutexes", OMRMEM_CATEGORY_OSMUTEXES, 0, 0, 0, 0, 0, NULL };
const OMRMemCategory condvarCategoryTemplate = { "OS Condvars", OMRMEM_CATEGORY_OSCONDVARS, 0, 0, 0, 0, 0, NULL };
#else /* defined(OMR_THR_FORK_SUPPORT) */
const uint32_t threadCategoryChildren[] = {OMRMEM_CATEGORY_THREADS_RUNTIME_STACK, OMRMEM_CATEGORY_THREADS_NATIVE_STACK};
const OMRMemCategory threadCategoryTemplate = { "Threads", OMRMEM_CATEGORY_THREADS, 0, 0, 0, 0, 2, threadCategoryChildren };
#endif /* defined(OMR_THR_FORK_SUPPORT) */
const OMRMemCategory nativeStackCategoryTemplate = { "Native Stack", OMRMEM_CATEGORY_THREADS_NATIVE_STACK, 0, 0, 0, 0, 0, NULL };


typedef struct J9ThreadMemoryHeader {
	uintptr_t blockSize;
#if ! defined(OMR_ENV_DATA64)
	/* Padding to ensure 8-byte alignment on 32 bit */
	uint8_t padding[4];
#endif
} J9ThreadMemoryHeader;

/**
 * Initializes omrthread memory.
 *
 * Called by omrthread_init
 */
void
omrthread_mem_init(omrthread_library_t threadLibrary)
{
	memcpy(&threadLibrary->threadLibraryCategory, &threadCategoryTemplate, sizeof(OMRMemCategory));
	memcpy(&threadLibrary->nativeStackCategory, &nativeStackCategoryTemplate, sizeof(OMRMemCategory));
#if defined(OMR_THR_FORK_SUPPORT)
	memcpy(&threadLibrary->mutexCategory, &mutexCategoryTemplate, sizeof(OMRMemCategory));
	memcpy(&threadLibrary->condvarCategory, &condvarCategoryTemplate, sizeof(OMRMemCategory));
#endif /* defined(OMR_THR_FORK_SUPPORT) */
}

/**
 * Thin-wrapper for malloc that increments the OMRMEM_CATEGORY_THREADS memory
 * counter.
 * @param [in] threadLibrary Thread library
 * @param [in] size Size of memory block to allocate
 * @param [in] memoryCategory Memory category (Currently ignored)
 * @return Pointer to allocated memory, or NULL if the allocation failed.
 */
void *
omrthread_allocate_memory(omrthread_library_t threadLibrary, uintptr_t size, uint32_t memoryCategory)
{
	uintptr_t paddedSize = size + sizeof(J9ThreadMemoryHeader);
	J9ThreadMemoryHeader *header;
	void *mallocPtr;

	mallocPtr = malloc(paddedSize);

	if (NULL == mallocPtr) {
		return mallocPtr;
	}

	header = (J9ThreadMemoryHeader *)mallocPtr;
	header->blockSize = paddedSize;

	increment_memory_counter(&threadLibrary->threadLibraryCategory, paddedSize);

	return (void *)((uintptr_t)mallocPtr + sizeof(J9ThreadMemoryHeader));
}

/**
 * Thin-wrapper for free that decrements the OMRMEM_CATEGORY_THREADS memory
 *  counter.
 * @param [in] threadLibrary Thread library
 * @param [in] ptr Pointer to free.
 */
void
omrthread_free_memory(omrthread_library_t threadLibrary, void *ptr)
{
	void *mallocPtr;
	J9ThreadMemoryHeader *header;

	if (NULL == ptr) {
		return;
	}

	mallocPtr = (void *)((uintptr_t)ptr - sizeof(J9ThreadMemoryHeader));

	header = (J9ThreadMemoryHeader *)mallocPtr;
	decrement_memory_counter(&threadLibrary->threadLibraryCategory, header->blockSize);

	free(mallocPtr);
}

/**
 * Updates memory category with an allocation
 *
 * @param [in] category Category to be updated
 * @param [in] size Size of memory block allocated
 */
void
increment_memory_counter(OMRMemCategory *category, uintptr_t size)
{
	/* Increment block count */
	VM_AtomicSupport::add(&category->liveAllocations, 1);

	/* Increment size */
	VM_AtomicSupport::add(&category->liveBytes, size);
}

/**
 * Updates memory category with a free.
 *
 * @param [in] category Category to be updated
 * @param [in] size Size of memory block freed
 */
void
decrement_memory_counter(OMRMemCategory *category, uintptr_t size)
{
	/* Decrement block count */
	VM_AtomicSupport::subtract(&category->liveAllocations, 1);

	/* Decrement size */
	VM_AtomicSupport::subtract(&category->liveBytes, size);
}

/**
 * Free memory.
 * Function used by the library's pools.
 *
 * @param[in] user user data (thread library pointer)
 * @param[in] ptr pointer to memory to be freed
 * @param[in] type ignored
 * @return none
 *
 */
void
omrthread_freeWrapper(void *user, void *ptr, uint32_t type)
{
	omrthread_free_memory((omrthread_library_t)user, ptr);
}


/**
 * Malloc memory.
 * Helper function used by the library's pools.
 *
 * @param[in] user user data (thread library pointer)
 * @param[in] size size of struct to be alloc'd
 * @param[in] callsite ignored
 * @param[in] memoryCategory
 * @param[in] type ignored
 * @param[out] doInit ignored
 * @return pointer to the malloc'd memory
 * @retval NULL on failure
 *
 */
void *
omrthread_mallocWrapper(void *user, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit)
{
	return omrthread_allocate_memory((omrthread_library_t)user, size, memoryCategory);
}

} /* extern "C" */
