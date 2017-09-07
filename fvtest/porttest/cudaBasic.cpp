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
