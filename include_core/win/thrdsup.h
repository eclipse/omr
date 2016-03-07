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

/* windows.h defined uintptr_t.  Ignore its definition */
#define UDATA UDATA_win32_
#include <windows.h>
#undef UDATA	/* this is safe because our UDATA is a typedef, not a macro */

#include <process.h>

#include "omrmutex.h"

/* ostypes */

typedef HANDLE OSTHREAD;
typedef DWORD TLSKEY;
typedef HANDLE COND;

typedef COND J9OSCond;
typedef MUTEX J9OSMutex;

#define WRAPPER_TYPE unsigned __stdcall
typedef void* WRAPPER_ARG;
#define WRAPPER_RETURN() return 0
typedef unsigned (__stdcall* WRAPPER_FUNC)(WRAPPER_ARG);

typedef HANDLE OSSEMAPHORE;

#include "omrcomp.h"
#include "thrtypes.h"

intptr_t init_thread_library(void);

extern const int priority_map[];

extern struct J9ThreadLibrary default_library;

/* COND_DESTROY */

#define COND_DESTROY(cond) CloseHandle(cond)

/* THREAD_SELF */

#define THREAD_SELF() (GetCurrentThread())
/* THREAD_YIELD */

#define THREAD_YIELD() (SwitchToThread())

/* COND_NOTIFY_ALL */

#define COND_NOTIFY_ALL(cond) SetEvent(cond)

/* COND_WAIT_IF_TIMEDOUT */

/* NOTE: the calling thread must already own mutex */

#define ADJUST_TIMEOUT(millis, nanos) (((nanos) && ((millis) != ((intptr_t) (((uintptr_t)-1) >> 1)))) ? ((millis) + 1) : (millis))

#define COND_WAIT_IF_TIMEDOUT(cond, mutex, millis, nanos) 								\
	do {																										\
		DWORD starttime_ = GetTickCount(); \
		intptr_t initialtimeout_, timeout_, rc_;				\
		initialtimeout_ = timeout_ = ADJUST_TIMEOUT(millis, nanos);														\
		while (1) {																							\
			ResetEvent((cond));																			\
			MUTEX_EXIT(mutex);																		\
			rc_ = WaitForSingleObject((cond), (DWORD)timeout_);										\
			MUTEX_ENTER(mutex);																	\
			if (rc_ == WAIT_TIMEOUT)

#define COND_WAIT_TIMED_LOOP()																\
			timeout_ = initialtimeout_ - (GetTickCount() - starttime_);										\
			if (timeout_ < 0) { timeout_ = 0; } \
		}	} while(0)

/* COND_WAIT */

/* NOTE: the calling thread must already own mutex */

#define COND_WAIT(cond, mutex) \
	do { \
		ResetEvent((cond));	\
		MUTEX_EXIT(mutex);	\
		WaitForSingleObject((cond), INFINITE);	\
		MUTEX_ENTER(mutex);

#define COND_WAIT_LOOP()	} while(1)



/* COND_INIT */

#define COND_INIT(cond) ((cond = CreateEvent(NULL, TRUE, FALSE, NULL)) != NULL)

/* TLS_DESTROY */

/* THREAD_CANCEL */

#define THREAD_CANCEL(thread) (TerminateThread(thread, (DWORD)-1)&&WaitForSingleObject(thread,INFINITE))

/* THREAD_EXIT */

#define THREAD_EXIT() _endthreadex(0)

/* THREAD_DETACH */

#define THREAD_DETACH(thread) /* no need to do anything */

/* THREAD_SET_NAME */

#define THREAD_SET_NAME(self, thread, name) 0 /* not implemented */

/* THREAD_SET_PRIORITY */

#define THREAD_SET_PRIORITY(thread, priority) (!SetThreadPriority((thread), omrthread_get_mapped_priority(priority)))

#define TLS_ALLOC(key) ((key = TlsAlloc()) == 0xFFFFFFFF)
#define TLS_ALLOC_WITH_DESTRUCTOR(key, destructor) (-1)
#define TLS_DESTROY(key) (TlsFree(key))
#define TLS_SET(key, value) (TlsSetValue(key, value))
#define TLS_GET(key) (TlsGetValue(key))


/* SEM_CREATE */

/* Arbitrary maximum count */

#define SEM_CREATE(lib, inval) CreateSemaphore(NULL,inval,2028,NULL)

/* SEM_INIT */

#define SEM_INIT(sm,pshrd,inval)  (sm != NULL) ? 0: -1
/* SEM_DESTROY */

#define SEM_DESTROY(sm)  CloseHandle(sm)

/* SEM_FREE */

#define SEM_FREE(lib, s)
/* SEM_POST */

#define SEM_POST(sm)  (ReleaseSemaphore((sm),1,NULL) ? 0 : -1)
/* SEM_WAIT */


#define SEM_WAIT(sm)  ((WaitForSingleObject((sm), INFINITE) == WAIT_FAILED) ? -1 : 0)


/* SEM_GETVALUE */

#define SEM_GETVALUE(sm)
/* GET_HIRES_CLOCK */

#ifdef OMR_THR_JLM_HOLD_TIMES
#define	GET_HIRES_CLOCK()	getTimebase()
#endif /* OMR_THR_JLM_HOLD_TIMES */
/* ENABLE_OS_THREAD_STATS */

#define ENABLE_OS_THREAD_STATS(self)

#define J9OSMUTEX_INIT(mutex) MUTEX_INIT((mutex))
#define J9OSMUTEX_DESTROY(mutex) MUTEX_DESTROY((mutex))
#define J9OSMUTEX_ENTER(mutex) MUTEX_ENTER((mutex))
#define J9OSMUTEX_EXIT(mutex) MUTEX_EXIT((mutex))
#define J9OSMUTEX_TRY_ENTER(mutex) MUTEX_TRY_ENTER((mutex))
#define J9OSMUTEX_FREE(mutex) (0)
#define J9OSCOND_INIT(cond) COND_INIT((cond))
#define J9OSCOND_DESTROY(cond) COND_DESTROY((cond))
#define J9OSCOND_NOTIFY(cond) COND_NOTIFY((cond))
#define J9OSCOND_NOTIFY_ALL(cond) COND_NOTIFY_ALL((cond))
#define J9OSCOND_FREE(cond) (0)
#define J9OSCOND_WAIT_IF_TIMEDOUT(cond, mutex, millis, nanos) COND_WAIT_IF_TIMEDOUT((cond), (mutex), (millis), (nanos))
#define J9OSCOND_WAIT_TIMED_LOOP() COND_WAIT_TIMED_LOOP()
#define J9OSCOND_WAIT(cond, mutex) COND_WAIT((cond), (mutex))
#define J9OSCOND_WAIT_LOOP() COND_WAIT_LOOP()

#endif     /* thrdsup_h */
