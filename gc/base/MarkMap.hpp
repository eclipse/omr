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

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(MARKMAP_HPP_)
#define MARKMAP_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "omrmodroncore.h"

#include "HeapMap.hpp"

#define J9MODRON_HEAP_SLOTS_PER_MARK_BIT  J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT
#define J9MODRON_HEAP_SLOTS_PER_MARK_SLOT J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT
#define J9MODRON_HEAP_BYTES_PER_MARK_BYTE J9MODRON_HEAP_BYTES_PER_HEAPMAP_BYTE

#define BITS_PER_BYTE 8

class MM_EnvironmentBase;

class MM_MarkMap : public MM_HeapMap
{
private:
	bool _isMarkMapValid; /** < Is this mark map valid */
	
public:
	MMINLINE bool isMarkMapValid() const { return _isMarkMapValid; }
	MMINLINE void setMarkMapValid(bool isMarkMapValid) {  _isMarkMapValid = isMarkMapValid; }

 	static MM_MarkMap *newInstance(MM_EnvironmentBase *env, uintptr_t maxHeapSize);
 	
 	void initializeMarkMap(MM_EnvironmentBase *env);

	MMINLINE void *getMarkBits() { return _heapMapBits; };
 	
	MMINLINE uintptr_t getHeapMapBaseRegionRounded() { return _heapMapBaseDelta; }

	MMINLINE void
	getSlotIndexAndBlockMask(omrobjectptr_t objectPtr, uintptr_t *slotIndex, uintptr_t *bitMask, bool lowBlock)
	{
		uintptr_t slot = ((uintptr_t)objectPtr) - _heapMapBaseDelta;
		uintptr_t bitIndex = (slot & _heapMapBitMask) >> _heapMapBitShift;
		if (lowBlock) {
			*bitMask = (((uintptr_t)-1) >> (J9BITS_BITS_IN_SLOT - 1 - bitIndex));
		} else {
			*bitMask = (((uintptr_t)-1) << bitIndex);
		}
		*slotIndex = slot >> _heapMapIndexShift;
	}

	MMINLINE void
	setMarkBlock(uintptr_t slotIndexLow, uintptr_t slotIndexHigh, uintptr_t value)
	{
		uintptr_t slotIndex;

		for (slotIndex = slotIndexLow; slotIndex <= slotIndexHigh; slotIndex++) {
			_heapMapBits[slotIndex] = value;
		}
	}

	MMINLINE void
	markBlockAtomic(uintptr_t slotIndex, uintptr_t bitMask)
	{
		volatile uintptr_t *slotAddress;
		uintptr_t oldValue;

		slotAddress = &(_heapMapBits[slotIndex]);

		do {
			oldValue = *slotAddress;
		} while(oldValue != MM_AtomicOperations::lockCompareExchange(slotAddress,
			oldValue,
			oldValue | bitMask));
	}

	MMINLINE uintptr_t
	getFirstCellByMarkSlotIndex(uintptr_t slotIndex)
	{
		return _heapMapBaseDelta + (slotIndex << _heapMapIndexShift);
	}

	/**
	 * check MarkMap if there is any liveObjects in the Card
	 * this function assumes that card covers exactly 512 bytes.
	 * @param heapAddress has to be card size aligned
	 */
	MMINLINE bool
	areAnyLiveObjectsInCard(void* heapAddress) const
	{
#if (8 != BITS_PER_BYTE) || (9 != CARD_SIZE_SHIFT)
#error Card size has to be exactly 512 bytes
#endif
		return 0 != *(uint64_t*)getSlotPtrForAddress((omrobjectptr_t) heapAddress);
	}

	/**
	 * Create a MarkMap object.
	 */
	MM_MarkMap(MM_EnvironmentBase *env, uintptr_t maxHeapSize) :
		MM_HeapMap(env, maxHeapSize, env->getExtensions()->isSegregatedHeap())
		, _isMarkMapValid(false)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* MARKMAP_HPP_ */
