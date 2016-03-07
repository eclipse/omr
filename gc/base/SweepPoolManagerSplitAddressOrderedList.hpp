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

#if !defined(SWEEPPOOLMANAGERSPLITADDRESSORDEREDLIST_HPP_)
#define SWEEPPOOLMANAGERSPLITADDRESSORDEREDLIST_HPP_

#include "omrcfg.h"
#include "modronopt.h"
#include "modronbase.h"

#if defined(OMR_GC_MODRON_STANDARD)

#include "Base.hpp"

#include "EnvironmentBase.hpp"
#include "MemoryPool.hpp"
#include "SweepPoolManagerAddressOrderedListBase.hpp"

class MM_SweepPoolManagerSplitAddressOrderedList : public MM_SweepPoolManagerAddressOrderedListBase
{
private:
	MMINLINE void setSplitCandidateInformation(MM_ParallelSweepChunk *chunk, MM_SweepPoolState *sweepState, MM_HeapLinkedFreeHeader* splitCandidate, MM_HeapLinkedFreeHeader* splitCandidatePreviousEntry)
	{
		chunk->_accumulatedFreeSize = sweepState->_sweepFreeBytes;
		chunk->_accumulatedFreeHoles = sweepState->_sweepFreeHoles;
		chunk->_splitCandidate = splitCandidate;
		chunk->_splitCandidatePreviousEntry = splitCandidatePreviousEntry;
	}
protected:
	MMINLINE virtual void connectChunkPostProcess(MM_ParallelSweepChunk *chunk, MM_SweepPoolState *sweepState, MM_HeapLinkedFreeHeader* splitCandidate, MM_HeapLinkedFreeHeader* splitCandidatePreviousEntry)
	{
		/* Set split candidate information */
		setSplitCandidateInformation(chunk, sweepState, splitCandidate, splitCandidatePreviousEntry);
	}
public:

	static MM_SweepPoolManagerSplitAddressOrderedList *newInstance(MM_EnvironmentBase *env);

	virtual void poolPostProcess(MM_EnvironmentBase *envModron, MM_MemoryPool *memoryPool);

	/**
	 * Create a SweepPoolManager object.
	 */
	MM_SweepPoolManagerSplitAddressOrderedList(MM_EnvironmentBase *env)
		: MM_SweepPoolManagerAddressOrderedListBase(env)
	{
		_typeId = __FUNCTION__;
	}

};

#endif /* defined(OMR_GC_MODRON_STANDARD) */
#endif /* SWEEPPOOLMANAGERSPLITADDRESSORDEREDLIST_HPP_ */
