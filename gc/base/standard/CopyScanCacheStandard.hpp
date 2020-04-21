/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Modron_Base
 */

#if !defined(COPYSCANCACHESTANDARD_HPP_)
#define COPYSCANCACHESTANDARD_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#include "CopyScanCache.hpp"
#include "ObjectScannerState.hpp"

class GC_ObjectScanner;

/**
 * @todo Provide class documentation
 * @ingroup GC_Modron_Base
 */
class MM_CopyScanCacheStandard : public MM_CopyScanCache
{
	/* Data Members */
private:
protected:
public:
	GC_ObjectScannerState _objectScannerState; /**< Space reserved for instantiation of object scanner for current object */
	bool _shouldBeRemembered; /**< whether current object being scanned should be remembered */
	uintptr_t _arraySplitIndex; /**< The index within a split array to start scanning from (meaningful if OMR_SCAVENGER_CACHE_TYPE_SPLIT_ARRAY is set) */
	uintptr_t _arraySplitAmountToScan; /**< The amount of elements that should be scanned by split array scanning. */
	omrobjectptr_t* _arraySplitRememberedSlot; /**< A pointer to the remembered set slot a split array came from if applicable. */

	/* Members Function */
private:
protected:
public:
	MMINLINE GC_ObjectScanner *
	getObjectScanner()
	{
		return (GC_ObjectScanner *)(&_objectScannerState);
	}

	/**
	 * Determine whether the receiver represents a split array.
	 * If so, the array object may be found in scanCurrent and the index in _arraySplitIndex.
	 * @return whether the receiver represents a split array
	 */
	MMINLINE bool
	isSplitArray() const
	{
		return (OMR_SCAVENGER_CACHE_TYPE_SPLIT_ARRAY == (flags & OMR_SCAVENGER_CACHE_TYPE_SPLIT_ARRAY));
	}
	
	/**
	 * reinitializes the cache with the given base and top addresses.
	 * @param base base address of cache
	 * @param top top address of cache
	 */
	void reinitCache(void *base, void *top) {
		cacheBase = base;
		cacheAlloc = base;
		scanCurrent = base;
		cacheTop = top;
		_arraySplitIndex = 0;
		_arraySplitAmountToScan = 0;
		_arraySplitRememberedSlot = NULL;
		_hasPartiallyScannedObject = false;
		_shouldBeRemembered = false;
	}

	/**
	 * Create a CopyScanCacheStandard object.
	 */	
	MM_CopyScanCacheStandard(uintptr_t givenFlags)
		: MM_CopyScanCache(givenFlags)
		, _shouldBeRemembered(false)
		, _arraySplitIndex(0)
		, _arraySplitAmountToScan(0)
		, _arraySplitRememberedSlot(NULL)
	{}
};

#endif /* COPYSCANCACHESTANDARD_HPP_ */
