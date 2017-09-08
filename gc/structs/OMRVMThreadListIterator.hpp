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

