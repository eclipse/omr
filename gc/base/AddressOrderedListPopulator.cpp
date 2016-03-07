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

#include "AddressOrderedListPopulator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "ObjectHeapBufferedIterator.hpp"
#include "ObjectHeapIteratorAddressOrderedList.hpp"

void
MM_AddressOrderedListPopulator::initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const
{
	reset(region, state, region->getLowAddress(), region->getHighAddress());
}

void
MM_AddressOrderedListPopulator::reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const
{
	state->skipFirstObject = false;
	state->data1 = (uintptr_t) base;
	state->data2 = (uintptr_t) top;
}

uintptr_t
MM_AddressOrderedListPopulator::populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const
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
MM_AddressOrderedListPopulator::advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const
{
	state->data1 = state->data1 + size;
	state->skipFirstObject = false;
}
