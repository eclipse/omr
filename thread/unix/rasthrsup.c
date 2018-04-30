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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <pthread.h>
#include <unistd.h>

/* this #include defines the buildspec flags */
#include "omrthread.h"

#if defined(LINUX) && defined(__GLIBC__)
#if __GLIBC_PREREQ(2,4)
#include <sys/syscall.h>
#endif /* __GLIBC_PREREQ(2,4) */
#elif defined(OSX)
#include <pthread.h>
#include <sys/syscall.h>
#endif /* defined(LINUX) && defined(__GLIBC__) */

#if defined(LINUX) && defined(__GLIBC__)
/**
 * This is required to pick up correct thread IDs on Linux
 */

#if !__GLIBC_PREREQ(2,4) && !defined(OMRZTPF)
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
#endif /* !__GLIBC_PREREQ(2,4) && !defined(OMRZTPF) */
#endif /* defined(LINUX) && defined(__GLIBC__) */

uintptr_t
omrthread_get_ras_tid(void)
{
	uintptr_t threadID = 0;

#if defined(LINUX) && !defined(OMRZTPF) && defined(__GLIBC__)
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
    uint64_t tid64;
    pthread_threadid_np(NULL, &tid64);
    threadID = (pid_t)tid64;
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
#endif /* defined(LINUX) && !defined(OMRZTPF) && defined(__GLIBC__) */
	return threadID;
}

