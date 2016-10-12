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

/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#include "omr/gc/memorySpacesAPI.h"
#include "omr/gc/modronAssertions.h"

#if defined(OMR_GC_MODRON_SCAVENGER)

#include "ConfigurationGenerational.hpp"

#include "EnvironmentStandard.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapSplit.hpp"
#include "MemoryPool.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpaceFlat.hpp"
#include "MemorySubSpaceGenerational.hpp"
#include "MemorySubSpaceGeneric.hpp"
#include "MemorySubSpaceSemiSpace.hpp"
#include "PhysicalArenaVirtualMemory.hpp"
#include "PhysicalSubArenaVirtualMemoryFlat.hpp"
#include "PhysicalSubArenaVirtualMemorySemiSpace.hpp"
#include "Scavenger.hpp"

MM_Configuration *
MM_ConfigurationGenerational::newInstance(MM_EnvironmentBase *env, MM_ConfigurationLanguageInterface* configurationLanguageInterface)
{
	MM_ConfigurationGenerational *configuration;
	
	configuration = (MM_ConfigurationGenerational *) env->getForge()->allocate(sizeof(MM_ConfigurationGenerational), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if(NULL != configuration) {
		new(configuration) MM_ConfigurationGenerational(env, configurationLanguageInterface);
		if(!configuration->initialize(env)) {
			configuration->kill(env);
			configuration = NULL;
		}
	}
	return configuration;
}

MM_MemorySubSpaceSemiSpace *
MM_ConfigurationGenerational::createSemiSpace(MM_EnvironmentBase *envBase, MM_Heap *heap, MM_Scavenger *scavenger, MM_InitializationParameters *parameters, UDATA numaNode)
{
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	MM_GCExtensionsBase *ext = env->getExtensions();
	MM_MemorySubSpaceSemiSpace *memorySubSpaceSemiSpace = NULL;
	MM_MemoryPoolAddressOrderedList *memoryPoolAllocate = NULL, *memoryPoolSurvivor = NULL;
	MM_MemorySubSpaceGeneric *memorySubSpaceGenericAllocate = NULL, *memorySubSpaceGenericSurvivor = NULL;
	MM_PhysicalSubArenaVirtualMemorySemiSpace *physicalSubArenaSemiSpace = NULL;
	UDATA minimumFreeEntrySize = ext->getMinimumFreeEntrySize();

	/* Create Sweep Pool Manager for Nursery if we can not use manager created for Tenure */
	if (!createSweepPoolManagerAddressOrderedList(env)) {
		return NULL;
	}

	/* allocate space */
	if(NULL == (memoryPoolAllocate = MM_MemoryPoolAddressOrderedList::newInstance(env, minimumFreeEntrySize, "Allocate/Survivor1"))) {
		return NULL;
	}
	if(NULL == (memorySubSpaceGenericAllocate = MM_MemorySubSpaceGeneric::newInstance(env, memoryPoolAllocate, NULL, false, parameters->_minimumNewSpaceSize / 2, parameters->_initialNewSpaceSize / 2, parameters->_maximumNewSpaceSize, MEMORY_TYPE_NEW, 0))) {
		memoryPoolAllocate->kill(env);
		return NULL;
	}

	/* survivor space */
	if(NULL == (memoryPoolSurvivor = MM_MemoryPoolAddressOrderedList::newInstance(env, minimumFreeEntrySize, "Allocate/Survivor2"))) {
		memorySubSpaceGenericAllocate->kill(env);
		return NULL;
	}
	if(NULL == (memorySubSpaceGenericSurvivor = MM_MemorySubSpaceGeneric::newInstance(env, memoryPoolSurvivor, NULL, false, parameters->_minimumNewSpaceSize / 2, parameters->_initialNewSpaceSize / 2, parameters->_maximumNewSpaceSize, MEMORY_TYPE_NEW, 0))) {
		memoryPoolSurvivor->kill(env);
		memorySubSpaceGenericAllocate->kill(env);
		return NULL;
	}

	if(NULL == (physicalSubArenaSemiSpace = MM_PhysicalSubArenaVirtualMemorySemiSpace::newInstance(envBase, heap))) {
		memorySubSpaceGenericAllocate->kill(env);
		memorySubSpaceGenericSurvivor->kill(env);
		return NULL;
	}
	physicalSubArenaSemiSpace->setNumaNode(numaNode);
	if(NULL == (memorySubSpaceSemiSpace = MM_MemorySubSpaceSemiSpace::newInstance(env, scavenger, physicalSubArenaSemiSpace, memorySubSpaceGenericAllocate, memorySubSpaceGenericSurvivor, false, parameters->_minimumNewSpaceSize, parameters->_initialNewSpaceSize, parameters->_maximumNewSpaceSize))) {
		memorySubSpaceGenericAllocate->kill(env);
		memorySubSpaceGenericSurvivor->kill(env);
		physicalSubArenaSemiSpace->kill(env);
		return NULL;
	}

	return memorySubSpaceSemiSpace;
}


MM_MemorySpace *
MM_ConfigurationGenerational::createDefaultMemorySpace(MM_EnvironmentBase *envBase, MM_Heap *heap, MM_InitializationParameters *parameters)
{
	MM_MemoryPool *memoryPoolOld = NULL;
	MM_MemorySubSpaceGeneric *memorySubSpaceGenericOld = NULL;
	MM_PhysicalSubArenaVirtualMemoryFlat *physicalSubArenaFlat = NULL;
	MM_MemorySubSpaceFlat *memorySubSpaceOld = NULL;
	MM_Scavenger *scavenger = NULL;
	MM_MemorySubSpaceGenerational *memorySubSpaceGenerational = NULL;
	MM_MemorySubSpace *memorySubSpaceNew = NULL;
	MM_PhysicalArenaVirtualMemory *physicalArena = NULL;
	MM_EnvironmentStandard *env = MM_EnvironmentStandard::getEnvironment(envBase);
	MM_GCExtensionsBase *ext = env->getExtensions();
	
	/* first we do the structures that correspond to the "old area" */
	if(NULL == (memoryPoolOld = createMemoryPool(env, true))) {
		return NULL;
	}
	if(NULL == (memorySubSpaceGenericOld = MM_MemorySubSpaceGeneric::newInstance(env, memoryPoolOld, NULL, false, parameters->_minimumOldSpaceSize, parameters->_initialOldSpaceSize, parameters->_maximumOldSpaceSize, MEMORY_TYPE_OLD, 0))) {
		memoryPoolOld->kill(env);
		return NULL;
	}
	if(NULL == (physicalSubArenaFlat = MM_PhysicalSubArenaVirtualMemoryFlat::newInstance(env, heap))) {
		memorySubSpaceGenericOld->kill(env);
		return NULL;
	}

	if(NULL == (memorySubSpaceOld = MM_MemorySubSpaceFlat::newInstance(env, physicalSubArenaFlat, memorySubSpaceGenericOld, false, parameters->_minimumOldSpaceSize, parameters->_initialOldSpaceSize, parameters->_maximumOldSpaceSize, MEMORY_TYPE_OLD, 0))) {
		physicalSubArenaFlat->kill(env);
		memorySubSpaceGenericOld->kill(env);
		return NULL;
	}

	/* then we build "new-space" - note that if we fail during this we must remember to kill() the oldspace we created */

	/* join them with a semispace */
	if(NULL == (scavenger = MM_Scavenger::newInstance(env, ext->collectorLanguageInterface, ext->heapRegionManager))) {
		memorySubSpaceOld->kill(env);
		return NULL;
	}

	if(NULL == (memorySubSpaceNew = createSemiSpace(env, heap, scavenger, parameters))) {
		memorySubSpaceOld->kill(env);
	}
	
	/* then we join newspace and oldspace together */;
	if(NULL == (memorySubSpaceGenerational = MM_MemorySubSpaceGenerational::newInstance(envBase, memorySubSpaceNew, memorySubSpaceOld, true, parameters->_minimumSpaceSize, parameters->_minimumNewSpaceSize, parameters->_initialNewSpaceSize, parameters->_maximumNewSpaceSize, parameters->_minimumOldSpaceSize, parameters->_initialOldSpaceSize, parameters->_maximumOldSpaceSize, parameters->_maximumSpaceSize))) {
		memorySubSpaceNew->kill(env);
		memorySubSpaceOld->kill(env);
		return NULL;
	}
	
	if(NULL == (physicalArena = MM_PhysicalArenaVirtualMemory::newInstance(env, heap))) {
		memorySubSpaceGenerational->kill(env);
		return NULL;
	}
	
	/* register the scavenger in GCExtensionsBase */
	ext->scavenger = scavenger;

	return MM_MemorySpace::newInstance(env, heap, physicalArena, memorySubSpaceGenerational, parameters, MEMORY_SPACE_NAME_GENERATIONAL, MEMORY_SPACE_DESCRIPTION_GENERATIONAL);
}

/**
 * Gencon can run in a split-heap virtual memory heap so see if the options for that have been set here or defer to our super
 */
MM_Heap *
MM_ConfigurationGenerational::createHeapWithManager(MM_EnvironmentBase *env, UDATA heapBytesRequested, MM_HeapRegionManager *regionManager)
{
	MM_GCExtensionsBase *extensions = env->getExtensions();
	MM_Heap *heap = NULL;

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* gencon supports split heaps so check that flag here when deciding what kind of MM_Heap to create */
	if (extensions->enableSplitHeap) {
		UDATA lowSize = extensions->oldSpaceSize;
		UDATA highSize = extensions->newSpaceSize;
		Assert_MM_true((lowSize + highSize) == heapBytesRequested);
		heap = MM_HeapSplit::newInstance(env, extensions->heapAlignment, lowSize, highSize, regionManager);
		/* FUTURE:  if this heap is NULL, we could try again with the arguments in the other order but that would require that we
		 * set and "inverted" flag in the extensions and check that in the PSA attach code when determining attachment policy.
		 * May allow more versatile use cases of the split heaps, though.
		 */
	} else
#endif /* OMR_GC_MODRON_SCAVENGER */
	{
		heap = MM_ConfigurationStandard::createHeapWithManager(env, heapBytesRequested, regionManager);
	}
	return heap;
}

bool 
MM_ConfigurationGenerational::initializeEnvironment(MM_EnvironmentBase *env)
{
	if (!MM_ConfigurationStandard::initializeEnvironment(env)) {
		return false;
	}
	
	return true;
}

void 
MM_ConfigurationGenerational::defaultMemorySpaceAllocated(MM_GCExtensionsBase *extensions, void* defaultMemorySpace)
{	
	MM_Configuration::defaultMemorySpaceAllocated(extensions, defaultMemorySpace);

	/* 
	 * Assume that the nursery is always at the top of the heap.
	 * Assume that the nursery was allocated with the expected minNewSpaceSize
	 */
	U_8* nurseryTop = (U_8*)extensions->getHeap()->getHeapTop();
	U_8* nurseryStart = nurseryTop - extensions->minNewSpaceSize;
	
	/* we can guarantee that the nursery is the topmost object space */
	extensions->setGuaranteedNurseryRange(nurseryStart, (void*)UDATA_MAX);
}

#endif /* defined(OMR_GC_MODRON_SCAVENGER) */
