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

#include "omrport.h"

#include "edcwccwi.h"
#include <leawi.h>
#include <ctest.h>
#include <signal.h>

#define MAX_UNIX_SIGNAL_TYPES  65
#define MAX_NAME 256
#define NUM_REGS 16
#define NUM_VECTOR_REGS 32

typedef struct J9PlatformSignalInfo {
	__mcontext_t_ *context;
	uintptr_t breakingEventAddr;
#if !defined(J9ZOS39064)
	char program_unit_name[MAX_NAME];
	_INT4 program_unit_address;
	char entry_name[MAX_NAME];
	_INT4 entry_address;
#endif
} J9PlatformSignalInfo;

typedef struct J9UnixSignalInfo {
	uint32_t portLibrarySignalType;
	void *handlerAddress;
	void *handlerAddress2;
	siginfo_t *sigInfo;
	struct J9PlatformSignalInfo platformSignalInfo;
} J9UnixSignalInfo;

uint32_t infoForFPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForVR(struct OMRPortLibrary *portLibrary, J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
void fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct J9UnixSignalInfo *j9Info);
BOOLEAN checkIfResumableTrapsSupported(struct OMRPortLibrary *portLibrary);

