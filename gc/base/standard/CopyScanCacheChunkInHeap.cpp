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

#include "omrcfg.h"
#include "omrport.h"

#include "CopyScanCacheChunkInHeap.hpp"

#include "AllocateDescription.hpp"
#include "Collector.hpp"
#include "CopyScanCacheStandard.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "MemorySubSpace.hpp"

MM_CopyScanCacheChunkInHeap *
MM_CopyScanCacheChunkInHeap::newInstance(MM_EnvironmentStandard *env, MM_CopyScanCacheChunk *nextChunk, MM_MemorySubSpace *memorySubSpace, MM_Collector *requestCollector,
		MM_CopyScanCacheStandard **sublistTail, uintptr_t *entries)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_CopyScanCacheChunkInHeap *chunk = NULL;
	void *addrBase = NULL;
	void *addrTop = NULL;

	/*
	 * Calculate size of memory to allocate in heap:
	 * 1. create hole in allocated memory
	 * 2. create MM_CopyScanCacheChunkInHeap
	 * 3. create number of CopyScanCache objects
	 *
	 * with total size barely larger then tlhMinimumSize
	 */
	/* Headers size: HeapLinkedFreeHeader and CopyScanCacheChunkInHeap */
	uintptr_t sizeToAllocate = sizeof(MM_HeapLinkedFreeHeader) + sizeof(MM_CopyScanCacheChunkInHeap);
	uintptr_t numberOfCaches = 1;

	if (sizeToAllocate < extensions->tlhMinimumSize) {
		/* calculate number of caches to just barely exceed tlhMinimumSize */
		numberOfCaches = ((extensions->tlhMinimumSize - sizeToAllocate) / sizeof(MM_CopyScanCacheStandard)) + 1;
	}

	/* total size required to allocate */
	sizeToAllocate += numberOfCaches * sizeof(MM_CopyScanCacheStandard);
	/* this is going to be allocated on the heap so ensure that the chunk we are allocating is adjusted for heap alignment (since object consumed sizes already have this requirement) */
	sizeToAllocate = MM_Math::roundToCeiling(env->getObjectAlignmentInBytes(), sizeToAllocate);

	/* Attempt to allocate in given subspace */
	MM_AllocateDescription allocDescription(sizeToAllocate, 0, false, true);
	addrBase = (MM_CopyScanCacheChunkInHeap *)memorySubSpace->collectorAllocate(env, requestCollector, &allocDescription);

	if (NULL != addrBase) {
		bool const compressed = env->compressObjectReferences();
		/* memory allocated */
		addrTop = (void *)(((uint8_t *)addrBase) + sizeToAllocate);

		/* create a hole */
		MM_HeapLinkedFreeHeader::fillWithHoles(addrBase, sizeToAllocate, compressed);

		/* create a CopyScanCacheChunkInHeap itself */
		chunk = (MM_CopyScanCacheChunkInHeap *)((uintptr_t)addrBase + sizeof(MM_HeapLinkedFreeHeader));
		new(chunk) MM_CopyScanCacheChunkInHeap(addrBase, addrTop, memorySubSpace);
		chunk->_baseCache = (MM_CopyScanCacheStandard *)(chunk + 1);
		if(chunk->initialize(env, numberOfCaches, nextChunk, OMR_SCAVENGER_CACHE_TYPE_HEAP, sublistTail)) {
			*entries = numberOfCaches;
		} else {
			chunk->kill(env);
			chunk = NULL;
		}
	}
	return chunk;
}

void
MM_CopyScanCacheChunkInHeap::kill(MM_EnvironmentBase *env)
{
	tearDown(env);

	/* return memory to pool */
	_memorySubSpace->abandonHeapChunk(_addrBase, _addrTop);
}


