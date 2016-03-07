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

#if !defined(HEAPREGIONMANAGERTAROK_HPP)
#define HEAPREGIONMANAGERTAROK_HPP

#include "Heap.hpp"

#include "HeapRegionManager.hpp"

class MM_EnvironmentBase;
class MM_MemorySubSpace;
class MM_HeapRegionDescriptor;
class MM_HeapMemorySnapshot;


class MM_HeapRegionManagerTarok : public MM_HeapRegionManager
{
	/*
	 * Data members
	 */
private:
	uintptr_t _freeRegionTableSize; /**< the number of lists in the _freeRegionTable (always >= 1) */
	MM_HeapRegionDescriptor **_freeRegionTable; /**< a table of pointers to free region lists, each associated with a specific NUMA node */
protected:
public:

	/*
	 * Function members
	 */
public:
	MMINLINE static MM_HeapRegionManagerTarok *getHeapRegionManager(MM_Heap *heap) { return (MM_HeapRegionManagerTarok *)heap->getHeapRegionManager(); }
	MMINLINE static MM_HeapRegionManagerTarok *getHeapRegionManager(MM_HeapRegionManager *manager) { return (MM_HeapRegionManagerTarok *)manager; }

	static MM_HeapRegionManagerTarok *newInstance(MM_EnvironmentBase *env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor);

	MM_HeapRegionManagerTarok(MM_EnvironmentBase *env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor);

	/**
	 * Returns a region within the heap's contiguous region table, attached to subSpace, and describing one region size
	 * bytes of the heap.
	 *
	 * The manager will only return regions from the desired NUMA node. 
	 *
	 * @param env The environment
	 * @param subSPace The subSpace to associate the region with
	 * @param numaNode The NUMA node (indexed from 1) to allocate the region from
	 * @return a region for the caller's use, or NULL if none available
	 */
	MM_HeapRegionDescriptor *acquireSingleTableRegion(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t numaNode);

	/**
	 * find the nearest low valid addresses that's below the given region
	 * @param targetRegion The target region
	 * @return low valid address of the target region
	 */
	void *findHighestValidAddressBelow(MM_HeapRegionDescriptor *targetRegion);
	/**
	 * find the nearest high valid addresses that's above the given region
	 * @param targetRegion The target region
	 * @return high valid address of the target region
	 */
	void *findLowestValidAddressAbove(MM_HeapRegionDescriptor *targetRegion);

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
	virtual bool setContiguousHeapRange(MM_EnvironmentBase *env, void *lowHeapEdge, void *highHeapEdge);

	 /**
	  * Provide destruction of Region Table if necessary
	  * Use in heap shutdown (correspondent call with setContiguousHeapRange)
	  * @param env The environment
	  */
	 virtual void destroyRegionTable(MM_EnvironmentBase *env);

	/**
	 * @see MM_HeapRegionManager::enableRegionsInTable
	 */
	virtual bool enableRegionsInTable(MM_EnvironmentBase *env, MM_MemoryHandle *handle);

	virtual MM_HeapMemorySnapshot* getHeapMemorySnapshot(MM_GCExtensionsBase *extensions, MM_HeapMemorySnapshot* snapshot, bool gcEnd);

	/**
	 * Releases the heap regions described by the given rootRegion (joined through "_nextInSet" links) to the heap for other uses.
	 */
	void releaseTableRegions(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *rootRegion);

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	/**
	 * Sets the NUMA node for the regions in the heap range [lowHeapEdge..highHeapEdge) and then adds them to the free region
	 * table and internally links them together.
	 *
	 * @param env[in] The master GC thread
	 * @param lowHeapEdge[in] The address of the first byte of the first region to map
	 * @param highHeapEdge[in] The address of the first byte after the last region to map
	 * @param numaNode[in] The NUMA node to which the listed regions should be mapped
	 */
	void setNodeAndLinkRegions(MM_EnvironmentBase *env, void *lowHeapEdge, void *highHeapEdge, uintptr_t numaNode);

private:
	/**
	 * The implementation of acquireSingleTableRegion
	 *
	 * @param[in] env The environment
	 * @param[in] subSpace The supSpace which will own the region
	 * @param[in] freeListIndex The index of the free list which the region should be acquired from
	 *
	 * @return the new region
	 */
	MM_HeapRegionDescriptor *internalAcquireSingleTableRegion(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t freeListIndex);

	/**
	 * Update the regions as a linked list of single regions.
	 *
	 * @param[in] env The environment
	 * @param[in] headRegion The first region to update
	 * @param[in] count The number of regions to update starting from headRegion
	 *
	 */
	void internalLinkRegions(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *headRegion, uintptr_t count);

	/**
	 * The actual implementation of releaseTableRegions (called through the listener chain)
	 */
	void internalReleaseTableRegions(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *rootRegion);
};

#endif /* HEAPREGIONMANAGERTAROK_HPP */
