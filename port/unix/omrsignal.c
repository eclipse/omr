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

/**
 * @file
 * @ingroup Port
 * @brief Signal handling
 */
#include "omrcfg.h"
#include "omrport.h"
#include "omrutil.h"
#include "omrutilbase.h"
#include "omrportpriv.h"
#include "ut_omrport.h"
#include "omrthread.h"
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <errno.h>
#include "omrsignal_context.h"

#if !defined(J9ZOS390)
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
#include "semaphore_i5.h"
#else /* defined(J9OS_I5) && defined(J9OS_I5_V5R4) */
#include <semaphore.h>
#endif /* defined(J9OS_I5) && defined(J9OS_I5_V5R4) */
#endif /* !defined(J9ZOS390) */

#if defined(J9ZOS390)
#include <pthread.h>
#endif /* defined(J9ZOS390) */

#if defined(J9ZOS390)
#if defined(OMR_ENV_DATA64)
#include <__le_api.h>
#else /* defined(OMR_ENV_DATA64) */
#include <leawi.h>
#include <ceeedcct.h>
#endif /* defined(OMR_ENV_DATA64) */

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
#include "omrsignal_ceehdlr.h"
#endif /* defined(OMR_PORT_ZOS_CEEHDLRSUPPORT) */
#endif /* defined(J9ZOS390) */

#if defined(OMRPORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

#if defined(S390) && defined(LINUX)
typedef void (*unix_sigaction)(int, siginfo_t *, void *, uintptr_t);
#else /* defined(S390) && defined(LINUX) */
typedef void (*unix_sigaction)(int, siginfo_t *, void *);
#endif /* defined(S390) && defined(LINUX) */

#define ARRAY_SIZE_SIGNALS  (MAX_UNIX_SIGNAL_TYPES + 1)

/* Keep track of signal counts. */
static volatile uintptr_t signalCounts[ARRAY_SIZE_SIGNALS] = {0};

/* Store the previous signal handlers. We need to restore them during shutdown. */
static struct {
	struct sigaction action;
	uint32_t restore;
}	oldActions[ARRAY_SIZE_SIGNALS];

/* Records the (port library defined) signals for which a handler is registered.
 * Access to these variables must be protected by the registerHandlerMonitor.
 *
 * syncSignalsWithHandlers represents if handlers are registered with synchronous
 * signals, and asyncSignalsWithHandlers represents if handlers are registered with
 * asynchronous signals.
 */
static uint32_t syncSignalsWithHandlers;
static uint32_t asyncSignalsWithHandlers;

/* Records the (port library defined) signals for which a master handler is
 * registered. A master handler can be either masterSynchSignalHandler or
 * masterASynchSignalHandler. A signal can only be associated to one master
 * handler. If a master handler is already registered for a signal, then avoid
 * re-registering a master handler for that signal. Access to these variables
 * must be protected by the registerHandlerMonitor.
 *
 * syncSignalsWithMasterHandlers represents if the synchronous master handler is
 * registered with synchronous signals, and asyncSignalsWithMasterHandlers
 * represents if the asynchronous master handler is registered with asynchronous
 * signals.
 */
static uint32_t syncSignalsWithMasterHandlers;
static uint32_t asyncSignalsWithMasterHandlers;

#if defined(OMR_PORT_ASYNC_HANDLER)
static uint32_t shutDownASynchReporter;
#endif

static uint32_t attachedPortLibraries;

typedef struct OMRUnixAsyncHandlerRecord {
	OMRPortLibrary *portLib;
	omrsig_handler_fn handler;
	void *handler_arg;
	uint32_t flags;
	struct OMRUnixAsyncHandlerRecord *next;
} OMRUnixAsyncHandlerRecord;

/* holds the options set by omrsig_set_options */
uint32_t signalOptionsGlobal;

static OMRUnixAsyncHandlerRecord *asyncHandlerList;

#if !defined(J9ZOS390)

#if defined(OSX)
#define SIGSEM_T sem_t *
#define SIGSEM_POST(_sem) sem_post(_sem)
#define SIGSEM_ERROR SEM_FAILED
#define SIGSEM_INIT(_sem, _name) ((_sem) = sem_open((_name), O_CREAT|O_EXCL, S_IRWXU, 0))
#define SIGSEM_UNLINK(_name) sem_unlink(_name)
#define SIGSEM_DESTROY(_sem) sem_close(_sem)
#define SIGSEM_WAIT(_sem) sem_wait(_sem)
#define SIGSEM_TRY_WAIT(_sem) sem_trywait(_sem)
#else /* defined(OSX) */
#define SIGSEM_T sem_t
#define SIGSEM_POST(_sem) sem_post(&(_sem))
#define SIGSEM_ERROR -1
#define SIGSEM_INIT(_sem, _name) sem_init(&(_sem), 0, 0)
#define SIGSEM_UNLINK(_name) do { /* do nothing */ } while (0)
#define SIGSEM_DESTROY(_sem) sem_destroy(&(_sem))
#define SIGSEM_WAIT(_sem) sem_wait(&(_sem))
#define SIGSEM_TRY_WAIT(_sem) sem_trywait(&(_sem))
#endif /* defined(OSX) */

static SIGSEM_T wakeUpASyncReporter;
#else /* !defined(J9ZOS390) */
/* The asyncSignalReporter synchronization has to be done differently on ZOS.
 * z/OS does not have system semaphores, yet we can't rely on thread library
 * being present for the masterASyncSignalHandler, so use pthread synchronization.
 * Note: the use of pthread functions is allowed on z/OS
 */
static pthread_cond_t wakeUpASyncReporterCond;
static pthread_mutex_t wakeUpASyncReporterMutex;
#endif /* !defined(J9ZOS390) */

static omrthread_monitor_t asyncMonitor;
static omrthread_monitor_t registerHandlerMonitor;
static omrthread_monitor_t asyncReporterShutdownMonitor;
static uint32_t asyncThreadCount;
static uint32_t attachedPortLibraries;

struct OMRSignalHandlerRecord {
	struct OMRSignalHandlerRecord *previous;
	struct OMRPortLibrary *portLibrary;
	omrsig_handler_fn handler;
	void *handler_arg;
	sigjmp_buf returnBuf;
#if defined(J9ZOS390)
	struct __jumpinfo farJumpInfo;
#endif /* defined(J9ZOS390) */
	uint32_t flags;
} OMRSignalHandlerRecord;

typedef struct OMRCurrentSignal {
	int signal;
	siginfo_t *sigInfo;
	void *contextInfo;
#if defined(S390) && defined(LINUX)
	uintptr_t breakingEventAddr;
#endif /* defined(S390) && defined(LINUX) */
	uint32_t portLibSignalType;
} OMRCurrentSignal;

/* key to get the end of the synchronous handler records */
static omrthread_tls_key_t tlsKey;

/* key to get the current synchronous signal */
static omrthread_tls_key_t tlsKeyCurrentSignal;

static struct {
	uint32_t portLibSignalNo;
	int unixSignalNo;
} signalMap[] = {
	{OMRPORT_SIG_FLAG_SIGSEGV, SIGSEGV},
	{OMRPORT_SIG_FLAG_SIGBUS, SIGBUS},
	{OMRPORT_SIG_FLAG_SIGILL, SIGILL},
	{OMRPORT_SIG_FLAG_SIGFPE, SIGFPE},
	{OMRPORT_SIG_FLAG_SIGTRAP, SIGTRAP},
	{OMRPORT_SIG_FLAG_SIGQUIT, SIGQUIT},
	{OMRPORT_SIG_FLAG_SIGABRT, SIGABRT},
	{OMRPORT_SIG_FLAG_SIGTERM, SIGTERM},
	{OMRPORT_SIG_FLAG_SIGXFSZ, SIGXFSZ},
	{OMRPORT_SIG_FLAG_SIGINT, SIGINT},
	{OMRPORT_SIG_FLAG_SIGHUP, SIGHUP},
	{OMRPORT_SIG_FLAG_SIGCONT, SIGCONT},
	{OMRPORT_SIG_FLAG_SIGWINCH, SIGWINCH},
	{OMRPORT_SIG_FLAG_SIGPIPE, SIGPIPE},
	{OMRPORT_SIG_FLAG_SIGALRM, SIGALRM},
	{OMRPORT_SIG_FLAG_SIGCHLD, SIGCHLD},
	{OMRPORT_SIG_FLAG_SIGTSTP, SIGTSTP},
	{OMRPORT_SIG_FLAG_SIGUSR1, SIGUSR1},
	{OMRPORT_SIG_FLAG_SIGUSR2, SIGUSR2},
	{OMRPORT_SIG_FLAG_SIGURG, SIGURG},
	{OMRPORT_SIG_FLAG_SIGXCPU, SIGXCPU},
	{OMRPORT_SIG_FLAG_SIGVTALRM, SIGVTALRM},
	{OMRPORT_SIG_FLAG_SIGPROF, SIGPROF},
	{OMRPORT_SIG_FLAG_SIGIO, SIGIO},
	{OMRPORT_SIG_FLAG_SIGSYS, SIGSYS},
	{OMRPORT_SIG_FLAG_SIGTTIN, SIGTTIN},
	{OMRPORT_SIG_FLAG_SIGTTOU, SIGTTOU}
#if defined(SIGINFO)
	, {OMRPORT_SIG_FLAG_SIGINFO, SIGINFO}
#endif /* defined(SIGINFO) */
#if defined(SIGIOT)
	, {OMRPORT_SIG_FLAG_SIGIOT, SIGIOT}
#endif /* defined(SIGIOT) */
#if defined(SIGPOLL)
	, {OMRPORT_SIG_FLAG_SIGPOLL, SIGPOLL}
#endif /* defined(SIGPOLL) */
#if defined(SIGRECONFIG)
	, {OMRPORT_SIG_FLAG_SIGRECONFIG, SIGRECONFIG}
#endif /* defined(SIGRECONFIG) */
#if defined(SIGABND)
	, {OMRPORT_SIG_FLAG_SIGABEND, SIGABND}
#endif /* defined(SIGABND) */
};

static omrthread_t asynchSignalReporterThread = NULL;
static int32_t registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags, void **oldOSHandler);
static void removeAsyncHandlers(OMRPortLibrary *portLibrary);
static uint32_t mapOSSignalToPortLib(uint32_t signalNo, siginfo_t *sigInfo);
#if defined(J9ZOS390)
static intptr_t addAsyncSignalsToSet(sigset_t *ss);
#endif /* defined(J9ZOS390) */

#if defined(OMR_PORT_ASYNC_HANDLER)
static void runHandlers(uint32_t asyncSignalFlag, int unixSignal);
static int J9THREAD_PROC asynchSignalReporter(void *userData);
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

static int32_t registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, unix_sigaction handler, void **oldOSHandler);
static uint32_t destroySignalTools(OMRPortLibrary *portLibrary);
static int mapPortLibSignalToOSSignal(uint32_t portLibSignal);
static uint32_t countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category);
static void sig_full_shutdown(struct OMRPortLibrary *portLibrary);
static int32_t initializeSignalTools(OMRPortLibrary *portLibrary);

#if defined(S390) && defined(LINUX)
static void masterSynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo, uintptr_t breakingEventAddr);
static void masterASynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo, uintptr_t nullArg);
#else /* defined(S390) && defined(LINUX) */
static void masterSynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo);
static void masterASynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo);
#endif /* defined(S390) && defined(LINUX) */

static int32_t unblockSignal(int signal);

static void setBitMaskSignalsWithHandlers(uint32_t flags);
static void unsetBitMaskSignalsWithHandlers(uint32_t flags);

static void setBitMaskSignalsWithMasterHandlers(uint32_t flags);
static void unsetBitMaskSignalsWithMasterHandlers(uint32_t flags);

static BOOLEAN checkForAmbiguousSignalFlags(uint32_t flags, const char *functionName);

int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags)
{
	uint32_t supportedFlags = OMRPORT_SIG_FLAG_MAY_RETURN;

	Trc_PRT_signal_omrsig_can_protect_entered(flags);

	if (checkForAmbiguousSignalFlags(flags, "omrsig_can_protect")) {
		return OMRPORT_SIG_ERROR;
	}

#if !defined(J9ZOS390)
	supportedFlags |= OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;
#else /* !defined(J9ZOS390) */
	if (TRUE == PPG_resumableTrapsSupported) {
		Trc_PRT_sig_can_protect_OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION_supported();
		supportedFlags |= OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;
	} else {
		Trc_PRT_sig_can_protect_OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION_NOT_supported();
	}
#endif /* !defined(J9ZOS390) */

	if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGALLSYNC;
	}

	if (OMR_ARE_ALL_BITS_SET(supportedFlags, flags)) {
		Trc_PRT_signal_omrsig_can_protect_exiting_is_able_to_protect(supportedFlags);
		return 1;
	}

	Trc_PRT_signal_omrsig_can_protect_exiting_is_not_able_to_protect(supportedFlags);
	return 0;
}

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
		return infoForFPR(portLibrary, info, index, name, value);
#if defined(J9ZOS390)
	case OMRPORT_SIG_VR:
		if (portLibrary->portGlobals->vectorRegsSupportOn) {
			return infoForVR(portLibrary, info, index, name, value);
		}
#endif /* defined(J9ZOS390) */
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

/**
 * We register the master signal handlers here to deal with -Xrs
 */
int32_t
omrsig_protect(struct OMRPortLibrary *portLibrary, omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result)
{
	struct OMRSignalHandlerRecord thisRecord = {0};
	omrthread_t thisThread = NULL;
	uint32_t flagsSignalsOnly = flags & OMRPORT_SIG_FLAG_SIGALLSYNC;
	uint32_t flagsWithoutMasterHandlers = (flagsSignalsOnly & ~syncSignalsWithMasterHandlers) & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK;

	Trc_PRT_signal_omrsig_protect_entered(fn, fn_arg, handler, handler_arg, flags);

	if (checkForAmbiguousSignalFlags(flags, "omrsig_protect")) {
		return OMRPORT_SIG_ERROR;
	}

	if (OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		/* -Xrs was set, we can't protect against any signals, do not install the master handler */
		Trc_PRT_signal_omrsig_protect_cannot_protect_dueto_Xrs(fn, fn_arg, flags);
		*result = fn(portLibrary, fn_arg);
		Trc_PRT_signal_omrsig_protect_exiting_did_not_protect_due_to_Xrs(fn, fn_arg, handler, handler_arg, flags);
		return 0;
	}

	if (0 != flagsWithoutMasterHandlers) {
		uint32_t rc = 0;
		/* Acquire the registerHandlerMonitor and install the handler via registerMasterHandlers. */
		omrthread_monitor_enter(registerHandlerMonitor);
		rc = registerMasterHandlers(portLibrary, flags, OMRPORT_SIG_FLAG_SIGALLSYNC, NULL);
		omrthread_monitor_exit(registerHandlerMonitor);

		if (0 != rc) {
			return OMRPORT_SIG_ERROR;
		}
	}

	thisThread = omrthread_self();

	thisRecord.previous = omrthread_tls_get(thisThread, tlsKey);
	thisRecord.portLibrary = portLibrary;
	thisRecord.handler = handler;
	thisRecord.handler_arg = handler_arg;
	thisRecord.flags = flags;

	if (OMR_ARE_ANY_BITS_SET(flags, OMRPORT_SIG_FLAG_MAY_RETURN)) {
		/* Record the current signal. We need to store this value back into tls if we jump back into this function
		 * because any signals that may have occurred within the scope of this layer of protection would have been handled
		 * by that point.
		 *
		 * The only scenario where this is of real concern, is if more than one signal was handled per call to omrsig_protect. In
		 * this case, the current signal in tls will be pointing at a stale stack frame and signal: CMVC 126838
		 */
		OMRCurrentSignal *currentSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);

		/* setjmp/longjmp does not clear the mask setup by the OS when it delivers the signal. User sigsetjmp/siglongjmp(buf, 1) instead */
		if (0 != sigsetjmp(thisRecord.returnBuf, 1)) {
			/* the handler had long jumped back here -- reset the signal handler stack and currentSignal and return */
			omrthread_tls_set(thisThread, tlsKey, thisRecord.previous);
			omrthread_tls_set(thisThread, tlsKeyCurrentSignal, currentSignal);
			*result = 0;
			Trc_PRT_signal_omrsignal_sig_protect_Exit_long_jumped_back_to_omrsig_protect(fn, fn_arg, handler, handler_arg, flags);
			return OMRPORT_SIG_EXCEPTION_OCCURRED;
		}
	}

	if (0 != omrthread_tls_set(thisThread, tlsKey, &thisRecord)) {
		Trc_PRT_signal_omrsignal_sig_protect_Exit_ERROR_accessing_tls(fn, fn_arg, handler, handler_arg, flags);
		return OMRPORT_SIG_ERROR;
	}

	*result = fn(portLibrary, fn_arg);

	/* if the first omrthread_tls_set succeeded, then this one will always succeed */
	omrthread_tls_set(thisThread, tlsKey, thisRecord.previous);

	Trc_PRT_signal_omrsignal_sig_protect_Exit_after_returning_from_fn(fn, fn_arg, handler, handler_arg, flags, *result);
	return 0;
}

int32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{
	int32_t rc = 0;
	OMRUnixAsyncHandlerRecord *cursor = NULL;
	OMRUnixAsyncHandlerRecord **previousLink = NULL;

	Trc_PRT_signal_omrsig_set_async_signal_handler_entered(handler, handler_arg, flags);

	if (checkForAmbiguousSignalFlags(flags, "omrsig_set_async_signal_handler")) {
		return OMRPORT_SIG_ERROR;
	}

	omrthread_monitor_enter(registerHandlerMonitor);
	if (OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		/* -Xrs was set, we can't protect against any signals, do not install any handlers except SIGXFSZ */
		if (OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_SIGXFSZ) && OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_SIGXFSZ)) {
			rc = registerMasterHandlers(portLibrary, OMRPORT_SIG_FLAG_SIGXFSZ, OMRPORT_SIG_FLAG_SIGALLASYNC, NULL);
		} else {
			Trc_PRT_signal_omrsig_set_async_signal_handler_will_not_set_handler_due_to_Xrs(handler, handler_arg, flags);
			rc = OMRPORT_SIG_ERROR;
		}
	} else {
		rc = registerMasterHandlers(portLibrary, flags, OMRPORT_SIG_FLAG_SIGALLASYNC, NULL);
	}
	omrthread_monitor_exit(registerHandlerMonitor);

	if (0 != rc) {
		Trc_PRT_signal_omrsig_set_async_signal_handler_exiting_did_nothing_possible_error(handler, handler_arg, flags);
		return rc;
	}

	omrthread_monitor_enter(asyncMonitor);

	/* wait until no signals are being reported */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	/* is this handler already registered? */
	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;

	while (NULL != cursor) {
		if ((cursor->portLib == portLibrary) && (cursor->handler == handler) && (cursor->handler_arg == handler_arg)) {
			if (0 == flags) {
				/* Remove the listener
				 * NOTE: masterHandlers get removed at omrsignal shutdown */

				/* remove this handler record */
				*previousLink = cursor->next;
				portLibrary->mem_free_memory(portLibrary, cursor);
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_removed(handler, handler_arg, flags);
			} else {
				/* update the listener with the new flags */
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_added_1(handler, handler_arg, flags);
				cursor->flags |= flags;
			}
			break;
		}
		previousLink = &cursor->next;
		cursor = cursor->next;
	}

	if (NULL == cursor) {
		/* cursor will only be NULL if we failed to find it in the list */
		if (0 != flags) {
			OMRUnixAsyncHandlerRecord *record = portLibrary->mem_allocate_memory(portLibrary, sizeof(*record), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

			if (NULL == record) {
				rc = OMRPORT_SIG_ERROR;
			} else {
				record->portLib = portLibrary;
				record->handler = handler;
				record->handler_arg = handler_arg;
				record->flags = flags;
				record->next = NULL;

				/* add the new record to the end of the list */
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_added_2(handler, handler_arg, flags);
				*previousLink = record;
			}
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
	OMRUnixAsyncHandlerRecord *cursor = NULL;
	OMRUnixAsyncHandlerRecord **previousLink = NULL;
	BOOLEAN foundHandler = FALSE;

	Trc_PRT_signal_omrsig_set_single_async_signal_handler_entered(handler, handler_arg, portlibSignalFlag);

	if (0 != portlibSignalFlag) {
		/* For non-zero portlibSignalFlag, check if only one signal bit is set. Otherwise, fail. */
		if (!OMR_IS_ONLY_ONE_BIT_SET(portlibSignalFlag & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK)) {
			Trc_PRT_signal_omrsig_set_single_async_signal_handler_error_multiple_signal_flags_found(portlibSignalFlag);
			return OMRPORT_SIG_ERROR;
		}

		if (checkForAmbiguousSignalFlags(portlibSignalFlag, "omrsig_set_single_async_signal_handler")) {
			return OMRPORT_SIG_ERROR;
		}
	}

	omrthread_monitor_enter(registerHandlerMonitor);

	if (OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		/* -Xrs was set, we can't protect against any signals, do not install any handlers except SIGXFSZ*/
		if (OMR_ARE_ALL_BITS_SET(portlibSignalFlag, OMRPORT_SIG_FLAG_SIGXFSZ) && OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_SIGXFSZ)) {
			rc = registerMasterHandlers(portLibrary, OMRPORT_SIG_FLAG_SIGXFSZ, OMRPORT_SIG_FLAG_SIGALLASYNC, oldOSHandler);
		} else {
			Trc_PRT_signal_omrsig_set_single_async_signal_handler_will_not_set_handler_due_to_Xrs(handler, handler_arg, portlibSignalFlag);
			rc = OMRPORT_SIG_ERROR;
		}
	} else {
		rc = registerMasterHandlers(portLibrary, portlibSignalFlag, OMRPORT_SIG_FLAG_SIGALLASYNC, oldOSHandler);
	}

	omrthread_monitor_exit(registerHandlerMonitor);

	if (0 != rc) {
		Trc_PRT_signal_omrsig_set_single_async_signal_handler_exiting_did_nothing_possible_error(rc, handler, handler_arg, portlibSignalFlag);
		return rc;
	}

	omrthread_monitor_enter(asyncMonitor);

	/* wait until no signals are being reported */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	/* is this handler already registered? */
	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;

	while (NULL != cursor) {
		if (cursor->portLib == portLibrary) {
			if ((cursor->handler == handler) && (cursor->handler_arg == handler_arg)) {
				foundHandler = TRUE;
				if (0 == portlibSignalFlag) {
					/* Remove the listener. Remove this handler record.
					 * NOTE: masterHandlers get removed at omrsignal shutdown
					 */
					*previousLink = cursor->next;
					portLibrary->mem_free_memory(portLibrary, cursor);
					Trc_PRT_signal_omrsig_set_single_async_signal_handler_user_handler_removed(handler, handler_arg, portlibSignalFlag);
					break;
				} else {
					/* Update the listener with the new portlibSignalFlag */
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
		OMRUnixAsyncHandlerRecord *record = portLibrary->mem_allocate_memory(portLibrary, sizeof(*record), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == record) {
			rc = OMRPORT_SIG_ERROR;
		} else {
			record->portLib = portLibrary;
			record->handler = handler;
			record->handler_arg = handler_arg;
			record->flags = portlibSignalFlag;
			record->next = NULL;

			/* add the new record to the end of the list */
			Trc_PRT_signal_omrsig_set_single_async_signal_handler_user_handler_added_2(handler, handler_arg, portlibSignalFlag);
			*previousLink = record;
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
	return mapOSSignalToPortLib(osSignalValue, NULL);
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

	if ((0 == portlibSignalFlag) || !OMR_IS_ONLY_ONE_BIT_SET(portlibSignalFlag & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK)) {
		/* If portlibSignalFlag is 0 or if portlibSignalFlag has multiple signal bits set, then fail. */
		Trc_PRT_signal_omrsig_register_os_handler_invalid_portlibSignalFlag(portlibSignalFlag);
		rc = OMRPORT_SIG_ERROR;
	} else if (checkForAmbiguousSignalFlags(portlibSignalFlag, "omrsig_register_os_handler")) {
		return OMRPORT_SIG_ERROR;
	} else {
		omrthread_monitor_enter(registerHandlerMonitor);
		rc = registerSignalHandlerWithOS(portLibrary, portlibSignalFlag, (unix_sigaction)newOSHandler, oldOSHandler);
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

	if ((osHandler == (void *)masterSynchSignalHandler)
		|| (osHandler == (void *)masterASynchSignalHandler)
	) {
		rc = TRUE;
	}

	Trc_PRT_signal_omrsig_is_master_signal_handler_exiting(rc);
	return rc;
}

int32_t
omrsig_is_signal_ignored(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, BOOLEAN *isSignalIgnored)
{
	int32_t rc = 0;
	int osSignalNo = OMRPORT_SIG_ERROR;
	void *oldHandler = NULL;
	struct sigaction oldSignalAction;

	Trc_PRT_signal_omrsig_is_signal_ignored_entered(portlibSignalFlag);

	*isSignalIgnored = FALSE;

	if (0 != portlibSignalFlag) {
		/* For non-zero portlibSignalFlag, check if only one signal bit is set. Otherwise, fail. */
		if (!OMR_IS_ONLY_ONE_BIT_SET(portlibSignalFlag & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK)) {
			rc = OMRPORT_SIG_ERROR;
			goto exit;
		}

		if (checkForAmbiguousSignalFlags(portlibSignalFlag, "omrsig_is_signal_ignored")) {
			return OMRPORT_SIG_ERROR;
		}
	}

	osSignalNo = mapPortLibSignalToOSSignal(portlibSignalFlag);
	if (OMRPORT_SIG_ERROR == osSignalNo) {
		rc = OMRPORT_SIG_ERROR;
		goto exit;
	}

	memset(&oldSignalAction, 0, sizeof(struct sigaction));
	OMRSIG_SIGACTION(osSignalNo, NULL, &oldSignalAction);

	oldHandler = (void *)oldSignalAction.sa_sigaction;
	if (NULL == oldHandler) {
		oldHandler = (void *)oldSignalAction.sa_handler;
	}

	if (oldHandler == (void *)SIG_IGN) {
		*isSignalIgnored = TRUE;
	}

exit:
	Trc_PRT_signal_omrsig_is_signal_ignored_exiting(rc, *isSignalIgnored);
	return rc;
}

/*
 * The full shutdown routine "sig_full_shutdown" overwrites this once we've completed startup
 */
void
omrsig_shutdown(struct OMRPortLibrary *portLibrary)
{
	Trc_PRT_signal_omrsig_shutdown_empty_routine(portLibrary);
	return;
}

/**
 * Start up the signal handling component of the port library
 *
 * Note: none of the master handlers are registered with the OS until the first call to either of omrsig_protect or omrsig_set_async_signal_handler
 */
int32_t
omrsig_startup(struct OMRPortLibrary *portLibrary)
{

	int32_t result = 0;
	omrthread_monitor_t globalMonitor = NULL;
	uint32_t index = 1;

	Trc_PRT_signal_omrsig_startup_entered(portLibrary);

	globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
	if (attachedPortLibraries++ == 0) {

		/* initialize the old actions */
		for (index = 1; index < ARRAY_SIZE_SIGNALS; index++) {
			oldActions[index].restore = 0;
		}

		result = initializeSignalTools(portLibrary);

	}
	omrthread_monitor_exit(globalMonitor);

	if (result == 0) {
		/* we have successfully started up the signal portion, install the full shutdown routine */
		portLibrary->sig_shutdown = sig_full_shutdown;
	}

	Trc_PRT_signal_omrsig_startup_exiting(portLibrary, result);
	return result;
}

static uint32_t
countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category)
{
	void *value = NULL;
	const char *name = NULL;
	uint32_t count = 0;

	while (portLibrary->sig_info(portLibrary, info, category, count, &name, &value) != OMRPORT_SIG_VALUE_UNDEFINED) {
		count++;
	}

	return count;
}

#if defined(OMR_PORT_ASYNC_HANDLER)
/**
 * Given a port library signal flag and Unix signal value, execute the associated handlers
 * stored within asyncHandlerList (list of OMRUnixAsyncHandlerRecord).
 *
 * @param asyncSignalFlag port library signal flag
 * @param unixSignal Unix signal value
 *
 * @return void
 */
static void
runHandlers(uint32_t asyncSignalFlag, int unixSignal)
{
	OMRUnixAsyncHandlerRecord *cursor = asyncHandlerList;

	/* report the signal recorded in signalType to all registered listeners (for this signal).
	 * incrementing the asyncThreadCount will prevent the list from being modified while we use it.
	 */
	omrthread_monitor_enter(asyncMonitor);
	asyncThreadCount++;
	omrthread_monitor_exit(asyncMonitor);

	while (NULL != cursor) {
		if (OMR_ARE_ALL_BITS_SET(cursor->flags, asyncSignalFlag)) {
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
	if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
		/* mapPortLibSignalToOSSignal returns OMRPORT_SIG_ERROR (-1) on unknown mapping */
		if (OMRPORT_SIG_ERROR != unixSignal) {
			omrsig_handler(unixSignal, NULL, NULL);
		}
	}
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */
}

/**
 * Reports the asynchronous signal to all listeners.
 */
static int J9THREAD_PROC
asynchSignalReporter(void *userData)
{
#if defined(J9ZOS390)
	/*
	 * CMVC 192198
	 * Prevent async signals that are handled by masterASynchSignalHandler()
	 * from being caught by this thread. We can't allow masterASynchSignalHandler()
	 * to recursively lock wakeUpASyncReporterMutex because it could cause this
	 * thread to miss signals on wakeUpASyncReporterCond.
	 *
	 * It's ok if this thread caught a signal just before this, because it has
	 * not locked wakeUpASyncReporterMutex yet.
	 *
	 * This limitation is unique to the z/OS implementation of async signal handling.
	 */
	{
		sigset_t asyncSigs;
		intptr_t rc = 0;
		int osRc = 0;

		osRc = sigemptyset(&asyncSigs);
		Assert_PRT_true(0 == osRc);

		rc = addAsyncSignalsToSet(&asyncSigs);
		Assert_PRT_true(0 == rc);

		osRc = sigprocmask(SIG_BLOCK, &asyncSigs, NULL);
		Assert_PRT_true(0 == osRc);
	}
#endif /* defined(J9ZOS390) */

	omrthread_set_name(omrthread_self(), "Signal Reporter");

	while (0 == shutDownASynchReporter) {
		int unixSignal = 1;
		uint32_t asyncSignalFlag = 0;

#if !defined(J9ZOS390)
		/* CMVC  119663 sem_wait can return -1/EINTR on signal in NPTL */
		while (0 != SIGSEM_WAIT(wakeUpASyncReporter));
#endif /* !defined(J9ZOS390) */

		/* determine which signal we've been woken up for */
		for (unixSignal = 1; unixSignal < ARRAY_SIZE_SIGNALS; unixSignal++) {
			uintptr_t signalCount = signalCounts[unixSignal];

			if (signalCount > 0) {
				asyncSignalFlag = mapOSSignalToPortLib(unixSignal, NULL);
				runHandlers(asyncSignalFlag, unixSignal);
				subtractAtomic(&signalCounts[unixSignal], 1);
#if defined(J9ZOS390)
				/* Before waiting on the condvar, we need to make sure all
				 * signals are handled. This will allow us to handle all signals
				 * even if some wake signals for the condvar are missed. Reset
				 * unixSignal to 0. for loop will start again with unixSignal = 1.
				 */
				unixSignal = 0;
#else /* defined(J9ZOS390) */
				/* sem_wait will fall-through for each sem_post. We can handle
				 * one signal at a time. Ultimately, all signals will be handled
				 * So, break out of the for loop.
				 */
				break;
#endif /* defined(J9ZOS390) */
			}
		}

#if defined(J9ZOS390)
		/* Only wait if no signal is pending and shutdown isn't requested. */
		if (0 == shutDownASynchReporter) {
			/* I won't attempt to generate diagnostics if the following pthread
			 * functions return errors because it may interfere with diagnostics
			 * we are attempting to generate for earlier events.
			 */
			Trc_PRT_signal_omrsig_asynchSignalReporterThread_going_to_sleep();
			pthread_mutex_lock(&wakeUpASyncReporterMutex);
			pthread_cond_wait(&wakeUpASyncReporterCond, &wakeUpASyncReporterMutex);
			pthread_mutex_unlock(&wakeUpASyncReporterMutex);
		}
#endif /* defined(J9ZOS390) */

		Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up();
	}

	omrthread_monitor_enter(asyncReporterShutdownMonitor);
	shutDownASynchReporter = 0;
	omrthread_monitor_notify(asyncReporterShutdownMonitor);

	omrthread_exit(asyncReporterShutdownMonitor);

	/* unreachable */
	return 0;
}
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

/**
 * This signal handler is specific to synchronous signals.
 * It will call all of the user's handlers that were registered with the vm using omrsig_protect,
 * upon receiving a signal they listen for.
 *
 */
#if defined(S390) && defined(LINUX)
static void
masterSynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo, uintptr_t breakingEventAddr)
#else /* defined(S390) && defined(LINUX) */
static void
masterSynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo)
#endif /* defined(S390) && defined(LINUX) */
{
#if defined(LINUXPPC)
	/*
	 * PR 56956: ensure the right register context is used if a signal occurs in a transaction.
	 * If there is a signal during a transaction, two contexts are provided: one for the start of the transaction
	 * and one for the context within the transaction.
	 *
	 * On Power 8 Linux, if a signal occurs during a transaction,
	 * the transaction aborts and the corresponding signal handler is invoked with the context from the start of the transaction.
	 * The masterSynchSignalHandler determines if the signal occurred within a transaction,
	 * and if so, returns such that the transaction is executed in the failed state,
	 * causing the non-transactional failure path to be taken (referred to as the "abort handler" in
	 * https://www.kernel.org/doc/Documentation/powerpc/transactional_memory.txt).
	 * Note: this means signals that occur during the transaction, but not in the failure path, are lost.
	 * Please see Jazz design 63039: Handle signals in PowerPC transactional memory operations for further discussion.
	 */

	/*
	 * This contains two contexts: the context at the start of the transaction, plus a link to the
	 * context within the transaction.  We use the former, other than to determine the transaction state.
	 */
	ucontext_t *platformContext = contextInfo;

	/*
	 * Check the Transaction State (TS) bits
	 * for either 01 (Suspended) or 10 (Transactional)
	 */

#if defined(LINUXPPC64)
#define MSR_TS_MASK 0x600000000ULL
	if (OMR_ARE_ANY_BITS_SET(platformContext->uc_mcontext.regs->msr, MSR_TS_MASK))
		/* the transactional context is in the high order bits */
#else /* defined(LINUXPPC64) */
#define MSR_TS_MASK 0x6U
	/*
	 * in 32-bit CPUs, the second context containing the transactional
	 * state is in a separate ucontext datastructure pointed to by uc_link.
	 */
	if ((NULL != platformContext->uc_link) && OMR_ARE_ANY_BITS_SET(platformContext->uc_link->uc_mcontext.regs->msr, MSR_TS_MASK))
#endif /* defined(LINUXPPC64) */
	{
		/*
		 * resume the transaction in the failed state, so it executes the failure path.
		 */
		return;
	}
#endif /* defined(LINUXPPC) */

	omrthread_t thisThread = omrthread_self();
	uint32_t result = U_32_MAX;

	if (NULL != thisThread) {
		struct OMRSignalHandlerRecord *thisRecord = NULL;
		struct OMRCurrentSignal currentSignal = {0};
		struct OMRCurrentSignal *previousSignal = NULL;
		uint32_t portLibType = mapOSSignalToPortLib(signal, sigInfo);

		/* thisRecord->flags will only have OMRPORT_SIG_FLAG_SIGFPE set since the SIGFPE
		 * variants are not included in the OMRPORT_SIG_FLAG_SIGALLSYNC bit-mask. The
		 * received signal can be a variant of SIGFPE: DIV_BY_ZERO, INT_DIV_BY_ZERO or
		 * INT_OVERFLOW. This will handle all the SIGFPE variants if thisRecord->flags
		 * has OMRPORT_SIG_FLAG_SIGFPE set.
		 */
		uint32_t portLibTypeFPEFilter = portLibType;
		if (OMR_ARE_ALL_BITS_SET(portLibType, OMRPORT_SIG_FLAG_SIGFPE)) {
			portLibTypeFPEFilter = OMRPORT_SIG_FLAG_SIGFPE;
		}

		/* record this signal in tls so that omrsig_handler can be called if any of the handlers decide we should be shutting down */
		currentSignal.signal = signal;
		currentSignal.sigInfo = sigInfo;
		currentSignal.contextInfo = contextInfo;
		currentSignal.portLibSignalType = portLibType;
#if defined(S390) && defined(LINUX)
		currentSignal.breakingEventAddr = breakingEventAddr;
#endif /* defined(S390) && defined(LINUX) */

		previousSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);

		omrthread_tls_set(thisThread, tlsKeyCurrentSignal, &currentSignal);

		/* walk the stack of registered handlers from top to bottom searching for one which handles this type of exception */
		thisRecord = omrthread_tls_get(thisThread, tlsKey);

		while (NULL != thisRecord) {
			if (OMR_ARE_ALL_BITS_SET(thisRecord->flags, portLibTypeFPEFilter)) {
				struct OMRUnixSignalInfo signalInfo;
				struct OMRPlatformSignalInfo platformSignalInfo;

				/* the equivalent of these memsets were here before, but were they needed? */
				memset(&signalInfo, 0, sizeof(signalInfo));
				memset(&platformSignalInfo, 0, sizeof(platformSignalInfo));

				signalInfo.portLibrarySignalType = portLibType;
				signalInfo.handlerAddress = (void *)thisRecord->handler;
				signalInfo.handlerAddress2 = (void *)masterSynchSignalHandler;
				signalInfo.sigInfo = sigInfo;
				signalInfo.platformSignalInfo = platformSignalInfo;

				/* found a suitable handler */
				/* what signal type do we want to pass on here? port or platform based ?*/
				fillInUnixSignalInfo(thisRecord->portLibrary, contextInfo, &signalInfo);
#if defined(S390) && defined(LINUX)
				signalInfo.platformSignalInfo.breakingEventAddr = breakingEventAddr;
#endif /* defined(S390) && defined(LINUX) */

				/* remove the handler we are about to invoke, now, in case the handler crashes */
				omrthread_tls_set(thisThread, tlsKey, thisRecord->previous);

				result = thisRecord->handler(thisRecord->portLibrary, portLibType, &signalInfo, thisRecord->handler_arg);

				/* The only case in which we don't want the previous handler back on top is if it just returned OMRPORT_SIG_EXCEPTION_RETURN
				 * 		In this case we will remove it from the top after executing the siglongjmp */
				omrthread_tls_set(thisThread, tlsKey, thisRecord);

				if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
					/* continue looping */
				} else if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION) {
#if defined(J9ZOS390)
					uintptr_t r13;
					uintptr_t rSSP;
					struct __mcontext *mcontext = contextInfo;
#endif /* defined(J9ZOS390) */

					omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
#if defined(J9ZOS390)
					fillInJumpInfo(thisRecord->portLibrary, contextInfo, &(thisRecord->farJumpInfo));

					__far_jump(&(thisRecord->farJumpInfo));
#endif /* defined(J9ZOS390) */
					return;
#if defined(J9ZOS390)
				} else if (result == OMRPORT_SIG_EXCEPTION_COOPERATIVE_SHUTDOWN) {
					break;
#endif /* defined(J9ZOS390) */
				} else /* if (result == OMRPORT_SIG_EXCEPTION_RETURN) */ {
					omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
					siglongjmp(thisRecord->returnBuf, 0);
					/* unreachable */
				}
			}

			thisRecord = thisRecord->previous;
		}

		omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);

	} /* if (thisThread != NULL) */

#if defined(J9ZOS390)

	if ((OMRPORT_SIG_EXCEPTION_COOPERATIVE_SHUTDOWN == result) || (OMR_ARE_ALL_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_COOPERATIVE_SHUTDOWN))) {

		/* Determine if returning from the signal handler will result in Language Environment (LE) initiating Resource Recovery Services (RRS)
		 *  and terminate the enclave, or if we need to explicitly trigger an abend to force RRS and termination.
		 *
		 * RRS is critical to backing out database multi-phase commit transactions that were interrupted before completion
		 *
		 * In general, the rules are:
		 *
		 * 		Hardware signals - return from the signal handler. This is preferable as the signal we're handling is reported in LE diagnostics
		 * 								as the cause of the crash. However, returning doesn't work for software signals.
		 *
		 * 		Software signals -  explicitly trigger an abend. Whereas this forces LE RRS in all cases,
		 * 								LE diagnostics will report the JVM abend as the cause of the crash, which somewhat clouds the issue.
		 */

		/* Default to explicitly triggering the abend, as this is guaranteed to trigger RRS */
		BOOLEAN triggerAbend = TRUE;

#if defined(OMR_ENV_DATA64)
		/* 64-bit: request the condition information block and verify that we got it */
		struct __cib *conditionInfoBlock = __le_cib_get();

		if (NULL != conditionInfoBlock)
#else /* defined(OMR_ENV_DATA64) */
		/* 31-bit: request the condition information block and verify that we got it */
		_CEECIB *conditionInfoBlock = NULL;
		_FEEDBACK cibfc;

		CEE3CIB(NULL, &conditionInfoBlock, &cibfc);

		if (0 ==  _FBCHECK(cibfc, CEE000))
#endif /* defined(OMR_ENV_DATA64) */
		{
			/* we successfully acquired the condition information block */

			/* check the abend and program check flags, if they are set, this is a "real" hardware signal */
			if ((1 == conditionInfoBlock-> cib_abf) || (1 == conditionInfoBlock->cib_pcf)) {
				if ((0 != sigInfo->si_code) && (_SEGV_SOFTLIMIT != sigInfo->si_code) &&
					((SIGSEGV == signal) ||
					 (SIGILL == signal) ||
					 (SIGFPE == signal) ||
					 (SIGABND == signal))) {

					triggerAbend = FALSE;
				}
			}
		}

#if defined(OMRPORT_OMRSIG_SUPPORT)
		/* do the signal chaining */
		if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
			omrsig_handler(signal, (void *)sigInfo, contextInfo);
		}
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

		if (TRUE == triggerAbend) {
			sigrelse(SIGABND); /* CMVC 191934: need to unblock sigabnd before issuing the abend call */
#if defined(OMR_ENV_DATA64)
			__cabend(PORT_ABEND_CODE, PORT_ABEND_REASON_CODE, PORT_ABEND_CLEANUP_CODE /* normal termination processing */);
#else /* defined(OMR_ENV_DATA64) */
			{
				_INT4 code = PORT_ABEND_CODE;
				_INT4 reason = PORT_ABEND_REASON_CODE;
				_INT4 cleanup = PORT_ABEND_CLEANUP_CODE;	 /* normal termination processing */

				CEE3AB2(&code, &reason, &cleanup);
			}
#endif /* defined(OMR_ENV_DATA64) */
		}

		/* we now know that returning from the signal handler will result in LE initiating RRS and terminate the enclave */
		return;
	}

	/* unreachable */
#endif /* defined(J9ZOS390) */


	/* The only way to get here is if (1) this thread was not attached to the thread library or (2) the thread hadn't registered
	 * any signal handlers with the port library that could handle the signal
	 */

#if defined(OMRPORT_OMRSIG_SUPPORT)
	if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
		int rc = omrsig_handler(signal, (void *)sigInfo, contextInfo);
#if !defined(J9ZOS390)
		if ((OMRSIG_RC_DEFAULT_ACTION_REQUIRED == rc) && (SI_USER != sigInfo->si_code)) {
			abort();
		}
#endif /* !defined(J9ZOS390) */
	}
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

	/* if we got this far there weren't any handlers on the stack that knew what to with this signal
	 * default action is to abort */
#if defined(J9ZOS390)
	if (SIGABND != signal) {
		/* Percolate unhandled SIGABND and let the default action occur */
#endif /* defined(J9ZOS390) */
		abort();
#if defined(J9ZOS390)
	}
#endif /* defined(J9ZOS390) */
}


/**
 * Determines the signal received and notifies the asynch signal reporter
 *
 * One semaphore is used to notify the asynchronous signal reporter that it is time to act.
 * Each expected aynch signal type has an associated semaphore which is used to count the number of "pending" signals.
 *
 */
#if defined(S390) && defined(LINUX)
static void
masterASynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo, uintptr_t nullArg)
#else /* defined(S390) && defined(LINUX) */
static void
masterASynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo)
#endif /* defined(S390) && defined(LINUX) */
{
	addAtomic(&signalCounts[signal], 1);
#if !defined(J9ZOS390)
	SIGSEM_POST(wakeUpASyncReporter);
#else /* !defined(J9ZOS390) */
	pthread_mutex_lock(&wakeUpASyncReporterMutex);
	pthread_cond_signal(&wakeUpASyncReporterCond);
	pthread_mutex_unlock(&wakeUpASyncReporterMutex);
#endif /* !defined(J9ZOS390) */

	return;
}

/**
 * Register the signal handler with the OS, generally used to register the master signal handlers
 * Not to be confused with omrsig_protect, which registers the user's handler with the port library.
 *
 * Calls to this function must be synchronized using "registerHandlerMonitor".
 *
 * The use of this function forces the flags SA_RESTART | SA_SIGINFO | SA_NODEFER to be set for the new signal action
 *
 * oldOSHandler points to the old signal handler function.
 *
 * During first registration, the old action for the signal handler is stored in oldActions. The original OS handler must
 * be restored before the portlibrary is shut down. For subsequent registrations, old action is not stored in oldActions
 * in order to avoid overwriting the original OS handler. Instead, a local sigaction variable is used to store old action
 * for subsequent registrations. oldOSHandler is updated to point to oldAction.sa_sigaction (signal handler function).
 *
 * @return 0 upon success, non-zero otherwise.
 */
static int32_t
registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, unix_sigaction handler, void **oldOSHandler)
{
	int unixSignalNo = mapPortLibSignalToOSSignal(portLibrarySignalNo);
	struct sigaction newAction;

	/* Don't register a handler for the unrecognized OS signals.
	 * Unrecognized OS signals are the ones which aren't included in signalMap.
	 */
	if (OMRPORT_SIG_ERROR == unixSignalNo) {
		return OMRPORT_SIG_ERROR;
	}

	/* If a process has blocked an asynchronous signal, then the signal stays
	 * blocked in the sub-processes across fork(s) and exec(s). A blocked
	 * signal prevents its OS signal handler to be invoked. A signal is
	 * unblocked as an OS signal handler is installed for it in case a
	 * parent process has blocked it.
	 */
	if (OMR_ARE_ALL_BITS_SET(OMRPORT_SIG_FLAG_SIGALLASYNC, portLibrarySignalNo)) {
		if (0 != unblockSignal(unixSignalNo)) {
			return OMRPORT_SIG_ERROR;
		}
	}

	memset(&newAction, 0, sizeof(struct sigaction));

	/* Do not block any signals. */
	if (0 != sigemptyset(&newAction.sa_mask)) {
		return OMRPORT_SIG_ERROR;
	}

	/* Automatically restart system calls that get interrupted by any signal.
	 * Neutrino V6.3 does not support this feature.
	 */
	newAction.sa_flags = SA_RESTART;

	/* Setting to SA_SIGINFO will result in "void (*sa_sigaction) (int, siginfo_t *, void *)" to be used, and
	 * not "__sighandler_t sa_handler". Both are members of struct sigaction. Using the former allows us to
	 * access more than just the signal number.
	 */
	newAction.sa_flags |= SA_SIGINFO;

	/* SA_NODEFER prevents the current signal from being masked by default in the handler. However, it can still
	 * be masked if one explicitly requests so in the sa_mask field, as done on z/OS.
	 */
	newAction.sa_flags |= SA_NODEFER;

#if defined(J9ZOS390)
	/* z/OS doesn't have POSIX semaphores. As a precaution, re-entering the masterASyncHandler must be avoided.
	 * Therefore, all the asynchronous signals are masked for the masterASyncHandler. The signal(s) are queued
	 * and delivered to the masterASyncHandler once the handler returns. No signals are lost.
	 */
	if (OMR_ARE_ALL_BITS_SET(OMRPORT_SIG_FLAG_SIGALLASYNC, portLibrarySignalNo)) {
		if (0 != addAsyncSignalsToSet(&newAction.sa_mask)) {
			return OMRPORT_SIG_ERROR;
		}
	}
#endif /* defined(J9ZOS390) */

#if defined(AIXPPC)
	/* Do the following while installing a handler for an asynchronous signal block SIGTRAP. */
	if (OMR_ARE_ALL_BITS_SET(OMRPORT_SIG_FLAG_SIGALLASYNC, portLibrarySignalNo)) {
		if (sigaddset(&newAction.sa_mask, SIGTRAP)) {
			return OMRPORT_SIG_ERROR;
		}
	}
#endif /* defined(AIXPPC) */

	/* The master exception handler:
	 * The (void *) casting only applies to zLinux because a new parameter "uintptr_t breakingEventAddr"
	 * has been introduced in masterSynchSignalHandler() to obtain BEA on zLinux but not for other platforms.
	 * As a result, the total number of parameters in masterSynchSignalHandler() on zLinux is 4 while the
	 * number is 3 on other platforms, because there is no need to change the signature of masterSynchSignalHandler
	 * in there. Since the code is shared on all platforms, the change here is used to split them up to avoid any
	 * compiling error.
	 */
#if defined(S390) && defined(LINUX)
	newAction.sa_sigaction = (void *)handler;
#else /* defined(S390) && defined(LINUX) */
	newAction.sa_sigaction = handler;
#endif /* defined(S390) && defined(LINUX) */

	/* After setting up the sigaction struct, register the handler with the OS. When registering the handler for
	 * the first time, the old OS handler is stored in the oldActions[unixSignalNo].action. This way the original
	 * OS handler can be restored during shutdown. For subsequent registrations, the oldActions[unixSignalNo].action
	 * is not updated since the original OS handler would be overwritten. Instead, a local sigaction variable is used
	 * to store the old OS handler.
	 */
	if (0 == oldActions[unixSignalNo].restore) {
		/* Initialize oldAction. */
		memset(&oldActions[unixSignalNo].action, 0, sizeof(struct sigaction));
		if (OMRSIG_SIGACTION(unixSignalNo, &newAction, &oldActions[unixSignalNo].action)) {
			Trc_PRT_signal_registerSignalHandlerWithOS_failed_to_registerHandler(portLibrarySignalNo, unixSignalNo, handler);
			return OMRPORT_SIG_ERROR;
		} else {
			Trc_PRT_signal_registerSignalHandlerWithOS_registeredHandler1(portLibrarySignalNo, unixSignalNo, handler, oldActions[unixSignalNo].action.sa_sigaction);
			oldActions[unixSignalNo].restore = 1;
			if (NULL != oldOSHandler) {
				*oldOSHandler = (void *)oldActions[unixSignalNo].action.sa_sigaction;
			}
		}
	} else {
		struct sigaction oldAction;
		memset(&oldAction, 0, sizeof(struct sigaction));
		if (OMRSIG_SIGACTION(unixSignalNo, &newAction, &oldAction)) {
			Trc_PRT_signal_registerSignalHandlerWithOS_failed_to_registerHandler(portLibrarySignalNo, unixSignalNo, handler);
			return OMRPORT_SIG_ERROR;
		} else {
			Trc_PRT_signal_registerSignalHandlerWithOS_registeredHandler1(portLibrarySignalNo, unixSignalNo, handler, oldAction.sa_sigaction);
			if (NULL != oldOSHandler) {
				*oldOSHandler = (void *)oldAction.sa_sigaction;
			}
		}
	}

	issueWriteBarrier();

	setBitMaskSignalsWithHandlers(portLibrarySignalNo);

	if ((handler == masterSynchSignalHandler) || (handler == masterASynchSignalHandler)) {
		setBitMaskSignalsWithMasterHandlers(portLibrarySignalNo);
	} else {
		unsetBitMaskSignalsWithMasterHandlers(portLibrarySignalNo);
	}

	return 0;
}

/**
 * The Unix signal number is converted to the corresponding port library
 * signal number.
 *
 * Some signals have subtypes which are detailed in the siginfo_t structure.
 */
static uint32_t
mapOSSignalToPortLib(uint32_t signalNo, siginfo_t *sigInfo)
{
	uint32_t index = 0;

	if ((SIGFPE == signalNo) && (NULL != sigInfo)) {
		/* If we are not looking up the mapping in response to a signal
		 * we will not have a siginfo_t structure.
		 */

		/* Linux 2.4 kernel bug: 64-bit platforms or in 0x30000 into si_code */
		switch (sigInfo->si_code & 0xff) {
		case FPE_FLTDIV:
			return OMRPORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO;
		case FPE_INTDIV:
			return OMRPORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO;
		case FPE_INTOVF:
			return OMRPORT_SIG_FLAG_SIGFPE_INT_OVERFLOW;
		default:
			return OMRPORT_SIG_FLAG_SIGFPE;
		}
	}

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); ++index) {
		if (signalMap[index].unixSignalNo == signalNo) {
			return signalMap[index].portLibSignalNo;
		}
	}

	Trc_PRT_signal_mapOSSignalToPortLib_ERROR_unknown_signal(signalNo);
	return 0;
}

/**
 * The defined port library signal is converted to the corresponding Unix
 * signal number.
 *
 * Note that FPE signal codes (subtypes) all map to the same signal number and are not included
 *
 * @param portLibSignal The internal port library signal number
 *
 * @return The corresponding Unix signal number or OMRPORT_SIG_ERROR (-1) if the portLibSignal
 *         could not be mapped
 */
static int
mapPortLibSignalToOSSignal(uint32_t portLibSignal)
{
	uint32_t index = 0;

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); index++) {
		if (signalMap[index].portLibSignalNo == portLibSignal) {
			return signalMap[index].unixSignalNo;
		}
	}

	Trc_PRT_signal_mapPortLibSignalToOSSignal_ERROR_unknown_signal(portLibSignal);
	return OMRPORT_SIG_ERROR;
}

#if defined(J9ZOS390)
/**
 * Add the async signals handled by the port library to a signal set.
 *
 * @param ss Signal set to be modified. Signals already in the set are not removed.
 * @return 0 on success, non-zero on failure
 */
static intptr_t
addAsyncSignalsToSet(sigset_t *ss)
{
	uintptr_t i = 0;

	Assert_PRT_true(NULL != ss);

	/* Iterate through all the known signals. */
	for (i = 0; i < sizeof(signalMap) / sizeof(signalMap[0]); i++) {
		if (OMR_ARE_ALL_BITS_SET(OMRPORT_SIG_FLAG_SIGALLASYNC, signalMap[i].portLibSignalNo)) {
			/* Add the current signal to the signal set. */
			if (sigaddset(ss, signalMap[i].unixSignalNo)) {
				return -1;
			}
		}
	}
	return 0;
}
#endif /* defined(J9ZOS390) */

/**
 * Registers the master handler for the signals in flags that don't have one.
 * If [sync|async]SignalsWithMasterHandlers suggests a master handler is already
 * registered with a signal, then a master handler isn't registered again for that
 * signal.
 *
 * Calls to this function must be synchronized using registerHandlerMonitor.
 *
 * @param[in] flags the flags that we want signals for
 * @param[in] allowedSubsetOfFlags must be one of OMRPORT_SIG_FLAG_SIGALLSYNC, or
 *            OMRPORT_SIG_FLAG_SIGALLASYNC
 * @param[out] oldOSHandler points to the old signal handler function
 *
 * @return	0 upon success; OMRPORT_SIG_ERROR otherwise.
 *			Possible failure scenarios include attempting to register a handler for
 *			a signal that is not included in the allowedSubsetOfFlags
*/
static int32_t
registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags, void **oldOSHandler)
{
	/* Bitwise-OR with OMRPORT_SIG_FLAG_CONTROL_BITS_MASK is performed in order to
	 * preserve the control bits when storing flags in flagsSignalsOnly.
	 */
	uint32_t signalFlags = (flags & allowedSubsetOfFlags) & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK;
	unix_sigaction handler = NULL;
	uint32_t signalType = OMRPORT_SIG_FLAG_IS_SYNC;
	uint32_t signalsWithMasterHandlersLocal = syncSignalsWithMasterHandlers;
	uint32_t allowedSignalType = 0;

	if (OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_ASYNC)) {
		signalType = OMRPORT_SIG_FLAG_IS_ASYNC;
		signalsWithMasterHandlersLocal = asyncSignalsWithMasterHandlers;
	}

	if (OMRPORT_SIG_FLAG_SIGALLSYNC == allowedSubsetOfFlags) {
		handler = masterSynchSignalHandler;
		allowedSignalType = OMRPORT_SIG_FLAG_IS_SYNC;
	} else if (OMRPORT_SIG_FLAG_SIGALLASYNC == allowedSubsetOfFlags) {
		handler = masterASynchSignalHandler;
		allowedSignalType = OMRPORT_SIG_FLAG_IS_ASYNC;
	} else {
		return OMRPORT_SIG_ERROR;
	}

	/* Only register handlers if signal bits are set in signalFlags, and flags
	 * and allowedSubsetOfFlags have the same signal type.
	 */
	if (signalType == allowedSignalType) {
		while (0 != signalFlags) {
			/* Get the rightmost 1 bit in signalFlags. */
			uint32_t portSignalFlag = signalFlags & -signalFlags;
			uint32_t portSignalFlagWithType = portSignalFlag | signalType;
			if (!OMR_ARE_ALL_BITS_SET(signalsWithMasterHandlersLocal, portSignalFlagWithType)) {
				/* Register a master handler for this (portSignalFlagWithType's) signal. */
				if (0 != registerSignalHandlerWithOS(portLibrary, portSignalFlagWithType, handler, oldOSHandler)) {
					return OMRPORT_SIG_ERROR;
				}
			} else {
				/* If the master handler is already registered, then the oldOSHandler must
				 * represent the master handler.
				 */
				if (NULL != oldOSHandler) {
					*oldOSHandler = (void *)handler;
				}
			}
			/* Unset the rightmost 1 bit in signalFlags. */
			signalFlags ^= portSignalFlag;
		}
	}

	return 0;
}


static int32_t
initializeSignalTools(OMRPortLibrary *portLibrary)
{
#if defined(OSX)
	char semNames[6][128] = {{0}};
#endif /* defined(OSX) */
	
	/* use this to record the end of the list of signal infos */
	if (omrthread_tls_alloc(&tlsKey)) {
		return -1;
	}

	/* use this to record the last signal that occurred such that we can call omrsig_handler in omrexit_shutdown_and_exit */
	if (omrthread_tls_alloc(&tlsKeyCurrentSignal)) {
		return -1;
	}

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
	if (0 != ceehdlr_startup(portLibrary)) {
		return -1;
	}
#endif /* defined(OMR_PORT_ZOS_CEEHDLRSUPPORT) */

	if (omrthread_monitor_init_with_name(&registerHandlerMonitor, 0, "portLibrary_omrsig_registerHandler_monitor")) {
		return -1;
	}

	if (omrthread_monitor_init_with_name(&asyncReporterShutdownMonitor, 0, "portLibrary_omrsig_asynch_reporter_shutdown_monitor")) {
		return -1;
	}

	if (omrthread_monitor_init_with_name(&asyncMonitor, 0, "portLibrary_omrsig_async_monitor")) {
		return -1;
	}

#if !defined(J9ZOS390)
#if defined(OSX)
	/* OSX only has named semaphores. They are not shared across processes, so unlink immediately. */
	portLibrary->str_printf(portLibrary, semNames[0], 128, "/omr/wakeUpASyncReporter-%d", getpid());
#define SIGSEM_NAME(_i) semNames[_i]
#else /* defined(OSX) */
#define SIGSEM_NAME(_i) NULL
#endif /* defined(OSX) */

	/* The asynchronous signal reporter will wait on this semaphore  */
	if (SIGSEM_ERROR == SIGSEM_INIT(wakeUpASyncReporter, SIGSEM_NAME(0))) {
		return -1;
	}
	SIGSEM_UNLINK(SIGSEM_NAME(0));
#undef SIGSEM_NAME

#else /* !defined(J9ZOS390) */
	if (pthread_mutex_init(&wakeUpASyncReporterMutex, NULL)) {
		return -1;
	}

	if (pthread_cond_init(&wakeUpASyncReporterCond, NULL)) {
		return -1;
	}

	if (TRUE == checkIfResumableTrapsSupported(portLibrary)) {
		PPG_resumableTrapsSupported = TRUE;
	} else {
		PPG_resumableTrapsSupported = FALSE;
	}
#endif /* !defined(J9ZOS390) */

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
		return -1;
	}
#endif /* defined(OMR_PORT_ASYNC_HANDLER) */

	return 0;
}

static int32_t
setReporterPriority(OMRPortLibrary *portLibrary, uintptr_t priority)
{
	Trc_PRT_signal_setReporterPriority(portLibrary, priority);

	if (NULL == asynchSignalReporterThread) {
		return -1;
	}

	return omrthread_set_priority(asynchSignalReporterThread, priority);
}

/**
 * sets the priority of the the async reporting thread
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

static uint32_t
destroySignalTools(OMRPortLibrary *portLibrary)
{
	omrthread_tls_free(tlsKey);
	omrthread_tls_free(tlsKeyCurrentSignal);
	omrthread_monitor_destroy(registerHandlerMonitor);
	omrthread_monitor_destroy(asyncReporterShutdownMonitor);
	omrthread_monitor_destroy(asyncMonitor);
#if !defined(J9ZOS390)
	SIGSEM_DESTROY(wakeUpASyncReporter);
#else /* !defined(J9ZOS390) */
	pthread_mutex_destroy(&wakeUpASyncReporterMutex);
	pthread_cond_destroy(&wakeUpASyncReporterCond);
#endif /* !defined(J9ZOS390) */

	return 0;
}

int32_t
omrsig_set_options(struct OMRPortLibrary *portLibrary, uint32_t options)
{
	Trc_PRT_signal_omrsig_set_options(options);

	if (OMR_ARE_ANY_BITS_SET(options, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS))  {
		/* Check that no handlers are installed. */
		uint32_t anyHandlersInstalled = 0;

		omrthread_monitor_enter(registerHandlerMonitor);
		if ((0 != syncSignalsWithHandlers) || (0 != asyncSignalsWithHandlers)) {
			anyHandlersInstalled = 1;
		}
		omrthread_monitor_exit(registerHandlerMonitor);

		if (0 != anyHandlersInstalled) {
			Trc_PRT_signal_omrsig_set_options_too_late_handlers_installed(options);
			return -1;
		}
	}

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
	if (OMR_ARE_ANY_BITS_SET(options, OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR)) {
		/* Received notification to use LE condition handling. Switch over to LE condition handling,
		 * unless POSIX handlers are installed for any synchronous signals.
		 */
		int32_t syncHandlersInstalled = 0;

		omrthread_monitor_enter(registerHandlerMonitor);

		if (OMR_ARE_NO_BITS_SET(syncSignalsWithHandlers, OMRPORT_SIG_FLAG_SIGALLSYNC & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK)) {
			/* No synchronous handlers are installed; so, it is fine to switch to LE condition handling. */
			portLibrary->sig_protect = omrsig_protect_ceehdlr;
			portLibrary->sig_info = omrsig_info_ceehdlr;
			portLibrary->sig_get_current_signal = omrsig_get_current_signal_ceehdlr;
		} else {
			/* Set syncHandlersInstalled to report failure. */
			syncHandlersInstalled = 1;
		}

		omrthread_monitor_exit(registerHandlerMonitor);

		if (syncHandlersInstalled == 1) {
			Trc_PRT_signal_omrsig_set_options_too_late_handlers_installed(options);
			return -1;
		}
	}
#endif /* defined(OMR_PORT_ZOS_CEEHDLRSUPPORT) */

	signalOptionsGlobal |= options;

	return 0;
}

uint32_t
omrsig_get_options(struct OMRPortLibrary *portLibrary)
{
	return signalOptionsGlobal;
}

intptr_t
omrsig_get_current_signal(struct OMRPortLibrary *portLibrary)
{
	OMRCurrentSignal *currentSignal = omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);
	if (NULL == currentSignal) {
		return 0;
	}
	return currentSignal->portLibSignalType;
}

static void
sig_full_shutdown(struct OMRPortLibrary *portLibrary)
{
	omrthread_monitor_t globalMonitor = NULL;
	uint32_t index = 1;

	Trc_PRT_signal_sig_full_shutdown_enter(portLibrary);
	globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
	if (--attachedPortLibraries == 0) {
		/* register the old actions we overwrote with our own */
		for (index = 1; index < ARRAY_SIZE_SIGNALS; index++) {
			if (oldActions[index].restore) {
				uint32_t portlibSignalFlag = mapOSSignalToPortLib(index, NULL);
				OMRSIG_SIGACTION(index, &oldActions[index].action, NULL);
				/* record that we no longer have a handler installed with the OS for this signal */
				Trc_PRT_signal_sig_full_shutdown_deregistered_handler_with_OS(portLibrary, index);
				unsetBitMaskSignalsWithHandlers(portlibSignalFlag);
				unsetBitMaskSignalsWithMasterHandlers(portlibSignalFlag);
				oldActions[index].restore = 0;
			}
		}

		removeAsyncHandlers(portLibrary);

#if defined(OMR_PORT_ASYNC_HANDLER)
		/* shut down the asynch reporter thread */
		omrthread_monitor_enter(asyncReporterShutdownMonitor);

#if defined(J9ZOS390)
		pthread_mutex_lock(&wakeUpASyncReporterMutex);
#endif /* defined(J9ZOS390) */
		shutDownASynchReporter = 1;

#if defined(J9ZOS390)
		pthread_cond_signal(&wakeUpASyncReporterCond);
		pthread_mutex_unlock(&wakeUpASyncReporterMutex);
#else /* defined(J9ZOS390) */
		SIGSEM_POST(wakeUpASyncReporter);
#endif /* defined(J9ZOS390) */
		while (shutDownASynchReporter) {
			omrthread_monitor_wait(asyncReporterShutdownMonitor);
		}

		omrthread_monitor_exit(asyncReporterShutdownMonitor);
#endif	/* defined(OMR_PORT_ASYNC_HANDLER) */

		/* destroy all of the remaining monitors */
		destroySignalTools(portLibrary);

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
		ceehdlr_shutdown(portLibrary);
#endif /* defined(OMR_PORT_ZOS_CEEHDLRSUPPORT) */

	}
	omrthread_monitor_exit(globalMonitor);
	Trc_PRT_signal_sig_full_shutdown_exiting(portLibrary);
}

static void
removeAsyncHandlers(OMRPortLibrary *portLibrary)
{
	/* clean up the list of async handlers */
	OMRUnixAsyncHandlerRecord *cursor = NULL;
	OMRUnixAsyncHandlerRecord **previousLink = NULL;

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

	omrthread_monitor_exit(asyncMonitor);
}

#if defined(OMRPORT_OMRSIG_SUPPORT)

/*  @internal
 *
 * omrexit_shutdown_and_exit needs to call this to ensure the signal is chained to omrsig (the application
 *  handler in the case when the shutdown is due to a fatal signal.
 *
 * @return
 */
void
omrsig_chain_at_shutdown_and_exit(struct OMRPortLibrary *portLibrary)
{
	OMRCurrentSignal *currentSignal = omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);

	Trc_PRT_signal_omrsig_chain_at_shutdown_and_exit_enter(portLibrary);

	if (currentSignal != NULL) {
		/* we are shutting down due to a signal, forward it to the application handlers */
		if (!(signalOptionsGlobal & OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
			Trc_PRT_signal_omrsig_chain_at_shutdown_and_exit_forwarding_to_omrsigHandler(portLibrary, currentSignal->signal);
			omrsig_handler(currentSignal->signal, currentSignal->sigInfo, currentSignal->contextInfo);
		}
	}
	Trc_PRT_signal_omrsig_chain_at_shutdown_and_exit_exiting(portLibrary);
}
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

/**
 * This function will unblock a signal by changing the signal mask of the
 * calling thread. This function is only invoked while registering a signal
 * handler with the OS (registerSignalHandlerWithOS), which is protected by
 * registerHandlerMonitor for synchronization.
 *
 * @param[in] signal the signal to be unblocked
 *
 * @return 0 on success and non-zero on failure
 */
static int32_t
unblockSignal(int signal) {
	int32_t rc = 0;
	sigset_t signalSet;

	rc = sigemptyset(&signalSet);
	if (0 != rc) {
		Trc_PRT_signal_unblockSignals_sigemptyset_failed(rc, errno);
		goto exit;
	}

	rc = sigaddset(&signalSet, signal);
	if (0 != rc) {
		Trc_PRT_signal_unblockSignals_sigaddset_failed(signal, rc, errno);
		goto exit;
	}

	/* Unblock the signal. */
	rc = pthread_sigmask(SIG_UNBLOCK, &signalSet, NULL);
	if (0 != rc) {
		Trc_PRT_signal_unblockSignals_pthread_sigmask_failed(rc, errno);
	}

exit:
	return rc;
}

/**
 * Set the port library signal flags in either syncSignalsWithHandlers or
 * asyncSignalsWithHandlers depending on whether the input is a set of
 * synchronous or asynchronous signals.
 *
 * @param[in] flags the port library signal flags
 *
 * @return void
 */
static void
setBitMaskSignalsWithHandlers(uint32_t flags)
{
	if (OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_SYNC)) {
		syncSignalsWithHandlers |= flags;
	} else {
		asyncSignalsWithHandlers |= flags;
	}
}

/**
 * Unset the port library signal flags in either syncSignalsWithHandlers or
 * asyncSignalsWithHandlers depending on whether the input is a set of
 * synchronous or asynchronous signals.
 *
 * @param[in] flags the port library signal flags
 *
 * @return void
 */
static void
unsetBitMaskSignalsWithHandlers(uint32_t flags)
{
	/* When unsetting the signal flags from the bit-mask, the control bits are not unset
	 * because the control bits are shared between all the port library signal flags. Also,
	 * this simplifies the checks to see if a signal flag is set in the bit-mask.
	 */
	if (OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_SYNC)) {
		syncSignalsWithHandlers &= ~(flags & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK);
	} else {
		asyncSignalsWithHandlers &= ~(flags & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK);
	}
}

/**
 * Set the port library signal flags in either syncSignalsWithMasterHandlers or
 * asyncSignalsWithMasterHandlers depending on whether the input is a set of
 * synchronous or asynchronous signals.
 *
 * @param[in] flags the port library signal flags
 *
 * @return void
 */
static void
setBitMaskSignalsWithMasterHandlers(uint32_t flags)
{
	if (OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_SYNC)) {
		syncSignalsWithMasterHandlers |= flags;
	} else {
		asyncSignalsWithMasterHandlers |= flags;
	}
}

/**
 * Unset the port library signal flags in either syncSignalsWithMasterHandlers or
 * asyncSignalsWithMasterHandlers depending on whether the input is a set of
 * synchronous or asynchronous signals.
 *
 * @param[in] flags the port library signal flags
 *
 * @return void
 */
static void
unsetBitMaskSignalsWithMasterHandlers(uint32_t flags)
{
	/* When unsetting the signal flags from the bit-mask, the control bits are not unset
	 * because the control bits are shared between all the port library signal flags. Also,
	 * this simplifies the checks to see if a signal flag is set in the bit-mask.
	 */
	if (OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_SYNC)) {
		syncSignalsWithMasterHandlers &= ~(flags & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK);
	} else {
		asyncSignalsWithMasterHandlers &= ~(flags & ~OMRPORT_SIG_FLAG_CONTROL_BITS_MASK);
	}
}

/**
 * Check if a set of port library flags is ambiguous. It can either have synchronous or
 * asynchronous signals. It cannot have both types of signals. If both
 * OMRPORT_SIG_FLAG_IS_ASYNC and OMRPORT_SIG_FLAG_IS_SYNC are set, then the set of signal
 * flags is considered ambiguous. If none of the two signal identifier flags are set, then
 * the set of signal flags is also considered ambiguous. flags=0 is valid in some cases,
 * and it indicates cleanup/removal of a handler.
 *
 * @param[in] flags the set of port library signal flags
 * @param[in] functionName the name of the function invoking checkForAmbiguousSignalFlags
 *
 * @return TRUE if the set of signal flags is ambiguous; otherwise, FALSE.
 */
static BOOLEAN
checkForAmbiguousSignalFlags(uint32_t flags, const char *functionName)
{
	BOOLEAN rc = FALSE;

	if ((0 != flags)
		&& ((OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_SYNC) && OMR_ARE_ALL_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_ASYNC))
			|| (OMR_ARE_NO_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_SYNC) && OMR_ARE_NO_BITS_SET(flags, OMRPORT_SIG_FLAG_IS_ASYNC)))
	) {
		rc = TRUE;
		/* The tracepoint below is an exit and exception tracepoint. The calling function
		 * should exit with an error code if an ambiguous signal is found, instead of
		 * flowing through and executing another exit tracepoint.
		 */
		Trc_PRT_omrsig_ambiguous_signal_flag_failed_exiting(functionName, flags);
	}

	return rc;
}
