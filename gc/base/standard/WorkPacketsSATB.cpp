/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrthread.h"

#if defined(OMR_GC_REALTIME)

#include "WorkPacketsSATB.hpp"

#include "Debug.hpp"
#include "GCExtensionsBase.hpp"
#include "OverflowStandard.hpp"

/**
 * Instantiate a MM_WorkPacketsSATB
 * @param mode type of packets (used for getting the right overflow handler)
 * @return pointer to the new object
 */
MM_WorkPacketsSATB *
MM_WorkPacketsSATB::newInstance(MM_EnvironmentBase *env)
{
	MM_WorkPacketsSATB *workPackets;

	workPackets = (MM_WorkPacketsSATB *)env->getForge()->allocate(sizeof(MM_WorkPacketsSATB), MM_AllocationCategory::WORK_PACKETS, J9_GET_CALLSITE());
	if (workPackets) {
		new(workPackets) MM_WorkPacketsSATB(env);
		if (!workPackets->initialize(env)) {
			workPackets->kill(env);
			workPackets = NULL;
		}
	}

	return workPackets;
}

/**
 * Initialize a MM_WorkPacketsSATB object
 * @return true on success, false otherwise
 */
bool
MM_WorkPacketsSATB::initialize(MM_EnvironmentBase *env)
{
	if (!MM_WorkPackets::initialize(env)) {
		return false;
	}

	if (!_inUseBarrierPacketList.initialize(env)) {
		return false;
	}

	return true;
}

#if defined(J9VM_OPT_CRIU_SUPPORT)
bool
MM_WorkPacketsSATB::reinitializeForRestore(MM_EnvironmentBase *env)
{
	bool rc = true;

	if (!MM_WorkPackets::reinitializeForRestore(env)
		|| !_inUseBarrierPacketList.reinitializeForRestore(env)
	) {
		rc = false;
	}

	return rc;
}
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

/**
 * Destroy the resources a MM_WorkPacketsSATB is responsible for
 */
void
MM_WorkPacketsSATB::tearDown(MM_EnvironmentBase *env)
{
	MM_WorkPackets::tearDown(env);

	_inUseBarrierPacketList.tearDown(env);
}

/**
 * Create the overflow handler
 */
MM_WorkPacketOverflow *
MM_WorkPacketsSATB::createOverflowHandler(MM_EnvironmentBase *env, MM_WorkPackets *wp)
{
	return MM_OverflowStandard::newInstance(env, wp);
}

/**
 * Return an empty packet for barrier processing.
 * If the emptyPacketList is empty then overflow a full packet.
 */
MM_Packet *
MM_WorkPacketsSATB::getBarrierPacket(MM_EnvironmentBase *env)
{
	MM_Packet *barrierPacket = NULL;

	/* Check the free list */
	barrierPacket = getPacket(env, &_emptyPacketList);
	if (NULL != barrierPacket) {
		return barrierPacket;
	}

	barrierPacket = getPacketByAdddingWorkPacketBlock(env);
	if (NULL != barrierPacket) {
		return barrierPacket;
	}

	/* Adding a block of packets failed so move on to overflow processing */
	return getPacketByOverflowing(env);
}

/**
 * Get a packet by overflowing a full packet or a barrierPacket
 *
 * @return pointer to a packet, or NULL if no packets could be overflowed
 */
MM_Packet *
MM_WorkPacketsSATB::getPacketByOverflowing(MM_EnvironmentBase *env)
{
	MM_Packet *packet = NULL;

	if (NULL != (packet = getPacket(env, &_fullPacketList))) {
		/* Attempt to overflow a full mark packet.
		 * Move the contents of the packet to overflow.
		 */
		emptyToOverflow(env, packet, OVERFLOW_TYPE_WORKSTACK);

		omrthread_monitor_enter(_inputListMonitor);

		/* Overflow was created - alert other threads that are waiting */
		if (_inputListWaitCount > 0) {
			omrthread_monitor_notify(_inputListMonitor);
		}
		omrthread_monitor_exit(_inputListMonitor);
	} else {
		/* Try again to get a packet off of the emptyPacketList as another thread
		 * may have emptied a packet.
		 */
		packet = getPacket(env, &_emptyPacketList);
	}

	return packet;
}

/**
 * Put the packet on the inUseBarrierPacket list.
 * @param packet the packet to put on the list
 */
void
MM_WorkPacketsSATB::putInUsePacket(MM_EnvironmentBase *env, MM_Packet *packet)
{
	_inUseBarrierPacketList.push(env, packet);
}

void
MM_WorkPacketsSATB::removePacketFromInUseList(MM_EnvironmentBase *env, MM_Packet *packet)
{
	_inUseBarrierPacketList.remove(packet);
}

void
MM_WorkPacketsSATB::putFullPacket(MM_EnvironmentBase *env, MM_Packet *packet)
{
	_fullPacketList.push(env, packet);
}

/**
 * Move all of the packets from the inUse list to the processing list
 * so they are available for processing.
 */
void
MM_WorkPacketsSATB::moveInUseToNonEmpty(MM_EnvironmentBase *env)
{
	MM_Packet *head, *tail;
	UDATA count;
	bool didPop;

	/* pop the inUseList */
	didPop = _inUseBarrierPacketList.popList(&head, &tail, &count);
	/* push the values from the inUseList onto the processingList */
	if (didPop) {
		_nonEmptyPacketList.pushList(head, tail, count);
	}
}

/**
 * Return the heap capactify factor used to determine how many packets to create
 *
 * @return the heap capactify factor
 */
float
MM_WorkPacketsSATB::getHeapCapacityFactor(MM_EnvironmentBase *env)
{
	/* Increase the factor for SATB barrier since more packets are required */
	return (float)0.008;
}

/**
 * Get an input packet from the current overflow handler
 *
 * @return a packet if one is found, NULL otherwise
 */
MM_Packet *
MM_WorkPacketsSATB::getInputPacketFromOverflow(MM_EnvironmentBase *env)
{
	MM_Packet *overflowPacket;

	/* SATB spec cannot loop here as all packets may currently be on
	 * the InUseBarrierList.  If all packets are on the InUseBarrierList then this
	 * would turn into an infinite busy loop.
	 * while(!_overflowHandler->isEmpty()) {
	 */
	if (!_overflowHandler->isEmpty()) {
		if (NULL != (overflowPacket = getPacket(env, &_emptyPacketList))) {

			_overflowHandler->fillFromOverflow(env, overflowPacket);

			if (overflowPacket->isEmpty()) {
				/* If we didn't end up filling the packet with anything, don't return it and try again */
				putPacket(env, overflowPacket);
			} else {
				return overflowPacket;
			}
		}
	}

	return NULL;
}

void
MM_WorkPacketsSATB::resetAllPackets(MM_EnvironmentBase *env)
{
	MM_Packet *packet;

	while (NULL != (packet = getPacket(env, &_inUseBarrierPacketList))) {
		packet->resetData(env);
		putPacket(env, packet);
	}

	MM_WorkPackets::resetAllPackets(env);
}

#endif /* OMR_GC_REALTIME */
