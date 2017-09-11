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

#if !defined(OVERFLOWSEGREGATED_HPP_)
#define OVERFLOWSEGREGATED_HPP_

#include "omr.h"

#include "WorkPacketOverflow.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_OverflowSegregated : public MM_WorkPacketOverflow
{
/* Data members */
public:
protected:
private:
	MM_GCExtensionsBase *_extensions;

/* Methods */
public:
	static MM_OverflowSegregated *newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);

	virtual void reset(MM_EnvironmentBase *env);
	virtual bool isEmpty();

	/**
	 * Empty a packet on overflow
	 *
	 * Empty a packet to resolve overflow by dirtying the appropriate
	 * cards for each object withing a given packet
	 *
	 * @param packet - Reference to packet to be empited
	 * @param type - ignored for concurrent collector
	 *
	 */
	virtual void emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type);

	/**
	 * Fill a packet from overflow list
	 *
	 * @param packet - Reference to packet to be filled.
	 *
	 * @note No-op in this overflow handler. We will never fill a packet from
	 * the card table.
	 */
	virtual void fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet);

	/**
	 * Overflow an item
	 *
	 * Overflow an item by dirtying the appropriate card
	 *
	 * @param item - item to overflow
	 * @param type - ignored for concurrent collector
	 *
	 */
	virtual void overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type);

	/**
	 * Handle Overflow - clean card table
	 * @param env current thread environment
	 */
	virtual void handleOverflow(MM_EnvironmentBase *env);

	/**
	 * Create a MM_OverflowSegregated object.
	 */
	MM_OverflowSegregated(MM_EnvironmentBase *env, MM_WorkPackets *workPackets) :
		MM_WorkPacketOverflow(env, workPackets),
		_extensions(MM_GCExtensionsBase::getExtensions(env->getOmrVM()))
	{
		_typeId = __FUNCTION__;
	};

protected:
	/**
	 * Initialize a MM_OverflowSegregated object.
	 *
	 * @return true on success, false otherwise
	 */
	bool initialize(MM_EnvironmentBase *env);

	/**
	 * Cleanup the resources for a MM_ConcurrentOverflow object
	 */
	void tearDown(MM_EnvironmentBase *env);

	/**
	 * Overflow an item
	 *
	 * Overflow an item by dirtying the appropriate card
	 *
	 * @param item - item to overflow
	 *
	 */
	void overflowItemInternal(MM_EnvironmentBase *env, void *item);

private:

};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* OVERFLOWSEGREGATED_HPP_ */
