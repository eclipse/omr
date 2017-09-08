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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


#include "omrcfg.h"

#include "OMRVMInterface.hpp"

#include "Dispatcher.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "GlobalCollector.hpp"
#include "MemoryPool.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectHeapIterator.hpp"
#include "ObjectModel.hpp"
#include "OMRVMThreadInterface.hpp"
#include "SublistPool.hpp"
#include "SublistPuddle.hpp"
#include "SublistIterator.hpp"
#include "SublistSlotIterator.hpp"
#include "Task.hpp"

extern "C" {

/**
 * Actions required prior to walking the heap.
 */
void
hookWalkHeapStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_WalkHeapStartEvent* event = (MM_WalkHeapStartEvent*)eventData;
	GC_OMRVMInterface::flushCachesForWalk(event->omrVM);
}

/**
 * Actions required after to walking the heap.
 */
void
hookWalkHeapEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	/* Nothing to do right now */
}

} /* extern "C" */

/**
 * Initialize Hooks for OMRVMInterface.
 */
void
GC_OMRVMInterface::initializeExtensions(MM_GCExtensionsBase *extensions)
{
	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(extensions->privateHookInterface);

	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_WALK_HEAP_START, hookWalkHeapStart, OMR_GET_CALLSITE(), NULL);
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_WALK_HEAP_END, hookWalkHeapEnd, OMR_GET_CALLSITE(), NULL);
}

/**
 * Return the OMR hook interface for the Memory Manager
 */
J9HookInterface**
GC_OMRVMInterface::getOmrHookInterface(MM_GCExtensionsBase *extensions)
{
	return J9_HOOK_INTERFACE(extensions->omrHookInterface);
}

/**
 * Flush Cache for walk.
 */
void
GC_OMRVMInterface::flushCachesForWalk(OMR_VM* omrVM)
{
	/*
	 * Environment at this point might be fake, so we can not get this thread info
	 * however only one thread suppose to have an exclusive access at this point
	 */
	//Assert_MM_true(J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState);

	OMR_VMThread *omrVMThread;

	GC_OMRVMThreadListIterator threadListIterator(omrVM);

	while((omrVMThread = threadListIterator.nextOMRVMThread()) != NULL) {
		MM_EnvironmentBase *envToFlush = MM_EnvironmentBase::getEnvironment(omrVMThread);
		GC_OMRVMThreadInterface::flushCachesForWalk(envToFlush);
	}
}

/**
 * Flush Cache for GC.
 */
void
GC_OMRVMInterface::flushCachesForGC(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	OMR_VMThread *omrVMThread;
	UDATA allocatedBytesMax = extensions->bytesAllocatedMost;
	OMR_VMThread *vmThreadMax = extensions->vmThreadAllocatedMost;

	GC_OMRVMThreadListIterator threadListIterator(env->getOmrVM());

	while((omrVMThread = threadListIterator.nextOMRVMThread()) != NULL) {
		/* Grab allocation bytes stats per-thread before they're cleared */
		MM_EnvironmentBase* threadEnv = MM_EnvironmentBase::getEnvironment(omrVMThread);
		MM_AllocationStats * stats= threadEnv->_objectAllocationInterface->getAllocationStats();
		UDATA allocatedBytes = stats->bytesAllocated();

		if(allocatedBytes >= allocatedBytesMax){
			allocatedBytesMax = allocatedBytes;
			vmThreadMax = omrVMThread;
		}
		GC_OMRVMThreadInterface::flushCachesForGC(threadEnv);
	}

	extensions->bytesAllocatedMost = allocatedBytesMax;
	extensions->vmThreadAllocatedMost = vmThreadMax;
}

/**
 * Flush Non Allocation Caches (for instance, for Clearable phase in Metronome).
 */
void
GC_OMRVMInterface::flushNonAllocationCaches(MM_EnvironmentBase *env)
{
	OMR_VMThread *omrVMThread;

	GC_OMRVMThreadListIterator threadListIterator(env->getOmrVM());

	while((omrVMThread = threadListIterator.nextOMRVMThread()) != NULL) {
		GC_OMRVMThreadInterface::flushNonAllocationCaches(MM_EnvironmentBase::getEnvironment(omrVMThread));
	}
}
