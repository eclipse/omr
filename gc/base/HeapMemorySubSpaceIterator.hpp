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

#if !defined(HEAPMEMORYSUBSPACEITERATOR_HPP_)
#define HEAPMEMORYSUBSPACEITERATOR_HPP_

#include "omrcomp.h"

class MM_Heap;
class MM_MemorySpace;
class MM_MemorySubSpace;

/**
 * Iterate through all visible memory subspaces in the system.
 * Provides a preordered walk of all memory subspaces for a MM_Heap.
 * 
 * @ingroup GC_Base
 */	
class MM_HeapMemorySubSpaceIterator
{
private:
	MM_MemorySpace *_memorySpace;
	MM_MemorySubSpace *_memorySubSpace;
	uintptr_t _state;

	void reset(MM_Heap *heap);

protected:
public:
	MM_MemorySubSpace *nextSubSpace();

	MM_HeapMemorySubSpaceIterator(MM_Heap *heap) :
		_memorySpace(NULL),
		_memorySubSpace(NULL),
		_state(0)
	{
		reset(heap);
	}
		
};

#endif /* HEAPMEMORYSUBSPACEITERATOR_HPP_ */
