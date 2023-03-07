/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
#if !defined(OS_CACHE_REGION_SERIALIZER_HPP_INCLUDED)
#define OS_CACHE_REGION_SERIALIZER_HPP_INCLUDED

class OSCacheContiguousRegion;
class OSMemoryMappedCacheHeader;
class OSSharedMemoryCacheHeader;

/**
 * @brief Class introduces the serilization functionality  
 */
class OSCacheRegionSerializer
{
public:
    virtual ~OSCacheRegionSerializer() {}

    /** 
     * @brief Calls to create OSMemoryMappedCacheHeader
     * param[in] OSMemoryMappedCacheHeader pointer
     * @return void
     */
    virtual void omr_serialize(OSMemoryMappedCacheHeader*) = 0;

    /** 
     * @brief Calls to create OSMemoryMappedCacheHeader
     * param[in] OSMemoryMappedCacheHeader pointer
     * @return void
     */
    virtual void omr_serialize(OSSharedMemoryCacheHeader*) = 0;

    /** 
     * @brief Serialize the contents of the OSCacheContiguousRegion to the cache
     * param[in] OSMemoryMappedCacheHeader pointer
     * @return void
     */
    virtual void omr_serialize(OSCacheContiguousRegion*) = 0;
};
#endif /* !defined(OS_CACHE_REGION_SERIALIZER_HPP_INCLUDED) */
