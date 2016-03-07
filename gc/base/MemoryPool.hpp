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

#if !defined(MEMORYPOOL_HPP_)
#define MEMORYPOOL_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronopt.h"
#include "ModronAssertions.h"

#include "AtomicOperations.hpp"
#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapStats.hpp"
#include "MemorySubSpace.hpp"

class MM_HeapLinkedFreeHeader;
class MM_AllocateDescription;
class MM_HeapRegionDescriptor;
class MM_LargeObjectAllocateStats;
class MM_SweepPoolManager;

#define HINT_ELEMENT_COUNT 8

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Base_Core
 */
typedef struct J9ModronAllocateHint {
	struct J9ModronAllocateHint* next;
	uintptr_t size;
	MM_HeapLinkedFreeHeader* heapFreeHeader;
	uintptr_t lru;
} J9ModronAllocateHint;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_MemoryPool : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	MM_MemoryPool *_next;
	MM_MemoryPool *_previous;
	MM_MemoryPool *_children;
	
	const char *_poolName;

protected:
	MM_MemoryPool *_parent;

	MM_MemorySubSpace *_memorySubSpace;  /**< Owning memory subspace */

	/* Minimum size of free chunks in this pool */
	uintptr_t _minimumFreeEntrySize; 
	
	uintptr_t _freeMemorySize;
	uintptr_t _freeEntryCount;	
	volatile uintptr_t _largestFreeEntry;  /**< largest free entry found at the end of a global GC cycle (no meaning during mutator execution) */

	uintptr_t _approximateFreeMemorySize;  /**< The approximate number of bytes free that could be made part of the free list */
	
	uintptr_t _allocCount;
	uintptr_t _allocBytes;
	
	/* Number of bytes free at end of last GC */
	uintptr_t _lastFreeBytes;
	
	uintptr_t _allocDiscardedBytes;
	uintptr_t _allocSearchCount;

	MM_GCExtensionsBase *_extensions; /**< GC Extensions for this JVM */
	
	MM_LargeObjectAllocateStats *_largeObjectAllocateStats; /**< Approximate allocation profile for large objects. Struct to keep merged stats from all free lists */

	uintptr_t _darkMatterBytes; /**< estimate of the dark matter in this pool (in bytes) */
	uintptr_t _darkMatterSamples;
	/*
	 * Function members 
	 */
private:
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

public:
	void kill(MM_EnvironmentBase *env);

	virtual void lock(MM_EnvironmentBase *env);
	virtual void unlock(MM_EnvironmentBase *env);
	
	virtual MM_MemoryPool *getMemoryPool(void * addr);
	virtual MM_MemoryPool *getMemoryPool(uintptr_t size);
	virtual MM_MemoryPool *getMemoryPool(MM_EnvironmentBase *env, void *addrBase, void *addrTop, void * &highAddr);
										
	virtual uintptr_t getMemoryPoolCount() { return 1; }
	virtual uintptr_t getActiveMemoryPoolCount() { return 1; }
	
	virtual void setParent(MM_MemoryPool *parent) { _parent = parent; }

	MMINLINE MM_MemorySubSpace *getSubSpace() { return _memorySubSpace; }
	virtual void setSubSpace(MM_MemorySubSpace *subSpace);
	
	virtual void resetHeapStatistics(bool memoryPoolCollected); 
	virtual void mergeHeapStats(MM_HeapStats *heapStats, bool active);

	MM_LargeObjectAllocateStats *getLargeObjectAllocateStats() { return _largeObjectAllocateStats; }
	/**< Reset current (as opposed to average) large object allocate stats */
	virtual void resetLargeObjectAllocateStats() {
		_largeObjectAllocateStats->resetCurrent();
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
		_largeObjectAllocateStats->getTlhAllocSizeClassStats()->resetCounts();
#endif
	}
	/**
	 * initialize frequentAllocation and reset the count of FreeEntrySizeClassStats
	 * @param[in] largeObjectAllocateStats
	 */
	void resetFreeEntryAllocateStats(MM_LargeObjectAllocateStats *largeObjectAllocateStats)
	{
		MM_MemoryPool *topLevelMemoryPool = getParent();
		if (NULL == topLevelMemoryPool) {
			topLevelMemoryPool = this;
		}
		Assert_MM_true(NULL == topLevelMemoryPool->getParent());
		largeObjectAllocateStats->getFreeEntrySizeClassStats()->initializeFrequentAllocation(topLevelMemoryPool->getLargeObjectAllocateStats());
		largeObjectAllocateStats->getFreeEntrySizeClassStats()->resetCounts();
	}
	
	/**< Configure the pool to append collector large allocate stats to the mutator large allocate stats */
	virtual void appendCollectorLargeAllocateStats() {}
	/**< Merge stats (from children pools or split free lists) for free entry stats */
 	virtual void mergeFreeEntryAllocateStats() {}
 	virtual void mergeTlhAllocateStats() {}
 	virtual void mergeLargeObjectAllocateStats() {}
	/**
	 * Average allocation stats.
	 */
 	virtual void averageLargeObjectAllocateStats(MM_EnvironmentBase *env, uintptr_t bytesAllocatedThisRound)
 	{
 		_largeObjectAllocateStats->average(env, bytesAllocatedThisRound);
 	}

	MMINLINE uintptr_t getMinimumFreeEntrySize() { return _minimumFreeEntrySize; }
	MMINLINE uintptr_t setMinimumFreeEntrySize(uintptr_t minimumFreeEntrySize ) { return _minimumFreeEntrySize = minimumFreeEntrySize; }

	virtual uintptr_t getActualFreeMemorySize();
	virtual uintptr_t getApproximateFreeMemorySize();
	
	MMINLINE virtual uintptr_t getCurrentLOASize()
	{
		return 0;
	}
	
	MMINLINE virtual uintptr_t getApproximateFreeLOAMemorySize()
	{
		return 0;
	}
	
	MMINLINE virtual uintptr_t getActualFreeEntryCount() { return _freeEntryCount; }
	
	MMINLINE void setFreeMemorySize(uintptr_t freeMemorySize) { _freeMemorySize = freeMemorySize; }
	MMINLINE void setFreeEntryCount(uintptr_t entryCount) { _freeEntryCount = entryCount; }
	MMINLINE void setApproximateFreeMemorySize(uintptr_t approximateFreeMemorySize) { _approximateFreeMemorySize = approximateFreeMemorySize; }
	
#if defined(DEBUG)	
	MMINLINE virtual bool isMemoryPoolValid(MM_EnvironmentBase *env, bool postCollect)	{ return true; }
#endif	

	void registerMemoryPool(MM_MemoryPool *memoryPool);
	void unregisterMemoryPool(MM_MemoryPool *memoryPool);

	MMINLINE MM_MemoryPool *getNext() { return _next; }
	MMINLINE MM_MemoryPool *getPrevious() { return _previous; }
	MMINLINE void setNext(MM_MemoryPool *memoryPool) { _next = memoryPool; }
	MMINLINE void setPrevious(MM_MemoryPool *memoryPool) { _previous = memoryPool; }

	MMINLINE MM_MemoryPool *getParent() { return _parent; }
	MMINLINE MM_MemoryPool *getChildren() { return _children; }
	
	MMINLINE const char *getPoolName() { return _poolName; }

	MMINLINE virtual void resetLargestFreeEntry() 
	{ 
		_largestFreeEntry = 0; 
	}
	MMINLINE virtual uintptr_t getLargestFreeEntry() 
	{
		return _largestFreeEntry;
	}
		
	MMINLINE void setLargestFreeEntry(uintptr_t largestFreeEntry) { _largestFreeEntry = largestFreeEntry; }
	
	/**
	 * Allocates the contiguous portion of an object.  This means all mixed objects, all indexable objects outside of arraylet
	 * VMs, and arraylet spines (including their leaves, if inline) take this path.
	 */
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription) = 0;
#if defined(OMR_GC_ARRAYLETS)
	/**
	 * Allocate an arraylet leaf.
	 */	
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocDesc);
#endif /* OMR_GC_ARRAYLETS */
	virtual void *collectorAllocate(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, bool lockingRequired);
#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	virtual void *allocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop);
	virtual void *collectorAllocateTLH(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, uintptr_t maximumSizeInBytesRequired, void * &addrBase, void * &addrTop, bool lockingRequired);
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	/* used for reset and postProcess, to notify callee who was the caller */
	enum Cause {
		forSweep,
		forCompact,
		any
	};

	virtual void reset(Cause cause = any);
	virtual MM_HeapLinkedFreeHeader *rebuildFreeListInRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, MM_HeapLinkedFreeHeader *previousFreeEntry);
	virtual void acquireResetLock(MM_EnvironmentBase *env) {};
	virtual void releaseResetLock(MM_EnvironmentBase *env) {};
	
	virtual void postProcess(MM_EnvironmentBase *env, Cause cause) {};

	virtual bool completeFreelistRebuildRequired(MM_EnvironmentBase *env);

	/* TODO: These methods imply knowledge of a "free list" for managing memory
	 * When the code that uses these is cleaned up (e.g. sweep), these methods should be removed
	 */
	virtual bool createFreeEntry(MM_EnvironmentBase *env, void *addrBase, void *addrTop,
								MM_HeapLinkedFreeHeader *previousFreeEntry, MM_HeapLinkedFreeHeader *nextFreeEntry);
	virtual bool createFreeEntry(MM_EnvironmentBase *env, void *addrBase, void *addrTop);
	
	virtual void addFreeEntries(MM_EnvironmentBase *env, MM_HeapLinkedFreeHeader* &freeListHead, MM_HeapLinkedFreeHeader* &freeListTail,
								uintptr_t freeMemoryCount, uintptr_t freeMemorySize);

#if defined(OMR_GC_LARGE_OBJECT_AREA) 
	virtual bool removeFreeEntriesWithinRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress,uintptr_t minimumSize,
														 MM_HeapLinkedFreeHeader* &retListHead, MM_HeapLinkedFreeHeader* &retListTail,
														 uintptr_t &retListMemoryCount, uintptr_t &retListMemorySize);

	virtual void *findAddressAfterFreeSize(MM_EnvironmentBase *env, uintptr_t sizeRequired, uintptr_t minimumSize);
#endif /* OMR_GC_LARGE_OBJECT_AREA */
											
	virtual void expandWithRange(MM_EnvironmentBase *env, uintptr_t expandSize, void *lowAddress, void *highAddress, bool canCoalesce) = 0;
	virtual void *contractWithRange(MM_EnvironmentBase *env, uintptr_t contractSize, void *lowAddress, void *highAddress) = 0;

	/**
	 * Abandon the heap chunk from [addrBase,addrTop), returning it
	 * to a free list or marking it as unused as appropriate.
	 * 
	 * @param[in] addrBase - the first byte of the chunk to be abandoned
	 * @param[in] addrTop - the first byte after the chunk to be abandoned
	 * 
	 * @note addrBase is permitted to be equal to addrTop, in which case the 
	 * chunk is empty and nothing is done. Otherwise, addrBase must be within 
	 * the pool and addrTop must be within or immediately following the pool 
	 * and must be higher than addrBase
	 * 
	 * @return true if the entry was added to a free list, false if it is now unusable
	 */ 
	virtual bool abandonHeapChunk(void *addrBase, void *addrTop) = 0;

#if defined(OMR_GC_THREAD_LOCAL_HEAP)
	/**
	 * Abandon the TLH chunk from [addrBase,addrTop), returning it
	 * to a free list or marking it as unused as appropriate.
	 * 
	 * @param[in] addrBase - the first byte of the chunk to be abandoned
	 * @param[in] addrTop - the first byte after the chunk to be abandoned
	 * 
	 * @note addrBase is permitted to be equal to addrTop, in which case the 
	 * chunk is empty and nothing is done. Otherwise, addrBase must be within 
	 * an active TLH and addrTop must be within or immediately following the 
	 * same TLH and must be higher than addrBase
	 */ 
	void abandonTlhHeapChunk(void *addrBase, void *addrTop);
#endif /* OMR_GC_THREAD_LOCAL_HEAP */

	virtual void *findFreeEntryEndingAtAddr(MM_EnvironmentBase *env, void *addr);
	virtual uintptr_t getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *env, MM_AllocateDescription *allocDescription, void *lowAddr, void *highAddr);
	virtual void *findFreeEntryTopStartingAtAddr(MM_EnvironmentBase *env, void *addr);
	virtual void *getFirstFreeStartingAddr(MM_EnvironmentBase *env);
	virtual void *getNextFreeStartingAddr(MM_EnvironmentBase *env, void *currentFree);
	
	virtual bool initializeSweepPool(MM_EnvironmentBase *env) { return true; }
	virtual MM_SweepPoolManager *getSweepPoolManager();

	virtual void moveHeap(MM_EnvironmentBase *env, void *srcBase, void *srcTop, void *dstBase);

#if defined(DEBUG)
	virtual bool isValidListOrdering();
#endif

	/**
	 * Increase the dark matter estimate for the receiver by the specified amount
	 * @param bytes the number of bytes to increase the recorded estimate
	 */
	MMINLINE void incrementDarkMatterBytes(uintptr_t bytes) { _darkMatterBytes += bytes; }
	/**
	 * @return the recorded estimate of dark matter in the receiver
	 */
	MMINLINE virtual uintptr_t getDarkMatterBytes() { return _darkMatterBytes; }

	MMINLINE void resetDarkMatterBytes() { _darkMatterBytes = 0; }

	MMINLINE void incrementDarkMatterSamples(uintptr_t samples) { _darkMatterSamples += samples; }

	MMINLINE virtual uintptr_t getDarkMatterSamples() { return _darkMatterSamples; }
	/**
	 * Create a MemoryPool object.
	 */
	MM_MemoryPool(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize) :
		MM_BaseVirtual(),
		_next(NULL),
		_previous(NULL),
		_children(NULL),
		_poolName("Unknown"),
		_parent(NULL),
		_memorySubSpace(NULL),
		_minimumFreeEntrySize(minimumFreeEntrySize),
		_freeMemorySize(0),
		_freeEntryCount(0),
		_largestFreeEntry(0),
		_approximateFreeMemorySize(0),
		_allocCount(0),
		_allocBytes(0),
		_lastFreeBytes(0),
		_allocDiscardedBytes(0),
		_allocSearchCount(0),
		_extensions(env->getExtensions()),
		_largeObjectAllocateStats(NULL),
		_darkMatterBytes(0)
		, _darkMatterSamples(0)
	{
		_typeId = __FUNCTION__;
	}

	/**
	 * Create a MemoryPool object.
	 */
	MM_MemoryPool(MM_EnvironmentBase *env, uintptr_t minimumFreeEntrySize, const char *name) :
		MM_BaseVirtual(),
		_next(NULL),
		_previous(NULL),
		_children(NULL),
		_poolName(name),
		_parent(NULL),
		_memorySubSpace(NULL),
		_minimumFreeEntrySize(minimumFreeEntrySize),
		_freeMemorySize(0),
		_freeEntryCount(0),
		_largestFreeEntry(0),
		_approximateFreeMemorySize(0),
		_allocCount(0),
		_allocBytes(0),
		_lastFreeBytes(0),
		_allocDiscardedBytes(0),
		_allocSearchCount(0),
		_extensions(env->getExtensions()),
		_largeObjectAllocateStats(NULL),
		_darkMatterBytes(0)
		, _darkMatterSamples(0)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* MEMORYPOOL_HPP_ */
