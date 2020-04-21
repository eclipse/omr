/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "omrport.h"


#define MAX_UNIX_SIGNAL_TYPES  _NSIG

#define __USE_GNU 1
#include <dlfcn.h>
#undef __USE_GNU

typedef struct OMRPlatformSignalInfo {
	ucontext_t *context;
	Dl_info dl_info;
} OMRPlatformSignalInfo;

typedef struct OMRUnixSignalInfo {
	struct OMRPlatformSignalInfo platformSignalInfo;
	uint32_t portLibrarySignalType;
	void *handlerAddress;
	void *handlerAddress2;
	siginfo_t *sigInfo;
} OMRUnixSignalInfo;

uint32_t infoForFPR(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForGPR(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForModule(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForControl(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
uint32_t infoForSignal(struct OMRPortLibrary *portLibrary, struct OMRUnixSignalInfo *info, int32_t index, const char **name, void **value);
void fillInUnixSignalInfo(struct OMRPortLibrary *portLibrary, void *contextInfo, struct OMRUnixSignalInfo *signalInfo);

