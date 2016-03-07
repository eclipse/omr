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


#include "omrcfg.h"

#include "Debug.hpp"
#include "MemorySubSpace.hpp"

#include "MemorySubSpaceChildIterator.hpp"

/**
 * Iterator states to maintain preorder traversal.
 */
enum {
	mm_msschild_iterator_current_subspace,  /**< Process the current subspace */
	mm_msschild_iterator_children_subspace,  /**< Process the children of the current subspace */
	mm_msschild_iterator_next_subspace  /**< Move to the next sibling (or parent if unavailable) of the current subspace */
};

/**
 * Initialize the iterator to the beginning of the memory subspace list.
 */
void
MM_MemorySubSpaceChildIterator::reset(MM_MemorySubSpace *memorySubSpace)
{
	_memorySubSpaceBase = memorySubSpace;
	_memorySubSpace = memorySubSpace;
	_state = mm_msschild_iterator_current_subspace;
}

/**
 * Walk all memory subspaces for the given heap.
 * The list traversal is preorder, in that all parent nodes are visited before any of their children.
 * @return Next memory subspace in the list, or NULL if all spaces have been processed.
 */
MM_MemorySubSpace *
MM_MemorySubSpaceChildIterator::nextSubSpace()
{
	while(NULL != _memorySubSpace) {
		switch(_state) {
		case mm_msschild_iterator_current_subspace:
			/* Process the current subspace */
			if(NULL == _memorySubSpace) {
				/* How did we get here? */
				assume0(0);
				_state = mm_msschild_iterator_current_subspace;
				break;
			}
			_state = mm_msschild_iterator_children_subspace;
			return _memorySubSpace;
			break;
		case mm_msschild_iterator_children_subspace:
			/* Process the children of the current subspace */
			if(NULL == _memorySubSpace->getChildren()) {
				_state = mm_msschild_iterator_next_subspace;
				break;
			}
			_memorySubSpace = _memorySubSpace->getChildren();
			_state = mm_msschild_iterator_current_subspace;
			break;
		case mm_msschild_iterator_next_subspace:
			/* Move to the next sibling (or parent if unavailable) of the current subspace */
			if(NULL == _memorySubSpace) {
				/* How did we get here? */
				assume0(0);
				_state = mm_msschild_iterator_current_subspace;
				break;
			}
			/* If the current subspace being processed is the base subspace, we are done */
			if(_memorySubSpace == _memorySubSpaceBase) {
				_memorySubSpace = NULL;
				_state = mm_msschild_iterator_current_subspace;
				break;
			}
			/* Move to the next/parent subspace */
			if(NULL == _memorySubSpace->getNext()) {
				/* Moving to the parent means we will just find its next sibling in the list */
				_memorySubSpace = _memorySubSpace->getParent();
				break;
			}
			_memorySubSpace = _memorySubSpace->getNext();
			_state = mm_msschild_iterator_current_subspace;
			break;
		}			
	}

	return NULL;
}

