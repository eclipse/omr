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
	uintptr_t _tlhRefreshCountFresh; 	/**< Number of refreshes where fresh memory was allocated. */
	uintptr_t _tlhRefreshCountReused; 	/**< Number of refreshes where TLHs were reused. */
	uintptr_t _tlhAllocatedFresh; 		/**< The amount of memory allocated fresh out of the heap. */
	uintptr_t _tlhAllocatedUsed; 		/**< The amount of used memory of already flushed TLHs */
	uintptr_t _tlhAllocatedReused; 		/**< The amount of memory allocated form reused TLHs. */
	uintptr_t _tlhRequestedBytes; 		/**< The amount of memory requested for refreshes. */
	uintptr_t _tlhDiscardedBytes; 		/**< The amount of memory from discarded TLHs. */
	uintptr_t _tlhMaxAbandonedListSize; /**< The maximum size of the abandoned list. */
#endif /* defined (OMR_GC_THREAD_LOCAL_HEAP) */

	uintptr_t _arrayletLeafAllocationCount;	/**< Number of arraylet leaf allocations */
	uintptr_t _arrayletLeafAllocationBytes; /**< The amount of memory allocated for arraylet leafs */

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
	uintptr_t tlhBytesAllocatedUsed() { return _tlhAllocatedUsed; }
	uintptr_t nontlhBytesAllocated() { return _allocationBytes; }
#endif

	/* return bytesAllocated includes new refreshed TLH, if includeJustRefreshedTLH == true(default)
	 * return bytesAllocated (but does not include new refreshed TLH), if includeJustRefreshedTLH == false.
	 */
	uintptr_t bytesAllocated(bool includeJustRefreshedTLH = true) {
		uintptr_t totalBytesAllocated = 0;

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
		if (includeJustRefreshedTLH) {
			totalBytesAllocated += tlhBytesAllocated();
		} else {
			totalBytesAllocated += tlhBytesAllocatedUsed();
		}
		totalBytesAllocated += nontlhBytesAllocated();
#else
		totalBytesAllocated += _allocationBytes;
#endif
		totalBytesAllocated += _arrayletLeafAllocationBytes;
		return totalBytesAllocated;
	}

	MM_AllocationStats() :
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
		_tlhRefreshCountFresh(0),
		_tlhRefreshCountReused(0),
		_tlhAllocatedFresh(0),
		_tlhAllocatedUsed(0),
		_tlhAllocatedReused(0),
		_tlhRequestedBytes(0),
		_tlhDiscardedBytes(0),
		_tlhMaxAbandonedListSize(0),
#endif /* defined (OMR_GC_THREAD_LOCAL_HEAP) */
		_arrayletLeafAllocationCount(0),
		_arrayletLeafAllocationBytes(0),
		_allocationCount(0),
		_allocationBytes(0),
		_ownableSynchronizerObjectCount(0),
		_discardedBytes(0),
		_allocationSearchCount(0),
		_allocationSearchCountMax(0)
	{}
};

#endif /* ALLOCATIONSTATS_HPP_ */
