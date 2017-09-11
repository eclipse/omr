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

#ifndef thread_api_h
#define thread_api_h

/*
 * @ddr_namespace: map_to_type=ThreadApiConstants
 */

/**
* @file thread_api.h
* @brief Public API for the THREAD module.
*
* This file contains public function prototypes and
* type definitions for the THREAD module.
*
*/

#include "omrcfg.h"
#include "omrthread.h"
#include "omrcomp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define J9THREAD_RWMUTEX_OK		 	 0
#define J9THREAD_RWMUTEX_FAIL	 	 1
#define J9THREAD_RWMUTEX_WOULDBLOCK -1

/* Define conversions for units of time used in thrprof.c */
#define SEC_TO_NANO_CONVERSION_CONSTANT		1000 * 1000 * 1000
#define MICRO_TO_NANO_CONVERSION_CONSTANT	1000
#define GET_PROCESS_TIMES_IN_NANO			100

typedef struct omrthread_process_time_t {
	/* For consistency sake, times are stored as int64_t's */
	int64_t _systemTime;
	int64_t _userTime;
} omrthread_process_time_t;

typedef struct omrthread_state_t {
	uintptr_t flags;
	omrthread_monitor_t blocker;
	omrthread_t owner;
	uintptr_t count;
} omrthread_state_t;

typedef struct omrthread_attr *omrthread_attr_t;
#define J9THREAD_ATTR_DEFAULT ((omrthread_attr_t *)NULL)

typedef enum omrthread_detachstate_t {
	J9THREAD_CREATE_DETACHED,
	J9THREAD_CREATE_JOINABLE,
	/* ensure 4-byte enum */
	omrthread_detachstate_EnsureWideEnum = 0x1000000
} omrthread_detachstate_t;

typedef enum omrthread_schedpolicy_t {
	J9THREAD_SCHEDPOLICY_INHERIT,
	J9THREAD_SCHEDPOLICY_OTHER,
	J9THREAD_SCHEDPOLICY_RR,
	J9THREAD_SCHEDPOLICY_FIFO,
	/* dummy value marking end of list */
	omrthread_schedpolicy_LastEnum,
	/* ensure 4-byte enum */
	omrthread_schedpolicy_EnsureWideEnum = 0x1000000
} omrthread_schedpolicy_t;

typedef uintptr_t omrthread_prio_t;

typedef struct omrthread_monitor_walk_state_t {
	struct J9ThreadMonitorPool *pool;
	uintptr_t monitorIndex;
	BOOLEAN lockTaken;
} omrthread_monitor_walk_state_t;

/* ---------------- omrthreadinspect.c ---------------- */
#if defined (J9VM_OUT_OF_PROCESS)
/* redefine thread functions */
#define omrthread_monitor_walk dbg_omrthread_monitor_walk
#define omrthread_monitor_walk_no_locking dbg_omrthread_monitor_walk_no_locking
#define omrthread_tls_get dbg_omrthread_tls_get
#define omrthread_get_priority dbg_omrthread_get_priority
#define omrthread_get_flags dbg_omrthread_get_flags
#define omrthread_get_state dbg_omrthread_get_state
#define omrthread_get_osId dbg_omrthread_get_osId
#define omrthread_monitor_get_name dbg_omrthread_monitor_get_name
#define omrthread_get_stack_range dbg_omrthread_get_stack_range
#define omrthread_monitor_get_tracing dbg_omrthread_monitor_get_tracing
#define getVMThreadRawState dbgGetVMThreadRawState
#endif


/**
* @brief
* @param void
* @return void
*/
intptr_t
omrthread_init_library(void);

/**
* @brief
* @param void
* @return void
*/
void
omrthread_shutdown_library(void);

/**
* @brief
* @param thread
* @param blocker
* @return uintptr_t
*/
uintptr_t
omrthread_get_flags(omrthread_t thread, omrthread_monitor_t *blocker);

/**
 * @brief
 * @param thread
 * @param state
 * @return void
 */
void
omrthread_get_state(omrthread_t thread, omrthread_state_t *const state);

/**
* @brief
* @param thread
* @return uintptr_t
*/
uintptr_t
omrthread_get_priority(omrthread_t thread);

/**
* @brief
* @param thread
* @return uintptr_t
*/
uintptr_t
omrthread_get_osId(omrthread_t thread);

/**
* @brief
* @param monitor
* @return char*
*/
char *
omrthread_monitor_get_name(omrthread_monitor_t monitor);

/**
 * @brief
 * @param thread
 * @param stackStart
 * @param stackEnd
 * @return uintptr_t
 */
uintptr_t
omrthread_get_stack_range(omrthread_t thread, void **stackStart, void **stackEnd);


#if (defined(OMR_THR_JLM))
/**
* @brief
* @param monitor
* @return J9ThreadMonitorTracing*
*/
J9ThreadMonitorTracing *
omrthread_monitor_get_tracing(omrthread_monitor_t monitor);
#endif /* OMR_THR_JLM */


/**
* @brief
* @param walkState
* @return omrthread_monitor_t
*/
omrthread_monitor_t
omrthread_monitor_walk(omrthread_monitor_walk_state_t *walkState);

/**
* @brief
* @param walkState
* @return omrthread_monitor_t
*/
omrthread_monitor_t
omrthread_monitor_walk_no_locking(omrthread_monitor_walk_state_t *walkState);

/**
* @brief
* @param walkState
* @return void
*/
void
omrthread_monitor_init_walk(omrthread_monitor_walk_state_t *walkState);

/**
* @brief
* @param thread
* @param key
* @return void*
*/
void *
omrthread_tls_get(omrthread_t thread, omrthread_tls_key_t key);

/**
 * @brief
 * @param monitor
 * @return uintptr_t
 */
uintptr_t
omrthread_monitor_owned_by_self(omrthread_monitor_t monitor);

/**
 * Turn on CPU monitoring and enable monitoring on the current thread.
 * This function is called early in startup.
 * (CPU Monitoring is turned off by default in OMR)
 * @param thread
 * @return void
 */
void
omrthread_lib_enable_cpu_monitor(omrthread_t thread);

/* ---------------- rwmutex.c ---------------- */

/**
* @struct
*/
struct RWMutex;

/**
*@typedef
*/
typedef struct RWMutex *omrthread_rwmutex_t;

/**
* @brief
* @param mutex
* @return intptr_t
*/
intptr_t
omrthread_rwmutex_destroy(omrthread_rwmutex_t mutex);


/**
* @brief
* @param mutex
* @return intptr_t
*/
intptr_t
omrthread_rwmutex_enter_read(omrthread_rwmutex_t mutex);


/**
* @brief
* @param mutex
* @return intptr_t
*/
intptr_t
omrthread_rwmutex_enter_write(omrthread_rwmutex_t mutex);

/**
 * @brief
 * @param mutex
 * @returns intptr_t
 */
intptr_t
omrthread_rwmutex_try_enter_write(omrthread_rwmutex_t mutex);

/**
* @brief
* @param mutex
* @return intptr_t
*/
intptr_t
omrthread_rwmutex_exit_read(omrthread_rwmutex_t mutex);


/**
* @brief
* @param mutex
* @return intptr_t
*/
intptr_t
omrthread_rwmutex_exit_write(omrthread_rwmutex_t mutex);


/**
* @brief
* @param handle
* @param flags
* @param name
* @return intptr_t
*/
intptr_t
omrthread_rwmutex_init(omrthread_rwmutex_t *handle, uintptr_t flags, const char *name);

/**
* @brief
* @param mutex
* @return BOOLEAN
*/
BOOLEAN
omrthread_rwmutex_is_writelocked(omrthread_rwmutex_t mutex);

/* ---------------- omrthreadpriority.c ---------------- */

/**
* @brief
* @param nativePriority
* @return uintptr_t
*/
uintptr_t
omrthread_map_native_priority(int nativePriority);


/* ---------------- omrthread.c ---------------- */

/**
* @brief
* @param s
* @return intptr_t
*/
intptr_t
j9sem_destroy(j9sem_t s);


/**
* @brief
* @param sp
* @param initValue
* @return intptr_t
*/
intptr_t
j9sem_init(j9sem_t *sp, int32_t initValue);


/**
* @brief
* @param s
* @return intptr_t
*/
intptr_t
j9sem_post(j9sem_t s);


/**
* @brief
* @param s
* @return intptr_t
*/
intptr_t
j9sem_wait(j9sem_t s);

/**
* @brief
* @param handle
* @return void
*/
void
omrthread_abort(omrthread_t handle);


/**
* @brief
* @param handle
* @return intptr_t
*/
intptr_t
omrthread_attach(omrthread_t *handle);

/**
 * Wait for a thread to terminate
 *
 * This function follows the conventions of pthread_join().
 *
 * - A thread can't join itself.
 * - A thread can only be joined once.
 * - The target thread must have been created with detachstate=J9THREAD_CREATE_JOINABLE.
 *
 * If you pass in an invalid thread argument, the behaviour is undefined
 * because the omrthread_t is in an unknown state. The function may segv.
 *
 * A joinable thread MUST be joined to free its resources.
 *
 * @param[in] thread Wait for this given thread to terminate.
 * @return a omrthread error code
 * @retval J9THREAD_SUCCESS Success
 * @retval J9THREAD_INVALID_ARGUMENT Target thread is the current thread,
 *         or the target thread could be determined to be non-joinable.
 * @retval J9THREAD_TIMED_OUT The O/S join function returned a timeout status.
 * @retval J9THREAD_ERR The O/S join function returned an error code.
 * @retval J9THREAD_ERR|J9THREAD_ERR_OS_ERRNO_SET The O/S join function returned an error code,
 *         and an os_errno is also available.
 */
intptr_t
omrthread_join(omrthread_t thread);

/**
* @brief
* @param thread
* @return void
*/
void
omrthread_cancel(omrthread_t thread);

/**
 * Helper function that attaches an existing OS thread
 * to the threading library based on the attributes passed.
 *
 * Create a new omrthread_t to represent the existing OS thread.
 * Attaching a thread is required when a thread was created
 * outside of the J9 threading library wants to use any of the
 * J9 threading library functionality.
 *
 * If the OS thread is already attached, handle is set to point
 * to the existing omrthread_t.
 *
 * If a NULL attr is passed, the thread will be classified as a J9THREAD_CATEGORY_SYSTEM_THREAD thread.
 * If a non NULL attr is passed, only the thread category will be used, all other attributes will be ignored.
 *
 * @param[out] handle pointer to a omrthread_t to be set (will be ignored if null)
 * @param[in]  attr of the thread to be set
 * @return success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_xxx failure
 *
 * @see omrthread_attach
 * @see omrthread_detach
 */
intptr_t
omrthread_attach_ex(omrthread_t *handle, omrthread_attr_t *attr);

/**
* @brief
* @param void
* @return uintptr_t
*/
uintptr_t
omrthread_clear_interrupted(void);


/**
* @brief
* @param void
* @return uintptr_t
*/
uintptr_t
omrthread_clear_priority_interrupted(void);


/**
* @brief
* @param handle
* @param stacksize
* @param priority
* @param suspend
* @param entrypoint
* @param entryarg
* @return intptr_t
*/
intptr_t
omrthread_create(omrthread_t *handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend, omrthread_entrypoint_t entrypoint, void *entryarg);

/**
* @brief
* @param handle
* @param attr
* @param suspend
* @param entrypoint
* @param entryarg
* @return intptr_t
*/
intptr_t
omrthread_create_ex(omrthread_t *handle, const omrthread_attr_t *attr, uintptr_t suspend, omrthread_entrypoint_t entrypoint, void *entryarg);

/**
* @brief
* @param void
* @return uintptr_t
*/
uintptr_t
omrthread_current_stack_free(void);


/**
* @brief
* @param thread
* @return void
*/
void
omrthread_detach(omrthread_t thread);


#if (defined(OMR_THR_TRACING))
/**
* @brief
* @param thread
* @return void
*/
void
omrthread_dump_trace(omrthread_t thread);
#endif /* OMR_THR_TRACING */

/**
* @brief
* @param monitor
* @return void
*/
void OMRNORETURN
omrthread_exit(omrthread_monitor_t monitor);

/**
* @brief
* @param name
* @return uintptr_t*
*/
uintptr_t *
omrthread_global(char *name);


/**
* @brief
* @return omrthread_monitor_t
*/
omrthread_monitor_t
omrthread_global_monitor(void);


/**
* @brief
* @param thread
* @return void
*/
void
omrthread_interrupt(omrthread_t thread);


/**
* @brief
* @param thread
* @return uintptr_t
*/
uintptr_t
omrthread_interrupted(omrthread_t thread);


#if (defined(OMR_THR_JLM))
/**
* @brief
* @param void
* @return J9ThreadMonitorTracing*
*/
J9ThreadMonitorTracing *
omrthread_jlm_get_gc_lock_tracing(void);
#endif /* OMR_THR_JLM */


#if (defined(OMR_THR_JLM))
/**
* @brief
* @param flags
* @return intptr_t
*/
intptr_t
omrthread_jlm_init(uintptr_t flags);
#endif /* OMR_THR_JLM */

#if defined(OMR_THR_ADAPTIVE_SPIN)
/**
 * @brief initializes jlm for capturing data needed by the adaptive spin options
 * @param thread
 * @param adaptiveFlags flags indicating the adaptive modes that have been enabled
 * @return intptr_t
 */
intptr_t
jlm_adaptive_spin_init(void);
#endif

/**
* @brief
* @param void
* @return uintptr_t
*/
uintptr_t
omrthread_lib_get_flags(void);

/**
* @brief
* @param flags
* @return uintptr_t
*/
uintptr_t
omrthread_lib_set_flags(uintptr_t flags);

/**
* @brief
* @param flags
* @return uintptr_t
*/
uintptr_t
omrthread_lib_clear_flags(uintptr_t flags);


#define J9THREAD_LIB_CONTROL_TRACE_START "trace_start"
#define J9THREAD_LIB_CONTROL_TRACE_STOP "trace_stop"

#define J9THREAD_LIB_CONTROL_GET_MEM_CATEGORIES "get_mem_categories"

#if defined(LINUX) || defined(OSX)
#define J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING "use_realtime_scheduling"
#define J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING_ENABLED ((uintptr_t) J9THREAD_LIB_FLAG_REALTIME_SCHEDULING_ENABLED)
#define J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING_DISABLED ((uintptr_t) 0)
#endif /* defined(LINUX) || defined(OSX) */

/**
* @brief Control the thread library.
* @param key
* @param value
* @return intptr_t 0 on success, -1 on failure.
*/
intptr_t
omrthread_lib_control(const char *key, uintptr_t value);

/**
* @brief
* @param self
* @return void
*/
void
omrthread_lib_lock(omrthread_t self);


/**
* @brief
* @param self
* @return intptr_t
*/
intptr_t
omrthread_lib_try_lock(omrthread_t self);


/**
* @brief
* @param self
* @return void
*/
void
omrthread_lib_unlock(omrthread_t self);

#if defined(OMR_THR_FORK_SUPPORT)
/**
 * Post-fork reset function used to clean up and reset old omrthread and omrthread_monitor structures
 * associated with non-existent threads in the child process. This function is to be called directly
 * before fork and requires an attached thread.
 *
 * Since WIN32 has no fork, this code is currently not included. For WIN32, some of the reset code
 * is incomplete: in postForkResetThreads, the omrthread_t->handle must be reset differently.
 *
 * @note This function must only be called in the child process immediately following a fork().
 */
void
omrthread_lib_postForkChild(void);

/**
 * Perform post-fork unlocking of mutexes that must be held over a fork and reset. This function is
 * to be called directly after a fork, before any other omrthread functions, and requires an attached
 * thread.
 */
void
omrthread_lib_postForkParent(void);

/**
 * Perform pre-fork locking of mutexes that must be held over a fork and reset. This function is
 * to be called directly after a fork, before any other omrthread functions, and requires an attached
 * thread.
 */
void
omrthread_lib_preFork(void);
#endif /* !defined(OMR_THR_FORK_SUPPORT) */

/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_destroy(omrthread_monitor_t monitor);

/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_destroy_nolock(omrthread_t self, omrthread_monitor_t monitor);

/**
* @brief
* @param void
* @return void
*/
void
omrthread_monitor_flush_destroyed_monitor_list(omrthread_t self);


#if (defined(OMR_THR_TRACING))
/**
* @brief
* @param void
* @return void
*/
void
omrthread_monitor_dump_all(void);
#endif /* OMR_THR_TRACING */


#if (defined(OMR_THR_TRACING))
/**
* @brief
* @param monitor
* @return void
*/
void
omrthread_monitor_dump_trace(omrthread_monitor_t monitor);
#endif /* OMR_THR_TRACING */


/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_enter(omrthread_monitor_t monitor);

/**
* @brief
* @param monitor
* @param threadId
* @return intptr_t
*/
intptr_t
omrthread_monitor_enter_abortable_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId);

/**
* @brief
* @param monitor
* @param threadId
* @return intptr_t
*/
intptr_t
omrthread_monitor_enter_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId);


/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_exit(omrthread_monitor_t monitor);

/**
* @brief
* @param monitor
* @param threadId
* @return intptr_t
*/
intptr_t
omrthread_monitor_exit_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId);


/**
* @brief
* @param handle
* @param flags
* @param name
* @return intptr_t
*/
intptr_t
omrthread_monitor_init_with_name(omrthread_monitor_t *handle, uintptr_t flags, const char *name);


/**
* @brief
* @param self
* @param monitor
* @return void
*/
void
omrthread_monitor_lock(omrthread_t self, omrthread_monitor_t monitor);


/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_notify(omrthread_monitor_t monitor);


/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_notify_all(omrthread_monitor_t monitor);


/**
* @brief
* @param monitor
* @return uintptr_t
*/
uintptr_t
omrthread_monitor_num_waiting(omrthread_monitor_t monitor);


/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_try_enter(omrthread_monitor_t monitor);


/**
* @brief
* @param monitor
* @param threadId
* @return intptr_t
*/
intptr_t
omrthread_monitor_try_enter_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId);


/**
* @brief
* @param self
* @param monitor
* @return void
*/
void
omrthread_monitor_unlock(omrthread_t self, omrthread_monitor_t monitor);


/**
* @brief
* @param monitor
* @return intptr_t
*/
intptr_t
omrthread_monitor_wait(omrthread_monitor_t monitor);

/**
* @brief
* @param monitor
* @param millis
* @param nanos
* @return intptr_t
*/
intptr_t
omrthread_monitor_wait_abortable(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos);

/**
* @brief
* @param monitor
* @param millis
* @param nanos
* @return intptr_t
*/
intptr_t
omrthread_monitor_wait_interruptable(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos);


/**
* @brief
* @param monitor
* @param millis
* @param nanos
* @return intptr_t
*/
intptr_t
omrthread_monitor_wait_timed(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos);

/**
* @brief
* @param nanos
* @return intptr_t
*/
intptr_t
omrthread_nanosleep(int64_t nanos);


/**
* @brief
* @param void
* @return intptr_t
*/
intptr_t
omrthread_nanosleep_supported(void);


/**
* @brief
* @param wakeTime
* @return intptr_t
*/
intptr_t
omrthread_nanosleep_to(int64_t wakeTime);


/**
* @brief
* @param millis
* @param nanos
* @return intptr_t
*/
intptr_t
omrthread_park(int64_t millis, intptr_t nanos);


/**
* @brief
* @param thread
* @return void
*/
void
omrthread_priority_interrupt(omrthread_t thread);


/**
* @brief
* @param thread
* @return uintptr_t
*/
uintptr_t
omrthread_priority_interrupted(omrthread_t thread);


#if (defined(OMR_THR_TRACING))
/**
* @brief
* @param void
* @return void
*/
void
omrthread_reset_tracing(void);
#endif /* OMR_THR_TRACING */


/**
* @brief
* @param thread
* @return intptr_t
*/
intptr_t
omrthread_resume(omrthread_t thread);


/**
* @brief
* @param void
* @return omrthread_t
*/
omrthread_t
omrthread_self(void);


/**
* @brief
* @param thread
* @param name
* @return intptr_t
*/
intptr_t
omrthread_set_name(omrthread_t thread, const char *name);


/**
* @brief
* @param thread
* @param priority
* @return intptr_t
*/
intptr_t
omrthread_set_priority(omrthread_t thread, uintptr_t priority);


/**
* @brief
* @return intptr_t
*/
intptr_t
omrthread_set_priority_spread(void);


/**
* @brief
* @param millis
* @return intptr_t
*/
intptr_t
omrthread_sleep(int64_t millis);


/**
* @brief
* @param millis
* @param nanos
* @return intptr_t
*/
intptr_t
omrthread_sleep_interruptable(int64_t millis, intptr_t nanos);


/**
* @brief
* @param void
* @return void
*/
void
omrthread_suspend(void);


/**
* @brief
* @param handle
* @return intptr_t
*/
intptr_t
omrthread_tls_alloc(omrthread_tls_key_t *handle);


/**
* @brief
* @param handle
* @param finalizer
* @return intptr_t
*/
intptr_t
omrthread_tls_alloc_with_finalizer(omrthread_tls_key_t *handle, omrthread_tls_finalizer_t finalizer);


/**
* @brief
* @param key
* @return intptr_t
*/
intptr_t
omrthread_tls_free(omrthread_tls_key_t key);


/**
* @brief
* @param thread
* @param key
* @param value
* @return intptr_t
*/
intptr_t
omrthread_tls_set(omrthread_t thread, omrthread_tls_key_t key, void *value);


/**
* @brief
* @param thread
* @return void
*/
void
omrthread_unpark(omrthread_t thread);


/**
* @brief
* @param void
* @return void
*/
void
omrthread_yield(void);


/**
* @brief Yield the processor
* @param sequencialYields, number of yields in a row that have been made
* @return void
*/
void
omrthread_yield_new(uintptr_t sequentialYields);


/**
 * Returns whether or not realtime thread scheduling is being used
 * @return TRUE if realtime scheduling is in use, FALSE otherwise
 */
BOOLEAN
omrthread_lib_use_realtime_scheduling(void);

/**
 * @brief Set the category of the thread.
 * @param thread the threadid whose category needs to be set.
 * @param category the category to be set to.
 * @param type type of category creation
 * @return 0 on success, -1 on failure.
 */
intptr_t
omrthread_set_category(omrthread_t thread, uintptr_t category, uintptr_t type);

/**
 * @brief Get the category of the thread.
 * @param thread the threadid whose category is needed.
 * @return thread category.
 */
uintptr_t
omrthread_get_category(omrthread_t thread);

/* ---------------- thrprof.c ---------------- */

/**
* @brief
* @param enable
* @return void
*/
void
omrthread_enable_stack_usage(uintptr_t enable);


/**
 * @brief
 * @param thread
 * @return int64_t
 */
int64_t
omrthread_get_cpu_time(omrthread_t thread);


/**
 * @brief
 * @param thread
 * @param cpuTime
 * @return intptr_t
 */
intptr_t
omrthread_get_cpu_time_ex(omrthread_t thread, int64_t *cpuTime);


/**
 * Return the amount of CPU time used by the entire process in nanoseconds.
 * @param void
 * @return time in nanoseconds
 * @retval -1 not supported on this platform
 * @see omrthread_get_self_cpu_time, omrthread_get_user_time
 */
int64_t
omrthread_get_process_cpu_time(void);

/**
 * @brief
 * @param self
 * @return int64_t
 */
int64_t
omrthread_get_self_cpu_time(omrthread_t self);

/**
* @brief
* @param thread
* @return int64_t
*/
int64_t
omrthread_get_user_time(omrthread_t thread);

/**
 * @brief
 * @param self
 * @return int64_t
 */
int64_t
omrthread_get_self_user_time(omrthread_t self);

/**
* @brief
* @param Pointer to processTime struct
* @return intptr_t
*/
intptr_t
omrthread_get_process_times(omrthread_process_time_t *processTime);

/**
* @brief
* @param thread
* @return uintptr_t
*/
uintptr_t
omrthread_get_handle(omrthread_t thread);

/**
* @brief
* @param thread
* @param policy
* @param *priority
* @return intptr_t
*/
intptr_t omrthread_get_os_priority(omrthread_t thread, intptr_t *policy, intptr_t *priority);

/**
* @brief
* @param thread
* @return uintptr_t
*/
uintptr_t
omrthread_get_stack_size(omrthread_t thread);

/**
* @brief
* @param thread
* @return uintptr_t
*/
uintptr_t
omrthread_get_stack_usage(omrthread_t thread);

/**
 * @brief Return the CPU usage for the various thread categories
 * @param cpuUsage CPU usage details to be filled in.
 * @return 0 on success and -1 on failure.
 */
intptr_t
omrthread_get_jvm_cpu_usage_info(J9ThreadsCpuUsage *cpuUsage);

/**
 * Called by the signal handler in javadump.cpp to release any mutexes
 * held by omrthread_get_jvm_cpu_usage_info if the thread walk fails.
 * Can only be used to be called in a signal handler running in the
 * context of the same thread that locked the mutexes in the first place
 * for the locks to be successfully released.
 */
void
omrthread_get_jvm_cpu_usage_info_error_recovery(void);

/* ---------------- omrthreadattr.c ---------------- */

/**
 * @brief
 * @param attr
 * @return intptr_t
 */
intptr_t
omrthread_attr_init(omrthread_attr_t *attr);

/**
 * @brief
 * @param attr
 * @return intptr_t
 */
intptr_t
omrthread_attr_destroy(omrthread_attr_t *attr);

/**
 * @brief
 * @param attr
 * @param name
 * @return intptr_t
 */
intptr_t
omrthread_attr_set_name(omrthread_attr_t *attr, const char *name);

/**
 * @brief
 * @param attr
 * @param policy
 * @return intptr_t
 */
intptr_t
omrthread_attr_set_schedpolicy(omrthread_attr_t *attr, omrthread_schedpolicy_t policy);

/**
 * @brief
 * @param attr
 * @param priority
 * @return intptr_t
 */
intptr_t
omrthread_attr_set_priority(omrthread_attr_t *attr, omrthread_prio_t priority);

/**
 * Set detach state
 *
 * @param[in] attr
 * @param[in] detachstate the desired state (i.e., detached or joinable)
 * @return success or failure
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute struct.
 * @retval J9THREAD_ERR_INVALID_VALUE Failed to set the specified detachstate.
 */
intptr_t
omrthread_attr_set_detachstate(omrthread_attr_t *attr, omrthread_detachstate_t detachstate);

/**
 * @brief
 * @param attr
 * @param stacksize
 * @return intptr_t
 */
intptr_t
omrthread_attr_set_stacksize(omrthread_attr_t *attr, uintptr_t stacksize);

/**
 * Set the thread category
 *
 * @param[in] attr
 * @param[in] category
 * @retval J9THREAD_SUCCESS on success
 * @retval J9THREAD_ERR_INVALID_ATTR attr is an invalid attribute
 * @retval J9THREAD_ERR_INVALID_VALUE category is invalid
 */
intptr_t
omrthread_attr_set_category(omrthread_attr_t *attr, uint32_t category);

/* ---------------- omrthreaderror.c ---------------- */
/**
 * @brief
 * @param err
 * @return const char *
 */
const char *
omrthread_get_errordesc(intptr_t err);

/**
 * @brief
 * @return omrthread_os_errno_t
 */
omrthread_os_errno_t
omrthread_get_os_errno(void);

#if defined(J9ZOS390)
/**
 * @brief
 * @return omrthread_os_errno_t
 */
omrthread_os_errno_t
omrthread_get_os_errno2(void);
#endif /* J9ZOS390 */


/* -------------- omrthreadnuma.c ------------------- */
/* success code for trheadnuma API */
#define J9THREAD_NUMA_OK 					0
/* generic error code */
#define J9THREAD_NUMA_ERR 					-1
/* no CPUs found for the specified node so affinity is undefined */
#define J9THREAD_NUMA_ERR_NO_CPUS_FOR_NODE 	-2
/* in case affinity is not supported (or at least can't be queried) on the given platform */
#define J9THREAD_NUMA_ERR_AFFINITY_NOT_SUPPORTED 	-3
/*
 * when setting NUMA node affinity, include all specified nodes,
 * including those which are not in the default node set.
 */
#define J9THREAD_NUMA_OVERRIDE_DEFAULT_AFFINITY 0x0001

/**
 * @brief
 * @return uintptr_t
 */
uintptr_t
omrthread_numa_get_max_node(void);

intptr_t
omrthread_numa_set_node_affinity(omrthread_t thread, const uintptr_t *numaNodes, uintptr_t nodeCount, uint32_t flags);


/**
 * @brief
 * @param thread
 * @param numaNodes
 * @param nodeCount
 * @return intptr_t
 */
intptr_t
omrthread_numa_get_node_affinity(omrthread_t thread, uintptr_t *numaNodes, uintptr_t *nodeCount);

/**
 * Sets the NUMA enabled status.
 * This applies to the entire process.
 */
void
omrthread_numa_set_enabled(BOOLEAN enabled);

/* -------------- rasthrsup.c ------------------- */
/**
 * @brief
 * @return uintptr_t
 */
uintptr_t
omrthread_get_ras_tid(void);

/* -------------- threadhelp.cpp ------------------- */

/**
* @brief Pin a monitor (prevent its deflation)
* @param monitor the monitor to pin
* @param self the current omrthread_t
* @return void
*/
void
omrthread_monitor_pin(omrthread_monitor_t monitor, omrthread_t self);

/**
* @brief Unpin a monitor (allow its deflation)
* @param monitor the monitor to unpin
* @param self the current omrthread_t
* @return void
*/
void
omrthread_monitor_unpin(omrthread_monitor_t monitor, omrthread_t self);

/* forward struct definition */
struct J9ThreadLibrary;

#ifdef __cplusplus
}
#endif

#endif /* thread_api_h */

