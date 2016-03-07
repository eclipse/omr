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

#include "omrmem32helpers.h"
#include "omrport.h"
#include "omrportpg.h"
#include "ut_omrport.h"

static void *allocateVmemRegion32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, J9HeapWrapper **heapWrapper, const char *callSite, uint32_t memoryCategory, uintptr_t vmemMode, uintptr_t vmemAllocOptions);
static void prependHeapWrapper(struct OMRPortLibrary *portLibrary, J9HeapWrapper *heapWrapper);
static void updatePPGHeapSizeInfo(struct OMRPortLibrary *portLibrary, uintptr_t totalSizeDelta, BOOLEAN isIncrement);
static void *allocateLargeRegion(struct OMRPortLibrary *portLibrary, uintptr_t regionSize, const char *callSite, uintptr_t vmemAllocOptions);
static void *iterateHeapsAndSubAllocate(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount);
static J9HeapWrapper *findMatchingHeap(struct OMRPortLibrary *portLibrary, void *memoryPointer, J9HeapWrapper ***heapWrapperLocation);
static void *allocateRegion(struct OMRPortLibrary *portLibrary, uintptr_t regionSize, uintptr_t byteAmount, const char *callSite, uintptr_t vmemAllocOptions);
static void *reserveAndCommitRegion(struct OMRPortLibrary *portLibrary, uintptr_t reserveSize, const char *callSite, uintptr_t vmemAllocOptions);

#define VMEM_MODE_COMMIT OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE | OMRPORT_VMEM_MEMORY_MODE_COMMIT
#define VMEM_MODE_WITHOUT_COMMIT OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE

struct {
	uintptr_t base;
	uintptr_t limit;
} regions[] = {
	{0x0, 0xFFFFFFFF}
};

#define MEM32_LIMIT 0XFFFFFFFF

/* VMDESIGN 1761 The size of a suballocation heap.
 * See VMDESIGN 1761 for the rationale behind the selection of this size.
 * We use a 8MB heap to give us more room in case an application loads a larger amount of classes than usual.
 * For testing purposes, this value is mirrored in port library test. If we tune this value, we should also adjust it in omrmemTest.cpp
 */
#if defined(AIXPPC) && defined(OMR_GC_COMPRESSED_POINTERS)
/* virtual memory is allocated in 256M segments on AIX, so grab the whole segment */
#define HEAP_SIZE_BYTES 256*1024*1024
#else
#define HEAP_SIZE_BYTES 8*1024*1024
#endif
/* Creates any of the resources required to use allocate_memory32
 *
 * Note: Any resources created here need to be cleaned up in shutdown_memory32_using_vmem
 */
int32_t
startup_memory32(struct OMRPortLibrary *portLibrary)
{
	/* initialize fields in subAllocHeap32 */
	PPG_mem_mem32_subAllocHeapMem32.totalSize = 0;
	PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper = NULL;
	PPG_mem_mem32_subAllocHeapMem32.canSubCommitHeapGrow = TRUE;
	PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize = 0;
	PPG_mem_mem32_subAllocHeapMem32.subCommitHeapWrapper = NULL;
	PPG_mem_mem32_subAllocHeapMem32.suballocator_initialSize = 0;
	PPG_mem_mem32_subAllocHeapMem32.suballocator_commitSize = 0;

	/* initialize the monitor in subAllocHeap32 */
	if (0 != omrthread_monitor_init(&(PPG_mem_mem32_subAllocHeapMem32.monitor), 0)) {
		return (int32_t) OMRPORT_ERROR_STARTUP_MEM;
	}
	return 0;
}

/* Cleans up any resources created by startup_memory32 and during mem_allocate_memory32
 */
void
shutdown_memory32(struct OMRPortLibrary *portLibrary)
{
	J9HeapWrapper *heapWrapperCursor = NULL;
	J9HeapWrapper *currentHeapWrapper = NULL;
	J9PortVmemIdentifier *vmemID;

	if (NULL != portLibrary->portGlobals) {
		/* iterate through the list of heaps starting at
		 * subAllocHeap32.firstHeapWrapper and perform cleanup on each entry
		 */
		heapWrapperCursor = PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper;
		while (NULL != heapWrapperCursor) {
			currentHeapWrapper = heapWrapperCursor;
			heapWrapperCursor = heapWrapperCursor->nextHeapWrapper;

			vmemID = currentHeapWrapper->vmemID;

			if (! currentHeapWrapper->heap) {
				/* jmem double-accounting prevention: Increment the memory counter so it can be decremented again inside vmem_free_memory */
				omrmem_categories_increment_counters(vmemID->category, vmemID->size);
			}

			/* free the vmem for each heap */
			portLibrary->vmem_free_memory(portLibrary, vmemID->address, vmemID->size, vmemID);
			/* free the vmemID */
			portLibrary->mem_free_memory(portLibrary, vmemID);
			/* free the heapWrapper struct */
			portLibrary->mem_free_memory(portLibrary, currentHeapWrapper);
		}

		/* destroy the monitor in subAllocHeap32 */
		omrthread_monitor_destroy(PPG_mem_mem32_subAllocHeapMem32.monitor);
	}
}

/* prepend the heapWrapper to the list in OMRPortPlatformGlobals.J9SubAllocateHeap32 */
static void
prependHeapWrapper(struct OMRPortLibrary *portLibrary, J9HeapWrapper *heapWrapper)
{
	heapWrapper->nextHeapWrapper = PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper;
	PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper = heapWrapper;
}

/* Adjust the totalSize in OMRPortPlatformGlobals.J9SubAllocateHeap32 based on boolean value isIncrement.
 * Increment the size info if true and decrement if isIncrement is false.
 */
static void
updatePPGHeapSizeInfo(struct OMRPortLibrary *portLibrary, uintptr_t totalSizeDelta, BOOLEAN isIncrement)
{
	if (isIncrement) {
		PPG_mem_mem32_subAllocHeapMem32.totalSize += totalSizeDelta;
	} else {
		PPG_mem_mem32_subAllocHeapMem32.totalSize -= totalSizeDelta;
	}
}

/* vmem alloc the large size requested and don't treat it as a J9Heap */
static void *
allocateLargeRegion(struct OMRPortLibrary *portLibrary, uintptr_t regionSize, const char *callSite, uintptr_t vmemAllocOptions)
{
	J9HeapWrapper *heapWrapper = NULL;
	void *allocPtr = NULL;
	uintptr_t pageSize = portLibrary->vmem_supported_page_sizes(portLibrary)[0];
	uintptr_t roundedRegionSize = regionSize;

	/* Round up the requested size to the page size */
	roundedRegionSize = pageSize * (regionSize / pageSize);
	if (roundedRegionSize < regionSize) {
		roundedRegionSize += pageSize;
	}

	/* reserve and commit the memory */
	allocPtr = allocateVmemRegion32(portLibrary, roundedRegionSize, &heapWrapper, callSite, OMRMEM_CATEGORY_PORT_LIBRARY, VMEM_MODE_COMMIT, vmemAllocOptions);

	if (NULL != allocPtr) {
		prependHeapWrapper(portLibrary, heapWrapper);
		updatePPGHeapSizeInfo(portLibrary, roundedRegionSize, TRUE);
		Trc_PRT_mem_allocate_memory32_alloc_large_region(allocPtr, roundedRegionSize);

		/* The memory category for the user of this memory will be incremented in omrmemtag.c. To prevent double-accounting,
		 * we must reset the port library counter.
		 */
		omrmem_categories_decrement_counters(heapWrapper->vmemID->category, roundedRegionSize);
	} else {
		Trc_PRT_mem_allocate_memory32_alloc_large_region_failed_2(callSite, roundedRegionSize);
	}
	return allocPtr;
}

/* attemp suballocation on each omrheap found in the chain untill there is a success, otherwise return NULL */
static void *
iterateHeapsAndSubAllocate(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount)
{
	J9HeapWrapper *heapWrapperCursor = NULL;

	/* traverse the linked list of J9HeapWrapper starting at J9SubAllocateHeap32.firstHeapWrapper */
	heapWrapperCursor = PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper;
	while (NULL != heapWrapperCursor) {
		J9Heap *heap = heapWrapperCursor->heap;

		/* heap field is non-null, we have a valid J9Heap to attempt our suballocation */
		if (NULL != heap) {
			void *subAllocPtr = portLibrary->heap_allocate(portLibrary, heap, byteAmount);
			if (NULL != subAllocPtr) {
				Trc_PRT_mem_allocate_memory32_suballoc_block(subAllocPtr, heap, byteAmount);

				/* omrmem category double-accounting prevention: subtract the amount allocated from the unused category */
				omrmem_categories_decrement_bytes(omrmem_get_category(portLibrary, OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS), byteAmount);

				return subAllocPtr;
			} else {
				Trc_PRT_mem_allocate_memory32_suballoc_block_failed(heap, byteAmount);
			}
		}
		heapWrapperCursor = heapWrapperCursor->nextHeapWrapper;
	}

	/**
	 * If we are here, then there is not enough space in the existing heapWrappers to satisfy the requested byteAmount.
	 * Then check the subCommitHeapWrapper whether it can grow or not.
	 * If it can grow, meaning not all of the initially reserved memory is committed,
	 * then commit more memory from the reserved memory, grow the heap and try to allocate byteAmount.
	 * If the memory is allocated successfully after growing the heap, then return the memory address of allocated memory,
	 * otherwise, return NULL.
	 *
	 */
	if ((NULL != PPG_mem_mem32_subAllocHeapMem32.subCommitHeapWrapper) &&
		(TRUE == PPG_mem_mem32_subAllocHeapMem32.canSubCommitHeapGrow)) {
		void *commitPtr = NULL;
		J9Heap *heap = NULL;
		uintptr_t commitSize = PPG_mem_mem32_subAllocHeapMem32.suballocator_commitSize;
		uintptr_t remainingSize = PPG_mem_mem32_subAllocHeapMem32.suballocator_initialSize - PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize;

		heap = PPG_mem_mem32_subAllocHeapMem32.subCommitHeapWrapper->heap;

		/* Make sure commitSize is not bigger than remaining size in the reserved memory*/
		if (commitSize > remainingSize) {
			commitSize = remainingSize;
		}
		/* commit the memory from the end of the last committed chunk,
		 * not the end address of the heap since it is probably different cause of rounding down in heap creation.
		 */
		commitPtr = (void *)(((uintptr_t)PPG_mem_mem32_subAllocHeapMem32.subCommitHeapWrapper->vmemID->address) + PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize);
		commitPtr = omrvmem_commit_memory(portLibrary, commitPtr, commitSize, PPG_mem_mem32_subAllocHeapMem32.subCommitHeapWrapper->vmemID);

		if (NULL == commitPtr) {
			PPG_mem_mem32_subAllocHeapMem32.canSubCommitHeapGrow = FALSE;
			Trc_PRT_mem_iterateHeapsAndSubAllocate_commitMemoryFailure(commitPtr, commitSize, PPG_mem_mem32_subAllocHeapMem32.subCommitHeapWrapper->vmemID);
			return NULL;
		}


		/* If memory is committed successfully, then update the value of committedSize */
		PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize += commitSize;

		if (PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize ==  PPG_mem_mem32_subAllocHeapMem32.suballocator_initialSize) {
			PPG_mem_mem32_subAllocHeapMem32.canSubCommitHeapGrow = FALSE;
		}

		if (omrheap_grow(portLibrary, heap, commitSize)) {
			void *subAllocPtr = portLibrary->heap_allocate(portLibrary, heap, byteAmount);
			Trc_PRT_mem_iterateHeapsAndSubAllocate_heap_grow_successful(heap, commitSize);
			updatePPGHeapSizeInfo(portLibrary, commitSize, TRUE);
			if (NULL != subAllocPtr) {
				Trc_PRT_mem_allocate_memory32_suballoc_block(subAllocPtr, heap, byteAmount);
				/* omrmem category double-accounting prevention: subtract the amount allocated from the unused category */
				omrmem_categories_decrement_bytes(omrmem_get_category(portLibrary, OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS), byteAmount);
				return subAllocPtr;
			} else {
				Trc_PRT_mem_allocate_memory32_suballoc_block_failed(heap, byteAmount);
			}
		} else {
			Trc_PRT_mem_iterateHeapsAndSubAllocate_heap_grow_failed(heap, commitSize);
		}
	}
	Trc_PRT_mem_iterateHeapsAndSubAllocate_failed_to_suballoc(byteAmount);
	return NULL;
}

/* The memory will be allocated using vmem, and an attempt will be made to suballocat byteAmount within that memory.
 * If the overhead of omrheap precludes using suballocation, omrheap will not be used and the memory will be used directly instead.
 */
static void *
allocateRegion(struct OMRPortLibrary *portLibrary, uintptr_t regionSize, uintptr_t byteAmount, const char *callSite, uintptr_t vmemAllocOptions)
{
	void *alloc32Ptr = NULL;
	J9HeapWrapper *heapWrapper = NULL;
	J9Heap *omrheap = NULL;
	void *subAllocPtr = NULL;
	void *returnPtr = NULL;
	uintptr_t pageSize = portLibrary->vmem_supported_page_sizes(portLibrary)[0];
	uintptr_t roundedRegionSize = regionSize;

	/* Round up the requested size to the page size */
	roundedRegionSize = pageSize * (regionSize / pageSize);
	if (roundedRegionSize < regionSize) {
		roundedRegionSize += pageSize;
	}

	/* the existing J9Heap in chain cannot satisfy the request,
	 * allocate and create a new roundedRegionSize bytes J9Heap and attempt the request again
	 */
	alloc32Ptr = allocateVmemRegion32(portLibrary, roundedRegionSize, &heapWrapper, callSite, OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS, VMEM_MODE_COMMIT, vmemAllocOptions);
	if (NULL == alloc32Ptr) {
		Trc_PRT_mem_allocate_memory32_alloc_normal_region_failed_2(callSite, roundedRegionSize);
		return NULL;
	}

	/* initialize the memory as a J9Heap */
	omrheap = portLibrary->heap_create(portLibrary, alloc32Ptr, roundedRegionSize, 0);
	/* assertion to check the omrheap is not NULL */
	Assert_PRT_true(omrheap != NULL);
	subAllocPtr = portLibrary->heap_allocate(portLibrary, omrheap, byteAmount);

	if (NULL != subAllocPtr) {
		/* if the suballocation attempt on the newly created heap is successful,
		 * store the heap handle in J9HeapWrapper and increment the size info.
		 */
		heapWrapper->heap = omrheap;
		returnPtr = subAllocPtr;

		/* omrmem_category double-accounting prevention: subtract allocated amount from unused category */
		omrmem_categories_decrement_bytes(omrmem_get_category(portLibrary, OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS), byteAmount);

		Trc_PRT_mem_allocate_memory32_alloc_normal_region_suballoc(alloc32Ptr, omrheap, returnPtr, byteAmount);
	} else {
		/* if the byteAmount requested is so close to roundedRegionSize that suballocate byteAmount in the omrheap
		 * cannot be satisfied due to omrheap management overhead and roudning effect, we simply return the vmem allocated
		 * pointer and not treat it as a J9Heap.
		 */
		/* no need to set heapWrapper->heap = NULL since it is already done in allocateVmemRegion32 */
		returnPtr = alloc32Ptr;

		/* Need to fix-up the category so the block is associated with port library, not the allocate32 slab */
		omrmem_categories_decrement_counters(heapWrapper->vmemID->category, heapWrapper->vmemID->size);
		heapWrapper->vmemID->category = omrmem_get_category(portLibrary, OMRMEM_CATEGORY_PORT_LIBRARY);
		/* Don't increment the category for port library - this category will be incremented for the user in omrmemtag.c */

		Trc_PRT_mem_allocate_memory32_alloc_normal_region_consumed(returnPtr, byteAmount);
	}

	updatePPGHeapSizeInfo(portLibrary, roundedRegionSize, TRUE);

	/* prepend the heapWrapper to the list in OMRPortPlatformGlobals.J9SubAllocateHeap32 */
	prependHeapWrapper(portLibrary, heapWrapper);
	return returnPtr;
}
/**
 * Attempt to reserve the memory and if it is successfully reserved,
 * then attempt to commit requested chunk of it.
 *
 * @param[in] portLibrary The port library
 * @param[in] reserveSize Amount of memory to reserve.
 * @param[in] commitSize Amount of memory to commit.
 * @param[in] callSite caller of this function.
 *
 * @return the address of the reserved memory.
 *
 */
static void *
reserveAndCommitRegion(struct OMRPortLibrary *portLibrary, uintptr_t reserveSize, const char *callSite, uintptr_t vmemAllocOptions)
{
	/* Initialize the local variables */
	void *reserve32Ptr = NULL;
	J9HeapWrapper *heapWrapper = NULL;
	J9Heap *omrheap = NULL;
	uintptr_t commitSize = PPG_mem_mem32_subAllocHeapMem32.suballocator_commitSize;
	uintptr_t pageSize = portLibrary->vmem_supported_page_sizes(portLibrary)[0];

	/* Round up the initial size to the page size and set it to global variable */
	uintptr_t roundedInitialSize = pageSize * (reserveSize / pageSize);
	if (roundedInitialSize < reserveSize) {
		roundedInitialSize += pageSize;
	}

	PPG_mem_mem32_subAllocHeapMem32.suballocator_initialSize = roundedInitialSize;

	/* If commitSize is zero, then commit the all reserved memory at once. */
	if (0 == commitSize) {
		commitSize = roundedInitialSize;
	} else {
		Assert_PRT_true(roundedInitialSize >= commitSize);
	}

	/* Attempt to reserve memory */
	reserve32Ptr = allocateVmemRegion32(portLibrary, roundedInitialSize, &heapWrapper, callSite, OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS, VMEM_MODE_WITHOUT_COMMIT, vmemAllocOptions);
	/* If reserving memory has failed, then do not continue and return NULL */
	if (NULL == reserve32Ptr) {
		Trc_PRT_mem_reserveAndCommitRegion_reserveMemoryFailure(roundedInitialSize);
		return NULL;
	}

	/* Attempt to commit the memory */
	reserve32Ptr = omrvmem_commit_memory(portLibrary, reserve32Ptr, commitSize, heapWrapper->vmemID);
	/* If committing part/all of the reserved memory has failed, then do not continue and return NULL */
	if (NULL == reserve32Ptr) {
		PPG_mem_mem32_subAllocHeapMem32.canSubCommitHeapGrow = FALSE;
		Trc_PRT_mem_reserveAndCommitRegion_commitMemoryFailure(reserve32Ptr, commitSize, heapWrapper->vmemID);
		return NULL;
	}

	/* PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize should be zero here,
	 * since this function is only called from ensure_capacity32
	 * which is called only on startup.
	 */
	PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize = commitSize;
	/* Create a heap for the committed junk of the reserved memory */
	omrheap = portLibrary->heap_create(portLibrary, reserve32Ptr, commitSize, 0);

	/* assertion to check the omrheap is not NULL */
	Assert_PRT_true(omrheap != NULL);
	heapWrapper->heap = omrheap;
	/* Initialize the global 'subCommitHeapWrapper' */
	PPG_mem_mem32_subAllocHeapMem32.subCommitHeapWrapper = heapWrapper;
	/* If there is no space left to grow heap, then set the global 'canSubCommitHeapGrow' to FALSE */
	if (roundedInitialSize == commitSize) {
		PPG_mem_mem32_subAllocHeapMem32.canSubCommitHeapGrow = FALSE;
	} else {
		PPG_mem_mem32_subAllocHeapMem32.canSubCommitHeapGrow = TRUE;
	}
	updatePPGHeapSizeInfo(portLibrary, commitSize, TRUE);

	/* prepend the heapWrapper to the list in OMRPortPlatformGlobals.J9SubAllocateHeap32 */
	prependHeapWrapper(portLibrary, heapWrapper);
	return reserve32Ptr;
}

/* Allocates memory below the 4GB boundary.
 *
 * Uses vmem as underlying storage and j9Heap API for the suballocation on the underlying storage.
 */
void *
allocate_memory32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, const char *callSite)
{
	void *returnPtr = NULL;
#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	BOOLEAN useMalloc31 = FALSE;
#endif

	Trc_PRT_mem_allocate_memory32_Entry(byteAmount);
#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	if (OMRPORT_DISABLE_ENSURE_CAP32 == portLibrary->portGlobals->disableEnsureCap32) {
		useMalloc31 = TRUE;
	}
	if (TRUE == useMalloc31) {
		returnPtr = __malloc31(byteAmount);
	} else {
#endif
		omrthread_monitor_enter(PPG_mem_mem32_subAllocHeapMem32.monitor);

		/* Check if byteAmout is larger than HEAP_SIZE_BYTES.
		 * The majority of size requests will typically be much smaller.
		 */
		returnPtr = iterateHeapsAndSubAllocate(portLibrary, byteAmount);
		if (NULL == returnPtr) {
			if (byteAmount >= HEAP_SIZE_BYTES) {
				returnPtr = allocateLargeRegion(portLibrary, byteAmount, callSite, 0);
			} else {
				returnPtr = allocateRegion(portLibrary, HEAP_SIZE_BYTES, byteAmount, callSite, 0);
			}
		}

		omrthread_monitor_exit(PPG_mem_mem32_subAllocHeapMem32.monitor);
#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	}
#endif

	Trc_PRT_mem_allocate_memory32_Exit(returnPtr);
	return returnPtr;
}
/* Ensure that we have byteAmount of memory available in the sub-allocator.
 *
 */
uintptr_t
ensure_capacity32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount)
{
	J9HeapWrapper *heapWrapperCursor = NULL;
	uintptr_t returnValue = OMRPORT_ENSURE_CAPACITY_FAILED;
#if defined(OMR_ENV_DATA64)
	/* For 64 bit os, use flag OMRPORT_VMEM_ALLOC_QUICK as it is in the startup period. */
	uintptr_t vmemAllocOptions = OMRPORT_VMEM_ALLOC_QUICK;
#else
	uintptr_t vmemAllocOptions = 0;
#endif

	Trc_PRT_mem_ensure_capacity32_Entry(byteAmount);

#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	if (OMRPORT_DISABLE_ENSURE_CAP32 == portLibrary->portGlobals->disableEnsureCap32) {
		Trc_PRT_mem_ensure_capacity32_NotRequired_Exit();
		return OMRPORT_ENSURE_CAPACITY_NOT_REQUIRED;
	}
#endif

	/* Ensured byte amount should be at least HEAP_SIZE_BYTES large */
	if (byteAmount < HEAP_SIZE_BYTES) {
		byteAmount = HEAP_SIZE_BYTES;
	}

	omrthread_monitor_enter(PPG_mem_mem32_subAllocHeapMem32.monitor);

	/* check if we have already allocated a heap for byteAmount */
	heapWrapperCursor = PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper;
	while (NULL != heapWrapperCursor) {
		if (heapWrapperCursor->heapSize >= byteAmount && heapWrapperCursor->heap) {
			returnValue = OMRPORT_ENSURE_CAPACITY_SUCCESS;
			Trc_PRT_mem_ensure_capacity32_already_initialized(heapWrapperCursor->heap, heapWrapperCursor->heapSize);
			break;
		}
		heapWrapperCursor = heapWrapperCursor->nextHeapWrapper;
	}

	/* allocate a omrheap of byteAmount bytes */
	if (0 == returnValue) {
		if (0 == PPG_mem_mem32_subAllocHeapMem32.subCommitCommittedMemorySize) {
			Trc_PRT_mem_ensure_capacity32_uninitialized();
			/* Reserve a memory and commit requested chunk of it */
			returnValue = (uintptr_t)reserveAndCommitRegion(portLibrary, byteAmount, OMR_GET_CALLSITE(), vmemAllocOptions);
		} else {
			returnValue = (uintptr_t)allocateRegion(portLibrary, byteAmount, 0, OMR_GET_CALLSITE(), vmemAllocOptions);
		}
	}

	omrthread_monitor_exit(PPG_mem_mem32_subAllocHeapMem32.monitor);

	Trc_PRT_mem_ensure_capacity32_Exit(returnValue);

	return returnValue;
}

/**
 * Allocates a chunk of memory with vmem in the low 32 bit using a brute force search.
 * If the underlying vmem allocation is successful, allocate a J9HeapWrapper and initialize its fields.
 */
static void *
allocateVmemRegion32(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount, J9HeapWrapper **heapWrapper, const char *callSite, uint32_t memoryCategory, uintptr_t vmemMode, uintptr_t vmemAllocOptions)
{
	void *pointer = NULL;

	/* If the port library does not support OMR_PORT_CAPABILITY_CAN_RESERVE_SPECIFIC_ADDRESS capability
	 * on this platform, allocateVmemRegion32 should return NULL, as omrvmem won't be able to
	 * satisfy the request for a specific address (it might get lucky, but we don't want to rely on that)
	 */
#if defined(OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS)
	J9PortVmemIdentifier *vmemID = NULL;
	uintptr_t pageSize = 0;
	uintptr_t i = 0;
	J9HeapWrapper *wrapper = NULL;

	if (0 == byteAmount)	{
		/* prevent malloc from failing causing allocate to return null */
		byteAmount = 1;
	}

	/* Create the vmemidentifier */
	vmemID = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9PortVmemIdentifier), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == vmemID) {
		Trc_PRT_mem_allocate_memory32_failed_vmemID(callSite);
		return NULL;
	}

	/* Create the heap wrapper */
	wrapper = portLibrary->mem_allocate_memory(portLibrary, sizeof(J9HeapWrapper), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == wrapper) {
		Trc_PRT_mem_allocate_memory32_failed_heapWrapper(callSite);
		portLibrary->mem_free_memory(portLibrary, vmemID);
		return NULL;
	}

	/* get the default page size */
	pageSize = portLibrary->vmem_supported_page_sizes(portLibrary)[0];
	if (0 == pageSize) {
		Trc_PRT_mem_allocate_memory32_failed_page(callSite);
		portLibrary->mem_free_memory(portLibrary, vmemID);
		portLibrary->mem_free_memory(portLibrary, wrapper);
		return NULL;
	}

	/* use a minimum 4K page size */
	if (pageSize < 0x1000) {
		pageSize = 0x1000;
	}

	/* iterate through the available regions */
	for (i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
		J9PortVmemParams params;
		uint8_t *lowAddress = NULL;
		uintptr_t limit = 0;

		/* set the lower and upper bounds for this region */
		lowAddress = (regions[i].base != 0) ? (uint8_t *)regions[i].base : (uint8_t *)pageSize;

		/* Check whether there is enough space to allocate in this region */
		if ((regions[i].limit - (uintptr_t)lowAddress) < byteAmount) {
			continue;
		}
		limit = regions[i].limit - byteAmount;

		portLibrary->vmem_vmem_params_init(portLibrary, &params);
		params.startAddress = (void *)lowAddress;
		params.endAddress = (void *)limit;
		params.byteAmount = byteAmount;
		params.pageSize = pageSize;
		params.mode = vmemMode;
		params.options = OMRPORT_VMEM_STRICT_ADDRESS | vmemAllocOptions;
		params.category = memoryCategory;

		pointer = portLibrary->vmem_reserve_memory_ex(portLibrary, vmemID, &params);

		/* stop searching if we found the memory that we wanted */
		if (pointer != NULL) {
			break;
		}
	}

	if (NULL == pointer) {
		portLibrary->mem_free_memory(portLibrary, vmemID);
		portLibrary->mem_free_memory(portLibrary, wrapper);
		Trc_PRT_mem_allocate_memory32_failed_alloc(byteAmount, callSite);
		return NULL;
	}

	/* initialize the J9HeapWrapper struct */
	wrapper->nextHeapWrapper = NULL;
	wrapper->heap = NULL;
	wrapper->heapSize = byteAmount;
	wrapper->vmemID = vmemID;

	*heapWrapper = wrapper;
#endif /* defined(OMR_PORT_CAN_RESERVE_SPECIFIC_ADDRESS) */

	return pointer;
}

/* Go through the list of heaps to find a J9HeapWrapper that contains memoryPointer */
static J9HeapWrapper *
findMatchingHeap(struct OMRPortLibrary *portLibrary, void *memoryPointer, J9HeapWrapper ***heapWrapperLocation)
{
	J9HeapWrapper *currentHeapWrapper = NULL;
	J9HeapWrapper *foundHeapWrapper = NULL;

	/* iterate through the J9HeapWrapper list and look for the entry associated with memoryPointer */
	currentHeapWrapper = PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper;
	*heapWrapperLocation = &(PPG_mem_mem32_subAllocHeapMem32.firstHeapWrapper);

	while (NULL != currentHeapWrapper) {
		J9PortVmemIdentifier *vmemID = currentHeapWrapper->vmemID;
		uint8_t *regionStart = (uint8_t *)vmemID->address;
		uint8_t *regionEnd = regionStart + (vmemID->size);

		/* we found the belonging heap if memoryPointer falls in the range of vmem allocated for the heap */
		if ((((uint8_t *)memoryPointer) >= regionStart) && (((uint8_t *)memoryPointer) < regionEnd)) {
			foundHeapWrapper = currentHeapWrapper;
			break;
		}

		*heapWrapperLocation = &(currentHeapWrapper->nextHeapWrapper);
		currentHeapWrapper = currentHeapWrapper->nextHeapWrapper;
	}

	/* we cannot find a containing J9HeapWrapper for memoryPointer, simply exit */
	if (NULL == foundHeapWrapper) {
		Trc_PRT_mem_free_memory32_no_matching_heap_found(memoryPointer);
	}

	return foundHeapWrapper;
}

/* Frees memory that was allocated below the 4GB boundary using mem_allocate_memory32 */
void
free_memory32(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	J9HeapWrapper *foundHeapWrapper = NULL;
	J9HeapWrapper **heapWrapperUpdate = NULL;

	Trc_PRT_mem_free_memory32_Entry(memoryPointer);

#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	if (OMRPORT_DISABLE_ENSURE_CAP32 == portLibrary->portGlobals->disableEnsureCap32) {
		free(memoryPointer);
	} else {
#endif
		omrthread_monitor_enter(PPG_mem_mem32_subAllocHeapMem32.monitor);

		if ((foundHeapWrapper = findMatchingHeap(portLibrary, memoryPointer, &heapWrapperUpdate)) != NULL) {
			J9Heap *omrheap = foundHeapWrapper->heap;
			if (NULL == omrheap) {
				uintptr_t heapSize = foundHeapWrapper->heapSize;
				J9PortVmemIdentifier *vmemID = foundHeapWrapper->vmemID;

				Trc_PRT_mem_free_memory32_found_vmem_heap(vmemID->address);

				/* jmem double-accounting prevention: Increment the memory counter so it can be decremented again inside vmem_free_memory */
				omrmem_categories_increment_counters(vmemID->category, vmemID->size);

				/* a null heap field in J9HeapWrapper means this is not a suballocating J9Heap,
				 * so we call vmem_free_memory to free the underlying vmem.
				 */
				portLibrary->vmem_free_memory(portLibrary, vmemID->address, vmemID->size, vmemID);

				/* now remove the J9HeapWrapper entry from the list */
				*heapWrapperUpdate = foundHeapWrapper->nextHeapWrapper;

				/* free all malloc'ed structs */
				portLibrary->mem_free_memory(portLibrary, vmemID);
				portLibrary->mem_free_memory(portLibrary, foundHeapWrapper);

				updatePPGHeapSizeInfo(portLibrary, heapSize, FALSE);
			} else {
				uintptr_t allocationSize;
				Trc_PRT_mem_free_memory32_found_omrheap(omrheap);

				/* omrmem_category double-accounting prevention: add the size of the block to the "unused" category */
				allocationSize = portLibrary->heap_query_size(portLibrary, omrheap, memoryPointer);
				omrmem_categories_increment_bytes(omrmem_get_category(portLibrary, OMRMEM_CATEGORY_PORT_LIBRARY_UNUSED_ALLOCATE32_REGIONS), allocationSize);

				/* we are freeing a suballocated block from a J9Heap */
				portLibrary->heap_free(portLibrary, omrheap, memoryPointer);
			}
		}

		omrthread_monitor_exit(PPG_mem_mem32_subAllocHeapMem32.monitor);

#if defined(J9ZOS390) && defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	}
#endif

	Trc_PRT_mem_free_memory32_Exit();
}
