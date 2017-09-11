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
 * @ingroup GC_Modron_Base
 */

#if !defined(HEAPMEMORYPOOLITERATOR_HPP_)
#define HEAPMEMORYPOOLITERATOR_HPP_

#include "omrcfg.h"
#include "omr.h"

#include "HeapMemorySubSpaceIterator.hpp"

class MM_Heap;
class MM_MemoryPool;
class MM_MemorySubSpace;
class MM_EnvironmentBase;

/**
 * Iterate through all visible memory pools in the system.
 * Provides a preordered walk of all memory subspaces to find all memory pools for a MM_Heap.
 * 
 * @ingroup GC_Modron_Base
 */	
class MM_HeapMemoryPoolIterator
{
private:
	MM_HeapMemorySubSpaceIterator _mssIterator;  /**< Memory subspace iterator for the heap */
	
	MM_MemorySubSpace *_memorySubSpace;
	MM_MemoryPool *_memoryPool;
	
	uintptr_t _state;
	
	void reset();

	/**
	 * Iterator states to maintain preorder traversal.
	 */
	enum {
		mm_heapmp_iterator_next_subspace,  	 /**< Get to next subspace	   */
		mm_heapmp_iterator_next_memory_pool  /**< Get to next pool		   */
	};

protected:
public:
	MM_MemoryPool *nextPool();
	MM_MemoryPool *nextPoolInSubSpace();

	MM_HeapMemoryPoolIterator(MM_EnvironmentBase *env, MM_Heap *heap) :
		_mssIterator(heap),
		_memorySubSpace(NULL),
		_memoryPool(NULL),
		_state(0)
	{
		reset();
	}

	MM_HeapMemoryPoolIterator(MM_EnvironmentBase *env, MM_Heap *heap, MM_MemorySubSpace *memorySubSpace) :
		_mssIterator(heap),
		_memorySubSpace(memorySubSpace),
		_memoryPool(NULL),
		_state(mm_heapmp_iterator_next_subspace)
	{
	}
		
};

#endif /* HEAPMEMORYPOOLITERATOR_HPP_ */
