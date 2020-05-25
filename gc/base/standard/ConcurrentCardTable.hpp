/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#if !defined(CONCURRENTCARDTABLE_HPP_)
#define CONCURRENTCARDTABLE_HPP_


#include "omrcfg.h"

#include "CardTable.hpp"
#include "ConcurrentCardTableStats.hpp"
#include "Debug.hpp"
#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "MemoryManager.hpp"

/**
 * @ingroup GC_Modron_Standard
 * @name CardTable platform dependent constants
 * @{
 */ 
#if defined(OMR_ENV_DATA64)                                       
#define TLH_MARKING_BIT_MASK ((uintptr_t)32767)
#define TLH_MARKING_INDEX_SHIFT ((uintptr_t)15)		/* i.e divide by 32768 */
#define TLH_CARD_INDEX_SHIFT ((uintptr_t)6)
#define TLH_CARD_BIT_MASK ((uintptr_t)63)
#else /* !OMR_ENV_DATA64 */                                        
#define TLH_MARKING_BIT_MASK ((uintptr_t)16383)
#define TLH_MARKING_INDEX_SHIFT ((uintptr_t)14)		/* i.e divide by 16384 */
#define TLH_CARD_INDEX_SHIFT ((uintptr_t)5)
#define TLH_CARD_BIT_MASK ((uintptr_t)31)
#endif /* !OMR_ENV_DATA64 */     

#define TLH_MARKING_SET_ALL_BITS ((uintptr_t)(-1))
#define TLH_MARKING_CLEAR_ALL_BITS ((uintptr_t)(0))
                                   
/**
 * @}
 */

/**
 * @ingroup GC_Modron_Standard
 * @name  misc card definitions
 * @{
 */
#define CONCURRENT_CARD_CLEAN_MASK (CARD_DIRTY)
#define FINAL_CARD_CLEAN_MASK (CARD_DIRTY)

#define SLOT_ALL_CLEAN (uintptr_t)CARD_CLEAN
#define EXCLUSIVE_VMACCESS_REQUESTED ((uintptr_t)-1)
 
/**
 * @}
 */

/**
 * @ingroup GC_Modron_Standard
 * @name misc definitions
 * @{
 */ 
#define BITS_IN_BYTE 8
#define ALL_BITS_SET (uintptr_t)(-1)
#define LOW_BIT_SET 0x1
#define HIGH_BIT_SET ~(((uintptr_t)(-1))>>1)

/**
 * @}
 */

/**
 * Concurrent card cleaning takes place in up to 3 phases as follows:
 * 
 * PHASE1(-Xgc:cardCleaningPasses == 1|2) during the first phase we process all card for heapBase thru to card
 * for head of free list at start of card cleaning
 * 
 * PHASE2(-Xgc:cardCleaningPasses == 1|2) during phase 2 we process all remaining cards, if any, in the card 
 * table not cleaned during phase1.  
 * 
 * PHASE3(-Xgc:cardCleaingPasses == 2) during the 3rd phase we re-clean all cards in the card table
 * 
 * @ingroup GC_Modron_Standard
 */
 
typedef enum {
	UNINITIALIZED=0,
	PHASE1_PREPARING,
	PHASE1_CLEANING,
	PHASE1_COMPLETE,
	PHASE2_PREPARING,
	PHASE2_CLEANING,
	PHASE2_COMPLETE,
	PHASE3_PREPARING,
	PHASE3_CLEANING,
	PHASE3_COMPLETE
} CardCleanPhase;

/**
 * @todo Provide typedef documentation
 * @ingroup GC_Modron_Standard
 */
typedef enum {
	SET=1,
	CLEAR=2
} BitMapAction;	


class MM_EnvironmentBase;
class MM_ConcurrentGC;
class MM_Dispatcher;
class MM_MarkingScheme;


typedef struct {
	Card * baseCard;
	Card * topCard;
	Card * volatile nextCard;
	uintptr_t numCards;
} CleaningRange;

/**
 * @todo Provide class documentation
 * @warn All card table functions assume EXCLUSIVE ranges, ie they take a base and top card where base card
 * is first card in range and top card is card immediately AFTER the end of the range.
 * @ingroup GC_Modron_Standard
 */
class MM_ConcurrentCardTable : public MM_CardTable 
{
	/*
	 * Data members
	 */
private:
	MM_MemoryHandle _tlhMarkMapMemoryHandle;
	
	uintptr_t *_tlhMarkBits;
	bool _cardTableReconfigured;
	bool _cleanAllCards;
protected:
	OMR_VM *_omrVM;
	MM_ConcurrentGC *_collector;
	MM_GCExtensionsBase *_extensions;
	MM_Dispatcher *_dispatcher; 
	MM_MarkingScheme *_markingScheme;
	MM_ConcurrentCardTableStats _cardTableStats;
	volatile CardCleanPhase	_cardCleanPhase;
	CardCleanPhase _lastCardCleanPhase;
	CleaningRange *_cleaningRanges;
	CleaningRange * volatile _currentCleaningRange;
	CleaningRange *_lastCleaningRange;
	uintptr_t _maxCleaningRanges;
	
	Card _concurrentCardCleanMask;
	Card _finalCardCleanMask;
	
	Card *_lastCard;
	Card *_firstCardInPhase;
	Card * volatile _lastCardInPhase;
	Card *_firstCardInPhase2;
public:
	
	/*
	 * Function members
	 */
private:
	virtual void tearDown(MM_EnvironmentBase *env);
	
	bool allocateCardTableEntriesForHeapRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, bool clearNewCards);
	bool freeCardTableEntriesForHeapRange(MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	bool allocateTLHMarkMapEntriesForHeapRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	bool freeTLHMarkMapEntriesForHeapRange(MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	
	void setTLHMarkBits(MM_EnvironmentBase *env, uintptr_t slotIndex, uintptr_t slotBits);
	void clearTLHMarkBits(MM_EnvironmentBase *env, uintptr_t slotIndex, uintptr_t slotBits);
	static uintptr_t calculateTLHMarkMapSize(MM_EnvironmentBase *env, uintptr_t cardTableSize);
	
	void determineCleaningRanges(MM_EnvironmentBase *env);
	void resetCleaningRanges(MM_EnvironmentBase *env);
	bool isCardInActiveTLH(MM_EnvironmentBase *env, Card *card);
	
	void reportCardCleanPass2Start(MM_EnvironmentBase *env);
		
	MMINLINE uintptr_t getTLHMarkBitMask(uintptr_t index)
	{
#if defined(OMR_ENV_LITTLE_ENDIAN)		
		return ((uintptr_t)LOW_BIT_SET << index);
#else
		return ((uintptr_t)HIGH_BIT_SET >> index);
#endif
	}
	
	MMINLINE uintptr_t getTLHMarkBitSetHead(uintptr_t index)
	{
#if defined(OMR_ENV_LITTLE_ENDIAN)		
		return ((uintptr_t)ALL_BITS_SET << index);
#else
		return ((uintptr_t)ALL_BITS_SET >> index);
#endif		
	}

	MMINLINE uintptr_t getTLHMarkBitSetTail(uintptr_t index)
	{
#if defined(OMR_ENV_LITTLE_ENDIAN)		
		return (~(((uintptr_t)ALL_BITS_SET << index) << 1));
#else
		return (~((uintptr_t)ALL_BITS_SET >> (index) >> 1));
#endif		
	}
	
	void processTLHMarkBits(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, void *tlhBase, void *tlhTop, BitMapAction action);
	
protected:
	bool initialize(MM_EnvironmentBase *env, MM_Heap *heap);
	
	bool cleanSingleCard(MM_EnvironmentBase *env, Card *card, uintptr_t bytesToClean, uintptr_t *totalBytesCleaned);
	Card* getNextDirtyCard(MM_EnvironmentBase *env, Card cardMask, bool concurrentCardClean);
	
	bool cardHasMarkedObjects(MM_EnvironmentBase *env, Card *card);
	
	virtual void prepareCardsForCleaning(MM_EnvironmentBase *env);
	virtual bool getExclusiveCardTableAccess(MM_EnvironmentBase *env, CardCleanPhase currentPhase, bool threadAtSafePoint);
	virtual void releaseExclusiveCardTableAccess(MM_EnvironmentBase *env);
	
	MMINLINE virtual void concurrentCleanCard(Card *card) { *card = CARD_CLEAN; };
	MMINLINE virtual void finalCleanCard(Card *card) { *card = CARD_CLEAN; };
			
	MMINLINE bool cardTableNeedsPreparing(CardCleanPhase currentPhase) 
	{ 	
		return ((currentPhase < _lastCardCleanPhase) && (currentPhase % 3 <  2 )) ? true : false;
	}
	
	bool cardTableBeingPrepared(CardCleanPhase currentPhase) { return (currentPhase % 3 == 1 ? true:false); };
	bool cardCleaningInProgress(CardCleanPhase currentPhase) { return (currentPhase % 3 == 2 ? true:false); };
	
	MMINLINE void initializeCardCleaningStatistics() 
	{ 
		_cardTableStats.initializeCardCleaningStatistics(); 
	}
	
	MMINLINE void incConcurrentCleanedCards(uintptr_t numCards, CardCleanPhase currentPhase)
	{
		switch(currentPhase) {
		case PHASE1_CLEANING:
			_cardTableStats.incConcurrentCleanedCardsPhase1(numCards);
			break;
		case PHASE2_CLEANING:
			_cardTableStats.incConcurrentCleanedCardsPhase2(numCards);
			break;	
		case PHASE3_CLEANING:
			_cardTableStats.incConcurrentCleanedCardsPhase3(numCards);
			break;	
		default: 
			assume0(0);
			break;
		}		
	}
	
	MMINLINE void incFinalCleanedCards(uintptr_t numCards, bool phase2)
	{
		if (0 == numCards) {
			return;
		}
		
		if(!phase2) {
			_cardTableStats.incFinalCleanedCardsPhase1(numCards);	
		} else {
			_cardTableStats.incFinalCleanedCardsPhase2(numCards);
		}
	}

public:
	/**
	 * Creates and returns a new instance of the card table.
	 * @param[in] env The thread starting up the collector
	 * @param[in] heap The heap which this card table is meant to describe
	 * @param[in] markingScheme The marking scheme which the card table will use to find object starts for precise card area marking
	 * @param[in] collector The collector with which we interact
	 * @return The new isntance (NULL on failure)
	 */
	static MM_ConcurrentCardTable	*newInstance(MM_EnvironmentBase *env, MM_Heap *heap, MM_MarkingScheme *markingScheme, MM_ConcurrentGC *collector);
	
	/**
	 * Called when a range of memory has been added to the heap.
	 * @param[in] env The thread which initiated the expansion request (typically the master GC thread)
	 * @param[in] subspace The expanding subspace
	 * @param[in] size The number of bytes between lowAddress and highAddress
	 * @param[in] lowAddress The first byte inside the expanded range
	 * @param[in] highAddress The first byte after the expanded range
	 * @param[in] cleanNewCards True if the new cards included in this range should be cleared (true if the subspace is collectable)
	 * @return true if expansion is successful
	 */
	bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, bool cleaNewCards);
	/**
	 * Called when a range of memory has been removed from the heap.
	 * @param[in] env The thread which initiated the contraction request (typically the master GC thread)
	 * @param[in] subspace The expanding subspace
	 * @param[in] size The amount of memory removed from the heap
	 * @param[in] lowAddress The base address of the memory added to the heap
	 * @param[in] highAddress The top address (non-inclusive) of the memory added to the heap
	 * @param[in] lowValidAddress The first valid address previous to the lowest in the heap range being removed
	 * @param[in] highValidAddress The first valid address following the highest in the heap range being removed
	 * @param[in] cleanNewCards True if the new cards included in this range should be cleared (true if the subspace is collectable)
	 * @return true if contraction is successful
	 */
	bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress);
	
	/**
	 * @return A pointer to the mutable card table stats structure
	 */
	MM_ConcurrentCardTableStats *getCardTableStats() {return &_cardTableStats; };

	/**
	 * Determine how many bytes of card table correspond to the supplied heap range.
	 * The current implementation will not count the heapTop card in this size.
	 *
	 * @param[in] heapBase Base address of heap range
	 * @param[in] heapTop Top (non-inclusive) address of heap range
	 * @return The size, in bytes, of all the cards which map to the supplied heap range
	 * @note Only called by MM_ConcurrentCardTable and MM_ConcurrentGC
	 */
	uintptr_t cardBytesForHeapRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop);
	/**
	 * Dirties the card backing the given range of the heap.  Will dirty the heapBase and heapTop cards, inclusively.
	 * @param[in] env The thread which caused the remembered set to resize
	 * @param[in] heapBase The base address of the fragment of the heap excluded from the remembered set
	 * @param[in] heapTop The byte after the highest address of the heap excluded from the remembered set
	 * @note Called when the remembered set resizes to exclude a range of the heap
	 */
	void dirtyCardsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop);
	/**
	 * Called to clear the cards backing the heap regions which are in active but not concurrently collectable subspaces.
	 * The purpose of this call is to clear the cards backing active nursery heap ranges, under the gencon policy.
	 * @param[in] env The thread which reported the concurrent work stack overflow
	 * @note Called only from MM_ConcurrentGC during concurrent work stack overflow
	 */
	void clearNonConcurrentCards(MM_EnvironmentBase *env);
	
	/**
	 * Prepare for next concurrent marking cycle (atomically sets internal state of the card table).
	 * @param[in] env The initializing concurrent card cleaning
	 * @note Called only from MM_ConcurrentGC
	 */
	void initializeCardCleaning(MM_EnvironmentBase *env);
	
	/**
	 * Start or continue the card cleaning process.
	 *
	 * Called during concurrent mode CONCURRENT_CLEAN_TRACE to clean a number of
	 * dirty cards; we clean enough cards to satisfy the amount of objects we need
	 * to trace represented by the input value "sizeToDo". Also, updates "sizeDone"
	 * the actual size traced which may be less than sizeToDo if a GC is requested
	 * whilst we are tracing or all cards cleaned.
	 *
	 * @param isMutator TRUE if called on a mutator thread, FALSE if called on a background thread
	 * @param sizeToDo the amount of objects to be traced
	 * @param threadAtSafePoint Whether the calling thread is at a safe point or not
	 * @return FALSE if a GC occurs during during card table preperation.
	 */
	bool cleanCards(MM_EnvironmentBase *env, bool isMutator, uintptr_t sizeToDo, uintptr_t *sizeDone, bool threadAtSafePoint);
	/**
	 * Initialize for final card cleaning.
	 *
	 * Called by STW to do any necessary initialization prior to final card cleaning.
	 */
	virtual void initializeFinalCardCleaning(MM_EnvironmentBase *env);
	/**
	 * Do final card cleaning.
	 *
	 * To be called by a STW parallel mark task to clean enough cards such that we
	 * push a packet worth of references.  Loops calling getNextDirtyCard() until
	 * we have pushed enough references or end of card table reached.
	 *
	 * @param bytesTraced  - reference to counter to pass back count of bytes traced
	 * 						 to caller
	 *
	 * @return return TRUE is any more cards left to process; FALSE if all cards
	 * 			processed. Also returns number of bytes traced whilst cleaning cards.
	 *
	 */
	bool finalCleanCards(MM_EnvironmentBase *env, uintptr_t *bytesTraced);
	/**
	 * Determine whether the referenced object is within a dirty card. Used if
	 * object reference may not be in tenure or nursery.
	 *
	 * @param object - reference to object to be checked
	 * @return TRUE if reference is to an object within a dirty card; FALSE otherwise
	 */
	bool isObjectInDirtyCard(MM_EnvironmentBase *env, omrobjectptr_t object);
	/**
	 * Is object in a dirty card
	 *
	 * Determine whether the referenced object is within a  dirty card. No range
	 * check on supplied object address. Used whenwe are sure the object is within
	 * the old/tenure heap.
	 *
	 * @param object - reference to object to be checked
	 * @return TRUE if reference is to an object within a dirty card; FALSE otherwise
	 */
	bool isObjectInDirtyCardNoCheck(MM_EnvironmentBase *env, omrobjectptr_t object);
	
	/**
	 * Is object in an uncleaned dirty card
	 *
	 * Determine whether a given object is in a DIRTY card which has already
	 * cleaned during current concurrent phase.
	 *
	 * @param object - reference to object to be checked
	 *
	 * @return TRUE if card has NOT already been cleaned; FALSE otherwise
	 */
	virtual bool isObjectInUncleanedDirtyCard(MM_EnvironmentBase *env, omrobjectptr_t object);
	
	/**
	 * @return TRUE if we are at least in PHASE1_CLEANING.
	 * @note Only called internally
	 */
	bool isCardCleaningStarted(){ return (_cardCleanPhase >= PHASE1_CLEANING) ? true : false; };
	/**
	 * @return TRUE if we are in the last card cleaning phase
	 */
	bool isCardCleaningComplete() { return (_lastCardCleanPhase == _cardCleanPhase) ;};
	
	/**
	 * Is objec tin an active TLH
	 *
	 * Determine whether the referenced object is within an active (incomplete) TLH.
	 *
	 * @param object - reference to the object to be checked
	 * @return TRUE if reference is to an object within an active TLH; FALSE otherwise
	 * @note Only called from MM_ConcurrentGC and internally
	 */
	bool isObjectInActiveTLH(MM_EnvironmentBase *env, omrobjectptr_t object);
	
	/**
	 * Is TLH mark bits empty?
	 * Check that all bits in the TLH mark bits map are OFF. All bits should be OFF
	 *
	 * @return TRUE if TLH mark bits all off; FALSE otherwise
	 */
	static void tlhCleared(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
	/**
	 * Called when a new TLH has been allocated.
	 * We use this broadcast event to trigger the update of  the TLH mark bits.
	 * @see MM_ParallelGlobalGC::TLHRefreshed()
	 */
	static void tlhRefreshed(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData);
		
#if defined(DEBUG)
	bool isTLHMarkBitsEmpty(MM_EnvironmentBase *env);
	bool isCardTableEmpty(MM_EnvironmentBase *env);
#endif /* DEBUG */
	
	/**
	 * Create a CardTable object.
	 */
	MM_ConcurrentCardTable(MM_EnvironmentBase *env, MM_MarkingScheme *markingScheme, MM_ConcurrentGC *collector):
		MM_CardTable(),
		_tlhMarkMapMemoryHandle(),
		_tlhMarkBits(NULL),
		_cardTableReconfigured(false),
		_cleanAllCards(false),
		_omrVM(env->getOmrVM()),
		_collector(collector),
		_extensions(MM_GCExtensionsBase::getExtensions(_omrVM)),
		_dispatcher(_extensions->dispatcher),
		_markingScheme(markingScheme),
		_cardCleanPhase(UNINITIALIZED),
		_lastCardCleanPhase(UNINITIALIZED),
		_cleaningRanges(NULL),
		_currentCleaningRange(NULL),
		_lastCleaningRange(NULL),
		_maxCleaningRanges(0),
		_lastCard(NULL),
		_firstCardInPhase(NULL),
		_lastCardInPhase(NULL),
		_firstCardInPhase2(NULL)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* CONCURRENTCARDTABLE_HPP_ */
