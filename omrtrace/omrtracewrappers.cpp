/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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
