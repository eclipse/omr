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

#if !defined(HEAPSPLIT_HPP_)
#define HEAPSPLIT_HPP_

#include "omrcomp.h"

#include "Heap.hpp"

class MM_EnvironmentBase;
class MM_HeapRegionManager;
class MM_HeapVirtualMemory;
class MM_MemorySubSpace;
class MM_PhysicalArena;

#if defined(OMR_GC_MODRON_SCAVENGER)

class MM_HeapSplit : public MM_Heap
{
	/* Data Members */
public:
protected:
private:
	MM_HeapVirtualMemory *_lowExtent; /**< the sub-heap which represents the committed low range of the heap (old space) */
	MM_HeapVirtualMemory *_highExtent; /**< the sub-heap which represents the committed high range of the heap (new space) */
	
	/* Member Functions */
public:
	static MM_HeapSplit *newInstance(MM_EnvironmentBase *env, uintptr_t heapAlignment, uintptr_t lowExtentSize, uintptr_t highExtentSize, MM_HeapRegionManager *regionManager);
	virtual void kill(MM_EnvironmentBase *env);
	/**
	 * Create a Heap object.
	 */
	MM_HeapSplit(MM_EnvironmentBase *env, uintptr_t lowExtentSize, uintptr_t highExtentSize, MM_HeapRegionManager *regionManager) :
		MM_Heap(env, lowExtentSize + highExtentSize, regionManager)
	{
		_typeId = __FUNCTION__;
	};
	
	virtual uintptr_t getPageSize();
	virtual uintptr_t getPageFlags();
	virtual void *getHeapBase();
	virtual void *getHeapTop();

	virtual uintptr_t getMaximumPhysicalRange();

	virtual bool attachArena(MM_EnvironmentBase *env, MM_PhysicalArena *arena, uintptr_t size);
	virtual void detachArena(MM_EnvironmentBase *env, MM_PhysicalArena *arena);

	virtual bool commitMemory(void *address, uintptr_t size);
	virtual bool decommitMemory(void *address, uintptr_t size, void *lowValidAddress, void *highValidAddress);
	
	virtual uintptr_t calculateOffsetFromHeapBase(void *address);
	
	virtual bool initializeHeapRegionManager(MM_EnvironmentBase *env, MM_HeapRegionManager *manager);
	virtual bool objectIsInGap(void *object);
protected:
	bool initialize(MM_EnvironmentBase *env, uintptr_t heapAlignment, uintptr_t lowExtentSize, uintptr_t highExtentSize, MM_HeapRegionManager *regionManager);
	void tearDown(MM_EnvironmentBase *env);
private:
};

#endif /* OMR_GC_MODRON_SCAVENGER */

#endif /* HEAPSPLIT_HPP_ */

