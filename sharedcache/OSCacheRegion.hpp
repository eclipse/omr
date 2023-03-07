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
#if !defined(OSCACHE_REGION_HPP_INCLUDED)
#define OSCACHE_REGION_HPP_INCLUDED

#include "omr.h"

#include "OSCacheMemoryProtector.hpp"
#include "OSCacheRegionInitializer.hpp"
#include "OSCacheRegionSerializer.hpp"

#define ROUND_UP_TO(granularity, number) ( (((number) % (granularity)) ? ((number) + (granularity) - ((number) % (granularity))) : (number)))
#define ROUND_DOWN_TO(granularity, number) ( (((number) % (granularity)) ? ((number) - ((number) % (granularity))) : (number)))

class OSCacheImpl;
class OSCacheLayout;
class OSCacheRegionMemoryProtector;

/**
 * @brief Class to manage OSCacheRegions 
 */
class OSCacheRegion {
protected:
    OSCacheLayout* _layout;
    int _regionID;

public:
    OSCacheRegion(OSCacheLayout* layout, int regionID)
        : _layout(layout)
        , _regionID(regionID)
    {}

    /**
    * @brief Find the start address of the region.
    * @return void pointer to the start of the region
    */
    virtual void* omr_regionStartAddress() const = 0;

    /**
    * @brief Finds if the described address of data fall within the region's memory.
    * @param[in] itemAddress Address of Data Item
    * @return bool
    */
    virtual bool omr_isAddressInRegion(void* itemAddress) = 0;

    /**
    * @brief Finds if described block of data fall within the region's memory or not.
    * @return bool
    */
    virtual bool omr_isBlockInRegion(void* itemAddress, UDATA itemSize) = 0;

    /**
    * @brief render the Region's memory protection settings as flags that can
    *        be used by the underlying components of the OMRPortLibrary, as
    *        anticipated by the OSCacheRegion subclass
    * @return UDATA
    */
    virtual UDATA omr_renderToMemoryProtectionFlags() = 0;

    /**
    * @brief Aligns CacheRegion to Page Boundary
    * @note This is not a predicate, but it may modify state. If the 
    *       subclass isn't concerned with aligning to page boundaries, it can
    *       just as well be a no-op. returns true if re-alignment was 
    *       attempted, and succeeded.
    * @return UDATA
    */
    virtual bool omr_alignToPageBoundary(UDATA osPageSize) = 0;

    /* visitor acceptor functions. */
    /** 
     * @brief Serialize the contents of the OSCacheRegion to the cache
     * param[in] serializer OSCacheRegionSerializer pointer
     * @return void
     */
    virtual void omr_serialize(OSCacheRegionSerializer* serializer) = 0;

    /** 
     * @brief Initailize the OSCacheRegion
     * param[in] initializer OSCacheRegionInitializer pointer
     * @return void
     */
    virtual void omr_initialize(OSCacheRegionInitializer* initializer) = 0;

    /** 
     * @brief Tweaks the the start and size of the region.
     * param[in] blockAddress Desired new start address
     * param[in] size Desired size of new region
     * @return bool
     */
    virtual bool omr_adjustRegionStartAndSize(void* blockAddress, uintptr_t size) = 0;

    /** 
     * @brief Calculates the size of the region.
     * @return UDATA Region Size
     */
    virtual UDATA omr_regionSize() const = 0;

    /** 
     * @brief Return the region ID
     * @return int
     */
    virtual int omr_regionID()
    {
        return _regionID;
    }

    /** 
     * @brief Add memory protections for OSCacheRegions
     * @param[in] protector OSCacheMemoryProtector pointer
     * @return 0 if the operations has been successful, -1 if an error has occured
     */
    virtual IDATA omr_setPermissions(OSCacheMemoryProtector* protector) = 0;

    /** 
     * @brief Calculate the CRC for a OSCacheRegions.
     * @param[in] seed Seed crc value
     * @param[in] stepSize The supplied 'step' is the gap between sampled bytes.   
     * @return U_32 seed crc value
     */
    virtual U_32 omr_computeCRC(U_32 seed, U_32 stepSize) = 0;

    /** 
     * @brief Sets the OSCache Layout to layout supplied in paramters
     * @param[in] layout OSCacheLayout pointer
     * @return void
     */
    virtual void omr_setCacheLayout(OSCacheLayout* layout)
    {
        _layout = layout;
    }
};
#endif /* !defined(OSCACHE_REGION_HPP_INCLUDED) */
