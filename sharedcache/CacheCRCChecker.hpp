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

#if !defined(CACHE_CRC_CHECKER_HPP_INCLUDED)
#define CACHE_CRC_CHECKER_HPP_INCLUDED

#include "OSCacheRegionFocus.hpp"
#include "OSCacheConfigOptions.hpp"

class CacheCRCChecker
{
protected:
    OSCacheRegionFocus<UDATA> _crcFocus;
    U_32 _maxCRCSamples;

public:
    CacheCRCChecker(OSCacheRegion* region, UDATA* crcField, U_32 maxCRCSamples)
        : _crcFocus(region, crcField)
        , _maxCRCSamples(maxCRCSamples)
    {}

    virtual U_32 computeRegionCRC();
    virtual void updateRegionCRC();
    virtual bool isRegionCRCValid();
};
#endif
