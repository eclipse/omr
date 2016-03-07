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

#if !defined(VERBOSEMANAGERBASE_HPP_)
#define VERBOSEMANAGERBASE_HPP_

#include "omrcfg.h"
#include "omrhookable.h"
#include "mmhook_common.h"

#include "AtomicOperations.hpp"
#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_VerboseHandlerOutput;
class MM_VerboseWriterChain;

typedef struct{
	uint64_t initialized;
	uint64_t systemGC;
	uint64_t allocationFailure;
	uint64_t nurseryAF;
	uint64_t tenureAF;
	uint64_t globalGC;
	uint64_t concurrentGC;
	uint64_t localGC;
	uint64_t partialGC;
	uint64_t globalMarkGC;
	uint64_t balancedGlobalGC;
	uint64_t taxationEntryPoint;
	uint64_t exclusiveAccessStart;
	uint64_t exclusiveAccessEnd;
#if defined(OMR_GC_REALTIME)
	uint64_t metronomeSynchGC;
	uint64_t metronomeHeartbeat;
	uint64_t metronomeThreadPriorityChange;
	uint64_t metronomeTriggerStart;
	uint64_t metronomeTriggerEnd;
	uint64_t metronomeCycleStart;
	uint64_t metronomeCycleEnd;		
#endif /* OMR_GC_REALTIME */
	uint64_t tarokIncrementStart;	/**< The last time a tarok increment started */
	uint64_t tarokIncrementEnd;	/**< The last time a tarok increment ended */
} PreviousTimes;

typedef struct{
	uintptr_t systemGC;
	uintptr_t nurseryAF;
	uintptr_t tenureAF;
	uintptr_t concurrentGC;
#if defined(OMR_GC_REALTIME)
	uintptr_t metronomeSynchGC;
	uintptr_t metronomeHeartbeat;
	uintptr_t metronomeThreadPriorityChange;
	uintptr_t metronomeTrigger;
	uintptr_t metronomeCycle;		
#endif /* OMR_GC_REALTIME */
} CollectionCounts;

#define VERBOSEGC_DATE_FORMAT "%b %d %H:%M:%S %Y"

#define VERBOSEGC_DATE_FORMAT_PRE_MS "%Y-%m-%dT%H:%M:%S."
#define VERBOSEGC_DATE_FORMAT_POST_MS ""

/**
 * The central class of the verbose gc mechanism.
 * Acts as the anchor point for the EventStream and Output Agent chain.
 * Provides routines for management of the output agents.
 * Stores data which needs to persist across streams.
 * @ingroup GC_verbose_engine
 */
class MM_VerboseManagerBase : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:

protected:
	OMR_VM *_omrVM;

	uintptr_t _indentationLevel;

	/* The current state of the hooks */
	bool _hooksAttached;

	/* Pointers to the gc internal Hook interface */
	J9HookInterface** _mmPrivateHooks;
	J9HookInterface** _omrHooks;
	
	/* Structures for tracking data across streams */
	PreviousTimes _timeOfLast;
	CollectionCounts _countOf;

	uint64_t _lastOutputTime; /**< The last time verbosegc was output */
	uintptr_t _outputCount; /**< Number of times verbosegc has been output */

	uintptr_t _curId;
	uint64_t _startTime;

public:
	/*
	 * Function members
	 */
private:

protected:

	virtual bool initialize(MM_EnvironmentBase *env) = 0;
	virtual void tearDown(MM_EnvironmentBase *env) = 0;

public:

	MMINLINE uint64_t getInitializedTime()              { return _timeOfLast.initialized; };
	MMINLINE uint64_t getLastSystemGCTime()             { return _timeOfLast.systemGC; };
	MMINLINE uint64_t getLastAllocationFailureTime()    { return _timeOfLast.allocationFailure; };
	MMINLINE uint64_t getLastNurseryAFTime()            { return _timeOfLast.nurseryAF; };
	MMINLINE uint64_t getLastTenureAFTime()             { return _timeOfLast.tenureAF; };
	MMINLINE uint64_t getLastGlobalGCTime()             { return _timeOfLast.globalGC; };
	MMINLINE uint64_t getLastConcurrentGCTime()         { return _timeOfLast.concurrentGC; };
	MMINLINE uint64_t getLastLocalGCTime()              { return _timeOfLast.localGC; };
	MMINLINE uint64_t getLastPartialGCTime()            { return _timeOfLast.partialGC; };
	MMINLINE uint64_t getLastGlobalMarkGCTime()         { return _timeOfLast.globalMarkGC; };
	MMINLINE uint64_t getLastBalancedGlobalGCTime()     { return _timeOfLast.balancedGlobalGC; };
	MMINLINE uint64_t getLastTaxationEntryPointTime()   { return _timeOfLast.taxationEntryPoint; };
	MMINLINE uint64_t getLastExclusiveAccessStartTime() { return _timeOfLast.exclusiveAccessStart; };
	MMINLINE uint64_t getLastExclusiveAccessEndTime()   { return _timeOfLast.exclusiveAccessEnd; };

	MMINLINE void setInitializedTime(uint64_t time)              { _timeOfLast.initialized = time; };
	MMINLINE void setLastSystemGCTime(uint64_t time)             { _timeOfLast.systemGC = time; };
	MMINLINE void setLastAllocationFailureTime(uint64_t time)    { _timeOfLast.allocationFailure = time; };
	MMINLINE void setLastNurseryAFTime(uint64_t time)            { _timeOfLast.nurseryAF = time; };
	MMINLINE void setLastTenureAFTime(uint64_t time)             { _timeOfLast.tenureAF = time; };
	MMINLINE void setLastGlobalGCTime(uint64_t time)             { _timeOfLast.globalGC = time; };
	MMINLINE void setLastConcurrentGCTime(uint64_t time)         { _timeOfLast.concurrentGC = time; };
	MMINLINE void setLastLocalGCTime(uint64_t time)              { _timeOfLast.localGC = time; };
	MMINLINE void setLastPartialGCTime(uint64_t time)            { _timeOfLast.partialGC = time; };
	MMINLINE void setLastGlobalMarkGCTime(uint64_t time)         { _timeOfLast.globalMarkGC = time; };
	MMINLINE void setLastBalancedGlobalGCTime(uint64_t time)     { _timeOfLast.balancedGlobalGC = time; };
	MMINLINE void setLastTaxationEntryPointTime(uint64_t time)   { _timeOfLast.taxationEntryPoint = time; };
	MMINLINE void setLastExclusiveAccessStartTime(uint64_t time) { _timeOfLast.exclusiveAccessStart = time; };
	MMINLINE void setLastExclusiveAccessEndTime(uint64_t time)   { _timeOfLast.exclusiveAccessEnd = time; };

	MMINLINE uint64_t getLastTarokIncrementEndTime() { return _timeOfLast.tarokIncrementStart; }
	MMINLINE void setLastTarokIncrementEndTime(uint64_t time) { _timeOfLast.tarokIncrementStart = time; }
	MMINLINE uint64_t getLastTarokIncrementStartTime() { return _timeOfLast.tarokIncrementEnd; }
	MMINLINE void setLastTarokIncrementStartTime(uint64_t time) { _timeOfLast.tarokIncrementEnd = time; }

	MMINLINE uintptr_t getSystemGCCount()		{	return _countOf.systemGC;	};
	MMINLINE uintptr_t getNurseryAFCount()		{	return _countOf.nurseryAF;	};
	MMINLINE uintptr_t getTenureAFCount()		{	return _countOf.tenureAF;	};
	MMINLINE uintptr_t getConcurrentGCCount()	{	return _countOf.concurrentGC;	};

	MMINLINE void incrementSystemGCCount()		{	_countOf.systemGC++;	};
	MMINLINE void incrementNurseryAFCount()		{	_countOf.nurseryAF++;	};
	MMINLINE void incrementTenureAFCount()		{	_countOf.tenureAF++;	};
	MMINLINE void incrementConcurrentGCCount()	{	_countOf.concurrentGC++;	};

#if defined(OMR_GC_REALTIME)

	/* The timestamp of the last of any of SyncGC/Heartbeat/Trigger events */
	MMINLINE uint64_t getLastMetronomeTime(void) { return OMR_MAX(OMR_MAX(_timeOfLast.metronomeSynchGC, _timeOfLast.metronomeHeartbeat),
		OMR_MAX(_timeOfLast.metronomeTriggerStart, _timeOfLast.metronomeTriggerEnd)); }

	MMINLINE uint64_t getLastMetronomeSynchGCTime() { return _timeOfLast.metronomeSynchGC; }
	MMINLINE void setLastMetronomeSynchGCTime(uint64_t time) { _timeOfLast.metronomeSynchGC = time; }
	MMINLINE uintptr_t getMetronomeSynchGCCount() { return _countOf.metronomeSynchGC; }
	MMINLINE void incrementMetronomeSynchGCCount() { _countOf.metronomeSynchGC += 1; }

	MMINLINE uint64_t getLastMetronomeHeartbeatTime() { return _timeOfLast.metronomeHeartbeat; }
	MMINLINE void setLastMetronomeHeartbeatTime(uint64_t time) { _timeOfLast.metronomeHeartbeat = time; }
	MMINLINE uintptr_t getMetronomeHeartbeatCount() { return _countOf.metronomeHeartbeat; }
	MMINLINE void incrementMetronomeHeartbeatCount() { _countOf.metronomeHeartbeat += 1; }

	MMINLINE uint64_t getLastMetronomeThreadPriorityChangeTime() { return _timeOfLast.metronomeThreadPriorityChange; }
	MMINLINE void setLastMetronomeThreadPriorityChangeTime(uint64_t time) { _timeOfLast.metronomeThreadPriorityChange = time; }
	MMINLINE uintptr_t getMetronomeThreadPriorityChangeCount() { return _countOf.metronomeThreadPriorityChange; }
	MMINLINE void incrementMetronomeThreadPriorityChangeCount() { _countOf.metronomeThreadPriorityChange += 1; }

	MMINLINE uint64_t getLastMetronomeTriggerStartTime(void) { return _timeOfLast.metronomeTriggerStart; }
	MMINLINE void setLastMetronomeTriggerStartTime(uint64_t time) { _timeOfLast.metronomeTriggerStart = time; }
	MMINLINE uint64_t getLastMetronomeTriggerEndTime(void) { return _timeOfLast.metronomeTriggerEnd; }
	MMINLINE void setLastMetronomeTriggerEndTime(uint64_t time) { _timeOfLast.metronomeTriggerEnd = time; }
	MMINLINE uintptr_t getMetronomeTriggerCount(void) { return _countOf.metronomeTrigger; }
	MMINLINE void incrementMetronomeTriggerCount(void) { _countOf.metronomeTrigger += 1; }

	MMINLINE uint64_t getLastMetronomeCycleStartTime(void) { return _timeOfLast.metronomeCycleStart; }
	MMINLINE void setLastMetronomeCycleStartTime(uint64_t time) { _timeOfLast.metronomeCycleStart = time; }
	MMINLINE uint64_t getLastMetronomeCycleEndTime(void) { return _timeOfLast.metronomeCycleEnd; }
	MMINLINE void setLastMetronomeCycleEndTime(uint64_t time) { _timeOfLast.metronomeCycleEnd = time; }
	MMINLINE uintptr_t getMetronomeCycleCount(void) { return _countOf.metronomeCycle; }
	MMINLINE void incrementMetronomeCycleCount(void) { _countOf.metronomeCycle += 1; }
#endif /* OMR_GC_REALTIME */

	MMINLINE void incrementIndent() { _indentationLevel++; }
	MMINLINE void decrementIndent() { _indentationLevel--; }
	
	MMINLINE uintptr_t getIndentLevel() { return _indentationLevel; }

	MMINLINE uintptr_t getIdAndIncrement() { return MM_AtomicOperations::add(&_curId, 1); }

	/* Interface for Dynamic Configuration */
	virtual bool configureVerboseGC(OMR_VM *vm, char* filename, uintptr_t fileCount, uintptr_t iterations) = 0;

	/**
	 * Determine the number of currently active output mechanisms.
	 * @return a count of the number of active output mechanisms.
	 */
	virtual uintptr_t countActiveOutputHandlers() = 0;

	virtual void enableVerboseGC() = 0;
	virtual void disableVerboseGC() = 0;

	virtual void kill(MM_EnvironmentBase *env) = 0;

	/**
	 * Close all output mechanisms on the receiver.
	 * @param env vm thread.
	 */
	virtual void closeStreams(MM_EnvironmentBase *env) = 0;

	uint64_t getLastOutputTime() { return _lastOutputTime; }
	void setLastOutputTime(uint64_t time) {  _lastOutputTime = time; }

	uintptr_t getOutputCount() { return _outputCount; }
	void incrementOutputCount() {  _outputCount++; }

	J9HookInterface** getPrivateHookInterface(){ return _mmPrivateHooks; }
	J9HookInterface** getOMRHookInterface(){ return _omrHooks; }

	MM_VerboseManagerBase(OMR_VM *omrVM)
		: MM_BaseVirtual()
		, _omrVM(omrVM)
		, _indentationLevel(0)
		, _hooksAttached(false)
		, _mmPrivateHooks(NULL)
		, _omrHooks(NULL)
		, _outputCount(0)
		, _curId(0)
	{
		OMRPORT_ACCESS_FROM_OMRVM(omrVM);

		/* Initialize structures */
		_timeOfLast.initialized = 0;
		_timeOfLast.systemGC = 0;
		_timeOfLast.allocationFailure = 0;
		_timeOfLast.nurseryAF = 0;
		_timeOfLast.tenureAF = 0;
		_timeOfLast.globalGC = 0;
		_timeOfLast.concurrentGC = 0;
		_timeOfLast.localGC = 0;
		_timeOfLast.partialGC = 0;
		_timeOfLast.globalMarkGC = 0;
		_timeOfLast.balancedGlobalGC = 0;
		_timeOfLast.taxationEntryPoint = 0;
		_timeOfLast.exclusiveAccessStart = 0;
		_timeOfLast.exclusiveAccessEnd = 0;
#if defined(OMR_GC_REALTIME)
		_timeOfLast.metronomeSynchGC = 0;
		_timeOfLast.metronomeHeartbeat = 0;
		_timeOfLast.metronomeThreadPriorityChange = 0;
		_timeOfLast.metronomeTriggerStart = 0;
		_timeOfLast.metronomeTriggerEnd = 0;
		_timeOfLast.metronomeCycleStart = 0;
		_timeOfLast.metronomeCycleEnd = 0;
#endif /* OMR_GC_REALTIME */
		_timeOfLast.tarokIncrementStart = 0;
		_timeOfLast.tarokIncrementEnd = 0;

		_countOf.systemGC = 0;
		_countOf.nurseryAF = 0;
		_countOf.tenureAF = 0;
		_countOf.concurrentGC = 0;
#if defined(OMR_GC_REALTIME)
		_countOf.metronomeSynchGC = 0;
		_countOf.metronomeHeartbeat = 0;
		_countOf.metronomeThreadPriorityChange = 0;
		_countOf.metronomeTrigger = 0;
		_countOf.metronomeCycle = 0;
#endif /* OMR_GC_REALTIME */

		_startTime = omrtime_hires_clock();
	}
};

#endif /* VERBOSEMANAGERBASE_HPP_ */
