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

