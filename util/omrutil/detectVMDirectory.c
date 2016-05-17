/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
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
 ******************************************************************************/

#include "omrcfg.h"

#if defined(WIN32)
#include <windows.h>
#endif /* defined(WIN32) */

#include "omrutil.h"

#if defined(WIN32)
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
#endif /* defined(WIN32) */
