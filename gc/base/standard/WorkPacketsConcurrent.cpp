/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

