/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#include "omrTest.h"
#include "omrutilbase.h"
#include "thread_api.h"
#include "thrdsup.h"
#include "threadExtendedTestHelpers.hpp"

#define NUM_ITERATIONS		3
#define TIME_IN_MILLIS		500

/**
 * Generate CPU Load for 1 seconds
 */
static void
cpuLoad(void)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	int64_t start = 0;
	int64_t end = 0;

	start = omrtime_current_time_millis();
	do {
		end = omrtime_current_time_millis();
	} while ((end - start) < TIME_IN_MILLIS);
}

/**
 * Test the monotonicity of omrtime_current_time_millis.
 *
 */
TEST(ThreadExtendedTest, TestTimeBaseMonotonicity)
{
	uint64_t timestampArray[NUM_ITERATIONS];
	uintptr_t i = 0;
	uintptr_t isMonotonic = 1;

	memset(&timestampArray, 0, sizeof(timestampArray));

	for (i = 0; i < NUM_ITERATIONS; i++) {
		cpuLoad();
		timestampArray[i] = GET_HIRES_CLOCK();
		ASSERT_TRUE(timestampArray[i] > 0);
	}

	for (i = 0; i < NUM_ITERATIONS - 1; i++) {
		if (timestampArray[i] > timestampArray[i + 1]) {
			isMonotonic = 0;
			break;
		}
	}

	ASSERT_TRUE(isMonotonic == 1);
}
