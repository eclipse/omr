/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 * @ingroup GC_Base
 */

#if !defined(CARDTABLE_HPP_)
#define CARDTABLE_HPP_

#include "omrcfg.h"
#include "omrmodroncore.h"
#include "modronbase.h"
#if defined (OMR_GC_HEAP_CARD_TABLE)

#include "BaseVirtual.hpp"
#include "MemoryManager.hpp"

class MM_EnvironmentBase;
class MM_CardCleaner;
class MM_Heap;
class MM_HeapRegionDescriptor;

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Base
 */

/**
 * @todo Provide class documentation
 * @warn All card table functions assume EXCLUSIVE ranges, ie they take a base and top card where base card
 * is first card in range and top card is card immediately AFTER the end of the range.
 * @ingroup GC_Base
 */
class MM_CardTable : public MM_BaseVirtual
{
public:
protected:
	void *_heapAlloc;
private:
	MM_MemoryHandle _cardTableMemoryHandle;	/**< memory handle for array backing store */
	Card *_cardTableStart;
	Card *_cardTableVirtualStart;
	void *_heapBase; 


public:

	/**
	 * Returns the base address of the card table (looked up for inline write barriers, etc)
	 * @return The address of the card which tracks changes to the first byte in the heap
	 */
	Card *getCardTableStart() { return _cardTableStart; };

	/**
	 * Returns a "virtual" base address of the card table, effectively allowing it to be indexed by a shifted heap address, alone.
	 * This is calculated by subtracting the shifted address of the first byte of the heap from the base of the card table.
	 * @return A representation of the base of the card table which can be indexed by a shifted heap address
	 */
	Card *getCardTableVirtualStart() { return _cardTableVirtualStart; };

	/**
	 * @return Our cached copy of the base address of the heap.
	 */
	void *getHeapBase() { return _heapBase; };

	/**
	 * Checks if card is dirty or has a specific value
 	 * @param[in] env A GC thread
	 * @param[in] cardValue The value to compare with
 	 * @return true if card is dirty or has the specified value
	 */
	bool isDirtyOrValue(MM_EnvironmentBase *env, void *heapAddr, Card cardValue);

	/**
	 * Dirties the card backing the given object.  Called, primarily, by the write barrier.
	 * Mark the card in the card table given the address of an object which contains an updated
	 * reference if it is not already dirty.
	 * @param[in] env The thread which is attempting to write to obj
	 * @param[in] objectRef The object being modified
	 * @note Called by the write barrier so this must be fast (although the operation is inlined, in the JIT)
	 */
	void dirtyCard(MM_EnvironmentBase *env, omrobjectptr_t objectRef);

	/**
	 * Dirties the card backing the given object with the passed cardValue.  This is called during
	 * specific kinds of card cleaning or during overflow, in Tarok.
	 * @param[in] env A GC thread
	 * @param[in] objectRef The object whose card is to be dirtied
	 * @param[in] cardValue The value to write into the card (must not be CARD_CLEAN or CARD_INVALID)
	 */
	void dirtyCardWithValue(MM_EnvironmentBase *env, omrobjectptr_t objectRef, Card cardValue);

	/*
	 * Dirties the cards in the heap range
	 * @param[in] heapAddrFrom starting address (inclusive)
	 * @param[in] heapAddrTo ending address (exclusive)
	 */
	void dirtyCardRange(MM_EnvironmentBase *env, void *heapAddrFrom, void *heapAddrTo);

	/**
	 * Calculate the address of the card which tracks the changes to the given heap address.
	 * @param[in] env The thread making the request
	 * @param[in] heapAddr The heap address to convert
	 * @return The address of the card which tracks changes to the given heap address 
	 * @note only called from MM_ConcurrentCardTable
	 */
	Card *heapAddrToCardAddr(MM_EnvironmentBase *env, void *heapAddr);
	
	/**
	 * The inverse operation to the heapAddrToCardAddr:  returns the base heap address that the given cardAddr covers.
	 * @param[in] env The thread making the request
	 * @param[in] cardAddr The card address to convert
	 * @return The lowest address of the heap which the given card tracks 
	 */
	void *cardAddrToHeapAddr(MM_EnvironmentBase *env, Card *cardAddr);	
	
	/**
	 * Called to request that that the CardTable for entire heap range be cleaned.
	 * This multi-threaded version must be executed under Parallel Task only
	 * @param env[in] The thread passed to the driveGlobalCollectMasterThread call
	 * @param cardCleaner[in] The card cleaner implementation which will be invoked to clean each card
	 */
	void cleanCardTable(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner);

	/**
	 * Clears (sets to 0 - "clean") the cards backing the given range of the heap.
	 * The current implementation will not clear the heapTop card.
	 * @param[in] heapBase Base of heap range whoose cards to be cleaned
	 * @param[in] heapTop Top (non-inclusive) of heap range whoose associated cards are to be cleared
	 * @return The size, in bytes, of the card table fragment cleared
	 */
	uintptr_t clearCardsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop);

	/**
	 * Called to request that that the CardTable for specified heap range be cleaned.
	 * This multi-threaded version must be executed under Parallel Task only
	 * @param env[in] The thread passed to the driveGlobalCollectMasterThread call
	 * @param cardCleaner[in] The card cleaner implementation which will be invoked to clean each card
	 * @param lowAddress lowerst address of heap Card Table must be cleaned for (inclusive)
	 * @param highAddress highest address of heap Card Table must be cleaned for (exclusive)
	 */
	void cleanCardTableForRange(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, void *lowAddress, void *highAddress);

	/**
	 * Called to request that that the CardTable under a specific region be cleaned.
	 * @param env[in] The thread passed to the driveGlobalCollectMasterThread call
	 * @param cardCleaner[in] The card cleaner implementation which will be invoked to clean each card
	 * @param region[in] The region to be cleaned
	 */
	void cleanCardsInRegion(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, MM_HeapRegionDescriptor *region);

	/**
	 * Calculate card table size
	 *
	 * Calculate the size (number of bytes) of the card table required for a given
	 * heap size. First round the supplied size to a multiple of CARD_SIZE. Then
	 * calculate how many bytes we need to map this size heap. We then round the
	 * result to a multiple of uint32_t's.
	 *
	 * @param[in] env A thread (typically the thread initializing the GC)
	 * @param[in] heapsize The size, in bytes, of the heap in question
	 * @return The size, in bytes, of the card table required to track changes to a heap of the given size 
	 * @note only called from MM_ConcurrentCardTable
	 */
	static uintptr_t calculateCardTableSize(MM_EnvironmentBase *env, uintptr_t heapsize);

	/**
	 * Align low address to virtual memory page size.
	 * Check, is it possible to round down (memory not in use), round up otherwise
	 * @param low address in card table
	 */
	void *getLowAddressToRelease(MM_EnvironmentBase *env, void *low);
	
	/**
	 * Align high address to virtual memory page size.
	 * Check, is it possible to round up (memory not in use), round down otherwise
	 * @param high address in card table
	 */
	void *getHighAddressToRelease(MM_EnvironmentBase *env, void *high);
	
	/**
	 * Check is heap memory correspondent with specified interval in card table might be decommited (nobody use it)
	 * @param low low border of interval in card table 
	 * @param high high border of interval in card table
	 * @return true if memory might be released 
	 */
	bool canMemoryBeReleased(MM_EnvironmentBase *env, void *low, void *high);
	
	/**
	 * Free the card table instance.
	 * @param[in] env The thread shutting down the collector
	 */
	void kill(MM_EnvironmentBase *env);

#if defined(OMR_GC_VLHGC)
	/**
	 * Commits the memory for the portion of the card table corresponding to the heap range in the given region.
	 * @param env[in] A GC thread (typically the one performing an expand)
	 * @param region[in] The region for which we must commit cards
	 * @return true if memory has been commited properly
	 */
	bool commitCardsForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region);

	/**
	 * Binds the cards under a given range of heap memory to its NUMA node.
	 * @param env[in] The master GC thread
	 * @param numaNode[in] The NUMA node to which the cards should be bound
	 * @param baseOfHeapRange[in] The heap address contained in the first card in the range to be bound
	 * @param topOfHeapRange[in] The heap address following the last card in the range to be bound
	 * @return True if the binding was successful
	 */
	bool setNumaAffinityCorrespondingToHeapRange(MM_EnvironmentBase *env, uintptr_t numaNode, void *baseOfHeapRange, void *topOfHeapRange);
#endif /* defined(OMR_GC_VLHGC) */

protected:
	/**
	 * Initialize a new card table object. This involves the instantiation
	 * of new Virtual Memory objects for the card table, debug card table
	 * and TLH mark bit map objects.
	 *
	 * @param env[in] The master GC thread
	 * @param heap[in] The heap which this card table is meant to describe
	 * @return TRUE if object initialized OK; FALSE otherwise
	 */
	bool initialize(MM_EnvironmentBase *env, MM_Heap *heap);
	virtual void tearDown(MM_EnvironmentBase *env);
	
	/**
	 * Commits the card table range between lowCard and highCard:  [lowCard, highCard)
	 * @return false if the commit failed
	 */
	bool commitCardTableMemory(MM_EnvironmentBase *env, Card *lowCard, Card *highCard);
	
	/**
	 * Decommits the card table range between lowCard and highCard:  [lowCard, highCard]
	 * @return false if the decommit failed
	 */
	bool decommitCardTableMemory(MM_EnvironmentBase *env, Card *lowCard, Card *highCard, Card *lowValidCard, Card *highValidCard);
	
	/**
	 * Create a CardTable object.
	 */
	MM_CardTable()
		: MM_BaseVirtual()
		, _heapAlloc(NULL)
		, _cardTableMemoryHandle()
		, _cardTableStart(NULL)
		, _cardTableVirtualStart(NULL)
		, _heapBase(NULL)
	{
		_typeId = __FUNCTION__;
	}

private:
	void cleanRange(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, Card *low, Card *high);
};

#endif /* defined (OMR_GC_HEAP_CARD_TABLE) */

#endif /* CARDTABLE_HPP_ */
