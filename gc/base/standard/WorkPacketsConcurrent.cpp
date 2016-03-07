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

#include "EnvironmentStandard.hpp"
#include "ConcurrentOverflow.hpp"

#include "WorkPacketsConcurrent.hpp"

/**
 * Instantiate a MM_WorkPacketsConcurrent
 * @param mode type of packets (used for getting the right overflow handler)
 * @return pointer to the new object
 */
MM_WorkPacketsConcurrent *
MM_WorkPacketsConcurrent::newInstance(MM_EnvironmentBase *env)
{
	MM_WorkPacketsConcurrent *workPackets = (MM_WorkPacketsConcurrent *)env->getForge()->allocate(sizeof(MM_WorkPacketsConcurrent), MM_AllocationCategory::WORK_PACKETS, OMR_GET_CALLSITE());
	if (NULL != workPackets) {
		new(workPackets) MM_WorkPacketsConcurrent(env);
		if (!workPackets->initialize(env)) {
			workPackets->kill(env);
			workPackets = NULL;
		}
	}

	return workPackets;
}

void
MM_WorkPacketsConcurrent::resetWorkPacketsOverflow()
{
#if defined(OMR_GC_MODRON_SCAVENGER)
	/* this is a temporary solution, code will be changed to have only one handler very soon */
	((MM_ConcurrentOverflow *)_overflowHandler)->setCardsForNewSpaceNotCleared();
#endif /*  OMR_GC_MODRON_SCAVENGER */
}

MM_WorkPacketOverflow *
MM_WorkPacketsConcurrent::createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
{
	return MM_ConcurrentOverflow::newInstance(env, this);
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */

