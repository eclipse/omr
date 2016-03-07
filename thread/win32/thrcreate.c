/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2007, 2016
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
