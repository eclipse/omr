/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
	MM_AllocationInterfaceGeneric *allocationInterface = (MM_AllocationInterfaceGeneric *)env->getForge()->allocate(sizeof(MM_AllocationInterfaceGeneric), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
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

/**
 * Flush the allocation cache.
 */
void
MM_AllocationInterfaceGeneric::flushCache(MM_EnvironmentBase *env)
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
