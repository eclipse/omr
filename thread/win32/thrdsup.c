/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/*
 * @file
 * @ingroup Thread
 */


#include <windows.h>
#include <stdlib.h>
#include "omrcfg.h"
#include "omrcomp.h"
#include "omrmutex.h"
#include "thread_internal.h"
#include "thrtypes.h"
#include "thrdsup.h"

const int priority_map[] = J9_PRIORITY_MAP;

J9ThreadLibrary default_library;

extern void omrthread_init(J9ThreadLibrary *lib);
extern void omrthread_shutdown(void);

/**
 * Perform OS-specific initializations for the threading library.
 *
 * @return 0 on success or non-zero value on failure.
 */
intptr_t
init_thread_library(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	if (lib->initStatus == 0) {
		HANDLE mutex = CreateMutex(NULL, TRUE, "omrthread_init_mutex");
		if (mutex == NULL) {
			return -1;
		}
		if (lib->initStatus == 0) {
			omrthread_init(lib);
			if (lib->initStatus == 1) {
				atexit(omrthread_shutdown);
			}
		}
		ReleaseMutex(mutex);
		CloseHandle(mutex);
	}
	return lib->initStatus != 1;
}

/**
 * Initialize a thread's priority.
 *
 * Here the threading library priority value is converted to the appropriate
 * OS-specific value.
 *
 * @param[in] thread a thread
 * @return none
 *
 */
void
initialize_thread_priority(omrthread_t thread)
{
	int priority;

	thread->priority = J9THREAD_PRIORITY_NORMAL;

	if (priority_map[J9THREAD_PRIORITY_MIN] == priority_map[J9THREAD_PRIORITY_MAX]) {
		return;
	}

	priority = GetThreadPriority(thread->handle);

	thread->priority = omrthread_map_native_priority(priority);
}

intptr_t
osthread_join(omrthread_t self, omrthread_t threadToJoin)
{
	intptr_t j9thrRc = J9THREAD_SUCCESS;
	DWORD rc = WaitForSingleObject(threadToJoin->handle, INFINITE);

	if (WAIT_OBJECT_0 != rc) {
		if (WAIT_ABANDONED == rc) {
			j9thrRc = J9THREAD_ERR;
		} else if (WAIT_TIMEOUT == rc) {
			j9thrRc = J9THREAD_TIMED_OUT;
		} else if (WAIT_FAILED == rc) {
			self->os_errno = GetLastError();
			j9thrRc = J9THREAD_ERR | J9THREAD_ERR_OS_ERRNO_SET;
		} else {
			/* Shouldn't happen. There are no other documented return codes. */
			j9thrRc = J9THREAD_ERR;
		}
	}
	return j9thrRc;
}
