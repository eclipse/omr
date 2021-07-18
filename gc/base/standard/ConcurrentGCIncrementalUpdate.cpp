/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#define J9_EXTERNAL_TO_VM

#include "mmprivatehook.h"
#include "modronbase.h"
#include "modronopt.h"
#include "ModronAssertions.h"
#include "omr.h"

#include <string.h>

#include "AllocateDescription.hpp"
#include "ConcurrentGCIncrementalUpdate.hpp"
#include "ConcurrentGC.hpp"
#include "ConcurrentCompleteTracingTask.hpp"
#include "ConcurrentFinalCleanCardsTask.hpp"
#include "ConcurrentCardTableForWC.hpp"
#include "ParallelDispatcher.hpp"
#include "SpinLimiter.hpp"
#include "WorkPacketsConcurrent.hpp"

extern "C" {

/**
 * Concurrent mark write barrier
 * External entry point for write barrier. Calls card table function to dirty card for
 * given object which has had an object reference (or multiple references) modified.
 *
 * @param destinationObject Address of object containing modified reference
 */
void
concurrentPostWriteBarrierStore(OMR_VMThread *vmThread, omrobjectptr_t destinationObject)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();

	extensions->cardTable->dirtyCard(env, (omrobjectptr_t)destinationObject);
}

} /* extern "C" */

void
MM_ConcurrentGCIncrementalUpdate::reportConcurrentHalted(MM_EnvironmentBase *env)
{
	MM_ConcurrentCardTable *cardTable = (MM_ConcurrentCardTable *)_cardTable;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentHalted(env->getLanguageVMThread(),
		(uintptr_t) _stats.getExecutionModeAtGC(),
		_stats.getTraceSizeTarget(),
		_stats.getTotalTraced(),
		_stats.getMutatorsTraced(),
		_stats.getConHelperTraced(),
		cardTable->getCardTableStats()->getConcurrentCleanedCards(),
		_stats.getCardCleaningThreshold(),
		(_stats.getConcurrentWorkStackOverflowOcurred() ? "true" : "false"),
		_stats.getConcurrentWorkStackOverflowCount()
	);

	Trc_MM_ConcurrentHaltedState(env->getLanguageVMThread(),
		cardTable->isCardCleaningComplete() ? "complete" : "incomplete",
		_concurrentDelegate.isConcurrentScanningComplete(env) ? "complete" : "incomplete",
		_markingScheme->getWorkPackets()->tracingExhausted() ? "complete" : "incomplete"
	);

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
		cardTable->getCardTableStats()->getConcurrentCleanedCards(),
		_stats.getCardCleaningThreshold(),
		_stats.getConcurrentWorkStackOverflowOcurred(),
		_stats.getConcurrentWorkStackOverflowCount(),
		(uintptr_t)cardTable->isCardCleaningComplete(),
		_concurrentDelegate.reportConcurrentScanningMode(env),
		(uintptr_t)_markingScheme->getWorkPackets()->tracingExhausted()
	);
}

void
MM_ConcurrentGCIncrementalUpdate::reportConcurrentCollectionStart(MM_EnvironmentBase *env)
{
	MM_ConcurrentCardTable *cardTable = (MM_ConcurrentCardTable *)_cardTable;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

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
		cardTable->getCardTableStats()->getConcurrentCleanedCards(),
		_stats.getCardCleaningThreshold(),
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
			cardTable->getCardTableStats()->getConcurrentCleanedCards(),
			_stats.getCardCleaningThreshold(),
			_stats.getConcurrentWorkStackOverflowOcurred(),
			_stats.getConcurrentWorkStackOverflowCount(),
			_stats.getThreadsToScanCount(),
			_stats.getThreadsScannedCount(),
			_stats.getCardCleaningReason()
		);
	}
}

void
MM_ConcurrentGCIncrementalUpdate::reportConcurrentFinalCardCleaningStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentCollectionCardCleaningStart(env->getLanguageVMThread(),
		_stats.getConcurrentWorkStackOverflowCount()
	);
	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_START,
		_stats.getConcurrentWorkStackOverflowCount()
	);
}

void
MM_ConcurrentGCIncrementalUpdate::reportConcurrentFinalCardCleaningEnd(MM_EnvironmentBase *env, uint64_t duration)
{
	MM_ConcurrentCardTable *cardTable = (MM_ConcurrentCardTable *)_cardTable;
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentCollectionCardCleaningEnd(env->getLanguageVMThread());
	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_CARD_CLEANING_END,
		duration,
		cardTable->getCardTableStats()->getFinalCleanedCardsPhase1(),
		cardTable->getCardTableStats()->getFinalCleanedCardsPhase2(),
		cardTable->getCardTableStats()->getFinalCleanedCards(),
		_stats.getFinalTraceCount() + _stats.getFinalCardCleanCount(),
		cardTable->getCardTableStats()->getConcurrentCleanedCardsPhase1(),
		cardTable->getCardTableStats()->getConcurrentCleanedCardsPhase2(),
		cardTable->getCardTableStats()->getConcurrentCleanedCardsPhase3(),
		cardTable->getCardTableStats()->getConcurrentCleanedCards(),
		_stats.getCardCleaningThreshold(),
		cardTable->getCardTableStats()->getCardCleaningPhase1Kickoff(),
		cardTable->getCardTableStats()->getCardCleaningPhase2Kickoff(),
		cardTable->getCardTableStats()->getCardCleaningPhase3Kickoff(),
		_stats.getConcurrentWorkStackOverflowCount()
	);
}

/**
 * Create new instance of ConcurrentGCIncrementalUpdate object.
 *
 * @return Reference to new MM_ConcurrentGCIncrementalUpdate object or NULL
 */
MM_ConcurrentGCIncrementalUpdate *
MM_ConcurrentGCIncrementalUpdate::newInstance(MM_EnvironmentBase *env)
{
	MM_ConcurrentGCIncrementalUpdate *concurrentGC =
			(MM_ConcurrentGCIncrementalUpdate *)env->getForge()->allocate(sizeof(MM_ConcurrentGCIncrementalUpdate),
					OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != concurrentGC) {
		new(concurrentGC) MM_ConcurrentGCIncrementalUpdate(env);
		if (!concurrentGC->initialize(env)) {
			concurrentGC->kill(env);
			concurrentGC = NULL;
		}
	}

	return concurrentGC;
}

/**
 * Destroy instance of an ConcurrentGCIncrementalUpdate object.
 *
 */
void
MM_ConcurrentGCIncrementalUpdate::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}


/* Teardown a ConcurrentGCIncrementalUpdate object
* Destroy referenced objects and release
* all allocated storage before ConcurrentGCIncrementalUpdate object is freed.
*/
void
MM_ConcurrentGCIncrementalUpdate::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _cardTable){
		_cardTable->kill(env);
		_cardTable= NULL;
		_extensions->cardTable = NULL;
	}

	/* ..and then tearDown our super class */
	MM_ConcurrentGC::tearDown(env);
}

/**
 * Initialize a new ConcurrentGCIncrementalUpdate object.
 * Instantiate card table and concurrent RAS objects(if required) and initialize all
 * monitors required by concurrent. Allocate and initialize the concurrent helper thread
 * table.
 *
 * @return TRUE if initialization completed OK;FALSE otheriwse
 */
bool
MM_ConcurrentGCIncrementalUpdate::initialize(MM_EnvironmentBase *env)
{
	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);

	if (!MM_ConcurrentGC::initialize(env)) {
		goto error_no_memory;
	}

	if (!createCardTable(env)) {
		goto error_no_memory;
	}

	/* Register on any hook we are interested in */
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CARD_CLEANING_PASS_2_START, hookCardCleanPass2Start, OMR_GET_CALLSITE(), (void *)this);

	_secondCardCleanPass = (_extensions->cardCleaningPasses == 2) ? true : false;
	_allocToTraceRateCardCleanPass2Boost = _extensions->cardCleanPass2Boost;

	_cardCleaningFactorPass1 = MM_ConcurrentGC::interpolateInRange(INITIAL_CARD_CLEANING_FACTOR_PASS1_1,
												INITIAL_CARD_CLEANING_FACTOR_PASS1_8,
												INITIAL_CARD_CLEANING_FACTOR_PASS1_10,
												_allocToTraceRateNormal);

	_maxCardCleaningFactorPass1 = MM_ConcurrentGC::interpolateInRange(MAX_CARD_CLEANING_FACTOR_PASS1_1,
												MAX_CARD_CLEANING_FACTOR_PASS1_8,
												MAX_CARD_CLEANING_FACTOR_PASS1_10,
												_allocToTraceRateNormal);

	_bytesTracedInPass1Factor = ALL_BYTES_TRACED_IN_PASS_1;

	if (_secondCardCleanPass) {
		_cardCleaningFactorPass2 = MM_ConcurrentGC::interpolateInRange(INITIAL_CARD_CLEANING_FACTOR_PASS2_1,
													INITIAL_CARD_CLEANING_FACTOR_PASS2_8,
													INITIAL_CARD_CLEANING_FACTOR_PASS2_10,
													_allocToTraceRateNormal);

		_maxCardCleaningFactorPass2 = MM_ConcurrentGC::interpolateInRange(MAX_CARD_CLEANING_FACTOR_PASS2_1,
													MAX_CARD_CLEANING_FACTOR_PASS2_8,
													MAX_CARD_CLEANING_FACTOR_PASS2_10,
													_allocToTraceRateNormal);
	} else {
		_cardCleaningFactorPass2 = 0;
		_maxCardCleaningFactorPass2 = 0;
	}

	_cardCleaningThresholdFactor = MM_ConcurrentGC::interpolateInRange(CARD_CLEANING_THRESHOLD_FACTOR_1,
													CARD_CLEANING_THRESHOLD_FACTOR_8,
													CARD_CLEANING_THRESHOLD_FACTOR_10,
													_allocToTraceRateNormal);

	if (_extensions->debugConcurrentMark) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("Initial tuning statistics: Card Cleaning Factors Pass1=\"%.3f\" Pass2=\"%.3f\" (Maximum: Pass1=\"%.3f\" Pass2=\"%.3f\")\n",
							_cardCleaningFactorPass1, _cardCleaningFactorPass2, _maxCardCleaningFactorPass1, _maxCardCleaningFactorPass2);
		omrtty_printf("                           Card Cleaning Threshold Factor=\"%.3f\"\n",
							_cardCleaningThresholdFactor);
		omrtty_printf("                           Allocate to trace Rate Factors Minimum=\"%f\" Maximum=\"%f\"\n",
							_allocToTraceRateMinFactor, _allocToTraceRateMaxFactor);
	}

	return true;

error_no_memory:
	return false;
}

bool
MM_ConcurrentGCIncrementalUpdate::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool clearCards = (CONCURRENT_OFF < _stats.getExecutionMode()) && subspace->isConcurrentCollectable();

	/* Expand any superclass structures including mark bits*/
	bool result = MM_ConcurrentGC::heapAddRange(env, subspace, size, lowAddress, highAddress);

	if (result) {
		/* expand the card table */
		result = ((MM_ConcurrentCardTable *)_cardTable)->heapAddRange(env, subspace, size, lowAddress, highAddress, clearCards);
		if (!result) {
			/* Expansion of Concurrent Card Table has failed
			 * ParallelGlobalGC expansion must be reversed
			 */
			MM_ParallelGlobalGC::heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
		}
	}

	_heapAlloc = _extensions->heap->getHeapTop();

	return result;
}

bool
MM_ConcurrentGCIncrementalUpdate::contractInternalConcurrentStructures(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	/* Contract the card table */
	return ((MM_ConcurrentCardTable *)_cardTable)->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
}

/**
 * Hook function called when an the 2nd pass over card table to clean cards starts.
 * This is a wrapper into the non-static MM_ConcurrentGCIncrementalUpdate::recordCardCleanPass2Start
 */
void
MM_ConcurrentGCIncrementalUpdate::hookCardCleanPass2Start(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_CardCleanPass2StartEvent* event = (MM_CardCleanPass2StartEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_ConcurrentGCIncrementalUpdate *)userData)->recordCardCleanPass2Start(env);

	/* Boost the trace rate for the 2nd card cleaning pass */
}

void
MM_ConcurrentGCIncrementalUpdate::recordCardCleanPass2Start(MM_EnvironmentBase *env)
{
	_pass2Started = true;

	/* Record how mush work we did before pass 2 KO */
	_totalTracedAtPass2KO = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();
	_totalCleanedAtPass2KO = _stats.getCardCleanCount() + _stats.getConHelperCardCleanCount();

	/* ..and boost tracing rate from here to end of cycle so we complete pass 2 ASAP */
	_allocToTraceRate *= _allocToTraceRateCardCleanPass2Boost;
}

bool
MM_ConcurrentGCIncrementalUpdate::createCardTable(MM_EnvironmentBase *env)
{
	bool result = false;

	Assert_MM_true(NULL == _cardTable);
	Assert_MM_true(NULL == _extensions->cardTable);

#if defined(AIXPPC) || defined(LINUXPPC)
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if ((uintptr_t)omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_ONLINE) > 1 ) {
		_cardTable = MM_ConcurrentCardTableForWC::newInstance(env, _extensions->getHeap(), _markingScheme, this);
	} else
#endif /* AIXPPC || LINUXPPC */
	{
		_cardTable = MM_ConcurrentCardTable::newInstance(env, _extensions->getHeap(), _markingScheme, this);
	}

	if(NULL != _cardTable) {
		result = true;
		/* Set card table address in GC Extensions */
		_extensions->cardTable = _cardTable;
	}

	return result;
}

#if defined(OMR_GC_MODRON_SCAVENGER)
/**
 * Process event from an external GC (Scavenger) when old-to-old reference is created
 * from an existing old-to-new or new-to-new reference.
 * We need to rescan the parent object, as slots which during original scan contained
 * new area references could now contain references into old area.
 * @param objectPtr  Parent old object that has a reference to a child old object
 */
void
MM_ConcurrentGCIncrementalUpdate::oldToOldReferenceCreated(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	/* todo: Consider doing mark and push-to-scan on child object.
	 * Child object will be tenured only once during a Scavenge cycle,
	 * so there are no risks of creating duplicates in scan queue. */
	Assert_MM_true(CONCURRENT_OFF != _stats.getExecutionMode());
	Assert_MM_true(_extensions->isOld(objectPtr));
	if (_markingScheme->isMarkedOutline(objectPtr) ) {
		_cardTable->dirtyCard(env,objectPtr);
	}
}
#endif /* OMR_GC_MODRON_SCAVENGER */

void
MM_ConcurrentGCIncrementalUpdate::finalConcurrentPrecollect(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	reportConcurrentFinalCardCleaningStart(env);
	uint64_t startTime = omrtime_hires_clock();

	bool overflow = false; /* assume the worst case*/
	uintptr_t overflowCount;

	do {
		/* remember count when we start */
		overflowCount = _stats.getConcurrentWorkStackOverflowCount();

		/* Get assistance from all worker threads to do final card cleaning */
		MM_ConcurrentFinalCleanCardsTask cleanCardsTask(env, _dispatcher, this, env->_cycleState);
		((MM_ConcurrentCardTable *)_cardTable)->initializeFinalCardCleaning(env);

		_dispatcher->run(env, &cleanCardsTask);

		/* Have we had a work stack overflow whilst processing card table ? */
		overflow = (overflowCount != _stats.getConcurrentWorkStackOverflowCount());
	} while (overflow);

	/* reset overflow flag */
	_markingScheme->getWorkPackets()->clearOverflowFlag();

	reportConcurrentFinalCardCleaningEnd(env, omrtime_hires_clock() - startTime);
#if defined(DEBUG)
	Assert_MM_true(_cardTable->isCardTableEmpty(env));
#endif
}

/**
 * Tune the concurrent adaptive parameters.
 * Using historical data attempt to predict how much work (both tracing and
 * card cleaning) will need to be performed during the next concurrent mark cycle.
 */
void
MM_ConcurrentGCIncrementalUpdate::tuneToHeap(MM_EnvironmentBase *env)
{
	MM_Heap *heap = (MM_Heap *)_extensions->heap;
	uintptr_t heapSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);
	uintptr_t kickoffThreshold;
	uintptr_t cardCleaningThreshold;
	uintptr_t kickoffThresholdPlusBuffer;
	uintptr_t totalBytesToTrace, totalBytesToClean;

	Trc_MM_ConcurrentGC_tuneToHeap_Entry(env->getLanguageVMThread());

	/* If the heap size is zero it means that we have not yet inflated the old
	 * area, and we must have been called for a nursery expansion. In this case
	 * we should return without doing anything as we will be called later when
	 * the old area expands.
	 */
	if(0 == heapSize) {
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
	if(0 == _stats.getKickoffThreshold() || _retuneAfterHeapResize ) {
		totalBytesToTrace = (uintptr_t)(heapSize * _tenureLiveObjectFactor * _tenureNonLeafObjectFactor);
		_bytesToTracePass1 = (uintptr_t)((float)totalBytesToTrace * _bytesTracedInPass1Factor);
		_bytesToTracePass2 = MM_Math::saturatingSubtract(totalBytesToTrace, _bytesToTracePass1);
		_bytesToCleanPass1 = (uintptr_t)((float)totalBytesToTrace * _cardCleaningFactorPass1);
		_bytesToCleanPass2 = (uintptr_t)((float)totalBytesToTrace * _cardCleaningFactorPass2);
		_retuneAfterHeapResize = false; /* just in case we have a resize before first concurrent cycle */
	} else {
		/* Re-tune based on actual amount traced if we completed tracing on last cycle */
		if ((NULL == env->_cycleState) || env->_cycleState->_gcCode.isExplicitGC() || !_stwCollectionInProgress) {
			/* Nothing to do - we can't update statistics on a system GC or when no cycle is running */
		} else if (CONCURRENT_EXHAUSTED <= _stats.getExecutionModeAtGC()) {

			uintptr_t totalTraced = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();
			uintptr_t totalCleaned = _stats.getCardCleanCount() + _stats.getConHelperCardCleanCount();

			if (_secondCardCleanPass) {
				assume0(_totalCleanedAtPass2KO != HIGH_VALUES);
				uintptr_t bytesTracedPass1 = _totalTracedAtPass2KO;
				uintptr_t bytesTracedPass2 = MM_Math::saturatingSubtract(totalTraced, _totalTracedAtPass2KO);
				uintptr_t bytesCleanedPass1 = _totalCleanedAtPass2KO;
				uintptr_t bytesCleanedPass2 = MM_Math::saturatingSubtract(totalCleaned, _totalCleanedAtPass2KO);

				_bytesToTracePass1 = (uintptr_t)MM_Math::weightedAverage((float)_bytesToTracePass1, (float)bytesTracedPass1, LIVE_PART_HISTORY_WEIGHT);
				_bytesToTracePass2 = (uintptr_t)MM_Math::weightedAverage((float)_bytesToTracePass2, (float)bytesTracedPass2, LIVE_PART_HISTORY_WEIGHT);
				_bytesToCleanPass1 = (uintptr_t)MM_Math::weightedAverage((float)_bytesToCleanPass1, (float)bytesCleanedPass1, CARD_CLEANING_HISTORY_WEIGHT);
				_bytesToCleanPass2 = (uintptr_t)MM_Math::weightedAverage((float)_bytesToCleanPass2, (float)bytesCleanedPass2, CARD_CLEANING_HISTORY_WEIGHT);
			} else {
				_bytesToTracePass1 = (uintptr_t)MM_Math::weightedAverage((float)_bytesToTracePass1, (float)totalTraced, LIVE_PART_HISTORY_WEIGHT);
				_bytesToCleanPass1 = (uintptr_t)MM_Math::weightedAverage((float)_bytesToCleanPass1, (float)totalCleaned, CARD_CLEANING_HISTORY_WEIGHT);
				_bytesToTracePass2 = 0;
				_bytesToCleanPass2 = 0;

			}
		} else if (_stats.getExecutionModeAtGC() == CONCURRENT_CLEAN_TRACE) {
			/* Assume amount to be traced on next cycle will what we traced this time PLUS
			 * the tracing we did to complete processing of any work packets that remained at
			 * the start of the collection PLUS tracing done during final card cleaning.
			 * This is an over estimate but will get us back on track.
			 */
			 totalBytesToTrace = _stats.getTraceSizeCount() +
			 					 _stats.getConHelperTraceSizeCount() +
			 					 _stats.getCompleteTracingCount() +
			 					 _stats.getFinalTraceCount();
			 totalBytesToClean = _stats.getCardCleanCount() +
			 					 _stats.getConHelperCardCleanCount() +
			 					 _stats.getFinalCardCleanCount();

			if (_secondCardCleanPass) {
				float pass1Ratio = _cardCleaningFactorPass2 > 0 ? (_cardCleaningFactorPass1 / (_cardCleaningFactorPass1 + _cardCleaningFactorPass2)) : 1;
				_bytesToTracePass1 = (uintptr_t)((float)totalBytesToTrace * _bytesTracedInPass1Factor);
				_bytesToTracePass2 = MM_Math::saturatingSubtract(totalBytesToTrace, _bytesToTracePass1);
				_bytesToCleanPass1 = (uintptr_t)((float)totalBytesToClean * pass1Ratio);
				_bytesToCleanPass2 = MM_Math::saturatingSubtract(totalBytesToClean, _bytesToCleanPass1);
			} else {
				_bytesToTracePass1 = totalBytesToTrace;
				_bytesToTracePass2 = 0;
				_bytesToCleanPass1 = totalBytesToClean;
				_bytesToCleanPass2 = 0;
			}
		} else {
			/* We did not trace enough to use amount traced to predict trace target so best we can do
			 * is estimate based on current heap size, live object factor, leaf object factor etc.
			 */
			totalBytesToTrace = (uintptr_t)(heapSize * _tenureLiveObjectFactor * _tenureNonLeafObjectFactor);
			_bytesToTracePass1 = (uintptr_t)((float)totalBytesToTrace * _bytesTracedInPass1Factor);
			_bytesToTracePass2 = MM_Math::saturatingSubtract(totalBytesToTrace, _bytesToTracePass1);
			_bytesToCleanPass1 = (uintptr_t)((float)totalBytesToTrace * _cardCleaningFactorPass1);
			_bytesToCleanPass2 = (uintptr_t)((float)totalBytesToTrace * _cardCleaningFactorPass2);
		}
	}

	recalculateInitWork(env);

	/* Reset trace rate for next concurrent cycle */
	_allocToTraceRate = _allocToTraceRateNormal;

	/*
	 * Trace target is simply a sum of all work we predict we need to complete marking.
	 * Initialization work is accounted for separately
	 */
	_traceTargetPass1 = _bytesToTracePass1 + _bytesToCleanPass1;
	_traceTargetPass2 = _bytesToTracePass2 + _bytesToCleanPass2;
	_stats.setTraceSizeTarget(_traceTargetPass1 + _traceTargetPass2);

	/* Calculate the KO point for concurrent. As we trace at different rates during the
	 * initialization and marking phases we need to allow for that when calculating
	 * the KO point.
	 */
	kickoffThreshold = (_stats.getInitWorkRequired() / _allocToInitRate) +
					   (_traceTargetPass1 / _allocToTraceRateNormal) +
					   (_traceTargetPass2 / (_allocToTraceRateNormal * _allocToTraceRateCardCleanPass2Boost));

	/* Determine card cleaning thresholds */
	cardCleaningThreshold = ((uintptr_t)((float)kickoffThreshold / _cardCleaningThresholdFactor));

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
	float cardCleaningProportion = (float)cardCleaningThreshold / (float)kickoffThreshold;

	kickoffThresholdPlusBuffer = (uintptr_t)((float)kickoffThreshold + boost + ((float)_extensions->concurrentSlack * kickoffProportion));
	_stats.setKickoffThreshold(kickoffThresholdPlusBuffer);
	_stats.setCardCleaningThreshold((uintptr_t)((float)cardCleaningThreshold + boost + ((float)_extensions->concurrentSlack * cardCleaningProportion)));
	_kickoffThresholdBuffer = MM_Math::saturatingSubtract(kickoffThresholdPlusBuffer, kickoffThreshold);

	if (_extensions->debugConcurrentMark) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("Tune to heap : Trace target Pass 1=\"%zu\" (Trace=\"%zu\" Clean=\"%zu\")\n",
							_traceTargetPass1, _bytesToTracePass1, _bytesToCleanPass1);
		omrtty_printf("               Trace target Pass 2=\"%zu\" (Trace=\"%zu\" Clean=\"%zu\")\n",
							_traceTargetPass2, _bytesToTracePass2, _bytesToCleanPass2);
		omrtty_printf("               KO threshold=\"%zu\" KO threshold buffer=\"%zu\"\n",
							 _stats.getKickoffThreshold(), _kickoffThresholdBuffer);
		omrtty_printf("               Card Cleaning Threshold=\"%zu\" \n",
							_stats.getCardCleaningThreshold());
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
MM_ConcurrentGCIncrementalUpdate::adjustTraceTarget()
{
	uintptr_t newTraceTarget, totalBytesToTrace;
	MM_Heap *heap = (MM_Heap *)_extensions->heap;
	uintptr_t heapSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);

 	/* Reset bytes to trace and clean based on new heap size and the average live rate */
 	 totalBytesToTrace = (uintptr_t)(heapSize * _tenureLiveObjectFactor * _tenureNonLeafObjectFactor);
	_bytesToTracePass1 = (uintptr_t)((float)totalBytesToTrace * _bytesTracedInPass1Factor);
	_bytesToTracePass2 = totalBytesToTrace - _bytesToTracePass1;
	_bytesToCleanPass1 = (uintptr_t)((float)_bytesToTracePass1 * _cardCleaningFactorPass1);
	_bytesToCleanPass2 = (uintptr_t)((float)_bytesToTracePass2 * _cardCleaningFactorPass2);

	/* Calculate new trace target */
	newTraceTarget = _bytesToTracePass1 + _bytesToTracePass2 + _bytesToCleanPass1 + _bytesToCleanPass2;
	_stats.setTraceSizeTarget(newTraceTarget);
}

void
MM_ConcurrentGCIncrementalUpdate::completeConcurrentTracing(MM_EnvironmentBase *env, uintptr_t executionModeAtGC) {
	if (CONCURRENT_FINAL_COLLECTION > executionModeAtGC) {
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

		reportConcurrentHalted(env);

		if (!_markingScheme->getWorkPackets()->tracingExhausted()) {

			reportConcurrentCompleteTracingStart(env);
			uint64_t startTime = omrtime_hires_clock();
			/* Get assistance from all worker threads to complete processing of any remaining work packets.
			 * In the event of work stack overflow we will just dirty cards which will get processed during
			 * final card cleaning.
			 */
			MM_ConcurrentCompleteTracingTask completeTracingTask(env, _dispatcher, this, env->_cycleState);
			_dispatcher->run(env, &completeTracingTask);

			reportConcurrentCompleteTracingEnd(env, omrtime_hires_clock() - startTime);
		}
	}
}

/**
 *  Also, reset the card cleaning factors based on cards we cleaned this cycle.
 */
void
MM_ConcurrentGCIncrementalUpdate::updateTuningStatisticsInternal(MM_EnvironmentBase *env)
{
	uintptr_t totalTraced = 0;
	uintptr_t totalCleaned = 0;
	uintptr_t totalTracedPass1 = 0;
	uintptr_t totalCleanedPass1 = 0;
	uintptr_t totalCleanedPass2 = 0;
	float newCardCleaningFactorPass1 = 0.0;
	float newCardCleaningFactorPass2 = 0.0;
	float newBytesTracedInPass1Factor = 0.0;

	/* Recalculate _cardCleaningFactor depending on status */
	uintptr_t executionModeAtGC = _stats.getExecutionModeAtGC();
	switch (executionModeAtGC) {
	/* Don't change values if concurrent not started */
	case CONCURRENT_OFF:
		break;

	/* Initial estimation was way off */
	case CONCURRENT_INIT_RUNNING:
	case CONCURRENT_TRACE_ONLY:
		_cardCleaningFactorPass1 = MM_Math::weightedAverage(_cardCleaningFactorPass1, _maxCardCleaningFactorPass1, CARD_CLEANING_HISTORY_WEIGHT);
		_cardCleaningFactorPass2 = MM_Math::weightedAverage(_cardCleaningFactorPass2, _maxCardCleaningFactorPass2, CARD_CLEANING_HISTORY_WEIGHT);
		_bytesTracedInPass1Factor = MM_Math::weightedAverage(_bytesTracedInPass1Factor, ALL_BYTES_TRACED_IN_PASS_1, BYTES_TRACED_IN_PASS_1_HISTORY_WEIGHT);
		break;
	/* Leave alone if we started but did not finish */
	case CONCURRENT_CLEAN_TRACE:
		break;
	/* Calculate new average if we got to end  */
	case CONCURRENT_EXHAUSTED:
	case CONCURRENT_FINAL_COLLECTION:

		totalTraced = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();
		totalCleaned = _stats.getCardCleanCount() + _stats.getConHelperCardCleanCount();

		if (_secondCardCleanPass) {
			assume0(_totalTracedAtPass2KO != HIGH_VALUES && _totalCleanedAtPass2KO != HIGH_VALUES);
			totalTracedPass1 = _totalTracedAtPass2KO;
			totalCleanedPass1 = _totalCleanedAtPass2KO;
			totalCleanedPass2 =	totalCleaned - _totalCleanedAtPass2KO;
		} else {
			assume0(_totalTracedAtPass2KO == HIGH_VALUES && _totalCleanedAtPass2KO == HIGH_VALUES);
			totalTracedPass1 = totalTraced;
			totalCleanedPass1 = totalCleaned;
			totalCleanedPass2 =	0;
		}

		/* What factor of the tracing work was done beofre we started 2nd pass of card cleaning ?*/
		newBytesTracedInPass1Factor = (float)totalTracedPass1 / (float)totalTraced;
		newCardCleaningFactorPass1 = (float)totalCleanedPass1 / (float)totalTraced;
		newCardCleaningFactorPass1 = OMR_MIN(newCardCleaningFactorPass1, _maxCardCleaningFactorPass1);
		_cardCleaningFactorPass1 = MM_Math::weightedAverage(_cardCleaningFactorPass1, newCardCleaningFactorPass1, CARD_CLEANING_HISTORY_WEIGHT);
		_bytesTracedInPass1Factor = MM_Math::weightedAverage(_bytesTracedInPass1Factor, newBytesTracedInPass1Factor, BYTES_TRACED_IN_PASS_1_HISTORY_WEIGHT);

		/* Only update pass 2 average if we did 2 passes */
		if (_secondCardCleanPass) {
			newCardCleaningFactorPass2 = (float)totalCleanedPass2 / (float)totalTraced;
			newCardCleaningFactorPass2 = OMR_MIN(newCardCleaningFactorPass2, _maxCardCleaningFactorPass2);
			_cardCleaningFactorPass2 = MM_Math::weightedAverage(_cardCleaningFactorPass2, newCardCleaningFactorPass2, CARD_CLEANING_HISTORY_WEIGHT);
		}

		break;
	default:
		Assert_GC_true_with_message(env, (CONCURRENT_ROOT_TRACING <= executionModeAtGC) && (CONCURRENT_TRACE_ONLY > executionModeAtGC), "MM_ConcurrentStats::_executionModeAtGC = %zu\n", executionModeAtGC);
		_cardCleaningFactorPass1 = MM_Math::weightedAverage(_cardCleaningFactorPass1, _maxCardCleaningFactorPass1, CARD_CLEANING_HISTORY_WEIGHT);
		_cardCleaningFactorPass2 = MM_Math::weightedAverage(_cardCleaningFactorPass2, _maxCardCleaningFactorPass2, CARD_CLEANING_HISTORY_WEIGHT);
		_bytesTracedInPass1Factor = MM_Math::weightedAverage(_bytesTracedInPass1Factor, ALL_BYTES_TRACED_IN_PASS_1, BYTES_TRACED_IN_PASS_1_HISTORY_WEIGHT);
		break;
	}

	if (_extensions->debugConcurrentMark) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

		char pass1Factor[10];
		char pass2Factor[10];

		if (_extensions->cardCleaningPasses > 0) {
			sprintf(pass1Factor, "%.3f", _cardCleaningFactorPass1);
		} else {
			sprintf(pass1Factor, "%s", "N/A");
		}

		if (_extensions->cardCleaningPasses > 1) {
			sprintf(pass2Factor, "%.3f", _cardCleaningFactorPass2);
		} else {
			sprintf(pass2Factor, "%s", "N/A");
		}

		omrtty_printf("Update tuning statistics: Total Traced=\"%zu\" (Pass 2 KO=\"%zu\")  Total Cleaned=\"%zu\" (Pass 2 KO=\"%zu\")\n",
						totalTraced, _totalTracedAtPass2KO, totalCleaned, _totalCleanedAtPass2KO);
		omrtty_printf("                          Tenure Live object Factor=\"%.3f\" Tenure non-leaf object factor=\"%.3f\" \n",
						_tenureLiveObjectFactor, _tenureNonLeafObjectFactor);
		omrtty_printf("                          Card Cleaning Factors: Pass1=\"%s\" Pass2=\"%s\"\n",
						pass1Factor, pass2Factor);
		omrtty_printf("                          Bytes traced in Pass 1 Factor=\"%.3f\"\n",
						_bytesTracedInPass1Factor);
	}
}

/**
 * Start card cleaning.
 * Either free space has reached the card cleaning kickoff level or we have run out
 * of other work so may as well start card cleaning early.
 *
 */
void
MM_ConcurrentGCIncrementalUpdate::kickoffCardCleaning(MM_EnvironmentBase *env, ConcurrentCardCleaningReason reason)
{
	/* Switch to CONCURRENT_CLEAN_TRACE...if we fail someone beat us to it */
	if (_stats.switchExecutionMode(CONCURRENT_TRACE_ONLY, CONCURRENT_CLEAN_TRACE)) {
		_stats.setCardCleaningReason(reason);
		_concurrentDelegate.cardCleaningStarted(env);
	}
}

void
MM_ConcurrentGCIncrementalUpdate::setupForConcurrent(MM_EnvironmentBase *env)
{
	_concurrentDelegate.signalThreadsToActivateWriteBarrier(env);
	_stats.switchExecutionMode(CONCURRENT_INIT_COMPLETE, CONCURRENT_ROOT_TRACING);
}

/**
 * Do some object tracing.
 * Mutator pays its allocation tax by tracing references until tax paid of no more tracing work
 * to do.
 *
 * @param sizeToTrace  The amount of tracing work, in bytes, this mutator has left to do.
 * @param threadAtSafePoint TRUE if mutator is at a safe point; false otherwiwse
 *
 * @return the amount of tracing work actually done
 */
uintptr_t
MM_ConcurrentGCIncrementalUpdate::doConcurrentTrace(MM_EnvironmentBase *env,
								MM_AllocateDescription *allocDescription,
								uintptr_t sizeToTrace,
								MM_MemorySubSpace *subspace,
								bool threadAtSafePoint)
{
	uintptr_t sizeTraced = 0;
	uintptr_t sizeTracedPreviously = (uintptr_t)-1;
	uintptr_t remainingFree;
	bool isGcOccurred = false;

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

	if (periodicalTuningNeeded(env,remainingFree)) {
		periodicalTuning(env, remainingFree);
		/* Move any remaining deferred work packets to regular lists */
		_markingScheme->getWorkPackets()->reuseDeferredPackets(env);
	}

	/* Switch state if card cleaning stage 1 threshold reached
	 * Typically kickoff would be triggered by remainingFree < kickoffThreshold (low heap/tenure occupancy).
	 * However, with some other kickoff criteria (for example, language specific, like class unloading) that could be
	 * much sooner, so that remainingFree is significantly higher than pre-calculated kickoffThreshold.
	 * Since CardCleaningThreshold was also pre-calculated, irrespective of actual remainingFree at the moment of kickoff
	 * (during tuneToHeap that occurred at the end of last global GC), it needs to be adjusted by
	 * the difference between actual remainingFree at the time of global kickoff and global kickoff threshold.
	 */
	uintptr_t relativeCardCleaningThreshold = _stats.getCardCleaningThreshold();
	uintptr_t absoluteCardCleaningThreshold = relativeCardCleaningThreshold;

	if (_stats.getRemainingFree() >= _stats.getKickoffThreshold()) {
		absoluteCardCleaningThreshold += (_stats.getRemainingFree() - _stats.getKickoffThreshold());
	}

	if( (CONCURRENT_TRACE_ONLY == _stats.getExecutionMode()) && (remainingFree < absoluteCardCleaningThreshold)) {
		kickoffCardCleaning(env, CARD_CLEANING_THRESHOLD_REACHED);
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

	/* Scan this threads stack, if it hasn't been done already. We cant do so if this is
	 * a TLH allocation as we are not at a safe point.
	 */
	if(!env->isThreadScanned() && threadAtSafePoint) {
		/* If call back is late, i.e after the collect,  then ignore it */
		/* CMVC 119942 : add a top range of CONCURRENT_EXHAUSTED so that we don't scan threads
		 * at the last second and report them as being scanned when we haven't actually had a
		 * chance to follow any of the references
		 */
		uintptr_t mode = _stats.getExecutionMode();
		if ((CONCURRENT_ROOT_TRACING <= mode) && (CONCURRENT_EXHAUSTED > mode)) {
			if (_concurrentDelegate.scanThreadRoots(env)) {
				flushLocalBuffers(env);
				env->setThreadScanned(true);
				_stats.incThreadsScannedCount();
				/* Resume concurrent helper threads to help mark any roots we have found.
				 * If thread already active this notify will be ignored
				 */
				resumeConHelperThreads(env);
			}
		}
	}

	/* Loop marking and cleaning cards until we have paid all our tax, another thread
	 * requests exclusive VM access to do a gc, or another thread has swithed to exhausted
	 */
	while (!env->isExclusiveAccessRequestWaiting() &&
			(sizeTraced < sizeToTrace) &&
			(sizeTraced != sizeTracedPreviously) &&
			(CONCURRENT_CLEAN_TRACE >= _stats.getExecutionMode())
	) {

		/* CMVC 131721 - record the amount traced up until now. If any iteration of this loop fails
		 * to increase the sizeTraced, break out of the loop and return to mutating
		 */
		sizeTracedPreviously = sizeTraced;

		/* Scan objects until there are no more, the trace size has been
		 * achieved, or gc is waiting
		 */
		uintptr_t bytesTraced = localMark(env,(sizeToTrace - sizeTraced));
		if (bytesTraced > 0 ) {
			/* Update global count of amount  traced */
			_stats.incTraceSizeCount(bytesTraced);
			/* ..and local count */
			sizeTraced += bytesTraced;
		}

		/* If GC is not waiting and we did not have enough to trace */
		if (!env->isExclusiveAccessRequestWaiting() && sizeTraced < sizeToTrace) {
			/* Check to see if we need to start card cleaning early */
			if (CONCURRENT_TRACE_ONLY == _stats.getExecutionMode()) {
				/* We have run out of tracing work. If all tracing is complete or
				 * we have passed the peak of tracing activity then we may as well
				 * start card cleaning now.
				 */
				if ((_markingScheme->getWorkPackets()->tracingExhausted() || tracingRateDropped(env)) && _stats.isRootTracingComplete()) {
					kickoffCardCleaning(env, TRACING_COMPLETED);
				} else {
					/* Nothing to do and not time to start card cleaning yet */
					break;
				}
			}

			if (CONCURRENT_CLEAN_TRACE == _stats.getExecutionMode()) {
				if(!((MM_ConcurrentCardTable *)_cardTable)->isCardCleaningComplete()) {
					/* Clean some cards. Returns when enough cards traced, no more cards
					 * to trace, _gcWaiting true or a GC occurred preparing cards.
					 */
					assume0(sizeToTrace > sizeTraced);
					uintptr_t bytesCleaned;
					if(cleanCards(env, true, (sizeToTrace - sizeTraced), &bytesCleaned, threadAtSafePoint)) {
						if (bytesCleaned > 0 ) {
							/* Update global count */
							_stats.incCardCleanCount(bytesCleaned);
							/* ..and local count */
							sizeTraced += bytesCleaned;
						}
					} else {
						/* A GC has occurred so quit tracing immediately */
						/* Return any work packets to appropriate lists */
						isGcOccurred = true;
						sizeTraced = 0;
						break;
					}
				} else {
					/* We have completed card cleaning and currently have nothing to trace. If we have not scanned
					 * this thread yet (and this should be a very rare event) then break out of tracing loop and return.
					 * Hopefully this will allow the thread to get to and async message check and so call us back to
					 * get the thread scanned.
					 */
					 if (!env->isThreadScanned()) {
						break;
					 } else {
							/*
							 * Must be no local work left at this point!
							 */
							Assert_MM_true(!env->_workStack.inputPacketAvailable());
							Assert_MM_true(!env->_workStack.outputPacketAvailable());
							Assert_MM_true(!env->_workStack.deferredPacketAvailable());

						/*
						 * Check to see if we are done; other threads may hold packets
						 */
						if	( _markingScheme->getWorkPackets()->tracingExhausted()) {
							break;
						} else {
							/*
							 * Tracing is not yet exhausted; some packets are held by other threads.
							 * If we get here we have finished all card cleaning and cannot get an input packet so we
							 * must be near completing the tracing. However, the lower priority concurrent helpers could be suspended and
							 * hanging onto packets and so preventing us from getting to CONCURENT_EXHAUSTED. This will delay
							 * the final collection; maybe until an AF which we don't want. We need to tell the concurrent helpers
							 * to release any packets they have so we can get them processed. This is easily achieved by suspending them.
							 * Anyway, now is also a good time to tell the concurrent helpers to park themselves for the imminent global collect.
							 */
							switchConHelperRequest(CONCURRENT_HELPER_MARK, CONCURRENT_HELPER_WAIT);

							/* ..and yield to give threads with work a chance to finish off
							 *  or publish work for us to do */
							omrthread_yield();
						}
					 } /* isThreadScanned() */
				} /* of isCardCleaningDone() */
			} /* of executionMode == CONCURRENT_CLEAN_TRACE */
		}
	} /* of while sizeTraced < sizeToTrace */

	if (!isGcOccurred) {
		/* If no more work left (and concurrent scanning is complete or disabled) then switch to exhausted now */
		if ((NULL != _cardTable) && ((MM_ConcurrentCardTable *)_cardTable)->isCardCleaningComplete() &&
			_markingScheme->getWorkPackets()->tracingExhausted() &&
			_concurrentDelegate.isConcurrentScanningComplete(env)) {

			if(_stats.switchExecutionMode(CONCURRENT_CLEAN_TRACE, CONCURRENT_EXHAUSTED)) {
				/* Tell all MSS to use slow path allocate and so get to a safe
				* point before paying allocation tax.
				*/
				subspace->setAllocateAtSafePointOnly(env, true);
			}
		}

		/* If there is work available on input lists then notify any waiting concurrent helpers */
		if ((_markingScheme->getWorkPackets()->inputPacketAvailable(env)) || ((NULL != _cardTable) && _cardTable->isCardCleaningStarted() && !_cardTable->isCardCleaningComplete())) {
			resumeConHelperThreads(env);
		}
	}

	/*
	 * Must be no local work left at this point!
	 */
	Assert_MM_true(!env->_workStack.inputPacketAvailable());
	Assert_MM_true(!env->_workStack.outputPacketAvailable());
	Assert_MM_true(!env->_workStack.deferredPacketAvailable());

	return sizeTraced;
}

/**
 * Do local mark.
 * Process reference until allocation tax paid in full or we asked to stop because
 * a GC is outstanding. Roots are popped from the work stack and either:
 * 1) deferred - if reference in and active TLH, or
 * 2) ignored - if reference in a dirty card, or
 * 3) traced - call marking scheme to trace referenced object for further references
 *
 * @param sizeToTrace The mutators allocation tax
 * @return The actual amount of work done
 */
uintptr_t
MM_ConcurrentGCIncrementalUpdate::localMark(MM_EnvironmentBase *env, uintptr_t sizeToTrace)
{
	omrobjectptr_t objectPtr;
	uintptr_t gcCount = _extensions->globalGCStats.gcCount;
	uint32_t const referenceSize = env->compressObjectReferences() ? sizeof(uint32_t) : sizeof(uintptr_t);

	env->_workStack.reset(env, _markingScheme->getWorkPackets());
	Assert_MM_true(env->_cycleState == NULL);
	Assert_MM_true(CONCURRENT_OFF < _stats.getExecutionMode());
	Assert_MM_true(_concurrentCycleState._referenceObjectOptions == MM_CycleState::references_default);
	env->_cycleState = &_concurrentCycleState;

	uintptr_t sizeTraced = 0;
	while(NULL != (objectPtr = (omrobjectptr_t)env->_workStack.popNoWait(env))) {
		/* Check for array scanPtr..if we find one ignore it*/
		if ((uintptr_t)objectPtr & PACKET_ARRAY_SPLIT_TAG){
			continue;
		} else 	if (((MM_ConcurrentCardTable *)_cardTable)->isObjectInActiveTLH(env,objectPtr)) {
			env->_workStack.pushDefer(env,objectPtr);
			/* We are deferring the tracing but get some "tracing credit" */
			sizeTraced += referenceSize;
		} else if (((MM_ConcurrentCardTable *)_cardTable)->isObjectInUncleanedDirtyCard(env,objectPtr)) {
			/* Dont need to trace this object now as we will re-visit it
			 * later when we clean the card during concurrent card cleaning
			 * but count it as it was included in trace target calculations.
			 */
			sizeTraced += _extensions->objectModel.getSizeInBytesWithHeader(objectPtr);

			_concurrentDelegate.processItem(env, objectPtr);

			/* If its a partially processed array this will leave the low_tagged
			 * scanPtr on the stack. Rather than check for it now we will just ignore the
			 * reference when we pop it or before we exit localMark()
			 */
		} else {
			/* Else trace the object */
			sizeTraced += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_PACKET, (sizeToTrace - sizeTraced));
		}

		/* Have we done enough tracing ? */
		if(sizeTraced >= sizeToTrace) {
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
	if ( ((uintptr_t)((omrobjectptr_t)env->_workStack.peek(env))) & PACKET_ARRAY_SPLIT_TAG) {
		env->_workStack.popNoWait(env);
	}

	/* STW collection should not occur while localMark is working */
	Assert_MM_true(gcCount == _extensions->globalGCStats.gcCount);

	flushLocalBuffers(env);
	env->_cycleState = NULL;

	return sizeTraced;
}

/**
 * Clean some dirty cards.
 * Called during concurrent mode CONCURRENT_CLEAN_TRACE to clean a number of
 * dirty cards. That is re-trace all objects which START within a dirty card
 * for references to UNMARKED objects.
 *
 * @param isMutator: TRUE if called on a mutator thread; FALSE if called on
 * a background thread.
 * @param sizeToDo : the amount of objects to be traced
 * @param threadAtSafePoint Whether the calling thread is at a safe point or not
 * @return FALSE if a GC occurred; TRUE otheriwse
 * Alos, updates "sizeDone" with the actual size traced which may be less than sizeToDo
 * if a GC is requested whilst we are tracing.
 */
MMINLINE bool
MM_ConcurrentGCIncrementalUpdate::cleanCards(MM_EnvironmentBase *env, bool isMutator, uintptr_t sizeToDo, uintptr_t *sizeDone, bool threadAtSafePoint)
{
	/* Clean required number of cards and... */
	env->_workStack.reset(env, _markingScheme->getWorkPackets());
	Assert_MM_true(env->_cycleState == NULL);
	Assert_MM_true(_concurrentCycleState._referenceObjectOptions == MM_CycleState::references_default);
	env->_cycleState = &_concurrentCycleState;
	bool gcOccurred = ((MM_ConcurrentCardTable *)_cardTable)->cleanCards(env, isMutator, sizeToDo, sizeDone, threadAtSafePoint);
	flushLocalBuffers(env);
	env->_cycleState = NULL;

	if (gcOccurred) {
		/* suspend con helper thread for pending GC */
		uintptr_t conHelperRequest = switchConHelperRequest(CONCURRENT_HELPER_MARK, CONCURRENT_HELPER_WAIT);
		Assert_MM_true(CONCURRENT_HELPER_MARK != conHelperRequest);
	}
	return gcOccurred;
}

/**
 * Perform final card cleaning.
 * This routine is called to perform the final scan on the card table to clean
 * any cards not cleaned during the concurrent phase.
 */
void
MM_ConcurrentGCIncrementalUpdate::finalCleanCards(MM_EnvironmentBase *env)
{
 	omrobjectptr_t objectPtr;
 	bool moreCards = true;
 	bool moreRefs = true;
 	uintptr_t totalTraced = 0;
 	uintptr_t totalCleaned = 0;
 	uintptr_t bytesCleaned;

	env->_workStack.reset(env, _markingScheme->getWorkPackets());

	/* Until no more refs to process */
	while (moreRefs) {
		/* Process any available refs. We may have some refs left from concurrent
		 * phase if we never reached EXHAUSTED; if so we want to process them
		 * first to reduce chances of a stack overflow
		 */
		objectPtr = (omrobjectptr_t)-1;
		while (objectPtr) {
			if (!moreCards) {
				/* This will block waiting for input or until all threads starved
				 * in which case we will get NULL */
				objectPtr = (omrobjectptr_t)env->_workStack.pop(env);
				if (NULL == objectPtr) {
					/* we are done */
					moreRefs = false;
				}
			} else {
				/* .. whereas this will return NULL without blocking if stack empty
				 * to allow us to scan some more cards whilst we wait for work
				 */
				objectPtr = (omrobjectptr_t)env->_workStack.popNoWait(env);
			}

			/* NB Reference object can be in TENURE or NURSERY at this time
			 * if scavenger is enbaled as we will have processed the remembered set
			 * by now
			 */

			if (objectPtr) {
				/* Check for array scanPtr.. If we find one ignore it*/
				if ((uintptr_t)objectPtr & PACKET_ARRAY_SPLIT_TAG){
					continue;
				/* If object in a dirty card we will re-visit later so ignore it for now */
				} else {
					bool dirty;

#if defined(OMR_GC_MODRON_SCAVENGER)
					/* If scavenger is enabled then ref may be in nursery */
					if(_extensions->scavengerEnabled) {
						dirty = ((MM_ConcurrentCardTable *)_cardTable)->isObjectInDirtyCard(env,objectPtr);
					} else
#endif /* OMR_GC_MODRON_SCAVENGER */
					{
						dirty = ((MM_ConcurrentCardTable *)_cardTable)->isObjectInDirtyCardNoCheck(env,objectPtr);
					}

					if (!dirty) {
						totalTraced += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_PACKET);
					} else {
						_concurrentDelegate.processItem(env, objectPtr);
					}
				}
			}
		}

		/* Process some cards if there are any left */
		if(moreCards) {
			/* Clean some more cards and remember if we cleaned last one or not */
			moreCards = ((MM_ConcurrentCardTable *)_cardTable)->finalCleanCards(env, &bytesCleaned);
			/* ..count any objects we traced whilst cleaning the cards */
			totalCleaned += bytesCleaned;
		}
	}

	flushLocalBuffers(env);
	_stats.incFinalTraceCount(totalTraced);
	_stats.incFinalCardCleanCount(totalCleaned);
}

void
MM_ConcurrentGCIncrementalUpdate::resetConcurrentParameters(MM_EnvironmentBase *env)
{
	MM_ConcurrentGC::resetConcurrentParameters(env);

	_totalTracedAtPass2KO = HIGH_VALUES;
	_totalCleanedAtPass2KO = HIGH_VALUES;
	_pass2Started = false;
}

void
MM_ConcurrentGCIncrementalUpdate::initalizeConcurrentStructures(MM_EnvironmentBase *env)
{
	((MM_ConcurrentCardTable *)_cardTable)->initializeCardCleaning(env);
}

uintptr_t
MM_ConcurrentGCIncrementalUpdate::doConcurrentInitializationInternal(MM_EnvironmentBase *env, uintptr_t initToDo)
{
	void *from, *to;
	InitType type;
	bool concurrentCollectable;

	uintptr_t initDone = 0;

	while(initToDo > initDone) {

		/* If another thread is waiting for exclusive VM access, possibly
		 * to collect, get out quick so as not to hold things up
		 */
		if(env->isExclusiveAccessRequestWaiting()) {
			break;
		}

		if(getInitRange(env, &from, &to, &type, &concurrentCollectable)) {
			switch(type) {
				case MARK_BITS:
					if (concurrentCollectable) {
						initDone += _markingScheme->setMarkBitsInRange(env,from,to,true);
					} else {
						initDone += _markingScheme->setMarkBitsInRange(env,from,to,false);
					}
					break;
				case CARD_TABLE:
					initDone += ((MM_ConcurrentCardTable *)_cardTable)->clearCardsInRange(env,from,to);
					break;
				default:
					assume0(0);
			}
		} else {
			/* All init ranges taken so exit and wait below for init
			 * to complete before returning to caller
			 */
			break;
		}
	}

	return initDone;
}

uint32_t
MM_ConcurrentGCIncrementalUpdate::numberOfInitRanages(MM_MemorySubSpace *subspace)
{
	/* If the segment is for a concurrently collectable subspace we will
	 * have some cards to clear too */
	if (subspace->isConcurrentCollectable()) {
		return 2; /* We will have some cards to clean as well */
	} else {
		return 1;
	}
}

void
MM_ConcurrentGCIncrementalUpdate::determineInitWorkInternal(MM_EnvironmentBase *env, uint32_t initIndex)
{
	/* Add init ranges for all card table ranges for concurrently collectable segments.
	 * We create one entry for each segment of heap which is concurrently
	 * collectable. All associated cards in card table will be set to clean (0x00). */
	for (I_32 x=initIndex-1; x >= 0; x--) {
		if ((_initRanges[x].type == MARK_BITS) && ((_initRanges[x].subspace)->isConcurrentCollectable())) {
			_initRanges[initIndex].base = _initRanges[x].base;
			_initRanges[initIndex].top = _initRanges[x].top;
			_initRanges[initIndex].current = _initRanges[initIndex].base;
			_initRanges[initIndex].subspace = _initRanges[x].subspace;
			_initRanges[initIndex].initBytes = ((MM_ConcurrentCardTable *)_cardTable)->cardBytesForHeapRange(env,_initRanges[initIndex].base,_initRanges[initIndex].top);
			_initRanges[initIndex].type = CARD_TABLE;
			_initRanges[initIndex].chunkSize = INIT_CHUNK_SIZE * CARD_SIZE;
			initIndex++;
		}
	}
}

void
MM_ConcurrentGCIncrementalUpdate::conHelperDoWorkInternal(MM_EnvironmentBase *env, ConHelperRequest *request, MM_SpinLimiter *spinLimiter, uintptr_t *totalScanned)
{
	uintptr_t sizeTraced = 0;

	spinLimiter->reset();

	/* clean cards */
	while ((CONCURRENT_HELPER_MARK == *request)
			&& (CONCURRENT_CLEAN_TRACE == _stats.getExecutionMode())
			&& _cardTable->isCardCleaningStarted()
			&& !_cardTable->isCardCleaningComplete()
			&& spinLimiter->spin()) {
		if (cleanCards(env, false, _conHelperCleanSize, &sizeTraced, false)) {
			if (sizeTraced > 0 ) {
				_stats.incConHelperCardCleanCount(sizeTraced);
				*totalScanned += sizeTraced;
				spinLimiter->reset();
			}
		}
		*request = getConHelperRequest(env);
	}
}

void
MM_ConcurrentGCIncrementalUpdate::internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	/* If we flushed all the TLH's correctly in GC the TLH mark bits should be
	 * all OFF
	 */
	assume(_cardTable->isTLHMarkBitsEmpty(env),"TLH mark map not empty");
	MM_ConcurrentGC::internalPostCollect(env, subSpace);
}

bool
MM_ConcurrentGCIncrementalUpdate::canSkipObjectRSScan(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	return _cardTable->isObjectInDirtyCardNoCheck(env,objectPtr);
}

void
MM_ConcurrentGCIncrementalUpdate::postConcurrentUpdateStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats, UDATA bytesConcurrentlyScanned)
{
	_concurrentPhaseStats._cardTableStats = _cardTable->getCardTableStats();
	MM_ConcurrentGC::postConcurrentUpdateStatsAndReport(env);
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
