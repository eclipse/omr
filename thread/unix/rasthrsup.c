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

#include <pthread.h>
#include <unistd.h>

/* this #include defines the buildspec flags */
#include "omrthread.h"

#if defined(LINUX)
#if __GLIBC_PREREQ(2,4)
#include <sys/syscall.h>
#endif /* __GLIBC_PREREQ(2,4) */
#elif defined(OSX)
#include <sys/syscall.h>
#endif /* defined(LINUX) */

#if defined(LINUX)
/**
 * This is required to pick up correct thread IDs on Linux
 */

#if !__GLIBC_PREREQ(2,4)
/**
 * Even though we don't use errno directly, it is used by the _syscall0 macro and some
 * distros incorrectly assume that errno is an int, in their header.  Including it here will
 * force them to behave properly
 */
#include <errno.h>
#include <sys/types.h>
#include <linux/unistd.h>

/* this line is needed to build the syscall macro which is called (as gettid) within the function */
_syscall0(pid_t, gettid);
#endif
#endif /* defined(LINUX) */

uintptr_t
omrthread_get_ras_tid(void)
{
	uintptr_t threadID = 0;

#if defined(LINUX)
#if __GLIBC_PREREQ(2,4)
	/* Want thread id that shows up in /proc etc.  gettid() does not cut it */
	threadID = syscall(SYS_gettid);
#else /* __GLIBC_PREREQ(2,4) */
	/*
	 * On Linux (and probably other Unices but testing to follow), pthread_self is not the kernel's thread ID!
	 * We will use the gettid call to get the actual ID of the thread
	 */
	threadID = (uintptr_t) gettid();
#endif /* __GLIBC_PREREQ(2,4) */
#elif defined(OSX)
	threadID = syscall(SYS_thread_selfid);
#else /* defined(OSX) */
	pthread_t myThread = pthread_self();

	/*
	 * Convert the local pthread_t variable, which could be a structure or a scalar value, into a uintptr_t
	 * by getting its address, casting that to a uintptr_t pointer and then dereferencing to get the value
	 *
	 * The result seems to match the thread id observed in GDB...
	 */
	if (sizeof(pthread_t) >= sizeof(uintptr_t)) {
		threadID = *((uintptr_t *)&myThread);
	} else {
		threadID = 0;
	}
#endif /* defined(LINUX) */
	return threadID;
}

