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
  * @brief TTY output
  *
  * All VM output goes to stderr by default.  These routines provide the helpers for such output.
  */


#include <windows.h>
#include <stdio.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"

/* private-prototypes */


/* #define TTY_LOG_FILE "d:\\ive\\tty" */

#ifdef TTY_LOG_FILE
int32_t fd;
#endif




/**
 * PortLibrary startup.
 *
 * This function is called during startup of the portLibrary.  Any resources that are required for
 * the TTY library operations may be created here.  All resources created here should be destroyed
 * in @ref omrtty_shutdown.
 *
 * @param[in] portLibrary The port library.
 *
 * @return 0 on success, negative error code on failure.  Error code values returned are
 * \arg OMRPORT_ERROR_STARTUP_TTY
 * \arg OMRPORT_ERROR_STARTUP_TTY_HANDLE
 * \arg OMRPORT_ERROR_STARTUP_TTY_CONSOLE
 *
 * @note Most implementations will simply return success.
 */
int32_t
omrtty_startup(struct OMRPortLibrary *portLibrary)
{
	HANDLE handle, dupHandle;
	HANDLE proc = GetCurrentProcess();

	handle = GetStdHandle(STD_INPUT_HANDLE);
	DuplicateHandle(proc, handle, proc, &dupHandle, 0, 0, DUPLICATE_SAME_ACCESS);
	PPG_tty_consoleInputHd = dupHandle;

#ifdef TTY_LOG_FILE
	fd = portLibrary->file_open(portLibrary, TTY_LOG_FILE, EsOpenCreate | EsOpenWrite, 0);
#define MSG "========== BEGIN ==========\n\n"
	portLibrary->file_write(portLibrary, fd, MSG, sizeof(MSG) - 1);
#undef MSG
#else
	handle = GetStdHandle(STD_OUTPUT_HANDLE);
	DuplicateHandle(proc, handle, proc, &dupHandle, 0, 0, DUPLICATE_SAME_ACCESS);
	PPG_tty_consoleOutputHd = dupHandle;

	handle = GetStdHandle(STD_ERROR_HANDLE);
	DuplicateHandle(proc, handle, proc, &dupHandle, 0, 0, DUPLICATE_SAME_ACCESS);
	PPG_tty_consoleErrorHd = dupHandle;
#endif

	/* initialize the lazy buffer for grabbing console events */
	PPG_tty_consoleEventBuffer = NULL;
	if (omrthread_monitor_init_with_name(&PPG_tty_consoleBufferMonitor, 0, "Windows native console event lock")) {
		return OMRPORT_ERROR_STARTUP_TTY;
	}

	return 0;
}
/**
 * PortLibrary shutdown.
 *
 * This function is called during shutdown of the portLibrary.  Any resources that were created by @ref omrtty_startup
 * should be destroyed here.
 *
 * @param[in] portLibrary The port library.
 *
 * @note Most implementations will be empty.
 */
void
omrtty_shutdown(struct OMRPortLibrary *portLibrary)
{
	if (NULL != portLibrary->portGlobals) {
		CloseHandle(PPG_tty_consoleInputHd);
#ifdef TTY_LOG_FILE
#define MSG "\n========== END ==========\n\n"
		portLibrary->file_write(portLibrary, fd, MSG, sizeof(MSG) - 1);
#undef MSG
		portLibrary->file_close(portLibrary, fd);
#else
		CloseHandle(PPG_tty_consoleOutputHd);
		CloseHandle(PPG_tty_consoleErrorHd);
#endif

		/* free the console event buffer and the related monitor */
		if (NULL != PPG_tty_consoleEventBuffer) {
			portLibrary->mem_free_memory(portLibrary, PPG_tty_consoleEventBuffer);
			PPG_tty_consoleEventBuffer = NULL;
		}
		omrthread_monitor_destroy(PPG_tty_consoleBufferMonitor);
	}
}



/**
 * Write characters to stderr.
 *
 * @param[in] portLibrary The port library.
 * @param[in] format The format string to be output.
 * @param[in] ... arguments for format.
 *
 * @note Use omrfile_printf for stdout output.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
omrtty_printf(struct OMRPortLibrary *portLibrary, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	portLibrary->tty_vprintf(portLibrary, format, args);
	va_end(args);
}




/**
 * Read characters from stdin into buffer.
 *
 * @param[in] portLibrary The port library.
 * @param[out] s Buffer.
 * @param[in] length Size of buffer (s).
 *
 * @return The number of characters read, -1 on error.
 */
intptr_t
omrtty_get_chars(struct OMRPortLibrary *portLibrary, char *s, uintptr_t length)
{
	DWORD	nCharsRead;
	DWORD	result;

	result = ReadFile(PPG_tty_consoleInputHd, s, (DWORD)length, &nCharsRead, NULL);

	/*[PR 103488] only return -1 on error, return zero for EOF */
	if (!result) {
		/*[CMVC 72713] return EOF for broken pipe */
		if (GetLastError() == ERROR_BROKEN_PIPE) {
			return 0;
		}
		return -1;
	}
	return nCharsRead;
}



/**
 * Output message to stderr.
 *
 * @param[in] portLibrary The port library.
 * @param[in] format The format String.
 * @param[in] ... argument list.
 *
 * @deprecated All output goes to stderr, use omrtty_printf()
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
omrtty_err_printf(struct OMRPortLibrary *portLibrary, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	portLibrary->tty_err_vprintf(portLibrary, format, args);
	va_end(args);
}



/**
 * Determine the number of characters remaining to be read from stdin.
 *
 * @param[in] portLibrary The port library.
 *
 * @return number of characters remaining to be read.
 */
intptr_t
omrtty_available(struct OMRPortLibrary *portLibrary)
{
	DWORD result, current, end;
	/* First try stdin as a pipe */
	if (PeekNamedPipe(PPG_tty_consoleInputHd, NULL, 0, NULL, &result, NULL)) {
		return result;
	} else {
		/* this is probably because we aren't reading a pipe but a console */
		if (ERROR_INVALID_HANDLE == GetLastError()) {
			/* Note that this could be done on the stack if we believe that it might overflow (40 k) so dynamic allocation is safer for now */
			/* The number of events being 2000 is consistent with other JDKs.
			 * (If you put more than 999 chars into the buffer before hitting enter, you lock up standard input.)
			 */
#define EVENTS_TO_CAPTURE 2000
			DWORD available = 0;

			omrthread_monitor_enter(PPG_tty_consoleBufferMonitor);
			if (NULL == PPG_tty_consoleEventBuffer) {
				PPG_tty_consoleEventBuffer = portLibrary->mem_allocate_memory(portLibrary, EVENTS_TO_CAPTURE * sizeof(INPUT_RECORD), OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
				/* if we failed to make the buffer, just give up and say we found nothing */
				if (NULL == PPG_tty_consoleEventBuffer) {
					omrthread_monitor_exit(PPG_tty_consoleBufferMonitor);
					return 0;
				}
			}

			if (PeekConsoleInput(PPG_tty_consoleInputHd, (INPUT_RECORD *)PPG_tty_consoleEventBuffer, EVENTS_TO_CAPTURE, &available)) {
				int x = 0;
				int count = 0;
				INPUT_RECORD *inputRecordPointer = (INPUT_RECORD *)PPG_tty_consoleEventBuffer;	/* provided to avoid some casting */
				BOOL bytesCanBeRead = FALSE;

				for (x = 0; x < (int)available; x++) {
					if ((KEY_EVENT == inputRecordPointer[x].EventType) && (inputRecordPointer[x].Event.KeyEvent.bKeyDown) && ('\r' == inputRecordPointer[x].Event.KeyEvent.uChar.AsciiChar)) {
						bytesCanBeRead = TRUE;	/* report true if we find a return char since we know that it is safe to read at least that one byte */
						break;
					}
				}
				omrthread_monitor_exit(PPG_tty_consoleBufferMonitor);
				return bytesCanBeRead ? 1 : 0;
			} else {
				omrthread_monitor_exit(PPG_tty_consoleBufferMonitor);
			}
#undef EVENTS_TO_CAPTURE
		}
		/* fall through to the file case */
		/* See if stdin is a file */
		current = SetFilePointer(PPG_tty_consoleInputHd, 0, NULL, FILE_CURRENT);
		if (current != 0xFFFFFFFF) {
			end = SetFilePointer(PPG_tty_consoleInputHd, 0, NULL, FILE_END);
			SetFilePointer(PPG_tty_consoleInputHd, current, NULL, FILE_BEGIN);
			if (end != 0xFFFFFFFF) {
				return end - current;
			}
		}
	}
	return 0;
}

/**
 * Output message to stderr.
 *
 * @param[in] portLibrary The port library.
 * @param[in] format The format String.
 * @param[in] args Variable argument list.
 *
 * @note Use omrfile_vprintf for stdout output.
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
omrtty_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args)
{
	portLibrary->file_vprintf(portLibrary, OMRPORT_TTY_ERR, format, args);
}
/**
 * Output message to stderr.
 *
 * @param[in] portLibrary The port library.
 * @param[in] format The format String.
 * @param[in] args Variable argument list.
 *
 * @deprecated All output goes to stderr, use omrtty_vprintf()
 *
 * @internal @note Supported, portable format specifiers are described in the document entitled "PortLibrary printf"
 * in the "Inside J9" Lotus Notes database.
 */
void
omrtty_err_vprintf(struct OMRPortLibrary *portLibrary, const char *format, va_list args)
{
	portLibrary->file_vprintf(portLibrary, OMRPORT_TTY_ERR, format, args);
}

/**
 * This method allows the caller to "daemonize" the current process by closing handles
 * used by the port library
 *
 * @param[in] portLibrary The port library.
 *
 */
void
omrtty_daemonize(struct OMRPortLibrary *portLibrary)
{
	/* close the duplicate file handles maintained by the port library */
	if (NULL != portLibrary->portGlobals) {
		CloseHandle(PPG_tty_consoleInputHd);
#ifdef TTY_LOG_FILE
#define MSG "\n========== END ==========\n\n"
		portLibrary->file_write(portLibrary, fd, MSG, sizeof(MSG) - 1);
#undef MSG
		portLibrary->file_close(portLibrary, fd);
#else
		CloseHandle(PPG_tty_consoleOutputHd);
		CloseHandle(PPG_tty_consoleErrorHd);
#endif
	}
}



