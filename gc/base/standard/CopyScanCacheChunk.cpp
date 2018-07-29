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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrcfg.h"
#include "omrport.h"

#include "CopyScanCacheChunk.hpp"
 
#include "CopyScanCacheStandard.hpp"
#include "CopyScanCache.hpp"

#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "ModronAssertions.h"


MM_CopyScanCacheChunk *
MM_CopyScanCacheChunk::newInstance(MM_EnvironmentBase* env, uintptr_t cacheEntryCount, MM_CopyScanCacheChunk *nextChunk, MM_CopyScanCache **sublistTail)
{
	MM_CopyScanCacheChunk *chunk;
	
	chunk = (MM_CopyScanCacheChunk *)env->getForge()->allocate(sizeof(MM_CopyScanCacheChunk) + cacheEntryCount * sizeof(MM_CopyScanCache), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE()); // CSCS
	if (chunk) {
		new(chunk) MM_CopyScanCacheChunk();
		chunk->_baseCache = (MM_CopyScanCache *)(chunk + 1);
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
MM_CopyScanCacheChunk::initialize(MM_EnvironmentBase *env, uintptr_t cacheEntryCount, MM_CopyScanCacheChunk *nextChunk, uintptr_t flags, MM_CopyScanCache **sublistTail)
{
	//OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	_nextChunk = nextChunk;

	Assert_MM_true(0 < cacheEntryCount);

	MM_CopyScanCache *previousCache = NULL;
	*sublistTail = _baseCache + cacheEntryCount - 1;

	for (MM_CopyScanCache *currentCache = *sublistTail; currentCache >= _baseCache; currentCache--) {
		new(currentCache) MM_CopyScanCache(flags);

		//omrtty_printf("{setting currentCache->next for %p to %p}\n", currentCache, previousCache);

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


