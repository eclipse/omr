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
