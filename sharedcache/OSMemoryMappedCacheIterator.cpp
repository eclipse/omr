/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

#include "OSMemoryMappedCacheIterator.hpp"

#include "omr.h"

/**
 * Find the first cache file in a given cacheDir
 * Follows the format of omrfile_findfirst
 *
 * @param [in] portLibrary  A port library
 * @param [in] findHandle  The handle of the last file found
 * @param [in] cacheType  The type of cache file
 * @param [out] resultbuf  The name of the file found
 *
 * @return A handle to the cache file found or -1 if the cache file doesn't exist
 */
UDATA
OSMemoryMappedCacheIterator::findFirst(OMRPortLibrary *portLibrary)
{
    UDATA findHandle = (UDATA) -1;
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    Trc_SHR_OSC_File_findfirst_Entry(_cacheDir);

    findHandle = omrfile_findfirst(_cacheDir, _resultBuf);
    
    if ((UDATA)-1 == findHandle) {
        Trc_SHR_OSC_File_findfirst_NoFileFound(_cacheDir);
        return (UDATA)-1;
    }

    while (!isCacheFileName(OMRPORTLIB, _resultBuf)) {
        if (-1 == omrfile_findnext(findHandle, _resultBuf)) {
            omrfile_findclose(findHandle);
            Trc_SHR_OSC_File_findfirst_NoSharedCacheFileFound(_cacheDir);
            return (UDATA)-1;
        }
    }

    Trc_SHR_OSC_File_findfirst_Exit(findHandle);
    return findHandle;
}

I_32
OSMemoryMappedCacheIterator::findNext(OMRPortLibrary *portLibrary, UDATA findHandle)
{
    I_32 rc = -1;
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    Trc_SHR_OSC_File_findnext_Entry();

    do {
        rc = omrfile_findnext(findHandle, _resultBuf);
    } while ((-1 != rc) && (!isCacheFileName(OMRPORTLIB, _resultBuf)));

    Trc_SHR_OSC_File_findnext_Exit();
    return rc;
}

void
OSMemoryMappedCacheIterator::findClose(OMRPortLibrary *portLibrary, UDATA findHandle)
{
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    Trc_SHR_OSC_File_findclose_Entry();
    omrfile_findclose(findHandle);
    Trc_SHR_OSC_File_findclose_Exit();
}
