/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <windows.h>
#include <stdlib.h>

#include "omrport.h"
#include "omrthread.h"
#include "omrsignal.h"
#include "omrutil.h"
#include "omrutilbase.h"
#include "ut_omrport.h"

#if defined(OMRPORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

typedef struct OMRWin32AsyncHandlerRecord {
	OMRPortLibrary *portLib;
	omrsig_handler_fn handler;
	void *handler_arg;
	uint32_t flags;
	struct OMRWin32AsyncHandlerRecord *next;
} OMRWin32AsyncHandlerRecord;

static struct {
	uint32_t portLibSignalNo;
	int osSignalNo;
} signalMap[] = {
	{OMRPORT_SIG_FLAG_SIGSEGV, SIGSEGV},
	{OMRPORT_SIG_FLAG_SIGILL, SIGILL},
	{OMRPORT_SIG_FLAG_SIGFPE, SIGFPE},
	{OMRPORT_SIG_FLAG_SIGABRT, SIGABRT},
	{OMRPORT_SIG_FLAG_SIGTERM, SIGTERM},
	{OMRPORT_SIG_FLAG_SIGINT, SIGINT},
	{OMRPORT_SIG_FLAG_SIGQUIT, SIGBREAK},
};

#define ARRAY_SIZE_SIGNALS (NSIG + 1)

typedef void (*win_signal)(int);

/* Store the original signal handler. During shutdown, we need to restore
 * the signal handler to the original OS handler.
 */
static struct {
	win_signal originalHandler;
	uint32_t restore;
} handlerInfo[ARRAY_SIZE_SIGNALS];

/* Keep track of signal counts. */
static volatile uintptr_t signalCounts[ARRAY_SIZE_SIGNALS] = {0};

/* Thread to invoke signal handlers for asynchronous signals. */
static omrthread_t asynchSignalReporterThread;

static OMRWin32AsyncHandlerRecord *asyncHandlerList;
static omrthread_monitor_t asyncMonitor;
static uint32_t asyncThreadCount;
static uint32_t attachedPortLibraries;
/* holds the options set by omrsig_set_options */
static uint32_t signalOptions;

/* key to get the current synchronous signal */
static omrthread_tls_key_t tlsKeyCurrentSignal;

/* Calls to registerSignalHandlerWithOS are synchronized using registerHandlerMonitor */
static omrthread_monitor_t registerHandlerMonitor;

/* wakeUpASyncReporter semaphore coordinates between master async signal handler and
 * async signal reporter thread.
 */
static j9sem_t wakeUpASyncReporter;

/* Used to synchronize shutdown of asynchSignalReporterThread. */
static omrthread_monitor_t asyncReporterShutdownMonitor;

/* Used to indicate start and end of asynchSignalReporterThread termination. */
static uint32_t shutDownASynchReporter;

static uint32_t mapWin32ExceptionToPortlibType(uint32_t exceptionCode);
static uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value);
static void removeAsyncHandlers(OMRPortLibrary *portLibrary);
static void fillInWin32SignalInfo(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, EXCEPTION_POINTERS *exceptionInfo, struct OMRWin32SignalInfo *signalInfo);
static uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category);
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType);
int structuredExceptionHandler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, EXCEPTION_POINTERS *exceptionInfo);
static uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value);
static intptr_t setCurrentSignal(struct OMRPortLibrary *portLibrary, intptr_t signal);

static uint32_t mapOSSignalToPortLib(uint32_t signalNo);
static int mapPortLibSignalToOSSignal(uint32_t portLibSignal);

static int32_t registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, win_signal handler, void **oldOSHandler);
static int32_t initializeSignalTools(OMRPortLibrary *portLibrary);
static void destroySignalTools(OMRPortLibrary *portLibrary);

static void updateSignalCount(int osSignalNo);
static void masterASynchSignalHandler(int osSignalNo);

#if defined(OMR_PORT_ASYNC_HANDLER)
static int J9THREAD_PROC asynchSignalReporter(void *userData);
static void runHandlers(uint32_t asyncSignalFlag, int osSignal);
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

static int32_t registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags, void **oldOSHandler);

static OMRWin32AsyncHandlerRecord *createAsyncHandlerRecord(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags);

static int32_t setReporterPriority(OMRPortLibrary *portLibrary, uintptr_t priority);

uint32_t
omrsig_info(struct OMRPortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (category) {
	case OMRPORT_SIG_SIGNAL:
		return infoForSignal(portLibrary, info, index, name, value);
	case OMRPORT_SIG_GPR:
		return infoForGPR(portLibrary, info, index, name, value);
	case OMRPORT_SIG_CONTROL:
		return infoForControl(portLibrary, info, index, name, value);
	case OMRPORT_SIG_MODULE:
		return infoForModule(portLibrary, info, index, name, value);
	case OMRPORT_SIG_FPR:
	case OMRPORT_SIG_OTHER:
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}
uint32_t
omrsig_info_count(struct OMRPortLibrary *portLibrary, void *info, uint32_t category)
{
	return countInfoInCategory(portLibrary, info, category);
}


int32_t
omrsig_protect(struct OMRPortLibrary *portLibrary,  omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result)
{

	if (0 == (signalOptions & OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		__try {
			*result = fn(portLibrary, fn_arg);
		} __except (structuredExceptionHandler(portLibrary, handler, handler_arg, flags, GetExceptionInformation())) {
			/* this code is only reachable if the handler returned OMRPORT_SIG_EXCEPTION_RETURN */
			*result = 0;
			return OMRPORT_SIG_EXCEPTION_OCCURRED;
		}
	} else {
		/* reduced signals, no protection */
		*result = fn(portLibrary, fn_arg);
	}


	return 0;
}

int32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{
	int32_t rc = 0;
	OMRWin32AsyncHandlerRecord *cursor = NULL;
	OMRWin32AsyncHandlerRecord **previousLink = NULL;

	Trc_PRT_signal_omrsig_set_async_signal_handler_entered(handler, handler_arg, flags);

	rc = registerMasterHandlers(portLibrary, flags, OMRPORT_SIG_FLAG_SIGALLASYNC, NULL);
	if (0 != rc) {
		Trc_PRT_signal_omrsig_set_async_signal_handler_exiting_did_nothing_possible_error(handler, handler_arg, flags);
		return rc;
	}

	omrthread_monitor_enter(asyncMonitor);

	/* Wait until no signals are being reported. */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	/* Is this handler already registered? */
	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;
	while (NULL != cursor) {
		if ((cursor->portLib == portLibrary) && (cursor->handler == handler) && (cursor->handler_arg == handler_arg)) {
			if (0 == flags) {
				*previousLink = cursor->next;
				portLibrary->mem_free_memory(portLibrary, cursor);
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_removed(handler, handler_arg, flags);

				/* If this is the last handler, unregister the handler function. */
				if (NULL == asyncHandlerList) {
					SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
				}
			} else {
				/* Update the listener with the new flags. */
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_added_1(handler, handler_arg, flags);
				cursor->flags |= flags;
			}
			break;
		}
		previousLink = &cursor->next;
		cursor = cursor->next;
	}

	/* Cursor will only be NULL if we failed to find it in the list. */
	if ((NULL == cursor) && (0 != flags)) {
		OMRWin32AsyncHandlerRecord *record = createAsyncHandlerRecord(portLibrary, handler, handler_arg, flags);
		if (NULL != record) {
			/* Add the new record to the end of the list. */
			Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_added_2(handler, handler_arg, flags);
			*previousLink = record;
		} else {
			rc = OMRPORT_SIG_ERROR;
		}
	}

	omrthread_monitor_exit(asyncMonitor);

	Trc_PRT_signal_omrsig_set_async_signal_handler_exiting(handler, handler_arg, flags);
	return rc;

}

int32_t
omrsig_set_single_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t portlibSignalFlag, void **oldOSHandler)
{
	int32_t rc = 0;
	OMRWin32AsyncHandlerRecord *cursor = NULL;
	OMRWin32AsyncHandlerRecord **previousLink = NULL;
	BOOLEAN foundHandler = FALSE;

	Trc_PRT_signal_omrsig_set_single_async_signal_handler_entered(handler, handler_arg, portlibSignalFlag);

	if (0 != portlibSignalFlag) {
		/* For non-zero portlibSignalFlag, check if only one signal bit is set. Otherwise, fail. */
		if (!OMR_IS_ONLY_ONE_BIT_SET(portlibSignalFlag)) {
			Trc_PRT_signal_omrsig_set_single_async_signal_handler_error_multiple_signal_flags_found(portlibSignalFlag);
			return OMRPORT_SIG_ERROR;
		}
	}

	rc = registerMasterHandlers(portLibrary, portlibSignalFlag, OMRPORT_SIG_FLAG_SIGALLASYNC, oldOSHandler);
	if (0 != rc) {
		Trc_PRT_signal_omrsig_set_single_async_signal_handler_exiting_did_nothing_possible_error(rc, handler, handler_arg, portlibSignalFlag);
		return rc;
	}

	omrthread_monitor_enter(asyncMonitor);

	/* Wait until no signals are being reported. */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	/* Is this handler already registered? */
	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;
	while (NULL != cursor) {
		if (cursor->portLib == portLibrary) {
			if ((cursor->handler == handler) && (cursor->handler_arg == handler_arg)) {
				foundHandler = TRUE;
				if (0 == portlibSignalFlag) {
					/* Remove this handler record. */
					*previousLink = cursor->next;
					portLibrary->mem_free_memory(portLibrary, cursor);
					Trc_PRT_signal_omrsig_set_single_async_signal_handler_user_handler_removed(handler, handler_arg, portlibSignalFlag);

					/* If this is the last handler, then unregister the handler function. */
					if (NULL == asyncHandlerList) {
						SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
					}
					break;
				} else {
					/* Update the listener with the new portlibSignalFlag. */
					Trc_PRT_signal_omrsig_set_single_async_signal_handler_user_handler_added_1(handler, handler_arg, portlibSignalFlag);
					cursor->flags |= portlibSignalFlag;
				}
			} else {
				/* Unset the portlibSignalFlag for other handlers. One signal must be associated to only one handler. */
				cursor->flags &= ~portlibSignalFlag;
			}
		}
		previousLink = &cursor->next;
		cursor = cursor->next;
	}

	if (!foundHandler && (0 != portlibSignalFlag)) {
		OMRWin32AsyncHandlerRecord *record = createAsyncHandlerRecord(portLibrary, handler, handler_arg, portlibSignalFlag);
		if (NULL != record) {
			/* Add the new record to the end of the list. */
			Trc_PRT_signal_omrsig_set_single_async_signal_handler_user_handler_added_2(handler, handler_arg, portlibSignalFlag);
			*previousLink = record;
		} else {
			rc = OMRPORT_SIG_ERROR;
		}
	}

	omrthread_monitor_exit(asyncMonitor);

	if (NULL != oldOSHandler) {
		Trc_PRT_signal_omrsig_set_single_async_signal_handler_exiting(rc, handler, handler_arg, portlibSignalFlag, *oldOSHandler);
	} else {
		Trc_PRT_signal_omrsig_set_single_async_signal_handler_exiting(rc, handler, handler_arg, portlibSignalFlag, NULL);
	}

	return rc;
}

uint32_t
omrsig_map_os_signal_to_portlib_signal(struct OMRPortLibrary *portLibrary, uint32_t osSignalValue)
{
	return mapOSSignalToPortLib(osSignalValue);
}

int32_t
omrsig_map_portlib_signal_to_os_signal(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag)
{
	return (int32_t)mapPortLibSignalToOSSignal(portlibSignalFlag);
}

int32_t
omrsig_register_os_handler(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, void *newOSHandler, void **oldOSHandler)
{
	int32_t rc = 0;

	Trc_PRT_signal_omrsig_register_os_handler_entered(portlibSignalFlag, newOSHandler);

	if ((0 == portlibSignalFlag) || !OMR_IS_ONLY_ONE_BIT_SET(portlibSignalFlag)) {
		/* If portlibSignalFlag is 0 or if portlibSignalFlag has multiple signal bits set, then fail. */
		Trc_PRT_signal_omrsig_register_os_handler_invalid_portlibSignalFlag(portlibSignalFlag);
		rc = OMRPORT_SIG_ERROR;
	} else {
		omrthread_monitor_enter(registerHandlerMonitor);
		rc = registerSignalHandlerWithOS(portLibrary, portlibSignalFlag, (win_signal)newOSHandler, oldOSHandler);
		omrthread_monitor_exit(registerHandlerMonitor);
	}

	if (NULL != oldOSHandler) {
		Trc_PRT_signal_omrsig_register_os_handler_exiting(rc, portlibSignalFlag, newOSHandler, *oldOSHandler);
	} else {
		Trc_PRT_signal_omrsig_register_os_handler_exiting(rc, portlibSignalFlag, newOSHandler, NULL);
	}

	return rc;
}

BOOLEAN
omrsig_is_master_signal_handler(struct OMRPortLibrary *portLibrary, void *osHandler)
{
	BOOLEAN rc = FALSE;
	Trc_PRT_signal_omrsig_is_master_signal_handler_entered(osHandler);

	if (osHandler == (void *)masterASynchSignalHandler) {
		rc = TRUE;
	}

	Trc_PRT_signal_omrsig_is_master_signal_handler_exiting(rc);
	return rc;
}

int32_t
omrsig_is_signal_ignored(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, BOOLEAN *isSignalIgnored)
{
	int32_t rc = 0;
	Trc_PRT_signal_omrsig_is_signal_ignored_entered(portlibSignalFlag);

	*isSignalIgnored = FALSE;

	Trc_PRT_signal_omrsig_is_signal_ignored_exiting(rc, *isSignalIgnored);
	return rc;
}

int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags)
{
	uint32_t supportedFlags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;

	Trc_PRT_signal_omrsig_can_protect_entered(flags);

	if (OMR_ARE_NO_BITS_SET(signalOptions, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGALLSYNC;
	}

	if (OMR_ARE_ALL_BITS_SET(supportedFlags, flags)) {
		Trc_PRT_signal_omrsig_can_protect_exiting_is_able_to_protect(supportedFlags);
		return 1;
	}

	Trc_PRT_signal_omrsig_can_protect_exiting_is_not_able_to_protect(supportedFlags);
	return 0;
}


/**
 * Shutdown the signal handling component of the port library
 */
void
omrsig_shutdown(struct OMRPortLibrary *portLibrary)
{
	uint32_t index = 1;
	omrthread_monitor_t globalMonitor = omrthread_global_monitor();

	removeAsyncHandlers(portLibrary);

	omrthread_monitor_enter(globalMonitor);

	if (--attachedPortLibraries == 0) {
#if defined(OMR_PORT_ASYNC_HANDLER)
		/* Terminate asynchSignalReporterThread. */
		omrthread_monitor_enter(asyncReporterShutdownMonitor);
		shutDownASynchReporter = 1;
		j9sem_post(wakeUpASyncReporter);
		while (0 != shutDownASynchReporter) {
			omrthread_monitor_wait(asyncReporterShutdownMonitor);
		}
		omrthread_monitor_exit(asyncReporterShutdownMonitor);
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

		/* Register the original signal handlers, which were overwritten. */
		for (index = 1; index < ARRAY_SIZE_SIGNALS; index++) {
			if (0 != handlerInfo[index].restore) {
				OMRSIG_SIGNAL(index, handlerInfo[index].originalHandler);
				/* record that we no longer have a handler installed with the OS for this signal */
				Trc_PRT_signal_sig_full_shutdown_deregistered_handler_with_OS(portLibrary, index);
				handlerInfo[index].restore = 0;
			}
		}
		destroySignalTools(portLibrary);
	}

	omrthread_monitor_exit(globalMonitor);
}
/**
 * Start up the signal handling component of the port library
 */
int32_t
omrsig_startup(struct OMRPortLibrary *portLibrary)
{
	omrthread_monitor_t globalMonitor = omrthread_global_monitor();
	int32_t result = 0;
	uint32_t index = 1;

	omrthread_monitor_enter(globalMonitor);

	if (attachedPortLibraries++ == 0) {
		/* initialize handlerInfo */
		for (index = 1; index < ARRAY_SIZE_SIGNALS; index++) {
			handlerInfo[index].restore = 0;
		}
		result = initializeSignalTools(portLibrary);
	}

	omrthread_monitor_exit(globalMonitor);

	return result;
}

int
structuredExceptionHandler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, EXCEPTION_POINTERS *exceptionInfo)
{
	uint32_t result;
	uint32_t type;
	intptr_t prevSigType;
	struct OMRWin32SignalInfo signalInfo;

	if ((exceptionInfo->ExceptionRecord->ExceptionCode & (ERROR_SEVERITY_ERROR | APPLICATION_ERROR_MASK)) != ERROR_SEVERITY_ERROR) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	type = mapWin32ExceptionToPortlibType(exceptionInfo->ExceptionRecord->ExceptionCode);
	if (0 == (type & flags)) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	fillInWin32SignalInfo(portLibrary, handler, exceptionInfo, &signalInfo);

	prevSigType = portLibrary->sig_get_current_signal(portLibrary);
	setCurrentSignal(portLibrary, type);

	__try {
		result = handler(portLibrary, signalInfo.type, &signalInfo, handler_arg);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		/* if a recursive exception occurs, ignore it and pass control to the next handler */
		setCurrentSignal(portLibrary, prevSigType);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	setCurrentSignal(portLibrary, prevSigType);
	if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
		return EXCEPTION_CONTINUE_SEARCH;
	} else if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION) {
		return EXCEPTION_CONTINUE_EXECUTION;
	} else /* if (result == OMRPORT_SIG_EXCEPTION_RETURN) */ {
		return EXCEPTION_EXECUTE_HANDLER;
	}

}

static uint32_t
mapWin32ExceptionToPortlibType(uint32_t exceptionCode)
{
	switch (exceptionCode) {

	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		return OMRPORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO;

	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return OMRPORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO;

	case EXCEPTION_INT_OVERFLOW:
		return OMRPORT_SIG_FLAG_SIGFPE_INT_OVERFLOW;

	case EXCEPTION_FLT_OVERFLOW:
	case EXCEPTION_FLT_UNDERFLOW:
	case EXCEPTION_FLT_INVALID_OPERATION:
	case EXCEPTION_FLT_INEXACT_RESULT:
	case EXCEPTION_FLT_DENORMAL_OPERAND:
	case EXCEPTION_FLT_STACK_CHECK:
		return OMRPORT_SIG_FLAG_SIGFPE;

	case EXCEPTION_PRIV_INSTRUCTION:
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		return OMRPORT_SIG_FLAG_SIGILL;

	case EXCEPTION_ACCESS_VIOLATION:
		return OMRPORT_SIG_FLAG_SIGSEGV;

	case EXCEPTION_IN_PAGE_ERROR:
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		return OMRPORT_SIG_FLAG_SIGBUS;

	default:
		return OMRPORT_SIG_FLAG_SIGTRAP;

	}
}

static void
fillInWin32SignalInfo(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, EXCEPTION_POINTERS *exceptionInfo, struct OMRWin32SignalInfo *signalInfo)
{
	memset(signalInfo, 0, sizeof(*signalInfo));

	signalInfo->type = mapWin32ExceptionToPortlibType(exceptionInfo->ExceptionRecord->ExceptionCode);
	signalInfo->handlerAddress = (void *)handler;
	signalInfo->handlerAddress2 = (void *)structuredExceptionHandler;
	signalInfo->ExceptionRecord = exceptionInfo->ExceptionRecord;
	signalInfo->ContextRecord = exceptionInfo->ContextRecord;

	/* module info is filled on demand */
}

static uint32_t
infoForSignal(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	switch (index) {

	case OMRPORT_SIG_SIGNAL_TYPE:
	case 0:
		*name = "J9Generic_Signal_Number";
		*value = &info->type;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_PLATFORM_SIGNAL_TYPE:
	case 1:
		*name = "ExceptionCode";
		*value = &info->ExceptionRecord->ExceptionCode;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_ADDRESS:
	case 2:
		*name = "ExceptionAddress";
		*value = &info->ExceptionRecord->ExceptionAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;

	case 3:
		*name = "ContextFlags";
		*value = &info->ContextRecord->ContextFlags;
		return OMRPORT_SIG_VALUE_32;

	case OMRPORT_SIG_SIGNAL_HANDLER:
	case 4:
		*name = "Handler1";
		*value = &info->handlerAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;

	case 5:
		*name = "Handler2";
		*value = &info->handlerAddress2;
		return OMRPORT_SIG_VALUE_ADDRESS;

	case OMRPORT_SIG_SIGNAL_INACCESSIBLE_ADDRESS:
	case 6:
		/* For EXCEPTION_ACCESS_VIOLATION (very common) we can extract the address
		 * that was being read or written to and whether it was an attempt to read, write
		 * or execute the data at that address.
		 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa363082%28v=vs.85%29.aspx */
		if (EXCEPTION_ACCESS_VIOLATION == info->ExceptionRecord->ExceptionCode) {
			if (EXCEPTION_READ_FAULT == info->ExceptionRecord->ExceptionInformation[0]) {
				*name = "InaccessibleReadAddress";
			} else if (EXCEPTION_WRITE_FAULT == info->ExceptionRecord->ExceptionInformation[0]) {
				*name = "InaccessibleWriteAddress";
			} else if (EXCEPTION_EXECUTE_FAULT == info->ExceptionRecord->ExceptionInformation[0]) {
				*name = "DEPAddress";
			}
			*value = &info->ExceptionRecord->ExceptionInformation[1];
			return OMRPORT_SIG_VALUE_ADDRESS;
		} else {
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}

	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

static uint32_t
infoForGPR(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	if (info->ContextRecord->ContextFlags & CONTEXT_INTEGER) {
		switch (index) {
		case OMRPORT_SIG_GPR_X86_EDI:
		case 0:
			*name = "EDI";
			*value = &info->ContextRecord->Edi;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_X86_ESI:
		case 1:
			*name = "ESI";
			*value = &info->ContextRecord->Esi;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_X86_EAX:
		case 2:
			*name = "EAX";
			*value = &info->ContextRecord->Eax;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_X86_EBX:
		case 3:
			*name = "EBX";
			*value = &info->ContextRecord->Ebx;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_X86_ECX:
		case 4:
			*name = "ECX";
			*value = &info->ContextRecord->Ecx;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_GPR_X86_EDX:
		case 5:
			*name = "EDX";
			*value = &info->ContextRecord->Edx;
			return OMRPORT_SIG_VALUE_ADDRESS;

		default:
			return OMRPORT_SIG_VALUE_UNDEFINED;
		}
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

static uint32_t
infoForControl(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value)
{
	*name = "";

	if (info->ContextRecord->ContextFlags & CONTEXT_CONTROL) {
		switch (index) {
		case OMRPORT_SIG_CONTROL_PC:
		case 0:
			*name = "EIP";
			*value = &info->ContextRecord->Eip;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_CONTROL_SP:
		case 1:
			*name = "ESP";
			*value = &info->ContextRecord->Esp;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_CONTROL_BP:
		case 2:
			*name = "EBP";
			*value = &info->ContextRecord->Ebp;
			return OMRPORT_SIG_VALUE_ADDRESS;

		case OMRPORT_SIG_CONTROL_X86_EFLAGS:
		case 3:
			*name = "EFLAGS";
			*value = &info->ContextRecord->EFlags;
			return OMRPORT_SIG_VALUE_ADDRESS;
		}
	}

	if (info->ContextRecord->ContextFlags & CONTEXT_SEGMENTS) {
		switch (index) {
		case 4:
			*name = "GS";
			*value = &info->ContextRecord->SegGs;
			return OMRPORT_SIG_VALUE_16;
		case 5:
			*name = "FS";
			*value = &info->ContextRecord->SegFs;
			return OMRPORT_SIG_VALUE_16;
		case 6:
			*name = "ES";
			*value = &info->ContextRecord->SegEs;
			return OMRPORT_SIG_VALUE_16;
		case 7:
			*name = "DS";
			*value = &info->ContextRecord->SegDs;
			return OMRPORT_SIG_VALUE_16;
		}
	}

	return OMRPORT_SIG_VALUE_UNDEFINED;
}

static uint32_t
infoForModule(struct OMRPortLibrary *portLibrary, struct OMRWin32SignalInfo *info, int32_t index, const char **name, void **value)
{
	if (info->moduleBaseAddress == NULL) {
		MEMORY_BASIC_INFORMATION mbi;

		if (VirtualQuery((LPBYTE) info->ExceptionRecord->ExceptionAddress, &mbi, sizeof(mbi)) == sizeof(mbi)) {
			if (MEM_FREE == mbi.State) {
				/* misc hack from Advanced Windows (Richter) */
				mbi.AllocationBase = mbi.BaseAddress;
			}

			GetModuleFileName((HINSTANCE) mbi.AllocationBase, info->moduleName, sizeof(info->moduleName));
			info->moduleBaseAddress = mbi.AllocationBase;
			info->offsetInDLL = (uintptr_t) info->ExceptionRecord->ExceptionAddress - (uintptr_t) mbi.AllocationBase;
		}
	}

	*name = "";

	switch (index) {
	case OMRPORT_SIG_MODULE_NAME:
	case 0:
		*name = "Module";
		*value = &info->moduleName;
		return OMRPORT_SIG_VALUE_STRING;
	case 1:
		*name = "Module_base_address";
		*value = &info->moduleBaseAddress;
		return OMRPORT_SIG_VALUE_ADDRESS;
	case 2:
		*name = "Offset_in_DLL";
		*value = &info->offsetInDLL;
		return OMRPORT_SIG_VALUE_32;
	default:
		return OMRPORT_SIG_VALUE_UNDEFINED;
	}
}

static uint32_t
countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category)
{
	void *value;
	const char *name;
	uint32_t count = 0;

	while (portLibrary->sig_info(portLibrary, info, category, count, &name, &value) != OMRPORT_SIG_VALUE_UNDEFINED) {
		count++;
	}

	return count;
}

/**
 * Translate the control signal into an OS signal, update signalCounts
 * and notify asynchSignalReporterThread to execute the associated handlers.
 *
 * @param[in] dwCtrlType the type of control signal received
 *
 * @return TRUE if the control signal is handled. Otherwise, return FALSE.
 */
static BOOL WINAPI
consoleCtrlHandler(DWORD dwCtrlType)
{
	BOOL result = FALSE;
	int osSignalNo = OMRPORT_SIG_ERROR;

	switch (dwCtrlType) {
	case CTRL_BREAK_EVENT:
		osSignalNo = SIGBREAK;
		break;
	case CTRL_C_EVENT:
		osSignalNo = SIGINT;
		break;
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		osSignalNo = SIGTERM;
		break;
	default:
		break;
	}

	if (OMRPORT_SIG_ERROR != osSignalNo) {
		updateSignalCount(osSignalNo);
		result = TRUE;
	}

	return result;
}

static void
removeAsyncHandlers(OMRPortLibrary *portLibrary)
{
	/* clean up the list of async handlers */
	OMRWin32AsyncHandlerRecord *cursor;
	OMRWin32AsyncHandlerRecord **previousLink;

	omrthread_monitor_enter(asyncMonitor);

	/* wait until no signals are being reported */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;
	while (cursor) {
		if (cursor->portLib == portLibrary) {
			*previousLink = cursor->next;
			portLibrary->mem_free_memory(portLibrary, cursor);
			cursor = *previousLink;
		} else {
			previousLink = &cursor->next;
			cursor = cursor->next;
		}
	}

	if (asyncHandlerList == NULL) {
		SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
	}

	omrthread_monitor_exit(asyncMonitor);
}

/**
 * none of the current options are supported by windows IA32
 */
int32_t
omrsig_set_options(struct OMRPortLibrary *portLibrary, uint32_t options)
{
	uint32_t supportedFlags = OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS;

	if (options & ~supportedFlags) {
		return -1;
	}

	signalOptions = options;
	return 0;
}

/* these options should always be 0 */
uint32_t
omrsig_get_options(struct OMRPortLibrary *portLibrary)
{
	return signalOptions;
}

/**
 * Set the priority of the asynchronous signal reporting thread (asynchSignalReporterThread).
 *
 * @param[in] portLibrary the OMR port library
 * @param[in] priority the thread priority
 *
 * @return 0 upon success and non-zero upon failure.
 */
int32_t
omrsig_set_reporter_priority(struct OMRPortLibrary *portLibrary, uintptr_t priority)
{
	int32_t result = 0;

	omrthread_monitor_t globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
	if (attachedPortLibraries > 0) {
		result = setReporterPriority(portLibrary, priority);
	}
	omrthread_monitor_exit(globalMonitor);

	return result;
}


static intptr_t
setCurrentSignal(struct OMRPortLibrary *portLibrary, intptr_t signal)
{
	omrthread_t thisThread = omrthread_self();
	omrthread_tls_set(thisThread, tlsKeyCurrentSignal, (intptr_t *) signal);
	return 1;
}


intptr_t
omrsig_get_current_signal(struct OMRPortLibrary *portLibrary)
{
	omrthread_t thisThread = omrthread_self();
	return (intptr_t) omrthread_tls_get(thisThread, tlsKeyCurrentSignal);
}

/**
 * The OS signal number is converted to the corresponding port library
 * signal flag.
 *
 * @param[in] signalNo OS signal number
 *
 * @return The corresponding port library signal flag on success.
 *         Otherwise, return 0 in case of error.
 */
static uint32_t
mapOSSignalToPortLib(uint32_t signalNo)
{
	uint32_t index = 0;

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); index++) {
		if (signalMap[index].osSignalNo == signalNo) {
			return signalMap[index].portLibSignalNo;
		}
	}

	Trc_PRT_signal_mapOSSignalToPortLib_ERROR_unknown_signal(signalNo);
	return 0;
}


/**
 * The port library signal flag is converted to the corresponding OS signal number.
 *
 * @param portLibSignal the port library signal flag
 *
 * @return The corresponding OS signal number or OMRPORT_SIG_ERROR (-1) if the portLibSignal
 *         can't be mapped.
 */
static int
mapPortLibSignalToOSSignal(uint32_t portLibSignal)
{
	uint32_t index = 0;

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); index++) {
		if (signalMap[index].portLibSignalNo == portLibSignal) {
			return signalMap[index].osSignalNo;
		}
	}

	Trc_PRT_signal_mapPortLibSignalToOSSignal_ERROR_unknown_signal(portLibSignal);
	return OMRPORT_SIG_ERROR;
}

/**
 * Register the signal handler with the OS. Calls to this function must be synchronized using
 * "registerHandlerMonitor". *oldOSHandler points to the old signal handler function. It is only
 * set if oldOSHandler is non-null. During the first registration, the old signal handler is
 * stored in handlerInfo[osSignalNo].originalHandler. The original OS handler must be restored
 * during port library shut down.
 *
 * @param[in] portLibrary the OMR port library
 * @param[in] portLibrarySignalNo the port library signal flag
 * @param[in] handler the OS handler to register
 * @param[out] oldOSHandler points to the old OS handler, if oldOSHandler is non-null
 *
 * @return 0 upon success, non-zero otherwise.
 */
static int32_t
registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, win_signal handler, void **oldOSHandler)
{
    int osSignalNo = mapPortLibSignalToOSSignal(portLibrarySignalNo);
    win_signal localOldOSHandler = NULL;

    /* Don't register a handler for unrecognized OS signals.
     * Unrecognized OS signals are the ones which aren't included in signalMap.
     */
    if (OMRPORT_SIG_ERROR == osSignalNo) {
        return OMRPORT_SIG_ERROR;
    }

    localOldOSHandler = OMRSIG_SIGNAL(osSignalNo, handler);
    if (SIG_ERR == localOldOSHandler) {
        Trc_PRT_signal_registerSignalHandlerWithOS_failed_to_registerHandler(portLibrarySignalNo, osSignalNo, handler);
        return OMRPORT_SIG_ERROR;
    }

    Trc_PRT_signal_registerSignalHandlerWithOS_registeredHandler1(portLibrarySignalNo, osSignalNo, handler, localOldOSHandler);
    if (0 == handlerInfo[osSignalNo].restore) {
        handlerInfo[osSignalNo].originalHandler = localOldOSHandler;
        handlerInfo[osSignalNo].restore = 1;
    }

    if (NULL != oldOSHandler) {
        *oldOSHandler = (void *)localOldOSHandler;
    }

    return 0;
}

/**
 * Initialize signal tools at startup.
 *
 * @param[in] portLibrary the OMR port library
 *
 * @return 0 upon success, non-zero otherwise.
 */
static int32_t
initializeSignalTools(OMRPortLibrary *portLibrary)
{
	if (0 != omrthread_monitor_init_with_name(&asyncMonitor, 0, "portLibrary_omrsig_async_monitor")) {
		goto error;
	}

	if (0 != omrthread_monitor_init_with_name(&registerHandlerMonitor, 0, "portLibrary_omrsig_register_handler_monitor")) {
		goto cleanup1;
	}

	if (0 != j9sem_init(&wakeUpASyncReporter, 0)) {
		goto cleanup2;
	}

	if (0 != omrthread_tls_alloc(&tlsKeyCurrentSignal)) {
		goto cleanup3;
	}

	if (0 != omrthread_monitor_init_with_name(&asyncReporterShutdownMonitor, 0, "portLibrary_omrsig_asynch_reporter_shutdown_monitor")) {
		goto cleanup4;
	}

#if defined(OMR_PORT_ASYNC_HANDLER)
	if (J9THREAD_SUCCESS != createThreadWithCategory(
			&asynchSignalReporterThread,
			256 * 1024,
			J9THREAD_PRIORITY_MAX,
			0,
			&asynchSignalReporter,
			NULL,
			J9THREAD_CATEGORY_SYSTEM_THREAD)
	) {
		goto cleanup5;
	}
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

	return 0;

#if defined(OMR_PORT_ASYNC_HANDLER)
cleanup5:
	omrthread_monitor_destroy(asyncReporterShutdownMonitor);
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */
cleanup4:
	omrthread_tls_free(tlsKeyCurrentSignal);
cleanup3:
	j9sem_destroy(wakeUpASyncReporter);
cleanup2:
	omrthread_monitor_destroy(registerHandlerMonitor);
cleanup1:
	omrthread_monitor_destroy(asyncMonitor);
error:
	return OMRPORT_SIG_ERROR;
}

/**
 * Destroys the signal tools.
 *
 * @param[in] portLibrary the OMR port library
 *
 * @return void
 */
static void
destroySignalTools(OMRPortLibrary *portLibrary)
{
	omrthread_monitor_destroy(asyncMonitor);
	omrthread_monitor_destroy(registerHandlerMonitor);
	j9sem_destroy(wakeUpASyncReporter);
	omrthread_tls_free(tlsKeyCurrentSignal);
	omrthread_monitor_destroy(asyncReporterShutdownMonitor);
}

/**
 * Atomically increment signalCounts[osSignalNo] and notify the async signal reporter thread
 * that a signal is received.
 *
 * @param[in] osSignalNo the integer value of the signal that is raised
 *
 * @return void
 */
static void
updateSignalCount(int osSignalNo)
{
	addAtomic(&signalCounts[osSignalNo], 1);
	j9sem_post(wakeUpASyncReporter);
}

/**
 * Invoke updateSignalCount(osSignalNo), and re-register masterASynchSignalHandler since the
 * masterASynchSignalHandler is unregistered after invocation.
 *
 * @param[in] osSignalNo the integer value of the signal that is raised
 *
 * @return void
 */
static void
masterASynchSignalHandler(int osSignalNo)
{
	updateSignalCount(osSignalNo);

	/* Signal handler is reset after invocation. So, the signal handler needs to
	 * be registered again after it is invoked.
	 */
	OMRSIG_SIGNAL(osSignalNo, masterASynchSignalHandler);
}

#if defined(OMR_PORT_ASYNC_HANDLER)
/**
 * Given a port library signal flag, execute the associated handlers stored
 * within asyncHandlerList (list of OMRWin32AsyncHandlerRecord).
 *
 * @param asyncSignalFlag port library signal flag
 *
 * @return void
 */
static void
runHandlers(uint32_t asyncSignalFlag, int osSignal)
{
	OMRWin32AsyncHandlerRecord *cursor = NULL;

	/* incrementing the asyncThreadCount will prevent the list from being modified while we use it */
	omrthread_monitor_enter(asyncMonitor);
	asyncThreadCount++;
	omrthread_monitor_exit(asyncMonitor);

	cursor = asyncHandlerList;
	while (NULL != cursor) {
		if (OMR_ARE_ANY_BITS_SET(cursor->flags, asyncSignalFlag)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_calling_handler(cursor->portLib, asyncSignalFlag, cursor->handler_arg);
			cursor->handler(cursor->portLib, asyncSignalFlag, NULL, cursor->handler_arg);
		}
		cursor = cursor->next;
	}

	omrthread_monitor_enter(asyncMonitor);
	if (--asyncThreadCount == 0) {
		omrthread_monitor_notify_all(asyncMonitor);
	}
	omrthread_monitor_exit(asyncMonitor);

#if defined(OMRPORT_OMRSIG_SUPPORT)
	if (OMR_ARE_NO_BITS_SET(signalOptions, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
		if (OMRPORT_SIG_ERROR != osSignal) {
			omrsig_handler(osSignal, NULL, NULL);
		}
	}
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */
}

/**
 * This is the main body of the asynchSignalReporterThread. It executes handlers
 * (OMRWin32AsyncHandlerRecord->handler) associated to a signal once a signal
 * is raised.
 *
 * @param[in] userData the user data provided during thread creation
 *
 * @return the return statement won't be executed since the thread exits before
 *         executing the return statement
 */
static int J9THREAD_PROC
asynchSignalReporter(void *userData)
{
	omrthread_set_name(omrthread_self(), "Signal Reporter");

	while (0 == shutDownASynchReporter) {
		int osSignal = 1;
		uint32_t asyncSignalFlag = 0;

		/* determine which signal we've been woken up for */
		for (osSignal = 1; osSignal < ARRAY_SIZE_SIGNALS; osSignal++) {
			uintptr_t signalCount = signalCounts[osSignal];

			if (signalCount > 0) {
				asyncSignalFlag = mapOSSignalToPortLib(osSignal);
				runHandlers(asyncSignalFlag, osSignal);
				subtractAtomic(&signalCounts[osSignal], 1);
				/* j9sem_wait will fall-through for each j9sem_post. We can handle
				 * one signal at a time. Ultimately, all signals will be handled
				 * So, break out of the for loop.
				 */
				break;
			}
		}

		j9sem_wait(wakeUpASyncReporter);

		Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up();
	}

	omrthread_monitor_enter(asyncReporterShutdownMonitor);
	shutDownASynchReporter = 0;
	omrthread_monitor_notify(asyncReporterShutdownMonitor);

	omrthread_exit(asyncReporterShutdownMonitor);

	/* Unreachable. */
	return 0;
}
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

/**
 * Register the master handler for the signals indicated in the flags. Only
 * masterASynchSignalHandler/OMRPORT_SIG_FLAG_SIGALLASYNC is supported
 * on win32. OMRPORT_SIG_FLAG_SIGALLSYNC is not supported on win32.
 *
 * @param[in] flags the flags that we want signals for
 * @param[in] allowedSubsetOfFlags must be OMRPORT_SIG_FLAG_SIGALLASYNC for
 *            asynchronous signals. OMRPORT_SIG_FLAG_SIGALLSYNC is not
 *            supported on win32.
 * @param[out] *oldOSHandler points to the old signal handler function, if
 *             oldOSHandler is non-null
 *
 * @return	0 upon success; OMRPORT_SIG_ERROR otherwise.
 *			Possible failure scenarios include:
 *			1) Master handlers are not registered if -Xrs is set.
 *			2) Attempting to register a handler for a signal that is not included
 *			   in the allowedSubsetOfFlags.
 *			3) Failure to register the OS signal handler.
 */
static int32_t
registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags, void **oldOSHandler)
{
	int32_t rc = 0;
	uint32_t flagsSignalsOnly = (flags & allowedSubsetOfFlags);
	win_signal handler = NULL;

	if (OMR_ARE_ALL_BITS_SET(signalOptions, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		/* Do not install any handlers if -Xrs is set. */
		return OMRPORT_SIG_ERROR;
	}

	if (OMRPORT_SIG_FLAG_SIGALLASYNC == allowedSubsetOfFlags) {
		handler = masterASynchSignalHandler;
	} else {
		return OMRPORT_SIG_ERROR;
	}

	if (0 != flagsSignalsOnly) {
		/* registering some handlers */
		uint32_t portSignalType = 0;

		/* OMRPORT_SIG_SMALLEST_SIGNAL_FLAG represents the smallest signal
		 * flag. portSignalType is initialized to the smallest signal flag
		 * in order to avoid non-signal flags. Any non-signal flags greater
		 * than the smallest signal flag are ignored via a bitwise-and
		 * operation with allowedSubsetOfFlags. allowedSubsetOfFlags either
		 * represents all synchronous signal flags (OMRPORT_SIG_FLAG_SIGALLSYNC)
		 * or all asynchronous signal flags (OMRPORT_SIG_FLAG_SIGALLASYNC).
		 */
		omrthread_monitor_enter(registerHandlerMonitor);
		for (portSignalType = OMRPORT_SIG_SMALLEST_SIGNAL_FLAG; ((portSignalType < allowedSubsetOfFlags) && (portSignalType != 0)); portSignalType = portSignalType << 1) {
			/* Iterate through all the  signals and register the master handler for the signals
			 * specified in flagsSignalsOnly.
			 */
			if (OMR_ARE_ALL_BITS_SET(flagsSignalsOnly, portSignalType)) {
				if (0 != registerSignalHandlerWithOS(portLibrary, portSignalType, handler, oldOSHandler)) {
					rc = OMRPORT_SIG_ERROR;
					break;
				}
			}
		}
		omrthread_monitor_exit(registerHandlerMonitor);
	}

	return rc;
}

/**
 * Create a new async handler record. If this is the first handler in asyncHandlerList,
 * then register consoleCtrlHandler (HandlerRoutine) using SetConsoleCtrlHandler.
 *
 * @param[in] portLibrary The OMR port library
 * @param[in] handler the function to call if an asynchronous signal arrives
 * @param[in] handler_arg the argument to handler
 * @param[in] flags indicates the asynchronous signals handled
 *
 * @return pointer to the new async handler record on success and NULL on failure.
 */
static OMRWin32AsyncHandlerRecord *
createAsyncHandlerRecord(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{
	OMRWin32AsyncHandlerRecord *record = (OMRWin32AsyncHandlerRecord *)portLibrary->mem_allocate_memory(portLibrary, sizeof(*record), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

	if (NULL != record) {
		record->portLib = portLibrary;
		record->handler = handler;
		record->handler_arg = handler_arg;
		record->flags = flags;
		record->next = NULL;

		/* If this is the first handler, register the handler function. */
		if (NULL == asyncHandlerList) {
			SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
		}
	}

	return record;
}

/**
 * Set the thread priority for asynchronous signal reporting thread (asynchSignalReporterThread).
 *
 * @param[in] portLibrary the OMR port library
 * @param[in] priority the thread priority
 *
 * @return 0 upon success and non-zero upon failure.
 */
static int32_t
setReporterPriority(OMRPortLibrary *portLibrary, uintptr_t priority)
{
	Trc_PRT_signal_setReporterPriority(portLibrary, priority);

	if (NULL == asynchSignalReporterThread) {
		return OMRPORT_SIG_ERROR;
	}

	return (int32_t)omrthread_set_priority(asynchSignalReporterThread, priority);
}
