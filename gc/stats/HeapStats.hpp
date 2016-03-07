/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
