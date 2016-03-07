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

#if !defined(OMRVMTHREADLISTITERATOR_HPP_)
#define OMRVMTHREADLISTITERATOR_HPP_

#include "omrcfg.h"
#include "omr.h"
#include "modronbase.h"

#include "OMR_VMThread.hpp"

/**
 * Iterate over a list of VM threads.
 */
class GC_OMRVMThreadListIterator
{
	OMR_VMThread *_initialOMRVMThread;
	OMR_VMThread *_omrVMThread;

public:
	/**
	 * Create an iterator which will start with the main thread in the given javaVM
	 */
	GC_OMRVMThreadListIterator(OMR_VM *vm) :
		_initialOMRVMThread(vm->_vmThreadList),
		_omrVMThread(vm->_vmThreadList)
	{}

	/**
	 * Create an iterator which will start with the given thread
	 */
	GC_OMRVMThreadListIterator(OMR_VMThread *thread) :
		_initialOMRVMThread(thread),
		_omrVMThread(thread)
	{}

	/**
	 * Restart the iterator back to the initial thread.
	 */
	MMINLINE void reset() {
		_omrVMThread = _initialOMRVMThread;
	}

	/**
	 * Restart the iterator back to a specific initial thread.
	 */
	MMINLINE void reset(OMR_VMThread *resetThread) {
		_omrVMThread = _initialOMRVMThread = resetThread;
	}

	OMR_VMThread *nextOMRVMThread();
};

#endif /* OMRVMTHREADLISTITERATOR_HPP_ */

