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
