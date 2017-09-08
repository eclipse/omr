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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(COPYSCANCACHECHUNKINHEAP_HPP_)
#define COPYSCANCACHECHUNKINHEAP_HPP_

#include "CopyScanCacheChunk.hpp"
#include "EnvironmentStandard.hpp"

class MM_Collector;
class MM_CopyScanCacheStandard;
class MM_MemorySubSpace;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_CopyScanCacheChunkInHeap : public MM_CopyScanCacheChunk
{
private:
	void *_addrBase;	/**< lowest address of allocated memory in heap */
	void *_addrTop;		/**< highest address of allocated memory in heap */
	MM_MemorySubSpace *_memorySubSpace;	/**< subspace memory is allocated with */

protected:

public:

private:

protected:

public:
	/**
	 * Create new instance in heap allocated memory:
	 * 1. Put MM_HeapLinkedFreeHeader first to keep heap walkable
	 * 2. Put MM_CopyScanCacheChunkInHeap itself
	 * 3. Put number of scan caches MM_CopyScanCacheStandard to just barely exceed tlhMinimumSize.
	 *    Such size should be small enough to easy to allocate and large enough to be reused as tlh
	 *
	 * @param[in] env current thread environment
	 * @param[in] nextChunk pointer to current chunk list head
	 * @param[in] memorySubSpace memory subspace to create chunk at
	 * @param[in] requestCollector collector issued a memory allocation request
	 * @param[out] sublistTail tail of sublist of scan caches created in this chunk
	 * @param[out] entries number of scan cache entries created in this chunk
	 * @return pointer to allocated chunk
	 */
	static MM_CopyScanCacheChunkInHeap *newInstance(MM_EnvironmentStandard *env, MM_CopyScanCacheChunk *nextChunk, MM_MemorySubSpace *memorySubSpace, MM_Collector *requestCollector,
			MM_CopyScanCacheStandard **sublistTail, uintptr_t *entries);

	virtual void kill(MM_EnvironmentBase *env);

	/**
	 * Create a CopyScanCacheChunk object.
	 */
	MM_CopyScanCacheChunkInHeap(void *addBase, void*addrTop, MM_MemorySubSpace *memorySubSpace)
		: MM_CopyScanCacheChunk()
		, _addrBase(addBase)
		, _addrTop(addrTop)
		, _memorySubSpace(memorySubSpace)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* COPYSCANCACHECHUNKINHEAP_HPP_ */

