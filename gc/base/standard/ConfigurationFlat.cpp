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
MM_ConfigurationFlat::newInstance(MM_EnvironmentBase* env)
{
	MM_ConfigurationFlat* configuration;

	configuration = (MM_ConfigurationFlat*)env->getForge()->allocate(sizeof(MM_ConfigurationFlat), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL != configuration) {
		new (configuration) MM_ConfigurationFlat(env);
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
