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
#include "modronopt.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include <string.h>

#include "MemorySubSpaceGenerational.hpp"

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp" 
#include "MemorySubSpace.hpp"

/**
 * Zos390 Platform dependent trickery for the ar command.  Without this definition
 * the ar command on Zos390 will fail to link this class.
 */
#if defined(J9ZOS390)
#include "ZOSLinkage.hpp"
int j9zos390LinkTrickMemorySubSpaceGenerational; 
#endif /* J9ZOS390 */

/****************************************
 * Allocation
 ****************************************
 */

void *
MM_MemorySubSpaceGenerational::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	if (shouldCollectOnFailure) {
		/* Should never receive this call */
		return NULL;
	} else {
		if(previousSubSpace == _memorySubSpaceNew) {
			/* The allocate request is coming from new space - forward on to the old area */
			return _memorySubSpaceOld->allocateObject(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		}
	
		/* The allocate comes from the old area - failure */
		return NULL;
	}
}

void *
MM_MemorySubSpaceGenerational::allocationRequestFailed(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace)
{
	/* TODO: This code is nearly the same as Flat and Concurrent - all three should be merged into a common superclass */
	void *addr = NULL;

	if (previousSubSpace == _memorySubSpaceNew) {
		/* Handle a failure coming from new space - attempt the old area before doing any collection work */
		addr = _memorySubSpaceOld->allocationRequestFailed(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace, this);
		if(NULL != addr) {
			return addr;
		}
	}

	allocateDescription->saveObjects(env);
	if (!env->tryAcquireExclusiveVMAccessForGC(_collector)) {
		allocateDescription->restoreObjects(env);
		addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace);
		if(NULL != addr) {
			return addr;
		}

		if (!env->acquireExclusiveVMAccessForGC(_collector)) {
			allocateDescription->restoreObjects(env);
			addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace);
			if(NULL != addr) {
				/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
				 * an event for tracing / verbose to report the occurrence.
				 */
				reportAcquiredExclusiveToSatisfyAllocate(env, allocateDescription);
				return addr;
			}

			reportAllocationFailureStart(env, allocateDescription);
			performResize(env, allocateDescription);
			addr = allocateGeneric(env, allocateDescription, allocationType, objectAllocationInterface, baseSubSpace);

			if(NULL != addr) {
				/* Satisfied the allocate after having grabbed exclusive access to perform a GC (without actually performing the GC).  Raise
				 * an event for tracing / verbose to report the occurrence.
				 */
				reportAcquiredExclusiveToSatisfyAllocate(env, allocateDescription);
				reportAllocationFailureEnd(env);
				return addr;
			}
			allocateDescription->saveObjects(env);
		} else {
			reportAllocationFailureStart(env, allocateDescription);
		}
	} else {
		reportAllocationFailureStart(env, allocateDescription);
	}

	Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

	allocateDescription->setAllocationType(allocationType);
	addr = _collector->garbageCollect(env, this, allocateDescription, J9MMCONSTANT_IMPLICIT_GC_DEFAULT, objectAllocationInterface, baseSubSpace, NULL);
	allocateDescription->restoreObjects(env);

	if(NULL != addr) {
		reportAllocationFailureEnd(env);
		return addr;
	}

	/* A more aggressive collect here on failure */
	allocateDescription->saveObjects(env);
	addr = _collector->garbageCollect(env, this, allocateDescription, J9MMCONSTANT_IMPLICIT_GC_AGGRESSIVE, objectAllocationInterface, baseSubSpace, NULL);
	allocateDescription->restoreObjects(env);
	
	reportAllocationFailureEnd(env);
	return addr;
}

#if defined(OMR_GC_ARRAYLETS)
void *
MM_MemorySubSpaceGenerational::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	if (shouldCollectOnFailure) {
		/* Should never receive this call */
		return NULL;
	} else {
		if(previousSubSpace == _memorySubSpaceNew) {
			/* The allocate request is coming from new space - forward on to the old area */
			return _memorySubSpaceOld->allocateArrayletLeaf(env, allocDescription, baseSubSpace, this, shouldCollectOnFailure);
		}
	
		/* The allocate comes from the old area - failure */
		return NULL;
	}
}
#endif /* OMR_GC_ARRAYLETS */

void *
MM_MemorySubSpaceGenerational::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, MM_ObjectAllocationInterface *objectAllocationInterface, MM_MemorySubSpace *baseSubSpace, MM_MemorySubSpace *previousSubSpace, bool shouldCollectOnFailure)
{
	if (shouldCollectOnFailure) {
		/* Should never receive this call */
		Assert_MM_unreachable();
		return NULL;
	} else {
		if(previousSubSpace == _memorySubSpaceNew) {
			/* The allocate request is coming from new space - forward on to the old area */
			return _memorySubSpaceOld->allocateTLH(env, allocDescription, objectAllocationInterface, baseSubSpace, this, false);
		}
	
		/* The allocate comes from the old area - failure */
		return NULL;
	}
}

/****************************************
 * Internal Allocation
 ****************************************
 */
void
MM_MemorySubSpaceGenerational::abandonHeapChunk(void *addrBase, void *addrTop)
{
}

/****************************************
 * Sub Space Categorization
 ****************************************
 */
MM_MemorySubSpace *
MM_MemorySubSpaceGenerational::getDefaultMemorySubSpace()
{
	return _memorySubSpaceNew->getDefaultMemorySubSpace();
}

MM_MemorySubSpace *
MM_MemorySubSpaceGenerational::getTenureMemorySubSpace()
{
	return getMemorySubSpaceOld()->getDefaultMemorySubSpace();
}

/**
 * Initialization
 */
MM_MemorySubSpaceGenerational *
MM_MemorySubSpaceGenerational::newInstance(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpaceNew, MM_MemorySubSpace *memorySubSpaceOld, bool usesGlobalCollector, uintptr_t minimumSize, uintptr_t minimumSizeNew, uintptr_t initialSizeNew, uintptr_t maximumSizeNew, uintptr_t minimumSizeOld, uintptr_t initialSizeOld, uintptr_t maximumSizeOld, uintptr_t maximumSize)
{
	MM_MemorySubSpaceGenerational *memorySubSpace;
	
	memorySubSpace = (MM_MemorySubSpaceGenerational *)env->getForge()->allocate(sizeof(MM_MemorySubSpaceGenerational), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memorySubSpace) {
		new(memorySubSpace) MM_MemorySubSpaceGenerational(env, memorySubSpaceNew, memorySubSpaceOld, usesGlobalCollector, minimumSize, minimumSizeNew, initialSizeNew, maximumSizeNew, minimumSizeOld, initialSizeOld, maximumSizeOld, maximumSize);
		if (!memorySubSpace->initialize(env)) {
			memorySubSpace->kill(env);
			memorySubSpace = NULL;
		}
	}
	return memorySubSpace;
}

bool
MM_MemorySubSpaceGenerational::initialize(MM_EnvironmentBase *env)
{
	if(!MM_MemorySubSpace::initialize(env)) {
		return false;
	}

	/* attach the children */
	registerMemorySubSpace(_memorySubSpaceOld);
	registerMemorySubSpace(_memorySubSpaceNew);
	
	return true;
}

void
MM_MemorySubSpaceGenerational::tearDown(MM_EnvironmentBase *env)
{
	MM_MemorySubSpace::tearDown(env);
}

/**
 * Perform the resize. For the generational, it applies only for the tenured subspace.
 * @return The actual amount resized.
 */
intptr_t
MM_MemorySubSpaceGenerational::performResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	return getMemorySubSpaceOld()->performResize(env, allocDescription);
}

/**
 * Calculate the resize required (if any). For the generational, it applies only for the tenured subspace.
 * @return The actual amount resized.
 */

void
MM_MemorySubSpaceGenerational::checkResize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool _systemGC)
{
	getMemorySubSpaceOld()->checkResize(env, allocDescription, _systemGC);
}

/**
 * Get the size of heap available for contraction.
 * @return Size of heap available for contraction.
 */
uintptr_t 
MM_MemorySubSpaceGenerational::getAvailableContractionSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	return getMemorySubSpaceOld()->getAvailableContractionSize(env, allocDescription);
}

/**
 * Counter balance a contract.
 * React to a pending contract of the the given subspace by possibly adjusted (expanding) other subspaces to fill minimum
 * quotas, etc.
 * The generational receiver determines which direction to push the request so that the opposing sibbling has a chance to
 * expand.
 * @return the adjusted contract size that is allowed to the receiver.
 */
uintptr_t
MM_MemorySubSpaceGenerational::counterBalanceContract(
	MM_EnvironmentBase *env,
	MM_MemorySubSpace *previousSubSpace,
	MM_MemorySubSpace *contractSubSpace,
	uintptr_t contractSize,
	uintptr_t contractAlignment)
{
	uintptr_t expandSize;
	/* Determine if a counter balancing expand is required */
	assume0(contractSize <= _currentSize);
	if((_currentSize - contractSize) >= _minimumSize) {
		return contractSize;
	}
	expandSize = _minimumSize - (_currentSize - contractSize);
	assume0(expandSize == MM_Math::roundToFloor(MM_GCExtensions::getExtensions(env)->heapAlignment, expandSize)); /* contract delta should be the same alignment as expand delta */
	
	/* Find the space that needs to expand, and do it */
	if(previousSubSpace == _memorySubSpaceNew) {
		return _memorySubSpaceOld->counterBalanceContractWithExpand(env, this, contractSubSpace, contractSize, contractAlignment, expandSize);
	}
	
	return _memorySubSpaceNew->counterBalanceContractWithExpand(env, this, contractSubSpace, contractSize, contractAlignment, expandSize);
}

#endif /* OMR_GC_MODRON_SCAVENGER */
