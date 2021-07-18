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

#if !defined(CONCURRENTGCINCREMENTALUPDATE_HPP_)
#define CONCURRENTGCINCREMENTALUPDATE_HPP_

#include "omrcfg.h"
#include "modronopt.h"

#include "omr.h"
#include "OMR_VM.hpp"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#include "ConcurrentGC.hpp"
#include "ConcurrentCardTable.hpp"

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
#define CARD_CLEANING_HISTORY_WEIGHT ((float)0.7)
#define BYTES_TRACED_IN_PASS_1_HISTORY_WEIGHT ((float)0.8)
#define ALL_BYTES_TRACED_IN_PASS_1 ((float)1.0)
/**
 * @}
 */

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentGCIncrementalUpdate : public MM_ConcurrentGC
{
	/*
	 * Data members
	 */
private:
	MM_ConcurrentCardTable *_cardTable;	/**< pointer to Cards Table */

	/* Concurrent card cleaning statistics */
	float _cardCleaningFactorPass1;
	float _cardCleaningFactorPass2;
	float _maxCardCleaningFactorPass1;
	float _maxCardCleaningFactorPass2;
	float _cardCleaningThresholdFactor;
	float _bytesTracedInPass1Factor;

	uintptr_t _bytesToCleanPass1;
	uintptr_t _bytesToCleanPass2;
	uintptr_t _bytesToTracePass1;
	uintptr_t _bytesToTracePass2;
	uintptr_t _allocToTraceRateCardCleanPass2Boost;
	uintptr_t _totalTracedAtPass2KO;
	uintptr_t _totalCleanedAtPass2KO;
	uintptr_t _traceTargetPass1;
	uintptr_t _traceTargetPass2;

	bool _pass2Started;
	bool  _secondCardCleanPass;

public:
	/**
	 * Return reference to Card Table
	 */
	MMINLINE MM_ConcurrentCardTable *getCardTable()
	{
		return _cardTable;
	}
	
	/*
	 * Function members
	 */
private:
	/**
	 * Creates Concurrent Card Table
	 * @param env current thread environment
	 * @return true if table is created
	 */
	bool createCardTable(MM_EnvironmentBase *env);
	static void hookCardCleanPass2Start(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	void recordCardCleanPass2Start(MM_EnvironmentBase *env);

protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	void reportConcurrentFinalCardCleaningStart(MM_EnvironmentBase *env);
	void reportConcurrentFinalCardCleaningEnd(MM_EnvironmentBase *env, uint64_t duration);

	virtual void reportConcurrentCollectionStart(MM_EnvironmentBase *env);
	virtual void reportConcurrentHalted(MM_EnvironmentBase *env);

	void kickoffCardCleaning(MM_EnvironmentBase *env, ConcurrentCardCleaningReason reason);
	virtual	bool cleanCards(MM_EnvironmentBase *env, bool isMutator, uintptr_t sizeToDo, uintptr_t  *sizeDone, bool threadAtSafePoint);

	virtual void tuneToHeap(MM_EnvironmentBase *env);
	virtual void updateTuningStatisticsInternal(MM_EnvironmentBase *env);
	virtual void resetConcurrentParameters(MM_EnvironmentBase *env);
	virtual void adjustTraceTarget();

	virtual void setupForConcurrent(MM_EnvironmentBase *env);
	virtual void finalConcurrentPrecollect(MM_EnvironmentBase *env);
	virtual void internalPostCollect(MM_EnvironmentBase *env, MM_MemorySubSpace *subSpace);
	virtual void completeConcurrentTracing(MM_EnvironmentBase *env, uintptr_t executionModeAtGC);

	virtual uintptr_t doConcurrentTrace(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t sizeToTrace, MM_MemorySubSpace *subspace, bool tlhAllocation);
	virtual uintptr_t localMark(MM_EnvironmentBase *env, uintptr_t sizeToTrace);
	virtual void conHelperDoWorkInternal(MM_EnvironmentBase *env, ConHelperRequest *request, MM_SpinLimiter *spinLimiter, uintptr_t *totalScanned);

	virtual uint32_t numberOfInitRanages(MM_MemorySubSpace *subspace);
	virtual void determineInitWorkInternal(MM_EnvironmentBase *env, uint32_t initIndex);
	virtual void initalizeConcurrentStructures(MM_EnvironmentBase *env);
	virtual uintptr_t doConcurrentInitializationInternal(MM_EnvironmentBase *env, uintptr_t initToDo);

	virtual bool contractInternalConcurrentStructures(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	virtual bool canSkipObjectRSScan(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);

	virtual void postConcurrentUpdateStatsAndReport(MM_EnvironmentBase *env, MM_ConcurrentPhaseStatsBase *stats = NULL, UDATA bytesConcurrentlyScanned = 0);

	virtual uintptr_t getTraceTarget() {
		if (_pass2Started) {
			return (_traceTargetPass1 + _traceTargetPass2);
		} else {
			return _traceTargetPass1;
		}
	}

	MMINLINE virtual uintptr_t getMutatorTotalTraced() {
		return (_stats.getTraceSizeCount() + _stats.getCardCleanCount());
	}

	MMINLINE virtual uintptr_t getConHelperTotalTraced() {
		return (_stats.getConHelperTraceSizeCount() + _stats.getConHelperCardCleanCount());
	}

	MMINLINE virtual uintptr_t workCompleted() {
		return (getMutatorTotalTraced() + getConHelperTotalTraced());
	}

public:
	virtual uintptr_t getVMStateID() { return OMRVMSTATE_GC_COLLECTOR_CONCURRENTGC; };
	virtual bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
#if defined(OMR_GC_MODRON_SCAVENGER)
	virtual void oldToOldReferenceCreated(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);
#endif /* OMR_GC_MODRON_SCAVENGER */
	void finalCleanCards(MM_EnvironmentBase *env);

	static MM_ConcurrentGCIncrementalUpdate *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	MM_ConcurrentGCIncrementalUpdate(MM_EnvironmentBase *env)
		: MM_ConcurrentGC(env)
		,_cardTable(NULL)
		,_bytesToCleanPass1(0)
		,_bytesToCleanPass2(0)
		,_bytesToTracePass1(0)
		,_bytesToTracePass2(0)
		{
			_typeId = __FUNCTION__;
		}

	friend class MM_ConcurrentCardTableForWC;
};

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

#endif /* CONCURRENTGCINCREMENTALUPDATE_HPP_ */
