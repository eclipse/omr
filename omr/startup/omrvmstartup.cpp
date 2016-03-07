/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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

#include <stdlib.h>

#include "omrport.h"
#if defined(OMR_GC)
#include "mminitcore.h"
#endif /* defined(OMR_GC) */
#include "omr.h"
#include "omrprofiler.h"
#include "omrrasinit.h"
#include "omrvm.h"
#include "thread_api.h"
#include "omrutil.h"

#if defined(OMR_GC)
#include "GCExtensionsBase.hpp"
#include "Heap.hpp"
#include "omrgcstartup.hpp"
#include "StartupManagerImpl.hpp"
#endif /* OMR_GC */
#include "OMR_VMThread.hpp"

/* ****************
 *   Port Library
 * ****************/

static OMRPortLibrary portLibrary;

/* ****************
 *    Public API
 * ****************/

omr_error_t
OMR_Initialize(void *languageVM, OMR_VM **vmSlot)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t j9self = NULL;
	OMR_VM *omrVM = NULL;
	OMR_Runtime *omrRuntime = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);

	/* Initialize the OMR thread and port libraries.*/
	if (0 != omrthread_init_library()) {
		fprintf(stderr, "Failed to initialize OMR thread library.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	/* Recursive omrthread_attach_ex() (i.e. re-attaching a thread that is already attached) is cheaper and less fragile
	 * than non-recursive. If performing a sequence of function calls that are likely to attach & detach internally,
	 * it is more efficient to call omrthread_attach_ex() before the entire block.
	 */
	if (0 != omrthread_attach_ex(&j9self, J9THREAD_ATTR_DEFAULT)) {
		omrtty_printf("Failed to attach main thread.\n");
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
		goto done;
	}

	if (0 != omrport_init_library(&portLibrary, sizeof(OMRPortLibrary))) {
		fprintf(stderr, "Failed to initialize OMR port library.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	/* Disable port library signal handling. This mechanism disables everything except SIGXFSZ.
	 * Handling other signals in the port library will interfere with language-specific signal handlers.
	 */
	if (0 != omrsig_set_options(OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		omrtty_printf("Failed to disable OMR signal handling.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	if (OMR_ERROR_NONE != omr_ras_initMemCategories(&portLibrary)) {
		omrtty_printf("Failed to initialize OMR RAS memory categories.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	omrRuntime = (OMR_Runtime *)omrmem_allocate_memory(sizeof(OMR_Runtime), OMRMEM_CATEGORY_VM);
	if (NULL == omrRuntime) {
		omrtty_printf("Failed to allocate omrRuntime.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	memset(omrRuntime, 0, sizeof(OMR_Runtime));

	omrVM = (OMR_VM *)omrmem_allocate_memory(sizeof(OMR_VM), OMRMEM_CATEGORY_VM);
	if (NULL == omrVM) {
		omrtty_printf("Failed to allocate omrVM.\n");
		rc = OMR_ERROR_INTERNAL;
		goto done;
	}

	memset(omrVM, 0, sizeof(OMR_VM));
	omrVM->_language_vm = languageVM;
	omrVM->_runtime = omrRuntime;
	*vmSlot = omrVM;

	omrRuntime->_configuration._maximum_vm_count = 1;
	omrRuntime->_vmCount = 0;
	omrRuntime->_portLibrary = &portLibrary;

	rc = omr_initialize_runtime(omrRuntime);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("Failed to initialize OMR runtime, rc=%d.\n", rc);
		goto done;
	}
	rc = omr_attach_vm_to_runtime(omrVM);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("Failed to attach OMR VM to runtime, rc=%d.\n", rc);
		goto done;
	}

done:
	omrthread_detach(j9self);
	return rc;
}

omr_error_t
OMR_Shutdown(OMR_VM *omrVM)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t self = NULL;

	if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
		if (NULL != omrVM) {
			OMRPORT_ACCESS_FROM_OMRVM(omrVM);
			OMR_Runtime *omrRuntime = omrVM->_runtime;
			if (NULL != omrRuntime) {
				omr_detach_vm_from_runtime(omrVM);
				omr_destroy_runtime(omrRuntime);
				omrmem_free_memory(omrRuntime);
				omrVM->_runtime = NULL;
			}
			omrmem_free_memory(omrVM);
		}

		portLibrary.port_shutdown_library(&portLibrary);
		omrthread_detach(self);
		omrthread_shutdown_library();
	} else {
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
		fprintf(stderr, "OMR_Shutdown: Failed to attach to omrthread. Proceeding without cleaning up.\n");
	}
	return rc;
}

omr_error_t
OMR_Thread_Init(OMR_VM *omrVM, void *language_vm_thread, OMR_VMThread **threadSlot, const char *threadName)
{
	omr_error_t rc = OMR_ERROR_NONE;

	OMR_VMThread *currentThread = omr_vmthread_getCurrent(omrVM);
	if (NULL == currentThread) {
		omrthread_t self = NULL;
		if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
			rc = OMR_Thread_FirstInit(omrVM, self, language_vm_thread, threadSlot, threadName);
			if (OMR_ERROR_NONE != rc) {
				omrthread_detach(self);
			}
		} else {
			rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
		}
	} else {
		omr_vmthread_reattach(currentThread, threadName);
		*threadSlot = currentThread;
	}
	return rc;
}

omr_error_t
OMR_Thread_FirstInit(OMR_VM *omrVM, omrthread_t self, void *language_vm_thread, OMR_VMThread **threadSlot, const char *threadName)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_VMThread *currentThread = NULL;

	omrthread_monitor_enter(omrVM->_vmThreadListMutex);

	rc = omr_vmthread_firstAttach(omrVM, &currentThread);
	if (OMR_ERROR_NONE == rc) {

		if (NULL != threadName) {
			setOMRVMThreadNameWithFlag(currentThread, currentThread, (char *)threadName, TRUE);
		}

		currentThread->_language_vmthread = language_vm_thread;

		if (NULL != omrVM->_trcEngine) {
			rc = omr_trc_startThreadTrace(currentThread, threadName);
			if (OMR_ERROR_NONE != rc) {
				goto done;
			}
		}

#if defined(OMR_GC)
		if (NULL != omrVM->_gcOmrVMExtensions) {
			if (0 != (omr_error_t)initializeMutatorModel(currentThread)) {
				rc = OMR_ERROR_INTERNAL;
				goto done;
			}
		}
#endif /* defined(OMR_GC) */

		*threadSlot = currentThread;
done:
		if (OMR_ERROR_NONE != rc) {
			/* Error cleanup */
			omr_vmthread_lastDetach(currentThread);
		}
	}
	omrthread_monitor_exit(omrVM->_vmThreadListMutex);
	return rc;
}

omr_error_t
OMR_Thread_Free(OMR_VMThread *omrVMThread)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL == omrVMThread) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;

	} else {
		/* omrVMThread might not be the current thread, so the current thread
		 * might not yet be omrthread_attach_ex()ed.
		 */
		omrthread_t self = NULL;
		if (0 == omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT)) {
			bool selfIsCurrent = (self == omrVMThread->_os_thread);

			if (omrVMThread->_attachCount == 1) {
				rc = OMR_Thread_LastFree(omrVMThread);
			} else {
				omr_vmthread_redetach(omrVMThread);
			}

			if (OMR_ERROR_NONE == rc) {
				if (selfIsCurrent) {
					/* balance the omrthread_attach_ex() from OMR_Thread_Init() */
					omrthread_detach(self);
				}
				/* if (!selfIsCurrent), the runtime must have omrthread_detach()ed when the pthread exited. */
			}

			/* balance the omrthread_attach_ex() in this function */
			omrthread_detach(self);
		} else {
			rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
		}
	}
	return rc;
}

omr_error_t
OMR_Thread_LastFree(OMR_VMThread *omrVMThread)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL == omrVMThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else {
		OMR_VM *omrVM = omrVMThread->_vm;

		/* OMRTODO
		 * We can't make the call to omr_trc_stopThreadTrace() conditional on whether
		 * omrVM->_trcEngine is NULL, because omrVM->_trcEngine might be NULL if the
		 * trace engine was already shutdown. However, the current thread must still
		 * be detached from the trace engine, to execute final cleanup.
		 *
		 * omr_trc_stopThreadTrace() has internal checks to prevent crashing if the
		 * trace engine wasn't initialized.
		 */
		rc = omr_trc_stopThreadTrace(omrVMThread);
		if (OMR_ERROR_NONE != rc) {
			goto done;
		}

		omrthread_monitor_enter(omrVM->_vmThreadListMutex);
#if defined(OMR_GC)
		if (NULL != omrVM->_gcOmrVMExtensions) {
			cleanupMutatorModel(omrVMThread, TRUE); /* cannot fail ? */
		}
#endif /* defined(OMR_GC) */

		/* If the thread had a malloc'ed threadName, this releases it */
		setOMRVMThreadNameWithFlag(omrVMThread, omrVMThread, NULL, FALSE);

		rc = omr_vmthread_lastDetach(omrVMThread);
		omrthread_monitor_exit(omrVM->_vmThreadListMutex);
	}
done:
	return rc;
}

omr_error_t
OMR_Initialize_VM(OMR_VM **omrVMSlot, OMR_VMThread **omrVMThreadSlot, void *languageVM, void *languageVMThread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	omrthread_t j9self = NULL;
	OMR_Runtime *omrRuntime = NULL;
	OMR_VM *omrVM = NULL;
	OMR_VMThread *vmThread = NULL;
	OMRPORT_ACCESS_FROM_OMRPORT(&portLibrary);

	/* Initialize the OMR thread and port libraries.*/
	if (0 != omrthread_init_library()) {
		fprintf(stderr, "Failed to initialize OMR thread library.\n");
		rc = OMR_ERROR_INTERNAL;
		goto failed;
	}

	/* Recursive omrthread_attach_ex() (i.e. re-attaching a thread that is already attached) is cheaper and less fragile
	 * than non-recursive. If performing a sequence of function calls that are likely to attach & detach internally,
	 * it is more efficient to call omrthread_attach_ex() before the entire block.
	 */
	if (0 != omrthread_attach_ex(&j9self, J9THREAD_ATTR_DEFAULT)) {
		omrtty_printf("Failed to attach main thread.\n");
		rc = OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD;
		goto failed;
	}

	if (0 != omrport_init_library(&portLibrary, sizeof(OMRPortLibrary))) {
		fprintf(stderr, "Failed to initialize OMR port library.\n");
		rc = OMR_ERROR_INTERNAL;
		goto failed;
	}

	/* Disable port library signal handling. This mechanism disables everything except SIGXFSZ.
	 * Handling other signals in the port library will interfere with language-specific signal handlers.
	 */
	if (0 != omrsig_set_options(OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		omrtty_printf("Failed to disable OMR signal handling.\n");
		rc = OMR_ERROR_INTERNAL;
		goto failed;
	}

	if (OMR_ERROR_NONE != omr_ras_initMemCategories(&portLibrary)) {
		omrtty_printf("Failed to initialize OMR RAS memory categories.\n");
		rc = OMR_ERROR_INTERNAL;
		goto failed;
	}

	omrRuntime = (OMR_Runtime *)omrmem_allocate_memory(sizeof(OMR_Runtime), OMRMEM_CATEGORY_VM);
	if (NULL == omrRuntime) {
		omrtty_printf("Failed to allocate omrRuntime.\n");
		rc = OMR_ERROR_INTERNAL;
		goto failed;
	}

	memset(omrRuntime, 0, sizeof(OMR_Runtime));
	omrRuntime->_configuration._maximum_vm_count = 1;
	omrRuntime->_vmCount = 0;
	omrRuntime->_portLibrary = &portLibrary;

	rc = omr_initialize_runtime(omrRuntime);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("Failed to initialize OMR runtime, rc=%d.\n", rc);
		goto failed;
	}

	omrVM = (OMR_VM *)omrmem_allocate_memory(sizeof(OMR_VM), OMRMEM_CATEGORY_VM);
	if (NULL == omrVM) {
		omrtty_printf("Failed to allocate omrVM.\n");
		rc = OMR_ERROR_INTERNAL;
		goto failed;
	}

	memset(omrVM, 0, sizeof(OMR_VM));
	omrVM->_language_vm = languageVM;
	omrVM->_runtime = omrRuntime;
	*omrVMSlot = omrVM;

	rc = omr_attach_vm_to_runtime(omrVM);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("Failed to attach OMR VM to runtime, rc=%d.\n", rc);
		goto failed;
	}

	{
		/* Take trace options from OMR_TRACE_OPTIONS */
		const char *traceOpts = getenv("OMR_TRACE_OPTIONS");
		if (NULL != traceOpts) {
			rc = omr_ras_initTraceEngine(omrVM, traceOpts, ".");
		}
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to init trace engine, rc=%d.\n", rc);
			goto failed;
		}
	}

#if defined(OMR_GC)
	{
		/* Initialize the structures required for the heap and mutator model. */
		MM_StartupManagerImpl manager(omrVM);
		rc = OMR_GC_IntializeHeapAndCollector(omrVM, &manager);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to start up Heap Memory Management and Garbage Collector, rc=%d.\n", rc);
			goto failed;
		}
	}
#endif /* OMR_GC */

	rc = OMR_Thread_Init(omrVM, NULL, &vmThread, "SOMMainThread");
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("Failed to attach current thread as OMR Thread, rc=%d.\n", rc);
		goto failed;
	}

	*omrVMThreadSlot = vmThread;

#if defined(OMR_GC)
	gcOmrInitializeTrace(vmThread);
	rc = OMR_GC_InitializeDispatcherThreads(vmThread);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("Failed to launch GC slave threads, rc=%d.\n", rc);
		goto failed;
	}
#endif /* OMR_GC */

	{
		/* Take agent options from OMR_AGENT_OPTIONS */
		const char *healthCenterOpt = getenv("OMR_AGENT_OPTIONS");
		if ((NULL != healthCenterOpt) && ('\0' != *healthCenterOpt)) {
			rc = omr_ras_initHealthCenter(omrVM, &(omrVM->_hcAgent), healthCenterOpt);
		}
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to init health center, rc=%d.\n", rc);
			goto failed;
		}
		rc = omr_ras_initMethodDictionary(omrVM, OMR_Glue_GetMethodDictionaryPropertyNum(), OMR_Glue_GetMethodDictionaryPropertyNames());
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to init method dictionary, rc=%d.\n", rc);
			goto failed;
		}
	}

failed:
	return rc;
}

omr_error_t
OMR_Shutdown_VM(OMR_VM *omrVM, OMR_VMThread *omrVMThread)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL != omrVMThread) {
		OMRPORT_ACCESS_FROM_OMRVMTHREAD(omrVMThread);
#if defined(OMR_GC)
		rc = OMR_GC_ShutdownDispatcherThreads(omrVMThread);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to shutdown GC slave threads, rc=%d.\n", rc);
		}

		rc = OMR_GC_ShutdownCollector(omrVMThread);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to shutdown Garbage Collector, rc=%d.\n", rc);
		}
#endif /* OMR_GC */

		omr_ras_cleanupHealthCenter(omrVM, &(omrVM->_hcAgent));

		rc = omr_ras_cleanupTraceEngine(omrVMThread);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to cleanup trace engine, rc=%d.\n", rc);
		}

		rc = OMR_Thread_Free(omrVMThread);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to free OMR VMThread for main thread, rc=%d.\n", rc);
		}
	} else {
		fprintf(stderr, "No OMR VMThread so skipping shutdown phases that required an attached thread, rc=%d.\n", rc);
		fflush(stderr);
	}

	if (NULL != omrVM) {
		OMRPORT_ACCESS_FROM_OMRVM(omrVM);
#if defined(OMR_GC)
		rc = OMR_GC_ShutdownHeap(omrVM);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("Failed to shutdown Heap Mememory Management, rc=%d.\n", rc);
		}
#endif /* OMR_GC */

		OMR_Runtime *omrRuntime = omrVM->_runtime;
		if (NULL != omrRuntime) {
			omr_detach_vm_from_runtime(omrVM);
			omr_destroy_runtime(omrRuntime);
			omrmem_free_memory(omrRuntime);
			omrVM->_runtime = NULL;
		}
		omrmem_free_memory(omrVM);
	} else {
		fprintf(stderr, "No OMR VM so skipping shutdown phases that required an initialized VM, rc=%d.\n", rc);
		fflush(stderr);
	}

	portLibrary.port_shutdown_library(&portLibrary);
	/*
	 * No need to detach your thread as you are the last running thread
	 * the thread pool will be destroyed by omrthread_shutdown_library
	 */
	omrthread_shutdown_library();

	return rc;
}
