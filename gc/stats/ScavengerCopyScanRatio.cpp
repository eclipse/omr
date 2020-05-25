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

#include "ModronAssertions.h"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "ScavengerStats.hpp"

#include "ScavengerCopyScanRatio.hpp"

void
MM_ScavengerCopyScanRatio::reset(MM_EnvironmentBase* env, bool resetHistory)
{
	_accumulatingSamples = 0;
	_accumulatedSamples = SCAVENGER_COUNTER_DEFAULT_ACCUMULATOR;
	_threadCount = env->getExtensions()->dispatcher->activeThreadCount();
	if (resetHistory) {
		OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
		_resetTimestamp = omrtime_hires_clock();
		_scalingUpdateCount = 0;
		_overflowCount = 0;
		_historyFoldingFactor = 1;
		_historyTableIndex = 0;
		_majorUpdateThreadEnv = 0;
		memset(_historyTable, 0, SCAVENGER_UPDATE_HISTORY_SIZE * sizeof(UpdateHistory));
	}
}

uintptr_t
MM_ScavengerCopyScanRatio::record(MM_EnvironmentBase* env, uintptr_t nonEmptyScanLists, uintptr_t cachesQueued)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if (SCAVENGER_UPDATE_HISTORY_SIZE <= _historyTableIndex) {
		Assert_MM_true(SCAVENGER_UPDATE_HISTORY_SIZE == _historyTableIndex);
		/* table full -- sum adjacent pairs of records and shift results to top half of table */
		UpdateHistory *head = &(_historyTable[0]);
		UpdateHistory *tail = &(_historyTable[1]);
		UpdateHistory *stop = &(_historyTable[SCAVENGER_UPDATE_HISTORY_SIZE]);
		while (tail < stop) {
			UpdateHistory *prev = tail - 1;
			prev->waits += tail->waits;
			prev->copied += tail->copied;
			prev->scanned += tail->scanned;
			prev->updates += tail->updates;
			prev->threads += tail->threads;
			prev->majorUpdates += tail->majorUpdates;
			prev->lists += tail->lists;
			prev->caches += tail->caches;
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
			prev->readObjectBarrierUpdate = tail->readObjectBarrierUpdate;
			prev->readObjectBarrierCopy = tail->readObjectBarrierCopy;
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
			prev->time = tail->time;
			if (prev > head) {
				memcpy(head, prev, sizeof(UpdateHistory));
			}
			head += 1;
			tail += 2;
		}
		_historyFoldingFactor <<= 1;
		_historyTableIndex = SCAVENGER_UPDATE_HISTORY_SIZE >> 1;
		uintptr_t zeroBytes = (SCAVENGER_UPDATE_HISTORY_SIZE >> 1) * sizeof(UpdateHistory);
		memset(&(_historyTable[_historyTableIndex]), 0, zeroBytes);
	}

	/* update record at current table index from fields in current acculumator */
	uintptr_t threadCount = env->getExtensions()->dispatcher->activeThreadCount();
	UpdateHistory *historyRecord = &(_historyTable[_historyTableIndex]);
	uint64_t accumulatedSamples = _accumulatedSamples;
	historyRecord->waits += waits(accumulatedSamples);
	historyRecord->copied += copied(accumulatedSamples);
	historyRecord->scanned += scanned(accumulatedSamples);
	historyRecord->updates += updates(accumulatedSamples);
	historyRecord->threads += threadCount;
	historyRecord->majorUpdates += 1;
	historyRecord->lists += nonEmptyScanLists;
	historyRecord->caches += cachesQueued;
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/* record current read barries values (we do not want to sum them up and average, we want last value) */
	MM_GCExtensionsBase *ext = env->getExtensions();
	historyRecord->readObjectBarrierUpdate = ext->scavengerStats._readObjectBarrierUpdate;
	historyRecord->readObjectBarrierCopy = ext->scavengerStats._readObjectBarrierCopy;
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	historyRecord->time = omrtime_hires_clock();

	/* advance table index if current record is maxed out */
	if (historyRecord->updates >= (_historyFoldingFactor * SCAVENGER_THREAD_UPDATES_PER_MAJOR_UPDATE)) {
		_historyTableIndex += 1;
	}
	return threadCount;
}

uint64_t
MM_ScavengerCopyScanRatio::getSpannedMicros(MM_EnvironmentBase* env, UpdateHistory *historyRecord)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t start = (historyRecord == _historyTable) ? _resetTimestamp : (historyRecord - 1)->time;
	return omrtime_hires_delta(start, historyRecord->time, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
}

void
MM_ScavengerCopyScanRatio::failedUpdate(MM_EnvironmentBase* env, uint64_t copied, uint64_t scanned)
{
	Assert_GC_true_with_message2(env, copied <= scanned, "MM_ScavengerCopyScanRatio::getScalingFactor(): copied (=%llu) exceeds scanned (=%llu) -- non-atomic 64-bit read\n", copied, scanned);
}

