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

#if !defined(OS_CACHE_UTILS_HPP_INCLUDED)
#define OS_CACHE_UTILS_HPP_INCLUDED

#include "OSCacheConfigOptions.hpp"
#include "OSCacheImpl.hpp"

#include "omr.h"
#include "ut_omrshr.h"

class OSCacheUtils
{
public:
  static IDATA getCacheDirName(OMRPortLibrary* portLibrary,
                               const char* ctrlDirName,
                               char* buffer,
                               UDATA bufferSize,
                               OSCacheConfigOptions* configOptions);

  static IDATA createCacheDir(OMRPortLibrary* portLibrary,
                              char* cacheDirName,
                              UDATA cacheDirPermissions,
                              bool cleanMemorySegments);
    
  static IDATA getCachePathName(OMRPortLibrary* portLibrary,
                                const char* cacheDirName,
                                char* buffer,
                                UDATA bufferSize,
                                const char* cacheName);
    
  static UDATA statCache(OMRPortLibrary* portLibrary,
                         const char* cacheDirName,
                         const char* cacheName,
                         bool displayNotFoundMsg);
    
  static SH_CacheFileAccess checkCacheFileAccess(OMRPortLibrary *portLibrary,
                                                 UDATA fileHandle,
                                                 OSCacheConfigOptions* configOptions,
                                                 LastErrorInfo *lastErrorInfo);
};
#endif
