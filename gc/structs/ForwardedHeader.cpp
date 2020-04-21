/*******************************************************************************
 * Copyright (c) 2015, 2020 IBM Corp. and others
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


#include "ForwardedHeader.hpp"
#if defined(FORWARDEDHEADER_DEBUG)
#include <stdio.h>
#include <stdlib.h>
#endif /* defined(FORWARDEDHEADER_DEBUG) */
#include <string.h>
#include "AtomicOperations.hpp"
#include "ModronAssertions.h"

#if defined(FORWARDEDHEADER_DEBUG)
void
MM_ForwardedHeader::Assert(bool condition, const char *assertion, const char *file, uint32_t line)
{
	if (!condition) {
		fprintf(stderr, "Assertion failed: %s\n\t@ %s:%d\n", assertion, file, line);
	}
}

void
MM_ForwardedHeader::ForwardedHeaderDump(omrobjectptr_t destinationObjectPtr)
{
#if defined (OMR_GC_COMPRESSED_POINTERS)
	if (compressObjectReferences()) {
		fprintf(stderr, "MM_ForwardedHeader@%p[%p(%p):%x:%x] -> %p(%p)\n", this, _objectPtr, (uintptr_t*)(*_objectPtr), getPreservedClassAndTags(), getPreservedOverlapNoCheck(), destinationObjectPtr, (uintptr_t*)(*destinationObjectPtr));
	} else
#endif /* defined (OMR_GC_COMPRESSED_POINTERS) */
	{
		fprintf(stderr, "MM_ForwardedHeader@%p[%p(%p):%x] -> %p(%p)\n", this, _objectPtr, (uintptr_t*)(*_objectPtr), getPreservedClassAndTags(), destinationObjectPtr, (uintptr_t*)(*destinationObjectPtr));
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
MM_ForwardedHeader::setForwardedObjectInternal(omrobjectptr_t destinationObjectPtr, uintptr_t forwardedTag)
{
	ForwardedHeaderAssert(!isForwardedPointer());
	uintptr_t oldValue = getPreserved();

	/* Forwarded tag should be in low bits of the pointer and at the same time be in forwarding slot */
	uintptr_t newValue = flipValue((uintptr_t)destinationObjectPtr | forwardedTag);
	if (MM_AtomicOperations::lockCompareExchange((volatile uintptr_t*)getObject(), oldValue, newValue) != oldValue) {
		/* If we lost forwarding it, return where we are really forwarded. Another thread could raced us to forward on another location
		 * or (Concurrent world) self-forward it. In the later case, we will return NULL */
		MM_ForwardedHeader forwardedObject(_objectPtr, compressObjectReferences());
		ForwardedHeaderAssert(forwardedObject.isForwardedPointer());
		destinationObjectPtr = forwardedObject.getForwardedObject();
	}

	return destinationObjectPtr;
}

/**
 * If this is a forwarded pointer then return the forwarded location.
 *
 * @return a (strict) pointer to the forwarded location, or NULL if this not a (strict) forwarded pointer.
 */
omrobjectptr_t
MM_ForwardedHeader::getForwardedObject()
{
	omrobjectptr_t forwardedObject = NULL;

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	uintptr_t forwardedTag = _forwardedTag | _beingCopiedHint;
	if (isStrictlyForwardedPointer()) {
#else
	uintptr_t forwardedTag = _forwardedTag;
	if (isForwardedPointer()) {
#endif
		forwardedObject = (omrobjectptr_t)(getCanonicalPreserved() & ~forwardedTag);
	}

	return forwardedObject;
}


omrobjectptr_t
MM_ForwardedHeader::getNonStrictForwardedObject()
{
	omrobjectptr_t forwardedObject = NULL;

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	uintptr_t forwardedTag = _forwardedTag | _beingCopiedHint;
	if (isStrictlyForwardedPointer()) {
#else
	uintptr_t forwardedTag = _forwardedTag;
	if (isForwardedPointer()) {
#endif /* OMR_GC_CONCURRENT_SCAVENGER */
		forwardedObject = (omrobjectptr_t)(getCanonicalPreserved() & ~forwardedTag);
	}
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	else if (isSelfForwardedPointer()) {
		forwardedObject = _objectPtr;
	}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

	return forwardedObject;
}

uintptr_t
MM_ForwardedHeader::copySetup(omrobjectptr_t destinationObjectPtr, uintptr_t *remainingSizeToCopy)
{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	uintptr_t copyOffset = referenceSize();

	ForwardedHeaderAssert(*remainingSizeToCopy >= copyOffset);
	*remainingSizeToCopy = *remainingSizeToCopy - copyOffset;
	/* take small section (about 1%) to copy now to maximize parallelism */
	uintptr_t sizeToCopy = SIZE_OF_SECTION_TO_COPY(*remainingSizeToCopy);
	/* but obey minimum increment size (to hide overhead associated with each section) */
	sizeToCopy = OMR_MAX(sizeToCopy, _minIncrement);
	/* still, don't copy more than the remaining size (whole object) */
	sizeToCopy = OMR_MIN(*remainingSizeToCopy, sizeToCopy);
	*remainingSizeToCopy -= sizeToCopy;
	uintptr_t alignmentResidue = *remainingSizeToCopy & _copySizeAlignement;
	sizeToCopy += alignmentResidue;
	*remainingSizeToCopy -= alignmentResidue;

	/* set the remaining length to copy */
	writeClassSlot(destinationObjectPtr, (*remainingSizeToCopy | (0 << OUTSTANDING_COPIES_SHIFT)) | _beingCopiedTag);
	/* Make sure that destination object header is visible to other potential participating threads.
	 * About to be executed atomic as part of forwarding operation is executed on source object header,
	 * hence it will not synchronize memory in the destination object header.
	 */
	MM_AtomicOperations::storeSync();

	return sizeToCopy;
#else
	/* currently no users outside of CS */
	ForwardedHeaderAssert(false);
	return 0;
#endif /* #if defined(OMR_GC_CONCURRENT_SCAVENGER) */
}

void
MM_ForwardedHeader::copySection(omrobjectptr_t destinationObjectPtr, uintptr_t remainingSizeToCopy, uintptr_t sizeToCopy)
{
	uintptr_t copyOffset = referenceSize() + remainingSizeToCopy;

	void *dstStartAddress = (void *)((uintptr_t)destinationObjectPtr + copyOffset);
	void *srcStartAddress = (void *)((uintptr_t)_objectPtr + copyOffset);

	memcpy(dstStartAddress, srcStartAddress, sizeToCopy);
}

#if defined(OMR_GC_CONCURRENT_SCAVENGER)
omrobjectptr_t
MM_ForwardedHeader::setSelfForwardedObject()
{
	uintptr_t oldValue = getPreservedClassAndTags();

	uintptr_t newValue = oldValue | _selfForwardedTag;

	omrobjectptr_t forwardedObject = getObject();

	if (oldValue != lockCompareExchangeObjectHeader(forwardedObject, oldValue, newValue)) {
		/* If we lost on self-forwarding, return where we are really forwarded. We could still be self-forwarded (another thread raced us) or
		 * strictly forwarded (another thread successfully copied the object). Either way, getNonStrictForwardedObject() should return us where we really are. */
		MM_ForwardedHeader forwardedHeader(forwardedObject, compressObjectReferences());
		forwardedObject = forwardedHeader.getNonStrictForwardedObject();
	}

	ForwardedHeaderAssert(NULL != forwardedObject);
	return forwardedObject;
}

void
MM_ForwardedHeader::restoreSelfForwardedPointer()
{
	ForwardedHeaderAssert(isSelfForwardedPointer());
	uintptr_t oldValue = getPreservedClassAndTags();

	uintptr_t newValue = oldValue & ~_selfForwardedTag;

	writeClassSlot(getObject(), newValue);
}


uintptr_t
MM_ForwardedHeader::winObjectSectionToCopy(volatile omrobjectptr_t copyProgressSlot, uintptr_t oldValue, uintptr_t *remainingSizeToCopy, uintptr_t outstandingCopies)
{
	/* take small section (about 1%) to copy now to maximize parallelism */
	uintptr_t sizeToCopy = SIZE_OF_SECTION_TO_COPY(*remainingSizeToCopy) & ~_copySizeAlignement;
	/* but obey minimum increment size (to hide overhead associated with each section) */
	sizeToCopy = OMR_MAX(sizeToCopy, _minIncrement);
	/* don't copy more then the remaining size */
	sizeToCopy = OMR_MIN(*remainingSizeToCopy, sizeToCopy);
	*remainingSizeToCopy -= sizeToCopy;

	/* atomically try to win this section */
	fomrobject_t newValue = (fomrobject_t)(*remainingSizeToCopy | (outstandingCopies << OUTSTANDING_COPIES_SHIFT)) | _beingCopiedTag;
	if (oldValue == lockCompareExchangeObjectHeader(copyProgressSlot, oldValue, newValue)) {
		return sizeToCopy;
	}

	return 0;
}

void
MM_ForwardedHeader::wait(uintptr_t *spinCount) {
	/* TODO: consider yielding and/or sleeping, for high spinCount value */
	for (uintptr_t i = *spinCount; i > 0; i--)	{
		MM_AtomicOperations::nop();
	}

	/* double the wait time, for subsequent re-tries */
	*spinCount += *spinCount;
}

void
MM_ForwardedHeader::copyOrWaitOutline(omrobjectptr_t destinationObjectPtr)
{
	uintptr_t spinCount = 10;

	bool participatingInCopy = false;
	while (true) {
		uintptr_t sizeToCopy = 0, remainingSizeToCopy = 0;
		do {
			uintptr_t oldValue = readClassSlot(destinationObjectPtr);
			if (0 == (oldValue & _beingCopiedTag)) {
				/* the object is fully copied */
				return;
			}
			remainingSizeToCopy = oldValue & _remainingSizeMask;
			uintptr_t outstandingCopies = (oldValue & _copySizeAlignement) >> OUTSTANDING_COPIES_SHIFT;

			if (0 == remainingSizeToCopy) {
				if (participatingInCopy) {
					Assert_MM_true(outstandingCopies > 0);
					MM_AtomicOperations::storeSync();
					uintptr_t newValue = ((outstandingCopies - 1) << OUTSTANDING_COPIES_SHIFT) | _beingCopiedTag;
					if (oldValue != lockCompareExchangeObjectHeader(destinationObjectPtr, oldValue, newValue)) {
						continue;
					}
					participatingInCopy = false;
				}
				/* No work left to help with. Just keep waiting and checking till copying/fixing is done. */
				wait(&spinCount);
				continue;
			}

			if (!participatingInCopy) {
				if (outstandingCopies < MAX_OUTSTANDING_COPIES) {
					outstandingCopies += 1;
				} else {
					wait(&spinCount);
					continue;
				}
			}

			sizeToCopy = winObjectSectionToCopy(destinationObjectPtr, oldValue, &remainingSizeToCopy, outstandingCopies);
		} while (0 == sizeToCopy);

		participatingInCopy = true;

		copySection(destinationObjectPtr, remainingSizeToCopy, sizeToCopy);
	}
}
#endif /* OMR_GC_CONCURRENT_SCAVENGER */

void
MM_ForwardedHeader::copyOrWaitWinner(omrobjectptr_t destinationObjectPtr)
{
#if defined(OMR_GC_CONCURRENT_SCAVENGER)
	uintptr_t spinCount = 10;

	while (true) {
		uintptr_t remainingSizeToCopy = 0, sizeToCopy = 0;
		do {
			uintptr_t oldValue = readClassSlot(destinationObjectPtr);
			remainingSizeToCopy = (uintptr_t)(oldValue & _remainingSizeMask);
			uintptr_t outstandingCopies = ((uintptr_t)oldValue & _copySizeAlignement) >> OUTSTANDING_COPIES_SHIFT;

			if (0 == remainingSizeToCopy) {
				if (0 == outstandingCopies) {
					return;
				} else {
					wait(&spinCount);
					continue;
				}
			}

			sizeToCopy = winObjectSectionToCopy(destinationObjectPtr, oldValue, &remainingSizeToCopy, outstandingCopies);
		} while (0 == sizeToCopy);

		copySection(destinationObjectPtr, remainingSizeToCopy, sizeToCopy);
	}
#endif /* #if defined(OMR_GC_CONCURRENT_SCAVENGER) */
}
