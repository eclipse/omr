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

#if !defined(OS_CACHE_REGION_BUMP_FOCUS_HPP_INCLUDED)
#define OS_CACHE_REGION_BUMP_FOCUS_HPP_INCLUDED

#include "OSCacheContiguousRegion.hpp"
#include "OSCacheRegionFocus.hpp"

#include "omrport.h"

template <typename T>
class OSCacheRegionBumpFocus : public OSCacheRegionFocus<T>
{
public:
    OSCacheRegionBumpFocus(OSCacheContiguousRegion* region, T* focus)
        : OSCacheRegionFocus<T>(region, focus)
    {}

    inline operator T*()
    {
        return this->_focus;
    }

    inline operator const T*() const
    {
        return this->_focus;
    }

    OSCacheRegionBumpFocus& operator +=(UDATA bump)
    {
        if (!blockInRange(bump)) {
            return *this;
        }

        this->_focus = (T*) ((U_8*) (this->_focus) + bump);
        
        return *this;
    }

    T* operator ++(int)
    {
        if (!blockInRange(sizeof(T))) {
            return NULL;
        }

        T* focus = this->_focus;
        this->_focus++;
        
        return focus;
    }

    inline bool operator <(const OSCacheRegionBumpFocus<T>& rhs) const
    {
        return this->_region == rhs._region && this->_focus < rhs._focus;
    }

    inline bool operator ==(const OSCacheRegionBumpFocus<T>& rhs) const
    {
        return this->_region == rhs._region && this->_focus == rhs._focus;
    }

    virtual OSCacheRegionBumpFocus<T>& operator =(T* newFocus)
    {
        this->_focus = newFocus;
        return *this;
    }

protected:
    inline bool blockInRange(UDATA size) const
    {
        return this->_region->isBlockInRegion((void*) this->_focus, size);
    }
};
#endif
