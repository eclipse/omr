/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2013, 2016
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
#if (defined(__xlC__) || defined(__ibmxl__)) && ! defined(__GNUC__)
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

/**
 * The minimum supported driver version.
 */
#define J9CUDA_DRIVER_VERSION_MINIMUM 5050

/**
 * The minimum supported runtime version.
 */
#define J9CUDA_RUNTIME_VERSION_MINIMUM 5050

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

	int32_t result = J9CUDA_NO_ERROR;

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
 * Translate a driver API error to a runtime API error code.
 *
 * @param[in] result a CUDA driver result code
 * @return a representative CUDA runtime error code
 */
cudaError_t
translate(CUresult result)
{
	switch (result) {
	case CUDA_SUCCESS:
		return cudaSuccess;

	case CUDA_ERROR_ASSERT:
		return cudaErrorAssert;
	case CUDA_ERROR_ECC_UNCORRECTABLE:
		return cudaErrorECCUncorrectable;
	case CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED:
		return cudaErrorHostMemoryAlreadyRegistered;
	case CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED:
		return cudaErrorHostMemoryNotRegistered;
	case CUDA_ERROR_INVALID_DEVICE:
		return cudaErrorInvalidDevice;
	case CUDA_ERROR_INVALID_HANDLE:
		return cudaErrorInvalidResourceHandle;
	case CUDA_ERROR_INVALID_IMAGE:
		return cudaErrorInvalidKernelImage;
	case CUDA_ERROR_INVALID_VALUE:
		return cudaErrorInvalidValue;
	case CUDA_ERROR_LAUNCH_FAILED:
		return cudaErrorLaunchFailure;
	case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES:
		return cudaErrorLaunchOutOfResources;
	case CUDA_ERROR_LAUNCH_TIMEOUT:
		return cudaErrorLaunchTimeout;
	case CUDA_ERROR_NO_BINARY_FOR_GPU:
		return cudaErrorNoKernelImageForDevice;
	case CUDA_ERROR_NO_DEVICE:
		return cudaErrorNoDevice;
	case CUDA_ERROR_NOT_PERMITTED:
		return cudaErrorNotPermitted;
	case CUDA_ERROR_NOT_READY:
		return cudaErrorNotReady;
	case CUDA_ERROR_NOT_SUPPORTED:
		return cudaErrorNotSupported;
	case CUDA_ERROR_OPERATING_SYSTEM:
		return cudaErrorOperatingSystem;
	case CUDA_ERROR_OUT_OF_MEMORY:
		return cudaErrorMemoryAllocation;
	case CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED:
		return cudaErrorPeerAccessAlreadyEnabled;
	case CUDA_ERROR_PEER_ACCESS_NOT_ENABLED:
		return cudaErrorPeerAccessNotEnabled;
	case CUDA_ERROR_PEER_ACCESS_UNSUPPORTED:
		return cudaErrorPeerAccessUnsupported;
	case CUDA_ERROR_PROFILER_ALREADY_STARTED:
		return cudaErrorProfilerAlreadyStarted;
	case CUDA_ERROR_PROFILER_ALREADY_STOPPED:
		return cudaErrorProfilerAlreadyStopped;
	case CUDA_ERROR_PROFILER_DISABLED:
		return cudaErrorProfilerDisabled;
	case CUDA_ERROR_PROFILER_NOT_INITIALIZED:
		return cudaErrorProfilerNotInitialized;
	case CUDA_ERROR_SHARED_OBJECT_INIT_FAILED:
		return cudaErrorSharedObjectInitFailed;
	case CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND:
		return cudaErrorSharedObjectSymbolNotFound;
	case CUDA_ERROR_TOO_MANY_PEERS:
		return cudaErrorTooManyPeers;
	case CUDA_ERROR_UNKNOWN:
		return cudaErrorUnknown;
	case CUDA_ERROR_UNMAP_FAILED:
		return cudaErrorUnmapBufferObjectFailed;
	case CUDA_ERROR_UNSUPPORTED_LIMIT:
		return cudaErrorUnsupportedLimit;

	default:
		return (cudaError_t)-(int32_t)result;
	}
}

/**
 * Query the name of a device. The name is returned as a nul-terminated string.
 * The parameter, nameSize, must include room for the trailing nul.
 *
 * @param[in] functions the function table pointer
 * @param[in] deviceId the device identifier
 * @param[in] nameSize the size of the buffer where the name should be stored
 * @param[out] nameOut the buffer where the name should be stored
 * @return cudaSuccess on success, any other value on failure
 */
cudaError_t
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

	return translate(result);
}

/**
 * Collect information about a specific CUDA device for inclusion
 * in a javacore file.
 *
 * @param[in] functions the function table pointer
 * @param[in] deviceId the device identifier
 * @param[out] deviceData the device descriptor pointer
 * @return cudaSuccess on success or a non-zero value on failure
 */
cudaError_t
initDeviceData(J9CudaFunctionTable *functions, uint32_t deviceId, J9CudaDeviceDescriptor *deviceData)
{
	Trc_PRT_cuda_initDeviceData_entry(deviceId);

	cudaError_t result = cudaSuccess;

	{
		result = functions->SetDevice((int)deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail2("set", result);
			goto done;
		}
	}

	{
		int pciDomainId = 0;

		result = functions->DeviceGetAttribute(&pciDomainId, cudaDevAttrPciDomainId, (int)deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail("PCI domain id", result);
			goto done;
		}

		deviceData->pciDomainId = (uint32_t)pciDomainId;
	}

	{
		int pciBusId = 0;

		result = functions->DeviceGetAttribute(&pciBusId, cudaDevAttrPciBusId, (int)deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail("PCI bus id", result);
			goto done;
		}

		deviceData->pciBusId = (uint32_t)pciBusId;
	}

	{
		int pciDeviceId = 0;

		result = functions->DeviceGetAttribute(&pciDeviceId, cudaDevAttrPciDeviceId, (int)deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail("PCI device id", result);
			goto done;
		}

		deviceData->pciDeviceId = (uint32_t)pciDeviceId;
	}

	{
		int computeMajor = 0;

		result = functions->DeviceGetAttribute(&computeMajor, cudaDevAttrComputeCapabilityMajor, (int)deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail("major compute capability", result);
			goto done;
		}

		deviceData->computeCapability = (uint32_t)(computeMajor * 10);
	}

	{
		int computeMinor = 0;

		result = functions->DeviceGetAttribute(&computeMinor, cudaDevAttrComputeCapabilityMinor, (int)deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail("minor compute capability", result);
			goto done;
		}

		deviceData->computeCapability += (uint32_t)computeMinor;
	}

	{
		int computeMode = 0;

		result = functions->DeviceGetAttribute(&computeMode, cudaDevAttrComputeMode, (int)deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail("compute mode", result);
			goto done;
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

	{
		result = getDeviceName(functions, deviceId, (uint32_t)sizeof(deviceData->deviceName), deviceData->deviceName);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_initDeviceData_fail("name", result);
			goto done;
		}
	}

done:
	Trc_PRT_cuda_initDeviceData_exit(result);

	return result;
}

/**
 * Collect configuration information about the CUDA environment for inclusion
 * in service data.
 *
 * @param[in] portLibrary the port library pointer
 * @return cudaSuccess on success or a non-zero value on failure
 */
cudaError_t
initConfigData(OMRPortLibrary *portLibrary)
{
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
	uint32_t deviceCount = globals->deviceCount;

	Trc_PRT_cuda_initConfigData_entry(deviceCount);

	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

	cudaError_t result = cudaSuccess;
	/**
	 * The instance of J9CudaConfig allocated here is immediately followed
	 * by an array of deviceCount J9CudaDeviceDescriptor objects.
	 */
	size_t configSize = sizeof(J9CudaConfig) + (sizeof(J9CudaDeviceDescriptor) * deviceCount);
	J9CudaConfig *config = (J9CudaConfig *)J9CUDA_ALLOCATE_MEMORY(configSize);

	if (NULL == config) {
		Trc_PRT_cuda_initConfigData_fail("allocate config space");
		result = cudaErrorMemoryAllocation;
		goto done;
	}

	memset(config, 0, configSize);

	if (0 != deviceCount) {
		J9CudaDeviceDescriptor *details = (J9CudaDeviceDescriptor *)&config[1];
		J9CudaFunctionTable *functions = (J9CudaFunctionTable *)globals->functionTable;

		if (1 == deviceCount) {
			/* With only one device, there's no need to save and restore the active device. */
			result = initDeviceData(functions, 0, &details[0]);
		} else {
			int current = 0;

			result = functions->GetDevice(&current);

			if (cudaSuccess != result) {
				Trc_PRT_cuda_initDeviceData_fail2("get", result);
				goto fail;
			}

			for (uint32_t deviceId = 0; deviceId < deviceCount; ++deviceId) {
				result = initDeviceData(functions, deviceId, &details[deviceId]);

				if (cudaSuccess != result) {
					Trc_PRT_cuda_initConfigData_fail("initialize detail");
					break;
				}
			}

			cudaError_t restore = functions->SetDevice(current);

			if (cudaSuccess != restore) {
				Trc_PRT_cuda_initDeviceData_fail2("restore", restore);

				if (cudaSuccess == result) {
					result = restore;
				}
			}
		}
	}

	if (cudaSuccess != result) {
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

/**
 * Expands to a J9CudaEntryDescriptor initializer for a runtime API functioln.
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
#elif defined(WIN32)
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

		if (version < J9CUDA_DRIVER_VERSION_MINIMUM) {
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
 * @param[in] library the name of the runtime library to open
 * @return the runtime version or zero on failure
 */
uint32_t
openRuntimeAndGetVersion(OMRPortLibrary *portLibrary, uint32_t bestVersion, const char *library)
{
	uintptr_t dllHandle = 0;
	J9CudaGlobalData *globals = &portLibrary->portGlobals->cudaGlobals;
	J9CudaFunctionTable localFunctions;
	int version = 0;

	/* open the runtime shared library */
	if (0 != portLibrary->sl_open_shared_library(
			portLibrary,
			(char *)library,
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

		if (version < J9CUDA_RUNTIME_VERSION_MINIMUM) {
			goto fail;
		}

		if ((uint32_t)version <= bestVersion) {
			/* ignore this share library unless it is a better version */
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
 * A descriptor for a shared library.
 */
struct J9CudaLibraryDescriptor {
	/** expected version */
	uint32_t version;
	/** (decorated) name */
	const char *name;
};

/**
 * The possible versions of runtime shared libraries providing the underlying
 * implementation of the CUDA access layer. Entries are qualified by the version
 * expected to be provided and are only considered if the version is newer than
 * the best found so far. Newer versions should appear earlier in the list to
 * minimize the time spent loading older versions.
 */
const J9CudaLibraryDescriptor runtimeLibraries[] = {
	/*
	 * This special entry must appear first for forward compatiblity. In normal
	 * installations, it is a link to the newest version available.
	 */
#if defined(LINUX) && defined(OMR_ENV_DATA64)
	{ 5050, "libcudart.so" },
#elif defined(OSX)
	{ 5050, "libcudart.dylib" },
#endif /* LINUX && OMR_ENV_DATA64 */

#if defined(LINUX) && defined(OMR_ENV_DATA64)
#   define OMRCUDA_LIBRARY_NAME(major, minor) ("libcudart.so." #major "." #minor)
#elif defined(OSX)
#   define OMRCUDA_LIBRARY_NAME(major, minor) ("libcudart." #major "." #minor ".dylib")
#elif defined(WIN32) && defined(OMR_ENV_DATA64)
#   define OMRCUDA_LIBRARY_NAME(major, minor) ("cudart64_" #major #minor ".dll")
#elif defined(WIN32)
#   define OMRCUDA_LIBRARY_NAME(major, minor) ("cudart32_" #major #minor ".dll")
#endif /* defined(LINUX) && defined(OMR_ENV_DATA64) */

#define OMRCUDA_LIBRARY_ENTRY(major, minor) { ((major) * 1000) + ((minor) * 10), OMRCUDA_LIBRARY_NAME(major, minor) }

	OMRCUDA_LIBRARY_ENTRY(7, 5),
	OMRCUDA_LIBRARY_ENTRY(7, 0),
	OMRCUDA_LIBRARY_ENTRY(6, 5),
	OMRCUDA_LIBRARY_ENTRY(6, 0),
	OMRCUDA_LIBRARY_ENTRY(5, 5)

#undef OMRCUDA_LIBRARY_ENTRY
#undef OMRCUDA_LIBRARY_NAME
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

	for (uint32_t index = 0; index < LENGTH_OF(runtimeLibraries); ++index) {
		const J9CudaLibraryDescriptor *library = &runtimeLibraries[index];

		if (bestVersion >= library->version) {
			/* this library isn't expected to be better */
			continue;
		}

		uint32_t version = openRuntimeAndGetVersion(portLibrary, bestVersion, library->name);

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

		if (openRuntime(portLibrary) < J9CUDA_RUNTIME_VERSION_MINIMUM) {
			goto fail;
		}

		if (openDriver(portLibrary) < J9CUDA_DRIVER_VERSION_MINIMUM) {
fail:
			globals->deviceCount = 0;
			newState = J9CUDA_STATE_FAILED;
		}

		if (cudaSuccess != initConfigData(portLibrary)) {
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
		cudaError_t result = cudaSuccess;

		result = functions->GetDevice(&current);

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
 * @return cudaSuccess on success; cudaErrorNoDevice or cudaErrorInvalidDevice on failure
 */
VMINLINE cudaError_t
validateDeviceId(OMRPortLibrary *portLibrary, uint32_t deviceId)
{
	uint32_t deviceCount = getDeviceCount(portLibrary);

	return (0 == deviceCount) ? cudaErrorNoDevice : (deviceId < deviceCount) ? cudaSuccess : cudaErrorInvalidDevice;
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
	 * @return cudaSuccess on success, any other value on failure
	 */
	static cudaError_t
	initDeviceForCurrentThread(OMRPortLibrary *portLibrary, J9CudaFunctionTable *functions, uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_initCurrent_entry(deviceId);

		uint32_t deviceCount = getDeviceCount(portLibrary);
		cudaError_t result = cudaErrorInvalidDevice;

		if (deviceId < deviceCount) {
			ThreadState *state = getStateForCurrentThread(portLibrary, deviceCount);

			if (NULL == state) {
				result = cudaErrorMemoryAllocation;
			} else if (state->test(deviceId)) {
				/* previously initialized */
				result = cudaSuccess;
			} else {
				/* initialize the device context for the current thread */
				result = functions->Free(NULL);

				if (cudaSuccess == result) {
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
	 * @return cudaSuccess on success, any other value on failure
	 */
	static cudaError_t
	markDeviceForCurrentThread(OMRPortLibrary *portLibrary, uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_markCurrent_entry(deviceId);

		uint32_t deviceCount = getDeviceCount(portLibrary);
		cudaError_t result = cudaErrorInvalidDevice;

		if (deviceId < deviceCount) {
			ThreadState *state = getStateForCurrentThread(portLibrary, deviceCount);

			if (NULL == state) {
				result = cudaErrorMemoryAllocation;
			} else {
				result = cudaSuccess;
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
	 * @return cudaSuccess on success, any other value on failure
	 */
	static cudaError_t
	unmarkDeviceForCurrentThread(OMRPortLibrary *portLibrary, uint32_t deviceId)
	{
		Trc_PRT_cuda_ThreadState_unmarkCurrent_entry(deviceId);

		uint32_t deviceCount = getDeviceCount(portLibrary);
		cudaError_t result = cudaErrorInvalidDevice;

		if (deviceId < deviceCount) {
			ThreadState *state = getStateForCurrentThread(portLibrary, deviceCount);

			if (NULL == state) {
				result = cudaErrorMemoryAllocation;
			} else {
				result = cudaSuccess;
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
	 * @return cudaSuccess on success, any other value on failure
	 */
	VMINLINE cudaError_t
	prepare(OMRPortLibrary *, J9CudaFunctionTable *, uint32_t) const
	{
		/* Initialization not needed. */
		return cudaSuccess;
	}

	/**
	 * Mark the calling thread as having successfully used the specified device.
	 *
	 * @param[in] (unused) the port library pointer
	 * @param[in] (unused) the device identifier
	 * @return cudaSuccess on success, any other value on failure
	 */
	VMINLINE cudaError_t
	onSuccess(OMRPortLibrary *, uint32_t) const
	{
		/* Initialization not needed. */
		return cudaSuccess;
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
	 * @return cudaSuccess on success, any other value on failure
	 */
	VMINLINE cudaError_t
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
	 * @return cudaSuccess on success, any other value on failure
	 */
	VMINLINE cudaError_t
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
 * @return cudaSuccess on success, any other value on failure
 */
template<typename Operation>
VMINLINE cudaError_t
withDevice(OMRPortLibrary *portLibrary, uint32_t deviceId, Operation &operation)
{
	Trc_PRT_cuda_withDevice_entry(deviceId);

	int current = 0;
	J9CudaFunctionTable *functions = getFunctions(portLibrary);
	cudaError_t result = cudaErrorNoDevice;

	/*
	 * If any of the (required) functions is not available, then GetDevice will
	 * be NULL in which case we simply return cudaErrorNoDevice. If GetDevice
	 * is not NULL, then we need not check again that a required function is
	 * available (neither here nor within the Operation type).
	 */
	if (NULL != functions->GetDevice) {
		result = functions->GetDevice(&current);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_withDevice_get_fail(result);
			goto done;
		}

		if (current != (int)deviceId) {
			result = functions->SetDevice((int)deviceId);

			if (cudaSuccess != result) {
				Trc_PRT_cuda_withDevice_set_fail(result);
				goto done;
			}
		}

		result = operation.prepare(portLibrary, functions, deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_withDevice_prepare_fail(result);
			goto restore;
		}

		result = operation.execute(functions);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_withDevice_execute_fail(result);
			goto restore;
		}

		result = operation.onSuccess(portLibrary, deviceId);

		if (cudaSuccess != result) {
			Trc_PRT_cuda_withDevice_onSuccess_fail(result);
		}

restore:
		if (current != (int)deviceId) {
			cudaError_t restoreResult = functions->SetDevice(current);

			if (cudaSuccess != restoreResult) {
				Trc_PRT_cuda_withDevice_restore_fail(restoreResult);

				if (cudaSuccess == result) {
					result = restoreResult;
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return functions->Malloc(&deviceAddress, byteCount);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if (cudaSuccess == result) {
		result = validateDeviceId(portLibrary, peerDeviceId);
	}

	if ((cudaSuccess == result) && (NULL != functions->DeviceCanAccessPeer)) {
		int access = 0;

		result = functions->DeviceCanAccessPeer(&access, (int)deviceId, (int)peerDeviceId);

		if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return functions->DeviceDisablePeerAccess((int)peerDeviceId);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return functions->DeviceEnablePeerAccess((int)peerDeviceId, 0);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return functions->Free(deviceAddress);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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
	cudaError_t result = cudaErrorNoDevice;
	cudaDeviceAttr deviceAttribute = cudaDevAttrWarpSize;

	if (NULL != functions->DeviceGetAttribute) {
		switch (attribute) {
		default:
			result = cudaErrorInvalidValue;
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

		result = functions->DeviceGetAttribute(&value, deviceAttribute, (int)deviceId);

		if (cudaSuccess != result) {
			goto done;
		}

		if (J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY == attribute) {
			int minor = 0;

			result = functions->DeviceGetAttribute(&minor, cudaDevAttrComputeCapabilityMinor, (int)deviceId);

			if (cudaSuccess != result) {
				goto done;
			}

			value = (value * 10) + minor;
		}

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return functions->DeviceGetCacheConfig(&config);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
		Trc_PRT_cuda_deviceGetCacheConfig_result(operation.config);

		switch (operation.config) {
		default:
			result = cudaErrorInvalidValue;
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

	Trc_PRT_cuda_deviceGetCount_exit(cudaSuccess);

	return cudaSuccess;
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		cudaLimit deviceLimit = cudaLimitStackSize;

		switch (limit) {
		default:
			return cudaErrorInvalidValue;

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

		return functions->DeviceGetLimit(&value, deviceLimit);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return functions->MemGetInfo(&freeBytes, &totalBytes);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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
	cudaError_t result = getDeviceName(functions, deviceId, nameSize, nameOut);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return functions->DeviceGetSharedMemConfig(&config);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
		Trc_PRT_cuda_deviceGetSharedMemConfig_result(operation.config);

		switch (operation.config) {
		default:
			result = cudaErrorInvalidValue;
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return functions->DeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return functions->DeviceReset();
	}

	/**
	 * Unmark the calling thread as having successfully used the specified device.
	 *
	 * @param[in] portLibrary the port library pointer
	 * @param[in] deviceId the device identifier
	 * @return cudaSuccess on success, any other value on failure
	 */
	VMINLINE cudaError_t
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		cudaFuncCache deviceConfig = cudaFuncCachePreferEqual;

		switch (config) {
		default:
			return cudaErrorInvalidValue;

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

		return functions->DeviceSetCacheConfig(deviceConfig);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		cudaLimit deviceLimit = cudaLimitStackSize;

		switch (limit) {
		default:
			return cudaErrorInvalidValue;

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

		return functions->DeviceSetLimit(deviceLimit, value);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		cudaSharedMemConfig deviceConfig = cudaSharedMemBankSizeDefault;

		switch (config) {
		default:
			return cudaErrorInvalidValue;

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

		return functions->DeviceSetSharedMemConfig(deviceConfig);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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
	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return functions->DeviceSynchronize();
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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
	cudaError_t result = cudaErrorNoDevice;

	if (0 != globals->deviceCount) {
		Trc_PRT_cuda_driverGetVersion_result(globals->driverVersion);

		*versionOut = globals->driverVersion;
		result = cudaSuccess;
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		const uint32_t allFlags = J9CUDA_EVENT_FLAG_BLOCKING_SYNC
								  | J9CUDA_EVENT_FLAG_DISABLE_TIMING
								  | J9CUDA_EVENT_FLAG_INTERPROCESS;

		cudaError_t error = cudaErrorInvalidValue;

		if (J9_ARE_NO_BITS_SET(flags, ~allFlags)) {
			unsigned int eventFlags = 0;

			if (J9_ARE_ANY_BITS_SET(flags, J9CUDA_EVENT_FLAG_BLOCKING_SYNC)) {
				eventFlags |= cudaEventBlockingSync;
			}
			if (J9_ARE_ANY_BITS_SET(flags, J9CUDA_EVENT_FLAG_DISABLE_TIMING)) {
				eventFlags |= cudaEventDisableTiming;
			}
			if (J9_ARE_ANY_BITS_SET(flags, J9CUDA_EVENT_FLAG_INTERPROCESS)) {
				eventFlags |= cudaEventInterprocess;
			}

			error = functions->EventCreateWithFlags(&event, eventFlags);
		}

		return error;
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if ((cudaSuccess == result) && (NULL != functions->EventDestroy)) {
		result = functions->EventDestroy((cudaEvent_t)event);

		if (cudaSuccess == result) {
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
	cudaError_t result = cudaErrorNoDevice;

	if (NULL != functions->EventElapsedTime) {
		result = functions->EventElapsedTime(elapsedMillisOut, (cudaEvent_t)startEvent, (cudaEvent_t)endEvent);

		if (cudaSuccess == result) {
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
	cudaError_t result = cudaErrorNoDevice;

	if (NULL != functions->EventQuery) {
		result = functions->EventQuery((cudaEvent_t)event);
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return functions->EventRecord(event, stream);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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
	cudaError_t result = cudaErrorNoDevice;

	if (NULL != functions->EventSynchronize) {
		result = functions->EventSynchronize((cudaEvent_t)event);
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		CUfunction_attribute functionAttribute = CU_FUNC_ATTRIBUTE_BINARY_VERSION;

		switch (attribute) {
		default:
			return cudaErrorInvalidValue;

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

		return translate(functions->FuncGetAttribute(&value, functionAttribute, function));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return translate(
				   functions->OccupancyMaxActiveBlocksPerMultiprocessorWithFlags(
					   &numBlocks,
					   function,
					   (int)blockSize,
					   (size_t)dynamicSharedMemorySize,
					   (int)flags));
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if (cudaSuccess == result) {
		if (NULL == functions->OccupancyMaxActiveBlocksPerMultiprocessorWithFlags) {
			result = cudaErrorNotSupported;
		} else {
			FunctionMaxActiveBlocks operation((CUfunction)function, blockSize, dynamicSharedMemorySize, flags);

			result = withDevice(portLibrary, deviceId, operation);

			if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
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

		return translate(result);
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if (cudaSuccess == result) {
		/* device & function attributes */
		uint32_t deviceMaxThreadsPerBlock = 0;
		uint32_t deviceMaxThreadsPerMultiProcessor = 0;
		uint32_t deviceMultiProcessorCount = 0;
		uint32_t deviceWarpSize = 0;
		uint32_t functionMaxThreadsPerBlock = 0;

		if (NULL == functions->OccupancyMaxActiveBlocksPerMultiprocessorWithFlags) {
			result = cudaErrorNotSupported;
			goto done;
		}

		/* these are implied by the presence of the occupancy API */
		Assert_PRT_true(NULL != functions->DeviceGetAttribute);
		Assert_PRT_true(NULL != functions->FuncGetAttribute);

#define GET_DEVICE_ATTRIBUTE(attribute) \
		do { \
			int value = 0; \
			result = functions->DeviceGetAttribute(&value, (cudaDevAttr ## attribute), (int)deviceId); \
			if (cudaSuccess != result) { \
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
			result = cudaErrorInvalidValue;
			goto done;
		}

		int value = 0;
		CUresult error = functions->FuncGetAttribute(
							 &value,
							 CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK,
							 (CUfunction)function);

		if (CUDA_SUCCESS != error) {
			result = translate(error);
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

		if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		CUfunc_cache deviceConfig = CU_FUNC_CACHE_PREFER_EQUAL;

		switch (config) {
		default:
			return cudaErrorInvalidValue;

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

		return translate(functions->FuncSetCacheConfig(function, deviceConfig));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		CUsharedconfig sharedConfig = CU_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;

		switch (config) {
		default:
			return cudaErrorInvalidValue;

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

		return translate(functions->FuncSetSharedMemConfig(function, sharedConfig));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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
			result = functions->GetErrorString((cudaError_t)error);
		}

		if (NULL == result) {
			/*
			 * Provide answers for the error codes we return if we
			 * were unable to load the required shared libraries.
			 */
			if (cudaSuccess == error) {
				result = "no error";
			} else if (cudaErrorNoDevice == error) {
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
	cudaError_t result = cudaErrorNoDevice;

	if (NULL != functions->HostAlloc) {
		if (J9_ARE_ANY_BITS_SET(flags, ~allFlags)) {
			result = cudaErrorInvalidValue;
		} else {
			unsigned int cudaFlags = cudaHostAllocDefault;

			if (J9_ARE_ANY_BITS_SET(flags, J9CUDA_HOST_ALLOC_MAPPED)) {
				cudaFlags |= cudaHostAllocMapped;
			}
			if (J9_ARE_ANY_BITS_SET(flags, J9CUDA_HOST_ALLOC_PORTABLE)) {
				cudaFlags |= cudaHostAllocPortable;
			}
			if (J9_ARE_ANY_BITS_SET(flags, J9CUDA_HOST_ALLOC_WRITE_COMBINED)) {
				cudaFlags |= cudaHostAllocWriteCombined;
			}

			result = functions->HostAlloc(hostAddressOut, size, cudaFlags);

			if (cudaSuccess == result) {
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
	cudaError_t result = cudaErrorNoDevice;

	if (NULL != functions->FreeHost) {
		result = functions->FreeHost(hostAddress);
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return translate(
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t
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
cudaError_t
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
			CUjit_target target = CU_TARGET_COMPUTE_10;

			switch (j9options->target) {
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
#if CUDA_VERSION >= 6000
			case 32:
				target = CU_TARGET_COMPUTE_32;
				break;
#endif
			case 35:
				target = CU_TARGET_COMPUTE_35;
				break;
#if CUDA_VERSION >= 6050
			case 37:
				target = CU_TARGET_COMPUTE_37;
				break;
#endif
#if CUDA_VERSION >= 6000
			case 50:
				target = CU_TARGET_COMPUTE_50;
				break;
#endif
			}

			keys[index] = CU_JIT_TARGET;
			values[index] = (void *)target;
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

	return cudaSuccess;

invalid:
	Trc_PRT_cuda_JitOptions_exit_invalid();

	return cudaErrorInvalidValue;
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		JitOptions addOptions;

		cudaError_t result = addOptions.set(options);

		if (cudaSuccess == result) {
			CUjitInputType inputType = CU_JIT_INPUT_FATBINARY;

			switch (type) {
			default:
				result = cudaErrorInvalidValue;
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

			result = translate(
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return translate(functions->LinkComplete(linker->state, &data, &size));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return translate(
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
	cudaError_t result = cudaErrorMemoryAllocation;

	if (NULL == linker) {
		Trc_PRT_cuda_linkerCreate_nomem();
	} else {
		result = linker->createOptions.set(options);

		if (cudaSuccess == result) {
			const LinkerCreate operation(linker);

			linker->state = NULL;
			result = withDevice(portLibrary, deviceId, operation);
		}

		if (cudaSuccess != result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return translate(functions->LinkDestroy(linker->state));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		if (sync) {
			return functions->Memcpy2D(
					   targetAddress, targetPitch,
					   sourceAddress, sourcePitch,
					   width, height, direction);
		} else {
			return functions->Memcpy2DAsync(
					   targetAddress, targetPitch,
					   sourceAddress, sourcePitch,
					   width, height, direction, (cudaStream_t)stream);
		}
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		if (sync) {
			return functions->Memcpy(targetAddress, sourceAddress, byteCount, direction);
		} else {
			return functions->MemcpyAsync(targetAddress, sourceAddress, byteCount, direction, (cudaStream_t)stream);
		}
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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
	cudaError_t result = validateDeviceId(portLibrary, targetDeviceId);

	if (cudaSuccess == result) {
		result = validateDeviceId(portLibrary, sourceDeviceId);
	}

	if ((cudaSuccess == result) && (NULL != functions->MemcpyPeer)) {
		result = functions->MemcpyPeer(
					 targetAddress, targetDeviceId,
					 sourceAddress, sourceDeviceId, byteCount);
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
	cudaError_t result = validateDeviceId(portLibrary, targetDeviceId);

	if (cudaSuccess == result) {
		result = validateDeviceId(portLibrary, sourceDeviceId);
	}

	if ((cudaSuccess == result) && (NULL != functions->MemcpyPeerAsync)) {
		result = functions->MemcpyPeerAsync(
					 targetAddress, targetDeviceId,
					 sourceAddress, sourceDeviceId, byteCount, (cudaStream_t)stream);
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return translate(functions->MemsetD8Async((CUdeviceptr)deviceAddress, value, count, (CUstream)stream));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return translate(functions->MemsetD16Async((CUdeviceptr)deviceAddress, value, count, (CUstream)stream));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return translate(functions->MemsetD32Async((CUdeviceptr)deviceAddress, value, count, (CUstream)stream));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return translate(functions->ModuleGetFunction(&function, module, name));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return translate(functions->ModuleGetGlobal(&symbol, &size, module, name));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return translate(functions->ModuleGetSurfRef(&surface, module, name));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return translate(functions->ModuleGetTexRef(&texture, module, name));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		JitOptions loadOptions;

		cudaError_t result = loadOptions.set(options);

		if (cudaSuccess == result) {
			result = translate(
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return translate(functions->ModuleUnload(module));
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

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
	cudaError_t result = cudaErrorNoDevice;

	if (0 != globals->deviceCount) {
		Trc_PRT_cuda_runtimeGetVersion_result(globals->runtimeVersion);

		*versionOut = globals->runtimeVersion;
		result = cudaSuccess;
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

		instance->clientFunction((J9CudaStream)stream, error, instance->clientData);

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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions) const
	{
		return functions->StreamAddCallback((cudaStream_t)stream, &StreamCallback::handler, callback, 0);
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

	cudaError_t result = cudaSuccess;
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

		if (cudaSuccess != result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		return functions->StreamCreate(&stream);
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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

	VMINLINE cudaError_t
	execute(J9CudaFunctionTable *functions)
	{
		const uint32_t allFlags = J9CUDA_STREAM_FLAG_NON_BLOCKING;

		cudaError_t error = cudaErrorInvalidValue;

		if (J9_ARE_NO_BITS_SET(flags, ~allFlags)) {
			unsigned int streamFlags = 0;

			if (J9_ARE_ANY_BITS_SET(flags, J9CUDA_STREAM_FLAG_NON_BLOCKING)) {
				streamFlags |= cudaStreamNonBlocking;
			}

			error = functions->StreamCreateWithPriority(&stream, streamFlags, priority);
		}

		return error;
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

	cudaError_t result = withDevice(portLibrary, deviceId, operation);

	if (cudaSuccess == result) {
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if ((cudaSuccess == result) && (NULL != functions->StreamDestroy)) {
		result = functions->StreamDestroy((cudaStream_t)stream);

		if (cudaSuccess == result) {
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if ((cudaSuccess == result) && (NULL != functions->StreamGetFlags)) {
		unsigned int flags = 0;

		result = functions->StreamGetFlags((cudaStream_t)stream, &flags);

		if (cudaSuccess == result) {
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if ((cudaSuccess == result) && (NULL != functions->StreamGetPriority)) {
		int priority = 0;

		result = functions->StreamGetPriority((cudaStream_t)stream, &priority);

		if (cudaSuccess == result) {
			Trc_PRT_cuda_streamGetPriority_result(priority);

			*priorityOut = (int32_t)priority;
			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if ((cudaSuccess == result) && (NULL != functions->StreamQuery)) {
		result = functions->StreamQuery((cudaStream_t)stream);
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if ((cudaSuccess == result) && (NULL != functions->StreamSynchronize)) {
		result = functions->StreamSynchronize((cudaStream_t)stream);

		if (cudaSuccess == result) {
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
	cudaError_t result = validateDeviceId(portLibrary, deviceId);

	if ((cudaSuccess == result) && (NULL != functions->StreamWaitEvent)) {
		result = functions->StreamWaitEvent((cudaStream_t)stream, (cudaEvent_t)event, 0);

		if (cudaSuccess == result) {
			result = ThreadState::markDeviceForCurrentThread(portLibrary, deviceId);
		}
	}

	Trc_PRT_cuda_streamWaitEvent_exit(result);

	return result;
}
