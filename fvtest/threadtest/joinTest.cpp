/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#include "omrTest.h"
#include "thread_api.h"
#include "threadTestHelp.h"

typedef struct JoinThreadHelperData {
	omrthread_t threadToJoin;
	intptr_t expectedRc;
} JoinThreadHelperData;

static void createThread(omrthread_t *newThread, uintptr_t suspend, omrthread_detachstate_t detachstate,
						 omrthread_entrypoint_t entryProc, void *entryArg);
static int J9THREAD_PROC doNothingHelper(void *entryArg);
static int J9THREAD_PROC joinThreadHelper(void *entryArg);

TEST(JoinTest, joinSelf)
{
	ASSERT_EQ(J9THREAD_INVALID_ARGUMENT, omrthread_join(omrthread_self()));
}

TEST(JoinTest, joinNull)
{
	ASSERT_EQ(J9THREAD_INVALID_ARGUMENT, omrthread_join(NULL));
}

TEST(JoinTest, createDetachedThread)
{
	omrthread_t helperThr = NULL;

	/* We can't pass any local data to the child thread because we don't guarantee that
	 * it won't go out of scope before the child thread uses it.
	 */
	ASSERT_NO_FATAL_FAILURE(createThread(&helperThr, FALSE, J9THREAD_CREATE_DETACHED, doNothingHelper, NULL));
}

TEST(JoinTest, createJoinableThread)
{
	JoinThreadHelperData helperData;
	helperData.threadToJoin = NULL;
	helperData.expectedRc = J9THREAD_INVALID_ARGUMENT;

	omrthread_t helperThr = NULL;
	ASSERT_NO_FATAL_FAILURE(createThread(&helperThr, FALSE, J9THREAD_CREATE_JOINABLE, joinThreadHelper, (void *)&helperData));

	/* .. and joins the new thread */
	VERBOSE_JOIN(helperThr, J9THREAD_SUCCESS);
}

TEST(JoinTest, joinDetachedThread)
{
	JoinThreadHelperData helperData;

	/* we can guarantee this thread is live, so we won't crash in omrthread_join() */
	helperData.threadToJoin = omrthread_self();
	helperData.expectedRc = J9THREAD_INVALID_ARGUMENT;

	omrthread_t helperThr = NULL;
	ASSERT_NO_FATAL_FAILURE(createThread(&helperThr, FALSE, J9THREAD_CREATE_JOINABLE, joinThreadHelper, (void *)&helperData));

	VERBOSE_JOIN(helperThr, J9THREAD_SUCCESS);
}

TEST(JoinTest, joinExitedThread)
{
	omrthread_t helperThr = NULL;

	ASSERT_NO_FATAL_FAILURE(createThread(&helperThr, FALSE, J9THREAD_CREATE_JOINABLE, doNothingHelper, NULL));

	/* hopefully wait long enough for the child thread to exit */
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_sleep(3000));

	VERBOSE_JOIN(helperThr, J9THREAD_SUCCESS);
}

TEST(JoinTest, joinLiveThread)
{
	omrthread_t suspendedThr = NULL;
	omrthread_t joinHelperThr = NULL;
	JoinThreadHelperData helperData;

	/* suspend the child thread when it starts */
	ASSERT_NO_FATAL_FAILURE(createThread(&suspendedThr, TRUE, J9THREAD_CREATE_JOINABLE, doNothingHelper, NULL));

	/* create a thread to join on the suspended thread */
	helperData.threadToJoin = suspendedThr;
	helperData.expectedRc = J9THREAD_SUCCESS;
	ASSERT_NO_FATAL_FAILURE(createThread(&joinHelperThr, FALSE, J9THREAD_CREATE_JOINABLE, joinThreadHelper, &helperData));

	/* hopefully wait long enough for the join helper thread to start joining */
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_sleep(3000));

	/* resume the child thread */
	printf("resuming suspendedThr\n");
	fflush(stdout);
	ASSERT_EQ(1, omrthread_resume(suspendedThr));

	VERBOSE_JOIN(joinHelperThr, J9THREAD_SUCCESS);
}

TEST(JoinTest, joinCanceledThread)
{
	omrthread_t threadToJoin = NULL;

	/* create a suspended thread */
	ASSERT_NO_FATAL_FAILURE(createThread(&threadToJoin, TRUE, J9THREAD_CREATE_JOINABLE, doNothingHelper, NULL));

	/* cancel the suspended thread */
	omrthread_cancel(threadToJoin);

	/* join the suspended + canceled thread */
	VERBOSE_JOIN(threadToJoin, J9THREAD_SUCCESS);
}

static void
createThread(omrthread_t *newThread, uintptr_t suspend, omrthread_detachstate_t detachstate,
			 omrthread_entrypoint_t entryProc, void *entryArg)
{
	omrthread_attr_t attr = NULL;
	intptr_t rc = 0;

	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_init(&attr));
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_set_detachstate(&attr, detachstate));
	EXPECT_EQ(J9THREAD_SUCCESS,
			  rc = omrthread_create_ex(newThread, &attr, suspend, entryProc, entryArg));
	if (rc & J9THREAD_ERR_OS_ERRNO_SET) {
		printf("omrthread_create_ex() returned os_errno=%d\n", (int)omrthread_get_os_errno());
	}
	ASSERT_EQ(J9THREAD_SUCCESS, omrthread_attr_destroy(&attr));
}

static int J9THREAD_PROC
doNothingHelper(void *entryArg)
{
	printf("doNothingHelper() is exiting\n");
	fflush(stdout);
	return 0;
}

static int J9THREAD_PROC
joinThreadHelper(void *entryArg)
{
	JoinThreadHelperData *helperData = (JoinThreadHelperData *)entryArg;
	printf("joinThreadHelper() is invoking omrthread_join()\n");
	fflush(stdout);
	VERBOSE_JOIN(helperData->threadToJoin, helperData->expectedRc);
	return 0;
}
