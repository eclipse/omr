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


#include "ModronAssertions.h"

#include "GCExtensionsBase.hpp"
#include "HeapRegionDescriptor.hpp"

#include "ObjectHeapBufferedIterator.hpp"

GC_ObjectHeapBufferedIterator::GC_ObjectHeapBufferedIterator(MM_GCExtensionsBase *extensions, MM_HeapRegionDescriptor *region, bool includeDeadObjects, uintptr_t maxElementsToCache)
{
	init(extensions, region, region->getLowAddress(), region->getHighAddress(), includeDeadObjects, maxElementsToCache);
}

GC_ObjectHeapBufferedIterator::GC_ObjectHeapBufferedIterator(MM_GCExtensionsBase *extensions, MM_HeapRegionDescriptor *region, void *base, void *top, bool includeDeadObjects, uintptr_t maxElementsToCache)
{
	init(extensions, region, base, top, includeDeadObjects, maxElementsToCache);
}

void
GC_ObjectHeapBufferedIterator::init(MM_GCExtensionsBase* extensions, MM_HeapRegionDescriptor* region, void *base, void *top, bool includeDeadObjects, uintptr_t maxElementsToCache)
{
	_region = region;
	_cacheIndex = 0;
	_cacheSizeToUse = OMR_MIN(maxElementsToCache, CACHE_SIZE);

	_populator = getPopulator();
	_state.extensions = extensions;
	_state.includeDeadObjects = includeDeadObjects;
	_populator->initializeObjectHeapBufferedIteratorState(region, &_state);
	_cacheCount = _populator->populateObjectHeapBufferedIteratorCache(_cache, _cacheSizeToUse, &_state);
}

void
GC_ObjectHeapBufferedIterator::reset(uintptr_t *base, uintptr_t *top)
{
	_populator->reset(_region, &_state, base, top);
	_cacheIndex = 0;
	_cacheCount = _populator->populateObjectHeapBufferedIteratorCache(_cache, _cacheSizeToUse, &_state);
}

omrobjectptr_t
GC_ObjectHeapBufferedIterator::nextObject()
{
	if (_cacheCount == 0) {
		return NULL;
	}

	if (_cacheIndex == _cacheCount) {
		_cacheIndex = 0;
		_cacheCount = _populator->populateObjectHeapBufferedIteratorCache(_cache, _cacheSizeToUse, &_state);

		if (_cacheCount == 0) {
			return NULL;
		}
	}

	omrobjectptr_t next = _cache[_cacheIndex];
	_cacheIndex++;
	return next;
}

const MM_ObjectHeapBufferedIteratorPopulator*
GC_ObjectHeapBufferedIterator::getPopulator()
{
	const MM_ObjectHeapBufferedIteratorPopulator *populator = NULL;

	switch (_region->getRegionType()) {
	case MM_HeapRegionDescriptor::RESERVED:
	case MM_HeapRegionDescriptor::FREE:
	case MM_HeapRegionDescriptor::ADDRESS_ORDERED_IDLE:
	case MM_HeapRegionDescriptor::BUMP_ALLOCATED_IDLE:
		/* (for all intents and purposes, an IDLE region is the same as a FREE region) */
	case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
		populator = &_emptyListPopulator;
		break;
	case MM_HeapRegionDescriptor::BUMP_ALLOCATED:
		populator = &_bumpAllocatedListPopulator;
		break;
	case MM_HeapRegionDescriptor::ADDRESS_ORDERED:
		populator = &_addressOrderedListPopulator;
		break;
	case MM_HeapRegionDescriptor::ADDRESS_ORDERED_MARKED:
	case MM_HeapRegionDescriptor::BUMP_ALLOCATED_MARKED:
		populator = &_markedObjectPopulator;
		break;
#if defined(OMR_GC_SEGREGATED_HEAP)
	case MM_HeapRegionDescriptor::SEGREGATED_SMALL:
	case MM_HeapRegionDescriptor::SEGREGATED_LARGE:
		populator = &_segregatedListPopulator;
		break;
#endif /* OMR_GC_SEGREGATED_HEAP */
	default:
		Assert_MM_unreachable();
		break;
	}

	return populator;
}

void
GC_ObjectHeapBufferedIterator::advance(uintptr_t sizeInBytes)
{
	_cacheIndex = 0;
	_populator->advance(sizeInBytes, &_state);
	_cacheCount = _populator->populateObjectHeapBufferedIteratorCache(_cache, _cacheSizeToUse, &_state);
}

