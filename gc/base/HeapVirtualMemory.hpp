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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

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
#if defined(OMR_GC_DOUBLE_MAP_ARRAYLETS)
	virtual void *doubleMapArraylet(MM_EnvironmentBase *env, uintptr_t* arrayletLeaves, UDATA arrayletLeafCount, UDATA arrayletLeafSize, UDATA byteAmount, struct J9PortVmemIdentifier *newIdentifier, UDATA pageSize);
#endif /* defined(OMR_GC_DOUBLE_MAP_ARRAYLETS) */

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
