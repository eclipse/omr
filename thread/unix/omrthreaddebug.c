/*******************************************************************************
 * Copyright (c) 2007, 2016 IBM Corp. and others
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
 * @file
 * @ingroup Thread
 * @brief Debug helpers
 */

#include <stdio.h>
#include <string.h>
#include "threaddef.h"

#ifdef THREAD_ASSERTS
/**
 * Verbose reporting of return codes from system calls.
 * @param func Function call, as a string.
 * @param retVal Return value from function call.
 * @returns retVal
 */
intptr_t
omrthread_debug_syscall(const char *func, intptr_t retVal)
{
	if (retVal != 0) {
		fprintf(stderr, "%s: %s\n", func, strerror(retVal));
	}
	return retVal;
}
#endif
