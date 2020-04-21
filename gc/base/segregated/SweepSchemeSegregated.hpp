/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(SWEEPSCHEMESEGREGATED_HPP_)
#define SWEEPSCHEMESEGREGATED_HPP_

#include "HeapLinkedFreeHeader.hpp"
#include "MemoryPoolAggregatedCellList.hpp"

#include "Base.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

#define SWEEP_CELL_COST 1
#define SWEEP_REGION_COST 200
#define SWEEP_BUDGET 2000
#define MAX_REGION_COALESCE 500

class MM_EnvironmentBase;
class MM_EnvironmentRealtime;
class MM_HeapRegionDescriptorSegregated;
class MM_MarkMap;
class MM_MemoryPoolSegregated;

class MM_SweepSchemeSegregated : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
public:
protected:
	MM_MemoryPoolSegregated *_memoryPool;
	MM_MarkMap *_markMap;
	MM_GCExtensionsBase *_extensions;
private:
	bool _isFixHeapForWalk;
	bool _clearMarkMapAfterSweep; /**< If a region should be unmarked after it is swept */

	/*
	 * Function members
	 */
public:
	static MM_SweepSchemeSegregated *newInstance(MM_EnvironmentBase *env, MM_MarkMap *markMap);
	void kill(MM_EnvironmentBase *env);
	
	MM_MarkMap *getMarkMap(MM_EnvironmentBase * env);

	void sweep(MM_EnvironmentBase *env, MM_MemoryPoolSegregated *memoryPool, bool isFixHeapForWalk);
	virtual void sweepRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region);

	bool isClearMarkMapAfterSweep() { return _clearMarkMapAfterSweep; }
	void setClearMarkMapAfterSweep(bool clearMarkMapAfterSweep) { _clearMarkMapAfterSweep = clearMarkMapAfterSweep; }
protected:
	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);

	virtual void preSweep(MM_EnvironmentBase *env);
	virtual void postSweep(MM_EnvironmentBase *env);

	virtual void incrementalSweepArraylet(MM_EnvironmentBase *env);

	/**
	 * Create a MM_SweepSchemeSegregated object
	 */
	MM_SweepSchemeSegregated(MM_EnvironmentBase *env, MM_MarkMap *markMap) :
		MM_BaseVirtual()
		,_memoryPool(NULL)
		,_markMap(markMap)
		,_extensions(env->getExtensions())
		,_isFixHeapForWalk(false)
		,_clearMarkMapAfterSweep(true)
	{
		_typeId = __FUNCTION__;
	};
	
private:
	void unmarkRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region);
	void sweepSmallRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region);
	void sweepArrayletRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region);
	void sweepLargeRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region);
	void addBytesFreedAfterSweep(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region);
	void incrementalSweepSmall(MM_EnvironmentBase *env);
	void incrementalSweepLarge(MM_EnvironmentBase *env);
	void incrementalCoalesceFreeRegions(MM_EnvironmentBase *env);

	MMINLINE bool addFreeChunk(MM_MemoryPoolAggregatedCellList *memoryPoolACL, uintptr_t *freeChunk, uintptr_t freeChunkSize, uintptr_t minimumFreeEntrySize, uintptr_t freeChunkCellCount)
	{
		bool result = false;
		bool const compressed = _extensions->compressObjectReferences();
		if (freeChunkSize >= minimumFreeEntrySize) {
			/* add to memory pool */
			memoryPoolACL->addFreeChunk(freeChunk, freeChunkSize, freeChunkCellCount);
			result = true;
		} else if (_isFixHeapForWalk) {
			/* fill with holes so debugging tools can walk the heap */
			MM_HeapLinkedFreeHeader::fillWithHoles(freeChunk, freeChunkSize, compressed);
		} else {
			/* dark matter */
		}
		return result;
	}

	/**
	 * Called to allow subclasses to yield to mutators if required (eg, realtime constraints)
	 */
	virtual void yieldFromSweep(MM_EnvironmentBase *env, uintptr_t yieldSlackTime = 0);

	/**
	 * Called from incrementalCoalesceFreeRegions() to track range of regions processed since last yield
	 * @return A nonzero value to indicate yield slack time when yield is required
	 */
	virtual uintptr_t resetCoalesceFreeRegionCount(MM_EnvironmentBase *env);

	/**
	 * @return True to indicate that yield is required
	 */
	virtual bool updateCoalesceFreeRegionCount(uintptr_t range);

	/**
	 * Called from incrementalSweepSmall() to track range of regions processed since last yield
	 * @return A nonzero value to indicate yield slack time when yield is required
	 */
	virtual uintptr_t resetSweepSmallRegionCount(MM_EnvironmentBase *env, uintptr_t yieldSmallRegionCount);

	/**
	 * @return True to indicate that yield is required
	 */
	virtual bool updateSweepSmallRegionCount();

	/**
	 * Calculate the maximum number of regions of a single small region size class to sweep before proceeding to the next size class
	 * while sweeping small regions.
	 */
	uintptr_t
	calcSweepSmallRegionsPerIteration(uintptr_t numCellsPerSizeClass)
	{
		uintptr_t regionCost = (SWEEP_CELL_COST * numCellsPerSizeClass) + SWEEP_REGION_COST;
		return 8 * OMR_MAX(1, SWEEP_BUDGET / regionCost);
	}
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* SWEEPSCHEMESEGREGATED_HPP_ */
