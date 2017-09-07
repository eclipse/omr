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

#include <stdio.h>
#include <stdarg.h>

#if defined(J9ZOS390)
#include <stdlib.h> /* for abs */
#include <string.h> /* for strlen, strcpy */
#endif /* defined(J9ZOS390) */

#include "omrtrace_internal.h"
#include "thread_api.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif

void
twFprintf(const char *formatStr, ...)
{
	OMRPORT_ACCESS_FROM_OMRPORT(OMR_TRACEGLOBAL(portLibrary));
	va_list arg_ptr;

	va_start(arg_ptr, formatStr);
	omrtty_err_vprintf(formatStr, arg_ptr);
	va_end(arg_ptr);
}

OMR_TraceThread *
twThreadSelf(void)
{
	omrthread_t self = omrthread_self();
	return (OMR_TraceThread *)(self? omrthread_tls_get(self, j9uteTLSKey) : NULL);
}

omr_error_t
twE2A(char *str)
{
#if defined(J9ZOS390)
	long length = (long)strlen(str);
	if (length > 0) {
		char *abuf;
		abuf = e2a(str, length);
		if (abuf) {
			strcpy(str, abuf);
			free(abuf);
		}
	}
#endif /* defined(J9ZOS390) */
	return OMR_ERROR_NONE;
}
