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
 *******************************************************************************/

#include "omrcomp.h"
#include "sizeclasses.h"
#include "ModronAssertions.h"

#include "EnvironmentBase.hpp"
#include "FreeHeapRegionList.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "MemoryPoolAggregatedCellList.hpp"
#include "MemoryPoolSegregated.hpp"
#include "RegionPoolSegregated.hpp"
#include "Task.hpp"

#include "SweepSchemeSegregated.hpp"
#include "HeapRegionQueue.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * Allocate and initialize a new instance of the receiver.
 * @return a new instance of the receiver, or NULL on failure.
 */
MM_SweepSchemeSegregated *
MM_SweepSchemeSegregated::newInstance(MM_EnvironmentBase *env, MM_MarkMap *markMap)
{
	MM_SweepSchemeSegregated *instance;
	
	instance = (MM_SweepSchemeSegregated *)env->getForge()->allocate(sizeof(MM_SweepSchemeSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != instance) {
		new(instance) MM_SweepSchemeSegregated(env, markMap);
		if (!instance->initialize(env)) {
			instance->kill(env);
			instance = NULL;
		}
	}

	return instance;
}

/**
 * Free the receiver and all associated resources.
 */
void
MM_SweepSchemeSegregated::kill(MM_EnvironmentBase *env)
{
	tearDown(env); 
	env->getForge()->free(this);
}

/**
 * Intialize the SweepSchemeSegregated instance.
 * 
 */
bool
MM_SweepSchemeSegregated::initialize(MM_EnvironmentBase *env)
{
	return true;
}

/**
 * Teardown the SweepSchemeSegregated instance.
 * 
 */
void
MM_SweepSchemeSegregated::tearDown(MM_EnvironmentBase *env)
{
}

void
MM_SweepSchemeSegregated::yieldFromSweep(MM_EnvironmentBase *env, uintptr_t yieldSlackTime)
{
}

void
MM_SweepSchemeSegregated::sweep(MM_EnvironmentBase *env, MM_MemoryPoolSegregated *memoryPool, bool isFixHeapForWalk)
{
	_memoryPool = memoryPool;
	_isFixHeapForWalk = isFixHeapForWalk;

	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		preSweep(env);
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
	
#if defined(OMR_GC_ARRAYLETS)
	incrementalSweepArraylet(env);
	env->_currentTask->synchronizeGCThreads(env, UNIQUE_ID);
#endif /* OMR_GC_ARRAYLETS */
	incrementalSweepLarge(env);
	
	MM_RegionPoolSegregated *regionPool = _memoryPool->getRegionPool();
	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		regionPool->setSweepSmallPages(true);
		regionPool->resetSkipAvailableRegionForAllocation();
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}

	incrementalSweepSmall(env);
	regionPool->joinBucketListsForSplitIndex(env);

	if (env->_currentTask->synchronizeGCThreadsAndReleaseMaster(env, UNIQUE_ID)) {
		regionPool->setSweepSmallPages(false);
		postSweep(env);
		env->_currentTask->releaseSynchronizedGCThreads(env);
	}
}

void
MM_SweepSchemeSegregated::preSweep(MM_EnvironmentBase *env)
{
	_memoryPool->moveInUseToSweep(env);
}

void
MM_SweepSchemeSegregated::postSweep(MM_EnvironmentBase *env)
{
	incrementalCoalesceFreeRegions(env);
}

void
MM_SweepSchemeSegregated::sweepRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region)
{
	region->getMemoryPoolACL()->resetCounts();

	switch (region->getRegionType()) {

	case MM_HeapRegionDescriptor::SEGREGATED_SMALL:
		sweepSmallRegion(env, region);
		if (isClearMarkMapAfterSweep()) {
			unmarkRegion(env, region);
		}
		addBytesFreedAfterSweep(env, region);
		break;

#if defined(OMR_GC_ARRAYLETS)
	case MM_HeapRegionDescriptor::ARRAYLET_LEAF:
		sweepArrayletRegion(env, region);
		addBytesFreedAfterSweep(env, region);
		break;
#endif /* defined(OMR_GC_ARRAYLETS) */

	case MM_HeapRegionDescriptor::SEGREGATED_LARGE:
		sweepLargeRegion(env, region);
		break;

	default:
		Assert_MM_unreachable();
	}
}

void
MM_SweepSchemeSegregated::unmarkRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region)
{
	/* block unmark the whole region */
	uintptr_t slotIndexLow;
	uintptr_t slotIndexHigh;
	uintptr_t bitMask;
	uintptr_t cellSize = region->getCellSize();
	uintptr_t numCells = region->getNumCells();

	uintptr_t regionLowAddress = (uintptr_t)region->getLowAddress();
	uintptr_t regionHighAddress = (uintptr_t)region->getHighAddress();
	uintptr_t lastCellStart = regionLowAddress + (numCells - 1) * cellSize ;
	uintptr_t scanBitOffset = 1 << OMR_SIZECLASSES_LOG_SMALLEST;

	_markMap->getSlotIndexAndMask((omrobjectptr_t) regionLowAddress, &slotIndexLow, &bitMask);

	/*
	 * CMVC 149635 : We need to ensure we do not miss to clear any scan bit, which is set for the ref array copy optimization.
	 * To not miss the scan bit of an object that's in the last cell of a region, we pretend the last object starts
	 * at 16 bytes past its header, which is where we would set the scan bit on the mark map.
	 * Note we need to ensure that cell size is at least >16 bytes, otherwise 16 bytes past J9Object* would go off the end of page, hence the if check below.
	 * And since there is no scan bit for object that's 16 bytes, this should be safe.
	 */
	if (lastCellStart + scanBitOffset >= regionHighAddress) {
		_markMap->getSlotIndexAndMask((omrobjectptr_t) lastCellStart, &slotIndexHigh, &bitMask);
	} else {
		_markMap->getSlotIndexAndMask((omrobjectptr_t) (lastCellStart + scanBitOffset), &slotIndexHigh, &bitMask);
	}
	_markMap->setMarkBlock(slotIndexLow, slotIndexHigh, 0);
}

void
MM_SweepSchemeSegregated::sweepSmallRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region)
{
	uintptr_t cellSize = region->getCellSize();
	uintptr_t numCells = region->getNumCells();
	uintptr_t freeChunk = 0;
	uintptr_t freeChunkSize = 0;
	uintptr_t freeChunkCellCount = 0;
	uintptr_t numCellsInBlock = 0; /* block is the set of consecutive cells that are all unmarked or all marked */
	uintptr_t blockSizeInBytes = 0;
	void *lowAddress = region->getLowAddress();
	MM_MemoryPoolAggregatedCellList *memoryPoolACL = region->getMemoryPoolACL();

	uintptr_t minimumFreeEntrySize = _memoryPool->getMinimumFreeEntrySize();
	uintptr_t sweepCostToCheckYield = env->getExtensions()->sweepCostToCheckYield;
	uintptr_t sweepCostCounter = 0;

	uintptr_t lastCellAddress = (uintptr_t)lowAddress + cellSize * (numCells - 1);
	uintptr_t lastCellSlotIndex, lastCellBitMask;

	/* Reset the free cells of the memory pool */
	memoryPoolACL->resetFreeList();

	_markMap->getSlotIndexAndMask((omrobjectptr_t)lastCellAddress, &lastCellSlotIndex, &lastCellBitMask);

	for (uintptr_t currentCellAddress = (uintptr_t)lowAddress; currentCellAddress <= lastCellAddress; currentCellAddress += blockSizeInBytes) {
		uintptr_t initialSlotIndex, slotIndex, bitMask;

		_markMap->getSlotIndexAndMask((omrobjectptr_t) currentCellAddress, &initialSlotIndex, &bitMask);

		if (0 != (_markMap->getSlot(initialSlotIndex) & bitMask)) {

			numCellsInBlock = 1;
			blockSizeInBytes = cellSize;

			if (freeChunk) {
				if (addFreeChunk(memoryPoolACL, (uintptr_t *) freeChunk, freeChunkSize, minimumFreeEntrySize, freeChunkCellCount)) {
					sweepCostCounter += 3;
				}
				freeChunk = 0;
				freeChunkSize = 0;
				freeChunkCellCount = 0;
			}

			memoryPoolACL->setMarkCount(memoryPoolACL->getMarkCount() + numCellsInBlock);

			sweepCostCounter += 1;
			if (sweepCostCounter > sweepCostToCheckYield) {
				sweepCostCounter = 0;
				yieldFromSweep(env);
			}
		} else {
	 		/* attempt block sweeping slots with all 0s */
			if ((0 == _markMap->getSlot(initialSlotIndex)) && (initialSlotIndex < lastCellSlotIndex)) {
				slotIndex = initialSlotIndex;
				do {
					slotIndex++;
				} while ((0 == _markMap->getSlot(slotIndex)) && (slotIndex < lastCellSlotIndex));
				sweepCostCounter += (slotIndex - initialSlotIndex);
				/* nextCell is the lowest possible address of a cell
				 * that may have mark bit in slotIndex (first slot not being all 0s) */
				uintptr_t nextCell = _markMap->getFirstCellByMarkSlotIndex(slotIndex);
				numCellsInBlock = (nextCell - 1 - currentCellAddress) / cellSize + 1;
				blockSizeInBytes = numCellsInBlock * cellSize;
				/* end of block sweeping */
			}
			else {
				numCellsInBlock = 1;
				blockSizeInBytes = cellSize;
			}

			if (!freeChunk) {
				freeChunk = currentCellAddress;
				freeChunkSize = 0;
				freeChunkCellCount = 0;
			}
			freeChunkSize += blockSizeInBytes;
			freeChunkCellCount += numCellsInBlock;
		}
	}

	if (freeChunk) {
		if (addFreeChunk(memoryPoolACL, (uintptr_t *) freeChunk, freeChunkSize, minimumFreeEntrySize, freeChunkCellCount)) {
			sweepCostCounter += 3;
		}
	}
	if (sweepCostCounter > sweepCostToCheckYield) {
		sweepCostCounter = 0;
		yieldFromSweep(env);
	}

	/* Reset the free cells of the memory pool */
	memoryPoolACL->resetCurrentEntry();

	_memoryPool->getRegionPool()->addDarkMatterCellsAfterSweepForSizeClass(region->getSizeClass(), numCells - memoryPoolACL->getMarkCount() - memoryPoolACL->getFreeCount());
}

#if defined(OMR_GC_ARRAYLETS)
void
MM_SweepSchemeSegregated::sweepArrayletRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region)
{
	uintptr_t arrayletsPerRegion = env->getExtensions()->arrayletsPerRegion;
	MM_MemoryPoolAggregatedCellList *memoryPoolACL = region->getMemoryPoolACL();

	// TODO: should arraylet region really be using Small region counters?
	uintptr_t firstUnused = UDATA_MAX;
	for (uintptr_t i = 0; i < arrayletsPerRegion; i++) {
		if (region->isArrayletUsed(i)) {
			if (!_markMap->isBitSet((omrobjectptr_t) region->getArrayletParent(i))) {
				if (i < firstUnused) {
					firstUnused = i;
					region->setNextArrayletIndex(firstUnused);
				}
				region->clearArraylet(i);
				memoryPoolACL->incrementFreeCount();
			}
		} else {
			if (i < firstUnused) {
				firstUnused = i;
				region->setNextArrayletIndex(firstUnused);
			}
			memoryPoolACL->incrementFreeCount();
		}
	}
}
#endif /* defined(OMR_GC_ARRAYLETS) */

void
MM_SweepSchemeSegregated::sweepLargeRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region)
{
	MM_MemoryPoolAggregatedCellList *memoryPoolACL = region->getMemoryPoolACL();
	omrobjectptr_t object = (omrobjectptr_t) region->getLowAddress();

	if (_markMap->isBitSet(object)) {
		if (isClearMarkMapAfterSweep()) {
			_markMap->clearBit(object);
		}
	} else {
		memoryPoolACL->incrementFreeCount();
	}
}

void
MM_SweepSchemeSegregated::addBytesFreedAfterSweep(MM_EnvironmentBase *env, MM_HeapRegionDescriptorSegregated *region)
{
	/* Bytes freed by large regions are counted when the regions are returned to the free region list,
	 * see emptyRefionReturned
	 */
	/* Notify the allocation tracker of the bytes that have been freed */
	MM_MemoryPoolAggregatedCellList *memoryPoolACL = region->getMemoryPoolACL();
	uintptr_t currentFreeBytes = memoryPoolACL->getFreeCount();
	if (region->isSmall()) {
		currentFreeBytes *= region->getCellSize();
#if defined(OMR_GC_ARRAYLETS)
	} else if (region->isArraylet()) {
		currentFreeBytes *= env->getOmrVM()->_arrayletLeafSize;
#endif /* defined(OMR_GC_ARRAYLETS) */
	} else {
		Assert_MM_unreachable();
	}
	env->_allocationTracker->addBytesFreed(env, (currentFreeBytes - memoryPoolACL->getPreSweepFreeBytes()));
	memoryPoolACL->setPreSweepFreeBytes(currentFreeBytes);
}

uintptr_t
MM_SweepSchemeSegregated::resetCoalesceFreeRegionCount(MM_EnvironmentBase *env)
{
	return 0;
}

bool
MM_SweepSchemeSegregated::updateCoalesceFreeRegionCount(uintptr_t range)
{
	return false;
}

void
MM_SweepSchemeSegregated::incrementalCoalesceFreeRegions(MM_EnvironmentBase *env)
{
	resetCoalesceFreeRegionCount(env);

	MM_GCExtensionsBase *ext = env->getExtensions();
	MM_HeapRegionManager *regionManager = ext->heap->getHeapRegionManager();
	uintptr_t regionCount = regionManager->getTableRegionCount();
	MM_RegionPoolSegregated *regionPool = _memoryPool->getRegionPool();
	MM_FreeHeapRegionList *coalesceFreeList = regionPool->getCoalesceFreeList();
	
	uintptr_t yieldSlackTime = resetCoalesceFreeRegionCount(env);
	yieldFromSweep(env, yieldSlackTime);

	coalesceFreeList->push(regionPool->getSingleFreeList());
	coalesceFreeList->push(regionPool->getMultiFreeList());
	
	MM_HeapRegionDescriptorSegregated *coalescing = NULL;
	MM_HeapRegionDescriptorSegregated *currentRegion = NULL;
	
	for (uintptr_t i=0; i< regionCount; ) {
		
		currentRegion = (MM_HeapRegionDescriptorSegregated *)regionManager->mapRegionTableIndexToDescriptor(i);
		uintptr_t range = currentRegion->getRange();
		i += range;
		
		bool shouldYield = updateCoalesceFreeRegionCount(range);
		bool shouldClose = shouldYield || (i >= regionCount);
		
		if (currentRegion->isFree()) {
			coalesceFreeList->detach(currentRegion);
			bool joined = (range < MAX_REGION_COALESCE) && (coalescing != NULL && coalescing->joinFreeRangeInit(currentRegion));
			if (joined) {
				currentRegion = NULL;
			} else {
				shouldClose = true;
			}
		} else {
			currentRegion = NULL;
		}
		if (shouldClose && coalescing != NULL) {
			coalescing->joinFreeRangeComplete();
			regionPool->addFreeRegion(env, coalescing, true);
			coalescing = NULL;
		}
		if (shouldYield) {
			if (currentRegion != NULL) {
				regionPool->addFreeRegion(env, currentRegion, true);
				currentRegion = NULL;
			}
			yieldFromSweep(env, yieldSlackTime);
		} else {
			if (coalescing == NULL) {
				coalescing = currentRegion;
			}
		}
	}
	if (currentRegion != NULL) {
		regionPool->addFreeRegion(env, currentRegion, true);		
		currentRegion = NULL;
	}

	yieldFromSweep(env);
}

void
MM_SweepSchemeSegregated::incrementalSweepLarge(MM_EnvironmentBase *env)
{
	/* Sweep through large objects. */
	MM_RegionPoolSegregated *regionPool = _memoryPool->getRegionPool();
	MM_HeapRegionQueue *largeSweepRegions = regionPool->getLargeSweepRegions();
	MM_HeapRegionQueue *largeFullRegions = regionPool->getLargeFullRegions();
	MM_HeapRegionDescriptorSegregated *currentRegion;
	while ((currentRegion = largeSweepRegions->dequeue()) != NULL) {
		sweepRegion(env, currentRegion);
		
		if (currentRegion->getMemoryPoolACL()->getFreeCount() == 0) {
			largeFullRegions->enqueue(currentRegion);
		} else {
			currentRegion->emptyRegionReturned(env);
			regionPool->addFreeRegion(env, currentRegion);
		}
		yieldFromSweep(env);
	}
}

void
MM_SweepSchemeSegregated::incrementalSweepArraylet(MM_EnvironmentBase *env)
{
	uintptr_t arrayletsPerRegion = env->getExtensions()->arrayletsPerRegion;

	MM_RegionPoolSegregated *regionPool = _memoryPool->getRegionPool();
	MM_HeapRegionQueue *arrayletSweepRegions = regionPool->getArrayletSweepRegions();
	MM_HeapRegionQueue *arrayletAvailableRegions = regionPool->getArrayletAvailableRegions();
	MM_HeapRegionDescriptorSegregated *currentRegion;
	
	while ((currentRegion = arrayletSweepRegions->dequeue()) != NULL) {
		sweepRegion(env, currentRegion);
		
		if (currentRegion->getMemoryPoolACL()->getFreeCount() != arrayletsPerRegion) {
			arrayletAvailableRegions->enqueue(currentRegion);
		} else {
			currentRegion->emptyRegionReturned(env);
			regionPool->addFreeRegion(env, currentRegion);
		}
		
		yieldFromSweep(env);
	}
}

uintptr_t
MM_SweepSchemeSegregated::resetSweepSmallRegionCount(MM_EnvironmentBase *env, uintptr_t yieldSmallRegionCount)
{
	return 0;
}

bool
MM_SweepSchemeSegregated::updateSweepSmallRegionCount()
{
	return false;
}

void
MM_SweepSchemeSegregated::incrementalSweepSmall(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *ext = env->getExtensions();
	bool shouldUpdateOccupancy = ext->nonDeterministicSweep;
	MM_RegionPoolSegregated *regionPool = _memoryPool->getRegionPool();
	uintptr_t splitIndex = env->getSlaveID() % (regionPool->getSplitAvailableListSplitCount());

	/* 
	 * Iterate through the regions so that each region is processed exactly once.
	 * Interleaved sweeping: free up some number of regions of all sizeClasses right away
	 * so that application have somewhere to allocate from and minimize the usage of
	 * non deterministic sweeps. 
	 * Any marked objects are unmarked.
	 * If a region holds a marked object, then the region is kept active; 
	 * if a region contains no marked objects, then it can be returned to a free list.
	 */
	MM_SizeClasses *sizeClasses = ext->defaultSizeClasses;
	while (regionPool->getCurrentTotalCountOfSweepRegions()) {
		for (uintptr_t sizeClass = OMR_SIZECLASSES_MIN_SMALL; sizeClass <= OMR_SIZECLASSES_MAX_SMALL; sizeClass++) {
			while (regionPool->getCurrentCountOfSweepRegions(sizeClass)) {
				float yetToComplete = (float)regionPool->getCurrentCountOfSweepRegions(sizeClass) / regionPool->getInitialCountOfSweepRegions(sizeClass);
				float totalYetToComplete = (float)regionPool->getCurrentTotalCountOfSweepRegions() / regionPool->getInitialTotalCountOfSweepRegions();
				
				if (yetToComplete < totalYetToComplete) {
					break;
				}
				
				MM_HeapRegionQueue *sweepList = regionPool->getSmallSweepRegions(sizeClass);
				MM_HeapRegionDescriptorSegregated *currentRegion;
				uintptr_t numCells = sizeClasses->getNumCells(sizeClass);
				uintptr_t sweepSmallRegionsPerIteration = calcSweepSmallRegionsPerIteration(numCells);
				uintptr_t yieldSlackTime = resetSweepSmallRegionCount(env, sweepSmallRegionsPerIteration);
				uintptr_t actualSweepRegions;
				if ((actualSweepRegions = sweepList->dequeue(env->getRegionWorkList(), sweepSmallRegionsPerIteration)) > 0) {
					regionPool->decrementCurrentCountOfSweepRegions(sizeClass, actualSweepRegions);
					regionPool->decrementCurrentTotalCountOfSweepRegions(actualSweepRegions);
					uintptr_t freedRegions = 0, processedRegions = 0;
					MM_HeapRegionQueue *fullList = env->getRegionLocalFull();
					while ((currentRegion = env->getRegionWorkList()->dequeue()) != NULL) {
						sweepRegion(env, currentRegion);
						if (currentRegion->getMemoryPoolACL()->getFreeCount() < numCells) {
							uintptr_t occupancy = (currentRegion->getMemoryPoolACL()->getMarkCount() * 100) / numCells;
							/* Maintain average occupancy needed for nondeterministic sweep heuristic */
							if (shouldUpdateOccupancy) {
								regionPool->updateOccupancy(sizeClass, occupancy);
							}
							if (currentRegion->getMemoryPoolACL()->getMarkCount() == numCells) {
								/* Return full regions to full list */
								fullList->enqueue(currentRegion);
							} else {
								regionPool->enqueueAvailable(currentRegion, sizeClass, occupancy, splitIndex);
							}
						} else {
							currentRegion->emptyRegionReturned(env);
							currentRegion->setFree(1);
							env->getRegionLocalFree()->enqueue(currentRegion);
							freedRegions++;
						}
						processedRegions++;
						
						if (updateSweepSmallRegionCount()) {
							yieldFromSweep(env, yieldSlackTime);
						}
					}
					regionPool->addSingleFree(env, env->getRegionLocalFree());				
					regionPool->getSmallFullRegions(sizeClass)->enqueue(fullList);
					yieldFromSweep(env, yieldSlackTime);
				}
			} /* end of while(currentTotalCountOfSweepRegions); */
		}
	}
}

#endif /* OMR_GC_SEGREGATED_HEAP */
