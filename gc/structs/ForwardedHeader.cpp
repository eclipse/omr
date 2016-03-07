/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#include "ForwardedHeader.hpp"

#include "AtomicOperations.hpp"

#if defined(FORWARDEDHEADER_DEBUG)
void
MM_ForwardedHeader::Assert(bool condition, const char *assertion, const char *file, uint32_t line)
{
	if (!condition) {
		fprintf(stderr, "Assertion failed: %s\n\t@ %s:%d\n", assertion, file, line);
	}
}

void
MM_ForwardedHeader::Dump(omrobjectptr_t destinationObjectPtr)
{
	fprintf(stderr, "MM_ForwardedHeader@%p[_objectPtr=%p] -> %p:\n", this, _objectPtr, destinationObjectPtr);
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
	fprintf(stderr, "\t*((uintptr_t*)_objectPtr): 0x%lx; msb(0x%x) lsb(0x%x); slot(0x%x) overlap(0x%x)\n", *((uintptr_t*)_objectPtr), (uint32_t)((*((uintptr_t*)_objectPtr) >> 32) & 0xffffffff), (uint32_t)(*((uintptr_t*)_objectPtr) & 0xffffffff), (uint32_t)_preserved.slot, (uint32_t)_preserved.overlap);
#else /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
	fprintf(stderr, "\t*((uintptr_t*)_objectPtr): 0x%lx; msb(0x%x) lsb(0x%x); slot(0x%lx)\n", *((uintptr_t*)_objectPtr), (uint32_t)((*((uintptr_t*)_objectPtr) >> 32) & 0xffffffff), (uint32_t)(*((uintptr_t*)_objectPtr) & 0xffffffff), (uintptr_t)_preserved.slot);
#endif /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
	fprintf(stderr, "\t\tgetObject(): %p\n", getObject());
	if (!isForwardedPointer()) {
		fprintf(stderr, "\t\tisReverseForwardedPointer(): 0x%x\n", isReverseForwardedPointer());
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER)
		fprintf(stderr, "\t\tgetPreservedSlot(): 0x%x\n", (uint32_t)getPreservedSlot());
		fprintf(stderr, "\t\tgetPreservedOverlap(): 0x%x\n", getPreservedOverlap());
#else /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
		fprintf(stderr, "\t\tgetPreservedSlot(): 0x%lx\n", (uintptr_t)getPreservedSlot());
#endif /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) */
	} else {
		fprintf(stderr, "\t\tgetForwardedObject(): %p\n", getForwardedObject());
	}
}
#endif /* defined(FORWARDEDHEADER_DEBUG) */

/**
 * Mark the object with a forwarding tag. This may overwrite the forwarding slot with a pointer to the
 * specified forwarded location with the forwarded bit set, if the object has not been forwarded by
 * another thread since this MM_ForwardedHeader was instantiated. In any case the actual forwarded
 * location is returned.
 *
 * @param[in] destinationObjectPtr the forwarded location
 * @return the actual location of the forwarded object
 */
omrobjectptr_t
MM_ForwardedHeader::setForwardedObject(omrobjectptr_t destinationObjectPtr)
{
	ForwardedHeaderAssert(!isForwardedPointer());
	volatile MutableHeaderFields* objectHeader = (volatile MutableHeaderFields *)((uintptr_t*)_objectPtr + _forwardingSlotOffset);
	uintptr_t oldValue = *(uintptr_t *)&_preserved.slot;

	/* Forwarded tag should be in low bits of the pointer and at the same time be in forwarding slot */
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) && !defined(OMR_ENV_LITTLE_ENDIAN)
	/* To get it for compressed big endian just swap halves of pointer */
	uintptr_t newValue = (((uintptr_t)destinationObjectPtr | _forwardedTag) << 32) | (((uintptr_t)destinationObjectPtr >> 32) & 0xffffffff);
#else /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) && !defined(OMR_ENV_LITTLE_ENDIAN) */
	/* For little endian or not compressed write uintptr_t bytes straight */
	uintptr_t newValue = (uintptr_t)destinationObjectPtr | _forwardedTag;
#endif /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) && !defined(OMR_ENV_LITTLE_ENDIAN) */

	if (MM_AtomicOperations::lockCompareExchange((volatile uintptr_t*)&objectHeader->slot, oldValue, newValue) != oldValue) {
		MM_ForwardedHeader forwardedObject(_objectPtr, _forwardingSlotOffset);
		ForwardedHeaderAssert(forwardedObject.isForwardedPointer());
		destinationObjectPtr = forwardedObject.getForwardedObject();
	}

	ForwardedHeaderAssert(NULL != destinationObjectPtr);
	return destinationObjectPtr;
}

/**
 * If this is a forwarded pointer then return the forwarded location.
 *
 * @return a pointer to the forwarded location, or NULL if this not a forwarded pointer.
 */
omrobjectptr_t
MM_ForwardedHeader::getForwardedObject()
{
	omrobjectptr_t forwardedObject = NULL;

	if (isForwardedPointer()) {
#if defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) && !defined(OMR_ENV_LITTLE_ENDIAN)
		/* Compressed big endian - read two halves separately */
		uint32_t hi = (uint32_t)_preserved.overlap;
		uint32_t lo = (uint32_t)_preserved.slot & ~_forwardedTag;
		uintptr_t restoredForwardingSlotValue = (((uintptr_t)hi) <<32 ) | ((uintptr_t)lo);
#else /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) && !defined(OMR_ENV_LITTLE_ENDIAN) */
		/* Little endian or not compressed - read all uintptr_t bytes at once */
		uintptr_t restoredForwardingSlotValue = *(uintptr_t *)(&_preserved.slot) & ~_forwardedTag;
#endif /* defined (OMR_INTERP_COMPRESSED_OBJECT_HEADER) && !defined(OMR_ENV_LITTLE_ENDIAN) */

		forwardedObject = (omrobjectptr_t)(restoredForwardingSlotValue);
	}

	return forwardedObject;
}



