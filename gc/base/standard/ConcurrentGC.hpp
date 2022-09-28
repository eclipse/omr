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

#if !defined(CONCURRENTGC_HPP_)
#define CONCURRENTGC_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "omr.h"
#include "OMR_VM.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#include "ConcurrentMarkingDelegate.hpp"
#include "ConcurrentMarkPhaseStats.hpp"
#include "Collector.hpp"
#include "CollectorLanguageInterface.hpp"
#include "ConcurrentGCStats.hpp"
#include "CycleState.hpp"
#include "EnvironmentStandard.hpp"
#include "ParallelGlobalGC.hpp"

extern "C" {
int con_helper_thread_proc(void *info);
uintptr_t con_helper_thread_proc2(OMRPortLibrary* portLib, void *info);
void concurrentPostWriteBarrierStore(OMR_VMThread *vmThread, omrobjectptr_t destinationObject, omrobjectptr_t storedObject);
void concurrentPostWriteBarrierBatchStore(OMR_VMThread *vmThread, omrobjectptr_t destinationObject);
}

/**
 * @name Concurrent marking rate
 * @{
 */
#define MAX_ALLOC_2_TRACE_RATE_1 ((float)4.0)
#define MAX_ALLOC_2_TRACE_RATE_8 ((float)2.0)
#define MAX_ALLOC_2_TRACE_RATE_10 ((float)1.8)
 
#define MIN_ALLOC_2_TRACE_RATE_1 ((float)1.0)
#define MIN_ALLOC_2_TRACE_RATE_8 ((float)2.0)
#define MIN_ALLOC_2_TRACE_RATE_10 ((float)2.0)

#define OVER_TRACING_BOOST_FACTOR ((float)2.0)

/**
 * @}
 */

/**
 * @name Concurrent mark misc definitions
 * @{
 */ 
#define INITIAL_OLD_AREA_LIVE_PART_FACTOR ((float)0.7)
#define LIVE_PART_HISTORY_WEIGHT ((float)0.8)
#define INITIAL_OLD_AREA_NON_LEAF_FACTOR ((float)0.4)
#define NON_LEAF_HISTORY_WEIGHT ((float)0.8)
#define CONCURRENT_HELPER_HISTORY_WEIGHT ((float)0.6)

#define TUNING_HEAP_SIZE_FACTOR ((float)0.05)
#define CONCURRENT_STOP_SAMPLE_GRAIN 0x0F

#define INIT_CHUNK_SIZE 8 * 1024
#define CONCURRENT_INIT_BOOST_FACTOR 8
#define CONCURRENT_KICKOFF_THRESHOLD_BOOST ((float)1.10)
#define LAST_FREE_SIZE_NEEDS_INITIALIZING ((uintptr_t)-1)

/**
 * @}
 */

#if defined(OMR_GC_CONCURRENT_SWEEP)
class MM_ConcurrentSweepScheme;
#endif /* OMR_GC_CONCURRENT_SWEEP */

class MM_AllocateDescription;
class MM_ConcurrentSafepointCallback;
class MM_MemorySubSpace;
class MM_MemorySubSpaceConcurrent;
class MM_MemorySubSpaceGenerational;
class MM_SpinLimiter;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentGC : public MM_ParallelGlobalGC
{
	/*
	 * Data members
	 */
private:
	enum MeteringType {
		SOA = 1,
		LOA
	};

	enum MeteringVote {
		VOTE_UNDEFINED = 0,
		VOTE_SOA,
		VOTE_LOA	
	};

	struct MeteringHistory {
		uintptr_t soaFreeBeforeGC;
		uintptr_t soaFreeAfterGC;
		uintptr_t loaFreeBeforeGC;
		uintptr_t loaFreeAfterGC;
		MeteringVote vote;
	};

	void *_heapBase;

#if defined(OMR_GC_LARGE_OBJECT_AREA)
	MeteringHistory *_meteringHistory;
	uint32_t _currentMeteringHistory;
	MeteringType _meteringType;
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	omrthread_t *_conHelpersTable;
	uint32_t _conHelperThreads;
	uint32_t _conHelpersStarted;
	volatile uint32_t _conHelpersShutdownCount;
	omrthread_monitor_t _conHelpersActivationMonitor;

	bool _initializeMarkMap;
	omrthread_monitor_t _initWorkMonitor;
	omrthread_monitor_t _initWorkCompleteMonitor;
	omrthread_monitor_t _concurrentTuningMonitor;

	/* Concurrent initialization */
	uint32_t _numInitRanges;
	uintptr_t _numPhysicalInitRanges;	/**< physical size of table */
	volatile uint32_t _nextInitRange;
	uintptr_t _initializers;
	bool _initSetupDone;

	/* Periodic tuning statistics */
	volatile uintptr_t _tuningUpdateInterval;
	volatile uintptr_t _lastFreeSize;
	float _lastAverageAlloc2TraceRate;
	float _maxAverageAlloc2TraceRate;
	uintptr_t _lastTotalTraced;

	/* Background helper thread statistics */
	uintptr_t _lastConHelperTraceSizeCount;
	float _alloc2ConHelperTraceRate;

	bool _forcedKickoff;	/**< Kickoff forced externally flag */

	uintptr_t _languageKickoffReason;

protected:
	enum InitType {
		MARK_BITS = 1,
		CARD_TABLE
	};

	struct InitWorkItem {
		void *base;
		void *top;
		void * volatile current;
		uintptr_t initBytes;
		InitType type;
		uintptr_t chunkSize;
		MM_MemorySubSpace *subspace;
	};

	enum {
		_minTraceSize = 1000,
		_maxTraceSize = 0x20000000,
		_conHelperCleanSize = 0x10000,
		_meteringHistorySize = 5
	};

	enum ConHelperRequest {
		CONCURRENT_HELPER_WAIT = 1,
		CONCURRENT_HELPER_MARK,
		CONCURRENT_HELPER_SHUTDOWN
	};

	InitWorkItem *_initRanges;

	/* Mutator tracing statistics */
	uintptr_t _allocToInitRate;
	uintptr_t _allocToTraceRate;
	uintptr_t _allocToTraceRateNormal;
	float _allocToTraceRateMaxFactor;
	float _allocToTraceRateMinFactor;

	float _tenureLiveObjectFactor;
	float _tenureNonLeafObjectFactor;
	uintptr_t _kickoffThresholdBuffer;

	ConHelperRequest _conHelpersRequest;
	MM_CycleState _concurrentCycleState;

	void *_heapAlloc;
	bool _rebuildInitWorkForAdd; /**< set if heap expansion triggered _initRanges table update */
	bool _rebuildInitWorkForRemove; /**< set if heap contraction triggered _initRanges table update */
	bool _retuneAfterHeapResize;

	MM_ConcurrentMarkingDelegate _concurrentDelegate;

	MM_ConcurrentSafepointCallback *_callback;
	MM_ConcurrentGCStats _stats;
	MM_ConcurrentMarkPhaseStats _concurrentPhaseStats;

	/*
	 * Function members
	 */
private:
	MMINLINE float getAllocToTraceRateMin() { return ((float)(_allocToTraceRate * _allocToTraceRateMinFactor)); };
	MMINLINE float getAllocToTraceRateMax() { return ((float)(_allocToTraceRate * _allocToTraceRateMaxFactor)); };
	MMINLINE float getAllocToTraceRateNormal() { return ((float)_allocToTraceRateNormal); };
	
	void determineInitWork(MM_EnvironmentBase *env);
	void resetInitRangesForConcurrentKO();
	void resetInitRangesForSTW();
	bool allInitRangesProcessed() { return ((_nextInitRange == _numInitRanges) ? true : false); };

	void updateTuningStatistics(MM_EnvironmentBase *env);

	void conHelperEntryPoint(OMR_VMThread *omrThread, uintptr_t workerID);
	void shutdownAndExitConHelperThread(OMR_VMThread *omrThread);

	bool initializeConcurrentHelpers(MM_GCExtensionsBase *extensions);
	void shutdownConHelperThreads(MM_GCExtensionsBase *extensions);
	bool timeToKickoffConcurrent(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

	void reportConcurrentKickoff(MM_EnvironmentBase *env);
	void reportConcurrentAborted(MM_EnvironmentBase *env, CollectionAbortReason reason);
	void reportConcurrentCollectionEnd(MM_EnvironmentBase *env, uint64_t duration);
	void reportConcurrentBackgroundThreadActivated(MM_EnvironmentBase *env);
	void reportConcurrentBackgroundThreadFinished(MM_EnvironmentBase *env, uintptr_t traceTotal);
	void reportConcurrentRememberedSetScanStart(MM_EnvironmentBase *env);
	void reportConcurrentRememberedSetScanEnd(MM_EnvironmentBase *env, uint64_t duration);
	virtual void reportConcurrentHalted(MM_EnvironmentBase *env) {};
	virtual void reportConcurrentCollectionStart(MM_EnvironmentBase *env) = 0;

	virtual bool internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription);

	virtual uintptr_t getTraceTarget() = 0;
#if defined(OMR_GC_CONCURRENT_SWEEP)
	void concurrentSweep(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_AllocateDescription *allocDescription);
	void completeConcurrentSweep(MM_EnvironmentBase *env);
	void completeConcurrentSweepForKickoff(MM_EnvironmentBase *env);
#endif /* OMR_GC_CONCURRENT_SWEEP */

#if defined(OMR_GC_LARGE_OBJECT_AREA)		
	void updateMeteringHistoryBeforeGC(MM_EnvironmentBase *env);
	void updateMeteringHistoryAfterGC(MM_EnvironmentBase *env);
#endif /* OMR_GC_LARGE_OBJECT_AREA */

protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	/**
	 * Switch the _conHelperRequest to appropriate state and return the new value.
	 * This will only switch the state if from == the current state.
	 *
	 * @param from - switch the state only if it currently equals this value
	 * @param to - the value to switch the state to
	 * @return the value of _conHelperRequest.
	 */
	ConHelperRequest switchConHelperRequest(ConHelperRequest from, ConHelperRequest to);

	/**
	 * Get the current value of _conHelperRequest
	 *
	 * @return the value of _conHelperRequest.
	 */
	ConHelperRequest getConHelperRequest(MM_EnvironmentBase *env);
	virtual void conHelperDoWorkInternal(MM_EnvironmentBase *env, ConHelperRequest *request, MM_SpinLimiter *spinLimiter, uintptr_t *totalScanned) {};
	void resumeConHelperThreads(MM_EnvironmentBase *env);

	virtual uintptr_t doConcurrentTrace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t sizeToTrace, MM_MemorySubSpace *subspace, bool tlhAllocation) = 0;
	void concurrentMark(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace,  MM_AllocateDescription *allocDescription);

	uintptr_t calculateInitSize(MM_EnvironmentBase *env, uintptr_t allocationSize);
	uintptr_t calculateTraceSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

	virtual void setupForConcurrent(MM_EnvironmentBase *env) = 0;
	virtual void finalConcurrentPrecollect(MM_EnvironmentBase *env) = 0;
	virtual void completeConcurrentTracing(MM_EnvironmentBase *env, uintptr_t executionModeAtGC) {};

	virtual void adjustTraceTarget() = 0;
	virtual void tuneToHeap(MM_EnvironmentBase *env) = 0;
	virtual void resetConcurrentParameters(MM_EnvironmentBase *env);
	virtual void updateTuningStatisticsInternal(MM_EnvironmentBase *env) {};

	bool tracingRateDropped(MM_EnvironmentBase *env);
	bool periodicalTuningNeeded(MM_EnvironmentBase *env, uintptr_t freeSize);
	void periodicalTuning(MM_EnvironmentBase *env, uintptr_t freeSize);

#if defined(OMR_GC_MODRON_SCAVENGER)
	uintptr_t potentialFreeSpace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
#endif /*OMR_GC_MODRON_SCAVENGER */

	void reportConcurrentCompleteTracingStart(MM_EnvironmentBase *env);
	void reportConcurrentCompleteTracingEnd(MM_EnvironmentBase *env, uint64_t duration);

	virtual void preConcurrentInitializeStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats = NULL);
	virtual void postConcurrentUpdateStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats = NULL, UDATA bytesConcurrentlyScanned = 0);

	uintptr_t doConcurrentInitialization(MM_EnvironmentBase *env, uintptr_t initToDo);
	virtual uintptr_t doConcurrentInitializationInternal(MM_EnvironmentBase *env, uintptr_t initToDo);

	virtual uint32_t numberOfInitRanages(MM_MemorySubSpace *subspace) { return 1; };
	virtual void determineInitWorkInternal(MM_EnvironmentBase *env, uint32_t initIndex) {};
	void recalculateInitWork(MM_EnvironmentBase *env);
	bool getInitRange(MM_EnvironmentBase *env, void **from, void **to, InitType *type, bool *concurrentCollectable);

	virtual void initalizeConcurrentStructures(MM_EnvironmentBase *env) {};
	virtual bool contractInternalConcurrentStructures(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress) { return true; };

	virtual bool canSkipObjectRSScan(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) { return false; };

	virtual void clearWorkStackOverflow();

	MMINLINE virtual uintptr_t getMutatorTotalTraced() { return _stats.getTraceSizeCount(); };

	MMINLINE virtual uintptr_t getConHelperTotalTraced() { return _stats.getConHelperTraceSizeCount(); };

	MMINLINE virtual uintptr_t workCompleted() { return (getMutatorTotalTraced() + getConHelperTotalTraced()); };

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
	MMINLINE float interpolateInRange(float val1, float val8, float val10, uintptr_t traceRate)
	{
		float ret;

		if (traceRate > 8) {
			ret = (float)(val8 + ( ((val10 - val8) / 2.0) * (traceRate-8) ) );
		} else {
			ret = (float)(val1 + ( ((val8 - val1)  / 7.0) * (traceRate-1) ) );
		}

		return ret;
	}

	virtual bool acquireExclusiveVMAccessForCycleStart(MM_EnvironmentBase *env) = 0;

	virtual bool acquireExclusiveVMAccessForCycleEnd(MM_EnvironmentBase *env)
	{
		return env->acquireExclusiveVMAccessForGC(this, true, true);
	}

public:
	virtual uintptr_t getVMStateID() { return OMRVMSTATE_GC_COLLECTOR_CONCURRENTGC; };

	virtual void kill(MM_EnvironmentBase *env);
	
	virtual void postMark(MM_EnvironmentBase *env);
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase *extensions);
	virtual void abortCollection(MM_EnvironmentBase *env, CollectionAbortReason reason);
	
	static void signalThreadsToActivateWriteBarrierAsyncEventHandler(OMR_VMThread *omrVMThread, void *userData);
	void acquireExclusiveVMAccessAndSignalThreadsToActivateWriteBarrier(MM_EnvironmentBase *env);
	
	virtual void prepareHeapForWalk(MM_EnvironmentBase *env);

	virtual void scanThread(MM_EnvironmentBase *env)
	{
		Assert_MM_true(!_extensions->usingSATBBarrier()); /* Threads are scanned in STW for Concurrent SATB, ensure we don't end up at this call back */
		uintptr_t mode = _stats.getExecutionMode();
		if ((CONCURRENT_ROOT_TRACING <= mode) && (CONCURRENT_EXHAUSTED > mode)) {
			env->_workStack.reset(env, _markingScheme->getWorkPackets());
			if (_concurrentDelegate.scanThreadRoots(env)) {
				flushLocalBuffers(env);
				env->setThreadScanned(true);
				_stats.incThreadsScannedCount();
			}
		}
	}

	/**
	 * Force Kickoff event externally
	 * @return true if Kickoff can be forced
	 */
	virtual bool forceKickoff(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);

#if defined(OMR_GC_CONCURRENT_SWEEP)
	virtual bool replenishPoolForAllocate(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool, uintptr_t size);
#endif /* OMR_GC_CONCURRENT_SWEEP */
	virtual void payAllocationTax(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_MemorySubSpace *baseSubSpace, MM_AllocateDescription *allocDescription);
	bool concurrentFinalCollection(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace);
	virtual uintptr_t localMark(MM_EnvironmentBase *env, uintptr_t sizeToTrace) = 0;
	
	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	virtual void heapReconfigured(MM_EnvironmentBase *env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress);

	/**
	 * Flush local buffers
	 * (local reference object buffer, work packets)
	 * @param env Local thread environment
	 */
	void flushLocalBuffers(MM_EnvironmentBase *env);
	
	void clearNewMarkBits(MM_EnvironmentBase *env);
	void completeTracing(MM_EnvironmentBase *env);

#if defined(OMR_GC_MODRON_SCAVENGER)
	void scanRememberedSet(MM_EnvironmentBase *env);
	virtual void oldToOldReferenceCreated(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) {};
	virtual void objectTenured(MM_EnvironmentBase *env, omrobjectptr_t objectPtr) {};
#endif /* OMR_GC_MODRON_SCAVENGER */
	
	/**
	 * Check if exclusive access has been requested 
	 * Determine whether exclusive access has been requested by another thread 
	 * but only do so once for every CONCURRENT_STOP_SAMPLE_GRAIN call to this routine.
	 * @return  TRUE if concurrent collection is in progress; FALSE otherwise 
	*/
	MMINLINE bool isExclusiveAccessRequestWaitingSparseSample(MM_EnvironmentBase *env, uintptr_t value)
	{
		/* Have we reached a sample point ? */ 
		if ((0 == (value & CONCURRENT_STOP_SAMPLE_GRAIN)) && env->isExclusiveAccessRequestWaiting()) {
			return true;
		} else {
			return false;
		}	
	}

	MMINLINE MM_ConcurrentGCStats *getConcurrentGCStats() { return &_stats; };

	virtual void workStackOverflow();
	virtual void notifyAcquireExclusiveVMAccess(MM_EnvironmentBase *env);
	
	MM_ConcurrentGC(MM_EnvironmentBase *env)
		: MM_ParallelGlobalGC(env)
		,_heapBase(NULL)
#if defined(OMR_GC_LARGE_OBJECT_AREA)		
		,_meteringHistory(NULL)
		,_currentMeteringHistory(0)
		,_meteringType(SOA)
#endif /* OMR_GC_LARGE_OBJECT_AREA */		
		,_conHelpersTable(NULL)
		,_conHelperThreads((uint32_t)_extensions->concurrentBackground)
		,_conHelpersStarted(0)
		,_conHelpersShutdownCount(0)
		,_conHelpersActivationMonitor(NULL)
		,_initializeMarkMap(false)
		,_initWorkMonitor(NULL)
		,_initWorkCompleteMonitor(NULL)
		,_concurrentTuningMonitor(NULL)
		,_numInitRanges(0)
		,_numPhysicalInitRanges(0)
		,_nextInitRange(0)
		,_initializers(0)
		,_initSetupDone(false)
		,_tuningUpdateInterval(_minTraceSize)
		,_lastFreeSize(LAST_FREE_SIZE_NEEDS_INITIALIZING)
		,_lastAverageAlloc2TraceRate(0)
		,_maxAverageAlloc2TraceRate(0)
		,_lastTotalTraced(0)
		,_lastConHelperTraceSizeCount(0)
		,_alloc2ConHelperTraceRate(0)
		,_forcedKickoff(false)
		,_languageKickoffReason(NO_LANGUAGE_KICKOFF_REASON)
		,_initRanges(NULL)
		,_allocToInitRate(0)
		,_allocToTraceRate(0)
		,_allocToTraceRateNormal(0)
		,_allocToTraceRateMaxFactor(0.0f)
		,_allocToTraceRateMinFactor(0.0f)
		,_tenureLiveObjectFactor(INITIAL_OLD_AREA_LIVE_PART_FACTOR)
		,_tenureNonLeafObjectFactor(INITIAL_OLD_AREA_NON_LEAF_FACTOR)
		,_kickoffThresholdBuffer(0)
		,_conHelpersRequest(CONCURRENT_HELPER_WAIT)
		,_concurrentCycleState()
		,_heapAlloc(NULL)
		,_rebuildInitWorkForAdd(false)
		,_rebuildInitWorkForRemove(false)
		,_retuneAfterHeapResize(false)
		,_callback(NULL)
		,_stats()
		,_concurrentPhaseStats()
		{
			_typeId = __FUNCTION__;
		}
	
	/*
	 * Friends
	 */
	friend int con_helper_thread_proc(void *info);
	friend uintptr_t con_helper_thread_proc2(OMRPortLibrary* portLib, void *info);
	friend class MM_ConcurrentOverflow;
	friend class MM_ConcurrentCardTableForWC;
#if defined(OMR_GC_CONCURRENT_SWEEP)
	friend class MM_ConcurrentSweepScheme;
#endif /* OMR_GC_CONCURRENT_SWEEP */
};

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

#endif /* CONCURRENTGC_HPP_ */
