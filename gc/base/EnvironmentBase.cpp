/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

	return _delegate.initialize(this);
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

	_delegate.tearDown();
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
	_delegate.acquireVMAccess();
}

void
MM_EnvironmentBase::releaseVMAccess()
{
	_delegate.releaseVMAccess();
}

void
MM_EnvironmentBase::acquireExclusiveVMAccess()
{
	if (0 == _exclusiveCount) {
		_delegate.acquireExclusiveVMAccess();
		reportExclusiveAccessAcquire();
	}
	_exclusiveCount += 1;
}

void
MM_EnvironmentBase::releaseExclusiveVMAccess()
{
	_exclusiveCount -= 1;
	if (0 == _exclusiveCount) {
		reportExclusiveAccessRelease();
		_delegate.releaseExclusiveVMAccess();
	}
}

bool
MM_EnvironmentBase::isExclusiveAccessRequestWaiting()
{
	return _delegate.isExclusiveAccessRequestWaiting();
}

bool
MM_EnvironmentBase::acquireExclusiveVMAccessForGC(MM_Collector *collector, bool failIfNotFirst, bool flushCaches)
{
	MM_GCExtensionsBase *extensions = getExtensions();
	uintptr_t collectorAccessCount = collector->getExclusiveAccessCount();

	/* Does the current thread have exclusive vm access? */
	if (_omrVMThread->exclusiveCount > 0) {
		/* Did the current thread get exclusive vm access via (or already come through this code) the
		 * GC exclusive vm access APIs.
		 */
		if (_omrVMThread != extensions->gcExclusiveAccessThreadId) {
			/* The current thread did not get exclusive vm access via the GC exclusive vm access API */
			/* If another thread has started to get exclusive vm access for a GC cache that value so it
			 * can be restored later. It is possible for a thread to win setting itself as the thread
			 * requesting exclusive vm access for a GC but not actually be the next thread to perform
			 * a GC.  It can happen if a system gc is requested while holding exclusive.
			 */
			_cachedGCExclusiveAccessThreadId = (OMR_VMThread *)extensions->gcExclusiveAccessThreadId;
			/* Current thread has exclusive VM access so there is no reason to grab the mutex! */
			extensions->gcExclusiveAccessThreadId = _omrVMThread;
		}
	} else {
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

				uintptr_t accessMask;
				_delegate.releaseCriticalHeapAccess(&accessMask);

				/* there is a chance the GC will already have executed at this
				 * point or other threads will re-win and re-execute.  loop until
				 * the thread sees that no more GCs are being requested.
				 */
				omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
				while(NULL != extensions->gcExclusiveAccessThreadId) {
					omrthread_monitor_wait(extensions->gcExclusiveAccessMutex);
				}

				if (failIfNotFirst) {
					if(collector->getExclusiveAccessCount() != collectorAccessCount) {
						_exclusiveAccessBeatenByOtherThread = true;
						omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);
						_delegate.reacquireCriticalHeapAccess(accessMask);
						return false;
					}
				}

				/* thread can now win and will request a GC */
				extensions->gcExclusiveAccessThreadId = _omrVMThread;

				omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);

				_delegate.reacquireCriticalHeapAccess(accessMask);
			}
		}
	}

	_exclusiveAccessBeatenByOtherThread = !(collector->getExclusiveAccessCount() == collectorAccessCount);

	/* thread is the winner for requesting a GC (possibly through recursive
	 * calls).  proceed with acquiring exclusive access. */
	Assert_MM_true(_omrVMThread == extensions->gcExclusiveAccessThreadId);

	collector->notifyAcquireExclusiveVMAccess(this);

	acquireExclusiveVMAccess();

	collector->incrementExclusiveAccessCount();

	if (flushCaches) {
		GC_OMRVMInterface::flushCachesForGC(this);
	}

	return !_exclusiveAccessBeatenByOtherThread;

}

void
MM_EnvironmentBase::releaseExclusiveVMAccessForGC()
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);

	Assert_MM_true(extensions->gcExclusiveAccessThreadId == _omrVMThread);
	Assert_MM_true(0 != _exclusiveCount);

	_exclusiveCount -= 1;
	if (0 == _exclusiveCount) {
		omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
		extensions->gcExclusiveAccessThreadId = _cachedGCExclusiveAccessThreadId;
		_cachedGCExclusiveAccessThreadId = NULL;
		omrthread_monitor_notify_all(extensions->gcExclusiveAccessMutex);
		omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);
		reportExclusiveAccessRelease();
		_delegate.releaseExclusiveVMAccess();
	}
}

void
MM_EnvironmentBase::unwindExclusiveVMAccessForGC()
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(_omrVM);

	if (0 < _exclusiveCount) {
		Assert_MM_true(extensions->gcExclusiveAccessThreadId == _omrVMThread);

		_exclusiveCount = 0;

		omrthread_monitor_enter(extensions->gcExclusiveAccessMutex);
		extensions->gcExclusiveAccessThreadId = _cachedGCExclusiveAccessThreadId;
		_cachedGCExclusiveAccessThreadId = NULL;
		omrthread_monitor_notify_all(extensions->gcExclusiveAccessMutex);
		omrthread_monitor_exit(extensions->gcExclusiveAccessMutex);
		reportExclusiveAccessRelease();

		_delegate.releaseExclusiveVMAccess();
	}
}

uintptr_t
MM_EnvironmentBase::relinquishExclusiveVMAccess()
{
	return _delegate.relinquishExclusiveVMAccess();
}

void
MM_EnvironmentBase::assumeExclusiveVMAccess(uintptr_t exclusiveCount)
{
	_delegate.assumeExclusiveVMAccess(exclusiveCount);
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

bool
MM_EnvironmentBase::saveObjects(omrobjectptr_t objectPtr)
{
	void *heapBase = getExtensions()->heap->getHeapBase();
	void *heapTop = getExtensions()->heap->getHeapTop();

	Assert_MM_true((heapBase <= objectPtr) && (heapTop > objectPtr));
	Assert_MM_true((heapBase <= objectPtr) && (heapTop > objectPtr));
	Assert_MM_true(_omrVMThread->_savedObject1 != objectPtr);
	Assert_MM_true(_omrVMThread->_savedObject2 != objectPtr);

	if (NULL == _omrVMThread->_savedObject1) {
		_omrVMThread->_savedObject1 = objectPtr;
		return true;
	} else {
		Assert_MM_true((heapBase <= _omrVMThread->_savedObject1) && (heapTop > _omrVMThread->_savedObject1));
	}

	if (NULL == _omrVMThread->_savedObject2) {
		_omrVMThread->_savedObject2 = objectPtr;
		return true;
	} else {
		Assert_MM_true((heapBase <= _omrVMThread->_savedObject2) && (heapTop > _omrVMThread->_savedObject2));
	}

	Assert_MM_unreachable();
	return false;
}

void
MM_EnvironmentBase::restoreObjects(omrobjectptr_t *objectPtrIndirect)
{
	void *heapBase = getExtensions()->heap->getHeapBase();
	void *heapTop = getExtensions()->heap->getHeapTop();

	if (NULL != _omrVMThread->_savedObject2) {
		Assert_MM_true((heapBase <= _omrVMThread->_savedObject2) && (heapTop > _omrVMThread->_savedObject2));
		*objectPtrIndirect = (omrobjectptr_t)_omrVMThread->_savedObject2;
		_omrVMThread->_savedObject2 = NULL;
	} else if (NULL != _omrVMThread->_savedObject1) {
		Assert_MM_true((heapBase <= _omrVMThread->_savedObject1) && (heapTop > _omrVMThread->_savedObject1));
		*objectPtrIndirect = (omrobjectptr_t)_omrVMThread->_savedObject1;
		_omrVMThread->_savedObject1 = NULL;
	} else {
		Assert_MM_unreachable();
	}
}
