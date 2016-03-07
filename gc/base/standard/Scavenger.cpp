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
 *******************************************************************************/

#if 0
#define J9MODRON_SCAVENGER_DEBUG
#define J9MODRON_SCAVENGER_TRACE
#define J9MODRON_SCAVENGER_TRACE_COPY
#define J9MODRON_SCAVENGER_TRACE_REMEMBERED_SET
#endif

#include <math.h>

#include "omrcfg.h"
#include "omrcomp.h"
#include "omrmodroncore.h"
#include "mmomrhook.h"
#include "mmomrhook_internal.h"
#include "modronapicore.hpp"
#include "modronbase.h"
#include "modronopt.h"
#include "ModronAssertions.h"
#include "omr.h"
#include "thread_api.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#include "CollectionStatisticsStandard.hpp"
#include "Collector.hpp"
#include "CollectorLanguageInterface.hpp"
#include "ConfigurationStandard.hpp"
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "EnvironmentStandard.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "HeapStats.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceRegionIterator.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "ObjectAllocationInterface.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectModel.hpp"
#include "OMRVMInterface.hpp"
#include "OMRVMThreadListIterator.hpp"
#include "PhysicalSubArena.hpp"
#include "Scavenger.hpp"
#include "ScavengerStats.hpp"
#include "SublistIterator.hpp"
#include "SublistPool.hpp"
#include "SublistPuddle.hpp"
#include "SublistSlotIterator.hpp"

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.
 */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"

#define INITIAL_FREE_HISTORY_WEIGHT ((float)0.8)
#define TENURE_BYTES_HISTORY_WEIGHT ((float)0.8)

uintptr_t
MM_Scavenger::getVMStateID()
{
	return J9VMSTATE_GC_COLLECTOR_SCAVENGER;
}

void
MM_Scavenger::hookGlobalCollectionStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_GlobalGCStartEvent *event = (MM_GlobalGCStartEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_Scavenger *)userData)->globalCollectionStart(env);
}

void
MM_Scavenger::hookGlobalCollectionComplete(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_GlobalGCEndEvent *event = (MM_GlobalGCEndEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_Scavenger *)userData)->globalCollectionComplete(env);
}

/**
 * Initialization
 */
bool
MM_Scavenger::initialize(MM_EnvironmentBase *env)
{
	J9HookInterface** mmOmrHooks = J9_HOOK_INTERFACE(_extensions->omrHookInterface);

	/* Register hook for global GC end. */
	(*mmOmrHooks)->J9HookRegister(mmOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, hookGlobalCollectionStart, (void *)this);
	(*mmOmrHooks)->J9HookRegister(mmOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, hookGlobalCollectionComplete, (void *)this);

	/* initialize the global scavenger gcCount */
	_extensions->scavengerStats._gcCount = 0;
	
	return true;
}

void
MM_Scavenger::tearDown(MM_EnvironmentBase *env)
{
	J9HookInterface** mmOmrHooks = J9_HOOK_INTERFACE(_extensions->omrHookInterface);
	/* Unregister hook for global GC end. */
	(*mmOmrHooks)->J9HookUnregister(mmOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, hookGlobalCollectionStart, (void *)this);
	(*mmOmrHooks)->J9HookUnregister(mmOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, hookGlobalCollectionComplete, (void *)this);
}

/****************************************
 * Initialization routines
 ****************************************
 */

void
MM_Scavenger::setupForGC(MM_EnvironmentBase *env)
{
}

void
MM_Scavenger::masterSetupForGC(MM_EnvironmentBase *envBase)
{
	/* Cache heap ranges for fast "valid object" checks (this can change in an expanding heap situation, so we refetch every cycle) */
	_heapBase = _extensions->heap->getHeapBase();
	_heapTop = _extensions->heap->getHeapTop();

	/* ensure heap base is aligned to region size */
	uintptr_t regionSize = _extensions->heap->getHeapRegionManager()->getRegionSize();
	Assert_MM_true((0 != regionSize) && (0 == ((uintptr_t)_heapBase % regionSize)));

	/* Clear the gc statistics */
	clearGCStats(envBase);

	/* invoke collector language interface callback */
	_cli->scavenger_masterSetupForGC_clearLangStats(envBase);

	/* Allow expansion in the tenure area on failed promotions (but no resizing on the semispace) */
	_expandTenureOnFailedAllocate = true;
	_activeSubSpace = (MM_MemorySubSpaceSemiSpace *)(envBase->_cycleState->_activeSubSpace);
	_cachedSemiSpaceResizableFlag = _activeSubSpace->setResizable(false);

	/* Reset the minimum failure sizes */
	_minTenureFailureSize = UDATA_MAX;
	_minSemiSpaceFailureSize = UDATA_MAX;

	/* Find tenure memory sub spaces for collection ( allocate and survivor are context specific) */
	/* Find the allocate, survivor and tenure memory sub spaces for collection */
	_evacuateMemorySubSpace = _activeSubSpace->getMemorySubSpaceAllocate();
	_survivorMemorySubSpace = _activeSubSpace->getMemorySubSpaceSurvivor();
	_tenureMemorySubSpace = _activeSubSpace->getTenureMemorySubSpace();
	
	/* Accumulate pre-scavenge allocation statistics */
	MM_HeapStats heapStatsSemiSpace;
	MM_HeapStats heapStatsTenureSpace;
	MM_ScavengerStats* scavengerStats = &_extensions->scavengerStats;
	_activeSubSpace->mergeHeapStats(&heapStatsSemiSpace);
	_tenureMemorySubSpace->mergeHeapStats(&heapStatsTenureSpace);
	scavengerStats->_tenureSpaceAllocBytesAcumulation += heapStatsTenureSpace._allocBytes;
	scavengerStats->_semiSpaceAllocBytesAcumulation += heapStatsSemiSpace._allocBytes;

	/* Record the tenure mask */
	_tenureMask = calculateTenureMask();
	
	_activeSubSpace->masterSetupForGC(envBase);
	
	/* evacuate range for GC is what allocate space is for allocation */
	GC_MemorySubSpaceRegionIterator allocateRegionIterator(_evacuateMemorySubSpace);
	MM_HeapRegionDescriptor* region = allocateRegionIterator.nextRegion();
	Assert_MM_true(NULL != region);
	Assert_MM_true(NULL == allocateRegionIterator.nextRegion());
	_evacuateSpaceBase = region->getLowAddress();
	_evacuateSpaceTop = region->getHighAddress();

	/* cache survivor ranges */
	GC_MemorySubSpaceRegionIterator survivorRegionIterator(_survivorMemorySubSpace);
	region = survivorRegionIterator.nextRegion();
	Assert_MM_true(NULL != region);
	Assert_MM_true(NULL == survivorRegionIterator.nextRegion());
	_survivorSpaceBase = region->getLowAddress();
	_survivorSpaceTop = region->getHighAddress();
}

void
MM_Scavenger::workerSetupForGC(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard* env = MM_EnvironmentStandard::getEnvironment(envBase);

	/* Clear local stats */
	memset((void *)&(envBase->_scavengerStats), 0, sizeof(MM_ScavengerStats));

	/* Clear local language-specific stats */
	_cli->scavenger_workerSetupForGC_clearEnvironmentLangStats(envBase);

	/* record that this thread is participating in this cycle */
	env->_scavengerStats._gcCount = _extensions->scavengerStats._gcCount;

	/* Reset the local remembered set fragment */
	env->_scavengerRememberedSet.count = 0;
	env->_scavengerRememberedSet.fragmentCurrent = NULL;
	env->_scavengerRememberedSet.fragmentTop = NULL;
	env->_scavengerRememberedSet.fragmentSize = (uintptr_t)J9_SCV_REMSET_FRAGMENT_SIZE;
	env->_scavengerRememberedSet.parentList = &_extensions->rememberedSet;
}

void
MM_Scavenger::reportScavengeStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	TRIGGER_J9HOOK_MM_PRIVATE_SCAVENGE_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SCAVENGE_START
	);
}

void
MM_Scavenger::reportScavengeEnd(MM_EnvironmentBase *envBase)
{
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	MM_EnvironmentStandard* env = MM_EnvironmentStandard::getEnvironment(envBase);

	bool scavengeSuccessful = scavengeCompletedSuccessfully(envBase);
	_cli->scavenger_reportScavengeEnd(envBase, scavengeSuccessful);

	_extensions->scavengerStats._tiltRatio = calculateTiltRatio();

	Trc_MM_Tiltratio(env->getLanguageVMThread(), _extensions->scavengerStats._tiltRatio);

	TRIGGER_J9HOOK_MM_PRIVATE_SCAVENGE_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SCAVENGE_END,
		env->_cycleState->_activeSubSpace
	);
}

void
MM_Scavenger::reportGCStart(MM_EnvironmentBase *envBase)
{
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	MM_EnvironmentStandard* env = MM_EnvironmentStandard::getEnvironment(envBase);
	/* TODO CRGTMP deprecate this trace point and add a new one */
	Trc_MM_LocalGCStart(env->getLanguageVMThread(),
		_extensions->globalGCStats.gcCount,
		_extensions->scavengerStats._gcCount,
		0, /* used to be weak reference count */
		0, /* used to be soft reference count */
		0, /* used to be phantom reference count */
		0
	);

	Trc_OMRMM_LocalGCStart(env->getOmrVMThread(),
	_extensions->globalGCStats.gcCount,
	        _extensions->scavengerStats._gcCount,
	        0, /* used to be weak reference count */
	        0, /* used to be soft reference count */
	        0, /* used to be phantom reference count */
	        0
	);

	TRIGGER_J9HOOK_MM_OMR_LOCAL_GC_START(
		_extensions->omrHookInterface,
		envBase->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_LOCAL_GC_START,
		_extensions->globalGCStats.gcCount,
		_extensions->scavengerStats._gcCount
	);
}

void
MM_Scavenger::reportGCEnd(MM_EnvironmentBase *envBase)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(envBase);
	MM_EnvironmentStandard * env = MM_EnvironmentStandard::getEnvironment(envBase);

	Trc_MM_LocalGCEnd(env->getLanguageVMThread(),
		_extensions->scavengerStats._rememberedSetOverflow,
		_extensions->scavengerStats._causedRememberedSetOverflow,
		_extensions->scavengerStats._scanCacheOverflow,
		_extensions->scavengerStats._failedFlipCount,
		_extensions->scavengerStats._failedFlipBytes,
		_extensions->scavengerStats._failedTenureCount,
		_extensions->scavengerStats._failedTenureBytes,
		_extensions->scavengerStats._flipCount,
		_extensions->scavengerStats._flipBytes,
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
		(_extensions-> largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : (uintptr_t)0 ),
		(_extensions-> largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : (uintptr_t)0 ),
		_extensions->scavengerStats._tenureAge
	);

	Trc_OMRMM_LocalGCEnd(env->getOmrVMThread(),
		_extensions->scavengerStats._rememberedSetOverflow,
		_extensions->scavengerStats._causedRememberedSetOverflow,
		_extensions->scavengerStats._scanCacheOverflow,
		_extensions->scavengerStats._failedFlipCount,
		_extensions->scavengerStats._failedFlipBytes,
		_extensions->scavengerStats._failedTenureCount,
		_extensions->scavengerStats._failedTenureBytes,
		_extensions->scavengerStats._flipCount,
		_extensions->scavengerStats._flipBytes,
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
		(_extensions-> largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : (uintptr_t)0 ),
		(_extensions-> largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : (uintptr_t)0 ),
		_extensions->scavengerStats._tenureAge
	);

	TRIGGER_J9HOOK_MM_OMR_LOCAL_GC_END(
		_extensions->omrHookInterface,
		envBase->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_LOCAL_GC_END,
		env->_cycleState->_activeSubSpace,
		_extensions->globalGCStats.gcCount,
		_extensions->scavengerStats._gcCount,
		_extensions->scavengerStats._rememberedSetOverflow,
		_extensions->scavengerStats._causedRememberedSetOverflow,
		_extensions->scavengerStats._scanCacheOverflow,
		_extensions->scavengerStats._failedFlipCount,
		_extensions->scavengerStats._failedFlipBytes,
		_extensions->scavengerStats._failedTenureCount,
		_extensions->scavengerStats._failedTenureBytes,
		_extensions->scavengerStats._backout,
		_extensions->scavengerStats._flipCount,
		_extensions->scavengerStats._flipBytes,
		_extensions->scavengerStats._tenureAggregateCount,
		_extensions->scavengerStats._tenureAggregateBytes,
		_extensions->tiltedScavenge ? 1 : 0,
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
		(_extensions->largeObjectArea ? 1 : 0),
		(_extensions->largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0),
		(_extensions->largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) :0),
		_extensions->scavengerStats._tenureAge,
		_extensions->heap->getMemorySize()
	);
}

/**
 * Clear any global stats associated to the scavenger.
 */
void
MM_Scavenger::clearGCStats(MM_EnvironmentBase *env)
{
	_extensions->scavengerStats.clear();
}

/**
 * Merge the current threads scavenge stats into the global scavenge stats.
 */
void
MM_Scavenger::mergeGCStats(MM_EnvironmentBase *envBase)
{
	MM_ScavengerStats *finalGCStats, *scavStats;
	MM_EnvironmentStandard* env = MM_EnvironmentStandard::getEnvironment(envBase);
	finalGCStats = &_extensions->scavengerStats;
	scavStats = &env->_scavengerStats;

	finalGCStats->_rememberedSetOverflow |= scavStats->_rememberedSetOverflow;
	finalGCStats->_causedRememberedSetOverflow |= scavStats->_causedRememberedSetOverflow;
	finalGCStats->_scanCacheOverflow |= scavStats->_scanCacheOverflow;
	finalGCStats->_backout |= scavStats->_backout;
	finalGCStats->_tenureAggregateCount += scavStats->_tenureAggregateCount;
	finalGCStats->_tenureAggregateBytes += scavStats->_tenureAggregateBytes;
#if defined(OMR_GC_LARGE_OBJECT_AREA)
	finalGCStats->_tenureLOACount += scavStats->_tenureLOACount;
	finalGCStats->_tenureLOABytes += scavStats->_tenureLOABytes;
#endif /* OMR_GC_LARGE_OBJECT_AREA */
	finalGCStats->_flipCount += scavStats->_flipCount;
	finalGCStats->_flipBytes += scavStats->_flipBytes;
	finalGCStats->_failedTenureCount += scavStats->_failedTenureCount;
	finalGCStats->_failedTenureBytes += scavStats->_failedTenureBytes;
	finalGCStats->_failedTenureLargest = OMR_MAX(scavStats->_failedTenureLargest,
											 finalGCStats->_failedTenureLargest);
	finalGCStats->_failedFlipCount += scavStats->_failedFlipCount;
	finalGCStats->_failedFlipBytes += scavStats->_failedFlipBytes;

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
	finalGCStats->_acquireFreeListCount += scavStats->_acquireFreeListCount;
	finalGCStats->_releaseFreeListCount += scavStats->_releaseFreeListCount;
	finalGCStats->_acquireScanListCount += scavStats->_acquireScanListCount;
	finalGCStats->_releaseScanListCount += scavStats->_releaseScanListCount;
	finalGCStats->_acquireListLockCount += scavStats->_acquireListLockCount;
	finalGCStats->_aliasToCopyCacheCount += scavStats->_aliasToCopyCacheCount;
	finalGCStats->_arraySplitCount += scavStats->_arraySplitCount;
	finalGCStats->_arraySplitAmount += scavStats->_arraySplitAmount;
#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

	finalGCStats->_flipDiscardBytes += scavStats->_flipDiscardBytes;
	finalGCStats->_tenureDiscardBytes += scavStats->_tenureDiscardBytes;

	finalGCStats->_survivorTLHRemainderCount += scavStats->_survivorTLHRemainderCount;
	finalGCStats->_tenureTLHRemainderCount += scavStats->_tenureTLHRemainderCount;

	finalGCStats->_semiSpaceAllocationCountLarge += scavStats->_semiSpaceAllocationCountLarge;
	finalGCStats->_semiSpaceAllocationCountSmall += scavStats->_semiSpaceAllocationCountSmall;

	finalGCStats->_tenureSpaceAllocationCountLarge += scavStats->_tenureSpaceAllocationCountLarge;
	finalGCStats->_tenureSpaceAllocationCountSmall += scavStats->_tenureSpaceAllocationCountSmall;

	if (env->isMasterThread()) {
		finalGCStats->getFlipHistory(0)->_tenureMask = _tenureMask;
		uintptr_t tenureAge = 0;
		for (tenureAge = 0; tenureAge <= OBJECT_HEADER_AGE_MAX; ++tenureAge) {
			if (_tenureMask & ((uintptr_t)1 << tenureAge)) {
				break;
			}
		}
		finalGCStats->_tenureAge = tenureAge;

		MM_ScavengerStats::FlipHistory* flipHistoryPrevious = finalGCStats->getFlipHistory(1);
		flipHistoryPrevious->_flipBytes[0] = finalGCStats->_semiSpaceAllocBytesAcumulation;
		flipHistoryPrevious->_tenureBytes[0] = finalGCStats->_tenureSpaceAllocBytesAcumulation;

		finalGCStats->_semiSpaceAllocBytesAcumulation = 0;
		finalGCStats->_tenureSpaceAllocBytesAcumulation = 0;
	}

	for (int i = 1; i <= OBJECT_HEADER_AGE_MAX+1; ++i) {
		finalGCStats->getFlipHistory(0)->_flipBytes[i] += scavStats->getFlipHistory(0)->_flipBytes[i];
		finalGCStats->getFlipHistory(0)->_tenureBytes[i] += scavStats->getFlipHistory(0)->_tenureBytes[i];
	}

	finalGCStats->_tenureExpandedBytes += scavStats->_tenureExpandedBytes;
	finalGCStats->_tenureExpandedCount += scavStats->_tenureExpandedCount;
	finalGCStats->_tenureExpandedTime += scavStats->_tenureExpandedTime;

	/* Merge language specific statistics */
	_cli->scavenger_mergeGCStats_mergeLangStats(envBase);
}

/**
 * Determine whether GC stats should be calculated for this round.
 * @return true if GC stats should be calculated for this round, false otherwise.
 */
bool
MM_Scavenger::canCalcGCStats(MM_EnvironmentBase *env)
{
	/* If we actually did a scavenge this time around then it's safe to gather stats */
	return _extensions->heap->getPercolateStats()->getScavengesSincePercolate() > 0 ? true : false;
}

/**
 * Calculate any GC stats after a collection.
 */
void
MM_Scavenger::calcGCStats(MM_EnvironmentBase *env)
{
	/* Do not calculate stats unless we actually collected */
	if (canCalcGCStats(env)) {
		MM_ScavengerStats *scavengerGCStats;
		scavengerGCStats = &_extensions->scavengerStats;
		uintptr_t initialFree = env->_cycleState->_activeSubSpace->getActualActiveFreeMemorySize();

		/* First collection  ? */
		if (scavengerGCStats->_gcCount > 1 ) {
			scavengerGCStats->_avgInitialFree = (uintptr_t)MM_Math::weightedAverage((float)scavengerGCStats->_avgInitialFree, (float)initialFree, INITIAL_FREE_HISTORY_WEIGHT);
			scavengerGCStats->_avgTenureBytes = (uintptr_t)MM_Math::weightedAverage((float)scavengerGCStats->_avgTenureBytes, (float)scavengerGCStats->_tenureAggregateBytes, TENURE_BYTES_HISTORY_WEIGHT);
#if defined(OMR_GC_LARGE_OBJECT_AREA)
			scavengerGCStats->_avgTenureSOABytes = (uintptr_t)MM_Math::weightedAverage((float)scavengerGCStats->_avgTenureSOABytes,
																		(float)(scavengerGCStats->_tenureAggregateBytes - scavengerGCStats->_tenureLOABytes),
																		TENURE_BYTES_HISTORY_WEIGHT);
			scavengerGCStats->_avgTenureLOABytes = (uintptr_t)MM_Math::weightedAverage((float)scavengerGCStats->_avgTenureLOABytes,
																		(float)scavengerGCStats->_tenureLOABytes,
																		TENURE_BYTES_HISTORY_WEIGHT);

#endif /* OMR_GC_LARGE_OBJECT_AREA */
		} else {
			scavengerGCStats->_avgInitialFree = initialFree;
			scavengerGCStats->_avgTenureBytes = scavengerGCStats->_tenureAggregateBytes;
#if defined(OMR_GC_LARGE_OBJECT_AREA)
			scavengerGCStats->_avgTenureSOABytes = scavengerGCStats->_tenureAggregateBytes - scavengerGCStats->_tenureLOABytes;
			scavengerGCStats->_avgTenureLOABytes = scavengerGCStats->_tenureLOABytes;
#endif /* OMR_GC_LARGE_OBJECT_AREA */
		}
	}
}

/**
 * Determine whether a scavenge that has been started did complete successfully.
 * @return true if the scavenge completed successfully, false otherwise.
 */
bool
MM_Scavenger::scavengeCompletedSuccessfully(MM_EnvironmentBase *envBase)
{
	return true;
}

/**
 * Setup, execute and complete a scavenge.
 */
void
MM_Scavenger::masterThreadGarbageCollect(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard* env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	Trc_MM_Scavenger_masterThreadGarbageCollect_Entry(env->getLanguageVMThread());

	if (_extensions->trackMutatorThreadCategory) {
		/* This thread is doing GC work, account for the time spent into the GC bucket */
		omrthread_set_category(env->getOmrVMThread()->_os_thread, J9THREAD_CATEGORY_SYSTEM_GC_THREAD, J9THREAD_TYPE_SET_GC);
	}

	if (_extensions->processLargeAllocateStats) {
		processLargeAllocateStatsBeforeGC(env);
	}

	reportGCCycleStart(env);
	reportGCStart(env);
	reportGCIncrementStart(env);
	reportScavengeStart(env);

	_extensions->scavengerStats._startTime = omrtime_hires_clock();

	/* Perform any master-specific setup */
	masterSetupForGC(env);

	/* And perform the scavenge */
	scavenge(env);

	/* defer to collector language interface */
	_cli->scavenger_masterThreadGarbageCollect_scavengeComplete(envBase);

	/* Record the completion time of the scavenge */
	_extensions->scavengerStats._endTime = omrtime_hires_clock();

	reportScavengeEnd(env);

	/* Reset the resizable flag of the semi space.
	 * NOTE: Must be done before we attempt to resize the new space.
	 */
	_activeSubSpace->setResizable(_cachedSemiSpaceResizableFlag);

	if(scavengeCompletedSuccessfully(env)) {
		/* Merge sublists in the remembered set (if necessary) */
		_extensions->rememberedSet.compact(env);

		/* Must report object events before memory spaces are flipped */
		_cli->scavenger_reportObjectEvents(env);
		
		/* If -Xgc:fvtest=forcePoisonEvacuate has been specified, poison(fill poison pattern) evacuate space */
		if(_extensions->fvtest_forcePoisonEvacuate) {
			_activeSubSpace->poisonEvacuateSpace();
		}

		/* Build free list in evacuate profile */
		_activeSubSpace->rebuildFreeListForEvacuate(env);

		/* Flip the memory space allocate profile */
		_activeSubSpace->flip();

		/* Let know MemorySpace about new default MemorySubSpace */
		_activeSubSpace->getMemorySpace()->setDefaultMemorySubSpace(_activeSubSpace->getDefaultMemorySubSpace());

		/* Adjust memory between the semi spaces where applicable */
		_activeSubSpace->checkResize(env);
		_activeSubSpace->performResize(env);

		/* Defer to collector language interface */
		_cli->scavenger_masterThreadGarbageCollect_scavengeSuccess(env);

		if(_extensions->scvTenureStrategyAdaptive) {
			/* Adjust the tenure age based on the percentage of new space used.  Also, avoid / by 0 */
			uintptr_t newSpaceTotalSize = _activeSubSpace->getActiveMemorySize();
			uintptr_t newSpaceConsumedSize = newSpaceTotalSize - _activeSubSpace->getActualActiveFreeMemorySize();
			uintptr_t newSpaceSizeScale = newSpaceTotalSize / 100;

			if((newSpaceConsumedSize < (_extensions->scvTenureRatioLow * newSpaceSizeScale)) && (_extensions->scvTenureAdaptiveTenureAge < OBJECT_HEADER_AGE_MAX)) {
				_extensions->scvTenureAdaptiveTenureAge++;
			} else {
				if((newSpaceConsumedSize > (_extensions->scvTenureRatioHigh * newSpaceSizeScale)) && (_extensions->scvTenureAdaptiveTenureAge > OBJECT_HEADER_AGE_MIN)) {
					_extensions->scvTenureAdaptiveTenureAge--;
				}
			}
		}
	} else {
		/* Build free list in survivor profile - the scavenge was unsuccessful, so rebuild the free list */
		_activeSubSpace->rebuildFreeListForBackout(env);
	}

	/* Restart the allocation caches associated to all threads */
	{
		GC_OMRVMThreadListIterator threadListIterator(_omrVM);
		OMR_VMThread *walkThread;
		while((walkThread = threadListIterator.nextOMRVMThread()) != NULL) {
			MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkThread);
			walkEnv->_objectAllocationInterface->restartCache(env);
		}
	}

	_extensions->heap->resetHeapStatistics(false);

	/* If there was a failed tenure of a size greater than the threshold, set the flag. */
	/* The next attempt to scavenge will result in a global collect */
	if (_extensions->scavengerStats._failedTenureCount > 0) {
		if (_extensions->scavengerStats._failedTenureBytes >= _extensions->scavengerFailedTenureThreshold) {
			Trc_MM_Scavenger_masterThreadGarbageCollect_setFailedTenureFlag(env->getLanguageVMThread(), _extensions->scavengerStats._failedTenureLargest);
			setFailedTenureThresholdFlag();
			setFailedTenureLargestObject(_extensions->scavengerStats._failedTenureLargest);
		}
	}
	if (_extensions->processLargeAllocateStats) {
		processLargeAllocateStatsAfterGC(env);
	}
	
	reportGCCycleFinalIncrementEnding(env);
	reportGCIncrementEnd(env);
	reportGCEnd(env);
	reportGCCycleEnd(env);

	if (_extensions->processLargeAllocateStats) {
		/* reset tenure processLargeAllocateStats after TGC */ 
		resetTenureLargeAllocateStats(env);
	}
	_extensions->allocationStats.clear();

	if (_extensions->trackMutatorThreadCategory) {
		/* Done doing GC, reset the category back to the old one */
		omrthread_set_category(env->getOmrVMThread()->_os_thread, 0, J9THREAD_TYPE_SET_GC);
	}

	Trc_MM_Scavenger_masterThreadGarbageCollect_Exit(env->getLanguageVMThread());
}

void
MM_Scavenger::processLargeAllocateStatsBeforeGC(MM_EnvironmentBase *env) 
{
	MM_MemorySpace *defaultMemorySpace = _extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *defaultMemorySubspace = defaultMemorySpace->getDefaultMemorySubSpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();

	/* merge largeObjectAllocateStats in nursery space */
	if (defaultMemorySubspace->isPartOfSemiSpace()) {
		/* SemiSpace stats include only Mutator stats (no Collector stats during flipping) */
		defaultMemorySubspace->getTopLevelMemorySubSpace(MEMORY_TYPE_NEW)->mergeLargeObjectAllocateStats(env);
	}
	
	/* TODO: remove the below 2 lines(resetLargeObjectAllocateStats), so that we do not loose direct mutator allocation info */ 
	MM_MemoryPool *tenureMemoryPool = tenureMemorySubspace->getMemoryPool();
	tenureMemoryPool->resetLargeObjectAllocateStats();
}

void 
MM_Scavenger::processLargeAllocateStatsAfterGC(MM_EnvironmentBase *env) 
{
	MM_MemorySpace *defaultMemorySpace = _extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
	MM_MemoryPool *memoryPool = tenureMemorySubspace->getMemoryPool();

	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();

	/* merge and average largeObjectAllocateStats in tenure space */
	memoryPool->mergeLargeObjectAllocateStats();
	memoryPool->mergeTlhAllocateStats();
	/* TODO: need to consider allocation form mutators for averaging later, currently only average allocation from collectors */
	memoryPool->averageLargeObjectAllocateStats(env, _extensions->scavengerStats._tenureAggregateBytes);
	/* merge FreeEntry AllocateStats in tenure space */
	memoryPool->mergeFreeEntryAllocateStats();
	MM_LargeObjectAllocateStats *stats = memoryPool->getLargeObjectAllocateStats();
	stats->setTimeMergeAverage(omrtime_hires_clock() - startTime);

	stats->verifyFreeEntryCount(memoryPool->getActualFreeEntryCount());
	/* estimate Fragmentation */
	if (LOCALGC_ESTIMATE_FRAGMENTATION == (_extensions->estimateFragmentation & LOCALGC_ESTIMATE_FRAGMENTATION)) {
		stats->estimateFragmentation(env);
	} else {
		stats->resetRemainingFreeMemoryAfterEstimate();
	}
}


void
MM_Scavenger::reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard* env = MM_EnvironmentStandard::getEnvironment(envBase);
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

/**
 * Report API for when an expansion has occurred during a collection.
 * @seealso MM_Collector::collectorExpanded(MM_EnvironmentBase *, MM_MemorySubSpace *, uintptr_t)
 * @note This call is NOT made pre/post collection, when actual user type allocation requests are being statisfied.
 * @note an expandSize of 0 represents a failed expansion attempt
 * @param subSpace memory subspace that has expanded.
 * @param expandSize number of bytes the subspace was expanded by.
 */
void
MM_Scavenger::collectorExpanded(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t expandSize)
{
	MM_Collector::collectorExpanded(env, subSpace, expandSize);

	if(0 == expandSize) {
		/* Cause a ggc on next scav as expand of tenure failed */
		setExpandFailedFlag();

		/* Expand failed so stop subsequent attempts during this scavenge */
		_expandTenureOnFailedAllocate = false;
	} else {
		MM_HeapResizeStats *resizeStats = _extensions->heap->getResizeStats();
		Assert_MM_true(SATISFY_COLLECTOR == resizeStats->getLastExpandReason());
		Assert_MM_true(MEMORY_TYPE_OLD == subSpace->getTypeFlags());

		env->_scavengerStats._tenureExpandedCount += 1;
		env->_scavengerStats._tenureExpandedBytes += expandSize;
		env->_scavengerStats._tenureExpandedTime += resizeStats->getLastExpandTime();
	}
}

/**
 * Answer whether the subspace can expand on a collector-invoked allocate request.
 * The query is made only on collectorAllocate() type requests when the allocation fails, and a decision on whether
 * to expand the subspace to satisfy the allocate is being made.
 * @seealso MM_Collector::canCollectorExpand(MM_EnvironmentBase *, MM_MemorySubSpace *, uintptr_t)
 * @note Call is only made during collection and during a collectorAllocate() type request to the subspace.
 * @return true if the subspace is allowed to expand, false otherwise.
 */
bool
MM_Scavenger::canCollectorExpand(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, uintptr_t expandSize)
{
	/* the only known caller is SubSpaceFlat */
	Assert_MM_true(subSpace == _tenureMemorySubSpace->getParent());
	return  _expandTenureOnFailedAllocate;
}

/**
 * Returns requested expand size.
 * The query is made only on collectorAllocate() type requests when the allocation fails, and a decision
 * on how much to expand the subspace to satisfy the allocate is being made.
 * @seealso MM_Collector::getCollectorExpandSize(MM_EnvironmentBase *)
 * @note Call is only made during collection and during a collectorAllocate() type request to the subspace.
 * @return size to subspace expand by.
 * @ingroup GC_Modron_base methodGroup
 */
uintptr_t
MM_Scavenger::getCollectorExpandSize(MM_EnvironmentBase *env)
{
	MM_ScavengerStats *scavengerGCStats= &_extensions->scavengerStats;
	uintptr_t expandSize =  ( (uintptr_t)(scavengerGCStats->_avgTenureBytes * _extensions->scavengerCollectorExpandRatio));
	expandSize = OMR_MIN(_extensions->scavengerMaximumCollectorExpandSize, expandSize);
	return expandSize;
}

/**
 * Perform any pre-collection work as requested by the garbage collection invoker.
 */
void
MM_Scavenger::internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode)
{
	_cycleState = MM_CycleState();
	env->_cycleState = &_cycleState;
	env->_cycleState->_gcCode = MM_GCCode(gcCode);
	env->_cycleState->_type = _cycleType;
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

	/* Flush any VM level changes to prepare for a safe slot walk */
	GC_OMRVMInterface::flushCachesForGC(env);
}

/**
 * Perform any post-collection work as requested by the garbage collection invoker.
 */
void
MM_Scavenger::internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	calcGCStats(env);

	return ;
}

/**
 * Internal API for invoking a garbage collect.
 * @return true if the collection completed successfully, false otherwise.
 */
bool
MM_Scavenger::internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription)
{
	MM_ScavengerStats *scavengerGCStats= &_extensions->scavengerStats;
	MM_MemorySubSpace *tenureMemorySubSpace = ((MM_MemorySubSpaceSemiSpace *)subSpace)->getTenureMemorySubSpace();

	/* First, if the previous scavenge had a failed tenure of a size greater than the threshold,
	 * ask parent MSS to try a collect.
	 */
	if (failedTenureThresholdReached()) {
		Trc_MM_Scavenger_percolate_failedTenureThresholdReached(env->getLanguageVMThread(), getFailedTenureLargestObject(), _extensions->heap->getPercolateStats()->getScavengesSincePercolate());

		/* Create an allocate description to describe the size of the
		 * largest chunk we need in the tenure space.
		 */
		MM_AllocateDescription percolateAllocDescription(getFailedTenureLargestObject(), OMR_GC_ALLOCATE_OBJECT_TENURED, false, true);

		/* We do an aggressive percolate if the last scavenge also percolated */
		uint32_t aggressivePercolate = _extensions->heap->getPercolateStats()->getScavengesSincePercolate() <= 1 ? J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE : J9MMCONSTANT_IMPLICIT_GC_PERCOLATE;

		/* Percolate the collect to parent MSS */
		bool result = percolateGarbageCollect(env, subSpace, &percolateAllocDescription, FAILED_TENURE, aggressivePercolate);

		/* Global GC must be executed */
		Assert_MM_true(result);

		/* Should have been reset by globalCollectionComplete() broadcast event */
		Assert_MM_true(!failedTenureThresholdReached());
		return true;
	}

	/*
	 * Second, if the previous scavenge failed to expand tenure, ask parent MSS to try a collect.
	 */
	if (expandFailed()) {
		Trc_MM_Scavenger_percolate_expandFailed(env->getLanguageVMThread());
	
		/* We do an aggressive percolate if the last scavenge also percolated */
		uint32_t aggressivePercolate = _extensions->heap->getPercolateStats()->getScavengesSincePercolate() <= 1 ? J9MMCONSTANT_IMPLICIT_GC_PERCOLATE_AGGRESSIVE : J9MMCONSTANT_IMPLICIT_GC_PERCOLATE;

		/* Aggressive percolate the collect to parent MSS */
		bool result = percolateGarbageCollect(env, subSpace, NULL, EXPAND_FAILED, aggressivePercolate);

		/* Global GC must be executed */
		Assert_MM_true(result);

		/* Should have been reset by globalCollectionComplete() broadcast event */
		Assert_MM_true(!expandFailed());
		return true;
	}

	/* If the tenure MSS is not expandable and/or  there is insufficent space left to tenure
	 * the average number of bytes tenured by a scavenge then percolate the collect to avoid
	 * an aborted scavenge and its associated time consuming backout
	 */
	if ((tenureMemorySubSpace->maxExpansionInSpace(env) + tenureMemorySubSpace->getApproximateActiveFreeMemorySize()) < scavengerGCStats->_avgTenureBytes ) {
		Trc_MM_Scavenger_percolate_insufficientTenureSpace(env->getLanguageVMThread(), tenureMemorySubSpace->maxExpansionInSpace(env), tenureMemorySubSpace->getApproximateActiveFreeMemorySize(), scavengerGCStats->_avgTenureBytes);

		/* Percolate the collect to parent MSS */
		bool result = percolateGarbageCollect(env, subSpace, NULL, INSUFFICIENT_TENURE_SPACE, J9MMCONSTANT_IMPLICIT_GC_PERCOLATE);

		/* Global GC must be executed */
		Assert_MM_true(result);

		return true;
	}

	/* If it has been too long since a global GC, execute one instead of a scavenge. */
	//TODO Probably should rename this -Xgc option as it may not always result in a ggc
	//in futre, e.g if we implement multiple generations.
	if (_extensions->maxScavengeBeforeGlobal) {
		if (_countSinceForcingGlobalGC++ >= _extensions->maxScavengeBeforeGlobal) {
			Trc_MM_Scavenger_percolate_maxScavengeBeforeGlobal(env->getLanguageVMThread(), _extensions->maxScavengeBeforeGlobal);

			/* Percolate the collect to parent MSS */
			bool result = percolateGarbageCollect(env, subSpace, NULL, MAX_SCAVENGES, J9MMCONSTANT_IMPLICIT_GC_PERCOLATE);

			/* Global GC must be executed */
			Assert_MM_true(result);

			/* Should have been reset by globalCollectionComplete() broadcast event */
			Assert_MM_true(_countSinceForcingGlobalGC == 0);
			return true;
		}
	}

	/**
	 * Language percolation trigger	
	 * Allow the CollectorLanguageInterface to advise if percolation should occur.
	 */
	PercolateReason percolateReason = NONE_SET;
	uint32_t gcCode = J9MMCONSTANT_IMPLICIT_GC_DEFAULT;

	bool shouldPercolate = _cli->scavenger_internalGarbageCollect_shouldPercolateGarbageCollect(env, & percolateReason, & gcCode);

	if (shouldPercolate) {
		bool didPercolate = percolateGarbageCollect(env, subSpace, NULL, percolateReason, gcCode);
		/* Percolation must occur if required by the cli. */
		if (didPercolate) {
			return true;
		}
	}

	/* Check if there is an RSO and the heap is not safely walkable */
	if(isRememberedSetInOverflowState() && _extensions->scavengerRsoScanUnsafe) {
		/* NOTE: No need to set that the collect was unsuccessful - we will actually execute
		 * the scavenger after percolation.
		 */

		Trc_MM_Scavenger_percolate_rememberedSetOverflow(env->getLanguageVMThread());

		/* Percolate the collect to parent MSS */
		percolateGarbageCollect(env, subSpace, NULL, RS_OVERFLOW, J9MMCONSTANT_IMPLICIT_GC_PERCOLATE);
	}

	_extensions->heap->getPercolateStats()->incrementScavengesSincePercolate();

	_extensions->scavengerStats._gcCount += 1;
	env->_cycleState->_activeSubSpace = subSpace;
	_collectorExpandedSize = 0;

	masterThreadGarbageCollect(env);

	/* If we know now that the next scavenge will cause a peroclate broadcast
	 * the fact so other parties can react, e.g concurrrent can adjust KO threshold
	 */

	if (failedTenureThresholdReached()
		|| expandFailed()
		|| (_extensions->maxScavengeBeforeGlobal && _countSinceForcingGlobalGC == _extensions->maxScavengeBeforeGlobal)
		|| ((tenureMemorySubSpace->maxExpansionInSpace(env) + tenureMemorySubSpace->getApproximateActiveFreeMemorySize()) < scavengerGCStats->_avgTenureBytes)) {
		_extensions->scavengerStats._nextScavengeWillPercolate = true;
	}

	return true;
}
/**
 * Percolate the garbage collect to the parent memory sub space
 *
 * @param allocate descriptor describing the allocation request the collector should aim to satify
 * @param percolateReason code for the percolate
 * @param gcCode GC code requested
 * @return true if Global GC was executed, false if concurrent kickoff forced or Global GC is not possible
 */
bool
MM_Scavenger::percolateGarbageCollect(MM_EnvironmentBase *envModron,  MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, PercolateReason percolateReason, uint32_t gcCode)
{
	/* save the cycle state since we are about to call back into the collector to start a new global cycle */
	MM_CycleState *scavengeCycleState = envModron->_cycleState;
	Assert_MM_true(NULL != scavengeCycleState);
	envModron->_cycleState = NULL;

	/* Set last percolate reason */
	_extensions->heap->getPercolateStats()->setLastPercolateReason(percolateReason);

	/* Percolate the collect to parent MSS */
	bool result = subSpace->percolateGarbageCollect(envModron, allocDescription, gcCode);

	/* Reset last Percolate reason */
	_extensions->heap->getPercolateStats()->resetLastPercolateReason();

	if (result) {
		_extensions->heap->getPercolateStats()->clearScavengesSincePercolate();
	}

	/* restore the cycle state to maintain symmetry */
	Assert_MM_true(NULL == envModron->_cycleState);
	envModron->_cycleState = scavengeCycleState;
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
MM_Scavenger::heapReconfigured(MM_EnvironmentBase *env)
{
}

void
MM_Scavenger::globalCollectionStart(MM_EnvironmentBase *env)
{
	/* Hold on to allocation stats that are useful but cleared on global collects. */
	MM_ScavengerStats* scavengerStats = &_extensions->scavengerStats;
	MM_HeapStats heapStatsSemiSpace;
	MM_HeapStats heapStatsTenureSpace;

	MM_MemorySpace* space = _extensions->heap->getDefaultMemorySpace();
	Assert_MM_true(NULL != space);

	MM_MemorySubSpace* semiSpace = space->getDefaultMemorySubSpace();
	MM_MemorySubSpace* tenureSpace = space->getTenureMemorySubSpace();

	Assert_MM_true(NULL != semiSpace);
	Assert_MM_true(NULL != tenureSpace);

	semiSpace->mergeHeapStats(&heapStatsSemiSpace);
	tenureSpace->mergeHeapStats(&heapStatsTenureSpace);

	scavengerStats->_semiSpaceAllocBytesAcumulation += heapStatsSemiSpace._allocBytes;
	scavengerStats->_tenureSpaceAllocBytesAcumulation += heapStatsTenureSpace._allocBytes;
}

void
MM_Scavenger::globalCollectionComplete(MM_EnvironmentBase *env)
{
	/* A global collection has occurred so if already set clear any
	 * flags which may force a global gc on next scavenge.
	 */
	clearFailedTenureThresholdFlag();
	clearExpandFailedFlag();
	_extensions->scavengerStats._nextScavengeWillPercolate = false;
	setFailedTenureLargestObject(0);
	_countSinceForcingGlobalGC = 0;
}

void
MM_Scavenger::reportGCCycleStart(MM_EnvironmentBase *env)
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
MM_Scavenger::reportGCCycleEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_GCExtensionsBase* extensions = env->getExtensions();
	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END(
		extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_GC_POST_CYCLE_END,
		extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type,
		extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowOccured(),
		extensions->globalGCStats.workPacketStats.getSTWWorkStackOverflowCount(),
		extensions->globalGCStats.workPacketStats.getSTWWorkpacketCountAtOverflow(),
		extensions->globalGCStats.fixHeapForWalkReason,
		extensions->globalGCStats.fixHeapForWalkTime
	);
}

uintptr_t
MM_Scavenger::calculateTiltRatio()
{
	/*
	 * Calculation of tilt ratio in percents:
	 *
	 * 								New_memory_size
	 * 	tilt_ratio =  ------------------------------------------ * 100
	 * 								Nursery size
	 *
	 *  To avoid of using of uint64_t and prevent an overflow of uintptr_t on 32-bit platforms change formula to
	 *
	 * 								New_memory_size
	 * 	tilt_ratio =  --------------------------------------------------
	 * 							Nursery size / 100
	 *
	 * Quality of calculation if good enough because Nursery size is a large number (at least grater then 1K)
	 */

	/* Calculate bottom part first */
	uintptr_t tmp = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW) / 100;

	/* Size of (Total - Tenure) can not be smaller then 100 bytes */
	Assert_MM_true (tmp > 0);

	/* allocate size = nursery size - survivor size */
	uintptr_t allocateSize = _extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW) - _extensions->heap->getActiveSurvivorMemorySize(MEMORY_TYPE_NEW);
	return allocateSize / tmp;
}

void
MM_Scavenger::reportGCIncrementStart(MM_EnvironmentBase *env)
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
MM_Scavenger::reportGCIncrementEnd(MM_EnvironmentBase *env)
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
}

uintptr_t
MM_Scavenger::calculateTenureMask()
{
	/* always tenure objects which have reached the maximum age */
	uintptr_t newMask = ((uintptr_t)1 << OBJECT_HEADER_AGE_MAX);

	/* Delegate tenure mask calculations to the active strategies. */
	if (_extensions->scvTenureStrategyFixed) {
		newMask |= calculateTenureMaskUsingFixed(_extensions->scvTenureFixedTenureAge);
	}
	if (_extensions->scvTenureStrategyAdaptive) {
		newMask |= calculateTenureMaskUsingFixed(_extensions->scvTenureAdaptiveTenureAge);
	}
	if (_extensions->scvTenureStrategyLookback) {
		newMask |= calculateTenureMaskUsingLookback(_extensions->scvTenureStrategySurvivalThreshold);
	}
	if (_extensions->scvTenureStrategyHistory) {
		newMask |= calculateTenureMaskUsingHistory(_extensions->scvTenureStrategySurvivalThreshold);
	}

	return newMask;
}

uintptr_t
MM_Scavenger::calculateTenureMaskUsingLookback(double minimumSurvivalRate)
{
	Assert_MM_true(0.0 <= minimumSurvivalRate);
	Assert_MM_true(1.0 >= minimumSurvivalRate);

	MM_ScavengerStats *stats = &_extensions->scavengerStats;
	uintptr_t mask = 0;

	/* We need a normalized representation of the initial generation size over history. */

	/* We start by getting the average initial generation size. */
	double accumulatedGenerationSizes = 0.0;
	uintptr_t count = 0;
	for (uintptr_t index = 1; index < SCAVENGER_FLIP_HISTORY_SIZE; index++) {
		uintptr_t initialGenerationSize = stats->getFlipHistory(index)->_flipBytes[1] + stats->getFlipHistory(index)->_tenureBytes[1];
		if (initialGenerationSize > 0) {
			accumulatedGenerationSizes += (double)initialGenerationSize;
			count += 1;
		}
	}
	double averageInitialGenerationSize;
	if (0 == count) {
		averageInitialGenerationSize = 0;
	} else {
		averageInitialGenerationSize = accumulatedGenerationSizes / (double)count;
	}

	/* Second, we calculate the standard deviation of the initial generation size over history. */
	double accumulatedSquareDeltas = 0.0;
	for (uintptr_t index = 1; index < SCAVENGER_FLIP_HISTORY_SIZE; index++) {
		uintptr_t initialGenerationSize = stats->getFlipHistory(index)->_flipBytes[1] + stats->getFlipHistory(index)->_tenureBytes[1];
		if (initialGenerationSize > 0) {
			double delta = (double)initialGenerationSize - averageInitialGenerationSize;
			accumulatedSquareDeltas += (delta * delta);
		}
	}
	double standardDeviationOfInitialGenerationSize;
	if (0 == count) {
		standardDeviationOfInitialGenerationSize = 0;
	} else {
		standardDeviationOfInitialGenerationSize = sqrt(accumulatedSquareDeltas / (double)count);
	}

	/* This normalized initial generation size (calculated using the standard
	 * deviation) is used for determining how far back in history we should be looking.
	 * The larger this is, the shallower we look back.
	 */
	uintptr_t normalizedInitialGenerationSize = (uintptr_t)OMR_MAX(0.0, averageInitialGenerationSize - standardDeviationOfInitialGenerationSize);

	for (uintptr_t age = 0; age <= OBJECT_HEADER_AGE_MAX + 1; ++age) {
		/* skip the first row (it's the current scavenge, and is all zero right now).
		 * Also skip the last row in the history (there's no previous to compare it to)
		 */
		bool shouldTenureThisAge = true;
		uintptr_t currentGenerationBytes = stats->getFlipHistory(1)->_flipBytes[age];

		/* The lookback distance is determined by the size of the generation.
		 * We use a simple logarithmic heuristic.
		 * If the generation would occupy at least half of survivor space, only look back one collection.
		 * If the generation would occupy at least a quarter of survivor space, look back two collections.
		 * If the generation would occupy at least an eighth of survivor space, look back three collections.
		 * . . .
		 */
		const uintptr_t maximumLookback = SCAVENGER_FLIP_HISTORY_SIZE - 1;
		uintptr_t requiredLookback = 1;
		uintptr_t minimumBytesForRequiredLookback = normalizedInitialGenerationSize;
		while ((requiredLookback < maximumLookback) && (currentGenerationBytes < minimumBytesForRequiredLookback)) {
			requiredLookback += 1;
			minimumBytesForRequiredLookback /= 2;
		}

		if (requiredLookback >= age) {
			/* this generation is too young to have enough history to satisfy the lookback */
			shouldTenureThisAge = false;
		} else {
			Assert_MM_true(1 <= requiredLookback);
			Assert_MM_true(requiredLookback < SCAVENGER_FLIP_HISTORY_SIZE);
			for (uintptr_t lookback = 1; (lookback <= requiredLookback) && shouldTenureThisAge; lookback++) {
				Assert_MM_true((age+1) >= lookback);
				uintptr_t currentAgeIndex = age - lookback + 1;
				uintptr_t previousAgeIndex = age - lookback;
				uintptr_t currentFlipBytes = stats->getFlipHistory(lookback)->_flipBytes[currentAgeIndex];
				uintptr_t currentTotalBytes = currentFlipBytes + stats->getFlipHistory(lookback)->_tenureBytes[currentAgeIndex];
				uintptr_t previousFlipBytes = stats->getFlipHistory(lookback+1)->_flipBytes[previousAgeIndex];

				if (0 != previousFlipBytes) {
					if (0 == currentFlipBytes) {
						/* There are no bytes in this age, don't bother tenuring. */
						shouldTenureThisAge = false;
					} else if (((double)currentTotalBytes / (double)previousFlipBytes) < minimumSurvivalRate) {
						/* Not enough objects are surviving, don't tenure. */
						shouldTenureThisAge = false;
					}
				}
			}
		}

		if (shouldTenureThisAge) {
			/* Objects in this age are historically still dying at a rate of less than 1%. Tenure them. */
			mask |= ((uintptr_t)1 << age);
		}
	}

	return mask;
}

uintptr_t
MM_Scavenger::calculateTenureMaskUsingHistory(double minimumSurvivalRate)
{
	Assert_MM_true(0.0 <= minimumSurvivalRate);
	Assert_MM_true(1.0 >= minimumSurvivalRate);

	MM_ScavengerStats* stats = &_extensions->scavengerStats;
	uintptr_t mask = 0;

	for (uintptr_t age = 0; age < OBJECT_HEADER_AGE_MAX; ++age) {
		bool shouldTenureThisAge = true;

		/* Skip the first row in the history (it's the current scavenge, and is all zero right now).
		 * Also skip the last row in the history (there's no previous to compare it to).
		 */
		for (uintptr_t lookback = 1; lookback < SCAVENGER_FLIP_HISTORY_SIZE - 1; lookback++) {
			uintptr_t currentBytes = stats->getFlipHistory(lookback + 1)->_flipBytes[age];
			uintptr_t nextBytes = stats->getFlipHistory(lookback)->_flipBytes[age+1] + stats->getFlipHistory(lookback)->_tenureBytes[age+1];
			if (0 == currentBytes) {
				shouldTenureThisAge = false;
				break;
			} else if (((double)nextBytes / (double)currentBytes) < minimumSurvivalRate) {
				shouldTenureThisAge = false;
				break;
			}
		}

		if (shouldTenureThisAge) {
			/* Objects in this age historically survive at a rate above the minimum survival rate. Tenure them. */
			mask |= ((uintptr_t)1 << age);
		}
	}

	return mask;
}

uintptr_t
MM_Scavenger::calculateTenureMaskUsingFixed(uintptr_t tenureAge)
{
	Assert_MM_true(tenureAge <= OBJECT_HEADER_AGE_MAX);
	uintptr_t mask = 0;
	for (uintptr_t i = tenureAge; i <= OBJECT_HEADER_AGE_MAX; ++i) {
		mask |= (uintptr_t)1 << i;
	}
	return mask;
}

void 
MM_Scavenger::resetTenureLargeAllocateStats(MM_EnvironmentBase *env)
{
	MM_MemorySpace *defaultMemorySpace = _extensions->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *tenureMemorySubspace = defaultMemorySpace->getTenureMemorySubSpace();
	MM_MemoryPool *tenureMemoryPool = tenureMemorySubspace->getMemoryPool();
	tenureMemoryPool->resetLargeObjectAllocateStats();
}

#endif /* OMR_GC_MODRON_SCAVENGER */
