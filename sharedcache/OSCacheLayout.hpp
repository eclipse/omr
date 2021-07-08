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

#if !defined(OSCACHE_LAYOUT_HPP_INCLUDED)
#define OSCACHE_LAYOUT_HPP_INCLUDED

#include "OMROSCache.hpp"
#include "OSCacheRegion.hpp"
#include "infra/vector.hpp"
#include "compile/Compilation.hpp"

#include "omr.h"

class OSCacheLayout
{
protected:
    UDATA _osPageSize;
    TR::vector<OSCacheRegion*> _regions;

public:

    /** 
     * @brief Constructor with page size
     * @param[in] osPageSize Cache Page Size
     */
    OSCacheLayout(UDATA osPageSize)
        : _osPageSize(osPageSize),
          _regions(getTypedAllocator<OSCacheRegion*>(TR::comp()->allocator()))
          {}

    /** 
     * @brief Aligns the OSCacheRegions to Page Boundaries
     * @return void
     */
    virtual void omr_alignRegionsToPageBoundaries();

    /** 
     * @brief Notifies the owning OSCacheLayout about the region size changes
     * @return bool Returns true on success.
     */
    virtual bool omr_notifyRegionSizeAdjustment(OSCacheRegion&) = 0;

    /** 
     * @brief Return the region mentioned in parameters
     * @param[in] i Region Number
     * @return OSCacheRegion* Returns OSCacheRegion pointer of the region.
     */
    virtual OSCacheRegion* operator [](uint64_t i)
    {
        return _regions[i];
    }

    /** 
     * @brief Return the number of regions
     * @return int
     */
    virtual int omr_numberOfRegions() const
    {
        return _regions.size();
    }

protected:
    /** 
     * @brief Initialize the cache block.
     * @return void
     */
    virtual void omr_init(void* blockAddress, uintptr_t size) = 0;

    /** 
     * @brief Add a region to the cache.
     * @return void
     */
    virtual void omr_addRegion(OSCacheRegion* region)
    {
        _regions.push_back(region);
    }
};

#endif /* !defined(OSCACHE_LAYOUT_HPP_INCLUDED) */
