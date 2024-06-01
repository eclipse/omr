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

#if !defined(MEMORYMANAGER_HPP)
#define MEMORYMANAGER_HPP

#include "omrcomp.h"
#include "modronbase.h"

#include "BaseNonVirtual.hpp"
#include "MemoryHandle.hpp"
#include "VirtualMemory.hpp"

class MM_EnvironmentBase;

class MM_MemoryManager : public MM_BaseNonVirtual {
	/*
	 * Data members
	 */
private:
	MM_MemoryHandle _preAllocated; /**< stored preallocated memory parameters in case of over-allocation */

protected:
public:

	/*
	 * Function members
	 */
private:
	/**
	 * Check can GC Matadata allocation be done in Virtual Memory
	 * An alternative is to use malloc (non-virtual memory)
	 * This decision is platform-based
	 *
	 * @param env environment
	 * @return true if Virtual Memory can be used
	 */
	MMINLINE bool isMetadataAllocatedInVirtualMemory(MM_EnvironmentBase *env)
	{
		bool result = true;

#if (defined(AIXPPC) && !defined(PPC64))
		result = false;
#elif(defined(AIXPPC) && defined(OMR_GC_REALTIME))
		MM_GCExtensionsBase *extensions = env->getExtensions();
		if (extensions->isMetronomeGC()) {
			result = false;
		}
#elif defined(J9ZOS39064)
		result = false;
#endif /* (defined(AIXPPC) && (!defined(PPC64) || defined(OMR_GC_REALTIME))) */

		return result;
	}

	/**
	 * Return memory segment size for preallocation purpose
	 * Is used for preallocation-enabled platforms only
	 * An idea is to take entire memory segment (reserved any way - over-allocate strategy)
	 * and use this memory later for GC Metadata allocations
	 *
	 * @return memory segment size
	 */
	MMINLINE uintptr_t getSegmentSize()
	{
		uintptr_t result = 0;

#if (defined(AIXPPC) && defined(PPC64))
		/*
		 * AIX-64 memory segment size is 256M
		 */
		result = (uintptr_t)256 * 1024 * 1024;
#endif /* (defined(AIXPPC) && defined(PPC64)) */

		return result;
	}

protected:
	/**
	 * Provide an initialization for the class
	 *
	 * @param env environment
	 * @return true if an initialization is successful
	 */
	bool initialize(MM_EnvironmentBase *env);

	MM_MemoryManager(MM_EnvironmentBase *env)
		: _preAllocated()
	{
		_typeId = __FUNCTION__;
	};

public:
	/**
	 * Create new instance for the class
	 *
	 * @param env environment
	 * @return pointer to created instance of class
	 */
	static MM_MemoryManager *newInstance(MM_EnvironmentBase *env);

	/**
	 * Check is provided page size larger then default page size from port library
	 *
	 * @param env environment
	 * @param pageSize requested page size
	 * @return true if page size is larger then default
	 */
	bool isLargePage(MM_EnvironmentBase *env, uintptr_t pageSize);

	/**
	 * Kill this instance of the class
	 *
	 * @param env environment
	 */
	void kill(MM_EnvironmentBase *env);

	/**
	 * Create virtual memory instance
	 *
	 * @param env environment
	 * @param[in/out] handle pointer to memory handle
	 * @param heapAlignment required heap alignment
	 * @param size required memory size
	 * @param tailPadding required tail padding
	 * @param preferredAddress requested preferred address
	 * @param ceiling highest address this memory can be allocated
	 * @return true if pointer to virtual memory is not NULL
	 *
	 */
	bool createVirtualMemoryForHeap(MM_EnvironmentBase *env, MM_MemoryHandle *handle, uintptr_t heapAlignment, uintptr_t size, uintptr_t tailPadding, void *preferredAddress, void *ceiling);

	/**
	 * Creates the correct type of VirtualMemory object for the current platform and configuration
	 *
	 * @param env environment
	 * @param[in/out] handle pointer to memory handle
	 * @param heapAlignment required heap alignment
	 * @param size required memory size
	 * @return true if pointer to virtual memory is not NULL
	 */
	bool createVirtualMemoryForMetadata(MM_EnvironmentBase *env, MM_MemoryHandle *handle, uintptr_t heapAlignment, uintptr_t size);

	/**
	 * Destroy virtual memory instance
	 *
	 * @param env environment
	 * @param[in/out] handle pointer to memory handle
	 */
	void destroyVirtualMemory(MM_EnvironmentBase *env, MM_MemoryHandle *handle);
	
	/**
	 * Destroy virtual memory instance, plus everything that is heap specific (for example, shadow heap)
	 *
	 * @param env environment
	 * @param[in/out] handle pointer to memory handle
	 */
	void destroyVirtualMemoryForHeap(MM_EnvironmentBase *env, MM_MemoryHandle *handle);
	
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
	/**
 	 * Double maps arraylets, arrays that does not fit into one region are split into leaves, 
 	 * which are then double mapped by this function 
 	 *
 	 * @param pointer to memory handle
 	 * @param env environment
 	 * @param arrayletLeaveAddrs, list of arraylet leaves addresses
 	 * @param arrayletLeafCount, number of arraylet leaves
 	 * @param arrayletLeafSize, size of each arraylet leaf
 	 * @param byteAmount, total byte amount to be allocate contiguous block of meory to double map 
 	 * @param newIdentifier, hold information of newly created contiguous block of memory
 	 * @param pageSize
 	 * @param category
  	 */
	void *doubleMapArraylet(MM_MemoryHandle *handle, MM_EnvironmentBase *env, void *arrayletLeaves[], UDATA arrayletLeafCount, UDATA arrayletLeafSize, UDATA byteAmount, struct J9PortVmemIdentifier *newIdentifier, UDATA pageSize);

	/**
	 * Double maps regions. Discontiguous regions are double mapped to one contiguous region.
	 *
	 * @param pointer to memory handle
	 * @param env environment
	 * @param regions list of regions to be double mapped
	 * @param regionsCount, number of regions to be double mapped
	 * @param regionSize, size of each region
	 * @param byteAmount, total byte amount to be allocate contiguous block of meory to double map
	 * @param newIdentifier, hold information of newly created contiguous block of memory
	 * @param pageSize
	 * @param preferredAddress, prefered address of contiguous region to double map
	 *
	 * @return pointer to contiguous region to which regions were double mapped into, NULL is returned if unsuccessful
	 */
	void *doubleMapRegions(MM_MemoryHandle *handle, MM_EnvironmentBase *env, void *regions[], UDATA regionsCount, UDATA regionSize, UDATA byteAmount, struct J9PortVmemIdentifier *newIdentifier, UDATA pageSize, void *preferredAddress);
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */

	/**
	 * Commit memory for range for specified virtual memory instance
	 *
	 * @param pointer to memory handle
	 * @param address start address of memory should be commited
	 * @param size size of memory should be commited
	 * @return true if succeed
	 */
	bool commitMemory(MM_MemoryHandle *handle, void *address, uintptr_t size);

	/**
	 * Decommit memory for range for specified virtual memory instance
	 *
	 * @param pointer to memory handle
	 * @param address start address of memory should be commited
	 * @param size size of memory should be commited
	 * @param lowValidAddress
	 * @param highValidAddress
	 * @return true if succeed
	 *
	 */
	bool decommitMemory(MM_MemoryHandle *handle, void *address, uintptr_t size, void *lowValidAddress, void *highValidAddress);

#if defined(OMR_GC_VLHGC) || defined(OMR_GC_MODRON_SCAVENGER)
	/*
	 * Set the NUMA affinity for the specified range within the receiver.
	 *
	 * @param pointer to memory handle
	 * @param numaNode - the node to associate the memory with
	 * @param[in] address - the start of the range to modify, must be aligned to the physical page size
	 * @param byteAmount - the size of the range to modify. Might NOT to be aligned to page size. Will be aligned inside.
	 *
	 * @return true on success, false on failure
	 */
	bool setNumaAffinity(const MM_MemoryHandle *handle, uintptr_t numaNode, void *address, uintptr_t byteAmount);
#endif /* defined(OMR_GC_VLHGC) || defined(OMR_GC_MODRON_SCAVENGER) */	

	/**
	 * Call roundDownTop for virtual memory instance provided in memory handle
	 *
	 * @param handle pointer to memory handle
	 * @param rounding rounding value
	 */
	MMINLINE void roundDownTop(MM_MemoryHandle *handle, uintptr_t rounding)
	{
		MM_VirtualMemory *memory = handle->getVirtualMemory();
		memory->roundDownTop(rounding);
		/* heapTop can be modified, refresh it in the memory handle */
		handle->setMemoryTop(memory->getHeapTop());
	}

	/**
	 * Return the heap base of the virtual memory object.
	 *
	 * @param handle pointer to memory handle
	 * @return pointer to base address of virtual memory
	 */
	MMINLINE void *getHeapBase(MM_MemoryHandle *handle)
	{
		return handle->getMemoryBase();
	};

	/**
	 * Return file descriptor associated with heap
	 *
	 * @param handle pointer to memory handle
	 * @return file descriptor associated with heap, if non existent return -1
	 */
	int getHeapFileDescriptor(MM_MemoryHandle *handle);

	/**
	 * Return the top of the heap of the virtual memory object.
	 *
	 * @param handle pointer to memory handle
	 * @return pointer to top address of virtual memory
	 */
	MMINLINE void *getHeapTop(MM_MemoryHandle *handle)
	{
		return handle->getMemoryTop();
	};

	/**
	 * Return the size of the pages used in the virtual memory object
	 *
	 * @param handle pointer to memory handle
	 * @return page size for pages this virtual memory is actually allocated
	 */
	MMINLINE uintptr_t getPageSize(MM_MemoryHandle *handle)
	{
		MM_VirtualMemory *memory = handle->getVirtualMemory();
		return memory->getPageSize();
	};

	/**
	 * Return the flags describing the pages used in the virtual memory object
	 *
	 * @param handle pointer to memory handle
	 * @return page flags for pages this virtual memory is actually allocated
	 */
	MMINLINE uintptr_t getPageFlags(MM_MemoryHandle *handle)
	{
		MM_VirtualMemory *memory = handle->getVirtualMemory();
		return memory->getPageFlags();
	};

	/**
	 * Return the maximum size of the heap.
	 *
	 * @param handle pointer to memory handle
	 * @return maximum size of memory for this virtual memory instance
	 */
	MMINLINE uintptr_t getMaximumSize(MM_MemoryHandle *handle)
	{
		return (uintptr_t)getHeapTop(handle) - (uintptr_t)getHeapBase(handle);
	};

	/**
	 * Calculate the size of the range from the supplied address to the top of the heap.
	 *
	 * @param handle pointer to memory handle
	 * @param address The base address of the range.
	 * @return The size of the range.
	 */
	MMINLINE uintptr_t calculateOffsetToHeapTop(MM_MemoryHandle *handle, void *address)
	{
		return (uintptr_t)getHeapTop(handle) - (uintptr_t)address;
	};

	/**
	 * Return the offset of an address from _heapBase.
	 *
	 * @param handle pointer to memory handle
	 * @param address The address to calculate offset for.
	 * @return The offset from _heapBase.
	 */
	MMINLINE uintptr_t calculateOffsetFromHeapBase(MM_MemoryHandle *handle, void *address)
	{
		return (uintptr_t)address - (uintptr_t)getHeapBase(handle);
	};
};

#endif /* MEMORYMANAGER_HPP */
