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
#include "omrcfg.h"
#include "omrargscan.h"
#include "omrport.h"
#include "pool_api.h"
#include "shrnls.h"
#include "omrsharedhelper.h"
#include "sharedconsts.h"

#include "OSCacheImpl.hpp"
#include "OSCacheConfigOptions.hpp"
#include "OSCacheUtils.hpp"

IDATA
OSCacheUtils::getCacheDirName(OMRPortLibrary* portLibrary,
                              const char* ctrlDirName,
                              char* buffer,
                              UDATA bufferSize,
                              OSCacheConfigOptions* configOptions)
{
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
    IDATA rc;

    /* Cache directory used is the omrshmem dir, regardless of whether we're using omrshmem or omrmmap */

    if ((NULL == ctrlDirName) && !configOptions->groupAccessEnabled())
        {
            /* omrshmem_getDir() always tries the CSIDL_LOCAL_APPDATA directory (C:\Documents and Settings\username\Local Settings\Application Data)
             * first on Windows if ctrlDirName is NULL, regardless of whether OMRSHMEM_GETDIR_USE_USERHOME is set or not. So OMRSHMEM_GETDIR_USE_USERHOME is effective on UNIX only.
             */

            configOptions->useUserHomeDirectoryForCacheDir();
        }

    rc = omrshmem_getDir(ctrlDirName, configOptions->renderToFlags(), buffer, bufferSize);

    if (rc == -1) {
        return -1;
    }

    return 0;
}

/**
 * Create the directory to use for the cache file or control file(s)
 *
 * @param [in] portLibrary  A portLibrary
 * @param[in] cacheDirName The name of the cache directory
 * @param[in] cacheDirPermissions The permissions of the created cache directory
 * @param[in] cleanMemorySegments, set TRUE to call cleanSharedMemorySegments. It will clean sysv memory segments.
 *                              It should be set to TRUE if the ctrlDirName is NULL. This stops shared memory
 *                              from being leaked when wiped from /tmp, the default sysv location if ctrlDirName
 *                              is NULL.
 *
 * Returns -1 for error, >=0 for success
 */
IDATA
OSCacheUtils::createCacheDir(OMRPortLibrary* portLibrary, char* cacheDirName, UDATA cacheDirPermissions, bool cleanMemorySegments)
{
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
    IDATA rc;

    Trc_SHR_OSC_createCacheDir_Entry(cacheDirName, cleanMemorySegments);

    omrthread_t self;
    omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

    rc = omrshmem_createDir(cacheDirName, cacheDirPermissions, cleanMemorySegments);

    omrthread_detach(self);

    Trc_SHR_OSC_createCacheDir_Exit();
    return rc;
}

/* Returns the full path of a cache based on the current cacheDir value */
IDATA
OSCacheUtils::getCachePathName(OMRPortLibrary* portLibrary,
                               const char* cacheDirName,
                               char* buffer,
                               UDATA bufferSize,
                               const char* cacheName)
{
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);

    Trc_SHR_OSC_getCachePathName_Entry(cacheName);

    omrstr_printf(buffer, bufferSize, "%s%s", cacheDirName, cacheName);

    Trc_SHR_OSC_getCachePathName_Exit();

    return 0;
}

UDATA
OSCacheUtils::statCache(OMRPortLibrary* portLibrary,
                        const char* cacheDirName,
                        const char* cacheName,
                        bool displayNotFoundMsg)
{
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
    char fullPath[OMRSH_MAXPATH];

    Trc_SHR_OSC_statCache_Entry(cacheName);

    omrstr_printf(fullPath, OMRSH_MAXPATH, "%s%s", cacheDirName, cacheName);

    if (omrfile_attr(fullPath) == EsIsFile) {
        Trc_SHR_OSC_statCache_cacheFound();
        return 1;
    }

    if (displayNotFoundMsg) {
        omrnls_printf(J9NLS_ERROR, OMRNLS_SHRC_OSCACHE_NOT_EXIST);
    }

    Trc_SHR_OSC_statCache_cacheNotFound();
    return 0;
}

/**
 * This method performs additional checks to catch scenarios that are not handled by permission and/or mode settings provided by operating system,
 * to avoid any unintended access to shared cache file
 *
 * @param [in] portLibrary  The port library
 * @param [in] findHandle  The handle of the shared cache file
 * @param[in] openMode The file access mode
 * @param[in] lastErrorInfo     Pointer to store last portable error code and/or message
 *
 * @return enum SH_CacheFileAccess indicating if the process can access the shared cache file set or not
 */
SH_CacheFileAccess
OSCacheUtils::checkCacheFileAccess(OMRPortLibrary *portLibrary,
                                   UDATA fileHandle,
                                   OSCacheConfigOptions* configOptions,
                                   LastErrorInfo *lastErrorInfo)
{
    SH_CacheFileAccess cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_ALLOWED;

    if (NULL != lastErrorInfo) {
        lastErrorInfo->_lastErrorCode = 0;
    }

#if !defined(WIN32)
    OMRPORT_ACCESS_FROM_OMRPORT(portLibrary);
    J9FileStat statBuf;
    IDATA rc = omrfile_fstat(fileHandle, &statBuf);

    if (-1 != rc) {
        UDATA uid = omrsysinfo_get_euid();

        if (statBuf.ownerUid != uid) {
            UDATA gid = omrsysinfo_get_egid();
            bool sameGroup = false;

            if (statBuf.ownerGid == gid) {
                sameGroup = true;
                Trc_SHR_OSC_File_checkCacheFileAccess_GroupIDMatch(gid, statBuf.ownerGid);
            } else {
                /* check supplementary groups */
                U_32 *list = NULL;
                IDATA size = 0;
                IDATA i = 0;

                size = omrsysinfo_get_groups(&list, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);
                if (size > 0) {
                    for (i = 0; i < size; i++) {
                        if (statBuf.ownerGid == list[i]) {
                            sameGroup = true;
                            Trc_SHR_OSC_File_checkCacheFileAccess_SupplementaryGroupMatch(list[i], statBuf.ownerGid);
                            break;
                        }
                    }
                } else {
                    cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED;
                    if (NULL != lastErrorInfo) {
                        lastErrorInfo->populate(OMRPORTLIB);
                    }

                    Trc_SHR_OSC_File_checkCacheFileAccess_GetGroupsFailed();
                    goto _end;
                }
                if (NULL != list) {
                    omrmem_free_memory(list);
                }
            }
            if (sameGroup) {
                /* This process belongs to same group as owner of the shared cache file. */
                if (configOptions->groupAccessEnabled()) {
                    /* If 'groupAccess' option is not set, it implies this process wants to access a shared cache file that it created.
                     * But this process is not the owner of the cache file.
                     * This implies we should not allow this process to use the cache.
                     */
                    cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED;
                    Trc_SHR_OSC_File_checkCacheFileAccess_GroupAccessRequired();
                }
            } else {
                /* This process does not belong to same group as owner of the shared cache file.
                 * Do not allow access to the cache.
                 */
                cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED;
                Trc_SHR_OSC_File_checkCacheFileAccess_OthersNotAllowed();
            }
        }
    } else {
        cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED;

        if (NULL != lastErrorInfo) {
            lastErrorInfo->populate(OMRPORTLIB);
        }

        Trc_SHR_OSC_File_checkCacheFileAccess_FileStatFailed();
    }

 _end:
#endif /* !defined(oWIN32) */

    return cacheFileAccess;
}
