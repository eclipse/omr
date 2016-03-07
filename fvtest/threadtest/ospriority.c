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

#if defined(WIN32)
#include <windows.h>
#endif
#if defined(J9ZOS390) || defined(LINUX) || defined(AIXPPC) || defined(OSX)
#include <pthread.h>
#if defined(OSX)
#include <mach/mach_host.h>
#endif /* defined(OSX) */
#endif /* defined(J9ZOS390) || defined(LINUX) || defined(AIXPPC) || defined(OSX) */
#include "createTestHelper.h"
#include "omrutil.h"
#include "omrutilbase.h"

/*
 * VM priorities are [0, 11]
 * OS priorities are platform-specific
 */
#define VM_PRIO_MAX 11

static osprio_t VMToOSPrioMap[VM_PRIO_MAX + 1];

static void printMap(void);
static void initialize_thread_policies(int *policy_regular_thread, int *policy_realtime_thread);
static int getCurrentThreadParams(int *priority, int *policy);

extern osprio_t
getOsPriority(omrthread_prio_t priority)
{
	osprio_t ospriority;

	ospriority = VMToOSPrioMap[priority];
	if (omrthread_lib_use_realtime_scheduling()) {
		ospriority &= 0x00ffffff;
	}

	return ospriority;
}

char *
mapOSPolicy(intptr_t policy)
{
	switch (policy) {
	case OS_SCHED_OTHER:
		return "SCHED_OTHER";
	case OS_SCHED_FIFO:
		return "SCHED_FIFO";
	case OS_SCHED_RR:
		return "SCHED_RR";
	default:
		return "unknown scheduling policy";
	}
}

#if defined(LINUX) || defined(OSX)

void
initPrioMap(void)
{
	uintptr_t i;
	osprio_t defaultPriority = 0;

#if defined(OSX)
	host_priority_info_data_t priorityInfoData;
	mach_msg_type_number_t msgTypeNumber = HOST_PRIORITY_INFO_COUNT;
	host_flavor_t flavor = HOST_PRIORITY_INFO;
	host_info(mach_host_self(), flavor, (host_info_t)&priorityInfoData, &msgTypeNumber);
	defaultPriority = priorityInfoData.user_priority;
#endif /* defined(OSX) */

	for (i = 0; i <= VM_PRIO_MAX; i++) {
		VMToOSPrioMap[i] = defaultPriority;
	}
}

int
getRTPolicy(omrthread_prio_t priority)
{
	return VMToOSPrioMap[priority] >> 24;
}

static void
printMap(void)
{
	int k;
	for (k = 0; k <= VM_PRIO_MAX; k++) {
		int pol = VMToOSPrioMap[k] >> 24;
		PRINTF("j9 thread prio mapping: %d -> %d %s\n", k,
			   VMToOSPrioMap[k] & 0x00ffffff, mapOSPolicy(pol));
	}
}

static void
initialize_thread_policies(int *policy_regular_thread, int *policy_realtime_thread)
{
	*policy_regular_thread = SCHED_OTHER;
	*policy_realtime_thread = SCHED_FIFO;
}

static int
getCurrentThreadParams(int *priority, int *policy)
{
	struct sched_param schedParam;

	if (pthread_getschedparam(pthread_self(), policy, &schedParam) == 0) {
		*priority = schedParam.sched_priority;
		return 0;
	}
	return -1;
}

/*
 * The real soft and hard real-time maps can be dynamically adjusted by command line options.
 * For testing purposes, hardcode the default map here.
 */
void
initRealtimePrioMap(void)
{
	intptr_t i;
	int prio = 0;
	int policy_regular_thread;
	int policy_realtime_thread;
	int spreading;
//todo	int reg_min, reg_max;
	int dups;
	int assignedPrio;

	/* should be 1 when -Xthr:spreadPrios appears as a JVM argument */
	int XthrSpreadPriosSet = 0;

	/* should be the shift value when -Xthr:realtimePriorityShift appears as a JVM argument */
	int XthrRealtimeShiftPriosValue = 0;

	initialize_thread_policies(&policy_regular_thread, &policy_realtime_thread);
	{
		/* the policies are not explicitly defined,
		 * so the regular priorities and policies will be based on the current thread's priority and policy
		 */
		if (getCurrentThreadParams(&prio, &policy_regular_thread)) {
			/* failed to get the current params */
			PRINTF("Could not get current thread params, assuming default\n");
			prio = 0;
			policy_regular_thread = SCHED_FIFO;
		} else {
			if (prio > 10) {
				prio = 10;
			}
		}
	}

	/* priorities */

	/* regular priorities first */

	spreading = (policy_regular_thread != SCHED_OTHER) && XthrSpreadPriosSet;

	/* how much spreading out can be done downwards? */
	dups = 5 - prio;
	if (dups < 0) {
		dups = 0;
	}
	assignedPrio = prio;
	for (i = 4; i >= 1; i--) {
		if (spreading && --dups < 0) {
			/* spread them out if we have the option set */
			assignedPrio--;
		}
		VMToOSPrioMap[i] = assignedPrio;
	}

	if (spreading) {
		if (assignedPrio > 1) {
			assignedPrio--;
		}
	}
	VMToOSPrioMap[0] = assignedPrio;

	/* how much spreading out can be done upwards? */
	dups = prio - 5;
	if (dups < 0) {
		dups = 0;
	}

	VMToOSPrioMap[5] = prio;
	assignedPrio = prio;
	for (i = 6; i <= 10; i++) {
		if (spreading && --dups < 0) {
			/* spread them out if we have the option set */
			assignedPrio++;
		}
		VMToOSPrioMap[i] = assignedPrio;
	}

	/* real-time priorities second */

	/* one higher than priority 10, unless we cannot go higher */
	VMToOSPrioMap[11] = VMToOSPrioMap[10] + (spreading ? 1 : 0);

	for (i = 12; i <= VM_PRIO_MAX; i++) {
		VMToOSPrioMap[i] = ((policy_realtime_thread == SCHED_OTHER) ? 0 : (i + XthrRealtimeShiftPriosValue));
	}

	/* policies */

	for (i = 0; i < 11; i++) {
		VMToOSPrioMap[i] |= (policy_regular_thread << 24);
	}

	VMToOSPrioMap[11] |= (policy_regular_thread << 24);

	for (i = 12; i <= VM_PRIO_MAX; i++) {
		VMToOSPrioMap[i] |= (policy_realtime_thread << 24);
	}

	printMap();
}

#elif defined(AIXPPC)

#if defined(J9OS_I5)

void
initPrioMap(void)
{
	const osprio_t tmpMap[VM_PRIO_MAX + 1] = {
		54, 55, 55, 56, 56, 57, 57, 58, 58, 59, 59, 60
	};

	memcpy(VMToOSPrioMap, tmpMap, sizeof(osprio_t) * (VM_PRIO_MAX + 1));
}

#else

void
initPrioMap(void)
{
	const osprio_t tmpMap[VM_PRIO_MAX + 1] = { 40, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 60 };

	memcpy(VMToOSPrioMap, tmpMap, sizeof(osprio_t) * (VM_PRIO_MAX + 1));
}
#endif /* defined(J9OS_I5) */

#elif defined(WIN32)

void
initPrioMap(void)
{
	const osprio_t tmpMap[VM_PRIO_MAX + 1] = {
		THREAD_PRIORITY_IDLE, /* 0 */
		THREAD_PRIORITY_LOWEST, /* 1 */
		THREAD_PRIORITY_BELOW_NORMAL, /* 2 */
		THREAD_PRIORITY_BELOW_NORMAL, /* 3 */
		THREAD_PRIORITY_BELOW_NORMAL, /* 4 */
		THREAD_PRIORITY_NORMAL, /* 5 */
		THREAD_PRIORITY_ABOVE_NORMAL, /* 6 */
		THREAD_PRIORITY_ABOVE_NORMAL, /* 7 */
		THREAD_PRIORITY_ABOVE_NORMAL, /* 8 */
		THREAD_PRIORITY_ABOVE_NORMAL, /* 9 */
		THREAD_PRIORITY_HIGHEST, /* 10 */
		THREAD_PRIORITY_TIME_CRITICAL /* 11 */
	};
	memcpy(VMToOSPrioMap, tmpMap, sizeof(osprio_t) * (VM_PRIO_MAX + 1));
}

#elif defined(J9ZOS390)

void
initPrioMap(void)
{
	uintptr_t i;
	for (i = 0; i <= VM_PRIO_MAX; i++) {
		VMToOSPrioMap[i] = -1;
	}
}

#else
#error "unsupported platform"
#endif /* defined(LINUX) || defined(OSX) */
