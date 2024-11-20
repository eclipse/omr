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
#if !defined(OS_MEMORY_MAPPED_CACHE_HEADER_HPP_INCLUDED)
#define OS_MEMORY_MAPPED_CACHE_HEADER_HPP_INCLUDED

#include "sharedconsts.h"

#include "OSCacheContiguousRegion.hpp"
#include "OSCacheRegionInitializer.hpp"
#include "OSCacheRegionSerializer.hpp"

#include "CacheHeaderMappingImpl.hpp"
#include "OSMemoryMappedCacheHeaderMapping.hpp"

template <class>
class SOMCacheStats;

class OSMemoryMappedCacheHeader : virtual public OSCacheContiguousRegion
{
protected:
    UDATA _numLocks;
    CacheHeaderMappingImpl<OSMemoryMappedCacheHeader>* _mapping;

public:
    OSMemoryMappedCacheHeader(UDATA numLocks, CacheHeaderMappingImpl<OSMemoryMappedCacheHeader>* mapping = NULL)
        : OSCacheContiguousRegion(NULL, 0, false)
        , _numLocks(numLocks)
        , _mapping(mapping)
    {}

    virtual void refresh(OMRPortLibrary* library);
    virtual void create(OMRPortLibrary* library);

    virtual void serialize(OSCacheRegionSerializer* serializer);
    virtual void initialize(OSCacheRegionInitializer* initializer);

    virtual OSMemoryMappedCacheHeaderMapping* baseMapping() {
        return _mapping->baseMapping();
    }

protected:
    virtual void refresh(OMRPortLibrary* library, OSMemoryMappedCacheHeaderMapping* mapping);

public:
    friend class OSMemoryMappedCacheConfig;
    friend class OSMemoryMappedCacheCreatingContext;

    typedef class OSMemoryMappedCacheConfig config_type;
};
#endif
