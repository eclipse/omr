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

#include "cudaTests.hpp"

/**
 * Verify event API.
 */
TEST_F(CudaDeviceTest, events)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		J9CudaEvent event1 = NULL;
		J9CudaEvent event2 = NULL;
		float elapsedMillis = 0;
		int32_t rc = 0;

		rc = omrcuda_eventCreate(deviceId, J9CUDA_EVENT_FLAG_DEFAULT, &event1);

		ASSERT_EQ(0, rc) << "omrcuda_eventCreate failed";
		ASSERT_NOT_NULL(event1) << "created null event";

		rc = omrcuda_eventCreate(deviceId, J9CUDA_EVENT_FLAG_DEFAULT, &event2);

		ASSERT_EQ(0, rc) << "omrcuda_eventCreate failed";
		ASSERT_NOT_NULL(event2) << "created null event";

		rc = omrcuda_eventRecord(deviceId, event1, NULL);

		ASSERT_EQ(0, rc) << "omrcuda_eventRecord failed";

		rc = omrcuda_eventRecord(deviceId, event2, NULL);

		ASSERT_EQ(0, rc) << "omrcuda_eventRecord failed";

		rc = omrcuda_eventSynchronize(event2);

		ASSERT_EQ(0, rc) << "omrcuda_eventSynchronize failed";

		rc = omrcuda_eventQuery(event1);

		ASSERT_EQ(0, rc) << "omrcuda_eventQuery failed";

		rc = omrcuda_eventQuery(event2);

		ASSERT_EQ(0, rc) << "omrcuda_eventQuery failed";

		rc = omrcuda_eventElapsedTime(event1, event2, &elapsedMillis);

		ASSERT_EQ(0, rc) << "omrcuda_eventElapsedTime failed";
		ASSERT_LT(0, elapsedMillis) << "elapsed time should be non-negative";

		if (NULL != event1) {
			rc = omrcuda_eventDestroy(deviceId, event1);

			ASSERT_EQ(0, rc) << "omrcuda_eventDestroy failed";
		}

		if (NULL != event2) {
			rc = omrcuda_eventDestroy(deviceId, event2);

			ASSERT_EQ(0, rc) << "omrcuda_eventDestroy failed";
		}
	}
}
