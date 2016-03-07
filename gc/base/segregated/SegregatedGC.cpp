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

#include "CollectionStatisticsStandard.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GlobalAllocationManagerSegregated.hpp"
#include "Heap.hpp"
#include "MarkMap.hpp"
#include "modronapicore.hpp"
#include "MemoryPoolSegregated.hpp"
#include "ParallelMarkTask.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SegregatedMarkingScheme.hpp"
#include "SegregatedSweepTask.hpp"
#include "SweepSchemeSegregated.hpp"
#include "SweepStats.hpp"
#include "WorkPackets.hpp"
#include "OMRVMInterface.hpp"
#include "SegregatedGC.hpp"

/* OMRTODO temporary workaround to allow both ut_j9mm.h and ut_omrmm.h to be included.
 *                 Dependency on ut_j9mm.h should be removed in the future.
 */
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_omrmm.h"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * Initialization
 */
MM_SegregatedGC *
MM_SegregatedGC::newInstance(MM_EnvironmentBase *env, MM_CollectorLanguageInterface *cli)
{
	MM_SegregatedGC *globalGC;

	globalGC = (MM_SegregatedGC *)env->getForge()->allocate(sizeof(MM_SegregatedGC), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != globalGC) {
		new(globalGC) MM_SegregatedGC(env, cli);
		if (!globalGC->initialize(env)) { 
			globalGC->kill(env);
			globalGC = NULL;
		}
	}
	return globalGC;
}

void
MM_SegregatedGC::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the collector's internal structures and values.
 * @return true if initialization completed, false otherwise
 */
bool
MM_SegregatedGC::initialize(MM_EnvironmentBase *env)
{
	_markingScheme = MM_SegregatedMarkingScheme::newInstance(env);
	if (NULL == _markingScheme) {
		return false;
	}

	_cli->parallelGlobalGC_setMarkingScheme(env, _markingScheme);

	_sweepScheme = MM_SweepSchemeSegregated::newInstance(env, _markingScheme->getMarkMap());
	if (NULL == _sweepScheme) {
		return false;
	}

	_sweepScheme->setClearMarkMapAfterSweep(false);
	return true;
}

/**
 * Free any internal structures associated to the receiver.
 */
void
MM_SegregatedGC::tearDown(MM_EnvironmentBase *env)
{
	if(NULL != _markingScheme) {
		_markingScheme->kill(env);
		_markingScheme = NULL;
	}

	if(NULL != _sweepScheme) {
		_sweepScheme->kill(env);
		_sweepScheme = NULL;
	}
}

bool
MM_SegregatedGC::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	return _markingScheme->heapAddRange(env, subspace, size, lowAddress, highAddress);
}

bool
MM_SegregatedGC::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	return _markingScheme->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
}

void MM_SegregatedGC::heapReconfigured(MM_EnvironmentBase* env)
{
	/* OMRTODO implement proper heap resizing in segregated heaps */
}

bool
MM_SegregatedGC::collectorStartup(MM_GCExtensionsBase* extensions)
{
	return true;
}

void
MM_SegregatedGC::collectorShutdown(MM_GCExtensionsBase *extensions)
{
}

void *
MM_SegregatedGC::createSweepPoolState(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool)
{
	/*
	 *  This function does nothing and is designed to have implementation of abstract in GlobalCollector,
	 *  but must return non-NULL value to pass initialization - so, return "this"
	 */
	return this;
}

/**
 * Request to destroy sweepPoolState class for pool
 * @param  sweepPoolState class to destroy
 */
void
MM_SegregatedGC::deleteSweepPoolState(MM_EnvironmentBase *env, void *sweepPoolState)
{

}

void
MM_SegregatedGC::setupForGC(MM_EnvironmentBase*)
{

}

void
MM_SegregatedGC::abortCollection(MM_EnvironmentBase* env, CollectionAbortReason reason)
{

}

/*
 * Garbage Collection
 */
bool
MM_SegregatedGC::internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription)
{
	env->_cycleState->_activeSubSpace->reset();
	_extensions->globalGCStats.clear();
	_extensions->globalGCStats.gcCount++;

	/*
	 * Marking
	 */
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_MarkStats *markStats = &_extensions->globalGCStats.markStats;

	/* OMRTODO the allocation contexts are never flushed for realtime, do
	 * we really need to do this here? */
	/* Flush the allocation contexts */
	MM_GlobalAllocationManager *gam = _extensions->globalAllocationManager;
	if (NULL != gam) {
		gam->flushAllocationContexts(env);
	}

	reportMarkStart(env);
	markStats->_startTime = omrtime_hires_clock();
	/* OMRTODO investigate / fix this function call */

	_markingScheme->masterSetupForGC(env);

//	if (env->_cycleState->_gcCode.isOutOfMemoryGC()) {
//		env->_cycleState->_referenceObjectOptions |= MM_CycleState::references_soft_as_weak;
//	}

	/* run the mark */
	bool initMarkMap = true; // reset the markmap?
	MM_ParallelMarkTask markTask(env, _dispatcher, _markingScheme, initMarkMap, env->_cycleState);
	_dispatcher->run(env, &markTask);

	Assert_MM_true(_markingScheme->getWorkPackets()->isAllPacketsEmpty());

	/* Do any post mark checks */
	/* OMRTODO we need to implement this function for segregated marking scheme */
//	_markingScheme->masterCleanupAfterGC(env);
	markStats->_endTime = omrtime_hires_clock();
	reportMarkEnd(env);

	/*
	 * Sweeping
	 */
	MM_SweepStats *sweepStats = &_extensions->globalGCStats.sweepStats;
	reportSweepStart(env);
	sweepStats->_startTime = omrtime_hires_clock();
	MM_SegregatedSweepTask sweepTask(env, _dispatcher, _sweepScheme, (MM_MemoryPoolSegregated *) env->getDefaultMemorySubSpace()->getMemoryPool());
	_dispatcher->run(env, &sweepTask);
	MM_MemorySubSpace *activeSubSpace = env->_cycleState->_activeSubSpace;
	bool isExplicitGC = env->_cycleState->_gcCode.isExplicitGC();
	/* We now have accurate free space statistics so recalculate any expand/contract amount */
	activeSubSpace->checkResize(env, allocDescription, isExplicitGC);
	sweepStats->_endTime = omrtime_hires_clock();
	reportSweepEnd(env);

	/* Perform the resize now based on expand/contract calculation from checkResize() (above) */
	activeSubSpace->performResize(env, allocDescription);

	/* Heap size now fixed for next cycle so reset heap statistics */
	_extensions->heap->resetHeapStatistics(true);

	/* Restart allocation caches */
	GC_OMRVMThreadListIterator vmThreadListIterator(env->getOmrVM());
	while(OMR_VMThread* thread = vmThreadListIterator.nextOMRVMThread()) {
		MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(thread);
		((MM_SegregatedAllocationInterface *)(walkEnv->_objectAllocationInterface))->restartCache(walkEnv);
	}

	return true;
}

void
MM_SegregatedGC::internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode)
{
	/* MM_GlobalCollector::internalPreCollect(env, subSpace, allocDescription, gcCode); */

	_cycleState = MM_CycleState();
	env->_cycleState = &_cycleState;
	env->_cycleState->_collectionStatistics = &_collectionStatistics;
	env->_cycleState->_gcCode = MM_GCCode(gcCode);
	env->_cycleState->_type = _cycleType;
	env->_cycleState->_activeSubSpace = subSpace;

	MM_MemoryPoolSegregated *memoryPool = (MM_MemoryPoolSegregated *) env->getDefaultMemorySubSpace()->getMemoryPool();

	/* The minimum free entry size is always re-adjusted at the end of a cycle.
	 * But if the current cycle is triggered due to OOM, at the start of the cycle
	 * set the minimum free entry size to the smallest size class.
	 */
	if (env->_cycleState->_gcCode.isOutOfMemoryGC()) {
		memoryPool->setMinimumFreeEntrySize((1 << J9VMGC_SIZECLASSES_LOG_SMALLEST));
	}

	/* OMRTODO we should check if we should fix the heap for walk here */

	/* Flush the caches for gc */
	GC_OMRVMInterface::flushCachesForGC(env);

	reportGCCycleStart(env);
	reportGCStart(env);
	reportGCIncrementStart(env);
}

void
MM_SegregatedGC::internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	MM_GlobalCollector::internalPostCollect(env, subSpace);

	/* If the mutator changed the minimum free entry size, use that */
	MM_MemoryPoolSegregated *memoryPool = (MM_MemoryPoolSegregated *) env->getDefaultMemorySubSpace()->getMemoryPool();
	if (_extensions->minimumFreeEntrySize != (uintptr_t)-1) {
		memoryPool->setMinimumFreeEntrySize(_extensions->minimumFreeEntrySize);
	}

	/* OMRTODO dynamically set the minimum free entry size. See realtime gc for reference */

#if defined(OMR_GC_OBJECT_MAP)
	_markingScheme->setLiveObjectsAsValidObjects();
#endif

	reportGCCycleFinalIncrementEnding(env);
	reportGCIncrementEnd(env);
	reportGCEnd(env);
	reportGCCycleEnd(env);
}


/*
 * Reporting
 */
void
MM_SegregatedGC::reportGCCycleFinalIncrementEnding(MM_EnvironmentBase *env)
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
MM_SegregatedGC::reportGCCycleStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_OMR_GC_CYCLE_START(
		_extensions->omrHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_OMR_GC_CYCLE_START,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		env->_cycleState->_type
	);
}

void
MM_SegregatedGC::reportGCCycleEnd(MM_EnvironmentBase *env)
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
MM_SegregatedGC::reportMarkStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkStart(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_START);
}

void
MM_SegregatedGC::reportMarkEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_MarkEnd(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_MARK_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_MARK_END);
}

void
MM_SegregatedGC::reportSweepStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepStart(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_START);
}

void
MM_SegregatedGC::reportSweepEnd(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	Trc_MM_SweepEnd(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_SWEEP_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_SWEEP_END);
}

void
MM_SegregatedGC::reportGCStart(MM_EnvironmentBase *env)
{
	uintptr_t scavengerCount = 0;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

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
MM_SegregatedGC::reportGCEnd(MM_EnvironmentBase *env)
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

	TRIGGER_J9HOOK_MM_PRIVATE_REPORT_MEMORY_USAGE(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_REPORT_MEMORY_USAGE,
		_extensions->getForge()->getCurrentStatistics()
	);

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
MM_SegregatedGC::reportGCIncrementStart(MM_EnvironmentBase *env)
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
MM_SegregatedGC::reportGCIncrementEnd(MM_EnvironmentBase *env)
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

#endif /* OMR_GC_SEGREGATED_HEAP */
