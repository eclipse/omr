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

#if !defined(OSCACHE_CONFIG_HPP_INCLUDED)
#define OSCACHE_CONFIG_HPP_INCLUDED

#include "omr.h"
#include "omrsharedconsts.h"

#include "OMROSCache.hpp"

/**
 * @brief A class to manage shared cache config at operation system level
 * @note This class declares virtual functions that are extended and defined by the persistent (Memory Mapped)
 * and non-persistent (Shared Memory) shared cache 
 */
class OSCacheConfig
{
public:

    /**
    * @brief Get an ID for a write area lock
    * @return a non-negative lockID on success, -1 on failure
    */
    virtual IDATA omr_getWriteLockID() = 0;

    /**
    * @brief Get an ID for a readwrite area lock
    * @return a non-negative lockID on success, -1 on failure
    */
    virtual IDATA omr_getReadWriteLockID() = 0;
    
    /**
    * @brief Obtain the exclusive access right for the shared cache
    * @detail If this method succeeds, the caller will own the exclusive access right to the lock specified
    * and any other thread that attempts to call this method will be suspended.
    * @param[in] library OMR port library
    * @param[in] lockID The ID of the lock to acquire
    * @param[in] lastErrorInfo Stores portable error number and message 
    * @return 0 if the operation has been successful, -1 if an error has occured
    */
    virtual IDATA omr_acquireLock(OMRPortLibrary* library, UDATA lockID, LastErrorInfo* lastErrorInfo = NULL) = 0;

    /**
    * @brief Relinquish the exclusive access right
    * @detail If this method succeeds, the caller will return the exclusive access right to the lock specified.
    * @param[in] library OMR port library
    * @param[in] lockID The ID of the lock to release
    * @return 0 if the operation has been successful, -1 if an error has occured
    */
    virtual IDATA omr_releaseLock(OMRPortLibrary* library, UDATA lockID) = 0;
  
    /**
    * @brief Sets the header and data section region of the shared cache to 0
    */
    virtual void omr_nullifyRegions() = 0;
};
#endif /* !defined(OSCACHE_CONFIG_HPP_INCLUDED) */
