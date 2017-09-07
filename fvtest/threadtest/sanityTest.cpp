/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "threadTestLib.hpp"
#include "sanityTestHelper.hpp"

#include "omrTest.h"
#include "testHelper.hpp"

extern ThreadTestEnvironment *omrTestEnv;

class SanityTest: public ::testing::Test
{
public:
	static unsigned int runTime;
protected:

	static void
	SetUpTestCase(void)
	{
		/* parse the command line options */
		for (int32_t i = 1; i < omrTestEnv->_argc; i++) {
			if (0 == strncmp(omrTestEnv->_argv[i], "-runTime=", strlen("-runTime="))) {
				sscanf(&omrTestEnv->_argv[i][strlen("-runTime=")], "%u", &runTime);
			}
		}
	}
};
unsigned int SanityTest::runTime = 2; /* default test run time of 2 second */

TEST_F(SanityTest, simpleSanity)
{
	SimpleSanity();
	ASSERT_TRUE(SimpleSanity());
}

TEST_F(SanityTest, SanityTestSingleThread)
{
	SanityTestNThreads(1, runTime);
}

TEST_F(SanityTest, SanityTestTwoThreads)
{
	SanityTestNThreads(2, runTime);
}

TEST_F(SanityTest, SanityTestFourThreads)
{
	SanityTestNThreads(4, runTime);
}

TEST_F(SanityTest, QuickNDirtyPerformanceTest)
{
	QuickNDirtyPerformanceTest(runTime);
}

TEST_F(SanityTest, TestWaitNotify)
{
	TestWaitNotify(runTime);
}

TEST_F(SanityTest, TestBlockingQueue)
{
	omrthread_t self;
	omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

	CThread cself(self);
	EXPECT_TRUE(TestBlockingQueue(cself, 1)) << "1 thread failed";
	EXPECT_TRUE(TestBlockingQueue(cself, 2)) << "2 threads failed";
	EXPECT_TRUE(TestBlockingQueue(cself, 3)) << "3 threads failed";
}
