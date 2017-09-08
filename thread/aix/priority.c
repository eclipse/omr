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

#include "thrdsup.h"
#include "unixpriority.h"
#include "omrutilbase.h"
#include "thread_internal.h" /* must be after thrdsup.h */

#include <sys/sched.h>

#include <stdio.h>
#ifdef J9OS_I5
#include "Xj9Interfaces.hpp"
#endif
#include <string.h>

int priority_map[J9THREAD_PRIORITY_MAX + 1] = J9_PRIORITY_MAP;

#if defined(J9OS_I5)
intptr_t
initialize_priority_map(void)
{
	char *priority_property;
	int i;
	const int default_priority[J9THREAD_PRIORITY_MAX + 1] = J9_PRIORITY_MAP;
	const int alt_priority[J9THREAD_PRIORITY_MAX + 1] = J9_PRIORITY_MAP_ALT;
#if (defined(J9OS_I5_V5R4)) /* V5R4 doesn't save alt.priority.mapping */
	priority_property = NULL;
#else
	priority_property = Xj9GetPropertyValue("alt.priority.mapping");
#endif /* J9OS_I5_V5R4 */
	if (priority_property) {
		for (i = 0; i <= J9THREAD_PRIORITY_MAX; i++) {
			priority_map[i] = alt_priority[i];
		}
	} else {
		for (i = 0; i <= J9THREAD_PRIORITY_MAX; i++) {
			priority_map[i] = default_priority[i];
		}
	}
	return 0;
}

#else

intptr_t
initialize_priority_map(void)
{
	/* this function should never be called on this spec */
	return -1;
}

#endif /* defined(J9OS_I5) */

intptr_t
set_priority_spread(void)
{
	/* this function should never be called on this spec */
	return -1;
}


intptr_t
set_pthread_priority(pthread_t handle, omrthread_prio_t j9ThreadPriority)
{
	struct sched_param sched_param;
	sched_param.sched_priority = omrthread_get_mapped_priority(j9ThreadPriority);
	return pthread_setschedparam(handle, omrthread_get_scheduling_policy(j9ThreadPriority), &sched_param);
}

void
initialize_thread_priority(omrthread_t thread)
{
	int policy, priority, i;
	struct sched_param sched_param;

	/* set the default value */
	thread->priority = J9THREAD_PRIORITY_NORMAL;

	/* are we using priorities at all? */
	if (priority_map[J9THREAD_PRIORITY_MIN] == priority_map[J9THREAD_PRIORITY_MAX]) {
		return;
	}
	if (pthread_getschedparam(thread->handle, &policy, &sched_param)) {
		/* the call failed */
		return;
	}

#if defined(J9_PRIORITY_MAP)
	if (policy != J9_DEFAULT_SCHED) {
		/* incompatible policy for our priority mapping */
		return;
	}
#endif

	/*
	* Similar story for AIX as IRIX, except AIX uses 1 instead of 0.
	*
	* Each thread in AIX starts out as a floating priority SCHED_OTHER thread.
	* Once we assign a pthread priority it becomes a fixed priority thread with
	* policy SCHED_OTHER, SCHED_FIFO, SCHED_RR or other.
	*/
	if (sched_param.sched_priority == 1) {
		set_pthread_priority(thread->handle, thread->priority);
		return;
	}


#ifndef J9OS_I5 /* explicitly disabled by iSeries team */
	/* on some platforms (i.e. Solaris) we get out of range values (e.g. 0) for threads with no explicitly set priority */
	if (sched_param.sched_priority < sched_get_priority_min(policy) || sched_param.sched_priority > sched_get_priority_max(policy)) {
		return;
	}
#endif

	thread->priority = omrthread_map_native_priority(sched_param.sched_priority);
}


