/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
#include "omrsignal_context.h"

#if !defined(J9ZOS390)
#if defined(J9OS_I5) && defined(J9OS_I5_V5R4)
#include "semaphore_i5.h"
#else
#include <semaphore.h>
#endif
#endif /* !defined(J9ZOS390) */

#if defined(J9ZOS390)
#include <pthread.h>
#endif /* defined(J9ZOS390) */

#if defined(J9ZOS390)
#if defined(OMR_ENV_DATA64)
#include <__le_api.h>
#else
#include <leawi.h>
#include <ceeedcct.h>
#endif

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
#include "omrsignal_ceehdlr.h"
#endif
#endif /* defined(J9ZOS390) */

#if defined(OMRPORT_OMRSIG_SUPPORT)
#include "omrsig.h"
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

#define MAX_PORTLIB_SIGNAL_TYPES  8

#if defined(S390) && defined(LINUX)
typedef  void (*unix_sigaction)(int, siginfo_t *, void *, uintptr_t);
#else
typedef  void (*unix_sigaction)(int, siginfo_t *, void *);
#endif

/* Store the previous signal handlers, we need to restore them when we're done */
static struct {
	struct sigaction action;
	uint32_t restore;
}	oldActions[MAX_UNIX_SIGNAL_TYPES];

/* Records the (port library defined) signals for which we have registered a master handler.
 * Access to this must be protected by the masterHandlerMonitor */
static uint32_t signalsWithMasterHandlers;

#if defined(OMR_PORT_ASYNC_HANDLER)
static uint32_t shutDownASynchReporter;
#endif

static uint32_t attachedPortLibraries;

typedef struct J9UnixAsyncHandlerRecord {
	OMRPortLibrary *portLib;
	omrsig_handler_fn handler;
	void *handler_arg;
	uint32_t flags;
	struct J9UnixAsyncHandlerRecord *next;
} J9UnixAsyncHandlerRecord;

/* holds the options set by omrsig_set_options */
uint32_t signalOptionsGlobal;

static J9UnixAsyncHandlerRecord *asyncHandlerList;

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
static SIGSEM_T sigQuitPendingSem;
static SIGSEM_T sigAbrtPendingSem;
static SIGSEM_T sigTermPendingSem;
static SIGSEM_T sigReconfigPendingSem;
static SIGSEM_T sigXfszPendingSem;

#else /* !defined(J9ZOS390) */

/* The asyncSignalReporter synchronization has to be
 * done differently on ZOS
 */

#define NUM_ASYNC_SIGNALS 4
#define SIGQUIT_PENDING 0
#define SIGABRT_PENDING 1
#define SIGTERM_PENDING 2
#define SIGXFSZ_PENDING 3

/* holds the current asynch signal type */
static uint32_t signalCounts[NUM_ASYNC_SIGNALS];

/* z/OS does not have system semaphores, yet we can't rely on thread library
 *  being present for the masterASyncSignalHandler, so use pthread synchronization
 * Note: the use of pthread functions is allowed on z/OS
 */
static pthread_cond_t wakeUpASyncReporterCond;
static pthread_mutex_t wakeUpASyncReporterMutex;

#endif /* !defined(J9ZOS390) */

static omrthread_monitor_t asyncMonitor;
static omrthread_monitor_t masterHandlerMonitor;
static omrthread_monitor_t asyncReporterShutdownMonitor;
static uint32_t asyncThreadCount;
static uint32_t attachedPortLibraries;

struct J9SignalHandlerRecord {
	struct J9SignalHandlerRecord *previous;
	struct OMRPortLibrary *portLibrary;
	omrsig_handler_fn handler;
	void *handler_arg;
	sigjmp_buf returnBuf;
#if defined(J9ZOS390)
	struct __jumpinfo farJumpInfo;
#endif /* defined(J9ZOS390) */
	uint32_t flags;
};

typedef struct J9CurrentSignal {
	int signal;
	siginfo_t *sigInfo;
	void *contextInfo;
#if defined(S390) && defined(LINUX)
	uintptr_t breakingEventAddr;
#endif
	uint32_t portLibSignalType;
} J9CurrentSignal;

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
	{OMRPORT_SIG_FLAG_SIGXFSZ, SIGXFSZ}
#if defined(AIXPPC)
	, {OMRPORT_SIG_FLAG_SIGRECONFIG, SIGRECONFIG}
#endif
#if defined(J9ZOS390)
	, {OMRPORT_SIG_FLAG_SIGABEND, SIGABND}
#endif

};

static omrthread_t asynchSignalReporterThread = NULL;
static int32_t registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags);
static void removeAsyncHandlers(OMRPortLibrary *portLibrary);
static uint32_t mapUnixSignalToPortLib(uint32_t signalNo, siginfo_t *sigInfo);
#if defined(J9ZOS390)
static intptr_t addAsyncSignalsToSet(sigset_t *ss);
#endif /* defined(J9ZOS390) */
#if defined(OMR_PORT_ASYNC_HANDLER)
static int J9THREAD_PROC asynchSignalReporter(void *userData);
#endif
static uint32_t registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, unix_sigaction handler);
static uint32_t destroySignalTools(OMRPortLibrary *portLibrary);
static int mapPortLibSignalToUnix(uint32_t portLibSignal);
static uint32_t countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category);
static void sig_full_shutdown(struct OMRPortLibrary *portLibrary);
static int32_t initializeSignalTools(OMRPortLibrary *portLibrary);

#if defined(S390) && defined(LINUX)
static void masterSynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo, uintptr_t breakingEventAddr);
static void masterASynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo, uintptr_t nullArg);
#else
static void masterSynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo);
static void masterASynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo);
#endif

int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags)
{
	uint32_t supportedFlags = OMRPORT_SIG_FLAG_MAY_RETURN;

	Trc_PRT_signal_omrsig_can_protect_entered(flags);

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


	if (J9_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGALLSYNC;
	}

	if (J9_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGQUIT | OMRPORT_SIG_FLAG_SIGABRT | OMRPORT_SIG_FLAG_SIGTERM;
	}

	if (J9_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_SIGXFSZ)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGXFSZ;
	}

	if (J9_ARE_ALL_BITS_SET(supportedFlags, flags)) {
		Trc_PRT_signal_omrsig_can_protect_exiting_is_able_to_protect(supportedFlags);
		return 1;
	} else {
		Trc_PRT_signal_omrsig_can_protect_exiting_is_not_able_to_protect(supportedFlags);
		return 0;
	}
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

	struct J9SignalHandlerRecord thisRecord;
	omrthread_t thisThread;
	uint32_t rc = 0;
	uint32_t flagsSignalsOnly;
	uint32_t flagsWithoutMasterHandlers;

	Trc_PRT_signal_omrsig_protect_entered(fn, fn_arg, handler, handler_arg, flags);

	if (J9_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		/* -Xrs was set, we can't protect against any signals, do not install the master handler */
		Trc_PRT_signal_omrsig_protect_cannot_protect_dueto_Xrs(fn, fn_arg, flags);
		*result = fn(portLibrary, fn_arg);
		Trc_PRT_signal_omrsig_protect_exiting_did_not_protect_due_to_Xrs(fn, fn_arg, handler, handler_arg, flags);
		return 0;
	}

	/* Check to see if we have already installed a masterHandler for these signal(s).
	 * We will need to aquire the masterHandlerMonitor if flagsWithoutMasterHandlers is not 0 in order to install the handlers (and to check if another thread had "just" taken care of it).
	 * If we do have masterHandlers installed already then OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS has not been set.
	 */
	flagsSignalsOnly = flags & OMRPORT_SIG_FLAG_SIGALLSYNC;
	flagsWithoutMasterHandlers = flagsSignalsOnly & (~signalsWithMasterHandlers);

	if (0 != flagsWithoutMasterHandlers) {

		/* we have not installed handlers for all the signals, aquire the masterHandlerMonitor, check again and install handlers as required all done in registerMasterHandlers */
		omrthread_monitor_enter(masterHandlerMonitor);
		rc = registerMasterHandlers(portLibrary, flags, OMRPORT_SIG_FLAG_SIGALLSYNC);
		omrthread_monitor_exit(masterHandlerMonitor);

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

	if (J9_ARE_ANY_BITS_SET(flags, OMRPORT_SIG_FLAG_MAY_RETURN)) {

		J9CurrentSignal *currentSignal;

		/* Record the current signal. We need to store this value back into tls if we jump back into this function
		 * because any signals that may have occurred within the scope of this layer of protection would have been handled
		 * by that point.
		 *
		 * The only scenario where this is of real concern, is if more than one signal was handled per call to omrsig_protect. In
		 * this case, the current signal in tls will be pointing at a stale stack frame and signal: CMVC 126838
		 */
		currentSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);

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
uint32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary,  omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{

	uint32_t rc = 0;
	J9UnixAsyncHandlerRecord *cursor;
	J9UnixAsyncHandlerRecord **previousLink;

	Trc_PRT_signal_omrsig_set_async_signal_handler_entered(handler, handler_arg, flags);

	omrthread_monitor_enter(masterHandlerMonitor);

	if (J9_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		/* -Xrs was set, we can't protect against any signals, do not install any handlers except SIGXFSZ*/
		if (J9_ARE_ANY_BITS_SET(flags, OMRPORT_SIG_FLAG_SIGXFSZ) && J9_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_SIGXFSZ)) {
			rc = registerMasterHandlers(portLibrary, OMRPORT_SIG_FLAG_SIGXFSZ, OMRPORT_SIG_FLAG_SIGALLASYNC);
		} else {
			Trc_PRT_signal_omrsig_set_async_signal_handler_will_not_set_handler_due_to_Xrs(handler, handler_arg, flags);
			rc = -1;
		}
	} else {
		rc = registerMasterHandlers(portLibrary, flags, OMRPORT_SIG_FLAG_SIGALLASYNC);
	}
	omrthread_monitor_exit(masterHandlerMonitor);

	if (0 != rc) {
		Trc_PRT_signal_omrsig_set_async_signal_handler_exiting_did_nothing_possible_error(handler, handler_arg, flags);
		return OMRPORT_SIG_ERROR;
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
			J9UnixAsyncHandlerRecord *record = portLibrary->mem_allocate_memory(portLibrary, sizeof(*record), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

			if (NULL == record) {
				rc = 1;
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
	omrthread_monitor_t globalMonitor;
	uint32_t index;

	Trc_PRT_signal_omrsig_startup_entered(portLibrary);

	globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
	if (attachedPortLibraries++ == 0) {

		/* initialize the old actions */
		for (index = 0; index < MAX_UNIX_SIGNAL_TYPES; index++) {
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
	void *value;
	const char *name;
	uint32_t count = 0;

	while (portLibrary->sig_info(portLibrary, info, category, count, &name, &value) != OMRPORT_SIG_VALUE_UNDEFINED) {
		count++;
	}

	return count;
}

/**
 * Reports the asynchronous signal to all listeners.
 */
#if defined(OMR_PORT_ASYNC_HANDLER)
static int J9THREAD_PROC
asynchSignalReporter(void *userData)
{
	J9UnixAsyncHandlerRecord *cursor;
	uint32_t asyncSignalFlag = 0;

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

	for (;;) {

		Trc_PRT_signal_omrsig_asynchSignalReporterThread_going_to_sleep();

#if defined(J9ZOS390)
		/*
		 * I won't attempt to generate diagnostics if the following pthread
		 * functions return errors because it may interfere with diagnostics
		 * we are attempting to generate for earlier events.
		 */

		pthread_mutex_lock(&wakeUpASyncReporterMutex);
		Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up();

		for (;;) {
			if (0 != signalCounts[SIGQUIT_PENDING]) {
				Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGQUIT();
				asyncSignalFlag = OMRPORT_SIG_FLAG_SIGQUIT;
				signalCounts[SIGQUIT_PENDING]--;
				break;
			} else if (0 != signalCounts[SIGABRT_PENDING]) {
				Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGABRT();
				asyncSignalFlag = OMRPORT_SIG_FLAG_SIGABRT;
				signalCounts[SIGABRT_PENDING]--;
				break;
			} else if (0 != signalCounts[SIGTERM_PENDING]) {
				Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGTERM();
				asyncSignalFlag = OMRPORT_SIG_FLAG_SIGTERM;
				signalCounts[SIGTERM_PENDING]--;
				break;
			} else if (0 != signalCounts[SIGXFSZ_PENDING]) {
				Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGXFSZ();
				asyncSignalFlag = OMRPORT_SIG_FLAG_SIGXFSZ;
				signalCounts[SIGXFSZ_PENDING]--;
				break;
			} else if (0 != shutDownASynchReporter) {
				Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_shutdown();
				asyncSignalFlag = 0;
				break;
			} else {
				pthread_cond_wait(&wakeUpASyncReporterCond, &wakeUpASyncReporterMutex);
			}
		}
		pthread_mutex_unlock(&wakeUpASyncReporterMutex);

		/* we get woken up if there is a signal pending or it is time to shutdown */
		if (0 != shutDownASynchReporter) {
			break;
		}
#else /* defined(J9ZOS390) */

		/* CMVC  119663 sem_wait can return -1/EINTR on signal in NPTL */
		while (0 != SIGSEM_WAIT(wakeUpASyncReporter));

		Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up();

		/* we get woken up if there is a signal pending or it is time to shutdown */
		if (0 != shutDownASynchReporter) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_shutdown();
			break;
		}

		/* determine which signal we've been woken up for */
		if (0 == SIGSEM_TRY_WAIT(sigQuitPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGQUIT();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGQUIT;
		} else if (0 == SIGSEM_TRY_WAIT(sigAbrtPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGABRT();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGABRT;
		} else if (0 == SIGSEM_TRY_WAIT(sigTermPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGTERM();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGTERM;
		} else if (0 == SIGSEM_TRY_WAIT(sigXfszPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGXFSZ();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGXFSZ;
#if defined(AIXPPC)
		} else if (0 == SIGSEM_TRY_WAIT(sigReconfigPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGRECONFIG();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGRECONFIG;
#endif
		}
#endif /* defined(J9ZOS390) */

		/* report the signal recorded in signalType to all registered listeners (for this signal) */
		/* incrementing the asyncThreadCount will prevent the list from being modified while we use it */
		omrthread_monitor_enter(asyncMonitor);
		asyncThreadCount++;
		omrthread_monitor_exit(asyncMonitor);

		cursor = asyncHandlerList;
		while (NULL != cursor) {
			if (J9_ARE_ANY_BITS_SET(cursor->flags, asyncSignalFlag)) {
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
		if (J9_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
			int unixSignal = mapPortLibSignalToUnix(asyncSignalFlag);
			/* mapPortLibSignalToUnix returns -1 on unknown mapping */
			if (-1 != unixSignal) {
				omrsig_handler(unixSignal, NULL, NULL);
			}
		}
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

		/* reset the signal store */
		asyncSignalFlag = 0;
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
#else
static void
masterSynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo)
#endif
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
	if (J9_ARE_ANY_BITS_SET(platformContext->uc_mcontext.regs->msr, MSR_TS_MASK))
		/* the transactional context is in the high order bits */
#else
#define MSR_TS_MASK 0x6U
	/*
	 * in 32-bit CPUs, the second context containing the transactional
	 * state is in a separate ucontext datastructure pointed to by uc_link.
	 */
	if ((NULL != platformContext->uc_link) && J9_ARE_ANY_BITS_SET(platformContext->uc_link->uc_mcontext.regs->msr, MSR_TS_MASK))
#endif
	{
		/*
		 * resume the transaction in the failed state, so it executes the failure path.
		 */
		return;
	}
#endif

	omrthread_t thisThread = omrthread_self();
	uint32_t result = U_32_MAX;

	if (NULL != thisThread) {

		uint32_t portLibType;
		struct J9SignalHandlerRecord *thisRecord;
		struct J9CurrentSignal currentSignal;
		struct J9CurrentSignal *previousSignal;

		thisRecord = NULL;

		portLibType = mapUnixSignalToPortLib(signal, sigInfo);

		/* record this signal in tls so that omrsig_handler can be called if any of the handlers decide we should be shutting down */
		currentSignal.signal = signal;
		currentSignal.sigInfo = sigInfo;
		currentSignal.contextInfo = contextInfo;
		currentSignal.portLibSignalType = portLibType;
#if defined(S390) && defined(LINUX)
		currentSignal.breakingEventAddr = breakingEventAddr;
#endif

		previousSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);

		omrthread_tls_set(thisThread, tlsKeyCurrentSignal, &currentSignal);

		/* walk the stack of registered handlers from top to bottom searching for one which handles this type of exception */
		thisRecord = omrthread_tls_get(thisThread, tlsKey);

		while (NULL != thisRecord) {

			if (J9_ARE_ANY_BITS_SET(thisRecord->flags, portLibType)) {
				struct J9UnixSignalInfo j9Info;
				struct J9PlatformSignalInfo platformSignalInfo;


				/* the equivalent of these memsets were here before, but were they needed? */
				memset(&j9Info, 0, sizeof(j9Info));
				memset(&platformSignalInfo, 0, sizeof(platformSignalInfo));

				j9Info.portLibrarySignalType = portLibType;
				j9Info.handlerAddress = (void *)thisRecord->handler;
				j9Info.handlerAddress2 = (void *)masterSynchSignalHandler;
				j9Info.sigInfo = sigInfo;
				j9Info.platformSignalInfo = platformSignalInfo;

				/* found a suitable handler */
				/* what signal type do we want to pass on here? port or platform based ?*/
				fillInUnixSignalInfo(thisRecord->portLibrary, contextInfo, &j9Info);
#if defined(S390) && defined(LINUX)
				j9Info.platformSignalInfo.breakingEventAddr = breakingEventAddr;
#endif

				/* remove the handler we are about to invoke, now, in case the handler crashes */
				omrthread_tls_set(thisThread, tlsKey, thisRecord->previous);

				result = thisRecord->handler(thisRecord->portLibrary, portLibType, &j9Info, thisRecord->handler_arg);

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

	if ((OMRPORT_SIG_EXCEPTION_COOPERATIVE_SHUTDOWN == result) || (J9_ARE_ALL_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_COOPERATIVE_SHUTDOWN))) {

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
#else
		/* 31-bit: request the condition information block and verify that we got it */
		_CEECIB *conditionInfoBlock = NULL;
		_FEEDBACK cibfc;

		CEE3CIB(NULL, &conditionInfoBlock, &cibfc);

		if (0 ==  _FBCHECK(cibfc , CEE000))
#endif
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
		if (J9_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
			omrsig_handler(signal, (void *)sigInfo, contextInfo);
		}
#endif /* defined(OMRPORT_OMRSIG_SUPPORT) */

		if (TRUE == triggerAbend) {
			sigrelse(SIGABND); /* CMVC 191934: need to unblock sigabnd before issuing the abend call */
#if defined(OMR_ENV_DATA64)
			__cabend(PORT_ABEND_CODE, PORT_ABEND_REASON_CODE, PORT_ABEND_CLEANUP_CODE /* normal termination processing */);
#else
			{
				_INT4 code = PORT_ABEND_CODE;
				_INT4 reason = PORT_ABEND_REASON_CODE;
				_INT4 cleanup = PORT_ABEND_CLEANUP_CODE;	 /* normal termination processing */

				CEE3AB2(&code, &reason, &cleanup);
			}
#endif
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
	if (J9_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
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
#else
static void
masterASynchSignalHandler(int signal, siginfo_t *sigInfo, void *contextInfo)
#endif
{
	uint32_t portLibSignalType;

	portLibSignalType = mapUnixSignalToPortLib(signal, sigInfo);

#if !defined(J9ZOS390)

	/* Todo: default? */
	switch (portLibSignalType) {
	case OMRPORT_SIG_FLAG_SIGQUIT:
		SIGSEM_POST(sigQuitPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGABRT:
		SIGSEM_POST(sigAbrtPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGTERM:
		SIGSEM_POST(sigTermPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGRECONFIG:
		SIGSEM_POST(sigReconfigPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGXFSZ:
		SIGSEM_POST(sigXfszPendingSem);
		break;
	}

	SIGSEM_POST(wakeUpASyncReporter);

#else /* !defined(J9ZOS390) */

	pthread_mutex_lock(&wakeUpASyncReporterMutex);
	switch (portLibSignalType) {
	case OMRPORT_SIG_FLAG_SIGQUIT:
		signalCounts[SIGQUIT_PENDING]++;
		break;
	case OMRPORT_SIG_FLAG_SIGABRT:
		signalCounts[SIGABRT_PENDING]++;
		break;
	case OMRPORT_SIG_FLAG_SIGTERM:
		signalCounts[SIGTERM_PENDING]++;
		break;
	case OMRPORT_SIG_FLAG_SIGXFSZ:
		signalCounts[SIGXFSZ_PENDING]++;
		break;
	}
	pthread_cond_signal(&wakeUpASyncReporterCond);
	pthread_mutex_unlock(&wakeUpASyncReporterMutex);
#endif /* !defined(J9ZOS390) */

	return;

}

/**
 * Register the signal handler with the OS, generally used to register the master signal handlers
 * Not to be confused with omrsig_protect, which registers the user's handler with the port library.
 *
 * Calls to this function must be synchronized using "masterHandlerMonitor".
 *
 * The use of this function forces the flags SA_RESTART | SA_SIGINFO | SA_NODEFER to be set for the new signal action
 *
 * The old action for the signal handler is stored in oldActions. These must be restored before the portlibrary is shut down.
 *
 * @return 0 upon success, non-zero otherwise.
 */
static uint32_t
registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo,  unix_sigaction handler)
{
	struct sigaction newAction;
	int unixSignalNo = mapPortLibSignalToUnix(portLibrarySignalNo);

	/* do not block any signals */
	if (0 != sigemptyset(&newAction.sa_mask)) {
		return -1;
	}

	/* Automatically restart system calls that get interrupted by any signal
	 * Neutrino V6.3 does not support this
	 */
	newAction.sa_flags = SA_RESTART;

	/* Setting to SA_SIGINFO will result in "void (*sa_sigaction) (int, siginfo_t *, void *)" to be used, and not "__sighandler_t sa_handler". Both are members of struct sigaction.
	 * Using the former allows us to access more than just the signal number
	 */
	newAction.sa_flags |= SA_SIGINFO;

	/* SA_NODEFER prevents the current signal from being masked by default in the handler.
	 * However, it can still be masked if one explicitly requests so in sa_mask field, as we do on z/OS
	 */
	newAction.sa_flags |= SA_NODEFER;

#if defined(J9ZOS390)
	/* z/OS doesn't have posix semaphores. 
	 * A side effect of that is that we don't want to re-enter the masterASyncHandler
	 * Therefore, mask all async signals for the masterASyncHandler. The signal(s) will be queued and delivered
	 * to the masterASyncHandler once the handler returns. No signals will be lost.
	 */
	if (J9_ARE_ANY_BITS_SET(portLibrarySignalNo, OMRPORT_SIG_FLAG_SIGALLASYNC)) {
		if (0 != addAsyncSignalsToSet(&newAction.sa_mask)) {
			return -1;
		}
	}
#endif /* defined(J9ZOS390) */

#if defined(AIXPPC)
	/* if we are installing a handler for an asynchronous signal block SIGTRAP */
	if (J9_ARE_ANY_BITS_SET(portLibrarySignalNo, OMRPORT_SIG_FLAG_SIGALLASYNC)) {
		if (sigaddset(&newAction.sa_mask, SIGTRAP)) {
			return -1;
		}
	}
#endif

	/* The master exception handler:
	 * The (void *) casting only applies to zLinux because a new parameter "uintptr_t breakingEventAddr"
	 * has been introduced in masterSynchSignalHandler() to obtain BEA on zLinux but not for other platforms.
	 * As a result, the total number of parameters in masterSynchSignalHandler() on zLinux
	 * is 4 while the number is 3 on other platforms, coz there is no need to change the signature of
	 * masterSynchSignalHandler in there. Since the code is shared on all platforms,
	 * the change here is used to split them up to avoid any compiling error.
	 */
#if defined(S390) && defined(LINUX)
	newAction.sa_sigaction = (void *)handler;
#else
	newAction.sa_sigaction = handler;
#endif

	/* now that we've set up the sigaction struct the way we want it, register the handler with the OS */
	if (OMRSIG_SIGACTION(unixSignalNo, &newAction, &oldActions[unixSignalNo].action)) {
		Trc_PRT_signal_registerSignalHandlerWithOS_failed_to_registerHandler(portLibrarySignalNo, unixSignalNo, handler);
		return -1;
	} else {
		Trc_PRT_signal_registerSignalHandlerWithOS_registeredHandler(portLibrarySignalNo, unixSignalNo, handler);
		oldActions[unixSignalNo].restore = 1;
	}

	/* CMVC 96193 signalsWithMasterHandlers is checked without acquiring the masterHandlerMonitor */
	issueWriteBarrier();

	/* we've successfully registered the master handler for this, record it! */
	signalsWithMasterHandlers |= portLibrarySignalNo;

	return 0;
}

/**
 * The unix signal number is converted to the corresponding port library
 * signal number.
 *
 * Some signals have subtypes which are detailed in the siginfo_t structure.
 */
static uint32_t
mapUnixSignalToPortLib(uint32_t signalNo, siginfo_t *sigInfo)
{
	uint32_t index;

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); index++) {
		if (signalMap[index].unixSignalNo == signalNo) {

			if (SIGFPE == signalNo) {

				if (0 != sigInfo) {
					/* If we are not looking up the mapping in response to a signal
					 * we will not have a siginfo_t structure */

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
			}

			return signalMap[index].portLibSignalNo;
		}
	}

	return 0;
}

/**
 * The defined port library signal is converted to the corresponding unix
 * signal number.
 *
 * Note that FPE signal codes (subtypes) all map to the same signal number and are not included
 *
 * @param portLibSignal The internal J9 Port Library signal number
 * @return The corresponding Unix signal number or -1 if the portLibSignal could not be mapped
 */
static int
mapPortLibSignalToUnix(uint32_t portLibSignal)
{
	uint32_t index;

	/* mask out subtypes */
#if defined(AIXPPC)
	portLibSignal &= OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_SIGQUIT | OMRPORT_SIG_FLAG_SIGABRT | OMRPORT_SIG_FLAG_SIGTERM | OMRPORT_SIG_FLAG_SIGXFSZ | OMRPORT_SIG_FLAG_SIGRECONFIG;
#else
	portLibSignal &= OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_SIGQUIT | OMRPORT_SIG_FLAG_SIGABRT | OMRPORT_SIG_FLAG_SIGTERM | OMRPORT_SIG_FLAG_SIGXFSZ;
#endif

	for (index = 0; index < sizeof(signalMap) / sizeof(signalMap[0]); index++) {

		if (signalMap[index].portLibSignalNo == portLibSignal) {
			return signalMap[index].unixSignalNo;
		}
	}
	Trc_PRT_signal_mapPortLibSignalToUnix_ERROR_unknown_signal(portLibSignal);
	return -1;
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

	/* iterate through all the known signals */
	for (i = 0; i < sizeof(signalMap) / sizeof(signalMap[0]); i++) {

		if (J9_ARE_ANY_BITS_SET(signalMap[i].portLibSignalNo, OMRPORT_SIG_FLAG_SIGALLASYNC)) {
			/* add the current signal to the signal set */
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
 *
 * Calls to this function must be synchronized using masterHandlerMonitor.
 *
 * @param[in] flags the flags that we want signals for
 * @param[in] allowedSubsetOfFlags must be one of
 * OMRPORT_SIG_FLAG_SIGALLSYNC, or OMRPORT_SIG_FLAG_SIGALLASYNC
 *
 * @return	0 upon success; OMRPORT_SIG_ERROR otherwise.
 *			Possible failure scenarios include attempting to register a handler for
 *			a signal that is not included in the allowedSubsetOfFlags
*/
static int32_t
registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags)
{
	uint32_t flagsSignalsOnly, flagsWithoutHandlers;
	unix_sigaction handler;

	if (OMRPORT_SIG_FLAG_SIGALLSYNC == allowedSubsetOfFlags) {
		handler = masterSynchSignalHandler;
	} else if (OMRPORT_SIG_FLAG_SIGALLASYNC == allowedSubsetOfFlags) {
		handler = masterASynchSignalHandler;
	} else {
		return OMRPORT_SIG_ERROR;
	}

	flagsSignalsOnly = flags & allowedSubsetOfFlags;
	flagsWithoutHandlers = flagsSignalsOnly & (~signalsWithMasterHandlers);

	if (0 != flagsWithoutHandlers) {
		/* we be registerin' some handlers */
		uint32_t portSignalType = 0;

		/* portSignalType starts off at 4 as it is the smallest synch
		 * signal. In the case that we are registering an asynch
		 * signal, flagsWithoutHandlers already has masked off the
		 * synchronous signals (eg. "4") so we are not at risk of registering a handler
		 * for an incorrect sigal
		 */
		for (portSignalType = 4; portSignalType < allowedSubsetOfFlags; portSignalType = portSignalType << 1) {
			/* iterate through all the  signals and register the master handler for those that don't have one yet */

			if (J9_ARE_ANY_BITS_SET(flagsWithoutHandlers, portSignalType)) {
				/* we need a master handler for this (portSignalType's) signal */

				if (0 != registerSignalHandlerWithOS(portLibrary, portSignalType, handler)) {
					return OMRPORT_SIG_ERROR;
				}
			}
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

	/* use this to record the last signal that occured such that we can call omrsig_handler in omrexit_shutdown_and_exit */
	if (omrthread_tls_alloc(&tlsKeyCurrentSignal)) {
		return -1;
	}

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
	if (0 != ceehdlr_startup(portLibrary)) {
		return -1;
	}
#endif

	if (omrthread_monitor_init_with_name(&masterHandlerMonitor, 0, "portLibrary_omrsig_masterHandler_monitor")) {
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
	portLibrary->str_printf(portLibrary, semNames[1], 128, "/omr/sigQuitPending-%d", getpid());
	portLibrary->str_printf(portLibrary, semNames[2], 128, "/omr/sigAbrtPending-%d", getpid());
	portLibrary->str_printf(portLibrary, semNames[3], 128, "/omr/sigTermPending-%d", getpid());
	portLibrary->str_printf(portLibrary, semNames[4], 128, "/omr/sigReconfigPending-%d", getpid());
	portLibrary->str_printf(portLibrary, semNames[5], 128, "/omr/sigXfszPending-%d", getpid());
#define SIGSEM_NAME(_i) semNames[_i]
#else /* defined(OSX) */
#define SIGSEM_NAME(_i) NULL
#endif /* defined(OSX) */

	/* The asynchronous signal reporter will wait on this semaphore  */
	if (SIGSEM_ERROR == SIGSEM_INIT(wakeUpASyncReporter, SIGSEM_NAME(0)))	 {
		return -1;
	}
	SIGSEM_UNLINK(SIGSEM_NAME(0));

	/* The asynchronous signal reporter keeps track of the number of pending singals with these... */
	if (SIGSEM_ERROR == SIGSEM_INIT(sigQuitPendingSem, SIGSEM_NAME(1)))	 {
		return -1;
	}
	SIGSEM_UNLINK(SIGSEM_NAME(1));
	if (SIGSEM_ERROR == SIGSEM_INIT(sigAbrtPendingSem, SIGSEM_NAME(2)))	 {
		return -1;
	}
	SIGSEM_UNLINK(SIGSEM_NAME(2));
	if (SIGSEM_ERROR == SIGSEM_INIT(sigTermPendingSem, SIGSEM_NAME(3)))	 {
		return -1;
	}
	SIGSEM_UNLINK(SIGSEM_NAME(3));
	if (SIGSEM_ERROR == SIGSEM_INIT(sigReconfigPendingSem, SIGSEM_NAME(4)))	 {
		return -1;
	}
	SIGSEM_UNLINK(SIGSEM_NAME(4));
	if (SIGSEM_ERROR == SIGSEM_INIT(sigXfszPendingSem, SIGSEM_NAME(5)))	 {
		return -1;
	}
	SIGSEM_UNLINK(SIGSEM_NAME(5));
#undef SIGSEM_NAME

#else /* !defined(J9ZOS390) */

	if (pthread_mutex_init(&wakeUpASyncReporterMutex, NULL)) {
		return -1;
	}

	if (pthread_cond_init(&wakeUpASyncReporterCond, NULL)) {
		return -1;
	}
#if defined(J9ZOS390)
	if (TRUE == checkIfResumableTrapsSupported(portLibrary)) {
		PPG_resumableTrapsSupported = TRUE;
	} else {
		PPG_resumableTrapsSupported = FALSE;
	}
#endif /* defined(J9ZOS390) */
#endif /* defined(OSX) */

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
#endif

	return 0;
}

static int32_t
setReporterPriority(OMRPortLibrary *portLibrary, uintptr_t priority)
{

	Trc_PRT_signal_setReporterPriority(portLibrary, priority);

	if (asynchSignalReporterThread == NULL) {
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
	omrthread_monitor_destroy(masterHandlerMonitor);
	omrthread_monitor_destroy(asyncReporterShutdownMonitor);
	omrthread_monitor_destroy(asyncMonitor);
#if !defined(J9ZOS390)
	SIGSEM_DESTROY(wakeUpASyncReporter);
	SIGSEM_DESTROY(sigQuitPendingSem);
	SIGSEM_DESTROY(sigAbrtPendingSem);
	SIGSEM_DESTROY(sigTermPendingSem);
	SIGSEM_DESTROY(sigReconfigPendingSem);
	SIGSEM_DESTROY(sigXfszPendingSem);
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

	if ((OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)  & options)  {
		/* check that we haven't already set any master handlers */

		uint32_t anyHandlersInstalled = 0;

		omrthread_monitor_enter(masterHandlerMonitor);
		if (0 != signalsWithMasterHandlers) {
			anyHandlersInstalled = 1;
		}
		omrthread_monitor_exit(masterHandlerMonitor);

		if (anyHandlersInstalled) {
			Trc_PRT_signal_omrsig_set_options_too_late_handlers_installed(options);
			return -1;
		}
	}

#if defined(OMR_PORT_ZOS_CEEHDLRSUPPORT)
	if (0 != (options & OMRPORT_SIG_OPTIONS_ZOS_USE_CEEHDLR)) {
		/* We are being told to use LE condition handling.
		 * Switch over to LE condition handling, unless we have already installed POSIX handlers for any sync signals
		 **/

		int32_t syncHandlersInstalled = 0;

		omrthread_monitor_enter(masterHandlerMonitor);

		if (0 == (signalsWithMasterHandlers & OMRPORT_SIG_FLAG_SIGALLSYNC)) {
			/* we haven't installed any synchronous handlers, so it's OK to switch to LE condition handling */
			portLibrary->sig_protect = omrsig_protect_ceehdlr;
			portLibrary->sig_info = omrsig_info_ceehdlr;
			portLibrary->sig_get_current_signal = omrsig_get_current_signal_ceehdlr;
		} else {
			/* set syncHandlersInstalled so that we return failure */
			syncHandlersInstalled = 1;
		}

		omrthread_monitor_exit(masterHandlerMonitor);

		if (syncHandlersInstalled == 1) {
			Trc_PRT_signal_omrsig_set_options_too_late_handlers_installed(options);
			return -1;
		}
	}
#endif

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
	J9CurrentSignal *currentSignal = omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);
	if (currentSignal == NULL) {
		return 0;
	}
	return currentSignal->portLibSignalType;
}

static void
sig_full_shutdown(struct OMRPortLibrary *portLibrary)
{
	omrthread_monitor_t globalMonitor;
	uint32_t index;

	Trc_PRT_signal_sig_full_shutdown_enter(portLibrary);
	globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
	if (--attachedPortLibraries == 0) {

		/* register the old actions we overwrote with our own */
		for (index = 0; index < MAX_UNIX_SIGNAL_TYPES; index++) {
			if (oldActions[index].restore) {
				OMRSIG_SIGACTION(index, &oldActions[index].action, NULL);
				/* record that we no longer have a handler installed with the OS for this signal */
				Trc_PRT_signal_sig_full_shutdown_deregistered_handler_with_OS(portLibrary, index);
				signalsWithMasterHandlers &= ~mapUnixSignalToPortLib(index, 0);
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
#endif

	}
	omrthread_monitor_exit(globalMonitor);
	Trc_PRT_signal_sig_full_shutdown_exiting(portLibrary);
}

static void
removeAsyncHandlers(OMRPortLibrary *portLibrary)
{
	/* clean up the list of async handlers */
	J9UnixAsyncHandlerRecord *cursor;
	J9UnixAsyncHandlerRecord **previousLink;

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

	J9CurrentSignal *currentSignal = omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);

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

