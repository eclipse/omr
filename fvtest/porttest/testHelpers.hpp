/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#ifndef TEST_HELPERS_HPP_INCLUDED
#define TEST_HELPERS_HPP_INCLUDED

/*
 * $RCSfile: testHelpers.h,v $
 * $Revision: 1.52 $
 * $Date: 2013-03-21 14:42:10 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Port library testing
 *
 * Definitions, types and functions common to port library test suites.
 */
#include <string.h>
#include <limits.h>
#include "omrcomp.h"
#include "omrport.h"
#include "omrTestHelpers.h"
#include "omrTest.h"
#include "portTestHelpers.hpp"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Test case status
 * @{
 */
#define TEST_PASS 0
#define TEST_FAIL 1
/** @} */

#define EXIT_OUT_OF_MEMORY 37

#define BOUNDCPUS "-boundcpus:"

/**
 * Save some typing, when displaying an error message need the
 * portlibrary, file name, line number and testname used
 */
#define PORTTEST_ERROR_ARGS privateOmrPortLibrary, __FILE__, __LINE__, testName

/*
 * Forward declarations, not under doxygen control
 */
void operationNotSupported(OMRPortLibrary *portLibrary, const char *operationName);
void reportTestEntry(OMRPortLibrary *portLibrary, const char *testName);
int reportTestExit(OMRPortLibrary *portLibrary, const char *testName);
void outputErrorMessage(OMRPortLibrary *portLibrary, const char *fileName, int32_t lineNumber, const char *testName, const char *format, ...);
void HEADING(OMRPortLibrary *portLibrary, const char *string);
uintptr_t verifyFileExists(struct OMRPortLibrary *portLibrary, const char *pltestFileName, int32_t lineNumber, const char *testName, const char *fileName);
void dumpTestFailuresToConsole(struct OMRPortLibrary *portLibrary);
void deleteControlDirectory(struct OMRPortLibrary *portLibrary, char *baseDir);
void getPortLibraryMemoryCategoryData(struct OMRPortLibrary *portLibrary, uintptr_t *blocks, uintptr_t *bytes);
int startsWith(const char *s, const char *prefix);
uintptr_t raiseSEGV(OMRPortLibrary *portLibrary, void *arg);
intptr_t omrfile_create_status_file(OMRPortLibrary *portLibrary, const char *filename, const char *testName);
intptr_t omrfile_status_file_exists(OMRPortLibrary *portLibrary, const char *filename, const char *testName);
void testFileCleanUp(const char* filePrefix);

/**
 * @see omrsignalTest.c::validateGPInfo
 */
void validateGPInfo(struct OMRPortLibrary *portLibrary, uint32_t gpType, omrsig_handler_fn handler, void *gpInfo, const char *testName);

#ifdef __cplusplus
}
#endif

#endif /* TEST_HELPERS_HPP_INCLUDED */
