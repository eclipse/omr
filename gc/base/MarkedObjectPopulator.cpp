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
			state->data1 = ((uintptr_t)object) + J9_GC_MINIMUM_OBJECT_SIZE;
#else /* OMR_GC_MINIMUM_OBJECT_SIZE */
			state->data1 = ((uintptr_t)object) + sizeof(J9Object);
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
