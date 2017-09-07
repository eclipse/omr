/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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

#if !defined(TESTPROCESSHELPER_HPP_INCLUDED)
#define TESTPROCESSHELPER_HPP_INCLUDED
#include "omrport.h"

typedef struct OMRProcessHandleStruct {
	intptr_t procHandle;
	intptr_t inHandle;
	intptr_t outHandle;
	intptr_t errHandle;
	int32_t pid;
	intptr_t exitCode;
} OMRProcessHandleStruct;

typedef struct OMRProcessHandleStruct *OMRProcessHandle;

intptr_t openLaunchSemaphore(OMRPortLibrary *portLibrary, const char *name, uintptr_t nProcess);
intptr_t SetLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore, uintptr_t nProcess);
intptr_t ReleaseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore, uintptr_t nProcess);
intptr_t WaitForLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore);
intptr_t CloseLaunchSemaphore(OMRPortLibrary *portLibrary, intptr_t semaphore);

OMRProcessHandle launchChildProcess(OMRPortLibrary *portLibrary, const char *testname, const char *argv0, const char *options);
intptr_t waitForTestProcess(OMRPortLibrary *portLibrary, OMRProcessHandle processHandle);
void SleepFor(intptr_t second);

#define OMRPROCESS_INVALID_FD	-1
#define OMRPROCESS_DEBUG 1
#define OMRPROCESS_ERROR -1
intptr_t j9process_close(OMRPortLibrary *portLibrary, OMRProcessHandle *processHandle, uint32_t options);
intptr_t j9process_waitfor(OMRPortLibrary *portLibrary, OMRProcessHandle processHandle);
intptr_t j9process_get_exitCode(OMRPortLibrary *portLibrary, OMRProcessHandle processHandle);
intptr_t j9process_create(OMRPortLibrary *portLibrary, const char *command[], uintptr_t commandLength, const char *dir, uint32_t options, OMRProcessHandle *processHandle);

#endif /* !defined(TESTPROCESSHELPER_HPP_INCLUDED) */
