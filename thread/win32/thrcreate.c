/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
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

#include "omrthread.h"
#include "threaddef.h"
#include "omrthreadattr.h"

intptr_t
osthread_create(struct J9Thread *self, OSTHREAD *handle, const omrthread_attr_t attr,
				WRAPPER_FUNC entrypoint, WRAPPER_ARG entryarg)
{
	omrthread_prio_t priority;
	OSTHREAD osHandle;

	ASSERT(self);
	ASSERT(handle);
	ASSERT(attr);
	ASSERT(entrypoint);

	if (self && (J9THREAD_SCHEDPOLICY_INHERIT == attr->schedpolicy)) {
		priority = self->priority;
	} else {
		priority = attr->priority;
	}

	osHandle = (HANDLE)_beginthreadex(NULL, (unsigned)attr->stacksize, entrypoint, entryarg, 0, NULL);
	if (0 == osHandle) {
		if (self) {
			self->os_errno = errno;
		}
		return J9THREAD_ERR_THREAD_CREATE_FAILED | J9THREAD_ERR_OS_ERRNO_SET;
	}
	*handle = osHandle;

	/* This will fail if the child thread has already exited. But it doesn't matter. */
	if (0 != THREAD_SET_PRIORITY(osHandle, priority)) {
		DWORD os_errno = GetLastError();
		if (ERROR_INVALID_HANDLE != os_errno) {
			if (self) {
				self->os_errno = os_errno;
			}
			return J9THREAD_ERR_INVALID_PRIORITY | J9THREAD_ERR_OS_ERRNO_SET;
		}
	}

	return J9THREAD_SUCCESS;
}
