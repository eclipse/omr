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

#include <stdlib.h>
#include <string.h>

#include "omrport.h"
#include "omrthread.h"
#include "omr.h"
#include "omrrasinit.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "omrtrace.h"
#include "omrvm.h"
#include "ut_omr_test.h"

#include "rasTestHelpers.hpp"

#define TIMEOUT 10
#define NUM_THREADS 10
#define STRESS_ITERATIONS_BUFFER 1000
#define STRESS_ITERATIONS_THREAD 50

/*
 * The executable for this test is omrsubscribertest
 */

typedef struct OMRTraceSubscriberForkTestResult {
	omr_error_t rc;
	const char *failureFile;
	int failureLine;
} OMRTraceSubscriberForkTestResult;

typedef struct TestData {
	OMRTestVM *testVM;
	omrthread_monitor_t readyCond;
	int32_t threadCount;
	int32_t waitingThreadCount;
	BOOLEAN forkReady;
	omr_error_t *siblingRc;
	omrthread_t *threads;
} TestData;

typedef struct TestSiblingThreadData {
	TestData *testData;
	omr_error_t *threadRc;
} TestSiblingThreadData;

static void runTest(omrthread_entrypoint_t entrypoint);
static void preforkSetup(OMR_VMThread **vmthread, struct OMR_Agent **agent, OMRTestVM *testVM, int pipedata[2]);
static void postForkChild(OMR_VMThread *vmthread, struct OMR_Agent *agent, OMRTestVM *testVM, int pipedata[2]);
static void postForkParent(OMR_VMThread *vmthread, struct OMR_Agent *agent, OMRTestVM *testVM, int pipedata[2]);
static int duringForkBufferTest(void *entryArg);
static int duringForkThreadTest(void *entryArg);
static int doNothing(void *entryArg);
static omr_error_t setupTestData(TestData *testData, OMRTestVM *testVM, int32_t threadCount);
static omr_error_t waitForThreadsReady(TestData *testData);
static omr_error_t waitForThreadsDone(TestData *testData);
static omr_error_t notifyThreadReadyAndWait(TestSiblingThreadData *threadData);
static void recordChildErrorStatus(OMRTraceSubscriberForkTestResult *result, omr_error_t rc, const char *filename, int lineno);

TEST(RASSubscriberForkTest, SubscriberAgentForkTest)
{
	runTest(NULL);
}

/* This test is currently disabled as it performs unsafe/unsupported operations.
 * createThread's allocation of omrthread_attr_t's concurrently to fork is
 * unsupported and may result in undefined behavior. It is a suspect in hangs
 * experienced in dlclose() when unloading the agenet at the end of the test.
 */
TEST(DISABLED_RASSubscriberForkTest, SubscriberAgentForkThreadTest)
{
	runTest(duringForkThreadTest);
}

TEST(RASSubscriberForkTest, SubscriberAgentForkBufferTest)
{
	runTest(duringForkBufferTest);
}

/* Create a group of test threads which will test one of several conditions
 * concurrently to a fork call.
 */
static void
runTest(omrthread_entrypoint_t entrypoint)
{
	/* OMR VM data structures */
	int pipedata[2];
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;
	struct OMR_Agent *agent = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());

	/* Initialize omr vm, trace, and an agent. */
	preforkSetup(&vmthread, &agent, &testVM, pipedata);

	/* Setup new thread data to pass to new threads. */
	TestData newTestData;
	TestSiblingThreadData threadData[NUM_THREADS];
	if (NULL != entrypoint) {
		OMRTEST_ASSERT_ERROR_NONE(setupTestData(&newTestData, &testVM, NUM_THREADS));

		/* Create threads which will perform stress tests concurrently to fork. */
		for (int32_t i = 0; i < NUM_THREADS; i += 1) {
			threadData[i].testData = &newTestData;
			threadData[i].threadRc = &newTestData.siblingRc[i];
			ASSERT_NO_FATAL_FAILURE(createThread(&newTestData.threads[i], FALSE, J9THREAD_CREATE_JOINABLE, entrypoint, &threadData[i]));
		}

		/* Wait for all test threads to be ready before forking. */
		OMRTEST_ASSERT_ERROR_NONE(waitForThreadsReady(&newTestData));
	}

	omr_vm_preFork(&testVM.omrVM);
	/* Fire trace points and unload the agent post fork, in both child and parent. */
	if (0 == fork()) {
		omr_vm_postForkChild(&testVM.omrVM);
		if (NULL != entrypoint) {
			omrmem_free_memory(newTestData.siblingRc);
			omrmem_free_memory(newTestData.threads);
		}
		postForkChild(vmthread, agent, &testVM, pipedata);
	} else {
		omr_vm_postForkParent(&testVM.omrVM);
		if (NULL != entrypoint) {
			OMRTEST_ASSERT_ERROR_NONE(waitForThreadsDone(&newTestData));
		}
		postForkParent(vmthread, agent, &testVM, pipedata);
	}
}

/* Setup the omr vm and load an agent pre fork. */
static void
preforkSetup(OMR_VMThread **vmthread, struct OMR_Agent **agent, OMRTestVM *testVM, int pipedata[2])
{
	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = NULL;
	const char *trcOpts = "maximal=all:buffers=1k";

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(testVM, OMRPORTLIB));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM->omrVM, NULL, vmthread, "subscriberForkTest"));

	datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM->omrVM, trcOpts, datDir));
	OMRTEST_ASSERT_ERROR_NONE(omr_trc_startThreadTrace(*vmthread, "initialization thread"));

	/* load agent */
	*agent = omr_agent_create(&testVM->omrVM, "subscriberAgent");
	ASSERT_FALSE(NULL == *agent) << "testAgent: createAgent() subscriberAgent failed";
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(*agent));
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(*agent));

	/*  Initialise the omr_test module for tracing */
	UT_OMR_TEST_MODULE_LOADED(testVM->omrVM._trcEngine->utIntf);

	/* Initialize pipe before fork. */
	int pipeRC = pipe(pipedata);
	EXPECT_TRUE(0 == pipeRC)  << "Failure occurred calling pipe";
}

/* Test unloading an agent, loading an agent, and trace points in the
 * post fork child process.
 */
void
postForkChild(OMR_VMThread *vmthread, struct OMR_Agent *agent, OMRTestVM *testVM, int pipedata[2])
{
	OMRTraceSubscriberForkTestResult result;
	omr_error_t rc = OMR_ERROR_NONE;
	result.rc = OMR_ERROR_NONE;

	/*  Fire some trace points! */
	Trc_OMR_Test_Init();
	Trc_OMR_Test_Ptr(vmthread, vmthread);
	Trc_OMR_Test_Int(vmthread, 10);
	Trc_OMR_Test_Int(vmthread, 99);
	Trc_OMR_Test_ManyParms(vmthread, "Test subscriberForkAgent!", vmthread, 10);

	/*  Unload the agent */
	rc = omr_agent_callOnUnload(agent);
	recordChildErrorStatus(&result, rc, __FILE__, __LINE__);

	if (OMR_ERROR_NONE == rc) {
		/* Destroy first agent */
		omr_agent_destroy(agent);

		/* Load new agent. */
		agent = omr_agent_create(&testVM->omrVM, "subscriberAgentWithJ9thread");
		if (NULL == agent) {
			rc = OMR_ERROR_INTERNAL;
			recordChildErrorStatus(&result, rc, __FILE__, __LINE__);
		}
	}
	if (OMR_ERROR_NONE == rc) {
		rc = omr_agent_openLibrary(agent);
		recordChildErrorStatus(&result, rc, __FILE__, __LINE__);
	}
	if (OMR_ERROR_NONE == rc) {
		rc = omr_agent_callOnLoad(agent);
		recordChildErrorStatus(&result, rc, __FILE__, __LINE__);
	}
	if (OMR_ERROR_NONE == rc) {
		/*  Fire some trace points! */
		Trc_OMR_Test_Init();
		Trc_OMR_Test_Ptr(vmthread, vmthread);
		Trc_OMR_Test_Int(vmthread, 20);
		Trc_OMR_Test_Int(vmthread, 109);
		Trc_OMR_Test_ManyParms(vmthread, "Test SubscriberAgentWithJ9thread!", vmthread, 10);

		/*  Unload the agent */
		rc = omr_agent_callOnUnload(agent);
		recordChildErrorStatus(&result, rc, __FILE__, __LINE__);
	}
	if (OMR_ERROR_NONE == rc) {
		omr_agent_destroy(agent);
		rc = omr_ras_cleanupTraceEngine(vmthread);
		recordChildErrorStatus(&result, rc, __FILE__, __LINE__);
	}
	if (OMR_ERROR_NONE == rc) {
		rc = OMR_Thread_Free(vmthread);
		recordChildErrorStatus(&result, rc, __FILE__, __LINE__);
	}
	if (OMR_ERROR_NONE == rc) {
		/*  Now clear up the VM we started for this test case. */
		rc = omrTestVMFini(testVM);
		recordChildErrorStatus(&result, rc, __FILE__, __LINE__);
	}

	/* Write result to pipe to notify parent. */
	J9_IGNORE_RETURNVAL(write(pipedata[1], (int *)&result, sizeof(result)));
	close(pipedata[0]);
	close(pipedata[1]);
	exit(0);
}

/* Fire some trace points and clean up everything in the parent after a fork. */
void
postForkParent(OMR_VMThread *vmthread, struct OMR_Agent *agent, OMRTestVM *testVM, int pipedata[2])
{
	/*  Fire some trace points! */
	Trc_OMR_Test_Init();
	Trc_OMR_Test_Ptr(vmthread, vmthread);
	Trc_OMR_Test_Int(vmthread, 10);
	Trc_OMR_Test_Int(vmthread, 99);
	Trc_OMR_Test_ManyParms(vmthread, "Test subscriberForkAgent!", vmthread, 10);

	/*  Unload the agent */
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);

	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));

	/*  Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(testVM));

	/* Wait on pipe for child process to complete. */
	OMRTraceSubscriberForkTestResult result;
	fd_set set;
	struct timeval timeout;

	FD_ZERO(&set);
	FD_SET(pipedata[0], &set);
	timeout.tv_sec = TIMEOUT;
	timeout.tv_usec = 0;
	int32_t rc = select(pipedata[0] + 1, &set, NULL, NULL, &timeout);
	EXPECT_FALSE(0 == rc) << "Timed out waiting for child process; possible deadlock.";

	if (0 != rc) {
		ssize_t readBytes = read(pipedata[0], (int *)&result, sizeof(result));
		EXPECT_TRUE(readBytes == sizeof(result)) << "Didn't read back enough from pipe";
		EXPECT_TRUE(0 == result.rc) << "Failure occurred in child process at " << result.failureFile << ":" << result.failureLine;
	}
	close(pipedata[0]);
	close(pipedata[1]);
}

/* Stress repeatedly firing trace points during fork. */
static int
duringForkBufferTest(void *entryArg)
{
	/* Get the thread data which was passed to this thread. */
	omr_error_t siblingRc = OMR_ERROR_NONE;
	TestSiblingThreadData *threadData = (TestSiblingThreadData *)entryArg;
	OMRTestVM *testVM = threadData->testData->testVM;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	/* Init trace for this thread. */
	OMR_VMThread *vmthread = NULL;
	siblingRc = OMRTEST_PRINT_ERROR(OMR_Thread_Init(&testVM->omrVM, NULL, &vmthread, "traceTestBuffer-child"));
	if (OMR_ERROR_NONE == siblingRc) {
		siblingRc = OMRTEST_PRINT_ERROR(notifyThreadReadyAndWait(threadData));
	}
	if (OMR_ERROR_NONE == siblingRc) {
		/* Test firing trace points concurrently to fork. */
		for (int32_t i = 0; i < STRESS_ITERATIONS_BUFFER; i += 1) {
			Trc_OMR_Test_Init();
		}
		siblingRc = OMRTEST_PRINT_ERROR(OMR_Thread_Free(vmthread));
	}

	if (OMR_ERROR_NONE != siblingRc) {
		*threadData->threadRc = siblingRc;
	}
	return 0;
}

/* Stress repeatedly creating threads during fork. */
static int
duringForkThreadTest(void *entryArg)
{
	/* Get the thread data which was passed to this thread. */
	omr_error_t siblingRc = OMR_ERROR_NONE;
	TestSiblingThreadData *threadData = (TestSiblingThreadData *)entryArg;
	OMRTestVM *testVM = threadData->testData->testVM;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	/* Setup new thread data to pass to new threads. */
	TestData newTestData;
	siblingRc = setupTestData(&newTestData, testVM, STRESS_ITERATIONS_THREAD);

	if (OMR_ERROR_NONE == siblingRc) {
		siblingRc = OMRTEST_PRINT_ERROR(notifyThreadReadyAndWait(threadData));
	}
	TestSiblingThreadData testThreadData[STRESS_ITERATIONS_THREAD];
	if (OMR_ERROR_NONE == siblingRc) {
		/* Create threads concurrently to fork. */
		for (int32_t i = 0; i < STRESS_ITERATIONS_THREAD; i += 1) {
			testThreadData[i].testData = &newTestData;
			testThreadData[i].threadRc = &newTestData.siblingRc[i];
			createThread(&newTestData.threads[i], FALSE, J9THREAD_CREATE_JOINABLE, doNothing, &testThreadData[i]);
			if (::testing::Test::HasFatalFailure()) {
				siblingRc = OMR_ERROR_INTERNAL;
				break;
			}
		}
	}
	if (OMR_ERROR_NONE == siblingRc) {
		/* Wait for all test threads to terminate. */
		siblingRc = OMRTEST_PRINT_ERROR(waitForThreadsDone(&newTestData));
	}

	if (OMR_ERROR_NONE != siblingRc) {
		*threadData->threadRc = siblingRc;
	}
	return 0;
}

/* Notify that the thread has complete and terminate.
 * This is used for the creating threads during fork test.
 */
static int
doNothing(void *entryArg)
{
	TestSiblingThreadData *threadData = (TestSiblingThreadData *)entryArg;
	OMRTestVM *testVM = threadData->testData->testVM;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);
	omr_error_t siblingRc = OMR_ERROR_NONE;

	OMR_VMThread *vmthread = NULL;
	siblingRc = OMRTEST_PRINT_ERROR(OMR_Thread_Init(&testVM->omrVM, NULL, &vmthread, "traceTestThread-child"));
	if (OMR_ERROR_NONE == siblingRc) {
		omrthread_sleep(200);
		siblingRc = OMRTEST_PRINT_ERROR(OMR_Thread_Free(vmthread));
	}

	*threadData->threadRc = siblingRc;
	return 0;
}

/* Setup test thread data and a monitor for a group of test threads. */
static omr_error_t
setupTestData(TestData *testData, OMRTestVM *testVM, int32_t threadCount)
{
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);
	omr_error_t rc = OMR_ERROR_NONE;

	/* Setup new thread data to pass to new threads. */
	testData->testVM = testVM;
	testData->threadCount = threadCount;
	testData->waitingThreadCount = threadCount;
	testData->forkReady = FALSE;
	testData->siblingRc = (omr_error_t *)omrmem_allocate_memory(sizeof(omr_error_t) * threadCount, OMRMEM_CATEGORY_VM);
	testData->threads = (omrthread_t *)omrmem_allocate_memory(sizeof(omrthread_t) * threadCount, OMRMEM_CATEGORY_VM);
	rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(omrthread_monitor_init_with_name(&testData->readyCond, 0, "traceTestShutdown"), J9THREAD_SUCCESS);
	for (int32_t i = 0; i < threadCount; i += 1) {
		testData->siblingRc[i] = OMR_ERROR_NONE;
	}

	return rc;
}

/* Wait for all test threads to be ready to begin testing. The main thread
 * calls this before fork.
 */
static omr_error_t
waitForThreadsReady(TestData *testData)
{
	OMRPORT_ACCESS_FROM_OMRPORT(testData->testVM->portLibrary);
	omr_error_t rc = OMR_ERROR_NONE;

	/* Wait for all test threads to be ready to begin testing. */
	omrthread_monitor_enter(testData->readyCond);
	while ((0 < testData->waitingThreadCount) && (OMR_ERROR_NONE == rc)) {
		rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(omrthread_monitor_wait_timed(testData->readyCond, TIMEOUT * 1000, 0), 0);
	}
	/* It will not get here until all test threads are ready and waiting to fork. */
	testData->forkReady = TRUE;
	omrthread_monitor_notify_all(testData->readyCond);
	omrthread_monitor_exit(testData->readyCond);
	return rc;
}

/* Wait for all test threads to terminate. The main thread calls this after
 * fork in the parent process.
 */
static omr_error_t
waitForThreadsDone(TestData *testData)
{
	OMRPORT_ACCESS_FROM_OMRPORT(testData->testVM->portLibrary);
	omr_error_t rc = OMR_ERROR_NONE;

	for (int32_t i = 0; i < testData->threadCount; i += 1) {
		rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(omrthread_join(testData->threads[i]), J9THREAD_SUCCESS);

		if (OMR_ERROR_NONE != testData->siblingRc[i]) {
			rc = testData->siblingRc[i];
			omrtty_printf("%s:%d ERROR: Failure occurred in thread %d.\n", __FILE__, __LINE__, i);
		}
	}

	omrmem_free_memory(testData->siblingRc);
	omrmem_free_memory(testData->threads);
	return rc;
}

/* Notify the main thread that this test thread is ready to begin testing.
 * This test thread then waits for the main thread to notify it that it is
 * about to fork.
 */
static omr_error_t
notifyThreadReadyAndWait(TestSiblingThreadData *threadData)
{
	TestData *testData = threadData->testData;
	OMRPORT_ACCESS_FROM_OMRPORT(testData->testVM->portLibrary);
	omr_error_t rc = OMR_ERROR_NONE;

	omrthread_monitor_enter(testData->readyCond);
	omrthread_monitor_notify_all(testData->readyCond);
	testData->waitingThreadCount -= 1;
	/* It will not pass this until all test threads get here and the main
	 * thread indicates that it is ready to fork.
	 */
	do {
		rc = OMRTEST_PRINT_UNEXPECTED_INT_RC(omrthread_monitor_wait_timed(testData->readyCond, TIMEOUT * 1000, 0), 0);
	} while ((!testData->forkReady) && (OMR_ERROR_NONE == rc));
	omrthread_monitor_exit(testData->readyCond);
	return rc;
}

/* Record the error code, file name, and line number for errors in the child
 * process to report back to the parent process via a pipe.
 */
static void
recordChildErrorStatus(OMRTraceSubscriberForkTestResult *result, omr_error_t rc, const char *filename, int lineno)
{
	if (OMR_ERROR_NONE != rc) {
		result->rc = rc;
		result->failureFile = filename;
		result->failureLine = lineno;
	}
}
