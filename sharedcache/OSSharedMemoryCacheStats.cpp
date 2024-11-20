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

#include "OSCacheImpl.hpp"
#include "OSCacheUtils.hpp"
#include "OSSharedMemoryCacheUtils.hpp"
#include "OSSharedMemoryCacheStats.hpp"

#include "ut_omrshr.h"

IDATA
OSSharedMemoryCacheStats::prepareCache(const char* cacheName, const char* cacheLocation)
{
    IDATA retval = 0;
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
    char cacheDirName[OMRSH_MAXPATH];

    OSCacheConfigOptions* configOptions = _cache->_configOptions;

    OSCacheUtils::getCacheDirName(OMRPORTLIB,
                                  cacheLocation,
                                  cacheDirName,
                                  OMRSH_MAXPATH,
                                  configOptions);

    if (getCacheStatsHelper(cacheDirName) == 0)
    {
        bool attachedMem = false;

        if (configOptions->statIterate() || configOptions->statList())
        {
            bool cacheStarted = _cache->started();

            if (!cacheStarted && !_cache->startup(cacheName, cacheLocation)) {
                goto done;
            }
        }
    } else {
        retval = -1;
    }

 done:
    return retval;
}

void
OSSharedMemoryCacheStats::shutdownCache()
{
    _cache->detach();
    _cache->cleanup();
}

IDATA
OSSharedMemoryCacheStats::getCacheStatsHelper(const char* cacheDirName)
{
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    OMRPortShmemStatistic statbuf;

    UDATA statrc = 0;
    OSCacheConfigOptions* configOptions = _cache->_configOptions;

    Trc_SHR_OSC_Sysv_getCacheStatsHelper_Entry(_cache->_cacheName);

#if !defined(WIN32)
    statrc = OSSharedMemoryCacheUtils::StatSysVMemoryHelper(OMRPORTLIB,
                                                            cacheDirName,
                                                            configOptions->groupPermissions(),
                                                            _cache->_cacheName,
                                                            &statbuf);
#else
    statrc = omrshmem_stat(cacheDirName,
                           configOptions->groupPermissions(),
                           _cache->_cacheName, &statbuf);
#endif

    Trc_SHR_OSC_Sysv_getCacheStatsHelper_Exit();
    return statrc;
}
