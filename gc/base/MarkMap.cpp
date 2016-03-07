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
 * @ingroup GC_Base_Core
 */

#include "omrcfg.h"
#include "omr.h"

#include "Dispatcher.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "HeapRegionDescriptor.hpp"
#include "HeapRegionIterator.hpp"
#include "HeapRegionManager.hpp"
#include "MarkMap.hpp"
#include "Task.hpp"


/**
 * Object creation and destruction
 */
MM_MarkMap *
MM_MarkMap::newInstance(MM_EnvironmentBase *env, uintptr_t maxHeapSize)
{
	MM_MarkMap *markMap = (MM_MarkMap *)env->getForge()->allocate(sizeof(MM_MarkMap), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != markMap) {
		new(markMap) MM_MarkMap(env, maxHeapSize);
		if (!markMap->initialize(env)) {
			markMap->kill(env);
			markMap = NULL;
		}
	}

	return markMap;
}

void
MM_MarkMap::initializeMarkMap(MM_EnvironmentBase *env)
{
	/* TODO: The multiplier should really be some constant defined globally */
	const uintptr_t MODRON_PARALLEL_MULTIPLIER = 32;
	uintptr_t heapAlignment = _extensions->heapAlignment;

	/* Determine the size of heap that a work unit of mark map clearing corresponds to */
	uintptr_t heapClearUnitFactor = env->_currentTask->getThreadCount();
	heapClearUnitFactor = ((heapClearUnitFactor == 1) ? 1 : heapClearUnitFactor * MODRON_PARALLEL_MULTIPLIER);
	uintptr_t heapClearUnitSize = _extensions->heap->getMemorySize() / heapClearUnitFactor;
	heapClearUnitSize = MM_Math::roundToCeiling(heapAlignment, heapClearUnitSize);

	/* Walk all object segments to determine what ranges of the mark map should be cleared */
	MM_HeapRegionDescriptor *region;
	MM_Heap *heap = _extensions->getHeap();
	MM_HeapRegionManager *regionManager = heap->getHeapRegionManager();
	GC_HeapRegionIterator regionIterator(regionManager);
	while(NULL != (region = regionIterator.nextRegion())) {
		if (region->isCommitted()) {
			/* Walk the segment in chunks the size of the heapClearUnit size, checking if the corresponding mark map
			 * range should  be cleared.
			 */
			uint8_t* heapClearAddress = (uint8_t*)region->getLowAddress();
			uintptr_t heapClearSizeRemaining = region->getSize();

			while(0 != heapClearSizeRemaining) {
				/* Calculate the size of heap that is to be processed */
				uintptr_t heapCurrentClearSize = (heapClearUnitSize > heapClearSizeRemaining) ? heapClearSizeRemaining : heapClearUnitSize;
				Assert_MM_true(heapCurrentClearSize > 0);

				/* Check if the thread should clear the corresponding mark map range for the current heap range */
				if(J9MODRON_HANDLE_NEXT_WORK_UNIT(env)) {
					/* Convert the heap address/size to its corresponding mark map address/size */
					/* NOTE: We calculate the low and high heap offsets, and build the mark map index and size values
					 * from these to avoid rounding errors (if we use the size, the conversion routine could get a different
					 * rounding result then the actual end address)
					 */
					uintptr_t heapClearOffset = ((uintptr_t)heapClearAddress) - _heapMapBaseDelta;
					uintptr_t heapMapClearIndex = convertHeapIndexToHeapMapIndex(env, heapClearOffset, sizeof(uintptr_t));
					uintptr_t heapMapClearSize =
						convertHeapIndexToHeapMapIndex(env, heapClearOffset + heapCurrentClearSize, sizeof(uintptr_t))
						- heapMapClearIndex;

					/* And clear the mark map */
					OMRZeroMemory((void *) (((uintptr_t)_heapMapBits) + heapMapClearIndex), heapMapClearSize);
				}

				/* Move to the next address range in the segment */
				heapClearAddress += heapCurrentClearSize;
				heapClearSizeRemaining -= heapCurrentClearSize;
			}
		}
	}
}
