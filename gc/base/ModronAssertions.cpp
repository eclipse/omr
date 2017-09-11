/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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

#include <stdarg.h>

#include "omr.h"
#include "ModronAssertions.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if defined(OMR_GC_DEBUG_ASSERTS)

#define ASSERTION_MESSAGE_BUFFER_SIZE	((uintptr_t)256)

/*
 * The purpose of moving of the body of macros GC Assertions with message to separate function is to prevent inlining
 * on fast path to keep stack frames small
 */

void
omrGcDebugAssertionOutput(OMR_VMThread *omrVMThread, const char *format, ...)
{
	char buffer[ASSERTION_MESSAGE_BUFFER_SIZE];
	OMRPortLibrary *portLibrary = omrVMThread->_vm->_runtime->_portLibrary;

	va_list args;
	va_start(args, format);
	portLibrary->str_vprintf(portLibrary, buffer, ASSERTION_MESSAGE_BUFFER_SIZE, format, args);
	va_end(args);

	Trc_MM_AssertionWithMessage_outputMessage(omrVMThread->_language_vmthread, buffer);
	portLibrary->tty_printf(portLibrary, "%s", buffer);
}

#endif /* defined(OMR_GC_DEBUG_ASSERTS) */

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */
