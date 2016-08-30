/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#if !defined(OBJECTMODEL_HPP_)
#define OBJECTMODEL_HPP_

/*
 * @ddr_namespace: default
 */

#include "ModronAssertions.h"
#include "modronbase.h"
#include "objectdescription.h"
#include "omrgcconsts.h"

#include "Bits.hpp"
#include "ForwardedHeader.hpp"
#include "HeapLinkedFreeHeader.hpp"

class MM_GCExtensionsBase;

#define J9_GC_OBJECT_ALIGNMENT_IN_BYTES 0x8
#define J9_GC_MINIMUM_OBJECT_SIZE 0x10

/*
 * Define structure of object slot that is to be used to represent an object's metadata. In this slot, one byte
 * must be reserved to hold flags and object age (4 bits age, 4 bits flags). The remaining bytes in this slot may
 * be used by the client language for other purposes and will not be altered by OMR.
 */
#define OMR_OBJECT_METADATA_SLOT_OFFSET		0 /* fomrobject_t offset from object header address to metadata slot */
#define OMR_OBJECT_METADATA_SLOT_EA(object)	((fomrobject_t*)(object) + OMR_OBJECT_METADATA_SLOT_OFFSET) /* fomrobject_t* pointer to metadata slot */
#define OMR_OBJECT_METADATA_SIZE_SHIFT		((uintptr_t) 8)
#define OMR_OBJECT_METADATA_FLAGS_MASK		((uintptr_t) 0xFF)
#define OMR_OBJECT_METADATA_AGE_MASK		((uintptr_t) 0xF0)
#define OMR_OBJECT_METADATA_AGE_SHIFT		((uintptr_t) 4)
#define OMR_OBJECT_AGE(object)				((*(OMR_OBJECT_METADATA_SLOT_EA(object)) & OMR_OBJECT_METADATA_AGE_MASK) >> OMR_OBJECT_METADATA_AGE_SHIFT)
#define OMR_OBJECT_FLAGS(object)			(*(OMR_OBJECT_METADATA_SLOT_EA(object)) & OMR_OBJECT_METADATA_FLAGS_MASK)
#define OMR_OBJECT_SIZE(object)				(*(OMR_OBJECT_METADATA_SLOT_EA(object)) >> OMR_OBJECT_METADATA_SIZE_SHIFT)

#define OMR_OBJECT_METADATA_REMEMBERED_BITS			OMR_OBJECT_METADATA_AGE_MASK
#define OMR_OBJECT_METADATA_REMEMBERED_BITS_TO_SET	0x10 /* OBJECT_HEADER_LOWEST_REMEMBERED */
#define OMR_OBJECT_METADATA_REMEMBERED_BITS_SHIFT	OMR_OBJECT_METADATA_AGE_SHIFT

#define STATE_NOT_REMEMBERED  	0
#define STATE_REMEMBERED		(OMR_OBJECT_METADATA_REMEMBERED_BITS_TO_SET & OMR_OBJECT_METADATA_REMEMBERED_BITS)

#define OMR_TENURED_STACK_OBJECT_RECENTLY_REFERENCED	(STATE_REMEMBERED + (1 << OMR_OBJECT_METADATA_REMEMBERED_BITS_SHIFT))
#define OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED	(STATE_REMEMBERED + (2 << OMR_OBJECT_METADATA_REMEMBERED_BITS_SHIFT))

/**
 * Provides information for a given object.
 * @ingroup GC_Base
 */
class GC_ObjectModel
{
/*
 * Member data and types
 */
private:
	uintptr_t _objectAlignmentInBytes; /**< Cached copy of object alignment for getting object alignment for adjusting for alignment */
	uintptr_t _objectAlignmentShift; /**< Cached copy of object alignment shift, must be log2(_objectAlignmentInBytes)  */

protected:
public:

/*
 * Member functions
 */
private:
protected:
public:
	/**
	 * Initialize the receiver, a new instance of GC_ObjectModel
	 *
	 * @return true on success, false on failure
	 */
	bool
	initialize(MM_GCExtensionsBase *extensions)
	{
		return true;
	}

	void tearDown(MM_GCExtensionsBase *extensions) {}

	MMINLINE uintptr_t
	adjustSizeInBytes(uintptr_t sizeInBytes)
	{
		sizeInBytes =  (sizeInBytes + (_objectAlignmentInBytes - 1)) & (uintptr_t)~(_objectAlignmentInBytes - 1);

#if defined(OMR_GC_MINIMUM_OBJECT_SIZE)
		if (sizeInBytes < J9_GC_MINIMUM_OBJECT_SIZE) {
			sizeInBytes = J9_GC_MINIMUM_OBJECT_SIZE;
		}
#endif /* OMR_GC_MINIMUM_OBJECT_SIZE */

		return sizeInBytes;
	}

	/**
	 * Returns TRUE if an object is indexable, FALSE otherwise.
	 *
	 * @param objectPtr pointer to the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool
	isIndexable(omrobjectptr_t objectPtr)
	{
		return false;
	}

	/**
	 * Set flags where ever they are
	 * @param objectPtr Pointer to an object
	 * @param bitsToClear mask to clear bits
	 * @param bitsToSet mask to set bits
	 */
	MMINLINE void
	setFlags(omrobjectptr_t objectPtr, uintptr_t bitsToClear, uintptr_t bitsToSet)
	{
		fomrobject_t* flagsPtr = (fomrobject_t*)OMR_OBJECT_METADATA_SLOT_EA(objectPtr);
		fomrobject_t clear = (fomrobject_t)bitsToClear;
		fomrobject_t set = (fomrobject_t)bitsToSet;

		Assert_MM_true(clear <= OMR_OBJECT_METADATA_FLAGS_MASK);
		Assert_MM_true(set <= OMR_OBJECT_METADATA_FLAGS_MASK);

		*flagsPtr = (*flagsPtr & ~clear) | set;
	}

	/**
	 * Returns TRUE if an object is dead, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is dead, FALSE otherwise
	 */
	MMINLINE bool
	isDeadObject(void *objectPtr)
	{
		return J9_GC_OBJ_HEAP_HOLE == (OMR_OBJECT_FLAGS(objectPtr) & J9_GC_OBJ_HEAP_HOLE);
	}

	/**
	 * Returns TRUE if an object is a dead single slot object, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is a dead single slot object, FALSE otherwise
	 */
	MMINLINE bool
	isSingleSlotDeadObject(omrobjectptr_t objectPtr)
	{
		return J9_GC_SINGLE_SLOT_HOLE == (OMR_OBJECT_FLAGS(objectPtr) & J9_GC_OBJ_HEAP_HOLE_MASK);
	}

	/**
	 * Returns the size, in bytes, of a single slot dead object.
	 * @param objectPtr Pointer to an object
	 * @return The size, in bytes, of a single slot dead object
	 */
	MMINLINE uintptr_t
	getSizeInBytesSingleSlotDeadObject(omrobjectptr_t objectPtr)
	{
		return sizeof(uintptr_t);
	}

	/**
	 * Returns the size, in bytes, of a multi-slot dead object.
	 * @param objectPtr Pointer to an object
	 * @return The size, in bytes, of a multi-slot dead object
	 */
	MMINLINE uintptr_t
	getSizeInBytesMultiSlotDeadObject(omrobjectptr_t objectPtr)
	{
		return MM_HeapLinkedFreeHeader::getHeapLinkedFreeHeader(objectPtr)->getSize();
	}

	/**
	 * Returns the size in bytes of a dead object.
	 * @param objectPtr Pointer to an object
	 * @return The size in byts of a dead object
	 */
	MMINLINE uintptr_t
	getSizeInBytesDeadObject(omrobjectptr_t objectPtr)
	{
		if(isSingleSlotDeadObject(objectPtr)) {
			return getSizeInBytesSingleSlotDeadObject(objectPtr);
		}
		return getSizeInBytesMultiSlotDeadObject(objectPtr);
	}

	MMINLINE uintptr_t
	getConsumedSizeInSlotsWithHeader(omrobjectptr_t objectPtr)
	{
		return MM_Bits::convertBytesToSlots(getConsumedSizeInBytesWithHeader(objectPtr));
	}

	MMINLINE uintptr_t
	getConsumedSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
		return getSizeInBytesWithHeader(objectPtr);
	}

	MMINLINE uintptr_t
	getConsumedSizeInBytesWithHeaderForMove(omrobjectptr_t objectPtr)
	{
		return getConsumedSizeInBytesWithHeader(objectPtr);
	}

	MMINLINE uintptr_t
	getSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
		return OMR_OBJECT_SIZE(objectPtr);
	}

	MMINLINE void
	setObjectSize(omrobjectptr_t objectPtr, uintptr_t size, bool preserveAgeAndFlags = true)
	{
		fomrobject_t *header = OMR_OBJECT_METADATA_SLOT_EA(objectPtr);
		fomrobject_t ageAndFlags = preserveAgeAndFlags ? OMR_OBJECT_FLAGS(objectPtr) : 0;
		*header = ((fomrobject_t)size << OMR_OBJECT_METADATA_SIZE_SHIFT) | ageAndFlags;
	}

	MMINLINE void
	preMove(OMR_VMThread* vmThread, omrobjectptr_t objectPtr)
	{
		/* do nothing */
	}

	MMINLINE void
	postMove(OMR_VMThread* vmThread, omrobjectptr_t objectPtr)
	{
		/* do nothing */
	}

	MMINLINE void
	initializeObject(omrobjectptr_t objectPtr, uintptr_t sizeInBytesWithHeader)
	{
		setObjectSize(objectPtr, sizeInBytesWithHeader, false);
	}

#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Return true if the object holds references to heap objects not reachable from reference graph (eg, heap object bound to associated class)
	 * @param vmThread Pointer to requesting thread
	 * @param objectPtr Pointer to an object
	 * @return true if object holds indirect references to heap objects that may be in new space
	 */
	MMINLINE bool
	hasIndirectObjectReferents(void *vmThread, omrobjectptr_t objectPtr)
	{
		return false;
	}

	/**
	 * Returns the real age of an object
	 * @param objectPtr Pointer to an object
	 * @return The numeric age of the object
	 */
	MMINLINE uint32_t
	getRealAge(omrobjectptr_t objectPtr)
	{
		return OMR_OBJECT_AGE(objectPtr);
	}

	/**
	 * Set object currently referenced
	 * @param objectPtr Pointer to an object
	 */
	MMINLINE void
	setObjectCurrentlyReferenced(omrobjectptr_t objectPtr)
	{
		setRememberedBits(objectPtr, OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED);
	}

	/**
	 * Set object currently referenced atomically
	 * @param objectPtr Pointer to an object
	 * @return true if OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED bit has been set this call
	 */
	MMINLINE bool
	atomicSetObjectCurrentlyReferenced(omrobjectptr_t objectPtr)
	{
		bool result = true;

		volatile fomrobject_t* flagsPtr = (fomrobject_t*) OMR_OBJECT_METADATA_SLOT_EA(objectPtr);
		fomrobject_t oldFlags;
		fomrobject_t newFlags;

		do {
			oldFlags = *flagsPtr;
			if ((oldFlags & OMR_OBJECT_METADATA_REMEMBERED_BITS) == (fomrobject_t)OMR_TENURED_STACK_OBJECT_RECENTLY_REFERENCED) {
				newFlags = (oldFlags & ~OMR_OBJECT_METADATA_REMEMBERED_BITS) | (fomrobject_t)OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED;
			} else {
				result = false;
				break;
			}
		}
#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
		while (oldFlags != MM_AtomicOperations::lockCompareExchangeU32(flagsPtr, oldFlags, newFlags));
#else /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
		while (oldFlags != MM_AtomicOperations::lockCompareExchange(flagsPtr, (uintptr_t)oldFlags, (uintptr_t)newFlags));
#endif /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */

		return result;
	}

	/**
	 * Set object currently referenced and remembered atomically
	 * @param objectPtr Pointer to an object
	 * @return true if remembered bit has been set this call
	 */
	MMINLINE bool
	atomicSetObjectCurrentlyReferencedAndRemembered(omrobjectptr_t objectPtr)
	{
		bool result = true;

		volatile fomrobject_t* flagsPtr = OMR_OBJECT_METADATA_SLOT_EA(objectPtr);
		fomrobject_t oldFlags;
		fomrobject_t newFlags;

		do {
			oldFlags = *flagsPtr;
			newFlags = (oldFlags & ~OMR_OBJECT_METADATA_REMEMBERED_BITS) | OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED;

			if (newFlags == oldFlags) {
				result = false;
				break;
			}
		}
#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
		while (oldFlags != MM_AtomicOperations::lockCompareExchangeU32(flagsPtr, oldFlags, newFlags));
#else /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
		while (oldFlags != MM_AtomicOperations::lockCompareExchange(flagsPtr, (uintptr_t)oldFlags, (uintptr_t)newFlags));
#endif /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */

		if (result && ((oldFlags & OMR_OBJECT_METADATA_REMEMBERED_BITS) >= STATE_REMEMBERED)) {
			result = false;
		}

		return result;
	}

	/**
	 * Set Remembered state to bits atomically
	 * Set Remembered bit atomically as well if it still supported
	 * @param objectPtr Pointer to an object
	 * @return true, if Remembered bit has been set this call
	 */
	MMINLINE bool
	atomicSetRemembered(omrobjectptr_t objectPtr)
	{
		bool result = true;

		volatile fomrobject_t* flagsPtr = (fomrobject_t*) OMR_OBJECT_METADATA_SLOT_EA(objectPtr);
		fomrobject_t oldFlags;
		fomrobject_t newFlags;

		do {
			oldFlags = *flagsPtr;
			if((oldFlags & OMR_OBJECT_METADATA_REMEMBERED_BITS) >= STATE_REMEMBERED) {
				/* Remembered state in age was set by somebody else */
				result = false;
				break;
			}
			newFlags = (oldFlags & ~OMR_OBJECT_METADATA_REMEMBERED_BITS) | STATE_REMEMBERED;
		}
#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
		while (oldFlags != MM_AtomicOperations::lockCompareExchangeU32(flagsPtr, oldFlags, newFlags));
#else /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
		while (oldFlags != MM_AtomicOperations::lockCompareExchange(flagsPtr, (uintptr_t)oldFlags, (uintptr_t)newFlags));
#endif /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */

		return result;
	}

	/**
	 * Extract the flag bits from an unforwarded object.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return the flag bits from the object encapsulated by forwardedHeader
	 * @see MM_ForwardingHeader::isForwardedObject()
	 */
	MMINLINE uintptr_t
	getPreservedFlags(MM_ForwardedHeader *forwardedHeader)
	{
		return (uintptr_t)(forwardedHeader->getPreservedSlot()) & OMR_OBJECT_METADATA_FLAGS_MASK;
	}

	/**
	 * Extract the age bits from an unforwarded object.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return the age bits from the object encapsulated by forwardedHeader
	 * @see MM_ForwardingHeader::isForwardedObject()
	 */
	MMINLINE uintptr_t
	getPreservedAge(MM_ForwardedHeader *forwardedHeader)
	{
		return ((uintptr_t)(forwardedHeader->getPreservedSlot()) & OMR_OBJECT_METADATA_AGE_MASK) >> OMR_OBJECT_METADATA_AGE_SHIFT;
	}

	/**
	 * Extract the size from an unforwarded object.
	 *
	 * This method will assert if the object is not indexable or has been marked as forwarded.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return the size (#elements) of the array encapsulated by forwardedHeader
	 * @see MM_ForwardingHeader::isForwardedObject()
	 */
	MMINLINE uint32_t
	getPreservedIndexableSize(MM_ForwardedHeader *forwardedHeader)
	{
		Assert_MM_unimplemented();
		return 0;
	}

	/**
	 * Returns TRUE if an unforwarded object has been moved after being hashed, FALSE otherwise.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return TRUE if an object has been moved after being hashed, FALSE otherwise
	 */
	MMINLINE bool
	hasBeenMoved(MM_ForwardedHeader *forwardedHeader)
	{
		return false;
	}

	/**
	 * Returns TRUE if an object has been hashed, FALSE otherwise.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return TRUE if an object has been hashed, FALSE otherwise
	 */
	MMINLINE bool
	hasBeenHashed(MM_ForwardedHeader *forwardedHeader)
	{
		return false;
	}

	/**
	 * Determine if the current object has been hashed
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @return true if object has been hashed
	 */
	MMINLINE bool
	isHasBeenHashedBitSet(MM_ForwardedHeader *forwardedHeader)
	{
		return false;
	}

	/**
	 * Returns TRUE if an unforwarded object has been moved after being hashed, FALSE otherwise.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return TRUE if an object has been moved after being hashed, FALSE otherwise
	 */
	MMINLINE bool
	hasBeenRecentlyMoved(MM_ForwardedHeader *forwardedHeader)
	{
		return false;
	}

	/**
	 * Returns TRUE if an unforwarded object's has been hashed bit is set, FALSE otherwise.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return TRUE if an object's has been hashed bit set, FALSE otherwise
	 */
	MMINLINE bool
	isObjectJustHasBeenMoved(MM_ForwardedHeader *forwardedHeader)
	{
		return false;
	}

	/**
	 * Returns TRUE if an object's has been hashed bit is set, FALSE otherwise.
	 *
	 * @param objeectPtr pointer to the object
	 * @return TRUE if an object's has been hashed bit set, FALSE otherwise
	 */
	MMINLINE bool
	isObjectJustHasBeenMoved(omrobjectptr_t objectPtr)
	{
		return false;
	}

	/**
	 * Returns TRUE if an unforwarded object is indexable, FALSE otherwise.
	 *
	 * This method will assert if the object has been marked as forwarded.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool
	isIndexable(MM_ForwardedHeader *forwardedHeader)
	{
		return false;
	}

	/**
	 * Calculate the actual object size and the size adjusted to object alignment.
	 *
	 * This method will assert if the object has been marked as forwarded. The calculated object size
	 * includes expansion slot for hash code if the object has been hashed or moved.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[out] objectCopySizeInBytes actual object size
	 * @param[out] objectReserveSizeInBytes size adjusted to object alignment
	 * @param[out] hotFieldAlignmentDescriptor hot field alignment for class
	 * @return TRUE if an object's has been hashed bit set, FALSE otherwise
	 */
	MMINLINE void
	calculateObjectDetailsForCopy(MM_ForwardedHeader *forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *objectReserveSizeInBytes, uintptr_t *hotFieldAlignmentDescriptor)
	{
		*objectCopySizeInBytes = forwardedHeader->getPreservedSlot() >> OMR_OBJECT_METADATA_SIZE_SHIFT;
		*objectReserveSizeInBytes = adjustSizeInBytes(*objectCopySizeInBytes);
		*hotFieldAlignmentDescriptor = 0;
	}

	/**
	 * Update the new version of this object after it has been copied. This undoes any damaged
	 * caused by installing the forwarding pointer into the original prior to the copy. This will
	 * install the correct (i.e. unforwarded) class pointer, update the hashed/moved flags and
	 * install the hash code if the object has been hashed but not previously moved.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[in] destinationObjectPtr pointer to the copied object to be fixed up
	 * @param[in] objectAge the age to set in the copied object
	 */
	MMINLINE void
	fixupForwardedObject(MM_ForwardedHeader *forwardedHeader, omrobjectptr_t destinationObjectPtr, uintptr_t objectAge)
	{
		/* Copy the preserved fields from the forwarded header into the destination object */
		forwardedHeader->fixupForwardedObject(destinationObjectPtr);

		uintptr_t age = objectAge << OMR_OBJECT_METADATA_AGE_SHIFT;
		setFlags(destinationObjectPtr, OMR_OBJECT_METADATA_AGE_MASK, age);
	}

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/**
	 * Set collector bits to the object's header
	 * @param objectPtr Pointer to an object
	 * @param bits bits to set
	 */
	MMINLINE void
	setRememberedBits(omrobjectptr_t objectPtr, uintptr_t bits)
	{
		setFlags(objectPtr, OMR_OBJECT_METADATA_REMEMBERED_BITS, bits);
	}

	/**
	 * Returns the collector bits from object's header.
	 * @param objectPtr Pointer to an object
	 * @return collector bits
	 */
	MMINLINE uintptr_t
	getRememberedBits(omrobjectptr_t objectPtr)
	{
		return OMR_OBJECT_FLAGS(objectPtr) & OMR_OBJECT_METADATA_REMEMBERED_BITS;
	}

	/**
	 * Returns TRUE if an object is remembered, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is remembered, FALSE otherwise
	 */
	MMINLINE bool
	isRemembered(omrobjectptr_t objectPtr)
	{
		return (getRememberedBits(objectPtr) >= STATE_REMEMBERED);
	}

	/**
	 * Clear Remembered bit in object flags
	 * @param objectPtr Pointer to an object
	 */
	MMINLINE void
	clearRemembered(omrobjectptr_t objectPtr)
	{
		setRememberedBits(objectPtr, STATE_NOT_REMEMBERED);
	}

 	/**
	 * Set run-time Object Alignment in the heap value
	 * Function exists because we can only determine it is way after ObjectModel is init
	 */
	MMINLINE void
	setObjectAlignmentInBytes(uintptr_t objectAlignmentInBytes)
	{
		_objectAlignmentInBytes = objectAlignmentInBytes;
	}

 	/**
	 * Set run-time Object Alignment Shift value
	 * Function exists because we can only determine it is way after ObjectModel is init
	 */
	MMINLINE void
	setObjectAlignmentShift(uintptr_t objectAlignmentShift)
	{
		_objectAlignmentShift = objectAlignmentShift;
	}

 	/**
	 * Get run-time Object Alignment in the heap value
	 */
	MMINLINE uintptr_t
	getObjectAlignmentInBytes()
	{
		return _objectAlignmentInBytes;
	}

 	/**
	 * Get run-time Object Alignment Shift value
	 */
	MMINLINE uintptr_t
	getObjectAlignmentShift()
	{
		return _objectAlignmentShift;
	}

};
#endif /* OBJECTMODEL_HPP_ */
