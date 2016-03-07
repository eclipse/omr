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


#include "PhysicalArenaVirtualMemory.hpp"

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "HeapVirtualMemory.hpp"
#include "MemoryManager.hpp"
#include "MemorySpace.hpp"
#include "ModronAssertions.h"
#include "PhysicalSubArenaVirtualMemory.hpp"

/**
 * Instantiate a new MM_PhysicalArenaVirtualMemory object.
 */
MM_PhysicalArenaVirtualMemory*
MM_PhysicalArenaVirtualMemory::newInstance(MM_EnvironmentBase* env, MM_Heap* heap)
{
	MM_PhysicalArenaVirtualMemory* arena;

	arena = (MM_PhysicalArenaVirtualMemory*)env->getForge()->allocate(sizeof(MM_PhysicalArenaVirtualMemory), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (arena) {
		new (arena) MM_PhysicalArenaVirtualMemory(env, heap);
		if (!arena->initialize(env)) {
			arena->kill(env);
			return NULL;
		}
	}
	return arena;
}

bool
MM_PhysicalArenaVirtualMemory::initialize(MM_EnvironmentBase* env)
{
	if (!MM_PhysicalArena::initialize(env)) {
		return false;
	}

	return true;
}

/**
 * Remove a physical sub arena from the parent arena receiver.
 */
void
MM_PhysicalArenaVirtualMemory::detachSubArena(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena)
{
	MM_PhysicalSubArenaVirtualMemory* previous, *next;
	MM_PhysicalSubArenaVirtualMemory* currentSubArena = (MM_PhysicalSubArenaVirtualMemory*)subArena;

	previous = currentSubArena->getPreviousSubArena();
	next = currentSubArena->getNextSubArena();

	if (previous) {
		previous->setNextSubArena(next);
	} else {
		_physicalSubArena = next;
	}

	if (next) {
		next->setPreviousSubArena(previous);
	}
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
MM_PhysicalArenaVirtualMemory::attachSubArena(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, uintptr_t attachPolicy)
{
	void* candidateBase;
	MM_PhysicalSubArenaVirtualMemory* insertionHead, *insertionTail;
	MM_PhysicalSubArenaVirtualMemory* currentSubArena = (MM_PhysicalSubArenaVirtualMemory*)subArena;

	/* Sanity check of the size */
	if (_memorySpace->getMaximumSize() < size) {
		return false;
	}

	switch (attachPolicy) {
	case modron_pavm_attach_policy_none:
		/* Find the insertion point for the arena */
		candidateBase = getLowAddress();
		insertionHead = NULL;
		insertionTail = _physicalSubArena;
		while (insertionTail) {
			if ((((uintptr_t)insertionTail->getLowAddress()) - ((uintptr_t)candidateBase)) >= size) {
				break;
			}

			candidateBase = insertionTail->getHighAddress();

			insertionHead = insertionTail;
			insertionTail = insertionTail->getNextSubArena();
		}

		/* If we have reached the end of the arena list, check if there is room between the candidateBase
		 * and the end of virtual memory */
		if (!insertionTail) {
			if (calculateOffsetToHighAddress(candidateBase) < size) {
				return false;
			}
		}
		break;
	case modron_pavm_attach_policy_high_memory:
		/* Determine the base address of the new sub arena */
		candidateBase = (void*)(((uintptr_t)getHighAddress()) - size);

		/* Find the tail of the list of sub arenas associated with the receiver */
		insertionHead = NULL;
		insertionTail = _physicalSubArena;

		while (insertionTail) {
			insertionHead = insertionTail;
			insertionTail = insertionTail->getNextSubArena();
		}

		/* If there is already a sub arena attached, check if the new sub arena fits at the top */
		if (insertionHead) {
			if (candidateBase < insertionHead->getHighAddress()) {
				/* No room - fail */
				return false;
			}
		}
		break;
	default:
		/* Bogus attach policy */
		return false;
	}

	/* Connect the physical arena into the list at the appropriate point */
	currentSubArena->setPreviousSubArena(insertionHead);
	currentSubArena->setNextSubArena(insertionTail);

	if (insertionTail) {
		insertionTail->setPreviousSubArena(currentSubArena);
	}

	if (insertionHead) {
		insertionHead->setNextSubArena(currentSubArena);
	} else {
		_physicalSubArena = currentSubArena;
	}

	currentSubArena->setLowAddress(candidateBase);
	currentSubArena->setHighAddress((void*)(((uint8_t*)candidateBase) + size));

#if defined(OMR_GC_MODRON_SCAVENGER)
	/* now that address range is known, just before we commit, bind memory to NUMA node, if applicable */
	if (0 != currentSubArena->getNumaNode()) {
		MM_GCExtensionsBase *ext = env->getExtensions();
		uintptr_t j9NodeNumber = ext->_numaManager.getJ9NodeNumber(currentSubArena->getNumaNode());
		if (0 != j9NodeNumber) {
			if (!ext->memoryManager->setNumaAffinity(((MM_HeapVirtualMemory *)_heap)->getVmemHandle(), j9NodeNumber, (void*)candidateBase, size)) {
				return false;
			}
		}
	}
#endif /* OMR_GC_MODRON_SCAVENGER */

	/* Commit the subarena memory into existence */
	return _heap->commitMemory(candidateBase, size);
}

/**
 * Check if the expand address and size request from a child sub arena is valid.
 * @return true if the request is valid, false otherwise.
 */
bool
MM_PhysicalArenaVirtualMemory::canExpand(MM_EnvironmentBase* env, MM_PhysicalSubArenaVirtualMemory* subArena, void* expandAddress, uintptr_t expandSize)
{
	return (expandAddress >= getLowAddress()) && (expandAddress < getHighAddress()) && (calculateOffsetToHighAddress(expandAddress) >= expandSize);
}

/**
 * Determine how much the child sub arena can expand.
 * @return maximum amount sub areana can expand.
 */
uintptr_t
MM_PhysicalArenaVirtualMemory::maxExpansion(MM_EnvironmentBase* env, MM_PhysicalSubArenaVirtualMemory* subArena, void* expandAddress)
{
	return calculateOffsetToHighAddress(expandAddress);
}

/**
 * Determine what the maximum physical expansion allowable starting at the given address is.
 * This routine does not account for any internal sub arenas, but rather what the theoretical maximum expansion is.
 * The routine also assumes that the expansion is towards the high address.
 * @return The number of bytes that are theoretically possible to expand by starting at the given address.
 */
uintptr_t
MM_PhysicalArenaVirtualMemory::getPhysicalMaximumExpandSizeHigh(MM_EnvironmentBase* env, void* address)
{
	/* Sanity check if the given address is out of range */
	if (getHighAddress() < address) {
		return 0;
	}

	return calculateOffsetToHighAddress(address);
}

/**
 * Determine what the maximum physical contraction allowable starting at the given address is.
 * This routine does not account for any internal sub arenas, but rather what the theoretical maximum contraction is.
 * The routine also assumes that the contraction is towards the low address.
 * @return The number of bytes that are theoretically possible to contract by starting at the given address.
 */
uintptr_t
MM_PhysicalArenaVirtualMemory::getPhysicalMaximumContractSizeLow(MM_EnvironmentBase* env, void* address)
{
	/* Sanity check if the given address is out of range */
	if (getLowAddress() > address) {
		return 0;
	}

	return ((uintptr_t)address) - ((uintptr_t)getLowAddress());
}

/**
 * Attach and reserve address space for the receiver.
 * @return true if the successful, false otherwise.
 */
bool
MM_PhysicalArenaVirtualMemory::inflate(MM_EnvironmentBase* env)
{
	/* Attach to the virtual memory heap store */
	return _heap->attachArena(env, this, _memorySpace->getMaximumSize());
}

/**
 * Find the next valid address higher than the current physical arenas memory.
 * This routine is typically used for decommit purposes, to find the valid ranges surrounding a particular
 * address range.
 * @return the next highest valid range, or NULL if there is none.
 */
void*
MM_PhysicalArenaVirtualMemory::findAdjacentHighValidAddress(MM_EnvironmentBase* env)
{
	/* Is there a valid higher address? */
	MM_PhysicalArena* next = getNextArena();
	if (NULL == next) {
		return NULL;
	}

	/* There is - return its lowest address */
	return next->getLowAddress();
}
