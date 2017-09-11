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

#if !defined(SWEEPPOOLSTATE_HPP_)
#define SWEEPPOOLSTATE_HPP_

#include "omrcfg.h"
#include "omrport.h"
#include "omrpool.h"
#include "modronopt.h"
#include "modronbase.h"

#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_MemoryPool;
class MM_HeapLinkedFreeHeader;
class MM_ParallelSweepChunk;

/**
 * Sweep state of a given memory pool.
 * @ingroup GC_Base
 */
class MM_SweepPoolState : public MM_BaseVirtual
{
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	
public:
	MM_MemoryPool *_memoryPool;  /**< Subspace to which the receiver is associated */

	MM_HeapLinkedFreeHeader *_connectPreviousFreeEntry;  /**< Last free entry to be connected into the subspace */
	uintptr_t _connectPreviousFreeEntrySize;  /**< Last free entry size to be connected into the subspace */
	MM_HeapLinkedFreeHeader *_connectPreviousPreviousFreeEntry;  /**< previous free entry of the last free entry to be connected into the subspace */
	MM_ParallelSweepChunk *_connectPreviousChunk;  /**< Last chunk connected into the subspace */

	uintptr_t _sweepFreeBytes;  /** Number of free bytes connected */
	uintptr_t _sweepFreeHoles;  /**< Number of free entries connected */
	uintptr_t _largestFreeEntry;  /**< Largest free entry found during the connection phase of a sweep */
	MM_HeapLinkedFreeHeader *_previousLargestFreeEntry; /**< previous free entry of the Largest Free Entry */
	
	/**
	 * Build a MM_SweepPoolState object within the memory supplied
	 * @param memPtr pointer to the memory to build the object within
	 */
	static void create(MM_EnvironmentBase *env, void *memPtr, MM_MemoryPool *memoryPool);

	/**
	 * Free the receiver and all associated resources.
	 * @param pool J9Pool used for allocation 
	 * @param mutex mutex to protect J9Pool operations
	 */
	virtual void kill(MM_EnvironmentBase *env, J9Pool *pool, omrthread_monitor_t mutex);

	/**
	 * Allocate and initialize a new instance of the receiver.
	 * @param pool J9Pool should be used for allocation
	 * @param mutex mutex to protect J9Pool operations
	 * @param memoryPool memory pool this sweepPoolState should be associated with
	 * @return a new instance of the receiver, or NULL on failure.
	 */
	static MM_SweepPoolState *newInstance(MM_EnvironmentBase *env, J9Pool *pool, omrthread_monitor_t mutex, MM_MemoryPool *memoryPool);

	virtual void tearDown(MM_EnvironmentBase *env);

	/**
	 * Initialize the data for sweep
	 */
	virtual void initializeForSweep(MM_EnvironmentBase *env) {
		_connectPreviousFreeEntry = NULL;
		_connectPreviousFreeEntrySize = 0;
		_connectPreviousPreviousFreeEntry = NULL;
		_connectPreviousChunk = NULL;
		_largestFreeEntry = 0;
		_previousLargestFreeEntry = NULL;
		_sweepFreeBytes = 0;
		_sweepFreeHoles = 0;
	}
	
	/**
	 * Reset all statistics that pertain to the free list.
	 */
	MMINLINE void resetFreeStats() {
		_largestFreeEntry = 0;
		_previousLargestFreeEntry = NULL;
		_sweepFreeHoles = 0;
		_sweepFreeBytes = 0;
	}

	MM_SweepPoolState(MM_MemoryPool *memoryPool);
	
	/**
	 * Update the previous largest Free Entry and the largest free entry size (only be called from SweepPoolManagerSplitAddressOrderedList)
	 */
	MMINLINE void updateLargestFreeEntry(UDATA largestFreeEntrySizeCandidate, MM_HeapLinkedFreeHeader * previousLargestFreeEntryCandidate)
	{
		if (largestFreeEntrySizeCandidate > _largestFreeEntry) {
			_previousLargestFreeEntry = previousLargestFreeEntryCandidate;
			_largestFreeEntry = largestFreeEntrySizeCandidate;
 		}
	}
};

#endif /* SWEEPPOOLSTATE_HPP_ */
