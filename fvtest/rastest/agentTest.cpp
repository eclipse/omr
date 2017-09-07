/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
