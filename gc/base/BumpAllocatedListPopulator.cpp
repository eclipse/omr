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

#include "BumpAllocatedListPopulator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "MemoryPoolBumpPointer.hpp"
#include "ObjectHeapBufferedIterator.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"

void
MM_BumpAllocatedListPopulator::initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const
{
	reset(region, state, region->getLowAddress(), region->getHighAddress());
}

void
MM_BumpAllocatedListPopulator::reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const
{
	state->skipFirstObject = false;
	state->data1 = (uintptr_t) base;
	/* note that walking from low address to allocation pointer is only valid if the region has been freshly taken from eden (since
	 * copy-forward or class unloading could make this space unwalkable.
	 */
	Assert_MM_true(MM_HeapRegionDescriptor::BUMP_ALLOCATED == region->getRegionType());
	state->data2 = OMR_MIN((uintptr_t)top, (uintptr_t) ((MM_MemoryPoolBumpPointer *)region->getMemoryPool())->getAllocationPointer());
}

uintptr_t
MM_BumpAllocatedListPopulator::populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const
{
	uintptr_t size = 0;
	omrobjectptr_t startPtr = (omrobjectptr_t) state->data1;
	
	if (NULL != startPtr) {
		GC_ObjectHeapIteratorAddressOrderedList iterator(state->extensions, startPtr, (omrobjectptr_t) state->data2, state->includeDeadObjects, state->skipFirstObject);
		omrobjectptr_t object = NULL;
		
		for (uintptr_t i = 0; i < count; i++) {
			object = iterator.nextObjectNoAdvance();
			if (NULL == object) {
				break;
			}
	
			cache[i] = object;
			size++;
		}
	
		if (0 != size) {
			state->data1 = (uintptr_t) object;
			state->skipFirstObject = true;
		}
	}

	return size;
}

void
MM_BumpAllocatedListPopulator::advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const
{
	state->data1 = state->data1 + size;
	state->skipFirstObject = false;
}
