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

#if !defined(SUBLISTITERATOR_HPP_)
#define SUBLISTITERATOR_HPP_

#include "omrcfg.h"
#include "omrcomp.h"
#include "modronbase.h"

#include "SublistPool.hpp"

class MM_SublistPuddle;

/**
 * Iterate over the all puddles of a sublist
 * @see MM_SublistPool
 * @ingroup GC_Structs
 */
class GC_SublistIterator
{
	MM_SublistPuddle *_currentPuddle;
	MM_SublistPool *_sublistPool;

public:
	GC_SublistIterator(MM_SublistPool *sublistPool) :
		_currentPuddle(NULL),
		_sublistPool(sublistPool)
	{};

	MM_SublistPuddle *nextList();
};

#endif /* SUBLISTITERATOR_HPP_ */

