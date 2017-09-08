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


/**
 * @file
 * @ingroup GC_Modron_Standard
 */

#if !defined(PHYSICALSUBARENAVIRTUALMEMORYFLAT_HPP_)
#define PHYSICALSUBARENAVIRTUALMEMORYFLAT_HPP_

#include "PhysicalSubArenaVirtualMemory.hpp"

class MM_AllocateDescription;
class MM_EnvironmentBase;
class MM_HeapRegionDescriptor;
class MM_MemorySubSpace;
class MM_PhysicalArena;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Standard
 */
class MM_PhysicalSubArenaVirtualMemoryFlat : public MM_PhysicalSubArenaVirtualMemory
{
private:
protected:
	MM_HeapRegionDescriptor *_region;

	virtual bool initialize(MM_EnvironmentBase *env);
	virtual void tearDown(MM_EnvironmentBase *env);

public:
	static MM_PhysicalSubArenaVirtualMemoryFlat *newInstance(MM_EnvironmentBase *env, MM_Heap *heap);
	virtual void kill(MM_EnvironmentBase *env);

	virtual bool inflate(MM_EnvironmentBase *env);

	virtual uintptr_t expand(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t expandNoCheck(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual uintptr_t checkCounterBalanceExpand(MM_EnvironmentBase *env, uintptr_t expandSizeDeltaAlignment, uintptr_t expandSize);

	virtual uintptr_t contract(MM_EnvironmentBase *env, uintptr_t expandSize);
	virtual bool canContract(MM_EnvironmentBase *env);

	virtual uintptr_t getAvailableContractionSize(MM_EnvironmentBase *env, MM_MemorySubSpace *memorySubSpace, MM_AllocateDescription *allocDescription);

	MM_PhysicalSubArenaVirtualMemoryFlat(MM_Heap *heap) :
		MM_PhysicalSubArenaVirtualMemory(heap),
		_region(NULL)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* PHYSICALSUBARENAVIRTUALMEMORYFLAT_HPP_ */
