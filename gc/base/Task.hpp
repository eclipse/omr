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

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(TASK_HPP_)
#define TASK_HPP_

#include "omrcomp.h"
#include "omrmodroncore.h"
#include "omrthread.h"

#include "AtomicOperations.hpp"
#include "BaseVirtual.hpp"
#include "Debug.hpp"

/* Macro to create an unique literal string identifier */ 
#define UNIQUE_ID ((const char *)(OMR_GET_CALLSITE()))

/**
 * @todo Provide define documentation
 * @ingroup GC_Base_Core
 */
#define J9MODRON_HANDLE_NEXT_WORK_UNIT(envPtr) envPtr->_currentTask->handleNextWorkUnit(envPtr)

class MM_EnvironmentBase;
class MM_ParallelDispatcher;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_Task : public MM_BaseVirtual
{
private:
protected:
	MM_ParallelDispatcher *_dispatcher;

	uintptr_t _oldVMstate; /**< the vmState at the start of the task */
	
public:
	virtual void setup(MM_EnvironmentBase *env);
	virtual void run(MM_EnvironmentBase *env) = 0;
	virtual void cleanup(MM_EnvironmentBase *env);

	virtual uintptr_t getRecommendedWorkingThreads() { return UDATA_MAX; }

	/**
	 * Single call setup routine for tasks invoked by the main thread before the task is dispatched.
	 */
	virtual void mainSetup(MM_EnvironmentBase *env);

	/**
	 * Single call cleanup routine for tasks invoked by the main thread after the task has been dispatched and all
	 * worker threads (if any) have been quiesced.
	 */
	virtual void mainCleanup(MM_EnvironmentBase *env);

	/**
	 * @note This should not be called by anyone but Dispatcher or ParallelDispatcher 
	 **/
	MMINLINE virtual void setThreadCount(uintptr_t threadCount) { assume0(1 == threadCount); }
	MMINLINE virtual uintptr_t getThreadCount() { return 1; }

	MMINLINE virtual void setSynchronizeMutex(omrthread_monitor_t synchronizeMutex)
	{
		/* in a Task we don't need a mutex */
	}

	virtual void accept(MM_EnvironmentBase *env);
	virtual void complete(MM_EnvironmentBase *env);

	/**
	 * Do work required or next task.
	 * @note no-op
	 */
	virtual bool handleNextWorkUnit(MM_EnvironmentBase *env);

	/**
	 * Synchronize threads.
	 * @parm id is a literal string for unique identification of synchronization point 
	 * @note no-op
	 */
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id);

	/**
	 * Synchronize threads.
	 * @parm id is a literal string for unique identification of synchronization point 
	 * @note no-op
	 */
	virtual bool synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *env, const char *id);

	/**
	 * Synchronize threads.
	 * @parm id is a literal string for unique identification of synchronization point
	 * @note no-op
	 */
	virtual bool synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id);

	/**
	 * Synchronize threads.
	 * @parm id is a literal string for unique identification of synchronization point 
	 * @param[out] stallTime time spent being blocked
	 * @note no-op
	 */
	virtual void synchronizeGCThreads(MM_EnvironmentBase *env, const char *id, uint64_t *stallTime) {}
	
		/**
	 * Synchronize threads.
	 * @parm id is a literal string for unique identification of synchronization point
	 * @param[out] stallTime time spent being blocked
	 * @note no-op
	 */
	virtual bool synchronizeGCThreadsAndReleaseMain(MM_EnvironmentBase *env, const char *id, uint64_t *stallTime) { return true; }

	/**
	 * Release synchronization of threads.
	 * @note no-op
	 */
	virtual void releaseSynchronizedGCThreads(MM_EnvironmentBase *env);
	
	/**
	 * Return the uintptr_t corresponding to the VMState for this Task.
	 * @note All tasks must implement this method - the IDs are defined in @ref j9modron.h
	 */
	virtual uintptr_t getVMStateID(void) = 0;
	
	/**
	 * Return true if threads are currently synchronized, false otherwise
	 * @return true if threads are currently synchronized, false otherwise
	 */
	virtual bool isSynchronized();
	
	/* Called within marking scheme to check if we should suspend the state of the mark and return to the caller.
	 * @param env[in] The current thread
	 * @return true if we should exit the mark, false if we should continue
	 */
	virtual bool shouldYieldFromTask(MM_EnvironmentBase *env) { return false; }

	/**
	 * Create a Task object.
	 */
	MM_Task(MM_EnvironmentBase *env, MM_ParallelDispatcher *dispatcher) :
		MM_BaseVirtual(),
		_dispatcher(dispatcher),
		_oldVMstate(0)
	{
		_typeId = __FUNCTION__;
	};
	
	friend class MM_ParallelDispatcher;
};

#endif /* TASK_HPP_ */
