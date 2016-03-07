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

#include <string.h>

#include "omrport.h"
#include "omr.h"
#include "omrrasinit.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "omrtrace.h"
#include "omrvm.h"
#include "ut_omr_test.h"

#include "AtomicSupport.hpp"
#include "rasTestHelpers.hpp"


/*
 * This test covers:
 * - Logging tracepoints concurrently from multiple threads
 * - Filling trace buffers
 * - Wrapping tracepoints across multiple trace buffers
 * - Verifies the contents of trace records sent to subscribers
 */

#define TRACE_BUFFER_BYTES 1024
#define NUM_CHILD_THREADS 4

/* Test data */
typedef struct TestChildThreadData {
	OMRTestVM *testVM;
	omr_error_t childRc;
	size_t traceDataCount;
	const char **traceData;
	omrthread_t osThread;

	int expectedLoggedCount;
	int loggedCount;
	int unloggedCount;
	PerThreadWrapBuffer wrapBuffer;
} TestChildThreadData;

typedef struct FailingSubscriberData {
	uint32_t callCount;
	uint32_t alarmCount;
} FailingSubscriberData;

static void startChildThread(OMRTestVM *testVM, omrthread_t *childThread, omrthread_entrypoint_t entryProc, TestChildThreadData *childData);
static omr_error_t waitForChildThread(OMRTestVM *testVM, omrthread_t childThread, TestChildThreadData *childData);
static int J9THREAD_PROC childThreadMain(void *entryArg);

static omr_error_t countTracepoints(UtSubscription *subscriptionID);
static omr_error_t countTracepointsIter(void *userData, const char *tpMod, const uint32_t tpModLength, const uint32_t tpId,
										const UtTraceRecord *record, uint32_t firstParameterOffset, uint32_t parameterDataLength,
										int32_t isBigEndian);
static omr_error_t failOnSecondCall(UtSubscription *subscriptionID);
static void failOnSecondCallAlarm(UtSubscription *subscriptionID);

static const char *lowercaseAlpha = "abcdefghijklmnopqrstuvwxyz";
static const char *uppercaseAlpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static const char *ibmText1[] = {
	"\"If the bringing of women - half the human race - into the center of",
	"historical inquiry poses a formidable challenge to historical",
	"scholarship, it also offers sustaining energy and a source of",
	"strength.\" Gerder Lerner, 1982",
	"",
	"At IBM women have been making contributions to the advancement of",
	"information technology for almost as long as the company has been in",
	"existence. Where many companies proudly date their affirmative action",
	"programs to the 1970s, IBM has been creating meaningful roles for",
	"female employees since the 1930s.",
	"",
	"This tradition was not the result of a happy accident. Instead, it was",
	"a deliberate and calculated initiative on the part of Thomas J.",
	"Watson, Sr., IBM's legendary leader. Watson discerned the value women",
	"could bring to the business equation, and he mandated that his company",
	"hire and train women to sell and service IBM products.",
	"",
	"Soon IBM had so many women professionals in its ranks that the company",
	"formed a Women's Education Division. Those early pioneers may not have",
	"realized it then, but block by block they laid the foundation for a",
	"tradition that lasts to this day.",
	"",
	"The tens of thousands of women who have been IBM employees since the",
	"1930s have built upon that foundation, for women now comprise more",
	"than 30 percent of the total U.S. employee population. But of even",
	"greater significance is the impact they've had on the information",
	"technology industry. That impact has no doubt been farther reaching",
	"and more enduring than even a visionary like Watson could have",
	"imagined seven decades ago."
};

static const char *ibmText2[] = {
	"Former Chairman Thomas J. Watson, Jr., said in 1957 that IBM \"is a",
	"company of human beings, not machines; personalities, not products;",
	"people, not real estate.\" That observation was true long before 1957",
	"-- and it has remained valid ever since.",
	"",
	"IBM was created, sustained and enlarged by a unique assembly of",
	"talented, able and dedicated men and women who, in a relatively few",
	"decades, built one of the world's most admired organizations. Starting",
	"with a product line of scales, clocks and punched card machines, all",
	"manner of IBM people -- from plant workers and engineers to sales",
	"representatives and scientists -- planned, devised and delivered",
	"thousands of innovations that propelled the company and its customers",
	"into the rapidly-evolving and exciting worlds of electronic data",
	"processing, information technology and e-business on demand.",
	"",
	"Ever since 1911, a global community spanning five generations of",
	"highly-skilled IBMers working in factories, laboratories, offices and",
	"cubicles in more than 100 countries have developed, produced and",
	"brought to the global marketplace an amazing array of devices,",
	"machines, programs, software and services that have transformed -- and",
	"even revolutionized -- the myriad activities of business, industry,",
	"government, education and the home.",
	"",
	"Those very special people -- individually and collectively -- who",
	"built this company from its modest beginnings to what it is today are",
	"the essence of what sets IBM apart."
};

TEST(TraceLogTest, stressTraceBufferManagement)
{
	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	struct OMR_Agent *agent = NULL;
	const OMR_TI *ti = omr_agent_getTI();

	UtSubscription *failedSubscription = NULL;
	FailingSubscriberData failData = { 0, 0 };

	omrthread_t childThread[NUM_CHILD_THREADS];
	TestChildThreadData childData[NUM_CHILD_THREADS];
	UtSubscription *subscriptionID[NUM_CHILD_THREADS];

	memset(childData, 0, sizeof(TestChildThreadData) * NUM_CHILD_THREADS);
	for (size_t i = 0; i < NUM_CHILD_THREADS; i += 1) {
		childData[i].testVM = &testVM;
		childData[i].childRc = OMR_ERROR_NONE;
		initWrapBuffer(&childData[i].wrapBuffer);
	}
	childData[0].traceDataCount = 1;
	childData[0].traceData = &lowercaseAlpha;
	childData[1].traceDataCount = 1;
	childData[1].traceData = &uppercaseAlpha;
	childData[2].traceDataCount = sizeof(ibmText1) / sizeof(ibmText1[0]);
	childData[2].traceData = ibmText1;
	childData[3].traceDataCount = sizeof(ibmText2) / sizeof(ibmText2[0]);
	childData[3].traceData = ibmText2;

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));

	/* Trace options:
	 *
	 * buffers=1k: Use small buffers to exercise buffer wrapping.
	 *
	 * maximal=!j9thr: Disable j9thr tracepoints because the trace engine uses monitors, and it is unsafe
	 * to log tracepoints from a omrthread function that manipulates monitor state. In particular, j9thr.17
	 * is fired from unblock_spinlock_threads() via omrthread_monitor_exit(omrVM->_vmThreadListMutex) in
	 * OMR_Thread_FirstInit().
	 */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:maximal=!j9thr", NULL));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "stressBufferManagement"));

	/* load traceagent */
	ASSERT_FALSE(NULL == (agent = omr_agent_create(&testVM.omrVM, "sampleSubscriber=traceLogTest.trc")));
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));

	/* Initialise the omr_test module for tracing */
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);

	OMRTEST_ASSERT_ERROR_NONE(
		ti->RegisterRecordSubscriber(vmthread, "fail", failOnSecondCall, failOnSecondCallAlarm, (void *)&failData, &failedSubscription));

	for (size_t i = 0; i < NUM_CHILD_THREADS; i += 1) {
		ASSERT_NO_FATAL_FAILURE(startChildThread(&testVM, &childThread[i], childThreadMain, &childData[i]));

		/* Create a subscription for each thread */
		OMRTEST_ASSERT_ERROR_NONE(
			ti->RegisterRecordSubscriber(vmthread, "child", countTracepoints, NULL, (void *)&childData[i], &subscriptionID[i]));
	}
	for (size_t i = 0; i < NUM_CHILD_THREADS; i += 1) {
		ASSERT_EQ(1, omrthread_resume(childThread[i]));
	}
	for (size_t i = 0; i < NUM_CHILD_THREADS; i += 1) {
		OMRTEST_ASSERT_ERROR_NONE(waitForChildThread(&testVM, childThread[i], &childData[i]));
	}
	/* All tracepoints from the child threads should have been published when they terminated */

	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);

	/* The failed subscriber should have deregistered itself */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_ILLEGAL_ARGUMENT,
						 ti->DeregisterRecordSubscriber(vmthread, failedSubscription));

	/* Unload the traceagent */
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	/* Verify counts of logged tracepoints */
	for (size_t i = 0; i < NUM_CHILD_THREADS; i += 1) {
		if (childData[i].expectedLoggedCount != childData[i].loggedCount) {
			printf("Unexpected loggedCount for thread \'%s\'\n", childData[i].traceData[0]);
		}
		ASSERT_EQ(childData[i].expectedLoggedCount, childData[i].loggedCount);
		ASSERT_EQ(0, childData[i].unloggedCount);
		freeWrapBuffer(&childData[i].wrapBuffer);
	}

	/* Verify failed subscriber call counts */
	ASSERT_EQ((uint32_t)1, failData.alarmCount);
	/* Implementation detail: The callCount is guaranteed because subscriber callbacks are invoked under mutex. */
	ASSERT_EQ((uint32_t)2, failData.callCount);
}


static void
startChildThread(OMRTestVM *testVM, omrthread_t *childThread, omrthread_entrypoint_t entryProc, TestChildThreadData *childData)
{
	ASSERT_NO_FATAL_FAILURE(createThread(childThread, TRUE, J9THREAD_CREATE_JOINABLE, entryProc, childData));
	childData->osThread = *childThread;
}

static omr_error_t
waitForChildThread(OMRTestVM *testVM, omrthread_t childThread, TestChildThreadData *childData)
{
	omr_error_t childRc = OMR_ERROR_INTERNAL;

	if (J9THREAD_SUCCESS == joinThread(childThread)) {
		childRc = childData->childRc;
	}
	return childRc;
}

static int J9THREAD_PROC
childThreadMain(void *entryArg)
{
	omr_error_t rc = OMR_ERROR_NONE;
	TestChildThreadData *childData = (TestChildThreadData *)entryArg;
	OMRTestVM *testVM = childData->testVM;
	OMR_VMThread *vmthread = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	/* Calculate # of tracepoints needed to fill (or overflow) a trace buffer */
	size_t dataLength = 0;
	for (size_t i = 0; i < childData->traceDataCount; i += 1) {
		dataLength += strlen(childData->traceData[i]);
	}
	const size_t stringsPerTraceBuffer = (TRACE_BUFFER_BYTES + dataLength - 1) / dataLength;
	/* Fill up at least 5 trace buffers */
	const size_t repeatCount = stringsPerTraceBuffer * 5;

	Trc_OMR_Test_UnloggedTracepoint(vmthread, "Trace is not yet enabled for this thread.");

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Init(&testVM->omrVM, NULL, &vmthread, "childThreadMain"));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}
	/* yield to encourage concurrency */
	omrthread_sleep(1);

	/* Fire some trace points */
	for (size_t i = 0; i < repeatCount; i += 1) {
		for (size_t j = 0; j < childData->traceDataCount; j += 1) {
			childData->expectedLoggedCount += 1;
			Trc_OMR_Test_String(vmthread, childData->traceData[j]);
			omrthread_yield();
		}
	}

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Free(vmthread));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}

	Trc_OMR_Test_UnloggedTracepoint(vmthread, "Trace has been disabled for this thread.");
	return 0;
}

/*
 * Callback invoked per tracepoint in a tracepoint buffer
 */
static omr_error_t
countTracepointsIter(void *userData, const char *tpMod, const uint32_t tpModLength, const uint32_t tpId,
					 const UtTraceRecord *record, uint32_t firstParameterOffset, uint32_t parameterDataLength, int32_t isBigEndian)
{
	TestChildThreadData *childData = (TestChildThreadData *)userData;
	const uint32_t omr_test_len = sizeof("omr_test") - 1;

	if ((omr_test_len == tpModLength) && (0 == memcmp("omr_test", tpMod, omr_test_len))) {
		/* The "unlogged" tracepoint is omr_test.5 */
		if (5 == tpId) {
			childData->unloggedCount += 1;
		} else {
			childData->loggedCount += 1;
			if (1 == tpId) { /* String tracepoint, data includes terminating nul */
				if (1 == childData->traceDataCount) {
					uint8_t *data = (uint8_t *)record + firstParameterOffset;
					if ((parameterDataLength != strlen(childData->traceData[0]) + 1)
						|| (0 != memcmp(childData->traceData[0], data, parameterDataLength))
					) {
						fprintf(stderr, "data mismatch, expected=\"%s\", actual=(%u bytes) \"%*s\"\n",
								childData->traceData[0], parameterDataLength, parameterDataLength, data);
						abort();
					}
				}
			}
		}
	}
	return OMR_ERROR_NONE;
}

/*
 * Count the number of omr_test tracepoints logged.
 * Count the number of "unlogged" tracepoints logged.
 */
static omr_error_t
countTracepoints(UtSubscription *subscriptionID)
{
	TestChildThreadData *childData = (TestChildThreadData *)subscriptionID->userData;
	const UtTraceRecord *traceRecord = (const UtTraceRecord *)subscriptionID->data;

	EXPECT_TRUE(NULL != childData->osThread);

	if ((omrthread_t)(uintptr_t)traceRecord->threadSyn1 == childData->osThread) {
		processTraceRecord(&childData->wrapBuffer, subscriptionID, countTracepointsIter, subscriptionID->userData);
	}
	return OMR_ERROR_NONE;
}

/*
 * Fail on 2nd call
 * Count the number of calls
 */
static omr_error_t
failOnSecondCall(UtSubscription *subscriptionID)
{
	FailingSubscriberData *failData = (FailingSubscriberData *)subscriptionID->userData;
	omr_error_t rc = OMR_ERROR_NONE;

	uintptr_t newCallCount = VM_AtomicSupport::addU32(&failData->callCount, 1);
	if (newCallCount > 1) {
		rc = OMR_ERROR_INTERNAL;
	}
	return rc;
}

static void
failOnSecondCallAlarm(UtSubscription *subscriptionID)
{
	FailingSubscriberData *failData = (FailingSubscriberData *)subscriptionID->userData;

	VM_AtomicSupport::addU32(&failData->alarmCount, 1);
}
