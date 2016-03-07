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

#include "rasTestHelpers.hpp"

#define ENABLE_BUGS 0

/*
 * This test covers unusual sequences of trace engine operations
 * relative to starting up and shutting down the trace engine.
 */


/* Test data */
typedef struct ChildThreadData {
	OMRTestVM *testVM;
	omr_error_t childRc;
} ChildThreadData;

typedef struct TracePointCounts {
	omrthread_t osThread;
	PerThreadWrapBuffer wrapBuffer;
	int loggedCount;
	int unloggedCount;
} TracePointCounts;

typedef struct TracePointCountsMT {
	int numThreads;
	TracePointCounts *tpCounts;
	omrthread_monitor_t lock;
} TracePointCountsMT;

static void startChildThread(OMRTestVM *testVM, omrthread_t *childThread, omrthread_entrypoint_t entryProc, ChildThreadData **childData);
static omr_error_t waitForChildThread(OMRTestVM *testVM, omrthread_t childThread, ChildThreadData *childData);

static int J9THREAD_PROC attachDetachHelper(void *entryArg);
static int J9THREAD_PROC moduleUnloadAfterThreadDetachHelper(void *entryArg);
static int J9THREAD_PROC shutdownTraceHelper(void *entryArg);
static omr_error_t subscribeFunc(UtSubscription *subscriptionID);
static omr_error_t countTracepoints(UtSubscription *subscriptionID);
static omr_error_t countTracepointsMT(UtSubscription *subscriptionID);
static omr_error_t countTracepointsIter(void *userData, const char *tpMod, const uint32_t tpModLength, const uint32_t tpId,
										const UtTraceRecord *record, uint32_t firstParameterOffset, uint32_t parameterDataLength,
										int32_t isBigEndian);

/*
 * This test just starts up and shuts down the trace engine.
 * Run it in a loop to check for memory leaks.
 */
TEST(TraceLifecycleTest, startupShutdownSanity)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));

	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", "."));

	/* Attach the thread to the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "stub"));

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
}

TEST(TraceLifecycleTest, moduleLoadBeforeThreadAttach)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;
	const OMR_TI *ti = omr_agent_getTI();
	UtSubscription *subscriptionID = NULL;

	TracePointCounts tpCounts;
	memset(&tpCounts, 0, sizeof(tpCounts));
	tpCounts.osThread = omrthread_self();
	initWrapBuffer(&tpCounts.wrapBuffer);

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", datDir));

	/* Attempt to load a module before attaching the thread. This should fail silently without crashing */
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);

	/* Attach the thread to the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "moduleLoadBeforeThreadAttach"));

	OMRTEST_ASSERT_ERROR_NONE(
		ti->RegisterRecordSubscriber(vmthread, "moduleLoadBeforeThreadAttach", countTracepoints, NULL, (void *)&tpCounts, &subscriptionID));

	/* Attempt to log a tracepoint. Nothing should happen. */
	Trc_OMR_Test_UnloggedTracepoint(vmthread, "The trace module was loaded before the current thread was attached.");

#if ENABLE_BUGS
	/* This can't be done because there is no check for whether the module was actually loaded. */
	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);
#endif /* ENABLE_BUGS */

	/* This should succeed. */
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);
	Trc_OMR_Test_String(vmthread, "This tracepoint should appear.");
	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	/*
	 * Must not deregister before omr_ras_cleanupTraceEngine() because cleanup flushes the trace buffers.
	 * This succeeds for a weird reason: omr_ras_cleanupTraceEngine() actually deregistered all
	 * subscribers when it detached the current thread. This Deregister call does nothing.
	 */
	OMRTEST_ASSERT_ERROR_NONE(ti->DeregisterRecordSubscriber(vmthread, subscriptionID));

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	/* Validate the number of tracepoints */
	ASSERT_EQ(0, tpCounts.unloggedCount);
	ASSERT_EQ(1, tpCounts.loggedCount);
	freeWrapBuffer(&tpCounts.wrapBuffer);

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}

TEST(TraceLifecycleTest, moduleUnloadAfterThreadDetach)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	omrthread_t childThread = NULL;
	ChildThreadData *childData = NULL;

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", datDir));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "moduleLoadBeforeThreadAttach"));

	ASSERT_NO_FATAL_FAILURE(startChildThread(&testVM, &childThread, moduleUnloadAfterThreadDetachHelper, &childData));
	ASSERT_EQ(1, omrthread_resume(childThread));
	OMRTEST_ASSERT_ERROR_NONE(waitForChildThread(&testVM, childThread, childData));

	/* Unload the module that was left loaded by the child thread */
	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}

/*
 * Without ENABLE_BUGS, this test doesn't test anything.
 */
TEST(TraceLifecycleTest, DISABLE_moduleLoadAfterTraceShutdown)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", datDir));

	/* Attach the thread to the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "moduleLoadAfterTraceShutdown"));

#if ENABLE_BUGS
	/* cache the UtInterface */
	UtInterface *utIntf = testVM.omrVM._trcEngine->utIntf;
#endif /* ENABLE_BUGS */

	/* Shut down the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	ASSERT_TRUE(NULL == vmthread->_vm->utIntf);
	ASSERT_TRUE(NULL == (void *)testVM.omrVM._trcEngine);

#if ENABLE_BUGS
	/* Attempt to load a module. Note this requires bogus caching of UtInterface*, and using it after it was freed. */

	/* moduleLoaded() crashes on attempt to use omrTraceGlobal */
	UT_OMR_TEST_MODULE_LOADED(utIntf);
	Trc_OMR_Test_UnloggedTracepoint(vmthread, "The module was loaded after trace shutdown.");
	UT_OMR_TEST_MODULE_UNLOADED(utIntf);
#endif /* ENABLE_BUGS */

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}

/*
 * Tests:
 * - Attempt to log a tracepoint after trace engine shutdown started.
 * - Attempt to unload a module after trace engine shutdown started.
 *
 * If module unloading fails, this test case tends to cause later tests
 * to crash because the static omr_test_UtModuleInfo and omr_test_UtActive
 * structures are left in a polluted state.
 */
TEST(TraceLifecycleTest, traceAndModuleUnloadAfterTraceShutdown)
{
	const OMR_TI *ti = omr_agent_getTI();
	UtSubscription *subscriptionID = NULL;

	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	/* child thread data */
	omrthread_t shutdownTrcThr = NULL;
	ChildThreadData *shutdownTrcData = NULL;

	TracePointCountsMT tpCountsMT;
	TracePointCounts tpCounts[2];
	tpCountsMT.numThreads = 2;
	tpCountsMT.tpCounts = tpCounts;
	memset(tpCounts, 0, sizeof(TracePointCounts) * 2);
	initWrapBuffer(&tpCounts[0].wrapBuffer);
	initWrapBuffer(&tpCounts[1].wrapBuffer);
	tpCounts[0].osThread = omrthread_self();
	ASSERT_EQ(0, omrthread_monitor_init_with_name(&tpCountsMT.lock, 0, "&tpCountsMT.lock"));

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", datDir));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "traceTest"));

	/* start counting tracepoints */
	OMRTEST_ASSERT_ERROR_NONE(
		ti->RegisterRecordSubscriber(vmthread, "traceAndModuleUnloadAfterTraceShutdown", countTracepointsMT, NULL, (void *)&tpCountsMT, &subscriptionID));

	/* Initialise the omr_test module for tracing */
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);

	/* Shut down the trace engine */
	ASSERT_NO_FATAL_FAILURE(startChildThread(&testVM, &shutdownTrcThr, shutdownTraceHelper, &shutdownTrcData));
	ASSERT_EQ(1, omrthread_resume(shutdownTrcThr));
	OMRTEST_ASSERT_ERROR_NONE(waitForChildThread(&testVM, shutdownTrcThr, shutdownTrcData));

	/* Attempt to log a tracepoint. It will not be logged. */
	Trc_OMR_Test_UnloggedTracepoint(vmthread, "Tracepoint initiated after trace engine shutdown is started.");

	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);

	/* Now clear up the VM we started for this test case. */

	/* Stop counting tracepoints */
	omr_trc_stopThreadTrace(vmthread);  /* flush the thread's trace buffer and delete all subscribers */
	OMRTEST_ASSERT_ERROR_NONE(ti->DeregisterRecordSubscriber(vmthread, subscriptionID)); /* do nothing */

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	ASSERT_EQ(0, tpCounts[0].unloggedCount);
	ASSERT_EQ(0, tpCounts[0].loggedCount);
	freeWrapBuffer(&tpCounts[0].wrapBuffer);

	ASSERT_EQ(0, tpCounts[1].unloggedCount);
	ASSERT_EQ(0, tpCounts[1].loggedCount);
	freeWrapBuffer(&tpCounts[1].wrapBuffer);

	ASSERT_EQ(0, omrthread_monitor_destroy(tpCountsMT.lock));

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}


TEST(TraceLifecycleTest, registerSubscriberAfterShutdown)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;
	UtSubscription *subscriptionID = NULL;
	const OMR_TI *ti = omr_agent_getTI();

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", datDir));

	/* Attach the thread to the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "registerSubscriberAfterShutdown"));

	/* Shut down the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	/* Attempt to register using external agent API */
	OMRTEST_ASSERT_ERROR(OMR_ERROR_NOT_AVAILABLE,
						 ti->RegisterRecordSubscriber(vmthread, "registerSubscriberAfterShutdown",
						 							  subscribeFunc, NULL, NULL, &subscriptionID));

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}

TEST(TraceLifecycleTest, deregisterSubscriberAfterShutdown)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;
	UtSubscription *subscriptionID = NULL;
	const OMR_TI *ti = omr_agent_getTI();

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", datDir));

	/* Attach the thread to the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "registerSubscriberAfterShutdown"));

	/* Register the subscriber */
	OMRTEST_ASSERT_ERROR_NONE(
		ti->RegisterRecordSubscriber(vmthread, "registerSubscriberAfterShutdown", subscribeFunc, NULL, NULL, &subscriptionID));

	/* Shut down the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	/* Attempt to deregister using external agent API. This succeeds because this thread is still attached to the trace engine. */
	OMRTEST_ASSERT_ERROR_NONE(ti->DeregisterRecordSubscriber(vmthread, subscriptionID));

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}

TEST(TraceLifecycleTest, rebindAndDeregisterSubscriberAfterShutdown)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;
	UtSubscription *subscriptionID = NULL;
	const OMR_TI *ti = omr_agent_getTI();

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all:print=omr_test", datDir));

	/* Attach the thread to the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "rebindAndDeregisterSubscriberAfterShutdown"));

	/* Register the subscriber */
	OMRTEST_ASSERT_ERROR_NONE(
		ti->RegisterRecordSubscriber(vmthread, "rebindAndDeregisterSubscriberAfterShutdown", subscribeFunc, NULL, NULL, &subscriptionID));

	/* Shut down the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	/* Detach the last thread from the trace engine, so it's freed */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));

	ASSERT_TRUE(NULL == (void *)testVM.omrVM._trcEngine);

	/* Re-bind this thread */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "rebindAndDeregisterSubscriberAfterShutdown#2"));
	/* Attempt to deregister using external agent API. This should return OMR_ERROR_NOT_AVAILABLE */
	OMRTEST_ASSERT_ERROR_NONE(ti->DeregisterRecordSubscriber(vmthread, subscriptionID));
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}

TEST(TraceLifecycleTest, threadAttachDetachStress)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;
	const int attachDetachHelpersCount = 10;
	omrthread_t attachDetachHelpers[attachDetachHelpersCount];
	ChildThreadData *attachDetachData[attachDetachHelpersCount];
	omrthread_t shutdownHelper = NULL;
	ChildThreadData *shutdownData = NULL;

	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));
	/* use small buffers to exercise buffer wrapping */
	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "buffers=1k:maximal=all", datDir));

	/* Attach the thread to the trace engine */
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "registerSubscriberAfterShutdown"));
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);
	/* module is not unloaded before trace engine shutdown */

	for (int i = 0; i < attachDetachHelpersCount; i++) {
		ASSERT_NO_FATAL_FAILURE(startChildThread(&testVM, &attachDetachHelpers[i], attachDetachHelper, &attachDetachData[i]));
	}
	ASSERT_NO_FATAL_FAILURE(startChildThread(&testVM, &shutdownHelper, shutdownTraceHelper, &shutdownData));

	for (int i = 0; i < attachDetachHelpersCount; i++) {
		ASSERT_EQ(1, omrthread_resume(attachDetachHelpers[i]));
	}
	ASSERT_EQ(1, omrthread_resume(shutdownHelper));

	for (int i = 0; i < attachDetachHelpersCount; i++) {
		OMRTEST_ASSERT_ERROR_NONE(waitForChildThread(&testVM, attachDetachHelpers[i], attachDetachData[i]));
	}
	OMRTEST_ASSERT_ERROR_NONE(waitForChildThread(&testVM, shutdownHelper, shutdownData));

	/* Now clear up the VM we started for this test case. */
	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);
	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));

	ASSERT_TRUE(NULL == (void *)omr_test_UtModuleInfo.intf);
}

static void
startChildThread(OMRTestVM *testVM, omrthread_t *childThread, omrthread_entrypoint_t entryProc, ChildThreadData **childData)
{
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	ChildThreadData *newChildData = (ChildThreadData *)omrmem_allocate_memory(sizeof(*newChildData), OMRMEM_CATEGORY_VM);
	ASSERT_FALSE(NULL == newChildData) << "Failed to alloc newChildData\n";
	memset(newChildData, 0, sizeof(*newChildData));
	newChildData->testVM = testVM;
	newChildData->childRc = OMR_ERROR_NONE;

	ASSERT_NO_FATAL_FAILURE(createThread(childThread, TRUE, J9THREAD_CREATE_JOINABLE, entryProc, newChildData));

	*childData = newChildData;
}

static omr_error_t
waitForChildThread(OMRTestVM *testVM, omrthread_t childThread, ChildThreadData *childData)
{
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);
	omr_error_t childRc = OMR_ERROR_INTERNAL;

	if (J9THREAD_SUCCESS == joinThread(childThread)) {
		childRc = childData->childRc;
	}
	omrmem_free_memory(childData);
	return childRc;
}

static int J9THREAD_PROC
moduleUnloadAfterThreadDetachHelper(void *entryArg)
{
	omr_error_t rc = OMR_ERROR_NONE;
	ChildThreadData *childData = (ChildThreadData *)entryArg;
	OMRTestVM *testVM = childData->testVM;
	OMR_VMThread *vmthread = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Init(&testVM->omrVM, NULL, &vmthread, "moduleUnloadAfterThreadDetachHelper"));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}

	UT_OMR_TEST_MODULE_LOADED(testVM->omrVM._trcEngine->utIntf);
	Trc_OMR_Test_String(vmthread, "This tracepoint should appear.");

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Free(vmthread));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}

	/* This should fail silently without crashing */
	UT_OMR_TEST_MODULE_UNLOADED(testVM->omrVM._trcEngine->utIntf);

	return 0;
}

static int J9THREAD_PROC
attachDetachHelper(void *entryArg)
{
	omr_error_t rc = OMR_ERROR_NONE;
	ChildThreadData *childData = (ChildThreadData *)entryArg;
	OMRTestVM *testVM = childData->testVM;
	OMR_VMThread *vmthread = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Init(&testVM->omrVM, NULL, &vmthread, "attachDetachHelper"));
	if (OMR_ERROR_NONE != rc) {
		if (OMR_ERROR_NOT_AVAILABLE == rc) {
			/* this is ok, it means the shutdown helper finished first */
			return 0;
		} else {
			childData->childRc = rc;
			return -1;
		}
	}
	/* yield to encourage concurency */
	omrthread_sleep(1);

	/* Fire some trace points */
	const char *alphabetUpper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	size_t stringsPerTraceBuffer = (1024 + strlen(alphabetUpper) - 1) / strlen(alphabetUpper);
	for (size_t i = 0; i < stringsPerTraceBuffer * 5; i += 1) {
		Trc_OMR_Test_String(vmthread, alphabetUpper);
		omrthread_yield();
	}

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Free(vmthread));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}
	return 0;
}

static int J9THREAD_PROC
shutdownTraceHelper(void *entryArg)
{
	omr_error_t rc = OMR_ERROR_NONE;
	ChildThreadData *childData = (ChildThreadData *)entryArg;
	OMRTestVM *testVM = childData->testVM;
	OMR_VMThread *vmthread = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Init(&testVM->omrVM, NULL, &vmthread, "shutdownTraceHelper"));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}
	rc = OMRTEST_PRINT_ERROR(omr_ras_cleanupTraceEngine(vmthread));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}
	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Free(vmthread));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}
	return 0;
}

static omr_error_t
subscribeFunc(UtSubscription *subscriptionID)
{
	return OMR_ERROR_NONE;
}

/*
 * Callback invoked per tracepoint in a tracepoint buffer
 *
 * Count the number of omr_test tracepoints logged.
 * Count the number of "unlogged" tracepoints logged.
 */
static omr_error_t
countTracepointsIter(void *userData, const char *tpMod, const uint32_t tpModLength, const uint32_t tpId,
					 const UtTraceRecord *record, uint32_t firstParameterOffset, uint32_t parameterDataLength,
					 int32_t isBigEndian)
{
	TracePointCounts *tpCounts = (TracePointCounts *)userData;
	const uint32_t omr_test_len = sizeof("omr_test") - 1;

	if ((omr_test_len == tpModLength) && (0 == memcmp("omr_test", tpMod, omr_test_len))) {
		/* The "unlogged" tracepoint is omr_test.5 */
		if (5 == tpId) {
			tpCounts->unloggedCount += 1;
		} else {
			tpCounts->loggedCount += 1;
		}
	}
	return OMR_ERROR_NONE;
}

static omr_error_t
countTracepoints(UtSubscription *subscriptionID)
{
	TracePointCounts *tpCounts = (TracePointCounts *)subscriptionID->userData;
	const UtTraceRecord *traceRecord = (const UtTraceRecord *)subscriptionID->data;
	if ((omrthread_t)(uintptr_t)traceRecord->threadSyn1 == tpCounts->osThread) {
		processTraceRecord(&tpCounts->wrapBuffer, subscriptionID, countTracepointsIter, (void *)tpCounts);
	}
	return OMR_ERROR_NONE;
}

static omr_error_t
countTracepointsMT(UtSubscription *subscriptionID)
{
	TracePointCountsMT *tpCountsMT = (TracePointCountsMT *)subscriptionID->userData;
	const UtTraceRecord *traceRecord = (const UtTraceRecord *)subscriptionID->data;

	EXPECT_EQ(0, omrthread_monitor_enter(tpCountsMT->lock));
	TracePointCounts *tpCounts = NULL;
	for (int i = 0; i < tpCountsMT->numThreads; i += 1) {
		if ((omrthread_t)(uintptr_t)traceRecord->threadSyn1 == tpCountsMT->tpCounts[i].osThread) {
			tpCounts = &tpCountsMT->tpCounts[i];
			break;
		}
	}
	if (NULL == tpCounts) {
		for (int i = 0; i < tpCountsMT->numThreads; i += 1) {
			if (NULL == tpCountsMT->tpCounts[i].osThread) {
				tpCounts = &tpCountsMT->tpCounts[i];
				tpCounts->osThread = (omrthread_t)(uintptr_t)traceRecord->threadSyn1;
				break;
			}
		}
	}
	EXPECT_EQ(0, omrthread_monitor_exit(tpCountsMT->lock));

	EXPECT_TRUE(NULL != tpCounts);
	if (NULL != tpCounts) {
		processTraceRecord(&tpCounts->wrapBuffer, subscriptionID, countTracepointsIter, (void *)tpCounts);
	}
	return OMR_ERROR_NONE;
}
