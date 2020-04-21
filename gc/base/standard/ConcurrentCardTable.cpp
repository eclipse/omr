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

#include "omrcfg.h"
#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "AtomicOperations.hpp"
#include "CollectorLanguageInterface.hpp"
#include "ConcurrentGC.hpp"
#include "ConcurrentGCStats.hpp"
#include "ConcurrentCardTable.hpp"
#include "Debug.hpp"
#include "EnvironmentStandard.hpp"
#include "Heap.hpp"
#include "HeapMapIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "MarkingScheme.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "WorkStack.hpp"
#include "WorkPacketsStandard.hpp"
#include "MarkingScheme.hpp"
#include "MemoryManager.hpp"

#include "mmprivatehook.h"
#include "mmprivatehook_internal.h"
#include "ModronAssertions.h"
#include "thread_api.h"

/**
 * Called when a new TLH has been allocated.
 * We use this broadcast event to trigger the update of  the TLH mark bits.
 * @see MM_ParallelGlobalGC::TLHRefreshed()
 */
void
MM_ConcurrentCardTable::tlhRefreshed(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_CacheRefreshedEvent* event = (MM_CacheRefreshedEvent*)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_ConcurrentCardTable *)userData)->processTLHMarkBits(env,
													(MM_MemorySubSpace *)event->subSpace,
													event->cacheBase,
													event->cacheTop,
													SET);
}

/**
 * Called when a full TLH is cleared.
 * We use this broadcast event to trigger the update of the TLH mark bits and set the ObjectMap bits
 * @see MM_ParallelGlobalGC::TLHRefreshed()
 */
void
MM_ConcurrentCardTable::tlhCleared(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_CacheClearedEvent* event = (MM_CacheClearedEvent*)eventData;
	MM_EnvironmentBase *env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_ConcurrentCardTable *)userData)->processTLHMarkBits(env,
													(MM_MemorySubSpace *)event->subSpace,
													event->cacheBase,
													event->cacheTop,
													CLEAR);

}

/**
 *						Card Table and TLH Mark Map Sizing
 * 						==================================
 *
 */

/**
 * Calculate TLH Mark map size
 *
 * Calculate the size (number of bytes) of the TLH mark map required for a given
 * card table size. First round the supplied size to a multiple of BITS_IN_BYTE.
 * Then calculate how many bytes we need to map this size of card table. We then
 * round the result to a multiple of uint32_t's.
 *
 * @param cardTableSize - the current heap size
 *
 * @return the required size (in bytes) of the TLH mark map
 */
uintptr_t
MM_ConcurrentCardTable::calculateTLHMarkMapSize(MM_EnvironmentBase *env, uintptr_t cardTableSize)
{
	uintptr_t size;

	size = MM_Math::roundToCeiling(sizeof(uint32_t),
				MM_Math::roundToCeiling(BITS_IN_BYTE, cardTableSize) / BITS_IN_BYTE) ;

	return size;
}

/**
 * Commit card table entries into memory that correspond to the newly added heap range.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param cleanNewCards If true all new cards are to be cleared to zero
 */
bool
MM_ConcurrentCardTable::allocateCardTableEntriesForHeapRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, bool clearNewCards)
{
	Card *lowCard, *highCard;

	/* Determine the low and high cards which map from the heap address ranges */
	lowCard = heapAddrToCardAddr(env, lowAddress);
	highCard = heapAddrToCardAddr(env, highAddress);

	/* Check if this is the last card in use */
	if(_lastCard < highCard) {
		_lastCard = highCard;
	}

	/* Commit the card table memory.  We must point to the address just being the highCard to get the correct range */
	bool commited = commitCardTableMemory(env, lowCard, highCard);

	if (commited) {
		/* Clear the new cards if requested by caller */
		if (clearNewCards) {
			//TODO Use gc helpers to do this init
			clearCardsInRange(env, lowAddress, highAddress);
		}
	}
	return commited;
}

/**
 * Decommit card table entries from memory that correspond to the removed heap range.
 *
 * @param size The amount of memory remove from the heap
 * @param lowAddress The base address of the memory removed from the heap
 * @param highAddress The top address (non-inclusive) of the memory removed from the heap
 */
bool
MM_ConcurrentCardTable::freeCardTableEntriesForHeapRange(
	MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress,
	void *lowValidAddress, void *highValidAddress)
{
	Card *lowCard, *highCard, *lowValidCard, *highValidCard;

	/* Determine the low and high cards which map from the heap address ranges */
	lowCard = heapAddrToCardAddr(env, lowAddress);
	highCard = heapAddrToCardAddr(env, highAddress);

	lowValidCard = NULL;
	if(NULL != lowValidAddress) {
		lowValidCard = heapAddrToCardAddr(env, (uint8_t *)lowValidAddress);
	}

	highValidCard = NULL;
	if(NULL != highValidAddress) {
		highValidCard = heapAddrToCardAddr(env, (uint8_t *)highValidAddress);
	}

	/* Check if this is the last card in use */
	if((NULL != lowValidCard) && (NULL == highValidCard)) {
		if(_lastCard > lowCard) {
			_lastCard = lowValidCard;
		}
	}

	/* Commit the card table memory.  We must point to the address just being the highCard to get the correct range */
	return decommitCardTableMemory(env, lowCard, highCard, lowValidCard, highValidCard);
}

/**
 * Commit the TLH mark map region into memory that correspond to the newly added heap range.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 */
bool
MM_ConcurrentCardTable::allocateTLHMarkMapEntriesForHeapRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool commited = true;

	if (_extensions->isFvtestForceConcurrentTLHMarkMapCommitFailure()) {
		commited = false;
		Trc_MM_ConcurrentCardTable_activeTLHMarkMapCommitFailureForced(env->getLanguageVMThread());
	} else {
		/* Do we have any TLH mark bits ? */
		if (NULL != _tlhMarkBits) {
			/* Determine the low and high entries which map from the heap address ranges */
			/* We assume that expansion rules keep addresses on mark map boundaries */
			uintptr_t numericalHeapBase = (uintptr_t)getHeapBase();
			uintptr_t lowTLHMarkMap = (((uintptr_t)lowAddress) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;
			uintptr_t highTLHMarkMap = (((uintptr_t)highAddress) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;

			/* Adjust the lowTLHMArkMap to account for rounding (since this is really the highTLHMarkMap address of another range).
			 * as we dont want to commit a slot we have previously commited andn more importantly we dont want to clear and existing
			 * slot. */
			if((lowTLHMarkMap << TLH_MARKING_INDEX_SHIFT) < (((uintptr_t)lowAddress) - numericalHeapBase)) {
				lowTLHMarkMap += 1;
			}

			/* Commit the tlh mark bit memory to span the low and high indexes just calculated.
			 * However as we always commit multiples of uintptr_t we need to check if the the above
			 * shift has rounded down the number of uintptr_t slots we need to commit. If it has
			 * we need to commit an extra uintptr_t worth otherwise will will end up trying to set
			 * TLH bits in uncomitted memory.
			 */
			if ((highTLHMarkMap << TLH_MARKING_INDEX_SHIFT) < (((uintptr_t)highAddress) - numericalHeapBase)) {
				highTLHMarkMap += 1;
			}

			uintptr_t mapSize = (highTLHMarkMap - lowTLHMarkMap) * sizeof(uintptr_t);

			MM_MemoryManager *memoryManager = _extensions->memoryManager;
			commited = memoryManager->commitMemory(&_tlhMarkMapMemoryHandle, (void *) &(_tlhMarkBits[lowTLHMarkMap]), mapSize);

			if (commited) {
				/* Clear the newly allocated range
				 *
				 * N.B We can't use OMRZeroMemory() here as that requires the  area to
				 * be cleared to be uintptr_t aligned
				 *
				 */
				memset((void *)&(_tlhMarkBits[lowTLHMarkMap]), 0, mapSize);
			} else {
				/* Failed to commit memory */
				Trc_MM_ConcurrentCardTable_activeTLHMarkMapCommitFailed(env->getLanguageVMThread(), (void *) &(_tlhMarkBits[lowTLHMarkMap]), mapSize);
			}
		}
	}
	return commited;
}

/**
 * Commit the TLH mark map region into memory that correspond to the newly added heap range.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 */
bool
MM_ConcurrentCardTable::freeTLHMarkMapEntriesForHeapRange(
	MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress,
	void *lowValidAddress, void *highValidAddress)
{
	bool decommited = true;

	if (_extensions->isFvtestForceConcurrentTLHMarkMapDecommitFailure()) {
		decommited = false;
		Trc_MM_ConcurrentCardTable_activeTLHMarkMapDecommitFailureForced(env->getLanguageVMThread());
	} else {
		/* Do we have any TLH mark bits ? */
		if (NULL != _tlhMarkBits) {
			/* Determine the low and high entries which map from the heap address ranges */
			/* We assume that expansion rules keep addresses on mark map boundaries */
			uintptr_t numericalHeapBase = (uintptr_t)getHeapBase();
			uintptr_t lowTLHMarkMap = (((uintptr_t)lowAddress) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;
			uintptr_t highTLHMarkMap = (((uintptr_t)highAddress) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;

			/* Decommit the tlh mark bit memory to span the low and high indexes just calculated.
			 * However as we always decommit multiples of uintptr_t we need to check if the the above
			 * shift has rounded down the number of uintptr_t slots we need to commit. If it has
			 * we need to commit an extra uintptr_t worth otherwise will will end up trying to set
			 * TLH bits in uncomitted memory.
			 */
			if ((highTLHMarkMap << TLH_MARKING_INDEX_SHIFT) < (((uintptr_t)highAddress) - numericalHeapBase)) {
				highTLHMarkMap += 1;
			}

			uintptr_t lowValidTLHMarkMap = 0;
			if(NULL != lowValidAddress) {
				lowValidTLHMarkMap = (((uintptr_t)lowValidAddress) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;
				/* Adjust the low valid range to account for rounding (since this is really the high address of another range) */
				if((lowValidTLHMarkMap << TLH_MARKING_INDEX_SHIFT) < (((uintptr_t)lowValidAddress) - numericalHeapBase)) {
					lowValidTLHMarkMap += 1;
				}
			}

			uintptr_t highValidTLHMarkMap = 0;
			if(NULL != highValidAddress) {
				highValidTLHMarkMap = (((uintptr_t)highValidAddress) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;
			}

			/* Adjust the decommit range to be within the still valid address range */
			if(lowTLHMarkMap < lowValidTLHMarkMap) {
				lowTLHMarkMap = lowValidTLHMarkMap;
			}
			if((NULL != highValidAddress) && (highTLHMarkMap > highValidTLHMarkMap)) {
				highTLHMarkMap = highValidTLHMarkMap;
			}

			/* decommit the card table memory */
			uintptr_t mapSize = (highTLHMarkMap - lowTLHMarkMap) * sizeof(uintptr_t);
			if(0 != mapSize) {
				MM_MemoryManager *memoryManager = _extensions->memoryManager;
				decommited = memoryManager->decommitMemory(
					&_tlhMarkMapMemoryHandle,
					(void *) &(_tlhMarkBits[lowTLHMarkMap]),
					mapSize,
					(NULL != lowValidAddress) ? (void *) &(_tlhMarkBits[lowValidTLHMarkMap]) : NULL,
					(NULL != highValidAddress) ? (void *) &(_tlhMarkBits[highValidTLHMarkMap]) : NULL);
				if (!decommited) {
					Trc_MM_ConcurrentCardTable_activeTLHMarkMapDecommitFailed(env->getLanguageVMThread(), (void *) &(_tlhMarkBits[lowTLHMarkMap]), mapSize,
						(NULL != lowValidAddress) ? (void *) &(_tlhMarkBits[lowValidTLHMarkMap]) : NULL,
						(NULL != highValidAddress) ? (void *) &(_tlhMarkBits[highValidTLHMarkMap]) : NULL);
				}
			}
		}
	}
	return decommited;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 *
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param cleaNewCards - If true all new cards should be cleared immediately
 */
bool
MM_ConcurrentCardTable::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, bool clearNewCards)
{
	/* Determine the current top of heap */
	_heapAlloc = _extensions->heap->getHeapTop();

	/* Adjust card table to reflect heap change */
	bool result = allocateCardTableEntriesForHeapRange(env, subspace, size, lowAddress, highAddress, clearNewCards);

	if (result) {
		if (subspace->isConcurrentCollectable()){
			/* Only need to adjust TLH mark map is storage added to a
			 * concurrently collectable subspace
			 */
			result = allocateTLHMarkMapEntriesForHeapRange(env, subspace, size, lowAddress, highAddress);
			_cardTableReconfigured = true;
		}
	}
	return result;
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 *
 * @param size The amount of memory removed from the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param lowValidAddress Lowest address of card table
 * @param highValidAddress Address of byte next after card table 
 *
 */
bool
MM_ConcurrentCardTable::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	bool result = true;
	/*
	 * An early attempt to remove heap range because of memory allocation failure
	 * (before actual expansion), so heap _heapAlloc is not set yet.
	 * It means that no Card Table memory has been commited at this point
	 */
	if (NULL != _heapAlloc) {
		/* Card Table has been expanded, so size can not be 0 */
		Assert_MM_true(size > 0);

		/* Adjust card table  to reflect heap change */
		result = freeCardTableEntriesForHeapRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);

		if (result) {
			if (subspace->isConcurrentCollectable()){
				/* Only need to adjust TLH mark map is storage removed
				 * from a concurrently collectable subspace
				 */
				/* If attempt to shrink TLH Mark Map fails return false, no other actions required */
				result = freeTLHMarkMapEntriesForHeapRange(env, size, lowAddress, highAddress, lowValidAddress, highValidAddress);
				_cardTableReconfigured = true;
				if (!result) {
					/* put tracepoint here */
				}
			}

			/* update our cached _heapAlloc */
			_heapAlloc = _extensions->heap->getHeapTop();
		}
	}
	return result;
}

/**
 *					Object creation and destruction
 * 					===============================
 *
 */

/**
 * Create a new instance of Concurrent card table
 *
 * @param markingScheme - reference to the colllector marking scheme
 * @param collector - reference to the collector itself
 *
 * @return reference to MM_ConcurrentCardTable object or NULL
 */
MM_ConcurrentCardTable *
MM_ConcurrentCardTable::newInstance(MM_EnvironmentBase *env, MM_Heap *heap, MM_MarkingScheme *markingScheme, MM_ConcurrentGC *collector)
{
	MM_ConcurrentCardTable *cardTable = (MM_ConcurrentCardTable *)env->getForge()->allocate(sizeof(MM_ConcurrentCardTable), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != cardTable) {
		new(cardTable) MM_ConcurrentCardTable(env, markingScheme, collector);
		if (!cardTable->initialize(env, heap)) {
			cardTable->kill(env);
			return NULL;
		}
	}
	return cardTable;
}


/**
 * Initialize a new card table object. This involves the instantiation
 * of new Virtual Memory objects for the card table, debug card table
 * and TLH mark bit map objects.
 *
 * @return TRUE if object initialized OK; FALSE otherwise
 */
bool
MM_ConcurrentCardTable::initialize(MM_EnvironmentBase *env, MM_Heap *heap)
{
	bool initialized = MM_CardTable::initialize(env, heap);
	if (initialized) {
		J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);
	
		/* Check that the heap is aligned as concurrent requires it. That is
		 * the heap alignment is a exact multiple of card size
		 */
		assume0(_extensions->heapAlignment % CARD_SIZE == 0);
	
		_lastCard = getCardTableStart();
	
		/* We only allocate TLH mark bits if scavenger is NOT active.
		 * If scavenger is active all TLH's are in NEW space and we don't trace
		 * into NEW space during concurrent mark phase.
		 */
#if defined(OMR_GC_MODRON_SCAVENGER)
		if(!_extensions->scavengerEnabled)
#endif /* OMR_GC_MODRON_SCAVENGER */
		{
			uintptr_t cardTableSizeRequired = calculateCardTableSize(env, heap->getMaximumPhysicalRange());
			/* Now instantiate the Virtual Memory object for the TLH Mark map */
			uintptr_t tlhMarkMapSizeRequired = calculateTLHMarkMapSize(env,cardTableSizeRequired);

			MM_MemoryManager *memoryManager = _extensions->memoryManager;
			if (!memoryManager->createVirtualMemoryForMetadata(env, &_tlhMarkMapMemoryHandle, sizeof(uintptr_t), tlhMarkMapSizeRequired)) {
				return false;
			}
	
			_tlhMarkBits= (uintptr_t *)(memoryManager->getHeapBase(&_tlhMarkMapMemoryHandle));
	
			/* Register hooks for TLH clear and refresh. Needed to maintian TLH mark bits */
			(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CACHE_CLEARED, tlhCleared, OMR_GET_CALLSITE(), (void *)this);
			(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_CACHE_REFRESHED, tlhRefreshed, OMR_GET_CALLSITE(), (void *)this);
		}
	
		/* Set default card cleaning masks used by getNextDirtycard */
		_concurrentCardCleanMask = CONCURRENT_CARD_CLEAN_MASK;
		_finalCardCleanMask = FINAL_CARD_CLEAN_MASK;
	
		/* How many of the card clean phases do we need to perform ?
		 *
		 * If 1 pass selected then we perform just the first 2 phases;
		 * if a 2nd pass selected then we further the further phase.
		 */
		switch (_extensions->cardCleaningPasses) {
		case 0:
			_lastCardCleanPhase = UNINITIALIZED;
			break;
		case 1:
			_lastCardCleanPhase = PHASE2_COMPLETE;
			break;
		case 2:
			_lastCardCleanPhase = PHASE3_COMPLETE;
			break;
		default:
			assume0(0);
			break;
		}
	}
	return initialized;
}

/**
 * Destroy a card table object by invoking the kill method on the
 * card table, debug card table and TLH mark map objects.
 *
 */
void
MM_ConcurrentCardTable::tearDown(MM_EnvironmentBase *env)
{
	/* Get rid of the virtual memory allocated for TLH mark map */
	MM_MemoryManager *memoryManager = _extensions->memoryManager;
	memoryManager->destroyVirtualMemory(env, &_tlhMarkMapMemoryHandle);

	if (NULL != _cleaningRanges) {
		env->getForge()->free(_cleaningRanges);
		_cleaningRanges = NULL;
	}
	MM_CardTable::tearDown(env);
}

/**
 * Determine how many bytes of card table correspond to the supplied heap range.
 *
 * @param heapBase - Base address of heap range
 * @param heapTop - Top (non-inclusive) address of heap range
 *
 * @return Size of all cards which map the supplied heap range
 */
uintptr_t
MM_ConcurrentCardTable::cardBytesForHeapRange(MM_EnvironmentBase *env, void* heapBase, void* heapTop)
{
	Card *firstCard,*lastCard;
	uintptr_t numBytes;

	firstCard = heapAddrToCardAddr(env,heapBase);
	lastCard = heapAddrToCardAddr(env,heapTop);

	numBytes = (uintptr_t)( (uint8_t *)lastCard - (uint8_t *)firstCard);

	return numBytes;
}

/*
 * Clear all cards for active nursery heap ranges.
 *
 * Iterate over all segments, and for any which are owned by active non-concurrently
 * collectable subspaces, e.g. allocate subspaces, clear the corresponding cards in card
 * table.
 *
 */
void
MM_ConcurrentCardTable::clearNonConcurrentCards(MM_EnvironmentBase *env)
{
	MM_HeapRegionDescriptor *region = NULL;
	MM_Heap *heap = _extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	while(NULL != (region = regionIterator.nextRegion())) {
		MM_MemorySubSpace *subspace = region->getSubSpace();
		if (subspace->isConcurrentCollectable() || !subspace->isActive()) {
			continue;
		}
		//todo this is a single memset. We should really use helper threads to speed it up
		clearCardsInRange(env, region->getLowAddress(), region->getHighAddress());
	}

	/* Ensure the next time we clean cards we include these cards */
	_cardTableReconfigured = true;
	_cleanAllCards = true;
}

/**
 * Dirty a range of cards in the card table
 *
 * Dirty the cards in the card table representing a given heap
 * range.
 * @param heapBase - base of heap range
 * @param heapTop - top of heap range
 */
void
MM_ConcurrentCardTable::dirtyCardsInRange(MM_EnvironmentBase *env, void *heapBase, void *heapTop)
{
	Card *baseCard = heapAddrToCardAddr(env, heapBase);
	Card *topCard = heapAddrToCardAddr(env, heapTop);

	while(baseCard <= topCard) {
		/* If card not already dirty then dirty it */
		if (*baseCard != (Card)CARD_DIRTY) {
			*baseCard = (Card)CARD_DIRTY;
		}
		baseCard += 1;
	}
}

/**
 * Prepare for next concurrent marking cycle
 *
 */
void
MM_ConcurrentCardTable::initializeCardCleaning(MM_EnvironmentBase *env)
{
	initializeCardCleaningStatistics();

	if (_extensions->cardCleaningPasses > 0) {
		MM_AtomicOperations::lockCompareExchangeU32((volatile uint32_t*)&_cardCleanPhase,
													(uint32_t)_cardCleanPhase,
													(uint32_t) UNINITIALIZED);

		/* If we cleaned all cards on last collect we must make sure we
		 * dont on this one unless requested to do so again
		 */
		if (_cleanAllCards) {
			_cardTableReconfigured = true;
			_cleanAllCards = false;
		}
	}
}

/**
 * Prepare cards which are to be cleaned in next stage of card cleaning.
 * On non-weakly ordered platforms this means just decide which cards to clean.
 *
 */
void
MM_ConcurrentCardTable::prepareCardsForCleaning(MM_EnvironmentBase *env)
{
	void *firstFree;
	MM_MemorySubSpace *subspace;
	MM_Heap *heap = (MM_Heap *)_extensions->heap;
	uintptr_t currentFree = heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);

	switch(_cardCleanPhase) {
	case PHASE1_PREPARING:
		assume0(_lastCardCleanPhase > PHASE1_PREPARING);
		subspace = env->getExtensions()->heap->getDefaultMemorySpace()->getTenureMemorySubSpace();
		firstFree= subspace->getFirstFreeStartingAddr(env);

		_firstCardInPhase = getCardTableStart();
		/* Provided we are not already out of free storage we clean cards up to start of free list */
		_lastCardInPhase = (NULL != firstFree)? heapAddrToCardAddr(env, firstFree) : _lastCard;

		/* Save so we can do analysis of re-dirtied cards in finalCleanCards */
		_firstCardInPhase2= _lastCardInPhase;

		/* Remember when we started phase 1 for stats */
		_cardTableStats.setCardCleaningPhase1Kickoff(currentFree);

		/* Determine the cleaning ranges  */
		if (_cardTableReconfigured){
			determineCleaningRanges(env);
		} else {
			resetCleaningRanges(env);
		}

		MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&_currentCleaningRange,
											 	(uintptr_t)_currentCleaningRange,
												(uintptr_t)_cleaningRanges);
		break;

	case PHASE2_PREPARING:
		assume0(_lastCardCleanPhase > PHASE2_PREPARING);
		_firstCardInPhase = _lastCardInPhase;
		_lastCardInPhase= _lastCard;
		/* Remember when we started phase 2 for stats */
		_cardTableStats.setCardCleaningPhase2Kickoff(currentFree);
		break;

	case PHASE3_PREPARING:
		assume0(_lastCardCleanPhase > PHASE3_PREPARING);
		_firstCardInPhase = getCardTableStart();
		_lastCardInPhase= _lastCard;

		/* Remember when we started phase 3 for stats */
		_cardTableStats.setCardCleaningPhase3Kickoff(currentFree);

		reportCardCleanPass2Start(env);

		/* Determine the cleaning ranges for 2nd pass */
		if (_cardTableReconfigured){
			determineCleaningRanges(env);
		} else {
			resetCleaningRanges(env);
		}

		break;
	default:
		assume0(0);
		break;
	}
}

/*
 * Report that we have switched to the second card cleaning pass
 */
void
MM_ConcurrentCardTable::reportCardCleanPass2Start(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

	TRIGGER_J9HOOK_MM_PRIVATE_CARD_CLEANING_PASS_2_START(
        _extensions->privateHookInterface,
        env->getOmrVMThread(),
        omrtime_hires_clock(),
        J9HOOK_MM_PRIVATE_CARD_CLEANING_PASS_2_START);
}

/*
 * Get exclusive control of the card table to prepare it for card cleaning
 *
 * @return TRUE if we got control; FALSE otherwise
 */
MMINLINE bool
MM_ConcurrentCardTable::getExclusiveCardTableAccess(MM_EnvironmentBase *env, CardCleanPhase currentPhase, bool threadAtSafePoint)
{

	/* Try to take control of preparation for next stage of card cleaning */
	if (!cardTableBeingPrepared(currentPhase)) {
		if (currentPhase == (CardCleanPhase) MM_AtomicOperations::lockCompareExchangeU32((volatile uint32_t*)&_cardCleanPhase,
																						(uint32_t) currentPhase,
																						(uint32_t) currentPhase + 1)) {
			assume0(cardTableBeingPrepared(_cardCleanPhase));
			return true;
		}
	}

	/* Another thread won race so block until card table prepared */
	while(cardTableBeingPrepared(_cardCleanPhase)) {
		omrthread_yield();
	}

	return false;
}

/*
 * Release exclusive control of the card table
 */
MMINLINE  void
MM_ConcurrentCardTable::releaseExclusiveCardTableAccess(MM_EnvironmentBase *env)
{

	/* Cache the current value */
	CardCleanPhase currentPhase = _cardCleanPhase;

	/* Finished initializing. Only one thread can be here but for
	 * consistency use atomic operation to update card cleaning phase
	*/

	assume0(cardCleaningInProgress((CardCleanPhase((uint32_t)currentPhase + 1))));
	MM_AtomicOperations::lockCompareExchangeU32((volatile uint32_t*)&_cardCleanPhase,
																(uint32_t) currentPhase,
																(uint32_t) currentPhase + 1);

}

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
 * @return FALSE if a GC occurs during during card table preparation.
 */
bool
MM_ConcurrentCardTable::cleanCards(MM_EnvironmentBase *env, bool isMutator, uintptr_t sizeToDo,	uintptr_t *sizeDone, bool threadAtSafePoint)
{
	Card *nextDirtyCard;
	uintptr_t cleanedSoFar;
	uintptr_t cardsCleaned = 0;
	uintptr_t maxPushes;
	uintptr_t gcCount = _extensions->globalGCStats.gcCount;

	/* Remember which phase of card cleaning active when we start */
	CardCleanPhase currentCleaningPhase = _cardCleanPhase;

	*sizeDone = 0;

	if (cardTableNeedsPreparing(currentCleaningPhase)) {
		/* Only allow mutator threads to do initialization work; don't want
		 * low priority concurrent helper threads holding up card cleaning.
		 */
		if (!isMutator) {
			return true;
		}

		/* Attempt to take control of card table preparation */
		if (getExclusiveCardTableAccess(env, currentCleaningPhase, threadAtSafePoint)) {

			if (0 != _extensions->fvtest_concurrentCardTablePreparationDelay) {
				omrthread_sleep_interruptable((I_64)_extensions->fvtest_concurrentCardTablePreparationDelay, 0);
			}

			/* Prepare cards to be cleaned in next phase of card cleaning */
			prepareCardsForCleaning(env);

			/* We are finished so release control */
			releaseExclusiveCardTableAccess(env);
		}

		/* We may have been dormant for a while whilst card table got prepared
		 * so check to see if cleaning still required. If not tell caller it
		 * must quit tracing.
		 */
		currentCleaningPhase = _cardCleanPhase;
	}

	if (gcCount != _extensions->globalGCStats.gcCount) {
		/* A gc has occured while attempting to acquire exclusive access to the card table */
		return false;
	}

	if (!cardCleaningInProgress(currentCleaningPhase)) {
		/* We may have been dormant for a while whilst card table got prepared
		 * so check to see if cleaning still required. If not tell caller it
		 * must quit tracing.
		 */
		return false;
	}

	/* Set upper limit of refs we push before returning to one packet's worth */
	maxPushes = (_markingScheme->getWorkPackets()->getSlotsInPacket())/2;

	/* Reset the number of pushes to this threads stack to zero */
	env->_workStack.clearPushCount();

	/* Thread now pays allocating tax by doing some tracing */
	cleanedSoFar = 0;
	nextDirtyCard = NULL;

	/* Clean cards until we have done enough or card clean phase changes */
	MM_ConcurrentGCStats *stats = _collector->getConcurrentGCStats();
	while ( cleanedSoFar < sizeToDo && currentCleaningPhase == _cardCleanPhase ) {

		/* Get next dirty card; if any */
		nextDirtyCard = getNextDirtyCard(env, _concurrentCardCleanMask, true);

		/* If no more cards or another thread waiting on exclusive access
		 * we are done
		 **/
		if ((Card *)NULL == nextDirtyCard  || (Card *)EXCLUSIVE_VMACCESS_REQUESTED == nextDirtyCard){
			break;
		}

		/*
		 * If the object is in an active TLH and provided no concurrent work stack overflow has
		 * occurred then we are done as all live objects in the card will be processed later. This
		 * is true as we know the object will have been pushed to a work packet when it was marked
		 * and as its in a active TLH either:
		 *
		 *		 (1) We have marked and pushed a reference to the object but not yet popped it, or
		 *		 (2) We have popped it and deferred it (re-pushed it to a deferred packet).
		 *
		 * Either way we don't need to process any objects on this card now.
		 *
		 * If concurrent work stack overflow has occurred the above conditions do not hold as to
		 * relieve work stack overflow we empty packets by dirtying cards for their referenced
		 * objects. Therefore we cannot be sure tracing into all active TLH's will be deferred.
		 */
		if (isCardInActiveTLH(env,nextDirtyCard) && !stats->getConcurrentWorkStackOverflowOcurred()) {
			continue;
		}

		/* Clean the dirty card */
		concurrentCleanCard(nextDirtyCard);
		cardsCleaned += 1;

		/* Now retrace the objects in the card */
		assume0(cleanedSoFar < sizeToDo);
		if ( !cleanSingleCard(env,nextDirtyCard,(sizeToDo - cleanedSoFar), &cleanedSoFar)) {
			break;
		}

		/* Have we pushed enough new refs ?*/
		if (env->_workStack.getPushCount() >= maxPushes) {
			/* yes..so return to process the refs pushed so far */
			break;
		}
	}

	/**
	 *  Update card cleaning statistics.
	 *
	 * Note: This will not be 100% accurate as _cardCleanPhase can change during a call
	 * to getNextDirtyCard so we may count the occasional card in wrong phase but the
	 * counts will be accurate enough for use currently made of them.
	 */
 	incConcurrentCleanedCards(cardsCleaned, currentCleaningPhase);

	/* If we ran out of cards to clean ...*/
	if (NULL == nextDirtyCard) {
        currentCleaningPhase = _cardCleanPhase;
        if (cardCleaningInProgress(currentCleaningPhase)) {
			MM_AtomicOperations::lockCompareExchangeU32((volatile uint32_t*)&_cardCleanPhase,
																								(uint32_t) currentCleaningPhase,
																								(uint32_t) currentCleaningPhase + 1);
        }
	}

	/* should not reach this point if STW collection occur */
	Assert_MM_true(gcCount == _extensions->globalGCStats.gcCount);

	*sizeDone = cleanedSoFar;
	return true;
}

/**
 * Clean all objects in a single card
 *
 * @param sizeToDo - amount of tax still outstanding
 * @param sizeDone - reference to count of total bytes cleaned
 *
 * @return TRUE if all objects cleaned; FALSE if GC is waiting
 */
bool
MM_ConcurrentCardTable::cleanSingleCard(MM_EnvironmentBase *env, Card *card, uintptr_t bytesToClean, uintptr_t *totalBytesCleaned)
{
	omrobjectptr_t objectPtr = 0;
	/* Calculate address of first slot in cards */
	uintptr_t *heapBase = (uintptr_t *)cardAddrToHeapAddr(env,card);
	/* ..and address of last slot N.B Range is EXCLUSIVE*/
	uintptr_t *heapTop = (uintptr_t *)((uint8_t *)heapBase + CARD_SIZE);
	bool rememberedObjectsFound = false;

	uintptr_t sizeDone = 0;
	/* Ensure we at least start to clean last object in card by making sure tax to pay at least as big as
	 * one cards worth. If last object is large pointer array  we may only partially retrace it but it
	 * will be pushed so another mutator can pick up where this thread left off.
	 */
	uintptr_t sizeToDo = (bytesToClean < CARD_SIZE ? CARD_SIZE : bytesToClean);

	/* Iterate over all marked objects in the card */
	MM_HeapMapIterator markedObjectIterator(_extensions, _markingScheme->getMarkMap(), heapBase, heapTop);

	/* Re-trace all objects which START in this card */
	MM_ConcurrentGCStats *stats = _collector->getConcurrentGCStats();
	while (NULL != (objectPtr = markedObjectIterator.nextObject())) {
		/* Check to see if another thread  is waiting for exclusive VM access. If so get out quick.
	 	 */
		if (env->isExclusiveAccessRequestWaiting()) {
			/* Re-dirty the card as we did not finish cleaning it ... */
			*card = (Card)CARD_DIRTY;
			/* ...and get out now */
			return false;
		}

		/*
		 * If the object is in an active TLH and provided no concurrent work stack overflow has
		 * occurred then we are done as we know this object, and any others in the same
		 * card,  will be processed later. This is true as we know the object will have been
		 * pushed to a work packet when it was marked and as its in a active TLH either:
		 *
		 *		 1) We have marked and pushed a reference to the object but not yet popped it, or
		 *		 2) We have popped it and deferred it (re-pushed it to a deferred packet).
		 * Either way we don't need to process any objects on this card now and we don't need to
		 * re-dirty the card as we know we will revisit it again later, and all cards for an
		 * active TLH are cleared when TLH is refreshed anyway.
		 *
		 * If concurrent work stack overflow has occurred the above conditions do not hold to
		 * relieve work stack overflow we empty packets by dirtying cards for their referenced
		 * objects. Therefore we cannot be sure all tracing into this particular TLH will be
		 * deferred.
		*/
		if (isObjectInActiveTLH(env,objectPtr) && !(stats->getConcurrentWorkStackOverflowOcurred())) {
			return true;
		}

		/* Is the object is in the remembered set ? */
		if(_extensions->objectModel.isRemembered(objectPtr)) {
			rememberedObjectsFound = true;
		}
		assume0(sizeToDo > sizeDone);
		sizeDone += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_DIRTY_CARD, (sizeToDo - sizeDone));
	}


	assume0(NULL == markedObjectIterator.nextObject());

	/* Update callers counter with amount cleaned for this card */
	*totalBytesCleaned += sizeDone;

	/* If we found any RS objects and RS overflow flag is ON the re-dirty the card
	 * so we re-visit any such objects later to trace nursery references. This will
	 * mean we re-trace objects not in the RS but RS overflow is assumed to be
	 * an exceptional circumstance.
	 */
	if (rememberedObjectsFound && (env->getExtensions()->isRememberedSetInOverflowState())) {
		*card = (Card)CARD_DIRTY;
	}

	return true;
}
/**
 * Initialize for final card cleaning.
 *
 * Called by STW to do any necessary initialization prior to final card cleaning.
 */
void
MM_ConcurrentCardTable::initializeFinalCardCleaning(MM_EnvironmentBase *env)
{
	if (_cardTableReconfigured){
		determineCleaningRanges(env);
	} else {
		resetCleaningRanges(env);
	}

	MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&_currentCleaningRange,
											 	(uintptr_t)_currentCleaningRange,
												(uintptr_t)_cleaningRanges);
	/* We process all cards in one go */
	_lastCardInPhase = _lastCard;
}

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
bool
MM_ConcurrentCardTable::finalCleanCards(MM_EnvironmentBase *env, uintptr_t *bytesTraced)
{
	uintptr_t traceCount = 0;
	Card * nextDirtyCard;
	omrobjectptr_t objectPtr;
	uintptr_t objects;
	uintptr_t cards = 0;
	bool phase2 = false;

	/* Set upper limit of refs we push before returning to one packets worth */
	uintptr_t maxPushes = _markingScheme->getWorkPackets()->getSlotsInPacket();

	/* Reset the number of pushes to this threads stack to zero */
	env->_workStack.clearPushCount();

	MM_MarkMap *markMap = _markingScheme->getMarkMap();
	
	for ( ;
		(nextDirtyCard= getNextDirtyCard(env, _finalCardCleanMask, false)) != NULL;
		) {

		/* Should never get EXCLUSIVE_VMACCESS_REQUESTED in final clean cards phase */
		assume0(nextDirtyCard != (Card *)EXCLUSIVE_VMACCESS_REQUESTED);

		/* Reset counters if we are now cleaning phase 2 cards */
		if(!phase2 && nextDirtyCard >= _firstCardInPhase2) {
			incFinalCleanedCards(cards, phase2);
			cards = 0;
			phase2 = true;
		}

		/* Clean the card before we trace into it */
		finalCleanCard(nextDirtyCard);
		cards += 1;

		/* Calculate address of first slot heap for the card to be cleaned... */
		uintptr_t *heapBase = (uintptr_t *)cardAddrToHeapAddr(env,nextDirtyCard);
		/* ..and address of last slot N.B Range is EXCLUSIVE */
		uintptr_t *heapTop = (uintptr_t *)((uint8_t *)heapBase + CARD_SIZE);

		/* Then iterate over all marked objects in the heap between the two addresses */
		MM_HeapMapIterator markedObjectIterator(_extensions, markMap, heapBase, heapTop);
		objects = 0;
		while (NULL != (objectPtr = markedObjectIterator.nextObject())) {
			objects +=1;
			traceCount += _markingScheme->scanObject(env, objectPtr, SCAN_REASON_DIRTY_CARD);
		}

		/* Have we pushed enough new refs ?*/
		if (env->_workStack.getPushCount() >= maxPushes) {
			/* yes..so return to process the refs pushed so far */
			break;
		}
	}

	/* We get here if we have pushed enough refs or we have cleaned all the cards
	 *
	 * First update number of dirty cards cleaned
	 */
	incFinalCleanedCards(cards, phase2);

	/* ..tell caller how many bytes we traced */
	*bytesTraced = traceCount;

	/* and tell caller if any more cards need cleaning */
	return (NULL == nextDirtyCard) ? false : true;
}

/**
 * Process TLH mark bits
 * Set or clear bits within the TLH mark bit map. The bit map contains one bit for
 * every card in the card table and we only set those bits for cards which are
 * wholly contained within a TLH.
 *
 * @param tlhbase - base address of a given tlh
 * @param tlhtop -  top address (exclusive) of given tlh
 * @param actions - If SET to set bits ON; otherwise CLEAR to set bits OFF
 *
 */
void
MM_ConcurrentCardTable::processTLHMarkBits(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, void *tlhBase, void *tlhTop, BitMapAction action)
{
	/* We should only get called for concurrently collectable subspaces */
	assume0(subspace->isConcurrentCollectable());

	/* Check passed tlh is reasonable */
	assume((uintptr_t *)tlhTop > (uintptr_t *)tlhBase,"processTLHMarkBits(): Invalid tlh limits passed");

	void *heapBase = getHeapBase();
	assume( (uintptr_t *)tlhBase >= (uintptr_t *)heapBase &&
			(uintptr_t *)tlhTop  <= (uintptr_t *)_heapAlloc,"processTLHMarkBits(): tlh not within heap");

	/* Round supplied tlhBase up to card boundary... */
	void *base = (void *)MM_Math::roundToCeiling(CARD_SIZE,(uintptr_t)tlhBase);

	/* ..and tlhTop down to card boundary */
	void *top = (void *)MM_Math::roundToFloor(CARD_SIZE,(uintptr_t)tlhTop);

	/* Check to see if tlh spans at least one complete card
	 * If tlh all within one card, i.e tlh < CARD_SIZE, then base > top
	 * if tlh spans no complete cards, then base = top
	 * Otherwise base < top
	 * */
	if (base < top) {
		uintptr_t  headBit, tailBit, headIndex, tailIndex, headBitMask, tailBitMask;

		/* If action is CLEAR and we have not had any work stack oveflow this cycle
		 * then we can safely clear all inner cards for this TLH. If an overflow has
		 * occurred we cannot clear the cards as we cannot guarantee that all tracing
		 * into them will have been deferred.
		 *
		 * We must not clear cards after we reset the tlh bits otherwise another
		 * thread may avoid tracing into a dirty card only for the card
		 * to be cleared later (rather than cleaned). We would not then retrace
		 * objects in that card and refernces may be missed.
		 */
		if ((CLEAR == action) && !(_collector->getConcurrentGCStats()->getConcurrentWorkStackOverflowOcurred())) {
			clearCardsInRange(env,base,top);

			/* Need a store barrier here so that all cards are cleared BEFORE we reset
		 	 * any TLH bits. storeSync will suffice here as it will ensure all cards
		 	 * cleared before we store new TLH bits.
		 	*/
		 	MM_AtomicOperations::storeSync();
		}

		uintptr_t numericalHeapBase = (uintptr_t)heapBase;
		/* Calculate the offset into heap of rounded tlhBase */
		headIndex = (uintptr_t)base - numericalHeapBase;
		/* Calculate bit index within a TLH mark bit slot for base */
		headBit = headIndex & TLH_MARKING_BIT_MASK;
		headBit >>= CARD_SIZE_SHIFT;
		/* ..and finally calculate the index of the slot within the TLH mark bit map for tlhBase */
		headIndex >>= TLH_MARKING_INDEX_SHIFT;

		/* Calculate the offset into heap of rounded tlhTtop */
		tailIndex = (uintptr_t)top - sizeof(uintptr_t) - numericalHeapBase;
		/* Calculate bit index within a TLH mark bit slot for top */
		tailBit = tailIndex & TLH_MARKING_BIT_MASK;
		tailBit >>= CARD_SIZE_SHIFT;
		/* ..and finally calculate the index of the slot within the TLH mark bit map for top */
		tailIndex >>= TLH_MARKING_INDEX_SHIFT;

		assume0(tailIndex >= headIndex);

		/* Convert the bit index to a mask  */
		headBitMask = getTLHMarkBitSetHead(headBit);
		tailBitMask = getTLHMarkBitSetTail(tailBit);

		/* First set all bits in head slot.
		 * If all bits in a sinlge slot the deduce composite bit pattern
		 */
		if (tailIndex == headIndex) {
			headBitMask &= tailBitMask;
		}

		if (SET == action) {
			setTLHMarkBits(env,headIndex,headBitMask);
		} else {
			assume0(CLEAR == action);
			clearTLHMarkBits(env,headIndex,headBitMask);
		}

		if (tailIndex > headIndex) {
			/* Bits span at least 2 TLH mark bit slots */
			headIndex += 1;
			/* Next mark all complete slots */
			uintptr_t mask = (SET == action) ? TLH_MARKING_SET_ALL_BITS : TLH_MARKING_CLEAR_ALL_BITS;
			while (headIndex < tailIndex) {
				_tlhMarkBits[headIndex++] = mask;
			}

			/* ..and finally mark the tail slot */
			if (SET == action) {
				setTLHMarkBits(env,tailIndex,tailBitMask);
			} else {
				clearTLHMarkBits(env,tailIndex,tailBitMask);
			}
		}

		if (SET == action) {
			/* Because we must ensure we defer tracing into any object in an active TLH
			 * we need a store barrier here so that the TLH mark bits are flushed
			 * to main memory before we expose any objects in the new TLH. That way no thread
			 * can possibly attempt to trace an object in a active TLH and find the TLH bits
			 * still off.
		 	 */
			MM_AtomicOperations::storeSync();

		}
	} /* tlh spans at least one card */
}

#if defined(DEBUG)
/**
 * Is card table empty
 *
 * heck that all cards in card table are clean.
 *
 * @return TRUE if cards are clean; FALSE otherwise
 */
bool
MM_ConcurrentCardTable::isCardTableEmpty(MM_EnvironmentBase *env)
{
	bool empty = true;
	Card *currentCard, *endCard;
	MM_HeapRegionDescriptor *region = NULL;
	MM_Heap *heap = _extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	while(NULL != (region = regionIterator.nextRegion())) {
		/* We are only interested in cards for concurrently collectable subspaces
		 * as these are the only ones we clear in first place
		 */
		if (region->getSubSpace()->isConcurrentCollectable()) {
			currentCard = heapAddrToCardAddr(env, region->getLowAddress());
			endCard = heapAddrToCardAddr(env, region->getHighAddress());

			while(currentCard < endCard) {
				if ((Card)CARD_DIRTY == *currentCard) {
					empty = false;
					break;
				}
				currentCard += 1;
			}
		}
	}
	
	return empty;
}

/**
 * Is TLH mark bits empty?
 * Check that all bits in the TLH mark bits map are OFF. All bits should be OFF
 *
 * @return TRUE if TLH mark bits all off; FALSE otherwise
 */
bool
MM_ConcurrentCardTable::isTLHMarkBitsEmpty(MM_EnvironmentBase *env)
{
	bool empty = true;

	/* Do we have any TLH mark bits to check ? */
	if (NULL == _tlhMarkBits) {
		return true;
	}

	MM_HeapRegionDescriptor *region = NULL;
	MM_Heap *heap = _extensions->heap;
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	uintptr_t numericalHeapBase = (uintptr_t)getHeapBase();
	while(NULL != (region = regionIterator.nextRegion())) {
		/* We only need to clear bits for segments owned by concurrently collectable subspaces */
		if (region->getSubSpace()->isConcurrentCollectable()) {
			uintptr_t currentSlot = (((uintptr_t)region->getLowAddress()) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;
			uintptr_t endSlot = (((uintptr_t)region->getHighAddress()) - numericalHeapBase) >> TLH_MARKING_INDEX_SHIFT;

			/* Make sure we walk all bits; endSlot may have been rounded down and we don't want to miss
			 * last few bits. We always round up the high address when we commit and clear so this should be
			 * OK.
			 */
			if ((endSlot << TLH_MARKING_INDEX_SHIFT) < (((uintptr_t)region->getHighAddress()) - numericalHeapBase)) {
				endSlot += 1;
			}

			while(currentSlot < endSlot) {
				if (_tlhMarkBits[currentSlot]!= 0) {
					empty = false;
					break;
				}
				currentSlot += 1;
			}
		}
	}
	
	return empty;
}
#endif /* DEBUG */

/**
 * Is object in an active TLH
 *
 * Determine whether the referenced object is within an active (incomplete) TLH.
 *
 * @param object - reference to the object to be checked
 * @return TRUE if reference is to an object within an active TLH; FALSE otherwise
 */
bool
MM_ConcurrentCardTable::isObjectInActiveTLH(MM_EnvironmentBase *env, omrobjectptr_t object)
{
	uintptr_t markIndex,markBit;
	bool active;

	/* Do we have any TLH mark bits to check ? */
	if (NULL == _tlhMarkBits) {
		return false;
	}

	/* First check reference is on heap */
	void *heapBase = getHeapBase();
	if ((void *)object >= heapBase && (void *)object < _heapAlloc) {

		/* its in the heap - calculate the offset into heap of object */
		markIndex = (uintptr_t)object - (uintptr_t)heapBase;
		/* Calculate bit index within a TLH mark bit slot for object*/
		markBit = markIndex & TLH_MARKING_BIT_MASK;
		markBit >>= CARD_SIZE_SHIFT;
		/* ..and finally calculate the index of the slot within the
		 * TLH mark bit map for tlhBase
		 */
		markIndex >>= TLH_MARKING_INDEX_SHIFT;

		active = (_tlhMarkBits[markIndex] & getTLHMarkBitMask(markBit)) ? true : false;
	} else {
		/* Reference not within the heap so it cant be in a TLH either */
		active =  false;
	}

	return active;
}

/**
 *
 * Determine whether the referenced card maps to an active (incomplete) TLH.
 *
 * @param card -  reference to the object to be checked
 * @return TRUE if card is within an active TLH; FALSE otherwise
 */
bool
MM_ConcurrentCardTable::isCardInActiveTLH(MM_EnvironmentBase *env, Card *card)
{
	uintptr_t markIndex,markBit;
	bool active;

	/* Do we have any TLH mark bits to check ? */
	if (NULL == _tlhMarkBits) {
		return false;
	}

	/* Calculate "bit"  index in TLH mark map which has one bit per
	 * card  in card table
	 */
	markIndex = (uintptr_t)(card - getCardTableStart());

	/* and then "bit" index within a TLH mark slot for card*/
	markBit = markIndex & TLH_CARD_BIT_MASK;

	/* ..and finally calculate the index of the slot within
	 * the TLH mark bit map which contain sthe bit we are
	 * intrested in
	 */
	markIndex >>= TLH_CARD_INDEX_SHIFT;

	/* if bit on then card maps to an active TLH */
	active = (_tlhMarkBits[markIndex] & getTLHMarkBitMask(markBit)) ? true : false;

	return active;
}

/**
 * Is object in a dirty card
 *
 * Determine whether the referenced object is within a  dirty card. No range
 * check on supplied object address. Used when we are sure the object is within
 * the old/tenure heap.
 *
 * @param object - reference to object to be checked
 * @return TRUE if reference is to an object within a dirty card; FALSE otherwise
 */
bool
MM_ConcurrentCardTable::isObjectInDirtyCardNoCheck(MM_EnvironmentBase *env, omrobjectptr_t object)
{
	bool dirty;

	Assert_MM_true(_extensions->isOld(object));

	Card *card = heapAddrToCardAddr(env,object);

	/* Is the card dirty or clean ? */
	dirty = ((Card)CARD_DIRTY == *card) ? true : false;

	return dirty;
}

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
bool
MM_ConcurrentCardTable::isObjectInUncleanedDirtyCard(MM_EnvironmentBase *env, omrobjectptr_t object)
{
	CleaningRange *currentRange;

	Assert_MM_true(_extensions->isOld(object));

	Card *card = heapAddrToCardAddr(env,object);

	/* First check card is dirty in first place */
	if ((Card)CARD_CLEAN == *card) {
		return false;
	}

	/* If card cleaning not yet started then card has not been cleaned */
	if (!isCardCleaningStarted()) {
		return true;
	}

	/* Get local copy of current cleaning range as it could change any time
	 * to last range
	 */
	currentRange = (CleaningRange *) _currentCleaningRange;
	/* If we have not finished card cleaning..have we cleaned card yet */
	/* TODO:Need to fix this for case where we have 2nd pass */
	if (!isCardCleaningComplete() &&
		(currentRange != _lastCleaningRange) &&
		(currentRange->nextCard < card)) {
		return true;
	}

	/* Card has already been cleaned this round */
	return false;
}


/**
 * Determine whether the referenced object is within a dirty card. Used if
 * object reference may not be in tenure or nursery.
 *
 * @param object - reference to object to be checked
 * @return TRUE if reference is to an object within a dirty card; FALSE otherwise
 */
bool
MM_ConcurrentCardTable::isObjectInDirtyCard(MM_EnvironmentBase *env, omrobjectptr_t object)
{
	bool dirty = false;

	/* As we only initialize the cards for the tenure area we must not
	 * inspect cards for any nursery objects
	 */
	if (_extensions->isOld(object)) {
		Card *card = heapAddrToCardAddr(env,object);

		/* Is the card dirty or clean ? */
		dirty = (*card == (Card)CARD_DIRTY) ? true : false;

	}
	return dirty;
}

/**
 * Determine ranges of card tblae which need cleaning.
 *
 * Populate the _cleaningRanges structure with details of those areas of card
 * table that need cleaning. We only need to clean cards which map to OLD area
 * segments.
 *
 */
void
MM_ConcurrentCardTable::determineCleaningRanges(MM_EnvironmentBase *env)
{
	bool initDone = false;

	while (!initDone) {
		uint32_t numRanges = 0;
		CleaningRange *nextRange = _cleaningRanges;

		_cardTableStats.totalCards = 0;

		/* Add init ranges for concurrently collectable segments */
		MM_HeapRegionDescriptor *region = NULL;
		MM_Heap *heap = _extensions->heap;
		MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
		GC_HeapRegionIterator regionIterator(regionManager);
		while(NULL != (region = regionIterator.nextRegion())) {
			/* Get reference to owning subspace */
			MM_MemorySubSpace *subspace = region->getSubSpace();

			/* Do we need to include this segments cards  */
			if (subspace->isActive() && (_cleanAllCards || subspace->isConcurrentCollectable())) {
				numRanges += 1;

				/* If there is space in cleaningRanges array so add it... */
				if (numRanges <= _maxCleaningRanges) {
					nextRange->baseCard = heapAddrToCardAddr(env, region->getLowAddress());
					nextRange->topCard = heapAddrToCardAddr(env, region->getHighAddress());
					nextRange->nextCard = nextRange->baseCard;
					nextRange->numCards = (uintptr_t)(nextRange->topCard - nextRange->baseCard);
					_cardTableStats.totalCards += nextRange->numCards;
					nextRange += 1;
				}
			}
		}

		/* Did all ranges fit in current array  ? */
		if (numRanges > _maxCleaningRanges) {
			/* We need to get a bigger initRanges array of i+1 elements but first
			 * free the one if have already, if any
			 */
			if (NULL != _cleaningRanges) {
				env->getForge()->free(_cleaningRanges);
			}

			uintptr_t sizeRequired = sizeof(CleaningRange) * numRanges;
			_cleaningRanges = (CleaningRange *) env->getForge()->allocate(sizeRequired, OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
			_maxCleaningRanges = numRanges;
		} else {
			/* Address first range for next round of card cleaning */
			MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&_currentCleaningRange,
																	(uintptr_t)_currentCleaningRange,
																	(uintptr_t)_cleaningRanges);

			/* ... and initialize to byte after last active range */
			_lastCleaningRange = nextRange;

			initDone = true;

		}
	}

	_cardTableReconfigured = false;
}

/**
 * Reset all cleanig ranges.
 *
 * Reset all ranges in _cleanRanges structure. This routine resets current
 * to base for all segments.
 *
 */
void
MM_ConcurrentCardTable::resetCleaningRanges(MM_EnvironmentBase *env)
{
	for (CleaningRange  *range =_cleaningRanges; range < _lastCleaningRange; range++) {
		range->nextCard = range->baseCard;
	}

	MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&_currentCleaningRange,
															(uintptr_t)_currentCleaningRange,
															(uintptr_t)_cleaningRanges);
}

/**
 * Get the next dirty card in card table.
 *
 * Find the next dirty card (as defined by cardmask) in the card table.
 *
 * @param cardMask - mask to apply to cards to identify those cards the caller
 * 					 is interested in
 *
 * @return Routine either returns address of next dirty card, NULL if no
 * more dirty cards, EXCLUSIVE_VMACCESS_REQUESTED if another thread waiting
 * for exclusive VM access.
 */
Card*
MM_ConcurrentCardTable::getNextDirtyCard(MM_EnvironmentBase *env, Card cardMask, bool concurrentCardClean)
{
	/* Get a local copy of next current range being cleaned */
	CleaningRange *currentRange = (CleaningRange *)_currentCleaningRange;

	/* Have we finished already ? */
	if (currentRange >= _lastCleaningRange ) {
		/* Yes..we are already done */
		return NULL;
	}

	/* Get a local copy of next card to check */
	Card *firstCard = (Card *)currentRange->nextCard;

	while (NULL != firstCard) {

		/* The last card we will process is either last card in current range or
		 * the last card to be cleaned in this phase of card cleaning.
		 */
		/* CMVC 132231 - cache _lastCardInPhase since it's volatile and min reads its arguments twice */
		Card *lastCardInPhase = _lastCardInPhase;
		Card *lastCardToClean = OMR_MIN(lastCardInPhase, currentRange->topCard);
		Card *nextDirtyCard, *currentCard;

		for (currentCard = firstCard; currentCard < lastCardToClean; currentCard++) {

			/* Are we are on an uintptr_t boundary? If so scan the card table a uintptr_t
	 		 * at a time until we find a slot which is non-zero or the end of card table
	 		 * found. This is based on the premise that the card table will be mostly
	 		 * empty and scanning an uintptr_t at a time will reduce the time taken to
	 		 * scan the card table.
	 		 */
			if (((Card)CARD_CLEAN == *currentCard) && (0 == (uintptr_t)currentCard % sizeof(uintptr_t))) {
				uintptr_t *nextSlot = (uintptr_t *)currentCard;
				/* Last card may be in middle of a slot so only scan up to an including last
				 * complete slots worth of cards; then go card at a time
				 **/
				uintptr_t *lastSlot = (uintptr_t *)MM_Math::roundToFloor(sizeof(uintptr_t), (uintptr_t)lastCardToClean);
				while ((nextSlot < lastSlot) && (SLOT_ALL_CLEAN == *nextSlot)) {
					nextSlot += 1;
				}
				/*
			     * Either end of scan or a slot which contains a dirty card found. Reset scan ptr
				 */
				currentCard = (Card *)nextSlot;

				if (currentCard >= lastCardToClean) {
					break;
				}
			}

			/* Have we found a card of interest yet ? */
			if (0 == (*currentCard & cardMask)) {
				/* No..so get next card. */
				continue;
			}

			/* Yes..so check to see if another thread got to next dirty card before us ? */
			if (firstCard != (Card *)currentRange->nextCard) {
				/* Yes..so re-sync with race winner and start scan again */
				break;
			} else {
				/* No .. so attempt to grab this card*/
				nextDirtyCard = currentCard;
				currentCard += 1;
				if (concurrentCardClean && env->isExclusiveAccessRequestWaiting()) {
					return (Card *)EXCLUSIVE_VMACCESS_REQUESTED;
				}

				/* Update next card to clean for next caller of getNextDirtyCard. If we fail
				 * then someone beat us to it so re-sync with race winner and start again
				 */
				if (firstCard != (Card *)MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&currentRange->nextCard,
											  							  (uintptr_t)firstCard,
											  							  (uintptr_t)currentCard)) {
					break;
				}
				
				return nextDirtyCard;
			}
		} /* of currentCard < lastCardToClean */

		/* We get here if we break out of FOR loop when another thread beat us to next
		 * dirty card or we reach then end of the card table.
		 *
		 * Did we reach end of card table segment ?
		 */
		if (currentCard < lastCardToClean) {
			/* No..so someone must have beat us to next dirty card. In which case we need
			 * to restart scan. First though make sure no thread is waiting for exclusive access.
			 */
			if (concurrentCardClean && env->isExclusiveAccessRequestWaiting()) {
				return (Card *)EXCLUSIVE_VMACCESS_REQUESTED;
			}

			firstCard = (Card *)currentRange->nextCard;
		} else if (currentCard >= currentRange->topCard) {
			assume0(currentCard == currentRange->topCard);
			/* Range complete so set nextCard of cleaning range to top card to show cleaning range finsished */
			MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&currentRange->nextCard, (uintptr_t)currentRange->nextCard, (uintptr_t)currentRange->topCard);
			
			/* Switch to next cleaning range */
			CleaningRange *nextRange =  currentRange + 1;
			MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&_currentCleaningRange, (uintptr_t)currentRange, (uintptr_t)nextRange);

			currentRange = (CleaningRange *)_currentCleaningRange;
			firstCard = currentRange < _lastCleaningRange ? (Card *)currentRange->nextCard : (Card *)NULL;
		} else {
			/* We have reached the last card to be processed in this phase
			 * of card cleaning. We must update _next card to clean so when we start to clean
			 * cards in next phase we start at last card in this phase. If we fail then
			 * another thread beat us to it
			 */
			if (firstCard != (Card *)MM_AtomicOperations::lockCompareExchange((volatile uintptr_t *)&currentRange->nextCard,
										  							  (uintptr_t)firstCard,
										  							  (uintptr_t)currentCard)) {
			}
			return NULL;
		}
	}

	/* All ranges processed, no more dirty cards */
	return NULL;
}

/**
 * Set TLH mark bits
 *
 * @param slotIndex - index of TLH mark bit slot to set
 * @param slotBits - bit mask which identifies which bits to be set
 *
 */
MMINLINE void
MM_ConcurrentCardTable::setTLHMarkBits(MM_EnvironmentBase *env, uintptr_t slotIndex, uintptr_t slotBits)
{
	volatile uintptr_t *tlhMarkMapSlot= (uintptr_t *)&(_tlhMarkBits[slotIndex]);
	uintptr_t oldValue, newValue;

	assume((*tlhMarkMapSlot & slotBits) == 0,"TLH mark bits already set!!");

	do {
		oldValue = *tlhMarkMapSlot;
		newValue = oldValue | slotBits;

	} while (oldValue != MM_AtomicOperations::lockCompareExchange(tlhMarkMapSlot, oldValue, newValue));
}

/**
 * Clear TLH mark bits
 *
 * @param slotIndex - index of TLH mark bit slot to set
 * @param slotBits - bit mask which identifes which bits need clearing
 *
 */
MMINLINE void
MM_ConcurrentCardTable::clearTLHMarkBits(MM_EnvironmentBase *env, uintptr_t slotIndex, uintptr_t slotBits)
{
	volatile uintptr_t *tlhMarkMapSlot = (uintptr_t *)&(_tlhMarkBits[slotIndex]);
	uintptr_t oldValue, newValue;
	/* All bits to be cleared should already be set */
	assume((*tlhMarkMapSlot & slotBits) == slotBits,"TLH mark bits to be cleared not set!!");

	do {
		oldValue = *tlhMarkMapSlot;
		newValue = oldValue & ~slotBits;

	} while (oldValue != MM_AtomicOperations::lockCompareExchange(tlhMarkMapSlot, oldValue, newValue));
}

/*
 * Card has marked objects ?
 *
 * Determine if there any any marked objects contained within the heap range
 * defined by the specified card.
 *
 * @param card - address of card
 * @return return TRUE is one or more marked objects in card; FALSE otherwise
 *
 */
bool
MM_ConcurrentCardTable::cardHasMarkedObjects(MM_EnvironmentBase *env, Card *card)
{
	bool hasMarked;
	/* Calculate address of first slot in heap for card to be scanned */
	uintptr_t *heapBase = (uintptr_t *)cardAddrToHeapAddr(env,card);
	/* ..and address of last slot N.B Range is EXCLUSIVE*/
	uintptr_t *heapTop = (uintptr_t *)((uint8_t *)heapBase + CARD_SIZE);

	/* Then iterate over all marked objects in the heap between the two addresses */
	MM_HeapMapIterator markedObjectIterator(_extensions, _markingScheme->getMarkMap(), heapBase, heapTop);

	/* At least one marked object ? */
	hasMarked = markedObjectIterator.nextObject() ? true : false;

	return hasMarked;
}

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
