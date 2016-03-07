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

#define TEST_TRACEAGENT 1

/* Test data */
typedef struct TestChildThreadData {
	OMRTestVM *testVM;
	omrthread_monitor_t shutdownCond;
	BOOLEAN isDead;
	omr_error_t childRc;
} TestChildThreadData;

static omr_error_t startTestChildThread(OMRTestVM *testVM, OMR_VMThread *curVMThread, omrthread_t *childThread, TestChildThreadData **childData);
static omr_error_t waitForTestChildThread(OMRTestVM *testVM, omrthread_t childThead, TestChildThreadData *childData);
static int J9THREAD_PROC childThreadMain(void *entryArg);

TEST(RASTraceTest, TraceAgent)
{
	struct OMR_Agent *agent = NULL;

	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	/* child thread data */
	omrthread_t childThread = NULL;
	TestChildThreadData *childData = NULL;


	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));

	/* WARNING: This negative test leaks memory. */
	OMRTEST_ASSERT_ERROR(omr_ras_initTraceEngine(&testVM.omrVM, "print=all:duck=quack", datDir), OMR_ERROR_ILLEGAL_ARGUMENT);

	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, "maximal=all", datDir));

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "traceTest"));

#if TEST_TRACEAGENT
	/* load traceagent */
	{
		agent = omr_agent_create(&testVM.omrVM, "traceagent");
		ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() traceagent failed";

		OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));

		OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));
	}
#endif /* TEST_TRACEAGENT */

	/* Initialise the omr_test module for tracing */
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);

	/* Fire some trace points! */
	Trc_OMR_Test_Init();
	Trc_OMR_Test_String(vmthread, "Hello World!");
	Trc_OMR_Test_Ptr(vmthread, vmthread);
	Trc_OMR_Test_Int(vmthread, 10);
	Trc_OMR_Test_ManyParms(vmthread, "Hello again!", vmthread, 10);

	/* Fire some trace points in another thread. */
	OMRTEST_ASSERT_ERROR_NONE(startTestChildThread(&testVM, vmthread, &childThread, &childData));

	OMRTEST_ASSERT_ERROR_NONE(waitForTestChildThread(&testVM, childThread, childData));

	/* Confirm that the test worked! */
	/* OMRTODO Check something */

	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);

#if TEST_TRACEAGENT
	/* Unload the traceagent */
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);
#endif /* TEST_TRACEAGENT */

	/* Load sampleSubscriber agent */
#if defined(WIN32)
	agent = omr_agent_create(&testVM.omrVM, "sampleSubscriber=NUL");
#else /* defined(WIN32) */
	agent = omr_agent_create(&testVM.omrVM, "sampleSubscriber=/dev/null");
#endif /* defined(WIN32) */
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() sampleSubscriber failed";

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);

	/* Load bindthreadagent agent */
	agent = omr_agent_create(&testVM.omrVM, "bindthreadagent");
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() bindthreadagent failed";

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);

	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
}

static omr_error_t
startTestChildThread(OMRTestVM *testVM, OMR_VMThread *curVMThread, omrthread_t *childThread, TestChildThreadData **childData)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);
	TestChildThreadData *newChildData = (TestChildThreadData *)omrmem_allocate_memory(sizeof(*newChildData), OMRMEM_CATEGORY_VM);
	if (NULL == newChildData) {
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		omrtty_printf("%s:%d ERROR: Failed to alloc newChildData\n", __FILE__, __LINE__);
	}

	if (OMR_ERROR_NONE == rc) {
		if (0 != omrthread_monitor_init_with_name(&newChildData->shutdownCond, 0, "traceTestChildShutdown")) {
			rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
			omrtty_printf("%s:%d ERROR: Failed to init shutdownCond monitor\n", __FILE__, __LINE__);
			omrmem_free_memory(newChildData);
		} else {
			newChildData->testVM = testVM;
			newChildData->isDead = FALSE;
			newChildData->childRc = OMR_ERROR_NONE;
		}
	}

	if (OMR_ERROR_NONE == rc) {
		if (0 != omrthread_create_ex(
			NULL, /* handle */
			J9THREAD_ATTR_DEFAULT, /* attr */
			FALSE, /* suspend */
			childThreadMain, /* entrypoint */
			newChildData)  /* entryarg */
		) {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
			omrtty_printf("%s:%d ERROR: Failed to init shutdownCond monitor\n", __FILE__, __LINE__);
			omrthread_monitor_destroy(newChildData->shutdownCond);
			omrmem_free_memory(newChildData);
		} else {
			*childData = newChildData;
		}
	}
	return rc;
}

static omr_error_t
waitForTestChildThread(OMRTestVM *testVM, omrthread_t childThead, TestChildThreadData *childData)
{
	omr_error_t childRc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRPORT(testVM->portLibrary);

	omrthread_monitor_enter(childData->shutdownCond);
	while (!childData->isDead) {
		omrthread_monitor_wait(childData->shutdownCond);
	}
	omrthread_monitor_exit(childData->shutdownCond);

	childRc = childData->childRc;

	omrthread_monitor_destroy(childData->shutdownCond);
	omrmem_free_memory(childData);
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

	Trc_OMR_Test_String(vmthread, "This tracepoint should be ignored. Trace is not yet enabled for this thread.");

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Init(&testVM->omrVM, NULL, &vmthread, "traceTest-child"));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}

	/* Fire some trace points */
	Trc_OMR_Test_Init();
	Trc_OMR_Test_String(vmthread, "Child Hello World!");
	Trc_OMR_Test_Ptr(vmthread, vmthread);
	Trc_OMR_Test_Int(vmthread, 10);
	Trc_OMR_Test_ManyParms(vmthread, "Child Hello again!", vmthread, 10);

	rc = OMRTEST_PRINT_ERROR(OMR_Thread_Free(vmthread));
	if (OMR_ERROR_NONE != rc) {
		childData->childRc = rc;
		return -1;
	}

	Trc_OMR_Test_String(vmthread, "This tracepoint should be ignored. Trace has been disabled for this thread.");

	omrthread_monitor_enter(childData->shutdownCond);
	childData->isDead = TRUE;
	omrthread_monitor_notify_all(childData->shutdownCond);
	omrthread_monitor_exit(childData->shutdownCond);

	return 0;
}

