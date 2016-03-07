/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(MARKMAP_HPP_)
#define MARKMAP_HPP_

#include "omrcfg.h"
#include "omr.h"

#include "HeapMap.hpp"

#define J9MODRON_HEAP_SLOTS_PER_MARK_BIT  J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT
#define J9MODRON_HEAP_SLOTS_PER_MARK_SLOT J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT
#define J9MODRON_HEAP_BYTES_PER_MARK_BYTE J9MODRON_HEAP_BYTES_PER_HEAPMAP_BYTE

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
