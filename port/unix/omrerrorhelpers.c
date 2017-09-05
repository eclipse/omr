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
 * @internal
 * @file
 * @ingroup Port
 * @brief Error Handling
 *
 * Helper utilities for @ref omrerror.c.  This file reduces the amount of code duplication as formatting of messages
 * from the OS is the only part of error handling that can not be handled in a generic manner.
 *
 * These functions are not accessible via the port library function table.
 */


#include <string.h>
#include <errno.h>
#include "omrportpriv.h"
#include "omrportptb.h"

/**
 * @internal
 * @brief Error Handling
 *
 * Given an error code save the OS error message to the ptBuffers and return
 * a reference to the saved message.
 *
 * @param[in] portLibrary The port library
 * @param[in] errorCode The platform specific error code to look up.
 *
 * @note By the time this function is called it is known that ptBuffers are not NULL.  It is possible, however, that the
 * buffer to hold the error message has not yet been allocated..
 *
 * @note Buffer is managed by the port library, do not free
 */
const char *
errorMessage(struct OMRPortLibrary *portLibrary, int32_t errorCode)
{
	PortlibPTBuffers_t ptBuffers;
	char *errString = strerror(errorCode);

	ptBuffers = omrport_tls_peek(portLibrary);
	if (0 == ptBuffers->errorMessageBufferSize) {
		ptBuffers->errorMessageBuffer = portLibrary->mem_allocate_memory(portLibrary, J9ERROR_DEFAULT_BUFFER_SIZE, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == ptBuffers->errorMessageBuffer) {
			return "";
		}
		ptBuffers->errorMessageBufferSize = J9ERROR_DEFAULT_BUFFER_SIZE;
	}

#if defined(AIXPPC)
	/* On AIX, strerror() returns an error string based on current locale encoding. When printing out the error message in file_write_using_iconv(),
	 * The NLS message (UTF-8) + the error string is converted from UTF-8 to locale encoding. So non-UTF-8 error string needs to be converted to UTF-8 here.
	 */
	if (0 != strcmp(nl_langinfo(CODESET), "UTF-8")) {
		char stackBuf[512];
		uintptr_t bufLen = sizeof(stackBuf);

		memset(stackBuf, '\0', bufLen);
		if (0 < portLibrary->str_convert(portLibrary, J9STR_CODE_PLATFORM_RAW, J9STR_CODE_UTF8, errString, strlen(errString), stackBuf, bufLen)) {
			errString = stackBuf;
		}
	}
#endif /* defined(AIXPPC) */

	/* Copy from OS to ptBuffers */
	portLibrary->str_printf(portLibrary, ptBuffers->errorMessageBuffer, ptBuffers->errorMessageBufferSize, "%s", errString);
	ptBuffers->errorMessageBuffer[ptBuffers->errorMessageBufferSize - 1] = '\0';
	return ptBuffers->errorMessageBuffer;
}

