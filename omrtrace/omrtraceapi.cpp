/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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

#include <string.h>

#include "omrport.h"
#include "omr.h"
#include "omragent.h"
#include "omrtrace_internal.h"
#include "pool_api.h"
#include "omrutil.h"

extern const char *UT_NO_THREAD_NAME;

#define OMR_TRACE_THREAD_FROM_VMTHREAD_NONNULL(thr) ((thr)->_trace.omrTraceThread)

/* Calls to control the trace engine life cycle, the sequence is ([] are optional)
 * Note: These functions will be renamed under task 65134
 * 1 - fillInUTInterfaces - Setup function pointers in interfaces via fillInUTInterfaces
 * 2 - initializeTrace 	- Tell trace to intialise via initializeTrace supplying default options.
 * 							(Usually just the location of the .dat file is standard)
 * 3 - threadStart 		- Notify trace about the current thread since it started before trace was available via threadStart
 * 						- Trace should be notified about about threads beginning and ending via calls to threadStart and
 * 						  threadStop when they start and end.
 * [6] - setOptions 	- set any additional default options or process any options specified on the command line.
 * 8 - Initialise port, thread and any other modules that may have loaded before trace was ready.
 * 9 - The real work happens here, hopefully for a long time.
 * 10 - utTerminateTrace - Tell the trace engine to shutdown.
 * 11 - freeTrace - Tell the trace engine to free its internal data structures.
 *
 */

#if defined(OMR_THR_FORK_SUPPORT)
static void postForkCleanupGlobals(OMR_TraceThread *thr);
static void postForkCleanupBuffers(OMR_TraceThread *thr);
static void postForkCleanupThreads(OMR_TraceThread *thr);
#endif /* defined(OMR_THR_FORK_SUPPORT) */

omr_error_t
omr_trc_startup(OMR_VM *omrVM, const OMR_TraceLanguageInterface *languageIntf, const char *datDir, const char **opts)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMRTraceEngine *newTrcEngine = NULL;
	OMRPORT_ACCESS_FROM_OMRVM(omrVM);

	newTrcEngine = (OMRTraceEngine *)omrmem_allocate_memory(sizeof(*newTrcEngine), OMRMEM_CATEGORY_TRACE);
	if (NULL == newTrcEngine) {
		rc = OMR_ERROR_OUT_OF_NATIVE_MEMORY;
		omrtty_printf("omr_trc_startup: Failed to alloc newTrcEngine, rc=%d\n", rc);
		goto done;
	}

	rc = fillInUTInterfaces(&newTrcEngine->utIntf, &newTrcEngine->omrTraceIntfS, &newTrcEngine->utModuleIntfS);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("omr_trc_startup: fillInUTInterfaces failed, rc=%d\n", rc);
		goto done;
	}

	/* Set the path to the .dat file or -Xtrace:print doesn't work. */
	rc = initializeTrace(omrVM, datDir, languageIntf);
	if (OMR_ERROR_NONE != rc) {
		omrtty_printf("omr_trc_startup: initializeTrace failed, rc=%d\n", rc);
		goto done;
	}

	OMR_TRACEGLOBAL(initState) = OMR_TRACE_ENGINE_ENABLED;
	if (NULL != opts) {
		rc = setOptions(NULL, opts, FALSE);
		if (OMR_ERROR_NONE != rc) {
			omrtty_printf("omr_trc_startup: failed to set trace options, rc=%d\n", rc);
			goto done;
		}
	}

	omrVM->_trcEngine = newTrcEngine;
done:
	return rc;
}

omr_error_t
omr_trc_shutdown(OMR_VMThread *currentThread)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_TraceThread **thrSlot = &OMR_TRACE_THREAD_FROM_VMTHREAD_NONNULL(currentThread);

	if (NULL == *thrSlot) {
		/* The current thread is not attached to the trace engine. */
		rc = OMR_THREAD_NOT_ATTACHED;
	} else {
		/*
		 * Shut trace down.
		 * Because the current thread must be attached to the trace engine, we can
		 * guarantee that at least one exiting thread will notice the shutdown state
		 * and free the trace engine data.
		 */
		rc = utTerminateTrace();
		if (OMR_ERROR_NONE == rc) {
			rc = threadStop(thrSlot);
		}
	}
	return rc;
}

void
omr_trc_startMultiThreading(OMR_VM *omrVM)
{
	if (omrVM->_trcEngine) {
		OMR_TRACEGLOBAL(initState) = OMR_TRACE_ENGINE_MT_ENABLED;
	}
}

omr_error_t
omr_trc_startThreadTrace(OMR_VMThread *currentThread, const char *threadName)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL != omrTraceGlobal) {
		if (OMR_TRACE_ENGINE_MT_ENABLED != OMR_TRACEGLOBAL(initState)) {
			rc = OMR_ERROR_NOT_AVAILABLE;
		} else {
			rc = threadStart(&OMR_TRACE_THREAD_FROM_VMTHREAD_NONNULL(currentThread),
							 ((NULL == currentThread->_language_vmthread)? currentThread : currentThread->_language_vmthread),
							 threadName,
							 currentThread->_os_thread,
							 currentThread);
		}
	}
	return rc;
}

omr_error_t
omr_trc_stopThreadTrace(OMR_VMThread *currentThread)
{
	omr_error_t rc = OMR_ERROR_NONE;

	if (NULL == currentThread) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		OMR_TraceThread **thrSlot = &OMR_TRACE_THREAD_FROM_VMTHREAD_NONNULL(currentThread);
		if (NULL == *thrSlot) {
			/* Do nothing if the thread is not attached to the trace engine. */
		} else {
			/* It's safe to read omrTraceGlobal because at least one thread is attached to the trace engine.
			 * OMRTODO The initState check may not be needed here.
			 */
			if ((NULL != omrTraceGlobal) && (OMR_TRACEGLOBAL(initState) >= OMR_TRACE_ENGINE_MT_ENABLED)) {
				rc = threadStop(thrSlot);
			}
		}
	}
	return rc;
}

#if defined(OMR_THR_FORK_SUPPORT)

void
omr_trc_preForkHandler(void)
{
	if ((NULL != omrTraceGlobal) && (OMR_TRACE_ENGINE_MT_ENABLED == OMR_TRACEGLOBAL(initState))) {
		/* Disable trace points until the post fork handler is called.
		 * Trace must be disabled while the monitors are held.
		 */
		OMR_TraceThread *thr = twThreadSelf();
		if (NULL != thr) {
			incrementRecursionCounter(thr);
		}

		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: requesting global subscribers lock.\n"));
		omrthread_monitor_enter(OMR_TRACEGLOBAL(subscribersLock));
		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: obtained global subscribers lock.\n"));

		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: requesting global trace lock.\n"));
		omrthread_monitor_enter(OMR_TRACEGLOBAL(traceLock));
		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: obtained global trace lock.\n"));

		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: requesting global thread pool lock.\n"));
		omrthread_monitor_enter(OMR_TRACEGLOBAL(threadPoolLock));
		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: obtained global thread pool lock.\n"));

		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: requesting global buffer pool lock.\n"));
		omrthread_monitor_enter(OMR_TRACEGLOBAL(bufferPoolLock));
		UT_DBGOUT(1, ("<UT> omr_trc_preForkHandler: obtained global buffer pool lock.\n"));
	}
}

void
omr_trc_postForkParentHandler(void)
{
	if ((NULL != omrTraceGlobal) && (OMR_TRACE_ENGINE_MT_ENABLED == OMR_TRACEGLOBAL(initState))) {
		omrthread_monitor_exit(OMR_TRACEGLOBAL(bufferPoolLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkParentHandler: released global buffer pool lock.\n"));

		omrthread_monitor_exit(OMR_TRACEGLOBAL(threadPoolLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkParentHandler: released global thread pool lock.\n"));

		omrthread_monitor_exit(OMR_TRACEGLOBAL(traceLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkParentHandler: released global trace lock.\n"));

		omrthread_monitor_exit(OMR_TRACEGLOBAL(subscribersLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkParentHandler: released global subscribers lock.\n"));

		/* This is after exiting the monitors to prevent any
		 * trace points while holding those locks.
		 */
		OMR_TraceThread *thr = twThreadSelf();
		if (NULL != thr) {
			decrementRecursionCounter(thr);
		}
	}
}

void
omr_trc_postForkChildHandler(void)
{
	if ((NULL != omrTraceGlobal) && (OMR_TRACE_ENGINE_MT_ENABLED == OMR_TRACEGLOBAL(initState))) {
		omrthread_monitor_exit(OMR_TRACEGLOBAL(bufferPoolLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkParentHandler: released global buffer pool lock.\n"));

		omrthread_monitor_exit(OMR_TRACEGLOBAL(threadPoolLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkParentHandler: released global thread pool lock.\n"));

		omrthread_monitor_exit(OMR_TRACEGLOBAL(traceLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkChildHandler: released global trace lock.\n"));

		omrthread_monitor_exit(OMR_TRACEGLOBAL(subscribersLock));
		UT_DBGOUT(1, ("<UT> omr_trc_postForkParentHandler: released global subscribers lock.\n"));

		OMR_TraceThread *thr = twThreadSelf();
		postForkCleanupGlobals(thr);
		postForkCleanupBuffers(thr);

		/* This is after exiting the pool monitors to prevent any
		 * trace points while holding those locks.
		 */
		if (NULL != thr) {
			decrementRecursionCounter(thr);
		}
		postForkCleanupThreads(thr);
	}
}

void
postForkCleanupGlobals(OMR_TraceThread *thr)
{
	/* Reset OMR_TRACEGLOBALs. */
	if (NULL != thr) {
		OMR_TRACEGLOBAL(threadCount) = 1;
	} else {
		OMR_TRACEGLOBAL(threadCount) = 0;
	}
	OMR_TRACEGLOBAL(lastPrint) = NULL;
	OMR_TRACEGLOBAL(lostRecords) = 0;
#if OMR_ENABLE_EXCEPTION_OUTPUT
	OMR_TRACEGLOBAL(exceptionTrcBuf) = NULL;
	OMR_TRACEGLOBAL(exceptionContext) = NULL;
#endif /* OMR_ENABLE_EXCEPTION_OUTPUT */
}

void
postForkCleanupBuffers(OMR_TraceThread *thr)
{
	/* Clear all buffers in the pool and in freeQueue. */
	OMR_TRACEGLOBAL(freeQueue) = NULL;
	if (NULL != thr) {
		thr->trcBuf = NULL;
	}
	pool_clear(OMR_TRACEGLOBAL(bufferPool));
}

void
postForkCleanupThreads(OMR_TraceThread *thr)
{
	J9PoolState threadPoolState;
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));

	/* Cleanup and free the name and OMR_TraceThread for all threads which do not exist after fork. */
	OMR_TraceThread *iteratingThread = (OMR_TraceThread *)pool_startDo(OMR_TRACEGLOBAL(threadPool), &threadPoolState);
	while (NULL != iteratingThread) {
		if (thr != iteratingThread) {
			if ((NULL != iteratingThread->name) && (UT_NO_THREAD_NAME != iteratingThread->name)) {
				omrmem_free_memory((void *)iteratingThread->name);
			}
			pool_removeElement(OMR_TRACEGLOBAL(threadPool), iteratingThread);
		}
		iteratingThread = (OMR_TraceThread *)pool_nextDo(&threadPoolState);
	}
}

#endif /* defined(OMR_THR_FORK_SUPPORT) */
