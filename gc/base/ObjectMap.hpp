/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2016, 2016
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


#if !defined(OBJECTMAP_HPP_)
#define OBJECTMAP_HPP_

#include "omrcfg.h"
#include "omr.h"

#if defined(OMR_GC_OBJECT_MAP)

#include "BaseVirtual.hpp"

#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "MarkMap.hpp"
#include "ModronAssertions.h"
#include "ObjectHeapIteratorAddressOrderedList.hpp"
#include "ObjectModel.hpp"
#include "WorkStack.hpp"

#define SCAN_MAX (uintptr_t)(-1)
#define BITS_PER_BYTE 8

class MM_CollectorLanguageInterface;

/**
 * Tracks the location of valid objects.  This is primarily used along side a
 * conservative object scanner, which may try to scan invalid objects.
 */
class MM_ObjectMap : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:
	MM_GCExtensionsBase *_extensions;
	MM_MarkMap *_objectMap;
	void *_heapBase;
	void *_heapTop;
	void *_committedHeapBase;
	void *_committedHeapTop;
protected:
public:

	/*
	 * Function members
	 */
private:
	MMINLINE void
	assertSaneObjectPtr(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		Assert_GC_true_with_message(env, objectPtr != J9_INVALID_OBJECT, "Invalid object pointer %p\n", objectPtr);
		Assert_MM_objectAligned(env, objectPtr);
		Assert_GC_true_with_message3(env, isHeapObject(objectPtr), "Object %p not in heap range [%p,%p)\n", objectPtr, _heapBase, _heapTop);
	}

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

public:
	static MM_ObjectMap *newInstance(MM_EnvironmentBase *env);
	virtual void kill(MM_EnvironmentBase *env);

	uintptr_t numMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop);
	uintptr_t setMarkBitsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop, bool clear);
	uintptr_t numHeapBytesPerMarkMapByte() { return (_objectMap->getObjectGrain() * BITS_PER_BYTE); };

	MMINLINE MM_MarkMap *getMarkMap() { return _objectMap; };
	MMINLINE void setMarkMap(MM_MarkMap *markMap) { _objectMap = markMap; };
	
	bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);

	MMINLINE bool
	isHeapObject(omrobjectptr_t objectPtr)
	{
		return ((_heapBase <= (uint8_t *)objectPtr) && (_heapTop > (uint8_t *)objectPtr));
	}

	MMINLINE MM_MarkMap * getObjectMap() { return _objectMap; }

	MMINLINE bool
	isCommittedHeapObject(omrobjectptr_t objectPtr)
	{
		return ((_committedHeapBase <= (uint8_t *)objectPtr) && (_committedHeapTop > (uint8_t *)objectPtr));
	}

	MMINLINE bool
	inlineMarkValidObjectNoCheckNoAtomic(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		assertSaneObjectPtr(env, objectPtr);

		if (!_objectMap->setBit(objectPtr)) {
			return false;
		}

		return true;
	}

	MMINLINE bool
	inlineMarkValidObjectNoAtomic(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		bool didMark = false;

		if (NULL != objectPtr) {
			didMark = inlineMarkValidObjectNoCheckNoAtomic(env, objectPtr);
		}

		return didMark;
	}

	bool markValidObjectNoAtomic(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);

	MMINLINE bool
	inlineMarkValidObjectNoCheck(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		assertSaneObjectPtr(env, objectPtr);

		if (!_objectMap->atomicSetBit(objectPtr)) {
			return false;
		}

		return true;
	}

	MMINLINE bool
	inlineMarkValidObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr)
	{
		bool didMark = false;

		if (NULL != objectPtr) {
			didMark = inlineMarkValidObjectNoCheck(env, objectPtr);
		}

		return didMark;
	}

	bool markValidObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr);

	MMINLINE bool
	isValidObject(omrobjectptr_t objectPtr)
	{
		bool shouldCheck = isCommittedHeapObject(objectPtr) && J9_ARE_NO_BITS_SET((uintptr_t)objectPtr, _extensions->getObjectAlignmentInBytes() - 1);
		if (shouldCheck) {
			return _objectMap->isBitSet(objectPtr);
		}
		/* Everything else is not a valid object. */
		return false;
	}

	/**
	 * Walk the TLH, marking all allocated objects in the object map.
	 */
	void markValidObjectForRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop);

	MM_ObjectMap(MM_EnvironmentBase *env)
		: MM_BaseVirtual()
		, _extensions(env->getExtensions())
		, _objectMap(NULL)
		, _heapBase(NULL)
		, _heapTop(NULL)
		, _committedHeapBase(NULL)
		, _committedHeapTop(NULL)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* defined(OMR_GC_OBJECT_MAP) */

#endif /* defined(OBJECTMAP_HPP_*/
