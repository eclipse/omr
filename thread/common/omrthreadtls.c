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
 * @brief Thread local storage
 */

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"
#include "threaddef.h"
#include "thread_internal.h"

static void J9THREAD_PROC tls_null_finalizer(void *entry);

/**
 * Allocate a thread local storage (TLS) key.
 *
 * Create and return a new, unique key for thread local storage.
 *
 * @note The handle returned will be > 0, so it is safe to test the handle against 0 to see if it's been
 * allocated yet.
 *
 * @param[out] handle pointer to a key to be initialized with a key value
 * @return 0 on success or negative value if a key could not be allocated (i.e. all TLS has been allocated)
 *
 * @see omrthread_tls_free, omrthread_tls_set
 */
intptr_t
omrthread_tls_alloc(omrthread_tls_key_t *handle)
{
	return omrthread_tls_alloc_with_finalizer(handle, tls_null_finalizer);
}


/**
 * Allocate a thread local storage (TLS) key.
 *
 * Create and return a new, unique key for thread local storage.
 *
 * @note The handle returned will be > 0, so it is safe to test the handle against 0 to see if it's been
 * allocated yet.
 *
 * @param[out] handle pointer to a key to be initialized with a key value
 * @param[in] finalizer a finalizer function which will be invoked when a thread is detached or terminates if the thread's TLS entry for this key is non-NULL
 * @return 0 on success or negative value if a key could not be allocated (i.e. all TLS has been allocated)
 *
 * @see omrthread_tls_free, omrthread_tls_set
 */
intptr_t
omrthread_tls_alloc_with_finalizer(omrthread_tls_key_t *handle, omrthread_tls_finalizer_t finalizer)
{
	intptr_t index;
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	ASSERT(lib);

	*handle = 0;

	J9OSMUTEX_ENTER(lib->tls_mutex);

	for (index = 0; index < J9THREAD_MAX_TLS_KEYS; index++) {
		if (lib->tls_finalizers[index] == NULL) {
			*handle = index + 1;
			lib->tls_finalizers[index] = finalizer;
			break;
		}
	}

	J9OSMUTEX_EXIT(lib->tls_mutex);

	return index < J9THREAD_MAX_TLS_KEYS ? 0 : -1;
}



/**
 * Release a TLS key.
 *
 * Release a TLS key previously allocated by omrthread_tls_alloc.
 *
 * @param[in] key TLS key to be freed
 * @return 0 on success or negative value on failure
 *
 * @see omrthread_tls_alloc, omrthread_tls_set
 *
 */
intptr_t
omrthread_tls_free(omrthread_tls_key_t key)
{
	J9PoolState state;
	omrthread_t each;
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	ASSERT(lib);

	/* clear the TLS in every existing thread */
	GLOBAL_LOCK_SIMPLE(lib);
	each = pool_startDo(lib->thread_pool, &state);
	while (each) {
		each->tls[key - 1] = NULL;
		each = pool_nextDo(&state);
	}
	GLOBAL_UNLOCK_SIMPLE(lib);

	/* now return the key to the free set */
	J9OSMUTEX_ENTER(lib->tls_mutex);
	lib->tls_finalizers[key - 1] = NULL;
	J9OSMUTEX_EXIT(lib->tls_mutex);

	return 0;
}


/**
 * Set a thread's TLS value.
 *
 * @param[in] thread a thread
 * @param[in] key key to have TLS value set (any value returned by omrthread_alloc)
 * @param[in] value value to be stored in TLS
 * @return 0 on success or negative value on failure
 *
 * @see omrthread_tls_alloc, omrthread_tls_free, omrthread_tls_get
 */
intptr_t
omrthread_tls_set(omrthread_t thread, omrthread_tls_key_t key, void *value)
{
	thread->tls[key - 1] = value;

	return 0;
}



/**
 * Run finalizers on any non-NULL TLS values for the current thread
 *
 * @param[in] thread current thread
 * @return none
 */
void
omrthread_tls_finalize(omrthread_t thread)
{
	intptr_t index;
	omrthread_library_t lib = thread->library;

	for (index = 0; index < J9THREAD_MAX_TLS_KEYS; index++) {
		if (thread->tls[index] != NULL) {
			void *value;
			omrthread_tls_finalizer_t finalizer;

			/* read the value and finalizer together under mutex to be sure that they belong together */
			J9OSMUTEX_ENTER(lib->tls_mutex);
			value = thread->tls[index];
			finalizer = lib->tls_finalizers[index];
			J9OSMUTEX_EXIT(lib->tls_mutex);

			if (value) {
				finalizer(value);
			}
		}
	}
}

void
omrthread_tls_finalizeNoLock(omrthread_t thread)
{
	intptr_t index = 0;
	omrthread_library_t lib = thread->library;

	for (index = 0; index < J9THREAD_MAX_TLS_KEYS; index++) {
		if (NULL != thread->tls[index]) {
			void *value = thread->tls[index];
			omrthread_tls_finalizer_t finalizer = lib->tls_finalizers[index];

			if (NULL != value) {
				finalizer(value);
			}
		}
	}
}

static void J9THREAD_PROC
tls_null_finalizer(void *entry)
{
	/* do nothing */
}
