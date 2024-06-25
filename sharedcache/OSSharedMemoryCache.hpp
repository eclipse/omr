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

#if !defined(OS_SHARED_MEMORY_CACHE_HPP_INCLUDED)
#define OS_SHARED_MEMORY_CACHE_HPP_INCLUDED

#include "OSCacheImpl.hpp"
#include "OSCacheLayout.hpp"
#include "OSSharedMemoryCacheIterator.hpp"
#include "OSSharedMemoryCacheConfig.hpp"
#include "OSSharedMemoryCacheInitializer.hpp"
#include "OSSharedMemoryCacheSerializer.hpp"
#include "OSSharedMemoryCachePolicies.hpp"
#include "OSSharedMemoryCacheSnapshot.hpp"
#include "OSCacheContiguousRegion.hpp"

#include "omr.h"
#include "omrport.h"

#define OS_SHARED_MEMORY_CACHE_RESTART 4
#define OS_SHARED_MEMORY_CACHE_OPENED 3
#define OS_SHARED_MEMORY_CACHE_CREATED 2
#define OS_SHARED_MEMORY_CACHE_EXIST   1
#define OS_SHARED_MEMORY_CACHE_NOT_EXIST 0
#define OS_SHARED_MEMORY_CACHE_FAILURE -1

#define OS_SHARED_MEMORY_CACHE_SUCCESS 0

#define SEM_HEADERLOCK 0

#define OMRSH_OSCACHE_RETRYMAX 30

class OSSharedMemoryCacheConfig;
class OSSharedMemoryCacheStats;

class OSSharedMemoryCache : public OSCacheImpl
{
protected:
    OSSharedMemoryCacheConfig* _config;
    OMRControlFileStatus _controlFileStatus;

    IDATA _attachCount;

    char* _shmFileName;
    char* _semFileName;

    bool _startupCompleted;
    bool _openSharedMemory;

    OSSharedMemoryCachePolicies* _policies;

public:
    OSSharedMemoryCache(OMRPortLibrary* library,
                        char* cacheName,
                        char* cacheDirName,
                        IDATA numLocks,
                        OSSharedMemoryCacheConfig* config,
                        OSCacheConfigOptions* configOptions);

    OSSharedMemoryCache(const OSSharedMemoryCache& cache,
                        char* cacheName,
                        char* ctrlDirName);

    virtual ~OSSharedMemoryCache() {}

    virtual bool startup(const char* cacheName, const char* ctrlDirName) override;
    IDATA destroy(bool suppressVerbose, bool isReset = false) override;

    virtual void initialize();
    virtual void cleanup() override;

    SH_CacheAccess isCacheAccessible() const override;
    UDATA isCacheActive() const;

protected:
    virtual void writeSemaphoreAndSharedMemoryFileNames()
    {
        sprintf(_semFileName, "%s%s%s", _cacheName, OMRSH_SEMAPHORE_ID, OMRSH_MEMORY_ID);
        _shmFileName = _cacheName;
    }

    IDATA installLayout(LastErrorInfo* lastErrorInfo);

    void setError(IDATA ec);
    IDATA getError() override;

    void runExitProcedure() override {}

    IDATA getLockCapabilities() override
    {
        return OMROSCACHE_DATA_WRITE_LOCK;
    }

    virtual void finalise() override
    {
    }

    IDATA openCache(const char* cacheName);

    virtual OSSharedMemoryCachePolicies* constructSharedMemoryPolicies();

    virtual OSCacheMemoryProtector* constructMemoryProtector() override;

    virtual OSCacheRegionSerializer* constructSerializer() override;
    virtual OSCacheRegionInitializer* constructInitializer() override;

    virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo) override;
    virtual void printErrorMessage(LastErrorInfo* lastErrorInfo);

    virtual UDATA getPermissionsRegionGranularity() override;
    virtual IDATA verifyCacheHeader() override;

    IDATA detachRegion();

    void* attach() override;
    IDATA detach();

    friend class OSSharedMemoryCachePolicies;
    friend class OSSharedMemoryCacheSnapshot;
    friend class OSSharedMemoryCacheStats;

    typedef OSSharedMemoryCacheHeader header_type;
    typedef OSSharedMemoryCacheConfig config_type;
    typedef OSSharedMemoryCacheIterator iterator_type;
    typedef OSSharedMemoryCacheStats stats_type;
};
#endif
