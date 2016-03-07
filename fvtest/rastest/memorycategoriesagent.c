/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "omragent.h"
#include "omrTestHelpers.h"

typedef struct OMRMemoryCategoriesTestData {
	omr_error_t rc;
} OMRMemoryCategoriesTestData;

static omr_error_t
testMemoryCategoriesFromAttachedThread(OMR_VMThread *vmThread, OMR_TI const *ti, OMRPortLibrary *portLibrary);
static omr_error_t
testMemoryCategoriesFromUnattachedThread(OMR_VMThread *vmThread, OMR_TI const *ti, OMRPortLibrary *portLibrary);

static BOOLEAN
validateCategories(OMRPortLibrary *portLibrary, OMR_TI_MemoryCategory *categories_buffer, int32_t written_count);
static void
printMemoryCategories(OMRPortLibrary *portLib, OMR_TI_MemoryCategory *categories_buffer, int32_t written_count);

static OMRMemoryCategoriesTestData testData;

/*
 * Use the function below with:
 * ((OMR_TI*)ti)->GetMemoryCategories = badGetMemoryCategories_AlwaysErrorNone;
 * in the main test sections to validate test case error detection.
 * It doesn't set any of the values the test case expects so should fail most of the time.)
 * (Adjust as needed for other cases.)
 */
/*
 omr_error_t
 badGetMemoryCategories_AlwaysErrorNone(OMR_VMThread *vmThread, int32_t max_categories,
 OMR_TI_MemoryCategory *categories_buffer, int32_t *written_count_ptr, int32_t *total_categories_ptr)
 {
 return OMR_ERROR_NONE;
 }
 */

omr_error_t
OMRAgent_OnLoad(OMR_TI const *ti, OMR_VM *vm, char const *options, OMR_AgentCallbacks *agentCallbacks, ...)
{
	OMRPORT_ACCESS_FROM_OMRVM(vm);
	OMR_VMThread *vmThread = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	testData.rc = OMR_ERROR_NONE;

	/* check GetMemoryCategories pointer is set in ti interface. */
	if (OMR_ERROR_NONE == rc) {
		if (NULL == ti) {
			omrtty_err_printf("%s:%d: NULL OMR_TI interface pointer.\n");
			rc = OMR_ERROR_INTERNAL;
		} else if (NULL == ti->GetMemoryCategories) {
			omrtty_err_printf("%s:%d: NULL OMR_TI->GetMemoryCategories pointer.\n");
			rc = OMR_ERROR_INTERNAL;
		}
	}

	/* exercise trace API without attaching thread to VM */
	if (OMR_ERROR_NONE == rc) {
		rc = testMemoryCategoriesFromUnattachedThread(vmThread, ti, OMRPORTLIB);
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->BindCurrentThread(vm, "main", &vmThread));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(testMemoryCategoriesFromAttachedThread(vmThread, ti, OMRPORTLIB));
	}

	if (OMR_ERROR_NONE == rc) {
		rc = OMRTEST_PRINT_ERROR(ti->UnbindCurrentThread(vmThread));
	}

	testData.rc = rc;
	return rc;
}

omr_error_t
OMRAgent_OnUnload(OMR_TI const *ti, OMR_VM *vm)
{
	return OMR_ERROR_NONE;
}

static omr_error_t
testMemoryCategoriesFromUnattachedThread(OMR_VMThread *vmThread, OMR_TI const *ti, OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	omr_error_t rc = OMR_ERROR_NONE;
	omr_error_t testRc = OMR_ERROR_NONE;

	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetMemoryCategories(vmThread, 0, NULL, NULL, NULL),
										 OMR_THREAD_NOT_ATTACHED);
		if (OMR_THREAD_NOT_ATTACHED != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}
	return testRc;
}

static omr_error_t
testMemoryCategoriesFromAttachedThread(OMR_VMThread *vmThread, OMR_TI const *ti, OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	omr_error_t testRc = OMR_ERROR_NONE;
	omr_error_t rc = OMR_ERROR_NONE;
	int32_t written_count = -1;
	int32_t total_categories = 0;
	int32_t initial_total_categories = 0;
	int32_t *total_categories_null = NULL;
	OMR_TI_MemoryCategory *memoryCategories = NULL;

	/* Negative path: try non-zero max_categories with NULL categories_buffer */
	rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetMemoryCategories(vmThread, 1, NULL, NULL, NULL),
									 OMR_ERROR_ILLEGAL_ARGUMENT);
	if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
		testRc = OMR_ERROR_INTERNAL;
	}

	/* Negative path: try all output pointers NULL */
	rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetMemoryCategories(vmThread, 0, NULL, NULL, NULL),
									 OMR_ERROR_ILLEGAL_ARGUMENT);
	if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
		testRc = OMR_ERROR_INTERNAL;
	}

	/* Positive path: query total number of categories */
	total_categories = -1;
	rc = OMRTEST_PRINT_ERROR(ti->GetMemoryCategories(vmThread, 0, NULL, NULL, &total_categories));
	if (OMR_ERROR_NONE != rc) {
		testRc = OMR_ERROR_INTERNAL;
	} else {
		if (-1 == total_categories) {
			testRc = OMR_ERROR_INTERNAL;
		} else {
			initial_total_categories = total_categories;
		}
	}

	/* Allocate our buffer */
	if (OMR_ERROR_NONE == testRc) {
		memoryCategories = omrmem_allocate_memory(total_categories * sizeof(OMR_TI_MemoryCategory),
						   OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == memoryCategories) {
			testRc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		} else {
			memset(memoryCategories, 0, total_categories * sizeof(OMR_TI_MemoryCategory));
		}
	}

	/* Negative path: get categories with a NULL written_count_ptr */
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(
				 ti->GetMemoryCategories(vmThread, total_categories, memoryCategories, NULL, &total_categories),
				 OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	/* Negative path: get categories with an undersized buffer */
	written_count = -1;
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("\nNegative path: get categories with an undersized buffer\n");
		rc = OMRTEST_PRINT_UNEXPECTED_RC(
				 ti->GetMemoryCategories(vmThread, total_categories - 1, memoryCategories, &written_count,
										 &total_categories), OMR_ERROR_OUT_OF_NATIVE_MEMORY);
		if (OMR_ERROR_OUT_OF_NATIVE_MEMORY != rc) {
			testRc = OMR_ERROR_INTERNAL;
		} else {
			if (-1 == written_count) {
				omrtty_err_printf("%s:%d:Expected written_count to be assigned, even on negative path.");
				testRc = OMR_ERROR_INTERNAL;
			} else if ((total_categories - 1) != written_count) {
				omrtty_err_printf("%s:%d:Expected written_count to be equal to total_cateogries -1.");
				testRc = OMR_ERROR_INTERNAL;
			} else if (initial_total_categories != total_categories) {
				omrtty_err_printf("%s:%d:Expected total_categories to remain constant.");
				testRc = OMR_ERROR_INTERNAL;
			} else if (!validateCategories(OMRPORTLIB, memoryCategories, written_count)) {
				testRc = OMR_ERROR_INTERNAL;
			}
			omrtty_printf("   written_count=%d, total_categories=%d\n", written_count, total_categories);
			printMemoryCategories(OMRPORTLIB, memoryCategories, written_count);
		}
	}

	written_count = -1;
	/* Positive path: get and validate categories */
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("\nPositive path: get and validate categories\n");
		rc = OMRTEST_PRINT_ERROR(
				 ti->GetMemoryCategories(vmThread, total_categories, memoryCategories, &written_count,
										 &total_categories));
		if (OMR_ERROR_NONE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		} else if (total_categories != written_count) {
			omrtty_err_printf("%s:%d:Expected written_count to be equal to total_cateogries.");
			testRc = OMR_ERROR_INTERNAL;
		} else if (initial_total_categories != total_categories) {
			omrtty_err_printf("%s:%d:Expected total_categories to remain constant.");
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("   written_count=%d, total_categories=%d\n", written_count, total_categories);
			if (!validateCategories(OMRPORTLIB, memoryCategories, written_count)) {
				testRc = OMR_ERROR_INTERNAL;
			}
			printMemoryCategories(OMRPORTLIB, memoryCategories, written_count);
		}
	}

	/* Positive path: get and validate categories and total_categories is null */
	written_count = -1;
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("\nPositive path: get and validate categories and total_categories is null\n");
		rc = OMRTEST_PRINT_ERROR(ti->GetMemoryCategories(vmThread, total_categories, memoryCategories, &written_count, total_categories_null));
		if (OMR_ERROR_NONE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		} else if (total_categories != written_count) {
			omrtty_err_printf("%s:%d:Expected written_count to be equal to total_cateogries.");
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("   written_count=%d\n", written_count);
			if (!validateCategories(OMRPORTLIB, memoryCategories, written_count)) {
				testRc = OMR_ERROR_INTERNAL;
			}
			printMemoryCategories(OMRPORTLIB, memoryCategories, written_count);
		}
	}

	/* Allocate our bigger buffer */
	if (OMR_ERROR_NONE == testRc) {
		omrmem_free_memory(memoryCategories);
		memoryCategories = omrmem_allocate_memory((total_categories + 1) * sizeof(OMR_TI_MemoryCategory),
						   OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == memoryCategories) {
			testRc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		} else {
			memset(memoryCategories, 0, (total_categories + 1) * sizeof(OMR_TI_MemoryCategory));
		}
	}

	/* Positive path: get and validate categories with oversized max_categories and oversized buffer */
	written_count = -1;
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("\nPositive path: get and validate categories with oversized max_categories and oversized buffer\n");
		rc = OMRTEST_PRINT_ERROR(
				 ti->GetMemoryCategories(vmThread, (total_categories + 1), memoryCategories, &written_count,
										 &total_categories));
		if (OMR_ERROR_NONE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		} else if (total_categories != written_count) {
			omrtty_err_printf("%s:%d:Expected written_count to be equal to total_cateogries.");
			testRc = OMR_ERROR_INTERNAL;
		} else if (initial_total_categories != total_categories) {
			omrtty_err_printf("%s:%d:Expected total_categories to remain constant.");
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("   written_count=%d, total_categories=%d\n", written_count, total_categories);
			if (!validateCategories(OMRPORTLIB, memoryCategories, written_count)) {
				testRc = OMR_ERROR_INTERNAL;
			}
			printMemoryCategories(OMRPORTLIB, memoryCategories, written_count);
		}
	}

	/* Positive path: get and validate categories with oversized buffer and total_categories is null */
	written_count = -1;
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("\nPositive path: get and validate categories with oversized buffer and total_categories is null\n");
		rc = OMRTEST_PRINT_ERROR(ti->GetMemoryCategories(vmThread, total_categories, memoryCategories, &written_count, total_categories_null));
		if (OMR_ERROR_NONE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		} else if (total_categories != written_count) {
			omrtty_err_printf("%s:%d:Expected written_count to be equal to total_cateogries.");
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("   written_count=%d, total_categories=%d\n", written_count, total_categories);
			if (!validateCategories(OMRPORTLIB, memoryCategories, written_count)) {
				testRc = OMR_ERROR_INTERNAL;
			}
			printMemoryCategories(OMRPORTLIB, memoryCategories, written_count);
		}
	}

	/* Positive path: get and validate categories with oversized max_categories, oversized buffer and total_categories is null*/
	written_count = -1;
	if (OMR_ERROR_NONE == testRc) {
		omrtty_printf("\nPositive path: get and validate categories with oversized max_categories, oversized buffer and total_categories is null\n");
		rc = OMRTEST_PRINT_ERROR(ti->GetMemoryCategories(vmThread, (total_categories + 1), memoryCategories, &written_count, total_categories_null));
		if (OMR_ERROR_NONE != rc) {
			testRc = OMR_ERROR_INTERNAL;
		} else if (total_categories != written_count) {
			omrtty_err_printf("%s:%d:Expected written_count to be equal to total_cateogries.");
			testRc = OMR_ERROR_INTERNAL;
		} else {
			omrtty_printf("   written_count=%d \n", written_count);
			if (!validateCategories(OMRPORTLIB, memoryCategories, written_count)) {
				testRc = OMR_ERROR_INTERNAL;
			}
			printMemoryCategories(OMRPORTLIB, memoryCategories, written_count);
		}
	}

	/* Negative path: get categories with oversized max_categories, oversized buffer and a NULL written_count_ptr */
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetMemoryCategories(vmThread, (total_categories + 1), memoryCategories, NULL, &total_categories), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	/* Negative path: get categories with oversized max_categories, buffer is NULL */
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetMemoryCategories(vmThread, (total_categories + 1), NULL, &written_count, NULL), OMR_ERROR_ILLEGAL_ARGUMENT);
		if (OMR_ERROR_ILLEGAL_ARGUMENT != rc) {
			testRc = OMR_ERROR_INTERNAL;
		}
	}

	/* Allocate our buffer */
	if (OMR_ERROR_NONE == testRc) {
		memoryCategories = omrmem_allocate_memory((total_categories - 1) * sizeof(OMR_TI_MemoryCategory), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == memoryCategories) {
			testRc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		} else {
			memset(memoryCategories, 0, (total_categories - 1) * sizeof(OMR_TI_MemoryCategory));
		}
	}

	/* Negative path: get categories with undersized max_categories and oversized buffer */
	written_count = -1;
	if (OMR_ERROR_NONE == testRc) {
		rc = OMRTEST_PRINT_UNEXPECTED_RC(ti->GetMemoryCategories(vmThread, total_categories - 2, memoryCategories, &written_count, total_categories_null), OMR_ERROR_OUT_OF_NATIVE_MEMORY);
		if (OMR_ERROR_OUT_OF_NATIVE_MEMORY != rc) {
			testRc = OMR_ERROR_INTERNAL;
		} else {
			if (-1 == written_count) {
				omrtty_err_printf("%s:%d:Expected written_count to be assigned, even on negative path.");
				testRc = OMR_ERROR_INTERNAL;
			} else if ((total_categories - 2) != written_count) {
				omrtty_err_printf("%s:%d:Expected written_count to be equal to total_cateogries -2.");
				testRc = OMR_ERROR_INTERNAL;
			} else if (!validateCategories(OMRPORTLIB, memoryCategories, written_count)) {
				testRc = OMR_ERROR_INTERNAL;
			}
		}
	}

	if (NULL != memoryCategories) {
		omrmem_free_memory(memoryCategories);
	}

	return testRc;
}

static void
printMemoryCategoryLine(OMRPortLibrary *portLib, OMR_TI_MemoryCategory *category, int depth)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	int i;

	for (i = 0; i < depth; i++) {
		omrtty_printf("   ");
	}
	omrtty_printf("%s %ld bytes / %ld allocations", category->name, category->liveBytesDeep,
				  category->liveAllocationsDeep);
	if (category->liveBytesShallow != category->liveBytesDeep) {
		omrtty_printf(" (Shallow: %ld bytes / %ld allocations)", category->liveBytesShallow,
					  category->liveAllocationsShallow);
	}
	omrtty_printf("\n");
}

static void
printMemoryCategory(OMRPortLibrary *portLib, OMR_TI_MemoryCategory *category, int depth)
{
	OMR_TI_MemoryCategory *next = NULL;
	if (category->liveBytesDeep == 0) {
		return; /* Don't print empty categories. */
	}
	printMemoryCategoryLine(portLib, category, depth);
	next = category->firstChild;
	while (NULL != next) {
		printMemoryCategory(portLib, next, depth + 1);
		next = next->nextSibling;
	}
}

static void
printMemoryCategories(OMRPortLibrary *portLib, OMR_TI_MemoryCategory *categories_buffer, int32_t written_count)
{
	int32_t categories_index = 0;

	for (categories_index = 0; categories_index < written_count; categories_index++) {
		/* For each root category (category has no parent)
		 * write out and indent the children. */
		if (NULL == categories_buffer[categories_index].parent) {
			printMemoryCategory(portLib, &categories_buffer[categories_index], 0);
		}
	}
}

/**
 * Validates that a category pointer (parent, firstChild, nextSibling) is valid
 */
static BOOLEAN
validateCategoryPointer(OMRPortLibrary *portLibrary, OMR_TI_MemoryCategory *value,
						OMR_TI_MemoryCategory *categories_buffer, int32_t written_count)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (value < categories_buffer || value > (categories_buffer + written_count)) {
		omrtty_err_printf(
			"%s:%d:Category pointer out of range. Value = %p, categories_buffer = %p, top of buffer = %p\n",
			__FILE__, __LINE__, value, categories_buffer, categories_buffer + written_count);
		return FALSE;
	}

	if (((char *)value - (char *)categories_buffer) % sizeof(OMR_TI_MemoryCategory)) {
		omrtty_err_printf(
			"%s:%d:Misaligned category pointer = %p. Categories_buffer=%p, sizeof(OMR_TI_MemoryCategory)=%u, remainder=%d\n",
			__FILE__, __LINE__, value, categories_buffer, (unsigned int)sizeof(OMR_TI_MemoryCategory),
			(int)((value - categories_buffer) % sizeof(OMR_TI_MemoryCategory)));
		return FALSE;
	}

	return TRUE;
}

/* Checks the data returned from TI extension.
 *
 * Looks for loops in the linked lists, checks for valid values etc.
 */
static BOOLEAN
validateCategories(OMRPortLibrary *portLibrary, OMR_TI_MemoryCategory *categories_buffer, int32_t written_count)
{
	int32_t i;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	for (i = 0; i < written_count; i++) {
		OMR_TI_MemoryCategory *thisCategory = categories_buffer + i;
		size_t nameLength;

		/* Name should have more than 0 characters (will also detect bad pointers ) */
		nameLength = strlen(thisCategory->name);

		if (0 == nameLength) {
			omrtty_err_printf("%s:%d:zero-length name in returned array. Index = %d\n", __FILE__, __LINE__, i);
			return FALSE;
		}

		/* Shallow counters should be >= 0 */
		if (thisCategory->liveBytesShallow < 0) {
			omrtty_err_printf("%s:%d:liveBytesShallow negative for category %s. Index = %d\n", __FILE__,
							  __LINE__, thisCategory->name, i);
			return FALSE;
		}

		if (thisCategory->liveAllocationsShallow < 0) {
			omrtty_err_printf("%s:%d:liveAllocationsShallow negative for category %s. Index = %d\n", __FILE__,
							  __LINE__, thisCategory->name, i);
			return FALSE;
		}

		/* Deep counters should be >= shallow counters */
		if (thisCategory->liveBytesShallow > thisCategory->liveBytesDeep) {
			omrtty_err_printf(
				"%s:%d:liveBytesShallow is larger than liveBytesDeep for category %s. Index = %d\n", __FILE__,
				__LINE__, thisCategory->name, i);
			return FALSE;
		}

		if (thisCategory->liveAllocationsShallow > thisCategory->liveAllocationsDeep) {
			omrtty_err_printf(
				"%s:%d:liveAllocationsShallow is larger than liveAllocationsDeep for category %s. Index = %d\n",
				__FILE__, __LINE__, thisCategory->name, i);
			return FALSE;
		}

		/* Check parent link */
		if (thisCategory->parent != NULL) {

			if (!validateCategoryPointer(portLibrary, thisCategory->parent, categories_buffer, written_count)) {
				omrtty_err_printf("%s:%d:Parent link for category %s  failed validation. Index = %d\n",
								  __FILE__, __LINE__, thisCategory->name, i);
				return FALSE;
			}

			/* Parent cannot be itself */
			if (thisCategory->parent == thisCategory) {
				omrtty_err_printf("%s:%d:Category %s is its own parent. Index = %d\n", __FILE__, __LINE__,
								  thisCategory->name, i);
				return FALSE;
			}
		}

		/* Validate child link */
		if (thisCategory->firstChild != NULL) {
			if (!validateCategoryPointer(portLibrary, thisCategory->firstChild, categories_buffer, written_count)) {
				omrtty_err_printf("%s:%d:firstChild link for category %s  failed validation. Index = %d\n",
								  __FILE__, __LINE__, thisCategory->name, i);
				return FALSE;
			}

			/* firstChild cannot be itself */
			if (thisCategory->firstChild == thisCategory) {
				omrtty_err_printf("%s:%d:Category %s is its own firstChild. Index = %d\n", __FILE__, __LINE__,
								  thisCategory->name, i);
				return FALSE;
			}
		}

		/* Validate sibling list. Each node must validate and we mustn't have loops */
		if (thisCategory->nextSibling != NULL) {
			OMR_TI_MemoryCategory *ptr1, *ptr2;

			if (!validateCategoryPointer(portLibrary, thisCategory->nextSibling, categories_buffer, written_count)) {
				omrtty_err_printf("%s:%d:nextSibling link for category %s  failed validation. Index = %d\n",
								  __FILE__, __LINE__, thisCategory->name, i);
				return FALSE;
			}

			if (thisCategory->nextSibling == thisCategory) {
				omrtty_err_printf("%s:%d:Category %s is its own nextSibling. Index = %d\n", __FILE__, __LINE__,
								  thisCategory->name, i);
				return FALSE;
			}

			/* Loop detection. */
			ptr1 = thisCategory;
			ptr2 = thisCategory->nextSibling;

			if (ptr2 && ptr2->nextSibling == ptr1) {
				omrtty_err_printf("%s:%d:1-2-1 loop found in nextSibling chain for category %s. Index = %d\n",
								  __FILE__, __LINE__, thisCategory->name, i);
				return FALSE;
			} else {
				while (ptr1 && ptr2) {
					/* ptr1 increments by 1 step, ptr2 increments by 2. If they are ever equal, we have a loop */
					/* See Van Der Linden, "Deep C Secrets" Appendix for details */
					ptr1 = ptr1->nextSibling;
					ptr2 = ptr2->nextSibling != NULL ? ptr2->nextSibling->nextSibling : NULL;

					if (ptr1 == ptr2) {
						omrtty_err_printf("%s:%d:loop found in nextSibling chain for category %s. Index = %d\n",
										  __FILE__, __LINE__, thisCategory->name, i);
						return FALSE;
					}
				}
			}
		}

	}

	return TRUE;
}

