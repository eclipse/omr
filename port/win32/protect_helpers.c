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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrcomp.h"
#include "omrport.h"
#include "omrportpriv.h"
#include <errno.h>
#include <windows.h>

/* Helper for j9shmem and omrmmap.
 *
 * Adheres to the j9shmem_protect() and omrmmap() APIs
 */
intptr_t
protect_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags)
{
	DWORD winFlags = 0;
	DWORD oldFlags;
	intptr_t retval = 0;
	intptr_t rc = 0;

	/* Convert from our J9 permissions to windows permissions */
	if ((flags & OMRPORT_PAGE_PROTECT_EXEC) != 0) {
		if ((flags & OMRPORT_PAGE_PROTECT_WRITE) != 0) {
			winFlags = 0x40; // EXECUTE_READWRITE
		} else if ((flags & OMRPORT_PAGE_PROTECT_READ) != 0) {
			winFlags = 0x20; // EXECUTE_READONLY
		} else {
			winFlags = 0x10; // EXECUTE_ONLY
		}
	} else {
		if ((flags & OMRPORT_PAGE_PROTECT_WRITE) != 0) {
			winFlags = 0x04; // READWRITE
		} else if ((flags & OMRPORT_PAGE_PROTECT_READ) != 0) {
			winFlags = 0x02; // READONLY
		} else {
			winFlags = 0x01; // NOACCESS
		}
	}

	/* System call - return value is zero if there was a problem */
	retval = (intptr_t)VirtualProtect(address, length, winFlags, &oldFlags);

	if (retval == 0) {
		/* There was a problem! */
		rc = 1;
		portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_PAGE_PROTECT_FAILED);
	}

	return rc;
}

uintptr_t
protect_region_granularity(struct OMRPortLibrary *portLibrary, void *address)
{
	uintptr_t granularity = 0;
	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	granularity = (uintptr_t)systemInfo.dwAllocationGranularity;

	return granularity;
}
