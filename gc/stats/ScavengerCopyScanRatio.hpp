/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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

#ifndef SCAVENGERCOPYSCANRATIO_HPP_
#define SCAVENGERCOPYSCANRATIO_HPP_

#include <string.h>

#include "omrcfg.h"
#include "omrgcconsts.h"
#include "modronbase.h"
#include "modronopt.h"

#include "AtomicOperations.hpp"
#include "Math.hpp"
#include "ScavengerStats.hpp"

class MM_EnvironmentBase;

/**
 * Constants defining how slot wait/copy/scan and update counters are packed into a 64-bit word for atomic
 * operations involving packed wait/copy/scan counts. The update counter rolls over to zero after 32 thread
 * updates (each GC thread update adds scanned count minimally >= 512, copy count <= scanned count). This layout
 * assumes most GC threads will update (finish or alias away from scanning one object or split array segment) with
 * scan counts not much greater than 512 but will overflow if every thread persistently updates with scan counts
 * that are greater than 2*512, and samples are harvested and reset after each 32nd sample is accumulated.
 *
 * Layout (each field includes an overflow bit):
 *   0..5: update count			 :  5 + 1 bits
 *  6..21: slots scanned count	 : 15 + 1 bits
 * 22..37: slots copied count	 : 15 + 1 bits
 * 38..63: stalled threads count : 25 + 1 bits
 *
 * Invariants:
 * copied <= scanned
 * update == 1 per thread sample, <= SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE in aggregate
 */
#define SCAVENGER_SAMPLE_COUNT_BITS 6
#define SCAVENGER_SAMPLE_SLOTS_BITS 16
#define SCAVENGER_SAMPLE_WAITS_BITS (64 - (2 * SCAVENGER_SAMPLE_SLOTS_BITS) - SCAVENGER_SAMPLE_COUNT_BITS)

/**
 * GC scavenger threads update their local wait/copy/scan counters whenever they complete or alias away from a scanned
 * object. The first update that increments the thread scan count to N >= SCAVENGER_SLOTS_SCANNED_PER_THREAD_UPDATE
 * will trigger an update to MM_ScavengerCopyScanRatio. MM_ScavengerCopyScanRatio triggers a major update to
 * the aggregate wait/copy/scan counters and resets its accumulator when it accumulates exactly
 * SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE thread updates.
 *
 */
#define SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE 32
#define SCAVENGER_SLOTS_SCANNED_PER_THREAD_UPDATE 512
#define SCAVENGER_SLOTS_SCANNED_PER_MAJOR_UPDATE (SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE * SCAVENGER_SLOTS_SCANNED_PER_THREAD_UPDATE)

#define SCAVENGER_SLOTS_SCANNED_SHIFT SCAVENGER_SAMPLE_COUNT_BITS
#define SCAVENGER_SLOTS_COPIED_SHIFT (SCAVENGER_SLOTS_SCANNED_SHIFT + SCAVENGER_SAMPLE_SLOTS_BITS)
#define SCAVENGER_THREAD_WAITS_SHIFT (SCAVENGER_SLOTS_COPIED_SHIFT + SCAVENGER_SAMPLE_SLOTS_BITS)

#define SCAVENGER_SLOTS_UPDATE_MASK (((uint64_t)1 << SCAVENGER_SAMPLE_COUNT_BITS) - 1)
#define SCAVENGER_SLOTS_SCANNED_MASK ((((uint64_t)1 << SCAVENGER_SAMPLE_SLOTS_BITS) - 1) << SCAVENGER_SLOTS_SCANNED_SHIFT)
#define SCAVENGER_SLOTS_COPIED_MASK ((((uint64_t)1 << SCAVENGER_SAMPLE_SLOTS_BITS) - 1) << SCAVENGER_SLOTS_COPIED_SHIFT)
#define SCAVENGER_THREAD_WAITS_MASK ((((uint64_t)1 << SCAVENGER_SAMPLE_WAITS_BITS) - 1) << SCAVENGER_THREAD_WAITS_SHIFT)

#define SCAVENGER_UPDATE_COUNT_OVERFLOW ((uint64_t)1 << (SCAVENGER_SAMPLE_COUNT_BITS - 1))
#define SCAVENGER_SLOTS_SCANNED_OVERFLOW ((uint64_t)1 << (SCAVENGER_SLOTS_SCANNED_SHIFT + SCAVENGER_SAMPLE_SLOTS_BITS - 1))
#define SCAVENGER_SLOTS_COPIED_OVERFLOW ((uint64_t)1 << (SCAVENGER_SLOTS_COPIED_SHIFT + SCAVENGER_SAMPLE_SLOTS_BITS - 1))
#define SCAVENGER_THREAD_WAITS_OVERFLOW ((uint64_t)1 << (SCAVENGER_THREAD_WAITS_SHIFT + SCAVENGER_SAMPLE_WAITS_BITS - 1))

#define SCAVENGER_COUNTER_OVERFLOW (SCAVENGER_THREAD_WAITS_OVERFLOW | SCAVENGER_SLOTS_COPIED_OVERFLOW | SCAVENGER_SLOTS_SCANNED_OVERFLOW )

#define SCAVENGER_COUNTER_DEFAULT_ACCUMULATOR 0

#define SCAVENGER_UPDATE_HISTORY_SIZE 16

class MM_ScavengerCopyScanRatio
{
	/* Data members */
public:
	typedef struct UpdateHistory {
		uint64_t waits;		/* number of stalled treads */
		uint64_t copied;	/* number of slots copied */
		uint64_t scanned;	/* number of slots scanned */
		uint64_t updates;	/* number of thread samples */
		uint64_t threads;	/* number of active or stalled threads */
		uint64_t lists;		/* number of nonempty scan lists */
		uint64_t caches;	/* number of caches in scan queues */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		uint64_t readObjectBarrierCopy; /* number of object copied by read barrier */
		uint64_t readObjectBarrierUpdate; /* number of reference slots updates by read barrier */
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
		uint64_t time;		/* timestamp of most recent sample included in this record */
		uint64_t majorUpdates;
	} UpdateHistory;


protected:

private:
	volatile uint64_t _accumulatingSamples;		/**< accumulator for aggregating per thread wait/copy/s`wait/copy/scanes-- these are periodically latched into _accumulatedSamples and reset */
	volatile uint64_t _accumulatedSamples;		/**< most recent aggregate wait/copy/scan counts from SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE samples  */
	volatile uintptr_t _majorUpdateThreadEnv;	/**< a token for the thread that has claimed a major update and owns the critical region wherein the update is effected */
	uintptr_t _scalingUpdateCount;				/**< the number of times _accumulatingSamples was latched into _accumulatedSamples */
	uintptr_t _overflowCount;					/**< the number of times _accumulatingSamples overflowed one or more counters */
	uint64_t _resetTimestamp;					/**< timestamp at reset() */
	uintptr_t _threadCount;						/**< number of gc threads participating in current gc cycle */
	uintptr_t _historyFoldingFactor;			/** number of major updates per history record */
	uintptr_t _historyTableIndex;				/** index of history table record that will receive next major update */
	UpdateHistory _historyTable[SCAVENGER_UPDATE_HISTORY_SIZE];

	/* Function members */
public:
	/**
	 * Default constructor zeros accumulators, so initial scaling factor is 1.0.
	 */
	MM_ScavengerCopyScanRatio()
		:
		_accumulatingSamples(0)
		,_accumulatedSamples(SCAVENGER_COUNTER_DEFAULT_ACCUMULATOR)
		,_majorUpdateThreadEnv(0)
		,_scalingUpdateCount(0)
		,_overflowCount(0)
		,_resetTimestamp(0)
		,_threadCount(0)
		,_historyFoldingFactor(1)
		,_historyTableIndex(0)
	{
		memset(_historyTable, 0, SCAVENGER_UPDATE_HISTORY_SIZE * sizeof(UpdateHistory));
	}

	/**
	 * Calculate and return cache size scaling factor from accumulated wait/copy/scan updates, or unity if none
	 * received yet. Use this form for calculating scaling factor from most recent update.
	 * @return a value >0.0 and <= 1.0 that can be used to scale copy/scan cache sizes to reduce stalling
	 */
	MMINLINE double
	getScalingFactor(MM_EnvironmentBase* env)
	{
		uint64_t accumulatedSamples = MM_AtomicOperations::getU64(&_accumulatedSamples);
		return getScalingFactor(env, _threadCount, waits(accumulatedSamples), copied(accumulatedSamples), scanned(accumulatedSamples), updates(accumulatedSamples));
	}

	/**
	 * Estimate and return maximal lower bound for cache size scaling factor from accumulated wait/copy/scan
	 * updates, or 0 if none received yet. Use this form for estimating scaling factor from history records.
	 * @param[in] historyRecord historical record of accumulated wait/copy/scan updates
	 * @return a value 0.0 <= value <= 1.0 not much less than the average runtime scaling factor returned in time spanned by historyRecord
	 */
	MMINLINE double
	getScalingFactor(MM_EnvironmentBase* env, UpdateHistory *historyRecord)
	{
		uint64_t majorUpdates = historyRecord->majorUpdates;
		/* Raise thread count to ceiling to get maximal lower bound for scaling factor estimate */
		uint64_t threads = (historyRecord->threads + majorUpdates - 1) / majorUpdates;
		/* Assert: threads <= max(<gc-thread-count>) recorded in historyRecord */
		return getScalingFactor(env, threads, historyRecord->waits, historyRecord->copied, historyRecord->scanned, historyRecord->updates);
	}

	/**
	 * Accept an update from a gc thread. This should be called by every scanning thread after completing or
	 * aliasing away from a scanned object or pointer array segment. Updates are accepted when the scanned
	 * slots counter exceeds a threshold. Threads that scan large array segments may report scanned slot
	 * counts in sub-segment intervals to reduce likelihood of accumulator overflows. Thread scanned and
	 * copied slots counters are decremented by the accepted amounts.
	 *
	 * @param[in/out] slotsScanned pointer to number of slots scanned since accumulator was reset
	 * @param[in/out] slotsCopied pointer to number of slots copied on thread while scanning slotsScanned slots
	 * @param waitingCount number of waiting threads at current instant
	 * @return if non zero, it's time for major update. the returned value is to be passed to majorUpdate
	 */
	MMINLINE uint64_t
	update(MM_EnvironmentBase* env, uint64_t *slotsScanned, uint64_t *slotsCopied, uint64_t waitingCount, uintptr_t *copyScanUpdates, bool flush = false)
	{
		if (SCAVENGER_SLOTS_SCANNED_PER_THREAD_UPDATE <= *slotsScanned || flush) {
			uint64_t scannedCount =  *slotsScanned;
			uint64_t copiedCount =  *slotsCopied;
			*slotsScanned = *slotsCopied = 0;

			/* this thread may have scanned a long array segment resulting in scanned/copied slot counts that must be scaled down to avoid overflow in the accumulator */
			while ((SCAVENGER_SLOTS_SCANNED_PER_THREAD_UPDATE << 1) < scannedCount) {
				/* scale scanned and copied counts identically */
				scannedCount >>= 1;
				copiedCount >>= 1;
			}

			/* add this thread's samples to the accumulating register */
			uint64_t updateSample = sample(scannedCount, copiedCount, waitingCount);
			uint64_t updateResult = atomicAddThreadUpdate(updateSample);
			uint64_t updateCount = updates(updateResult);
			(*copyScanUpdates)++;

			/* this next section includes a critical region for the thread that increments the update counter to threshold */
			if (SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE == updateCount) {
				/* make sure that every other thread knows that a specific thread is performing the major update. if
				 * this thread gets timesliced in the section below while other free-running threads work up another major
				 * update, that update will be discarded */
				 
				if  (0 == MM_AtomicOperations::lockCompareExchange(&_majorUpdateThreadEnv, 0, (uintptr_t)env)) {
					return updateResult;
				}
			}
		}
		
		return 0;
	}
	
	/**
	 * Major update of progress stats: a snapshot returned by minor update is stored into _accumulatedSamples.
	 * This is the value used for subsequent calculations of copy/scan ratios and average wait counts, up until 
	 * the next major update. Various other parameters (like scan queue metrics) are also updated at this point.
	 * @param updateResult snapshot of packed wait/copy/scan/update value return by minor update and to be stored up until next major update
	 * @param nonEmptyScanLists number of non-empty scan queue lists
	 * @param cachesQueued total number of items in scan queue lists
	 */
	MMINLINE void 
	majorUpdate(MM_EnvironmentBase* env, uint64_t updateResult, uintptr_t nonEmptyScanLists, uintptr_t cachesQueued) {
		if (0 == (SCAVENGER_COUNTER_OVERFLOW & updateResult)) {
			/* no overflow so latch updateResult into _accumulatedSamples and record the update */
			MM_AtomicOperations::setU64(&_accumulatedSamples, updateResult);
			_scalingUpdateCount += 1;
			_threadCount = record(env, nonEmptyScanLists, cachesQueued);
		} else {
			/* one or more counters overflowed so discard this update */
			_overflowCount += 1;
		}
		/* Ensure updates are visible to other threads that go on to do a major update */
		MM_AtomicOperations::storeSync();
		_majorUpdateThreadEnv = 0;
	}
	/**
	 * Flush major update which may be discarded: accumulator may not of reached threshold to perform a major update. 
	 * Progress stats in the accumulator will be discarded if threshold is not reached and cycle completes. 
	 * Cached Scan queue metrics are used for flushing, a snapshot of these metrics is taken during minor update which would 
	 * of triggered a major update had it reached the threshold.
	 *
	 * This is not thread safe and should be called at the end of scan completion routine by the last blocking thread
	 */
	MMINLINE void
	flush(MM_EnvironmentBase* env, uintptr_t nonEmptyScanLists, uintptr_t cachesQueued) 
	{
		uint64_t updateResult = _accumulatingSamples;
		_accumulatingSamples = 0;
		if (0 != updateResult) {
			if (0 == (SCAVENGER_COUNTER_OVERFLOW & updateResult)) {
				/* no overflow so latch updateResult into _accumulatedSamples and record the update */
				MM_AtomicOperations::setU64(&_accumulatedSamples, updateResult);
				_scalingUpdateCount += 1;
				_threadCount = record(env, nonEmptyScanLists, cachesQueued);
			} else {
				_overflowCount += 1;
			}
		}
		reset(env, false);
	}

	/**
	 * Reset to initial state (0 accumulated samples, unit scaling factor). This
	 * should be called on each scavenging gc thread when it enters completeScan()
	 * and on the master thread before starting a scavenging gc cycle to ensure
	 * that the effective copy/scan cache size is maximal until all gc threads
	 * have entered completeScan().
	 * @param resetHistory if true history will be reset (eg at end of gc after reporting stats)
	 */
	void reset(MM_EnvironmentBase* env, bool resetHistory);

	/**
	 * Each accumulator update represents global scanned slot count of at
	 * least SCAVENGER_SLOTS_SCANNED_PER_MAJOR_UPDATE. The scaling factor
	 * can change with every major update.
	 *
	 * @return the number of major accumulator updates since start of current gc cycle
	 */
	MMINLINE uintptr_t getScalingUpdateCount() { return _scalingUpdateCount; }

	/**
	 * Overflow can occur if the thread committing a major update gets stalled while other threads drive
	 * the _accumulatingSample counters into an overflow state. The committing thread detects this state
	 * and resets the scaling factor to unity in the event.
	 *
	 * @return the number of times overflow was detected during a major update since start of current gc cycle
	 */
	MMINLINE uintptr_t getOverflowCount() { return _overflowCount; }

	/**
	 * Access the update history. This is not thread safe and should be called only after GC completes.
	 *
	 * @param[out] recordCount the number of records available
	 * @return a pointer to the least recent record
	 */
	MMINLINE UpdateHistory *
	getHistory(uintptr_t *recordCount)
	{
		*recordCount = _historyTableIndex;
		if ((SCAVENGER_UPDATE_HISTORY_SIZE > _historyTableIndex) && (0 < _historyTable[_historyTableIndex].updates)) {
			*recordCount += 1;
		}
		return _historyTable;
	}

	/**
	 * Get the number of microseconds spanned by this history record. The first history record span starts
	 * with reset(). Subsequent records span from end of previous record span.
	 * @param[in] historyRecord historical record of accumulated wait/copy/scan updates
	 * @return the number of microseconds spanned by this history record
	 */
	uint64_t getSpannedMicros(MM_EnvironmentBase* env, UpdateHistory *historyRecord);

protected:

private:
	/**
	 * Calculate and return cache size scaling factor from accumulated wait/copy/scan updates, or zero if none
	 * received yet.
	 *
	 * @param[in] threadCount total thread count
	 * @return a value >=0.0 and <= 1.0 that can be used to scale copy/scan cache sizes to reduce stalling
	 */
	MMINLINE double
	getScalingFactor(MM_EnvironmentBase* env, uint64_t threadCount, uint64_t waits, uint64_t copied, uint64_t scanned, uint64_t updates)
	{
		double scalingFactor= 0.0;

		/* validate scaling factor metrics */
		if (copied > scanned) {
			failedUpdate(env, copied, scanned);
			scalingFactor = 1.0;
			updates = 0;
		}

		if (0 < updates) {
			double copyScanRatio = 1.0;
			double runRatio = 1.0;

			/* quantize copy/scan counts and round up to nearest slot update rollover so ratio steps down in increments from 1 */
			uint64_t copyCount = MM_Math::roundToCeilingU64(SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE, copied);
			uint64_t scanCount = MM_Math::roundToCeilingU64(SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE, scanned);
			if (0 < scanCount) {
				copyScanRatio = (double)copyCount / (double)scanCount;
			}

			/* quantize average waiting thread counts and round up to nearest thread sample count so ratio steps down in increments from 1 */
			if (0 < threadCount) {
				uint64_t runCount = threadCount;
				runCount -= (waits + updates - 1) / updates;
				runRatio = (double)runCount / (double)threadCount;
			}

			/* scale more aggressively when threads are stalling, linearly with aggregate copy/scan slot count ratio */
			scalingFactor = runRatio * copyScanRatio;
		}

		return scalingFactor;
	}

	/**
	 * Attempt to add a thread update to _accumulatingSamples as an atomic operation if the accumulated
	 * sample count is below major update threshold but do not loop if contended. If another thread is
	 * concurrently updating _accumulatingSamples and its update wins, the calling thread will discard
	 * its update and proceed with the value updated by the other thread.
	 *
	 * In any case, if the result of the update has an update count not less than the major update
	 * threshold, the calling thread must reset _accumulatingSamples regardless of whether its update
	 * was accepted.
	 *
	 * @param threadUpdate The thread update to be added
	 *
	 * @return The value at _accumulatingSamples
	 */
	MMINLINE uint64_t
	atomicAddThreadUpdate(uint64_t threadUpdate)
	{
		uint64_t newValue = 0;
		/* Stop compiler optimizing away load of oldValue */
		volatile uint64_t *localAddr = &_accumulatingSamples;
		uint64_t oldValue = *localAddr;
		if (oldValue == MM_AtomicOperations::lockCompareExchangeU64(localAddr, oldValue, oldValue + threadUpdate)) {
			newValue = oldValue + threadUpdate;
			uint64_t updateCount = updates(newValue);
			if (SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE <= updateCount) {
				MM_AtomicOperations::setU64(&_accumulatingSamples, 0);
				if (SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE < updateCount) {
					newValue = 0;
				}
			}
		}
		return newValue;
	}

	MMINLINE uint64_t
	sample(uint64_t slotsScanned, uint64_t slotsCopied, uint64_t waitingCount)
	{
		return (uint64_t)1 | (slotsScanned << SCAVENGER_SLOTS_SCANNED_SHIFT) | (slotsCopied << SCAVENGER_SLOTS_COPIED_SHIFT) | (waitingCount << SCAVENGER_THREAD_WAITS_SHIFT);
	}

	MMINLINE uint64_t waits(uint64_t samples) { return samples >> SCAVENGER_THREAD_WAITS_SHIFT; }

	MMINLINE uint64_t copied(uint64_t samples) { return (SCAVENGER_SLOTS_COPIED_MASK & samples) >> SCAVENGER_SLOTS_COPIED_SHIFT; }

	MMINLINE uint64_t scanned(uint64_t samples) { return (SCAVENGER_SLOTS_SCANNED_MASK & samples) >> SCAVENGER_SLOTS_SCANNED_SHIFT; }

	MMINLINE uint64_t updates(uint64_t samples) { return samples & SCAVENGER_SLOTS_UPDATE_MASK; }

	uintptr_t record(MM_EnvironmentBase* env, uintptr_t nonEmptyScanLists, uintptr_t cachesQueued);

	void failedUpdate(MM_EnvironmentBase* env, uint64_t copied, uint64_t scanned);
};

#endif /* SCAVENGERCOPYSCANRATIO_HPP_ */
