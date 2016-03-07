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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Memory Utilities
 */


/*
 * This file contains code for the portability library memory management.
 */

#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"

void *
omrmem_allocate_memory_basic(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount)
{
	return (void *) malloc(byteAmount);
}

void
omrmem_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	free(memoryPointer);
}

void
omrmem_advise_and_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t memorySize)
{
	omrmem_free_memory_basic(portLibrary, memoryPointer);
}

void *
omrmem_reallocate_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount)
{
	return (void *) realloc(memoryPointer, byteAmount);
}

void
omrmem_shutdown_basic(struct OMRPortLibrary *portLibrary)
{
	portLibrary->mem_free_memory(portLibrary, portLibrary->portGlobals);
}

void
omrmem_startup_basic(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize)
{
	portLibrary->portGlobals = portLibrary->mem_allocate_memory(portLibrary, portGlobalSize, OMR_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY);
	if (!portLibrary->portGlobals) {
		return;
	}
	memset(portLibrary->portGlobals, 0, portGlobalSize);
}

void *
omrmem_allocate_portLibrary_basic(uintptr_t byteAmount)
{
	return (void *) malloc(byteAmount);
}

void
omrmem_deallocate_portLibrary_basic(void *memoryPointer)
{
	free(memoryPointer);
}
