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
#include <sys/mman.h>

struct {
	uint32_t portLibPermission;
	int unixPermission;
} permissionsMap[] = {
	{OMRPORT_PAGE_PROTECT_NONE, PROT_NONE},
	{OMRPORT_PAGE_PROTECT_READ, PROT_READ},
	{OMRPORT_PAGE_PROTECT_WRITE, PROT_WRITE},
	{OMRPORT_PAGE_PROTECT_EXEC, PROT_EXEC}
};

/**
 * @internal @file
 * @ingroup Port
 * @brief Helpers for protecting shared memory regions in the virtual address space (used by omrmmap and j9shmem).
 */

/* Helper for j9shmem and omrmmap.
 *
 * Adheres to the j9shmem_protect() and omrmmap() APIs
 */
intptr_t
protect_memory(struct OMRPortLibrary *portLibrary, void *address, uintptr_t length, uintptr_t flags)
{
	uintptr_t index;
	intptr_t unixFlags = 0;
	intptr_t rc = -1;

	/* convert the port library permissions to the UNIX equivalents */
	for (index = 0; index < sizeof(permissionsMap) / sizeof(permissionsMap[0]); index++) {
		if ((permissionsMap[index].portLibPermission & flags) != 0) {
			unixFlags |= permissionsMap[index].unixPermission;
		}
	}

	rc = mprotect(address, length, unixFlags);
	if (rc != 0) {
		portLibrary->error_set_last_error(portLibrary, errno, OMRPORT_PAGE_PROTECT_FAILED);
		/* TODO: set the actual error message as well */
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
