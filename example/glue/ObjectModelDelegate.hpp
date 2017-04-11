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

#ifndef OBJECTMODELDELEGATE_HPP_
#define OBJECTMODELDELEGATE_HPP_

#include "objectdescription.h"

#include "ForwardedHeader.hpp"

class MM_AllocateInitialization;
class MM_EnvironmentBase;

#define CLI_THREAD_TYPE OMR_VMThread

struct CLI_THREAD_TYPE;

class GC_ObjectModelDelegate
{
/*
 * Member data and types
 */
private:
	/**
	 * OMR requires that the language reserve the least significant byte in the first fomrobject_t
	 * slot of an object to record object flag bits used in generational and compacting garbage
	 * collectors.
	 *
	 * This constraint may be removed in future revisions. For now, _objectHeaderSlotOffset
	 * must be zero as it represents the fomrobject_t offset to the object header slot
	 * containing the OMR flag bits.
	 *
	 * The _objectHeaderSlotFlagsShift is the right shift required to bring the flags byte
	 * in the object header slot into the least significant byte. For the time being this shift
	 * must be zero.
	 *
	 * The _objectHeaderSlotSizeShift is unique to this example (transparent to OMR). It is used to
	 * extract example object size from the object header slot.
	 */
	static const uintptr_t _objectHeaderSlotOffset = 0;
	static const uintptr_t _objectHeaderSlotFlagsShift = 0;
	static const uintptr_t _objectHeaderSlotSizeShift = 8;

protected:
public:

/*
 * Member functions
 */
private:
	/**
	 * Get the total size (including header slot) of the object from the object header slot.
	 */
	uintptr_t
	extractSizeFromObjectHeaderSlot(fomrobject_t headerSlot)
	{
		return headerSlot >> _objectHeaderSlotSizeShift;
	}

protected:
public:
	/**
	 * Get the fomrobjectptr_t offset of the slot containing the object header.
	 */
	MMINLINE uintptr_t
	getObjectHeaderSlotOffset()
	{
		return _objectHeaderSlotOffset;
	}

	/**
	 * Get the bit offset to the flags byte in object headers.
	 */
	MMINLINE uintptr_t
	getObjectHeaderSlotFlagsShift()
	{
		return _objectHeaderSlotFlagsShift;
	}

	/**
	 * Get the exact size of the object header, in bytes. This includes the size of the metadata slot.
	 */
	MMINLINE uintptr_t
	getObjectHeaderSizeInBytes(omrobjectptr_t objectPtr)
	{
		return sizeof(fomrobject_t);
	}

	/**
	 * Get the exact size of the object data, in bytes. This excludes the size of the object header and
	 * any bytes added for object alignment. If the object has a discontiguous representation, this
	 * method should return the size of the root object that the discontiguous parts depend from.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the exact size of an object, in bytes, excluding padding bytes and header bytes
	 */
	MMINLINE uintptr_t
	getObjectSizeInBytesWithoutHeader(omrobjectptr_t objectPtr)
	{
		return getObjectSizeInBytesWithHeader(objectPtr) - getObjectHeaderSizeInBytes(objectPtr);
	}

	/**
	 * Get the exact size of the object, in bytes, including the object header and data. This should not
	 * include any padding bytes added for alignment. If the object has a discontiguous representation,
	 * this method should return the size of the root object that the discontiguous parts depend from.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the exact size of an object, in bytes, excluding padding bytes
	 */
	MMINLINE uintptr_t
	getObjectSizeInBytesWithHeader(omrobjectptr_t objectPtr)
	{
		fomrobject_t *headerSlotAddress = (fomrobject_t *)objectPtr + _objectHeaderSlotOffset;
		return extractSizeFromObjectHeaderSlot(*headerSlotAddress);
	}

	/**
	 * Get the total footprint of an object, in bytes, including the object header and all data.
	 * If the object has a discontiguous representation, this method should return the size of
	 * the root object plus the total of all the discontiguous parts of the object.
	 *
	 * Languages that support indexable objects (e.g. arrays) must provide an implementation
	 * that distinguishes indexable and scalar objects and handles them appropriately.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the total size of an object, in bytes, including discontiguous parts
	 */
	MMINLINE uintptr_t
	getTotalFootprintInBytes(omrobjectptr_t objectPtr)
	{
		Assert_MM_false(isIndexable(objectPtr));
		return getObjectSizeInBytesWithHeader(objectPtr);
	}

	/**
	 * If object initialization fails for any reason, this method must return NULL. In that case, the heap
	 * memory allocated for the object will become floating garbage in the heap and will be recovered in
	 * the next GC cycle.
	 *
	 * @param[in] env points to the environment for the calling thread
	 * @param[in] allocatedBytes points to the heap memory allocated for the object
	 * @param[in] allocateInitialization points to the MM_AllocateInitialization instance used to allocate the heap memory
	 * @return pointer to the initialized object, or NULL if initialization fails
	 */
	omrobjectptr_t initializeAllocation(MM_EnvironmentBase *env, void *allocatedBytes, MM_AllocateInitialization *allocateInitialization);

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
		return false;
	}

	/**
	 * The following methods (defined(OMR_GC_MODRON_SCAVENGER)) are required if generational GC is
 	 * configured for the build (--enable-OMR_GC_MODRON_SCAVENGER in configure_includes/configure_*.mk).
 	 * They typically involve a MM_ForwardedHeader object, and allow information about the forwarded
 	 * object to be obtained.
	 */
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
		return false;
	}

	/**
	 * Get the instance size (total) of a forwarded object from the forwarding pointer. The  size must
	 * include the header and any expansion bytes to be allocated if the object will grow when moved.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @return The instance size (total) of the forwarded object
	 */
	MMINLINE uintptr_t
	getForwardedObjectSizeInBytes(MM_ForwardedHeader *forwardedHeader)
	{
		return extractSizeFromObjectHeaderSlot(forwardedHeader->getPreservedSlot());
	}

	/**
	 * Return true if the object holds references to heap objects not reachable from reference graph. For
	 * example, an object may be associated with a class and the class may have associated meta-objects
	 * that are in the heap but not directly reachable from the root set. This method is called to
	 * determine whether or not any such objects exist.
	 *
	 * @param thread points to calling thread
	 * @param objectPtr points to an object
	 * @return true if object holds indirect references to heap objects
	 */
	MMINLINE bool
	hasIndirectObjectReferents(CLI_THREAD_TYPE *thread, omrobjectptr_t objectPtr)
	{
		return false;
	}

	/**
	 * Calculate the actual object size and the size adjusted to object alignment. The calculated object size
	 * includes any expansion bytes allocated if the object will grow when moved.
	 *
	 * @param[in] env points to the environment for the calling thread
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance encapsulating the object
	 * @param[out] objectCopySizeInBytes actual object size
	 * @param[out] objectReserveSizeInBytes size adjusted to object alignment
	 * @param[out] hotFieldAlignmentDescriptor pointer to hot field alignment descriptor for class (or NULL)
	 */
	void calculateObjectDetailsForCopy(MM_EnvironmentBase *env, MM_ForwardedHeader *forwardedHeader, uintptr_t *objectCopySizeInBytes, uintptr_t *objectReserveSizeInBytes, uintptr_t *hotFieldAlignmentDescriptor);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/**
	 * Constructor receives a copy of OMR's object flags mask, normalized to low order byte.
	 */
	GC_ObjectModelDelegate(fomrobject_t omrHeaderSlotFlagsMask) {}
};
#endif /* OBJECTMODELDELEGATE_HPP_ */
