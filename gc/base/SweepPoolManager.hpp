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


/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(SWEEPPOOLMANAGER_HPP_)
#define SWEEPPOOLMANAGER_HPP_

#include "omrcfg.h"
#include "modronopt.h"
#include "modronbase.h"

#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_GCExtensionsBase;
class MM_MemoryPool;
class MM_ParallelSweepChunk;
class MM_SweepPoolState;

/*
 * 	 superclass MM_SweepPoolManager
 */

class MM_SweepPoolManager : public MM_BaseVirtual
{
private:

protected:

	MM_GCExtensionsBase *_extensions;

	virtual void tearDown(MM_EnvironmentBase *env);
	virtual bool initialize(MM_EnvironmentBase *env);

public:

	void kill(MM_EnvironmentBase *env);

	/**
	 * Flush any unaccounted for free entries to the free list.
	 */
	virtual void flushFinalChunk(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool) = 0;

	/**
	 *  Finally if there is at least 1 entry in the subspace, the last entry should be connected to NULL in the free list
	 */
	virtual void connectFinalChunk(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool) = 0;
	
	virtual void poolPostProcess(MM_EnvironmentBase *env, MM_MemoryPool *memoryPool) = 0;

	/**
	 * Connect a chunk into the free list.
	 * Given a previously swept chunk, connect its data to the free list of the associated memory subspace.
	 * This requires that all previous chunks in the memory subspace have been swept.
	 * @note It is expected that the sweep state free information is updated properly.
	 * @note the free list is not guaranteed to be in a consistent state at the end of the routine
	 * @note a chunk can only be connected by a single thread (single threaded routine)
	 * @todo add the general algorithm to the comment header.
	 */
	virtual void connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk) = 0;

	/**
	 * 	Add free memory slot to pool list
	 * 
	 * @param sweepChunk current memory chunk
	 * @param address start address of memory slot
	 * @param size size of free memory slot
	 */
	virtual bool addFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, uintptr_t *heapSlotFreeHead, uintptr_t heapSlotFreeCount) = 0;

	/**
	 * Update trailing free memory
	 *
	 * @param sweepChunk current chunk
	 * @param trailingCandidate trailing candidate address
	 * @param trailingCandidateSlotCount trailing candidate size
	 */
	virtual void updateTrailingFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, uintptr_t *heapSlotFreeHead, uintptr_t heapSlotFreeCount) = 0;

	/**
	 * Get the sweep scheme state for the given memory pool.
	 */
	virtual MM_SweepPoolState *getPoolState(MM_MemoryPool *memoryPool) = 0;

	/**
	 * Create a SweepPoolManager object.
	 */
	MM_SweepPoolManager(MM_EnvironmentBase *env)
		: _extensions(env->getExtensions())
	{
		_typeId = __FUNCTION__;
	}

};
#endif /* SWEEPPOOLMANAGER_HPP_ */
