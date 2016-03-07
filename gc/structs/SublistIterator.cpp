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

#include "omrcfg.h"
#include "omrcomp.h"

#include "SublistIterator.hpp"

#include "SublistPuddle.hpp"

/**
 * @return the next sublist puddle
 * @return NULL if there are no more puddles
 */
MM_SublistPuddle *
GC_SublistIterator::nextList()
{
	if(NULL != _currentPuddle) {
		_currentPuddle = _currentPuddle->_next;
	} else {
		_currentPuddle = _sublistPool->_list;
	}
	
	return _currentPuddle;
}
