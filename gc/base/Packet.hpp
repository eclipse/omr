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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/


#if !defined(PACKET_HPP_)
#define PACKET_HPP_

#include "omr.h"
#include "modronbase.h"
#include <string.h>

#include "BaseNonVirtual.hpp"

#define PACKET_RETURNED 0x01

class MM_EnvironmentBase;

class MM_Packet : public MM_BaseNonVirtual
{
	/*
	 * Data members
	 */
private:
	uintptr_t *_baseAddress;
	uintptr_t *_basePtr;
	uintptr_t *_topPtr;
	uintptr_t *_currentPtr;
	uintptr_t _sublistIndex;
	MM_EnvironmentBase *_owner;
protected:
public:
	MM_Packet *_next;
	MM_Packet *_previous;

	/*
	 * Function members
	 */
private:
	MMINLINE uintptr_t getSublistIndex()
	{
		return _sublistIndex;
	}

	MMINLINE void setSublistIndex(uintptr_t sublistIndex)
	{
		_sublistIndex = sublistIndex;
	}

protected:
public:
	/**
	 * Returns the number of free slots
	 */
	MMINLINE uintptr_t freeSlots()
	{
		return (uintptr_t)(_topPtr - _currentPtr);
	}

	/**
	 * Sets the address of the owning threads env
	 */
	MMINLINE void setOwner(MM_EnvironmentBase *owner)
	{
		_owner = owner;
	}
	
	/**
	 * Clears the address of the owning threads env
	 */
	MMINLINE void clearOwner()
	{
		_owner = NULL;
	}
	
	/**
	 * Low tags the address of the owning threads env so we know its no longer owned by
	 * a thread but we do know which thread last used it for input/output
	 */
	MMINLINE void resetOwner()
	{
		_owner = (MM_EnvironmentBase *)((uintptr_t)_owner | PACKET_RETURNED);
	}
	
	void *peek(MM_EnvironmentBase *env);
	
	/**
	 * Return the address of currentPtr for this packet.
	 * 
	 * @note currently only used by realtime
	 * @return the address of currentPtr
	 */
	uintptr_t **getCurrentAddr(MM_EnvironmentBase *env)
	{
		return &_currentPtr;
	}
	
	/**
	 * Return the address of topPtr for this packet.
	 * 
	 * @note currently only used by realtime
	 * @return the address of topPtr
	 */
	uintptr_t **getTopAddr(MM_EnvironmentBase *env)
	{
		return &_topPtr;
	}
	
	/**
	 * Set _currentPtr equal to _topPtr so
	 * the packet looks as if it is full.
	 * 
	 * @note currently only used by realtime
	 */
	void setFull(MM_EnvironmentBase *env)
	{
		_currentPtr = _topPtr;
	}
	
	MMINLINE bool isFull(MM_EnvironmentBase *env)
	{
		return (_currentPtr == _topPtr);
	}

	MMINLINE void *pop(MM_EnvironmentBase *env)
	{
		if(_currentPtr > _basePtr) {
			void *result = (void *)*(--_currentPtr);
			return result;
		}

		return (void *)NULL;
	}

	/*
	 * Push a single element to the current packet
	 *
	 * @param element - element to push
	 * @return TRUE is room in current packet for element; FALSE otherwise
	 *
	 */
	MMINLINE bool push(MM_EnvironmentBase *env, void *element)
	{
		if(_currentPtr < _topPtr) {
			*_currentPtr++ = (uintptr_t)element;
			return true;
		}

		return false;
	}

	/*
	 * Push a pair of elements to the current packet.
	 *
	 * @param element1 - first element to push
	 * @param element2 - second element to push
	 * @return TRUE is room in current packet for two elements; FALSE otherwise
	 */
	MMINLINE bool push(MM_EnvironmentBase *env, void *element1, void *element2)
	{
		if((_currentPtr + 1) < _topPtr) {
			*_currentPtr++ = (uintptr_t)element2;
			*_currentPtr++ = (uintptr_t)element1;
			return true;
		}
		return false;
	}

	/**
	 * Returns TRUE if the packet is empty, FALSE otherwise
	 */
	MMINLINE bool isEmpty()
	{
		return _currentPtr == _basePtr;
	}
	
	/**
	 * Zero the storage area for the packet.
	 */
	MMINLINE void clear(MM_EnvironmentBase *env)
	{
		memset(_basePtr, 0, ((uint8_t*)_topPtr-(uint8_t*)_basePtr));
	}

	bool initialize(MM_EnvironmentBase *env, MM_Packet *next, MM_Packet *previous, uintptr_t *baseAddress,  uintptr_t size);

	/**
	 * Create a Packet object.
	 */
	MM_Packet() :
		MM_BaseNonVirtual(),
		_baseAddress(NULL),
		_basePtr(NULL),
		_topPtr(NULL),
		_currentPtr(NULL),
		_sublistIndex(0),
		_owner(NULL),
		_next(NULL),
		_previous(NULL)
	{
		_typeId = __FUNCTION__;
	}
		
protected:
	/*
	 * Reset current packet to empty.  This should only be called 
	 * when all current packet data is discarded.  
	 * 
	 * @NOTE If this is called without restarting all processing 
	 * it will likely cause incorrect behaviour
	 * 
	 */ 
	MMINLINE void resetData(MM_EnvironmentBase *env)
	{
		_currentPtr = _baseAddress;
	
	}
	
	/*
	 * Friends
	 */
	friend class MM_PacketList;
	friend class MM_PacketSlotIterator;
	friend class MM_WorkPackets;
};

#endif /* PACKET_HPP_ */
