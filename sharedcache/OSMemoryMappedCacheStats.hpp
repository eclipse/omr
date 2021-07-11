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

#if !defined(OS_MEMORY_MAPPED_CACHE_STATS_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_CACHE_STATS_HPP_INCLUDED

#include "OSCacheStats.hpp"
#include "OSMemoryMappedCache.hpp"

class OSMemoryMappedCacheStats: public OSCacheStats
{
protected:
    OSMemoryMappedCache* _cache;
    I_32 _inUse;

public:
    OSMemoryMappedCacheStats(OSMemoryMappedCache* cache)
        : _cache(cache)
        , _inUse(0)
    {}

    virtual IDATA prepareCache(const char* cacheName, const char* cacheLocation);
    virtual void shutdownCache();
    virtual void getCacheStats() = 0;
};
#endif
