/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef OMR_OM_OBJECTMODELDELEGATE_HPP_
#define OMR_OM_OBJECTMODELDELEGATE_HPP_

#include <OMR/Om/Array.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/Object.hpp>
#include <OMR/Om/CellOperations.hpp>

#include "ForwardedHeader.hpp"
#include "objectdescription.h"

class MM_GCExtensionsBase;
class MM_AllocateInitialization;
class MM_EnvironmentBase;

using CLI_THREAD_TYPE = ::OMR::Om::Context;

namespace OMR
{
namespace Om
{

class ObjectModelDelegate
{
public:
	/**
	 * If the received object holds an indirect reference (ie a reference to an
	 * object that is not reachable from the object reference graph) a pointer to
	 * the referenced object should be returned here. This method is called during
	 * heap walks for each heap object.
	 *
	 * @param objectPtr the object to botain indirct reference from
	 * @return a pointer to the indirect object, or NULL if none
	 */
	MMINLINE Cell* getIndirectObject(const Cell* cell) const { return nullptr; }

	/**
	 * Get the fomrobjectptr_t offset of the slot containing the object header.
	 */
	constexpr uintptr_t getObjectHeaderSlotOffset() const { return 0; }

	/**
	 * Get the bit offset to the flags byte in object headers.
	 */
	constexpr uintptr_t getObjectHeaderSlotFlagsShift() const { return 0; }

	/**
	 * Get the exact size of the object header, in bytes. This includes the size
	 * of the metadata slot.
	 */
	constexpr uintptr_t getObjectHeaderSizeInBytes(Cell* cell) const { return 0; }

	/**
	 * Get the exact size of the object data, in bytes. This excludes the size of
	 * the object header and any bytes added for object alignment. If the object
	 * has a discontiguous representation, this method should return the size of
	 * the root object that the discontiguous parts depend from.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the exact size of an object, in bytes, excluding padding bytes and
	 * header bytes
	 */
	MMINLINE uintptr_t getObjectSizeInBytesWithoutHeader(Cell* objectPtr)
	{
		return getObjectSizeInBytesWithHeader(objectPtr) -
			getObjectHeaderSizeInBytes(objectPtr);
	}

	/**
	 * Get the exact size of the object, in bytes, including the object header and
	 * data. This should not include any padding bytes added for alignment. If the
	 * object has a discontiguous representation, this method should return the
	 * size of the root object that the discontiguous parts depend from.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the exact size of an object, in bytes, excluding padding bytes
	 */
	MMINLINE uintptr_t getObjectSizeInBytesWithHeader(const Cell* cell)
	{
		switch (cellKind(cell)) {
		case CellKind::OBJECT:
			return reinterpret_cast<const Object*>(cell)->allocSize();
		case CellKind::SHAPE:
			return reinterpret_cast<const Shape*>(cell)->allocSize();
		case CellKind::ARRAY:
			return reinterpret_cast<const Array*>(cell)->allocSize();
		default: throw std::runtime_error("Unrecognized cell type");
		}
	}

	/**
	 * Get the total footprint of an object, in bytes, including the object
	 * header and all data. If the object has a discontiguous representation,
	 * this method should return the size of the root object plus the total of
	 * all the discontiguous parts of the object.
	 *
	 * Languages that support indexable objects (e.g. arrays) must provide an
	 * implementation that distinguishes indexable and scalar objects and
	 * handles them appropriately.
	 *
	 * @param[in] objectPtr points to the object to determine size for
	 * @return the total size of an object, in bytes, including discontiguous
	 * parts
	 */
	MMINLINE uintptr_t getTotalFootprintInBytes(omrobjectptr_t objectPtr)
	{
		Assert_MM_false(isIndexable(objectPtr));
		return getObjectSizeInBytesWithHeader(objectPtr);
	}

	/**
	 * If object initialization fails for any reason, this method must return
	 * NULL. In that case, the heap memory allocated for the object will become
	 * floating garbage in the heap and will be recovered in the next GC cycle.
	 *
	 * @param[in] env points to the environment for the calling thread
	 * @param[in] allocatedBytes points to the heap memory allocated for the
	 * object
	 * @param[in] allocateInitialization points to the MM_AllocateInitialization
	 * instance used to allocate the heap memory
	 * @return pointer to the initialized object, or NULL if initialization
	 * fails
	 */
	omrobjectptr_t initializeAllocation(
		MM_EnvironmentBase* env, void* allocatedBytes, MM_AllocateInitialization* init);

	/**
	 * Returns TRUE if an object is indexable, FALSE otherwise. Languages that
	 * support indexable objects (e.g. arrays) must provide an implementation
	 * that distinguishes indexable from scalar objects.
	 *
	 * @param objectPtr pointer to the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool isIndexable(omrobjectptr_t objectPtr) { return false; }

/**
 * The following methods (defined(OMR_GC_MODRON_SCAVENGER)) are required if
 * generational GC is configured for the build (--enable-OMR_GC_MODRON_SCAVENGER
 * in configure_includes/configure_*.mk). They typically involve a
 * MM_ForwardedHeader object, and allow information about the forwarded object
 * to be obtained.
 */
#if defined(OMR_GC_MODRON_SCAVENGER)
	/**
	 * Returns TRUE if the object referred to by the forwarded header is
	 * indexable.
	 *
	 * @param forwardedHeader pointer to the MM_ForwardedHeader instance
	 * encapsulating the object
	 * @return TRUE if object is indexable, FALSE otherwise
	 */
	MMINLINE bool isIndexable(MM_ForwardedHeader* forwardedHeader) { return false; }

	/**
	 * Get the instance size (total) of a forwarded object from the forwarding
	 * pointer. The  size must include the header and any expansion bytes to be
	 * allocated if the object will grow when moved.
	 *
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance
	 * encapsulating the object
	 * @return The instance size (total) of the forwarded object
	 */
	MMINLINE uintptr_t getForwardedObjectSizeInBytes(MM_ForwardedHeader* forwardedHeader)
	{
		/// TODO: Why is this giving us an fomrobject_t, and not a real pointer?
		return getTotalFootprintInBytes((reinterpret_cast<Cell*>(forwardedHeader->getPreservedSlot())));
	}

	/**
	 * Return true if the object holds references to heap objects not reachable
	 * from reference graph. For example, an object may be associated with a
	 * class and the class may have associated meta-objects that are in the heap
	 * but not directly reachable from the root set. This method is called to
	 * determine whether or not any such objects exist.
	 *
	 * @param thread points to calling thread
	 * @param objectPtr points to an object
	 * @return true if object holds indirect references to heap objects
	 */
	MMINLINE bool hasIndirectObjectReferents(CLI_THREAD_TYPE* thread, omrobjectptr_t objectPtr)
	{
		return false;
	}

	/**
	 * Calculate the actual object size and the size adjusted to object
	 * alignment. The calculated object size includes any expansion bytes
	 * allocated if the object will grow when moved.
	 *
	 * @param[in] env points to the environment for the calling thread
	 * @param[in] forwardedHeader pointer to the MM_ForwardedHeader instance
	 * encapsulating the object
	 * @param[out] objectCopySizeInBytes actual object size
	 * @param[out] objectReserveSizeInBytes size adjusted to object alignment
	 * @param[out] hotFieldAlignmentDescriptor pointer to hot field alignment
	 * descriptor for class (or NULL)
	 */
	void calculateObjectDetailsForCopy(
		MM_EnvironmentBase* env, MM_ForwardedHeader* forwardedHeader,
		uintptr_t* objectCopySizeInBytes, uintptr_t* objectReserveSizeInBytes,
		uintptr_t* hotFieldAlignmentDescriptor);
#endif /* defined(OMR_GC_MODRON_SCAVENGER) */

	/**
	 * Constructor receives a copy of OMR's object flags mask, normalized to low
	 * order byte.
	 */
	constexpr ObjectModelDelegate(fomrobject_t omrHeaderSlotFlagsMask) {}
};

}  // namespace Om
}  // namespace OMR

using GC_ObjectModelDelegate = OMR::Om::ObjectModelDelegate;

#endif /* OMR_OM_OBJECTMODELDELEGATE_HPP_ */
