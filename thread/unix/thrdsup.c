/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <pthread.h>
#include <stdlib.h>
#include "omrcomp.h"
#include "omrmutex.h"
#include "thrtypes.h"
#include "pool_api.h"
#include "threaddef.h"
#include "thread_internal.h"

#if defined(LINUX) && !defined(OMRZTPF)
#include <sys/prctl.h>
#endif /* defined(LINUX) */

#if defined(OMRZTPF)
#include <tpf/c_eb0eb.h>
#include <tpf/sysapi.h>
#include <tpf/tpfapi.h>
#endif /* if defined(OMRZTPF) */

#if (defined(LINUX) || defined(OSX)) && defined(J9X86)
#include <fpu_control.h>
#endif /* (defined(LINUX) || defined(OSX)) && defined(J9X86) */

#if J9THREAD_USE_MONOTONIC_COND_CLOCK
/*
 * The default cond clock is CLOCK_REALTIME.
 * In omrthread library init, we will attempt to switch it to CLOCK_MONOTONIC.
 * If the initialization fails, we revert to the default.
 */
clockid_t timeoutClock = CLOCK_REALTIME; /**< the clock used to derive absolute time from relative wait time */
pthread_condattr_t *defaultCondAttr = NULL; /**< attribute passed to pthread_cond_init(). NULL means the system default. */
static pthread_condattr_t defaultCondAttr_s; /* do not use directly */
static intptr_t initCondAttr(void);
#endif /* J9THREAD_USE_MONOTONIC_COND_CLOCK */

#if !defined(J9_PRIORITY_MAP) || defined(J9OS_I5) /* i5/OS always calls initialize_priority_map */
extern intptr_t initialize_priority_map(void);
#endif

int linux_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime);
intptr_t init_thread_library(void);

#ifdef J9ZOS390
static intptr_t zos_init_yielding(void);
#endif
intptr_t sem_getvalue_zos(j9sem_t s);
void call_omrthread_init(void);
intptr_t sem_trywait_zos(j9sem_t s);

#if defined (OMRZTPF)
void  ztpf_init_proc(void);
#endif /* defined (OMRZTPF) */

struct J9ThreadLibrary default_library;

pthread_once_t init_once = PTHREAD_ONCE_INIT;

void
call_omrthread_init(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);

#if defined(LINUX) || !defined(J9_PRIORITY_MAP) || defined(J9OS_I5) || defined(OSX)
	if (initialize_priority_map()) {
		goto thread_init_error;
	}
#endif /* defined(LINUX) || !defined(J9_PRIORITY_MAP) || defined(J9OS_I5) || defined(OSX) */

#ifdef J9ZOS390
	zos_init_yielding();
#endif

#if  defined(OMRZTPF)
	ztpf_init_proc();
#endif /* defined(OMRZTPF) */

#if J9THREAD_USE_MONOTONIC_COND_CLOCK
	initCondAttr(); /* ignore the result */
#endif

	omrthread_init(lib);
	return;

thread_init_error:
	lib->initStatus = -1;
}

intptr_t
init_thread_library(void)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	pthread_once(&init_once, call_omrthread_init);
	return lib->initStatus != 1;
}

#if defined(LINUX) || defined(OSX)
intptr_t
set_pthread_name(pthread_t self, pthread_t thread, const char *name)
{
	if (thread != self) {
		/* for Linux and OSX, the thread being named must be the current thread */
		return -1;
	}
#if defined(LINUX)
#ifndef PR_SET_NAME
#define PR_SET_NAME 15
#endif
#ifndef OMRZTPF
	prctl(PR_SET_NAME, name);
#endif
	/* we ignore the return value of prctl, since naming is not supported on some older linux distributions */
#else /* defined(LINUX) */
	pthread_setname_np(name);
#endif /* defined(LINUX) */
	return 0;
}
#endif /* defined(LINUX) || defined(OSX) */

intptr_t
osthread_join(omrthread_t self, omrthread_t threadToJoin)
{
#if defined(J9ZOS390)
	intptr_t j9thrRc = J9THREAD_SUCCESS;
	if (0 != pthread_join(threadToJoin->handle, NULL)) {
		self->os_errno = errno;
		j9thrRc = J9THREAD_ERR | J9THREAD_ERR_OS_ERRNO_SET;
	}
	return j9thrRc;
#else /* defined(J9ZOS390) */
	intptr_t j9thrRc = J9THREAD_SUCCESS;
	int rc = pthread_join(threadToJoin->handle, NULL);
	if (0 != rc) {
		self->os_errno = rc;
		j9thrRc = J9THREAD_ERR | J9THREAD_ERR_OS_ERRNO_SET;
	}
	return j9thrRc;
#endif /* defined(J9ZOS390) */
}

#if defined(LINUX) && defined(J9X86)
int
linux_pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
	/* This is a wrapper around the pthread_cond_timedwait which restores the
	 * fpu control word. The libpthread-0.9 version pthread_cond_timedwait on
	 * return resets the fpu control word to 0x37f
	 */

	int rValue, oldCW;
	_FPU_GETCW(oldCW);
	rValue = pthread_cond_timedwait(cond, mutex, abstime);
	oldCW &= 0xffff;
	_FPU_SETCW(oldCW);
	return rValue;
}
#endif

#if defined(J9ZOS390) && defined(OMR_INTERP_HAS_SEMAPHORES)

intptr_t
sem_init_zos(j9sem_t s, int pShared, int initValue)
{
	intptr_t rval;
	zos_sem_t *zs = (zos_sem_t *) s;

	zs->count = initValue;
	rval = omrthread_monitor_init_with_name(&zs->monitor, 0, "&zs->monitor");
	return rval;
}

intptr_t
sem_destroy_zos(j9sem_t s)
{
	intptr_t rval = 0;
	zos_sem_t *zs = (zos_sem_t *) s;
	if (zs->monitor) {
		rval = omrthread_monitor_destroy(zs->monitor);
	}
	return rval;
}

intptr_t
sem_wait_zos(j9sem_t s)
{
	zos_sem_t *zs = (zos_sem_t *) s;

	omrthread_monitor_enter(zs->monitor);
	while (zs->count == 0) {
		omrthread_monitor_wait(zs->monitor);
	}
	zs->count--;
	omrthread_monitor_exit(zs->monitor);

	return 0;
}

intptr_t
sem_post_zos(j9sem_t s)
{
	zos_sem_t *zs = (zos_sem_t *) s;

	omrthread_monitor_enter(zs->monitor);
	zs->count++;
	omrthread_monitor_notify(zs->monitor);
	omrthread_monitor_exit(zs->monitor);

	return 0;
}


intptr_t
sem_getvalue_zos(j9sem_t s)
{
	uintptr_t rval;
	zos_sem_t *zs = (zos_sem_t *) s;
	rval =  zs->count;
	return rval;
}

intptr_t
sem_trywait_zos(j9sem_t s)
{
	uintptr_t rval = -1;
	zos_sem_t *zs = (zos_sem_t *) s;

	omrthread_monitor_enter(zs->monitor);
	if (zs->count > 0) {
		-- zs->count;
		rval =  zs->count;
	}
	omrthread_monitor_exit(zs->monitor);

	return rval;
}


#endif

#if J9THREAD_USE_MONOTONIC_COND_CLOCK
/**
 * Called by omrthread library init to set the cond clock to CLOCK_MONOTONIC.
 * If the attempt fails, revert to CLOCK_REALTIME.
 * This requires AIX 5.3 or later.
 *
 * @return zero on success, non-zero on error or CLOCK_MONOTONIC not available
 */
static intptr_t
initCondAttr(void)
{
	intptr_t rc = 0;

	rc = pthread_condattr_init(&defaultCondAttr_s);
	if (0 == rc) {
		struct timespec tp;

		/* Confirm that CLOCK_MONOTONIC is supported. We don't care about the returned time. */
		if (0 == clock_gettime(CLOCK_MONOTONIC, &tp)) {
			rc = pthread_condattr_setclock(&defaultCondAttr_s, CLOCK_MONOTONIC);
			if (0 == rc) {
				defaultCondAttr = &defaultCondAttr_s;
				timeoutClock = CLOCK_MONOTONIC;
			}
		} else {
			rc = errno;
			if (0 == rc) {
				/* set rc to indicate an error, in case clock_gettime() failed to follow its spec */
				rc = -1;
			}
		}
	}
	return rc;
}
#endif /* J9THREAD_USE_MONOTONIC_COND_CLOCK */

#if  defined(OMRZTPF)
/**
 * process scoped settings for the z/TPF operating system.
 *
 */
void
ztpf_init_proc()
{
        /*
         * Disable heap check mode for the jvm process. See tpf rtc 15110.
         */
        tpf_eheap_heapcheck(TPF_EHEAP_HEAPCHECK_DISABLE);
        /*
         *  Set ECB attributes, ensure that these attributes are set in child ECBs too.
         */
        tpf_easetc(TPF_EASETC_SWITCHABLE, TPF_EASETC_SET_ON+TPF_EASETC_INHERIT_YES);
        tpf_easetc(TPF_EASETC_NOSTACKVAL, TPF_EASETC_SET_ON+TPF_EASETC_INHERIT_YES);
}
#endif /* defined(OMRZTPF) */

#if defined(J9ZOS390)
/**
 * We have to tell the z/OS LE that we want a specific kind of yielding threshold,
 * not the default kind.
 * But, if someone has said they explicitly want a specific kind, we won't
 * override their wishes.
 *
 * See VM Idea 1090 for more information
 */
static intptr_t
zos_init_yielding(void)
{
	/* Suspend conversion of strings to ascii, and ensure we use the OS's setenv and getenv functions */
#pragma convlit(suspend)
#undef setenv
#undef getenv
	if (getenv("_EDC_PTHREAD_YIELD") == NULL) {
		if (setenv("_EDC_PTHREAD_YIELD", "-2", 1) != 0) {
			return -1;
		}
	}
	return 0;
#pragma convlit(resume)
}
#endif

#if defined(OMR_THR_FORK_SUPPORT)

/**
 * @param[out] J9OSMutex* The mutex to init
 * @return 1 on success, 0 otherwise
 */
intptr_t
j9OSMutex_allocAndInit(J9OSMutex *mutex)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	intptr_t rc = 1;
	*mutex = (J9OSMutex)omrthread_allocate_memory(lib, sizeof(**mutex), OMRMEM_CATEGORY_OSMUTEXES);
	rc = (NULL != *mutex) && (pthread_mutex_init(*mutex, NULL) == 0);
	return rc;
}

/**
 * @param[in] J9OSMutex The mutex to free
 * @return 0 on success
 */
intptr_t
j9OSMutex_freeAndDestroy(J9OSMutex mutex)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	intptr_t rc = pthread_mutex_destroy(mutex);
	omrthread_free_memory(lib, mutex);
	return rc;
}

/**
 * @param[in] J9OSMutex The mutex to enter
 * @return 0 on success
 */
intptr_t
j9OSMutex_enter(J9OSMutex mutex)
{
	return pthread_mutex_lock(mutex);
}

/**
 * @param[in] J9OSMutex The mutex to try to enter
 * @return 0 on success
 */
intptr_t
j9OSMutex_tryEnter(J9OSMutex mutex)
{
	return pthread_mutex_trylock(mutex);
}

/**
 * @param[in] J9OSMutex The mutex to exit
 * @return 0 on success
 */
intptr_t
j9OSMutex_exit(J9OSMutex mutex)
{
	return pthread_mutex_unlock(mutex);
}

/**
 * @param[in] J9OSCond The cond to destroy
 * @return 0 on success
 */
intptr_t
j9OSCond_freeAndDestroy(J9OSCond cond)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	intptr_t rc = pthread_cond_destroy(cond);
	omrthread_free_memory(lib, cond);
	return rc;
}

/**
 * @param[in] J9OSCond The cond to notify
 * @return 0 on success
 */
intptr_t
j9OSCond_notifyAll(J9OSCond cond)
{
	return pthread_cond_broadcast(cond);
}

/**
 * @param[in] J9OSCond The cond to notify
 * @return 0 on success
 */
intptr_t
j9OSCond_notify(J9OSCond cond)
{
	return pthread_cond_signal(cond);
}

/**
 * @param[in] J9OSCond* The cond to init
 * @return 1 on success and 0 otherwise
 */
intptr_t
j9OSCond_allocAndInit(J9OSCond *cond)
{
	omrthread_library_t lib = GLOBAL_DATA(default_library);
	intptr_t rc = 1;
	*cond = (J9OSCond)omrthread_allocate_memory(lib, sizeof(**cond), OMRMEM_CATEGORY_OSCONDVARS);
#if J9THREAD_USE_MONOTONIC_COND_CLOCK
	rc = (NULL != *cond) && (pthread_cond_init(*cond, defaultCondAttr) == 0);
#else
	rc = (NULL != *cond) && (pthread_cond_init(*cond, NULL) == 0);
#endif
	return rc;
}

#endif /* defined(OMR_THR_FORK_SUPPORT) */
