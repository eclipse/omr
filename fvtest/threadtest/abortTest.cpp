/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp. and others
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "omrTest.h"
#include "omrthread.h"

#ifdef OMR_THR_THREE_TIER_LOCKING

#define START_DELAY (1 * 1000)
#define TIMEOUT (60 * 1000)

static void abortNull();
static int runningMain(void *arg);
static int sleepingMain(void *arg);
static int waitingMain(void *arg);

typedef int (*functionPoint)(void *);

static intptr_t
createDefaultThread(omrthread_t *t, functionPoint entrypoint, void *entryarg)
{
	return omrthread_create_ex(t, J9THREAD_ATTR_DEFAULT, 0, entrypoint, entryarg);
}

/*
 * zos does not support death test
 */
#ifndef J9ZOS390
TEST(ThreadAbortDeathTest, Nullpointer)
{
	ASSERT_DEATH(abortNull(), ".*");
}

static void
abortNull()
{
	omrthread_t t = NULL;
	omrthread_abort(t);
}
#endif /* J9ZOS390 */

typedef struct run_testdata_t {
	omrthread_monitor_t startSync;
	omrthread_monitor_t exitSync;
	volatile bool started;
	volatile bool done;
	volatile bool rc;
} run_testdata_t;

TEST(ThreadAbortTest, Running)
{
	omrthread_t t;
	run_testdata_t testdata;

	testdata.started = false;
	testdata.done = false;
	testdata.rc = false;

	omrthread_monitor_init(&testdata.startSync, 0);
	omrthread_monitor_init(&testdata.exitSync, 0);

	createDefaultThread(&t, runningMain, &testdata);

	/* make sure runningMain thread has started: */
	omrthread_monitor_enter(testdata.startSync);
	while (!testdata.started) {
		omrthread_monitor_wait(testdata.startSync);
	}
	omrthread_monitor_exit(testdata.startSync);

	omrthread_abort(t);

	omrthread_monitor_enter(testdata.exitSync);
	while (!testdata.done) {
		omrthread_monitor_wait(testdata.exitSync);
	}
	omrthread_monitor_exit(testdata.exitSync);

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.startSync);

	EXPECT_TRUE(testdata.rc) << "Failed to abort running thread";
}

static int
runningMain(void *arg)
{
	run_testdata_t *testdata = (run_testdata_t *)arg;
	J9AbstractThread *self = (J9AbstractThread *)omrthread_self();

	omrthread_monitor_enter(testdata->startSync);
	testdata->started = true;
	omrthread_monitor_notify(testdata->startSync);
	omrthread_monitor_exit(testdata->startSync);

	clock_t elapsedTime = 0;
	clock_t start = clock() * 1000 / CLOCKS_PER_SEC;
	do {
		elapsedTime = clock() * 1000 / CLOCKS_PER_SEC - start;
		omrthread_yield();
	} while ((!(self->flags & J9THREAD_FLAG_ABORTED)) && (elapsedTime < TIMEOUT));

	testdata->rc = (0 != (self->flags & J9THREAD_FLAG_ABORTED));
	omrthread_monitor_enter(testdata->exitSync);
	testdata->done = true;
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}

typedef struct sleep_testdata_t {
	omrthread_monitor_t sync;
} sleep_testdata_t;

TEST(ThreadAbortTest, Sleeping)
{
	omrthread_t t;
	sleep_testdata_t d;
	sleep_testdata_t *data = &d;

	omrthread_monitor_init(&data->sync, 0);

	omrthread_monitor_enter(data->sync);
	createDefaultThread(&t, sleepingMain, data);
	omrthread_monitor_wait(data->sync); // startup

	omrthread_abort(t);

	omrthread_monitor_exit(data->sync);
	omrthread_monitor_destroy(data->sync);
}

static int
sleepingMain(void *arg)
{
	sleep_testdata_t *data = (sleep_testdata_t *)arg;

	omrthread_monitor_enter(data->sync);
	omrthread_monitor_notify(data->sync);
	omrthread_monitor_exit(data->sync);

	intptr_t rc = J9THREAD_SUCCESS;
	do {
		const int timeout = 100; // in millis
		rc = omrthread_sleep_interruptable(timeout, 0);
	} while (rc == J9THREAD_SUCCESS);

	EXPECT_EQ(rc, J9THREAD_PRIORITY_INTERRUPTED);

	return 0;
}

typedef struct wait_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t waitSync;
	volatile int waiting;
	volatile int aborted;
	volatile intptr_t rc;
} wait_testdata_t;

TEST(ThreadAbortTest, Waiting)
{
	omrthread_t t;
	wait_testdata_t testdata;

	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.waitSync, 0);
	testdata.waiting = 0;
	testdata.aborted = 0;

	omrthread_monitor_enter(testdata.exitSync);

	createDefaultThread(&t, waitingMain, &testdata);
	while (1) {
		omrthread_sleep(START_DELAY);
		omrthread_monitor_enter(testdata.waitSync);
		if (1 == testdata.waiting) {
			omrthread_monitor_exit(testdata.waitSync);
			break;
		}
		omrthread_monitor_exit(testdata.waitSync);
	}

	omrthread_abort(t);

	if (0 == testdata.aborted) {
		omrthread_monitor_wait(testdata.exitSync);
	}
	omrthread_monitor_exit(testdata.exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata.waitSync;
		EXPECT_TRUE(NULL == mon->waiting);
		EXPECT_TRUE(NULL == mon->blocking);
		EXPECT_TRUE(0 == mon->count);
		EXPECT_TRUE(NULL == mon->owner);
	}

	omrthread_monitor_destroy(testdata.exitSync);
	omrthread_monitor_destroy(testdata.waitSync);

	EXPECT_TRUE(testdata.rc == J9THREAD_INTERRUPTED_MONITOR_ENTER) << "Failed to abort waiting thread!";
}

static int
waitingMain(void *arg)
{
	wait_testdata_t *testdata = (wait_testdata_t *)arg;

	omrthread_monitor_enter(testdata->waitSync);
	testdata->waiting = 1;
	testdata->rc = omrthread_monitor_wait_interruptable(testdata->waitSync, 0, 0);

	if (J9THREAD_INTERRUPTED_MONITOR_ENTER == testdata->rc) {
		J9AbstractThread *self = (J9AbstractThread *)omrthread_self();
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata->waitSync;

		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTABLE));
		assert(!(self->flags & J9THREAD_FLAG_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_PRIORITY_INTERRUPTED));
		assert(!(self->flags & J9THREAD_FLAG_BLOCKED));
		assert(!(self->flags & J9THREAD_FLAG_WAITING));
		EXPECT_TRUE(NULL == self->monitor);

		EXPECT_TRUE(NULL == mon->waiting);
		/*assert((J9AbstractThread *)mon->owner == self);*/
		EXPECT_TRUE(NULL == mon->blocking);
		/*assert(mon->count == 1);*/
	} else {
		omrthread_monitor_exit(testdata->waitSync);
	}

	EXPECT_EQ(0, omrthread_monitor_enter(testdata->exitSync));
	testdata->aborted = 1;
	EXPECT_EQ(0, omrthread_monitor_notify(testdata->exitSync));
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}

#endif /* OMR_THR_THREE_TIER_LOCKING */
