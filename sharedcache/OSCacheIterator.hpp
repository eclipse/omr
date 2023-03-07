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

#if !defined(OSCACHE_ITERATOR_HPP_INCLUDED)
#define OSCACHE_ITERATOR_HPP_INCLUDED

#include "omr.h"
#include "omrport.h"
#include "ut_omrshr.h"

/**
 * @brief Provides functionality to interate through shared cache files at operation system level
 * @note This class declares virtual functions that are extended and defined by the persistent (Memory Mapped)
 * and non-persistent (Shared Memory) shared cache 
 */
class OSCacheIterator
{
public: 
    
    /**
    * @brief Find the first cache file in a given cacheDir
    * Follows the format of omrfile_findfirst
    * @param [in] portLibrary  A port library
    * @return A handle to the cache file found or -1 if the cache file doesn't exist
    */
    virtual UDATA omr_findFirst(OMRPortLibrary *portLibrary) = 0;

    /**
    * @brief Find the next cache file in a given cacheDir
    * Follows the format of omrfile_findnext
    * @param [in] portLibrary  A port library
    * @param [in] findHandle  The handle of the last file found
    * @return A handle to the cache file found or -1 if the next cache file doesn't exist
    */
    virtual I_32 omr_findNext(OMRPortLibrary *portLibrary, UDATA findHandle) = 0;

    /**
    * @brief Find the closest existing cache file in a given cacheDir
    * Follows the format of omrfile_findclose
    * @param [in] portLibrary  A port library
    * @param [in] findHandle  The handle of the last file found
    * @return A handle to the cache file found or -1 if the cache file doesn't exist
    */
    virtual void omr_findClose(OMRPortLibrary *portLibrary, UDATA findHandle) = 0;
};

#endif /* !defined(OSCACHE_ITERATOR_HPP_INCLUDED) */
