/*******************************************************************************
 * Copyright (c) 2016, 2016 IBM Corp. and others
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


#include "omrcfg.h"

#if defined(OMR_GC_OBJECT_MAP)

#include "omr.h"
#include "ModronAssertions.h"

#include <string.h>

#include "ObjectMap.hpp"

#include "MarkMap.hpp"

MM_ObjectMap *
MM_ObjectMap::newInstance(MM_EnvironmentBase *env)
{
	MM_ObjectMap *objectMap;

	objectMap = (MM_ObjectMap *)env->getForge()->allocate(sizeof(MM_ObjectMap), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (objectMap) {
		new(objectMap) MM_ObjectMap(env);
		if (!objectMap->initialize(env)) {
			objectMap->kill(env);
			objectMap= NULL;
		}
	}

	return objectMap;
}

void
MM_ObjectMap::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_ObjectMap::initialize(MM_EnvironmentBase *env)
{
	_objectMap = MM_MarkMap::newInstance(env, _extensions->heap->getMaximumPhysicalRange());

	if (NULL == _objectMap) {
		goto error_no_memory;
	}

	return true;

error_no_memory:
	return false;
}

void
MM_ObjectMap::tearDown(MM_EnvironmentBase *env)
{
	if (_objectMap) {
		_objectMap->kill(env);
		_objectMap = NULL;
	}
}

bool
MM_ObjectMap::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool result = true;
	/* Record the range in which valid objects appear */
	_heapBase = _extensions->heap->getHeapBase();
	_heapTop = _extensions->heap->getHeapTop();

	_committedHeapBase = _heapBase;
	_committedHeapTop = highAddress;
	result = _objectMap->heapAddRange(env, size, lowAddress, highAddress);

	return result;
}

bool
MM_ObjectMap::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	bool result = true;
	/* Record the range in which valid objects appear */
	_heapBase = _extensions->heap->getHeapBase();
	_heapTop = _extensions->heap->getHeapTop();

	_committedHeapBase = _heapBase;
	_committedHeapTop = lowAddress;
	result = _objectMap->heapAddRange(env, size, lowAddress, highAddress);

	return result;
}


bool
MM_ObjectMap::markValidObjectNoAtomic(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
{
	return inlineMarkValidObjectNoAtomic(env, objectPtr);
}

bool
MM_ObjectMap::markValidObject(MM_EnvironmentBase * env, omrobjectptr_t objectPtr)
{
	return inlineMarkValidObject(env, objectPtr);
}

uintptr_t
MM_ObjectMap::setMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop, bool clear)
{
	return _objectMap->setBitsInRange(env, heapBase, heapTop, clear);
}

void
MM_ObjectMap::markValidObjectForRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop)
{

	if (heapBase == NULL || heapTop == NULL || (heapBase == heapTop))
	{
		return;
	}

	/* We must atomically mark bits in the first and last bytes of the MarkMap belonging to this range.
	 * TLH.  This is because other threads may be marking the same byte.
	 * Internal MarkMap slots to this range cannot be written to by other threads. */

	GC_ObjectHeapIteratorAddressOrderedList objectHeapIterator(env->getExtensions(), (omrobjectptr_t)heapBase, heapTop, false, false);
	uintptr_t slotIndexLow = 0;
	uintptr_t slotIndexHigh = 0;
	omrobjectptr_t object = NULL;
	uintptr_t slotIndex = 0;
	uintptr_t bitMask = 0;
	uintptr_t markBlock = 0;

	/* Find the first and last slot we are marking in the mark map */
	_objectMap->getSlotIndexAndMask(heapBase, &slotIndexLow, &bitMask);
	/* Remove one off of the range of heapTop, since heapTop is a non-inclusive bound */
	_objectMap->getSlotIndexAndMask((omrobjectptr_t) ((uintptr_t) heapTop - 1), &slotIndexHigh, &bitMask);

	object = objectHeapIterator.nextObject();
	_objectMap->getSlotIndexAndMask(object, &slotIndex , &bitMask);

	/* Atomically set the first slot */
	while (object != NULL && slotIndex == slotIndexLow) {
		markBlock |= bitMask;
		object = objectHeapIterator.nextObject();
		_objectMap->getSlotIndexAndMask(object, &slotIndex, &bitMask);
	}
	_objectMap->markBlockAtomic(slotIndexLow, markBlock);

	if (slotIndexLow != slotIndexHigh) {

		/* Mark the middle slots */
		while (object != NULL && slotIndex != slotIndexHigh) {
			this->inlineMarkValidObjectNoCheckNoAtomic(env, object);
			object = objectHeapIterator.nextObject();
			_objectMap->getSlotIndexAndMask(object, &slotIndex, &bitMask);
		}

		/* Atomically set the last slot */
		markBlock = 0;
		while (object != NULL && slotIndex == slotIndexHigh) {
			markBlock |= bitMask;
			object = objectHeapIterator.nextObject();
			_objectMap->getSlotIndexAndMask(object, &slotIndex, &bitMask);
		}
		_objectMap->markBlockAtomic(slotIndexHigh, markBlock);
	}
}

#endif /* defined(OMR_GC_OBJECT_MAP) */
