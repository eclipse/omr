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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(CONCURRENTOVERFLOW_HPP_)
#define CONCURRENTOVERFLOW_HPP_

#include "omr.h"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "WorkPacketOverflow.hpp"

class MM_ConcurrentCardTable;
class MM_ConcurrentGC;
class MM_EnvironmentStandard;
class MM_Packet;
class MM_WorkPackets;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentOverflow : public MM_WorkPacketOverflow
{
/* Data members */
public:
protected:
private:
	MM_GCExtensionsBase *_extensions;
#if defined(OMR_GC_MODRON_SCAVENGER)
	omrthread_monitor_t _cardsClearingMonitor;	/**< monitor for safe initial Cards clearing in WP Overflow handler */
	bool _cardsForNewSpaceCleared;	/**< cards must be cleared at first overflow in Concurrent cycle */
#endif /*  OMR_GC_MODRON_SCAVENGER */
	
/* Methods */
public:
	static MM_ConcurrentOverflow *newInstance(MM_EnvironmentBase *env, MM_WorkPackets *workPackets); 

	virtual void reset(MM_EnvironmentBase *env);
	virtual bool isEmpty();

	virtual void emptyToOverflow(MM_EnvironmentBase *env, MM_Packet *packet, MM_OverflowType type);
	virtual void overflowItem(MM_EnvironmentBase *env, void *item, MM_OverflowType type);

	/**
	 * Fill a packet from overflow list
	 *
	 * @param packet - Reference to packet to be filled.
	 *
	 * @note No-op in this overflow handler. We will never fill a packet from
	 * the card table.
	 */
	virtual void fillFromOverflow(MM_EnvironmentBase *env, MM_Packet *packet);

#if defined(OMR_GC_MODRON_SCAVENGER)
	MMINLINE void setCardsForNewSpaceNotCleared()
	{
		_cardsForNewSpaceCleared = false;
	}
#endif /*  OMR_GC_MODRON_SCAVENGER */

	/**
	 * Handle Overflow - clean card table
	 * @param env current thread environment
	 */
	virtual void handleOverflow(MM_EnvironmentBase *env);

	/**
	 * Create a ConcurrentOverflow object.
	 */	
	MM_ConcurrentOverflow(MM_EnvironmentBase *env,MM_WorkPackets *workPackets) :
		MM_WorkPacketOverflow(env,workPackets)
		,_extensions(env->getExtensions())
#if defined(OMR_GC_MODRON_SCAVENGER)
		,_cardsClearingMonitor(NULL)
		,_cardsForNewSpaceCleared(false)
#endif /*  OMR_GC_MODRON_SCAVENGER */
	{
		_typeId = __FUNCTION__;
	};
	
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);
private:

	/**
	 * Overflow an item
	 *
	 * Overflow an item by dirtying the appropriate card
	 *
	 * @param item - item to overflow
	 * @param type - ignored for concurrent collector
	 *
	 */
	void overflowItemInternal(MM_EnvironmentBase *env, void *item, MM_ConcurrentCardTable *cardTable);

#if defined(OMR_GC_MODRON_SCAVENGER)
	/*
	 * Clear cards for new space if necessary.
	 * Cards must be cleared at the first overflow in STW part Concurrent GC cycle
	 * @param env Thread Environment
	 * @param collector reference to global Concurrent collector
	 */
	void clearCardsForNewSpace(MM_EnvironmentStandard *env, MM_ConcurrentGC *collector);
#endif /*  OMR_GC_MODRON_SCAVENGER */

};

#endif /* CONCURRENTOVERFLOW_HPP_ */
