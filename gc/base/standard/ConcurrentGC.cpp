/*******************************************************************************
 * Copyright (c) 1991, 2022 IBM Corp. and others
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
#include "ConcurrentGC.hpp"
#include "ConcurrentCompleteTracingTask.hpp"
#include "ConcurrentClearNewMarkBitsTask.hpp"
#include "ConcurrentSafepointCallback.hpp"
#include "ConcurrentScanRememberedSetTask.hpp"
#if defined(OMR_GC_CONCURRENT_SWEEP)
#include "ConcurrentSweepScheme.hpp"
#endif /* OMR_GC_CONCURRENT_SWEEP */
#include "Configuration.hpp"
#include "CycleState.hpp"
#include "Debug.hpp"
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
#include "ParallelDispatcher.hpp"
#include "SpinLimiter.hpp"
#include "SublistIterator.hpp"
#include "SublistPuddle.hpp"
#include "SublistSlotIterator.hpp"
#include "WorkPacketsConcurrent.hpp"

#if defined(OMR_GC_REALTIME)
#include "RememberedSetSATB.hpp"
#endif /* defined(OMR_GC_REALTIME) */

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
	omrsig_protect(con_helper_thread_proc2, info, signalHandler, signalHandlerArg, (OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION), &rc);

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
	conHelperThreadInfo->threadFlags = (NULL != omrThread) ? CON_HELPER_INFO_FLAG_OK : CON_HELPER_INFO_FLAG_FAIL;
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

void objectTenured(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	((MM_ConcurrentGC *)env->getExtensions()->getGlobalCollector())->objectTenured(env, objectPtr);
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

void
MM_ConcurrentGC::preConcurrentInitializeStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	_concurrentPhaseStats._cycleID = _concurrentCycleState._verboseContextID;
	_concurrentPhaseStats._startTime = omrtime_hires_clock();

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_PHASE_START(
			_extensions->privateHookInterface,
			env->getOmrVMThread(),
			omrtime_hires_clock(),
			J9HOOK_MM_PRIVATE_CONCURRENT_PHASE_START,
			&_concurrentPhaseStats);
}

void
MM_ConcurrentGC::postConcurrentUpdateStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats, UDATA bytesConcurrentlyScanned)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	_concurrentPhaseStats._cycleID = _concurrentCycleState._verboseContextID;
	_concurrentPhaseStats._collectionStats = &_stats;

	TRIGGER_J9HOOK_MM_PRIVATE_CONCURRENT_PHASE_END(
		_extensions->privateHookInterface,
		env->getOmrVMThread(),
		omrtime_hires_clock(),
		J9HOOK_MM_PRIVATE_CONCURRENT_PHASE_END,
		&_concurrentPhaseStats);
}

/**
 * Aysnc callback routine to signal all threads that CM is active (Activate SATB Barrier or Start Dirtying Cards).
 *
 * @note Caller assumed to be at a safe point
 *
 */
void
MM_ConcurrentGC::signalThreadsToActivateWriteBarrierAsyncEventHandler(OMR_VMThread *omrVMThread, void *userData)
{
	MM_ConcurrentGC *collector  = (MM_ConcurrentGC *)userData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

	collector->_concurrentDelegate.acquireExclusiveVMAccessAndSignalThreadsToActivateWriteBarrier(env);
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
 * Instantiate concurrent RAS objects(if required) and initialize all  monitors required by concurrent.
 * Allocate and initialize the concurrent helper thread table.
 *
 * @return TRUE if initialization completed OK; FALSE otherwise
 */
bool
MM_ConcurrentGC::initialize(MM_EnvironmentBase *env)
{
	/* First call super class initialize */
	if (!MM_ParallelGlobalGC::initialize(env)) {
		goto error_no_memory;
	}

	if (!_concurrentDelegate.initialize(env, this)) {
		goto error_no_memory;
	}

	if (_extensions->optimizeConcurrentWB) {
		_callback = _concurrentDelegate.createSafepointCallback(env);
		if (NULL == _callback) {
			goto error_no_memory;
		}
		_callback->registerCallback(env, signalThreadsToActivateWriteBarrierAsyncEventHandler, this);
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
	if (omrthread_monitor_init_with_name(&_conHelpersActivationMonitor, 0, "MM_ConcurrentGC::conHelpersActivation")) {
		goto error_no_memory;
	}

	/* Initialize the initialisation work monitor */
	if (omrthread_monitor_init_with_name(&_initWorkMonitor, 0, "MM_ConcurrentGC::initWork")) {
		goto error_no_memory;
	}

	/* Initialize the tuning monitor monitor */
	if (omrthread_monitor_init_with_name(&_concurrentTuningMonitor, 0, "MM_ConcurrentGC::concurrentTuning")) {
		goto error_no_memory;
	}

	/* ..and the initialization work complete monitor */
	if (omrthread_monitor_init_with_name(&_initWorkCompleteMonitor, 0, "MM_ConcurrentGC::initWorkComplete")) {
		goto error_no_memory;
	}

	_allocToInitRate = _extensions->concurrentLevel * CONCURRENT_INIT_BOOST_FACTOR;
	_allocToTraceRate = _extensions->concurrentLevel;
	_allocToTraceRateNormal = _extensions->concurrentLevel;
	_allocToTraceRateMinFactor = ((float)1) / interpolateInRange(MIN_ALLOC_2_TRACE_RATE_1, MIN_ALLOC_2_TRACE_RATE_8, MIN_ALLOC_2_TRACE_RATE_10, _allocToTraceRateNormal);
	_allocToTraceRateMaxFactor = interpolateInRange(MAX_ALLOC_2_TRACE_RATE_1, MAX_ALLOC_2_TRACE_RATE_8, MAX_ALLOC_2_TRACE_RATE_10, _allocToTraceRateNormal);

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	/* Has user elected to run with an LOA ? */
	if (_extensions->largeObjectArea) {
		if (MM_GCExtensionsBase::METER_DYNAMIC == _extensions->concurrentMetering) {

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
		} else if (MM_GCExtensionsBase::METER_BY_LOA == _extensions->concurrentMetering) {
			_meteringType = LOA;
		}
	}
#endif /* OMR_GC_LARGE_OBJECT_AREA) */

	return true;

error_no_memory:
	return false;
}

/**
 * Teardown a ConcurrentGC object
 * Destroy referenced objects and release
 * all allocated storage before ConcurrentGC object is freed.
 */
void
MM_ConcurrentGC::tearDown(MM_EnvironmentBase *env)
{
	OMR::GC::Forge *forge = env->getForge();

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

/**
 * Determine concurrent initialization work
 * Populate the _initRanges structure with the areas of storage which need
 * initializing at each concurrent kickoff. Currently mark bits (and card table for Incremental CM).
 * For mark bits we create one entry in _initRanges for each segment of heap.
 * The mark bits for areas which are concurrently collectable, eg OLD,  will be
 * set OFF; but all mark bits areas not concurrent collectable, eg NEW, will be
 * set ON so that concurrent does not trace into these areas.
 */
void
MM_ConcurrentGC::determineInitWork(MM_EnvironmentBase *env)
{
	bool initDone = false;

	Trc_MM_ConcurrentGC_determineInitWork_Entry(env->getLanguageVMThread());
	
	while (!initDone) {
		uint32_t i = 0;
		_numInitRanges = 0;

		/* Add init ranges for all old and new area segments */
		MM_HeapRegionDescriptor *region = NULL;
		MM_Heap *heap = _extensions->heap;
		MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
		GC_HeapRegionIterator regionIterator(regionManager);

		while (NULL != (region = regionIterator.nextRegion())) {
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

			_numInitRanges += numberOfInitRanages(subspace);
		}

		/* Any room left ? */
		if (_numInitRanges > _numPhysicalInitRanges) {
			/* We need to get a bigger initRanges array of i+1 elements but first free the one if have already, if any */
			if (_initRanges) {
				env->getForge()->free(_initRanges);
			}

			/* TODO: dynamically allocating this structure.  Should the VM tear itself down in this scenario? */
			_initRanges = (InitWorkItem *) env->getForge()->allocate(sizeof(InitWorkItem) * _numInitRanges, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
			if (NULL == _initRanges) {
				initDone = true;
				_numPhysicalInitRanges = 0;
				_numInitRanges = 0;
			} else {
				_numPhysicalInitRanges = _numInitRanges;
			}
		} else {
			determineInitWorkInternal(env, i);

			_nextInitRange = 0;
			initDone = true;
		}
	}

	/* Now count total initialization work we have to do */
	uintptr_t initWork = 0;
	for (uint32_t i = 0; i < _numInitRanges; i++) {
		if (NULL != _initRanges[i].base) {
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
	for (uint32_t i = 0; i < _numInitRanges; i++) {
		_initRanges[i].current = _initRanges[i].base;
	}

	/* Reset scan ptr */
	_nextInitRange = 0;
}

void
MM_ConcurrentGC::resetConcurrentParameters(MM_EnvironmentBase *env)
{
	/* Reset all ConcurrentStats for next cycle */
	_stats.reset();

	_initSetupDone = false;
	_alloc2ConHelperTraceRate = 0;
	_lastConHelperTraceSizeCount = 0;
	_lastAverageAlloc2TraceRate = 0;
	_maxAverageAlloc2TraceRate = 0;
	_lastFreeSize = LAST_FREE_SIZE_NEEDS_INITIALIZING;
	_lastTotalTraced = 0;
}

/**
 * Reset initialization work ranges for STW collect.
 * Reset any non concurrently collectbale heap ranges, eg new heap ranges,
 * in _initRanges structure so we can reset their associated mark bits OFF
 * prior to a full collection. We don't want to trace into such objects during
 * the concurrent mark cycle but we must do so during a full collection.
 */
void
MM_ConcurrentGC::resetInitRangesForSTW()
{
	for (uint32_t i = 0; i < _numInitRanges; i++) {
		if ((MARK_BITS  == _initRanges[i].type) && (!((_initRanges[i].subspace)->isConcurrentCollectable()))) {
			_initRanges[i].current = _initRanges[i].base;
		} else {
			/* Set current == top so we don't bother doing any re-init of these areas */
			_initRanges[i].current = _initRanges[i].top;
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
MM_ConcurrentGC::getInitRange(MM_EnvironmentBase *env, void **from, void **to, InitType *type, bool *concurrentCollectable)
{
	/* Cache _numInitRanges as it may be changed by another thread */
	uint32_t i = (uint32_t)_nextInitRange;

	void *localFrom = NULL;
    void *localTo = NULL;

    Trc_MM_ConcurrentGC_getInitRange_Entry(env->getLanguageVMThread());

	while (i < _numInitRanges) {
		/* Cache ptr to next range */
		localFrom = (void *)(_initRanges[i].current);
		if (localFrom < _initRanges[i].top) {
			uintptr_t chunkSize = _initRanges[i].chunkSize;

			if (((uintptr_t)((uint8_t *)_initRanges[i].top - (uint8_t *)localFrom)) <= chunkSize) {
				localTo = _initRanges[i].top;
			} else {
				localTo = (void *)((uint8_t *)localFrom + chunkSize);

				/* Finish off range if remaining part is less than half a chunk. */
				if (localTo >= (void *)((uint8_t *)_initRanges[i].top - (chunkSize/2))) {
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
			uint32_t oldIndex = i;
			uint32_t newIndex = oldIndex + 1;
			/* Don't care if this fails, it just means another thread someone beat us to it */
			MM_AtomicOperations::lockCompareExchangeU32(&_nextInitRange, oldIndex, newIndex);
			/* ..and refresh local cached value, may have been changed by other threads */
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
	/* Always return false for now until we understand how to fix this code to ensure
	 * we do not prematurely report that the rate has dropped and so start card cleaning
	 * too early.
	 */
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

MM_ConcurrentGC::ConHelperRequest
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

MM_ConcurrentGC::ConHelperRequest
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
MM_ConcurrentGC::conHelperEntryPoint(OMR_VMThread *omrThread, uintptr_t workerID)
{
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrThread);
	ConHelperRequest request = CONCURRENT_HELPER_WAIT;
	uintptr_t sizeTraced = 0;
	uintptr_t totalScanned = 0;
	uintptr_t sizeToTrace = 0;
	MM_SpinLimiter spinLimiter(env);

	/* Thread not a mutator so identify its type */
	env->initializeGCThread();
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

		Assert_GC_true_with_message(env, (CONCURRENT_OFF != _stats.getExecutionMode()), "MM_ConcurrentStats::_executionMode = %zu\n", _stats.getExecutionMode());

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
			if (sizeTraced > 0) {
				_stats.incConHelperTraceSizeCount(sizeTraced);
				totalScanned += sizeTraced;
				spinLimiter.reset();
			}
			request = getConHelperRequest(env);
		}

		/* Collector specific Con Helper Work */
		conHelperDoWorkInternal(env, &request, &spinLimiter, &totalScanned);

		if (CONCURRENT_HELPER_MARK == request) {
			request = switchConHelperRequest(CONCURRENT_HELPER_MARK, CONCURRENT_HELPER_WAIT);
		}

		Assert_MM_true(CONCURRENT_HELPER_MARK != request);

		reportConcurrentBackgroundThreadFinished(env, totalScanned);
		env->releaseVMAccess();
	} /* end while (SHUTDOWN != request) */

	/*  Must be no local work left at this point! */
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

	/* The last thread to shut down must notify the main thread */
	if (_conHelpersShutdownCount == _conHelpersStarted) {
		omrthread_monitor_notify(_conHelpersActivationMonitor);
	}

	/* Clear the entry in _conHelpersTable containing the thread id,
	 * so that other parts of code know now this thread is gone.
	 * The thread itself does not know which entry in the table it is,
	 * so we search the table. */
	for (uint32_t i = 0; i < _conHelpersStarted; i++) {
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
 * minimum priority so they do not compete with application threads for use of processors;
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

	uint32_t conHelperThreadCount = 0;
	ConHelperThreadInfo conHelperThreadInfo;

	/* Attach the concurrent helper thread threads */
	conHelperThreadInfo.omrVM = extensions->getOmrVM();

	omrthread_monitor_enter(_conHelpersActivationMonitor);
	_conHelpersRequest = CONCURRENT_HELPER_WAIT;

	for(conHelperThreadCount = 0; conHelperThreadCount < _conHelperThreads;	conHelperThreadCount++) {
		conHelperThreadInfo.threadFlags = 0;
		conHelperThreadInfo.threadID = conHelperThreadCount;
		conHelperThreadInfo.collector = this;

		IDATA threadForkResult = createThreadWithCategory(&(_conHelpersTable[conHelperThreadCount]), OMR_OS_STACK_SIZE, J9THREAD_PRIORITY_MIN,
															0, con_helper_thread_proc, (void *)&conHelperThreadInfo, J9THREAD_CATEGORY_SYSTEM_GC_THREAD);

		if (threadForkResult != 0) {
			break;
		}

		do {
			omrthread_monitor_wait(_conHelpersActivationMonitor);
		} while (!conHelperThreadInfo.threadFlags);

		if (CON_HELPER_INFO_FLAG_OK != conHelperThreadInfo.threadFlags) {
			break;
		}
	}
	omrthread_monitor_exit(_conHelpersActivationMonitor);
	_conHelpersStarted = conHelperThreadCount;

	return ((_conHelpersStarted == _conHelperThreads) ? true : false);
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
 * Update tuning statistics at end of a concurrent cycle.
 *
 * Calculate the proportion of live objects in heap and the proportion of non-leaf
 * objects in the heap at the end of this collection. These factors are used
 * if the heap changes in size to predict how much of new heap will need to be
 * traced before next collection.
 *
 */
void
MM_ConcurrentGC::updateTuningStatistics(MM_EnvironmentBase *env)
{
	/* Don't update statistics if its a system GC or we aborted a concurrent collection cycle */
    if (env->_cycleState->_gcCode.isExplicitGC() || (CONCURRENT_TRACE_ONLY > _stats.getExecutionModeAtGC())) {
        return;
    } else {
		/* Get current heap size and free bytes */
		MM_Heap *heap = _extensions->heap;
		uintptr_t heapSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);
		uintptr_t freeSize = heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);
		uintptr_t totalLiveObjects = heapSize - freeSize;
		uintptr_t bytesTraced = 0;
		float newLiveObjectFactor = 0.0;
		float newNonLeafObjectFactor = 0.0;

		newLiveObjectFactor = ((float)totalLiveObjects) / ((float)heapSize);
		_tenureLiveObjectFactor = MM_Math::weightedAverage(_tenureLiveObjectFactor, newLiveObjectFactor, LIVE_PART_HISTORY_WEIGHT);

		/* Work out the new _tenureNonLeafObjectFactor based on
		 * how much we actually traced and the amount of live bytes
		 * found by collector
		 */
		bytesTraced = _stats.getTraceSizeCount() + _stats.getConHelperTraceSizeCount();

		/* If we did not complete mark add in those we missed. This will be an over estimate but will get us back on track */
		if (CONCURRENT_EXHAUSTED > _stats.getExecutionModeAtGC()) {
			bytesTraced += _stats.getFinalTraceCount();
		}

		newNonLeafObjectFactor = ((float)bytesTraced) / ((float)totalLiveObjects);
		_tenureNonLeafObjectFactor = MM_Math::weightedAverage(_tenureNonLeafObjectFactor, newNonLeafObjectFactor, NON_LEAF_HISTORY_WEIGHT);

		updateTuningStatisticsInternal(env);
	}
}

/**
 * Calculate allocation tax during initialization phase
 * Calculate how much initialization work should be done by a mutator for the current
 * allocation request. We don't need to worry about work being available at the right time
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
 * Calculate how much data should be traced by a mutator for the current allocation request.
 *
 * @return the allocation tax
 */
uintptr_t
MM_ConcurrentGC::calculateTraceSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	float thisTraceRate;
	uintptr_t sizeToTrace = 0;
	uintptr_t remainingFree = 0;

	uintptr_t allocationSize = allocDescription->getAllocationTaxSize();

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* Allocation tax for nursery allocation is based on potential nursery
	 * free space as for every N bytes of nursery we allocate we only
	 * tenure a proportion of it. Tenure area allocations are always taxed
	 * based on current amount of "taxable" free space left.
	 */
	if (allocDescription->isNurseryAllocation()) {
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
	 * that all work is assigned to mutators by the time free space drops to _kickoffThresholdBuffer.
	 */
	remainingFree = (remainingFree > _kickoffThresholdBuffer) ? (remainingFree - _kickoffThresholdBuffer) : 0;

	/* Calculate the size to trace for this alloc based on the
	 * trace target and how much work has been already completed.
	 */
	uintptr_t workCompleteSoFar = workCompleted();

	/* Calculate how much work we need to get through */
	uintptr_t traceTarget = getTraceTarget();

	/* Provided we are not into buffer zone already and we have not already done more work
	 * than we predicted calculate required trace rate for this allocate request to keep us on
	 * track.
	 */
	if ((remainingFree > 0) && (workCompleteSoFar < traceTarget)) {

		thisTraceRate = (float)((traceTarget - workCompleteSoFar) / (float)(remainingFree));

		if (thisTraceRate > _allocToTraceRate) {
	    /* The "over tracing" should not only adjust to the current ratio between
	     * free space and estimated remaining tracing, but also try to do even more tracing, in
	     * order to correct the ratio back to the required alloc to trace rate.
	     */
	    	thisTraceRate += ((thisTraceRate - _allocToTraceRate) * OVER_TRACING_BOOST_FACTOR);
			/* Make sure its not now greater than max */
			if (thisTraceRate > getAllocToTraceRateMax()) {
				thisTraceRate = getAllocToTraceRateMax();
			}
		} else if (thisTraceRate < getAllocToTraceRateMin()) {
			thisTraceRate = getAllocToTraceRateMin();
		}

		if (_forcedKickoff) {
			/* in case of external kickoff use at least default trace rate */
			if (thisTraceRate < getAllocToTraceRateNormal()) {
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

	if (sizeToTrace > _maxTraceSize) {
		sizeToTrace = _maxTraceSize;
	}

	return sizeToTrace;
}

/**
 * Determine if its time to do periodical tuning.
 * Has the free space reduced by the _tuningUpdateInterval from the last time
 * we did periodical tuning, or do we need to initialize for a new concurrent
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

	if ((LAST_FREE_SIZE_NEEDS_INITIALIZING == _lastFreeSize)  ||
        ((_lastFreeSize > freeSize) && (_lastFreeSize - freeSize > _tuningUpdateInterval))) {
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
uintptr_t
MM_ConcurrentGC::potentialFreeSpace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	MM_MemorySpace *memorySpace = env->getExtensions()->heap->getDefaultMemorySpace();
	MM_MemorySubSpace *oldSubspace = memorySpace->getTenureMemorySubSpace();
	MM_MemorySubSpace *newSubspace = memorySpace->getDefaultMemorySubSpace();
	MM_ScavengerStats *scavengerStats = &_extensions->scavengerStats;

	uintptr_t headRoom = 0;
	uintptr_t currentOldFree = 0;

	/* Have we done at least 1 scavenge ? If not no statistics available so return high values */
	if (!scavengerStats->isAvailable(env)) {
		return (uintptr_t)-1;
	}

	uintptr_t nurseryPromotion = (scavengerStats->_avgTenureBytes == 0) ? 1 : (uintptr_t)(scavengerStats->_avgTenureBytes + (env->getExtensions()->tenureBytesDeviationBoost * scavengerStats->_avgTenureBytesDeviation));

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	/* Do we need to tax this allocation ? */
	if (LOA == _meteringType) {
		nurseryPromotion = scavengerStats->_avgTenureLOABytes == 0 ? 1 : scavengerStats->_avgTenureLOABytes;
		currentOldFree = oldSubspace->getApproximateActiveFreeLOAMemorySize();
		headRoom = (uintptr_t)(_extensions->concurrentKickoffTenuringHeadroom * _extensions->lastGlobalGCFreeBytesLOA);
	} else {
		assume0(SOA == _meteringType);
		currentOldFree = oldSubspace->getApproximateActiveFreeMemorySize() - oldSubspace->getApproximateActiveFreeLOAMemorySize();
		headRoom = (uintptr_t)(_extensions->concurrentKickoffTenuringHeadroom * (_extensions->getLastGlobalGCFreeBytes() - _extensions->lastGlobalGCFreeBytesLOA));
	}
#else
	currentOldFree = oldSubspace->getApproximateActiveFreeMemorySize();
	headRoom = (uintptr_t)(_extensions->concurrentKickoffTenuringHeadroom * _extensions->getLastGlobalGCFreeBytes());
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

	uintptr_t nurseryInitialFree = scavengerStats->_avgInitialFree;
	uintptr_t currentNurseryFree =  newSubspace->getApproximateFreeMemorySize();

	/* Calculate the number of scavenge's before we will tenure enough objects to
	 * fill the old space. If we know next scavenge will percolate then we have no scavenges
	 * remaining before concurren KO is required
	 */
	uintptr_t scavengesRemaining = 0;
	if (scavengerStats->_nextScavengeWillPercolate) {
		_stats.setKickoffReason(NEXT_SCAVENGE_WILL_PERCOLATE);
		_languageKickoffReason = NO_LANGUAGE_KICKOFF_REASON;
	} else {
		scavengesRemaining = (uintptr_t)(currentOldFree/nurseryPromotion);
	}

	/* The headroom remaining will be the maximum value between 1 , and the current
	 * headroom calculation. We want a worst case headroom, so that we avoid percolate.
	 * By having a higher estimate of headroom, we will have less scavenges remaining,
	 *  and will start concurrent mark earlier
	 */

	float scavengesRemainingHeadroom = ((float)headRoom)/nurseryPromotion;
	scavengesRemainingHeadroom = OMR_MAX(scavengesRemainingHeadroom, 1);
	scavengesRemaining = MM_Math::saturatingSubtract(scavengesRemaining, (uintptr_t)scavengesRemainingHeadroom);

	/* Now calculate how many bytes we can therefore allocate in the nursery before
	 * we will fill the tenure area. On 32 bit platforms this can be greater than 4G
	 * hence the 64 bit maths. We assume that the heap will not be big enough on 64 bit
	 * platforms to overflow 64 bits!
	 */

	uint64_t potentialFree = (uint64_t)currentNurseryFree + ((uint64_t)nurseryInitialFree * (uint64_t)scavengesRemaining);

#if !defined(OMR_ENV_DATA64)
	/* On a 32 bit platforms the amount of free space could be more than 4G. Therefore
	 * if the amount of potential free space is greater than can be expressed in 32
	 * bits we just return 4G.
	 */
	uint64_t maxFree = 0xFFFFFFFF;
	if (potentialFree > maxFree) {
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
	/* Single thread this code; ensure update for earlier interval completes
	 * we update for a later interval
	 */
	omrthread_monitor_enter(_concurrentTuningMonitor);

	if (LAST_FREE_SIZE_NEEDS_INITIALIZING == _lastFreeSize) {
        _lastFreeSize = freeSize;
        _tuningUpdateInterval = (uintptr_t)((float)freeSize  * TUNING_HEAP_SIZE_FACTOR);

        if (_tuningUpdateInterval > _maxTraceSize) {
            _tuningUpdateInterval = _maxTraceSize;
        }

        if (_tuningUpdateInterval < _minTraceSize) {
            _tuningUpdateInterval = _minTraceSize;
        }

    } else if ((_lastFreeSize > freeSize) && (_lastFreeSize - freeSize) >= _tuningUpdateInterval) {
        /* This thread first to update for this interval so calculate total traced so far */
		uintptr_t totalTraced = getMutatorTotalTraced();
		uintptr_t freeSpaceUsed = _lastFreeSize - freeSize;

		/* Update concurrent helper trace rate if we have any */
		if (_conHelpersStarted > 0) {
			uintptr_t conTraced = getConHelperTotalTraced();
			float newConHelperRate = ((float)(conTraced - _lastConHelperTraceSizeCount)) / ((float)(freeSpaceUsed));
			_lastConHelperTraceSizeCount = conTraced;
			_alloc2ConHelperTraceRate = MM_Math::weightedAverage(_alloc2ConHelperTraceRate, newConHelperRate, CONCURRENT_HELPER_HISTORY_WEIGHT);

			totalTraced += conTraced;
		}

		_lastAverageAlloc2TraceRate = ((float)(totalTraced - _lastTotalTraced)) / ((float)freeSpaceUsed);
		_lastTotalTraced = totalTraced;

		if (_maxAverageAlloc2TraceRate < _lastAverageAlloc2TraceRate) {
			_maxAverageAlloc2TraceRate =  _lastAverageAlloc2TraceRate;
		}

		/* Set for next interval */
		_lastFreeSize = freeSize;
	}

	omrthread_monitor_exit(_concurrentTuningMonitor);
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
	if (_extensions->concurrentSweep) {
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
	assume0(allocDescription->getAllocationTaxSize() > 0);
	/* Thread roots must have been flushed by this point */
	Assert_MM_true(!_concurrentDelegate.flushThreadRoots(env));

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	/* Do we need to tax this allocation ? */
	if ((LOA == _meteringType && !allocDescription->isLOAAllocation()) || (SOA == _meteringType && allocDescription->isLOAAllocation())) {
		/* no tax to pay so return */
		return;
	}
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	/* Check if its time to KO */
	if (CONCURRENT_OFF == _stats.getExecutionMode()) {
		if (!timeToKickoffConcurrent(env, allocDescription)) {
#if defined(OMR_GC_CONCURRENT_SWEEP)
			/* Try to do some concurrent sweeping */
			if (_extensions->concurrentSweep) {
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
	uintptr_t oldVMstate = env->pushVMstate(OMRVMSTATE_GC_CONCURRENT_MARK_TRACE);

	/* Get required information from Alloc description */
	uintptr_t allocationSize = allocDescription->getAllocationTaxSize();
	bool threadAtSafePoint = allocDescription->isThreadAtSafePoint();

	/* Sanity check on allocation size and .. */
	assume0(allocationSize > 0);
	/* .. we must not mark anything if WB not yet active */
#if 0	/* TODO 90354: Find a way to reestablish this assertion */
	Assert_MM_true((_stats.getExecutionMode() < CONCURRENT_ROOT_TRACING1) || (((J9VMThread *)env->getLanguageVMThread())->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE));
#endif

	/* Boost priority of thread whilst we are paying our tax; don't want low priority application threads holding on to work packets */
	uintptr_t priority = env->getPriority();
	if (J9THREAD_PRIORITY_NORMAL > priority) {
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
		if (env->isExclusiveAccessRequestWaiting()) {
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
				sizeToTrace = calculateInitSize(env, allocationSize);
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
					if (threadAtSafePoint) {
						_concurrentDelegate.acquireExclusiveVMAccessAndSignalThreadsToActivateWriteBarrier(env);
					} else {
						/* Register for this thread to get called back at safe point */
						_callback->requestCallback(env);
						/* ..and return so that thread can get to a safe point */
						taxPaid = true;
					}
				} else {
					Assert_MM_true(_extensions->configuration->isIncrementalUpdateBarrierEnabled());
					/* TODO: Once optimizeConcurrentWB enabled by default this code will be deleted */
					_stats.switchExecutionMode(CONCURRENT_INIT_COMPLETE, CONCURRENT_ROOT_TRACING);
				}
				break;

			case CONCURRENT_EXHAUSTED:
				/* If another thread has set exhausted whilst we were allocating then we can trigger
				 * the final collection here provided we are at a safe point. If not then GC will be
				 * triggered by next allocation.
				 */
				if (threadAtSafePoint) {
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
				Assert_MM_true(_extensions->configuration->isIncrementalUpdateBarrierEnabled());
				nextExecutionMode = _concurrentDelegate.getNextTracingMode(CONCURRENT_ROOT_TRACING);
				Assert_GC_true_with_message(env, (CONCURRENT_ROOT_TRACING < nextExecutionMode) || (CONCURRENT_TRACE_ONLY == nextExecutionMode), "MM_ConcurrentMarkingDelegate::getNextTracingMode(CONCURRENT_ROOT_TRACING) = %zu\n", nextExecutionMode);
				if(_stats.switchExecutionMode(CONCURRENT_ROOT_TRACING, nextExecutionMode)) {
					/* Signal threads for async callback to scan stack*/
					_concurrentDelegate.signalThreadsToTraceStacks(env);
					taxPaid = true;
				}
				break;

			default:
				Assert_MM_true(_extensions->configuration->isIncrementalUpdateBarrierEnabled());
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
	if (J9THREAD_PRIORITY_NORMAL > priority) {
		env->setPriority(priority);
	}

	env->popVMstate(oldVMstate);
}

void
MM_ConcurrentGC::acquireExclusiveVMAccessAndSignalThreadsToActivateWriteBarrier(MM_EnvironmentBase *env)
{
	uintptr_t gcCount = _extensions->globalGCStats.gcCount;

	/* Things may have moved on since async callback requested */
	while (CONCURRENT_INIT_COMPLETE == _stats.getExecutionMode()) {
		/* We may or may not have exclusive access but another thread may have beat us to it and
		 * prepared the threads or even collected.
		 */
		if (acquireExclusiveVMAccessForCycleStart(env)) {
			MM_CycleState *previousCycleState = env->_cycleState;
			_concurrentCycleState = MM_CycleState();
			_concurrentCycleState._type = _cycleType;
			env->_cycleState = &_concurrentCycleState;
			reportGCCycleStart(env);
			env->_cycleState = previousCycleState;

			_concurrentPhaseStats.clear();
			preConcurrentInitializeStatsAndReport(env);

			setupForConcurrent(env);

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
	uintptr_t remainingFree = 0;

	/* If -Xgc:noConcurrentMarkKO specified then we never kickoff concurrent mark */
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

		if (_stats.switchExecutionMode(CONCURRENT_OFF, CONCURRENT_INIT_RUNNING)) {
			_stats.setRemainingFree(remainingFree);
			/* Set kickoff reason if it is not set yet */
			_stats.setKickoffReason(KICKOFF_THRESHOLD_REACHED);
			if (LANGUAGE_DEFINED_REASON != _stats.getKickoffReason()) {
				_languageKickoffReason = NO_LANGUAGE_KICKOFF_REASON;
			}
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
	uintptr_t oldVMstate = env->pushVMstate(OMRVMSTATE_GC_CONCURRENT_SWEEP);
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
	if (!_extensions->concurrentSweep) {
		return ;
	}

	MM_ConcurrentSweepScheme *concurrentSweep = (MM_ConcurrentSweepScheme *)_sweepScheme;

	/* If concurrent sweep was not on this cycle, or has already been completed, do nothing */
	if (!concurrentSweep->isConcurrentSweepActive()) {
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
	if (!_extensions->concurrentSweep) {
		return ;
	}

	((MM_ConcurrentSweepScheme *)_sweepScheme)->completeSweepingConcurrently(env);
}
#endif /* OMR_GC_CONCURRENT_SWEEP */

/**
 *
 * Do some initialization work.
 * Mutator pays its allocation tax by initializing sections of mark map and (/or
 * card table for Incremental CM) until tax paid or no more sections to initialize.
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

	omrthread_monitor_enter(_initWorkMonitor);
	if (CONCURRENT_INIT_RUNNING != _stats.getExecutionMode()) {
		omrthread_monitor_exit(_initWorkMonitor);
		return initDone;
	}

	if (!allInitRangesProcessed()) { /* We just act as an general helper */
		_initializers += 1;
		if (!_initSetupDone ) {
			_markingScheme->getWorkPackets()->reset(env);
			_markingScheme->workerSetupForGC(env);
			initalizeConcurrentStructures(env);
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

	initDone += doConcurrentInitializationInternal(env, initToDo);

	omrthread_monitor_enter(_initWorkMonitor);
	_initializers--;

	/* Other threads still busy initializing */
	if (_initializers > 0) {
		/* Did we run out of initialization work before paying all of our tax ? */
		if ((initDone < initToDo) && !env->isExclusiveAccessRequestWaiting()) {
			/* Yes..so block here and wait for main thread to finish and update execution mode */
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

uintptr_t
MM_ConcurrentGC::doConcurrentInitializationInternal(MM_EnvironmentBase *env, uintptr_t initToDo)
{
	InitType type;
	void *from = NULL;
	void *to = NULL;
	bool concurrentCollectable = false;

	uintptr_t initDone = 0;

	while (initToDo > initDone) {

		/* If another thread is waiting for exclusive VM access, possibly
		 * to collect, get out quick so as not to hold things up
		 */
		if (env->isExclusiveAccessRequestWaiting()) {
			break;
		}

		if (getInitRange(env, &from, &to, &type, &concurrentCollectable)) {
			Assert_MM_true(MARK_BITS == type);

			if (concurrentCollectable) {
				initDone += _markingScheme->setMarkBitsInRange(env,from,to,true);
			} else {
				initDone += _markingScheme->setMarkBitsInRange(env,from,to,false);
			}
		} else {
			/* All init ranges taken so exit and wait below for init to complete before returning to caller */
			break;
		}
	}

	return initDone;
}

/**
 * Initiate final collection
 * Concurrent tracing has completed all concurrent work so its
 * now time to request a global collection.
 *
 * @return TRUE if final collection actually requested; FALSE otherwise
 */
bool
MM_ConcurrentGC::concurrentFinalCollection(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace)
{
	/* Switch to FINAL_COLLECTION; if we fail another thread beat us to it so just return */
	if (_stats.switchExecutionMode(CONCURRENT_EXHAUSTED, CONCURRENT_FINAL_COLLECTION)) {
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
		_concurrentPhaseStats._endTime = omrtime_hires_clock();
		postConcurrentUpdateStatsAndReport(env);

		if (acquireExclusiveVMAccessForCycleEnd(env)) {
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
 * We have suffered a work stack overflow during the concurrent mark cycle.
 *
 */
void
MM_ConcurrentGC::workStackOverflow()
{
	_stats.setConcurrentWorkStackOverflowOcurred(true);
	_stats.incConcurrentWorkStackOverflowCount();
}

/**
 * Reset overflow indicators for next concurrent cycle
 *
 */
void
MM_ConcurrentGC::clearWorkStackOverflow()
{
	_stats.setConcurrentWorkStackOverflowOcurred(false);
}

/**
 * By the time we are called all tracing activity should be stopped and any concurrent
 * helper threads should have parked themselves. Before the final collection can take
 * place we need to ensure that all objects in dirty cards are retraced (only for an incremental CM config) and the remembered
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
	Assert_MM_true(_stwCollectionInProgress);

	/* Assume for now we will need to initialize the mark map. If we subsequently find
	 * we got far enough through the concurrent mark cycle then we will reset this flag
	 */
	_initializeMarkMap = true;

	/* Remember the executionMode at the point when the GC was triggered */
	uintptr_t executionModeAtGC = _stats.getExecutionMode();
	_stats.setExecutionModeAtGC(executionModeAtGC);
	
	Assert_MM_true(NULL == env->_cycleState);

	if (CONCURRENT_OFF == executionModeAtGC) {
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

		/* Report concurrent-end now, concurrent-start was reported but we didn't get
		 * the opportunity to report concurrent-end (due to GC idle collection or concurrent halted)
		 */
		if ((CONCURRENT_INIT_COMPLETE < executionModeAtGC) && (CONCURRENT_FINAL_COLLECTION > executionModeAtGC)) {
			postConcurrentUpdateStatsAndReport(env);
		}
	}

	if ((executionModeAtGC > CONCURRENT_OFF) && _extensions->debugConcurrentMark) {
		_stats.printAllocationTaxReport(env->getOmrVM());
	}

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	updateMeteringHistoryBeforeGC(env);
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	/* Empty all the Packets if we are aborting concurrent mark.
	 * If we have had a RS overflow or an explicit gc (resulting for idleness) then 
	 * abort the concurrent collection regardless of how far we got to force a full STW mark.
	 */

	if ((J9MMCONSTANT_EXPLICIT_GC_PREPARE_FOR_CHECKPOINT == gcCode) && (CONCURRENT_OFF < executionModeAtGC)) {
			abortCollection(env, ABORT_COLLECTION_PREPARE_FOR_CHECKPOINT_GC);
			MM_ParallelGlobalGC::internalPreCollect(env, subSpace, allocDescription, gcCode);
	} else
#if defined(OMR_GC_IDLE_HEAP_MANAGER)
	if ((J9MMCONSTANT_EXPLICIT_GC_IDLE_GC == gcCode) && (CONCURRENT_OFF < executionModeAtGC)) {
		abortCollection(env, ABORT_COLLECTION_IDLE_GC);
		MM_ParallelGlobalGC::internalPreCollect(env, subSpace, allocDescription, gcCode);
	} else
#endif /* OMR_GC_IDLE_HEAP_MANAGER */
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

		completeConcurrentTracing(env, executionModeAtGC);

#if defined(OMR_GC_MODRON_SCAVENGER)
		if (_extensions->scavengerEnabled) {
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

			/* Get assistance from all worker threads to reset all mark bits for any NEW areas of heap */
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

		/* Incremental must do final card cleaning */
		finalConcurrentPrecollect(env);

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

	mainThreadGarbageCollect(env, static_cast<MM_AllocateDescription*>(allocDescription), _initializeMarkMap, false);

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
	/* First call superclass postMark() routine */
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
	clearWorkStackOverflow();

	/* Re tune for next concurrent cycle if we have had a heap resize or we got far enough
	 * last time. We only re-tune on a system GC in the event of a heap resize.
	 */
	if (_retuneAfterHeapResize || (CONCURRENT_OFF < _stats.getExecutionModeAtGC())) {
		tuneToHeap(env);
	}

	/* Collection is complete so reset flags */
	_forcedKickoff  = false;
	_stats.clearKickoffReason();

	if (_extensions->optimizeConcurrentWB) {
		/* Reset vmThread CM Active flags/barriers */
		if (CONCURRENT_INIT_RUNNING < _stats.getExecutionModeAtGC()) {
			_concurrentDelegate.signalThreadsToDeactivateWriteBarrier(env);
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
	if (CONCURRENT_OFF == _stats.getExecutionMode()) {
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

	_stats.switchExecutionMode(_stats.getExecutionMode(), CONCURRENT_OFF);
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
 * Adjust Concurrent GC related structures after heap expand.
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
	Trc_MM_ConcurrentGC_heapAddRange_Entry(env->getLanguageVMThread(), subspace, size, lowAddress, highAddress);

	_rebuildInitWorkForAdd = true;
	if (subspace->isConcurrentCollectable()) {
		_retuneAfterHeapResize = true;
	}

	/* Expand any superclass structures including mark bits*/
	bool result = MM_ParallelGlobalGC::heapAddRange(env, subspace, size, lowAddress, highAddress);

	_heapAlloc = _extensions->heap->getHeapTop();

	Trc_MM_ConcurrentGC_heapAddRange_Exit(env->getLanguageVMThread());

	return result;
}

/* Adjust Concurrent GC related structures after heap contraction.
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

	result = result && contractInternalConcurrentStructures(env, subspace, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

	_heapAlloc = (void *)_extensions->heap->getHeapTop();

	Trc_MM_ConcurrentGC_heapRemoveRange_Exit(env->getLanguageVMThread());

	return result;
}

/* (non-doxygen)
 * @see MM_ParallelGlobalGC::heapReconfigured()
 */
void
MM_ConcurrentGC::heapReconfigured(MM_EnvironmentBase *env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress)
{
	/* _rebuildInitWorkForAdd/_rebuildInitWorkForRemove are not sufficient for determining on how to proceed with headReconfigured
	 * We end up here in the following scenarios:
	 *  1) During heap expand - after heapAddRange
	 *  2) During heap contact - after heapRemoveRange
	 *  3) After Scavenger Tilt (no resize)
	 *
	 *  It is necessary that _rebuildInitWorkForAdd is set when we're here during an expand (after heapAddRange), or
	 *  _rebuildInitWorkForRemove in the case of contract. However, it is not a sufficient check to ensure the reason we're
	 *  here. For instance, when Concurrent is on, _rebuildInitWorkForAdd will be set but not cleared.
	 *  As a result, we can have multiple calls of expands interleaved with contracts, resulting in both flags being set.
	 *  Similarly, we can end up here after scavenger tilt with any of the flags set.
	 */

	Assert_MM_true(HEAP_RECONFIG_NONE != reason);

	/* If called outside a global collection for a heap expand/contract.. */
	if ((HEAP_RECONFIG_CONTRACT == reason) || (HEAP_RECONFIG_EXPAND == reason)) {
		Assert_MM_true(_rebuildInitWorkForAdd || _rebuildInitWorkForRemove);
		if (!_stwCollectionInProgress) {
			/* ... and a concurrent cycle has not yet started then we
			 *  tune to heap here to reflect new heap size
			 *  Note: CMVC 153167 : Under gencon, there is a timing hole where
			 *  if we are in the middle of initializing the heap ranges while a
			 *  scavenge occurs, and if the scavenge causes the heap to contract,
			 *  we will try to memset ranges that are now contracted (decommitted memory)
			 *  when we resume the init work.
			 */
			if (CONCURRENT_INIT_COMPLETE > _stats.getExecutionMode()) {
				tuneToHeap(env);
			} else {
				/* Heap expand/contract is during a concurrent cycle..we need to adjust the trace target so
				 * that the trace rate is adjusted correctly on  subsequent allocates.
				 */
				adjustTraceTarget();
			}
		}
	}

	if ((NULL != lowAddress) && (NULL != highAddress)) {
		Assert_MM_true(HEAP_RECONFIG_EXPAND == reason);
		/* If we are within a concurrent cycle we need to initialize the mark bits
		 * for new region of heap now
		 */
		if (CONCURRENT_OFF < _stats.getExecutionMode()) {
			/* If subspace is concurrently collectible then clear bits otherwise
			 * set the bits on to stop tracing INTO this area during concurrent
			 * mark cycle.
			 */
			_markingScheme->setMarkBitsInRange(env, lowAddress, highAddress, subspace->isConcurrentCollectable());
		}
	}

	/* Expand any superclass structures */
	MM_ParallelGlobalGC::heapReconfigured(env, reason, subspace, lowAddress, highAddress);
}

void
MM_ConcurrentGC::recalculateInitWork(MM_EnvironmentBase *env)
{
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
			 * otherwise skip it (rebuild flag will remain set, and the table will be reconfigured
			 * at a later, safe point (like beginning of STW phase).
			 * Even if happens to be 0 initializers, but we are still doing the init, skip it.
			 * It is safe for heap expansion (caller of this method already did initialization
			 * for that  part of the memory), and more importantly it will not trigger unnecessary
			 * redo of the init work, if we were to reset the able in a middle of init phase. */
			if ((0 == _initializers) && (CONCURRENT_INIT_RUNNING != _stats.getExecutionMode())) {
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
}

/**
 * Clear mark bits for any nursery heaps.
 *
 * This routine is called by ConcurrentClearNewMarkBits task on main and any worker
 * threads during internalPreCollect().
 */
void
MM_ConcurrentGC::clearNewMarkBits(MM_EnvironmentBase *env)
{
	InitType type;
	void *from = NULL;
	void *to = NULL;
	bool concurrentCollectable = false;

	while (getInitRange(env, &from, &to, &type, &concurrentCollectable)) {
		/* If resetInitRangesForSTW has done it job we should only find mark bits
		 * for subspaces which are not concurrent collectible, e.g ALLOCATE
		 * and SURVIVOR subspaces
		 */
		assume0((MARK_BITS == type) && !concurrentCollectable);
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
	omrobjectptr_t objectPtr = NULL;
	uintptr_t bytesTraced = 0;
	env->_workStack.reset(env, _markingScheme->getWorkPackets());

	while (NULL != (objectPtr = (omrobjectptr_t)env->_workStack.popNoWait(env))) {
		bytesTraced += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_PACKET);
	}
	env->_workStack.clearPushCount();

	/* ..and amount traced */
	_stats.incCompleteTracingCount(bytesTraced);

	flushLocalBuffers(env);
}

#if defined(OMR_GC_MODRON_SCAVENGER)
/**
 * Scan remembered set looking for any MARKED objects (which are not in dirty cards for Incremental CM).
 * A marked object which is not in a dirty card needs rescanning now for any references
 * to the nursery which will not have been traced by concurrent mark
 *
 * This routine is called ConcurrentScanRememberedSetTask on main and any worker threads
 * during internalPreCollect().
 */
void
MM_ConcurrentGC::scanRememberedSet(MM_EnvironmentBase *env)
{
	MM_SublistPuddle *puddle = NULL;
	omrobjectptr_t *slotPtr = NULL;
	omrobjectptr_t objectPtr = NULL;
 	uintptr_t maxPushes = _markingScheme->getWorkPackets()->getSlotsInPacket() / 2;
	/* Get a fresh work stack */
	env->_workStack.reset(env, _markingScheme->getWorkPackets());
	env->_workStack.clearPushCount();
	env->_markStats.clear();

	GC_SublistIterator rememberedSetIterator(&_extensions->rememberedSet);
	while (NULL != (puddle = rememberedSetIterator.nextList())) {
		if (J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			GC_SublistSlotIterator rememberedSetSlotIterator(puddle);
			while (NULL != (slotPtr = (omrobjectptr_t*)rememberedSetSlotIterator.nextSlot())) {
				objectPtr = *slotPtr;
				/* For all objects in remembered set that have been marked scan the object
				 * unless its card is dirty in which case we leave it for later processing
				 * by finalCleanCards()
				 */
				if ((objectPtr >= _heapBase)
					&& (objectPtr <  _heapAlloc)
					&& _markingScheme->isMarkedOutline(objectPtr)
					&& !canSkipObjectRSScan(env, objectPtr)) {
						/* VMDESIGN 2048 -- due to barrier elision optimizations, the JIT may not have dirtied
						 * cards for some objects in the remembered set. Therefore we may discover references
						 * to both nursery and tenure objects while scanning remembered objects.
						 */
						_markingScheme->scanObject(env,objectPtr, SCAN_REASON_REMEMBERED_SET_SCAN);

						/* Have we pushed enough new references? */
						if (env->_workStack.getPushCount() >= maxPushes) {
							/* To reduce the chances of mark stack overflow, we do some marking
							 * of what we have just pushed.
							 *
							 * WARNING. If we HALTED concurrent then we will process any remaining
							 * workpackets at this point. This will make RS processing appear more
							 * expensive than it really is.
							 */
							while (NULL != (objectPtr = (omrobjectptr_t)env->_workStack.popNoWait(env))) {
								_markingScheme->scanObject(env, objectPtr, SCAN_REASON_PACKET);
							}
							env->_workStack.clearPushCount();
						}
				}
			}
		}
	}

	env->_workStack.clearPushCount();

	/* Call completeScan to allow for improved RS scanning parallelism */
	_markingScheme->completeScan(env);

	/* Flush this threads reference object buffer, work stack, returning any packets to appropriate lists */
	flushLocalBuffers(env);

	/* Add RS objects found to global count */
	_stats.incRSObjectsFound(env->_markStats._objectsScanned);

	/* ..and amount traced */
	_stats.incRSScanTraceCount(env->_markStats._bytesScanned);
}
#endif /* OMR_GC_MODRON_SCAVENGER */

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
		 * Otherwise we use history of previous collection to decide which one to meter
		 */
		if (0 == _meteringHistory[_currentMeteringHistory].soaFreeBeforeGC) {
			_meteringHistory[_currentMeteringHistory].vote = VOTE_SOA;
			_meteringType = SOA;
		} else if ((loaSize > 0) && (0 == _meteringHistory[_currentMeteringHistory].loaFreeBeforeGC)) {
			_meteringHistory[_currentMeteringHistory].vote = VOTE_LOA;
			_meteringType = LOA;
		} else {
			/*
			 * Using an history of this and previous collections decide which area
			 * is most likely to exhaust first on next cycle and hence which area
			 * we should meter.
			 *
			 * First for this GC decide which area is exhausting the quickest.
			 */
			soaExhaustion = ((float)soaFreeAfterGC) / ((float)(_meteringHistory[_currentMeteringHistory].soaFreeBeforeGC));
			loaExhaustion = ((float)loaFreeAfterGC) / ((float)(_meteringHistory[_currentMeteringHistory].soaFreeBeforeGC));

			/* ..and add vote into history */
			_meteringHistory[_currentMeteringHistory].vote = (soaExhaustion >= loaExhaustion) ? VOTE_SOA : VOTE_LOA;

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
void
MM_ConcurrentGC::notifyAcquireExclusiveVMAccess(MM_EnvironmentBase *env)
{
	MM_ParallelGlobalGC::notifyAcquireExclusiveVMAccess(env);
	/* Record concurrent end time, concurrent may be halted and complete in STW */
	if (_stats.concurrentMarkInProgress() && (CONCURRENT_FINAL_COLLECTION > _stats.getExecutionMode())) {
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
		_concurrentPhaseStats._endTime = omrtime_hires_clock();
	}
}
#endif /* OMR_GC_LARGE_OBJECT_AREA */
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
