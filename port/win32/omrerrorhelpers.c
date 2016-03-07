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

#include <stdio.h>
#include <string.h>
#include "omrportpriv.h"
#include "portnls.h"
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
	/* Code cloned in EPOC32 helper */
	PortlibPTBuffers_t ptBuffers;
	char *message;
	int rc = 0, i;
	uintptr_t out;
	wchar_t ubuffer[J9ERROR_DEFAULT_BUFFER_SIZE];

	ptBuffers = omrport_tls_peek(portLibrary);
	if (0 == ptBuffers->errorMessageBufferSize) {
		ptBuffers->errorMessageBuffer = portLibrary->mem_allocate_memory(portLibrary, J9ERROR_DEFAULT_BUFFER_SIZE, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL == ptBuffers->errorMessageBuffer) {
			return "";
		}
		ptBuffers->errorMessageBufferSize = J9ERROR_DEFAULT_BUFFER_SIZE;
	}
	message = ptBuffers->errorMessageBuffer;

	rc = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, 0, ubuffer, J9ERROR_DEFAULT_BUFFER_SIZE, NULL);
	if (rc == 0) {
		const char *format;
		format = portLibrary->nls_lookup_message(portLibrary,
				 J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				 J9NLS_PORT_ERROR_OPERATION_FAILED,
				 "Operation Failed: %d (%s failed: %d)");
		portLibrary->str_printf(portLibrary, message, ptBuffers->errorMessageBufferSize, format, errorCode, "FormatMessage", GetLastError());
		message[ptBuffers->errorMessageBufferSize - 1] = '\0';
		return message;
	}

	/* Convert Unicode -> UTF8, with a few special cases */
	out = portLibrary->str_printf(portLibrary, message, ptBuffers->errorMessageBufferSize, "(%d) ", errorCode);
	for (i = 0; i < rc; i++) {
		uintptr_t ch = ubuffer[i];
		if (ch == '\r') { /* Strip CR */
			continue;
		}
		if (ch == '\n') { /* Convert LF to space */
			ch = ' ';
		}
		if (ch < 0x80) {
			if ((out + 2) >= J9ERROR_DEFAULT_BUFFER_SIZE) {
				break;
			}
			message[out++] = (char)ch;
		} else if (ch < 0x800) {
			if ((out + 3) >= J9ERROR_DEFAULT_BUFFER_SIZE) {
				break;
			}
			message[out++] = (char)(0x80 | (ch & 0x3f));
			message[out++] = (char)(0xc0 | (ch >> 6));
		} else {
			if ((out + 4) >= J9ERROR_DEFAULT_BUFFER_SIZE) {
				break;
			}
			message[out++] = (char)(0x80 | (ch & 0x3f));
			message[out++] = (char)(0x80 | ((ch >> 6) & 0x3f));
			message[out++] = (char)(0xe0 | (ch >> 12));
		}
	}
	message[out] = '\0';

	/* There may be extra spaces at the end of the message, due to stripping of the LF
	 * it is replaced by a space.  Multi-line OS messages are thus one long continuous line for us
	 */
	while (iswspace(message[out]) || (message[out] == '\0')) {
		message[out--] = '\0';
	}
	return message;
}

