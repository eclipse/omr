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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(MEMORYPOOLBUMPPOINTER_HPP_)
#define MEMORYPOOLBUMPPOINTER_HPP_

#include "omrcfg.h"

#include "MemoryPool.hpp"
#include "EnvironmentBase.hpp"

class MM_AllocateDescription;
class MM_SweepPoolManager;
class MM_SweepPoolState;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base
 */
class MM_MemoryPoolBumpPointer : public MM_MemoryPool
{
/*
 * Data members
 */
private:
	void *_allocatePointer;	/**< The base address of the unused portion of the receiver and the next pointer which could be returned by an allocation request */
	void *_topPointer;	/**< The top of the memory area managed by the receiver */
		
	uintptr_t _scannableBytes;	/**< estimate of scannable bytes in the pool (only out of sampled objects) */
	uintptr_t _nonScannableBytes; /**< estimate of non-scannable bytes in the pool (only out of sampled objects) */

	MM_SweepPoolState *_sweepPoolState;	/**< GC Sweep Pool State */
	MM_SweepPoolManager *_sweepPoolManager;		/**< pointer to SweepPoolManager class */

protected:
public:
	
/*
 * Function members
 */	
private:
	void *internalAllocate(MM_EnvironmentBase *env, uintptr_t sizeInBytesRequired);
	bool internalAllocateTLH(MM_EnvironmentBase *env, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop);

	void internalRecycleHeapChunk(void *addrBase, void *addrTop);

	/**
	 * Check, can free memory element be connected to memory pool
	 * 
	 * @param address free memory start address
	 * @param size free memory size in bytes
	 * @return true if free memory element would be accepted
	 */
	MMINLINE bool canMemoryBeConnectedToPool(MM_EnvironmentBase *env, void *address, uintptr_t size)
	{
		return (size >= getMinimumFreeEntrySize());
	}
	
protected:

public:
	static MM_MemoryPoolBumpPointer *newInstance(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize);

	virtual void *allocateObject(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription);
	virtual void *allocateTLH(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop);
	virtual void *collectorAllocate(MM_EnvironmentBase *env,  MM_AllocateDescription *allocDescription, bool lockingRequired);
	virtual void *collectorAllocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired);

	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	virtual void reset(Cause cause = any);
	virtual MM_HeapLinkedFreeHeader *rebuildFreeListInRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, MM_HeapLinkedFreeHeader *previousFreeEntry);

	/**
	 * Called by the WriteOnceCompactor after it has internally compacted a region to update the receiver's _allocatePointer so that it
	 * is consistent with the objects in the receiver's managed memory.
	 * @param env[in] A GC thread
	 * @param allocatePointer[in] The first byte available for object allocations (if we were permitting fragmented allocates)
	 */
	void setAllocationPointer(MM_EnvironmentBase *env, void *allocatePointer);
	
	virtual void expandWithRange(MM_EnvironmentBase *env, uintptr_t expandSize, void *lowAddress, void *highAddress, bool canCoalesce);
	virtual void *contractWithRange(MM_EnvironmentBase *env, uintptr_t contractSize, void *lowAddress, void *highAddress);

	bool abandonHeapChunk(void *addrBase, void *addrTop);

	MMINLINE MM_SweepPoolState * getSweepPoolState()
	{
		Assert_MM_true(NULL != _sweepPoolState);
		return _sweepPoolState;
	}

	/**
	 * Get access to Sweep Pool Manager
	 * @return pointer to Sweep Pool Manager associated with this pool or NULL for superpools
	 */
	virtual MM_SweepPoolManager *getSweepPoolManager();
	
	/**
	 * Recalculate the memory pool statistics by actually examining the contents of the pool.
	 */
	void recalculateMemoryPoolStatistics(MM_EnvironmentBase *env);

	/**
	 * Increase the scannable/non-scannable estimate for the receiver by the specified amount
	 * @param scannableBytes the number of bytes to increase for scannable objects
	 * @param non-scannableBytes the number of bytes to increase for scannable objects
	 */
	MMINLINE void incrementScannableBytes(uintptr_t scannableBytes, uintptr_t nonScannableBytes) { _scannableBytes += scannableBytes; _nonScannableBytes += nonScannableBytes; }

	MMINLINE uintptr_t getFreeMemoryAndDarkMatterBytes() {
		uintptr_t actualFreeMemory = getActualFreeMemorySize();
		uintptr_t darkMatter = getDarkMatterBytes();
		uintptr_t allocatableMemory = getAllocatableBytes();
		/* note that actualFreeMemory can be measured as less than allocatableMemory if sweep calculated it since it won't
		 * see the very end of the pool if it is less than minimumFreeEntrySize.
		 */
		Assert_MM_true((0 == actualFreeMemory) || (actualFreeMemory >= allocatableMemory));
		return OMR_MAX(actualFreeMemory + darkMatter, allocatableMemory);
	}

	/**
	 * @return the recorded estimate of scannable in the receiver
	 */
	MMINLINE uintptr_t getScannableBytes() { return _scannableBytes; }
	/**
	 * @return the recorded estimate of non-scannable in the receiver
	 */	
	MMINLINE uintptr_t getNonScannableBytes() { return _nonScannableBytes; }


	/**
	 * Used when a caller wishes to determine the end of the allocated space in the receiver.  Note that the space between this
	 * valud and _topPointer has undefined contents.
	 * @return The point address where the next object will be allocated or _topPointer if the pool cannot satisfy any more allocates
	 */
	MMINLINE void *getAllocationPointer() { return _allocatePointer; }

	/**
	 * Changes the allocation pointer to the given value.
	 * @note The caller must ensure that access to the receiver is serialized as the implementation does not log or atomically update
	 * the pointer.
	 * @note The implementation will fail an assertion if this pointer isn't less than the current _allocatePointer
	 * @param pointer[in] The new value to be used as the receiver's allocation pointer for the next tail-filling opportunity
	 */
	void rewindAllocationPointerTo(void *pointer);

	/**
	 * Returns the number of bytes remaining in the pool which can be used for allocation.  This is a different value
	 * than the total free memory of the pool because it does not count free memory between objects, only the amount
	 * of memory left after the bump pointer.
	 * @return The number of bytes which can still be allocated out of the receiver
	 */
	MMINLINE uintptr_t getAllocatableBytes() { return (uintptr_t)_topPointer - (uintptr_t)getAllocationPointer(); }

	/**
	 * Rounds the receiver's _allocatePointer up to the nearest multiple of alignmentMultiple.
	 * @param alignmentMultiple The multiple to which the _allocatePointer must be aligned (must be a power of 2)
	 */
	void alignAllocationPointer(uintptr_t alignmentMultiple);

	/**
	 * Create a MemoryPoolBumpPointer object.
	 */
	MM_MemoryPoolBumpPointer(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize) :
		MM_MemoryPool(env, minimumFreeEntrySize)
		,_allocatePointer(NULL)
		,_topPointer(NULL)
		,_scannableBytes(0)
		,_nonScannableBytes(0)
		,_sweepPoolState(NULL)
		,_sweepPoolManager(NULL)
	{
		_typeId = __FUNCTION__;
	};
	
	friend class MM_SweepPoolManagerVLHGC;
};

#endif /* MEMORYPOOLBUMPPOINTER_HPP_ */
