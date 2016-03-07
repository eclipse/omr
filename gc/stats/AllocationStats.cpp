/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include "AllocationStats.hpp"
#include "AtomicOperations.hpp"

void
MM_AllocationStats::clear()
{
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	_tlhRefreshCountFresh = 0;
	_tlhRefreshCountReused = 0;
	_tlhAllocatedFresh = 0;
	_tlhAllocatedReused = 0;
	_tlhRequestedBytes = 0;
	_tlhDiscardedBytes = 0;
	_tlhMaxAbandonedListSize = 0;
#endif /* defined (OMR_GC_THREAD_LOCAL_HEAP) */

#if defined(OMR_GC_ARRAYLETS)
	_arrayletLeafAllocationCount = 0;
	_arrayletLeafAllocationBytes = 0;
#endif

	_allocationCount = 0;
	_allocationBytes = 0;
	_ownableSynchronizerObjectCount = 0;
	_discardedBytes = 0;
	_allocationSearchCount = 0;
	_allocationSearchCountMax = 0;
}

void
MM_AllocationStats::merge(MM_AllocationStats *stats)
{
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	MM_AtomicOperations::add(&_tlhRefreshCountFresh, stats->_tlhRefreshCountFresh);
	MM_AtomicOperations::add(&_tlhRefreshCountReused, stats->_tlhRefreshCountReused);
	MM_AtomicOperations::add(&_tlhAllocatedFresh, stats->_tlhAllocatedFresh);
	MM_AtomicOperations::add(&_tlhRequestedBytes, stats->_tlhRequestedBytes);
	MM_AtomicOperations::add(&_tlhDiscardedBytes, stats->_tlhDiscardedBytes);
	MM_AtomicOperations::add(&_tlhAllocatedReused, stats->_tlhAllocatedReused);
	/* looping to set a maximum value in _tlhMaxAbandonedListSize */
	for (
			uintptr_t prevMax = _tlhMaxAbandonedListSize;
			prevMax < stats->_tlhMaxAbandonedListSize;
			prevMax = _tlhMaxAbandonedListSize) {
		MM_AtomicOperations::lockCompareExchange(
			&_tlhMaxAbandonedListSize, prevMax, stats->_tlhMaxAbandonedListSize);
	}
#endif /* defined (OMR_GC_THREAD_LOCAL_HEAP) */

#if defined(OMR_GC_ARRAYLETS)
	MM_AtomicOperations::add(&_arrayletLeafAllocationCount, stats->_arrayletLeafAllocationCount);
	MM_AtomicOperations::add(&_arrayletLeafAllocationBytes, stats->_arrayletLeafAllocationBytes);
#endif

	MM_AtomicOperations::add(&_allocationCount, stats->_allocationCount);
	MM_AtomicOperations::add(&_allocationBytes, stats->_allocationBytes);
	MM_AtomicOperations::add(&_ownableSynchronizerObjectCount, stats->_ownableSynchronizerObjectCount);
	MM_AtomicOperations::add(&_discardedBytes, stats->_discardedBytes);
	MM_AtomicOperations::add(&_allocationSearchCount, stats->_allocationSearchCount);
	/* looping to set a maximum value in _tlhMaxAbandonedListSize */
	for (
			uintptr_t prevMax = _allocationSearchCountMax;
			prevMax < stats->_allocationSearchCountMax;
			prevMax = _allocationSearchCountMax) {
		MM_AtomicOperations::lockCompareExchange(
			&_allocationSearchCountMax, prevMax, stats->_allocationSearchCountMax);
	}
}
