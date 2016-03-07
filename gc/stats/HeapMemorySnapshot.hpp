/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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
 
#if !defined(HEAPMEMORYSNAPSHOT_HPP_)
#define HEAPMEMORYSNAPSHOT_HPP_

#include "omrcomp.h"

#include "Base.hpp"

/**
 * A collection of interesting statistics for the HeapMemory used by MemoryManagement.
 * @ingroup GC_Stats
 */
class MM_HeapMemorySnapshot : public MM_Base
{
public:
	uintptr_t _totalHeapSize;
	uintptr_t _freeHeapSize;

	uintptr_t _totalTenuredSize;
	uintptr_t _freeTenuredSize;
	uintptr_t _totalTenuredSOASize;
	uintptr_t _freeTenuredSOASize;
	uintptr_t _totalTenuredLOASize;
	uintptr_t _freeTenuredLOASize;
	uintptr_t _totalNurseryAllocateSize;
	uintptr_t _freeNurseryAllocateSize;
	uintptr_t _totalNurserySurvivorSize;
	uintptr_t _freeNurserySurvivorSize;

	uintptr_t _totalRegionOldSize;
	uintptr_t _freeRegionOldSize;
	uintptr_t _totalRegionEdenSize;
	uintptr_t _freeRegionEdenSize;
	uintptr_t _totalRegionSurvivorSize;
	uintptr_t _freeRegionSurvivorSize;
	uintptr_t _totalRegionReservedSize;
	uintptr_t _freeRegionReservedSize;

	/**
	 * Create a HeapMemorySnapshot object.
	 */
	MM_HeapMemorySnapshot() :
		MM_Base(),
		_totalHeapSize(0),
		_freeHeapSize(0),
		_totalTenuredSize(0),
		_freeTenuredSize(0),
		_totalTenuredSOASize(0),
		_freeTenuredSOASize(0),
		_totalTenuredLOASize(0),
		_freeTenuredLOASize(0),
		_totalNurseryAllocateSize(0),
		_freeNurseryAllocateSize(0),
		_totalNurserySurvivorSize(0),
		_freeNurserySurvivorSize(0),
		_totalRegionOldSize(0),
		_freeRegionOldSize(0),
		_totalRegionEdenSize(0),
		_freeRegionEdenSize(0),
		_totalRegionSurvivorSize(0),
		_freeRegionSurvivorSize(0),
		_totalRegionReservedSize(0),
		_freeRegionReservedSize(0)
	{};
};

#endif /* HEAPMEMORYSNAPSHOT_HPP_ */
