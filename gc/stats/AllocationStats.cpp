/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
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
	_tlhAllocatedUsed = 0;
	_tlhAllocatedReused = 0;
	_tlhRequestedBytes = 0;
	_tlhDiscardedBytes = 0;
	_tlhMaxAbandonedListSize = 0;
#endif /* defined (OMR_GC_THREAD_LOCAL_HEAP) */

	_arrayletLeafAllocationCount = 0;
	_arrayletLeafAllocationBytes = 0;

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
	MM_AtomicOperations::add(&_tlhAllocatedUsed, stats->_tlhAllocatedUsed);
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

	MM_AtomicOperations::add(&_arrayletLeafAllocationCount, stats->_arrayletLeafAllocationCount);
	MM_AtomicOperations::add(&_arrayletLeafAllocationBytes, stats->_arrayletLeafAllocationBytes);

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
