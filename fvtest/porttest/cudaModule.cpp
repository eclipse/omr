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

static const J9CudaFunctionAttribute
functionAttributes[] = {
	J9CUDA_FUNCTION_ATTRIBUTE_BINARY_VERSION,
	J9CUDA_FUNCTION_ATTRIBUTE_CONST_SIZE_BYTES,
	J9CUDA_FUNCTION_ATTRIBUTE_LOCAL_SIZE_BYTES,
	J9CUDA_FUNCTION_ATTRIBUTE_MAX_THREADS_PER_BLOCK,
	J9CUDA_FUNCTION_ATTRIBUTE_NUM_REGS,
	J9CUDA_FUNCTION_ATTRIBUTE_PTX_VERSION,
	J9CUDA_FUNCTION_ATTRIBUTE_SHARED_SIZE_BYTES
};

static uintptr_t
variableDynamicSharedMemorySize(uint32_t blockSize, uintptr_t userData)
{
	return blockSize * userData;
}

/**
 * Verify function API.
 *
 * @param[in] portLibrary the port library under test
 * @param[in] deviceId the current device
 * @param[in] function the function to test
 */
void
CudaDeviceTest::testFunction(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	for (size_t index = 0; index < LENGTH_OF(functionAttributes); ++index) {
		int32_t value = 0;
		int32_t rc = omrcuda_funcGetAttribute(deviceId, function, functionAttributes[index], &value);

		ASSERT_EQ(0, rc) << "omrcuda_funcGetAttribute(" << functionAttributes[index] << ") failed";
	}

	for (size_t index = 0; index < allCacheConfigs.length; ++index) {
		int32_t rc = omrcuda_funcSetCacheConfig(deviceId, function, allCacheConfigs.data[index]);

		ASSERT_EQ(0, rc) << "omrcuda_funcSetCacheConfig(" << allCacheConfigs.data[index] << ") failed";
	}

	for (size_t index = 0; index < allSharedMemConfigs.length; ++index) {
		int32_t rc = omrcuda_funcSetSharedMemConfig(deviceId, function, allSharedMemConfigs.data[index]);

		ASSERT_EQ(0, rc) << "omrcuda_funcSetSharedMemConfig(" << allSharedMemConfigs.data[index] << ") failed";
	}

	/* occupancy */
	{
		uint32_t blockSize = 384;
		uint32_t dynamicSharedMemorySize = 1024;
		uint32_t flags = J9CUDA_OCCUPANCY_DEFAULT;
		uint32_t maxActiveBlocks = 0;

		int32_t rc = omrcuda_funcMaxActiveBlocksPerMultiprocessor(deviceId, function, blockSize,
					 dynamicSharedMemorySize, flags, &maxActiveBlocks);

		if (J9CUDA_ERROR_NOT_SUPPORTED != rc) {
			ASSERT_EQ(0, rc) << "omrcuda_funcMaxActiveBlocksPerMultiprocessor() failed";
		}
	}

	{
		uint32_t fixedDynamicSharedMemorySize = 2048;
		uint32_t flags = J9CUDA_OCCUPANCY_DISABLE_CACHING_OVERRIDE;
		uintptr_t userData = 256;
		uint32_t blockSizeLimit = 0;
		uint32_t minGridSize = 0;
		uint32_t maxBlockSize = 0;

		int32_t rc = omrcuda_funcMaxPotentialBlockSize(deviceId, function,
					 NULL, fixedDynamicSharedMemorySize,
					 blockSizeLimit, flags, &minGridSize, &maxBlockSize);

		if (J9CUDA_ERROR_NOT_SUPPORTED != rc) {
			ASSERT_EQ(0, rc) << "omrcuda_funcMaxPotentialBlockSize(fixed) failed";
		}

		rc = omrcuda_funcMaxPotentialBlockSize(deviceId, function,
											   variableDynamicSharedMemorySize, userData,
											   blockSizeLimit, flags, &minGridSize, &maxBlockSize);

		if (J9CUDA_ERROR_NOT_SUPPORTED != rc) {
			ASSERT_EQ(0, rc) << "omrcuda_funcMaxPotentialBlockSize(variable) failed";
		}
	}
}

/**
 * Verify module API.
 */
TEST_F(CudaDeviceTest, module)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uintptr_t address;
		J9CudaFunction function = NULL;
		J9CudaModule module = NULL;
		int32_t rc = 0;
		uintptr_t size;

		rc = omrcuda_moduleLoad(deviceId, ptxModule.data, NULL, &module);

		ASSERT_EQ(0, rc) << "omrcuda_moduleLoad failed";
		ASSERT_NOT_NULL(module) << "created null module";

		rc = omrcuda_moduleGetFunction(deviceId, module, "stepFirst", &function);

		ASSERT_EQ(0, rc) << "omrcuda_moduleGetFunction failed";
		ASSERT_NOT_NULL(function) << "null function address";

		testFunction(OMRPORTLIB, deviceId, function);

		rc = omrcuda_moduleGetFunction(deviceId, module, "swapCount", &function);

		ASSERT_NE(0, rc) << "omrcuda_moduleGetFunction should have failed";

		rc = omrcuda_moduleGetGlobal(deviceId, module, "swapCount", &address, &size);

		ASSERT_EQ(0, rc) << "omrcuda_moduleGetGlobal failed";
		ASSERT_NE(0U, address) << "null synmbol address";
		ASSERT_NE(0U, size) << "global too small";

		rc = omrcuda_moduleGetSurfaceRef(deviceId, module, "surfaceHandle", &address);

		ASSERT_EQ(0, rc) << "omrcuda_moduleGetSurfaceRef failed";
		ASSERT_NE(0U, address) << "null surface address";

		rc = omrcuda_moduleGetTextureRef(deviceId, module, "textureHandle", &address);

		ASSERT_EQ(0, rc) << "omrcuda_moduleGetTextureRef failed";
		ASSERT_NE(0U, address) << "null texture address";

		rc = omrcuda_moduleUnload(deviceId, module);

		ASSERT_EQ(0, rc) << "omrcuda_moduleUnload failed";
	}
}
