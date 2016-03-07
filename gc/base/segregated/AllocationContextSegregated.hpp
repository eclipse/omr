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

#if !defined(ALLOCATIONCONTEXTSEGREGATED_HPP_)
#define ALLOCATIONCONTEXTSEGREGATED_HPP_

#include "omrcfg.h"
#include "omrcomp.h"

#include "AtomicOperations.hpp"
#include "sizeclasses.h"

#include "AllocationContext.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_GlobalAllocationManagerSegregated;
class MM_HeapRegionDescriptorSegregated;
class MM_HeapRegionQueue;
class MM_HeapRegionDescriptorSegregated;
class MM_SegregatedMarkingScheme;
class MM_RegionPoolSegregated;

class MM_AllocationContextSegregated : public MM_AllocationContext
{
/* Data members / Types */
public:
	MM_HeapRegionDescriptorSegregated *_smallRegions[OMR_SIZECLASSES_NUM_SMALL+1];

protected:
	MM_RegionPoolSegregated *_regionPool;

private:
	MM_HeapRegionDescriptorSegregated *_arrayletRegion;
	MM_SegregatedMarkingScheme *_markingScheme;
	omrthread_monitor_t _mutexSmallAllocations; /**< Allocation lock for small size class */
	omrthread_monitor_t _mutexArrayletAllocations; /**< Allocation lock for arraylet size class */
	volatile uint32_t _count; /**< how many threads are attached to me */

	MM_HeapRegionQueue *_perContextSmallFullRegions[OMR_SIZECLASSES_NUM_SMALL+1]; /**< Per-context Regions that have been allocated into during this GC cycle. */
	MM_HeapRegionQueue *_perContextArrayletFullRegions; /**< Per-context Arraylet regions that have been allocated into during this GC cycle. */
	MM_HeapRegionQueue *_perContextLargeFullRegions; /**< Per-context Large object regions that have been allocated into during this GC cycle. */

/* Methods */
public:
	static MM_AllocationContextSegregated *newInstance(MM_EnvironmentBase *env, MM_GlobalAllocationManagerSegregated *gam, MM_RegionPoolSegregated *regionPool);

	virtual void flush(MM_EnvironmentBase *env);
	virtual void flushForShutdown(MM_EnvironmentBase *env) {
		/* ---- no implementation -----
		 * this isn't necessary in Metronome, and calling flush() crashes since the env may not have 
		 * an initialized feedlet for event reporting 
		 */
	}

	/**
	 * Flush the per-context full regions to the region pool.
	 */
	void returnFullRegionsToRegionPool(MM_EnvironmentBase *env);

	/**
	 * Acquire exclusive access to the allocation context by setting its count field to 1.
	 * Since another thread may be trying to do this at the same time, it must be done with
	 * an atomic operation.
	 * @return true if the thread succeeded in acquiring exclusive access
	 */
	MMINLINE void 
	enter(MM_EnvironmentBase *env) 
	{
		MM_AtomicOperations::addU32((uint32_t *)&_count, 1);
	}

	/**
	 * Release exclusive access to the allocation context.
	 * This is safe to do without locking.
	 */
	MMINLINE void 
	exit(MM_EnvironmentBase *env)
	{ 
		MM_AtomicOperations::subtractU32((uint32_t *)&_count, 1);
	}

	uintptr_t *preAllocateSmall(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired);

	virtual uintptr_t *allocateLarge(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired);

	void setMarkingScheme(MM_SegregatedMarkingScheme *markingScheme) { _markingScheme = markingScheme; }

#if defined(OMR_GC_ARRAYLETS)
	uintptr_t *allocateArraylet(MM_EnvironmentBase *env, omrarrayptr_t parent);
#endif /* defined(OMR_GC_ARRAYLETS) */

protected:
	MM_AllocationContextSegregated(MM_EnvironmentBase *env, MM_GlobalAllocationManagerSegregated *gam, MM_RegionPoolSegregated *regionPool)
		: MM_AllocationContext()
		, _regionPool(regionPool)
		, _arrayletRegion(NULL)
		, _markingScheme(NULL)
		, _mutexSmallAllocations(NULL)
		, _mutexArrayletAllocations(NULL)
		, _count(0)
		, _perContextArrayletFullRegions(NULL)
		, _perContextLargeFullRegions(NULL)
	{
		_typeId = __FUNCTION__;
	}

	bool initialize(MM_EnvironmentBase *env);
	void tearDown(MM_EnvironmentBase *env);


	void flushSmall(MM_EnvironmentBase *env, uintptr_t sizeClass);
	void flushArraylet(MM_EnvironmentBase *env);

	virtual bool shouldPreMarkSmallCells(MM_EnvironmentBase *env);

	virtual bool trySweepAndAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass, uintptr_t *sweepCount, uint64_t *sweepStartTime);

	virtual void signalSmallRegionDepleted(MM_EnvironmentBase *env, uintptr_t sizeClass);

	/**
	 * Lock the allocation context for small allocations.
	 * Unless the context is already being shared, this is a no-op
	 */
	MMINLINE void smallAllocationLock()
	{
		omrthread_monitor_enter(_mutexSmallAllocations);
	}

	/**
	 * Unlock the allocation context for small allocations.
	 * Unless the context is already being shared, this is a no-op
	 */
	MMINLINE
	void smallAllocationUnlock()
	{
		omrthread_monitor_exit(_mutexSmallAllocations);
	}

	/**
	 * Lock the allocation context for arraylet allocations.
	 * Unless the context is already being shared, this is a no-op.
	 */
	MMINLINE void arrayletAllocationLock()
	{
		omrthread_monitor_enter(_mutexArrayletAllocations);
	}

	/**
	 * Unlock the allocation context for small allocations.
	 * Unless the context is already being shared, this is a no-op
	 */
	MMINLINE 
	void arrayletAllocationUnlock()
	{
		omrthread_monitor_exit(_mutexArrayletAllocations);
	}
	
	/**
	 * Lock the whole allocation context.
	 */
	MMINLINE
	void lockContext()
	{
		smallAllocationLock();
		arrayletAllocationLock();
	}

	/**
	 * Unlock the whole allocation context.
	 */
	MMINLINE
	void unlockContext()
	{
		arrayletAllocationUnlock();
		smallAllocationUnlock();
	}

	MMINLINE
	void flushHelper(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region, uintptr_t cellSize)
	{
		/* Update the counts to take allocation into acount. */
		region->updateCounts(env, true);
	}

	bool tryAllocateRegionFromSmallSizeClass(MM_EnvironmentBase *env, uintptr_t sizeClass);

	bool tryAllocateFromRegionPool(MM_EnvironmentBase *env, uintptr_t sizeClass);

private:

};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* ALLOCATIONCONTEXTSEGREGATED_HPP_ */
