/*******************************************************************************
 * Copyright (c) 2018, 2022 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_CONCURRENT_MARK) && defined(OMR_GC_REALTIME)

#define J9_EXTERNAL_TO_VM

#include "mmprivatehook.h"
#include "modronbase.h"
#include "modronopt.h"
#include "ModronAssertions.h"
#include "omr.h"

#include <string.h>

#include "AllocateDescription.hpp"
#include "ConcurrentGCSATB.hpp"
#include "ParallelMarkTask.hpp"
#include "ConcurrentCompleteTracingTask.hpp"
#include "OMRVMInterface.hpp"
#include "ParallelDispatcher.hpp"
#include "RememberedSetSATB.hpp"
#include "WorkPacketsConcurrent.hpp"

/**
 * Create new instance of ConcurrentGCSATB object.
 *
 * @return Reference to new MM_ConcurrentGCSATB object or NULL
 */
MM_ConcurrentGCSATB *
MM_ConcurrentGCSATB::newInstance(MM_EnvironmentBase *env)
{
	MM_ConcurrentGCSATB *concurrentGC = (MM_ConcurrentGCSATB *)env->getForge()->allocate(sizeof(MM_ConcurrentGCSATB), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

	if (NULL != concurrentGC) {
		new(concurrentGC) MM_ConcurrentGCSATB(env);
		if (!concurrentGC->initialize(env)) {
			concurrentGC->kill(env);
			concurrentGC = NULL;
		}
	}

	return concurrentGC;
}

/**
 * Initialize a new MM_ConcurrentGCSATB object.
 *
 * @return TRUE if initialization completed OK;FALSE otheriwse
 */
bool
MM_ConcurrentGCSATB::initialize(MM_EnvironmentBase *env)
{
	if (!MM_ConcurrentGC::initialize(env)) {
		goto error_no_memory;
	}

	return true;

error_no_memory:
	return false;
}

/**
 * Destroy instance of an ConcurrentGCSATB object.
 *
 */
void
MM_ConcurrentGCSATB::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}


/**
 * Teardown a MM_ConcurrentGCSATB object
 * Destroy referenced objects and release
 * all allocated storage before MM_ConcurrentGCSATB object is freed.
 */
void
MM_ConcurrentGCSATB::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _extensions->sATBBarrierRememberedSet) {
		_extensions->sATBBarrierRememberedSet->kill(env);
		_extensions->sATBBarrierRememberedSet = NULL;
	}
	/* ..and then tearDown our super class */
	MM_ConcurrentGC::tearDown(env);
}

void
MM_ConcurrentGCSATB::reportConcurrentHalted(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	/* Redo Trace points for SATB */

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_HALTED(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_HALTED,
		(uintptr_t)_stats.getExecutionModeAtGC(),
		_stats.getTraceSizeTarget(),
		_stats.getTotalTraced(),
		_stats.getMutatorsTraced(),
		_stats.getConHelperTraced(),
		UDATA_MAX,
		UDATA_MAX,
		_stats.getConcurrentWorkStackOverflowOcurred(),
		_stats.getConcurrentWorkStackOverflowCount(),
		UDATA_MAX,
		_concurrentDelegate.reportConcurrentScanningMode(env),
		(uintptr_t)_markingScheme->getWorkPackets()->tracingExhausted()
	);
}

uintptr_t
MM_ConcurrentGCSATB::localMark(MM_EnvironmentBase *env, uintptr_t sizeToTrace)
{
	omrobjectptr_t objectPtr = NULL;
	uintptr_t gcCount = _extensions->globalGCStats.gcCount;

	env->_workStack.reset(env, _markingScheme->getWorkPackets());
	Assert_MM_true(NULL == env->_cycleState);
	Assert_MM_true(CONCURRENT_OFF < _stats.getExecutionMode());
	Assert_MM_true(_concurrentCycleState._referenceObjectOptions == MM_CycleState::references_default);
	env->_cycleState = &_concurrentCycleState;

	uintptr_t sizeTraced = 0;
	while (NULL != (objectPtr = (omrobjectptr_t)env->_workStack.popNoWait(env))) {
		/* Check for array scanPtr..if we find one ignore it*/
		if (((uintptr_t)objectPtr) & PACKET_ARRAY_SPLIT_TAG) {
			continue;
		} else {
			/* Else trace the object */
			sizeTraced += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_PACKET, (sizeToTrace - sizeTraced));
		}

		/* Have we done enough tracing ? */
		if (sizeTraced >= sizeToTrace) {
			break;
		}

		/* Before we do any more tracing check to see if GC is waiting */
		if (env->isExclusiveAccessRequestWaiting()) {
			/* suspend con helper thread for pending GC */
			uintptr_t conHelperRequest = switchConHelperRequest(CONCURRENT_HELPER_MARK, CONCURRENT_HELPER_WAIT);
			Assert_MM_true(CONCURRENT_HELPER_MARK != conHelperRequest);
			break;
		}
	}

	/* Pop the top of the work packet if its a partially processed array tag */
	if (((uintptr_t)((omrobjectptr_t)env->_workStack.peek(env))) & PACKET_ARRAY_SPLIT_TAG) {
		env->_workStack.popNoWait(env);
	}

	/* STW collection should not occur while localMark is working */
	Assert_MM_true(gcCount == _extensions->globalGCStats.gcCount);

	flushLocalBuffers(env);
	env->_cycleState = NULL;

	return sizeTraced;
}

/**
 * Tune the concurrent adaptive parameters.
 * Using historical data attempt to predict how much work (tracing)
 * will need to be performed during the next concurrent mark cycle.
 */
void
MM_ConcurrentGCSATB::tuneToHeap(MM_EnvironmentBase *env)
{
	MM_Heap *heap = _extensions->heap;
	uintptr_t heapSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);

	Trc_MM_ConcurrentGC_tuneToHeap_Entry(env->getLanguageVMThread());

	/* If the heap size is zero it means that we have not yet inflated the old
	 * area, and we must have been called for a nursery expansion. In this case
	 * we should return without doing anything as we will be called later when
	 * the old area expands.
	 */
	if (0 == heapSize) {
		Trc_MM_ConcurrentGC_tuneToHeap_Exit1(env->getLanguageVMThread());
		Assert_MM_true(!_stwCollectionInProgress);
		return;
	}

	/* If _kickoffThreashold is 0
	 *  then this is first time through so do initialisation,
	 *  else heap size has changed
	 *
	 * We estimate new bytes to trace based on average live rate.
	 *
	 * Note: Heap can change in size outside a global GC and after a system GC.
	 */
	if ((0 == _stats.getKickoffThreshold()) || _retuneAfterHeapResize ) {
		_bytesToTrace = (uintptr_t)(heapSize * _tenureLiveObjectFactor * _tenureNonLeafObjectFactor);
	    _retuneAfterHeapResize = false; /* just in case we have a resize before first concurrent cycle */
	} else {
		/* Re-tune based on actual amount traced if we completed tracing on last cycle */
		if ((NULL == env->_cycleState) || env->_cycleState->_gcCode.isExplicitGC() || !_stwCollectionInProgress) {
			/* Nothing to do - we can't update statistics on a system GC or when no cycle is running */
		} else if (CONCURRENT_EXHAUSTED <= _stats.getExecutionModeAtGC()) {

			uintptr_t totalTraced = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();
			_bytesToTrace = (uintptr_t)MM_Math::weightedAverage((float)_bytesToTrace, (float)totalTraced, LIVE_PART_HISTORY_WEIGHT);

		} else if (CONCURRENT_TRACE_ONLY == _stats.getExecutionModeAtGC()) {
			/* Assume amount to be traced on next cycle will what we traced this time PLUS
			 * the tracing we did to complete processing of any work packets that remained at
			 * the start of the collection.
			 * This is an over estimate but will get us back on track.
			 */
			_bytesToTrace = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount() + _stats.getCompleteTracingCount();
		} else {
			/* We did not trace enough to use amount traced to predict trace target so best we can do
			 * is estimate based on current heap size, live object factor, leaf object factor etc.
			 */
			_bytesToTrace = (uintptr_t)(heapSize * _tenureLiveObjectFactor * _tenureNonLeafObjectFactor);
		}
	}

	recalculateInitWork(env);

	/* Reset trace rate for next concurrent cycle */
	_allocToTraceRate = _allocToTraceRateNormal;

	_traceTarget = _bytesToTrace;

	_stats.setTraceSizeTarget(_bytesToTrace);

	/* Calculate the KO point for concurrent. As we trace at different rates during the
	 * initialization and marking phases we need to allow for that when calculating
	 * the KO point.
	 */
	uintptr_t kickoffThreshold = (_stats.getInitWorkRequired() / _allocToInitRate) +
					   (_bytesToTrace / _allocToTraceRateNormal);


	/* We need to ensure that we complete tracing just before we run out of
	 * storage otherwise we will more than likely get an AF whilst last few allocates
	 * are paying finishing off the last bit of tracing. So we create a buffer zone
	 * by bringing forward the KO threshold. We remember by how much so we can
	 * make the necessary adjustments to calculations in calculateTraceSize().
	 *
	 * Two factors are applied to increase the thresholds:
	 *  1) a boost factor (10% of the kickoff threshold) is applied to both thresholds
	 *  2) a user-specified slack value (in bytes) is added proportionally to each threshold
	 *
	 * e.g. if kickoffThreshold = 10M, cardCleaningThreshold = 2M and concurrentSlack = 100M
	 *  1) the boost will be 1M (10% of 10M)
	 *  2) the kickoff slack will be 100M
	 *  3) the cardcleaning slack will be 20M (100M * (10M / 2M))
	 *  resulting in a final kickoffThreshold = 111M and a cardCleaningThreshold = 23M
	 */
	float boost = ((float)kickoffThreshold * CONCURRENT_KICKOFF_THRESHOLD_BOOST) - (float)kickoffThreshold;
	float kickoffProportion = 1.0;

	uintptr_t kickoffThresholdPlusBuffer = (uintptr_t)((float)kickoffThreshold + boost + ((float)_extensions->concurrentSlack * kickoffProportion));
	_stats.setKickoffThreshold(kickoffThresholdPlusBuffer);

	_kickoffThresholdBuffer = MM_Math::saturatingSubtract(kickoffThresholdPlusBuffer, kickoffThreshold);

	if (_extensions->debugConcurrentMark) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("Tune to heap SATB: Trace target=\"%zu\"\n", _bytesToTrace);
		omrtty_printf("               KO threshold=\"%zu\" KO threshold buffer=\"%zu\"\n",
							 _stats.getKickoffThreshold(), _kickoffThresholdBuffer);
		omrtty_printf("               Init Work Required=\"%zu\" \n",
							_stats.getInitWorkRequired());
	}

	resetConcurrentParameters(env);

	Trc_MM_ConcurrentGC_tuneToHeap_Exit2(env->getLanguageVMThread(), _stats.getTraceSizeTarget(), _stats.getInitWorkRequired(), _stats.getKickoffThreshold());
}

/**
 * Adjust the current trace target after heap change.
 * The heap has been reconfigured (i.e expanded or contracted) midway through a
 * concurrent cycle so we need to re-calculate the trace  target so the trace
 * ate is adjusted accordingly on subsequent allocations.
 */
void
MM_ConcurrentGCSATB::adjustTraceTarget()
{
	MM_Heap *heap = _extensions->heap;
	uintptr_t heapSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);

	/* Reset bytes to trace based on new heap size and the average live rate */
	_bytesToTrace = (uintptr_t)(heapSize * _tenureLiveObjectFactor * _tenureNonLeafObjectFactor);

	_stats.setTraceSizeTarget(_bytesToTrace);
}

void
MM_ConcurrentGCSATB::setupForConcurrent(MM_EnvironmentBase *env)
{
	GC_OMRVMInterface::flushCachesForGC(env);

	enableSATB(env);

	_extensions->newThreadAllocationColor = GC_MARK;
	_concurrentDelegate.setupClassScanning(env);

	MM_ParallelMarkTask markRootsTask(env, _dispatcher, _markingScheme, false, env->_cycleState, MM_ParallelMarkTask::MARK_ROOTS);
	_dispatcher->run(env, &markRootsTask);

	/* The mark task flushed this thread's work stack, which also NULLs the thread local reference to the work packets.
	 * We must set it again as this thread will need it to, it will continue to do another loop for paying tax after exiting here (taxPaid is not set in this path, CONCURRENT_INIT_COMPLETE state).  */
	env->_workStack.prepareForWork(env, _markingScheme->getWorkPackets());

	setThreadsScanned(env);
	_stats.switchExecutionMode(CONCURRENT_INIT_COMPLETE, CONCURRENT_TRACE_ONLY);
}

void
MM_ConcurrentGCSATB::enableSATB(MM_EnvironmentBase *env) {
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	/* Activate SATB Write Barrier */
	_extensions->sATBBarrierRememberedSet->restoreGlobalFragmentIndex(env);

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_SATB_TOGGLED(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_SATB_TOGGLED,
		true);
}

void
MM_ConcurrentGCSATB::disableSATB(MM_EnvironmentBase *env) {
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	/* Deactivate barrier */
	_extensions->sATBBarrierRememberedSet->preserveGlobalFragmentIndex(env);

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_SATB_TOGGLED(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_SATB_TOGGLED,
		false);
}

uintptr_t
MM_ConcurrentGCSATB::doConcurrentTrace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t sizeToTrace, MM_MemorySubSpace *subspace, bool threadAtSafePoint)
{
	uintptr_t sizeTraced = 0;
	uintptr_t sizeTracedPreviously = (uintptr_t)-1;
	uintptr_t remainingFree;

	/* Determine how much "taxable" free space remains to be allocated. */
#if defined(OMR_GC_MODRON_SCAVENGER)
	if(_extensions->scavengerEnabled) {
		remainingFree = MM_ConcurrentGC::potentialFreeSpace(env, allocDescription);
	} else
#endif /* OMR_GC_MODRON_SCAVENGER */
	{
		MM_MemoryPool *pool = allocDescription->getMemoryPool();
		/* in the case of Tarok phase4, the pool from the allocation description doesn't have enough context to help guide concurrent decisions so we need the parent */
		MM_MemoryPool *targetPool = pool->getParent();
		if (NULL == targetPool) {
			/* if pool is the top-level, however, it is best for the job */
			targetPool = pool;
		}
		remainingFree = targetPool->getApproximateFreeMemorySize();
	}

	Assert_MM_true(env->isThreadScanned());

	if (periodicalTuningNeeded(env,remainingFree)) {
		periodicalTuning(env, remainingFree);

		Assert_MM_true(_markingScheme->getWorkPackets()->getDeferredPacketCount() == 0);
	}

	uintptr_t bytesTraced = 0;
	bool completedConcurrentScanning = false;
	if (_concurrentDelegate.startConcurrentScanning(env, &bytesTraced, &completedConcurrentScanning)) {
		if (completedConcurrentScanning) {
			resumeConHelperThreads(env);
		}
		flushLocalBuffers(env);
		Trc_MM_concurrentClassMarkEnd(env->getLanguageVMThread(), sizeTraced);
		_concurrentDelegate.concurrentScanningStarted(env, bytesTraced);
		sizeTraced += bytesTraced;
	}

	/* Loop marking  until we have paid all our tax, another thread requests exclusive VM access to do a gc, or another thread has switched to exhausted */
	while ((!env->isExclusiveAccessRequestWaiting()) && (sizeTraced < sizeToTrace) &&
			(sizeTraced != sizeTracedPreviously) && (CONCURRENT_TRACE_ONLY >= _stats.getExecutionMode())
	) {

		sizeTracedPreviously = sizeTraced;

		/* Scan objects until there are no more, the trace size has been achieved, or gc is waiting */
		uintptr_t bytesTraced = localMark(env,(sizeToTrace - sizeTraced));
		if (bytesTraced > 0) {
			/* Update global count of amount  traced */
			_stats.incTraceSizeCount(bytesTraced);
			/* ..and local count */
			sizeTraced += bytesTraced;
		}

		/* TODO: If there's no work and we haven't scanned enough, then consider flushing/scanning the threads in use barrier packet. */
		if (sizeTraced < sizeToTrace) {
			/* Must be no local work left at this point! */
			Assert_MM_true(!env->_workStack.inputPacketAvailable());
			Assert_MM_true(!env->_workStack.outputPacketAvailable());
			Assert_MM_true(!env->_workStack.deferredPacketAvailable());

			if ((((MM_WorkPacketsSATB *)_markingScheme->getWorkPackets())->effectiveTraceExhausted())) {
				break;
			} else if (!env->isExclusiveAccessRequestWaiting()) {
				resumeConHelperThreads(env);
			}
		}
	} /* of while sizeTraced < sizeToTrace */

	/* If no more work left (and concurrent scanning is complete or disabled) then switch to exhausted now */
	if ((((MM_WorkPacketsSATB *)_markingScheme->getWorkPackets())->effectiveTraceExhausted()) && _concurrentDelegate.isConcurrentScanningComplete(env)) {
		if(_stats.switchExecutionMode(CONCURRENT_TRACE_ONLY, CONCURRENT_EXHAUSTED)) {
			/* Tell all MSS to use slow path allocate and so get to a safe  point before paying allocation tax. */
			subspace->setAllocateAtSafePointOnly(env, true);
		}
	}

	/* If there is work available on input lists then notify any waiting concurrent helpers */
	if ((!env->isExclusiveAccessRequestWaiting()) && (_markingScheme->getWorkPackets()->inputPacketAvailable(env))) {
		resumeConHelperThreads(env);
	}

	/* Must be no local work left at this point! */
	Assert_MM_true(!env->_workStack.inputPacketAvailable());
	Assert_MM_true(!env->_workStack.outputPacketAvailable());
	Assert_MM_true(!env->_workStack.deferredPacketAvailable());

	return sizeTraced;
}

void
MM_ConcurrentGCSATB::completeConcurrentTracing(MM_EnvironmentBase *env, uintptr_t executionModeAtGC)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	/* Flush barrier packets */
	if (((MM_WorkPacketsSATB *)_markingScheme->getWorkPackets())->inUsePacketsAvailable(env)) {
			((MM_WorkPacketsSATB *)_markingScheme->getWorkPackets())->moveInUseToNonEmpty(env);
			_extensions->sATBBarrierRememberedSet->flushFragments(env);
	}

	disableSATB(env);

	_extensions->newThreadAllocationColor = GC_UNMARK;

	if (CONCURRENT_FINAL_COLLECTION > executionModeAtGC) {
		reportConcurrentHalted(env);
	}

	/* Get assistance from all worker threads to complete processing of any remaining work packets.*/
	if (!_markingScheme->getWorkPackets()->isAllPacketsEmpty()) {
		reportConcurrentCompleteTracingStart(env);
		uint64_t startTime = omrtime_hires_clock();
		MM_ConcurrentCompleteTracingTask completeTracingTask(env, _dispatcher, this, env->_cycleState);
		_dispatcher->run(env, &completeTracingTask);
		reportConcurrentCompleteTracingEnd(env, omrtime_hires_clock() - startTime);
	}

	Assert_MM_true(_markingScheme->getWorkPackets()->isAllPacketsEmpty());
}

void
MM_ConcurrentGCSATB::setThreadsScanned(MM_EnvironmentBase *env)
{
	GC_OMRVMThreadListIterator vmThreadListIterator(env->getOmrVMThread());
	OMR_VMThread *walkThread;
	while ((walkThread = vmThreadListIterator.nextOMRVMThread()) != NULL) {
		MM_EnvironmentBase *walkEnv = MM_EnvironmentBase::getEnvironment(walkThread);
		walkEnv->setAllocationColor(GC_MARK);
		walkEnv->setThreadScanned(true);
	}
}

void
MM_ConcurrentGCSATB::reportConcurrentCollectionStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	/* For now, use Incremental Trace Point - Pass UDATA_MAX as card cleaned/threshold  */
	Trc_MM_ConcurrentCollectionStart(env->getLanguageVMThread(),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
		(_extensions-> largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0 ),
		(_extensions-> largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0 ),
		_stats.getTraceSizeTarget(),
		_stats.getTotalTraced(),
		_stats.getMutatorsTraced(),
		_stats.getConHelperTraced(),
		UDATA_MAX,
		UDATA_MAX,
		(_stats.getConcurrentWorkStackOverflowOcurred() ? "true" : "false"),
		_stats.getConcurrentWorkStackOverflowCount()
	);

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

	if (J9_EVENT_IS_HOOKED(_extensions->privateHookInterface, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START)) {
		MM_CommonGCStartData commonData;
		_extensions->heap->initializeCommonGCStartData(env, &commonData);

		ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START(
			_extensions->privateHookInterface,
			env->getOmrVMThread(),
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_START,
			_concurrentCycleState._verboseContextID,
			&commonData,
			_stats.getTraceSizeTarget(),
			_stats.getTotalTraced(),
			_stats.getMutatorsTraced(),
			_stats.getConHelperTraced(),
			UDATA_MAX,
			UDATA_MAX,
			_stats.getConcurrentWorkStackOverflowOcurred(),
			_stats.getConcurrentWorkStackOverflowCount(),
			_stats.getThreadsToScanCount(),
			_stats.getThreadsScannedCount(),
			UDATA_MAX
		);
	}
}

/**
 * TLH is about to be cleared, we must premark it if SATB is active. This is
 * non-trival to do, we must know the start of the last object to mark the proper range of the TLH.
 * Arbitrary bits at the end of the TLH can’t be marked, this is problematic for sweep.
 *
 * To get around this issue we seal the TLH with a bogus/dummy obj at the
 * end of the TLH (done before call to this method). With this, we know the precise addr to batch mark to. We have
 * guaranteed that we have at least min obj size available in the TLH by reservering it when the TLH was initially allocated.
 *
 * @param base The base address of the cache
 * @param top  The start of the last obj in the cache
 */
void
MM_ConcurrentGCSATB::preAllocCacheFlush(MM_EnvironmentBase *env, void *base, void *top) {
	Assert_MM_true(_extensions->isSATBBarrierActive());

	uintptr_t lastTLHobjSize = _extensions->objectModel.getConsumedSizeInBytesWithHeader((omrobjectptr_t)top);
	Assert_MM_true(OMR_MINIMUM_OBJECT_SIZE == lastTLHobjSize);

	/* Mark all newly allocated objects */
	_markingScheme->markObjectsForRange(env, (uint8_t *)base, (uint8_t *)top);
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void
MM_ConcurrentGCSATB::objectTenured(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	Assert_MM_true(CONCURRENT_OFF != _stats.getExecutionMode());
	Assert_MM_true(_extensions->isOld(objectPtr));

	_concurrentDelegate.rememberObjectToRescan(env, objectPtr);
}
#endif /* OMR_GC_MODRON_SCAVENGER */
#endif /* OMR_GC_MODRON_CONCURRENT_MARK && OMR_GC_REALTIME */
