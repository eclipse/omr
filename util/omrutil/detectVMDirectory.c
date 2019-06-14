/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

#include "omrcfg.h"

#if (HOST_OS == OMR_WINDOWS)
#include <windows.h>
#endif /* (HOST_OS == OMR_WINDOWS) */

#include "omrutil.h"

#if (HOST_OS == OMR_WINDOWS)
omr_error_t
detectVMDirectory(wchar_t *vmDirectory, size_t vmDirectoryLength, wchar_t **vmDirectoryEnd)
{
	omr_error_t rc = OMR_ERROR_NOT_AVAILABLE;
	LPCSTR moduleName = NULL;
	HANDLE moduleHandle = NULL;
	DWORD pathLength = 0;

	if (OMR_ERROR_NONE == OMR_Glue_GetVMDirectoryToken((void *)&moduleName)) {
		moduleHandle = GetModuleHandle(moduleName);
		if (NULL != moduleHandle) {
			pathLength = GetModuleFileNameW(moduleHandle, vmDirectory, (DWORD)vmDirectoryLength);
			if ((0 != pathLength) && (pathLength < (DWORD)vmDirectoryLength)) {
				/* remove the module name from the path */
				*vmDirectoryEnd = wcsrchr(vmDirectory, '\\');
				if (NULL != *vmDirectoryEnd) {
					**vmDirectoryEnd = L'\0';
				}
				rc = OMR_ERROR_NONE;
			}
		}
	}
	return rc;
}
#endif /* (HOST_OS == OMR_WINDOWS) */
