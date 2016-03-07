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


/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(MEMORYSUBSPACEREGIONITERATOR_HPP_)
#define MEMORYSUBSPACEREGIONITERATOR_HPP_

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

