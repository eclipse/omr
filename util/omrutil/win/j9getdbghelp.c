/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
 *
 * This program and the accompanying materials are made available 
 *  under the terms of the Eclipse Public License 2.0 which 
 * accompanies this distribution and is available at 
 * https://www.eclipse.org/legal/epl-2.0/ or the 
 * Apache License, Version 2.0 which accompanies this distribution 
 *  and is available at https://www.apache.org/licenses/LICENSE-2.0.
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
 ******************************************************************************/

#if !defined(WIN32_IBMC)

#include <windows.h>
#include <winnt.h>
#include <stdlib.h>
#include <stdio.h>
#include "omrport.h"
#include "omr.h"
#include "omrutil.h"

#define UNICODE_DBGHELPDLL_NAME L"dbghelp.dll"

static uintptr_t getDbgHelpDLLLocation(wchar_t *dbghelpPath);

/**
 * Load the version of dbghelp.dll that shipped with the JRE.
 * If we can't find the shipped version, try to find it somewhere else.
 *
 * @return A handle to dbghelp.dll if we were able to find one, NULL otherwise.
 *
 */
uintptr_t
j9getdbghelp_loadDLL(void)
{

	HINSTANCE dbghelpDLL = NULL;
	wchar_t dbgHelpPath[EsMaxPath];

	if (0 != getDbgHelpDLLLocation(dbgHelpPath)) {
		return 0;
	}

	dbghelpDLL = LoadLibraryExW(dbgHelpPath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (dbghelpDLL != NULL) {
#if defined(GDHL_DEBUG)
		printf("\nloadDbgHelpDLL loaded shipped version");
#endif
	} else {
		/* see if the OS can find it for us anywhere */
		dbghelpDLL = LoadLibraryW(UNICODE_DBGHELPDLL_NAME);

		if (dbghelpDLL != NULL) {
#if defined(GDHL_DEBUG)
			printf("\nloadDbgHelpDLL could not find shipped version, but the OS found another");
#endif
		}
	}

#if defined(GDHL_DEBUG)
	printf("\nloadDbgHelpDLL returning %p\n", dbghelpDLL);
#endif

	return (uintptr_t) dbghelpDLL;

}

/**
 * Get a previously loaded version of dbghelp.dll that shipped with the JRE.
 *
 * @return A handle to dbghelp.dll that shipped with the JRE if we were able to find a previously loaded version, NULL otherwise.
 *
 */
uintptr_t
j9getdbghelp_getDLL(void)
{

	HINSTANCE dbghelpDLL = NULL;
	wchar_t dbgHelpPath[EsMaxPath];

	if (0 != getDbgHelpDLLLocation(dbgHelpPath)) {
		return 0;
	}

	dbghelpDLL = GetModuleHandleW(dbgHelpPath);

#if defined(GDHL_DEBUG)
	printf("\ngetDbgHelpDLL returning %p\n", dbghelpDLL);
#endif

	return (uintptr_t) dbghelpDLL;

}

/**
 * Free the supplied version of dbgHelpDLL
 *
 * @return 0 if the library was freed, non-zero otherwise
 *
 */
void
j9getdbghelp_freeDLL(uintptr_t dbgHelpDLL)
{
	if (NULL != (HMODULE)dbgHelpDLL) {
		FreeLibrary((HMODULE) dbgHelpDLL);
	}
	return;
}

/**
 *  Tell us where the shipped version of dbghelp.dll is located
 *
 * @param [in/out] dbghelpPath buffer to populate with the location of dbghelp.dll. Assumed to be character length of EsMaxPath.
 *
 * @return 0 on success, non-zero otherwise.
 *
 */
static uintptr_t
getDbgHelpDLLLocation(wchar_t *dbghelpPath)
{
	wchar_t *pathEnd = NULL;
	if (OMR_ERROR_NONE == detectVMDirectory(dbghelpPath, EsMaxPath, &pathEnd)) {
		size_t length = EsMaxPath - wcslen(dbghelpPath) - 2;
		_snwprintf(pathEnd, length, L"\\%s", UNICODE_DBGHELPDLL_NAME);
		dbghelpPath[EsMaxPath - 1] = L'\0';
		return 0;
	}
	return 1;
}

#endif /* !defined(WIN32_IBMC) */
