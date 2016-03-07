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


#if !defined(MEMORYSPACE_HPP_)
#define MEMORYSPACE_HPP_

#include "omrcomp.h"
#include "j9nongenerated.h"
#include "modronbase.h"

#include "BaseVirtual.hpp"
#include "InitializationParameters.hpp"
#include "MemorySpacesAPI.h"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_Heap;
class MM_HeapStats;
class MM_MemorySubSpace;
class MM_PhysicalArena;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_MemorySpace : public MM_BaseVirtual
{
	friend class GC_CheckEngine;
/*
 * Data members
 */
private:
	MM_MemorySpace *_next;
	MM_MemorySpace *_previous;
	uintptr_t _initialSize;
	uintptr_t _minimumSize;
	uintptr_t _currentSize;
	uintptr_t _maximumSize;

	MM_Heap *_heap;

	MM_MemorySubSpace *_defaultMemorySubSpace;
	MM_MemorySubSpace *_tenureMemorySubSpace;
	MM_MemorySubSpace *_memorySubSpaceList;
	
protected:
	MM_PhysicalArena *_physicalArena;
	
	const char *_name;
	const char *_description;
	uint64_t _uniqueID;

public:
	
/*
 * Function members
 */
private:
	
protected:
	
	virtual uintptr_t getAllTypeFlags();

	bool initialize(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace);
	/* TODO: IOP: delete me */
	bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

public:
	const char *
	getName()
	{
		const char *name = (const char *)_name;
		if (NULL == name) {
			name = MEMORY_SPACE_NAME_UNDEFINED;
		}
		return name;
	}
	void setName(const char *name) { _name = name; }
	virtual const char *getDescription() { return _description ? _description : MEMORY_SPACE_DESCRIPTION_UNDEFINED; }
	virtual void setDescription(const char *description) { _description = description; }
	MMINLINE uint64_t getUniqueID() { return _uniqueID; }
	MMINLINE void setUniqueID(uint64_t uniqueID) { _uniqueID = uniqueID; }

	static MM_MemorySpace *newInstance(MM_EnvironmentBase *env, MM_Heap *heap, MM_PhysicalArena *physicalArena, MM_MemorySubSpace *memorySubSpace, MM_InitializationParameters *parameters, const char *name, const char *description);
	virtual void kill(MM_EnvironmentBase *env);

	MMINLINE MM_MemorySpace *getNext() { return _next; }
	MMINLINE MM_MemorySpace *getPrevious() { return _previous; }
	MMINLINE void setNext(MM_MemorySpace *next) { _next = next; }
	MMINLINE void setPrevious(MM_MemorySpace *previous) { _previous = previous; }
	
	virtual uintptr_t getDepth() const { return 0; }
	virtual void setDepth(MM_EnvironmentBase *env, uintptr_t depth) {}
	virtual uintptr_t *getBaseAddress() { return NULL; }
	virtual uintptr_t *setBaseAddress(uintptr_t *baseAddress) { return NULL; }		
	virtual void cleanJVMTIReferences(MM_EnvironmentBase *env) { return; }
	virtual void resetFirstUnfinalized(MM_EnvironmentBase *env) { return; }

	MMINLINE uintptr_t getInitialSize() { return _initialSize; }
	MMINLINE uintptr_t getMinimumSize() { return _minimumSize; }
	MMINLINE uintptr_t getCurrentSize() { return _currentSize; }
	MMINLINE uintptr_t getMaximumSize() { return _maximumSize; }
	uintptr_t getActualFreeMemorySize();
	uintptr_t getApproximateFreeMemorySize();
	
	uintptr_t getActiveMemorySize();
	uintptr_t getActualActiveFreeMemorySize();
	uintptr_t getApproximateActiveFreeMemorySize();
	
	uintptr_t getActiveMemorySize(uintptr_t includeMemoryType);
	uintptr_t getActualActiveFreeMemorySize(uintptr_t includeMemoryType);
	uintptr_t getApproximateActiveFreeMemorySize(uintptr_t includeMemoryType);
	
	uintptr_t getActiveLOAMemorySize(uintptr_t includeMemoryType);
	uintptr_t getApproximateActiveFreeLOAMemorySize(uintptr_t includeMemoryType);
	
	uintptr_t getActiveSurvivorMemorySize(uintptr_t includeMemoryType);
	uintptr_t getApproximateActiveFreeSurvivorMemorySize(uintptr_t includeMemoryType);

	void mergeHeapStats(MM_HeapStats *heapStats);
	void mergeHeapStats(MM_HeapStats *heapStats, uintptr_t includeMemoryType);
	void resetHeapStatistics(bool globalCollect);

	MMINLINE MM_Heap *getHeap() { return _heap; }

	MMINLINE MM_MemorySubSpace *getDefaultMemorySubSpace() { return _defaultMemorySubSpace; }
	MMINLINE MM_MemorySubSpace *getTenureMemorySubSpace() { return _tenureMemorySubSpace; }
	MMINLINE void setDefaultMemorySubSpace(MM_MemorySubSpace *memorySpace) { _defaultMemorySubSpace = memorySpace; }
	MMINLINE void setTenureMemorySubSpace(MM_MemorySubSpace *memorySpace) { _tenureMemorySubSpace = memorySpace; }

	void registerMemorySubSpace(MM_MemorySubSpace *memorySubSpace);
	void unregisterMemorySubSpace(MM_MemorySubSpace *memorySubSpace);

	MMINLINE MM_MemorySubSpace *getMemorySubSpaceList() { return _memorySubSpaceList; }

	void systemGarbageCollect(MM_EnvironmentBase *env, uint32_t gcCode);
	void localGarbageCollect(MM_EnvironmentBase *env);
	
	void reset(MM_EnvironmentBase *env);
	void rebuildFreeList(MM_EnvironmentBase *env);

	virtual bool inflate(MM_EnvironmentBase *env);

	MMINLINE MM_PhysicalArena *getPhysicalArena() { return _physicalArena; }

	bool canExpand(MM_EnvironmentBase *env, uintptr_t expandSize);
	uintptr_t maxExpansion(MM_EnvironmentBase *env);

	void resetLargestFreeEntry();
	uintptr_t findLargestFreeEntry(MM_EnvironmentBase *env, MM_AllocateDescription *allocateDescription);

	bool canContract(MM_EnvironmentBase *env, uintptr_t contractSize);
	uintptr_t maxContraction(MM_EnvironmentBase *env);

	bool heapAddRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress);
	bool heapRemoveRange(MM_EnvironmentBase *env, MM_MemorySubSpace *subspace, uintptr_t size, void *lowAddress, void *highAddress, void *lowValidAddres, void *highValidAddress);
	/**
	 * Called after the heap geometry changes to allow any data structures dependent on this to be updated.
	 * This call could be triggered by memory ranges being added to or removed from the heap or memory being
	 * moved from one subspace to another.
	 * @param env[in] The thread which performed the change in heap geometry 
	 */
	void heapReconfigured(MM_EnvironmentBase *env);

	static MM_MemorySpace *getMemorySpace(void *memorySpace) { return (MM_MemorySpace *)memorySpace; }
	
	/**
	 * Create a MemorySpace object.
	 */
	MM_MemorySpace(MM_Heap *heap, MM_PhysicalArena *physicalArena, MM_InitializationParameters *parameters, const char *name, const char *description)
		: MM_BaseVirtual()
		, _next(NULL)
		, _previous(NULL)
		, _initialSize(parameters->_initialNewSpaceSize + parameters->_initialOldSpaceSize)
		, _minimumSize(parameters->_minimumSpaceSize)
		, _currentSize(0)
		, _maximumSize(parameters->_maximumSpaceSize)
		, _heap(heap)
		, _defaultMemorySubSpace(NULL)
		, _tenureMemorySubSpace(NULL)
		, _memorySubSpaceList(NULL)
		, _physicalArena(physicalArena)
		, _name(name)
		, _description(description)
		, _uniqueID(0)
	{
		_typeId = __FUNCTION__;
	}

};

#endif /* MEMORYSPACE_HPP_ */
