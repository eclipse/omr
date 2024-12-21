/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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

#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include "omrgetsysname.h"

/* Get the zOS SYSNAME sysparm. */
uintptr_t
omrget_sysname(struct OMRPortLibrary *portLibrary, char *sysname, uintptr_t length)
{
	uintptr_t result = 0;
	struct utsname osnamedetails;
	int rc = __osname(&osnamedetails);

	if (rc < 0) {
		/* Return empty string on failure. */
		if ((NULL == sysname) || (length < 1)) {
			result = 1;
		} else {
			*sysname = '\0';
		}
	} else {
		/* The SYSNAME sysparm is found in the nodename field. */
		int len = strlen(osnamedetails.nodename) + 1;
		if ((NULL == sysname) || (len > length)) {
			result = len;
		} else {
			strcpy(sysname, osnamedetails.nodename);
		}
	}

	return result;
}
