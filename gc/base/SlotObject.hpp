/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
#if defined (OMR_GC_FULL_POINTERS)
	bool _compressObjectReferences;
#endif /* OMR_GC_FULL_POINTERS */
#endif /* OMR_GC_COMPRESSED_POINTERS */

protected:
public:

private:
	/* Inlined version of converting a pointer to a compressed token */
	MMINLINE fomrobject_t
	convertTokenFromPointer(omrobjectptr_t pointer)
	{
		uintptr_t value = (uintptr_t)pointer;
#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
			value >>= _compressedPointersShift;
		}
#endif /* OMR_GC_COMPRESSED_POINTERS */
		return (fomrobject_t)value;
	}

public:
	/**
	 * Read the value of a slot.
	 *
	 * @param[in] slotPtr the slot address
	 * @param[in] compressed true if object to object references are compressed, false if not
	 * @return the raw contents of the slot (NOT rebased/shifted for compressed references)
	 */
	MMINLINE static fomrobject_t readSlot(fomrobject_t *slotPtr, bool compressed)
	{
		if (compressed) {
			return (fomrobject_t)*(uint32_t*)slotPtr;
		} else {
			return (fomrobject_t)*(uintptr_t*)slotPtr;
		}
	}

	/**
	 * Calculate the difference between two object slot addresses, in slots
	 *
	 * @param[in] p1 the value to be subtracted from
	 * @param[in] p2 the value to be subtracted
	 * @param[in] compressed true if object to object references are compressed, false if not
	 * @return p1 - p2 in slots
	 */
	MMINLINE static intptr_t subtractSlotAddresses(fomrobject_t *p1, fomrobject_t *p2, bool compressed)
	{
		if (compressed) {
			return (uint32_t*)p1 - (uint32_t*)p2;
		} else {
			return (uintptr_t*)p1 - (uintptr_t*)p2;
		}
	}

	/**
	 * Calculate the addition of an integer to an object slot address
	 *
	 * @param[in] base the base slot address
	 * @param[in] index the index to add
	 * @param[in] compressed true if object to object references are compressed, false if not
	 * @return the adjusted address
	 */
	MMINLINE static fomrobject_t *addToSlotAddress(fomrobject_t *base, intptr_t index, bool compressed)
	{
		if (compressed) {
			return (fomrobject_t*)((uint32_t*)base + index);
		} else {
			return (fomrobject_t*)((uintptr_t*)base + index);
		}
	}

	/**
	 * Calculate the subtraction of an integer from an object slot address
	 *
	 * @param[in] base the base slot address
	 * @param[in] index the index to subtract
	 * @param[in] compressed true if object to object references are compressed, false if not
	 * @return the adjusted address
	 */
	MMINLINE static fomrobject_t *subtractFromSlotAddress(fomrobject_t *base, intptr_t index, bool compressed)
	{
		if (compressed) {
			return (fomrobject_t*)((uint32_t*)base - index);
		} else {
			return (fomrobject_t*)((uintptr_t*)base - index);
		}
	}

	/**
	 * Return back true if object references are compressed
	 * @return true, if object references are compressed
	 */
	MMINLINE bool compressObjectReferences() {
#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
		return _compressObjectReferences;
#else /* defined(OMR_GC_FULL_POINTERS) */
		return true;
#endif /* defined(OMR_GC_FULL_POINTERS) */
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */
		return false;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	}

	/**
	 * Read reference from slot
	 * @return address of object slot reference to.
	 */
	MMINLINE omrobjectptr_t readReferenceFromSlot()
	{
		omrobjectptr_t value = NULL;
#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
			value = (omrobjectptr_t)(((uintptr_t)*(uint32_t volatile *)_slot) << _compressedPointersShift);
		} else
#endif /* OMR_GC_COMPRESSED_POINTERS */
		{
			value = (omrobjectptr_t)*(uintptr_t volatile *)_slot;
		}
		return value;
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
#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
			*(uint32_t volatile *)_slot = (uint32_t)((uintptr_t)reference >> _compressedPointersShift);
		} else
#endif /* OMR_GC_COMPRESSED_POINTERS */
		{
			*(uintptr_t volatile *)_slot = (uintptr_t)reference;
		}
	}

	/**
	 * Atomically replace heap reference. It is accepted to fail - some other thread
	 * might have raced us and put a more up to date value.
	 * @return true if write succeeded
	 */ 	
	MMINLINE bool atomicWriteReferenceToSlot(omrobjectptr_t oldReference, omrobjectptr_t newReference)
	{
		/* Caller should ensure oldReference != newReference */
		uintptr_t oldValue = (uintptr_t)oldReference;
		uintptr_t newValue = (uintptr_t)newReference;
		bool swapResult = false;

#if defined (OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
			uint32_t oldCompressed = (uint32_t)(oldValue >> _compressedPointersShift);
			uint32_t newCompressed = (uint32_t)(newValue >> _compressedPointersShift);
			swapResult = (oldCompressed == MM_AtomicOperations::lockCompareExchangeU32((uint32_t volatile *)_slot, oldCompressed, newCompressed));
		} else
#endif /* OMR_GC_COMPRESSED_POINTERS */
		{
			swapResult = (oldValue == MM_AtomicOperations::lockCompareExchange((uintptr_t volatile *)_slot, oldValue, newValue));
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

	/**
	 * Advance the slot address by an integer offset
	 *
	 * @param[in] offset the offset to add
	 */
	MMINLINE void addToSlotAddress(intptr_t offset)
	{
		writeAddressToSlot(addToSlotAddress(readAddressFromSlot(), offset, compressObjectReferences()));
	}

	/**
	 * Back up the slot address by an integer offset
	 *
	 * @param[in] offset the offset to subtract
	 */
	MMINLINE void subtractFromSlotAddress(intptr_t offset)
	{
		writeAddressToSlot(subtractFromSlotAddress(readAddressFromSlot(), offset, compressObjectReferences()));
	}

	GC_SlotObject(OMR_VM *omrVM, volatile fomrobject_t* slot)
	: _slot(slot)
#if defined (OMR_GC_COMPRESSED_POINTERS)
	, _compressedPointersShift(omrVM->_compressedPointersShift)
#if defined (OMR_GC_FULL_POINTERS)
	, _compressObjectReferences(OMRVM_COMPRESS_OBJECT_REFERENCES(omrVM))
#endif /* OMR_GC_FULL_POINTERS */
#endif /* OMR_GC_COMPRESSED_POINTERS */
	{}
};
#endif /* SLOTOBJECT_HPP_ */
