/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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

#include "omrTest.h"
#include "gcTestHelpers.hpp"

extern "C" {
int testMain(int argc, char **argv, char **envp);
}

GCTestEnvironment *gcTestEnv;

int
testMain(int argc, char **argv, char **envp)
{
	int result = 1;

	gcTestEnv = (GCTestEnvironment *)testing::AddGlobalTestEnvironment(new GCTestEnvironment(argc, argv));
	/* This must be called before InitGoogleTest. Because we need to initialize the parameter vector before InitGoogleTest
	 * calls INSTANTIATE_TEST_CASE_P and takes the vector. */
	gcTestEnv->GCTestSetUp();

	/*
	 * ASSERT_NO_FATAL_FAILURE() cannot be used in non-void function. Therefore,
	 * we need to use HasFatalFailure() to check for fatal failures in sub-routine.
	 */
	if (!testing::Test::HasFatalFailure()) {
		::testing::InitGoogleTest(&argc, argv);
		result = RUN_ALL_TESTS();
	}

	gcTestEnv->GCTestTearDown();
	return result;
}
