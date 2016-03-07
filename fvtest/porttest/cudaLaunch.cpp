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
 * A structure populated by a device through use of ptxParameters/store.
 */
struct Packed {
	double doubleValue;
	int64_t longValue;
	uint64_t pointer;
	float floatValue;
	int32_t intValue;
	uint16_t charValue;
	int16_t shortValue;
	int8_t byteValue;
};

/**
 * Verify launch API.
 */
TEST_F(CudaDeviceTest, launching)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uintptr_t bufferSize = sizeof(Packed);
		void *deviceBuf = NULL;
		Packed expected;
		J9CudaFunction function = NULL;
		void *kernelParms[9];
		J9CudaModule module = NULL;
		int32_t rc = 0;
		Packed result;

		rc = omrcuda_moduleLoad(deviceId, ptxParameters.data, NULL, &module);

		ASSERT_EQ(0, rc) << "omrcuda_moduleLoad failed";
		ASSERT_NOT_NULL(module) << "created null module";

		rc = omrcuda_moduleGetFunction(deviceId, module, "store", &function);

		ASSERT_EQ(0, rc) << "omrcuda_moduleGetFunction failed";
		ASSERT_NOT_NULL(function) << "null function address";

		rc = omrcuda_deviceAlloc(deviceId, bufferSize, &deviceBuf);

		ASSERT_EQ(0, rc) << "omrcuda_deviceAlloc failed";
		ASSERT_NOT_NULL(deviceBuf) << "allocated null device address";

		expected.byteValue   = 85;
		expected.charValue   = 33;
		expected.doubleValue = 3.14159265358979323846; /* pi */
		expected.floatValue  = 2.71828182845904523536f; /* e */
		expected.intValue    = 28;
		expected.longValue   = 42;
		expected.pointer     = (uintptr_t)deviceBuf;
		expected.shortValue  = 85;

		kernelParms[0] = &deviceBuf;
		kernelParms[1] = &bufferSize;
		kernelParms[2] = &expected.byteValue;
		kernelParms[3] = &expected.charValue;
		kernelParms[4] = &expected.doubleValue;
		kernelParms[5] = &expected.floatValue;
		kernelParms[6] = &expected.intValue;
		kernelParms[7] = &expected.longValue;
		kernelParms[8] = &expected.shortValue;

		rc = omrcuda_launchKernel(deviceId, function, 1, 1, 1, 32, 1, 1, 0, NULL, kernelParms);

		ASSERT_EQ(0, rc) << "omrcuda_launchKernel failed";

		rc = omrcuda_deviceSynchronize(deviceId);

		ASSERT_EQ(0, rc) << "omrcuda_deviceSynchronize failed";

		rc = omrcuda_memcpyDeviceToHost(deviceId, &result, deviceBuf, bufferSize);

		ASSERT_EQ(0, rc) << "omrcuda_memcpyDeviceToHost failed";

		ASSERT_DOUBLE_EQ(expected.doubleValue, result.doubleValue) << "doubleValue";
		ASSERT_EQ(expected.longValue, result.longValue) << "longValue";
		ASSERT_EQ(expected.pointer, result.pointer) << "pointer";
		ASSERT_FLOAT_EQ(expected.floatValue, result.floatValue) << "floatValue";
		ASSERT_EQ(expected.intValue, result.intValue) << "intValue";
		ASSERT_EQ(expected.charValue, result.charValue) << "charValue";
		ASSERT_EQ(expected.shortValue, result.shortValue) << "shortValue";
		ASSERT_EQ(expected.byteValue, result.byteValue) << "byteValue";

		if (NULL != deviceBuf) {
			rc = omrcuda_deviceFree(deviceId, deviceBuf);

			ASSERT_EQ(0, rc) << "omrcuda_deviceFree failed";
		}

		rc = omrcuda_moduleUnload(deviceId, module);

		ASSERT_EQ(0, rc) << "omrcuda_moduleUnload failed";
	}
}
