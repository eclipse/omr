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


#include "ReferenceChainWalkerMarkMap.hpp"

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "MemoryManager.hpp"

/**
 * Object creation and destruction
 */
MM_ReferenceChainWalkerMarkMap *
MM_ReferenceChainWalkerMarkMap::newInstance(MM_EnvironmentBase *env, uintptr_t maxHeapSize)
{
	MM_ReferenceChainWalkerMarkMap *markMap = (MM_ReferenceChainWalkerMarkMap *)env->getForge()->allocate(sizeof(MM_ReferenceChainWalkerMarkMap), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != markMap) {
		new(markMap) MM_ReferenceChainWalkerMarkMap(env, maxHeapSize);
		if (!markMap->initialize(env)) {
			markMap->kill(env);
			markMap = NULL;
		}
	}

	return markMap;
}

bool
MM_ReferenceChainWalkerMarkMap::initialize(MM_EnvironmentBase *env)
{
	bool result = MM_HeapMap::initialize(env);
	if (result) {
		/* commit mark map memory for each region and clear it */
		result = clearMapForRegions(env, true);
	}
	return result;
}

void
MM_ReferenceChainWalkerMarkMap::clearMap(MM_EnvironmentBase *env)
{
	clearMapForRegions(env, false);
}

bool
MM_ReferenceChainWalkerMarkMap::clearMapForRegions(MM_EnvironmentBase *env, bool commit)
{
	/* Walk all object segments to determine what ranges of the mark map should be cleared */
	bool result = true;
	MM_HeapRegionDescriptor *region;
	MM_Heap *heap = _extensions->getHeap();
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	MM_MemoryManager *memoryManager = _extensions->memoryManager;
	GC_HeapRegionIterator regionIterator(regionManager, true, true);
	while(NULL != (region = regionIterator.nextRegion())) {
		if (region->isCommitted()) {
			/* Walk the segment in chunks the size of the heapClearUnit size, checking if the corresponding mark map
			 * range should  be cleared.
			 */
			uint8_t* heapClearAddress = (uint8_t*)region->getLowAddress();
			uintptr_t heapClearSize = region->getSize();

			/* Convert the heap address/size to its corresponding mark map address/size */
			/* NOTE: We calculate the low and high heap offsets, and build the mark map index and size values
			 * from these to avoid rounding errors (if we use the size, the conversion routine could get a different
			 * rounding result then the actual end address)
			 */
			uintptr_t heapClearOffset = ((uintptr_t)heapClearAddress) - _heapMapBaseDelta;
			uintptr_t heapMapClearIndex = convertHeapIndexToHeapMapIndex(env, heapClearOffset, sizeof(uintptr_t));
			uintptr_t heapMapClearSize = convertHeapIndexToHeapMapIndex(env, heapClearOffset + heapClearSize, sizeof(uintptr_t)) - heapMapClearIndex;

			if (commit) {
				/* commit memory */
				if (_extensions->isFvtestForceReferenceChainWalkerMarkMapCommitFailure()) {
					result = false;
					Trc_MM_ReferenceChainWalkerMarkMap_markMapCommitFailureForced(env->getLanguageVMThread());
					break;
				} else {
					if (!memoryManager->commitMemory(&_heapMapMemoryHandle, (void *) (((uintptr_t)_heapMapBits) + heapMapClearIndex), heapMapClearSize)) {
						result = false;
						/* print tracepoint in case of real failure */
						Trc_MM_ReferenceChainWalkerMarkMap_markMapCommitFailed(env->getLanguageVMThread(), (void *) (((uintptr_t)_heapMapBits) + heapMapClearIndex), heapMapClearSize);
						break;
					}
				}
			}

			/* And clear the mark map */
			OMRZeroMemory((void *) (((uintptr_t)_heapMapBits) + heapMapClearIndex), heapMapClearSize);
		}
	}
	return result;
}

