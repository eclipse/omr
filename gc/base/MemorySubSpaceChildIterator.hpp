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

#if !defined(MEMORYSUBSPACECHILDITERATOR_HPP_)
#define MEMORYSUBSPACECHILDITERATOR_HPP_

#include "omrcfg.h"

class MM_MemorySubSpace;

/**
 * Iterate through all memory subspaces that are children of the given memory subspace (including the given space).
 * Provides a preordered walk of all memory subspaces for a memory subspace (including the given space).
 * 
 * @ingroup GC_Base
 */	
class MM_MemorySubSpaceChildIterator
{
private:
	MM_MemorySubSpace *_memorySubSpaceBase;  /**< The starting (and parent) memory subspace to traverse */
	MM_MemorySubSpace *_memorySubSpace;  /**< Current memory subspace being iterated over */
	uintptr_t _state;  /**< State of iteration on current memory subspace */

	void reset(MM_MemorySubSpace *memorySubSpace);

protected:
public:
	MM_MemorySubSpace *nextSubSpace();

	MM_MemorySubSpaceChildIterator(MM_MemorySubSpace *memorySubSpace) :
		_memorySubSpaceBase(NULL),
		_memorySubSpace(NULL),
		_state(0)
	{
		reset(memorySubSpace);
	}
		
};

#endif /* MEMORYSUBSPACECHILDITERATOR_HPP_ */
