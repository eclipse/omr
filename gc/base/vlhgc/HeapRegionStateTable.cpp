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

#include "omrcfg.h"

#include "HeapRegionStateTable.hpp"

#include "omrcomp.h"
#include "Forge.hpp"

#include <cstring>

namespace OMR {
namespace GC {

HeapRegionStateTable *HeapRegionStateTable::newInstance(Forge *forge, uintptr_t heapBase, uintptr_t regionShift, uintptr_t regionCount)
{
	HeapRegionStateTable* table = ::new(forge, AllocationCategory::FIXED, OMR_GET_CALLSITE(), std::nothrow) HeapRegionStateTable();
	if(NULL != table) {
		bool success = table->initialize(forge, heapBase, regionShift, regionCount);
		if (! success) {
			table->kill(forge);
			table = NULL;
		}
	}
	return table;
}

bool
HeapRegionStateTable::initialize(Forge *forge, uintptr_t heapBase, uintptr_t regionShift, uintptr_t regionCount)
{
	_heapBase = heapBase;
	_regionShift= regionShift;

	/* 1 byte per region */
	_table = (uint8_t *) forge->allocate(regionCount, AllocationCategory::FIXED, OMR_GET_CALLSITE());
	if (NULL == _table) {
		return false;
	}

	memset(_table, 0, regionCount);

	return true;
}

void
HeapRegionStateTable::kill(Forge *forge)
{
	tearDown(forge);
	forge->free(this);
}

void
HeapRegionStateTable::tearDown(Forge *forge)
{
	if (_table != NULL) {
		forge->free(_table);
	}
}

} // namespace GC
} // namespace OMR