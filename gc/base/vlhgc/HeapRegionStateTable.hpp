/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#if !defined(HEAPREGIONSTATETABLE_HPP)
#define HEAPREGIONSTATETABLE_HPP

#include <omrcfg.h>

#include "omrgcconsts.h"
#include "BaseVirtual.hpp"

#include <stdint.h>

namespace OMR {
namespace GC {

class Forge;

class HeapRegionStateTable : public MM_BaseVirtual
{
	/*
	 * Data members
	 */
  public:
  protected:
  private:
	uint8_t *_table;
	uintptr_t _heapBase;
	uintptr_t _regionShift;

	/*
	 * Function members
	 */
  public:

	static HeapRegionStateTable *newInstance(Forge *forge, uintptr_t heapBase, uintptr_t regionShift, uintptr_t regionCount);

	bool initialize(Forge *forge, uintptr_t heapBase, uintptr_t regionShift, uintptr_t regionCount);

	void kill(Forge *forge);

	void tearDown(Forge *forge);

	HeapRegionStateTable()
		: _table(NULL)
		, _heapBase(0)
		, _regionShift(0)
	{
		_typeId = __FUNCTION__;
	}

	MMINLINE uint8_t *getTable() { return _table; }

	MMINLINE uintptr_t
	getIndex(const void *heapAddress)
	{
		uintptr_t heapDelta = ((uintptr_t)heapAddress) - _heapBase;
		uintptr_t index = heapDelta >> _regionShift; 
		return index;
	}

	MMINLINE uint8_t
	getRegionState(const void *heapAddress)
	{
		return _table[getIndex(heapAddress)];
	}

	/**
	 * Set the state of the region in the table.  Must use
	 * constants defined by enum HeapRegionState,
	 * 	HEAP_REGION_STATE_NONE, or HEAP_REGION_STATE_COPY_FORWARD
	 */
	MMINLINE void
	setRegionState(const void *heapAddress, uint8_t state)
	{
		_table[getIndex(heapAddress)] = state;
	}

  protected:
  private:
};

} // namespace GC
} // namespace OMR

#endif /* defined(HEAPREGIONSTATETABLE_HPP) */
