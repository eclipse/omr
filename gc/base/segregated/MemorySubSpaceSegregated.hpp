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

#if !defined(MEMORYSUBSPACESEGREGATED_HPP_)
#define MEMORYSUBSPACESEGREGATED_HPP_

#include "omrcfg.h"

#include "MemorySubSpaceUniSpace.hpp"
#include "RegionPoolSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_GCCode;
class MM_MemoryPool;
class MM_MemoryPoolSegregated;
class MM_MemorySpace;

class MM_MemorySubSpaceSegregated : public MM_MemorySubSpaceUniSpace
{
	/*
	 * Data members
	 */
private:
	void *_regionExpansionBase;
	void *_regionExpansionTop;

protected:
	typedef enum AllocateType {
		mixedObject = 0,
		arrayletSpine,
		arrayletLeaf
	} AllocateType;
	
	MM_MemoryPoolSegregated *_memoryPoolSegregated;

public:

	/*
	 * Function members
	 */
private:
	/* TODO: this is temporary as a way to avoid dup code in MemorySubSpaceMetronome::allocate.
	 * We will specifically fix this allocate method in a separate design.
	 */
	void *allocateMixedObjectOrArraylet(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, AllocateType allocType);

	MMINLINE void expandRegionPool()
	{
		if (_regionExpansionBase != _regionExpansionTop) {
			_memoryPoolSegregated->getRegionPool()->addFreeRange(_regionExpansionBase, _regionExpansionTop);
			_regionExpansionBase = _regionExpansionTop;
		}
	}

protected:
	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	void *allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace);
	void *allocate(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, AllocateType allocType);

public:
	static MM_MemorySubSpaceSegregated *newInstance(
		MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemoryPool *memoryPool,
		bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize);

	virtual const char *getName() { return MEMORY_SUBSPACE_NAME_UNDEFINED; }
	virtual const char *getDescription() { return MEMORY_SUBSPACE_DESCRIPTION_UNDEFINED; }

	virtual void collect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);

#if defined(OMR_GC_ARRAYLETS)
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure);
	virtual uintptr_t largestDesirableArraySpine();
#endif /* defined(OMR_GC_ARRAYLETS) */

	/* Calls for internal collection routines */
	virtual void abandonHeapChunk(void *addrBase, void *addrTop);

	virtual MM_MemorySubSpace *getDefaultMemorySubSpace();
	virtual MM_MemorySubSpace *getTenureMemorySubSpace();

	virtual uintptr_t getActualFreeMemorySize();
	virtual uintptr_t getApproximateFreeMemorySize();
	virtual uintptr_t getActiveMemorySize();
	virtual uintptr_t getActualActiveFreeMemorySize();
	virtual uintptr_t getApproximateActiveFreeMemorySize();
	virtual uintptr_t getActiveMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getActualActiveFreeMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType);
	virtual uintptr_t getAvailableContractionSize(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription) { return 0; }

	virtual bool expanded(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, MM_HeapRegionDescriptor *region, bool canCoalesce);

	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	virtual void heapReconfigured(MM_EnvironmentBase *env);

	virtual MM_MemoryPool *getMemoryPool();
	
	MM_MemorySubSpaceSegregated(
		MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemoryPool *memoryPool,
		bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize
	)
		: MM_MemorySubSpaceUniSpace(env, physicalSubArena, usesGlobalCollector, minimumSize, initialSize, maximumSize, MEMORY_TYPE_OLD, 0)
		,_regionExpansionBase(NULL)
		,_regionExpansionTop(NULL)
		, _memoryPoolSegregated((MM_MemoryPoolSegregated *)memoryPool)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* MEMORYSUBSPACESEGREGATED_HPP_ */

