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

#ifndef OMRMEMCATEGORIES_H
#define OMRMEMCATEGORIES_H
#include <stdint.h>

#include "omrcfg.h"

typedef struct OMRMemCategory {
	const char *const name;
	const uint32_t categoryCode;
	uintptr_t liveBytes;
	uintptr_t liveBytesLockWord;
	uintptr_t liveAllocations;
	uintptr_t liveAllocationsLockWord;
	const uint32_t numberOfChildren;
	const uint32_t *const children;
} OMRMemCategory;

typedef struct OMRMemCategorySet {
	uint32_t numberOfCategories;
	OMRMemCategory **categories;
} OMRMemCategorySet;

/*
 * To add a new category:
 * - Add a new #define to the end of this list
 * - Add the corresponding entries in omrrasinit.c
 *
 * Don't delete old categories. Only add new categories at the
 * end of the range. OMR uses the high categories from 0x80000000
 * upwards internally to allow language implementors to use
 * 0x0 and upwards.
 */
/* Special memory category for memory allocated for unknown categories */
#define OMRMEM_CATEGORY_UNKNOWN 0x80000000
/* Special memory category for memory allocated for the port library itself */
#define OMRMEM_CATEGORY_PORT_LIBRARY 0x80000001
#define OMRMEM_CATEGORY_VM 0x80000002
#define OMRMEM_CATEGORY_MM 0x80000003
#define OMRMEM_CATEGORY_THREADS 0x80000004
#define OMRMEM_CATEGORY_THREADS_RUNTIME_STACK 0x80000005
#define OMRMEM_CATEGORY_THREADS_NATIVE_STACK 0x80000006
#define OMRMEM_CATEGORY_TRACE 0x80000007
#define OMRMEM_CATEGORY_OMRTI 0x80000008

/* Special memory category for *unused* sections of regions allocated for <32bit allocations on 64 bit.
 * The used sections will be accounted for under the categories they are used by. */
#define OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS 0x80000009
#define OMRMEM_CATEGORY_MM_RUNTIME_HEAP 0x8000000A

#define OMRMEM_CATEGORY_JIT 0x8000000B
#define OMRMEM_CATEGORY_JIT_CODE_CACHE 0x8000000C
#define OMRMEM_CATEGORY_JIT_DATA_CACHE 0x8000000D

#if defined(OMR_THR_FORK_SUPPORT)
#define OMRMEM_CATEGORY_OSMUTEXES 0x8000000E
#define OMRMEM_CATEGORY_OSCONDVARS 0x8000000F
#endif /* defined(OMR_THR_FORK_SUPPORT) */

#if defined(OMR_OPT_CUDA)
#define OMRMEM_CATEGORY_CUDA 0x80000010
#endif /* OMR_OPT_CUDA */

/* Helper macro to convert the category codes to indices starting from 0 */
#define OMRMEM_LANGUAGE_CATEGORY_LIMIT 0x7FFFFFFF
#define OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(code) (((uint32_t)0x7FFFFFFF) & (code))

#define OMRMEM_CATEGORY_NO_CHILDREN(description, code) \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 0, NULL}
#define OMRMEM_CATEGORY_1_CHILD(description, code, c1) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 1, _omrmem_##code##_child_categories}
#define OMRMEM_CATEGORY_2_CHILDREN(description, code, c1, c2) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1, c2}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 2, _omrmem_##code##_child_categories}
#define OMRMEM_CATEGORY_3_CHILDREN(description, code, c1, c2, c3) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1, c2, c3}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 3, _omrmem_##code##_child_categories}
#define OMRMEM_CATEGORY_4_CHILDREN(description, code, c1, c2, c3, c4) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1, c2, c3, c4}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 4, _omrmem_##code##_child_categories}
#define OMRMEM_CATEGORY_5_CHILDREN(description, code, c1, c2, c3, c4, c5) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1, c2, c3, c4, c5}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 5, _omrmem_##code##_child_categories}
#define OMRMEM_CATEGORY_6_CHILDREN(description, code, c1, c2, c3, c4, c5, c6) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1, c2, c3, c4, c5, c6}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 6, _omrmem_##code##_child_categories}
#define OMRMEM_CATEGORY_7_CHILDREN(description, code, c1, c2, c3, c4, c5, c6, c7) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1, c2, c3, c4, c5, c6, c7}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 7, _omrmem_##code##_child_categories}
#define OMRMEM_CATEGORY_8_CHILDREN(description, code, c1, c2, c3, c4, c5, c6, c7, c8) \
	static uint32_t _omrmem_##code##_child_categories[] = {c1, c2, c3, c4, c5, c6, c7, c8}; \
	static OMRMemCategory _omrmem_category_##code = {description, code, 0, 0, 0, 0, 8, _omrmem_##code##_child_categories}

#define CATEGORY_TABLE_ENTRY(name) &_omrmem_category_##name

#endif /* OMRMEMCATEGORIES_H */
