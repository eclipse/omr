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
