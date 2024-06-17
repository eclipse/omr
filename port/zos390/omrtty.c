/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

/**
  * @file
  * @ingroup Port
  * @brief TTY output
  *
  * All VM output goes to stderr by default.  These routines provide the helpers for such output.
  */


#include <stdio.h>
#include <stdlib.h> /* minimally needed for atoe.h below - for malloc() prototype */
#include <stdarg.h>
#include "omrport.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <errno.h>

#if !defined(OMR_EBCDIC)
#include "atoe.h"
#include "omriconvhelpers.h"
#endif /* !defined(OMR_EBCDIC) */

void WRITE_TTY(int fileno, char *b, int bcount);


void
WRITE_TTY(int fileno, char *b, int bcount)
{
#if !defined(OMR_EBCDIC)
	char *s = a2e(b, bcount);
	write(fileno, s, bcount);
	free(s);
#else /* !defined(OMR_EBCDIC) */
	write(fileno, b, bcount);
#endif /* !defined(OMR_EBCDIC) */
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
	int rc;
	off_t curr, end;
	intptr_t avail = 0;

	/* when redirected from a file */
	curr = lseek(STDIN_FILENO, 0L, SEEK_CUR); /* don't use tell(), it doesn't exist on all platforms, i.e. linux */
	if (curr != -1) {
		end = lseek(STDIN_FILENO, 0L, SEEK_END);
		lseek(STDIN_FILENO, curr, SEEK_SET);
		if (end >= curr) {
			return end - curr;
		}
	}

	/* ioctl doesn't work for files on all platforms (i.e. SOLARIS) */
	rc = ioctl(STDIN_FILENO, FIONREAD, &avail);
	if (rc != -1) {
		return avail;
	}
	return 0;
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
	intptr_t rc = -1;
	/* CMVC 178203 - Restart system calls interrupted by EINTR */
	do {
		rc = read(STDIN_FILENO, s, length);
	} while ((-1 == rc) && (EINTR == errno));

	return rc;
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
#if !defined(OMR_EBCDIC)
	iconv_init();
#endif /* !defined(OMR_EBCDIC) */
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
#if !defined(OMR_EBCDIC)
	/* corresponding iconv_global_init() is invoked in protectedInitializeJavaVM (setGlobalConvertersAware())
	 * instead of omrtty_startup because a certain parameter needs to be parsed
	 * before omrtty_startup is called.
	 */
	iconv_global_destroy(portLibrary);
#endif /* !defined(OMR_EBCDIC) */
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
	/* no special handling of file handles, nothing to do */
}

