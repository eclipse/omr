/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(HEAPLINKEDFREEHEADER_HPP_)
#define HEAPLINKEDFREEHEADER_HPP_

#include "omrcomp.h"
#include "modronbase.h"
/* #include "ModronAssertions.h" -- removed for now because it causes a compile error in TraceOutput.cpp on xlC */

#include "AtomicOperations.hpp"

/**
 *  This class supercedes the J9GCModronLinkedFreeHeader struct used
 * in older versions of Modron.
 *
 * @ingroup GC_Base_Core
 */

class MM_HeapLinkedFreeHeader
{
public:
protected:
	uintptr_t _next; /**< tagged pointer to the next free list entry, or a tagged pointer to NULL */
	uintptr_t _size; /**< size in bytes (including header) of the free list entry */

private:

	/*
	 * CMVC 130331
	 * The following private member functions must be declared before the public functions
	 * due to a bug in an old version of GCC used on the HardHat Linux platforms. These
	 * functions can't be in-lined if they're not declared before the point they're used.
	 */
private:
	/**
	 * Fetch the value encoded in the next pointer.
	 * Since the encoding may be different depending on the header shape
	 * other functions in this class should use this helper rather than
	 * reading the field directly.
	 *
	 * @return the decoded next pointer, with any tag bits intact
	 */
	MMINLINE uintptr_t
	getNextImpl(bool compressed)
	{
		uintptr_t result = _next;
#if defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN)
		if (compressed) {
			/* On big endian compressed, the pointer has been stored
			 * endian-flipped, so flip it back. See MM_HeapLinkedFreeHeader::setNextImpl.
			 */
			result = (result >> 32) | (result << 32);
		}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN) */
		return result;
	}

	/**
	 * Encoded the specified value as the next pointer.
	 * Since the encoding may be different depending on the header shape
	 * other functions in this class should use this helper rather than
	 * writing the field directly.
	 *
	 * @parm value the value to be stored
	 */
	MMINLINE void
	setNextImpl(uintptr_t value, bool compressed)
	{
#if defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN)
		if (compressed) {
			/* On big endian compressed, endian flip the pointer so that the
			 * tag bits appear in the compressed class slot.
			 */
			value = (value >> 32) | (value << 32);
		}
#endif /* defined(OMR_GC_COMPRESSED_POINTERS) && !defined(OMR_ENV_LITTLE_ENDIAN) */
		_next = value;
	}

public:
	/**
	 * Convert a pointer to a dead object to a HeapLinkedFreeHeader.
	 */
	static MMINLINE MM_HeapLinkedFreeHeader *getHeapLinkedFreeHeader(void* pointer) { return (MM_HeapLinkedFreeHeader*)pointer; }

	/**
	 * Get the next free header in the linked list of free entries
	 * @return the next entry or NULL
	 */
	MMINLINE MM_HeapLinkedFreeHeader *getNext(bool compressed) {
		/* Assert_MM_true(0 != ((getNextImpl(compressed)) & J9_GC_OBJ_HEAP_HOLE)); */
		return (MM_HeapLinkedFreeHeader*)((getNextImpl(compressed)) & ~((uintptr_t)J9_GC_OBJ_HEAP_HOLE_MASK));
	}

	/**
	 * Set the next free header in the linked list and mark the receiver as a multi-slot hole
	 * Set to NULL to terminate the list.
	 */
	MMINLINE void setNext(MM_HeapLinkedFreeHeader* freeEntryPtr, bool compressed) { setNextImpl(((uintptr_t)freeEntryPtr) | ((uintptr_t)J9_GC_MULTI_SLOT_HOLE), compressed); }

	/**
	 * Set the next free header in the linked list, preserving the type of the receiver
	 * Set to NULL to terminate the list.
	 */
	MMINLINE void updateNext(MM_HeapLinkedFreeHeader* freeEntryPtr, bool compressed) { setNextImpl(((uintptr_t)freeEntryPtr) | (getNextImpl(compressed) & (uintptr_t)J9_GC_OBJ_HEAP_HOLE_MASK), compressed); }

	/**
	 * Get the size in bytes of this free entry. The size is measured
	 * from the beginning of the header.
	 * @return size in bytes
	 */
	MMINLINE uintptr_t getSize()
	{
		return _size;
	}

	/**
	 * Set the size in bytes of this free entry.
	 */
	MMINLINE void setSize(uintptr_t size)
	{
		_size = size;
	}

	/**
	 * Expand this entry by the specified number of bytes.
	 */
	MMINLINE void expandSize(uintptr_t increment)
	{
		_size += increment;
	}

	/**
	 * Return the address immediately following the free section
	 * described by this header
	 * @return address following this free section
	 */
	MMINLINE MM_HeapLinkedFreeHeader* afterEnd() { return (MM_HeapLinkedFreeHeader*)( ((uintptr_t)this) + getSize() ); }

	/**
	 * Mark the specified region of memory as walkable dark matter
	 * @param[in] addrBase the address where the hole begins
	 * @param[in] freeEntrySize the number of bytes to be consumed (must be a multiple of sizeof(uintptr_t))
	 * @return The header written or null if the space was too small and was filled with single-slot holes
	 */
	MMINLINE static MM_HeapLinkedFreeHeader*
	fillWithHoles(void* addrBase, uintptr_t freeEntrySize, bool compressed)
	{
		MM_HeapLinkedFreeHeader *freeEntry = NULL;
		if (freeEntrySize < sizeof(MM_HeapLinkedFreeHeader)) {
			/* Assert_MM_true(0 == (freeEntrySize % sizeof(uintptr_t))); */
			while (0 != freeEntrySize) {
				((MM_HeapLinkedFreeHeader*)addrBase)->setNextImpl(J9_GC_SINGLE_SLOT_HOLE, compressed);
				addrBase = (void*)(((uintptr_t*)addrBase) + 1);
				freeEntrySize -= sizeof(uintptr_t);
			}
		} else {
			/* this is too big to use single slot holes so generate an AOL-style hole (note that this is not correct for other allocation schemes) */
			freeEntry = (MM_HeapLinkedFreeHeader *)addrBase;

			freeEntry->setNext(NULL, compressed);
			freeEntry->setSize(freeEntrySize);
		}
		return freeEntry;
	}

	/**
	 * Links a new free header in at the head of the free list.
	 * @param[in/out] currentHead Input pointer is set to nextHead on return
	 * @param[in] nextHead Pointer to the new head of list, with nextHead->_next pointing to previous value of currentHead on return
	 * @note Caller must set nextHead->_size
	 */
	MMINLINE static void
	linkInAsHead(volatile uintptr_t *currentHead, MM_HeapLinkedFreeHeader* nextHead, bool compressed)
	{
		uintptr_t oldValue, newValue;
		do {
			oldValue = *currentHead;
			newValue = MM_AtomicOperations::lockCompareExchange(currentHead, oldValue, (uintptr_t)nextHead);

		} while (oldValue != newValue);
		nextHead->setNext((MM_HeapLinkedFreeHeader *)newValue, compressed);
	}

protected:
private:

};

#endif /* HEAPLINKEDFREEHEADER_HPP_ */

