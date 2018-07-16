/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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

#if !defined(OBJECTSCANNER_HPP_)
#define OBJECTSCANNER_HPP_

#include "omrcfg.h"
#include "ModronAssertions.h"
#include "objectdescription.h"

#include "BaseVirtual.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "SlotObject.hpp"

/**
 * This object scanning model allows an inline getNextSlot() implementation and works best
 * with object representations that contain all object reference slots within one or more
 * contiguous blocks of fomrobject_t-aligned memory. This allows reference slots to be
 * identified by a series of (base-pointer, bit-map) pairs. Subclasses present the initial
 * pair when they instantiate and subsequent pairs, if any, in subsequent calls to
 * getNextSlotMap().
 *
 * Subclasses can set the noMoreSlots bit in _flags, to signal that any future calls to
 * getNextSlotMap() will return NULL, when they instantiate or in their implementation of
 * getNextSlotMap(). This assumes that subclasses can look ahead in their object traverse
 * and allows scanning for most objects (<32/64 slots) to complete inline, without
 * requiring a virtual method call (getNextSlotMap()). Otherwise at least one such call
 * is required before getNextSlot() will return NULL.
 */
class GC_ObjectScanner : public MM_BaseVirtual
{
	/* Data Members */
private:

protected:
	static const intptr_t _bitsPerScanMap = sizeof(uintptr_t) << 3;

	omrobjectptr_t const _parentObjectPtr;	/**< Object being scanned */
	uintptr_t _scanMap;						/**< Bit map of reference slots in object being scanned (32/64-bit window) */
#if defined(OMR_GC_LEAF_BITS)
	uintptr_t _leafMap;						/**< Bit map of reference slots in object that refernce leaf objects */
#endif /* defined(OMR_GC_LEAF_BITS) */
	fomrobject_t *_scanPtr;					/**< Pointer to base of object slots mapped by current _scanMap */
	GC_SlotObject _slotObject;				/**< Create own SlotObject class to provide output */
	uintptr_t _flags;						/**< Scavenger context flags (scanRoots, scanHeap, ...) */
	fomrobject_t *_hotScanPtr;				/** saved value of initial _scaanPtr if there is a hot field */
	uintptr_t _hotFieldsDescriptor;			/**< Hot fields descriptor for languages that support hot field tracking */
	
public:
	/**
	 *  Instantiation flags used to specialize scanner for specific scavenger operations
	 */
	enum InstanceFlags
	{
		scanRoots = 1					/* scavenge roots phase -- scan & copy/forward root objects */
		, scanHeap = 2					/* scavenge heap phase -- scan  & copy/forward objects reachable from scanned roots */
		, indexableObject = 4			/* this is set for array object scanners where the array elements can be partitioned for multithreaded scanning */
		, indexableObjectNoSplit = 8	/* this is set for array object scanners where the array elements cannot be partitioned for multithreaded scanning */
		, headObjectScanner = 16		/* this is set for array object scanners containing the elements from the first split segment, and for all non-indexable objects */
		, noMoreSlots = 128				/* this is set when object has more no slots to scan past current bitmap */
	};

	/* Member Functions */
private:

protected:
	/**
	 * Constructor. Without leaf optimization. Context generational nursery collection.
	 *
	 * For marking context with leaf optimization see below:
	 *
	 * GC_ObjectScanner(MM_EnvironmentBase *, omrobjectptr_t, fomrobject_t *, uintptr_t, uintptr_t, uintptr_t, uintptr_t)
	 *
	 * @param[in] env The environment for the scanning thread
	 * @param[in] parentObjectPtr The object to be scanned
	 * @param[in] scanPtr The first slot contained in the object to be scanned
	 * @param[in] scanMap Bit map marking object reference slots, with least significant bit mapped to slot at scanPtr
	 * @param[in] flags A bit mask comprised of InstanceFlags
	 * @param[in] hotFieldsDescriptor Hot fields descriptor for languages that support hot field tracking (0 if no hot fields support)
	 */
	GC_ObjectScanner(MM_EnvironmentBase *env, omrobjectptr_t parentObjectPtr, fomrobject_t *scanPtr, uintptr_t scanMap, uintptr_t flags, uintptr_t hotFieldsDescriptor = 0)
		: MM_BaseVirtual()
		, _parentObjectPtr(parentObjectPtr)
		, _scanMap(scanMap)
#if defined(OMR_GC_LEAF_BITS)
		, _leafMap(0)
#endif /* defined(OMR_GC_LEAF_BITS) */
		, _scanPtr(scanPtr)
		, _slotObject(env->getOmrVM(), NULL)
		, _flags(flags | headObjectScanner)
		, _hotScanPtr(NULL)
		, _hotFieldsDescriptor(hotFieldsDescriptor)
	{
		_typeId = __FUNCTION__;
	}
	
	/**
	 * Set up the scanner. Subclasses should provide a non-virtual implementation
	 * to build next slot map and call it from their constructor or just after
	 * their constructor. This will obviate the need to make an initial call to
	 * the virtual getNextSlotMap() function and, in most cases, avoid the need
	 * to call getNextSlotMap() entirely.
	 *
	 * If all of the object reference fields are mapped in the initial slot map,
	 * the sublcass implementation should call setNoMoreSlots() to indicate that
	 * the getNextSlotMap() method should not be called to refresh the slot map.
	 *
	 * This non-virtual base class implementation should be called directly from
	 * the sublcass implementation, eg
	 *
	 *    MM_ObjectScanner::initialize(env);
	 *
	 * @param[in] env Current environment
	 * @see getNextSlotMap()
	 * @see putNextSlotMapBit()
	 */
	MMINLINE void
	initialize(MM_EnvironmentBase *env)
	{
		uintptr_t hotField = (_scanMap & _hotFieldsDescriptor);
		if (0 != hotField) {
			uintptr_t zeros = MM_Bits::leadingZeroes(hotField);
			_slotObject.writeAddressToSlot(&_scanPtr[zeros]);
			_scanMap &= ~((uintptr_t)1 << zeros);
#if defined(OMR_GC_LEAF_BITS)
			_leafMap &= ~((uintptr_t)1 << zeros);
#endif /* defined(OMR_GC_LEAF_BITS) */
			_hotScanPtr = _scanPtr;
			_scanPtr = NULL;
		}
	}

	/**
	 * Helper function can be used to rebuild bit map of reference fields in
	 * implementation of getNextSlotMap(). Simply call this method once for
	 * each object slot holding a reference pointer. Best to present reference
	 * fields in increasing address order until method returns false or no
	 * more fields.
	 *
	 * If the method returns false, the field presented in the call will not
	 * be included in the slot map and must be presented first in the next
	 * call to getNextSlotMap().
	 */
	MMINLINE bool
	putNextSlotMapBit(fomrobject_t *nextSlotAddress)
	{
		if (0 != _scanMap) {
			intptr_t bitOffset = nextSlotAddress - _scanPtr;
			if (_bitsPerScanMap < bitOffset) {
				_scanMap |= (uintptr_t)1 << bitOffset;
			} else {
				return false;
			}
		} else {
			_scanPtr = nextSlotAddress;
			_scanMap = 1;
		}
		return true;
	}

public:
	/**
	 * Leaf objects contain no reference slots (eg plain value object or empty array).
	 *
	 * @return true if the object to be scanned is a leaf object
	 */
	MMINLINE bool isLeafObject() { return (0 == _scanMap) && !hasMoreSlots(); }

	/**
	 * Return base pointer and slot bit map for next block of contiguous slots to be scanned. The
	 * base pointer must be fomrobject_t-aligned. Bits in the bit map are scanned in order of
	 * increasing significance, and the least significant bit maps to the slot at the returned
	 * base pointer.
	 *
	 * @param[out] scanMap the bit map for the slots contiguous with the returned base pointer
	 * @param[out] hasNextSlotMap set this to true if this method should be called again, false if this map is known to be last
	 * @return a pointer to the first slot mapped by the least significant bit of the map, or NULL if no more slots
	 */
	virtual fomrobject_t *getNextSlotMap(uintptr_t &scanMap, bool &hasNextSlotMap) = 0;

	/**
	 * Get the next object slot if one is available.
	 *
	 * @return a pointer to a slot object encapsulating the next object slot, or NULL if no next object slot
	 */
	MMINLINE GC_SlotObject *
	getNextSlot()
	{
		while (NULL != _scanPtr) {
			/* while there is at least one bit-mapped slot, advance scan ptr to a non-NULL slot or end of map */
			while ((0 != _scanMap) && ((0 == ((uintptr_t)1 & _scanMap)) || (0 == *_scanPtr))) {
				_scanPtr += 1;
				_scanMap >>= 1;
			}
			if (0 != _scanMap) {
				/* set up to return slot object for non-NULL slot at scan ptr and advance scan ptr */
				_slotObject.writeAddressToSlot(_scanPtr);
				_scanPtr += 1;
				_scanMap >>= 1;
				return &_slotObject;
			}

			/* slot bit map is empty -- try to refresh it */
			if (hasMoreSlots()) {
				bool hasNextSlotMap;
				_scanPtr = getNextSlotMap(_scanMap, hasNextSlotMap);
				if (!hasNextSlotMap) {
					setNoMoreSlots();
				}
			} else {
				_scanPtr = NULL;
			}
		}

		if (NULL != _hotScanPtr) {
			/* restore the scan pointer and return hot slot found in initialize() */
			_scanPtr = _hotScanPtr;
			_hotScanPtr = NULL;
			return &_slotObject;
		}

		return NULL;
	}

	/**
	 * The object scanner leaf optimization option is enabled by the OMR_GC_LEAF_BITS
	 * flag in omrcfg.h.
	 *
	 * TODO: Get OMR_GC_LEAF_BITS into omrcfg.h.
	 *
	 * Scanners with leaf optimization are used in marking contexts when parent
	 * object class includes information identifying which child slots refer to
	 * objects belonging to classes that contain no reference slots. The OMR
	 * marking scheme will use this information to optimize the marking work
	 * stack.
	 *
	 * If leaf information is available it should be expressed in the implementation
	 * of getNextSlotMap(uintptr_t &, uintptr_t &, bool &). This method is called to
	 * obtain a bit map of the contained leaf slots conforming to the reference slot
	 * map. An initial leaf map is provided to the GC_ObjectScanner constructor and
	 * it is refreshed when required from getNextSlot(bool &).
	 */

#if defined(OMR_GC_LEAF_BITS)
	/**
	 * Return base pointer and slot bit map for next block of contiguous slots to be scanned. The
	 * base pointer must be fomrobject_t-aligned. Bits in the bit map are scanned in order of
	 * increasing significance, and the least significant bit maps to the slot at the returned
	 * base pointer.
	 *
	 * @param[out] scanMap the bit map for the slots contiguous with the returned base pointer
	 * @param[out] leafMap the leaf bit map for the slots contiguous with the returned base pointer
	 * @param[out] hasNextSlotMap set this to true if this method should be called again, false if this map is known to be last
	 * @return a pointer to the first slot mapped by the least significant bit of the map, or NULL if no more slots
	 */
	virtual fomrobject_t *getNextSlotMap(uintptr_t &scanMap, uintptr_t &leafMap, bool &hasNextSlotMap) = 0;

	/**
	 * Get the next object slot if one is available.
	 *
	 * @param[out] *isLeafPointer will be true if the slot refers to a leaf object
	 * @return a pointer to a slot object encapsulating the next object slot, or NULL if no next object slot
	 */
	MMINLINE GC_SlotObject *
	getNextSlot(bool &isLeafSlot)
	{
		while (NULL != _scanPtr) {
			/* while there is at least one bit-mapped slot, advance scan ptr to a non-NULL slot or end of map */
			while ((0 != _scanMap) && ((0 == (1 & _scanMap)) || (0 == *_scanPtr))) {
				_scanPtr += 1;
				_scanMap >>= 1;
				_leafMap >>= 1;
			}
			if (0 != _scanMap) {
				/* set up to return slot object for non-NULL slot at scan ptr and advance scan ptr */
				_slotObject.writeAddressToSlot(_scanPtr);
				isLeafSlot = (0 != ((uintptr_t)1 & _leafMap));
				_scanPtr += 1;
				_scanMap >>= 1;
				_leafMap >>= 1;
				return &_slotObject;
			}

			/* slot bit map is empty -- try to refresh it */
			if (hasMoreSlots()) {
				bool hasNextSlotMap;
				_scanPtr = getNextSlotMap(_scanMap, _leafMap, hasNextSlotMap);
				if (!hasNextSlotMap) {
					setNoMoreSlots();
				}
			} else {
				_scanPtr = NULL;
				setNoMoreSlots();
			}
		}

		if (NULL != _hotScanPtr) {
			/* restore the scan pointer and return hot slot found in initialize() */
			isLeafSlot = (0 != (((uintptr_t)1 << (_hotScanPtr - _scanPtr)) & _leafMap));
			_scanPtr = _hotScanPtr;
			_hotScanPtr = NULL;
			return &_slotObject;
		}

		isLeafSlot = true;
		return NULL;
	}
#endif /* defined(OMR_GC_LEAF_BITS) */

	/**
	 * Informational, relating to scanning context (_flags)
	 */
	MMINLINE void setNoMoreSlots() { _flags |= (uintptr_t)GC_ObjectScanner::noMoreSlots; }

	MMINLINE bool hasMoreSlots() { return 0 == (GC_ObjectScanner::noMoreSlots & _flags); }

	MMINLINE omrobjectptr_t const getParentObject() { return _parentObjectPtr; }

	MMINLINE static bool isRootScan(uintptr_t flags) { return (0 != (scanRoots & flags)); }

	MMINLINE bool isRootScan() { return (0 != (scanRoots & _flags)); }

	MMINLINE static bool isHeapScan(uintptr_t flags) { return (0 != (scanHeap & flags)); }

	MMINLINE bool isHeapScan() { return (0 != (scanHeap & _flags)); }

	MMINLINE static bool isIndexableObject(uintptr_t flags) { return (0 != (indexableObject & flags)); }

	MMINLINE bool isIndexableObject() { return (0 != (indexableObject & _flags)); }

	MMINLINE static bool isIndexableObjectNoSplit(uintptr_t flags) { return (0 != (indexableObjectNoSplit & flags)); }

	MMINLINE bool isIndexableObjectNoSplit() { return (0 != (indexableObjectNoSplit & _flags)); }

	MMINLINE void clearHeadObjectScanner() { _flags &= ~headObjectScanner; }

	MMINLINE bool isHeadObjectScanner() { return (0 != (headObjectScanner & _flags)); }
};

#endif /* OBJECTSCANNER_HPP_ */
