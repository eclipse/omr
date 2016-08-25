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

#if !defined(CONCURRENTGCSTATS_HPP_)
#define CONCURRENTGCSTATS_HPP_

#include "omrcomp.h"
#include "omrport.h"
#include "modronbase.h"
#include "omr.h"

#include "AtomicOperations.hpp"
#include "Base.hpp"

class MM_EnvironmentBase;

/*
 * @todo Provide class documentation
 * @ingroup GC_Stats
 */
class MM_ConcurrentGCStats : public MM_Base 
{
	volatile uintptr_t _executionMode;
	uintptr_t _executionModeAtGC;
	
	uintptr_t _initWorkRequired;
	uintptr_t _traceSizeTarget;
	uintptr_t _kickoffThreshold;
	uintptr_t _cardCleaningThreshold;
	uintptr_t _remainingFree;
	
	uintptr_t _allocationsTaxed;
	uintptr_t _allocationsTaxedAt0;
	uintptr_t _allocationsTaxedAt25;
	uintptr_t _allocationsTaxedAt50;
	uintptr_t _allocationsTaxedAt75;
	uintptr_t _allocationsTaxedAt100;
	
	/* The following statistics are reset at the end of every concurrent cycle */
	volatile uintptr_t _traceSizeCount;
	volatile uintptr_t _cardCleanCount;
	volatile uintptr_t _conHelperTraceSizeCount;
	volatile uintptr_t _conHelperCardCleanCount;
	volatile uintptr_t _completeTracingCount;
	volatile uintptr_t _finalTraceCount;
	volatile uintptr_t _finalCardCleanCount;
	volatile uintptr_t _RSScanTraceCount;
	volatile uintptr_t _RSObjectsFound;
	volatile uintptr_t _threadsScannedCount;
	uintptr_t _threadsToScanCount;
	
	bool _concurrentWorkStackOverflowOcurred;
	uintptr_t _concurrentWorkStackOverflowCount;
	
	volatile uint32_t _completedModes; /**< a bit mask of modes which have been completed in the current cycle */
	
	ConcurrentKickoffReason _kickoffReason; /**< a constant indicating why kickoff occured */
	ConcurrentCardCleaningReason _cardCleaningReason; /**< a constant indicating why card cleaning was kicked off */
	
public:
	static const char* getConcurrentStatusString(MM_EnvironmentBase *env, uintptr_t status, char *statusBuffer, uintptr_t statusBufferLength);
	MMINLINE uintptr_t  getExecutionMode() { return _executionMode; };

	MMINLINE bool concurrentMarkNotStarted() { return (_executionMode == CONCURRENT_OFF); }
	MMINLINE bool concurrentMarkInProgress() { return (_executionMode > CONCURRENT_OFF); }
	
	MMINLINE bool switchExecutionMode(uintptr_t oldMode, uintptr_t newMode)
	{
		return oldMode == MM_AtomicOperations::lockCompareExchange(&_executionMode, oldMode, newMode);
	}
	
	MMINLINE uintptr_t  getExecutionModeAtGC() { return _executionModeAtGC; };
	MMINLINE void  setExecutionModeAtGC(uintptr_t executionMode) { _executionModeAtGC = executionMode; };
	
	/* Getter and setter methods for concurrent statistics */
	MMINLINE uintptr_t getKickoffThreshold() { return _kickoffThreshold; };
	MMINLINE void  setKickoffThreshold(uintptr_t threshold ){ _kickoffThreshold= threshold; };

	MMINLINE ConcurrentKickoffReason getKickoffReason() { return _kickoffReason; }
	MMINLINE void  clearKickoffReason(){ _kickoffReason = NO_KICKOFF_REASON; }
	
	MMINLINE void  setKickoffReason(ConcurrentKickoffReason reason)
	{
		if (NO_KICKOFF_REASON == _kickoffReason) {
			_kickoffReason = reason;
		}
	}
	
	MMINLINE uintptr_t getInitWorkRequired() { return _initWorkRequired; };
	MMINLINE void  setInitWorkRequired(uintptr_t required )	{ _initWorkRequired = required; };
	MMINLINE uintptr_t getTraceSizeTarget() { return _traceSizeTarget; };
	MMINLINE void  setTraceSizeTarget(uintptr_t target ){ _traceSizeTarget = target; };
	
	MMINLINE void  setRemainingFree(uintptr_t free) { _remainingFree = free; };
	MMINLINE uintptr_t getRemainingFree() { return _remainingFree; };
	
	MMINLINE void  clearAllocationTaxCounts()				
	{
		_allocationsTaxed = 0;
		_allocationsTaxedAt0 = 0;
		_allocationsTaxedAt25 = 0;
		_allocationsTaxedAt50 = 0;
		_allocationsTaxedAt75 = 0;
		_allocationsTaxedAt100 = 0;
	}	
	
	MMINLINE void  analyzeAllocationTax(uintptr_t taxDue, uintptr_t taxPaid)
	{
		_allocationsTaxed++;
		
		if (taxPaid == 0) {
			_allocationsTaxedAt0++;
		} else if (taxPaid <= (uintptr_t)((double)taxDue * 0.25)) {
			_allocationsTaxedAt25++;
		} else if (taxPaid <= (uintptr_t)((double)taxDue * 0.5)) {	
			_allocationsTaxedAt50++;
		} else if (taxPaid <= (uintptr_t)((double)taxDue * 0.75)) {
			_allocationsTaxedAt75++;
		} else {	
			_allocationsTaxedAt100++;
		}
	}	
	
	MMINLINE void  printAllocationTaxReport(OMR_VM *omrVM)
	{
		OMRPORT_ACCESS_FROM_OMRVM(omrVM);
		
		omrtty_printf("Concurrent mark analysis: Total Allocations: %zu Tax Paid 0%%: %zu 25%%: %zu 50%%: %zu  75%%: %zu 100%%+: %zu\n", 
						_allocationsTaxed,
						_allocationsTaxedAt0,
						_allocationsTaxedAt25,
						_allocationsTaxedAt50,
						_allocationsTaxedAt75,
						_allocationsTaxedAt100
					);	
		
	}
	
	MMINLINE uintptr_t getTraceSizeCount() { return (uintptr_t) _traceSizeCount; };
	MMINLINE uintptr_t getCardCleanCount() { return (uintptr_t) _cardCleanCount; };
	MMINLINE uintptr_t getConHelperTraceSizeCount()	{ return (uintptr_t) _conHelperTraceSizeCount; };
	MMINLINE uintptr_t getConHelperCardCleanCount() { return (uintptr_t) _conHelperCardCleanCount; };
	MMINLINE uintptr_t getFinalTraceCount() { return (uintptr_t) _finalTraceCount; };
	MMINLINE uintptr_t getCompleteTracingCount(){ return (uintptr_t) _completeTracingCount; };
	MMINLINE uintptr_t getFinalCardCleanCount() { return (uintptr_t) _finalCardCleanCount; };
	MMINLINE uintptr_t getRSScanTraceCount() { return (uintptr_t) _RSScanTraceCount; };
	MMINLINE uintptr_t getRSObjectsFound(){ return (uintptr_t) _RSObjectsFound; };
	
	MMINLINE uintptr_t getCardCleaningThreshold() { return _cardCleaningThreshold; };
	MMINLINE void  setCardCleaningThreshold(uintptr_t threshold) { _cardCleaningThreshold= threshold; };
	MMINLINE uintptr_t getTotalTraced() { return _conHelperTraceSizeCount + _conHelperCardCleanCount + _traceSizeCount + _cardCleanCount; };
	MMINLINE uintptr_t getMutatorsTraced() { return _traceSizeCount + _cardCleanCount; };
	MMINLINE uintptr_t getConHelperTraced() { return _conHelperTraceSizeCount + _conHelperCardCleanCount; };
	
	MMINLINE bool getConcurrentWorkStackOverflowOcurred() { return _concurrentWorkStackOverflowOcurred; };
	MMINLINE void setConcurrentWorkStackOverflowOcurred(bool overflow){ _concurrentWorkStackOverflowOcurred = overflow; };
	MMINLINE uintptr_t getConcurrentWorkStackOverflowCount() { return _concurrentWorkStackOverflowCount; };
		
	/* Functions required to perform atomic updates of concurrent statistics */
	MMINLINE void incrementCount(uintptr_t *count, uintptr_t increment) 
	{
		MM_AtomicOperations::add(count, increment);
	};
	
	MMINLINE void clearCount(uintptr_t *count) 
	{
		MM_AtomicOperations::set(count, 0);
	};
	
	MMINLINE void incTraceSizeCount(uintptr_t increment) { incrementCount((uintptr_t *)&_traceSizeCount, increment); };
	MMINLINE void incConHelperTraceSizeCount(uintptr_t increment) { incrementCount((uintptr_t *)&_conHelperTraceSizeCount, increment); };
	MMINLINE void incCardCleanCount(uintptr_t increment){ incrementCount((uintptr_t *)&_cardCleanCount, increment); };
	MMINLINE void incConHelperCardCleanCount(uintptr_t increment){ incrementCount((uintptr_t *)&_conHelperCardCleanCount,increment); };	
	MMINLINE void incCompleteTracingCount(uintptr_t increment) { incrementCount((uintptr_t *)&_completeTracingCount, increment); };
	MMINLINE void incFinalTraceCount(uintptr_t increment) { incrementCount((uintptr_t *)&_finalTraceCount, increment); };
	MMINLINE void incFinalCardCleanCount(uintptr_t increment){ incrementCount((uintptr_t *)&_finalCardCleanCount, increment); };
	MMINLINE void incRSScanTraceCount(uintptr_t increment) { incrementCount((uintptr_t *)&_RSScanTraceCount, increment); };	
	MMINLINE void incRSObjectsFound(uintptr_t increment) { incrementCount((uintptr_t *)&_RSObjectsFound, increment); };
	MMINLINE void incConcurrentWorkStackOverflowCount() { incrementCount((uintptr_t *)&_concurrentWorkStackOverflowCount, 1); }; 
	
	MMINLINE void setThreadsToScanCount(uintptr_t count) { _threadsToScanCount = count; };
	MMINLINE uintptr_t getThreadsToScanCount() { return _threadsToScanCount; };
	MMINLINE void incThreadsScannedCount() { incrementCount((uintptr_t*)&_threadsScannedCount, 1); };
	MMINLINE uintptr_t getThreadsScannedCount() { return _threadsScannedCount; };
	
	MMINLINE bool isRootTracingComplete() { return (_completedModes & CONCURRENT_ROOT_TRACING) == CONCURRENT_ROOT_TRACING; };
	MMINLINE void setModeComplete(ConcurrentStatus mode) {
		uint32_t mask = (uint32_t)1 << mode;
		uint32_t currentValue = _completedModes;
		do {
			currentValue = MM_AtomicOperations::lockCompareExchangeU32(&_completedModes, currentValue, currentValue | mask);
		} while ( (currentValue & mask) != mask);
	}
	
	MMINLINE void setCardCleaningReason(ConcurrentCardCleaningReason reason) { _cardCleaningReason = reason; };
	MMINLINE ConcurrentCardCleaningReason getCardCleaningReason() { return _cardCleaningReason; };
	
	MMINLINE void reset()
	{
		clearCount((uintptr_t *)&_traceSizeCount);
		clearCount((uintptr_t *)&_conHelperTraceSizeCount);
		clearCount((uintptr_t *)&_cardCleanCount);
		clearCount((uintptr_t *)&_conHelperCardCleanCount);
		clearCount((uintptr_t *)&_completeTracingCount);
		clearCount((uintptr_t *)&_finalTraceCount);
		clearCount((uintptr_t *)&_finalCardCleanCount);
		clearCount((uintptr_t *)&_RSScanTraceCount);
		clearCount((uintptr_t *)&_RSObjectsFound);
		clearCount((uintptr_t *)&_threadsScannedCount);
		clearCount(&_threadsToScanCount);
		_completedModes = 0;
		_cardCleaningReason = CARD_CLEANING_REASON_NONE;
	};
	
	/**
	 * Create a ConcurrentGCStats object.
	 */   
	MM_ConcurrentGCStats() :
		MM_Base(),
		_executionMode((uintptr_t)CONCURRENT_OFF),
		_executionModeAtGC(CONCURRENT_OFF),
		_initWorkRequired(0),
		_traceSizeTarget(0),
		_kickoffThreshold(0),
		_cardCleaningThreshold(0),
		_remainingFree(0),
		_allocationsTaxed(0),
		_allocationsTaxedAt0(0),
		_allocationsTaxedAt25(0),
		_allocationsTaxedAt50(0),
		_allocationsTaxedAt75(0),
		_allocationsTaxedAt100(0),
		_traceSizeCount(0),
		_cardCleanCount(0),
		_conHelperTraceSizeCount(0),
		_conHelperCardCleanCount(0),
		_completeTracingCount(0),
		_finalTraceCount(0),
		_finalCardCleanCount(0),
		_RSScanTraceCount(0),
		_RSObjectsFound(0),
		_threadsScannedCount(0),
		_threadsToScanCount(0),
		_concurrentWorkStackOverflowOcurred(false),
		_concurrentWorkStackOverflowCount(0),
		_completedModes(0),
		_kickoffReason(NO_KICKOFF_REASON),
		_cardCleaningReason(CARD_CLEANING_REASON_NONE)
	{}

};

#endif /* CONCURRENTGCSTATS_HPP_ */
