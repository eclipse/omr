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
