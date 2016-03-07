/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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

