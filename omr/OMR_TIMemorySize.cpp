/*******************************************************************************
 * Copyright (c) 2014, 2016 IBM Corp. and others
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

#include "omrport.h"
#include "omr.h"
#include "omragent_internal.h"

static omr_error_t getProcessMemorySizeHelper(OMR_VMThread *vmThread, J9VMemMemoryQuery queryType, uint64_t *processMemorySize);

extern "C" omr_error_t
omrtiGetFreePhysicalMemorySize(OMR_VMThread *vmThread, uint64_t *freePhysicalMemorySize)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == freePhysicalMemorySize) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
		int32_t portStatus = omrvmem_get_available_physical_memory(freePhysicalMemorySize);

		if (portStatus < 0) {
			rc = OMR_ERROR_NOT_AVAILABLE;
		}
	}
	OMR_TI_RETURN(vmThread, rc);
}

extern "C" omr_error_t
omrtiGetProcessVirtualMemorySize(OMR_VMThread *vmThread, uint64_t *processMemorySize)
{
	omr_error_t rc = getProcessMemorySizeHelper(vmThread, OMRPORT_VMEM_PROCESS_VIRTUAL, processMemorySize);
	return rc;
}

extern "C" omr_error_t
omrtiGetProcessPhysicalMemorySize(OMR_VMThread *vmThread, uint64_t *processMemorySize)
{
	omr_error_t rc = getProcessMemorySizeHelper(vmThread, OMRPORT_VMEM_PROCESS_PHYSICAL, processMemorySize);
	return rc;
}

extern "C" omr_error_t
omrtiGetProcessPrivateMemorySize(OMR_VMThread *vmThread, uint64_t *processMemorySize)
{
	omr_error_t rc = getProcessMemorySizeHelper(vmThread, OMRPORT_VMEM_PROCESS_PRIVATE, processMemorySize);
	return rc;
}

static omr_error_t
getProcessMemorySizeHelper(OMR_VMThread *vmThread, J9VMemMemoryQuery queryType, uint64_t *processMemorySize)
{
	omr_error_t rc = OMR_ERROR_NONE;
	OMR_TI_ENTER_FROM_VM_THREAD(vmThread);

	if (NULL == vmThread) {
		rc = OMR_THREAD_NOT_ATTACHED;
	} else if (NULL == processMemorySize) {
		rc = OMR_ERROR_ILLEGAL_ARGUMENT;
	} else {
		OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
		int32_t portStatus = omrvmem_get_process_memory_size(queryType, processMemorySize);

		if (portStatus < 0) {
			rc = OMR_ERROR_NOT_AVAILABLE;
		}
	}
	OMR_TI_RETURN(vmThread, rc);
}
