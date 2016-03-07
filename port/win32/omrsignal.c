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

#include <windows.h>
#include <stdlib.h>
#include "omrport.h"
#include "omrthread.h"
#include "omrsignal.h"

typedef struct J9Win32AsyncHandlerRecord {
	OMRPortLibrary *portLib;
	omrsig_handler_fn handler;
	void *handler_arg;
	uint32_t flags;
	struct J9Win32AsyncHandlerRecord *next;
} J9Win32AsyncHandlerRecord;

static J9Win32AsyncHandlerRecord *asyncHandlerList;
static omrthread_monitor_t asyncMonitor;
static uint32_t asyncThreadCount;
static uint32_t attachedPortLibraries;
/* holds the options set by omrsig_set_options */
static uint32_t signalOptions;

/* key to get the current synchronous signal */
static omrthread_tls_key_t tlsKeyCurrentSignal;

static uint32_t mapWin32ExceptionToPortlibType(uint32_t exceptionCode);
static uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static void removeAsyncHandlers(OMRPortLibrary *portLibrary);
static void fillInWin32SignalInfo(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, EXCEPTION_POINTERS *exceptionInfo, struct J9Win32SignalInfo *j9info);
static uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static uint32_t countInfoInCategory(struct OMRPortLibrary *portLibrary, void *info, uint32_t category);
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType);
int structuredExceptionHandler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, EXCEPTION_POINTERS *exceptionInfo);
static uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value);
static intptr_t setCurrentSignal(struct OMRPortLibrary *portLibrary, intptr_t signal);



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

uint32_t
omrsig_set_async_signal_handler(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, void *handler_arg, uint32_t flags)
{

	uint32_t rc = 0;
	J9Win32AsyncHandlerRecord *cursor;
	J9Win32AsyncHandlerRecord **previousLink;

	if (OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS & signalOptions) {
		/* -Xrs was set, do not install any handlers */
		return 1;
	}

	omrthread_monitor_enter(asyncMonitor);

	/* wait until no signals are being reported */
	while (asyncThreadCount > 0) {
		omrthread_monitor_wait(asyncMonitor);
	}

	/* is this handler already registered? */
	previousLink = &asyncHandlerList;
	cursor = asyncHandlerList;
	while (cursor) {
		if ((cursor->portLib == portLibrary) && (cursor->handler == handler) && (cursor->handler_arg == handler_arg)) {
			if (flags == 0) {
				*previousLink = cursor->next;
				portLibrary->mem_free_memory(portLibrary, cursor);

				/* if this is the last handler, unregister the Win32 handler function */
				if (asyncHandlerList == NULL) {
					SetConsoleCtrlHandler(consoleCtrlHandler, FALSE);
				}
			} else {
				cursor->flags = flags;
			}
			break;
		}
		previousLink = &cursor->next;
		cursor = cursor->next;
	}

	if (cursor == NULL) {
		/* cursor will only be NULL if we failed to find it in the list */
		if (flags != 0) {
			J9Win32AsyncHandlerRecord *record = portLibrary->mem_allocate_memory(portLibrary, sizeof(*record), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);

			if (record == NULL) {
				rc = 1;
			} else {
				record->portLib = portLibrary;
				record->handler = handler;
				record->handler_arg = handler_arg;
				record->flags = flags;
				record->next = NULL;

				/* if this is the first handler, register the Win32 handler function */
				if (asyncHandlerList == NULL) {
					SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);
				}

				/* add the new record to the end of the list */
				*previousLink = record;
			}
		}
	}

	omrthread_monitor_exit(asyncMonitor);

	return rc;

}

int32_t
omrsig_can_protect(struct OMRPortLibrary *portLibrary,  uint32_t flags)
{
	uint32_t supportedFlags = OMRPORT_SIG_FLAG_MAY_RETURN | OMRPORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;

	if (0 == (signalOptions & OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_SYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGALLSYNC;
	}

	/* split this up into OMRPORT_SIG_FLAG_SIGALLSYNC and OMRPORT_SIG_FLAG_SIGALLASYNC */
	if (0 == (signalOptions & OMRPORT_SIG_OPTIONS_REDUCED_SIGNALS_ASYNCHRONOUS)) {
		supportedFlags |= OMRPORT_SIG_FLAG_SIGQUIT | OMRPORT_SIG_FLAG_SIGTERM;
	}

	if ((flags & supportedFlags) == flags) {
		return 1;
	} else {
		return 0;
	}
}


/**
 * Shutdown the signal handling component of the port library
 */
void
omrsig_shutdown(struct OMRPortLibrary *portLibrary)
{
	omrthread_monitor_t globalMonitor = omrthread_global_monitor();

	removeAsyncHandlers(portLibrary);

	omrthread_monitor_enter(globalMonitor);

	if (--attachedPortLibraries == 0) {
		omrthread_monitor_destroy(asyncMonitor);
		omrthread_tls_free(tlsKeyCurrentSignal);
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

	omrthread_monitor_enter(globalMonitor);

	if (attachedPortLibraries++ == 0) {
		if (omrthread_monitor_init_with_name(&asyncMonitor, 0, "portLibrary_omrsig_async_monitor")) {
			result = -1;
		}

		if (omrthread_tls_alloc(&tlsKeyCurrentSignal)) {
			return -1;
		}
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
	struct J9Win32SignalInfo j9info;

	if ((exceptionInfo->ExceptionRecord->ExceptionCode & (ERROR_SEVERITY_ERROR | APPLICATION_ERROR_MASK)) != ERROR_SEVERITY_ERROR) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	type = mapWin32ExceptionToPortlibType(exceptionInfo->ExceptionRecord->ExceptionCode);
	if (0 == (type & flags)) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	fillInWin32SignalInfo(portLibrary, handler, exceptionInfo, &j9info);

	prevSigType = portLibrary->sig_get_current_signal(portLibrary);
	setCurrentSignal(portLibrary, type);

	__try {
		result = handler(portLibrary, j9info.type, &j9info, handler_arg);
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
fillInWin32SignalInfo(struct OMRPortLibrary *portLibrary, omrsig_handler_fn handler, EXCEPTION_POINTERS *exceptionInfo, struct J9Win32SignalInfo *j9info)
{
	memset(j9info, 0, sizeof(*j9info));

	j9info->type = mapWin32ExceptionToPortlibType(exceptionInfo->ExceptionRecord->ExceptionCode);
	j9info->handlerAddress = (void *)handler;
	j9info->handlerAddress2 = (void *)structuredExceptionHandler;
	j9info->ExceptionRecord = exceptionInfo->ExceptionRecord;
	j9info->ContextRecord = exceptionInfo->ContextRecord;

	/* module info is filled on demand */
}

static uint32_t
infoForSignal(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
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
infoForGPR(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
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
infoForControl(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
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
infoForModule(struct OMRPortLibrary *portLibrary, struct J9Win32SignalInfo *info, int32_t index, const char **name, void **value)
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

static BOOL WINAPI
consoleCtrlHandler(DWORD dwCtrlType)
{
	uint32_t flags;
	BOOL result = FALSE;

	switch (dwCtrlType) {
	case CTRL_BREAK_EVENT:
		flags = OMRPORT_SIG_FLAG_SIGQUIT;
		break;
	case CTRL_C_EVENT:
		flags = OMRPORT_SIG_FLAG_SIGINT;
		break;
	case CTRL_CLOSE_EVENT:
		flags = OMRPORT_SIG_FLAG_SIGTERM;
		break;
	default:
		return result;
	}

	if (0 == omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT)) {
		J9Win32AsyncHandlerRecord *cursor;
		uint32_t handlerCount = 0;

		/* incrementing the asyncThreadCount will prevent the list from being modified while we use it */
		omrthread_monitor_enter(asyncMonitor);
		asyncThreadCount++;
		omrthread_monitor_exit(asyncMonitor);

		cursor = asyncHandlerList;
		while (cursor) {

			if (cursor->flags & flags) {
				cursor->handler(cursor->portLib, flags, NULL, cursor->handler_arg);

				/* Returning TRUE stops control from being passed to the next handler routine. The OS default handler may be the next handler routine.
				 * The default action of the OS default handler routine is to shut down the process
				 */
				if (flags & OMRPORT_SIG_FLAG_SIGQUIT) {
					/* Continue executing */
					result = TRUE;
				}
			}
			cursor = cursor->next;
		}

		omrthread_monitor_enter(asyncMonitor);
		if (--asyncThreadCount == 0) {
			omrthread_monitor_notify_all(asyncMonitor);
		}
		omrthread_monitor_exit(asyncMonitor);

		/* TODO: possible timing hole. The thread library could be unloaded by the time we reach this line. We can't use omrthread_exit(), as that kills the async reporting thread */
		omrthread_detach(NULL);
	}

	return result;
}

static void
removeAsyncHandlers(OMRPortLibrary *portLibrary)
{
	/* clean up the list of async handlers */
	J9Win32AsyncHandlerRecord *cursor;
	J9Win32AsyncHandlerRecord **previousLink;

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
 * sets the priority of the the async reporting thread
 *
 * In windows, a new thread is created to run the handlers each time a signal is received,
 * so this function does nothing.
*/
int32_t
omrsig_set_reporter_priority(struct OMRPortLibrary *portLibrary, uintptr_t priority)
{
	return 0;
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
