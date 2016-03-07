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

