/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
 * @file omrsignal.c
 * @ingroup Port
 * @brief Signal handling details exclusive to z/TPF.&nbsp;&nbsp;  
 *
 * \details z/TPF does not have <i>reliable signals</i>, also known as <i>kernel-space</i> 
 * signals in certain circles. Instead, all signals processing takes place in user space. Normally, the 
 * act of delivering a signal (via <tt>kill()</tt>, <tt>pthread_kill()</tt>, <tt>raise()</tt>, et al) takes 
 * place in user-space with no supervisory intervention; and once it's delivered, the process (or thread) must
 * <i>declare itself ready to accept signals</i> via either a <tt>tpf_process_signals()</tt> or 
 * <tt>tpf_thread_process_signals()</tt> call. That's going to sound weird to a modern UNIX 
 * programmer, but that's the way it is, and it actually <i>works</i> for certain kinds
 * of programs. Unfortunately, those programs which expect to be rerouted to a signal handler
 * without warning (or consciously <i>"being ready" to process a signal</i>) aren't 
 * among that set of code. In other words, there are no such things as 'synchronous signals'
 * in z/TPF: all units of work (UOW) must declare themselves ready and willing to process any
 * signals which may have been delivered to them since the last such declaration through the
 * issuance of a <tt>tpf_process_signals()</tt> call.
 *
 * UNIX, on which most of this segment was originally founded, _does_ have reliable
 * signals whereas z/TPF has none. Reliable signals is a way of saying that when a signal is 
 * delivered and is NOT masked out, control is immediately transferred to the signal handler 
 * registered for that particular signal -- it spends no time in a 'signal pending' state. 
 * The way UNIX scheduling works, a <i>"context switch"</i> is performed when one 
 * scheduleable unit of work (hereafter, UOW) has the CPU taken from it and assigned to the 
 * next scheduleable UOW. To put it another way, the scheduler will change the now active UOW's <i>context</i>
 * from "active, running" to "dormant, awaiting rescheduling" (or whichever event it's awaiting, which 
 * could be an I/O completion, for example). 
 *
 * The dirty work of taking a CPU core away from a UOW is accomplished by stopping
 * the current CPU, taking a copy of the machine state into what's called the <b><i>"machine
 * state context"</i></b> (<tt>struct uc_mcontext</tt>) consisting of the PSW, registers, 
 * etc, then setting its <b>"link state"</b> context (<tt>struct uc_link</tt>) to the next 
 * executable instruction the outgoing UOW would have executed, putting this UOW at the 
 * end of the queue; then selecting the next runnable UOW, restoring <i>its</i> <tt>uc_link</tt>
 * data members to the proper places as dictated by hardware, then causing execution to resume at 
 * the place <tt>uc_link</tt> says it should until the next timeslice expires. <i>Ad infinitum</i>.
 *
 * One of the descriptive statistics of how hard a UNIX-like machine is working is
 * the <i>"context switch per second"</i> rate. A large *NIX server executes approximately
 * twice as many context switches as it does interrupts over any small slice of time; some run 
 * with over 10,000 (and more) context switches per second.
 *
 * z/TPF schedules very differently. For a timesliced process, which all JVM
 * threads belong to, interrupts happen many, many times a second, and at each interrupt,
 * the scheduler decides if the UOW has had its timeslice expire (or for more traditional TPF
 * processes, which perform the equivalent of a <tt>yield()</tt> instruction for SCHED_FIFO
 * scheduling, which is the original scheduling paradigm of the z/TPF system. If so, the machine
 * state is stored in pages 2 and 3 of the Entry Control Block, and its scheduleable
 * UOW address (the <i>Entry Control Block</i> or <b>ECB</b>) is queued on one of at least five 
 * "CPU lists", depending on what its next expected state is. This data placement is the z/TPF
 * equivalent of a UNIX context switch. But <b>the current machine state has nothing to do with 
 * signal deliveries</b>, and that's what this segment is about: <i>the context switch that would have taken place if z/TPF supported 
 * the delivery of reliable signals to the signal handler</i>.
 *
 * What we <em>do</em> have is the full machine state <b>when any dump is taken.</b> Whether
 * the dump is the result of what we've been calling a "non-preemptible" event 
 * (i.e. a <i>program check</i>, meaning that the process is going to exit shortly) 
 * or a "hooked" event (meaning that the Java user
 * has called for a dump on some otherwise normal event taking place, such as a 
 * JVM start or stop, or perhaps a 'soft' Java exception having been thrown, where
 * function <TT>omrdump_create()</TT> has to call for a dump from the z/TPF Control
 * Program), we still have the <b>extremely recent</b> machine state captured for
 * us by the z/TPF SERRC processor, CCCPSE. Therefore, we <i>can</i> create a 'fake'
 * ucontext structure <b>as long as the data the structure is to contain comes from
 * a recent dump</b>. 
 *
 * In this segment, we labor under the presumption that the required dump has indeed been anticipated
 * by the "kernel's" (we call the supervisor the "z/TPF Control Program") program check interrupt routine.
 * If such a dump has been anticipated and the faulting UOW belongs to a process which is known to have 
 * been bound to a JVM, CCCPSE (the Control Program's program check handler) has created what's called 
 * a "Dump Interchange Record", out of which, in turn, is anchored a "Java Dump Buffer", or JDB.
 *
 * CCCPSE determines that eligible program checks fall into one of two categories. The first 
 * category is called a "preemptible error", one which the Java language has specified as a 
 * divide-by-zero or null-pointer exception (DBZ or NPE, respectively). In these situations,
 * the Java interpreter routine receiving the signal is supposed to know what to do with it, 
 * namely issue the 'soft' Java ArithmeticException("Divide-by-zero") or NullPointerException,
 * depending upon the exact type of interrupt taken. The TPF-ish data is translated into its
 * corresponding UNIX-information, such as "context blocks" required for each of these two
 * situations. In these cases, no JDB is anchored in the DIB, as no system dump agent activity
 * will take place.
 *
 * The second category is called a "non-preemptible error", one which CCCPSE cannot classify as
 * preemptible and is associated with a Java program under interpretation or a byte interpreter
 * malfunction. These kinds of program checks indicate something seriously wrong, such as a programming
 * or data error, and infers that the JVM may not be in a condition to continue. In this kind of case,
 * a JDB is anchored off the DIB in anticipation of the system dump agent being required to write
 * an ELF core dump file.
 *
 * The JDB contains in-memory copies of all the native stack frames, an Error Information
 * Record, and has made in-memory copies of every heap (malloc()'d heap nodes) in use by
 * the faulting address space. We write our own ELF core dump file from this information 
 * since the z/TPF kernel does not have this task as one of its duties like a UNIX kernel
 * would.
 *
 * Regardless of the kind of error CCCPSE has determined we have, we always have a DIB, which 
 * contains enough information to create the required context blocks associated with a 
 * synchronous signal. If the error is not preemptible, we have also spawned a thread to write
 * the ELF core dump file, in anticipation that it will be required by the system dump agent's
 * execution, to follow.
 */

#include <omrsig.h>

#include "omrsignal_context.h"
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
#include <semaphore.h>

/*
 * get siginfo_t and SIGRTMIN definitions included
 * Set up a thread to run a tpf_process_signals() loop,
 * also defined here.
*/
#include <tpf/sysapi.h>
#include <tpf/tpfapi.h>
#include <signal.h>
#include <tpf/i_jdib.h>
#include <tpf/i_jdbf.h>

#include "omrsignal.h"		/* declare a couple of platform-unique structures */

static uint32_t	ztpf_enableSignalHandlers(OMRPortLibrary *);
static int J9THREAD_PROC ztpf_fireSignalHandlers(void *);

static uint32_t	shutdown_ztpf_sigpush;
static omrthread_monitor_t ztpfSigShutdownMonitor;

static uint32_t
ztpf_enableSignalHandlers(OMRPortLibrary *portLibrary)
{ 
	int	rc = J9THREAD_SUCCESS;
	omrthread_t ztpf_signal_enabler_thd;

	rc = createThreadWithCategory(&ztpf_signal_enabler_thd, 512*256, J9THREAD_PRIORITY_NORMAL, 0, &ztpf_fireSignalHandlers, portLibrary, J9THREAD_CATEGORY_SYSTEM_THREAD);
	if( J9THREAD_SUCCESS == rc ) { 
		omrthread_set_name( omrthread_self(), "z/TPF Signal handler push loop" );
	}
	return rc;
}

#define WAIT_QUANTUM 50000				/* 50 ms in microsecond units			*/

/*
 * z/TPF does not have kernel threads. As such, signals are delivered reliably to 
 * a process' or thread's pending signal queue; but the signal handler -- if any --
 * for any signal does not receive control until a function called tpf_process_signals()
 * is called. The usleep() function does this on its way into -- and back from -- 
 * an EVNWC service whose timeout value we pass to it in microseconds; we've 
 * #defined it as WAIT_QUANTUM above. Net-net, all we have to do is usleep, and
 * we get any pending signals processed. This is a much lower-overhead operation than
 * a pthread_cond_timedwait() operation or similar; even much lower than a tight DEFRC 
 * loop.
 *
 * It continues to loop forever until the JVM controller thread tells it to kill
 * itself via the <shutdown_ztpf_sigpush> monitor alarm.
 */

static int J9THREAD_PROC 
ztpf_fireSignalHandlers(void *portlib)
{
	OMRPortLibrary *portLibrary = (OMRPortLibrary *)portlib;

	omrthread_monitor_init_with_name(&ztpfSigShutdownMonitor, 0, "ztpf_thread_enabler_shutdown_monitor");

	while(1) {										/* Never break this loop		*/
		if( 0 == shutdown_ztpf_sigpush ) {
			usleep(WAIT_QUANTUM);
		} else { 
			omrthread_monitor_enter(ztpfSigShutdownMonitor);
			shutdown_ztpf_sigpush = 0;
			omrthread_monitor_notify(ztpfSigShutdownMonitor);
			omrthread_exit(ztpfSigShutdownMonitor);	/* This thread is gone.			*/
		}
	}
	return 0;										/* UNREACHABLE					*/
}


#define MAX_PORTLIB_SIGNAL_TYPES 8

typedef  void (*unix_sigaction) (int, siginfo_t *, void *, UDATA);

/* CMVC 96193
 * Prototyping here to avoid including j9protos.h
 */
extern J9_CFUNC void issueWriteBarrier(void);

/* Store the previous signal handlers, we need to restore them when we're done */
static struct {
	struct sigaction action;
	uint32_t restore;
} oldActions[MAX_UNIX_SIGNAL_TYPES];

/* Records the (port library defined) signals for which we have registered a master handler.
 * Access to this must be protected by the masterHandlerMonitor */
static uint32_t signalsWithMasterHandlers;

#if defined(OMR_PORT_ASYNC_HANDLER)
static uint32_t	shutDownASynchReporter;
#endif

static uint32_t	attachedPortLibraries;

typedef struct OMRUnixAsyncHandlerRecord {
	OMRPortLibrary* portLib;
	omrsig_handler_fn handler;
	void *handler_arg;
	uint32_t flags;
	struct OMRUnixAsyncHandlerRecord *next;
} OMRUnixAsyncHandlerRecord;

/* holds the options set by omrsig_set_options */
uint32_t signalOptionsGlobal;

static OMRUnixAsyncHandlerRecord *asyncHandlerList = NULL;

/* asyncSignalReporter synchronization	*/

static sem_t wakeUpASyncReporter;
static sem_t sigQuitPendingSem;
static sem_t sigAbrtPendingSem;
static sem_t sigTermPendingSem;
static sem_t sigReconfigPendingSem;
static sem_t sigXfszPendingSem;

static omrthread_monitor_t asyncMonitor;
static omrthread_monitor_t masterHandlerMonitor;
static omrthread_monitor_t asyncReporterShutdownMonitor;
static uint32_t	asyncThreadCount;
static uint32_t	attachedPortLibraries;

/* key to get the end of the synchronous handler records */
omrthread_tls_key_t tlsKey;

/* key to get the current synchronous signal */
omrthread_tls_key_t tlsKeyCurrentSignal;

struct {
	uint32_t portLibSignalNo;
	int	unixSignalNo;
} signalMap[] =	{
	{OMRPORT_SIG_FLAG_SIGSEGV, SIGSEGV},
	{OMRPORT_SIG_FLAG_SIGILL,  SIGILL},
	{OMRPORT_SIG_FLAG_SIGFPE,  SIGFPE},
	{OMRPORT_SIG_FLAG_SIGQUIT, SIGIQUIT},
	{OMRPORT_SIG_FLAG_SIGABRT, SIGABRT},
	{OMRPORT_SIG_FLAG_SIGTERM, SIGTERM}
};

static omrthread_t asynchSignalReporterThread = NULL;
static int32_t registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags);
static void	removeAsyncHandlers(OMRPortLibrary* portLibrary);
static uint32_t	mapUnixSignalToPortLib(uint32_t signalNo, siginfo_t *sigInfo);
#if defined(OMR_PORT_ASYNC_HANDLER)
static int asynchSignalReporter(void *userData);
#endif
static uint32_t	registerSignalHandlerWithOS(OMRPortLibrary *portLibrary, uint32_t portLibrarySignalNo, unix_sigaction handler);
static uint32_t	destroySignalTools(OMRPortLibrary *portLibrary);
static int32_t	mapPortLibSignalToUnix(uint32_t portLibSignal);
static uint32_t	countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category);
static void	sig_full_shutdown(struct OMRPortLibrary *portLibrary);
static int32_t	initializeSignalTools(OMRPortLibrary *portLibrary);

void masterSynchSignalHandler(int signal, siginfo_t * sigInfo, void *contextInfo, UDATA breakingEventAddr);
static void	masterASynchSignalHandler(int signal, siginfo_t * sigInfo, void *contextInfo, UDATA nullArg);

int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags)
{
	uint32_t supportedFlags = OMRPORT_SIG_FLAG_MAY_RETURN;

	Trc_PRT_signal_omrsig_can_protect_entered(flags);

	supportedFlags |= OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;

	if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGALLSYNC;
	}

	if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGQUIT | OMRPORT_SIG_FLAG_SIGABRT | OMRPORT_SIG_FLAG_SIGTERM;
	}

	if (OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_SIGXFSZ)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGXFSZ;
	}

	if (OMR_ARE_ALL_BITS_SET(supportedFlags, flags)) {
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
omrsig_protect(struct OMRPortLibrary *portLibrary,  omrsig_protected_fn fn, void* fn_arg,
					 omrsig_handler_fn handler, void* handler_arg, uint32_t flags, UDATA *result )
{
	struct OMRSignalHandlerRecord thisRecord;
	omrthread_t thisThread;
	uint32_t rc = 0;
	uint32_t flagsSignalsOnly;
	uint32_t flagsWithoutMasterHandlers;
	
	Trc_PRT_signal_omrsig_protect_entered(fn, fn_arg, handler, handler_arg, flags);

	if (OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		/* -Xrs was set, we can't protect against any signals, do not install the master handler */
		Trc_PRT_signal_omrsig_protect_cannot_protect_dueto_Xrs(fn, fn_arg, flags);
		*result = fn(portLibrary, fn_arg);		
		Trc_PRT_signal_omrsig_protect_exiting_did_not_protect_due_to_Xrs(fn, fn_arg, handler, handler_arg, flags);
		return 0;
	} 

	/*
	 * Check to see if we have already installed a masterHandler for these signal(s).
	 * We will need to aquire the masterHandlerMonitor if flagsWithoutMasterHandlers is not 0
	 * in order to install the handlers (and to check if another thread had "just" taken care of it).
	 *
	 * If we do have masterHandlers installed already then OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS
	 * has not been set.
	 */
	flagsSignalsOnly = flags & OMRPORT_SIG_FLAG_SIGALLSYNC;
	flagsWithoutMasterHandlers = flagsSignalsOnly & (~signalsWithMasterHandlers);

	if (0 != flagsWithoutMasterHandlers) {
		/*
		 * We have not installed handlers for all the signals, so acquire the masterHandlerMonitor,
		 * check again, then install handlers as required: all done in registerMasterHandlers
		 */
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

	if (OMR_ARE_ANY_BITS_SET(flags, OMRPORT_SIG_FLAG_MAY_RETURN)) {
		OMRCurrentSignal *currentSignal;
		/* 
		 * Record the current signal. We need to store this value back into tls if we
		 * jump back into this function because any signals that may have occurred 
		 * within the scope of this layer of protection would have been handled by that point.
		 * 
		 * The only scenario where this is of real concern, is if more than one signal was 
		 * handled per call to omrsig_protect. In this case, the current signal in tls will
		 * be pointing at a stale stack frame and signal: CMVC 126838 
		 */
		currentSignal = omrthread_tls_get(thisThread, tlsKeyCurrentSignal);
		/*
		 * setjmp/longjmp does not clear the mask setup by the OS when it delivers the signal. Use
		 * sigsetjmp/siglongjmp(buf, 1) instead
		 */
		if (0 != setjmp(thisRecord.mark)) {
			/*
			 * The handler has long jumped back here -- reset the 
			 * signal handler stack, and CurrentSignal, then return 
			 */
			omrthread_tls_set(thisThread, tlsKey, thisRecord.previous);
			omrthread_tls_set(thisThread, tlsKeyCurrentSignal, currentSignal);
			*result = 0;
			Trc_PRT_signal_omrsignal_sig_protect_Exit_long_jumped_back_to_omrsig_protect(fn, fn_arg, handler,
																					   handler_arg, flags); 
			return OMRPORT_SIG_EXCEPTION_OCCURRED;
		}
	}

	if (0 != omrthread_tls_set(thisThread, tlsKey, &thisRecord)) {
		Trc_PRT_signal_omrsignal_sig_protect_Exit_ERROR_accessing_tls(fn, fn_arg, handler, handler_arg, flags);
		return OMRPORT_SIG_ERROR;
	}

	*result = fn(portLibrary, fn_arg);
	/*
	 * if the first omrthread_tls_set succeeded, then this one will always succeed
	 */
	omrthread_tls_set(thisThread, tlsKey, thisRecord.previous);

	Trc_PRT_signal_omrsignal_sig_protect_Exit_after_returning_from_fn(fn, fn_arg, handler, handler_arg, flags, *result);
	return 0;
}


int32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary* portLibrary, omrsig_handler_fn handler,
							   void* handler_arg, uint32_t flags)
{
	int32_t rc = 0;
	OMRUnixAsyncHandlerRecord *cursor;
	OMRUnixAsyncHandlerRecord **previousLink;

	Trc_PRT_signal_omrsig_set_async_signal_handler_entered(handler, handler_arg, flags);
	
	omrthread_monitor_enter(masterHandlerMonitor);

	if (OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		/*
		 * -Xrs was set, we can't protect against any signals,
		 *  do not install any handlers except SIGXFSZ.
		 */
		if (OMR_ARE_ANY_BITS_SET(flags, OMRPORT_SIG_FLAG_SIGXFSZ)
			&& OMR_ARE_ANY_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_SIGXFSZ)) {
			rc = registerMasterHandlers(portLibrary, OMRPORT_SIG_FLAG_SIGXFSZ, OMRPORT_SIG_FLAG_SIGALLASYNC);
		} else {
			Trc_PRT_signal_omrsig_set_async_signal_handler_will_not_set_handler_due_to_Xrs(handler, handler_arg, flags);
			rc = OMRPORT_SIG_ERROR;
		}
	} else {
		rc = registerMasterHandlers(portLibrary, flags, OMRPORT_SIG_FLAG_SIGALLASYNC);
	}
	omrthread_monitor_exit(masterHandlerMonitor);

	if (0 != rc) {
		Trc_PRT_signal_omrsig_set_async_signal_handler_exiting_did_nothing_possible_error(handler, handler_arg, flags);
		return rc;
	}
	
	omrthread_monitor_enter(asyncMonitor);
	/*
	 * wait until no signals are being reported
	 */
	while(asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}
	/*
	 * is this handler already registered? 
	 */
	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;

	while (NULL != cursor) {
		if ( (cursor->portLib == portLibrary) && (cursor->handler == handler) && (cursor->handler_arg == handler_arg) ) {
			if (0 == flags) {	/* Remove the listener, but not masterHandlers, which get removed at omrsignal shutdown */
				*previousLink = cursor->next;						/* remove this handler record */
				portLibrary->mem_free_memory(portLibrary, cursor);
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_removed(handler, handler_arg, flags);
			} else {
				/*
				 * update the listener with the new flags 
				 */
				Trc_PRT_signal_omrsig_set_async_signal_handler_user_handler_added_1(handler, handler_arg, flags);
				cursor->flags |= flags;
			}
			break;
		}
		previousLink = &cursor->next;
		cursor = cursor->next;
	}

	if (NULL == cursor) {							/* cursor will only be NULL if we failed to find it in the list */
		if (0 != flags) {
			OMRUnixAsyncHandlerRecord *record = portLibrary->mem_allocate_memory(portLibrary, sizeof(*record),
																				OMR_GET_CALLSITE(),
																				OMRMEM_CATEGORY_PORT_LIBRARY);
			if (NULL == record) {
				rc = OMRPORT_SIG_ERROR;
			} else {
				record->portLib = portLibrary;
				record->handler = handler;
				record->handler_arg = handler_arg;
				record->flags = flags;
				record->next = NULL;
				/*
				 * add the new record to the end of the list
				 */
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
	return OMRPORT_SIG_ERROR;
}

uint32_t
omrsig_map_os_signal_to_portlib_signal(struct OMRPortLibrary *portLibrary, uint32_t osSignalValue)
{
	return 0;
}

int32_t
omrsig_map_portlib_signal_to_os_signal(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag)
{
	return OMRPORT_SIG_ERROR;
}

int32_t
omrsig_register_os_handler(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, void *newOSHandler, void **oldOSHandler)
{
	return OMRPORT_SIG_ERROR;
}

BOOLEAN
omrsig_is_master_signal_handler(struct OMRPortLibrary *portLibrary, void *osHandler)
{
	return FALSE;
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

/*
 * The full shutdown routine "sig_full_shutdown" overrides this once we've completed startup 
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
 * Note: none of the master handlers are registered with the OS until the first call to either
 *		 omrsig_protect or omrsig_set_async_signal_handler.
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
	if (attachedPortLibraries++ == 0) {				/* Restore the old signal actions	*/
		for (index = 0; index < MAX_UNIX_SIGNAL_TYPES ;index++) {
			oldActions[index].restore = 0;
		}
		result = initializeSignalTools(portLibrary);
	}
	omrthread_monitor_exit(globalMonitor);

	/*
	 * z/TPF needs a thread calling tpf_process_signals() every 50 ms or so.
	 * This thread gets launched here.
	 */
	if( J9THREAD_SUCCESS != ztpf_enableSignalHandlers(portLibrary) ) { 
		portLibrary->tty_err_printf( portLibrary, "JVMZTPF01E Cannot enable tpf_process_signals()" );
	}
	/*
	 *	If we have successfully started up the signal portion, 
	 *	then install the full shutdown routine.
	 */
	if(result == 0) {
		portLibrary->sig_shutdown = sig_full_shutdown;
	}
	Trc_PRT_signal_omrsig_startup_exiting(portLibrary, result);
	return result;
}

static uint32_t
countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category) {

	void* value;
	const char* name;
	uint32_t count = 0;

	while (portLibrary->sig_info(portLibrary, info, category, count, &name, &value) != OMRPORT_SIG_VALUE_UNDEFINED) {
		count++;
	}
	return count;
}

/**
 * Reports the asynchronous signal to all listeners.
 */
static int32_t J9THREAD_PROC
asynchSignalReporter(void *userData) {

	OMRUnixAsyncHandlerRecord *cursor;
	uint32_t asyncSignalFlag = 0;
	int result = 0;
	omrthread_set_name(omrthread_self(), "Signal Reporter");

	for(;;) {

		Trc_PRT_signal_omrsig_asynchSignalReporterThread_going_to_sleep();
		/* CMVC  119663 sem_wait can return -1/EINTR on signal in NPTL */
		while (0 != sem_wait(&wakeUpASyncReporter));
		Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up();
		/* we get woken up if there is a signal pending or it is time to shutdown */
		if(0 != shutDownASynchReporter) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_shutdown();
			break;
		}
		/*
		 * determine which signal we've being woken up for 
		 */
		if (0 == sem_trywait(&sigQuitPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGQUIT();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGQUIT;
		} 
		else if (0 == sem_trywait(&sigAbrtPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGABRT();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGABRT;
		} 
		else if (0 == sem_trywait(&sigTermPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGTERM();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGTERM;
		} 
		else if (0 == sem_trywait(&sigXfszPendingSem)) {
			Trc_PRT_signal_omrsig_asynchSignalReporter_woken_up_for_SIGXFSZ();
			asyncSignalFlag = OMRPORT_SIG_FLAG_SIGXFSZ;
		}
		/*
		 * report the signal recorded in signalType to all registered listeners (for this signal)
		 * incrementing the asyncThreadCount will prevent the list from being modified while we use it	
		 */
		omrthread_monitor_enter(asyncMonitor);
		asyncThreadCount++;
		omrthread_monitor_exit(asyncMonitor);

		cursor = asyncHandlerList;
		while (NULL != cursor) {
			if (OMR_ARE_ANY_BITS_SET(cursor->flags, asyncSignalFlag)) {
				Trc_PRT_signal_omrsig_asynchSignalReporter_calling_handler(cursor->portLib,
																		  asyncSignalFlag, cursor->handler_arg); 
				cursor->handler(cursor->portLib, asyncSignalFlag, NULL, cursor->handler_arg);
			}
			cursor = cursor->next;
		}
		omrthread_monitor_enter(asyncMonitor);
		if (--asyncThreadCount == 0) {
			omrthread_monitor_notify_all(asyncMonitor);
		}
		omrthread_monitor_exit(asyncMonitor);
#ifdef OMRPORT_JSIG_SUPPORT
		if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
			int unixSignal = mapPortLibSignalToUnix(asyncSignalFlag);	 /* mapPortLibSignalToUnix returns */
																		 /* -1 on an unknown mapping	   */
			if (-1 != unixSignal) {
				omrsig_handler(unixSignal, NULL, NULL);
			}
		}
#endif
		asyncSignalFlag = 0;								/* reset the signal store */
	}
	
	omrthread_monitor_enter(asyncReporterShutdownMonitor);
	shutDownASynchReporter = 0;
	omrthread_monitor_notify(asyncReporterShutdownMonitor);
	omrthread_exit(asyncReporterShutdownMonitor);

	/* unreachable */
	return 0;
}

/**
 * This signal handler is specific to synchronous signals.
 * It will call all of the user's handlers that were registered with the 
 * vm using omrsig_protect upon receiving a signal one listens for.
 *
 */ 
void
masterSynchSignalHandler(int signal, siginfo_t * sigInfo, void *contextInfo, UDATA breakingEventAddr)
{ 
	omrthread_t thisThread = omrthread_self();
	uint32_t result = U_32_MAX;

	if (NULL != thisThread) {
		uint32_t portLibType;
		struct OMRSignalHandlerRecord *thisRecord;
		struct OMRCurrentSignal currentSignal; 
		struct OMRCurrentSignal *previousSignal;
		
		/* Trc_PRT_signal_masterSynchSignalHandler_entered(signal, sigInfo, contextInfo); */

		thisRecord = NULL;
		
		portLibType = mapUnixSignalToPortLib(signal, sigInfo);
	
		/* record this signal in tls so that jsig_handler can be called if any of the handlers decide we should be shutting down */
		currentSignal.signal = signal;
		currentSignal.sigInfo = sigInfo;
		currentSignal.contextInfo = contextInfo;
		currentSignal.portLibSignalType = portLibType;
#ifndef OMRZTPF
		currentSignal.breakingEventAddr = breakingEventAddr;
#endif
	
		previousSignal =  omrthread_tls_get(thisThread, tlsKeyCurrentSignal);

		omrthread_tls_set(thisThread, tlsKeyCurrentSignal, &currentSignal);
		/*
		 * walk the stack of registered handlers from top to bottom searching for one which handles this type of exception 
		 */
		thisRecord = omrthread_tls_get(thisThread, tlsKey);

		while (NULL != thisRecord) {

			if (OMR_ARE_ANY_BITS_SET(thisRecord->flags,portLibType)) {
				struct OMRUnixSignalInfo signalInfo;
				struct OMRPlatformSignalInfo platformSignalInfo;

				/*
				 * the equivalent of these memsets were here before, but were they needed? 
				 */
				memset(&signalInfo, 0, sizeof(signalInfo));
				memset(&platformSignalInfo, 0, sizeof(platformSignalInfo));

				signalInfo.portLibrarySignalType = portLibType;
				signalInfo.handlerAddress = (void*)thisRecord->handler;
				signalInfo.handlerAddress2 = (void*)masterSynchSignalHandler;
				signalInfo.sigInfo = sigInfo;
				signalInfo.platformSignalInfo = platformSignalInfo;
				/*
				 * Found a suitable handler - what signal type do we want to pass 
				 * on here? port or platform based ?
				 */
				fillInUnixSignalInfo(thisRecord->portLibrary, contextInfo, &signalInfo);
				signalInfo.platformSignalInfo.breakingEventAddr = breakingEventAddr;
				/*
				 * remove the handler we are about to invoke, now, in case the handler crashes
				 */
				omrthread_tls_set(thisThread, tlsKey, thisRecord->previous);
#if 0
				Trc_PRT_signal_masterSynchSignalHandler_calling_handler
				   (signal, sigInfo, contextInfo, thisRecord->handler, thisRecord->portLibrary);
#endif
				result = thisRecord->handler(thisRecord->portLibrary, portLibType, &signalInfo, thisRecord->handler_arg);
#if 0
				Trc_PRT_signal_masterSynchSignalHandler_called_handler
				   (signal, sigInfo, contextInfo, thisRecord->handler, thisRecord->portLibrary, result);
#endif
				/* 
				 * The only case in which we don't want the previous handler back on top is if 
				 * it just returned OMRPORT_SIG_EXCEPTION_RETURN. In this case we will remove it
				 * from the top after executing the siglongjmp 
				 */
				omrthread_tls_set(thisThread, tlsKey, thisRecord);

				if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH) {
					/* continue looping */
				} 
				else if (result == OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION) {
					omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
#if 0
					Trc_PRT_signal_masterSynchSignalHandler_exiting_continuing_execution
					   (signal, sigInfo, contextInfo, thisRecord->handler, thisRecord->portLibrary);
#endif
					return;
				} 
				else /* if (result == OMRPORT_SIG_EXCEPTION_RETURN) */ {
					omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);
#if 0
					Trc_PRT_signal_masterSynchSignalHandler_exiting_siglongjumping
					   (signal, sigInfo, contextInfo, thisRecord->handler, thisRecord->portLibrary);
#endif
					longjmp(thisRecord->mark, 0);
					/* unreachable */
				}
			}
			thisRecord = thisRecord->previous;
		}
		omrthread_tls_set(thisThread, tlsKeyCurrentSignal, previousSignal);

	} /* if (thisThread != NULL) */

	/* The only ways to get here are: (1) if this thread was not attached to the thread library or 
	 * (2) the thread hasn't registered any signal handlers with the port library that could handle the signal
	 */
	if (OMR_ARE_NO_BITS_SET(signalOptionsGlobal, OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN)) {
#if 0
		Trc_PRT_signal_masterSynchSignalHandler_calling_jsig_handler(signal, sigInfo, contextInfo);
#endif
		int rc	= omrsig_handler(signal, (void *)sigInfo, contextInfo);
		if ((OMRSIG_RC_DEFAULT_ACTION_REQUIRED == rc) && (SI_USER != sigInfo->si_code) ) {
#if 0
			Trc_PRT_signal_masterSynchSignalHandler_aborting_after_jsig_handler(signal, sigInfo, contextInfo);
#endif
			abort();
		}

	}
	/*
	 * If we got this far there weren't any handlers on the stack that knew what to with this signal,
	 * so the default action is to abort 
	 */
#if 0
	Trc_PRT_signal_masterSynchSignalHandler_no_handlers_so_aborting(signal, sigInfo, contextInfo);
#endif
	abort();

}


/** 
 * Determines the signal received and notifies the asynch signal reporter
 *
 * One semaphore is used to notify the asynchronous signal reporter that it is time to act.
 * Each expected aynch signal type has an associated semaphore which is used to count the number of "pending" signals.
 *
 */
static void
masterASynchSignalHandler(int signal, siginfo_t * sigInfo, void *contextInfo, UDATA nullArg)
{ 
	uint32_t portLibSignalType;
	portLibSignalType = mapUnixSignalToPortLib(signal, sigInfo);

	/* Todo: default? */
	switch (portLibSignalType) {
	case OMRPORT_SIG_FLAG_SIGQUIT:
		sem_post(&sigQuitPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGABRT:
		sem_post(&sigAbrtPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGTERM:
		sem_post(&sigTermPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGRECONFIG:
		sem_post(&sigReconfigPendingSem);
		break;
	case OMRPORT_SIG_FLAG_SIGXFSZ:
		sem_post(&sigXfszPendingSem);
		break;
	}
#if 0
	Trc_PRT_signal_omrsignal_AsynchSignalHandler_Entry(signal, sigInfo, contextInfo);
#endif
	sem_post(&wakeUpASyncReporter);
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
	int	unixSignalNo = mapPortLibSignalToUnix(portLibrarySignalNo);

	if( unixSignalNo == -1 )	/* if the signal is undefined to TPF,	*/
		return 0;				/*	 skip the rest of this function.	*/
	/*
	 * do not block any signals
	 */
	if (0 != sigemptyset(&newAction.sa_mask)) {
		return -1;
	}
	/*
	 * Automatically restart system calls that get interrupted by any signal
	 * Neutrino V6.3 does not support this 
	 */
	newAction.sa_flags = SA_RESTART;
	/*
	 * Setting to SA_SIGINFO will result in "void (*sa_sigaction) (int, siginfo_t *, void *)" 
	 * to be used, and not "__sighandler_t sa_handler". Both are members of struct sigaction,
	 * using the former allows us to access much more information than just the signal number 
	 */
	newAction.sa_flags |= SA_SIGINFO;

	/* 
	 * SA_NODEFER prevents the current signal from being masked by default in the handler. 
	 * However, it can still be masked if one explicitly requests so in sa_mask field, 
	 * as is done on z/OS 
	 */
	newAction.sa_flags |= SA_NODEFER;

	/*
	 * The master exception handler:
	 *		The (void *) casting only applies to zLinux because a new parameter "UDATA breakingEventAddr" 
	 *		has been introduced in masterSynchSignalHandler() to obtain BEA on zLinux but not for other platforms.
	 *		As a result, the total number of parameters in masterSynchSignalHandler() on zLinux & z/TPF
	 *		is 4 while the number is 3 on other platforms, because there is no need to change the signature of 
	 *		masterSynchSignalHandler in there. Since the code is shared on all platforms, the change here is used 
	 *		to split them up to avoid any compiling error.
	 */
	newAction.sa_sigaction = (void *)handler;

	/* now that we've set up the sigaction struct the way we want it, register the handler with the OS */
	if (OMRSIG_SIGACTION(unixSignalNo, &newAction, &oldActions[unixSignalNo].action)) {
		Trc_PRT_signal_registerSignalHandlerWithOS_failed_to_registerHandler(portLibrarySignalNo, unixSignalNo, handler);
		return -1;
	} 
	else {
		Trc_PRT_signal_registerSignalHandlerWithOS_registeredHandler(portLibrarySignalNo, unixSignalNo, handler);	
		oldActions[unixSignalNo].restore = 1;
	}

	/* CMVC 96193 signalsWithMasterHandlers is checked without acquiring the masterHandlerMonitor */ 
	issueWriteBarrier();
	/*
	 * we've successfully registered the master handler for this, record it!
	 */
	signalsWithMasterHandlers |= portLibrarySignalNo;

	return 0;
}

 /**
 * \fn	The unix signal number is converted to the corresponding port library 
 *		signal number.
 *
 *	We do this by going in and matching the Program Interrupt Code, which we
 *	have if and only if we have an associated Java Dump Buffer. If we don't
 *	have one, then we can't map it, so just return zero.
 *
 *	There are multiple functions which call this routine. Test to see if the
 *	siginfo_t passed by reference is already built. If so, do not rebuild it.
 *
 *	Some signals have subtypes which are detailed in the siginfo_t structure.
 *	That's fine for other platforms, but not too likely on z/TPF; and even if
 *	so, probably doesn't distinguish between floating point, integer, and 
 *	packed decimal datatype errors (for example). So go ahead and do it 
 *	ourselves -- that way, we're sure.
 *
 *	The only signal value we do this for is SIGFPE; all the others just return
 *	what they're passed.
 *
 *	\param[in]	signalNo	The signal number as passed back to the signal handler.
 *	\param[in]	sigInfo		The <tt>siginfo_t</tt> structure as passed back to the
 *							upper-layer signal handler.
 *
 *	\note	Make sure to test the sigInfo pointer, treat it as non-extant if zero.
 *			Go ahead and write on it if it's there. If we have one, we'll store
 *			the signal value and the translated PIC into it.
 */
uint32_t
mapUnixSignalToPortLib(uint32_t signalNo, siginfo_t *sigInfo)
{
	uint32_t index;
	struct ijdib *dibPtr = (struct ijdib *)(ecbp2()->ce2dib);
	struct ijavdbf *dbfPtr = (struct ijavdbf *)(dibPtr->dibjdb);

	for (index = 0 ; index < sizeof(signalMap)/sizeof(signalMap[0]); index++) {
		if (signalMap[index].unixSignalNo == signalNo) {									
			if( !sigInfo )							/* If we don't have a siginfo_t pointer,*/
				return signalNo;					/*	then just return what we got passed */

			/*	Otherwise, do translation from the JDB if there's one of those as long		*/
			/*	as we have a DIB pointer belonging to this thread from which to build it.	*/
			/*	If these conditions aren't met, then just return it to the caller verbatim. */

			if( (0 != sigInfo->si_signo || 0 != sigInfo->si_code) && dbfPtr ) {
				ztpfDeriveSiginfo( sigInfo );		/* Build a new siginfo_t				*/
				signalNo = sigInfo->si_signo;		/* Set return value.					*/
			}
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
				return OMRPORT_SIG_FLAG_SIGFPE;
				}
			}
			return signalMap[index].portLibSignalNo;
		}
	}
	return 0;
}

/**
 * \fn The defined port library signal is converted to the corresponding unix 
 * signal number.
 *
 * Note that FPE signal codes (subtypes) all map to the same signal number and are not included 
 * 
 * \param[in] portLibSignal The internal port library signal number
 * \return The corresponding Unix signal number or -1 if the portLibSignal could not be mapped
 */
static int
mapPortLibSignalToUnix(uint32_t portLibSignal)
{
	uint32_t index;

	/* mask out subtypes */
	portLibSignal &= OMRPORT_SIG_FLAG_SIGALLSYNC | OMRPORT_SIG_FLAG_SIGQUIT | OMRPORT_SIG_FLAG_SIGABRT | OMRPORT_SIG_FLAG_SIGTERM | OMRPORT_SIG_FLAG_SIGXFSZ;
	for (index = 0; index < sizeof(signalMap)/sizeof(signalMap[0]); index++) {

		if (signalMap[index].portLibSignalNo == portLibSignal) {
			return signalMap[index].unixSignalNo;
		}
	}
	return -1;
}


/**
 * \fn Registers the master handler for the signals in flags that don't have one.
 * 
 * Calls to this function must be synchronized using masterHandlerMonitor.
 *
 * @param[in] portLibrary			Address of this JVM's OMRPortLibrary
 * @param[in] flags					The flags that we want signals for
 * @param[in] allowedSubsetOfFlags	Must be either OMRPORT_SIG_FLAG_SIGALLSYNC or OMRPORT_SIG_FLAG_SIGALLASYNC
 * 
 * @return	0 upon success; OMRPORT_SIG_ERROR otherwise.
 *			Possible failure scenarios include attempting to register a handler for
 *			a signal that is not included in the allowedSubsetOfFlags
*/
static int32_t
registerMasterHandlers(OMRPortLibrary *portLibrary, uint32_t flags, uint32_t allowedSubsetOfFlags)
{
	uint32_t flagsSignalsOnly = 0;
	uint32_t flagsWithoutHandlers = 0;
	unix_sigaction handler;

	if (OMRPORT_SIG_FLAG_SIGALLSYNC == allowedSubsetOfFlags) {
		handler = masterSynchSignalHandler;
	} 
	else if (OMRPORT_SIG_FLAG_SIGALLASYNC == allowedSubsetOfFlags) {
		handler = masterASynchSignalHandler;
	} 
	else {
		return OMRPORT_SIG_ERROR;
	}
	flagsSignalsOnly = flags & allowedSubsetOfFlags;
	flagsWithoutHandlers = flagsSignalsOnly & (~signalsWithMasterHandlers);

	if (0 != flagsWithoutHandlers) {		/* we be registerin' some handlers */
		uint32_t portSignalType = 0;
		/*
		 * portSignalType starts off at 0 as it is the smallest synch signal. 
		 * In the case that we are registering an asynch signal, flagsWithoutHandlers has already
		 * masked off the synchronous signals (eg. "0") so we are not at risk of registering a 
		 * handler for an incorrect signal	
		*/
		for (portSignalType = 4; portSignalType < allowedSubsetOfFlags; portSignalType = portSignalType << 1) {
			/*
			 * Iterate through all the signals and register the master handler
			 * for those that don't have one yet
			 */
			if (OMR_ARE_ANY_BITS_SET(flagsWithoutHandlers, portSignalType)) {
				/*
				 * we need a master handler for this (portSignalType's) signal
				 */
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

	/* use this to record the end of the list of signal infos */
	if(omrthread_tls_alloc(&tlsKey))		{
		return -1;
	}

	/* use this to record the last signal that occured such that we can call omrsig_handler in omrexit_shutdown_and_exit */
	if(omrthread_tls_alloc(&tlsKeyCurrentSignal)) {
		return -1;
	}

	if(omrthread_monitor_init_with_name( &masterHandlerMonitor, 0, "portLibrary_omrsig_masterHandler_monitor" ) ) {
		return -1;
	}

	if(omrthread_monitor_init_with_name( &asyncReporterShutdownMonitor, 0, "portLibrary_omrsig_asynch_reporter_shutdown_monitor" ) ) {
		return -1;
	}

	if(omrthread_monitor_init_with_name( &asyncMonitor, 0, "portLibrary_omrsig_async_monitor" ) ) {
		return -1;
	}

	/* The asynchronous signal reporter will wait on this semaphore  */
	if(0 != sem_init(&wakeUpASyncReporter, 0, 0))	 {
		return -1;
	}

	/* The asynchronous signal reporter keeps track of the number of pending singals with these... */  
	if(0 != sem_init(&sigQuitPendingSem, 0, 0))		{
		return -1;
	}
	if(0 != sem_init(&sigAbrtPendingSem, 0, 0))		{
		return -1;
	}
	if(0 != sem_init(&sigTermPendingSem, 0, 0))		{
		return -1;
	}
	if(0 != sem_init(&sigReconfigPendingSem, 0, 0))	{
		return -1;
	}
	if(0 != sem_init(&sigXfszPendingSem, 0, 0))		{
		return -1;
	}
#if defined(OMR_PORT_ASYNC_HANDLER)
	if(J9THREAD_SUCCESS != createThreadWithCategory(&asynchSignalReporterThread, 256 * 1024, J9THREAD_PRIORITY_MAX, 0, &asynchSignalReporter, NULL, J9THREAD_CATEGORY_SYSTEM_THREAD)) {
		return -1;
	}
#endif
	return 0;
}

static int32_t
setReporterPriority(OMRPortLibrary *portLibrary, UDATA priority)
{
	
	Trc_PRT_signal_setReporterPriority(portLibrary, priority);
	
	if(asynchSignalReporterThread == NULL) {
		return -1;
	}
	
	return omrthread_set_priority(asynchSignalReporterThread, priority);
}

/**
 * sets the priority of the the async reporting thread
*/
int32_t
omrsig_set_reporter_priority(struct OMRPortLibrary *portLibrary, UDATA priority)
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
	omrthread_monitor_destroy( asyncMonitor);
	omrthread_monitor_destroy(ztpfSigShutdownMonitor);
	sem_destroy(&wakeUpASyncReporter);
	sem_destroy(&sigQuitPendingSem);
	sem_destroy(&sigAbrtPendingSem);
	sem_destroy(&sigTermPendingSem);
	sem_destroy(&sigReconfigPendingSem);
	sem_destroy(&sigXfszPendingSem);
	return 0;
}

int32_t
omrsig_set_options(struct OMRPortLibrary *portLibrary, uint32_t options)
{
	Trc_PRT_signal_omrsig_set_options(options);
	
	if ( (OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS | OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)  & options)  {
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
	OMRCurrentSignal * currentSignal = omrthread_tls_get(omrthread_self(), tlsKeyCurrentSignal);
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

	globalMonitor =omrthread_global_monitor();
	omrthread_monitor_enter(globalMonitor);
	if (--attachedPortLibraries == 0) {		/* register the old actions we overwrote with our own */
		for ( index = 0; index < MAX_UNIX_SIGNAL_TYPES; index++) {
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
		shutDownASynchReporter = 1;
		sem_post(&wakeUpASyncReporter);
		while (shutDownASynchReporter) {
			omrthread_monitor_wait(asyncReporterShutdownMonitor);
		}
		omrthread_monitor_exit(asyncReporterShutdownMonitor);
#endif	/* defined(OMR_PORT_ASYNC_HANDLER) */
		omrthread_monitor_enter(ztpfSigShutdownMonitor);
		shutdown_ztpf_sigpush = 1;
		while (shutdown_ztpf_sigpush) {
			omrthread_monitor_wait(ztpfSigShutdownMonitor);
		}
		omrthread_monitor_exit(ztpfSigShutdownMonitor);
		/* destroy all of the remaining monitors */
		destroySignalTools(portLibrary);
	}
	omrthread_monitor_exit(globalMonitor);
	Trc_PRT_signal_sig_full_shutdown_exiting(portLibrary);
}

static void
removeAsyncHandlers(OMRPortLibrary* portLibrary)
{
	/* clean up the list of async handlers */
	OMRUnixAsyncHandlerRecord *cursor;
	OMRUnixAsyncHandlerRecord **previousLink;

	omrthread_monitor_enter(asyncMonitor);

	/* wait until no signals are being reported */
	while(asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;
	while (cursor) {
		if ( cursor->portLib == portLibrary ) {
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

#if defined(OMRPORT_JSIG_SUPPORT)

/*	@internal 
 *
 * omrexit_shutdown_and_exit needs to call this to ensure the signal is chained to omrsig (the application
 * handler) in the case when the shutdown is due to a fatal signal.
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
#endif /* OMRPORT_JSIG_SUPPORT */
