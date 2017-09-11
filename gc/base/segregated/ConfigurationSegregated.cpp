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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Metronome
 */

#include "omrcfg.h"
#include "MemorySpacesAPI.h"

#include "ConfigurationSegregated.hpp"
#include "EnvironmentBase.hpp"
#include "GCExtensionsBase.hpp"
#include "GlobalAllocationManagerSegregated.hpp"
#include "HeapRegionDescriptorSegregated.hpp"
#include "HeapRegionManagerTarok.hpp"
#include "HeapVirtualMemory.hpp"
#include "MemoryPoolSegregated.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpaceSegregated.hpp"
#include "ParallelDispatcher.hpp"
#include "PhysicalArenaRegionBased.hpp"
#include "PhysicalSubArenaRegionBased.hpp"
#include "RegionPoolSegregated.hpp"
#include "SegregatedAllocationInterface.hpp"
#include "SegregatedAllocationTracker.hpp"
#include "SegregatedGC.hpp"
#include "SizeClasses.hpp"

#if defined(OMR_GC_SEGREGATED_HEAP)

/* from j9consts.h */
#define SEGREGATED_REGION_SIZE_BYTES (64 * 1024)

/* Define realtime arraylet leaf size as the largest small size class we can have */
#define SEGREGATED_ARRAYLET_LEAF_SIZE_BYTES OMR_SIZECLASSES_MAX_SMALL_SIZE_BYTES

/* from metronome.h */
#define MINIMUM_FREE_CHUNK_SIZE 64

MM_Configuration *
MM_ConfigurationSegregated::newInstance(MM_EnvironmentBase *env)
{
	MM_ConfigurationSegregated *configuration;

	configuration = (MM_ConfigurationSegregated *) env->getForge()->allocate(sizeof(MM_ConfigurationSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(NULL != configuration) {
		new(configuration) MM_ConfigurationSegregated(env);
		if(!configuration->initialize(env)) {
			configuration->kill(env);
			configuration = NULL;
		}
	}
	return configuration;
}

bool
MM_ConfigurationSegregated::initialize(MM_EnvironmentBase *env)
{

	bool success = false;

	/* OMRTODO investigate why these must be equal or it segfaults. */
	MM_GCExtensionsBase *extensions = env->getExtensions();
	extensions->splitAvailableListSplitAmount = extensions->gcThreadCount;

	if (MM_Configuration::initialize(env)) {
		env->getOmrVM()->_sizeClasses = _delegate.getSegregatedSizeClasses(env);
		if (NULL != env->getOmrVM()->_sizeClasses) {
			extensions->setSegregatedHeap(true);
			extensions->setStandardGC(true);
			extensions->arrayletsPerRegion = extensions->regionSize / env->getOmrVM()->_arrayletLeafSize;
			success = true;
		}
	}
	return success;
}

void
MM_ConfigurationSegregated::tearDown(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* extensions = env->getExtensions();

	if (NULL != extensions->defaultSizeClasses) {
		extensions->defaultSizeClasses->kill(env);
		extensions->defaultSizeClasses = NULL;
	}

	MM_Configuration::tearDown(env);
}

MM_Heap *
MM_ConfigurationSegregated::createHeapWithManager(MM_EnvironmentBase *env, uintptr_t heapBytesRequested, MM_HeapRegionManager *regionManager)
{
	return MM_HeapVirtualMemory::newInstance(env, env->getExtensions()->heapAlignment, heapBytesRequested, regionManager);
}

MM_MemorySpace *
MM_ConfigurationSegregated::createDefaultMemorySpace(MM_EnvironmentBase *env, MM_Heap *heap, MM_InitializationParameters *parameters)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_MemoryPoolSegregated *memoryPool = NULL;
	MM_MemorySubSpaceSegregated *memorySubSpaceSegregated = NULL;
	MM_PhysicalSubArenaRegionBased *physicalSubArena = NULL;
	MM_PhysicalArenaRegionBased *physicalArena = NULL;
	MM_RegionPoolSegregated *regionPool = NULL;

	if(NULL == (extensions->defaultSizeClasses = MM_SizeClasses::newInstance(env))) {
		return NULL;
	}

	regionPool = MM_RegionPoolSegregated::newInstance(env, extensions->heapRegionManager);
	if (NULL == regionPool) {
		return NULL;
	}

	extensions->globalAllocationManager = MM_GlobalAllocationManagerSegregated::newInstance(env, regionPool);
	if(NULL == extensions->globalAllocationManager) {
		return NULL;
	}

	if(NULL == (memoryPool = MM_MemoryPoolSegregated::newInstance(env, regionPool, MINIMUM_FREE_CHUNK_SIZE, (MM_GlobalAllocationManagerSegregated*)extensions->globalAllocationManager))) {
		return NULL;
	}

	if(NULL == (physicalSubArena = MM_PhysicalSubArenaRegionBased::newInstance(env, heap))) {
		memoryPool->kill(env);
		return NULL;
	}

	memorySubSpaceSegregated = MM_MemorySubSpaceSegregated::newInstance(env, physicalSubArena, memoryPool, true, parameters->_minimumSpaceSize, parameters->_initialOldSpaceSize, parameters->_maximumSpaceSize);
	if(NULL == memorySubSpaceSegregated) {
		return NULL;
	}

	if(NULL == (physicalArena = MM_PhysicalArenaRegionBased::newInstance(env, heap))) {
		memorySubSpaceSegregated->kill(env);
		return NULL;
	}

	return MM_MemorySpace::newInstance(env, heap, physicalArena, memorySubSpaceSegregated, parameters, MEMORY_SPACE_NAME_METRONOME, MEMORY_SPACE_DESCRIPTION_METRONOME);
}

void
MM_ConfigurationSegregated::defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace)
{
	/* OMRTODO Fix the assumption that we are using SegregatedGC */
	((MM_GlobalAllocationManagerSegregated *) extensions->globalAllocationManager)->setSweepScheme(((MM_SegregatedGC *) extensions->getGlobalCollector())->getSweepScheme());
	((MM_GlobalAllocationManagerSegregated *) extensions->globalAllocationManager)->setMarkingScheme(((MM_SegregatedGC *) extensions->getGlobalCollector())->getMarkingScheme());
}

MM_EnvironmentBase *
MM_ConfigurationSegregated::allocateNewEnvironment(MM_GCExtensionsBase *extensions, OMR_VMThread *omrVMThread)
{
	return MM_EnvironmentBase::newInstance(extensions, omrVMThread);
}

J9Pool *
MM_ConfigurationSegregated::createEnvironmentPool(MM_EnvironmentBase *env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());

	uintptr_t numberOfElements = getConfigurationDelegate()->getInitialNumberOfPooledEnvironments(env);
	/* number of elements, pool flags = 0, 0 selects default pool configuration (at least 1 element, puddle size rounded to OS page size) */
	return pool_new(sizeof(MM_EnvironmentBase), numberOfElements, sizeof(uint64_t), 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(OMRPORTLIB));
}

bool
MM_ConfigurationSegregated::initializeEnvironment(MM_EnvironmentBase *env)
{
	if (MM_Configuration::initializeEnvironment(env)) {
		env->getExtensions()->globalAllocationManager->acquireAllocationContext(env);
		if (env->getAllocationContext() != NULL) {
			/* BEN TODO: VMDESIGN 1880: When allocation trackers are commonized, the setting of the allocation
			 * tracker in the env should be done by the base configuration initializeEnvironment.
			 */
			env->_allocationTracker = ((MM_MemoryPoolSegregated *)env->getExtensions()->heap->getDefaultMemorySpace()->getDefaultMemorySubSpace()->getMemoryPool())->createAllocationTracker(env);
			return (NULL != env->_allocationTracker);
		}
	}
	return false;
}

MM_HeapRegionManager *
MM_ConfigurationSegregated::createHeapRegionManager(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	uintptr_t descriptorSize = sizeof(MM_HeapRegionDescriptorSegregated) + sizeof(uintptr_t *) * extensions->arrayletsPerRegion;
	
	MM_HeapRegionManagerTarok *heapRegionManager = MM_HeapRegionManagerTarok::newInstance(env, extensions->regionSize, descriptorSize, MM_HeapRegionDescriptorSegregated::initializer, MM_HeapRegionDescriptorSegregated::destructor);
	return heapRegionManager;
}

/**
 * Create the global collector for a Standard configuration
 */
MM_GlobalCollector*
MM_ConfigurationSegregated::createGlobalCollector(MM_EnvironmentBase* env)
{
	return MM_SegregatedGC::newInstance(env, env->getExtensions()->collectorLanguageInterface);
}

MM_Dispatcher *
MM_ConfigurationSegregated::createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize)
{
	return MM_ParallelDispatcher::newInstance(env, handler, handler_arg, defaultOSStackSize);
}

#endif /* OMR_GC_SEGREGATED_HEAP */
