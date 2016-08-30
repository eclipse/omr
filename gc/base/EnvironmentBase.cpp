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

#include "omrcfg.h"

#include "j9nongenerated.h"
#include "mmhook_common.h"
#include "mmprivatehook.h"
#include "mmprivatehook_internal.h"
#include "omrhookable.h"
#include "omrport.h"
#include "ut_j9mm.h"

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "ConcurrentGCStats.hpp"
#include "EnvironmentLanguageInterface.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalAllocationManager.hpp"
#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "ModronAssertions.h"
#include "OMRVMInterface.hpp"
#include "ObjectAllocationInterface.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)
#include "HeapRegionQueue.hpp"
#include "RegionPoolSegregated.hpp"
#include "SegregatedAllocationTracker.hpp"
#endif /* defined(OMR_GC_SEGREGATED_HEAP) */

#include "EnvironmentBase.hpp"

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.  */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"

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

#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	if (!_hotFieldStats.initialize(this)) {
		return false;
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */

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

#if defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC)
	_hotFieldStats.tearDown(this);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) || defined(OMR_GC_VLHGC) */

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
MM_EnvironmentBase::reportExclusiveAccessAcquire()
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

	/* record statistics */
	U_64 meanResponseTime = _omrVM->exclusiveVMAccessStats.totalResponseTime / (_omrVM->exclusiveVMAccessStats.haltedThreads + 1); /* +1 for the requester */
	_exclusiveAccessTime = _omrVM->exclusiveVMAccessStats.endTime - _omrVM->exclusiveVMAccessStats.startTime;
	_meanExclusiveAccessIdleTime = _exclusiveAccessTime - meanResponseTime;
	_lastExclusiveAccessResponder = _omrVM->exclusiveVMAccessStats.lastResponder;
	_exclusiveAccessHaltedThreads = _omrVM->exclusiveVMAccessStats.haltedThreads;

	/* report hook */
	/* first the deprecated trigger */
	TRIGGER_J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS(this->getExtensions()->privateHookInterface, _omrVMThread);
	/* now the new trigger */
	TRIGGER_J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_ACQUIRE(
			this->getExtensions()->privateHookInterface,
			_omrVMThread,
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_ACQUIRE,
			_exclusiveAccessTime,
			_meanExclusiveAccessIdleTime,
			_lastExclusiveAccessResponder,
			_exclusiveAccessHaltedThreads);
}

void
MM_EnvironmentBase::reportExclusiveAccessRelease()
{
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	TRIGGER_J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_RELEASE(
				this->getExtensions()->privateHookInterface,
				_omrVMThread,
				omrtime_hires_clock(),
				J9HOOK_MM_PRIVATE_EXCLUSIVE_ACCESS_RELEASE);
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

void
MM_EnvironmentBase::acquireVMAccess()
{
	_envLanguageInterface->acquireVMAccess();
}

void
MM_EnvironmentBase::releaseVMAccess()
{
	_envLanguageInterface->releaseVMAccess();
}

bool
MM_EnvironmentBase::tryAcquireExclusiveVMAccess()
{
	if (0 == _omrVMThread->exclusiveCount) {
		if (_envLanguageInterface->tryAcquireExclusiveVMAccess()) {
			/* Report exclusive access time if we won race */
			reportExclusiveAccessAcquire();
		} else {
			return false;
		}
	}
	_omrVMThread->exclusiveCount += 1;
	return true;
}

void
MM_EnvironmentBase::acquireExclusiveVMAccess()
{
	if (0 == _omrVMThread->exclusiveCount) {
		_envLanguageInterface->acquireExclusiveVMAccess();
		reportExclusiveAccessAcquire();
	}
	_omrVMThread->exclusiveCount += 1;
}

void
MM_EnvironmentBase::releaseExclusiveVMAccess()
{
	Assert_MM_true(0 < _omrVMThread->exclusiveCount);
	_omrVMThread->exclusiveCount -= 1;
	if (0 == _omrVMThread->exclusiveCount) {
		reportExclusiveAccessRelease();
		_envLanguageInterface->releaseExclusiveVMAccess();
	}
}

bool
MM_EnvironmentBase::isExclusiveAccessRequestWaiting()
{
	return _envLanguageInterface->isExclusiveAccessRequestWaiting();
}

bool
MM_EnvironmentBase::acquireExclusiveVMAccessForGC(MM_Collector *collector)
{
	MM_GCExtensionsBase *extensions = getExtensions();
	uintptr_t collectorAccessCount = collector->getExclusiveAccessCount();

	_exclusiveAccessBeatenByOtherThread = false;

	while(_omrVMThread != extensions->gcExclusiveAccessThreadId) {
		if(NULL == extensions->gcExclusiveAccessThreadId) {
			/* there is a chance the thread can win the race to acquiring
			 * exclusive for GC */
			omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
			if(NULL == extensions->gcExclusiveAccessThreadId) {
				/* thread is the winner and will request the GC */
				extensions->gcExclusiveAccessThreadId = _omrVMThread;
			}
			omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);
		}

		if(_omrVMThread != extensions->gcExclusiveAccessThreadId) {
			/* thread was not the winner for requesting a GC - allow the GC to
			 * proceed and wait for it to complete */
			Assert_MM_true(NULL != extensions->gcExclusiveAccessThreadId);

			_envLanguageInterface->exclusiveAccessForGCBeatenByOtherThread();

			_envLanguageInterface->releaseVMAccess();

			/* there is a chance the GC will already have executed at this
			 * point or other threads will re-win and re-execute.  loop until
			 * the thread sees that no more GCs are being requested.
			 */
			omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
			while(NULL != extensions->gcExclusiveAccessThreadId) {
				omrthread_monitor_wait(extensions->gcExclusiveAccessMutex);
			}
			/* thread can now win and will request a GC */
			extensions->gcExclusiveAccessThreadId = _omrVMThread;

			omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);

			this->acquireVMAccess();
			_envLanguageInterface->exclusiveAccessForGCObtainedAfterBeatenByOtherThread();
		}
	}

	/* thread is the winner for requesting a GC (possibly through recursive
	 * calls).  proceed with acquiring exclusive access. */
	Assert_MM_true(_omrVMThread == extensions->gcExclusiveAccessThreadId);

	this->acquireExclusiveVMAccess();

	_exclusiveAccessBeatenByOtherThread = !(collector->getExclusiveAccessCount() == collectorAccessCount);

	collector->incrementExclusiveAccessCount();

	GC_OMRVMInterface::flushCachesForGC(this);

	return !_exclusiveAccessBeatenByOtherThread;

}

bool
MM_EnvironmentBase::tryAcquireExclusiveVMAccessForGC(MM_Collector *collector)
{
	MM_GCExtensionsBase *extensions = getExtensions();
	uintptr_t collectorAccessCount = collector->getExclusiveAccessCount();

	_exclusiveAccessBeatenByOtherThread = false;

	while(_omrVMThread != extensions->gcExclusiveAccessThreadId) {
		if(NULL == extensions->gcExclusiveAccessThreadId) {
			/* there is a chance the thread can win the race to acquiring exclusive for GC */
			omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
			if(NULL == extensions->gcExclusiveAccessThreadId) {
				/* thread is the winner and will request the GC */
				extensions->gcExclusiveAccessThreadId = _omrVMThread;
			}
			omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);
		}

		if(_omrVMThread != extensions->gcExclusiveAccessThreadId) {
			/* thread was not the winner for requesting a GC - allow the GC to proceed and wait for it to complete */
			Assert_MM_true(NULL != extensions->gcExclusiveAccessThreadId);

			uintptr_t accessMask;
			_envLanguageInterface->releaseCriticalHeapAccess(&accessMask);

			/* there is a chance the GC will already have executed at this point or other threads will re-win and re-execute.  loop until the
			 * thread sees that no more GCs are being requested.
			 */
			omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
			while(NULL != extensions->gcExclusiveAccessThreadId) {
				omrthread_monitor_wait(extensions->gcExclusiveAccessMutex);
			}
			omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);

			_envLanguageInterface->reacquireCriticalHeapAccess(accessMask);

			/* May have been beaten to a GC, but perhaps not the one we wanted.  Check and if in fact the collection we intended has been
			 * completed, we will not acquire exclusive access.
			 */
			if(collector->getExclusiveAccessCount() != collectorAccessCount) {
				return false;
			}
		}
	}

	/* thread is the winner for requesting a GC (possibly through recursive calls).  proceed with acquiring exclusive access. */
	Assert_MM_true(_omrVMThread == extensions->gcExclusiveAccessThreadId);

	this->acquireExclusiveVMAccess();

	collector->incrementExclusiveAccessCount();

	GC_OMRVMInterface::flushCachesForGC(this);

	return true;
}

void
MM_EnvironmentBase::releaseExclusiveVMAccessForGC()
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);

	Assert_MM_true(extensions->gcExclusiveAccessThreadId == _omrVMThread);
	Assert_MM_true(0 != _omrVMThread->exclusiveCount);

	_omrVMThread->exclusiveCount -= 1;
	if(0 == _omrVMThread->exclusiveCount) {
		omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
		extensions->gcExclusiveAccessThreadId = NULL;
		omrthread_monitor_notify_all(extensions->gcExclusiveAccessMutex);
		omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);

		reportExclusiveAccessRelease();

		_envLanguageInterface->releaseExclusiveVMAccess();
	}
}

void
MM_EnvironmentBase::unwindExclusiveVMAccessForGC()
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);

	if(_omrVMThread->exclusiveCount > 0) {
		Assert_MM_true(extensions->gcExclusiveAccessThreadId == _omrVMThread);

		_omrVMThread->exclusiveCount = 0;

		omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
		extensions->gcExclusiveAccessThreadId = NULL;
		omrthread_monitor_notify_all(extensions->gcExclusiveAccessMutex);
		omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);

		reportExclusiveAccessRelease();

		_envLanguageInterface->releaseExclusiveVMAccess();
	}
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
bool
MM_EnvironmentBase::tryAcquireExclusiveForConcurrentKickoff(MM_ConcurrentGCStats *stats)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);
	uintptr_t gcCount = extensions->globalGCStats.gcCount;

	while (_omrVMThread != extensions->gcExclusiveAccessThreadId) {
		if (NULL == extensions->gcExclusiveAccessThreadId) {
			/* there is a chance the thread can win the race to acquiring exclusive for GC */
			omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
			if (NULL == extensions->gcExclusiveAccessThreadId) {
				/* thread is the winner and will request the GC */
				extensions->gcExclusiveAccessThreadId =_omrVMThread ;
			}
			omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);
		}

		if (_omrVMThread != extensions->gcExclusiveAccessThreadId) {
			/* thread was not the winner for requesting a GC - allow the GC to proceed and wait for it to complete */
			Assert_MM_true(NULL != extensions->gcExclusiveAccessThreadId);

			uintptr_t accessMask = 0;

			_envLanguageInterface->releaseCriticalHeapAccess(&accessMask);

			/* there is a chance the GC will already have executed at this point or other threads will re-win and re-execute.  loop until the
			 * thread sees that no more GCs are being requested.
			 */
			omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
			while (NULL != extensions->gcExclusiveAccessThreadId) {
				omrthread_monitor_wait(extensions->gcExclusiveAccessMutex);
			}
			omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);

			_envLanguageInterface->reacquireCriticalHeapAccess(accessMask);

			/* May have been beaten to a GC, but perhaps not the one we wanted.  Check and if in fact the collection we intended has been
			 * completed, we will not acquire exclusive access.
			 */
			if ((gcCount != extensions->globalGCStats.gcCount) || (CONCURRENT_INIT_COMPLETE != stats->getExecutionMode())) {
				return false;
			}
		}
	}

	Assert_MM_true(_omrVMThread == extensions->gcExclusiveAccessThreadId);
	Assert_MM_true(CONCURRENT_INIT_COMPLETE == stats->getExecutionMode());

	/* thread is the winner for requesting a GC (possibly through recursive calls).  proceed with acquiring exclusive access. */
	this->acquireExclusiveVMAccess();

	return true;
}

void
MM_EnvironmentBase::releaseExclusiveForConcurrentKickoff()
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);

	Assert_MM_true(extensions->gcExclusiveAccessThreadId ==_omrVMThread );
	Assert_MM_true(0 != _omrVMThread->exclusiveCount);

	_omrVMThread->exclusiveCount -= 1;
	if (0 == _omrVMThread->exclusiveCount) {
		omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
		extensions->gcExclusiveAccessThreadId = NULL;
		omrthread_monitor_notify_all(extensions->gcExclusiveAccessMutex);
		omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);

		reportExclusiveAccessRelease();
		_envLanguageInterface->releaseExclusiveVMAccess();
	}
}
#endif /* defined(OMR_GC_MODRON_CONCURRENT_MARK) */

uintptr_t
MM_EnvironmentBase::relinquishExclusiveVMAccess()
{
	return _envLanguageInterface->relinquishExclusiveVMAccess();
}

void
MM_EnvironmentBase::assumeExclusiveVMAccess(uintptr_t exclusiveCount)
{
	_envLanguageInterface->assumeExclusiveVMAccess(exclusiveCount);
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
