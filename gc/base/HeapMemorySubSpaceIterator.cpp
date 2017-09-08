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

