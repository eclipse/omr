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
#ifndef thrdsup_h
#define thrdsup_h

#if !defined(J9ZOS390)
#define J9_POSIX_THREADS
#endif

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <setjmp.h>
#include "omrcomp.h"
#include "omrutilbase.h"

#if (defined(LINUX) || defined(OSX) || defined(MVS) || defined(J9ZOS390))
#include <sys/time.h>
#if defined(OSX)
#include <mach/clock.h>
#include <mach/mach.h>
#endif /* defined(OSX) */
#endif /* (defined(LINUX) || defined(OSX) || defined(MVS) || defined(J9ZOS390)) */


#include "omrmutex.h"

#define J9THR_STR(x) #x

/* ostypes */

typedef pthread_t OSTHREAD;
typedef pthread_key_t TLSKEY;
typedef pthread_cond_t COND;

#if defined(OMR_THR_FORK_SUPPORT)
typedef pthread_mutex_t* J9OSMutex;
typedef pthread_cond_t* J9OSCond;
#else /* defined(OMR_THR_FORK_SUPPORT) */
typedef COND J9OSCond;
typedef MUTEX J9OSMutex;
#endif /* defined(OMR_THR_FORK_SUPPORT) */

#define WRAPPER_TYPE void*
typedef void* WRAPPER_ARG;
#define WRAPPER_RETURN() return NULL
typedef WRAPPER_TYPE (*WRAPPER_FUNC)(WRAPPER_ARG);

#if defined(LINUX) || defined(AIXPPC)
#include <semaphore.h>
typedef sem_t OSSEMAPHORE;
#elif defined(OSX) /* defined(LINUX) || defined(AIXPPC) */
#include <dispatch/dispatch.h>
typedef dispatch_semaphore_t OSSEMAPHORE;
#elif defined(J9ZOS390) /* defined(OSX) */
typedef struct zos_sem_t {
	int count;
	struct J9ThreadMonitor *monitor;
} zos_sem_t;
typedef zos_sem_t OSSEMAPHORE;
#else /* defined(J9ZOS390) */
typedef intptr_t OSSEMAPHORE;
#endif /* defined(LINUX) || defined(AIXPPC) */


#include "thrtypes.h"

int linux_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
intptr_t init_thread_library(void);
intptr_t set_pthread_priority(pthread_t handle, omrthread_prio_t j9ThreadPriority);
intptr_t set_pthread_name(pthread_t self, pthread_t thread, const char *name);

extern struct J9ThreadLibrary default_library;

/* priority_map */

extern int priority_map[];

/* For real-time, priority map encodes scheduling policy and priority for each entry */
#define PRIORITY_MAP_ADJUSTED_POLICY(policy) ((policy)<<24)

#ifdef OMR_ENV_DATA64
#define J9DIV_T ldiv_t
#define J9DIV ldiv
#else
#define J9DIV_T div_t
#define J9DIV div
#endif

/**
 * Define the clock used to time pthread_cond_timedwait().
 *
 * CLOCK_REALTIME is affected by changes to the system time. This is the default
 * clock used by pthreads.
 *
 * CLOCK_MONOTONIC should not be affected by changes to the system time. Using
 * this clock should preserve the duration of relative timed waits even when
 * the system time is adjusted.
 *
 * If we fail to set the cond clock to CLOCK_MONOTONIC, we default back to CLOCK_REALTIME.
 *
 * For AIX:
 * pthread_condattr_setclock() is only available on AIX 5.3 and higher. In older streams
 * where we compile against AIX 5.2, we need to dynamically look up the methods at runtime.
 *
 * iSeries does not support CLOCK_MONOTONIC.
 *
 * CMVC 199234 - use CLOCK_MONOTONIC also on Linux
 */
#if (defined(AIXPPC) && !defined(J9OS_I5)) || defined(LINUX)
#define J9THREAD_USE_MONOTONIC_COND_CLOCK 1
#else /* (defined(AIXPPC) && !defined(J9OS_I5)) || defined(LINUX) */
#define J9THREAD_USE_MONOTONIC_COND_CLOCK 0
#endif /* (defined(AIXPPC) && !defined(J9OS_I5)) || defined(LINUX) */

#if J9THREAD_USE_MONOTONIC_COND_CLOCK
#define TIMEOUT_CLOCK timeoutClock
extern clockid_t timeoutClock;
extern pthread_condattr_t *defaultCondAttr;
#else /* J9THREAD_USE_MONOTONIC_COND_CLOCK */
#define TIMEOUT_CLOCK CLOCK_REALTIME
#endif /* J9THREAD_USE_MONOTONIC_COND_CLOCK */

#if ( defined(MVS) || defined(J9ZOS390) )
#define SETUP_TIMEOUT(ts_, millis, nanos) {								\
		struct timeval tv_;											\
		J9DIV_T secs_ = J9DIV(millis, 1000);					\
		int nanos_ = secs_.rem * 1000000 + nanos;				\
		gettimeofday(&tv_, NULL);								\
		nanos_ += tv_.tv_usec * 1000;						\
		if (nanos_ >= 1000000000) {							\
			ts_.tv_sec = tv_.tv_sec + secs_.quot + 1;		\
			ts_.tv_nsec = nanos_ - 1000000000;			\
		} else {														\
			ts_.tv_sec = tv_.tv_sec + secs_.quot;			\
			ts_.tv_nsec = nanos_;								\
		} }
#elif defined(OSX)
#define SETUP_TIMEOUT(ts_, millis, nanos) {						\
		J9DIV_T secs_ = J9DIV(millis, 1000);					\
		int nanos_ = secs_.rem * 1000000 + nanos;				\
		if (nanos_ >= 1000000000) {								\
			ts_.tv_sec = secs_.quot + 1;						\
			ts_.tv_nsec = nanos_ - 1000000000;					\
		} else {												\
			ts_.tv_sec = secs_.quot;							\
			ts_.tv_nsec = nanos_;								\
		} }
#else /* defined(OSX) */
#define SETUP_TIMEOUT(ts_, millis, nanos) {								\
		J9DIV_T secs_ = J9DIV(millis, 1000);					\
		int nanos_ = secs_.rem * 1000000 + nanos;				\
 		clock_gettime(TIMEOUT_CLOCK, &ts_);		\
		nanos_ += ts_.tv_nsec;									\
		if (nanos_ >= 1000000000) {							\
			ts_.tv_sec += secs_.quot + 1;						\
			ts_.tv_nsec = nanos_ - 1000000000;			\
		} else {														\
			ts_.tv_sec += secs_.quot;							\
			ts_.tv_nsec = nanos_;								\
		} }
#endif /* defined(OSX) */
/* COND_DESTROY */

#define COND_DESTROY(cond) pthread_cond_destroy(&(cond))

/* COND_WAIT */

/* NOTE: the calling thread must already own mutex */
/* NOTE: a timeout less than zero indicates infinity */

#define COND_WAIT(cond, mutex) \
	do {	\
		pthread_cond_wait(&(cond), &(mutex))

#define COND_WAIT_LOOP()	} while(1)

/* THREAD_SELF */

#define THREAD_SELF() (pthread_self())

/* THREAD_YIELD */

#if (defined(LINUX) || defined(AIXPPC) || defined(OSX))
#define THREAD_YIELD() (sched_yield())

#ifdef OMR_THR_YIELD_ALG
#define THREAD_YIELD_NEW(sequentialYields) {\
	omrthread_t thread = MACRO_SELF();\
	if (thread->library->yieldAlgorithm ==  J9THREAD_LIB_YIELD_ALGORITHM_INCREASING_USLEEP) {\
		if ((sequentialYields >> 5 ) != 0){\
			usleep(thread->library->yieldUsleepMultiplier*16);\
		} else if ((sequentialYields >> 4 ) != 0){\
			usleep(thread->library->yieldUsleepMultiplier*8);\
		} else if ((sequentialYields >> 3 ) != 0){\
			usleep(thread->library->yieldUsleepMultiplier*4);\
		} else if ((sequentialYields >> 2 ) != 0){\
			usleep(thread->library->yieldUsleepMultiplier*2);\
		} else if ((sequentialYields >> 1 ) != 0){\
			usleep(thread->library->yieldUsleepMultiplier);\
		} else {\
			usleep(thread->library->yieldUsleepMultiplier);\
		}\
	} else if (thread->library->yieldAlgorithm ==  J9THREAD_LIB_YIELD_ALGORITHM_CONSTANT_USLEEP){ \
		usleep(thread->library->yieldUsleepMultiplier);\
	} else { \
		sched_yield();\
	}\
}
#endif
#endif /* (defined(LINUX) || defined(AIXPPC) || defined(OSX)) */

#if defined(MVS) || defined (J9ZOS390)
#define THREAD_YIELD() (pthread_yield(0))
#endif



/* last chance. If it's not defined by now, use yield */
#ifndef THREAD_YIELD
#define THREAD_YIELD() (yield())
#endif

/* THREAD_CANCEL */

/* pthread_cancel is asynchronous. Use join to wait for it to complete */
#define THREAD_CANCEL(thread) (pthread_cancel(thread) || pthread_join(thread, NULL))



/* COND_NOTIFY_ALL */

#define COND_NOTIFY_ALL(cond) pthread_cond_broadcast(&(cond))

/* COND_NOTIFY */

#define COND_NOTIFY(cond) pthread_cond_signal(&(cond))

/* COND_WAIT_IF_TIMEDOUT */

/* NOTE: the calling thread must already own the mutex! */

#if defined(LINUX) && defined(J9X86)
#define PTHREAD_COND_TIMEDWAIT(x,y,z) linux_pthread_cond_timedwait(x,y,z)
#elif defined(OSX)
#define PTHREAD_COND_TIMEDWAIT(x,y,z) pthread_cond_timedwait_relative_np(x,y,z)
#else /* defined(OSX) */
#define PTHREAD_COND_TIMEDWAIT(x,y,z) pthread_cond_timedwait(x,y,z)
#endif /* defined(OSX) */

#if defined(J9ZOS390)
#define COND_WAIT_RC_TIMEDOUT -1
#else
#define COND_WAIT_RC_TIMEDOUT ETIMEDOUT
#endif

#define COND_WAIT_IF_TIMEDOUT(cond, mutex, millis, nanos) 											\
	do {																													\
		struct timespec ts_;																							\
		SETUP_TIMEOUT(ts_, millis, nanos);																						\
		while (1) {																										\
				if (PTHREAD_COND_TIMEDWAIT(&(cond), &(mutex), &ts_) == COND_WAIT_RC_TIMEDOUT)

#define COND_WAIT_TIMED_LOOP()		}	} while(0)



/* COND_INIT */

#if J9THREAD_USE_MONOTONIC_COND_CLOCK
#define COND_INIT(cond) (pthread_cond_init(&(cond), defaultCondAttr) == 0)
#else
#define COND_INIT(cond) (pthread_cond_init(&(cond), NULL) == 0)
#endif

#define THREAD_EXIT() pthread_exit(NULL)

/* THREAD_DETACH */

#if defined(J9ZOS390)
#define THREAD_DETACH(thread) pthread_detach(&(thread))
#else
#define THREAD_DETACH(thread) pthread_detach(thread)
#endif

/* THREAD_SET_NAME */
#if defined(LINUX) || defined(OSX)
#define THREAD_SET_NAME(self, thread, name) set_pthread_name((self), (thread), (name))
#else /* defined(LINUX) || defined(OSX) */
#define THREAD_SET_NAME(self, thread, name) 0 /* not implemented */
#endif /* defined(LINUX) || defined(OSX) */

/* THREAD_SET_PRIORITY */
#define THREAD_SET_PRIORITY(thread, priority) set_pthread_priority((thread), (priority))

/* TLS functions */
#define TLS_ALLOC(key) (pthread_key_create(&key, NULL))
#define TLS_ALLOC_WITH_DESTRUCTOR(key, destructor) (pthread_key_create(&(key), (destructor)))
#define TLS_DESTROY(key) (pthread_key_delete(key))
#define TLS_SET(key, value) (pthread_setspecific(key, value))
#if defined(J9ZOS390)
#define TLS_GET(key) (pthread_getspecific_d8_np(key))
#else
#define TLS_GET(key) (pthread_getspecific(key))
#endif

/* SEM_CREATE */

#if defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX)
#define SEM_CREATE(threadLibrary, initValue) omrthread_allocate_memory(threadLibrary, sizeof(OSSEMAPHORE), OMRMEM_CATEGORY_THREADS)
#else /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX) */
#define SEM_CREATE(threadLibrary, initValue)
#endif /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX) */
/* SEM_INIT */

#if defined(LINUX) || defined(AIXPPC)
#define SEM_INIT(sm, pshrd, inval)	\
	(sem_init((sem_t*)(sm), pshrd, inval))
#elif defined(OSX)
intptr_t dispatch_semaphore_init(j9sem_t s, int initValue);
#define SEM_INIT(sm, pshrd, inval)	\
	(dispatch_semaphore_init((sm),(inval)))
#elif defined(J9ZOS390)
#define SEM_INIT(sm,pshrd,inval)  (sem_init_zos(sm, pshrd, inval))
#else /* defined(J9ZOS390) */
#define SEM_INIT(sm,pshrd,inval)
#endif /* defined(J9ZOS390) */
/* SEM_DESTROY */

#if defined(LINUX) || defined(AIXPPC)
#define SEM_DESTROY(sm)	\
	(sem_destroy((sem_t*)(sm)))
#elif defined(J9ZOS390)
#define SEM_DESTROY(sm)	\
	(sem_destroy_zos(sm))
#else
#define SEM_DESTROY(sm) 0
#endif
/* SEM_FREE */

#if defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX)
#define	SEM_FREE(lib, s)  	omrthread_free_memory(lib, s);
#else /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX) */
#define SEM_FREE(lib, s)
#endif /* defined(LINUX) || defined(AIXPPC) || defined(J9ZOS390) || defined(OSX) */
/* SEM_POST */

#if defined(LINUX) || defined(AIXPPC)
#define SEM_POST(smP)	\
	(sem_post((sem_t*)(smP)))
#elif defined(OSX)
#define SEM_POST(smP)	\
	(dispatch_semaphore_signal((dispatch_semaphore_t)(smP)))
#elif defined(J9ZOS390)
#define SEM_POST(smP)   (sem_post_zos(smP))
#else
#define SEM_POST(sm)
#endif
/* SEM_WAIT */

#if defined(LINUX) || defined(AIXPPC)
#define SEM_WAIT(smP)	\
	(sem_wait((sem_t*)(smP)))
#elif defined(OSX)
#define SEM_WAIT(smP)	\
	(dispatch_semaphore_wait((dispatch_semaphore_t)(smP), DISPATCH_TIME_FOREVER))
#elif defined(J9ZOS390)
#define SEM_WAIT(smP)           (sem_wait_zos(smP))
#else
#define SEM_WAIT(sm)
#endif
/* SEM_GETVALUE */

#if defined(LINUX) || defined(AIXPPC)
#define SEM_GETVALUE(smP, intP)	\
	(sem_getvalue(smP, intP))
#elif defined(J9ZOS390)
#define SEM_GETVALUE(sm) (sem_getvalue_zos(smP, intP))
#else
#define SEM_GETVALUE(sm)
#endif
/* GET_HIRES_CLOCK */

#ifdef OMR_THR_JLM_HOLD_TIMES
#define GET_HIRES_CLOCK()  getTimebase()
#endif /* OMR_THR_JLM_HOLD_TIMES */
/* ENABLE_OS_THREAD_STATS */

#if defined (AIXPPC)
#define ENABLE_OS_THREAD_STATS(self)\
	do {\
		struct rusage usageInfo;\
		memset(&usageInfo,0,sizeof(usageInfo));\
		pthread_getrusage_np((self)->handle, &usageInfo, PTHRDSINFO_RUSAGE_START);\
	} while(0)
#else
#define ENABLE_OS_THREAD_STATS(self)
#endif

#if defined(OMR_THR_FORK_SUPPORT)

intptr_t j9OSMutex_allocAndInit(J9OSMutex *mutex);
intptr_t j9OSMutex_freeAndDestroy(J9OSMutex mutex);
intptr_t j9OSMutex_enter(J9OSMutex mutex);
intptr_t j9OSMutex_exit(J9OSMutex mutex);
intptr_t j9OSMutex_tryEnter(J9OSMutex mutex);
intptr_t j9OSCond_allocAndInit(J9OSCond *cond);
intptr_t j9OSCond_freeAndDestroy(J9OSCond cond);
intptr_t j9OSCond_notify(J9OSCond cond);
intptr_t j9OSCond_notifyAll(J9OSCond cond);

#define J9OSCOND_WAIT_IF_TIMEDOUT(cond, mutex, millis, nanos) 							\
	do {																				\
		struct timespec ts_;															\
		SETUP_TIMEOUT(ts_, millis, nanos);												\
		while (1) {																		\
			if (PTHREAD_COND_TIMEDWAIT(cond, mutex, &ts_) == COND_WAIT_RC_TIMEDOUT)
#define J9OSCOND_WAIT_TIMED_LOOP()		}	} while(0)

#define J9OSCOND_WAIT(cond, mutex) \
	do {	\
		pthread_cond_wait((cond), (mutex))
#define J9OSCOND_WAIT_LOOP()	} while(1)

#define J9OSMUTEX_INIT(mutex) j9OSMutex_allocAndInit(&(mutex))
#define J9OSMUTEX_DESTROY(mutex) j9OSMutex_freeAndDestroy((mutex))
#define J9OSMUTEX_ENTER(mutex) j9OSMutex_enter((mutex))
#define J9OSMUTEX_EXIT(mutex) j9OSMutex_exit((mutex))
#define J9OSMUTEX_TRY_ENTER(mutex) j9OSMutex_tryEnter((mutex))
#define J9OSCOND_INIT(cond) j9OSCond_allocAndInit(&(cond))
#define J9OSCOND_DESTROY(cond) j9OSCond_freeAndDestroy((cond))
#define J9OSCOND_NOTIFY(cond) j9OSCond_notify((cond))
#define J9OSCOND_NOTIFY_ALL(cond) j9OSCond_notifyAll((cond))

#else /* defined(OMR_THR_FORK_SUPPORT) */

#define J9OSMUTEX_INIT(mutex) MUTEX_INIT((mutex))
#define J9OSMUTEX_DESTROY(mutex) MUTEX_DESTROY((mutex))
#define J9OSMUTEX_ENTER(mutex) MUTEX_ENTER((mutex))
#define J9OSMUTEX_EXIT(mutex) MUTEX_EXIT((mutex))
#define J9OSMUTEX_TRY_ENTER(mutex) MUTEX_TRY_ENTER((mutex))
#define J9OSCOND_INIT(cond) COND_INIT((cond))
#define J9OSCOND_DESTROY(cond) COND_DESTROY((cond))
#define J9OSCOND_NOTIFY(cond) COND_NOTIFY((cond))
#define J9OSCOND_NOTIFY_ALL(cond) COND_NOTIFY_ALL((cond))
#define J9OSCOND_WAIT_IF_TIMEDOUT(cond, mutex, millis, nanos) COND_WAIT_IF_TIMEDOUT((cond), (mutex), (millis), (nanos))
#define J9OSCOND_WAIT_TIMED_LOOP() COND_WAIT_TIMED_LOOP()
#define J9OSCOND_WAIT(cond, mutex) COND_WAIT((cond), (mutex))
#define J9OSCOND_WAIT_LOOP() COND_WAIT_LOOP()

#endif /* defined(OMR_THR_FORK_SUPPORT) */

#endif     /* thrdsup_h */
