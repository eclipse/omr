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
 * Verify device peering API.
 */
TEST_F(CudaDeviceTest, peer)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uint32_t peerDeviceId = 0;

		for (peerDeviceId = 0; peerDeviceId < deviceCount; ++peerDeviceId) {
			BOOLEAN canAccess = FALSE;
			int32_t rc = 0;

			if (peerDeviceId == deviceId) {
				continue;
			}

			rc = omrcuda_deviceCanAccessPeer(deviceId, peerDeviceId, &canAccess);

			ASSERT_EQ(0, rc) << "omrcuda_deviceCanAccessPeer failed";

			/* Toggle peer access on then off or, off then on. */

			if (!canAccess) {
				rc = omrcuda_deviceEnablePeerAccess(deviceId, peerDeviceId);

				if (J9CUDA_ERROR_PEER_ACCESS_UNSUPPORTED == rc) {
					continue;
				}

				ASSERT_EQ(0, rc) << "omrcuda_deviceEnablePeerAccess failed";
			}

			rc = omrcuda_deviceDisablePeerAccess(deviceId, peerDeviceId);

			ASSERT_EQ(0, rc) << "omrcuda_deviceDisablePeerAccess failed";

			testPeerTransfer(OMRPORTLIB, deviceId, peerDeviceId);

			if (canAccess) {
				rc = omrcuda_deviceEnablePeerAccess(deviceId, peerDeviceId);

				ASSERT_EQ(0, rc) << "omrcuda_deviceEnablePeerAccess failed";
			}
		}
	}
}

/**
 * Verify peer transfer API.
 *
 * @param[in] portLibrary   the port library under test
 * @param[in] deviceId      the current device
 * @param[in] peerDeviceId  the peer device
 */
void
CudaDeviceTest::testPeerTransfer(OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	uintptr_t const BufferBytes = 2 * 1024 * 1024;
	void *hostBuf = NULL;
	int32_t rc = 0;

	rc = omrcuda_hostAlloc(BufferBytes, J9CUDA_HOST_ALLOC_DEFAULT, &hostBuf);

	ASSERT_EQ(0, rc) << "omrcuda_hostAlloc failed";
	ASSERT_NOT_NULL(hostBuf) << "allocated null host address";

	void *deviceBuf = NULL;

	rc = omrcuda_deviceAlloc(deviceId, BufferBytes, &deviceBuf);

	ASSERT_EQ(0, rc) << "omrcuda_deviceAlloc failed";
	ASSERT_NOT_NULL(deviceBuf) << "allocated null device address";

	void *peerBuf = NULL;

	rc = omrcuda_deviceAlloc(peerDeviceId, BufferBytes, &peerBuf);

	ASSERT_EQ(0, rc) << "omrcuda_deviceAlloc failed";
	ASSERT_NOT_NULL(peerBuf) << "allocated null device address";

	patternFill(hostBuf, BufferBytes, deviceId);

	rc = omrcuda_memcpyHostToDevice(deviceId, deviceBuf, hostBuf, BufferBytes);

	ASSERT_EQ(0, rc) << "omrcuda_memcpyHostToDevice failed";

	rc = omrcuda_memcpyPeer(peerDeviceId, peerBuf, deviceId, deviceBuf, BufferBytes);

	ASSERT_EQ(0, rc) << "omrcuda_memcpyPeer failed";

	memset(hostBuf, 0, BufferBytes);

	rc = omrcuda_memcpyDeviceToHost(peerDeviceId, hostBuf, peerBuf, BufferBytes);

	ASSERT_EQ(0, rc) << "omrcuda_memcpyDeviceToHost failed";

	ASSERT_TRUE(patternVerify(hostBuf, BufferBytes, deviceId)) << "data transferred does not match expected pattern";

	if (NULL != hostBuf) {
		rc = omrcuda_hostFree(hostBuf);

		ASSERT_EQ(0, rc) << "omrcuda_hostFree failed";
	}

	if (NULL != deviceBuf) {
		rc = omrcuda_deviceFree(deviceId, deviceBuf);

		ASSERT_EQ(0, rc) << "omrcuda_deviceFree failed";
	}

	if (NULL != peerBuf) {
		rc = omrcuda_deviceFree(peerDeviceId, peerBuf);

		ASSERT_EQ(0, rc) << "omrcuda_deviceFree failed";
	}
}
