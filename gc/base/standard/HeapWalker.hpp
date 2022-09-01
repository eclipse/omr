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
