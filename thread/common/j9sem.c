/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Thread
 * @brief Semaphores
 */

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"
#include "thrdsup.h"
#include "thread_internal.h"

/*
 * The SEM_CREATE() and SEM_DESTROY() macros hide calls to
 * omrthread_mallocWrapper() and omrthread_freeWrapper().
 */

/**
 * Initialize a semaphore.
 *
 * Acquire a semaphore from the threading library.
 *
 * @param[out] sp pointer to semaphore to be initialized
 * @param[in] initValue initial count value (>=0) for the semaphore
 * @return  0 on success or negative value on failure
 *
 * @deprecated Semaphores are no longer supported.
 *
 * @see j9sem_destroy, j9sem_init, j9sem_post
 */
intptr_t
j9sem_init(j9sem_t *sp, int32_t initValue)
{
#ifdef OMR_INTERP_HAS_SEMAPHORES
	j9sem_t s;
	intptr_t rc = -1;
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	(*sp) = s = SEM_CREATE(lib, initValue);
	if (s) {
		rc = SEM_INIT(s, 0, initValue);
	}
	return rc;
#else
	return -1;
#endif
}


/**
 * Release a semaphore by 1.
 *
 * @param[in] s semaphore to be released by 1
 * @return  0 on success or negative value on failure
 *
 * @deprecated Semaphores are no longer supported.
 *
 * @see j9sem_init, j9sem_destroy, j9sem_wait
 */
intptr_t
j9sem_post(j9sem_t s)
{
#ifdef OMR_INTERP_HAS_SEMAPHORES
	if (s) {
		return SEM_POST(s);
	}
	return -1;
#else
	return -1;
#endif
}


/**
 * Wait on a semaphore.
 *
 * @param[in] s semaphore to be waited on
 * @return  0 on success or negative value on failure
 *
 * @deprecated Semaphores are no longer supported.
 *
 * @see j9sem_init, j9sem_destroy, j9sem_wait
 *
 */
intptr_t
j9sem_wait(j9sem_t s)
{
#ifdef OMR_INTERP_HAS_SEMAPHORES
	if (s) {
		while (SEM_WAIT(s) != 0) {
			/* loop until success */
		}
		return 0;
	} else {
		return -1;
	}

#else
	return -1;
#endif
}

/**
 * Destroy a semaphore.
 *
 * Returns the resources associated with a semaphore back to the J9 threading library.
 *
 * @param[in] s semaphore to be destroyed
 * @return  0 on success or negative value on failure
 *
 * @deprecated Semaphores are no longer supported.
 *
 * @see j9sem_init, j9sem_wait, j9sem_post
 */
intptr_t
j9sem_destroy(j9sem_t s)
{
#ifdef OMR_INTERP_HAS_SEMAPHORES
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	int rval = 0;
	if (s) {
		rval = SEM_DESTROY(s);
		SEM_FREE(lib, s);
	}
	return rval;
#else
	return -1;
#endif
}
