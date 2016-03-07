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

#if !defined(SEGREGATEDALLOCATIONINTERFACE_HPP_)
#define SEGREGATEDALLOCATIONINTERFACE_HPP_

#include "omrcfg.h"
#include "sizeclasses.h"

#include "LanguageSegregatedAllocationCache.hpp"

#include "ObjectAllocationInterface.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

class MM_SizeClasses;

typedef struct SegregatedAllocationCacheStats {
	uint64_t bytesPreAllocatedTotal[OMR_SIZECLASSES_NUM_SMALL + 1]; /**< The total count of cells pre-allocated since the cache has existed (per size class). */
	uint64_t replenishesTotal[OMR_SIZECLASSES_NUM_SMALL + 1]; /**< The amount of times the cache has been replenished since the cache has existed (per size class). */
	uint64_t bytesPreAllocatedSinceRestart[OMR_SIZECLASSES_NUM_SMALL + 1]; /**< The count of cells pre-allocated since the cache was last flushed. */
	uint64_t replenishesSinceRestart[OMR_SIZECLASSES_NUM_SMALL + 1]; /**< The amount of times the cache has been replenished since the cache was last flushed. */
} SegregatedAllocationCacheStats;

class MM_SegregatedAllocationInterface : public MM_ObjectAllocationInterface 
{
	/*
	 * Data members
	 */
public:
protected:
private:
	MM_LanguageSegregatedAllocationCache _languageAllocationCache;
	LanguageSegregatedAllocationCacheEntryStruct *_allocationCache; /**< The current cache (per size class). */
	uintptr_t _replenishSizes[OMR_SIZECLASSES_NUM_SMALL + 1]; /**< The next replenish size (per size class). */
	SegregatedAllocationCacheStats _allocationCacheStats; /**< Contains stats about the past pre-allocations. */
	MM_SizeClasses* _sizeClasses; /**< The size classes used to map byte sizes to size class indexes. */
	
	bool _cachedAllocationsEnabled; /**< Are cached allocations enabled? */
	
	uintptr_t *_allocationCacheBases[OMR_SIZECLASSES_NUM_SMALL + 1]; /**< The Base of each current cache (per size class). */

	/*
	 * Function members
	 */
public:
	static MM_SegregatedAllocationInterface* newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);
	
	static MMINLINE
	MM_SegregatedAllocationInterface* getObjectAllocationInterface(MM_ObjectAllocationInterface *allocationInterface)
	{
		return (MM_SegregatedAllocationInterface *) allocationInterface;
	}
	
	static MMINLINE
	MM_SegregatedAllocationInterface* getObjectAllocationInterface(MM_EnvironmentBase* env)
	{
		return getObjectAllocationInterface(env->_objectAllocationInterface);
	}
	
	virtual void *allocateObject(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArray(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
#if defined(OMR_GC_ARRAYLETS)
	virtual void *allocateArrayletSpine(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
	virtual void *allocateArrayletLeaf(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription, MM_MemorySpace *memorySpace, bool shouldCollectOnFailure);
#endif /* OMR_GC_ARRAYLETS */
	
	virtual void flushCache(MM_EnvironmentBase *env);
	virtual void reconnectCache(MM_EnvironmentBase *env);

	void restartCache(MM_EnvironmentBase *env);
	uintptr_t getAllocatableSize(uintptr_t sizeClass) { return (uintptr_t)_allocationCache[sizeClass].top - (uintptr_t)_allocationCache[sizeClass].current; }
	void* allocateFromCache(MM_EnvironmentBase* env, uintptr_t sizeInBytes);
	void replenishCache(MM_EnvironmentBase* env, uintptr_t sizeInBytes, void *cacheMemory, uintptr_t cacheSize);
	uintptr_t getReplenishSize(MM_EnvironmentBase* env, uintptr_t sizeInBytes);
	
	virtual void enableCachedAllocations(MM_EnvironmentBase *env);
	virtual void disableCachedAllocations(MM_EnvironmentBase *env);
	virtual bool cachedAllocationsEnabled(MM_EnvironmentBase *env) { return _cachedAllocationsEnabled; }
	
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	MM_SegregatedAllocationInterface(MM_EnvironmentBase *env) :
		MM_ObjectAllocationInterface(env),
		_sizeClasses(NULL),
		_cachedAllocationsEnabled(true)
	{
		_typeId = __FUNCTION__;
		memset(_allocationCacheBases, 0, sizeof(_allocationCacheBases));
	};
	
private:
	void updateFrequentObjectsStats(MM_EnvironmentBase *env, uintptr_t sizeClass);
	
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /*SEGREGATEDALLOCATIONINTERFACE_HPP_*/
