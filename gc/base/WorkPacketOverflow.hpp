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

#if !defined(WORKPACKETOVERFLOW_HPP_)
#define WORKPACKETOVERFLOW_HPP_

#include "omr.h"

#include "BaseVirtual.hpp"

class MM_GCExtensionsBase;
class MM_EnvironmentBase;
class MM_Packet;
class MM_WorkPackets;

typedef enum {
	OVERFLOW_TYPE_WORKSTACK = 1,
	OVERFLOW_TYPE_BARRIER = 2
} MM_OverflowType;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_WorkPacketOverflow : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
public:
	volatile bool _overflow;	/**< Volatile flag to report about overflow latched */
	
protected:
	MM_WorkPackets *_workPackets;
	omrthread_monitor_t _overflowListMonitor; /**< Monitor to be held when modifying the overflow list */
private:

	/*
	 * Function members
	 */
public:
	static MM_WorkPacketOverflow *newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets);
	virtual void kill(MM_EnvironmentBase *env);
			
	virtual void reset(MM_EnvironmentBase *env);

	/**
	 * Return TRUE if list is empty, FALSE otherwise
	 */
	virtual bool isEmpty();

	virtual void emptyToOverflow(MM_EnvironmentBase *env,MM_Packet *packet, MM_OverflowType type);
	virtual void fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet);
	virtual void overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type);

	/**
	 * Handle Overflow - clean card table
	 * @param env current thread environment
	 */
	virtual void handleOverflow(MM_EnvironmentBase *env);

	/**
	 * Create a WorkPacketOverflow object.
	 */
	MM_WorkPacketOverflow(MM_EnvironmentBase *env, MM_WorkPackets *workPackets) :
		MM_BaseVirtual()
		, _overflow(false)
		, _workPackets(workPackets)
		, _overflowListMonitor(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
private:
};

#endif /* WORKPACKETOVERFLOW_HPP_ */
