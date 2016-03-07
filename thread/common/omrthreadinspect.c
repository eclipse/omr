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
 */

/**
 * This file contains thread routines which are compiled twice -- once for in-process, and
 * once for out-of-process uses (e.g. debug extensions).
 * The APIs in this file are only used for inspecting threads -- not for modifying them
 */

#if defined(LINUX)
/* Allowing the use of pthread_attr_getstack in omrthread_get_stack_range */
#define _GNU_SOURCE
#include <features.h>
#elif defined(OSX)
#include <pthread.h>
#define _XOPEN_SOURCE
#endif /* defined(OSX) */

#include "threaddef.h"
#include "omrthreadinspect.h"

#define READU(field) ((uintptr_t)(field))
#define READP(field) (field)

static omrthread_library_t get_default_library(void);


/**
 * Initialise a omrthread_monitor_walk_state_t structure used to walk the monitor pools.
 *
 * @param[in] walkState This is a pointer to a omrthread_monitor_walk_state_t. When this call returns it
 * will be ready to use on the first call to omrthread_monitor_walk
 * @return void
 *
 * @see omrthread_monitor_walk
 *
 */
void
omrthread_monitor_init_walk(omrthread_monitor_walk_state_t *walkState)
{
	omrthread_library_t lib = get_default_library();
	ASSERT(lib);
	ASSERT(lib->monitor_pool);
	ASSERT(lib->monitor_pool->entries);
	walkState->pool = lib->monitor_pool;
	walkState->monitorIndex = 0;
	walkState->lockTaken = FALSE;
}

/**
 * Walk all active monitors.
 *
 * NOTE: Do *NOT* factor out the calls to MACRO_SELF().
 *       e.g. omrthread_t self = MACRO_SELF();
 * They must be made within the calls to GLOBAL_(UN)LOCK so that
 * when this file is compiled for "out of process" (i.e. in dbgext),
 * these GLOBAL_* macros become nops, so no call is made to MACRO_SELF(),
 * and therefore there's no required reference to 'default_library'.
 *
 * @param[in] walkState This is a pointer to a omrthread_monitor_walk_state_t. It should be initialised using omrthread_monitor_init_walk
 * before calling omrthread_monitor_walk to start a new walk. (thread lib will be globally locked on the first call)
 * @return a pointer to a monitor, or NULL if all monitors walked (thread lib will be globally unlocked when NULL is returned).
 *
 * @note As this is currently implemented, this must be called to walk ALL monitors. It can't
 * be used to look for a specific monitor and then quit because the GLOBAL_LOCK be taken when
 * the walk starts and exited when the walk ends. If the walk is not completed the lock will not
 * be released.
 *
 * @see omrthread_monitor_walk_no_locking
 *
 */
omrthread_monitor_t
omrthread_monitor_walk(omrthread_monitor_walk_state_t *walkState)
{

	omrthread_monitor_t monitor = NULL;

	if (FALSE == walkState->lockTaken) {
		/* Take the lock if we are starting the walk */
		GLOBAL_LOCK(MACRO_SELF(), CALLER_MONITOR_WALK);
		walkState->lockTaken = TRUE;
	}

	monitor = omrthread_monitor_walk_no_locking(walkState);

	if (NULL == monitor) {
		/* Release the lock if we have finished the walk */
		walkState->lockTaken = FALSE;
		GLOBAL_UNLOCK(MACRO_SELF());
	}

	return monitor;
}

/**
 * Walk all active monitors without locking the global lock.
 *
 * The caller MUST own the global lock.
 *
 * @param[in] walkState This is a pointer to a omrthread_monitor_walk_state_t. It should be initialised
 * using omrthread_monitor_init_walk before calling omrthread_monitor_walk_no_locking to start a new walk.
 * @return a pointer to a monitor, or NULL if all monitors walked
 *
 * @note Because the caller has explicit ownership of the global lock, the caller does not have to walk all monitors.
 *
 * @see omrthread_monitor_walk
 *
 */
omrthread_monitor_t
omrthread_monitor_walk_no_locking(omrthread_monitor_walk_state_t *walkState)
{
	omrthread_monitor_t monitor = NULL;

	if (walkState->monitorIndex >= MONITOR_POOL_SIZE) {
		if (NULL == (walkState->pool = walkState->pool->next)) {
			/* we've walked all the monitors, final monitor was in use */
			return NULL;
		}
		walkState->monitorIndex = 0;
	}

	monitor = &(walkState->pool->entries[walkState->monitorIndex]);

	while (FREE_TAG == monitor->count) {
		walkState->monitorIndex++;
		if (walkState->monitorIndex >= MONITOR_POOL_SIZE) {
			if (NULL == (walkState->pool = walkState->pool->next)) {
				/* we've walked all the monitors, final monitor was free */
				return NULL;
			}
			walkState->monitorIndex = 0;
		}
		monitor = &(walkState->pool->entries[walkState->monitorIndex]);
	}
	walkState->monitorIndex++;
	return monitor;
}

/**
 * Get a thread's thread local storage (TLS) value.
 *
 * @param[in] a thread
 * @param[in] key key to have TLS value returned (value returned by omrthread_tls_alloc)
 * @return pointer to location of TLS or NULL on failure.
 *
 */
void *
omrthread_tls_get(omrthread_t thread, omrthread_tls_key_t key)
{
	return (void *)READU(thread->tls[key - 1]);
}

/**
 * Return a thread's scheduling priority.
 *
 * @param[in] thread (non-NULL)
 * @return scheduling priority
 * @see omrthread_create, omrthread_set_priority
 *
 */
uintptr_t
omrthread_get_priority(omrthread_t thread)
{
	ASSERT(thread);
	return READU(thread->priority);
}


/**
 * Return a thread's flags.
 *
 * @param[in] thread (non-NULL)
 * @param[in] blocker if non-NULL, will be set to the monitor on which the thread is blocked (if any)
 * @return flags
 *
 */
uintptr_t
omrthread_get_flags(omrthread_t thread, omrthread_monitor_t *blocker)
{
	uintptr_t flags;

	ASSERT(thread);

	J9OSMUTEX_ENTER(thread->mutex);

	if (blocker) {
		*blocker = READP(thread->monitor);
	}
	flags = READU(thread->flags);

	J9OSMUTEX_EXIT(thread->mutex);

	return flags;
}

/**
 * Return a thread's state.

 * The returned data is not validated in any way, and may appear inconsistent if,
 * for example, the thread owning the blocker exits the blocker monitor.
 *
 * @param[in] thread (non-NULL)
 * @param[out] state
 * @return void
 */
void
omrthread_get_state(omrthread_t thread, omrthread_state_t *const state)
{
	if (!thread) {
		return;
	}

	if (!state) {
		return;
	}

	J9OSMUTEX_ENTER(thread->mutex);
	state->flags = READU(thread->flags);
	state->blocker = READP(thread->monitor);
	if (state->blocker) {
		state->owner = READP(state->blocker->owner);
		state->count = READU(state->blocker->count);
	} else {
		state->owner = 0;
		state->count = 0;
	}
	J9OSMUTEX_EXIT(thread->mutex);
}

/**
 * Return a thread's OS id
 *
 * @param[in] thread (non-NULL)
 * @return OS id
 * @see omrthread_get_ras_tid
 */
uintptr_t
omrthread_get_osId(omrthread_t thread)
{
	ASSERT(thread);
	return READU(thread->tid);
}


/**
 * Return a monitor's name.
 *
 * @param[in] monitor (non-NULL)
 * @return pointer to the monitor's name (may be NULL)
 *
 * @see omrthread_monitor_init_with_name
 *
 */
char *
omrthread_monitor_get_name(omrthread_monitor_t monitor)
{
	ASSERT(monitor);
	return READP(monitor->name);
}

/**
 * Get native stack address range and size (VMDESIGN 2038)
 * @param[in] thread Current native thread
 * @param[in] stackStart A pointer to the starting of native thread stack
 * @param[in] stackEnd   A pointer to the end of native thread stack
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_THREAD for NULL thread argument
 * @retval J9THREAD_ERR_GETATTR_NP for pthread_getattr_np failure. thread->errno set
 * @retval J9THREAD_ERR_GETSTACK for stack range retrieval failure. thread->errno set
 * @retval J9THREAD_ERR_UNSUPPORTED_PLAT for unsupported platform
 */
uintptr_t
omrthread_get_stack_range(omrthread_t thread, void **stackStart, void **stackEnd)
{

#if defined(LINUX)
	pthread_attr_t attr;
	OSTHREAD osTid = thread->handle;
	uintptr_t rc = 0;
	size_t stackSize;

	if (!thread) {
		return J9THREAD_ERR_INVALID_THREAD;
	}

	/* Retrieve the pthread_attr_t from the thread */
	if ((rc = pthread_getattr_np(osTid, &attr)) != 0) {
		thread->os_errno = rc;
		return (J9THREAD_ERR_GETATTR_NP | J9THREAD_ERR_OS_ERRNO_SET);
	}

	/* Retrieve base stack address and stack size from pthread_attr_t */
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600)
	if ((rc = pthread_attr_getstack(&attr, stackStart, &stackSize)) != 0) {
		thread->os_errno = rc;
		return (J9THREAD_ERR_GETSTACK | J9THREAD_ERR_OS_ERRNO_SET);
	}
#else
	if ((rc = pthread_attr_getstackaddr(&attr, stackStart)) != 0) {
		thread->os_errno = rc;
		return (J9THREAD_ERR_GETSTACK | J9THREAD_ERR_OS_ERRNO_SET);
	}

	if ((rc = pthread_attr_getstacksize(&attr, &stackSize)) != 0) {
		thread->os_errno = rc;
		return (J9THREAD_ERR_GETSTACK | J9THREAD_ERR_OS_ERRNO_SET);
	}
#endif /* #if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) */
	pthread_attr_destroy(&attr);

	/* On Linux, native stack grows from high to low memory */
	*stackEnd = (void *)((uintptr_t)*stackStart + stackSize);
	return J9THREAD_SUCCESS;
#elif defined(OSX)
	OSTHREAD osTid = thread->handle;
	uintptr_t rc = 0;
	size_t stackSize = 0;

	*stackStart = pthread_get_stackaddr_np(osTid);
	stackSize = pthread_get_stacksize_np(osTid);
	*stackEnd = (void *)((uintptr_t)*stackStart + stackSize);
	return J9THREAD_SUCCESS;
#else /* defined(OSX) */
	return J9THREAD_ERR_UNSUPPORTED_PLAT;
#endif /* defined(LINUX) */
}

#if (defined(OMR_THR_JLM))
/*
 * Return a monitor's tracing information.
 *
 * @param[in] monitor (non-NULL)
 * @return pointer to the monitor's tracing information (may be NULL)
 *
 */
J9ThreadMonitorTracing *
omrthread_monitor_get_tracing(omrthread_monitor_t monitor)
{
	ASSERT(monitor);

	return READP(monitor->tracing);
}

#endif /* OMR_THR_JLM */


/**
 * Return the default threading library.
 *
 * @return pointer to the default threading library
 *
 */
static omrthread_library_t
get_default_library(void)
{
#if defined (J9VM_OUT_OF_PROCESS)
	return dbgGetThreadLibrary();
#else
	return GLOBAL_DATA(default_library);
#endif
}
