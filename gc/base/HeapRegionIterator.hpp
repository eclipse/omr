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

#if !defined(HEAPREGIONITERATOR_HPP_)
#define HEAPREGIONITERATOR_HPP_

#include "omrcomp.h"

class MM_HeapRegionDescriptor;
class MM_HeapRegionManager;
class MM_MemorySpace;

/**
 * Iterate over all of the regions in the heap in address order.
 * @ingroup GC_Base_Core
 */
class GC_HeapRegionIterator
{
private:
	MM_MemorySpace *_space; /**< The MemorySpace to which all regions returned by the iterator must belong (can be NULL if this check is not needed) */
	MM_HeapRegionDescriptor *_auxRegion; /**< The auxiliary region we will process next */
	MM_HeapRegionDescriptor *_tableRegion; /**< The table region we will process next */
	MM_HeapRegionManager *_regionManager; /**< The HeapRegionManager from which this iterator requests its regions */

	uint32_t _includedRegionsMask; /**< A bitmap of MM_HeapRegionDescriptor::RegionProperties of the regions to include during iteration */

private:
	bool shouldIncludeRegion(MM_HeapRegionDescriptor* region);
	
protected:
	
public:
	
	/**
	 * Construct a HeapRegionIterator for the specified heap.
	 * 
	 * @param manager The versions of the regions returned will come from this manager
	 */
	GC_HeapRegionIterator(MM_HeapRegionManager *manager);

	/**
	 * Construct a HeapRegionIteratorwith selector for main and aux regions.
	 * 
	 * @param manager The versions of the regions returned will come from this manager
	 * @param includeTableRegions If true, include table region (contiguous heap) for in the iteration
	 * @param includeAuxRegions If true, include auxilary regions for in the iteration
	 */
	GC_HeapRegionIterator(MM_HeapRegionManager *manager, bool includeTableRegions, bool includeAuxRegions);

	/**
	 * Construct a HeapRegionIterator for the regions which belong to the specified memory space.
	 * 
	 * @param manager The versions of the regions returned will come from this manager
	 * @param space the memory space whose regions should be walked
	 */
	GC_HeapRegionIterator(MM_HeapRegionManager *manager, MM_MemorySpace* space);
	
	/**
	 * @param manager The versions of the regions returned will come from this manager
	 * @param includedRegionsMask A bitmap of the HeapRegionDescriptor properties of the regions that should be included in the iterator
	 */
	GC_HeapRegionIterator(MM_HeapRegionManager *manager, uint32_t includedRegionsMask);

	/**
	 * @return the next region in the heap, or NULL if there are no more regions
	 */
	MM_HeapRegionDescriptor *nextRegion();
};

#endif /* HEAPREGIONITERATOR_HPP_ */

