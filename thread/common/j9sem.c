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
