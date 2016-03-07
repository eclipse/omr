/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2013, 2016
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

#define linkNext _linkNext
#define linkPrevious _linkPrevious
#include "omrlinkedlist.h"
#include "mminitcore.h"
#include "omrtrace.h"
#include "ut_omrvm.h"
#include "omrutil.h"
#include "OMR_Agent.hpp"
#include "OMR_Runtime.hpp"
#include "OMR_VM.hpp"
#include "OMR_VMThread.hpp"

extern "C" {

omr_error_t
omr_attach_vm_to_runtime(OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		if ((0 == omrthread_tls_alloc(&vm->_vmThreadKey)) &&
			(0 == omrthread_monitor_init_with_name(&vm->_vmThreadListMutex, 0, "OMR VM thread list mutex"))
		) {
			rc = attachVM(vm->_runtime, vm);
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
omr_detach_vm_from_runtime(OMR_VM *vm)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		omrthread_monitor_destroy(vm->_vmThreadListMutex);
		if (vm->_vmThreadKey > 0) {
			omrthread_tls_free(vm->_vmThreadKey);
		}
		rc = detachVM(vm->_runtime, vm);
		omrthread_detach(self);
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
	}

	return rc;

}

omr_error_t
attachThread(OMR_VM *vm, OMR_VMThread *vmthread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	uintptr_t maximum_thread_count = vm->_configuration._maximum_thread_count;
	uintptr_t internal = vmthread->_internal;

	omrthread_monitor_enter(vm->_vmThreadListMutex);
	if (internal || (0 == maximum_thread_count) || (vm->_languageThreadCount < maximum_thread_count)) {
		J9_LINKED_LIST_ADD_LAST(vm->_vmThreadList, vmthread);
		if (internal) {
			vm->_internalThreadCount += 1;
		} else {
			vm->_languageThreadCount += 1;
		}
	} else {
		rc = OMR_ERROR_MAXIMUM_THREAD_COUNT_EXCEEDED;
	}
	/* Set the TLS for vmthread, NOT the current thread */
	omrthread_tls_set(vmthread->_os_thread, vm->_vmThreadKey, (void *)vmthread);
	omrthread_monitor_exit(vm->_vmThreadListMutex);

	return rc;
}

omr_error_t
detachThread(OMR_VM *vm, OMR_VMThread *vmthread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	uintptr_t internal = vmthread->_internal;

	omrthread_monitor_enter(vm->_vmThreadListMutex);
	if (internal) {
		vm->_internalThreadCount -= 1;
	} else {
		vm->_languageThreadCount -= 1;
	}
	J9_LINKED_LIST_REMOVE(vm->_vmThreadList, vmthread);
	/* Set the TLS for vmthread, NOT the current thread */
	omrthread_tls_set(vmthread->_os_thread, vm->_vmThreadKey, NULL);
	omrthread_monitor_exit(vm->_vmThreadListMutex);

	return rc;
}

#if defined(OMR_THR_FORK_SUPPORT)

void
omr_vm_preFork(OMR_VM *omrVM)
{
	omrthread_t self = NULL;

	/* We cannot take (and hold) a lock unless the thread is attached. Being unattached is fatal. */
	intptr_t result = omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);
	Assert_OMRVM_true(0 == result);

	if (NULL != omrVM->_omrTIAccessMutex) {
		omrthread_monitor_enter(omrVM->_omrTIAccessMutex);
	}
	omrthread_monitor_enter(omrVM->_vmThreadListMutex);

	if (NULL != omrVM->_hcAgent) {
		omrVM->_hcAgent->callOnPreFork();
	}
	omr_trc_preForkHandler();
	omrthread_lib_preFork();
}

void
omr_vm_postForkParent(OMR_VM *omrVM)
{
	omrthread_lib_postForkParent();
	omr_trc_postForkParentHandler();
	if (NULL != omrVM->_hcAgent) {
		omrVM->_hcAgent->callOnPostForkParent();
	}
	omrthread_monitor_exit(omrVM->_vmThreadListMutex);

	if (NULL != omrVM->_omrTIAccessMutex) {
		omrthread_monitor_exit(omrVM->_omrTIAccessMutex);
	}
	omrthread_detach(NULL);
}

void
omr_vm_postForkChild(OMR_VM *omrVM)
{
	omrthread_t self = omrthread_self();
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);

	omrthread_lib_postForkChild();

	omr_trc_postForkChildHandler();

	omrVM->parentPID = omrsysinfo_get_ppid();
	omrVM->forkGeneration += 1;

	/* Find the current OMR_VMThread and free the rest.
	 *
	 * vmThreads hold references to pthread sync objects that may have been corrupted
	 * by forking, such as the mutex protecting the threadName. The memory for those
	 * sync objects can't be reused, so they must not be freed. Destroying a vmThread
	 * normally frees them. We can destroy vmThreads normally here because we have
	 * already run omrthread_lib_postForkChild() to remove the references to these sync
	 * objects.
	 *
	 * If vmThread cleanup is moved prior to omrthread_lib_postForkChild(), we must ensure
	 * that pthread sync objects are not freed if they can't be reused.
	 */
	OMR_VMThread *currentVMThread = NULL;
	OMR_VMThread *walkThread = J9_LINKED_LIST_START_DO(omrVM->_vmThreadList);
	while (NULL != walkThread) {
		OMR_VMThread *nextThread = J9_LINKED_LIST_NEXT_DO(omrVM->_vmThreadList, walkThread);
		if (walkThread->_os_thread == self) {
			currentVMThread = walkThread;
		} else {
			void *languageThread = walkThread->_language_vmthread;

#if defined(OMR_GC)
			if (NULL != omrVM->_gcOmrVMExtensions) {
				cleanupMutatorModel(walkThread, 1);
			}
#endif /* defined(OMR_GC) */
			setOMRVMThreadNameWithFlagNoLock(walkThread, NULL, FALSE);

			omr_vmthread_destroy(walkThread);
			omrmem_free_memory(walkThread);
			OMR_Glue_FreeLanguageThread(languageThread);
		}
		walkThread = nextThread;
	}

	/* Reset vm thread counts and add the current thread to thread list. */
	omrVM->_vmThreadList = NULL;
	Assert_OMRVM_true(NULL != currentVMThread);
	if (currentVMThread->_internal) {
		omrVM->_internalThreadCount = 1;
		omrVM->_languageThreadCount = 0;
	} else {
		omrVM->_internalThreadCount = 0;
		omrVM->_languageThreadCount = 1;
	}
	J9_LINKED_LIST_ADD_LAST(omrVM->_vmThreadList, currentVMThread);

	if (NULL != omrVM->_hcAgent) {
		omrVM->_hcAgent->callOnPostForkChild();
	}
	omrthread_monitor_exit(omrVM->_vmThreadListMutex);

	if (NULL != omrVM->_omrTIAccessMutex) {
		omrthread_monitor_exit(omrVM->_omrTIAccessMutex);
	}
	Trc_OMRVM_PostForkChild(omrVM->forkGeneration, omrVM->parentPID);
	omrthread_detach(NULL);
}

#endif /* defined(OMR_THR_FORK_SUPPORT) */

}
