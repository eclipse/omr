/*******************************************************************************
 * Copyright IBM Corp. and others 2013
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "omrcuda.h"
#include "omrportpriv.h"
#include "omr.h"
#include "omrmemcategories.h"
#include <string.h>

#include "ut_omrport.h"

/**
 * If we're using a version of xlC that doesn't define __GNUC__,
 * define it now so CUDA header files will be acceptable.
 */
#if (defined(__xlC__) || defined(__ibmxl__) || defined(__open_xl__)) && ! defined(__GNUC__)
#define __GNUC__ 4
#define __GNUC_MINOR__ 8
#define __GNUC_PATCHLEVEL__ 0
#endif /* (__xlC__ || __ibmxl__) && ! __GNUC__ */

#include <cuda.h>
#include <cuda_runtime.h>

#define J9CUDA_ALLOCATE_MEMORY(byteCount) \
		OMRPORTLIB->mem_allocate_memory(OMRPORTLIB, byteCount, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_CUDA)

#define J9CUDA_FREE_MEMORY(buffer) \
		OMRPORTLIB->mem_free_memory(OMRPORTLIB, buffer)

/* Statically assert that 'condition' is true (non-zero). */
#define J9CUDA_STATIC_ASSERT(name, condition) \
		typedef char omrcuda_static_assertion_ ## name[(condition) ? 1 : -1];

#define LENGTH_OF(array) (sizeof(array) / sizeof((array)[0]))

namespace
{

/**
 * A descriptor for a shared library entry point.
 */
struct J9CudaEntryDescriptor {
	/** C-linkage function name */
	const char *name;
	/** function signature (see omrsl.c) */
	const char *signature;
	/** offset in associated table */
	uint32_t offset;
	/** first library version which defines the entry point */
	uint32_t version;
};

/**
 * Lookup all the given entry points in a shared library.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] dllHandle the shared library handle
 * @param[in] libraryVersion the shared library version
 * @param[in] entries the array of entry descriptors
 * @param[in] entryCount the number of entry descriptors
 * @param[out] table the array of function pointers
 * @return 0 on success, any other value on failure
 */
int32_t
initializeTable(OMRPortLibrary *portLibrary, uintptr_t dllHandle, uint32_t libraryVersion, const J9CudaEntryDescriptor *entries, uintptr_t entryCount, uintptr_t *table)
{
	Trc_PRT_cuda_initializeTable_entry();

	int32_t result = 0;
	uint32_t satisfiedOffset = ~(uint32_t)0;

	for (uintptr_t index = 0; index < entryCount; ++index) {
		const J9CudaEntryDescriptor *entry = &entries[index];
		uintptr_t function = 0;

		if (entry->offset == satisfiedOffset) {
			/* we already found a good entry point */
			continue;
		}

		if (entry->version > libraryVersion) {
			/* ignore entries that require a newer version of the library than we have */
			continue;
		}

		if (0 != portLibrary->sl_lookup_name(
				portLibrary,
				dllHandle,
				(char *)entry->name,
				&function,
				entry->signature)) {
			Trc_PRT_cuda_symbol_not_found(entry->name, entry->signature);
			result = 1;
			break;
		}

		Trc_PRT_cuda_symbol_found(entry->name, entry->signature, (void *)function);

		*(uintptr_t *)(((uintptr_t)table) + entry->offset) = function;
		satisfiedOffset = entry->offset;
	}

	Trc_PRT_cuda_initializeTable_exit(result);

	return result;
}

/**
 * Collect information about a specific CUDA device for inclusion
 * in a javacore file.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[out] deviceData the device descriptor pointer
 * @return J9CUDA_NO_ERROR on success or a non-zero value on failure
 */
int32_t
getDeviceData(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceDescriptor *deviceData)
{
	Trc_PRT_cuda_getDeviceData_entry(deviceId);

	J9CudaConfig *config = portLibrary->cuda_configData;
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	Assert_PRT_true(NULL != config);
	Assert_PRT_true(NULL != globals);

	J9CudaError result = J9CUDA_NO_ERROR;

	if (globals->deviceCount <= deviceId) {
		result = J9CUDA_ERROR_INVALID_DEVICE;
	} else {
		J9CudaDeviceDescriptor *details = (J9CudaDeviceDescriptor *)&config[1];

		memcpy(deviceData, &details[deviceId], sizeof(*deviceData));

		if (J9CUDA_NO_ERROR != omrcuda_deviceGetMemInfo(portLibrary, deviceId, &deviceData->availableMemory, &deviceData->totalMemory)) {
			/* on failure, we mark 'availableMemory' unknown */
			deviceData->availableMemory = ~(uintptr_t)0;
		}
	}

	Trc_PRT_cuda_getDeviceData_exit(result);

	return result;
}

/**
 * Normalize an API version number encoded as (1000 * major + 10 * minor)
 * to (10 * major + minor) for consistency with other scaled values like
 * compute capability. For example, runtime version 5050 maps to 55.
 *
 * @param[in] apiVersion the API version number
 * @return a normalized version number
 */
VMINLINE uint32_t
normalizeVersion(uint32_t apiVersion)
{
	uint32_t major = (apiVersion / 1000);
	uint32_t minor = (apiVersion / 10) % 10;

	return (major * 10) + minor;
}

/**
 * Collect summary information about the CUDA environment for inclusion
 * in a javacore file.
 *
 * @param[in] portLibrary the port library pointer
 * @param[out] summaryData the summary descriptor pointer
 * @return J9CUDA_NO_ERROR on success or a non-zero value on failure
 */
int32_t
getSummaryData(OMRPortLibrary *portLibrary, J9CudaSummaryDescriptor *summaryData)
{
	Trc_PRT_cuda_getSummaryData_entry();

	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	Assert_PRT_true(NULL != globals);

	summaryData->deviceCount = globals->deviceCount;
	summaryData->driverVersion = normalizeVersion(globals->driverVersion);
	summaryData->runtimeVersion = normalizeVersion(globals->runtimeVersion);

	Trc_PRT_cuda_getSummaryData_exit();

	return J9CUDA_NO_ERROR;
}

/*
 * A set of functions located in the CUDA driver and runtime libraries.
 */
struct J9CudaFunctionTable {
	/* The signatures of the following must match the driver API declared in cuda.h. */
	CUresult (CUDAAPI *DeviceGet)(CUdevice *device, int ordinal);
	CUresult (CUDAAPI *DeviceGetName)(char *nameOut, int nameSize, CUdevice device);
	CUresult (CUDAAPI *FuncGetAttribute)(int *value, CUfunction_attribute attribute, CUfunction function);
	CUresult (CUDAAPI *FuncSetCacheConfig)(CUfunction function, CUfunc_cache config);
	CUresult (CUDAAPI *FuncSetSharedMemConfig)(CUfunction function, CUsharedconfig config);
	CUresult (CUDAAPI *GetDriverErrorString)(CUresult error, const char **nameOut);
	CUresult (CUDAAPI *LaunchKernel)(CUfunction function,
									 unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
									 unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
									 unsigned int sharedMemBytes, CUstream stream, void **kernelParms, void **extra);
	CUresult (CUDAAPI *LinkAddData)(CUlinkState state, CUjitInputType type, void *data,
									size_t size, const char *name, unsigned int numOptions, CUjit_option *options, void **optionValues);
	CUresult (CUDAAPI *LinkComplete)(CUlinkState state, void **cubinOut, size_t *sizeOut);
	CUresult (CUDAAPI *LinkCreate)(unsigned int numOptions, CUjit_option *options, void **optionValues, CUlinkState *stateOut);
	CUresult (CUDAAPI *LinkDestroy)(CUlinkState state);
	CUresult (CUDAAPI *MemsetD8Async)(CUdeviceptr dstDevice, unsigned char value, size_t count, CUstream stream);
	CUresult (CUDAAPI *MemsetD16Async)(CUdeviceptr dstDevice, unsigned short value, size_t count, CUstream stream);
	CUresult (CUDAAPI *MemsetD32Async)(CUdeviceptr dstDevice, unsigned int value, size_t count, CUstream stream);
	CUresult (CUDAAPI *ModuleGetFunction)(CUfunction *functionOut, CUmodule module, const char *name);
	CUresult (CUDAAPI *ModuleGetGlobal)(CUdeviceptr *addressOut, size_t *sizeOut, CUmodule module, const char *name);
	CUresult (CUDAAPI *ModuleGetSurfRef)(CUsurfref *surfRefOut, CUmodule module, const char *name);
	CUresult (CUDAAPI *ModuleGetTexRef)(CUtexref *texRefOut, CUmodule module, const char *name);
	CUresult (CUDAAPI *ModuleLoadDataEx)(CUmodule *module, const void *image, unsigned int numOptions, CUjit_option *options, void **optionValues);
	CUresult (CUDAAPI *ModuleUnload)(CUmodule module);
	CUresult (CUDAAPI *OccupancyMaxActiveBlocksPerMultiprocessorWithFlags)(int *numBlocks, CUfunction function, int blockSize, size_t dynamicSMemSize, unsigned int flags);

	/* The signatures of the following must match the runtime API declared in cuda_runtime_api.h. */
	cudaError_t (CUDARTAPI *DeviceCanAccessPeer)(int *canAccessPeerOut, int device, int peerDevice);
	cudaError_t (CUDARTAPI *DeviceDisablePeerAccess)(int peerDevice);
	cudaError_t (CUDARTAPI *DeviceEnablePeerAccess)(int peerDevice, unsigned int flags);
	cudaError_t (CUDARTAPI *DeviceGetAttribute)(int *value, cudaDeviceAttr attr, int device);
	cudaError_t (CUDARTAPI *DeviceGetCacheConfig)(cudaFuncCache *config);
	cudaError_t (CUDARTAPI *DeviceGetLimit)(size_t *pValue, cudaLimit limit);
	cudaError_t (CUDARTAPI *DeviceGetSharedMemConfig)(cudaSharedMemConfig *config);
	cudaError_t (CUDARTAPI *DeviceReset)(void);
	cudaError_t (CUDARTAPI *DeviceGetStreamPriorityRange)(int *leastPriority, int *greatestPriority);
	cudaError_t (CUDARTAPI *DeviceSetCacheConfig)(cudaFuncCache config);
	cudaError_t (CUDARTAPI *DeviceSetLimit)(cudaLimit limit, size_t value);
	cudaError_t (CUDARTAPI *DeviceSetSharedMemConfig)(cudaSharedMemConfig config);
	cudaError_t (CUDARTAPI *DeviceSynchronize)(void);
	cudaError_t (CUDARTAPI *EventCreateWithFlags)(cudaEvent_t *event, unsigned int flags);
	cudaError_t (CUDARTAPI *EventDestroy)(cudaEvent_t event);
	cudaError_t (CUDARTAPI *EventElapsedTime)(float *elapsedMillis, cudaEvent_t start, cudaEvent_t end);
	cudaError_t (CUDARTAPI *EventQuery)(cudaEvent_t event);
	cudaError_t (CUDARTAPI *EventRecord)(cudaEvent_t event, cudaStream_t stream);
	cudaError_t (CUDARTAPI *EventSynchronize)(cudaEvent_t event);
	cudaError_t (CUDARTAPI *Free)(void *deviceAddress);
	cudaError_t (CUDARTAPI *FreeHost)(void *ptr);
	cudaError_t (CUDARTAPI *GetDevice)(int *deviceId);
	const char *(CUDARTAPI *GetErrorString)(cudaError_t error);
	cudaError_t (CUDARTAPI *HostAlloc)(void **pHost, size_t size, unsigned int flags);
	cudaError_t (CUDARTAPI *Malloc)(void **deviceAddressOut, size_t size);
	cudaError_t (CUDARTAPI *Memcpy)(void *dst, const void *src, size_t count, cudaMemcpyKind kind);
	cudaError_t (CUDARTAPI *MemcpyAsync)(void *dst, const void *src, size_t count, cudaMemcpyKind kind, cudaStream_t stream);
	cudaError_t (CUDARTAPI *Memcpy2D)(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, cudaMemcpyKind kind);
	cudaError_t (CUDARTAPI *Memcpy2DAsync)(void *dst, size_t dpitch, const void *src, size_t spitch, size_t width, size_t height, cudaMemcpyKind kind, cudaStream_t stream);
	cudaError_t (CUDARTAPI *MemcpyPeer)(void *dst, int dstDevice, const void *src, int srcDevice, size_t count);
	cudaError_t (CUDARTAPI *MemcpyPeerAsync)(void *dst, int dstDevice, const void *src, int srcDevice, size_t count, cudaStream_t stream);
	cudaError_t (CUDARTAPI *MemGetInfo)(size_t *free, size_t *total);
	cudaError_t (CUDARTAPI *SetDevice)(int deviceId);
	cudaError_t (CUDARTAPI *StreamAddCallback)(cudaStream_t stream, cudaStreamCallback_t callback, void *userData, unsigned int flags);
	cudaError_t (CUDARTAPI *StreamCreate)(cudaStream_t *stream);
	cudaError_t (CUDARTAPI *StreamCreateWithPriority)(cudaStream_t *stream, unsigned int flags, int priority);
	cudaError_t (CUDARTAPI *StreamDestroy)(cudaStream_t stream);
	cudaError_t (CUDARTAPI *StreamGetFlags)(cudaStream_t stream, unsigned int *flags);
	cudaError_t (CUDARTAPI *StreamGetPriority)(cudaStream_t stream, int *priority);
	cudaError_t (CUDARTAPI *StreamQuery)(cudaStream_t stream);
	cudaError_t (CUDARTAPI *StreamSynchronize)(cudaStream_t stream);
	cudaError_t (CUDARTAPI *StreamWaitEvent)(cudaStream_t stream, cudaEvent_t event, unsigned int flags);
};

/**
 * The function table in J9CudaGlobalData must be the same size as J9CudaFunctionTable.
 */
J9CUDA_STATIC_ASSERT(
	functionTable_size,
	sizeof(J9CudaFunctionTable) == sizeof(((J9CudaGlobalData *)0)->functionTable));

/**
 * Translate the given driver API error to an OMR error code.
 *
 * @param[in] result a CUDA driver result code
 * @return an equivalent OMR error code
 */
J9CudaError
mapDriver(CUresult result)
{
	switch (result) {
	case CUDA_SUCCESS:
		return J9CUDA_NO_ERROR;

	case CUDA_ERROR_ASSERT:
		return J9CUDA_ERROR_ASSERT;
	case CUDA_ERROR_ECC_UNCORRECTABLE:
		return J9CUDA_ERROR_ECCUNCORRECTABLE;
	case CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED:
		return J9CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED;
	case CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED:
		return J9CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED;
	case CUDA_ERROR_INVALID_DEVICE:
		return J9CUDA_ERROR_INVALID_DEVICE;
	case CUDA_ERROR_INVALID_HANDLE:
		return J9CUDA_ERROR_INVALID_RESOURCE_HANDLE;
	case CUDA_ERROR_INVALID_IMAGE:
		return J9CUDA_ERROR_INVALID_KERNEL_IMAGE;
	case CUDA_ERROR_INVALID_VALUE:
		return J9CUDA_ERROR_INVALID_VALUE;
	case CUDA_ERROR_LAUNCH_FAILED:
		return J9CUDA_ERROR_LAUNCH_FAILURE;
	case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES:
		return J9CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES;
	case CUDA_ERROR_LAUNCH_TIMEOUT:
		return J9CUDA_ERROR_LAUNCH_TIMEOUT;
	case CUDA_ERROR_NO_BINARY_FOR_GPU:
		return J9CUDA_ERROR_NO_KERNEL_IMAGE_FOR_DEVICE;
	case CUDA_ERROR_NO_DEVICE:
		return J9CUDA_ERROR_NO_DEVICE;
	case CUDA_ERROR_NOT_PERMITTED:
		return J9CUDA_ERROR_NOT_PERMITTED;
	case CUDA_ERROR_NOT_READY:
		return J9CUDA_ERROR_NOT_READY;
	case CUDA_ERROR_NOT_SUPPORTED:
		return J9CUDA_ERROR_NOT_SUPPORTED;
	case CUDA_ERROR_OPERATING_SYSTEM:
		return J9CUDA_ERROR_OPERATING_SYSTEM;
	case CUDA_ERROR_OUT_OF_MEMORY:
		return J9CUDA_ERROR_MEMORY_ALLOCATION;
	case CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED:
		return J9CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED;
	case CUDA_ERROR_PEER_ACCESS_NOT_ENABLED:
		return J9CUDA_ERROR_PEER_ACCESS_NOT_ENABLED;
	case CUDA_ERROR_PEER_ACCESS_UNSUPPORTED:
		return J9CUDA_ERROR_PEER_ACCESS_UNSUPPORTED;
	case CUDA_ERROR_PROFILER_DISABLED:
		return J9CUDA_ERROR_PROFILER_DISABLED;
	case CUDA_ERROR_SHARED_OBJECT_INIT_FAILED:
		return J9CUDA_ERROR_SHARED_OBJECT_INIT_FAILED;
	case CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND:
		return J9CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND;
	case CUDA_ERROR_TOO_MANY_PEERS:
		return J9CUDA_ERROR_TOO_MANY_PEERS;
	case CUDA_ERROR_UNKNOWN:
		return J9CUDA_ERROR_UNKNOWN;
	case CUDA_ERROR_UNMAP_FAILED:
		return J9CUDA_ERROR_UNMAP_BUFFER_OBJECT_FAILED;
	case CUDA_ERROR_UNSUPPORTED_LIMIT:
		return J9CUDA_ERROR_UNSUPPORTED_LIMIT;

	default:
		return (J9CudaError)-(int32_t)result;
	}
}

/**
 * Translate the given runtime API error to an OMR error code.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] error a CUDA runtime API error code
 * @return an equivalent OMR error code
 */
J9CudaError
mapRuntime(OMRPortLibrary *portLibrary, cudaError_t error)
{
	/* The mapping is trivial for success, or if the runtime predates version 10.1. */
	if ((cudaSuccess != error)
	&&  (portLibrary->portGlobals->cudaGlobals.runtimeVersion >= 10010)
	) {
		/*
		 * This switch statement was automatically generated by a tool compiled
		 * against CUDA 10.1 header files. The case label values must be numeric
		 * constants because they are potentially associated with a version of
		 * the runtime which is different than what was used at compile time.
		 */
		switch ((int32_t)error) {
		case 52: /* cudaErrorMissingConfiguration */
			return J9CUDA_ERROR_MISSING_CONFIGURATION;
		case 719: /* cudaErrorLaunchFailure */
			return J9CUDA_ERROR_LAUNCH_FAILURE;
		case 702: /* cudaErrorLaunchTimeout */
			return J9CUDA_ERROR_LAUNCH_TIMEOUT;
		case 701: /* cudaErrorLaunchOutOfResources */
			return J9CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES;
		case 98: /* cudaErrorInvalidDeviceFunction */
			return J9CUDA_ERROR_INVALID_DEVICE_FUNCTION;
		case 101: /* cudaErrorInvalidDevice */
			return J9CUDA_ERROR_INVALID_DEVICE;
		case 1: /* cudaErrorInvalidValue */
			return J9CUDA_ERROR_INVALID_VALUE;
		case 205: /* cudaErrorMapBufferObjectFailed */
			return J9CUDA_ERROR_MAP_BUFFER_OBJECT_FAILED;
		case 206: /* cudaErrorUnmapBufferObjectFailed */
			return J9CUDA_ERROR_UNMAP_BUFFER_OBJECT_FAILED;
		case 4: /* cudaErrorCudartUnloading */
			return J9CUDA_ERROR_CUDART_UNLOADING;
		case 999: /* cudaErrorUnknown */
			return J9CUDA_ERROR_UNKNOWN;
		case 400: /* cudaErrorInvalidResourceHandle */
			return J9CUDA_ERROR_INVALID_RESOURCE_HANDLE;
		case 600: /* cudaErrorNotReady */
			return J9CUDA_ERROR_NOT_READY;
		case 708: /* cudaErrorSetOnActiveProcess */
			return J9CUDA_ERROR_SET_ON_ACTIVE_PROCESS;
		case 100: /* cudaErrorNoDevice */
			return J9CUDA_ERROR_NO_DEVICE;
		case 214: /* cudaErrorECCUncorrectable */
			return J9CUDA_ERROR_ECCUNCORRECTABLE;
		case 302: /* cudaErrorSharedObjectSymbolNotFound */
			return J9CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND;
		case 303: /* cudaErrorSharedObjectInitFailed */
			return J9CUDA_ERROR_SHARED_OBJECT_INIT_FAILED;
		case 215: /* cudaErrorUnsupportedLimit */
			return J9CUDA_ERROR_UNSUPPORTED_LIMIT;
		case 200: /* cudaErrorInvalidKernelImage */
			return J9CUDA_ERROR_INVALID_KERNEL_IMAGE;
		case 209: /* cudaErrorNoKernelImageForDevice */
			return J9CUDA_ERROR_NO_KERNEL_IMAGE_FOR_DEVICE;
		case 704: /* cudaErrorPeerAccessAlreadyEnabled */
			return J9CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED;
		case 705: /* cudaErrorPeerAccessNotEnabled */
			return J9CUDA_ERROR_PEER_ACCESS_NOT_ENABLED;
		case 216: /* cudaErrorDeviceAlreadyInUse */
			return J9CUDA_ERROR_DEVICE_ALREADY_IN_USE;
		case 5: /* cudaErrorProfilerDisabled */
			return J9CUDA_ERROR_PROFILER_DISABLED;
		case 710: /* cudaErrorAssert */
			return J9CUDA_ERROR_ASSERT;
		case 711: /* cudaErrorTooManyPeers */
			return J9CUDA_ERROR_TOO_MANY_PEERS;
		case 712: /* cudaErrorHostMemoryAlreadyRegistered */
			return J9CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED;
		case 713: /* cudaErrorHostMemoryNotRegistered */
			return J9CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED;
		case 304: /* cudaErrorOperatingSystem */
			return J9CUDA_ERROR_OPERATING_SYSTEM;
		case 217: /* cudaErrorPeerAccessUnsupported */
			return J9CUDA_ERROR_PEER_ACCESS_UNSUPPORTED;
		case 800: /* cudaErrorNotPermitted */
			return J9CUDA_ERROR_NOT_PERMITTED;
		case 801: /* cudaErrorNotSupported */
			return J9CUDA_ERROR_NOT_SUPPORTED;
		default:
			break;
		}
	}

	return (J9CudaError)(int32_t)error;
}

/**
 * Translate the given OMR error code to a CUDA runtime API error code.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] error an OMR error code
 * @return an equivalent CUDA error code
 */
cudaError_t
unmapRuntime(OMRPortLibrary *portLibrary, J9CudaError error)
{
	/* The mapping is trivial for success, or if the runtime predates version 10.1. */
	if ((J9CUDA_NO_ERROR != error)
	&&  (portLibrary->portGlobals->cudaGlobals.runtimeVersion >= 10010)
	) {
		/*
		 * This switch statement was automatically generated by a tool compiled
		 * against CUDA 10.1 header files. The return values must be numeric
		 * constants because they are potentially associated with a version of
		 * the runtime which is different than what was used at compile time.
		 */
		switch (error) {
		case J9CUDA_ERROR_MISSING_CONFIGURATION:
			return (cudaError_t)52; /* cudaErrorMissingConfiguration */
		case J9CUDA_ERROR_LAUNCH_FAILURE:
			return (cudaError_t)719; /* cudaErrorLaunchFailure */
		case J9CUDA_ERROR_LAUNCH_TIMEOUT:
			return (cudaError_t)702; /* cudaErrorLaunchTimeout */
		case J9CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES:
			return (cudaError_t)701; /* cudaErrorLaunchOutOfResources */
		case J9CUDA_ERROR_INVALID_DEVICE_FUNCTION:
			return (cudaError_t)98; /* cudaErrorInvalidDeviceFunction */
		case J9CUDA_ERROR_INVALID_DEVICE:
			return (cudaError_t)101; /* cudaErrorInvalidDevice */
		case J9CUDA_ERROR_INVALID_VALUE:
			return (cudaError_t)1; /* cudaErrorInvalidValue */
		case J9CUDA_ERROR_MAP_BUFFER_OBJECT_FAILED:
			return (cudaError_t)205; /* cudaErrorMapBufferObjectFailed */
		case J9CUDA_ERROR_UNMAP_BUFFER_OBJECT_FAILED:
			return (cudaError_t)206; /* cudaErrorUnmapBufferObjectFailed */
		case J9CUDA_ERROR_CUDART_UNLOADING:
			return (cudaError_t)4; /* cudaErrorCudartUnloading */
		case J9CUDA_ERROR_UNKNOWN:
			return (cudaError_t)999; /* cudaErrorUnknown */
		case J9CUDA_ERROR_INVALID_RESOURCE_HANDLE:
			return (cudaError_t)400; /* cudaErrorInvalidResourceHandle */
		case J9CUDA_ERROR_NOT_READY:
			return (cudaError_t)600; /* cudaErrorNotReady */
		case J9CUDA_ERROR_SET_ON_ACTIVE_PROCESS:
			return (cudaError_t)708; /* cudaErrorSetOnActiveProcess */
		case J9CUDA_ERROR_NO_DEVICE:
			return (cudaError_t)100; /* cudaErrorNoDevice */
		case J9CUDA_ERROR_ECCUNCORRECTABLE:
			return (cudaError_t)214; /* cudaErrorECCUncorrectable */
		case J9CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND:
			return (cudaError_t)302; /* cudaErrorSharedObjectSymbolNotFound */
		case J9CUDA_ERROR_SHARED_OBJECT_INIT_FAILED:
			return (cudaError_t)303; /* cudaErrorSharedObjectInitFailed */
		case J9CUDA_ERROR_UNSUPPORTED_LIMIT:
			return (cudaError_t)215; /* cudaErrorUnsupportedLimit */
		case J9CUDA_ERROR_INVALID_KERNEL_IMAGE:
			return (cudaError_t)200; /* cudaErrorInvalidKernelImage */
		case J9CUDA_ERROR_NO_KERNEL_IMAGE_FOR_DEVICE:
			return (cudaError_t)209; /* cudaErrorNoKernelImageForDevice */
		case J9CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED:
			return (cudaError_t)704; /* cudaErrorPeerAccessAlreadyEnabled */
		case J9CUDA_ERROR_PEER_ACCESS_NOT_ENABLED:
			return (cudaError_t)705; /* cudaErrorPeerAccessNotEnabled */
		case J9CUDA_ERROR_DEVICE_ALREADY_IN_USE:
			return (cudaError_t)216; /* cudaErrorDeviceAlreadyInUse */
		case J9CUDA_ERROR_PROFILER_DISABLED:
			return (cudaError_t)5; /* cudaErrorProfilerDisabled */
		case J9CUDA_ERROR_ASSERT:
			return (cudaError_t)710; /* cudaErrorAssert */
		case J9CUDA_ERROR_TOO_MANY_PEERS:
			return (cudaError_t)711; /* cudaErrorTooManyPeers */
		case J9CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED:
			return (cudaError_t)712; /* cudaErrorHostMemoryAlreadyRegistered */
		case J9CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED:
			return (cudaError_t)713; /* cudaErrorHostMemoryNotRegistered */
		case J9CUDA_ERROR_OPERATING_SYSTEM:
			return (cudaError_t)304; /* cudaErrorOperatingSystem */
		case J9CUDA_ERROR_PEER_ACCESS_UNSUPPORTED:
			return (cudaError_t)217; /* cudaErrorPeerAccessUnsupported */
		case J9CUDA_ERROR_NOT_PERMITTED:
			return (cudaError_t)800; /* cudaErrorNotPermitted */
		case J9CUDA_ERROR_NOT_SUPPORTED:
			return (cudaError_t)801; /* cudaErrorNotSupported */
		default:
			break;
		}
	}

	return (cudaError_t)(int32_t)error;
}

/**
 * Query the name of a device. The name is returned as a nul-terminated string.
 * The parameter, nameSize, must include room for the trailing nul.
 *
 * @param[in] functions the function table pointer
 * @param[in] deviceId the device identifier
 * @param[in] nameSize the size of the buffer where the name should be stored
 * @param[out] nameOut the buffer where the name should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
J9CudaError
getDeviceName(J9CudaFunctionTable *functions, uint32_t deviceId, uint32_t nameSize, char *nameOut)
{
	CUresult result = CUDA_ERROR_NO_DEVICE;

	if ((NULL != functions->DeviceGet) && (NULL != functions->DeviceGetName)) {
		CUdevice device = 0;

		result = functions->DeviceGet(&device, deviceId);

		if (CUDA_SUCCESS == result) {
			result = functions->DeviceGetName(nameOut, (int)nameSize, device);
		}
	}

	return mapDriver(result);
}

/**
 * Collect information about a specific CUDA device for inclusion
 * in a javacore file.
 *
 * @param[in] functions the function table pointer
 * @param[in] deviceId the device identifier
 * @param[out] deviceData the device descriptor pointer
 * @return J9CUDA_NO_ERROR on success or a non-zero value on failure
 */
J9CudaError
initDeviceData(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceDescriptor *deviceData)
{
	Trc_PRT_cuda_initDeviceData_entry(deviceId);

	J9CudaError result = J9CUDA_NO_ERROR;
	cudaError_t error = cudaSuccess;
	J9CudaFunctionTable *functions = (J9CudaFunctionTable *)portLibrary->portGlobals->cudaGlobals.functionTable;

	{
		error = functions->SetDevice((int)deviceId);

		if (cudaSuccess != error) {
			Trc_PRT_cuda_initDeviceData_fail2("set", error);
			goto fail;
		}
	}

	{
		int pciDomainId = 0;

		error = functions->DeviceGetAttribute(&pciDomainId, cudaDevAttrPciDomainId, (int)deviceId);

		if (cudaSuccess != error) {
			Trc_PRT_cuda_initDeviceData_fail("PCI domain id", error);
			goto fail;
		}

		deviceData->pciDomainId = (uint32_t)pciDomainId;
	}

	{
		int pciBusId = 0;

		error = functions->DeviceGetAttribute(&pciBusId, cudaDevAttrPciBusId, (int)deviceId);

		if (cudaSuccess != error) {
			Trc_PRT_cuda_initDeviceData_fail("PCI bus id", error);
			goto fail;
		}

		deviceData->pciBusId = (uint32_t)pciBusId;
	}

	{
		int pciDeviceId = 0;

		error = functions->DeviceGetAttribute(&pciDeviceId, cudaDevAttrPciDeviceId, (int)deviceId);

		if (cudaSuccess != error) {
			Trc_PRT_cuda_initDeviceData_fail("PCI device id", error);
			goto fail;
		}

		deviceData->pciDeviceId = (uint32_t)pciDeviceId;
	}

	{
		int computeMajor = 0;

		error = functions->DeviceGetAttribute(&computeMajor, cudaDevAttrComputeCapabilityMajor, (int)deviceId);

		if (cudaSuccess != error) {
			Trc_PRT_cuda_initDeviceData_fail("major compute capability", error);
			goto fail;
		}

		deviceData->computeCapability = (uint32_t)(computeMajor * 10);
	}

	{
		int computeMinor = 0;

		error = functions->DeviceGetAttribute(&computeMinor, cudaDevAttrComputeCapabilityMinor, (int)deviceId);

		if (cudaSuccess != error) {
			Trc_PRT_cuda_initDeviceData_fail("minor compute capability", error);
			goto fail;
		}

		deviceData->computeCapability += (uint32_t)computeMinor;
	}

	{
		int computeMode = 0;

		error = functions->DeviceGetAttribute(&computeMode, cudaDevAttrComputeMode, (int)deviceId);

		if (cudaSuccess != error) {
			Trc_PRT_cuda_initDeviceData_fail("compute mode", error);
			goto fail;
		}

		switch (computeMode) {
		case cudaComputeModeDefault:
			deviceData->computeMode = J9CUDA_COMPUTE_MODE_DEFAULT;
			break;
		case cudaComputeModeExclusive:
			deviceData->computeMode = J9CUDA_COMPUTE_MODE_PROCESS_EXCLUSIVE;
			break;
		case cudaComputeModeProhibited:
			deviceData->computeMode = J9CUDA_COMPUTE_MODE_PROHIBITED;
			break;
		case cudaComputeModeExclusiveProcess:
			deviceData->computeMode = J9CUDA_COMPUTE_MODE_THREAD_EXCLUSIVE;
			break;
		default:
			break;
		}
	}

	result = getDeviceName(functions, deviceId, (uint32_t)sizeof(deviceData->deviceName), deviceData->deviceName);

	if (J9CUDA_NO_ERROR != result) {
		Trc_PRT_cuda_initDeviceData_fail("name", result);
	}

done:
	Trc_PRT_cuda_initDeviceData_exit(result);

	return result;

fail:
	result = mapRuntime(portLibrary, error);
	goto done;
}

/**
 * Collect configuration information about the CUDA environment for inclusion
 * in service data.
 *
 * @param[in] portLibrary the port library pointer
 * @return J9CUDA_NO_ERROR on success or a non-zero value on failure
 */
J9CudaError
initConfigData(OMRPortLibrary *portLibrary)
{
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
	uint32_t deviceCount = globals->deviceCount;

	Trc_PRT_cuda_initConfigData_entry(deviceCount);

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	J9CudaError result = J9CUDA_NO_ERROR;
	/**
	 * The instance of J9CudaConfig allocated here is immediately followed
	 * by an array of deviceCount J9CudaDeviceDescriptor objects.
	 */
	size_t configSize = sizeof(J9CudaConfig) + (sizeof(J9CudaDeviceDescriptor) * deviceCount);
	J9CudaConfig *config = (J9CudaConfig *)J9CUDA_ALLOCATE_MEMORY(configSize);

	if (NULL == config) {
		Trc_PRT_cuda_initConfigData_fail("allocate config space");
		result = J9CUDA_ERROR_MEMORY_ALLOCATION;
		goto done;
	}

	memset(config, 0, configSize);

	if (0 != deviceCount) {
		J9CudaDeviceDescriptor *details = (J9CudaDeviceDescriptor *)&config[1];
		J9CudaFunctionTable *functions = (J9CudaFunctionTable *)globals->functionTable;

		if (1 == deviceCount) {
			/* With only one device, there's no need to save and restore the active device. */
			result = initDeviceData(portLibrary, 0, &details[0]);
		} else {
			int current = 0;
			cudaError_t error = functions->GetDevice(&current);

			if (cudaSuccess != error) {
				Trc_PRT_cuda_initDeviceData_fail2("get", result);
				result = mapRuntime(portLibrary, error);
				goto fail;
			}

			for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
				result = initDeviceData(portLibrary, deviceId, &details[deviceId]);

				if (J9CUDA_NO_ERROR != result) {
					Trc_PRT_cuda_initConfigData_fail("initialize detail");
					break;
				}
			}

			error = functions->SetDevice(current);

			if (cudaSuccess != error) {
				Trc_PRT_cuda_initDeviceData_fail2("restore", error);

				if (J9CUDA_NO_ERROR == result) {
					result = mapRuntime(portLibrary, error);
				}
			}
		}
	}

	if (J9CUDA_NO_ERROR != result) {
fail:
		J9CUDA_FREE_MEMORY(config);
		config = NULL;
		goto done;
	}

	config->getDeviceData = getDeviceData;
	config->getSummaryData = getSummaryData;

	portLibrary->cuda_configData = config;

done:
	Trc_PRT_cuda_initConfigData_exit(result);

	return result;
}

/**
 * Expands to a J9CudaEntryDescriptor initializer for a driver API function.
 *
 * @param version is the minimum required library version
 * @param name is a function pointer field of J9CudaFunctionTable
 * @param signature is a C (i.e. NUL-terminated) string (see omrsl.c)
 */
#define J9CUDA_DRIVER_ENTRY_v1(version, name, signature) \
	{ "cu" #name, signature, (uint32_t)offsetof(J9CudaFunctionTable, name), version }
#define J9CUDA_DRIVER_ENTRY_v2(version, name, signature) \
	{ "cu" #name "_v2", signature, (uint32_t)offsetof(J9CudaFunctionTable, name), version }

/**
 * Descriptors of required CUDA driver functions.
 */
const J9CudaEntryDescriptor driverDescriptors[] = {
	J9CUDA_DRIVER_ENTRY_v1(5050, DeviceGet, "iPi"),
	J9CUDA_DRIVER_ENTRY_v1(5050, DeviceGetName, "iPii"),
	J9CUDA_DRIVER_ENTRY_v1(5050, FuncGetAttribute, "iPiP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, FuncSetCacheConfig, "iPi"),
	J9CUDA_DRIVER_ENTRY_v1(5050, FuncSetSharedMemConfig, "iPi"),
	J9CUDA_DRIVER_ENTRY_v1(5050, LaunchKernel, "iPiiiiiiiPPP"),
	J9CUDA_DRIVER_ENTRY_v2(7000, LinkAddData, "iPiPLPiPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, LinkAddData, "iPiPLPiPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, LinkComplete, "iPPP"),
	J9CUDA_DRIVER_ENTRY_v2(7000, LinkCreate, "iiPPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, LinkCreate, "iiPPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, LinkDestroy, "iP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, MemsetD8Async, "iPbLP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, MemsetD16Async, "iPcLP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, MemsetD32Async, "iPiLP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, ModuleGetFunction, "iPPP"),
	J9CUDA_DRIVER_ENTRY_v2(5050, ModuleGetGlobal, "iPPPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, ModuleGetSurfRef, "iPPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, ModuleGetTexRef, "iPPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, ModuleLoadDataEx, "iPPiPP"),
	J9CUDA_DRIVER_ENTRY_v1(5050, ModuleUnload, "iP"),
	J9CUDA_DRIVER_ENTRY_v1(7000, OccupancyMaxActiveBlocksPerMultiprocessorWithFlags, "iPPILi"),

	/* GetDriverErrorString is so named to avoid clashing with the runtime entry GetErrorString. */
	{ "cuGetErrorString", "iiP", offsetof(J9CudaFunctionTable, GetDriverErrorString), 6000 }
};

#undef J9CUDA_DRIVER_ENTRY_v1
#undef J9CUDA_DRIVER_ENTRY_v2

/**
 * Expands to a J9CudaEntryDescriptor initializer for a runtime API function.
 *
 * version is the minimum required library version
 * name is the function pointer field of J9CudaFunctionTable
 * suffix is appended to form the full name of the CUDA API function
 * signature is a C (i.e. NUL-terminated) string (see omrsl.c)
 */
#define J9CUDA_RUNTIME_ENTRY(version, name, signature) \
	{ "cuda" #name, signature, (uint32_t)offsetof(J9CudaFunctionTable, name), version }

/**
 * Descriptors of required CUDA runtime functions.
 */
const J9CudaEntryDescriptor runtimeDescriptors[] = {
	J9CUDA_RUNTIME_ENTRY(5050, DeviceCanAccessPeer, "iPII"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceDisablePeerAccess, "iI"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceEnablePeerAccess, "iIi"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceGetAttribute, "iPiI"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceGetCacheConfig, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceGetLimit, "iPi"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceGetSharedMemConfig, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceGetStreamPriorityRange, "iPP"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceSetCacheConfig, "ii"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceReset, "i"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceSetLimit, "iiL"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceSetSharedMemConfig, "ii"),
	J9CUDA_RUNTIME_ENTRY(5050, DeviceSynchronize, "i"),
	J9CUDA_RUNTIME_ENTRY(5050, EventCreateWithFlags, "iPI"),
	J9CUDA_RUNTIME_ENTRY(5050, EventDestroy, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, EventElapsedTime, "iPPP"),
	J9CUDA_RUNTIME_ENTRY(5050, EventQuery, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, EventRecord, "iPP"),
	J9CUDA_RUNTIME_ENTRY(5050, EventSynchronize, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, Free, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, FreeHost, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, GetDevice, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, GetErrorString, "Pi"),
	J9CUDA_RUNTIME_ENTRY(5050, HostAlloc, "iPLi"),
	J9CUDA_RUNTIME_ENTRY(5050, Malloc, "iPL"),
	J9CUDA_RUNTIME_ENTRY(5050, Memcpy, "iPPLi"),
	J9CUDA_RUNTIME_ENTRY(5050, MemcpyAsync, "iPPLiP"),
	J9CUDA_RUNTIME_ENTRY(5050, Memcpy2D, "iPLPLLLi"),
	J9CUDA_RUNTIME_ENTRY(5050, Memcpy2DAsync, "iPLPLLLiP"),
	J9CUDA_RUNTIME_ENTRY(5050, MemcpyPeer, "iPIPIL"),
	J9CUDA_RUNTIME_ENTRY(5050, MemcpyPeerAsync, "iPIPILP"),
	J9CUDA_RUNTIME_ENTRY(5050, MemGetInfo, "iPP"),
	J9CUDA_RUNTIME_ENTRY(5050, SetDevice, "iI"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamAddCallback, "iPPPi"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamCreate, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamCreateWithPriority, "iPiI"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamDestroy, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamGetFlags, "iPP"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamGetPriority, "iPP"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamQuery, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamSynchronize, "iP"),
	J9CUDA_RUNTIME_ENTRY(5050, StreamWaitEvent, "iPPi")
};

#undef J9CUDA_RUNTIME_ENTRY

/**
 * Possible states of the CUDA access layer.
 */
enum J9CudaGlobalState {
	/** shutdown or not yet started (must be zero) */
	J9CUDA_STATE_SHUTDOWN = 0,
	/** started but initialization not yet attempted */
	J9CUDA_STATE_STARTED,
	/** successfully initialized */
	J9CUDA_STATE_INITIALIZED,
	/** initialization failed */
	J9CUDA_STATE_FAILED
};

/**
 * Open the driver shared library, find the required entry points and return
 * its version or zero on any failure.
 *
 * @param[in] portLibrary the port library pointer
 * @return the driver version or zero on failure
 */
uint32_t
openDriver(OMRPortLibrary *portLibrary)
{
#if defined(LINUX)
	const char *driverLibrary = "libcuda.so";
#elif defined(OSX)
	const char *driverLibrary = "libcuda.dylib";
#elif defined(OMR_OS_WINDOWS)
	const char *driverLibrary = "nvcuda.dll";
#endif /* defined(LINUX) */
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
	int deviceCount = 0;
	int version = 0;

	/* open the driver shared library */
	if (0 != portLibrary->sl_open_shared_library(
			portLibrary,
			(char *)driverLibrary,
			&globals->driverHandle,
			OMRPORT_SLOPEN_LAZY)) {
		Trc_PRT_cuda_library_not_found(driverLibrary);
		goto fail;
	}

	/* initialize the driver */
	{
		CUresult (CUDAAPI *driverInit)(unsigned int flags) = NULL;

		if (0 != portLibrary->sl_lookup_name(
				portLibrary,
				globals->driverHandle,
				(char *)"cuInit",
				(uintptr_t *)&driverInit,
				"iI")) {
			Trc_PRT_cuda_symbol_not_found("cuInit", "iI");
			goto fail;
		}

		if (CUDA_SUCCESS != driverInit(0)) {
			goto fail;
		}
	}

	/* query the driver version */
	{
		CUresult (CUDAAPI *driverGetVersion)(int *version) = NULL;

		if (0 != portLibrary->sl_lookup_name(
				portLibrary,
				globals->driverHandle,
				(char *)"cuDriverGetVersion",
				(uintptr_t *)&driverGetVersion,
				"iP")) {
			Trc_PRT_cuda_symbol_not_found("cuDriverGetVersion", "iP");
			goto fail;
		}

		if (CUDA_SUCCESS != driverGetVersion(&version)) {
			goto fail;
		}

		/**
		 * The minimum supported driver version is defined by
		 * the version of the toolkit used at compile time.
		 */
		if (version < CUDA_VERSION) {
			goto fail;
		}
	}

	/* query the number of available devices */
	{
		CUresult (CUDAAPI *deviceGetCount)(int *count) = NULL;

		if (0 != portLibrary->sl_lookup_name(
				portLibrary,
				globals->driverHandle,
				(char *)"cuDeviceGetCount",
				(uintptr_t *)&deviceGetCount,
				"iP")) {
			Trc_PRT_cuda_symbol_not_found("cuDeviceGetCount", "iP");
			goto fail;
		}

		if (CUDA_SUCCESS != deviceGetCount(&deviceCount)) {
			goto fail;
		}

		if (deviceCount <= 0) {
			goto fail;
		}
	}

	/* find the driver functions */
	if (0 != initializeTable(
			portLibrary,
			globals->driverHandle,
			(uint32_t)version,
			driverDescriptors,
			LENGTH_OF(driverDescriptors),
			(uintptr_t *)globals->functionTable)) {
fail:
		deviceCount = 0;
		version = 0;
	}

	globals->deviceCount = (uint32_t)deviceCount;
	globals->driverVersion = (uint32_t)version;

	return (uint32_t)version;
}

/**
 * Open a specific runtime shared library, find the required entry points and
 * return its version or zero on any failure or if its version is less than
 * the best version discovered so far.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] bestVersion the best version seen thus far
 * @param[in] fileVersion the version of the runtime library to open
 * @return the runtime version or zero on failure
 */
uint32_t
openRuntimeAndGetVersion(OMRPortLibrary *portLibrary, uint32_t bestVersion, uint32_t fileVersion)
{
	uintptr_t dllHandle = 0;
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
	J9CudaFunctionTable localFunctions;
	int version = 0;
	/*
	 * more than enough room for the longest names
	 *   libcudart.so.XX.Y
	 *   libcudart.XX.Y.dylib
	 *   cudart64_XXY.dll
	 */
	char library[40];

	/* derive (decorated) library name */
	{
#if defined(LINUX) && defined(OMR_ENV_DATA64)
#   define OMRCUDA_LIBRARY_PREFIX "libcudart.so"
#   define OMRCUDA_LIBRARY_MAJOR  ".%u"
#   define OMRCUDA_LIBRARY_MINOR  ".%u"
#   define OMRCUDA_LIBRARY_SUFFIX ""
#elif defined(OSX) /* defined(LINUX) && defined(OMR_ENV_DATA64) */
#   define OMRCUDA_LIBRARY_PREFIX "libcudart"
#   define OMRCUDA_LIBRARY_MAJOR  ".%u"
#   define OMRCUDA_LIBRARY_MINOR  ".%u"
#   define OMRCUDA_LIBRARY_SUFFIX ".dylib"
#elif defined(OMR_OS_WINDOWS) /* defined(OSX) */
# if defined(OMR_ENV_DATA64)
#   define OMRCUDA_LIBRARY_PREFIX "cudart64"
# else /* defined(OMR_ENV_DATA64) */
#   define OMRCUDA_LIBRARY_PREFIX "cudart32"
# endif /* defined(OMR_ENV_DATA64) */
#   define OMRCUDA_LIBRARY_MAJOR  "_%u"
#   define OMRCUDA_LIBRARY_MINOR  "%u"
#   define OMRCUDA_LIBRARY_SUFFIX ".dll"
#endif /* defined(LINUX) && defined(OMR_ENV_DATA64) */
		const char *format = NULL;

		if (fileVersion >= 12000) {
			format = OMRCUDA_LIBRARY_PREFIX OMRCUDA_LIBRARY_MAJOR                       OMRCUDA_LIBRARY_SUFFIX;
		} else if (fileVersion > 0) {
			format = OMRCUDA_LIBRARY_PREFIX OMRCUDA_LIBRARY_MAJOR OMRCUDA_LIBRARY_MINOR OMRCUDA_LIBRARY_SUFFIX;
		} else {
			format = OMRCUDA_LIBRARY_PREFIX                                             OMRCUDA_LIBRARY_SUFFIX;
		}

		portLibrary->str_printf(portLibrary, library, sizeof(library), format, fileVersion / 1000, (fileVersion % 1000) / 10);

#undef OMRCUDA_LIBRARY_PREFIX
#undef OMRCUDA_LIBRARY_MAJOR
#undef OMRCUDA_LIBRARY_MINOR
#undef OMRCUDA_LIBRARY_SUFFIX
	}

	/* open the runtime shared library */
	if (0 != portLibrary->sl_open_shared_library(
			portLibrary,
			library,
			&dllHandle,
			OMRPORT_SLOPEN_LAZY)) {
		Trc_PRT_cuda_library_not_found(library);
		goto notFound;
	}

	/* query the runtime version */
	{
		cudaError_t (CUDARTAPI *runtimeGetVersion)(int *runtimeVersion) = NULL;

		if (0 != portLibrary->sl_lookup_name(
				portLibrary,
				dllHandle,
				(char *)"cudaRuntimeGetVersion",
				(uintptr_t *)&runtimeGetVersion,
				"iP")) {
			Trc_PRT_cuda_symbol_not_found("cudaRuntimeGetVersion", "iP");
			goto fail;
		}

		if (cudaSuccess != runtimeGetVersion(&version)) {
			goto fail;
		}

		if (version < 5050) {
			/* ignore this shared library that predates the first supported version */
			goto fail;
		}

		if ((uint32_t)version < fileVersion) {
			/* ignore this shared library that predates its expected version */
			goto fail;
		}

		if ((uint32_t)version <= bestVersion) {
			/* ignore this shared library unless it is a better version */
			goto fail;
		}
	}

	memset(&localFunctions, 0, sizeof(localFunctions));

	/* find the runtime functions */
	if (0 == initializeTable(
			portLibrary,
			dllHandle,
			(uint32_t)version,
			runtimeDescriptors,
			LENGTH_OF(runtimeDescriptors),
			(uintptr_t *)&localFunctions)) {
		/* close the previous runtime shared library if necessary */
		if (0 != globals->runtimeHandle) {
			portLibrary->sl_close_shared_library(portLibrary, globals->runtimeHandle);
		}

		globals->runtimeHandle = dllHandle;

		/* copy all runtime entries to the global function table */
		uintptr_t globalTable = (uintptr_t)globals->functionTable;
		uintptr_t localTable = (uintptr_t)&localFunctions;

		for (uintptr_t index = 0; index < LENGTH_OF(runtimeDescriptors); ++index) {
			uintptr_t offset = runtimeDescriptors[index].offset;

			*(uintptr_t *)(globalTable + offset) = *(uintptr_t *)(localTable + offset);
		}
	} else {
fail:
		if (0 != dllHandle) {
			portLibrary->sl_close_shared_library(portLibrary, dllHandle);
		}

notFound:
		version = 0;
	}

	return (uint32_t)version;
}

/**
 * The possible versions of runtime shared libraries providing the underlying
 * implementation of the CUDA access layer. Entries are qualified by the version
 * expected to be provided and are only considered if the version is newer than
 * the best found so far. Newer versions should appear earlier in the list to
 * minimize the time spent loading older versions.
 */
const uint32_t runtimeFileVersions[] = {
#define OMRCUDA_LIBRARY_VERSION(major, minor) (((major) * 1000) + ((minor) * 10))

	OMRCUDA_LIBRARY_VERSION(12, 0), /* 12.0.0 (Dec 2022) - 12.2.1 (Jul 2023) */
	OMRCUDA_LIBRARY_VERSION(11, 0), /* 11.0.0 (Mar 2020) - 11.8.0 (Oct 2022) */
	OMRCUDA_LIBRARY_VERSION(10, 2), /* 10.2.0 (Nov 2019) */
	OMRCUDA_LIBRARY_VERSION(10, 1), /* 10.1.0 (Feb 2019) */
	OMRCUDA_LIBRARY_VERSION(10, 0), /* 10.0.0 (Sep 2018) */
	OMRCUDA_LIBRARY_VERSION( 9, 2), /*  9.2.0 (May 2018) */
	OMRCUDA_LIBRARY_VERSION( 9, 1), /*  9.1.0 (Dec 2017) */
	OMRCUDA_LIBRARY_VERSION( 9, 0), /*  9.0.0 (Sep 2017) */
	OMRCUDA_LIBRARY_VERSION( 8, 0), /*  8.0.0 (Sep 2016) */
	OMRCUDA_LIBRARY_VERSION( 7, 5), /*  7.5.0 (Sep 2015) */
	OMRCUDA_LIBRARY_VERSION( 7, 0), /*  7.0.0 (Mar 2015) */
	OMRCUDA_LIBRARY_VERSION( 6, 5), /*  6.5.0 (Aug 2014) */
	OMRCUDA_LIBRARY_VERSION( 6, 0), /*  6.0.0 (Apr 2014) */
	OMRCUDA_LIBRARY_VERSION( 5, 5)  /*  5.5.0 (Jul 2013) */

#undef OMRCUDA_LIBRARY_VERSION
};

/**
 * Open the best runtime shared library and return its version.
 * Zero is returned on any failure.
 *
 * @param[in] portLibrary the port library pointer
 * @return the best runtime version or zero on failure
 */
uint32_t
openRuntime(OMRPortLibrary *portLibrary)
{
	uint32_t bestVersion = 0;

#if (defined(LINUX) && defined(OMR_ENV_DATA64)) || defined(OSX)
	/*
	 * This special case is considered first for forward compatiblity.
	 * It commonly involves a link to the newest version available.
	 */
	bestVersion = openRuntimeAndGetVersion(portLibrary, bestVersion, 0);
#endif /* (defined(LINUX) && defined(OMR_ENV_DATA64)) || defined(OSX) */

	for (uint32_t index = 0; index < LENGTH_OF(runtimeFileVersions); ++index) {
		uint32_t fileVersion = runtimeFileVersions[index];

		if (fileVersion <= bestVersion) {
			/* this library isn't expected to be better */
			continue;
		}

		uint32_t version = openRuntimeAndGetVersion(portLibrary, bestVersion, fileVersion);

		if (bestVersion < version) {
			bestVersion = version;
		}
	}

	portLibrary->portGlobals->cudaGlobals.runtimeVersion = bestVersion;

	return bestVersion;
}

/**
 * Ensure all our shared libraries are closed.
 *
 * @param[in] portLibrary the port library pointer
 */
void
closeLibraries(OMRPortLibrary *portLibrary)
{
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	if (0 != globals->runtimeHandle) {
		portLibrary->sl_close_shared_library(portLibrary, globals->runtimeHandle);
		globals->runtimeHandle = 0;
	}

	if (0 != globals->driverHandle) {
		portLibrary->sl_close_shared_library(portLibrary, globals->driverHandle);
		globals->driverHandle = 0;
	}
}

/**
 * Attempt initialization if it has not been attempted previously.
 *
 * @param[in] portLibrary the port library pointer
 */
void
attemptInitialization(OMRPortLibrary *portLibrary)
{
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	MUTEX_ENTER(globals->stateMutex);

	if (J9CUDA_STATE_STARTED == globals->state) {
		/* initialization *has* not been attempted */
		uint32_t newState = J9CUDA_STATE_INITIALIZED;

		if (0 == openRuntime(portLibrary)) {
			goto fail;
		}

		if (0 == openDriver(portLibrary)) {
fail:
			globals->deviceCount = 0;
			newState = J9CUDA_STATE_FAILED;
		}

		if (J9CUDA_NO_ERROR != initConfigData(portLibrary)) {
			newState = J9CUDA_STATE_FAILED;
		}

		if (J9CUDA_STATE_FAILED == newState) {
			Trc_PRT_cuda_getFunctions_failed();
			globals->deviceCount = 0;
		} else {
			Trc_PRT_cuda_getFunctions_initialized();
		}

		if (0 == globals->deviceCount) {
			globals->driverVersion = 0;
			globals->runtimeVersion = 0;
			memset(globals->functionTable, 0, sizeof(globals->functionTable));
			closeLibraries(portLibrary);
		}

		globals->state = newState;
	}

	MUTEX_EXIT(globals->stateMutex);
}

/**
 * Reset all devices recognized at startup, permitting
 * the CUDA runtime and driver to release resources.
 *
 * @param[in] globals the CUDA globals pointer
 */
void
resetDevices(J9CudaGlobalData *globals)
{
	if ((J9CUDA_STATE_INITIALIZED == globals->state) && (0 != globals->deviceCount)) {
		int current = 0;
		J9CudaFunctionTable *functions = (J9CudaFunctionTable *)globals->functionTable;
		cudaError_t result = functions->GetDevice(&current);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_reset_fail1("get", result);
		} else {
			for (uint32_t deviceId = 0; deviceId < globals->deviceCount; ++deviceId) {
				result = functions->SetDevice((int)deviceId);

				if (cudaSuccess != result) {
					Trc_PRT_cuda_reset_fail1("set", result);
					break;
				}

				result = functions->DeviceReset();

				if (cudaSuccess != result) {
					Trc_PRT_cuda_reset_fail2(deviceId, result);
				}
			}

			result = functions->SetDevice(current);

			if (cudaSuccess != result) {
				Trc_PRT_cuda_reset_fail1("restore", result);
			}
		}
	}
}

/**
 * Ensure that initialization is done (or at least has been attempted)
 * and return a pointer our internal function table.
 *
 * @param[in] portLibrary the port library pointer
 * @return a pointer to our function table
 */
VMINLINE J9CudaFunctionTable *
getFunctions(OMRPortLibrary *portLibrary)
{
	Trc_PRT_cuda_getFunctions_entry();

	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	if (J9CUDA_STATE_STARTED == globals->state) {
		/* initialization *may* not have been attempted */
		attemptInitialization(portLibrary);
	}

	J9CudaFunctionTable *functions = (J9CudaFunctionTable *)globals->functionTable;

	Trc_PRT_cuda_getFunctions_exit(functions);

	return functions;
}

/**
 * Return the number of devices visible to this process (the result
 * is influenced by the environment variable CUDA_VISIBLE_DEVICES).
 *
 * @param[in] portLibrary the port library pointer
 * @return the number of devices
 */
VMINLINE uint32_t
getDeviceCount(OMRPortLibrary *portLibrary)
{
	return portLibrary->portGlobals->cudaGlobals.deviceCount;
}

/**
 * Determine whether deviceId is a valid device identifier.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @return J9CUDA_NO_ERROR on success; J9CUDA_ERROR_NO_DEVICE or J9CUDA_ERROR_INVALID_DEVICE on failure
 */
VMINLINE J9CudaError
validateDeviceId(OMRPortLibrary *portLibrary, uint32_t deviceId)
{
	uint32_t deviceCount = getDeviceCount(portLibrary);

	return (0 == deviceCount) ? J9CUDA_ERROR_NO_DEVICE : (deviceId < deviceCount) ? J9CUDA_NO_ERROR : J9CUDA_ERROR_INVALID_DEVICE;
}

/**
 * Each thread has an instance of this type to track which devices
 * have been initialized for that thread. Immediately following an
 * instanceo of ThreadState is an array of uint32_t which stores a
 * bit set tracking which devices have been initialized by the
 * related thread.
 */
class ThreadState
{
private:
	OMRPortLibrary *portLibrary;

	/* uint32_t initWords[]; */

	/**
	 * The finalizer for our thread-local storage.
	 *
	 * @param[in] handle the thread state pointer
	 */
	static void
	finalizer(void *handle)
	{
		Trc_PRT_cuda_ThreadState_finalizer_entry(handle);

		ThreadState *state = (ThreadState *)handle;

		OMRPORT_ACCESS_FROM_OMRPORT(state->portLibrary);

		J9CUDA_FREE_MEMORY(state);

		Trc_PRT_cuda_ThreadState_finalizer_exit();
	}

	/**
	 * Return a pointer to the bit set (immediately following this object)
	 * tracking which devices have been initialized by the related thread.
	 *
	 * @param[in] deviceId the device identifier
	 * @return a pointer to the state word for the specified device
	 */
	VMINLINE uint32_t *
	getStateWord(uint32_t deviceId)
	{
		uint32_t *initWords = (uint32_t *)(this + 1);

		return &initWords[deviceId / 32];
	}

	/**
	 * Return the ThreadState pointer for the calling thread
	 * (allocating space if necessary).
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] deviceCount the number of devices available
	 * @return the ThreadState pointer for the calling thread
	 */
	static ThreadState *
	getStateForCurrentThread(OMRPortLibrary *portLibrary, uint32_t deviceCount)
	{
		Trc_PRT_cuda_ThreadState_getCurrent_entry();

		omrthread_t self = omrthread_self();
		J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
		ThreadState *state = (ThreadState *)omrthread_tls_get(self, globals->tlsKey);

		if (NULL == state) {
			/*
			 * The size of the bit set following a state object depends
			 * on the number of visible devices.
			 */
			uint32_t initWordsSize = ((deviceCount + 31) / 32) * sizeof(uint32_t);

			OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

			state = (ThreadState *)J9CUDA_ALLOCATE_MEMORY(sizeof(ThreadState) + initWordsSize);

			if (NULL != state) {
				if (0 == omrthread_tls_set(self, globals->tlsKey, state)) {
					state->portLibrary = portLibrary;
					memset(state->getStateWord(0), 0, initWordsSize);
				} else {
					J9CUDA_FREE_MEMORY(state);
					state = NULL;
				}
			}
		}

		Trc_PRT_cuda_ThreadState_getCurrent_exit(state);

		return state;
	}

	/**
	 * Clear the bit indicating that the specified device has been initialized.
	 *
	 * @param[in] deviceId the device identifier
	 */
	VMINLINE void
	clear(uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_clear_entry(this, deviceId);

		uint32_t shift = deviceId & 0x1f;

		*getStateWord(deviceId) &= ~(((uint32_t) 1U) << shift);

		Trc_PRT_cuda_ThreadState_clear_exit();
	}

	/**
	 * Set the bit indicating that the specified device has been initialized.
	 *
	 * @param[in] deviceId the device identifier
	 */
	VMINLINE void
	set(uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_set_entry(this, deviceId);

		uint32_t shift = deviceId & 0x1f;

		*getStateWord(deviceId) |= ((uint32_t)1U) << shift;

		Trc_PRT_cuda_ThreadState_set_exit();
	}

	/**
	 * Test the bit indicating whether the specified device has been initialized.
	 *
	 * @param[in] deviceId the device identifier
	 * @return a non-zero value if the device has been initialized, zero otherwise
	 */
	VMINLINE uint32_t
	test(uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_test_entry(this, deviceId);

		uint32_t shift = deviceId & 0x1f;
		uint32_t result = (*getStateWord(deviceId) >> shift) & 1;

		Trc_PRT_cuda_ThreadState_test_exit(result);

		return result;
	}

public:
	/**
	 * Perform 'startup' activities relating to ThreadState.
	 *
	 * @param[in] globals the CUDA global data pointer
	 * @return 0 on success, any other value on failure
	 */
	static int32_t
	startup(J9CudaGlobalData *globals)
	{
		Trc_PRT_cuda_ThreadState_startup_entry(globals);

		int32_t result = 1;

		if (0 == omrthread_tls_alloc_with_finalizer(&globals->tlsKey, finalizer)) {
			result = 0;
		}

		Trc_PRT_cuda_ThreadState_startup_exit(result);

		return result;
	}

	/**
	 * Perform 'shutdown' activities relating to ThreadState.
	 *
	 * @param[in] globals the CUDA global data pointer
	 */
	static void
	shutdown(J9CudaGlobalData *globals)
	{
		Trc_PRT_cuda_ThreadState_shutdown_entry(globals);

		if (0 != globals->tlsKey) {
			omrthread_tls_free(globals->tlsKey);
			globals->tlsKey = 0;
		}

		Trc_PRT_cuda_ThreadState_shutdown_exit();
	}

	/**
	 * Ensure that we have an initialized CUDA device context for the current
	 * thread and device.
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] functions the function table pointer
	 * @param[in] deviceId the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	static J9CudaError
	initDeviceForCurrentThread(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions, uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_initCurrent_entry(deviceId);

		uint32_t deviceCount = getDeviceCount(portLibrary);
		J9CudaError result = J9CUDA_ERROR_INVALID_DEVICE;

		if (deviceId < deviceCount) {
			ThreadState *state = getStateForCurrentThread(portLibrary, deviceCount);

			if (NULL == state) {
				result = J9CUDA_ERROR_MEMORY_ALLOCATION;
			} else if (state->test(deviceId)) {
				/* previously initialized */
				result = J9CUDA_NO_ERROR;
			} else {
				/* initialize the device context for the current thread */
				cudaError_t error = functions->Free(NULL);

				result = mapRuntime(portLibrary, error);

				if (J9CUDA_NO_ERROR == result) {
					state->set(deviceId);
				}
			}
		}

		Trc_PRT_cuda_ThreadState_initCurrent_exit(result);

		return result;
	}

	/**
	 * Record that we have an initialized CUDA device context for the current
	 * thread and device.
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] deviceId the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	static J9CudaError
	markDeviceForCurrentThread(OMRPortLibrary *portLibrary, uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_markCurrent_entry(deviceId);

		uint32_t deviceCount = getDeviceCount(portLibrary);
		J9CudaError result = J9CUDA_ERROR_INVALID_DEVICE;

		if (deviceId < deviceCount) {
			ThreadState *state = getStateForCurrentThread(portLibrary, deviceCount);

			if (NULL == state) {
				result = J9CUDA_ERROR_MEMORY_ALLOCATION;
			} else {
				result = J9CUDA_NO_ERROR;
				state->set(deviceId);
			}
		}

		Trc_PRT_cuda_ThreadState_markCurrent_exit(result);

		return result;
	}

	/**
	 * Record that we do not have an initialized CUDA device context for the
	 * current thread and device.
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] deviceId the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	static J9CudaError
	unmarkDeviceForCurrentThread(OMRPortLibrary *portLibrary, uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_unmarkCurrent_entry(deviceId);

		uint32_t deviceCount = getDeviceCount(portLibrary);
		J9CudaError result = J9CUDA_ERROR_INVALID_DEVICE;

		if (deviceId < deviceCount) {
			ThreadState *state = getStateForCurrentThread(portLibrary, deviceCount);

			if (NULL == state) {
				result = J9CUDA_ERROR_MEMORY_ALLOCATION;
			} else {
				result = J9CUDA_NO_ERROR;
				state->clear(deviceId);
			}
		}

		Trc_PRT_cuda_ThreadState_unmarkCurrent_exit(result);

		return result;
	}
};

/**
 * Operation types to be used with the template function 'withDevice'
 * that neither require nor guarantee an initialized CUDA device context
 * upon successful execution should extend this type.
 */
struct InitializerNotNeeded {
	/**
	 * Prepare the calling thread for use of the specified device.
	 *
	 * @param[in] (unused) the port library pointer
	 * @param[in] (unused) the function table pointer
	 * @param[in] (unused) the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	VMINLINE J9CudaError
	prepare(OMRPortLibrary *, J9CudaFunctionTable *, uint32_t) const
	{
		/* Initialization not needed. */
		return J9CUDA_NO_ERROR;
	}

	/**
	 * Mark the calling thread as having successfully used the specified device.
	 *
	 * @param[in] (unused) the port library pointer
	 * @param[in] (unused) the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	VMINLINE J9CudaError
	onSuccess(OMRPortLibrary *, uint32_t) const
	{
		/* Initialization not needed. */
		return J9CUDA_NO_ERROR;
	}
};

/**
 * Operation types to be used with the template function 'withDevice'
 * that, when successful, implicitly initialize a CUDA device context
 * should extend this type.
 */
struct InitializedAfter : public InitializerNotNeeded {
	/**
	 * Mark the calling thread as having successfully used the specified device.
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] deviceId the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	VMINLINE J9CudaError
	onSuccess(OMRPortLibrary *portLibrary, uint32_t deviceId) const
	{
		return ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
	}
};

/**
 * Operation types to be used with the template function 'withDevice'
 * that require an initialized CUDA device context before execution
 * should extend this type.
 */
struct InitializedBefore : public InitializerNotNeeded {
	/**
	 * Prepare the calling thread for use of the specified device.
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] functions the function table pointer
	 * @param[in] deviceId the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	VMINLINE J9CudaError
	prepare(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions, uint32_t deviceId) const
	{
		return ThreadState::initDeviceForCurrentThread(portLibrary, functions, deviceId);
	}
};

/**
 * Execute the given operation in the context of the specified device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] operation the operation to be performed
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
template<typename Operation>
VMINLINE J9CudaError
withDevice(OMRPortLibrary *portLibrary, uint32_t deviceId, Operation &operation)
{
	Trc_PRT_cuda_withDevice_entry(deviceId);

	int current = 0;
	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	/*
	 * If any of the (required) functions is not available, then GetDevice will
	 * be NULL in which case we simply return J9CUDA_ERROR_NO_DEVICE. If GetDevice
	 * is not NULL, then we need not check again that a required function is
	 * available (neither here nor within the Operation type).
	 */
	if (NULL != functions->GetDevice) {
		cudaError_t error = functions->GetDevice(&current);

		result = mapRuntime(portLibrary, error);

		if (J9CUDA_NO_ERROR != result) {
			Trc_PRT_cuda_withDevice_get_fail(result);
			goto done;
		}

		if (current != (int)deviceId) {
			error = functions->SetDevice((int)deviceId);

			result = mapRuntime(portLibrary, error);

			if (J9CUDA_NO_ERROR != result) {
				Trc_PRT_cuda_withDevice_set_fail(result);
				goto done;
			}
		}

		result = operation.prepare(portLibrary, functions, deviceId);

		if (J9CUDA_NO_ERROR != result) {
			Trc_PRT_cuda_withDevice_prepare_fail(result);
			goto restore;
		}

		result = operation.execute(portLibrary, functions);

		if (J9CUDA_NO_ERROR != result) {
			Trc_PRT_cuda_withDevice_execute_fail(result);
			goto restore;
		}

		result = operation.onSuccess(portLibrary, deviceId);

		if (J9CUDA_NO_ERROR != result) {
			Trc_PRT_cuda_withDevice_onSuccess_fail(result);
		}

restore:
		if (current != (int)deviceId) {
			error = functions->SetDevice(current);

			if (cudaSuccess != error) {
				Trc_PRT_cuda_withDevice_restore_fail(error);

				if (J9CUDA_NO_ERROR == result) {
					result = mapRuntime(portLibrary, error);
				}
			}
		}
	}

done:
	Trc_PRT_cuda_withDevice_exit(result);

	return result;
}

} /* namespace */

/**
 * Start the CUDA access library.
 *
 * @param[in] portLibrary the port library pointer
 * @return 0 on success or a non-zero value on failure.
 * Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_CUDA
 */
extern "C" int32_t
omrcuda_startup(OMRPortLibrary *portLibrary)
{
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	if (!MUTEX_INIT(globals->stateMutex)) {
		goto fail;
	}

	if (0 != ThreadState::startup(globals)) {
		goto fail;
	}

	globals->state = J9CUDA_STATE_STARTED;

	return 0;

fail:
	globals->state = J9CUDA_STATE_FAILED;

	return OMRPORT_ERROR_STARTUP_CUDA;
}

/**
 * Shutdown the CUDA access library.
 *
 * @param[in] portLibrary the port library pointer
 */
extern "C" void
omrcuda_shutdown(OMRPortLibrary *portLibrary)
{
	Trc_PRT_cuda_shutdown_entry();

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	if (NULL != portLibrary->cuda_configData) {
		J9CUDA_FREE_MEMORY(portLibrary->cuda_configData);
		portLibrary->cuda_configData = NULL;
	}

	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	switch (globals->state) {
	case J9CUDA_STATE_INITIALIZED:
		resetDevices(globals);
		/* FALL-THROUGH */

	case J9CUDA_STATE_STARTED:
		closeLibraries(portLibrary);
		ThreadState::shutdown(globals);
		MUTEX_DESTROY(globals->stateMutex);
		break;

	default:
		break;
	}

	/* clear everything */
	memset(globals, 0, sizeof(J9CudaGlobalData));

	Trc_PRT_cuda_shutdown_exit();
}

namespace
{

/**
 * The closure required to allocate device memory in conjunction with use of
 * the template function 'withDevice'.
 *
 * @param[in] byteCount the size in bytes of the required block
 * @param[out] deviceAddress the device address
 */
struct Allocate : public InitializedAfter {
	uintptr_t const byteCount;
	void *deviceAddress;

	explicit VMINLINE
	Allocate(uintptr_t byteCount)
		: byteCount(byteCount)
		, deviceAddress(NULL)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		cudaError_t error = functions->Malloc(&deviceAddress, byteCount);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Allocate a block of memory on the specified device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] size the number of bytes requested
 * @param[out] deviceAddressOut where the device memory address should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceAlloc(OMRPortLibrary *portLibrary, uint32_t deviceId, uintptr_t size, void **deviceAddressOut)
{
	Trc_PRT_cuda_deviceAlloc_entry(deviceId, size);

	Allocate operation(size);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_deviceAlloc_result(operation.deviceAddress);

		*deviceAddressOut = operation.deviceAddress;
	}

	Trc_PRT_cuda_deviceAlloc_exit(result);

	return result;
}

/**
 * Query whether a device can access a peer device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] peerDeviceId the peer device identifier
 * @param[out] canAccessPeerOut where the query result should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceCanAccessPeer(OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId, BOOLEAN *canAccessPeerOut)
{
	Trc_PRT_cuda_deviceCanAccessPeer_entry(deviceId, peerDeviceId);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if (J9CUDA_NO_ERROR == result) {
		result = validateDeviceId(portLibrary, peerDeviceId);
	}

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->DeviceCanAccessPeer)) {
		int access = 0;
		cudaError_t error = functions->DeviceCanAccessPeer(&access, (int)deviceId, (int)peerDeviceId);

		if (cudaSuccess != error) {
			result = mapRuntime(portLibrary, error);
		} else {
			Trc_PRT_cuda_deviceCanAccessPeer_result(access);

			*canAccessPeerOut = 0 != access;
		}
	}

	Trc_PRT_cuda_deviceCanAccessPeer_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to disable access to a peer device in conjunction with
 * use of the template function 'withDevice'.
 *
 * @param[in] peerDeviceId the peer device identifier
 */
struct DisablePeerAccess : public InitializerNotNeeded {
	uint32_t const peerDeviceId;

	explicit VMINLINE
	DisablePeerAccess(uint32_t peerDeviceId)
		: peerDeviceId(peerDeviceId)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = functions->DeviceDisablePeerAccess((int)peerDeviceId);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Disable access to a peer device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] peerDeviceId the peer device identifier
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceDisablePeerAccess(OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId)
{
	Trc_PRT_cuda_deviceDisablePeerAccess_entry(deviceId, peerDeviceId);

	const DisablePeerAccess operation(peerDeviceId);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceDisablePeerAccess_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to enable access to a peer device in conjunction with
 * use of the template function 'withDevice'.
 *
 * @param[in] peerDeviceId the peer device identifier
 */
struct EnablePeerAccess : public InitializerNotNeeded {
	uint32_t const peerDeviceId;

	explicit VMINLINE
	EnablePeerAccess(uint32_t peerDeviceId)
		: peerDeviceId(peerDeviceId)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = functions->DeviceEnablePeerAccess((int)peerDeviceId, 0);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Enable access to a peer device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] peerDeviceId the peer device identifier
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceEnablePeerAccess(OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t peerDeviceId)
{
	Trc_PRT_cuda_deviceEnablePeerAccess_entry(deviceId, peerDeviceId);

	const EnablePeerAccess operation(peerDeviceId);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceEnablePeerAccess_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to release device memory in conjunction with use of
 * the template function 'withDevice'.
 *
 * @param[in] deviceAddress the device address
 */
struct Free : public InitializedAfter {
	void *const deviceAddress;

	explicit VMINLINE
	Free(void *deviceAddress)
		: deviceAddress(deviceAddress)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = functions->Free(deviceAddress);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Release a block of memory on the specified device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] deviceAddress the device memory address
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceFree(OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress)
{
	Trc_PRT_cuda_deviceFree_entry(deviceId, deviceAddress);

	const Free operation(deviceAddress);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceFree_exit(result);

	return result;
}

/**
 * Query a device attribute. The set of device attributes is defined by the
 * enum J9CudaDeviceAttribute, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] attribute the attribute to query
 * @param[out] valueOut where the value of the attribute should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetAttribute(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceAttribute attribute, int32_t *valueOut)
{
	Trc_PRT_cuda_deviceGetAttribute_entry(deviceId, attribute);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;
	cudaDeviceAttr deviceAttribute = cudaDevAttrWarpSize;

	if (NULL != functions->DeviceGetAttribute) {
		switch (attribute) {
		default:
			result = J9CUDA_ERROR_INVALID_VALUE;
			goto done;

		case J9CUDA_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT:
			deviceAttribute = cudaDevAttrAsyncEngineCount;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY:
			deviceAttribute = cudaDevAttrCanMapHostMemory;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_CLOCK_RATE:
			deviceAttribute = cudaDevAttrClockRate;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY:
		case J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR:
			deviceAttribute = cudaDevAttrComputeCapabilityMajor;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR:
			deviceAttribute = cudaDevAttrComputeCapabilityMinor;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_MODE:
			deviceAttribute = cudaDevAttrComputeMode;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS:
			deviceAttribute = cudaDevAttrConcurrentKernels;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_ECC_ENABLED:
			deviceAttribute = cudaDevAttrEccEnabled;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH:
			deviceAttribute = cudaDevAttrGlobalMemoryBusWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_INTEGRATED:
			deviceAttribute = cudaDevAttrIntegrated;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT:
			deviceAttribute = cudaDevAttrKernelExecTimeout;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_L2_CACHE_SIZE:
			deviceAttribute = cudaDevAttrL2CacheSize;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X:
			deviceAttribute = cudaDevAttrMaxBlockDimX;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y:
			deviceAttribute = cudaDevAttrMaxBlockDimY;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z:
			deviceAttribute = cudaDevAttrMaxBlockDimZ;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X:
			deviceAttribute = cudaDevAttrMaxGridDimX;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y:
			deviceAttribute = cudaDevAttrMaxGridDimY;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z:
			deviceAttribute = cudaDevAttrMaxGridDimZ;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_PITCH:
			deviceAttribute = cudaDevAttrMaxPitch;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK:
			deviceAttribute = cudaDevAttrMaxRegistersPerBlock;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK:
			deviceAttribute = cudaDevAttrMaxSharedMemoryPerBlock;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK:
			deviceAttribute = cudaDevAttrMaxThreadsPerBlock;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR:
			deviceAttribute = cudaDevAttrMaxThreadsPerMultiProcessor;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_LAYERS:
			deviceAttribute = cudaDevAttrMaxSurface1DLayeredLayers;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_WIDTH:
			deviceAttribute = cudaDevAttrMaxSurface1DLayeredWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_WIDTH:
			deviceAttribute = cudaDevAttrMaxSurface1DWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_HEIGHT:
			deviceAttribute = cudaDevAttrMaxSurface2DHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_HEIGHT:
			deviceAttribute = cudaDevAttrMaxSurface2DLayeredHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_LAYERS:
			deviceAttribute = cudaDevAttrMaxSurface2DLayeredLayers;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_WIDTH:
			deviceAttribute = cudaDevAttrMaxSurface2DLayeredWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_WIDTH:
			deviceAttribute = cudaDevAttrMaxSurface2DWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_DEPTH:
			deviceAttribute = cudaDevAttrMaxSurface3DDepth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_HEIGHT:
			deviceAttribute = cudaDevAttrMaxSurface3DHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_WIDTH:
			deviceAttribute = cudaDevAttrMaxSurface3DWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS:
			deviceAttribute = cudaDevAttrMaxSurfaceCubemapLayeredLayers;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH:
			deviceAttribute = cudaDevAttrMaxSurfaceCubemapLayeredWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_WIDTH:
			deviceAttribute = cudaDevAttrMaxSurfaceCubemapWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS:
			deviceAttribute = cudaDevAttrMaxTexture1DLayeredLayers;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture1DLayeredWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture1DLinearWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture1DMipmappedWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture1DWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT:
			deviceAttribute = cudaDevAttrMaxTexture2DGatherHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture2DGatherWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT:
			deviceAttribute = cudaDevAttrMaxTexture2DHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_HEIGHT:
			deviceAttribute = cudaDevAttrMaxTexture2DLayeredHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_LAYERS:
			deviceAttribute = cudaDevAttrMaxTexture2DLayeredLayers;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture2DLayeredWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_HEIGHT:
			deviceAttribute = cudaDevAttrMaxTexture2DLinearHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_PITCH:
			deviceAttribute = cudaDevAttrMaxTexture2DLinearPitch;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture2DLinearWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT:
			deviceAttribute = cudaDevAttrMaxTexture2DMipmappedHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture2DMipmappedWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture2DWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH:
			deviceAttribute = cudaDevAttrMaxTexture3DDepth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE:
			deviceAttribute = cudaDevAttrMaxTexture3DDepthAlt;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT:
			deviceAttribute = cudaDevAttrMaxTexture3DHeight;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE:
			deviceAttribute = cudaDevAttrMaxTexture3DHeightAlt;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH:
			deviceAttribute = cudaDevAttrMaxTexture3DWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE:
			deviceAttribute = cudaDevAttrMaxTexture3DWidthAlt;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS:
			deviceAttribute = cudaDevAttrMaxTextureCubemapLayeredLayers;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH:
			deviceAttribute = cudaDevAttrMaxTextureCubemapLayeredWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_WIDTH:
			deviceAttribute = cudaDevAttrMaxTextureCubemapWidth;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE:
			deviceAttribute = cudaDevAttrMemoryClockRate;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT:
			deviceAttribute = cudaDevAttrMultiProcessorCount;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_PCI_BUS_ID:
			deviceAttribute = cudaDevAttrPciBusId;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_PCI_DEVICE_ID:
			deviceAttribute = cudaDevAttrPciDeviceId;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID:
			deviceAttribute = cudaDevAttrPciDomainId;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_STREAM_PRIORITIES_SUPPORTED:
			deviceAttribute = cudaDevAttrStreamPrioritiesSupported;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT:
			deviceAttribute = cudaDevAttrSurfaceAlignment;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_TCC_DRIVER:
			deviceAttribute = cudaDevAttrTccDriver;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT:
			deviceAttribute = cudaDevAttrTextureAlignment;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT:
			deviceAttribute = cudaDevAttrTexturePitchAlignment;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY:
			deviceAttribute = cudaDevAttrTotalConstantMemory;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING:
			deviceAttribute = cudaDevAttrUnifiedAddressing;
			break;
		case J9CUDA_DEVICE_ATTRIBUTE_WARP_SIZE:
			deviceAttribute = cudaDevAttrWarpSize;
			break;
		}

		int value = 0;
		cudaError_t error = functions->DeviceGetAttribute(&value, deviceAttribute, (int)deviceId);

		if (cudaSuccess != error) {
fail:
			result = mapRuntime(portLibrary, error);
			goto done;
		}

		if (J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY == attribute) {
			int minor = 0;

			error = functions->DeviceGetAttribute(&minor, cudaDevAttrComputeCapabilityMinor, (int)deviceId);

			if (cudaSuccess != error) {
				goto fail;
			}

			value = (value * 10) + minor;
		}

		result = J9CUDA_NO_ERROR;

		Trc_PRT_cuda_deviceGetAttribute_result(attribute, value);
		*valueOut = (int32_t)value;
	}

done:
	Trc_PRT_cuda_deviceGetAttribute_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to query the cache configuration of a device in
 * conjunction with use of the template function 'withDevice'.
 *
 * @return the device cache configuration
 */
struct DeviceGetCacheConfig : public InitializedAfter {
	cudaFuncCache config;

	VMINLINE
	DeviceGetCacheConfig()
		: config(cudaFuncCachePreferNone)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		cudaError_t error = functions->DeviceGetCacheConfig(&config);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Query the cache configuration of a device. Possible cache configurations are
 * defined by the enum J9CudaCacheConfig, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[out] configOut where the cache configuration should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetCacheConfig(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaCacheConfig *configOut)
{
	Trc_PRT_cuda_deviceGetCacheConfig_entry(deviceId);

	DeviceGetCacheConfig operation;

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_deviceGetCacheConfig_result(operation.config);

		switch (operation.config) {
		default:
			result = J9CUDA_ERROR_INVALID_VALUE;
			break;

		case cudaFuncCachePreferNone:
			*configOut = J9CUDA_CACHE_CONFIG_PREFER_NONE;
			break;
		case cudaFuncCachePreferShared:
			*configOut = J9CUDA_CACHE_CONFIG_PREFER_SHARED;
			break;
		case cudaFuncCachePreferL1:
			*configOut = J9CUDA_CACHE_CONFIG_PREFER_L1;
			break;
		case cudaFuncCachePreferEqual:
			*configOut = J9CUDA_CACHE_CONFIG_PREFER_EQUAL;
			break;
		}
	}

	Trc_PRT_cuda_deviceGetCacheConfig_exit(result);

	return result;
}

/**
 * Return the number of devices visible to this process (the result
 * is influenced by the environment variable CUDA_VISIBLE_DEVICES).
 *
 * @param[in] portLibrary the port library pointer
 * @param[out] countOut where the device count should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetCount(OMRPortLibrary *portLibrary, uint32_t *countOut)
{
	Trc_PRT_cuda_deviceGetCount_entry();

	/* Ensure that initialization has been done. */
	getFunctions(portLibrary);

	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;

	Trc_PRT_cuda_deviceGetCount_result(globals->deviceCount);

	*countOut = globals->deviceCount;

	Trc_PRT_cuda_deviceGetCount_exit(J9CUDA_NO_ERROR);

	return J9CUDA_NO_ERROR;
}

namespace
{

/**
 * The closure required to query some limit of a device in conjunction with
 * use of the template function 'withDevice'.
 *
 * @param[in] limit the limit to be queried
 * @return the value of the requested limit
 */
struct DeviceGetLimit : public InitializedAfter {
	J9CudaDeviceLimit const limit;
	size_t value;

	explicit VMINLINE
	DeviceGetLimit(J9CudaDeviceLimit limit)
		: limit(limit)
		, value(0)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		cudaLimit deviceLimit = cudaLimitStackSize;

		switch (limit) {
		default:
			return J9CUDA_ERROR_INVALID_VALUE;

		case J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT:
			deviceLimit = cudaLimitDevRuntimePendingLaunchCount;
			break;
		case J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_SYNC_DEPTH:
			deviceLimit = cudaLimitDevRuntimeSyncDepth;
			break;
		case J9CUDA_DEVICE_LIMIT_MALLOC_HEAP_SIZE:
			deviceLimit = cudaLimitMallocHeapSize;
			break;
		case J9CUDA_DEVICE_LIMIT_PRINTF_FIFO_SIZE:
			deviceLimit = cudaLimitPrintfFifoSize;
			break;
		case J9CUDA_DEVICE_LIMIT_STACK_SIZE:
			deviceLimit = cudaLimitStackSize;
			break;
		}

		cudaError_t error = functions->DeviceGetLimit(&value, deviceLimit);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Query a limit of a device. The set of device limits is defined by the
 * enum J9CudaDeviceLimit, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] limit the limit being queried
 * @param[out] valueOut where the limit value should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetLimit(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceLimit limit, uintptr_t *valueOut)
{
	Trc_PRT_cuda_deviceGetLimit_entry(deviceId, limit);

	DeviceGetLimit operation(limit);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_deviceGetLimit_result(operation.value);

		*valueOut = operation.value;
	}

	Trc_PRT_cuda_deviceGetLimit_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to query memory information of a device in conjunction
 * with use of the template function 'withDevice'.
 *
 * @return the number of bytes of free memory on the device
 * @return the number of bytes of memory on the device
 */
struct DeviceGetMemInfo : public InitializedAfter {
	size_t freeBytes;
	size_t totalBytes;

	VMINLINE
	DeviceGetMemInfo()
		: freeBytes(0)
		, totalBytes(0)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		cudaError_t error = functions->MemGetInfo(&freeBytes, &totalBytes);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Query the total amount and free memory on a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[out] freeOut where the number of bytes of free device memory should be stored
 * @param[out] totalOut where the total number of bytes of device memory should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetMemInfo(OMRPortLibrary *portLibrary, uint32_t deviceId, uintptr_t *freeOut, uintptr_t *totalOut)
{
	Trc_PRT_cuda_deviceGetMemInfo_entry(deviceId);

	DeviceGetMemInfo operation;

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_deviceGetMemInfo_result(operation.freeBytes, operation.totalBytes);

		*freeOut = operation.freeBytes;
		*totalOut = operation.totalBytes;
	}

	Trc_PRT_cuda_deviceGetMemInfo_exit(result);

	return result;
}

/**
 * Query the name of a device. The name is returned as a nul-terminated string.
 * The parameter, nameSize, must include room for the trailing nul.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] nameSize the size of the buffer where the name should be stored
 * @param[out] nameOut the buffer where the name should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetName(OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t nameSize, char *nameOut)
{
	Trc_PRT_cuda_deviceGetName_entry(deviceId);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = getDeviceName(functions, deviceId, nameSize, nameOut);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_deviceGetName_result(nameOut);
	}

	Trc_PRT_cuda_deviceGetName_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to query the shared memory configuration of a device
 * in conjunction with use of the template function 'withDevice'.
 *
 * @return the shared memory configuration
 */
struct DeviceGetSharedMemConfig : public InitializedAfter {
	cudaSharedMemConfig config;

	VMINLINE
	DeviceGetSharedMemConfig()
		: config(cudaSharedMemBankSizeDefault)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		cudaError_t error = functions->DeviceGetSharedMemConfig(&config);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Query the shared memory configuration of a device. The set of shared memory
 * configurations is defined by the enum J9CudaSharedMemConfig, declared in
 * omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[out] configOut where the shared memory configurations should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetSharedMemConfig(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaSharedMemConfig *configOut)
{
	Trc_PRT_cuda_deviceGetSharedMemConfig_entry(deviceId);

	DeviceGetSharedMemConfig operation;

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_deviceGetSharedMemConfig_result(operation.config);

		switch (operation.config) {
		default:
			result = J9CUDA_ERROR_INVALID_VALUE;
			break;

		case cudaSharedMemBankSizeDefault:
			*configOut = J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
			break;
		case cudaSharedMemBankSizeFourByte:
			*configOut = J9CUDA_SHARED_MEM_CONFIG_4_BYTE_BANK_SIZE;
			break;
		case cudaSharedMemBankSizeEightByte:
			*configOut = J9CUDA_SHARED_MEM_CONFIG_8_BYTE_BANK_SIZE;
			break;
		}
	}

	Trc_PRT_cuda_deviceGetSharedMemConfig_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to query stream priority limits of a device in
 * conjunction with use of the template function 'withDevice'.
 *
 * @return the greatest stream priority
 * @return the least stream priority
 */
struct DeviceGetPriorities : public InitializedAfter {
	int greatestPriority;
	int leastPriority;

	VMINLINE
	DeviceGetPriorities()
		: greatestPriority(0)
		, leastPriority(0)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		cudaError_t error = functions->DeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Query the limits on stream priorities of a device. Note that stream priorities
 * follow a convention where lower numbers imply greater priorities, thus upon
 * successful return, (*greatestPriorityOut <= *leastPriorityOut).
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[out] leastPriorityOut where the least possible priority should be stored
 * @param[out] greatestPriorityOut where the greatest possible priority should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceGetStreamPriorityRange(OMRPortLibrary *portLibrary, uint32_t deviceId, int32_t *leastPriorityOut, int32_t *greatestPriorityOut)
{
	Trc_PRT_cuda_deviceGetStreamPriorityRange_entry(deviceId);

	DeviceGetPriorities operation;

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_deviceGetStreamPriorityRange_result(
			operation.leastPriority,
			operation.greatestPriority);

		*leastPriorityOut = operation.leastPriority;
		*greatestPriorityOut = operation.greatestPriority;
	}

	Trc_PRT_cuda_deviceGetStreamPriorityRange_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to reset a device in conjunction with use of the
 * template function 'withDevice'.
 */
struct DeviceReset : public InitializerNotNeeded {
	VMINLINE
	DeviceReset()
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = functions->DeviceReset();

		return mapRuntime(portLibrary, error);
	}

	/**
	 * Unmark the calling thread as having successfully used the specified device.
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] deviceId the device identifier
	 * @return J9CUDA_NO_ERROR on success, any other value on failure
	 */
	VMINLINE J9CudaError
	onSuccess(OMRPortLibrary *portLibrary, uint32_t deviceId) const
	{
		return ThreadState::unmarkDeviceForCurrentThread(portLibrary, deviceId);
	}
};

} /* namespace */

/**
 * Reset a device. Care must be taken using this function. It immediately
 * invalidates all resources associated with the specified device (e.g. memory,
 * streams). No other threads should be using the device at the time of a
 * call to this function.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceReset(OMRPortLibrary *portLibrary, uint32_t deviceId)
{
	Trc_PRT_cuda_deviceReset_entry(deviceId);

	const DeviceReset operation;

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceReset_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to set the cache configuration of a device in
 * conjunction with use of the template function 'withDevice'.
 *
 * @param[in] config the requested cache configuration
 */
struct DeviceSetCacheConfig : public InitializedAfter {
	J9CudaCacheConfig const config;

	explicit VMINLINE
	DeviceSetCacheConfig(J9CudaCacheConfig config)
		: config(config)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaFuncCache deviceConfig = cudaFuncCachePreferEqual;

		switch (config) {
		default:
			return J9CUDA_ERROR_INVALID_VALUE;

		case J9CUDA_CACHE_CONFIG_PREFER_EQUAL:
			deviceConfig = cudaFuncCachePreferEqual;
			break;
		case J9CUDA_CACHE_CONFIG_PREFER_L1:
			deviceConfig = cudaFuncCachePreferL1;
			break;
		case J9CUDA_CACHE_CONFIG_PREFER_NONE:
			deviceConfig = cudaFuncCachePreferNone;
			break;
		case J9CUDA_CACHE_CONFIG_PREFER_SHARED:
			deviceConfig = cudaFuncCachePreferShared;
			break;
		}

		cudaError_t error = functions->DeviceSetCacheConfig(deviceConfig);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Set the cache configuration of a device. Possible cache configurations are
 * defined by the enum J9CudaCacheConfig, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] config the requested cache configuration
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceSetCacheConfig(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaCacheConfig config)
{
	Trc_PRT_cuda_deviceSetCacheConfig_entry(deviceId, config);

	const DeviceSetCacheConfig operation(config);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceSetCacheConfig_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to set a device limit in conjunction with use of the
 * template function 'withDevice'.
 *
 * @param[in] limit the limit to be adjusted
 * @param[in] value the new value of the limit
 */
struct DeviceSetLimit : public InitializedAfter {
	J9CudaDeviceLimit const limit;
	uintptr_t const value;

	VMINLINE
	DeviceSetLimit(J9CudaDeviceLimit limit, uintptr_t value)
		: limit(limit)
		, value(value)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaLimit deviceLimit = cudaLimitStackSize;

		switch (limit) {
		default:
			return J9CUDA_ERROR_INVALID_VALUE;

		case J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT:
			deviceLimit = cudaLimitDevRuntimePendingLaunchCount;
			break;
		case J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_SYNC_DEPTH:
			deviceLimit = cudaLimitDevRuntimeSyncDepth;
			break;
		case J9CUDA_DEVICE_LIMIT_MALLOC_HEAP_SIZE:
			deviceLimit = cudaLimitMallocHeapSize;
			break;
		case J9CUDA_DEVICE_LIMIT_PRINTF_FIFO_SIZE:
			deviceLimit = cudaLimitPrintfFifoSize;
			break;
		case J9CUDA_DEVICE_LIMIT_STACK_SIZE:
			deviceLimit = cudaLimitStackSize;
			break;
		}

		cudaError_t error = functions->DeviceSetLimit(deviceLimit, value);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Set a limit of a device. The set of device limits is defined by the
 * enum J9CudaDeviceLimit, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] limit the limit being queried
 * @param[in] value the requested value for the limit
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceSetLimit(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceLimit limit, uintptr_t value)
{
	Trc_PRT_cuda_deviceSetLimit_entry(deviceId, limit, value);

	const DeviceSetLimit operation(limit, value);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceSetLimit_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to set the shared memory configuration of a device in
 * conjunction with use of the template function 'withDevice'.
 *
 * @param[in] config the requested shared memory configuration
 */
struct DeviceSetSharedMemConfig : public InitializedAfter {
	J9CudaSharedMemConfig const config;

	explicit VMINLINE
	DeviceSetSharedMemConfig(J9CudaSharedMemConfig config)
		: config(config)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaSharedMemConfig deviceConfig = cudaSharedMemBankSizeDefault;

		switch (config) {
		default:
			return J9CUDA_ERROR_INVALID_VALUE;

		case J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE:
			deviceConfig = cudaSharedMemBankSizeDefault;
			break;
		case J9CUDA_SHARED_MEM_CONFIG_4_BYTE_BANK_SIZE:
			deviceConfig = cudaSharedMemBankSizeFourByte;
			break;
		case J9CUDA_SHARED_MEM_CONFIG_8_BYTE_BANK_SIZE:
			deviceConfig = cudaSharedMemBankSizeEightByte;
			break;
		}

		cudaError_t error = functions->DeviceSetSharedMemConfig(deviceConfig);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Set the shared memory configuration of a device. The set of shared memory
 * configurations is defined by the enum J9CudaSharedMemConfig, declared in
 * omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] config the requested shared memory configuration
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceSetSharedMemConfig(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaSharedMemConfig config)
{
	Trc_PRT_cuda_deviceSetSharedMemConfig_entry(deviceId, config);

	const DeviceSetSharedMemConfig operation(config);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceSetSharedMemConfig_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to synchronize with a device in conjunction with use
 * of the template function 'withDevice'.
 */
struct DeviceSynchronize : public InitializedAfter {
	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = functions->DeviceSynchronize();

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Synchronize with a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_deviceSynchronize(OMRPortLibrary *portLibrary, uint32_t deviceId)
{
	Trc_PRT_cuda_deviceSynchronize_entry(deviceId);

	DeviceSynchronize operation;

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_deviceSynchronize_exit(result);

	return result;
}

/**
 * Query the version number of the CUDA device driver. Version numbers
 * are represented as ((major * 1000) + (minor * 10)).
 *
 * @param[in] portLibrary the port library pointer
 * @param[out] versionOut where the driver version number should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_driverGetVersion(OMRPortLibrary *portLibrary, uint32_t *versionOut)
{
	Trc_PRT_cuda_driverGetVersion_entry();

	/* Ensure that initialization has been done. */
	getFunctions(portLibrary);

	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	if (0 != globals->deviceCount) {
		Trc_PRT_cuda_driverGetVersion_result(globals->driverVersion);

		*versionOut = globals->driverVersion;
		result = J9CUDA_NO_ERROR;
	}

	Trc_PRT_cuda_driverGetVersion_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to create an event with the spcified flags in
 * conjunction with use of the template function 'withDevice'.
 *
 * @param[in] flags the creation flags
 * @return a new event
 */
struct EventCreate : public InitializedAfter {
	cudaEvent_t event;
	uint32_t const flags;

	explicit VMINLINE
	EventCreate(uint32_t flags)
		: event(NULL)
		, flags(flags)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		const uint32_t allFlags = J9CUDA_EVENT_FLAG_BLOCKING_SYNC
								  | J9CUDA_EVENT_FLAG_DISABLE_TIMING
								  | J9CUDA_EVENT_FLAG_INTERPROCESS;

		J9CudaError result = J9CUDA_ERROR_INVALID_VALUE;

		if (OMR_ARE_NO_BITS_SET(flags, ~allFlags)) {
			unsigned int eventFlags = 0;

			if (OMR_ARE_ANY_BITS_SET(flags, J9CUDA_EVENT_FLAG_BLOCKING_SYNC)) {
				eventFlags |= cudaEventBlockingSync;
			}
			if (OMR_ARE_ANY_BITS_SET(flags, J9CUDA_EVENT_FLAG_DISABLE_TIMING)) {
				eventFlags |= cudaEventDisableTiming;
			}
			if (OMR_ARE_ANY_BITS_SET(flags, J9CUDA_EVENT_FLAG_INTERPROCESS)) {
				eventFlags |= cudaEventInterprocess;
			}

			cudaError_t error = functions->EventCreateWithFlags(&event, eventFlags);

			result = mapRuntime(portLibrary, error);
		}

		return result;
	}
};

} /* namespace */

/**
 * Create an event on the specified device. The value of the flags parameter
 * must be produced by ORing together one or more of the literals defined by
 * the enum J9CudaEventFlags, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] flags the event flags
 * @param[out] eventOut where the event handle should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_eventCreate(OMRPortLibrary *portLibrary, uint32_t deviceId, uint32_t flags, J9CudaEvent *eventOut)
{
	Trc_PRT_cuda_eventCreate_entry(deviceId, flags);

	EventCreate operation(flags);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_eventCreate_result(operation.event);

		*eventOut = (J9CudaEvent)operation.event;
	}

	Trc_PRT_cuda_eventCreate_exit(result);

	return result;
}

/**
 * Destroy an event.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] event the event handle
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_eventDestroy(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaEvent event)
{
	Trc_PRT_cuda_eventDestroy_entry(deviceId, event);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->EventDestroy)) {
		cudaError_t error = functions->EventDestroy((cudaEvent_t)event);

		if (cudaSuccess != error) {
			result = mapRuntime(portLibrary, error);
		} else {
			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
		}
	}

	Trc_PRT_cuda_eventDestroy_exit(result);

	return result;
}

/**
 * Query the elapsed time in milliseconds between two events.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] startEvent the earlier event handle
 * @param[in] endEvent the reference event handle
 * @param[out] elapsedMillisOut where the elapsed time in milliseconds
 * between the two events should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_eventElapsedTime(OMRPortLibrary *portLibrary, J9CudaEvent startEvent, J9CudaEvent endEvent, float *elapsedMillisOut)
{
	Trc_PRT_cuda_eventElapsedTime_entry(startEvent, endEvent);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	if (NULL != functions->EventElapsedTime) {
		cudaError_t error = functions->EventElapsedTime(elapsedMillisOut, (cudaEvent_t)startEvent, (cudaEvent_t)endEvent);

		result = mapRuntime(portLibrary, error);

		if (J9CUDA_NO_ERROR == result) {
			Trc_PRT_cuda_eventElapsedTime_result(*elapsedMillisOut);
		}
	}

	Trc_PRT_cuda_eventElapsedTime_exit(result);

	return result;
}

/**
 * Query the status of an event.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] event the event handle
 * @return 0 if the event has been recorded; else J9CUDA_ERROR_NOT_READY or another error
 */
extern "C" int32_t
omrcuda_eventQuery(OMRPortLibrary *portLibrary, J9CudaEvent event)
{
	Trc_PRT_cuda_eventQuery_entry(event);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	if (NULL != functions->EventQuery) {
		cudaError_t error = functions->EventQuery((cudaEvent_t)event);

		result = mapRuntime(portLibrary, error);
	}

	Trc_PRT_cuda_eventQuery_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to record an event on the spcified stream in
 * conjunction with use of the template function 'withDevice'.
 *
 * @param[in] stream the stream, or NULL for the default stream
 * @param[in] event the event to be recorded
 */
struct EventRecord : public InitializedAfter {
	cudaEvent_t const event;
	cudaStream_t const stream;

	VMINLINE
	EventRecord(cudaStream_t stream, cudaEvent_t event)
		: event(event)
		, stream(stream)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = functions->EventRecord(event, stream);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Record an event.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] event the event handle
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_eventRecord(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaEvent event, J9CudaStream stream)
{
	Trc_PRT_cuda_eventRecord_entry(deviceId, event, stream);

	EventRecord operation((cudaStream_t)stream, (cudaEvent_t)event);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_eventRecord_exit(result);

	return result;
}

/**
 * Synchronize with an event.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] event the event handle
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_eventSynchronize(OMRPortLibrary *portLibrary, J9CudaEvent event)
{
	Trc_PRT_cuda_eventSynchronize_entry(event);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	if (NULL != functions->EventSynchronize) {
		cudaError_t error = functions->EventSynchronize((cudaEvent_t)event);

		result = mapRuntime(portLibrary, error);
	}

	Trc_PRT_cuda_eventSynchronize_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to query a function attribute in conjunction with use
 * of the template function 'withDevice'.
 *
 * @param[in] function the function pointer
 * @param[in] attribute the attribute to query
 * @return the value of the requested attribute
 */
struct FunctionGetAttribute : public InitializedBefore {
	J9CudaFunctionAttribute const attribute;
	CUfunction const function;
	int value;

	VMINLINE
	FunctionGetAttribute(J9CudaFunctionAttribute attribute, CUfunction function)
		: attribute(attribute)
		, function(function)
		, value(0)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		CUfunction_attribute functionAttribute = CU_FUNC_ATTRIBUTE_BINARY_VERSION;

		switch (attribute) {
		default:
			return J9CUDA_ERROR_INVALID_VALUE;

		case J9CUDA_FUNCTION_ATTRIBUTE_BINARY_VERSION:
			functionAttribute = CU_FUNC_ATTRIBUTE_BINARY_VERSION;
			break;
		case J9CUDA_FUNCTION_ATTRIBUTE_CONST_SIZE_BYTES:
			functionAttribute = CU_FUNC_ATTRIBUTE_CONST_SIZE_BYTES;
			break;
		case J9CUDA_FUNCTION_ATTRIBUTE_LOCAL_SIZE_BYTES:
			functionAttribute = CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES;
			break;
		case J9CUDA_FUNCTION_ATTRIBUTE_MAX_THREADS_PER_BLOCK:
			functionAttribute = CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK;
			break;
		case J9CUDA_FUNCTION_ATTRIBUTE_NUM_REGS:
			functionAttribute = CU_FUNC_ATTRIBUTE_NUM_REGS;
			break;
		case J9CUDA_FUNCTION_ATTRIBUTE_PTX_VERSION:
			functionAttribute = CU_FUNC_ATTRIBUTE_PTX_VERSION;
			break;
		case J9CUDA_FUNCTION_ATTRIBUTE_SHARED_SIZE_BYTES:
			functionAttribute = CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES;
			break;
		}

		CUresult error = functions->FuncGetAttribute(&value, functionAttribute, function);

		return mapDriver(error);
	}
};

} /* namespace */

/**
 * Query a function attribute. The set of function attributes is defined
 * by the enum J9CudaFunctionAttribute, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] function the function handle
 * @param[in] attribute the requested function attribute
 * @param[out] valueOut where the value of the attribute should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_funcGetAttribute(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaFunctionAttribute attribute, int32_t *valueOut)
{
	Trc_PRT_cuda_funcGetAttribute_entry(deviceId, function, attribute);

	FunctionGetAttribute operation(attribute, (CUfunction)function);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_funcGetAttribute_result(operation.value);

		*valueOut = operation.value;
	}

	Trc_PRT_cuda_funcGetAttribute_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to query the maximum number of active thread blocks
 * per streaming multiprocessor for a kernel function in conjunction with use
 * of the template function 'withDevice'.
 *
 * @param[in] function the function pointer
 * @param[in] blockSize the intended thread block size
 * @param[in] dynamicSharedMemorySize the intended dynamic shared memory usage in bytes
 * @param[in] flags the query flags (see J9CudaOccupancyFlags)
 * @return numBlocks the maximum number of active thread blocks per streaming multiprocessor
 */
struct FunctionMaxActiveBlocks : public InitializedBefore {
	uint32_t const blockSize;
	uint32_t const dynamicSharedMemorySize;
	uint32_t const flags;
	CUfunction const function;
	int numBlocks;

	VMINLINE
	FunctionMaxActiveBlocks(
			CUfunction function,
			uint32_t blockSize,
			uint32_t dynamicSharedMemorySize,
			uint32_t flags)
		: blockSize(blockSize)
		, dynamicSharedMemorySize(dynamicSharedMemorySize)
		, flags(flags)
		, function(function)
		, numBlocks(0)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		CUresult error = functions->OccupancyMaxActiveBlocksPerMultiprocessorWithFlags(
				&numBlocks,
				function,
				(int)blockSize,
				(size_t)dynamicSharedMemorySize,
				(int)flags);

		return mapDriver(error);
	}
};

} /* namespace */

/**
 * Query the maximum number of active thread blocks per streaming multiprocessor for
 * a kernel function. The set of flags is defined by the enum J9CudaOccupancyFlags,
 * declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] function the function handle
 * @param[in] blockSize the intended thread block size
 * @param[in] dynamicSharedMemorySize the intended dynamic shared memory usage in bytes
 * @param[in] flags the query flags
 * @param[out] valueOut where the maximum number of active thread blocks per streaming multiprocessor should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_funcMaxActiveBlocksPerMultiprocessor(
	OMRPortLibrary *portLibrary,
	uint32_t deviceId,
	J9CudaFunction function,
	uint32_t blockSize,
	uint32_t dynamicSharedMemorySize,
	uint32_t flags,
	uint32_t *valueOut)
{
	Trc_PRT_cuda_funcMaxActiveBlocksPerMultiprocessor_entry(
		deviceId,
		function,
		blockSize,
		dynamicSharedMemorySize,
		flags);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if (J9CUDA_NO_ERROR == result) {
		if (NULL == functions->OccupancyMaxActiveBlocksPerMultiprocessorWithFlags) {
			result = J9CUDA_ERROR_NOT_SUPPORTED;
		} else {
			FunctionMaxActiveBlocks operation((CUfunction)function, blockSize, dynamicSharedMemorySize, flags);

			result = withDevice(portLibrary, deviceId, operation);

			if (J9CUDA_NO_ERROR == result) {
				Trc_PRT_cuda_funcMaxActiveBlocksPerMultiprocessor_result((uint32_t)operation.numBlocks);
				*valueOut = (uint32_t)operation.numBlocks;
			}
		}
	}

	Trc_PRT_cuda_funcMaxActiveBlocksPerMultiprocessor_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to query reasonable block and grid sizes to achieve maximal
 * occupancy of a kernel function in conjunction with use of the template function
 * 'withDevice'.
 *
 * @param[in] function the function pointer
 * @param[in] dynamicSharedMemoryFunction a function which gives the dynamic shared
 * memory required for a particular block size
 * @param[in] userData the user data to be provided in calls to dynamicSharedMemoryFunction
 * @param[in] blockSizeLimit the maximum block size to consider;
 * 0 is equivalent to specifying the maximum block size permitted by the device / function
 * @param[in] flags the query flags
 * @param[in] maxThreadsPerMultiProcessor the maximum number of threads per multiprocessor
 * @param[in] warpSize the number of threads in a warp
 *
 * @return bestBlockSize the maximum block size that achieves maximal occupancy
 * @return bestBlockCountPerMultiProcessor the minimum number of blocks per multiprocessor
 * that achieves maximal occupancy
 */
struct FunctionPotentialBlockSize: public InitializedBefore {
	uint32_t bestBlockCountPerMultiProcessor;
	uint32_t bestBlockSize;
	uint32_t const blockSizeLimit;
	J9CudaBlockToDynamicSharedMemorySize const callback;
	uint32_t const flags;
	CUfunction const function;
	uint32_t const maxThreadsPerMultiProcessor;
	uintptr_t const userData;
	uint32_t const warpSize;

	VMINLINE
	FunctionPotentialBlockSize(
			CUfunction function,
			J9CudaBlockToDynamicSharedMemorySize dynamicSharedMemoryFunction,
			uintptr_t userData,
			uint32_t blockSizeLimit,
			uint32_t flags,
			uint32_t maxThreadsPerMultiProcessor,
			uint32_t warpSize)
		: bestBlockCountPerMultiProcessor(0)
		, bestBlockSize(0)
		, blockSizeLimit(blockSizeLimit)
		, callback(dynamicSharedMemoryFunction)
		, flags(flags)
		, function(function)
		, maxThreadsPerMultiProcessor(maxThreadsPerMultiProcessor)
		, userData(userData)
		, warpSize(warpSize)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		CUresult result = CUDA_SUCCESS;
		uint32_t bestOccupancy = 0;
		uint32_t blockStep = blockSizeLimit % warpSize;
		uintptr_t dynamicSharedMemorySize = userData;

		if (0 == blockStep) {
			blockStep = warpSize;
		}

		for (uint32_t blockSize = blockSizeLimit; blockSize != 0;) {
			if (NULL != callback) {
				dynamicSharedMemorySize = callback(blockSize, userData);
			}

			int blocksPerMultiProcessor = 0;

			result = functions->OccupancyMaxActiveBlocksPerMultiprocessorWithFlags(
						 &blocksPerMultiProcessor,
						 function,
						 (int)blockSize,
						 (size_t)dynamicSharedMemorySize,
						 (int)flags);

			if (CUDA_SUCCESS != result) {
				break;
			}

			uint32_t threadsPerMultiProcessor = blockSize * blocksPerMultiProcessor;

			if (bestOccupancy < threadsPerMultiProcessor) {
				bestBlockCountPerMultiProcessor = blocksPerMultiProcessor;
				bestBlockSize = blockSize;
				bestOccupancy = threadsPerMultiProcessor;

				/* quit if multiprocessors would be fully utilized */
				if (bestOccupancy >= maxThreadsPerMultiProcessor) {
					break;
				}
			}

			blockSize -= blockStep;
			blockStep = warpSize;
		}

		return mapDriver(result);
	}
};

} /* namespace */

/**
 * Query reasonable block and grid sizes to achieve maximal occupancy of a kernel
 * function.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] function the function handle
 * @param[in] dynamicSharedMemoryFunction a function which gives the dynamic shared
 * memory required for a particular block size; NULL may be used if the requirements
 * don't depend upon the block size, in which case userData will be used to define
 * the dynamic shared for all block sizes
 * @param[in] userData the user data to be provided in calls to dynamicSharedMemoryFunction,
 * or the fixed dynamic shared memory requirement
 * @param[in] blockSizeLimit the maximum block size that should be considered;
 * 0 is equivalent to specifying the maximum block size permitted by the device / function
 * @param[in] flags the query flags
 * @param[out] minGridSizeOut where the minimum computed grid size should be stored
 * @param[out] maxBlockSizeOut where the maximum computed block size should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_funcMaxPotentialBlockSize(
	OMRPortLibrary *portLibrary,
	uint32_t deviceId,
	J9CudaFunction function,
	J9CudaBlockToDynamicSharedMemorySize dynamicSharedMemoryFunction,
	uintptr_t userData,
	uint32_t blockSizeLimit,
	uint32_t flags,
	uint32_t *minGridSizeOut,
	uint32_t *maxBlockSizeOut)
{
	Trc_PRT_cuda_funcMaxPotentialBlockSize_entry(
		deviceId,
		function,
		dynamicSharedMemoryFunction,
		userData,
		blockSizeLimit,
		flags);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if (J9CUDA_NO_ERROR == result) {
		/* device & function attributes */
		uint32_t deviceMaxThreadsPerBlock = 0;
		uint32_t deviceMaxThreadsPerMultiProcessor = 0;
		uint32_t deviceMultiProcessorCount = 0;
		uint32_t deviceWarpSize = 0;
		uint32_t functionMaxThreadsPerBlock = 0;

		if (NULL == functions->OccupancyMaxActiveBlocksPerMultiprocessorWithFlags) {
			result = J9CUDA_ERROR_NOT_SUPPORTED;
			goto done;
		}

		/* these are implied by the presence of the occupancy API */
		Assert_PRT_true(NULL != functions->DeviceGetAttribute);
		Assert_PRT_true(NULL != functions->FuncGetAttribute);

#define GET_DEVICE_ATTRIBUTE(attribute) \
		do { \
			int value = 0; \
			cudaError_t error = functions->DeviceGetAttribute(&value, (cudaDevAttr ## attribute), (int)deviceId); \
			if (cudaSuccess != error) { \
				result = mapRuntime(portLibrary, error); \
				goto done; \
			} \
			device ## attribute = (uint32_t)value; \
		} while (0)

		GET_DEVICE_ATTRIBUTE(MaxThreadsPerBlock);
		GET_DEVICE_ATTRIBUTE(MaxThreadsPerMultiProcessor);
		GET_DEVICE_ATTRIBUTE(MultiProcessorCount);
		GET_DEVICE_ATTRIBUTE(WarpSize);

#undef GET_DEVICE_ATTRIBUTE

		if (0 == deviceWarpSize) {
			result = J9CUDA_ERROR_INVALID_VALUE;
			goto done;
		}

		int value = 0;
		CUresult error = functions->FuncGetAttribute(
				&value,
				CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK,
				(CUfunction)function);

		if (CUDA_SUCCESS != error) {
			result = mapDriver(error);
			goto done;
		}

		functionMaxThreadsPerBlock = (uint32_t)value;

		if ((blockSizeLimit > deviceMaxThreadsPerBlock) || (0 == blockSizeLimit)) {
			blockSizeLimit = deviceMaxThreadsPerBlock;
		}

		if (blockSizeLimit > functionMaxThreadsPerBlock) {
			blockSizeLimit = functionMaxThreadsPerBlock;
		}

		FunctionPotentialBlockSize operation(
			(CUfunction)function,
			dynamicSharedMemoryFunction,
			userData,
			blockSizeLimit,
			flags,
			(uint32_t)deviceMaxThreadsPerMultiProcessor,
			(uint32_t)deviceWarpSize);

		result = withDevice(portLibrary, deviceId, operation);

		if (J9CUDA_NO_ERROR == result) {
			*minGridSizeOut = (uint32_t)operation.bestBlockCountPerMultiProcessor * deviceMultiProcessorCount;
			*maxBlockSizeOut = (uint32_t)operation.bestBlockSize;

			Trc_PRT_cuda_funcMaxPotentialBlockSize_result(*minGridSizeOut, *maxBlockSizeOut);
		}
	}

done:
	Trc_PRT_cuda_funcMaxPotentialBlockSize_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to set the cache configuration of a kernel function in
 * conjunction with use of the template function 'withDevice'.
 *
 * @param[in] function the function pointer
 * @param[in] config the requested cache configuration
 */
struct FunctionSetCacheConfig : public InitializedBefore {
	J9CudaCacheConfig const config;
	CUfunction const function;

	VMINLINE
	FunctionSetCacheConfig(CUfunction function, J9CudaCacheConfig config)
		: config(config)
		, function(function)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		CUfunc_cache deviceConfig = CU_FUNC_CACHE_PREFER_EQUAL;

		switch (config) {
		default:
			return J9CUDA_ERROR_INVALID_VALUE;

		case J9CUDA_CACHE_CONFIG_PREFER_EQUAL:
			deviceConfig = CU_FUNC_CACHE_PREFER_EQUAL;
			break;
		case J9CUDA_CACHE_CONFIG_PREFER_L1:
			deviceConfig = CU_FUNC_CACHE_PREFER_L1;
			break;
		case J9CUDA_CACHE_CONFIG_PREFER_NONE:
			deviceConfig = CU_FUNC_CACHE_PREFER_NONE;
			break;
		case J9CUDA_CACHE_CONFIG_PREFER_SHARED:
			deviceConfig = CU_FUNC_CACHE_PREFER_SHARED;
			break;
		}

		return mapDriver(functions->FuncSetCacheConfig(function, deviceConfig));
	}
};

} /* namespace */

/**
 * Set the cache configuration of a kernel function. The set of cache
 * configurations is defined by the enum J9CudaCacheConfig, declared
 * in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] function the function handle
 * @param[in] config the requested cache configuration
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_funcSetCacheConfig(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaCacheConfig config)
{
	Trc_PRT_cuda_funcSetCacheConfig_entry(deviceId, function, config);

	const FunctionSetCacheConfig operation((CUfunction)function, config);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_funcSetCacheConfig_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to set the shared memory configuration of a kernel
 * function in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] function the function pointer
 * @param[in] config the requested shared memory configuration
 */
struct FunctionSetSharedMemConfig : public InitializedBefore {
	J9CudaSharedMemConfig const config;
	CUfunction const function;

	VMINLINE
	FunctionSetSharedMemConfig(CUfunction function, J9CudaSharedMemConfig config)
		: config(config)
		, function(function)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		CUsharedconfig sharedConfig = CU_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;

		switch (config) {
		default:
			return J9CUDA_ERROR_INVALID_VALUE;

		case J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE:
			sharedConfig = CU_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
			break;
		case J9CUDA_SHARED_MEM_CONFIG_4_BYTE_BANK_SIZE:
			sharedConfig = CU_SHARED_MEM_CONFIG_FOUR_BYTE_BANK_SIZE;
			break;
		case J9CUDA_SHARED_MEM_CONFIG_8_BYTE_BANK_SIZE:
			sharedConfig = CU_SHARED_MEM_CONFIG_EIGHT_BYTE_BANK_SIZE;
			break;
		}

		return mapDriver(functions->FuncSetSharedMemConfig(function, sharedConfig));
	}
};

} /* namespace */

/**
 * Set the shared memory configuration of a kernel function. The set of shared
 * memory configurations is defined by the enum J9CudaSharedMemConfig, declared
 * in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] function the function handle
 * @param[in] config the requested shared memory configuration
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_funcSetSharedMemConfig(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function, J9CudaSharedMemConfig config)
{
	Trc_PRT_cuda_funcSetSharedMemConfig_entry(deviceId, function, config);

	const FunctionSetSharedMemConfig operation((CUfunction)function, config);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_funcSetSharedMemConfig_exit(result);

	return result;
}

/**
 * Return a string describing the given error code.
 *
 * Note that the strings here are not localized because we make use of the
 * driver API function
 * CUresult cuGetErrorString(CUresult, const char **);
 * when it is available (starting with CUDA 6.0).
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] error the error code
 * @return a non-NULL pointer on success, NULL for unknown error codes
 */
extern "C" const char *
omrcuda_getErrorString(OMRPortLibrary *portLibrary, int32_t error)
{
	Trc_PRT_cuda_getErrorString_entry(error);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	const char *result = NULL;

	if (error >= 0) {
		/* a runtime error */
		if (NULL != functions->GetErrorString) {
			cudaError_t cudaError = unmapRuntime(portLibrary, (J9CudaError)error);

			result = functions->GetErrorString(cudaError);
		}

		if (NULL == result) {
			/*
			 * Provide answers for the error codes we return if we
			 * were unable to load the required shared libraries.
			 */
			if (J9CUDA_NO_ERROR == error) {
				result = "no error";
			} else if (J9CUDA_ERROR_NO_DEVICE == error) {
				result = "no CUDA-capable device is detected";
			}
		}
	} else {
		/* a driver error */
		if ((NULL == functions->GetDriverErrorString)
			|| (CUDA_SUCCESS != functions->GetDriverErrorString((CUresult)-error, &result))) {
			switch (-error) {
			case CUDA_ERROR_ALREADY_ACQUIRED:
				result = "CUDA_ERROR_ALREADY_ACQUIRED";
				break;
			case CUDA_ERROR_ALREADY_MAPPED:
				result = "CUDA_ERROR_ALREADY_MAPPED";
				break;
			case CUDA_ERROR_ARRAY_IS_MAPPED:
				result = "CUDA_ERROR_ARRAY_IS_MAPPED";
				break;
			case CUDA_ERROR_CONTEXT_ALREADY_IN_USE:
				result = "CUDA_ERROR_CONTEXT_ALREADY_IN_USE";
				break;
			case CUDA_ERROR_CONTEXT_IS_DESTROYED:
				result = "CUDA_ERROR_CONTEXT_IS_DESTROYED";
				break;
			case CUDA_ERROR_DEINITIALIZED:
				result = "CUDA_ERROR_DEINITIALIZED";
				break;
			case CUDA_ERROR_FILE_NOT_FOUND:
				result = "CUDA_ERROR_FILE_NOT_FOUND";
				break;
			case CUDA_ERROR_INVALID_CONTEXT:
				result = "CUDA_ERROR_INVALID_CONTEXT";
				break;
			case CUDA_ERROR_INVALID_SOURCE:
				result = "CUDA_ERROR_INVALID_SOURCE";
				break;
			case CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING:
				result = "CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING";
				break;
			case CUDA_ERROR_MAP_FAILED:
				result = "CUDA_ERROR_MAP_FAILED";
				break;
			case CUDA_ERROR_NOT_FOUND:
				result = "named symbol not found";
				break;
			case CUDA_ERROR_NOT_INITIALIZED:
				result = "CUDA_ERROR_NOT_INITIALIZED";
				break;
			case CUDA_ERROR_NOT_MAPPED_AS_ARRAY:
				result = "CUDA_ERROR_NOT_MAPPED_AS_ARRAY";
				break;
			case CUDA_ERROR_NOT_MAPPED_AS_POINTER:
				result = "CUDA_ERROR_NOT_MAPPED_AS_POINTER";
				break;
			case CUDA_ERROR_NOT_MAPPED:
				result = "CUDA_ERROR_NOT_MAPPED";
				break;
			case CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE:
				result = "CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE";
				break;
			default:
				break;
			}
		}
	}

	Trc_PRT_cuda_getErrorString_exit(result);

	return result;
}

/**
 * Allocate host storage to be used for transfers between host and device memory.
 * The value of the flags parameter must be produced by ORing together one or more
 * of the literals defined by the enum J9CudaHostAllocFlags, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] size the number of bytes requested
 * @param[in] flags allocation flags
 * @param[out] hostAddressOut where the host address should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_hostAlloc(OMRPortLibrary *portLibrary, uintptr_t size, uint32_t flags, void **hostAddressOut)
{
	Trc_PRT_cuda_hostAlloc_entry(size, flags);

	const uint32_t allFlags = J9CUDA_HOST_ALLOC_MAPPED
							  | J9CUDA_HOST_ALLOC_PORTABLE
							  | J9CUDA_HOST_ALLOC_WRITE_COMBINED;

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	if (NULL != functions->HostAlloc) {
		if (OMR_ARE_ANY_BITS_SET(flags, ~allFlags)) {
			result = J9CUDA_ERROR_INVALID_VALUE;
		} else {
			unsigned int cudaFlags = cudaHostAllocDefault;

			if (OMR_ARE_ANY_BITS_SET(flags, J9CUDA_HOST_ALLOC_MAPPED)) {
				cudaFlags |= cudaHostAllocMapped;
			}
			if (OMR_ARE_ANY_BITS_SET(flags, J9CUDA_HOST_ALLOC_PORTABLE)) {
				cudaFlags |= cudaHostAllocPortable;
			}
			if (OMR_ARE_ANY_BITS_SET(flags, J9CUDA_HOST_ALLOC_WRITE_COMBINED)) {
				cudaFlags |= cudaHostAllocWriteCombined;
			}

			cudaError_t error = functions->HostAlloc(hostAddressOut, size, cudaFlags);

			result = mapRuntime(portLibrary, error);

			if (J9CUDA_NO_ERROR == result) {
				Trc_PRT_cuda_hostAlloc_result(*hostAddressOut);
			}
		}
	}

	Trc_PRT_cuda_hostAlloc_exit(result);

	return result;
}

/**
 * Release host storage (allocated via omrcuda_hostAlloc).
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] hostAddress the host address
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_hostFree(OMRPortLibrary *portLibrary, void *hostAddress)
{
	Trc_PRT_cuda_hostFree_entry(hostAddress);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	if (NULL != functions->FreeHost) {
		cudaError_t error = functions->FreeHost(hostAddress);

		result = mapRuntime(portLibrary, error);
	}

	Trc_PRT_cuda_hostFree_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to launch a grid of threads executing a kernel function
 * in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] function the function pointer
 * @param[in] grdDimX the grid size in the X dimension
 * @param[in] grdDimY the grid size in the Y dimension
 * @param[in] grdDimZ the grid size in the Z dimension
 * @param[in] blkDimX the block size in the X dimension
 * @param[in] blkDimY the block size in the Y dimension
 * @param[in] blkDimZ the block size in the Z dimension
 * @param[in] sharedMem the number of bytes of dynamic shared memory required
 * @param[in] stream the stream, or NULL for the default stream
 * @param[in] args the kernel arguments
 */
struct Launch : public InitializedBefore {
	CUfunction const function;
	dim3 const gridDim;
	dim3 const blockDim;
	unsigned int const sharedMem;
	CUstream const stream;
	void **const args;

	VMINLINE
	Launch(CUfunction function, const dim3 &gridDim, const dim3 &blockDim, uint32_t sharedMem, CUstream stream, void **args)
		: function(function)
		, gridDim(gridDim)
		, blockDim(blockDim)
		, sharedMem(sharedMem)
		, stream(stream)
		, args(args)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		return mapDriver(
				functions->LaunchKernel(
						function,
						gridDim.x,
						gridDim.y,
						gridDim.z,
						blockDim.x,
						blockDim.y,
						blockDim.z,
						sharedMem,
						stream,
						args,
						NULL));
	}
};

} /* namespace */

/**
 * Launch a grid of threads executing a kernel function.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] function the function handle
 * @param[in] gridDimX the grid size in the X dimension
 * @param[in] gridDimY the grid size in the Y dimension
 * @param[in] gridDimZ the grid size in the Z dimension
 * @param[in] blockDimX the block size in the X dimension
 * @param[in] blockDimY the block size in the Y dimension
 * @param[in] blockDimZ the block size in the Z dimension
 * @param[in] sharedMemBytes the number of bytes of dynamic shared memory required
 * @param[in] stream the stream handle, or NULL for the default stream
 * @param[in] kernelParms the kernel arguments
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_launchKernel(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaFunction function,
					uint32_t gridDimX, uint32_t gridDimY, uint32_t gridDimZ,
					uint32_t blockDimX, uint32_t blockDimY, uint32_t blockDimZ,
					uint32_t sharedMemBytes, J9CudaStream stream, void **kernelParms)
{
	Trc_PRT_cuda_launchKernel_entry(
		deviceId,
		function,
		gridDimX, gridDimY, gridDimZ,
		blockDimX, blockDimY, blockDimZ,
		sharedMemBytes,
		stream,
		kernelParms);

	const dim3 gridDim(gridDimX, gridDimY, gridDimZ);
	const dim3 blockDim(blockDimX, blockDimY, blockDimZ);

	const Launch operation(
		(CUfunction)function,
		gridDim,
		blockDim,
		sharedMemBytes,
		(CUstream)stream,
		kernelParms);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_launchKernel_exit(result);

	return result;
}

namespace
{

/**
 * A container for CUDA driver JIT options.
 */
class JitOptions
{
private:
	/** number of keys/values present */
	uint32_t count;
	/** driver option keys */
	CUjit_option keys[CU_JIT_NUM_OPTIONS];
	/** driver option values */
	void *values[CU_JIT_NUM_OPTIONS];

public:
	VMINLINE
	JitOptions()
		: count(0)
	{
	}

	J9CudaError
	set(J9CudaJitOptions *j9Ooptions);

	VMINLINE uint32_t
	getCount() const
	{
		return count;
	}

	VMINLINE CUjit_option *
	getKeys()
	{
		return keys;
	}

	VMINLINE void **
	getValues()
	{
		return values;
	}
};

/**
 * JitOptions initializer: repackage information from the given
 * J9CudaJitOptions object as required by CUDA device driver API.
 *
 * @param[in] j9options the input options pointer
 */
J9CudaError
JitOptions::set(J9CudaJitOptions *j9options)
{
	Trc_PRT_cuda_JitOptions_entry(j9options);

	count = 0;

	if (NULL != j9options) {
		uint32_t index = 0;

		if (0 != j9options->maxRegisters) {
			keys[index] = CU_JIT_MAX_REGISTERS;
			values[index] = (void *)(uintptr_t)j9options->maxRegisters;
			index += 1;
		}

		if (0 != j9options->threadsPerBlock) {
			keys[index] = CU_JIT_THREADS_PER_BLOCK;
			values[index] = &j9options->threadsPerBlock;
			index += 1;
		}

		switch (j9options->recordWallTime) {
		default:
			goto invalid;

		case J9CUDA_JIT_FLAG_ENABLED:
			keys[index] = CU_JIT_WALL_TIME;
			values[index] = &j9options->wallTime;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_UNSPECIFIED:
		case J9CUDA_JIT_FLAG_DISABLED:
			break;
		}

		if (0 != j9options->infoLogBufferSize) {
			if (NULL == j9options->infoLogBuffer) {
				goto invalid;
			}

			keys[index] = CU_JIT_INFO_LOG_BUFFER;
			values[index] = j9options->infoLogBuffer;
			index += 1;

			keys[index] = CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES;
			values[index] = (void *)j9options->infoLogBufferSize;
			index += 1;
		}

		if (0 != j9options->errorLogBufferSize) {
			if (NULL == j9options->errorLogBuffer) {
				goto invalid;
			}

			keys[index] = CU_JIT_ERROR_LOG_BUFFER;
			values[index] = j9options->errorLogBuffer;
			index += 1;

			keys[index] = CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES;
			values[index] = (void *)j9options->errorLogBufferSize;
			index += 1;
		}

		switch (j9options->optimizationLevel) {
		default:
			goto invalid;

		case J9CUDA_JIT_OPTIMIZATION_LEVEL_0:
			keys[index] = CU_JIT_OPTIMIZATION_LEVEL;
			values[index] = (void *)0;
			index += 1;
			break;

		case J9CUDA_JIT_OPTIMIZATION_LEVEL_1:
			keys[index] = CU_JIT_OPTIMIZATION_LEVEL;
			values[index] = (void *)1;
			index += 1;
			break;

		case J9CUDA_JIT_OPTIMIZATION_LEVEL_2:
			keys[index] = CU_JIT_OPTIMIZATION_LEVEL;
			values[index] = (void *)2;
			index += 1;
			break;

		case J9CUDA_JIT_OPTIMIZATION_LEVEL_3:
			keys[index] = CU_JIT_OPTIMIZATION_LEVEL;
			values[index] = (void *)3;
			index += 1;
			break;

		case J9CUDA_JIT_OPTIMIZATION_LEVEL_4:
			keys[index] = CU_JIT_OPTIMIZATION_LEVEL;
			values[index] = (void *)4;
			index += 1;
			break;

		case J9CUDA_JIT_OPTIMIZATION_LEVEL_UNSPECIFIED:
			break;
		}

		switch (j9options->targetFromContext) {
		default:
			goto invalid;

		case J9CUDA_JIT_FLAG_ENABLED:
			keys[index] = CU_JIT_TARGET_FROM_CUCONTEXT;
			values[index] = (void *)1;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_UNSPECIFIED:
		case J9CUDA_JIT_FLAG_DISABLED:
			break;
		}

		if (0 != j9options->target) {
			uint32_t target = j9options->target;

			/* In CUDA 6.0 and later, values of CU_TARGET_* don't need translation. */
#if CUDA_VERSION < 6000
			switch (target) {
			default:
				goto invalid;
			case 10:
				target = CU_TARGET_COMPUTE_10;
				break;
			case 11:
				target = CU_TARGET_COMPUTE_11;
				break;
			case 12:
				target = CU_TARGET_COMPUTE_12;
				break;
			case 13:
				target = CU_TARGET_COMPUTE_13;
				break;
			case 20:
				target = CU_TARGET_COMPUTE_20;
				break;
			case 21:
				target = CU_TARGET_COMPUTE_21;
				break;
			case 30:
				target = CU_TARGET_COMPUTE_30;
				break;
			case 35:
				target = CU_TARGET_COMPUTE_35;
				break;
			}
#endif /* CUDA_VERSION < 6000 */

			keys[index] = CU_JIT_TARGET;
			values[index] = (void *)(uintptr_t)target;
			index += 1;
		}

		switch (j9options->fallbackStrategy) {
		default:
			goto invalid;

		case J9CUDA_JIT_FALLBACK_STRATEGY_PREFER_BINARY:
			keys[index] = CU_JIT_FALLBACK_STRATEGY;
			values[index] = (void *)CU_PREFER_BINARY;
			index += 1;
			break;

		case J9CUDA_JIT_FALLBACK_STRATEGY_PREFER_PTX:
			keys[index] = CU_JIT_FALLBACK_STRATEGY;
			values[index] = (void *)CU_PREFER_PTX;
			index += 1;
			break;

		case J9CUDA_JIT_FALLBACK_STRATEGY_UNSPECIFIED:
			break;
		}

		switch (j9options->generateDebugInfo) {
		default:
			goto invalid;

		case J9CUDA_JIT_FLAG_DISABLED:
			keys[index] = CU_JIT_GENERATE_DEBUG_INFO;
			values[index] = (void *)0;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_ENABLED:
			keys[index] = CU_JIT_GENERATE_DEBUG_INFO;
			values[index] = (void *)1;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_UNSPECIFIED:
			break;
		}

		switch (j9options->verboseLogging) {
		default:
			goto invalid;

		case J9CUDA_JIT_FLAG_DISABLED:
			keys[index] = CU_JIT_LOG_VERBOSE;
			values[index] = (void *)0;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_ENABLED:
			keys[index] = CU_JIT_LOG_VERBOSE;
			values[index] = (void *)1;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_UNSPECIFIED:
			break;
		}

		switch (j9options->generateLineInfo) {
		default:
			goto invalid;

		case J9CUDA_JIT_FLAG_DISABLED:
			keys[index] = CU_JIT_GENERATE_LINE_INFO;
			values[index] = (void *)0;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_ENABLED:
			keys[index] = CU_JIT_GENERATE_LINE_INFO;
			values[index] = (void *)1;
			index += 1;
			break;

		case J9CUDA_JIT_FLAG_UNSPECIFIED:
			break;
		}

		switch (j9options->cacheMode) {
		default:
			goto invalid;

		case J9CUDA_JIT_CACHE_MODE_DEFAULT:
			keys[index] = CU_JIT_CACHE_MODE;
			values[index] = (void *)CU_JIT_CACHE_OPTION_NONE;
			index += 1;
			break;

		case J9CUDA_JIT_CACHE_MODE_L1_DISABLED:
			keys[index] = CU_JIT_CACHE_MODE;
			values[index] = (void *)CU_JIT_CACHE_OPTION_CG;
			index += 1;
			break;

		case J9CUDA_JIT_CACHE_MODE_L1_ENABLED:
			keys[index] = CU_JIT_CACHE_MODE;
			values[index] = (void *)CU_JIT_CACHE_OPTION_CA;
			index += 1;
			break;

		case J9CUDA_JIT_CACHE_MODE_UNSPECIFIED:
			break;
		}

		count = index;
	}

	Trc_PRT_cuda_JitOptions_exit(count);

	return J9CUDA_NO_ERROR;

invalid:
	Trc_PRT_cuda_JitOptions_exit_invalid();

	return J9CUDA_ERROR_INVALID_VALUE;
}

} /* namespace */

/**
 * Options specified when a linker is created must have a lifetime at least as
 * long as the linker itself. This structure bundles the them together so they
 * have the same lifetime.
 */
struct J9CudaLinkerState {
	JitOptions createOptions;
	CUlinkState state;

	VMINLINE
	J9CudaLinkerState()
		: createOptions()
		, state(NULL)
	{
	}
};

namespace
{

/**
 * The closure required to add a module fragment to the set of inputs to a
 * linker in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] linker the linker state pointer
 * @param[in] type the type of input in image
 * @param[in] image the input image
 * @param[in] imageSize the size of the input image in bytes
 * @param[in] name an optional name for use in log messages
 * @param[in] options an optional options pointer
 */
struct LinkerAdd : public InitializedBefore {
	void *const image;
	size_t const imageSize;
	J9CudaLinker const linker;
	const char *const name;
	J9CudaJitOptions *const options;
	J9CudaJitInputType const type;

	VMINLINE
	LinkerAdd(J9CudaLinker linker, J9CudaJitInputType type, void *image, size_t imageSize, const char *name, J9CudaJitOptions *options)
		: image(image)
		, imageSize(imageSize)
		, linker(linker)
		, name(name)
		, options(options)
		, type(type)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		JitOptions addOptions;

		J9CudaError result = addOptions.set(options);

		if (J9CUDA_NO_ERROR == result) {
			CUjitInputType inputType = CU_JIT_INPUT_FATBINARY;

			switch (type) {
			default:
				result = J9CUDA_ERROR_INVALID_VALUE;
				goto done;

			case J9CUDA_JIT_INPUT_TYPE_CUBIN:
				inputType = CU_JIT_INPUT_CUBIN;
				break;
			case J9CUDA_JIT_INPUT_TYPE_FATBINARY:
				inputType = CU_JIT_INPUT_FATBINARY;
				break;
			case J9CUDA_JIT_INPUT_TYPE_LIBRARY:
				inputType = CU_JIT_INPUT_LIBRARY;
				break;
			case J9CUDA_JIT_INPUT_TYPE_OBJECT:
				inputType = CU_JIT_INPUT_OBJECT;
				break;
			case J9CUDA_JIT_INPUT_TYPE_PTX:
				inputType = CU_JIT_INPUT_PTX;
				break;
			}

			result = mapDriver(
					functions->LinkAddData(
							linker->state,
							inputType,
							image,
							imageSize,
							name,
							addOptions.getCount(),
							addOptions.getKeys(),
							addOptions.getValues()));
		}

done:
		return result;
	}
};

} /* namespace */

/**
 * Add a module fragment to the set of linker inputs.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] linker the linker handle
 * @param[in] type the type of input in data
 * @param[in] data the input content
 * @param[in] size the number of input bytes
 * @param[in] name an optional name for use in log messages
 * @param[in] options an optional options pointer
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_linkerAddData(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker,
	J9CudaJitInputType type, void *data, uintptr_t size, const char *name, J9CudaJitOptions *options)
{
	Trc_PRT_cuda_linkerAddData_entry(deviceId, linker, type, data, size, name, options);

	const LinkerAdd operation(linker, type, data, size, name, options);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_linkerAddData_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to complete linking a module in conjunction with use
 * of the template function 'withDevice'.
 *
 * @param[in] linker the linker state pointer
 * @return data the completed module ready for loading
 * @return size the number of bytes comp
 */
struct LinkerComplete : public InitializedBefore {
	void *data;
	J9CudaLinker const linker;
	size_t size;

	explicit VMINLINE
	LinkerComplete(J9CudaLinker linker)
		: data(NULL)
		, linker(linker)
		, size(0)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		return mapDriver(functions->LinkComplete(linker->state, &data, &size));
	}
};

} /* namespace */

/**
 * Complete linking a module. Note that the image pointer returned refers to
 * storage owned by the linker object which will be invalidated by a call to
 * omrcuda_linkerDestroy.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] linker the linker handle
 * @param[out] cubinOut where the pointer to the module image should be stored
 * @param[out] sizeOut where the size (in bytes) of the module image should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_linkerComplete(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker, void **cubinOut, uintptr_t *sizeOut)
{
	Trc_PRT_cuda_linkerComplete_entry(deviceId, linker);

	LinkerComplete operation(linker);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_linkerComplete_result(operation.data, operation.size);

		*cubinOut = operation.data;
		*sizeOut = operation.size;
	}

	Trc_PRT_cuda_linkerComplete_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to create a linker session
 * in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] j9options an optional options pointer
 * @return the linker state pointer
 */
struct LinkerCreate : public InitializedBefore {
	J9CudaLinkerState *const linker;

	explicit VMINLINE
	LinkerCreate(J9CudaLinkerState *linker)
		: linker(linker)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		return mapDriver(
				functions->LinkCreate(
						linker->createOptions.getCount(),
						linker->createOptions.getKeys(),
						linker->createOptions.getValues(),
						&linker->state));
	}
};

} /* namespace */

/**
 * Create a new linker session.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] options an optional options pointer
 * @param[out] linkerOut where the linker handle should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_linkerCreate(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaJitOptions *options, J9CudaLinker *linkerOut)
{
	Trc_PRT_cuda_linkerCreate_entry(deviceId, options);

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	J9CudaLinker linker = (J9CudaLinker)J9CUDA_ALLOCATE_MEMORY(sizeof(J9CudaLinkerState));
	J9CudaError result = J9CUDA_ERROR_MEMORY_ALLOCATION;

	if (NULL == linker) {
		Trc_PRT_cuda_linkerCreate_nomem();
	} else {
		result = linker->createOptions.set(options);

		if (J9CUDA_NO_ERROR == result) {
			const LinkerCreate operation(linker);

			linker->state = NULL;
			result = withDevice(portLibrary, deviceId, operation);
		}

		if (J9CUDA_NO_ERROR != result) {
			J9CUDA_FREE_MEMORY(linker);
		} else {
			Trc_PRT_cuda_linkerCreate_result(linker);

			*linkerOut = linker;
		}
	}

	Trc_PRT_cuda_linkerCreate_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to destroy a linker session in conjunction with use
 * of the template function 'withDevice'.
 *
 * @param[in] linker the linker state pointer
 */
struct LinkerDestroy : public InitializedBefore {
	J9CudaLinker const linker;

	explicit VMINLINE
	LinkerDestroy(J9CudaLinker linker)
		: linker(linker)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		return mapDriver(functions->LinkDestroy(linker->state));
	}
};

} /* namespace */

/**
 * Destroy a linker session.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] linker the linker handle
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_linkerDestroy(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaLinker linker)
{
	Trc_PRT_cuda_linkerDestroy_entry(deviceId, linker);

	const LinkerDestroy operation(linker);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	J9CUDA_FREE_MEMORY(linker);

	Trc_PRT_cuda_linkerDestroy_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to copy 2D data between regions within a single
 * device in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] targetAddress the host or device target address
 * @param[in] targetPitch the pitch of the target region
 * @param[in] sourceAddress the host or device source address
 * @param[in] sourcePitch the pitch of the source region
 * @param[in] width the width of a row in bytes
 * @param[in] height the number of rows to be copied
 * @param[in] direction the direction to copy
 * @param[in] stream the stream, or NULL for the default stream
 */
struct Copy2D : public InitializedAfter {
	cudaMemcpyKind const direction;
	size_t const height;
	const void *const sourceAddress;
	size_t const sourcePitch;
	J9CudaStream const stream;
	bool const sync;
	void *const targetAddress;
	size_t const targetPitch;
	size_t const width;

	VMINLINE
	Copy2D(
			void *targetAddress,
			size_t targetPitch,
			const void *sourceAddress,
			size_t sourcePitch,
			size_t width,
			size_t height,
			cudaMemcpyKind direction)
		: direction(direction)
		, height(height)
		, sourceAddress(sourceAddress)
		, sourcePitch(sourcePitch)
		, stream(NULL)
		, sync(true)
		, targetAddress(targetAddress)
		, targetPitch(targetPitch)
		, width(width)
	{
	}

	VMINLINE
	Copy2D(
			void *targetAddress,
			size_t targetPitch,
			const void *sourceAddress,
			size_t sourcePitch,
			size_t width,
			size_t height,
			cudaMemcpyKind direction,
			J9CudaStream stream)
		: direction(direction)
		, height(height)
		, sourceAddress(sourceAddress)
		, sourcePitch(sourcePitch)
		, stream(stream)
		, sync(false)
		, targetAddress(targetAddress)
		, targetPitch(targetPitch)
		, width(width)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = cudaSuccess;

		if (sync) {
			error = functions->Memcpy2D(
					targetAddress, targetPitch,
					sourceAddress, sourcePitch,
					width, height, direction);
		} else {
			error = functions->Memcpy2DAsync(
					targetAddress, targetPitch,
					sourceAddress, sourcePitch,
					width, height, direction, (cudaStream_t)stream);
		}

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Copy data between two regions on a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] targetPitch the pitch of the target region
 * @param[in] sourceAddress the device source address
 * @param[in] sourcePitch the pitch of the source region
 * @param[in] width the width of a row in bytes
 * @param[in] height the number of rows to be copied
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpy2DDeviceToDevice(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height)
{
	Trc_PRT_cuda_memcpy2D_entry(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToDevice);

	const Copy2D operation(targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToDevice);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy2D_exit(result);

	return result;
}

/**
 * Copy data between two regions on a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] targetPitch the pitch of the target region
 * @param[in] sourceAddress the device source address
 * @param[in] sourcePitch the pitch of the source region
 * @param[in] width the width of a row in bytes
 * @param[in] height the number of rows to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpy2DDeviceToDeviceAsync(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream)
{
	Trc_PRT_cuda_memcpy2DAsync_entry(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToDevice, stream);

	const Copy2D operation(targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToDevice, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy2DAsync_exit(result);

	return result;
}

/**
 * Copy data from device to host.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the host target address
 * @param[in] targetPitch the pitch of the target region
 * @param[in] sourceAddress the device source address
 * @param[in] sourcePitch the pitch of the source region
 * @param[in] width the width of a row in bytes
 * @param[in] height the number of rows to be copied
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpy2DDeviceToHost(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height)
{
	Trc_PRT_cuda_memcpy2D_entry(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToHost);

	const Copy2D operation(targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToHost);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy2D_exit(result);

	return result;
}

/**
 * Copy data from device to host.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the host target address
 * @param[in] targetPitch the pitch of the target region
 * @param[in] sourceAddress the device source address
 * @param[in] sourcePitch the pitch of the source region
 * @param[in] width the width of a row in bytes
 * @param[in] height the number of rows to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpy2DDeviceToHostAsync(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream)
{
	Trc_PRT_cuda_memcpy2DAsync_entry(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToHost, stream);

	const Copy2D operation(targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyDeviceToHost, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy2DAsync_exit(result);

	return result;
}

/**
 * Copy data from host to device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] targetPitch the pitch of the target region
 * @param[in] sourceAddress the host source address
 * @param[in] sourcePitch the pitch of the source region
 * @param[in] width the width of a row in bytes
 * @param[in] height the number of rows to be copied
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpy2DHostToDevice(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height)
{
	Trc_PRT_cuda_memcpy2D_entry(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyHostToDevice);

	const Copy2D operation(targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyHostToDevice);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy2D_exit(result);

	return result;
}

/**
 * Copy data from host to device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] targetPitch the pitch of the target region
 * @param[in] sourceAddress the host source address
 * @param[in] sourcePitch the pitch of the source region
 * @param[in] width the width of a row in bytes
 * @param[in] height the number of rows to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpy2DHostToDeviceAsync(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, size_t targetPitch, const void *sourceAddress, size_t sourcePitch, uintptr_t width, uintptr_t height, J9CudaStream stream)
{
	Trc_PRT_cuda_memcpy2DAsync_entry(deviceId, targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyHostToDevice, stream);

	const Copy2D operation(targetAddress, targetPitch, sourceAddress, sourcePitch, width, height, cudaMemcpyHostToDevice, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy2DAsync_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to copy data between regions within a single device
 * in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] targetAddress the host or device target address
 * @param[in] sourceAddress the host or device source address
 * @param[in] byteCount the number of bytes to be copied
 * @param[in] direction the direction to copy
 */
struct Copy : public InitializedAfter {
	uintptr_t const byteCount;
	cudaMemcpyKind const direction;
	void const *const sourceAddress;
	J9CudaStream const stream;
	bool const sync;
	void *const targetAddress;

	VMINLINE
	Copy(void *targetAddress, void const *sourceAddress, uintptr_t byteCount, cudaMemcpyKind direction)
		: byteCount(byteCount)
		, direction(direction)
		, sourceAddress(sourceAddress)
		, stream(NULL)
		, sync(true)
		, targetAddress(targetAddress)
	{
	}

	VMINLINE
	Copy(void *targetAddress, void const *sourceAddress, uintptr_t byteCount, cudaMemcpyKind direction, J9CudaStream stream)
		: byteCount(byteCount)
		, direction(direction)
		, sourceAddress(sourceAddress)
		, stream(stream)
		, sync(false)
		, targetAddress(targetAddress)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = cudaSuccess;

		if (sync) {
			error = functions->Memcpy(targetAddress, sourceAddress, byteCount, direction);
		} else {
			error = functions->MemcpyAsync(targetAddress, sourceAddress, byteCount, direction, (cudaStream_t)stream);
		}

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Copy data between two regions on a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] sourceAddress the device source address
 * @param[in] byteCount the number of bytes to be copied
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyDeviceToDevice(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount)
{
	Trc_PRT_cuda_memcpy_entry(deviceId, targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToDevice);

	const Copy operation(targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToDevice);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy_exit(result);

	return result;
}

/**
 * Copy data between two regions on a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] sourceAddress the device source address
 * @param[in] byteCount the number of bytes to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyDeviceToDeviceAsync(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream)
{
	Trc_PRT_cuda_memcpyAsync_entry(deviceId, targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToDevice, stream);

	const Copy operation(targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToDevice, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpyAsync_exit(result);

	return result;
}

/**
 * Copy data from device to host.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the host target address
 * @param[in] sourceAddress the device source address
 * @param[in] byteCount the number of bytes to be copied
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyDeviceToHost(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount)
{
	Trc_PRT_cuda_memcpy_entry(deviceId, targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToHost);

	const Copy operation(targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToHost);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy_exit(result);

	return result;
}

/**
 * Copy data from device to host.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the host target address
 * @param[in] sourceAddress the device source address
 * @param[in] byteCount the number of bytes to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyDeviceToHostAsync(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream)
{
	Trc_PRT_cuda_memcpyAsync_entry(deviceId, targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToHost, stream);

	const Copy operation(targetAddress, sourceAddress, byteCount, cudaMemcpyDeviceToHost, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpyAsync_exit(result);

	return result;
}

/**
 * Copy data from host to device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] sourceAddress the host source address
 * @param[in] byteCount the number of bytes to be copied
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyHostToDevice(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount)
{
	Trc_PRT_cuda_memcpy_entry(deviceId, targetAddress, sourceAddress, byteCount, cudaMemcpyHostToDevice);

	const Copy operation(targetAddress, sourceAddress, byteCount, cudaMemcpyHostToDevice);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpy_exit(result);

	return result;
}

/**
 * Copy data from host to device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] targetAddress the device target address
 * @param[in] sourceAddress the host source address
 * @param[in] byteCount the number of bytes to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyHostToDeviceAsync(OMRPortLibrary *portLibrary, uint32_t deviceId, void *targetAddress, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream)
{
	Trc_PRT_cuda_memcpyAsync_entry(deviceId, targetAddress, sourceAddress, byteCount, cudaMemcpyHostToDevice, stream);

	const Copy operation(targetAddress, sourceAddress, byteCount, cudaMemcpyHostToDevice, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memcpyAsync_exit(result);

	return result;
}

/**
 * Copy data between two devices.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] targetDeviceId the target device identifier
 * @param[in] targetAddress the device target address
 * @param[in] sourceDeviceId the source device identifier
 * @param[in] sourceAddress the device source address
 * @param[in] byteCount the number of bytes to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyPeer(OMRPortLibrary *portLibrary, uint32_t targetDeviceId, void *targetAddress,
				  uint32_t sourceDeviceId, const void *sourceAddress, uintptr_t byteCount)
{
	Trc_PRT_cuda_memcpyPeer_entry(targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, targetDeviceId);

	if (J9CUDA_NO_ERROR == result) {
		result = validateDeviceId(portLibrary, sourceDeviceId);
	}

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->MemcpyPeer)) {
		cudaError_t error = functions->MemcpyPeer(
				targetAddress, targetDeviceId,
				sourceAddress, sourceDeviceId, byteCount);

		result = mapRuntime(portLibrary, error);
	}

	Trc_PRT_cuda_memcpyPeer_exit(result);

	return result;
}

/**
 * Copy data between two devices.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] targetDeviceId the target device identifier
 * @param[in] targetAddress the device target address
 * @param[in] sourceDeviceId the source device identifier
 * @param[in] sourceAddress the device source address
 * @param[in] byteCount the number of bytes to be copied
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memcpyPeerAsync(OMRPortLibrary *portLibrary, uint32_t targetDeviceId, void *targetAddress,
					   uint32_t sourceDeviceId, const void *sourceAddress, uintptr_t byteCount, J9CudaStream stream)
{
	Trc_PRT_cuda_memcpyPeerAsync_entry(targetDeviceId, targetAddress, sourceDeviceId, sourceAddress, byteCount, stream);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, targetDeviceId);

	if (J9CUDA_NO_ERROR == result) {
		result = validateDeviceId(portLibrary, sourceDeviceId);
	}

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->MemcpyPeerAsync)) {
		cudaError_t error = functions->MemcpyPeerAsync(
					targetAddress, targetDeviceId,
					sourceAddress, sourceDeviceId, byteCount, (cudaStream_t)stream);

		result = mapRuntime(portLibrary, error);
	}

	Trc_PRT_cuda_memcpyPeerAsync_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to fill a region of device memory with a repeating
 * 8-bit value in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] deviceAddress the device address
 * @param[in] value the value to be repeated
 * @param[in] count the number of copies requested
 * @param[in] stream the stream, or NULL for the default stream
 */
struct Fill8 : InitializedBefore {
	uintptr_t const count;
	void *const deviceAddress;
	J9CudaStream const stream;
	uint8_t const value;

	VMINLINE
	Fill8(void *deviceAddress, uint8_t value, uintptr_t count, J9CudaStream stream)
		: count(count)
		, deviceAddress(deviceAddress)
		, stream(stream)
		, value(value)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		return mapDriver(functions->MemsetD8Async((CUdeviceptr)deviceAddress, value, count, (CUstream)stream));
	}
};

} /* namespace */

/**
 * Fill device memory with a repeating 8-bit value.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] deviceAddress the device address
 * @param[in] value the value to be stored
 * @param[in] count the number of values to be stored
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memset8Async(OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, uint8_t value, uintptr_t count, J9CudaStream stream)
{
	Trc_PRT_cuda_memset8_entry(deviceId, deviceAddress, value, count, stream);

	Fill8 operation(deviceAddress, value, count, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memset8_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to fill a region of device memory with a repeating
 * 16-bit value in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] deviceAddress the device address
 * @param[in] value the value to be repeated
 * @param[in] count the number of copies requested
 * @param[in] stream the stream, or NULL for the default stream
 */
struct Fill16 : public InitializedBefore {
	uintptr_t const count;
	void *const deviceAddress;
	J9CudaStream const stream;
	uint16_t const value;

	VMINLINE
	Fill16(void *deviceAddress, uint16_t value, uintptr_t count, J9CudaStream stream)
		: count(count)
		, deviceAddress(deviceAddress)
		, stream(stream)
		, value(value)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		return mapDriver(functions->MemsetD16Async((CUdeviceptr)deviceAddress, value, count, (CUstream)stream));
	}
};

} /* namespace */

/**
 * Fill device memory with a repeating 16-bit value.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] deviceAddress the device address
 * @param[in] value the value to be stored
 * @param[in] count the number of values to be stored
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memset16Async(OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, uint16_t value, uintptr_t count, J9CudaStream stream)
{
	Trc_PRT_cuda_memset16_entry(deviceId, deviceAddress, value, count, stream);

	Fill16 operation(deviceAddress, value, count, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memset16_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to fill a region of device memory with a repeating
 * 32-bit value in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] deviceAddress the device address
 * @param[in] value the value to be repeated
 * @param[in] count the number of copies requested
 * @param[in] stream the stream, or NULL for the default stream
 */
struct Fill32 : public InitializedBefore {
	uintptr_t const count;
	void *const deviceAddress;
	J9CudaStream const stream;
	uint32_t const value;

	VMINLINE
	Fill32(void *deviceAddress, uint32_t value, uintptr_t count, J9CudaStream stream)
		: count(count)
		, deviceAddress(deviceAddress)
		, stream(stream)
		, value(value)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		return mapDriver(functions->MemsetD32Async((CUdeviceptr)deviceAddress, value, count, (CUstream)stream));
	}
};

} /* namespace */

/**
 * Fill device memory with a repeating 32-bit value.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] deviceAddress the device address
 * @param[in] value the value to be stored
 * @param[in] count the number of values to be stored
 * @param[in] stream the stream, or NULL for the default stream
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_memset32Async(OMRPortLibrary *portLibrary, uint32_t deviceId, void *deviceAddress, uint32_t value, uintptr_t count, J9CudaStream stream)
{
	Trc_PRT_cuda_memset32_entry(deviceId, deviceAddress, value, count, stream);

	Fill32 operation(deviceAddress, value, count, stream);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_memset32_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to find a kernel function in a module in conjunction
 * with use of the template function 'withDevice'.
 *
 * @param[in] module the module pointer
 * @param[in] name the requested function name
 * @return the function pointer
 */
struct GetFunction : public InitializedBefore {
	CUfunction function;
	CUmodule const module;
	const char *const name;

	VMINLINE
	GetFunction(CUmodule module, const char *name)
		: function(NULL)
		, module(module)
		, name(name)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		return mapDriver(functions->ModuleGetFunction(&function, module, name));
	}
};

} /* namespace */

/**
 * Find a kernel function in a module.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] module the module handle
 * @param[in] name the requested function name
 * @param[out] functionOut where the function handle should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_moduleGetFunction(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, J9CudaFunction *functionOut)
{
	Trc_PRT_cuda_moduleGetFunction_entry(deviceId, module, name);

	GetFunction operation((CUmodule)module, name);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_moduleGetFunction_result(operation.function);

		*functionOut = (J9CudaFunction)operation.function;
	}

	Trc_PRT_cuda_moduleGetFunction_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to find a global symbol in a module in conjunction with
 * use of the template function 'withDevice'.
 *
 * @param[in] module the module pointer
 * @param[in] name the requested symbol name
 * @return the size of the global
 * @return the global pointer
 */
struct GetGlobal : public InitializedBefore {
	CUmodule const module;
	const char *const name;
	size_t size;
	CUdeviceptr symbol;

	VMINLINE
	GetGlobal(CUmodule module, const char *name)
		: module(module)
		, name(name)
		, size(0)
		, symbol(0)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		return mapDriver(functions->ModuleGetGlobal(&symbol, &size, module, name));
	}
};

} /* namespace */

/**
 * Find a global symbol in a module.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] module the module handle
 * @param[in] name the requested global symbol name
 * @param[out] addressOut where the device address of the symbol should be stored
 * @param[out] sizeOut where the size in bytes of the symbol should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_moduleGetGlobal(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *addressOut, uintptr_t *sizeOut)
{
	Trc_PRT_cuda_moduleGetGlobal_entry(deviceId, module, name);

	GetGlobal operation((CUmodule)module, name);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_moduleGetGlobal_result((void *)operation.symbol, operation.size);

		*addressOut = operation.symbol;
		*sizeOut = operation.size;
	}

	Trc_PRT_cuda_moduleGetGlobal_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to find a surface in a module in conjunction with use
 * of the template function 'withDevice'.
 *
 * @param[in] module the module pointer
 * @param[in] name the requested surface name
 * @return the surface pointer
 */
struct GetSurface : public InitializedBefore {
	CUmodule const module;
	const char *const name;
	CUsurfref surface;

	VMINLINE
	GetSurface(CUmodule module, const char *name)
		: module(module)
		, name(name)
		, surface(NULL)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		return mapDriver(functions->ModuleGetSurfRef(&surface, module, name));
	}
};

} /* namespace */

/**
 * Find a surface reference in a module.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] module the module handle
 * @param[in] name the requested surface reference name
 * @param[out] surfRefOut where the surface reference should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_moduleGetSurfaceRef(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *surfRefOut)
{
	Trc_PRT_cuda_moduleGetSurfRef_entry(deviceId, module, name);

	GetSurface operation((CUmodule)module, name);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_moduleGetSurfRef_result(operation.surface);

		*surfRefOut = (uintptr_t)operation.surface;
	}

	Trc_PRT_cuda_moduleGetSurfRef_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to find a texture in a module in conjunction with use
 * of the template function 'withDevice'.
 *
 * @param[in] module the module pointer
 * @param[in] name the requested texture name
 * @return the texture pointer
 */
struct GetTexture : public InitializedBefore {
	CUmodule const module;
	const char *const name;
	CUtexref texture;

	VMINLINE
	GetTexture(CUmodule module, const char *name)
		: module(module)
		, name(name)
		, texture(NULL)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		return mapDriver(functions->ModuleGetTexRef(&texture, module, name));
	}
};

} /* namespace */

/**
 * Find a texture reference in a module.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] module the module handle
 * @param[in] name the requested texture reference name
 * @param[out] texRefOut where the texture reference should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_moduleGetTextureRef(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module, const char *name, uintptr_t *texRefOut)
{
	Trc_PRT_cuda_moduleGetTexRef_entry(deviceId, module, name);

	GetTexture operation((CUmodule)module, name);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_moduleGetTexRef_result(operation.texture);

		*texRefOut = (uintptr_t)operation.texture;
	}

	Trc_PRT_cuda_moduleGetTexRef_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to load a module in conjunction with use of the
 * template function 'withDevice'.
 *
 * @param[in] image the module image
 * @param[in] options an optional options pointer
 * @return the module pointer
 */
struct Load : public InitializedBefore {
	const void *const image;
	CUmodule module;
	J9CudaJitOptions *const options;

	VMINLINE
	Load(const void *image, J9CudaJitOptions *options)
		: image(image)
		, module(NULL)
		, options(options)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		JitOptions loadOptions;

		J9CudaError result = loadOptions.set(options);

		if (J9CUDA_NO_ERROR == result) {
			result = mapDriver(
					functions->ModuleLoadDataEx(
							&module,
							image,
							loadOptions.getCount(),
							loadOptions.getKeys(),
							loadOptions.getValues()));
		}

		return result;
	}
};

} /* namespace */

/**
 * Load a module onto a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] image the module image
 * @param[in] options an optional options pointer
 * @param[out] moduleOut where the module handle should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_moduleLoad(OMRPortLibrary *portLibrary, uint32_t deviceId, const void *image, J9CudaJitOptions *options, J9CudaModule *moduleOut)
{
	Trc_PRT_cuda_moduleLoad_entry(deviceId, image, options);

	Load operation(image, options);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_moduleLoad_result(operation.module);

		*moduleOut = (J9CudaModule)operation.module;
	}

	Trc_PRT_cuda_moduleLoad_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to unload a module in conjunction with use of the
 * template function 'withDevice'.
 *
 * @param[in] module the module pointer
 */
struct Unload : public InitializedBefore {
	CUmodule const module;

	explicit VMINLINE
	Unload(CUmodule module)
		: module(module)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		return mapDriver(functions->ModuleUnload(module));
	}
};

} /* namespace */

/**
 * Unload a module from a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] module the module handle
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_moduleUnload(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaModule module)
{
	Trc_PRT_cuda_moduleUnload_entry(deviceId, module);

	const Unload operation((CUmodule)module);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	Trc_PRT_cuda_moduleUnload_exit(result);

	return result;
}

/**
 * Query the version number of the CUDA runtime library. Version numbers
 * are represented as ((major * 1000) + (minor * 10)).
 *
 * @param[in] portLibrary the port library pointer
 * @param[out] versionOut where the runtime version number should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_runtimeGetVersion(OMRPortLibrary *portLibrary, uint32_t *versionOut)
{
	Trc_PRT_cuda_runtimeGetVersion_entry();

	/* Ensure that initialization has been done. */
	getFunctions(portLibrary);

	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
	J9CudaError result = J9CUDA_ERROR_NO_DEVICE;

	if (0 != globals->deviceCount) {
		Trc_PRT_cuda_runtimeGetVersion_result(globals->runtimeVersion);

		*versionOut = globals->runtimeVersion;
		result = J9CUDA_NO_ERROR;
	}

	Trc_PRT_cuda_runtimeGetVersion_exit(result);

	return result;
}

namespace
{

/**
 * Instances of this class convey the necessary information to trigger
 * a callback to the function provided by the client.
 */
struct StreamCallback {
	uintptr_t clientData;
	J9CudaStreamCallback clientFunction;
	OMRPortLibrary *portLibrary;

	static void CUDART_CB
	handler(cudaStream_t stream, cudaError_t error, void *data)
	{
		Trc_PRT_cuda_StreamCallback_handler_entry(stream, error, data);

		StreamCallback *instance = (StreamCallback *)data;
		J9CudaError omrCode = mapRuntime(instance->portLibrary, error);

		instance->clientFunction((J9CudaStream)stream, omrCode, instance->clientData);

		OMRPORT_ACCESS_FROM_OMRPORT(instance->portLibrary);

		J9CUDA_FREE_MEMORY(instance);

		Trc_PRT_cuda_StreamCallback_handler_exit();
	}
};

/**
 * The closure required to add a callback on a stream (or the default stream)
 * in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] stream the stream, or NULL for the default stream
 * @param[in] callback the callback object pointer
 */
struct StreamAddCallback : public InitializedAfter {
	StreamCallback *const callback;
	cudaStream_t const stream;

	VMINLINE
	StreamAddCallback(cudaStream_t stream, StreamCallback *callback)
		: callback(callback)
		, stream(stream)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions) const
	{
		cudaError_t error = functions->StreamAddCallback((cudaStream_t)stream, &StreamCallback::handler, callback, 0);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Add a callback to a stream or the default stream of a device.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] stream the stream, or NULL for the default stream
 * @param[in] clientFunction the client's callback function
 * @param[in] clientData data for the client's callback function
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamAddCallback(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, J9CudaStreamCallback clientFunction, uintptr_t clientData)
{
	Trc_PRT_cuda_streamAddCallback_entry(deviceId, stream, (void *)clientFunction, clientData);

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	J9CudaError result = J9CUDA_ERROR_MEMORY_ALLOCATION;
	StreamCallback *callback = (StreamCallback *)J9CUDA_ALLOCATE_MEMORY(sizeof(StreamCallback));

	if (NULL == callback) {
		Trc_PRT_cuda_streamAddCallback_nomem();
	} else {
		Trc_PRT_cuda_streamAddCallback_instance(callback);

		callback->clientData = clientData;
		callback->clientFunction = clientFunction;
		callback->portLibrary = portLibrary;

		const StreamAddCallback operation((cudaStream_t)stream, callback);

		result = withDevice(portLibrary, deviceId, operation);

		if (J9CUDA_NO_ERROR != result) {
			J9CUDA_FREE_MEMORY(callback);
		}
	}

	Trc_PRT_cuda_streamAddCallback_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to create a stream with the default priority in
 * conjunction with use of the template function 'withDevice'.
 *
 * @return a new stream
 */
struct StreamCreate : public InitializedAfter {
	cudaStream_t stream;

	VMINLINE
	StreamCreate()
		: stream(NULL)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		cudaError_t error = functions->StreamCreate(&stream);

		return mapRuntime(portLibrary, error);
	}
};

} /* namespace */

/**
 * Create a stream with the default priority and flags.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[out] streamOut where the stream handle should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamCreate(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream *streamOut)
{
	Trc_PRT_cuda_streamCreate_entry(deviceId);

	StreamCreate operation;

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_streamCreate_result(operation.stream);

		*streamOut = (J9CudaStream)operation.stream;
	}

	Trc_PRT_cuda_streamCreate_exit(result);

	return result;
}

namespace
{

/**
 * The closure required to create a stream with the specified flags and
 * priority in conjunction with use of the template function 'withDevice'.
 *
 * @param[in] flags the stream creation flags
 * @param[in] priority the requested stream priority
 * @return a new stream
 */
struct StreamCreateEx : public InitializedAfter {
	uint32_t const flags;
	int32_t const priority;
	cudaStream_t stream;

	VMINLINE
	StreamCreateEx(uint32_t flags, int32_t priority)
		: flags(flags)
		, priority(priority)
		, stream(NULL)
	{
	}

	VMINLINE J9CudaError
	execute(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions)
	{
		const uint32_t allFlags = J9CUDA_STREAM_FLAG_NON_BLOCKING;

		J9CudaError result = J9CUDA_ERROR_INVALID_VALUE;

		if (OMR_ARE_NO_BITS_SET(flags, ~allFlags)) {
			unsigned int streamFlags = 0;

			if (OMR_ARE_ANY_BITS_SET(flags, J9CUDA_STREAM_FLAG_NON_BLOCKING)) {
				streamFlags |= cudaStreamNonBlocking;
			}

			cudaError_t error = functions->StreamCreateWithPriority(&stream, streamFlags, priority);

			result = mapRuntime(portLibrary, error);
		}

		return result;
	}
};

} /* namespace */

/**
 * Create a stream with the specified priority and flags. The value of the
 * flags parameter must be produced by ORing together one or more of the
 * literals defined by the enum J9CudaStreamFlags, declared in omrcuda.h.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] priority the requested stream priority
 * @param[in] flags the stream creation flags
 * @param[out] streamOut where the stream handle should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamCreateWithPriority(OMRPortLibrary *portLibrary, uint32_t deviceId, int32_t priority, uint32_t flags, J9CudaStream *streamOut)
{
	Trc_PRT_cuda_streamCreateWithPriority_entry(deviceId, priority, flags);

	StreamCreateEx operation(flags, priority);

	J9CudaError result = withDevice(portLibrary, deviceId, operation);

	if (J9CUDA_NO_ERROR == result) {
		Trc_PRT_cuda_streamCreateWithPriority_result(operation.stream);

		*streamOut = (J9CudaStream)operation.stream;
	}

	Trc_PRT_cuda_streamCreateWithPriority_exit(result);

	return result;
}

/**
 * Destroy a stream.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] stream the stream handle
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamDestroy(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream)
{
	Trc_PRT_cuda_streamDestroy_entry(deviceId, stream);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->StreamDestroy)) {
		cudaError_t error = functions->StreamDestroy((cudaStream_t)stream);

		if (cudaSuccess != error) {
			result = mapRuntime(portLibrary, error);
		} else {
			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
		}
	}

	Trc_PRT_cuda_streamDestroy_exit(result);

	return result;
}

/**
 * Query the flags of a stream.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] stream the stream handle
 * @param[out] flagsOut where the stream's flags should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamGetFlags(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, uint32_t *flagsOut)
{
	Trc_PRT_cuda_streamGetFlags_entry(deviceId, stream);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->StreamGetFlags)) {
		unsigned int flags = 0;
		cudaError_t error = functions->StreamGetFlags((cudaStream_t)stream, &flags);

		if (cudaSuccess != error) {
			result = mapRuntime(portLibrary, error);
		} else {
			Trc_PRT_cuda_streamGetFlags_result(flags);

			*flagsOut = 0;

			if (flags & cudaStreamNonBlocking) {
				*flagsOut |= J9CUDA_STREAM_FLAG_NON_BLOCKING;
			}

			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
		}
	}

	Trc_PRT_cuda_streamGetFlags_exit(result);

	return result;
}

/**
 * Query the priority of a stream.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] stream the stream handle
 * @param[out] priorityOut where the stream's priority should be stored
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamGetPriority(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, int32_t *priorityOut)
{
	Trc_PRT_cuda_streamGetPriority_entry(deviceId, stream);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->StreamGetPriority)) {
		int priority = 0;
		cudaError_t error = functions->StreamGetPriority((cudaStream_t)stream, &priority);

		if (cudaSuccess != error) {
			result = mapRuntime(portLibrary, error);
		} else {
			*priorityOut = (int32_t)priority;
			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
			Trc_PRT_cuda_streamGetPriority_result(priority);
		}
	}

	Trc_PRT_cuda_streamGetPriority_exit(result);

	return result;
}

/**
 * Query the completion status of a stream.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] stream the stream handle
 * @return 0 if the stream is complete; else J9CUDA_ERROR_NOT_READY or another error
 */
extern "C" int32_t
omrcuda_streamQuery(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream)
{
	Trc_PRT_cuda_streamQuery_entry(deviceId, stream);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->StreamQuery)) {
		cudaError_t error = functions->StreamQuery((cudaStream_t)stream);

		result = mapRuntime(portLibrary, error);
	}

	Trc_PRT_cuda_streamQuery_exit(result);

	return result;
}

/**
 * Synchronize with a stream.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] stream the stream handle
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamSynchronize(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream)
{
	Trc_PRT_cuda_streamSynchronize_entry(deviceId, stream);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->StreamSynchronize)) {
		cudaError_t error = functions->StreamSynchronize((cudaStream_t)stream);

		if (cudaSuccess != error) {
			result = mapRuntime(portLibrary, error);
		} else {
			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
		}
	}

	Trc_PRT_cuda_streamSynchronize_exit(result);

	return result;
}

/**
 * Cause a stream to wait for an event.
 *
 * @param[in] portLibrary the port library pointer
 * @param[in] deviceId the device identifier
 * @param[in] stream the stream handle
 * @param[in] event the event handle
 * @return J9CUDA_NO_ERROR on success, any other value on failure
 */
extern "C" int32_t
omrcuda_streamWaitEvent(OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaStream stream, J9CudaEvent event)
{
	Trc_PRT_cuda_streamWaitEvent_entry(deviceId, stream, event);

	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	J9CudaError result = validateDeviceId(portLibrary, deviceId);

	if ((J9CUDA_NO_ERROR == result) && (NULL != functions->StreamWaitEvent)) {
		cudaError_t error = functions->StreamWaitEvent((cudaStream_t)stream, (cudaEvent_t)event, 0);

		if (cudaSuccess != error) {
			result = mapRuntime(portLibrary, error);
		} else {
			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
		}
	}

	Trc_PRT_cuda_streamWaitEvent_exit(result);

	return result;
}
