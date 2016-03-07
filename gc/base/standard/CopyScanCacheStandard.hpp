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
	uintptr_t _arraySplitIndex; /**< The index within a split array to start scanning from (meaningful if J9VM_MODRON_SCAVENGER_CACHE_TYPE_SPLIT_ARRAY is set) */
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
		return (J9VM_MODRON_SCAVENGER_CACHE_TYPE_SPLIT_ARRAY == (flags & J9VM_MODRON_SCAVENGER_CACHE_TYPE_SPLIT_ARRAY));
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
