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

#if !defined(MEMORYSUBSPACEITERATORSTANDARD_HPP_)
#define MEMORYSUBSPACEITERATORSTANDARD_HPP_

#include "omrcfg.h"

#include "HeapRegionDescriptorStandard.hpp"
#include "MemorySubSpaceRegionIterator.hpp"

class GC_MemorySubSpaceRegionIteratorStandard : public GC_MemorySubSpaceRegionIterator {

public:
protected:
private:
	
public:

	/**
	 * Construct a MemorySubSpaceRegionIterator for the regions which belong to the specified subspace.
	 * 
	 * @param subspace the memory subspace whose regions should be walked
	 */
	GC_MemorySubSpaceRegionIteratorStandard(MM_MemorySubSpace* subspace)
		: GC_MemorySubSpaceRegionIterator(subspace)
		{ }
	
	/**
	 * @return the next region in the heap, or NULL if there are no more regions
	 */
	MM_HeapRegionDescriptorStandard *nextRegion() 
	{
		return (MM_HeapRegionDescriptorStandard*)GC_MemorySubSpaceRegionIterator::nextRegion();
	}
protected:
private:
};


#endif /* MEMORYSUBSPACEITERATORSTANDARD_HPP_ */
