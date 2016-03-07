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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#ifndef J9CUDA_H_
#define J9CUDA_H_

#include "omrcomp.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/**
 * A function pointer for use with omrcuda_funcMaxActiveBlocksPerMultiprocessor.
 * Given a potential block size the function should return the number of bytes
 * of dynamic shared memory required to launch a related kernel function with
 * that block size.
 */
typedef uintptr_t (* J9CudaBlockToDynamicSharedMemorySize)(uint32_t blockSize, uintptr_t userData);

/**
 * Fast local memory can be split between an L1 cache and shared memory.
 *
 * Note: literal values must not be changed without making the corresponding
 * changes to CudaDevice.CacheConfig in CUDA4J.
 */
typedef enum J9CudaCacheConfig {
	/** prefer equal sized L1 cache and shared memory */
	J9CUDA_CACHE_CONFIG_PREFER_EQUAL,
	/** prefer larger L1 cache and smaller shared memory */
	J9CUDA_CACHE_CONFIG_PREFER_L1,
	/** no preference for shared memory or L1 (default) */
	J9CUDA_CACHE_CONFIG_PREFER_NONE,
	/** prefer larger shared memory and smaller L1 cache */
	J9CUDA_CACHE_CONFIG_PREFER_SHARED,

	J9CUDA_CACHE_CONFIG_DUMMY = 0x40000000 /* force wide enums */
} J9CudaCacheConfig;

/**
 * Possible values returned for device attribute J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_MODE.
 *
 * Note: literal values must not be changed without affecting public API of CUDA4J
 * (see CudaDevice.COMPUTE_MODE_*).
 */
typedef enum J9CudaComputeMode {
	/** Default compute mode (multiple contexts allowed per device). */
	J9CUDA_COMPUTE_MODE_DEFAULT,

	/**
	 * Compute exclusive process mode (at most one context used by a single process
	 * can be present on this device at a time).
	 */
	J9CUDA_COMPUTE_MODE_PROCESS_EXCLUSIVE,

	/** Compute prohibited mode (no contexts can be created on this device at this time). */
	J9CUDA_COMPUTE_MODE_PROHIBITED,

	/**
	 * Exclusive thread mode (at most one context, used by a single thread,
	 * can be present on this device at a time).
	 */
	J9CUDA_COMPUTE_MODE_THREAD_EXCLUSIVE,

	J9CUDA_COMPUTE_MODE_DUMMY = 0x40000000 /* force wide enums */
} J9CudaComputeMode;

/**
 * A description of a specific CUDA device.
 */
typedef struct J9CudaDeviceDescriptor {
	/** total device global memory in bytes */
	uintptr_t totalMemory;
	/** available device global memory in bytes, or ~0 if can't be determined */
	uintptr_t availableMemory;
	/** PCI domain identifier */
	uint32_t pciDomainId;
	/** PCI bus identifier */
	uint32_t pciBusId;
	/** PCI device identifier */
	uint32_t pciDeviceId;
	/** compute capability encoded as (10 * major + minor) */
	uint32_t computeCapability;
	/** compute mode */
	J9CudaComputeMode computeMode;
	/** nul-terminated device name */
	char deviceName[256];
} J9CudaDeviceDescriptor;

/**
 * CUDA device attribute codes for use with omrcuda_deviceGetAttribute.
 */
typedef enum J9CudaDeviceAttribute {
	/** Number of asynchronous engines. */
	J9CUDA_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT,

	/** Device can map host memory into CUDA address space. */
	J9CUDA_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY,

	/** Typical clock frequency in kilohertz. */
	J9CUDA_DEVICE_ATTRIBUTE_CLOCK_RATE,

	/**
	 * Compute capability version number. This value is the major compute
	 * capability version * 10 + the minor compute capability version, so
	 * a compute capability version 3.5 function would return the value 35.
	 */
	J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY,

	/** Major compute capability version number. */
	J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR,

	/** Minor compute capability version number. */
	J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR,

	/** Compute mode (see J9CudaComputeMode for known values). */
	J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_MODE,

	/** Device can possibly execute multiple kernels concurrently. */
	J9CUDA_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS,

	/** Device has ECC support enabled. */
	J9CUDA_DEVICE_ATTRIBUTE_ECC_ENABLED,

	/** Global memory bus width in bits. */
	J9CUDA_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH,

	/** Device is integrated with host memory. */
	J9CUDA_DEVICE_ATTRIBUTE_INTEGRATED,

	/** Specifies whether there is a run time limit on kernels. */
	J9CUDA_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT,

	/** Size of L2 cache in bytes. */
	J9CUDA_DEVICE_ATTRIBUTE_L2_CACHE_SIZE,

	/** Maximum block dimension X. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X,

	/** Maximum block dimension Y. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y,

	/** Maximum block dimension Z. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z,

	/** Maximum grid dimension X. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X,

	/** Maximum grid dimension Y. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y,

	/** Maximum grid dimension Z. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z,

	/** Maximum pitch in bytes allowed by memory copies. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_PITCH,

	/** Maximum number of 32-bit registers available per block. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK,

	/** Maximum shared memory available per block in bytes. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK,

	/** Maximum number of threads per block. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK,

	/** Maximum resident threads per multiprocessor. */
	J9CUDA_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR,

	/** Maximum layers in a 1D layered surface. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_LAYERS,

	/** Maximum 1D layered surface width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_WIDTH,

	/** Maximum 1D surface width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_WIDTH,

	/** Maximum 2D surface height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_HEIGHT,

	/** Maximum 2D layered surface height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_HEIGHT,

	/** Maximum layers in a 2D layered surface. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_LAYERS,

	/** Maximum 2D layered surface width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_WIDTH,

	/** Maximum 2D surface width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_WIDTH,

	/** Maximum 3D surface depth. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_DEPTH,

	/** Maximum 3D surface height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_HEIGHT,

	/** Maximum 3D surface width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_WIDTH,

	/** Maximum layers in a cubemap layered surface. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS,

	/** Maximum cubemap layered surface width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH,

	/** Maximum cubemap surface width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_WIDTH,

	/** Maximum layers in a 1D layered texture. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS,

	/** Maximum 1D layered texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH,

	/** Maximum 1D linear texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH,

	/** Maximum mipmapped 1D texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH,

	/** Maximum 1D texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH,

	/** Maximum 2D texture height if CUDA_ARRAY3D_TEXTURE_GATHER is set. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT,

	/** Maximum 2D texture width if CUDA_ARRAY3D_TEXTURE_GATHER is set. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH,

	/** Maximum 2D texture height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT,

	/** Maximum 2D layered texture height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_HEIGHT,

	/** Maximum layers in a 2D layered texture. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_LAYERS,

	/** Maximum 2D layered texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_WIDTH,

	/** Maximum 2D linear texture height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_HEIGHT,

	/** Maximum 2D linear texture pitch in bytes. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_PITCH,

	/** Maximum 2D linear texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_WIDTH,

	/** Maximum mipmapped 2D texture height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT,

	/** Maximum mipmapped 2D texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH,

	/** Maximum 2D texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH,

	/** Maximum 3D texture depth. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH,

	/** Alternate maximum 3D texture depth. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE,

	/** Maximum 3D texture height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT,

	/** Alternate maximum 3D texture height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE,

	/** Maximum 3D texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH,

	/** Alternate maximum 3D texture width. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE,

	/** Maximum layers in a cubemap layered texture. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS,

	/** Maximum cubemap layered texture width/height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH,

	/** Maximum cubemap texture width/height. */
	J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_WIDTH,

	/** Peak memory clock frequency in kilohertz. */
	J9CUDA_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE,

	/** Number of multiprocessors on device. */
	J9CUDA_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,

	/** PCI bus ID of the device. */
	J9CUDA_DEVICE_ATTRIBUTE_PCI_BUS_ID,

	/** PCI device ID of the device. */
	J9CUDA_DEVICE_ATTRIBUTE_PCI_DEVICE_ID,

	/** PCI domain ID of the device. */
	J9CUDA_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID,

	/** Device supports stream priorities. */
	J9CUDA_DEVICE_ATTRIBUTE_STREAM_PRIORITIES_SUPPORTED,

	/** Alignment requirement for surfaces. */
	J9CUDA_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT,

	/** Device is using TCC driver model. */
	J9CUDA_DEVICE_ATTRIBUTE_TCC_DRIVER,

	/** Alignment requirement for textures. */
	J9CUDA_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT,

	/** Pitch alignment requirement for textures. */
	J9CUDA_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT,

	/** Memory available on device for __constant__ variables in a kernel in bytes. */
	J9CUDA_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY,

	/** Device shares a unified address space with the host. */
	J9CUDA_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING,

	/** Warp size in threads. */
	J9CUDA_DEVICE_ATTRIBUTE_WARP_SIZE,

	J9CUDA_DEVICE_ATTRIBUTE_DUMMY = 0x40000000 /* force wide enums */
} J9CudaDeviceAttribute;

/**
 * Codes for device limits for use with omrcuda_deviceGetLimit & omrcuda_deviceSetLimit.
 *
 * Note: literal values must not be changed without making the corresponding
 * changes to CudaDevice.Limit in CUDA4J.
 */
typedef enum J9CudaDeviceLimit {
	/** maximum number of outstanding device runtime launches that can be made from this context */
	J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT,

	/** maximum grid depth at which a thread can issue the device runtime call omrcuda_deviceSynchronize() to wait on child grid launches to complete */
	J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_SYNC_DEPTH,

	/** size in bytes of the heap used by the ::malloc() and ::free() device system calls */
	J9CUDA_DEVICE_LIMIT_MALLOC_HEAP_SIZE,

	/** size in bytes of the FIFO used by the ::printf() device system call */
	J9CUDA_DEVICE_LIMIT_PRINTF_FIFO_SIZE,

	/** stack size in bytes of each GPU thread */
	J9CUDA_DEVICE_LIMIT_STACK_SIZE,

	J9CUDA_DEVICE_LIMIT_DUMMY = 0x40000000 /* force wide enums */
} J9CudaDeviceLimit;

/**
 * Flags that can be ORed together for use with omrcuda_eventCreate.
 *
 * Note: literal values must not be changed without affecting public API of CUDA4J
 * (see CudaEvent.FLAG_*).
 */
typedef enum J9CudaEventFlags {
	/** Default event creation flag. */
	J9CUDA_EVENT_FLAG_DEFAULT = 0,

	/** Use blocking synchronization. */
	J9CUDA_EVENT_FLAG_BLOCKING_SYNC = 1,

	/** Do not record timing data. */
	J9CUDA_EVENT_FLAG_DISABLE_TIMING = 2,

	/** Event is suitable for interprocess use. FLAG_DISABLE_TIMING must be set. */
	J9CUDA_EVENT_FLAG_INTERPROCESS = 4,

	J9CUDA_EVENT_FLAG_DUMMY = 0x40000000 /* force wide enums */
} J9CudaEventFlags;

/**
 * Possible return values from omrcuda API functions.
 *
 * Note: The literal values here match those defined by CUDA (in driver_types.h)
 * to avoid requiring translations across the API boundary defined here, and
 * must not be changed without affecting public API of CUDA4J (see CudaError).
 */
typedef enum J9CudaError {
	/**
	 * The API call returned with no errors. In the case of query calls, this
	 * can also mean that the operation being queried is complete (see
	 * omrcuda_eventQuery() and omrcuda_streamQuery()).
	 */
	J9CUDA_NO_ERROR = 0,

	/**
	 * The device function being invoked (usually via ::cudaLaunch()) was not
	 * previously configured via the ::cudaConfigureCall() function.
	 * (This should never occur because omrcuda does not expose API that makes
	 * use of the (deprecated) cudaLaunch() function).
	 */
	J9CUDA_ERROR_MISSING_CONFIGURATION = 1,

	/**
	 * The API call failed because it was unable to allocate enough memory to
	 * perform the requested operation.
	 */
	J9CUDA_ERROR_MEMORY_ALLOCATION = 2,

	/**
	 * The API call failed because the CUDA driver and runtime could not be
	 * initialized.
	 */
	J9CUDA_ERROR_INITIALIZATION_ERROR = 3,

	/**
	 * An exception occurred on the device while executing a kernel. Common
	 * causes include dereferencing an invalid device pointer and accessing
	 * out of bounds shared memory. The device cannot be used until
	 * omrcuda_deviceReset() is called. All existing device memory allocations
	 * are invalid and must be reconstructed if the program is to continue
	 * using CUDA.
	 */
	J9CUDA_ERROR_LAUNCH_FAILURE = 4,

	/**
	 * This indicates that the device kernel took too long to execute. This can
	 * only occur if timeouts are enabled (J9CUDA_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT).
	 * The device cannot be used until omrcuda_deviceReset() is called. All existing
	 * device memory allocations are invalid and must be reconstructed if the
	 * program is to continue using CUDA.
	 */
	J9CUDA_ERROR_LAUNCH_TIMEOUT = 6,

	/**
	 * This indicates that a launch did not occur because it did not have
	 * appropriate resources. Although this error is similar to
	 * J9CUDA_ERROR_INVALID_CONFIGURATION, this error usually indicates that the
	 * user has attempted to pass too many arguments to the device kernel, or the
	 * kernel launch specifies too many threads for the kernel's register count.
	 */
	J9CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES = 7,

	/**
	 * The requested device function does not exist or is not compiled for the
	 * proper device architecture.
	 */
	J9CUDA_ERROR_INVALID_DEVICE_FUNCTION = 8,

	/**
	 * This indicates that a kernel launch is requesting resources that can
	 * never be satisfied by the current device. Requesting more shared memory
	 * per block than the device supports will trigger this error, as will
	 * requesting too many threads or blocks. Use omrcuda_deviceGetAttribute()
	 * to query device limitations.
	 */
	J9CUDA_ERROR_INVALID_CONFIGURATION = 9,

	/**
	 * This indicates that the device ordinal supplied by the user does not
	 * correspond to a valid CUDA device.
	 */
	J9CUDA_ERROR_INVALID_DEVICE = 10,

	/**
	 * This indicates that one or more of the parameters passed to the API call
	 * is not within an acceptable range of values.
	 */
	J9CUDA_ERROR_INVALID_VALUE = 11,

	/**
	 * This indicates that one or more of the pitch-related parameters passed
	 * to the API call is not within the acceptable range for pitch.
	 */
	J9CUDA_ERROR_INVALID_PITCH_VALUE = 12,

	/**
	 * This indicates that the symbol name/identifier passed to the API call
	 * is not a valid name or identifier.
	 */
	J9CUDA_ERROR_INVALID_SYMBOL = 13,

	/**
	 * This indicates that the buffer object could not be mapped.
	 */
	J9CUDA_ERROR_MAP_BUFFER_OBJECT_FAILED = 14,

	/**
	 * This indicates that the buffer object could not be unmapped.
	 */
	J9CUDA_ERROR_UNMAP_BUFFER_OBJECT_FAILED = 15,

	/**
	 * This indicates that at least one host pointer passed to the API call is
	 * not a valid host pointer.
	 */
	J9CUDA_ERROR_INVALID_HOST_POINTER = 16,

	/**
	 * This indicates that at least one device pointer passed to the API call is
	 * not a valid device pointer.
	 */
	J9CUDA_ERROR_INVALID_DEVICE_POINTER = 17,

	/**
	 * This indicates that the texture passed to the API call is not a valid
	 * texture.
	 */
	J9CUDA_ERROR_INVALID_TEXTURE = 18,

	/**
	 * This indicates that the texture binding is not valid. This occurs if you
	 * call ::cudaGetTextureAlignmentOffset() with an unbound texture.
	 */
	J9CUDA_ERROR_INVALID_TEXTURE_BINDING = 19,

	/**
	 * This indicates that the channel descriptor passed to the API call is not
	 * valid. This occurs if the format is not one of the formats specified by
	 * ::cudaChannelFormatKind, or if one of the dimensions is invalid.
	 */
	J9CUDA_ERROR_INVALID_CHANNEL_DESCRIPTOR = 20,

	/**
	 * This indicates that the direction of the memcpy passed to the API call is
	 * invalid.
	 */
	J9CUDA_ERROR_INVALID_MEMCPY_DIRECTION = 21,

	/**
	 * This indicates that a non-float texture was being accessed with linear
	 * filtering. This is not supported by CUDA.
	 */
	J9CUDA_ERROR_INVALID_FILTER_SETTING = 26,

	/**
	 * This indicates that an attempt was made to read a non-float texture as a
	 * normalized float. This is not supported by CUDA.
	 */
	J9CUDA_ERROR_INVALID_NORM_SETTING = 27,

	/**
	 * This indicates that a CUDA Runtime API call cannot be executed because
	 * it is being called during process shut down, at a point in time after
	 * CUDA driver has been unloaded.
	 */
	J9CUDA_ERROR_CUDART_UNLOADING = 29,

	/**
	 * This indicates that an unknown internal error has occurred.
	 */
	J9CUDA_ERROR_UNKNOWN = 30,

	/**
	 * This indicates that a resource handle passed to the API call was
	 * not valid. Resource handles are opaque types like J9CudaEvent and
	 * J9CudaStream.
	 */
	J9CUDA_ERROR_INVALID_RESOURCE_HANDLE = 33,

	/**
	 * This indicates that asynchronous operations issued previously have not
	 * completed yet. This result is not actually an error, but must be indicated
	 * differently than J9CUDA_NO_ERROR (which indicates completion). Calls that
	 * may return this value include omrcuda_eventQuery() and omrcuda_streamQuery().
	 */
	J9CUDA_ERROR_NOT_READY = 34,

	/**
	 * This indicates that the installed NVIDIA CUDA driver is older than the
	 * CUDA runtime library. This is not a supported configuration. Users should
	 * install an updated NVIDIA display driver to allow the application to run.
	 */
	J9CUDA_ERROR_INSUFFICIENT_DRIVER = 35,

	/**
	 * This indicates that the user has called ::cudaSetValidDevices(),
	 * ::cudaSetDeviceFlags(), ::cudaD3D9SetDirect3DDevice(),
	 * ::cudaD3D10SetDirect3DDevice, ::cudaD3D11SetDirect3DDevice(), or
	 * ::cudaVDPAUSetVDPAUDevice() after initializing the CUDA runtime by
	 * calling non-device management operations (allocating memory and
	 * launching kernels are examples of non-device management operations).
	 * This error can also be returned if using runtime/driver
	 * interoperability and there is an existing ::CUcontext active on the
	 * host thread.
	 */
	J9CUDA_ERROR_SET_ON_ACTIVE_PROCESS = 36,

	/**
	 * This indicates that the surface passed to the API call is not a valid
	 * surface.
	 */
	J9CUDA_ERROR_INVALID_SURFACE = 37,

	/**
	 * This indicates that no CUDA-capable devices were detected by the installed
	 * CUDA driver.
	 */
	J9CUDA_ERROR_NO_DEVICE = 38,

	/**
	 * This indicates that an uncorrectable ECC error was detected during
	 * execution.
	 */
	J9CUDA_ERROR_ECCUNCORRECTABLE = 39,

	/**
	 * This indicates that a link to a shared object failed to resolve.
	 */
	J9CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND = 40,

	/**
	 * This indicates that initialization of a shared object failed.
	 */
	J9CUDA_ERROR_SHARED_OBJECT_INIT_FAILED = 41,

	/**
	 * This indicates that the limit passed to omrcuda_deviceGetLimit() or
	 * omrcuda_deviceSetLimit() is not supported by the active device.
	 */
	J9CUDA_ERROR_UNSUPPORTED_LIMIT = 42,

	/**
	 * This indicates that multiple global or constant variables (across
	 * separate CUDA source files in the application) share the same string
	 * name.
	 */
	J9CUDA_ERROR_DUPLICATE_VARIABLE_NAME = 43,

	/**
	 * This indicates that multiple textures (across separate CUDA source
	 * files in the application) share the same string name.
	 */
	J9CUDA_ERROR_DUPLICATE_TEXTURE_NAME = 44,

	/**
	 * This indicates that multiple surfaces (across separate CUDA source
	 * files in the application) share the same string name.
	 */
	J9CUDA_ERROR_DUPLICATE_SURFACE_NAME = 45,

	/**
	 * This indicates that all CUDA devices are busy or unavailable at the
	 * current time. Devices are often busy/unavailable due to use of
	 * J9CUDA_COMPUTE_MODE_PROCESS_EXCLUSIVE, J9CUDA_COMPUTE_MODE_PROHIBITED
	 * or when long running CUDA kernels have filled up the GPU and are blocking
	 * new work from starting. They can also be unavailable due to memory
	 * constraints on a device that already has active CUDA work being performed.
	 */
	J9CUDA_ERROR_DEVICES_UNAVAILABLE = 46,

	/**
	 * This indicates that the device kernel image is invalid.
	 */
	J9CUDA_ERROR_INVALID_KERNEL_IMAGE = 47,

	/**
	 * This indicates that there is no kernel image available that is suitable
	 * for the device. This can occur when a user specifies code generation
	 * options for a particular CUDA source file that do not include the
	 * corresponding device configuration.
	 */
	J9CUDA_ERROR_NO_KERNEL_IMAGE_FOR_DEVICE = 48,

	/**
	 * This indicates that the current context is not compatible with this
	 * the CUDA Runtime. This can only occur if you are using CUDA
	 * runtime/driver interoperability and have created an existing driver
	 * context using the driver API. The driver context may be incompatible
	 * either because the driver context was created using an older version
	 * of the API, because the runtime API call expects a primary driver
	 * context and the driver context is not primary, or because the driver
	 * context has been destroyed. Please see \ref CUDART_DRIVER "Interactions
	 * with the CUDA Driver API" for more information.
	 */
	J9CUDA_ERROR_INCOMPATIBLE_DRIVER_CONTEXT = 49,

	/**
	 * This error indicates that a call to omrcuda_deviceEnablePeerAccess() is
	 * trying to re-enable peer addressing on from a context which has already
	 * had peer addressing enabled.
	 */
	J9CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED = 50,

	/**
	 * This error indicates that omrcuda_deviceDisablePeerAccess() is trying to
	 * disable peer addressing which has not been enabled yet via
	 * omrcuda_deviceEnablePeerAccess().
	 */
	J9CUDA_ERROR_PEER_ACCESS_NOT_ENABLED = 51,

	/**
	 * This indicates that a call tried to access an thread-exclusive device that
	 * is already in use by a different thread.
	 */
	J9CUDA_ERROR_DEVICE_ALREADY_IN_USE = 54,

	/**
	 * This indicates profiler is not initialized for this run. This can
	 * happen when the application is running with external profiling tools
	 * like visual profiler.
	 */
	J9CUDA_ERROR_PROFILER_DISABLED = 55,

	/**
	 * An assert triggered in device code during kernel execution. The device
	 * cannot be used again until omrcuda_deviceReset() is called. All existing
	 * allocations are invalid and must be reconstructed if the program is to
	 * continue using CUDA.
	 */
	J9CUDA_ERROR_ASSERT = 59,

	/**
	 * This error indicates that the hardware resources required to enable
	 * peer access have been exhausted for one or more of the devices
	 * passed to omrcuda_deviceEnablePeerAccess().
	 */
	J9CUDA_ERROR_TOO_MANY_PEERS = 60,

	/**
	 * This error indicates that the memory range passed to ::cudaHostRegister()
	 * has already been registered.
	 */
	J9CUDA_ERROR_HOST_MEMORY_ALREADY_REGISTERED = 61,

	/**
	 * This error indicates that the pointer passed to ::cudaHostUnregister()
	 * does not correspond to any currently registered memory region.
	 */
	J9CUDA_ERROR_HOST_MEMORY_NOT_REGISTERED = 62,

	/**
	 * This error indicates that an OS call failed.
	 */
	J9CUDA_ERROR_OPERATING_SYSTEM = 63,

	/**
	 * This error indicates that P2P access is not supported across the given
	 * devices.
	 */
	J9CUDA_ERROR_PEER_ACCESS_UNSUPPORTED = 64,

	/**
	 * This error indicates that a device runtime grid launch did not occur
	 * because the depth of the child grid would exceed the maximum supported
	 * number of nested grid launches.
	 */
	J9CUDA_ERROR_LAUNCH_MAX_DEPTH_EXCEEDED = 65,

	/**
	 * This error indicates that a grid launch did not occur because the kernel
	 * uses file-scoped textures which are unsupported by the device runtime.
	 * Kernels launched via the device runtime only support textures created with
	 * the Texture Object APIs.
	 */
	J9CUDA_ERROR_LAUNCH_FILE_SCOPED_TEX = 66,

	/**
	 * This error indicates that a grid launch did not occur because the kernel
	 * uses file-scoped surfaces which are unsupported by the device runtime.
	 * Kernels launched via the device runtime only support surfaces created with
	 * the Surface Object APIs.
	 */
	J9CUDA_ERROR_LAUNCH_FILE_SCOPED_SURF = 67,

	/**
	 * This error indicates that a call to omrcuda_deviceSynchronize() made from
	 * the device runtime failed because the call was made at grid depth greater
	 * than than either the default (2 levels of grids) or user specified device
	 * limit J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_SYNC_DEPTH. To be able to synchronize
	 * on launched grids at a greater depth successfully, the maximum nested
	 * depth at which omrcuda_deviceSynchronize() will be called must be specified
	 * with the J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_SYNC_DEPTH limit to the
	 * omrcuda_deviceSetLimit() API before the host-side launch of a kernel using
	 * the device runtime. Keep in mind that additional levels of sync depth require
	 * the runtime to reserve large amounts of device memory that cannot be used for
	 * user allocations.
	 */
	J9CUDA_ERROR_SYNC_DEPTH_EXCEEDED = 68,

	/**
	 * This error indicates that a device runtime grid launch failed because the
	 * launch would exceed the limit J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT.
	 * For this launch to proceed successfully, omrcuda_deviceSetLimit() must be
	 * called to set the J9CUDA_DEVICE_LIMIT_DEV_RUNTIME_PENDING_LAUNCH_COUNT to
	 * be higher than the upper bound of outstanding launches that can be issued
	 * to the device runtime. Keep in mind that raising the limit of pending device
	 * runtime launches will require the runtime to reserve device memory that
	 * cannot be used for user allocations.
	 */
	J9CUDA_ERROR_LAUNCH_PENDING_COUNT_EXCEEDED = 69,

	/**
	 * This error indicates the attempted operation is not permitted.
	 */
	J9CUDA_ERROR_NOT_PERMITTED = 70,

	/**
	 * This error indicates the attempted operation is not supported
	 * on the current system or device.
	 */
	J9CUDA_ERROR_NOT_SUPPORTED = 71,

	/**
	 * This indicates an internal startup failure in the CUDA runtime.
	 */
	J9CUDA_ERROR_STARTUP_FAILURE = 0x7f,

	/**
	 * This indicates that a named symbol (function, global, surface
	 * or texture) was not found.
	 */
	J9CUDA_ERROR_NOT_FOUND = -500,

	J9CUDA_ERROR_DUMMY = 0x40000000 /* force wide enums */
} J9CudaError;

/**
 * An opaque handle for an event.
 */
struct J9CudaEventState;
typedef struct J9CudaEventState * J9CudaEvent;

/**
 * An opaque handle for a function.
 */
struct J9CudaFunctionState;
typedef struct J9CudaFunctionState * J9CudaFunction;

/**
 * Kernel function attributes codes for use with omrcuda_funcGetAttribute.
 *
 * Note: literal values must not be changed without affecting public API of CUDA4J
 * (see CudaFunction.ATTRIBUTE_*).
 */
typedef enum J9CudaFunctionAttribute {
	/**
	 * The binary architecture version for which the function was compiled.
	 * This value is the major binary version * 10 + the minor binary version,
	 * so a binary version 1.3 function would return the value 13. Note that
	 * this will return a value of 10 for legacy cubins that do not have a
	 * properly-encoded binary architecture version.
	 */
	J9CUDA_FUNCTION_ATTRIBUTE_BINARY_VERSION = 6,

	/**
	 * The size in bytes of user-allocated constant memory required by this
	 * function.
	 */
	J9CUDA_FUNCTION_ATTRIBUTE_CONST_SIZE_BYTES = 2,

	/**
	 * The size in bytes of local memory used by each thread of this function.
	 */
	J9CUDA_FUNCTION_ATTRIBUTE_LOCAL_SIZE_BYTES = 3,

	/**
	 * The maximum number of threads per block, beyond which a launch of the
	 * function would fail. This number depends on both the function and the
	 * device on which the function is currently loaded.
	 */
	J9CUDA_FUNCTION_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0,

	/**
	 * The number of registers used by each thread of this function.
	 */
	J9CUDA_FUNCTION_ATTRIBUTE_NUM_REGS = 4,

	/**
	 * The PTX virtual architecture version for which the function was
	 * compiled. This value is the major PTX version * 10 + the minor PTX
	 * version, so a PTX version 1.3 function would return the value 13.
	 * Note that this may return the undefined value of 0 for cubins
	 * compiled prior to CUDA 3.0.
	 */
	J9CUDA_FUNCTION_ATTRIBUTE_PTX_VERSION = 5,

	/**
	 * The size in bytes of statically-allocated shared memory required by
	 * this function. This does not include dynamically-allocated shared
	 * memory requested by the user at runtime.
	 */
	J9CUDA_FUNCTION_ATTRIBUTE_SHARED_SIZE_BYTES = 1,

	J9CUDA_FUNCTION_ATTRIBUTE_DUMMY = 0x40000000 /* force wide enums */
} J9CudaFunctionAttribute;

/**
 * Values that may be ORed together to produce valid flags for omrcuda_hostAlloc.
 */
typedef enum J9CudaHostAllocFlags {
	/** Default page-locked allocation flag. */
	J9CUDA_HOST_ALLOC_DEFAULT = 0,
	/** Map allocation into device space. */
	J9CUDA_HOST_ALLOC_MAPPED = 1,
	/** Pinned memory accessible by all CUDA contexts. */
	J9CUDA_HOST_ALLOC_PORTABLE = 2,
	/** Write-combined memory. */
	J9CUDA_HOST_ALLOC_WRITE_COMBINED = 4,

	J9CUDA_HOST_ALLOC_DUMMY = 0x40000000 /* force wide enums */
} J9CudaHostAllocFlags;

/**
 * Cache management choices.
 *
 * Note: literal values must not be changed without making the corresponding
 * changes to CudaJitOptions.CacheMode in CUDA4J.
 */
typedef enum J9CudaJitCacheMode {
	/** Cache mode not specified. */
	J9CUDA_JIT_CACHE_MODE_UNSPECIFIED,
	/** Compile with no -dlcm flag specified. */
	J9CUDA_JIT_CACHE_MODE_DEFAULT,
	/** Compile with L1 cache disabled. */
	J9CUDA_JIT_CACHE_MODE_L1_DISABLED,
	/** Compile with L1 cache enabled. */
	J9CUDA_JIT_CACHE_MODE_L1_ENABLED,

	J9CUDA_JIT_CACHE_MODE_DUMMY = 0x40000000 /* force wide enums */
} J9CudaJitCacheMode;

/**
 * Available fall-back strategies when an exactly matching object is not available.
 *
 * Note: literal values must not be changed without making the corresponding
 * changes to CudaJitOptions.Fallback in CUDA4J.
 */
typedef enum J9CudaJitFallbackStrategy {
	/** Fallback strategy not specified. */
	J9CUDA_JIT_FALLBACK_STRATEGY_UNSPECIFIED,
	/** Prefer to fall back to compatible binary code if exact match not found. */
	J9CUDA_JIT_FALLBACK_STRATEGY_PREFER_BINARY,
	/** Prefer to compile ptx if exact binary match not found. */
	J9CUDA_JIT_FALLBACK_STRATEGY_PREFER_PTX,

	J9CUDA_JIT_FALLBACK_STRATEGY_DUMMY = 0x40000000 /* force wide enums */
} J9CudaJitFallbackStrategy;

/**
 * Generic JIT flag values.
 */
typedef enum J9CudaJitFlag {
	/** Flag not specified. */
	J9CUDA_JIT_FLAG_UNSPECIFIED,
	/** Flag disabled. */
	J9CUDA_JIT_FLAG_DISABLED,
	/** Flag enabled. */
	J9CUDA_JIT_FLAG_ENABLED,

	J9CUDA_JIT_FLAG_DUMMY = 0x40000000 /* force wide enums */
} J9CudaJitFlag;

/**
 * Identifies the type of input being provided to the linker.
 *
 * Note: literal values must not be changed without making the corresponding
 * changes to CudaJitInputType in CUDA4J.
 */
typedef enum J9CudaJitInputType {
	/** Compiled device-class-specific device code. */
	J9CUDA_JIT_INPUT_TYPE_CUBIN,
	/** Bundle of multiple cubins and/or PTX of some device code. */
	J9CUDA_JIT_INPUT_TYPE_FATBINARY,
	/** Archive of host objects with embedded device code. */
	J9CUDA_JIT_INPUT_TYPE_LIBRARY,
	/** Host object with embedded device code. */
	J9CUDA_JIT_INPUT_TYPE_OBJECT,
	/** PTX source code. */
	J9CUDA_JIT_INPUT_TYPE_PTX,

	J9CUDA_JIT_INPUT_TYPE_DUMMY = 0x40000000 /* force wide enums */
} J9CudaJitInputType;

/**
 * Optimization levels (higher levels imply more optimization).
 */
typedef enum J9CudaJitOptimizationLevel {
	/** Optimization level not specified. */
	J9CUDA_JIT_OPTIMIZATION_LEVEL_UNSPECIFIED,
	/** Optimization level 0. */
	J9CUDA_JIT_OPTIMIZATION_LEVEL_0,
	/** Optimization level 1. */
	J9CUDA_JIT_OPTIMIZATION_LEVEL_1,
	/** Optimization level 2. */
	J9CUDA_JIT_OPTIMIZATION_LEVEL_2,
	/** Optimization level 3. */
	J9CUDA_JIT_OPTIMIZATION_LEVEL_3,
	/** Optimization level 4. */
	J9CUDA_JIT_OPTIMIZATION_LEVEL_4,

	J9CUDA_JIT_OPTIMIZATION_LEVEL_DUMMY = 0x40000000 /* force wide enums */
} J9CudaJitOptimizationLevel;

/**
 * For specifying options to the CUDA compiler and linker.
 * An instance should be filled with zeros before setting
 * any desired options.
 */
typedef struct J9CudaJitOptions {
	/**
	 * If non-null, the compiler or linker may write error messages of up to
	 * errorLogBufferSize bytes in length (including a terminating NUL).
	 */
	char *errorLogBuffer;

	/**
	 * The size in bytes of errorLogBuffer. If this is non-zero, errorLogBuffer
	 * may not be null.
	 */
	uintptr_t errorLogBufferSize;

	/**
	 * If non-null, the compiler or linker may write informational messages of
	 * up to infoLogBufferSize bytes in length (including a terminating NUL).
	 */
	char *infoLogBuffer;

	/**
	 * The size in bytes of infoLogBuffer. If this is non-zero, infoLogBuffer
	 * may not be null.
	 */
	uintptr_t infoLogBufferSize;

	/**
	 * Controls whether the compiler should enable caching explicitly.
	 */
	J9CudaJitCacheMode cacheMode;

	/**
	 * Selects a fall-back strategy for the compiler when an exactly matching
	 * object is not available.
	 */
	J9CudaJitFallbackStrategy fallbackStrategy;

	/**
	 * Specifies whether the compiler or linker should generate debug information.
	 */
	J9CudaJitFlag generateDebugInfo;

	/**
	 * Specifies whether the compiler should generate line number information.
	 */
	J9CudaJitFlag generateLineInfo;

	/**
	 * Specifies the level of optimization the compiler should use.
	 */
	J9CudaJitOptimizationLevel optimizationLevel;

	/**
	 * Should the target be determined from the applicable device?
	 */
	J9CudaJitFlag targetFromContext;

	/**
	 * Should the compiler or linker record the elapsed time?
	 */
	J9CudaJitFlag recordWallTime;

	/**
	 * Specifies whether compiler or linker should generate verbose log messages.
	 */
	J9CudaJitFlag verboseLogging;

	/**
	 * The maximum number of registers that the compiler may permit a thread to use.
	 * If set to zero, no limit will be imposed.
	 */
	uint32_t maxRegisters;

	/**
	 * The target compute capability expressed as ((10 * major) + minor).
	 * E.g. The value 35 means compute capability 3.5.
	 */
	uint32_t target;

	/**
	 * Limits resource usage of the compiler so that thread blocks of the specified
	 * size (or larger) may be actually be launched. If set to zero, no limit will
	 * be imposed. If set to a non-zero value, this field will be updated to reflect
	 * the number of threads the compiler actually targeted.
	 */
	uint32_t threadsPerBlock;

	/**
	 * If recordWallTime is enabled, this field will be updated to reflect the
	 * elapsed time in milliseconds taken by the compiler or linker.
	 */
	float wallTime;
} J9CudaJitOptions;

/**
 * An opaque handle for a linker.
 */
struct J9CudaLinkerState;
typedef struct J9CudaLinkerState *J9CudaLinker;

/**
 * An opaque handle for a module.
 */
struct J9CudaModuleState;
typedef struct J9CudaModuleState *J9CudaModule;

/**
 * Flags for use with omrcuda_funcMaxActiveBlocksPerMultiprocessor
 * or omrcuda_funcMaxPotentialBlockSize.
 */
typedef enum J9CudaOccupancyFlags {
	/** default behavior */
	J9CUDA_OCCUPANCY_DEFAULT = 0,

	/** assume global caching is enabled and cannot be automatically disabled */
	J9CUDA_OCCUPANCY_DISABLE_CACHING_OVERRIDE = 1,

	J9CUDA_OCCUPANCY_DUMMY = 0x40000000 /* force wide enums */
} J9CudaOccupancyFlags;

/**
 * Bank size options for shared memory.
 *
 * Note: literal values must not be changed without making the corresponding
 * changes to CudaDevice.SharedMemConfig in CUDA4J.
 */
typedef enum J9CudaSharedMemConfig {
	/** default shared memory bank size */
	J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE,
	/** four byte shared memory bank width */
	J9CUDA_SHARED_MEM_CONFIG_4_BYTE_BANK_SIZE,
	/** eight byte shared memory bank width */
	J9CUDA_SHARED_MEM_CONFIG_8_BYTE_BANK_SIZE,

	J9CUDA_SHARED_MEM_CONFIG_DUMMY = 0x40000000 /* force wide enums */
} J9CudaSharedMemConfig;

/**
 * Flags that can be ORed together for use with omrcuda_streamCreateWithPriority
 * or returned by omrcuda_streamGetFlags.
 *
 * Note: literal values must not be changed without affecting public API of CUDA4J
 * (see CudaStream.FLAG_*).
 */
typedef enum J9CudaStreamFlags {
	/** Default stream creation flag. */
	J9CUDA_STREAM_FLAG_DEFAULT = 0,

	/** Work in the stream may run concurrently with the default stream. */
	J9CUDA_STREAM_FLAG_NON_BLOCKING = 1,

	J9CUDA_STREAM_FLAG_DUMMY = 0x40000000 /* force wide enums */
} J9CudaStreamFlags;

/**
 * An opaque handle for a stream.
 */
struct J9CudaStreamState;
typedef struct J9CudaStreamData *J9CudaStream;

/**
 * A pointer to a function to be invoked after completing precediing
 * activities on a stream or the default stream of a device.
 */
typedef void (* J9CudaStreamCallback)(J9CudaStream stream, int32_t error, uintptr_t userData);

/**
 * A summary description of the CUDA runtime environment.
 */
typedef struct J9CudaSummaryDescriptor {
	/** CUDA driver version encoded as (10 * major + minor) */
	uint32_t driverVersion;
	/** CUDA runtime version encoded as (10 * major + minor) */
	uint32_t runtimeVersion;
	/** number of visible CUDA devices */
	uint32_t deviceCount;
} J9CudaSummaryDescriptor;

struct OMRPortLibrary; /* forward struct declaration */

/**
 * Accessor functions to CUDA environment descriptions.
 */
typedef struct J9CudaConfig {
	int32_t (* getSummaryData)(struct OMRPortLibrary *portLibrary, J9CudaSummaryDescriptor *summaryData);
	int32_t (* getDeviceData)(struct OMRPortLibrary *portLibrary, uint32_t deviceId, J9CudaDeviceDescriptor *deviceData);
} J9CudaConfig;

#if defined(__cplusplus)
} /* extern "C" */
#endif /* __cplusplus */

#endif /* J9CUDA_H_ */
