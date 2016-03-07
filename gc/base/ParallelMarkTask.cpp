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


#include "omrcfg.h"
#include "omrmodroncore.h"
#include "omrport.h"
#include "modronopt.h"
#include "ut_j9mm.h"

#include "ParallelMarkTask.hpp"

#include "EnvironmentLanguageInterface.hpp"
#include "EnvironmentBase.hpp"
#include "MarkingScheme.hpp"
#include "GlobalGCStats.hpp"
#include "WorkStack.hpp"


uintptr_t
MM_ParallelMarkTask::getVMStateID()
{
	return J9VMSTATE_GC_MARK;
}

void
MM_ParallelMarkTask::run(MM_EnvironmentBase *env)
{
	env->_workStack.prepareForWork(env, (MM_WorkPackets *)(_markingScheme->getWorkPackets()));

	_markingScheme->markLiveObjectsInit(env, _initMarkMap);
	_markingScheme->markLiveObjectsRoots(env);
	_markingScheme->markLiveObjectsScan(env);
	_markingScheme->markLiveObjectsComplete(env);

	env->_workStack.flush(env);
}

void
MM_ParallelMarkTask::setup(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());
	
	env->_markStats.clear();
	env->_workPacketStats.clear();

	env->_envLanguageInterface->parallelMarkTask_setup(env);

	/* record that this thread is participating in this cycle */
	env->_markStats._gcCount = extensions->globalGCStats.gcCount;
	env->_workPacketStats._gcCount = extensions->globalGCStats.gcCount;

	if(env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		Assert_MM_true(NULL == env->_cycleState);
		env->_cycleState = _cycleState;
	}
}

void
MM_ParallelMarkTask::cleanup(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase *extensions = MM_GCExtensionsBase::getExtensions(env->getOmrVM());

	env->_envLanguageInterface->parallelMarkTask_cleanup(env);

	extensions->globalGCStats.markStats.merge(&env->_markStats);
	extensions->globalGCStats.workPacketStats.merge(&env->_workPacketStats);
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		env->_cycleState = NULL;
	}
	
	/* record the thread-specific parallelism stats in the trace buffer. This partially duplicates info in -Xtgc:parallel */
	Trc_MM_ParallelMarkTask_parallelStats(
		env->getLanguageVMThread(),
		(uint32_t)env->getSlaveID(),
		(uint32_t)omrtime_hires_delta(0, env->_workPacketStats._workStallTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS),
		(uint32_t)omrtime_hires_delta(0, env->_workPacketStats._completeStallTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS),
		(uint32_t)omrtime_hires_delta(0, env->_markStats._syncStallTime, OMRPORT_TIME_DELTA_IN_MILLISECONDS),
		(uint32_t)env->_workPacketStats._workStallCount,
		(uint32_t)env->_workPacketStats._completeStallCount,
		(uint32_t)env->_markStats._syncStallCount,
		env->_workPacketStats.workPacketsAcquired,
		env->_workPacketStats.workPacketsReleased,
		env->_workPacketStats.workPacketsExchanged,
		0/* TODO CRG figure out to get the array split size*/);
}

#if defined(J9MODRON_TGC_PARALLEL_STATISTICS)
void
MM_ParallelMarkTask::synchronizeGCThreads(MM_EnvironmentBase *env, const char *id)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	MM_ParallelTask::synchronizeGCThreads(env, id);
	uint64_t endTime = omrtime_hires_clock();
	env->_markStats.addToSyncStallTime(startTime, endTime);
}

bool
MM_ParallelMarkTask::synchronizeGCThreadsAndReleaseMaster(MM_EnvironmentBase *env, const char *id)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseMaster(env, id);
	uint64_t endTime = omrtime_hires_clock();
	env->_markStats.addToSyncStallTime(startTime, endTime);
	
	return result;	
}

bool
MM_ParallelMarkTask::synchronizeGCThreadsAndReleaseSingleThread(MM_EnvironmentBase *env, const char *id)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t startTime = omrtime_hires_clock();
	bool result = MM_ParallelTask::synchronizeGCThreadsAndReleaseSingleThread(env, id);
	uint64_t endTime = omrtime_hires_clock();
	env->_markStats.addToSyncStallTime(startTime, endTime);

	return result;
}

#endif /* J9MODRON_TGC_PARALLEL_STATISTICS */

