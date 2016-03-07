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

#ifndef threaddef_h
#define threaddef_h

#include <string.h>

#include "omrcfg.h"

#include "thrdsup.h"

#include "omrthread.h"
#include "thread_internal.h"
#undef omrthread_monitor_init
#undef omrthread_monitor_init_with_name
#include "thrtypes.h"
#include "pool_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Define this to force a thread to be spawned when
 * interrupting a waiting thread (it's a debug thing)
 */
#undef ALWAYS_SPAWN_THREAD_TO_INTERRUPT

/* You got to know what time it is */
typedef uint64_t omrtime_t;
typedef int64_t omrtime_delta_t;

/* ASSERT and Debug */
#ifndef STATIC_ASSERT
#define STATIC_ASSERT(x) \
	do{ \
		typedef int failed_assert[(x) ? 1 : -1]; \
	} while(0)
#endif

#if defined (THREAD_ASSERTS)

#define UNOWNED ((omrthread_t)-1)
extern omrthread_t global_lock_owner;

#undef NDEBUG
#include <assert.h>

#ifndef ASSERT
#define ASSERT(x) assert(x)
#endif /* ASSERT */

#ifndef ASSERT_DEBUG
#define ASSERT_DEBUG(x) assert(x)
#endif /* ASSERT_DEBUG */

#define DEBUG_SYSCALL(x) omrthread_debug_syscall(#x, (x))
extern intptr_t omrthread_debug_syscall(const char *func, intptr_t retval);

#else /* THREAD_ASSERTS */

#ifndef ASSERT
#define ASSERT(ignore) ((void)0)
#endif /* ASSERT */

#ifndef ASSERT_DEBUG
#define ASSERT_DEBUG(ignore) ((void)0)
#endif /* ASSERT_DEBUG */

#define DEBUG_SYSCALL(x) (x)

#endif /* THREAD_ASSERTS */

#undef  DEBUG
#define DEBUG (0)

intptr_t omrthread_spinlock_acquire(omrthread_t self, omrthread_monitor_t monitor);
intptr_t omrthread_spinlock_acquire_no_spin(omrthread_t self, omrthread_monitor_t monitor);
uintptr_t omrthread_spinlock_swapState(omrthread_monitor_t monitor, uintptr_t newState);

/*
 * constants for profiling
 */
enum {
	CALLER_ATTACH = 0,
	CALLER_DESTROY,
	CALLER_SUSPEND,
	CALLER_RESUME,
	CALLER_CLEAR_INTERRUPTED,
	CALLER_THREAD_WRAPPER,
	CALLER_NOTIFY_ONE_OR_ALL,
	CALLER_SLEEP_INTERRUPTABLE,
	CALLER_TRY_ENTER_USING,
	CALLER_SLEEP,
	CALLER_EXIT_MONITOR,
	CALLER_DETACH,
	CALLER_CLEAR_PRIORITY_INTERRUPTED,
	CALLER_INTERNAL_EXIT1,
	CALLER_INTERNAL_EXIT2,
	CALLER_MONITOR_ENTER1,
	CALLER_MONITOR_ENTER2,
	CALLER_MONITOR_ENTER_THREE_TIER1,
	CALLER_MONITOR_ENTER_THREE_TIER2,
	CALLER_MONITOR_ENTER_THREE_TIER3,
	CALLER_MONITOR_EXIT1,
	CALLER_MONITOR_WAIT1,
	CALLER_MONITOR_WAIT2,
	CALLER_INTERRUPT_THREAD,
	CALLER_MONITOR_NUM_WAITING,
	CALLER_MONITOR_DESTROY,
	CALLER_GLOBAL_LOCK,
	CALLER_MONITOR_ACQUIRE,
	CALLER_INTERRUPT_SERVER,
	CALLER_RESET_TRACING,
	CALLER_LIB_SET_FLAGS,
	CALLER_LIB_CLEAR_FLAGS,
	CALLER_PARK,
	CALLER_UNPARK,
	CALLER_JLM_INIT,
	CALLER_CHECK_NOTIFIED,
	CALLER_MARK_INTERNAL_WAKEUP,
	CALLER_INTERRUPT_WAITING,
	CALLER_INTERRUPT_BLOCKED,
	CALLER_MONITOR_ENTER_THREE_TIER4,
	CALLER_STORE_EXIT_CPU_USAGE,
	CALLER_GET_JVM_CPU_USAGE_INFO,
	CALLER_SET_FLAG_ENABLE_CPU_MONITOR,
	CALLER_LAST_INDEX
};
#define MAX_CALLER_INDEX CALLER_LAST_INDEX

/*
 * helper defines for local functions
 */
#define NOTIFY_ONE (0)
#define NOTIFY_ALL (1)
#define GLOBAL_NOT_LOCKED (0)
#define GLOBAL_IS_LOCKED  (1)

/*
 * Boolean flags for monitor_enter_three_tier()
 */
#define SET_ABORTABLE		(1)
#define DONT_SET_ABORTABLE	(0)

/*
 * Boolean flag for custom adaptive spin
 */
#define CUSTOM_ADAPTIVE_SPIN_TRUE  (1)

#define MACRO_SELF() ((omrthread_t)TLS_GET(((omrthread_library_t)GLOBAL_DATA(default_library))->self_ptr))

#if defined(THREAD_ASSERTS)
#define GLOBAL_LOCK(self, caller) \
	do { \
		ASSERT(global_lock_owner != (self)); \
		J9OSMUTEX_ENTER((self)->library->monitor_mutex); \
		ASSERT(UNOWNED == global_lock_owner); \
		global_lock_owner = (self); \
	} while(0)
#else
#define GLOBAL_LOCK(self, caller) J9OSMUTEX_ENTER((self)->library->monitor_mutex)
#endif

#define GLOBAL_TRY_LOCK(self, caller) J9OSMUTEX_TRY_ENTER((self)->library->monitor_mutex)

#ifdef THREAD_ASSERTS
#define GLOBAL_UNLOCK(self) \
	do { \
		ASSERT((self) == global_lock_owner); \
		global_lock_owner = UNOWNED; \
		J9OSMUTEX_EXIT((self)->library->monitor_mutex); \
	} while(0)
#else
#define GLOBAL_UNLOCK(self) J9OSMUTEX_EXIT((self)->library->monitor_mutex)
#endif

/*
 * GLOBAL_LOCK_SIMPLE
 * locking when you don't have a thread, just a lib
 */
#ifdef THREAD_ASSERTS
#define GLOBAL_LOCK_SIMPLE(lib) \
	do { \
		omrthread_t self = MACRO_SELF(); \
		ASSERT(self != global_lock_owner); \
		J9OSMUTEX_ENTER((lib)->monitor_mutex); \
		ASSERT(UNOWNED == global_lock_owner); \
		global_lock_owner = self; \
	} while(0)
#else
#define GLOBAL_LOCK_SIMPLE(lib) J9OSMUTEX_ENTER((lib)->monitor_mutex)
#endif

/*
 * GLOBAL_UNLOCK_SIMPLE
 * unlocking when you don't have a thread, just a lib
 */
#ifdef THREAD_ASSERTS
#define GLOBAL_UNLOCK_SIMPLE(lib) \
	do { \
		ASSERT(MACRO_SELF() == global_lock_owner); \
		global_lock_owner = UNOWNED; \
		J9OSMUTEX_EXIT((lib)->monitor_mutex); \
	} while(0)
#else /* THREAD_ASSERTS */
#define GLOBAL_UNLOCK_SIMPLE(lib) J9OSMUTEX_EXIT((lib)->monitor_mutex)
#endif /* THREAD_ASSERTS */

#define THREAD_LOCK(thread, caller) J9OSMUTEX_ENTER((thread)->mutex)

#define THREAD_UNLOCK(thread) J9OSMUTEX_EXIT((thread)->mutex)

#define MONITOR_LOCK(monitor, caller) J9OSMUTEX_ENTER((monitor)->mutex)

#ifdef FORCE_TO_USE_IS_THREAD
/*
 * Force the use of the interruptServer (IS) thread by always failing
 * when trying to enter a monitor without blocking
 */
#define MONITOR_TRY_LOCK(monitor)  (-1)
#else /* FORCE_TO_USE_IS_THREAD */
#define MONITOR_TRY_LOCK(monitor) J9OSMUTEX_TRY_ENTER((monitor)->mutex)
#endif /* FORCE_TO_USE_IS_THREAD */

#define MONITOR_UNLOCK(monitor) J9OSMUTEX_EXIT((monitor)->mutex)

#define IS_OBJECT_MONITOR(monitor) (J9THREAD_MONITOR_OBJECT == ((monitor)->flags & J9THREAD_MONITOR_OBJECT))

#define IS_JLM_ENABLED(thread) ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_INIT_DATA_STRUCTURES)

#if defined(OMR_THR_ADAPTIVE_SPIN)
#define IS_JLM_TIME_STAMPS_ENABLED(thread, monitor) \
	(((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_TIME_STAMPS_ENABLED) || IS_JLM_HOLDTIME_SAMPLING_ENABLED((thread), (monitor)))
#else /* IS_JLM_TIME_STAMPS_ENABLED */
#define IS_JLM_TIME_STAMPS_ENABLED(thread, monitor) ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_TIME_STAMPS_ENABLED)
#endif /* IS_JLM_TIME_STAMPS_ENABLED */

#define IS_JLM_HST_ENABLED(thread) ((thread)->library->flags & J9THREAD_LIB_FLAG_JLMHST_ENABLED)

/* MACROS FOR ADAPTIVE SPINNING */
#if defined(OMR_THR_ADAPTIVE_SPIN)
#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
#define IS_JLM_SAMPLE_ENABLED(thread, monitor) ((NULL == (monitor)->customSpinOptions) ? \
											   ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_INFO_SAMPLING_ENABLED) : \
											   (CUSTOM_ADAPTIVE_SPIN_TRUE == (monitor)->customSpinOptions->customAdaptSpin))
#define IS_JLM_HOLDTIME_SAMPLING_ENABLED(thread, monitor) ((NULL == (monitor)->customSpinOptions) ? \
														  ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_HOLDTIME_SAMPLING_ENABLED) : \
														  (CUSTOM_ADAPTIVE_SPIN_TRUE == (monitor)->customSpinOptions->customAdaptSpin))
#define IS_ADAPT_HOLDTIME_ENABLED(thread, monitor) ((NULL == (monitor)->customSpinOptions) ? \
												   ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_HOLDTIME_SAMPLING_ENABLED) : \
												   (CUSTOM_ADAPTIVE_SPIN_TRUE == (monitor)->customSpinOptions->customAdaptSpin))
#define IS_ADAPT_SLOW_ENABLED(thread, monitor) ((NULL == (monitor)->customSpinOptions) ? \
											   ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_SLOW_SAMPLING_ENABLED) : \
											   (CUSTOM_ADAPTIVE_SPIN_TRUE == (monitor)->customSpinOptions->customAdaptSpin))
#define IS_ADAPTIVE_SPIN_REQUIRED(monitor) ((NULL == (monitor)->customSpinOptions) || (CUSTOM_ADAPTIVE_SPIN_TRUE == (monitor)->customSpinOptions->customAdaptSpin))
#else /* OMR_THR_CUSTOM_SPIN_OPTIONS */
#define IS_JLM_SAMPLE_ENABLED(thread, monitor) ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_INFO_SAMPLING_ENABLED)
#define IS_JLM_HOLDTIME_SAMPLING_ENABLED(thread, monitor) ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_HOLDTIME_SAMPLING_ENABLED)
#define IS_ADAPT_HOLDTIME_ENABLED(thread, monitor) ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_HOLDTIME_SAMPLING_ENABLED)
#define IS_ADAPT_SLOW_ENABLED(thread, monitor) ((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_SLOW_SAMPLING_ENABLED)
#define IS_ADAPTIVE_SPIN_REQUIRED(monitor)  (TRUE)
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */

#define IS_ADAPT_SLOW_PERCENT_ENABLED(thread, monitor) (IS_ADAPT_SLOW_ENABLED((thread), (monitor)) && (0 != (thread)->library->adaptSpinSlowPercent))

#define JLM_NON_RECURSIVE_ENTER_COUNT(monitor) ((monitor)->tracing->enter_count - (monitor)->tracing->recursive_count)
#define JLM_AVERAGE_HOLDTIME(monitor) ((monitor)->tracing->holdtime_avg)
#define JLM_SLOW_PERCENT(monitor) (((monitor)->tracing->slow_count*100)/JLM_NON_RECURSIVE_ENTER_COUNT(monitor))

#define ADAPT_MONITOR_TRACE(thread, monitor, adaptTracepoint) \
	adaptTracepoint( \
		(IS_OBJECT_MONITOR(monitor) ? "object" : "system"), (monitor), \
		(uint64_t)(monitor)->tracing->holdtime_sum, (monitor)->tracing->holdtime_count, \
		(uint64_t)((monitor)->tracing->holdtime_count > 0 ? JLM_AVERAGE_HOLDTIME(monitor) : 0), \
		(monitor)->tracing->slow_count, JLM_NON_RECURSIVE_ENTER_COUNT(monitor), \
		((monitor)->tracing->enter_count > 0 ? JLM_SLOW_PERCENT(monitor) : 0))

#ifdef OMR_THR_THREE_TIER_LOCKING
#define DISABLE_RAW_MONITOR_SPIN(thread, monitor) \
	do { \
		(monitor)->spinCount1 = 1; \
		(monitor)->spinCount2 = 1; \
		(monitor)->spinCount3 = 1; \
		ADAPT_MONITOR_TRACE((thread), (monitor), Trc_THR_Adapt_DisableSpinning); \
	} while (0)

#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
#define ENABLE_RAW_MONITOR_SPIN(thread, monitor) \
	do {\
		if (NULL == (monitor)->customSpinOptions) { \
			(monitor)->spinCount1 = (thread)->library->defaultMonitorSpinCount1; \
			(monitor)->spinCount2 = (thread)->library->defaultMonitorSpinCount2; \
			(monitor)->spinCount3 = (thread)->library->defaultMonitorSpinCount3; \
		} else { \
			(monitor)->spinCount1 = (monitor)->customSpinOptions->customThreeTierSpinCount1; \
			(monitor)->spinCount2 = (monitor)->customSpinOptions->customThreeTierSpinCount2; \
			(monitor)->spinCount3 = (monitor)->customSpinOptions->customThreeTierSpinCount3; \
			Trc_THR_EnableRawMonitorSpin_CustomSpinOption((monitor)->name, \
														  (monitor), \
														  (monitor)->spinCount1, \
														  (monitor)->spinCount2, \
														  (monitor)->spinCount3, \
														  (monitor)->customSpinOptions->customAdaptSpin); \
		} \
		ADAPT_MONITOR_TRACE((thread), (monitor), Trc_THR_Adapt_EnableSpinning); \
	} while (0)
#else /* OMR_THR_CUSTOM_SPIN_OPTIONS */
#define ENABLE_RAW_MONITOR_SPIN(thread, monitor) \
	do {\
		(monitor)->spinCount1 = (thread)->library->defaultMonitorSpinCount1; \
		(monitor)->spinCount2 = (thread)->library->defaultMonitorSpinCount2; \
		(monitor)->spinCount3 = (thread)->library->defaultMonitorSpinCount3; \
		ADAPT_MONITOR_TRACE((thread), (monitor), Trc_THR_Adapt_EnableSpinning); \
	} while (0)
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */
#else /* OMR_THR_THREE_TIER_LOCKING */
#define	DISABLE_RAW_MONITOR_SPIN(thread, monitor)
#define	ENABLE_RAW_MONITOR_SPIN(thread, monitor)
#endif /* OMR_THR_THREE_TIER_LOCKING */

#define ADAPT_DISABLE_SAMPLING(thread, monitor) \
	do { \
		(monitor)->flags |= J9THREAD_MONITOR_STOP_SAMPLING; \
		ADAPT_MONITOR_TRACE((thread), (monitor), Trc_THR_Adapt_StopSampling); \
	} while (0)

#define ADAPT_DISABLE_SPIN_CHECK(thread, monitor) \
	do { \
		if (IS_ADAPTIVE_SPIN_REQUIRED(monitor)) { \
			if ((IS_ADAPT_HOLDTIME_ENABLED((thread), (monitor)) && ((monitor)->tracing->holdtime_count > 0) && (JLM_AVERAGE_HOLDTIME(monitor) > (thread)->library->adaptSpinHoldtime)) \
				|| (IS_ADAPT_SLOW_PERCENT_ENABLED((thread), (monitor)) && ((monitor)->tracing->enter_count > 100) && (JLM_SLOW_PERCENT(monitor) >= (thread)->library->adaptSpinSlowPercent)) \
			) { \
				if (0 == ((monitor)->flags & J9THREAD_MONITOR_DISABLE_SPINNING)) { \
					(monitor)->flags |= J9THREAD_MONITOR_DISABLE_SPINNING; \
					DISABLE_RAW_MONITOR_SPIN((thread), (monitor)); \
					if (0 == (thread)->library->adaptSpinKeepSampling) { \
						ADAPT_DISABLE_SAMPLING((thread), (monitor)); \
					} \
				} \
			} else if (0 != ((monitor)->flags & J9THREAD_MONITOR_DISABLE_SPINNING)) { \
				(monitor)->flags &= ~J9THREAD_MONITOR_DISABLE_SPINNING; \
				ENABLE_RAW_MONITOR_SPIN((thread), (monitor)); \
			} \
		} \
	} while(0)

#define ADAPT_SAMPLE_STOP_MIN_COUNT(thread, monitor) ((thread)->library->adaptSpinSampleStopCount)

#define ADAPT_SAMPLE_STOP_MAX_HOLDTIME(thread, monitor) \
	(JLM_NON_RECURSIVE_ENTER_COUNT(monitor) * (thread)->library->adaptSpinSampleCountStopRatio)

#define SHOULD_DISABLE_ADAPT_SAMPLING(thread, monitor) \
	(IS_ADAPT_HOLDTIME_ENABLED((thread), (monitor)) && \
	 ((monitor)->tracing->holdtime_count > 0) && \
	 (JLM_NON_RECURSIVE_ENTER_COUNT(monitor) >= ADAPT_SAMPLE_STOP_MIN_COUNT((thread), (monitor))) && \
	 (JLM_AVERAGE_HOLDTIME(monitor) < ADAPT_SAMPLE_STOP_MAX_HOLDTIME((thread), (monitor))))

#define JLM_INIT_SAMPLE_INTERVAL(lib, monitor) ((monitor)->sampleCounter = (lib)->adaptSpinSampleThreshold)

#define DO_ADAPT_CHECK(thread, monitor) \
	do { \
		if (IS_ADAPTIVE_SPIN_REQUIRED(monitor)) { \
			if (IS_ADAPT_SAMPLING_ENABLED((thread), (monitor))) { \
				if (0 == (monitor)->sampleCounter) { \
					JLM_INIT_SAMPLE_INTERVAL((thread)->library, (monitor)); \
					if (SHOULD_DISABLE_ADAPT_SAMPLING((thread), (monitor))) { \
						ADAPT_DISABLE_SAMPLING((thread), (monitor)); \
					} \
				} else { \
					(monitor)->sampleCounter--; \
				} \
			} \
		} \
	} while (0)

#define IS_ADAPT_SAMPLING_ENABLED(thread, monitor) \
	(IS_JLM_SAMPLE_ENABLED((thread), (monitor)) && (0 == ((monitor)->flags & J9THREAD_MONITOR_STOP_SAMPLING)))

#define TAKE_JLM_SAMPLE(thread, monitor) \
	(((thread)->library->flags & J9THREAD_LIB_FLAG_JLM_ENABLED) || \
	 (IS_ADAPT_SAMPLING_ENABLED((thread), (monitor)) && (0 == (monitor)->sampleCounter)))

#else /* OMR_THR_ADAPTIVE_SPIN */
#define DO_ADAPT_CHECK(thread, monitor)
#define ADAPT_DISABLE_SPIN_CHECK(thread, monitor)
#define TAKE_JLM_SAMPLE(thread, monitor) IS_JLM_ENABLED(thread)
#endif /* OMR_THR_ADAPTIVE_SPIN */

#if defined(OMR_THR_JLM)
#if defined(OMR_THR_ADAPTIVE_SPIN)

#ifdef OMR_THR_THREE_TIER_LOCKING
#define SHOULD_SAMPLE_MONITOR_TYPE(monitor)  (1)
#else /* OMR_THR_THREE_TIER_LOCKING */
/* We don't sample for non-object monitors as there is no spinning for these monitors when three tier spinning is disabled. */
#define SHOULD_SAMPLE_MONITOR_TYPE(monitor) IS_OBJECT_MONITOR(monitor)
#endif /* OMR_THR_THREE_TIER_LOCKING */

/* Start sampling once we hit a slow enter for the very first time.
 * It is possible to tell if it's the very first time if:
 *   a) enter_count is 0
 *   b) J9THREAD_MONITOR_STOP_SAMPLING is set (which is the case initially)
 */
#define SHOULD_ENABLE_SAMPLING(thread, monitor, isSlowEnter) \
	((isSlowEnter) && \
	 SHOULD_SAMPLE_MONITOR_TYPE(monitor) && \
	 (NULL != (monitor)->tracing) && \
	 (0 == (monitor)->tracing->enter_count) && \
	 (0 != ((monitor)->flags & J9THREAD_MONITOR_STOP_SAMPLING)))
#else /* OMR_THR_ADAPTIVE_SPIN */
#define SHOULD_ENABLE_SAMPLING(thread, monitor, isSlowEnter)  (0)
#endif /* OMR_THR_ADAPTIVE_SPIN */

#define UPDATE_JLM_MON_ENTER(self, monitor, isRecursiveEnter, isSlowEnter) \
	do { \
		if (SHOULD_ENABLE_SAMPLING((self), (monitor), (isSlowEnter))) { \
			/* Enable sampling once we hit a slow enter. */ \
			(monitor)->flags &= ~J9THREAD_MONITOR_STOP_SAMPLING; \
			Trc_THR_Adapt_StartSampling((IS_OBJECT_MONITOR(monitor) ? "object" : "system"), (monitor)); \
		} \
		if (TAKE_JLM_SAMPLE((self), (monitor))) { \
			ASSERT((monitor)->tracing); \
			(monitor)->tracing->enter_count++; \
			/* handle the roll-over case */ \
			if ((monitor)->tracing->enter_count == 0){ \
				(monitor)->tracing->enter_count++; \
				(monitor)->tracing->recursive_count = 0; \
				(monitor)->tracing->slow_count = 0; \
				(monitor)->tracing->holdtime_count = 0; \
				(monitor)->tracing->holdtime_sum = 0; \
				(monitor)->tracing->holdtime_avg = 0; \
				(monitor)->tracing->spin2_count = 0; \
				(monitor)->tracing->yield_count = 0; \
				(monitor)->tracing->spin_time = 0; \
			} \
			if (isSlowEnter) { \
				(monitor)->tracing->slow_count++; \
				(monitor)->flags |= J9THREAD_MONITOR_SLOW_ENTER; \
			} \
			if (isRecursiveEnter) { \
				(monitor)->tracing->recursive_count++; \
			} else { \
				UPDATE_JLM_MON_ENTER_HOLD_TIMES((self), (monitor)); \
			} \
		} \
	} while (0)
#else /* OMR_THR_JLM */
#define UPDATE_JLM_MON_ENTER(self, monitor, isRecursiveEnter, isSlowEnter)
#endif /* OMR_THR_JLM */

#define IS_SLOW_ENTER  (1)
#define IS_RECURSIVE_ENTER  (1)

#if defined(OMR_THR_JLM_HOLD_TIMES)
#define UPDATE_JLM_MON_ENTER_HOLD_TIMES(self, monitor) \
	do { \
		if (IS_JLM_TIME_STAMPS_ENABLED((self), (monitor))) { \
			ASSERT((monitor)->tracing); \
			ASSERT((self)->tracing); \
			(monitor)->tracing->enter_pause_count = (self)->tracing->pause_count; \
			(monitor)->tracing->enter_time = GET_HIRES_CLOCK(); \
		} \
	} while (0)
#else /* OMR_THR_JLM_HOLD_TIMES */
#define UPDATE_JLM_MON_ENTER_HOLD_TIMES(self, monitor)
#endif /* OMR_THR_JLM_HOLD_TIMES */

#if defined(OMR_THR_JLM)
#define CLEAR_MONITOR_FLAGS(monitor, flagsToClear) \
	do { \
		if (0 != ((monitor)->flags & (flagsToClear))) { \
			(monitor)->flags &= ~(flagsToClear); \
		} \
	} while (0)

/* NOTE: This is only done for non-recursive exits. */
#define UPDATE_JLM_MON_EXIT(self, monitor) \
	do { \
		if (TAKE_JLM_SAMPLE((self), (monitor))) { \
			ASSERT((monitor)->tracing); \
			if (0 != ((monitor)->flags & J9THREAD_MONITOR_IGNORE_ENTER)) { \
				(monitor)->tracing->enter_count--; \
				if (0 != ((monitor)->flags & J9THREAD_MONITOR_SLOW_ENTER)) { \
					(monitor)->tracing->slow_count--; \
				} \
			} else { \
				UPDATE_JLM_MON_EXIT_HOLD_TIMES((self), (monitor)); \
			} \
		} \
		CLEAR_MONITOR_FLAGS((monitor), (J9THREAD_MONITOR_IGNORE_ENTER | J9THREAD_MONITOR_SLOW_ENTER)); \
		DO_ADAPT_CHECK((self), (monitor)); \
	} while(0)

#define UPDATE_JLM_MON_WAIT(self, monitor) \
	do { \
		if (TAKE_JLM_SAMPLE((self), (monitor))) { \
			ASSERT((monitor)->tracing); \
			if (0 != ((monitor)->flags & J9THREAD_MONITOR_IGNORE_ENTER)) { \
				(monitor)->tracing->enter_count--; \
				if (0 != ((monitor)->flags & J9THREAD_MONITOR_SLOW_ENTER)) { \
					(monitor)->tracing->slow_count--; \
				} \
			} else { \
				UPDATE_JLM_MON_EXIT_HOLD_TIMES((self), (monitor)); \
				if (monitor->flags & J9THREAD_MONITOR_JLM_TIME_STAMP_INVALIDATOR) { \
					self->tracing->pause_count++; \
				} \
			} \
		} \
		CLEAR_MONITOR_FLAGS((monitor), (J9THREAD_MONITOR_IGNORE_ENTER | J9THREAD_MONITOR_SLOW_ENTER)); \
		DO_ADAPT_CHECK((self), (monitor)); \
	} while(0)

#else /* OMR_THR_JLM */
#define UPDATE_JLM_MON_EXIT(self, monitor)
#define UPDATE_JLM_MON_WAIT(self, monitor)
#endif /* OMR_THR_JLM */

#if defined(OMR_THR_JLM_HOLD_TIMES)
#define UPDATE_JLM_MON_EXIT_HOLD_TIMES(self, monitor) \
	do { \
		if ((NULL != (monitor)->tracing) && (0 != (monitor)->tracing->enter_time)) { \
			ASSERT((self)->tracing); \
			/* If enter count is 0, this monitor was held when jlm was initialized, so ignore stats this time */ \
			if ((monitor)->tracing->enter_count > 0) { \
				if ((self)->tracing->pause_count == (monitor)->tracing->enter_pause_count) { \
					omrtime_delta_t holdTime = GET_HIRES_CLOCK() - (monitor)->tracing->enter_time; \
					if (holdTime > 0) { \
						if ((0 == (self)->library->clock_skew) || ((omrtime_t)holdTime < (self)->library->clock_skew)) { \
							uintptr_t holdTimeCount = (monitor)->tracing->holdtime_count + 1; \
							(monitor)->tracing->holdtime_count = holdTimeCount; \
							(monitor)->tracing->holdtime_sum += (omrtime_t)holdTime; \
							(monitor)->tracing->holdtime_avg = (monitor)->tracing->holdtime_sum / ((uint64_t)holdTimeCount); \
							ADAPT_DISABLE_SPIN_CHECK((self), (monitor)); \
						} \
					} \
				} \
			} \
			(monitor)->tracing->enter_time = 0; \
		} \
	} while(0)
#else /* OMR_THR_JLM_HOLD_TIMES */
#define UPDATE_JLM_MON_EXIT_HOLD_TIMES(self, monitor)
#endif /* OMR_THR_JLM_HOLD_TIMES */

#ifdef __cplusplus
}
#endif

#endif     /* threaddef_h */
