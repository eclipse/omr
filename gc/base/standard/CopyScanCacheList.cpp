/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "omrcfg.h"
#include "omrport.h"

#include "CopyScanCacheList.hpp"
#include "CopyScanCacheChunk.hpp"
#include "CopyScanCacheChunkInHeap.hpp"
#include "CopyScanCacheStandard.hpp"
#include "Dispatcher.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"

#if defined(OMR_GC_MODRON_SCAVENGER)

bool
MM_CopyScanCacheList::initialize(MM_EnvironmentBase *env, volatile uintptr_t *cachedEntryCount)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	bool result = true;
	
	_sublistCount = extensions->cacheListSplit;
	Assert_MM_true(0 < _sublistCount);

	_sublists = (struct CopyScanCacheSublist *)extensions->getForge()->allocate(sizeof(struct CopyScanCacheSublist) * _sublistCount, MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _sublists) {
		result = false;
	} else {
		memset(_sublists, 0, sizeof(struct CopyScanCacheSublist) * _sublistCount);
		for (uintptr_t i = 0; i < _sublistCount; i++) {
			if (!_sublists[i]._cacheLock.initialize(env, &extensions->lnrlOptions, "MM_CopyScanCacheList:_sublists[]._cacheLock")) {
				result = false;
				break;
			}
		}
	}

	_cachedEntryCount = cachedEntryCount;

	return result;
}

void
MM_CopyScanCacheList::tearDown(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();

	/* Free the memory allocated for the caches */
	while (NULL != _chunkHead) {
		MM_CopyScanCacheChunk *_next = _chunkHead->getNext();
		_chunkHead->kill(env);
		_chunkHead = _next;
	}
	
	if (NULL != _sublists) {
		for (uintptr_t i = 0; i < _sublistCount; i++) {
			_sublists[i]._cacheLock.tearDown();
		}
		extensions->getForge()->free(_sublists);
		_sublists = NULL;
	}

}

bool
MM_CopyScanCacheList::resizeCacheEntries(MM_EnvironmentBase *env, uintptr_t allocateCacheEntryCount, uintptr_t incrementCacheEntryCount)
{
	MM_GCExtensionsBase *ext = env->getExtensions();
	
	/* 0 has special meaning of 'do not change' */
	if (0 == allocateCacheEntryCount) {
		allocateCacheEntryCount = _totalAllocatedEntryCount;
	}
	if (0 != incrementCacheEntryCount) {
		 _incrementEntryCount = incrementCacheEntryCount;
	}
	
	/* If -Xgc:fvtest=scanCacheCountn has been specified, then restrict the number of scan caches to n.
	 * Stop all future resizes from having any effect. */
	if (0 != ext->fvtest_scanCacheCount) {
		if (0 == _totalAllocatedEntryCount) {
			allocateCacheEntryCount = ext->fvtest_scanCacheCount;
			_incrementEntryCount = 0;
			return appendCacheEntries(env, allocateCacheEntryCount);
		} else {
			return true;
		}
	}
	
	if ( allocateCacheEntryCount > _totalAllocatedEntryCount) {
		/* Increase cacheEntries by incrementEntryCount */
		return appendCacheEntries(env, _incrementEntryCount);
	}

	/* downsizing is non-trivial with current list/chunk implementation since
	 * the free caches are scattered across the chunks and cross reference themselves */
	
	return true;
}

void
MM_CopyScanCacheList::removeAllHeapAllocatedChunks(MM_EnvironmentStandard *env)
{
	if (_allocationInHeap) {
		uintptr_t reservedInHeap = 0;
		/* execute if allocation in heap occur this Local GC */
		/*
		 * Walk caches list first to remove all references to heap allocated caches
		 */
		for (uintptr_t index = 0; index < _sublistCount; index++) {
			MM_CopyScanCacheStandard *previousCache = NULL;
			MM_CopyScanCacheStandard *cache = _sublists[index]._cacheHead;
	
			while(cache != NULL) {
				if (0 != (cache->flags & OMR_SCAVENGER_CACHE_TYPE_HEAP)) {
					/* this cache is heap allocated - remove it from list */
					if (NULL == previousCache) {
						/* still be a first element */
						_sublists[index]._cacheHead = (MM_CopyScanCacheStandard *)cache->next;
					} else {
						/* remove middle element */
						previousCache->next = cache->next;
					}
					/* count number of removed */
					reservedInHeap += 1;

					Assert_MM_true(_sublists[index]._entryCount >= 1);
					_sublists[index]._entryCount -= 1;
				} else {
					/* not heap allocated - just skip */
					previousCache = cache;
				}
				cache = (MM_CopyScanCacheStandard *)cache->next;
			}
		}

		/*
		 *  Walk caches chunks list and release all heap allocated
		 */
		MM_CopyScanCacheChunk *previousChunk = NULL;
		MM_CopyScanCacheChunk *chunk = _chunkHead;

		while(chunk != NULL) {
			MM_CopyScanCacheChunk *nextChunk = chunk->getNext();

			if (0 != (chunk->getBase()->flags & OMR_SCAVENGER_CACHE_TYPE_HEAP)) {
				/* this chunk is heap allocated - remove it from list */
				if (NULL == previousChunk) {
					/* still be a first element */
					_chunkHead = nextChunk;
				} else {
					/* remove middle element */
					previousChunk->setNext(nextChunk);
				}

				/* release heap allocated chunk */
				chunk->kill(env);

			} else {
				/* not heap allocated - just skip */
				previousChunk = chunk;
			}
			chunk = nextChunk;
		}

		/* clear flag - no more heap allocated caches */
		_allocationInHeap = false;

		Assert_MM_true(0 < reservedInHeap);

		/* increase number of permanent scan caches by number of incrementEntryCount */
		appendCacheEntries(env, _incrementEntryCount);
	}
}

bool
MM_CopyScanCacheList::appendCacheEntries(MM_EnvironmentBase *env, uintptr_t cacheEntryCount)
{
	bool result = false;
	MM_CopyScanCacheStandard *sublistTail = NULL;
	MM_CopyScanCacheChunk *chunk = MM_CopyScanCacheChunk::newInstance(env, cacheEntryCount, _chunkHead, &sublistTail);
	if(NULL != chunk) {
		uintptr_t index = getSublistIndex(env);

		Assert_MM_true(NULL != sublistTail);
		Assert_MM_true(NULL == sublistTail->next);

		_sublists[index]._cacheLock.acquire();
		/* attach sublist of caches in chunk to main list */
		sublistTail->next = _sublists[index]._cacheHead;
		_sublists[index]._cacheHead = chunk->getBase();
		_sublists[index]._entryCount += cacheEntryCount;
		_sublists[index]._cacheLock.release();

		_chunkHead = chunk;
		_totalAllocatedEntryCount += cacheEntryCount;
		result = true;
	}
	return result;
}

MM_CopyScanCacheStandard *
MM_CopyScanCacheList::appendCacheEntriesInHeap(MM_EnvironmentStandard *env, MM_MemorySubSpace *memorySubSpace, MM_Collector *requestCollector)
{
	MM_CopyScanCacheStandard *result = NULL;
	MM_CopyScanCacheStandard *sublistTail = NULL;
	uintptr_t entries = 0;
	MM_CopyScanCacheChunkInHeap *chunk = MM_CopyScanCacheChunkInHeap::newInstance(env, _chunkHead, memorySubSpace, requestCollector, &sublistTail, &entries);
	if(NULL != chunk) {
		uintptr_t index = getSublistIndex(env);

		Assert_MM_true(0 <= entries);
		Assert_MM_true(NULL != sublistTail);
		Assert_MM_true(NULL == sublistTail->next);

		_sublists[index]._cacheLock.acquire();
		/* attach sublist of caches in chunk to main list */
		sublistTail->next = _sublists[index]._cacheHead;
		result = chunk->getBase();
		_sublists[index]._cacheHead = (MM_CopyScanCacheStandard *)result->next;
		_sublists[index]._entryCount += (entries - 1);
		_sublists[index]._cacheLock.release();

		_chunkHead = chunk;
		_allocationInHeap = true;
	}
	return result;
}

uintptr_t
MM_CopyScanCacheList::getApproximateEntryCount()
{
	uintptr_t entries = 0;
	for (uintptr_t i = 0; i < _sublistCount; i++) {
		entries += _sublists[i]._entryCount;
	}

	return entries;
}

bool
MM_CopyScanCacheList::areAllCachesReturned()
{
	return (getApproximateEntryCount() == _totalAllocatedEntryCount);
}

MMINLINE void
MM_CopyScanCacheList::incrementCount(CopyScanCacheSublist *sublist, uintptr_t value)
{
	if ((0 == sublist->_entryCount) && (NULL != _cachedEntryCount)) {
		if (1 == _sublistCount) {
			*_cachedEntryCount += 1;
		} else {
			/* use an atomic, as the locks have been split up */
			MM_AtomicOperations::add(_cachedEntryCount, 1);
		}
	}
	sublist->_entryCount += value;
}

MMINLINE void
MM_CopyScanCacheList::decrementCount(CopyScanCacheSublist *sublist, uintptr_t value)
{
	Assert_MM_true(sublist->_entryCount >= value);
	sublist->_entryCount -= value;

	if ((0 == sublist->_entryCount) && (NULL != _cachedEntryCount)) {
		Assert_MM_true(*_cachedEntryCount >= 1);
		Assert_MM_true(NULL == sublist->_cacheHead);
		if (1 == _sublistCount) {
			*_cachedEntryCount -= 1;
		} else {
			/* use an atomic, as the locks have been split up */
			MM_AtomicOperations::subtract(_cachedEntryCount, 1);
		}
	}
}

void
MM_CopyScanCacheList::pushCache(MM_EnvironmentBase *env, MM_CopyScanCacheStandard *cacheEntry)
{
	MM_CopyScanCacheList::CopyScanCacheSublist *list = &_sublists[getSublistIndex(env)];

	/* This is a useful assertion to find who drop the same element to list twice
	 * It is fatal and caused hang right away.
	 * However it is too expensive to have this assertion here permanently
	 */
	/*
	Assert_MM_true(newCacheEntry != _list->_cacheHead);
	*/

	list->_cacheLock.acquire();
	cacheEntry->next = list->_cacheHead;
	list->_cacheHead = cacheEntry;
	incrementCount(list, 1);
	list->_cacheLock.release();
}

MM_CopyScanCacheStandard *
MM_CopyScanCacheList::popCache(MM_EnvironmentBase *env)
{
	uintptr_t index = getSublistIndex(env);
	MM_CopyScanCacheStandard *cache = NULL;

	for (uintptr_t i = 0; i < _sublistCount; i++) {
		MM_CopyScanCacheList::CopyScanCacheSublist *list = &_sublists[index];

		if (NULL != list->_cacheHead) {
			env->_scavengerStats._acquireListLockCount += 1;
			list->_cacheLock.acquire();
			cache = list->_cacheHead;
			if (NULL != cache) {
				list->_cacheHead = (MM_CopyScanCacheStandard *)cache->next;
				decrementCount(list, 1);

				if (NULL == list->_cacheHead) {
					Assert_MM_true(0 == list->_entryCount);
				}
			}
			list->_cacheLock.release();

			if (NULL != cache) {
				break;
			}
		}

		index = (index + 1) % _sublistCount;
	}

	return cache;
}

#endif /* OMR_GC_MODRON_SCAVENGER */

