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

#if !defined(HEAP_HPP_)
#define HEAP_HPP_

#include "omrcomp.h"
#include "modronbase.h"
#include "omr.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapResizeStats.hpp"
#include "PercolateStats.hpp"

class MM_HeapRegionDescriptor;
class MM_HeapRegionManager;
class MM_HeapStats;
class MM_MemorySpace;
class MM_MemorySubSpace;
class MM_PhysicalArena;

class MM_Heap : public MM_BaseVirtual
{
/*
* Data members
*/
private:
protected:
	OMR_VM *_omrVM;
	OMRPortLibrary *_portLibrary;

	MM_MemorySpace *_defaultMemorySpace;
	MM_MemorySpace *_memorySpaceList;

	uintptr_t _maximumMemorySize;

	MM_HeapResizeStats _heapResizeStats;
	MM_PercolateStats _percolateStats;

	MM_HeapRegionManager *_heapRegionManager;

public:

/*
* Function members
*/
private:
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
public:
	virtual void kill(MM_EnvironmentBase *env);

	virtual uintptr_t calculateOffsetFromHeapBase(void*) = 0;

	MMINLINE MM_HeapResizeStats *getResizeStats() { return &_heapResizeStats; }

	MMINLINE MM_PercolateStats *getPercolateStats() { return &_percolateStats; }

	MMINLINE MM_MemorySpace *getDefaultMemorySpace() { return _defaultMemorySpace; }
	MMINLINE void setDefaultMemorySpace(MM_MemorySpace *memorySpace) { _defaultMemorySpace = memorySpace; }
	MMINLINE MM_MemorySpace *getMemorySpaceList() { return _memorySpaceList; }
	MMINLINE MM_HeapRegionManager *getHeapRegionManager() { return _heapRegionManager; }

	uintptr_t getMemorySize();
	MMINLINE uintptr_t getMaximumMemorySize() { return _maximumMemorySize; }
	uintptr_t getActualFreeMemorySize();
	uintptr_t getApproximateFreeMemorySize();

	uintptr_t getActiveMemorySize();
	uintptr_t getActualActiveFreeMemorySize();
	uintptr_t getApproximateActiveFreeMemorySize();

	uintptr_t getActiveMemorySize(uintptr_t includeMemoryType);
	uintptr_t getActualActiveFreeMemorySize(uintptr_t includeMemoryType);
	uintptr_t getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType);

	uintptr_t getActiveLOAMemorySize(uintptr_t includeMemoryType);
	uintptr_t getApproximateActiveFreeLOAMemorySize(uintptr_t includeMemoryType);

	uintptr_t getActiveSurvivorMemorySize(uintptr_t includeMemoryType);
	uintptr_t getApproximateActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType);

	/**
	 * Return the page size used for the heap memory.  This should only be used for 
	 * reporting purposes only!  (e.g. In MM_HeapSplit, the low extent and high extent
	 * may have different size pages, in this case the minimum value is returned)
	 */
	virtual uintptr_t getPageSize() = 0;

	/**
	 * Return the page flags describing the pages used for the heap memory.
	 */
	virtual uintptr_t getPageFlags() = 0;
	
	virtual void *getHeapBase() = 0;
	virtual void *getHeapTop() = 0;
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
	virtual void *doubleMapArraylet(MM_EnvironmentBase *env, void* arrayletLeaves[], UDATA arrayletLeafCount, UDATA arrayletLeafSize, UDATA byteAmount, struct J9PortVmemIdentifier *newIdentifier, UDATA pageSize) = 0;
	virtual void *doubleMapRegions(MM_EnvironmentBase *env, void* regions[], UDATA regionsCount, UDATA regionSize, UDATA byteAmount, struct J9PortVmemIdentifier *newIdentifier, UDATA pageSize, void *preferredAddress) = 0;
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */

	virtual uintptr_t getMaximumPhysicalRange() = 0;

	virtual bool attachArena(MM_EnvironmentBase *env, MM_PhysicalArena *arena, uintptr_t size) = 0;
	virtual void detachArena(MM_EnvironmentBase *env, MM_PhysicalArena *arena) = 0;

	virtual bool commitMemory(void *address, uintptr_t size) = 0;
	virtual bool decommitMemory(void *address, uintptr_t size, void *lowValidAddress, void *highValidAddress) = 0;

	void mergeHeapStats(MM_HeapStats *heapStats, uintptr_t includeMemoryType);
	void mergeHeapStats(MM_HeapStats *heapStats);
	void resetHeapStatistics(bool globalCollect);

	void resetLargestFreeEntry();

	void registerMemorySpace(MM_MemorySpace *memorySpace);
	void unregisterMemorySpace(MM_MemorySpace *memorySpace);

	void systemGarbageCollect(MM_EnvironmentBase *env, uint32_t gcCode);
	void resetSpacesForGarbageCollect(MM_EnvironmentBase *env);

	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	/**
	 * Called after the heap geometry changes to allow any data structures dependent on this to be updated.
	 * This call could be triggered by memory ranges being added to or removed from the heap or memory being
	 * moved from one subspace to another.
	 * @param env[in] The thread which performed the change in heap geometry 
	 */
	virtual void heapReconfigured(MM_EnvironmentBase *env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress);

	virtual bool objectIsInGap(void *object);

	/**
	 * Called after the heap has been initialized so that it can properly initialize HeapRegionManager.
	 *
	 * @param env[in] the thread which performed the initialization
	 * @param manager[in] - the manager to initialize for this heap
	 */
	virtual bool initializeHeapRegionManager(MM_EnvironmentBase *env, MM_HeapRegionManager *manager) = 0;

	struct MM_CommonGCData* initializeCommonGCData(MM_EnvironmentBase *env, struct MM_CommonGCData *data);
	void initializeCommonGCStartData(MM_EnvironmentBase *env, struct MM_CommonGCStartData *data);
	void initializeCommonGCEndData(MM_EnvironmentBase *env, struct MM_CommonGCEndData *data);

	uintptr_t getActualSoftMxSize(MM_EnvironmentBase *env, uintptr_t memoryType = MEMORY_TYPE_OLD);

	/**
	 * Create a Heap object.
	 */
	MM_Heap(MM_EnvironmentBase *env, uintptr_t maximumMemorySize, MM_HeapRegionManager *regionManager) :
		MM_BaseVirtual()
		,_omrVM(env->getOmrVM())
		,_portLibrary(env->getPortLibrary())
		,_defaultMemorySpace(NULL)
		,_memorySpaceList(NULL)
		,_maximumMemorySize(maximumMemorySize)
		,_heapResizeStats()
		,_percolateStats()
		,_heapRegionManager(regionManager)
	{
		_typeId = __FUNCTION__;
	}
};


#endif /* HEAP_HPP_ */
