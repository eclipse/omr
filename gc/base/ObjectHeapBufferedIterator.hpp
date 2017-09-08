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


#if !defined(OBJECTHEAPBUFFEREDITERATOR_HPP_)
#define OBJECTHEAPBUFFEREDITERATOR_HPP_

#include "AddressOrderedListPopulator.hpp"
#include "BumpAllocatedListPopulator.hpp"
#include "EmptyListPopulator.hpp"
#include "MarkedObjectPopulator.hpp"
#include "ObjectHeapBufferedIteratorPopulator.hpp"
#if defined(OMR_GC_SEGREGATED_HEAP)
#include "SegregatedListPopulator.hpp"
#endif /* OMR_GC_SEGREGATED_HEAP */

class GCExtensionsBase;
class MM_HeapRegionDescriptor;
class MM_ObjectHeapBufferedIteratorPopulator;

typedef struct GC_ObjectHeapBufferedIteratorState {
	MM_GCExtensionsBase *extensions; /**< the GC extensions associated with the JVM */
	bool includeDeadObjects;
	bool skipFirstObject;
	uintptr_t data1;
	uintptr_t data2;
	uintptr_t data3;
	uintptr_t data4;
} GC_ObjectHeapBufferedIteratorState;

class GC_ObjectHeapBufferedIterator
{
/* Data Members */
private:
	MM_AddressOrderedListPopulator _addressOrderedListPopulator;
	MM_BumpAllocatedListPopulator _bumpAllocatedListPopulator;
	MM_EmptyListPopulator _emptyListPopulator;
	MM_MarkedObjectPopulator _markedObjectPopulator;
#if defined(OMR_GC_SEGREGATED_HEAP)
	MM_SegregatedListPopulator _segregatedListPopulator;
#endif /* OMR_GC_SEGREGATED_HEAP */
protected:
	enum {
		CACHE_SIZE = 256
	};
	MM_HeapRegionDescriptor *_region;
	GC_ObjectHeapBufferedIteratorState _state;
	omrobjectptr_t _cache[CACHE_SIZE];
	uintptr_t _cacheIndex;
	uintptr_t _cacheCount;
	uintptr_t _cacheSizeToUse;
	const MM_ObjectHeapBufferedIteratorPopulator *_populator;
public:

/* Function Members */
private:
	void init(MM_GCExtensionsBase *extensions, MM_HeapRegionDescriptor *region, void *base, void *top, bool includeDeadObjects, uintptr_t maxElementsToCache);
protected:
	virtual const MM_ObjectHeapBufferedIteratorPopulator *getPopulator();
public:
	GC_ObjectHeapBufferedIterator(MM_GCExtensionsBase *extensions, MM_HeapRegionDescriptor *region, bool includeDeadObjects = false, uintptr_t maxElementsToCache = CACHE_SIZE);
	GC_ObjectHeapBufferedIterator(MM_GCExtensionsBase *extensions, MM_HeapRegionDescriptor *region, void *base, void *top, bool includeDeadObjects = false, uintptr_t maxElementsToCache = CACHE_SIZE);
	omrobjectptr_t nextObject();
	void advance(uintptr_t sizeInBytes);
	void reset(uintptr_t *base, uintptr_t *top);
};
#endif /*OBJECTHEAPBUFFEREDITERATOR_HPP_*/
