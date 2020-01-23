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

#include "SynchronizedCacheCounter.hpp"

bool
SynchronizedCacheCounter::incrementCount(OSCacheImpl& osCache)
{
    UDATA oldNum, value;

    if (!osCache.started() || osCache.runningReadOnly()) {
        Trc_SHR_Assert_ShouldNeverHappen();
        return false;
    }

    oldNum = *_regionFocus.focus();
    value = 0;

    osCache.setRegionPermissions(_regionFocus.region());

    do {
        value = oldNum + 1;
        oldNum = VM_AtomicSupport::lockCompareExchange(_regionFocus.focus(), oldNum, value);
    } while ((UDATA)value != (oldNum + 1));

    osCache.setRegionPermissions(_regionFocus.region());

    Trc_SHR_CC_incReaderCount_Exit(value);

    return true;
}

bool
SynchronizedCacheCounter::decrementCount(OSCacheImpl& osCache)
{
    UDATA oldNum, value;

    if (!osCache.started() || osCache.runningReadOnly()) {
        Trc_SHR_Assert_ShouldNeverHappen();
        return false;
    }

    oldNum = *_regionFocus.focus();
    value = 0;

    do {
        if (0 == oldNum) {
            break;
        }

        value = oldNum - 1;
        oldNum = VM_AtomicSupport::lockCompareExchange(_regionFocus.focus(), oldNum, value);
    } while ((UDATA) value != (oldNum - 1));

    Trc_SHR_CC_decReaderCount_Exit(value);

    return true;
}

volatile UDATA
SynchronizedCacheCounter::count() const {
    return *_regionFocus.focus();
}
