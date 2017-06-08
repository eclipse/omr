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
 * @brief Threading and synchronization support
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "omrcfg.h"
#include "omrthread.h"
#include "threaddef.h"
#include "omrthreadattr.h"
#include "omrutilbase.h"
#include "ut_j9thr.h"

void omrthread_init(omrthread_library_t lib);
void omrthread_shutdown(void);

static omrthread_t threadAllocate(omrthread_library_t lib, int globalIsLocked);
static intptr_t threadCreate(omrthread_t *handle, const omrthread_attr_t *attr, uintptr_t suspend, omrthread_entrypoint_t entrypoint, void *entryarg, int globalIsLocked);
static intptr_t threadDestroy(omrthread_t thread, int globalAlreadyLocked);
static void threadFree(omrthread_t thread, int globalAlreadyLocked);
static WRAPPER_TYPE thread_wrapper(WRAPPER_ARG arg);
static void OMRNORETURN threadInternalExit(void);

static void threadEnqueue(omrthread_t *queue, omrthread_t thread);
static void threadDequeue(omrthread_t volatile *queue, omrthread_t thread);

static omrthread_monitor_pool_t allocate_monitor_pool(omrthread_library_t lib);
static void free_monitor_pools(void);
static intptr_t init_global_monitor(omrthread_library_t lib);

static omrthread_monitor_t monitor_allocate(omrthread_t self, intptr_t policy, intptr_t policyData);
static intptr_t monitor_alloc_and_init(omrthread_monitor_t *handle, uintptr_t flags, intptr_t policy, intptr_t policyData, const char *name);
static intptr_t monitor_init(omrthread_monitor_t monitor, uintptr_t flags, omrthread_library_t lib, const char *name);
static void monitor_free(omrthread_library_t lib, omrthread_monitor_t monitor);
static void monitor_free_nolock(omrthread_library_t lib, omrthread_t thread, omrthread_monitor_t monitor);
#if !defined(OMR_THR_THREE_TIER_LOCKING)
static intptr_t monitor_enter(omrthread_t self, omrthread_monitor_t monitor);
#endif /* !defined(OMR_THR_THREE_TIER_LOCKING) */
static intptr_t monitor_exit(omrthread_t self, omrthread_monitor_t monitor);
static intptr_t monitor_wait(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos, uintptr_t interruptible);
static intptr_t monitor_notify_one_or_all(omrthread_monitor_t monitor, int notifyall);
#if defined(OMR_THR_THREE_TIER_LOCKING)
static intptr_t monitor_enter_three_tier(omrthread_t self, omrthread_monitor_t monitor, BOOLEAN isAbortable);
static intptr_t monitor_wait_three_tier(omrthread_t self, omrthread_monitor_t monitor, int64_t millis, intptr_t nanos, uintptr_t interruptible);
static intptr_t monitor_notify_three_tier(omrthread_t self, omrthread_monitor_t monitor, int notifyall);
#endif /* OMR_THR_THREE_TIER_LOCKING */
static intptr_t monitor_wait_original(omrthread_t self, omrthread_monitor_t monitor, int64_t millis, intptr_t nanos, uintptr_t interruptible);
static intptr_t monitor_notify_original(omrthread_t self, omrthread_monitor_t monitor, int notifyall);

#if defined(OMR_THR_THREE_TIER_LOCKING)
static intptr_t init_spinCounts(omrthread_library_t lib);
static void unblock_spinlock_threads(omrthread_t self, omrthread_monitor_t monitor);
#endif /* OMR_THR_THREE_TIER_LOCKING */

static intptr_t init_threadParam(char *name, uintptr_t *pDefault);
static intptr_t init_spinParameters(omrthread_library_t lib);

static void threadInterrupt(omrthread_t thread, uintptr_t interruptFlag);
static void threadInterruptWake(omrthread_t thread, omrthread_monitor_t monitor);
static void threadNotify(omrthread_t threadToNotify);
static int32_t J9THREAD_PROC interruptServer(void *entryArg);
static intptr_t interrupt_waiting_thread(omrthread_t self, omrthread_t threadToInterrupt);
static void interrupt_blocked_thread(omrthread_t self, omrthread_t threadToInterrupt);

static intptr_t check_notified(omrthread_t self, omrthread_monitor_t monitor);
static uintptr_t monitor_maximum_wait_number(omrthread_monitor_t monitor);
static uintptr_t monitor_on_notify_all_wait_list(omrthread_t self, omrthread_monitor_t monitor);
static void monitor_notify_all_migration(omrthread_monitor_t monitor);

static intptr_t failedToSetAttr(intptr_t rc);
#if defined(OSX)
static intptr_t copyAttr(omrthread_attr_t *attrTo, const omrthread_attr_t *attrFrom);
#endif /* defined(OSX) */

static intptr_t fixupThreadAccounting(omrthread_t thread, uintptr_t type);
static void storeExitCpuUsage(omrthread_t thread);

static BOOLEAN fillInEmptyMemCategorySlot(const OMRMemCategorySet *const categorySet, OMRMemCategory *const category,
		const uint32_t startIndex, uint32_t *const filledSlotIndex);

static void threadEnableCpuMonitor(omrthread_t thread);

#if defined(OMR_THR_FORK_SUPPORT)
static void postForkResetThreads(omrthread_t self);
static void postForkResetLib(omrthread_t self);
static void postForkResetMonitors(omrthread_t self);
static void postForkResetRWMutexes(omrthread_t self);
#endif /* defined(OMR_THR_FORK_SUPPORT) */

#ifdef THREAD_ASSERTS
/**
 * Helper variable for asserts.
 * We use this to keep track of when the global lock is owned.
 * We don't want to do a re-entrant enter on the global lock
 */
omrthread_t global_lock_owner = UNOWNED;
#endif /* THREAD_ASSERTS */

#ifdef OMR_THR_THREE_TIER_LOCKING
#define ASSERT_MONITOR_UNOWNED_IF_NOT_3TIER(monitor) do {} while (0)
#else
#define ASSERT_MONITOR_UNOWNED_IF_NOT_3TIER(monitor) \
	do { \
		ASSERT(NULL == (monitor)->owner); \
		ASSERT(0    == (monitor)->count); \
	} while (0)
#endif

#ifdef OMR_ENV_DATA64
#if defined(AIXPPC)
/* See CMVC 87602 - Problems on AIX 5.2 with very long wait times */
#define BOUNDED_I64_TO_IDATA(longValue) ((longValue) > 0x7FFFFFFFLL ? (intptr_t)0x7FFFFFFF : (intptr_t)(longValue))
#else /* AIXPPC */
#define BOUNDED_I64_TO_IDATA(longValue) ((intptr_t)longValue)
#endif /* AIXPPC */
#else /* OMR_ENV_DATA64 */
#define BOUNDED_I64_TO_IDATA(longValue) ((longValue) > 0x7FFFFFFF ? 0x7FFFFFFF : (intptr_t)(longValue))
#endif /* OMR_ENV_DATA64 */

#define USE_CLOCK_NANOSLEEP 0

#ifndef ADJUST_TIMEOUT
#define ADJUST_TIMEOUT(millis, nanos) \
	  (((nanos) && ((millis) != ((intptr_t) (((uintptr_t)-1) >> 1)))) ? \
		 ((millis) + 1) : (millis))
#endif

#define MONITOR_WAIT_CONDITION(thread, monitor)  (thread)->condition

/*
 * Useful J9THREAD_FLAG_xxx masks
 */
#define J9THREAD_FLAGM_BLOCKED_ABORTABLE (J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_ABORTABLE)
#define J9THREAD_FLAGM_SLEEPING_TIMED (J9THREAD_FLAG_SLEEPING | J9THREAD_FLAG_TIMER_SET)
#define J9THREAD_FLAGM_SLEEPING_TIMED_INTERRUPTIBLE (J9THREAD_FLAGM_SLEEPING_TIMED | J9THREAD_FLAG_INTERRUPTABLE)
#define J9THREAD_FLAGM_PARKED_INTERRUPTIBLE (J9THREAD_FLAG_PARKED | J9THREAD_FLAG_INTERRUPTABLE)

#define J9THR_WAIT_INTERRUPTED(flags) (((flags) & J9THREAD_FLAG_INTERRUPTED) != 0)
#define J9THR_WAIT_PRI_INTERRUPTED(flags) (((flags) & (J9THREAD_FLAG_PRIORITY_INTERRUPTED | J9THREAD_FLAG_ABORTED)) != 0)

#if defined(WIN32) || !defined(OMR_NOTIFY_POLICY_CONTROL)
#define NOTIFY_WRAPPER(thread) J9OSCOND_NOTIFY_ALL((thread)->condition);
#else /* defined(WIN32) || !defined(OMR_NOTIFY_POLICY_CONTROL) */
#define NOTIFY_WRAPPER(thread) \
	do { \
		if (J9_ARE_ALL_BITS_SET((thread)->library->flags, J9THREAD_LIB_FLAG_NOTIFY_POLICY_BROADCAST)) { \
			J9OSCOND_NOTIFY_ALL((thread)->condition); \
		} else { \
			J9OSCOND_NOTIFY((thread)->condition); \
		} \
	} while (0)
#endif /* defined(WIN32) || !defined(OMR_NOTIFY_POLICY_CONTROL) */

/*
 * Thread Library
 */

#if !defined(WIN32)

/**
 * pthread TLS key destructor for self_ptr
 *
 * This will be called if the current pthread exits before it has been detached
 * from the thread library with omrthread_detach.
 *
 * Up to a maximum of OMR_MAX_KEY_DELETION_ATTEMPTS times, the TLS key will be
 * restored to allow other key destructors to run and use the thread library.
 *
 * Once the maximum number of attempts have been made, the key will no longer be
 * restored and the behaviour of any remaining key destructors that attempt to use
 * the thread library is undefined.
 *
 * @param[in] current_omr_thread the previous value of the TLS slot - the current omrthread_t
 */

#define OMR_MAX_KEY_DELETION_ATTEMPTS 10

static void
self_key_destructor(void *current_omr_thread)
{
	omrthread_t omrthread = current_omr_thread;
	uintptr_t attempts = omrthread->key_deletion_attempts;
	if (attempts < OMR_MAX_KEY_DELETION_ATTEMPTS) {
		omrthread->key_deletion_attempts = attempts + 1;
		TLS_SET(((omrthread_library_t)GLOBAL_DATA(default_library))->self_ptr, current_omr_thread);
	}
}

#undef OMR_MAX_KEY_DELETION_ATTEMPTS

#endif /* !WIN32 */

/**
 * Initialize a J9 threading library.
 *
 * @note This must only be called once.
 *
 * If any OS threads were created before calling this function, they must be attached using
 * omrthread_attach before accessing any J9 thread library functions.
 *
 * @param[in] lib pointer to the J9 thread library to be initialized (non-NULL)
 * @return The J9 thread library's initStatus will be set to 0 on success or
 * a negative value on failure.
 *
 * @see omrthread_attach, omrthread_shutdown
 */
void
omrthread_init(omrthread_library_t lib)
{
	ASSERT(lib);

	lib->spinlock = 0;
	lib->threadCount = 0;
	lib->globals = NULL;
#if defined(WIN32)
	lib->stack_usage = 0;
#endif /* WIN32 */
	lib->flags = 0;

	omrthread_mem_init(lib);

	/* set all TLS finalizers to NULL. This indicates that the key is unused */
	memset(lib->tls_finalizers, 0, sizeof(lib->tls_finalizers));

	STATIC_ASSERT(CALLER_LAST_INDEX <= MAX_CALLER_INDEX);

#if defined(WIN32)
	if (TLS_ALLOC(lib->self_ptr))
#else /* WIN32 */
	if (TLS_ALLOC_WITH_DESTRUCTOR(lib->self_ptr, self_key_destructor))
#endif /* WIN32 */
	{
		goto init_cleanup1;
	}

	lib->monitor_pool = allocate_monitor_pool(lib);
	if (lib->monitor_pool == NULL) {
		goto init_cleanup2;
	}

#if defined(OMR_THR_FORK_SUPPORT)
	lib->rwmutexPool = omrthread_rwmutex_init_pool(lib);
	if (lib->rwmutexPool == NULL) {
		goto init_cleanup3;
	}
#endif /* defined(OMR_THR_FORK_SUPPORT) */

	if (!J9OSMUTEX_INIT(lib->monitor_mutex)) {
		goto init_cleanup4;
	}
	if (!J9OSMUTEX_INIT(lib->tls_mutex)) {
		goto init_cleanup5;
	}
	if (!J9OSMUTEX_INIT(lib->global_mutex)) {
		goto init_cleanup6;
	}
	if (!J9OSMUTEX_INIT(lib->resourceUsageMutex)) {
		goto init_cleanup7;
	}

	lib->threadWalkMutexesHeld = 0;

	lib->thread_pool = pool_new(sizeof(J9Thread), 0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_THREADS, omrthread_mallocWrapper, omrthread_freeWrapper, lib);
	if (lib->thread_pool == NULL) {
		goto init_cleanup8;
	}

	lib->global_pool = pool_new(sizeof(J9ThreadGlobal), 0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_THREADS, omrthread_mallocWrapper, omrthread_freeWrapper, lib);
	if (lib->global_pool == NULL) {
		goto init_cleanup9;
	}

	if (init_spinParameters(lib)) {
		goto init_cleanup10;
	}

	if (init_global_monitor(lib)) {
		goto init_cleanup10;
	}

#ifdef OMR_THR_JLM
	lib->monitor_tracing_pool = NULL;
	lib->thread_tracing_pool = NULL;
	lib->gc_lock_tracing = NULL;
#endif

#if	(defined(WIN32) || defined(WIN64))
	lib->flags |= J9THREAD_LIB_FLAG_DESTROY_MUTEX_ON_MONITOR_FREE;
#endif

	if (omrthread_attr_init(&lib->systemThreadAttr) != J9THREAD_SUCCESS) {
		goto init_cleanup10;
	}

#if defined(OSX)
	if (KERN_SUCCESS != host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &lib->clockService)) {
		goto init_cleanup11;
	}
#endif /* defined(OSX) */

#if defined(OMR_PORT_NUMA_SUPPORT)
	/* Load the numa library from the dll */
	omrthread_numa_init(lib);
#endif /* OMR_PORT_NUMA_SUPPORT */

	memset(&lib->cumulativeThreadsInfo, 0, sizeof(lib->cumulativeThreadsInfo));
	lib->initStatus = 1;
	return;

#if defined(OSX)
/* Unused cleanup label for future error handling. */
init_cleanup12:		mach_port_deallocate(mach_task_self(), lib->clockService);
init_cleanup11:		omrthread_attr_destroy(&lib->systemThreadAttr);
#endif /* defined(OSX) */
init_cleanup10:		pool_kill(lib->global_pool);
init_cleanup9:		pool_kill(lib->thread_pool);
init_cleanup8:		J9OSMUTEX_DESTROY(lib->resourceUsageMutex);
init_cleanup7:		J9OSMUTEX_DESTROY(lib->global_mutex);
init_cleanup6:		J9OSMUTEX_DESTROY(lib->tls_mutex);
init_cleanup5:		J9OSMUTEX_DESTROY(lib->monitor_mutex);
#if defined(OMR_THR_FORK_SUPPORT)
init_cleanup4:		pool_kill(lib->rwmutexPool);
init_cleanup3:		free_monitor_pools();
#else /* defined(OMR_THR_FORK_SUPPORT) */
init_cleanup4:		free_monitor_pools();
#endif /* defined(OMR_THR_FORK_SUPPORT) */
init_cleanup2:		TLS_DESTROY(lib->self_ptr);
init_cleanup1:		lib->initStatus = -1;
}

/**
 * Look for an empty NULL slot in a J9MemoryCategorySet, and add a new category into it.
 *
 * @param[in] categorySet A J9MemoryCategorySet. Must be non-NULL.
 * @param[in] category The category to add to categorySet. Must be non-NULL.
 * @param[in] startIndex The first category slot to start searching from.
 * @param[out] filledSlotIndex If found, the empty slot where the new category is added. Must be non-NULL.
 * @return TRUE if an empty slot is found, FALSE if no empty slot is found.
 */
static BOOLEAN
fillInEmptyMemCategorySlot(const OMRMemCategorySet *const categorySet, OMRMemCategory *const category,
						   const uint32_t startIndex, uint32_t *const filledSlotIndex)
{
	BOOLEAN foundEmptySlot = FALSE;
	uint32_t i = 0;

	for (i = startIndex; i < categorySet->numberOfCategories; i++) {
		if (NULL == categorySet->categories[i]) {
			categorySet->categories[i] = category;
			*filledSlotIndex = i;
			foundEmptySlot = TRUE;
			break;
		}
	}
	return foundEmptySlot;
}

intptr_t
omrthread_lib_control(const char *key, uintptr_t value)
{
	intptr_t rc = -1;

	if (0 != value) {
#if defined(OMR_RAS_TDF_TRACE)
		/* return value of 0 is success */
		if (0 == strcmp(J9THREAD_LIB_CONTROL_TRACE_START, key)) {
			UtInterface *utIntf = (UtInterface *)value;
			UT_MODULE_LOADED(utIntf);
			Trc_THR_VMInitStages_Event1(NULL);
			omrthread_lib_set_flags(J9THREAD_LIB_FLAG_TRACING_ENABLED);
			rc = 0;
		} else if (0 == strcmp(J9THREAD_LIB_CONTROL_TRACE_STOP, key)) {
			UtInterface *utIntf = (UtInterface *)value;
			utIntf->module->TraceTerm(NULL, &UT_MODULE_INFO);
			rc = 0;
		}
#endif
		if (0 == strcmp(J9THREAD_LIB_CONTROL_GET_MEM_CATEGORIES, key)) {
			omrthread_library_t lib = GLOBAL_DATA(default_library);
			OMRMemCategorySet *categorySet = (OMRMemCategorySet *)value;
			uint32_t filledSlotIndex = 0;

			/* We can drop categories into any blank slot. The port library will
			 * reorganise them when they are properly registered.
			 */
			if (fillInEmptyMemCategorySlot(categorySet, &lib->threadLibraryCategory, 0, &filledSlotIndex)) {
				if (fillInEmptyMemCategorySlot(categorySet, &lib->nativeStackCategory, filledSlotIndex + 1, &filledSlotIndex)) {
#if defined(OMR_THR_FORK_SUPPORT)
					if (fillInEmptyMemCategorySlot(categorySet, &lib->mutexCategory, filledSlotIndex + 1, &filledSlotIndex)) {
						if (fillInEmptyMemCategorySlot(categorySet, &lib->condvarCategory, filledSlotIndex + 1, &filledSlotIndex)) {
							rc = 0;
						}
					}
#else /* defined(OMR_THR_FORK_SUPPORT) */
					rc = 0;
#endif /* defined(OMR_THR_FORK_SUPPORT) */
				}
			}
		}
	}

#if defined(LINUX) || defined(OSX)
	if (0 == strcmp(J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING, key)) {
		if ((J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING_DISABLED == value)
		 || (J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING_ENABLED == value)
		) {
			omrthread_library_t lib = GLOBAL_DATA(default_library);
			if (value != (lib->flags & J9THREAD_LIB_FLAG_REALTIME_SCHEDULING_ENABLED)) {
				pool_state state;
				omrthread_t walkThread = NULL;

				if (J9THREAD_LIB_CONTROL_USE_REALTIME_SCHEDULING_ENABLED == value) {
					omrthread_lib_set_flags(J9THREAD_LIB_FLAG_REALTIME_SCHEDULING_ENABLED);
				} else {
					omrthread_lib_clear_flags(J9THREAD_LIB_FLAG_REALTIME_SCHEDULING_ENABLED);
				}

				/* re-initialize the priority map */
				rc = initialize_priority_map();

				GLOBAL_LOCK_SIMPLE(lib);

				/* Walk the list of omrthread's and make them use the
				 * new priority map by resetting their priorities.
				 */
				walkThread = pool_startDo(lib->thread_pool, &state);
				while ((0 == rc) && (NULL != walkThread)) {
					omrthread_prio_t priority = walkThread->priority;
					/* the omrthread_prio_t will be mapped to a different native priority */
					rc = omrthread_set_priority(walkThread, priority);
					walkThread = pool_nextDo(&state);
				}

				GLOBAL_UNLOCK_SIMPLE(lib);
			} else {
				/* no need to do anything if the value hasn't changed */
				rc = 0;
			}
		}
	}
#endif /* defined(LINUX) || defined(OSX) */

	return rc;
}

BOOLEAN
omrthread_lib_use_realtime_scheduling(void)
{
#if defined(LINUX) || defined(OSX)
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	return J9_ARE_ALL_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_REALTIME_SCHEDULING_ENABLED);
#else /* defined(LINUX) || defined(OSX) */
	return FALSE;
#endif /* defined(LINUX) || defined(OSX) */
}

/**
 * Initialize the mutex used to synchronize access
 * to thread library global data.
 *
 * @param[in] lib pointer to the thread library
 * @return 0 on success or negative value on failure
 *
 */
static intptr_t
init_global_monitor(omrthread_library_t lib)
{
	omrthread_monitor_pool_t pool = lib->monitor_pool;
	omrthread_monitor_t monitor = pool->next_free;
	static char name[] = "Thread global";

	ASSERT(monitor);
	pool->next_free = (omrthread_monitor_t)monitor->owner;

	if (monitor_init(monitor, 0, lib, name) != 0) {
		return -1;
	}
	if (!J9OSMUTEX_INIT(monitor->mutex)) {
		return -1;
	}

	lib->globalMonitor = monitor;

	return 0;
}



/**
 * Initialize one thread parameter
 * make it accessible to the outside world via the
 * omrthread globals.
 *
 */
static intptr_t
init_threadParam(char *name, uintptr_t *pDefault)
{
	uintptr_t *p;

	ASSERT(name);
	ASSERT(pDefault);
	ASSERT(0 != *pDefault);

	p = omrthread_global(name);
	if (NULL == p) {
		return -1;
	}
	ASSERT(0 == *p);
	*p = (uintptr_t)pDefault;

	return 0;
}


#if (defined(OMR_THR_THREE_TIER_LOCKING))
/**
 * Initialize default spin counts and make them
 * accessible to the outside world via the
 * omrthread globals.
 *
 * Must be called after globals are initialized
 *
 * These numbers are taken from the GC spinlock.
 *
 * N.B. Every spinCount must be > 0!
 *
 * @param[in] lib omrthread library
 * @see omrthread_init, omrthread_global, omrthread_spinlock_acquire
 *
 */
static intptr_t
init_spinCounts(omrthread_library_t lib)
{
	ASSERT(lib);

	lib->defaultMonitorSpinCount1 = 256;
	lib->defaultMonitorSpinCount2 = 32;
#if defined(J9ZOS390)
	/*
	 * z/OS has exponentially increasing yield times
	 * if you call it back to back within a short period
	 * of time.
	 */
	lib->defaultMonitorSpinCount3 = 1;
#else
	lib->defaultMonitorSpinCount3 = 45;
#endif

	/* Set up the globals so anyone can change the counts */
	if (init_threadParam("defaultMonitorSpinCount1", &lib->defaultMonitorSpinCount1)) {
		return -1;
	}
	if (init_threadParam("defaultMonitorSpinCount2", &lib->defaultMonitorSpinCount2)) {
		return -1;
	}
	if (init_threadParam("defaultMonitorSpinCount3", &lib->defaultMonitorSpinCount3)) {
		return -1;
	}

	ASSERT(lib->defaultMonitorSpinCount1 != 0);
	ASSERT(lib->defaultMonitorSpinCount2 != 0);
	ASSERT(lib->defaultMonitorSpinCount3 != 0);

	return 0;

}
#endif /* OMR_THR_THREE_TIER_LOCKING */

/**
 * Initialize spin parameters
 *
 * Must be called after globals are initialized
 *
 * These numbers are taken from the GC spinlock.
 *
 * N.B. Every spinCount must be > 0!
 *
 * @param[in] lib omrthread library
 * @see omrthread_init, omrthread_global, omrthread_spinlock_acquire
 *
 */
static intptr_t
init_spinParameters(omrthread_library_t lib)
{
	ASSERT(lib);

#if defined(OMR_THR_ADAPTIVE_SPIN)
	lib->adaptSpinHoldtime = 0;
	if (init_threadParam("adaptSpinHoldtime", &lib->adaptSpinHoldtime)) {
		return -1;
	}

	lib->adaptSpinSlowPercent = 0;
	if (init_threadParam("adaptSpinSlowPercent", &lib->adaptSpinSlowPercent)) {
		return -1;
	}

	lib->adaptSpinSampleThreshold = 0;
	if (init_threadParam("adaptSpinSampleThreshold", &lib->adaptSpinSampleThreshold)) {
		return -1;
	}

	lib->adaptSpinSampleStopCount = 0;
	if (init_threadParam("adaptSpinSampleStopCount", &lib->adaptSpinSampleStopCount)) {
		return -1;
	}

	lib->adaptSpinSampleCountStopRatio = 0;
	if (init_threadParam("adaptSpinSampleCountStopRatio", &lib->adaptSpinSampleCountStopRatio)) {
		return -1;
	}
#endif

#if (defined(OMR_THR_YIELD_ALG))
	lib->yieldAlgorithm = 0;
	if (init_threadParam("yieldAlgorithm", &lib->yieldAlgorithm)) {
		return -1;
	}
	lib->yieldUsleepMultiplier = 0;
	if (init_threadParam("yieldUsleepMultiplier", &lib->yieldUsleepMultiplier)) {
		return -1;
	}
#endif

#if defined(OMR_THR_THREE_TIER_LOCKING)
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
  	lib->maxSpinThreads = OMRTHREAD_MINIMUM_SPIN_THREADS;
  	if (init_threadParam("maxSpinThreads", &lib->maxSpinThreads)) {
  		return -1;
  	}

  	lib->maxWakeThreads = OMRTHREAD_MINIMUM_WAKE_THREADS;
  	if (init_threadParam("maxWakeThreads", &lib->maxWakeThreads)) {
  		return -1;
  	}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */

	return init_spinCounts(lib);

#else
	return 0;
#endif
}

/**
 * Shut down the J9 threading library associated with the current thread.
 *
 * @return none
 *
 * @see omrthread_init
 */
void
omrthread_shutdown(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	ASSERT(lib);
#if defined(OMR_PORT_NUMA_SUPPORT)
	omrthread_numa_shutdown(lib);
#endif /* OMR_PORT_NUMA_SUPPORT */
	omrthread_attr_destroy(&lib->systemThreadAttr);
	J9OSMUTEX_DESTROY(lib->tls_mutex);
	J9OSMUTEX_DESTROY(lib->monitor_mutex);
	J9OSMUTEX_DESTROY(lib->global_mutex);
	J9OSMUTEX_DESTROY(lib->resourceUsageMutex);
	pool_kill(lib->global_pool);
	lib->global_pool = 0;

	free_monitor_pools();

	TLS_DESTROY(lib->self_ptr);

	pool_kill(lib->thread_pool);
	lib->thread_pool = 0;
#if defined(OMR_THR_FORK_SUPPORT)
	pool_kill(lib->rwmutexPool);
#endif /* defined(OMR_THR_FORK_SUPPORT) */
#if defined(OSX)
	mach_port_deallocate(mach_task_self(), lib->clockService);
#endif /* defined(OSX) */
}



/**
 * Acquire the threading library's global lock.
 *
 * @note This must not be called recursively by a thread that already owns the global lock.
 * @param[in] self omrthread_t for the current thread
 * @return none
 *
 * @see omrthread_lib_unlock
 */
void
omrthread_lib_lock(omrthread_t self)
{
	ASSERT(self);
	GLOBAL_LOCK(self, CALLER_GLOBAL_LOCK);
}


/**
 * Attempt to enter the threading library's global lock without blocking.
 *
 * @param[in] self omrthread_t for the current thread
 * @return zero on success, non-zero on failure
 */
intptr_t
omrthread_lib_try_lock(omrthread_t self)
{
#ifdef THREAD_ASSERTS
	intptr_t retVal;

	ASSERT(self);
	retVal = GLOBAL_TRY_LOCK(self, CALLER_GLOBAL_LOCK);
	if (retVal == 0) {
		ASSERT(UNOWNED == global_lock_owner);
		global_lock_owner = self;
	}
	return retVal;
#else
	ASSERT(self);
	return GLOBAL_TRY_LOCK(self, CALLER_GLOBAL_LOCK);
#endif
}

/**
 * Release the threading library's global lock.
 *
 * @note This must be called only by the thread that currently has the global lock locked.
 * @param[in] self omrthread_t for the current thread
 * @return none
 *
 * @see omrthread_lib_lock
 */
void
omrthread_lib_unlock(omrthread_t self)
{
	ASSERT(self);
	GLOBAL_UNLOCK(self);
}

#if defined(OMR_THR_FORK_SUPPORT)

void
omrthread_lib_preFork(void)
{
	omrthread_t self = MACRO_SELF();
	omrthread_library_t lib = NULL;
	if (NULL == self) {
		fprintf(stderr, "Can't acquire locks from an unattached thread.\n");
		abort();
	}

	lib = self->library;
	J9OSMUTEX_ENTER(lib->monitor_mutex);
	J9OSMUTEX_ENTER(lib->tls_mutex);
	J9OSMUTEX_ENTER(lib->global_mutex);
	J9OSMUTEX_ENTER(lib->resourceUsageMutex);
}

void
omrthread_lib_postForkParent(void)
{
	omrthread_t self = MACRO_SELF();
	omrthread_library_t lib = NULL;
	if (NULL == self) {
		fprintf(stderr, "Can't release locks from an unattached thread.\n");
		abort();
	}

	lib = self->library;
	J9OSMUTEX_EXIT(lib->resourceUsageMutex);
	J9OSMUTEX_EXIT(lib->global_mutex);
	J9OSMUTEX_EXIT(lib->tls_mutex);
	J9OSMUTEX_EXIT(lib->monitor_mutex);
}

void
omrthread_lib_postForkChild(void)
{
	omrthread_t self = MACRO_SELF();
	omrthread_library_t lib = NULL;
	if (NULL == self) {
		fprintf(stderr, "Can't release locks from an unattached thread.\n");
		abort();
	}

	lib = self->library;
	postForkResetLib(self);
	postForkResetMonitors(self);
	postForkResetThreads(self);
	postForkResetRWMutexes(self);

	J9OSMUTEX_EXIT(lib->resourceUsageMutex);
	J9OSMUTEX_EXIT(lib->global_mutex);
	J9OSMUTEX_EXIT(lib->tls_mutex);
	J9OSMUTEX_EXIT(lib->monitor_mutex);
}

/**
 * To reset the omrthread's, destroy their mutexes and conds. For the fork survivor thread,
 * fix the tid and handle and recreate the mutex and cond. Otherwise, free J9Thread.tracing,
 * flush the destroyed monitor lists back into the pool, and tls_finalize() the tls.
 *
 * Note that the thread mutex and cond are reallocated and initialized without freeing the
 * old ones, since if it allocates the same piece of memory, it fails with mutex already
 * initialized.
 *
 * @pre There is a pre-condition that the monitors have been reset first with postForkResetMonitors
 * in order for omrthread_tls_finalize to use mutexes. A user-provided tls_finalize callback
 * may fail post-fork if it uses mutexes/cond vars the thread library does not know about.
 * In case it uses omrthread_monitors, they must be reset first.
 */
static void
postForkResetThreads(omrthread_t self)
{
	omrthread_library_t lib = self->library;
	omrthread_t threadIterator = NULL;
	J9PoolState threadPoolState;

	threadIterator = (omrthread_t)pool_startDo(lib->thread_pool, &threadPoolState);
	while (NULL != threadIterator) {
		if (threadIterator == self) {
			/* cond/mutex reinitialized without freeing the old one. */
			J9OSCOND_INIT(threadIterator->condition);
			J9OSMUTEX_INIT(threadIterator->mutex);
			threadIterator->tid = omrthread_get_ras_tid();
#if defined(WIN32)
#error "The implementation of postForkResetThreads() is not complete in WIN32."
#endif /* defined(WIN32) */
			threadIterator->handle = THREAD_SELF();
		} else {
			omrthread_tls_finalizeNoLock(threadIterator);

			/* Flush the thread local list of destroyed monitors of the thread that no longer exists to the global list. */
			if (NULL != threadIterator->destroyed_monitor_head) {
				threadIterator->destroyed_monitor_tail->owner = (omrthread_t)lib->monitor_pool->next_free;
				lib->monitor_pool->next_free = threadIterator->destroyed_monitor_head;
			}
			/* Free the thread. */
			threadFree(threadIterator, GLOBAL_IS_LOCKED);
		}

		threadIterator = (omrthread_t)pool_nextDo(&threadPoolState);
	}
}

/**
 * Reset the omrthread_library's mutexes, the global lock owner, and the dead threads info.
 */
static void
postForkResetLib(omrthread_t self)
{
	omrthread_library_t lib = self->library;
	memset(&lib->cumulativeThreadsInfo, 0, sizeof(lib->cumulativeThreadsInfo));
#ifdef THREAD_ASSERTS
	if (self != global_lock_owner) {
		global_lock_owner = NULL;
	}
#endif /* THREAD_ASSERTS */
}

/**
 * For each monitor in the pool, destroy and reinit the mutex if it is allocated,
 * clear the waiting, blocking, and notifyAll thread list (with an assert if its the
 * fork survivor), set the owner to null if it is not the fork survivor, and reset
 * the spinLockState. If its not owned by the fork survivor, reset the owner,
 * pinCount, and sampleCounter as well.
 *
 * Note that the mutex is reallocated and initialized without freeing the old ones,
 * since if it allocates the same piece of memory, it fails with mutex already
 * initialized.
 */
static void
postForkResetMonitors(omrthread_t self)
{
	omrthread_library_t lib = self->library;
	omrthread_monitor_pool_t threadMonitorPool = NULL;

	threadMonitorPool = lib->monitor_pool;
	while (NULL != threadMonitorPool) {
		uintptr_t i;
		omrthread_monitor_t entry = &threadMonitorPool->entries[0];
		for (i = 0; i < MONITOR_POOL_SIZE - 1; i++, entry++) {
			if (entry->flags != J9THREAD_MONITOR_MUTEX_UNINITIALIZED) {
				J9OSMUTEX_INIT(entry->mutex);
			}
			if (FREE_TAG != entry->count) {
				if (self != entry->owner) {
					entry->owner = NULL;
#if defined(OMR_THR_ADAPTIVE_SPIN)
					entry->sampleCounter = 0;
#endif /* defined(OMR_THR_ADAPTIVE_SPIN) */
					entry->pinCount = 0;
#if defined(OMR_THR_THREE_TIER_LOCKING)
					entry->spinlockState = J9THREAD_MONITOR_SPINLOCK_UNOWNED;
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
					entry->spinThreads = 0;
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
				} else {
#if defined(OMR_THR_THREE_TIER_LOCKING)
					if (J9THREAD_MONITOR_SPINLOCK_EXCEEDED == entry->spinlockState) {
						entry->spinlockState = J9THREAD_MONITOR_SPINLOCK_OWNED;
					}
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
				}

#if defined(THREAD_ASSERTS)
				{
					omrthread_t threadIterator = NULL;
#if defined(OMR_THR_THREE_TIER_LOCKING)
					threadIterator = entry->blocking;
					while (NULL != threadIterator) {
						ASSERT(threadIterator != self);
						threadIterator = threadIterator->next;
					}
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
					threadIterator = entry->waiting;
					while (NULL != threadIterator) {
						ASSERT(threadIterator != self);
						threadIterator = threadIterator->next;
					}
					threadIterator = entry->notifyAllWaiting;
					while (NULL != threadIterator) {
						ASSERT(threadIterator != self);
						threadIterator = threadIterator->next;
					}
				}
#endif /* defined(THREAD_ASSERTS) */
#if defined(OMR_THR_THREE_TIER_LOCKING)
				entry->blocking = NULL;
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */
				entry->waiting = NULL;
				entry->notifyAllWaiting = NULL;
			}
		}
		threadMonitorPool = threadMonitorPool->next;
	}
}

static void
postForkResetRWMutexes(omrthread_t self)
{
	omrthread_library_t lib = self->library;
	omrthread_rwmutex_t rwmutexIterator = NULL;
	J9PoolState rwmutexPoolState;

	rwmutexIterator = (omrthread_rwmutex_t)pool_startDo(lib->rwmutexPool, &rwmutexPoolState);
	while (NULL != rwmutexIterator) {
		omrthread_rwmutex_reset(rwmutexIterator, self);
		rwmutexIterator = (omrthread_rwmutex_t)pool_nextDo(&rwmutexPoolState);
	}
}

#endif /* defined(OMR_THR_FORK_SUPPORT) */

/**
 * Get threading library global flags.
 *
 * Returns the flags for the threading library associated with the current thread.
 *
 * @note: assumes caller has global lock
 *
 * @see omrthread_lib_clear_flags, omrthread_lib_set_flags, omrthread_lib_lock
 * @return current flags value
 */
uintptr_t
omrthread_lib_get_flags(void)
{
	omrthread_t self;
	self = MACRO_SELF();

	ASSERT(self);
	ASSERT(self->library);

	return self->library->flags;
}

/**
 * Set threading library global flags.
 *
 * Sets the flags for the threading library associated with the current thread.
 *
 * @param[in] flags flags to be set (bit vector: 1 means set the flag, 0 means ignore)
 * @return old flags values
 * @see omrthread_lib_clear_flags, omrthread_lib_get_flags
 *
 */
uintptr_t
omrthread_lib_set_flags(uintptr_t flags)
{
	omrthread_t self;
	uintptr_t oldFlags;
	self = MACRO_SELF();

	ASSERT(self);
	ASSERT(self->library);

	GLOBAL_LOCK(self, CALLER_LIB_SET_FLAGS);
	oldFlags = self->library->flags;
	self->library->flags |= flags;
	GLOBAL_UNLOCK(self);

	return oldFlags;

}

/**
 * Clear specified threading library global flags.
 *
 * @see omrthread_lib_set_flags, omrthread_lib_get_flags
 * @param[in] flags flags to be cleared (bit vector: 1 means clear the flag, 0 means ignore)
 * @return old flags values
 */
uintptr_t
omrthread_lib_clear_flags(uintptr_t flags)
{
	omrthread_t self;
	uintptr_t oldFlags;
	self = MACRO_SELF();

	ASSERT(self);
	ASSERT(self->library);

	GLOBAL_LOCK(self, CALLER_LIB_CLEAR_FLAGS);
	oldFlags = self->library->flags;
	self->library->flags &= ~flags;
	GLOBAL_UNLOCK(self);

	return oldFlags;
}


/**
 * Fetch or create a 'named global'.
 *
 * Return a pointer to the data associated with a named global with the specified name.<br>
 * A new named global is created if a named global with the specified name can't be found.
 *
 * @param[in] name name of named global to read/create
 * @return a pointer to a uintptr_t associated with name<br>
 * 0 on failure.
 *
 */
uintptr_t *
omrthread_global(char *name)
{
	J9ThreadGlobal *global;
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	J9OSMUTEX_ENTER(lib->global_mutex);

	global = lib->globals;

	while (global) {
		if (strcmp(global->name, name) == 0) {
			J9OSMUTEX_EXIT(lib->global_mutex);
			return &global->data;
		}
		global = global->next;
	}

	/*
	 * If we got here, we couldn't find it, therefore
	 * we will create a new one
	 */

	global = pool_newElement(lib->global_pool);
	if (global == NULL) {
		J9OSMUTEX_EXIT(lib->global_mutex);
		return NULL;
	}

	global->next = lib->globals;
	global->name = name;
	global->data = 0;
	lib->globals = global;

	J9OSMUTEX_EXIT(lib->global_mutex);

	return &global->data;
}


/**
 * Returns a pointer to the thread global monitor.
 *
 * @return omrthread_monitor_t
 */
omrthread_monitor_t
omrthread_global_monitor(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	return lib->globalMonitor;
}


/*
 * Threads
 */

/**
 * Attach an OS thread to the threading library.
 *
 * Create a new omrthread_t to represent the existing OS thread.
 * Attaching a thread is required when a thread was created
 * outside of the J9 threading library wants to use any of the
 * J9 threading library functionality.
 *
 * If the OS thread is already attached, handle is set to point
 * to the existing omrthread_t.
 *
 * @param[out] handle pointer to a omrthread_t to be set (will be ignored if null)
 * @return  0 on success or negative value on failure
 *
 * @see omrthread_detach
 */
intptr_t
omrthread_attach(omrthread_t *handle)
{
	omrthread_t thread;
	intptr_t retVal = J9THREAD_SUCCESS;

	if (init_thread_library()) {
		return J9THREAD_ERR;
	}

	thread = MACRO_SELF();
	if (NULL != thread) {
		if (handle) {
			*handle = thread;
		}
		THREAD_LOCK(thread, CALLER_ATTACH);
		thread->attachcount++;
		THREAD_UNLOCK(thread);
		return J9THREAD_SUCCESS;
	}

	retVal = omrthread_attach_ex(handle, J9THREAD_ATTR_DEFAULT);

	return -retVal;
}

/*
 * See thread_api.h for description
 */
intptr_t
omrthread_attach_ex(omrthread_t *handle, omrthread_attr_t *attr)
{
	intptr_t retVal = J9THREAD_SUCCESS;
	omrthread_t thread;
	omrthread_library_t lib;

	if (init_thread_library()) {
		goto cleanup0;
	}

	thread = MACRO_SELF();
	if (NULL != thread) {
		if (handle) {
			*handle = thread;
		}
		THREAD_LOCK(thread, CALLER_ATTACH);
		thread->attachcount++;
		THREAD_UNLOCK(thread);
		return J9THREAD_SUCCESS;
	}

	lib = GLOBAL_DATA(default_library);
	thread = threadAllocate(lib, GLOBAL_NOT_LOCKED);
	if (!thread) {
		retVal = J9THREAD_ERR_CANT_ALLOCATE_J9THREAD_T;
		goto cleanup0;
	}

	ASSERT(thread->library == lib);
	thread->attachcount = 1;
	thread->priority = J9THREAD_PRIORITY_NORMAL;
	thread->flags = J9THREAD_FLAG_ATTACHED;
	/* Note an attached thread can never be omrthread-joinable */
	thread->lockedmonitorcount = 0;

	if (!J9OSCOND_INIT(thread->condition)) {
		retVal = J9THREAD_ERR_CANT_INIT_CONDITION;
		goto cleanup1;
	}

	if (!J9OSMUTEX_INIT(thread->mutex)) {
		retVal = J9THREAD_ERR_CANT_INIT_MUTEX;
		goto cleanup2;
	}

#if defined(WIN32) && !defined(BREW)
	DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &thread->handle, 0, TRUE, DUPLICATE_SAME_ACCESS);
#else
	thread->handle = THREAD_SELF();
#endif

	initialize_thread_priority(thread);

	TLS_SET(lib->self_ptr, thread);

	thread->tid = omrthread_get_ras_tid();
	thread->waitNumber = 0;

#if defined(OMR_PORT_NUMA_SUPPORT)
	{
		/* Initialize the thread->numaAffinity field */
		uintptr_t affinity[J9THREAD_MAX_NUMA_NODE];
		uintptr_t count = J9THREAD_MAX_NUMA_NODE;

		memset(&thread->numaAffinity, 0x0, sizeof(thread->numaAffinity));
		memset(affinity, 0x0, sizeof(affinity));

		if (0 == omrthread_numa_get_node_affinity(thread, affinity, &count)) {
			uintptr_t i = 0;
			for (i = 0; i < count; i++) {
				uintptr_t node = affinity[i];

				omrthread_add_node_number_to_affinity_cache(thread, node);
			}
		}
	}
#endif /* OMR_PORT_NUMA_SUPPORT */

	ENABLE_OS_THREAD_STATS(thread);
	/* Set the thread category to what has been passed in. If no attr has been passed in, then set to
	 * J9THREAD_CATEGORY_SYSTEM_THREAD. Since a number of OMR APIs call omrthread_attach, having
	 * J9THREAD_CATEGORY_SYSTEM_THREAD as the default, means that these APIs can call with a NULL attribute
	 * and it will automatically get associated to the right category.
	 * Ignore errors, the cpu monitor should not cause thread attach failure.
	 */
	if (!attr) {
		omrthread_set_category(thread, J9THREAD_CATEGORY_SYSTEM_THREAD, J9THREAD_TYPE_SET_ATTACH);
	} else {
		omrthread_set_category(thread, (*attr)->category, J9THREAD_TYPE_SET_ATTACH);
	}

	threadEnableCpuMonitor(thread);

	if (handle) {
		*handle = thread;
	}
	return J9THREAD_SUCCESS;

/* failure points */
cleanup2:	J9OSCOND_DESTROY(thread->condition);
cleanup1:	threadFree(thread, GLOBAL_NOT_LOCKED);
cleanup0:	return retVal;
}

/**
 * Store the cpu usage info of thread that is either exiting or being detached
 * into the global cumulativeThreadsInfo structure.
 */
static void
storeExitCpuUsage(omrthread_t thread)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	J9ThreadsCpuUsage *cumulativeInfo = &lib->cumulativeThreadsInfo;
	int64_t threadCpuTime = 0;

	/* If -XX:-EnableCPUMonitor has been set, this function is a no-op */
	if (J9_ARE_NO_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_ENABLE_CPU_MONITOR)) {
		return;
	}

	J9OSMUTEX_ENTER(lib->resourceUsageMutex);

	THREAD_LOCK(thread, CALLER_STORE_EXIT_CPU_USAGE);
	/* No longer safe for querying cpu usage for this thread */
	thread->flags &= ~J9THREAD_FLAG_CPU_SAMPLING_ENABLED;
	THREAD_UNLOCK(thread);

	if (J9_ARE_ALL_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_ENABLE_CPU_MONITOR)) {
		/* Store dying thread's CPU data */
		threadCpuTime = omrthread_get_cpu_time(thread);
		if (threadCpuTime > 0) {
			threadCpuTime /= 1000;
			/* We only need the time that the cpu usage is unaccounted for */
			threadCpuTime -= thread->lastCategorySwitchTime;
			if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD)) {
				cumulativeInfo->resourceMonitorCpuTime += threadCpuTime;
			} else if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_SYSTEM_THREAD)) {
				cumulativeInfo->systemJvmCpuTime += threadCpuTime;
				if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_SYSTEM_GC_THREAD)) {
					cumulativeInfo->gcCpuTime += threadCpuTime;
				} else if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_SYSTEM_JIT_THREAD)) {
					cumulativeInfo->jitCpuTime += threadCpuTime;
				}
			} else {
				cumulativeInfo->applicationCpuTime += threadCpuTime;
				switch (thread->effective_category) {
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_1:
					cumulativeInfo->applicationUserCpuTime[0] += threadCpuTime;
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_2:
					cumulativeInfo->applicationUserCpuTime[1] += threadCpuTime;
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_3:
					cumulativeInfo->applicationUserCpuTime[2] += threadCpuTime;
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_4:
					cumulativeInfo->applicationUserCpuTime[3] += threadCpuTime;
					break;
				case J9THREAD_USER_DEFINED_THREAD_CATEGORY_5:
					cumulativeInfo->applicationUserCpuTime[4] += threadCpuTime;
					break;
				default:
					break;
				}
			}
		}
		/* Reset the lastCategorySwitchTime, now that it is accounted for */
		thread->lastCategorySwitchTime = 0;
	}

	J9OSMUTEX_EXIT(lib->resourceUsageMutex);
}

/**
 * Detach a thread from the threading library.
 *
 * @note Assumes that the thread being detached is already attached.<br>
 *
 * If the thread is an attached thread, then detach should only be called by the thread
 * itself. Internal resources associated with the thread are freed.
 *
 * If the thread is already dead, this call will destroy it.
 *
 * @param[in] thread a omrthread_t representing the thread to be detached.
 * If this is NULL, the current thread is detached.
 * @return none
 *
 * @see omrthread_attach
 */
void
omrthread_detach(omrthread_t thread)
{
	uintptr_t destroy = 0;
	uintptr_t attached = 0;
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	if (thread == NULL) {
		thread = MACRO_SELF();
	}
	ASSERT(thread);

	THREAD_LOCK(thread, CALLER_DETACH);
	if (thread->attachcount < 1) {
		/* error! */
	} else {
		if (--thread->attachcount == 0) {
			if (thread->flags & J9THREAD_FLAG_ATTACHED) {
				/* This is an attached thread, and it is now fully
				 * detached.  Mark it dead so that it can be destroyed.
				 */
				thread->flags |= J9THREAD_FLAG_DEAD;
				attached = destroy = 1;
			} else {
				/* A j9-created thread should never be dead here */
				destroy = thread->flags & J9THREAD_FLAG_DEAD;
			}
		}
	}
	THREAD_UNLOCK(thread);

	if (destroy) {
		omrthread_library_t library = thread->library;

		omrthread_tls_finalize(thread);
		/* store the cpu usage info of this thread before detaching */
		storeExitCpuUsage(thread);

		/* This check should always pass because an attached thread can never
		 * be joinable, and a j9-created thread should never enter this code path.
		 */
		if (0 == (thread->flags & J9THREAD_FLAG_JOINABLE)) {
			threadDestroy(thread, GLOBAL_NOT_LOCKED);
		}
		TLS_SET(library->self_ptr, NULL);
	}
}


/**
 * Create a new OS thread.
 *
 * The created thread is attached to the threading library.<br>
 * <br>
 * Unlike POSIX, this doesn't require an attributes structure.
 * Instead, any interesting attributes (e.g. stacksize) are
 * passed in with the arguments.
 *
 * @param[out] handle a pointer to a omrthread_t which will point to the thread (if successfully created)
 * @param[in] stacksize the size of the new thread's stack (bytes)<br>
 *			0 indicates use default size
 * @param[in] priority priorities range from J9THREAD_PRIORITY_MIN to J9THREAD_PRIORITY_MAX (inclusive)
 * @param[in] suspend set to non-zero to create the thread in a suspended state.
 * @param[in] entrypoint pointer to the function which the thread will run
 * @param[in] entryarg a value to pass to the entrypoint function
 *
 * @return  0 on success or negative value on failure
 *
 * @see omrthread_create_ex, omrthread_exit, omrthread_resume
 */
intptr_t
omrthread_create(omrthread_t *handle, uintptr_t stacksize, uintptr_t priority, uintptr_t suspend,
				omrthread_entrypoint_t entrypoint, void *entryarg)
{
	intptr_t retVal = J9THREAD_SUCCESS;
	omrthread_attr_t attr;

	if (omrthread_attr_init(&attr) != J9THREAD_SUCCESS) {
		retVal = J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
		return -retVal;
	}

	if (failedToSetAttr(omrthread_attr_set_schedpolicy(&attr, J9THREAD_SCHEDPOLICY_OTHER))) {
		retVal = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	/* HACK: the priority must be set after the policy because RTJ might override the schedpolicy */
	if (failedToSetAttr(omrthread_attr_set_priority(&attr, priority))) {
		retVal = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	if (failedToSetAttr(omrthread_attr_set_stacksize(&attr, stacksize))) {
		retVal = J9THREAD_ERR_INVALID_CREATE_ATTR;
		goto destroy_attr;
	}

	retVal = omrthread_create_ex(handle, &attr, suspend, entrypoint, entryarg);

destroy_attr:
	omrthread_attr_destroy(&attr);
	return -retVal;
}

static intptr_t
failedToSetAttr(intptr_t rc)
{
	rc &= ~J9THREAD_ERR_OS_ERRNO_SET;
	return ((rc != J9THREAD_SUCCESS) && (rc != J9THREAD_ERR_UNSUPPORTED_ATTR));
}

/**
 * Create a new OS thread.
 *
 * The created thread is attached to the threading library.
 *
 * @pre The caller must be an attached thread.
 * @post This function must not modify attr.
 *
 * @param[out] handle Location where the new omrthread_t should be returned, if successfully created. May be NULL.
 * @param[in] attr Creation attributes. If attr is NULL, we will supply a default attr.
 * @param[in] suspend Set to non-zero to create the thread in a suspended state.
 * @param[in] entrypoint Pointer to the function which the thread will run.
 * @param[in] entryarg A value to pass to the entrypoint function.
 * @return success or error code
 * @retval J9THREAD_SUCCESS success
 * @retval J9THREAD_ERR_xxx failure
 * @retval J9THREAD_ERR_OS_ERRNO_SET Bit flag optionally or'd into the return code. Indicates that an os_errno is available.
 *
 * @see omrthread_create, omrthread_exit, omrthread_resume
 */
intptr_t
omrthread_create_ex(omrthread_t *handle, const omrthread_attr_t *attr, uintptr_t suspend,
				   omrthread_entrypoint_t entrypoint, void *entryarg)
{
	return threadCreate(handle, attr, suspend, entrypoint, entryarg, GLOBAL_NOT_LOCKED);
}

/**
 * Function we pass to the OS for new threads.
 *
 * @param arg pointer to the omrthread_t for the new thread
 * @return none
 */
static WRAPPER_TYPE
thread_wrapper(WRAPPER_ARG arg)
{
	omrthread_t thread = (omrthread_t)arg;
	omrthread_library_t lib = thread->library;
	uintptr_t flags;

	ASSERT(thread);
	ASSERT(lib);

	thread->tid = omrthread_get_ras_tid();

	TLS_SET(lib->self_ptr, thread);

#if defined(WIN32)
	if (lib->stack_usage) {
		paint_stack(thread);
	}
#endif /* WIN32 */


	if (thread->flags & J9THREAD_FLAG_CANCELED) {
		threadInternalExit();
	}

	increment_memory_counter(&lib->nativeStackCategory, thread->stacksize);

	/* Handle the create-suspended case.
	 * (This code is basically the same as omrthread_suspend, but we need to
	 * test the condition under mutex or else there's a timing hole)
	 */
	THREAD_LOCK(thread, CALLER_THREAD_WRAPPER);
	if (thread->flags & J9THREAD_FLAG_SUSPENDED) {
		J9OSCOND_WAIT(thread->condition, thread->mutex);
		if ((thread->flags & J9THREAD_FLAG_SUSPENDED) == 0) {
			break;
		}
		J9OSCOND_WAIT_LOOP();
	}
	thread->flags |= J9THREAD_FLAG_STARTED;
	flags = thread->flags;

	/* Set the numa affinity if it is pending on this thread */
#if defined(OMR_PORT_NUMA_SUPPORT)
	{
		/* Initialize the thread->numaAffinity field */
		uintptr_t affinity[J9THREAD_MAX_NUMA_NODE];
		uintptr_t affinityCount = 0;
		uintptr_t nodeNumber = 0;
		uintptr_t numaMaxNode = omrthread_numa_get_max_node();

		memset(affinity, 0x0, sizeof(affinity));

		for (nodeNumber = 1; nodeNumber <= numaMaxNode; nodeNumber++) {
			if (omrthread_does_affinity_cache_contain_node(thread, nodeNumber)) {
				affinity[affinityCount] = nodeNumber;
				affinityCount += 1;
			}
		}
		/* If the affinityCount is zero, this call will have the effect of clearing any affinity that the thread may have inherited
		 * and it assuming the initial affinity that the process had (if reverting to that affinity isn't possible on the platform,
		 * the call is supposed to do nothing).
		 */
		omrthread_numa_set_node_affinity_nolock(thread, affinity, affinityCount, 0);
	}
#endif /* OMR_PORT_NUMA_SUPPORT */
	THREAD_UNLOCK(thread);

	if (thread->flags & J9THREAD_FLAG_CANCELED) {
		threadInternalExit();
	}

	ENABLE_OS_THREAD_STATS(thread);

	threadEnableCpuMonitor(thread);

#if defined(LINUX)
	/* Workaround for NPTL bug on Linux. See omrthread_exit() */
	{
		jmp_buf jumpBuffer;

		if (0 == setjmp(jumpBuffer)) {
			thread->jumpBuffer = &jumpBuffer;
			thread->entrypoint(thread->entryarg);
		}
		thread->jumpBuffer = NULL;
	}
#else /* defined(LINUX) */
	thread->entrypoint(thread->entryarg);
#endif /* defined(LINUX) */

	threadInternalExit();
	/* UNREACHABLE */
	WRAPPER_RETURN();
}



/**
 * Terminate a running thread.
 *
 * @note This should only be used as a last resort.  The system may be in
 * an unpredictable state once a thread is cancelled.  In addition, the thread
 * may not even stop running if it refuses to cancel.
 *
 * @param[in] thread a thread to be terminated
 * @return none
 */
void
omrthread_cancel(omrthread_t thread)
{
	omrthread_monitor_t monitor = NULL;

	ASSERT(thread);
	THREAD_LOCK(thread, CALLER_CANCEL);

	/* Special case -- we can cancel cleanly if the thread hasn't started yet */
	if (0 == (thread->flags & J9THREAD_FLAG_STARTED)) {
		thread->flags |= J9THREAD_FLAG_CANCELED;
		THREAD_UNLOCK(thread);
		omrthread_resume(thread);

	} else {
		/* This is unsafe because thread may have exited and deleted itself. It should never be done. */
		abort();
	}
}

intptr_t
omrthread_join(omrthread_t thread)
{
	intptr_t rc = J9THREAD_ERR;
	omrthread_t self = MACRO_SELF();

	ASSERT(self);

	if ((self == thread) || (NULL == thread)) {
		/* error: can't join yourself */
		rc = J9THREAD_INVALID_ARGUMENT;
	} else if (0 == (thread->flags & J9THREAD_FLAG_JOINABLE)) {
		/* if the target thread isn't joinable, the function may also just segv */
		rc = J9THREAD_INVALID_ARGUMENT;
	} else {
		rc = osthread_join(self, thread);
		if (J9THREAD_SUCCESS == rc) {
			threadDestroy(thread, GLOBAL_NOT_LOCKED);
		}
	}
	return rc;
}

/**
 * Exit the current thread.
 *
 * If the thread has been detached, it is destroyed.
 *
 * If monitor is not NULL, the monitor will be exited before the thread terminates.
 * This is useful if the thread wishes to signal its termination to a watcher, since
 * it exits the monitor and terminates the thread without ever returning control
 * to the thread's routine, which might be running in a DLL which is about to
 * be closed.
 *
 * @param[in] monitor monitor to be exited before exiting (ignored if NULL)
 * @return none
 */
void OMRNORETURN
omrthread_exit(omrthread_monitor_t monitor)
{
	omrthread_t self = MACRO_SELF();

	if (monitor) {
		omrthread_monitor_exit(monitor);
	}

	/* Walk all monitors: if this thread owns a monitor, exit it */
	if (self->lockedmonitorcount > 0) {
		omrthread_monitor_walk_state_t walkState;
		omrthread_monitor_init_walk(&walkState);
		monitor = NULL;
		while (NULL != (monitor = omrthread_monitor_walk(&walkState))) {
			if (monitor->owner == self) {
				monitor->count = 1;	/* exit n-1 times */
				omrthread_monitor_exit(monitor);
			}
		}
	}

#if defined(LINUX)
	/* NPTL calls __pthread_unwind() from pthread_exit(). That walks the stack.
	 * We can't allow that to happen, since our caller might have already been
	 * unloaded. Walking the calling frame could cause a crash. Instead, we
	 * longjmp back out to the initial frame.
	 *
	 * See CMVC defect 76014.
	 */
	if (self->jumpBuffer) {
		longjmp(*(jmp_buf *)self->jumpBuffer, 1);
	}
#endif /* defined(LINUX) */

	threadInternalExit();

dontreturn:
	goto dontreturn; /* avoid warnings */
}

/**
 * Create a new OS thread and attach it to the library.
 *
 * @param[out] handle
 * @param[in] attr attr must not be modified by this function.
 * @param[in] suspend Non-zero if the thread should suspend before entering entrypoint,
 * zero to allow the thread to run freely.
 * @param[in] entrypoint
 * @param[in] entryarg
 * @param[in] globalIsLocked Indicates whether the threading library global mutex is already locked.
 * @return success status
 *
 * @see threadDestroy
 */
static intptr_t
threadCreate(omrthread_t *handle, const omrthread_attr_t *attr, uintptr_t suspend,
			 omrthread_entrypoint_t entrypoint, void *entryarg, int globalIsLocked)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	omrthread_t self = MACRO_SELF();
	omrthread_attr_t tempAttr = NULL;
	omrthread_t thread;
	intptr_t retVal = J9THREAD_ERR;
	BOOLEAN tempAttrAllocated = FALSE;

	ASSERT(lib->initStatus);
	ASSERT(self);

	/* create a default attr if none was supplied */
	if (NULL != attr) {
#if defined(OSX)
		/* OSX threads ignore policy inheritance. Create a new attr and do inheritance manually as needed. */
		if (J9THREAD_SCHEDPOLICY_INHERIT == (*attr)->schedpolicy) {
			if (J9THREAD_SUCCESS != copyAttr(&tempAttr, attr)) {
				retVal = J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
				goto threadCreateDone;
			}
			tempAttrAllocated = TRUE;
		} else
#endif /* defined(OSX) */
		{
			tempAttr = *attr;
		}
	} else {
		if (J9THREAD_SUCCESS != omrthread_attr_init(&tempAttr)) {
			retVal = J9THREAD_ERR_CANT_ALLOC_CREATE_ATTR;
			goto threadCreateDone;
		}
		tempAttrAllocated = TRUE;
	}
	ASSERT(tempAttr);

	thread = threadAllocate(lib, globalIsLocked);
	if (!thread) {
		retVal = J9THREAD_ERR_CANT_ALLOCATE_J9THREAD_T;
		goto cleanup0;
	}
	if (handle) {
		*handle = thread;
	}
	ASSERT(thread->library == lib);

	if (self && (J9THREAD_SCHEDPOLICY_INHERIT == tempAttr->schedpolicy)) {
		thread->priority = self->priority;
#if defined(OSX)
		if (J9THREAD_SUCCESS != omrthread_attr_set_priority(&tempAttr, self->priority)) {
			retVal = J9THREAD_ERR_INVALID_SCHEDPOLICY;
			goto cleanup1;
		}
#endif /* defined(OSX) */
	} else {
		thread->priority = tempAttr->priority;
	}
	thread->attachcount = 0;
	thread->stacksize = tempAttr->stacksize;

	memset(thread->tls, 0, sizeof(thread->tls));

	thread->interrupter = NULL;

	if (!J9OSCOND_INIT(thread->condition)) {
		retVal = J9THREAD_ERR_CANT_INIT_CONDITION;
		goto cleanup1;
	}
	if (!J9OSMUTEX_INIT(thread->mutex)) {
		retVal = J9THREAD_ERR_CANT_INIT_MUTEX;
		goto cleanup2;
	}

	thread->flags = suspend ? J9THREAD_FLAG_SUSPENDED : 0;
	if (J9THREAD_CREATE_JOINABLE == tempAttr->detachstate) {
		thread->flags |= J9THREAD_FLAG_JOINABLE;
	}
	thread->entrypoint = entrypoint;
	thread->entryarg = entryarg;
	thread->lockedmonitorcount = 0;
	thread->waitNumber = 0;
	thread->lastCategorySwitchTime = 0;

	/* Ignore errors, the thead cpu monitor should not cause thread creation failure */
	omrthread_set_category(thread, tempAttr->category, J9THREAD_TYPE_SET_CREATE);

#if defined(LINUX)
	thread->jumpBuffer = NULL;
#endif /* defined(LINUX) */

#if defined(OMR_PORT_NUMA_SUPPORT)
	memset(&(thread->numaAffinity), 0x0, sizeof(thread->numaAffinity));
#endif /* OMR_PORT_NUMA_SUPPORT */

	retVal = osthread_create(self, &(thread->handle), tempAttr, thread_wrapper, (WRAPPER_ARG)thread);
	if (retVal != J9THREAD_SUCCESS) {
		goto cleanup4;
	}

	/* clean up the temp attr */
	if (tempAttrAllocated) {
		omrthread_attr_destroy(&tempAttr);
	}
	return J9THREAD_SUCCESS;

	/* Cleanup points */
cleanup4:
	J9OSMUTEX_DESTROY(thread->mutex);
cleanup2:	J9OSCOND_DESTROY(thread->condition);
cleanup1:	threadFree(thread, globalIsLocked);
cleanup0:	if (handle) *handle = NULL;
	/* clean up the temp attr */
	if (tempAttrAllocated) {
		omrthread_attr_destroy(&tempAttr);
	}
threadCreateDone:
	ASSERT(retVal > 0);
	return retVal;
}

#if defined(OSX)
static intptr_t
copyAttr(omrthread_attr_t *attrTo, const omrthread_attr_t *attrFrom)
{
	intptr_t rc = J9THREAD_SUCCESS;
	if (NULL == attrFrom) {
		rc = J9THREAD_ERR_INVALID_ATTR;
		goto copyAttrDone;
	}

	/* Start with a new attr. */
	rc = omrthread_attr_init(attrTo);
	if (J9THREAD_SUCCESS != rc) {
		goto copyAttrDone;
	}

	/* For each attribute used by omr, set it in attrTo if the initialized default differs from attrFrom. */
	if ((*attrTo)->category != (*attrFrom)->category) {
		if (failedToSetAttr(omrthread_attr_set_category(attrTo, (*attrFrom)->category))) {
			rc = J9THREAD_ERR_INVALID_ATTR;
			goto destroyAttr;
		}
	}
	if ((*attrTo)->stacksize != (*attrFrom)->stacksize) {
		if (failedToSetAttr(omrthread_attr_set_stacksize(attrTo, (*attrFrom)->stacksize))) {
			rc = J9THREAD_ERR_INVALID_ATTR;
			goto destroyAttr;
		}
	}
	if ((*attrTo)->schedpolicy != (*attrFrom)->schedpolicy) {
		if (failedToSetAttr(omrthread_attr_set_schedpolicy(attrTo, (*attrFrom)->schedpolicy))) {
			rc = J9THREAD_ERR_INVALID_ATTR;
			goto destroyAttr;
		}
	}
	if ((*attrTo)->priority != (*attrFrom)->priority) {
		if (failedToSetAttr(omrthread_attr_set_priority(attrTo, (*attrFrom)->priority))) {
			rc = J9THREAD_ERR_INVALID_ATTR;
			goto destroyAttr;
		}
	}
	if ((*attrTo)->detachstate != (*attrFrom)->detachstate) {
		if (failedToSetAttr(omrthread_attr_set_detachstate(attrTo, (*attrFrom)->detachstate))) {
			rc = J9THREAD_ERR_INVALID_ATTR;
			goto destroyAttr;
		}
	}
	if ((*attrTo)->name != (*attrFrom)->name) {
		if (failedToSetAttr(omrthread_attr_set_name(attrTo, (*attrFrom)->name))) {
			rc = J9THREAD_ERR_INVALID_ATTR;
			goto destroyAttr;
		}
	}

	return rc;
	
destroyAttr:
	omrthread_attr_destroy((omrthread_attr_t *)&attrTo);
copyAttrDone:
	return rc;
}
#endif /* defined(OSX) */

/**
 * Allocate a omrthread_t from the omrthread_t pool.
 *
 * This function should initialize anything that is needed whenever a omrthread is created or attached.
 *
 * @note assumes the threading library's thread pool is already initialized
 * @param[in] globalIsLocked Indicates whether the threading library global mutex is already locked.
 * @return a new omrthread_t on success, NULL on failure.
 *
 * @see threadFree
 */
static omrthread_t
threadAllocate(omrthread_library_t lib, int globalIsLocked)
{
	omrthread_t newThread;
	ASSERT(lib);
	ASSERT(lib->thread_pool);

	if (!globalIsLocked) {
		GLOBAL_LOCK_SIMPLE(lib);
	}

	newThread = pool_newElement(lib->thread_pool);

	if (newThread != NULL) {
		lib->threadCount++;
		newThread->library = lib;
		newThread->os_errno = J9THREAD_INVALID_OS_ERRNO;
#if defined(J9ZOS390)
		newThread->os_errno2 = 0;
#endif /* J9ZOS390 */
#if defined(OMR_THR_JLM)
		if (newThread) {
			if (IS_JLM_ENABLED(newThread)) {
				if (jlm_thread_init(newThread) != 0) {
					threadFree(newThread, GLOBAL_IS_LOCKED);
					newThread = NULL;
				}
			}
		}
#endif /* OMR_THR_JLM */
	}

	if (!globalIsLocked) {
		GLOBAL_UNLOCK_SIMPLE(lib);
	}

	return newThread;
}



/**
 * Destroy the resources associated with the thread.
 *
 * If the thread is not already dead, the function fails.
 *
 * @param[in] thread thread to be destroyed
 * @param[in] globalAlreadyLocked indicated whether the thread library global mutex is already locked
 * @return 0 on success or negative value on failure.
 *
 * @see threadCreate
 */
static intptr_t
threadDestroy(omrthread_t thread, int globalAlreadyLocked)
{
	omrthread_library_t lib;

	ASSERT(thread);
	lib = thread->library;
	ASSERT(lib);

	THREAD_LOCK(thread, CALLER_DESTROY);
	if ((thread->flags & J9THREAD_FLAG_DEAD) == 0) {
		THREAD_UNLOCK(thread);
		return -1;
	}
	THREAD_UNLOCK(thread);

	J9OSCOND_DESTROY(thread->condition);

	J9OSMUTEX_DESTROY(thread->mutex);

#ifdef OMR_THR_TRACING
	omrthread_dump_trace(thread);
#endif

#if defined(WIN32)
	/* Acquire the global monitor mutex (if needed) while performing handle closing and freeing. */
	if (!globalAlreadyLocked) {
		GLOBAL_LOCK_SIMPLE(lib);
	}

	/* For attached threads, need to close the duplicated handle.
	 * For created threads, need to close the handle returned from _beginthreadex().
	 */
	CloseHandle(thread->handle);

	threadFree(thread, GLOBAL_IS_LOCKED);

	if (!globalAlreadyLocked) {
		GLOBAL_UNLOCK_SIMPLE(lib);
	}

#else /* defined(WIN32) */

	/* On z/OS, we cannot call GLOBAL_LOCK_SIMPLE(lib) before calling threadFree(), since this leads to
	 * seg faults in the JVM. See PR 82332: [zOS S390] 80 VM_Extended.TestJvmCpuMonitorMXBeanLocal : crash
	 */
	threadFree(thread, globalAlreadyLocked);

#endif /* defined(WIN32) */

	return 0;
}



/**
 * Return a omrthread_t to the threading library's monitor pool.
 *
 * @param[in] thread thread to be returned to the pool
 * @param[in] globalAlreadyLocked indicated whether the threading library global
 * mutex is already locked
 * @return none
 *
 * @see threadAllocate
 */
static void
threadFree(omrthread_t thread, int globalAlreadyLocked)
{
	omrthread_library_t lib = thread->library;

	ASSERT(thread);

	if (!globalAlreadyLocked) {
		GLOBAL_LOCK_SIMPLE(lib);
	}

#ifdef OMR_THR_JLM
	jlm_thread_free(lib, thread);
#endif

	pool_removeElement(lib->thread_pool, thread);
	lib->threadCount--;

	if (!globalAlreadyLocked) {
		GLOBAL_UNLOCK_SIMPLE(lib);
	}
}



/**
 * Exit from the current thread.
 *
 * If the thread has been detached it is destroyed.
 */
static void OMRNORETURN
threadInternalExit(void)
{
	omrthread_t self = MACRO_SELF();
	omrthread_library_t lib = self->library;
	int detached;

	ASSERT(self);
	ASSERT(lib);

	omrthread_tls_finalize(self);

	GLOBAL_LOCK(self, CALLER_INTERNAL_EXIT1);
	THREAD_LOCK(self, CALLER_INTERNAL_EXIT1);
	self->flags |= J9THREAD_FLAG_DEAD;
	detached = (0 == self->attachcount);

	decrement_memory_counter(&lib->nativeStackCategory, self->stacksize);

	/*
	 * Is there an interruptServer thread out there trying to interrupt us?
	 * Its services are no longer required.
	 */
	if (self->interrupter) {
		THREAD_LOCK(self->interrupter, CALLER_INTERNAL_EXIT1);
		self->interrupter->flags |= J9THREAD_FLAG_CANCELED;
		THREAD_UNLOCK(self->interrupter);
		self->interrupter = NULL;
	}

	THREAD_UNLOCK(self);

	/* Capture the cpu usage of this thread for future accounting */
	storeExitCpuUsage(self);

	if (0 == (self->flags & J9THREAD_FLAG_JOINABLE)) {
#if !defined(J9ZOS390)
		/* On z/OS we create the thread in the detached state, so the
		 * call to pthread_detach is not required.
		 */
		THREAD_DETACH(self->handle);
#endif /* !defined(J9ZOS390) */

		if (detached) {
			threadDestroy(self, GLOBAL_IS_LOCKED);
		}
	}

	GLOBAL_UNLOCK_SIMPLE(lib);

	/* We couldn't clear TLS earlier because the debug version of
	 * GLOBAL_UNLOCK_SIMPLE() uses omrthread_self().
	 */
	if (detached) {
		TLS_SET(lib->self_ptr, NULL);
	}

	THREAD_EXIT();
	ASSERT(0);
	/* UNREACHABLE */

dontreturn:
	goto dontreturn; /* avoid warnings */
}



/**
 * Return the omrthread_t for the current thread.
 *
 * @note Must be called only by an attached thread
 *
 * @return omrthread_t for the current thread
 *
 * @see omrthread_attach
 *
 */
omrthread_t
omrthread_self(void)
{
	return MACRO_SELF();
}



/**
 * Suspend the current thread from executing
 * for at least the specified time.
 *
 * @param[in] millis
 * @param[in] nanos
 * @return  0 on success<br>
 *    J9THREAD_INVALID_ARGUMENT if the arguments are invalid<br>
 *    J9THREAD_INTERRUPTED if the sleep was interrupted<br>
 *    J9THREAD_PRIORITY_INTERRUPTED if the sleep was priority-interrupted
 *
 * @see omrthread_sleep
 */
intptr_t
omrthread_sleep_interruptable(int64_t millis, intptr_t nanos)
{
	omrthread_t self = MACRO_SELF();
	intptr_t boundedMillis = BOUNDED_I64_TO_IDATA(millis);

	ASSERT(self);

	if ((millis < 0) || (nanos < 0) || (nanos >= 1000000)) {
		return J9THREAD_INVALID_ARGUMENT;
	}

	THREAD_LOCK(self, CALLER_SLEEP_INTERRUPTABLE);

	if (self->flags & J9THREAD_FLAG_INTERRUPTED) {
		self->flags &= ~J9THREAD_FLAG_INTERRUPTED;
		THREAD_UNLOCK(self);
		return J9THREAD_INTERRUPTED;
	}
	if (self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED) {
		self->flags &= ~J9THREAD_FLAG_PRIORITY_INTERRUPTED;
		THREAD_UNLOCK(self);
		return J9THREAD_PRIORITY_INTERRUPTED;
	}
	if (self->flags & J9THREAD_FLAG_ABORTED) {
		THREAD_UNLOCK(self);
		return J9THREAD_PRIORITY_INTERRUPTED;
	}

	self->flags |= J9THREAD_FLAGM_SLEEPING_TIMED_INTERRUPTIBLE;

	J9OSCOND_WAIT_IF_TIMEDOUT(self->condition, self->mutex, boundedMillis, nanos) {
		break;
	} else {
		if (self->flags & J9THREAD_FLAG_INTERRUPTED) {
			self->flags &= ~(J9THREAD_FLAG_INTERRUPTED | J9THREAD_FLAGM_SLEEPING_TIMED_INTERRUPTIBLE);
			THREAD_UNLOCK(self);
			return J9THREAD_INTERRUPTED;
		}
		if (self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED) {
			self->flags &= ~(J9THREAD_FLAG_PRIORITY_INTERRUPTED | J9THREAD_FLAGM_SLEEPING_TIMED_INTERRUPTIBLE);
			THREAD_UNLOCK(self);
			return J9THREAD_PRIORITY_INTERRUPTED;
		}
		if (self->flags & J9THREAD_FLAG_ABORTED) {
			self->flags &= ~(J9THREAD_FLAGM_SLEEPING_TIMED_INTERRUPTIBLE);
			THREAD_UNLOCK(self);
			return J9THREAD_PRIORITY_INTERRUPTED;
		}
	}
	J9OSCOND_WAIT_TIMED_LOOP();

	self->flags &= ~(J9THREAD_FLAGM_SLEEPING_TIMED_INTERRUPTIBLE);
	THREAD_UNLOCK(self);
	return 0;
}


/**
 * Suspend the current thread from executing
 * for at least the specified time.
 *
 * @param[in] millis minimum number of milliseconds to sleep
 * @return  0 on success<br> J9THREAD_INVALID_ARGUMENT if millis < 0
 *
 * @see omrthread_sleep_interruptable
 */
intptr_t
omrthread_sleep(int64_t millis)
{
	omrthread_t self = MACRO_SELF();
#ifdef OMR_ENV_DATA64
	intptr_t boundedMillis = millis;
#else
	intptr_t boundedMillis = (millis > 0x7FFFFFFF ? 0x7FFFFFFF : (intptr_t)millis);
#endif

	ASSERT(self);

	if (millis < 0) {
		return J9THREAD_INVALID_ARGUMENT;
	}

	THREAD_LOCK(self, CALLER_SLEEP);

	self->flags |= J9THREAD_FLAGM_SLEEPING_TIMED;

	J9OSCOND_WAIT_IF_TIMEDOUT(self->condition, self->mutex, boundedMillis, 0) {
		break;
	}
	J9OSCOND_WAIT_TIMED_LOOP();

	self->flags &= ~J9THREAD_FLAGM_SLEEPING_TIMED;
	THREAD_UNLOCK(self);
	return 0;
}


/**
* @brief Yield the processor
* @param sequencialYields, number of yields in a row that have been made
* @return void
*/
void
omrthread_yield_new(uintptr_t sequentialYields)
{
#ifdef OMR_THR_YIELD_ALG
	THREAD_YIELD_NEW(sequentialYields);
#else
	THREAD_YIELD();
#endif
}

/**
 * Yield the processor.
 *
 * @return none
 */
void
omrthread_yield(void)
{
#ifdef OMR_THR_YIELD_ALG
	THREAD_YIELD_NEW(1);
#else
	THREAD_YIELD();
#endif
}

/**
 * Suspend the current thread from executing
 * for at least the specified time.
 *
 * Should only be called if omrthread_nanosleep_supported returns true
 *
 * @param[in] nanos minimum number of nanoseconds to sleep
 * @return  0 on success<br> J9THREAD_INVALID_ARGUMENT if nanos < 0 or nanosleep not supported
 *
 * @see omrthread_sleep
 */
intptr_t
omrthread_nanosleep(int64_t nanos)
{
#if USE_CLOCK_NANOSLEEP
	omrthread_t self = MACRO_SELF();
	struct timespec sleeptime;

	ASSERT(self);

	if (nanos < 0) {
		return J9THREAD_INVALID_ARGUMENT;
	}

	sleeptime.tv_sec = (time_t)(nanos / 1000000000);
	sleeptime.tv_nsec = (long)(nanos % 1000000000);
	while (clock_nanosleep(NANOSLEEP_CLOCK, NANOSLEEP_FLAGS, &sleeptime, NULL) == EINTR);
	return 0;
#else
	/* Nanosecond sleep resolution is not supported. */
	/* For now just use the millisecond resolution version.  */
	int64_t millis = nanos / 1000000;
	int32_t remNanos = (int32_t)(nanos % 1000000);
	return omrthread_sleep(ADJUST_TIMEOUT(millis, remNanos));
#endif
}


/**
 * Tells whether nanosleep is supported on this platform.
 *
 * @return  1 if nanosleep supported, 0 if not
 *
 */
intptr_t
omrthread_nanosleep_supported(void)
{
#if USE_CLOCK_NANOSLEEP
	return 1;
#else
	return 0;
#endif
}


/**
 * Suspend the current thread from executing
 * until the specified absolute time.
 *
 * Should only be called if omrthread_nanosleep_supported returns true
 *
 * @param[in] wakeTime absolute wake up time in nanoseconds since the epoch (Jan 1, 1970)
 * @return  0 on success<br>
 *          J9THREAD_INVALID_ARGUMENT if nanosleep not supported
 *
 * @see omrthread_sleep
 */
intptr_t
omrthread_nanosleep_to(int64_t wakeTime)
{
#if USE_CLOCK_NANOSLEEP
	omrthread_t self = MACRO_SELF();
	struct timespec sleeptime;
	ASSERT(self);

	sleeptime.tv_sec = (int)(wakeTime / 1000000000);
	sleeptime.tv_nsec = wakeTime % 1000000000;
	while (clock_nanosleep(NANOSLEEP_CLOCK, NANOSLEEP_ABS_FLAGS, &sleeptime, NULL) == EINTR);
	return 0;
#else
	return J9THREAD_INVALID_ARGUMENT;
#endif
}


/**
 * Suspend the current thread.
 *
 * Stop the current thread from executing until it is resumed.
 *
 * @return none
 *
 * @see omrthread_resume
 */
void
omrthread_suspend(void)
{
	omrthread_t self = MACRO_SELF();
	ASSERT(self);

	Trc_THR_ThreadSuspendEnter(self);

	THREAD_LOCK(self, CALLER_SUSPEND);
	self->flags |= J9THREAD_FLAG_SUSPENDED;

	J9OSCOND_WAIT(self->condition, self->mutex);
		if ((self->flags & J9THREAD_FLAG_SUSPENDED) == 0) {
			break;
		}
	J9OSCOND_WAIT_LOOP();

	THREAD_UNLOCK(self);

	Trc_THR_ThreadSuspendExit(self);
}


/**
 * Resume a thread.
 *
 * Take a thread out of the suspended state.
 *
 * If the thread is not suspended, no action is taken.
 *
 * @param[in] thread a thread to be resumed
 * @return 1 if thread was suspended, 0 if not
 *
 * @see omrthread_create, omrthread_suspend
 */
intptr_t
omrthread_resume(omrthread_t thread)
{
	ASSERT(thread);

	if ((thread->flags & J9THREAD_FLAG_SUSPENDED) == 0) {
		/* it wasn't suspended! */
		return 0;
	}

	THREAD_LOCK(thread, CALLER_RESUME);

	/*
	 * The thread _should_ only be OS suspended once, but
	 * handle the case where it's suspended more than once anyway.
	 */
	NOTIFY_WRAPPER(thread);
	thread->flags &= ~J9THREAD_FLAG_SUSPENDED;

	Trc_THR_ThreadResumed(thread, MACRO_SELF());

	THREAD_UNLOCK(thread);
	return 1;
}

/**
 * Set a thread's name on platforms that support thread naming.
 *
 * @param[in] thread a thread
 * @param[in] name
 *
 * @returns 0 on success or negative value on failure (name wasn't changed).
 * Platforms that do not support thread naming will return 0.
 *
 */
intptr_t
omrthread_set_name(omrthread_t thread, const char *name)
{
	omrthread_t self;

	ASSERT(thread);

	if (name == NULL || !name[0]) {
		return -1;
	}

	self = MACRO_SELF();
	ASSERT(self);
	Trc_THR_ThreadSetName(thread, name);
	return THREAD_SET_NAME(self->handle, thread->handle, name);
}

/**
 * Set a thread's execution priority.
 *
 * @param[in] thread a thread
 * @param[in] priority
 * Use the following symbolic constants for priorities:<br>
 *				J9THREAD_PRIORITY_MAX<br>
 *				J9THREAD_PRIORITY_USER_MAX<br>
 *				J9THREAD_PRIORITY_NORMAL<br>
 *				J9THREAD_PRIORITY_USER_MIN<br>
 *				J9THREAD_PRIORITY_MIN<br>
 *
 * @returns 0 on success or negative value on failure (priority wasn't changed)
 *
 *
 */
intptr_t
omrthread_set_priority(omrthread_t thread, uintptr_t priority)
{
	ASSERT(thread);

	if (priority > J9THREAD_PRIORITY_MAX) {
		return -1;
	}

	if (0 == (thread->library->flags & J9THREAD_LIB_FLAG_NO_SCHEDULING)) {
		if (THREAD_SET_PRIORITY(thread->handle, priority)) {
			return -1;
		}
	}

	thread->priority = priority;

	Trc_THR_ThreadSetPriority(thread, priority);

	return 0;
}


/**
 * spreads the native priorities so that they are not all mapped to the same value.
 * Returns 0 on success.  If the spread is not possible returns non-zero.
 */
intptr_t
omrthread_set_priority_spread(void)
{
	intptr_t rc = -1;

	if (omrthread_lib_use_realtime_scheduling()) {
		rc = set_priority_spread();
	}

	return rc;
}

/**
 * Abort a thread.
 *
 * Sets the permanent J9THREAD_FLAG_ABORTED flag on the thread state.
 *
 * If the thread is in an interruptable blocked state,
 * resume the thread and return from the blocking function with
 * J9THREAD_PRIORITY_INTERRUPTED or
 * J9THREAD_INTERRUPTED_MONITOR_ENTER
 *
 * @param[in] thread a thead to be aborted
 * @return none
 *
 * @see omrthread_priority_interrupt, omrthread_interrupt
 */
void
omrthread_abort(omrthread_t thread)
{
	threadInterrupt(thread, J9THREAD_FLAG_ABORTED);
}


/**
 * Interrupt a thread.
 *
 * If the thread is currently blocked (i.e. waiting on a monitor_wait or sleeping)
 * resume the thread and cause it to return from the blocking function with
 * J9THREAD_INTERRUPTED.
 *
 * @param[in] thread a thead to be interrupted
 * @return none
 */
void
omrthread_interrupt(omrthread_t thread)
{
	Trc_THR_ThreadInterruptEnter(MACRO_SELF(), thread);
	threadInterrupt(thread, J9THREAD_FLAG_INTERRUPTED);
	Trc_THR_ThreadInterruptExit(thread);
}


/**
 * Return the value of a thread's interrupted flag.
 *
 * @param[in] thread thread to be queried
 * @return 0 if not interrupted, non-zero if interrupted
 */
uintptr_t
omrthread_interrupted(omrthread_t thread)
{
	ASSERT(thread);
	return (thread->flags & J9THREAD_FLAG_INTERRUPTED) != 0;
}


/**
 * Clear the interrupted flag of the current thread and return its previous value.
 *
 * @return  previous value of interrupted flag: non-zero if the thread had been interrupted.
 */
uintptr_t
omrthread_clear_interrupted(void)
{
	uintptr_t oldFlags;
	omrthread_t self = MACRO_SELF();
	ASSERT(self);

	THREAD_LOCK(self, CALLER_CLEAR_INTERRUPTED);
	oldFlags = self->flags;
	self->flags = oldFlags & ~J9THREAD_FLAG_INTERRUPTED;
	THREAD_UNLOCK(self);

	return (oldFlags & J9THREAD_FLAG_INTERRUPTED) != 0;

}


/**
 * Priority interrupt a thread.
 *
 * If the thread is currently blocked (i.e. waiting on a monitor_wait or sleeping)
 * resume the thread and return from the blocking function with
 * J9THREAD_PRIORITY_INTERRUPTED
 *
 * @param[in] thread a thead to be priority interrupted
 * @return none
 */
void
omrthread_priority_interrupt(omrthread_t thread)
{
	threadInterrupt(thread, J9THREAD_FLAG_PRIORITY_INTERRUPTED);
}


/**
 * Return the value of a thread's priority interrupted flag.
 *
 * @param[in] thread thread to be queried
 * @return 0 if not priority interrupted, non-zero if priority interrupted flag set
 */
uintptr_t
omrthread_priority_interrupted(omrthread_t thread)
{
	return (thread->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED) != 0;
}


/**
 * Clear the priority interrupted flag of the current thread and return its previous value.
 *
 * @return  previous value of priority interrupted flag: nonzero if the thread had been priority interrupted.
 */
uintptr_t
omrthread_clear_priority_interrupted(void)
{
	uintptr_t oldFlags;
	omrthread_t self = MACRO_SELF();
	ASSERT(self);

	THREAD_LOCK(self, CALLER_CLEAR_PRIORITY_INTERRUPTED);
	oldFlags = self->flags;
	self->flags = oldFlags & ~J9THREAD_FLAG_PRIORITY_INTERRUPTED;
	THREAD_UNLOCK(self);

	return (oldFlags & J9THREAD_FLAG_PRIORITY_INTERRUPTED) != 0;
}



/**
 * Interrupt a thread.
 *
 * If the thread is currently blocked (e.g. waiting on monitor_wait)
 * resume the thread and return from the blocking function with
 * J9THREAD_INTERRUPTED, J9THREAD_PRIORITY_INTERRUPTED,
 * or J9THREAD_INTERRUPTED_MONITOR_ENTER
 *
 * If it can't be resumed (it's not in an interruptable state)
 * then just set the appropriate interrupt flag.
 *
 * @param[in] thread thread to be interrupted
 * @param[in] interruptFlag indicates type of interrupt (normal/priority-interrupt/abort)
 * @return none
 */
static void
threadInterrupt(omrthread_t thread, uintptr_t interruptFlag)
{
	uintptr_t currFlags;
	uintptr_t testFlags;
	omrthread_t self = MACRO_SELF();
	BOOLEAN threadMutexIsUnlocked = FALSE;

	ASSERT(self);
	ASSERT(thread);

	testFlags = J9THREAD_FLAG_INTERRUPTABLE;
	if (J9THREAD_FLAG_ABORTED == interruptFlag) {
		testFlags |= J9THREAD_FLAG_ABORTABLE;
	}

	GLOBAL_LOCK(self, CALLER_INTERRUPT_THREAD);
	THREAD_LOCK(thread, CALLER_INTERRUPT_THREAD);
	if (thread->flags & interruptFlag) {
		THREAD_UNLOCK(thread);
		GLOBAL_UNLOCK(self);
		return;
	}

	currFlags = thread->flags;
	thread->flags |= interruptFlag;

	if (currFlags & testFlags) {
		if (currFlags & (J9THREAD_FLAG_SLEEPING | J9THREAD_FLAG_PARKED)) {
			NOTIFY_WRAPPER(thread);

		} else if (currFlags & J9THREAD_FLAG_WAITING) {
			if (interrupt_waiting_thread(self, thread) == 1) {
				threadMutexIsUnlocked = TRUE;
			}
#ifdef OMR_THR_THREE_TIER_LOCKING
		} else if (currFlags & J9THREAD_FLAG_BLOCKED) {
			interrupt_blocked_thread(self, thread);
#endif
		}
	}

	if (FALSE == threadMutexIsUnlocked) {
		THREAD_UNLOCK(thread);
	}
	GLOBAL_UNLOCK(self);
}

/*
 * Interrupt a blocked thread.
 *
 * @param[in] self current thread
 * @param[in] threadToInterrupt
 * @note: Assumes caller has locked the global mutex
 * @note: Assumes caller has locked the thread mutex
 */
static void
interrupt_blocked_thread(omrthread_t self, omrthread_t threadToInterrupt)
{
	omrthread_monitor_t monitor;

	ASSERT(self);
	ASSERT(threadToInterrupt);
	ASSERT(self != threadToInterrupt);
	ASSERT(threadToInterrupt->flags & J9THREAD_FLAG_ABORTABLE);
	ASSERT(threadToInterrupt->monitor);
	ASSERT(NULL == threadToInterrupt->interrupter);

	monitor = threadToInterrupt->monitor;

	if (MONITOR_TRY_LOCK(monitor) == 0) {
		NOTIFY_WRAPPER(threadToInterrupt);
	} else {
		omrthread_monitor_pin(monitor, self);
		THREAD_UNLOCK(threadToInterrupt);

		MONITOR_LOCK(monitor, CALLER_INTERRUPT_BLOCKED);
		THREAD_LOCK(threadToInterrupt, 0);

		if (threadToInterrupt->monitor == monitor) {
			if ((threadToInterrupt->flags &
				 (J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_ABORTABLE | J9THREAD_FLAG_ABORTED)) ==
				(J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_ABORTABLE | J9THREAD_FLAG_ABORTED)) {
				NOTIFY_WRAPPER(threadToInterrupt);
			}
		}

		omrthread_monitor_unpin(monitor, self);
	}
	MONITOR_UNLOCK(monitor);
}

/**
 * Interrupt a waiting thread.
 *
 * @param[in] self current thread
 * @param[in] threadToInterrupt
 * @return 1 if the thread was immediately interrupted<br>
 * 0 if the thread will be interrupted asap by a special thread.
 * @note: if 1 is returned, THE THREAD MUTEX HAS BEEN RELEASED AS A SIDE EFFECT.
 * @note: Assumes caller has locked the global mutex
 * @note: Assumes caller has locked the thread mutex
 */
static intptr_t
interrupt_waiting_thread(omrthread_t self, omrthread_t threadToInterrupt)
{
	intptr_t retVal = 0;
	omrthread_monitor_t monitor;

	ASSERT(self);
	ASSERT(threadToInterrupt);
	ASSERT(self != threadToInterrupt);
	ASSERT(threadToInterrupt->flags & (J9THREAD_FLAG_INTERRUPTABLE | J9THREAD_FLAG_ABORTABLE));
	ASSERT(threadToInterrupt->monitor);
	ASSERT(NULL == threadToInterrupt->interrupter);

	/* THE CALLER MUST HAVE THREAD_LOCK */

#if !defined(ALWAYS_SPAWN_THREAD_TO_INTERRUPT)
	/*
	 * If we can enter the monitor without blocking, we don't need the
	 * interruptServer thread.
	 */
	monitor = threadToInterrupt->monitor;

	/*
	 * Holding THREAD_LOCK here prevents threadToInterrupt from exiting monitor_wait.
	 * This keeps the monitor pinned while we're using it.
	 *
	 * In the 3-tier case, this prevents us from acquiring MONITOR_LOCK before the
	 * thread to interrupt can.
	 */
	if (omrthread_monitor_try_enter_using_threadId(monitor, self) == 0) {
		ASSERT(monitor->owner == self);

#ifdef OMR_THR_THREE_TIER_LOCKING
		/* CMVC 109361. Can't block in MONITOR_LOCK() while holding THREAD_LOCK() */
		THREAD_UNLOCK(threadToInterrupt);

		/* Required to prevent COND_NOTIFY before threadToInterrupt can COND_WAIT */
		MONITOR_LOCK(monitor, CALLER_INTERRUPT_WAITING);

		THREAD_LOCK(threadToInterrupt, CALLER_INTERRUPT_WAITING);
		/*
		 * COND_NOTIFY only if the thread is still waiting and waiting on the same monitor.
		 *
		 * If the thread is not waiting, we don't need to do anything. The thread has already awakened
		 * and observed that it is interrupted by checking its flags.
		 *
		 * It should be impossible for the thread to have awakened and waited again,
		 * on this monitor or any other, because we own the monitor and monitor_wait
		 * can't exit without owning the monitor.
		 */
		if (threadToInterrupt->monitor == monitor) {
			if (threadToInterrupt->flags & J9THREAD_FLAG_WAITING) {

				threadInterruptWake(threadToInterrupt, monitor);

			} /* J9THREAD_FLAG_WAITING */
		} /* ->monitor == monitor */
#else /* OMR_THR_THREE_TIER_LOCKING */
		/*
		 * In the non-3-tier case, MONITOR_LOCK is acquired by try_enter,
		 * so we don't need to release and reacquire THREAD_LOCK.
		 */
		threadInterruptWake(threadToInterrupt, monitor);
#endif /* OMR_THR_THREE_TIER_LOCKING */

		/*
		 * THREAD_UNLOCK is required before monitor_exit
		 * because monitor_exit may MONITOR_LOCK in the 3-tier case.
		 */
		THREAD_UNLOCK(threadToInterrupt);

#if defined(OMR_THR_THREE_TIER_LOCKING)
		MONITOR_UNLOCK(monitor);
#endif
		omrthread_monitor_exit_using_threadId(monitor, self);
		retVal = 1;
	}
	else
#endif /* ALWAYS_SPAWN_THREAD_TO_INTERRUPT */
	{
		omrthread_library_t lib = self->library;

		/*
		 * spawn a thread to do it for us, because it's possible that
		 * having this thread lock the waiting thread's monitor may
		 * cause deadlock
		 */
		threadCreate(&threadToInterrupt->interrupter, &lib->systemThreadAttr,
					 0, interruptServer, (void *)threadToInterrupt, GLOBAL_IS_LOCKED);
	}
	return retVal;
}

/**
 * Interrupt a thread waiting on a monitor.
 *
 * This function serves as the entry point for a
 * thread whose sole purpose is to interrupt another
 * thread.
 *
 * @param[in] entryArg pointer to the thread to interrupt (non-NULL)
 * @return 0
 */
static int32_t J9THREAD_PROC
interruptServer(void *entryArg)
{
	omrthread_t self = MACRO_SELF();
	omrthread_t threadToInterrupt = (omrthread_t)entryArg;
	omrthread_monitor_t monitor;

	ASSERT(threadToInterrupt);
	ASSERT(self);

	GLOBAL_LOCK(self, CALLER_INTERRUPT_SERVER);

	/*
	 * Did the thread to interrupt die or come out of wait already?
	 * If it did, it cancelled this thread (set our CANCELED bit)
	 */
	if (self->flags & J9THREAD_FLAG_CANCELED) {
		GLOBAL_UNLOCK(self);
		omrthread_exit(NULL); /* this should not return */
	}

	THREAD_LOCK(threadToInterrupt, CALLER_INTERRUPT_SERVER);

	if (threadToInterrupt->interrupter != self) {
		THREAD_UNLOCK(threadToInterrupt);
		GLOBAL_UNLOCK(self);
		omrthread_exit(NULL); /* this should not return */
	}

	monitor = threadToInterrupt->monitor;
	ASSERT(monitor);

	/* This assertion is bogus. If the thread was notified after being interrupted,
	 * it could still have an interrupter but would not be WAITING
	 * ASSERT(threadToInterrupt->flags & J9THREAD_FLAG_WAITING);
	 */

	omrthread_monitor_pin(monitor, self);
	THREAD_UNLOCK(threadToInterrupt);
	GLOBAL_UNLOCK(self);

	/* try to take the monitor so that we can notify the thread to interrupt */
	omrthread_monitor_enter(monitor);

	GLOBAL_LOCK(self, CALLER_INTERRUPT_SERVER);
	omrthread_monitor_unpin(monitor, self);

	/* Did the thread to interrupt die or come out of wait already? */
	if (self->flags & J9THREAD_FLAG_CANCELED) {
		GLOBAL_UNLOCK(self);
		omrthread_exit(monitor); /* this should not return */
		ASSERT(0);
	}

	THREAD_LOCK(threadToInterrupt, CALLER_INTERRUPT_SERVER);
	if ((threadToInterrupt->interrupter == self) && (threadToInterrupt->flags & J9THREAD_FLAG_WAITING)) {
		threadInterruptWake(threadToInterrupt, monitor);
	}
	threadToInterrupt->interrupter = NULL;
	ASSERT(threadToInterrupt->flags & (J9THREAD_FLAG_INTERRUPTED | J9THREAD_FLAG_ABORTED));
	THREAD_UNLOCK(threadToInterrupt);

	GLOBAL_UNLOCK(self);
	omrthread_exit(monitor);

	ASSERT(0);
	return 0;
}

/*
 * !!! NOTE !!!
 * The prev pointers in a thread queue form a circular list.
 * The next pointers do not.
 */

/**
 * Remove a thread from a monitor's queue.
 *
 * @param[in] queue head of a monitor's queue
 * @param[in] thread thread to be removed from queue
 * @return none
 */
static void
threadDequeue(omrthread_t volatile *queue, omrthread_t thread)
{
	omrthread_t queued, next, prev;

	ASSERT(thread);

	if ((queued = *queue) == NULL) {
		return;
	}
	next = thread->next;
	prev = thread->prev;

	if (queued == thread) {
		*queue = next;
		if (*queue) {
			(*queue)->prev = prev; /* circular */
		}
	} else {
		prev->next = next;
		if (next == NULL) {
			(*queue)->prev = prev; /* circular */
		} else {
			next->prev = prev;  /* circular */
		}
	}
	thread->next = NULL;
	thread->prev = NULL;

	ASSERT(NULL == thread->next);
}


/**
 * Add a thread to a monitor's wait queue.
 *
 * @note The calling thread must be the current owner of the monitor
 * @param[in] queue head of the monitor's wait queue
 * @param[in] thread thread to be added
 * @return none
 *
 */
static void
threadEnqueue(omrthread_t *queue, omrthread_t thread)
{
	omrthread_t qthread = *queue;
	omrthread_t qtail;

	ASSERT(thread);
	/* can't be on two queues at the same time */
	ASSERT(NULL == thread->next);

	if (qthread != NULL) {
		qtail = qthread->prev;
		qtail->next = thread;
		thread->prev = qtail;
		qthread->prev = thread;  /* circular */
	} else {
		*queue = thread;
		thread->prev = thread;  /* circular */
	}

	ASSERT(*queue != NULL);
	ASSERT(NULL == thread->next);
}

/**
 * Notify a thread.
 *
 * Helper routine because we notify a thread in
 * a couple of places.
 * @note: assumes the caller has THREAD_LOCK'd the
 * thread being notified (and owns the monitor being notified on)
 * @param[in] threadToNotify thread to notify
 * @param[in] setNotifiedFlag indicates whether to set the notified thread's notified flag.
 * @return none
 */
static void
threadNotify(omrthread_t threadToNotify)
{
	ASSERT(threadToNotify);
	ASSERT(threadToNotify->flags & J9THREAD_FLAG_WAITING);

	threadToNotify->flags &= ~J9THREAD_FLAG_WAITING;
	threadToNotify->flags |= J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED;
	NOTIFY_WRAPPER(threadToNotify);
}

/**
 * Wake a thread that's being interrupted.
 *
 * @param[in] thread thread to interrupt
 * @param[in] monitor the monitor that the thread is waiting on
 * @return none
 * @note: assumes the caller owns the monitor and has THREAD_LOCK()'d the thread
 */
static void
threadInterruptWake(omrthread_t thread, omrthread_monitor_t monitor)
{
	ASSERT(thread);
	ASSERT(thread->flags & J9THREAD_FLAG_WAITING);
	ASSERT(0 != monitor);

	thread->flags |= J9THREAD_FLAG_BLOCKED;
	NOTIFY_WRAPPER(thread);
}

/**
 * 'Park' the current thread.
 *
 * Stop the current thread from executing until it is unparked, interrupted, or the specified timeout elapses.
 *
 * Unlike wait or sleep, the interrupted flag is NOT cleared by this API.
 *
 * @param[in] millis
 * @param[in] nanos
 *
 * @return 0 if the thread is unparked
 * J9THREAD_INTERRUPTED if the thread was interrupted while parked<br>
 * J9THREAD_PRIORITY_INTERRUPTED if the thread was priority interrupted or aborted while parked<br>
 * J9THREAD_TIMED_OUT if the timeout expired<br>
 *
 * @see omrthread_unpark
 */
intptr_t
omrthread_park(int64_t millis, intptr_t nanos)
{
	intptr_t rc = 0;
	omrthread_t self = MACRO_SELF();
	ASSERT(self);

	THREAD_LOCK(self, CALLER_PARK);

	if (self->flags & J9THREAD_FLAG_UNPARKED) {
		self->flags &= ~J9THREAD_FLAG_UNPARKED;
	} else if (self->flags & J9THREAD_FLAG_INTERRUPTED) {
		rc = J9THREAD_INTERRUPTED;
	} else if (self->flags & (J9THREAD_FLAG_PRIORITY_INTERRUPTED | J9THREAD_FLAG_ABORTED)) {
		rc = J9THREAD_PRIORITY_INTERRUPTED;
	} else {
		self->flags |= J9THREAD_FLAGM_PARKED_INTERRUPTIBLE;

		if (millis || nanos) {
			intptr_t boundedMillis = BOUNDED_I64_TO_IDATA(millis);

			self->flags |= J9THREAD_FLAG_TIMER_SET;

			J9OSCOND_WAIT_IF_TIMEDOUT(self->condition, self->mutex, boundedMillis, nanos) {
				rc = J9THREAD_TIMED_OUT;
				break;
			} else if (self->flags & J9THREAD_FLAG_UNPARKED) {
				self->flags &= ~J9THREAD_FLAG_UNPARKED;
				break;
			} else if (self->flags & J9THREAD_FLAG_INTERRUPTED) {
				rc = J9THREAD_INTERRUPTED;
				break;
			} else if (self->flags & (J9THREAD_FLAG_PRIORITY_INTERRUPTED | J9THREAD_FLAG_ABORTED)) {
				rc = J9THREAD_PRIORITY_INTERRUPTED;
				break;
			}
			J9OSCOND_WAIT_TIMED_LOOP();
		} else {
			J9OSCOND_WAIT(self->condition, self->mutex);
				if (self->flags & J9THREAD_FLAG_UNPARKED) {
					self->flags &= ~J9THREAD_FLAG_UNPARKED;
					break;
				} else if (self->flags & J9THREAD_FLAG_INTERRUPTED) {
					rc = J9THREAD_INTERRUPTED;
					break;
				} else if (self->flags & (J9THREAD_FLAG_PRIORITY_INTERRUPTED | J9THREAD_FLAG_ABORTED)) {
					rc = J9THREAD_PRIORITY_INTERRUPTED;
					break;
				}
			J9OSCOND_WAIT_LOOP();
		}
	}

	self->flags &= ~(J9THREAD_FLAGM_PARKED_INTERRUPTIBLE | J9THREAD_FLAG_TIMER_SET);

	THREAD_UNLOCK(self);

	return rc;
}


/**
 * 'Unpark' the specified thread.
 *
 * If the thread is parked, it will return from park.
 * If the thread is not parked, its 'UNPARKED' flag will be set, and it will return immediately the next time it is parked.
 *
 * Note that unparks are not counted. Unparking a thread once is the same as unparking it n times.
 *
 * @see omrthread_park
 */
void
omrthread_unpark(omrthread_t thread)
{
	ASSERT(thread);

	THREAD_LOCK(thread, CALLER_UNPARK_THREAD);

	thread->flags |= J9THREAD_FLAG_UNPARKED;

	if (thread->flags & J9THREAD_FLAG_PARKED) {
		NOTIFY_WRAPPER(thread);
	}

	THREAD_UNLOCK(thread);
}


/**
 * Return the remaining useable bytes of the current thread's OS stack.
 *
 * @return OS stack free size in bytes, 0 if it cannot be determined.
 */
uintptr_t
omrthread_current_stack_free(void)
{
#if defined(WIN32)
	MEMORY_BASIC_INFORMATION memInfo;
	SYSTEM_INFO sysInfo;
	uintptr_t stackFree;
	uintptr_t guardPageSize;

	GetSystemInfo(&sysInfo);
	VirtualQuery(&memInfo, &memInfo, sizeof(MEMORY_BASIC_INFORMATION));
	stackFree = ((uintptr_t)&memInfo - (uintptr_t)memInfo.AllocationBase) & ~sizeof(uintptr_t);

	/* By observation, Win32 reserves 3 pages at the low end of the stack for guard pages, so omit them */

	guardPageSize = 3 * (uintptr_t)sysInfo.dwPageSize;
	return (stackFree < guardPageSize) ? 0 : stackFree - guardPageSize;
#else
	return 0;
#endif
}


/*
 * Monitors
 */
/**
 * Acquire and initialize a new monitor from the threading library.
 *
 * @param[out] handle pointer to a omrthread_monitor_t to be set to point to the new monitor
 * @param[in] flags initial flag values for the monitor
 * @param[in] name pointer to a C string with a description of how the monitor will be used (may be NULL)<br>
 * If non-NULL, the C string must be valid for the entire life of the monitor
 *
 * @return  0 on success or negative value on failure
 *
 * @see omrthread_monitor_destroy
 *
 */
intptr_t
omrthread_monitor_init_with_name(omrthread_monitor_t *handle, uintptr_t flags, const char *name)
{
	intptr_t rc;

	rc = monitor_alloc_and_init(handle, flags, J9THREAD_LOCKING_DEFAULT, J9THREAD_LOCKING_NO_DATA, name);
	return rc;
}

/**
 * Destroy a monitor.
 *
 * Destroying a monitor frees the internal resources associated
 * with it.
 *
 * @note A monitor must NOT be destroyed if threads are waiting on
 * it, or if it is currently owned.
 *
 * @param[in] monitor a monitor to be destroyed
 * @return  0 on success or non-0 on failure (the monitor is in use)
 *
 * @see omrthread_monitor_init_with_name
 */
intptr_t
omrthread_monitor_destroy(omrthread_monitor_t monitor)
{
	omrthread_t self = MACRO_SELF();

	ASSERT(self);
	ASSERT(monitor);

	GLOBAL_LOCK(self, CALLER_MONITOR_DESTROY);

#ifdef OMR_THR_TRACING
	omrthread_monitor_dump_trace(monitor);
#endif

	if (monitor->owner || monitor_maximum_wait_number(monitor)) {
		/* This monitor is in use! It was probably abandoned when a thread was cancelled.
		 * There's actually a very small timing hole here -- if the thread had just locked the
		 * mutex and not yet set the owner field when it was cancelled, we have no way of
		 * knowing that the mutex may be in an invalid state. The same thing can happen
		 * if the thread has just cleared the field and is about to unlock the mutex.
		 * Hopefully the OS takes care of this for us, but it might not.
		 */
		GLOBAL_UNLOCK(self);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	monitor_free(self->library, monitor);

	GLOBAL_UNLOCK(self);
	return 0;
}

/**
 * Destroy a monitor.  Destroying a monitor frees the OS mutex and links the monitor onto a thread
 * local list.  omrthread_monitor_flush_destroyed_monitor_list() MUST be called to flush the thread
 * local list to the global list.  The JLM data for this monitor is not destroyed.
 *
 * @note A monitor must NOT be destroyed if threads are waiting on  it, or if it is currently owned.
 * @note This API can only be called by the GC for object monitor destruction while the GC holds exclusive VM access
 * @note This API can NOT be called on RAW monitors
 * @note This API can only be called for object monitors whose object has died
 *
 * @param[in] self the thread calling this function
 * @param[in] monitor a monitor to be destroyed
 * @return  0 on success or non-0 on failure (the monitor is in use)
 */
intptr_t
omrthread_monitor_destroy_nolock(omrthread_t self, omrthread_monitor_t monitor)
{
	ASSERT(self);
	ASSERT(monitor);

#ifdef OMR_THR_TRACING
	omrthread_monitor_dump_trace(monitor);
#endif

	if (monitor->owner || monitor_maximum_wait_number(monitor)) {
		/* This monitor is in use! It was probably abandoned when a thread was cancelled.
		 * There's actually a very small timing hole here -- if the thread had just locked the
		 * mutex and not yet set the owner field when it was cancelled, we have no way of
		 * knowing that the mutex may be in an invalid state. The same thing can happen
		 * if the thread has just cleared the field and is about to unlock the mutex.
		 * Hopefully the OS takes care of this for us, but it might not.
		 */
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	monitor_free_nolock(self->library, self, monitor);

	return 0;
}

/**
 * Flush the thread local list of destroyed monitors to the global list under the GLOBAL_LOCK.
 * Monitors can only be added to the thread local list via omrthread_monitor_destroy_nolock
 *
 * @note This API can only be called by the GC at the end of object monitor clearing while the GC holds exclusive VM access
 *
 * @param[in] self thread thread calling this function
 */
void
omrthread_monitor_flush_destroyed_monitor_list(omrthread_t self)
{
	omrthread_library_t lib = self->library;
	omrthread_monitor_t cacheTail = NULL;

	ASSERT(self);
	ASSERT(lib);

	if (0 != self->destroyed_monitor_head) {
		GLOBAL_LOCK(self, CALLER_MONITOR_DESTROY);

		cacheTail = self->destroyed_monitor_tail;

		ASSERT(cacheTail);

		cacheTail->owner = (omrthread_t)lib->monitor_pool->next_free;
		lib->monitor_pool->next_free = self->destroyed_monitor_head;

		self->destroyed_monitor_head = 0;
		self->destroyed_monitor_tail = 0;

		GLOBAL_UNLOCK(self);
	}
}

/**
 * Acquire a monitor from the threading library. (Private)
 *
 * @param[in] self current thread
 * @param[in] locking policy
 * @param[in] locking policy data or J9THREAD_LOCKING_NO_DATA
 * @return NULL on failure, non-NULL on success
 *
 * @see omrthread_monitor_init_with_name, omrthread_monitor_destroy
 *
 *
 */
static omrthread_monitor_t
monitor_allocate(omrthread_t self, intptr_t policy, intptr_t policyData)
{
	omrthread_monitor_t newMonitor;
	omrthread_library_t lib = self->library;
	omrthread_monitor_pool_t pool = lib->monitor_pool;
	intptr_t rc;

	ASSERT(self);
	ASSERT(lib);
	ASSERT(pool);

	GLOBAL_LOCK(self, CALLER_MONITOR_ACQUIRE);

	newMonitor = pool->next_free;
	if (newMonitor == NULL) {
		omrthread_monitor_pool_t last_pool = pool;
		while (last_pool->next != NULL) {
			last_pool = last_pool->next;
		}
		last_pool->next = allocate_monitor_pool(lib);
		if (last_pool->next == NULL) {
			/* failed to grow monitor pool */
			GLOBAL_UNLOCK(self);
			return NULL;
		}
		newMonitor = last_pool->next->next_free;
	}


	/* the first time that a mutex is acquired from the pool, we need to
	 * initialize its mutex
	 */
	if (newMonitor->flags == J9THREAD_MONITOR_MUTEX_UNINITIALIZED) {
		rc = J9OSMUTEX_INIT(newMonitor->mutex);
		if (!rc) {
			/* failed to initialize mutex */
			ASSERT_DEBUG(0);
			GLOBAL_UNLOCK(self);
			return NULL;
		}

		newMonitor->flags = 0;
	}

	pool->next_free = (omrthread_monitor_t)newMonitor->owner;
	newMonitor->count = 0;

#if	defined(OMR_THR_JLM)
	if (IS_JLM_ENABLED(self)) {
		if (NULL == newMonitor->tracing) {
			if (jlm_monitor_init(lib, newMonitor) != 0) {
				monitor_free(lib, newMonitor);
				newMonitor = NULL;
			}
		} else {
			jlm_monitor_clear(lib, newMonitor);
		}
	}
#endif

	GLOBAL_UNLOCK(self);

	return newMonitor;
}


/**
 * Return a omrthread_monitor_t to the monitor pool.
 *
 * Must be called under the protection of GLOBAL LOCK
 *
 * @param[in] monitor monitor to be returned to the pool

 * @return none
 */
static void
monitor_free(omrthread_library_t lib, omrthread_monitor_t monitor)
{
	ASSERT(lib);
	ASSERT(lib->monitor_pool);
	ASSERT(monitor);

#ifdef OMR_THR_JLM
	jlm_monitor_free(lib, monitor);
#endif

	monitor->owner = (omrthread_t)lib->monitor_pool->next_free;
	monitor->count = FREE_TAG;
	monitor->userData = 0;

	/* CMVC 112926 Delete the name if we copied it */
	if (monitor->flags & J9THREAD_MONITOR_NAME_COPY) {
		if (monitor->name) {
			omrthread_free_memory(lib, monitor->name);
		}
		monitor->name = NULL;
		monitor->flags &= ~J9THREAD_MONITOR_NAME_COPY;
	}

	/* CMVC 144063 J9VM is "leaking" handles */
	if ((monitor->flags & J9THREAD_MONITOR_MUTEX_UNINITIALIZED) == 0) {
		J9OSMUTEX_DESTROY(monitor->mutex);
		monitor->flags = J9THREAD_MONITOR_MUTEX_UNINITIALIZED;
	}

	lib->monitor_pool->next_free = monitor;
}

/**
 * Return a omrthread_monitor_t to the monitor pool.
 *
 * @param[in] lib a pointer to the thread library
 * @param[in] thread the thread which is currently freeing this monitor
 * @param[in] monitor monitor to be returned to the pool
 *
 * @return none
 */
static void
monitor_free_nolock(omrthread_library_t lib, omrthread_t thread, omrthread_monitor_t monitor)
{
	ASSERT(lib);
	ASSERT(thread);
	ASSERT(monitor);

	monitor->owner = (omrthread_t)thread->destroyed_monitor_head;
	monitor->count = FREE_TAG;
	monitor->userData = 0;

	/* CMVC 112926 Delete the name if we copied it */
	if (monitor->flags & J9THREAD_MONITOR_NAME_COPY) {
		if (monitor->name) {
			omrthread_free_memory(lib, monitor->name);
		}
		monitor->name = NULL;
		monitor->flags &= ~J9THREAD_MONITOR_NAME_COPY;
	}

	/* CMVC 144063 J9VM is "leaking" handles */
	if (lib->flags & J9THREAD_LIB_FLAG_DESTROY_MUTEX_ON_MONITOR_FREE) {
		if ((monitor->flags & J9THREAD_MONITOR_MUTEX_UNINITIALIZED) == 0) {
			J9OSMUTEX_DESTROY(monitor->mutex);
			monitor->flags = J9THREAD_MONITOR_MUTEX_UNINITIALIZED;
		}
	}

	if (0 == thread->destroyed_monitor_head) {
		thread->destroyed_monitor_tail = monitor;
	}
	thread->destroyed_monitor_head = monitor;
}


/**
 * Re-initialize the 'simple' fields of a monitor
 * that has been initialized previously, but is now
 * being re-used.
 *
 * @param[in] monitor monitor to be initialized
 * @return 0 on success or negative value on failure.
 * @see omrthread_monitor_init_with_name
 *
 */
static intptr_t
monitor_init(omrthread_monitor_t monitor, uintptr_t flags, omrthread_library_t lib, const char *name)
{
	ASSERT(monitor);
	ASSERT(lib);

	monitor->count = 0;
	monitor->owner = NULL;
	monitor->waiting = NULL;
	monitor->flags = flags;
#if (defined(OMR_THR_ADAPTIVE_SPIN))
	/* Default to no sampling. */
	monitor->flags |= J9THREAD_MONITOR_STOP_SAMPLING;
#endif
	monitor->userData = 0;
	monitor->name = NULL;
	monitor->pinCount = 0;

#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
	monitor->customSpinOptions = NULL;
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */

#ifdef OMR_THR_THREE_TIER_LOCKING
	monitor->blocking = NULL;
	monitor->spinlockState = J9THREAD_MONITOR_SPINLOCK_UNOWNED;

	/* check if we should spin on system monitors that are backing a Object monitor
	 * the default is now that we do not spin.
	 */
	if ((J9THREAD_MONITOR_OBJECT != (flags & J9THREAD_MONITOR_OBJECT))
		|| J9_ARE_ALL_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_SECONDARY_SPIN_OBJECT_MONITORS_ENABLED)
	) {
		monitor->flags |= J9THREAD_MONITOR_TRY_ENTER_SPIN;
	}

	monitor->spinCount1 = lib->defaultMonitorSpinCount1;
	monitor->spinCount2 = lib->defaultMonitorSpinCount2;
	monitor->spinCount3 = lib->defaultMonitorSpinCount3;
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	monitor->spinThreads = 0;
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */

	ASSERT(monitor->spinCount1 != 0);
	ASSERT(monitor->spinCount2 != 0);
	ASSERT(monitor->spinCount3 != 0);

#endif

	if (name) {
		if (monitor->flags & J9THREAD_MONITOR_NAME_COPY) {
			uintptr_t length = strlen(name);

			/* Allow names that are empty strings because, for example,
			 * JVMTI might allow users to specify such a string.
			 */
			monitor->name = (char *)omrthread_allocate_memory(lib, length + 1, OMRMEM_CATEGORY_THREADS);
			if (NULL == monitor->name) {
				return -1;
			}
			strcpy(monitor->name, name);
		} else {
			monitor->name = (char *)name;
		}
	}

	return 0;
}

static intptr_t
monitor_alloc_and_init(omrthread_monitor_t *handle, uintptr_t flags, intptr_t policy, intptr_t policyData, const char *name)
{
	omrthread_monitor_t monitor;
	omrthread_t self = MACRO_SELF();

	ASSERT(self);
	ASSERT(handle);

	monitor = monitor_allocate(self, policy, policyData);
	if (NULL == monitor) {
		return -1;
	}

	if (monitor_init(monitor, flags, self->library, name) != 0) {
		GLOBAL_LOCK(self, 0);
		monitor_free(self->library, monitor);
		GLOBAL_UNLOCK(self);
		return -1;
	}

	*handle = monitor;
	return 0;
}

/**
 * Enter a monitor. Not abortable.
 *
 * A thread may re-enter a monitor it owns multiple times, but must
 * exit the monitor the same number of times before any other thread
 * wanting to enter the monitor is permitted to continue.
 *
 * @param[in] monitor a monitor to be entered
 * @return 0 on success
 *
 * @see omrthread_monitor_enter_using_threadId
 * @see omrthread_monitor_enter_abortable_using_threadId
 * @see omrthread_monitor_exit, omrthread_monitor_exit_using_threadId
 */
intptr_t
omrthread_monitor_enter(omrthread_monitor_t monitor)
{
	omrthread_t self = MACRO_SELF();

	ASSERT(self);
	ASSERT(monitor);
	ASSERT(FREE_TAG != monitor->count);
	ASSERT(0 == self->monitor);

	if (monitor->owner == self) {
		ASSERT(monitor->count >= 1);
		monitor->count++;
		UPDATE_JLM_MON_ENTER(self, monitor, IS_RECURSIVE_ENTER, !IS_SLOW_ENTER);
		return 0;
	}

#ifdef OMR_THR_THREE_TIER_LOCKING
	return monitor_enter_three_tier(self, monitor, DONT_SET_ABORTABLE);
#else
	return monitor_enter(self, monitor);
#endif
}



/**
 * Enter a monitor. Not abortable.
 *
 * This is a slightly faster version of omrthread_monitor_enter because
 * the omrthread_t for the current thread doesn't have to be looked up.
 *
 * @param[in] monitor a monitor to be entered
 * @param[in] threadId omrthread_t for the current thread
 * @return 0 on success
 *
 * @see omrthread_monitor_enter, omrthread_monitor_enter_abortable_using_threadId
 * @see omrthread_monitor_exit, omrthread_monitor_exit_using_threadId
 */
intptr_t
omrthread_monitor_enter_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId)
{
	ASSERT(threadId != 0);
	ASSERT(threadId == MACRO_SELF());
	ASSERT(monitor);
	ASSERT(FREE_TAG != monitor->count);
	ASSERT(0 == threadId->monitor);

	if (monitor->owner == threadId) {
		ASSERT(monitor->count >= 1);
		monitor->count++;
		UPDATE_JLM_MON_ENTER(threadId, monitor, IS_RECURSIVE_ENTER, !IS_SLOW_ENTER);
		return 0;
	}

#ifdef OMR_THR_THREE_TIER_LOCKING
	return monitor_enter_three_tier(threadId, monitor, DONT_SET_ABORTABLE);
#else
	return monitor_enter(threadId, monitor);
#endif
}

/**
 * Enter a monitor. This is an abortable version of omrthread_monitor_enter.
 *
 * @param[in] monitor a monitor to be entered
 * @param[in] threadId omrthread_t for the current thread
 * @return 0 on success<br>
 * J9THREAD_INTERRUPTED_MONITOR_ENTER if the thread was aborted while blocked
 *
 * @see omrthread_monitor_enter, omrthread_monitor_enter_using_threadId
 * @see omrthread_monitor_exit, omrthread_monitor_exit_using_threadId
 * @see omrthread_abort
 */
intptr_t
omrthread_monitor_enter_abortable_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId)
{
	ASSERT(threadId != 0);
	ASSERT(threadId == MACRO_SELF());
	ASSERT(monitor);
	ASSERT(FREE_TAG != monitor->count);
	ASSERT(0 == threadId->monitor);

	if (monitor->owner == threadId) {
		ASSERT(monitor->count >= 1);
		monitor->count++;
		UPDATE_JLM_MON_ENTER(threadId, monitor, IS_RECURSIVE_ENTER, !IS_SLOW_ENTER);
		return 0;
	}

#ifdef OMR_THR_THREE_TIER_LOCKING
	return monitor_enter_three_tier(threadId, monitor, SET_ABORTABLE);
#else
	return monitor_enter(threadId, monitor);
#endif
}


#if !defined(OMR_THR_THREE_TIER_LOCKING)
/**
 * Enter a monitor.
 *
 * A thread may enter a monitor it owns multiple times, but must
 * exit the monitor the same number of times before other threads
 * waiting on the monitor are permitted to continue.
 *
 * @param[in] self current thread
 * @param[in] monitor monitor to enter
 * @return 0 on success
 * @todo Get JLM code out of here
 */
static intptr_t
monitor_enter(omrthread_t self, omrthread_monitor_t monitor)
{
	ASSERT(self);
	ASSERT(0 == self->monitor);
	ASSERT(monitor);
	ASSERT(monitor->owner != self);
	ASSERT(FREE_TAG != monitor->count);

	self->lockedmonitorcount++; /* one more locked monitor on this thread */

	THREAD_LOCK(self, CALLER_MONITOR_ENTER1);
	self->flags |= (J9THREAD_FLAG_BLOCKED);
	self->monitor = monitor;
	THREAD_UNLOCK(self);

	MONITOR_LOCK(monitor, CALLER_MONITOR_ENTER);

	UPDATE_JLM_MON_ENTER(self, monitor, !IS_RECURSIVE_ENTER, IS_SLOW_ENTER);

	THREAD_LOCK(self, CALLER_MONITOR_ENTER2);
	self->flags &= ~J9THREAD_FLAG_BLOCKED;
	self->monitor = 0;
	THREAD_UNLOCK(self);

	ASSERT(NULL == monitor->owner);
	ASSERT(0 == monitor->count);
	monitor->owner = self;
	monitor->count = 1;

	ASSERT(0 == self->monitor);

	return 0;
}
#endif /* !defined(OMR_THR_THREE_TIER_LOCKING) */



#if (defined(OMR_THR_THREE_TIER_LOCKING))
/**
 * Enter a three-tier monitor.
 *
 * Spin on a spinlock. Block when that fails, and repeat.
 *
 * @param[in] self current thread
 * @param[in] monitor monitor to enter
 * @return 0 on success, J9THREAD_INTERRUPTED_MONITOR_ENTER otherwise
 * @todo Get JLM code out of here
 */
static intptr_t
monitor_enter_three_tier(omrthread_t self, omrthread_monitor_t monitor, BOOLEAN isAbortable)
{
	int blockedCount = 0;

	ASSERT(self);
	ASSERT(monitor);
	ASSERT(monitor->spinCount1 != 0);
	ASSERT(monitor->spinCount2 != 0);
	ASSERT(monitor->spinCount3 != 0);
	ASSERT(monitor->owner != self);
	ASSERT(FREE_TAG != monitor->count);

	while (1) {

		if (omrthread_spinlock_acquire(self, monitor) == 0) {
			monitor->owner = self;
			monitor->count = 1;
			ASSERT(monitor->spinlockState != J9THREAD_MONITOR_SPINLOCK_UNOWNED);
			break;
		}

		MONITOR_LOCK(monitor, CALLER_MONITOR_ENTER_THREE_TIER1);

		if (J9THREAD_MONITOR_SPINLOCK_UNOWNED == omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_EXCEEDED)) {
			MONITOR_UNLOCK(monitor);
			monitor->owner = self;
			monitor->count = 1;
			ASSERT(monitor->spinlockState != J9THREAD_MONITOR_SPINLOCK_UNOWNED);
			break;
		}

		blockedCount++;

		THREAD_LOCK(self, CALLER_MONITOR_ENTER_THREE_TIER2);
		/*
		 * Check for abort before blocking.
		 * Catches aborts that occur before we start waiting.
		 */
		if (SET_ABORTABLE == isAbortable) {
			if (self->flags & J9THREAD_FLAG_ABORTED) {
				self->flags &= ~J9THREAD_FLAGM_BLOCKED_ABORTABLE;
				self->monitor = 0;
				THREAD_UNLOCK(self);
				MONITOR_UNLOCK(monitor);
				return J9THREAD_INTERRUPTED_MONITOR_ENTER;
			}
		}

		if (SET_ABORTABLE == isAbortable) {
			self->flags |= J9THREAD_FLAGM_BLOCKED_ABORTABLE;
		} else {
			self->flags |= J9THREAD_FLAG_BLOCKED;
		}
		self->monitor = monitor;
		THREAD_UNLOCK(self);

		threadEnqueue(&monitor->blocking, self);
		J9OSCOND_WAIT(self->condition, monitor->mutex);
			break;
		J9OSCOND_WAIT_LOOP();
		threadDequeue(&monitor->blocking, self);

		/*
		 * Check for abort upon waking.
		 * If aborted, we shouldn't continue to contend for the monitor.
		 */
		if (SET_ABORTABLE == isAbortable) {
			THREAD_LOCK(self, CALLER_MONITOR_ENTER_THREE_TIER4);
			if (self->flags & J9THREAD_FLAG_ABORTED) {
				self->flags &= ~J9THREAD_FLAGM_BLOCKED_ABORTABLE;
				self->monitor = 0;
				THREAD_UNLOCK(self);
				MONITOR_UNLOCK(monitor);
				return J9THREAD_INTERRUPTED_MONITOR_ENTER;
			}
			THREAD_UNLOCK(self);
		}

		MONITOR_UNLOCK(monitor);
	}

	/* We now own the monitor */
	self->lockedmonitorcount++;

	/*
	 * If the monitor field is set, we must have blocked on it
	 * at some point. We're no longer blocked, so clear this.
	 */
	if ((self->monitor != 0) || (SET_ABORTABLE == isAbortable)) {
		THREAD_LOCK(self, CALLER_MONITOR_ENTER_THREE_TIER3);
		self->flags &= ~J9THREAD_FLAGM_BLOCKED_ABORTABLE;
		self->monitor = 0;

		/* Check for abort that may have occurred after we got the monitor. */
		if (self->flags & J9THREAD_FLAG_ABORTED) {
			THREAD_UNLOCK(self);
			monitor_exit(self, monitor);
			return J9THREAD_INTERRUPTED_MONITOR_ENTER;
		}
		THREAD_UNLOCK(self);
	}

	UPDATE_JLM_MON_ENTER(self, monitor, !IS_RECURSIVE_ENTER, (blockedCount > 0));

	ASSERT(!(self->flags & J9THREAD_FLAG_BLOCKED));
	ASSERT(0 == self->monitor);

	return 0;
}
#endif /* OMR_THR_THREE_TIER_LOCKING */


#if (defined(OMR_THR_THREE_TIER_LOCKING))
/**
 * Notify all threads blocked on the monitor's mutex, waiting
 * to be told that it's ok to try again to get the spinlock.
 *
 * Assumes that the caller already owns the monitor's mutex.
 *
 */
static void
unblock_spinlock_threads(omrthread_t self, omrthread_monitor_t monitor)
{
	omrthread_t queue, next;
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	uintptr_t i = self->library->maxWakeThreads;
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */

	ASSERT(self);
	ASSERT(monitor);

	next = monitor->blocking;
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	for (; (NULL != next) && (i > 0); i--)
#else /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
	while (NULL != next)
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
	{
		queue = next;
		next = queue->next;
		NOTIFY_WRAPPER(queue);
		Trc_THR_ThreadSpinLockThreadUnblocked(self, queue, monitor);
	}
}

#endif /* OMR_THR_THREE_TIER_LOCKING */



/**
 * Attempt to enter a monitor without blocking.
 *
 * If the thread must block before it enters the monitor this function
 * returns immediately with a negative value to indicate failure.
 *
 * @param[in] monitor a monitor
 * @return  0 on success or negative value on failure
 *
 * @see omrthread_monitor_try_enter_using_threadId
 *
 */
intptr_t
omrthread_monitor_try_enter(omrthread_monitor_t monitor)
{
	return omrthread_monitor_try_enter_using_threadId(monitor, MACRO_SELF());
}



/**
 * Attempt to enter a monitor without blocking.
 *
 * If the thread must block before it enters the monitor this function
 * returns immediately with a negative value to indicate failure.<br>
 *
 * This is a slightly faster version of omrthread_monitor_try_enter because
 * the current thread's omrthread_t doesn't have to be looked up.
 *
 * @param[in] monitor a monitor
 * @param[in] threadId the current thread
 * @return  0 on success or negative value on failure
 *
 * @see omrthread_monitor_try_enter
 *
 */
intptr_t
omrthread_monitor_try_enter_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId)
{
	intptr_t lockAcquired = -1;
	ASSERT(threadId != 0);
	ASSERT(threadId == MACRO_SELF());
	ASSERT(FREE_TAG != monitor->count);

	/* Are we already the owner? */
	if (monitor->owner == threadId) {
		ASSERT(monitor->count >= 1);
		monitor->count++;
		UPDATE_JLM_MON_ENTER(threadId, monitor, IS_RECURSIVE_ENTER, !IS_SLOW_ENTER);
		return 0;
	}
#if defined(OMR_THR_THREE_TIER_LOCKING)
	if (J9THREAD_MONITOR_TRY_ENTER_SPIN == (monitor->flags & J9THREAD_MONITOR_TRY_ENTER_SPIN)) {
		lockAcquired = omrthread_spinlock_acquire(threadId, monitor);
	} else {
		lockAcquired = omrthread_spinlock_acquire_no_spin(threadId, monitor);
	}
#else
	lockAcquired = MONITOR_TRY_LOCK(monitor);
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

	if (0 == lockAcquired) {
		ASSERT(NULL == monitor->owner);
		ASSERT(0 == monitor->count);

		monitor->owner = threadId;
		monitor->count = 1;

		threadId->lockedmonitorcount++;

		UPDATE_JLM_MON_ENTER(threadId, monitor, !IS_RECURSIVE_ENTER, !IS_SLOW_ENTER);

		return 0;
	}

	return -1;
}


/**
 * Exit a monitor.
 *
 * Exit a monitor, and if the owning count is zero, release it.
 *
 * @param[in] monitor a monitor to be exited
 * @return 0 on success, <br>J9THREAD_ILLEGAL_MONITOR_STATE if the current thread does not own the monitor
 *
 * @see omrthread_monitor_exit_using_threadId, omrthread_monitor_enter, omrthread_monitor_enter_using_threadId
 */
intptr_t
omrthread_monitor_exit(omrthread_monitor_t monitor)
{
	omrthread_t self = MACRO_SELF();

	ASSERT(self);
	ASSERT(monitor);
	ASSERT(FREE_TAG != monitor->count);

	if (monitor->owner != self) {
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	return monitor_exit(self, monitor);
}



/**
 * Exit a monitor.
 *
 * This is a slightly faster version of omrthread_monitor_exit because
 * the omrthread_t for the current thread doesn't have to be looked up
 *
 * @param[in] monitor a monitor to be exited
 * @param[in] threadId omrthread_t for the current thread
 * @return 0 on success<br>
 * J9THREAD_ILLEGAL_MONITOR_STATE if the current thread does not own the monitor
 *
 * @see omrthread_monitor_exit, omrthread_monitor_enter, omrthread_monitor_enter_using_threadId
 */
intptr_t
omrthread_monitor_exit_using_threadId(omrthread_monitor_t monitor, omrthread_t threadId)
{
	ASSERT(threadId == MACRO_SELF());
	ASSERT(monitor);
	ASSERT(FREE_TAG != monitor->count);

	if (monitor->owner != threadId) {
		ASSERT_DEBUG(0);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	return monitor_exit(threadId, monitor);

}



/**
 * Exit a monitor.
 *
 * If the current thread is not the owner of the monitor, the
 * mutex is unaffected, and an error is returned. This should be
 * tested to determine if IllegalMonitorState should be
 * thrown.
 *
 * @param[in] self current thread
 * @param[in] monitor monitor to be exited
 * @return 0 on success<br>
 * J9THREAD_ILLEGAL_MONITOR_STATE if the current thread does not
 * own the monitor
 */
static intptr_t
monitor_exit(omrthread_t self, omrthread_monitor_t monitor)
{
	ASSERT(monitor);
	ASSERT(self);
	ASSERT(0 == self->monitor);

	if (monitor->owner != self) {
		ASSERT_DEBUG(0);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	monitor->count--;
	ASSERT(monitor->count >= 0);

	if (monitor->count == 0) {
		self->lockedmonitorcount--; /* one less locked monitor on this thread */
		monitor->owner = NULL;
		UPDATE_JLM_MON_EXIT(self, monitor);

#ifdef OMR_THR_THREE_TIER_LOCKING
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
		omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_UNOWNED);
 		MONITOR_LOCK(monitor, CALLER_MONITOR_EXIT1);
 		if (0 == monitor->spinThreads) {
 			unblock_spinlock_threads(self, monitor);
 		}
 		MONITOR_UNLOCK(monitor);
#else /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
		if (J9THREAD_MONITOR_SPINLOCK_EXCEEDED == omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_UNOWNED)) {
			MONITOR_LOCK(monitor, CALLER_MONITOR_EXIT1);
			unblock_spinlock_threads(self, monitor);
			MONITOR_UNLOCK(monitor);
		}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
#else
		MONITOR_UNLOCK(monitor);
#endif
	}

	return 0;
}

/**
 * Wait on a monitor until notified.
 *
 * Release the monitor, wait for a signal (notification), then re-acquire the monitor.
 * The wait may not be interrupted.
 *
 * @param[in] monitor a monitor to be waited on
 *
 * @return status code
 * @retval 0 The monitor has been waited on, notified, and reobtained.
 * @retval J9THREAD_ILLEGAL_MONITOR_STATE The current thread does not own the monitor.
 *
 * @see omrthread_monitor_wait_interruptable, omrthread_monitor_wait_timed, omrthread_monitor_wait_abortable
 * @see omrthread_monitor_enter
 */
intptr_t
omrthread_monitor_wait(omrthread_monitor_t monitor)
{
	return monitor_wait(monitor, 0, 0, 0);
}

/**
 * Wait on a monitor until notified, aborted, or timed out.
 *
 * A timeout of 0 (0ms, 0ns) indicates wait indefinitely.
 *
 * The wait may be interrupted by omrthread_abort().
 *
 * @param[in] monitor a monitor to be waited on
 * @param[in] millis >=0
 * @param[in] nanos >=0
 *
 * @return status code
 * @retval 0 the monitor has been waited on, notified, and reobtained
 * @retval J9THREAD_INVALID_ARGUMENT millis or nanos is out of range (millis or nanos < 0, or nanos >= 1E6)
 * @retval J9THREAD_ILLEGAL_MONITOR_STATE the current thread does not own the monitor
 * @retval J9THREAD_PRIORITY_INTERRUPTED the thread was aborted and still holds the monitor
 * @retval J9THREAD_INTERRUPTED_MONITOR_ENTER the current thread was aborted and no longer holds the monitor
 * @retval J9THREAD_TIMED_OUT the timeout expired
 *
 * @see omrthread_monitor_wait, omrthread_monitor_wait_timed, omrthread_monitor_wait_interruptable
 * @see omrthread_monitor_enter
 * @see omrthread_abort
 */
intptr_t
omrthread_monitor_wait_abortable(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos)
{
	return monitor_wait(monitor, millis, nanos, J9THREAD_FLAG_ABORTABLE);
}

/**
 * Wait on a monitor until notified, interrupted (priority or normal), aborted, or timed out.
 *
 * A timeout of 0 (0ms, 0ns) indicates wait indefinitely.
 *
 * The wait may be interrupted by one of the interrupt functions.
 * (i.e. omrthread_interrupt, omrthread_priority_interrupt, omrthread_abort)
 *
 * @param[in] monitor a monitor to be waited on
 * @param[in] millis >=0
 * @param[in] nanos >=0
 *
 * @return status code
 * @retval 0 The monitor has been waited on, notified, and reobtained.
 * @retval J9THREAD_INVALID_ARGUMENT millis or nanos is out of range (millis or nanos < 0, or nanos >= 1E6).
 * @retval J9THREAD_ILLEGAL_MONITOR_STATE The current thread does not own the monitor.
 * @retval J9THREAD_INTERRUPTED The thread was interrupted while waiting. It has reobtained the monitor.
 * @retval J9THREAD_PRIORITY_INTERRUPTED The thread was priority interrupted or aborted and has reobtained the monitor.
 * @retval J9THREAD_INTERRUPTED_MONITOR_ENTER The current thread was aborted while reobtaining the monitor. It no longer holds the monitor.
 * @retval J9THREAD_TIMED_OUT The timeout expired.
 *
 * @see omrthread_monitor_wait, omrthread_monitor_wait_timed, omrthread_monitor_wait_abortable
 * @see omrthread_monitor_enter
 * @see omrthread_interrupt, omrthread_priority_interrupt, omrthread_abort
 */
intptr_t
omrthread_monitor_wait_interruptable(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos)
{
	return monitor_wait(monitor, millis, nanos, J9THREAD_FLAG_ABORTABLE | J9THREAD_FLAG_INTERRUPTABLE);
}

/**
 * Wait on a monitor until notified or timed out.
 *
 * A timeout of 0 (0ms, 0ns) indicates wait indefinitely.
 * The wait may not be interrupted.
 *
 * @param[in] monitor a monitor to be waited on
 * @param[in] millis >=0
 * @param[in] nanos >=0
 *
 * @return status code
 * @retval 0 The monitor has been waited on, notified, and reobtained.
 * @retval J9THREAD_INVALID_ARGUMENT millis or nanos is out of range (millis or nanos < 0, or nanos >= 1E6).
 * @retval J9THREAD_ILLEGAL_MONITOR_STATE The current thread does not own the monitor.
 * @retval J9THREAD_TIMED_OUT The timeout expired.
 *
 * @see omrthread_monitor_wait, omrthread_monitor_wait_interruptable, omrthread_monitor_wait_abortable
 * @see omrthread_monitor_enter
 */
intptr_t
omrthread_monitor_wait_timed(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos)
{
	return monitor_wait(monitor, millis, nanos, 0);
}

/**
 *
 * Wait on a monitor.
 *
 * Release the monitor, wait for a signal (notification), then re-acquire the monitor.
 *
 * In this function, we 'unwind' any recursive hold (monitor->count) the thread has
 * on the monitor and release the OS monitor. When the monitor is re-acquired,
 * the recursive count is restored to its original value.
 *
 * A timeout of 0 (0ms, 0ns) indicates wait indefinitely.
 *
 * If 'interruptible' is non-zero, the wait may be interrupted by one of the
 * interrupt functions. (i.e. omrthread_interrupt, omrthread_priority_interrupt, omrthread_abort)
 *
 * @param[in] monitor monitor to be waited on
 * @param[in] millis >=0
 * @param[in] nanos >=0
 * @param[in] interruptible bitflag that may have one or more of these bits set:<br>
 * J9THREAD_FLAG_INTERRUPTABLE, J9THREAD_FLAG_ABORTABLE
 *
 * @return status code
 * @retval 0 The monitor has been waited on, notified, and reobtained.
 * @retval J9THREAD_TIMED_OUT The timeout expired.
 * @retval J9THREAD_INVALID_ARGUMENT millis or nanos is out of range (millis or nanos<0, or nanos>=1E6).
 * @retval J9THREAD_ILLEGAL_MONITOR_STATE The current thread does not own the monitor.
 * @retval J9THREAD_INTERRUPTED The thread was interrupted.
 * @retval J9THREAD_PRIORITY_INTERRUPTED The thread was priority interrupted or aborted. The thread still owns the monitor.
 * @retval J9THREAD_INTERRUPTED_MONITOR_ENTER The current thread was aborted. the thread has released the monitor.
 *
 * @see omrthread_monitor_wait, omrthread_monitor_wait_interruptable, omrthread_monitor_enter
 * @see omrthread_interrupt, omrthread_priority_interrupt, omrthread_abort
 */
static intptr_t
monitor_wait(omrthread_monitor_t monitor, int64_t millis, intptr_t nanos, uintptr_t interruptible)
{
	omrthread_t self = MACRO_SELF();

#if defined(OMR_THR_THREE_TIER_LOCKING)
	if (self->library->flags & J9THREAD_LIB_FLAG_FAST_NOTIFY) {
		return monitor_wait_three_tier(self, monitor, millis, nanos, interruptible);
	} else {
		return monitor_wait_original(self, monitor, millis, nanos, interruptible);
	}
#else
	return monitor_wait_original(self, monitor, millis, nanos, interruptible);
#endif
}

/*
 * VMDESIGN WIP 1320
 * This is the original implementation of monitor_wait().
 * It handles both 3-tier and non-3-tier.
 * TODO: Split the 3-tier and non-3-tier implementations.
 */
static intptr_t
monitor_wait_original(omrthread_t self, omrthread_monitor_t monitor,
					  int64_t millis, intptr_t nanos, uintptr_t interruptible)
{
	omrthread_t *queue;
	intptr_t count = -1;
	uintptr_t interrupted = 0, notified = 0, priorityinterrupted = 0;
	uintptr_t intrMask = 0;
	uintptr_t intrFlags = 0;
	uintptr_t timedOut = 0;

	ASSERT(monitor);
	ASSERT(FREE_TAG != monitor->count);

	if (monitor->owner != self) {
		ASSERT_DEBUG(0);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	if ((millis < 0) || (nanos < 0) || (nanos >= 1000000)) {
		ASSERT_DEBUG(0);
		return J9THREAD_INVALID_ARGUMENT;
	}

	count = monitor->count;

	intrMask = 0;
	if (interruptible & J9THREAD_FLAG_INTERRUPTABLE) {
		intrMask |= J9THREAD_FLAG_INTERRUPTED | J9THREAD_FLAG_PRIORITY_INTERRUPTED;
	}
	if (interruptible & J9THREAD_FLAG_ABORTABLE) {
		intrMask |= J9THREAD_FLAG_ABORTED;
	}

	THREAD_LOCK(self, CALLER_MONITOR_WAIT1);
	ASSERT(0 == self->monitor);

	/*
	 * Before we wait, check if we've already been interrupted
	 */
	intrFlags = self->flags & intrMask;
	if (intrFlags & J9THREAD_FLAG_INTERRUPTED) {
		self->flags &= ~J9THREAD_FLAG_INTERRUPTED;
		THREAD_UNLOCK(self);
		return J9THREAD_INTERRUPTED;
	}
	if (intrFlags & J9THREAD_FLAG_PRIORITY_INTERRUPTED) {
		self->flags &= ~J9THREAD_FLAG_PRIORITY_INTERRUPTED;
		THREAD_UNLOCK(self);
		return J9THREAD_PRIORITY_INTERRUPTED;
	}
	if (intrFlags & J9THREAD_FLAG_ABORTED) {
		/* don't clear the flag */
		THREAD_UNLOCK(self);
		return J9THREAD_PRIORITY_INTERRUPTED;
	}

	self->flags |= (J9THREAD_FLAG_WAITING | interruptible);
	if (millis || nanos) {
		self->flags |= J9THREAD_FLAG_TIMER_SET;
	}
	self->monitor = monitor;

	THREAD_UNLOCK(self);

#if defined(OMR_THR_JLM_HOLD_TIMES)
	UPDATE_JLM_MON_WAIT(self, monitor);
#endif

	ASSERT(self->flags & J9THREAD_FLAG_WAITING);
	monitor->owner = NULL;
	monitor->count = 0;

#ifdef OMR_THR_THREE_TIER_LOCKING
	MONITOR_LOCK(monitor, CALLER_MONITOR_WAIT);
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_UNOWNED);
	if (0 == monitor->spinThreads) {
		unblock_spinlock_threads(self, monitor);
	}
#else /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
	if (J9THREAD_MONITOR_SPINLOCK_EXCEEDED == omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_UNOWNED)) {
		unblock_spinlock_threads(self, monitor);
	}
#endif  /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
	self->lockedmonitorcount--;
#endif /* defined(OMR_THR_THREE_TIER_LOCKING) */

	self->waitNumber = monitor_maximum_wait_number(monitor) + 1;
	threadEnqueue(&monitor->waiting, self);

	if (millis || nanos) {
		/*
		 * TIMED WAIT
		 */
		intptr_t boundedMillis = BOUNDED_I64_TO_IDATA(millis);

		ASSERT_MONITOR_UNOWNED_IF_NOT_3TIER(monitor);
		J9OSCOND_WAIT_IF_TIMEDOUT(MONITOR_WAIT_CONDITION(self, monitor), monitor->mutex, boundedMillis, nanos) {
			ASSERT_MONITOR_UNOWNED_IF_NOT_3TIER(monitor);

			THREAD_LOCK(self, CALLER_MONITOR_WAIT2);
			intrFlags = self->flags & intrMask;
			interrupted = J9THR_WAIT_INTERRUPTED(intrFlags);
			priorityinterrupted = J9THR_WAIT_PRI_INTERRUPTED(intrFlags);
			notified = check_notified(self, monitor);
			if (!(interrupted || priorityinterrupted || notified)) {
				timedOut = 1;
				self->flags |= J9THREAD_FLAG_BLOCKED;
			}
			break;
		} else {
			ASSERT_MONITOR_UNOWNED_IF_NOT_3TIER(monitor);

			THREAD_LOCK(self, CALLER_MONITOR_WAIT2);
			intrFlags = self->flags & intrMask;
			interrupted = J9THR_WAIT_INTERRUPTED(intrFlags);
			priorityinterrupted = J9THR_WAIT_PRI_INTERRUPTED(intrFlags);
			notified = check_notified(self, monitor);
			if (interrupted || priorityinterrupted || notified) {
				break;
			}
			THREAD_UNLOCK(self);
		}
		J9OSCOND_WAIT_TIMED_LOOP();

	} else {
		/*
		 * WAIT UNTIL NOTIFIED, NO TIMEOUT
		 */

		ASSERT_MONITOR_UNOWNED_IF_NOT_3TIER(monitor);
		J9OSCOND_WAIT(MONITOR_WAIT_CONDITION(self,monitor), monitor->mutex);
			ASSERT_MONITOR_UNOWNED_IF_NOT_3TIER(monitor);

			THREAD_LOCK(self, CALLER_MONITOR_WAIT2);
			intrFlags = self->flags & intrMask;
			interrupted = J9THR_WAIT_INTERRUPTED(intrFlags);
			priorityinterrupted = J9THR_WAIT_PRI_INTERRUPTED(intrFlags);
			notified = check_notified(self, monitor);
			if (interrupted || priorityinterrupted || notified) {
				break;
			}
			THREAD_UNLOCK(self);
		J9OSCOND_WAIT_LOOP();
	}

	/* DONE WAITING AT THIS POINT */

#ifndef OMR_THR_THREE_TIER_LOCKING
	self->monitor = 0;
#endif
	/* we have to remove self from the wait queue */
	if (monitor_on_notify_all_wait_list(self, monitor)) {
		queue = &monitor->notifyAllWaiting;
	} else {
		queue = &monitor->waiting;
	}
	self->waitNumber = 0; /* reset the wait number */
	threadDequeue(queue, self);

#ifdef OMR_THR_THREE_TIER_LOCKING
	MONITOR_UNLOCK(monitor);
#endif

	/* at this point, this thread should already be locked */

	ASSERT(notified || interrupted || priorityinterrupted || timedOut);
	/* if we were interrupted, then we'd better have been interruptible */
	ASSERT(!interrupted || (interruptible & J9THREAD_FLAG_INTERRUPTABLE));
	ASSERT(!priorityinterrupted || (interruptible & (J9THREAD_FLAG_INTERRUPTABLE | J9THREAD_FLAG_ABORTABLE)));

	self->flags &= ~(J9THREAD_FLAG_WAITING | J9THREAD_FLAG_TIMER_SET
					 | J9THREAD_FLAG_INTERRUPTABLE
					 | J9THREAD_FLAG_NOTIFIED
#ifndef OMR_THR_THREE_TIER_LOCKING
					 | J9THREAD_FLAG_BLOCKED
#endif
					);
	if (interruptible & J9THREAD_FLAG_INTERRUPTABLE) {
		self->flags &= ~J9THREAD_FLAG_PRIORITY_INTERRUPTED;
	}
	/*
	 * The interrupt remains pending if the thread was priority-interrupted or notified.
	 */
	if (interrupted && !(notified || priorityinterrupted)) {
		self->flags &= ~J9THREAD_FLAG_INTERRUPTED;
	}
	/*
	 * Don't clear J9THREAD_FLAG_ABORTED.
	 * Don't clear J9THREAD_FLAG_ABORTABLE. We don't want a hole here where the thread is
	 * not abortable until it enters monitor_enter_three_tier().
	 */

	/*
	 * Is there an interruptServer thread out there trying to interrupt us?
	 * Its services are no longer required.
	 */
	if (self->interrupter) {
		ASSERT(interrupted || priorityinterrupted);
		THREAD_LOCK(self->interrupter, CALLER_MONITOR_WAIT2);
		self->interrupter->flags |= J9THREAD_FLAG_CANCELED;
		THREAD_UNLOCK(self->interrupter);
		self->interrupter = NULL;
	}

	THREAD_UNLOCK(self);

#ifdef OMR_THR_THREE_TIER_LOCKING
	if (monitor_enter_three_tier(
			self, monitor,
			(BOOLEAN)((interruptible & J9THREAD_FLAG_ABORTABLE)? SET_ABORTABLE: DONT_SET_ABORTABLE))
		== J9THREAD_INTERRUPTED_MONITOR_ENTER
	) {
		/* we don't own the monitor */
		return J9THREAD_INTERRUPTED_MONITOR_ENTER;
	}
#else
	monitor->owner = self;
	UPDATE_JLM_MON_ENTER(self, monitor, !IS_RECURSIVE_ENTER, IS_SLOW_ENTER);
#endif
	monitor->count = count;

	ASSERT(monitor->owner == self);
	ASSERT(monitor->count == count);
	ASSERT(monitor->count >= 1);
	ASSERT(0 == self->monitor);
	ASSERT(!(monitor->flags & J9THREAD_FLAG_WAITING));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_TIMER_SET));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_BLOCKED));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_NOTIFIED));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_INTERRUPTABLE));
	ASSERT(NULL == self->next);

	if (priorityinterrupted) {
		return J9THREAD_PRIORITY_INTERRUPTED;
	}
	if (notified) {
		return 0;
	}
	if (interrupted) {
		return J9THREAD_INTERRUPTED;
	}
	if (timedOut) {
		return J9THREAD_TIMED_OUT;
	}
	ASSERT(0);
	return 0;
}

#if defined(OMR_THR_THREE_TIER_LOCKING)
static intptr_t
monitor_wait_three_tier(omrthread_t self, omrthread_monitor_t monitor,
						int64_t millis, intptr_t nanos, uintptr_t interruptible)
{
	omrthread_t *queue;
	intptr_t count = -1;
	uintptr_t interrupted = 0, notified = 0, priorityinterrupted = 0;
	uintptr_t intrMask = 0;
	uintptr_t intrFlags = 0;
	uintptr_t timedOut = 0;

	ASSERT(monitor);
	ASSERT(FREE_TAG != monitor->count);

	if (monitor->owner != self) {
		ASSERT_DEBUG(0);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	if ((millis < 0) || (nanos < 0) || (nanos >= 1000000)) {
		ASSERT_DEBUG(0);
		return J9THREAD_INVALID_ARGUMENT;
	}

	count = monitor->count;

	intrMask = 0;
	if (interruptible & J9THREAD_FLAG_INTERRUPTABLE) {
		intrMask |= J9THREAD_FLAG_INTERRUPTED | J9THREAD_FLAG_PRIORITY_INTERRUPTED;
	}
	if (interruptible & J9THREAD_FLAG_ABORTABLE) {
		intrMask |= J9THREAD_FLAG_ABORTED;
	}

	THREAD_LOCK(self, CALLER_MONITOR_WAIT1);
	ASSERT(0 == self->monitor);

	/*
	 * Before we wait, check if we've already been interrupted
	 */
	intrFlags = self->flags & intrMask;
	if (intrFlags & J9THREAD_FLAG_INTERRUPTED) {
		self->flags &= ~J9THREAD_FLAG_INTERRUPTED;
		THREAD_UNLOCK(self);
		return J9THREAD_INTERRUPTED;
	}
	if (intrFlags & J9THREAD_FLAG_PRIORITY_INTERRUPTED) {
		self->flags &= ~J9THREAD_FLAG_PRIORITY_INTERRUPTED;
		THREAD_UNLOCK(self);
		return J9THREAD_PRIORITY_INTERRUPTED;
	}
	if (intrFlags & J9THREAD_FLAG_ABORTED) {
		THREAD_UNLOCK(self);
		return J9THREAD_PRIORITY_INTERRUPTED;
	}

	self->flags |= (J9THREAD_FLAG_WAITING | interruptible);
	if (millis || nanos) {
		self->flags |= J9THREAD_FLAG_TIMER_SET;
	}
	self->monitor = monitor;

	THREAD_UNLOCK(self);

#if defined(OMR_THR_JLM_HOLD_TIMES)
	if (IS_JLM_TIME_STAMPS_ENABLED(self, monitor)) {
		UPDATE_JLM_MON_EXIT_HOLD_TIMES(self, monitor);
		/*
		 * If this is a pause monitor, increment this thread's pause count
		 * so that the hold times of currently held monitors won't be measured
		 */
		if (monitor->flags & J9THREAD_MONITOR_JLM_TIME_STAMP_INVALIDATOR) {
			self->tracing->pause_count++;
		}
	}
#endif

	ASSERT(self->flags & J9THREAD_FLAG_WAITING);
	monitor->owner = NULL;
	monitor->count = 0;

	MONITOR_LOCK(monitor, CALLER_MONITOR_WAIT);
#if defined(OMR_THR_SPIN_WAKE_CONTROL)
	omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_UNOWNED);
	if (0 == monitor->spinThreads) {
		unblock_spinlock_threads(self, monitor);
	}
#else /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
	if (J9THREAD_MONITOR_SPINLOCK_EXCEEDED == omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_UNOWNED)) {
		unblock_spinlock_threads(self, monitor);
	}
#endif /* defined(OMR_THR_SPIN_WAKE_CONTROL) */
	self->lockedmonitorcount--;

	threadEnqueue(&monitor->waiting, self);

	if (millis || nanos) {
		/*
		 * TIMED WAIT
		 */
		intptr_t boundedMillis = BOUNDED_I64_TO_IDATA(millis);

		J9OSCOND_WAIT_IF_TIMEDOUT(self->condition, monitor->mutex, boundedMillis, nanos) {
			THREAD_LOCK(self, CALLER_MONITOR_WAIT2);
			intrFlags = self->flags & intrMask;
			interrupted = J9THR_WAIT_INTERRUPTED(intrFlags);
			priorityinterrupted = J9THR_WAIT_PRI_INTERRUPTED(intrFlags);
			notified = self->flags & J9THREAD_FLAG_NOTIFIED;
			if (!(interrupted || priorityinterrupted || notified)) {
				timedOut = 1;
				self->flags |= J9THREAD_FLAG_BLOCKED;
			}
			break;
		} else {
			THREAD_LOCK(self, CALLER_MONITOR_WAIT2);
			intrFlags = self->flags & intrMask;
			interrupted = J9THR_WAIT_INTERRUPTED(intrFlags);
			priorityinterrupted = J9THR_WAIT_PRI_INTERRUPTED(intrFlags);
			notified = self->flags & J9THREAD_FLAG_NOTIFIED;
			if (interrupted || priorityinterrupted || notified) {
				break;
			}
			THREAD_UNLOCK(self);
			ASSERT(0);
		}
		J9OSCOND_WAIT_TIMED_LOOP();

	} else {
		/*
		 * WAIT UNTIL NOTIFIED, NO TIMEOUT
		 */
		J9OSCOND_WAIT(self->condition, monitor->mutex);
			THREAD_LOCK(self, CALLER_MONITOR_WAIT2);
			intrFlags = self->flags & intrMask;
			interrupted = J9THR_WAIT_INTERRUPTED(intrFlags);
			priorityinterrupted = J9THR_WAIT_PRI_INTERRUPTED(intrFlags);
			notified = self->flags & J9THREAD_FLAG_NOTIFIED;
			if (interrupted || priorityinterrupted || notified) {
				break;
			}
			THREAD_UNLOCK(self);
			ASSERT(0);
		J9OSCOND_WAIT_LOOP();
	}

	/* DONE WAITING AT THIS POINT */

	/* we have to remove self from the wait queue */
	if (notified) {
		queue = &monitor->blocking;
	} else {
		queue = &monitor->waiting;
	}
	threadDequeue(queue, self);

	MONITOR_UNLOCK(monitor);

	/* at this point, this thread should already be locked */

	ASSERT(notified || interrupted || priorityinterrupted || timedOut);
	/* if we were interrupted, then we'd better have been interruptible */
	ASSERT(!interrupted || (interruptible & J9THREAD_FLAG_INTERRUPTABLE));
	ASSERT(!priorityinterrupted || (interruptible & (J9THREAD_FLAG_INTERRUPTABLE | J9THREAD_FLAG_ABORTABLE)));

	self->flags &= ~(J9THREAD_FLAG_WAITING | J9THREAD_FLAG_TIMER_SET
					 | J9THREAD_FLAG_INTERRUPTABLE
					 | J9THREAD_FLAG_NOTIFIED);
	if (interruptible & J9THREAD_FLAG_INTERRUPTABLE) {
		self->flags &= ~J9THREAD_FLAG_PRIORITY_INTERRUPTED;
	}
	/*
	 * The interrupt remains pending if the thread was priority-interrupted or notified.
	 */
	if (interrupted && !(notified || priorityinterrupted)) {
		self->flags &= ~J9THREAD_FLAG_INTERRUPTED;
	}
	/*
	 * Don't clear J9THREAD_FLAG_ABORTED.
	 * Don't clear J9THREAD_FLAG_ABORTABLE. We don't want a hole here where the thread is
	 * not abortable until it enters monitor_enter_three_tier().
	 */

	/*
	 * Is there an interruptServer thread out there trying to interrupt us?
	 * Its services are no longer required.
	 */
	if (self->interrupter) {
		ASSERT(interrupted || priorityinterrupted);
		THREAD_LOCK(self->interrupter, CALLER_MONITOR_WAIT2);
		self->interrupter->flags |= J9THREAD_FLAG_CANCELED;
		THREAD_UNLOCK(self->interrupter);
		self->interrupter = NULL;
	}

	THREAD_UNLOCK(self);

	if (monitor_enter_three_tier(
			self, monitor,
			(BOOLEAN)((interruptible & J9THREAD_FLAG_ABORTABLE)? SET_ABORTABLE: DONT_SET_ABORTABLE))
		== J9THREAD_INTERRUPTED_MONITOR_ENTER
	) {
		/* we don't own the monitor */
		return J9THREAD_INTERRUPTED_MONITOR_ENTER;
	}
	monitor->count = count;

	ASSERT(monitor->owner == self);
	ASSERT(monitor->count == count);
	ASSERT(monitor->count >= 1);
	ASSERT(0 == self->monitor);
	ASSERT(!(monitor->flags & J9THREAD_FLAG_WAITING));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_TIMER_SET));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_BLOCKED));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_NOTIFIED));
	ASSERT(!(monitor->flags & J9THREAD_FLAG_INTERRUPTABLE));
	ASSERT(NULL == self->next);

	if (priorityinterrupted) {
		return J9THREAD_PRIORITY_INTERRUPTED;
	}
	if (notified) {
		return 0;
	}
	if (interrupted) {
		return J9THREAD_INTERRUPTED;
	}
	if (timedOut) {
		return J9THREAD_TIMED_OUT;
	}
	ASSERT(0);
	return 0;
}
#endif /* OMR_THR_THREE_TIER_LOCKING */

/**
 * Returns how many threads are currently waiting on a monitor.
 *
 * @note This can only be called by the owner of this monitor.
 *
 * @param[in] monitor a monitor
 * @return number of threads waiting on the monitor (>=0)
 */
uintptr_t
omrthread_monitor_num_waiting(omrthread_monitor_t monitor)
{
	uintptr_t numWaiting = 0;
	omrthread_t curr;

	ASSERT(monitor);

#ifdef OMR_THR_THREE_TIER_LOCKING
	MONITOR_LOCK(monitor, CALLER_MONITOR_NUM_WAITING);
#endif

	curr = monitor->waiting;
	while (curr != NULL) {
		numWaiting++;
		curr = curr->next;
	}
	curr = monitor->notifyAllWaiting;
	while (curr != NULL) {
		numWaiting++;
		curr = curr->next;
	}

#ifdef OMR_THR_THREE_TIER_LOCKING
	MONITOR_UNLOCK(monitor);
#endif

	return numWaiting;

}


/**
 * Notify a single thread waiting on a monitor.
 *
 * A thread is considered to be waiting on the monitor if
 * it is currently blocked while executing omrthread_monitor_wait on the monitor.
 *
 * If no threads are waiting, no action is taken.
 *
 * @param[in] monitor a monitor to be signaled
 * @return  0 once the monitor has been signaled<br>J9THREAD_ILLEGAL_MONITOR_STATE if the current thread does not own the monitor
 *
 * @see omrthread_monitor_notify_all, omrthread_monitor_enter, omrthread_monitor_wait
 */
intptr_t
omrthread_monitor_notify(omrthread_monitor_t monitor)
{
	return monitor_notify_one_or_all(monitor, NOTIFY_ONE);
}

/**
 * Notify all threads waiting on a monitor.
 *
 * A thread is considered to be waiting on the monitor if
 * it is currently blocked while executing omrthread_monitor_wait on the monitor.
 *
 * If no threads are waiting, no action is taken.
 *
 *
 * @param[in] monitor a monitor to be signaled
 * @return  0 once the monitor has been signaled<br>J9THREAD_ILLEGAL_MONITOR_STATE if the current thread does not own the monitor
 *
 * @see omrthread_monitor_notify, omrthread_monitor_enter, omrthread_monitor_wait
 */
intptr_t
omrthread_monitor_notify_all(omrthread_monitor_t monitor)
{
	return monitor_notify_one_or_all(monitor, NOTIFY_ALL);
}



/**
 * Signal one or all threads waiting on the monitor.
 *
 * If no threads are waiting, this does nothing.
 *
 * @param[in] monitor monitor to be notified on
 * @param[in] notifyall 0 to notify one, non-zero to notify all
 * @return 0 once the monitor has been signalled<br>
 * J9THREAD_ILLEGAL_MONITOR_STATE if the current thread does not
 * own the monitor
 *
 */
static intptr_t
monitor_notify_one_or_all(omrthread_monitor_t monitor, int notifyall)
{
	intptr_t rc = J9THREAD_ILLEGAL_MONITOR_STATE;
	omrthread_t self = MACRO_SELF();

	Trc_THR_ThreadMonitorNotifyEnter(self, monitor, notifyall);

#if defined(OMR_THR_THREE_TIER_LOCKING)
	if (self->library->flags & J9THREAD_LIB_FLAG_FAST_NOTIFY) {
		rc = monitor_notify_three_tier(self, monitor, notifyall);
	} else {
		rc = monitor_notify_original(self, monitor, notifyall);
	}
#else
	rc = monitor_notify_original(self, monitor, notifyall);
#endif

	Trc_THR_ThreadMonitorNotifyExit(self, monitor, rc);
	return rc;
}

/*
 * VMDESIGN WIP 1320
 * This is the original implementation of monitor_notify_one_or_all().
 * It handles both 3-tier and non-3-tier.
 * TODO: Split the 3-tier and non-3-tier implementations.
 */
static intptr_t
monitor_notify_original(omrthread_t self, omrthread_monitor_t monitor, int notifyall)
{
	omrthread_t queue, next;
	int someoneNotified = 0;

	ASSERT(self);
	ASSERT(monitor);

	if (monitor->owner != self) {
		ASSERT_DEBUG(0);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

#ifdef OMR_THR_THREE_TIER_LOCKING
	MONITOR_LOCK(monitor, CALLER_NOTIFY_ONE_OR_ALL);
#endif

	next = monitor->waiting;
	if (next) {
		if (notifyall) {
			monitor_notify_all_migration(monitor);
		}
	}

	while (next) {
		queue = next;
		next = queue->next;
		THREAD_LOCK(queue, CALLER_NOTIFY_ONE_OR_ALL);
		if (queue->flags & J9THREAD_FLAG_WAITING) {
			threadNotify(queue);
			Trc_THR_ThreadMonitorNotifyThreadNotified(self, queue, monitor);
			someoneNotified = 1;
		}
		THREAD_UNLOCK(queue);

		if ((someoneNotified) && (!notifyall)) {
			break;
		}
	}

#ifdef OMR_THR_THREE_TIER_LOCKING
	MONITOR_UNLOCK(monitor);
#endif

	return 0;
}

#if defined(OMR_THR_THREE_TIER_LOCKING)
static intptr_t
monitor_notify_three_tier(omrthread_t self, omrthread_monitor_t monitor, int notifyall)
{
	omrthread_t queue;

	ASSERT(self);
	ASSERT(monitor);

	if (monitor->owner != self) {
		ASSERT_DEBUG(0);
		return J9THREAD_ILLEGAL_MONITOR_STATE;
	}

	MONITOR_LOCK(monitor, CALLER_NOTIFY_ONE_OR_ALL);
	queue = monitor->waiting;
	if (queue) {
		intptr_t state;

		state = omrthread_spinlock_swapState(monitor, J9THREAD_MONITOR_SPINLOCK_EXCEEDED);
		ASSERT((state == J9THREAD_MONITOR_SPINLOCK_OWNED)
			|| (state == J9THREAD_MONITOR_SPINLOCK_EXCEEDED));

		if (notifyall) {
			/* set all the thread flags */
			do {
				THREAD_LOCK(queue, 0);
				queue->flags &= ~J9THREAD_FLAG_WAITING;
				queue->flags |= J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED;
				Trc_THR_ThreadMonitorNotifyThreadNotified(self, queue, monitor);
				THREAD_UNLOCK(queue);

				queue = queue->next;
			} while (queue);

			/* append waiting queue to the blocking queue */
			if (monitor->blocking) {
				omrthread_t tail;

				tail = monitor->blocking->prev;
				ASSERT(tail->next == NULL);
				tail->next = monitor->waiting;

				monitor->blocking->prev = monitor->waiting->prev;
				monitor->waiting->prev = tail;
			} else {
				monitor->blocking = monitor->waiting;
			}
			monitor->waiting = NULL;

		} else {
			THREAD_LOCK(queue, 0);
			queue->flags &= ~J9THREAD_FLAG_WAITING;
			queue->flags |= J9THREAD_FLAG_BLOCKED | J9THREAD_FLAG_NOTIFIED;
			Trc_THR_ThreadMonitorNotifyThreadNotified(self, queue, monitor);
			THREAD_UNLOCK(queue);

			threadDequeue(&monitor->waiting, queue);
			threadEnqueue(&monitor->blocking, queue);
		}
	}

	MONITOR_UNLOCK(monitor);
	return 0;
}
#endif

/**
 * Acquire the threading library's global lock.
 *
 * @param[in] self omrthread_t for the current thread
 * @param[in] monitor must be NULL
 * @return none
 *
 * @deprecated This has been replaced by omrthread_lib_lock.
 * @see omrthread_lib_lock, omrthread_lib_unlock
 */
void
omrthread_monitor_lock(omrthread_t self, omrthread_monitor_t monitor)
{
	ASSERT(self);

	if (monitor == NULL) {
		GLOBAL_LOCK(self, CALLER_GLOBAL_LOCK);
	} else {
		ASSERT(0);
	}
}



/**
 * Release the threading library's global lock.
 *
 * @param[in] self omrthread_t for the current thread
 * @param[in] monitor
 * @return none
 *
 * @deprecated This has been replaced by omrthread_lib_unlock.
 * @see omrthread_lib_lock, omrthread_lib_unlock
 */
void
omrthread_monitor_unlock(omrthread_t self, omrthread_monitor_t monitor)
{
	ASSERT(self);

	if (monitor == NULL) {
		GLOBAL_UNLOCK(self);
	} else {
		ASSERT(0);
	}
}


/**
 * Create and initialize a pool of monitors.
 *
 * @return pointer to a new monitor pool on success, NULL on failure
 *
 */
static omrthread_monitor_pool_t
allocate_monitor_pool(omrthread_library_t lib)
{
	int i;
	omrthread_monitor_t entry;
	omrthread_monitor_pool_t pool = (omrthread_monitor_pool_t)omrthread_allocate_memory(lib, sizeof(J9ThreadMonitorPool), OMRMEM_CATEGORY_THREADS);
	if (pool == NULL) {
		return NULL;
	}
	memset(pool, 0, sizeof(J9ThreadMonitorPool));

	pool->next_free = entry = &pool->entries[0];
	for (i = 0; i < MONITOR_POOL_SIZE - 1; i++, entry++) {
		entry->count = FREE_TAG;
		entry->owner = (omrthread_t)(entry + 1);
		/* entry->waiting = entry->blocked = NULL; */ /* (unnecessary) */
		entry->flags = J9THREAD_MONITOR_MUTEX_UNINITIALIZED;
	}
	/* initialize the last monitor */
	entry->count = FREE_TAG;
	entry->flags = J9THREAD_MONITOR_MUTEX_UNINITIALIZED;

	return pool;
}

/**
 * Free the J9 threading library's monitor pool.
 *
 * This requires destroying each and every one of the
 * monitors in the pool.
 *
 * @return none
 */
static void
free_monitor_pools(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	omrthread_monitor_pool_t pool = lib->monitor_pool;

	ASSERT(lib);
	ASSERT(pool);

	while (pool) {
		int i;
		omrthread_monitor_pool_t next = pool->next;
		omrthread_monitor_t entry = &pool->entries[0];
		for (i = 0; i < MONITOR_POOL_SIZE - 1; i++, entry++) {
			if (entry->flags != J9THREAD_MONITOR_MUTEX_UNINITIALIZED) {
				J9OSMUTEX_DESTROY(entry->mutex);
			}
		}
		omrthread_free_memory(lib, pool);
		pool = next;
	}

	lib->monitor_pool = 0;
}


#if (defined(OMR_THR_TRACING))
/**
 * Dump information about a monitor to stderr
 *
 * @param[in] monitor monitor to be dumped (non-NULL)
 * @return none
 *
 * @see omrthread_monitor_dump_all
 */
void
omrthread_monitor_dump_trace(omrthread_monitor_t monitor)
{
	fprintf(stderr,
			"<thr_mon: %s enter=%u hold=%u exit=%u hldtm=%I64d blktm=%I64d>\n",
			monitor->tracing.monitor_name
			? monitor->tracing.monitor_name
			: ((monitor->flags & J9THREAD_MONITOR_OBJECT) ? "(object)" : "(null)"),
			monitor->tracing.enter_count,
			monitor->tracing.hold_count,
			monitor->tracing.exit_count,
			monitor->tracing.holdtime_sum,
			0);
	fflush(stderr);
}

#endif /* OMR_THR_TRACING */


#if (defined(OMR_THR_TRACING))
/**
 * Dump information about all monitors currently in use.
 *
 * @return none
 * @see omrthread_monitor_dump_trace
 */
void
omrthread_monitor_dump_all(void)
{
	omrthread_monitor_t monitor = NULL;
	omrthread_monitor_walk_state_t walkState;
	omrthread_monitor_init_walk(&walkState);

	while (NULL != (monitor = omrthread_monitor_walk(&walkState))) {
		omrthread_monitor_dump_trace(monitor);
	}

}

#endif /* OMR_THR_TRACING */


#if (defined(OMR_THR_TRACING))
/**
 * @internal
 *
 *  Dump a thread's tracing values to stderr.
 *
 *  @param[in] thread thread with tracing values to dump
 *  @return none
 */
void
omrthread_dump_trace(omrthread_t thread)
{
	int i;
	fprintf(stderr,
			"<thr_thr: id=%p createtime=%I64d now=%I64d holdtime=%I64d>\n",
			thread,
			thread->tracing.createtime,
			GetCycles(),
			thread->tracing.holdtime);
	for (i = 0; i < sizeof(thread->tracing.count) / sizeof(thread->tracing.count[0]); i++) {
		fprintf(stderr,
				"<thr_lock: id=%p location=%2d count=%u time=%I64d>\n",
				thread,
				i + 1,
				thread->tracing.count[i],
				thread->tracing.gettime[i]);
	}
	fflush(stderr);
}

#endif /* OMR_THR_TRACING */


/**
 * Check if the current awakened thread was a target for a notify request.
 *
 * This routine must be called with MONITOR_LOCK() held.
 *
 * @param[in] self   the current thread with THREAD_LOCK() held on it
 *
 * @return  non-0 if thread is target of notify, otherwise 0
 *
 */
static intptr_t
check_notified(omrthread_t self, omrthread_monitor_t monitor)
{
	intptr_t rc = 0;
	rc = self->flags & J9THREAD_FLAG_NOTIFIED;
	if (rc) {
		return rc;
	}

	/* was the thread a target of a notifyAll operation ? */
	rc = monitor_on_notify_all_wait_list(self, monitor);

	return rc;
}

/**
 * Determine the maximum waitNumber of threads waiting on a monitor.
 * This routine must be called with MONITOR_LOCK() held.
 *
 * @param[monitor] the given monitor
 *
 * @return  The maximum wait number or 0 if no waiters
 *
 */
static uintptr_t
monitor_maximum_wait_number(omrthread_monitor_t monitor)
{
	uintptr_t hwn;

	if (monitor->waiting) {
		hwn = monitor->waiting->prev->waitNumber;
	} else if (monitor->notifyAllWaiting) {
		hwn = monitor->notifyAllWaiting->prev->waitNumber;
	} else {
		hwn = 0;  /* no waiters */
	}
	return hwn;
}

/**
 * Determine if a given thread is on the notifyAll wait list.
 * This is done by looking at the wait number in the thread and
 * the highest wait number of a thread on the wait list.
 *
 * This routine must be called with MONITOR_LOCK() held.
 *
 * @param[in] monitor the given monitor
 *
 * @return  TRUE - the monitor is on the notifyAll wait list
 *          FALSE - otherwise
 *
 */
static uintptr_t
monitor_on_notify_all_wait_list(omrthread_t self, omrthread_monitor_t monitor)
{
	uintptr_t hwn;  /* highest wait number of a thread on notifyAll wait list */
	uintptr_t rc = FALSE;

	if (monitor->notifyAllWaiting) {
		hwn = monitor->notifyAllWaiting->prev->waitNumber;
		rc = self->waitNumber <= hwn;
	}
	return rc;
}

/**
 * Move the notify wait list to the tail of the notifyAll wait list
 * This method must be called with MONITOR_LOCK() held.
 *
 * @param[monitor] the given monitor
 *
 * @return  none
 *
 */
static void
monitor_notify_all_migration(omrthread_monitor_t monitor)
{
	omrthread_t waiting, wtail;
	omrthread_t notifyAllWaiting, nawtail;

	notifyAllWaiting = monitor->notifyAllWaiting;
	waiting = monitor->waiting;
	if (notifyAllWaiting) {
		/* Append the waiters to those currently notifyAll'ed */
		nawtail = notifyAllWaiting->prev;
		wtail = waiting->prev;

		nawtail->next = waiting;
		waiting->prev = nawtail;
		notifyAllWaiting->prev = wtail;

	} else {
		/* The wait list becomes the notifyAlled wait list */
		monitor->notifyAllWaiting = waiting;
	}
	monitor->waiting = NULL;
}

/**
 * Test to see whether the current thread is the owner of a monitor
 *
 * @param[in] monitor a monitor to be tested
 * @return 1 if the current thread owns the monitor, 0 otherwise
 */
uintptr_t
omrthread_monitor_owned_by_self(omrthread_monitor_t monitor)
{
	omrthread_t self = MACRO_SELF();

	ASSERT(self);
	ASSERT(monitor);

	if (monitor->owner == self) {
		return 1;
	}
	return 0;
}


#if defined(OMR_PORT_NUMA_SUPPORT)
#define NODE_TO_INDEX(node) (node / 8)
#define NODE_TO_MASK(node) (((uintptr_t)1) << (node % 8))
/**
 * Adds the given nodeNumber to the given thread's numaAffinity cache.
 * @param thread[in] The thread to change
 * @param nodeNumber[in] The node number to add (expressed as a 1-indexed J9-level NUMA node number)
 */
void
omrthread_add_node_number_to_affinity_cache(omrthread_t thread, uintptr_t nodeNumber)
{
	/* the cache stores 1024 nodes as 0-indexed values */
	if (nodeNumber > 0) {
		uintptr_t node = nodeNumber - 1;
		uintptr_t index = NODE_TO_INDEX(node);
		uintptr_t mask = NODE_TO_MASK(node);
		thread->numaAffinity[index] |= mask;
		Trc_THR_ThreadNUMAAffinitySet(thread, nodeNumber);
	}
}

BOOLEAN
omrthread_does_affinity_cache_contain_node(omrthread_t thread, uintptr_t nodeNumber)
{
	BOOLEAN result = FALSE;
	/* the cache stores 1024 nodes as 0-indexed values */
	if (nodeNumber > 0) {
		uintptr_t node = nodeNumber - 1;
		uintptr_t index = NODE_TO_INDEX(node);
		uintptr_t mask = NODE_TO_MASK(node);
		result = (0 != (thread->numaAffinity[index] & mask));
	}
	return result;
}
#endif /* defined(OMR_PORT_NUMA_SUPPORT) */

/**
 * Calculate the unaccounted CPU usage quantum and charge it to the appropriate category.
 *
 * @param thread[in] The thread whose unaccounted usage quantum needs to charged.
 * @param type[in] The type of accounting.
 *
 * @return -1 on error and 0 on success.
 */
static intptr_t
fixupThreadAccounting(omrthread_t thread, uintptr_t type)
{
	omrthread_t self = MACRO_SELF();
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	J9ThreadsCpuUsage *cumulativeUsage = &lib->cumulativeThreadsInfo;
	int64_t cpuQuantum = 0;
	int64_t currCpuTime = 0;
	intptr_t result = 0;

	if (J9THREAD_TYPE_SET_CREATE == type) {
		/* During thread creation, account for all time from the start into the right category */
		/* This will happen automatically on either a query or a category change */
		thread->lastCategorySwitchTime = 0;
		return 0;
	}

	result = omrthread_get_cpu_time_ex(thread, &currCpuTime);
	if (J9THREAD_SUCCESS != result) {
		result &= ~J9THREAD_ERR_OS_ERRNO_SET;
		/* If no OS thread attached as yet, nothing to be done */
		if (J9THREAD_ERR_NO_SUCH_THREAD == result) {
			return 0;
		} else {
			Trc_THR_fixupThreadAccounting_omrthread_get_cpu_time_ex_error(result, thread);
			return -1;
		}
	}
	currCpuTime /= 1000;
	cpuQuantum = currCpuTime - thread->lastCategorySwitchTime;

	/* For both a ATTACH and a MODIFY, capture the current used cpu */
	thread->lastCategorySwitchTime = currCpuTime;

	/* If it is a ATTACH, we can ignore any previous usage */
	if (J9THREAD_TYPE_SET_ATTACH == type) {
		return 0;
	}

	/* Capture the current cpu usage of the thread since the last event (such as attach, category switch */
	if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD)) {
		cumulativeUsage->resourceMonitorCpuTime += cpuQuantum;
	} else if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_SYSTEM_THREAD)) {
		cumulativeUsage->systemJvmCpuTime += cpuQuantum;
		if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_SYSTEM_GC_THREAD)) {
			cumulativeUsage->gcCpuTime += cpuQuantum;
		} else if (J9_ARE_ALL_BITS_SET(thread->effective_category, J9THREAD_CATEGORY_SYSTEM_JIT_THREAD)) {
			cumulativeUsage->jitCpuTime += cpuQuantum;
		}
	} else {
		cumulativeUsage->applicationCpuTime += cpuQuantum;
		if ((thread->effective_category & J9THREAD_USER_DEFINED_THREAD_CATEGORY_MASK) > 0) {
			int userCat = (thread->effective_category - J9THREAD_USER_DEFINED_THREAD_CATEGORY_1) & J9THREAD_USER_DEFINED_THREAD_CATEGORY_MASK;
			userCat >>= J9THREAD_USER_DEFINED_THREAD_CATEGORY_BIT_SHIFT;
			ASSERT(userCat >= J9THREAD_MAX_USER_DEFINED_THREAD_CATEGORIES);
			cumulativeUsage->applicationUserCpuTime[userCat] += cpuQuantum;
		}
	}

	return 0;
}

/**
 * Set the thread category for the given thread.
 * Most of the error and validity checking for application threads will happen at the JNI layer.
 * @param thread[in] The thread whose category is to be set.
 * @param category[in] The category to be set.
 * @param type[in] The type of accounting.
 *
 * @return -1 on error and 0 on success
 */
intptr_t
omrthread_set_category(omrthread_t thread, uintptr_t category, uintptr_t type)
{
	intptr_t rc = 0;
	omrthread_library_t lib = GLOBAL_DATA(default_library);

	ASSERT(thread);

	J9OSMUTEX_ENTER(lib->resourceUsageMutex);

	/* If the category to be set is the same as the current category, nothing to be done */
	if (category == thread->category && category == thread->effective_category) {
		goto unlock_exit;
	}

	/* If -XX:-EnableCPUMonitor has been set, no cpu usage accounting will be done.
	 * Check if we are in the middle of a GC cycle, if so, no accounting updates will be done */
	if (J9_ARE_ALL_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_ENABLE_CPU_MONITOR)
	 && ((J9THREAD_TYPE_SET_GC == type)
	 || ((J9THREAD_TYPE_SET_GC != type) && (thread->category == thread->effective_category)))
	) {
		rc = fixupThreadAccounting(thread, type);
		/* Change the category of the thread regardless of fixupThreadAccounting returning an error */
	}

	/* If an application thread is doing GC work, only change effective_category to reflect that */
	if (J9THREAD_TYPE_SET_GC == type) {
		if (J9THREAD_CATEGORY_SYSTEM_GC_THREAD == category) {
			thread->effective_category = (uint32_t)category;
		} else {
			thread->effective_category = thread->category;
		}
	} else {
		switch (category) {
		case J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD:
		case J9THREAD_CATEGORY_SYSTEM_THREAD:
		case J9THREAD_CATEGORY_SYSTEM_GC_THREAD:
		case J9THREAD_CATEGORY_SYSTEM_JIT_THREAD:
		case J9THREAD_CATEGORY_APPLICATION_THREAD:
		case J9THREAD_USER_DEFINED_THREAD_CATEGORY_1:
		case J9THREAD_USER_DEFINED_THREAD_CATEGORY_2:
		case J9THREAD_USER_DEFINED_THREAD_CATEGORY_3:
		case J9THREAD_USER_DEFINED_THREAD_CATEGORY_4:
		case J9THREAD_USER_DEFINED_THREAD_CATEGORY_5:
			/* If in a GC cycle, do not update effective category */
			if (thread->effective_category == thread->category) {
				thread->effective_category = thread->category = (uint32_t)category;
			} else {
				thread->category = (uint32_t)category;
			}
			break;

		default:
			rc = -1;
			Trc_THR_omrthread_set_category_invalid_category(category, thread);
		}
	}

unlock_exit:
	J9OSMUTEX_EXIT(lib->resourceUsageMutex);
	return rc;
}

/**
 * Get the thread category for the given thread
 * @param thread[in] The thread whose category is to be returned
 *
 * @return category
 */
uintptr_t
omrthread_get_category(omrthread_t thread)
{
	ASSERT(thread);
	/* If we ensure all read writes are completed at this time, we dont need to take the GLOBAL_LOCK_SIMPLE */
	issueReadWriteBarrier();
	return thread->category;
}

/* Return 0 on success, non-zero on failure */
intptr_t
omrthread_init_library(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	if (0 == lib->initStatus) {
		init_thread_library();
	}
	return 1 != lib->initStatus;
}

void
omrthread_shutdown_library(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	if (1 == lib->initStatus) {
		omrthread_shutdown();
		lib->initStatus = 0;
	}
}

/**
 * @brief Set the J9THREAD_FLAG_CPU_SAMPLING_ENABLED flag for the given thread
 *        if CPU Monitoring has been turned on.
 * @param thread
 * @return void
 */
static void
threadEnableCpuMonitor(omrthread_t thread)
{
	omrthread_library_t lib = thread->library;

	if (J9_ARE_ALL_BITS_SET(lib->flags, J9THREAD_LIB_FLAG_ENABLE_CPU_MONITOR)) {
		THREAD_LOCK(thread, CALLER_SET_FLAG_ENABLE_CPU_MONITOR);
		/* Should be safe now to query cpu usage for the freshly attached thread */
		thread->flags |= J9THREAD_FLAG_CPU_SAMPLING_ENABLED;
		THREAD_UNLOCK(thread);
	}
}

/*
 * @see thread_api.h for description
 */
void
omrthread_lib_enable_cpu_monitor(omrthread_t thread)
{
	omrthread_lib_set_flags(J9THREAD_LIB_FLAG_ENABLE_CPU_MONITOR);

	threadEnableCpuMonitor(thread);
}

omrthread_t
j9thread_self(void)
{
	return omrthread_self();
}
void *
j9thread_tls_get(omrthread_t thread, omrthread_tls_key_t key)
{
	return omrthread_tls_get(thread, key);
}

