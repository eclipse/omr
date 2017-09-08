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

#if !defined(POOLITERATOR_HPP_)
#define POOLITERATOR_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"
#include "omrpool.h"
#include "pool_api.h"

/**
 * Iterate over the contents of a J9Pool.
 * @ingroup GC_Structs
 */
class GC_PoolIterator
{
	J9Pool *_pool;
	pool_state _state;
	void **_nextValue;	

public:

	/**
	 * Initialize an existing iterator with a new pool.
	 */
	MMINLINE void init(J9Pool *aPool)
	{
		_pool = aPool;
		if(NULL != _pool) {
			_nextValue = (void **)pool_startDo(_pool, &_state);
		} else {
			_nextValue = NULL;
		}
	}

	/**
	 * @note This constructor will accept NULL (needed by GC_VMThreadJNISlotIterator) but behaviour of nextSlot() is undefined after that. 
	 */
	GC_PoolIterator(J9Pool *aPool) :
		_pool(aPool),
		_nextValue(NULL)
	{
		if (_pool) {
			_nextValue = (void**)pool_startDo(_pool, &_state);
		}
	};

	void **nextSlot();
};

#endif /* POOLITERATOR_HPP_ */

