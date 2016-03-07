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

/**
 * @file
 * @ingroup GC_Modron_Metronome
 */

#include "omrcfg.h"
#include "MemorySpacesAPI.h"

#include "ConfigurationLanguageInterface.hpp"
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
MM_ConfigurationSegregated::newInstance(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface* cli)
{
	MM_ConfigurationSegregated *configuration;

	configuration = (MM_ConfigurationSegregated *) env->getForge()->allocate(sizeof(MM_ConfigurationSegregated), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(NULL != configuration) {
		new(configuration) MM_ConfigurationSegregated(env, cli);
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
	MM_GCExtensionsBase *extensions = env->getExtensions();

	bool success = MM_Configuration::initialize(env);

	/* OMRTODO investigate why these must be equal or it segfaults. */
	extensions->splitAvailableListSplitAmount = extensions->gcThreadCount;

	if (success) {
		success = MM_Configuration::initializeSizeClasses(env);
		if (success) {
			extensions->setSegregatedHeap(true);
			extensions->setStandardGC(true);

			extensions->arrayletsPerRegion = extensions->regionSize / env->getOmrVM()->_arrayletLeafSize;
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

	uintptr_t numberElements = _configurationLanguageInterface->getEnvPoolNumElements();
	uintptr_t poolFlags = _configurationLanguageInterface->getEnvPoolFlags();

	return pool_new(sizeof(MM_EnvironmentBase), numberElements, sizeof(uint64_t), poolFlags, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_MM, POOL_FOR_PORT(OMRPORTLIB));
}

bool
MM_ConfigurationSegregated::initializeEnvironment(MM_EnvironmentBase *env)
{
	if (!MM_Configuration::initializeEnvironment(env)) {
		return false;
	}
	
	if (!aquireAllocationContext(env)) {
		return false;
	}
	
	/* BEN TODO: VMDESIGN 1880: When allocation trackers are commonized, the setting of the allocation
	 * tracker in the env should be done by the base configuration initializeEnvironment.
	 */
	env->_allocationTracker = createAllocationTracker(env);
	if(NULL == env->_allocationTracker) {
		return false;
	}
	
	return true;
}

/**
 * Initialize and return the appropriate object allocation interface for the given Environment.
 * Given the allocation scheme and particular environment, select and return the appropriate object allocation interface.
 * @return new subtype instance of MM_ObjectAllocationInterface, or NULL on failure.
 */ 
MM_ObjectAllocationInterface *
MM_ConfigurationSegregated::createObjectAllocationInterface(MM_EnvironmentBase *env)
{
	return MM_SegregatedAllocationInterface::newInstance(env);
}

/**
 * Get an allocationContext for this thread.
 */
bool 
MM_ConfigurationSegregated::aquireAllocationContext(MM_EnvironmentBase *env)
{
	env->getExtensions()->globalAllocationManager->acquireAllocationContext(env);
	return (env->getAllocationContext() != NULL);
}

/**
 * Initialize and return the appropriate allocation tracker for the given Environment. Note
 * that this function is currently called by the ConfigurationRealtime since Realtime GCs are
 * currently the only configurations with allocation trackers. When this changes, this function
 * should be called by the base configuration initializeEnvironment.
 */
MM_SegregatedAllocationTracker*
MM_ConfigurationSegregated::createAllocationTracker(MM_EnvironmentBase* env)
{
	/* BEN TODO: Fix this ugly assumption that there is only one memory pool...  */
	return ((MM_MemoryPoolSegregated *)env->getExtensions()->heap->getDefaultMemorySpace()->getDefaultMemorySubSpace()->getMemoryPool())->createAllocationTracker(env);
}

MM_HeapRegionManager *
MM_ConfigurationSegregated::createHeapRegionManager(MM_EnvironmentBase *env)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	uintptr_t descriptorSize = sizeof(MM_HeapRegionDescriptorSegregated) + sizeof(uintptr_t *) * extensions->arrayletsPerRegion;
	
	MM_HeapRegionManagerTarok *heapRegionManager = MM_HeapRegionManagerTarok::newInstance(env, extensions->regionSize, descriptorSize, MM_HeapRegionDescriptorSegregated::initializer, MM_HeapRegionDescriptorSegregated::destructor);
	return heapRegionManager;
}

uintptr_t
MM_ConfigurationSegregated::internalGetDefaultRegionSize(MM_EnvironmentBase *env)
{
	return SEGREGATED_REGION_SIZE_BYTES;
}

uintptr_t
MM_ConfigurationSegregated::internalGetDefaultArrayletLeafSize(MM_EnvironmentBase *env)
{
	return SEGREGATED_ARRAYLET_LEAF_SIZE_BYTES;
}

uintptr_t
MM_ConfigurationSegregated::internalGetWriteBarrierType(MM_EnvironmentBase *env)
{
	return _configurationLanguageInterface->internalGetWriteBarrierType(env);
}

uintptr_t
MM_ConfigurationSegregated::internalGetAllocationType(MM_EnvironmentBase *env)
{
	return _configurationLanguageInterface->internalGetAllocationType(env);
}

/**
 * Create the global collector for a Standard configuration
 */
MM_GlobalCollector*
MM_ConfigurationSegregated::createGlobalCollector(MM_EnvironmentBase* env)
{
	/* OMRTODO should we be going through the configurationLanguageInterface to create the segregated GC? */
	//return _configurationLanguageInterface->createGlobalCollector(env);
	return MM_SegregatedGC::newInstance(env, MM_GCExtensionsBase::getExtensions(env->getOmrVM())->collectorLanguageInterface);
}

MM_Dispatcher *
MM_ConfigurationSegregated::createDispatcher(MM_EnvironmentBase *env, omrsig_handler_fn handler, void* handler_arg, uintptr_t defaultOSStackSize)
{
	return MM_ParallelDispatcher::newInstance(env, handler, handler_arg, defaultOSStackSize);
}

#endif /* OMR_GC_SEGREGATED_HEAP */
