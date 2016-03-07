/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "ConcurrentGC.hpp"

#include "ConcurrentScanRememberedSetTask.hpp"

void
MM_ConcurrentScanRememberedSetTask::run(MM_EnvironmentBase *envBase)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	_collector->scanRememberedSet(env);
}

void
MM_ConcurrentScanRememberedSetTask::setup(MM_EnvironmentBase *env)
{
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		Assert_MM_true(NULL == env->_cycleState);
		env->_cycleState = _cycleState;
	}
	env->_workPacketStats.clear();
}

void
MM_ConcurrentScanRememberedSetTask::cleanup(MM_EnvironmentBase *env)
{
	if (env->isMasterThread()) {
		Assert_MM_true(_cycleState == env->_cycleState);
	} else {
		env->_cycleState = NULL;
	}
	/* take a snapshot of WorkPacket stats, since it will be overwritten with subsequent phases. The stats will be needed later (TGC parallel for example) */
	env->_workPacketStatsRSScan = env->_workPacketStats;
}
#endif /* OMR_GC_MODRON_SCAVENGER */
#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
 
