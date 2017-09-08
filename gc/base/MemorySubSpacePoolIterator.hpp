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
