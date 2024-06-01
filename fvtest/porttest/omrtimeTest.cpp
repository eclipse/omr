/*******************************************************************************
 * Copyright IBM Corp. and others 1991
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/*
 * $RCSfile: omrtimeTest.c,v $
 * $Revision: 1.55 $
 * $Date: 2012-11-23 21:11:32 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library timer operations.
 *
 * Exercise the API for port library timer operations.  These functions
 * can be found in the file @ref omrtime.c
 *
 * @note port library string operations are not optional in the port library table.
 *
 */
#include <stdlib.h>
#include <string.h>
#if defined(OMR_OS_WINDOWS)
#include <windows.h>
#endif /* defined(OMR_OS_WINDOWS) */

#include "testHelpers.hpp"
#include "omrport.h"


static int J9THREAD_PROC nanoTimeDirectionTest(void *portLibrary);

/**
 * @internal
 * @def
 * The interval to run the time tests
 * @note Must be at least 100
 * @note Assume 1~=10ms
 */
#define TEST_DURATION 1000

/**
 * @internal
 * @def
 * Number of time intervals to test
 * @note (1ms, 2ms, 3ms, ... J9TIME_REPEAT_TESTms)
 */
#define J9TIME_REPEAT_TEST 5

/**
 * Verify port library timer operations.
 *
 * Ensure the library table is properly setup to run timer tests.
 */
TEST(PortTimeTest, time_test0)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtime_test0";

	reportTestEntry(OMRPORTLIB, testName);

	/* Verify that the time function pointers are non NULL */

	/* Not tested, implementation dependent.  No known functionality.
	 * Startup is private to the portlibary, it is not re-entrant safe
	 */
	if (NULL == OMRPORTLIB->time_startup) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_startup is NULL\n");
	}

	/* Not tested, implementation dependent.  No known functionality */
	if (NULL == OMRPORTLIB->time_shutdown) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_shutdown is NULL\n");
	}

	/* omrtime_test1, omrtime_test3 */
	if (NULL == OMRPORTLIB->time_msec_clock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_msec_clock is NULL\n");
	}

	/* omrtime_test1, omrtime_test3 */
	if (NULL == OMRPORTLIB->time_usec_clock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_usec_clock is NULL\n");
	}

	/* omrtime_test1, omrtime_test3 */
	if (NULL == OMRPORTLIB->time_current_time_nanos) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_current_time_nanos is NULL\n");
	}

	/* omrtime_test1, omrtime_test3 */
	if (NULL == OMRPORTLIB->time_current_time_millis) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_current_time_millis is NULL\n");
	}

	/* omrtime_test1, omrtime_test3 */
	if (NULL == OMRPORTLIB->time_hires_clock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_hires_clock is NULL\n");
	}

	/* omrtime_test1, omrtime_test3 */
	if (NULL == OMRPORTLIB->time_hires_frequency) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_hires_frequency is NULL\n");
	}

	/* omrtime_test1, omrtime_test2, omrtime_test3 */
	if (NULL == OMRPORTLIB->time_hires_delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->time_hires_delta is NULL\n");
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library timer operations.
 *
 * Ensure that clocks are advancing.  Accuracy is not verified.
 *
 * Functions verified by this test:
 * @arg @ref omrtime.c::omrtime_msec_clock "omrtime_msec_clock()"
 * @arg @ref omrtime.c::omrtime_usec_clock "omrtime_usec_clock()"
 * @arg @ref omrtime.c::omrtime_current_time_nanos "omrtime_current_time_nanos()"
 * @arg @ref omrtime.c::omrtime_hires_clock "omrtime_hires_clock()"
 * @arg @ref omrtime.c::omrtime_hires_frequency "omrtime_hires_frequency()"
 * @arg @ref omrtime.c::omrtime_hires_delta "omrtime_hires_delta()"
 */
TEST(PortTimeTest, time_test1)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtime_test1";

	/* get a thread so that we can use native time delays (the original solution would spin and fail on fast machines) */
	omrthread_t self;

	reportTestEntry(OMRPORTLIB, testName);

	/* Verify the current time is advancing */
	/* attach the thread so we can use the delay primitives */
	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		/* success in starting up thread library and attaching */
		int64_t timeStart, timeCheck; /* current time in millis */
		uintptr_t mtimeStart, mtimeCheck;
		uintptr_t utimeStart, utimeCheck;
		int64_t ntimeStart, ntimeCheck; /* nanotime */
		int64_t nClocktimeStart, nClocktimeCheck; /* current time in nanos */
		uintptr_t success = 0;

		timeStart = omrtime_current_time_millis();
		mtimeStart = omrtime_msec_clock();
		utimeStart = omrtime_usec_clock();
		ntimeStart = omrtime_nano_time();
		nClocktimeStart = omrtime_current_time_nanos(&success);
		/* sleep for half a second */
		omrthread_sleep(500);
		timeCheck = omrtime_current_time_millis();
		mtimeCheck = omrtime_msec_clock();
		utimeCheck = omrtime_usec_clock();
		ntimeCheck = omrtime_nano_time();
		nClocktimeCheck = omrtime_current_time_nanos(&success);

		/* print errors if any of these failed */
		if (timeCheck == timeStart) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_millis did not change after a half-second forced delay\n");
		}
		if (mtimeStart == mtimeCheck) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_msec_clock did not change after a half-second forced delay\n");
		}
		if (utimeStart == utimeCheck) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_usec_clock did not change after a half-second forced delay\n");
		}
		if (ntimeStart == ntimeCheck) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_nano_time did not change after a half-second forced delay\n");
		}
		
		if (success) {
			if (nClocktimeStart == nClocktimeCheck) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_nanos did not change after a half-second forced delay\n");
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_nanos failed to return a valid time\n");
		}

		/* see if we can run the high-res test */
		if (0 != omrtime_hires_frequency()) {
			/* timer is valid so run the tests */
			uint64_t hiresTimeStart, hiresTimeCheck;

			hiresTimeStart = omrtime_hires_frequency();
			/* sleep for half a second */
			omrthread_sleep(500);
			hiresTimeCheck = omrtime_hires_frequency();

			if (hiresTimeCheck == hiresTimeStart) {
				/* we can run the tests since the timer is stable */
				hiresTimeStart = omrtime_hires_clock();
				/* sleep for half a second */
				omrthread_sleep(500);
				hiresTimeCheck = omrtime_hires_clock();
				if (hiresTimeCheck == hiresTimeStart) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_hires_clock has not advanced after a half-second forced delay\n");
				}
			} else {
				/* the timer advanced so don't even try to run the test */
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_hires_frequency has advanced\n");
			}
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_hires_frequency is 0, timer will not advance\n");
		}
	} else {
		/* failure initializing thread library: we won't be able to test reliably */
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_attach failed in omrtime_test1 so testing is not possible\n");
	}

	/* Verify that hires timer can advance */
	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library timer operations.
 *
 * Verify that hires timer properly handles rollover.
 *
 * Functions verified by this test:
 * @arg @ref omrtime.c::omrtime_hires_delta "omrtime_hires_delta()"
 */
TEST(PortTimeTest, time_test2)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtime_test2";

	uint64_t hiresTimeStart, hiresTimeStop;
	uint64_t simulatedValue, expectedValue;
	const int32_t timeInterval = 5;

	reportTestEntry(OMRPORTLIB, testName);

	/* The time interval being simulated */
	expectedValue = timeInterval * omrtime_hires_frequency();

	/* start < stop */
	simulatedValue = omrtime_hires_delta(0, expectedValue, omrtime_hires_frequency());
	if (simulatedValue != expectedValue) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_hires_delta returned %llu expected %llu\n", simulatedValue, expectedValue);
	}

	/* start > stop (add one to the expected value for 0, we can live with it ...*/
	hiresTimeStart = ((uint64_t)-1) - ((timeInterval - 2) * omrtime_hires_frequency());
	hiresTimeStop = 2 * omrtime_hires_frequency();
	simulatedValue = omrtime_hires_delta(hiresTimeStart, hiresTimeStop, omrtime_hires_frequency());
	if (simulatedValue != expectedValue + 1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_hires_delta returned %llu expected %llu\n", simulatedValue, expectedValue);
	}

	reportTestExit(OMRPORTLIB, testName);
}

/**
 * Verify port library timer operations.
 *
 * For various intervals verify the various timers for consistency.  There is no point in testing
 * against clock()
 *
 * Functions verified by this test:
 * @arg @ref omrtime.c::omrtime_msec_clock "omrtime_msec_clock()"
 * @arg @ref omrtime.c::omrtime_usec_clock "omrtime_usec_clock()"
 * @arg @ref omrtime.c::omrtime_current_time_nanos "omrtime_current_time_nanos()"
 * @arg @ref omrtime.c::omrtime_current_time_millis "omrtime_current_time_millis()"
 * @arg @ref omrtime.c::omrtime_hires_clock "omrtime_hires_clock()"
 * @arg @ref omrtime.c::omrtime_hires_frequency "omrtime_hires_frequency()"
 * @arg @ref omrtime.c::omrtime_hires_delta "omrtime_hires_delta()"
 */
TEST(PortTimeTest, time_test3)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtime_test3";

	int64_t oldTime, time, newTime, timeDelta;
	uintptr_t mtimeStart, mtimeStop, mtimeDelta;
	uintptr_t utimeStart, utimeStop, utimeDelta;
	uint64_t ntimeStart, ntimeStop, ntimeDelta;
	uint64_t hiresTimeStart, hiresTimeStop;
	uint64_t hiresDeltaAsMillis, hiresDeltaAsMicros;
	uint64_t ntimeDeltaAsMillis;
	uint32_t i;
	int32_t millires;

	reportTestEntry(OMRPORTLIB, testName);

	if (1 == omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE)) {
		/* one CPU means we have no chance of falling into the "difference CPUs have different times" trap.  Let the test begin */
		/*let's test the others vs current_time_millis*/
		portTestEnv->log("%10s %10s %10s %10s %12s %12s\n", "millires", "millis", "msec", "usec", "hires msec ", "hires usec");
		for (i = 0; i < J9TIME_REPEAT_TEST; i++) {
			uintptr_t failed = 0;
			uintptr_t success = 0;
			/*change of millis*/
			time = omrtime_current_time_millis();
			oldTime = time;
			while (oldTime == time) {
				oldTime = omrtime_current_time_millis();
			}
			millires = (int32_t)(oldTime - time);

			/*grab old times*/
			oldTime = omrtime_current_time_millis();
			newTime = oldTime;
			mtimeStart = omrtime_msec_clock();
			utimeStart = omrtime_usec_clock();
			ntimeStart = omrtime_current_time_nanos(&success);
			if (!success) {
				failed = 1;
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_nanos failed to return a valid time\n");
			}
			hiresTimeStart = omrtime_hires_clock();

			/*(busy)wait some time*/
			time = newTime + TEST_DURATION * (i + 1);
			while (newTime < time) {
				newTime = omrtime_current_time_millis();
			}
			/*grab new times*/
			hiresTimeStop = omrtime_hires_clock();
			ntimeStop = omrtime_current_time_nanos(&success);
			if (!success) {
				failed = 1;
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_nanos failed to return a valid time\n");
			}
			mtimeStop = omrtime_msec_clock();
			utimeStop = omrtime_usec_clock();	/*higher-precision CLK should get presidence NaH!*/

			hiresDeltaAsMillis = (uint32_t)omrtime_hires_delta(hiresTimeStart, hiresTimeStop, OMRPORT_TIME_DELTA_IN_MILLISECONDS);
			hiresDeltaAsMicros = (uint32_t)omrtime_hires_delta(hiresTimeStart, hiresTimeStop, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
			ntimeDelta = ntimeStop - ntimeStart;
			ntimeDeltaAsMillis = ntimeDelta / 1000000;
			mtimeDelta = mtimeStop - mtimeStart;
			utimeDelta = utimeStop - utimeStart;
			timeDelta = newTime - oldTime;

			portTestEnv->log("%10d %10d %10d %10d %12d %12d %12d\n",
						  millires, (int32_t)timeDelta, (int32_t)mtimeDelta, (int32_t)utimeDelta, (int32_t)ntimeDelta, (int32_t)hiresDeltaAsMillis, (int32_t)hiresDeltaAsMicros);

			hiresDeltaAsMillis = hiresDeltaAsMillis > mtimeDelta ? hiresDeltaAsMillis - mtimeDelta : mtimeDelta - hiresDeltaAsMillis;
			if (hiresDeltaAsMillis > (0.1 * mtimeDelta)) {
				portTestEnv->log("\n");
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_hires_clock() drift greater than 10%%\n");
				failed = 1;
			}
			
			ntimeDeltaAsMillis = ntimeDeltaAsMillis > mtimeDelta ? ntimeDeltaAsMillis - mtimeDelta : mtimeDelta - ntimeDeltaAsMillis;
			if (ntimeDeltaAsMillis > (0.1 * mtimeDelta)) {
				portTestEnv->log("\n");
				outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_nanos() drift greater than 10%%\n");
				failed = 1;
			}
			if (failed) {
				break;
			}
		}
	} else {
		/* test is invalid since this is a multi-way machine:  if we get the time on one CPU, then get rescheduled on a different one where we ask for the time,
		 * there is no reason why the time values need to be monotonically increasing */
		portTestEnv->log(LEVEL_ERROR, "Test is invalid since the host machine reports more than one CPU (time may differ across CPUs - makes test results useless - re-enable if we develop thread affinity support)\n");
	}
	portTestEnv->log("\n");

	reportTestExit(OMRPORTLIB, testName);
}

/*
 * Check that value2 is greater or equal to value1 and that the
 * difference is less than epsilon.
 */
static BOOLEAN
compareMillis(uint64_t value1, uint64_t value2, uint64_t epsilon)
{
	return (value2 >= value1) && (value2 - value1 < epsilon);
}

static uint64_t
portGetNanos(struct OMRPortLibrary *portLibrary)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	const uint64_t NANOS_PER_SECOND = 1000000000L;

	uint64_t ticks = omrtime_hires_clock();
	uint64_t ticksPerSecond = omrtime_hires_frequency();

	if (ticksPerSecond < NANOS_PER_SECOND) {
		ticks *= (NANOS_PER_SECOND / ticksPerSecond);
	} else {
		ticks /= (ticksPerSecond / NANOS_PER_SECOND);
	}

	return ticks;
}

/* Disabled because
		 * - generates way too much output
		 * - the output is confusing and does not help to diagnose errors
		 * - the test mostly does not actually test anything, because most of the time it detects:
		 * 		"WARNING: Skipping check due to possible NTP daemon interference."
		 * 		- this only adds to the confusion
		 * - the test is known to not work on x86 because of the omrtime_hires_clock implementation
		 */
TEST(DISABLED_PortTimeTest, time_test4)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtime_test4";

	const uintptr_t SLEEP_TIME = 100; /* in millis */
	/* EPSILON is an arbitrarily-chosen duration used for the sanity check. */
	const uintptr_t EPSILON = 2; /* in millis */
	const uintptr_t NUMBER_OF_ITERATIONS = 100;

	uintptr_t i;
	uint64_t milliStartOuter, milliEndOuter, milliDeltaOuter;
	uint64_t nanoStart, nanoEnd, nanoDeltaInMillis;
	uint64_t milliStartInner, milliEndInner, milliDeltaInner;
	uint64_t delta;

	portTestEnv->changeIndent(1);
	portTestEnv->log("\nRunning time test with:\n");
	portTestEnv->log("SLEEP_TIME = %u\n", SLEEP_TIME);
	portTestEnv->log("EPSILON = %u\n", EPSILON);
	portTestEnv->log("NUMBER_OF_ITERATIONS = %u\n", NUMBER_OF_ITERATIONS);
	portTestEnv->changeIndent(-1);

	for (i = 0; i < NUMBER_OF_ITERATIONS; i++) {

		portTestEnv->log("\nIteration %u:\n", i);

		milliStartOuter = omrtime_current_time_millis();
		nanoStart = portGetNanos(OMRPORTLIB);
		milliStartInner = omrtime_current_time_millis();

		if (0 != omrthread_sleep(SLEEP_TIME)) {
			portTestEnv->log(LEVEL_WARN, "WARNING: Skipping check due to omrthread_sleep() returning non-zero.\n");
			continue;
		}

		milliEndInner = omrtime_current_time_millis();
		nanoEnd = portGetNanos(OMRPORTLIB);
		milliEndOuter = omrtime_current_time_millis();

		milliDeltaInner = milliEndInner - milliStartInner;
		nanoDeltaInMillis = (nanoEnd - nanoStart) / 1000000;
		milliDeltaOuter = milliEndOuter - milliStartOuter;

		portTestEnv->log("milliStartOuter = %llu\n", milliStartOuter);
		portTestEnv->log("nanoStart = %llu\n", nanoStart);
		portTestEnv->log("milliStartInner = %llu\n", milliStartInner);

		portTestEnv->log("milliEndInner = %llu\n", milliEndInner);
		portTestEnv->log("nanoEnd = %llu\n", nanoEnd);
		portTestEnv->log("milliEndOuter = %llu\n", milliEndOuter);

		portTestEnv->log("milliDeltaInner = %llu\n", milliDeltaInner);
		portTestEnv->log("nanoDeltaInMillis = %llu\n", nanoDeltaInMillis);
		portTestEnv->log("milliDeltaOuter = %llu\n", milliDeltaOuter);

		/* To avoid false positives, do a sanity check on all the millis values. */
		if (!compareMillis(milliStartOuter, milliStartInner, EPSILON) ||
			!compareMillis(SLEEP_TIME, milliDeltaInner, EPSILON) ||
			!compareMillis(milliEndInner, milliEndOuter, EPSILON) ||
			(milliDeltaOuter < milliDeltaInner)) {
			portTestEnv->log(LEVEL_WARN, "WARNING: Skipping check due to possible NTP daemon interference.\n");
			continue;
		}

		delta = (milliDeltaOuter > nanoDeltaInMillis ? milliDeltaOuter - nanoDeltaInMillis : nanoDeltaInMillis - milliDeltaOuter);
		if (delta > EPSILON) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "abs(milliDeltaOuter - nanoDeltaInMillis) > EPSILON (%llu)", delta);
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_millis() is not consistent with omrtime_hires_clock()!");
			break;
		}

		delta = (nanoDeltaInMillis > milliDeltaInner ? nanoDeltaInMillis - milliDeltaInner : milliDeltaInner - nanoDeltaInMillis);
		if (delta > EPSILON) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "abs(nanoDeltaInMillis - milliDeltaInner) > EPSILON (%llu)", delta);
			outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_current_time_millis() is not consistent with omrtime_hires_clock()!");
			break;
		}
	}

	reportTestExit(OMRPORTLIB, testName);
}


#define J9TIME_TEST_DIRECTION_TIMEOUT_MILLIS 60000 /* 1 minute */
static uintptr_t omrtimeTestDirectionNumThreads = 0;

typedef struct J9TimeTestDirectionStruct {
	struct OMRPortLibrary *portLibrary;
	omrthread_monitor_t monitor;
	BOOLEAN failed;
	uintptr_t finishedCount;
} J9TimeTestDirectionStruct;

/**
 * Check that omrtime_nano_time always moves forward
 */
TEST(PortTimeTest, time_nano_time_direction)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	omrthread_t self;
	const char *testName = "omrtime_nano_time_direction";

	reportTestEntry(OMRPORTLIB, testName);

#if defined(OMR_OS_WINDOWS)
	/**
	 * On Windows, if QueryPerformanceCounter is used on a multiprocessor computer,
	 * time might be different accross CPUs. Therefore skip this test and only
	 * re-enable if we develop thread affinity support.
	 */
	{
		LARGE_INTEGER i;
		if (QueryPerformanceCounter(&i)) {
			portTestEnv->log(LEVEL_WARN, "WARNING: Test is invalid since the host machine uses QueryPerformanceCounter() (time may differ across CPUs - makes test results useless - re-enable if we develop thread affinity support)\n");
			reportTestExit(OMRPORTLIB, testName);
		}
	}
#endif /* defined(OMR_OS_WINDOWS) */

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		/* success in starting up thread library and attaching */
		J9TimeTestDirectionStruct tds;
		tds.failed = FALSE;
		tds.portLibrary = OMRPORTLIB;
		tds.finishedCount = 0;

		if (0 == omrthread_monitor_init(&tds.monitor, 0)) {
			uintptr_t i;
			intptr_t waitRetVal = 0;
			const uintptr_t threadToCPUFactor = 2;
			omrthread_t *threads = NULL;

			omrtimeTestDirectionNumThreads = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE) * threadToCPUFactor;
			threads = (omrthread_t *)omrmem_allocate_memory(omrtimeTestDirectionNumThreads * sizeof(omrthread_t), OMRMEM_CATEGORY_PORT_LIBRARY);

			if (NULL != threads) {
				if (0 == omrthread_monitor_enter(tds.monitor)) {
					for (i = 0; i < omrtimeTestDirectionNumThreads ; i ++) {
						intptr_t rc = omrthread_create(&threads[i], 128 * 1024, J9THREAD_PRIORITY_MAX, 0, &nanoTimeDirectionTest, &tds);
						if (0 != rc) {
							outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create thread, rc=%zd, i=%zu", rc, i);
						}
					}

					portTestEnv->log("Num threads created: %zu\n", omrtimeTestDirectionNumThreads);
					portTestEnv->log("Threads that have finished running: ");

					/* wait for all threads to finish */
					while ((0 == waitRetVal) && (tds.finishedCount < omrtimeTestDirectionNumThreads)) {
						waitRetVal = omrthread_monitor_wait_timed(tds.monitor, J9TIME_TEST_DIRECTION_TIMEOUT_MILLIS, 0);
					}

					portTestEnv->log("\n");

					if (0 != waitRetVal) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "omrthread_monitor_wait_timed() failed, waitRetVal=%zd", waitRetVal);
					}

					if (tds.failed) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "omrtime_hires_clock() did not go forward in at least one of the launched threads");
					}

					omrthread_monitor_exit(tds.monitor);
				}  else {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to enter tds.monitor");
				}

				omrmem_free_memory(threads);
			} else {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate memory for threads array");
			}
			omrthread_monitor_destroy(tds.monitor);
		} else {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to initialize tds.monitor");
		}

		omrthread_detach(self);
	} else {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to attach to thread library");
	}

	reportTestExit(OMRPORTLIB, testName);
}


static int
J9THREAD_PROC nanoTimeDirectionTest(void *arg)
{
	J9TimeTestDirectionStruct *tds = (J9TimeTestDirectionStruct *) arg;
	uintptr_t i = 0;
	const uintptr_t numLoops = 500;
	const I_64 sleepMillis = 20; /* 20*500 -> total thread execution time should be ~ 10 seconds */
	OMRPORT_ACCESS_FROM_OMRPORT(tds->portLibrary);

	for (i = 0 ; i < numLoops ; i++) {
		I_64 finish = 0;
		I_64 start = omrtime_nano_time();

		if (0 != omrthread_sleep(sleepMillis)) {
			portTestEnv->log(LEVEL_ERROR, "\tomrthread_sleep() did not return zero.\n");
			omrthread_monitor_enter(tds->monitor);
			tds->failed = TRUE;
			omrthread_monitor_exit(tds->monitor);
			break;
		}

		finish = omrtime_nano_time();
		if (finish <= start) {
			portTestEnv->log(LEVEL_ERROR, "\tTime did not go forward after omrthread_sleep, start=%llu, finish=%llu\n", start, finish);
			omrthread_monitor_enter(tds->monitor);
			tds->failed = TRUE;
			omrthread_monitor_exit(tds->monitor);
			break;
		}
	}

	omrthread_monitor_enter(tds->monitor);
	tds->finishedCount += 1;
	if (omrtimeTestDirectionNumThreads == tds->finishedCount) {
		omrthread_monitor_notify(tds->monitor);
	}
	portTestEnv->log("%zu ", tds->finishedCount);
	omrthread_monitor_exit(tds->monitor);

	return 0;
}

/**
 * Computes error as a fraction of expected result.
 *
 * @param[in] exp Expected result
 * @param[in] actual Actual result
 *
 * @return abs(actual - exp) / exp
 */
static double
omrtime_test_compute_error_pct(double exp, double actual)
{
	double error = 0.0;

	if (exp > actual) {
		error = (exp - actual) / exp;
	} else {
		error = (actual - exp) / exp;
	}
	return error;
}

/**
 * Verify precision of omrtime_hires_delta().
 *
 * omrtime_hires_delta() converts the time interval into the requested resolution by
 * effectively multiplying the interval by (requiredRes / omrtime_hires_frequency()).
 *
 * We use a test interval equal to (ticksPerSec = omrtime_hires_frequency()), so if
 * there was no loss of precision, we should have:
 *     delta = ticksPerSec * (requiredRes / ticksPerSec) = requiredRes
 *
 * This test fails if the difference between the computed and expected delta is > 1%.
 *
 * The worst roundoff error cases for omrtime_hires_delta() are:
 * 1. (ticksPerSec > requiredRes) and (ticksPerSec / requiredRes) truncates down a lot.
 * 2. (ticksPerSec < requiredRes) and (ticks * requiredRes) overflows.
 *
 * Functions verified by this test:
 * @arg @ref omrtime.c::omrtime_hires_delta "omrtime_hires_delta()"
 */
TEST(PortTimeTest, time_test_hires_delta_rounding)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	const char *testName = "omrtime_test_hires_delta_rounding";
	uint64_t ticksPerSec = 0;
	uint64_t requiredRes = 0;
	uint64_t delta = 0;
	double error = 0.0;

	reportTestEntry(OMRPORTLIB, testName);

	ticksPerSec = omrtime_hires_frequency();
	if (0 == ticksPerSec) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "invalid hires frequency\n");
		goto exit;
	}
	portTestEnv->log("hires frequency: %llu\n", ticksPerSec);

	/* Case 1: ticksPerSec / requiredRes potentially rounds down */
	/*
	 * requiredRes is picked so that the ratio (ticksPerSec / requiredRes) is 1.9, which
	 * has a big fractional part that would be lost if naive integer math were used.
	 *
	 * If we did lose the fractional part, then we would get:
	 * delta = ticksPerSec / (ticksPerSec / requiredRes) = ticksPerSec / 1 = ticksPerSec
	 * error = abs(ticksPerSec - requiredRes) / requiredRes = 0.9 ... which fails the test.
	 */
	requiredRes = (uint64_t)(ticksPerSec / (double)1.9);

	delta = omrtime_hires_delta(0, ticksPerSec, requiredRes);
	if (0 == delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 1. omrtime_hires_delta returned 0");
	}

	error = omrtime_test_compute_error_pct((double)requiredRes, (double)delta);
	if (error > 0.01) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 1. error is too high");
	}
	portTestEnv->log("Case 1. expected: %llu    actual: %llu    error: %lf\n", requiredRes, delta, error);

	/* Case 2: ticks * requiredRes overflows */
	requiredRes = ((uint64_t)-1) / (ticksPerSec - 1) + ticksPerSec;

	delta = omrtime_hires_delta(0, ticksPerSec, requiredRes);
	if (0 == delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 2. omrtime_hires_delta returned 0");
	}

	error = omrtime_test_compute_error_pct((double)requiredRes, (double)delta);
	if (error > 0.01) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 2. error is too high");
	}
	portTestEnv->log("Case 2. expected: %llu    actual: %llu    error: %lf\n", requiredRes, delta, error);

	/* Case 3: requiredRes = ticksPerSec */
	requiredRes = ticksPerSec;

	delta = omrtime_hires_delta(0, ticksPerSec, requiredRes);
	if (0 == delta) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 3. omrtime_hires_delta returned 0");
	}

	error = omrtime_test_compute_error_pct((double)requiredRes, (double)delta);
	if (error > 0.01) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "    Case 3. error is too high");
	}
	portTestEnv->log("Case 3. expected: %llu    actual: %llu    error: %lf\n", requiredRes, delta, error);

exit:
	reportTestExit(OMRPORTLIB, testName);
}
