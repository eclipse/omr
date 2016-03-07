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
	granularity = omrvmem_supported_page_sizes(portLibrary)[0];
	return granularity;
}
