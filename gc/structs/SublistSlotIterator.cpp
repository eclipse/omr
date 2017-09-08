/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Structs
 */

#include "omrcfg.h"
#include "omrcomp.h"

#include "SublistSlotIterator.hpp"

#include "SublistPool.hpp"
#include "SublistPuddle.hpp"


/**
 * Return the next sublist slot.
 */
void *
GC_SublistSlotIterator::nextSlot()
{
	void *currentScanPtr;
		
	/* Check if an element was nulled between calls of nextSlot */
	if( _returnedFilledSlot ) {
		uintptr_t *checkNullPtr = _scanPtr;
		
		/* The last element returned is one previous to _scanPtr */
		checkNullPtr--;
		
		if(0 == *checkNullPtr) {
			_removedCount++;
		}
	}	
	
	if(_scanPtr < _puddle->_listCurrent) {		
		if(0 == *_scanPtr) {
			_returnedFilledSlot = false;
		} else {			
			_returnedFilledSlot = true;
		}
				
		currentScanPtr = (void *)_scanPtr;
		_scanPtr++;

		return currentScanPtr;
	}
	/* Update count of pool for slots removed */
	MM_SublistPool* parent = _puddle->getParent();
	parent->decrementCount( _removedCount );
	
	return NULL;
}

/**
 * Remove the current sublist slot, by replacing it with the one
 * from the end of the puddle, and then shrinking the puddle.
 * @note There must be at least 1 element in the puddle.
 * @note Assumes at least 1 slot has been scanned 
 * @todo Does this belong in the SublistPuddle implementation?
 */
void
GC_SublistSlotIterator::removeSlot()
{	
	/* Check if we are removing a non-null slot */
	if( _returnedFilledSlot ) {
		_removedCount++;
	}
	
	_returnedFilledSlot = false;
	
	/* Scan pointer is already one beyond current.  Back the scan pointer up for
	 * the element exchange and so that the slot (with new value) gets processed
	 * again
	 */
	_scanPtr--;
	_puddle->_listCurrent--;
	*_scanPtr = *_puddle->_listCurrent;
	*_puddle->_listCurrent = 0;
}
