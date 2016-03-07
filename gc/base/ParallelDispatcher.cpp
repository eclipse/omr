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


/**
 * @file
 * @ingroup GC_Base
 */

#include "omrcfg.h"
#include "omr.h"
#include "ModronAssertions.h"
#include "ut_j9mm.h"

#include "CollectorLanguageInterfaceImpl.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "Task.hpp"

#include "ParallelDispatcher.hpp"

typedef struct slaveThreadInfo {
	OMR_VM *omrVM;
	uintptr_t slaveID;
	uintptr_t slaveFlags;
	MM_ParallelDispatcher *dispatcher;
} slaveThreadInfo;

#define SLAVE_INFO_FLAG_OK 1
#define SLAVE_INFO_FLAG_FAILED 2

#define MINIMUM_HEAP_PER_THREAD (2*1024*1024)

uintptr_t
dispatcher_thread_proc2(OMRPortLibrary* portLib, void *info)
{
	slaveThreadInfo *slaveInfo = (slaveThreadInfo *)info;
	OMR_VM *omrVM = slaveInfo->omrVM;
	OMR_VMThread *omrVMThread = NULL;
	uintptr_t slaveID = 0;
	MM_ParallelDispatcher *dispatcher = slaveInfo->dispatcher;
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(omrVM);
	MM_CollectorLanguageInterface *cli = extensions->collectorLanguageInterface;
	MM_EnvironmentBase *env = NULL;
	uintptr_t oldVMState = 0;

	/* Cache values from the info before releasing it */
	slaveID = slaveInfo->slaveID;

	/* Attach the thread as a system daemon thread */
	omrVMThread = cli->attachVMThread(omrVM, "GC Slave", MM_CollectorLanguageInterfaceImpl::ATTACH_GC_DISPATCHER_THREAD);
	if (NULL == omrVMThread) {
		goto startup_failed;
	}

	env = MM_EnvironmentBase::getEnvironment(omrVMThread);
	env->setSlaveID(slaveID);
	/* Enviroment initialization specific for GC threads (after slave ID is set) */
	env->initializeGCThread();

	/* Signal that the thread was created succesfully */
	slaveInfo->slaveFlags = SLAVE_INFO_FLAG_OK;

	oldVMState = env->pushVMstate(J9VMSTATE_GC_DISPATCHER_IDLE);

	/* Begin running the thread */
	if (env->isMasterThread()) {
		env->setThreadType(GC_MASTER_THREAD);
		cli->parallelDispatcher_handleMasterThread(omrVMThread);
		dispatcher->masterEntryPoint(env);
	} else {
		env->setThreadType(GC_SLAVE_THREAD);
		dispatcher->slaveEntryPoint(env);
	}

	env->popVMstate(oldVMState);

	/* Thread is terminating -- shut it down */
	env->setSlaveID(0);
	cli->detachVMThread(omrVM, omrVMThread, MM_CollectorLanguageInterfaceImpl::ATTACH_GC_DISPATCHER_THREAD);
	
	omrthread_monitor_enter(dispatcher->_dispatcherMonitor);
	dispatcher->_threadShutdownCount -= 1;
	omrthread_monitor_notify(dispatcher->_dispatcherMonitor);
	/* Terminate the thread */
	omrthread_exit(dispatcher->_dispatcherMonitor);

	Assert_MM_unreachable();
	return 0;

startup_failed:
	slaveInfo->slaveFlags = SLAVE_INFO_FLAG_FAILED;

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
	OMR_VM *omrVM = ((slaveThreadInfo *)info)->omrVM;
	MM_ParallelDispatcher *dispatcher = ((slaveThreadInfo *)info)->dispatcher;
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
 * Run the main loop for a fully-constructed slave thread.
 * Subclasses can override this to have their own method of controlling slave
 * threads. They should keep the basic pattern of:
 * <code>
 * acceptTask(env); 
 * env->_currentTask->run(env); 
 * completeTask(env);
 * </code>
 */
void
MM_ParallelDispatcher::slaveEntryPoint(MM_EnvironmentBase *env) 
{
	uintptr_t slaveID = env->getSlaveID();
	
	setThreadInitializationComplete(env);
	
	omrthread_monitor_enter(_slaveThreadMutex);

	while(slave_status_dying != _statusTable[slaveID]) {
		/* Wait for a task to be dispatched to the slave thread */
		while(slave_status_waiting == _statusTable[slaveID]) {
			omrthread_monitor_wait(_slaveThreadMutex);
		}

		if(slave_status_reserved == _statusTable[slaveID]) {
			/* Found a task to dispatch to - do prep work for dispatch */
			acceptTask(env);
			omrthread_monitor_exit(_slaveThreadMutex);	

			env->_currentTask->run(env);

			omrthread_monitor_enter(_slaveThreadMutex);
			/* Returned from task - do clean up work from dispatch */
			completeTask(env);
		}
	}
	omrthread_monitor_exit(_slaveThreadMutex);	
}

void
MM_ParallelDispatcher::masterEntryPoint(MM_EnvironmentBase *env)
{
	/* The default implementation is to not start a separate
	 * master thread, but any subclasses that override 
	 * useSeparateMasterThread() must also override this method.
	 */
	assume0(0);
}

MM_ParallelDispatcher *
MM_ParallelDispatcher::newInstance(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize)
{
	MM_ParallelDispatcher *dispatcher;
	
	dispatcher = (MM_ParallelDispatcher *)env->getForge()->allocate(sizeof(MM_ParallelDispatcher), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (dispatcher) {
		new(dispatcher) MM_ParallelDispatcher(env, handler, handler_arg, defaultOSStackSize);
		if(!dispatcher->initialize(env)) {
			dispatcher->kill(env);
			return NULL;
		}
	}
	return dispatcher;
}

void
MM_ParallelDispatcher::kill(MM_EnvironmentBase *env)
{
	MM_Forge *forge = env->getForge();

	if(_slaveThreadMutex) {
		omrthread_monitor_destroy(_slaveThreadMutex);
		_slaveThreadMutex = NULL;
	}
    if(_dispatcherMonitor) {
        omrthread_monitor_destroy(_dispatcherMonitor);
        _dispatcherMonitor = NULL;
    }
	if(_synchronizeMutex) {
		omrthread_monitor_destroy(_synchronizeMutex);
		_synchronizeMutex = NULL;
	}

	if(_taskTable) {
		forge->free(_taskTable);
		_taskTable = NULL;
	}
	if(_statusTable) {
		forge->free(_statusTable);
		_statusTable = NULL;
	}
	if(_threadTable) {
		forge->free(_threadTable);
		_threadTable = NULL;
	}

	MM_Dispatcher::kill(env);
}

bool
MM_ParallelDispatcher::initialize(MM_EnvironmentBase *env)
{
	MM_Forge *forge = env->getForge();

	_threadCountMaximum = env->getExtensions()->gcThreadCount;
	Assert_MM_true(0 < _threadCountMaximum);

	if(omrthread_monitor_init_with_name(&_slaveThreadMutex, 0, "MM_ParallelDispatcher::slaveThread")
	|| omrthread_monitor_init_with_name(&_dispatcherMonitor, 0, "MM_ParallelDispatcher::dispatcherControl")
	|| omrthread_monitor_init_with_name(&_synchronizeMutex, 0, "MM_ParallelDispatcher::synchronize")) {
		goto error_no_memory;
	}

	/* Initialize the thread tables */
	_threadTable = (omrthread_t *)forge->allocate(_threadCountMaximum * sizeof(omrthread_t), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(!_threadTable) {
		goto error_no_memory;
	}
	memset(_threadTable, 0, _threadCountMaximum * sizeof(omrthread_t));

	_statusTable = (uintptr_t *)forge->allocate(_threadCountMaximum * sizeof(uintptr_t *), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(!_statusTable) {
		goto error_no_memory;
	}
	memset(_statusTable, 0, _threadCountMaximum * sizeof(uintptr_t *));

	_taskTable = (MM_Task **)forge->allocate(_threadCountMaximum * sizeof(MM_Task *), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(!_taskTable) {
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
	intptr_t threadForkResult;
	uintptr_t slaveThreadCount;
	slaveThreadInfo slaveInfo;

	/* Fork the slave threads */
	slaveInfo.omrVM = _extensions->getOmrVM();
	slaveInfo.dispatcher = this;

	_threadShutdownCount = 0;

	omrthread_monitor_enter(_dispatcherMonitor);

	/* We may be starting the master thread at this point too */
	slaveThreadCount = useSeparateMasterThread() ? 0 : 1;
	
	while (slaveThreadCount < _threadCountMaximum) {
		slaveInfo.slaveFlags = 0;
		slaveInfo.slaveID = slaveThreadCount;

		threadForkResult =
			createThreadWithCategory(
				&(_threadTable[slaveThreadCount]),
				_defaultOSStackSize,
				getThreadPriority(),
				0,
				dispatcher_thread_proc,
				(void *)&slaveInfo,
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
		} while (!slaveInfo.slaveFlags);

		if(slaveInfo.slaveFlags != SLAVE_INFO_FLAG_OK ) {
			goto error;
		}

		_threadShutdownCount += 1;
		slaveThreadCount += 1;
	}
	omrthread_monitor_exit(_dispatcherMonitor);

	_threadCount = _threadCountMaximum;
	
	_activeThreadCount = adjustThreadCount(_threadCount);

	return true;

error:
	/* exit from monitor */
	omrthread_monitor_exit(_dispatcherMonitor);

	/* Clean up the thread table and monitors */
	shutDownThreads();
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
	omrthread_monitor_enter(_slaveThreadMutex);

	while (_slaveThreadsReservedForGC) {
		omrthread_monitor_wait(_slaveThreadMutex);
	}

	/* Set the slave thread mode to dying */
	for(uintptr_t index=0; index < _threadCountMaximum; index++) {
		_statusTable[index] = slave_status_dying;
	}

	/* Set the active parallel thread count to 1 */
	/* This allows slave threads to cause a GC during their detach, */
	/* making them the master in a single threaded GC */
	_threadCount = 1;

	wakeUpThreads(_threadShutdownCount);
	omrthread_monitor_exit(_slaveThreadMutex);

	omrthread_monitor_enter(_dispatcherMonitor);
	while (0 != _threadShutdownCount) {
		omrthread_monitor_wait(_dispatcherMonitor);
	}
	omrthread_monitor_exit(_dispatcherMonitor);
}

/**
 * Wake up at least the first <code>count</code> slave threads.
 * In this implementation, since slaveThreadEntryPoint() allows a thread to
 * go back to sleep if it wasn't selected, we can wake them all up. This
 * may not apply to all subclasses though.
 */
void
MM_ParallelDispatcher::wakeUpThreads(uintptr_t count)
{
	omrthread_monitor_notify_all(_slaveThreadMutex);	
}

/**
 * Decide how many threads should be active for a given task.
 */
void
MM_ParallelDispatcher::recomputeActiveThreadCount(MM_EnvironmentBase *env)
{
	/* On entry _threadCount will be either:
	 *  1) the value specified by user on -Xgcthreads
	 *  2) the number of active CPU's at JVM startup 
	 *  3) 1 if we have not yet started the GC helpers or are in process of  shutting down 
	 *     the GC helper threads.
	 */ 	
	_activeThreadCount = adjustThreadCount(_threadCount);
}

uintptr_t 
MM_ParallelDispatcher::adjustThreadCount(uintptr_t maxThreadCount)
{
	uintptr_t toReturn = maxThreadCount;
	
	/* Did user specify number of gc threads? */
	if(!_extensions->gcThreadCountForced) {
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
MM_ParallelDispatcher::prepareThreadsForTask(MM_EnvironmentBase *env, MM_Task *task)
{
	omrthread_monitor_enter(_slaveThreadMutex);
	
	/* Set _slaveThreadsReservedForGC to true so that shutdown will not 
	 * attempt to kill the slave threads until after this task is completed
	 */
	_slaveThreadsReservedForGC = true; 

	if (!_extensions->isMetronomeGC()) {
		/* Metronome recomputes the number of GC threads at the beginning of
		 * a GC cycle. It may not be safe to do so at the beginning of a task
		 */	
		recomputeActiveThreadCount(env);
	}

	task->setThreadCount(_activeThreadCount);
	task->setSynchronizeMutex(_synchronizeMutex);
	
	for(uintptr_t index=0; index < _activeThreadCount; index++) {
		_statusTable[index] = slave_status_reserved;
		_taskTable[index] = task;
	}
	wakeUpThreads(_activeThreadCount);
	omrthread_monitor_exit(_slaveThreadMutex);
}

void
MM_ParallelDispatcher::acceptTask(MM_EnvironmentBase *env)
{
	uintptr_t slaveID = env->getSlaveID();
	
	env->resetWorkUnitIndex();
	_statusTable[slaveID] = slave_status_active;
	env->_currentTask = _taskTable[slaveID];

	env->_currentTask->accept(env);
}

void
MM_ParallelDispatcher::completeTask(MM_EnvironmentBase *env)
{
	uintptr_t slaveID = env->getSlaveID();
	_statusTable[slaveID] = slave_status_waiting;
	
	MM_Task *currentTask = env->_currentTask;
	env->_currentTask = NULL;
	_taskTable[slaveID] = NULL;

	currentTask->complete(env);
}

void
MM_ParallelDispatcher::cleanupAfterTask(MM_EnvironmentBase *env)
{
	omrthread_monitor_enter(_slaveThreadMutex);
	
	_slaveThreadsReservedForGC = false;
	
	if (_inShutdown) {
		omrthread_monitor_notify_all(_slaveThreadMutex);
	}
	
	omrthread_monitor_exit(_slaveThreadMutex);
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
 * Mark the slave thread as ready then notify everyone who is waiting
 * on the _slaveThreadMutex.
 */
void
MM_ParallelDispatcher::setThreadInitializationComplete(MM_EnvironmentBase *env)
{
	uintptr_t slaveID = env->getSlaveID();
	
	/* Set the status of the thread to waiting and notify that the thread has started up */
	omrthread_monitor_enter(_dispatcherMonitor);
	_statusTable[slaveID] = MM_ParallelDispatcher::slave_status_waiting;
	omrthread_monitor_notify_all(_dispatcherMonitor);
	omrthread_monitor_exit(_dispatcherMonitor);
}

void
MM_ParallelDispatcher::reinitAfterFork(MM_EnvironmentBase *env, uintptr_t newThreadCount)
{
	/* Set the slave thread mode to dying */
	for(uintptr_t index=0; index < _threadCountMaximum; index++) {
		_statusTable[index] = slave_status_dying;
	}

	if (newThreadCount < _threadCountMaximum) {
		_threadCountMaximum = newThreadCount;
	}

	startUpThreads();
}
