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
 * @brief J9 Lock Monitoring
 */

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrthread.h"
#include "threaddef.h"
#include "thread_internal.h"

/*
 * This file should be compiled only if OMR_THR_JLM is #defined.
 */
#if !defined(OMR_THR_JLM)
#error JLM feature is not enabled by this buildspec.
#endif

static intptr_t jlm_init(omrthread_library_t lib);
static intptr_t jlm_init_pools(omrthread_library_t lib);
static intptr_t jlm_gc_lock_init(omrthread_library_t lib);
static void jlm_thread_clear(omrthread_t thread);

/**
 * Initialize storage and clear structures for JLM thread and monitor tracing structures
 *
 * @return 0 on success, non-zero on failure
 */
intptr_t
omrthread_jlm_init(uintptr_t flags)
{
	omrthread_t self = MACRO_SELF();
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	intptr_t retVal;

	ASSERT(self);
	ASSERT(lib);

	GLOBAL_LOCK(self, CALLER_JLM_INIT);

	retVal = jlm_init(lib);

	/*
	 * Clear all JLM flags and then,
	 * if we were successful, set the appropriate flags
	 */
	lib->flags &= ~J9THREAD_LIB_FLAG_JLM_ENABLED_ALL;
	if (0 == retVal) {
		lib->flags |= (flags | J9THREAD_LIB_FLAG_JLM_HAS_BEEN_ENABLED);
	} else {
		/* tracing info in indeterminate state.. don't use it */
		lib->flags &= ~J9THREAD_LIB_FLAG_JLM_HAS_BEEN_ENABLED;
	}


	GLOBAL_UNLOCK(self);

	return retVal;
}

/**
 * Initialize and clear a thread's JLM tracing information.
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param[in] thread thread to be initialized
 * @return 0 on success, non-zero on failure
 */
static intptr_t
jlm_base_init(omrthread_library_t lib)
{
	omrthread_t thread;
	omrthread_monitor_t monitor;
	omrthread_monitor_walk_state_t walkState;
	pool_state state;

	ASSERT(lib);
	ASSERT(lib->thread_pool);

	/* Initialize pools needed for JLM */
	if (jlm_init_pools(lib) != 0) {
		return -1;
	}

	/* Set up the existing threads */
	thread = pool_startDo(lib->thread_pool, &state);
	while (thread != NULL) {
		if (jlm_thread_init(thread) != 0) {
			return -1;
		}
		thread = pool_nextDo(&state);
	}

	/* Set up the existing monitors */
	monitor = NULL;
	omrthread_monitor_init_walk(&walkState);
	while (NULL != (monitor = omrthread_monitor_walk_no_locking(&walkState))) {
		if (jlm_monitor_init(lib, monitor) != 0) {
			return -1;
		}
	}

#if defined(OMR_THR_JLM_HOLD_TIMES)
	{
		/* Set the minimum clock skew to expect for timings */
		uintptr_t *skew_hi;
		skew_hi = omrthread_global("clockSkewHi");
		if (skew_hi != NULL) {
			lib->clock_skew = ((uint64_t)*skew_hi) << 32;
		} else {
			lib->clock_skew = (uint64_t)0;
		}
	}
#endif

	return 0;
}

/**
 * Initialize and clear a thread's JLM tracing information.
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param[in] thread thread to be initialized
 * @return 0 on success, non-zero on failure
 */
static intptr_t
jlm_init(omrthread_library_t lib)
{
	intptr_t result;

	/* do the base initialization */
	result = jlm_base_init(lib);
	if (result != 0) {
		return result;
	}

	/* Set up the GC lock tracing structure */
	if (jlm_gc_lock_init(lib) != 0) {
		return -1;
	}

	return 0;
}

#if (defined(OMR_THR_ADAPTIVE_SPIN))
/**
 * Initialize structures needed for doing adaptive spinning using
 * the jlm data
 *
 * @return 0 on success, non-zero on failure
 *
 */
intptr_t
jlm_adaptive_spin_init(void)
{
	intptr_t result;
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	omrthread_t self = MACRO_SELF();
	uintptr_t adaptiveFlags = 0;

	ASSERT(self);
	ASSERT(lib);

	if (0 != *(uintptr_t *)omrthread_global("adaptSpinHoldtimeEnable")) {
		adaptiveFlags |= J9THREAD_LIB_FLAG_JLM_HOLDTIME_SAMPLING_ENABLED;
	}
	if (0 != *(uintptr_t *)omrthread_global("adaptSpinSlowPercentEnable")) {
		adaptiveFlags |= J9THREAD_LIB_FLAG_JLM_SLOW_SAMPLING_ENABLED;
	}

#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
	if (0 != *(uintptr_t *)omrthread_global("customAdaptSpinEnabled")) {
		adaptiveFlags |= J9THREAD_LIB_FLAG_CUSTOM_ADAPTIVE_SPIN_ENABLED;
	}
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */

	if (0 != adaptiveFlags) {
		GLOBAL_LOCK(self, CALLER_JLM_INIT);

		/* do the base initialization */
		result = jlm_base_init(lib);
		if (0 != result) {
			GLOBAL_UNLOCK(self);
			return result;
		}

		lib->flags |= adaptiveFlags;

		GLOBAL_UNLOCK(self);
	}

	return 0;
}
#endif /* OMR_THR_ADAPTIVE_SPIN */



/**
 * Allocate (if not done already) the pools for JLM tracing structures.
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param[in] lib thread library
 * @return 0 on success, non-zero on failure
 */
static intptr_t
jlm_init_pools(omrthread_library_t lib)
{
	ASSERT(lib);

	if (NULL == lib->monitor_tracing_pool) {
		lib->monitor_tracing_pool = pool_new(sizeof(J9ThreadMonitorTracing), 0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_THREADS, omrthread_mallocWrapper, omrthread_freeWrapper, lib);
		if (NULL == lib->monitor_tracing_pool) {
			return -1;
		}
	}
#if defined(OMR_THR_JLM_HOLD_TIMES)
	if (NULL == lib->thread_tracing_pool) {
		lib->thread_tracing_pool = pool_new(sizeof(J9ThreadTracing), 0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_THREADS, omrthread_mallocWrapper, omrthread_freeWrapper, lib);
		if (NULL == lib->thread_tracing_pool) {
			return -1;
		}
	}
#endif

	return 0;

}


/**
 * Initialize special tracing for GC
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param  lib thread library
 * @return 0 on success, non-zero on failure
 * @todo remove knowledge of GC
 */
static intptr_t
jlm_gc_lock_init(omrthread_library_t lib)
{
	ASSERT(lib);
	ASSERT(lib->monitor_tracing_pool);

	if (lib->gc_lock_tracing == NULL) {
		lib->gc_lock_tracing = pool_newElement(lib->monitor_tracing_pool);
		if (NULL != lib->gc_lock_tracing) {
			memset(lib->gc_lock_tracing, 0, sizeof(*lib->gc_lock_tracing));
		}
	}

	return (NULL == lib->gc_lock_tracing) ? -1 : 0;

}


/**
 * Return tracing info.
 *
 * @return pointer to GC lock tracing structure. 0 of not yet initialized
 */
J9ThreadMonitorTracing *
omrthread_jlm_get_gc_lock_tracing(void)
{
	omrthread_t self = MACRO_SELF();
	ASSERT(self);
	ASSERT(self->library);
	return self->library->gc_lock_tracing;
}


/**
 * Initialize and clear a thread's JLM tracing information.
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param[in] thread thread to be initialized
 * @return 0 on success, non-zero on failure
 */
intptr_t
jlm_thread_init(omrthread_t thread)
{
#if defined(OMR_THR_JLM_HOLD_TIMES)
	omrthread_library_t lib = thread->library;
	ASSERT(thread);
	ASSERT(lib);
	ASSERT(lib->thread_tracing_pool);
	if (NULL == thread->tracing) {
		thread->tracing = pool_newElement(lib->thread_tracing_pool);
	}
	if (thread->tracing != NULL) {
		jlm_thread_clear(thread);
	}

	return (NULL == thread->tracing) ? -1 : 0;
#else
	return 0;
#endif

}


/**
 * Clear (reset) a thread's JLM tracing structure.
 *
 * Assumes the thread's tracing structure has already been allocated.
 *
 * @param[in] thread thread to be initialized (non-NULL)
 * @return none
 */
static void
jlm_thread_clear(omrthread_t thread)
{
	ASSERT(thread);
#if	defined(OMR_THR_JLM_HOLD_TIMES)
	ASSERT(thread->tracing);
	memset(thread->tracing, 0, sizeof(*thread->tracing));
#endif
}


/**
 * Free a monitor's JLM tracing information.
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param[in] monitor monitor
 * @return none
 */
void
jlm_thread_free(omrthread_library_t lib, omrthread_t thread)
{
#if defined(OMR_THR_JLM_HOLD_TIMES)
	ASSERT(lib);
	ASSERT(thread);

	if (thread->tracing != NULL) {
		ASSERT(lib->thread_tracing_pool);
		pool_removeElement(lib->thread_tracing_pool, thread->tracing);
		thread->tracing = NULL;
	}
#endif
}


/**
 * Initialize and clear a monitor's JLM tracing information.
 *
 * Tracing information for the monitor is allocated if not already allocated
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param[in] monitor monitor to be initialized
 * @return none
 */
intptr_t
jlm_monitor_init(omrthread_library_t lib, omrthread_monitor_t monitor)
{
	ASSERT(lib);
	ASSERT(lib->monitor_tracing_pool);
	ASSERT(monitor);

	if (NULL == monitor->tracing) {
		monitor->tracing = pool_newElement(lib->monitor_tracing_pool);
	}
	if (monitor->tracing != NULL) {
		jlm_monitor_clear(lib, monitor);
	}
	return (NULL == monitor->tracing) ? -1 : 0;
}


/**
 * Clear (reset) a monitor's JLM tracing structure.
 *
 * @param[in] pointer to thread library that method can use
 * Assumes the monitor's tracing structure has already been allocated.
 *
 * @param[in] monitor a monitor to be initialized
 * @return none
 */
void
jlm_monitor_clear(omrthread_library_t lib, omrthread_monitor_t monitor)
{
	ASSERT(monitor);
	ASSERT(monitor->tracing);
	memset(monitor->tracing, 0, sizeof(*monitor->tracing));
}


/**
 * Free a monitor's JLM tracing information.
 *
 * Must be called under protection of GLOBAL LOCK
 *
 * @param[in] monitor monitor
 * @return none
 */
void
jlm_monitor_free(omrthread_library_t lib, omrthread_monitor_t monitor)
{
	ASSERT(lib);
	ASSERT(monitor);

	if (monitor->tracing != NULL) {
		ASSERT(lib->monitor_tracing_pool);
		pool_removeElement(lib->monitor_tracing_pool, monitor->tracing);
		monitor->tracing = NULL;
	}

}
