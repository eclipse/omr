/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

