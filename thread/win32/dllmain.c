/*******************************************************************************
 * Copyright (c) 1991, 2016 IBM Corp. and others
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

/*
 * @file
 * @ingroup Thread
 */


#include <windows.h>
#include <stdlib.h>
#include "omrcfg.h"
#include "omrcomp.h"
#include "omrmutex.h"
#include "thrtypes.h"
#include "thrdsup.h"

extern void omrthread_init(J9ThreadLibrary *lib);
extern void omrthread_shutdown(void);

extern J9ThreadLibrary default_library;

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved);

/**
 * Initialize OS-specific threading helpers.
 *
 * @param hModule handle to module being loaded
 * @param ul_reason_for_call reason why DllMain being called
 * @param lpReserved reserved
 * @return TRUE on success, FALSE on failure.
 */
BOOL APIENTRY
DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{

	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH: {
		omrthread_library_t lib;

		/* Disable DLL_THREAD_ATTACH and DLL_THREAD_DETACH notifications for WIN32*/
		DisableThreadLibraryCalls(hModule);
		lib = GLOBAL_DATA(default_library);
		omrthread_init(lib);
		return lib->initStatus == 1;
	}
	case DLL_PROCESS_DETACH:
		omrthread_shutdown();
	}

	return TRUE;
}
