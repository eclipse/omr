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

#include "omrutilbase.h"
#include "unixpriority.h"
#include "thrdsup.h"
#include "thread_internal.h" /* Must be after thrdsup.h */

#include <sched.h>	/* Must be after <pthread.h> */
#include <stdio.h>
#include <string.h>

int priority_map[J9THREAD_PRIORITY_MAX + 1] = {0};
static intptr_t initialize_realtime_priority_map(void);

static void initialize_thread_policies(int *policy_regular_thread, int *policy_realtime_thread);
static void initializePolicy(int range_start, int range_end, int policy, int map[]);
static void initializeRange(int start_index, int end_index, int range_min, int range_max, int values[]);
static int getCurrentThreadParams(int *prio, int *policy);

/**
 * Initializes the priority_map with the correct values.
 *
 * @returns 0 upon success, -1 otherwise
 */
intptr_t
initialize_priority_map(void)
{
	intptr_t rc = 0;
	if (omrthread_lib_use_realtime_scheduling()) {
		rc = initialize_realtime_priority_map();
	} else {
		uintptr_t i = 0;

		/* Minimum, maximum, and default priorities are available from host_info().
		 * The default is 31. pthread attributes must use this default to have the same
		 * priority as the main thread.
		 */
		host_priority_info_data_t priorityInfoData;
		mach_msg_type_number_t msgTypeNumber = HOST_PRIORITY_INFO_COUNT;
		host_flavor_t flavor = HOST_PRIORITY_INFO;

		if (KERN_SUCCESS == host_info(mach_host_self(), flavor, (host_info_t)&priorityInfoData, &msgTypeNumber)) {
			for (i = 0; i <= J9THREAD_PRIORITY_MAX; i+=1) {
				priority_map[i] = priorityInfoData.user_priority;
			}
		} else {
			rc = -1;
		}
	}
	return rc;
}

static int policy_regular_thread;
static int policy_realtime_thread;

/* Represents the O/S limits for realtime priorities */
static int minRealtimePrio;
static int maxRealtimePrio;

/* Represents the O/S limits for regular priorities */
static int minRegularPrio;
static int maxRegularPrio;

/* Represents both the permissible and actual range of realtime priorities */
static int lowerBoundRealtime;
static int higherBoundRealtime;

/* Represents the permissible range of regular priorities */
static int lowerBoundRegular;
static int higherBoundRegular;

/* Represents the actual range of regular priorities - we may not use up the entire permissible range */
static int lowerBoundRegularMapped;
static int higherBoundRegularMapped;

/*
Here are the constraints:

All of these values are calculated for both hard and soft.

Regular ordering:
minRegularPrio <= lowerBoundRegular <= lowerBoundRegularMapped <= higherBoundRegularMapped <= higherBoundRegular <= maxRegularPrio

Real-time ordering:
minRealtimePrio <= lowerBoundRealtime <= higherBoundRealtime <= maxRealtimePrio

The permissible regular prios lie below the permissible real-time prios
higherBoundRegular <= lowerBoundRealtime

*/

#if defined(DEBUG)
void
printMap(void)
{
	int k;
	for (k = 0; k <= J9THREAD_PRIORITY_MAX; k+=1) {
		int pol = omrthread_get_scheduling_policy(k);
		printf("j9 thread prio mapping: %d -> %d %s\n", k,
			   omrthread_get_mapped_priority(k), (pol == SCHED_OTHER) ? "SCHED_OTHER" : ((pol == SCHED_RR) ? "SCHED_RR" : "SCHED_FIFO"));
	}

	fprintf(stderr, "min rt %d, max rt %d\n", minRealtimePrio, maxRealtimePrio);
	fprintf(stderr, "min reg %d, max reg %d\n", minRegularPrio, maxRegularPrio);
	fprintf(stderr, "min bound rt %d, max bound rt %d\n", lowerBoundRealtime, higherBoundRealtime);
	fprintf(stderr, "min  bound reg %d, max bound reg %d\n", lowerBoundRegular, higherBoundRegular);
	fprintf(stderr, "min  bound reg mapped %d, max bound reg mapped %d\n", lowerBoundRegularMapped, higherBoundRegularMapped);
}
#endif /* defined(DEBUG) */


static intptr_t
initialize_realtime_priority_map(void)
{
	int defaultPrio = 0;
	int currentPrio = 0;
	int lowestPrio = 0;
	int highestPrio = 0;
	int range = 0;
	int extra = 0;
	int overlap = 0;

	initialize_thread_policies(&policy_regular_thread, &policy_realtime_thread);

	/* Base the regular priorities on the current thread's
	 * priority and the policy will match the current thread's policy.
	 */
	if (0 != getCurrentThreadParams(&currentPrio, &policy_regular_thread)) {
		return -1;
	}
	defaultPrio = currentPrio;

	minRegularPrio = sched_get_priority_min(policy_regular_thread);
	maxRegularPrio = sched_get_priority_max(policy_regular_thread);
	minRealtimePrio = sched_get_priority_min(policy_realtime_thread);
	maxRealtimePrio = sched_get_priority_max(policy_realtime_thread);

	lowerBoundRealtime = minRealtimePrio;
	higherBoundRealtime = maxRealtimePrio;
	lowerBoundRegular = minRegularPrio;
	higherBoundRegular = maxRegularPrio;
	range = higherBoundRealtime - lowerBoundRealtime;

	if (0 > range) {
		/* This line should be unreachable: the POSIX.4 standard chose larger numbers to represent stronger priorities */
		return -1;
	}

	/* We need 79 realtime slots */
	extra = range - 78;

	/* Shrink the real-time range as much as we can to fit into the 11-89 range if possible */
	if (0 < extra) {
		/* Shrink at the top end */
		int shrink = higherBoundRealtime - 89;
		if (0 < shrink) {
			if (shrink > extra) {
				/* Ensure we maintain the 79 slots */
				shrink = extra;
			}
			higherBoundRealtime -= shrink;
			extra -= shrink;
		}

		if (0 < extra) {
			/* Shrink at the bottom end */
			shrink = 11 - lowerBoundRealtime;
			if (0 < shrink) {
				if (shrink > extra) {
					/* Ensure we maintain the 79 slots */
					shrink = extra;
				}
				lowerBoundRealtime += shrink;
				extra -= shrink;
			}
		}
	}

	/* Disconnect the regular and realtime ranges */
	overlap = 1 + higherBoundRegular - lowerBoundRealtime;
	if (0 < overlap) {
		int shrink = higherBoundRegular - lowerBoundRegular;
		if (shrink > overlap) {
			shrink = overlap;
		}
		higherBoundRegular -= shrink;
		overlap -= shrink;

		/* Ensure the default prio still falls in the new range */
		if (defaultPrio > higherBoundRegular) {
			defaultPrio = higherBoundRegular;
		}

		if (0 < overlap) {
			int shrink = higherBoundRealtime - lowerBoundRealtime;
			if (shrink > overlap) {
				shrink = overlap;
			}
			lowerBoundRealtime += shrink;
			extra -= shrink;

			if (0 > extra) {
				/* Re-adjust the real-time range to maintain the 79 slots.
				 * This is only necessary if we increased the lower real-time boundary
				 * because it was below the lower regular boundary, which is unlikely.
				 */
				int gap = maxRealtimePrio - higherBoundRealtime;
				if (0 < gap) {
					int unshrink = -extra;
					if (unshrink > gap) {
						unshrink = gap;
					}
					higherBoundRealtime += unshrink;
				}
			}
		}
	}

	/* Map all regular threads to the same priority. */
	lowerBoundRegularMapped = higherBoundRegularMapped = defaultPrio;

	initializeRange(J9THREAD_PRIORITY_USER_MIN, J9THREAD_PRIORITY_USER_MAX, lowerBoundRegularMapped, higherBoundRegularMapped, priority_map);
	initializePolicy(J9THREAD_PRIORITY_USER_MIN, J9THREAD_PRIORITY_USER_MAX, policy_regular_thread, priority_map);


	/* assign the lowest priority, which is outside the range of java thread priorities */

	lowestPrio = lowerBoundRegularMapped;
	initializeRange(J9THREAD_PRIORITY_MIN, J9THREAD_PRIORITY_MIN, lowestPrio, lowestPrio, priority_map);
	initializePolicy(J9THREAD_PRIORITY_MIN, J9THREAD_PRIORITY_MIN, policy_regular_thread, priority_map);

	/* now we define priorities 11 and above which differs for hard and soft real-time */

	if (SCHED_FIFO == policy_regular_thread) {
		fprintf(stderr, "JVM cannot be invoked by a thread with scheduling policy SCHED_FIFO.\n");
		return -1;
	}

	/* the highest priority can lie outside the range of java thread priorities */
	highestPrio = higherBoundRegularMapped;
	initializeRange(J9THREAD_PRIORITY_MAX, J9THREAD_PRIORITY_MAX, highestPrio, highestPrio, priority_map);
	initializePolicy(J9THREAD_PRIORITY_MAX, J9THREAD_PRIORITY_MAX, policy_regular_thread, priority_map);


	/*
	 * Note that it is not just when using fixed policies and prios that the current thread needs adjusting.
	 * It is also when we are basing the map on the current thread's prio, but we
	 * have adjusted the current thread's prio because it must fall within the allowable range.
	 */
	if (currentPrio != defaultPrio) {
		/* Force priority of initial thread to be consistent with priority_map */
		set_pthread_priority(pthread_self(), J9THREAD_PRIORITY_NORMAL);
	}

#if defined(DEBUG)
	printMap();
#endif /* defined(DEBUG) */
	return 0;
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
	int policy;
	struct sched_param sched_param;

	/* Set the default value */
	thread->priority = J9THREAD_PRIORITY_NORMAL;

	/* Are priorities used at all? */
	if (priority_map[J9THREAD_PRIORITY_MIN] == priority_map[J9THREAD_PRIORITY_MAX]) {
		return;
	}
	if (0 != pthread_getschedparam(thread->handle, &policy, &sched_param)) {
		/* The call failed */
		return;
	}

	if (!omrthread_lib_use_realtime_scheduling()) {
		if (J9_DEFAULT_SCHED != policy) {
			/* Incompatible policy for our priority mapping */
			return;
		}
	}

	thread->priority = omrthread_map_native_priority(sched_param.sched_priority);
}

/**
 * Initialize the thread policies.
 */
static void
initialize_thread_policies(int *policy_regular_thread, int *policy_realtime_thread)
{
	*policy_regular_thread = SCHED_OTHER;
	*policy_realtime_thread = SCHED_FIFO;
}

static int
getCurrentThreadParams(int *prio, int *policy)
{
	struct sched_param schedParam;

	if (0 == pthread_getschedparam(pthread_self(), policy, &schedParam)) {
		*prio = schedParam.sched_priority;
		return 0;
	}
	return -1;
}

static void
initializePolicy(int range_start, int range_end, int policy, int map[])
{
	int i;

	for (i = range_start; i <= range_end; i+=1) {
		map[i] = omrthread_get_mapped_priority(i) + PRIORITY_MAP_ADJUSTED_POLICY(policy);
	}
}

static void
initializeRange(int start_index, int end_index, int range_min, int range_max, int values[])
{
	int interval = end_index - start_index;

	if (0 == interval) {
		values[start_index] = (range_min + range_max) / 2;
	} else {
		values[end_index] = range_max;
		values[start_index] = range_min;
		if (1 < interval) {
			int midpoint = interval / 2;

			values[start_index + midpoint] = (range_min + range_max) / 2;

			if (2 < interval) {
				int delta;
				int i;
				int tailcount;

				/* give us some room to do some math */
				int tmpmax = range_max * 1024;
				int tmpmin = range_min * 1024;
				int mid = (tmpmin + tmpmax) / 2;

				delta = (mid - tmpmin) / midpoint;
				for (i = 1; i < midpoint; i+=1) {
					values[start_index + midpoint - i] = (mid - delta * i) / 1024;
				}

				tailcount = interval - midpoint;
				delta = (tmpmax - mid) / tailcount;
				for (i = 1; i < tailcount; i+=1) {
					values[start_index + midpoint + i] = (mid + delta * i) / 1024;
				}
			}
		}
	}
}

intptr_t
set_priority_spread(void)
{
	int i, j;
	int lowestPrio;
	int highestPrio;
	int prio;
	int spread;
	int prioSpread;
	int dups;

	prio = omrthread_get_mapped_priority(J9THREAD_PRIORITY_NORMAL);
	spread = prio - lowerBoundRegular;
	prioSpread = J9THREAD_PRIORITY_NORMAL - J9THREAD_PRIORITY_USER_MIN;
	dups = prioSpread - spread;

	if (dups < 0) {
		dups = 0;
	}
	for (i = J9THREAD_PRIORITY_NORMAL - dups - 1, j = prio; i >= J9THREAD_PRIORITY_USER_MIN; i--, j) {
		priority_map[i] = --j + PRIORITY_MAP_ADJUSTED_POLICY(policy_regular_thread);
	}
	lowerBoundRegularMapped = j;

	spread = higherBoundRegular - prio;
	prioSpread = J9THREAD_PRIORITY_USER_MAX - J9THREAD_PRIORITY_NORMAL;
	dups = prioSpread - spread;
	if (dups < 0) {
		dups = 0;
	}
	for (i = J9THREAD_PRIORITY_NORMAL + dups + 1, j = prio; i <= J9THREAD_PRIORITY_USER_MAX; i++) {
		priority_map[i] = ++j + PRIORITY_MAP_ADJUSTED_POLICY(policy_regular_thread);
	}
	higherBoundRegularMapped = j;

	/* assign the lowest priority, which is outside the range of java thread priorities */
	lowestPrio = lowerBoundRegularMapped;
	if (lowerBoundRegularMapped > minRegularPrio) {
		lowestPrio--;
	}
	priority_map[J9THREAD_PRIORITY_MIN] = lowestPrio + PRIORITY_MAP_ADJUSTED_POLICY(policy_regular_thread);

	/* assign the highest priority, also outside the range of java thread priorities */

	highestPrio = higherBoundRegularMapped;
	if (higherBoundRegularMapped < maxRegularPrio) {
		highestPrio++;
	}
	priority_map[J9THREAD_PRIORITY_MAX] = highestPrio + PRIORITY_MAP_ADJUSTED_POLICY(policy_regular_thread);
#if defined(DEBUG)
	printMap();
#endif
	return 0;
}
