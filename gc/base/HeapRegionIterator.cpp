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
 ******************************************************************************/

#include "HeapRegionIterator.hpp"

#include "HeapRegionDescriptor.hpp"
#include "HeapRegionManager.hpp"
#include "MemorySubSpace.hpp"
#include "ModronAssertions.h"


GC_HeapRegionIterator::GC_HeapRegionIterator(MM_HeapRegionManager *manager)
	: _space(NULL)
	, _auxRegion(NULL)
	, _tableRegion(NULL)
	, _regionManager(manager)
	, _includedRegionsMask(MM_HeapRegionDescriptor::ALL)
{	
	_auxRegion = _regionManager->getFirstAuxiliaryRegion();
	_tableRegion = _regionManager->getFirstTableRegion();
}

/**
 * Construct a HeapRegionIteratorwith selector for main and aux regions.
 * 
 * @param manager The versions of the regions returned will come from this manager
 */
GC_HeapRegionIterator::GC_HeapRegionIterator(MM_HeapRegionManager *manager, bool includeTableRegions, bool includeAuxRegions)
	: _space(NULL)
	, _auxRegion(NULL)
	, _tableRegion(NULL)
	, _regionManager(manager)
	, _includedRegionsMask(MM_HeapRegionDescriptor::ALL)
{	
	if (includeAuxRegions) {
		_auxRegion = _regionManager->getFirstAuxiliaryRegion();
	}
	if (includeTableRegions) {
		_tableRegion = _regionManager->getFirstTableRegion();
	}
}

/**
 * Construct a HeapRegionIterator for the regions which belong to the specified memory space
 * 
 * @param subspace the memory subspace whose regions should be walked
 */
GC_HeapRegionIterator::GC_HeapRegionIterator(MM_HeapRegionManager *manager, MM_MemorySpace* space)
	: _space(space)
	, _auxRegion(NULL)
	, _tableRegion(NULL)
	, _regionManager(manager)
	, _includedRegionsMask(MM_HeapRegionDescriptor::ALL)
{
	_auxRegion = _regionManager->getFirstAuxiliaryRegion();
	_tableRegion = _regionManager->getFirstTableRegion();
}

GC_HeapRegionIterator::GC_HeapRegionIterator(MM_HeapRegionManager *manager, uint32_t includedRegionsMask)
	: _space(NULL)
	, _auxRegion(NULL)
	, _tableRegion(NULL)
	, _regionManager(manager)
	, _includedRegionsMask(includedRegionsMask)
{
	_auxRegion = _regionManager->getFirstAuxiliaryRegion();
	_tableRegion = _regionManager->getFirstTableRegion();
}

/**
 * Determine if the specified region should be included or skipped.
 * @return true if the region should be included, false otherwise
 */
bool 
GC_HeapRegionIterator::shouldIncludeRegion(MM_HeapRegionDescriptor* region)
{
	if ( 0 == (_includedRegionsMask & region->getRegionProperties()) ) {
		return false;
	} else {
		if (NULL != _space) {
			MM_MemorySubSpace *subspace = region->getSubSpace();
			if (NULL != subspace) {
				return (_space == subspace->getMemorySpace());
			} else {
				return false;
			}
		} else {
			return true;
		}
	}
}

/**
 * @return the next region in the heap, or NULL if there are no more regions
 */
MM_HeapRegionDescriptor *
GC_HeapRegionIterator::nextRegion()
{
	while ((NULL != _auxRegion) || (NULL != _tableRegion)) {
		MM_HeapRegionDescriptor *currentRegion = NULL;
		
		/* we need to return these in-order */
		if ((NULL != _auxRegion) && ((NULL == _tableRegion) || (_auxRegion < _tableRegion))) {
			currentRegion = _auxRegion;
			_auxRegion = _regionManager->getNextAuxiliaryRegion(_auxRegion);
		} else if (NULL != _tableRegion) {
			currentRegion = _tableRegion;
			_tableRegion = _regionManager->getNextTableRegion(_tableRegion);
		} else {
			Assert_MM_unreachable();
		}
		if (shouldIncludeRegion(currentRegion)) {
			return currentRegion;
		}
	}
	return NULL;
}
