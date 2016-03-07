/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2010, 2015
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
 * @brief Memory category management
 */


/*
 * This file contains the code for managing memory categories.
 *
 * Memory categories are used to break down native memory usage under
 * areas a language programmer would understand.
 */
#include <stdlib.h>
#include <string.h>

#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"

/* J9VMAtomicFunctions*/
#ifndef _J9VMATOMICFUNCTIONS_
#define _J9VMATOMICFUNCTIONS_
extern uintptr_t compareAndSwapUDATA(uintptr_t *location, uintptr_t oldValue, uintptr_t newValue, uintptr_t *spinlock);
extern uint32_t compareAndSwapU32(uint32_t *location, uint32_t oldValue, uint32_t newValue, uintptr_t *spinlock);
#endif /* _J9VMATOMICFUNCTIONS_ */


/* Templates for categories that are copied into malloc'd memory in omrmem_startup_categories */
OMRMEM_CATEGORY_NO_CHILDREN("Unknown", OMRMEM_CATEGORY_UNKNOWN);

#if defined(OMR_ENV_DATA64)
/* On 64 bit we define an additional special category to cover unused regions of allocate32 slabs. */
OMRMEM_CATEGORY_NO_CHILDREN("Unused <32bit allocation regions", OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS);
OMRMEM_CATEGORY_1_CHILD("Port Library", OMRMEM_CATEGORY_PORT_LIBRARY, OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS);
#else
OMRMEM_CATEGORY_NO_CHILDREN("Port Library", OMRMEM_CATEGORY_PORT_LIBRARY);
#endif /* OMR_ENV_DATA64 */

/**
 * Increments the counters for a memory category.
 *
 * Called by port library code when a memory allocation is made.
 */
void
omrmem_categories_increment_counters(OMRMemCategory *category, uintptr_t size)
{
	uintptr_t oldValue;

	Trc_Assert_PTR_mem_categories_increment_counters_NULL_category(NULL != category);

	/* Increment block count */
	do {
		oldValue = category->liveAllocations;
	} while (compareAndSwapUDATA(&category->liveAllocations, oldValue, oldValue + 1, & category->liveAllocationsLockWord) != oldValue);

	omrmem_categories_increment_bytes(category, size);
}

/**
 * Increments just the byte counter for a memory category
 *
 * Used in omrmem32helpers to manage <32 bit slab regions
 */
void
omrmem_categories_increment_bytes(OMRMemCategory *category, uintptr_t size)
{
	uintptr_t oldValue;

	Trc_Assert_PTR_mem_categories_increment_bytes_NULL_category(NULL != category);

	/* Increment bytes */
	do {
		oldValue = category->liveBytes;
	} while (compareAndSwapUDATA(&category->liveBytes, oldValue, oldValue + size, &category->liveBytesLockWord) != oldValue);
}

/**
 * Decrements the counters for a memory category.
 *
 * Called by port library code when a memory block is freed.
 */
void
omrmem_categories_decrement_counters(OMRMemCategory *category, uintptr_t size)
{
	uintptr_t oldValue;

	Trc_Assert_PTR_mem_categories_decrement_counters_NULL_category(NULL != category);

	/* Decrement block count */
	do {
		oldValue = category->liveAllocations;
	} while (compareAndSwapUDATA(&category->liveAllocations, oldValue, oldValue - 1, & category->liveAllocationsLockWord) != oldValue);

	omrmem_categories_decrement_bytes(category, size);
}

/**
 * Decrements the byte counter for a memory category.
 *
 * Used in omrmem32helpers to manage <32 bit slab regions
 */
void
omrmem_categories_decrement_bytes(OMRMemCategory *category, uintptr_t size)
{
	uintptr_t oldValue;

	Trc_Assert_PTR_mem_categories_decrement_bytes_NULL_category(NULL != category);

	/* Decrement size */
	do {
		oldValue = category->liveBytes;
	} while (compareAndSwapUDATA(&category->liveBytes, oldValue, oldValue - size, &category->liveBytesLockWord) != oldValue);
}

/**
 * Returns a reference to the OMRMemCategory structure represented by categoryCode.
 *
 * Will always return a valid reference.
 *
 * @param portLibrary   [in]   Port library instance.
 * @param categoryCode  [in]   Category code to look-up
 */
OMRMemCategory *
omrmem_get_category(struct OMRPortLibrary *portLibrary, uint32_t categoryCode)
{

	J9PortControlData *portControl = &(portLibrary->portGlobals->control);
	if (categoryCode < OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
		if (categoryCode < portControl->language_memory_categories.numberOfCategories
			&& NULL != portControl->language_memory_categories.categories[categoryCode]) {
			return portControl->language_memory_categories.categories[categoryCode];
		}
	} else if (categoryCode > OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
		uint32_t categoryIndex = OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(categoryCode);
		if (categoryIndex < portControl->omr_memory_categories.numberOfCategories
			&& NULL != portControl->omr_memory_categories.categories[categoryIndex]) {
			return portControl->omr_memory_categories.categories[categoryIndex];
		}
	}
	/**
	 * Nothing past here should be called after port control has intialised the categories,
	 * unless someone uses a bad category code.
	 * PORT_LIBRARY and ALLOCATE32 are inserted in the category tables but the port library
	 * may use them before port control is called.
	 * The hot path should be above here.
	 */
	if (OMRMEM_CATEGORY_PORT_LIBRARY == categoryCode) {
		return &portLibrary->portGlobals->portLibraryMemoryCategory;
	}
#if defined(OMR_ENV_DATA64)
	if (OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS == categoryCode) {
		return &portLibrary->portGlobals->unusedAllocate32HeapRegionsMemoryCategory;
	}
#endif

	/* No categories supplied by the application - map to unknown */
	return &portLibrary->portGlobals->unknownMemoryCategory;

}

static uintptr_t
_recursive_category_walk_children(struct OMRPortLibrary *portLibrary, OMRMemCategoryWalkState *state, OMRMemCategory *parent)
{
	uint32_t i;
	uintptr_t result;

	for (i = 0; i < parent->numberOfChildren; i++) {
		uint32_t childCode = parent->children[i];
		OMRMemCategory *child = omrmem_get_category(portLibrary, childCode);
		result = state->walkFunction(child->categoryCode, child->name, child->liveBytes, child->liveAllocations, FALSE, parent->categoryCode, state);

		if (result == J9MEM_CATEGORIES_KEEP_ITERATING) {
			result = _recursive_category_walk_children(portLibrary, state, child);

			if (result != J9MEM_CATEGORIES_KEEP_ITERATING) {
				return result;
			}
		} else {
			return result;
		}
	}

	return J9MEM_CATEGORIES_KEEP_ITERATING;
}

static uintptr_t
_recursive_category_walk_root(struct OMRPortLibrary *portLibrary, OMRMemCategoryWalkState *state, OMRMemCategory *walkPoint)
{
	uintptr_t result;

	result = state->walkFunction(walkPoint->categoryCode, walkPoint->name, walkPoint->liveBytes, walkPoint->liveAllocations, TRUE, 0, state);

	if (result == J9MEM_CATEGORIES_KEEP_ITERATING) {
		return _recursive_category_walk_children(portLibrary, state, walkPoint);
	} else {
		return result;
	}
}

/**
 * Walks registered omrmem categories and calls back to application code
 *
 * @param[in] portLibrary             Port library
 * @param[in] state                   Walk state containing callback pointer
 *
 */
void
omrmem_walk_categories(struct OMRPortLibrary *portLibrary, OMRMemCategoryWalkState *state)
{
	/* User supplied categories are expected to include PORT_LIBRARY and UNKNOWN as part of their tree */
	if (portLibrary->portGlobals->control.language_memory_categories.categories != NULL) {
		_recursive_category_walk_root(portLibrary, state, portLibrary->portGlobals->control.language_memory_categories.categories[0]);
	} else {
		uintptr_t result = _recursive_category_walk_root(portLibrary, state, &portLibrary->portGlobals->portLibraryMemoryCategory);
		if (result != J9MEM_CATEGORIES_KEEP_ITERATING) {
			return;
		}

		result = _recursive_category_walk_root(portLibrary, state, &portLibrary->portGlobals->unknownMemoryCategory);
		if (result != J9MEM_CATEGORIES_KEEP_ITERATING) {
			return;
		}

#if defined(OMR_ENV_DATA64)
		_recursive_category_walk_root(portLibrary, state, &portLibrary->portGlobals->unusedAllocate32HeapRegionsMemoryCategory);
#endif
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
 *
 * @note Sets up categories for "Unknown" and "Port Library". Others will be supplied by the application.
 *
 */
int32_t
omrmem_startup_categories(struct OMRPortLibrary *portLibrary)
{
	memcpy(&portLibrary->portGlobals->unknownMemoryCategory, CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_UNKNOWN), sizeof(OMRMemCategory));
	memcpy(&portLibrary->portGlobals->portLibraryMemoryCategory, CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_PORT_LIBRARY), sizeof(OMRMemCategory));
#if defined(OMR_ENV_DATA64)
	memcpy(&portLibrary->portGlobals->unusedAllocate32HeapRegionsMemoryCategory, CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS), sizeof(OMRMemCategory));
#endif
	portLibrary->portGlobals->control.language_memory_categories.numberOfCategories = 0;
	portLibrary->portGlobals->control.language_memory_categories.categories = NULL;
	portLibrary->portGlobals->control.omr_memory_categories.numberOfCategories = 0;
	portLibrary->portGlobals->control.omr_memory_categories.categories = NULL;
	return 0;
}

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.
 *
 * @param[in] portLibrary The port library
 *
 */
void
omrmem_shutdown_categories(struct OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	/* Free any allocated memory categories data. */
	if (NULL != portLibrary->portGlobals->control.language_memory_categories.categories) {
		portLibrary->mem_free_memory(OMRPORTLIB, portLibrary->portGlobals->control.language_memory_categories.categories);
		portLibrary->portGlobals->control.language_memory_categories.categories = NULL;
		portLibrary->portGlobals->control.language_memory_categories.numberOfCategories = 0;
	}
	if (NULL != portLibrary->portGlobals->control.omr_memory_categories.categories) {
		portLibrary->mem_free_memory(OMRPORTLIB, portLibrary->portGlobals->control.omr_memory_categories.categories);
		portLibrary->portGlobals->control.omr_memory_categories.categories = NULL;
		portLibrary->portGlobals->control.omr_memory_categories.numberOfCategories = 0;
	}
}
