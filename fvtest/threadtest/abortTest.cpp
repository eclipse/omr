/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2015
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
	omrthread_monitor_t exitSync;
	volatile bool done;
	volatile bool rc;
} run_testdata_t;

TEST(ThreadAbortTest, Running)
{
	omrthread_t t;
	run_testdata_t testdata;

	testdata.done = false;
	omrthread_monitor_init(&testdata.exitSync, 0);


	createDefaultThread(&t, runningMain, &testdata);
	omrthread_sleep(START_DELAY);

	omrthread_abort(t);

	omrthread_monitor_enter(testdata.exitSync);
	while (!testdata.done) {
		omrthread_monitor_wait(testdata.exitSync);
	}
	omrthread_monitor_exit(testdata.exitSync);

	omrthread_monitor_destroy(testdata.exitSync);

	EXPECT_TRUE(testdata.rc) << "Failed to abort running thread";
}

static int
runningMain(void *arg)
{
	run_testdata_t *testdata = (run_testdata_t *)arg;
	J9AbstractThread *self = (J9AbstractThread *)omrthread_self();

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
	omrthread_monitor_t exitSync;
	volatile intptr_t rc;
} sleep_testdata_t;

TEST(ThreadAbortTest, Sleeping)
{
	omrthread_t t;
	sleep_testdata_t mythread;

	omrthread_monitor_init(&mythread.exitSync, 0);
	omrthread_monitor_enter(mythread.exitSync);
	createDefaultThread(&t, sleepingMain, &mythread);

	omrthread_sleep(START_DELAY);
	omrthread_abort(t);

	omrthread_monitor_wait(mythread.exitSync);
	omrthread_monitor_exit(mythread.exitSync);

	omrthread_monitor_destroy(mythread.exitSync);

	EXPECT_TRUE(mythread.rc == J9THREAD_PRIORITY_INTERRUPTED) << "Failed to abort sleeping thread!";
}

static int
sleepingMain(void *arg)
{
	sleep_testdata_t *testdata = (sleep_testdata_t *)arg;
	intptr_t rc;

	rc = omrthread_sleep_interruptable(60 * 1000, 0);

	omrthread_monitor_enter(testdata->exitSync);
	testdata->rc = rc;
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}


typedef struct wait_testdata_t {
	omrthread_monitor_t exitSync;
	omrthread_monitor_t waitSync;
	volatile int waiting;
	volatile intptr_t rc;
} wait_testdata_t;

TEST(ThreadAbortTest, Waiting)
{
	omrthread_t t;
	wait_testdata_t testdata;

	omrthread_monitor_init(&testdata.exitSync, 0);
	omrthread_monitor_init(&testdata.waitSync, 0);
	testdata.waiting = 0;

	omrthread_monitor_enter(testdata.exitSync);

	createDefaultThread(&t, waitingMain, &testdata);
	while (1) {
		omrthread_sleep(START_DELAY);
		omrthread_monitor_enter(testdata.waitSync);
		if (testdata.waiting == 1) {
			omrthread_monitor_exit(testdata.waitSync);
			break;
		}
		omrthread_monitor_exit(testdata.waitSync);
	}

	omrthread_abort(t);

	omrthread_monitor_wait(testdata.exitSync);
	omrthread_monitor_exit(testdata.exitSync);

	{
		J9ThreadAbstractMonitor *mon = (J9ThreadAbstractMonitor *)testdata.waitSync;
		assert(mon->waiting == NULL);
		assert(mon->blocking == NULL);
		assert(mon->count == 0);
		assert(mon->owner == NULL);
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
		assert(self->monitor == NULL);

		assert(mon->waiting == NULL);
		/*assert((J9AbstractThread *)mon->owner == self);*/
		assert(mon->blocking == NULL);
		/*assert(mon->count == 1);*/
	} else {
		omrthread_monitor_exit(testdata->waitSync);
	}

	omrthread_monitor_enter(testdata->exitSync);
	omrthread_monitor_notify(testdata->exitSync);
	omrthread_monitor_exit(testdata->exitSync);

	return 0;
}

#endif /* OMR_THR_THREE_TIER_LOCKING */
