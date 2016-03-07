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

#include "Packet.hpp"
#include "EnvironmentBase.hpp"

/*
 * Get top of current packet.
 * 
 * @return element at top of current packet
 */ 
void *
MM_Packet::peek(MM_EnvironmentBase *env)
{
	
	if(_currentPtr > _basePtr) {
		void *result = (void *)*(_currentPtr - 1);
		return result;
	}
	
	return NULL;
}

/*
 * Initialize a packet. 
 * 
 * @return TRUE is packet initialized OK; FALSE otheriwse
 */ 
bool
MM_Packet::initialize(MM_EnvironmentBase *env, MM_Packet *next, MM_Packet *previous, uintptr_t *baseAddress,  uintptr_t size)
{

	_next = next;
	_previous = previous;

	_baseAddress = baseAddress;

	_basePtr = _baseAddress;
	_topPtr = _baseAddress + size;
	_currentPtr = _baseAddress;
	
	_owner = NULL;

	return true;
}
