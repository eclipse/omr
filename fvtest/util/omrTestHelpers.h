/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
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

#if !defined(OMRTESTHELPERS_H_INCLUDED)
#define OMRTESTHELPERS_H_INCLUDED

#include "omrport.h"
#include "omr.h"
#include "omragent.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * ================
 * OMR Test Helpers
 * ================
 */
#define OMRTEST_PRINT_ERROR(x) omrTestPrintError(#x, (x), OMRPORTLIB, __FILE__, __LINE__)
omr_error_t omrTestPrintError(const char *funcCall, const omr_error_t rc, OMRPortLibrary *portLibrary, const char *callFile, intptr_t callLine);

#define OMRTEST_PRINT_UNEXPECTED_INT_RC(x, exp) omrTestPrintUnexpectedIntRC(#x, (x), (exp), OMRPORTLIB, __FILE__, __LINE__)
omr_error_t omrTestPrintUnexpectedIntRC(const char *funcCall, const intptr_t rc, const intptr_t expectedRC,
										OMRPortLibrary *portLibrary, const char *callFile, intptr_t callLine);

#define OMRTEST_PRINT_UNEXPECTED_RC(x, exp) omrTestPrintUnexpectedRC(#x, (x), (exp), OMRPORTLIB, __FILE__, __LINE__)
omr_error_t omrTestPrintUnexpectedRC(const char *funcCall, const omr_error_t rc, const omr_error_t expectedRC,
									 OMRPortLibrary *portLibrary, const char *callFile, intptr_t callLine);

#define OMRTEST_ASSERT_ERROR_NONE(x) \
	do { \
		omr_error_t rc = (x); \
		ASSERT_EQ(OMR_ERROR_NONE, rc)<<#x " failed, "<< omrErrorToString(rc); \
	} while (0)

#define OMRTEST_ASSERT_ERROR(e, x) \
	do { \
		omr_error_t rc = (x); \
		ASSERT_EQ(e, rc)<<#x " failed, "<< omrErrorToString(rc); \
	} while (0)


const char *omrErrorToString(omr_error_t rc);
BOOLEAN strStartsWith(const char *s, const char *prefix);
void outputComment(OMRPortLibrary *portLibrary, const char *format, ...);

/*
 * ===========
 * OMR Test VM
 * ===========
 */

/**
 * Data structures for a stub OMR VM.
 */
typedef struct OMRTestVM {
	OMR_Runtime omrRuntime;
	OMR_VM omrVM;
	OMRPortLibrary *portLibrary;
} OMRTestVM;

/**
 * Initialize a stub OMR VM for testing.
 */
omr_error_t omrTestVMInit(OMRTestVM *const testVM, OMRPortLibrary *portLibrary);

/**
 * Shutdown a stub OMR VM.
 */
omr_error_t omrTestVMFini(OMRTestVM *const testVM);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OMRTESTHELPERS_H_INCLUDED */
