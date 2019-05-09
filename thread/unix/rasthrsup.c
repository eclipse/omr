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

#if defined(LINUX)
#include <sys/syscall.h>
#elif defined(OSX)
#include <pthread.h>
#include <sys/syscall.h>
#endif /* defined(LINUX) */

uintptr_t
omrthread_get_ras_tid(void)
{
	uintptr_t threadID = 0;

#if defined(LINUX) && !defined(OMRZTPF)
	threadID = (uintptr_t) syscall(SYS_gettid);
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
#endif /* defined(LINUX) && !defined(OMRZTPF) */
	return threadID;
}

