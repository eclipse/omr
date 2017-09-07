/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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
