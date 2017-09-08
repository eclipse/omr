/*******************************************************************************
 * Copyright (c) 2013, 2016 IBM Corp. and others
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

#define linkNext _linkNext
#define linkPrevious _linkPrevious
#include "omrlinkedlist.h"
#include "OMR_Runtime.hpp"
#include "OMR_VM.hpp"
#include "OMR_VMThread.hpp"

extern "C" {

omr_error_t
omr_initialize_runtime(OMR_Runtime *runtime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		if (0 == omrthread_monitor_init_with_name(&runtime->_vmListMutex, 0, "OMR VM list mutex")) {
			runtime->_initialized = TRUE;
		} else {
			rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		}
		omrthread_detach(self);
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
	}

	return rc;
}

omr_error_t
omr_destroy_runtime(OMR_Runtime *runtime)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		if (runtime->_initialized) {
			if (0 == runtime->_vmCount) {
				omrthread_monitor_destroy(runtime->_vmListMutex);
				runtime->_vmListMutex = NULL;
				runtime->_initialized = FALSE;
			} else {
				rc = OMR_VM_STILL_ATTACHED;
			}
		}
		omrthread_detach(self);
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
	}

	return rc;
}

omr_error_t
attachVM(OMR_Runtime *runtime, OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	uintptr_t maximum_vm_count = runtime->_configuration._maximum_vm_count;

	omrthread_monitor_enter(runtime->_vmListMutex);
	if ((0 == maximum_vm_count) || (runtime->_vmCount < maximum_vm_count)) {
		J9_LINKED_LIST_ADD_LAST(runtime->_vmList, vm);
		runtime->_vmCount += 1;
	} else {
		rc = OMR_ERROR_MAXIMUM_VM_COUNT_EXCEEDED;
	}
	omrthread_monitor_exit(runtime->_vmListMutex);

	return rc;
}

omr_error_t
detachVM(OMR_Runtime *runtime, OMR_VM *vm)
{
	omrthread_monitor_enter(runtime->_vmListMutex);
	J9_LINKED_LIST_REMOVE(runtime->_vmList, vm);
	runtime->_vmCount -= 1;
	omrthread_monitor_exit(runtime->_vmListMutex);
	return OMR_ERROR_NONE;
}

}
