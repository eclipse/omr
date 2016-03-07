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
