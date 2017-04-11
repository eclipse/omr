/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2017
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

#if !defined(OBJECTMODELBASE_HPP_)
#define OBJECTMODELBASE_HPP_

#if defined(OMR_EXAMPLE)
#define OBJECT_MODEL_MODRON_ASSERTIONS
#endif /* defined(OMR_EXAMPLE) */

#if defined(OBJECT_MODEL_MODRON_ASSERTIONS)
#include "ModronAssertions.h"
#endif /* defined(OBJECT_MODEL_MODRON_ASSERTIONS) */
#include "modronbase.h"
#include "objectdescription.h"
#include "omrgcconsts.h"

#include "BaseVirtual.hpp"
#include "ForwardedHeader.hpp"
#include "HeapLinkedFreeHeader.hpp"
#include "ObjectModelDelegate.hpp"

class MM_AllocateInitialization;
class MM_EnvironmentBase;
class MM_GCExtensionsBase;
struct OMR_VMThread;

/**
 * Basic object geometry in heap
 */
#define OMR_MINIMUM_OBJECT_ALIGNMENT 		8		/* heap alignment, in bytes */
#define OMR_MINIMUM_OBJECT_ALIGNMENT_SHIFT	3		/* base 2 log of heap alignment */
#define OMR_MINIMUM_OBJECT_SIZE 			16		/* size of smallest possible object allocation, in bytes */

/**
 * Bit geometry within header flags byte.
 *
 * NOTE: These are normalized to low-order byte. Object header flags must be right-shifted to low-order
 * byte (see GC_ObjectModelBase::getObjectHeaderSlotFlagsShift()) before applying these masks/shifts and
 * subsets of these masks must be left-shifted to align with object header flags.
 */
#define OMR_OBJECT_METADATA_FLAGS_BIT_COUNT	8
#define OMR_OBJECT_METADATA_FLAGS_MASK		0xFF
#define OMR_OBJECT_METADATA_AGE_MASK		0xF0
#define OMR_OBJECT_METADATA_AGE_SHIFT		4

/**
 * Remembered bit states overlay tenured header age flags. These are normalized to low-order byte, as above.
 */
#define OMR_OBJECT_METADATA_REMEMBERED_BITS				OMR_OBJECT_METADATA_AGE_MASK
#define STATE_NOT_REMEMBERED  							0x00
#define STATE_REMEMBERED								0x10
#define OMR_TENURED_STACK_OBJECT_RECENTLY_REFERENCED	(STATE_REMEMBERED + (1 << OMR_OBJECT_METADATA_AGE_SHIFT))
#define OMR_TENURED_STACK_OBJECT_CURRENTLY_REFERENCED	(STATE_REMEMBERED + (2 << OMR_OBJECT_METADATA_AGE_SHIFT))

/**
 * Provides information for a given object.
 * @ingroup GC_Base
 */
class GC_ObjectModelBase : public MM_BaseVirtual
{
/*
 * Member data and types
 */
private:
	GC_ObjectModelDelegate _delegate;	/**< instance of object model delegate class */

protected:
	uintptr_t _objectAlignmentInBytes; 	/**< cached copy of heap object alignment factor, in bytes */
	uintptr_t _objectAlignmentShift; 	/**< cached copy of heap object alignment shift, must be log2(_objectAlignmentInBytes)  */

public:

/*
 * Member functions
 */
private:

protected:
	/**
	 * Get a pointer to the object model delegate associated with this object model
	 */
	MMINLINE GC_ObjectModelDelegate *
	getObjectModelDelegate()
	{
		return &_delegate;
	}

	/**
	 * Get the address of the fomrobject_t slot containing the object header.
	 *
	 * @param objectPtr the object to obtain the header slot address for
	 * @return the address of the fomrobject_t slot containing the object header
	 */
	MMINLINE fomrobject_t*
	getObjectHeaderSlotAddress(omrobjectptr_t objectPtr)
	{
		return (fomrobject_t*)(objectPtr) + _delegate.getObjectHeaderSlotOffset();
	}

public:
	/**
	 * Initialize the receiver, a new instance of GC_ObjectModel
	 *
	 * @return true on success, false on failure
	 */
	virtual bool initialize(MM_GCExtensionsBase *extensions) = 0;

	/**
	 * Tear down the receiver
	 */
	virtual void tearDown(MM_GCExtensionsBase *extensions) = 0;

	/**
	 * Get the bit offset to the flags byte in object headers.
	 */
	MMINLINE uintptr_t
	getObjectHeaderSlotFlagsShift()
	{
		return _delegate.getObjectHeaderSlotFlagsShift();
	}

	/**
	 * Get the value of the flags byte from the header of an object. The flags value is
	 * returned in the low-order byte of the returned value.
	 *
	 * @param objectPtr the object to extract the flags byte from
	 * @return the value of the flags byte from the object header
	 */
	MMINLINE uintptr_t
	getObjectFlags(omrobjectptr_t objectPtr)
	{
		return ((*getObjectHeaderSlotAddress(objectPtr)) >> getObjectHeaderSlotFlagsShift()) & (fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK;
	}

	/**
	 * Get the age of a heap object. The age value is returned in the low order bits of
	 * the returned value.
	 *
	 * @param objectPtr the object to extract the age from
	 */
	MMINLINE uintptr_t
	getObjectAge(omrobjectptr_t objectPtr)
	{
		return getObjectFlags(objectPtr) >> OMR_OBJECT_METADATA_AGE_SHIFT;
	}

	/**
	 * Returns the size of an object header, in bytes.
	 * @param objectPtr Pointer to an object
	 * @return The size of an object header, in bytes.
	 */
	MMINLINE UDATA
	getHeaderSize(omrobjectptr_t objectPtr)
	{
		return _delegate.getObjectHeaderSizeInBytes(objectPtr);
	}

	/**
	 * Returns the size of an object, in bytes, excluding the header.
	 * @param objectPtr Pointer to an object
	 * @return The size of an object, in bytes, excluding the header
	 */
	MMINLINE UDATA
	getSizeInBytesWithoutHeader(omrobjectptr_t objectPtr)
	{
		return _delegate.getObjectSizeInBytesWithoutHeader(objectPtr);
	}

	/**
	 * Returns the size of an object, in bytes, including the header.
	 * @param objectPtr Pointer to an object
	 * @return The size of an object, in bytes, including the header
	 */
	MMINLINE UDATA
	getSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
		return _delegate.getObjectSizeInBytesWithHeader(objectPtr);
	}

	/**
	 * Determine the total size of an object, in bytes, including padding bytes added to bring tail
	 * of object into heap alignment (see GC_ObjectModelBase::adjustSizeInBytes()). If the object has
	 * a discontiguous representation, this method should return the size of the root object that the
	 * discontiguous parts depend from. If the object has been expanded, the total must include the
	 * expansion size.
	 *
	 * This method should also account for any expansion bytes added to the object since instantiation.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the total size of an object, in bytes, including padding bytes
	 */
	MMINLINE uintptr_t
	getConsumedSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
		return adjustSizeInBytes(getSizeInBytesWithHeader(objectPtr));
	}

	/**
	 * Determine the total footprint of an object, in bytes, including padding bytes added to bring tail
	 * of object into heap alignment (see GC_ObjectModelBase::adjustSizeInBytes()). If the object has
	 * a discontiguous representation, this method should return the size of the root object plus the
	 * total of all the discontiguous parts of the object.
	 *
	 * This method should also account for any expansion bytes added to the object since instantiation.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the total size of an object, in bytes, including padding bytes and discontiguous parts
	 */
	MMINLINE uintptr_t
	getTotalFootprintInBytes(omrobjectptr_t objectPtr)
	{
		return adjustSizeInBytes(_delegate.getTotalFootprintInBytes(objectPtr));
	}

	/**
	 * Object size should be at least OMR_MINIMUM_OBJECT_SIZE and _objectAlignmentInBytes-byte aligned.
	 * @param sizeInBytes Unadjusted size of an object
	 * @return Adjusted size
	 */
	MMINLINE uintptr_t
	adjustSizeInBytes(uintptr_t sizeInBytes)
	{
		sizeInBytes =  (sizeInBytes + (_objectAlignmentInBytes - 1)) & (uintptr_t)~(_objectAlignmentInBytes - 1);

#if defined(OMR_GC_MINIMUM_OBJECT_SIZE)
		if (sizeInBytes < OMR_MINIMUM_OBJECT_SIZE) {
			sizeInBytes = OMR_MINIMUM_OBJECT_SIZE;
		}
#endif /* OMR_GC_MINIMUM_OBJECT_SIZE */

		return sizeInBytes;
	}

	/**
	 * This method will be called by MM_AllocateInitialization::allocateAndInitializeObject() to allow
	 * the object model to complete the initialization of the memory (allocatedBytes) allocated for the
	 * object, after the OMR object flags have been set in the allocated object header.
	 *
	 * For most languages, allocateInitialization will point to a language-specific subclass of
	 * MM_AllocateInitialization and will use MM_AllocateInitialization::getAllocationCategory() to
	 * determine and cast to the specific subclass and delegate object initialization to the subclass.
	 *
	 * If object initialization fails for any reason, this method must return NULL. In that case, the heap
	 * memory allocated for the object will become floating garbage in the heap and will be recovered in
	 * the next GC cycle.
	 *
	 * @param[in] env points to the environment for the calling thread
	 * @param[in] allocatedBytes points to the heap memory allocated for the object
	 * @param[in] allocateInitialization points to the MM_AllocateInitialization instance used to allocate the heap memory
	 * @return pointer to the initialized object, or NULL if initialization fails
	 */
	MMINLINE omrobjectptr_t
	initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization)
	{
		return _delegate.initializeAllocation(env, allocatedBytes, allocateInitialization);
	}

#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Calculate the actual object size and the size adjusted to object alignment. The calculated object size
	 * includes any expansion bytes allocated if the object will grow when moved.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[out] objectCopySizeInBytes actual object size
	 * @param[out] objectReserveSizeInBytes size adjusted to object alignment
	 * @param[out] hotFieldAlignmentDescriptor pointer to hot field alignment descriptor for class (or NULL)
	 */
	MMINLINE void
	calculateObjectDetailsForCopy(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *objectReserveSizeInBytes, uintptr_t *hotFieldAlignmentDescriptor)
	{
		_delegate.calculateObjectDetailsForCopy(env, forwardedHeader, objectCopySizeInBytes, objectReserveSizeInBytes, hotFieldAlignmentDescriptor);
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/**
	 * Set run-time object alignment and shift values in this object model and in the OMR VM struct. These
	 * values are dependent on OMR_VM::_compressedPointersShift, which must be set before calling this
	 * method. These values would be const except that they can sometimes be determined only after the
	 * object model has been instantiated. In any case, this method should be called only once, before
	 * attempting to allocate objects from the heap.
	 *
	 * @param omrVM pointer to the OMR VM struct, with final value for OMR_VM::_compressedPointersShift
	 */
	MMINLINE void
	setObjectAlignment(OMR_VM *omrVM)
	{
		_objectAlignmentInBytes = OMR_MAX((uintptr_t)1 << omrVM->_compressedPointersShift, OMR_MINIMUM_OBJECT_ALIGNMENT);
		_objectAlignmentShift = OMR_MAX(omrVM->_compressedPointersShift, OMR_MINIMUM_OBJECT_ALIGNMENT_SHIFT);

		omrVM->_objectAlignmentInBytes = _objectAlignmentInBytes;
		omrVM->_objectAlignmentShift = _objectAlignmentShift;
	}

 	/**
	 * Get run-time Object Alignment value
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

	/**
	 * Returns TRUE if an object is indexable, FALSE otherwise. Languages that support indexable objects
	 * (e.g. arrays) must provide an implementation that distinguishes indexable from scalar objects.
	 *
	 * @param objectPtr pointer to the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool
	isIndexable(omrobjectptr_t objectPtr)
	{
		return _delegate.isIndexable(objectPtr);
	}

	/**
	 * Returns TRUE if an object is dead, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is dead, FALSE otherwise
	 */
	MMINLINE bool
	isDeadObject(void *objectPtr)
	{
		fomrobject_t *headerSlotPtr = getObjectHeaderSlotAddress((omrobjectptr_t)objectPtr);
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
		/* for ConcurrentScavenger, forwarded bit must be 0, otherwise it's a self-forwarded object */
		return J9_GC_OBJ_HEAP_HOLE == (*headerSlotPtr & (J9_GC_OBJ_HEAP_HOLE | OMR_FORWARDED_TAG));
#else
		return J9_GC_OBJ_HEAP_HOLE == (*headerSlotPtr & J9_GC_OBJ_HEAP_HOLE);
#endif /* OMR_GC_CONCURRENT_SCAVENGER */ 		
	}

	/**
	 * Returns TRUE if an object is a dead single slot object, FALSE otherwise.
	 * @param objectPtr Pointer to an object
	 * @return TRUE if an object is a dead single slot object, FALSE otherwise
	 */
	MMINLINE bool
	isSingleSlotDeadObject(omrobjectptr_t objectPtr)
	{
		fomrobject_t *headerSlotPtr = getObjectHeaderSlotAddress((omrobjectptr_t)objectPtr);
		return J9_GC_SINGLE_SLOT_HOLE == (*headerSlotPtr & J9_GC_OBJ_HEAP_HOLE_MASK);
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
		} else {
			return getSizeInBytesMultiSlotDeadObject(objectPtr);
		}
	}

	/**
	 * Set flags in object header. The bitsToClear and bitsToSet masks are expected to be unshifted
	 * (aligned in low-order byte).
	 *
	 * @param objectPtr Pointer to an object
	 * @param bitsToClear unshifted mask to clear bits
	 * @param bitsToSet unshifted mask to set bits
	 */
	MMINLINE void
	setObjectFlags(omrobjectptr_t objectPtr, uintptr_t bitsToClear, uintptr_t bitsToSet)
	{
#if defined(OBJECT_MODEL_MODRON_ASSERTIONS)
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & (fomrobject_t)bitsToClear));
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & (fomrobject_t)bitsToSet));
#endif /* defined(OBJECT_MODEL_MODRON_ASSERTIONS) */

		fomrobject_t* flagsPtr = getObjectHeaderSlotAddress(objectPtr);
		fomrobject_t clear = (fomrobject_t)bitsToClear << getObjectHeaderSlotFlagsShift();
		fomrobject_t set = (fomrobject_t)bitsToSet << getObjectHeaderSlotFlagsShift();

		*flagsPtr = (*flagsPtr & ~clear) | set;
	}

	/**
	 * Unconditionally and atomically set object flags. The bitsToClear and bitsToSet
	 * masks are expected to be non-zero and unshifted (aligned in low-order byte).
	 *
	 * Call will fail (return false) if change was idempotent (previous flags unchanged).
	 *
	 * @param objectPtr Pointer to an object
	 * @param bitsToClear unshifted mask of flags to clear
	 * @param bitsToSet unshifted mask of flags to set
	 * @return true if flags have been been changed in this call
	 */
	MMINLINE bool
	atomicSetObjectFlags(omrobjectptr_t objectPtr, uintptr_t bitsToClear, uintptr_t bitsToSet)
	{
		bool result = true;

#if defined(OBJECT_MODEL_MODRON_ASSERTIONS)
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & bitsToClear));
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & bitsToSet));
#endif /* defined(OBJECT_MODEL_MODRON_ASSERTIONS) */

		volatile fomrobject_t* flagsPtr = getObjectHeaderSlotAddress(objectPtr);
		fomrobject_t clear = (fomrobject_t)(bitsToClear << getObjectHeaderSlotFlagsShift());
		fomrobject_t set = (fomrobject_t)(bitsToSet << getObjectHeaderSlotFlagsShift());
		fomrobject_t oldFlags;
		fomrobject_t newFlags;

		do {
			oldFlags = *flagsPtr;
			newFlags = (oldFlags & ~clear) | set;

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

		return result;
	}

	/**
	 * Set remembered bits to the object's header. The bits mask is expected to be unshifted (aligned in
	 * low-order byte).
	 *
	 * This method is not thread safe and can produce unexpected results if the object is exposed to other
	 * concurrent threads. Use atomicSetObjectFlags() for thread safe operation.
	 *
	 * @param objectPtr Pointer to an object
	 * @param bits unshifted bits to set
	 */
	MMINLINE void
	setRememberedBits(omrobjectptr_t objectPtr, uintptr_t bits)
	{
		setObjectFlags(objectPtr, OMR_OBJECT_METADATA_REMEMBERED_BITS, bits);
	}

	/**
	 * Returns the collector bits from object's header. The returned value is shifted down to the low-order
	 * byte of the returned value.
	 *
	 * @param objectPtr Pointer to an object
	 * @return collector bits
	 */
	MMINLINE uintptr_t
	getRememberedBits(omrobjectptr_t objectPtr)
	{
		return getObjectFlags(objectPtr) & OMR_OBJECT_METADATA_REMEMBERED_BITS;
	}

	/**
	 * Returns TRUE if an object is remembered, FALSE otherwise.
	 *
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
	 *
	 * This method is not thread safe and can produce unexpected results if the object is exposed to other
	 * concurrent threads. Use atomicSetObjectFlags() for thread safe operation.
	 *
	 * @param objectPtr Pointer to an object
	 */
	MMINLINE void
	clearRemembered(omrobjectptr_t objectPtr)
	{
		setRememberedBits(objectPtr, STATE_NOT_REMEMBERED);
	}

	/**
	 * Conditionally and atomically switch the referenced state of an object. The fromState and toState
	 * masks are expected to be unshifted (aligned in low-order byte).
	 *
	 * Call will return false with no change to object state if referenced state of the object
	 * is not fromState.
	 *
	 * @param objectPtr Pointer to an object
	 * @param fromState unshifted assumed current remembered state of object
	 * @param toState unshifted desired new remembered state of object
	 * @return true if referenced state has been set (to toState) this call
	 */
	MMINLINE bool
	atomicSwitchReferencedState(omrobjectptr_t objectPtr, uintptr_t fromState, uintptr_t toState)
	{
		bool result = true;

#if defined(OBJECT_MODEL_MODRON_ASSERTIONS)
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & fromState));
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & toState));
#endif /* defined(OBJECT_MODEL_MODRON_ASSERTIONS) */

		volatile fomrobject_t* flagsPtr = getObjectHeaderSlotAddress(objectPtr);
		fomrobject_t from = (fomrobject_t)(fromState << getObjectHeaderSlotFlagsShift());
		fomrobject_t to = (fomrobject_t)(toState << getObjectHeaderSlotFlagsShift());
		fomrobject_t rememberedMask = (fomrobject_t)(OMR_OBJECT_METADATA_REMEMBERED_BITS  << getObjectHeaderSlotFlagsShift());
		fomrobject_t oldFlags;
		fomrobject_t newFlags;

		do {
			oldFlags = *flagsPtr;
			if ((oldFlags & rememberedMask) == from) {
				newFlags = (oldFlags & ~rememberedMask) | to;
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
	 * Atomically set object referenced state. The toState mask is expected to be
	 * non-zero and unshifted (aligned in low-order byte).
	 *
	 * Call will return false if toState was set for the object by another thread.
	 *
	 * @param objectPtr Pointer to an object
	 * @param toState unshifted desired new remembered state of object
	 * @return true if referenced state has been set (to toState) this call
	 */
	MMINLINE bool
	atomicSwitchReferencedState(omrobjectptr_t objectPtr, uintptr_t toState)
	{
		bool result = true;

#if defined(OBJECT_MODEL_MODRON_ASSERTIONS)
		Assert_MM_true(0 != ((fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & toState));
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & toState));
#endif /* defined(OBJECT_MODEL_MODRON_ASSERTIONS) */

		volatile fomrobject_t* flagsPtr = getObjectHeaderSlotAddress(objectPtr);
		fomrobject_t to = (fomrobject_t)(toState << getObjectHeaderSlotFlagsShift());
		fomrobject_t rememberedMask = (fomrobject_t)(OMR_OBJECT_METADATA_REMEMBERED_BITS << getObjectHeaderSlotFlagsShift());
		fomrobject_t oldFlags;
		fomrobject_t newFlags;

		do {
			oldFlags = *flagsPtr;
			newFlags = (oldFlags & ~rememberedMask) | to;

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

		/* check to verify that this thread and no other thread atomically set flags -> toState */
		return result ? (0 == (oldFlags & rememberedMask)) : false;
	}

	/**
	 * Conditionally and atomically set the remembered state of an object. The toState
	 * mask is expected to be unshifted (aligned in low-order byte).
	 *
	 * Call will return false with no change to object state if any remembered bits
	 * are set in the object header.
	 *
	 * @param objectPtr Pointer to an object
	 * @param toState unshifted desired new remembered state of object
	 * @return true if remembered state has been set (to toState) this call
	 */
	MMINLINE bool
	atomicSetRememberedState(omrobjectptr_t objectPtr, uintptr_t toState)
	{
		bool result = true;

#if defined(OBJECT_MODEL_MODRON_ASSERTIONS)
		Assert_MM_true(0 == (~(fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK & toState));
#endif /* defined(OBJECT_MODEL_MODRON_ASSERTIONS) */

		volatile fomrobject_t* flagsPtr = getObjectHeaderSlotAddress(objectPtr);
		fomrobject_t to = (fomrobject_t)(toState << getObjectHeaderSlotFlagsShift());
		fomrobject_t rememberedMask = (fomrobject_t)(OMR_OBJECT_METADATA_REMEMBERED_BITS << getObjectHeaderSlotFlagsShift());
		fomrobject_t oldFlags;
		fomrobject_t newFlags;

		do {
			oldFlags = *flagsPtr;
			if (0 != (oldFlags & rememberedMask)) {
				result = false;
				break;
			}
			newFlags = (oldFlags & ~rememberedMask) | to;
		}
#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
		while (oldFlags != MM_AtomicOperations::lockCompareExchangeU32(flagsPtr, oldFlags, newFlags));
#else /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
		while (oldFlags != MM_AtomicOperations::lockCompareExchange(flagsPtr, (uintptr_t)oldFlags, (uintptr_t)newFlags));
#endif /* defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER) */

		return result;
	}

#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Returns TRUE if the object referred to by the forwarded header is indexable.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool
	isIndexable(MM_ForwardedHeader *forwardedHeader)
	{
		return _delegate.isIndexable(forwardedHeader);
	}

	/**
	 * Return true if the object holds references to heap objects not reachable from reference graph. For
	 * example, an object may be associated with a class and the class may have associated meta-objects
	 * that are in the heap but not directly reachable from the root set. This method is called to
	 * determine whether or not any such objects exist.
	 *
	 * @param thread points to requesting thread
	 * @param objectPtr points to an object
	 * @return true if object holds indirect references to heap objects
	 */
	MMINLINE bool
	hasIndirectObjectReferents(CLI_THREAD_TYPE *thread, omrobjectptr_t objectPtr)
	{
		return _delegate.hasIndirectObjectReferents(thread, objectPtr);
	}

	/**
	 * Get the total instance size of a forwarded object.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return the total instance size of the forwarded object
	 */
	MMINLINE uintptr_t
	getForwardedObjectSizeInBytes(MM_ForwardedHeader *forwardedHeader)
	{
		return _delegate.getForwardedObjectSizeInBytes(forwardedHeader);
	}

	/**
	 * Extract the flag bits from an unforwarded object. Flag bits are returned in the low-order byte of the returned value.
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
		return ((uintptr_t)(forwardedHeader->getPreservedSlot()) >> getObjectHeaderSlotFlagsShift()) & (fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK;
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
		return (getPreservedFlags(forwardedHeader) & OMR_OBJECT_METADATA_AGE_MASK) >> OMR_OBJECT_METADATA_AGE_SHIFT;
	}

	/**
	 * Update the new version of this object after it has been copied. This undoes any damaged
	 * caused by installing the forwarding pointer into the original prior to the copy, and sets
	 * the object age.
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
		setObjectFlags(destinationObjectPtr, OMR_OBJECT_METADATA_AGE_MASK, age);
	}
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/**
	 * Constructor.
	 */
	GC_ObjectModelBase()
		: _delegate((fomrobject_t)OMR_OBJECT_METADATA_FLAGS_MASK)
	{
		_typeId = __FUNCTION__;
#if defined(OBJECT_MODEL_MODRON_ASSERTIONS)
		Assert_MM_true((8 * (sizeof(fomrobject_t) - 1)) >= _delegate.getObjectHeaderSlotFlagsShift());
#endif /* defined(OBJECT_MODEL_MODRON_ASSERTIONS) */
	}
};
#if defined(OMR_EXAMPLE)
#undef OBJECT_MODEL_MODRON_ASSERTIONS
#endif /* defined(OMR_EXAMPLE) */
#endif /* OBJECTMODELBASE_HPP_ */
