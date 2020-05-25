/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#if !defined(HEAPMAP_HPP_)
#define HEAPMAP_HPP_

/*
 * @ddr_namespace: map_to_type=MM_HeapMap
 */

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "objectdescription.h"
#if defined(OMR_GC_SEGREGATED_HEAP)
#include "sizeclasses.h"
#endif /* OMR_GC_SEGREGATED_HEAP */

#include "AtomicOperations.hpp"
#include "BaseVirtual.hpp"
#include "Bits.hpp"
#include "EnvironmentBase.hpp"
#include "MemoryHandle.hpp"

class MM_GCExtensionsBase;
class MM_HeapRegionDescriptor;

#if defined(OMR_ENV_DATA64)
#define J9MODRON_HEAPMAP_BIT_MASK ((uintptr_t)511)
#define J9MODRON_HEAPMAP_BIT_SHIFT ((uintptr_t)3)
#define J9MODRON_HEAPMAP_INDEX_SHIFT ((uintptr_t)9)
#define J9MODRON_HEAPMAP_SLOT_MASK ((uintptr_t)0xFFFFFFFFFFFFFFFF)
#define J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT ((uintptr_t)1)
#else /* OMR_ENV_DATA64 */
#if defined(OMR_GC_MINIMUM_OBJECT_SIZE)
#define J9MODRON_HEAPMAP_BIT_MASK ((uintptr_t)255)
#define J9MODRON_HEAPMAP_BIT_SHIFT ((uintptr_t)3)
#define J9MODRON_HEAPMAP_INDEX_SHIFT ((uintptr_t)8)
#define J9MODRON_HEAPMAP_SLOT_MASK ((uintptr_t)0xFFFFFFFF)
#define J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT ((uintptr_t)2)
#else /* defined(OMR_GC_MINIMUM_OBJECT_SIZE) */
#define J9MODRON_HEAPMAP_BIT_MASK ((uintptr_t)127)
#define J9MODRON_HEAPMAP_BIT_SHIFT ((uintptr_t)2)
#define J9MODRON_HEAPMAP_INDEX_SHIFT ((uintptr_t)7)
#define J9MODRON_HEAPMAP_SLOT_MASK ((uintptr_t)0xFFFFFFFF)
#define J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT ((uintptr_t)1)
#endif /* defined(OMR_GC_MINIMUM_OBJECT_SIZE)*/
#endif /* OMR_ENV_DATA64*/

#if defined(OMR_ENV_DATA64)
#define J9MODRON_HEAPMAP_LOG_SIZEOF_UDATA 0x6
#else /* OMR_ENV_DATA64 */
#define J9MODRON_HEAPMAP_LOG_SIZEOF_UDATA 0x5
#endif /* OMR_ENV_DATA64 */

#define BITS_IN_BYTE 8
#define J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT (J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * J9BITS_BITS_IN_SLOT)
#define J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT (J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * sizeof(uintptr_t))
#define J9MODRON_HEAP_BYTES_PER_HEAPMAP_BYTE (J9MODRON_HEAP_BYTES_PER_HEAPMAP_BIT * BITS_IN_BYTE)
#define J9MODRON_HEAP_BYTES_PER_HEAPMAP_SLOT (J9MODRON_HEAP_BYTES_PER_HEAPMAP_BYTE * sizeof(uintptr_t))

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_HeapMap : public MM_BaseVirtual
{
/*
 * Data members
 */
private:
	const bool _useCompressedHeapMap;	/* selects compressed/uncompressed heap map for realtime/nonrealtime contexts */

protected:
	const uintptr_t _heapMapIndexShift;	/* number of low-order bits to be shifted out of heap address to obtain heap map slot index */
	const uintptr_t _heapMapBitMask;	/* bit mask for capturing bit index within heap map slot from (unshifted) heap address */
	const uintptr_t _heapMapBitShift;	/* number of low-order bits to be shifted out of captured bit index to obtain actual bit index */

	MM_GCExtensionsBase *_extensions;

	void *_heapBase;
	void *_heapTop;
	
	MM_MemoryHandle	_heapMapMemoryHandle;
	uintptr_t _heapMapBaseDelta;
	uintptr_t *_heapMapBits;
	
	uintptr_t _maxHeapSize;

public:
	
/*
 * Function members
 */
private:
protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	uintptr_t getMaximumHeapMapSize(MM_EnvironmentBase *env);
	uintptr_t convertHeapIndexToHeapMapIndex(MM_EnvironmentBase *env, uintptr_t size, uintptr_t roundTo);
	
public:
	void kill(MM_EnvironmentBase *env);
	
	virtual bool heapAddRange(MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	
	MMINLINE void *getHeapBase() { return _heapBase; }

	MMINLINE uintptr_t *getHeapMapBits() { return _heapMapBits; }
	MMINLINE const uintptr_t *getHeapMapBits() const { return _heapMapBits; }

	MMINLINE uintptr_t getObjectGrain() { return ((uintptr_t)1) << _heapMapBitShift; };
		
	MMINLINE void
	getSlotIndexAndMask(omrobjectptr_t objectPtr, uintptr_t *slotIndex, uintptr_t *bitMask)
	{
		uintptr_t slot = ((uintptr_t)objectPtr) - _heapMapBaseDelta;
		// _heapBitMask is in order of 2^n -1, and val % 2^n = val & (2^n -1)
		uintptr_t bitIndex = (slot & _heapMapBitMask) >> _heapMapBitShift;
		*bitMask = (((uintptr_t)1) << bitIndex);
		*slotIndex = slot >> _heapMapIndexShift;
	}

	MMINLINE uintptr_t
	getBitIndex(omrobjectptr_t objectPtr)
	{
		uintptr_t heapOffsetInBytes = ((uintptr_t)objectPtr) - _heapMapBaseDelta;

		/* Calculate the starting mark bit location */
		uintptr_t bitIndex = heapOffsetInBytes & _heapMapBitMask;
		bitIndex >>= _heapMapBitShift;

		return bitIndex;
	}
	
	MMINLINE uintptr_t 
	getSlotIndex(omrobjectptr_t objectPtr) const
	{
		uintptr_t slotIndex = ((uintptr_t)objectPtr) - _heapMapBaseDelta;
		slotIndex >>= _heapMapIndexShift;
		return slotIndex;
	}

	MMINLINE bool 
	isBitSet(omrobjectptr_t objectPtr)
	{
		uintptr_t slotIndex, bitMask;
		
		/*  Just check if the lead bit of the object is set */
		getSlotIndexAndMask(objectPtr, &slotIndex, &bitMask);
	
		return (_heapMapBits[slotIndex] & bitMask) ? true : false;
	}

	MMINLINE bool 
	atomicSetBit(omrobjectptr_t objectPtr)
	{
		uintptr_t slotIndex, bitMask;
		/* Ensure compiler does not optimize away assign into oldValue */
		volatile uintptr_t *slotAddress;
		uintptr_t oldValue;
			
		getSlotIndexAndMask(objectPtr, &slotIndex, &bitMask);
		slotAddress = &(_heapMapBits[slotIndex]);
		
		do {
			oldValue = *slotAddress;
			if(oldValue & bitMask) {
				return false;
			}
		} while(oldValue != MM_AtomicOperations::lockCompareExchange(slotAddress,
																	 oldValue, 
																	 oldValue | bitMask));
		return true;
	}

	MMINLINE void 
	atomicSetSlot(uintptr_t slotIndex, uintptr_t slotValue)
	{
		/* Ensure compiler does not optimize away assign into oldValue */
		volatile uintptr_t *slotAddress = &(_heapMapBits[slotIndex]);
		uintptr_t oldValue;
		
		do {
			oldValue = *slotAddress;
		} while(oldValue != MM_AtomicOperations::lockCompareExchange(slotAddress,
																	 oldValue, 
																	 oldValue | slotValue));
	}

	MMINLINE uintptr_t 
	getSlot(uintptr_t slotIndex)
	{
		return _heapMapBits[slotIndex];
	}
	
	MMINLINE void 
	setSlot(uintptr_t slotIndex, uintptr_t slotValue)
	{
		_heapMapBits[slotIndex] = slotValue;
	}

	MMINLINE bool 
	setBit(omrobjectptr_t objectPtr)
	{
		uintptr_t slotIndex, bitMask;
		uintptr_t *slotAddress;
		
		getSlotIndexAndMask(objectPtr, &slotIndex, &bitMask);
		slotAddress = &(_heapMapBits[slotIndex]);
		
		if(*slotAddress & bitMask) {
			return false;
		}
		*slotAddress |= bitMask;
		return true;
	}

	MMINLINE bool 
	clearBit(omrobjectptr_t objectPtr)
	{
		uintptr_t slotIndex, bitMask;
		uintptr_t *slotAddress;
		
		getSlotIndexAndMask(objectPtr, &slotIndex, &bitMask);
		slotAddress = &(_heapMapBits[slotIndex]);
	
		/* If bit set then clear it */	
		if(*slotAddress & bitMask) {
			bitMask = ~bitMask;
			*slotAddress &= bitMask;
			return true;
		}
		
		return false;
	}
	
	MMINLINE uintptr_t *
	getSlotPtr(uintptr_t index)
	{
		return &(getHeapMapBits()[index]);
	}

	MMINLINE const uintptr_t *
	getSlotPtr(uintptr_t index) const
	{
		return &(getHeapMapBits()[index]);
	}

	MMINLINE uintptr_t *
	getSlotPtrForAddress(omrobjectptr_t address)
	{
		return getSlotPtr(getSlotIndex(address));
	}

	MMINLINE const uintptr_t *
	getSlotPtrForAddress(omrobjectptr_t address) const
	{
		return getSlotPtr(getSlotIndex(address));
	}

	uintptr_t numberBitsInRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress);

	/**
	 * Set all heap map bits for a specified heap range either ON or OFF
	 *
	 * @param lowAddress - base of region of heap whoose heap map bits are to be set
	 * @param highAddress - top of region of heap whoose heap map bits to be set
	 * @param clear - if TRUE set BITS OFF; otherwise set bits ON
	 * @return the amount of memory actually cleard
	 */
	uintptr_t setBitsInRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress, bool clear);

	/**
	 * Set all heap map bits for a specified heap range either ON or OFF
	 *
	 * @param region, for which mark map should be set
	 * @param clear - if TRUE set BITS OFF; otherwise set bits ON
	 * @return the amount of memory actually cleard
	 */
	uintptr_t setBitsForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, bool clear);

	/**
	 * Check if the mark map of a region is cleared.
	 * @param env[in] GC thread
 	 * @param region for which mark map should be checked
	 * @return true if cleared
	 */
	bool checkBitsForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region);
	
	/**
	 * Create a HeapMap object.
	 */
#if defined(OMR_GC_SEGREGATED_HEAP)
#define J9MODRON_HEAPMAP_SELECT_INDEX_SHIFT(compress) (compress ? ((J9MODRON_HEAPMAP_LOG_SIZEOF_UDATA) + (OMR_SIZECLASSES_LOG_SMALLEST)) : (J9MODRON_HEAPMAP_INDEX_SHIFT))
#define J9MODRON_HEAPMAP_SELECT_BIT_MASK(compress) ((1 << (J9MODRON_HEAPMAP_SELECT_INDEX_SHIFT(compress))) - 1)
#define J9MODRON_HEAPMAP_SELECT_BIT_SHIFT(compress) (compress ? (OMR_SIZECLASSES_LOG_SMALLEST) : (J9MODRON_HEAPMAP_BIT_SHIFT))
#else
#define J9MODRON_HEAPMAP_SELECT_INDEX_SHIFT(compress) (J9MODRON_HEAPMAP_INDEX_SHIFT)
#define J9MODRON_HEAPMAP_SELECT_BIT_MASK(compress) ((1 << (J9MODRON_HEAPMAP_SELECT_INDEX_SHIFT(compress))) - 1)
#define J9MODRON_HEAPMAP_SELECT_BIT_SHIFT(compress) (J9MODRON_HEAPMAP_BIT_SHIFT)
#endif /* OMR_GC_SEGREGATED_HEAP */

	MM_HeapMap(MM_EnvironmentBase *env, uintptr_t maxHeapSize, bool useCompressedHeapMap = false) :
		MM_BaseVirtual()
		,_useCompressedHeapMap(useCompressedHeapMap)
		,_heapMapIndexShift(J9MODRON_HEAPMAP_SELECT_INDEX_SHIFT(useCompressedHeapMap))
		,_heapMapBitMask(J9MODRON_HEAPMAP_SELECT_BIT_MASK(useCompressedHeapMap))
		,_heapMapBitShift(J9MODRON_HEAPMAP_SELECT_BIT_SHIFT(useCompressedHeapMap))
		,_extensions(env->getExtensions())
		,_heapBase(NULL)
		,_heapTop(NULL)
		,_heapMapMemoryHandle()
		,_heapMapBaseDelta(0)
		,_heapMapBits(NULL)
		,_maxHeapSize(maxHeapSize)
	{
		_typeId = __FUNCTION__;
	}
#undef J9MODRON_HEAPMAP_SELECT_INDEX_SHIFT
#undef J9MODRON_HEAPMAP_SELECT_BIT_MASK
#undef J9MODRON_HEAPMAP_SELECT_BIT_SHIFT
};

#endif /* HEAPMAP_HPP_ */
