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
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(COPYSCANCACHELIST_HPP_)
#define COPYSCANCACHELIST_HPP_

#include "modronopt.h"	

#include "string.h"

#include "BaseVirtual.hpp"
#include "EnvironmentStandard.hpp" 
#include "LightweightNonReentrantLock.hpp"
#include "omr/gc/modronAssertions.h"

class MM_Collector;
class MM_CopyScanCacheStandard;
class MM_CopyScanCacheChunk;
class MM_MemorySubSpace;
 
/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_CopyScanCacheList : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	bool _allocationInHeap;	/**< set if scan cache headers allocated in Heap */

	struct CopyScanCacheSublist {
		MM_CopyScanCacheStandard * volatile _cacheHead;  /**< Head of the list */
		MM_LightweightNonReentrantLock _cacheLock;  /**< Lock for getting/putting caches */
		uintptr_t _entryCount;	/**< number of entries in sublist */
	};
	
	struct CopyScanCacheSublist *_sublists;	/**< An array of CopyScanCacheSublist structures which is _sublistCount elements long */
	uintptr_t _sublistCount; /**< the number of lists (split for parallelism). Must be at least 1 */
	
	MM_CopyScanCacheChunk *_chunkHead; 
	uintptr_t _incrementEntryCount;
	uintptr_t _totalAllocatedEntryCount;
	
	volatile uintptr_t *_cachedEntryCount; /* pointer to cachedEntryCount, that is shared among all lists (of all nodes) */

protected:
public:

	/*
	 * Function members
	 */
private:
	bool appendCacheEntries(MM_EnvironmentBase *env, uintptr_t cacheEntryCount);

	/**
	 * Hash the specified environment to determine what sublist index
	 * it should use
	 * 
	 * @param env the current environment
	 * 
	 * @return an index into the _sublists array
	 */
	uintptr_t getSublistIndex(MM_EnvironmentBase *env)
	{
		return env->getEnvironmentId() % _sublistCount;
	}
	
	/**
	 * Increment the sublist counter by the specified amount
	 * Also increment the shared counter if current sublist counter value is zero
	 * Must be called inside of a locked region
	 * 
	 * @param sublistIndex sublist number counter should be incremented
	 * @param value the positive value to increment
	 */
	void incrementCount(CopyScanCacheSublist *sublist, uintptr_t value);
	
	/**
	 * Decrement the sublist counter by the specified amount
	 * Also decrement the shared counter if sublist counter value after operation is zero
	 * Must be called inside of a locked region
	 *
	 * @param sublistIndex sublist number counter should be incremented
	 * @param value the positive value to increment
	 */
	void decrementCount(CopyScanCacheSublist *sublist, uintptr_t value);

protected:
public:
	bool initialize(MM_EnvironmentBase *env, volatile uintptr_t *cachedEntryCount);
	virtual void tearDown(MM_EnvironmentBase *env);

	/**
	 * Retrieve Allocated cache entry count
	 */
	MMINLINE uintptr_t
	getAllocatedCacheCount()
	{
		return _totalAllocatedEntryCount;
	}
	 
	/**
	 * Resizes the number of cache entries.
	 *
	 * @param env[in] A GC thread
	 * @param allocatedCacheEntryCount[in] The number of cache entries which this list should be resized to contain
	 * @param incrementCacheEntryCount[in] increment increase count
	 * @return true if resize success
	 */
	bool resizeCacheEntries(MM_EnvironmentBase *env, uintptr_t allocatedCacheEntryCount, uintptr_t incrementCacheEntryCount);

	/**
	 * Remove all heap allocated chunks from chunks list
	 * Fixup caches list
	 * @param env - current thread environment
	 */
	void removeAllHeapAllocatedChunks(MM_EnvironmentStandard *env);

	/**
	 * Create chunk of caches in heap
	 * @param env - current thread environment
	 * @param memorySubSpace memory subspace to create chunk at
	 * @param requestCollector collector issued a memory allocation request
	 * @return pointer to first scan cache if allocation is successful
	 */
	MM_CopyScanCacheStandard * appendCacheEntriesInHeap(MM_EnvironmentStandard *env, MM_MemorySubSpace *memorySubSpace, MM_Collector *requestCollector);

	/**
	 * Walk all sublists and count total number of entries
	 * Return true if all permanent scan caches are currently returned back to list
	 * @return true if total number of entries attached to sublists is equal total number of allocated scan caches
	 */
	bool areAllCachesReturned();

	/**
	 * Walk all sublists and count of number of used cache entries.
	 * Being a non-atomic walk this is an approximate count. 
	 * Meant mostly for statistical usage, or possibly for some heuristics, 
	 * but not for synchronous decisions.
	 * @return appox number of used caches
	 */
	uintptr_t getApproximateEntryCount();

	/**
	 * Add the specified entry to this list.
	 * @param env[in] the current GC thread
	 * @param cacheEntry[in] the cache entry to add
	 */
	void pushCache(MM_EnvironmentBase *env, MM_CopyScanCacheStandard *cacheEntry);

	/**
	 * Pop a cache entry from this list.
	 * @param env[in] the current GC thread
	 * @return the cache entry, or NULL if the list is empty
	 */
	MM_CopyScanCacheStandard *popCache(MM_EnvironmentBase *env);

	/**
	 * Create a CopyScanCacheList object.
	 */
	MM_CopyScanCacheList() 
		: MM_BaseVirtual()
		, _allocationInHeap(false)
		, _sublists(NULL)
		, _sublistCount(0)
		, _chunkHead(NULL)
		, _incrementEntryCount(0)
		, _totalAllocatedEntryCount(0)
		, _cachedEntryCount(NULL)
	{
		_typeId = __FUNCTION__;
	}
	
};

#endif /* COPYSCANCACHELIST_HPP_ */
