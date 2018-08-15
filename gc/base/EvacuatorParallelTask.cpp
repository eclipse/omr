/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "ModronAssertions.h"

#include "EnvironmentStandard.hpp"
#include "Evacuator.hpp"
#include "EvacuatorController.hpp"

#include "EvacuatorParallelTask.hpp"

void
MM_EvacuatorParallelTask::run(MM_EnvironmentBase *envBase)
{
	/* bind the worker thread (env) to an evacuator instance and set up the evacuator for a new gc cycle */
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	MM_Evacuator *evacuator = _controller->bindWorker(env);

	Assert_MM_true(evacuator->getWorkerIndex() < _controller->getEvacuatorThreadCount());
	Assert_MM_true(env == evacuator->getEnvironment());

	/* send the bound evacuator off to work ... */
	evacuator->workThreadGarbageCollect(env);

	/* controller will release evacuator binding and evacuator will passivate */
	_controller->unbindWorker(env);
}

void
MM_EvacuatorParallelTask::setup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		Assert_MM_true(NULL == env->_cycleState);
		env->_cycleState = _cycleState;
	}
}

void
MM_EvacuatorParallelTask::cleanup(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		env->_cycleState = NULL;
	}
}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
void
MM_EvacuatorParallelTask::synchronizeGCThreads(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	_controller->waitToSynchronize(env->getEvacuator(), id);

	uint64_t startTime = omrtime_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	uint64_t endTime = omrtime_hires_clock();

	env->_scavengerStats.addToSyncStallTime(startTime, endTime);
	_controller->continueAfterSynchronizing(env->getEvacuator(), startTime, endTime, id);
}

bool
MM_EvacuatorParallelTask::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	_controller->waitToSynchronize(env->getEvacuator(), id);

	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster(env, id);
	uint64_t endTime = omrtime_hires_clock();

	env->_scavengerStats.addToSyncStallTime(startTime, endTime);
	_controller->continueAfterSynchronizing(env->getEvacuator(), startTime, endTime, id);
	return result;	
}

bool
MM_EvacuatorParallelTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	_controller->waitToSynchronize(env->getEvacuator(), id);

	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(env, id);
	uint64_t endTime = omrtime_hires_clock();

	env->_scavengerStats.addToSyncStallTime(startTime, endTime);
	_controller->continueAfterSynchronizing(env->getEvacuator(), startTime, endTime, id);
	return result;
}

#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
