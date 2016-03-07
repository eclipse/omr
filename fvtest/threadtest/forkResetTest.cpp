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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "omrTest.h"
#include "threadTestHelp.h"

#define START_DELAY (1 * 1000)
#define TIMEOUT (60 * 1000)

typedef struct testdata_t {
	omrthread_monitor_t monitor;
	volatile intptr_t rc;
} testdata_t;

typedef struct testdata2_t {
	omrthread_monitor_t monitor1;
	omrthread_monitor_t monitor2;
	volatile intptr_t rc;
} testdata2_t;

static void childForkHelper(int forkCount);
static void waitNotifyStart(int forkCount);
static void waitNotifyChild(testdata_t *, int[2]);
static int waitNotifyMain(void *arg);
static void waitNotify2Start(int forkCount);
static void waitNotify2Child(testdata2_t *, int[2]);
static int waitNotify2aMain(void *arg);
static int waitNotify2bMain(void *arg);
static void attachDetachStart(int forkCount);

static void
childForkHelper(int forkCount)
{
	while (forkCount > 0) {
		omrthread_lib_postForkChild();
		omrthread_lib_preFork();
		if (0 == fork()) {
			forkCount = forkCount - 1;
		} else {
			exit(0);
		}
	}
}

/**
 * Note: All tests are disabled until issues with hangs related to undefined behavior are solved in
 * omrthread_lib_post_fork_reset.
 *
 * Tests the following:
 * 1. Sibling thread enter monitor before fork and the child process can enter and exit the monitor
 * 2. Sibling thread waiting on monitor before the fork and the child process can enter and exit the monitor
 */
TEST(ThreadForkResetTest, WaitNotify)
{
	waitNotifyStart(1);
}

/**
 *
 * Parent thread
 * 		Sibling thread
 * 			Forked process
 *
 * 1. Parent initializes monitor and enters
 * 2. Parent starts sibling thread
 * 		1. Sibling thread enters monitor
 * 3. Parent waits for sibling thread to be done entering monitor
 * 		2. Sibling thread notifies parent it can now fork
 * 		3. Sibling thread waits until after fork
 * 4. Parent forks
 * 			1. Child process enters monitor
 * 			2. Child process exits monitor
 * 			3. Child process destroys monitor
 * 			4. Child process ends
 * 5. Parent notifies sibling that fork is done
 * 6. Parent waits for sibling thread to exit using join
 * 		5. Sibling thread exits monitor
 * 		6. Sibling thread ends
 * 6. Parent destroys monitor
 * 7. Test ends
 */
static void
waitNotifyStart(int forkCount)
{
	omrthread_t t;
	testdata_t testdata;
	int pipedata[2];

	/*
	 * Pre-fork:
	 * Initialize and enter the monitor, then begin the sibling thread.
	 * The sibling thread cannot enter the monitor because it is already entered here.
	 */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	omrthread_monitor_init(&testdata.monitor, 0);
	omrthread_monitor_enter(testdata.monitor);
	ASSERT_NO_FATAL_FAILURE(createJoinableThread(&t, waitNotifyMain, &testdata));

	/*
	 * Wait releases it, allowing the sibling thread to continue.
	 * The sibling will do notify once its entered the monitor and then wait until after the fork.
	 * The sibling's wait will release the monitor, allowing the parent to continue here.
	 */
	EXPECT_FALSE(J9THREAD_TIMED_OUT == omrthread_monitor_wait_timed(testdata.monitor, TIMEOUT, 0))
			<< "Timed out waiting for sibling thread to enter monitor.";

	omrthread_lib_preFork();
	if (0 >= forkCount || 0 == fork()) {
		childForkHelper(forkCount - 1);
		/* Child process post-fork */
		waitNotifyChild(&testdata, pipedata);
	}
	omrthread_lib_postForkParent();

	/*
	 * Parent process post-fork:
	 * Notify the sibling that fork is done and it can now exit.
	 * Then, the wait call will release the monitor to let the sibling thread finish.
	 * Once the sibling thread done, it will notify again and exit the monitor to
	 * let he parent destroy the monitor.
	 */
	ssize_t readBytes = read(pipedata[0], (int *)&testdata.rc, sizeof(testdata.rc));
	EXPECT_TRUE(readBytes == sizeof(testdata.rc)) << "Didn't read back enough from pipe";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in child process.";
	close(pipedata[0]);
	close(pipedata[1]);
	EXPECT_TRUE(0 == omrthread_monitor_notify(testdata.monitor))
			<< "Notifying monitor failed after fork: parent thread does not own monitor.";
	EXPECT_TRUE(0 == omrthread_monitor_exit(testdata.monitor))
			<< "Exiting monitor after fork failed: parent thread does not own monitor.";

	VERBOSE_JOIN(t, J9THREAD_SUCCESS);

	EXPECT_TRUE(0 == omrthread_monitor_destroy(testdata.monitor))
			<< "Destroying monitor failed: monitor still in use.";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in sibling thread.";
}

static void
waitNotifyChild(testdata_t *testdata, int pipedata[2])
{
	omrthread_lib_postForkChild();
	testdata->rc = omrthread_monitor_enter(testdata->monitor);
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_destroy(testdata->monitor);
	}
	J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&testdata->rc, sizeof(testdata->rc)));
	close(pipedata[0]);
	close(pipedata[1]);
	exit(0);
}

static int
waitNotifyMain(void *arg)
{
	testdata_t *testdata = (testdata_t *)arg;
	testdata->rc = omrthread_monitor_enter(testdata->monitor);
	/* Notify enter is complete. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify(testdata->monitor);
	}
	/* Wait until after fork. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_wait_timed(testdata->monitor, TIMEOUT, 0);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor);
	}

	return 0;
}

/**
 * Note: All tests are disabled until issues with hangs related to undefined behavior are solved in
 * omrthread_lib_post_fork_reset.
 *
 * Tests:
 * 1. Monitor owned by sibling thread in parent process can do monitor enter, wait, notify,
 * and exit as if it were unowned.
 * 2.
 */

TEST(ThreadForkResetTest, WaitNotify2)
{
	waitNotify2Start(2);
}

/*
 * Sibling 1 is to enter monitor and wait, then sibling 2 is to enter monitor,
 * notify on it, and not exit. Then sibling 1 gets the notify and notifies the
 * parent to continue to fork.
 *
 * sib 1 is queued on monitor 1, already notified
 * sib 2 holds lock 1, waits on 2
 * parent is go, holds lock 2
 *
 * Three monitors: one for sibling 1, one for sibling 2, one for the test notification
 * 1. Parent inits monitor 1 and 2
 * 2. Parent enters monitor 1
 * 3. Parent starts sibling 1
 * 		1. Sibling 1 enters monitor 1
 * 4. Parent waits on monitor 1
 * 		2. Sibling 1 notifies and waits on monitor 1
 * 5. Parent exits monitor 1
 * 6. Parent enters monitor 2
 * 7. Parent starts sibling 2
 * 			1. Sibling 2 enters monitor 2
 * 8. Parent waits on monitor 2
 * 			2. Sibling 2 enters and notifies monitor 1 without exiting
 * 			3. Sibling 2 notifies and waits on monitor 2
 * 9. Parent forks
 * 				1. Child process can enter monitor 1, wait/timeout, and exit monitor
 * 				2. Child process can enter monitor 1, notify_all, and exit monitor
 * 				3. Child process exits and destroys monitor 2
 * 				4. Child process destroys monitor 1
 * 				5. Child process writes return code on pipe
 * 10. Parent notifies and exits monitor 2 then waits on joining the threads
 * 			4. Sibling 2 waits on monitor 1
 * 		3. Sibling 1 notifies and exits monitor 1
 * 		4. Sibling 1 ends
 * 			5. Sibling 2 exits monitor 1 and 2
 * 			6. Sibling 2 ends
 * 11. Parent destroys monitor 1 and 2
 */

static void
waitNotify2Start(int forkCount)
{
	omrthread_t t1;
	omrthread_t t2;
	testdata2_t testdata;
	int pipedata[2];

	/*
	 * Pre-fork:
	 * Initialize and enter the monitor, then begin the sibling thread.
	 * The sibling thread cannot enter the monitor because it is already entered here.
	 */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	omrthread_monitor_init(&testdata.monitor1, 0);
	omrthread_monitor_init(&testdata.monitor2, 0);

	/*
	 * Wait releases it, allowing the sibling thread to continue.
	 * The sibling will do notify once its entered the monitor and then wait until after the fork.
	 * The sibling's wait will release the monitor, allowing the parent to continue here.
	 */
	omrthread_monitor_enter(testdata.monitor1);
	ASSERT_NO_FATAL_FAILURE(createJoinableThread(&t1, waitNotify2aMain, &testdata));
	EXPECT_FALSE(J9THREAD_TIMED_OUT == omrthread_monitor_wait_timed(testdata.monitor1, TIMEOUT, 0))
			<< "Timed out waiting for sibling thread 1 to enter monitor 1.";
	EXPECT_TRUE(0 == omrthread_monitor_exit(testdata.monitor1))
			<< "Exiting monitor 1 before fork failed: parent thread does not own monitor.";

	omrthread_monitor_enter(testdata.monitor2);
	ASSERT_NO_FATAL_FAILURE(createJoinableThread(&t2, waitNotify2bMain, &testdata));
	EXPECT_FALSE(J9THREAD_TIMED_OUT == omrthread_monitor_wait_timed(testdata.monitor2, TIMEOUT, 0))
			<< "Timed out waiting for sibling thread 2 to enter monitor 2.";

	omrthread_lib_preFork();
	if (0 >= forkCount || 0 == fork()) {
		childForkHelper(forkCount - 1);
		/* Child process post-fork */
		waitNotify2Child(&testdata, pipedata);
	}
	omrthread_lib_postForkParent();

	/*
	 * Parent process post-fork:
	 * Notify the sibling that fork is done and it can now exit.
	 * Then, the wait call will release the monitor to let the sibling thread finish.
	 * Once the sibling thread done, it will notify again and exit the monitor to
	 * let he parent destroy the monitor.
	 */
	ssize_t readBytes = read(pipedata[0], (int *)&testdata.rc, sizeof(testdata.rc));
	EXPECT_TRUE(readBytes == sizeof(testdata.rc)) << "Didn't read back enough from pipe";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in child process.";
	close(pipedata[0]);
	close(pipedata[1]);
	EXPECT_TRUE(0 == omrthread_monitor_notify(testdata.monitor2))
			<< "Notifying monitor 2 failed after fork: parent thread does not own monitor.";
	EXPECT_TRUE(0 == omrthread_monitor_exit(testdata.monitor2))
			<< "Exiting monitor 2 after fork failed: parent thread does not own monitor.";

	VERBOSE_JOIN(t1, J9THREAD_SUCCESS);
	VERBOSE_JOIN(t2, J9THREAD_SUCCESS);

	EXPECT_TRUE(0 == omrthread_monitor_destroy(testdata.monitor1))
			<< "Destroying monitor 1 failed: monitor still in use.";
	EXPECT_TRUE(0 == omrthread_monitor_destroy(testdata.monitor2))
			<< "Destroying monitor 2 failed: monitor still in use.";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in sibling thread.";
}

static void
waitNotify2Child(testdata2_t *testdata, int pipedata[2])
{
	omrthread_lib_postForkChild();
	testdata->rc = omrthread_monitor_enter(testdata->monitor1);
	if (0 == testdata->rc) {
		if (J9THREAD_TIMED_OUT != omrthread_monitor_wait_timed(testdata->monitor1, START_DELAY, 0)) {
			testdata->rc = 1;
		}
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_enter(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify_all(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor2);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_destroy(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_destroy(testdata->monitor2);
	}
	J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&testdata->rc, sizeof(testdata->rc)));
	close(pipedata[0]);
	close(pipedata[1]);
	exit(0);
}

static int
waitNotify2aMain(void *arg)
{
	testdata2_t *testdata = (testdata2_t *)arg;
	testdata->rc = omrthread_monitor_enter(testdata->monitor1);

	/* Notify enter is complete. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify(testdata->monitor1);
	}
	/* Wait until after fork. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_wait_timed(testdata->monitor1, TIMEOUT, 0);
	}
	/* Notify done. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor1);
	}

	return 0;
}

static int
waitNotify2bMain(void *arg)
{
	testdata2_t *testdata = (testdata2_t *)arg;
	testdata->rc = omrthread_monitor_enter(testdata->monitor2);
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_enter(testdata->monitor1);
	}
	/* Notify enter is complete. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify(testdata->monitor2);
	}
	/* Wait until after fork. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_wait_timed(testdata->monitor2, TIMEOUT, 0);
	}
	/* Wait for sibling 1 to finish. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_wait_timed(testdata->monitor1, TIMEOUT, 0);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor1);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor2);
	}

	return 0;
}

TEST(ThreadForkResetTest, AttachDetach)
{
	attachDetachStart(1);
}

static void
attachDetachStart(int forkCount)
{
	testdata_t testdata;
	int pipedata[2];

	/* Pre-fork */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	omrthread_lib_preFork();
	if (0 >= forkCount || 0 == fork()) {
		childForkHelper(forkCount - 1);
		/* Post-fork child process. */
		omrthread_t self = NULL;

		omrthread_lib_postForkChild();
		testdata.rc = omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);
		if (0 == testdata.rc) {
			omrthread_detach(self);
		}

		J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&testdata.rc, sizeof(testdata.rc)));
		close(pipedata[0]);
		close(pipedata[1]);
		exit(0);
	}
	omrthread_lib_postForkParent();

	/* After fork, notify the sibling thread and wait for it to unlock the global mutex. */
	ssize_t readBytes = read(pipedata[0], (int *)&testdata.rc, sizeof(testdata.rc));
	EXPECT_TRUE(readBytes == sizeof(testdata.rc)) << "Didn't read back enough from pipe";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in child process.";
	close(pipedata[0]);
	close(pipedata[1]);
}

/**
 * Note: All tests are disabled until issues with hangs related to undefined behavior are solved in
 * omrthread_lib_post_fork_reset.
 *
 * The grandchild process test is to test forking in a child process after a fork.
 * The all tests are run again, but with a second call to fork.
 */
TEST(ThreadForkResetTest, GrandchildFork)
{
	waitNotifyStart(2);
	waitNotify2Start(2);
	attachDetachStart(2);
}
