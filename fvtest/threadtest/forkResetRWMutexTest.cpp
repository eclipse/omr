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
#include "thread_api.h"
#include "threadTestHelp.h"

#define START_DELAY (1 * 1000)
#define TIMEOUT (60 * 1000)

typedef struct testdata_rw_t {
	omrthread_monitor_t monitor;
	omrthread_rwmutex_t rwmutex;
	volatile intptr_t rc;
} testdata_rw_t;

static intptr_t createJoinableThreadNoMacros(omrthread_t *newThread, omrthread_entrypoint_t entryProc, void *entryArg);
static void childForkHelper(int forkCount);
static void siblingWriteStart(int forkCount);
static void siblingWriteChild(testdata_rw_t *, int[2]);
static int siblingWriteMain(void *arg);
static void parentWriteStart(int forkCount);
static void parentWriteChild(testdata_rw_t *, int[2]);
static void childSiblingWriteStart(int forkCount);
static void childSiblingWriteChild(testdata_rw_t *, int[2]);
static int childSiblingWriteMain(void *arg);
static void parentWriteSiblingReadStart(int forkCount);
static void parentWriteSiblingReadChild(testdata_rw_t *, int[2]);
static int  parentWriteSiblingReadMain(void *arg);

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

TEST(ThreadForkResetTest, SiblingWriteLock)
{
	siblingWriteStart(1);
}

static void
siblingWriteStart(int forkCount)
{
	omrthread_t t;
	testdata_rw_t testdata;
	int pipedata[2];

	/* Pre-fork */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	omrthread_monitor_init(&testdata.monitor, 0);
	omrthread_rwmutex_init(&testdata.rwmutex, 0, "OMR_TEST SiblingWriteLock");
	omrthread_monitor_enter(testdata.monitor);
	ASSERT_NO_FATAL_FAILURE(createJoinableThread(&t, siblingWriteMain, &testdata));

	EXPECT_FALSE(J9THREAD_TIMED_OUT == omrthread_monitor_wait_timed(testdata.monitor, TIMEOUT, 0))
			<< "Timed out waiting for sibling thread to enter monitor.";

	omrthread_lib_preFork();
	if (0 >= forkCount || 0 == fork()) {
		childForkHelper(forkCount - 1);
		/* Child process post-fork */
		siblingWriteChild(&testdata, pipedata);
	}
	omrthread_lib_postForkParent();

	/* Parent process post-fork */
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
	EXPECT_TRUE(0 == omrthread_rwmutex_destroy(testdata.rwmutex))
			<< "Destroying rwmutex failed: monitor still in use.";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in sibling thread.";
}

static void
siblingWriteChild(testdata_rw_t *testdata, int pipedata[2])
{
	omrthread_lib_postForkChild();
	testdata->rc = omrthread_rwmutex_try_enter_write(testdata->rwmutex);
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_write(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_destroy(testdata->rwmutex);
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
siblingWriteMain(void *arg)
{
	testdata_rw_t *testdata = (testdata_rw_t *)arg;
	testdata->rc = omrthread_monitor_enter(testdata->monitor);
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_try_enter_write(testdata->rwmutex);
	}
	/* Notify write lock is complete. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify(testdata->monitor);
	}
	/* Wait until after fork. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_wait_timed(testdata->monitor, TIMEOUT, 0);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_write(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor);
	}

	return 0;
}

TEST(ThreadForkResetTest, ParentWriteLock)
{
	parentWriteStart(1);
}

static void
parentWriteStart(int forkCount)
{
	testdata_rw_t testdata;
	int pipedata[2];

	/* Pre-fork */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	testdata.rc = omrthread_rwmutex_init(&testdata.rwmutex, 0, "OMR_TEST ParentWriteLock");
	EXPECT_TRUE(0 == omrthread_rwmutex_try_enter_write(testdata.rwmutex))
			<< "Enter write rwmutex failed before fork.";

	omrthread_lib_preFork();
	if (0 >= forkCount || 0 == fork()) {
		childForkHelper(forkCount - 1);
		/* Child process post-fork */
		parentWriteChild(&testdata, pipedata);
	}
	omrthread_lib_postForkParent();

	/* Parent process post-fork */
	ssize_t readBytes = read(pipedata[0], (int *)&testdata.rc, sizeof(testdata.rc));
	EXPECT_TRUE(readBytes == sizeof(testdata.rc)) << "Didn't read back enough from pipe";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in child process.";
	close(pipedata[0]);
	close(pipedata[1]);
	EXPECT_TRUE(0 == omrthread_rwmutex_exit_write(testdata.rwmutex))
			<< "Exit rwmutex failed after fork: monitor still in use.";
	EXPECT_TRUE(0 == omrthread_rwmutex_destroy(testdata.rwmutex))
			<< "Destroying monitor failed: monitor still in use.";
}

static void
parentWriteChild(testdata_rw_t *testdata, int pipedata[2])
{
	omrthread_lib_postForkChild();
	testdata->rc = omrthread_rwmutex_try_enter_write(testdata->rwmutex);
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_write(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_write(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_destroy(testdata->rwmutex);
	}
	J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&testdata->rc, sizeof(testdata->rc)));
	close(pipedata[0]);
	close(pipedata[1]);
	exit(0);
}

TEST(ThreadForkResetTest, ChildSiblingWriteLock)
{
	childSiblingWriteStart(1);
}

static void
childSiblingWriteStart(int forkCount)
{
	testdata_rw_t testdata;
	int pipedata[2];

	/* Pre-fork */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	testdata.rc = omrthread_rwmutex_init(&testdata.rwmutex, 0, "OMR_TEST ParentWriteLock");
	EXPECT_TRUE(0 == omrthread_rwmutex_try_enter_write(testdata.rwmutex))
			<< "Enter write rwmutex failed before fork.";

	omrthread_lib_preFork();
	if (0 >= forkCount || 0 == fork()) {
		childForkHelper(forkCount - 1);
		/* Child process post-fork */
		childSiblingWriteChild(&testdata, pipedata);
	}
	omrthread_lib_postForkParent();

	/* Parent process post-fork */
	ssize_t readBytes = read(pipedata[0], (int *)&testdata.rc, sizeof(testdata.rc));
	EXPECT_TRUE(readBytes == sizeof(testdata.rc)) << "Didn't read back enough from pipe";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in child process.";
	close(pipedata[0]);
	close(pipedata[1]);
	EXPECT_TRUE(0 == omrthread_rwmutex_exit_write(testdata.rwmutex))
			<< "Exit rwmutex failed after fork: monitor still in use.";
	EXPECT_TRUE(0 == omrthread_rwmutex_destroy(testdata.rwmutex))
			<< "Destroying monitor failed: monitor still in use.";
}

static void
childSiblingWriteChild(testdata_rw_t *testdata, int pipedata[2])
{
	omrthread_t t = NULL;
	omrthread_lib_postForkChild();
	testdata->rc = createJoinableThreadNoMacros(&t, childSiblingWriteMain, testdata);
	if (NULL != t) {
		testdata->rc = omrthread_join(t);
		if (testdata->rc & J9THREAD_ERR_OS_ERRNO_SET) {
			printf("omrthread_join() returned 0x%08x and os_errno=%d\n", (int)testdata->rc, (int)omrthread_get_os_errno());
		}
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_write(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_destroy(testdata->rwmutex);
	}
	J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&testdata->rc, sizeof(testdata->rc)));
	close(pipedata[0]);
	close(pipedata[1]);
	exit(0);
}

static int
childSiblingWriteMain(void *arg)
{
	testdata_rw_t *testdata = (testdata_rw_t *)arg;

	testdata->rc = omrthread_rwmutex_try_enter_write(testdata->rwmutex);
	if (J9THREAD_RWMUTEX_WOULDBLOCK == testdata->rc) {
		testdata->rc = 0;
	} else {
		omrthread_rwmutex_exit_write(testdata->rwmutex);
		testdata->rc = 1;
	}

	return 0;
}

/*
 * This test seems less than ideal with the sleeps involved.
 * Perhaps there is a better way..
 */
TEST(ThreadForkResetTest, ParentWriteSiblingReadLock)
{
	parentWriteSiblingReadStart(1);
}

static void
parentWriteSiblingReadStart(int forkCount)
{
	omrthread_t t;
	testdata_rw_t testdata;
	int pipedata[2];

	/* Pre-fork */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";

	omrthread_monitor_init(&testdata.monitor, 0);
	omrthread_rwmutex_init(&testdata.rwmutex, 0, "OMR_TEST ParentWriteSiblingReadLock");
	omrthread_monitor_enter(testdata.monitor);
	omrthread_rwmutex_enter_write(testdata.rwmutex);
	ASSERT_NO_FATAL_FAILURE(createJoinableThread(&t, parentWriteSiblingReadMain, &testdata));

	EXPECT_FALSE(J9THREAD_TIMED_OUT == omrthread_monitor_wait_timed(testdata.monitor, TIMEOUT, 0))
			<< "Timed out waiting for sibling thread to enter monitor.";
	EXPECT_TRUE(0 == omrthread_monitor_exit(testdata.monitor))
			<< "Exiting monitor after fork failed: parent thread does not own monitor.";
	EXPECT_TRUE(0 == omrthread_monitor_destroy(testdata.monitor))
			<< "Destroying monitor failed: monitor still in use.";
	omrthread_sleep(START_DELAY);

	omrthread_lib_preFork();
	if (0 >= forkCount || 0 == fork()) {
		childForkHelper(forkCount - 1);
		/* Child process post-fork */
		parentWriteSiblingReadChild(&testdata, pipedata);
	}
	omrthread_lib_postForkParent();

	/* Parent process post-fork */
	ssize_t readBytes = read(pipedata[0], (int *)&testdata.rc, sizeof(testdata.rc));
	EXPECT_TRUE(readBytes == sizeof(testdata.rc)) << "Didn't read back enough from pipe";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in child process.";
	close(pipedata[0]);
	close(pipedata[1]);
	EXPECT_TRUE(0 == omrthread_rwmutex_exit_write(testdata.rwmutex))
			<< "Exit write failed after fork.";

	VERBOSE_JOIN(t, J9THREAD_SUCCESS);

	EXPECT_TRUE(0 == omrthread_rwmutex_destroy(testdata.rwmutex))
			<< "Destroying rwmutex failed: monitor still in use.";
	EXPECT_TRUE(0 == testdata.rc) << "Failure occurred in sibling thread.";
}

static void
parentWriteSiblingReadChild(testdata_rw_t *testdata, int pipedata[2])
{
	omrthread_lib_postForkChild();
	testdata->rc = omrthread_rwmutex_exit_write(testdata->rwmutex);
	omrthread_sleep(START_DELAY);
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_try_enter_write(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_write(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_enter_read(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_read(testdata->rwmutex);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_destroy(testdata->rwmutex);
	}
	J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&testdata->rc, sizeof(testdata->rc)));
	close(pipedata[0]);
	close(pipedata[1]);
	exit(0);
}

static int
parentWriteSiblingReadMain(void *arg)
{
	testdata_rw_t *testdata = (testdata_rw_t *)arg;
	testdata->rc = omrthread_monitor_enter(testdata->monitor);
	/* Notify write lock is complete. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_notify(testdata->monitor);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_monitor_exit(testdata->monitor);
	}
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_enter_read(testdata->rwmutex);
	}
	/* Wait until after fork. */
	if (0 == testdata->rc) {
		testdata->rc = omrthread_rwmutex_exit_read(testdata->rwmutex);
	}

	return 0;
}

TEST(ThreadForkResetTest, GrandchildRWLock)
{
	siblingWriteStart(2);
	//siblingReadStart(2);
	parentWriteStart(2);
	childSiblingWriteStart(2);
	parentWriteSiblingReadStart(2);
}

static intptr_t
createJoinableThreadNoMacros(omrthread_t *newThread, omrthread_entrypoint_t entryProc, void *entryArg)
{
	omrthread_attr_t attr = NULL;
	intptr_t rc = 0;

	rc = omrthread_attr_init(&attr);
	if (J9THREAD_SUCCESS == rc) {
		rc = omrthread_attr_set_detachstate(&attr, J9THREAD_CREATE_JOINABLE);
	}
	if (J9THREAD_SUCCESS == rc) {
		rc = omrthread_create_ex(newThread, &attr, 0, entryProc, entryArg);
		if (J9THREAD_SUCCESS != rc) {
			if (rc & J9THREAD_ERR_OS_ERRNO_SET) {
				printf("omrthread_create_ex() returned 0x%08x and os_errno=%d\n", (int)rc, (int)omrthread_get_os_errno());
			}
		}
	}
	if (NULL != attr) {
		rc = omrthread_attr_destroy(&attr);
	}
	return rc;
}
