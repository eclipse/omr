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


#if !defined(PHYSICALSUBARENA_HPP_)
#define PHYSICALSUBARENA_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "BaseVirtual.hpp"
#include "Heap.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_HeapRegionManager;
class MM_MemorySubSpace;
class MM_PhysicalArena;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_PhysicalSubArena : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
private:

protected:
	MM_PhysicalArena *_parent;
	MM_MemorySubSpace *_subSpace;

	MM_Heap *_heap;

	bool _resizable;  /**< Flag whether the instance can be resized (expanded/contracted) */

public:
	
/*
 * Function members
 */
private:

protected:
	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

	/* a helper to get the root region manager from the heap, since it was needed in so many place */
	MM_HeapRegionManager *getHeapRegionManager() {return _heap->getHeapRegionManager();}

public:
	virtual void kill(MM_EnvironmentBase *env) = 0;

	virtual bool inflate(MM_EnvironmentBase *env) = 0;

	virtual uintptr_t expand(MM_EnvironmentBase *env, uintptr_t expandSize) = 0;
	virtual uintptr_t expandNoCheck(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual bool canExpand(MM_EnvironmentBase *env);
	virtual uintptr_t checkCounterBalanceExpand(MM_EnvironmentBase *env, uintptr_t expandSizeDeltaAlignment, uintptr_t expandSize);

	virtual uintptr_t contract(MM_EnvironmentBase *env, uintptr_t contractSize);
	virtual bool canContract(MM_EnvironmentBase *env);

	MMINLINE MM_MemorySubSpace *getSubSpace() { return _subSpace; }
	MMINLINE void setSubSpace(MM_MemorySubSpace *subSpace) { _subSpace = subSpace; }

	MMINLINE void setParent(MM_PhysicalArena *parent) { _parent = parent; }

	virtual void tilt(MM_EnvironmentBase *env, uintptr_t allocateSpaceSize, uintptr_t survivorSpaceSize);
	virtual void tilt(MM_EnvironmentBase *env, uintptr_t survivorSpaceSizeRequest);

	virtual uintptr_t getAvailableContractionSize(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace, MM_AllocateDescription *allocDescription);

	/**
	 * Set whether resizing is available on the receiver and return the previous value.
	 * @return the previous value of the resizable flag.
	 */
	MMINLINE bool setResizable(bool resizable) {
		bool prev;
		prev = _resizable;
		_resizable = resizable;
		return prev;
	}

	MM_PhysicalSubArena(MM_Heap *heap) :
		MM_BaseVirtual(),
		_parent(NULL),
		_subSpace(NULL),
		_heap(heap),
		_resizable(true)
	{
		_typeId = __FUNCTION__;
	}
};

#endif /* PHYSICALSUBARENA_HPP_ */
