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
 * Verify proper behavior using invalid device identifiers.
 */
TEST_F(CudaDeviceTest, invalidDevices)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	int32_t const expectedError = (0 == deviceCount) ? J9CUDA_ERROR_NO_DEVICE : J9CUDA_ERROR_INVALID_DEVICE;
	uint32_t deviceId = deviceCount;
	uint32_t deviceIdBit = 1;
	J9CudaCacheConfig cacheConfig = J9CUDA_CACHE_CONFIG_PREFER_EQUAL;
	J9CudaStreamCallback callback = NULL;
	J9CudaDeviceAttribute deviceAttribute = J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY;
	J9CudaEvent event = NULL;
	J9CudaFunction function = NULL;
	J9CudaFunctionAttribute functionAttribute = J9CUDA_FUNCTION_ATTRIBUTE_BINARY_VERSION;
	J9CudaDeviceLimit limit = J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT;
	J9CudaLinker linker = NULL;
	J9CudaModule module = NULL;
	J9CudaSharedMemConfig sharedMemConfig = J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
	J9CudaStream stream = NULL;
	J9CudaJitInputType type = J9CUDA_JIT_INPUT_TYPE_CUBIN;
	BOOLEAN boolVal = FALSE;
	int32_t i32Val = 0;
	uint32_t u32Val = 0;
	uintptr_t ptrVal = 0;
	void *address = NULL;
	char name[40];

	for (;;) {
		int32_t rc = 0;

#define DO_BAD_CALL(func, args)                    \
		do {                                       \
			rc = omrcuda_ ## func args;             \
			ASSERT_EQ(expectedError, rc) << #func; \
		} while (0)

		DO_BAD_CALL(deviceAlloc,                  (deviceId, ptrVal, &address));
		DO_BAD_CALL(deviceCanAccessPeer,          (deviceId, u32Val, &boolVal));
		DO_BAD_CALL(deviceDisablePeerAccess,      (deviceId, u32Val));
		DO_BAD_CALL(deviceEnablePeerAccess,       (deviceId, u32Val));
		DO_BAD_CALL(deviceFree,                   (deviceId, address));
		DO_BAD_CALL(deviceGetAttribute,           (deviceId, deviceAttribute, &i32Val));
		DO_BAD_CALL(deviceGetCacheConfig,         (deviceId, &cacheConfig));
		DO_BAD_CALL(deviceGetLimit,               (deviceId, limit, &ptrVal));
		DO_BAD_CALL(deviceGetMemInfo,             (deviceId, &ptrVal, &ptrVal));
		DO_BAD_CALL(deviceGetName,                (deviceId, sizeof(name), name));
		DO_BAD_CALL(deviceGetSharedMemConfig,     (deviceId, &sharedMemConfig));
		DO_BAD_CALL(deviceGetStreamPriorityRange, (deviceId, &i32Val, &i32Val));
		DO_BAD_CALL(deviceReset,                  (deviceId));
		DO_BAD_CALL(deviceSetCacheConfig,         (deviceId, cacheConfig));
		DO_BAD_CALL(deviceSetLimit,               (deviceId, limit, i32Val));
		DO_BAD_CALL(deviceSetSharedMemConfig,     (deviceId, sharedMemConfig));
		DO_BAD_CALL(deviceSynchronize,            (deviceId));
		DO_BAD_CALL(eventCreate,                  (deviceId, i32Val, &event));
		DO_BAD_CALL(eventDestroy,                 (deviceId, event));
		DO_BAD_CALL(eventRecord,                  (deviceId, event, stream));
		DO_BAD_CALL(funcGetAttribute,             (deviceId, function, functionAttribute, &i32Val));
		DO_BAD_CALL(funcSetCacheConfig,           (deviceId, function, cacheConfig));
		DO_BAD_CALL(funcSetSharedMemConfig,       (deviceId, function, sharedMemConfig));
		DO_BAD_CALL(launchKernel,                 (deviceId, function, 1, 1, 1, 1, 1, 1, 0, NULL, NULL));
		DO_BAD_CALL(linkerAddData,                (deviceId, linker, type, address, ptrVal, name, NULL));
		DO_BAD_CALL(linkerComplete,               (deviceId, linker, &address, &ptrVal));
		DO_BAD_CALL(linkerCreate,                 (deviceId, NULL, &linker));
		DO_BAD_CALL(linkerDestroy,                (deviceId, linker));
		DO_BAD_CALL(memcpyDeviceToDevice,         (deviceId, NULL, NULL, ptrVal));
		DO_BAD_CALL(memcpyDeviceToHost,           (deviceId, NULL, NULL, ptrVal));
		DO_BAD_CALL(memcpyHostToDevice,           (deviceId, NULL, NULL, ptrVal));
		DO_BAD_CALL(memcpyPeer,                   (deviceId, NULL, u32Val, NULL, ptrVal));
		DO_BAD_CALL(memset8,                      (deviceId, address, i32Val, ptrVal));
		DO_BAD_CALL(memset16,                     (deviceId, address, i32Val, ptrVal));
		DO_BAD_CALL(memset32,                     (deviceId, address, i32Val, ptrVal));
		DO_BAD_CALL(moduleGetFunction,            (deviceId, module, name, &function));
		DO_BAD_CALL(moduleGetGlobal,              (deviceId, module, name, &ptrVal, &ptrVal));
		DO_BAD_CALL(moduleGetSurfaceRef,          (deviceId, module, name, &ptrVal));
		DO_BAD_CALL(moduleGetTextureRef,          (deviceId, module, name, &ptrVal));
		DO_BAD_CALL(moduleLoad,                   (deviceId, address, NULL, &module));
		DO_BAD_CALL(moduleUnload,                 (deviceId, module));
		DO_BAD_CALL(streamAddCallback,            (deviceId, stream, callback, 0));
		DO_BAD_CALL(streamCreate,                 (deviceId, &stream));
		DO_BAD_CALL(streamCreateWithPriority,     (deviceId, i32Val, i32Val, &stream));
		DO_BAD_CALL(streamDestroy,                (deviceId, stream));
		DO_BAD_CALL(streamGetFlags,               (deviceId, stream, &u32Val));
		DO_BAD_CALL(streamGetPriority,            (deviceId, stream, &i32Val));
		DO_BAD_CALL(streamQuery,                  (deviceId, stream));
		DO_BAD_CALL(streamSynchronize,            (deviceId, stream));
		DO_BAD_CALL(streamWaitEvent,              (deviceId, stream, event));

#undef DO_BAD_CALL

		while (0 != (deviceId & deviceIdBit)) {
			deviceIdBit <<= 1;
		}

		if (0 == deviceIdBit) {
			break;
		}

		deviceId |= deviceIdBit;
		deviceIdBit <<= 1;
	}
}
