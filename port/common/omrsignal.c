/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
 * @note If the exception is resumed using J9SIG_EXCEPTION_CONTINUE_EXECUTION, the modified values will be used
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
 * @return 0 on success
 */
uint32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary,  omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{
	return 1;
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

