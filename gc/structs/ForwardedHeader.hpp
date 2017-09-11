/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#if !defined(FORWARDEDHEADER_HPP_)
#define FORWARDEDHEADER_HPP_

/* Do not define FORWARDEDHEADER_DEBUG for production builds */
#undef FORWARDEDHEADER_DEBUG

#include "omrcfg.h"
#include "modronbase.h"
#include "objectdescription.h"

#include "HeapLinkedFreeHeader.hpp"


#if defined(OMR_VALGRIND_MEMCHECK)
#include "MemcheckWrapper.hpp"
#endif /* defined(OMR_VALGRIND_MEMCHECK) */

#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) != defined(OMR_GC_COMPRESSED_POINTERS)
#error "MutableHeaderFields requires sizeof(fomrobject_t) == sizeof(j9objectclass_t)"
#endif /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) != defined(OMR_GC_COMPRESSED_POINTERS) */

#define OMR_FORWARDED_TAG 4

/**
 * Scavenger forwarding header is used to distinguish objects in evacuate space that are being/have been
 * copied into survivor space. Client classes provide an uintptr_t-aligned offset from the head of the
 * forwarded object to the uintptr_t slot that will be used to store the forwarding pointer. A copy of
 * the previous contents of this slot (the forwarding slot) is preserved in the MM_ForwardedHeader instance.
 */
class MM_ForwardedHeader
{
/*
 * Data members
 */
public:
protected:
private:
	/*
	 * First slot in preserved header fields is always aligned to uintptr_t (compressed or not)
	 * so forwarded pointer is stored in uintptr_t word at forwarding slot address and overlaps
	 * next slot for compressed. So, in any case this reads all preserved header fields:
	 *
	 *	*(uintptr_t *)&a.slot = *((uintptr_t *)&b.slot);
	 *
	 *	for MutableHeaderFields a, b.
	 */
	struct MutableHeaderFields {
		/* first slot must be always aligned as for an object slot */
		fomrobject_t slot;

#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
		/* this field must be here to reserve space if slots are 4 bytes long (extend to 8 bytes starting from &MutableHeaderFields.clazz) */
		uint32_t overlap;
#endif /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
	};

	omrobjectptr_t _objectPtr;					/**< the object on which to act */
	MutableHeaderFields _preserved; 			/**< a backup copy of the header fields which may be modified by this class */
	const uintptr_t _forwardingSlotOffset;		/**< fomrobject_t offset from _objectPtr to fomrobject_t slot that will hold the forwarding pointer */
	static const uintptr_t _forwardedTag = OMR_FORWARDED_TAG;	/**< bit mask used to mark forwarding slot value as forwarding pointer */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	static const uintptr_t _selfForwardedTag = _forwardedTag | J9_GC_MULTI_SLOT_HOLE;	
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
/*
 * Function members
 */
public:
#if defined(FORWARDEDHEADER_DEBUG)
#define ForwardedHeaderAssertCondition(condition) #condition
#define ForwardedHeaderAssert(condition) MM_ForwardedHeader::Assert((condition), ForwardedHeaderAssertCondition(((condition))), __FILE__, __LINE__)
	static void Assert(bool condition, const char *assertion, const char *file, uint32_t line);
	void ForwardedHeaderDump(omrobjectptr_t destinationObjectPtr);
#else
#define ForwardedHeaderAssert(condition)
#define ForwardedHeaderDump(destinationObjectPtr)
#endif /* defined(FORWARDEDHEADER_DEBUG) */

	/**
	 * Update this object to be forwarded to destinationObjectPtr using atomic operations.
	 * If the update fails (because the object has already been forwarded), read the forwarded
	 * header and return the forwarded object which was written into the header.
	 *
	 * @param[in] destinationObjectPtr the object to forward to
	 *
	 * @return the winning forwarded object (either destinationObjectPtr or one written by another thread)
	 */
	omrobjectptr_t setForwardedObject(omrobjectptr_t destinationObjectPtr);

	/**
	 * Return the (strictly) forwarded version of the object, or NULL if the object has not been (strictly) forwarded.
	 */
	omrobjectptr_t getForwardedObject();
	
	/**
	 * Get either strict or non-strict forwarded version of the object, or NULL if object is not forwarded at all.
	 * In non Concurrent Scavenger world, this is identical to getForwardedObject()
	 */
	omrobjectptr_t getNonStrictForwardedObject();
	
	/**
	 * @return the object pointer represented by the receiver
	 */
	MMINLINE omrobjectptr_t
	getObject()
	{
		return _objectPtr;
	}

	/**
	 * Determine if the current object is forwarded.
	 *
	 * @return true if the current object is forwarded, false otherwise
	 */
	MMINLINE bool
	isForwardedPointer()
	{
		return _forwardedTag == ((uintptr_t)_preserved.slot & _forwardedTag);
	}
	
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/**
	 * If object is forwarded (isForwardedPointer() returns true) the object can be either
	 * - strictly forwarded (to a remote object, with explicit forwarding pointer)
	 * - self-forwarded (pointing to itself, implicitly just by notion of a special self-forwarding bit in the header .
	 */
	MMINLINE bool
	isSelfForwardedPointer()
	{
		return _selfForwardedTag == ((uintptr_t)_preserved.slot & _selfForwardedTag);
	}
	
	MMINLINE bool
	isStrictlyForwardedPointer()
	{
		return _forwardedTag == ((uintptr_t)_preserved.slot & _selfForwardedTag);
	}

	
	omrobjectptr_t setSelfForwardedObject();
	
	void restoreSelfForwardedPointer();
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	/**
	 * This method will assert if the object has been forwarded. Use isForwardedPointer() to test before calling.
	 *
	 * @return the contents of the preserved slot.
	 *
	 * @see isForwardedPointer()
	 */
	MMINLINE fomrobject_t
	getPreservedSlot()
	{
		ForwardedHeaderAssert(!isForwardedPointer());
		return _preserved.slot;
	}

#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	/**
	 * This method will assert if the object has been forwarded. Use isForwardedPointer() to test before calling.
	 *
	 * @return a pointer to the 32-bit word overlapped word in the 64-bit preserved slot (compressed pointers only).
	 *
	 * @see isForwardedPointer()
	 */
	MMINLINE uint32_t
	getPreservedOverlap()
	{
		ForwardedHeaderAssert(!isForwardedPointer());
		return _preserved.overlap;
	}

	/**
	 * Recover the overlap slot in the original object after forwarding it. This operation is necessary
	 * in order to reverse a forwarding operation during backout.
	 *
	 * @param[in] restoredValue the value to restore in the original object
	 */
	MMINLINE void
	restoreDestroyedOverlap(uint32_t restoredValue)
	{
		((MutableHeaderFields *)((fomrobject_t *)getObject() + _forwardingSlotOffset))->overlap = restoredValue;
	}

	/**
	 * Recover the overlap slot in the original object from the forwarded object after forwarding it.
	 */
	MMINLINE void
	restoreDestroyedOverlap()
	{
		restoreDestroyedOverlap(((MutableHeaderFields *)((fomrobject_t *)getForwardedObject() + _forwardingSlotOffset))->overlap);
	}
#endif /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */

	/**
	 * Update the new version of this object after it has been copied. This undoes any damaged caused
	 * by installing the forwarding pointer into the original prior to the copy. Minimally this will
	 * install the preserved forwarding slot value in the forwarding slot of the copied object. This
	 * will result in the flags from the preserved slot of the original object being copied into the new
	 * object. This method must be called before any updates to the new object flags.
	 *
	 * @param[in] destinationObjectPtr the copied object to be fixed up
	 */
	MMINLINE void
	fixupForwardedObject(omrobjectptr_t destinationObjectPtr)
	{
		MutableHeaderFields* newHeader = (MutableHeaderFields *)((fomrobject_t *)destinationObjectPtr + _forwardingSlotOffset);
		newHeader->slot = _preserved.slot;
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
		newHeader->overlap = _preserved.overlap;
#endif /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
	}

	/**
	 * Determine if the current object is a reverse forwarded object.
	 *
	 * @note reverse forwarded objects are indistinguishable from holes. Only use this
	 * function if you can be sure the object is not a hole.
	 *
	 * @return true if the current object is reverse forwarded, false otherwise
	 */
	MMINLINE bool
	isReverseForwardedPointer()
	{
		return J9_GC_MULTI_SLOT_HOLE == ((uintptr_t)getPreservedSlot() & J9_GC_OBJ_HEAP_HOLE_MASK);
	}

	/**
	 * Get the reverse forwarded pointer for this object.
	 *
	 * This method will assert if the object is not reverse forwarded. Use isReverseForwardedPointer() to test before calling.
	 *
	 * @return the reverse forwarded value
	 */
	MMINLINE omrobjectptr_t
	getReverseForwardedPointer()
	{
		ForwardedHeaderAssert(isReverseForwardedPointer());
		omrobjectptr_t returnPtr;
		MM_HeapLinkedFreeHeader* freeHeader = MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(_objectPtr);
		returnPtr = (omrobjectptr_t) freeHeader->getNext();
#if defined(OMR_VALGRIND_MEMCHECK)		
		valgrindMakeMemDefined((uintptr_t)freeHeader,(uintptr_t)sizeof(MM_HeapLinkedFreeHeader));
#endif /* defined(OMR_VALGRIND_MEMCHECK) */
		return returnPtr;
	}

	/**
	 * Constructor.
	 *
	 * @param[in] objectPtr pointer to the object, which may or may not have been forwarded
	 * @param[in] forwardingSlotOffset fomrobject_t offset to uintptr_t size slot that will hold the forwarding pointer
	 */
	MM_ForwardedHeader(omrobjectptr_t objectPtr)
	: _objectPtr(objectPtr)
	, _forwardingSlotOffset(0)
	{
		/* TODO: Fix the constraint that the object header/forwarding slot offset must be zero. */
		volatile MutableHeaderFields* originalHeader = (volatile MutableHeaderFields *)((fomrobject_t*)_objectPtr + _forwardingSlotOffset);
		*(uintptr_t *)&_preserved.slot = *((uintptr_t *)&originalHeader->slot);
	}

protected:
private:
};

#endif /* FORWARDEDHEADER_HPP_ */
