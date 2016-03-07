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


#include "omrcfg.h"
#include "omr.h"
#include "modronopt.h"

#include "ParallelTask.hpp"

#include "AtomicOperations.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"

#include "ModronAssertions.h"

bool
MM_ParallelTask::handleNextWorkUnit(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	if(1 == _totalThreadCount) {
		return true;
	}
	

	/* If all threads are currently being held by synchronization except the master, then
	 * only the master can make this call, and so should handle any and all work units.
	 * We don't want to advance the index of the master and have it get out of sync with
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
MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id)
{
	bool isMasterThread = false;

	Trc_MM_SynchronizeGCThreadsAndReleaseMaster_Entry(env->getLanguageVMThread(), id);
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
				"%s at %p from synchronizeGCThreadsAndReleaseMaster: call from (%s), expected (%s)\n", getBaseVirtualTypeId(), this, id, _syncPointUniqueId);
			Assert_GC_true_with_message4(env, _syncPointWorkUnitIndex == env->getWorkUnitIndex(),
				"%s at %p from synchronizeGCThreadsAndReleaseMaster: call with syncPointWorkUnitIndex %zu, expected %zu\n", getBaseVirtualTypeId(), this, env->getWorkUnitIndex(), _syncPointWorkUnitIndex);
		}

		_synchronizeCount += 1;
		if(_synchronizeCount == _threadCount) {
			if(env->isMasterThread()) {
				omrthread_monitor_exit(_synchronizeMutex);
				isMasterThread = true;
				_synchronized = true;
				goto done;
			}
			omrthread_monitor_notify_all(_synchronizeMutex);
		}

		while(index == _synchronizeIndex) {
			if(env->isMasterThread() && (_synchronizeCount == _threadCount)) {
				omrthread_monitor_exit(_synchronizeMutex);
				isMasterThread = true;
				_synchronized = true;
				goto done;
			}
			omrthread_monitor_wait(_synchronizeMutex);
		}
		omrthread_monitor_exit(_synchronizeMutex);
	} else {
		_synchronized = true;
		isMasterThread = true;
	}

done:
	Trc_MM_SynchronizeGCThreadsAndReleaseMaster_Exit(env->getLanguageVMThread());
	return isMasterThread;	
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
	omrthread_monitor_notify_all(_synchronizeMutex);
	omrthread_monitor_exit(_synchronizeMutex);
}

void
MM_ParallelTask::complete(MM_EnvironmentBase *env)
{
	const char *id = UNIQUE_ID;

	/* Update this slave thread's CPU time */
	if(!env->isMasterThread()) {
		env->_slaveThreadCpuTimeNanos = omrthread_get_self_cpu_time(env->getOmrVMThread()->_os_thread);
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
	
		if(env->isMasterThread()) {
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
MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id, uint64_t *stallTime)
{
	uint64_t startTime, endTime;
	bool result;
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	startTime = omrtime_hires_clock();

	result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster(env, id);

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

