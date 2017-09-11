/*******************************************************************************
 * Copyright (c) 2015, 2016 IBM Corp. and others
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
