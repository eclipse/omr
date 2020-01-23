/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

#include "ut_omrshr.h"
#include "omr.h"
#include "sharedconsts.h"

#include "OSCacheConfigOptions.hpp"

I_32
OSCacheConfigOptions::fileMode()
{
    I_32 perm = 0;

    Trc_SHR_OSC_Mmap_getFileMode_Entry();

    if (isUserSpecifiedCacheDir()) {
        if (_openMode & OMROSCACHE_OPEN_MODE_GROUPACCESS) {
            perm = OMRSH_CACHE_FILE_MODE_USERDIR_WITH_GROUPACCESS;
        } else {
            perm = OMRSH_CACHE_FILE_MODE_USERDIR_WITHOUT_GROUPACCESS;
        }
    } else {
        if (_openMode & OMROSCACHE_OPEN_MODE_GROUPACCESS) {
            perm = OMRSH_CACHE_FILE_MODE_DEFAULTDIR_WITH_GROUPACCESS;
        } else {
            perm = OMRSH_CACHE_FILE_MODE_DEFAULTDIR_WITHOUT_GROUPACCESS;
        }
    }

    Trc_SHR_OSC_Mmap_getFileMode_Exit(_openMode, perm);
    return perm;
}
