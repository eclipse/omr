/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
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
