/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1998, 2016
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

#include <limits.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <assert.h>

#include "omrtrace_internal.h"
#include "pool_api.h"
#include "ute_core.h"
#include "omrutil.h"

#include "AtomicSupport.hpp"

const char *UT_NO_THREAD_NAME = "MISSING";

/* Structures for trace interfaces. */
static UtInterface internalUtIntfS;

OMR_TraceGlobal *omrTraceGlobal = NULL;
omrthread_tls_key_t j9uteTLSKey;

static omr_error_t trcFlushTraceData(OMR_TraceThread *thr);
static omr_error_t trcGetTraceMetadata(void **data, int32_t *length);
static omr_error_t trcSetOptions(OMR_TraceThread *thr, const char *opts[]);
static omr_error_t moduleLoaded(OMR_TraceThread *thr, UtModuleInfo *modInfo);
static omr_error_t moduleUnLoading(OMR_TraceThread *thr, UtModuleInfo *modInfo);
static void omrTraceInit(void *env, UtModuleInfo *modInfo);
static void omrTraceTerm(void *env, UtModuleInfo *modInfo);
static void setStartTime(void);

static void
omrTraceInit(void *env, UtModuleInfo *modInfo)
{
	moduleLoaded(OMR_TRACE_THREAD_FROM_ENV(env), modInfo);
}

/*******************************************************************************
 * name        - moduleLoaded
 * description - Initialize tracing for a loaded module
 * parameters  - OMR_TraceThread,  UtModuleInfo pointer
 * returns     - an OMR error code
 ******************************************************************************/
omr_error_t
moduleLoaded(OMR_TraceThread *thr, UtModuleInfo *modInfo)
{
	UtComponentData *compData = NULL;
	omr_error_t rc = OMR_ERROR_NONE;

	if ((NULL == thr) && (OMR_TRACE_ENGINE_MT_ENABLED == OMR_TRACEGLOBAL(initState))) {
		return OMR_THREAD_NOT_ATTACHED;
	}
	if (NULL == modInfo) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	UT_DBGOUT(1, ("<UT> ModuleLoaded: %s\n", modInfo->name));

	if (modInfo->traceVersionInfo == NULL) {
		/* this is a pre 142 module - not compatible with this trace engine fail silently to register this module */
		UT_DBGOUT(1, ("<UT> ModuleLoaded refusing registration to %s because it's version is less than the supported UT version %d\n", modInfo->name, UT_VERSION));
		return OMR_ERROR_NONE;
	} /* else {
		this field contains the version number and can be used to modify behaviour based on level of module loaded
	} */

	checkGetTraceLock(thr);

	if (modInfo->intf == NULL) {
		modInfo->intf = internalUtIntfS.module;

		rc = initialiseComponentData(&compData, modInfo, modInfo->name);
		if (OMR_ERROR_NONE == rc) {
			rc = addComponentToList(compData, OMR_TRACEGLOBAL(componentList));
		}
		if (OMR_ERROR_NONE == rc) {
			rc = processComponentDefferedConfig(compData, OMR_TRACEGLOBAL(componentList));
		}
		if (OMR_ERROR_NONE != rc) {
			/* Module not configured for trace: %s */
			freeTraceLock(thr);
			return OMR_ERROR_INTERNAL;
		}
	} else {
		/* guard against stale pointers */
		modInfo->intf = internalUtIntfS.module;
		/* this module has already been registered. Just increment the reference count */
		modInfo->referenceCount += 1;
	}

	checkFreeTraceLock(thr);

	UT_DBGOUT(1, ("<UT> ModuleLoaded: %s, interface: "
				  UT_POINTER_SPEC "\n", modInfo->name, modInfo->intf));
	return OMR_ERROR_NONE;
}

static void
omrTraceTerm(void *env, UtModuleInfo *modInfo)
{
	OMR_TraceThread *thr = OMR_TRACE_THREAD_FROM_ENV(env);
	moduleUnLoading(thr, modInfo);
}

/*******************************************************************************
 * name        - moduleUnLoading
 * description - Terminate tracing for an module
 * parameters  - OMR_TraceThread, UtModuleInfo pointer
 * returns     - void
 ******************************************************************************/
omr_error_t
moduleUnLoading(OMR_TraceThread *thr, UtModuleInfo *modInfo)
{
	omr_error_t rc = OMR_ERROR_NONE;

	/* A module can be unloaded in shutdown phase, as long as the current thread is attached */
	if (NULL == omrTraceGlobal) {
		return OMR_ERROR_NOT_AVAILABLE;
	}
	if (NULL == modInfo) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (NULL == thr) {
		/*
		 * It's unsafe to unload from a detached thread because the trace engine
		 * might be in the process of being deleted.
		 */
		return OMR_THREAD_NOT_ATTACHED;
	}

	UT_DBGOUT(1, ("<UT> ModuleUnloading: %s\n", modInfo->name));

	if (NULL == modInfo->traceVersionInfo) {
		/* this is a pre 142 module - not compatible with this trace engine fail silently to register this module */
		UT_DBGOUT(1, ("<UT> ModuleLoaded refusing deregistration to %s because it's version is less than the supported UT version %d\n", modInfo->name, UT_VERSION));
		return OMR_ERROR_NONE;
	} /* else {
		this field contains the version number and can be used to modify behaviour based on level of module loaded
	} */


	getTraceLock(thr);

	if (modInfo->referenceCount > 0) {
		modInfo->referenceCount -= 1;
	} else {
		rc = setTracePointsTo(modInfo->name, OMR_TRACEGLOBAL(componentList), TRUE, 0, 0, 0, -1, NULL, FALSE, TRUE);
		if (OMR_ERROR_NONE != rc) {
			UT_DBGOUT(1, ("<UT> problem turning off trace in %s as it unloads\n", modInfo->name));
			/* proceed to remove it from list in case it has gone and we try and manipulate it's control array */
		}
		rc = removeModuleFromList(modInfo, OMR_TRACEGLOBAL(componentList));

		/* Only clear intf if the module is no longer referenced */
		modInfo->intf = NULL;
	}

	freeTraceLock(thr);

	return  rc;
}

/*******************************************************************************
 * name        - threadStop
 * description - Handle Thread termination
 * parameters  - OMR_TraceThread
 * returns     - void
 ******************************************************************************/
omr_error_t
threadStop(OMR_TraceThread **thrSlot)
{
	if (NULL == omrTraceGlobal) {			/* very fatal! */
		if (NULL != thrSlot) {
			*thrSlot = NULL;
		}
		return OMR_ERROR_INTERNAL;
	}

	/*
	 *  Ensure a valid thread pointer has been passed
	 */
	if (NULL == *thrSlot) {
		UT_DBGOUT(1, ("<UT> Bad thread passed to ThreadStop\n"));
		return OMR_ERROR_INTERNAL;
	}

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));
	OMR_TraceThread *thr = *thrSlot;
	OMR_TraceBuffer *trcBuf = thr->trcBuf;

	UT_DBGOUT(3, ("<UT> ThreadStop entered for thread anchor " UT_POINTER_SPEC "\n", thr));

	/*
	 *  Write out active trace buffer
	 */
	if (NULL != trcBuf) {
		if (!OMR_TRACEGLOBAL(traceInCore)) {
			/*
			 * Trace the thread purge unconditionally
			 * Write Trc_Purge, dg.262
			 */
			internalTrace(thr, NULL, (UT_TRC_PURGE_ID << 8) | UT_MINIMAL, NULL);

			UT_DBGOUT(3, ("<UT> Purging buffer " UT_POINTER_SPEC " for thread " UT_POINTER_SPEC "\n", trcBuf, thr));
			publishTraceBuffer(thr, trcBuf);
		} else {
			releaseTraceBuffer(thr, trcBuf);
		}
	}

	/*
	 * Mark the thread detached from the trace engine. No more tracepoints after this.
	 */
	*thrSlot = NULL;
	omrthread_tls_set(OS_THREAD_FROM_TRACE_THREAD(thr), j9uteTLSKey, NULL);

	/*
	 *  Free the OMR_TraceThread
	 */
	omrthread_monitor_enter(OMR_TRACEGLOBAL(threadPoolLock));
	if ((NULL != thr->name) && (UT_NO_THREAD_NAME != thr->name)) {
		omrmem_free_memory((void *)thr->name);
	}
	pool_removeElement(OMR_TRACEGLOBAL(threadPool), thr);
	omrthread_monitor_exit(OMR_TRACEGLOBAL(threadPoolLock));

	uintptr_t newCount = VM_AtomicSupport::subtractU32(&OMR_TRACEGLOBAL(threadCount), 1);

	/*
	 * Last thread out frees the trace engine, including OMR_TraceGlobal and the pool of trace buffers.
	 *
	 * If this is not the last thread out, another thread can start deleting the trace engine now.
	 * We must not access the trace engine after this.
	 */
	if (0 == newCount) {
		/*
		 * Don't allow initState to be read before threadCount. Prevents this thread from missing
		 * whether another thread started trace shutdown.
		 *
		 * e.g.
		 * (this thread)						(other thread)
		 * read initState, MT_ENABLED
		 * 										set initState to SHUTDOWN_STARTED (from utTerminateTrace())
		 * 										atomic decr threadCount,
		 * 											new threadCount is 1
		 * 										skip freeTrace() because threadCount > 0
		 * 	atomic decr threadCount,
		 * 		new threadCount is 0
		 * 	skip freeTrace() because initState
		 * 		is not SHUTDOWN_STARTED
		 */
		VM_AtomicSupport::readBarrier();
		if (OMR_TRACE_ENGINE_SHUTDOWN_STARTED == OMR_TRACEGLOBAL(initState)) {
			if (OMR_TRACEGLOBAL(traceDebug) >= 2) {
				omrtty_err_printf("<UT> threadStop entered for final thread " UT_POINTER_SPEC ", freeing buffers\n", thr);
			}
			freeTrace();
		}
	}
	return OMR_ERROR_NONE;
}


/*
 * Assumes current thread is attached to trace.
 */
omr_error_t
utTerminateTrace(void)
{
	omr_error_t result = OMR_ERROR_NONE;

	if (NULL == omrTraceGlobal) {			/* very fatal! */
		return OMR_ERROR_INTERNAL;
	}

	/* This is not threadsafe. We don't expect this function to be called concurrently. */
	OMR_TraceEngineInitState oldState = OMR_TRACEGLOBAL(initState);
	OMR_TRACEGLOBAL(initState) = OMR_TRACE_ENGINE_SHUTDOWN_STARTED;

	if (OMR_TRACE_ENGINE_IS_ENABLED(oldState)) {
		UT_DBGOUT(1, ("<UT> Trace shutdown started\n"));
	} else {
		result = OMR_ERROR_INTERNAL;
	}

	if (OMR_TRACEGLOBAL(traceCount)) {
		listCounters();
	}

	if (OMR_TRACEGLOBAL(lostRecords) != 0) {
		UT_DBGOUT(1, ("<UT> Discarded %d trace buffers\n", OMR_TRACEGLOBAL(lostRecords)));
	}
	return result;
}

/*******************************************************************************
 * name        - freeTrace
 * description - Free up all trace structures
 * parameters  - OMR_TraceThread struct
 * returns     - void
 ******************************************************************************/
void
freeTrace(void)
{
	OMR_TraceGlobal *global = omrTraceGlobal;
	OMRTraceEngine *trcEngine = global->vm->_trcEngine;
	OMRPORT_ACCESS_FROM_OMRPORT(global->portLibrary);
	int32_t traceDebug = global->traceDebug;

	UT_DBGOUT(1, ("<UT> freeTrace Entered\n"));

	if (OMR_TRACEGLOBAL(initState) < OMR_TRACE_ENGINE_SHUTDOWN_STARTED) {
		/* shut everything down before freeing everything */
		UT_DBGOUT(1, ("<UT> Error: freeTrace called before trace has been finalized\n"));
	}

	/*
	 * Set omrTraceglobal to NULL.
	 * This prevents new threads from attaching to the trace engine, and new modules from being loaded.
	 * OMR_TRACEGLOBAL() macro can't be used after this.
	 *
	 * OMRTODO
	 * We still have a problem where we fail to notice that a concurrent thread is in the process
	 * of attaching, and start deleting omrTraceGlobal from under it.
	 */
	omrTraceGlobal = NULL;
	const_cast<OMR_VM *>(global->vm)->_trcEngine = NULL;
	const_cast<OMR_VM *>(global->vm)->utIntf = NULL;

	/* Set freed fields to NULL to trap code that uses stale pointers */

	UtTraceCfg *config = global->config;
	while (NULL != config) {
		UtTraceCfg *tmpconfig = config;
		config = config->next;
		omrmem_free_memory(tmpconfig);
	}

	/*
	 * Free all trace component list storage.
	 *
	 * Note that the static UtModuleInfo, such as the array of active tracepoints,
	 * is NOT cleared here. If a module is not properly unloaded, and the trace
	 * engine is restarted, then the UtModuleInfo for that module may be in a bad
	 * state, and tracepoints from that module may crash.
	 *
	 * The UtModuleInfo is normally cleared when a module is unloaded, see moduleUnLoading().
	 */
	freeComponentList(global, global->componentList);
	freeComponentList(global, global->unloadedComponentList);

	if (NULL != global->traceFormatSpec) {
		omrmem_free_memory(global->traceFormatSpec);
		global->traceFormatSpec = NULL;
	}

	if (NULL != global->properties) {
		omrmem_free_memory(global->properties);
		global->properties = NULL;
	}

	if (NULL != global->serviceInfo) {
		omrmem_free_memory(global->serviceInfo);
		global->serviceInfo = NULL;
	}

	if (NULL != global->traceHeader) {
		omrmem_free_memory(global->traceHeader);
		global->traceHeader = NULL;
	}

	UtSubscription *subscription = (UtSubscription *)global->subscribers;
	while (NULL != subscription) {
		UtSubscription *next = subscription->next;
		deleteRecordSubscriber(global, subscription);
		subscription = next;
	}
	global->subscribers = NULL;

	omrthread_monitor_destroy(global->subscribersLock);
	global->subscribersLock = NULL;

	omrthread_monitor_destroy(global->freeQueueLock);
	global->freeQueueLock = NULL;

	omrthread_monitor_destroy(global->traceLock);
	global->traceLock = NULL;

	omrthread_monitor_destroy(global->bufferPoolLock);
	global->bufferPoolLock = NULL;

	pool_kill(global->bufferPool);
	global->bufferPool = NULL;

	omrthread_monitor_destroy(global->threadPoolLock);
	global->threadPoolLock = NULL;

	pool_kill(global->threadPool);
	global->threadPool = NULL;

	omrthread_tls_free(j9uteTLSKey);
	j9uteTLSKey = 0;

	omrmem_free_memory(trcEngine);
	omrmem_free_memory(global);

	if (traceDebug >= 1) {
		UT_DBG_PRINT(("<UT> freeTrace complete\n"));
	}
}

/*
 * @pre Attached to omrthread.
 */
omr_error_t
threadStart(OMR_TraceThread **thr, const void *threadId, const char *threadName, const void *threadSynonym1, const void *threadSynonym2)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	omrthread_monitor_enter(OMR_TRACEGLOBAL(threadPoolLock));
	OMR_TraceThread *newThr = (OMR_TraceThread *)pool_newElement(OMR_TRACEGLOBAL(threadPool));
	if (NULL == newThr) {
		UT_DBGOUT(1, ("<UT> Unable to obtain storage for thread control block \n"));
		*thr = NULL;
		omrthread_monitor_exit(OMR_TRACEGLOBAL(threadPoolLock));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}
	memset(newThr, 0, sizeof(OMR_TraceThread));
	if (NULL == threadName) {
		newThr->name = UT_NO_THREAD_NAME;
	} else {
		/* Obtain storage, copy the thread name, and reference it off OMR_TraceThread */
		char *tempName = (char *)omrmem_allocate_memory(strlen(threadName) + 1, OMRMEM_CATEGORY_TRACE);
		if (NULL != tempName) {
			strcpy(tempName, threadName);
			newThr->name = tempName;
		} else {
			UT_DBGOUT(1, ("<UT> Unable to obtain storage for thread name\n"));
			newThr->name = UT_NO_THREAD_NAME;
		}
	}
	omrthread_monitor_exit(OMR_TRACEGLOBAL(threadPoolLock));

	newThr->id = threadId;
	newThr->synonym1 = threadSynonym1;
	newThr->synonym2 = threadSynonym2;
	newThr->suspendResume = OMR_TRACEGLOBAL(initialSuspendResume);

	VM_AtomicSupport::addU32(&OMR_TRACEGLOBAL(threadCount), 1);

	*thr = newThr;
	UT_DBGOUT(2, ("<UT> Thread started , thread anchor " UT_POINTER_SPEC "\n", *thr));
	UT_DBGOUT(2, ("<UT> thread Id " UT_POINTER_SPEC ", thread name \"%s\", syn1 " UT_POINTER_SPEC ", syn2 " UT_POINTER_SPEC " \n", threadId, threadName, threadSynonym1, threadSynonym2));

	omrthread_tls_set(OS_THREAD_FROM_TRACE_THREAD(newThr), j9uteTLSKey, newThr);

	return rc;
}

omr_error_t
initializeTrace(OMR_VM *vm, const char *datDir, const OMR_TraceLanguageInterface *languageIntf)

{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_TraceGlobal tempGbl;
	OMR_TraceGlobal *newGbl = NULL;
	OMRPORT_ACCESS_FROM_OMRVM(vm);

	/*
	 *  Bootstrap OMR_TraceGlobal using stack variable
	 */
	memset(&tempGbl, 0, sizeof(OMR_TraceGlobal));
	tempGbl.vm = vm;
	tempGbl.portLibrary = vm->_runtime->_portLibrary;

	tempGbl.dynamicBuffers = TRUE;
	tempGbl.bufferSize = UT_DEFAULT_BUFFERSIZE;

	/* Make the trace functions available to the rest of OMR */
	/* OMRTODO Remove this. GC uses it to register the module.
	 * Change tracegen to register modules using the trace engine.
	 */
	((OMR_VM *)vm)->utIntf = &internalUtIntfS;

	/*
	 * Attach global data to thread data
	 */
	omrTraceGlobal = &tempGbl;
	{
		char *envString = getenv(UT_DEBUG);
		if (envString != NULL) {
			if (hexStringLength(envString) == 1 &&
				*envString >= '0' &&
				*envString <= '9') {
				OMR_TRACEGLOBAL(traceDebug) = atoi(envString);
			} else {
				OMR_TRACEGLOBAL(traceDebug) = 9;
			}
		}
	}

	rc = initialiseComponentList(&OMR_TRACEGLOBAL(componentList));
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error initializing component list\n"));
		goto fail;
	}

	rc = initialiseComponentList(&OMR_TRACEGLOBAL(unloadedComponentList));
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error initializing unloaded component list\n"));
		goto fail;
	}

	/*
	 *  Initialize the semaphores in the global data area
	 */
	if (0 != omrthread_monitor_init_with_name(&OMR_TRACEGLOBAL(traceLock), 0, "Global Trace")) {
		UT_DBGOUT(1, ("<UT> Initialization of traceLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&OMR_TRACEGLOBAL(subscribersLock), 0, "Global Record Subscribers")) {
		UT_DBGOUT(1, ("<UT> Initialization of subscribersLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&OMR_TRACEGLOBAL(freeQueueLock), 0, "Global Trace Free Queue")) {
		UT_DBGOUT(1, ("<UT> Initialization of freeQueueLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&OMR_TRACEGLOBAL(bufferPoolLock), 0, "Global Trace Buffer Pool")) {
		UT_DBGOUT(1, ("<UT> Initialization of bufferPoolLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}
	if (0 != omrthread_monitor_init_with_name(&OMR_TRACEGLOBAL(threadPoolLock), 0, "Global Trace Thread Pool")) {
		UT_DBGOUT(1, ("<UT> Initialization of threadPoolLock failed\n"));
		rc = OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR;
		goto fail;
	}

	/*
	 *  Obtain storage for the real OMR_TraceGlobal
	 */

	newGbl = (OMR_TraceGlobal *)omrmem_allocate_memory(sizeof(OMR_TraceGlobal), OMRMEM_CATEGORY_TRACE);
	if (newGbl == NULL) {
		UT_DBGOUT(1, ("<UT> Unable to obtain storage for global control block \n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto fail;
	}

	memcpy(newGbl, &tempGbl, sizeof(OMR_TraceGlobal));
	omrTraceGlobal = newGbl;

	/*
	* Get TLS keys
	*/
	if (omrthread_tls_alloc(&j9uteTLSKey)) {
		/* Unable to allocate UTE thread local storage key */
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto fail;
	}

	if (NULL != languageIntf) {
		memcpy(&OMR_TRACEGLOBAL(languageIntf), languageIntf, sizeof(OMR_TRACEGLOBAL(languageIntf)));
	} else {
		memset(&OMR_TRACEGLOBAL(languageIntf), 0, sizeof(OMR_TRACEGLOBAL(languageIntf)));
	}

	/* Initialize locks and pools for buffers and thread data. */
	OMR_TRACEGLOBAL(bufferPool) = pool_new(OMR_TRACEGLOBAL(bufferSize) + offsetof(OMR_TraceBuffer, record),
										   0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_TRACE, POOL_FOR_PORT(OMRPORTLIB));
	if (NULL == OMR_TRACEGLOBAL(bufferPool)) {
		UT_DBGOUT(1, ("<UT> Unable to obtain storage for trace buffer pool\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto fail;
	}
	OMR_TRACEGLOBAL(threadPool) = pool_new(sizeof(OMR_TraceThread),
										   0, 0, 0, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_TRACE, POOL_FOR_PORT(OMRPORTLIB));
	if (NULL == OMR_TRACEGLOBAL(threadPool)) {
		UT_DBGOUT(1, ("<UT> Unable to obtain storage for trace thread pool\n"));
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto fail;
	}

	/*
	 *  Set the trace start time
	 */
	setStartTime();

	/* Enable fatal assertions by default */
	OMR_TRACEGLOBAL(fatalassert) = 1;

	rc = setFormat(datDir);
	if (OMR_ERROR_NONE != rc) {
		UT_DBGOUT(1, ("<UT> Error setting trace format directory\n"));
		goto fail;
	}

	OMR_TRACEGLOBAL(traceInCore) = TRUE;

	return OMR_ERROR_NONE;

fail:
	omrTraceGlobal = NULL;
	return rc;
}

/*******************************************************************************
 * name        - trcRegisterRecordSubscriber
 * description - Registers a subscriber that consumes trace records from the
 * 				 write queue.
 * parameters  - thr, description, subscriber, alarm,, userData, start, stop, subscription
 * 				 start: -1 means head, NULL means tail, anything else is a buffer
 *               stop: NULL means don't stop, anything else is a buffer
 * returns     - Success or error code
 ******************************************************************************/
omr_error_t
trcRegisterRecordSubscriber(OMR_TraceThread *thr, const char *description, utsSubscriberCallback subscriber,
							utsSubscriberAlarmCallback alarm, void *userData, UtSubscription **subscriptionReference)
{
	omr_error_t result = OMR_ERROR_NONE;
	size_t descriptionLen = 0;

	if (NULL == subscriber) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (NULL == thr) {
		return OMR_THREAD_NOT_ATTACHED;
	}

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));
	UtSubscription *subscription = (UtSubscription *)omrmem_allocate_memory(sizeof(UtSubscription), OMRMEM_CATEGORY_TRACE);
	if (subscription == NULL) {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Out of memory allocating subscription\n", thr));
		return OMR_ERROR_OUT_OF_NATIVE_MEMORY;
	}

	incrementRecursionCounter(thr);
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Acquiring lock for registration\n", thr));
	omrthread_monitor_enter(OMR_TRACEGLOBAL(subscribersLock));
	getTraceLock(thr);
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock acquired for registration\n", thr));

	if (subscriptionReference != NULL) {
		*subscriptionReference = subscription;
	}

	subscription->subscriber = subscriber;
	subscription->userData = userData;
	subscription->alarm = alarm;
	subscription->next = NULL;
	subscription->prev = NULL;
	if (description == NULL) {
		description = "Trace Subscriber [unnamed]";
	}
	descriptionLen = strlen(description);
	subscription->description = (char *)omrmem_allocate_memory(sizeof(char) * (descriptionLen + 1), OMRMEM_CATEGORY_TRACE);
	if (subscription->description == NULL) {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Out of memory allocating description\n", thr));
		result = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		goto out;
	}
	strcpy(subscription->description, description);

	enlistRecordSubscriber(subscription);

out:
	if (OMR_ERROR_NONE == result) {
		OMR_TRACEGLOBAL(traceInCore) = FALSE;
	}

	if (OMR_ERROR_NONE != result) {
		UT_DBGOUT(1, ("<UT> Error starting trace thread for \"%s\": %i\n", description, result));
		destroyRecordSubscriber(thr, subscription, FALSE);
	}

	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Releasing lock for registration\n", thr));
	freeTraceLock(thr);
	omrthread_monitor_exit(OMR_TRACEGLOBAL(subscribersLock));
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock released for registration\n", thr));
	decrementRecursionCounter(thr);
	return result;
}


/*******************************************************************************
 * name        - trcDeregisterRecordSubscriber
 * description - Deregisters a subscriber currently consuming trace buffers from the
 * 				 write queue
 * parameters  - thr, subscriptionID
 * returns     - Success or error code
 ******************************************************************************/
omr_error_t
trcDeregisterRecordSubscriber(OMR_TraceThread *thr, UtSubscription *subscriptionID)
{
	omr_error_t result = OMR_ERROR_NONE;

	if (NULL == subscriptionID) {
		return OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	if (NULL == thr) {
		return OMR_THREAD_NOT_ATTACHED;
	}

	incrementRecursionCounter(thr);
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Acquiring lock for deregistration\n", thr));
	omrthread_monitor_enter(OMR_TRACEGLOBAL(subscribersLock));
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock acquired for deregistration\n", thr));

	if (findRecordSubscriber(subscriptionID)) {
		getTraceLock(thr);
		destroyRecordSubscriber(thr, subscriptionID, TRUE);
		freeTraceLock(thr);
	} else {
		UT_DBGOUT(1, ("<UT thr="UT_POINTER_SPEC"> Failed to find subscriber to deregister\n", thr));
		result = OMR_ERROR_ILLEGAL_ARGUMENT;
	}
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Releasing lock for deregistration\n", thr));
	omrthread_monitor_exit(OMR_TRACEGLOBAL(subscribersLock));
	UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Lock released for deregistration\n", thr));
	decrementRecursionCounter(thr);
	return result;
}


/*******************************************************************************
 * name        - trcFlushTraceData
 * description - Places in use trace buffers on the write queue and prompts queue
 * 				 processing
 * parameters  - thr, first, last, pause
 * returns     - Success or error code
 ******************************************************************************/
static omr_error_t
trcFlushTraceData(OMR_TraceThread *thr)
{
	return OMR_ERROR_NONE;
}

/*******************************************************************************
 * name        - trcGetTraceMetadata
 * description - Retrieves the metadata needed to initialize the formatter
 * parameters  - data, length
 * returns     - Success or error code
 ******************************************************************************/
static omr_error_t
trcGetTraceMetadata(void **data, int32_t *length)
{
	UtTraceFileHdr *traceHeader;

	/* Ensure we have initialised the trace header */
	if (initTraceHeader() != OMR_ERROR_NONE) {
		return OMR_ERROR_INTERNAL;
	}

	traceHeader = OMR_TRACEGLOBAL(traceHeader);

	if (traceHeader == NULL) {
		return OMR_ERROR_INTERNAL;
	}

	*data = traceHeader;
	*length = traceHeader->header.length;

	return OMR_ERROR_NONE;
}

static omr_error_t
trcSetOptions(OMR_TraceThread *thr, const char *opts[])
{
	return setOptions(thr, opts, TRUE);
}

static void
setStartTime(void)
{

	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));
	uint64_t hires[2], millis[2];
	int i = 0;

	UT_DBGOUT(1, ("<UT> SetStartTime called\n"));

	/*
	 * Try to get the time of the next millisecond rollover
	 */
	millis[0] = (uint64_t)omrtime_current_time_millis();
	hires[0] = (uint64_t)omrtime_hires_clock();
	do {
		i ^= 1;
		millis[i] = (uint64_t)omrtime_current_time_millis();
		hires[i] = (uint64_t)omrtime_hires_clock();
	} while (millis[0] == millis[1]);

	OMR_TRACEGLOBAL(startPlatform) = ((hires[0] >> 1) + (hires[1] >> 1));
	OMR_TRACEGLOBAL(startSystem) = millis[i];

	UT_DBGOUT(1, ("<UT> Start time initialized\n"));
}

omr_error_t
fillInUTInterfaces(UtInterface **utIntf, OMR_TraceInterface *omrTraceIntf, UtModuleInterface *utModuleIntf)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if ((NULL == utIntf) || (NULL == omrTraceIntf) || (NULL == utModuleIntf)) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		/*
		 * Initialize the server interface...
		 */
		memset(omrTraceIntf, 0, sizeof(*omrTraceIntf));
		omrTraceIntf->RegisterRecordSubscriber		= trcRegisterRecordSubscriber;
		omrTraceIntf->DeregisterRecordSubscriber	= trcDeregisterRecordSubscriber;
		omrTraceIntf->FlushTraceData				= trcFlushTraceData;
		omrTraceIntf->GetTraceMetadata				= trcGetTraceMetadata;
		omrTraceIntf->SetOptions					= trcSetOptions;

		/*
		 * Initialise the direct module interface, these are
		 * wrappers to calls within trace that obtain a OMR_TraceThread
		 * before calling the real function.
		 */
		memset(utModuleIntf, 0, sizeof(*utModuleIntf));
		utModuleIntf->Trace           = omrTrace;
		utModuleIntf->TraceInit       = omrTraceInit;
		utModuleIntf->TraceTerm       = omrTraceTerm;

		/*
		 * Make the interfaces available.
		 */
		internalUtIntfS.server = NULL;
		internalUtIntfS.module = utModuleIntf;
		*utIntf = &internalUtIntfS;
	}
	return rc;
}

/**
 * Add a subscription to OMR_TRACEGLOBAL(subscribers).
 *
 * @pre hold OMR_TRACEGLOBAL(subscribersLock)
 */
void
enlistRecordSubscriber(UtSubscription *subscription)
{
	if (OMR_TRACEGLOBAL(subscribers) == NULL) {
		OMR_TRACEGLOBAL(subscribers) = subscription;
	} else {
		subscription->next = (UtSubscription *)OMR_TRACEGLOBAL(subscribers);
		OMR_TRACEGLOBAL(subscribers)->prev = subscription;
		OMR_TRACEGLOBAL(subscribers) = subscription;
	}
}

/**
 * Remove a subscription from OMR_TRACEGLOBAL(subscribers).
 *
 * @pre hold OMR_TRACEGLOBAL(subscribersLock)
 */
void
delistRecordSubscriber(UtSubscription *subscription)
{
	if (subscription->prev != NULL) {
		subscription->prev->next = subscription->next;
	}
	if (subscription->next != NULL) {
		subscription->next->prev = subscription->prev;
	}
	if (subscription->prev == NULL) {
		OMR_TRACEGLOBAL(subscribers) = subscription->next;
	}
	subscription->next = NULL;
	subscription->prev = NULL;
}

/**
 * Free the memory for a subscription.
 *
 * @post subscription is freed, and must not be accessed anymore.
 *
 * This function is used by freeTrace(), when omrTraceGlobal may have been NULLed.
 */
void
deleteRecordSubscriber(OMR_TraceGlobal *global, UtSubscription *subscription)
{
	OMRPORT_ACCESS_FROM_OMRPORT(global->portLibrary);
	omrmem_free_memory(subscription->description);
	omrmem_free_memory(subscription);
}

/**
 * Find a subscription in OMR_TRACEGLOBAL(subscribers).
 *
 * @pre hold OMR_TRACEGLOBAL(subscribersLock)
 */
BOOLEAN
findRecordSubscriber(UtSubscription *subscription)
{
	if (NULL != subscription) {
		volatile UtSubscription *subscriberIter = OMR_TRACEGLOBAL(subscribers);
		for (; NULL != subscriberIter; subscriberIter = subscriberIter->next) {
			if (subscriberIter == subscription) {
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
 * Destroy a record subscriber.
 *
 * The subscriber must be removed from OMR_TRACEGLOBAL(subscribers) and deleted atomically,
 * with respect to other operations on OMR_TRACEGLOBAL(subscribers), to resolve race conditions
 * between multiple threads attempting to destroy the same subscription.
 *
 * @pre hold OMR_TRACEGLOBAL(traceLock)
 * @pre hold OMR_TRACEGLOBAL(subscribersLock)
 * @post subscription is freed, and must not be accessed anymore
 */
omr_error_t
destroyRecordSubscriber(OMR_TraceThread *thr, UtSubscription *subscription, BOOLEAN callAlarm)
{
	omr_error_t result = OMR_ERROR_NONE;

	if (callAlarm) {
		if (NULL != subscription->alarm) {
			subscription->alarm(subscription);
		}
	}

	/*
	 * Delisting is required to prevent multiple threads from attempting to Deregister the subscription.
	 * If multiple callers attempt to deregister the subscriber, only one of them
	 * passes the findRecordSubscriber() test.
	 *
	 * No need to check whether subscription was enlisted, because next and prev ptrs will be NULL
	 * if it was not enlisted.
	 */
	delistRecordSubscriber(subscription);

	if (NULL == OMR_TRACEGLOBAL(subscribers)) {
		OMR_TRACEGLOBAL(traceInCore) = TRUE;
		UT_DBGOUT(5, ("<UT thr="UT_POINTER_SPEC"> Set traceInCore to TRUE\n", thr));
	}

	deleteRecordSubscriber(omrTraceGlobal, subscription);
	return result;
}
