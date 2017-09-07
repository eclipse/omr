/*******************************************************************************
 * Copyright (c) 2008, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "AtomicSupport.hpp"
#include "omrTest.h"
#include "threadExtendedTestHelpers.hpp"
#include "thread_api.h"

/**
 * Do work in a syscall consuming kernel CPU time. This is the core function to
 * test system / kernel time returned by get_process_times. Uses file handling
 * sys call to increase time taken in kernel mode Runtime is deterministic and
 * it does the same amount of work each time it is invoked.
 */
void
systemTimeCpuBurn(void)
{
	uintptr_t j = 0;
	FILE * tempFile = NULL;

	for (j = 0; j < 100; j++) {
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
 * Do work in the process consuming user CPU time. This is the core function to
 * test the user time returned by get_process_times. Its run time is
 * deterministic and it does the same amount of work each time it is invoked.
 */
void
userTimeCpuBurn()
{
	for (size_t i = 0; i < 40000; i++) {
		VM_AtomicSupport::nop();
	}
}

TEST(ThreadCpuTime, userCpuTimeIncreasesMonotonically)
{
	omrthread_process_time_t cpuTime;
	omrthread_process_time_t prevCpuTime;
	ASSERT_EQ(omrthread_get_process_times(&prevCpuTime), 0);

	for (size_t i = 0; i < 500; i += 1) {
		userTimeCpuBurn();
		ASSERT_EQ(omrthread_get_process_times(&cpuTime), 0);
		ASSERT_GE(cpuTime._userTime, prevCpuTime._userTime);
		prevCpuTime = cpuTime;
	}
}

TEST(ThreadCpuTime, systemCpuTimeIncreasesMonotonically)
{
	omrthread_process_time_t cpuTime;
	omrthread_process_time_t prevCpuTime;
	ASSERT_EQ(omrthread_get_process_times(&prevCpuTime), 0);

	for (size_t i = 0; i < 500; i += 1) {
		systemTimeCpuBurn();
		ASSERT_EQ(omrthread_get_process_times(&cpuTime), 0);
		ASSERT_GE(cpuTime._systemTime, prevCpuTime._systemTime);
		prevCpuTime = cpuTime;
	}
}

class CpuTimeTest : public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		// CPU usage tracking is disabled by default.
		// Enable it for the current thread.
		omrthread_lib_enable_cpu_monitor(omrthread_self());
	}
};

TEST_F(CpuTimeTest, increasesMonotonically)
{
	J9ThreadsCpuUsage cpuUsage;
	J9ThreadsCpuUsage prevCpuUsage;
	omrthread_get_jvm_cpu_usage_info(&prevCpuUsage);

	for (unsigned int i = 0; i < 500; i += 1) {
		userTimeCpuBurn();
		ASSERT_EQ(omrthread_get_jvm_cpu_usage_info(&cpuUsage), 0);
		ASSERT_GE(cpuUsage.systemJvmCpuTime, prevCpuUsage.systemJvmCpuTime);
		ASSERT_GE(cpuUsage.timestamp, prevCpuUsage.timestamp);
		ASSERT_EQ(cpuUsage.applicationCpuTime, prevCpuUsage.applicationCpuTime);
		prevCpuUsage = cpuUsage;
	}
}

class ApplicationCpuTimeTest : public CpuTimeTest
{
protected:
	virtual void SetUp()
	{
		omrthread_set_category(omrthread_self(),
		                       J9THREAD_CATEGORY_APPLICATION_THREAD,
		                       J9THREAD_TYPE_SET_MODIFY);
		CpuTimeTest::SetUp();
	}

	virtual void TearDown()
	{
		omrthread_set_category(omrthread_self(), J9THREAD_CATEGORY_SYSTEM_THREAD,
		                       J9THREAD_TYPE_SET_MODIFY);
	}
};

TEST_F(ApplicationCpuTimeTest, increasesMonotonically)
{
	J9ThreadsCpuUsage cpuUsage, prevCpuUsage;
	omrthread_get_jvm_cpu_usage_info(&prevCpuUsage);

	for (unsigned int i = 0; i < 500; i += 1) {
		userTimeCpuBurn();
		ASSERT_EQ(omrthread_get_jvm_cpu_usage_info(&cpuUsage), 0);
		ASSERT_GE(cpuUsage.applicationCpuTime, prevCpuUsage.applicationCpuTime);
		ASSERT_GE(cpuUsage.timestamp, prevCpuUsage.timestamp);
		ASSERT_EQ(cpuUsage.systemJvmCpuTime, prevCpuUsage.systemJvmCpuTime);
		prevCpuUsage = cpuUsage;
	}
}

#define NUM_ITERATIONS_JVM_CPU_USAGE 250

#define LONG_MILLI_TIMEOUT 900000
#define MILLI_TIMEOUT 10000
#define NANO_TIMEOUT 0

#define ONESEC 1000 /**< 1 sec in ms */

#define MATRIX_SIZE 500
#define NUM_ITERATIONS_MONOTONIC_TEST 500
#define NUM_ITERATIONS_SYSTEM_CPU_BURN 3000

typedef struct SupportThreadInfo
{
	uint32_t counter;
	uint32_t app;
	uint32_t sync;
	omrthread_monitor_t synchronization;
} SupportThreadInfo;

SupportThreadInfo * thrInfo;

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
sysThread(void * arg)
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
		omrthread_monitor_wait_interruptable(thrInfo->synchronization,
		                                     MILLI_TIMEOUT, NANO_TIMEOUT);
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
appThread(void * arg)
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
		omrthread_monitor_wait_interruptable(thrInfo->synchronization,
		                                     MILLI_TIMEOUT, NANO_TIMEOUT);
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

#define NUM_APP_THREAD 50
#define NUM_SYS_THREAD 10

/**
 * Compare the values of omrthread_get_cpu_time and omrthread_get_self_cpu_time
 * and the monotonicity of the omrthread_get_cpu_time values
 */
TEST(ThreadExtendedTest, DISABLED_TestThreadCpuTime)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	uint32_t i = 0;
	intptr_t ret = 0;
	omrthread_attr_t attr;
	omrthread_t tid[NUM_APP_THREAD + NUM_SYS_THREAD];
	J9ThreadsCpuUsage cpuUsageBef, cpuUsageAft;

	/* Enable CPU monitoring as it is disabled by default */
	omrthread_lib_enable_cpu_monitor(omrthread_self());

	/* Create the SupportThreadInfo structure for the required number of threads
	 * and intialize it */
	thrInfo = (SupportThreadInfo *)omrmem_allocate_memory(
	        sizeof(SupportThreadInfo), OMRMEM_CATEGORY_THREADS);
	ASSERT_TRUE(thrInfo != NULL);
	thrInfo->counter = 0;
	thrInfo->sync = 0;
	thrInfo->app = 0;
	omrthread_monitor_init_with_name(&thrInfo->synchronization, 0,
	                                 "threadCpuTimeInfo monitor");

	ret = omrthread_attr_init(&attr);
	ASSERT_TRUE(ret == 0);
	ret = omrthread_attr_set_category(&attr, J9THREAD_CATEGORY_APPLICATION_THREAD);
	ASSERT_TRUE(ret == 0);

	/* Create the required number of threads */
	omrthread_monitor_enter(thrInfo->synchronization);
	for (i = 0; i < NUM_APP_THREAD; i++) {
		ret = omrthread_create_ex(&(tid[i]), &attr, 0,
		                          (omrthread_entrypoint_t)appThread, (void *)&tid[i]);
		ASSERT_TRUE(ret == 0);
	}

	omrthread_attr_destroy(&attr);
	/* Now create the system threads */
	for (i = 0; i < NUM_SYS_THREAD; i++) {
		ret = omrthread_create_ex(&(tid[i]), J9THREAD_ATTR_DEFAULT, 0,
		                          (omrthread_entrypoint_t)sysThread, (void *)&tid[i]);
		ASSERT_TRUE(ret == 0);
	}

	/* Wait for the threads to complete work */
	do {
		omrthread_monitor_wait_interruptable(thrInfo->synchronization,
		                                     MILLI_TIMEOUT, NANO_TIMEOUT);
	} while (thrInfo->counter > 0);

	/* All threads have stopped, now get their cpu usage details */
	ret = omrthread_get_jvm_cpu_usage_info(&cpuUsageBef);

	/* Wake any remaining threads */
	thrInfo->sync = 1;
	omrthread_monitor_notify_all(thrInfo->synchronization);

	do {
		/* Ensure that all threads have exited before the master thread exits */
		omrthread_monitor_wait_interruptable(thrInfo->synchronization,
		                                     MILLI_TIMEOUT, NANO_TIMEOUT);
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
