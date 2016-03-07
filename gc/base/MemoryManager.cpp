/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 ******************************************************************************/


#include "MemoryManager.hpp"

#include "ModronAssertions.h"
#include "GCExtensionsBase.hpp"
#include "NonVirtualMemory.hpp"

MM_MemoryManager*
MM_MemoryManager::newInstance(MM_EnvironmentBase* env)
{
	MM_MemoryManager* memoryManager = (MM_MemoryManager*)env->getForge()->allocate(sizeof(MM_MemoryManager), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());

	if (NULL != memoryManager) {
		new (memoryManager) MM_MemoryManager(env);
		if (!memoryManager->initialize(env)) {
			memoryManager->kill(env);
			memoryManager = NULL;
		}
	}

	return memoryManager;
}

void
MM_MemoryManager::kill(MM_EnvironmentBase* env)
{
	env->getForge()->free(this);
}

bool
MM_MemoryManager::initialize(MM_EnvironmentBase* env)
{
	return true;
}

bool
MM_MemoryManager::createVirtualMemoryForHeap(MM_EnvironmentBase* env, MM_MemoryHandle* handle, uintptr_t heapAlignment, uintptr_t size, uintptr_t tailPadding, void* preferredAddress, void* ceiling)
{
	Assert_MM_true(NULL != handle);
	MM_GCExtensionsBase* extensions = env->getExtensions();

	MM_VirtualMemory* instance = NULL;
	uintptr_t mode = (OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE);
	uintptr_t options = 0;
	uint32_t memoryCategory = OMRMEM_CATEGORY_MM_RUNTIME_HEAP;

	uintptr_t pageSize = extensions->requestedPageSize;
	uintptr_t pageFlags = extensions->requestedPageFlags;
	Assert_MM_true(0 != pageSize);

	uintptr_t allocateSize = size;
	if (heapAlignment > pageSize) {
		allocateSize += (heapAlignment - pageSize);
	}

#if defined(OMR_GC_MODRON_SCAVENGER)
	if (extensions->enableSplitHeap) {
		/* currently (ceiling != NULL) is using to recognize CompressedRefs so must be NULL for 32 bit platforms */
		Assert_MM_true(NULL == ceiling);

		switch(extensions->splitHeapSection) {
		case MM_GCExtensionsBase::HEAP_INITIALIZATION_SPLIT_HEAP_TENURE:
			/* trying to get Tenure at the bottom of virtual memory */
			options |= OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP;
			break;
		case MM_GCExtensionsBase::HEAP_INITIALIZATION_SPLIT_HEAP_NURSERY:
			/* trying to get Nursery at the top of virtual memory */
			options |= OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN;
			break;
		case MM_GCExtensionsBase::HEAP_INITIALIZATION_SPLIT_HEAP_UNKNOWN:
		default:
			Assert_MM_unreachable();
			break;
		}
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	if (NULL == ceiling) {
		instance = MM_VirtualMemory::newInstance(env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
												 ceiling, mode, options, memoryCategory);
	} else {
#if defined(OMR_GC_COMPRESSED_POINTERS)
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		/*
		 * This function is used for Compressed References platforms only
		 * The ceiling for such platforms is set maximum memory value be supported (32G for 3-bit shift)
		 * The ceiling it is 0 for all other platforms
		 */
		/* NON_SCALING_LOW_MEMORY_HEAP_CEILING is set to 4G for 64-bit platforms only, 0 for 32-bit platforms */
		Assert_MM_true(NON_SCALING_LOW_MEMORY_HEAP_CEILING > 0);
		
		bool allocationTopDown = true;

		/*
		 *	Allocate option section
		 */
		options |= OMRPORT_VMEM_STRICT_ADDRESS | OMRPORT_VMEM_ALLOC_QUICK;

		/* define the scan direction when reserving the GC heap in the range of (4G, 32G) */
#if defined(S390) || defined(J9ZOS390)
		/* s390 benefits from smaller shift values so allocate direction is bottom up */
		options |= OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP;
		allocationTopDown = false;
#else
		options |= OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN;
#endif /* defined(S390) || defined(J9ZOS390) */

#if defined(J9ZOS39064)
		/* 2TO32G area is extended to 64G */
		options |= OMRPORT_VMEM_ZOS_USE2TO32G_AREA;
#endif /* defined(J9ZOS39064) */

		/*
		 * Usually the suballocator memory should be allocated first (before heap) however
		 * in case when preferred address is specified we will try to allocate heap first
		 * to avoid possible interference with requested heap location
		 */
		bool shouldHeapBeAllocatedFirst = (NULL != preferredAddress);

		void* startAllocationAddress = preferredAddress;

#if defined(J9ZOS39064)
		/*
		 * On ZOS an address space below 2G can not be taken for virtual memory
		 */
#define TWO_GB_ADDRESS ((void*)((uintptr_t)2 * 1024 * 1024 * 1024))
		if (NULL == preferredAddress) {
			startAllocationAddress = TWO_GB_ADDRESS;
		}
#endif /* defined(J9ZOS39064) */

		void* requestedTopAddress = (void*)((uintptr_t)startAllocationAddress + allocateSize + tailPadding);

		if (requestedTopAddress <= ceiling) {
			/* Set the commit size for the sub allocator. This needs to be completed before the call to omrmem_ensure_capacity32 */
			omrport_control(OMRPORT_CTLDATA_ALLOCATE32_COMMIT_SIZE, extensions->suballocatorCommitSize);

			if (!shouldHeapBeAllocatedFirst) {
				if (OMRPORT_ENSURE_CAPACITY_FAILED == omrmem_ensure_capacity32(extensions->suballocatorInitialSize)) {
					extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_ALLOCATE_LOW_MEMORY_RESERVE;
					return false;
				}
			}

			if (allocationTopDown && extensions->shouldForceSpecifiedShiftingCompression) {
				/* force to allocate heap top-down from correspondent to shift address */
				void* maxAddress = (void *)(((uintptr_t)1 << 32) << extensions->forcedShiftingCompressionAmount);

				instance = MM_VirtualMemory::newInstance(env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
						maxAddress, mode, options, memoryCategory);
			} else {
				if (requestedTopAddress < (void*)NON_SCALING_LOW_MEMORY_HEAP_CEILING) {
					/*
					 * Attempt to allocate heap below 4G
					 */
					instance = MM_VirtualMemory::newInstance(env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
															 (void*)OMR_MIN(NON_SCALING_LOW_MEMORY_HEAP_CEILING, (uintptr_t)ceiling), mode, options, memoryCategory);
				}

				if ((NULL == instance) && (ceiling > (void*)NON_SCALING_LOW_MEMORY_HEAP_CEILING)) {

#define THIRTY_TWO_GB_ADDRESS ((uintptr_t)32 * 1024 * 1024 * 1024)
					if (requestedTopAddress <= (void *)THIRTY_TWO_GB_ADDRESS) {
						/*
						 * If requested object heap size is in range 28G-32G its allocation with 3-bit shift might compromise amount of low memory below 4G
						 * To prevent this go straight to 4-bit shift if it possible.
						 * Set of logical conditions to skip allocation attempt below 32G
						 *  - 4-bit shift is available option
						 *  - requested size is larger then 28G (32 minus 4)
						 *  - allocation direction is top-down, otherwise it does not make sense
						 */
						bool skipAllocationBelow32G = (ceiling > (void*)THIRTY_TWO_GB_ADDRESS)
							 && (requestedTopAddress > (void*)(THIRTY_TWO_GB_ADDRESS - NON_SCALING_LOW_MEMORY_HEAP_CEILING))
							 && allocationTopDown;

						if (!skipAllocationBelow32G) {
							/*
							 * Attempt to allocate heap below 32G
							 */
							instance = MM_VirtualMemory::newInstance(env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
																	 (void*)OMR_MIN((uintptr_t)THIRTY_TWO_GB_ADDRESS, (uintptr_t)ceiling), mode, options, memoryCategory);
						}
					}

					/*
					 * Attempt to allocate above 32G
					 */
					if ((NULL == instance) && (ceiling > (void *)THIRTY_TWO_GB_ADDRESS)) {
						instance = MM_VirtualMemory::newInstance(env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
																 ceiling, mode, options, memoryCategory);
					}
				}
			}

			/*
			 * If preferredAddress is requested check is it really taken: if not - release memory
			 * for backward compatibility this check should be done for compressedrefs platforms only
			 */
			if ((NULL != preferredAddress) && (NULL != instance) && (instance->getHeapBase() != preferredAddress)) {
				instance->kill(env);
				instance = NULL;
			}

			if ((NULL != instance) && shouldHeapBeAllocatedFirst) {
				if (OMRPORT_ENSURE_CAPACITY_FAILED == omrmem_ensure_capacity32(extensions->suballocatorInitialSize)) {
					extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_ALLOCATE_LOW_MEMORY_RESERVE;
					instance->kill(env);
					instance = NULL;
					return false;
				}
			}
		}
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */

		/*
		 * Code above might be used for non-compressedrefs platforms but need a few adjustments on it for this:
		 *  - NON_SCALING_LOW_MEMORY_HEAP_CEILING should be set
		 *  - OMRPORT_VMEM_ZOS_USE2TO32G_AREA flag for ZOS is expected to be used for compressedrefs heap allocation only
		 */
		Assert_MM_unimplemented();

#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	}

	if((NULL != instance) && extensions->largePageFailOnError && (instance->getPageSize() != extensions->requestedPageSize)) {
		extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_SATISFY_REQUESTED_PAGE_SIZE;
		instance->kill(env);
		instance = NULL;
		return false;
	}

	handle->setVirtualMemory(instance);
	if (NULL != instance) {
		instance->incrementConsumerCount();
		handle->setMemoryBase(instance->getHeapBase());
		handle->setMemoryTop(instance->getHeapTop());
	}

	return NULL != instance;
}

bool
MM_MemoryManager::createVirtualMemoryForMetadata(MM_EnvironmentBase* env, MM_MemoryHandle* handle, uintptr_t alignment, uintptr_t size)
{
	Assert_MM_true(NULL != handle);
	Assert_MM_true(NULL == handle->getVirtualMemory());
	MM_GCExtensionsBase* extensions = env->getExtensions();

	/*
	 * Can we take already preallocated memory?
	 */
	if (NULL != _preAllocated.getVirtualMemory()) {
		/* base might be not aligned */
		void* base = (void*)MM_Math::roundToCeiling(alignment, (uintptr_t)_preAllocated.getMemoryBase());
		void* top = (void*)((uintptr_t)base + MM_Math::roundToCeiling(alignment, size));
		if (top <= _preAllocated.getMemoryTop()) {
			/* it is enough room in preallocated memory - take chunk of it */
			MM_VirtualMemory* instance = _preAllocated.getVirtualMemory();
			/* Add one more consumer to Virtual Memory instance */
			instance->incrementConsumerCount();

			/* set Virtual Memory parameters to the provided handle */
			handle->setVirtualMemory(instance);
			handle->setMemoryBase(base);
			handle->setMemoryTop(top);

			/* If it is not all preallocated memory is consumed make correct settings for rest of it, clean otherwise */
			if (top < _preAllocated.getMemoryTop()) {
				/* something left in preallocated */
				_preAllocated.setMemoryBase(top);
			} else {
				/* nothing left in preallocated */
				_preAllocated.setVirtualMemory(NULL);
			}
		}
	}

	/*
	 * Make real memory allocation if necessary
	 */
	if (NULL == handle->getVirtualMemory()) {
		uint32_t memoryCategory = OMRMEM_CATEGORY_MM;
		MM_VirtualMemory* instance = NULL;
		bool isOverAllocationRequested = false;

		/* memory consumer might expect memory to be aligned so allocate a little bit more */
		uintptr_t allocateSize = size + ((2 * alignment) - 1);

		if (isMetadataAllocatedInVirtualMemory(env)) {
			uintptr_t tailPadding = 0;
			void* preferredAddress = NULL;
			void* ceiling = NULL;
			uintptr_t mode = (OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE);
			uintptr_t options = 0;

			uintptr_t pageSize = extensions->gcmetadataPageSize;
			uintptr_t pageFlags = extensions->gcmetadataPageFlags;
			Assert_MM_true(0 != pageSize);

			/*
			 * Preallocation is enabled for all platforms where metadata can be allocated in virtual memory
			 * Segmentation is enabled for AIX-64 only, so physical page size is used as a segment size for other platforms
			 * To get advantage of preallocation this memory should be allocated in large pages
			 */
			if (isLargePage(env, pageSize)) {

				uintptr_t minimumAllocationUnit = getSegmentSize();
				if (0 == minimumAllocationUnit) {
					/*
					 * Memory is not segmented on this platform, so minimum unit is limited by page size only
					 */
					minimumAllocationUnit = pageSize;
				}

				/*
				 * Round up allocation size to the minimum allocation unit (large page or segment)
				 * Reminder can be used as a preallocated memory
				 */
				allocateSize = MM_Math::roundToCeiling(minimumAllocationUnit, allocateSize);
				isOverAllocationRequested = true;
			}

			/*
			 * Create Virtual Memory instance
			 */
			instance = MM_VirtualMemory::newInstance(env, alignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress, ceiling, mode, options, memoryCategory);
		} else {
			/*
			 * Allocate memory using malloc (create NonVirtual Memory instance)
			 */
			instance = MM_NonVirtualMemory::newInstance(env, alignment, allocateSize, memoryCategory);
		}

		if (NULL != instance) {
			/* Add one more consumer to Virtual Memory instance */
			instance->incrementConsumerCount();

			/* set Virtual Memory parameters to the provided handle */
			handle->setVirtualMemory(instance);
			handle->setMemoryBase(instance->getHeapBase());
			handle->setMemoryTop((void*)((uintptr_t)instance->getHeapBase() + size));

			/*
			 * Setup preallocated memory if over-allocation has been requested
			 */
			if (isOverAllocationRequested) {
				_preAllocated.setVirtualMemory(instance);
				/* base might be not rounded here - will be when memory is going to be taken */
				_preAllocated.setMemoryBase(handle->getMemoryTop());
				_preAllocated.setMemoryTop(instance->getHeapTop());
			}
		}
	}

	return NULL != handle->getVirtualMemory();
}

void
MM_MemoryManager::destroyVirtualMemory(MM_EnvironmentBase* env, MM_MemoryHandle* handle)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory* memory = handle->getVirtualMemory();
	if (NULL != memory) {
		Assert_MM_true(memory->getConsumerCount() > 0);
		memory->decrementConsumerCount();
		if (0 == memory->getConsumerCount()) {
			/* this is last consumer attached to this Virtual Memory instance - delete it */
			memory->kill(env);

			/*
			 * If this instance has been used as a preallocated (but not taken) memory it should be cleared as well
			 */
			if (memory == _preAllocated.getVirtualMemory()) {
				_preAllocated.setVirtualMemory(NULL);
			}
		}
	}

	handle->setVirtualMemory(NULL);
	handle->setMemoryBase(NULL);
	handle->setMemoryTop(NULL);
}

bool
MM_MemoryManager::commitMemory(MM_MemoryHandle* handle, void* address, uintptr_t size)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory* memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->commitMemory(address, size);
}

bool
MM_MemoryManager::decommitMemory(MM_MemoryHandle* handle, void* address, uintptr_t size, void* lowValidAddress, void* highValidAddress)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory* memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->decommitMemory(address, size, lowValidAddress, highValidAddress);
}

bool
MM_MemoryManager::isLargePage(MM_EnvironmentBase* env, uintptr_t pageSize)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uintptr_t* pageSizes = omrvmem_supported_page_sizes();
	return pageSize > pageSizes[0];
}

#if defined(OMR_GC_VLHGC) || defined(OMR_GC_MODRON_SCAVENGER)
bool
MM_MemoryManager::setNumaAffinity(const MM_MemoryHandle* handle, uintptr_t numaNode, void* address, uintptr_t byteAmount)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory* memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->setNumaAffinity(numaNode, address, byteAmount);
}

#endif /* defined(OMR_GC_VLHGC) || defined(OMR_GC_MODRON_SCAVENGER) */
