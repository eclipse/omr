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

#if !defined(HEAPLINKEDFREEHEADER_HPP_)
#define HEAPLINKEDFREEHEADER_HPP_

#include "omrcomp.h"
#include "modronbase.h"
/* #include "ModronAssertions.h" -- removed for now because it causes a compile error in TraceOutput.cpp on xlC */

#include"AtomicOperations.hpp"

/* Split pointer for all compressed platforms */
#if defined(OMR_INTERP_COMPRESSED_OBJECT_HEADER)
#define SPLIT_NEXT_POINTER
#endif

/**
 * For VMs using the new header shape (interp_newHeaderShape), _next lines
 * up instead object header fields. In this case, the picture on full 64-bit
 * resembles the compressed header:
 *
 * +-------+
 * + clazz |
 * +-------+-------+---------------+
 * + _next |_nextHi| _size         |
 * +-------+-------+---------------+
 *
 */

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
#if defined(SPLIT_NEXT_POINTER)
	uint32_t _next; /**< tagged pointer to the next free list entry, or a tagged pointer to NULL */
	uint32_t _nextHighBits; /**< the high 32 bits of the next pointer on 64-bit builds */
#else /* defined(SPLIT_NEXT_POINTER) */
	uintptr_t _next; /**< tagged pointer to the next free list entry, or a tagged pointer to NULL */
#endif /* defined(SPLIT_NEXT_POINTER) */
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
	getNextImpl()
	{
#if defined(SPLIT_NEXT_POINTER)		
		uintptr_t lowBits = _next;
		uintptr_t highBits = _nextHighBits;
		return (highBits << 32) | lowBits;
#else /* defined(SPLIT_NEXT_POINTER) */
		return _next;
#endif /* defined(SPLIT_NEXT_POINTER) */
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
	setNextImpl(uintptr_t value)
	{
#if defined(SPLIT_NEXT_POINTER)
		_next = (uint32_t)value;
		_nextHighBits = (uint32_t)(value >> 32);
#else /* defined(SPLIT_NEXT_POINTER) */
		_next = value;
#endif /* defined(SPLIT_NEXT_POINTER) */
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
	MMINLINE MM_HeapLinkedFreeHeader *getNext() {
		/* Assert_MM_true(0 != ((getNextImpl()) & J9_GC_OBJ_HEAP_HOLE)); */
		return (MM_HeapLinkedFreeHeader*)((getNextImpl()) & ~((uintptr_t)J9_GC_OBJ_HEAP_HOLE_MASK));
	}

	/**
	 * Set the next free header in the linked list and mark the receiver as a multi-slot hole
	 * Set to NULL to terminate the list.
	 */
	MMINLINE void setNext(MM_HeapLinkedFreeHeader* freeEntryPtr) { setNextImpl( ((uintptr_t)freeEntryPtr) | ((uintptr_t)J9_GC_MULTI_SLOT_HOLE) ); }

	/**
	 * Set the next free header in the linked list, preserving the type of the receiver
	 * Set to NULL to terminate the list.
	 */
	MMINLINE void updateNext(MM_HeapLinkedFreeHeader* freeEntryPtr) { setNextImpl( ((uintptr_t)freeEntryPtr) | (getNextImpl() & (uintptr_t)J9_GC_OBJ_HEAP_HOLE_MASK) ); }

	/**
	 * Get the size in bytes of this free entry. The size is measured
	 * from the beginning of the header.
	 * @return size in bytes
	 */
	MMINLINE uintptr_t getSize() { return _size; }

	/**
	 * Set the size in bytes of this free entry.
	 */
	MMINLINE void setSize(uintptr_t size) {
		_size = size;
	}

	/**
	 * Expand this entry by the specified number of bytes.
	 */
	MMINLINE void expandSize(uintptr_t increment) {
		_size += increment;
	}

	/**
	 * Return the address immediately following the free section
	 * described by this header
	 * @return address following this free section
	 */
	MMINLINE MM_HeapLinkedFreeHeader* afterEnd() { return (MM_HeapLinkedFreeHeader*)( ((uintptr_t)this) + getSize() ); }

	/**
	 * Mark the specified region of memory as all single slot holes
	 * @param[in] addrBase the address to start marking from
	 * @param freeEntrySize the number of bytes to be consumed
	 */
	MMINLINE static void
	fillWithSingleSlotHoles(void* addrBase, uintptr_t freeEntrySize)
	{
#if defined(SPLIT_NEXT_POINTER)
		uint32_t *freeSlot = (uint32_t *) addrBase;
		while(freeEntrySize) {
			*freeSlot++ = J9_GC_SINGLE_SLOT_HOLE;
			freeEntrySize -= sizeof(uint32_t);
		}
#else /* defined(SPLIT_NEXT_POINTER) */
		uintptr_t *freeSlot = (uintptr_t*) addrBase;
		while(freeEntrySize) {
			*freeSlot++ = J9_GC_SINGLE_SLOT_HOLE;
			freeEntrySize -= sizeof(uintptr_t);
		}
#endif /* defined(SPLIT_NEXT_POINTER) */
	}

	/**
	 * Mark the specified region of memory as walkable dark matter
	 * @param[in] addrBase the address where the hole begins
	 * @param[in] freeEntrySize the number of bytes to be consumed (must be a multiple of sizeof(uintptr_t))
	 * @return The header written or null if the space was too small and was filled with single-slot holes
	 */
	MMINLINE static MM_HeapLinkedFreeHeader*
	fillWithHoles(void* addrBase, uintptr_t freeEntrySize)
	{
		MM_HeapLinkedFreeHeader *freeEntry = NULL;
		if (freeEntrySize < sizeof(MM_HeapLinkedFreeHeader)) {
			/* Entry will be abandoned. Recycle the remainder as single slot entries */
			fillWithSingleSlotHoles(addrBase, freeEntrySize);
		} else {
			/* this is too big to use single slot holes so generate an AOL-style hole (note that this is not correct for other allocation schemes) */
			freeEntry = (MM_HeapLinkedFreeHeader *)addrBase;

			freeEntry->setNext(NULL);
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
	linkInAsHead(volatile uintptr_t *currentHead, MM_HeapLinkedFreeHeader* nextHead)
	{
		uintptr_t oldValue, newValue;
		do {
			oldValue = *currentHead;
			newValue = MM_AtomicOperations::lockCompareExchange(currentHead, oldValue, (uintptr_t)nextHead);

		} while (oldValue != newValue);
		nextHead->setNext((MM_HeapLinkedFreeHeader *)newValue);
	}

protected:
private:

};

#undef SPLIT_NEXT_POINTER

#endif /* HEAPLINKEDFREEHEADER_HPP_ */
