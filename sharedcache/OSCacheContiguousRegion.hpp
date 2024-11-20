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
#if !defined(OSCACHE_CONTIGUOUS_REGION_HPP_INCLUDED)
#define OSCACHE_CONTIGUOUS_REGION_HPP_INCLUDED

#include "CacheMemoryProtectionOptions.hpp"
#include "OSCacheLayout.hpp"
#include "OSCacheRegion.hpp"

#include "env/TRMemory.hpp"
#include "omrport.h"

class OSCacheContiguousRegion : public OSCacheRegion
{
protected:
    void* _regionStart;
    UDATA _regionSize;
    bool _pageBoundaryAligned;

    CacheMemoryProtectionOptions* _protectionOptions;

public:
    TR_ALLOC(TR_Memory::SharedCacheRegion)

    OSCacheContiguousRegion(OSCacheLayout* layout, int regionID, bool pageBoundaryAligned);

    virtual void* regionStartAddress() const override;
    virtual void* regionEnd();
    virtual UDATA regionSize() const override;

    virtual bool adjustRegionStartAndSize(void* blockAddress, uintptr_t size) override;
    virtual bool alignToPageBoundary(UDATA osPageSize) override;

    virtual UDATA renderToMemoryProtectionFlags() override;

    virtual bool isAddressInRegion(void* itemAddress) override;
    virtual bool isBlockInRegion(void* itemAddress, UDATA itemSize) override;

    virtual IDATA setPermissions(OSCacheMemoryProtector* protector) override;

    virtual U_32 computeCRC(U_32 seed, U_32 stepSize) override;

    virtual void serialize(OSCacheRegionSerializer* serializer) override
    {
        serializer->serialize(this);
    }

    virtual void initialize(OSCacheRegionInitializer* initializer) override
    {
        initializer->initialize(this);
    }
};
#endif
