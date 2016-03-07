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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#include <string.h>
#include "omrutil.h"
#include "omrport.h"
#include "omr.h"

static char *
getOMRVMThreadNameNoLock(OMR_VMThread *vmThread)
{
	char *name = NULL;

	name = (char *)vmThread->threadName;

	if (name == NULL) {
		name = OMR_Glue_GetThreadNameForUnamedThread(vmThread);
	}
	return name;
}

char *
getOMRVMThreadName(OMR_VMThread *vmThread)
{
	omrthread_monitor_enter(vmThread->threadNameMutex);
	return getOMRVMThreadNameNoLock(vmThread);
}

char *
tryGetOMRVMThreadName(OMR_VMThread *vmThread)
{
	if (omrthread_monitor_try_enter(vmThread->threadNameMutex) == 0) {
		return getOMRVMThreadNameNoLock(vmThread);
	} else {
		return NULL;
	}
}

void
releaseOMRVMThreadName(OMR_VMThread *vmThread)
{
	omrthread_monitor_exit(vmThread->threadNameMutex);
}

void
setOMRVMThreadNameWithFlag(OMR_VMThread *currentThread, OMR_VMThread *vmThread, char *name, uint8_t nameIsStatic)
{
	omrthread_monitor_enter(vmThread->threadNameMutex);
	setOMRVMThreadNameWithFlagNoLock(vmThread, name, nameIsStatic);
	omrthread_monitor_exit(vmThread->threadNameMutex);
}

void
setOMRVMThreadNameWithFlagNoLock(OMR_VMThread *vmThread, char *name, uint8_t nameIsStatic)
{
	if (!vmThread->threadNameIsStatic) {
		char *oldName = (char *)vmThread->threadName;

		if (name != oldName) {
			OMRPORT_ACCESS_FROM_OMRVMTHREAD(vmThread);
			omrmem_free_memory(oldName);
		}
	}
	vmThread->threadNameIsStatic = nameIsStatic;
	vmThread->threadName = (uint8_t *)name;
}
