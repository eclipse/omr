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
