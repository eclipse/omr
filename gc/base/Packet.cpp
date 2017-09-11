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
