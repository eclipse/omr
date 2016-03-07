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

#include "HeapRegionDescriptor.hpp"
#include "ObjectHeapBufferedIterator.hpp"

#include "EmptyListPopulator.hpp"

void 
MM_EmptyListPopulator::initializeObjectHeapBufferedIteratorState(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state) const
{
}

void
MM_EmptyListPopulator::reset(MM_HeapRegionDescriptor* region, GC_ObjectHeapBufferedIteratorState* state, void* base, void* top) const
{
}

uintptr_t 
MM_EmptyListPopulator::populateObjectHeapBufferedIteratorCache(omrobjectptr_t* cache, uintptr_t count, GC_ObjectHeapBufferedIteratorState* state) const
{
	return 0;
}

void
MM_EmptyListPopulator::advance(uintptr_t size, GC_ObjectHeapBufferedIteratorState* state) const
{
}
