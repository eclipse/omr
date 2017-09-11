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

#include "ModronAssertions.h"

#include "GCExtensionsBase.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "ObjectHeapBufferedIterator.hpp"

#include "MarkedObjectPopulator.hpp"

void
MM_MarkedObjectPopulator::initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const
{
	reset(region, state, region->getLowAddress(), region->getHighAddress());
}

void
MM_MarkedObjectPopulator::reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const
{
	state->skipFirstObject = false;
	state->data1 = (uintptr_t) base;
	state->data2 = (uintptr_t) top;
}

uintptr_t
MM_MarkedObjectPopulator::populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const
{
	uintptr_t size = 0;
	uintptr_t *startPtr = (uintptr_t*) state->data1;
	uintptr_t *endPtr = (uintptr_t*) state->data2;
	Assert_MM_true(false == state->skipFirstObject);
	
	if (NULL != startPtr) {
		MM_HeapMap *markMap = state->extensions->previousMarkMap;
		Assert_MM_true(NULL != markMap);
		
		/* TODO: what to do if we've been asked to include dead objects? There's no obvious way to include them. Perhaps it doesn't matter? Or we can infer them? */
		MM_HeapMapIterator iterator(state->extensions, markMap, startPtr, endPtr, false);

		omrobjectptr_t object = NULL;
		
		for (uintptr_t i = 0; i < count; i++) {
			object = iterator.nextObject();
			if (NULL == object) {
				break;
			}

			cache[i] = object;
			size++;
		}

		if (NULL == object) {
			/* set startPtr to NULL */
			state->data1 = 0;
		} else {
			/* set startPtr to just past the last object seen */
#if defined(OMR_GC_MINIMUM_OBJECT_SIZE)
			state->data1 = ((uintptr_t)object) + OMR_MINIMUM_OBJECT_SIZE;
#else /* OMR_GC_MINIMUM_OBJECT_SIZE */
			state->data1 = ((uintptr_t)object) + sizeof(uintptr_t);
#endif /* OMR_GC_MINIMUM_OBJECT_SIZE */
		}
	}

	return size;
}

void
MM_MarkedObjectPopulator::advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const
{
	state->data1 = state->data1 + size;
}
