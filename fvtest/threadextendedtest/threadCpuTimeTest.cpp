/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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


#include "omrTest.h"
#include "thread_api.h"
#include "threadExtendedTestHelpers.hpp"

#define NUM_ITERATIONS	50

#define LONG_MILLI_TIMEOUT	900000
#define MILLI_TIMEOUT		10000
#define NANO_TIMEOUT		0

typedef struct ThreadInfo {
	uint32_t	tcount;
	int32_t		error_code;
	int64_t		cpu_time;
	int64_t		self_cpu_time;
	int64_t		other_cpu_time;
	omrthread_t	j9tid;
} ThreadInfo;

typedef struct SupportThreadInfo {
	uint32_t			counter;
	uint32_t			wait;
	uint32_t			sync;
	omrthread_monitor_t	synchronization;
	ThreadInfo		*thread_info;
} SupportThreadInfo;

SupportThreadInfo *info;

#define ONESEC	1000 /**< 1 sec in ms */

/**
 * Generate CPU Load for 1 second
 */
static void
cpuLoad(void)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	int64_t start = 0;
	int64_t end = 0;

	start = omrtime_current_time_millis();
	/* Generate CPU Load for 1 second */
	do {
		end = omrtime_current_time_millis();
	} while ((end - start) < ONESEC);
}

#define THREEMINS	180
#define NANOSEC		1000000000

#define CHECK_AND_SAVE_ERROR(val) \
	do { \
		if (-1 == val) { \
			tinfo->error_code = (int32_t) val; \
			return -1; \
		} \
	} while (0)

#define ERROR_CPU_TIME_STALLED	-2
#define ERROR_CPU_TIME_MISMATCH	-3

/**
 * This function records thread CPU times using both the omrthread_get_cpu_time
 * and the omrthread_get_self_cpu_time APIs.
 *
 * @param arg The index into the ThreadInfo array
 */
static int32_t J9THREAD_PROC
runTest(void *arg)
{
	uintptr_t i = 0;
	uintptr_t arrayLength = NUM_ITERATIONS;
	uint32_t *tcount = (uint32_t *)arg;
	int64_t start_time = 0;
	ThreadInfo *tinfo = &info->thread_info[*tcount];

	/* Increment the counters to indicate a new thread has started */
	omrthread_monitor_enter(info->synchronization);
	info->counter += 1;
	info->wait += 1;
	omrthread_monitor_exit(info->synchronization);

	tinfo->error_code = 0;
	/* Save the start time */
	start_time = omrthread_get_cpu_time(tinfo->j9tid);
	/* CuAssert doesn't seem to work inside a thread but works in the main thread! */
	CHECK_AND_SAVE_ERROR(start_time);

	/* Do work, earlier threads do less work than later threads */
	for (i = 0; i < (arrayLength + info->counter); i++) {
		cpuLoad();
	}

	/* Capture CPU time info using both APIs */
	tinfo->cpu_time = omrthread_get_cpu_time(tinfo->j9tid);
	CHECK_AND_SAVE_ERROR(tinfo->cpu_time);
	tinfo->self_cpu_time = omrthread_get_self_cpu_time(tinfo->j9tid);
	CHECK_AND_SAVE_ERROR(tinfo->self_cpu_time);

	/* Check that the value returned by omrthread_get_cpu_time has increased after work done */
	if (tinfo->cpu_time - start_time <= 0) {
		tinfo->error_code = ERROR_CPU_TIME_STALLED;
		return -1;
	}

	/*
	 * Check that the CPU time data from the two APIs are close.
	 * The check was modified as it is possible for these threads to get preempted
	 * between the two APIs, esp because of the presence of a 100 threads all CPU bound.
	 * Since the entire test runs for about 150s, we are checking the times are within
	 * 3 mins of each other
	 */
	if (tinfo->self_cpu_time > tinfo->cpu_time) {
		if ((tinfo->self_cpu_time - tinfo->cpu_time) > (int64_t)THREEMINS * NANOSEC) {
			tinfo->error_code = ERROR_CPU_TIME_MISMATCH;
			return -1;
		}
	} else {
		if ((tinfo->cpu_time - tinfo->self_cpu_time) > (int64_t)THREEMINS * NANOSEC) {
			tinfo->error_code = ERROR_CPU_TIME_MISMATCH;
			return -1;
		}
	}

	/* Inform master thread that we are done doing work */
	omrthread_monitor_enter(info->synchronization);
	info->counter -= 1;
	if (0 == info->counter) {
		omrthread_monitor_notify(info->synchronization);
	}

	/* Wait for the master thread to get cpu usage info for all threads */
	do {
		omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (0 == info->sync);

	info->wait -= 1;
	/* Inform the master thread that we are now exiting */
	if (0 == info->wait) {
		omrthread_monitor_notify(info->synchronization);
	}
	omrthread_monitor_exit(info->synchronization);

	return 0;
}

#define THREAD_NUM 100

/**
 * Compare the values of omrthread_get_cpu_time and omrthread_get_self_cpu_time
 * and the monotonicity of the omrthread_get_cpu_time values
 */
TEST(ThreadExtendedTest, TestOtherThreadCputime)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	uint32_t i = 0;
	intptr_t ret = 0;

	/* Create the SupportThreadInfo structure for the required number of threads and intialize it */
	info = (SupportThreadInfo *)omrmem_allocate_memory(sizeof(SupportThreadInfo), OMRMEM_CATEGORY_THREADS);
	ASSERT_TRUE(NULL != info);
	info->counter = 0;
	info->wait = 0;
	info->sync = 0;
	info->thread_info = (ThreadInfo *)omrmem_allocate_memory(sizeof(ThreadInfo) * THREAD_NUM, OMRMEM_CATEGORY_THREADS);
	ASSERT_TRUE(info->thread_info != NULL);
	omrthread_monitor_init_with_name(&info->synchronization, 0, "threadCpuTimeInfo monitor");

	/* Create the required number of threads */
	omrthread_monitor_enter(info->synchronization);
	for (i = 0; i < THREAD_NUM; i++) {
		ThreadInfo *tinfo = &info->thread_info[i];
		tinfo->tcount = i;
		ret = omrthread_create_ex(
				  &(tinfo->j9tid),
				  J9THREAD_ATTR_DEFAULT,
				  0,
				  (omrthread_entrypoint_t) runTest,
				  (void *) &tinfo->tcount);
		ASSERT_TRUE(ret == 0);
	}

	/* Wait for the threads to complete work */
	do {
		omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (info->counter > 0);

	/* All threads have stopped, now get their cpu usage details */
	for (i = 0; i < THREAD_NUM; i++) {
		ThreadInfo *tinfo = &info->thread_info[i];
		tinfo->other_cpu_time = omrthread_get_cpu_time(tinfo->j9tid);
		ASSERT_TRUE(tinfo->other_cpu_time != -1);

		/* Check if the thread reported any error */
		ASSERT_EQ(0, tinfo->error_code);

		/*
		 * Check that the cp time that we got above is similar to the stored cpu time
		 * by the thread itself
		 */
		ASSERT_TRUE((tinfo->other_cpu_time - tinfo->cpu_time) >= 0);
	}

	/* Wake all threads, so that they can exit */
	info->sync = 1;
	omrthread_monitor_notify_all(info->synchronization);

	do {
		/* Ensure that all threads have exited before the master thread exits */
		omrthread_monitor_wait_interruptable(info->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (info->wait > 0);

	/* All threads have exited, we are done */
	omrthread_monitor_exit(info->synchronization);
}
