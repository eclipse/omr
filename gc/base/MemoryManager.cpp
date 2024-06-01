/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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


#include "MemoryManager.hpp"

#include "ModronAssertions.h"
#include "GCExtensionsBase.hpp"
#include "NonVirtualMemory.hpp"
#if defined(OMR_VALGRIND_MEMCHECK)
#include "MemcheckWrapper.hpp"
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

#define TWO_GB_ADDRESS ((void *)((uintptr_t)2 * 1024 * 1024 * 1024))
#define EIGHT_GB_ADDRESS ((void *)((uintptr_t)8 * 1024 * 1024 * 1024))
#define SIXTEEN_GB_ADDRESS ((void *)((uintptr_t)16 * 1024 * 1024 * 1024))
#define THIRTY_TWO_GB_ADDRESS ((void *)((uintptr_t)32 * 1024 * 1024 * 1024))

MM_MemoryManager *
MM_MemoryManager::newInstance(MM_EnvironmentBase *env)
{
	MM_MemoryManager *memoryManager = (MM_MemoryManager *)env->getForge()->allocate(sizeof(MM_MemoryManager), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

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
MM_MemoryManager::kill(MM_EnvironmentBase *env)
{
	env->getForge()->free(this);
}

bool
MM_MemoryManager::initialize(MM_EnvironmentBase *env)
{
	return true;
}

bool
MM_MemoryManager::createVirtualMemoryForHeap(MM_EnvironmentBase *env, MM_MemoryHandle *handle, uintptr_t heapAlignment, uintptr_t size, uintptr_t tailPadding, void *preferredAddress, void *ceiling)
{
	Assert_MM_true(NULL != handle);
	MM_GCExtensionsBase *extensions = env->getExtensions();

	MM_VirtualMemory *instance = NULL;
	uintptr_t mode = (OMRPORT_VMEM_MEMORY_MODE_READ | OMRPORT_VMEM_MEMORY_MODE_WRITE);
	uintptr_t options = 0;
	uint32_t memoryCategory = OMRMEM_CATEGORY_MM_RUNTIME_HEAP;

	uintptr_t pageSize = extensions->requestedPageSize;
	uintptr_t pageFlags = extensions->requestedPageFlags;
	Assert_MM_true(0 != pageSize);

	uintptr_t allocateSize = size;

	uintptr_t concurrentScavengerPageSize = 0;
	if (extensions->isConcurrentScavengerHWSupported()) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		/*
		 * Allocate extra memory to guarantee proper alignment regardless start address location
		 * Minimum size of over-allocation should be (Concurrent_Scavenger_page_size - section size) however
		 * Virtual Memory can return heap size shorter by a region (and here region size == section size)
		 * So to guarantee desired heap size over-allocate it by full Concurrent_Scavenger_page_size
		 */
		concurrentScavengerPageSize = extensions->getConcurrentScavengerPageSectionSize() * CONCURRENT_SCAVENGER_PAGE_SECTIONS;
		allocateSize += concurrentScavengerPageSize;
		if (extensions->isDebugConcurrentScavengerPageAlignment()) {
			omrtty_printf("Requested heap size 0x%zx has been extended to 0x%zx for guaranteed alignment\n", size, allocateSize);
		}
	} else {
		if (heapAlignment > pageSize) {
			allocateSize += (heapAlignment - pageSize);
		}
	}

#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
	/*
	 * The decision to create a heap with a shared file descriptor is based on if double map was requested
	 * or not, because we do not know at this point the actual heap page size (double map does not support
	 * huge pages). Nonetheless, this is a safe operation because in case page size equals system's huge page
	 * the mode flag OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN will be ignored.
	 */
	if (extensions->isVLHGC() && extensions->isArrayletDoubleMapRequested) {
		mode |= OMRPORT_VMEM_MEMORY_MODE_SHARE_FILE_OPEN;
	}
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */

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

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
		if (!env->compressObjectReferences()) {
			if (1 == extensions->fvtest_enableReadBarrierVerification) {
				MM_VirtualMemory* instanceShadow = MM_VirtualMemory::newInstance(env, heapAlignment, allocateSize, pageSize, pageFlags,
						tailPadding, preferredAddress, (void *)OMR_MIN(NON_SCALING_LOW_MEMORY_HEAP_CEILING,
						(uintptr_t)ceiling), mode, options, memoryCategory);

				extensions->shadowHeapBase = instanceShadow->getHeapBase();
				extensions->shadowHeapTop = instanceShadow->getHeapTop();
				extensions->shadowHeapHandle.setVirtualMemory(instanceShadow);
			}
		}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
	} else {
#if defined(OMR_GC_COMPRESSED_POINTERS)
		if (env->compressObjectReferences()) {
			OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
			/*
			 * This function is used for Compressed References platforms only
			 * The ceiling for such platforms is set maximum memory value be supported (32G for 3-bit shift)
			 * The ceiling it is 0 for all other platforms
			 */
			/* NON_SCALING_LOW_MEMORY_HEAP_CEILING is set to 4G for 64-bit platforms only, 0 for 32-bit platforms */
			Assert_MM_true(NON_SCALING_LOW_MEMORY_HEAP_CEILING > 0);
		
			/*
			 * Usually the suballocator memory should be allocated first (before heap) however
			 * in case when preferred address is specified we will try to allocate heap first
			 * to avoid possible interference with requested heap location
			 */
			bool shouldHeapBeAllocatedFirst = (NULL != preferredAddress);
			void *startAllocationAddress = preferredAddress;

			/* Set the commit size for the sub allocator. This needs to be completed before the call to omrmem_ensure_capacity32 */
			omrport_control(OMRPORT_CTLDATA_ALLOCATE32_COMMIT_SIZE, extensions->suballocatorCommitSize);

			if (!shouldHeapBeAllocatedFirst) {
				if (OMRPORT_ENSURE_CAPACITY_FAILED == omrmem_ensure_capacity32(extensions->suballocatorInitialSize)) {
					extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_ALLOCATE_LOW_MEMORY_RESERVE;
					return false;
				}
			}

			options |= OMRPORT_VMEM_STRICT_ADDRESS | OMRPORT_VMEM_ALLOC_QUICK;

#if defined(J9ZOS39064)
			/* 2TO32G area is extended to 64G */
			options |= OMRPORT_VMEM_ZOS_USE2TO32G_AREA;

			/*
			 * On ZOS an address space below 2G can not be taken for virtual memory
			 */
			if (NULL == preferredAddress) {
				startAllocationAddress = TWO_GB_ADDRESS;
			}
#endif /* defined(J9ZOS39064) */

			void *requestedTopAddress = (void *)((uintptr_t)startAllocationAddress + allocateSize + tailPadding);

			if (extensions->isConcurrentScavengerHWSupported()) {
				void *ceilingToRequest = ceiling;
				/* Requested top address might be higher then ceiling because of added chunk */
				if ((requestedTopAddress > ceiling) && ((void *)((uintptr_t)requestedTopAddress - concurrentScavengerPageSize) <= ceiling)) {
					/* ZOS 2_TO_64/2_TO_32 options would not allow memory request larger then 64G/32G so total requested size including tail padding should not exceed it */
					allocateSize = (uintptr_t)ceiling - (uintptr_t)startAllocationAddress - tailPadding;

					if (extensions->isDebugConcurrentScavengerPageAlignment()) {
						omrtty_printf("Total allocate size exceeds ceiling %p, reduce allocate size to 0x%zx\n", ceiling, allocateSize);
					}
					/*
					 * There is no way that Nursery will be pushed above ceiling for valid memory options however we have
					 * no idea about start address. So to guarantee an allocation up to the ceiling we need to request extended chunk of memory. 
					 * Set ceiling to NULL to disable ceiling control. This required bottom-up direction for allocation.
					 */
					ceilingToRequest = NULL;
				}

				options |= OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP;

				/* An attempt to allocate memory chunk for heap for Concurrent Scavenger */
				instance = MM_VirtualMemory::newInstance(env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress, ceilingToRequest, mode, options, memoryCategory);
			} else {
				if (requestedTopAddress <= ceiling) {
					bool allocationTopDown = true;
					/* define the scan direction when reserving the GC heap in the range of (4G, 32G) */
#if defined(J9ZOS390)
					/*
					 * zOS does not allow to choose direction of allocation and uses Bottom Up.
					 * So, this flag is not necessary. However set it to match actual behavior.
					 */
					options |= OMRPORT_VMEM_ALLOC_DIR_BOTTOM_UP;
					allocationTopDown = false;
#else /* defined(J9ZOS390) */
					/*
					 * Use Top Down for all platforms except zOS.
					 */
					options |= OMRPORT_VMEM_ALLOC_DIR_TOP_DOWN;
#endif /* defined(J9ZOS390) */

					if (allocationTopDown && extensions->shouldForceSpecifiedShiftingCompression) {
						/* force to allocate heap top-down from correspondent to shift address */
						void *maxAddress = (void *)(((uintptr_t)1 << 32) << extensions->forcedShiftingCompressionAmount);

						instance = MM_VirtualMemory::newInstance(
								env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
								maxAddress, mode, options, memoryCategory);
					} else {
						void *adjustedCeiling = NULL;

						/*
						 * Attempt to allocate heap below 4G.
						 */
						if (requestedTopAddress < (void *)NON_SCALING_LOW_MEMORY_HEAP_CEILING) {
							adjustedCeiling = (void *)OMR_MIN(NON_SCALING_LOW_MEMORY_HEAP_CEILING, (uintptr_t)ceiling);

							instance = MM_VirtualMemory::newInstance(
									env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
									adjustedCeiling, mode, options, memoryCategory);
						}

						if ((NULL == instance) && (ceiling > (void *)NON_SCALING_LOW_MEMORY_HEAP_CEILING)) {

#if defined(LINUX) && defined(S390)
							/*
							 * Z platform executes smaller shift faster, so it is beneficial to have smaller one.
							 * Attempting to allocate object heap below 8G or 16G to get Compressed
							 * References shift 1 or 2 for zLinux only.
							 */

							/*
							 * Attempt to allocate heap below 8G.
							 */
							if ((NULL == instance) && (requestedTopAddress <= EIGHT_GB_ADDRESS)) {
								adjustedCeiling = (void *)OMR_MIN((uintptr_t)EIGHT_GB_ADDRESS, (uintptr_t)ceiling);

								instance = MM_VirtualMemory::newInstance(
										env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
										adjustedCeiling, mode, options, memoryCategory);
							}

							/*
							 * Attempt to allocate heap below 16G.
							 */
							if ((NULL == instance) && (requestedTopAddress <= SIXTEEN_GB_ADDRESS)) {
								adjustedCeiling = (void *)OMR_MIN((uintptr_t)SIXTEEN_GB_ADDRESS, (uintptr_t)ceiling);

								instance = MM_VirtualMemory::newInstance(
										env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
										adjustedCeiling, mode, options, memoryCategory);
							}
#endif /* defined(LINUX) && defined(S390) */

							if ((NULL == instance) && (requestedTopAddress <= THIRTY_TWO_GB_ADDRESS)) {
								/*
								 * If requested object heap size is in range 28G-32G its allocation with 3-bit shift might compromise amount of low memory below 4G
								 * To prevent this go straight to 4-bit shift if it possible.
								 * Set of logical conditions to skip allocation attempt below 32G
								 *  - 4-bit shift is available option
								 *  - requested size is larger then 28G (32 minus 4)
								 *  - allocation direction is top-down, otherwise it does not make sense
								 */
								bool skipAllocationBelow32G = (ceiling > THIRTY_TWO_GB_ADDRESS)
									 && (requestedTopAddress > (void *)((uintptr_t)THIRTY_TWO_GB_ADDRESS - NON_SCALING_LOW_MEMORY_HEAP_CEILING))
									 && allocationTopDown;

								if (!skipAllocationBelow32G) {
									/*
									 * Attempt to allocate heap below 32G.
									 */
									adjustedCeiling = (void *)OMR_MIN((uintptr_t)THIRTY_TWO_GB_ADDRESS, (uintptr_t)ceiling);

									instance = MM_VirtualMemory::newInstance(
											env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
											adjustedCeiling, mode, options, memoryCategory);
								}
							}

							/*
							 * Attempt to allocate heap at any location below ceiling.
							 */
							if (NULL == instance) {
								instance = MM_VirtualMemory::newInstance(
										env, heapAlignment, allocateSize, pageSize, pageFlags, tailPadding, preferredAddress,
										ceiling, mode, options, memoryCategory);
							}
						}
					}
				}
			}

			/*
			 * If preferredAddress is requested check is it really taken: if not - release memory
			 * for backward compatibility this check should be done for compressedrefs platforms only
			 */
			if ((NULL != preferredAddress) && (NULL != instance) && (instance->getHeapBase() != preferredAddress)) {
				extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_HEAP;
				instance->kill(env);
				instance = NULL;
				return false;
			}

			if ((NULL != instance) && shouldHeapBeAllocatedFirst) {
				if (OMRPORT_ENSURE_CAPACITY_FAILED == omrmem_ensure_capacity32(extensions->suballocatorInitialSize)) {
					extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_ALLOCATE_LOW_MEMORY_RESERVE;
					instance->kill(env);
					instance = NULL;
					return false;
				}
			}
		} else
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		{
			/*
			 * Code above might be used for non-compressedrefs platforms but need a few adjustments on it for this:
			 *  - NON_SCALING_LOW_MEMORY_HEAP_CEILING should be set
			 *  - OMRPORT_VMEM_ZOS_USE2TO32G_AREA flag for ZOS is expected to be used for compressedrefs heap allocation only
			 */
			Assert_MM_unimplemented();
		}
	}

	if ((NULL != instance) && extensions->largePageFailOnError && (instance->getPageSize() != extensions->requestedPageSize)) {
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
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
		if (instance->isDoubleMapAvailable()) {
			extensions->isArrayletDoubleMapAvailable = true;
		}
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */

		/*
		 * Aligning Nursery location to Concurrent Scavenger Page and calculate Concurrent Scavenger Page start address
		 * There are two possible cases here:
		 * - Nursery fits Concurrent Scavenger Page already = no extra alignment required
		 * - current Nursery location has crossed Concurrent Scavenger Page boundary so it needs to be pushed higher to
		 *   have Nursery low address to be aligned to Concurrent Scavenger Page
		 */
		if (extensions->isConcurrentScavengerHWSupported()) {
			OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
			/* projected Nursery base and top */
			/* assumed Nursery location in high addresses of the heap */
			uintptr_t heapBase = (uintptr_t)handle->getMemoryBase();
			uintptr_t nurseryTop = heapBase + size;
			uintptr_t nurseryBase = nurseryTop - extensions->maxNewSpaceSize;

			if (extensions->isDebugConcurrentScavengerPageAlignment()) {
				omrtty_printf("Allocated memory for heap: [%p,%p]\n", handle->getMemoryBase(), handle->getMemoryTop());
			}

			uintptr_t baseAligned = MM_Math::roundToCeiling(concurrentScavengerPageSize, nurseryBase + 1);
			uintptr_t topAligned = MM_Math::roundToCeiling(concurrentScavengerPageSize, nurseryTop);

			if (baseAligned == topAligned) {
				/* Nursery fits Concurrent Scavenger Page already */
				extensions->setConcurrentScavengerPageStartAddress((void *)(baseAligned - concurrentScavengerPageSize));

				if (extensions->isDebugConcurrentScavengerPageAlignment()) {
					omrtty_printf("Expected Nursery start address 0x%zx\n", nurseryBase);
				}
			} else {
				/* Nursery location should be adjusted */
				extensions->setConcurrentScavengerPageStartAddress((void *)baseAligned);

				if (extensions->isDebugConcurrentScavengerPageAlignment()) {
					omrtty_printf("Expected Nursery start address adjusted to 0x%zx\n", baseAligned);
				}

				/* Move up entire heap for proper Nursery adjustment */
				heapBase += (baseAligned - nurseryBase);
				handle->setMemoryBase((void *)heapBase);

				/* top of adjusted Nursery should fit reserved memory */
				Assert_GC_true_with_message3(env, ((heapBase + size) <= (uintptr_t)handle->getMemoryTop()),
						"End of projected heap (base 0x%zx + size 0x%zx) is larger then Top allocated %p\n",
						heapBase, size, handle->getMemoryTop());
			}

			/* adjust heap top to lowest possible address */
			handle->setMemoryTop((void *)(heapBase + size));

			if (extensions->isDebugConcurrentScavengerPageAlignment()) {
				omrtty_printf("Adjusted heap location: [%p,%p], Concurrent Scavenger Page start address %p, Concurrent Scavenger Page size 0x%zx\n",
						handle->getMemoryBase(), handle->getMemoryTop(), extensions->getConcurrentScavengerPageStartAddress(), concurrentScavengerPageSize);
			}

			/*
			 * Concurrent Scavenger Page location might be aligned out of Compressed References supported memory range
			 * Fail to initialize in this case
			 */
			if ((NULL != ceiling) && (handle->getMemoryTop() > ceiling)) {
				extensions->heapInitializationFailureReason = MM_GCExtensionsBase::HEAP_INITIALIZATION_FAILURE_REASON_CAN_NOT_INSTANTIATE_HEAP;
				destroyVirtualMemory(env, handle);
				instance = NULL;
			}
		}
	}

#if defined(OMR_VALGRIND_MEMCHECK)
	//Use handle's Memory Base to refer valgrind memory pool
	valgrindCreateMempool(extensions, env, (uintptr_t)handle->getMemoryBase());
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

	return NULL != instance;
}

bool
MM_MemoryManager::createVirtualMemoryForMetadata(MM_EnvironmentBase *env, MM_MemoryHandle *handle, uintptr_t alignment, uintptr_t size)
{
	Assert_MM_true(NULL != handle);
	Assert_MM_true(NULL == handle->getVirtualMemory());
	MM_GCExtensionsBase *extensions = env->getExtensions();

	/*
	 * Can we take already preallocated memory?
	 */
	if (NULL != _preAllocated.getVirtualMemory()) {
		/* base might be not aligned */
		void *base = (void *)MM_Math::roundToCeiling(alignment, (uintptr_t)_preAllocated.getMemoryBase());
		void *top = (void *)((uintptr_t)base + MM_Math::roundToCeiling(alignment, size));
		if (top <= _preAllocated.getMemoryTop()) {
			/* it is enough room in preallocated memory - take chunk of it */
			MM_VirtualMemory *instance = _preAllocated.getVirtualMemory();
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
		MM_VirtualMemory *instance = NULL;
		bool isOverAllocationRequested = false;

		/* memory consumer might expect memory to be aligned so allocate a little bit more */
		uintptr_t allocateSize = size + ((2 * alignment) - 1);

		if (isMetadataAllocatedInVirtualMemory(env)) {
			uintptr_t tailPadding = 0;
			void *preferredAddress = NULL;
			void *ceiling = NULL;
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
MM_MemoryManager::destroyVirtualMemoryForHeap(MM_EnvironmentBase *env, MM_MemoryHandle *handle)
{
	destroyVirtualMemory(env, handle);

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_VirtualMemory *shadowMemory = extensions->shadowHeapHandle.getVirtualMemory();
	if (NULL != shadowMemory) {
		shadowMemory->kill(env);
		extensions->shadowHeapHandle.setVirtualMemory(NULL);
		extensions->shadowHeapHandle.setMemoryBase(NULL);
		extensions->shadowHeapHandle.setMemoryTop(NULL);
	}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
}

void
MM_MemoryManager::destroyVirtualMemory(MM_EnvironmentBase *env, MM_MemoryHandle *handle)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory *memory = handle->getVirtualMemory();
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

#if defined(OMR_VALGRIND_MEMCHECK)
	valgrindDestroyMempool(env->getExtensions());
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

}

int
MM_MemoryManager::getHeapFileDescriptor(MM_MemoryHandle *handle)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory *memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->getHeapFileDescriptor();
}

#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
void *
MM_MemoryManager::doubleMapArraylet(MM_MemoryHandle *handle, MM_EnvironmentBase *env, void *arrayletLeaves[], UDATA arrayletLeafCount, UDATA arrayletLeafSize, UDATA byteAmount, struct J9PortVmemIdentifier *newIdentifier, UDATA pageSize)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory *memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->doubleMapArraylet(env, arrayletLeaves, arrayletLeafCount, arrayletLeafSize, byteAmount, newIdentifier, pageSize);
}

void *
MM_MemoryManager::doubleMapRegions(MM_MemoryHandle *handle, MM_EnvironmentBase *env, void *regions[], UDATA regionsCount, UDATA regionSize, UDATA byteAmount, struct J9PortVmemIdentifier *newIdentifier, UDATA pageSize, void *preferredAddress)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory *memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->doubleMapRegions(env, regions, regionsCount, regionSize, byteAmount, newIdentifier, pageSize, preferredAddress);
}
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */

bool
MM_MemoryManager::commitMemory(MM_MemoryHandle *handle, void *address, uintptr_t size)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory* memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->commitMemory(address, size);
}

bool
MM_MemoryManager::decommitMemory(MM_MemoryHandle *handle, void *address, uintptr_t size, void *lowValidAddress, void *highValidAddress)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory *memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->decommitMemory(address, size, lowValidAddress, highValidAddress);
}

bool
MM_MemoryManager::isLargePage(MM_EnvironmentBase *env, uintptr_t pageSize)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uintptr_t *pageSizes = omrvmem_supported_page_sizes();
	return pageSize > pageSizes[0];
}

#if defined(OMR_GC_VLHGC) || defined(OMR_GC_MODRON_SCAVENGER)
bool
MM_MemoryManager::setNumaAffinity(const MM_MemoryHandle *handle, uintptr_t numaNode, void *address, uintptr_t byteAmount)
{
	Assert_MM_true(NULL != handle);
	MM_VirtualMemory *memory = handle->getVirtualMemory();
	Assert_MM_true(NULL != memory);
	return memory->setNumaAffinity(numaNode, address, byteAmount);
}

#endif /* defined(OMR_GC_VLHGC) || defined(OMR_GC_MODRON_SCAVENGER) */
