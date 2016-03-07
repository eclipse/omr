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

#include "AllocateDescription.hpp"
#include "AllocationInterfaceGeneric.hpp"
#include "GCExtensionsBase.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "ModronAssertions.h"

/**
 * Create and return a new instance of MM_SegregatedAllocationInterface.
 *
 * @return the new instance, or NULL on failure.
 */
MM_AllocationInterfaceGeneric*
MM_AllocationInterfaceGeneric::newInstance(MM_EnvironmentBase *env)
{
	MM_AllocationInterfaceGeneric *allocationInterface = (MM_AllocationInterfaceGeneric *)env->getForge()->allocate(sizeof(MM_AllocationInterfaceGeneric), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(NULL != allocationInterface) {
		new(allocationInterface) MM_AllocationInterfaceGeneric(env);
		if(!allocationInterface->initialize(env)) {
			allocationInterface->kill(env);
			return NULL;
		}
	}
	return allocationInterface;
}

/**
 * Clean up all state and resources held by the receiver.
 */
void
MM_AllocationInterfaceGeneric::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * One time initialization of the receivers state.
 * @return true on successful initialization, false otherwise.
 */
bool
MM_AllocationInterfaceGeneric::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Internal clean up of the receivers state and resources.
 */
void
MM_AllocationInterfaceGeneric::tearDown(MM_EnvironmentBase *env)
{

}

void*
MM_AllocationInterfaceGeneric::allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	allocateDescription->setMemorySpace(memorySpace);

	return memorySpace->getDefaultMemorySubSpace()->allocateObject(env, allocateDescription, NULL, NULL, shouldCollectOnFailure);

}

void*
MM_AllocationInterfaceGeneric::allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	return allocateObject(env, allocateDescription, memorySpace, shouldCollectOnFailure);
}

#if defined(OMR_GC_ARRAYLETS)
/**
 * Allocate the arraylet spine.
 */
void *
MM_AllocationInterfaceGeneric::allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Allocate an arraylet leaf.
 */
void *
MM_AllocationInterfaceGeneric::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure)
{
	Assert_MM_unreachable();
	return NULL;
}
#endif /* OMR_GC_ARRAYLETS */

/**
 * Flush the allocation cache.
 */
void
MM_AllocationInterfaceGeneric::flushCache(MM_EnvironmentBase *env)
{
}

/**
 * Reconnect the allocation cache.
 */
void
MM_AllocationInterfaceGeneric::reconnectCache(MM_EnvironmentBase *env)
{
}

void
MM_AllocationInterfaceGeneric::enableCachedAllocations(MM_EnvironmentBase* env)
{
	if (!_cachedAllocationsEnabled) {
		_cachedAllocationsEnabled = true;
	}
}

void
MM_AllocationInterfaceGeneric::disableCachedAllocations(MM_EnvironmentBase* env)
{
	if (_cachedAllocationsEnabled) {
		_cachedAllocationsEnabled = false;
		flushCache(env);
	}
}
