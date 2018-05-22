/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#include "AtomicOperations.hpp"
#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConcurrentCardTable.hpp"
#include "ConcurrentCardTableForWC.hpp"
#include "ConcurrentGC.hpp"
#include "ConcurrentCompleteTracingTask.hpp"
#include "ConcurrentClearNewMarkBitsTask.hpp"
#include "ConcurrentFinalCleanCardsTask.hpp"
#include "ConcurrentSafepointCallback.hpp"
#include "ConcurrentScanRememberedSetTask.hpp"
#if defined(OMR_GC_CONCURRENT_SWEEP)
#include "ConcurrentSweepScheme.hpp"
#endif /* OMR_GC_CONCURRENT_SWEEP */
#include "CycleState.hpp"
#include "Debug.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "MarkingScheme.hpp"
#include "Math.hpp"
#include "MemoryPool.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceFlat.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "ObjectModel.hpp"
#include "SpinLimiter.hpp"
#include "SublistIterator.hpp"
#include "SublistPuddle.hpp"
#include "SublistSlotIterator.hpp"
#include "WorkPacketsConcurrent.hpp"

typedef struct ConHelperThreadInfo {
	OMR_VM *omrVM;
	uintptr_t threadID;
	uintptr_t threadFlags;
	MM_ConcurrentGC *collector;
} ConHelperThreadInfo;

#define CON_HELPER_INFO_FLAG_OK 1
#define CON_HELPER_INFO_FLAG_FAIL 2

extern "C" {

/**
 * Concurrent mark write barrier
 * External entry point for write barrier. Calls card table function to dirty card for
 * given object which has had an object reference modified.
 *
 * @param destinationObject Address of object containing modified reference
 * @param storedObject The value of the modified reference
 */
void
J9ConcurrentWriteBarrierStore (OMR_VMThread *vmThread, omrobjectptr_t destinationObject, omrobjectptr_t storedObject)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();

	extensions->cardTable->dirtyCard(env, (omrobjectptr_t)destinationObject);
}

/**
 * Concurrent mark write barrier - batch store
 * External entry point for write barrier. Calls card table function to dirty card for
 * given object which has had multiple object references mofified.
 *
 * @param destinationObject Address of object containing modified reference
 */
void
J9ConcurrentWriteBarrierBatchStore (OMR_VMThread *vmThread, omrobjectptr_t destinationObject)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(vmThread);
	MM_GCExtensionsBase *extensions = env->getExtensions();

	extensions->cardTable->dirtyCard(env, (omrobjectptr_t)destinationObject);
}

/**
 * Concurrent helper thread procedure
 *
 * @parm info Address of ConHelperThreadInfo structure
 */
int J9THREAD_PROC
con_helper_thread_proc(void *info)
{
	ConHelperThreadInfo *conHelperThreadInfo = (ConHelperThreadInfo *)info;
	MM_ConcurrentGC *collector = conHelperThreadInfo->collector;
	OMRPORT_ACCESS_FROM_OMRVM(conHelperThreadInfo->omrVM);

	uintptr_t rc;
	void *signalHandlerArg = NULL;
	omrsig_handler_fn signalHandler = collector->_concurrentDelegate.getProtectedSignalHandler(&signalHandlerArg);
	omrsig_protect(con_helper_thread_proc2, info, signalHandler, signalHandlerArg,
		OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&rc);

	return 0;
}

/**
 * Concurrent helper thread procedure
 *
 * @parm info Address of ConHelperThreadInfo structure
 * @return return code; always 0
 */
uintptr_t
con_helper_thread_proc2(OMRPortLibrary* portLib, void *info)
{
	ConHelperThreadInfo *conHelperThreadInfo = (ConHelperThreadInfo *)info;
	OMR_VM *omrVM = conHelperThreadInfo->omrVM;
	uintptr_t threadID = conHelperThreadInfo->threadID;
	MM_ConcurrentGC *collector = conHelperThreadInfo->collector;

	/* Attach the thread as a system daemon thread */
	/* You need a VM thread so that the stack walker can work */
	OMR_VMThread *omrThread = MM_EnvironmentBase::attachVMThread(omrVM, "Concurrent Mark Helper", MM_EnvironmentBase::ATTACH_GC_HELPER_THREAD);

	/* Signal that the concurrent helper thread has started (or not) */
	conHelperThreadInfo->threadFlags = NULL != omrThread ? CON_HELPER_INFO_FLAG_OK : CON_HELPER_INFO_FLAG_FAIL;
	omrthread_monitor_enter(collector->_conHelpersActivationMonitor);
	omrthread_monitor_notify_all(collector->_conHelpersActivationMonitor);
	omrthread_monitor_exit(collector->_conHelpersActivationMonitor);

	/* If thread started invoke main entry point */
	if (NULL != omrThread) {
		collector->conHelperEntryPoint(omrThread, threadID);
	}

	return 0;
}

#if defined(OMR_GC_MODRON_SCAVENGER)
void oldToOldReferenceCreated(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	((MM_ConcurrentGC *)env->getExtensions()->getGlobalCollector())->oldToOldReferenceCreated(env, objectPtr);
}
#endif /* OMR_GC_MODRON_SCAVENGER */

} /* extern "C" */

void
MM_ConcurrentGC::reportConcurrentKickoff(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentKickoff(env->getLanguageVMThread(),
		_stats.getTraceSizeTarget(),
		_stats.getKickoffThreshold(),
		_stats.getRemainingFree()
	);

	MM_CommonGCData commonData;

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_KICKOFF(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_KICKOFF,
		_extensions->getHeap()->initializeCommonGCData(env, &commonData),
		_stats.getTraceSizeTarget(),
		_stats.getKickoffThreshold(),
		_stats.getRemainingFree(),
		_stats.getKickoffReason(),
		_languageKickoffReason
	);
}

void
MM_ConcurrentGC::reportConcurrentAborted(MM_EnvironmentBase *env, CollectionAbortReason reason)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentAborted(env->getLanguageVMThread());

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_ABORTED(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_ABORTED,
		reason
	);
}

void
MM_ConcurrentGC::reportConcurrentHalted(MM_EnvironmentBase *env)
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
MM_ConcurrentGC::reportConcurrentFinalCardCleaningStart(MM_EnvironmentBase *env)
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
MM_ConcurrentGC::reportConcurrentFinalCardCleaningEnd(MM_EnvironmentBase *env, uint64_t duration)
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

void
MM_ConcurrentGC::reportConcurrentCollectionStart(MM_EnvironmentBase *env)
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
MM_ConcurrentGC::reportConcurrentCollectionEnd(MM_EnvironmentBase *env, uint64_t duration)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentCollectionEnd(env->getLanguageVMThread(),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_NEW),
		_extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD),
		_extensions->heap->getActiveMemorySize(MEMORY_TYPE_OLD),
		(_extensions-> largeObjectArea ? _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD) : 0 ),
		(_extensions-> largeObjectArea ? _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD) : 0 )
	);

	if (J9_EVENT_IS_HOOKED(_extensions->privateHookInterface, J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END)) {
		MM_CommonGCEndData commonData;
		_extensions->heap->initializeCommonGCEndData(env, &commonData);

		ALWAYS_TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END(
				_extensions->privateHookInterface,
				env->getOmrVMThread(),
				omrtime_hires_clock(),
				J9HOOK_MM_PRIVATE_CONCURRENT_COLLECTION_END,
				duration,
				env->getExclusiveAccessTime(),
				&commonData
		);
	}
}

void
MM_ConcurrentGC::reportConcurrentBackgroundThreadActivated(MM_EnvironmentBase *env)
{
	Trc_MM_ConcurrentBackgroundThreadActivated(env->getLanguageVMThread());
	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_BACKGROUND_THREAD_ACTIVATED(_extensions->privateHookInterface, env->getOmrVMThread());
}

void
MM_ConcurrentGC::reportConcurrentBackgroundThreadFinished(MM_EnvironmentBase *env, uintptr_t traceTotal)
{
	Trc_MM_ConcurrentBackgroundThreadFinished(env->getLanguageVMThread());
	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_BACKGROUND_THREAD_FINISHED(_extensions->privateHookInterface, env->getOmrVMThread(), traceTotal);
}

void
MM_ConcurrentGC::reportConcurrentCompleteTracingStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentCompleteTracingStart(env->getLanguageVMThread(),
		_stats.getConcurrentWorkStackOverflowCount()
	);

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_START,
		_stats.getConcurrentWorkStackOverflowCount()
	);
}

void
MM_ConcurrentGC::reportConcurrentCompleteTracingEnd(MM_EnvironmentBase *env, uint64_t duration)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentCompleteTracingEnd(env->getLanguageVMThread(),
		_stats.getCompleteTracingCount(),
		_stats.getConcurrentWorkStackOverflowCount()
	);

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_COMPLETE_TRACING_END,
		duration,
		_stats.getCompleteTracingCount(),
		_stats.getConcurrentWorkStackOverflowCount()
	);
}

void
MM_ConcurrentGC::reportConcurrentRememberedSetScanStart(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentRememberedSetScanStart(env->getLanguageVMThread(),
		_stats.getConcurrentWorkStackOverflowCount()
	);
	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_START(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_START,
		_stats.getConcurrentWorkStackOverflowCount()
	);
}

void
MM_ConcurrentGC::reportConcurrentRememberedSetScanEnd(MM_EnvironmentBase *env, uint64_t duration)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentRememberedSetScanEnd(env->getLanguageVMThread(),
		_stats.getRSObjectsFound(),
		_stats.getRSScanTraceCount(),
		_stats.getConcurrentWorkStackOverflowCount()
	);
	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_REMEMBERED_SET_SCAN_END,
		duration,
		_stats.getRSObjectsFound(),
		_stats.getRSScanTraceCount(),
		_stats.getConcurrentWorkStackOverflowCount()
	);
}

/**
 * Hook function called when an the 2nd pass over card table to clean cards starts.
 * This is a wrapper into the non-static MM_ConcurrentGC::recordCardCleanPass2Start
 */
void
MM_ConcurrentGC::hookCardCleanPass2Start(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_CardCleanPass2StartEvent* event = (MM_CardCleanPass2StartEvent *)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_ConcurrentGC *)userData)->recordCardCleanPass2Start(env);

	/* Boost the trace rate for the 2nd card cleaning pass */
}

/**
 * Aysnc callback routine to signal all threads that they needs to start dirtying cards.
 *
 * @note Caller assumed to be at a safe point
 *
 */
void
MM_ConcurrentGC::signalThreadsToDirtyCardsAsyncEventHandler(OMR_VMThread *omrVMThread, void *userData)
{
	MM_ConcurrentGC *collector  = (MM_ConcurrentGC *)userData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	collector->signalThreadsToDirtyCards(env);
}

/**
 * Create new instance of ConcurrentGC object.
 *
 * @return Reference to new MM_COncurrentGC object or NULL
 */
MM_ConcurrentGC *
MM_ConcurrentGC::newInstance(MM_EnvironmentBase *env)
{
	MM_ConcurrentGC *concurrentGC = (MM_ConcurrentGC *)env->getForge()->allocate(sizeof(MM_ConcurrentGC), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != concurrentGC) {
		new(concurrentGC) MM_ConcurrentGC(env);
		if (!concurrentGC->initialize(env)) {
			concurrentGC->kill(env);
			concurrentGC = NULL;
		}
	}

	return concurrentGC;
}

/**
 * Destroy instance of an ConcurrentGC object.
 *
 */
void
MM_ConcurrentGC::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize a new ConcurrentGC object.
 * Instantiate card table and concurrent RAS objects(if required) and initialize all
 * monitors required by concurrent. Allocate and initialize the concurrent helper thread
 * table.
 *
 * @return TRUE if initialization completed OK;FALSE otheriwse
 */
bool
MM_ConcurrentGC::initialize(MM_EnvironmentBase *env)
{
	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);

	/* First call super class initialize */
	if (!MM_ParallelGlobalGC::initialize(env)) {
		goto error_no_memory;
	}

	if (!_concurrentDelegate.initialize(env, this)) {
		goto error_no_memory;
	}

	if(!createCardTable(env)) {
		goto error_no_memory;
	}

	if (_extensions->optimizeConcurrentWB) {
		_callback = _concurrentDelegate.createSafepointCallback(env);
		if (NULL == _callback) {
			goto error_no_memory;
		}
		_callback->registerCallback(env, signalThreadsToDirtyCardsAsyncEventHandler, this);
	}

	if (_conHelperThreads > 0) {
		/* Get storage for concurrent helper thread table */
		_conHelpersTable = (omrthread_t *)env->getForge()->allocate(_conHelperThreads * sizeof(omrthread_t), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
		if(!_conHelpersTable) {
			goto error_no_memory;
		}

		/* Clear storage to zeroes */
		memset(_conHelpersTable, 0, _conHelperThreads * sizeof(omrthread_t));
	}

	/* Init pointer to heap base; we will reset _heapAlloc in expanded as heap grows */
	_heapBase= (void *)_extensions->heap->getHeapBase();

	/* Initialize the concurrent helper thread monitors */
	if(omrthread_monitor_init_with_name(&_conHelpersActivationMonitor, 0, "MM_ConcurrentGC::conHelpersActivation")) {
		goto error_no_memory;
	}

	/* Initialize the initialisation work monitor */
	if(omrthread_monitor_init_with_name(&_initWorkMonitor, 0, "MM_ConcurrentGC::initWork")) {
		goto error_no_memory;
	 }

	/* Initialize the tuning monitor monitor */
	if(omrthread_monitor_init_with_name(&_concurrentTuningMonitor, 0, "MM_ConcurrentGC::concurrentTuning")) {
		goto error_no_memory;
	 }

	/* ..and the initialization work complete monitor */
	if(omrthread_monitor_init_with_name(&_initWorkCompleteMonitor, 0, "MM_ConcurrentGC::initWorkComplete")) {
		goto error_no_memory;
	 }

	_allocToInitRate = _extensions->concurrentLevel * CONCURRENT_INIT_BOOST_FACTOR;
	_allocToTraceRate = _extensions->concurrentLevel;
	_allocToTraceRateNormal = _extensions->concurrentLevel;
	_secondCardCleanPass = (_extensions->cardCleaningPasses == 2) ? true : false;
	_allocToTraceRateCardCleanPass2Boost = _extensions->cardCleanPass2Boost;

	_cardCleaningFactorPass1 = interpolateInRange(INITIAL_CARD_CLEANING_FACTOR_PASS1_1,
												INITIAL_CARD_CLEANING_FACTOR_PASS1_8,
												INITIAL_CARD_CLEANING_FACTOR_PASS1_10,
												_allocToTraceRateNormal);
	_maxCardCleaningFactorPass1 = interpolateInRange(MAX_CARD_CLEANING_FACTOR_PASS1_1,
												MAX_CARD_CLEANING_FACTOR_PASS1_8,
												MAX_CARD_CLEANING_FACTOR_PASS1_10,
												_allocToTraceRateNormal);
	_bytesTracedInPass1Factor = ALL_BYTES_TRACED_IN_PASS_1;
	if (_secondCardCleanPass) {
		_cardCleaningFactorPass2 = interpolateInRange(INITIAL_CARD_CLEANING_FACTOR_PASS2_1,
													INITIAL_CARD_CLEANING_FACTOR_PASS2_8,
													INITIAL_CARD_CLEANING_FACTOR_PASS2_10,
													_allocToTraceRateNormal);

		_maxCardCleaningFactorPass2 = interpolateInRange(MAX_CARD_CLEANING_FACTOR_PASS2_1,
													MAX_CARD_CLEANING_FACTOR_PASS2_8,
													MAX_CARD_CLEANING_FACTOR_PASS2_10,
													_allocToTraceRateNormal);
	} else {
		_cardCleaningFactorPass2 = 0;
		_maxCardCleaningFactorPass2 = 0;
	}

	_cardCleaningThresholdFactor = interpolateInRange(CARD_CLEANING_THRESHOLD_FACTOR_1,
													CARD_CLEANING_THRESHOLD_FACTOR_8,
													CARD_CLEANING_THRESHOLD_FACTOR_10,
													_allocToTraceRateNormal);
	_allocToTraceRateMinFactor = (float)1 / interpolateInRange(MIN_ALLOC_2_TRACE_RATE_1,
												        		MIN_ALLOC_2_TRACE_RATE_8,
																MIN_ALLOC_2_TRACE_RATE_10,
																_allocToTraceRateNormal);
	_allocToTraceRateMaxFactor = interpolateInRange(MAX_ALLOC_2_TRACE_RATE_1,
													MAX_ALLOC_2_TRACE_RATE_8,
													MAX_ALLOC_2_TRACE_RATE_10,
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

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	/* Has user elected to run with an LOA ? */
	if (_extensions->largeObjectArea) {
		if (_extensions->concurrentMetering == MM_GCExtensionsBase::METER_DYNAMIC) {

			/* Get storage for metering history table */
			uintptr_t historySize = _meteringHistorySize * sizeof(MeteringHistory);

			_meteringHistory = (MeteringHistory *)env->getForge()->allocate(historySize, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

			if(!_meteringHistory) {
				goto error_no_memory;
			}

			/* Clear storage to zeroes */
			memset(_meteringHistory, 0, historySize);

			/* and initialize first free slot */
			_currentMeteringHistory = 0;
		} else if (_extensions->concurrentMetering == MM_GCExtensionsBase::METER_BY_LOA) {
			_meteringType = LOA;
		}
	}
#endif /* OMR_GC_LARGE_OBJECT_AREA) */

	/* Register on any hook we are interested in */
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CARD_CLEANING_PASS_2_START, hookCardCleanPass2Start, OMR_GET_CALLSITE(), (void *)this);

	return true;

error_no_memory:
	return false;
}

/**
 * Teardown a ConcurrentGC object
 * Destroy referenced objects (CardTable and ConcurrenRAS) and release
 * all allocated storage before ConcurrentGC object is freed.
 */
void
MM_ConcurrentGC::tearDown(MM_EnvironmentBase *env)
{
	OMR::GC::Forge *forge = env->getForge();

	if (NULL != _cardTable){
		_cardTable->kill(env);
		_cardTable= NULL;
		_extensions->cardTable = NULL;
	}

	if (NULL != _conHelpersTable) {
		forge->free(_conHelpersTable);
		_conHelpersTable = NULL;
	}

	if (NULL != _initRanges) {
		forge->free(_initRanges);
		_initRanges = NULL;
	}

	if (NULL != _callback) {
		_callback->kill(env);
		_callback = NULL;
	}

	/* ..and then tearDown our super class */
	MM_ParallelGlobalGC::tearDown(env);
}

bool
MM_ConcurrentGC::createCardTable(MM_EnvironmentBase *env)
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


/**
 * Interpolate value of a tuning factor.
 * Interpolate the value of a tuning factor within a given range based on
 * a given concurrent trace rate.
 *
 * @param val1 Lower bound of range for factor if trace rate <= 8
 * @param val8 Upper bound of range if trace rate <= 8; lower bound if > 8
 * @param val10 Upper bound of range if trace rate > 8
 * @traceRate The concurrent trace rate
 *
 * @return The value of the factor to be used for given trace rate
 */
MMINLINE float
MM_ConcurrentGC::interpolateInRange(float val1, float val8, float val10, uintptr_t traceRate)
{
	float ret;

	if (traceRate > 8) {
		ret = (float)(val8 + ( ((val10 - val8) / 2.0) * (traceRate-8) ) );
	} else {
		ret = (float)(val1 + ( ((val8 - val1)  / 7.0) * (traceRate-1) ) );
	}

	return ret;
}

/**
 * Determine concurrent initialization work
 * Populate the _initRanges structure with the areas of storage which need
 * initializing at each concurrent kickoff. Currently mark bits and card table.
 * For mark bits we create one entry in _initRanges for each segment of heap.
 * The mark bits for areas which are concurrently collectable, eg OLD,  will be
 * set OFF; but all mark bits areas not concurrent collectable, eg NEW, will be
 * set ON so that concurrent does not trace into these areas.
 * For the card table we create one entry for each segment of heap which is concurrently
 * collectable. All associated cards in card table will be set to clean (0x00).
 */
void
MM_ConcurrentGC::determineInitWork(MM_EnvironmentBase *env)
{
	bool initDone= false;

	Trc_MM_ConcurrentGC_determineInitWork_Entry(env->getLanguageVMThread());
	
	while (!initDone) {
		uint32_t i = 0;
		_numInitRanges = 0;

		/* Add init ranges for all old and new area segments */
		MM_HeapRegionDescriptor *region = NULL;
		MM_Heap *heap = _extensions->heap;
		MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
		GC_HeapRegionIterator regionIterator(regionManager);

		while(NULL != (region = regionIterator.nextRegion())) {
			if (0 == region->getSize()) {
				continue;
			}
			/* Get reference to owning subspace */
			MM_MemorySubSpace *subspace = region->getSubSpace();

			/* If space in initRanges array add it */
			if (_numInitRanges < _numPhysicalInitRanges) {
				_initRanges[i].base = region->getLowAddress();
				_initRanges[i].top = region->getHighAddress();
				_initRanges[i].subspace = subspace;
				_initRanges[i].current = _initRanges[i].base;
				_initRanges[i].initBytes = _markingScheme->numMarkBitsInRange(env,_initRanges[i].base,_initRanges[i].top);
				_initRanges[i].type = MARK_BITS;
				_initRanges[i].chunkSize = INIT_CHUNK_SIZE * _markingScheme->numHeapBytesPerMarkMapByte();
				i++;
			}

			/* If the segment is for a concurrently collectable subspace we will
			 * have some cards to clear too */
			if (subspace->isConcurrentCollectable()) {
				_numInitRanges += 2; /* We will have some cards to clean as well */
			} else {
				_numInitRanges += 1;
			}
		}

		/* Any room left ? */
		if (_numInitRanges > _numPhysicalInitRanges) {
			/* We need to get a bigger initRanges array of i+1 elements but first
			 * free the one if have already, if any
			 */
			if (_initRanges) {
				env->getForge()->free(_initRanges);
			}

			/* TODO: dynamically allocating this structure.  Should the VM tear itself down
			 * in this scenario?
			 */
			_initRanges = (InitWorkItem *) env->getForge()->allocate(sizeof(InitWorkItem) * _numInitRanges, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
			if (NULL == _initRanges) {
				initDone = true;
				_numPhysicalInitRanges = 0;
				_numInitRanges = 0;
			} else {
				_numPhysicalInitRanges = _numInitRanges;
			}
		} else {
			/* Add init ranges for all card table ranges for concurrently collectable segments */
			for (I_32 x=i-1; x >= 0; x--) {
				if ((_initRanges[x].type == MARK_BITS) && ((_initRanges[x].subspace)->isConcurrentCollectable())) {
					_initRanges[i].base = _initRanges[x].base;
					_initRanges[i].top = _initRanges[x].top;
					_initRanges[i].current = _initRanges[i].base;
					_initRanges[i].subspace = _initRanges[x].subspace;
					_initRanges[i].initBytes = ((MM_ConcurrentCardTable *)_cardTable)->cardBytesForHeapRange(env,_initRanges[i].base,_initRanges[i].top);
					_initRanges[i].type = CARD_TABLE;
					_initRanges[i].chunkSize = INIT_CHUNK_SIZE * CARD_SIZE;
					i++;
				}
			}
			_nextInitRange = 0;
			initDone = true;
		}
	}

	/* Now count total initailization work we have to do */
	uintptr_t initWork = 0;
	for (uint32_t i = 0; i < _numInitRanges; i++) {
		if (_initRanges[i].base != NULL) {
			initWork += _initRanges[i].initBytes;
		}
	}

	_stats.setInitWorkRequired(initWork);
	_rebuildInitWorkForAdd = false;
	_rebuildInitWorkForRemove = false;

	Trc_MM_ConcurrentGC_determineInitWork_Exit(env->getLanguageVMThread());
}

/**
 * Reset initialization work ranges for concurrent KO.
 * Reset all ranges in _initRanges structure prior to next concurrent kickoff,
 * i.e reset "current" to "base" for all ranges.
 */
void
MM_ConcurrentGC::resetInitRangesForConcurrentKO()
{
	for (uint32_t i=0; i < _numInitRanges; i++) {
		_initRanges[i].current= _initRanges[i].base;
	}

	/* Reset scan ptr */
	_nextInitRange= 0;
}

/**
 * Reset initialization work ranges for STW collect.
 * Reset any non concurrently collectbale heap ranges, eg new heap ranges,
 * in _initRanges structure so we can reset their associated mark bits OFF
 * prior to a full collection. We dont want to trace into such objects during
 * the concurrent mark cycle but we must do so during a full collection.
 */
void
MM_ConcurrentGC::resetInitRangesForSTW()
{
	for (uint32_t i=0; i < _numInitRanges; i++) {
		if (MARK_BITS  == _initRanges[i].type && (!((_initRanges[i].subspace)->isConcurrentCollectable())) ) {
			_initRanges[i].current= _initRanges[i].base;
		} else {
			/* Set current == top so we dont bother doing any re-init of these areas */
			_initRanges[i].current= _initRanges[i].top;
		}
	}

	/* Reset scan ptr */
	_nextInitRange = 0;
}

/**
 * Get next initialization range
 * Determine the next chunk of storage that need initializing. We break the
 * work up into INIT_CHUNK_SIZE sized chunks although is remainder in range
 * after taking INIT_CHUNK_SIZE is less than 1K we complete the whole range.
 *
 * @param from  Pointer to start address of initialization chunk
 * @param to    Pointer to end  address of initialization chunk
 * @param type  Identifier of type of range to be initialized
 * @param concurrentCollectable TRUE if heap range is concurrently collectible;
 * FALSE otherwise.
 * @return TRUE if work left to be done, FALSE otherwise.
 */
bool
MM_ConcurrentGC::getInitRange(MM_EnvironmentBase *env, void **from, void **to, InitType *type, bool *concurrentCollectable )
{
    uint32_t i;
    void *localTo,*localFrom;
    uint32_t oldIndex, newIndex;

    Trc_MM_ConcurrentGC_getInitRange_Entry(env->getLanguageVMThread());
    
	/* Cache _numInitRanges as it may be changed by another thread */
	i= (uint32_t)_nextInitRange;
	while(i < _numInitRanges) {
		/* Cache ptr to next range */
		localFrom = (void *)_initRanges[i].current;
		if (localFrom < _initRanges[i].top) {
			uintptr_t chunkSize = _initRanges[i].chunkSize;

			if ( (uintptr_t)((uint8_t *)_initRanges[i].top - (uint8_t *)localFrom) <= chunkSize) {
				localTo = _initRanges[i].top;
			} else {
				localTo = (void *)((uint8_t *)localFrom + chunkSize);

				/* Finish off range if remaining part is less than half a chunk
			 	*/
				if(localTo >= (void *)((uint8_t *)_initRanges[i].top - (chunkSize/2))) {
					localTo = _initRanges[i].top;
				}
			}

			if(localFrom == (uint8_t *)MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&(_initRanges[i].current), (uintptr_t)localFrom, (uintptr_t)localTo)) {
				/* Got the range so return details */
				*from = localFrom;
				*to = localTo;
				*type= _initRanges[i].type;
				*concurrentCollectable = (_initRanges[i].subspace)->isConcurrentCollectable() ? true : false;

				Trc_MM_ConcurrentGC_getInitRange_Succeed(env->getLanguageVMThread(), *from, *to, *type, *concurrentCollectable ? "true" : "false");

				return true;
			}
		} else {
			/* Get next range, if any */
			oldIndex = i;
			newIndex = oldIndex + 1;
			/* Dont care if this fails, it just means another thread someone beat us to it */
			MM_AtomicOperations::lockCompareExchangeU32(&_nextInitRange, oldIndex, newIndex);
			/* ..and refresh local cached value, may have been chnaged by other threads */
			i = (uint32_t)_nextInitRange;
		}
	}

	Trc_MM_ConcurrentGC_getInitRange_Fail(env->getLanguageVMThread());
	
	return false;
}

/**
 * Determine if trace rate has dropped
 * Determine if we have passed the peak of tracing activity, i.e
 * majority of roots traced.
 *
 * @return TRUIE if we are past the peak; FALSE otherwise
 */
bool
MM_ConcurrentGC::tracingRateDropped(MM_EnvironmentBase *env)
{
	// Always return false for now until we understand how to fix this code to ensure
	// we do not prematurely report that the rate has dropped and so start card cleaning
	// too early.
	return false;

#if 0
	/* TODO CRGTMP these two variable are never used since this code is no longer used.  Should
	 * this function and those vaiables be removed?
	 */
	if (_lastAverageAlloc2TraceRate < (_maxAverageAlloc2TraceRate / 4)) {
		return true;
	} else {
		return false;
	}
#endif
}

MMINLINE MM_ConcurrentGC::ConHelperRequest
MM_ConcurrentGC::switchConHelperRequest(ConHelperRequest from, ConHelperRequest to)
{
	ConHelperRequest result = to;

	omrthread_monitor_enter(_conHelpersActivationMonitor);
	if (from == _conHelpersRequest) {
		_conHelpersRequest = to;
	} else {
		result = _conHelpersRequest;
	}
	omrthread_monitor_exit(_conHelpersActivationMonitor);

	return result;
}

MMINLINE MM_ConcurrentGC::ConHelperRequest
MM_ConcurrentGC::getConHelperRequest(MM_EnvironmentBase *env)
{
	ConHelperRequest result;

	omrthread_monitor_enter(_conHelpersActivationMonitor);
	if (env->isExclusiveAccessRequestWaiting()) {
		if (CONCURRENT_HELPER_MARK == _conHelpersRequest) {
			_conHelpersRequest = CONCURRENT_HELPER_WAIT;
		}
	}
	result = _conHelpersRequest;
	omrthread_monitor_exit(_conHelpersActivationMonitor);

	return result;
}

void
MM_ConcurrentGC::conHelperEntryPoint(OMR_VMThread *omrThread, uintptr_t slaveID)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrThread);
	ConHelperRequest request = CONCURRENT_HELPER_WAIT;
	uintptr_t sizeTraced = 0;
	uintptr_t totalScanned = 0;
	uintptr_t sizeToTrace = 0;
	MM_SpinLimiter spinLimiter(env);

	/* Thread not a mutator so identify its type */
	env->setThreadType(CON_MARK_HELPER_THREAD);

	while (CONCURRENT_HELPER_SHUTDOWN != request) {

		omrthread_monitor_enter(_conHelpersActivationMonitor);
		while (CONCURRENT_HELPER_WAIT == (request = _conHelpersRequest)) {
			omrthread_monitor_wait(_conHelpersActivationMonitor);
		}
		omrthread_monitor_exit(_conHelpersActivationMonitor);

		if (CONCURRENT_HELPER_SHUTDOWN == request) {
			continue;
		}

		env->acquireVMAccess();
		request = getConHelperRequest(env);
		if (CONCURRENT_HELPER_MARK != request) {
			env->releaseVMAccess();
			continue;
		}
		Assert_GC_true_with_message(env, CONCURRENT_OFF != _stats.getExecutionMode(), "MM_ConcurrentStats::_executionMode = %zu\n", _stats.getExecutionMode());

		sizeTraced = 0;
		totalScanned = 0;
		sizeToTrace = _tuningUpdateInterval;

		reportConcurrentBackgroundThreadActivated(env);

		spinLimiter.reset();

		/* perform trace work */
		while ((CONCURRENT_HELPER_MARK == request)
				&& _markingScheme->getWorkPackets()->inputPacketAvailable(env)
				&& spinLimiter.spin()) {
			sizeTraced = localMark(env, sizeToTrace);
			if (sizeTraced > 0 ) {
				_stats.incConHelperTraceSizeCount(sizeTraced);
				totalScanned += sizeTraced;
				spinLimiter.reset();
			}
			request = getConHelperRequest(env);
		}

		spinLimiter.reset();

		/* clean cards */
		while ((CONCURRENT_HELPER_MARK == request)
				&& (CONCURRENT_CLEAN_TRACE == _stats.getExecutionMode())
				&& _cardTable->isCardCleaningStarted()
				&& !_cardTable->isCardCleaningComplete()
				&& spinLimiter.spin()) {
			if (cleanCards(env, false, _conHelperCleanSize, &sizeTraced, false)) {
				if (sizeTraced > 0 ) {
					_stats.incConHelperCardCleanCount(sizeTraced);
					totalScanned += sizeTraced;
					spinLimiter.reset();
				}
			}
			request = getConHelperRequest(env);
		}

		if (CONCURRENT_HELPER_MARK == request) {
			request = switchConHelperRequest(CONCURRENT_HELPER_MARK, CONCURRENT_HELPER_WAIT);
		}
		Assert_MM_true(CONCURRENT_HELPER_MARK != request);

		reportConcurrentBackgroundThreadFinished(env, totalScanned);
		env->releaseVMAccess();
	} // end while (SHUTDOWN != request)
	
	/*
	 * Must be no local work left at this point!
	 */
	Assert_MM_true(!env->_workStack.inputPacketAvailable());
	Assert_MM_true(!env->_workStack.outputPacketAvailable());
	Assert_MM_true(!env->_workStack.deferredPacketAvailable());


	/* All done, so shutdown the helper threads */
	shutdownAndExitConHelperThread(omrThread);
}

/**
 * Shutdown and exit a concurrent helper
 * Detach a concurrent helper. Notify _conHelpersActivationMonitor if this is
 * last concurrent helper to shutdown.
 *
 */
void
MM_ConcurrentGC::shutdownAndExitConHelperThread(OMR_VMThread *omrThread)
{
	MM_EnvironmentBase::detachVMThread(_extensions->getOmrVM(), omrThread, MM_EnvironmentBase::ATTACH_GC_HELPER_THREAD);
	omrthread_monitor_enter(_conHelpersActivationMonitor);
	_conHelpersShutdownCount += 1;
	/* The last thread to shut down must notify the master thread */
	if (_conHelpersShutdownCount == _conHelpersStarted) {
		omrthread_monitor_notify(_conHelpersActivationMonitor);
	}

	/* Clear the entry in _conHelpersTable containing the thread id,
	 * so that other parts of code know now this thread is gone.
	 * The thread itself does not know which entry in the table it is,
	 * so we search the table. */
	for (uint32_t i=0; i < _conHelpersStarted; i++) {
		if (_conHelpersTable[i] == omrthread_self()) {
			_conHelpersTable[i] = 0;
			break;
		}
	}

	/* Exit the monitor and terminate the thread */
	omrthread_exit(_conHelpersActivationMonitor);
}

/**
 * Initialize concurrent helper threads.
 * Attach the required number of concurrent helper threads which are attached with
 * minimum priority so they do not compete with applciation threads for use of procesors;
 * they are only intended to use up any spare cycles not used by application threads.
 *
 * @return TRUE if concurrent helper thread attached and initailzed OK;
 * FALSE otherwise
 */
bool
MM_ConcurrentGC::initializeConcurrentHelpers(MM_GCExtensionsBase *extensions)
{
	/* If user has elected to run without helpers then nothing to do */
	if (0 == _conHelperThreads) {
		return true;
	}

	IDATA threadForkResult;
	uint32_t conHelperThreadCount;
	ConHelperThreadInfo conHelperThreadInfo;

	/*
	 * Attach the concurrent helper thread threads
	 */
	conHelperThreadInfo.omrVM = extensions->getOmrVM();

	omrthread_monitor_enter(_conHelpersActivationMonitor);
	_conHelpersRequest = CONCURRENT_HELPER_WAIT;

	for(conHelperThreadCount = 0; conHelperThreadCount < _conHelperThreads;	conHelperThreadCount++) {
		conHelperThreadInfo.threadFlags = 0;
		conHelperThreadInfo.threadID = conHelperThreadCount;
		conHelperThreadInfo.collector = this;

		threadForkResult = createThreadWithCategory(&(_conHelpersTable[conHelperThreadCount]),
							OMR_OS_STACK_SIZE,
							J9THREAD_PRIORITY_MIN,
							0,
							con_helper_thread_proc,
							(void *)&conHelperThreadInfo,
							J9THREAD_CATEGORY_SYSTEM_GC_THREAD);
		if (threadForkResult != 0) {
			break;
		}

		do {
			omrthread_monitor_wait(_conHelpersActivationMonitor);
		} while (!conHelperThreadInfo.threadFlags);

		if(conHelperThreadInfo.threadFlags != CON_HELPER_INFO_FLAG_OK ) {
			break;
		}
	}
	omrthread_monitor_exit(_conHelpersActivationMonitor);
	_conHelpersStarted = conHelperThreadCount;

	return (_conHelpersStarted == _conHelperThreads ? true : false);
}

/**
 * Shutdown all concurrent helper threads.
 * Ask all active concurrent helper threads to terminate.
 */
void
MM_ConcurrentGC::shutdownConHelperThreads(MM_GCExtensionsBase *extensions)
{
	Trc_MM_shutdownConHelperThreads_Entry();

	if (_conHelpersStarted > 0) {
		omrthread_monitor_enter(_conHelpersActivationMonitor);

		/* Set shutdown request flag */
		_conHelpersRequest = CONCURRENT_HELPER_SHUTDOWN;
		_conHelpersShutdownCount = 0;

		omrthread_monitor_notify_all(_conHelpersActivationMonitor);

		/* Now wait for all concurrent helper threads to terminate */
		while (_conHelpersShutdownCount < _conHelpersStarted) {
			omrthread_monitor_wait(_conHelpersActivationMonitor);
		}
		omrthread_monitor_exit(_conHelpersActivationMonitor);
	}

	Trc_MM_shutdownConHelperThreads_Exit();
}

/**
 * Resume the concurrent helper threads.
 * Concurrent kickoff point has been reached so wake up any Concurrent helper
 * threads which are waiting on the _conHelpersActivationMonitor monitor.
 *
 */
void
MM_ConcurrentGC::resumeConHelperThreads(MM_EnvironmentBase *env)
{
	if (_conHelpersStarted > 0) {
		omrthread_monitor_enter(_conHelpersActivationMonitor);
		if (!env->isExclusiveAccessRequestWaiting()) {
			if (CONCURRENT_HELPER_WAIT == _conHelpersRequest) {
				_conHelpersRequest = CONCURRENT_HELPER_MARK;
				omrthread_monitor_notify_all(_conHelpersActivationMonitor);
			}
		}
		omrthread_monitor_exit(_conHelpersActivationMonitor);
	}
}

/**
 * Tune the concurrent adaptive parameters.
 * Using historical data attempt to predict how much work (both tracing and
 * card cleaning) will need to be performed during the next concurrent mark cycle.
 */
void
MM_ConcurrentGC::tuneToHeap(MM_EnvironmentBase *env)
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
		assume0(!_stwCollectionInProgress);
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

	/* If heap has changed we need to recalculate the initialization work for
	 * the next cycle now so we get most accurate estimate for trace size target
	 */
	if (_rebuildInitWorkForAdd || _rebuildInitWorkForRemove) {
		if (_extensions->isConcurrentScavengerInProgress()) {
			/* This should really occur in a middle of concurrent phase of Scavenger, 
			 * when expanding tenure due to failed tenuring */
			Assert_MM_true(_rebuildInitWorkForAdd);
			/* Hold _initWorkMonitor to prevent any thread from starting/joining/leaving init work,
			 * since we are about to change the _initRange table (that initializer threads consume) */
			omrthread_monitor_enter(_initWorkMonitor);

			/* If there are no active initializers, we are safe to proceed and modify _initRagne table,
			 * otherwise skip it (rebuild flag will remain set, and the table will be reconfigurated
			 * at a later, safe point (like beginning of STW phase).
			 * Even if happens to be 0 initializers, but we are still doing the init, skip it.
			 * It is safe for heap expansion (caller of this method already did initialization 
			 * for that  part of the memory), and more importantly it will not trigger unncessary
			 * redo of the init work, if we were to reset the able in a middle of init phase. */
			if ((0 == _initializers) && (_stats.getExecutionMode() != CONCURRENT_INIT_RUNNING)) {
				determineInitWork(env);
			}
			omrthread_monitor_exit(_initWorkMonitor);
		} else {
			Assert_MM_true(0 == _initializers);
			determineInitWork(env);
		}
	} else {
		/* ..else just reset for next cycle */
		resetInitRangesForConcurrentKO();
	}

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

	_initSetupDone = false;

	/* Reset all ConcurrentStats for next cycle */
	_stats.reset();

	_totalTracedAtPass2KO = HIGH_VALUES;
	_totalCleanedAtPass2KO = HIGH_VALUES;
	_pass2Started = false;

	_alloc2ConHelperTraceRate = 0;
	_lastConHelperTraceSizeCount = 0;
	_lastAverageAlloc2TraceRate = 0;
	_maxAverageAlloc2TraceRate = 0;
    _lastFreeSize = LAST_FREE_SIZE_NEEDS_INITIALIZING;
	_lastTotalTraced = 0;

	Trc_MM_ConcurrentGC_tuneToHeap_Exit2(env->getLanguageVMThread(), _stats.getTraceSizeTarget(), _stats.getInitWorkRequired(), _stats.getKickoffThreshold());
}

/**
 * Adjust the current trace target after heap change.
 * The heap has been reconfigured (i.e expanded or contracted) midway through a
 * concurrent cycle so we need to re-calculate the trace  target so the trace
 * ate is adjusted accordingly on subsequent allocations.
 */
void
MM_ConcurrentGC::adjustTraceTarget()
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

/**
 * Update tuning statistics at end of a concurrent cycle.
 *
 * Calculate the proportion of live objects in heap and the proportion of non-leaf
 * objects in the heap at the end of this collection. These factors are used
 * if the heap changes in size to predict how much of new heap will need to be
 * traced before next collection.
 *
 * Also, reset the card cleaning factors based on cards we cleaned this cycle.
 *
 */
void
MM_ConcurrentGC::updateTuningStatistics(MM_EnvironmentBase *env)
{
	/* Dont update statistics if its a system GC or we aborted a concurrent collection cycle
	 */
    if (env->_cycleState->_gcCode.isExplicitGC() || (CONCURRENT_TRACE_ONLY > _stats.getExecutionModeAtGC())) {
        return;
    } else {
		/* Get current heap size and free bytes */
		MM_Heap *heap = _extensions->heap;
		uintptr_t heapSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);
		uintptr_t freeSize = heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
		uintptr_t totalLiveObjects = heapSize - freeSize;
		uintptr_t bytesTraced = 0;
		uintptr_t totalTraced = 0;
		uintptr_t totalCleaned = 0;
		uintptr_t totalTracedPass1 = 0;
		uintptr_t totalCleanedPass1 = 0;
		uintptr_t totalCleanedPass2 = 0;
		float newLiveObjectFactor = 0.0;
		float newNonLeafObjectFactor = 0.0;
		float newCardCleaningFactorPass1 = 0.0;
		float newCardCleaningFactorPass2 = 0.0;
		float newBytesTracedInPass1Factor = 0.0;

		newLiveObjectFactor = (float)totalLiveObjects / (float)heapSize;
		_tenureLiveObjectFactor = MM_Math::weightedAverage(_tenureLiveObjectFactor, newLiveObjectFactor, LIVE_PART_HISTORY_WEIGHT);

		/* Work out the new _tenureNonLeafObjectFactor based on
		 * how much we actually traced and the amount of live bytes
		 * found by collector
		*/
		bytesTraced = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();

		/* If we did not complete mark add in those we missed. This will be an over estimate
		 * but will get us back on track
		 */
		if (CONCURRENT_EXHAUSTED > _stats.getExecutionModeAtGC()) {
			bytesTraced += _stats.getFinalTraceCount();
		}

		newNonLeafObjectFactor = (float)bytesTraced / (float)totalLiveObjects;
		_tenureNonLeafObjectFactor = MM_Math::weightedAverage(_tenureNonLeafObjectFactor, newNonLeafObjectFactor, NON_LEAF_HISTORY_WEIGHT);

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
}

/**
 * Calculate allocation tax during initialization phase
 * Calculate how much initialization work should be done by a mutator for the current
 * allocation request. We dont need to worry about work being available at the right time
 * as we do when tracing. Initialization work will either be available or it will be all
 * allocated; one exhausted no more will arrive at a later time as is the case with tracing.
 * We boost the target trace rate during initialization in order to minimize the time to
 * complete the initialization phase.
 *
 * @param allocationSize  the size of the allocation
 * @return the allocation tax
 */
uintptr_t
MM_ConcurrentGC::calculateInitSize(MM_EnvironmentBase *env, uintptr_t allocationSize)
{
	/* During initialization we boost the target trace rate in order to complete init ASAP */
	return allocationSize * _allocToInitRate;
}

/**
 * Calculate allocation tax.
 * Calculate how much data should be traced by a mutator for the current
 * allocation request.
 *
 * @return the allocation tax
 */
uintptr_t
MM_ConcurrentGC::calculateTraceSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	float thisTraceRate;
	uintptr_t sizeToTrace, remainingFree, workCompleteSoFar, traceTarget;
	uintptr_t allocationSize = allocDescription->getAllocationTaxSize();

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* Allocation tax for nursery allocation is based on potential nursery
	 * free space as for every N btyes of nursery we allocate we only
	 * tenure a proportion of it. Tenure area allocations are always taxed
	 * based on current amount of "taxable" free space left.
	 */
	if(allocDescription->isNurseryAllocation()) {
		remainingFree = potentialFreeSpace(env, allocDescription);
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

	/* We need to adjust the remaining free by same amount we boosted KO threshold
	 * so that the trace rate calculation works out. Basically we are attempting to ensure
	 * that all work is assigned to mutators by the time free space drops to
	 * _kickoffThresholdBuffer.
	 */
	 remainingFree = (remainingFree > _kickoffThresholdBuffer) ? remainingFree - _kickoffThresholdBuffer : 0;

	/* Calculate the size to trace for this alloc based on the
	 * trace target and how much work has been already completed.
	 */
	workCompleteSoFar = (_stats.getTraceSizeCount() + _stats.getCardCleanCount() +
						 _stats.getConHelperTraceSizeCount() + _stats.getConHelperCardCleanCount());

	/* Calculate how much work we need to get through */
	traceTarget = _pass2Started ? _traceTargetPass1 + _traceTargetPass2 : _traceTargetPass1;

	/* Provided we are not into buffer zone already and we have not already done more work
	 * than we predicted calculate required trace rate for this allocate request to keep us on
	 * track.
	 */
	if ((remainingFree > 0) && (workCompleteSoFar < traceTarget)) {

		thisTraceRate = (float)((traceTarget - workCompleteSoFar) / (float)(remainingFree));

		if ( thisTraceRate > _allocToTraceRate) {
	    /* The "over tracing" should not only adjust to the current ratio between
	     * free space and estimated remaining tracing, but also try to do even more tracing, in
	     * order to correct the ratio back to the required alloc to trace rate.
	     */
	    	thisTraceRate += ((thisTraceRate - _allocToTraceRate) * OVER_TRACING_BOOST_FACTOR);
			/* Make sure its not now greater than max */
			if(thisTraceRate > getAllocToTraceRateMax()) {
				thisTraceRate = getAllocToTraceRateMax();
			}
		} else 	if(thisTraceRate < getAllocToTraceRateMin()) {
			thisTraceRate = getAllocToTraceRateMin();
		}

		if(_forcedKickoff) {
			/* in case of external kickoff use at least default trace rate */
			if( thisTraceRate < getAllocToTraceRateNormal()) {
				thisTraceRate = getAllocToTraceRateNormal();
			}
		} 
		
		/* Provided background thread is not already doing enough tracing ....*/
		if (thisTraceRate > _alloc2ConHelperTraceRate) {
			/* ..calculate tax for mutator taking into account any tracing being done by concurrent helpers */
			sizeToTrace = (uintptr_t)(allocationSize * (thisTraceRate - _alloc2ConHelperTraceRate));
		} else {
			/* Background thread is doing enough tracing on its own so mutator gets away without paying any tax */
			sizeToTrace = 0;
		}
	} else {
		/* We are already in buffer zone of free space or we have already done all work we predicted and we are
		 * still not done. We run a high risk of hitting AF before we do complete so trace at the max from
		 * now on in an attempt to complete the work before we do hit AF.
		 */
		 sizeToTrace = (uintptr_t)(allocationSize * getAllocToTraceRateMax());
	}

	if(sizeToTrace > _maxTraceSize) {
		sizeToTrace = _maxTraceSize;
	}

	return sizeToTrace;
}

/**
 * Determine if its time to do periodical tuning.
 * Has the free space reduced by the _tuningUpdateInterval from the last time
 * we did periodical tuning, or do we need to initailize for a new concurrent
 * cycle.
 *
 * @param freeSize The current amount of free space in the old area
 * @return TRUE if its time for periodical tuning; FALSE otherwise
 */
bool
MM_ConcurrentGC::periodicalTuningNeeded(MM_EnvironmentBase *env, uintptr_t freeSize)
{
	/* Is it time to update statistics. If _lastFreeSize <= freeSize another thread
	 * has already updated statistics for this interval
	 */

	if (LAST_FREE_SIZE_NEEDS_INITIALIZING ==_lastFreeSize  ||
        ( (_lastFreeSize > freeSize) &&
          (_lastFreeSize - freeSize > _tuningUpdateInterval))) {
		return true;
	}

	return false;
}

#if defined(OMR_GC_MODRON_SCAVENGER)
/**
 * Calculate potential free space.
 * Calculate an estimate of the number of bytes that can be allocated in the
 * new area before the old area is exhausted.
 *
 * @return  Number of bytes available for allocation before old area is exhausted
 */
MMINLINE uintptr_t
MM_ConcurrentGC::potentialFreeSpace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
		uintptr_t nurseryPromotion, nurseryInitialFree, currentOldFree, currentNurseryFree;
		MM_MemorySpace *memorySpace = env->getExtensions()->heap->getDefaultMemorySpace();
		MM_MemorySubSpace *oldSubspace = memorySpace->getTenureMemorySubSpace();
		MM_MemorySubSpace *newSubspace = memorySpace->getDefaultMemorySubSpace();
		MM_ScavengerStats *scavengerStats = &_extensions->scavengerStats;

		/* Have we done at least 1 scavenge ? If not no statistics available so return high values */
		if (0 == scavengerStats->_gcCount) {
			return (uintptr_t)-1;
		}
#if defined(OMR_GC_LARGE_OBJECT_AREA)
		/* Do we need to tax this allocation ? */
		if (LOA == _meteringType) {
			nurseryPromotion = scavengerStats->_avgTenureLOABytes == 0 ? 1 : scavengerStats->_avgTenureLOABytes;
			currentOldFree = oldSubspace->getApproximateActiveFreeLOAMemorySize();
		} else {
			assume0(SOA == _meteringType);
			nurseryPromotion = scavengerStats->_avgTenureSOABytes == 0 ? 1 : scavengerStats->_avgTenureSOABytes;
			currentOldFree = oldSubspace->getApproximateActiveFreeMemorySize() - oldSubspace->getApproximateActiveFreeLOAMemorySize();
		}
#else
		nurseryPromotion = scavengerStats->_avgTenureBytes == 0 ? 1: scavengerStats->_avgTenureBytes;
		currentOldFree = oldSubspace->getApproximateActiveFreeMemorySize();
#endif
		/* reduce oldspace free memory by fragmented estimation */
		MM_LargeObjectAllocateStats *stats = oldSubspace->getMemoryPool()->getLargeObjectAllocateStats();
		if (NULL != stats) {
			uintptr_t fragmentation = (uintptr_t) ((env->getExtensions())->concurrentSlackFragmentationAdjustmentWeight * stats->getRemainingFreeMemoryAfterEstimate());
			if (currentOldFree > fragmentation) {
				currentOldFree -= fragmentation;
			} else {
				currentOldFree = 0;
			}
		}
		nurseryInitialFree = scavengerStats->_avgInitialFree;
		currentNurseryFree =  newSubspace->getApproximateFreeMemorySize();

		/* Calculate the number of scavenge's before we will tenure enough objects to
		 * fill the old space. If we know next scavenge will percolate then we have no scavenges
		 * remaining before concurren KO is required
		 */
		uintptr_t scavengesRemaining;
		if (scavengerStats->_nextScavengeWillPercolate) {
			scavengesRemaining = 0;
			_stats.setKickoffReason(NEXT_SCAVENGE_WILL_PERCOLATE);
			_languageKickoffReason = NO_LANGUAGE_KICKOFF_REASON;
		} else {
			scavengesRemaining =  (uintptr_t)(currentOldFree/nurseryPromotion);
		}

		/* KO concurrent one scavenge early to reduce chances of tenure space being exhausted
		 * before we complete concurrent marking by a scavenge tenuring significantly more than
		 * the average number of bytes.
		 */
		scavengesRemaining = scavengesRemaining > 0  ?  scavengesRemaining - 1 : 0;

		/* Now calculate how many bytes we can therefore allocate in the nursery before
		 * we will fill the tenure area. On 32 bit platforms this can be greater than 4G
		 * hence the 64 bit maths. We assume that the heap will not be big enough on 64 bit
		 * platforms to overflow 64 bits!
		 */

		uint64_t potentialFree = (uint64_t)currentNurseryFree +
							 ((uint64_t)nurseryInitialFree * (uint64_t)scavengesRemaining);

#if !defined(OMR_ENV_DATA64)
		/* On a 32 bit platforms the amount of free space could be more than 4G. Therefore
		 * if the amount of potential free space is greater than can be expressed in 32
		 * bits we just return 4G.
		 */
		uint64_t maxFree = 0xFFFFFFFF;
		if (potentialFree > maxFree){
			return (uintptr_t)maxFree;
		} else {
			return (uintptr_t)potentialFree;
		}
#else
		return (uintptr_t)potentialFree;
#endif
}

#endif /* OMR_GC_MODRON_SCAVENGER */

/**
 * Adjust the concurrent helper tuning.
 * Determine how much work the concurrent helper has got through in last interval
 * and adjust the counters.
 *
 * @param  freeSize the current amount of free space in the old area
 */
void
MM_ConcurrentGC::periodicalTuning(MM_EnvironmentBase *env, uintptr_t freeSize)
{
	float newConHelperRate;

	/* Single thread this code; ensure update for earlier interval completes
	 * we update for a later interval
	 *  */
	omrthread_monitor_enter(_concurrentTuningMonitor);

	if (_lastFreeSize == LAST_FREE_SIZE_NEEDS_INITIALIZING) {
        _lastFreeSize =  freeSize;
        _tuningUpdateInterval= (uintptr_t)((float)freeSize  * TUNING_HEAP_SIZE_FACTOR);

        if (_tuningUpdateInterval > _maxTraceSize) {
            _tuningUpdateInterval = _maxTraceSize;
        }

        if (_tuningUpdateInterval < _minTraceSize) {
            _tuningUpdateInterval = _minTraceSize;
        }

    } else if ( (_lastFreeSize > freeSize) && (_lastFreeSize - freeSize) >= _tuningUpdateInterval) {
        /* This thread first to update for this interval so calculate
         * total traced so far
         */
		uintptr_t totalTraced = _stats.getTraceSizeCount() + _stats.getCardCleanCount();
		uintptr_t freeSpaceUsed = _lastFreeSize - freeSize;

		/* Update concurrent helper trace rate if we have any */
		if (_conHelpersStarted > 0) {
			uintptr_t conTraced = _stats.getConHelperTraceSizeCount() +  _stats.getConHelperCardCleanCount();
			newConHelperRate =  (float)(conTraced - _lastConHelperTraceSizeCount) / (float) (freeSpaceUsed);
			_lastConHelperTraceSizeCount = conTraced;
			_alloc2ConHelperTraceRate = MM_Math::weightedAverage(_alloc2ConHelperTraceRate,
														newConHelperRate,
														CONCURRENT_HELPER_HISTORY_WEIGHT);

			totalTraced += conTraced;
		}

		_lastAverageAlloc2TraceRate = ((float)(totalTraced - _lastTotalTraced)) / (float)freeSpaceUsed;
		_lastTotalTraced = totalTraced;

		if (_maxAverageAlloc2TraceRate < _lastAverageAlloc2TraceRate) {
			_maxAverageAlloc2TraceRate =  _lastAverageAlloc2TraceRate;
		}

		/* Set for next interval */
		_lastFreeSize = freeSize;
	}

	omrthread_monitor_exit(_concurrentTuningMonitor);
}

/**
 * Start card cleaning.
 * Either free space has reached the card cleaning kickoff level or we have run out
 * of other work so may as well start card cleaning early.
 *
 */
void
MM_ConcurrentGC::kickoffCardCleaning(MM_EnvironmentBase *env, ConcurrentCardCleaningReason reason)
{
	/* Switch to CONCURRENT_CLEAN_TRACE...if we fail someone beat us to it */
	if (_stats.switchExecutionMode(CONCURRENT_TRACE_ONLY, CONCURRENT_CLEAN_TRACE)) {
		_stats.setCardCleaningReason(reason);
		_concurrentDelegate.cardCleaningStarted(env);
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
MM_ConcurrentGC::replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, uintptr_t size)
{
	if(_extensions->concurrentSweep) {
		return _sweepScheme->replenishPoolForAllocate(env, memoryPool, size);
	}
	return false;
}
#endif /* OMR_GC_CONCURRENT_SWEEP */

/**
 * Pay the allocation tax for the mutator.
 * @note This is a potential GC point.
 */
void
MM_ConcurrentGC::payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace,	MM_MemorySubSpace *baseSubSpace, MM_AllocateDescription *allocDescription)
{
	/* Allocation size must be greater than zero */
	assume0(allocDescription->getAllocationTaxSize() >  0);
	/* Thread roots must have been flushed by this point */
	Assert_MM_true(!_concurrentDelegate.flushThreadRoots(env));

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	/* Do we need to tax this allocation ? */
	if ( (LOA == _meteringType && !allocDescription->isLOAAllocation())
	|| (SOA == _meteringType && allocDescription->isLOAAllocation())) {
		/* no tax to pay so return */
		return;
	}
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	/* Check if its time to KO */
	if (CONCURRENT_OFF == _stats.getExecutionMode()) {
		if (!timeToKickoffConcurrent(env, allocDescription)) {
#if defined(OMR_GC_CONCURRENT_SWEEP)
			/* Try to do some concurrent sweeping */
			if(_extensions->concurrentSweep) {
				concurrentSweep(env, baseSubSpace, allocDescription);
			}
#endif /* OMR_GC_CONCURRENT_SWEEP */
			return;
		}
	}

	/* Concurrent marking is active */
	concurrentMark(env, subspace, allocDescription);
	/* Thread roots must have been flushed by this point */
	Assert_MM_true(!_concurrentDelegate.flushThreadRoots(env));
}

/**
 * Mutator pays its allocation tax.
 * Decide what work, if any, this mutator needs to do to pay its allocation tax.
 *
 * @param allocDescription - defines allocation which is to be taxed
 *
 * @note This is a potential GC point.
 */
void
MM_ConcurrentGC::concurrentMark(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_AllocateDescription *allocDescription)
{
	uintptr_t oldVMstate = env->pushVMstate(J9VMSTATE_GC_CONCURRENT_MARK_TRACE);

	/* Get required information from Alloc description */
	uintptr_t allocationSize = allocDescription->getAllocationTaxSize();
	bool threadAtSafePoint = allocDescription->isThreadAtSafePoint();

	/* Sanity check on allocation size and .. */
	assume0(allocationSize > 0);
	/* .. we must not mark anything if WB not yet active */
#if 0	/* TODO 90354: Find a way to reestablish this assertion */
	Assert_MM_true((_stats.getExecutionMode() < CONCURRENT_ROOT_TRACING1) || (((J9VMThread *)env->getLanguageVMThread())->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE));
#endif

	/* Boost priority of thread whilst we are paying our tax; dont want low priority
	 * application threads holding on to work packets
	 */
	uintptr_t priority = env->getPriority();
	if(priority < J9THREAD_PRIORITY_NORMAL) {
		env->setPriority(J9THREAD_PRIORITY_NORMAL);
	}

	uintptr_t sizeTraced = 0;
	uintptr_t sizeToTrace = 0;
	bool taxPaid = false;

	env->_workStack.prepareForWork(env, _markingScheme->getWorkPackets());
	while (!taxPaid) {

		/* If another thread is waiting for exclusive VM access, possibly
		 * to collect, get out quick so as not to hold things up
		 */
		if(env->isExclusiveAccessRequestWaiting() ) {
			flushLocalBuffers(env);
			break;
		}

		uintptr_t nextExecutionMode = CONCURRENT_TRACE_ONLY;
		uintptr_t executionMode = _stats.getExecutionMode();
		switch (executionMode) {
		case CONCURRENT_OFF:
			taxPaid = true;
			break;

		case CONCURRENT_TRACE_ONLY:
		case CONCURRENT_CLEAN_TRACE:
			sizeToTrace = calculateTraceSize(env, allocDescription);
			if (sizeToTrace > 0) {
				sizeTraced = doConcurrentTrace(env, allocDescription, sizeToTrace, subspace, threadAtSafePoint);
			}

			taxPaid = true;
			break;

		case CONCURRENT_INIT_RUNNING:
			sizeToTrace= calculateInitSize(env, allocationSize);
			sizeTraced = doConcurrentInitialization(env, sizeToTrace);

			/* If we have done at least half then we are done as mutator will have
			 * waited for other initializing mutators to finish
			 */
			if (sizeTraced >= (sizeToTrace/2)) {
				taxPaid = true;
			}
			break;

		case CONCURRENT_INIT_COMPLETE:
			if (_extensions->optimizeConcurrentWB) {
				if(threadAtSafePoint) {
					signalThreadsToDirtyCards(env);
				} else {
					/* Register for this thread to get called back at safe point */
					_callback->requestCallback(env);
					/* ..and return so that thread can get to a safe point */
					taxPaid = true;
				}
			} else {
				/* TODO: Once optimizeConcurrentWB enabled by default this code will be deleted */
				_stats.switchExecutionMode(CONCURRENT_INIT_COMPLETE, CONCURRENT_ROOT_TRACING);
			}
			break;

		case CONCURRENT_EXHAUSTED:
			/* If another thread has set exhausted whilst we were allocating then we can trigger
			 * the final collection here provided we are at a safe point. If not then GC will be
			 * triggered by next allocation.
			 */
			if(threadAtSafePoint) {
				concurrentFinalCollection(env,subspace);
			}

			/* Thats enough for this mutator */
			taxPaid = true;
			break;

		case CONCURRENT_FINAL_COLLECTION:
			/* Nothing to do here as a final collection is happening or about to. */
			taxPaid = true;
			break;

		case CONCURRENT_ROOT_TRACING:
			nextExecutionMode = _concurrentDelegate.getNextTracingMode(CONCURRENT_ROOT_TRACING);
			Assert_GC_true_with_message(env, (CONCURRENT_ROOT_TRACING < nextExecutionMode) || (CONCURRENT_TRACE_ONLY == nextExecutionMode), "MM_ConcurrentMarkingDelegate::getNextTracingMode(CONCURRENT_ROOT_TRACING) = %zu\n", nextExecutionMode);
			if(_stats.switchExecutionMode(CONCURRENT_ROOT_TRACING, nextExecutionMode)) {
				/* Signal threads for async callback to scan stack*/
				_concurrentDelegate.signalThreadsToTraceStacks(env);
				taxPaid = true;
			}
			break;

		default:
			/* Client language defines 1 or more execution modes with values > CONCURRENT_ROOT_TRACING */
			Assert_GC_true_with_message(env, (CONCURRENT_ROOT_TRACING < executionMode) && (CONCURRENT_TRACE_ONLY > executionMode), "MM_ConcurrentStats::_executionMode = %zu\n", executionMode);
			nextExecutionMode = _concurrentDelegate.getNextTracingMode(executionMode);
			if (_stats.switchExecutionMode(executionMode, nextExecutionMode)) {
				Assert_GC_true_with_message2(env, (CONCURRENT_ROOT_TRACING < nextExecutionMode) && (CONCURRENT_TRACE_ONLY >= nextExecutionMode), "MM_ConcurrentStats::_executionMode = %zu; MM_ConcurrentMarkingDelegate::getNextTracingMode(MM_ConcurrentStats::_executionMode) = %zu\n", executionMode, nextExecutionMode);
				/* Collect some roots */
				bool collectedRoots = false;
				_concurrentDelegate.collectRoots(env, executionMode, &collectedRoots, &taxPaid);
				if (collectedRoots) {
					/* Resume concurrent helper threads to help mark any roots we have found. Another
					 * CONCURRENT_ROOT_TRACING thread may have beat us to it but if thread
					 * already active this notify will be ignored
					 */
					resumeConHelperThreads(env);
				}
				if (taxPaid) {
					flushLocalBuffers(env);
				}
				if (CONCURRENT_TRACE_ONLY == nextExecutionMode) {
					_stats.setModeComplete(CONCURRENT_ROOT_TRACING);
				}
			}
			break;
		}
	} /* !taxPaid */

	flushLocalBuffers(env);

	if (_extensions->debugConcurrentMark) {
		_stats.analyzeAllocationTax(sizeToTrace, sizeTraced);
	}

	/* Reset priority of thread if we boosted it on entry */
	if(priority < J9THREAD_PRIORITY_NORMAL) {
		env->setPriority(priority);
	}

	env->popVMstate(oldVMstate);
}

void
MM_ConcurrentGC::signalThreadsToDirtyCards(MM_EnvironmentBase *env)
{
	uintptr_t gcCount = _extensions->globalGCStats.gcCount;

	/* Things may have moved on since async callback requested */
	while (CONCURRENT_INIT_COMPLETE == _stats.getExecutionMode()) {
		/* We may or may not have exclusive access but another thread may have beat us to it and
		 * prepared the threads or even collected.
		 */
		if (env->acquireExclusiveVMAccessForGC(this, true, false)) {
			MM_CycleState *previousCycleState = env->_cycleState;
			_concurrentCycleState = MM_CycleState();
			_concurrentCycleState._type = _cycleType;
			env->_cycleState = &_concurrentCycleState;
			reportGCCycleStart(env);
			env->_cycleState = previousCycleState;

			_concurrentDelegate.signalThreadsToDirtyCards(env);
			_stats.switchExecutionMode(CONCURRENT_INIT_COMPLETE, CONCURRENT_ROOT_TRACING);
			/* Cancel any outstanding call backs on other threads as this thread has done the necessary work */
			_callback->cancelCallback(env);

			env->releaseExclusiveVMAccessForGC();
		}

		if (gcCount != _extensions->globalGCStats.gcCount) {
			break;
		}
	}
}

/**
 * Force Kickoff event externally
 *
 * @return true if Kickoff can be forced
 */
bool
MM_ConcurrentGC::forceKickoff(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode)
{
	if (_extensions->concurrentKickoffEnabled && _concurrentDelegate.canForceConcurrentKickoff(env, gcCode, &_languageKickoffReason)) {
		_stats.setKickoffReason(LANGUAGE_DEFINED_REASON);
		_forcedKickoff = true;
		return true;
	} else {
		return false;
	}
}

/**
 * Decide if we have reached the kickoff threshold for concurrent mark.
 *
 * @note This is a potential GC point.
 *
 * @return TRUE if concurrent KO threshold reached; FALSE  otherwise
 */
bool
MM_ConcurrentGC::timeToKickoffConcurrent(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	uintptr_t remainingFree;

	/* If -Xgc:noConcurrentMarkKO specifed then we never
	 * kickoff concurrent mark
	 */
	if (!_extensions->concurrentKickoffEnabled) {
		return false;
	}

	/* Determine how much "taxable" free space remains to be allocated. */
#if defined(OMR_GC_MODRON_SCAVENGER)
	if(_extensions->scavengerEnabled) {
		remainingFree = potentialFreeSpace(env, allocDescription);
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

	/* If we are already out of storage no point even starting concurrent */
	if (0 == remainingFree) {
		return false;
	}

	if ((remainingFree < _stats.getKickoffThreshold()) || _forcedKickoff) {
#if defined(OMR_GC_CONCURRENT_SWEEP)
		/* Finish off any sweep work that was still in progress */
		completeConcurrentSweepForKickoff(env);
#endif /* OMR_GC_CONCURRENT_SWEEP */

		if(_stats.switchExecutionMode(CONCURRENT_OFF, CONCURRENT_INIT_RUNNING)) {
			_stats.setRemainingFree(remainingFree);
			/* Set kickoff reason if it is not set yet */
			_stats.setKickoffReason(KICKOFF_THRESHOLD_REACHED);
			_languageKickoffReason = NO_LANGUAGE_KICKOFF_REASON;
#if defined(OMR_GC_MODRON_SCAVENGER)
			_extensions->setConcurrentGlobalGCInProgress(true);
#endif
			reportConcurrentKickoff(env);
		}
		return true;
	} else {
		return false;
	}
}

#if defined(OMR_GC_CONCURRENT_SWEEP)
/**
 * Run a concurrent sweep as part of the current allocation tax.
 */
void
MM_ConcurrentGC::concurrentSweep(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_AllocateDescription *allocDescription)
{
	uintptr_t oldVMstate = env->pushVMstate(J9VMSTATE_GC_CONCURRENT_SWEEP);
	((MM_ConcurrentSweepScheme *)_sweepScheme)->payAllocationTax(env, subspace, allocDescription);
	env->popVMstate(oldVMstate);
}

/**
 * Finish all concurrent sweep activities.
 *
 * @note Expects exclusive access to be held.
 * @note Expects to have parallel helper threads available.
 */
void
MM_ConcurrentGC::completeConcurrentSweep(MM_EnvironmentBase *env)
{
	/* If concurrent sweep is not enabled, do nothing */
	if(!_extensions->concurrentSweep) {
		return ;
	}

	MM_ConcurrentSweepScheme *concurrentSweep = (MM_ConcurrentSweepScheme *)_sweepScheme;

	/* If concurrent sweep was not on this cycle, or has already been completed, do nothing */
	if(!concurrentSweep->isConcurrentSweepActive()) {
		return ;
	}

	concurrentSweep->completeSweep(env, ABOUT_TO_GC);
}

/**
 * Finish all concurrent sweep activities.
 */
void
MM_ConcurrentGC::completeConcurrentSweepForKickoff(MM_EnvironmentBase *env)
{
	/* If concurrent sweep is not enabled, do nothing */
	if(!_extensions->concurrentSweep) {
		return ;
	}

	((MM_ConcurrentSweepScheme *)_sweepScheme)->completeSweepingConcurrently(env);
}
#endif /* OMR_GC_CONCURRENT_SWEEP */

/**
 *
 * Do some initialization work.
 * Mutator pays its allocation tax by initailizing sections of mark map and/or
 * card table until tax paid or no more sections to initailize.
 *
 * @param initToDo  The amount of initialization work, in bytes, this mutator has
 * been taxed to do.
 *
 * @return the amount of initialization work actually done
 */
uintptr_t
MM_ConcurrentGC::doConcurrentInitialization(MM_EnvironmentBase *env, uintptr_t initToDo)
{
	uintptr_t initDone = 0;
	void *from, *to;
	InitType type;
	bool concurrentCollectable;

	omrthread_monitor_enter(_initWorkMonitor);
	if (_stats.getExecutionMode() != CONCURRENT_INIT_RUNNING) {
		omrthread_monitor_exit(_initWorkMonitor);
		return initDone;
	}

	if (!allInitRangesProcessed()){ /* We just act as an general helper */
		_initializers += 1;
		if (!_initSetupDone ) {
			_markingScheme->getWorkPackets()->reset(env);
			_markingScheme->workerSetupForGC(env);
			((MM_ConcurrentCardTable *)_cardTable)->initializeCardCleaning(env);
			_initSetupDone = true;
		}

		omrthread_monitor_exit(_initWorkMonitor);

	} else {

		/* wait for active initializers to finish */
		omrthread_monitor_enter(_initWorkCompleteMonitor);
		omrthread_monitor_exit(_initWorkMonitor);
		omrthread_monitor_wait(_initWorkCompleteMonitor);
		omrthread_monitor_exit(_initWorkCompleteMonitor);

		/* ..and we are done */
		return 0;
	}

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

	omrthread_monitor_enter(_initWorkMonitor);
	_initializers--;

	/* Other threads still busy initailizing */
	if(_initializers > 0) {
		/* Did we run out of initialization work before paying all of our tax ? */
		if (initDone < initToDo && !env->isExclusiveAccessRequestWaiting()) {
			/* Yes..so block here and wait for master thread to finish and update execution mode
			 */
			omrthread_monitor_enter(_initWorkCompleteMonitor);
			omrthread_monitor_exit(_initWorkMonitor);
			omrthread_monitor_wait(_initWorkCompleteMonitor);
			omrthread_monitor_exit(_initWorkCompleteMonitor);
		} else {
			/* No..we have completely paid our tax or GC is waiting so return */
			omrthread_monitor_exit(_initWorkMonitor);
		}
	} else {
		/* We are last initializer so tidy up */
		if (allInitRangesProcessed()) {
			_concurrentDelegate.concurrentInitializationComplete(env);
			_stats.switchExecutionMode(CONCURRENT_INIT_RUNNING, CONCURRENT_INIT_COMPLETE);
		}

		if (allInitRangesProcessed() || env->isExclusiveAccessRequestWaiting()) {
			/* Wake up any helper threads */
			omrthread_monitor_enter(_initWorkCompleteMonitor);
			omrthread_monitor_notify_all(_initWorkCompleteMonitor);
			omrthread_monitor_exit(_initWorkCompleteMonitor);

			omrthread_monitor_exit(_initWorkMonitor);
		} else {
			omrthread_monitor_exit(_initWorkMonitor);
		}
	}

	return initDone;
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
MM_ConcurrentGC::doConcurrentTrace(MM_EnvironmentBase *env,
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
		remainingFree = potentialFreeSpace(env, allocDescription);
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

	/* Switch state if card cleaning stage 1 threshold reached */
	if( (CONCURRENT_TRACE_ONLY == _stats.getExecutionMode()) && (remainingFree < _stats.getCardCleaningThreshold())) {
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
		if (((MM_ConcurrentCardTable *)_cardTable)->isCardCleaningComplete() &&
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
		if ((_markingScheme->getWorkPackets()->inputPacketAvailable(env)) || (_cardTable->isCardCleaningStarted() && !_cardTable->isCardCleaningComplete())) {
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
 * Initiate final collection
 * Concurrent tracing has completed all tracing and card cleaning work so its
 * now time to request a global collection.
 *
 * @return TRUE if final collection actually requested; FALSE otherwise
 */
bool
MM_ConcurrentGC::concurrentFinalCollection(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	/* Switch to FINAL_COLLECTION; if we fail another thread beat us to it so just return */
	if	(_stats.switchExecutionMode(CONCURRENT_EXHAUSTED, CONCURRENT_FINAL_COLLECTION)) {

		if(env->acquireExclusiveVMAccessForGC(this, true, true)) {
			OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
			/* We got exclusive control first so do collection */
			reportConcurrentCollectionStart(env);
			uint64_t startTime = omrtime_hires_clock();
			garbageCollect(env, subSpace, NULL, J9MMCONSTANT_IMPLICIT_GC_DEFAULT, NULL, NULL, NULL);
			reportConcurrentCollectionEnd(env, omrtime_hires_clock() - startTime);

			env->releaseExclusiveVMAccessForGC();
		}

		return true;

	} else {
		return false;
	}
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
MM_ConcurrentGC::localMark(MM_EnvironmentBase *env, uintptr_t sizeToTrace)
{
	omrobjectptr_t objectPtr;
	uintptr_t gcCount = _extensions->globalGCStats.gcCount;

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
			sizeTraced += sizeof(fomrobject_t);
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
MM_ConcurrentGC::cleanCards(MM_EnvironmentBase *env, bool isMutator, uintptr_t sizeToDo, uintptr_t *sizeDone, bool threadAtSafePoint)
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
 * We have suferred a work stack overflow during the concurrent mark cycle.
 *
 */
void
MM_ConcurrentGC::concurrentWorkStackOverflow()
{
	_stats.setConcurrentWorkStackOverflowOcurred(true);
	_stats.incConcurrentWorkStackOverflowCount();
}

/**
 * Reset overflow inidcators for next concurrent cycle
 *
 */
void
MM_ConcurrentGC::clearConcurrentWorkStackOverflow()
{
	_stats.setConcurrentWorkStackOverflowOcurred(false);

#if defined(OMR_GC_MODRON_SCAVENGER)
	MM_WorkPacketsConcurrent *packets = (MM_WorkPacketsConcurrent *)_markingScheme->getWorkPackets();
	packets->resetWorkPacketsOverflow();
#endif /* OMR_GC_MODRON_SCAVENGER */
}

/**
 * By the time we are called all tracing activity should be stopped and any concurrent
 * helper threads should have parked themselves. Before the final collection can take
 * place we need to ensure that all objects in dirty cards are retraced and the remembered
 * set is processed.
 *
 * @see MM_ParallelGlobalGC::internalPreCollect()
 */
void
MM_ConcurrentGC::internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	Trc_MM_ConcurrentGC_internalPreCollect_Entry(env->getLanguageVMThread(), subSpace);

	/* Thread roots must have been flushed by this point */
	Assert_MM_true(!_concurrentDelegate.flushThreadRoots(env));

#if defined(OMR_GC_CONCURRENT_SWEEP)
	/**
	 * Finish off any sweep work still pending before the GC.  We do this to make sure the
	 * heap is walkable (the sweep could have connected part of an entry that spans many chunks,
	 * thus creating an unwalkable portion of the heap).
	 */
	completeConcurrentSweep(env);
#endif /* OMR_GC_CONCURRENT_SWEEP */

	/* Ensure caller acquired exclusive VM access before calling */
	Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());

	/* Set flag to show STW collector is active; some operations need to know if they
	 * are called during a global collect or not, eg heapAddRange
	 */
	_stwCollectionInProgress = true;

	/* Assume for now we will need to initialize the mark map. If we subsequenly find
	 * we got far enough through the concurrent mark cycle then we will reset this flag
	 */
	_initializeMarkMap = true;

	/* Remember the executionMode at the point when the GC was triggered */
	uintptr_t executionModeAtGC = _stats.getExecutionMode();
	_stats.setExecutionModeAtGC(executionModeAtGC);
	
	Assert_MM_true(NULL == env->_cycleState);

	if (executionModeAtGC == CONCURRENT_OFF) {
		/* Not in a concurrent cycle so do perform the super class precollect work */
		_concurrentCycleState = MM_CycleState();
		_concurrentCycleState._type = _cycleType;
		MM_ParallelGlobalGC::internalPreCollect(env, subSpace, allocDescription, gcCode);
	} else {
		/* Currently in a concurrent cycle */
		env->_cycleState = &_concurrentCycleState;
		env->_cycleState->_gcCode = MM_GCCode(gcCode);
		env->_cycleState->_activeSubSpace = subSpace;
		env->_cycleState->_collectionStatistics = &_collectionStatistics;
	}

	if (executionModeAtGC > CONCURRENT_OFF && _extensions->debugConcurrentMark) {
		_stats.printAllocationTaxReport(env->getOmrVM());
	}

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	updateMeteringHistoryBeforeGC(env);
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	/* Empty all the Packets if we are aborting concurrent mark.
	 * If we have had a RS overflow abort the concurrent collection
	 * regardless of how far we got to force a full STW mark.
	 */
	if (_extensions->isRememberedSetInOverflowState() || ((CONCURRENT_OFF < executionModeAtGC) && (CONCURRENT_TRACE_ONLY > executionModeAtGC))) {
		CollectionAbortReason reason = (_extensions->isRememberedSetInOverflowState() ? ABORT_COLLECTION_REMEMBERSET_OVERFLOW : ABORT_COLLECTION_INSUFFICENT_PROGRESS);
		abortCollection(env, reason);
		/* concurrent cycle was aborted so we need to kick off a new cycle and set up the cycle state */
		MM_ParallelGlobalGC::internalPreCollect(env, subSpace, allocDescription, gcCode);
	} else if (CONCURRENT_TRACE_ONLY <= executionModeAtGC) {
		/* We are going to complete the concurrent cycle to generate the GCStart/GCIncrement events */
		reportGCStart(env);
		reportGCIncrementStart(env);
		reportGlobalGCIncrementStart(env);

		/* Switch the executionMode to OFF to complete the STW collection */
		_stats.switchExecutionMode(executionModeAtGC, CONCURRENT_OFF);
#if defined(OMR_GC_MODRON_SCAVENGER)
		_extensions->setConcurrentGlobalGCInProgress(false);
#endif

		/* Mark map is usable. We got far enough into the concurrent to be
		 * sure we at least initialized the mark map so no need to do so again.
		 */
		_initializeMarkMap = false;

		if (CONCURRENT_FINAL_COLLECTION > executionModeAtGC) {
			reportConcurrentHalted(env);

			if (!_markingScheme->getWorkPackets()->tracingExhausted()) {

				reportConcurrentCompleteTracingStart(env);
				uint64_t startTime = omrtime_hires_clock();
				/* Get assistance from all slave threads to complete processing of any remaining work packets.
				 * In the event of work stack overflow we will just dirty cards which will get processed during
				 * final card cleaning.
				 */
				MM_ConcurrentCompleteTracingTask completeTracingTask(env, _dispatcher, this, env->_cycleState);
				_dispatcher->run(env, &completeTracingTask);

				reportConcurrentCompleteTracingEnd(env, omrtime_hires_clock() - startTime);
			}
		}

#if defined(OMR_GC_MODRON_SCAVENGER)
		if(_extensions->scavengerEnabled) {
			reportConcurrentRememberedSetScanStart(env);
			uint64_t startTime = omrtime_hires_clock();

			/* Do we need to clear mark bits for any NEW heaps ?
			 * If heap has been reconfigured since we kicked off
			 * concurrent we need to determine the new ranges of
			 * NEW bits which need clearing
			 */
			if (_rebuildInitWorkForAdd || _rebuildInitWorkForRemove) {
				determineInitWork(env);
			}
			resetInitRangesForSTW();

			/* Get assistance from all slave threads to reset all mark bits for any NEW areas of heap */
			MM_ConcurrentClearNewMarkBitsTask clearNewMarkBitsTask(env, _dispatcher, this);
			_dispatcher->run(env, &clearNewMarkBitsTask);

			/* If remembered set if not empty then re-scan any objects in the remembered set */
			if (!(_extensions->rememberedSet.isEmpty())) {
				MM_ConcurrentScanRememberedSetTask scanRememberedSetTask(env, _dispatcher, this, env->_cycleState);
				_dispatcher->run(env, &scanRememberedSetTask);
			}

			reportConcurrentRememberedSetScanEnd(env, omrtime_hires_clock() - startTime);
		}
#endif /* OMR_GC_MODRON_SCAVENGER */

		reportConcurrentFinalCardCleaningStart(env);
		uint64_t startTime = omrtime_hires_clock();

		bool overflow = false; /* assume the worst case*/
		uintptr_t overflowCount;

        do {
			/* remember count when we start */
			overflowCount = _stats.getConcurrentWorkStackOverflowCount();

			/* Get assistance from all slave threads to do final card cleaning */
			MM_ConcurrentFinalCleanCardsTask cleanCardsTask(env, _dispatcher, this, env->_cycleState);
			((MM_ConcurrentCardTable *)_cardTable)->initializeFinalCardCleaning(env);

			_dispatcher->run(env, &cleanCardsTask);

			/* Have we had a work stack overflow whilst processing card table ? */
			overflow = (overflowCount != _stats.getConcurrentWorkStackOverflowCount());
		} while (overflow);

        /* reset overflow flag */
    	_markingScheme->getWorkPackets()->clearOverflowFlag();

    	reportConcurrentFinalCardCleaningEnd(env, omrtime_hires_clock() - startTime);

		assume(_cardTable->isCardTableEmpty(env),"internalPreCollect: card cleaning has failed to clean all cards");

		/* Move any remaining deferred work packets to regular lists */
		_markingScheme->getWorkPackets()->reuseDeferredPackets(env);
	}

	switchConHelperRequest(CONCURRENT_HELPER_MARK, CONCURRENT_HELPER_WAIT);

	Trc_MM_ConcurrentGC_internalPreCollect_Exit(env->getLanguageVMThread(), subSpace);
}

/* (non-doxygen)
 * @see MM_ParallelGlobalGC::internalGarbageCollect()
 */
bool
MM_ConcurrentGC::internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription)
{
	_extensions->globalGCStats.gcCount += 1;

	masterThreadGarbageCollect(env, static_cast<MM_AllocateDescription*>(allocDescription), _initializeMarkMap, false);

	/* Restore normal allocation rules.
	 * It's not good to do it in concurrentFinalCollection(), as an AF may occur before we get chance to do final collection, so we may miss to restore it.
	 * It's too late to do it in postCollect(), as the AF will make us re-try allocate just after collection is done, but postCollect() is invoked after the re-try.
	 */
	env->_cycleState->_activeSubSpace->setAllocateAtSafePointOnly(env, false);

	return true;
}

/**
 * Post mark broadcast event.
 * The mark phase of a global collection has completed. The only thing we need do here
 * is to perform any concurrent RAS checks if they have been requested.
 */
void
MM_ConcurrentGC::postMark(MM_EnvironmentBase *envModron)
{
	/*
	 *  First call superclass postMark() routine
	 */
	MM_ParallelGlobalGC::postMark(envModron);
}

/**
 * A global collection has been completed. All we need do here is request a retune
 * of concurrent adaptive parameters.
 * @see MM_ParallelGlobalGC::internalPostCollect()
 */
void
MM_ConcurrentGC::internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	Trc_MM_ConcurrentGC_internalPostCollect_Entry(env->getLanguageVMThread(), subSpace);

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	updateMeteringHistoryAfterGC(env);
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	if (_extensions->debugConcurrentMark) {
		_stats.clearAllocationTaxCounts();
	}

	 /* Reset concurrent work stack overflow flags for next cycle */
	clearConcurrentWorkStackOverflow();

	/* If we flushed all the TLH's correctly in GC the TLH mark bits should be
	 * all OFF
	 */
	assume(_cardTable->isTLHMarkBitsEmpty(env),"TLH mark map not empty");

	/* Re tune for next concurrent cycle if we have had a heap resize or we got far enough
	 * last time. We only re-tune on a system GC in the event of a heap resize.
	 */
	if (_retuneAfterHeapResize || _stats.getExecutionModeAtGC() > CONCURRENT_OFF) {
		tuneToHeap(env);
	}

	/* Collection is complete so reset flags */
	_stwCollectionInProgress = false;
	_forcedKickoff  = false;
	_stats.clearKickoffReason();

	if (_extensions->optimizeConcurrentWB) {
		/* Reset vmThread flag so mutators don't dirty cards until next concurrent KO */
		if (_stats.getExecutionModeAtGC() > CONCURRENT_INIT_RUNNING) {
			_concurrentDelegate.signalThreadsToStopDirtyingCards(env);
		}

		/* Cancel any outstanding call backs */
		_callback->cancelCallback(env);
	}

	/* Call the super class to do any required work */
	MM_ParallelGlobalGC::internalPostCollect(env, subSpace);

	Trc_MM_ConcurrentGC_internalPostCollect_Exit(env->getLanguageVMThread(), subSpace);
}

/**
 * Abort any concurrent mark activity
 * If a concurrent mark cycle is in progress abort it. All work packets
 * are reset to empty and concurrent execution mode is reset to CONCURRNET_OFF.
 * @warning Any caller of this routine is assumed to have exclusive VM access. This
 * implies that all concurrent tracers will have stopped including any concurrent
 * helper threads which should be parked.
 */
void
MM_ConcurrentGC::abortCollection(MM_EnvironmentBase *env, CollectionAbortReason reason)
{
	/* Allow the superclass to do its work */
	MM_ParallelGlobalGC::abortCollection(env, reason);

	/* If concurrent is OFF nothing to do so get out now */
	if ( CONCURRENT_OFF == _stats.getExecutionMode()) {
		Assert_MM_true(_markingScheme->getWorkPackets()->isAllPacketsEmpty());
		return;
	}

	MM_CycleState *oldCycleState = env->_cycleState;
	env->_cycleState = &_concurrentCycleState;
	reportConcurrentAborted(env, reason);
	reportGCCycleEnd(env);
	env->_cycleState = oldCycleState;

	/* Since all of the current mark data is being flushed make sure to flush the reference
	 * lists so they can be properly rebuilt by the next mark phase.
	 */
	_concurrentDelegate.abortCollection(env);

	/* Clear contents of all work packets */
	_markingScheme->getWorkPackets()->resetAllPackets(env);

	/* Unconditionally change execution mode back to OFF. On a subsequent allocation
	 * we will then restart a concurrent mark cycle. We probably won't finish before
	 * we get an AF but we may as well do as much as possible.
	 *
	 * As caller should have exclusive VM access before making this call
	 * the following switch of execution mode should always succeed ....
	 */
	switchConHelperRequest(CONCURRENT_HELPER_MARK, CONCURRENT_HELPER_WAIT);

	_stats.switchExecutionMode(_stats.getExecutionMode(), CONCURRENT_OFF );
#if defined(OMR_GC_MODRON_SCAVENGER)
	_extensions->setConcurrentGlobalGCInProgress(false);
#endif

	/* make sure to reset the init ranges before the next kickOff */
	resetInitRangesForConcurrentKO();

	/* ...but just in case check it does */
	Assert_GC_true_with_message(env, CONCURRENT_OFF == _stats.getExecutionMode(), "MM_ConcurrentStats::_executionMode = %zu\n", _stats.getExecutionMode());
}

/**
 * Prepare heap for safe walk
 *
 * This routine prepares the heap for a parallel walk by performing
 * a marking phase - concurrent marking performed so far is not used.
 */
void
MM_ConcurrentGC::prepareHeapForWalk(MM_EnvironmentBase *envModron)
{
	abortCollection(envModron, ABORT_COLLECTION_PREPARE_HEAP_FOR_WALK);

	/* call the superclass method to do the actual mark phase */
	MM_ParallelGlobalGC::prepareHeapForWalk(envModron);
}

/*
 * Adjust Concurrent GC releated structures after heap expand.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @return true if operation completes with success
 *
 * @see MM_ParallelGlobalGC::heapAddRange()
 */
bool
MM_ConcurrentGC::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool clearCards = false;

	Trc_MM_ConcurrentGC_heapAddRange_Entry(env->getLanguageVMThread(), subspace, size, lowAddress, highAddress);

	_rebuildInitWorkForAdd = true;
	if (subspace->isConcurrentCollectable()) {
		_retuneAfterHeapResize = true;
	}

	/* Expand any superclass structures including mark bits*/
	bool result = MM_ParallelGlobalGC::heapAddRange(env, subspace, size, lowAddress, highAddress);

	if (result) {
		/* If we are within a concurrent cycle we need to initialize the mark bits
		 * for new region of heap now
		 */
		if (CONCURRENT_OFF < _stats.getExecutionMode()) {
			/* If subspace is concurrently collectible then clear bits otherwise
			 * set the bits on to stop tracing INTO this area during concurrent
			 * mark cycle.
			 */
			if (subspace->isConcurrentCollectable()) {
				_markingScheme->setMarkBitsInRange(env, lowAddress, highAddress, true);
				clearCards = true;
			} else {
				_markingScheme->setMarkBitsInRange(env, lowAddress, highAddress, false);
			}
		}

		/* ...and then expand the card table */
		result = ((MM_ConcurrentCardTable *)_cardTable)->heapAddRange(env, subspace, size, lowAddress, highAddress, clearCards);
		if (!result) {
			/* Expansion of Concurrent Card Table has failed
			 * ParallelGlobalGC expansion must be reversed
			 */
			MM_ParallelGlobalGC::heapRemoveRange(env, subspace, size, lowAddress, highAddress, NULL, NULL);
		}
	}

	_heapAlloc = _extensions->heap->getHeapTop();

	Trc_MM_ConcurrentGC_heapAddRange_Exit(env->getLanguageVMThread());

	return result;
}

/* Adjust Concurrent GC releated structures after heap contraction.
 *
 *
 * @param size The amount of memory removed from the heap
 * @param lowAddress The base address of the memory removed from the heap
 * @param highAddress The top address (non-inclusive) of the memory removed from the heap
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 * @return true if operation completes with success
 *
 * @see MM_ParallelGlobalGC::heapRemoveRange()
 */
bool
MM_ConcurrentGC::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	Trc_MM_ConcurrentGC_heapRemoveRange_Entry(env->getLanguageVMThread(), subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

	_rebuildInitWorkForRemove = true;
	if (subspace->isConcurrentCollectable()) {
		_retuneAfterHeapResize = true;
	}

	/* Contract any superclass structures */
	bool result = MM_ParallelGlobalGC::heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

	/* ...and then contract the card table */
	result = result && ((MM_ConcurrentCardTable *)_cardTable)->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
	_heapAlloc = (void *)_extensions->heap->getHeapTop();

	Trc_MM_ConcurrentGC_heapRemoveRange_Exit(env->getLanguageVMThread());

	return result;
}

/* (non-doxygen)
 * @see MM_ParallelGlobalGC::heapReconfigured()
 */
void
MM_ConcurrentGC::heapReconfigured(MM_EnvironmentBase *env)
{
	/* If called outside a global collection for a heap expand/contract..
	 */
	if (!_stwCollectionInProgress && (_rebuildInitWorkForAdd || _rebuildInitWorkForRemove)) {
		/* ... and a concurrent cycle has not yet started then we
		 *  tune to heap here to reflect new heap size
		 *  Note: CMVC 153167 : Under gencon, there is a timing hole where
		 *  if we are in the middle of initializing the heap ranges while a
		 *  scavenge occurs, and if the scavenge causes the heap to contract,
		 *  we will try to memset ranges that are now contracted (decommitted memory)
		 *  when we resume the init work.
		 */
		if (_stats.getExecutionMode() < CONCURRENT_INIT_COMPLETE) {
			tuneToHeap(env);
		} else {
			/* Heap expand/contract is during a concurrent cycle..we need to adjust the trace target so
			 * that the trace rate is adjusted correctly on  subsequent allocates.
			 */
			adjustTraceTarget();
		}
	}

	/* Expand any superclass structures */
	MM_ParallelGlobalGC::heapReconfigured(env);

	/* ...and then expand the card table */
	((MM_ConcurrentCardTable *)_cardTable)->heapReconfigured(env);
}

/**
 * Clear mark bits for any nursery heaps.
 *
 * This routine is called by ConcurrentClearNewMarkBits task on master and any slave
 * threads during internalPreCollect().
 */
void
MM_ConcurrentGC::clearNewMarkBits(MM_EnvironmentBase *env)
{
	void *from, *to;
	InitType type;
	bool concurrentCollectable;

	while(getInitRange(env, &from, &to, &type, &concurrentCollectable)) {
		/* If resetInitRangesForSTW has done it job we should only find mark bits
		 * for subspaces which are not concurrent collectible, e.g ALLOCATE
		 * and SURVIVOR subspaces
		 */
		assume0(MARK_BITS == type && !concurrentCollectable);
		/* Set all its mark bits to 0x00 */
		_markingScheme->setMarkBitsInRange(env,from,to,true);
	}
}

/*
 * Complete processing of any work packets.
 * If a concurrent cycle gets halted then some work packets may not get processed.
 * Process them now so the time to do so is not incorrectly reported against
 * stages of collect that follow.
 *
 */
void
MM_ConcurrentGC::completeTracing(MM_EnvironmentBase *env)
{
	omrobjectptr_t objectPtr;
	uintptr_t bytesTraced = 0;
	env->_workStack.reset(env, _markingScheme->getWorkPackets());

	while(NULL != (objectPtr = (omrobjectptr_t)env->_workStack.popNoWait(env))) {
		bytesTraced += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_PACKET);
	}
	env->_workStack.clearPushCount();

	/* ..and amount traced */
	_stats.incCompleteTracingCount(bytesTraced);

	flushLocalBuffers(env);
}


/**
 * Perform final card cleaning.
 * This routine is called to perform the final scan on the card table to clean
 * any cards not cleaned during the concurrent phase.
 */
void
MM_ConcurrentGC::finalCleanCards(MM_EnvironmentBase *env)
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

#if defined(OMR_GC_MODRON_SCAVENGER)
/**
 * Scan remembered set looking for any MARKED objects which are not in dirty cards.
 * A marked object which is not in a dirty card needs rescanning now for any references
 * to the nursery which will not have been traced by concurrent mark
 *
 * This routine is called ConcurrentScanRememberedSetTask on master and any slave threads
 * during internalPreCollect().
 */
void
MM_ConcurrentGC::scanRememberedSet(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	MM_SublistPuddle *puddle;
 	omrobjectptr_t *slotPtr, objectPtr;
 	uintptr_t RSObjects = 0;
 	uintptr_t bytesTraced = 0;
 	uintptr_t maxPushes = _markingScheme->getWorkPackets()->getSlotsInPacket() / 2;
	/* Get a fresh work stack */
	env->_workStack.reset(env, _markingScheme->getWorkPackets());
	env->_workStack.clearPushCount();

	GC_SublistIterator rememberedSetIterator(&_extensions->rememberedSet);
	while((puddle = rememberedSetIterator.nextList()) != NULL) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			GC_SublistSlotIterator rememberedSetSlotIterator(puddle);
			while((slotPtr = (omrobjectptr_t*)rememberedSetSlotIterator.nextSlot()) != NULL) {
				objectPtr = *slotPtr;
				/* For all objects in remembered set that have been marked scan the object
				 * unless its card is dirty in which case we leave it for later processing
				 * by finalCleanCards()
				 */
				if((objectPtr >= _heapBase)
					&& (objectPtr <  _heapAlloc)
					&& _markingScheme->isMarkedOutline(objectPtr)
					&& !_cardTable->isObjectInDirtyCardNoCheck(env,objectPtr)) {
						RSObjects += 1;
						if (_extensions->dirtCardDuringRSScan) {
							_cardTable->dirtyCard(env, objectPtr);
						} else {
							/* VMDESIGN 2048 -- due to barrier elision optimizations, the JIT may not have dirtied
							 * cards for some objects in the remembered set. Therefore we may discover references
							 * to both nursery and tenure objects while scanning remembered objects.
							 */

							bytesTraced += _markingScheme->scanObject(env,objectPtr, SCAN_REASON_REMEMBERED_SET_SCAN);

							/* Have we pushed enough new references? */
							if(env->_workStack.getPushCount() >= maxPushes) {
								/* To reduce the chances of mark stack overflow, we do some marking
								 * of what we have just pushed.
								 *
								 * WARNING. If we HALTED concurrent then we will process any remaining
								 * workpackets at this point. This will make RS processing appear more
								 * expensive than it really is.
								 */
								while(NULL != (objectPtr = (omrobjectptr_t)env->_workStack.popNoWait(env))) {
									bytesTraced += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_PACKET);
								}
								env->_workStack.clearPushCount();
							}
						}
				}
			}
		}
	}

	env->_workStack.clearPushCount();
	/* sort of abusing addToWorkStallTime to record the point when thread is finished with RS Scan Work
	 * Since popNoWait is used, we never stalled before this point. All stall time will be from this point till RS scan end event */
	env->_workPacketStats.addToWorkStallTime(0, omrtime_hires_clock());

	/* Flush this threads reference object buffer, work stack, returning any packets to appropriate lists */
	flushLocalBuffers(env);

	/* Add RS objects found to global count */
	_stats.incRSObjectsFound(RSObjects);

	/* ..and amount traced */
	_stats.incRSScanTraceCount(bytesTraced);
}

/**
 * Process event from an external GC (Scavenger) when old-to-old reference is created
 * from an existing old-to-new or new-to-new reference.
 * We need to rescan the parent object, as slots which during original scan contained
 * new area references could now contain references into old area.
 * @param objectPtr  Parent old object that has a reference to a child old object
 */
void
MM_ConcurrentGC::oldToOldReferenceCreated(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
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
MM_ConcurrentGC::recordCardCleanPass2Start(MM_EnvironmentBase *env)
{
	_pass2Started = true;

	/* Record how mush work we did before pass 2 KO */
	_totalTracedAtPass2KO = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();
	_totalCleanedAtPass2KO = _stats.getCardCleanCount() + _stats.getConHelperCardCleanCount();

	/* ..and boost tracing rate from here to end of cycle so we complete pass 2 ASAP */
	_allocToTraceRate *= _allocToTraceRateCardCleanPass2Boost;
}

/**
 * Perform any collector initialization particular to the concurrent collector.
 * @see MM_ParallelGlobalGC::collectorStartup()
 */
bool
MM_ConcurrentGC::collectorStartup(MM_GCExtensionsBase* extensions)
{
	bool result = MM_ParallelGlobalGC::collectorStartup(extensions);

	if (result) {
		result = initializeConcurrentHelpers(extensions);
	}

	return result;
}

/**
 * Perform any collector shutdown particular to the concurrent collector.
 * Currently this just involves stopping the concurrent background helper threads.
 * @see MM_ParallelGlobalGC::collectorShutdown()
 */
void
MM_ConcurrentGC::collectorShutdown(MM_GCExtensionsBase *extensions)
{
	shutdownConHelperThreads(extensions);
	MM_ParallelGlobalGC::collectorShutdown(extensions);
}

void
MM_ConcurrentGC::flushLocalBuffers(MM_EnvironmentBase *env)
{
	/* flush local reference object buffer */
	_concurrentDelegate.flushThreadRoots(env);

	/* Return any work packets to appropriate lists */
	env->_workStack.flush(env);

	/* set up local workstack for more marking */
	env->_workStack.reset(env, _markingScheme->getWorkPackets());
}

#if defined(OMR_GC_LARGE_OBJECT_AREA)
/**
 * Update concurrent metering history before a GC
 * Determine remaining free space in SOA and LOA before a collect
 * @see MM_ParallelGlobalGC::collectorShutdown()
 */
void
MM_ConcurrentGC::updateMeteringHistoryBeforeGC(MM_EnvironmentBase *env)
{
	/* If LOA disabled or a system GC then nothing to do */
	if (!_extensions->largeObjectArea || env->_cycleState->_gcCode.isExplicitGC()) {
		return;
	}

	if (MM_GCExtensionsBase::METER_DYNAMIC == _extensions->concurrentMetering) {
		uintptr_t oldFreeBeforeGC, soaFreeBeforeGC, loaFreeBeforeGC;
		assume0(NULL != _meteringHistory);

		oldFreeBeforeGC = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
		loaFreeBeforeGC = _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD);

		assume0(oldFreeBeforeGC >= loaFreeBeforeGC);

		soaFreeBeforeGC = oldFreeBeforeGC - loaFreeBeforeGC;

		_meteringHistory[_currentMeteringHistory].soaFreeBeforeGC = soaFreeBeforeGC;
		_meteringHistory[_currentMeteringHistory].loaFreeBeforeGC = loaFreeBeforeGC;
	}
}

/**
 * Update concurrent metering history after a GC
 * Determine remaining free space in SOA and LOA after a collection and then
 * determine using current history of previous collection cycles whether we need
 * to modify metering type
 * @see MM_ParallelGlobalGC::collectorShutdown()
 */
void
MM_ConcurrentGC::updateMeteringHistoryAfterGC(MM_EnvironmentBase *env)
{
	/* If LOA disabled or a system GC then nothing to do */
	if (!_extensions->largeObjectArea || env->_cycleState->_gcCode.isExplicitGC()) {
		return;
	}

	if (MM_GCExtensionsBase::METER_DYNAMIC	== _extensions->concurrentMetering) {
		uintptr_t oldFreeAfterGC, soaFreeAfterGC, loaFreeAfterGC;
		uintptr_t loaSize, soaVotes, loaVotes;
		float soaExhaustion, loaExhaustion;

		assume0(_meteringHistory != NULL);

		oldFreeAfterGC = _extensions->heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
		loaFreeAfterGC = _extensions->heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD);
		loaSize = _extensions->heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD);

		assume0(oldFreeAfterGC >= loaFreeAfterGC);

		soaFreeAfterGC = oldFreeAfterGC - loaFreeAfterGC;

		_meteringHistory[_currentMeteringHistory].soaFreeAfterGC = soaFreeAfterGC;
		_meteringHistory[_currentMeteringHistory].loaFreeAfterGC = loaFreeAfterGC;

		/* Decide which area (SOA or LOA) to meter on next cycle. If either area was
		 * completely exhausted at start of THIS collection then that one wins.
		 * Othewise we use history of previous collectiosn to decide which one to meter
		 */
		if (0 == _meteringHistory[_currentMeteringHistory].soaFreeBeforeGC) {
			_meteringHistory[_currentMeteringHistory].vote = VOTE_SOA;
			_meteringType = SOA;
		} else if (loaSize > 0 && 0 == _meteringHistory[_currentMeteringHistory].loaFreeBeforeGC) {
			_meteringHistory[_currentMeteringHistory].vote = VOTE_LOA;
			_meteringType = LOA;
		} else {
			/*
			 * Using an history of this and previous collections decide which area
			 * is most likely to exhaust first on next cycle and hence which area
			 * we shodul meter.
			 *
			 * First for this GC decide which area is exhausting the quickest.
			 */
			soaExhaustion = (float) soaFreeAfterGC / (float)(_meteringHistory[_currentMeteringHistory].soaFreeBeforeGC);
			loaExhaustion = (float) loaFreeAfterGC / (float)(_meteringHistory[_currentMeteringHistory].soaFreeBeforeGC);

			/* ..and add vote into history */
			_meteringHistory[_currentMeteringHistory].vote = soaExhaustion >= loaExhaustion ? VOTE_SOA : VOTE_LOA;

			/* Count the votes */
			soaVotes = 0;
			loaVotes = 0;
			for (uint32_t i = 0; i < _meteringHistorySize; i++) {

				switch(_meteringHistory[i].vote) {
				case VOTE_SOA:
					soaVotes += 1;
					break;
				case VOTE_LOA:
					loaVotes += 1;
					break;
				default:
					assume0(VOTE_UNDEFINED == _meteringHistory[i].vote);
					/* Cant have had enough collects to build hitory up*/
				}
			}

			/* Decide on metering method for next cycle */
			if (soaVotes > (_meteringHistorySize / 2)) {
				_meteringType = SOA;
			} else if (loaVotes > (_meteringHistorySize / 2)) {
				_meteringType = LOA;
			}
		}

		/* Decide which history to replace on next collection */
		_currentMeteringHistory = ((_currentMeteringHistory + 1) == _meteringHistorySize) ? 0 : _currentMeteringHistory + 1;
	}
}
#endif /* OMR_GC_LARGE_OBJECT_AREA */
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
