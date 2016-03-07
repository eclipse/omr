/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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
#include "threadExtendedTestHelpers.hpp"

ThreadExtendedTestEnvironment *omrTestEnv;

extern "C" int
testMain(int argc, char **argv, char **envp)
{
	::testing::InitGoogleTest(&argc, argv);

	int result = 0;
	OMRPortLibrary portLibrary;
	thrExtendedTestSetUp(&portLibrary);

	/*
	 * check for fatal failures in sub-routine thrExtendedTestSetUp().
	 */
	if (!testing::Test::HasFatalFailure()) {
		omrTestEnv = (ThreadExtendedTestEnvironment *)testing::AddGlobalTestEnvironment(new ThreadExtendedTestEnvironment(argc, argv, &portLibrary));

		OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);

		uint64_t start = omrtime_current_time_millis();
		uint64_t end = start;
		uint64_t grindingTime = omrTestEnv->grindTime * 60 * 1000;

		/* keep running the tests until we have run for the requested number of minutes */
		omrtty_printf("grinding time is %llu millis. (The value can be specified using -grind=<mins>)\n", grindingTime);
		do {
			result = RUN_ALL_TESTS();
			end = omrtime_current_time_millis();
		} while ((end < (start + grindingTime)) && (0 == result));
	}

	thrExtendedTestTearDown(&portLibrary);
	return result;
}
