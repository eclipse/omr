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
