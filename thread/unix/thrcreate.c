/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include <pthread.h>
#include "threaddef.h"
#include "unix/unixthreadattr.h"

intptr_t
osthread_create(struct J9Thread *self, OSTHREAD *handle, const omrthread_attr_t attr,
				WRAPPER_FUNC entrypoint, WRAPPER_ARG entryarg)
{
	intptr_t retCode;
	unixthread_attr_t ux;

	ASSERT(self);
	ASSERT(handle);
	ASSERT(attr);
	ASSERT(entrypoint);

	ux = (unixthread_attr_t)attr;

	retCode = DEBUG_SYSCALL(pthread_create(handle, &ux->pattr, entrypoint, entryarg));
	if (retCode != 0) {
		if (self) {
#if defined(J9ZOS390)
			/* z stores the error code in errno and errno2, not in the return value from pthread_create() */
			self->os_errno = errno;
			self->os_errno2 = __errno2();
#else /* J9ZOS390 */
			self->os_errno = (omrthread_os_errno_t)retCode;
#endif /* J9ZOS390 */
		}
		return J9THREAD_ERR_THREAD_CREATE_FAILED | J9THREAD_ERR_OS_ERRNO_SET;
	}
	return J9THREAD_SUCCESS;
}
