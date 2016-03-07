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
 ******************************************************************************/

#if !defined(HEAPVIRTUALMEMORY_HPP_)
#define HEAPVIRTUALMEMORY_HPP_

#include "omrcomp.h"

#include "Heap.hpp"
#include "MemoryHandle.hpp"

class MM_EnvironmentBase;
class MM_HeapRegionManager;
class MM_MemorySubSpace;
class MM_PhysicalArena;

class MM_HeapVirtualMemory : public MM_Heap {
protected:
	MM_MemoryHandle _vmemHandle;
	uintptr_t _heapAlignment;

	MM_PhysicalArena* _physicalArena;

private:
protected:
	bool initialize(MM_EnvironmentBase* env, uintptr_t size);
	void tearDown(MM_EnvironmentBase* env);

public:
	static MM_HeapVirtualMemory* newInstance(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t size, MM_HeapRegionManager* regionManager);
	virtual void kill(MM_EnvironmentBase* env);
	
	/**< returning pointer to our vmem handle (have to specify a pointer to const, to ensure caller will not modify it) */
	const MM_MemoryHandle *getVmemHandle() { return &_vmemHandle; }

	virtual uintptr_t getPageSize();
	virtual uintptr_t getPageFlags();
	virtual void* getHeapBase();
	virtual void* getHeapTop();

	virtual uintptr_t getMaximumPhysicalRange();

	virtual bool attachArena(MM_EnvironmentBase* env, MM_PhysicalArena* arena, uintptr_t size);
	virtual void detachArena(MM_EnvironmentBase* env, MM_PhysicalArena* arena);

	virtual bool commitMemory(void* address, uintptr_t size);
	virtual bool decommitMemory(void* address, uintptr_t size, void* lowValidAddress, void* highValidAddress);

	virtual uintptr_t calculateOffsetFromHeapBase(void* address);

	virtual bool heapAddRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress);
	virtual bool heapRemoveRange(MM_EnvironmentBase* env, MM_MemorySubSpace* subspace, uintptr_t size, void* lowAddress, void* highAddress, void* lowValidAddress, void* highValidAddress);
	virtual bool initializeHeapRegionManager(MM_EnvironmentBase* env, MM_HeapRegionManager* manager);

	/**
	 * Create a Heap object.
	 */
	MM_HeapVirtualMemory(MM_EnvironmentBase* env, uintptr_t heapAlignment, uintptr_t maximumMemorySize, MM_HeapRegionManager* regionManager)
		: MM_Heap(env, maximumMemorySize, regionManager)
		, _vmemHandle()
		, _heapAlignment(heapAlignment)
		, _physicalArena(NULL)
	{
		_typeId = __FUNCTION__;
	}

	/* Split heap needs access to the _vmem instance for region manager initialization */
	friend class MM_HeapSplit;
};

#endif /* HEAPVIRTUALMEMORY_HPP_ */
