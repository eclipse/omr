/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/


/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(PARALLEL_TASK_HPP_)
#define PARALLEL_TASK_HPP_

#include "omr.h"
#include "omrthread.h"

#include "AtomicOperations.hpp"
#include "Task.hpp"

class MM_EnvironmentBase;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_ParallelTask : public MM_Task
{
	/*
	 * Data members
	 */
private:
protected:
	bool _synchronized;
	const char *_syncPointUniqueId;
	uintptr_t _syncPointWorkUnitIndex; /**< The _workUnitIndex of the first thread to sync. All threads should have the same index once sync'ed. */

	uintptr_t _totalThreadCount;
	volatile uintptr_t _threadCount;
	
	volatile uintptr_t _workUnitIndex;
	volatile uintptr_t _synchronizeIndex;
	volatile uintptr_t _synchronizeCount;
	omrthread_monitor_t _synchronizeMutex;
public:
	
	/*
	 * Function members
	 */
public:
	virtual bool handleNextWorkUnit(MM_EnvironmentBase *env);
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id);
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);
	virtual void releaseSynchronizedGCThreads(MM_EnvironmentBase *env);
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id, uint64_t *stallTime);
	virtual bool synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id, uint64_t *stallTime);
	
	MMINLINE virtual void setSynchronizeMutex(omrthread_monitor_t synchronizeMutex) { _synchronizeMutex = synchronizeMutex; }
	virtual void complete(MM_EnvironmentBase *env);

	/**
	 * @note This should not be called by anyone but Dispatcher or ParallelDispatcher 
	 **/
	MMINLINE virtual void setThreadCount(uintptr_t threadCount) {
		_threadCount = threadCount;
		_totalThreadCount = threadCount;
	}
	MMINLINE virtual uintptr_t getThreadCount() { return _totalThreadCount; }
	
	virtual bool isSynchronized();

	/**
	 * Create a ParallelTask object.
	 */
	MM_ParallelTask(MM_EnvironmentBase *env, MM_Dispatcher *dispatcher) :
		MM_Task(env, dispatcher)
		,_synchronized(false)
		,_syncPointUniqueId(NULL)
		,_syncPointWorkUnitIndex(0)
		,_totalThreadCount(0)
		,_threadCount(0)
		,_workUnitIndex(0)
		,_synchronizeIndex(0)
		,_synchronizeCount(0)
		,_synchronizeMutex(NULL)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* PARALLEL_TASK_HPP_ */
