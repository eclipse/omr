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

#include "omrcfg.h"
#include "omrcomp.h"

#include "Dispatcher.hpp"

#include "EnvironmentBase.hpp"
#include "Task.hpp"

MM_Dispatcher *
MM_Dispatcher::newInstance(MM_EnvironmentBase *env)
{
	MM_Dispatcher *dispatcher;
	
	dispatcher = (MM_Dispatcher *)env->getForge()->allocate(sizeof(MM_Dispatcher), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (dispatcher) {
		new(dispatcher) MM_Dispatcher(env);
		if(!dispatcher->initialize(env)) {
			dispatcher->kill(env);
			return NULL;
		}
	}
	return dispatcher;
}

void
MM_Dispatcher::kill(MM_EnvironmentBase *env)
{
	env->getForge()->free(this);
}

bool
MM_Dispatcher::initialize(MM_EnvironmentBase *env)
{
	return true;
}

void
MM_Dispatcher::prepareThreadsForTask(MM_EnvironmentBase *env, MM_Task *task, uintptr_t threadCount)
{
	task->setThreadCount(1);
	_task = task;
}

void
MM_Dispatcher::acceptTask(MM_EnvironmentBase *env)
{
	env->_currentTask = _task;
	env->_currentTask->accept(env);
}

void
MM_Dispatcher::completeTask(MM_EnvironmentBase *env)
{
	MM_Task *currentTask = env->_currentTask;

	env->_currentTask = NULL;
	_task = NULL;

	currentTask->complete(env);
}

void
MM_Dispatcher::cleanupAfterTask(MM_EnvironmentBase *env)
{
}

void
MM_Dispatcher::run(MM_EnvironmentBase *env, MM_Task *task, uintptr_t newThreadCount)
{
	uintptr_t activeThreads = recomputeActiveThreadCountForTask(env, task, newThreadCount);
	task->masterSetup(env);
	prepareThreadsForTask(env, task, activeThreads);
	acceptTask(env);
	task->run(env);
	completeTask(env);
	cleanupAfterTask(env);
	task->masterCleanup(env);
}

bool 
MM_Dispatcher::startUpThreads() 
{ 
	return true; 
}

void 
MM_Dispatcher::shutDownThreads() 
{
}

uintptr_t
MM_Dispatcher::recomputeActiveThreadCountForTask(MM_EnvironmentBase *env, MM_Task *task, uintptr_t newThreadCount)
{
	return 1;
}
