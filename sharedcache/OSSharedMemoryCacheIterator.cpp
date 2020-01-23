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

#include "OSSharedMemoryCacheIterator.hpp"

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
OSSharedMemoryCacheIterator::findFirst(OMRPortLibrary *portLibrary)
{
    UDATA rc;
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    Trc_SHR_OSC_Sysv_findfirst_Entry();

    rc = omrshmem_findfirst(_cacheDir, _resultBuf);

    Trc_SHR_OSC_Sysv_findfirst_Exit(rc);
    return rc;
}

I_32
OSSharedMemoryCacheIterator::findNext(OMRPortLibrary *portLibrary, UDATA findHandle)
{
    I_32 rc;
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    Trc_SHR_OSC_Sysv_findnext_Entry(findHandle);

    rc = omrshmem_findnext(findHandle, _resultBuf);

    Trc_SHR_OSC_Sysv_findnext_Exit(rc);
    return rc;
}

void
OSSharedMemoryCacheIterator::findClose(OMRPortLibrary *portLibrary, UDATA findHandle)
{
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    Trc_SHR_OSC_File_findclose_Entry();
    omrfile_findclose(findHandle);
    Trc_SHR_OSC_File_findclose_Exit();
}
