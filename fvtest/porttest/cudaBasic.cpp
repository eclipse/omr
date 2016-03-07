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
 * Verify basic functions.
 *
 * Ensure that the number of devices present can be determined.
 * If there are any devices, verify that we can query the driver
 * and runtime API versions.
 */
TEST_F(CudaTest, basic)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	int32_t rc = 0;
	uint32_t deviceCount = 0;

	rc = omrcuda_deviceGetCount(&deviceCount);

	ASSERT_EQ(0, rc) << "omrcuda_deviceGetCount failed";

	outputComment(OMRPORTLIB, "omrcuda: found %d device%s\n", deviceCount, (1 == deviceCount) ? "" : "s");

	uint32_t driverVersion = 0;
	uint32_t runtimeVersion = 0;

	if (0 != deviceCount) {
		rc = omrcuda_driverGetVersion(&driverVersion);

		ASSERT_EQ(0, rc) << "omrcuda_driverGetVersion failed";

		rc = omrcuda_runtimeGetVersion(&runtimeVersion);

		ASSERT_EQ(0, rc) << "omrcuda_runtimeGetVersion failed";
	}

	J9CudaConfig *config = OMRPORTLIB->cuda_configData;

	ASSERT_NOT_NULL(config) << "omrcuda config data is missing";
	ASSERT_NOT_NULL(config->getSummaryData) << "omrcuda getSummaryData is missing";

	J9CudaSummaryDescriptor summary;

	rc = config->getSummaryData(OMRPORTLIB, &summary);

	ASSERT_EQ(0, rc) << "omrcuda getSummaryData failed";
	ASSERT_EQ(deviceCount, summary.deviceCount);
	ASSERT_EQ(normalizeVersion(driverVersion), summary.driverVersion);
	ASSERT_EQ(normalizeVersion(runtimeVersion), summary.runtimeVersion);
	ASSERT_NOT_NULL(config->getDeviceData) << "omrcuda getDeviceData is missing";

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		J9CudaDeviceDescriptor detail;

		rc = config->getDeviceData(OMRPORTLIB, deviceId, &detail);

		ASSERT_EQ(0, rc) << "omrcuda getDeviceData failed";
	}

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		rc = omrcuda_deviceReset(deviceId);

		ASSERT_EQ(0, rc) << "omrcuda omrcuda_deviceReset failed";
	}
}
