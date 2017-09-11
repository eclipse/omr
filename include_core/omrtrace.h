/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

#ifndef OMR_TRACE_H_INCLUDED
#define OMR_TRACE_H_INCLUDED

/*
 * @ddr_namespace: default
 */

#include "ute_core.h"

#ifdef  __cplusplus
extern "C" {
#endif

#define OMR_TRACE_THREAD_FROM_VMTHREAD(thr) ((thr) ? (thr)->_trace.omrTraceThread : NULL)

/*
 * =============================================================================
 * Keywords for trace command line options.
 * =============================================================================
 */

#define UT_DEBUG_KEYWORD              "DEBUG"
#define UT_FORMAT_KEYWORD             "FORMAT"
#define UT_SUFFIX_KEYWORD             "SUFFIX"
#define UT_LIBPATH_KEYWORD            "LIBPATH"
#define UT_BUFFERS_KEYWORD            "BUFFERS"
#define UT_MINIMAL_KEYWORD            "MINIMAL"
#define UT_MAXIMAL_KEYWORD            "MAXIMAL"
#define UT_COUNT_KEYWORD              "COUNT"
#define UT_PRINT_KEYWORD              "PRINT"
#define UT_IPRINT_KEYWORD             "IPRINT"
#define UT_EXCEPTION_KEYWORD          "EXCEPTION"
#define UT_NONE_KEYWORD               "NONE"
#define UT_OUTPUT_KEYWORD             "OUTPUT" /* deprecated */
#define UT_LEVEL_KEYWORD              "LEVEL"
#define UT_SUSPEND_KEYWORD            "SUSPEND"
#define UT_RESUME_KEYWORD             "RESUME"
#define UT_RESUME_COUNT_KEYWORD       "RESUMECOUNT"
#define UT_SUSPEND_COUNT_KEYWORD      "SUSPENDCOUNT"
#define UT_ALL                        "ALL"
#define UT_BACKTRACE                  "BACKTRACE"
#define UT_FATAL_ASSERT_KEYWORD       "FATALASSERT"
#define UT_NO_FATAL_ASSERT_KEYWORD    "NOFATALASSERT"

/*
 * =============================================================================
 * WIP omr_trc API for VM implementors
 * =============================================================================
 */
struct OMR_TraceThread;

typedef struct OMR_TraceBuffer {
	struct OMR_TraceBuffer *next;		/* Next thread/freebuffer           */
	volatile uint32_t flags;			/* Flags                            */
	int32_t bufferType;					/* Buffer type                      */
	struct OMR_TraceThread *thr;		/* The thread that last owned this  */
	/* This section written to disk     */
	UtTraceRecord record;				/* Disk record                      */
} OMR_TraceBuffer;

typedef struct OMR_TraceThread {
	const void *id;					/* Thread identifier               */
	const void *synonym1;			/* Alternative thread identifier   */
	const void *synonym2;			/* And another thread identifier   */
	const char *name;				/* Thread name                     */
	unsigned char currentOutputMask;/* Bitmask containing the options
									 * for the tracepoint currently being formatted
									 */
	OMR_TraceBuffer *trcBuf;		/* Trace buffer                    */
	int32_t suspendResume;			/* Suspend / resume count          */
	int recursion;					/* Trace recursion indicator       */
	int indent;						/* Iprint indentation count        */
} OMR_TraceThread;

typedef struct OMR_TraceInterface {
	omr_error_t (*RegisterRecordSubscriber)(struct OMR_TraceThread *thr, const char *description,
		utsSubscriberCallback func, utsSubscriberAlarmCallback alarm,
		void *userData, UtSubscription **record);
	omr_error_t (*DeregisterRecordSubscriber)(struct OMR_TraceThread *thr, UtSubscription *subscriptionID);
	omr_error_t (*FlushTraceData)(struct OMR_TraceThread *thr);
	omr_error_t (*GetTraceMetadata)(void **data, int32_t *length);
	omr_error_t (*SetOptions)(struct OMR_TraceThread *thr, const char *opts[]);
} OMR_TraceInterface;

/*
 * =============================================================================
 * Language interface - embedded in OMR_TraceGlobals
 *
 * This interface supplies callback functions for language-specific behaviour.
 * A callback may be NULL. We will NULL-check each function pointer before invoking it.
 * =============================================================================
 */
typedef void (*OMR_TraceCommandLineErrorFunc)(OMRPortLibrary *portLibrary, const char *detailStr, va_list args);

typedef struct OMR_TraceLanguageInterface {
	/**
	 * Report errors with trace options to the implementor's error stream.
	 */
	OMR_TraceCommandLineErrorFunc ReportCommandLineError;
} OMR_TraceLanguageInterface;

typedef struct OMRTraceEngine {
	UtInterface *utIntf;
	OMR_TraceInterface omrTraceIntfS;
	UtModuleInterface utModuleIntfS;
} OMRTraceEngine;

/**
 * Start tracing support.
 * omr_trc_startup can only be called once during the life of the process. Restarting trace is
 * not supported.
 *
 * @pre attached to omrthread
 *
 * @param[in,out] omrVM The OMR VM
 * @param[in]     languageIntf Language interface (optional)
 * @param[in]     datDir Path containing trace format files (*.dat)
 * @param[in]     opts A NULL-terminated array of trace options. The options must be provided in name / value pairs.
 * 				    If a name has no value, a NULL array entry must be provided.
 * 				    e.g. { "print", "none", NULL } is valid
 * 				    e.g. { "print", NULL } is invalid
 *
 * @return an OMR error code
 */
omr_error_t omr_trc_startup(OMR_VM *omrVM, const OMR_TraceLanguageInterface *languageIntf,
	const char *datDir, const char **opts);

/**
 * Shut down tracing and free the trace engine data before exiting the process
 * when the OMR VM is shutting down.
 * omr_trc_startup cannot be called to restart trace after omr_trc_shutdown has
 * been called.
 *
 * @pre Trace must be enabled for the current thread.
 *
 * @param[in,out] currentThread The current OMR VM thread.
 *
 * @return an OMR error code
 */
omr_error_t omr_trc_shutdown(OMR_VMThread *currentThread);

/**
 * @brief Indicate the trace engine is ready to accept thread attachment.
 *
 * This allows trace modules to be loaded by external features before
 * the current OMR_VMThread has been initialized.
 *
 * @pre omr_trc_startup() has succeeded.
 * @param[in,out] omrVM The OMR VM.
 */
void omr_trc_startMultiThreading(OMR_VM *omrVM);

/**
 * Enable tracing for the current thread.
 *
 * @pre The current thread must be attached to omrthread.
 * @pre The current thread must be attached to the OMR VM.
 *
 * @param[in,out] currentThread The current OMR VM thread.
 *                currentThread, currentThread->_language_vmthread, and currentThread->_os_thread
 *                are used to register the thread. The trace engine's thread identifier will be
 *                saved into a field of currentThread.
 * @param[in]     threadName The current thread's name
 *
 * @return an OMR error code
 */
omr_error_t omr_trc_startThreadTrace(OMR_VMThread *currentThread, const char *threadName);

/**
 * Disable tracing for the current thread.
 *
 * @pre The current thread must be attached to the OMR VM.
 * @pre The current thread must be attached to the trace engine.
 *
 * @param[in,out] currentThread The current OMR VM thread
 *
 * @return an OMR error code
 */
omr_error_t omr_trc_stopThreadTrace(OMR_VMThread *currentThread);

#if defined(OMR_THR_FORK_SUPPORT)

/**
 * This function should be called in the parent process prior to any fork() operation in any
 * environment where OMR expects the runtime to call fork(). Trace initialization and termination
 * concurrent to fork is not supported; these operations must be done at vm start up and shutdown.
 */
void omr_trc_preForkHandler(void);

/**
 * This function should be called in the parent process after any fork() operation in any
 * environment where OMR expects the runtime to call fork().
 */
void omr_trc_postForkParentHandler(void);

/**
 * This function should be called in the child process after any fork() operation in any
 * environment where OMR expects the runtime to call fork().
 */
void omr_trc_postForkChildHandler(void);

#endif /* defined(OMR_THR_FORK_SUPPORT) */

#ifdef  __cplusplus
}
#endif

#endif /* !OMR_TRACE_H_INCLUDED */
