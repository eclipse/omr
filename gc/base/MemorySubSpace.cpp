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


#include "MemorySubSpace.hpp"

#include <string.h>

#include "omrlinkedlist.h"
#include "j9nongenerated.h"
#include "omrport.h"

#include "AllocateDescription.hpp"
#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCCode.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalCollector.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapResizeStats.hpp"
#include "Math.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "ModronAssertions.h"
#include "PercolateStats.hpp"
#include "PhysicalSubArena.hpp"

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.
 */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"

class MM_EnvironmentBase;

extern "C" {

void
memorySubSpaceAsyncCallbackHandler(OMR_VMThread *omrVMThread)
{
	/* TODO: MultiMemorySpace - This is really once for every collector in the system */
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	if(!env->isThreadScanned()) {
		MM_MemorySpace *defaultMemorySpace =  env->getExtensions()->heap->getDefaultMemorySpace();
		MM_MemorySubSpace *memorySubSpaceList = defaultMemorySpace->getMemorySubSpaceList();
		while(memorySubSpaceList) {
			memorySubSpaceList->getCollector()->scanThread(env);
			memorySubSpaceList = memorySubSpaceList->getNext();
		}
	}
}

} /* extern "C" */

void
MM_MemorySubSpace::reportSystemGCStart(MM_EnvironmentBase* env, uint32_t gcCode)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_OMRMM_SystemGCStart(env->getOmrVMThread(),
						 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
						 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
						 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
						 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
						 (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
						 (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));
	Trc_MM_SystemGCStart(env->getLanguageVMThread(),
							 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
							 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
							 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
							 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
							 (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
							 (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));

	uint64_t exclusiveAccessTimeMicros = omrtime_hires_delta(0, env->getExclusiveAccessTime(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	uint64_t meanExclusiveAccessIdleTimeMicros = omrtime_hires_delta(0, env->getMeanExclusiveAccessIdleTime(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	Trc_MM_ExclusiveAccess(env->getLanguageVMThread(),
						   (uint32_t)(exclusiveAccessTimeMicros / 1000),
						   (uint32_t)(exclusiveAccessTimeMicros % 1000),
						   (uint32_t)(meanExclusiveAccessIdleTimeMicros / 1000),
						   (uint32_t)(meanExclusiveAccessIdleTimeMicros % 1000),
						   env->getExclusiveAccessHaltedThreads(),
						   env->getLastExclusiveAccessResponder(),
						   env->exclusiveAccessBeatenByOtherThread());

	if (J9_EVENT_IS_HOOKED(_extensions->privateHookInterface, J9HOOK_MM_PRIVATE_SYSTEM_GC_START)) {
		MM_CommonGCStartData commonData;
		_extensions->heap->initializeCommonGCStartData(env, &commonData);

		ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_SYSTEM_GC_START(
			_extensions->privateHookInterface,
			env->getOmrVMThread(),
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_SYSTEM_GC_START,
			gcCode,
			&commonData);
	}
}

void
MM_MemorySubSpace::reportSystemGCEnd(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_OMRMM_SystemGCEnd(env->getOmrVMThread(),
					   _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
					   _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
					   _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
					   _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
					   (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
					   (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));
	Trc_MM_SystemGCEnd(env->getLanguageVMThread(),
						   _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
						   _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
						   _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
						   _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
						   (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
						   (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));

	if (J9_EVENT_IS_HOOKED(_extensions->privateHookInterface, J9HOOK_MM_PRIVATE_SYSTEM_GC_END)) {
		MM_CommonGCEndData commonData;
		_extensions->heap->initializeCommonGCEndData(env, &commonData);

		ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_SYSTEM_GC_END(
			_extensions->privateHookInterface,
			env->getOmrVMThread(),
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_SYSTEM_GC_END,
			env->getExclusiveAccessTime(),
			&commonData);
	}
}

/**
 * Report the start of a heap expansion event through hooks.
 */
void
MM_MemorySubSpace::reportHeapResizeAttempt(MM_EnvironmentBase* env, uintptr_t amount, uintptr_t type)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	MM_HeapResizeStats *resizeStats = _extensions->heap->getResizeStats();

	uint64_t resizeTime = (type == HEAP_EXPAND)
						  ? resizeStats->getLastExpandTime()
						  : resizeStats->getLastContractTime();

	uint32_t gcTimeRatio = 0;

	if (HEAP_EXPAND == type) {
		gcTimeRatio = resizeStats->getRatioExpandPercentage();
	} else if (HEAP_CONTRACT == type) {
		gcTimeRatio = resizeStats->getRatioContractPercentage();
	}

	uintptr_t reason = 0;

	if (HEAP_EXPAND == type) {
		reason = (uintptr_t)resizeStats->getLastExpandReason();
	} else if (HEAP_CONTRACT == type) {
		reason = (uintptr_t)resizeStats->getLastContractReason();
	} else if (HEAP_LOA_EXPAND == type) {
		reason = (uintptr_t)resizeStats->getLastLoaResizeReason();
		Assert_MM_true(reason <= LOA_EXPAND_LAST_RESIZE_REASON);
	} else if (HEAP_LOA_CONTRACT == type) {
		reason = (uintptr_t)resizeStats->getLastLoaResizeReason();
		Assert_MM_true(reason > LOA_EXPAND_LAST_RESIZE_REASON);
	}

	TRIGGER_J9HOOK_MM_PRIVATE_HEAP_RESIZE(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_HEAP_RESIZE,
		type,
		getTypeFlags(),
		gcTimeRatio,
		amount,
		getActiveMemorySize(),
		omrtime_hires_delta(0, resizeTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS),
		reason);
}

void
MM_MemorySubSpace::reportPercolateCollect(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	TRIGGER_J9HOOK_MM_PRIVATE_PERCOLATE_COLLECT(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_PERCOLATE_COLLECT,
		(uintptr_t)_extensions->heap->getPercolateStats()->getLastPercolateReason());
}

/**
 * Return the number of memory pools associated to the receiver.
 * @note In the general case, 0 is returned.
 * @return memory pool count
 */
uintptr_t
MM_MemorySubSpace::getMemoryPoolCount()
{
	Assert_MM_unreachable();
	return 0;
}

/**
 * Event reporting
 */
void
MM_MemorySubSpace::reportAllocationFailureStart(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	generateAllocationFailureStats(env, allocDescription);

	env->allocationFailureStartReportIfRequired(allocDescription, getTypeFlags());

	Trc_MM_AllocationFailureCycleStart(env->getLanguageVMThread(),
									   _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
									   _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
									   _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
									   _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
									   (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
									   (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0),
									   allocDescription->getBytesRequested());

	Trc_OMRMM_AllocationFailureCycleStart(env->getOmrVMThread(),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
		(_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
		(_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0),
		allocDescription->getBytesRequested());

	uint64_t exclusiveAccessTimeMicros = omrtime_hires_delta(0, env->getExclusiveAccessTime(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	uint64_t meanExclusiveAccessIdleTimeMicros = omrtime_hires_delta(0, env->getMeanExclusiveAccessIdleTime(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	Trc_MM_ExclusiveAccess(env->getLanguageVMThread(),
						   (uint32_t)(exclusiveAccessTimeMicros / 1000),
						   (uint32_t)(exclusiveAccessTimeMicros % 1000),
						   (uint32_t)(meanExclusiveAccessIdleTimeMicros / 1000),
						   (uint32_t)(meanExclusiveAccessIdleTimeMicros % 1000),
						   env->getExclusiveAccessHaltedThreads(),
						   env->getLastExclusiveAccessResponder(),
						   env->exclusiveAccessBeatenByOtherThread());

	if (J9_EVENT_IS_HOOKED(_extensions->privateHookInterface, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_START)) {
		MM_CommonGCStartData commonData;
		_extensions->heap->initializeCommonGCStartData(env, &commonData);

		ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_START(
			_extensions->privateHookInterface,
			env->getOmrVMThread(),
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_START,
			allocDescription->getBytesRequested(),
			&commonData,
			getTypeFlags());
	}
}

void
MM_MemorySubSpace::reportAllocationFailureEnd(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_AllocationFailureCycleEnd(env->getLanguageVMThread(),
									 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
									 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
									 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
									 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
									 (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
									 (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));

	Trc_OMRMM_AllocationFailureCycleEnd(env->getOmrVMThread(),
									 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
									 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
									 _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
									 _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
									 (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
									 (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));

	if (J9_EVENT_IS_HOOKED(_extensions->privateHookInterface, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_END)) {
		MM_CommonGCEndData commonData;
		_extensions->heap->initializeCommonGCEndData(env, &commonData);

		ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_END(
			_extensions->privateHookInterface,
			env->getOmrVMThread(),
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_END,
			env->getExclusiveAccessTime(),
			getTypeFlags(),
			&commonData);
	}
}

void
MM_MemorySubSpace::reportAcquiredExclusiveToSatisfyAllocate(MM_EnvironmentBase* env, MM_AllocateDescription* allocateDescription)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_AcquiredExclusiveToSatisfyAllocation(env->getLanguageVMThread(), allocateDescription->getBytesRequested(), getTypeFlags());

	TRIGGER_J9HOOK_MM_PRIVATE_ACQUIRED_EXCLUSIVE_TO_SATISFY_ALLOCATION(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_START,
		allocateDescription->getBytesRequested(),
		getTypeFlags());
}

void
MM_MemorySubSpace::registerMemorySubSpace(MM_MemorySubSpace* memorySubSpace)
{
	memorySubSpace->setParent(this);

	if (_children) {
		_children->setPrevious(memorySubSpace);
	}
	memorySubSpace->setNext(_children);
	memorySubSpace->setPrevious(NULL);
	_children = memorySubSpace;
}

void
MM_MemorySubSpace::unregisterMemorySubSpace(MM_MemorySubSpace* memorySubSpace)
{
	MM_MemorySubSpace* previous, *next;
	previous = memorySubSpace->getPrevious();
	next = memorySubSpace->getNext();

	if (previous) {
		previous->setNext(next);
	} else {
		_children = next;
	}

	if (next) {
		next->setPrevious(previous);
	}
}

/**
 * Set the owning memory space and cascade down the hierarchy.
 */
void
MM_MemorySubSpace::setMemorySpace(MM_MemorySpace* memorySpace)
{
	if (NULL != _physicalSubArena) {
		_physicalSubArena->setParent(memorySpace->getPhysicalArena());
	}

	if (NULL != _children) {
		_children->setMemorySpace(memorySpace);
	}

	if (NULL != _next) {
		_next->setMemorySpace(memorySpace);
	}

	_memorySpace = memorySpace;
}

MM_MemorySubSpace *
MM_MemorySubSpace::getTopLevelMemorySubSpace(uintptr_t typeFlags)
{
	Assert_MM_true(typeFlags == (getTypeFlags() & typeFlags));
	MM_MemorySubSpace *topLevelSubSpace = this;

	while (topLevelSubSpace->getParent() && (typeFlags == (topLevelSubSpace->getParent()->getTypeFlags() & typeFlags))) {
		topLevelSubSpace = topLevelSubSpace->getParent();
	}

	return topLevelSubSpace;
}

/**
 * If the subSpace is active return true, else return false.
 * @note All subSpaces, with the exception of the semi-space, are active by default.
 */
bool
MM_MemorySubSpace::isActive()
{
	return true;
}

/**
 * If the child subSpace is active return true, else return false. 
 * @note All subSpaces, with the exception of the semi-space, are active by default.
 */
bool
MM_MemorySubSpace::isChildActive(MM_MemorySubSpace* memorySubSpace)
{
	return true;
}

/**
 * Initialize a memorysubspace
 */
bool
MM_MemorySubSpace::initialize(MM_EnvironmentBase* env)
{
	if (!_lock.initialize(env, &env->getExtensions()->lnrlOptions, "MM_MemorySubSpace:_lock")) {
		return false;
	}

	/* Attach to the correct parent */
	/* TODO: this code to go */
	if (_parent) {
		_parent->registerMemorySubSpace(this);
	} else if (_memorySpace) {
		_memorySpace->registerMemorySubSpace(this);
	}

	/* If the receiver uses the global collector, find and associate to it */
	if (_usesGlobalCollector) {
		_collector = _extensions->getGlobalCollector();
	}

	if (NULL != _physicalSubArena) {
		_physicalSubArena->setSubSpace(this);
	}

	return true;
}

/**
 * Free all internal structures and resource of the receiver, and then free the receiver itself.
 */
void
MM_MemorySubSpace::kill(MM_EnvironmentBase* env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_MemorySubSpace::tearDown(MM_EnvironmentBase* env)
{
	MM_MemorySubSpace* child;

	/* Kill the subarena */
	if (NULL != _physicalSubArena) {
		_physicalSubArena->kill(env);
		_physicalSubArena = NULL;
	}

	/* Kill the collector (if applicable) */
	if ((NULL != _collector) && !_usesGlobalCollector) {
		_collector->kill(env);
		_collector = NULL;
	}

	/* Kill all children */
	child = _children;
	while (NULL != child) {
		MM_MemorySubSpace* next;
		next = child->getNext();
		child->kill(env);
		child = next;
	}
	_children = NULL;

	/* Remove the receiver from its owners list */
	if (NULL != _parent) {
		_parent->unregisterMemorySubSpace(this);
	} else if (NULL != _memorySpace) {
		_memorySpace->unregisterMemorySubSpace(this);
	}

	_lock.tearDown();
}

bool
MM_MemorySubSpace::inflate(MM_EnvironmentBase* env)
{
	bool result;
	MM_MemorySubSpace* child;

	if (_physicalSubArena && !_physicalSubArena->inflate(env)) {
		return false;
	}

	result = true;
	child = _children;
	while (result && child) {
		result = child->inflate(env);
		child = child->getNext();
	}

	return result;
}

/**
 * Return the number of active memory pools associated to the receiver.
 * @note In the general case, 0 is returned.
 * @return memory pool count
 */
uintptr_t
MM_MemorySubSpace::getActiveMemoryPoolCount()
{
	Assert_MM_unreachable();
	return 0;
}

/**
 * Return the memory pool associated to the receiver.
 * @note In the general case, NULL is returned.
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemorySubSpace::getMemoryPool()
{
	return NULL;
}

/**
 * Return the memory pool associated with a given storage location.
 * @param Address of stoarge location 
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemorySubSpace::getMemoryPool(void* addr)
{
	return getMemoryPool();
}

/**
 * Return the memory pool associated with a given allocation size
 * @param Size of allocation request  
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemorySubSpace::getMemoryPool(uintptr_t size)
{
	return getMemoryPool();
}

/**
 * Return the memory pool associated with a specified range of storage locations. 
 * 
 * @param addrBase Low address in specified range  
 * @param addrTop High address in  specified range 
 * @param highAddr If range spans end of memory pool set to address of first byte
 * which does not belong in returned pool.
 * @return MM_MemoryPool for storage location addrBase
 */
MM_MemoryPool*
MM_MemorySubSpace::getMemoryPool(MM_EnvironmentBase* env, void* addrBase, void* addrTop, void*& highAddr)
{
	highAddr = NULL;
	return getMemoryPool();
}


MM_LargeObjectAllocateStats*
MM_MemorySubSpace::getLargeObjectAllocateStats()
{
	if (NULL == _children) {
		return getMemoryPool()->getLargeObjectAllocateStats();
	}

	return NULL;
}

/**
 * Generate the allocationfailurestats for the given allocDescription
 * @param allocDescription the allocDescription to store the stats of
 */
void
MM_MemorySubSpace::generateAllocationFailureStats(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	_allocationFailureStats.subSpaceType = getTypeFlags();
	_allocationFailureStats.allocationFailureSize = allocDescription->getBytesRequested();
	_allocationFailureStats.allocationFailureCount += 1;
}

/**
 * Get the sum of all free memory currently available for allocation in the subspace and its children.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider defered work that may be done to increase current free memory stores.
 * @see getApproximateFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_MemorySubSpace::getActualFreeMemorySize()
{
	uintptr_t freeMemory;

	freeMemory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		freeMemory += child->getActualFreeMemorySize();
		child = child->getNext();
	}

	return freeMemory;
}

/**
 * Get the approximate sum of all free memory available for allocation in the subspace and its children.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * @see getActualFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_MemorySubSpace::getApproximateFreeMemorySize()
{
	uintptr_t freeMemory;

	freeMemory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		freeMemory += child->getApproximateFreeMemorySize();
		child = child->getNext();
	}

	return freeMemory;
}

uintptr_t
MM_MemorySubSpace::getActiveMemorySize()
{
	return getActiveMemorySize(MEMORY_TYPE_OLD | MEMORY_TYPE_NEW);
}

uintptr_t
MM_MemorySubSpace::getActiveMemorySize(uintptr_t includeMemoryType)
{
	uintptr_t memory;

	memory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		memory += child->getActiveMemorySize(includeMemoryType);
		child = child->getNext();
	}

	return memory;
}

uintptr_t
MM_MemorySubSpace::getActiveLOAMemorySize(uintptr_t includeMemoryType)
{
	uintptr_t memory;

	memory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		memory += child->getActiveLOAMemorySize(includeMemoryType);
		child = child->getNext();
	}

	return memory;
}

uintptr_t
MM_MemorySubSpace::getActiveSurvivorMemorySize(uintptr_t includeMemoryType)
{
	uintptr_t memory;

	memory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		memory += child->getActiveSurvivorMemorySize(includeMemoryType);
		child = child->getNext();
	}

	return memory;
}

/**
 * Get the sum of all free memory currently available for allocation in the subspace and its children.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider defered work that may be done to increase current free memory stores.
 * @see getApproximateActiveFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_MemorySubSpace::getActualActiveFreeMemorySize()
{
	return getActualActiveFreeMemorySize(MEMORY_TYPE_OLD | MEMORY_TYPE_NEW);
}

/**
 * Get the sum of all free memory currently available for allocation in the subspace and its children of the specified type.
 * This call will return an accurate count of the current size of all free memory of the specified type.  It will not
 * consider defered work that may be done to increase current free memory stores.
 *
 * @see getApproximateActiveFreeMemorySize(uintptr_t)
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_MemorySubSpace::getActualActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	uintptr_t freeMemory;

	freeMemory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		freeMemory += child->getActualActiveFreeMemorySize(includeMemoryType);
		child = child->getNext();
	}

	return freeMemory;
}

/**
 * Get the sum of all free LOA memory currently available for allocation in the subspace and its children.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 *
 * @return the total free LOA memory currently available for allocation
 */
uintptr_t
MM_MemorySubSpace::getApproximateActiveFreeLOAMemorySize()
{
	return getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD);
}

uintptr_t
MM_MemorySubSpace::getApproximateActiveFreeSurvivorMemorySize()
{
	return getApproximateActiveFreeSurvivorMemorySize(MEMORY_TYPE_NEW);
}


/**
 * Get the sum of all free LOA memory currently available for allocation in the subspace and its children of the specified type.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 *
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_MemorySubSpace::getApproximateActiveFreeLOAMemorySize(uintptr_t includeMemoryType)
{
	uintptr_t freeMemory;

	freeMemory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		freeMemory += child->getApproximateActiveFreeLOAMemorySize(includeMemoryType);
		child = child->getNext();
	}

	return freeMemory;
}

uintptr_t
MM_MemorySubSpace::getApproximateActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType)
{
	uintptr_t freeMemory;

	freeMemory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		freeMemory += child->getApproximateActiveFreeSurvivorMemorySize(includeMemoryType);
		child = child->getNext();
	}

	return freeMemory;
}

/**
 * Get the approximate sum of all free memory available for allocation in the subspace and its children.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * 
 * @see getActualActiveFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_MemorySubSpace::getApproximateActiveFreeMemorySize()
{
	return getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD | MEMORY_TYPE_NEW);
}

/**
 * Get the approximate sum of all free memory available for allocation in subspace and its children of the specified type.
 * This call will return an estimated count of the current size of all free memory of the specified type.  Although this
 * estimate may be accurate, it will consider defered work that may be done to increase current free memory stores.
 *
 * @see getActualActiveFreeMemorySize(uintptr_t)
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_MemorySubSpace::getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType)
{
	uintptr_t freeMemory;

	freeMemory = 0;

	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		freeMemory += child->getApproximateActiveFreeMemorySize(includeMemoryType);
		child = child->getNext();
	}

	return freeMemory;
}

/**
 * Ask all memory spaces/pools if a complete rebuild of the freelist  is requried
 */

bool
MM_MemorySubSpace::completeFreelistRebuildRequired(MM_EnvironmentBase* env)
{
	MM_MemorySubSpace* child = _children;
	bool rebuildRequired = false;

	while (!rebuildRequired && child) {
		rebuildRequired = child->completeFreelistRebuildRequired(env);
		child = child->getNext();
	}

	return rebuildRequired;
}

void
MM_MemorySubSpace::mergeHeapStats(MM_HeapStats* heapStats)
{
	mergeHeapStats(heapStats, (MEMORY_TYPE_OLD | MEMORY_TYPE_NEW));
}

void
MM_MemorySubSpace::mergeHeapStats(MM_HeapStats* heapStats, uintptr_t includeMemoryType)
{
	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;

	while (child) {
		child->mergeHeapStats(heapStats, includeMemoryType);
		child = child->getNext();
	}
}

void
MM_MemorySubSpace::resetHeapStatistics(bool globalCollect)
{
	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		child->resetHeapStatistics(globalCollect);
		child = child->getNext();
	}
}

/**
 * Return the allocation failure stats for this subSpace.
 */
MM_AllocationFailureStats*
MM_MemorySubSpace::getAllocationFailureStats()
{
	return &_allocationFailureStats;
}

void
MM_MemorySubSpace::systemGarbageCollect(MM_EnvironmentBase* env, uint32_t gcCode)
{
	if (_parent) {
		_parent->systemGarbageCollect(env, gcCode);
		return;
	}

	/* do not launch system gc in -Xgcpolicy:nogc */
	if (_collector && _usesGlobalCollector && !_collector->isDisabled(env)) {
		/* TODO: This is bogus for multiple memory spaces - should ask the space, not the heap */
		_extensions->heap->getResizeStats()->setFreeBytesAtSystemGCStart(getApproximateActiveFreeMemorySize());

		env->acquireExclusiveVMAccessForGC(_collector);
		reportSystemGCStart(env, gcCode);

		/* system GCs are accounted into "user" time in GC/total time ratio calculation */
		_collector->garbageCollect(env, this, NULL, gcCode, NULL, NULL, NULL);

		reportSystemGCEnd(env);
		env->releaseExclusiveVMAccessForGC();

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
		if ((J9MMCONSTANT_EXPLICIT_GC_IDLE_GC == gcCode) && (_extensions->gcOnIdle)) {
			OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
			uint64_t startTime = omrtime_hires_clock();
			uintptr_t releasedBytes = _extensions->heap->getDefaultMemorySpace()->releaseFreeMemoryPages(env);
			uint64_t endTime = omrtime_hires_clock();
			TRIGGER_J9HOOK_MM_PRIVATE_HEAP_RESIZE(
				_extensions->privateHookInterface,
				env->getOmrVMThread(),
				omrtime_hires_clock(),
				J9HOOK_MM_PRIVATE_HEAP_RESIZE,
				HEAP_RELEASE_FREE_PAGES,
				getTypeFlags(),
				/* GC Time Ratio not applicable for "release free heap pages" */
				0, 
				releasedBytes,
				getActiveMemorySize(),
				omrtime_hires_delta(startTime, endTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS),
				/* reason enum variable not applicable/used, so passing univeral value 1 = not found*/
				1 
				);
		}
#endif
	}
}

bool
MM_MemorySubSpace::percolateGarbageCollect(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, uint32_t gcCode)
{
	Trc_MM_MemorySubSpace_percolateGarbageCollect_Entry(env->getLanguageVMThread());

	if (_parent) {
		bool result = _parent->garbageCollect(env, allocDescription, gcCode);
		Trc_MM_MemorySubSpace_percolateGarbageCollect_Exit1(env->getLanguageVMThread(), result ? "true" : "false");
		Trc_OMRMM_MemorySubSpace_percolateGarbageCollect_Exit1(env->getOmrVMThread(), result ? "true" : "false");
		return result;
	}

	Trc_MM_MemorySubSpace_percolateGarbageCollect_Exit2(env->getLanguageVMThread());
	Trc_OMRMM_MemorySubSpace_percolateGarbageCollect_Exit2(env->getOmrVMThread());
	return false; /* No gc done */
}

bool
MM_MemorySubSpace::garbageCollect(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, uint32_t gcCode)
{
	Trc_MM_MemorySubSpace_garbageCollect_Entry(env->getLanguageVMThread());

	/* If MMS has a collector call it.. */
	if (_collector) {
		if (_collector->forceKickoff(env, this, allocDescription, gcCode)) {
			Trc_MM_MemorySubSpace_garbageCollect_Exit4(env->getLanguageVMThread());
			return false; /* forced kickoff activated */
		}

		if (MM_GCCode(gcCode).isPercolateGC()) {
			reportPercolateCollect(env);
		}
		if (NULL != allocDescription) {
			/* TODO temporary fix for gencon.  maybe this should be moved up to the caller since this
			 * only seems to be called by scavenger percolate.
			 */
			allocDescription->setAllocationType(ALLOCATION_TYPE_INVALID);
		}
		_collector->garbageCollect(env, this, allocDescription, gcCode, NULL, NULL, NULL);
		Trc_MM_MemorySubSpace_garbageCollect_Exit1(env->getLanguageVMThread());
		return true;
	}

	/* ..otherwise percolate to parent if any */
	if (_parent) {
		bool result = _parent->garbageCollect(env, allocDescription, gcCode);
		Trc_MM_MemorySubSpace_garbageCollect_Exit2(env->getLanguageVMThread(), result ? "true" : "false");
		return result;
	}

	Trc_MM_MemorySubSpace_garbageCollect_Exit3(env->getLanguageVMThread());
	return false; /* No gc done */
}

void
MM_MemorySubSpace::reset()
{
	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		child->reset();
		child = child->getNext();
	}
}

/**
 * As opposed to reset, which will empty out, this will fill out as if everything is free
 */
void
MM_MemorySubSpace::rebuildFreeList(MM_EnvironmentBase* env)
{
	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;
	while (child) {
		child->rebuildFreeList(env);
		child = child->getNext();
	}
}

void*
MM_MemorySubSpace::allocateGeneric(MM_EnvironmentBase* env, MM_AllocateDescription* allocateDescription, AllocationType allocationType, MM_ObjectAllocationInterface* objectAllocationInterface, MM_MemorySubSpace* attemptSubspace)
{
	void* result = NULL;

	switch (allocationType) {
	case ALLOCATION_TYPE_OBJECT:
		result = attemptSubspace->allocateObject(env, allocateDescription, this, this, false);
		break;
	case ALLOCATION_TYPE_LEAF:
		result = attemptSubspace->allocateArrayletLeaf(env, allocateDescription, this, this, false);
		break;
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	case ALLOCATION_TYPE_TLH:
		result = attemptSubspace->allocateTLH(env, allocateDescription, objectAllocationInterface, this, this, false);
		break;
#endif /* defined(OMR_GC_THREAD_LOCAL_HEAP) */
	default:
		Assert_MM_unreachable();
	}
	return result;
}

#if defined(OMR_GC_ALLOCATION_TAX)
/**
 * Pay the allocation tax for the mutator.
 * @note Public entry point for paying an allocation tax.
 */
void
MM_MemorySubSpace::payAllocationTax(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	payAllocationTax(env, this, allocDescription);
}

/**
 * Pay the allocation tax for the mutator.
 */
void
MM_MemorySubSpace::payAllocationTax(MM_EnvironmentBase* env, MM_MemorySubSpace* baseSubSpace, MM_AllocateDescription* allocDescription)
{
	if (_extensions->payAllocationTax) {
		if (_parent) {
			_parent->payAllocationTax(env, baseSubSpace, allocDescription);
		} else {
			Assert_MM_true(_usesGlobalCollector); /* If you don't have a parent, you really should be using the global */
			if (_usesGlobalCollector) {
				_collector->payAllocationTax(env, this, baseSubSpace, allocDescription);
			}
		}
	}
}
#endif /* OMR_GC_ALLOCATION_TAX */

/**
 * Set whether resizing is available on the receivers PSA.
 * @return the previous value of the resizable flag.
 */
bool
MM_MemorySubSpace::setResizable(bool resizable)
{
	if (NULL != _physicalSubArena) {
		return _physicalSubArena->setResizable(resizable);
	}

	/* No PSA - what do you do? */
	return false;
}

/**
 * Determine if the receiver will allow the expansion request size.
 * @return true if the expand size fits into the receivers limits, false otherwise.
 */
bool
MM_MemorySubSpace::canExpand(MM_EnvironmentBase* env, uintptr_t expandSize)
{
	if ((expandSize <= _maximumSize) && (_currentSize <= (_maximumSize - expandSize))) {
		if (_parent) {
			return _parent->canExpand(env, expandSize);
		}
		return _memorySpace->canExpand(env, expandSize);
	}
	return false;
}

/**
 * Adjust the specified expansion amount by the specified user increment amount (i.e. -Xmoi)
 * @return the updated expand size
 */
uintptr_t
MM_MemorySubSpace::adjustExpansionWithinUserIncrement(MM_EnvironmentBase* env, uintptr_t expandSize)
{
	return expandSize;
}

/**
 * Determine the maximum amount of memory subspace can expand by.
 * The amount returned is local to the receiver, and does not account for any other restrictions
 * imposed by its lineage.
 * @return the amount by which the receiver can expand
 */
uintptr_t
MM_MemorySubSpace::maxExpansion(MM_EnvironmentBase* env)
{
	return _maximumSize - _currentSize;
}

/**
 * Determine the maximum expansion amount the memory subspace can expand by.
 * The amount returned is restricted by values within the receiver of the call, as well as those imposed by
 * the parents of the receiver and the owning MemorySpace of the receiver.
 * @return the amount by which the receiver can expand
 */
uintptr_t
MM_MemorySubSpace::maxExpansionInSpace(MM_EnvironmentBase* env)
{
	uintptr_t max = _maximumSize - _currentSize;

	/* Is memory space aready at maximum size then we cant expand anymore */
	if (max == 0) {
		return 0;
	} else {
		if (_parent) {
			return OMR_MIN(_parent->maxExpansionInSpace(env), max);
		}
		return OMR_MIN(_memorySpace->maxExpansion(env), max);
	}
}

/**
 * Determine the maximum amount of memory subspace can contract by.
 * The amount returned is local to the receiver, and does not account for any other restrictions
 * imposed by its lineage.
 * @return the amount by which the receiver can contract
 */
uintptr_t
MM_MemorySubSpace::maxContraction(MM_EnvironmentBase* env)
{
	return _currentSize - _minimumSize;
}

/**
 * Determine the maximum amount of memory subspace can contract by.
 * The amount returned is restricted by values within the receiver of the call, as well as those imposed by
 * the parents of the receiver and the owning MemorySpace of the receiver.
 * @return the amount by which the receiver can contract
 */
uintptr_t
MM_MemorySubSpace::maxContractionInSpace(MM_EnvironmentBase* env)
{
	uintptr_t max = _currentSize - _minimumSize;

	/* Is memory space already at minimum size then we cant contrac anymore */
	if (max == 0) {
		return 0;
	} else {
		if (_parent) {
			return OMR_MIN(_parent->maxContractionInSpace(env), max);
		}
		return OMR_MIN(_memorySpace->maxContraction(env), max);
	}
}

/**
 * Pass the expand request from subspace to physical subarena
 * @return Expand the memory subspace by the required amount
 */
uintptr_t
MM_MemorySubSpace::expand(MM_EnvironmentBase* env, uintptr_t expandSize)
{
	uintptr_t actualExpandAmount;
	uint64_t timeStart, timeEnd;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	Trc_MM_MemorySubSpace_expand_Entry(env->getLanguageVMThread(), expandSize);

	/* Determine if the PSA or memory sub space can be expanded */
	if ((NULL == _physicalSubArena) || !_physicalSubArena->canExpand(env) || (maxExpansionInSpace(env) == 0)) {
		/* Cannot expand so dont bother trying */
		Trc_MM_MemorySubSpace_expand_Exit1(env->getLanguageVMThread());
		return 0;
	}

	timeStart = omrtime_hires_clock();
	/* Expand the sub arena by as much as we can up to the desrired amount */
	uintptr_t alignedExpandSize = MM_Math::roundToCeiling(_extensions->heapAlignment, expandSize);
	alignedExpandSize = MM_Math::roundToCeiling(_extensions->regionSize, alignedExpandSize);
	actualExpandAmount = _physicalSubArena->expand(env, OMR_MIN(alignedExpandSize, maxExpansionInSpace(env)));
	timeEnd = omrtime_hires_clock();
	_extensions->heap->getResizeStats()->setLastExpandTime(timeEnd - timeStart);

	reportHeapResizeAttempt(env, actualExpandAmount, HEAP_EXPAND);

	Trc_MM_MemorySubSpace_expand_Exit2(env->getLanguageVMThread(), actualExpandAmount);
	return actualExpandAmount;
}

/**
 * Contract the heap area by the as much as the byte amount specified.
 * @return The amount by which we actually managed to contract the heap.
 */
uintptr_t
MM_MemorySubSpace::contract(MM_EnvironmentBase* env, uintptr_t contractSize)
{
	uint64_t timeStart, timeEnd;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	Trc_MM_MemorySubSpace_contract_Entry(env->getLanguageVMThread(), contractSize);

	uintptr_t actualContractAmount;

	/* Determine if the PSA or memory sub space can be contracted */
	if ((NULL == _physicalSubArena) || !_physicalSubArena->canContract(env) || (maxContraction(env) == 0)) {
		/* Cannot contract so don't bother trying */
		Trc_MM_MemorySubSpace_contract_Exit1(env->getLanguageVMThread());
		return 0;
	}

	timeStart = omrtime_hires_clock();
	actualContractAmount = _physicalSubArena->contract(env, OMR_MIN(contractSize, maxContraction(env)));
	timeEnd = omrtime_hires_clock();
	_extensions->heap->getResizeStats()->setLastContractTime(timeEnd - timeStart);

	reportHeapResizeAttempt(env, actualContractAmount, HEAP_CONTRACT);

	Trc_MM_MemorySubSpace_contract_Exit(env->getLanguageVMThread(), actualContractAmount);
	return actualContractAmount;
}

/**
 * Memory described by the range has added to the heap and been made available to the subspace as free memory.
 */
bool
MM_MemorySubSpace::expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, MM_HeapRegionDescriptor* region, bool canCoalesce)
{
	/* Should never get here */
	Assert_MM_unreachable();

	return false;
}

/**
 * Memory described by the range has added to the heap and been made available to the subspace as free memory.
 */
bool
MM_MemorySubSpace::expanded(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress, bool canCoalesce)
{
	/* Should never get here */
	Assert_MM_unreachable();

	return false;
}

/**
 * Memory described by the range which was already part of the heap has been made available to the subspace
 * as free memory.
 * @note This is really a hack for now to explore what is needed for tilted (sliding) new spaces.
 * @warn Temporary routine that should not be used.
 */
void
MM_MemorySubSpace::addExistingMemory(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress, bool canCoalesce)
{
	/* Should never get here */
	Assert_MM_unreachable();
}

/**
 * Memory described by the range which was already part of the heap is being removed from the current memory spaces
 * ownership.  Adjust accordingly.
 * @return the start address of the range of memory removed.
 * @note This is really a hack for now to explore what is needed for tilted (sliding) new spaces.
 * @warn Temporary routine that should not be used.
 */
void*
MM_MemorySubSpace::removeExistingMemory(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, void* lowAddress, void* highAddress)
{
	/* Should never get here */
	Assert_MM_unreachable();
	return NULL;
}

/**
 * The heap has added a range of memory associated to the receiver or one of its children.
 * @note The low address is inclusive, the high address exclusive.
 */
bool
MM_MemorySubSpace::heapAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress)
{
	bool result = true;

	_currentSize += size;

	if (!_usesGlobalCollector && (NULL != _collector)) {
		result = _collector->heapAddRange(env, subspace, size, lowAddress, highAddress);
	}

	if (_parent) {
		result = result && _parent->heapAddRange(env, subspace, size, lowAddress, highAddress);
	} else if (_memorySpace) {
		result = result && _memorySpace->heapAddRange(env, subspace, size, lowAddress, highAddress);
	}
	return result;
}

/**
 * The heap has removed a range of memory associated to the receiver or one of its children.
 * @note The low address is inclusive, the high address exclusive.
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 * 
 */
bool
MM_MemorySubSpace::heapRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress, void* lowValidAddress, void* highValidAddress)
{
	bool result = true;

	_currentSize -= size;

	if (!_usesGlobalCollector && (NULL != _collector)) {
		result = _collector->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}

	if (_parent) {
		result = result && _parent->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	} else if (_memorySpace) {
		result = result && _memorySpace->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	}
	return result;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * No new memory has been added to a heap reconfiguration.  This call typically is the result
 * of having segment range changes (memory redistributed between segments) or the meaning of
 * memory changed.
 * 
 */
void
MM_MemorySubSpace::heapReconfigured(MM_EnvironmentBase* env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress)
{
	if (!_usesGlobalCollector && (NULL != _collector)) {
		_collector->heapReconfigured(env, reason, subspace, lowAddress, highAddress);
	}

	if (_parent) {
		_parent->heapReconfigured(env, reason, subspace, lowAddress, highAddress);
	} else if (_memorySpace) {
		_memorySpace->heapReconfigured(env, reason, subspace, lowAddress, highAddress);
	}
}

/**
 * Determine if the receiver will allow the contraction request size.
 * @return true if the contraction size fits into the receivers limits, false otherwise.
 */
bool
MM_MemorySubSpace::canContract(MM_EnvironmentBase* env, uintptr_t contractSize)
{
	if ((contractSize < _currentSize) && (_minimumSize <= (_currentSize - contractSize))) {
		if (_parent) {
			return _parent->canContract(env, contractSize);
		}
		return _memorySpace->canContract(env, contractSize);
	}
	return false;
}

#if defined (OMR_GC_MODRON_STANDARD)
/**
 * Find the free list entry whos end address matches the parameter.
 * 
 * @param addr Address to match against the high end of a free entry.
 * 
 * @return The leading address of the free entry whos top matches addr.
 */
void *
MM_MemorySubSpace::findFreeEntryEndingAtAddr(MM_EnvironmentBase *env, void *addr)
{
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Find the top of the free list entry whos start address matches the parameter.
 * 
 * @param addr Address to match against the low end of a free entry.
 * 
 * @return The trailing address of the free entry whos top matches addr.
 */
void *
MM_MemorySubSpace::findFreeEntryTopStartingAtAddr(MM_EnvironmentBase *env, void *addr)
{
	Assert_MM_unreachable();
	return NULL;
}
#endif /* OMR_GC_MODRON_STANDARD */

/**
 * Get the length of the free chunk at top of the heap.
 * 
 * @return The length of last chunk, 0 if no free chunk at end of heap.
 * 
 */
uintptr_t
MM_MemorySubSpace::getAvailableContractionSize(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	Assert_MM_unreachable();
	return 0;
}

/**
 * Return the length of the free chunk at the end of the heap
 * @return Length of free chunk at top of heap, or 0 if chunk at top of heap not free
 */
uintptr_t
MM_MemorySubSpace::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, void* lowAddr, void* highAddr)
{
	MM_MemoryPool* memoryPool = getMemoryPool((highAddr > lowAddr) ? (void*)(((uintptr_t)highAddr) - 1) : highAddr);

	Assert_MM_true(NULL != memoryPool); /* How did we get here? */
	return memoryPool->getAvailableContractionSizeForRangeEndingAt(env, allocDescription, lowAddr, highAddr);
}

#if defined (OMR_GC_MODRON_STANDARD)
/**
 * Find the address of the first entry on free list entry
 *
 * @return The address of head of free chain
 */
void*
MM_MemorySubSpace::getFirstFreeStartingAddr(MM_EnvironmentBase* env)
{
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Find the address of the next entry on free list entry
 *
 * @return The address of next free entry or NULL
 */
void*
MM_MemorySubSpace::getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree)
{
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Move a chunk of heap from one location to another within the receivers owned regions.
 *
 * @param srcBase Start address to move.
 * @param srcTop End address to move.
 * @param dstBase Start of destination address to move into.
 *
 */
void
MM_MemorySubSpace::moveHeap(MM_EnvironmentBase* env, void* srcBase, void* srcTop, void* dstBase)
{
	Assert_MM_unreachable();
}
#endif /* OMR_GC_MODRON_STANDARD */

/**
 * Expand the subspace due to a collector request. By defualt a subspace is not expandable for that reason.
 * Appropriate subclasses (like Flat subspace) my overlaod the behaviour
 *
 * @return the actual amount being expanded
 */
uintptr_t
MM_MemorySubSpace::collectorExpand(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription)
{
	/* unless overloaded, a subspace does not get expanded by a collector */
	return 0;
}

/**
 * Check the need for subspace resize. Normally called at the end of collection.
 * This method is supposed to set class members that determine amount of the resize. Latter these variables are read
 * by perform resize, who does the acutal resize.
 *
 */
void
MM_MemorySubSpace::checkResize(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, bool _systemGC)
{
}

/**
 * Perform the resize of the subspace, based on decisions made by checkResize
 *
 * @return amount of the resize (negative for contract, positive for expand)
 */
intptr_t
MM_MemorySubSpace::performResize(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	return 0;
}

/**
 * Internal Allocation.
 * @todo Provide class documentation
 */
void*
MM_MemorySubSpace::collectorAllocate(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription)
{
	/* Should not be called */
	Assert_MM_unreachable();
	return NULL;
}

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
/**
 * Internal Allocation.
 * @todo Provide class documentation
 */
void*
MM_MemorySubSpace::collectorAllocateTLH(MM_EnvironmentBase* env, MM_Collector* requestCollector, MM_AllocateDescription* allocDescription, uintptr_t maximumBytesRequired, void*& addrBase, void*& addrTop)
{
	/* Should not be called */
	Assert_MM_unreachable();
	return NULL;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

/**
 * Broadcast to allow subspaces that allocation of new TLH's is to be resumed
 */
void
MM_MemorySubSpace::setAllocateAtSafePointOnly(MM_EnvironmentBase* env, bool safePoint)
{
	/* Call on children */
	MM_MemorySubSpace* child;
	child = _children;

	while (child) {
		child->setAllocateAtSafePointOnly(env, safePoint);
		child = child->getNext();
	}
}

/**
 * Reset the largest free chunk of all memorySubSpaces to 0.
 */
void
MM_MemorySubSpace::resetLargestFreeEntry()
{
	MM_MemorySubSpace* child = _children;
	MM_MemoryPool* memoryPool = getMemoryPool();

	if (NULL != memoryPool) {
		memoryPool->resetLargestFreeEntry();
	}

	while (child) {
		child->resetLargestFreeEntry();
		child = child->getNext();
	}
}

void
MM_MemorySubSpace::recycleRegion(MM_EnvironmentBase* env, MM_HeapRegionDescriptor* region)
{
	/* Should have been implemented */
	Assert_MM_unreachable();
}

/**
 * Return the largest free chunk of all memorySubSpaces that satisifies the allocateDescription
 * found during Sweep phase of global collect.
 */
uintptr_t
MM_MemorySubSpace::findLargestFreeEntry(MM_EnvironmentBase* env, MM_AllocateDescription* allocateDescription)
{
	uintptr_t largestFreeEntry = 0;
	uintptr_t maximumLargestFreeEntry = 0;

	/*If memory type of MSS appropriate and...  */
	if (!allocateDescription->getTenuredFlag() || (MEMORY_TYPE_OLD == (getTypeFlags() & MEMORY_TYPE_OLD))) {

		MM_MemoryPool* memoryPool = getMemoryPool();

		/* ..subspace is allocatable and has a memory pool */
		if (_isAllocatable && memoryPool) {
			maximumLargestFreeEntry = memoryPool->getLargestFreeEntry();
		}
	}

	/* Call on children */
	MM_MemorySubSpace* child = _children;
	while (child) {
		largestFreeEntry = child->findLargestFreeEntry(env, allocateDescription);
		maximumLargestFreeEntry = OMR_MAX(largestFreeEntry, maximumLargestFreeEntry);
		child = child->getNext();
	}

	return maximumLargestFreeEntry;
}

/**
 * Counter balance a contract.
 * React to a pending contract of the receiver by possibly adjusted (expanding) other subspaces to fill minimum
 * quotas, etc.  The call assumes that the contract size is at least small enough to fit into the locally available
 * subspace (ie: that it will fit according to minimum size restrictions).
 * @return the adjusted contract size that is allowed to the receiver.
 */
uintptr_t
MM_MemorySubSpace::counterBalanceContract(MM_EnvironmentBase* env, uintptr_t contractSize, uintptr_t contractAlignment)
{
	if (NULL != _parent) {
		return _parent->counterBalanceContract(env, this, this, contractSize, contractAlignment);
	}
	return contractSize;
}

/**
 * Counter balance a contract.
 * React to a pending contract of the the given subspace by possibly adjusted (expanding) other subspaces to fill minimum
 * quotas, etc.
 * The generic implementation consists of limiting the contraction to set limits.  Override to allow sibling of the
 * contracting subspace to fill any quotas.
 * @return the adjusted contract size that is allowed to the receiver.
 */
uintptr_t
MM_MemorySubSpace::counterBalanceContract(MM_EnvironmentBase* env, MM_MemorySubSpace* previousSubSpace, MM_MemorySubSpace* contractSubSpace, uintptr_t contractSize, uintptr_t contractAlignment)
{
	uintptr_t adjustedContractSize;

	adjustedContractSize = OMR_MIN(contractSize, maxContraction(env));

	if ((adjustedContractSize != 0) && (NULL != _parent)) {
		return _parent->counterBalanceContract(env, this, contractSubSpace, adjustedContractSize, contractAlignment);
	}

	return adjustedContractSize;
}

/**
 * Counter balance a contract with an expand.
 * React to a pending contract of a subspace by expanding to filling the remaining quota with the expand request.  The expand request size can
 * be adjusted to be lower.
 * @todo adjusting the expand size may have an effect on the virtual addresses - needs to be addressed.
 * @return the adjusted contract size based on the expand that was possible.
 */
uintptr_t
MM_MemorySubSpace::counterBalanceContractWithExpand(MM_EnvironmentBase* env, MM_MemorySubSpace* previousSubSpace, MM_MemorySubSpace* contractSubSpace,
													uintptr_t contractSize, uintptr_t contractAlignment, uintptr_t expandSize)
{
	if (NULL != _physicalSubArena) {
		uintptr_t maximumExpandSize;
		uintptr_t adjustedExpandSize;
		uintptr_t adjustedContractSize;

		/* Record the expand and contract sizes */
		adjustedContractSize = contractSize;
		adjustedExpandSize = expandSize;

		/* Check the expand size against the current local limits of the receiver */
		maximumExpandSize = maxExpansion(env);
		if (maximumExpandSize < adjustedExpandSize) {
			uintptr_t expandSizeDelta;

			/* The expand request is too large - reduce the amount while keeping the contraction alignment in mind */
			expandSizeDelta = MM_Math::roundToCeiling(contractAlignment, adjustedExpandSize - maximumExpandSize);

			/* If the adjusted expand request has been reduced to 0, then we cannot expand.  Adjust the contraction size to account
			 * for the lack of an expand, and return the value.  The expandSizeDelta can be increased through rounding to be
			 * greater than the adjustedExpandSize (hence the >= check)
			 */
			if (expandSizeDelta >= adjustedExpandSize) {
				adjustedContractSize = (adjustedContractSize > adjustedExpandSize) ? adjustedContractSize - adjustedExpandSize : 0;
				return MM_Math::roundToFloor(contractAlignment, adjustedContractSize);
			}

			/* Apply the delta */
			Assert_MM_true(expandSizeDelta <= adjustedContractSize);
			Assert_MM_true(expandSizeDelta <= adjustedExpandSize);
			adjustedContractSize = adjustedContractSize - expandSizeDelta;
			adjustedExpandSize = adjustedExpandSize - expandSizeDelta;
		}

		/* Check with the PSA for the adjusted counter balance expand size (physical memory restrictions) */
		uintptr_t psaExpandSize;

		psaExpandSize = _physicalSubArena->checkCounterBalanceExpand(env, contractAlignment, adjustedExpandSize);
		Assert_MM_true(psaExpandSize <= adjustedExpandSize);
		if (0 == psaExpandSize) {
			/* No room for expansion, return the adjusted contract size */
			adjustedContractSize = (adjustedContractSize > adjustedExpandSize) ? adjustedContractSize - adjustedExpandSize : 0;
			return MM_Math::roundToFloor(contractAlignment, adjustedContractSize);
		}

		/* Record the adjusted expand and contract sizes */
		Assert_MM_true((adjustedExpandSize - psaExpandSize) <= adjustedContractSize);
		adjustedContractSize = adjustedContractSize - (adjustedExpandSize - psaExpandSize);
		adjustedExpandSize = psaExpandSize;

		/* Enqueue the receiver as a counter balancing sub space in the contract sub space */
		contractSubSpace->enqueueCounterBalanceExpand(env, this, adjustedExpandSize);

		return adjustedContractSize;
	}

	Assert_MM_unreachable(); /* This should have been overridden if you get here (?) */
	return contractSize;
}

/**
 * Enqueue the expand request for the given subspace into the receiver to be triggered when the current size adjustment
 * takes place.
 * @todo Eventually this routine might want to account for multiple counterbalancing actions on the same space.
 * @todo Should the chaining algorithm not put things at the end of the list?
 */
void
MM_MemorySubSpace::enqueueCounterBalanceExpand(MM_EnvironmentBase* env, MM_MemorySubSpace* subSpace, uintptr_t expandSize)
{
	/* Set the counter balancing mode of the subspace */
	subSpace->_counterBalanceType = MODRON_COUNTER_BALANCE_TYPE_EXPAND;
	subSpace->_counterBalanceSize = expandSize;

	/* Connect the subspace to the receiver */
	subSpace->_counterBalanceChain = _counterBalanceChainHead;
	_counterBalanceChainHead = subSpace;
}

/**
 * Execute an counter balancing measures that are to be taken by other subspaces based on size adjustments made by the
 * receiver.
 */
void
MM_MemorySubSpace::triggerEnqueuedCounterBalancing(MM_EnvironmentBase* env)
{
	MM_MemorySubSpace* subSpace;

	subSpace = _counterBalanceChainHead;

	while (subSpace) {
		MM_MemorySubSpace* tmpNext;
		subSpace->runEnqueuedCounterBalancing(env);
		tmpNext = subSpace->_counterBalanceChain;
		subSpace->clearCounterBalancing();
		subSpace = tmpNext;
	}

	_counterBalanceChainHead = NULL;
}

/**
 * Clear enqueued counter balancing measures from the receiver.
 * This operation cancels any enqueued counter balancing, and is typically used in failure cases after a counter
 * balance has been decided upon (the resulting size change is unacceptable by the receiver).
 */
void
MM_MemorySubSpace::clearEnqueuedCounterBalancing(MM_EnvironmentBase* env)
{
	MM_MemorySubSpace* subSpace;

	subSpace = _counterBalanceChainHead;

	while (subSpace) {
		MM_MemorySubSpace* tmpNext;
		tmpNext = subSpace->_counterBalanceChain;
		subSpace->clearCounterBalancing();
		subSpace = tmpNext;
	}

	_counterBalanceChainHead = NULL;
}

/**
 * Run the counter balancing measures of the receiver.
 */
void
MM_MemorySubSpace::runEnqueuedCounterBalancing(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	Assert_MM_true(_physicalSubArena != NULL); /* How did we get enqueued if this is NULL? */

	if (NULL != _physicalSubArena) {
		switch (_counterBalanceType) {
		case MODRON_COUNTER_BALANCE_TYPE_EXPAND:
			uintptr_t expandSize;
			uint64_t timeStart, timeEnd;

			timeStart = omrtime_hires_clock();
			expandSize = _physicalSubArena->expandNoCheck(env, _counterBalanceSize);
			timeEnd = omrtime_hires_clock();
			Assert_MM_true(expandSize == _counterBalanceSize);
			_extensions->heap->getResizeStats()->setLastExpandTime(timeEnd - timeStart);

			if (0 != expandSize) {
				reportHeapResizeAttempt(env, expandSize, HEAP_EXPAND);
			}

			break;
		default:
			Assert_MM_unreachable();
			break;
		}
	}
}

#if defined(OMR_GC_CONCURRENT_SWEEP)
/**
 * Replenish a pools free lists to satisfy a given allocate.
 * The given pool was unable to satisfy an allocation request of (at least) the given size.  See if there is work
 * that can be done to increase the free stores of the pool so that the request can be met.
 * @note This call is made under the pools allocation lock (or equivalent)
 * @return True if the pool was replenished with a free entry that can satisfy the size, false otherwise.
 */
bool
MM_MemorySubSpace::replenishPoolForAllocate(MM_EnvironmentBase* env, MM_MemoryPool* memoryPool, uintptr_t size)
{
	/* If we have no parent, we're the top most subspace, see if the (global) collector can help us */
	if (NULL == _parent) {
		if (_usesGlobalCollector && (NULL != _collector)) {
			return _collector->replenishPoolForAllocate(env, memoryPool, size);
		}
		return false;
	}

	/* We have a parent, forward the request to it */
	return _parent->replenishPoolForAllocate(env, memoryPool, size);
}
#endif /* OMR_GC_CONCURRENT_SWEEP */

/**
 * Determine whether the given subspace is a descendant of the receiver.
 * @return true if the given subspace is a descendant of the receiver, false otherwise.
 */
bool
MM_MemorySubSpace::isDescendant(MM_MemorySubSpace* memorySubSpace)
{
	MM_MemorySubSpace* currentSubSpace;

	currentSubSpace = memorySubSpace;
	do {
		if (this == currentSubSpace) {
			return true;
		}
		currentSubSpace = currentSubSpace->getParent();
	} while (NULL != currentSubSpace);

	return false;
}

bool
MM_MemorySubSpace::isPartOfSemiSpace()
{
	bool result = false;

	if (NULL != _parent) {
		result = _parent->isPartOfSemiSpace();
	}
	return result;
}

void
MM_MemorySubSpace::registerRegion(MM_HeapRegionDescriptor* region)
{
	lockRegionList();

	J9_LINEAR_LINKED_LIST_ADD(_nextRegionInSubSpace, _previousRegionInSubSpace, _regionList, region);

	unlockRegionList();
}

void
MM_MemorySubSpace::unregisterRegion(MM_HeapRegionDescriptor* region)
{
	lockRegionList();

	J9_LINEAR_LINKED_LIST_REMOVE(_nextRegionInSubSpace, _previousRegionInSubSpace, _regionList, region);

	unlockRegionList();
}

void
MM_MemorySubSpace::lockRegionList()
{
	_lock.acquire();
}

void
MM_MemorySubSpace::unlockRegionList()
{
	_lock.release();
}

MM_HeapRegionDescriptor*
MM_MemorySubSpace::getFirstRegion()
{
	return _regionList;
}

MM_HeapRegionDescriptor*
MM_MemorySubSpace::getNextRegion(MM_HeapRegionDescriptor* region)
{
	return region->_nextRegionInSubSpace;
}

MM_HeapRegionDescriptor*
MM_MemorySubSpace::selectRegionForContraction(MM_EnvironmentBase* env, uintptr_t numaNode)
{
	Assert_MM_unreachable();
	return NULL;
}

bool
MM_MemorySubSpace::wasContractedThisGC(uintptr_t gcCount)
{
	return (gcCount == _extensions->heap->getResizeStats()->getLastHeapContractionGCCount());
}

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
uintptr_t
MM_MemorySubSpace::releaseFreeMemoryPages(MM_EnvironmentBase* env)
{
	Assert_MM_unreachable();
        return 0;
}
#endif
