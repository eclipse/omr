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

#include "omrport.h"

#include "OSMemoryMappedCacheHeader.hpp"

void
OSMemoryMappedCacheHeader::refresh(OMRPortLibrary* library)
{
    *_mapping = regionStartAddress();
  
    OSMemoryMappedCacheHeaderMapping* mapping = _mapping->baseMapping();
    refresh(library, mapping);
}

void
OSMemoryMappedCacheHeader::refresh(OMRPortLibrary* library, OSMemoryMappedCacheHeaderMapping* mapping)
{
    OMRPORT_ACCESS_FROM_OMRPORT(library);

    mapping->_createTime = omrtime_current_time_millis();
    mapping->_lastAttachedTime = omrtime_current_time_millis();
    mapping->_lastDetachedTime = omrtime_current_time_millis();
}

void
OSMemoryMappedCacheHeader::create(OMRPortLibrary* library)
{
    *_mapping = regionStartAddress();

    OSMemoryMappedCacheHeaderMapping* mapping = _mapping->baseMapping();

    memset(mapping, 0, mapping->size());
  
    for(U_32 i = 0; i < _numLocks; ++i) {
        mapping->_dataLocks[i] = 0;
    }

    strncpy(mapping->_eyecatcher, OMRSH_OSCACHE_MMAP_EYECATCHER, OMRSH_OSCACHE_MMAP_EYECATCHER_LENGTH);
    refresh(library, mapping);
}

void
OSMemoryMappedCacheHeader::serialize(OSCacheRegionSerializer* serializer) {
    serializer->serialize(this);
}

void
OSMemoryMappedCacheHeader::initialize(OSCacheRegionInitializer* initializer) {
    initializer->initialize(this);
}
