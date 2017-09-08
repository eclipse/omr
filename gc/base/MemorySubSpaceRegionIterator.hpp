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
 * @ingroup GC_Base_Core
 */

#if !defined(MEMORYSUBSPACEREGIONITERATOR_HPP_)
#define MEMORYSUBSPACEREGIONITERATOR_HPP_

/*
 * @ddr_namespace: default
 */

#include "omrcfg.h"
#include "modronbase.h"

class MM_HeapRegionDescriptor;
class MM_MemorySubSpace;

/**
 * Iterate over all of the regions in the subspace, recursively from all its leaf subspaces.
 * @ingroup GC_Base_Core
 */
class GC_MemorySubSpaceRegionIterator
{
public:
protected:
private:
#define MAX_STACK_SLOTS 4
	MM_MemorySubSpace *_subSpaceStack[MAX_STACK_SLOTS]; /**< current stack of subspaces used for recursive search of leaf subspaces. root subspace is at slot 0. */
	uintptr_t _leafStackSlot; /**< current most nested subspace in the stack */
	MM_HeapRegionDescriptor *_region; /**< The region we will process next */
public:

	/**
	 * Construct a MemorySubSpaceRegionIterator for the regions which belong to the specified subspace
	 * 
	 * @param subspace the memory subspace whose regions should be walked
	 */
	GC_MemorySubSpaceRegionIterator(MM_MemorySubSpace* subspace);
	
	/**
	 * @return the next region in the heap, or NULL if there are no more regions
	 */
	MM_HeapRegionDescriptor *nextRegion();
protected:
private:
	void initializeStack(uintptr_t fromStackSlot);
};

#endif /* MEMORYSUBSPACEREGIONITERATOR_HPP_ */

