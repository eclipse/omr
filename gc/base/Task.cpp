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
 ******************************************************************************/

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
