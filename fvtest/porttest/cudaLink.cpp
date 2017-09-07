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
 * Verify linker API.
 */
TEST_F(CudaDeviceTest, linking)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		int32_t computeCapability = 0;
		void *image = NULL;
		uintptr_t imageSize;
		J9CudaLinker linker = NULL;
		int32_t rc = 0;

		rc = omrcuda_deviceGetAttribute(deviceId, J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY, &computeCapability);

		ASSERT_EQ(0, rc) << "omrcuda_deviceGetAttribute(compute_capability) failed";

		if (20 > computeCapability) {
			/* separate compilation requires compute capability 2.0 or above */
			continue;
		}

		rc = omrcuda_linkerCreate(deviceId, NULL, &linker);

		ASSERT_EQ(0, rc) << "omrcuda_linkerCreate failed";
		ASSERT_NOT_NULL(linker) << "created null linker";

		rc = omrcuda_linkerAddData(deviceId, linker, J9CUDA_JIT_INPUT_TYPE_PTX,
		        ptxFragment1.data, ptxFragment1.length, "fragment1", NULL);

		ASSERT_EQ(0, rc) << "omrcuda_linkerAddData failed";

		rc = omrcuda_linkerAddData(deviceId, linker, J9CUDA_JIT_INPUT_TYPE_PTX,
				ptxFragment2.data, ptxFragment2.length, "fragment2", NULL);

		ASSERT_EQ(0, rc) << "omrcuda_linkerAddData failed";

		rc = omrcuda_linkerComplete(deviceId, linker, &image, &imageSize);

		ASSERT_EQ(0, rc) << "omrcuda_linkerComplete failed";
		ASSERT_NOT_NULL(image) << "null linker image";
		ASSERT_NE(0U, imageSize) << "empty linker image";

		rc = omrcuda_linkerDestroy(deviceId, linker);

		ASSERT_EQ(0, rc) << "omrcuda_linkerDestroy failed";
	}
}
