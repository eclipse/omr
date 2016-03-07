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

#if !defined(HEAPREGIONITERATORSTANDARD_HPP)
#define HEAPREGIONITERATORSTANDARD_HPP

#include "omrcfg.h"

#include "HeapRegionDescriptorStandard.hpp"
#include "HeapRegionIterator.hpp"

class GC_HeapRegionIteratorStandard : public GC_HeapRegionIterator {

private:
private:
protected:
	
public:
	
	/**
	 * Construct a HeapRegionIterator for the specified heap.
	 * 
	 * @param manager The versions of the regions returned will come from this manager
	 */
	GC_HeapRegionIteratorStandard(MM_HeapRegionManager *manager) 
		: GC_HeapRegionIterator(manager)
		{ }

	/**
	 * Construct a HeapRegionIterator for the regions which belong to the specified memory space.
	 * 
	 * @param manager The versions of the regions returned will come from this manager
	 * @param space the memory space whose regions should be walked
	 */
	GC_HeapRegionIteratorStandard(MM_HeapRegionManager *manager, MM_MemorySpace* space)
		: GC_HeapRegionIterator(manager, space)
		{ }
	
	/**
	 * @return the next region in the heap, or NULL if there are no more regions
	 */
	MM_HeapRegionDescriptorStandard *nextRegion() 
	{
		return (MM_HeapRegionDescriptorStandard*)GC_HeapRegionIterator::nextRegion();
	}

};


#endif /* HEAPREGIONITERATORSTANDARD_HPP */
