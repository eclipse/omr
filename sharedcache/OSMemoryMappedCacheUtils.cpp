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

#include "OSCacheConfigOptions.hpp"
#include "OSMemoryMappedCache.hpp"
#include "OSMemoryMappedCacheUtils.hpp"
#include "OSMemoryMappedCacheConfig.hpp"

/**
 * This method checks whether the group access of the shared cache file is successfully set when a new cache/snapshot is created with "groupAccess" suboption
 *
 * @param [in] portLibrary The port library
 * @param[in] fileHandle The handle of the shared cache file
 * @param[in] lastErrorInfo	Pointer to store last portable error code and/or message
 *
 * @return -1 Failed to get the stats of the file.
 * 			0 Group access is not set.
 * 			1 Group access is set.
 */
I_32
OSMemoryMappedCacheUtils::verifyCacheFileGroupAccess(OMRPortLibrary *portLibrary,
                                                     IDATA fileHandle,
                                                     LastErrorInfo *lastErrorInfo)
{
    I_32 rc = 1;
#if !defined(WIN32)
    struct J9FileStat statBuf;
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    memset(&statBuf, 0, sizeof(statBuf));
    if (0 == omrfile_fstat(fileHandle, &statBuf)) {
        if ((1 != statBuf.perm.isGroupWriteable)
            || (1 != statBuf.perm.isGroupReadable)
            ) {
            rc = 0;
        }
    } else {
        if (NULL != lastErrorInfo) {
            lastErrorInfo->populate(OMRPORTLIB);
        }

        rc = -1;
    }

#endif /* !defined(WIN32) */
    return rc;
}
