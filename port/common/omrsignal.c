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
 * @file
 * @ingroup Port
 * @brief Signal handling
 */
#include "omrport.h"



/**
 * Provides the name and value, specified by category/index of the gp information in info.
 * Returns the kind of information found at category/index specified, or undefined
 * if the category/index are invalid.  The number of items in the category specified must
 * equal the count @ref omrsig_info_count returns for that category.
 *
 * @param[in] portLibrary The port library
 * @param[in] info struct containing all available signal information. Normally includes register values, name of module where crash occured and its base address.
 * @param[in] category the category of signal information that you are querying. Valid categories are:
 * \arg OMRPORT_SIG_SIGNAL (0) -- information about the signal
 * \arg OMRPORT_SIG_GPR (1) -- general purpose registers
 * \arg OMRPORT_SIG_OTHER (2) -- other information
 * \arg OMRPORT_SIG_CONTROL (3) -- control registers
 * \arg OMRPORT_SIG_FPR (4) -- floating point registers
 * \arg OMRPORT_SIG_MODULE (5) -- module information
 * @param[in] index the index of the item in the specified category. Use @ref omrsig_info_count to determine how many indices in each category
 * \arg OMRPORT_SIG_SIGNAL_TYPE -- when category = OMRPORT_SIG_SIGNAL, returns the signal type. Always defined.
 * \arg OMRPORT_SIG_SIGNAL_HANDLER -- when category = OMRPORT_SIG_SIGNAL, returns the address of the handler. Always defined.
 * \arg OMRPORT_SIG_SIGNAL_ADDRESS -- when category = OMRPORT_SIG_SIGNAL, returns the address where the fault occurred, if available
 * \arg OMRPORT_SIG_CONTROL_PC -- when category = OMRPORT_SIG_CONTROL, returns the PC value when the fault occurred, if available
 * \arg OMRPORT_SIG_MODULE_NAME -- when category = OMRPORT_SIG_MODULE, returns the name of the module where the fault occurred, if available
 * @param[out] name the name of the item at the specified index.
 * @param[out] value the value of the item at the specified index
 *
 * @return the kind of info at the specified index. For example, this allows the caller to determine whether the item placed in **value corresponds to a 32/64-bit integer or a pointer to a string.
 * \arg OMRPORT_SIG_VALUE_UNDEFINED -- value is unknown
 * \arg OMRPORT_SIG_VALUE_STRING -- value is a (const char*) and points to a NUL terminated string
 * \arg OMRPORT_SIG_VALUE_ADDRESS -- value is a (void**) and points to another pointer
 * \arg OMRPORT_SIG_VALUE_32 -- value is a (uint32_t*) and points to an integer value
 * \arg OMRPORT_SIG_VALUE_64 -- value is a (uint64_t*) and points to an integer value
 * \arg OMRPORT_SIG_VALUE_FLOAT_64 -- value is a (double*) and points to a floating point value. No assumptions should be made about the validity of the floating point number.
 *
 * @note The caller may modify the value returned in value for some entries. Only entries in the OMRPORT_SIG_GPR, OMRPORT_SIG_CONTROL or OMRPORT_SIG_FPR categories may be modified.
 * @note If the exception is resumed using OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION, the modified values will be used
 */
uint32_t
omrsig_info(struct OMRPortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value)
{
	*name = "";
	return OMRPORT_SIG_VALUE_UNDEFINED;
}
/**
 * Returns the number of items that exist in the category specified, or zero if the category is undefined.
 *
 * @param[in] portLibrary The port library
 * @param[in] info struct containing all available signal information. Normally includes register values, name of module where crash occured and its base address.
 * @param[in] category the category in which we want to find the number of items that exist.
 *
 * @note Return value must agree with the number of items that @ref omrsig_info makes available for the category specified.
*/
uint32_t
omrsig_info_count(struct OMRPortLibrary *portLibrary, void *info, uint32_t category)
{
	return 0;
}

/**
 * Execute the function 'fn' protecting it from synchronous signals. The particular signals which are handled are platform specific.
 * If a synchronous signal occurs while fn is executing, function 'handler' will be invoked.
 * The handler must return one of:
 *      OMRPORT_SIG_EXCEPTION_CONTINUE_SEARCH, indicating that the next exception handler in the chain should be invoked,
 *      OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION, indicating that execution should be resumed using the information stored in the info structure
 *      OMRPORT_SIG_EXCEPTION_RETURN, indicating that execution should continue as if the protected function had returned 0.
 *
 * @param[in] portLibrary The port library
 * @param[in] fn the function to protect from signals
 * @param[in] arg argument to fn
 * @param[in] handler the function to handle signals
 * @param[in] handler_arg extra argument to handler
 * @param[in] flags flags to control the signal handlers. Construct from bitwise-orring any of the following constants:
 *	OMRPORT_SIG_FLAG_MAY_RETURN - indicates that the handler might return OMRPORT_SIG_EXCEPTION_RETURN
 *	OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION - indicates that the handler might return OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION.
 *	Only the first handler may return OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION. Behaviour is undefined if a handler returns
 * 	OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION after another handler has been invoked.
 *
 *		NOTE: @ref flags can not include both OMRPORT_SIG_FLAG_MAY_RETURN and OMRPORT_SIG_FLAG_CONTINUE_EXECUTION.
 *
 *	OMRPORT_SIG_FLAG_SIGSEGV - catch SIGSEGV
 *	OMRPORT_SIG_FLAG_SIGBUS - catch SIGBUS
 *	OMRPORT_SIG_FLAG_SIGILL - catch SIGILL
 *	OMRPORT_SIG_FLAG_SIGFPE - catch SIGFPE
 *	OMRPORT_SIG_FLAG_SIGTRAP - catch SIGTRAP
 * OMRPORT_SIG_FLAG_SIGALLSYNC - catch any of the above signals
 * In addition, the following bits may be set if OMRPORT_SIG_FLAG_SIGFPE is set. These provide additional information about the cause of the signal:
 * OMRPORT_SIG_FLAG_SIGFPE_DIV_BY_ZERO - a floating point division by zero
 * OMRPORT_SIG_FLAG_SIGFPE_INT_DIV_BY_ZERO - an integer division by zero
 * OMRPORT_SIG_FLAG_SIGFPE_INT_OVERFLOW - an integer operation caused an overflow
 *
 * @param[out] result the return value of the function of fn, if it completed successfully
 *
 * @return
 * \arg 0 if fn completed successfully,
 * \arg OMRPORT_SIG_EXCEPTION_OCCURRED, if an exception occurred and the handler returned OMRPORT_SIG_EXCEPTION_RETURN
 * \arg OMRPORT_SIG_ERROR, if an error occurred before fn could be executed
 */
int32_t
omrsig_protect(struct OMRPortLibrary *portLibrary,  omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result)
{
	*result = fn(portLibrary, fn_arg);
	return 0;
}

/**
 * Synchronous signals (i.e. those caused by program execution) must be handled via structured exception handling (omrsig_protect).
 * Asynchronous signals, such as SIGQUIT, are not suitable for structured exception handling, since they don't represent an error condition, but instead
 * indicate that some external event has occurred. They are not associated with a specific thread or with a  section of code.
 *
 * When an asynchronous signal arrives, a new thread is created and the handler function is invoked on that thread.
 * If multiple handlers are registered, they will all be invoked on the same thread in an arbitrary order.
 *
 * If asynchronous signals are currently being reported this function will block until all threads reporting an asynchronous signal
 * have completed. This has two consequences. Firstly, omrsig_set_async_signal_handler is a blocking operation. Do not call it
 * while you hold a synchronization resource which might be required by a handler. Secondly, you may safely assume that your
 * signal handler will never be invoked again once you have removed it. There is no possibility that the handler is currently running,
 * or that it is just about to run. Once a handler is removed (by using flags=0) any resources it relies on may safely be freed.
 *
 * @param[in] portLibrary The port library
 * @param[in] handler the function to call if an async signal arrives
 * @param[in] handler_arg the argument to handler
 * @param[in] flags used to indicate which asynchronous signals are handled:
 * 	OMRPORT_SIG_FLAG_SIGQUIT - (triggered when program state should be dumped - Ctrl+/ on Unix, Ctrl+Break on Windows)
 * 	OMRPORT_SIG_FLAG_SIGABRT - (triggered when the the program should terminate and produce a core file)
 * 	OMRPORT_SIG_FLAG_SIGTERM - (triggered when the program should terminate gracefully - Ctrl+C on Unix and Windows)
 * OMRPORT_SIG_FLAG_SIGRECONFIG - (triggered when DLPAR information has changed and the program should dynamically reconfigure itself)
 * OMRPORT_SIG_FLAG_SIGXFSZ - (triggered when program exceeds the allowed ulimit file size)
 * OMRPORT_SIG_FLAG_SIGALLASYNC - any of the above asynchronous signals
 *
 * @note If a handler with the specified function pointer and argument is already registered, calling this function again will change its flags.
 * @note To remove a handler, use 0 for the flags argument.
 *
 * @note A handler should not rely on the handling thread being an attached omrthread. It may be a raw operating system thread.
 * @note A handler should not do anything to cause the reporting thread to terminate (e.g. call omrthread_exit()) as that may prevent
 *  future signals from being reported.
 *
 * @return 0 on success or non-zero on failure
 */
int32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{
	return OMRPORT_SIG_ERROR;
}


/**
 * @brief Similar to omrsig_set_async_signal_handler. Refer to omrsig_set_async_signal_handler's description above.
 * A new element is added to the asyncHandlerList for omrsig_handler_fn, and masterASynchSignalHandler is registered
 * with the OS for the signal corresponding to the specified portlibSignalFlag. masterASynchSignalHandler invokes
 * asyncHandlerList elements when a relevant signal is raised. If portlibSignalFlag is 0, then the asyncHandlerList
 * entry corresponding to omrsig_handler_fn is removed, and related resources are freed. portlibSignalFlag can only
 * have one signal flag set; otherwise, OMRPORT_SIG_ERROR is returned. One omrsig_handler_fn handler is registered
 * with a signal at any time instead of multiple handlers. When associating a new omrsig_handler_fn with a signal,
 * prior omrsig_handler_fn(s) are dissociated from the signal. This function supports all signals listed in
 * OMRPORT_SIG_FLAG_SIGALLASYNC.
 *
 * The address of the old signal handler function is stored in *oldOSHandler. In case of error or if NULL is provided
 * for oldOSHandler, then *oldOSHandler is not updated to reflect the old signal handler function.
 *
 * @param[in] portLibrary The port library
 * @param[in] handler the function to call if an asynchronous signal arrives
 * @param[in] handler_arg the argument to handler
 * @param[in] portlibSignalFlag a single port library signal flag, or 0 to remove the omrsig_handler_fn entry
 * @param[out] oldOSHandler points to the old signal handler function
 *
 * @return 0 on success or non-zero on failure
 */
int32_t
omrsig_set_single_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t portlibSignalFlag, void **oldOSHandler)
{
	return OMRPORT_SIG_ERROR;
}

/**
 * @brief Given an OS signal value, return the corresponding port library signal flag.
 *
 * @param[in] portLibrary The port library
 * @param[in] osSignalValue OS signal value
 *
 * @return port library signal flag on success and 0 on failure
 */
uint32_t
omrsig_map_os_signal_to_portlib_signal(struct OMRPortLibrary *portLibrary, uint32_t osSignalValue)
{
	return 0;
}

/**
 * @brief Given a port library signal flag, return the corresponding OS signal value.
 *
 * @param[in] portLibrary The port library
 * @param[in] portlibSignalFlag port library signal flag
 *
 * @return OS signal value on success and OMRPORT_SIG_ERROR (-1) on failure
 */
int32_t
omrsig_map_portlib_signal_to_os_signal(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag)
{
	return OMRPORT_SIG_ERROR;
}

/**
 * @brief Register a handler with the OS. For an invalid portlibSignalFlag, an error is returned.
 * portlibSignalFlag is invalid: if it is zero or if multiple signal bits are specified or if the
 * specified flag is not supported. If OS fails to register newOSHandler for the specified signal,
 * then an error is returned.
 *
 * The address of the old signal handler function is stored in *oldOSHandler. In case of error or
 * if NULL is provided for oldOSHandler, then *oldOSHandler is not updated to reflect the old
 * signal handler function.
 *
 * This function may override a master handler which was previously set by omrsig_protect or
 * omrsig_set_*_async_signal_handler variant. The records associated with the master handler
 * for the portlibSignalFlag signal are left unchanged when this function overrides the master
 * handler. When the master handler is re-registered with the portlibSignalFlag signal, then
 * the records associated with master handler don't need to be restored. An example of records
 * is OMR*AsyncHandlerRecord(s) in asyncHandlerList.
 *
 * Each platform variant of omrsignal.c should have a signalMap. signalMap should have a list of
 * signals, which are supported on a platform. This function supports all signals listed in
 * omrsignal.c::signalMap.
 *
 * On Unix variants, sigaction is used to register the handler. SA_SIGINFO flag is set.
 * So, the signature of the signal handling function should be void (*)(int, siginfo_t *, void *).
 * On zLinux, the signature changes to void (*)(int, siginfo_t *, void *, uintptr_t).
 *
 * sigaction isn't available on Windows. On Windows, signal is used to register the handler.
 * The signature of the signal handling function should be void (*)(int).
 *
 * On Unix variants, original OS handler is cached in oldActions while registering the first handler
 * with the OS. The original OS handler is restored during shutdown. If OMRSIG_SIGACTION is called outside
 * OMR, then the original OS handler can get overwritten without being cached in oldActions. Subsequently,
 * the oldActions can cache a user handler instead of the original OS handler. During shutdown, the
 * original OS handler can be restored incorrectly. omrsig_register_os_handler lets a user register a
 * handler with the OS while properly caching the original OS handler. The original OS handler is properly
 * restored during shutdown.
 *
 * @param[in] portLibrary The port library
 * @param[in] portlibSignalFlag a single port library signal flag
 * @param[in] newOSHandler the new signal handler function to register
 * @param[out] oldOSHandler points to the old signal handler function
 *
 * @return 0 on success or non-zero on failure
 */
int32_t
omrsig_register_os_handler(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, void *newOSHandler, void **oldOSHandler)
{
	return OMRPORT_SIG_ERROR;
}

/**
 * Determine if the osHandler is a predefined master signal handler. masterASynchSignalHandler is used for
 * asynchronous signals and masterSynchSignalHandler is used for synchronous signals.
 *
 * @param[in] portLibrary The port library
 * @param[in] osHandler A signal handler function
 *
 * @return TRUE if osHandler is a predefined master handler. Otherwise, return FALSE.
 */
BOOLEAN
omrsig_is_master_signal_handler(struct OMRPortLibrary *portLibrary, void *osHandler)
{
	return FALSE;
}

/**
 * Determine if a signal is ignored by the OS.
 *
 * @param[in] portLibrary the port library
 * @param[in] portlibSignalFlag a single port library signal flag
 * @param[out] *isSignalIgnored will contain either TRUE if the signal
 *             is ignored or FALSE if the signal is not ignored
 *
 * @return 0 on success and OMRPORT_SIG_ERROR (-1) on failure
 */
int32_t
omrsig_is_signal_ignored(struct OMRPortLibrary *portLibrary, uint32_t portlibSignalFlag, BOOLEAN *isSignalIgnored)
{
	int32_t rc = 0;
	Trc_PRT_signal_omrsig_is_signal_ignored_entered(portlibSignalFlag);

	*isSignalIgnored = FALSE;

	Trc_PRT_signal_omrsig_is_signal_ignored_exiting(rc, *isSignalIgnored);
	return rc;
}


/**
 * Determine if the port library is capable of protecting a function from the indicated signals in the indicated way.
 *
 * @param[in] portLibrary The port library
 * @param[in] flags flags to control the signal handlers. Construct from bitwise-orring any of the following constants:
 *	OMRPORT_SIG_FLAG_MAY_RETURN - indicates that the handler might return OMRPORT_SIG_EXCEPTION_RETURN, so a jmpbuf must be maintained in case that happens
 *	OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION - indicates that the handler might return OMRPORT_SIG_EXCEPTION_CONTINUE_EXECUTION

 *	Synchronous Signals: If the signal option OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS has been set we cannot protect against any synchronous signals.
 *		OMRPORT_SIG_FLAG_SIGSEGV - catch SIGSEGV
 *		OMRPORT_SIG_FLAG_SIGBUS - catch SIGBUS
 *		OMRPORT_SIG_FLAG_SIGILL - catch SIGILL
 *		OMRPORT_SIG_FLAG_SIGFPE - catch SIGFPE
 *		OMRPORT_SIG_FLAG_SIGTRAP - catch SIGTRAP
 * 		OMRPORT_SIG_FLAG_SIGALLSYNC - catch any of the above synchronous signals
 *
 *  Asynchronous Signals: If the signal option OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS has been set we cannot protect against any asynchronous signals.
 *  	OMRPORT_SIG_FLAG_SIGQUIT - catch SIGQUIT
 * 		OMRPORT_SIG_FLAG_SIGABRT - catch SIGABRT
 *		OMRPORT_SIG_FLAG_SIGTERM - catch SIGTERM
 *		OMRPORT_SIG_FLAG_SIGRECONFIG - catch SIGRECONFIG
 *		OMRPORT_SIG_FLAG_SIGINT - catch SIGINT
 *		OMRPORT_SIG_FLAG_SIGXFSZ - catch SIGXFSZ
 *		OMRPORT_SIG_FLAG_SIGALLASYNC - catch any of the above asynchronous signals
 *
 * @return non-zero if the portlibrary can support the specified flags.
 */
int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags)
{
	/* in the stub implementation, no signals are supported */
	if (flags & OMRPORT_SIG_FLAG_SIGALLSYNC) {
		return 0;
	} else {
		return 1;
	}
}

/**
 * sets the priority of the the async reporting thread
 *
 * the default behaviour is to do nothing
*/
int32_t
omrsig_set_reporter_priority(struct OMRPortLibrary *portLibrary, uintptr_t priority)
{
	return 0;
}

/**
 * Shutdown the signal handling component of the port library
 */
void
omrsig_shutdown(struct OMRPortLibrary *portLibrary)
{
}
/**
 * Start up the signal handling component of the port library
 */
int32_t
omrsig_startup(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Set signal options for all port libraries.
 *
 * @param[in] portLibrary The port library
 * @param[in] options set these options
 * 	OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS
 * 	OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS
 *	OMRPORT_SIG_OPTIONS_OMRSIG_NO_CHAIN
 *
 * @return 0 on success, non-zero otherwise
 * @note attempting to set 	OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS or OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS
 * 	after the vm has installed a handler will result in failure
 */
int32_t
omrsig_set_options(struct OMRPortLibrary *portLibrary, uint32_t options)
{
	/* options are not set in the default implementation */
	return 1;
}
/**
 * Get the port library's signal options.
 * These options apply to all port libraries.
 *
 * @return the value of the port library options
 */
uint32_t
omrsig_get_options(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

/**
 * Used to see if this thread is in a signal handler. If so, the caller should avoid functions
 * that are not safe to use in an asynchronous manner, such as omrmem_allocate_memory and omrstr_printf.
 *
 * @param[in] portLibrary the port library
 *
 * @return 0 if not in a signal handler, negative on failure, positive if in a signal handler. When positive
 *         the return value indicates the most recently set signal; see OMRPORT_SIG_FLAG_SIG* constants for
 *         possible values, excluding OMRPORT_SIG_FLAG_SIGALLSYNC and OMRPORT_SIG_FLAG_SIGALLASYNC.
 */
intptr_t
omrsig_get_current_signal(struct OMRPortLibrary *portLibrary)
{
	return 0;
}

