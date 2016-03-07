/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2007, 2016
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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
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
