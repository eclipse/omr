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

/*
 * The executable for this test is omrsubscribertest
 */

TEST(RASSubscriberTest, SubscriberAgent)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	/* OMR Trace data structures */
	const char *trcOpts = "print=all";

	struct OMR_Agent *agent = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(rasTestEnv->getPortLibrary());
	char *datDir = NULL;

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, OMRPORTLIB));

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "subscriberTest"));

	datDir = getTraceDatDir(rasTestEnv->_argc, (const char **)rasTestEnv->_argv);

	OMRTEST_ASSERT_ERROR_NONE(omr_ras_initTraceEngine(&testVM.omrVM, trcOpts, datDir));
	OMRTEST_ASSERT_ERROR_NONE(omr_trc_startThreadTrace(vmthread, "initialization thread"));

	/* load agent */
	agent = omr_agent_create(&testVM.omrVM, "subscriberAgent");
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() subscriberAgent failed";

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));

	/*  Initialise the omr_test module for tracing */
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);

	/*  Fire some trace points! */
	Trc_OMR_Test_Init();
	Trc_OMR_Test_Ptr(vmthread, vmthread);
	Trc_OMR_Test_Int(vmthread, 10);
	Trc_OMR_Test_Int(vmthread, 99);
	Trc_OMR_Test_ManyParms(vmthread, "Test subscriberAgent!", vmthread, 10);

	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);

	/*  Unload the agent */
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);

	/* load agent */
	agent = omr_agent_create(&testVM.omrVM, "subscriberAgentWithJ9thread");
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() subscriberAgentWithJ9thread failed";

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));

	/*  Initialise the omr_test module for tracing */
	UT_OMR_TEST_MODULE_LOADED(testVM.omrVM._trcEngine->utIntf);

	/*  Fire some trace points! */
	Trc_OMR_Test_Init();
	Trc_OMR_Test_Ptr(vmthread, vmthread);
	Trc_OMR_Test_Int(vmthread, 20);
	Trc_OMR_Test_Int(vmthread, 109);
	Trc_OMR_Test_ManyParms(vmthread, "Test SubscriberAgentWithJ9thread!", vmthread, 10);

	UT_OMR_TEST_MODULE_UNLOADED(testVM.omrVM._trcEngine->utIntf);

	/*  Unload the agent */
	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));
	omr_agent_destroy(agent);

	OMRTEST_ASSERT_ERROR_NONE(omr_ras_cleanupTraceEngine(vmthread));

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));

	/*  Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
}
