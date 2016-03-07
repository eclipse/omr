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

#include "omrcfg.h"
#include "omrport.h"

#include "CopyScanCacheChunk.hpp"
#include "CopyScanCacheStandard.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronAssertions.h"


MM_CopyScanCacheChunk *
MM_CopyScanCacheChunk::newInstance(MM_EnvironmentBase* env, uintptr_t cacheEntryCount, MM_CopyScanCacheChunk *nextChunk, MM_CopyScanCacheStandard **sublistTail)
{
	MM_CopyScanCacheChunk *chunk;
	
	chunk = (MM_CopyScanCacheChunk *)env->getForge()->allocate(sizeof(MM_CopyScanCacheChunk) + cacheEntryCount * sizeof(MM_CopyScanCacheStandard), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (chunk) {
		new(chunk) MM_CopyScanCacheChunk();
		chunk->_baseCache = (MM_CopyScanCacheStandard *)(chunk + 1);
		if(!chunk->initialize(env, cacheEntryCount, nextChunk, 0, sublistTail)) {
			chunk->kill(env);
			return NULL;
		}
	}
	return chunk;	
}

void
MM_CopyScanCacheChunk::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_CopyScanCacheChunk::initialize(MM_EnvironmentBase *env, uintptr_t cacheEntryCount, MM_CopyScanCacheChunk *nextChunk, uintptr_t flags, MM_CopyScanCacheStandard **sublistTail)
{
	_nextChunk = nextChunk;

	Assert_MM_true(0 < cacheEntryCount);

	MM_CopyScanCacheStandard *previousCache = NULL;
	*sublistTail = _baseCache + cacheEntryCount - 1;

	for (MM_CopyScanCacheStandard *currentCache = *sublistTail; currentCache >= _baseCache; currentCache--) {
		new(currentCache) MM_CopyScanCacheStandard(flags);
		currentCache->next = previousCache;
		previousCache = currentCache;
	}
	
	return true;
}

void
MM_CopyScanCacheChunk::tearDown(MM_EnvironmentBase *env)
{
	_baseCache = NULL;
	_nextChunk = NULL;
}


