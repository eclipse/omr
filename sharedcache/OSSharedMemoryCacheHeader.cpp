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

#include "OSCacheConfig.hpp"
#include "OSSharedMemoryCacheConfig.hpp"

void
OSSharedMemoryCacheHeader::create(OMRPortLibrary* library, bool inDefaultControlDir)
{
    *_mapping = regionStartAddress();

    OSSharedMemoryCacheHeaderMapping* mapping = _mapping->baseMapping();
    memset(mapping, 0, mapping->size());

    strncpy(mapping->_eyecatcher,
            OMRSH_OSCACHE_SYSV_EYECATCHER,
            OMRSH_OSCACHE_SYSV_EYECATCHER_LENGTH);

    refresh(library, mapping, inDefaultControlDir);
}

void
OSSharedMemoryCacheHeader::refresh(OMRPortLibrary* library,
                                   OSSharedMemoryCacheHeaderMapping* mapping,
                                   bool inDefaultControlDir)
{
    OMRPORT_ACCESS_FROM_OMRPORT(library);

    mapping->_createTime = omrtime_current_time_millis();
    mapping->_inDefaultControlDir = inDefaultControlDir;
}

void
OSSharedMemoryCacheHeader::refresh(OMRPortLibrary* library, bool inDefaultControlDir)
{
    *_mapping = regionStartAddress();
    refresh(library, _mapping->baseMapping(), inDefaultControlDir);
}

void
OSSharedMemoryCacheHeader::serialize(OSCacheRegionSerializer* serializer)
{
    return serializer->serialize(this);
}

void
OSSharedMemoryCacheHeader::initialize(OSCacheRegionInitializer* initializer)
{
    return initializer->initialize(this);
}
