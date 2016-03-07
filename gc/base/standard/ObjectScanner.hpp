/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2015
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

#if !defined(OBJECTSCANNER_HPP_)
#define OBJECTSCANNER_HPP_

#include "ModronAssertions.h"
#include "objectdescription.h"

#include "BaseVirtual.hpp"
#include "EnvironmentStandard.hpp"
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
	fomrobject_t *_scanPtr;					/**< Pointer to base of object slots mapped by current _scanMap */
	GC_SlotObject _slotObject;				/**< Create own SlotObject class to provide output */
	uintptr_t _flags;						/**< Scavenger context flags (scanRoots, scanHeap, ...) */
	
public:
	/**
	 *  Instantiation flags used to specialize scanner for specific scavenger operations
	 */
	enum InstanceFlags
	{
		scanRoots = 1			/* scavenge roots phase -- scan & copy/forward root objects */
		, scanHeap = 2			/* scavenge heap phase -- scan  & copy/forward objects reachable from scanned roots */
		, indexableObject = 4	/* this is set for array object scanners */
		, noMoreSlots = 8		/* this is set when object has more no slots to scan past current bitmap */
	};

	/* Member Functions */
private:

protected:
	/**
	 * Constructor.
	 *
	 * @param[in] env The environment for the scanning thread
	 * @param[in] parentObjectPtr The object to be scanned
	 * @param[in] scanPtr The first slot contained in the object to be scanned
	 * @param[in] scanMap Bit map marking object reference slots, with least significant bit mapped to slot at scanPtr
	 * @param[in] flags A bit mask comprised of InstanceFlags
	 */
	GC_ObjectScanner(MM_EnvironmentStandard *env, omrobjectptr_t parentObjectPtr, fomrobject_t *scanPtr, uintptr_t scanMap, uintptr_t flags)
		: MM_BaseVirtual()
		, _parentObjectPtr(parentObjectPtr)
		, _scanMap(scanMap)
		, _scanPtr(scanPtr)
		, _slotObject(env->getOmrVM(), NULL)
		, _flags(flags)
	{
		_typeId = __FUNCTION__;
	}
	
	/**
	 * Set up the scanner.
	 * @param[in] env Current environment
	 */
	MMINLINE void
	initialize(MM_EnvironmentStandard *env)
	{
	}

public:
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
	 * Leaf objects contain no reference slots (eg plain value object or empty array).
	 *
	 * @return true if the object to be scanned is a leaf object
	 */
	MMINLINE bool isLeafObject() { return (0 == _scanMap) && !hasMoreSlots(); }

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
			while ((0 != _scanMap) && ((0 == (1 & _scanMap)) || (0 == *_scanPtr))) {
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
				_flags = setNoMoreSlots(_flags, !(hasNextSlotMap));
			} else {
				_scanPtr = NULL;
				_flags = setNoMoreSlots(_flags, true);
			}
		}

		return NULL;
	}

	/**
	 * Informational, relating to scanning context (_flags)
	 */
	MMINLINE bool hasMoreSlots() { return 0 == (GC_ObjectScanner::noMoreSlots & _flags); }

	MMINLINE static uintptr_t setNoMoreSlots(uintptr_t flags, bool hasNoMoreSlots)
	{
		if (hasNoMoreSlots) {
			return flags | (uintptr_t)GC_ObjectScanner::noMoreSlots;
		} else {
			return flags & ~((uintptr_t)GC_ObjectScanner::noMoreSlots);
		}
	}

	MMINLINE omrobjectptr_t const getParentObject() { return _parentObjectPtr; }

	MMINLINE static bool isRootScan(uintptr_t flags) { return (0 != (scanRoots & flags)); }

	MMINLINE bool isRootScan() { return (0 != (scanRoots & _flags)); }

	MMINLINE static bool isHeapScan(uintptr_t flags) { return (0 != (scanHeap & flags)); }

	MMINLINE bool isHeapScan() { return (0 != (scanHeap & _flags)); }

	MMINLINE static bool isIndexableObject(uintptr_t flags) { return (0 != (indexableObject & flags)); }

	MMINLINE bool isIndexableObject() { return (0 != (indexableObject & _flags)); }
};

#endif /* OBJECTSCANNER_HPP_ */
