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


#include <string.h>
#include "algorithm_test_internal.h"
#include "omrTest.h"
#include "testEnvironment.hpp"

extern PortEnvironment *omrTestEnv;

static void showResult(OMRPortLibrary *portlib, uintptr_t passCount, uintptr_t failCount, int32_t numSuitesNotRun);

TEST(OmrAlgoTest, avltest)
{
	char *avlTestFile = NULL;
	uintptr_t passCount = 0;
	uintptr_t failCount = 0;
	int32_t numSuitesNotRun = 0;

	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());

	for (int i = 0; i < omrTestEnv->_argc; i++) {
		if (!strncmp("-avltest:", omrTestEnv->_argv[i], 9)) {
			avlTestFile = &omrTestEnv->_argv[i][9];
		}
	}

	if (avlTestFile) {
		if (verifyAVLTree(privateOmrPortLibrary, avlTestFile, &passCount, &failCount)) {
			numSuitesNotRun++;
		}
	} else {
		omrtty_printf("No AVL test file specified (use: -avltest:filename)\n");
		numSuitesNotRun++;
	}
	showResult(privateOmrPortLibrary, passCount, failCount, numSuitesNotRun);
}

TEST(OmrAlgoTest, pooltest)
{
	uintptr_t passCount = 0;
	uintptr_t failCount = 0;
	int32_t numSuitesNotRun = 0;

	if (verifyPools(omrTestEnv->getPortLibrary(), &passCount, &failCount)) {
		numSuitesNotRun++;
	}
	showResult(omrTestEnv->getPortLibrary(), passCount, failCount, numSuitesNotRun);
}

TEST(OmrAlgoTest, hookabletest)
{
	uintptr_t passCount = 0;
	uintptr_t failCount = 0;
	int32_t numSuitesNotRun = 0;

	if (verifyHookable(omrTestEnv->getPortLibrary(), &passCount, &failCount)) {
		numSuitesNotRun++;
	}
	showResult(omrTestEnv->getPortLibrary(), passCount, failCount, numSuitesNotRun);
}

TEST(OmrAlgoTest, hashtabletest)
{
	uintptr_t passCount = 0;
	uintptr_t failCount = 0;
	int32_t numSuitesNotRun = 0;

	if (verifyHashtable(omrTestEnv->getPortLibrary(), &passCount, &failCount)) {
		numSuitesNotRun++;
	}
	showResult(omrTestEnv->getPortLibrary(), passCount, failCount, numSuitesNotRun);
}

static void
showResult(OMRPortLibrary *portlib, uintptr_t passCount, uintptr_t failCount, int32_t numSuitesNotRun)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portlib);

	omrtty_printf("Algorithm Test Finished\n");
	omrtty_printf("total tests: %d\n", passCount + failCount);
	omrtty_printf("total passes: %d\n", passCount);
	omrtty_printf("total failures: %d\n", failCount);

	/* want to report failures even if suites skipped or failed */
	EXPECT_TRUE(0 == numSuitesNotRun) << numSuitesNotRun << " SUITE" << (numSuitesNotRun != 1 ? "S" : "") << " NOT RUN!!!";
	EXPECT_TRUE(0 == failCount) << "THERE WERE FAILURES!!!";

	if ((0 == numSuitesNotRun) && (0 == failCount)) {
		omrtty_printf("ALL TESTS PASSED.\n");
	}
}
