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
