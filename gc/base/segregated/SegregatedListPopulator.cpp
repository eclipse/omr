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

#include "HeapRegionDescriptor.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "ObjectHeapBufferedIterator.hpp"
#include "ObjectHeapIteratorSegregated.hpp"

#include "SegregatedListPopulator.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

void
MM_SegregatedListPopulator::initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const
{
	reset(region, state, region->getLowAddress(), region->getHighAddress());
	state->data3 = (uintptr_t) region->getRegionType();
	state->data4 = ((MM_HeapRegionDescriptorSegregated*)region)->getCellSize();
}


void
MM_SegregatedListPopulator::reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const
{
	state->skipFirstObject = false;
	state->data1 = (uintptr_t) base;
	state->data2 = (uintptr_t) top;
}

uintptr_t
MM_SegregatedListPopulator::populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const
{
	omrobjectptr_t startPtr = (omrobjectptr_t) state->data1;
	uintptr_t size = 0;

	if (NULL != startPtr) {
		GC_ObjectHeapIteratorSegregated iterator(state->extensions, startPtr, (omrobjectptr_t)state->data2, (MM_HeapRegionDescriptor::RegionType)state->data3, state->data4, state->includeDeadObjects, state->skipFirstObject);
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
			state->data1 = (uintptr_t)object;
			state->skipFirstObject = true;
		}
	}
	
	return size;
}

void
MM_SegregatedListPopulator::advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const
{
	state->data1 = state->data1 + size;
	state->skipFirstObject = false;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
