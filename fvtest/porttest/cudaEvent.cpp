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
