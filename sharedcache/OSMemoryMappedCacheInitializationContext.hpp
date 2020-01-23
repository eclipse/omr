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
#if !defined(OS_MEMORY_MAPPED_INIT_CONTEXT_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_INIT_CONTEXT_HPP_INCLUDED

#include "sharedconsts.h"

#include "OSCacheConfigOptions.hpp"

class OSMemoryMappedCache;

class OSMemoryMappedCacheInitializationContext
{
protected:
    OSMemoryMappedCache* _cache;
    bool _startupCompleted;

public:
    OSMemoryMappedCacheInitializationContext(OSMemoryMappedCache* cache)
        : _cache(cache)
        , _startupCompleted(false)
    {}

    virtual bool startup(IDATA& errorCode) = 0;
    virtual bool attach(IDATA& errorCode) = 0;

    virtual bool creatingNewCache() = 0;
    virtual bool initAttach(void* blockAddress, IDATA& rc) = 0;

    bool startupCompleted() const
    {
        return _startupCompleted;
    }
};
#endif
