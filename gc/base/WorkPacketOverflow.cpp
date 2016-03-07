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

#include "omr.h"
#include "modronbase.h"

#include "WorkPacketOverflow.hpp"
#include "EnvironmentBase.hpp"
#include "ModronAssertions.h"


MM_WorkPacketOverflow *
MM_WorkPacketOverflow::newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets)
{
	MM_WorkPacketOverflow *overflow;

	overflow = (MM_WorkPacketOverflow *)env->getForge()->allocate(sizeof(MM_WorkPacketOverflow), MM_AllocationCategory::WORK_PACKETS, OMR_GET_CALLSITE());
	if (overflow) {
		new(overflow) MM_WorkPacketOverflow(env, workPackets);
		if (!overflow->initialize(env)) {
			overflow->kill(env);
			overflow = NULL;
		}
	}

	return overflow;
}

/**
 * Initialize a MM_WorkPacketOverflow object.
 * 
 * @return true on success, false otherwise
 */
bool
MM_WorkPacketOverflow::initialize(MM_EnvironmentBase *env)
{
	if(omrthread_monitor_init_with_name(&_overflowListMonitor, 0, "MM_WorkPacketOverflow::overflowList")) {
		return false;
	}
	
	reset(env);
	return true;
}

/**
 * Cleanup the resources for a MM_WorkPacketOverflow object
 */
void
MM_WorkPacketOverflow::tearDown(MM_EnvironmentBase *env)
{
	if (NULL != _overflowListMonitor) {
		omrthread_monitor_destroy(_overflowListMonitor);
		_overflowListMonitor = NULL;
	}
}

void
MM_WorkPacketOverflow::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

void
MM_WorkPacketOverflow::handleOverflow(MM_EnvironmentBase *env)
{
	Assert_MM_unreachable();
}

bool
MM_WorkPacketOverflow::isEmpty()
{
	return true;
}

void
MM_WorkPacketOverflow::reset(MM_EnvironmentBase *env)
{
	/* Do nothing */
}

void
MM_WorkPacketOverflow::emptyToOverflow(MM_EnvironmentBase *env,MM_Packet *packet, MM_OverflowType type)
{
	Assert_MM_unreachable();
}

void
MM_WorkPacketOverflow::fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet)
{
	Assert_MM_unreachable();
}

void
MM_WorkPacketOverflow::overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type)
{
	Assert_MM_unreachable();
}

