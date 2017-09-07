/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include <string.h>

#include "omrTestHelpers.h"
#include "omrrasinit.h"
#include "thread_api.h"

extern "C" omr_error_t
omrTestVMInit(OMRTestVM *const testVM, OMRPortLibrary *portLibrary)
{
	omr_error_t rc = OMR_ERROR_NONE;
	const char *nlsSearchPaths[] = { "./" };
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	/* HACK: Look for NLS catalog in the current directory */
	omrnls_set_catalog(nlsSearchPaths, 1, "java", "properties");

	testVM->portLibrary = portLibrary;
	memset(&testVM->omrRuntime, 0, sizeof(testVM->omrRuntime));
	memset(&testVM->omrVM, 0, sizeof(testVM->omrVM));

	testVM->omrRuntime._configuration._maximum_vm_count = 1;
	testVM->omrRuntime._vmCount = 0;
	testVM->omrRuntime._portLibrary = portLibrary;
	if (OMR_ERROR_NONE != OMRTEST_PRINT_ERROR(omr_initialize_runtime(&testVM->omrRuntime))) {
		rc = OMR_ERROR_INTERNAL;
	}

	if (OMR_ERROR_NONE == rc) {
		testVM->omrVM._runtime = &testVM->omrRuntime;
		testVM->omrVM._language_vm = NULL;
		if (OMR_ERROR_NONE != OMRTEST_PRINT_ERROR(omr_attach_vm_to_runtime(&testVM->omrVM))) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		if (OMR_ERROR_NONE != OMRTEST_PRINT_ERROR(omr_ras_initMemCategories(OMRPORTLIB))) {
			rc = OMR_ERROR_INTERNAL;
		}
	}
	if (OMR_ERROR_NONE == rc) {
		if (OMR_ERROR_NONE != OMRTEST_PRINT_ERROR(omr_ras_initTI(&testVM->omrVM))) {
			rc = OMR_ERROR_INTERNAL;
		}
	}
	return rc;
}

extern "C" omr_error_t
omrTestVMFini(OMRTestVM *const testVM)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	if (OMR_ERROR_NONE == rc) {
		if (OMR_ERROR_NONE != OMRTEST_PRINT_ERROR(omr_ras_cleanupTI(&testVM->omrVM))) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		omrthread_t self = NULL;

		if (0 != omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
			rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
		} else {
			omrthread_monitor_enter(testVM->omrVM._vmThreadListMutex);
			while ((testVM->omrVM._languageThreadCount + testVM->omrVM._internalThreadCount) > 0) {
				omrthread_monitor_wait(testVM->omrVM._vmThreadListMutex);
			}
			omrthread_monitor_exit(testVM->omrVM._vmThreadListMutex);
			omrthread_detach(self);
		}
	}

	if (OMR_ERROR_NONE == rc) {
		/*
		 * Memory categories can be reset to 0 only for testing purposes.
		 * The portlib returns an error in this case, but we can ignore it.
		 * During normal VM operation, it is not permitted to change the memory category
		 * definitions after they have been initialized.
		 */
		omrport_control(OMRPORT_CTLDATA_MEM_CATEGORIES_SET, 0);
	}

	if (OMR_ERROR_NONE == rc) {
		if (OMR_ERROR_NONE != OMRTEST_PRINT_ERROR(omr_detach_vm_from_runtime(&testVM->omrVM))) {
			rc = OMR_ERROR_INTERNAL;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		if (OMR_ERROR_NONE != OMRTEST_PRINT_ERROR(omr_destroy_runtime(&testVM->omrRuntime))) {
			rc = OMR_ERROR_INTERNAL;
		}
	}
	return rc;
}
