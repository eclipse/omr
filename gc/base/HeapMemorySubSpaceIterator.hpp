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
