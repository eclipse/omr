/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

/**
 * @file
 * @ingroup GC_Base
 */

#if !defined(HEAPMAPWORDITERATOR_HPP_)
#define HEAPMAPWORDITERATOR_HPP_

#include "omrcfg.h"
#include "ModronAssertions.h"

#include "Bits.hpp"
#include "HeapMap.hpp"

enum {
	J9MODRON_HEAP_BYTES_PER_UDATA_OF_HEAP_MAP = (J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * sizeof(uintptr_t) * sizeof(uintptr_t) * BITS_IN_BYTE)
};

class MM_HeapMapWordIterator
{
	/* Data Members */
private:
	uintptr_t _cache;	/**< The uintptr_t-size chunk of the mark map which the receiver was instantiated to read */
	uintptr_t *_heapSlotCurrent;  /**< Current heap slot that corresponds to the heap map bit index being scanned */
protected:
public:

	/* Member Functions */
private:
protected:
public:	
	MMINLINE omrobjectptr_t nextObject()
	{
		omrobjectptr_t nextObject = NULL;
		if (0 != _cache) {
			uintptr_t leadingZeroes = MM_Bits::leadingZeroes(_cache);
			_heapSlotCurrent += J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * leadingZeroes;
			_cache >>= leadingZeroes;
			nextObject = (omrobjectptr_t) _heapSlotCurrent;
			/* skip to the next bit in _heapSlotCurrent and in _cache */
			_heapSlotCurrent += J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT;
			_cache >>= 1;
		}
		return nextObject;
	}
	
	MMINLINE MM_HeapMapWordIterator(MM_HeapMap *heapMap, void *heapCardAddress)
	{
		uintptr_t heapOffsetInBytes = (uintptr_t)heapCardAddress - (uintptr_t)heapMap->getHeapBase();

		/* Some compilers (z/OS) will read the mark map word multiple times if it isn't marked volatile.
		 * Other threads modifying the mark map can cause inconsistencies and the iterator will return invalid results.
		 */
		volatile uintptr_t *mapPointer = (volatile uintptr_t *) ( ((uint8_t *)heapMap->getHeapMapBits()) + (heapOffsetInBytes / J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT) );
		Assert_MM_true(0 == ((uintptr_t)mapPointer & (sizeof(uintptr_t) - 1)));
		_cache = *mapPointer;
		_heapSlotCurrent = (uintptr_t *)heapCardAddress;
	}
	
	MMINLINE MM_HeapMapWordIterator(uintptr_t heapMapWord, void *heapCardAddress)
	{
		_cache = heapMapWord;
		_heapSlotCurrent = (uintptr_t *)heapCardAddress;
	}

};

#endif /* HEAPMAPWORDITERATOR_HPP_ */
