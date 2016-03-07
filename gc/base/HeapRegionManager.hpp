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
 ******************************************************************************/

#if !defined(HEAPREGIONMANAGER_HPP)
#define HEAPREGIONMANAGER_HPP

#include "omrcomp.h"
#include "modronbase.h"

#include "BaseVirtual.hpp"
#include "HeapRegionDescriptor.hpp"
#include "LightweightNonReentrantReaderWriterLock.hpp"
#include "ModronAssertions.h"

class MM_EnvironmentBase;
class MM_HeapRegionManager;
class MM_MemoryHandle;
class MM_MemorySubSpace;
class MM_HeapMemorySnapshot;

/**
 * A function pointer used to initialize newly allocated HRDs.
 * 
 * The function is responsible for invoking a constructor on the descriptor.
 *
 * @param[in] env - the current env
 * @param[in] regionManager - the regionManager which owns the descriptor 
 * @param[in] descriptor - the uninitialized descriptor
 * @param[in] lowAddress - the lowest address in the region
 * @param[in] highAddress - the address of the first byte beyond the region
 * 
 * @return true if the initialization succeeded, false otherwise 
 */
typedef bool (*MM_RegionDescriptorInitializer)(MM_EnvironmentBase* env, MM_HeapRegionManager* regionManager, MM_HeapRegionDescriptor* descriptor, void* lowAddress, void* highAddress);

/**
 * A function pointer used to destroy HRDs on shutdown.
 * 
 * @param[in] env - the current env
 * @param[in] regionManager - the regionManager which owns the descriptor 
 * @param[in] descriptor - the descriptor to be destroyed
 */
typedef void (*MM_RegionDescriptorDestructor)(MM_EnvironmentBase* env, MM_HeapRegionManager* regionManager, MM_HeapRegionDescriptor* descriptor);

class MM_HeapRegionManager : public MM_BaseVirtual {
	friend class GC_HeapRegionIterator;

public:
protected:
	friend class MM_InterRegionRememberedSet;
	MM_LightweightNonReentrantReaderWriterLock _heapRegionListMonitor; /**< monitor that controls modification of the heap region linked list */
	MM_HeapRegionDescriptor* _auxRegionDescriptorList; /**< address ordered doubly linked list for auxiliary heap regions */
	uintptr_t _auxRegionCount; /**< number of heap regions on the auxiliary list */

	/* Heap Region Table data */
	uintptr_t _regionSize; /**< the size, in bytes, of a region in the _regionTable */
	uintptr_t _regionShift; /**< the shift value to use against pointers to determine the corresponding region index */
	MM_HeapRegionDescriptor* _regionTable; /**< the raw array of fixed-sized regions for representing a flat heap */
	uintptr_t _tableRegionCount; /**< number of heap regions on the fixed-sized table (_regionTable) */
	void* _lowTableEdge; /**< the first (lowest address) byte of heap which is addressable by the table */
	void* _highTableEdge; /**< the first byte AFTER the heap range which is addressable by the table */
	uintptr_t _tableDescriptorSize; /**< The size, in bytes, of the HeapRegionDescriptor subclass used by this manager */
	MM_RegionDescriptorInitializer _regionDescriptorInitializer; /**< A function pointer used to initialize a newly allocated HRD */
	MM_RegionDescriptorDestructor _regionDescriptorDestructor; /**< A function pointer used to destroy HRDs, or NULL */

	uintptr_t _totalHeapSize; /**< The size, in bytes, of all currently active regions on the heap (that is, both table descriptors attached to subspaces and aux descriptors in the list) */

public:
	void kill(MM_EnvironmentBase* env);

	MM_HeapRegionManager(MM_EnvironmentBase* env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor);

	static MM_HeapRegionManager *newInstance(MM_EnvironmentBase *env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor);

	/**
	 * Returns the total size of the committed heap (includes both table regions with subspaces and aux regions)
	 * @return The total size of the committed heap, in bytes.
	 */
	uintptr_t getTotalHeapSize();

	/**
	 * Register an auxiliary region with the manager and create a descriptor 
	 * @param env MM_EnvironmentBase object
	 * @param subSpace the mem subspace the region belongs to
	 * @param lowAddress low address of the region
	 * @param highAddress high bound address of the region
	 */
	MM_HeapRegionDescriptor* createAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, void* lowAddress, void* highAddress);

	/**
	 * Deregister an auxiliary region with the manager and destroy its descriptor
	 * @param env MM_EnvironmentBase object
	 * @param descriptor region descriptor to be removed from the manager
	 */
	void destroyAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* descriptor);

	MM_HeapRegionDescriptor*
	getFirstAuxiliaryRegion()
	{
		return _auxRegionDescriptorList;
	}

	MM_HeapRegionDescriptor*
	getNextAuxiliaryRegion(MM_HeapRegionDescriptor* heapRegion)
	{
		return heapRegion->_nextRegion;
	}

	MM_HeapRegionDescriptor* getFirstTableRegion();
	MM_HeapRegionDescriptor* getNextTableRegion(MM_HeapRegionDescriptor* heapRegion);

	/**
	 * Lock the region manager.
	 * This prevents any regions from being added or removed until the manager is unlocked.
	 * This is a read-write lock, so multiple readers may hold the lock simultaneously.
	 */
	void lock();

	/**
	 * Unlock the region manager.
	 * @see lock()
	 */
	void unlock();

	/**
	 * Lock the region manager.
	 * This prevents any regions from being added or removed until the manager is unlocked.
	 * This is a write lock, so only one writer may hold the lock at a time.
	 */
	void writeLock();

	/**
	 * Unlock the region manager from a writeLock().
	 */
	void writeUnlock();

	/**
	 * Called as soon as the bounds of the contiguous heap are known (in the case of split heaps, this would also include the "gap").
	 * Calls to this method have the side-effect of allocating internal data structures to manage the regions backing this contiguous heap
	 * so a boolean return value is provided to inform the caller if these internal data structures were successfully initialized.
	 * 
	 * The is expected to be called exactly once on each HRM.
	 * 
	 * @param env The environment
	 * @param lowHeapEdge the lowest byte addressable in the contiguous heap (note that this might not be presently committed)
	 * @param highHeapEdge the byte after the highest byte addressable in the contiguous heap
	 * @return true if this manager succeeded in initializing internal data structures to manage this heap or false if an error occurred (this is
	 * generally fatal)
	 */
	virtual bool setContiguousHeapRange(MM_EnvironmentBase* env, void* lowHeapEdge, void* highHeapEdge)
	{
		/* TODO: Might need logic from HeapRegionManagerStandard */
		return true;
	}

	/**
	  * Provide destruction of Region Table if necessary
	  * Use in heap shutdown (correspondent call with setContiguousHeapRange)
	  * @param env The environment
	  */
	virtual void destroyRegionTable(MM_EnvironmentBase* env)
	{
		Assert_MM_unreachable();
	}

	/**
	 * Retrieve current Heap snapshot
	 * free and total memory sizes for all memory pools
	 * sub class of HeapRegionManager would override this method to provide proper memory pools
	 */
	virtual MM_HeapMemorySnapshot* getHeapMemorySnapshot(MM_GCExtensionsBase *extensions, MM_HeapMemorySnapshot* snapshot, bool gcEnd)
	{
		Assert_MM_unreachable();
		return NULL;
	}

	/**
	 * @return the size, in bytes, of a single region in the contiguous heap (note that AUX regions are not consulted, and spanning regions will be some multiple
	 * of this number)
	 */
	MMINLINE uintptr_t getRegionSize()
	{
		return _regionSize;
	}
	MMINLINE uintptr_t getRegionShift()
	{
		return _regionShift;
	}
	MMINLINE uintptr_t getTableRegionCount() const
	{
		return _tableRegionCount;
	}
	uintptr_t getHeapSize()
	{
		return (uintptr_t)_highTableEdge - (uintptr_t)_lowTableEdge;
	}
	uintptr_t getTotalHeapSizeInBytes()
	{
		return _totalHeapSize;
	}

	/**
	 * Get the physical table descriptor at the specified index.
	 * 
	 * @param regionIndex - the index of the region - this is not bounds checked
	 * @return the region descriptor at the specified index 
	 */
	MMINLINE MM_HeapRegionDescriptor* physicalTableDescriptorForIndex(uintptr_t regionIndex)
	{
		uintptr_t descriptorSize = _tableDescriptorSize;
		uintptr_t regionTable = (uintptr_t)_regionTable;
		return (MM_HeapRegionDescriptor*)(regionTable + (regionIndex * descriptorSize));
	}

	/**
	 * Get the index of the physical table descriptor that describes the address
	 * 
	 * @param heapAddress - the address whose region we are trying to find - this is not bounds checked
	 * @return the index into table
	 */
	MMINLINE uintptr_t physicalTableDescriptorIndexForAddress(const void* heapAddress)
	{
		void* baseOfHeap = (_regionTable)->getLowAddress();
		uintptr_t heapDelta = (uintptr_t)heapAddress - (uintptr_t)baseOfHeap;
		uintptr_t index = heapDelta >> _regionShift;
		return index;
	}

	/**
	 * Get the physical table descriptor that describes the address
	 * 
	 * @param heapAddress - the address whose region we are trying to find - this is not bounds checked
	 * @return the region descriptor for the specified address 
	 */
	MMINLINE MM_HeapRegionDescriptor* physicalTableDescriptorForAddress(const void* heapAddress)
	{
		uintptr_t index = physicalTableDescriptorIndexForAddress(heapAddress);
		return physicalTableDescriptorForIndex(index);
	}

	/**
	 * Get the logical table descriptor at the specified index. Note that the region described
	 * could be part of a spanning region so return the headOfSPan
	 * 
	 * @param regionIndex - the index of the region - this is not bounds checked
	 * @return the region descriptor at the specified index 
	 */
	MMINLINE MM_HeapRegionDescriptor* tableDescriptorForIndex(uintptr_t regionIndex)
	{
		MM_HeapRegionDescriptor* tableDescriptor = physicalTableDescriptorForIndex(regionIndex);
		return tableDescriptor->_headOfSpan;
	}

	/**
	 * Get the logical table descriptor that describes the address. Note that the region described
	 * could be part of a spanning region so return the headOfSPan
	 * 
	 * @param heapAddress - the address whose region we are trying to find
	 * @return the region descriptor for the specified address 
	 */
	MMINLINE MM_HeapRegionDescriptor* tableDescriptorForAddress(const void* heapAddress)
	{
		Assert_MM_true(heapAddress >= _lowTableEdge);
		Assert_MM_true(heapAddress < _highTableEdge);
		MM_HeapRegionDescriptor* tableDescriptor = physicalTableDescriptorForAddress(heapAddress);
		return tableDescriptor->_headOfSpan;
	}

	/**
	 * Get the logical region descriptor that describes the address. The region may be a table
	 * region or an auxillar region.  Note that the region described could be part of a spanning 
	 * region so return the headOfSPan for table regions.
	 * 
	 * @param heapAddress - the address whose region we are trying to find
	 * @return the region descriptor for the specified address 
	 */
	MMINLINE MM_HeapRegionDescriptor* regionDescriptorForAddress(const void* heapAddress)
	{
		MM_HeapRegionDescriptor* result = NULL;
		if ((heapAddress >= _lowTableEdge) && (heapAddress < _highTableEdge)) {
			result = tableDescriptorForAddress(heapAddress);
		} else {
			result = auxillaryDescriptorForAddress(heapAddress);
		}

		return result;
	}

	/**
	 * Set the associated subspace of the given region to subSpace.
	 */
	void reassociateRegionWithSubSpace(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region, MM_MemorySubSpace* subSpace);


	MM_HeapRegionDescriptor* auxillaryDescriptorForAddress(const void* address);
	MMINLINE void resizeAuxillaryRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region, void* lowAddress, void* highAddress)
	{
		writeLock();
		_totalHeapSize -= region->getSize();
		region->reinitialize(env, lowAddress, highAddress);
		_totalHeapSize += region->getSize();
		writeUnlock();
	}

	/**
	 * Instructs the receiver that the regions representing the heap range managed by vmem are valid for use.
	 * 
	 * @param[in] env The current thread
	 * @param[in] handle The handle to Virtual Memory object describing the range we want to enable
	 * 
	 * @return false if the table failed to be allocated or initialized (this is a fatal error in GC start-up)
	 */
	virtual bool enableRegionsInTable(MM_EnvironmentBase* env, MM_MemoryHandle* handle)
	{
		return true;
	}

	/**
	 * Given an index, return the matching region descriptor.
	 * @param index the 0-based index of the region to fetch
	 * @return the region at the specified index 
	 */
	MM_HeapRegionDescriptor* mapRegionTableIndexToDescriptor(uintptr_t index);

	/**
	 * Given a region, return its index in the table.
	 * @param region[in] the region to examine
	 * @return the region's 0-based index in the table
	 */
	uintptr_t mapDescriptorToRegionTableIndex(MM_HeapRegionDescriptor* region);

protected:
	virtual bool initialize(MM_EnvironmentBase* env);
	virtual void tearDown(MM_EnvironmentBase* env);

	/**
	 * Implemented by sub-classes to allocate and initialize a single AUX region descriptor for the extent:  [lowAddress, highAddress).  This is required since
	 * the sub-class is allowed to implement descriptors as any sub-class of MM_HeapRegionDescriptor and the caller cannot assume how its storage is managed.
	 * The returned descriptor will later be passed to freeAuxiliaryRegionDescriptor when it is no longer needed.
	 * 
	 * @param env The environment
	 * @param lowAddress the lowest byte addressable in the AUX region (note that this might not be presently committed)
	 * @param highAddress the byte after the highest byte addressable in the AUX region
	 * @return an instance of an implementation-defined MM_HeapRegionDescriptor sub-class on success or NULL on failure
	 */
	virtual MM_HeapRegionDescriptor* internalAllocateAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, void* lowAddress, void* highAddress);

	/**
	 * Implemented by sub-classes to free (and otherwise dispose of) a single AUX region descriptor (region).  This is required since the sub-class is
	 * allowed to implement descriptors as any sub-class of MM_HeapRegionDescriptor and the caller cannot assume how its storage is managed.
	 * 
	 * @param env The environment
	 * @param region The AUX region to be freed by the implementor (previously returned by allocateAuxiliaryRegionDescriptor)
	 */
	virtual void internalFreeAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region);

	/**
	 * Implemented by sub-classes to allocate and initialize the contiguous region descriptor table for contiguous heap extent [lowHeapEdge, highHeapEdge).
	 * This is required since the sub-class is allowed to implement the descriptor table as a contiguous array of any sub-class of MM_HeapRegionDescriptor
	 * and the caller cannot assume how its storage is managed.  The returned descriptor will later be passed to freeRegionTable when it is no longer needed.
	 * 
	 * @param env The environment
	 * @param lowHeapEdge the lowest byte addressable in the contiguous heap (note that this might not be presently committed)
	 * @param highHeapEdge the byte after the highest byte addressable in the contiguous heap
	 * @return an instance of an implementation-defined MM_HeapRegionDescriptor sub-class on success or NULL on failure
	 */
	virtual MM_HeapRegionDescriptor* internalAllocateAndInitializeRegionTable(MM_EnvironmentBase* env, void* lowHeapEdge, void* highHeapEdge);

	/**
	 * Implemented by sub-classes to free (and otherwise dispose of) the contiguous region descriptor table starting at tableBase.  This is required since the
	 * sub-class is allowed to implement the descriptor table as a contiguous array of any sub-class of MM_HeapRegionDescriptor and the caller cannot assume
	 * how its storage is managed.
	 * 
	 * @param env The environment
	 * @param tableBase The base of the region table to be freed by the implementor (previously returned by allocateAndInitializeRegionTable)
	 * @param tableElementCount The number of contiguous descriptor elements in the table
	 */
	virtual void internalFreeRegionTable(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* tableBase, uintptr_t tableElementCount);

private:
	/**Insert a MM_HeapRegionDescriptor in the address ordered list
	 * @param env MM_EnvironmentBase object
	 * @param heapRegion MM_HeapRegionDescriptor to be inserted
	 */
	void insertHeapRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* heapRegion);

	/**Remove a MM_HeapRegionDescriptor from the address ordered list
	 * @param env MM_EnvironmentBase object
	 * @param heapRegion MM_HeapRegionDescriptor to be removed
	 */
	void removeHeapRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* heapRegion);

	/**
	 * The actual implementation of createAuxiliaryRegionDescriptor (called through the listener chain)
	 */
	MM_HeapRegionDescriptor* internalCreateAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, void* lowAddress, void* highAddress);

	/**
	 * The actual implementation of destroyAuxiliaryRegionDescriptor (called through the listener chain)
	 */
	void internalDestroyAuxiliaryRegionDescriptor(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* descriptor);

	MM_HeapRegionDescriptor* findFirstUsedRegion(MM_HeapRegionDescriptor* start);
};

#endif /* HEAPREGIONDESCRIPTOR_HPP */
