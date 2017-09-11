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


#if !defined(SLOTOBJECT_HPP_)
#define SLOTOBJECT_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "objectdescription.h"

#include "AtomicOperations.hpp"

class GC_SlotObject
{
private:
	volatile fomrobject_t* _slot;		/**< stored slot address (volatile, because in concurrent GC the mutator can change the value in _slot) */
#if defined (OMR_GC_COMPRESSED_POINTERS)
	uintptr_t _compressedPointersShift; /**< the number of bits to shift by when converting between the compressed pointers heap and real heap */
#endif /* OMR_GC_COMPRESSED_POINTERS */

protected:
public:

private:
	/* Inlined version of converting a compressed token to an actual pointer */
	MMINLINE omrobjectptr_t
	convertPointerFromToken(fomrobject_t token)
	{
#if defined (OMR_GC_COMPRESSED_POINTERS)
		return (omrobjectptr_t)((uintptr_t)token << _compressedPointersShift);
#else
		return (omrobjectptr_t)token;
#endif
	}
	/* Inlined version of converting a pointer to a compressed token */
	MMINLINE fomrobject_t
	convertTokenFromPointer(omrobjectptr_t pointer)
	{
#if defined (OMR_GC_COMPRESSED_POINTERS)
		return (fomrobject_t)((uintptr_t)pointer >> _compressedPointersShift);
#else
		return (fomrobject_t)pointer;
#endif
	}

public:
	/**
	 * Read reference from slot
	 * @return address of object slot reference to.
	 */
	MMINLINE omrobjectptr_t readReferenceFromSlot()
	{
		return convertPointerFromToken(*_slot);
	}

	/**
	 * Return slot address. This address must be used as read only
	 * Created for compatibility with existing code
	 * @return slot address
	 */
	MMINLINE fomrobject_t* readAddressFromSlot()
	{
		return (fomrobject_t*)_slot;
	}

	/**
	 * Write reference to slot if it was changed only.
	 * @param reference address of object should be written to slot
	 */
	MMINLINE void writeReferenceToSlot(omrobjectptr_t reference)
	{
		fomrobject_t compressed = convertTokenFromPointer(reference);
		if (compressed != *_slot) {
			*_slot = compressed;
		}
	}

	/**
	 * Atomically replace heap reference. It is accepted to fail - some other thread
	 * might have raced us and put a more up to date value.
	 * @return true if write succeeded
	 */ 	
	MMINLINE bool atomicWriteReferenceToSlot(omrobjectptr_t oldReference, omrobjectptr_t newReference)
	{
		bool swapResult = false;
		
		if (oldReference != newReference) {
			fomrobject_t compressedOld = convertTokenFromPointer(oldReference);
			fomrobject_t compressedNew = convertTokenFromPointer(newReference);
		
#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
			swapResult = ((uint32_t)(uintptr_t)compressedOld == MM_AtomicOperations::lockCompareExchangeU32((uint32_t *)_slot, (uint32_t)(uintptr_t)compressedOld, (uint32_t)(uintptr_t)compressedNew));
#else
			swapResult = ((uintptr_t)compressedOld == MM_AtomicOperations::lockCompareExchange((uintptr_t *)_slot, (uintptr_t)compressedOld, (uintptr_t)compressedNew));
#endif /* OMR_INTERP_COMPRESSED_OBJECT_HEADER */
		}
		
		return swapResult;
	}

	/**
	 *	Update of slot address.
	 *	Must be used by friends only for fast address replacement
	 *	@param slot slot address
	 */
	MMINLINE void writeAddressToSlot(fomrobject_t* slot)
	{
		_slot = slot;
	}

	GC_SlotObject(OMR_VM *omrVM, volatile fomrobject_t* slot)
	: _slot(slot)
#if defined (OMR_GC_COMPRESSED_POINTERS)
	, _compressedPointersShift(omrVM->_compressedPointersShift)
#endif /* OMR_GC_COMPRESSED_POINTERS */
	{}

	GC_SlotObject(void *omrVM, volatile fomrobject_t* slot)
	: _slot(slot)
#if defined (OMR_GC_COMPRESSED_POINTERS)
	, _compressedPointersShift(((OMR_VM *)omrVM)->_compressedPointersShift)
#endif /* OMR_GC_COMPRESSED_POINTERS */
	{}
};
#endif /* SLOTOBJECT_HPP_ */
