/*******************************************************************************
 * Copyright (c) 2015 IBM Corporation
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *    IBM Corporation - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "omrcfg.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "omr/gc/modronAssertions.h"

#include "EnvironmentStandard.hpp"
#include "Scavenger.hpp"

#include "ParallelScavengeTask.hpp"

void
MM_ParallelScavengeTask::run(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	_collector->workThreadGarbageCollect(env);
}

void
MM_ParallelScavengeTask::setup(MM_EnvironmentBase *env)
{
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		Assert_MM_true(NULL == env->_cycleState);
		env->_cycleState = _cycleState;
	}
}

void
MM_ParallelScavengeTask::cleanup(MM_EnvironmentBase *env)
{
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		env->_cycleState = NULL;
	}
}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
void
MM_ParallelScavengeTask::synchronizeGCThreads(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	uint64_t endTime = omrtime_hires_clock();

	env->_scavengerStats.addToSyncStallTime(startTime, endTime);
}

bool
MM_ParallelScavengeTask::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster(env, id);
	uint64_t endTime = omrtime_hires_clock();

	env->_scavengerStats.addToSyncStallTime(startTime, endTime);
	
	return result;	
}

bool
MM_ParallelScavengeTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *envBase, const char *id)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	OMRPORT_ACCESS_FROM_OMRPORT(envBase->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(env, id);
	uint64_t endTime = omrtime_hires_clock();

	env->_scavengerStats.addToSyncStallTime(startTime, endTime);

	return result;
}

#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
