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

#include "HeapResizeStats.hpp"

uint32_t
MM_HeapResizeStats::calculateGCPercentage() 
{
	uint32_t percentage = 0;
	uint64_t totalGCTicks = 0;
	uint64_t totalNonGCTicks = 0;

	/* Make sure histories has not been cleared in last 3 cycles. This will
	 * be the case if the first element in the array is zero
	 */
	if (_ticksOutsideGC[0] == 0 ) {
		return 0; 
	}
	
	/* Sum up all ticks */
	for (int i = 0; i < RATIO_RESIZE_HISTORIES; i++) {
		totalGCTicks += _ticksInGC[i];
		totalNonGCTicks += _ticksOutsideGC[i];
	}

	/* Ignore oldest history for time outside of gc */
	totalNonGCTicks -= _ticksOutsideGC[0];

	/* Add latest history for time outside of gc */
	totalNonGCTicks += _lastTimeOutsideGC;

	/* ..and calculate percentage of time being spent in GC without using floats */
	percentage = (uint32_t)((totalGCTicks * 100) / (totalGCTicks + totalNonGCTicks));

	/* Remember the answer; may be need by verbose */
	_lastGCPercentage = percentage;
	
	return percentage;
}

void
MM_HeapResizeStats::updateHeapResizeStats()
{
	/* 
	 * Calculate ratio statistics provided we have had
	 * at least one preceding AF ... 
	 */
	if (_lastAFEndTime) {
		/* CMVC 125876:  Note that time can go backward (core-swap, for example) so store a 1 as the delta if this
		 * is happening since we can't use 0 time deltas and negative deltas can cause arithmetic exceptions
		 */
		uint64_t timeInGC = 1;
		/* cache the ivars so that they aren't changed concurrently (not sure if this is a threat but better safe than sorry) */
		uint64_t cacheThisAFStartTime = _thisAFStartTime;
		uint64_t cacheLastAFEndTime = _lastAFEndTime;
		/* make sure that we won't underflow */
		if (cacheThisAFStartTime < cacheLastAFEndTime) {
			timeInGC = cacheLastAFEndTime - cacheThisAFStartTime;
		}
		uint64_t timeOutsideGC = _lastTimeOutsideGC;
		
		/* Clocks are not always accurate on embedded systems.  Subsequent calls
	 	 * may return the same value (i.e. delta of 0). If this happens round the 
	 	 * delta up to 1 tick so we never store a 0 into ratio ticks array.
	 	*/
	 	timeInGC = (timeInGC == 0 ? 1 : timeInGC);
		timeOutsideGC = (timeOutsideGC == 0 ? 1 : timeOutsideGC);
		
		/* Update the last entry in ratio resize history array*/
		updateRatioTicks(timeInGC, timeOutsideGC);
	}			
}	
