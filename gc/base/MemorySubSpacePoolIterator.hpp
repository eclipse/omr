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

#if !defined(MEMORYSUBSPACEPOOLITERATOR_HPP_)
#define MEMORYSUBSPACEPOOLITERATOR_HPP_

#include "omrcfg.h"

#include "MemorySubSpaceChildIterator.hpp"

class MM_MemoryPool;
class MM_MemorySubSpace;

/**
 * Iterate through all visible memory pools for a given memory sub space.
 * Provides a preordered walk of all memory subspaces siblings to find all memory pools
 * for a memory subspace.
 * 
 * @ingroup GC_Base
 */	
class MM_MemorySubSpacePoolIterator
{
private:
	MM_MemorySubSpaceChildIterator _mssChildIterator;
	MM_MemorySubSpace *_memorySubSpace;
	MM_MemoryPool *_memoryPool;
	uintptr_t _state;
	
	void reset();

protected:
public:
	MM_MemoryPool *nextPool();

	MM_MemorySubSpacePoolIterator(MM_MemorySubSpace *memorySubSpace) :
		_mssChildIterator(memorySubSpace),
		_memorySubSpace(NULL),
		_memoryPool(NULL),
		_state(0)
	{
		reset();
	}
		
};

#endif /* MEMORYSUBSPACEPOOLITERATOR_HPP_ */
