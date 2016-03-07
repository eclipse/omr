/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
 *
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

#include "omrcfg.h"
#if defined (OMR_GC_HEAP_CARD_TABLE)

#include "CardTable.hpp"

#include "AtomicOperations.hpp"
#include "CardCleaner.hpp"
#include "GCExtensionsBase.hpp"
#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "MemoryManager.hpp"
#include "HeapRegionDescriptor.hpp"
#include "Dispatcher.hpp"
#include "Task.hpp"

#include "ModronAssertions.h"

bool
MM_CardTable::initialize(MM_EnvironmentBase *env, MM_Heap *heap)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	bool initialized = false;
	
	uintptr_t cardTableSizeRequired = calculateCardTableSize(env, heap->getMaximumPhysicalRange());
	
	/* Instantiate the Virtual Memory object for the card table */
	MM_MemoryManager *memoryManager = extensions->memoryManager;
	if (memoryManager->createVirtualMemoryForMetadata(env, &_cardTableMemoryHandle, extensions->heapAlignment, cardTableSizeRequired)) {
		_cardTableStart = (Card *)(memoryManager->getHeapBase(&_cardTableMemoryHandle));
		/* Initialize _heapbase; we will reset _heapAlloc in heapAddRange()/heapRemoveRange() as heap changes */
		_heapBase = (void *)heap->getHeapBase();
		_heapAlloc = (void *)heap->getHeapTop();
		_cardTableVirtualStart = (Card *) ((uintptr_t)_cardTableStart - (((uintptr_t)getHeapBase()) >> CARD_SIZE_SHIFT));
		initialized = true;
	}

	return initialized;
}

/**
 * Destroy a card table object by invoking the kill method on the
 * card table, debug card table and TLH mark map objects.
 *
 */
void
MM_CardTable::tearDown(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemoryManager *memoryManager = extensions->memoryManager;
	/* Get rid of the virtual memory allocated for card table */
	memoryManager->destroyVirtualMemory(env, &_cardTableMemoryHandle);
}

uintptr_t
MM_CardTable::calculateCardTableSize(MM_EnvironmentBase *env, uintptr_t heapSize)
{
	return MM_Math::roundToCeiling(sizeof(uint32_t), (MM_Math::roundToCeiling(CARD_SIZE, heapSize) / CARD_SIZE) * sizeof(Card));
}

bool
MM_CardTable::commitCardTableMemory(MM_EnvironmentBase *env, Card *lowCard, Card *highCard)
{
	bool commited = true;
	MM_GCExtensionsBase *extensions = env->getExtensions();

	if(extensions->isFvtestForceCardTableCommitFailure()) {
		commited = false;
		Trc_MM_CardTable_cardTableCommitFailureForced(env->getLanguageVMThread());
	} else {
		MM_MemoryManager *memoryManager = extensions->memoryManager;
		commited = memoryManager->commitMemory(&_cardTableMemoryHandle, (void *)lowCard, (uintptr_t)highCard - (uintptr_t)lowCard);
		if (!commited) {
			Trc_MM_CardTable_cardTableCommitFailed(env->getLanguageVMThread(), (void *)lowCard, (uintptr_t)highCard - (uintptr_t)lowCard);
		}
	}
	return commited;
}

bool
MM_CardTable::decommitCardTableMemory(MM_EnvironmentBase *env, Card *lowCard, Card *highCard, Card *lowValidCard, Card *highValidCard)
{
	Assert_MM_true((lowCard >= lowValidCard) || (lowCard < highValidCard)); 
	Assert_MM_true((highCard > lowValidCard) || (highCard <= highValidCard));

	bool decommited = true;
	MM_GCExtensionsBase *extensions = env->getExtensions();

	if (extensions->isFvtestForceCardTableDecommitFailure()) {
		decommited = false;
		Trc_MM_CardTable_cardTableDecommitFailureForced(env->getLanguageVMThread());
	} else {
		/*
		 *  Note: it is possible that after aligning to virtual memory page size highAddress <= lowAddress
		 *  This just mean that we can not decommit now
		 */
		void *lowAddress = getLowAddressToRelease(env, lowCard);
		void *highAddress = getHighAddressToRelease(env, highCard);

		if (lowAddress < highAddress) {
			MM_MemoryManager *memoryManager = extensions->memoryManager;
			decommited = memoryManager->decommitMemory(&_cardTableMemoryHandle, lowAddress, ((uintptr_t)highAddress) - ((uintptr_t)lowAddress), lowAddress, highAddress);
			if (!decommited) {
				Trc_MM_CardTable_cardTableDecommitFailed(env->getLanguageVMThread(), lowAddress, ((uintptr_t)highAddress) - ((uintptr_t)lowAddress), lowAddress, highAddress);
			}
		}
	}
	return decommited;
}

void *
MM_CardTable::getLowAddressToRelease(MM_EnvironmentBase *env, void *low)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemoryManager *memoryManager = extensions->memoryManager;

	Assert_MM_true(low >= getCardTableStart());
	Assert_MM_true(low <= memoryManager->getHeapTop(&_cardTableMemoryHandle));

	void *result = low;
	uintptr_t pageSize = memoryManager->getPageSize(&_cardTableMemoryHandle);

	Assert_MM_true (0 != pageSize);
	
	/* rounded down address to virtual memory page */
	void *lowAddressPageAligned = (void *)MM_Math::roundToFloor(pageSize,(uintptr_t)low);

	if (lowAddressPageAligned < low) {
		/* Low address is not aligned */
		void *lowAddressToCheck = lowAddressPageAligned;
		if (lowAddressToCheck < getCardTableStart()) {
			/* if aligned down address out of card table correct it */
			lowAddressToCheck = getCardTableStart();
		}
		/* Check, can we release memory from aligned down address */
		if (canMemoryBeReleased(env, lowAddressToCheck, low)) {
			/* Yes we can! */
			result = lowAddressPageAligned;
		} else {
			/* we can not release memory - so round address up*/
			result = (void *)MM_Math::roundToCeiling(pageSize,(uintptr_t)low);
		}
	}

	return result;
}

void *
MM_CardTable::getHighAddressToRelease(MM_EnvironmentBase *env, void *high)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemoryManager *memoryManager = extensions->memoryManager;

	Assert_MM_true(high >= getCardTableStart());

	void *topAddress = memoryManager->getHeapTop(&_cardTableMemoryHandle);

	Assert_MM_true(high <= topAddress);

	void *result = high;
	uintptr_t pageSize = memoryManager->getPageSize(&_cardTableMemoryHandle);
	
	Assert_MM_true(0 != pageSize);
	
	/* rounded up address to virtual memory page */
	void *highAddressPageAligned = (void *)MM_Math::roundToCeiling(pageSize,(uintptr_t)high);

	if (high < highAddressPageAligned) {
		/* High address is not aligned */
		if (highAddressPageAligned >= topAddress) {
			/* if aligned up address out of card table memory correct it */
			highAddressPageAligned = topAddress;
		}
		/* Check, can we release memory from aligned up address */
		if (canMemoryBeReleased(env, high, highAddressPageAligned)) {
			/* Yes we can! */
			result = highAddressPageAligned;
		} else {
			/* we can not release memory - use next aligned address */
			result = (void *)MM_Math::roundToFloor(pageSize,(uintptr_t)high);
		}
	}

	return result;
}

bool
MM_CardTable::canMemoryBeReleased(MM_EnvironmentBase *env, void *low, void *high)
{
	bool result = true;
	MM_GCExtensionsBase *extensions = env->getExtensions();
	uintptr_t regionSizeCardSize = extensions->regionSize >> CARD_SIZE_SHIFT;
	Assert_MM_true(regionSizeCardSize > 0);
	MM_HeapRegionManager *regionManager = extensions->heapRegionManager;

	void *regionAligned = (void *)MM_Math::roundToFloor(regionSizeCardSize, (uintptr_t)low);
	
	do {
		void *heapAddress = (void *)cardAddrToHeapAddr(env, (Card *)regionAligned);
		MM_HeapRegionDescriptor *region = regionManager->regionDescriptorForAddress(heapAddress);
		if((NULL != region) && (NULL != region->getSubSpace())) {
			result = false;
			break;
		}
		regionAligned = (void *)((uintptr_t)regionAligned + regionSizeCardSize);
	} while (regionAligned < high);
	
	return result;
}

bool
MM_CardTable::isDirtyOrValue(MM_EnvironmentBase *env, void *heapAddr, Card cardValue)
{
	Card *cardAddr = heapAddrToCardAddr(env, heapAddr);
	return (CARD_DIRTY == *cardAddr) || (cardValue == *cardAddr);
}

void
MM_CardTable::dirtyCard(MM_EnvironmentBase *env, omrobjectptr_t objectRef)
{
	dirtyCardWithValue(env, objectRef, CARD_DIRTY);
}

void
MM_CardTable::dirtyCardWithValue(MM_EnvironmentBase *env, omrobjectptr_t objectRef, Card newValue)
{
	Assert_MM_true(CARD_CLEAN != newValue);
	Assert_MM_true(CARD_INVALID != newValue);
	/* Check passed reference within the heap */
	if(((uintptr_t *)objectRef >= (uintptr_t *)getHeapBase()) && ((uintptr_t *)objectRef < (uintptr_t *)_heapAlloc)) {
		Card *card = heapAddrToCardAddr(env, objectRef);

		/* If card not already set to this value, update it */
		Card oldValue = *card;
		if (newValue != oldValue) {
			Assert_MM_true((CARD_DIRTY == newValue) || (CARD_CLEAN == oldValue));
			*card = newValue;
		}
	}
}

void
MM_CardTable::dirtyCardRange(MM_EnvironmentBase *env, void *heapAddrFrom, void *heapAddrTo)
{
	Card *card = heapAddrToCardAddr(env, heapAddrFrom);
	Card *toCard = heapAddrToCardAddr(env, heapAddrTo);
		
	for ( ; card < toCard; card++) {
		/* If card not already dirty then dirty it */
		if ((Card)CARD_DIRTY != *card) {
			*card = (Card)CARD_DIRTY;
		}
	}
}

Card *
MM_CardTable::heapAddrToCardAddr(MM_EnvironmentBase *env, void *heapAddr)
{
	/* Check passed reference within the heap */
	Assert_MM_true((uintptr_t *)heapAddr >= (uintptr_t *)getHeapBase());
	Assert_MM_true((uintptr_t *)heapAddr <= (uintptr_t *)_heapAlloc);

	uintptr_t virtualStart = (uintptr_t)getCardTableVirtualStart();
	return (Card *)(virtualStart + (((uintptr_t)heapAddr) >> CARD_SIZE_SHIFT));
}

void *
MM_CardTable::cardAddrToHeapAddr(MM_EnvironmentBase *env, Card *cardAddr)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemoryManager *memoryManager = extensions->memoryManager;

	/* Check passed card address is within the card table  */
	Assert_MM_true((void *)cardAddr >= getCardTableStart());
	Assert_MM_true((void *)cardAddr <= memoryManager->getHeapTop(&_cardTableMemoryHandle));

	return (void *)((uintptr_t)getHeapBase() + (((uintptr_t)cardAddr - (uintptr_t)getCardTableStart()) << CARD_SIZE_SHIFT));
}

MMINLINE void
MM_CardTable::cleanRange(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, Card *low, Card *high)
{
	Card *thisCard = low;
	Card *endCard = high;
	uintptr_t cardsCleaned = 0;
	while (thisCard < endCard) {
		if (CARD_CLEAN != *thisCard) {
			void *lowAddress = (void *)cardAddrToHeapAddr(env, thisCard);
			void *highAddress = (void *)((uintptr_t)lowAddress + CARD_SIZE);
			
			cardCleaner->clean(env, lowAddress, highAddress, thisCard);
			cardsCleaned += 1;
		}
		thisCard += 1;
	}
	env->_cardCleaningStats._cardsCleaned += cardsCleaned;
}

void
MM_CardTable::cleanCardTable(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner)
{
	/* clean cards for entire heap range */
	cleanCardTableForRange(env, cardCleaner, getHeapBase(), _heapAlloc);
}

void
MM_CardTable::cleanCardTableForRange(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, void *lowAddress, void *highAddress)
{
	uintptr_t oldVMState = env->pushVMstate(cardCleaner->getVMStateID());
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uint64_t cleanStartTime = omrtime_hires_clock();

	Card *lowCard = heapAddrToCardAddr(env, lowAddress);
	Card *finalCard = heapAddrToCardAddr(env, highAddress);
	uintptr_t cardsInCleaningRange = (2*1024*1024) / CARD_SIZE;
	uintptr_t compleCleaningRanges = ((uintptr_t)finalCard - (uintptr_t)lowCard) / cardsInCleaningRange;
	Card *highCard = lowCard + (compleCleaningRanges * cardsInCleaningRange);

	Assert_MM_true(((uintptr_t)finalCard - (uintptr_t)highCard) < cardsInCleaningRange);

	while(lowCard < highCard) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			cleanRange(env, cardCleaner, lowCard, lowCard + cardsInCleaningRange);
		}
		lowCard += cardsInCleaningRange;
	}

	if (highCard < finalCard) {
		if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
			cleanRange(env, cardCleaner, highCard, finalCard);
		}
	}

	uint64_t cleanEndTime = omrtime_hires_clock();
	env->_cardCleaningStats.addToCardCleaningTime(cleanStartTime, cleanEndTime);
	env->popVMstate(oldVMState);
}

void
MM_CardTable::cleanCardsInRegion(MM_EnvironmentBase *env, MM_CardCleaner *cardCleaner, MM_HeapRegionDescriptor *region)
{
	uintptr_t oldVMState = env->pushVMstate(cardCleaner->getVMStateID());
	/* look up the card range for this region and walk it */
	Card *lowCard = heapAddrToCardAddr(env, region->getLowAddress());
	Card *highCard = heapAddrToCardAddr(env, region->getHighAddress());
	
	cleanRange(env, cardCleaner, lowCard, highCard);
	env->popVMstate(oldVMState);
}

uintptr_t
MM_CardTable::clearCardsInRange(MM_EnvironmentBase *env, void* heapBase, void* heapTop)
{
	Assert_MM_true(heapTop >= heapBase);

	Card *firstCard = heapAddrToCardAddr(env,heapBase);
	Card *lastCard = heapAddrToCardAddr(env,heapTop);
	uintptr_t sizeToClear = (uint8_t *)lastCard - (uint8_t *)firstCard;

	/* We can't use OMRZeroMemory() here as that requires the  area to
	 * be cleared to be uintptr_t aligned
	 */
	memset((void *)firstCard, 0, sizeToClear);

	return sizeToClear;
}

void
MM_CardTable::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

#if defined(OMR_GC_VLHGC)
bool
MM_CardTable::commitCardsForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
{
	void *base = region->getLowAddress();
	Card *lowCard = heapAddrToCardAddr(env, base);
	void *top = region->getHighAddress();
	Card *highCard = heapAddrToCardAddr(env, top);
	return commitCardTableMemory(env, lowCard, highCard);
}

bool
MM_CardTable::setNumaAffinityCorrespondingToHeapRange(MM_EnvironmentBase *env, uintptr_t numaNode, void *baseOfHeapRange, void *topOfHeapRange)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemoryManager *memoryManager = extensions->memoryManager;
	Assert_MM_true(0 != numaNode);
	bool hasPhysicalNUMASupport = (0 != MM_GCExtensionsBase::getExtensions(env->getOmrVM())->_numaManager.isPhysicalNUMASupported());
	/* it only makes sense to call this if NUMA support is enabled - this is not made to be called from highly generic code */
	Assert_MM_true(hasPhysicalNUMASupport);
	Card *lowCard = heapAddrToCardAddr(env, baseOfHeapRange);
	Card *highCard = heapAddrToCardAddr(env, topOfHeapRange);
	/* this rounding could cause us to 'adopt' neighbouring cards for this node */
	uintptr_t pageSize = memoryManager->getPageSize(&_cardTableMemoryHandle);
	/* low address must be page aligned */
	uintptr_t lowAddress = MM_Math::roundToFloor(pageSize, (uintptr_t)lowCard);
	/* high address might be not aligned but should be in valid memory range
	 * it will be aligned to page size inside vmem->setNumaAffinity()
	 */
	uintptr_t highAddress = (uintptr_t)highCard;
	return memoryManager->setNumaAffinity(&_cardTableMemoryHandle, numaNode, (void*)lowAddress, highAddress - lowAddress);
}
#endif /* defined(OMR_GC_VLHGC) */

#endif /* defined (OMR_GC_HEAP_CARD_TABLE) */

