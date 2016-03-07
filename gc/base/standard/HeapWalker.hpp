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

#if !defined(HEAP_WALKER_HPP_)
#define HEAP_WALKER_HPP_

#include "omr.h"
#include "omrcfg.h"
#include "objectdescription.h"

#include "BaseVirtual.hpp"

class MM_EnvironmentBase;
class MM_Heap;
class MM_HeapRegionDescriptor;
class MM_MemorySubSpace;

typedef void (*MM_HeapWalkerObjectFunc)(OMR_VMThread *, MM_HeapRegionDescriptor *, omrobjectptr_t, void *);
typedef void (*MM_HeapWalkerSlotFunc)(OMR_VM *, omrobjectptr_t *, void *, uint32_t);

class MM_HeapWalker : public MM_BaseVirtual
{
protected:

#if defined(OMR_GC_MODRON_SCAVENGER)
	void rememberedObjectSlotsDo(MM_EnvironmentBase *env, MM_HeapWalkerSlotFunc function, void *userData, uintptr_t walkFlags, bool parallel);
#endif /* OMR_GC_MODRON_SCAVENGER */
	bool initialize(MM_EnvironmentBase *env);

public:
	virtual void allObjectSlotsDo(MM_EnvironmentBase *env, MM_HeapWalkerSlotFunc function, void *userData, uintptr_t walkFlags, bool parallel, bool prepareHeapForWalk);
	virtual void allObjectsDo(MM_EnvironmentBase *env, MM_HeapWalkerObjectFunc function, void *userData, uintptr_t walkFlags, bool parallel, bool prepareHeapForWalk);

	static MM_HeapWalker *newInstance(MM_EnvironmentBase *env); 	
	virtual void kill(MM_EnvironmentBase *env);
	
	/**
	 * constructor of Heap Walker
	 */
	MM_HeapWalker()
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* HEAP_WALKER_HPP_ */
