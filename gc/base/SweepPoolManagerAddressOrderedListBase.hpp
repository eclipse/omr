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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(SWEEPPOOLMANAGERADDRESSORDEREDLISTBASE_HPP_)
#define SWEEPPOOLMANAGERADDRESSORDEREDLISTBASE_HPP_

#include "omrcfg.h"
#include "modronopt.h"
#include "modronbase.h"

#if defined(OMR_GC_MODRON_STANDARD)

#include "Base.hpp"
#include "SweepPoolManager.hpp"
#include "ParallelSweepChunk.hpp"
#include "SweepPoolState.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_MemoryPool;
class MM_HeapLinkedFreeHeader;

class MM_SweepPoolManagerAddressOrderedListBase : public MM_SweepPoolManager
{
private:

protected:

	MMINLINE void calculateTrailingDetails(MM_ParallelSweepChunk *sweepChunk, uintptr_t *trailingCandidate, uintptr_t trailingCandidateSlotCount);
	MMINLINE virtual void connectChunkPostProcess(MM_ParallelSweepChunk *chunk, MM_SweepPoolState *sweepState, MM_HeapLinkedFreeHeader* splitCandidate, MM_HeapLinkedFreeHeader* splitCandidatePreviousEntry){}
	MMINLINE void updateLargestFreeEntryInChunk(MM_ParallelSweepChunk *chunk, MM_SweepPoolState *sweepState, MM_HeapLinkedFreeHeader* previousFreeEntry)
	{
		if (chunk->_largestFreeEntry > sweepState->_largestFreeEntry) {
			/* _previousLargestFreeEntry is only for SAOL */
			if (NULL == chunk->_previousLargestFreeEntry) {
				sweepState->_previousLargestFreeEntry = previousFreeEntry;
			} else {
				sweepState->_previousLargestFreeEntry = chunk->_previousLargestFreeEntry;
			}
			sweepState->_largestFreeEntry = chunk->_largestFreeEntry;
		}
	}

public:

	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void flushFinalChunk(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool);
	virtual void connectFinalChunk(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool);
	virtual void poolPostProcess(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool){}
	virtual void connectChunk(MM_EnvironmentBase *env, MM_ParallelSweepChunk *chunk);
	virtual bool addFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, uintptr_t *heapSlotFreeHead, uintptr_t heapSlotFreeCount);
	virtual void updateTrailingFreeMemory(MM_EnvironmentBase *env, MM_ParallelSweepChunk *sweepChunk, uintptr_t *heapSlotFreeHead, uintptr_t heapSlotFreeCount);
	virtual MM_SweepPoolState *getPoolState(MM_MemoryPool *memoryPool);

	/**
	 * Create a SweepPoolManager object.
	 */
	MM_SweepPoolManagerAddressOrderedListBase(MM_EnvironmentBase *env)
		: MM_SweepPoolManager(env)
	{
//		_typeId = __FUNCTION__;
	}

};
#endif /* defined(OMR_GC_MODRON_STANDARD) */
#endif /* SWEEPPOOLMANAGERADDRESSORDEREDLISTBASE_HPP_ */
