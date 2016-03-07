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
 *******************************************************************************/

#include "omrcfg.h"
/* TODO 90354 rm: #include "j9.h" */
#if defined(AIXPPC) || defined(LINUXPPC)

#if defined(OMR_GC_MODRON_CONCURRENT_MARK)

#include <string.h>
#include <stdio.h>
#include <stdlib.h> 

#include "AtomicOperations.hpp" 
#include "ConcurrentCardTableForWC.hpp"
#include "ConcurrentPrepareCardTableTask.hpp"
#include "Debug.hpp"
#include "Dispatcher.hpp"
#include "MemorySubSpace.hpp"
#include "WorkPackets.hpp"

/**
 * Create a new instance of MM_ConcurrentCardTableForWC object
 *
 * @param markingScheme - reference to the colllectors marking scheme
 * @param collector - reference to the collector itself
 *
 * @return reference to new MM_ConcurrentCardTableForWC object or NULL
 */
MM_ConcurrentCardTable *
MM_ConcurrentCardTableForWC::newInstance(MM_EnvironmentStandard *env, MM_Heap *heap, MM_MarkingScheme *markingScheme, MM_ConcurrentGC *collector)
{
	MM_ConcurrentCardTableForWC *cardTable;
	
	cardTable = (MM_ConcurrentCardTableForWC *)env->getForge()->allocate(sizeof(MM_ConcurrentCardTableForWC), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != cardTable) {
		new(cardTable) MM_ConcurrentCardTableForWC(env, markingScheme, collector);
		if (!cardTable->initialize(env, heap)) {
			cardTable->kill(env);
			return NULL;
		}
	}
	return cardTable;
}

/**
 * Initialize a new MM_ConcurrentCardTableForWC object 
 * Call super class to initialize common attributes and then override those
 * fields we need to when operating on a weak ordered architecture. 
 *
 * @return TRUE if object initialized OK; FALSE otherwise
 */
bool
MM_ConcurrentCardTableForWC::initialize(MM_EnvironmentStandard *env, MM_Heap *heap)
{
	/* First call super class initialize */
	if (!MM_ConcurrentCardTable::initialize(env, heap)) {
		goto error_no_memory;
	}	
	
	_callback = env->getExtensions()->collectorLanguageInterface->concurrentGC_createSafepointCallback(env);

	if (NULL == _callback) {
		goto error_no_memory;
	}

	_callback->registerCallback(env, prepareCardTableAsyncEventHandler, this, true);

	/* Override default card cleaning masks used by getNextDirtycard */
	_concurrentCardCleanMask = CONCURRENT_CARD_CLEAN_MASK_FOR_WC;
	_finalCardCleanMask = FINAL_CARD_CLEAN_MASK_FOR_WC;
	
	return true;
	
error_no_memory:
	return false;
}

void
MM_ConcurrentCardTableForWC::tearDown(MM_EnvironmentBase *env)
{
	MM_CardTable::tearDown(env);
	_callback->kill(env);
	_callback = NULL;
}

/**
 * Aysnc callback routine to prepare card table for cleaning
 * 
 */
void 
MM_ConcurrentCardTableForWC::prepareCardTableAsyncEventHandler(OMR_VMThread *omrVMThread, void *userData)
{
	MM_ConcurrentCardTableForWC *cardTable  = (MM_ConcurrentCardTableForWC *)userData;
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(omrVMThread);

	cardTable->prepareCardTable(env);
}

/**
 * Prepare the card table for cleaning.
 * 
 */
void
MM_ConcurrentCardTableForWC::prepareCardTable(MM_EnvironmentStandard *env)
{
	CardCleanPhase currentPhase = _cardCleanPhase;
	
	/* Check if another thread beat us to it */
	if(cardTableNeedsPreparing(currentPhase)) {
		if (getExclusiveCardTableAccess(env, currentPhase, true)) {
			prepareCardsForCleaning(env);
			releaseExclusiveCardTableAccess(env);
		}	
	}
}	
/**
 * Prepare cards for cleaning.
 * 
 * On a weakly ordered platform the update of a reference and dirtying of a card could
 * be reversed so a card is dirtied before the associated update is visible. As we need
 * to be sure that we see all the updated references when we rescan the cards marked 
 * objects we need to ensure the card table and objects are synchronized. As we don't want
 * to have to issue a store barrier after every reference update (far too expensive) we
 * instead stop all threads to ensure all its updates are flushed. We then walk the card table
 * and flag any dirty cards (i.e set them to CARD_CLEAN_SAFE) to show that all updates to its
 * associated objects are visible. We then restart all threads and start concurrent card cleaning.
 * If a card is subsequently redirtied it is set again to CARD_DIRTY by the write barrier.
 * 
 * Thus during the concurrent phase we can clean all unclean cards (CARD_DIRTY or CARD_CLEAN_SAFE)
 * but any marked as CARD_DIRTY at final card cleaning time must be re-cleaned to ensure we don't
 * miss any object references. 
 */
void
MM_ConcurrentCardTableForWC::prepareCardsForCleaning(MM_EnvironmentStandard *env)
{
	/* First call superclass which will set card cleaning range for next
	 * phase of card cleaning
	 */
	MM_ConcurrentCardTable::prepareCardsForCleaning(env);
		
	/* Any cards to prepare  */
	if (_firstCardInPhase >= _lastCardInPhase) {
		/* No..so nothing to do */
		return;
	}	
	
	/* Now get assistance from all the gc slave threads to prepare the dirty cards
	 * in the card table for cleaning by marking dirty cards as safe to clean.
	 */ 
	 
	//ToDo make this more intelligent; no point using helpers if only a "few" cards
	//to process. Also, may want to chop chunk up into less parts in 2nd phase 
	//of concurrent card cleaning 		 
	MM_ConcurrentPrepareCardTableTask prepareCardTableTask(env, 
														  _dispatcher,
														  this,
														  _firstCardInPhase,
														  _lastCardInPhase,
														  MARK_DIRTY_CARD_SAFE); 
	_dispatcher->run(env, &prepareCardTableTask);
	
	/* Remember we prepared some cards as we may need to 
	 * reverse the process before final card cleaning 
	 * if we dont actually get around to cleaning them all
	 */ 
	_cardTablePreparedForCleaning = true;
}	

/**
 * Get exclusive control of the card table to prepare it for card cleaning.
 * 
 * @param currentPhase the current card clean pahse at point of call
 * @param threadAtSafePoint Whether the calling thread is at a safe point or not
 * @return TRUE if exclusive access acquired; FALSE otherwise
 */
MMINLINE bool
MM_ConcurrentCardTableForWC::getExclusiveCardTableAccess(MM_EnvironmentStandard *env, CardCleanPhase currentPhase, bool threadAtSafePoint)
{
	/* Because the WC CardTable requires exclusive access to prepare the cards for cleaning, we cannot gain
	 * exclusive access to the card table if the thread is not at a safe point. Request async call back
	 * on this thread if this is the case so we can prepare card table 
	 */
	if(!threadAtSafePoint) {
		_callback->requestCallback(env);
		return false;
	}

	/* Get the current global gc count */
	uintptr_t gcCount = _extensions->globalGCStats.gcCount;

	bool acquired = false;

    while(!acquired) {

        /* Get exclusive VM access so we can prepare the card table. */
        acquired = env->tryAcquireExclusiveVMAccess();

        /* We may or may not have exclusive access but another thread may have beat us to it and moved things along
         * anyway,or we may have collected and be in a different concurrent cycle. If we are not in the same concurrent 
         * collection cycle or the _cardCleanPhase is not as it was when we requested exclusive access then nothing to do
         * anymore. Otherwise if we got beat to exclusive access request it again and re-check, if we got it prepare the
         * card table.
         */  
        if ((gcCount != _extensions->globalGCStats.gcCount) || (currentPhase != _cardCleanPhase)) {
            /* Nothing to do so get out  */
            if (acquired) {
                env->releaseExclusiveVMAccess(); 
            }
            return false;
        }
    }

    MM_AtomicOperations::lockCompareExchangeU32((volatile uint32_t*)&_cardCleanPhase,
                                            (uint32_t) currentPhase,
                                            (uint32_t) currentPhase + 1);
	assume0(cardTableBeingPrepared(_cardCleanPhase));                                            
    return true;
}

/**
 * Release exclusive control of the card table
 */
MMINLINE void
MM_ConcurrentCardTableForWC::releaseExclusiveCardTableAccess(MM_EnvironmentStandard *env)
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
											
	/* Cancel any outstanding events on other threads */
	_callback->cancelCallback(env);
											
	/* We are done so release exclusive now so mutators can restart */
	env->releaseExclusiveVMAccess(); 										
}

/**
 * Count the number of "active" cards in a given chunk of card table.
 * As a chunk may contain holes (segmented heap) we use the _cleaningRanges 
 * structure to work out how many cards there are within a defined range 
 * of cards; "chunkStart" thru "chunkEnd".
 * 
 * @param chunkStart First card in the chunk of cards to be processed 
 * @param chunkEnd Last card in the chunk
 * 
 * @return Number of active cards in the the chunk
 */ 
uintptr_t
MM_ConcurrentCardTableForWC::countCardsInRange(MM_EnvironmentStandard *env, Card *chunkStart, Card *chunkEnd)
{
	uintptr_t cardsInRange = 0;
	CleaningRange  *cleaningRange = _cleaningRanges;
	
	/* Find cleaning range that corresponds to start of card table range to be counted */
	while (cleaningRange < _lastCleaningRange && cleaningRange->topCard < chunkStart) {
		cleaningRange++;
	}	

	/* Count all cards until we get to end of all cleaning ranges or end of 
	 * chunk whose cards we are counting
	 */		
	while (cleaningRange < _lastCleaningRange && cleaningRange->baseCard < chunkEnd) {
		Card *first = OMR_MAX(chunkStart, cleaningRange->baseCard);
		Card *last = OMR_MIN(chunkEnd,cleaningRange->topCard);
		/* Add cards in this cleaning range into total cards to be cleaned */
		cardsInRange += (uintptr_t)(last - first);
		cleaningRange++;
	}

	return cardsInRange;	
}	

/**
 * Prepare a chunk of the card table for card cleaning.
 * 
 * This function is passed the start and end of a chunk of the card table which needs preparing
 * for card cleaning; be it concurrent or final card cleaning. As the chunk may contain 
 * holes (non-contiguous heap) we use the _cleaningRanges array to determine which cards within
 * the chunk actually need processing, ie a chunk can span part of one or more cleaning ranges.
 * The "action" passed to this function defines what we do to each unclean card within the chunk. 
 * If action is MARK_DIRTY_CARD_SAFE we modify all cards with value CARD_DIRTY to CARD_CLEAN_SAFE; 
 * for action MARK_SAFE_CARD_DIRTY we reset any cards marked as CARD_CLEAN_SAFE to CARD_DIRTY.
 *  
 * @param chunkStart - first card to be processed 
 * @param chunkEnd   - last card to be processed
 * @param action - defines what to do to each un-clean card, value is either MARK_DIRTY_CARD_SAFE or
 * MARK_SAFE_CARD_DIRTY.
 */ 
void
MM_ConcurrentCardTableForWC::prepareCardTableChunk(MM_EnvironmentStandard *env, Card *chunkStart, Card *chunkEnd, CardAction action)
{
	uintptr_t prepareUnitFactor, prepareUnitSize;
	
 	/* Determine the size of card table work unit */ 
 	prepareUnitFactor = env->_currentTask->getThreadCount();
 	prepareUnitFactor = ((prepareUnitFactor == 1) ? 1 : prepareUnitFactor * PREPARE_PARALLEL_MULTIPLIER);
	prepareUnitSize = countCardsInRange(env, chunkStart, chunkEnd);
	prepareUnitSize = prepareUnitSize / prepareUnitFactor;
	prepareUnitSize = prepareUnitSize > 0 ? MM_Math::roundToCeiling(PREPARE_UNIT_SIZE_ALIGNMENT, prepareUnitSize) : 
                                            PREPARE_UNIT_SIZE_ALIGNMENT;												 
	
	/* Walk all card cleaning ranges to determine which cards should be prepared */
	for (CleaningRange *range=_cleaningRanges; range < _lastCleaningRange; range++) {
		
		Card *prepareAddress;
		uintptr_t prepareSizeRemaining;
		uintptr_t currentPrepareSize;
		/* Is this range strictly before the chunk we are looking for? */
		if (chunkStart >= range->topCard){
			/* Yes, so just skip to the next */
			continue;
		}
		/* Is this range strictly after the chunk we are looking for? */
		if (chunkEnd <= range->baseCard){
			/* Yes, so our work is done */
			break;
		}
		
		/* Walk the segment in chunks the size of the heapPrepareUnit size */ 
		prepareAddress = chunkStart > range->baseCard ? chunkStart : range->baseCard;
		prepareSizeRemaining = chunkEnd > range->topCard ? range->topCard - prepareAddress :
														  chunkEnd - prepareAddress;   	

		while(prepareSizeRemaining > 0 ) {
			/* Calculate the size of card table chunk to be processed next */
			currentPrepareSize = (prepareUnitSize > prepareSizeRemaining) ? prepareSizeRemaining : prepareUnitSize;
				
			/* Check if the thread should clear the corresponding mark map range for the current heap range */ 
			if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
				Card *firstCard,*endCard;
				firstCard = prepareAddress;
				endCard = prepareAddress + currentPrepareSize;
				
				for (Card *currentCard = firstCard; currentCard < endCard; currentCard++) {
					/* Are we are on an uintptr_t boundary ?. If so scan the card table a uintptr_t
					 * at a time until we find a slot which is non-zero or the end of card table
					 * found. This is based on the premise that the card table will be mostly
					 * empty and scanning an uintptr_t at a time will reduce the time taken to
					 * scan the card table.
					 */
					if (((Card)CARD_CLEAN == *currentCard) &&
						((uintptr_t)currentCard % sizeof(uintptr_t) == 0)) {
						 uintptr_t *nextSlot= (uintptr_t *)currentCard;
						while ( ((Card *)nextSlot < endCard) && (*nextSlot == SLOT_ALL_CLEAN) ) {
							nextSlot++;
						}
						
						/*
						 * Either end of scan or a slot which contains a dirty card found. Reset scan ptr
						 */
						currentCard= (Card *)nextSlot; 

						/* End of card table reached ? */
						if (currentCard >= endCard) {
							break; 
						}  
					}

					if (MARK_DIRTY_CARD_SAFE == action) {
						/* Is next card dirty ? If so flag it as safe to clean */	
						if ((Card)CARD_DIRTY == *currentCard) {
							/* If card has marked objects we need to clean it */
							if (cardHasMarkedObjects(env, currentCard)) {
								*currentCard = (Card)CARD_CLEAN_SAFE;
							} else {	
								*currentCard = (Card)CARD_CLEAN;
							}	
						}
					} else {
						assume0(action == MARK_SAFE_CARD_DIRTY);
						if ((Card)CARD_CLEAN_SAFE == *currentCard) {
							*currentCard = (Card)CARD_DIRTY;
						}
					}
				}
			} /* of J9MODRON_HANDLE_NEXT_WORK_UNIT */	
			
			/* Move to the next address range in the segment */ 
			prepareAddress += currentPrepareSize; 
			prepareSizeRemaining -= currentPrepareSize; 
		}	
	}	
} 

/**
 * Initialize for final card cleaning.
 * 
 * If we prepared some cards during concurrent phase then we need to check to ensure
 * we managed to process them all, i.e. did we complete concurrent card cleaning pass. If
 * not we need to reset any prepared cards (i.e those set to CARD_CLEAN_SAFE) which were
 * not cleaned back to card_DIRTY so that they are rescanned during final card cleaning.
 */ 
void
MM_ConcurrentCardTableForWC::initializeFinalCardCleaning(MM_EnvironmentStandard *env)
{
	/* Did we get as far as preparing the card table for cleaning ? */
	if (_cardTablePreparedForCleaning ) {
		/* Yes..So we must reverse any cards that got prepared */
		if (_currentCleaningRange < _lastCleaningRange) {
			/* Get assistance from all gc slave threads to do the work */ 
			MM_ConcurrentPrepareCardTableTask prepareCardTableTask(env, 
																  _dispatcher,
																  this,
																  (Card *)_currentCleaningRange->nextCard,
																  _lastCard,
																  MARK_SAFE_CARD_DIRTY); 
			_dispatcher->run(env, &prepareCardTableTask);
		}	
		
		/* Reset for next cycle */
		_cardTablePreparedForCleaning = false;
	}
	
	/* Then call the superclass to complete regular initialization */
	MM_ConcurrentCardTable::initializeFinalCardCleaning(env);
}	

#endif /* OMR_GC_MODRON_CONCURRENT_MARK */
#endif /* AIXPPC || LINUXPPC */
