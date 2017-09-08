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
