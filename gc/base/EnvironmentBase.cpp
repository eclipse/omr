/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include "EnvironmentBase.hpp"

#include "omrhookable.h"
#include "j9nongenerated.h"
#include "omrport.h"
#include "mmhook_common.h"
#include "mmprivatehook.h"
#include "mmprivatehook_internal.h"
#include "ut_j9mm.h"

#include "AllocateDescription.hpp"
#include "EnvironmentLanguageInterface.hpp"
#include "GlobalAllocationManager.hpp"
#include "Heap.hpp"
#include "ModronAssertions.h"
#include "MemorySpace.hpp"
#include "ObjectAllocationInterface.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)
#include "RegionPoolSegregated.hpp"
#include "HeapRegionQueue.hpp"
#include "SegregatedAllocationTracker.hpp"
#endif /* defined(OMR_GC_SEGREGATED_HEAP) */

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.
 */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"

class MM_Collector;

MM_EnvironmentBase *
MM_EnvironmentBase::newInstance(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread)
{
	void *envPtr;
	MM_EnvironmentBase *env = NULL;

	envPtr = (void *)pool_newElement(extensions->environments);
	if (NULL != envPtr) {
		env = new(envPtr) MM_EnvironmentBase(omrVMThread);
		if (!env->initialize(extensions)) {
			env->kill();
			env = NULL;
		}
	}

	return env;
}

/**
 * Null method
 */
void
MM_EnvironmentBase::kill()
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);

	tearDown(extensions);
	/* This requires that the environments pool be locked */
	pool_removeElement(extensions->environments, (void *)this);
}

bool
MM_EnvironmentBase::initialize(MM_GCExtensionsBase *extensions)
{
	setEnvironmentId(MM_AtomicOperations::add(&extensions->currentEnvironmentCount, 1) - 1);
	setAllocationColor(extensions->newThreadAllocationColor);

	if (extensions->isStandardGC()) {
		/* pass veryLargeObjectThreshold = 0 to initialize limited size of veryLargeEntryPool for thread (to reduce footprint), 
		 * but if the threshold is bigger than maxHeap size, we would pass orignal threshold to indicate no veryLargeEntryPool needed 
		 */
		uintptr_t veryLargeObjectThreshold = (extensions->largeObjectAllocationProfilingVeryLargeObjectThreshold <= extensions->memoryMax)?0:extensions->largeObjectAllocationProfilingVeryLargeObjectThreshold;
		if (!_freeEntrySizeClassStats.initialize(this, extensions->largeObjectAllocationProfilingTopK, extensions->freeMemoryProfileMaxSizeClasses, veryLargeObjectThreshold)) {
			return false;
		}
	}

#if defined(OMR_GC_SEGREGATED_HEAP)
	if (extensions->isSegregatedHeap()) {
		_regionWorkList = MM_RegionPoolSegregated::allocateHeapRegionQueue(this, MM_HeapRegionList::HRL_KIND_LOCAL_WORK, true, false, false);
		if (NULL == _regionWorkList) {
			return false;
		}
		_regionLocalFree = MM_RegionPoolSegregated::allocateHeapRegionQueue(this, MM_HeapRegionList::HRL_KIND_LOCAL_WORK, true, false, false);
		if (NULL == _regionLocalFree) {
			return false;
		}
		_regionLocalFull = MM_RegionPoolSegregated::allocateHeapRegionQueue(this, MM_HeapRegionList::HRL_KIND_LOCAL_WORK, true, false, false);
		if (NULL == _regionLocalFull) {
			return false;
		}
	}
#endif /* OMR_GC_SEGREGATED_HEAP */

	return true;
}

void
MM_EnvironmentBase::tearDown(MM_GCExtensionsBase *extensions)
{
#if defined(OMR_GC_SEGREGATED_HEAP)
	if (_regionWorkList != NULL) {
		_regionWorkList->kill(this);
		_regionWorkList = NULL;

	}
	if (_regionLocalFree != NULL) {
		_regionLocalFree->kill(this);
		_regionLocalFree = NULL;
	}

	if (NULL != _regionLocalFull) {
		_regionLocalFull->kill(this);
		_regionLocalFull = NULL;
	}

	if (NULL != _allocationTracker) {
		_allocationTracker->kill(this);
		_allocationTracker = NULL;
	}
#endif /* OMR_GC_SEGREGATED_HEAP */

	if(NULL != _envLanguageInterface) {
		_envLanguageInterface->kill(this);
		_envLanguageInterface = NULL;
	}

	if(NULL != _objectAllocationInterface) {
		_objectAllocationInterface->kill(this);
		_objectAllocationInterface = NULL;
	}

	MM_AtomicOperations::subtract(&extensions->currentEnvironmentCount, 1);

	if (getOmrVMThread() == extensions->vmThreadAllocatedMost) {
		/* we lose information about thread that allocated most, but at least the user of this value will not crash */
		extensions->vmThreadAllocatedMost = NULL;
	}

	_freeEntrySizeClassStats.tearDown(this);

	if (NULL != extensions->globalAllocationManager) {
		extensions->globalAllocationManager->releaseAllocationContext(this);
	}
}

/**
 * Set the vmState to that supplied, and return the previous
 * state so it can be restored later
 */
uintptr_t
MM_EnvironmentBase::pushVMstate(uintptr_t newState)
{
	uintptr_t oldState = _omrVMThread->vmState;
	_omrVMThread->vmState = newState;
	return oldState;
}

/**
 * Restore the vmState to that supplied (which should have
 * been previously obtained through a call to @ref pushVMstate()
 */
void
MM_EnvironmentBase::popVMstate(uintptr_t newState)
{
	_omrVMThread->vmState = newState;
}

void
MM_EnvironmentBase::allocationFailureStartReportIfRequired(MM_AllocateDescription *allocDescription, uintptr_t flags)
{
	if (!_allocationFailureReported) {
		MM_GCExtensionsBase *extensions = getExtensions();
		OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

		Trc_MM_AllocationFailureStart(getLanguageVMThread(),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
			(extensions->largeObjectArea ? extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
			(extensions->largeObjectArea ? extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0),
			allocDescription->getBytesRequested());

		Trc_OMRMM_AllocationFailureStart(getOmrVMThread(),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
			(extensions->largeObjectArea ? extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
			(extensions->largeObjectArea ? extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0),
			allocDescription->getBytesRequested());

		if (J9_EVENT_IS_HOOKED(extensions->privateHookInterface, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_START)) {
			MM_CommonGCStartData commonData;
			extensions->heap->initializeCommonGCStartData(this, &commonData);

			ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_START(
				extensions->privateHookInterface,
				getOmrVMThread(),
				omrtime_hires_clock(),
				J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_START,
				allocDescription->getBytesRequested(),
				&commonData,
				flags,
				allocDescription->getTenuredFlag());
		}

		_allocationFailureReported = true;
	}
}

void
MM_EnvironmentBase::allocationFailureEndReportIfRequired(MM_AllocateDescription *allocDescription)
{
	if (_allocationFailureReported) {
		MM_GCExtensionsBase *extensions = getExtensions();
		OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

		TRIGGER_J9HOOK_MM_PRIVATE_FAILED_ALLOCATION_COMPLETED(
			extensions->privateHookInterface,
			getOmrVMThread(),
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_FAILED_ALLOCATION_COMPLETED,
			allocDescription->getAllocationSucceeded() ? TRUE : FALSE,
			allocDescription->getBytesRequested());

		Trc_MM_AllocationFailureEnd(getLanguageVMThread(),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
			(extensions->largeObjectArea ? extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
			(extensions->largeObjectArea ? extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));

		Trc_OMRMM_AllocationFailureEnd(getOmrVMThread(),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
			extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
			extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
			(extensions->largeObjectArea ? extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
			(extensions->largeObjectArea ? extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0));

		if (J9_EVENT_IS_HOOKED(extensions->privateHookInterface, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END)) {
			MM_CommonGCEndData commonData;
			extensions->heap->initializeCommonGCEndData(this, &commonData);

			ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END(
				extensions->privateHookInterface,
				getOmrVMThread(),
				omrtime_hires_clock(),
				J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_END,
				getExclusiveAccessTime(),
				&commonData,
				allocDescription);
		}

		_allocationFailureReported = false;
	}
}

bool
MM_EnvironmentBase::tryAcquireExclusiveVMAccess()
{
	return _envLanguageInterface->tryAcquireExclusiveVMAccess();
}

bool
MM_EnvironmentBase::inquireExclusiveVMAccessForGC()
{
	return _envLanguageInterface->inquireExclusiveVMAccessForGC();
}

bool
MM_EnvironmentBase::acquireExclusiveVMAccessForGC(MM_Collector *collector)
{
	return _envLanguageInterface->acquireExclusiveVMAccessForGC(collector);
}

void
MM_EnvironmentBase::releaseExclusiveVMAccessForGC()
{
	_envLanguageInterface->releaseExclusiveVMAccessForGC();
}

void
MM_EnvironmentBase::unwindExclusiveVMAccessForGC()
{
	_envLanguageInterface->unwindExclusiveVMAccessForGC();
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
bool
MM_EnvironmentBase::tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats)
{
	return _envLanguageInterface->tryAcquireExclusiveForConcurrentKickoff(stats);
}

void
MM_EnvironmentBase::releaseExclusiveForConcurrentKickoff()
{
	_envLanguageInterface->releaseExclusiveForConcurrentKickoff();
}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

bool
MM_EnvironmentBase::tryAcquireExclusiveVMAccessForGC(MM_Collector *collector)
{
	return _envLanguageInterface->tryAcquireExclusiveVMAccessForGC(collector);
}

void
MM_EnvironmentBase::acquireExclusiveVMAccess()
{
	_envLanguageInterface->acquireExclusiveVMAccess();
}

void
MM_EnvironmentBase::releaseExclusiveVMAccess()
{
	_envLanguageInterface->releaseExclusiveVMAccess();
}

bool
MM_EnvironmentBase::isExclusiveAccessRequestWaiting()
{
	return _envLanguageInterface->isExclusiveAccessRequestWaiting();
}

uint64_t
MM_EnvironmentBase::getExclusiveAccessTime()
{
	return _envLanguageInterface->getExclusiveAccessTime();
}

uint64_t
MM_EnvironmentBase::getMeanExclusiveAccessIdleTime()
{
	return _envLanguageInterface->getMeanExclusiveAccessIdleTime();
}

OMR_VMThread*
MM_EnvironmentBase::getLastExclusiveAccessResponder()
{
	return _envLanguageInterface->getLastExclusiveAccessResponder();
}

uintptr_t
MM_EnvironmentBase::getExclusiveAccessHaltedThreads()
{
	return _envLanguageInterface->getExclusiveAccessHaltedThreads();
}

bool
MM_EnvironmentBase::exclusiveAccessBeatenByOtherThread()
{
	return _envLanguageInterface->exclusiveAccessBeatenByOtherThread();
}

MM_MemorySubSpace *
MM_EnvironmentBase::getDefaultMemorySubSpace()
{
	return getMemorySpace()->getDefaultMemorySubSpace();
}

MM_MemorySubSpace *
MM_EnvironmentBase::getTenureMemorySubSpace()
{
	return getMemorySpace()->getTenureMemorySubSpace();
}
