/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
