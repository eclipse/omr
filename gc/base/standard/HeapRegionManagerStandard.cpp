/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

#include "HeapRegionManagerStandard.hpp"

MM_HeapRegionManagerStandard::MM_HeapRegionManagerStandard(MM_EnvironmentBase *env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
	: MM_HeapRegionManager(env, regionSize, tableDescriptorSize, regionDescriptorInitializer, regionDescriptorDestructor)
	,_lowHeapAddress(NULL)
	,_highHeapAddress(NULL)
{
	_typeId = __FUNCTION__;
}

MM_HeapRegionManagerStandard *
MM_HeapRegionManagerStandard::newInstance(MM_EnvironmentBase *env, uintptr_t regionSize, uintptr_t tableDescriptorSize, MM_RegionDescriptorInitializer regionDescriptorInitializer, MM_RegionDescriptorDestructor regionDescriptorDestructor)
{
	MM_HeapRegionManagerStandard *regionManager = (MM_HeapRegionManagerStandard *)env->getForge()->allocate(sizeof(MM_HeapRegionManagerStandard), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (regionManager) {
		new(regionManager) MM_HeapRegionManagerStandard(env, regionSize, tableDescriptorSize, regionDescriptorInitializer, regionDescriptorDestructor);
		if (!regionManager->initialize(env)) {
			regionManager->kill(env);
			regionManager = NULL;
		}
	}
	return regionManager;
}

bool
MM_HeapRegionManagerStandard::initialize(MM_EnvironmentBase *env)
{
	return MM_HeapRegionManager::initialize(env);
}

void
MM_HeapRegionManagerStandard::tearDown(MM_EnvironmentBase *env)
{
	MM_HeapRegionManager::tearDown(env);
}

bool
MM_HeapRegionManagerStandard::setContiguousHeapRange(MM_EnvironmentBase *env, void *lowHeapEdge, void *highHeapEdge)
{
	writeLock();
	/* ensure that this manager was configured with a valid region size */
	Assert_MM_true(0 != _regionSize);
	/* we don't yet support multiple enabling calls (split heaps) */
	Assert_MM_true(NULL == _regionTable);
	/* the regions must be aligned (in present implementation) */
	Assert_MM_true(0 == ((uintptr_t)lowHeapEdge % _regionSize));
	Assert_MM_true(0 == ((uintptr_t)highHeapEdge % _regionSize));
	/* make sure that the range is in the right order and of non-zero size*/
	Assert_MM_true(highHeapEdge > lowHeapEdge);
	_lowHeapAddress = lowHeapEdge;
	_highHeapAddress = highHeapEdge;

	writeUnlock();
	return true;
}

void
MM_HeapRegionManagerStandard::destroyRegionTable(MM_EnvironmentBase *env)
{
}

bool
MM_HeapRegionManagerStandard::enableRegionsInTable(MM_EnvironmentBase *env, MM_MemoryHandle *handle)
{
	return true;
}

MM_HeapMemorySnapshot*
MM_HeapRegionManagerStandard::getHeapMemorySnapshot(MM_GCExtensionsBase *extensions, MM_HeapMemorySnapshot* snapshot, bool gcEnd)
{
	MM_Heap *heap = extensions->getHeap();
	snapshot->_totalHeapSize = heap->getActiveMemorySize();
	snapshot->_freeHeapSize = heap->getApproximateFreeMemorySize();

	snapshot->_totalTenuredSize = heap->getActiveMemorySize(MEMORY_TYPE_OLD);
	snapshot->_freeTenuredSize = heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_OLD);

	if (extensions->largeObjectArea) {
		snapshot->_totalTenuredLOASize = heap->getActiveLOAMemorySize(MEMORY_TYPE_OLD);
		snapshot->_freeTenuredLOASize = heap->getApproximateActiveFreeLOAMemorySize(MEMORY_TYPE_OLD);
		snapshot->_totalTenuredSOASize = snapshot->_totalTenuredSize - snapshot->_totalTenuredLOASize;
		snapshot->_freeTenuredSOASize = snapshot->_freeTenuredSize - snapshot->_freeTenuredLOASize;
	}
#if defined(OMR_GC_MODRON_SCAVENGER)
	if (extensions->scavengerEnabled) {
		snapshot->_totalNurseryAllocateSize = heap->getActiveMemorySize(MEMORY_TYPE_NEW) - heap->getActiveSurvivorMemorySize(MEMORY_TYPE_NEW);
		snapshot->_freeNurseryAllocateSize = heap->getApproximateActiveFreeMemorySize(MEMORY_TYPE_NEW);
		snapshot->_totalNurserySurvivorSize = heap->getActiveSurvivorMemorySize(MEMORY_TYPE_NEW);
		snapshot->_freeNurserySurvivorSize = 0;
	}
#endif /* OMR_GC_MODRON_SCAVENGER */
	return snapshot;
}
