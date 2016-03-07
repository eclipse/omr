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

#include <errno.h>
#include "createTestHelper.h"

extern OMRPortLibrary *portLib;

void
port_printf(const char *str, ...)
{
	va_list vargs;
	OMRPORT_ACCESS_FROM_OMRPORT(portLib);
	va_start(vargs, str);
	omrtty_vprintf(str, vargs);
	va_end(vargs);
}

intptr_t
omrthread_verboseCall(const char *func, intptr_t retVal)
{
	if (retVal != J9THREAD_SUCCESS) {
		intptr_t errnoSet = retVal & J9THREAD_ERR_OS_ERRNO_SET;
		omrthread_os_errno_t os_errno = J9THREAD_INVALID_OS_ERRNO;
#if defined(J9ZOS390)
		omrthread_os_errno_t os_errno2 = omrthread_get_os_errno2();
#endif /* J9ZOS390 */

		if (errnoSet) {
			os_errno = omrthread_get_os_errno();
		}
		retVal &= ~J9THREAD_ERR_OS_ERRNO_SET;

		if (retVal == J9THREAD_ERR_UNSUPPORTED_ATTR) {
			if (errnoSet) {
#if defined(J9ZOS390)
				PRINTF("%s unsupported: retVal %zd (%zx) : errno %zd (%zx) %s, errno2 %zd (%zx)\n", func, retVal, retVal, os_errno, os_errno, strerror((int)os_errno), os_errno2, os_errno2);
#else /* !J9ZOS390 */
				PRINTF("%s unsupported: retVal %zd (%zx) : errno %zd %s\n", func, retVal, retVal, os_errno, strerror((int)os_errno));
#endif /* !J9ZOS390 */
			} else {
				PRINTF("%s unsupported: retVal %zd (%zx)\n", func, retVal, retVal);
			}
		} else {
			if (errnoSet) {
#if defined(J9ZOS390)
				PRINTF("%s failed: retVal %zd (%zx) : errno %zd (%zx) %s, errno2 %zd (%zx)\n", func, retVal, retVal, os_errno, os_errno, strerror((int)os_errno), os_errno2, os_errno2);
#else /* !J9ZOS390 */
				PRINTF("%s failed: retVal %zd (%zx) : errno %zd %s\n", func, retVal, retVal, os_errno, strerror((int)os_errno));
#endif /* !J9ZOS390 */
			} else {
				PRINTF("%s failed: retVal %zd (%zx)\n", func, retVal, retVal);
			}
		}
	} else {
		PRINTF("%s\n", func);
	}
	return retVal;
}

intptr_t
pthread_verboseCall(const char *func, intptr_t retVal)
{
	if (retVal != 0) {
#ifdef J9ZOS390
		PRINTF("%s: %s\n", func, strerror(errno));
#else
		PRINTF("%s: %s\n", func, strerror((int)retVal));
#endif
	}
	return retVal;
}
