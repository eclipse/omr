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

#include <sys/ldr.h>
#include <sys/ucontext.h>
#include <stdlib.h>

#ifdef PPC64
#define J9SIG_VALUE_UDATA OMRPORT_SIG_VALUE_64
#else
#define J9SIG_VALUE_UDATA OMRPORT_SIG_VALUE_32
#endif

#define MAX_UNIX_SIGNAL_TYPES  SIGMAX32

/*
 * User context structure
 * The user context structure has the same layout as a
 * sigcontext structure until the uc_link structure, which
 * follows the sigcontext structure.
 */
typedef struct J9PlatformSignalInfo {
	ucontext_t *context;
	struct ld_info ldInfo[128];
} J9PlatformSignalInfo;

typedef struct J9UnixSignalInfo {
	struct J9PlatformSignalInfo platformSignalInfo;
	uint32_t portLibrarySignalType;
	void *handlerAddress;
	void *handlerAddress2;
	siginfo_t *sigInfo;
} J9UnixSignalInfo;

uint32_t infoForFPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
void fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct J9UnixSignalInfo *j9Info);

