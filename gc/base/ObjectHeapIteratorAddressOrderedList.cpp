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


/**
 * @file
 * @ingroup GC_Base
 */

#include "omrcfg.h"
#include "ModronAssertions.h"

#include "ObjectHeapIteratorAddressOrderedList.hpp"

/**
 * @see GC_ObjectHeapIterator::reset()
 */
void
GC_ObjectHeapIteratorAddressOrderedList::reset(uintptr_t *base, uintptr_t *top) 
{
	_scanPtr = (omrobjectptr_t)base;
	_scanPtrTop = (omrobjectptr_t)top;
}

/*
 * Initialize the _deadObjectSize field for the current _scanPtr
 */
uintptr_t 
GC_ObjectHeapIteratorAddressOrderedList::computeDeadObjectSize()
{
	uintptr_t deadObjectSize = 0;
	
	if(_isSingleSlotHole) {
		deadObjectSize = _extensions->objectModel.getSizeInBytesSingleSlotDeadObject(_scanPtr);
	} else {
		deadObjectSize = _extensions->objectModel.getSizeInBytesMultiSlotDeadObject(_scanPtr);
	}
	
	return deadObjectSize;
}

void 
GC_ObjectHeapIteratorAddressOrderedList::advanceScanPtr(uintptr_t increment)
{
	_scanPtr = (omrobjectptr_t)( ((uintptr_t)_scanPtr) + increment );
}

/**
 * @see GC_ObjectHeapIterator::nextObjectNoAdvance()
 */
omrobjectptr_t
GC_ObjectHeapIteratorAddressOrderedList::nextObjectNoAdvance() {
	if(!_pastFirstObject) {
		_pastFirstObject = true;
		if(_scanPtr >= _scanPtrTop) {
			return NULL;
		}
		_isDeadObject = _extensions->objectModel.isDeadObject(_scanPtr);
		if (_isDeadObject) {
			_isSingleSlotHole = _extensions->objectModel.isSingleSlotDeadObject(_scanPtr);
			_deadObjectSize = computeDeadObjectSize();
		}
		if ((_isDeadObject && _includeDeadObjects) || !_isDeadObject) {
			return _scanPtr;
		}
	}

	while(_scanPtr < _scanPtrTop) {
		/* These flags were set before we returned the last object, but the object might have changed. */
		_isDeadObject = _extensions->objectModel.isDeadObject(_scanPtr);
		_isSingleSlotHole = _isDeadObject ? _extensions->objectModel.isSingleSlotDeadObject(_scanPtr) : false;

		if(!_isDeadObject) {
			_scanPtr = (omrobjectptr_t) ( ((uintptr_t)_scanPtr) + _extensions->objectModel.getConsumedSizeInBytesWithHeader(_scanPtr) );
		} else {
			_deadObjectSize = computeDeadObjectSize();
			advanceScanPtr( _deadObjectSize );
		}

		if (_scanPtr < _scanPtrTop) {
			_isDeadObject = _extensions->objectModel.isDeadObject(_scanPtr);
			if (_isDeadObject) {
				_isSingleSlotHole = _extensions->objectModel.isSingleSlotDeadObject(_scanPtr);
				_deadObjectSize = computeDeadObjectSize();
			}
			if ((_isDeadObject && _includeDeadObjects) || !_isDeadObject) {
				return _scanPtr;
			}
		}
	}

	return NULL;
}

/**
 * @see GC_ObjectHeapIterator::advance()
 */
void
GC_ObjectHeapIteratorAddressOrderedList::advance(uintptr_t size) {
   _pastFirstObject = false;
   advanceScanPtr(size);
}
