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


#ifndef CUDA_TESTS_HPP_INCLUDED
#define CUDA_TESTS_HPP_INCLUDED

#include "portTestHelpers.hpp"

#define LENGTH_OF(array) (sizeof(array) / sizeof((array)[0]))

#define ASSERT_NOT_NULL(pointer) ASSERT_NE((const void *)0, (pointer))

template <typename Element>
struct CudaTestArray {
	size_t length;
	Element *data;
};

class CudaTest : public ::testing::Test
{
private:
	/* Parameters of our linear congruential generator. */
	static const uint64_t MULTIPLIER = 25214903917;
	static const uint64_t INCREMENT = 11;

protected:
	static OMRPortLibrary *
	getPortLibrary()
	{
		return portTestEnv->getPortLibrary();
	}

	static bool
	fillVerify(const void *buffer, uintptr_t size, const void *fill, uintptr_t fillSize);

	static void
	patternFill(void *buffer, uintptr_t size, uint32_t seed);

	static bool
	patternVerify(const void *buffer, uintptr_t size, uint32_t seed);

	/**
	 * Normalize an API version number encoded as (1000 * major + 10 * minor)
	 * to (10 * major + minor) for consistency with other scaled values like
	 * compute capability. For example, runtime version 5050 maps to 55.
	 *
	 * @param[in] apiVersion  the API version number
	 * @return a normalized version number
	 */
	static uint32_t
	normalizeVersion(uint32_t apiVersion)
	{
		uint32_t major = (apiVersion / 1000);
		uint32_t minor = (apiVersion /   10) % 10;

		return (major * 10) + minor;
	}
};

class CudaDeviceTest : public CudaTest
{
protected:
	static const CudaTestArray<const J9CudaCacheConfig> allCacheConfigs;
	static const CudaTestArray<const J9CudaSharedMemConfig> allSharedMemConfigs;

	static const CudaTestArray<char> ptxFragment1;
	static const CudaTestArray<char> ptxFragment2;
	static const CudaTestArray<char> ptxModule;
	static const CudaTestArray<char> ptxParameters;

	uint32_t deviceCount;

	virtual void
	SetUp();

	/**
	 * Is a message for the given error code required when
	 * the specified number of devices are available?
	 *
	 * @param[in] error the error code
	 */
	bool
	isErrorStringRequired(int32_t error)
	{
		if (0 != deviceCount) {
			return true;
		}

		if ((J9CUDA_NO_ERROR == error) || (J9CUDA_ERROR_NO_DEVICE == error)) {
			return true;
		}

		return false;
	}

	static void
	testFunction(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function);

	static void
	testPeerTransfer(OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId);

public:
	CudaDeviceTest()
		: deviceCount(0)
	{
	}
};

#endif /* CUDA_TESTS_HPP_INCLUDED */
