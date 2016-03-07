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

#if !defined(HEAPRESIZESTATS_HPP_)
#define HEAPRESIZESTATS_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "Base.hpp"
#include "Debug.hpp"

#define RATIO_RESIZE_HISTORIES				3

/**
 * @todo Provide class documentation
 * @ingroup GC_Stats
 */
class MM_HeapResizeStats : public MM_Base
{
	/*
	 * Data members
	 */
private:
	uint64_t 				_lastAFEndTime;
	uint64_t				_thisAFStartTime;

	uintptr_t 				_freeBytesAtSystemGCStart;

	/* Remember gc count on last expansion or contraction of heap */
	uintptr_t				_lastHeapExpansionGCCount;
	uintptr_t 				_lastHeapContractionGCCount;

	/* Remember actual size of last expansion/contraction */
	uintptr_t				_lastActualHeapExpansionSize;
	uintptr_t 				_lastActualHeapContractionSize;

	/* Remember reason for last expansion or contraction. Perists until next
	 * contraction/expansion */
	ExpandReason		_lastExpandReason;
	ContractReason 		_lastContractReason;
	LoaResizeReason		_lastLoaResizeReason;

	uint64_t				_lastExpandTime; /**< time in hi-res ticks of the last expansion */
	uint64_t				_lastContractTime; /**< time in hi-res ticks of the last expansion */
	uint32_t				_lastGCPercentage;
	
	uint64_t				_lastTimeOutsideGC;
	uintptr_t				_globalGCCountAtAF;

	uint64_t 				_ticksInGC[RATIO_RESIZE_HISTORIES];
	uint64_t 				_ticksOutsideGC[RATIO_RESIZE_HISTORIES];

protected:
public:

	/*
	 * Function members
	 */
private:
protected:
public:

	uint32_t	calculateGCPercentage();

	void	updateHeapResizeStats();

	MMINLINE void 	resetRatioTicks()
	{
		for (int i = 0; i < RATIO_RESIZE_HISTORIES; i++)
		{
  			_ticksInGC[i] = 0;
  			_ticksOutsideGC[i] = 0;
		}	
	}
	
	MMINLINE void	updateRatioTicks(uint64_t timeInGC, uint64_t timeOutsideGC)	 	
	{
		/* Both time deltas must be greater than zero */
		assume0(timeInGC > 0); 
		assume0(timeOutsideGC > 0);
		
		for (int i = 0; i < RATIO_RESIZE_HISTORIES-1; i++)
		{
  			_ticksInGC[i] = _ticksInGC[i+1];
  			_ticksOutsideGC[i] = _ticksOutsideGC[i+1];
		}	
		_ticksInGC[RATIO_RESIZE_HISTORIES-1] = timeInGC;
		_ticksOutsideGC[RATIO_RESIZE_HISTORIES-1] = timeOutsideGC;	
	}
	
	MMINLINE void	setLastAFEndTime(uint64_t time) { _lastAFEndTime = time; }
	MMINLINE uint64_t	getLastAFEndTime() { return _lastAFEndTime; }
	
	MMINLINE void	setThisAFStartTime(uint64_t time) { _thisAFStartTime = time; }
	MMINLINE uint64_t	getThisAFStartTime() { return _thisAFStartTime; }
	
	MMINLINE void 	setFreeBytesAtSystemGCStart(uintptr_t freeBytes) { _freeBytesAtSystemGCStart= freeBytes; }
	MMINLINE uintptr_t	getFreeBytesAtSystemGCStart() { return _freeBytesAtSystemGCStart; }
	
	MMINLINE void	setLastHeapExpansionGCCount(uintptr_t gccount) { _lastHeapExpansionGCCount = gccount; }
	MMINLINE uintptr_t	getLastHeapExpansionGCCount() { return _lastHeapExpansionGCCount; }
	MMINLINE void	setLastHeapContractionGCCount(uintptr_t gccount) { _lastHeapContractionGCCount = gccount; }
	MMINLINE uintptr_t	getLastHeapContractionGCCount() { return _lastHeapContractionGCCount; }
	
	MMINLINE void	setLastExpandReason(ExpandReason reason) { _lastExpandReason = reason; }
	MMINLINE ExpandReason getLastExpandReason() { return _lastExpandReason; }
	MMINLINE void	setLastContractReason(ContractReason reason) { _lastContractReason = reason; }
	MMINLINE ContractReason getLastContractReason() { return _lastContractReason; }
	MMINLINE void	setLastLoaResizeReason(LoaResizeReason reason) { _lastLoaResizeReason = reason; }
	MMINLINE LoaResizeReason getLastLoaResizeReason() { return _lastLoaResizeReason; }

	MMINLINE void	setLastExpandActualSize(uintptr_t size) { _lastActualHeapExpansionSize = size; }
	MMINLINE uintptr_t getLastExpandActualSize() { return _lastActualHeapExpansionSize; }
	MMINLINE void	setLastContractActualSize(uintptr_t size) { _lastActualHeapContractionSize = size; }
	MMINLINE uintptr_t getLastContractActualSize() { return _lastActualHeapContractionSize; }

	MMINLINE void setLastExpandTime(uint64_t ticks) { _lastExpandTime = ticks; }
	MMINLINE uint64_t getLastExpandTime() { return _lastExpandTime; }
	MMINLINE void setLastContractTime(uint64_t ticks) { _lastContractTime = ticks; }
	MMINLINE uint64_t getLastContractTime() { return _lastContractTime; }
	
	MMINLINE void	setLastTimeOutsideGC()			{
		/* CMVC 125876:  Note that time can go backward (core-swap, for example) so store a 1 as the delta if this
		 * is happening since we can't use 0 time deltas and negative deltas can cause arithmetic exceptions
		 */
		/* cache the ivars so that they aren't changed concurrently (not sure if this is a threat but better safe than sorry) */
		uint64_t cacheLastAFEndTime = _lastAFEndTime;
		uint64_t cacheThisAFStartTime = _thisAFStartTime;
		if (cacheLastAFEndTime >= cacheThisAFStartTime) {
			_lastTimeOutsideGC = 1;
		} else {
			_lastTimeOutsideGC = cacheThisAFStartTime - cacheLastAFEndTime;
		}
	}
	MMINLINE uint64_t	getLastTimeOutsideGC()			{	return _lastTimeOutsideGC; }
	MMINLINE void	setGlobalGCCountAtAF(uintptr_t count)	{	_globalGCCountAtAF = count; }
	MMINLINE uintptr_t   getGlobalGCCountAtAF()			{	return _globalGCCountAtAF; }
	
	MMINLINE uint32_t	getRatioExpandPercentage()
	{
		if (_lastExpandReason == GC_RATIO_TOO_HIGH) {
			return _lastGCPercentage;
		} else {
			return 0;
		}
	}
	
	MMINLINE uint32_t	getRatioContractPercentage()
	{
		if (_lastContractReason == GC_RATIO_TOO_LOW) {
			return _lastGCPercentage;
		} else {
			return 0;
		}			
	}
	
	MM_HeapResizeStats() :
		MM_Base(),
		_lastAFEndTime(0),
		_thisAFStartTime(0),
		_freeBytesAtSystemGCStart(0),
		_lastHeapExpansionGCCount(0),
		_lastHeapContractionGCCount(0),
		_lastActualHeapExpansionSize(0),
		_lastActualHeapContractionSize(0),
		_lastExpandReason(NO_EXPAND),
		_lastContractReason(NO_CONTRACT),
		_lastLoaResizeReason(NO_LOA_RESIZE),
		_lastExpandTime(0),
		_lastContractTime(0),
		_lastGCPercentage(0),
		_lastTimeOutsideGC(0),
		_globalGCCountAtAF(0)
	{
		resetRatioTicks();
	}
	
}; 

#endif /* HEAPRESIZESTATS_HPP_ */
