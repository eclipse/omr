/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
#include "omrmodroncore.h"
#include "gcutils.h"
#include "ModronAssertions.h"
#include "modronbase.h"
#include "modronopt.h"
#include "modronapicore.hpp"

#include "AllocateDescription.hpp"
#include "AllocationFailureStats.hpp"
#include "CollectionStatisticsStandard.hpp"
#include "CollectorLanguageInterface.hpp"
#if defined(OMR_GC_MODRON_COMPACTION)
#include "CompactScheme.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#include "Configuration.hpp"
#include "CycleState.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GlobalAllocationManager.hpp"
#include "Heap.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIteratorStandard.hpp"
#include "MarkingScheme.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "MemoryPoolLargeObjects.hpp"
#include "ObjectAllocationInterface.hpp"
#if defined(OMR_GC_OBJECT_MAP)
#include "ObjectMap.hpp"
#endif /* defined(OMR_GC_OBJECT_MAP) */
#include "OMRVMInterface.hpp"
#include "ObjectIterator.hpp"
#if defined(OMR_GC_MODRON_COMPACTION)
#include "ParallelCompactTask.hpp"
#endif /* OMR_GC_MODRON_COMPACTION */
#include "ParallelGlobalGC.hpp"
#include "ParallelMarkTask.hpp"
#include "ParallelSweepScheme.hpp"
#include "ParallelTask.hpp"
#if defined(OMR_GC_MODRON_SCAVENGER)
#include "Scavenger.hpp"
#endif /* OMR_GC_MODRON_SCAVENGER */
#include "WorkPackets.hpp"

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.
 */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"

#if defined(OMR_VALGRIND_MEMCHECK)
#include "MemcheckWrapper.hpp"
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
void
poisonReferenceSlot(MM_EnvironmentBase *env, GC_SlotObject *slotObject)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	uintptr_t heapBase = (uintptr_t)extensions->heap->getHeapBase();
	uintptr_t heapTop = (uintptr_t)extensions->heap->getHeapTop();
	uintptr_t referenceFromSlot = (uintptr_t)slotObject->readReferenceFromSlot();

	if ((heapTop > referenceFromSlot) && (heapBase <= referenceFromSlot)) {
		uintptr_t shadowHeapBase = (uintptr_t)extensions->shadowHeapBase;
		uintptr_t poisonedAddress = shadowHeapBase + (referenceFromSlot - heapBase);
		slotObject->writeReferenceToSlot((omrobjectptr_t)poisonedAddress);
	}
}

void
poisonReferenceSlots(OMR_VMThread *omrVMThread, MM_HeapRegionDescriptor *region, omrobjectptr_t object, void *userData)
{
	GC_ObjectIterator objectIterator(omrVMThread->_vm, object);
	GC_SlotObject *slotObject = NULL;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	while (NULL != (slotObject = objectIterator.nextSlot())) {
		poisonReferenceSlot(env, slotObject);
	}
}

void
healReferenceSlot(MM_EnvironmentBase *env, GC_SlotObject *slotObject)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	uintptr_t shadowHeapBase = (uintptr_t)extensions->shadowHeapBase;
	uintptr_t referenceFromSlot = (uintptr_t)slotObject->readReferenceFromSlot();
	uintptr_t shadowHeapTop = (uintptr_t)extensions->shadowHeapTop;

	if ((shadowHeapTop > referenceFromSlot) && (shadowHeapBase <= referenceFromSlot)) {
		uintptr_t heapBase = (uintptr_t)extensions->heap->getHeapBase();
		uintptr_t healedHeapAddress = heapBase + (referenceFromSlot - shadowHeapBase);
		/* This healing occurs at the start of GC, hence, does not need atomic write */
		slotObject->writeReferenceToSlot((omrobjectptr_t)healedHeapAddress);
	}
}

void
healReferenceSlots(OMR_VMThread *omrVMThread, MM_HeapRegionDescriptor *region, omrobjectptr_t object, void *userData)
{
		GC_ObjectIterator objectIterator(omrVMThread->_vm, object);
		GC_SlotObject *slotObject = NULL;

		MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

		while (NULL != (slotObject = objectIterator.nextSlot())) {
			healReferenceSlot(env, slotObject);
		}
}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

/* Define hook routines to be called on AF start and End */
static void globalGCHookAFCycleStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void globalGCHookAFCycleEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
static void globalGCHookCCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void globalGCHookCCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData); 
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
static void globalGCHookSysStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
static void globalGCHookSysEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);


/**
 * Function to fix single dead object on the heap
 *
 * NOTE that this function is expected to represent the SUPERSET of dead object fixup needs in that it will
 * fix all objects which are known to be dead.  Any optimizations to this function must be careful not to
 * violate that meaning (see CMVC 122959 for an example of such a mistake).
 */
static void
fixObject(OMR_VMThread *omrVMThread, MM_HeapRegionDescriptor *region, omrobjectptr_t object, void *userData)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	MM_ParallelGlobalGC *collector = (MM_ParallelGlobalGC *)extensions->getGlobalCollector();
	/* Check the mark state of each object. If it isn't marked, build a dead object */
	if( !collector->getMarkingScheme()->isMarked(object) ) {
		MM_MemorySubSpace *memorySubSpace = region->getSubSpace();
		uintptr_t deadObjectByteSize = extensions->objectModel.getConsumedSizeInBytesWithHeader(object);
#if defined(OMR_VALGRIND_MEMCHECK)
		/* Also clear dead object from valgrind pool
		 * This could have been done directly inside internalRecycleHeapChunk (MemoryPoolAddressOrderedListBase.hpp)
		 * but due to current limitation of API that requires address of object to be freed
		 * which we need to check from a set stored in extensions, which weren't further passed
		 * we will check it here. But in case new API is added, it is better to move there.
		*/
		if(valgrindCheckObjectInPool(extensions,(uintptr_t) object))
			valgrindFreeObject(extensions,(uintptr_t) object);
#endif /* defined(OMR_VALGRIND_MEMCHECK) */
		memorySubSpace->abandonHeapChunk(object, ((U_8*)object) + deadObjectByteSize);
		/* the userdata is a counter of dead objects fixed up so increment it here as a uintptr_t */
		*((uintptr_t *)userData) += 1;
	}
}

#if defined(OMR_GC_MODRON_SCAVENGER)
/**
 * Fix the heap if the remembered set for the scavenger is in an overflow state.
 */
static void
hookGlobalGcSweepEndRsoSafetyFixHeap(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent*)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();

	extensions->scavengerRsoScanUnsafe = !extensions->isRememberedSetInOverflowState();
	if (!extensions->scavengerRsoScanUnsafe) {
		MM_ParallelGlobalGC *pggc = (MM_ParallelGlobalGC *)userData;
		pggc->fixHeapForWalk(env, MEMORY_TYPE_OLD_RAM, FIXUP_DEBUG_TOOLING, fixObject);
	}
}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
static void
hookGlobalGcSweepEndAbortedCSFixHeap(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	MM_SweepEndEvent* event = (MM_SweepEndEvent*)eventData;
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(event->currentThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();
	uintptr_t holeCount = 0;

	Trc_MM_FixHeapForWalk_Entry(env->getLanguageVMThread(), MEMORY_TYPE_NEW);

	if (extensions->isScavengerBackOutFlagRaised()) {
		GC_HeapRegionIteratorStandard regionIterator(extensions->getHeap()->getHeapRegionManager());
		MM_HeapRegionDescriptorStandard *region = NULL;

		/* create holes between two marked objects */
		while(NULL != (region = regionIterator.nextRegion())) {
			if (MEMORY_TYPE_NEW == (region->getTypeFlags() & MEMORY_TYPE_NEW)) {
				void *lowAddress = region->getLowAddress();
				void *highAddress = region->getHighAddress();
				MM_HeapMapIterator markedObjectIterator(extensions, ((MM_ParallelGlobalGC *)extensions->getGlobalCollector())->getMarkingScheme()->getMarkMap(), (UDATA *)lowAddress, (UDATA *)highAddress);
				void * prevEndObjectPtr = lowAddress;
				omrobjectptr_t objectPtr = NULL;
				while (NULL != (objectPtr = markedObjectIterator.nextObject())) {
					UDATA objectSize = extensions->objectModel.getConsumedSizeInBytesWithHeader(objectPtr);
					if (prevEndObjectPtr != objectPtr) {
						region->getSubSpace()->abandonHeapChunk(prevEndObjectPtr, objectPtr);
						holeCount += 1;
					}
					prevEndObjectPtr = (void *)((U_8*)objectPtr + objectSize);
				}
				if (prevEndObjectPtr != highAddress) {
					region->getSubSpace()->abandonHeapChunk(prevEndObjectPtr, highAddress);
					holeCount += 1;
				}
			}
		}
	}
	Trc_MM_FixHeapForWalk_Exit(env->getLanguageVMThread(), holeCount);

}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#endif /* OMR_GC_MODRON_SCAVENGER */

/**
 * Initialization
 */
MM_ParallelGlobalGC *
MM_ParallelGlobalGC::newInstance(MM_EnvironmentBase *env)
{
	MM_ParallelGlobalGC *globalGC;
		
	globalGC = (MM_ParallelGlobalGC *)env->getForge()->allocate(sizeof(MM_ParallelGlobalGC), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (globalGC) {
		new(globalGC) MM_ParallelGlobalGC(env);
		if (!globalGC->initialize(env)) { 
			globalGC->kill(env);
			globalGC = NULL;
		}
	}
	return globalGC;
}

void
MM_ParallelGlobalGC::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the collector's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_ParallelGlobalGC::initialize(MM_EnvironmentBase *env)
{
	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);

	if (gc_policy_nogc == env->getExtensions()->configurationOptions._gcPolicy) {
		_cycleType = OMR_GC_CYCLE_TYPE_EPSILON;
		_disableGC = true;
	}

	_markingScheme = MM_MarkingScheme::newInstance(env);
	if (NULL == _markingScheme) {
		goto error_no_memory;
	}

	_delegate.initialize(env, this, _markingScheme);

	_sweepScheme = createSweepScheme(env, this);
	if (NULL == _sweepScheme) {
		goto error_no_memory;
	}

#if defined(OMR_GC_OBJECT_MAP)
	_extensions->setObjectMap(MM_ObjectMap::newInstance(env));
	if(NULL == _extensions->getObjectMap()) {
		goto error_no_memory;
	}
#endif /* defined(OMR_GC_OBJECT_MAP) */

#if defined(OMR_GC_MODRON_COMPACTION)
	_compactScheme = MM_CompactScheme::newInstance(env, _markingScheme);
	if(NULL == _compactScheme) {
		goto error_no_memory;
	}
#endif /* defined(OMR_GC_MODRON_COMPACTION) */

	_heapWalker = MM_ParallelHeapWalker::newInstance(this, _markingScheme->getMarkMap(), env);
	if (NULL == _heapWalker) {
		goto error_no_memory;
	}

	/* Attach to hooks required by the global collector's
	 * heap resize (expand/contraction) functions
	 */
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_START, globalGCHookAFCycleStart, OMR_GET_CALLSITE(), NULL);
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_ALLOCATION_FAILURE_CYCLE_END, globalGCHookAFCycleEnd, OMR_GET_CALLSITE(), NULL);
	 
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START, globalGCHookCCStart, OMR_GET_CALLSITE(), NULL);
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END, globalGCHookCCEnd, OMR_GET_CALLSITE(), NULL);
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_START, globalGCHookSysStart, OMR_GET_CALLSITE(), NULL);
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_SYSTEM_GC_END, globalGCHookSysEnd, OMR_GET_CALLSITE(), NULL);

#if defined(OMR_GC_MODRON_SCAVENGER)
	if (_extensions->scavengerEnabled) {
		/* Hook the global collector to guarantee heap walk safety in the event of an RSO */
		/* NOTE: This will be a one time hook across all instances as the function and user data will be identical - i.e.,
		 * we will only get one Hook registered no matter how many scavengers are created/initialized
		 */
		(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, hookGlobalGcSweepEndRsoSafetyFixHeap, OMR_GET_CALLSITE(), this);
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		if (_extensions->isConcurrentScavengerEnabled()) {
			(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_SWEEP_END, hookGlobalGcSweepEndAbortedCSFixHeap, OMR_GET_CALLSITE(), this);
		}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	}
#endif /* OMR_GC_MODRON_SCAVENGER */

	return true;
	
error_no_memory:
	return false;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_ParallelGlobalGC::tearDown(MM_EnvironmentBase *env)
{
	_delegate.tearDown(env);

	if(NULL != _markingScheme) {
		_markingScheme->kill(env);
		_markingScheme = NULL;
	}

	if(NULL != _sweepScheme) {
		_sweepScheme->kill(env);
		_sweepScheme = NULL;
	}

#if defined(OMR_GC_MODRON_COMPACTION)
	if(NULL != _compactScheme) {
		_compactScheme->kill(env);
		_compactScheme = NULL;
	}
#endif /* OMR_GC_MODRON_COMPACTION */

	if (NULL != _heapWalker) {
		_heapWalker->kill(env);
		_heapWalker = NULL;
	}
}

uintptr_t
MM_ParallelGlobalGC::getVMStateID()
{
	return OMRVMSTATE_GC_COLLECTOR_GLOBALGC;
}

/****************************************
 * Thread work routines
 ****************************************
 */
void
MM_ParallelGlobalGC::cleanupAfterGC(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	updateTuningStatistics(env);

	/* Perform the resize now. The decision was earlier */
	env->_cycleState->_activeSubSpace->performResize(env, allocDescription);

	/* Heap size now fixed for next cycle so reset heap statistics */
	_extensions->heap->resetHeapStatistics(true);

#if defined(OMR_GC_MODRON_SCAVENGER)
	GC_OMRVMThreadListIterator threadIterator(_extensions->getOmrVM());
	OMR_VMThread *walkThread = NULL;

	/* Null tenure TLH (Copy Cache) references for all GC slave and Mutator (for concurrent scavenger) threads as the memory will be invalidated on sweep cycle*/
	while((walkThread = threadIterator.nextOMRVMThread()) != NULL) {
		MM_EnvironmentStandard *threadEnvironment = MM_EnvironmentStandard::getEnvironment(walkThread);
		threadEnvironment->_tenureTLHRemainderBase = NULL;
		threadEnvironment->_tenureTLHRemainderTop = NULL;
	}

	_extensions->_masterThreadTenureTLHRemainderTop = NULL;
	_extensions->_masterThreadTenureTLHRemainderBase = NULL;
#endif /* OMR_GC_MODRON_SCAVENGER */
}

void
MM_ParallelGlobalGC::masterThreadGarbageCollect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool initMarkMap, bool rebuildMarkBits)
{
	if (_extensions->trackMutatorThreadCategory) {
		/* This thread is doing GC work, account for the time spent into the GC bucket */
		omrthread_set_category(env->getOmrVMThread()->_os_thread, J9THREAD_CATEGORY_SYSTEM_GC_THREAD, J9THREAD_TYPE_SET_GC);
	}

	/* Perform any master-specific setup */
	/* Tell the GAM to flush its contexts */
	MM_GlobalAllocationManager *gam = _extensions->globalAllocationManager;
	if (NULL != gam) {
		gam->flushAllocationContexts(env);
	}

	/* ----- start of setupForCollect ------*/

	/* ensure heap base is aligned to region size */
	uintptr_t heapBase = (uintptr_t)_extensions->heap->getHeapBase();
	uintptr_t regionSize = _extensions->regionSize;
	Assert_MM_true((0 != regionSize) && (0 == (heapBase % regionSize)));

	/* Reset memory pools of associated memory spaces */
	_extensions->heap->resetSpacesForGarbageCollect(env);
	
	/* Clear the gc stats structure */
	_extensions->globalGCStats.clear();

#if defined(OMR_GC_MODRON_COMPACTION)
	_compactThisCycle = false;
#endif /* OMR_GC_MODRON_COMPACTION */

	_fixHeapForWalkCompleted = false;

	_delegate.masterThreadGarbageCollectStarted(env);

	/* ----- end of setupForCollect ------*/
	
	/* Run a garbage collect */

	/* Mark */	
	markAll(env, initMarkMap);

	_delegate.postMarkProcessing(env);
	
	sweep(env, allocDescription, rebuildMarkBits);


#if defined(OMR_GC_MODRON_COMPACTION)
	/* If a compaction was required, then do one */
	if (_compactThisCycle) {
		_collectionStatistics._tenureFragmentation = MICRO_FRAGMENTATION;
		if (GLOBALGC_ESTIMATE_FRAGMENTATION == (_extensions->estimateFragmentation & GLOBALGC_ESTIMATE_FRAGMENTATION)) {
			_collectionStatistics._tenureFragmentation |= MACRO_FRAGMENTATION;
		}

		masterThreadCompact(env, allocDescription, rebuildMarkBits);
		_collectionStatistics._tenureFragmentation = NO_FRAGMENTATION;
		if (_extensions->processLargeAllocateStats) {
			processLargeAllocateStatsAfterCompact(env);
		}
	} else {
		/* If a compaction was prevented, report the reason */
		CompactPreventedReason compactPreventedReason = (CompactPreventedReason)(_extensions->globalGCStats.compactStats._compactPreventedReason);
		if(COMPACT_PREVENTED_NONE != compactPreventedReason) {
			MM_CompactStats *compactStats = &_extensions->globalGCStats.compactStats;
			reportCompactStart(env);
			Trc_MM_CompactPrevented(env->getLanguageVMThread(), getCompactionPreventedReasonAsString(compactPreventedReason));
			compactStats->_startTime = 0;
			compactStats->_endTime = 0;
			reportCompactEnd(env);
		}
		_collectionStatistics._tenureFragmentation = MICRO_FRAGMENTATION;
		if (GLOBALGC_ESTIMATE_FRAGMENTATION == (_extensions->estimateFragmentation & GLOBALGC_ESTIMATE_FRAGMENTATION)) {
			_collectionStatistics._tenureFragmentation |= MACRO_FRAGMENTATION;
		}
	}
#endif /* defined(OMR_GC_MODRON_COMPACTION) */	

	bool compactedThisCycle = false;
#if defined(OMR_GC_MODRON_COMPACTION)
	compactedThisCycle = _compactThisCycle;
#endif /* OMR_GC_MODRON_COMPACTION */

	/* If the delegate has isAllowUserHeapWalk set, fix the heap so that it can be walked */
	if (_delegate.isAllowUserHeapWalk() || env->_cycleState->_gcCode.isRASDumpGC()) {
		if (!_fixHeapForWalkCompleted) {
#if defined(OMR_GC_MODRON_COMPACTION)
			if (compactedThisCycle) {
				OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
				U_64 startTime = omrtime_hires_clock();
				getCompactScheme(env)->fixHeapForWalk(env);
				_extensions->globalGCStats.fixHeapForWalkTime = omrtime_hires_delta(startTime, omrtime_hires_clock(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
				_extensions->globalGCStats.fixHeapForWalkReason = FIXUP_DEBUG_TOOLING;
			} else
#endif /* OMR_GC_MODRON_COMPACTION */
			{
				fixHeapForWalk(env, MEMORY_TYPE_RAM, FIXUP_DEBUG_TOOLING, fixObject);
			}
			/* since this is the superset of all walk operations, we can safely set the flag that states other walks
			 * can be omitted for this cycle as redundant (CMVC 122959)
			 */
			_fixHeapForWalkCompleted = true;
		}
	}

	_delegate.masterThreadGarbageCollectFinished(env, compactedThisCycle);

#if defined(OMR_GC_MODRON_COMPACTION)
	if (compactedThisCycle) {
		/* Free space will have changed as a result of compaction so recalculate
		 * any expand or contract target.
		 * Concurrent Scavenger requires this be done after fixup heap for walk pass.
		*/
		env->_cycleState->_activeSubSpace->checkResize(env, allocDescription, env->_cycleState->_gcCode.isExplicitGC());
	}
#endif
	
#if defined(OMR_GC_MODRON_SCAVENGER)
	/* Merge sublists in the remembered set (if necessary) */
	_extensions->rememberedSet.compact(env);

	_extensions->oldHeapSizeOnLastGlobalGC = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD);
	_extensions->freeOldHeapSizeOnLastGlobalGC = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
#endif /* OMR_GC_MODRON_SCAVENGER */
	
	/* Restart the allocation caches associated to all threads */
	masterThreadRestartAllocationCaches(env);
	
	/* ----- start of cleanupAfterCollect ------*/

	reportGlobalGCCollectComplete(env);
	
	cleanupAfterGC(env, allocDescription);

	if (_extensions->trackMutatorThreadCategory) {
		/* Done doing GC, reset the category back to the old one */
		omrthread_set_category(env->getOmrVMThread()->_os_thread, 0, J9THREAD_TYPE_SET_GC);
	}
}

#if defined(OMR_GC_MODRON_COMPACTION)
bool
MM_ParallelGlobalGC::shouldCompactThisCycle(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t activeSubspaceMaxExpansionInSpace, MM_GCCode gcCode) 
{
	MM_AllocationStats *allocStats = &_extensions->allocationStats;
	CompactReason compactReason = COMPACT_NONE;
	CompactPreventedReason compactPreventedReason = COMPACT_PREVENTED_NONE;
	uintptr_t tlhPercent, totalBytesAllocated;

	/* Assume no compaction is required until we prove otherwise*/
	/* If user has specified -XnoCompact then were done */
	if(_extensions->noCompactOnGlobalGC) {
		compactReason = COMPACT_NONE;
		goto nocompact;
	}	

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	if((_extensions->compactOnIdle) && (J9MMCONSTANT_EXPLICIT_GC_IDLE_GC == gcCode.getCode())) {
		compactReason = COMPACT_FORCED_GC;
		goto compactionReqd;
	}
#endif
	
	/* RAS dump compact requests override all other options. If a dump agent requested 
	 * a compact we always honour it in order to produce optimal heap dumps
	 */
	if (J9MMCONSTANT_EXPLICIT_GC_RASDUMP_COMPACT == gcCode.getCode()) {
		compactReason = COMPACT_FORCED_GC;
		goto compactionReqd;
	}	
	
	/* If user has specified -XCompact then we compact every time */
	if(_extensions->compactOnGlobalGC) {
		compactReason = COMPACT_ALWAYS;
		goto compactionReqd;
	}

	/* Aborted CS needs global GC with Nursery compaction */
	if (_extensions->isConcurrentScavengerEnabled() && _extensions->isScavengerBackOutFlagRaised()) {
		compactReason = COMPACT_ABORTED_SCAVENGE;
		goto compactionReqd;
	}	

	/* Is this a system GC ? */ 
	if(gcCode.isExplicitGC()) { 
		/* If the user as specified -XcompactexplicitGC then compact*/
		if(_extensions->compactOnSystemGC){
			compactReason = COMPACT_FORCED_GC;
			goto compactionReqd;
		} else if(_extensions->nocompactOnSystemGC){
			compactReason = COMPACT_NONE;
			/* The user as specified -XnocompactexplicitGC then we don't compact*/
			goto nocompact;
		}		
	}

	/* Has GC found enough storage to satisfy the allocation request ? */
	if(allocDescription){
		MM_MemorySpace *memorySpace = env->getMemorySpace();
		 
		uintptr_t largestFreeEntry = memorySpace->findLargestFreeEntry(env, allocDescription);
		uintptr_t bytesRequested = allocDescription->getBytesRequested();
		
		if(bytesRequested > largestFreeEntry){
			compactReason = COMPACT_LARGE;
			goto compactionReqd;
		}
	}
	
	/* If -Xgc:compactToSatisfyAllocate has been specified, then we skip the rest of the triggers */
	if (_extensions->compactToSatisfyAllocate) {
		compactReason = COMPACT_NONE;
		goto nocompact;
	}
	
#if defined(OMR_GC_MODRON_SCAVENGER)
	if (_extensions->scavengerEnabled) {
		/* Get size of largest object we failed to tenure on last scavenege */
		uintptr_t failedTenureLargest = _extensions->scavengerStats._failedTenureLargest;
		if (failedTenureLargest > 0) { 
			MM_MemorySpace *memorySpace = env->getMemorySpace();
			MM_AllocateDescription tenureAllocDescription(failedTenureLargest, OMR_GC_ALLOCATE_OBJECT_TENURED, false, true);
			uintptr_t largestTenureFreeEntry = memorySpace->findLargestFreeEntry(env, &tenureAllocDescription);
			
			if(failedTenureLargest > largestTenureFreeEntry){
				compactReason = COMPACT_LARGE;
				goto compactionReqd;
			}
		}	
	}	
#endif /* OMR_GC_MODRON_SCAVENGER */

	/* If this is an aggressive collect and the last collect did not compact then 
	 * make sure we do this time.
	 */
	if (gcCode.isAggressiveGC() && 
		(_extensions->globalGCStats.compactStats._lastHeapCompaction + 1 <  _extensions->globalGCStats.gcCount)) {
		compactReason = COMPACT_AGGRESSIVE;
		goto compactionReqd;
	}
	
#if defined(OMR_GC_THREAD_LOCAL_HEAP)	

	/* Now check for signs of fragmentation by checking the average size of TLH
	 * allocated since the last global collection
	 */

	if (allocStats->_tlhRefreshCountFresh > 0) {
		Assert_MM_true(allocStats->_tlhAllocatedFresh > 0);
	}

	/* Calculate total bytes allocated in tenure area since last global collection */
	totalBytesAllocated = allocStats->_allocationBytes + allocStats->_tlhAllocatedFresh;

	/* ..and what percentage of allocations were tlh's */
	tlhPercent = allocStats->_tlhRefreshCountFresh > 0 ? (uintptr_t) (((uint64_t) allocStats->_tlhAllocatedFresh * 100) / (uint64_t) totalBytesAllocated) : 0;
	
	/* Check at least 50% of free space at end of last GC has been consumed by tlh allocations */
	if( tlhPercent > 50 ) {
		/* Calculate average size of tlh allocated since tenure area last collected */
		uintptr_t avgTlh= allocStats->_tlhAllocatedFresh / allocStats->_tlhRefreshCountFresh;
		
		/* Compaction trigger is a multiple of the minimum tlh size */
		uintptr_t compaction_trigger_avgtlh= _extensions->tlhMinimumSize * MINIMUM_TLHSIZE_MULTIPLIER;
		if(avgTlh < compaction_trigger_avgtlh) {
			compactReason = COMPACT_FRAGMENTED;
			goto compactionReqd;
		}
	}
#endif

	/* We know if we get here we know:
	 * 	o we can meet the allocation request out of available free memory, and 
	 *  o the heap is not fragmented 
	 * 
	 * So if we are not fully expanded but we are low on storage there is little point doing 
	 * a compaction, we just need to expand. However, if we are fully expanded but low 
	 * on free memory there is little more we can do to avoid OOM at this point other than 
	 * compact to get as much free memory as possible.
	 */
	if ( activeSubspaceMaxExpansionInSpace == 0) { 
		uintptr_t oldFree = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
		uintptr_t oldSize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD);
		uintptr_t desperateFree = (oldSize  / OLDFREE_DESPERATE_RATIO_DIVISOR) * OLDFREE_DESPERATE_RATIO_MULTIPLIER;
			
		/* Is heap space getting tight? */
		if(oldFree < desperateFree) { 
			compactReason = COMPACT_AVOID_DESPERATE;
			goto compactionReqd;
		}
		
		if(oldFree < OLDFREE_INSUFFICIENT ) { 
			compactReason = COMPACT_MEMORY_INSUFFICIENT;
			goto compactionReqd;
		}
	}	

	{
		/* Tenure space dark matter trigger */
		MM_MemorySubSpace *memorySubSpace = _extensions->heap->getDefaultMemorySpace()->getTenureMemorySubSpace();
		uintptr_t totalSize = memorySubSpace->getActiveMemorySize();
		MM_MemoryPool *memoryPool= memorySubSpace->getMemoryPool();
		uintptr_t darkMatterBytes = 0;
		if (!_extensions->concurrentSweep) {
			darkMatterBytes = memoryPool->getDarkMatterBytes();
		}
		uintptr_t freeMemorySize = memoryPool->getActualFreeMemorySize();
		float darkMatterRatio = ((float)darkMatterBytes)/((float)freeMemorySize + (float)totalSize / 2);

		if (darkMatterRatio > _extensions->getDarkMatterCompactThreshold()) {
			compactReason = COMPACT_MICRO_FRAG;
			goto compactionReqd;
		}

#if defined(OMR_GC_IDLE_HEAP_MANAGER) 
		if ((J9MMCONSTANT_EXPLICIT_GC_IDLE_GC == gcCode.getCode()) && (_extensions->gcOnIdle)){

			MM_LargeObjectAllocateStats *stats = memoryPool->getLargeObjectAllocateStats();

			uintptr_t pageSize = env->getExtensions()->heap->getPageSize();
			uintptr_t reusableFreeMemory = stats->getPageAlignedFreeMemory(pageSize);

			uintptr_t memoryFragmentationDiff = freeMemorySize - reusableFreeMemory;
			uintptr_t totalFragmentation = memoryFragmentationDiff + darkMatterBytes;
			float totalFragmentationRatio = ((float)totalFragmentation)/((float)freeMemorySize + (float)totalSize / 2);

			Trc_ParallelGlobalGC_shouldCompactThisCycle(env->getLanguageVMThread(), totalFragmentationRatio, _extensions->gcOnIdleCompactThreshold);

			if (totalFragmentationRatio > _extensions->gcOnIdleCompactThreshold) {
				compactReason = COMPACT_PAGE;
				goto compactionReqd;
			}
		}
#endif /* OMR_GC_IDLE_HEAP_MANAGER */
	}

	
nocompact:	
	/* Compaction not required or prevented from running */
	_extensions->globalGCStats.compactStats._compactReason = compactReason;
	_extensions->globalGCStats.compactStats._compactPreventedReason = compactPreventedReason;
	return false;
	
compactionReqd:
	compactPreventedReason = _delegate.checkIfCompactionShouldBePrevented(env);
	if (COMPACT_PREVENTED_NONE != compactPreventedReason) {
		goto nocompact;
	}

	_extensions->globalGCStats.compactStats._compactReason = compactReason;
	_extensions->globalGCStats.compactStats._compactPreventedReason = compactPreventedReason;

	return true;
}

/**
 * Determine if a compact is required to aid contraction.
 * A heap contraction is due so decide whether a compaction would be
 * beneficial before we attempt to contract the heap.
 * @return true if a compaction is required, false otherwise.
 */
bool
MM_ParallelGlobalGC::compactRequiredBeforeHeapContraction(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t contractionSize)
{
	uintptr_t lengthLastFree;

	/* Assume no compaction is required until we prove otherwise*/
	
	/* if the user specified -XnoCompact then we're done */
	if (_extensions->noCompactOnGlobalGC) {
		return false;
	}	
	
	if (env->_cycleState->_gcCode.isExplicitGC() && _extensions->nocompactOnSystemGC){
		/* if the user specified -XnocompactexplicitGC then we don't compact*/
		return false;
	}

	uintptr_t actualSoftMx = _extensions->heap->getActualSoftMxSize(env);

	if(0 != actualSoftMx) {
		if(actualSoftMx < _extensions->heap->getActiveMemorySize()) {
			/* a softmx has been set that's less than the current heap size - it's 
			 * highly likely we'll need to compact in order to meet the contract, so do it */
			goto compactionReqd;
		}
	}
 
	/* We should compact to assist a later contraction if:
	 * 
	 * - no compaction performed this GC (if we get here then we have decided not to compact for other
	 *   reasons so non eed to check that)
	 * - the last GC did not do a shrink and compact 
	 * - there is no free chunk at the end of the heap or the chunk at the end of the heap
	 * 	 is less than 10% of required shrink size 
	 */
	if((_extensions->globalGCStats.compactStats._lastHeapCompaction + 1 ==  _extensions->globalGCStats.gcCount) &&
	(_extensions->heap->getResizeStats()->getLastHeapContractionGCCount() + 1 ==  _extensions->globalGCStats.gcCount)) {
		return false;
	}

	/* Determine length of free chunk at top of heap */
	/* Note: We know based on the collector that this is a single contiguous area */
	lengthLastFree = env->_cycleState->_activeSubSpace->getAvailableContractionSize(env, allocDescription);
	
	/* If chunk at end of heap is free then check its at least 10% of 
	 * requested contraction amount
	 */
	if (lengthLastFree > 0 ) {
		uintptr_t minContractSize = (contractionSize / MINIMUM_CONTRACTION_RATIO_DIVISOR)
								 * MINIMUM_CONTRACTION_RATIO_MULTIPLIER;
								 
		if (lengthLastFree > minContractSize ) {						 
			return false;
		}	
	}

compactionReqd:
	_extensions->globalGCStats.compactStats._compactPreventedReason = _delegate.checkIfCompactionShouldBePrevented(env);
	if (COMPACT_PREVENTED_NONE != _extensions->globalGCStats.compactStats._compactPreventedReason) {
		return false;
	}

	/* If we get here we need to compact to assist the contraction. 
	 * Remember why for verbose
	 */
	_extensions->globalGCStats.compactStats._compactReason = COMPACT_CONTRACT;
	
	return true;
}
#endif /* defined(OMR_GC_MODRON_COMPACTION) */

void 
MM_ParallelGlobalGC::sweep(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool rebuildMarkBits)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_SweepStats *sweepStats = &_extensions->globalGCStats.sweepStats;

	reportSweepStart(env);
	sweepStats->_startTime = omrtime_hires_clock();
	masterThreadSweepStart(env, allocDescription);

	if (_extensions->processLargeAllocateStats) {
		processLargeAllocateStatsAfterSweep(env);
	}

	MM_MemorySubSpace *activeSubSpace = env->_cycleState->_activeSubSpace;
	bool isExplicitGC = env->_cycleState->_gcCode.isExplicitGC();
#if defined(OMR_GC_MODRON_COMPACTION)
	/* Decide is a compaction is required - this decision must be made after we sweep since we use the largestFreeEntrySize, as changed by sweep, to determine if a compaction should be done */
	_compactThisCycle = shouldCompactThisCycle(env, allocDescription, activeSubSpace->maxExpansionInSpace(env), env->_cycleState->_gcCode);

	if (!_compactThisCycle)  
#endif /* OMR_GC_MODRON_COMPACTION */		
	{
		/* Decide whether we need to expand or shrink the heap. If the decision is to 
		 * shrink, we may need to force a compaction to assist the contraction.
 		*/ 
		activeSubSpace->checkResize(env, allocDescription, isExplicitGC);
	}	
			
	/* If we need to completely rebuild the freeelist (impending compaction or contraction), then do it */
	SweepCompletionReason reason = NOT_REQUIRED;
	if(completeFreelistRebuildRequired(env, &reason)) {
		masterThreadSweepComplete(env, reason);
			
#if defined(OMR_GC_MODRON_COMPACTION)
		if (!_compactThisCycle)  
#endif /* OMR_GC_MODRON_COMPACTION */		
		{
			/* We now have accurate free space statistics so recalculate any expand/contract amount
		 	 * as it will no doubt have changed
		 	*/ 
			activeSubSpace->checkResize(env, allocDescription, isExplicitGC);
		} 
	}	
				
#if defined(OMR_GC_MODRON_COMPACTION)
	if (0 != activeSubSpace->getContractionSize()) {
		_compactThisCycle = compactRequiredBeforeHeapContraction(env, allocDescription, activeSubSpace->getContractionSize());
	}
#endif /* OMR_GC_MODRON_COMPACTION */

	sweepStats->_endTime = omrtime_hires_clock();
	reportSweepEnd(env);
}

bool
MM_ParallelGlobalGC::completeFreelistRebuildRequired(MM_EnvironmentBase *env, SweepCompletionReason *reason)
{
	*reason = NOT_REQUIRED;
	
	MM_MemorySubSpace *activeSubSpace = env->_cycleState->_activeSubSpace;
#if defined(OMR_GC_MODRON_COMPACTION)	
	if (_compactThisCycle) { 
		*reason = COMPACTION_REQUIRED;
	} else 
#endif 	/* OMR_GC_MODRON_COMPACTION */
	if (activeSubSpace->getActiveLOAMemorySize(MEMORY_TYPE_OLD) > 0  && 0 != activeSubSpace->getExpansionSize()) {
		//todo remove once we sort out how to reallocate sweep chunks if heap expands
		// after a concurrent sweep cycle has started but for now we need to complete sweep
		// if current LOA size is > 0.
		*reason = EXPANSION_REQUIRED;
	} else if (0 != activeSubSpace->getContractionSize()) {
		*reason = CONTRACTION_REQUIRED;
	} else if (activeSubSpace->completeFreelistRebuildRequired(env)) {
		*reason = LOA_RESIZE;
	} else if (env->_cycleState->_gcCode.isExplicitGC()) {
		*reason = SYSTEM_GC;
	}
	
	return (*reason == NOT_REQUIRED ? false : true);
}

void
MM_ParallelGlobalGC::markAll(MM_EnvironmentBase *env, bool initMarkMap)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_MarkStats *markStats = &_extensions->globalGCStats.markStats;

	reportMarkStart(env);
	markStats->_startTime = omrtime_hires_clock();

	_markingScheme->masterSetupForGC(env);

	if (env->_cycleState->_gcCode.isOutOfMemoryGC()) {
		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_soft_as_weak;
	}

	/* run the mark */
	MM_ParallelMarkTask markTask(env, _dispatcher, _markingScheme, initMarkMap, env->_cycleState);
	_dispatcher->run(env, &markTask);
	
	Assert_MM_true(_markingScheme->getWorkPackets()->isAllPacketsEmpty());

	/* Do any post mark checks */
	postMark(env);
	_markingScheme->masterCleanupAfterGC(env);
	markStats->_endTime = omrtime_hires_clock();
	reportMarkEnd(env);
}

void
MM_ParallelGlobalGC::masterThreadSweepStart(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	_sweepScheme->setMarkMap(_markingScheme->getMarkMap());
	_sweepScheme->sweepForMinimumSize(env, env->_cycleState->_activeSubSpace,  allocDescription);
}

void
MM_ParallelGlobalGC::masterThreadSweepComplete(MM_EnvironmentBase *env, SweepCompletionReason reason)
{
	_sweepScheme->completeSweep(env, reason);
}
	
#if defined(OMR_GC_MODRON_COMPACTION)
void
MM_ParallelGlobalGC::masterThreadCompact(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool rebuildMarkBits)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_CompactStats *compactStats = &_extensions->globalGCStats.compactStats;
	MM_MarkMap *markMap = _markingScheme->getMarkMap();

	markMap->setMarkMapValid(false);
	_compactScheme->setMarkMap(markMap);

	reportCompactStart(env);
	compactStats->_startTime = omrtime_hires_clock();
	MM_ParallelCompactTask compactTask(env, _dispatcher, _compactScheme, rebuildMarkBits, env->_cycleState->_gcCode.shouldAggressivelyCompact());
	_dispatcher->run(env, &compactTask);
	compactStats->_endTime = omrtime_hires_clock();
	reportCompactEnd(env);
	
	/* Remember the gc count of the last compaction */ 
	_extensions->globalGCStats.compactStats._lastHeapCompaction= _extensions->globalGCStats.gcCount;
}
#endif /* OMR_GC_MODRON_COMPACTION */

void
MM_ParallelGlobalGC::masterThreadRestartAllocationCaches(MM_EnvironmentBase *env)
{
	GC_OMRVMThreadListIterator vmThreadListIterator(env->getOmrVMThread());
	OMR_VMThread *walkThread;
	while((walkThread = vmThreadListIterator.nextOMRVMThread()) != NULL) {
		MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkThread);
		/* CMVC 123281: setThreadScanned(false) is called here because the concurrent collector
		 * uses this as the reset point for all threads scanned state. This should really be moved
		 * into the STW thread scan phase.
		 */  
		walkEnv->setThreadScanned(false);

		walkEnv->_objectAllocationInterface->restartCache(env);
	}
}



/****************************************
 * VM Garbage Collection API
 ****************************************
 */
void
MM_ParallelGlobalGC::internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode)
{
#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	if (!env->compressObjectReferences()) {
		if (1 == _extensions->fvtest_enableReadBarrierVerification) {
			/* heal the roots slots that were not read/healed*/
			_delegate.healSlots(env);
			/* heal the heap object slots that were not read/healed*/
			healHeap(env);
		}
	}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

	_cycleState = MM_CycleState();
	env->_cycleState = &_cycleState;
	env->_cycleState->_gcCode = MM_GCCode(gcCode);
	env->_cycleState->_type = _cycleType;
	env->_cycleState->_activeSubSpace = subSpace;
	env->_cycleState->_collectionStatistics = &_collectionStatistics;

	/* If we are in an excessiveGC level beyond normal then an aggressive GC is
	 * conducted to free up as much space as possible
	 */
	if (!env->_cycleState->_gcCode.isExplicitGC()) {
		if(excessive_gc_normal != _extensions->excessiveGCLevel) {
			/* convert the current mode to excessive GC mode */
			env->_cycleState->_gcCode = MM_GCCode(J9MMCONSTANT_IMPLICIT_GC_EXCESSIVE);
		}
	}

	GC_OMRVMInterface::flushCachesForGC(env);
	
	_markingScheme->getMarkMap()->setMarkMapValid(false);
	
	if (_extensions->processLargeAllocateStats) {
		processLargeAllocateStatsBeforeGC(env);
	}

	reportGCCycleStart(env);
	reportGCStart(env);
	reportGCIncrementStart(env);
	reportGlobalGCIncrementStart(env);
	return;
}

void
MM_ParallelGlobalGC::tenureMemoryPoolPostCollect(MM_EnvironmentBase *env)
{
	MM_MemorySpace *defaultMemorySpace = _extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();

	if (_extensions->largeObjectArea && _sweepScheme->isSweepCompleted(env)) {
		/* resize LOA only when the sweep is completed (to avoid concurrent sweep's confusion due to the resize) */
		MM_MemoryPoolLargeObjects *memoryPool = (MM_MemoryPoolLargeObjects *) tenureMemorySubspace->getMemoryPool();
		memoryPool->resizeLOA(env);
	}
}

void
MM_ParallelGlobalGC::internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	MM_GlobalCollector::internalPostCollect(env, subSpace);

	tenureMemoryPoolPostCollect(env);

	reportGCCycleFinalIncrementEnding(env);
	reportGlobalGCIncrementEnd(env);
	reportGCIncrementEnd(env);
	reportGCEnd(env);
	reportGCCycleEnd(env);

	_markingScheme->getMarkMap()->setMarkMapValid(false);

#if defined(OMR_GC_OBJECT_MAP)
	/* Swap the the mark maps used by the ObjectMap and MarkingScheme. */
	MM_ObjectMap *objectMap = _extensions->getObjectMap();
	MM_MarkMap *markMap = _markingScheme->getMarkMap();
	_markingScheme->setMarkMap(objectMap->getObjectMap());
	objectMap->setMarkMap(markMap);
#endif

	env->_cycleState->_activeSubSpace = NULL;

	/* Clear overflow flag regardless */
	_extensions->globalGCStats.workPacketStats.setSTWWorkStackOverflowOccured(false);
	_extensions->allocationStats.clear();
	_extensions->setLastGlobalGCFreeBytes(_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD));
#if defined(OMR_GC_LARGE_OBJECT_AREA)
	_extensions->lastGlobalGCFreeBytesLOA = _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD); 
#endif /* defined (OMR_GC_LARGE_OBJECT_AREA) */


#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
	if (!env->compressObjectReferences()) {
		if (1 == _extensions->fvtest_enableReadBarrierVerification) {
			/* poison root slots */
			_delegate.poisonSlots(env);
			/* poison heap object slots */
			poisonHeap(env);
		}
	}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */

}

void
MM_ParallelGlobalGC::processLargeAllocateStatsBeforeGC(MM_EnvironmentBase *env)
{
	MM_MemorySpace *defaultMemorySpace = _extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *defaultMemorySubspace = defaultMemorySpace->getDefaultMemorySubSpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
	MM_MemoryPool *memoryPool = tenureMemorySubspace->getMemoryPool();

	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();

	/* merge and average largeObjectAllocateStats in tenure space */
	memoryPool->mergeLargeObjectAllocateStats();
	memoryPool->mergeTlhAllocateStats();

	/* TODO: average LargeObjectAllocateStats for allocation form mutators in ScavengerEnabled case */
#if defined(OMR_GC_MODRON_SCAVENGER)
	if (!_extensions->scavengerEnabled) {
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
		memoryPool->averageLargeObjectAllocateStats(env, _extensions->allocationStats.bytesAllocated());
#if defined(OMR_GC_MODRON_SCAVENGER)
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	memoryPool->getLargeObjectAllocateStats()->setTimeMergeAverage(omrtime_hires_clock() - startTime);

	/* merge largeObjectAllocateStats in nursery space */
	if (defaultMemorySubspace->isPartOfSemiSpace()) {
		/* SemiSpace stats include only Mutator stats (no Collector stats during flipping) */
		defaultMemorySubspace->getTopLevelMemorySubSpace(MEMORY_TYPE_NEW)->mergeLargeObjectAllocateStats(env);
	}
}

void
MM_ParallelGlobalGC::processLargeAllocateStatsAfterCompact(MM_EnvironmentBase *env)
{
	processLargeAllocateStatsAfterSweep(env);
}


void
MM_ParallelGlobalGC::processLargeAllocateStatsAfterSweep(MM_EnvironmentBase *env)
{
	MM_MemorySpace *defaultMemorySpace = _extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
	MM_MemoryPool *memoryPool = tenureMemorySubspace->getMemoryPool();
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	/* merge FreeEntry AllocateStats in tenure space */
	memoryPool->mergeFreeEntryAllocateStats();

	MM_LargeObjectAllocateStats *stats = memoryPool->getLargeObjectAllocateStats();
	stats->addTimeMergeAverage(omrtime_hires_clock() - startTime);

	stats->verifyFreeEntryCount(memoryPool->getActualFreeEntryCount());
	/* estimate Fragmentation */
	if ((GLOBALGC_ESTIMATE_FRAGMENTATION == (_extensions->estimateFragmentation & GLOBALGC_ESTIMATE_FRAGMENTATION))
		&& _extensions->configuration->canCollectFragmentationStats(env)
	) {
		stats->estimateFragmentation(env);
	} else {
		stats->resetRemainingFreeMemoryAfterEstimate();
	}
}

void 
MM_ParallelGlobalGC::reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_OMR_GC_CYCLE_END(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GC_CYCLE_END,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type,
		omrgc_condYieldFromGC
	);
}

void
MM_ParallelGlobalGC::postMark(MM_EnvironmentBase *env)
{
	_markingScheme->getMarkMap()->setMarkMapValid(true);
}

void 
MM_ParallelGlobalGC::setupForGC(MM_EnvironmentBase *env)
{
}	

bool
MM_ParallelGlobalGC::internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription)
{
	_extensions->globalGCStats.gcCount += 1;

	/* only try to expand heap instead of garbage collection in -Xgcpolicy:nogc */
	if (_disableGC) {
		env->_cycleState->_activeSubSpace->checkResize(env, allocDescription, false);
		env->_cycleState->_activeSubSpace->performResize(env, allocDescription);
	} else {
		masterThreadGarbageCollect(env, allocDescription, true, false);
	}
	return true;
}

/**
 * This routine prepares the heap for a parallel walk by performing
 * a marking phase.
 * @see MM_GlobalCollector::prepareHeapForWalk()
 */
void
MM_ParallelGlobalGC::prepareHeapForWalk(MM_EnvironmentBase *env)
{
	GC_OMRVMInterface::flushCachesForGC(env);

	_markingScheme->masterSetupForWalk(env);
	
	/* Run a parallel mark */
	/* TODO CRGTMP fix the cycleState parameter */
	MM_ParallelMarkTask markTask(env, _dispatcher, _markingScheme, true, NULL);
	_dispatcher->run(env, &markTask);

	_delegate.prepareHeapForWalk(env);
}

/**
 * Fixing the heap to be walkable involves finding all objects NOT marked and converting them to dark matter.
 * For the most part, a heap is usually walkable (objects that are dead can be walked, but their fields are
 * suspect).  However, after a class unload, there is no way to skip over a dead object, as the class pointer, which
 * contains sizing information, is now a dangling pointer.
 *
 * Note that this represents a superset of walk requirements and will call fixObject, the most generic of the fixup
 * routines.  Other fixup operations or optimizations to the fixup process should not assume that they can modify
 * or replace this function since there are callers which assume that this will fix up all dead objects and that
 * contract must be preserved as well as the rule that this be a superset of all fixup operations (see CMVC 122959).
 */
uintptr_t
MM_ParallelGlobalGC::fixHeapForWalk(MM_EnvironmentBase *env, UDATA walkFlags, uintptr_t walkReason, MM_HeapWalkerObjectFunc walkFunction)
{
	uintptr_t fixedObjectCount = 0;

	Trc_MM_FixHeapForWalk_Entry(env->getLanguageVMThread(), walkFlags);

	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	U_64 startTime = omrtime_hires_clock();

	_heapWalker->allObjectsDo(env, walkFunction, &fixedObjectCount, walkFlags, true, false);

	_extensions->globalGCStats.fixHeapForWalkTime = omrtime_hires_delta(startTime, omrtime_hires_clock(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
	_extensions->globalGCStats.fixHeapForWalkReason = walkReason;

	Trc_MM_FixHeapForWalk_Exit(env->getLanguageVMThread(), fixedObjectCount);

	return fixedObjectCount;
}

/* (non-doxygen)
 * @see MM_GlobalCollector::heapAddRange()
 */
bool
MM_ParallelGlobalGC::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool result = _markingScheme->heapAddRange(env, subspace, size, lowAddress, highAddress);
	if (0 == result) {
		goto markingScheme_failed_heapAddRange;
	}

	result = _sweepScheme->heapAddRange(env, subspace, size, lowAddress, highAddress);
	if (0 == result) {
		goto sweepScheme_failed_heapAddRange;
	}

#if defined(OMR_GC_OBJECT_MAP)
	result = _extensions->getObjectMap()->heapAddRange(env, subspace, size, lowAddress, highAddress);
	if (0 == result) {
		goto objectMap_failed_heapAddRange;
	}
#endif /* defined(OMR_GC_OBJECT_MAP) */

	result = _delegate.heapAddRange(env, subspace, size, lowAddress, highAddress);
	if (0 == result) {
		goto parallelGlobalGC_failed_heapAddRange;
	}

	return true;

parallelGlobalGC_failed_heapAddRange:
#if defined(OMR_GC_OBJECT_MAP)
	_extensions->getObjectMap()->heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
objectMap_failed_heapAddRange:
#endif /* defined(OMR_GC_OBJECT_MAP) */
	_sweepScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
sweepScheme_failed_heapAddRange:
	_markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
markingScheme_failed_heapAddRange:

	return false;
}

/* (non-doxygen)
 * @see MM_GlobalCollector::heapRemoveRange()
 */
bool
MM_ParallelGlobalGC::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace,uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	bool result = _markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	result = result && _sweepScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

	result = result && _delegate.heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

	return result;
}

/* (non-doxygen)
 * @see MM_GlobalCollector::heapReconfigured()
 */
void
MM_ParallelGlobalGC::heapReconfigured(MM_EnvironmentBase *env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress)
{
	_sweepScheme->heapReconfigured(env);
}

bool
MM_ParallelGlobalGC::collectorStartup(MM_GCExtensionsBase* extensions)
{
#if defined(OMR_GC_MODRON_SCAVENGER)
	if (extensions->scavengerEnabled && (NULL != extensions->scavenger)) {
		extensions->scavenger->collectorStartup(extensions);
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
	return true;
}

void
MM_ParallelGlobalGC::collectorShutdown(MM_GCExtensionsBase *extensions)
{
#if defined(OMR_GC_MODRON_SCAVENGER)
	if (extensions->scavengerEnabled && (NULL != extensions->scavenger)) {
		extensions->scavenger->collectorShutdown(extensions);
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
}

/**
 * Determine if a  expand is required 
 * @return expand size if rator expand required or 0 otheriwse
 */
uint32_t
MM_ParallelGlobalGC::getGCTimePercentage(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	/* Calculate what ratio of time we are spending in GC */	
	uint32_t ratio= extensions->heap->getResizeStats()->calculateGCPercentage();
	return ratio;
}

static void
globalGCHookSysStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_SystemGCStartEvent* event = (MM_SystemGCStartEvent*)eventData;
	OMR_VMThread *omrVMThread = event->currentThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);

	extensions->heap->getResizeStats()->resetRatioTicks();
}

static void
globalGCHookSysEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_SystemGCEndEvent* event = (MM_SystemGCEndEvent*)eventData;
	OMR_VMThread *omrVMThread = event->currentThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);

	/* Save end time so at next AF we can get a realistic time outside gc, while it 
	 * will never be used it may be useful for debugging. */
	extensions->heap->getResizeStats()->setLastAFEndTime(omrtime_hires_clock());
}

static void
globalGCHookAFCycleStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_AllocationFailureCycleStartEvent* event = (MM_AllocationFailureCycleStartEvent*)eventData;
	OMR_VMThread *omrVMThread = event->currentThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
	
	extensions->heap->getResizeStats()->setThisAFStartTime(omrtime_hires_clock());
	extensions->heap->getResizeStats()->setLastTimeOutsideGC();
	extensions->heap->getResizeStats()->setGlobalGCCountAtAF(extensions->globalGCStats.gcCount);
}

static void
globalGCHookAFCycleEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_AllocationFailureCycleEndEvent* event = (MM_AllocationFailureCycleEndEvent*)eventData;
	OMR_VMThread *omrVMThread = event->currentThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
	
	if((event->subSpaceType == MEMORY_TYPE_NEW) && ((extensions->heap->getResizeStats()->getGlobalGCCountAtAF()) == (extensions->globalGCStats.gcCount))){
		return;
	}
		
	/* ..and remember time of last AF end */
	extensions->heap->getResizeStats()->setLastAFEndTime(omrtime_hires_clock());
	
	/* If we have contracted and compacted on this GC then reset ratio ticks as compact will obviously result
	 * in a large increase in time in GC and could result in an unexpected/undesirable ratio EXPAND on next GC
	 */ 
#if defined(OMR_GC_MODRON_COMPACTION)
	if (extensions->globalGCStats.compactStats._lastHeapCompaction == extensions->globalGCStats.gcCount &&
		extensions->heap->getResizeStats()->getLastHeapContractionGCCount() == extensions->globalGCStats.gcCount ) {
		extensions->heap->getResizeStats()->resetRatioTicks();
	} else {
#endif /* OMR_GC_MODRON_COMPACTION */
		extensions->heap->getResizeStats()->updateHeapResizeStats();
#if defined(OMR_GC_MODRON_COMPACTION)
	}
#endif /* OMR_GC_MODRON_COMPACTION */
}

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

static void
globalGCHookCCStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	// triggered by J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START
	MM_ConcurrentCollectionStartEvent* event = (MM_ConcurrentCollectionStartEvent*)eventData;
	OMR_VMThread *omrVMThread = event->currentThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
	
	extensions->heap->getResizeStats()->setThisAFStartTime(omrtime_hires_clock());
	extensions->heap->getResizeStats()->setLastTimeOutsideGC();
 
}

static void
globalGCHookCCEnd(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	// triggered by J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END
	MM_ConcurrentCollectionEndEvent* event = (MM_ConcurrentCollectionEndEvent*)eventData;
	OMR_VMThread *omrVMThread = event->currentThread;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVMThread->_vm);
	OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
	
	/* ..and remember time of last AF end */
	extensions->heap->getResizeStats()->setLastAFEndTime(omrtime_hires_clock());
	
	/* If we have contracted and compacted on this GC then reset ratio ticks as compact will obviously result
	 * in a large increase in time in GC and could result in an unexpected/undesirable ratio EXPAND on next GC
	 */ 
#if defined(OMR_GC_MODRON_COMPACTION)
	if (extensions->globalGCStats.compactStats._lastHeapCompaction == extensions->globalGCStats.gcCount &&
		extensions->heap->getResizeStats()->getLastHeapContractionGCCount() == extensions->globalGCStats.gcCount ) {
		extensions->heap->getResizeStats()->resetRatioTicks();
	} else {
#endif /* OMR_GC_MODRON_COMPACTION */
		extensions->heap->getResizeStats()->updateHeapResizeStats();
#if defined(OMR_GC_MODRON_COMPACTION)
	}
#endif /* OMR_GC_MODRON_COMPACTION */
}
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

/**
 * Request to create sweepPoolState class for pool
 * @param  memoryPool memory pool to attach sweep state to
 * @return pointer to created class
 */
void *
MM_ParallelGlobalGC::createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool)
{
	Assert_MM_true(NULL != _sweepScheme);
	return _sweepScheme->createSweepPoolState(env, memoryPool);
}

/**
 * Request to destroy sweepPoolState class for pool
 * @param  sweepPoolState class to destroy
 */
void
MM_ParallelGlobalGC::deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState)
{
	Assert_MM_true(NULL != _sweepScheme);
	_sweepScheme->deleteSweepPoolState(env, sweepPoolState);
}

bool
MM_ParallelGlobalGC::isMarked(void *objectPtr)
{
	return _markingScheme->getMarkMap()->isBitSet(static_cast<omrobjectptr_t>(objectPtr));
}

void
MM_ParallelGlobalGC::completeExternalConcurrentCycle(MM_EnvironmentBase *env)
{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	if (_extensions->isConcurrentScavengerEnabled()) {
		/* ParallelGlobalGC or ConcurrentGC (STW phase) cannot start before Concurrent Scavenger cycle is in progress */
		_extensions->scavenger->completeConcurrentCycle(env);
	}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
}

void
MM_ParallelGlobalGC::notifyAcquireExclusiveVMAccess(MM_EnvironmentBase *env)
{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	if (_extensions->concurrentScavengeExhaustiveTermination && _extensions->isConcurrentScavengerInProgress()) {
		/* This thread is not releasing VM access (as being the one to request Exclusive) and doesn't have a chance (as other threads) to flush copy caches
		 * through VMaccess-release hook, so it's doing it now.
		 * false (not final) argument should work this time, since this will be flushed one more time (as final), as any other thread, at the start of STW increment.
		 */
		env->flushGCCaches(false);

		/* Must specifically call it against Scavenger, even though the Exclusive access might have been requested to perform Concurrent Global. */
		_extensions->scavenger->externalNotifyToYield(env);
	}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
}

void
MM_ParallelGlobalGC::reportGCCycleStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_OMR_GC_CYCLE_START(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GC_CYCLE_START,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type);
}

void
MM_ParallelGlobalGC::reportGCCycleEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type,
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow(),
		_extensions->globalGCStats.fixHeapForWalkReason,
		_extensions->globalGCStats.fixHeapForWalkTime
	);
}

void
MM_ParallelGlobalGC::reportGCStart(MM_EnvironmentBase *env)
{
	uintptr_t scavengerCount = 0;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

#if defined(OMR_GC_MODRON_SCAVENGER)
	scavengerCount = _extensions->scavengerStats._gcCount;
#endif /* OMR_GC_MODRON_SCAVENGER */

	Trc_MM_GlobalGCStart(env->getLanguageVMThread(), _extensions->globalGCStats.gcCount);
	Trc_OMRMM_GlobalGCStart(env->getOmrVMThread(), _extensions->globalGCStats.gcCount);

	TRIGGER_J9HOOK_MM_OMR_GLOBAL_GC_START(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GLOBAL_GC_START,
		_extensions->globalGCStats.gcCount,
		scavengerCount,
		env->_cycleState->_gcCode.isExplicitGC() ? 1 : 0,
		env->_cycleState->_gcCode.isAggressiveGC() ? 1: 0,
		_bytesRequested);
}

void
MM_ParallelGlobalGC::reportGCIncrementStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CollectionStatisticsStandard *stats = (MM_CollectionStatisticsStandard *)env->_cycleState->_collectionStatistics;
	stats->collectCollectionStatistics(env, stats);
	stats->_startTime = omrtime_hires_clock();

	intptr_t rc = omrthread_get_process_times(&stats->_startProcessTimes);
	switch (rc){
	case -1: /* Error: Function un-implemented on architecture */
	case -2: /* Error: getrusage() or GetProcessTimes() returned error value */
		stats->_startProcessTimes._userTime = I_64_MAX;
		stats->_startProcessTimes._systemTime = I_64_MAX;
		break;
	case  0:
		break; /* Success */
	default:
		Assert_MM_unreachable();
	}

	TRIGGER_J9HOOK_MM_PRIVATE_GC_INCREMENT_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		stats->_startTime,
		J9HOOK_MM_PRIVATE_GC_INCREMENT_START,
		stats);
}

void
MM_ParallelGlobalGC::reportGCIncrementEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CollectionStatisticsStandard *stats = (MM_CollectionStatisticsStandard *)env->_cycleState->_collectionStatistics;
	stats->collectCollectionStatistics(env, stats);
	
	intptr_t rc = omrthread_get_process_times(&stats->_endProcessTimes);
	switch (rc){
	case -1: /* Error: Function un-implemented on architecture */
	case -2: /* Error: getrusage() or GetProcessTimes() returned error value */
		stats->_endProcessTimes._userTime = 0;
		stats->_endProcessTimes._systemTime = 0;
		break;
	case  0:
		break; /* Success */
	default:
		Assert_MM_unreachable();
	}

	stats->_endTime = omrtime_hires_clock();

	TRIGGER_J9HOOK_MM_PRIVATE_GC_INCREMENT_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		stats->_endTime,
		J9HOOK_MM_PRIVATE_GC_INCREMENT_END,
		stats);

	/* reset fragmentation indicator after reporting fragmentation */
	stats->_tenureFragmentation = NO_FRAGMENTATION;
}

void
MM_ParallelGlobalGC::reportGlobalGCIncrementStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uintptr_t scavengerCount = 0;
#if defined(OMR_GC_MODRON_SCAVENGER)
	scavengerCount = _extensions->scavengerStats._gcCount;
#endif /* OMR_GC_MODRON_SCAVENGER */

	TRIGGER_J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread() ,
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START,
		_extensions->globalGCStats.gcCount,
		scavengerCount,
		_bytesRequested);
}

void
MM_ParallelGlobalGC::reportGlobalGCCollectComplete(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_GlobalGCCollectComplete(env->getLanguageVMThread());
	Trc_OMRMM_GlobalGCCollectComplete(env->getOmrVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_GLOBAL_GC_COLLECT_COMPLETE(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_GLOBAL_GC_COLLECT_COMPLETE
	);
}

void
MM_ParallelGlobalGC::reportGCEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	uintptr_t approximateNewActiveFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW);
	uintptr_t newActiveMemorySize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW);
	uintptr_t approximateOldActiveFreeMemorySize = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
	uintptr_t oldActiveMemorySize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD);
	uintptr_t approximateLoaActiveFreeMemorySize = (_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0 );
	uintptr_t loaActiveMemorySize = (_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0 );

	/* not including LOA in total (already accounted by OLD */
	uintptr_t approximateTotalActiveFreeMemorySize = approximateNewActiveFreeMemorySize + approximateOldActiveFreeMemorySize;
	uintptr_t totalActiveMemorySizeTotal = newActiveMemorySize + oldActiveMemorySize;

	Trc_MM_GlobalGCEnd(env->getLanguageVMThread(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		approximateTotalActiveFreeMemorySize,
		totalActiveMemorySizeTotal
	);

	Trc_OMRMM_GlobalGCEnd(env->getOmrVMThread(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		approximateTotalActiveFreeMemorySize,
		totalActiveMemorySizeTotal
	);

	/* these are assigned to temporary variable out-of-line since some preprocessors get confused if you have directives in macros */
	uintptr_t approximateActiveFreeMemorySize = 0;
	uintptr_t activeMemorySize = 0;

	TRIGGER_J9HOOK_MM_OMR_GLOBAL_GC_END(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GLOBAL_GC_END,
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		_extensions->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow(),
		approximateNewActiveFreeMemorySize,
		newActiveMemorySize,
		approximateOldActiveFreeMemorySize,
		oldActiveMemorySize,
		(_extensions-> largeObjectArea ? 1 : 0),
		approximateLoaActiveFreeMemorySize,
		loaActiveMemorySize,
		/* We can't just ask the heap for everything of type FIXED, because that includes scopes as well */
		approximateActiveFreeMemorySize,
		activeMemorySize,
		_extensions->globalGCStats.fixHeapForWalkReason,
		_extensions->globalGCStats.fixHeapForWalkTime
	);

}

void
MM_ParallelGlobalGC::reportGlobalGCIncrementEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_END,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData)
	);
}

void
MM_ParallelGlobalGC::reportMarkStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkStart(env->getLanguageVMThread());
	Trc_OMRMM_MarkStart(env->getOmrVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_START);
}

void
MM_ParallelGlobalGC::reportMarkEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkEnd(env->getLanguageVMThread());
	Trc_OMRMM_MarkEnd(env->getOmrVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_END);
}

void
MM_ParallelGlobalGC::reportSweepStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepStart(env->getLanguageVMThread());
	Trc_OMRMM_SweepStart(env->getOmrVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_START);
}

void
MM_ParallelGlobalGC::reportSweepEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepEnd(env->getLanguageVMThread());
	Trc_OMRMM_SweepEnd(env->getOmrVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_END);
}

#if defined(OMR_GC_MODRON_COMPACTION)
void
MM_ParallelGlobalGC::reportCompactStart(MM_EnvironmentBase *env)
{
	CompactReason compactReason = (CompactReason)(_extensions->globalGCStats.compactStats._compactReason);

	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_CompactStart(env->getLanguageVMThread(), getCompactionReasonAsString(compactReason));
	Trc_OMRMM_CompactStart(env->getOmrVMThread(), getCompactionReasonAsString(compactReason));

	TRIGGER_J9HOOK_MM_PRIVATE_COMPACT_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_COMPACT_START,
		_extensions->globalGCStats.gcCount);
}

void
MM_ParallelGlobalGC::reportCompactEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_CompactEnd(env->getLanguageVMThread(), _extensions->globalGCStats.compactStats._movedBytes);
	Trc_OMRMM_CompactEnd(env->getOmrVMThread(), _extensions->globalGCStats.compactStats._movedBytes);

	TRIGGER_J9HOOK_MM_OMR_COMPACT_END(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_COMPACT_END);
}
#endif /* OMR_GC_MODRON_COMPACTION */

#if defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS)
void
MM_ParallelGlobalGC::poisonHeap(MM_EnvironmentBase *env)
{
	/* This will poison only the heap slots */
	_heapWalker->allObjectsDo(env, poisonReferenceSlots, NULL, 0, true, false);
}

void
MM_ParallelGlobalGC::healHeap(MM_EnvironmentBase *env)
{
	/* This will heal only the heap slots */
	_heapWalker->allObjectsDo(env, healReferenceSlots, NULL, 0, false, false);
	/* Don't have the mark map at the start of gc so we'll have to iterate over
	 * in a sequential manner
	 */
}
#endif /* defined(OMR_ENV_DATA64) && defined(OMR_GC_FULL_POINTERS) */
