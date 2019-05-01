/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "modronopt.h"
#include "ModronAssertions.h"
#include "omrport.h"

#include "MasterGCThread.hpp"

#include "CollectorLanguageInterfaceImpl.hpp"
#include "ConcurrentGMPStats.hpp"
#include "ConcurrentPhaseStatsBase.hpp"
#include "ParallelDispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "Collector.hpp"

/**
 * Initialization
 */
MM_MasterGCThread::MM_MasterGCThread(MM_EnvironmentBase *env)
	: MM_BaseNonVirtual()
	, _collectorControlMutex(NULL)
	, _masterThreadState(STATE_ERROR)
	, _masterGCThread(NULL)
	, _incomingCycleState(NULL)
	, _allocDesc(NULL)
	, _extensions(env->getExtensions())
	, _collector(NULL)
	, _runAsImplicit(false)
	, _acquireVMAccessDuringConcurrent(false)
{
	_typeId = __FUNCTION__;
}

uintptr_t
MM_MasterGCThread::master_thread_proc2(OMRPortLibrary* portLib, void *info)
{
	MM_MasterGCThread *masterGCThread = (MM_MasterGCThread*)info;
	/* jump into the master thread procedure and wait for work.  This method will NOT return */
	masterGCThread->masterThreadEntryPoint();
	Assert_MM_unreachable();
	return 0;
}

int J9THREAD_PROC
MM_MasterGCThread::master_thread_proc(void *info)
{
	MM_MasterGCThread *masterGCThread = (MM_MasterGCThread*)info;
	MM_GCExtensionsBase *extensions = masterGCThread->_extensions;
	OMR_VM *omrVM = extensions->getOmrVM();
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	uintptr_t rc = 0;
	omrsig_protect(master_thread_proc2, info,
			((MM_ParallelDispatcher *)extensions->dispatcher)->getSignalHandler(), omrVM,
		OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&rc);
	return 0;
}



bool
MM_MasterGCThread::initialize(MM_Collector *collector, bool runAsImplicit, bool acquireVMAccessDuringConcurrent)
{
	bool success = true;
	if(omrthread_monitor_init_with_name(&_collectorControlMutex, 0, "MM_MasterGCThread::_collectorControlMutex")) {
		success = false;
	}

	_collector = collector;
	_runAsImplicit = runAsImplicit;
	_acquireVMAccessDuringConcurrent = acquireVMAccessDuringConcurrent;

	return success;
}

void
MM_MasterGCThread::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _collectorControlMutex) {
		omrthread_monitor_destroy(_collectorControlMutex);
		_collectorControlMutex = NULL;
	}
	_collector = NULL;
}


bool
MM_MasterGCThread::startup()
{
	/* set the success flag to false and we will set it true if everything succeeds */
	bool success = false;

	if (_extensions->fvtest_disableExplictMasterThread) {
		/* GC should be able to act even if master thread is not created (or late) */
		_masterThreadState = STATE_DISABLED;
		success = true;
	} else {
		/* hold the monitor over start-up of this thread so that we eliminate any timing hole where it might notify us of its start-up state before we wait */
		omrthread_monitor_enter(_collectorControlMutex);
		_masterThreadState = STATE_STARTING;
		intptr_t forkResult = createThreadWithCategory(
			NULL,
			OMR_OS_STACK_SIZE,
			J9THREAD_PRIORITY_NORMAL,
			0,
			master_thread_proc,
			this,
			J9THREAD_CATEGORY_SYSTEM_GC_THREAD);
		if (forkResult == 0) {
			/* thread creation success */
			/* wait to find out if they started up, successfully */
			while (STATE_STARTING == _masterThreadState) {
				omrthread_monitor_wait(_collectorControlMutex);
			}
			if (STATE_ERROR != _masterThreadState) {
				/* the master thread managed to start up and is in the waiting state, ready for GC requests */
				success = true;
			}
		} else {
			_masterThreadState = STATE_ERROR;
		}
		omrthread_monitor_exit(_collectorControlMutex);
	}

	return success;
}

void
MM_MasterGCThread::shutdown()
{
	Assert_MM_true(NULL != _collectorControlMutex);
	if ((STATE_ERROR != _masterThreadState) && (STATE_DISABLED != _masterThreadState)) {
		/* tell the background thread to shut down and then wait for it to exit */
		omrthread_monitor_enter(_collectorControlMutex);
		while(STATE_TERMINATED != _masterThreadState) {
			_masterThreadState = STATE_TERMINATION_REQUESTED;
			omrthread_monitor_notify(_collectorControlMutex);
			omrthread_monitor_wait(_collectorControlMutex);
		}
		omrthread_monitor_exit(_collectorControlMutex);
		
		/* don't NULL _collector as RAS could still trigger a collection after we've started shutting down */
	}
}


void
MM_MasterGCThread::handleSTW(MM_EnvironmentBase *env)
{
	Assert_MM_true(NULL != _incomingCycleState);
	env->_cycleState = _incomingCycleState;

	/* this thread effectively inherits exclusive access from the mutator thread -- set its state to indicate this */
	env->assumeExclusiveVMAccess(1);

	_collector->masterThreadGarbageCollect(env, _allocDesc);

	uintptr_t exclusiveCount = env->relinquishExclusiveVMAccess();
	Assert_MM_true(1 == exclusiveCount);

	env->_cycleState = NULL;
	_incomingCycleState = NULL;
	_masterThreadState = STATE_WAITING;
	omrthread_monitor_notify(_collectorControlMutex);
}

bool
MM_MasterGCThread::handleConcurrent(MM_EnvironmentBase *env)
{
	bool workDone = false;

	_masterThreadState = STATE_RUNNING_CONCURRENT;

	do {
		if (_acquireVMAccessDuringConcurrent) {
			omrthread_monitor_exit(_collectorControlMutex);
			env->acquireVMAccess();
		}
		if (_collector->isConcurrentWorkAvailable(env)) {
			MM_ConcurrentPhaseStatsBase *stats = _collector->getConcurrentPhaseStats();
			stats->clear();

			_collector->preConcurrentInitializeStatsAndReport(env, stats);
			if (!_acquireVMAccessDuringConcurrent) {
				omrthread_monitor_exit(_collectorControlMutex);
			}
			uintptr_t bytesConcurrentlyScanned = _collector->masterThreadConcurrentCollect(env);

			if (!_acquireVMAccessDuringConcurrent) {
				omrthread_monitor_enter(_collectorControlMutex);
			}

			_collector->postConcurrentUpdateStatsAndReport(env, stats, bytesConcurrentlyScanned);
			workDone = true;
		}

		if (_acquireVMAccessDuringConcurrent) {
			env->releaseVMAccess();
			omrthread_monitor_enter(_collectorControlMutex);
		}
	} while (_collector->getConcurrentPhaseStats()->isTerminationRequestExternal() && _collector->isConcurrentWorkAvailable(env));

	if (STATE_RUNNING_CONCURRENT == _masterThreadState) {
		_masterThreadState = STATE_WAITING;
	}

	return workDone;
}

void
MM_MasterGCThread::masterThreadEntryPoint()
{
	OMR_VMThread *omrVMThread = NULL;
	Assert_MM_true(NULL != _collectorControlMutex);
	Assert_MM_true(NULL == _masterGCThread);

	/* Attach the thread as a system daemon thread */	
	/* You need a VM thread so that the stack walker can work */
	omrVMThread = MM_EnvironmentBase::attachVMThread(_extensions->getOmrVM(), "Dedicated GC Master", MM_EnvironmentBase::ATTACH_GC_MASTER_THREAD);
	if (NULL == omrVMThread) {
		/* we failed to attach so notify the creating thread that we should fail to start up */
		omrthread_monitor_enter(_collectorControlMutex);
		_masterThreadState = STATE_ERROR;
		omrthread_monitor_notify(_collectorControlMutex);
		omrthread_exit(_collectorControlMutex);
	} else {
		/* thread attached successfully */
		MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(omrVMThread);

		/* attachVMThread could allocate an execute a barrier (since it that point, this thread acted as a mutator thread.
		 * Flush GC chaches (like barrier buffers) before turning into the master thread */
		env->flushGCCaches();

		env->setThreadType(GC_MASTER_THREAD);

		/* Begin running the thread */
		omrthread_monitor_enter(_collectorControlMutex);
		
		_collector->preMasterGCThreadInitialize(env);
		
		_masterThreadState = STATE_WAITING;
		_masterGCThread = omrthread_self();
		omrthread_monitor_notify(_collectorControlMutex);
		do {
			if (STATE_GC_REQUESTED == _masterThreadState) {
				if (_runAsImplicit) {
					handleConcurrent(env);
				} else {
					handleSTW(env);
				}
			}

			if (STATE_WAITING == _masterThreadState) {
				if (_runAsImplicit || !handleConcurrent(env)) {
					omrthread_monitor_wait(_collectorControlMutex);
				}
			}
		} while (STATE_TERMINATION_REQUESTED != _masterThreadState);
		/* notify the other side that we are active so that they can continue running */
		_masterThreadState = STATE_TERMINATED;
		_masterGCThread = NULL;
		omrthread_monitor_notify(_collectorControlMutex);
		MM_EnvironmentBase::detachVMThread(_extensions->getOmrVM(), omrVMThread, MM_EnvironmentBase::ATTACH_GC_MASTER_THREAD);
		omrthread_exit(_collectorControlMutex);
	}
}

bool
MM_MasterGCThread::garbageCollect(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription)
{
	Assert_MM_mustHaveExclusiveVMAccess(env->getOmrVMThread());
	bool didAttemptCollect = false;
	
	if (NULL != _collector) {
		/* the collector has started up so try to run */
		/* once the master thread has stored itself in the _masterGCThread, it should never need to collect - this would hang */
		Assert_MM_true(omrthread_self() != _masterGCThread);
		if (_runAsImplicit || (NULL == _masterGCThread)) {
			/* We might not have _masterGCThread in the startup phase or late in the shutdown phase.
			 * For example, there may be a native out-of-memory during startup or RAS may 
			 * trigger a GC after we've shutdown the master thread.
			 */
			Assert_MM_true(0 == env->getSlaveID());
			_collector->preMasterGCThreadInitialize(env);
			_collector->masterThreadGarbageCollect(env, allocDescription);

			if (_runAsImplicit && _collector->isConcurrentWorkAvailable(env)) {
				omrthread_monitor_enter(_collectorControlMutex);

				if (STATE_WAITING == _masterThreadState) {
					_masterThreadState = STATE_GC_REQUESTED;
					omrthread_monitor_notify(_collectorControlMutex);
				}

				omrthread_monitor_exit(_collectorControlMutex);
			}
		} else {
			/* this is the general case, when the master thread is running internally */
			omrthread_monitor_enter(_collectorControlMutex);
			/* The variable assignments below are safe because we hold Xaccess.  Otherwise, it is possible (based on the wait/notify mechanism here)
			 * that another thread could come in under this mutex and stomp on the "parameters" while another thread is waiting.
			 */
			_allocDesc = allocDescription;
			_incomingCycleState = env->_cycleState;
			MasterGCThreadState previousState = _masterThreadState;
			_masterThreadState = STATE_GC_REQUESTED;
			if (STATE_WAITING == previousState) {
				omrthread_monitor_notify(_collectorControlMutex);
			} else if (STATE_RUNNING_CONCURRENT == previousState) {
				_collector->forceConcurrentFinish();
			} else {
				Assert_MM_unreachable();
			}
			
			/* The master thread will claim exclusive VM access. Artificially give it up in this thread so that tools like -Xcheck:vm continue to work. */
			uintptr_t savedExclusiveCount = env->relinquishExclusiveVMAccess();
			while (STATE_GC_REQUESTED == _masterThreadState) {
				omrthread_monitor_wait(_collectorControlMutex);
			}
			env->assumeExclusiveVMAccess(savedExclusiveCount);

			Assert_MM_true(NULL == _incomingCycleState);
			omrthread_monitor_exit(_collectorControlMutex);
		}
		
		didAttemptCollect = true;
	}
	return didAttemptCollect;
}
