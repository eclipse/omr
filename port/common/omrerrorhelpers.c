/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
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
errorMessage(struct OMRPortLibrary *portLibrary,  int32_t errorCode)
{
	PortlibPTBuffers_t ptBuffers;
	uint32_t requiredSize;
	const char *format;

	/* Get the ptBuffers */
	ptBuffers = omrport_tls_peek(portLibrary);

	/* Get the message from NLS */
	format = portLibrary->nls_lookup_message(portLibrary,
			 J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE,
			 J9NLS_PORT_ERROR_OPERATION_FAILED,
			 "Operation Failed: %d");

	requiredSize = strlen(format) + 20; /* Giving 20 digits for a number, seems sufficient */
	requiredSize < J9ERROR_DEFAULT_BUFFER_SIZE ? J9ERROR_DEFAULT_BUFFER_SIZE : requiredSize;
	if (requiredSize > ptBuffers->errorMessageBufferSize) {
		char *newBuffer = portLibrary->mem_allocate_memory(portLibrary, requiredSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
		if (NULL != newBuffer) {
			portLibrary->mem_free_memory(portLibrary, ptBuffers->errorMessageBuffer);
			ptBuffers->errorMessageBuffer = newBuffer;
			ptBuffers->errorMessageBufferSize = requiredSize;
		}
	}

	/* Save the message */
	if (ptBuffers->errorMessageBufferSize > 0) {
		portLibrary->str_printf(portLibrary, ptBuffers->errorMessageBuffer, ptBuffers->errorMessageBufferSize, format, errorCode);
		ptBuffers->errorMessageBuffer[ptBuffers->errorMessageBufferSize - 1] = '\0';
		return ptBuffers->errorMessageBuffer;
	}

	return "";
}

