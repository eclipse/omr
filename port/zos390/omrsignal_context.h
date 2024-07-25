/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "omrport.h"

#include "edcwccwi.h"
#include <leawi.h>
#include <ctest.h>
#include <signal.h>

#define MAX_UNIX_SIGNAL_TYPES  65
#define MAX_NAME 256
#define NUM_REGS 16
#define NUM_VECTOR_REGS 32

typedef struct OMRPlatformSignalInfo {
	__mcontext_t_ *context;
	uintptr_t breakingEventAddr;
#if !defined(J9ZOS39064)
	char program_unit_name[MAX_NAME];
	_INT4 program_unit_address;
	char entry_name[MAX_NAME];
	_INT4 entry_address;
#endif
} OMRPlatformSignalInfo;

typedef struct OMRUnixSignalInfo {
	uint32_t portLibrarySignalType;
	void *handlerAddress;
	void *handlerAddress2;
	siginfo_t *sigInfo;
	struct OMRPlatformSignalInfo platformSignalInfo;
} OMRUnixSignalInfo;

uint32_t infoForFPR(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForVR(struct OMRPortLibrary *portLibrary, OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
void fillInJumpInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, void *jumpInfo);
void fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct OMRUnixSignalInfo *signalInfo);
BOOLEAN checkIfResumableTrapsSupported(struct OMRPortLibrary *portLibrary);

