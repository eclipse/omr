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

#include "SweepPoolState.hpp"

#include "EnvironmentBase.hpp"

bool 
MM_SweepPoolState::initialize(MM_EnvironmentBase *env)
{
	return true;
}


void 
MM_SweepPoolState::create(MM_EnvironmentBase *env, void *memPtr, MM_MemoryPool *memoryPool)
{
	MM_SweepPoolState *poolState = (MM_SweepPoolState *) memPtr;
	new(poolState) MM_SweepPoolState(memoryPool);
	poolState->initialize(env);
}

void 
MM_SweepPoolState::kill(MM_EnvironmentBase *env, J9Pool *pool, omrthread_monitor_t mutex)
{
	tearDown(env);

	omrthread_monitor_enter(mutex);
	pool_removeElement(pool, this);
	omrthread_monitor_exit(mutex);
}

MM_SweepPoolState *
MM_SweepPoolState::newInstance(MM_EnvironmentBase *env, J9Pool *pool, omrthread_monitor_t mutex, MM_MemoryPool *memoryPool)
{
	MM_SweepPoolState *sweepPoolState;
	
	omrthread_monitor_enter(mutex);
	sweepPoolState = (MM_SweepPoolState *)pool_newElement(pool);
	omrthread_monitor_exit(mutex);

	if (sweepPoolState) {
		new(sweepPoolState) MM_SweepPoolState(memoryPool);
		if (!sweepPoolState->initialize(env)) { 
			sweepPoolState->kill(env, pool, mutex);        
			sweepPoolState = NULL;            
		}                                       
	}

	return sweepPoolState;
}

void 
MM_SweepPoolState::tearDown(MM_EnvironmentBase *env)
{
	return;
}

MM_SweepPoolState::MM_SweepPoolState(MM_MemoryPool *memoryPool) :
	_memoryPool(memoryPool),
	_connectPreviousFreeEntry(NULL),
	_connectPreviousFreeEntrySize(0),
	_connectPreviousPreviousFreeEntry(NULL),
	_connectPreviousChunk(NULL),
	_sweepFreeBytes(0),
	_sweepFreeHoles(0),
	_largestFreeEntry(0),
	_previousLargestFreeEntry(NULL)
{
	_typeId = __FUNCTION__;
}
