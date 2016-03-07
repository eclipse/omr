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

/**
 * @file
 * @ingroup GC_Structs
 */

#if !defined(SUBLISTSLOTITERATOR_HPP_)
#define SUBLISTSLOTITERATOR_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#include "SublistPuddle.hpp"

/**
 * Iterate over the contents of an MM_SublistPuddle.
 * Can be used in conjunction with MM_SublistIterator to iterate over all elements in a sublist.
 * @ingroup GC_Structs
 */
class GC_SublistSlotIterator
{
	MM_SublistPuddle *_puddle;
	uintptr_t *_scanPtr;
	uintptr_t _removedCount; /**< keep a running count of elements removed from puddle */
	
	bool _returnedFilledSlot; /**< if last slot returned was filled or null */

public:
	GC_SublistSlotIterator(MM_SublistPuddle *sublist) :
		_puddle(sublist),
		_scanPtr(sublist->_listBase),
		_removedCount(0),
		_returnedFilledSlot(false)
	{};

	void *nextSlot();
	void removeSlot();
};

#endif /* SUBLISTSLOTITERATOR_HPP_ */

