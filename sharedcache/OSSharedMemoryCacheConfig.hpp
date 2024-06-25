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

#if !defined(OS_SHARED_MEMORY_CACHE_CONFIG_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_CONFIG_HPP_INCLUDED

#include "omr.h"
#include "omrport.h"
#include "pool_api.h"
#include "sharedconsts.h"

#include "OSCacheConfig.hpp"
#include "OSSharedMemoryCacheHeader.hpp"
#include "OSSharedMemoryCacheHeaderMapping.hpp"

#define OS_SHARED_MEMORY_CACHE_RESTART 4
#define OS_SHARED_MEMORY_CACHE_OPENED 3
#define OS_SHARED_MEMORY_CACHE_CREATED 2
#define OS_SHARED_MEMORY_CACHE_EXIST   1
#define OS_SHARED_MEMORY_CACHE_NOT_EXIST 0
#define OS_SHARED_MEMORY_CACHE_FAILURE -1

#define OS_SHARED_MEMORY_CACHE_SUCCESS 0

/**
 * This enum contains constants that are used to indicate the reason for not allowing access to the semaphore set.
 * It is returned by @ref SH_OSCachesysv::checkSemaphoreAccess().
 */
typedef enum SH_SysvSemAccess {
    OMRSH_SEM_ACCESS_ALLOWED 				= 0,
    OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED,
    OMRSH_SEM_ACCESS_OWNER_NOT_CREATOR,
    OMRSH_SEM_ACCESS_GROUP_ACCESS_REQUIRED,
    OMRSH_SEM_ACCESS_OTHERS_NOT_ALLOWED
} SH_SysvSemAccess;

/**
 * This enum contains constants that are used to indicate the reason for not allowing access to the shared memory.
 * It is returned by @ref SH_OSCachesysv::checkSharedMemoryAccess().
 */
typedef enum SH_SysvShmAccess {
    OMRSH_SHM_ACCESS_ALLOWED 						= 0,
    OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED,
    OMRSH_SHM_ACCESS_OWNER_NOT_CREATOR,
    OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED,
    OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED,
    OMRSH_SHM_ACCESS_OTHERS_NOT_ALLOWED
} SH_SysvShmAccess;

#define SEM_HEADERLOCK 0

#define OMRSH_OSCACHE_RETRYMAX 30

class OSSharedMemoryCache;

class OSSharedMemoryCacheConfig : public OSCacheConfig
{
protected:
    UDATA _numLocks;

    OSSharedMemoryCacheHeaderMapping* _mapping;
    OSSharedMemoryCacheHeader* _header;

    SH_SysvSemAccess _semAccess;
    SH_SysvShmAccess _shmAccess;

    omrshmem_handle* _shmhandle;
    omrshsem_handle* _semhandle;

    UDATA _totalNumSems;
    UDATA _userSemCntr;
    I_32 _semid;

public:
    OSSharedMemoryCacheConfig(UDATA numLocks);

    virtual IDATA getWriteLockID();
    virtual IDATA getReadWriteLockID();

    virtual IDATA acquireLock(OMRPortLibrary* library, UDATA lockID, LastErrorInfo* lastErrorInfo = NULL);
    virtual IDATA releaseLock(OMRPortLibrary* library, UDATA lockID);

    virtual void serializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize) = 0;
    virtual void initializeCacheLayout(OSCache* osCache, void* blockAddress, U_32 cacheSize) = 0;

    virtual void* getHeaderLocation()
    {
        return _header->regionStartAddress();
    }

    virtual UDATA getHeaderSize()
    {
        return _header->regionSize();
    }

    virtual void* getDataSectionLocation() = 0;
    virtual U_32 getDataSectionSize() = 0;

    virtual U_32* getCacheSizeFieldLocation() = 0;
    virtual U_32 getCacheSize() = 0;

    virtual U_32* getDataLengthFieldLocation() = 0;

    virtual U_64* getInitCompleteLocation() = 0;
    virtual bool setCacheInitComplete() = 0;

protected:
    virtual IDATA getNewWriteLockID();

    IDATA acquireHeaderWriteLock(OMRPortLibrary* library, const char* cacheName, LastErrorInfo* lastErrorInfo);
    IDATA releaseHeaderWriteLock(OMRPortLibrary* library, LastErrorInfo* lastErrorInfo);

    virtual void detachRegions() = 0;

    friend class OSSharedMemoryCache;
    friend class OSSharedMemoryCachePolicies;
    friend class OSSharedMemoryCacheStats;

    typedef OSSharedMemoryCacheHeader header_type;
    typedef OSSharedMemoryCache cache_type;
};
#endif
