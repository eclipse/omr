/*******************************************************************************
 * Copyright (c) 2015, 2022 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <string.h>
#include "algorithm_test_internal.h"
#include "omrTest.h"
#include "testEnvironment.hpp"
#include "pool_api.h"

extern PortEnvironment *omrTestEnv;

typedef struct AVLParams {
	const char *testName;
	const char *success;
	const char *data;
} AVLParams;

static const AVLParams avlParams[] = {
	{"Empty tree", "@", "0"},
	{"Single element", "{=10@@}", "10, 0"},
	{"Left heavy", "{L10{=5@@}@}", "10, 5, 0"},
	{"Right heavy", "{R10@{=15@@}}", "10, 15, 0"},
	{"Double insert", "{=10@@}", "10, 10, 0"},
	{"Remove root: root is leaf node", "@", "10, -10, 0"},
	{"Remove root: adding node after", "{=5@@}", "10, -10, 5, 0"},
	{"Remove root: root is left branch", "{=5@@}", "10, 5, -10, 0"},
	{"Remove root: root is right branch", "{=15@@}", "10, 15, -10, 0"},
	{"Balanced tree: right then left", "{=10{=5@@}{=15@@}}", "10, 15, 5, 0"},
	{"Balanced tree: left then right", "{=10{=5@@}{=15@@}}", "10, 5, 15, 0"},
	{"Single rotate: left; including root; no children; via insertion", "{=10{=5@@}{=15@@}}", "5, 10, 15, 0"},
	{"Single rotate: right; including root; no children; via insertion", "{=10{=5@@}{=15@@}}", "15, 10, 5, 0"},
	{"Double rotate: left; including root; no children; via insertion", "{=10{=5@@}{=15@@}}", "5, 15, 10, 0"},
	{"Double rotate: right; including root; no children; via insertion", "{=10{=5@@}{=15@@}}", "15, 5, 10, 0"},
	{"Remove node: leaf node; right; no children", "{=10@@}", "10, 15, -15, 0"},
	{"Remove node: leaf node; left; no children", "{=10@@}", "10, 5, -5, 0"},
	{"Simple tree; height 3; right heavy", "{R5{=1@@}{=10{=7@@}{=15@@}}}", "5, 10, 1, 7, 15, 0"},
	{"Simple tree; height 3; left heavy", "{L20{=10{=7@@}{=15@@}}{=25@@}}", "20, 25, 10, 7, 15, 0"},
	{"Single rotate: left; including root; balanced; via deletion", "{L10{R5@{=7@@}}{=15@@}}", "5, 10, 1, 7, 15, -1, 0"},
	{"Single rotate: right; including root; balanced; via deletion", "{R10{=7@@}{L20{=15@@}@}}", "20, 25, 10, 15, 7, -25, 0"},
	{"double rotation; height 3; left heavy", "{L20{=7{=1@@}{=10@@}}{=25@@}}", "20, 25, 10, 1, 7, 0"},
	{"Single rotate: right; not root; no graft", "{=30{=5{=1@@}{=10@@}}{L40{=35@@}@}}", "30, 10, 40, 5, 20, 35, 1, -20, 0"},
	{"Delete non-existant node", "{R10@{=15@@}}", "10, 15, -20, 0"},
	{"Delete node twice", "{=10@@}", "10, 15, -15, -15, 0"},
	{"Delete node; no root" , "@", "-15, 0"},
	{"single rotation test1", "{L20{=5{=1@@}{=10@@}}{=40@@}}", "20, 10, 40, 5, 1, 0"},
	{"single rotation test2", "{=20{=5{=1@@}{=10@@}}{R40@{=50@@}}}", "20, 10, 40, 5, 50, 1, 0"},
	{"single rotation test3", "{L20{R5{=1@@}{L10{=7@@}@}}{R40@{=50@@}}}", "20, 10, 40, 5, 15, 50, 1, 7, -15, 0"},
	{"single rotation test4", "{=20{R5{=1@@}{L10{=7@@}@}}{R40{=30@@}{R50@{=60@@}}}}", "20, 10, 40, 5, 15, 30, 50, 1, 7, 60, -15, 0"},
	{"single rotation test5", "{L20{=5{L4{=1@@}@}{=10{=7@@}{=15@@}}}{R40@{=50@@}}}", "20, 10, 40, 5, 15, 50, 4, 7, 1, 0"},
	{"single rotation test6", "{=20{=5{L4{=1@@}@}{=10{=7@@}{=15@@}}}{R40{=30@@}{R50@{=60@@}}}}", "20, 10, 40, 5, 15, 30, 50, 4, 7, 60, 1, 0"},
	{"single rotation test7", "{R80{=60@@}{=95{=90@@}{=99@@}}}", "80, 60, 90, 95, 99, 0"},
	{"single rotation test8", "{=80{L60{=50@@}@}{=95{=90@@}{=99@@}}}", "80, 60, 90, 50, 95, 99, 0"},
	{"single rotation test9", "{R80{L60{=50@@}@}{L95{R90@{=93@@}}{=99@@}}}", "80, 60, 90, 50, 85, 95, 93, 99, -85, 0"},
	{"single rotation test10", "{=80{L60{L50{=40@@}@}{=70@@}}{L95{R90@{=93@@}}{=99@@}}}", "80, 60, 90, 50, 70, 85, 95, 40, 93, 99, -85, 0"},
	{"single rotation test11", "{R80{L60{=50@@}@}{=95{=90{=85@@}{=93@@}}{R96@{=99@@}}}}", "80, 60, 90, 50, 85, 95, 93, 96, 99, 0"},
	{"single rotation test12", "{=80{L60{L50{=40@@}@}{=70@@}}{=95{=90{=85@@}{=93@@}}{R96@{=99@@}}}}", "80, 60, 90, 50, 70, 85, 95, 40, 93, 96, 99, 0"},
	{"remove with immediate leaf predecessor", "{R20{=10@@}{R30@{=50@@}}}", "20, 10, 40, 30, 50, -40, 0"},
	{"remove with immediate left branch predecessor", "{=20{L10{=5@@}@}{=30{=25@@}{=50@@}}}", "20, 10, 40, 5, 30, 50, 25, -40, 0"},
	{"remove with leaf predecessor", "{R20{L10{=5@@}@}{L35{L30{=25@@}@}{=50@@}}}", "20, 10, 40, 5, 30, 50, 25, 35, -40, 0"},
	{"remove with leaf left branch predecessor", "{=20{L10{L5{=1@@}@}{=15@@}}{=35{=30{=25@@}{=32@@}}{R50@{=55@@}}}}", "20, 10, 40, 5, 15, 30, 50, 1, 25, 35, 55, 32, -40, 0"},
	{"double rotation test1", "{L20{=7{=5@@}{=10@@}}{=40@@}}", "20, 10, 40, 5, 7, 0"},
	{"double rotation test2", "{=20{=7{=5@@}{=10@@}}{R40@{=50@@}}}", "20, 10, 40, 5, 50, 7, 0"},
	{"double rotation test3", "{L20{=12{=10@@}{=15@@}}{=40@@}}", "20, 10, 40, 15, 12, 0"},
	{"double rotation test4", "{=20{=12{=10@@}{=15@@}}{R40@{=50@@}}}", "20, 10, 40, 15, 50, 12, 0"},
	{"double rotation test5", "{L20{=7{L5{=4@@}@}{=10{=8@@}{=15@@}}}{R40@{=50@@}}}", "20, 10, 40, 5, 15, 50, 4, 7, 8, 0"},
	{"double rotation test6", "{=20{=7{=5{=4@@}{=6@@}}{=10{=8@@}{=15@@}}}{R40{=30@@}{R50@{=60@@}}}}", "20, 10, 40, 5, 15, 30, 50, 4, 7, 17, 60, 6, 8, -17, 0"},
	{"double rotation test7", "{L20{=13{L10{=5@@}@}{=15{=14@@}{=17@@}}}{R40@{=50@@}}}", "20, 10, 40, 5, 15, 50, 13, 17, 14, 0"},
	{"double rotation test8", "{=20{=13{=10{=5@@}{=12@@}}{=15{=14@@}{=17@@}}}{R40{=30@@}{R50@{=60@@}}}}", "20, 10, 40, 5, 15, 30, 50, 4, 7, 13, 17, 60, 12, 14, -7, -4, 0"},
	{"double rotation test9", "{R80{=60@@}{=93{=90@@}{=95@@}}}", "80, 60, 90, 95, 93, 0"},
	{"double rotation test10", "{=80{L60{=50@@}@}{=93{=90@@}{=95@@}}}", "80, 60, 90, 50, 95, 93, 0"},
	{"double rotation test11", "{R80{=60@@}{=87{=85@@}{=90@@}}}", "80, 60, 90, 85, 87, 0"},
	{"double rotation test12", "{=80{L60{=50@@}@}{=87{=85@@}{=90@@}}}", "80, 60, 90, 85, 50, 87, 0"},
	{"double rotation test13", "{R80{L60{=50@@}@}{=93{=90{=85@@}{=92@@}}{R95@{=96@@}}}}", "80, 60, 90, 50, 85, 95, 93, 96, 92, 0"},
	{"double rotation test14", "{=80{L60{L50{=40@@}@}{=70@@}}{=93{=90{=85@@}{=92@@}}{=95{=94@@}{=96@@}}}}", "80, 60, 90, 50, 70, 85, 95, 40, 83, 93, 96, 92, 94, -83, 0"},
	{"double rotation test15", "{R80{L60{=50@@}@}{=87{=85{=83@@}{=86@@}}{R90@{=95@@}}}}", "80, 60, 90, 50, 85, 95, 83, 87, 86, 0"},
	{"double rotation test16", "{=80{L60{L50{=40@@}@}{=70@@}}{=87{=85{=83@@}{=86@@}}{=90{=88@@}{=95@@}}}}", "80, 60, 90, 50, 70, 85, 95, 40, 83, 87, 93, 86, 88, -93, 0"},
};

static const PoolInputData poolParams[] = {
	{"very small pool",										1,		1,		sizeof(uintptr_t),		0,		0},
	{"page size pool - with 4-byte elements",				4,		0,		sizeof(uintptr_t),		0,		0},
	{"small pool",											4,		10,		sizeof(uintptr_t),		0,		0},
	{"small pool - default alignment size",					4,		10,		0,						0,		0},
	{"small pool - 8 alignment size",						4,		10,		8,						0,		0},
	{"small pool - large alignment size",					4,		10,		64,						0,		0},
	{"page size pool - with 8-byte elements", 				8,		0,		sizeof(uintptr_t),		0,		0},
	{"medium pool",											8,		100,	sizeof(uintptr_t),		0,		0},
	{"medium pool - default alignment size",				8,		100,	0,						0,		0},
	{"medium pool - 8 alignment size",						8,		100,	8,						0,		0},
	{"medium pool - large alignment size",					8,		100,	64,						0,		0},
	{"page size pool - with 16-byte elements",				16,		0,		sizeof(uintptr_t),		0,		0},
	{"larger pool",											16,		256,	sizeof(uintptr_t),		0,		0},
	{"larger pool - default alignment size",				16,		256,	0,						0,		0},
	{"larger pool - 8 alignment size",						16,		256,	8,						0,		0},
	{"larger pool - large alignment size",					16,		256,	64,						0,		0},
	{"large struct, small number",							9999,	5,		sizeof(uintptr_t),		0,		0},
	{"small struct, large number",							4,		9999,	sizeof(uintptr_t),		0,		0},
	{"zero minElements - should default to 1",				64,		0,		sizeof(uintptr_t),		0,		0},
	{"zero alignment - should default to MIN_GRANULARITY",	64,		10,		0,						0,		0},
	{"POOL_NO_ZERO flag",									32,		10,		sizeof(uintptr_t),		0,		POOL_NO_ZERO},
	{"POOL_ALWAYS_KEEP_SORTED flag",						32,		10,		sizeof(uintptr_t),		0,		POOL_ALWAYS_KEEP_SORTED},
	{"POOL_ROUND_TO_PAGE_SIZE flag",						32,		10,		sizeof(uintptr_t),		0,		POOL_ROUND_TO_PAGE_SIZE},
	{"POOL_NEVER_FREE_PUDDLES flag",						32,		10,		sizeof(uintptr_t),		0,		POOL_NEVER_FREE_PUDDLES},
};

static const uintptr_t data1[] = {1, 2, 3, 4, 5, 6, 7, 17, 18, 19, 20, 21, 22, 23, 24, 25};
static const uintptr_t data2[] = {1, 3, 5, 7, 18, 20, 22, 23, 24, 35, 36, 37};
static const uintptr_t data3[] = {25, 24, 23, 22, 21, 20, 19, 18, 17, 7, 6, 5, 4, 3, 2, 1};
static const uintptr_t data4[] = {37, 36, 35, 24, 23, 22, 20, 18, 7, 5, 3, 1};
static const uintptr_t data5[] = {17, 14, 8, 73, 23, 38, 91, 84, 31, 25, 11, 41, 26, 87, 35, 6};
static const uintptr_t data6[] = {17, 14, 8, 73, 23, 38, 91, 84, 31, 25, 11, 41, 26, 87, 35, 6, 13, 61, 10, 20, 92};
static const uintptr_t data7[] = {
	1, 87, 49, 12, 176, 178, 102, 166, 121, 193, 6, 84, 249, 230, 44, 163,
	14, 197, 213, 181, 161, 85, 218, 80, 64, 239, 24, 226, 236, 142, 38, 200,
	110, 177, 104, 103, 141, 253, 255, 50, 77, 101, 81, 18, 45, 96, 31, 222,
	25, 107, 190, 70, 86, 237, 240, 34, 72, 242, 20, 214, 244, 227, 149, 235,
	97, 234, 57, 22, 60, 250, 82, 175, 208, 5, 127, 199, 111, 62, 135, 248,
	132, 56, 148, 75, 128, 133, 158, 100, 130, 126, 91, 13, 153, 246, 216, 219,
	119, 68, 223, 78, 83, 88, 201, 99, 122, 11, 92, 32, 136, 114, 52, 10,
	138, 30, 48, 183, 156, 35, 61, 26, 143, 74, 251, 94, 129, 162, 63, 152,
	170, 7, 115, 167, 241, 206, 3, 150, 55, 59, 151, 220, 90, 53, 23, 131,
};

uint32_t listToTreeThresholdValues[] = {0, 1, 5, 10, 100};

static const HashtableInputData hastableParams[] = {
	{"Test1", data1, sizeof(data1) / sizeof(uintptr_t), FALSE},
	{"Test2", data2, sizeof(data2) / sizeof(uintptr_t), FALSE},
	{"Test3", data3, sizeof(data3) / sizeof(uintptr_t), FALSE},
	{"Test4", data4, sizeof(data4) / sizeof(uintptr_t), FALSE},
	{"Test5", data5, sizeof(data5) / sizeof(uintptr_t), FALSE},
	{"Test6", data6, sizeof(data6) / sizeof(uintptr_t), FALSE},
	{"Test6", data7, sizeof(data7) / sizeof(uintptr_t), FALSE},
};

static void showResult(OMRPortLibrary *portlib, uintptr_t passCount, uintptr_t failCount, int32_t numSuitesNotRun);

class AVLTest: public ::testing::TestWithParam<AVLParams>
{
};

TEST_P(AVLTest, test)
{
	AVLParams params = GetParam();
	const char *testName = params.testName;
	const char *success = params.success;
	const char *data = params.data;

	ASSERT_EQ(0, buildAndVerifyAVLTree(omrTestEnv->getPortLibrary(), success, data)) << "Test verification failed for " << testName;
}

INSTANTIATE_TEST_SUITE_P(OmrAlgoTest, AVLTest, ::testing::ValuesIn(avlParams));

class PoolTest: public ::testing::TestWithParam<PoolInputData>
{
};

TEST_P(PoolTest, test)
{
	PoolInputData params = GetParam();

	ASSERT_EQ(0, createAndVerifyPool(omrTestEnv->getPortLibrary(), &params)) << "Test verification failed for " << params.poolName;
}

INSTANTIATE_TEST_SUITE_P(OmrAlgoTest, PoolTest, ::testing::ValuesIn(poolParams));

TEST(OmrAlgoTest, PoolTestPuddleSharing)
{
	ASSERT_EQ(0, testPoolPuddleListSharing(omrTestEnv->getPortLibrary()));
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

class HashtableTest: public ::testing::TestWithParam<HashtableInputData>
{
};

TEST_P(HashtableTest, Force)
{
	HashtableInputData params = GetParam();
	params.forceCollisions = TRUE;
	params.collisionResistant = FALSE;

	ASSERT_EQ(0, buildAndVerifyHashtable(omrTestEnv->getPortLibrary(), &params)) << "Test verification failed for " << params.hashtableName;
}

TEST_P(HashtableTest, NoForce)
{
	HashtableInputData params = GetParam();
	params.forceCollisions = FALSE;
	params.collisionResistant = FALSE;

	ASSERT_EQ(0, buildAndVerifyHashtable(omrTestEnv->getPortLibrary(), &params)) << "Test verification failed for " << params.hashtableName;
}

INSTANTIATE_TEST_SUITE_P(OmrAlgoTest, HashtableTest, ::testing::ValuesIn(hastableParams));

class CollisionResilientHashtableTest: public ::testing::TestWithParam< ::testing::tuple<HashtableInputData, uint32_t> >
{
};

TEST_P(CollisionResilientHashtableTest, Force)
{
	HashtableInputData params = ::testing::get<0>(GetParam());
	params.forceCollisions = TRUE;
	params.collisionResistant = TRUE;
	params.listToTreeThreshold = ::testing::get<1>(GetParam());

	ASSERT_EQ(0, buildAndVerifyHashtable(omrTestEnv->getPortLibrary(), &params)) << "Test verification failed for " << params.hashtableName;
}

TEST_P(CollisionResilientHashtableTest, NoForce)
{
	HashtableInputData params = ::testing::get<0>(GetParam());
	params.forceCollisions = FALSE;
	params.collisionResistant = TRUE;
	params.listToTreeThreshold = ::testing::get<1>(GetParam());

	ASSERT_EQ(0, buildAndVerifyHashtable(omrTestEnv->getPortLibrary(), &params)) << "Test verification failed for " << params.hashtableName;
}

INSTANTIATE_TEST_SUITE_P(OmrAlgoTest, CollisionResilientHashtableTest,
	::testing::Combine(
		::testing::ValuesIn(hastableParams),
		::testing::ValuesIn(listToTreeThresholdValues)
	)
);

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
