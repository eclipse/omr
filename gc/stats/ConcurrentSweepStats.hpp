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

#if !defined(CONCURRENTSWEEPSTATS_HPP_)
#define CONCURRENTSWEEPSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#if defined(OMR_GC_CONCURRENT_SWEEP)

#include "Base.hpp"
#include "Debug.hpp"

enum {
	concurrentsweep_mode_off = 0,
	concurrentsweep_mode_stw_find_minimum_free_size,
	concurrentsweep_mode_on,
	concurrentsweep_mode_completing_sweep_phase_concurrently,
	concurrentsweep_mode_completed_sweep_phase_concurrently,
	concurrentsweep_mode_stw_complete_sweep
};

/**
 * Statistics for concurrent sweep operations.
 */
class MM_ConcurrentSweepStats : public MM_Base
{
private:
protected:
public:
	/**
	 * General concurrent statistics.
	 * @{
	 */
	volatile uintptr_t _mode;  /**< Concurrent sweep phase of operation */

	uintptr_t _totalChunkCount;  /**< Total number of chunks included in the concurrent sweep calculation */
	volatile uintptr_t _totalChunkSweptCount;  /**< Total number of chunks that have been swept through concurrent sweep */
	/**
	 * @}
	 */

	/**
	 * STW Find minimum free sized entry sweep statistics.
	 * @{
	 */
	uintptr_t _minimumFreeEntryBytesSwept;  /**< Total bytes swept in heap while finding minimum free sized entry */
	uintptr_t _minimumFreeEntryBytesConnected;  /**< Total bytes connected in heap while finding minimum free sized entry */
	/**
	 * @}
	 */

	/**
	 * Concurrent completion of sweep phase statistics.
	 * @{
	 */
	uint64_t _concurrentCompleteSweepTimeStart;  /**< Start timestamp for concurrent completion of sweep phase */
	uint64_t _concurrentCompleteSweepTimeEnd;  /**< End timestamp for concurrent completion of sweep phase */
	volatile uintptr_t _concurrentCompleteSweepBytesSwept;  /**< Bytes swept during the concurrent completion of sweep phase */
	/**
	 * @}
	 */

	/**
	 * STW completion of concurrent sweep statistics.
	 * @{
	 */
	uint64_t _completeSweepPhaseTimeStart;  /**< Start timestamp for completion of sweep phase */
	uint64_t _completeSweepPhaseTimeEnd;  /**< End timestamp for completion of sweep phase */
	volatile uintptr_t _completeSweepPhaseBytesSwept;  /**< Bytes swept during the completion of sweep phase */
	uint64_t _completeConnectPhaseTimeStart;  /**< Start timestamp for completion of connect phase */
	uint64_t _completeConnectPhaseTimeEnd;  /**< End timestamp for completion of connect phase */
	uintptr_t _completeConnectPhaseBytesConnected;  /**< Bytes connected during the completion of connect phase */
	/**
	 * @}
	 */

	/**
	 * Force the concurrent sweep mode into a particular state.
	 * @note This routine should only be used for initialization or clearing.
	 * @sa switchMode(uintptr_t, uintptr_t)
	 */
	MMINLINE void setMode(uintptr_t mode) {
		_mode = mode;
	}

	/**
	 * Move the concurrent sweep mode from one type to another.
	 */
	MMINLINE void switchMode(uintptr_t oldMode, uintptr_t newMode) {
		assume0(_mode == oldMode);
		_mode = newMode;
	}

	/**
	 * Determine whether concurrent sweeping activity is possible this round.
	 * @return true if concurrent sweep is active, false otherwise.
	 */
	MMINLINE bool isConcurrentSweepActive() { return concurrentsweep_mode_off != _mode; }

	/**
	 * Determine whether a concurrent completion of the sweep phase can be started or is already in progress.
	 * @return true if the work can or has been started, false if it cannot or has been completed.
	 */
	MMINLINE bool canCompleteSweepConcurrently() { return (concurrentsweep_mode_on <= _mode) && (concurrentsweep_mode_completed_sweep_phase_concurrently > _mode); }

	/**
	 * Determine if a concurrent completion of the sweep phase is in progress.
	 * @return true if the work is in progress, false otherwise.
	 */
	MMINLINE bool isCompletingSweepConcurrently() { return concurrentsweep_mode_completing_sweep_phase_concurrently == _mode; }

	/**
	 * Determine whether the concurrent completion of the sweep phase is done.
	 * @return true if the work has completed, false otherwise.
	 */
	MMINLINE bool hasCompletedSweepConcurrently() { return concurrentsweep_mode_completed_sweep_phase_concurrently == _mode; }

	/**
	 * Reset all statistics to starting values.
	 */
	MMINLINE void clear() {
		_totalChunkCount = 0;
		_totalChunkSweptCount = 0;
		_minimumFreeEntryBytesSwept = 0;
		_minimumFreeEntryBytesConnected = 0;
		_concurrentCompleteSweepTimeStart = 0;
		_concurrentCompleteSweepTimeEnd = 0;
		_concurrentCompleteSweepBytesSwept = 0;
		_completeSweepPhaseTimeStart = 0;
		_completeSweepPhaseTimeEnd = 0;
		_completeSweepPhaseBytesSwept = 0;
		_completeConnectPhaseTimeStart = 0;
		_completeConnectPhaseTimeEnd = 0;
		_completeConnectPhaseBytesConnected = 0;
	}

	MM_ConcurrentSweepStats() :
		MM_Base(),
		_mode(concurrentsweep_mode_off),
		_totalChunkCount(0),
		_totalChunkSweptCount(0),
		_minimumFreeEntryBytesSwept(0),
		_minimumFreeEntryBytesConnected(0),
		_concurrentCompleteSweepTimeStart(0),
		_concurrentCompleteSweepTimeEnd(0),
		_concurrentCompleteSweepBytesSwept(0),
		_completeSweepPhaseTimeStart(0),
		_completeSweepPhaseTimeEnd(0),
		_completeSweepPhaseBytesSwept(0),
		_completeConnectPhaseTimeStart(0),
		_completeConnectPhaseTimeEnd(0),
		_completeConnectPhaseBytesConnected(0)
	{}
};

#endif /* OMR_GC_CONCURRENT_SWEEP */

#endif /* CONCURRENTSWEEPSTATS_HPP_ */
