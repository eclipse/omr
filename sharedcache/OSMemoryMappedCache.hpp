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

#if !defined(OS_MEMORY_MAPPED_CACHE_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_CACHE_HPP_INCLUDED

#include "OSCacheRegion.hpp"
#include "OSCacheImpl.hpp"
#include "OSCacheLayout.hpp"
#include "OSMemoryMappedCacheConfig.hpp"
#include "OSMemoryMappedCacheInitializationContext.hpp"
#include "OSMemoryMappedCacheInitializer.hpp"
#include "OSMemoryMappedCacheSerializer.hpp"
#include "OSMemoryMappedCache.hpp"

#include "omr.h"
#include "omrport.h"

class OSMemoryMappedCacheConfig;
class OSMemoryMappedCacheIterator;
class OSMemoryMappedCacheStats;

class OSMemoryMappedCache : public OSCacheImpl
{
protected:    
    OSMemoryMappedCacheInitializationContext* _initContext;
    OSMemoryMappedCacheConfig* _config;

    J9MmapHandle *_mapFileHandle;

public:
    OSMemoryMappedCache(OMRPortLibrary* library,
                        char* cacheName,
                        char* ctrlDirName,
                        IDATA numLocks,
                        OSMemoryMappedCacheConfig* config,
                        OSCacheConfigOptions* configOptions);

    OSMemoryMappedCache(const OSMemoryMappedCache& cache,
                        char* cacheName,
                        char* ctrlDirName);
  
    virtual ~OSMemoryMappedCache() {}

    bool startup(const char* cacheName, const char* ctrlDirName);
    IDATA destroy(bool suppressVerbose, bool isReset);
    bool checkTime(U_64 moduleTime);

    virtual void* attach();
    virtual void detach();

    virtual void initialize();
    virtual void finalise();
    virtual void cleanup();

    virtual IDATA getError();
    
protected:
    virtual IDATA verifyCacheHeader();
  
    bool setCacheLength(LastErrorInfo*);

    bool openCacheFile(LastErrorInfo*);
    bool closeCacheFile();

    IDATA internalAttach();
    void internalDetach();

    IDATA getLockCapabilities();

    virtual UDATA getPermissionsRegionGranularity();

    virtual void setError(IDATA errorCode);
    virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo);

    virtual void runExitProcedure();
    virtual void handleCacheHeaderCorruption(IDATA headerRc);

#if defined(OMRSH_MSYNC_SUPPORT)
    virtual IDATA syncUpdates(void* start, UDATA length, U_32 flags);
#endif

    virtual bool deleteCacheFile(LastErrorInfo* lastErrorInfo);

    virtual OSCacheMemoryProtector* constructMemoryProtector();

    virtual OSCacheRegionSerializer* constructSerializer();
    virtual OSCacheRegionInitializer* constructInitializer();

    friend class OSMemoryMappedCacheAttachingContext;
    friend class OSMemoryMappedCacheCreatingContext;
    friend class OSMemoryMappedCacheStats;

    typedef OSMemoryMappedCacheHeader header_type;
    typedef OSMemoryMappedCacheConfig config_type;
    typedef OSMemoryMappedCacheIterator iterator_type;
    typedef OSMemoryMappedCacheStats stats_type;
};
#endif
