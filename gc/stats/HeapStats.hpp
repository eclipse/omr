/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

#if !defined(HEAPSTATS_HPP_)
#define HEAPSTATS_HPP_

#include "omrcomp.h"

#include "Base.hpp"

/**
 * A collection of interesting statistics for the Heap.
 * @ingroup GC_Stats
 */
class MM_HeapStats : public MM_Base 
{
public:
	uintptr_t _allocCount;
	uintptr_t _allocBytes;
	uintptr_t _allocDiscardedBytes;
	uintptr_t _allocSearchCount;
	
	/* Number of bytes free at end of last GC */
	uintptr_t _lastFreeBytes;

	uintptr_t _activeFreeEntryCount;
	uintptr_t _inactiveFreeEntryCount;

	/**
	 * Create a HeapStats object.
	 */   
	MM_HeapStats() :
		MM_Base(),
		_allocCount(0),
		_allocBytes(0),
		_allocDiscardedBytes(0),
		_allocSearchCount(0),
		_lastFreeBytes(0),
		_activeFreeEntryCount(0),
		_inactiveFreeEntryCount(0)
	{};
};

#endif /* HEAPSTATS_HPP_ */
