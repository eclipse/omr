/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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


#include "omrport.h"
#include "omr.h"
#include "omrTest.h"
#include "omrTestHelpers.h"

#include "OMR_Agent.hpp"
#include "rasTestHelpers.hpp"

class RASAgentTest: public ::testing::TestWithParam<const char *>
{
protected:
	virtual void
	SetUp()
	{
		OMRTEST_ASSERT_ERROR_NONE(omrTestVMInit(&testVM, rasTestEnv->getPortLibrary()));
	}

	virtual void
	TearDown()
	{
		OMRTEST_ASSERT_ERROR_NONE(omrTestVMFini(&testVM));
	}

	OMRTestVM testVM;
};

TEST_P(RASAgentTest, AgentCPP)
{
	OMR_Agent *agent = OMR_Agent::createAgent(&testVM.omrVM, GetParam());
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() failed";

	OMRTEST_ASSERT_ERROR_NONE(agent->openLibrary());

	OMRTEST_ASSERT_ERROR_NONE(agent->callOnLoad());

	OMRTEST_ASSERT_ERROR_NONE(agent->callOnUnload());

	OMR_Agent::destroyAgent(agent);
}

TEST_P(RASAgentTest, AgentC)
{
	struct OMR_Agent *agent = omr_agent_create(&testVM.omrVM, GetParam());
	ASSERT_FALSE(NULL == agent) << "testAgent: createAgent() failed";

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_openLibrary(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnLoad(agent));

	OMRTEST_ASSERT_ERROR_NONE(omr_agent_callOnUnload(agent));

	omr_agent_destroy(agent);
}

INSTANTIATE_TEST_CASE_P(TraceNotStartedAgentOpts, RASAgentTest, ::testing::Values("traceNotStartedAgent", "traceNotStartedAgent=", "traceNotStartedAgent=abc"));
INSTANTIATE_TEST_CASE_P(CpuLoadAgentOpts, RASAgentTest, ::testing::Values("cpuLoadAgent"));
INSTANTIATE_TEST_CASE_P(BindThreadAgentOpts, RASAgentTest, ::testing::Values("bindthreadagent"));

/*
 * Test scenario for ArgNullC and ArgNullCPP:
 *  - arg is NULL (default case for cpython)
 */
TEST_F(RASAgentTest, ArgNullC)
{
	struct OMR_Agent *agent = omr_agent_create(&testVM.omrVM, NULL);
	ASSERT_TRUE(NULL == agent);
}

TEST_F(RASAgentTest, ArgNullCPP)
{
	OMR_Agent *agent = OMR_Agent::createAgent(&testVM.omrVM, NULL);
	ASSERT_TRUE(NULL == agent);
}
