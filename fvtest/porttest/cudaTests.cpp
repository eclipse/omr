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

void
CudaDeviceTest::SetUp()
{
	::testing::Test::SetUp();

	OMRPORT_ACCESS_FROM_OMRPORT(portTestEnv->getPortLibrary());
	int32_t rc = omrcuda_deviceGetCount(&deviceCount);

	ASSERT_EQ(0, rc) << "omrcuda_deviceGetCount failed";
}

/**
 * Test whether memory contains a repeating pattern.
 */
bool
CudaTest::fillVerify(const void *buffer, uintptr_t size, const void *fill, uintptr_t fillSize)
{
	const uint8_t *bufferBytes = (const uint8_t *)buffer;
	const uint8_t *fillBytes = (const uint8_t *)fill;
	uint64_t bufferIndex = 0;
	uint64_t fillIndex = 0;

	for (bufferIndex = 0; bufferIndex < size; ++bufferIndex) {
		if (bufferBytes[bufferIndex] != fillBytes[fillIndex]) {
			return false;
		}

		fillIndex += 1;
		if (fillIndex >= fillSize) {
			fillIndex = 0;
		}
	}

	return true;
}

/**
 * Fill memory with a semi-random pattern.
 */
void
CudaTest::patternFill(void *buffer, uintptr_t size, uint32_t seed)
{
	uint8_t *bufferBytes = (uint8_t *)buffer;
	uint64_t value = seed << 16;
	uint64_t i = 0;

	for (i = 0; i < seed; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;
	}

	for (i = 0; i < size; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;
		bufferBytes[i] = (uint8_t)(value >> 16);
	}
}

/**
 * Test whether memory contains a semi-random pattern.
 */
bool
CudaTest::patternVerify(const void *buffer, uintptr_t size, uint32_t seed)
{
	const uint8_t *bufferBytes = (const uint8_t *)buffer;
	uint64_t value = seed << 16;
	uint64_t i = 0;

	for (i = 0; i < seed; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;
	}

	for (i = 0; i < size; ++i) {
		value = (value * MULTIPLIER) + INCREMENT;

		if ((uint8_t)(value >> 16) != bufferBytes[i]) {
			return false;
		}
	}

	return true;
}

static const J9CudaCacheConfig
cacheConfigs[] = {
	J9CUDA_CACHE_CONFIG_PREFER_EQUAL,
	J9CUDA_CACHE_CONFIG_PREFER_L1,
	J9CUDA_CACHE_CONFIG_PREFER_NONE,
	J9CUDA_CACHE_CONFIG_PREFER_SHARED
};

const CudaTestArray<const J9CudaCacheConfig>
CudaDeviceTest::allCacheConfigs = { LENGTH_OF(cacheConfigs), cacheConfigs };

static const J9CudaSharedMemConfig
sharedMemConfigs[] = {
	J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE,
	J9CUDA_SHARED_MEM_CONFIG_4_BYTE_BANK_SIZE,
	J9CUDA_SHARED_MEM_CONFIG_8_BYTE_BANK_SIZE
};

const CudaTestArray<const J9CudaSharedMemConfig>
CudaDeviceTest::allSharedMemConfigs = { LENGTH_OF(sharedMemConfigs), sharedMemConfigs };
