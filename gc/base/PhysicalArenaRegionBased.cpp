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


#include "PhysicalArenaRegionBased.hpp"

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Heap.hpp"
#include "HeapRegionManager.hpp"
#include "MemorySpace.hpp"
#include "PhysicalSubArenaRegionBased.hpp"
#include "PhysicalSubArenaVirtualMemory.hpp"

#include "PhysicalArenaRegionBased.hpp"

/**
 * Instantiate a new MM_PhysicalArenaRegionBased object.
 */
MM_PhysicalArenaRegionBased *
MM_PhysicalArenaRegionBased::newInstance(MM_EnvironmentBase *env, MM_Heap *heap)
{
	MM_PhysicalArenaRegionBased *arena = (MM_PhysicalArenaRegionBased *)env->getForge()->allocate(sizeof(MM_PhysicalArenaRegionBased), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (arena) {
		new(arena) MM_PhysicalArenaRegionBased(env, heap);
		if(!arena->initialize(env)) {
			arena->kill(env);
			return NULL;
		}
	}
	return arena;
}

bool
MM_PhysicalArenaRegionBased::initialize(MM_EnvironmentBase *env)
{
	if(!MM_PhysicalArena::initialize(env)) {
		return false;
	}
	return true;
}

/**
 * Remove a physical sub arena from the parent arena receiver.
 */
void
MM_PhysicalArenaRegionBased::detachSubArena(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena)
{
	Assert_MM_true(_physicalSubArena == (MM_PhysicalSubArenaRegionBased *)subArena);
	_physicalSubArena = NULL;
}

/**
 * Attach a physical subarena of the specified size to the parent arena receiver.
 * This reserves the address space within the receiver for the subarena, and connects the subarena to the list
 * of those associated to the receiver (in address order).  Commit the subarena memory.
 * 
 * @param subArena The physical sub arena to attach
 * @param size Maximum size the sub arena will have (also the total reserve size).
 * 
 * @return true if the subarena was attached successfully, false otherwise.
 */
bool
MM_PhysicalArenaRegionBased::attachSubArena(MM_EnvironmentBase *env, MM_PhysicalSubArena *subArena, uintptr_t size, uintptr_t attachPolicy)
{
	MM_PhysicalSubArenaRegionBased *subArenaToAttach = (MM_PhysicalSubArenaRegionBased *)subArena;
	MM_PhysicalSubArenaRegionBased *headSubArena = (MM_PhysicalSubArenaRegionBased *)_physicalSubArena;
	MM_PhysicalSubArenaRegionBased *currentSubArena = headSubArena;
	
	if (_memorySpace->getMaximumSize() < size) {
		return false;
	}
	
	uintptr_t regionSize = _heap->getHeapRegionManager()->getRegionSize();
	if (0 != (size % regionSize)) {
		return false;
	}
	
	while (NULL != currentSubArena) {
		if (currentSubArena == subArenaToAttach) {
			/* subArena already attached */
			return true;
		}
		
		currentSubArena = currentSubArena->getNextSubArena();
	}
	
	subArenaToAttach->setNextSubArena(headSubArena);
	_physicalSubArena = subArenaToAttach;
	
	uintptr_t expanded = _physicalSubArena->performExpand(env, size);
	return (size == expanded);
}

/**
 * Check if the expand address and size request from a child sub arena is valid.
 * @return true if the request is valid, false otherwise.
 */
bool
MM_PhysicalArenaRegionBased::canResize(MM_EnvironmentBase *env, MM_PhysicalSubArenaRegionBased *subArena, uintptr_t sizeDelta)
{
	/* ensure we are trying to expand by a region size multiple */
	uintptr_t regionSize = _heap->getHeapRegionManager()->getRegionSize();
	if (0 != (sizeDelta % regionSize)) {
		return false;
	}
	return true;
}

/**
 * Determine how much the child sub arena can expand.
 * @return maximum amount sub areana can expand.
 */
uintptr_t
MM_PhysicalArenaRegionBased::maxExpansion(MM_EnvironmentBase *env, MM_PhysicalSubArenaRegionBased *subArena)
{
	return _heap->getHeapRegionManager()->getRegionSize();
}

/**
 * Attach and reserve address space for the receiver.
 * @return true if the successful, false otherwise.
 */
bool
MM_PhysicalArenaRegionBased::inflate(MM_EnvironmentBase *env)
{
	/* Attach to the virtual memory heap store */
	return _heap->attachArena(env, this, _memorySpace->getMaximumSize());
}
