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

#if !defined(OBJECTHEAPITERATORADDRESSORDEREDLIST_HPP_)
#define OBJECTHEAPITERATORADDRESSORDEREDLIST_HPP_

#include "omrcfg.h"

#include "GCExtensionsBase.hpp"
#include "HeapRegionDescriptor.hpp"
#include "ObjectHeapIterator.hpp"
#include "ObjectModel.hpp"

/**
 * Iterate over all objects and non-objects in a memory segment which uses
 * an address-ordered free list.
 *
 * @ingroup GC_Base
 */
class GC_ObjectHeapIteratorAddressOrderedList : public GC_ObjectHeapIterator
{
	/*
	 * Data members
	 */
private:
	bool _includeDeadObjects;

	omrobjectptr_t _scanPtr;
	omrobjectptr_t _scanPtrTop;
	bool _isDeadObject;
	bool _isSingleSlotHole;
	uintptr_t _deadObjectSize;
	bool _pastFirstObject;
	
	MM_GCExtensionsBase *_extensions; /**< The GC extensions associated with the JVM */

protected:
	
public:

	/*
	 * Function members
	 */
private:
	/**
	 * Compute the deadObjectSize for the current _scanPtr
	 */
	uintptr_t computeDeadObjectSize();

	/**
	 * Advances the _scanPtr field by the specified amount
	 *
	 * @parm increment - the number of bytes to advance the scanPtr by
	 */
	void advanceScanPtr(uintptr_t increment);
	
protected:
	
public:
	/**
	 * @param memorySegment the memory segment to be walked, from <code>heapBase</code>
	 * to <code>heapAlloc</code>.
	 * @param includeLiveObjects if true, objects will be included in the walk
	 * @param includeDeadObjects if true, non-objects will be included in the walk
	 */
	GC_ObjectHeapIteratorAddressOrderedList(MM_GCExtensionsBase *extensions, MM_HeapRegionDescriptor *region, bool includeDeadObjects) :
		/* Iterator base values */
		_includeDeadObjects(includeDeadObjects),
		_isDeadObject(false),
		_isSingleSlotHole(false),
		_deadObjectSize(0),
		_pastFirstObject(false),
		_extensions(extensions)
	{
		_scanPtrTop = (omrobjectptr_t)region->getHighAddress();
		/* Iterator scan values */
		_scanPtr = (omrobjectptr_t)region->getLowAddress();
	}

	/**
	 * @param base object or free entry at which to begin the walk
	 * @param top walk is complete when this address is reached
	 * @param includeLiveObjects if true, objects will be included in the walk
	 * @param includeDeadObjects if true, non-objects will be included in the walk
	 */
	GC_ObjectHeapIteratorAddressOrderedList(MM_GCExtensionsBase *extensions, omrobjectptr_t base, omrobjectptr_t top, bool includeDeadObjects, bool skipFirstObject = false) :
		/* Iterator base values */
		_includeDeadObjects(includeDeadObjects),
		_scanPtr(base),
		_scanPtrTop(top),
		/* Iterator scan values */
		_isDeadObject(false),
		_isSingleSlotHole(false),
		_deadObjectSize(0),
		_pastFirstObject(skipFirstObject),
		_extensions(extensions)
	{}

	virtual omrobjectptr_t nextObjectNoAdvance();
	virtual void advance(uintptr_t size);
	virtual void reset(uintptr_t *base, uintptr_t *top);

	/**
	 * @see GC_ObjectHeapIterator::nextObject()
	 */
	MMINLINE virtual omrobjectptr_t nextObject()
	{
		omrobjectptr_t currentObject;

		while(_scanPtr < _scanPtrTop) {
			_isDeadObject = _extensions->objectModel.isDeadObject(_scanPtr);
			currentObject = _scanPtr;
			if(!_isDeadObject) {
				_scanPtr = (omrobjectptr_t) ( ((uintptr_t)_scanPtr) + _extensions->objectModel.getConsumedSizeInBytesWithHeader(_scanPtr) );
				return currentObject;
			} else {
				_isSingleSlotHole = _extensions->objectModel.isSingleSlotDeadObject(_scanPtr);
				if(_isSingleSlotHole) {
					_deadObjectSize = _extensions->objectModel.getSizeInBytesSingleSlotDeadObject(_scanPtr);
				} else {
					_deadObjectSize = _extensions->objectModel.getSizeInBytesMultiSlotDeadObject(_scanPtr);
				}
				_scanPtr = (omrobjectptr_t)( ((uintptr_t)_scanPtr) + _deadObjectSize );
				if(_includeDeadObjects) {
					return currentObject;
				}
			}
		}

		return NULL;
	}

	/**
	 * Returns true if the object is dead
	 */
	MMINLINE bool isDeadObject()
	{
		return _isDeadObject;
	}

	/**
	 * Returns true if the slot hole is a singleton
	 */
	MMINLINE bool isSingleSlotHole() {
		return _isSingleSlotHole;
	}

	/**
	 * Returns the size of the dead object
	 */
	MMINLINE uintptr_t getDeadObjectSize() {
		return _deadObjectSize;
	}
};

#endif /* OBJECTHEAPITERATORADDRESSORDEREDLIST_HPP_ */
