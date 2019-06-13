/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#if !defined(FORWARDEDHEADER_HPP_)
#define FORWARDEDHEADER_HPP_

/* Do not define FORWARDEDHEADER_DEBUG for production builds */
#undef FORWARDEDHEADER_DEBUG

#include "omrcfg.h"
#include "modronbase.h"
#include "objectdescription.h"

#include "HeapLinkedFreeHeader.hpp"


/* Source object header bits */
#define OMR_FORWARDED_TAG 4
/* If 'being copied hint' is set, it hints that destination might still be being copied (although it might have just completed).
   It tells the caller it should go and fetch master info from the destination header to tell if coping is really complete.
   If hint is reset, the copying is definitely complete, no need to fetch the master info.
   This hint is not necessary for correctness of copying protocol, it's just an optimization to avoid visiting destination object header
   in cases when it's likely not in data cash (GC thread encountering already forwarded object) */
#define OMR_BEING_COPIED_HINT 2
#define OMR_SELF_FORWARDED_TAG J9_GC_MULTI_SLOT_HOLE


/* Destination object header bits, masks, consts... */
/* Master being-copied bit is the destination header. If set, object is still being copied,
   and the rest of the header indicate progress info (bytes yet to copy and number of threads participating).
   If the bit is reset, the object is fully copied, and the rest of header is fully restored (class info etc).
 */
#define OMR_BEING_COPIED_TAG OMR_FORWARDED_TAG
/* 
   Shift is exactly bit size of OMR_OBJECT_METADATA_FLAGS_MASK, which is not visible here. 
   Constants here, are however visible in OMR object model and  
   we do assert there that our masks do not overlap with flags mask */
#define OUTSTANDING_COPIES_SHIFT 8
/* 4 bits reserved for outstandingCopy count */
#define OUTSTANDING_COPIES_MASK_BASE 0xf
/* actually limit on outstandingCopy count is conservatively set to a lower number than what is the bit allotment (OUTSTANDING_COPIES_MASK_BASE),
 * since having too many copies in parallel may be counterproductive */
#define MAX_OUTSTANDING_COPIES 4
#define SIZE_ALIGNMENT 0xfffUL
#define REMAINING_SIZE_MASK ~SIZE_ALIGNMENT
#define OUTSTANDING_COPIES_MASK (OUTSTANDING_COPIES_MASK_BASE << OUTSTANDING_COPIES_SHIFT)
#define COPY_PROGRESS_INFO_MASK (REMAINING_SIZE_MASK | OUTSTANDING_COPIES_MASK)
#define SIZE_OF_SECTION_TO_COPY(size) ((size) >> 7)

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

#if defined (OMR_GC_COMPRESSED_POINTERS)
		/* this field must be here to reserve space if slots are 4 bytes long (extend to 8 bytes starting from &MutableHeaderFields.clazz) */
		uint32_t overlap;
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
	};

	omrobjectptr_t _objectPtr;					/**< the object on which to act */
	MutableHeaderFields _preserved; 			/**< a backup copy of the header fields which may be modified by this class */
	const uintptr_t _forwardingSlotOffset;		/**< fomrobject_t offset from _objectPtr to fomrobject_t slot that will hold the forwarding pointer */
	static const uintptr_t _forwardedTag = OMR_FORWARDED_TAG;	/**< bit mask used to mark forwarding slot value as forwarding pointer */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	static const fomrobject_t _selfForwardedTag = (fomrobject_t)(_forwardedTag | OMR_SELF_FORWARDED_TAG);	
	static const fomrobject_t _beingCopiedHint = (fomrobject_t)OMR_BEING_COPIED_HINT; /**< used in source object f/w pointer to hint that object might still be being copied */
	static const fomrobject_t _beingCopiedTag = (fomrobject_t)OMR_BEING_COPIED_TAG; /**< used in destination object, but using the same bit as _forwardedTag in source object */
	static const fomrobject_t _remainingSizeMask = (fomrobject_t)REMAINING_SIZE_MASK; 
	static const fomrobject_t _copyProgressInfoMask = (fomrobject_t)(_remainingSizeMask | OUTSTANDING_COPIES_MASK);
	static const uintptr_t _copySizeAlignement = (uintptr_t)SIZE_ALIGNMENT;
	static const uintptr_t _minIncrement = (131072 & _remainingSizeMask); /**< min size of copy section; does not have to be a power of 2, but it has to be aligned with _copySizeAlignement */ 
	
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
/*
 * Function members
 */
private:
	MMINLINE_DEBUG static fomrobject_t
	lockCompareExchangeObjectHeader(volatile fomrobject_t *address, fomrobject_t oldValue, fomrobject_t newValue)
	{
#if defined(OMR_GC_COMPRESSED_POINTERS)
		return MM_AtomicOperations::lockCompareExchangeU32((volatile uint32_t*)address, oldValue, newValue);
#else /* OMR_GC_COMPRESSED_POINTERS */
		return MM_AtomicOperations::lockCompareExchange((volatile uintptr_t*)address, oldValue, newValue);
#endif /* OMR_GC_COMPRESSED_POINTERS */
	}
	
	/**
	 * Atomically try to win forwarding. It's internal implementation of public setForwardedObject()
	 */
	omrobjectptr_t setForwardedObjectInternal(omrobjectptr_t destinationObjectPtr, uintptr_t forwardedTag);
	
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/**
	 * Try to win a section of large object that is still being copied
	 */
	static uintptr_t winObjectSectionToCopy(volatile fomrobject_t *copyProgressSlot, fomrobject_t oldValue, uintptr_t *remainingSizeToCopy, uintptr_t outstandingCopies);
	
	/**
	 * Just spin (or pause) for certain amount of cycles
	 */
	static void wait(uintptr_t *spinCount);

	/**
	 * Return true, if based on the hint in forwarding header, the object might still be copied
	 */
	MMINLINE bool
	isBeingCopied()
	{
		/* strictly forwarded object with _beingCopiedHint set */
		return (_beingCopiedHint | _forwardedTag) == (_preserved.slot & (_beingCopiedHint | _selfForwardedTag));
	}
	
	/**
	 * Based on progress info in destination object header, try to participate in copying,
	 * wait if all work is done (but still copy is in progress), or simply return if copy is complete)
	 */
	void copyOrWaitOutline(omrobjectptr_t destinationObjectPtr);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
	
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
	MMINLINE omrobjectptr_t setForwardedObject(omrobjectptr_t destinationObjectPtr) {
		return setForwardedObjectInternal(destinationObjectPtr, _forwardedTag);
	}
	
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
		return _selfForwardedTag == (_preserved.slot & _selfForwardedTag);
	}
	
	MMINLINE bool
	isStrictlyForwardedPointer()
	{
		/* only _forwardedTag set ('self forwarded bit' reset) */
		return _forwardedTag == (_preserved.slot & _selfForwardedTag);
	}
	
	omrobjectptr_t setSelfForwardedObject();
	
	void restoreSelfForwardedPointer();
	
	/**
	 * Initial step for destination object fixup - restore object flags and overlap, while still maintaining progess info and being copied bit.
	 */
	MMINLINE void
	commenceFixup(omrobjectptr_t destinationObjectPtr)
	{
		MutableHeaderFields* newHeader = (MutableHeaderFields *)((fomrobject_t *)destinationObjectPtr + _forwardingSlotOffset);
		
		/* copy preserved flags, and keep the rest (which should be all 0s) */
		newHeader->slot = (_preserved.slot & ~_copyProgressInfoMask) | _beingCopiedTag;

#if defined (OMR_GC_COMPRESSED_POINTERS)
		newHeader->overlap = _preserved.overlap;
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
	}	
	
	/**
	 * Final fixup step. Reset copy-in-progress flag and set info (typically class) that overlap with progress info.
	 * This operation must be atomic (single memory update)
	 */ 
	MMINLINE void
	commitFixup(omrobjectptr_t destinationObjectPtr)
	{
		MutableHeaderFields* newHeader = (MutableHeaderFields *)((fomrobject_t *)destinationObjectPtr + _forwardingSlotOffset);

		/* before we announce this copy of the object is available, do a write sync */
		MM_AtomicOperations::storeSync();
		
		/* get flags */
		newHeader->slot = (_preserved.slot & _copyProgressInfoMask) | (newHeader->slot & ~_copyProgressInfoMask & ~(fomrobject_t)_beingCopiedTag);
		
		/* remove the hint in the source object */
		volatile MutableHeaderFields* objectHeader = (volatile MutableHeaderFields *)((fomrobject_t*)_objectPtr + _forwardingSlotOffset);
		objectHeader->slot &= ~_beingCopiedHint;
	}
	
	/**
	 * A variant of setForwardedObject(), that will also set being-copied hint in the forwarding header
	 */
	MMINLINE omrobjectptr_t
	setForwardedObjectWithBeingCopiedHint(omrobjectptr_t destinationObjectPtr)
	{
		return setForwardedObjectInternal(destinationObjectPtr, _forwardedTag | _beingCopiedHint);
	}	
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	/**
	 * Copy intermediate section of the object. Could be done by any thread that is interested in this object
	 */
	void copySection(omrobjectptr_t destinationObjectPtr, uintptr_t remainingSizeToCopy, uintptr_t sizeToCopy);
	
	/** 
	 * Try helping with object copying or if no outstanding work left just wait till copy is complete.
	 * Used by threads that tried to win forwarding, but lost.
	 */
	MMINLINE void 
	copyOrWait(omrobjectptr_t destinationObjectPtr)
	{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		/* Check the hint bit in the forwarding pointer itself, before fetching the master info in the destination object header */ 
		if (isBeingCopied()) {
			copyOrWaitOutline(destinationObjectPtr);
		}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */		
	}	
	
	/**
	 * Try helping with object copying or if no outstandig work left just wait till copy is complete.
	 * Used only by the thread that won forwarding.
	 */
	void copyOrWaitWinner(omrobjectptr_t destinationObjectPtr);
	
	/**
	 * Setup initial copy-progress information
     */
	uintptr_t copySetup(omrobjectptr_t destinationObjectPtr, uintptr_t *remainingSizeToCopy);

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

#if defined (OMR_GC_COMPRESSED_POINTERS)
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
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */

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
#if defined (OMR_GC_COMPRESSED_POINTERS)
		newHeader->overlap = _preserved.overlap;
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
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
		MM_HeapLinkedFreeHeader* freeHeader = MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(_objectPtr);
		return (omrobjectptr_t) freeHeader->getNext();
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
