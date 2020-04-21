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


#include "ObjectHeapIteratorSegregated.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * @copydoc ObjectHeapIteratorSegregated::nextObject()
 */
omrobjectptr_t
GC_ObjectHeapIteratorSegregated::nextObject()
{
	omrobjectptr_t object = NULL;
	
	switch (_type) {
		case MM_HeapRegionDescriptor::SEGREGATED_SMALL:
			while(_scanPtr < _smallPtrTop) {
				if (!_extensions->objectModel.isDeadObject(_scanPtr)) {
					object = _scanPtr;
					_scanPtr = (omrobjectptr_t) (((uintptr_t)_scanPtr) + _cellSize);
					break;
				} else {
					_scanPtr = (omrobjectptr_t)(((uintptr_t)_scanPtr) + _extensions->objectModel.getSizeInBytesDeadObject(_scanPtr));
					if (_includeDeadObjects) {
						object = _scanPtr;
						break;
					}
				}
			}
			break;
		case MM_HeapRegionDescriptor::SEGREGATED_LARGE:
			if (_scanPtr < _scanPtrTop) {
				object = (omrobjectptr_t)_scanPtr;
				_scanPtr = _scanPtrTop;
			}
			break;
		case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
		case MM_HeapRegionDescriptor::FREE:
		case MM_HeapRegionDescriptor::RESERVED:
			/* ARRAYLET, FREE and RESERVED do not contain any objects */
			break;
		default:
			Assert_MM_unreachable();
			break;
	}
	
	return object;
}

/**
 * @copydoc ObjectHeapIteratorSegregated::nextObjectNoAdvance()
 * 
 */
omrobjectptr_t
GC_ObjectHeapIteratorSegregated::nextObjectNoAdvance()
{
	omrobjectptr_t object = NULL;
		
	switch (_type) {
		case MM_HeapRegionDescriptor::SEGREGATED_SMALL:
			if(!_pastFirstObject) {
				_pastFirstObject = true;
				if(_scanPtr >= _smallPtrTop) {
					return NULL;
				}
				if (!_extensions->objectModel.isDeadObject(_scanPtr) || _includeDeadObjects) {
					return _scanPtr;
				}
			}
			while(_scanPtr < _smallPtrTop) {
				if (!_extensions->objectModel.isDeadObject(_scanPtr)) {
					_scanPtr = (omrobjectptr_t)((uintptr_t)_scanPtr + _cellSize);
				} else {
					_scanPtr = (omrobjectptr_t)((uintptr_t)_scanPtr + _extensions->objectModel.getSizeInBytesDeadObject(_scanPtr));
				}
				if (_scanPtr < _smallPtrTop) {
					if (!_extensions->objectModel.isDeadObject(_scanPtr) || _includeDeadObjects) {
						return _scanPtr;
					}
				}
			}
			break;
		case MM_HeapRegionDescriptor::SEGREGATED_LARGE:
			if (!_pastFirstObject && (_scanPtr < _scanPtrTop)) {
				object = (omrobjectptr_t)_scanPtr;
				_scanPtr = _scanPtrTop;
			}
			break;
		case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
		case MM_HeapRegionDescriptor::FREE:
		case MM_HeapRegionDescriptor::RESERVED:
			/* ARRAYLET, FREE and RESERVED do not contain any objects */
			break;
		default:
			Assert_MM_unreachable();
			break;
	}
	
	return object;
}

/**
 * @copydoc ObjectHeapIteratorSegregated::advance()
 * 
 * The concept of "no advance" doesn't apply to MemoryPoolSegregated,
 * so this method is a no-op.
 */
void 
GC_ObjectHeapIteratorSegregated::advance(uintptr_t size)
{
	_pastFirstObject = false;
	_scanPtr = (omrobjectptr_t)(((uintptr_t)_scanPtr) + size);
}

/**
 * @copydoc ObjectHeapIteratorSegregated::reset()
 */
void 
GC_ObjectHeapIteratorSegregated::reset(uintptr_t *base, uintptr_t *top)
{
	_scanPtr = (omrobjectptr_t)base;
	_scanPtrTop = (omrobjectptr_t)top;
	calculateActualScanPtrTop();
}


/**
 * @copydoc ObjectHeapIteratorSegregated::calculateActualScanPtrTop()
 */
void 
GC_ObjectHeapIteratorSegregated::calculateActualScanPtrTop()
{
	if (MM_HeapRegionDescriptor::SEGREGATED_SMALL == _type) {
		uintptr_t cellCount = ((uintptr_t)_scanPtrTop - (uintptr_t)_scanPtr) / _cellSize;
		uintptr_t actualSize = cellCount * _cellSize;
		_smallPtrTop = (omrobjectptr_t)((uintptr_t)_scanPtr + actualSize);
	} 
}

#endif /* OMR_GC_SEGREGATED_HEAP */
