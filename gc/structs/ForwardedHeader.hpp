/*******************************************************************************
 * Copyright (c) 2015, 2021 IBM Corp. and others
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
   It tells the caller it should go and fetch main info from the destination header to tell if coping is really complete.
   If hint is reset, the copying is definitely complete, no need to fetch the main info.
   This hint is not necessary for correctness of copying protocol, it's just an optimization to avoid visiting destination object header
   in cases when it's likely not in data cash (GC thread encountering already forwarded object) */
#define OMR_BEING_COPIED_HINT 2
#define OMR_SELF_FORWARDED_TAG J9_GC_MULTI_SLOT_HOLE


/* Destination object header bits, masks, consts... */
/* Main being-copied bit is the destination header. If set, object is still being copied,
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
	omrobjectptr_t _objectPtr;					/**< the object on which to act */
	uintptr_t _preserved; 						/**< a backup copy of the header fields which may be modified by this class */
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) && !defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
	bool const _compressObjectReferences;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) && !defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES) */

	static const uintptr_t _forwardedTag = OMR_FORWARDED_TAG;	/**< bit mask used to mark forwarding slot value as forwarding pointer */
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	static const uintptr_t _selfForwardedTag = (uintptr_t)(_forwardedTag | OMR_SELF_FORWARDED_TAG);	
	static const uintptr_t _beingCopiedHint = (uintptr_t)OMR_BEING_COPIED_HINT; /**< used in source object f/w pointer to hint that object might still be being copied */
	static const uintptr_t _beingCopiedTag = (uintptr_t)OMR_BEING_COPIED_TAG; /**< used in destination object, but using the same bit as _forwardedTag in source object */
	static const uintptr_t _remainingSizeMask = (uintptr_t)REMAINING_SIZE_MASK; 
	static const uintptr_t _copyProgressInfoMask = (uintptr_t)(_remainingSizeMask | OUTSTANDING_COPIES_MASK);
	static const uintptr_t _copySizeAlignement = (uintptr_t)SIZE_ALIGNMENT;
	static const uintptr_t _minIncrement = (131072 & _remainingSizeMask); /**< min size of copy section; does not have to be a power of 2, but it has to be aligned with _copySizeAlignement */ 
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
#if defined(OMR_GC_VLHGC)
	/* the grow tag is used by VLHGC and can only be set if the FORWARDED_TAG is already set.  It signifies that the object grew a hash field when moving */
	static const uintptr_t _grow_tag = (uintptr_t)0x2;
	/* combine these flags into one mask which should be stripped from the pointer in order to remove all tags */
	static const uintptr_t _all_tags =  _forwardedTag | _grow_tag;
#endif /* defined(OMR_GC_VLHGC) */

/*
 * Function members
 */
private:
	/**
	 * Return the size of an object to object reference
	 */
	MMINLINE uintptr_t
	referenceSize()
	{
		uintptr_t size = sizeof(uintptr_t);
		if (compressObjectReferences()) {
			size = sizeof(uint32_t);
		}
		return size;
	}

	/**
	 * Fetch the class portion of the preserved data (with any tags).
	 * 
	 * @return the class and tags
	 */
	MMINLINE uintptr_t
	getPreservedClassAndTags()
	{
		uintptr_t result = _preserved;
#if defined(OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
#if defined(OMR_ENV_LITTLE_ENDIAN)
			result &= 0xFFFFFFFF;
#else /* defined(OMR_ENV_LITTLE_ENDIAN) */
			result >>= 32;
#endif /* defined(OMR_ENV_LITTLE_ENDIAN) */
		} 
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		return result;
	}

	/**
	 * Fetch the complete preserved value in memory order.
	 * 
	 * @return the preserved value
	 */
	MMINLINE uintptr_t
	getPreserved()
	{
		return _preserved;
	}

	/**
	 * Fetch the complete preserved value, endian flipping if necessary to ensure
	 * any tag bits appear in the low-order bits.
	 * 
	 * @return the preserved value in canonical format
	 */
	MMINLINE uintptr_t
	getCanonicalPreserved()
	{
		return flipValue(getPreserved());
	}

	/**
	 * Endian flip a 64-bit value if running on compressed big endian.
	 * 
	 * @return the flipped value on compressed big endian, the input value otherwise
	 */
	MMINLINE uintptr_t
	flipValue(uintptr_t value)
	{
#if defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN)
		if (compressObjectReferences()) {
			value = (value >> 32) | (value << 32);
		}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN) */
		return value;
	}

	MMINLINE_DEBUG uintptr_t
	lockCompareExchangeObjectHeader(volatile omrobjectptr_t address, uintptr_t oldValue, uintptr_t newValue)
	{
		uintptr_t result = 0;
		if (compressObjectReferences()) {
			result = MM_AtomicOperations::lockCompareExchangeU32((volatile uint32_t*)address, (uint32_t)oldValue, (uint32_t)newValue);
		} else {
			result = MM_AtomicOperations::lockCompareExchange((volatile uintptr_t*)address, oldValue, newValue);
		}
		return result;
	}

	/**
	 * Write the class slot of an object pointer
	 */
	MMINLINE void
	writeClassSlot(omrobjectptr_t destinationObjectPtr, uintptr_t newValue)
	{
		if (compressObjectReferences()) {
			*(uint32_t*)destinationObjectPtr = (uint32_t)newValue;
		} else {
			*(uintptr_t*)destinationObjectPtr = newValue;
		}
	}

	/**
	 * Read the class slot from an object pointer
	 * @return the slot value, zero-extended to uintptr_t (for compressed refs)
	 */
	MMINLINE uintptr_t
	readClassSlot(omrobjectptr_t destinationObjectPtr)
	{
		uintptr_t value = 0;
		if (compressObjectReferences()) {
			value = *(volatile uint32_t*)destinationObjectPtr;
		} else {
			value = *(volatile uintptr_t*)destinationObjectPtr;
		}
		return value;
	}

	/**
	 * Atomically try to win forwarding. It's internal implementation of public setForwardedObject()
	 */
	omrobjectptr_t setForwardedObjectInternal(omrobjectptr_t destinationObjectPtr, uintptr_t forwardedTag);
	
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	/**
	 * Try to win a section of large object that is still being copied
	 */
	 uintptr_t winObjectSectionToCopy(volatile omrobjectptr_t copyProgressSlot, uintptr_t oldValue, uintptr_t *remainingSizeToCopy, uintptr_t outstandingCopies);
	
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
		return (_beingCopiedHint | _forwardedTag) == (getPreservedClassAndTags() & (_beingCopiedHint | _selfForwardedTag));
	}
	
	/**
	 * Based on progress info in destination object header, try to participate in copying,
	 * wait if all work is done (but still copy is in progress), or simply return if copy is complete)
	 */
	void copyOrWaitOutline(omrobjectptr_t destinationObjectPtr);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

#if defined(OMR_GC_VLHGC)
	/**
	 * Return the forwarded version of the object
	 */
	MMINLINE omrobjectptr_t
	getForwardedObjectVLHGCNoCheck()
	{
		uintptr_t forwardedObject = _preserved;
#if defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN)
		forwardedObject = flipValue(forwardedObject);
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN) */
		return (omrobjectptr_t)(forwardedObject & ~_all_tags);
	}
#endif /* defined(OMR_GC_VLHGC) */
	
public:
	/**
	 * Return back true if object references are compressed
	 * @return true, if object references are compressed
	 */
	MMINLINE bool compressObjectReferences() {
#if defined(OMR_GC_COMPRESSED_POINTERS)
#if defined(OMR_GC_FULL_POINTERS)
#if defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
		return (bool)OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES;
#else /* defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES) */
		return _compressObjectReferences;
#endif /* defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES) */
#else /* defined(OMR_GC_FULL_POINTERS) */
		return true;
#endif /* defined(OMR_GC_FULL_POINTERS) */
#else /* defined(OMR_GC_COMPRESSED_POINTERS) */
		return false;
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
	}

#if defined(FORWARDEDHEADER_DEBUG)
#define ForwardedHeaderAssertCondition(condition) #condition
#define ForwardedHeaderAssert(condition) MM_ForwardedHeader::Assert((condition), ForwardedHeaderAssertCondition(((condition))), __FILE__, __LINE__)
	static void Assert(bool condition, const char *assertion, const char *file, uint32_t line);
	void ForwardedHeaderDump(omrobjectptr_t destinationObjectPtr);
#else
#define ForwardedHeaderAssert(condition)
#define ForwardedHeaderDump(destinationObjectPtr)
#endif /* defined(FORWARDEDHEADER_DEBUG) */

#if defined(OMR_GC_VLHGC)
	/**
	 * Similar to setForwardedObject but optionally sets the "_grow_tag" on the forwarding pointer so that we know the object
	 * grew a hashcode slot when it moved.
	 * @param destinationObjectPtr[in] The new location of the object
	 * @param isObjectGrowing[in] True if the object grew during the move
	 * @return The location of the new copy of the object (will be destinationObjectPtr if the calling thread succeeded in the copy)
	 */
	omrobjectptr_t setForwardedObjectGrowing(omrobjectptr_t destinationObjectPtr, bool isObjectGrowing);

	/**
	 * @return True if the underlying object (original location) resized when it was copied to its new location
	 */
	bool didObjectGrowOnCopy();

	/**
	 * If the object has been forwarded, return the forwarded version of the
	 * object, otherwise return NULL.
	 *
	 * @note This function can safely be called for a free list entry. It will
	 * return NULL in that case.
	 *
	 * @return the forwarded version of this object or NULL
	 */
	MMINLINE omrobjectptr_t
	getForwardedObjectVLHGC()
	{
		if (isForwardedPointer()) {
			return getForwardedObjectVLHGCNoCheck();
		} else {
			return NULL;
		}
	}
#endif /* defined(OMR_GC_VLHGC) */

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
		return _forwardedTag == (getPreservedClassAndTags() & _forwardedTag);
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
		return _selfForwardedTag == (getPreservedClassAndTags() & _selfForwardedTag);
	}
	
	MMINLINE bool
	isStrictlyForwardedPointer()
	{
		/* only _forwardedTag set ('self forwarded bit' reset) */
		return _forwardedTag == (getPreservedClassAndTags() & _selfForwardedTag);
	}
	
	omrobjectptr_t setSelfForwardedObject();
	
	void restoreSelfForwardedPointer();
	
	/**
	 * Initial step for destination object fixup - restore object flags and overlap, while still maintaining progress info and being copied bit.
	 */
	MMINLINE void
	commenceFixup(omrobjectptr_t destinationObjectPtr)
	{
		uintptr_t mask = ~_copyProgressInfoMask;
#if defined(OMR_GC_COMPRESSED_POINTERS)
		if (compressObjectReferences()) {
			/* _copyProgressInfoMask has the high 32 bits set, so they will be 0 in mask.
			 * Update mask to not remove the overlap.
			 */
			mask |= 0xFFFFFFFF00000000;
		}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) */
		*(uintptr_t*)destinationObjectPtr = flipValue((getCanonicalPreserved() & mask) | _beingCopiedTag);
	}	
	
	/**
	 * Final fixup step. Reset copy-in-progress flag and set info (typically class) that overlap with progress info.
	 * This operation must be atomic (single memory update)
	 */ 
	MMINLINE void
	commitFixup(omrobjectptr_t destinationObjectPtr)
	{
		/* before we announce this copy of the object is available, do a write sync */
		MM_AtomicOperations::storeSync();

		/* get flags */
		uintptr_t newValue = (getPreservedClassAndTags() & _copyProgressInfoMask) | (readClassSlot(destinationObjectPtr) & ~(_copyProgressInfoMask | _beingCopiedTag));
		writeClassSlot(destinationObjectPtr, newValue);

		/* remove the hint in the source object */
		newValue = readClassSlot(getObject()) & ~_beingCopiedHint;
		writeClassSlot(getObject(), newValue);
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
		/* Check the hint bit in the forwarding pointer itself, before fetching the main info in the destination object header */ 
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
	MMINLINE uintptr_t
	getPreservedSlot()
	{
		ForwardedHeaderAssert(!isForwardedPointer());
		return getPreservedClassAndTags();
	}

#if defined (OMR_GC_COMPRESSED_POINTERS)
	/**
	 * Fetch the overlap portion of the preserved data (with any tags).
	 * 
	 * @return the class and tags
	 */
	MMINLINE uint32_t
	getPreservedOverlapNoCheck()
	{
		ForwardedHeaderAssert(compressObjectReferences());
		uintptr_t result = _preserved;
#if defined(OMR_ENV_LITTLE_ENDIAN)
		result >>= 32;
#else /* defined(OMR_ENV_LITTLE_ENDIAN) */
		result &= 0xFFFFFFFF;
#endif /* defined(OMR_ENV_LITTLE_ENDIAN) */
		return (uint32_t)result;
	}

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
		return getPreservedOverlapNoCheck();
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
		ForwardedHeaderAssert(compressObjectReferences());
		uint32_t *header = (uint32_t*)getObject();
		header[1] = restoredValue;
	}

	/**
	 * Recover the overlap slot in the original object from the forwarded object after forwarding it.
	 */
	MMINLINE void
	restoreDestroyedOverlap()
	{
		uint32_t *header = (uint32_t*)getForwardedObject();
		restoreDestroyedOverlap(header[1]);
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
		*(uintptr_t*)destinationObjectPtr = getPreserved();
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
		return J9_GC_MULTI_SLOT_HOLE == (getPreservedSlot() & J9_GC_OBJ_HEAP_HOLE_MASK);
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
		return (omrobjectptr_t) freeHeader->getNext(compressObjectReferences());
	}

	/**
	 * Constructor.
	 *
	 * @param[in] objectPtr pointer to the object, which may or may not have been forwarded
	 * @param[in] compressed bool describing whether object references are compressed or not
	 */
	MM_ForwardedHeader(omrobjectptr_t objectPtr, bool compressed)
	: _objectPtr(objectPtr)
	, _preserved(*(volatile uintptr_t *)_objectPtr)
#if defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) && !defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES)
	, _compressObjectReferences(compressed)
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && defined(OMR_GC_FULL_POINTERS) && !defined(OMR_OVERRIDE_COMPRESS_OBJECT_REFERENCES) */
	{
	}

protected:
private:
};

#endif /* FORWARDEDHEADER_HPP_ */
