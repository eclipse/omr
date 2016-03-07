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
#include "MemoryPool.hpp"
#include "MemorySubSpace.hpp"

#include "MemorySubSpacePoolIterator.hpp"

/**
 * Iterator states to maintain preorder traversal.
 */
enum {
	mm_msspool_iterator_next_subspace,  	 /**< Get to next subspace	   */
	mm_msspool_iterator_next_memory_pool	 /**< Get to next pool		   */
};

/**
 * Initialize the iterator to the beginning of the memory subspace list.
 */
void
MM_MemorySubSpacePoolIterator::reset()
{
	_memorySubSpace = _mssChildIterator.nextSubSpace();
	_memoryPool = NULL;
	_state = mm_msspool_iterator_next_subspace;
}


/**
 * Walk all memory pools for the memory subspace.
 * The list traversal is preorder, in that all parent nodes are visited before 
 * any of their children.
 * 
 * @return Next memory pool, or NULL if all pools have been processed.
 */
MM_MemoryPool *
MM_MemorySubSpacePoolIterator::nextPool()
{
	MM_MemoryPool *nextPool;
	
	while(NULL != _memorySubSpace) { 
		
		switch(_state) {
			
		case mm_msspool_iterator_next_subspace:
						
			if(NULL != _memorySubSpace->getMemoryPool()) {
				_memoryPool = _memorySubSpace->getMemoryPool();
		
				/* Does this Memory pool have children ? */		
				if(NULL != _memoryPool->getChildren()) {
					/* Yes ..So we only return details of its children */
					_memoryPool = _memoryPool->getChildren();
				}
				
				_state = mm_msspool_iterator_next_memory_pool;
				break;
			}
			
			_memorySubSpace = _mssChildIterator.nextSubSpace();
			break;
			
		case mm_msspool_iterator_next_memory_pool:
			assume0(_memoryPool);
			nextPool = _memoryPool;
			_memoryPool= _memoryPool->getNext(); 
			
			/* Any more children ? */
			if (NULL == _memoryPool) {
				_memorySubSpace = _mssChildIterator.nextSubSpace();
				_state = mm_msspool_iterator_next_subspace;
			}		 
			
			return nextPool;
			break;
		}	
	}	
	
	return NULL;
}
