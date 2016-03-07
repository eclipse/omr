/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2008, 2016
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


#include <math.h>

#include "omrTest.h"
#include "thread_api.h"
#include "threadExtendedTestHelpers.hpp"

#define MATRIX_SIZE 			500
#define NUM_ITERATIONS_MONOTONIC_TEST	500
#define NUM_ITERATIONS_SYSTEM_CPU_BURN	3000

typedef struct SupportThreadInfo {
	uint32_t counter;
	uint32_t app;
	uint32_t sync;
	omrthread_monitor_t synchronization;
} SupportThreadInfo;

SupportThreadInfo *thrInfo;

/**
 * This is the core function to test the user time returned by get_process_times.
 * Its run time is deterministic and it does the same amount of work each time it is invoked.
 */
void
matrixSquare(void)
{
	/*
	 * Declare and initialize the matrix. memset works here since this is a true 2D
	 * array defined in the same scope as the memset.
	 */
	uintptr_t matrix[MATRIX_SIZE][MATRIX_SIZE];
	uintptr_t i = 0;
	uintptr_t j = 0;

	memset(matrix, 0, sizeof(matrix));

	for (j = 0; j < MATRIX_SIZE; j++) {
		matrix[i][j] = (uintptr_t)pow((double)100, 2.0);
	}
}

/**
 * This is the core function to test system / kernel time returned by get_process_times.
 * Uses file handling sys call to increase time taken in kernel mode
 * Runtime is deterministic and it does the same amount of work each time it is invoked.
 */
void
systemTimeCPUBurn(void)
{
	uintptr_t j = 0;
	FILE *tempFile = NULL;

	for (j = 0; j < NUM_ITERATIONS_SYSTEM_CPU_BURN; j++) {
#if defined(WIN32) || defined(WIN64)
		tempFile = fopen("nul", "w");
#else
		tempFile = fopen("/dev/null", "w");
#endif
		fwrite("garbage", 1, sizeof("garbage"), tempFile);
		fclose(tempFile);
	}
}

/**
 * This function records process times after each call to the 'user time' CPU burner.
 * It also checks for valid return codes after each call.
 *
 * @param[in]  arrayLength		Length of the processTimeArray
 * @param[out] processTimeArray	Pointer to the array of process times
 */
void
runCPUBurnToTestUserTimes(omrthread_process_time_t *processTimeArray, uintptr_t arrayLength)
{
	uintptr_t i = 0;
	intptr_t retValOfGetProcessTimes = 0;

	/*
	 * If we increase the workload done (running the matrixSquare multiple times),
	 * CPU time recorded should increase after each run
	 */
	for (i = 0; i < arrayLength; i++) {
		matrixSquare();
		retValOfGetProcessTimes = (omrthread_get_process_times(processTimeArray++) || retValOfGetProcessTimes);
		EXPECT_TRUE(0 == retValOfGetProcessTimes);
	}
}
/**
 * This function records process times after each call to the 'system time' CPU burner.
 * It also checks for valid return codes after each call.
 *
 * @param[in]  arrayLength		Length of the processTimeArray
 * @param[out] processTimeArray	Pointer to the array of process times
 */
void
runCPUBurnToTestSystemTimes(omrthread_process_time_t *processTimeArray, uintptr_t arrayLength)
{
	uintptr_t i = 0;
	intptr_t retValOfGetProcessTimes = 0;

	/*
	 * If we increase the workload done (running the same function multiple times),
	 * CPU time recorded above should increase after each run
	 */
	for (i = 0; i < arrayLength; i++) {
		systemTimeCPUBurn();
		retValOfGetProcessTimes = (omrthread_get_process_times(processTimeArray++) || retValOfGetProcessTimes);
		EXPECT_TRUE(0 == retValOfGetProcessTimes);
	}
}

/**
 * Test for the monotonicity of get_process_times
 * w.r.t system time returned.
 */
TEST(ThreadExtendedTest, TestSystemTimesMonotonic)
{
	uintptr_t i = 0;
	uintptr_t isMonotonic = 1;

	omrthread_process_time_t processTimeArray[NUM_ITERATIONS_MONOTONIC_TEST];
	memset(&processTimeArray, 0, sizeof(processTimeArray));

	/*
	 * Runs the CPU burner which internally increasing workload and stores the
	 * process time at each increment at the next slot sin the array
	 */
	runCPUBurnToTestSystemTimes(processTimeArray, NUM_ITERATIONS_MONOTONIC_TEST);

	/* Test for monotonicity */
	for (i = 0; i < NUM_ITERATIONS_MONOTONIC_TEST - 1; i++) {
		if (processTimeArray[i]._systemTime > processTimeArray[i + 1]._systemTime) {
			isMonotonic = 0;
			break;
		}
	}
	ASSERT_TRUE(isMonotonic == 1);
}

/**
 * Test for the monotonicity of get_process_times
 * w.r.t user times returned.
 */
TEST(ThreadExtendedTest, TestUserTimesMonotonic)
{
	uintptr_t i = 0;
	uintptr_t isMonotonic = 1;

	omrthread_process_time_t processTimeArray[NUM_ITERATIONS_MONOTONIC_TEST];
	memset(&processTimeArray, 0, sizeof(processTimeArray));

	/*
	 * Runs the CPU burner which internally increases workload and stores the
	 * process time at each increment in the next processTimeArray slot
	 */
	runCPUBurnToTestUserTimes(processTimeArray, NUM_ITERATIONS_MONOTONIC_TEST);

	/* Test for monotonicity */
	for (i = 0; i < NUM_ITERATIONS_MONOTONIC_TEST - 1; i++) {
		if (processTimeArray[i]._userTime > processTimeArray[i + 1]._userTime) {
			isMonotonic = 0;
			break;
		}
	}
	ASSERT_TRUE(isMonotonic == 1);
}

#define NUM_ITERATIONS_JVM_CPU_USAGE  250

#define LONG_MILLI_TIMEOUT      900000
#define MILLI_TIMEOUT           10000
#define NANO_TIMEOUT            0

#define ONESEC					1000 /**< 1 sec in ms */

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

/**
 * This function records thread CPU times using both the omrthread_get_cpu_time
 * and the omrthread_get_self_cpu_time APIs.
 *
 * @param arg The index into the ThreadInfo array
 */
static int32_t J9THREAD_PROC
sysThread(void *arg)
{
	uintptr_t i = 0;
	uintptr_t arrayLength = NUM_ITERATIONS_JVM_CPU_USAGE;

	/* Increment the counters to indicate a new thread has started */
	omrthread_monitor_enter(thrInfo->synchronization);
	thrInfo->counter += 1;
	omrthread_monitor_exit(thrInfo->synchronization);

	/* Do work */
	for (i = 0; i < arrayLength; i++) {
		cpuLoad();
	}

	/* Inform master thread that we are done doing work */
	omrthread_monitor_enter(thrInfo->synchronization);
	thrInfo->counter -= 1;
	if (0 == thrInfo->counter) {
		omrthread_monitor_notify(thrInfo->synchronization);
	}

	/* Wait for the master thread to get cpu usage thrInfo for all threads */
	do {
		omrthread_monitor_wait_interruptable(thrInfo->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (0 == thrInfo->sync);

	omrthread_monitor_exit(thrInfo->synchronization);

	/* System threads exit after on round of work */
	return 0;
}

/**
 * This function records thread CPU times using both the omrthread_get_cpu_time
 * and the omrthread_get_self_cpu_time APIs.
 *
 * @param arg The index into the ThreadInfo array
 */
static int32_t J9THREAD_PROC
appThread(void *arg)
{
	uintptr_t i = 0;
	uintptr_t arrayLength = NUM_ITERATIONS_JVM_CPU_USAGE;

	/* Increment the counters to indicate a new thread has started */
	omrthread_monitor_enter(thrInfo->synchronization);
	thrInfo->counter += 1;
	thrInfo->app += 1;
	omrthread_monitor_exit(thrInfo->synchronization);

	/* Do work */
	for (i = 0; i < arrayLength; i++) {
		cpuLoad();
	}

	/* Inform master thread that we are done doing work */
	omrthread_monitor_enter(thrInfo->synchronization);
	thrInfo->counter -= 1;
	if (0 == thrInfo->counter) {
		omrthread_monitor_notify(thrInfo->synchronization);
	}

	/* Wait for the master thread to get cpu usage thrInfo for all threads */
	do {
		omrthread_monitor_wait_interruptable(thrInfo->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (0 == thrInfo->sync);

	omrthread_monitor_exit(thrInfo->synchronization);

	/* App threads do more work */
	for (i = 0; i < arrayLength; i++) {
		cpuLoad();
	}

	omrthread_monitor_enter(thrInfo->synchronization);
	thrInfo->app -= 1;
	/* Inform the master thread that we are now exiting */
	if (0 == thrInfo->app) {
		omrthread_monitor_notify(thrInfo->synchronization);
	}
	omrthread_monitor_exit(thrInfo->synchronization);

	return 0;
}

#define NUM_APP_THREAD	50
#define NUM_SYS_THREAD	10

/**
 * Compare the values of omrthread_get_cpu_time and omrthread_get_self_cpu_time
 * and the monotonicity of the omrthread_get_cpu_time values
 */
TEST(ThreadExtendedTest, TestThreadCpuTime)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	uint32_t i = 0;
	intptr_t ret = 0;
	omrthread_attr_t attr;
	omrthread_t tid[NUM_APP_THREAD + NUM_SYS_THREAD];
	J9ThreadsCpuUsage cpuUsageBef, cpuUsageAft;

	/* Enable CPU monitoring as it is disabled by default */
	omrthread_lib_enable_cpu_monitor(omrthread_self());

	/* Create the SupportThreadInfo structure for the required number of threads and intialize it */
	thrInfo = (SupportThreadInfo *)omrmem_allocate_memory(sizeof(SupportThreadInfo), OMRMEM_CATEGORY_THREADS);
	ASSERT_TRUE(thrInfo != NULL);
	thrInfo->counter = 0;
	thrInfo->sync = 0;
	thrInfo->app = 0;
	omrthread_monitor_init_with_name(&thrInfo->synchronization, 0, "threadCpuTimeInfo monitor");

	ret = omrthread_attr_init(&attr);
	ASSERT_TRUE(ret == 0);
	ret = omrthread_attr_set_category(&attr, J9THREAD_CATEGORY_APPLICATION_THREAD);
	ASSERT_TRUE(ret == 0);

	/* Create the required number of threads */
	omrthread_monitor_enter(thrInfo->synchronization);
	for (i = 0; i < NUM_APP_THREAD; i++) {
		ret = omrthread_create_ex(
				  &(tid[i]),
				  &attr,
				  0,
				  (omrthread_entrypoint_t) appThread,
				  (void *) &tid[i]);
		ASSERT_TRUE(ret == 0);
	}

	omrthread_attr_destroy(&attr);
	/* Now create the system threads */
	for (i = 0; i < NUM_SYS_THREAD; i++) {
		ret = omrthread_create_ex(
				  &(tid[i]),
				  J9THREAD_ATTR_DEFAULT,
				  0,
				  (omrthread_entrypoint_t) sysThread,
				  (void *) &tid[i]);
		ASSERT_TRUE(ret == 0);
	}

	/* Wait for the threads to complete work */
	do {
		omrthread_monitor_wait_interruptable(thrInfo->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (thrInfo->counter > 0);

	/* All threads have stopped, now get their cpu usage details */
	ret = omrthread_get_jvm_cpu_usage_info(&cpuUsageBef);

	/* Wake any remaining threads */
	thrInfo->sync = 1;
	omrthread_monitor_notify_all(thrInfo->synchronization);

	do {
		/* Ensure that all threads have exited before the master thread exits */
		omrthread_monitor_wait_interruptable(thrInfo->synchronization, MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (thrInfo->app > 0);

	/* All threads have completed, now get their cpu usage details */
	ret = omrthread_get_jvm_cpu_usage_info(&cpuUsageAft);
	ASSERT_TRUE(ret == 0);

	/* All threads have exited, we are done */
	omrthread_monitor_exit(thrInfo->synchronization);

	/* Timestamp has to increase */
	ASSERT_TRUE(cpuUsageAft.timestamp > cpuUsageBef.timestamp);
	/* Application threads' cpu usage should increase */
	ASSERT_TRUE(cpuUsageAft.applicationCpuTime > cpuUsageBef.applicationCpuTime);
	/*
	 * system cpu threads stopped after we checked the first time, but their time
	 * should either be the same or (slightly) higher
	 */
	ASSERT_TRUE(cpuUsageAft.systemJvmCpuTime >= cpuUsageBef.systemJvmCpuTime);
}

TEST(ThreadExtendedTest, TestJvmCpuUsageInfoMonotonic)
{
	uintptr_t i = 0;
	intptr_t retVal = 0;
	uintptr_t isMonotonic = 1;
	J9ThreadsCpuUsage cpuUsage[NUM_ITERATIONS_MONOTONIC_TEST];

	/* Enable CPU monitoring as it is disabled by default */
	omrthread_lib_enable_cpu_monitor(omrthread_self());

	memset(&cpuUsage, 0, sizeof(cpuUsage));

	for (i = 0; i < NUM_ITERATIONS_MONOTONIC_TEST; i++) {
		retVal = omrthread_get_jvm_cpu_usage_info(&cpuUsage[i]);
		matrixSquare();
		ASSERT_TRUE(0 == retVal);
	}

	for (i = 0; i < NUM_ITERATIONS_MONOTONIC_TEST - 1; i++) {
		if (cpuUsage[i].applicationCpuTime > cpuUsage[i + 1].applicationCpuTime) {
			isMonotonic = 0;
			break;
		}
	}
	ASSERT_TRUE(isMonotonic == 1);
}
