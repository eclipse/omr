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

