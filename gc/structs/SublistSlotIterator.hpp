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

