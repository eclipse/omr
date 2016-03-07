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
 * @ingroup GC_Modron_Standard
 */

#include "omrcfg.h"
#include "MemorySpacesAPI.h"

#if defined(OMR_GC_MODRON_STANDARD)

#include "ConfigurationFlat.hpp"

#include "EnvironmentBase.hpp"
#include "MemoryPoolAddressOrderedList.hpp"
#include "MemoryPoolLargeObjects.hpp"
#include "MemorySpace.hpp"
#include "MemorySubSpaceFlat.hpp"
#include "MemorySubSpaceGeneric.hpp"
#include "PhysicalArenaVirtualMemory.hpp"
#include "PhysicalSubArenaVirtualMemoryFlat.hpp"

MM_Configuration*
MM_ConfigurationFlat::newInstance(MM_EnvironmentBase* env, MM_ConfigurationLanguageInterface* configurationLanguageInterface)
{
	MM_ConfigurationFlat* configuration;

	configuration = (MM_ConfigurationFlat*)env->getForge()->allocate(sizeof(MM_ConfigurationFlat), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != configuration) {
		new (configuration) MM_ConfigurationFlat(env, configurationLanguageInterface);
		if (!configuration->initialize(env)) {
			configuration->kill(env);
			configuration = NULL;
		}
	}
	return configuration;
}

MM_MemorySpace*
MM_ConfigurationFlat::createDefaultMemorySpace(MM_EnvironmentBase* env, MM_Heap* heap, MM_InitializationParameters* parameters)
{
	MM_MemoryPool* memoryPool = NULL;
	MM_MemorySubSpaceGeneric* memorySubSpaceGeneric = NULL;
	MM_PhysicalSubArenaVirtualMemoryFlat* physicalSubArena = NULL;
	MM_MemorySubSpaceFlat* memorySubSpaceFlat = NULL;
	MM_PhysicalArenaVirtualMemory* physicalArena = NULL;

	if (NULL == (memoryPool = createMemoryPool(env, false))) {
		return NULL;
	}

	if (NULL == (memorySubSpaceGeneric = MM_MemorySubSpaceGeneric::newInstance(env, memoryPool, NULL, false, parameters->_minimumSpaceSize, parameters->_initialOldSpaceSize, parameters->_maximumSpaceSize, MEMORY_TYPE_OLD, 0))) {
		return NULL;
	}

	if (NULL == (physicalSubArena = MM_PhysicalSubArenaVirtualMemoryFlat::newInstance(env, heap))) {
		memorySubSpaceGeneric->kill(env);
		return NULL;
	}

	if (NULL == (memorySubSpaceFlat = MM_MemorySubSpaceFlat::newInstance(env, physicalSubArena, memorySubSpaceGeneric, true, parameters->_minimumSpaceSize, parameters->_initialOldSpaceSize, parameters->_maximumSpaceSize, MEMORY_TYPE_OLD, 0))) {
		return NULL;
	}

	if (NULL == (physicalArena = MM_PhysicalArenaVirtualMemory::newInstance(env, heap))) {
		memorySubSpaceFlat->kill(env);
		return NULL;
	}

	return MM_MemorySpace::newInstance(env, heap, physicalArena, memorySubSpaceFlat, parameters, MEMORY_SPACE_NAME_FLAT, MEMORY_SPACE_DESCRIPTION_FLAT);
}

#endif /* defined(OMR_GC_MODRON_STANDARD) */
