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
#include "omr.h"

#include "HeapMemoryPoolIterator.hpp"
#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"

/**
 * Initialize the iterator to the beginning of the memory subspace list.
 */
void
MM_HeapMemoryPoolIterator::reset()
{
	_memorySubSpace = _mssIterator.nextSubSpace();
	_memoryPool = NULL;
	_state = mm_heapmp_iterator_next_subspace;
}


/**
 * Walk all memory pools for the given heap.
 * The list traversal is preorder, in that all parent nodes are visited before any of their children.
 * 
 * @return Next memory pool, or NULL if all pools have been processed.
 */
MM_MemoryPool *
MM_HeapMemoryPoolIterator::nextPool()
{
	MM_MemoryPool *nextPool;
	
	while(NULL != _memorySubSpace) { 
		
		switch(_state) {
			
		case mm_heapmp_iterator_next_subspace:
						
			if(NULL != _memorySubSpace->getMemoryPool()) {
				_memoryPool = _memorySubSpace->getMemoryPool();
		
				/* Does this Memory pool have children ? */		
				if(NULL != _memoryPool->getChildren()) {
					/* Yes ..So we only return details of its children */
					_memoryPool = _memoryPool->getChildren();
				}
				
				_state = mm_heapmp_iterator_next_memory_pool;
				break;
			}
			
			_memorySubSpace = _mssIterator.nextSubSpace();
			break;
			
		case mm_heapmp_iterator_next_memory_pool:
			nextPool = _memoryPool;
			_memoryPool= _memoryPool->getNext(); 
			
			/* Any more children ? */
			if (NULL == _memoryPool) {
				_memorySubSpace = _mssIterator.nextSubSpace();
				_state = mm_heapmp_iterator_next_subspace;
			}		 
			
			return nextPool;
			break;
		}	
	}	
	
	return NULL;
}

/**
 * Walk all memory pools for the given subspace. Based on nextPool() - the only difference is that when we want to move to next _memorySubSpace we set it to NULL.
 * The list traversal is preorder, in that all parent nodes are visited before any of their children.
 *
 * @return Next memory pool, or NULL if all pools have been processed.
 */
MM_MemoryPool *
MM_HeapMemoryPoolIterator::nextPoolInSubSpace()
{
	MM_MemoryPool *nextPool;

	while (NULL != _memorySubSpace) {

		switch(_state) {

		case mm_heapmp_iterator_next_subspace:

			if(NULL != _memorySubSpace->getMemoryPool()) {
				_memoryPool = _memorySubSpace->getMemoryPool();

				/* Does this Memory pool have children ? */
				if(NULL != _memoryPool->getChildren()) {
					/* Yes ..So we only return details of its children */
					_memoryPool = _memoryPool->getChildren();
				}

				_state = mm_heapmp_iterator_next_memory_pool;
				break;
			}

			_memorySubSpace = NULL;
			break;

		case mm_heapmp_iterator_next_memory_pool:
			nextPool = _memoryPool;
			_memoryPool= _memoryPool->getNext();

			/* Any more children ? */
			if (NULL == _memoryPool) {
				_memorySubSpace = NULL;
				_state = mm_heapmp_iterator_next_subspace;
			}

			return nextPool;
			break;
		}
	}

	return NULL;
}
