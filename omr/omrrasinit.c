/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include "omrrasinit.h"

#include <string.h>

#include "omrport.h"
#include "omragent_internal.h"
#include "omrprofiler.h"

#define OMR_MEM_CATEGORY_RUNTIME 0

/*
 * Categories OMRMEM_CATEGORY_UNKNOWN, OMRMEM_CATEGORY_PORT_LIBRARY, and
 * OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS are defined inside the
 * port library.
 *
 * Categories OMRMEM_CATEGORY_THREADS, OMRMEM_CATEGORY_THREADS_NATIVE_STACK are
 * defined inside the thread library.
 */
OMRMEM_CATEGORY_3_CHILDREN("RUNTIME", OMR_MEM_CATEGORY_RUNTIME, OMRMEM_CATEGORY_VM, OMRMEM_CATEGORY_JIT, OMRMEM_CATEGORY_UNKNOWN);
#if defined(OMR_OPT_CUDA)
OMRMEM_CATEGORY_6_CHILDREN("VM", OMRMEM_CATEGORY_VM, OMRMEM_CATEGORY_MM, OMRMEM_CATEGORY_THREADS, OMRMEM_CATEGORY_CUDA, OMRMEM_CATEGORY_PORT_LIBRARY, OMRMEM_CATEGORY_TRACE, OMRMEM_CATEGORY_OMRTI);
#else
OMRMEM_CATEGORY_5_CHILDREN("VM", OMRMEM_CATEGORY_VM, OMRMEM_CATEGORY_MM, OMRMEM_CATEGORY_THREADS, OMRMEM_CATEGORY_PORT_LIBRARY, OMRMEM_CATEGORY_TRACE, OMRMEM_CATEGORY_OMRTI);
#endif /* OMR_OPT_CUDA */
OMRMEM_CATEGORY_1_CHILD("Memory Manager (GC)", OMRMEM_CATEGORY_MM, OMRMEM_CATEGORY_MM_RUNTIME_HEAP);
OMRMEM_CATEGORY_NO_CHILDREN("Object Heap", OMRMEM_CATEGORY_MM_RUNTIME_HEAP);
OMRMEM_CATEGORY_NO_CHILDREN("Trace", OMRMEM_CATEGORY_TRACE);
OMRMEM_CATEGORY_NO_CHILDREN("OMRTI", OMRMEM_CATEGORY_OMRTI);
OMRMEM_CATEGORY_NO_CHILDREN("VM Stack", OMRMEM_CATEGORY_THREADS_RUNTIME_STACK);
OMRMEM_CATEGORY_2_CHILDREN("JIT", OMRMEM_CATEGORY_JIT, OMRMEM_CATEGORY_JIT_CODE_CACHE, OMRMEM_CATEGORY_JIT_DATA_CACHE);
OMRMEM_CATEGORY_NO_CHILDREN("JIT Code Cache", OMRMEM_CATEGORY_JIT_CODE_CACHE);
OMRMEM_CATEGORY_NO_CHILDREN("JIT Data Cache", OMRMEM_CATEGORY_JIT_DATA_CACHE);
#if defined(OMR_OPT_CUDA)
OMRMEM_CATEGORY_NO_CHILDREN("CUDA", OMRMEM_CATEGORY_CUDA);
#endif /* OMR_OPT_CUDA */


omr_error_t
omr_ras_initMemCategories(OMRPortLibrary *portLibrary)
{
	/*
	 * Every category used by the application, except UNKNOWN, must have an entry in the table.
	 * The port library will supplement this set with internal port library categories.
	 * Space must be reserved for the thread library to insert its four categories.
	 *
	 * This table should not be a global variable because it is modified by omrthread_lib_control(),
	 * and the modifications should not be visible across multiple calls to this function.
	 */
	OMRMemCategory *memCategories[] = {
		CATEGORY_TABLE_ENTRY(OMR_MEM_CATEGORY_RUNTIME),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_VM),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_MM),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_MM_RUNTIME_HEAP),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_TRACE),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_OMRTI),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_THREADS_RUNTIME_STACK),
		NULL, /* OMRMEM_CATEGORY_THREADS populated by thread library */
		NULL, /* OMRMEM_CATEGORY_THREADS_NATIVE_STACK populated by thread library */
#if defined(OMR_THR_FORK_SUPPORT)
		NULL, /* OMRMEM_CATEGORY_OSMUTEXES populated by thread library */
		NULL, /* OMRMEM_CATEGORY_OSCONDVARS populated by thread library */
#endif /* defined(OMR_THR_FORK_SUPPORT) */
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_JIT),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_JIT_CODE_CACHE),
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_JIT_DATA_CACHE),
#if defined(OMR_OPT_CUDA)
		CATEGORY_TABLE_ENTRY(OMRMEM_CATEGORY_CUDA),
#endif /* OMR_OPT_CUDA */
	};
	OMRMemCategorySet memCategorySet = { sizeof(memCategories) / sizeof(memCategories[0]), memCategories };
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (OMR_ERROR_NONE == rc) {
		if (0 != omrthread_lib_control(J9THREAD_LIB_CONTROL_GET_MEM_CATEGORIES, (uintptr_t)&memCategorySet)) {
			rc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == rc) {
		if (0 != omrport_control(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, (uintptr_t)&memCategorySet)) {
			rc = OMR_ERROR_INTERNAL;
		}
	}
	return rc;
}

omr_error_t
omr_ras_initHealthCenter(OMR_VM *omrVM, OMR_Agent **hc, const char *healthCenterOpt)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);

	if ((NULL != healthCenterOpt) && ('\0' != *healthCenterOpt)) {
		OMR_Agent *hcAgent = NULL;

		rc = omr_ras_initTI(omrVM);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("failed to init TI, rc=%d\n", rc);
			goto error_noCleanup;
		}

		hcAgent = omr_agent_create(omrVM, healthCenterOpt);
		if (NULL == hcAgent) {
			omrtty_printf("omr_agent_create error (Agent options: %s)\n", healthCenterOpt);
			rc = OMR_ERROR_INTERNAL;
			goto error_cleanupTI;
		}
		rc = omr_agent_openLibrary(hcAgent);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("omr_agent_openLibrary error (Agent options: %s, rc=%d)\n", healthCenterOpt, rc);
			goto error_cleanupTI;
		}
		rc = omr_agent_callOnLoad(hcAgent);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("omr_agent_callOnLoad error (Agent options: %s, rc=%d)\n", healthCenterOpt, rc);
			goto error_cleanupTI;
		}

		*hc = hcAgent;
		goto success;
	} else {
		*hc = NULL;
		goto success;
	}
error_cleanupTI:
	omr_ras_cleanupTI(omrVM);
error_noCleanup:
success:
	return rc;
}

omr_error_t
omr_ras_cleanupHealthCenter(OMR_VM *omrVM, OMR_Agent **hc)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if ((NULL != hc) && (NULL != *hc)) {
		rc = omr_agent_callOnUnload(*hc);
		if (OMR_ERROR_NONE == rc) {
			omr_agent_destroy(*hc);
			omr_ras_cleanupTI(omrVM);
			*hc = NULL;
		}
	}
	return rc;
}

omr_error_t
omr_ras_initTI(OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	/* Initialise this mutex if it hasn't already been initialised. */
	if (NULL == vm->_omrTIAccessMutex) {
		if (0 != omrthread_monitor_init_with_name(&vm->_omrTIAccessMutex, 0, "OMRTI access mutex")) {
			rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		}
	}
	if (OMR_ERROR_NONE == rc) {
		if (NULL == vm->sysInfo) {
			vm->sysInfo = (OMR_SysInfo *)omrmem_allocate_memory(sizeof(OMR_SysInfo), OMRMEM_CATEGORY_OMRTI);
			if (NULL != vm->sysInfo) {
				if ((0 != omrthread_monitor_init_with_name(&vm->sysInfo->syncProcessCpuLoad, 0, "syncProcessCpuLoad"))
					|| (0 != omrthread_monitor_init_with_name(&vm->sysInfo->syncSystemCpuLoad, 0, "syncSystemCpuLoad"))
				) {
					omrmem_free_memory(vm->sysInfo);
					vm->sysInfo = NULL;
					rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
				} else {
					vm->sysInfo->processCpuLoadCallStatus = NO_HISTORY;
					vm->sysInfo->processCpuTimeNegativeElapsedTimeCount = 0;
					memset(&vm->sysInfo->interimProcessCpuTime, 0, sizeof(OMRSysInfoProcessCpuTime));
					memset(&vm->sysInfo->oldestProcessCpuTime, 0, sizeof(OMRSysInfoProcessCpuTime));

					vm->sysInfo->systemCpuLoadCallStatus = NO_HISTORY;
					vm->sysInfo->systemCpuTimeNegativeElapsedTimeCount = 0;
					memset(&vm->sysInfo->interimSystemCpuTime, 0, sizeof(J9SysinfoCPUTime));
					memset(&vm->sysInfo->oldestSystemCpuTime, 0, sizeof(J9SysinfoCPUTime));
				}
			} else {
				rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			}
		}
	}
	return rc;
}

omr_error_t
omr_ras_cleanupTI(OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	if (NULL != vm->sysInfo) {
		omrthread_monitor_destroy(vm->sysInfo->syncProcessCpuLoad);
		omrthread_monitor_destroy(vm->sysInfo->syncSystemCpuLoad);
		omrmem_free_memory(vm->sysInfo);
		vm->sysInfo = NULL;
	}

	/* Free the mutex if it has been initialised. */
	if (NULL != vm->_omrTIAccessMutex) {
		if (0 != omrthread_monitor_destroy(vm->_omrTIAccessMutex)) {
			rc = OMR_ERROR_INTERNAL;
		}
	}
	return rc;
}
