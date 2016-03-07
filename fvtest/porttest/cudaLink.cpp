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
