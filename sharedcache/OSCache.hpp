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

#if !defined(OSCACHE_HPP_INCLUDED)
#define OSCACHE_HPP_INCLUDED

#include "OSCacheConfigOptions.hpp"
#include "OSCacheRegion.hpp"
#include "OSCacheRegionInitializer.hpp"
#include "OSCacheRegionSerializer.hpp"

#include "omr.h"
#include "sharedconsts.h"

class LastErrorInfo
{
public:
    I_32 _lastErrorCode;
    const char* _lastErrorMsg;

    LastErrorInfo()
        : _lastErrorCode(0)
        , _lastErrorMsg(NULL)
    {}

    void populate(OMRPortLibrary* library) {
        OMRPORT_ACCESS_FROM_OMRPORT(library);

        _lastErrorCode = omrerror_last_error_number();
        _lastErrorMsg = omrerror_last_error_message();
    }
};

class OSCache
{
protected:
    IDATA _corruptionCode;
    UDATA _corruptValue;

    OSCacheConfigOptions* _configOptions;

public:
    OSCache(OSCacheConfigOptions* configOptions)
        : _configOptions(configOptions)
    {}

    virtual ~OSCache() {}

    virtual U_32 getTotalSize() = 0;
    virtual void errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo) = 0;

    virtual SH_CacheAccess isCacheAccessible(void) const
    {
        return OMRSH_CACHE_ACCESS_ALLOWED;
    }

    virtual void getCorruptionContext(IDATA *corruptionCode, UDATA *corruptValue)
    {
        if (NULL != corruptionCode) {
            *corruptionCode = _corruptionCode;
        }

        if (NULL != corruptValue) {
            *corruptValue = _corruptValue;
        }
    }

    virtual void setCorruptionContext(IDATA corruptionCode, UDATA corruptValue)
    {
        _corruptionCode = corruptionCode;
        _corruptValue = corruptValue;
    }

    virtual OSCacheRegionSerializer* constructSerializer() = 0;
    virtual OSCacheRegionInitializer* constructInitializer() = 0;

    virtual void* attach() = 0;
    virtual bool startup(const char* cacheName, const char* ctrlDirName) = 0;

    virtual void cleanup() = 0;
    virtual IDATA destroy(bool suppressVerbose, bool isReset = false) = 0;
    virtual IDATA getError() = 0;
    virtual void runExitProcedure() = 0;

    virtual IDATA getLockCapabilities() = 0;
    virtual IDATA setRegionPermissions(OSCacheRegion* region) = 0;
    virtual UDATA getPermissionsRegionGranularity() = 0;
};
#endif
