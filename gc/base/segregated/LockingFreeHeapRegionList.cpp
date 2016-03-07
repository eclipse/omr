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

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronopt.h"
#include "sizeclasses.h"

#include "LockingFreeHeapRegionList.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

MM_LockingFreeHeapRegionList *
MM_LockingFreeHeapRegionList::newInstance(MM_EnvironmentBase *env, MM_HeapRegionList::RegionListKind regionListKind, bool singleRegionsOnly)
{
	MM_LockingFreeHeapRegionList *fpl = (MM_LockingFreeHeapRegionList *)env->getForge()->allocate(sizeof(MM_LockingFreeHeapRegionList), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (fpl) {
		new (fpl) MM_LockingFreeHeapRegionList(regionListKind, singleRegionsOnly);
		if (!fpl->initialize(env)) {
			fpl->kill(env);
			return NULL;
		}		
	}
	return fpl;
}

void
MM_LockingFreeHeapRegionList::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_LockingFreeHeapRegionList::initialize(MM_EnvironmentBase *env)
{
	if (0 != omrthread_monitor_init_with_name(&_lockMonitor, 0, "FreeHeapRegionList lock monitor")) {
		return false;
	}
	return true;
}
	
void
MM_LockingFreeHeapRegionList::tearDown(MM_EnvironmentBase *env)
{
	if (_lockMonitor) {
		omrthread_monitor_destroy(_lockMonitor);
		_lockMonitor = NULL;
	}
}

uintptr_t
MM_LockingFreeHeapRegionList::getTotalRegions()
{
	uintptr_t count = 0;
	lock();
	for (MM_HeapRegionDescriptorSegregated *cur = _head; cur != NULL; cur = cur->getNext()) {
		count += cur->getRange();		
	}
	unlock();
	return count;
}

uintptr_t
MM_LockingFreeHeapRegionList::getMaxRegions()
{
	uintptr_t max = 0;
	lock();
	for (MM_HeapRegionDescriptorSegregated *cur = _head; cur != NULL; cur = cur->getNext()) {
		max = (max > cur->getRange()) ? max : cur->getRange();
	}
	unlock();
	return max;
}

void
MM_LockingFreeHeapRegionList::showList(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uintptr_t index = 0;
	uintptr_t count = 0;
	lock();
	omrtty_printf("LockingFreeHeapRegionList 0x%x: ", this);
	for (MM_HeapRegionDescriptorSegregated *cur = _head; cur != NULL; cur = cur->getNext()) {
		omrtty_printf("  %d-%d-%d ", count, index, cur->getRange());
		count += 1;
		index += cur->getRange();
	}
	omrtty_printf("\n");
	unlock();
}

MM_HeapRegionDescriptorSegregated*
MM_LockingFreeHeapRegionList::allocate(MM_EnvironmentBase *env, uintptr_t szClass, uintptr_t numRegions, uintptr_t maxExcess)
{
	lock();
	for (MM_HeapRegionDescriptorSegregated *cur = _head; cur != NULL; cur = cur->getNext()) {
		uintptr_t currentSize = cur->getRange();
		if ((currentSize >= numRegions) && cur->isCommitted()) {
			uintptr_t leftOver = currentSize - numRegions;
			if (leftOver < maxExcess) {
				/* The detach call is safe even though we are iterating over the list because iterations stops immediately. */
				detachInternal(cur);
				if (leftOver > 0) {
					MM_HeapRegionDescriptorSegregated *remainder = cur->splitRange(numRegions);
					pushInternal(remainder);
				}
				cur->setRangeHead(cur);
				if (szClass == OMR_SIZECLASSES_LARGE) {
					cur->setLarge(numRegions);
#if defined(OMR_GC_ARRAYLETS)
				} else if (szClass == OMR_SIZECLASSES_ARRAYLET) {
					cur->setArraylet();
#endif /* defined(OMR_GC_ARRAYLETS) */		
				} else {
					cur->setSmall(szClass);
				}
				unlock();
				return cur;
			}
		}
	}
	unlock();
	return NULL;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
