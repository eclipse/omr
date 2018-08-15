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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(PARALLEL_DISPATCHER_HPP_)
#define PARALLEL_DISPATCHER_HPP_

#include "omrcfg.h"

#include "modronopt.h"
#include "modronbase.h"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

class MM_EnvironmentBase;

class MM_ParallelDispatcher : public MM_Dispatcher
{
	/*
	 * Data members
	 */
private:
protected:
	MM_GCExtensionsBase *_extensions;

	enum {
		slave_status_inactive = 0,	/* Must be 0 - set at initialization time by memset */
		slave_status_waiting,
		slave_status_reserved,
		slave_status_active,
		slave_status_dying
	};

	uintptr_t _threadShutdownCount;
	omrthread_t *_threadTable;
	uintptr_t *_statusTable;
	MM_Task **_taskTable;
	
	omrthread_monitor_t _slaveThreadMutex;
	omrthread_monitor_t _dispatcherMonitor; /**< Provides signalling between threads for startup and shutting down as well as the thread that initiated the shutdown */

	/* The synchronize mutex should eventually be a table of mutexes that are distributed to each */
	/* Task as they are dispatched.  For now, since there is only one task active at any time, a */
	/* single mutex is sufficient */
	omrthread_monitor_t _synchronizeMutex;
	
	bool _slaveThreadsReservedForGC;  /**< States whether or not the slave threads are currently taking part in a GC */
	bool _inShutdown;  /**< Shutdown request is received */

	uintptr_t _threadCountMaximum; /**< maximum threadcount - this is the size of the thread tables etc */
	uintptr_t _threadCount; /**< number of threads currently forked */
	uintptr_t _activeThreadCount; /**< number of threads actively running a task */

	omrsig_handler_fn _handler;
	void* _handler_arg;
	uintptr_t _defaultOSStackSize; /**< default OS stack size */

public:

	/*
	 * Function members
	 */
private:
protected:
	virtual void slaveEntryPoint(MM_EnvironmentBase *env);
	virtual void masterEntryPoint(MM_EnvironmentBase *env);

	bool initialize(MM_EnvironmentBase *env);
	
	virtual void prepareThreadsForTask(MM_EnvironmentBase *env, MM_Task *task);
	virtual void cleanupAfterTask(MM_EnvironmentBase *env);
	virtual uintptr_t getThreadPriority(); 

	/**
	 * Decides whether the dispatcher also start a separate thread to be the master
	 * GC thread. Usually no, because the master thread will be the thread that
	 * requested the GC.
	 */  
	virtual bool useSeparateMasterThread() { return false; }
	
	virtual void acceptTask(MM_EnvironmentBase *env);
	virtual void completeTask(MM_EnvironmentBase *env);
	virtual void wakeUpThreads(uintptr_t count);

	virtual void recomputeActiveThreadCount(MM_EnvironmentBase *env);
	
	virtual void setThreadInitializationComplete(MM_EnvironmentBase *env);
	
public:
	virtual bool startUpThreads();
	virtual void shutDownThreads();
	
	uintptr_t adjustThreadCount(uintptr_t maxThreadCount);
	MMINLINE virtual uintptr_t threadCount() { return _threadCount; }
	MMINLINE virtual uintptr_t threadCountMaximum() { return _threadCountMaximum; }
	MMINLINE omrthread_t* getThreadTable() { return _threadTable; }
	MMINLINE virtual uintptr_t activeThreadCount() { return _activeThreadCount; }
	virtual void setThreadCount(uintptr_t threadCount);

	MMINLINE omrsig_handler_fn getSignalHandler() {return _handler;}
	MMINLINE void * getSignalHandlerArg() {return _handler_arg;}

	static MM_ParallelDispatcher *newInstance(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize);
	virtual void kill(MM_EnvironmentBase *env);

	virtual void reinitAfterFork(MM_EnvironmentBase *env, uintptr_t newThreadCount);

	MM_ParallelDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize) :
		MM_Dispatcher(env)
		,_extensions(MM_GCExtensionsBase::getExtensions(env->getOmrVM()))
		,_threadShutdownCount(0)
		,_threadTable(NULL)
		,_statusTable(NULL)
		,_taskTable(NULL)
		,_slaveThreadMutex(NULL)
		,_dispatcherMonitor(NULL)
		,_synchronizeMutex(NULL)
		,_slaveThreadsReservedForGC(false)
		,_inShutdown(false)
		,_threadCountMaximum(1)
		,_threadCount(1)
		,_activeThreadCount(1)
		,_handler(handler)
		,_handler_arg(handler_arg)
		,_defaultOSStackSize(defaultOSStackSize)
	{
		_typeId = __FUNCTION__;
	}

	/*
	 * Friends
	 */
	friend class MM_Task;
	friend uintptr_t dispatcher_thread_proc2(OMRPortLibrary* portLib, void *info);
};

#endif /* PARALLEL_DISPATCHER_HPP_ */
