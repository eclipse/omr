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

#include "omrcfg.h"
#include "ModronAssertions.h"
#include "objectdescription.h"
#include "sizeclasses.h"

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "EnvironmentLanguageInterface.hpp"
#include "GCCode.hpp"
#include "Heap.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "RegionPoolSegregated.hpp"

#include "MemorySubSpaceSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/*
 * Allocation
 */

void *
MM_MemorySubSpaceSegregated::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	void *result = NULL;

	if (shouldCollectOnFailure) {
		result = allocateMixedObjectOrArraylet(env, allocDescription, mixedObject);
	} else {
		/* not setting these object flags causes a failure working with empty arrays but where this is set should probably be hoisted into the caller */
		allocDescription->setObjectFlags(getObjectFlags());
		result = _memoryPoolSegregated->allocateObject(env, allocDescription);
	}
	return result;
}

#if defined(OMR_GC_ARRAYLETS)
/**
 * Allocate an arraylet leaf.
 */
void *
MM_MemorySubSpaceSegregated::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	omrarrayptr_t spine = allocDescription->getSpine();
	/* spine object refers to the barrier-safe "object" reference, as opposed to the internal "heap address" represented by the spine variable */
	omrobjectptr_t spineObject = (omrobjectptr_t) spine;
	void *leaf = NULL;
	if(env->_envLanguageInterface->saveObjects(spineObject)) {
		leaf = allocateMixedObjectOrArraylet(env, allocDescription, arrayletLeaf);
		env->_envLanguageInterface->restoreObjects(&spineObject);
		spine = (omrarrayptr_t)spineObject;
		allocDescription->setSpine(spine);
	}
	return leaf;
}
#endif /* OMR_GC_ARRAYLETS */

void *
MM_MemorySubSpaceSegregated::allocate(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, AllocateType allocType)
{
	void *result = NULL;
	switch (allocType) {
	case mixedObject:
	case arrayletSpine:
		result = _memoryPoolSegregated->allocateObject(env, allocDescription);
		break;
#if defined(OMR_GC_ARRAYLETS)
	case arrayletLeaf:
		result = _memoryPoolSegregated->allocateArrayletLeaf(env, allocDescription);
		break;
#endif /* defined(OMR_GC_ARRAYLETS) */
	default:
		Assert_MM_unreachable();
	}
	return result;
}

void *
MM_MemorySubSpaceSegregated::allocateMixedObjectOrArraylet(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, AllocateType allocType)
{
	void *result = NULL;

	allocDescription->setObjectFlags(getObjectFlags());

	result = allocate(env, allocDescription, allocType);
	if (NULL != result) {
		return result;
	}

	if (NULL != _collector) {
		allocDescription->saveObjects(env);
		if (!env->acquireExclusiveVMAccessForGC(_collector)) {
			/* we have exclusive access but another thread beat us to the GC so see if they collected enough to satisfy our request */
			allocDescription->restoreObjects(env);
			result =  allocate(env, allocDescription, allocType);
			if (NULL != result) {
				/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
				 * an event for tracing / verbose to report the occurrence.
				 */
				reportAcquiredExclusiveToSatisfyAllocate(env, allocDescription);
				return result;
			}

			/* Failed to satisfy allocate - now really go for a GC */
			allocDescription->saveObjects(env);
			/* acquire exclusive access and, after we get it, see if we need to perform a collect or if someone else beat us to it */
			if (!env->acquireExclusiveVMAccessForGC(_collector)) {
				/* we have exclusive access but another thread beat us to the GC so see if they collected enough to satisfy our request */
				allocDescription->restoreObjects(env);
				result = allocate(env, allocDescription, allocType);
				if (NULL != result) {
					/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
					 * an event for tracing / verbose to report the occurrence.
					 */
					reportAcquiredExclusiveToSatisfyAllocate(env, allocDescription);
					return result;
				}

				/* we still failed the allocate so try a resize to get more space */
				reportAllocationFailureStart(env, allocDescription);

				/* OMRTODO implement proper heap resizing in segregated heaps */
				/* performResize(env, allocDescription); */

				result = allocate(env, allocDescription, allocType);

				if (NULL != result) {
					/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
					 * an event for tracing / verbose to report the occurrence.
					 */
					reportAcquiredExclusiveToSatisfyAllocate(env, allocDescription);
					reportAllocationFailureEnd(env);
					return result;
				}
				allocDescription->saveObjects(env);

				/* we still failed so we will need to run a GC */

			} else {
				/* we have exclusive and no other thread beat us to it so we can now collect */
				reportAllocationFailureStart(env, allocDescription);
			}
		} else {
			/* we have exclusive and no other thread beat us to it so we can now collect */
			reportAllocationFailureStart(env, allocDescription);
		}

		Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

		/* run the collector in the default mode (ie:  not explicitly aggressive) */
		result = _collector->garbageCollect(env, this, allocDescription, J9MMCONSTANT_IMPLICIT_GC_DEFAULT, NULL, NULL, NULL);
		allocDescription->restoreObjects(env);
		result = allocate(env, allocDescription, allocType);

		if (NULL != result) {
			reportAllocationFailureEnd(env);
			return result;
		}

		allocDescription->saveObjects(env);
		/* The collect wasn't good enough to satisfy the allocate so attempt an aggressive collection */
		result = _collector->garbageCollect(env, this, allocDescription, J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE, NULL, NULL, NULL);
		allocDescription->restoreObjects(env);
		result = allocate(env, allocDescription, allocType);

		reportAllocationFailureEnd(env);

		if (result) {
			return result;
		}
		/* there was nothing we could do to satisfy the allocate at this level (we will either OOM or attempt a collect at the higher level in our parent) */
	}

	return result;
}

void *
MM_MemorySubSpaceSegregated::allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace)
{
	return allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace);
}

#if defined(OMR_GC_ARRAYLETS)
uintptr_t
MM_MemorySubSpaceSegregated::largestDesirableArraySpine()
{
	return OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES;
}
#endif /* defined(OMR_GC_ARRAYLETS) */

void
MM_MemorySubSpaceSegregated::collect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	Assert_MM_unreachable();
}


void
MM_MemorySubSpaceSegregated::abandonHeapChunk(void *addrBase, void *addrTop)
{
}

MM_MemorySubSpace *
MM_MemorySubSpaceSegregated::getDefaultMemorySubSpace()
{
	return this;
}

MM_MemorySubSpace *
MM_MemorySubSpaceSegregated::getTenureMemorySubSpace()
{
	return this;
}

MM_MemorySubSpaceSegregated *
MM_MemorySubSpaceSegregated::newInstance(
	MM_EnvironmentBase *env, MM_PhysicalSubArena *physicalSubArena, MM_MemoryPool *memoryPool,
	bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t initialSize, uintptr_t maximumSize)
{
	MM_MemorySubSpaceSegregated *memorySubSpace;

	memorySubSpace = (MM_MemorySubSpaceSegregated *)env->getForge()->allocate(sizeof(MM_MemorySubSpaceSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != memorySubSpace) {
		new(memorySubSpace) MM_MemorySubSpaceSegregated(env, physicalSubArena, memoryPool, usesGlobalCollector, minimumSize, initialSize, maximumSize);
		if (!memorySubSpace->initialize(env)) {
			memorySubSpace->kill(env);
			memorySubSpace = NULL;
		}
	}
	return memorySubSpace;
}

bool
MM_MemorySubSpaceSegregated::initialize(MM_EnvironmentBase *env)
{
	if(!MM_MemorySubSpaceUniSpace::initialize(env)) {
		return false;
	}
	_memoryPoolSegregated->setSubSpace(this);

	return true;
}

void
MM_MemorySubSpaceSegregated::tearDown(MM_EnvironmentBase *env)
{
	if(NULL != _memoryPoolSegregated) {
		_memoryPoolSegregated->kill(env);
		_memoryPoolSegregated = NULL;
	}

	MM_MemorySubSpaceUniSpace::tearDown(env);
}

bool
MM_MemorySubSpaceSegregated::expanded(
	MM_EnvironmentBase *env,
	MM_PhysicalSubArena *subArena,
	MM_HeapRegionDescriptor *region,
	bool canCoalesce)
{
	void* regionLowAddress = region->getLowAddress();
	void* regionHighAddress = region->getHighAddress();

	/* Inform the sub space hierarchy of the size change */
	bool result = heapAddRange(env, this, region->getSize(), regionLowAddress, regionHighAddress);

	/* Expand the valid range for arraylets. */
#if defined(OMR_GC_ARRAYLETS)
	if (result) {
		_extensions->indexableObjectModel.expandArrayletSubSpaceRange(this, regionLowAddress, regionHighAddress, largestDesirableArraySpine());
	}
#endif /* defined(OMR_GC_ARRAYLETS) */

	return result;
}

bool
MM_MemorySubSpaceSegregated::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool result = MM_MemorySubSpaceUniSpace::heapAddRange(env, subspace, size, lowAddress, highAddress);
	if (result) {
		if (_regionExpansionBase != _regionExpansionTop) {
			if (_regionExpansionTop == lowAddress) {
				_regionExpansionTop = highAddress;
			} else {
				expandRegionPool();
			}
		} else {
			_regionExpansionBase = lowAddress;
			_regionExpansionTop = highAddress;
		}
	}
	return result;
}

bool
MM_MemorySubSpaceSegregated::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	Assert_MM_unreachable();
	return MM_MemorySubSpaceUniSpace::heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
}

void
MM_MemorySubSpaceSegregated::heapReconfigured(MM_EnvironmentBase *env)
{
	MM_MemorySubSpaceUniSpace::heapReconfigured(env);
	if (_regionExpansionBase != _regionExpansionTop) {
		expandRegionPool();
	}
}

uintptr_t
MM_MemorySubSpaceSegregated::getActualFreeMemorySize()
{
	return _memoryPoolSegregated->getActualFreeMemorySize();
}

uintptr_t
MM_MemorySubSpaceSegregated::getApproximateFreeMemorySize()
{
	return _memoryPoolSegregated->getApproximateFreeMemorySize();
}

uintptr_t
MM_MemorySubSpaceSegregated::getActiveMemorySize()
{
	return getActiveMemorySize(MEMORY_TYPE_OLD|MEMORY_TYPE_NEW);
}

uintptr_t
MM_MemorySubSpaceSegregated::getActiveMemorySize(uintptr_t includeMemoryType)
{
	if (includeMemoryType & getTypeFlags()) {
		return getCurrentSize();
	}
	return 0;
}

uintptr_t
MM_MemorySubSpaceSegregated::getActualActiveFreeMemorySize()
{
	return getActualActiveFreeMemorySize(MEMORY_TYPE_OLD|MEMORY_TYPE_NEW);
}

uintptr_t
MM_MemorySubSpaceSegregated::getActualActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	if (includeMemoryType & getTypeFlags()) {
		return _memoryPoolSegregated->getActualFreeMemorySize();
	}
	return 0;
}

uintptr_t
MM_MemorySubSpaceSegregated::getApproximateActiveFreeMemorySize()
{
	return getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD|MEMORY_TYPE_NEW);
}

uintptr_t
MM_MemorySubSpaceSegregated::getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	if (includeMemoryType & getTypeFlags()) {
		return _memoryPoolSegregated->getApproximateActiveFreeMemorySize();
	}
	return 0;
}

MM_MemoryPool *
MM_MemorySubSpaceSegregated::getMemoryPool()
{
	return _memoryPoolSegregated;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
