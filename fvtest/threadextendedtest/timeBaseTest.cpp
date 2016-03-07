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
#include "omrutilbase.h"
#include "thread_api.h"
#include "thrdsup.h"
#include "threadExtendedTestHelpers.hpp"

#define NUM_ITERATIONS		12
#define FIVE_SEC_IN_MSEC	5000 /**< 5 sec in ms */

/**
 * Generate CPU Load for 5 seconds
 */
static void
cpuLoad(void)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	int64_t start = 0;
	int64_t end = 0;

	start = omrtime_current_time_millis();
	/* Generate CPU Load for 5 seconds */
	do {
		end = omrtime_current_time_millis();
	} while ((end - start) < FIVE_SEC_IN_MSEC);
}

/**
 * Test the monotonicity of the GET_HIRES_CLOCK()/getTimebase() functions.
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
