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


#include "MemorySpace.hpp"

#include "omrport.h"

#include "EnvironmentBase.hpp"
#include "Forge.hpp"
#include "Heap.hpp"
#include "MemorySubSpace.hpp"
#include "PhysicalArena.hpp"

/**
 * Create a new MemorySpace
 * @note Certain subclasses of this exist, to provide specialised behaviour (eg scoped memory).
 * Ensure you are instantiating the correct class.
 */
MM_MemorySpace *
MM_MemorySpace::newInstance(MM_EnvironmentBase *env, MM_Heap *heap, MM_PhysicalArena *physicalArena, MM_MemorySubSpace *memorySubSpace, MM_InitializationParameters *parameters, const char *name, const char *description)
{
	MM_MemorySpace *memorySpace = (MM_MemorySpace *)env->getForge()->allocate(sizeof(MM_MemorySpace), MM_AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (memorySpace) {
		new(memorySpace) MM_MemorySpace(heap, physicalArena, parameters, name, description);
		if (!memorySpace->initialize(env, memorySubSpace)) {
			memorySpace->kill(env);
			memorySpace = NULL;
		}
	}
	return memorySpace;
}

/**
 * @todo Provide function documentation
 */
void
MM_MemorySpace::kill(MM_EnvironmentBase *env)
{
	tearDown(env);
	env->getForge()->free(this);
}

/**
 * Initialize the MemorySpace internal structure/variables.
 * @return true if the initialization is successful, false otherwise.
 */
bool
MM_MemorySpace::initialize(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace)
{
	_heap->registerMemorySpace(this);
	
	registerMemorySubSpace(memorySubSpace);
	
	if(NULL != _physicalArena) {
		_physicalArena->setMemorySpace(this);
	}

	setDefaultMemorySubSpace(memorySubSpace->getDefaultMemorySubSpace());
	setTenureMemorySubSpace(memorySubSpace->getTenureMemorySubSpace());

	return true;
}

/* TODO: IOP: delete me */
bool
MM_MemorySpace::initialize(MM_EnvironmentBase *env)
{
	_heap->registerMemorySpace(this);

	return true;
}

/**
 * Destroy/free any internal structure.
 */
void
MM_MemorySpace::tearDown(MM_EnvironmentBase *env)
{
	MM_MemorySubSpace *memorySubSpaceCurrent, *memorySubSpaceNext;

	memorySubSpaceCurrent = getMemorySubSpaceList();
	while(NULL != memorySubSpaceCurrent) {
		memorySubSpaceNext = memorySubSpaceCurrent->getNext();
		memorySubSpaceCurrent->kill(env);
		memorySubSpaceCurrent = memorySubSpaceNext;
	}

	setDefaultMemorySubSpace(NULL);
	setTenureMemorySubSpace(NULL);

	if(NULL != _physicalArena) {
		_physicalArena->kill(env);
		_physicalArena = NULL;
 	}
 	
	_heap->unregisterMemorySpace(this);
}

/**
 * Register a child subspace
 */
void
MM_MemorySpace::registerMemorySubSpace(MM_MemorySubSpace *memorySubSpace)
{
	memorySubSpace->setMemorySpace(this);
	
	memorySubSpace->setParent(NULL);
	
	if (_memorySubSpaceList) {
		_memorySubSpaceList->setPrevious(memorySubSpace);
	}
	memorySubSpace->setNext(_memorySubSpaceList);
	memorySubSpace->setPrevious(NULL);
	_memorySubSpaceList = memorySubSpace;
}

/**
 * Unregister a child subspace
 */
void
MM_MemorySpace::unregisterMemorySubSpace(MM_MemorySubSpace *memorySubSpace)
{
	MM_MemorySubSpace *previous, *next;
	previous = memorySubSpace->getPrevious();
	next = memorySubSpace->getNext();

	if(previous) {
		previous->setNext(next);
	} else {
		_memorySubSpaceList = next;
	}

	if(next) {
		next->setPrevious(previous);
	}
}

/**
 * Get the sum of all free memory currently available for allocation in this memory space.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider defered work that may be done to increase current free memory stores.
 * @see getApproximateFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_MemorySpace::getActualFreeMemorySize()
{
	MM_MemorySubSpace *currentMemorySubSpace;
	uintptr_t freeMemory;

	currentMemorySubSpace = _memorySubSpaceList;
	freeMemory = 0;
	while(currentMemorySubSpace) {
		freeMemory += currentMemorySubSpace->getActualFreeMemorySize();
		currentMemorySubSpace = currentMemorySubSpace->getNext();
	}
	return freeMemory;
}

/**
 * Get the approximate sum of all free memory available for allocation in this memory space.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * @see getActualFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_MemorySpace::getApproximateFreeMemorySize()
{
	MM_MemorySubSpace *currentMemorySubSpace;
	uintptr_t freeMemory;

	currentMemorySubSpace = _memorySubSpaceList;
	freeMemory = 0;
	while(currentMemorySubSpace) {
		freeMemory += currentMemorySubSpace->getApproximateFreeMemorySize();
		currentMemorySubSpace = currentMemorySubSpace->getNext();
	}
	return freeMemory;
}

uintptr_t
MM_MemorySpace::getActiveMemorySize()
{
	return getActiveMemorySize(getAllTypeFlags());
}

uintptr_t
MM_MemorySpace::getActiveMemorySize(uintptr_t includeMemoryType)
{
   MM_MemorySubSpace *currentMemorySubSpace;
   uintptr_t memory;

   currentMemorySubSpace = _memorySubSpaceList;
   memory = 0;
   while(currentMemorySubSpace) {
      memory += currentMemorySubSpace->getActiveMemorySize(includeMemoryType);
      currentMemorySubSpace = currentMemorySubSpace->getNext();
   }
   return memory;
}

uintptr_t
MM_MemorySpace::getActiveLOAMemorySize(uintptr_t includeMemoryType)
{
   MM_MemorySubSpace *currentMemorySubSpace;
   uintptr_t memory;

   currentMemorySubSpace = _memorySubSpaceList;
   memory = 0;
   while(currentMemorySubSpace) {
      memory += currentMemorySubSpace->getActiveLOAMemorySize(includeMemoryType);
      currentMemorySubSpace = currentMemorySubSpace->getNext();
   }
   return memory;
}

uintptr_t
MM_MemorySpace::getActiveSurvivorMemorySize(uintptr_t includeMemoryType)
{
   MM_MemorySubSpace *currentMemorySubSpace;
   uintptr_t memory;

   currentMemorySubSpace = _memorySubSpaceList;
   memory = 0;
   while(currentMemorySubSpace) {
      memory += currentMemorySubSpace->getActiveSurvivorMemorySize(includeMemoryType);
      currentMemorySubSpace = currentMemorySubSpace->getNext();
   }
   return memory;
}

uintptr_t
MM_MemorySpace::getApproximateActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType)
{
   MM_MemorySubSpace *currentMemorySubSpace;
   uintptr_t memory;

   currentMemorySubSpace = _memorySubSpaceList;
   memory = 0;
   while(currentMemorySubSpace) {
      memory += currentMemorySubSpace->getApproximateActiveFreeSurvivorMemorySize(includeMemoryType);
      currentMemorySubSpace = currentMemorySubSpace->getNext();
   }
   return memory;
}

/**
 * Get the sum of all free memory currently available for allocation in this memory space.
 * This call will return an accurate count of the current size of all free memory.  It will not
 * consider defered work that may be done to increase current free memory stores.
 * @see getApproximateActiveFreeMemorySize()
 * @return the total free memory currently available for allocation.
 */
uintptr_t
MM_MemorySpace::getActualActiveFreeMemorySize()
{
	return getActualActiveFreeMemorySize(getAllTypeFlags());
}

/**
 * Get the sum of all free memory currently available for allocation in all memory subspace of the specified type.
 * This call will return an accurate count of the current size of all free memory of the specified type.  It will not
 * consider defered work that may be done to increase current free memory stores.
 *
 * @see getApproximateActiveFreeMemorySize(uintptr_t)
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_MemorySpace::getActualActiveFreeMemorySize(uintptr_t includeMemoryType)
{
   MM_MemorySubSpace *currentMemorySubSpace;
   uintptr_t freeMemory;

   currentMemorySubSpace = _memorySubSpaceList;
   freeMemory = 0;
   while(currentMemorySubSpace) {
      freeMemory += currentMemorySubSpace->getActualActiveFreeMemorySize(includeMemoryType);
      currentMemorySubSpace = currentMemorySubSpace->getNext();
   }
   return freeMemory;
}

/**
 * Get the approximate sum of all free memory available for allocation in this memory space.
 * This call will return an estimated count of the current size of all free memory.  Although this
 * estimate may be accurate, it will consider potential defered work that may be done to increase current
 * free memory stores.
 * @see getActualActiveFreeMemorySize()
 * @return the approximate total free memory available for allocation.
 */
uintptr_t
MM_MemorySpace::getApproximateActiveFreeMemorySize()
{
	return getApproximateActiveFreeMemorySize(getAllTypeFlags());
}

/**
 * Get the sum of all free memory currently available for allocation in all memory subspace of the specified type.
 * This call will return an accurate count of the current size of all free memory of the specified type.  It will not
 * consider defered work that may be done to increase current free memory stores.
 *
 * @see getApproximateActiveFreeMemorySize(uintptr_t)
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_MemorySpace::getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType)
{
   MM_MemorySubSpace *currentMemorySubSpace;
   uintptr_t freeMemory;

   currentMemorySubSpace = _memorySubSpaceList;
   freeMemory = 0;
   while(currentMemorySubSpace) {
      freeMemory += currentMemorySubSpace->getApproximateActiveFreeMemorySize(includeMemoryType);
      currentMemorySubSpace = currentMemorySubSpace->getNext();
   }
   return freeMemory;
}

/**
 * Get the sum of all free LOA memory currently available for allocation in all memory subspace of the specified type.
 * This call will return an accurate count of the current size of all free memory of the specified type.  It will not
 * consider defered work that may be done to increase current free memory stores.
 *
 * @param includeMemoryType memory subspace types to consider in the calculation.
 * @return the total free memory currently available for allocation from subspaces of the specified type.
 */
uintptr_t
MM_MemorySpace::getApproximateActiveFreeLOAMemorySize(uintptr_t includeMemoryType)
{
   MM_MemorySubSpace *currentMemorySubSpace;
   uintptr_t freeMemory;

   currentMemorySubSpace = _memorySubSpaceList;
   freeMemory = 0;
   while(currentMemorySubSpace) {
      freeMemory += currentMemorySubSpace->getApproximateActiveFreeLOAMemorySize(includeMemoryType);
      currentMemorySubSpace = currentMemorySubSpace->getNext();
   }
   return freeMemory;
}

void
MM_MemorySpace::mergeHeapStats(MM_HeapStats *heapStats)
{
	mergeHeapStats(heapStats, (MEMORY_TYPE_OLD|MEMORY_TYPE_NEW));
}

void
MM_MemorySpace::mergeHeapStats(MM_HeapStats *heapStats, uintptr_t includeMemoryType)
{
	MM_MemorySubSpace *currentMemorySubSpace;
		
	currentMemorySubSpace = _memorySubSpaceList;
	while(currentMemorySubSpace) {
		currentMemorySubSpace->mergeHeapStats(heapStats, includeMemoryType);
		currentMemorySubSpace = currentMemorySubSpace->getNext();
	}
}

void
MM_MemorySpace::resetHeapStatistics(bool globalCollect)
{
	MM_MemorySubSpace *currentMemorySubSpace;

	currentMemorySubSpace = _memorySubSpaceList;
	while(currentMemorySubSpace) {
		currentMemorySubSpace->resetHeapStatistics(globalCollect);
		currentMemorySubSpace = currentMemorySubSpace->getNext();
	}
}

void
MM_MemorySpace::systemGarbageCollect(MM_EnvironmentBase *env, uint32_t gcCode)
{
	getTenureMemorySubSpace()->systemGarbageCollect(env, gcCode);
}

void
MM_MemorySpace::localGarbageCollect(MM_EnvironmentBase *env)
{
	getDefaultMemorySubSpace()->systemGarbageCollect(env, J9MMCONSTANT_IMPLICIT_GC_DEFAULT);
}

bool
MM_MemorySpace::inflate(MM_EnvironmentBase *env)
{
	bool result;
	MM_MemorySubSpace *mm_memorysubspace;

	if(_physicalArena && !_physicalArena->inflate(env)) {
		return false;
	}

	result = true;
	mm_memorysubspace  = _memorySubSpaceList;
	while(result && (NULL != mm_memorysubspace)) {
		result = mm_memorysubspace->inflate(env);
		mm_memorysubspace = mm_memorysubspace->getNext();
	}

	return result;
}

/**
 * Determine if the receiver will allow the expansion request size.
 * @return true if the expand size fits into the receivers limits, false otherwise.
 */
bool
MM_MemorySpace::canExpand(MM_EnvironmentBase *env, uintptr_t expandSize)
{
	return ((expandSize <= _maximumSize) && (_currentSize <= (_maximumSize - expandSize)));
}

/**
 * Determine the maximum possible amount by which we can expand the memory space
 * @return the amount by which the receiver can expand
 */
uintptr_t
MM_MemorySpace::maxExpansion(MM_EnvironmentBase *env)
{
	return  (uintptr_t)(_maximumSize - _currentSize);
}

/**
 * Reset the largest free chunk of all memorySubSpaces to 0.
 */
void
MM_MemorySpace::resetLargestFreeEntry()
{
	MM_MemorySubSpace *currentMemorySubSpace = _memorySubSpaceList;
	while(currentMemorySubSpace) {
		currentMemorySubSpace->resetLargestFreeEntry();
		currentMemorySubSpace = currentMemorySubSpace->getNext();
	}
}

/**
 * Return the largest free chunk of all memorySubSpaces that satisifies the allocateDescription
 * found during Sweep phase of global collect.
 * @note No check is made to verify the allocateDescription request is from the current memory space.
 */
uintptr_t
MM_MemorySpace::findLargestFreeEntry(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription)
{
	uintptr_t largestFreeEntry = 0;
	uintptr_t maximumLargestFreeEntry = 0;

	/* Record largest entry that satsifies allocateDescription */
	MM_MemorySubSpace *currentMemorySubSpace = _memorySubSpaceList;
	while(currentMemorySubSpace) {
		largestFreeEntry = currentMemorySubSpace->findLargestFreeEntry(env, allocateDescription);
		if (largestFreeEntry > maximumLargestFreeEntry) {
			maximumLargestFreeEntry = largestFreeEntry;
		}
		currentMemorySubSpace = currentMemorySubSpace->getNext();
	}
	return maximumLargestFreeEntry;
}

/**
 * The heap has added a range of memory associated to the receiver or one of its children.
 * @note The low address is inclusive, the high address exclusive.
 */
bool
MM_MemorySpace::heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress)
{
	_currentSize += size;
	return _heap->heapAddRange(env, subspace, size, lowAddress, highAddress);
}

/**
 * The heap has removed a range of memory associated to the receiver or one of its children.
 * @note The low address is inclusive, the high address exclusive.
 * @param lowValidAddress The first valid address previous to the lowest in the heap range being removed
 * @param highValidAddress The first valid address following the highest in the heap range being removed
 * 
 */
bool
MM_MemorySpace::heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddres, void *highValidAddress)
{
	_currentSize -= size;
	return _heap->heapRemoveRange(env, subspace, size, lowAddress, highAddress, lowValidAddres, highValidAddress);
}

/**
 * Re-size all structures which are dependent on the current size of the heap.
 * No new memory has been added to a heap reconfiguration.  This call typically is the result
 * of having segment range changes (memory redistributed between segments) or the meaning of
 * memory changed.
 * 
 */
void
MM_MemorySpace::heapReconfigured(MM_EnvironmentBase *env)
{
	_heap->heapReconfigured(env);
}

/**
 * Determine if the receiver will allow the contraction request size.
 * @return true if the contract size fits into the receivers limits, false otherwise.
 */
bool
MM_MemorySpace::canContract(MM_EnvironmentBase *env, uintptr_t contractSize)
{
	return ((contractSize <= _currentSize) && (_minimumSize <= (_currentSize - contractSize)));
}

/**
 * Determine the maximum possible amount by which we can contract the memory space
 * @return the amount by which the receiver can expand
 */
uintptr_t
MM_MemorySpace::maxContraction(MM_EnvironmentBase *env)
{
	return  (uintptr_t)(_currentSize - _minimumSize);
}

/**
 * Reset the free list and associated information of all subspaces assocated with the receiver.
 */
void
MM_MemorySpace::reset(MM_EnvironmentBase *env)
{
	MM_MemorySubSpace *memorySubSpace;
	memorySubSpace = _memorySubSpaceList;
	while(NULL != memorySubSpace) {
		memorySubSpace->reset();
		memorySubSpace = memorySubSpace->getNext();
	}
}

/**
 * Rebuild the free list and associated information of all subspaces assocated with the receiver.
 * As opposed to reset, which will empty out, this will fill out as if everything is free
 */
void
MM_MemorySpace::rebuildFreeList(MM_EnvironmentBase *env)
{
	MM_MemorySubSpace *memorySubSpace;
	memorySubSpace = _memorySubSpaceList;
	while(NULL != memorySubSpace) {
		memorySubSpace->rebuildFreeList(env);
		memorySubSpace = memorySubSpace->getNext();
	}
}

/**
 * Return the bitwise OR of all possible memory types controlled by this memory space.
 */
uintptr_t 
MM_MemorySpace::getAllTypeFlags()
{
	return MEMORY_TYPE_OLD | MEMORY_TYPE_NEW;
}
