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

#if !defined(PHYSICALSUBARENAVIRTUALMEMORY_HPP_)
#define PHYSICALSUBARENAVIRTUALMEMORY_HPP_

#include "omrcomp.h"
#include "modronbase.h"

#include "PhysicalSubArena.hpp"

class MM_EnvironmentBase;
class MM_Heap;

/**
 * @todo Provide class documentation
 * @ingroup GC_Base_Core
 */
class MM_PhysicalSubArenaVirtualMemory : public MM_PhysicalSubArena {
private:
protected:
	MM_PhysicalSubArenaVirtualMemory* _lowArena, *_highArena;
	void* _lowAddress, *_highAddress;

	bool _hasVirtualLowAddress, _hasVirtualHighAddress;
	void* _virtualLowAddress, *_virtualHighAddress;

	bool _expandFromHighRange, _expandFromLowRange;

	uintptr_t _numaNode;  /**< NUMA node binding, starting from 0. if UDATA_MAX, no explicit binding */

	virtual bool initialize(MM_EnvironmentBase* env);

public:
	MMINLINE MM_PhysicalSubArenaVirtualMemory* getNextSubArena() { return _highArena; }
	MMINLINE void setNextSubArena(MM_PhysicalSubArenaVirtualMemory* subArena) { _highArena = subArena; }
	MMINLINE MM_PhysicalSubArenaVirtualMemory* getPreviousSubArena() { return _lowArena; }
	MMINLINE void setPreviousSubArena(MM_PhysicalSubArenaVirtualMemory* subArena) { _lowArena = subArena; }

	MMINLINE void* getLowAddress() { return _lowAddress; }
	MMINLINE void setLowAddress(void* addr) { _lowAddress = addr; }
	MMINLINE void* getHighAddress() { return _highAddress; }
	MMINLINE void setHighAddress(void* addr) { _highAddress = addr; }

	MMINLINE void* getVirtualLowAddress() { return _hasVirtualLowAddress ? _virtualLowAddress : _lowAddress; }
	MMINLINE void setVirtualLowAddress(void* virtualLowAddress)
	{
		_hasVirtualLowAddress = true;
		_virtualLowAddress = virtualLowAddress;
	}
	MMINLINE void* getVirtualHighAddress() { return _hasVirtualHighAddress ? _virtualHighAddress : _highAddress; }
	MMINLINE void setVirtualHighAddress(void* virtualHighAddress)
	{
		_hasVirtualHighAddress = true;
		_virtualHighAddress = virtualHighAddress;
	}
	MMINLINE void clearVirtualAddresses()
	{
		_hasVirtualLowAddress = false;
		_hasVirtualHighAddress = false;
		_virtualLowAddress = NULL;
		_virtualHighAddress = NULL;
	}

	void* findAdjacentHighValidAddress(MM_EnvironmentBase* env);
	
	MMINLINE uintptr_t getNumaNode() { return _numaNode; }
	MMINLINE void setNumaNode(uintptr_t numaNode) { _numaNode = numaNode; }

	/**
	 * Calculate the size of the range from the supplied address to the top of the sub arena.
	 * @param address The base address of the range.
	 * @return The size of the range.
	 */
	MMINLINE uintptr_t calculateOffsetToHighAddress(void* address) { return (uintptr_t)(((uintptr_t)_highAddress - (uintptr_t)address)); }

	MM_PhysicalSubArenaVirtualMemory(MM_Heap* heap)
		: MM_PhysicalSubArena(heap)
		, _lowArena(NULL)
		, _highArena(NULL)
		, _lowAddress(NULL)
		, _highAddress(NULL)
		, _hasVirtualLowAddress(false)
		, _hasVirtualHighAddress(false)
		, _virtualLowAddress(NULL)
		, _virtualHighAddress(NULL)
		, _expandFromHighRange(false)
		, _expandFromLowRange(false)
		, _numaNode(0)
	{
		_typeId = __FUNCTION__;
	};
};

#endif /* PHYSICALSUBARENAVIRTUALMEMORY_HPP_ */
