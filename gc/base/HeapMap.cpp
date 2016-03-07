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

#include "HeapMap.hpp"

#include <string.h>

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "Math.hpp"
#include "MemoryManager.hpp"
#include "ModronAssertions.h"

/**
 * Object creation and destruction 
 *
 */
void
MM_HeapMap::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_HeapMap::initialize(MM_EnvironmentBase *env)
{
	bool result = false;

#if !defined(OMR_GC_SEGREGATED_HEAP)
	if (_useCompressedHeapMap) {
		return false;
	}
#endif /* OMR_GC_SEGREGATED_HEAP */

	uintptr_t heapMapSizeRequired = getMaximumHeapMapSize(env);
	
	MM_MemoryManager *memoryManager = _extensions->memoryManager;
	if (memoryManager->createVirtualMemoryForMetadata(env, &_heapMapMemoryHandle, _extensions->heapAlignment, heapMapSizeRequired)) {
		_heapMapBits = (uintptr_t *)memoryManager->getHeapBase(&_heapMapMemoryHandle);
		_heapBase = _extensions->heap->getHeapBase();
		_heapMapBaseDelta = (uintptr_t)_heapBase;
		result = true;
	}
	return result;
}

void
MM_HeapMap::tearDown(MM_EnvironmentBase *env)
{
	MM_MemoryManager *memoryManager = _extensions->memoryManager;
	memoryManager->destroyVirtualMemory(env, &_heapMapMemoryHandle);
	
	_heapMapBits = NULL;
}

/**
 * Heap Map re-sizing
 *
 */
uintptr_t
MM_HeapMap::convertHeapIndexToHeapMapIndex(MM_EnvironmentBase *env, uintptr_t offset, uintptr_t roundTo)
{
	if (!_useCompressedHeapMap) {
		return MM_Math::roundToCeiling(roundTo, MM_Math::roundToCeiling(J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT, offset) / J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT) ;
	} else {
#if defined(OMR_GC_SEGREGATED_HEAP)
		return MM_Math::roundToCeiling(roundTo, (offset >> OMR_SIZECLASSES_LOG_SMALLEST) / 8);
#else
		Assert_MM_unreachable();
		return 0;
#endif /* OMR_GC_SEGREGATED_HEAP */
	}

}

uintptr_t
MM_HeapMap::getMaximumHeapMapSize(MM_EnvironmentBase *env)
{ 
		MM_GCExtensionsBase *extensions = env->getExtensions();
		return convertHeapIndexToHeapMapIndex(env, _maxHeapSize, extensions->heapAlignment);
}	

/**
 * Adjust internal structures to reflect the change in heap size.
 * 
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @return true if succeed
 */
bool
MM_HeapMap::heapAddRange(MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress)
{
	bool commited = true;

	if (_extensions->isFvtestForceMarkMapCommitFailure()) {
		commited = false;
		Trc_MM_HeapMap_markMapCommitFailureForced(env->getLanguageVMThread());
	} else {
		/* Record the range in which valid objects appear */
		_heapBase = _extensions->heap->getHeapBase();
		_heapTop = _extensions->heap->getHeapTop();
		Assert_MM_true(_heapMapBaseDelta == (uintptr_t) _heapBase);

		/* Commit the mark map memory that is associated with the heap range added */
		/* NOTE: We calculate the low and high heap offsets, and build the mark map index and size values
		 * from these to avoid rounding errors (if we use the size, the conversion routine could get a different
		 * rounding result then the actual end address)
		 */

		uintptr_t heapOffsetLow = _extensions->heap->calculateOffsetFromHeapBase(lowAddress);
		uintptr_t heapOffsetHigh = _extensions->heap->calculateOffsetFromHeapBase(highAddress);
		uintptr_t heapMapCommitOffset = convertHeapIndexToHeapMapIndex(env, heapOffsetLow, sizeof(uintptr_t));
		uintptr_t heapMapCommitSize = convertHeapIndexToHeapMapIndex(env, heapOffsetHigh, sizeof(uintptr_t)) - heapMapCommitOffset;
	
		MM_MemoryManager *memoryManager = _extensions->memoryManager;
		commited = memoryManager->commitMemory(&_heapMapMemoryHandle, (void *)(((uintptr_t)_heapMapBits) + heapMapCommitOffset), heapMapCommitSize);
		if (!commited) {
			Trc_MM_HeapMap_markMapCommitFailed(env->getLanguageVMThread(), (void *)(((uintptr_t)_heapMapBits) + heapMapCommitOffset), heapMapCommitSize);
		}
	}
	return commited;
}

/**
 * Adjust internal structures to reflect the change in heap size.
 * 
 * @param size The amount of memory added to the heap
 * @param lowAddress The base address of the memory added to the heap
 * @param highAddress The top address (non-inclusive) of the memory added to the heap
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 * @return true if succeed
 */
bool
MM_HeapMap::heapRemoveRange(MM_EnvironmentBase *env, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddress, void *highValidAddress)
{
	bool decommited = true;

	if (_extensions->isFvtestForceMarkMapDecommitFailure()) {
		decommited = false;
		Trc_MM_HeapMap_markMapDecommitFailureForced(env->getLanguageVMThread());
	} else {
		/* Record the range in which valid objects appear */
		_heapBase = _extensions->heap->getHeapBase();
		_heapTop = _extensions->heap->getHeapTop();
		Assert_MM_true(_heapMapBaseDelta == (uintptr_t) _heapBase);

		/* Commit the mark map memory that is associated with the heap range added */

		/* NOTE: We calculate the low and high heap offsets, and build the mark map index and size values
		 * from these to avoid rounding errors (if we use the size, the conversion routine could get a different
		 * rounding result then the actual end address)
		 */
		uintptr_t heapOffsetLow = _extensions->heap->calculateOffsetFromHeapBase(lowAddress);
		uintptr_t heapOffsetHigh = _extensions->heap->calculateOffsetFromHeapBase(highAddress);
		uintptr_t heapMapCommitOffset = convertHeapIndexToHeapMapIndex(env, heapOffsetLow, sizeof(uintptr_t));
		uintptr_t heapMapCommitEnd = convertHeapIndexToHeapMapIndex(env, heapOffsetHigh, sizeof(uintptr_t));
		uintptr_t heapMapCommitSize = heapMapCommitEnd - heapMapCommitOffset;

		void *validHeapMapCommitLow = NULL;
		if(NULL != lowValidAddress) {
			validHeapMapCommitLow = (void *) (((uintptr_t)_heapMapBits) + heapMapCommitOffset);
		}

		void *validHeapMapCommitHigh = NULL;
		if(NULL != highValidAddress) {
			validHeapMapCommitHigh = (void *) (((uintptr_t)_heapMapBits) + heapMapCommitEnd);
		}

		MM_MemoryManager *memoryManager = _extensions->memoryManager;
		decommited = memoryManager->decommitMemory(&_heapMapMemoryHandle,
										(void *)(((uintptr_t)_heapMapBits) + heapMapCommitOffset),
										heapMapCommitSize,
										validHeapMapCommitLow,
										validHeapMapCommitHigh);
		if (!decommited) {
			Trc_MM_HeapMap_markMapDecommitFailed(env->getLanguageVMThread(), (void *)(((uintptr_t)_heapMapBits) + heapMapCommitOffset), heapMapCommitSize,
				validHeapMapCommitLow, validHeapMapCommitHigh);
		}
	}
	return decommited;
}

/**
 * Number of bits in specified range 
 *
 * @param lowAddress - base of region of heap whose heap map bits are to be counted
 * @param highAddress - top of region of heap whose heap map bits to be counted
 * @return the number of bytes worth of bits for specified heap range 
 */
uintptr_t 
MM_HeapMap::numberBitsInRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress)
{
	uintptr_t baseIndex, topIndex;
	
	/* Validate passed heap references */
	Assert_MM_true(lowAddress < highAddress);
	Assert_MM_true((uintptr_t)lowAddress == MM_Math::roundToCeiling(_extensions->heapAlignment,(uintptr_t)lowAddress));
	
	/* Find mark index for base ptr */
	baseIndex = ((uintptr_t)lowAddress) - _heapMapBaseDelta;
	baseIndex >>= _heapMapIndexShift;
		
	/* ..and for top ptr */
	topIndex = ((uintptr_t)highAddress) - _heapMapBaseDelta;
	topIndex >>= _heapMapIndexShift;
	
	return  ((topIndex - baseIndex) * sizeof(uintptr_t));
		
}

/**
 * Set all heap map bits for a specified heap range either ON or OFF
 * 				  
 * @param lowAddress - base of region of heap whoose heap map bits are to be set 
 * @param highAddress - top of region of heap whoose heap map bits to be set
 * @param clear - if TRUE set BITS OFF; otherwise set bits ON 		
 * @return the amount of memory actually cleard
 */
uintptr_t 
MM_HeapMap::setBitsInRange(MM_EnvironmentBase *env, void *lowAddress, void *highAddress, bool clear)
{
	uintptr_t baseIndex, topIndex;
	uintptr_t bytesToSet;
	
	/* Validate passed heap references */
	Assert_MM_true(lowAddress < _heapTop);
	Assert_MM_true(lowAddress >= _heapBase);
	Assert_MM_true((uintptr_t)lowAddress == MM_Math::roundToCeiling(_extensions->heapAlignment,(uintptr_t)lowAddress));
	Assert_MM_true(highAddress  <= _heapTop);
		
	/* Find mark index for base ptr. Be careful to calculate offset relative to
	 * absolute heapBase not active heapBase. Hence we can't use _heapBaseDelta here
	 */
	baseIndex = _extensions->heap->calculateOffsetFromHeapBase(lowAddress);
	baseIndex >>= _heapMapIndexShift;
		
	/* ..and for top ptr */
	topIndex = _extensions->heap->calculateOffsetFromHeapBase(highAddress);
	topIndex >>= _heapMapIndexShift;
	
	bytesToSet= (topIndex - baseIndex) * sizeof(uintptr_t);
		
	if (clear) {
		OMRZeroMemory((void *)&(_heapMapBits[baseIndex]), bytesToSet);
	} else {
		memset(&(_heapMapBits[baseIndex]), 0xFF, bytesToSet);
	}
		
	return bytesToSet;
}

uintptr_t
MM_HeapMap::setBitsForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region, bool clear)
{
	return setBitsInRange(env, region->getLowAddress(), region->getHighAddress(), clear);
}

bool
MM_HeapMap::checkBitsForRegion(MM_EnvironmentBase *env, MM_HeapRegionDescriptor *region)
{
	uintptr_t baseIndex, topIndex;
	uintptr_t bytesToCheck;

	void *lowAddress = region->getLowAddress();
	void *highAddress = region->getHighAddress();

	/* Validate passed heap references */
	Assert_MM_true(lowAddress < _heapTop);
	Assert_MM_true(lowAddress >= _heapBase);
	Assert_MM_true((uintptr_t)lowAddress == MM_Math::roundToCeiling(_extensions->heapAlignment,(uintptr_t)lowAddress));
	Assert_MM_true(highAddress  <= _heapTop);

	/* Find mark index for base ptr. Be careful to calculate offset relative to
	 * absolute heapBase not active heapBase. Hence we can't use _heapBaseDelta here
	 */
	baseIndex = _extensions->heap->calculateOffsetFromHeapBase(lowAddress);
	baseIndex >>= _heapMapIndexShift;

	/* ..and for top ptr */
	topIndex = _extensions->heap->calculateOffsetFromHeapBase(highAddress);
	topIndex >>= _heapMapIndexShift;

	bytesToCheck= (topIndex - baseIndex) * sizeof(uintptr_t);

	uint8_t *markMapBytes = (uint8_t *)(&_heapMapBits[baseIndex]);
	for (uintptr_t i = 0; i < bytesToCheck; i++) {
		if (0 != markMapBytes[i]) {
			return false;
		}
	}

	return true;
}
