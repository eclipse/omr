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

struct Attribute {
	const char *name;
	J9CudaDeviceAttribute attribute;
	int32_t minValue;
	int32_t maxValue;
};

#define ATTRIBUTE_ENTRY(name, minValue, maxValue) \
		{ # name, J9CUDA_DEVICE_ATTRIBUTE_ ## name, minValue, maxValue }

static const Attribute
deviceAttributes[] = {
	ATTRIBUTE_ENTRY(ASYNC_ENGINE_COUNT,                    0, 2),
	ATTRIBUTE_ENTRY(CAN_MAP_HOST_MEMORY,                   0, 1),
	ATTRIBUTE_ENTRY(CLOCK_RATE /*kHz*/,                    0, 5 * 1024 * 1024),
	ATTRIBUTE_ENTRY(COMPUTE_CAPABILITY,                    0, 70),
	ATTRIBUTE_ENTRY(COMPUTE_CAPABILITY_MAJOR,              0, 7),
	ATTRIBUTE_ENTRY(COMPUTE_CAPABILITY_MINOR,              0, 5),
	ATTRIBUTE_ENTRY(COMPUTE_MODE,                          0, 3),
	ATTRIBUTE_ENTRY(CONCURRENT_KERNELS,                    0, 1),
	ATTRIBUTE_ENTRY(ECC_ENABLED,                           0, 1),
	ATTRIBUTE_ENTRY(GLOBAL_MEMORY_BUS_WIDTH,               128, 384),
	ATTRIBUTE_ENTRY(INTEGRATED,                            0, 1),
	ATTRIBUTE_ENTRY(KERNEL_EXEC_TIMEOUT,                   0, 1),
	ATTRIBUTE_ENTRY(L2_CACHE_SIZE,                         0, 4 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MAX_BLOCK_DIM_X,                       512, 1024),
	ATTRIBUTE_ENTRY(MAX_BLOCK_DIM_Y,                       512, 1024),
	ATTRIBUTE_ENTRY(MAX_BLOCK_DIM_Z,                       64, 64),
	ATTRIBUTE_ENTRY(MAX_GRID_DIM_X,                        65535, 2147483647),
	ATTRIBUTE_ENTRY(MAX_GRID_DIM_Y,                        65535, 65535),
	ATTRIBUTE_ENTRY(MAX_GRID_DIM_Z,                        1, 65535),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE1D_LAYERED_LAYERS,      0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE1D_LAYERED_WIDTH,       0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE1D_WIDTH,               0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_HEIGHT,              0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_LAYERED_HEIGHT,      0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_LAYERED_LAYERS,      0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_LAYERED_WIDTH,       0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE2D_WIDTH,               0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE3D_DEPTH,               0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE3D_HEIGHT,              0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACE3D_WIDTH,               0, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS, 0, 2046),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH,  0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_SURFACECUBEMAP_WIDTH,          0, 32 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_LAYERED_LAYERS,      512, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_LAYERED_WIDTH,       8 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_LINEAR_WIDTH,        128 * 1024 * 1024, 128 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH,     8 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE1D_WIDTH,               8 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_GATHER_HEIGHT,       0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_GATHER_WIDTH,        0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_HEIGHT,              32 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LAYERED_HEIGHT,      8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LAYERED_LAYERS,      512, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LAYERED_WIDTH,       8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LINEAR_HEIGHT,       65000, 65000),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LINEAR_PITCH,        1 * 1024, 1 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_LINEAR_WIDTH,        65000, 65000),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT,    8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH,     8 * 1024, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE2D_WIDTH,               64 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_DEPTH,               2 * 1024, 4 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE,     0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_HEIGHT,              2 * 1024, 4 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE,    0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_WIDTH,               2 * 1024, 4 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE,     0, 2 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS, 0, 2046),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH,  0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAXIMUM_TEXTURECUBEMAP_WIDTH,          0, 16 * 1024),
	ATTRIBUTE_ENTRY(MAX_PITCH,                             2147483647, 2147483647),
	ATTRIBUTE_ENTRY(MAX_REGISTERS_PER_BLOCK,               16 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAX_SHARED_MEMORY_PER_BLOCK,           16 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(MAX_THREADS_PER_BLOCK,                 512, 1 * 1024),
	ATTRIBUTE_ENTRY(MAX_THREADS_PER_MULTIPROCESSOR,        512, 2 * 1024),
	ATTRIBUTE_ENTRY(MEMORY_CLOCK_RATE /*kHz*/,             500 * 1024, 6 * 1024 * 1024),
	ATTRIBUTE_ENTRY(MULTIPROCESSOR_COUNT,                  1, 64),
	ATTRIBUTE_ENTRY(PCI_BUS_ID,                            0, 255),
	ATTRIBUTE_ENTRY(PCI_DEVICE_ID,                         0, 31),
	ATTRIBUTE_ENTRY(PCI_DOMAIN_ID,                         0, 255),
	ATTRIBUTE_ENTRY(STREAM_PRIORITIES_SUPPORTED,           0, 1),
	ATTRIBUTE_ENTRY(SURFACE_ALIGNMENT,                     1, 1 * 1024),
	ATTRIBUTE_ENTRY(TCC_DRIVER,                            0, 1),
	ATTRIBUTE_ENTRY(TEXTURE_ALIGNMENT,                     256, 1 * 1024),
	ATTRIBUTE_ENTRY(TEXTURE_PITCH_ALIGNMENT,               32, 1 * 1024),
	ATTRIBUTE_ENTRY(TOTAL_CONSTANT_MEMORY,                 64 * 1024, 64 * 1024),
	ATTRIBUTE_ENTRY(UNIFIED_ADDRESSING,                    0, 1),
	ATTRIBUTE_ENTRY(WARP_SIZE,                             32, 32),
	{ NULL, (J9CudaDeviceAttribute)0, 0, 0 } /* terminator */
};

#undef ATTRIBUTE_ENTRY

/**
 * Verify querying device attributes.
 */
TEST_F(CudaDeviceTest, attribute)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		for (const Attribute *entry = deviceAttributes; NULL != entry->name; ++entry) {
			int32_t value = 0;
			int32_t rc = omrcuda_deviceGetAttribute(deviceId, entry->attribute, &value);

			ASSERT_EQ(0, rc) << "omrcuda_deviceGetAttribute(" << entry->attribute << ") failed";

			ASSERT_GE(value, entry->minValue);
			ASSERT_LE(value, entry->maxValue);
		}
	}
}

/**
 * Verify querying/setting device cache configuration.
 */
TEST_F(CudaDeviceTest, cacheConfig)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		J9CudaCacheConfig cacheConfig = J9CUDA_CACHE_CONFIG_PREFER_EQUAL;
		int32_t rc = 0;

		rc = omrcuda_deviceGetCacheConfig(deviceId, &cacheConfig);

		ASSERT_EQ(0, rc) << "omrcuda_deviceGetCacheConfig failed";

		for (size_t index = 0; index < allCacheConfigs.length; ++index) {
			rc = omrcuda_deviceSetCacheConfig(deviceId, allCacheConfigs.data[index]);

			ASSERT_EQ(0, rc) << "omrcuda_deviceSetCacheConfig(" << allCacheConfigs.data[index] << ") failed";
		}
	}
}

typedef struct Limit {
	const char *name;
	J9CudaDeviceLimit limit;
	int32_t minComputeCapability;
	uint32_t minValue;
	uint32_t maxValue;
} Limit;

#define LIMIT_ENTRY(name, minComputeCapability, minValue, maxValue) \
		{ # name, J9CUDA_DEVICE_LIMIT_ ## name, minComputeCapability, minValue, maxValue }

const Limit
deviceLimits[] = {
	LIMIT_ENTRY(DEV_RUNTIME_PENDING_LAUNCH_COUNT, 35, 4, 4 * 1024),
	LIMIT_ENTRY(DEV_RUNTIME_SYNC_DEPTH,           35, 0, 16),
	LIMIT_ENTRY(MALLOC_HEAP_SIZE,                 20, 4 * 1024, 16 * 1024 * 1024),
	LIMIT_ENTRY(PRINTF_FIFO_SIZE,                 20, 4 * 1024, 8 * 1024 * 1024),
	LIMIT_ENTRY(STACK_SIZE,                       20, 1 * 1024, 256 * 1024),
	{ NULL, (J9CudaDeviceLimit)0, 0, 0 } /* terminator */
};

#undef LIMIT_ENTRY

/**
 * Verify querying/setting device limits.
 */
TEST_F(CudaDeviceTest, limit)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		int32_t computeCapability = 0;
		int32_t rc = 0;

		rc = omrcuda_deviceGetAttribute(deviceId, J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY, &computeCapability);

		ASSERT_EQ(0, rc) << "omrcuda_deviceGetAttribute(compute_capability) failed";

		for (const Limit *entry = deviceLimits; NULL != entry->name; ++entry) {
			uintptr_t value = 0;

			rc = omrcuda_deviceGetLimit(deviceId, entry->limit, &value);

			if (computeCapability < entry->minComputeCapability) {
				ASSERT_EQ(J9CUDA_ERROR_UNSUPPORTED_LIMIT, rc) << "omrcuda_deviceGetLimit should have failed";
			} else {
				ASSERT_EQ(0, rc) << "omrcuda_deviceGetLimit failed";

				ASSERT_GE(value, entry->minValue);
				ASSERT_LE(value, entry->maxValue);

				/* compute the median of the legal range */
				value = entry->minValue + ((entry->maxValue - entry->minValue) / 2);

				rc = omrcuda_deviceSetLimit(deviceId, entry->limit, value);

				ASSERT_EQ(0, rc) << "omrcuda_deviceSetLimit failed";
			}
		}
	}
}

/**
 * Verify querying device memory information.
 */
TEST_F(CudaDeviceTest, memoryInfo)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		uintptr_t freeBytes = 0;
		uintptr_t totalBytes = 0;

		int32_t rc = omrcuda_deviceGetMemInfo(deviceId, &freeBytes, &totalBytes);

		ASSERT_EQ(0, rc) << "omrcuda_deviceGetMemInfo failed";
		ASSERT_LE(freeBytes, totalBytes);
	}
}

/**
 * Verify querying device name.
 */
TEST_F(CudaDeviceTest, name)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		char deviceName[200];
		int32_t rc = omrcuda_deviceGetName(deviceId, sizeof(deviceName), deviceName);

		ASSERT_EQ(0, rc) << "omrcuda_deviceGetName failed";
	}
}

/**
 * Verify querying/setting device shared memory configuration.
 */
TEST_F(CudaDeviceTest, sharedMemoryConfig)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		J9CudaSharedMemConfig sharedMemConfig = J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
		int32_t rc = 0;

		rc = omrcuda_deviceGetSharedMemConfig(deviceId, &sharedMemConfig);

		ASSERT_EQ(0, rc) << "omrcuda_deviceGetSharedMemConfig failed";

		for (size_t index = 0; index < allSharedMemConfigs.length; ++index) {
			rc = omrcuda_deviceSetSharedMemConfig(deviceId, allSharedMemConfigs.data[index]);

			ASSERT_EQ(0, rc) << "omrcuda_deviceSetSharedMemConfig(" << allSharedMemConfigs.data[index] << ") failed";
		}
	}
}

/**
 * Verify querying device stream priority range.
 */
TEST_F(CudaDeviceTest, streamPriorityRange)
{
	OMRPORT_ACCESS_FROM_OMRPORT(getPortLibrary());

	for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
		int32_t leastPriority = 0;
		int32_t greatestPriority = 0;
		int32_t rc = omrcuda_deviceGetStreamPriorityRange(deviceId, &leastPriority, &greatestPriority);

		ASSERT_EQ(0, rc) << "omrcuda_deviceGetStreamPriorityRange failed";
		ASSERT_LE(greatestPriority, leastPriority);
	}
}
