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


#include "omr.h"

#include "OverflowSegregated.hpp"

#include "WorkPacketsSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * Instantiate a MM_WorkPacketsSegregated
 * @param mode type of packets (used for getting the right overflow handler)
 * @return pointer to the new object
 */
MM_WorkPacketsSegregated *
MM_WorkPacketsSegregated::newInstance(MM_EnvironmentBase *env)
{
	MM_WorkPacketsSegregated *workPackets;

	workPackets = (MM_WorkPacketsSegregated *)env->getForge()->allocate(sizeof(MM_WorkPacketsSegregated), MM_AllocationCategory::WORK_PACKETS, OMR_GET_CALLSITE());
	if (NULL != workPackets) {
		new(workPackets) MM_WorkPacketsSegregated(env);
		if (!workPackets->initialize(env)) {
			workPackets->kill(env);
			workPackets = NULL;
		}
	}

	return workPackets;
}

MM_WorkPacketOverflow *
MM_WorkPacketsSegregated::createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
{
	return MM_OverflowSegregated::newInstance(env, this);
}

#endif /* OMR_GC_SEGREGATED_HEAP */
