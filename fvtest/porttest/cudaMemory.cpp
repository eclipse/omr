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
 * Verify device global memory API.
 */
TEST_F(CudaDeviceTest, memory)
{
	if (0 == deviceCount) {
		return;
	}

	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	uintptr_t const BufferBytes = 2 * 1024 * 1024;
	int32_t rc = 0;
	void *hostBuf1 = NULL;
	void *hostBuf2 = NULL;

	rc = omrcuda_hostAlloc(BufferBytes, J9CUDA_HOST_ALLOC_DEFAULT, &hostBuf1);

	ASSERT_EQ(0, rc) << "omrcuda_hostAlloc failed";
	ASSERT_NOT_NULL(hostBuf1) << "allocated null host address";

	rc = omrcuda_hostAlloc(BufferBytes, J9CUDA_HOST_ALLOC_DEFAULT, &hostBuf2);

	ASSERT_EQ(0, rc) << "omrcuda_hostAlloc failed";
	ASSERT_NOT_NULL(hostBuf2) << "allocated null host address";

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		void *deviceBuf1 = NULL;
		void *deviceBuf2 = NULL;
		uint32_t fillValue = 0;

		rc = omrcuda_deviceAlloc(deviceId, BufferBytes, &deviceBuf1);

		ASSERT_EQ(0, rc) << "omrcuda_deviceAlloc failed";
		ASSERT_NOT_NULL(deviceBuf1) << "allocated null device address";

		rc = omrcuda_deviceAlloc(deviceId, BufferBytes, &deviceBuf2);

		ASSERT_EQ(0, rc) << "omrcuda_deviceAlloc failed";
		ASSERT_NOT_NULL(deviceBuf2) << "allocated null device address";

		patternFill(hostBuf1, BufferBytes, deviceId);

		rc = omrcuda_memcpyHostToDevice(deviceId, deviceBuf1, hostBuf1, BufferBytes);

		ASSERT_EQ(0, rc) << "omrcuda_memcpyHostToDevice failed";

		rc = omrcuda_memcpyDeviceToDevice(deviceId, deviceBuf2, deviceBuf1, BufferBytes);

		ASSERT_EQ(0, rc) << "omrcuda_memcpyDeviceToDevice failed";

		rc = omrcuda_memcpyDeviceToHost(deviceId, hostBuf2, deviceBuf2, BufferBytes);

		ASSERT_EQ(0, rc) << "omrcuda_memcpyDeviceToHost failed";

		ASSERT_TRUE(patternVerify(hostBuf2, BufferBytes, deviceId))
				<< "data transferred does not match expected pattern";

		fillValue = 0x23232323;
		rc = omrcuda_memset8(deviceId, deviceBuf1, 0x23, BufferBytes);

		ASSERT_EQ(0, rc) << "omrcuda_memset8 failed";

		rc = omrcuda_memcpyDeviceToHost(deviceId, hostBuf1, deviceBuf1, BufferBytes);

		ASSERT_EQ(0, rc) << "omrcuda_memcpyDeviceToHost failed";

		ASSERT_TRUE(fillVerify(hostBuf1, BufferBytes, &fillValue, 1))
				<< "data transferred does not match expected byte pattern";

		fillValue = 0x45674567;
		rc = omrcuda_memset16(deviceId, deviceBuf1, 0x4567, BufferBytes / 2);

		ASSERT_EQ(0, rc) << "omrcuda_memset16 failed";

		rc = omrcuda_memcpyDeviceToHost(deviceId, hostBuf1, deviceBuf1, BufferBytes);

		ASSERT_EQ(0, rc) << "omrcuda_memcpyDeviceToHost failed";

		ASSERT_TRUE(fillVerify(hostBuf1, BufferBytes, &fillValue, 2))
				<< "data transferred does not match expected byte pattern";

		fillValue = 0xabcddcba;
		rc = omrcuda_memset32(deviceId, deviceBuf1, fillValue, BufferBytes / 4);

		ASSERT_EQ(0, rc) << "omrcuda_memset32 failed";

		rc = omrcuda_memcpyDeviceToHost(deviceId, hostBuf1, deviceBuf1, BufferBytes);

		ASSERT_EQ(0, rc) << "omrcuda_memcpyDeviceToHost failed";

		ASSERT_TRUE(fillVerify(hostBuf1, BufferBytes, &fillValue, 4))
				<< "data transferred does not match expected byte pattern";

		if (NULL != deviceBuf1) {
			rc = omrcuda_deviceFree(deviceId, deviceBuf1);

			ASSERT_EQ(0, rc) << "omrcuda_deviceFree failed";
		}

		if (NULL != deviceBuf2) {
			rc = omrcuda_deviceFree(deviceId, deviceBuf2);

			ASSERT_EQ(0, rc) << "omrcuda_deviceFree failed";
		}
	}

	if (NULL != hostBuf1) {
		rc = omrcuda_hostFree(hostBuf1);

		ASSERT_EQ(0, rc) << "omrcuda_hostFree failed";
	}

	if (NULL != hostBuf2) {
		rc = omrcuda_hostFree(hostBuf2);

		ASSERT_EQ(0, rc) << "omrcuda_hostFree failed";
	}
}
