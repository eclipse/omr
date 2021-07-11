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

#if !defined(OS_SHARED_MEMORY_CACHE_POLICIES_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_POLICIES_HPP_INCLUDED

#include "omr.h"

#include "OSCache.hpp"
#include "OSCacheConfigOptions.hpp"

class OSSharedMemoryCache;

class OSSharedMemoryCachePolicies
{
protected:
    OSSharedMemoryCache* _cache;

public:
    OSSharedMemoryCachePolicies(OSSharedMemoryCache* cache)
        : _cache(cache)
    {}

#if !defined(WIN32)
    virtual IDATA openSharedMemory(const char* fileName,
                                   U_32 permissions,
                                   LastErrorInfo *lastErrorInfo);
    virtual IDATA openSharedSemaphore(LastErrorInfo* lastErrorInfo);
    virtual IDATA destroySharedSemaphore();
    virtual IDATA destroySharedMemory();

    virtual I_32 getControlFilePermissions(char *cacheDirName,
                                           char *filename,
                                           bool& isNotReadable,
                                           bool& isReadOnly);

    virtual I_32 verifySharedSemaphoreGroupAccess(LastErrorInfo* lastErrorInfo);
    virtual I_32 verifySharedMemoryGroupAccess(LastErrorInfo* lastErrorInfo);
#endif

    virtual void cleanupSystemResources();

    virtual SH_SysvSemAccess checkSharedSemaphoreAccess(LastErrorInfo* lastErrorInfo);
    virtual SH_SysvShmAccess checkSharedMemoryAccess(LastErrorInfo* lastErrorInfo);

    virtual IDATA openSharedMemoryWrapper(const char *cacheName, LastErrorInfo *lastErrorInfo);
};
#endif
