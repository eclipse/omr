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


#if !defined(PACKETLIST_HPP_)
#define PACKETLIST_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "ModronAssertions.h"

#include "AtomicOperations.hpp"
#include "BaseNonVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "LightweightNonReentrantLock.hpp"
#include "Packet.hpp"

class MM_GCExtensionsBase;

class MM_PacketList: public MM_BaseNonVirtual
{

/* Data Section */
public:
	struct PacketSublist {
		MM_Packet * _head;  /**< Head of the list */
		MM_Packet * _tail;  /**< Tail of the list */
		MM_LightweightNonReentrantLock _lock;  /**< Lock for getting/putting packets */
	};
	
protected:
	
private:
	struct PacketSublist *_sublists;	/**< An array of PacketSublist structures which is _sublistCount elements long */
	
	uintptr_t _sublistCount; /**< the number of lists (split for parallelism). Must be at least 1 */
	volatile uintptr_t _count;  /**< Number of items in the list */
	
/* Functionality Section */
private:
	/**
	 * Increment the shared counter by the specified amount.
	 * Must be called inside of a locked region
	 * 
	 * @param value the positive value to increment
	 */
	void incrementCount(uintptr_t value)
	{
		if (1 == _sublistCount) {
			_count += value;
		} else {
			/* use an atomic, as the locks have been split up */
			MM_AtomicOperations::add(&_count, value);
		}
	}
	
	/**
	 * Decrement the shared counter by the specified amount.
	 * Must be called inside of a locked region
	 *
	 * @param value the positive value to decrement
	 */
	void decrementCount(uintptr_t value)
	{
		if (1 == _sublistCount) {
			_count -= value;
		} else {
			/* use an atomic, as the locks have been split up */
			MM_AtomicOperations::subtract(&_count, value);
		}
	}

	/**
	 * Hash the specified environment to determine what sublist index
	 * it should use
	 * 
	 * @param env the current environment
	 * 
	 * @return an index into the _sublists array
	 */
	MMINLINE uintptr_t
	getSublistIndex(MM_EnvironmentBase *env)
	{
		return env->getEnvironmentId() % _sublistCount;
	}
		
protected:
	
public:
	
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env) ;
	
	/**
	 * Push a list of packets onto this packet list.
	 * 
	 * @param head The first entry in the list
	 * @param tail The last entry in the list
	 * @param count The number of entries in the list
	 */
	void pushList(MM_Packet *head, MM_Packet *tail, uintptr_t count);
	
	/**
	 * Pop all of the entries from this packetlist into the references
	 * passed as parameters.
	 * 
	 * @note this function will destroy the values passes in as parameters.
	 * 
	 * @param head The first entry in the list
	 * @param end The last entry in the list
	 * @param count The number of entries in the list
	 * 
	 * @return true is there were entries, false otherwise
	 */
	bool popList(MM_Packet **head, MM_Packet **tail, uintptr_t *count);
	void remove(MM_Packet *packetToRemove);
	
	/**
	 * Push a packet on the packetList.
	 *
	 * @param packet The packet to put on the list
	 */
	MMINLINE void push(MM_EnvironmentBase *env, MM_Packet *packet)
	{
		uintptr_t index = getSublistIndex(env);
		PacketSublist *list = &_sublists[index];
	
		list->_lock.acquire();

		packet->_next = list->_head;
		packet->_previous = NULL;
		packet->setSublistIndex(index);
		if (NULL == list->_head) {
			list->_tail = packet;
		} else {
			list->_head->_previous = packet;
		}
		list->_head = packet;
		incrementCount(1);
		
		list->_lock.release();
	}
	
	/**
	 * Pop a packet off of the packetList.
	 *
	 * @return packet The packet to put on the list
	 */
	MMINLINE MM_Packet *pop(MM_EnvironmentBase *env)
	{
		uintptr_t index = getSublistIndex(env);
		MM_Packet *packet = NULL;

		for (uintptr_t i = 0; i < _sublistCount; i++) {
			PacketSublist *list = &_sublists[index];

			if (NULL != list->_head) {
				list->_lock.acquire();
				if (NULL != list->_head) {
					packet = list->_head;
					list->_head = packet->_next;
					decrementCount(1);
					if (NULL == list->_head) {
						list->_tail = NULL;
					} else {
						list->_head->_previous = NULL;
					}		
				}
				list->_lock.release();

				if (NULL != packet) {
					break;
				}
			}
			
			index = (index + 1) %  _sublistCount;
		}

		return packet;
	}
	
	/**
	 * Check to see if the list is empty
	 *
	 * @return true if the list is empty, false otherwise
	 */
	MMINLINE bool isEmpty() 
	{
		return 0 == _count;
	}
	
	/**
	 * Return the count of how many entries there are in the list
	 *
	 * @return count The number of entries in the list
	 */
	MMINLINE uintptr_t getCount()
	{
		return _count;
	}
	
	/**
	 * Return the first element in the list.
	 * This should be avoided as it combines all sublists in to one
	 * single list which is a big hit to performance.
	 *
	 * @return head The first entry in the list
	 */
	MM_Packet *getHead();
	
	/**
	 * Create a PacketList object.
	 */
	MM_PacketList(MM_EnvironmentBase *env) :
		MM_BaseNonVirtual()
		,_sublists(NULL)
		,_sublistCount(0)
		,_count(0)
	{
		_typeId = __FUNCTION__;
	}
	
protected:
private:
	
	friend class MM_PacketSublistIterator;
};
#endif /* PACKETLIST_HPP_ */
