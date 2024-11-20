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

#include "OSCacheImpl.hpp"
#include "OSSharedMemoryCacheConfig.hpp"

#include "omrport.h"
#include "shrnls.h"
#include "ut_omrshr.h"

OSSharedMemoryCacheConfig::OSSharedMemoryCacheConfig(UDATA numLocks)
    : _numLocks(numLocks)
    , _header(NULL)
    , _mapping(NULL)
    , _userSemCntr(0)
{}

/**
 * Get an ID for a new write lock
 *
 * @return a non-negative lockID on success, -1 on failure
 */
IDATA
OSSharedMemoryCacheConfig::getNewWriteLockID()
{
    if (_userSemCntr < (_totalNumSems-1)) {
        return ++_userSemCntr;
    } else {
        return -1;
    }
}

/**
 * Get an ID for a readwrite area lock
 *
 * @return a non-negative lockID on success, -1 on failure
 */
IDATA
OSSharedMemoryCacheConfig::getReadWriteLockID() {
    return this->getNewWriteLockID();
}

/**
 * Get an ID for a write area lock
 *
 * @return a non-negative lockID on success, -1 on failure
 */
IDATA
OSSharedMemoryCacheConfig::getWriteLockID() {
    return this->getNewWriteLockID();
}


IDATA
OSSharedMemoryCacheConfig::acquireHeaderWriteLock(OMRPortLibrary* library, const char* cacheName, LastErrorInfo *lastErrorInfo)
{
    OMRPORT_ACCESS_FROM_OMRPORT(library);
    IDATA rc = 0;
    Trc_SHR_OSC_GlobalLock_getMutex(cacheName);

    if (NULL != lastErrorInfo) {
        lastErrorInfo->_lastErrorCode = 0;
    }

    if (_semhandle != NULL) {
        rc = omrshsem_deprecated_wait(_semhandle, SEM_HEADERLOCK, OMRPORT_SHSEM_MODE_UNDO);
        if (-1 == rc) {
            if (NULL != lastErrorInfo) {
                lastErrorInfo->populate(OMRPORTLIB);
            }
        }
    }
    Trc_SHR_OSC_GlobalLock_gotMutex(cacheName);
    return rc;
}

IDATA
OSSharedMemoryCacheConfig::releaseHeaderWriteLock(OMRPortLibrary* library,
                                                  LastErrorInfo* lastErrorInfo)
{
    OMRPORT_ACCESS_FROM_OMRPORT(library);
    IDATA rc = 0;
    if (NULL != lastErrorInfo) {
        lastErrorInfo->_lastErrorCode = 0;
    }

    if (_semhandle != NULL) {
        rc = omrshsem_deprecated_post(_semhandle, SEM_HEADERLOCK, OMRPORT_SHSEM_MODE_UNDO);
        if (-1 == rc) {
            if (NULL != lastErrorInfo) {
                lastErrorInfo->populate(OMRPORTLIB);
            }
        }
    }

    Trc_SHR_OSC_GlobalLock_released();
    return rc;
}

/**
 * Obtain the exclusive access right for the shared cache
 *
 * If this method succeeds, the caller will own the exclusive access right to the lock specified
 * and any other thread that attempts to call this method will be suspended.
 * If the process which owns the exclusive access right has crashed without relinquishing the access right,
 * it will automatically resume one of the waiting threads which will then own the access right.
 *
 * @param[in] lockID  The ID of the lock to acquire
 *
 * @return 0 if the operation has been successful, -1 if an error has occured
 */
IDATA
OSSharedMemoryCacheConfig::acquireLock(OMRPortLibrary* library,
                                       UDATA lockID,
                                       LastErrorInfo* lastErrorInfo)
{
    OMRPORT_ACCESS_FROM_OMRPORT(library);
    IDATA rc;

    Trc_SHR_OSC_enterMutex_Entry(lockID);

    if(_semhandle == NULL) {
        Trc_SHR_OSC_enterMutex_Exit1();
        Trc_SHR_Assert_ShouldNeverHappen();
        return -1;
    }

    if (lockID > (_totalNumSems-1)) {
        Trc_SHR_OSC_enterMutex_Exit2_V2(lockID, _totalNumSems-1);
        Trc_SHR_Assert_ShouldNeverHappen();
        return -1;
    }

    rc = omrshsem_deprecated_wait(_semhandle, lockID, OMRPORT_SHSEM_MODE_UNDO);
    if (rc == -1) {
        /* CMVC 97181 : Don't print error message because if JVM terminates with ^C signal, this function will return -1 and this is not an error*/
        I_32 myerror = omrerror_last_error_number();

        if ( ((I_32)(myerror | 0xFFFF0000)) != OMRPORT_ERROR_SYSV_IPC_ERRNO_EINTR) {
            Trc_SHR_OSC_enterMutex_Exit3(myerror);
            Trc_SHR_Assert_ShouldNeverHappen();

            return -1;
        }
    }

    Trc_SHR_OSC_enterMutex_Exit(lockID);
    return rc;
}

/**
 * Relinquish the exclusive access right
 *
 * If this method succeeds, the caller will return the exclusive access right to the lock specified.
 * If there is one or more thread(s) suspended on the Mutex by calling @ref SH_OSCache::acquireWriteLock,
 * then one of the threads will be resumed and become the new owner of the exclusive access rights for the lock
 *
 * @param[in] lockID  The ID of the lock to release
 *
 * @return 0 if the operations has been successful, -1 if an error has occured
 */
IDATA
OSSharedMemoryCacheConfig::releaseLock(OMRPortLibrary* library, UDATA lockID)
{
    OMRPORT_ACCESS_FROM_OMRPORT(library);
    IDATA rc;

    Trc_SHR_OSC_exitMutex_Entry(lockID);

    if(_semhandle == NULL) {
        Trc_SHR_OSC_exitMutex_Exit1();
        Trc_SHR_Assert_ShouldNeverHappen();
        return -1;
    }

    if (lockID > (_totalNumSems-1)) {
        Trc_SHR_OSC_exitMutex_Exit2_V2(lockID, _totalNumSems);
        Trc_SHR_Assert_ShouldNeverHappen();
        return -1;
    }

    rc = omrshsem_deprecated_post(_semhandle, lockID, OMRPORT_SHSEM_MODE_UNDO);
    Trc_SHR_OSC_exitMutex_Exit(lockID);
    return rc;
}
