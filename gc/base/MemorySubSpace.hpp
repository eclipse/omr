/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(MEMORYSUBSPACE_HPP_)
#define MEMORYSUBSPACE_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "AllocationFailureStats.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "MemorySpacesAPI.h"
#include "ModronAssertions.h"

class GC_MemorySubSpaceRegionIterator;
class MM_AllocateDescription;
class MM_AllocationContext;
class MM_Collector;
class MM_EnvironmentBase;
class MM_GCExtensionsBase;
class MM_HeapRegionDescriptor;
class MM_HeapStats;
class MM_LargeObjectAllocateStats;
class MM_MemoryPool;
class MM_MemorySpace;
class MM_ObjectAllocationInterface;
class MM_PhysicalSubArena;

typedef enum {
	MODRON_COUNTER_BALANCE_TYPE_NONE = 1,
	MODRON_COUNTER_BALANCE_TYPE_EXPAND
} ModronCounterBalanceType;

extern "C" {
/**
 */
void memorySubSpaceAsyncCallbackHandler(OMR_VMThread *omrVMThread);
}

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_MemorySubSpace : public MM_BaseVirtual
{
friend class GC_MemorySubSpaceRegionIterator;

/*
 * Data members
 */
private:
	MM_MemorySubSpace *_next, *_previous;
	MM_MemorySubSpace *_children;
	MM_AllocationFailureStats _allocationFailureStats;
	MM_LightweightNonReentrantLock _lock; /**< Lock used to ensure region list is maintained safely */
	MM_HeapRegionDescriptor *_regionList;
		
	bool _concurrentCollectable;

	uintptr_t _memoryType;

	/* TODO: RTJ Merge: This flag appears to only be used in MemorySubSpaceFlat -- should it move there? */
	uint32_t _objectFlags;

protected:
	MM_GCExtensionsBase *_extensions;
	MM_Collector *_collector;
	MM_MemorySpace *_memorySpace;
	MM_MemorySubSpace *_parent;
	MM_PhysicalSubArena *_physicalSubArena;

	uintptr_t _initialSize;
	uintptr_t _minimumSize;
	uintptr_t _currentSize;
	uintptr_t _maximumSize;
	uint64_t _uniqueID;
	bool _usesGlobalCollector;
	bool _isAllocatable;

	ModronCounterBalanceType _counterBalanceType;
	uintptr_t _counterBalanceSize;
	MM_MemorySubSpace* _counterBalanceChainHead;
	MM_MemorySubSpace* _counterBalanceChain;

	uintptr_t _contractionSize;
	uintptr_t _expansionSize;

public:

/*
 * Function members
 */
private:
	
protected:
	void generateAllocationFailureStats(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription);

public:
	
	MMINLINE void clearCounterBalancing() {
		_counterBalanceType = MODRON_COUNTER_BALANCE_TYPE_NONE;
		_counterBalanceSize = 0;
		_counterBalanceChainHead = NULL;
		_counterBalanceChain = NULL;
	}
	
	void reportSystemGCStart(MM_EnvironmentBase *env, uint32_t gcCode);
	void reportSystemGCEnd(MM_EnvironmentBase *env);
	void reportHeapResizeAttempt(MM_EnvironmentBase *env, uintptr_t amount, uintptr_t type);
	void reportPercolateCollect(MM_EnvironmentBase *env);

	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	MM_HeapRegionDescriptor *getFirstRegion();
	MM_HeapRegionDescriptor *getNextRegion(MM_HeapRegionDescriptor *region);

	/**
	 * The type of allocation we were trying to perform when calling allocationRequestFailed.  This is used to determine
	 * what call-out function to use to restart the allocation after we have dealt with the failure
	 */
	enum AllocationType {
		ALLOCATION_TYPE_INVALID = 0,
		ALLOCATION_TYPE_OBJECT,
		ALLOCATION_TYPE_LEAF,
		ALLOCATION_TYPE_TLH,
	};

	MMINLINE MM_PhysicalSubArena *getPhysicalSubArena() const { return _physicalSubArena; }

	virtual const char *getName() { return MEMORY_SUBSPACE_NAME_UNDEFINED; }
	virtual const char *getDescription() { return MEMORY_SUBSPACE_DESCRIPTION_UNDEFINED; }
	MMINLINE uint64_t getUniqueID() { return _uniqueID; }
	MMINLINE void setUniqueID(uint64_t uniqueID) { _uniqueID = uniqueID; }

	virtual MM_MemoryPool *getMemoryPool();
	virtual MM_MemoryPool *getMemoryPool(void *addr);
	virtual MM_MemoryPool *getMemoryPool(uintptr_t size);
	virtual MM_MemoryPool *getMemoryPool(MM_EnvironmentBase *env, void *addrBase, void *addrTop, void * &highAddr);

	virtual uintptr_t getMemoryPoolCount();
	virtual uintptr_t getActiveMemoryPoolCount();

	virtual void kill(MM_EnvironmentBase *env);

	/* Subclass may or may not implement this. MemorySubSpace that have children may implement this where merged stats will be reported. */
	virtual MM_LargeObjectAllocateStats *getLargeObjectAllocateStats();

	virtual void resetLargestFreeEntry();
	/**
	 * Called by the by the collector when a region is determined to be unused to allow the receiver to recycle the given region
	 * which it is responsible for managing.
	 * @param env[in] The calling thread (usually the main thread of the GC)
	 * @param region[in] The region to be recycled
	 */
	virtual void recycleRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region);
	virtual uintptr_t findLargestFreeEntry(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription);

	MMINLINE void isAllocatable(bool isAllocatable) {_isAllocatable = isAllocatable; }
	MMINLINE bool isAllocatable() { return _isAllocatable; }
	
	virtual MMINLINE uintptr_t getContractionSize() const { return _contractionSize; }
	MMINLINE uintptr_t getExpansionSize() const { return _expansionSize; }
	MMINLINE void setContractionSize(uintptr_t size) { _contractionSize = size; }
	MMINLINE void setExpansionSize(uintptr_t size) { _expansionSize = size; }

	void registerMemorySubSpace(MM_MemorySubSpace *memorySubSpace);
	void unregisterMemorySubSpace(MM_MemorySubSpace *memorySubSpace);
	
	MMINLINE MM_MemorySubSpace* getNext() { return _next; }
	MMINLINE MM_MemorySubSpace* getPrevious() { return _previous; }
	MMINLINE void setNext(MM_MemorySubSpace* memorySubSpace) { _next = memorySubSpace; }
	MMINLINE void setPrevious(MM_MemorySubSpace* memorySubSpace) { _previous = memorySubSpace; }

	MMINLINE MM_MemorySubSpace* getParent() { return _parent; }
	MMINLINE void setParent(MM_MemorySubSpace* subSpace) { _parent = subSpace; }
	MMINLINE MM_MemorySubSpace* getChildren() { return _children; }
	
	void setMemorySpace(MM_MemorySpace *memorySpace);

	MMINLINE uintptr_t getInitialSize() { return _initialSize; }
	MMINLINE uintptr_t getMinimumSize() { return _minimumSize; }
	MMINLINE uintptr_t getCurrentSize() { return _currentSize; }
	MMINLINE uintptr_t getMaximumSize() { return _maximumSize; }

	MMINLINE void setCurrentSize(uintptr_t currentSize) { _currentSize = currentSize; }
	
	virtual uintptr_t getActualFreeMemorySize();
	virtual uintptr_t getApproximateFreeMemorySize();
	
	virtual uintptr_t getActiveMemorySize();
	virtual uintptr_t getActiveMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getActiveLOAMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getActiveSurvivorMemorySize(uintptr_t includeMemoryType);
	
	virtual uintptr_t getActualActiveFreeMemorySize();
	virtual uintptr_t getApproximateActiveFreeMemorySize();
	virtual uintptr_t getApproximateActiveFreeLOAMemorySize();
	virtual uintptr_t getApproximateActiveFreeSurvivorMemorySize();
	
	virtual uintptr_t getActualActiveFreeMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getApproximateActiveFreeLOAMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getApproximateActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType);
	
	virtual void mergeHeapStats(MM_HeapStats *heapStats);
	virtual void mergeHeapStats(MM_HeapStats *heapStats, uintptr_t includeMemoryType);
	virtual void resetHeapStatistics(bool globalCollect);
	virtual MM_AllocationFailureStats* getAllocationFailureStats();

	/**
	 * Determine whether the receiver is a leaf subspace (ie: has no children).
	 * @return true if the receiver has no children, false otherwise.
	 */
	MMINLINE bool isLeafSubSpace() { return NULL == _children; }

	virtual bool completeFreelistRebuildRequired(MM_EnvironmentBase *env);
	
	MMINLINE MM_Collector *getCollector() { return _collector; }
	MMINLINE void setCollector(MM_Collector *collector) { _collector = collector; }

	MMINLINE MM_MemorySpace *getMemorySpace() { return _memorySpace; }

	/**
	 * Generic allocation failure handler.  This will attempt to acquire exclusive VM access and perform a garbage collection to reclaim resources.  The given "allocationType"
	 * variable is consulted to determine which allocation routine is to be called, after the collection, but before giving up exclusive to ensure that the failing allocation
	 * succeeds before other allocations can be attempted.
	 * 
	 * @param[in] env The current thread
	 * @param[in] allocateDescription The allocation request which triggered the failure
	 * @param[in] objectAllocationInterface The allocation interface which attempted to satisfy the allocate (only relevant for TLH allocation failures, NULL if not a TLH allocation failure)
	 * @param[in] baseSubSpace The subspace which was first requested to satisfy the allocate
	 * @param[in] previousSubSpace The subspace which tried to handle the failure before calling the receiver
	 * 
	 * @return The address of the allocated object or base address of the allocated TLH.  NULL returned on failure. 
	 */
	virtual void *allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace) = 0;
	/**
	 * A generic helper which is used by most implementations of allocationRequestFailed to issue the actual allocation request between GC attempt stages.  It decodes
	 * the allocationType to determine which actual allocation call to issue (Object, Leaf, or TLH).
	 * 
	 * @param[in] env The current thread
	 * @param[in] allocateDescription The description of the allocate to issue
	 * @param[in] allocationType The type of entity we are trying to allocate
	 * @param[in] objectAllocationInterface The allocation interface through which the allocate request was issued (only relevant if the allocationType is TLH, otherwise this can be NULL)
	 * @param[in] attemptSubspace The subspace in which to issue the allocation call
	 * 
	 * @return The address of the allocation or NULL on failure (in the case of Object or Leaf allocation, this is the address of the allocate.  In the case of TLH,
	 * this is the base address of the TLH)
	 */
	void *allocateGeneric(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *attemptSubspace);
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure) = 0;

	/* TODO: RTJ Merge: Arraylet cleanup */
	virtual uintptr_t largestDesirableArraySpine() { return UDATA_MAX; /* (no limit) */ }
	
	/**
	 * Allocate an arraylet leaf.
	 */
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}

	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
	{
		Assert_MM_unreachable();
		return NULL;
	}
	
	/**
	 * Called under exclusive to request that the subspace attempt to replenish the given context and satisfy the given allocateDescription of the given allocationType
	 * 
	 * @param[in] env The current thread
	 * @param[in] context The allocation context which the sender failed to replenish
	 * @param[in] objectAllocationInterface The alocation interface through which the original allocation call was initiated (only used by TLH allocations, can be NULL in other cases)
	 * @param[in] allocateDescription The allocation request which initiated the allocation failure
	 * @param[in] allocationType The type of allocation request we eventually must satisfy
	 * 
	 * @return The result of the allocation attempted under exclusive
	 */
	virtual void *lockedReplenishAndAllocate(MM_EnvironmentBase *env, MM_AllocationContext *context, MM_ObjectAllocationInterface *objectAllocationInterface, MM_AllocateDescription *allocateDescription, AllocationType allocationType)
	{
		Assert_MM_unreachable();
		return NULL;
	}

	virtual void setAllocateAtSafePointOnly(MM_EnvironmentBase *env, bool safePoint);
	virtual bool shouldAllocateAtSafePointOnly() { return false; }

	/* Calls for internal collection routines */
	virtual void *collectorAllocate(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription);
	virtual void *collectorAllocateTLH(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription, uintptr_t maximumBytesRequired, void * &addrBase, void * &addrTop);
	virtual uintptr_t collectorExpand(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription);

	/**
	 * Convert the specified section of the subspace into an unused section.
	 * @parm addrBase the start address of the section
	 * @parm addrTop the first byte beyond the end of the region
	 */
	virtual void abandonHeapChunk(void *addrBase, void *addrTop) = 0;

	virtual MM_MemorySubSpace *getDefaultMemorySubSpace() = 0;
	virtual MM_MemorySubSpace *getTenureMemorySubSpace() = 0;
	
	/**
	 * Find top level parent MemorySubSpace that matches provided typeFlags. Assumption is that this memory space already matches the flags.
	 */
	MM_MemorySubSpace *getTopLevelMemorySubSpace(uintptr_t typeFlags);

	MMINLINE uintptr_t getTypeFlags() { return _memoryType; }		
	MMINLINE uint32_t getObjectFlags() { return _objectFlags; }

	virtual bool isActive();
	virtual bool isChildActive(MM_MemorySubSpace *memorySubSpace);

	virtual bool inflate(MM_EnvironmentBase *env);

	virtual void systemGarbageCollect(MM_EnvironmentBase *env, uint32_t gcCode);
	virtual bool percolateGarbageCollect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uint32_t gcCode);
	virtual bool garbageCollect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uint32_t gcCode);

	virtual void reset();
	virtual void rebuildFreeList(MM_EnvironmentBase *env);

	virtual void payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *baseSubSpace, MM_AllocateDescription *allocDescription);
	void payAllocationTax(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

	void reportAllocationFailureStart(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
	void reportAllocationFailureEnd(MM_EnvironmentBase *env);

	/**
	 * Report that exclusive access was acquired to satisfy an allocation.
	 * Exclusive access was force acquired in order to perform a GC and guarantee a succesful allocation.  However, upon
	 * acquiring exclusive access, the allocation was satisfied WITHOUT having to perform a GC.  This routine is called
	 * under those circumstances.
	 *
	 * @param env allocating thread.
	 * @param allocateDescription descriptor for the allocation that satisfied.
	 */
	void reportAcquiredExclusiveToSatisfyAllocate(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription);

	bool setResizable(bool resizable);

	bool canExpand(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t adjustExpansionWithinSoftMax(MM_EnvironmentBase *env, uintptr_t expandSize, uintptr_t minimumBytesRequired, uintptr_t memoryType = MEMORY_TYPE_OLD);
	virtual uintptr_t adjustExpansionWithinUserIncrement(MM_EnvironmentBase *env, uintptr_t expandSize);
	uintptr_t maxExpansion(MM_EnvironmentBase *env);
	virtual uintptr_t maxExpansionInSpace(MM_EnvironmentBase *env);

	virtual void checkResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription = NULL, bool _systemGC = false);
	virtual intptr_t performResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription = NULL);
	virtual uintptr_t expand(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t contract(MM_EnvironmentBase *env, uintptr_t contractSize);
	uintptr_t counterBalanceContract(MM_EnvironmentBase *env, uintptr_t contractSize, uintptr_t contractAlignment);
	virtual uintptr_t counterBalanceContract(MM_EnvironmentBase *env, MM_MemorySubSpace *previousSubSpace, MM_MemorySubSpace *contractSubSpace, uintptr_t contractSize, uintptr_t contractAlignment);
	virtual uintptr_t counterBalanceContractWithExpand(MM_EnvironmentBase *env, MM_MemorySubSpace *previousSubSpace, MM_MemorySubSpace *contractSubSpace, uintptr_t contractSize, uintptr_t contractAlignment, uintptr_t expandSize);

	bool canContract(MM_EnvironmentBase *env, uintptr_t contractSize);
	uintptr_t maxContraction(MM_EnvironmentBase *env);
	uintptr_t maxContractionInSpace(MM_EnvironmentBase *env);
	uintptr_t getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, void *lowAddr, void *highAddr);
	virtual bool expanded(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, MM_HeapRegionDescriptor *region, bool canCoalesce);
	virtual bool expanded(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, uintptr_t size, void *lowAddress, void *highAddress, bool canCoalesce);
	virtual void addExistingMemory(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, uintptr_t size, void *lowAddress, void *highAddress, bool canCoalesce);
	virtual void *removeExistingMemory(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, uintptr_t size, void *lowAddress, void *highAddress);
	
	/**
	 * Select a region for contraction, preferably on the specified NUMA node
	 * 
	 * @param env the environment
	 * @param numaNode the preferred node to contract, or 0 if no preference
	 * 
	 * @return the region to contract, or NULL if none available
	 */
	virtual MM_HeapRegionDescriptor * selectRegionForContraction(MM_EnvironmentBase *env, uintptr_t numaNode);
	
	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	/**
	 * Called after the heap geometry changes to allow any data structures dependent on this to be updated.
	 * This call could be triggered by memory ranges being added to or removed from the heap or memory being
	 * moved from one subspace to another.
	 * @param env[in] The thread which performed the change in heap geometry 
	 */
	virtual void heapReconfigured(MM_EnvironmentBase *env, HeapReconfigReason reason = HEAP_RECONFIG_NONE, MM_MemorySubSpace *subspace = NULL, void *lowAddress = NULL, void *highAddress = NULL);

	virtual void *findFreeEntryEndingAtAddr(MM_EnvironmentBase *env, void *addr);
	virtual void *findFreeEntryTopStartingAtAddr(MM_EnvironmentBase *env, void *addr);
	virtual void *getFirstFreeStartingAddr(MM_EnvironmentBase *env);
	virtual void *getNextFreeStartingAddr(MM_EnvironmentBase *env, void *currentFree);
	virtual void moveHeap(MM_EnvironmentBase *env, void *srcBase, void *srcTop, void *dstBase);

	virtual uintptr_t getAvailableContractionSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

	MMINLINE bool isConcurrentCollectable() { return _concurrentCollectable; }
	MMINLINE void setConcurrentCollectable() { _concurrentCollectable= true; }

	void enqueueCounterBalanceExpand(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t expandSize);
	void triggerEnqueuedCounterBalancing(MM_EnvironmentBase *env);
	void clearEnqueuedCounterBalancing(MM_EnvironmentBase *env);
	void runEnqueuedCounterBalancing(MM_EnvironmentBase *env);

#if defined(OMR_GC_CONCURRENT_SWEEP)
	virtual bool replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, uintptr_t size);
#endif /* OMR_GC_CONCURRENT_SWEEP */

	bool isDescendant(MM_MemorySubSpace *memorySubSpace);
	
	/**
	 * Some optimizations require knowing if this subspace is involved in a copying semi-space
	 */
	virtual bool isPartOfSemiSpace();
	
	/**
	 * Merge the stats from children memory subSpaces
	 * Not thread safe - caller has to make sure no other threads are modifying the stats for any of children.
	 */
	virtual void mergeLargeObjectAllocateStats(MM_EnvironmentBase *env) {}

	virtual void registerRegion(MM_HeapRegionDescriptor *region);
	virtual void unregisterRegion(MM_HeapRegionDescriptor *region);
	void lockRegionList();
	void unlockRegionList();
	bool wasContractedThisGC(uintptr_t gcCount);

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	virtual uintptr_t releaseFreeMemoryPages(MM_EnvironmentBase* env);
#endif

	/**
	 * Create a MemorySubSpace object.
	 */
	MM_MemorySubSpace(
		MM_EnvironmentBase *env, MM_Collector *collector, MM_PhysicalSubArena *physicalSubArena,
		bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize, uintptr_t memoryType, uint32_t objectFlags)
		: MM_BaseVirtual()
		, _next(NULL)
		, _previous(NULL)
		, _children(NULL)
		, _regionList(NULL)
		, _concurrentCollectable(false)		
		, _memoryType(memoryType)
		, _objectFlags(objectFlags)
		, _extensions(env->getExtensions())
		, _collector(collector)
		, _memorySpace(NULL)
		, _parent(NULL)
		, _physicalSubArena(physicalSubArena)
		, _initialSize(initialSize)
		, _minimumSize(minimumSize)
		, _currentSize(0)
		, _maximumSize(maximumSize)
		, _uniqueID(0)
		, _usesGlobalCollector(usesGlobalCollector)
		, _isAllocatable(true)
		, _counterBalanceType(MODRON_COUNTER_BALANCE_TYPE_NONE)
		, _counterBalanceSize(0)
		, _counterBalanceChainHead(NULL)
		, _counterBalanceChain(NULL)
		, _contractionSize(0)
		, _expansionSize(0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* MEMORYSUBSPACE_HPP_ */

