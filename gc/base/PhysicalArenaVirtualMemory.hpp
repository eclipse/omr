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


#if !defined(PHYSICALARENAVIRTUALMEMORY_HPP_)
#define PHYSICALARENAVIRTUALMEMORY_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "PhysicalArena.hpp"

class MM_EnvironmentBase;
class MM_Heap;
class MM_PhysicalSubArena;
class MM_PhysicalSubArenaVirtualMemory;

enum {
	modron_pavm_attach_policy_none = 0,
	modron_pavm_attach_policy_high_memory
};

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_PhysicalArenaVirtualMemory : public MM_PhysicalArena {
private:
protected:
	MM_PhysicalSubArenaVirtualMemory* _physicalSubArena;

	bool initialize(MM_EnvironmentBase* env);

public:
	static MM_PhysicalArenaVirtualMemory* newInstance(MM_EnvironmentBase* env, MM_Heap* heap);

	virtual bool inflate(MM_EnvironmentBase* env);

	/**
	 * Calculate the size of the range from the supplied address to the sub arenas high address.
	 * @param address The base address of the range.
	 * @return The size of the range.
	 */
	MMINLINE uintptr_t calculateOffsetToHighAddress(void* address)
	{
		return (uintptr_t)getHighAddress() - (uintptr_t)address;
	};

	virtual bool attachSubArena(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena, uintptr_t size, uintptr_t attachPolicy);
	virtual void detachSubArena(MM_EnvironmentBase* env, MM_PhysicalSubArena* subArena);

	using MM_PhysicalArena::canExpand;
	bool canExpand(MM_EnvironmentBase* env, MM_PhysicalSubArenaVirtualMemory* subArena, void* expandAddress, uintptr_t expandSize);
	uintptr_t maxExpansion(MM_EnvironmentBase* env, MM_PhysicalSubArenaVirtualMemory* subArena, void* expandAddress);
	uintptr_t getPhysicalMaximumExpandSizeHigh(MM_EnvironmentBase* env, void* address);

	uintptr_t getPhysicalMaximumContractSizeLow(MM_EnvironmentBase* env, void* address);

	void* findAdjacentHighValidAddress(MM_EnvironmentBase* env);

	MM_PhysicalArenaVirtualMemory(MM_EnvironmentBase* env, MM_Heap* heap)
		: MM_PhysicalArena(env, heap)
		, _physicalSubArena(NULL)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* PHYSICALARENAVIRTUALMEMORY_HPP_ */
