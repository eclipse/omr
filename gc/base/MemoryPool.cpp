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

#include "MemoryPool.hpp"

#include "AtomicOperations.hpp"
#include "Debug.hpp"
#include "GCExtensionsBase.hpp"

#include "ModronAssertions.h"

bool
MM_MemoryPool::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Free any associated structures or resources in the receiver.
 */
void
MM_MemoryPool::tearDown(MM_EnvironmentBase *env)
{
	/* Remove the receiver from its owner's list */
	if(_parent) {
		_parent->unregisterMemoryPool(this);
	}
}

/**
 * Get the sum of all free memory currently available for allocation in the receiver.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider deferred work that may be done to increase current free memory stores.
 * @see getApproximateFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_MemoryPool::getActualFreeMemorySize()
{
	return _freeMemorySize;
}

/**
 * Get the approximate sum of all free memory available for allocation in the receiver.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * @see getActualFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_MemoryPool::getApproximateFreeMemorySize()
{
	return getActualFreeMemorySize() + _approximateFreeMemorySize;
}

/**
 * Return the memory pool associated with a given storage location.
 * @param Address of storage location 
 * @return MM_MemoryPool
 */
MM_MemoryPool *
MM_MemoryPool::getMemoryPool(void *addr)
{
	return this;	
}	

/**
 * Return the memory pool associated with a given allocation size
 * @param Size of allocation 
 * @return MM_MemoryPool
 */
MM_MemoryPool *
MM_MemoryPool::getMemoryPool(uintptr_t size)
{
	return this;	
}	

/**
 * Return the memory pool associated with a specified range of storage locations. 
 * 
 * @param addrBase Low address in specified range  
 * @param addrTop High address in  specified range 
 * @param highAddr If range spans the end of memory pool set to the first byte which doesn't belong in the returned pool.
 * @return MM_MemoryPool for storage location addrBase
 */
MM_MemoryPool *
MM_MemoryPool::getMemoryPool(MM_EnvironmentBase *env, void *addrBase, void *addrTop, void * &highAddr)
{
	highAddr = NULL;
	return this;
}								

void
MM_MemoryPool::registerMemoryPool(MM_MemoryPool *memoryPool)
{
	memoryPool->setParent(this);
	
	if (_children) {
		_children->setPrevious(memoryPool);
	}
	memoryPool->setNext(_children);
	memoryPool->setPrevious(NULL);
	_children = memoryPool;
}

void
MM_MemoryPool::unregisterMemoryPool(MM_MemoryPool *memoryPool)
{
	MM_MemoryPool *previous, *next;
	previous = memoryPool->getPrevious();
	next = memoryPool->getNext();

	if(previous) {
		previous->setNext(next);
	} else {
		_children = next;
	}

	if(next) {
		next->setPrevious(previous);
	}
}
#if defined(OMR_GC_ARRAYLETS)
	/**
	 * Allocate an arraylet leaf.
	 */
	void *
	MM_MemoryPool::allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDesc)
	{
		Assert_MM_unreachable();
		return NULL;
	}
#endif /* OMR_GC_ARRAYLETS */

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	void *
	MM_MemoryPool::allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop)
	{
		Assert_MM_unreachable();
		return NULL;
	}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

/**
 * Set the owning subspace.
 * This call cascades the owning subspace down through the memorypool hierarchy.
 */
void
MM_MemoryPool::setSubSpace(MM_MemorySubSpace *memorySubSpace)
{
	if(NULL != _children) {
		_children->setSubSpace(memorySubSpace);
	}
	
	if(NULL != _next) {
		_next->setSubSpace(memorySubSpace);
	}
	
	_memorySubSpace = memorySubSpace;
}

void 
MM_MemoryPool::reset(Cause cause)
{
	_freeMemorySize = 0;
	_freeEntryCount = 0;
	_approximateFreeMemorySize = 0;
	_darkMatterBytes = 0;
	_darkMatterSamples = 0;
}	

MM_HeapLinkedFreeHeader *
MM_MemoryPool::rebuildFreeListInRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, MM_HeapLinkedFreeHeader *previousFreeEntry)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	omrtty_printf("Class Type: %s\n",_typeId);
	Assert_MM_unreachable(); // should never get here
	return NULL;
}


#if defined(OMR_GC_THREAD_LOCAL_HEAP)
void
MM_MemoryPool::abandonTlhHeapChunk(void *addrBase, void *addrTop)
{
	Assert_MM_true(addrTop >= addrBase);
	
	if (addrTop > addrBase) {
		abandonHeapChunk(addrBase, addrTop);
	}
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

void
MM_MemoryPool::resetHeapStatistics(bool memoryPoolCollected) 
{
	/* This assumes all heap resizing has  been completed before this call. */
	if (memoryPoolCollected) {
		_lastFreeBytes= getApproximateFreeMemorySize();
	}	

	_allocCount = 0;
	_allocBytes = 0;
	_allocDiscardedBytes = 0;
	_allocSearchCount = 0;
}

/**
 * Record heap statistics .
 */
void
MM_MemoryPool::mergeHeapStats(MM_HeapStats *heapStats, bool active)
{
	heapStats->_allocCount += _allocCount;
	heapStats->_allocBytes += _allocBytes;
	heapStats->_lastFreeBytes += _lastFreeBytes;	
	
	heapStats->_allocDiscardedBytes += _allocDiscardedBytes;
	heapStats->_allocSearchCount += _allocSearchCount;

	if (active) {
		heapStats->_activeFreeEntryCount += getActualFreeEntryCount();
	} else {
		heapStats->_inactiveFreeEntryCount += getActualFreeEntryCount();
	}
}

/**
 * Find the free list entry whos end address matches the parameter.
 * 
 * @param addr Address to match against the high end of a free entry.
 * @return The leading address of the free entry whos top matches addr.
 */
void *
MM_MemoryPool::findFreeEntryEndingAtAddr(MM_EnvironmentBase *env, void *addr)
{
	Assert_MM_unreachable(); // should never get here
	return NULL;
}

/**
 * Find the contraction size available in the given range that MUST end at the high address.
 * This routines shoud find any available free entries that can be removed that are within the given range,
 * ending at a particular range, all while satisfying the described allocation.  The allocDescription is passed
 * so that if the allocation type or size class has an effect on what can/cannot be contracted, it can be factored
 * into the returned value.
 * 
 * @param lowAddr Address to match against the high end of a free entry.
 * @param highAddr Address to match against the high end of a free entry.
 * @return Size available to contract 
 */
uintptr_t 
MM_MemoryPool::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, void *lowAddr, void *highAddr)
{
	/* Unimplemented - no contraction available */
	return 0;
}

/**
 * Find the top of the free list entry whos start address matches the parameter.
 * 
 * @param addr Address to match against the low end of a free entry.
 * @return The trailing address of the free entry whos top matches addr.
 */
void *
MM_MemoryPool::findFreeEntryTopStartingAtAddr(MM_EnvironmentBase *env, void *addr)
{
	Assert_MM_unreachable(); // should never get here
	return NULL;
}

/**
 * Find the address of the first entry on free list entry.
 * 
 * @return The address of head of free chain 
 */
void *
MM_MemoryPool::getFirstFreeStartingAddr(MM_EnvironmentBase *env)
{
	Assert_MM_unreachable(); // should never get here
	return NULL;
}

/**
 * Find the address of the next entry on free list after the specified entry.
 * 
 * @return The address of next free entry or NULL 
 */
void *
MM_MemoryPool::getNextFreeStartingAddr(MM_EnvironmentBase *env, void *currentFree)
{
	Assert_MM_unreachable(); // should never get here
	return NULL;
}

/**
 * Move a chunk of heap from one location to another within the receivers owned regions.
 * This involves fixing up any free list information that may change as a result of an address change.
 * 
 * @param srcBase Start address to move.
 * @param srcTop End address to move.
 * @param dstBase Start of destination address to move into.
 */
void
MM_MemoryPool::moveHeap(MM_EnvironmentBase *env, void *srcBase, void *srcTop, void *dstBase)
{
	Assert_MM_unreachable(); // should never get here
}

/**
 * Lock any free list information from use.
 */
void
MM_MemoryPool::lock(MM_EnvironmentBase *env)
{
}

/**
 * Unlock any free list information for use.
 */
void
MM_MemoryPool::unlock(MM_EnvironmentBase *env)
{
}

/**
 * @todo Provide function documentation
 */
void *
MM_MemoryPool::collectorAllocate(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool lockingRequired)
{
	Assert_MM_unreachable();  // should not be called?
	return NULL;
}

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
/**
 * @todo Provide function documentation
 */
void *
MM_MemoryPool::collectorAllocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired)
{
	Assert_MM_unreachable();  // should not be called?
	return NULL;
}
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

/**
 * Is a complete rebuild of the freelist required? 
 * @return TRUE if a complete sweep is required; FALSE otherwise
 */
bool 
MM_MemoryPool::completeFreelistRebuildRequired(MM_EnvironmentBase *env)
{
	return false;
}

/**
 * Convert a chunk of heap into a free list entry, and potentially link it into a list.
 * @param addrBase the base address (inclusive) of the heap chunk
 * @param addrTop the top address (exclusive) of the heap chunk
 * @previousFreeEntry the previous entry in the list (may be NULL)
 * @nextFreeEntry next entry in the list (may be NULL)
 * 
 * @note If both <code>previousFreeEntry</code> and <code>nextFreeEntry</code> are NULL,
 * this call is equivalent to @ref abandonHeapChunk().
 * @note This call does not implicitly update any stats (e.g. freeMemorySize)
 * 
 * @todo This method implies knowledge of a "free list" for managing memory
 * When the code that uses these is cleaned up (e.g. sweep), this method should be removed
 * 
 * @return true if the heap chunk is large enough to be on the free list, false otherwise
 */
bool 
MM_MemoryPool::createFreeEntry(
	MM_EnvironmentBase *env,
	void *addrBase,
	void *addrTop,
	MM_HeapLinkedFreeHeader *previousFreeEntry,
	MM_HeapLinkedFreeHeader *nextFreeEntry)
{
	Assert_MM_unreachable(); /* implementation required */	
	return false;
}

/**
 * Add contents of an address ordered list of all free entries to this pool
 *
 * @param addrbase Head of list of free entries to be added
 * @param addrTop Tail of list of free entries to be added
 */
void
MM_MemoryPool::addFreeEntries(MM_EnvironmentBase *env, MM_HeapLinkedFreeHeader* &freeListHead, MM_HeapLinkedFreeHeader* &freeListTail,
							uintptr_t freeMemoryCount, uintptr_t freeMemorySize)
{
	Assert_MM_unreachable(); /* implementation required */
}

#if defined(OMR_GC_LARGE_OBJECT_AREA)
/**
 * Remove  all free entries in a  specified range from pool and return them to caller 
 *  as an address ordered list.
 * 
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 * @param minimumSize Minimum size of entries to be added to returned list 
 * @param retListHead Head of list of free entries being returned 
 * @param retListTail Tail of list of free entries being returned 
 * @param retListMemoryCount Number of free chunks on returned list 
 * @param retListmemorySize Total size of all free chunks on returned list 
 *  
 * @return TRUE if at least one chunk in specified range found; FALSE otherwise
 */
bool
MM_MemoryPool::removeFreeEntriesWithinRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress, uintptr_t minimumSize,
										 MM_HeapLinkedFreeHeader* &retListHead, MM_HeapLinkedFreeHeader* &retListTail,
										 uintptr_t &retListMemoryCount, uintptr_t &retListMemorySize)
{
	Assert_MM_unreachable(); /* implementation required */	
	return false;
}

/**
 * Determine the address in this memoryPool where there is at least sizeRequired free bytes in free entries.
 * The free entries must be of size minimumSize to be counted.
 * 
 * @param sizeRequired Amount of free memory to be returned
 * @param minimumSize Minimum size of free entry we are intrested in
 *  
 * @return address of memory location which has sizeRequired free bytes available in this memoryPool.
 * @return NULL if this memory pool can not satisfy sizeRequired bytes.
 */
void *
MM_MemoryPool::findAddressAfterFreeSize(MM_EnvironmentBase *env, uintptr_t sizeRequired, uintptr_t minimumSize)
{
	Assert_MM_unreachable(); /* implementation required */	
	return NULL;
}
#endif /* OMR_GC_LARGE_OBJECT_AREA */

/**
 * Convert a chunk of heap into a free list entry, and potentially link it into a list.
 * Used for flushing the tail (last free entry).
 * @param addrBase the base address (inclusive) of the heap chunk
 * @param addrTop the top address (exclusive) of the heap chunk
 * 
 * @todo This method implies knowledge of a "free list" for managing memory
 * When the code that uses these is cleaned up (e.g. sweep), this method should be removed
 * 
 * @return true if the heap chunk is large enough to be on the free list, false otherwise
 */
bool 
MM_MemoryPool::createFreeEntry(MM_EnvironmentBase *env, void *addrBase, void *addrTop)
{
	Assert_MM_unreachable(); /* implementation required */	
	return false;
}

#if defined(DEBUG)
/**
 * Ensure the free list is in a valid order.
 * @todo This method implies knowledge of a "free list" for managing memory
 * When the code that uses these is cleaned up (e.g. sweep), this method should be removed
 */
bool
MM_MemoryPool::isValidListOrdering()
{
	Assert_MM_unreachable(); /* implementation required */	
	return true;
}
#endif /* DEBUG */

/**
 * Get access to Sweep Pool Manager
 * @return pointer to Sweep Pool Manager associated with this pool
 * or NULL for superpools
 */
MM_SweepPoolManager *
MM_MemoryPool::getSweepPoolManager()
{
	/* Should have been implemented */
	Assert_MM_unreachable();
	return NULL;
}

/**
 * Teardown and free the receiver by invoking the virtual #tearDown() function
 */
void
MM_MemoryPool::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}
