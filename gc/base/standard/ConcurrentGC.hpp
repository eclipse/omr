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

#if !defined(CONCURRENTGC_HPP_)
#define CONCURRENTGC_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "omr.h"
#include "OMR_VM.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#include "ConcurrentCardTable.hpp"
#include "ConcurrentMarkingDelegate.hpp"
#include "Collector.hpp"
#include "CollectorLanguageInterface.hpp"
#include "ConcurrentGCStats.hpp"
#include "CycleState.hpp"
#include "EnvironmentBase.hpp"
#include "ParallelGlobalGC.hpp"

extern "C" {
int con_helper_thread_proc(void *info);
uintptr_t con_helper_thread_proc2(OMRPortLibrary* portLib, void *info);
void J9ConcurrentWriteBarrierStore (OMR_VMThread *vmThread, omrobjectptr_t destinationObject, omrobjectptr_t storedObject);
void J9ConcurrentWriteBarrierBatchStore (OMR_VMThread *vmThread, omrobjectptr_t destinationObject);
}

/**
 * @name Concurrent marking rate
 * @{
 */
#define MAX_ALLOC_2_TRACE_RATE_1 (float)4.0
#define MAX_ALLOC_2_TRACE_RATE_8 (float)2.0
#define MAX_ALLOC_2_TRACE_RATE_10 (float)1.8
 
#define MIN_ALLOC_2_TRACE_RATE_1 (float)1.0
#define MIN_ALLOC_2_TRACE_RATE_8 (float)2.0
#define MIN_ALLOC_2_TRACE_RATE_10 (float)2.0

#define OVER_TRACING_BOOST_FACTOR (float)2.0

/**
 * @}
 */

/**
 * @name Concurrent mark card cleaning factor
 * @{
 */
#define INITIAL_CARD_CLEANING_FACTOR_PASS1_1 (float)0.5
#define INITIAL_CARD_CLEANING_FACTOR_PASS1_8 (float)0.05
#define INITIAL_CARD_CLEANING_FACTOR_PASS1_10 (float)0.05

#define INITIAL_CARD_CLEANING_FACTOR_PASS2_1 (float)0.1
#define INITIAL_CARD_CLEANING_FACTOR_PASS2_8 (float)0.01
#define INITIAL_CARD_CLEANING_FACTOR_PASS2_10 (float)0.01
 
#define MAX_CARD_CLEANING_FACTOR_PASS1_1 (float)0.8
#define MAX_CARD_CLEANING_FACTOR_PASS1_8 (float)0.2
#define MAX_CARD_CLEANING_FACTOR_PASS1_10 (float)0.2

#define MAX_CARD_CLEANING_FACTOR_PASS2_1 (float)0.5
#define MAX_CARD_CLEANING_FACTOR_PASS2_8 (float)0.1
#define MAX_CARD_CLEANING_FACTOR_PASS2_10 (float)0.1

/**
 * @}
 */

/**
 * @name Concurrent mark card cleaning threshold
 * @{
 */
#define CARD_CLEANING_THRESHOLD_FACTOR_1 (float)4.0  
#define CARD_CLEANING_THRESHOLD_FACTOR_8 (float)3.0  
#define CARD_CLEANING_THRESHOLD_FACTOR_10 (float)1.5   
 
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
#define CARD_CLEANING_HISTORY_WEIGHT ((float)0.7)
#define CONCURRENT_HELPER_HISTORY_WEIGHT ((float)0.6) 
#define BYTES_TRACED_IN_PASS_1_HISTORY_WEIGHT ((float)0.8)
	
#define TUNING_HEAP_SIZE_FACTOR ((float)0.05)
#define CONCURRENT_STOP_SAMPLE_GRAIN 0x0F

#define INIT_CHUNK_SIZE 8 * 1024
#define CONCURRENT_INIT_BOOST_FACTOR 8
#define CONCURRENT_KICKOFF_THRESHOLD_BOOST ((float)1.10)
#define LAST_FREE_SIZE_NEEDS_INITIALIZING ((uintptr_t)-1)
#define ALL_BYTES_TRACED_IN_PASS_1 ((float)1.0)

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
	typedef enum {
		MARK_BITS=1,
		CARD_TABLE
	} InitType;
	
	typedef struct {
		void *base;
		void *top;
		void * volatile current;
		uintptr_t initBytes;
		InitType type;
		uintptr_t chunkSize;
		MM_MemorySubSpace *subspace;
	} InitWorkItem;

	typedef enum {
		SOA = 1,
		LOA
	} MeteringType;

	typedef enum {
		VOTE_UNDEFINED = 0,
		VOTE_SOA,
		VOTE_LOA	
	} MeteringVote;

	typedef struct {
		uintptr_t soaFreeBeforeGC;
		uintptr_t soaFreeAfterGC;
		uintptr_t loaFreeBeforeGC;
		uintptr_t loaFreeAfterGC;
		MeteringVote vote;
	} MeteringHistory;

	enum {
		_minTraceSize = 1000,
		_maxTraceSize = 0x20000000,
		_conHelperCleanSize= 0x10000,
		_meteringHistorySize = 5
	};
	void *_heapBase;
	bool _rebuildInitWorkForRemove; /**< set if heap contraction triggered _initRanges table update */

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
	InitWorkItem *_initRanges;
	uint32_t _numInitRanges;
	uintptr_t _numPhysicalInitRanges;	/**< physical size of table */
	volatile uint32_t _nextInitRange;
	uintptr_t _initializers;
	bool _initSetupDone;

	/* Mutator tracing statistics */
	uintptr_t _allocToInitRate;
	uintptr_t _allocToTraceRateNormal;
	bool  _secondCardCleanPass;
	float _allocToTraceRateMaxFactor;
	float _allocToTraceRateMinFactor;
	float _bytesTracedInPass1Factor;
	uintptr_t _bytesToCleanPass1;
	uintptr_t _bytesToCleanPass2;
	uintptr_t _bytesToTracePass1;
	uintptr_t _bytesToTracePass2;
	uintptr_t _traceTargetPass1;
	uintptr_t _traceTargetPass2;
	float _tenureLiveObjectFactor;
	float _tenureNonLeafObjectFactor;
	uintptr_t _kickoffThresholdBuffer;

	/* Periodic tuning statistics */
	volatile uintptr_t _tuningUpdateInterval;
	volatile uintptr_t _lastFreeSize;
	float _lastAverageAlloc2TraceRate;
	float _maxAverageAlloc2TraceRate;
	uintptr_t _lastTotalTraced;

	/* Background helper thread statistics */
	uintptr_t _lastConHelperTraceSizeCount;
	float _alloc2ConHelperTraceRate;

	/* Concurrent card cleaning statistics */
	float _cardCleaningFactorPass1;
	float _cardCleaningFactorPass2;
	float _maxCardCleaningFactorPass1;
	float _maxCardCleaningFactorPass2;
	float _cardCleaningThresholdFactor;

	bool _forcedKickoff;	/**< Kickoff forced externally flag */

	uintptr_t _languageKickoffReason;

protected:

	typedef enum {
		CONCURRENT_HELPER_WAIT = 1,
		CONCURRENT_HELPER_MARK,
		CONCURRENT_HELPER_SHUTDOWN
	} ConHelperRequest;

	ConHelperRequest _conHelpersRequest;
	MM_CycleState _concurrentCycleState;
	MM_ConcurrentCardTable *_cardTable;	/**< pointer to Cards Table */

	void *_heapAlloc;
	bool _rebuildInitWorkForAdd; /**< set if heap expansion triggered _initRanges table update */
	bool _retuneAfterHeapResize;

	/* Mutator tracing statistics */
	uintptr_t _allocToTraceRate;
	uintptr_t _allocToTraceRateCardCleanPass2Boost;
	bool _pass2Started;
	uintptr_t _totalTracedAtPass2KO;
	uintptr_t _totalCleanedAtPass2KO;
	
	MM_ConcurrentMarkingDelegate _concurrentDelegate;

	MM_ConcurrentSafepointCallback *_callback;
	MM_ConcurrentGCStats _stats;


public:
	
	/*
	 * Function members
	 */
private:
	MMINLINE float getAllocToTraceRateMin() { return (float)(_allocToTraceRate * _allocToTraceRateMinFactor);};
	MMINLINE float getAllocToTraceRateMax() { return (float)(_allocToTraceRate * _allocToTraceRateMaxFactor);};
	MMINLINE float getAllocToTraceRateNormal() { return (float)_allocToTraceRateNormal;};
	
	float interpolateInRange(float, float, float, uintptr_t);
	void determineInitWork(MM_EnvironmentBase *env);
	void resetInitRangesForConcurrentKO();
	void resetInitRangesForSTW();
	bool getInitRange(MM_EnvironmentBase *env, void **from, void **to, InitType *type, bool *concurrentCollectable);
	bool allInitRangesProcessed() { return (_nextInitRange == _numInitRanges ? true : false); };
	bool periodicalTuningNeeded(MM_EnvironmentBase *env, uintptr_t freeSize);
	void periodicalTuning(MM_EnvironmentBase *env, uintptr_t freeSize);
	void kickoffCardCleaning(MM_EnvironmentBase *env, ConcurrentCardCleaningReason reason);
	
	void adjustTraceTarget();
	void updateTuningStatistics(MM_EnvironmentBase *env);
	void tuneToHeap(MM_EnvironmentBase *env);

	void conHelperEntryPoint(OMR_VMThread *omrThread, uintptr_t slaveID);
	void shutdownAndExitConHelperThread(OMR_VMThread *omrThread);

	bool initializeConcurrentHelpers(MM_GCExtensionsBase *extensions);
	void shutdownConHelperThreads(MM_GCExtensionsBase *extensions);
	bool timeToKickoffConcurrent(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);

	bool tracingRateDropped(MM_EnvironmentBase *env);
#if defined(OMR_GC_MODRON_SCAVENGER)	
	uintptr_t potentialFreeSpace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
#endif /*OMR_GC_MODRON_SCAVENGER */	

	bool cleanCards(MM_EnvironmentBase *env, bool isMutator, uintptr_t sizeToDo, uintptr_t  *sizeDone, bool threadAtSafePoint);

	void reportConcurrentKickoff(MM_EnvironmentBase *env);
	void reportConcurrentAborted(MM_EnvironmentBase *env, CollectionAbortReason reason);
	virtual void reportConcurrentHalted(MM_EnvironmentBase *env);
	void reportConcurrentFinalCardCleaningStart(MM_EnvironmentBase *env);
	void reportConcurrentFinalCardCleaningEnd(MM_EnvironmentBase *env, uint64_t duration);
	void reportConcurrentCollectionStart(MM_EnvironmentBase *env);
	void reportConcurrentCollectionEnd(MM_EnvironmentBase *env, uint64_t duration);
	
	void reportConcurrentBackgroundThreadActivated(MM_EnvironmentBase *env);
	void reportConcurrentBackgroundThreadFinished(MM_EnvironmentBase *env, uintptr_t traceTotal);
	
	void reportConcurrentCompleteTracingStart(MM_EnvironmentBase *env);
	void reportConcurrentCompleteTracingEnd(MM_EnvironmentBase *env, uint64_t duration);
	
	void reportConcurrentRememberedSetScanStart(MM_EnvironmentBase *env);
	void reportConcurrentRememberedSetScanEnd(MM_EnvironmentBase *env, uint64_t duration);

	virtual bool internalGarbageCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription);

	void clearConcurrentWorkStackOverflow();

#if defined(OMR_GC_CONCURRENT_SWEEP)
	void concurrentSweep(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, MM_AllocateDescription *allocDescription);
	void completeConcurrentSweep(MM_EnvironmentBase *env);
	void completeConcurrentSweepForKickoff(MM_EnvironmentBase *env);
#endif /* OMR_GC_CONCURRENT_SWEEP */

#if defined(OMR_GC_LARGE_OBJECT_AREA)		
	void updateMeteringHistoryBeforeGC(MM_EnvironmentBase *env);
	void updateMeteringHistoryAfterGC(MM_EnvironmentBase *env);
#endif /* OMR_GC_LARGE_OBJECT_AREA */

	/**
	 * Get the current value of _conHelperRequest
	 *
	 * @return the value of _conHelperRequest.
	 */
	ConHelperRequest getConHelperRequest(MM_EnvironmentBase *env);

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

	uintptr_t doConcurrentInitialization(MM_EnvironmentBase *env, uintptr_t initToDo);
	uintptr_t doConcurrentTrace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t sizeToTrace, MM_MemorySubSpace *subspace, bool tlhAllocation);
	void signalThreadsToActivateWriteBarrier(MM_EnvironmentBase *env);
	void resumeConHelperThreads(MM_EnvironmentBase *env);
	uintptr_t calculateInitSize(MM_EnvironmentBase *env, uintptr_t allocationSize);
	uintptr_t calculateTraceSize(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription);
	void concurrentMark(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace,  MM_AllocateDescription *allocDescription);
	virtual void internalPreCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace, MM_AllocateDescription *allocDescription, uint32_t gcCode);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);

public:
	virtual uintptr_t getVMStateID() { return OMRVMSTATE_GC_COLLECTOR_CONCURRENTGC; };

	static 	MM_ConcurrentGC *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	
	virtual void postMark(MM_EnvironmentBase *env);
	virtual bool collectorStartup(MM_GCExtensionsBase* extensions);
	virtual void collectorShutdown(MM_GCExtensionsBase *extensions);
	virtual void abortCollection(MM_EnvironmentBase *env, CollectionAbortReason reason);
	
	static void signalThreadsToActivateWriteBarrierAsyncEventHandler(OMR_VMThread *omrVMThread, void *userData);
	
	virtual void prepareHeapForWalk(MM_EnvironmentBase *env);

	virtual void scanThread(MM_EnvironmentBase *env)
	{
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
	virtual uintptr_t localMark(MM_EnvironmentBase *env, uintptr_t sizeToTrace);
	
	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	virtual void heapReconfigured(MM_EnvironmentBase *env, HeapReconfigReason reason, MM_MemorySubSpace *subspace, void *lowAddress, void *highAddress);

	void finalCleanCards(MM_EnvironmentBase *env);

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
	void oldToOldReferenceCreated(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);
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

	/**
	 * Return reference to Card Table
	 */
	MMINLINE MM_ConcurrentCardTable *getCardTable()
	{
		return _cardTable;
	}

	MMINLINE MM_ConcurrentGCStats *getConcurrentGCStats() { return &_stats; }

	void concurrentWorkStackOverflow();
	
	MM_ConcurrentGC(MM_EnvironmentBase *env)
		: MM_ParallelGlobalGC(env)
		,_heapBase(NULL)
		,_rebuildInitWorkForRemove(false)
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
		,_initRanges(NULL)
		,_numInitRanges(0)
		,_numPhysicalInitRanges(0)
		,_nextInitRange(0)
		,_initializers(0)
		,_initSetupDone(false)
		,_bytesToCleanPass1(0)
		,_bytesToCleanPass2(0)
		,_bytesToTracePass1(0)
		,_bytesToTracePass2(0)
		,_tenureLiveObjectFactor(INITIAL_OLD_AREA_LIVE_PART_FACTOR)
		,_tenureNonLeafObjectFactor(INITIAL_OLD_AREA_NON_LEAF_FACTOR)
		,_tuningUpdateInterval(_minTraceSize)
		,_lastFreeSize(LAST_FREE_SIZE_NEEDS_INITIALIZING)
		,_lastAverageAlloc2TraceRate(0)
		,_maxAverageAlloc2TraceRate(0)
		,_lastTotalTraced(0)
		,_lastConHelperTraceSizeCount(0)
		,_alloc2ConHelperTraceRate(0)
		,_forcedKickoff(false)
		,_languageKickoffReason(NO_LANGUAGE_KICKOFF_REASON)
		,_conHelpersRequest(CONCURRENT_HELPER_WAIT)
		,_concurrentCycleState()
		,_cardTable(NULL)
		,_heapAlloc(NULL)
		,_rebuildInitWorkForAdd(false)
		,_retuneAfterHeapResize(false)
		,_callback(NULL)
		,_stats()
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
