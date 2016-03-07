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
