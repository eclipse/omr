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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#include "omrport.h"
#include "pool_api.h"
#include "omrutil.h"

void *
pool_portLibAlloc(OMRPortLibrary *portLibrary, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit)
{
	return portLibrary->mem_allocate_memory(portLibrary, size, callSite, memoryCategory);
}

void
pool_portLibFree(OMRPortLibrary *portLibrary, void *address, uint32_t type)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	omrmem_free_memory(address);
}


#if defined(OMR_ENV_DATA64)

void *
pool_portLibAlloc32(OMRPortLibrary *portLibrary, uint32_t size, const char *callSite, uint32_t memoryCategory, uint32_t type, uint32_t *doInit)
{
	void *address = NULL;

	if (POOL_ALLOC_TYPE_PUDDLE == type) {
		address = portLibrary->mem_allocate_memory32(portLibrary, size, callSite, memoryCategory);
	} else {
		address = portLibrary->mem_allocate_memory(portLibrary, size, callSite, memoryCategory);
	}

	return address;
}


void
pool_portLibFree32(OMRPortLibrary *portLibrary, void *address, uint32_t type)
{
	OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
	if (POOL_ALLOC_TYPE_PUDDLE == type) {
		omrmem_free_memory32(address);
	} else {
		omrmem_free_memory(address);
	}
}

#endif /* defined(OMR_ENV_DATA64) */
