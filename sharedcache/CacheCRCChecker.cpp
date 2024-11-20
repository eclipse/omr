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

#include "CacheCRCChecker.hpp"

#include "omrutil.h"

U_32
CacheCRCChecker::computeRegionCRC()
{
    /*
     * 1535=1.5k - 1.  Chosen so that we aren't stepping on exact power of two boundaries through
     * the cache and yet we use a decent number of samples through the cache.
     * For a 16Meg cache this will cause us to take 10000 samples.
     * For a 100Meg cache this will cause us to take 68000 samples.
     */

    U_32 stepSize = 1535;
    U_32 regionSize = _crcFocus.region()->regionSize();

    if ((regionSize / stepSize) > _maxCRCSamples) {
        stepSize = regionSize / _maxCRCSamples;
    }

    U_32 seed = omrcrc32(0, NULL, 0);

    return _crcFocus.region()->computeCRC(seed, stepSize);
}

void
CacheCRCChecker::updateRegionCRC()
{
    U_32 value = 0;

    value = computeRegionCRC();

    if (value != 0) {
        *_crcFocus.focus() = value;
    }
}

bool
CacheCRCChecker::isRegionCRCValid()
{
    U_32 value = computeRegionCRC();
    return *_crcFocus.focus() == value;
}
