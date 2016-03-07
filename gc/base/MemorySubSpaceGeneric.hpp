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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


#if !defined(MEMORYSUBSPACEGENERIC_HPP_)
#define MEMORYSUBSPACEGENERIC_HPP_

#include "omrcfg.h"

#include "MemorySubSpace.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_HeapStats;
class MM_MemoryPool;
class MM_MemorySpace;
class MM_ObjectAllocationInterface;
class MM_RegionPool;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_MemorySubSpaceGeneric : public MM_MemorySubSpace {
	MM_MemoryPool* _memoryPool; /**< the memory pool associated with this subspace */
	MM_RegionPool* _regionPool; /**< the region pool associated with this subspace */
	bool _allocateAtSafePointOnly;

public:
	static MM_MemorySubSpaceGeneric* newInstance(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool, MM_RegionPool* regionPool, bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize, uintptr_t memoryType, uint32_t objectFlags);

	virtual const char* getName() { return MEMORY_SUBSPACE_NAME_GENERIC; }
	virtual const char* getDescription() { return MEMORY_SUBSPACE_DESCRIPTION_GENERIC; }

	virtual MM_MemoryPool* getMemoryPool();
	virtual MM_MemoryPool* getMemoryPool(void* addr);
	virtual MM_MemoryPool* getMemoryPool(uintptr_t size);
	virtual MM_MemoryPool* getMemoryPool(MM_EnvironmentBase* env, void* addrBase, void* addrTop, void*& highAddr);

	virtual uintptr_t getMemoryPoolCount();
	virtual uintptr_t getActiveMemoryPoolCount();

	virtual uintptr_t getActiveMemorySize();
	virtual uintptr_t getActiveMemorySize(uintptr_t includeMemoryType);

	virtual uintptr_t getActualFreeMemorySize();
	virtual uintptr_t getApproximateFreeMemorySize();

	virtual uintptr_t getActualActiveFreeMemorySize();
	virtual uintptr_t getActualActiveFreeMemorySize(uintptr_t includememoryType);
	virtual uintptr_t getApproximateActiveFreeMemorySize();
	virtual uintptr_t getApproximateActiveFreeMemorySize(uintptr_t includememoryType);

	virtual uintptr_t getActiveLOAMemorySize(uintptr_t includememoryType);
	virtual uintptr_t getApproximateActiveFreeLOAMemorySize();
	virtual uintptr_t getApproximateActiveFreeLOAMemorySize(uintptr_t includememoryType);

	virtual void mergeHeapStats(MM_HeapStats* heapStats);
	virtual void mergeHeapStats(MM_HeapStats* heapStats, uintptr_t includeMemoryType);
	virtual void resetHeapStatistics(bool globalCollect);
	virtual MM_AllocationFailureStats* getAllocationFailureStats();

	virtual void* allocateObject(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure);
#if defined(OMR_GC_ARRAYLETS)
	virtual void* allocateArrayletLeaf(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure);
#endif /* OMR_GC_ARRAYLETS */

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	virtual void* allocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace, bool shouldCollectOnFailure);
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void setAllocateAtSafePointOnly(MM_EnvironmentBase* env, bool safePoint) { _allocateAtSafePointOnly = safePoint; }

	/* Calls for internal collection routines */
	virtual void* collectorAllocate(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription);
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	virtual void* collectorAllocateTLH(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription, uintptr_t maximumBytesRequired, void*& addrBase, void*& addrTop);
#endif /* OMR_GC_THREAD_LOCAL_HEAP */
	virtual uintptr_t collectorExpand(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription);

	virtual void abandonHeapChunk(void* addrBase, void* addrTop);

	virtual MM_MemorySubSpace* getDefaultMemorySubSpace();
	virtual MM_MemorySubSpace* getTenureMemorySubSpace();

	virtual void reset();
	virtual void rebuildFreeList(MM_EnvironmentBase* env);

	virtual bool completeFreelistRebuildRequired(MM_EnvironmentBase* env);

	virtual bool expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, MM_HeapRegionDescriptor* region, bool canCoalesce);
	virtual bool expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress, bool canCoalesce);

	virtual void addExistingMemory(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress, bool canCoalesce);
	virtual void* removeExistingMemory(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress);

#if defined(OMR_GC_MODRON_STANDARD)
	virtual void* findFreeEntryEndingAtAddr(MM_EnvironmentBase* env, void* addr);
	virtual void* findFreeEntryTopStartingAtAddr(MM_EnvironmentBase* env, void* addr);
	virtual void* getFirstFreeStartingAddr(MM_EnvironmentBase* env);
	virtual void* getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree);
	virtual void moveHeap(MM_EnvironmentBase* env, void* srcBase, void* srcTop, void* dstBase);
#endif /* OMR_GC_MODRON_STANDARD */

	virtual bool isActive();

	/**
	 * Create a MemorySubSpaceGeneric object
	 */
	MM_MemorySubSpaceGeneric(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool, MM_RegionPool* regionPool, bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize, uintptr_t memoryType, uint32_t objectFlags)
		: MM_MemorySubSpace(env, (MM_Collector*)NULL, NULL, usesGlobalCollector, minimumSize, initialSize, maximumSize, memoryType, objectFlags)
		, _memoryPool(memoryPool)
		, _regionPool(regionPool)
		, _allocateAtSafePointOnly(false)
	{
		_typeId = __FUNCTION__;
	};

protected:
	virtual void* allocationRequestFailed(MM_EnvironmentBase* env, MM_AllocateDescription* allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* baseSubSpace, MM_MemorySubSpace* previousSubSpace);
	bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);

private:
	/**
	 * Add an expansion to old subspace range
	 * As far as old subspace expected to be contiguous an expansion should be at the top or bottom of the memory range
	 * An exception is an initial setup when both of borders are set. The range before such set better to be (NULL,NULL)
	 * @param size the size of the expansion
	 * @param low low expansion address
	 * @param high high expansion address
	 */
	void addTenureRange(MM_EnvironmentBase* env, uintptr_t size, void* low, void* high);

	/**
	 * Add a contraction of old subspace range
	 * As far as old subspace expected to be contiguous a contraction should be at the top or bottom of the memory range
	 * @param size the size of the contraction
	 * @param low low expansion address
	 * @param high high expansion address
	 */
	void removeTenureRange(MM_EnvironmentBase* env, uintptr_t size, void* low, void* high);
};

#endif /* MEMORYSUBSPACEGENERIC_HPP_ */
