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
#if defined(OMR_GC_ARRAYLETS)
		case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
#endif /* defined(OMR_GC_ARRAYLETS) */		
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
#if defined(OMR_GC_ARRAYLETS)
		case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
#endif /* defined(OMR_GC_ARRAYLETS) */		
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
