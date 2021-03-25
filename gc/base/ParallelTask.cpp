/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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


#include "omrcfg.h"
#include "omr.h"
#include "modronopt.h"

#include "ParallelTask.hpp"

#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "ModronAssertions.h"
#include "ParallelDispatcher.hpp"

bool
MM_ParallelTask::handleNextWorkUnit(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	if(1 == _totalThreadCount) {
		return true;
	}
	

	/* If all threads are currently being held by synchronization except the main, then
	 * only the main can make this call, and so should handle any and all work units.
	 * We don't want to advance the index of the main and have it get out of sync with
	 * all other threads.
	 */
	if(_synchronized) {
		return true;
	}

	uintptr_t envWorkUnitToHandle = env->getWorkUnitToHandle();
	uintptr_t envWorkUnitIndex = env->nextWorkUnitIndex();

	if(envWorkUnitIndex > envWorkUnitToHandle) {
		envWorkUnitToHandle = MM_AtomicOperations::add(&_workUnitIndex, 1);
		env->setWorkUnitToHandle(envWorkUnitToHandle);

		if (extensions->_holdRandomThreadBeforeHandlingWorkUnit) {
			if (0 == (rand() % extensions->_holdRandomThreadBeforeHandlingWorkUnitPeriod)) {
				Trc_MM_ParallelTask_handleNextWorkUnit_holdingThread(env->getLanguageVMThread(), env->getWorkUnitIndex(), env->_lastSyncPointReached);
				omrthread_sleep(10);
			}
		}
	}

	return envWorkUnitIndex == envWorkUnitToHandle;
}

void
MM_ParallelTask::synchronizeGCThreads(MM_EnvironmentBase *env, const char *id)
{
	Trc_MM_SynchronizeGCThreads_Entry(env->getLanguageVMThread(), id);
	env->_lastSyncPointReached = id;
	
	if(1 < _totalThreadCount) {
		omrthread_monitor_enter(_synchronizeMutex);

		/*check synchronization point*/
		if (0 == _synchronizeCount) {
			_syncPointUniqueId = id;
			_syncPointWorkUnitIndex = env->getWorkUnitIndex();
		} else {
			Assert_GC_true_with_message4(env, _syncPointUniqueId == id,
				"%s at %p from synchronizeGCThreads: call from (%s), expected (%s)\n", getBaseVirtualTypeId(), this, id, _syncPointUniqueId);
			Assert_GC_true_with_message4(env, _syncPointWorkUnitIndex == env->getWorkUnitIndex(),
				"%s at %p from synchronizeGCThreads: call with syncPointWorkUnitIndex %zu, expected %zu\n", getBaseVirtualTypeId(), this, env->getWorkUnitIndex(), _syncPointWorkUnitIndex);
		}

		_synchronizeCount += 1;

		if(_synchronizeCount == _threadCount) {
			_synchronizeCount = 0;
			_synchronizeIndex += 1;
			omrthread_monitor_notify_all(_synchronizeMutex);
		} else {
			volatile uintptr_t index = _synchronizeIndex;

			do {
				omrthread_monitor_wait(_synchronizeMutex);
			} while(index == _synchronizeIndex);
		}
		omrthread_monitor_exit(_synchronizeMutex);

	}

	Trc_MM_SynchronizeGCThreads_Exit(env->getLanguageVMThread());
}

bool
MM_ParallelTask::synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *env, const char *id)
{
	bool isMainThread = false;

	Trc_MM_SynchronizeGCThreadsAndReleaseMain_Entry(env->getLanguageVMThread(), id);
	env->_lastSyncPointReached = id;

	if(1 < _totalThreadCount) {
		volatile uintptr_t index = _synchronizeIndex;

		omrthread_monitor_enter(_synchronizeMutex);

		/*check synchronization point*/
		if (0 == _synchronizeCount) {
			_syncPointUniqueId = id;
			_syncPointWorkUnitIndex = env->getWorkUnitIndex();
		} else {
			Assert_GC_true_with_message4(env, _syncPointUniqueId == id,
				"%s at %p from synchronizeGCThreadsAndReleaseMain: call from (%s), expected (%s)\n", getBaseVirtualTypeId(), this, id, _syncPointUniqueId);
			Assert_GC_true_with_message4(env, _syncPointWorkUnitIndex == env->getWorkUnitIndex(),
				"%s at %p from synchronizeGCThreadsAndReleaseMain: call with syncPointWorkUnitIndex %zu, expected %zu\n", getBaseVirtualTypeId(), this, env->getWorkUnitIndex(), _syncPointWorkUnitIndex);
		}

		_synchronizeCount += 1;
		if(_synchronizeCount == _threadCount) {
			if(env->isMainThread()) {
				omrthread_monitor_exit(_synchronizeMutex);
				isMainThread = true;
				_synchronized = true;
				goto done;
			}
			omrthread_monitor_notify_all(_synchronizeMutex);
		}

		while(index == _synchronizeIndex) {
			if(env->isMainThread() && (_synchronizeCount == _threadCount)) {
				omrthread_monitor_exit(_synchronizeMutex);
				isMainThread = true;
				_synchronized = true;
				goto done;
			}
			omrthread_monitor_wait(_synchronizeMutex);
		}
		omrthread_monitor_exit(_synchronizeMutex);
	} else {
		_synchronized = true;
		isMainThread = true;
	}

done:
	Trc_MM_SynchronizeGCThreadsAndReleaseMain_Exit(env->getLanguageVMThread());
	return isMainThread;	
}

bool
MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id)
{
	bool isReleasedThread = false;

	Trc_MM_SynchronizeGCThreadsAndReleaseSingleThread_Entry(env->getLanguageVMThread(), id);
	env->_lastSyncPointReached = id;

	if(1 < _totalThreadCount) {
		volatile uintptr_t index = _synchronizeIndex;
		uintptr_t workUnitIndex = env->getWorkUnitIndex();

		omrthread_monitor_enter(_synchronizeMutex);

		/*check synchronization point*/
		if (0 == _synchronizeCount) {
			_syncPointUniqueId = id;
			_syncPointWorkUnitIndex = workUnitIndex;
		} else {
			Assert_GC_true_with_message4(env, _syncPointUniqueId == id,
				"%s at %p from synchronizeGCThreadsAndReleaseSingleThread: call from (%s), expected (%s)\n", getBaseVirtualTypeId(), this, id, _syncPointUniqueId);
			Assert_GC_true_with_message4(env, _syncPointWorkUnitIndex == env->getWorkUnitIndex(),
				"%s at %p from synchronizeGCThreadsAndReleaseSingleThread: call with syncPointWorkUnitIndex %zu, expected %zu\n", getBaseVirtualTypeId(), this, env->getWorkUnitIndex(), _syncPointWorkUnitIndex);
		}

		_synchronizeCount += 1;
		if(_synchronizeCount == _threadCount) {
			omrthread_monitor_exit(_synchronizeMutex);
			isReleasedThread = true;
			_synchronized = true;
			goto done;
		}

		do {
			omrthread_monitor_wait(_synchronizeMutex);
		} while(index == _synchronizeIndex);
		omrthread_monitor_exit(_synchronizeMutex);
	} else {
		_synchronized = true;
		isReleasedThread = true;
	}

done:
	Trc_MM_SynchronizeGCThreadsAndReleaseSingleThread_Exit(env->getLanguageVMThread());
	return isReleasedThread;
}

void
MM_ParallelTask::releaseSynchronizedGCThreads(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	if (_syncCriticalSectionStartTime != 0) {
		/* Critical section complete, synced threads are about to be released. Record the duration. */
		_syncCriticalSectionDuration = (omrtime_hires_clock() - _syncCriticalSectionStartTime);
		_syncCriticalSectionStartTime = 0;
	}

	if (1 == _totalThreadCount) {
		_synchronized = false;
		return;
	}
	
	Assert_GC_true_with_message2(env, _synchronized, "%s at %p from releaseSynchronizedGCThreads: call for non-synchronized\n", getBaseVirtualTypeId(), this);
	/* Could not have gotten here unless all other threads are sync'd - don't check, just release */
	_synchronized = false;
	omrthread_monitor_enter(_synchronizeMutex);
	_synchronizeCount = 0;
	_synchronizeIndex += 1;
	uint64_t notifyStartTime = omrtime_hires_clock();
	omrthread_monitor_notify_all(_synchronizeMutex);

	addToNotifyStallTime(env, notifyStartTime, omrtime_hires_clock());

	omrthread_monitor_exit(_synchronizeMutex);
}

void
MM_ParallelTask::complete(MM_EnvironmentBase *env)
{
	const char *id = UNIQUE_ID;

	/* Update this worker thread's CPU time */
	if(!env->isMainThread()) {
		env->_workerThreadCpuTimeNanos = omrthread_get_self_cpu_time(env->getOmrVMThread()->_os_thread);
	}

	if(1 == _totalThreadCount) {
		_threadCount -= 1;
		MM_Task::complete(env);
		
	} else {
		omrthread_monitor_enter(_synchronizeMutex);

		if (0 == _synchronizeCount) {
			_syncPointUniqueId = id;
			_syncPointWorkUnitIndex = env->getWorkUnitIndex();
		} else {
			Assert_GC_true_with_message3(env, _syncPointUniqueId == id,
				"%s at %p from complete: reach end of the task however threads are waiting at (%s)\n", getBaseVirtualTypeId(), this, _syncPointUniqueId);
			/*
			 * MM_ParallelScrubCardTableTask is implemented to be aborted if it takes too much time
			 * An abortion left work unit counters in unpredictable state so it would trigger this assertion
			 * Temporary commenting out
			 */
			/*
			Assert_GC_true_with_message4(env, _syncPointWorkUnitIndex == env->getWorkUnitIndex(),
				"%s at %p from complete: call with syncPointWorkUnitIndex %zu, expected %zu\n", getBaseVirtualTypeId(), this, env->getWorkUnitIndex(), _syncPointWorkUnitIndex);
			*/
		}

		_synchronizeCount += 1;
		_threadCount -= 1;
	
		MM_Task::complete(env);
	
		if(env->isMainThread()) {
			/* Synchronization on exit - cannot delete the task object until all threads are done with it */
			while(0 != _threadCount) {
				omrthread_monitor_wait(_synchronizeMutex);
			}
		} else {
			if(0 == _threadCount) {
				omrthread_monitor_notify_all(_synchronizeMutex);
			}
		}
		omrthread_monitor_exit(_synchronizeMutex);
	}
}

/**
 * Return true if threads are currently syncronized, false otherwise
 * @return true if threads are currently syncronized, false otherwise
 */
bool 
MM_ParallelTask::isSynchronized()
{
	return _synchronized;
}

bool
MM_ParallelTask::synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *env, const char *id, uint64_t *stallTime)
{
	uint64_t startTime, endTime;
	bool result;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	startTime = omrtime_hires_clock();

	result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMain(env, id);

	endTime = omrtime_hires_clock();
	*stallTime += (endTime - startTime);

	return result;
}

void
MM_ParallelTask::synchronizeGCThreads(MM_EnvironmentBase *env, const char *id, uint64_t *stallTime)
{
	uint64_t startTime, endTime;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	startTime = omrtime_hires_clock();

	MM_ParallelTask::synchronizeGCThreads(env, id);

	endTime = omrtime_hires_clock();

	*stallTime += (endTime - startTime);
}

