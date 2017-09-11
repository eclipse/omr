/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "omrcfg.h"
#include "omrport.h"
#include "modronopt.h"

#include "AllocateDescription.hpp"
#include "AtomicOperations.hpp"
#include "EnvironmentBase.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "LockingHeapRegionQueue.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/**
 * @param trackFreeBytes True if a running total of free bytes in the list should be kept.
 * This value will not necessarily be accurate. It will remain fairly accurate as long as single
 * region push and pop operations are used and updateCounts doesn't get called on a region that lives
 * on the list.
 */
MM_LockingHeapRegionQueue *
MM_LockingHeapRegionQueue::newInstance(MM_EnvironmentBase *env, RegionListKind regionListKind, bool singleRegionsOnly, bool concurrentAccess, bool trackFreeBytes)
{
	MM_LockingHeapRegionQueue *regionList = (MM_LockingHeapRegionQueue *)env->getForge()->allocate(sizeof(MM_LockingHeapRegionQueue), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (regionList) {
		new (regionList) MM_LockingHeapRegionQueue(regionListKind, singleRegionsOnly, concurrentAccess, trackFreeBytes);
		if (!regionList->initialize(env)) {
			regionList->kill(env);
			return NULL;
		}		
	}
	return regionList;
}

void
MM_LockingHeapRegionQueue::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

bool
MM_LockingHeapRegionQueue::initialize(MM_EnvironmentBase *env)
{
	if (_needLock && (0 != omrthread_monitor_init_with_name(&_lockMonitor, 0, "RegionList lock monitor"))) {
		return false;
	}
	
	return true;
}
	
void
MM_LockingHeapRegionQueue::tearDown(MM_EnvironmentBase *env)
{
	assert1(isEmpty());
	if (_needLock && _lockMonitor) {
		omrthread_monitor_destroy(_lockMonitor);
		_lockMonitor = NULL;
	}
}

uintptr_t
MM_LockingHeapRegionQueue::getTotalRegions()
{
	if (_singleRegionsOnly) {
		return length();
	} else {
		uintptr_t count = 0;
		lock();
		for (MM_HeapRegionDescriptorSegregated *cur = _head; cur != NULL; cur = cur->getNext()) {
			count += cur->getRange();
		}
		unlock();
		return count;
	}
}

void
MM_LockingHeapRegionQueue::showList(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	uintptr_t index = 0;
	uintptr_t count = 0;	
	lock();
	omrtty_printf("LockingHeapRegionList 0x%x: ", this);
	for (MM_HeapRegionDescriptorSegregated *cur = _head; cur != NULL; cur = cur->getNext()) {
		omrtty_printf("  %d-%d-%d ", count, index, cur->getRange());
		count += 1;
		index += cur->getRange();
	}
	omrtty_printf("\n");
	unlock();
}

/**
 * DEBUG method that iterates over all regions in the list and sums up the free bytes.
 * @note This is extremely slow but accurate since it will iterate over the entire region and count
 * the unused cells.
 * @see MM_HeapRegionDescriptorSegregated::debugCountFreeBytes()
 */
uintptr_t
MM_LockingHeapRegionQueue::debugCountFreeBytesInRegions()
{
	uintptr_t freeBytes = 0;
	lock();
	for (MM_HeapRegionDescriptorSegregated *cur = _head; cur != NULL; cur = cur->getNext()) {
		freeBytes += cur->debugCountFreeBytes();
	}
	unlock();
	return freeBytes;
}

#endif /* OMR_GC_SEGREGATED_HEAP */
