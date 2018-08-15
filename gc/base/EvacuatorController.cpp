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

#include <stdint.h>
#include "omrport.h"

#include "AllocateDescription.hpp"
#include "AtomicSupport.hpp"
#include "Bits.hpp"
#include "Dispatcher.hpp"
#include "EvacuatorController.hpp"
#include "EvacuatorScanspace.hpp"
#include "Math.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "ScavengerCopyScanRatio.hpp"
#include "ScavengerStats.hpp"

bool
MM_EvacuatorController::setEvacuatorFlag(uintptr_t flag, bool value)
{
	uintptr_t oldFlags = _evacuatorFlags;
	if (value) {
		if (!isEvacuatorFlagSet(flag)) {
			acquireController();
			if (!isEvacuatorFlagSet(flag)) {
				oldFlags = VM_AtomicSupport::bitOr(&_evacuatorFlags, flag);
			}
			releaseController();
		}
	} else {
		if (isEvacuatorFlagSet(flag)) {
			acquireController();
			if (isEvacuatorFlagSet(flag)) {
				oldFlags = VM_AtomicSupport::bitAnd(&_evacuatorFlags, ~flag);
			}
			releaseController();
		}
	}
	return (flag == (flag & oldFlags));
}

bool
MM_EvacuatorController::setAborting(MM_Evacuator *abortingWorker)
{
	/* test & set the aborting flag */
	if (!setEvacuatorFlag(aborting, true)) {
#if defined(EVACUATOR_DEBUG)
		if (_debugger.isDebugEnd()) {
			MM_EnvironmentBase *env = abortingWorker->getEnvironment();
			OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
			omrtty_printf("%5lu %2llu %2llu:     abort; ", getEpoch()->gc, getEpoch()->epoch, abortingWorker->getWorkerIndex());
			printEvacuatorBitmap(env, "stalled", _stalledEvacuatorBitmap);
			printEvacuatorBitmap(env, "; resuming", _resumingEvacuatorBitmap);
			printEvacuatorBitmap(env, "; mask", _evacuatorMask);
			omrtty_printf("; flags:%llx\n", _evacuatorFlags);
		}
#endif /* defined(EVACUATOR_DEBUG) */
	}

	return isAborting();
}

bool
MM_EvacuatorController::initialize(MM_EnvironmentBase *env)
{
	bool result = true;

	if (_extensions->isEvacuatorEnabled()) {
		if (0 != omrthread_monitor_init_with_name(&_controllerMutex, 0, "MM_EvacuatorController::_controllerMutex")) {
			_controllerMutex = NULL;
			return false;
		}

		if (0 != omrthread_monitor_init_with_name(&_reporterMutex, 0, "MM_EvacuatorController::_reporterMutex")) {
			omrthread_monitor_destroy(_controllerMutex);
			_controllerMutex = NULL;
			_reporterMutex = NULL;
			return false;
		}

		/* initialize heap region bounds, copied here at the start of each gc cycle for ease of access */
		for (intptr_t space = (intptr_t)MM_Evacuator::survivor; space <= (intptr_t)MM_Evacuator::evacuate; space += 1) {
			_heapLayout[space][0] = NULL;
			_heapLayout[space][1] = NULL;
			_memorySubspace[space] = NULL;
		}

		_history.reset();

#if defined(EVACUATOR_DEBUG)
		_debugger.setDebugFlags();
#endif /* defined(EVACUATOR_DEBUG) */

		result = (NULL != _controllerMutex);
	}

	return result;
}

void
MM_EvacuatorController::tearDown(MM_EnvironmentBase *env)
{
	if (_extensions->isEvacuatorEnabled()) {
		for (uintptr_t workerIndex = 0; workerIndex < _maxGCThreads; workerIndex++) {
			if (NULL != _evacuatorTask[workerIndex]) {
				Debug_MM_true(NULL == _evacuatorTask[workerIndex]->getEnvironment());
				_evacuatorTask[workerIndex]->kill();
				_evacuatorTask[workerIndex] = NULL;
			}
		}

		MM_Forge *forge = env->getForge();
		forge->free((void *)_boundEvacuatorBitmap);
		forge->free((void *)_stalledEvacuatorBitmap);
		forge->free((void *)_resumingEvacuatorBitmap);
		forge->free((void *)_evacuatorMask);
		forge->free((void *)_evacuatorTask);

		if (NULL != _controllerMutex) {
			omrthread_monitor_destroy(_controllerMutex);
			_controllerMutex = NULL;
		}
		if (NULL != _reporterMutex) {
			omrthread_monitor_destroy(_reporterMutex);
			_reporterMutex = NULL;
		}
	}
}

bool
MM_EvacuatorController::collectorStartup(MM_GCExtensionsBase* extensions)
{
#if defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS)
	OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
	_collectorStartTime = omrtime_hires_clock();
#endif /* defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS) */

	return true;
}

void
MM_EvacuatorController::collectorShutdown(MM_GCExtensionsBase* extensions)
{
	flushTenureWhitespace(true);
}

void
MM_EvacuatorController::flushTenureWhitespace(bool shutdown)
{
	uint64_t flushed = 0;

	if (_extensions->isEvacuatorEnabled()) {
		for (uintptr_t workerIndex = 0; workerIndex < _maxGCThreads; workerIndex += 1) {
			if (NULL != _evacuatorTask[workerIndex]) {
				Debug_MM_true(NULL == _evacuatorTask[workerIndex]->getEnvironment());
				flushed += _evacuatorTask[workerIndex]->flushTenureWhitespace();
			}
		}
		_globalTenureFlushedBytes += flushed;
	}

#if defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS)
#if defined(EVACUATOR_DEBUG)
	if (_debugger.isDebugEnd()) {
#endif /* defined(EVACUATOR_DEBUG) */
		OMRPORT_ACCESS_FROM_OMRVM(_omrVM);
		if (!shutdown) {
			omrtty_printf("%5llu      : global gc; tenure; flushed:%llx\n", _history.epoch()->gc, flushed);
		} else {
			uint64_t collectorElapsedMicros = omrtime_hires_delta(_collectorStartTime, omrtime_hires_clock(), OMRPORT_TIME_DELTA_IN_MICROSECONDS);
			omrtty_printf("%5llu      :  shutdown; elapsed:%llu; flushed:%llx\n", _history.epoch()->gc, collectorElapsedMicros, _globalTenureFlushedBytes);
		}
#if defined(EVACUATOR_DEBUG)
	}
#endif /* defined(EVACUATOR_DEBUG) */
#endif /* defined(EVACUATOR_DEBUG) || defined(EVACUATOR_DEBUG_ALWAYS) */
}

void
MM_EvacuatorController::copyHeapLayout(uint8_t *a[][2], uint8_t *b[][2])
{
	for (uintptr_t regionIndex = (uintptr_t)MM_Evacuator::survivor; regionIndex <= ((uintptr_t)MM_Evacuator::evacuate); regionIndex += 1) {
		a[regionIndex][0] = b[regionIndex][0]; /* lower bound for heap region address range */
		a[regionIndex][1] = b[regionIndex][1]; /* upper bound for heap region address range */
	}
}

void
MM_EvacuatorController::masterSetupForGC(MM_EnvironmentStandard *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	_evacuatorFlags = 0;
	_evacuatorIndex = 0;
	_evacuatorCount = _dispatcher->adjustThreadCount(_dispatcher->threadCount());
	_finalDiscardedBytes = 0;
	_finalFlushedBytes = 0;
	_copiedBytes[MM_Evacuator::survivor] = 0;
	_copiedBytes[MM_Evacuator::tenure] = 0;
	_scannedBytes = 0;

	/* reset the evacuator bit maps */
	uintptr_t tailWords = 0;
	uintptr_t bitmapWords = countEvacuatorBitmapWords(&tailWords);
	for (uintptr_t map = 0; map < bitmapWords; map += 1) {
		Debug_MM_true(0 == _boundEvacuatorBitmap[map]);
		_boundEvacuatorBitmap[map] = 0;
		Debug_MM_true(0 == _stalledEvacuatorBitmap[map]);
		_stalledEvacuatorBitmap[map] = 0;
		Debug_MM_true(0 == _resumingEvacuatorBitmap[map]);
		_resumingEvacuatorBitmap[map] = 0;
		_evacuatorMask[map] = 0;
	}
	_stalledEvacuatorCount = 0;

	/* set up controller's subspace and layout arrays to match collector subspaces */
	_memorySubspace[MM_Evacuator::evacuate] = _evacuateMemorySubSpace;
	_memorySubspace[MM_Evacuator::survivor] = _survivorMemorySubSpace;
	_memorySubspace[MM_Evacuator::tenure] = _tenureMemorySubSpace;

	_heapLayout[MM_Evacuator::evacuate][0] = (uint8_t *)_evacuateSpaceBase;
	_heapLayout[MM_Evacuator::evacuate][1] = (uint8_t *)_evacuateSpaceTop;
	_heapLayout[MM_Evacuator::survivor][0] = (uint8_t *)_survivorSpaceBase;
	_heapLayout[MM_Evacuator::survivor][1] = (uint8_t *)_survivorSpaceTop;
	_heapLayout[MM_Evacuator::tenure][0] = (uint8_t *)_extensions->_tenureBase;
	_heapLayout[MM_Evacuator::tenure][1] = _heapLayout[MM_Evacuator::tenure][0] + _extensions->_tenureSize;

	/* set upper bounds for tlh allocation size, indexed by outside region -- reduce these for small (<32mb) heaps */
	uintptr_t copyspaceSize = maximumCopyspaceSize;
	uintptr_t survivorSize = (uintptr_t)_heapLayout[MM_Evacuator::survivor][1] - (uintptr_t)_heapLayout[MM_Evacuator::survivor][0];
	if (((uintptr_t)2 << 20) >= survivorSize) {
		/* heap no more than 2mb, reduce to 25% of maximumCopyspaceSize */
		copyspaceSize >>= 2;
	} else if (((uintptr_t)32 << 20) >= survivorSize) {
		/* heap more than 2mb and no more than 32mb, reduce to 50% of maximumCopyspaceSize */
		copyspaceSize >>= 1;
	}
	for (uintptr_t copyspaceIndex = (uintptr_t)MM_Evacuator::survivor; copyspaceIndex <= ((uintptr_t)MM_Evacuator::tenure); copyspaceIndex += 1) {
		_copyspaceAllocationCeiling[copyspaceIndex] = copyspaceSize;
		_objectAllocationCeiling[copyspaceIndex] = ~(uintptr_t)0xff;
	}

#if defined(EVACUATOR_DEBUG)|| defined(EVACUATOR_DEBUG_ALWAYS)
#if defined(EVACUATOR_DEBUG)
	if (_debugger.isDebugEnd()) {
#endif /* defined(EVACUATOR_DEBUG) */
		omrtty_printf("%5llu      :  gc start; survivor{%llx %llx} tenure{%llx %llx} evacuate{%llx %llx}; threads:%llu; projection:%llx; allocation:%llx\n",
				_extensions->scavengerStats._gcCount, (uintptr_t)_heapLayout[0][0], (uintptr_t)_heapLayout[0][1], (uintptr_t)_heapLayout[1][0], (uintptr_t)_heapLayout[1][1],
				(uintptr_t)_heapLayout[2][0], (uintptr_t)_heapLayout[2][1], _dispatcher->adjustThreadCount(_dispatcher->threadCount()),
				calculateProjectedEvacuationBytes(), copyspaceSize);
#if defined(EVACUATOR_DEBUG)
	}
#endif /* defined(EVACUATOR_DEBUG) */
#endif /* defined(EVACUATOR_DEBUG)|| defined(EVACUATOR_DEBUG_ALWAYS) */

	/* reset history, maximize inital allocation ceiling, minimize work release threshold to allow even distribution of work during first epoch */
	_history.reset(_extensions->scavengerStats._gcCount, minimumCopyspaceSize, minimumCopyspaceSize);
	_epochTimestamp = omrtime_hires_clock();

	/* prepare the evacuator delegate class and enable it to add private flags for the cycle */
	_evacuatorFlags |= MM_EvacuatorDelegate::prepareForEvacuation(env);
}

MM_Evacuator *
MM_EvacuatorController::bindWorker(MM_EnvironmentStandard *env)
{
	/* get an unbound evacuator instance */
	uintptr_t workerIndex = VM_AtomicSupport::add(&_evacuatorIndex, 1) - 1;
	Debug_MM_true(_evacuatorCount == env->_currentTask->getThreadCount());
	Debug_MM_true(_evacuatorCount > workerIndex);

	/* instantiate evacuator task for this worker thread if required (evacuators are instantiated once and persist until vm shuts down) */
	if (NULL == _evacuatorTask[workerIndex]) {
		_evacuatorTask[workerIndex] = MM_Evacuator::newInstance(workerIndex, this, &env->getExtensions()->objectModel, env->getExtensions()->getForge());
	}

	/* controller doesn't have final view on evacuator thread count until after tasks are dispatched ... */
	if (isEvacuatorBitmapEmpty(_evacuatorMask)) {
		/* ... so first evacuator to reach this point must complete thread count dependent initialization */
		acquireController();
		if (isEvacuatorBitmapEmpty(_evacuatorMask)) {
			fillEvacuatorBitmap(_evacuatorMask);
			_copiedBytesReportingDelta = MM_Math::roundToCeiling(epochs_per_cycle, (uintptr_t)(calculateProjectedEvacuationBytes() / (env->_currentTask->getThreadCount() * epochs_per_cycle)));
			if (_copiedBytesReportingDelta < minimumCopyspaceSize) {
				_copiedBytesReportingDelta = minimumCopyspaceSize;
			}
			/* reporting delta roughly partitions scanned byte count to produce a preset number of epochs per gc cycle */
			VM_AtomicSupport::setU64(&_nextEpochCopiedBytesThreshold, _copiedBytesReportingDelta * env->_currentTask->getThreadCount());
		}
		releaseController();
	}
	setEvacuatorBit(workerIndex, _boundEvacuatorBitmap);

	Debug_MM_true(testEvacuatorBit(workerIndex, _evacuatorMask));
	Debug_MM_true(isEvacuatorBitmapFull(_evacuatorMask));

	_evacuatorTask[workerIndex]->bindWorkerThread(env);

#if defined(EVACUATOR_DEBUG)
	if (_debugger.isDebugCycle()) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("%5lu %2llu %2llu: %cbind[%2llu]; ", getEpoch()->gc, getEpoch()->epoch, workerIndex, env->isMasterThread() ? '*' : ' ', _evacuatorCount);
		printEvacuatorBitmap(env, "bound", _boundEvacuatorBitmap);
		printEvacuatorBitmap(env, "; stalled", _stalledEvacuatorBitmap);
		printEvacuatorBitmap(env, "; resuming", _resumingEvacuatorBitmap);
		printEvacuatorBitmap(env, "; mask", _evacuatorMask);
		omrtty_printf("; flags%llx; threads:%llu; reporting:%llx\n", _evacuatorFlags, env->_currentTask->getThreadCount(), _copiedBytesReportingDelta);
	}
#endif /* defined(EVACUATOR_DEBUG) */

	return _evacuatorTask[workerIndex];
}

void
MM_EvacuatorController::unbindWorker(MM_EnvironmentStandard *env)
{
	MM_Evacuator *evacuator = env->getEvacuator();

	/* pull final remaining metrics from evacuator */
	VM_AtomicSupport::addU64(&_finalDiscardedBytes, evacuator->getDiscarded());
	VM_AtomicSupport::addU64(&_finalFlushedBytes, evacuator->getFlushed());

	/* passivate the evacuator instance */
	evacuator->unbindWorkerThread(env);
	clearEvacuatorBit(evacuator->getWorkerIndex(), _boundEvacuatorBitmap);
	if (isEvacuatorBitmapEmpty(_boundEvacuatorBitmap)) {
		Debug_MM_true(isEvacuatorBitmapEmpty(_stalledEvacuatorBitmap) || isAborting());
		_finalEvacuatedBytes = _copiedBytes[MM_Evacuator::survivor] + _copiedBytes[MM_Evacuator::tenure];
		Debug_MM_true((_finalEvacuatedBytes == _scannedBytes) || isAborting());
	}

#if defined(EVACUATOR_DEBUG)
	if (_debugger.isDebugCycle()) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("%5lu %2llu %2llu:    unbind; ", getEpoch()->gc, getEpoch()->epoch, evacuator->getWorkerIndex());
		printEvacuatorBitmap(env, "bound", _boundEvacuatorBitmap);
		printEvacuatorBitmap(env, "; stalled", _stalledEvacuatorBitmap);
		printEvacuatorBitmap(env, "; resuming", _resumingEvacuatorBitmap);
		omrtty_printf("; flags:%llx\n", _evacuatorFlags);
	}
#endif /* defined(EVACUATOR_DEBUG) */
}

void
MM_EvacuatorController::startWorker(MM_Evacuator *worker, uintptr_t *tenureMask, uint8_t *heapBounds[][2], uint64_t *copiedBytesReportingDelta)
{
	*tenureMask = _tenureMask;
	copyHeapLayout(heapBounds, _heapLayout);
	*copiedBytesReportingDelta = _copiedBytesReportingDelta;
}

void
MM_EvacuatorController::waitToSynchronize(MM_Evacuator *worker, const char *id)
{
	Debug_MM_true(!testEvacuatorBit(worker->getWorkerIndex(), _stalledEvacuatorBitmap));
	Debug_MM_true(!testEvacuatorBit(worker->getWorkerIndex(), _resumingEvacuatorBitmap));
#if defined(EVACUATOR_DEBUG)
	if (_debugger.isDebugCycle()) {
		MM_EnvironmentBase *env = worker->getEnvironment();
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("%5lu %2llu %2llu:      sync; ", getEpoch()->gc, getEpoch()->epoch, worker->getWorkerIndex());
		printEvacuatorBitmap(env, "stalled", _stalledEvacuatorBitmap);
		printEvacuatorBitmap(env, "; resuming", _resumingEvacuatorBitmap);
		omrtty_printf("; flags:%llx; workunit:%llx; %s\n", _evacuatorFlags, worker->getEnvironment()->getWorkUnitIndex(), MM_EvacuatorBase::callsite(id));
	}
#endif /* defined(EVACUATOR_DEBUG) */
}

void
MM_EvacuatorController::continueAfterSynchronizing(MM_Evacuator *worker, uint64_t startTime, uint64_t endTime, const char *id)
{
	Debug_MM_true(!testEvacuatorBit(worker->getWorkerIndex(), _stalledEvacuatorBitmap));
	Debug_MM_true(!testEvacuatorBit(worker->getWorkerIndex(), _resumingEvacuatorBitmap));
#if defined(EVACUATOR_DEBUG)
	if (_debugger.isDebugCycle()) {
		MM_EnvironmentBase *env = worker->getEnvironment();
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		uint64_t waitMicros = omrtime_hires_delta(startTime, endTime, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
		omrtty_printf("%5lu %2llu %2llu:  continue; ", getEpoch()->gc, getEpoch()->epoch, worker->getWorkerIndex());
		printEvacuatorBitmap(env, "stalled", _stalledEvacuatorBitmap);
		printEvacuatorBitmap(env, "; resuming", _resumingEvacuatorBitmap);
		omrtty_printf("; flags:%llx; micros:%llx; %s\n", _evacuatorFlags, waitMicros, MM_EvacuatorBase::callsite(id));
	}
#endif /* defined(EVACUATOR_DEBUG) */
}

bool
MM_EvacuatorController::isWaitingToCompleteStall(MM_Evacuator *worker, MM_EvacuatorWorkPacket *work)
{
	uintptr_t workerIndex = worker->getWorkerIndex();

	if (NULL == work) {
		/* this worker is stalled or stalling -- its stall bit will remain set until it acknowledges that its resume bit has been set */
		uint64_t otherStalledEvacuators = setEvacuatorBit(workerIndex, _stalledEvacuatorBitmap);
		/* the thread that atomically sets the last stall bit to complete the stalled bitmap will end the scan cycle and notify other stalled evacuators */
		if (0 == (otherStalledEvacuators & getEvacuatorBitMask(workerIndex))) {
			_stalledEvacuatorCount += 1;
			if (isEvacuatorBitmapFull(_stalledEvacuatorBitmap)) {
				/* check that none of the evacuators in the stalled bitmap are resuming (ie, with work) */
				if (isEvacuatorBitmapEmpty(_resumingEvacuatorBitmap)) {
					Debug_MM_true(_stalledEvacuatorCount == _evacuatorCount);
					Debug_MM_true(isAborting() || ((_copiedBytes[MM_Evacuator::survivor] + _copiedBytes[MM_Evacuator::tenure]) == _scannedBytes));
					/* all evacuators are stalled at this point and can now complete or abort scan cycle -- set them all up to resume */
					fillEvacuatorBitmap(_resumingEvacuatorBitmap);
	#if defined(EVACUATOR_DEBUG)
					if (_debugger.isDebugCycle() || _debugger.isDebugWork()) {
						MM_EnvironmentBase *env = worker->getEnvironment();
						OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
						omrtty_printf("%5lu %2llu %2llu:   complete; ", _history.epoch()->gc, _history.epoch()->epoch, worker->getWorkerIndex());
						printEvacuatorBitmap(env, "stalled", _stalledEvacuatorBitmap);
						printEvacuatorBitmap(env, "; resuming", _resumingEvacuatorBitmap);
						omrtty_printf("; flags:%llx; copied:%llx; scanned:%llx\n",  _evacuatorFlags, (_copiedBytes[MM_Evacuator::survivor] + _copiedBytes[MM_Evacuator::tenure]), _scannedBytes);
					}
	#endif /* defined(EVACUATOR_DEBUG) */
					/* notify the other stalled evacuators to  resume after they call this method once more */
					omrthread_monitor_notify_all(_controllerMutex);
				}
			}
		}
	} else {
		Debug_MM_true(!isAborting() && isStalledEvacuator(workerIndex));
		/* this evacuator has work and can resume */
		setEvacuatorBit(workerIndex, _resumingEvacuatorBitmap);
	}

	Debug_MM_true(testEvacuatorBit(workerIndex, _stalledEvacuatorBitmap));
	return (!testEvacuatorBit(workerIndex, _resumingEvacuatorBitmap));
}

MM_EvacuatorWorkPacket *
MM_EvacuatorController::continueAfterStall(MM_Evacuator *worker, MM_EvacuatorWorkPacket *work)
{
	uintptr_t workerIndex = worker->getWorkerIndex();

	/* stalled and resuming bits must be set for the evacuator */
	Debug_MM_true(testEvacuatorBit(workerIndex, _stalledEvacuatorBitmap));
	Debug_MM_true(testEvacuatorBit(workerIndex, _resumingEvacuatorBitmap));

	/* clear the stalled and resuming bits for the evacuator */
	clearEvacuatorBit(workerIndex, _stalledEvacuatorBitmap);
	clearEvacuatorBit(workerIndex, _resumingEvacuatorBitmap);
	_stalledEvacuatorCount -= 1;

	/* no work at this point means all evacuators are completing or aborting the scan cycle */
	if (NULL == work) {
		Debug_MM_true4(worker->getEnvironment(), isEvacuatorBitmapEmpty(_stalledEvacuatorBitmap) == isEvacuatorBitmapEmpty(_resumingEvacuatorBitmap), "*stalled=%llx; *resuming=%llx; *evacuators=%llx; *bound=%llx\n",
				*_stalledEvacuatorBitmap, *_resumingEvacuatorBitmap, *_evacuatorMask, *_boundEvacuatorBitmap);
		/* complete scan for this evacuator */
		worker->scanComplete();
	} else {
		/* notify any other evacuators that there may be work available */
		omrthread_monitor_notify(_controllerMutex);
	}

#if defined(EVACUATOR_DEBUG)
	if ((_debugger.isDebugCycle() || _debugger.isDebugWork()) && (NULL == work) && isEvacuatorBitmapEmpty(_stalledEvacuatorBitmap) && isEvacuatorBitmapEmpty(_resumingEvacuatorBitmap)) {
		MM_EnvironmentBase *env = worker->getEnvironment();
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("%5lu %2llu %2llu:  end scan; ", _history.epoch()->gc, _history.epoch()->epoch, worker->getWorkerIndex());
		printEvacuatorBitmap(env, "stalled", _stalledEvacuatorBitmap);
		printEvacuatorBitmap(env, "; resuming", _resumingEvacuatorBitmap);
		omrtty_printf("; flags:%llx; copied:%llx; scanned:%llx\n",  _evacuatorFlags, (_copiedBytes[MM_Evacuator::survivor] + _copiedBytes[MM_Evacuator::tenure]), _scannedBytes);
	}
#endif /* defined(EVACUATOR_DEBUG) */

	return work;
}

void
MM_EvacuatorController::reportProgress(MM_Evacuator *worker,  uint64_t *copied, uint64_t *scanned)
{
	VM_AtomicSupport::addU64(&_copiedBytes[MM_Evacuator::survivor], copied[MM_Evacuator::survivor]);
	VM_AtomicSupport::addU64(&_copiedBytes[MM_Evacuator::tenure], copied[MM_Evacuator::tenure]);
	VM_AtomicSupport::addU64(&_scannedBytes, *scanned);

	copied[MM_Evacuator::survivor] = 0;
	copied[MM_Evacuator::tenure] = 0;
	*scanned = 0;

#if defined(EVACUATOR_DEBUG)
	uint64_t survivorCopied = _copiedBytes[MM_Evacuator::survivor];
	uint64_t tenureCopied = _copiedBytes[MM_Evacuator::tenure];
#endif /* defined(EVACUATOR_DEBUG) */

	uint64_t nextEpochCopiedBytesThreshold = _nextEpochCopiedBytesThreshold;
	uint64_t totalCopied = _copiedBytes[MM_Evacuator::survivor] + _copiedBytes[MM_Evacuator::tenure];
	if ((totalCopied > nextEpochCopiedBytesThreshold) || (totalCopied == _scannedBytes)) {
		omrthread_monitor_enter(_reporterMutex);
		MM_EvacuatorHistory::Epoch *epoch = NULL;
		if (nextEpochCopiedBytesThreshold == _nextEpochCopiedBytesThreshold) {
			/* conclude the epoch at the tip of epochal record */
			OMRPORT_ACCESS_FROM_ENVIRONMENT(worker->getEnvironment());
			uint64_t currentTimestamp = omrtime_hires_clock();
			uint64_t epochDurationMicros = omrtime_hires_delta(_epochTimestamp, currentTimestamp, OMRPORT_TIME_DELTA_IN_MICROSECONDS);
			epoch = _history.add(worker->getEnvironment()->_scavengerStats._gcCount, epochDurationMicros, totalCopied, _scannedBytes);
			epoch->tlhAllocationCeiling = calculateOptimalWhitespaceSize();
			epoch->releaseThreshold = (uintptr_t)calculateOptimalWorkPacketSize(0 == _scannedBytes);
			_epochTimestamp = currentTimestamp;
			_history.commit(epoch);
			/* set next epochal threshold */
			VM_AtomicSupport::setU64(&_nextEpochCopiedBytesThreshold, totalCopied + (_copiedBytesReportingDelta * _evacuatorCount));
		}
		omrthread_monitor_exit(_reporterMutex);

#if defined(EVACUATOR_DEBUG)
		if (NULL != epoch) {
			_debugger.setDebugCycleAndEpoch(epoch->gc, epoch->epoch);
			if (_debugger.isDebugEpoch()) {
				OMRPORT_ACCESS_FROM_ENVIRONMENT(worker->getEnvironment());
				omrtty_printf("%5llu %2llu %2llu:     epoch; survivor:%llx; tenure:%llx; scanned:%llx; delta:%llx; micros:%llu; allocate:%llx; release:%llx\n",
						epoch->gc, epoch->epoch, worker->getWorkerIndex(), survivorCopied, tenureCopied, epoch->scanned, ((survivorCopied + tenureCopied) - epoch->scanned), epoch->duration,
						epoch->tlhAllocationCeiling, epoch->releaseThreshold);
			}
		}
#endif /* defined(EVACUATOR_DEBUG) */
	}
}

uint64_t
MM_EvacuatorController::calculateProjectedEvacuationBytes()
{
	if (0 < _finalEvacuatedBytes) {
		/* inflate evacuated volume from most recent gc by 1.25 */
		return (5 * _finalEvacuatedBytes) >> 2;
	} else {
		/* for first gc cycle deflate projected survivor volume by 0.75 */
		return (3 * (_heapLayout[MM_Evacuator::survivor][1] - _heapLayout[MM_Evacuator::survivor][0])) >> 2;
	}
}

double
MM_EvacuatorController::calcluateEvacuatorBandwidth()
{
	return (double)(_evacuatorCount - _stalledEvacuatorCount) / (double)_evacuatorCount;
}

uintptr_t
MM_EvacuatorController::calculateOptimalWhitespaceSize()
{
	/* maximize whitespace allocation size until heap scan is nearly complete, then drop it rapidly to reduce macro fragmentation */
	uintptr_t whitesize = MM_EvacuatorController::maximumCopyspaceSize;

	/* threads stall in quick succession near the end of heap scan so scale by the square of the available evacuator bandwidth */
	if ((3 < _evacuatorCount) && (MM_Math::floorLog2(_evacuatorCount) < _stalledEvacuatorCount)) {
		/* use stalled thread count to guess likeihood of nearly complete scan */
		double evacuatorBandwidth = calcluateEvacuatorBandwidth();
		double whitespaceScaling = evacuatorBandwidth * evacuatorBandwidth;
		whitesize = MM_EvacuatorController::minimumCopyspaceSize;
		whitesize += (uintptr_t)(whitespaceScaling * (MM_EvacuatorController::maximumCopyspaceSize - MM_EvacuatorController::minimumCopyspaceSize));
	}

	/* limit to allocation ceiling */
	uintptr_t limit = OMR_MIN(_copyspaceAllocationCeiling[MM_Evacuator::survivor], _copyspaceAllocationCeiling[MM_Evacuator::tenure]);
	if (whitesize > limit) {
		whitesize = limit;
	}

	/* round down to page size -- result will be object aligned */
	whitesize = MM_Math::roundToFloor(MM_EvacuatorBase::tlh_allocation_granularity, whitesize);

	return whitesize;
}

uintptr_t
MM_EvacuatorController::calculateOptimalWorkPacketSize(bool isRootScan)
{
	/* minimize work packet size while building up shareable work during root scanning and clearing phases */
	uintptr_t worksize = MM_EvacuatorController::minimumWorkspaceSize;

	if (!isRootScan) {
		/* during heap scan track the volume of unscanned evacuated objects */
		uint64_t lowVolumeOfWork = _evacuatorCount * MM_EvacuatorController::minimumWorkspaceSize;
		uint64_t aggegateVolumeOfWork = (_copiedBytes[MM_Evacuator::survivor] + _copiedBytes[MM_Evacuator::tenure]) - _scannedBytes;
		if (aggegateVolumeOfWork < lowVolumeOfWork) {
			/* scale work packet size down if there is a low aggregate volume of proven scan work */
			worksize = MM_EvacuatorController::minimumWorkspaceSize;
			double workspaceScaling = calcluateEvacuatorBandwidth() * ((double)aggegateVolumeOfWork / (double)lowVolumeOfWork);
			worksize += (uintptr_t)(workspaceScaling * (MM_EvacuatorController::maximumWorkspaceSize - MM_EvacuatorController::minimumWorkspaceSize));
		} else {
			/* go with the maximum work packet size */
			worksize = MM_EvacuatorController::maximumWorkspaceSize;
		}
	}

	return alignToObjectSize(worksize);
}

MM_EvacuatorWhitespace *
MM_EvacuatorController::getInsideFreespace(MM_Evacuator *evacuator, MM_Evacuator::EvacuationRegion region, uintptr_t length)
{
	MM_EvacuatorWhitespace *freespace = NULL;
	if (!isAborting()) {
		freespace = allocateWhitespace(evacuator, region, 0, length, true);
	}
	return freespace;
}

MM_EvacuatorWhitespace *
MM_EvacuatorController::getOutsideFreespace(MM_Evacuator *evacuator, MM_Evacuator::EvacuationRegion region, uintptr_t remainder, uintptr_t length)
{
	MM_EvacuatorWhitespace *freespace = NULL;
	if (!isAborting()) {
		freespace = allocateWhitespace(evacuator, region, remainder, length, false);
	}
	return freespace;
}

MM_EvacuatorWhitespace *
MM_EvacuatorController::allocateWhitespace(MM_Evacuator *evacuator, MM_Evacuator::EvacuationRegion region, uintptr_t remainder, uintptr_t minimumLength, bool inside)
{
	Debug_MM_true(MM_Evacuator::evacuate > region);
	Debug_MM_true(isObjectAligned((void*)minimumLength));
	Debug_MM_true(remainder < minimumLength);

	if (minimumLength >= _objectAllocationCeiling[region]) {
		return NULL;
	}

	MM_EnvironmentBase *env = evacuator->getEnvironment();
	MM_EvacuatorWhitespace *whitespace = NULL;
	uintptr_t optimalSize = 0;

	/* try to allocate a tlh unless object won't fit in outside copyspace remainder and remainder is still too big to whitelist */
	uintptr_t maximumLength = (MM_EvacuatorBase::low_work_volume < evacuator->getVolumeOfWork()) ? calculateOptimalWhitespaceSize() : minimumCopyspaceSize;
	if ((minimumLength <= maximumLength) && (inside || (remainder < MM_EvacuatorBase::max_copyspace_remainder))) {
		/* try to allocate tlh in region to contain at least minimumLength bytes */
		optimalSize =  maximumLength;
		uintptr_t limitSize = OMR_MAX(allocation_page_size, minimumLength);
		while ((NULL == whitespace) && (optimalSize >= limitSize)) {
			void *addrBase = NULL, *addrTop = NULL;
			MM_AllocateDescription allocateDescription(0, 0, false, true);
			allocateDescription.setCollectorAllocateExpandOnFailure(MM_Evacuator::tenure == region);
			void *allocation = (MM_EvacuatorWhitespace *)getMemorySubspace(region)->collectorAllocateTLH(env, this, &allocateDescription, optimalSize, addrBase, addrTop);
			if (NULL != allocation) {
				 bool isLOA = (MM_Evacuator::tenure == region) && allocateDescription.isLOAAllocation();
				/* got a tlh of some size <= optimalSize */
				uintptr_t whitesize = (uintptr_t)addrTop - (uintptr_t)addrBase;
				whitespace = MM_EvacuatorWhitespace::whitespace(allocation, whitesize, isLOA);
				env->_scavengerStats.countCopyCacheSize(whitesize, maximumCopyspaceSize);
				if (MM_Evacuator::survivor == region) {
					env->_scavengerStats._semiSpaceAllocationCountSmall += 1;
				} else {
					env->_scavengerStats._tenureSpaceAllocationCountSmall += 1;
				}
			} else {
				/* lower (reduce by half) the tlh allocation ceiling for the region */
				uintptr_t allocationCeiling = _copyspaceAllocationCeiling[region] >> 1;
				while (allocationCeiling < _copyspaceAllocationCeiling[region]) {
					VM_AtomicSupport::lockCompareExchange(&_copyspaceAllocationCeiling[region], _copyspaceAllocationCeiling[region], allocationCeiling);
				}
				/* and try again using a reduced optimalSize */
				optimalSize = _copyspaceAllocationCeiling[region];
			}
		}

		/* hand off any unused tlh allocation to evacuator to reuse later */
		if ((NULL != whitespace) && (whitespace->length() < minimumLength)) {
			evacuator->receiveWhitespace(whitespace);
			whitespace = NULL;
		}
	}

	/* on inside allocation failure stack will shutdown and retry, copying this and subsequent objects to outside or large copyspace */
	if (!inside && (NULL == whitespace)) {
		 /* allocate minimal (this object's exact) size */
		optimalSize = minimumLength;
		MM_AllocateDescription allocateDescription(optimalSize, 0, false, true);
		allocateDescription.setCollectorAllocateExpandOnFailure(MM_Evacuator::tenure == region);
		void *allocation = getMemorySubspace(region)->collectorAllocate(env, this, &allocateDescription);
		if (NULL != allocation) {
			bool isLOA = (MM_Evacuator::tenure == region) && allocateDescription.isLOAAllocation();
			Debug_MM_true(isObjectAligned(allocation));
			whitespace = MM_EvacuatorWhitespace::whitespace(allocation, optimalSize, isLOA);
			if (MM_Evacuator::survivor == region) {
				env->_scavengerStats._semiSpaceAllocationCountLarge += 1;
			} else {
				env->_scavengerStats._tenureSpaceAllocationCountLarge += 1;
			}
			env->_scavengerStats.countCopyCacheSize(optimalSize, maximumCopyspaceSize);
		} else {
			/* lower the object allocation ceiling for the region */
			while (minimumLength < _objectAllocationCeiling[region]) {
				VM_AtomicSupport::lockCompareExchange(&_objectAllocationCeiling[region], _objectAllocationCeiling[region], minimumLength);
			}
		}
	}

#if defined(EVACUATOR_DEBUG)
	if ((NULL != whitespace) && _debugger.isDebugAllocate()) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(evacuator->getEnvironment());
		omrtty_printf("%5lu %2llu %2llu:%c allocate; %s; base:%llx; length:%llx; requested:%llx; required:%llx; remainder:%llx; epoch-ceiling:%llx; copyspace-ceiling:%llx, object-ceiling:%llx)\n",
				getEpoch()->gc, getEpoch()->epoch, evacuator->getWorkerIndex(), (inside ? 'I' : 'O'),
				((MM_Evacuator::survivor == region) ? "survivor" : "tenure"), (uintptr_t)whitespace, ((NULL != whitespace) ? whitespace->length() : 0),
				optimalSize, minimumLength, remainder, calculateOptimalWhitespaceSize(), _copyspaceAllocationCeiling[region], _objectAllocationCeiling[region]);
	}
	if ((NULL != whitespace) && _debugger.isDebugPoisonDiscard()) {
		MM_EvacuatorWhitespace::poison(whitespace);
	}
#endif /* defined(EVACUATOR_DEBUG) */

	return whitespace;
}

/* calculate the number of active words in the evacuator bitmaps */
uintptr_t
MM_EvacuatorController::countEvacuatorBitmapWords(uintptr_t *tailWords)
{
	*tailWords = (0 != (_evacuatorCount & index_to_map_word_modulus)) ? 1 : 0;
	return (_evacuatorCount >> index_to_map_word_shift) + *tailWords;
}

/* test evacuator bitmap for all 0s (reliable only when caller holds controller mutex) */
bool
MM_EvacuatorController::isEvacuatorBitmapEmpty(volatile uint64_t *bitmap)
{
	uintptr_t tailWords = 0;
	uintptr_t bitmapWords = countEvacuatorBitmapWords(&tailWords);
	for (uintptr_t map = 0; map < bitmapWords; map += 1) {
		if (0 != bitmap[map]) {
			return false;
		}
	}
	return true;
}

/* test evacuator bitmap for all 1s (reliable only when caller holds controller mutex) */
bool
MM_EvacuatorController::isEvacuatorBitmapFull(volatile uint64_t *bitmap)
{
	uintptr_t tailWords = 0;
	uintptr_t bitmapWords = countEvacuatorBitmapWords(&tailWords);
	uintptr_t fullWords = bitmapWords - tailWords;
	for (uintptr_t map = 0; map < fullWords; map += 1) {
		if (~(uint64_t)0 != bitmap[map]) {
			return false;
		}
	}
	if (0 < tailWords) {
		uint64_t tailBits = ((uint64_t)1 << (_evacuatorCount & index_to_map_word_modulus)) - 1;
		if (tailBits != bitmap[fullWords]) {
			return false;
		}
	}
	return true;
}

/* fill evacuator bitmap with all 1s (reliable only when caller holds controller mutex) */
void
MM_EvacuatorController::fillEvacuatorBitmap(volatile uint64_t * bitmap)
{
	uintptr_t tailWords = 0;
	uintptr_t bitmapWords = countEvacuatorBitmapWords(&tailWords);
	uintptr_t fullWords = bitmapWords - tailWords;
	for (uintptr_t map = 0; map < fullWords; map += 1) {
		bitmap[map] = ~(uint64_t)0;
	}
	if (0 < tailWords) {
		bitmap[fullWords] = ((uint64_t)1 << (_evacuatorCount & index_to_map_word_modulus)) - 1;
	}
}

/* set evacuator bit in evacuator bitmap */
uint64_t
MM_EvacuatorController::setEvacuatorBit(uintptr_t evacuatorIndex, volatile uint64_t *bitmap)
{
	uint64_t evacuatorMask = 0;
	uintptr_t evacuatorMap = mapEvacuatorIndexToMapAndMask(evacuatorIndex, &evacuatorMask);
	return VM_AtomicSupport::bitOrU64(&bitmap[evacuatorMap], evacuatorMask);
}

/* clear evacuator bit in evacuator bitmap */
void
MM_EvacuatorController::clearEvacuatorBit(uintptr_t evacuatorIndex, volatile uint64_t *bitmap)
{
	uint64_t evacuatorMask = 0;
	uintptr_t evacuatorMap = mapEvacuatorIndexToMapAndMask(evacuatorIndex, &evacuatorMask);
	VM_AtomicSupport::bitAndU64(&bitmap[evacuatorMap], ~evacuatorMask);
}

#if defined(EVACUATOR_DEBUG)
void
MM_EvacuatorController::printEvacuatorBitmap(MM_EnvironmentBase *env, const char *label, volatile uint64_t *bitmap)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	uintptr_t tailWords = 0;
	uintptr_t bitmapWords = countEvacuatorBitmapWords(&tailWords);
	omrtty_printf("%s:%llx", label, bitmap[0]);
	for (uintptr_t map = 1; map < bitmapWords; map += 1) {
		omrtty_printf(" %llx", bitmap[map]);
	}
}
#endif /* defined(EVACUATOR_DEBUG) */

