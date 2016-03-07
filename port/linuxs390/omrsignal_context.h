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

#include "omrport.h"


#include <unistd.h>
#include <sys/ucontext.h>

#define MAX_UNIX_SIGNAL_TYPES  _NSIG

#define __USE_GNU 1
#include <dlfcn.h>
#undef __USE_GNU

typedef struct J9PlatformSignalInfo {
	ucontext_t *context;
	uintptr_t breakingEventAddr;
	Dl_info dl_info;
} J9PlatformSignalInfo;

typedef struct J9UnixSignalInfo {
	struct J9PlatformSignalInfo platformSignalInfo;
	uint32_t portLibrarySignalType;
	void *handlerAddress;
	void *handlerAddress2;
	siginfo_t *sigInfo;
} J9UnixSignalInfo;

/* START: need to define this ourselves until we compile on a newer distro */
#define UC_EXTENDED		0x00000001

typedef struct ucontext_extended {
	unsigned long     uc_flags;
	struct ucontext  *uc_link;
	stack_t           uc_stack;
	_sigregs          uc_mcontext;
	unsigned long     uc_sigmask[2];
	unsigned long     uc_gprs_high[16];
} ucontext_extended ;
/* END: need to define this ourselves until we compile on a newer distro */

uint32_t infoForFPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct J9UnixSignalInfo *info, int32_t index, const char **name, void **value);
void fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct J9UnixSignalInfo *j9Info);

