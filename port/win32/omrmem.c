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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Memory Utilities
 */


/*
 * This file contains code for the portability library memory management.
 */

#include <windows.h>
#include "omrport.h"
#include "omrportpriv.h"
#include "omrportpg.h"
#include "ut_omrport.h"

void *
omrmem_allocate_memory_basic(struct OMRPortLibrary *portLibrary, uintptr_t byteAmount)
{
	return HeapAlloc(PPG_mem_heap, 0, byteAmount);
}

void
omrmem_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer)
{
	HeapFree(PPG_mem_heap, 0, memoryPointer);
}

void
omrmem_advise_and_free_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t memorySize)
{
	omrmem_free_memory_basic(portLibrary, memoryPointer);
}

void *
omrmem_reallocate_memory_basic(struct OMRPortLibrary *portLibrary, void *memoryPointer, uintptr_t byteAmount)
{
	return HeapReAlloc(PPG_mem_heap, 0, memoryPointer, byteAmount);
}

void
omrmem_shutdown_basic(struct OMRPortLibrary *portLibrary)
{
	HANDLE memHeap = GetProcessHeap();

	/* has to be cleaned up with HeapFree since omrmem_startup allocated it with HeapAlloc */
	HeapFree(memHeap, 0, portLibrary->portGlobals);
}

void
omrmem_startup_basic(struct OMRPortLibrary *portLibrary, uintptr_t portGlobalSize)
{
	HANDLE memHeap = GetProcessHeap();

	/* done as a HeapAlloc because omrmem_allocate_memory requires portGlobals to be initialized */
	portLibrary->portGlobals = HeapAlloc(memHeap, 0, portGlobalSize);
	if (NULL == portLibrary->portGlobals) {
		return;
	}
	memset(portLibrary->portGlobals, 0, portGlobalSize);

	PPG_mem_heap = memHeap;
}

void *
omrmem_allocate_portLibrary_basic(uintptr_t byteAmount)
{
	HANDLE memHeap = GetProcessHeap();
	return HeapAlloc(memHeap, 0, byteAmount);
}

void
omrmem_deallocate_portLibrary_basic(void *memoryPointer)
{
	HANDLE memHeap = GetProcessHeap();
	HeapFree(memHeap, 0, memoryPointer);
}
