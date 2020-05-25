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

#include <string.h>
#include "omrport.h"
#include "omrportpriv.h"
#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
#include <leawi.h>
#include "omrsignal_ceehdlr.h"
#endif


/* internal portlib functions */
uintptr_t
syslogOpen(struct OMRPortLibrary *portLibrary, uintptr_t flags);
uintptr_t
syslogClose(struct OMRPortLibrary *portLibrary);

#if defined(OMR_RAS_TDF_TRACE)
#define _UTE_STATIC_
#include "ut_omrport.h"
#endif /* OMR_RAS_TDF_TRACE */

#if defined(RS6000)
extern void __clearTickTock(void);
#endif


int32_t
omrport_control(struct OMRPortLibrary *portLibrary, const char *key, uintptr_t value)
{
	/* return value of 0 is success */
	if (!strcmp(OMRPORT_CTLDATA_SIG_FLAGS, key)) {
		portLibrary->portGlobals->control.sig_flags = value;
		return 0;
	}
#if defined(OMR_ENV_DATA64)
	if (!strcmp(OMRPORT_CTLDATA_ALLOCATE32_COMMIT_SIZE, key)) {
		if (0 != value) {
			/* CommitSize is immutable. It can only be set once. */
			if (0 == PPG_mem_mem32_subAllocHeapMem32.suballocator_commitSize) {
				/* Round up the commit size to the page size and set it to global variable */
				uintptr_t pageSize = portLibrary->vmem_supported_page_sizes(portLibrary)[0];
				uintptr_t roundedCommitSize = pageSize * (value / pageSize);
				if (roundedCommitSize < value) {
					roundedCommitSize += pageSize;
				}
				PPG_mem_mem32_subAllocHeapMem32.suballocator_commitSize = roundedCommitSize;
			} else {
				return 1;
			}
		} else {
			return (int32_t)PPG_mem_mem32_subAllocHeapMem32.suballocator_commitSize;
		}
		return 0;
	}
#endif

#if defined(OMR_RAS_TDF_TRACE)
	if (!strcmp(OMRPORT_CTLDATA_TRACE_START, key) && value) {
		UtInterface *utIntf = (UtInterface *) value;
		utIntf->module->TraceInit(NULL, &UT_MODULE_INFO);
		Trc_PRT_PortInitStages_Event1();
		return 0;
	}
	if (!strcmp(OMRPORT_CTLDATA_TRACE_STOP, key) && value) {
		UtInterface *utIntf = (UtInterface *) value;
		utIntf->module->TraceTerm(NULL, &UT_MODULE_INFO);
		return 0;
	}
#endif

	if (!strcmp(OMRPORT_CTLDATA_SYSLOG_OPEN, key)) {
		if (TRUE == syslogOpen(portLibrary, value)) {
			PPG_syslog_flags = value;
			return 1;
		}
		return 0;
	}

	if (!strcmp(OMRPORT_CTLDATA_SYSLOG_CLOSE, key)) {
		if (TRUE == syslogClose(portLibrary)) {
			return 1;
		}
		return 0;
	}

#if defined(OMR_OS_WINDOWS) && !defined(OMR_ENV_DATA64)
	if (!strcmp("SIG_INTERNAL_HANDLER", key)) {
		/* used by optimized code to implement fast signal handling on Windows */
		extern int structuredExceptionHandler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, EXCEPTION_POINTERS *exceptionInfo);
		*(int (**)(struct OMRPortLibrary *, omrsig_handler_fn, void *, uint32_t, EXCEPTION_POINTERS *))value = structuredExceptionHandler;
		return 0;
	}
#endif /* defined(OMR_OS_WINDOWS) && !defined(OMR_ENV_DATA64) */

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
	if (!strcmp("SIG_INTERNAL_HANDLER", key)) {
		if (portLibrary->sig_protect == omrsig_protect_ceehdlr) {
			/* Only expose the internal condition handler if the port library's
			 * implementation for CEEHDLR hasn't been overridden.
			 * We can't use j9port_isFunctionOverridden to check for this because
			 * the port library overrides sig_protect itself (with omrsig_protect_ceehdlr)
			 * when the option OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR is passed into omrsig_set_options */
			extern void omrsig_le_condition_handler(_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc);

			*(void (**)(_FEEDBACK *fc, _INT4 *token, _INT4 *leResult, _FEEDBACK *newfc))value = omrsig_le_condition_handler;
			return 0;
		} else {
			return 1;
		}

	}
#endif


	/* return 1 if numa is available on the platform, otherwise, return 0 */
	if (!strcmp(OMRPORT_CTLDATA_VMEM_NUMA_IN_USE, key)) {
#if defined(PPG_numa_platform_supports_numa)
		if (1 == PPG_numa_platform_supports_numa) {
			/* NUMA is available */
			return 1;
		} else {
			/* NUMA is not supported on this platform */
			return 0;
		}
#else /* PPG_numa_platform_supports_numa */
		return 0;
#endif /* PPG_numa_platform_supports_numa) */
	}


	/* enable or disable NUMA memory interleave */
	if (0 == strcmp(OMRPORT_CTLDATA_VMEM_NUMA_INTERLEAVE_MEM, key)) {
#if defined(PPG_numa_platform_interleave_memory)
		Assert_PRT_true((0 == value) || (1 == value));
		PPG_numa_platform_interleave_memory = value;
#endif /* PPG_numa_platform_interleave_memory */
		return 0;
	}

	if (0 == strcmp(OMRPORT_CTLDATA_VMEM_NUMA_ENABLE, key)) {
#if defined(PPG_numa_platform_supports_numa)
		Assert_PRT_true((0 == value) || (1 == value));
		PPG_numa_platform_supports_numa = value;
#endif /* PPG_numa_platform_supports_numa) */
		return 0;
	}

	if (!strcmp(OMRPORT_CTLDATA_TIME_CLEAR_TICK_TOCK, key)) {
#if defined(RS6000)
		__clearTickTock();
#endif
		return 0;
	}

	if (strcmp(OMRPORT_CTLDATA_NOIPT, key) == 0) {
#if defined(J9VM_PROVIDE_ICONV)
		int rc = 0;

		rc = iconv_global_init(portLibrary);
		if (rc == 0) {/* iconv_global_init done */
			PPG_global_converter_enabled = 1;
		}
		return rc;
#endif
		return 0;
	}

	if (!strcmp(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, key)) {
		J9PortControlData *portControl = &portLibrary->portGlobals->control;
		OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
		/* Allow categories to be reset to NULL (for testing purposes) - but not reset to anything else */
		if (0 == value) {
			omrmem_shutdown_categories(portLibrary);
		} else if (NULL == portControl->language_memory_categories.categories) {
			uint32_t i = 0;
			OMRMemCategorySet *categories = (OMRMemCategorySet *)value;

			/* The port library knows the port library will include it's own allocations so we
			 * will need an array at least large enough for that. */
			uint32_t omrCategoryCount = OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(OMRMEM_CATEGORY_PORT_LIBRARY);
			uint32_t languageCategoryCount = 0;
#if defined(OMR_ENV_DATA64)
			omrCategoryCount = OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(
								   OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS);
#endif
			/* Now sort the categories into two sets < OMRMEM_LANGUAGE_CATEGORY_LIMIT and > OMRMEM_LANGUAGE_CATEGORY_LIMIT */
			/* Find out how big the category arrays need to be. */
			for (i = 0; i < categories->numberOfCategories; i++) {
				uint32_t categoryCode = categories->categories[i]->categoryCode;
				if (categoryCode < OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
					languageCategoryCount =
						(categoryCode > languageCategoryCount) ? categoryCode : languageCategoryCount;
				} else if (categoryCode > OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
					uint32_t categoryIndex = OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(categoryCode);
					omrCategoryCount = (categoryIndex > omrCategoryCount) ? categoryIndex : omrCategoryCount;
				}
			}
			/* The indices start at 0, add 1 to get the count. */
			languageCategoryCount++;
			omrCategoryCount++;
			portControl->language_memory_categories.numberOfCategories = 0;
			/* We are calling the real omrmem_allocate_memory, not the macro. */
			portControl->language_memory_categories.categories = portLibrary->mem_allocate_memory(OMRPORTLIB,
					(languageCategoryCount) * sizeof(OMRMemCategory *), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == portControl->language_memory_categories.categories) {
				return 1;
			}
			portControl->omr_memory_categories.numberOfCategories = 0;
			portControl->omr_memory_categories.categories = portLibrary->mem_allocate_memory(OMRPORTLIB,
					(omrCategoryCount) * sizeof(OMRMemCategory *), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == portControl->omr_memory_categories.categories) {
				portLibrary->mem_free_memory(OMRPORTLIB, portControl->language_memory_categories.categories);
				portControl->language_memory_categories.categories = NULL;
				return 1;
			}
			memset(portControl->language_memory_categories.categories, 0,
				   (languageCategoryCount) * sizeof(OMRMemCategory *));
			memset(portControl->omr_memory_categories.categories, 0, (omrCategoryCount) * sizeof(OMRMemCategory *));
			/* Copy the categories. */
			for (i = 0; i < categories->numberOfCategories; i++) {
				uint32_t categoryCode = categories->categories[i]->categoryCode;
				if (categoryCode < OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
					portControl->language_memory_categories.categories[categoryCode] = categories->categories[i];
				}
				if (categoryCode > OMRMEM_LANGUAGE_CATEGORY_LIMIT) {
					uint32_t categoryIndex = OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(categoryCode);
					portControl->omr_memory_categories.categories[categoryIndex] = categories->categories[i];
				}
			}
			/* These two won't have been passed in by the caller of port control. */
			portControl->omr_memory_categories.categories[OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(
						OMRMEM_CATEGORY_PORT_LIBRARY)] = &(portLibrary->portGlobals->portLibraryMemoryCategory);
#if defined(OMR_ENV_DATA64)
			portControl->omr_memory_categories.categories[OMRMEM_OMR_CATEGORY_INDEX_FROM_CODE(
						OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS)] =
							&portLibrary->portGlobals->unusedAllocate32HeapRegionsMemoryCategory;
#endif
			portControl->language_memory_categories.numberOfCategories = languageCategoryCount;
			portControl->omr_memory_categories.numberOfCategories = omrCategoryCount;
			return 0;
		} else {
			Trc_Assert_PRT_mem_categories_already_set(NULL != portControl->language_memory_categories.categories);
			return 1;
		}
	}

#if defined(AIXPPC)
	/* OMRPORT_CTLDATA_AIX_PROC_ATTR key is used only on AIX systems */
	if (0 == strcmp(OMRPORT_CTLDATA_AIX_PROC_ATTR, key)) {
		return (int32_t)portLibrary->portGlobals->control.aix_proc_attr;
	}
#endif

#if defined(J9ZOS390) && defined(OMR_GC_COMPRESSED_POINTERS)
	if (0 == strcmp(OMRPORT_CTLDATA_NOSUBALLOC32BITMEM, key)) {
		portLibrary->portGlobals->disableEnsureCap32 = value;
		return 0;
	}
#endif

	if (0 == strcmp(OMRPORT_CTLDATA_VMEM_ADVISE_OS_ONFREE, key)) {
		portLibrary->portGlobals->vmemAdviseOSonFree = value;
		return 0;
	}

	if (0 == strcmp(OMRPORT_CTLDATA_VMEM_HUGE_PAGES_MMAP_ENABLED, key)) {
#if defined(LINUX)
		Assert_PRT_true((0 == value) || (1 == value));
		PPG_huge_pages_mmap_enabled = value;
#endif /* defined(LINUX) */
		return 0;
	}

	if (0 == strcmp(OMRPORT_CTLDATA_VECTOR_REGS_SUPPORT_ON, key)) {
		portLibrary->portGlobals->vectorRegsSupportOn = value;
		return 0;
	}

	if (0 == strcmp(OMRPORT_CTLDATA_NLS_DISABLE, key)) {
		portLibrary->portGlobals->nls_data.isDisabled = value;
		return 0;
	}

	if (0 == strcmp(OMRPORT_CTLDATA_VMEM_ADVISE_HUGEPAGE, key)) {
#if defined(LINUX)
		/* set value to advise OS about vmem to consider for Transparent HugePage (Only for Linux) */
		portLibrary->portGlobals->vmemEnableMadvise &= value;
#endif
		return 0;
	}

	/* work around for case if smart address feature still be not reliable enough */
	if (0 == strcmp(OMRPORT_CTLDATA_VMEM_PERFORM_FULL_MEMORY_SEARCH, key)) {
#if defined(PPG_performFullMemorySearch)
		Assert_PRT_true((0 == value) || (1 == value));
		PPG_performFullMemorySearch = value;
#endif /* PPG_performFullMemorySearch */
		return 0;
	}

	return 1;
}


