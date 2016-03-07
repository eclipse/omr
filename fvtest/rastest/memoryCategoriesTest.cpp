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

#include "omr.h"
#include "omrTest.h"
#include "omrTestHelpers.h"
#include "omrvm.h"

#include "rasTestHelpers.hpp"

TEST(RASMemoryCategoriesTest, Agent)
{
	/* OMR VM data structures */
	OMRTestVM testVM;
	OMR_VMThread *vmthread = NULL;

	const char *agentName = "memorycategoriesagent";
	struct OMR_Agent *agent = NULL;

	OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, rasTestEnv->getPortLibrary()));

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Init(&testVM.omrVM, NULL, &vmthread, "memoryCategoriesTest"));

	/* load memorycategoriesagent */
	agent = omr_agent_create(&testVM.omrVM, agentName);
	ASSERT_FALSE(NULL == agent) << "createAgent(" << agentName << ") failed";

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));

	/** The test code is actually in the OnLoadFunction **/

	/* Unload the agent */
	omr_agent_callOnUnload(agent);
	omr_agent_destroy(agent);

	OMRTEST_ASSERT_ERROR_NONE(OMR_Thread_Free(vmthread));

	/* Now clear up the VM we started for this test case. */
	OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
}
