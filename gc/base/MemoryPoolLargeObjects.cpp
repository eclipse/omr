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
#include "omrthread.h"
#include <algorithm>

#include "MemoryPoolLargeObjects.hpp"

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#include "CycleState.hpp"
#include "Debug.hpp"
#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpace.hpp"
#include "MemorySubSpaceRegionIterator.hpp"
#include "HeapRegionDescriptor.hpp"
#include "LargeObjectAllocateStats.hpp"

#include "ModronAssertions.h"

#define JVM_INITIALIZATION_COLLECTIONS 4


/**
 * Called at the start of a global collect.
 * We use this broadcast event to update target LOA ratio for this collect
 */
void
reportGlobalGCIncrementStart(J9HookInterface** hook, uintptr_t eventNum, void* eventData, void* userData)
{
	MM_GlobalGCIncrementStartEvent* event = (MM_GlobalGCIncrementStartEvent*)eventData;
	MM_EnvironmentBase* env = MM_EnvironmentBase::getEnvironment(event->currentThread);

	((MM_MemoryPoolLargeObjects*)userData)->preCollect(env,
													   env->_cycleState->_gcCode.isExplicitGC(),
													   env->_cycleState->_gcCode.isAggressiveGC(),
													   event->bytesRequested);
}

/**
 * Initialization
 */
MM_MemoryPoolLargeObjects*
MM_MemoryPoolLargeObjects::newInstance(MM_EnvironmentBase* env, MM_MemoryPoolAddressOrderedListBase* largeObjectArea, MM_MemoryPoolAddressOrderedListBase* smallObjectArea)
{
	MM_MemoryPoolLargeObjects* memoryPool;

	memoryPool = (MM_MemoryPoolLargeObjects*)env->getForge()->allocate(sizeof(MM_MemoryPoolLargeObjects), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != memoryPool) {
		memoryPool = new (memoryPool) MM_MemoryPoolLargeObjects(env, largeObjectArea, smallObjectArea);
		if (!memoryPool->initialize(env)) {
			memoryPool->kill(env);
			memoryPool = NULL;
		}
	}
	return memoryPool;
}

bool
MM_MemoryPoolLargeObjects::initialize(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	bool debug = _extensions->debugLOAResize;

	if (!MM_MemoryPool::initialize(env)) {
		return false;
	}

	/* it's important that the LOA is registered first - so that it's last in the memorypool chain */
	registerMemoryPool(_memoryPoolLargeObjects);
	registerMemoryPool(_memoryPoolSmallObjects);

	/* Ensure we always expand the heap by at least largeObjectMinimumSize bytes */
	_extensions->heapExpansionMinimumSize = OMR_MAX(_extensions->heapExpansionMinimumSize, _extensions->largeObjectMinimumSize);


	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);
	/* Register hook for global GC start - needed to trigger update of LOA
	 * ratio at start of a collect.
	 *
	 * THIS MUST HAPPEN AFTER VERBOSE GC PRINTS LOA SIZE. IT MUST NOT HAPPEN ON SCAVENGES.
	 */
	(*mmPrivateHooks)->J9HookRegisterWithCallSite(mmPrivateHooks, J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START, reportGlobalGCIncrementStart, OMR_GET_CALLSITE(), (void*)this);

	uintptr_t minimumFreeEntrySize = OMR_MAX(_memoryPoolLargeObjects->getMinimumFreeEntrySize(), _memoryPoolSmallObjects->getMinimumFreeEntrySize());
	/* this memoryPool can be used by scavenger, maximum tlh size should be max(_extensions->tlhMaximumSize, _extensions->scavengerScanCacheMaximumSize) */
#if defined(OMR_GC_MODRON_SCAVENGER)
	uintptr_t tlhMaximumSize = OMR_MAX(_extensions->tlhMaximumSize, _extensions->scavengerScanCacheMaximumSize);
#else /* OMR_GC_MODRON_SCAVENGER */
	uintptr_t tlhMaximumSize = _extensions->tlhMaximumSize;
#endif /* OMR_GC_MODRON_SCAVENGER */
	_largeObjectAllocateStats = MM_LargeObjectAllocateStats::newInstance(env, (uint16_t)_extensions->largeObjectAllocationProfilingTopK, _extensions->largeObjectAllocationProfilingThreshold, _extensions->largeObjectAllocationProfilingVeryLargeObjectThreshold, (float)_extensions->largeObjectAllocationProfilingSizeClassRatio / (float)100.0,
																		 _extensions->heap->getMaximumMemorySize(), tlhMaximumSize + minimumFreeEntrySize, _extensions->tlhMinimumSize);

	if (NULL == _largeObjectAllocateStats) {
		return false;
	}

	if (debug) {
		omrtty_printf("LOA Initialize: SOA subpool %p LOA subpool %p\n ", _memoryPoolSmallObjects, _memoryPoolLargeObjects);
	}

	_loaFreeRatioHistory = (double*)env->getForge()->allocate(_extensions->loaFreeHistorySize * sizeof(double), OMR::GC::AllocationCategory::FIXED, OMR_GET_CALLSITE());

	if (NULL == _loaFreeRatioHistory){
		return false;
	}

	/*
	 * initialize _loaFreeRatioArray to 0 (even though initialy LOA shold have a free ratio of 1),
	 * to prevent early contraction
	 */

	for (int i = 0; i < _extensions->loaFreeHistorySize; i++ ){
		_loaFreeRatioHistory[i] = 0;
	}

	return true;
}

void
MM_MemoryPoolLargeObjects::tearDown(MM_EnvironmentBase* env)
{
	J9HookInterface** mmPrivateHooks = J9_HOOK_INTERFACE(_extensions->privateHookInterface);

	/* Unregister the global GC hooks for this instance */
	(*mmPrivateHooks)->J9HookUnregister(mmPrivateHooks, J9HOOK_MM_PRIVATE_GLOBAL_GC_INCREMENT_START, reportGlobalGCIncrementStart, (void*)this);

	if (NULL != _memoryPoolSmallObjects) {
		_memoryPoolSmallObjects->kill(env);
		_memoryPoolSmallObjects = NULL;
	}

	if (NULL != _memoryPoolLargeObjects) {
		_memoryPoolLargeObjects->kill(env);
		_memoryPoolLargeObjects = NULL;
	}

	if (NULL != _largeObjectAllocateStats) {
		_largeObjectAllocateStats->kill(env);
		_largeObjectAllocateStats = NULL;
	}
	if (NULL != _loaFreeRatioHistory){
		env->getForge()->free(_loaFreeRatioHistory);
	}


	MM_MemoryPool::tearDown(env);
}

void
MM_MemoryPoolLargeObjects::reset(Cause cause)
{
	/* Reset aggregate (LOA + SOA) free space statistics in this memory pool ..*/
	//to do do we really need aggregate stats in MPLO ?
	MM_MemoryPool::reset(cause);

	/* and in LOA and SOA */
	_memoryPoolSmallObjects->reset();
	_memoryPoolLargeObjects->reset();

	/* Reset size of smallest object which caused a AF in SOA */
	_soaObjectSizeLWM = ((uintptr_t) - 1);
	resetFreeEntryAllocateStats(_largeObjectAllocateStats);
	resetLargeObjectAllocateStats();
}

/**
 * Perform any pre-collection work on pool.
 */
void
MM_MemoryPoolLargeObjects::preCollect(MM_EnvironmentBase* env, bool systemGC, bool aggressive, uintptr_t bytesRequested)
{
	bool debug = _extensions->debugLOAFreelist;
	double newLOARatio;

	/* Dont resize LOA if its a system GC */
	if (!systemGC) {
		if (aggressive) {
			newLOARatio = resetTargetLOARatio(env);
		} else {
			newLOARatio = calculateTargetLOARatio(env, bytesRequested);
		}
		resetLOASize(env, newLOARatio);
	}

	if (debug) {
		if (_memoryPoolSmallObjects->getApproximateFreeMemorySize() > 0) {
			_memoryPoolSmallObjects->printCurrentFreeList(env, "SOA");
		}
		if (_memoryPoolLargeObjects->getApproximateFreeMemorySize() > 0) {
			_memoryPoolLargeObjects->printCurrentFreeList(env, "LOA");
		}
	}
}

/**
 * Perform resize LOA, currently it only tried to contract LOA.
 */
void
MM_MemoryPoolLargeObjects::resizeLOA(MM_EnvironmentBase* env)
{
	bool debug = _extensions->debugLOAResize;

	_soaFreeBytesAfterLastGC = _memoryPoolSmallObjects->getApproximateFreeMemorySize();

	float  minimumFreeRatio = ((float)_extensions->heapFreeMinimumRatioMultiplier) / ((float)_extensions->heapFreeMinimumRatioDivisor);

	uintptr_t minimumSOAFreeRequired = uintptr_t (_soaSize * minimumFreeRatio);

	if ((_soaFreeBytesAfterLastGC < minimumSOAFreeRequired) && (LOA_EMPTY != _currentLOABase)) {
		MM_HeapLinkedFreeHeader* moveListHead;
		MM_HeapLinkedFreeHeader* moveListTail;
		uintptr_t moveListMemoryCount;
		uintptr_t moveListMemorySize;
		uintptr_t spaceDelta;
		void* newLOABase;
		double oldLOARatio;
		uintptr_t contractRequired;
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);

		/* Calculate LOA size based on new loa ratio */
		uintptr_t loaMinimumSize = MM_Math::roundToFloor(_extensions->heapAlignment,
				(uintptr_t)((float)_memorySubSpace->getActiveMemorySize() * _extensions->largeObjectAreaMinimumRatio));

		/* No point having a LOA less than minimum size of a free entry for pool */
		loaMinimumSize = loaMinimumSize < _memoryPoolLargeObjects->getMinimumFreeEntrySize() ? 0 : loaMinimumSize;

		/* We are short on free memory, but try to be fair to both SOA and LOA.
		 * largeObjectAreaInitialRatio is considered optimal and will be used as long as free SOA meets min free ratio requirement.
		 * If, for example, SOA free is only 40% of the minimum, than we will contract LOA to 40% of the optimal size.
		 */

		uintptr_t newLOAsize = _soaFreeBytesAfterLastGC * (uintptr_t)( _extensions->largeObjectAreaInitialRatio / minimumFreeRatio);

		if (debug) {
			omrtty_printf("post GC resize LOA: newLOAsize %zu\n", newLOAsize);
		}

		if (newLOAsize < _loaSize){

			/* The above formula makes sense if LOA is completely free. But if LOA is already partially occupied than we want
			 * to contract LOA even less (since LOA clearly shows being useful).
			 *
			 * The size we contract by is proportionate to free LOA ratio. If, for example, LOA is completely full (no free memory),
			 * we will not contract LOA at all.
			 *
			 * We do not just take the current LOA occupancy, but we look into the history of several global GCs and take the highest LOA occupancy.
			 *
			 */

			contractRequired = (uintptr_t)((_loaSize - newLOAsize) * _minLOAFreeRatio);

			newLOAsize = _loaSize - contractRequired;

			if (debug) {
				omrtty_printf("newLOAsize adjusted by LOA occupnacy %zu\n", newLOAsize);
			}

			if ((double)newLOAsize/(double)_memorySubSpace->getActiveMemorySize() < _extensions->largeObjectAreaMinimumRatio){

				contractRequired = (uintptr_t)((double)_loaSize - ((double)(_memorySubSpace->getActiveMemorySize() * _extensions->largeObjectAreaMinimumRatio)));
				newLOAsize = _loaSize - contractRequired;
				if (debug) {
					omrtty_printf("newLOAsize adjusted min LOA ratio %zu\n", newLOAsize);
				}
			}

			/* If minimum required now zero then there is no storage available for transfer */
			if (0 < contractRequired) {


				/* LOA base may land in a middle of a live object, but it should be fine */
				newLOABase = (void*)((uint8_t*)_currentLOABase + contractRequired);

				newLOABase = (void*)MM_Math::roundToCeiling(_extensions->heapAlignment, (uintptr_t)newLOABase);

				_memoryPoolLargeObjects->removeFreeEntriesWithinRange(env, _currentLOABase, newLOABase,
						_memoryPoolSmallObjects->getMinimumFreeEntrySize(),
						moveListHead, moveListTail, moveListMemoryCount, moveListMemorySize);

				if (NULL != moveListHead) {
					_memoryPoolSmallObjects->addFreeEntries(env, moveListHead, moveListTail, moveListMemoryCount, moveListMemorySize);
				}

				spaceDelta = (NULL != newLOABase) ? (uintptr_t)newLOABase - (uintptr_t)_currentLOABase : _loaSize;
				oldLOARatio = _currentLOARatio;

				if ((_loaSize - spaceDelta) < _memoryPoolLargeObjects->getMinimumFreeEntrySize()) {
					spaceDelta = _loaSize;
					_soaSize += spaceDelta;
					_loaSize = 0;
					_currentLOABase = LOA_EMPTY;
					_currentLOARatio = 0;
				} else {
					_soaSize += spaceDelta;
					_loaSize -= spaceDelta;
					_currentLOABase = newLOABase;
					_currentLOARatio = ((double)_loaSize) / (_loaSize + _soaSize);

					/* Rounding during float operations may result in a new LOA ratio less than
					 * minimum so fix up if necessary
					 */
					if (_currentLOARatio < _extensions->largeObjectAreaMinimumRatio) {
						_currentLOARatio = _extensions->largeObjectAreaMinimumRatio;
					}
					assume0(0 != _currentLOARatio);
				}

				if (debug) {
					omrtty_printf("LOA Rebalanced to meet minimum SOA requirements: LOA ratio has decreased from %.3f --> %.3f\n",
							oldLOARatio,
							_currentLOARatio);
				}

				_extensions->heap->getResizeStats()->setLastLoaResizeReason(LOA_CONTRACT_MIN_SOA);
				_memorySubSpace->reportHeapResizeAttempt(env, spaceDelta , HEAP_LOA_CONTRACT);

				/* Verify all pools in valid state after we are done */
				assume0(_memoryPoolSmallObjects->isMemoryPoolValid(env, true));
				assume0(_memoryPoolLargeObjects->isMemoryPoolValid(env, true));
				assume0(_memoryPoolSmallObjects->isValidListOrdering());
				assume0(_memoryPoolLargeObjects->isValidListOrdering());
			}
		}
	}
}

/**
 * Decide if we need collector to perform a complete rebuild of freelist
 *
 * We need to ensue that a completely rebuild the freelist  if we need to redistribute free memory between
 * LOA and SOA in order to meet SOA minimum free space requirements.
 *
 * @return TRUE if a complete sweep is required; FALSE otherwise
 */
bool
MM_MemoryPoolLargeObjects::completeFreelistRebuildRequired(MM_EnvironmentBase* env)
{
	uintptr_t soaFree = _memoryPoolSmallObjects->getApproximateFreeMemorySize();
	uintptr_t minimumRequired = (_soaSize / _extensions->heapFreeMinimumRatioDivisor) * _extensions->heapFreeMinimumRatioMultiplier;

	return ((soaFree < minimumRequired) && (LOA_EMPTY != _currentLOABase));
}

double
MM_MemoryPoolLargeObjects::calculateTargetLOARatio(MM_EnvironmentBase* env, uintptr_t allocSize)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	bool debug = _extensions->debugLOAResize;
	double newLOARatio = _currentLOARatio;
	float maxLOAFreeRatio = ((float)_extensions->heapFreeMaximumRatioMultiplier) / ((float)_extensions->heapFreeMinimumRatioDivisor);
	uintptr_t loaFreeBytes = _memoryPoolLargeObjects->getActualFreeMemorySize();

	/*
	 * shift elements to make room for current loa free Ratio
	 */
	for (int i = _extensions->loaFreeHistorySize; i > 0 ; i--){
		_loaFreeRatioHistory[i] = _loaFreeRatioHistory[i-1];
	}
	if (0 == _loaSize) {
		_loaFreeRatioHistory[0] = (double)0;
	} else {
		_loaFreeRatioHistory[0] = (double)loaFreeBytes / (double)_loaSize;
	}

	_minLOAFreeRatio = *std::min_element(_loaFreeRatioHistory, _loaFreeRatioHistory + _extensions->loaFreeHistorySize);

	/* If we have had an allocation failure in the LOA then we need to consider
	 * whether its time we expanded the LOA
	 */
	if (allocSize >= _extensions->largeObjectMinimumSize) {
		/* If the allocation size is 1/5 times greater than current LOA size..expand LOA */
		if (allocSize > _loaSize / LOA_EXPAND_TRGGER3) {
			if (_currentLOARatio < _extensions->largeObjectAreaMaximumRatio) {
				newLOARatio += LOA_RESIZE_AMOUNT_NORMAL;
			}
		} else if (_currentLOARatio >= _extensions->largeObjectAreaInitialRatio) {
			if (_minLOAFreeRatio < LOA_EXPAND_TRIGGER1) {
				if (_currentLOARatio < _extensions->largeObjectAreaMaximumRatio) {
					newLOARatio += LOA_RESIZE_AMOUNT_NORMAL;
				}
			}
		} else {
			/* currentLOARatio < _extensions->largeObjectAreaInitialRatio */
			if (_minLOAFreeRatio < LOA_EXPAND_TRIGGER2) {
				assume0(_extensions->largeObjectAreaInitialRatio <= _extensions->largeObjectAreaMaximumRatio);
				newLOARatio += LOA_RESIZE_AMOUNT_NORMAL;
			}
		}
		/* Belt and braces check. Because _currentLOARatio is a float we need to check that
		 * we have not exceeded maximum and if we have round down to maximum
		 */
		if (newLOARatio > _extensions->largeObjectAreaMaximumRatio) {
			newLOARatio = _extensions->largeObjectAreaMaximumRatio;
		}

		if (_currentLOARatio != newLOARatio) {
			_extensions->heap->getResizeStats()->setLastLoaResizeReason(LOA_EXPAND_FAILED_ALLOCATE);
		}
	} else if (_minLOAFreeRatio > maxLOAFreeRatio) {
		if (_currentLOARatio >= _extensions->largeObjectAreaMinimumRatio) {
			newLOARatio -= LOA_RESIZE_AMOUNT_NORMAL;
			/* Ensure we do not contract below minimum */
			if (newLOARatio < _extensions->largeObjectAreaMinimumRatio) {
				newLOARatio = _extensions->largeObjectAreaMinimumRatio;
			}
			_extensions->heap->getResizeStats()->setLastLoaResizeReason(LOA_CONTRACT_UNDERUTILIZED);
		}

	} else if (newLOARatio < _extensions->largeObjectAreaMinimumRatio) {
		newLOARatio = _extensions->largeObjectAreaMinimumRatio;
		_extensions->heap->getResizeStats()->setLastLoaResizeReason(LOA_EXPAND_HEAP_ALIGNMENT);
	}

	if (debug && newLOARatio != _currentLOARatio) {
		omrtty_printf("LOA Calculate target ratio: ratio has %s from  %.3f --> %.3f\n",
				newLOARatio < _currentLOARatio ? "decreased" : "increased",
						_currentLOARatio,
						newLOARatio);
	}

	return newLOARatio;
}


/**
 * Reset the LOA ratio, and size to minimum size.
 */
double
MM_MemoryPoolLargeObjects::resetTargetLOARatio(MM_EnvironmentBase* env)
{
	bool debug = _extensions->debugLOAResize;

	/* Nothing needs to be done if the LOA size is already at minimum size */
	if (_currentLOARatio == _extensions->largeObjectAreaMinimumRatio) {
		return _currentLOARatio;
	}

	if (debug) {
		OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
		omrtty_printf("LOA Reset target ratio: ratio reset from %.3f to minimum size of %.3f\n",
					 _currentLOARatio, _extensions->largeObjectAreaMinimumRatio);
	}

	_extensions->heap->getResizeStats()->setLastLoaResizeReason(LOA_CONTRACT_AGGRESSIVE);

	return _extensions->largeObjectAreaMinimumRatio;
}

void
MM_MemoryPoolLargeObjects::resetLOASize(MM_EnvironmentBase* env, double newLOARatio)
{
	uintptr_t oldLOASize = _loaSize;
	uintptr_t newLOASize, oldAreaSize;
	bool debug = _extensions->debugLOAResize;
	HeapResizeType resizeType = HEAP_NO_RESIZE;

	/* Has LOA changed in size ? */
	if (_currentLOARatio != newLOARatio) {

		/* Get total size of owning subspace */
		oldAreaSize = _memorySubSpace->getActiveMemorySize();

		/* Calculate LOA size based on new loa ratio */
		newLOASize = MM_Math::roundToFloor(_extensions->heapAlignment, (uintptr_t)(oldAreaSize * newLOARatio));

		uintptr_t resizeSize = 0;
		/* Does this leave a reasonable sized LOA ? */
		if (newLOASize < _extensions->largeObjectMinimumSize) {
			/* No.. make LOA empty as not even big enough for one free chunk */
			_currentLOARatio = 0;
			_soaSize = oldAreaSize;
			_loaSize = 0;
		} else {
			_currentLOARatio = newLOARatio;
			_loaSize = newLOASize;
			/* ..and SOA is whats left after LOA allocation */
			_soaSize = oldAreaSize - _loaSize;
		}

		/* Rememeber if we expanded or contracted the LOA */
		if ( _loaSize > oldLOASize) {
			resizeType = HEAP_LOA_EXPAND;
			resizeSize = newLOASize - oldLOASize;
		} else if (_loaSize < oldLOASize) {
			resizeType = HEAP_LOA_CONTRACT;
			resizeSize = oldLOASize - newLOASize;
		}
		/* else, could be newLOASize == oldLOASize  (originally expand, but not big enough) */

		/* and new LOA base if LOA ratio > 0 */
		_currentLOABase = _currentLOARatio > 0 ? determineLOABase(env, _soaSize) : LOA_EMPTY;

		if (debug) {
			OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
			omrtty_printf("Reset LOA Size: LOA Base is now %p\n", _currentLOABase);
		}

		_memorySubSpace->reportHeapResizeAttempt(env, resizeSize , resizeType);
	}
}

void
MM_MemoryPoolLargeObjects::resetHeapStatistics(bool memoryPoolCollected)
{
	/* Reset heap statistics for LOA an SOA */
	_memoryPoolSmallObjects->resetHeapStatistics(memoryPoolCollected);
	_memoryPoolLargeObjects->resetHeapStatistics(memoryPoolCollected);
}

void
MM_MemoryPoolLargeObjects::mergeHeapStats(MM_HeapStats* heapStats, bool active)
{
	/* Add in statistics for both LOA and SOA */
	_memoryPoolSmallObjects->mergeHeapStats(heapStats, active);
	_memoryPoolLargeObjects->mergeHeapStats(heapStats, active);
}

void*
MM_MemoryPoolLargeObjects::allocateObject(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	bool debug = _extensions->debugLOAAllocate;

	void* addr = NULL;
	uintptr_t sizeInBytesRequired = allocDescription->getContiguousBytes();

	/* First we try to allocate ALL objects in the SOA, even large ones
	 * provided we have not already had a AF for a smaller object this
	 * cycle.
	 */
	if (sizeInBytesRequired < _soaObjectSizeLWM) {
		addr = _memoryPoolSmallObjects->allocateObject(env, allocDescription);
	}

	if (NULL == addr) {
		_soaObjectSizeLWM = OMR_MIN(_soaObjectSizeLWM, sizeInBytesRequired);

		if (sizeInBytesRequired >= _extensions->largeObjectMinimumSize) {

			/* Retry allocation in LOA ..if we have one */
			if (_loaSize > 0) {
				addr = _memoryPoolLargeObjects->allocateObject(env, allocDescription);

				if (addr != NULL) {
					allocDescription->setLOAAllocation(true);
					if (debug) {
						omrtty_printf("LOA allocate: object allocated at %p of size %zu bytes. SOA LWM is %zu bytes\n",
									 addr, sizeInBytesRequired, _soaObjectSizeLWM);
					}
				}
			}
		}
	}

	return addr;
}

void*
MM_MemoryPoolLargeObjects::allocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription,
									   uintptr_t maximumSizeInBytesRequired, void*& addrBase, void*& addrTop)
{
	return _memoryPoolSmallObjects->allocateTLH(env, allocDescription, maximumSizeInBytesRequired, addrBase, addrTop);
}

/**
 * Find the free list entry whos end address matches the parameter.
 *
 * @param addr Address to match against the high end of a free entry.
 *
 * @return The leading address of the free entry whos top matches addr.
 */
void*
MM_MemoryPoolLargeObjects::findFreeEntryEndingAtAddr(MM_EnvironmentBase* env, void* addr)
{
	if (addr >= _currentLOABase) {
		return _memoryPoolLargeObjects->findFreeEntryEndingAtAddr(env, addr);
	} else {
		return _memoryPoolSmallObjects->findFreeEntryEndingAtAddr(env, addr);
	}
}

/**
 * @copydoc MM_MemoryPool::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase *, MM_AllocateDescription *, void *, void *)
 */
uintptr_t
MM_MemoryPoolLargeObjects::getAvailableContractionSizeForRangeEndingAt(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription,
																	   void* lowAddr, void* highAddr)
{
	if (highAddr >= _currentLOABase) {
		return _memoryPoolLargeObjects->getAvailableContractionSizeForRangeEndingAt(env, allocDescription, lowAddr, highAddr);
	} else {
		return _memoryPoolSmallObjects->getAvailableContractionSizeForRangeEndingAt(env, allocDescription, lowAddr, highAddr);
	}
}

/**
 * Return the memory pool associated with a given storage location.
 * @param Address of storage location
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemoryPoolLargeObjects::getMemoryPool(void* addr)
{
	if (addr >= _currentLOABase) {
		return _memoryPoolLargeObjects;
	} else {
		return _memoryPoolSmallObjects;
	}
}

/**
 * Return the memory pool associated with a given allocation request.
 * @param Size of allocation request
 * @return MM_MemoryPool
 */
MM_MemoryPool*
MM_MemoryPoolLargeObjects::getMemoryPool(uintptr_t size)
{
	if (size >= _extensions->largeObjectMinimumSize) {
		return _memoryPoolLargeObjects;
	} else {
		return _memoryPoolSmallObjects;
	}
}

/**
 * Return the memory pool associated with a specified range of storage locations.
 *
 * @param addrBase Low address in specified range
 * @param addrTop High address in  specified range
 * @param highAddr If range spans end of memory pool set to address of first byte
 * which does not belong in returned pool.
 * @return MM_MemoryPool for storage location addrBase
 */
MM_MemoryPool*
MM_MemoryPoolLargeObjects::getMemoryPool(MM_EnvironmentBase* env, void* addrBase, void* addrTop, void*& highAddr)
{
	if (addrBase < _currentLOABase && addrTop <= _currentLOABase) {
		/* chunk wholly contained in SOA */
		highAddr = NULL;
		return _memoryPoolSmallObjects;
	} else if (addrBase < _currentLOABase && addrTop > _currentLOABase) {
		/* Range spans SOA/LOA boundary so split into 2 */
		highAddr = _currentLOABase;
		return _memoryPoolSmallObjects;
	} else {
		/* chunk wholly contained in LOA */
		assume0(addrBase >= _currentLOABase);
		highAddr = NULL;
		return _memoryPoolLargeObjects;
	}
}

/**
 * Get the sum of all free memory currently available for allocation in the receiver.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider defered work that may be done to increase current free memory stores.
 * @see getApproximateFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_MemoryPoolLargeObjects::getActualFreeMemorySize()
{
	uintptr_t LOASize = _memoryPoolLargeObjects->getActualFreeMemorySize();
	uintptr_t SOASize = _memoryPoolSmallObjects->getActualFreeMemorySize();

	return LOASize + SOASize;
}

uintptr_t
MM_MemoryPoolLargeObjects::getActualFreeEntryCount()
{
	uintptr_t LOACount = _memoryPoolLargeObjects->getActualFreeEntryCount();
	uintptr_t SOACount = _memoryPoolSmallObjects->getActualFreeEntryCount();

	return LOACount + SOACount;
}

/**
 * Get the approximate sum of all free memory available for allocation in the receiver.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * @see getActualFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_MemoryPoolLargeObjects::getApproximateFreeMemorySize()
{
	uintptr_t LOASize = _memoryPoolLargeObjects->getApproximateFreeMemorySize();
	uintptr_t SOASize = _memoryPoolSmallObjects->getApproximateFreeMemorySize();

	return LOASize + SOASize;
}


/**
 * Reset largest free entry in this memory pool
 *
 */
void
MM_MemoryPoolLargeObjects::resetLargestFreeEntry()
{
	_memoryPoolLargeObjects->resetLargestFreeEntry();
	_memoryPoolSmallObjects->resetLargestFreeEntry();
}

/**
 * Return the largest free entry in this memory pool
 * @return Size of largest free entry
 */
uintptr_t
MM_MemoryPoolLargeObjects::getLargestFreeEntry()
{

	uintptr_t soaLargest = _memoryPoolSmallObjects->getLargestFreeEntry();
	uintptr_t loaLargest = _memoryPoolLargeObjects->getLargestFreeEntry();

	return OMR_MAX(soaLargest, loaLargest);
}

/**
 * Find the top of the free list entry whos start address matches the parameter.
 *
 * @param addr Address to match against the low end of a free entry.
 *
 * @return The trailing address of the free entry whos top matches addr.
 */
void*
MM_MemoryPoolLargeObjects::findFreeEntryTopStartingAtAddr(MM_EnvironmentBase* env, void* addr)
{
	if (addr >= _currentLOABase) {
		return _memoryPoolLargeObjects->findFreeEntryTopStartingAtAddr(env, addr);
	} else {
		return _memoryPoolSmallObjects->findFreeEntryTopStartingAtAddr(env, addr);
	}
}

/**
 * Find the address of the first entry on free list entry
 *
 *
 * @return The address of head of free chain
 */
void*
MM_MemoryPoolLargeObjects::getFirstFreeStartingAddr(MM_EnvironmentBase* env)
{
	void* firstFree = _memoryPoolSmallObjects->getFirstFreeStartingAddr(env);

	if (NULL != firstFree) {
		return firstFree;
	} else {
		/* Nothing free in SOA.. so check LOA*/
		return _memoryPoolLargeObjects->getFirstFreeStartingAddr(env);
	}
}

/**
 * Find the address of the first entry on free list entry
 *
 *
 * @return The address of next free entry or NULL
 */
void*
MM_MemoryPoolLargeObjects::getNextFreeStartingAddr(MM_EnvironmentBase* env, void* currentFree)
{
	void* nextFree;

	assume0(currentFree != NULL);

	if (currentFree < _currentLOABase) {
		nextFree = _memoryPoolSmallObjects->getNextFreeStartingAddr(env, currentFree);

		if (NULL == nextFree) {
			nextFree = _memoryPoolLargeObjects->getFirstFreeStartingAddr(env);
		}
	} else {
		nextFree = _memoryPoolLargeObjects->getNextFreeStartingAddr(env, currentFree);
	}

	return nextFree;
}

/**
 * Move a chunk of heap from one location to another within the receivers owned regions.
 * This involves fixing up any free list information that may change as a result of an
 * address change.
 *
 * This method should not be called; only required if we implement a LOA for semi-spaces
 *
 * @param srcBase Start address to move.
 * @param srcTop End address to move.
 * @param dstBase Start of destination address to move into.
 *
 */
void
MM_MemoryPoolLargeObjects::moveHeap(MM_EnvironmentBase* env, void* srcBase, void* srcTop, void* dstBase)
{
	assume(false, "MM_MemoryPoolLargeObjects::moveHeap not supported");
}

/**
 * Lock any free list information from use.
 */
void
MM_MemoryPoolLargeObjects::lock(MM_EnvironmentBase* env)
{
	_memoryPoolSmallObjects->lock(env);
	_memoryPoolLargeObjects->lock(env);
}

/**
 * Unlock any free list information for use.
 */
void
MM_MemoryPoolLargeObjects::unlock(MM_EnvironmentBase* env)
{
	_memoryPoolLargeObjects->unlock(env);
	_memoryPoolSmallObjects->unlock(env);
}

/**
 * @todo Provide function documentation
 */
void*
MM_MemoryPoolLargeObjects::collectorAllocate(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, bool lockingRequired)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	bool debug = _extensions->debugLOAAllocate;

	void* addr = NULL;
	uintptr_t sizeInBytesRequired = allocDescription->getContiguousBytes();

	/* First we try to allocate ALL objects in the SOA, even large ones
	 * provided we have not already had a AF for a smaller object this
	 * cycle.
	 */
	if (sizeInBytesRequired < _soaObjectSizeLWM) {
		addr = _memoryPoolSmallObjects->collectorAllocate(env, allocDescription, lockingRequired);
	}

	if (NULL == addr) {
		_soaObjectSizeLWM = OMR_MIN(_soaObjectSizeLWM, sizeInBytesRequired);

		/* We relax normal rule and allow small objects to be allocated in LOA if caller requests */
		if (allocDescription->isCollectorAllocateSatisfyAnywhere() || sizeInBytesRequired >= _extensions->largeObjectMinimumSize) {

			/* Retry allocation in LOA ..if we have one */
			if (_loaSize > 0) {
				addr = _memoryPoolLargeObjects->collectorAllocate(env, allocDescription, lockingRequired);

				if (NULL != addr) {
					allocDescription->setLOAAllocation(true);

					if (debug) {
						omrtty_printf("LOA allocate(collector): normal object allocated at %p of size %zu bytes. SOA LWM is %zu bytes\n",
									 addr, sizeInBytesRequired, _soaObjectSizeLWM);
					}
				}
			}
		}
	}

	return addr;
}

/**
 * @todo Provide function documentation
 */
void*
MM_MemoryPoolLargeObjects::collectorAllocateTLH(MM_EnvironmentBase* env, MM_AllocateDescription* allocDescription, uintptr_t maximumSizeInBytesRequired,
												void*& addrBase, void*& addrTop, bool lockingRequired)
{
	void* base = _memoryPoolSmallObjects->collectorAllocateTLH(env, allocDescription, maximumSizeInBytesRequired, addrBase, addrTop, lockingRequired);

	/* We relax normal rule and allow TLH to be allocated in LOA if caller allows */
	if ((NULL == base) && allocDescription->isCollectorAllocateSatisfyAnywhere()) {
		base = _memoryPoolLargeObjects->collectorAllocateTLH(env, allocDescription, maximumSizeInBytesRequired, addrBase, addrTop, lockingRequired);
	}

	return base;
}

/**
 * Add the range of memory to the appropriate free list.
 *
 * The heap has expanded so we need to recalculate the LOA boundary and update the SOA and LOA
 * freelists appropriately.
 *
 * @param expandSize Number of bytes to remove from the memory pool
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 *
 */


void
MM_MemoryPoolLargeObjects::expandWithRange(MM_EnvironmentBase* env, uintptr_t expandSize, void* lowAddress, void* highAddress, bool canCoalesce)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	bool debug = _extensions->debugLOAResize;

	uintptr_t oldAreaSize, newLOASize;

	assume0(MM_Math::roundToCeiling(_extensions->heapAlignment, expandSize) == expandSize);

	/* Get total size of owning subspace.. this will be post expand size */
	oldAreaSize = _memorySubSpace->getActiveMemorySize();

	/* Is this the initial expand ? */
	if (0 == _currentOldAreaSize) {

		/* Work out initial SOA to LOA split */
		newLOASize = (uintptr_t)(oldAreaSize * _currentLOARatio);
		_loaSize = MM_Math::roundToFloor(_extensions->heapAlignment, newLOASize);

		/* SOA is what is left after LOA allocation */
		_soaSize = oldAreaSize - _loaSize;

		_currentLOABase = _loaSize > 0 ? determineLOABase(env, _soaSize) : LOA_EMPTY;

		_memoryPoolSmallObjects->expandWithRange(env, _soaSize, lowAddress, _currentLOABase, canCoalesce);

		if (_loaSize > 0) {
			_memoryPoolLargeObjects->expandWithRange(env, _loaSize, _currentLOABase, highAddress, canCoalesce);
		}

		if (debug) {
			omrtty_printf("LOA Initial Allocation: heapSize %zu, Initial LOA ratio is %.3f --> LOA base is %p initial LOA size %zu\n",
						 oldAreaSize, _currentLOARatio, _currentLOABase, _loaSize);
		}

	} else {

		/* If LOA ratio has reduced to zero then we have an empty
		 * LOA at the moment and all new storage goes to SOA
		 */
		if (0 == _currentLOARatio) {
			_memoryPoolSmallObjects->expandWithRange(env, expandSize, lowAddress, highAddress, canCoalesce);

			_currentLOABase = LOA_EMPTY;
			_loaSize = 0;
			_soaSize = oldAreaSize;

		} else {
			/* First add new storage to LOA */
			_memoryPoolLargeObjects->expandWithRange(env, expandSize, lowAddress, highAddress, canCoalesce);

			/* ..and then redistribute free memory between SOA and LOA */
			redistributeFreeMemory(env, oldAreaSize);

			if (debug) {
				omrtty_printf("LOA resized on heap expansion: heapSize %zu,  LOA ratio is %.3f --> LOA base is now %p LOA size %zu\n",
							 oldAreaSize, _currentLOARatio, _currentLOABase, _loaSize);
			}
		}

		/* Reset SOA LWM.
		 *
		 * N.B We have to reset here as well as in reset() as heap can
		 * expand outside a global GC, e.g when scaveneger expands
		 * tenure space to continue collection.
		 */
		_soaObjectSizeLWM = ((uintptr_t) - 1);
	}

	_currentOldAreaSize = oldAreaSize;
}

/**
 * Remove the range of memory from the appropriate free list.
 *
 * The heap has contracted so we need to recalculate the LOA boundary and update the SOA and LOA
 * freelists appropriately.
 *
 * @param expandSize Number of bytes to remove from the memory pool
 * @param lowAddress Low address of memory to remove (inclusive)
 * @param highAddress High address of memory to remove (non inclusive)
 *
 */

void*
MM_MemoryPoolLargeObjects::contractWithRange(MM_EnvironmentBase* env, uintptr_t contractSize, void* lowAddress, void* highAddress)
{
	OMRPORT_ACCESS_FROM_ENVIRONMENT(env);
	bool debug = _extensions->debugLOAResize;

	/* Get current size of old area */
	uintptr_t oldAreaSize = _memorySubSpace->getActiveMemorySize();

	/* Get the new old area size..cant ask MSS as size not updated yet */
	oldAreaSize -= contractSize;
	assume0((_currentOldAreaSize - contractSize) == oldAreaSize);

	/* First remove the memory from the appropraiate pool */
	if (0 == _currentLOARatio) {
		assume0((0 == _loaSize) && (LOA_EMPTY == _currentLOABase));

		/* No LOA so just remove memory from SOA and we are done */
		_memoryPoolSmallObjects->contractWithRange(env, contractSize, lowAddress, highAddress);
	} else {

		assume0(lowAddress >= _currentLOABase);

		/* First remove memory from LOA */
		_memoryPoolLargeObjects->contractWithRange(env, contractSize, lowAddress, highAddress);

		redistributeFreeMemory(env, oldAreaSize);

		if (debug) {
			omrtty_printf("LOA resized on heap contraction: heapSize %zu,  LOA ratio is %.3f --> LOA base is now %p LOA size %zu\n",
						 oldAreaSize, _currentLOARatio, _currentLOABase, _loaSize);
		}
	}

	/* ..and remmeber new old area size for next time */
	_currentOldAreaSize = oldAreaSize;

	return lowAddress;
}

/**
 * Redistribute free memory bewteen the LOA and SOA after a heap expansion/contraction.
 *
 * @param newOldAreaSize Number of bytes in old area AFTER expansion/contraction
 *
 */
void
MM_MemoryPoolLargeObjects::redistributeFreeMemory(MM_EnvironmentBase* env, uintptr_t newOldAreaSize)
{
	uintptr_t count, size;
	MM_HeapLinkedFreeHeader* freeListHead = NULL;
	MM_HeapLinkedFreeHeader* freeListTail = NULL;
	void* oldLOABase = _currentLOABase;

	/* Calculate new LOA size based on current LOA ratio */
	_loaSize = MM_Math::roundToFloor(_extensions->heapAlignment, (uintptr_t)((float)newOldAreaSize * _currentLOARatio));

	assume0(_loaSize > 0);

	/* ..and SOA is what's left after LOA allocation */
	_soaSize = newOldAreaSize - _loaSize;
	assume0(MM_Math::roundToFloor(_extensions->heapAlignment, _soaSize) == _soaSize);

	/* .. and new LOA base */
	_currentLOABase = determineLOABase(env, _soaSize);

	if (oldLOABase < _currentLOABase) {
		/* Take chunks away from LOA and give to SOA */
		_memoryPoolLargeObjects->removeFreeEntriesWithinRange(env, oldLOABase, _currentLOABase,
															  _memoryPoolSmallObjects->getMinimumFreeEntrySize(),
															  freeListHead, freeListTail, count, size);

		if (NULL != freeListHead) {
			_memoryPoolSmallObjects->addFreeEntries(env, freeListHead, freeListTail, count, size);
		}

	} else if (oldLOABase > _currentLOABase) {
		/* Take chunks away from SOA and give to LOA */
		_memoryPoolSmallObjects->removeFreeEntriesWithinRange(env, _currentLOABase, oldLOABase,
															  _memoryPoolLargeObjects->getMinimumFreeEntrySize(),
															  freeListHead, freeListTail, count, size);

		if (NULL != freeListHead) {
			_memoryPoolLargeObjects->addFreeEntries(env, freeListHead, freeListTail, count, size);
		}
	}

	/* Verify all pools in valid state after we are done */
	assume0(_memoryPoolSmallObjects->isMemoryPoolValid(env, true));
	assume0(_memoryPoolLargeObjects->isMemoryPoolValid(env, true));
}

bool
MM_MemoryPoolLargeObjects::abandonHeapChunk(void* addrBase, void* addrTop)
{
	Assert_MM_true(addrTop >= addrBase);

	/* Direct request to correct subpool */
	if (addrBase < _currentLOABase) {
		return _memoryPoolSmallObjects->abandonHeapChunk(addrBase, addrTop);
	} else {
		return _memoryPoolLargeObjects->abandonHeapChunk(addrBase, addrTop);
	}
}

/*
 * Determine base address for LOA given size of SOA. Iterate over segments (which
 * could be non-contiguous) until we have accounted for size of SOA
 *
 *
 */
uintptr_t*
MM_MemoryPoolLargeObjects::determineLOABase(MM_EnvironmentBase* env, uintptr_t soaSize)
{
	uintptr_t sizeLeft = soaSize;

	/* SOA can't be bigger than size of subspace */
	Assert_MM_true(soaSize <= _memorySubSpace->getActiveMemorySize());

	MM_HeapRegionDescriptor* region;
	GC_MemorySubSpaceRegionIterator regionIterator((MM_MemorySubSpace*)_memorySubSpace);
	while ((region = regionIterator.nextRegion()) != NULL) {
		uintptr_t regionSize = region->getSize();
		if (sizeLeft >= regionSize) {
			sizeLeft -= regionSize;
		} else {
			uintptr_t* loaBase = (uintptr_t*)((uint8_t*)region->getLowAddress() + sizeLeft);
			loaBase = (uintptr_t*)MM_Math::roundToCeiling(_extensions->heapAlignment, (uintptr_t)loaBase);
			Assert_MM_true(loaBase <= (uintptr_t*)region->getHighAddress());
			return loaBase;
		}
	}

	/* Should never get here */
	Assert_MM_unreachable();

	return NULL;
}

void
MM_MemoryPoolLargeObjects::mergeFreeEntryAllocateStats()
{
	_largeObjectAllocateStats->getFreeEntrySizeClassStats()->resetCounts();
	_memoryPoolSmallObjects->mergeFreeEntryAllocateStats();
	_memoryPoolLargeObjects->mergeFreeEntryAllocateStats();
	_largeObjectAllocateStats->getFreeEntrySizeClassStats()->merge(_memoryPoolSmallObjects->getLargeObjectAllocateStats()->getFreeEntrySizeClassStats());
	_largeObjectAllocateStats->getFreeEntrySizeClassStats()->merge(_memoryPoolLargeObjects->getLargeObjectAllocateStats()->getFreeEntrySizeClassStats());
}

void
MM_MemoryPoolLargeObjects::mergeTlhAllocateStats()
{
	_largeObjectAllocateStats->getTlhAllocSizeClassStats()->resetCounts();
	_memoryPoolSmallObjects->mergeTlhAllocateStats();
	_memoryPoolLargeObjects->mergeTlhAllocateStats();
	_largeObjectAllocateStats->getTlhAllocSizeClassStats()->merge(_memoryPoolSmallObjects->getLargeObjectAllocateStats()->getTlhAllocSizeClassStats());
	_largeObjectAllocateStats->getTlhAllocSizeClassStats()->merge(_memoryPoolLargeObjects->getLargeObjectAllocateStats()->getTlhAllocSizeClassStats());
}

void
MM_MemoryPoolLargeObjects::mergeLargeObjectAllocateStats()
{
	_largeObjectAllocateStats->resetCurrent();
	_memoryPoolSmallObjects->mergeLargeObjectAllocateStats();
	_memoryPoolLargeObjects->mergeLargeObjectAllocateStats();
	_largeObjectAllocateStats->mergeCurrent(_memoryPoolSmallObjects->getLargeObjectAllocateStats());
	_largeObjectAllocateStats->mergeCurrent(_memoryPoolLargeObjects->getLargeObjectAllocateStats());
}

void
MM_MemoryPoolLargeObjects::averageLargeObjectAllocateStats(MM_EnvironmentBase* env, uintptr_t bytesAllocatedThisRound)
{
	_largeObjectAllocateStats->resetAverage();
	_memoryPoolSmallObjects->averageLargeObjectAllocateStats(env, bytesAllocatedThisRound);
	_memoryPoolLargeObjects->averageLargeObjectAllocateStats(env, bytesAllocatedThisRound);
	_largeObjectAllocateStats->mergeAverage(_memoryPoolSmallObjects->getLargeObjectAllocateStats());
	_largeObjectAllocateStats->mergeAverage(_memoryPoolLargeObjects->getLargeObjectAllocateStats());
}

void
MM_MemoryPoolLargeObjects::resetLargeObjectAllocateStats()
{
	_largeObjectAllocateStats->resetCurrent();
	_largeObjectAllocateStats->getTlhAllocSizeClassStats()->resetCounts();
	_memoryPoolSmallObjects->resetLargeObjectAllocateStats();
	_memoryPoolLargeObjects->resetLargeObjectAllocateStats();
}

#if defined(OMR_GC_IDLE_HEAP_MANAGER)
uintptr_t
MM_MemoryPoolLargeObjects::releaseFreeMemoryPages(MM_EnvironmentBase* env)
{
	uintptr_t releasedMemory = _memoryPoolSmallObjects->releaseFreeMemoryPages(env);
	releasedMemory += _memoryPoolLargeObjects->releaseFreeMemoryPages(env);
	return releasedMemory;
}
#endif
