/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

/**
 * @file
 * @ingroup Port
 * @brief Virtual memory
 */
#include <string.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"

/* This file is used for documentation purposes only */

/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrvmem_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
omrvmem_shutdown(struct OMRPortLibrary *portLibrary)
{
}
/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the virtual memory operations may be created here.  All resources created here should be destroyed
 * in @ref omrvmem_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_VMEM
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrvmem_startup(struct OMRPortLibrary *portLibrary)
{
	return OMRPORT_ERROR_STARTUP_VMEM;
}
/**
 * Commit memory in virtual address space.
 *
 * @param[in] portLibrary The port library.
 * @param[in] address The page aligned starting address of the memory to commit.
 * @param[in] byteAmount The number of bytes to commit. Must be an exact multiple of page size.
 * @param[in] identifier Descriptor for virtual memory block.
 *
 * @return pointer to the allocated memory on success, NULL on failure.
 */
void *
omrvmem_commit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	return NULL;
}

/**
 * Decommit memory in virtual address space.
 *
 * Decommits physical storage of the size specified starting at the address specified, checking first if port library global
 * vmemAdviseOSonFree is set, based on -XX:+DisclaimVirtualMemory and -XX:-DisclaimVirtualMemory command line options.
 * If vmemAdviseOSonFree is not set, then no memory will be disclaimed.
 *
 * @param[in] portLibrary The port library.
 * @param[in] address The starting address of the memory to be decommitted. Must be page aligned.
 * @param[in] byteAmount The number of bytes to be decommitted. Must be an exact multiple of page size.
 * @param[in] identifier Descriptor for virtual memory block.
 *
 * @return 0 on success, non zero on failure.
 */
intptr_t
omrvmem_decommit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	return OMRPORT_ERROR_VMEM_OPFAILED;
}

/**
 * Free memory in virtual address space.
 *
 * Frees physical storage of the size specified starting at the address specified.
 *
 * @param[in] portLibrary The port library.
 * @param[in] address The starting address of the memory to be de-allocated.
 * @param[in] byteAmount The number of bytes to be allocated.
 * @param[in] identifier Descriptor for virtual memory block.
 *
 * @return 0 on success, non zero on failure.
 */
int32_t
omrvmem_free_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	return OMRPORT_ERROR_VMEM_OPFAILED;
}

/**
 * Initialize vmemParams
 *
 * Initializes all the parameters of the J9PortVMemParams to their default values.
 * Defaults are: 	startAddress = 0
 *			endAddress = OMRPORT_VMEM_MAX_ADDRESS
 *			byteAmount = 0
 * 			pageSize = omrvmem_supported_page_sizes()[0]
 *			mode = OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE
 *			options = 0
 *
 * @param[in] portLibrary The port library.
 * @param[out] J9PortVMemParams Struct containing necessary information about requested memory
 *					 It is the responsibility of the user to create the storage for
 *					 this struct
 *
 * @return	0, if no errors occurred, otherwise the (negative) error code.
 */
int32_t
omrvmem_vmem_params_init(struct OMRPortLibrary *portLibrary, struct J9PortVmemParams *params)
{
	return OMRPORT_ERROR_VMEM_OPFAILED;
}

/**
 * DEPRECATED - Use omrvmem_reserve_memory_ex instead
 *
 * Reserve memory in virtual address space.
 *
 * Reserves a range of  virtual address space without allocating any actual physical storage.
 * The memory is not available for use until committed @ref omrvmem_commit_memory.
 * The memory may not be used by other memory allocation routines until it is explicitly released.
 *
 * @param[in] portLibrary The port library.
 * @param[in] address The starting address of the memory to be reserved. Requesting memory at a specific address is only supported if the build flag OMR_PORT_CAPABILITY_CAN_RESERVE_SPECIFIC_ADDRESS is set. Address is ignored if this capability is not supported.
 * @param[in] byteAmount The number of bytes to be reserved.
 * @param[in] identifier Descriptor for virtual memory block.
 * @param[in] mode Bitmap indicating how memory is to be reserved.  Expected values combination of:
 * \arg OMRPORT_VMEM_MEMORY_MODE_READ memory is readable
 * \arg OMRPORT_VMEM_MEMORY_MODE_WRITE memory is writable
 * \arg OMRPORT_VMEM_MEMORY_MODE_EXECUTE memory is executable
 * \arg OMRPORT_VMEM_MEMORY_MODE_COMMIT commits memory as part of the reserve
 * @param[in] pageSize Size of the page requested, a value returned by @ref omrvmem_supported_page_sizes
 * @param[in] category Memory allocation category code
 *
 * @return pointer to the reserved memory on success, NULL on failure.
 *
 * @internal @warning Do not call error handling code @ref omrerror upon error as
 * the error handling code uses per thread buffers to store the last error.  If memory
 * can not be allocated the result would be an infinite loop.
 */
void *
omrvmem_reserve_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, uint32_t category)
{
	return NULL;
}


/**
 * Reserve memory
 *
 * Reserves memory as specified by J9PortVMemParams without allocating any actual physical storage.
 * The memory is not available for use until committed @ref omrvmem_commit_memory.
 * The memory may not be used by other memory allocation routines until it is explicitly released.
 *
 * @param[in] portLibrary The port library.
 * @param[out] identifier Descriptor for virtual memory block.
 * @param[in] J9PortVMemParams Struct containing necessary information about requested memory
 *					 It is the responsibility of the user to create the storage for this struct
 *					 This structure must be initialized using @ref omrvmem_vmem_params_init
 * 					 This structure may be discarded after this function returns
 *
 * @return pointer to the reserved memory on success, NULL on failure.
 */
void *
omrvmem_reserve_memory_ex(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, struct J9PortVmemParams *params)
{
	return NULL;
}

/**
 * Get the page size used to back a region of virtual memory.
 *
 * @param[in] portLibrary The port library.
 * @param[in] identifier Descriptor for virtual memory block.
 *
 * @return The page size in bytes used to back the virtual memory region.
 */
uintptr_t
omrvmem_get_page_size(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier)
{
	return 0;
}

/**
 * Get the flags describing type of page used to back a region of virtual memory.
 *
 * @param[in] portLibrary The port library.
 * @param[in] identifier Descriptor for virtual memory block.
 *
 * @return flags dscribing type of page used to back the virtual memory region.
 *
 */
uintptr_t
omrvmem_get_page_flags(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier)
{
	return 0;
}

/**
 * Determine the page sizes supported.
 *
 * @param[in] portLibrary The port library.
 *
 * @return A 0 terminated array of supported page sizes in bytes.  The first entry is the default page size, other entries
 * are the large page sizes supported.
 */
uintptr_t *
omrvmem_supported_page_sizes(struct OMRPortLibrary *portLibrary)
{
	return NULL;
}

/*
 * Determine the flags describing the page type corresponding to the supported page sizes.
 *
 * @param[in] portLibrary The port library.
 *
 * @return A 0 terminated array of flags describing the page types corresponding to supported page sizes.
 */
uintptr_t *
omrvmem_supported_page_flags(struct OMRPortLibrary *portLibrary)
{
	return NULL;
}

/**
 * The default large page size and corresponding page flags used when large pages are enabled but no specific page size is requested
 *
 * @param[in] portLibrary The port library.
 * @param[in] mode Used only on 64-bit z/OS, ignored on other platforms. Valid value is OMRPORT_VMEM_MEMORY_MODE_EXECUTE or 0.
 * @param[out] pageSize  Pointer to store default large page size.
 * @param[out] pageFlags  Pointer to store flags for default large page.
 *
 * @return void
 */
void
omrvmem_default_large_page_size_ex(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags)
{
	return;
}

/**
 * Used to get a valid page size and corresponding page flags to be used by callers for making allocation request.
 * Callers need to pass in pageSize and pageFlags as hints for the valid page size.
 * If portlibrary decides that caller provided page size and page flags combination is supported, it returns the same,
 * otherwise, it may return different page size and flags according to platform-specific rules.
 *
 * If mode is set to OMRPORT_VMEM_MEMORY_MODE_EXECUTE, page size to be returned is for executable pages.
 * This flag is required on platforms where the rules for getting
 * valid page size are different for executable and non-executable pages
 * (as is the case with z/OS).
 *
 * In cases where availability of default large page size is a pre-requisite
 * to use large pages (eg -Xlp<size> options), caller of this API is responsible
 * to check default large page size and should call this API only if it is supported.

 * @param[in] portLibrary The port library.
 * @param[in] mode Used only z/OS, ignored on other platforms. Valid value is OMRPORT_VMEM_MEMORY_MODE_EXECUTE or 0.
 * @param[in/out] pageSize  As input, stores hint provided by caller to decide initial page size.
 * 							On return, stores the page size to be used by the caller for making allocation requests.
 * @param[in/out] pageFlags As input, stores page flags corresponding to the pageSize hint provided by the caller.
 *  						On return, stores page flags corresponding to value returned in pageSize.
 * @param[out] isSizeSupported Set to true if the requested page size/flags is supported, false otherwise
 *
 * @return 0 on success, does not fail.
 */
intptr_t
omrvmem_find_valid_page_size(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags, BOOLEAN *isSizeSupported)
{
	return 0;
}

/**
* Associate memory in the virtual address space with the specified NUMA node.
*
* The memory must have already been reserved (using omrvmem_reserve_memory or omrvmem_reserve_memory_ex),
* but must not have been committed. This function does not commit the memory.
*
* When memory in the range is committed, the OS will make a best-effort to associate it with the desired
* node. The association is permanent if the memory is decommitted and subsequently recommitted. Calling
* omrvmem_numa_set_affinity() twice on the same range (or on overlapping ranges) will result in undefined
* affinity.
*
* @param portLibrary The port library.
* @param numaNode The identifier of the NUMA node to associate the memory with (indexed from 1)
* @param address The page aligned starting address of the memory to associate with the NUMA node.
* @param byteAmount The number of bytes to associate with the NUMA node (must be a multiple of page size)
* @param identifier Descriptor for virtual memory block.
*
* @return	0, if no errors occurred, otherwise the (negative) error code.
*/
intptr_t
omrvmem_numa_set_affinity(struct OMRPortLibrary *portLibrary, uintptr_t numaNode, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	return OMRPORT_ERROR_VMEM_OPFAILED;
}

/**
 * Populates the user-provided buffer numaNodes with a structure describing the first nodeCount NUMA nodes
 * on the system at the time of the call.  Note that the structure will always contain at least 1 node (in
 * a non-NUMA system, this will be the degenerate case of "all memory and CPUs").  Note that at least one
 * node will have "preferred" memory usage (typically, the implementation will mark all nodes which
 * haven't been somehow restricted via numactl as preferred).
 *
 * @param portLibrary[in] The Port Library instance
 * @param numaNodes[out] The buffer containing a description of the NUMA nodes on the system subject to the restrictions of the current NUMA policy
 * @param nodeCount[in/out] On enter, the size of the numaNodes buffer, in J9MemoryNodeDetail structs.  On exit, the number of NUMA nodes known to the implementation (the number of entries in numaNodes populated is the minimum of these two values)
 *
 * @return 0 on success or an error code if an error occurred
 */
intptr_t
omrvmem_numa_get_node_details(struct OMRPortLibrary *portLibrary, J9MemoryNodeDetail *numaNodes, uintptr_t *nodeCount)
{
	/* Default implementation has no concept of node details */
	return OMRPORT_ERROR_VMEM_OPFAILED;
}

/**
* Get the  number of currently available bytes of physical memory.  This is not supported on z/OS.
* @param [in] portLibrary port library
* @param [out] freePhysicalMemorySize pointer to variable to receive result
* @return 0 on success, OMRPORT_ERROR_VMEM_OPFAILED if an error occurred, or OMRPORT_ERROR_VMEM_NOT_SUPPORTED.
*/
int32_t
omrvmem_get_available_physical_memory(struct OMRPortLibrary *portLibrary, uint64_t *freePhysicalMemorySize)
{
	return OMRPORT_ERROR_VMEM_NOT_SUPPORTED;
}

/**
* Get the size of a process's memory in bytes.  This is not supported on z/OS.
* @param [in] portLibrary port library
* @param [in] J9VmemMemoryQuery queryType indicates which memory aspect to measure
* @param [out] freePhysicalMemorySize pointer to variable to receive result
* @return 0 on success, OMRPORT_ERROR_VMEM_OPFAILED or OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES if an error occurred, or OMRPORT_ERROR_VMEM_NOT_SUPPORTED.
*/
int32_t
omrvmem_get_process_memory_size(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize)
{
	return OMRPORT_ERROR_VMEM_NOT_SUPPORTED;
}
