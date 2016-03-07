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

#include "HeapMemorySubSpaceIterator.hpp"

#include "Heap.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"

/**
 * Iterator states to maintain preorder traversal.
 */
enum {
	mm_heapmss_iterator_current_space,  /**< Process the subspace associated to the current space */
	mm_heapmss_iterator_current_subspace,  /**< Process the current subspace */
	mm_heapmss_iterator_children_subspace,  /**< Process the children of the current subspace */
	mm_heapmss_iterator_next_subspace,  /**< Move to the next sibling (or parent if unavailable) of the current subspace */
	mm_heapmss_iterator_next_space  /**< Move to the next space */
};

/**
 * Initialize the iterator to the beginning of the memory subspace list.
 */
void
MM_HeapMemorySubSpaceIterator::reset(MM_Heap *heap)
{
	_memorySpace = heap->getMemorySpaceList();
	_memorySubSpace = NULL;
	_state = mm_heapmss_iterator_current_space;
}

/**
 * Walk all memory subspaces for the given heap.
 * The list traversal is preorder, in that all parent nodes are visited before any of their children.
 * 
 * @todo Should integrate use of MM_MemorySubSpaceChildIterator.
 * @return Next memory subspace in the list, or NULL if all spaces have been processed.
 */
MM_MemorySubSpace *
MM_HeapMemorySubSpaceIterator::nextSubSpace()
{
	while(NULL != _memorySpace) {
		switch(_state) {
		case mm_heapmss_iterator_current_space:
			/* Process the subspace associated to the current space */
			_memorySubSpace = _memorySpace->getMemorySubSpaceList();
			_state = mm_heapmss_iterator_current_subspace;
			break;
		case mm_heapmss_iterator_current_subspace:
			/* Process the current subspace */
			if(NULL == _memorySubSpace) {
				_state = mm_heapmss_iterator_next_space;
				break;
			}
			_state = mm_heapmss_iterator_children_subspace;
			return _memorySubSpace;
			break;
		case mm_heapmss_iterator_children_subspace:
			/* Process the children of the current subspace */
			if(NULL == _memorySubSpace->getChildren()) {
				_state = mm_heapmss_iterator_next_subspace;
				break;
			}
			_memorySubSpace = _memorySubSpace->getChildren();
			_state = mm_heapmss_iterator_current_subspace;
			break;
		case mm_heapmss_iterator_next_subspace:
			/* Move to the next sibling (or parent if unavailable) of the current subspace */
			if(NULL == _memorySubSpace) {
				_state = mm_heapmss_iterator_next_space;
				break;
			}
			if(NULL == _memorySubSpace->getNext()) {
				/* Moving to the parent means we will just find its next sibling in the list */
				_memorySubSpace = _memorySubSpace->getParent();
				break;
			}
			_memorySubSpace = _memorySubSpace->getNext();
			_state = mm_heapmss_iterator_current_subspace;
			break;
		case mm_heapmss_iterator_next_space:
			/* Move to the next space */
			_memorySpace = _memorySpace->getNext();
			_state = mm_heapmss_iterator_current_space;
			break;
		}			
	}

	return NULL;
}

