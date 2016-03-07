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

#if !defined(ALLOCATIONSTATS_HPP_)
#define ALLOCATIONSTATS_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "Base.hpp"

class MM_AllocationStats : public MM_Base
{
private:
public:

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	uintptr_t _tlhRefreshCountFresh; /**< Number of refreshes where fresh memory was allocated. */
	uintptr_t _tlhRefreshCountReused; /**< Number of refreshes where TLHs were reused. */
	uintptr_t _tlhAllocatedFresh; /**< The amount of memory allocated fresh out of the heap. */
	uintptr_t _tlhAllocatedReused; /**< The amount of memory allocated form reused TLHs. */
	uintptr_t _tlhRequestedBytes; /**< The amount of memory requested for refreshes. */
	uintptr_t _tlhDiscardedBytes; /**< The amount of memory from discarded TLHs. */
	uintptr_t _tlhMaxAbandonedListSize; /**< The maximum size of the abandoned list. */
#endif /* defined (OMR_GC_THREAD_LOCAL_HEAP) */

#if defined(OMR_GC_ARRAYLETS)
	uintptr_t _arrayletLeafAllocationCount;	/**< Number of arraylet leaf allocations */
	uintptr_t _arrayletLeafAllocationBytes; /**< The amount of memory allocated for arraylet leafs */
#endif

	uintptr_t _allocationCount;
	uintptr_t _allocationBytes;
	uintptr_t _ownableSynchronizerObjectCount;  /**< Number of Ownable Synchronizer Object allocations */
	uintptr_t _discardedBytes;
	uintptr_t _allocationSearchCount;
	uintptr_t _allocationSearchCountMax;

	void clear();
	void clearOwnableSynchronizer() { _ownableSynchronizerObjectCount = 0; }
	void merge(MM_AllocationStats * stats);

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	uintptr_t tlhBytesAllocated() { return _tlhAllocatedFresh - _tlhDiscardedBytes; }
	uintptr_t nontlhBytesAllocated() { return _allocationBytes; }
#endif

	uintptr_t bytesAllocated(){
		uintptr_t totalBytesAllocated = 0;

#if defined(OMR_GC_THREAD_LOCAL_HEAP)	
		totalBytesAllocated += tlhBytesAllocated();
		totalBytesAllocated += nontlhBytesAllocated();
#else
		totalBytesAllocated += _allocationBytes;
#endif
#if defined(OMR_GC_ARRAYLETS)
		totalBytesAllocated += _arrayletLeafAllocationBytes;
#endif
		return totalBytesAllocated;
	}

	MM_AllocationStats() :
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
		_tlhRefreshCountFresh(0),
		_tlhRefreshCountReused(0),
		_tlhAllocatedFresh(0),
		_tlhAllocatedReused(0),
		_tlhRequestedBytes(0),
		_tlhDiscardedBytes(0),
		_tlhMaxAbandonedListSize(0),
#endif /* defined (OMR_GC_THREAD_LOCAL_HEAP) */
#if defined(OMR_GC_ARRAYLETS)
		_arrayletLeafAllocationCount(0),
		_arrayletLeafAllocationBytes(0),
#endif
		_allocationCount(0),
		_allocationBytes(0),
		_ownableSynchronizerObjectCount(0),
		_discardedBytes(0),
		_allocationSearchCount(0),
		_allocationSearchCountMax(0)
	{}
};

#endif /* ALLOCATIONSTATS_HPP_ */
