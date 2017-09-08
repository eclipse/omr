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

#include "omrcfg.h"
#include "ModronAssertions.h"
#include "modronopt.h"

#include "Task.hpp"

#include "EnvironmentBase.hpp"

void
MM_Task::accept(MM_EnvironmentBase *env)
{
	/* store the old VMstate */
	uintptr_t oldVMstate = env->pushVMstate(getVMStateID());
	if (env->isMasterThread()) {
		_oldVMstate = oldVMstate;
	} else {
		Assert_MM_true(J9VMSTATE_GC_DISPATCHER_IDLE == oldVMstate);
	}
	
	/* do task-specific setup */
	setup(env);
}

void
MM_Task::masterSetup(MM_EnvironmentBase *env)
{
}

void
MM_Task::masterCleanup(MM_EnvironmentBase *env)
{
}

void 
MM_Task::setup(MM_EnvironmentBase *env)
{
}

void 
MM_Task::cleanup(MM_EnvironmentBase *env)
{
}

void 
MM_Task::complete(MM_EnvironmentBase *env)
{
	Assert_MM_true(getVMStateID() == env->getOmrVMThread()->vmState);

	/* restore the previous VMstate */
	uintptr_t oldVMstate = J9VMSTATE_GC_DISPATCHER_IDLE;
	if (env->isMasterThread()) {
		oldVMstate = _oldVMstate;
	}

	env->popVMstate(oldVMstate);
	
	/* do task-specific cleanup */
	cleanup(env);
}

bool 
MM_Task::handleNextWorkUnit(MM_EnvironmentBase *env)
{
	return true;
}

void 
MM_Task::synchronizeGCThreads(MM_EnvironmentBase *env, const char *id)
{
}

bool 
MM_Task::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id)
{
	return true;
}

bool
MM_Task::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id)
{
	return true;
}

void 
MM_Task::releaseSynchronizedGCThreads(MM_EnvironmentBase *env)
{
}

bool 
MM_Task::isSynchronized()
{
	return false;
}
