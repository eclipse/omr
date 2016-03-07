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

#include "omrcfg.h"
#include "omr.h"

#include "OMRVMThreadListIterator.hpp"

/**
 * @return the next VM Thread in the list
 * @return NULL if there are no more threads in the list
 */
OMR_VMThread *
GC_OMRVMThreadListIterator::nextOMRVMThread()
{
	if(_omrVMThread) {
		OMR_VMThread *currentOMRVMThread;
		currentOMRVMThread = _omrVMThread;
		_omrVMThread = _omrVMThread->_linkNext;
		if(_omrVMThread == _initialOMRVMThread) {
			_omrVMThread = NULL;
		}
		return currentOMRVMThread;
	}
	return NULL;
}

