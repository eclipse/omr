/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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


/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(MEMORYSUBSPACEFLAT_HPP_)
#define MEMORYSUBSPACEFLAT_HPP_

#include "MemorySubSpaceUniSpace.hpp"

class MM_EnvironmentBase;
class MM_MemoryPool;
class MM_MemorySpace;
class MM_MemorySubSpace;
class MM_PhysicalSubArena;
class MM_ObjectAllocationInterface;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_MemorySubSpaceFlat : public MM_MemorySubSpaceUniSpace
{
protected:
	bool initialize(MM_EnvironmentBase *env);
	
	MM_MemorySubSpace *_memorySubSpace;
	
	MMINLINE void setMemorySubSpace(MM_MemorySubSpace *memorySubSpace) { _memorySubSpace = memorySubSpace; };
	MMINLINE MM_MemorySubSpace *getMemorySubSpace() { return _memorySubSpace; };
	virtual void *allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace);

public:
	static MM_MemorySubSpaceFlat *newInstance(MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemorySubSpace *childMemorySubSpace,
			bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize, uintptr_t memoryType, uint32_t objectFlags);

	virtual const char *getName() { return MEMORY_SUBSPACE_NAME_FLAT; }
	virtual const char *getDescription() { return MEMORY_SUBSPACE_DESCRIPTION_FLAT; }

	virtual MM_AllocationFailureStats *getAllocationFailureStats();

	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
	
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	/* Calls for internal collection routines */
	virtual void abandonHeapChunk(void *addrBase, void *addrTop);

	MMINLINE MM_MemorySubSpace *getChildSubSpace() { return _memorySubSpace; };

	virtual MM_MemorySubSpace *getDefaultMemorySubSpace();
	virtual MM_MemorySubSpace *getTenureMemorySubSpace();

	virtual uintptr_t collectorExpand(MM_EnvironmentBase *env, MM_Collector *requestCollector, MM_AllocateDescription *allocDescription);

	virtual uintptr_t adjustExpansionWithinUserIncrement(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t maxExpansionInSpace(MM_EnvironmentBase *env);
	virtual bool expanded(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, MM_HeapRegionDescriptor *region, bool canCoalesce);
	virtual bool expanded(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, uintptr_t size, void *lowAddress, void *highAddress, bool canCoalesce);
	virtual uintptr_t getAvailableContractionSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);	
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	virtual uintptr_t releaseFreeMemoryPages(MM_EnvironmentBase* env);
#endif

	/**
	 * Create a new MM_MemorySubSpaceFlat object
	 */
	MM_MemorySubSpaceFlat(
		MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemorySubSpace *memorySubSpace,
		bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize,
		uintptr_t memoryType, uint32_t objectFlags)
	:
		MM_MemorySubSpaceUniSpace(env, physicalSubArena, usesGlobalCollector, minimumSize, initialSize, maximumSize, memoryType, objectFlags)
		,_memorySubSpace(memorySubSpace)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* MEMORYSUBSPACEFLAT_HPP_ */
