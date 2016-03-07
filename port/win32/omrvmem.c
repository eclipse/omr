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


#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>

#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"
#include "omrportasserts.h"
#include "omrvmem.h"

#include "avl_api.h"
#include "omrutil.h"

/* VMDESIGN 1778 : always allocate using the MEM_TOP_DOWN flag to avoid competition for the low memory area */
#if defined (OMR_ENV_DATA64)
#define J9_EXTRA_TYPE_FLAGS MEM_TOP_DOWN
#else
#define J9_EXTRA_TYPE_FLAGS 0
#endif


#if 0
#define J9VMEM_DEBUG
#endif

BOOL SetLockPagesPrivilege(HANDLE hProcess, BOOL bEnable);


/**
 * The structure stored in the bindingTree to describe NUMA-bound memory extents.
 */
typedef struct BindingNode {
	J9AVLTreeNode avlTree;		/**< This struct needs to be embedded so that the struct can act like a proper AVL node */
	void *baseAddress;			/**< The address of the first byte in the bound range */
	uintptr_t extentOfRangeInBytes;	/**< The number of bytes in the range */
	uintptr_t j9NodeNumber;			/**< The node number (in our internal node numbering scheme (1-indexed)) */
} BindingNode;


static void *getMemoryInRange(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, DWORD allocationFlags, DWORD protection, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode);
static BOOLEAN rangeIsValid(struct J9PortVmemIdentifier *identifier, void *address, uintptr_t byteAmount);
DWORD getProtectionBits(uintptr_t mode);
void update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address, void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, OMRMemCategory *category);
static void *commitAndTouch(struct OMRPortLibrary *portLibrary, J9PortVmemIdentifier *identifier, void *address, uintptr_t byteAmount);
static void touchOnNumaNode(struct OMRPortLibrary *portLibrary, uintptr_t pageSizeInBytes, void *address, uintptr_t byteAmount, uintptr_t j9NumaNode);
static intptr_t rangeInsertionComparator(J9AVLTree *tree, BindingNode *insertNode, BindingNode *walkNode);
static intptr_t rangeSearchComparator(J9AVLTree *tree, uintptr_t value, BindingNode *searchNode);
static int32_t getProcessPrivateMemorySize(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize);

/**
 * @internal
 * Determines the proper portable error code to return given a native error code
 *
 * @param[in] errorCode The error code reported by the OS
 *
 * @return	the (negative) portable error code
 */
static int32_t
findError(int32_t errorCode)
{
	switch (errorCode) {
	case ERROR_NO_SYSTEM_RESOURCES:
		return OMRPORT_ERROR_VMEM_INSUFFICENT_RESOURCES;
	default:
		return OMRPORT_ERROR_VMEM_OPFAILED;
	}
}

/**
 * Commit memory in virtual address space.
 *
 * @param[in] portLibrary The port library.
 * @param[in] address The page aligned starting address of the memory to commit.
 * @param[in] byteAmount The number of bytes to commit.
 * @param[in] identifier Descriptor for virtual memory block.
 *
 * @return pointer to the allocated memory on success, NULL on failure.
 */
void *
omrvmem_commit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	void *ptr = NULL;

	Trc_PRT_vmem_omrvmem_commit_memory_Entry(address, byteAmount);

	if (rangeIsValid(identifier, address, byteAmount)) {
		ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
		ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);

		if (PPG_vmem_pageSize[0] ==  identifier->pageSize) {
			ptr = commitAndTouch(portLibrary, identifier, address, byteAmount);
		} else {
			ptr = address;
		}
	} else {
		Trc_PRT_vmem_omrvmem_commit_memory_invalidRange(identifier->address, identifier->size, address, byteAmount);
		portLibrary->error_set_last_error(portLibrary,  -1, OMRPORT_ERROR_VMEM_INVALID_PARAMS);
	}

	Trc_PRT_vmem_omrvmem_commit_memory_Exit(ptr);
	return ptr;
}

/**
 * Decommit memory in virtual address space.
 *
 * Decommits physical storage of the size specified starting at the address specified.
 *
 * @param[in] portLibrary The port library.
 * @param[in] address The starting address of the memory to be decommitted.
 * @param[in] byteAmount The number of bytes to be decommitted.
 * @param[in] identifier Descriptor for virtual memory block.
 *
 * @return 0 on success, non zero on failure.
 */
intptr_t
omrvmem_decommit_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	intptr_t ret = -1;

	Trc_PRT_vmem_omrvmem_decommit_memory_Entry(address, byteAmount);

	if (portLibrary->portGlobals->vmemAdviseOSonFree == 1) {
		if (rangeIsValid(identifier, address, byteAmount)) {
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);

			if (byteAmount > 0) {
				if (PPG_vmem_pageSize[0] ==  identifier->pageSize) {
					ret = (intptr_t)VirtualFree((LPVOID)address, (size_t)byteAmount, MEM_DECOMMIT);
					if (ret != 0) {
						/* VirtualFree returns non-zero on success (we return 0) */
						ret = 0;
					} else {
						ret = -1;
						Trc_PRT_vmem_omrvmem_decommit_memory_failure(GetLastError(), address, byteAmount);
					}
				} else {
					ret = 0;
				}
			} else {
				/* nothing to de-commit. */
				Trc_PRT_vmem_decommit_memory_zero_pages();
				ret = 0;
			}
		} else {
			ret = -1;
			Trc_PRT_vmem_omrvmem_decommit_memory_invalidRange(identifier->address, identifier->size, address, byteAmount);
			portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_VMEM_INVALID_PARAMS);
		}
	} else {
		if (!rangeIsValid(identifier, address, byteAmount)) {
			ret = -1;
			Trc_PRT_vmem_omrvmem_decommit_memory_invalidRange(identifier->address, identifier->size, address, byteAmount);
			portLibrary->error_set_last_error(portLibrary, -1, OMRPORT_ERROR_VMEM_INVALID_PARAMS);
		} else {
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(address, identifier->pageSize);
			ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(byteAmount, identifier->pageSize);

			/* JVM is not allowed to decommit, just return success */
			Trc_PRT_vmem_decommit_memory_not_allowed(portLibrary->portGlobals->vmemAdviseOSonFree);
			ret = 0;
		}
	}
	Trc_PRT_vmem_omrvmem_decommit_memory_Exit(ret);
	return ret;
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
	int32_t ret = 0;
	OMRMemCategory *category = identifier->category;

	Trc_PRT_vmem_omrvmem_free_memory_Entry(address, byteAmount);
	update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, NULL);

	ret = (int32_t)VirtualFree((LPVOID)address, (size_t)0, MEM_RELEASE);
	if (ret != 0) {
		/* ensure that we also delete any NUMA binding extents which describe the memory under this area */
		/* (note that the sub-allocator can cause us to shut-down the binding pool prior to freeing the last reserved area so ignore NUMA clean-up if the pool is already gone) */
		if (NULL != portLibrary->portGlobals->platformGlobals.bindingPool) {
			void *nextBaseToLookup = address;
			J9AVLTree *boundExtents = &portLibrary->portGlobals->platformGlobals.bindingTree;

			/* There is one case where the tree can have precisely zero entries - failure in allocating the first node in the range.
			 * Otherwise, the sequence of nodes should be complete and contiguous.
			 */
			omrthread_monitor_enter(PPG_bindingAccessMonitor);
			if (NULL != (BindingNode *)avl_search(boundExtents, (uintptr_t)nextBaseToLookup)) {
				J9Pool *extentPool = portLibrary->portGlobals->platformGlobals.bindingPool;
				void *topOfExtent = (void *)((uintptr_t)address + byteAmount);

				while (nextBaseToLookup < topOfExtent) {
					BindingNode *node = (BindingNode *)avl_search(boundExtents, (uintptr_t)nextBaseToLookup);
					Assert_PRT_true(NULL != node);
					nextBaseToLookup = (void *)((uintptr_t)nextBaseToLookup + node->extentOfRangeInBytes);
					avl_delete(boundExtents, (J9AVLTreeNode *)node);
					pool_removeElement(extentPool, node);
				}
			}
			omrthread_monitor_exit(PPG_bindingAccessMonitor);
		}

		/* VirtualFree returns non-zero on success (we return 0) */
		ret = 0;
	} else {
		ret = -1;
	}

	omrmem_categories_decrement_counters(category, byteAmount);
	Trc_PRT_vmem_omrvmem_free_memory_Exit(ret);
	return ret;
}

int32_t
omrvmem_vmem_params_init(struct OMRPortLibrary *portLibrary, struct J9PortVmemParams *params)
{
	memset(params, 0, sizeof(struct J9PortVmemParams));
	params->startAddress = NULL;
	params->endAddress = OMRPORT_VMEM_MAX_ADDRESS;
	params->byteAmount = 0;
	params->pageSize = PPG_vmem_pageSize[0];
	params->pageFlags = PPG_vmem_pageFlags[0];
	params->mode = OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE;
	params->options = 0;
	params->category = OMRMEM_CATEGORY_UNKNOWN;
	params->alignmentInBytes = 0;
	return 0;
}

void *
omrvmem_reserve_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier, uintptr_t mode, uintptr_t pageSize, uint32_t category)
{
	struct J9PortVmemParams params;
	omrvmem_vmem_params_init(portLibrary, &params);
	if (NULL != address) {
		params.startAddress = address;
		params.endAddress = address;
	}
	params.byteAmount = byteAmount;
	params.mode = mode;
	params.pageSize = pageSize;
	params.pageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
	params.options = 0;
	params.category = category;
	return portLibrary->vmem_reserve_memory_ex(portLibrary, identifier, &params);
}

void *
omrvmem_reserve_memory_ex(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, struct J9PortVmemParams *params)
{
	LPVOID memoryPointer = NULL;
	DWORD protection = getProtectionBits(params->mode);
	DWORD allocationFlags = 0;
	OMRMemCategory *category = omrmem_get_category(portLibrary, params->category);


#if defined(J9VMEM_DEBUG)
	printf("\n\tomrvmem_reserve_memory_ex byteAmount: %p, startAddress: %p, endAddress: %p, pageSize: %p\n"
		   " \t\t%s\n"
		   " \t\t%s\n"
		   " \t\t%s\n"
		   " \t\t%s\n ",
		   params->byteAmount,
		   params->startAddress,
		   params->endAddress,
		   params->pageSize,
		   (OMRPORT_VMEM_STRICT_PAGE_SIZE & params->options) ? "OMRPORT_VMEM_STRICT_PAGE_SIZE" : "\t",
		   (OMRPORT_VMEM_STRICT_ADDRESS & params->options) ? "OMRPORT_VMEM_STRICT_ADDRESS" : "\t",
		   (OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP & params->options) ? "OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP" : "\t",
		   (OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN & params->options) ? "OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN" : "\t",
		   (OMRPORT_VMEM_ZOS_USE2TO32G_AREA & params->options) ? "OMRPORT_VMEM_ZOS_USE2TO32G_AREA" : "\t");
#endif

	Trc_PRT_vmem_omrvmem_reserve_memory_Entry(params->startAddress, params->byteAmount);

	Assert_PRT_true(params->startAddress <= params->endAddress);
	ASSERT_VALUE_IS_PAGE_SIZE_ALIGNED(params->byteAmount, params->pageSize);

	/* Invalid input */
	if (0 == params->pageSize) {
		update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, NULL);
		Trc_PRT_vmem_omrvmem_reserve_memory_Exit1();
	} else if (PPG_vmem_pageSize[0] == params->pageSize) {
		uintptr_t alignmentInBytes = OMR_MAX(params->pageSize, params->alignmentInBytes);
		uintptr_t minimumGranule = OMR_MIN(params->pageSize, params->alignmentInBytes);
		/* Handle default page size */
		allocationFlags |= MEM_RESERVE | J9_EXTRA_TYPE_FLAGS;

		/* Determine if a commit is required */
		if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & params->mode)) {
			allocationFlags |= MEM_COMMIT;
		} else {
			/*
			 * If we don't reserve with PAGE_NOACCESS, CE won't give us large blocks.
			 * On Win32 the protection bits appear to be ignored for uncommitted memory.
			 */
			protection = PAGE_NOACCESS;
		}

		/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
		if ((0 == minimumGranule) || (0 == (alignmentInBytes % minimumGranule))) {
			memoryPointer = getMemoryInRange(portLibrary, identifier, allocationFlags, protection, category, params->byteAmount, params->startAddress, params->endAddress, alignmentInBytes, params->options, params->mode);
		}
		if (NULL == memoryPointer) {
			Trc_PRT_vmem_omrvmem_reserve_memory_failure();
			update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, NULL);
		} else {
			update_vmemIdentifier(identifier, (void *)memoryPointer, (void *)memoryPointer, params->byteAmount, params->mode, PPG_vmem_pageSize[0], OMRPORT_VMEM_PAGE_FLAG_NOT_USED, category);
		}
		Trc_PRT_vmem_omrvmem_reserve_memory_Exit2(memoryPointer);
	} else if (PPG_vmem_pageSize[1] == params->pageSize) {
		uintptr_t largePageAlignmentInBytes = OMR_MAX(params->pageSize, params->alignmentInBytes);
		uintptr_t largePageMinimumGranule = OMR_MIN(params->pageSize, params->alignmentInBytes);
		uintptr_t largePageSize = PPG_vmem_pageSize[1];
		uintptr_t numOfPages = params->byteAmount / largePageSize;
		uintptr_t leftOver = params->byteAmount - numOfPages * largePageSize;
		uintptr_t totalAllocateSize = 0;
		DWORD lastError;

		/* MEM_COMMIT is required when reserving large pages */
		allocationFlags |= MEM_LARGE_PAGES | MEM_COMMIT | MEM_RESERVE | J9_EXTRA_TYPE_FLAGS;

		if (leftOver != 0) {
			numOfPages++;
		}
		totalAllocateSize = numOfPages * largePageSize;

		/* Allocate large pages in the process's virtual address space, must commit. */
		SetLockPagesPrivilege(GetCurrentProcess(), TRUE);
		/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
		if ((0 == largePageMinimumGranule) || (0 == (largePageAlignmentInBytes % largePageMinimumGranule))) {
			memoryPointer = getMemoryInRange(portLibrary, identifier, allocationFlags, protection, category, totalAllocateSize, params->startAddress, params->endAddress, largePageAlignmentInBytes, params->options, params->mode);
		}
		if (NULL == memoryPointer) {
			/* if OMRPORT_VMEM_STRICT_PAGE_SIZE is not set try again with default page size */
			if (0 == (OMRPORT_VMEM_STRICT_PAGE_SIZE & params->options)) {
				/* Handle default page size */
				uintptr_t pageSize = PPG_vmem_pageSize[0];
				uintptr_t alignmentInBytes = OMR_MAX(pageSize, params->alignmentInBytes);
				uintptr_t minimumGranule = OMR_MIN(pageSize, params->alignmentInBytes);
				DWORD defaultProtection = getProtectionBits(params->mode);
				DWORD defaultAllocationFlags = MEM_RESERVE | J9_EXTRA_TYPE_FLAGS;

				/* Determine if a commit is required */
				if (0 != (OMRPORT_VMEM_MEMORY_MODE_COMMIT & params->mode)) {
					defaultAllocationFlags |= MEM_COMMIT;
				} else {
					/*
					 * If we don't reserve with PAGE_NOACCESS, CE won't give us large blocks.
					 * On Win32 the protection bits appear to be ignored for uncommitted memory.
					 */
					defaultProtection = PAGE_NOACCESS;
				}

				/* Make sure that the alignment is a multiple of both requested alignment and page size (enforces that arguments are powers of two and, thus, their max is their lowest common multiple) */
				if ((0 == minimumGranule) || (0 == (alignmentInBytes % minimumGranule))) {
					memoryPointer = getMemoryInRange(portLibrary, identifier, defaultAllocationFlags, defaultProtection, category, params->byteAmount, params->startAddress, params->endAddress, alignmentInBytes, params->options, params->mode);
				}
				if (NULL == memoryPointer) {
					Trc_PRT_vmem_omrvmem_reserve_memory_failure();
					update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, NULL);
				} else {
					update_vmemIdentifier(identifier, (void *)memoryPointer, (void *)memoryPointer, params->byteAmount, params->mode, PPG_vmem_pageSize[0], OMRPORT_VMEM_PAGE_FLAG_NOT_USED, category);
				}
			} else {
				Trc_PRT_vmem_omrvmem_reserve_memory_failure();
				lastError = GetLastError();
				portLibrary->error_set_last_error(portLibrary, lastError, findError(lastError));
				update_vmemIdentifier(identifier, NULL, NULL, 0, 0, 0, 0, NULL);
			}
		} else {
			update_vmemIdentifier(identifier, (void *)memoryPointer, (void *)memoryPointer, totalAllocateSize, params->mode, largePageSize, OMRPORT_VMEM_PAGE_FLAG_NOT_USED, category);
		}
		SetLockPagesPrivilege(GetCurrentProcess(), FALSE);
	}

	if (NULL != memoryPointer) {
		/* we don't want to have to search the space for a node descriptor when committing something which might only include part of a
		 * bound range so create the "unbound range" entry so that we can immediately find the boundaries of the extent
		 */
		BindingNode *baseNode = NULL;
		omrthread_monitor_enter(PPG_bindingAccessMonitor);
		baseNode = pool_newElement(portLibrary->portGlobals->platformGlobals.bindingPool);
		if (NULL != baseNode) {
			baseNode->baseAddress = (void *)memoryPointer;
			baseNode->extentOfRangeInBytes = params->byteAmount;
			baseNode->j9NodeNumber = 0;
			avl_insert(&portLibrary->portGlobals->platformGlobals.bindingTree, (J9AVLTreeNode *)baseNode);
			omrthread_monitor_exit(PPG_bindingAccessMonitor);
		} else {
			omrthread_monitor_exit(PPG_bindingAccessMonitor);
			omrvmem_free_memory(portLibrary, memoryPointer, params->byteAmount, identifier);
			memoryPointer = NULL;
		}
	}
	Trc_PRT_vmem_omrvmem_reserve_memory_Exit(memoryPointer);

#if defined(J9VMEM_DEBUG)
	printf("\t\t omrvmem_reserve_memory_ex returning address: %p\n", memoryPointer);
#endif

	return (void *)memoryPointer;
}

uintptr_t
omrvmem_get_page_size(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier)
{
	return identifier->pageSize;
}

uintptr_t
omrvmem_get_page_flags(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier)
{
	return identifier->pageFlags;
}

/**
 * PortLibrary shutdown stubbed.
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
 * PortLibrary shutdown Implementation.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrvmem_startup
 * should be destroyed here. This function replaces the stub version during omrvmem_startup. This is done so that omrvmem
 * resources are not destroyed without first being initialized.
 *
 * @param[in] portLibrary The port library.
 */
void
omrvmem_shutdownImpl(struct OMRPortLibrary *portLibrary)
{
	omrthread_monitor_enter(PPG_bindingAccessMonitor);
	if (NULL != portLibrary->portGlobals->platformGlobals.bindingPool) {
		pool_kill(portLibrary->portGlobals->platformGlobals.bindingPool);
		portLibrary->portGlobals->platformGlobals.bindingPool = NULL;
	}
	omrthread_monitor_exit(PPG_bindingAccessMonitor);

	PPG_numa_platform_supports_numa = 0;
	omrthread_monitor_destroy(PPG_bindingAccessMonitor);
	PPG_bindingAccessMonitor = NULL;
	MUTEX_DESTROY(PPG_managementDataLock);
	PPG_managementCounterPathStatus = MANAGEMENT_COUNTER_PATH_UNINITIALIZED;
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
	SYSTEM_INFO systemInfo;
	uintptr_t handle;
	uintptr_t (WINAPI *GetLargePageMinimumFunc)();
	HMODULE kernel32Module = GetModuleHandle("Kernel32.dll");
	J9AVLTree *boundExtents = &portLibrary->portGlobals->platformGlobals.bindingTree;
	int32_t returnValue = 0;

	/* Slabs may force vmem to initialize early, make second attempt succeed. */
	if (TRUE == portLibrary->portGlobals->platformGlobals.vmem_initialized) {
		return 0;
	}

	/*
	 * Initialize the bindingAccessMonitor. Required to ensure thread safety when calls
	 * are made to omrvmem operations. Specifically, to provide synchronization when modifying the
	 * bindingTree and accessing the bindingPool.
	 */
	if (omrthread_monitor_init_with_name(&(PPG_bindingAccessMonitor), 0, "omrvmem_binding_access_monitor")) {
		return OMRPORT_ERROR_STARTUP_VMEM;
	}

	portLibrary->vmem_shutdown = &omrvmem_shutdownImpl;

	/* 0 terminate the table */
	memset(PPG_vmem_pageSize, 0, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));
	memset(PPG_vmem_pageFlags, 0, OMRPORT_VMEM_PAGESIZE_COUNT * sizeof(uintptr_t));

	/* Default page size */
	GetSystemInfo(&systemInfo);
	PPG_vmem_pageSize[0] = (uintptr_t)systemInfo.dwPageSize;
	PPG_vmem_pageFlags[0] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	/* Determine if largePages are supported on this platform */
	/* Look for GetLargePageMinimum in the Kernel32 DLL */
	if (0 == portLibrary->sl_open_shared_library(portLibrary, "Kernel32", &handle, TRUE)) {
		if (0 == portLibrary->sl_lookup_name(portLibrary, handle, "GetLargePageMinimum", (uintptr_t *)&GetLargePageMinimumFunc, "PV")) {
			PPG_vmem_pageSize[1] = GetLargePageMinimumFunc();
			PPG_vmem_pageFlags[1] = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;
			/* Safety check, best guess if necessary */
			if (PPG_vmem_pageSize[1] == 0) {
				PPG_vmem_pageSize[1] = 4194304;
			}
		}

		if (portLibrary->sl_close_shared_library(portLibrary, handle)) {
			/* NLS has not yet initialized */
			Trc_PRT_vmem_omrvmem_startup_failed_to_close_dll();
		}
	}

	/* Now check that the user has permissions to allocate large pages */
	if (SetLockPagesPrivilege(GetCurrentProcess(), TRUE) != TRUE) {
		/* local policy forbids raising the priveledge - user must check documentation */
		Trc_PRT_vmem_user_does_not_have_Lock_Pages_in_Memory_permission_in_local_security_policy_settings();
		PPG_vmem_pageSize[1] = 0;
		PPG_vmem_pageFlags[1] = 0;
	} else {
		SetLockPagesPrivilege(GetCurrentProcess(), FALSE);
	}

	if (NULL != kernel32Module) {
		portLibrary->portGlobals->platformGlobals.GetThreadIdealProcessorExProc = (GetThreadIdealProcessorExType)GetProcAddress(kernel32Module, "GetThreadIdealProcessorEx");
		portLibrary->portGlobals->platformGlobals.SetThreadIdealProcessorExProc = (SetThreadIdealProcessorExType)GetProcAddress(kernel32Module, "SetThreadIdealProcessorEx");
		portLibrary->portGlobals->platformGlobals.GetNumaNodeProcessorMaskExProc = (GetNumaNodeProcessorMaskExType)GetProcAddress(kernel32Module, "GetNumaNodeProcessorMaskEx");
	}

	memset(boundExtents, 0x0, sizeof(J9AVLTree));
	boundExtents->insertionComparator = (intptr_t (*)(J9AVLTree *, J9AVLTreeNode *, J9AVLTreeNode *))rangeInsertionComparator;
	boundExtents->searchComparator = (intptr_t (*)(J9AVLTree *, uintptr_t, J9AVLTreeNode *))rangeSearchComparator;
	boundExtents->portLibrary = portLibrary;
	portLibrary->portGlobals->platformGlobals.bindingPool = pool_new(sizeof(BindingNode), 5, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY, POOL_FOR_PORT(portLibrary));
	if (NULL == portLibrary->portGlobals->platformGlobals.bindingPool) {
		returnValue = OMRPORT_ERROR_STARTUP_VMEM;
	}
	if (!MUTEX_INIT(PPG_managementDataLock)) {
		returnValue = OMRPORT_ERROR_STARTUP_VMEM;
	}
	portLibrary->portGlobals->platformGlobals.managementCounterPathStatus = MANAGEMENT_COUNTER_PATH_UNINITIALIZED;
	if (0 == returnValue) {
		portLibrary->portGlobals->platformGlobals.vmem_initialized = TRUE;
		/* set default value to advise OS about vmem that is no longer needed */
		portLibrary->portGlobals->vmemAdviseOSonFree = 1;
	}

	return returnValue;
}
/**
 * Determine the page sizes supported.
 *
 * @param[in] portLibrary The port library.
 *
 * @return A 0 terminated array of supported page sizes.  The first entry is the default page size, other entries
 * are the large page sizes supported.
 */
uintptr_t *
omrvmem_supported_page_sizes(struct OMRPortLibrary *portLibrary)
{
	return PPG_vmem_pageSize;
}

uintptr_t *
omrvmem_supported_page_flags(struct OMRPortLibrary *portLibrary)
{
	return PPG_vmem_pageFlags;
}

/**
 * @internal
 * Update J9PortVmIdentifier structure
 *
 * @param[in] identifier The structure to be updated
 * @param[in] address Base address
 * @param[in] handle Platform specific handle for reserved memory
 * @param[in] byteAmount Size of allocated area
 * @param[in] mode Access Mode
 * @param[in] pageSize Constant describing pageSize
 * @param[in] pageFlags flags for describing page type
 * @param[in] category Memory allocation category
 */
void
update_vmemIdentifier(J9PortVmemIdentifier *identifier, void *address,  void *handle, uintptr_t byteAmount, uintptr_t mode, uintptr_t pageSize, uintptr_t pageFlags, OMRMemCategory *category)
{
	identifier->address = address;
	identifier->handle = handle;
	identifier->size = byteAmount;
	identifier->pageSize = pageSize;
	identifier->pageFlags = pageFlags;
	identifier->mode = mode;
	identifier->category = category;
}


BOOL
SetLockPagesPrivilege(HANDLE hProcess, BOOL bEnable)
{
	struct {
		DWORD Count;
		LUID_AND_ATTRIBUTES Privilege [1];
	} Info;

	HANDLE Token;

	/* Open the token. */
	if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &Token) != TRUE) {
		return FALSE;
	}

	/* Enable or disable? */
	Info.Count = 1;
	if (bEnable) {
		Info.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
	} else {
		Info.Privilege[0].Attributes = 0;
	}

	/* Get the LUID. */
	if (LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &(Info.Privilege[0].Luid)) != TRUE) {
		return FALSE;
	}

	/* Adjust the privilege. */
	if (AdjustTokenPrivileges(Token, FALSE, (PTOKEN_PRIVILEGES) &Info, 0, NULL, NULL) != TRUE) {
		return FALSE;
	} else {
		if (GetLastError() != ERROR_SUCCESS) {
			return FALSE;
		}
	}

	CloseHandle(Token);

	return TRUE;
}


DWORD
getProtectionBits(uintptr_t mode)
{
	if (0 != (OMRPORT_VMEM_MEMORY_MODE_EXECUTE & mode)) {
		if (0 != (OMRPORT_VMEM_MEMORY_MODE_READ & mode)) {
			if (0 != (OMRPORT_VMEM_MEMORY_MODE_WRITE & mode)) {
				return PAGE_EXECUTE_READWRITE;
			} else {
				return PAGE_EXECUTE_READ;
			}
		} else {
			return PAGE_NOACCESS;
		}
	} else {
		if (0 != (OMRPORT_VMEM_MEMORY_MODE_READ & mode)) {
			if (0 != (OMRPORT_VMEM_MEMORY_MODE_WRITE & mode)) {
				return PAGE_READWRITE;
			} else {
				return PAGE_READONLY;
			}
		} else {
			return PAGE_NOACCESS;
		}
	}
}

void
omrvmem_default_large_page_size_ex(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags)
{
	/* Note that the PPG_vmem_pageSize is a null-terminated list of page sizes.
	 * There will always be the 0 element (default page size),
	 * so the 1 element will be zero or the default large size where only 2 are supported
	 * (platforms with more complicated logic will use their own special process to determine the best response
	 */
	if (NULL != pageSize) {
		*pageSize = PPG_vmem_pageSize[1];
	}
	if (NULL != pageFlags) {
		*pageFlags = PPG_vmem_pageFlags[1];
	}
	return;
}

intptr_t
omrvmem_find_valid_page_size(struct OMRPortLibrary *portLibrary, uintptr_t mode, uintptr_t *pageSize, uintptr_t *pageFlags, BOOLEAN *isSizeSupported)
{
	uintptr_t validPageSize = *pageSize;
	uintptr_t validPageFlags = *pageFlags;
	uintptr_t defaultLargePageSize = 0;
	uintptr_t defaultLargePageFlags = OMRPORT_VMEM_PAGE_FLAG_NOT_USED;

	Assert_PRT_true_wrapper(OMRPORT_VMEM_PAGE_FLAG_NOT_USED == validPageFlags);

	if (0 != validPageSize) {
		uintptr_t pageIndex = 0;
		uintptr_t *supportedPageSizes = portLibrary->vmem_supported_page_sizes(portLibrary);
		uintptr_t *supportedPageFlags = portLibrary->vmem_supported_page_flags(portLibrary);

		for (pageIndex = 0; 0 != supportedPageSizes[pageIndex]; pageIndex++) {
			if ((supportedPageSizes[pageIndex] == validPageSize) &&
				(supportedPageFlags[pageIndex] == validPageFlags)
			) {
				goto _end;
			}
		}
	}

	portLibrary->vmem_default_large_page_size_ex(portLibrary, mode, &defaultLargePageSize, &defaultLargePageFlags);
	if (0 != defaultLargePageSize) {
		validPageSize = defaultLargePageSize;
		validPageFlags = defaultLargePageFlags;
	} else {
		validPageSize = PPG_vmem_pageSize[0];
		validPageFlags = PPG_vmem_pageFlags[0];
	}

_end:
	/* Since page type is not used, ignore pageFlags when setting isSizeSupported. */
	*isSizeSupported = (*pageSize == validPageSize);
	*pageSize = validPageSize;
	*pageFlags = validPageFlags;

	return 0;
}

/**
 * Allocates memory in specified range using a best effort approach
 * (unless OMRPORT_VMEM_STRICT_ADDRESS flag is used) and returns a pointer
 * to the newly allocated memory. Returns NULL on failure.
 */
static void *
getMemoryInRange(struct OMRPortLibrary *portLibrary, struct J9PortVmemIdentifier *identifier, DWORD allocationFlags, DWORD protection, OMRMemCategory *category, uintptr_t byteAmount, void *startAddress, void *endAddress, uintptr_t alignmentInBytes, uintptr_t vmemOptions, uintptr_t mode)
{
	intptr_t direction = 1;
	void *currentAddress = startAddress;
	void *oldAddress;
	void *memoryPointer;
#if defined(J9VMEM_DEBUG)
	static int count = 0;
#endif


	/* check allocation direction */
	if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN)) {
		allocationFlags |= MEM_TOP_DOWN;
		direction = -1;
		currentAddress = endAddress;
#if defined(J9VMEM_DEBUG)
		printf("\t\t getMemoryInRange top down, start address: %p\n", currentAddress);
#endif

	} else if (0 != (vmemOptions & OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP)) {
		allocationFlags &= ~MEM_TOP_DOWN;
		if (startAddress == NULL) {
			currentAddress = (void *)((uintptr_t)currentAddress + direction * alignmentInBytes);
		}
#if defined(J9VMEM_DEBUG)
		printf("\t\t getMemoryInRange bottom up, start address: %p\n", currentAddress);
#endif

	} else if (NULL == startAddress) {
		if (OMRPORT_VMEM_MAX_ADDRESS == endAddress) {
			/* if caller specified the entire address range and does not care about the direction
			 * save time by letting OS choose where to allocate the memory
			 */
			goto allocAnywhere;
		} else {
			/* if the startAddress is NULL but the endAddress is not we
			 * need to change the startAddress so that we have a better chance
			 * of getting memory within the range because NULL means don't care
			 */
			currentAddress = (void *)alignmentInBytes;
		}
	}

	/*
	 * Don't bother scanning the entire address space if there simply isn't enough memory to satisfy the request.
	 * Large allocations with large pages can be slow on certain Windows versions, see CMVC 194435.
	 * Test using the caller's setting for MEM_LARGE_PAGES, because the allocation may be feasible with small pages
	 * but not with large pages.
	 */
	memoryPointer = VirtualAlloc(NULL, (SIZE_T)byteAmount, allocationFlags, protection);
	if (NULL == memoryPointer) {
		/* there isn't enough memory */
		return NULL;
	} else {
		/* there is memory, free it then continue to request it in the range and direction we're after */
		VirtualFree((LPVOID)memoryPointer, (size_t)0, MEM_RELEASE);
	}

	/* prevent while loop from attempting to free memory on first entry */
	memoryPointer = NULL;

	/* try all addresses within range */
	while ((startAddress <= currentAddress) && (endAddress >= currentAddress)) {

#if defined(J9VMEM_DEBUG)
		if ((count % 0x10) == 0) {
			printf("\t\t getMemoryInRange calling VirtualAlloc: address: %p, size: %p, allocationFlags: %x, protection: %x\n", (LPVOID)currentAddress, byteAmount, allocationFlags, protection);
			fflush(stdout);
		}
#endif

		/* See comment above concerning CMVC 194435 */
		memoryPointer = VirtualAlloc((LPVOID)currentAddress, (SIZE_T)byteAmount, allocationFlags & ~MEM_LARGE_PAGES, protection);

		if (NULL != memoryPointer) {
			if (MEM_LARGE_PAGES == (allocationFlags & MEM_LARGE_PAGES)) {
				void *smallPagesMemoryPointer = memoryPointer;

				VirtualFree((LPVOID)smallPagesMemoryPointer, (size_t)0, MEM_RELEASE);
				memoryPointer = VirtualAlloc((LPVOID)currentAddress, (SIZE_T)byteAmount, allocationFlags, protection);
				if (NULL == memoryPointer) {
					Trc_PRT_vmem_omrvmem_reserve_memory_ex_UnableToReallocateWithLargePages(byteAmount, smallPagesMemoryPointer);
				}
			}
		}

#if defined(J9VMEM_DEBUG)
		if ((count++ % 0x10) == 0) {
			printf("\t\t\t getMemoryInRange returned from VirtualAlloc\n");
			fflush(stdout);
		}
#endif
		/* stop if returned pointer is within range */
		if (NULL != memoryPointer) {
			if ((startAddress <= memoryPointer) && (endAddress >= memoryPointer)) {
				break;
			} else {
				VirtualFree((LPVOID)memoryPointer, (size_t)0, MEM_RELEASE);
				memoryPointer = NULL ; /* This is necessary, cause below OMRPORT_VMEM_STRICT_ADDRESS if statement might try to free the same memoryPointer*/
			}
		}

		oldAddress = currentAddress;

		currentAddress = (void *)((uintptr_t)currentAddress + direction * alignmentInBytes);

		/* protect against loop around */
		if (((1 == direction) && ((uintptr_t)oldAddress > (uintptr_t)currentAddress)) ||
			((-1 == direction) && ((uintptr_t)oldAddress < (uintptr_t)currentAddress))) {
			break;
		}
	}

	if (NULL == memoryPointer) {
		if (0 == (OMRPORT_VMEM_STRICT_ADDRESS & vmemOptions)) {
allocAnywhere:
			memoryPointer = VirtualAlloc(NULL, (SIZE_T)byteAmount, allocationFlags, protection);
		} else {
			Trc_PRT_vmem_omrvmem_reserve_memory_ex_UnableToAllocateWithinSpecifiedRange(byteAmount, startAddress, endAddress);
		}
	}

	if (NULL != memoryPointer) {
		omrmem_categories_increment_counters(category, (SIZE_T)byteAmount);
	}

	return memoryPointer;
}

/**
 * Ensures that the requested address and (address + byteAmount) fall
 * within the reserved identifier->address and (identifier->address + identifier->size)
 * @returns TRUE if the range is valid, FALSE otherwise
 */
static BOOLEAN
rangeIsValid(struct J9PortVmemIdentifier *identifier, void *address, uintptr_t byteAmount)
{
	BOOLEAN isValid = FALSE;
	uintptr_t requestedUpperLimit = (uintptr_t)address + byteAmount - 1;

	if (requestedUpperLimit + 1 >= byteAmount) {
		/* Requested range does not wrap around */
		uintptr_t realUpperLimit = (uintptr_t)identifier->address + identifier->size - 1;

		if (((uintptr_t)address >= (uintptr_t)identifier->address) &&
			(requestedUpperLimit <= realUpperLimit)
		) {
			isValid = TRUE;
		}
	}

	return isValid;
}

intptr_t
omrvmem_numa_set_affinity(struct OMRPortLibrary *portLibrary, uintptr_t numaNode, void *address, uintptr_t byteAmount, struct J9PortVmemIdentifier *identifier)
{
	intptr_t returnValue = 0;
	void *topAddress = (void *)((uintptr_t)address + byteAmount);
	J9AVLTree *boundExtents = &portLibrary->portGlobals->platformGlobals.bindingTree;
	J9Pool *extentPool = portLibrary->portGlobals->platformGlobals.bindingPool;
	BindingNode *before = NULL;
	BindingNode *after = NULL;
	BindingNode *match = NULL;
	BindingNode *end = NULL;
	BindingNode *insertedNode = NULL;
	void *nextSearchAddress = address;

	omrthread_monitor_enter(PPG_bindingAccessMonitor);
	before = (BindingNode *)avl_search(boundExtents, (uintptr_t)address - 1);
	match = (BindingNode *)avl_search(boundExtents, (uintptr_t)address);

	/* Find where to store this extent in the AVL tree of extent descriptors */
	Assert_PRT_true(NULL != match);
	if (before == match) {
		/* we need to split this one for the start */
		uintptr_t newBeforeSize = (uintptr_t)address - (uintptr_t)before->baseAddress;
		uintptr_t originalExtentLength = before->extentOfRangeInBytes;
		BindingNode *newNode = (BindingNode *)pool_newElement(extentPool);
		if (NULL != newNode) {
			avl_delete(boundExtents, (J9AVLTreeNode *)before);
			before->extentOfRangeInBytes = newBeforeSize;
			newNode->baseAddress = address;
			newNode->extentOfRangeInBytes = originalExtentLength - newBeforeSize;
			newNode->j9NodeNumber = before->j9NodeNumber;
			avl_insert(boundExtents, (J9AVLTreeNode *)before);
			avl_insert(boundExtents, (J9AVLTreeNode *)newNode);
			match = newNode;
		} else {
			returnValue = OMRPORT_ERROR_VMEM_OPFAILED;
		}
	}
	if (0 == returnValue) {
		after = (BindingNode *)avl_search(boundExtents, (uintptr_t)topAddress);
		end = (BindingNode *)avl_search(boundExtents, (uintptr_t)topAddress - 1);
		if (end == after) {
			/* we need to split this one for the end */
			uintptr_t newEndSize = (uintptr_t)topAddress - (uintptr_t)end->baseAddress;
			uintptr_t originalExtentLength = end->extentOfRangeInBytes;
			BindingNode *newNode = (BindingNode *)pool_newElement(extentPool);
			if (NULL != newNode) {
				avl_delete(boundExtents, (J9AVLTreeNode *)end);
				end->extentOfRangeInBytes = newEndSize;
				newNode->baseAddress = topAddress;
				newNode->extentOfRangeInBytes = originalExtentLength - newEndSize;
				newNode->j9NodeNumber = end->j9NodeNumber;
				avl_insert(boundExtents, (J9AVLTreeNode *)end);
				avl_insert(boundExtents, (J9AVLTreeNode *)newNode);
				after = newNode;
			} else {
				returnValue = OMRPORT_ERROR_VMEM_OPFAILED;
			}
		}
	}
	if (0 == returnValue) {
		/* now, walk all the extents in the range [address, address+byteAmount) and delete them */
		while (nextSearchAddress < topAddress) {
			BindingNode *toDelete = (BindingNode *)avl_search(boundExtents, (uintptr_t)nextSearchAddress);
			nextSearchAddress = (void *)((uintptr_t)nextSearchAddress + toDelete->extentOfRangeInBytes);
			avl_delete(boundExtents, (J9AVLTreeNode *)toDelete);
			pool_removeElement(extentPool, toDelete);
		}
		/* finally, insert the new node */
		insertedNode = (BindingNode *)pool_newElement(extentPool);
		if (NULL != insertedNode) {
			insertedNode->baseAddress = address;
			insertedNode->extentOfRangeInBytes = byteAmount;
			insertedNode->j9NodeNumber = numaNode;
			avl_insert(boundExtents, (J9AVLTreeNode *)insertedNode);
		} else {
			returnValue = OMRPORT_ERROR_VMEM_OPFAILED;
		}
	}
	if (0 == returnValue) {
		/* coalesce matches */
		if ((NULL != before) && (before->j9NodeNumber == insertedNode->j9NodeNumber)) {
			uintptr_t sizeToGrowTo = before->extentOfRangeInBytes + insertedNode->extentOfRangeInBytes;
			avl_delete(boundExtents, (J9AVLTreeNode *)before);
			avl_delete(boundExtents, (J9AVLTreeNode *)insertedNode);
			pool_removeElement(extentPool, insertedNode);
			before->extentOfRangeInBytes = sizeToGrowTo;
			insertedNode = before;
			avl_insert(boundExtents, (J9AVLTreeNode *)insertedNode);
		}
		if ((NULL != after) && (after->j9NodeNumber == insertedNode->j9NodeNumber)) {
			uintptr_t sizeToGrowTo = after->extentOfRangeInBytes + insertedNode->extentOfRangeInBytes;
			avl_delete(boundExtents, (J9AVLTreeNode *)after);
			avl_delete(boundExtents, (J9AVLTreeNode *)insertedNode);
			pool_removeElement(extentPool, after);
			insertedNode->extentOfRangeInBytes = sizeToGrowTo;
			avl_insert(boundExtents, (J9AVLTreeNode *)insertedNode);
		}
	}
	omrthread_monitor_exit(PPG_bindingAccessMonitor);
	return returnValue;
}

intptr_t
omrvmem_numa_get_node_details(struct OMRPortLibrary *portLibrary, J9MemoryNodeDetail *numaNodes, uintptr_t *nodeCount)
{
	uintptr_t arraySize = *nodeCount;
	uintptr_t totalNodeCount = 0;
	ULONG platformMaxNodeNumber = 0;
	typedef BOOL (__stdcall *getNumaNodeProcessorMaskExProcType)(USHORT, PGROUP_AFFINITY);
	typedef BOOL (__stdcall *getNumaAvailableMemoryNodeExProcType)(USHORT, PULONGLONG);
	HMODULE kernel32Module = GetModuleHandle("Kernel32.dll");
	getNumaNodeProcessorMaskExProcType getNumaNodeProcessorMaskExProc = NULL;
	getNumaAvailableMemoryNodeExProcType getNumaAvailableMemoryNodeExProc = NULL;

	/* Check the parameters such that if numaNodes is NULL that nodeCount/arraySize is 0, as per the specification. */
	Assert_PRT_true((NULL == numaNodes) == (0 == arraySize));

	if (NULL != kernel32Module) {
		getNumaNodeProcessorMaskExProc = (getNumaNodeProcessorMaskExProcType)GetProcAddress(kernel32Module, "GetNumaNodeProcessorMaskEx");
		getNumaAvailableMemoryNodeExProc = (getNumaAvailableMemoryNodeExProcType)GetProcAddress(kernel32Module, "GetNumaAvailableMemoryNodeEx");

		/* Note that we only want to describe NUMA capabilities if we see more than 1 node.  At some point we might want to change our definition of "NUMA enabled" so that all systems
		 * have NUMA properties but some have only 1 node.  This way, the special case is removed and the GC can decide how to handle that environment.
		 */
		if ((NULL != getNumaNodeProcessorMaskExProc) && (NULL != getNumaAvailableMemoryNodeExProc) && GetNumaHighestNodeNumber(&platformMaxNodeNumber) && (platformMaxNodeNumber > 1)) {
			USHORT i = 0;

			PPG_numa_platform_supports_numa = 1;
			for (i = 0; i <= platformMaxNodeNumber; i++) {

				ULONGLONG bytesAvailable = 0;
				GROUP_AFFINITY processorMask;

				memset(&processorMask, 0x0, sizeof(processorMask));
				if (getNumaAvailableMemoryNodeExProc(i, &bytesAvailable) && getNumaNodeProcessorMaskExProc(i, &processorMask)) {
					/* this node appears to exist so populate its data in the node detail array */
					uintptr_t processorCount = 0;
					KAFFINITY groupAffinity = processorMask.Mask;

					if (totalNodeCount < arraySize) {
						/* Count the bits (TODO:  see if there is a helper to do this more efficiently) */
						while (0 != groupAffinity) {
							if (0x1 == (0x1 & groupAffinity)) {
								processorCount += 1;
							}
							groupAffinity >>= 1;
						}
						numaNodes[totalNodeCount].j9NodeNumber = (uintptr_t)i + 1;
						numaNodes[totalNodeCount].memoryPolicy = (0 != bytesAvailable) ? J9NUMA_ALLOWED : J9NUMA_DENIED;
						numaNodes[totalNodeCount].computationalResourcesAvailable = processorCount;
					}
					totalNodeCount += 1;
				}
			}
		}
	}
	*nodeCount = totalNodeCount;
	return 0;
}

/**
 * @internal
 * Commits byteAmount bytes starting at address and touches the pages within the range which possess a NUMA binding.
 * @param portLibrary[in] The Port Library instance
 * @param identifier[in] The identifier for this particular virtual memory fragment
 * @param address[in] The base address of the region of memory to be committed
 * @param byteAmount[in] The length, in bytes, of the virtual memory fragment starting at address
 * @return The base address of the committed region or NULL if the operation failed
 */
static void *
commitAndTouch(struct OMRPortLibrary *portLibrary, J9PortVmemIdentifier *identifier, void *address, uintptr_t byteAmount)
{
	LPVOID finalResult = NULL;
	uintptr_t current = (uintptr_t)address;
	uintptr_t top = current + byteAmount;
	while ((current != 0) && (current < top)) {
		/* touchOnNumaNode() is going to write to the pages we commit so it's important to only commit pages which
		 * haven't yet been committed. If we were to recommit a page touchOnNumaNode() would overwrite its first
		 * word, causing data corruption (see CMVC 175149 for example).
		 */
		MEMORY_BASIC_INFORMATION memInfo;
		SIZE_T queryResult = VirtualQuery((LPVOID)current, &memInfo, sizeof(memInfo));
		Assert_PRT_true(0 != queryResult);
		if ((0 != queryResult) && (0 == (memInfo.State & MEM_COMMIT))) {
			/* this range isn't committed yet: commit the range */
			uintptr_t commitBase = (uintptr_t)OMR_MAX(memInfo.BaseAddress, address);
			uintptr_t commitTop = OMR_MIN((uintptr_t)memInfo.BaseAddress + memInfo.RegionSize, top);
			LPVOID result = VirtualAlloc((LPVOID)commitBase, commitTop - commitBase, MEM_COMMIT | J9_EXTRA_TYPE_FLAGS, getProtectionBits(identifier->mode));

			if (NULL != result) {
				/* the commit succeeded so we just need to touch the memory with a thread on the correct node */
				J9AVLTree *boundExtents = &portLibrary->portGlobals->platformGlobals.bindingTree;
				void *thisAddress = (void *)commitBase;
				BindingNode *node = NULL;
				uintptr_t pageSizeInBytes = identifier->pageSize;

				omrthread_monitor_enter(PPG_bindingAccessMonitor);
				node = (BindingNode *)avl_search(boundExtents, (uintptr_t)thisAddress);

				Assert_PRT_true(NULL != node);
				while ((node != NULL) && ((uintptr_t)thisAddress < commitTop)) {
					void *nextAddress = (void *)OMR_MIN(commitTop, (uintptr_t)node->baseAddress + node->extentOfRangeInBytes);
					uintptr_t bytesToTouch = (uintptr_t)nextAddress - (uintptr_t)thisAddress;
					uintptr_t j9NodeNumber = node->j9NodeNumber;

					if (0 != j9NodeNumber) {
						touchOnNumaNode(portLibrary, pageSizeInBytes, thisAddress, bytesToTouch, j9NodeNumber);
					}
					/* we didn't need to touch the memory or the touch was successful so continue */
					thisAddress = nextAddress;
					node = (BindingNode *)avl_search(boundExtents, (uintptr_t)thisAddress);
				}
				omrthread_monitor_exit(PPG_bindingAccessMonitor);
			}
		}
		current = (uintptr_t)memInfo.BaseAddress + memInfo.RegionSize;
	}

	/* It's possible that some of the intermediate operations failed. Commit the whole range again and use the result of that the determine overall success. */
	finalResult = VirtualAlloc((LPVOID)address, (SIZE_T)byteAmount, MEM_COMMIT | J9_EXTRA_TYPE_FLAGS, getProtectionBits(identifier->mode));
	return finalResult;
}

/**
 * @internal
 * Touches every page of memory for byteAmount bytes, starting at address, in such a way that the pages should be allocated on the
 * given j9NumaNode.
 * @param portLibrary[in] The Port Library instance
 * @param pageSizeInBytes[in] The size of each page in the memory extent
 * @param address[in] The base address of the region of memory to be committed
 * @param byteAmount[in] The length, in bytes, of the virtual memory fragment starting at address
 * @param j9NumaNode[in] The node number to which the pages must be bound (1-indexed node number)
 */
static void
touchOnNumaNode(struct OMRPortLibrary *portLibrary, uintptr_t pageSizeInBytes, void *address, uintptr_t byteAmount, uintptr_t j9NumaNode)
{
	GetThreadIdealProcessorExType GetThreadIdealProcessorExProc = portLibrary->portGlobals->platformGlobals.GetThreadIdealProcessorExProc;
	SetThreadIdealProcessorExType SetThreadIdealProcessorExProc = portLibrary->portGlobals->platformGlobals.SetThreadIdealProcessorExProc;
	GetNumaNodeProcessorMaskExType GetNumaNodeProcessorMaskExProc = portLibrary->portGlobals->platformGlobals.GetNumaNodeProcessorMaskExProc;

	if ((NULL != GetThreadIdealProcessorExProc) && (NULL != SetThreadIdealProcessorExProc) && (NULL != GetNumaNodeProcessorMaskExProc)) {
		HANDLE thisThread = GetCurrentThread();
		PROCESSOR_NUMBER originalProc;

		if (GetThreadIdealProcessorExProc(thisThread, &originalProc)) {
			GROUP_AFFINITY processorMask;

			memset(&processorMask, 0x0, sizeof(processorMask));
			/* get the processors for the target node (remember the native layer is 0-indexed and we are 1-indexed) */
			if (GetNumaNodeProcessorMaskExProc((USHORT)(j9NumaNode - 1), &processorMask)) {
				PROCESSOR_NUMBER newProc;
				KAFFINITY groupAffinity = processorMask.Mask;
				BOOLEAN didBind = FALSE;
				BYTE proc = 0;
				BYTE index = 0;

				memcpy(&newProc, &originalProc, sizeof(newProc));
				/* find a processor in the node to which we can bind ourselves for the touch */
				while ((!didBind) && (0 != groupAffinity)) {
					if (0x1 == (0x1 & groupAffinity)) {
						/* bind to the processor */
						newProc.Number = index;
						if (SetThreadIdealProcessorExProc(thisThread, &newProc, NULL)) {
							didBind = TRUE;
						} else {
							/* we failed to bind which typically means that the core is parked so we will try another */
							Trc_PRT_vmem_touchOnNumaNode_failedToBindToComputeResource((uintptr_t)index, j9NumaNode, (uintptr_t)GetLastError());
						}
					}
					groupAffinity >>= 1;
					index += 1;
				}
				/* if we bound to the new processor, touch the memory */
				if (didBind) {
					void *top = (void *)((uintptr_t)address + byteAmount);
					void *loop = NULL;

					/* now that we have bound this thread to the node where we want to commit this, touch all the pages to force them to be allocated here */
					for (loop = address; loop < top; loop = (void *)((uintptr_t)loop + pageSizeInBytes)) {
						*((uintptr_t *)loop) = 0x0;
					}

					if (SetThreadIdealProcessorExProc(thisThread, &originalProc, NULL)) {
						/* success */
					} else {
						/* we failed to set our affinity back to what it just was - this failure should not ever be possible */
						Trc_PRT_vmem_touchOnNumaNode_failedToReturnToComputeResource((uintptr_t)originalProc.Number, (uintptr_t)newProc.Number, (uintptr_t)GetLastError());
					}
				} else {
					/* we weren't able to change to the given node so it is possible that all the cores on the node are parked so we will have to fall back to the system's default page allocation strategy */
					Trc_PRT_vmem_touchOnNumaNode_failedToBindToNodeForTouch(j9NumaNode);
				}
			} else {
				/* we weren't able to look up what node this is which implies that we were asking for an invalid node - this indicates a usage error somewhere in the caller */
				Assert_PRT_true(FALSE);
			}
		} else {
			/* we failed to read the thread's affinity which implies something is seriously wrong (the thread probably doesn't have any access rights so we can't be running any of this code) */
			Assert_PRT_true(FALSE);
		}
	}
}


/**
 * @internal
 * Used by the AVL tree "bindingTree" to place the given binding node in the tree.
 * @param tree[in] The AVL tree
 * @param insertNode[in] The node we are trying to insert
 * @param walkNode[in] The current node we are walking past
 * @return The location where insertNode should be stored relative to walkNode (1 for after, -1 for before, 0 for match)
 */
static intptr_t
rangeInsertionComparator(J9AVLTree *tree, BindingNode *insertNode, BindingNode *walkNode)
{
	uintptr_t insertBase = (uintptr_t)insertNode->baseAddress;
	uintptr_t walkBase = (uintptr_t)walkNode->baseAddress;
	uintptr_t comparisonResult = 0;

	if (insertBase != walkBase) {
		comparisonResult = (insertBase > walkBase) ? 1 : -1;
	}
	return comparisonResult;
}

/**
 * @internal
 * Used by the AVL tree "bindingTree" to find a given value's binding node in the tree.
 * @param tree[in] The AVL tree
 * @param value[in] The key we are searching for
 * @param searchNode[in] The current node we are comparing to the given value
 * @return The location where we would find the appropriate node relative to searchNode (1 for after, -1 for before, 0 for match)
 */
static intptr_t
rangeSearchComparator(J9AVLTree *tree, uintptr_t value, BindingNode *searchNode)
{
	uintptr_t numericalBaseAddress = (uintptr_t) searchNode->baseAddress;
	uintptr_t numericalTopAddress = numericalBaseAddress + searchNode->extentOfRangeInBytes;
	uintptr_t searchResult = 0;

	if ((value < numericalBaseAddress) || (value >= numericalTopAddress)) {
		searchResult = (value > numericalBaseAddress) ? 1 : -1;
	}
	return searchResult;
}

int32_t
omrvmem_get_available_physical_memory(struct OMRPortLibrary *portLibrary, uint64_t *freePhysicalMemorySize)
{
	MEMORYSTATUSEX statex;

	statex.dwLength = sizeof(statex);
	if (0 != GlobalMemoryStatusEx(&statex)) {
		*freePhysicalMemorySize = statex.ullAvailPhys;
		Trc_PRT_vmem_get_available_physical_memory_result(*freePhysicalMemorySize);
	} else {
		intptr_t lastError = (intptr_t)GetLastError();
		Trc_PRT_vmem_get_available_physical_memory_failed("GlobalMemoryStatusEx", lastError);
		return OMRPORT_ERROR_VMEM_OPFAILED;
	}
	return 0;
}

int32_t
omrvmem_get_process_memory_size(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize)
{
	int32_t result = OMRPORT_ERROR_VMEM_OPFAILED;
	Trc_PRT_vmem_get_process_memory_enter((int32_t)queryType);
	switch (queryType) {
	case OMRPORT_VMEM_PROCESS_PHYSICAL: {
		PROCESS_MEMORY_COUNTERS info;
		int64_t size = 0;

		info.cb = sizeof(info);
		if (0 != GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) {
			size = info.WorkingSetSize;
			if (size > 0) {
				*memorySize = size;
				result = 0;
			}
		} else {
			Trc_PRT_vmem_get_available_physical_memory_failed("GetProcessMemoryInfo failed", 0);
			result = OMRPORT_ERROR_VMEM_OPFAILED;
		}
		break;
	}
	case OMRPORT_VMEM_PROCESS_PRIVATE:
		result = getProcessPrivateMemorySize(portLibrary, queryType, memorySize);
		break;
	case OMRPORT_VMEM_PROCESS_VIRTUAL: {
		PROCESS_MEMORY_COUNTERS info;
		info.cb = sizeof(info);
		if (0 != GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info))) {
			*memorySize = info.PagefileUsage;
			result = 0;
		} else {
			Trc_PRT_vmem_get_available_physical_memory_failed("GetProcessMemoryInfo failed", 0);
			result = OMRPORT_ERROR_VMEM_OPFAILED;
		}
		break;
	}
	default:
		Trc_PRT_vmem_get_available_physical_memory_failed("invalid query", 0);
		result = OMRPORT_ERROR_VMEM_OPFAILED;
		break;
	}
	Trc_PRT_vmem_get_process_memory_exit(result, *memorySize);
	return result;
}

static int32_t
getProcessPrivateMemorySize(struct OMRPortLibrary *portLibrary, J9VMemMemoryQuery queryType, uint64_t *memorySize)
{
	PDH_HCOUNTER counter;
	PDH_STATUS status;
	PDH_FMT_COUNTERVALUE value;
	PDH_HQUERY query = NULL;
	PDH_COUNTER_PATH_ELEMENTS *pathElements = NULL;
	uintptr_t isCounterPathInitialized = 0;
	uintptr_t isQueryOpened = FALSE;
	uintptr_t isCounterAdded = FALSE;
	DWORD ret = 0;
	TCHAR *pathList = NULL;
	BOOL pdhSuccess = FALSE;
	int32_t rc = OMRPORT_ERROR_VMEM_OPFAILED;

	MUTEX_ENTER(PPG_managementDataLock);
	isCounterPathInitialized = PPG_managementCounterPathStatus;
	MUTEX_EXIT(PPG_managementDataLock);

	if (MANAGEMENT_COUNTER_USE_OLD_ALGORITHM == isCounterPathInitialized) {
		/* PDH family failed before, fall back to old impl immediately */
		goto OLD_IMPL;
	}

	if (ERROR_SUCCESS != PdhOpenQuery(NULL, 0, &query))	{
		goto OLD_IMPL;
	}

	isQueryOpened = TRUE;

	if (MANAGEMENT_COUNTER_PATH_UNINITIALIZED == isCounterPathInitialized) {
		/* pathElementSize is used to pre-allocate a buffer for PdhParseCounterPath. This is done
		 * for performance reason as normal API invocation needs two passes, first to acquire the space needed, then
		 * second pass to actually fetch the data. Per MSDN documentation, pathElement contains pointers to each
		 * individual string path elements of the structure. The actual strings are appended to the end of structure.
		 * Hence, the total memory required cannot exceed the size of the structure plus the size of maximum counter
		 * path allowed.
		 */
		DWORD pathElementsSize = PDH_MAX_COUNTER_PATH + sizeof(PDH_COUNTER_PATH_ELEMENTS);
		TCHAR *str = NULL;
		DWORD pathListSize = 0;
		DWORD pid = GetCurrentProcessId();

		/* First pass to obtain the requirement memory buffer size. Note, buffer cannot be pre-allocated as there
		 * is no upper bound to how much memory will be needed.*/
		if (PDH_MORE_DATA != PdhExpandWildCardPath(NULL, "\\Process(*)\\ID Process", pathList, &pathListSize, 0)) {
			goto CLEAN_UP;
		}

		pathList = portLibrary->mem_allocate_memory(portLibrary, pathListSize + 1, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_VM);

		if (NULL == pathList) {
			goto CLEAN_UP;
		}

		/* Get the list of counter paths */
		if (ERROR_SUCCESS != PdhExpandWildCardPath(NULL, "\\Process(*)\\ID Process", pathList, &pathListSize, 0)) {
			goto CLEAN_UP;
		}

		pathElements = (PDH_COUNTER_PATH_ELEMENTS *)portLibrary->mem_allocate_memory(portLibrary, pathElementsSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_VM);

		if (NULL == pathElements) {
			goto CLEAN_UP;
		}

		str = pathList;

		/* Searches through the obtained list of counter path for current instance */
		while ('\0' != *str) {
			/* fetching PID of the counter path*/
			if (ERROR_SUCCESS != PdhAddCounter(query, str, 0, &counter)) {
				str += strlen(str) + 1;
				continue;
			}

			isCounterAdded = TRUE;

			if (ERROR_SUCCESS != PdhCollectQueryData(query)) {
				PdhRemoveCounter(counter);
				isCounterAdded = FALSE;
				str += strlen(str) + 1;
				continue;
			}

			if (ERROR_SUCCESS != PdhGetFormattedCounterValue(counter, PDH_FMT_LONG, &ret, &value)) {
				PdhRemoveCounter(counter);
				isCounterAdded = FALSE;
				str += strlen(str) + 1;
				continue;
			}

			if (value.longValue == pid) {
				/* Found the instance corresponding to current process */
				PDH_COUNTER_PATH_ELEMENTS tmp;

				/* PdhParseCounterPath resets the pathElementSize value, needs to set it to actual size of buffer to avoid error */
				pathElementsSize = PDH_MAX_COUNTER_PATH + sizeof(PDH_COUNTER_PATH_ELEMENTS);

				/*
				 * Typical process is to invoke PdhParseCounterPath twice, first time to obtain the necessary buffer size.
				 * However, to avoid overhead of invoking the function twice, we pass a pre-allocated max size buffer.
				 */
				if (ERROR_SUCCESS != PdhParseCounterPath(str, pathElements, &pathElementsSize, 0)) {
					goto CLEAN_UP;
				}

				tmp.szMachineName = pathElements->szMachineName;
				tmp.szObjectName = pathElements->szObjectName;
				tmp.szInstanceName = pathElements->szInstanceName;
				tmp.szParentInstance = pathElements->szParentInstance;
				tmp.dwInstanceIndex = pathElements->dwInstanceIndex;
				tmp.szCounterName = "Private Bytes";

				/*
				 * Store the correct counter path into counterPath, and toggle path initialized bit to avoid
				 * necessary processing in subsequent invocation
				 */
				MUTEX_ENTER(PPG_managementDataLock);
				if (MANAGEMENT_COUNTER_PATH_UNINITIALIZED == PPG_managementCounterPathStatus) {
					DWORD counterPathSize = PDH_MAX_COUNTER_PATH;

					if (ERROR_SUCCESS != PdhMakeCounterPath(&tmp, PPG_managementCounterPath, &counterPathSize, 0)) {
						MUTEX_EXIT(PPG_managementDataLock);
						goto CLEAN_UP;
					}
					PPG_managementCounterPathStatus = MANAGEMENT_COUNTER_PATH_INITIALIZED;
				}
				MUTEX_EXIT(PPG_managementDataLock);
				/* Removing PID counter */
				PdhRemoveCounter(counter);
				isCounterAdded = FALSE;
				break;
			}

			/* Removing PID counter */
			PdhRemoveCounter(counter);
			isCounterAdded = FALSE;
			str += strlen(str) + 1;
		}
	}
	MUTEX_ENTER(PPG_managementDataLock);
	if (MANAGEMENT_COUNTER_PATH_INITIALIZED != PPG_managementCounterPathStatus) {
		MUTEX_EXIT(PPG_managementDataLock);
		goto CLEAN_UP;
	}
	/* Add the Private Bytes counter path */
	if (ERROR_SUCCESS != PdhAddCounter(query, (LPCSTR)PPG_managementCounterPath, 0, &counter)) {
		MUTEX_EXIT(PPG_managementDataLock);
		goto CLEAN_UP;
	}
	isCounterAdded = TRUE;
	MUTEX_EXIT(PPG_managementDataLock);

	status = PdhCollectQueryData(query);
	if ((status != PDH_INVALID_HANDLE) && (status != PDH_NO_DATA)) {
		if (ERROR_SUCCESS != PdhGetFormattedCounterValue(counter, PDH_FMT_LARGE, &ret, &value)) {
			goto CLEAN_UP;
		}

		if ((value.CStatus == PDH_CSTATUS_VALID_DATA) || (value.CStatus == PDH_CSTATUS_NEW_DATA)) {
			*memorySize = value.largeValue;
			pdhSuccess = TRUE;
			rc = 0;
		}
	}
CLEAN_UP:
	if (isCounterAdded) {
		PdhRemoveCounter(counter);
	}
	if (isQueryOpened) {
		PdhCloseQuery(query);
	}
	portLibrary->mem_free_memory(portLibrary, pathElements);
	portLibrary->mem_free_memory(portLibrary, pathList);

OLD_IMPL:
	if (!pdhSuccess) {
		int64_t size = 0;
		uintptr_t base = 0;
		MEMORY_BASIC_INFORMATION minfo;

		/* PDH family failed. Cache this fact for all subsequent invocation to directly pick the old impl */
		if (MANAGEMENT_COUNTER_USE_OLD_ALGORITHM != PPG_managementCounterPathStatus) {
			MUTEX_ENTER(PPG_managementDataLock);
			PPG_managementCounterPathStatus = MANAGEMENT_COUNTER_USE_OLD_ALGORITHM;
			MUTEX_EXIT(PPG_managementDataLock);
		}
		memset(&minfo, 0, sizeof(minfo));
		while (0 != VirtualQuery((LPCVOID) base, &minfo, sizeof(minfo))) {
			base += minfo.RegionSize;
			if ((MEM_COMMIT == minfo.State) && (MEM_PRIVATE == minfo.Type)) {
				size += minfo.RegionSize;
			}
		}
		if (size > 0) {
			*memorySize = size;
			rc = 0;
		}
	}
	return rc;
}
