/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/


/**
 * @file
 * @ingroup GC_Base
 */

#include "omrcfg.h"
#include "omr.h"
#include "ModronAssertions.h"
#include "ut_j9mm.h"

#include "Collector.hpp"
#include "CollectorLanguageInterfaceImpl.hpp"
#include "Configuration.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "Task.hpp"

#include "ParallelDispatcher.hpp"

typedef struct workerThreadInfo {
	OMR_VM *omrVM;
	uintptr_t workerID;
	uintptr_t workerFlags;
	MM_ParallelDispatcher *dispatcher;
} workerThreadInfo;

#define WORKER_INFO_FLAG_OK 1
#define WORKER_INFO_FLAG_FAILED 2

#define MINIMUM_HEAP_PER_THREAD (2*1024*1024)

uintptr_t
dispatcher_thread_proc2(OMRPortLibrary* portLib, void *info)
{
	workerThreadInfo *workerInfo = (workerThreadInfo *)info;
	OMR_VM *omrVM = workerInfo->omrVM;
	OMR_VMThread *omrVMThread = NULL;
	uintptr_t workerID = 0;
	MM_ParallelDispatcher *dispatcher = workerInfo->dispatcher;
	MM_EnvironmentBase *env = NULL;
	uintptr_t oldVMState = 0;

	/* Cache values from the info before releasing it */
	workerID = workerInfo->workerID;

	/* Attach the thread as a system daemon thread */
	omrVMThread = MM_EnvironmentBase::attachVMThread(omrVM, "GC Worker", MM_EnvironmentBase::ATTACH_GC_DISPATCHER_THREAD);
	if (NULL == omrVMThread) {
		goto startup_failed;
	}

	env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	env->setWorkerID(workerID);
	/* Enviroment initialization specific for GC threads (after worker ID is set) */
	env->initializeGCThread();

	/* Signal that the thread was created succesfully */
	workerInfo->workerFlags = WORKER_INFO_FLAG_OK;

	oldVMState = env->pushVMstate(OMRVMSTATE_GC_DISPATCHER_IDLE);

	/* Begin running the thread */
	if (env->isMainThread()) {
		env->setThreadType(GC_MAIN_THREAD);
		dispatcher->mainEntryPoint(env);
		env->setThreadType(GC_WORKER_THREAD);
	} else {
		env->setThreadType(GC_WORKER_THREAD);
		dispatcher->workerEntryPoint(env);
	}

	env->popVMstate(oldVMState);

	/* Thread is terminating -- shut it down */
	env->setWorkerID(0);
	MM_EnvironmentBase::detachVMThread(omrVM, omrVMThread, MM_EnvironmentBase::ATTACH_GC_DISPATCHER_THREAD);
	
	omrthread_monitor_enter(dispatcher->_dispatcherMonitor);
	dispatcher->_threadShutdownCount -= 1;
	omrthread_monitor_notify(dispatcher->_dispatcherMonitor);
	/* Terminate the thread */
	omrthread_exit(dispatcher->_dispatcherMonitor);

	Assert_MM_unreachable();
	return 0;

startup_failed:
	workerInfo->workerFlags = WORKER_INFO_FLAG_FAILED;

	omrthread_monitor_enter(dispatcher->_dispatcherMonitor);
	omrthread_monitor_notify_all(dispatcher->_dispatcherMonitor);
	omrthread_exit(dispatcher->_dispatcherMonitor);

	Assert_MM_unreachable();
	return 0;
}

extern "C" {

int J9THREAD_PROC
dispatcher_thread_proc(void *info)
{
	OMR_VM *omrVM = ((workerThreadInfo *)info)->omrVM;
	MM_ParallelDispatcher *dispatcher = ((workerThreadInfo *)info)->dispatcher;
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);
	uintptr_t rc;
	omrsig_protect(dispatcher_thread_proc2, info,
		dispatcher->getSignalHandler(), dispatcher->getSignalHandlerArg(),
		OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
		&rc);
	return 0;
}

} /* extern "C" */


/**
 * Run the main loop for a fully-constructed worker thread.
 * Subclasses can override this to have their own method of controlling worker
 * threads. They should keep the basic pattern of:
 * <code>
 * acceptTask(env); 
 * env->_currentTask->run(env); 
 * completeTask(env);
 * </code>
 */
void
MM_ParallelDispatcher::workerEntryPoint(MM_EnvironmentBase *env) 
{
	uintptr_t workerID = env->getWorkerID();
	
	setThreadInitializationComplete(env);
	
	omrthread_monitor_enter(_workerThreadMutex);

	while (worker_status_dying != _statusTable[workerID]) {
		/* Wait for a task to be dispatched to the worker thread */
		while (worker_status_waiting == _statusTable[workerID]) {
			if (_workerThreadsReservedForGC && (_threadsToReserve > 0)) {
				_threadsToReserve -= 1;
				_statusTable[workerID] = worker_status_reserved;
				_taskTable[workerID] = _task;
			} else {
				omrthread_monitor_wait(_workerThreadMutex);
			}
		}

		/* Assert dispatcher and thread states. In most cases, a thread exits during task dispatch (_workerThreadsReservedForGC) with a reserved status. */
		if (_workerThreadsReservedForGC) {
			/* Thread can only break out while being reserved, except in the rare case that a task is dispatched during shutdown.
			 * In which case, _workerThreadsReservedForGC is set but the task runs single threaded and exiting threads are dying instead of reserved. */
			Assert_MM_true((worker_status_reserved == _statusTable[workerID]) || ((0 == _threadsToReserve) && (worker_status_dying == _statusTable[workerID])));
		} else {
			/* If there is no task dispatched and thread exited, then we must be in shutdown */
			Assert_MM_true(_inShutdown && (worker_status_dying == _statusTable[workerID]));
		}

		if (worker_status_reserved == _statusTable[workerID]) {
			/* Found a task to dispatch to - do prep work for dispatch */
			acceptTask(env);
			omrthread_monitor_exit(_workerThreadMutex);	

			env->_currentTask->run(env);

			omrthread_monitor_enter(_workerThreadMutex);
			/* Returned from task - do clean up work from dispatch */
			completeTask(env);
		}
	}
	omrthread_monitor_exit(_workerThreadMutex);	
}

void
MM_ParallelDispatcher::mainEntryPoint(MM_EnvironmentBase *env)
{
	/* The default implementation is to not start a separate
	 * main thread, but any subclasses that override 
	 * useSeparateMainThread() must also override this method.
	 */
	assume0(0);
}

MM_ParallelDispatcher *
MM_ParallelDispatcher::newInstance(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize)
{
	MM_ParallelDispatcher *dispatcher;
	
	dispatcher = (MM_ParallelDispatcher *)env->getForge()->allocate(sizeof(MM_ParallelDispatcher), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (dispatcher) {
		new(dispatcher) MM_ParallelDispatcher(env, handler, handler_arg, defaultOSStackSize);
		if (!dispatcher->initialize(env)) {
			dispatcher->kill(env);
			return NULL;
		}
	}
	return dispatcher;
}

void
MM_ParallelDispatcher::kill(MM_EnvironmentBase *env)
{
	OMR::GC::Forge *forge = env->getForge();

	if (NULL != _workerThreadMutex) {
		omrthread_monitor_destroy(_workerThreadMutex);
		_workerThreadMutex = NULL;
	}

	if (NULL != _dispatcherMonitor) {
		omrthread_monitor_destroy(_dispatcherMonitor);
		_dispatcherMonitor = NULL;
	}

	if (NULL != _synchronizeMutex) {
		omrthread_monitor_destroy(_synchronizeMutex);
		_synchronizeMutex = NULL;
	}

	if (NULL != _taskTable) {
		forge->free(_taskTable);
		_taskTable = NULL;
	}

	if (NULL != _statusTable) {
		forge->free(_statusTable);
		_statusTable = NULL;
	}

	if (NULL != _threadTable) {
		forge->free(_threadTable);
		_threadTable = NULL;
	}

	env->getForge()->free(this);
}

bool
MM_ParallelDispatcher::initialize(MM_EnvironmentBase *env)
{
	OMR::GC::Forge *forge = env->getForge();

	_threadCountMaximum = _extensions->gcThreadCount;

#if defined(J9VM_OPT_CRIU_SUPPORT)
	_poolMaxCapacity = _threadCountMaximum;
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

	Assert_MM_true(0 < _threadCountMaximum);

	if(omrthread_monitor_init_with_name(&_workerThreadMutex, 0, "MM_ParallelDispatcher::workerThread")
	|| omrthread_monitor_init_with_name(&_dispatcherMonitor, 0, "MM_ParallelDispatcher::dispatcherControl")
	|| omrthread_monitor_init_with_name(&_synchronizeMutex, 0, "MM_ParallelDispatcher::synchronize")) {
		goto error_no_memory;
	}

	/* Initialize the thread tables */
	_threadTable = (omrthread_t *)forge->allocate(_threadCountMaximum * sizeof(omrthread_t), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _threadTable) {
		goto error_no_memory;
	}
	memset(_threadTable, 0, _threadCountMaximum * sizeof(omrthread_t));

	_statusTable = (uintptr_t *)forge->allocate(_threadCountMaximum * sizeof(uintptr_t *), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _statusTable) {
		goto error_no_memory;
	}
	memset(_statusTable, 0, _threadCountMaximum * sizeof(uintptr_t *));

	_taskTable = (MM_Task **)forge->allocate(_threadCountMaximum * sizeof(MM_Task *), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _taskTable) {
		goto error_no_memory;
	}
	memset(_taskTable, 0, _threadCountMaximum * sizeof(MM_Task *));

	return true;

error_no_memory:
	return false;
}

bool
MM_ParallelDispatcher::startUpThreads()
{
	_threadShutdownCount = 0;

	/* The main thread may concurrently start at the same time. */
	uintptr_t workerThreadCount = useSeparateMainThread() ? 0 : 1;

	bool result = internalStartupThreads(workerThreadCount, _threadCountMaximum);

	if (result) {
		_threadCount = _threadCountMaximum;
		_activeThreadCount = adjustThreadCount(_threadCount);
	}

	return result;
}

bool
MM_ParallelDispatcher::internalStartupThreads(uintptr_t workerThreadCount, uintptr_t maxWorkerThreadIndex)
{
	intptr_t threadForkResult;
	workerThreadInfo workerInfo;

	/* Fork the worker threads */
	workerInfo.omrVM = _extensions->getOmrVM();
	workerInfo.dispatcher = this;

	omrthread_monitor_enter(_dispatcherMonitor);

	while (workerThreadCount < maxWorkerThreadIndex) {
		workerInfo.workerFlags = 0;
		workerInfo.workerID = workerThreadCount;

		Assert_MM_true(NULL == _threadTable[workerThreadCount]);
		Assert_MM_true(worker_status_inactive == _statusTable[workerThreadCount]);

		threadForkResult =
			createThreadWithCategory(
				&(_threadTable[workerThreadCount]),
				_defaultOSStackSize,
				getThreadPriority(),
				0,
				dispatcher_thread_proc,
				(void *)&workerInfo,
				J9THREAD_CATEGORY_SYSTEM_GC_THREAD);
		if (threadForkResult != 0) {
			/* Thread creation failed - for safety sake, set the shutdown flag to true */
			goto error;
		}
		do {
			if(_inShutdown) {
				goto error;
			}
			omrthread_monitor_wait(_dispatcherMonitor);
		} while (!workerInfo.workerFlags);

		if (workerInfo.workerFlags != WORKER_INFO_FLAG_OK ) {
			goto error;
		}

		_threadShutdownCount += 1;
		workerThreadCount += 1;
	}

	omrthread_monitor_exit(_dispatcherMonitor);

	return true;

error:
	/* exit from monitor */
	omrthread_monitor_exit(_dispatcherMonitor);

	Trc_MM_ParallelDispatcher_internalStartupThreads_Failed(workerThreadCount, maxWorkerThreadIndex, _threadShutdownCount);

	return false;
}

void
MM_ParallelDispatcher::shutDownThreads()
{
	_inShutdown = true;

	omrthread_monitor_enter(_dispatcherMonitor);
	omrthread_monitor_notify_all(_dispatcherMonitor);
	omrthread_monitor_exit(_dispatcherMonitor);

	/* TODO: Shutdown should account for threads that failed to start up in the first place */
	omrthread_monitor_enter(_workerThreadMutex);

	while (_workerThreadsReservedForGC) {
		omrthread_monitor_wait(_workerThreadMutex);
	}

	/* Set the worker thread mode to dying */
	for (uintptr_t index = 0; index < _threadCountMaximum; index++) {
		_statusTable[index] = worker_status_dying;
	}

	/* Set the active parallel thread count to 1. This allows
	 * worker threads to cause a GC during their detach,
	 * making them the main in a single threaded GC.
	 */
	_threadCount = 1;

	wakeUpThreads(_threadShutdownCount);
	omrthread_monitor_exit(_workerThreadMutex);

	omrthread_monitor_enter(_dispatcherMonitor);
	while (0 != _threadShutdownCount) {
		omrthread_monitor_wait(_dispatcherMonitor);
	}
	omrthread_monitor_exit(_dispatcherMonitor);
}

/**
 * Wake up at least the first <code>count</code> worker threads.
 * In this implementation, since workerThreadEntryPoint() allows a thread to
 * go back to sleep if it wasn't selected, we can wake them all up. This
 * may not apply to all subclasses though.
 */
void
MM_ParallelDispatcher::wakeUpThreads(uintptr_t count)
{
	/* This thread should notify and release _workerThreadMutex asap. Threads waking up will need to
	 * reacquire the mutex before proceeding with the task.
	 *
	 * Hybrid approach to notifying threads:
	 * 	- Cheaper to do to individual notifies for small set of threads from a large pool
	 * 	- More expensive to do with individual notifies with large set of threads
	 */
	if (count < OMR_MIN((_threadCountMaximum / 2), _extensions->dispatcherHybridNotifyThreadBound)) {
		for (uintptr_t threads = 0; threads < count; threads++) {
			omrthread_monitor_notify(_workerThreadMutex);
		}
	} else {
		omrthread_monitor_notify_all(_workerThreadMutex);
	}
}

/**
 * Decide how many threads should be active for a given task.
 */
uintptr_t
MM_ParallelDispatcher::recomputeActiveThreadCountForTask(MM_EnvironmentBase *env, MM_Task *task, uintptr_t threadCount)
{
	/* Metronome recomputes the number of GC threads at the beginning of
	 * a GC cycle. It may not be safe to do so at the beginning of a task
	 */
	if (!_extensions->isMetronomeGC()) {
		/* On entry _threadCount will be either:
		 *  1) the value specified by user on -Xgcthreads
		 *  2) the number of active CPU's at JVM startup
		 *  3) 1 if we have not yet started the GC helpers or are in process of  shutting down
		 *     the GC helper threads.
		 */
		_activeThreadCount = adjustThreadCount(_threadCount);
	}


	/* Caller might have tried to override thread count for this task with an explicit value.
	 * Obey it, only if <= than what we calculated it should be (there might not be more active threads
	 * available and ready to run).
	 */
	uintptr_t taskActiveThreadCount = OMR_MIN(_activeThreadCount, threadCount);

	/* Account for Adaptive Threading. RecommendedWorkingThreads will not be set (will return UDATA_MAX) if:
	 *
	 *  1) User forced a thread count (e.g Xgcthreads)
	 *  2) Adaptive threading flag is not set (-XX:-AdaptiveGCThreading)
	 *  3) or simply the task wasn't recommended a thread count (currently only recommended for STW Scavenge Tasks)
	 */
	if (UDATA_MAX != task->getRecommendedWorkingThreads()) {
		/* Bound the recommended thread count. Determine the  upper bound for the thread count,
		 * This will either be the user specified gcMaxThreadCount (-XgcmaxthreadsN) or else default max
		 */
		taskActiveThreadCount = OMR_MIN(_threadCount, task->getRecommendedWorkingThreads());

		_activeThreadCount = taskActiveThreadCount;

		Trc_MM_ParallelDispatcher_recomputeActiveThreadCountForTask_useCollectorRecommendedThreads(task->getRecommendedWorkingThreads(), taskActiveThreadCount);
	}

	task->setThreadCount(taskActiveThreadCount);
 	return taskActiveThreadCount;
}

uintptr_t 
MM_ParallelDispatcher::adjustThreadCount(uintptr_t maxThreadCount)
{
	uintptr_t toReturn = maxThreadCount;
	
	/* Did user force a fixed number of gc threads? */
	if (!_extensions->gcThreadCountForced) {
		/* No ...Use a sensible number of threads for current heap size. Using too many 
		 * can lead to unacceptable pause times due to insufficient parallelism. Additionally,
		 * it can lead to excessive fragmentation, causing aborts and percolates. 
		 */
		MM_Heap *heap = (MM_Heap *)_extensions->heap;
		uintptr_t heapSize = heap->getActiveMemorySize();
		uintptr_t maximumThreadsForHeapSize = (heapSize > MINIMUM_HEAP_PER_THREAD) ?  heapSize / MINIMUM_HEAP_PER_THREAD : 1;
		if (maximumThreadsForHeapSize < maxThreadCount) {
			Trc_MM_ParallelDispatcher_adjustThreadCount_smallHeap(maximumThreadsForHeapSize);
			toReturn = maximumThreadsForHeapSize;
		}

		OMRPORT_ACCESS_FROM_OMRVM(_extensions->getOmrVM());
		/* No, use the current active CPU count (unless it would overflow our threadtables) */
		uintptr_t activeCPUs = omrsysinfo_get_number_CPUs_by_type(OMRPORT_CPU_TARGET);
		if (activeCPUs < toReturn) {
			Trc_MM_ParallelDispatcher_adjustThreadCount_ReducedCPU(activeCPUs);
			toReturn = activeCPUs;
		}
	}
	
	return toReturn;
}

void
MM_ParallelDispatcher::prepareThreadsForTask(MM_EnvironmentBase *env, MM_Task *task, uintptr_t threadCount)
{
	omrthread_monitor_enter(_workerThreadMutex);
	
	uintptr_t activeThreads = recomputeActiveThreadCountForTask(env, task, threadCount);
	task->mainSetup(env);

	/* Set _workerThreadsReservedForGC to true so that shutdown will not 
	 * attempt to kill the worker threads until after this task is completed
	 */
	_workerThreadsReservedForGC = true; 

	Assert_MM_true(NULL == _task);
	_task = task;

	task->setSynchronizeMutex(_synchronizeMutex);

	/* Main thread will be used - update status */
	_statusTable[env->getWorkerID()] = worker_status_reserved;
	_taskTable[env->getWorkerID()] = task;

	/* Main thread doesn't need to be woken up */
	Assert_MM_true(0 == _threadsToReserve);
	_threadsToReserve = activeThreads - 1;
	wakeUpThreads(_threadsToReserve);

	omrthread_monitor_exit(_workerThreadMutex);
}

void
MM_ParallelDispatcher::acceptTask(MM_EnvironmentBase *env)
{
	uintptr_t workerID = env->getWorkerID();
	
	env->resetWorkUnitIndex();
	_statusTable[workerID] = worker_status_active;
	env->_currentTask = _taskTable[workerID];

	env->_currentTask->accept(env);
}

void
MM_ParallelDispatcher::completeTask(MM_EnvironmentBase *env)
{
	uintptr_t workerID = env->getWorkerID();
	_statusTable[workerID] = worker_status_waiting;
	
	MM_Task *currentTask = env->_currentTask;
	env->_currentTask = NULL;
	_taskTable[workerID] = NULL;

	currentTask->complete(env);
}

void
MM_ParallelDispatcher::cleanupAfterTask(MM_EnvironmentBase *env)
{
	omrthread_monitor_enter(_workerThreadMutex);
	
	_workerThreadsReservedForGC = false;
	Assert_MM_true(_threadsToReserve == 0);
	_task = NULL;
	
	if (_inShutdown) {
		omrthread_monitor_notify_all(_workerThreadMutex);
	}
	
	omrthread_monitor_exit(_workerThreadMutex);
}

void
MM_ParallelDispatcher::run(MM_EnvironmentBase *env, MM_Task *task, uintptr_t newThreadCount)
{
	prepareThreadsForTask(env, task, newThreadCount);
	acceptTask(env);
	task->run(env);
	completeTask(env);
	cleanupAfterTask(env);
	task->mainCleanup(env);
}

/**
 * Return a value indicating the priority at which GC threads should be run.
 */
uintptr_t 
MM_ParallelDispatcher::getThreadPriority()
{
	return J9THREAD_PRIORITY_NORMAL;
}

/**
 * Mark the worker thread as ready then notify everyone who is waiting
 * on the _workerThreadMutex.
 */
void
MM_ParallelDispatcher::setThreadInitializationComplete(MM_EnvironmentBase *env)
{
	uintptr_t workerID = env->getWorkerID();
	
	/* Set the status of the thread to waiting and notify that the thread has started up */
	omrthread_monitor_enter(_dispatcherMonitor);
	_statusTable[workerID] = MM_ParallelDispatcher::worker_status_waiting;
	omrthread_monitor_notify_all(_dispatcherMonitor);
	omrthread_monitor_exit(_dispatcherMonitor);
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
void
MM_ParallelDispatcher::prepareForCheckpoint(MM_EnvironmentBase *env, uintptr_t newThreadCount)
{
	contractThreadPool(env, newThreadCount);
}

bool
MM_ParallelDispatcher::reinitializeForRestore(MM_EnvironmentBase *env)
{
	return expandThreadPool(env);
}

void
MM_ParallelDispatcher::contractThreadPool(MM_EnvironmentBase *env, uintptr_t newThreadCount)
{
	Assert_MM_false(_inShutdown);

	Assert_MM_true(_threadShutdownCount == (_poolMaxCapacity - 1));
	Assert_MM_true(_threadCountMaximum == _extensions->gcThreadCount);
	Assert_MM_true(_threadCountMaximum == _poolMaxCapacity);

	uintptr_t preShutdownThreadCount = _extensions->gcThreadCount;

	Trc_MM_ParallelDispatcher_contractThreadPool_Entry(preShutdownThreadCount, newThreadCount);

	/* The main thread can't shutdown since the dispatcher didn't start it. */
	if (0 == newThreadCount) {
		newThreadCount = 1;
	}

	if (newThreadCount < _threadCountMaximum) {
		Trc_MM_ParallelDispatcher_contractThreadPool_Attempt();

		_inShutdown = true;

		omrthread_monitor_enter(_workerThreadMutex);

		while (_workerThreadsReservedForGC) {
			omrthread_monitor_wait(_workerThreadMutex);
		}

		/* Set the active parallel thread count to 1. This allows
		 * worker threads to cause a GC during their detach,
		 * making them the main in a single threaded GC.
		 */
		_threadCount = 1;

		/* Clip the thread pool past newThreadCount. */
		for (uintptr_t index = newThreadCount; index < _threadCountMaximum; index++) {
			_statusTable[index] = worker_status_dying;
		}

		omrthread_monitor_notify_all(_workerThreadMutex);

		omrthread_monitor_exit(_workerThreadMutex);

		uintptr_t expectedThreadShutdownThread = newThreadCount - 1;

		omrthread_monitor_enter(_dispatcherMonitor);
		while (expectedThreadShutdownThread != _threadShutdownCount) {
			omrthread_monitor_wait(_dispatcherMonitor);
		}
		omrthread_monitor_exit(_dispatcherMonitor);

		/* Cleanup the dispatcher tables. */
		for (uintptr_t index = newThreadCount; index < _threadCountMaximum; index++) {
			Assert_MM_true(worker_status_dying == _statusTable[index]);
			_statusTable[index] = worker_status_inactive;
			_threadTable[index] = NULL;
		}

		Assert_MM_true(_threadShutdownCount == expectedThreadShutdownThread);

		_activeThreadCount = newThreadCount;
		_threadCount = newThreadCount;
		_threadCountMaximum = newThreadCount;

		_inShutdown = false;

		Trc_MM_ParallelDispatcher_contractThreadPool_Success(preShutdownThreadCount, newThreadCount);
	}

	Trc_MM_ParallelDispatcher_contractThreadPool_Exit(_extensions->gcThreadCount);
}

bool
MM_ParallelDispatcher::expandThreadPool(MM_EnvironmentBase *env)
{
	Trc_MM_ParallelDispatcher_expandThreadPool_Entry();

	Assert_MM_false(_workerThreadsReservedForGC);
	Assert_MM_false(_inShutdown);

	Assert_MM_true(_threadShutdownCount == (_threadCountMaximum - 1));

	bool result = true;

	uintptr_t preExpandThreadCount = _threadCountMaximum;
	uintptr_t newThreadCount = _extensions->gcThreadCount;

	Assert_MM_true(newThreadCount >= preExpandThreadCount);

	Trc_MM_ParallelDispatcher_expandThreadPool_params(
			newThreadCount, _poolMaxCapacity,
			_extensions->configuration->defaultGCThreadCount(env), preExpandThreadCount);

	result = reinitializeThreadPool(env, newThreadCount);

	if (result) {
		if (newThreadCount > preExpandThreadCount) {
			result = internalStartupThreads(preExpandThreadCount, newThreadCount);

			if (result) {
				Assert_MM_true(_threadShutdownCount == (newThreadCount - 1));
			} else {
				/* Infer from _threadShutdownCount to determine the number of threads that started up prior to failing. */
				newThreadCount = _threadShutdownCount + 1;
			}

			_threadCount = newThreadCount;
			_threadCountMaximum = newThreadCount;
		}
	}

	_activeThreadCount = adjustThreadCount(_threadCount);

	Trc_MM_ParallelDispatcher_expandThreadPool_Exit(preExpandThreadCount, _extensions->gcThreadCount, _threadShutdownCount);

	return result;
}

bool
MM_ParallelDispatcher::reinitializeThreadPool(MM_EnvironmentBase *env, uintptr_t newPoolSize)
{
	 if (!_extensions->isStandardGC()) {
		  /* Dispatcher reinitialization is only supported for standard collectors. */
		 Assert_MM_true(newPoolSize <= _poolMaxCapacity);
	 } else if (newPoolSize > _poolMaxCapacity) {
		/* Re-size/allocate the dispatcher tables: _threadTable, _statusTable & _taskTable. */
		 OMR::GC::Forge *forge = env->getForge();

		 omrthread_t *threadTableTemp = (omrthread_t *)forge->allocate(
				 newPoolSize * sizeof(omrthread_t),
				 OMR::GC::AllocationCategory::FIXED,
				 OMR_GET_CALLSITE());
		 if(NULL == threadTableTemp) {
			 goto error_no_memory;
		 }
		 memset(threadTableTemp, 0, newPoolSize * sizeof(omrthread_t));

		 uintptr_t *statusTableTemp = (uintptr_t *)forge->allocate(
				 newPoolSize * sizeof(uintptr_t *),
				 OMR::GC::AllocationCategory::FIXED,
				 OMR_GET_CALLSITE());
		 if (NULL == statusTableTemp) {
			 goto error_no_memory;
		 }
		 memset(statusTableTemp, 0, newPoolSize * sizeof(uintptr_t *));

		 MM_Task **taskTableTemp = (MM_Task **)forge->allocate(
				 newPoolSize * sizeof(MM_Task *),
				 OMR::GC::AllocationCategory::FIXED,
				 OMR_GET_CALLSITE());
		 if (NULL == taskTableTemp) {
			 goto error_no_memory;
		 }
		 memset(taskTableTemp, 0, newPoolSize * sizeof(MM_Task *));

		 for (uintptr_t index = 0; index < _threadCountMaximum; index++) {
			 threadTableTemp[index] =_threadTable[index];
			 statusTableTemp[index] = _statusTable[index];
			 taskTableTemp[index] = _taskTable[index];
		 }

		 forge->free(_taskTable);
		 _taskTable = taskTableTemp;

		 forge->free(_statusTable);
		_statusTable = statusTableTemp;

		forge->free(_threadTable);
		_threadTable = threadTableTemp;

		_poolMaxCapacity = newPoolSize;
	}

	return true;

error_no_memory:
	return false;
}

#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
