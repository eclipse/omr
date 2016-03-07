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

#if !defined(OBJECTHEAPITERATORSEGREGATED_HPP_)
#define OBJECTHEAPITERATORSEGREGATED_HPP_

#include "HeapRegionDescriptor.hpp"
#include "ObjectModel.hpp"

#include "ObjectHeapIterator.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * Object heap iterator for regions which use a segregated free list implementation.
 */
class GC_ObjectHeapIteratorSegregated : public GC_ObjectHeapIterator
{
public:

protected:
	omrobjectptr_t _scanPtr;
	omrobjectptr_t _scanPtrTop;

private:
	MM_HeapRegionDescriptor::RegionType _type;
	uintptr_t _cellSize;
	bool _includeDeadObjects;
	bool _pastFirstObject;
	omrobjectptr_t _smallPtrTop;
	MM_GCExtensionsBase *_extensions;

public:
	virtual omrobjectptr_t nextObject();
	virtual omrobjectptr_t nextObjectNoAdvance();
	virtual void advance(uintptr_t size);
	virtual void reset(uintptr_t *base, uintptr_t *top);

	GC_ObjectHeapIteratorSegregated(MM_GCExtensionsBase *extensions, omrobjectptr_t base, omrobjectptr_t top, MM_HeapRegionDescriptor::RegionType type, uintptr_t cellSize, bool includeDeadObjects, bool skipFirstObject) :
		_scanPtr(base)
		,_scanPtrTop(top)
		,_type(type)
		,_cellSize(cellSize)
		,_includeDeadObjects(includeDeadObjects)
		,_pastFirstObject(skipFirstObject)
		,_smallPtrTop(NULL)
		,_extensions(extensions)
	{
		calculateActualScanPtrTop();
	}

protected:

private:
	void calculateActualScanPtrTop();
};

#endif /* OMR_GC_SEGREGATED_HEAP */

#endif /* OBJECTHEAPITERATORSEGREGATED_HPP_ */

