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
#include "OSSharedMemoryCachePolicies.hpp"
#include "OSSharedMemoryCache.hpp"

#include "shrnls.h"
#include "ut_omrshr.h"

#if !defined(WIN32)
IDATA
OSSharedMemoryCachePolicies::openSharedMemory(const char* fileName,
                                              U_32 permissions,
                                              LastErrorInfo* lastErrorInfo)
{
    IDATA rc = -1;
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    Trc_SHR_OSC_Sysv_OpenSysVMemoryHelper_Enter();

    UDATA action;
    UDATA flags = OMRSHMEM_NO_FLAGS;

    if (NULL != lastErrorInfo) {
        lastErrorInfo->_lastErrorCode = 0;
    }

    action = OMRSH_SYSV_REGULAR_CONTROL_FILE;

    switch(action){
    case OMRSH_SYSV_REGULAR_CONTROL_FILE:
        flags = _cache->_configOptions->renderCreateOptionsToFlags();

        rc = omrshmem_open(_cache->_cacheLocation,
                           _cache->_configOptions->groupPermissions(),
                           &_cache->_config->_shmhandle,
                           fileName,
                           _cache->_configOptions->cacheSize(),
                           permissions,
                           OMRMEM_CATEGORY_CLASSES,
                           flags,
                           &_cache->_controlFileStatus);
        break;
    case OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE:
        rc = omrshmem_openDeprecated(_cache->_cacheLocation,
                                     _cache->_configOptions->groupPermissions(),
                                     &_cache->_config->_shmhandle,
                                     fileName,
                                     permissions,
                                     OMRSH_SYSV_OLDER_EMPTY_CONTROL_FILE,
                                     OMRMEM_CATEGORY_CLASSES);
        break;
    case OMRSH_SYSV_OLDER_CONTROL_FILE:
        rc = omrshmem_openDeprecated(_cache->_cacheLocation,
                                     _cache->_configOptions->groupPermissions(),
                                     &_cache->_config->_shmhandle,
                                     fileName,
                                     permissions,
                                     OMRSH_SYSV_OLDER_CONTROL_FILE,
                                     OMRMEM_CATEGORY_CLASSES);
        break;
    default:
        Trc_SHR_Assert_ShouldNeverHappen();
        break;
    }

 done:
    /* if above portLibrary call is successful, portable error number is set to 0 */
    if (NULL != lastErrorInfo) {
        lastErrorInfo->populate(OMRPORTLIB);
    }

    Trc_SHR_OSC_Sysv_OpenSysVMemoryHelper_Exit(rc);
    return rc;
}

IDATA
OSSharedMemoryCachePolicies::openSharedSemaphore(LastErrorInfo *lastErrorInfo)
{
    IDATA rc = -1;
    UDATA flags = _cache->_configOptions->renderCreateOptionsToFlags();
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    Trc_SHR_OSC_Sysv_OpenSysVSemaphoreHelper_Enter();

    UDATA action;

    if (NULL != lastErrorInfo) {
        lastErrorInfo->_lastErrorCode = 0;
    }

    action = OMRSH_SYSV_REGULAR_CONTROL_FILE;
    rc = omrshsem_deprecated_open(_cache->_cacheLocation,
                                  _cache->_configOptions->groupPermissions(),
                                  &_cache->_config->_semhandle,
                                  _cache->_semFileName,
                                  (int) _cache->_config->_totalNumSems,
                                  0,
                                  flags,
                                  &_cache->_controlFileStatus);

    if (NULL != lastErrorInfo) {
        lastErrorInfo->populate(_cache->_portLibrary);
    }

    Trc_SHR_OSC_Sysv_OpenSysVSemaphoreHelper_Exit(rc);
    return rc;
}

IDATA
OSSharedMemoryCachePolicies::destroySharedMemory()
{
    IDATA rc = -1;
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    Trc_SHR_OSC_Sysv_DestroySysVMemoryHelper_Enter();

    UDATA action;

    action = OMRSH_SYSV_REGULAR_CONTROL_FILE;
    rc = omrshmem_destroy(_cache->_cacheLocation,
                          _cache->_configOptions->groupPermissions(),
                          &_cache->_config->_shmhandle);

    if (-1 == rc) {
#if !defined(WIN32)
        I_32 errorno = omrerror_last_error_number();
        I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
        I_32 lastSysCall = errorno - lastError;

        if ((OMRPORT_ERROR_SYSV_IPC_SHMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError))
        {
            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_SHARED_MEMORY_DESTROY_NOT_PERMITTED,
                           omrshmem_getid(_cache->_config->_shmhandle));
        } else {
            const char * errormsg = omrerror_last_error_message();
            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_DESTROYSHM_ERROR_WITH_SHMID,
                           omrshmem_getid(_cache->_config->_shmhandle));
            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR,
                           errorno);
            Trc_SHR_Assert_True(errormsg != NULL);
            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR,
                           errormsg);
        }
#else /* !defined(WIN32) */
        I_32 errorno = omrerror_last_error_number();
        const char * errormsg = omrerror_last_error_message();
        OSC_ERR_TRACE(_cache->_configOptions,
                      OMRNLS_SHRC_OSCACHE_DESTROYSHM_ERROR);
        OSC_ERR_TRACE1(_cache->_configOptions,
                       OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR,
                       errorno);
        Trc_SHR_Assert_True(errormsg != NULL);
        OSC_ERR_TRACE1(_cache->_configOptions,
                       OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR,
                       errormsg);
#endif /* !defined(WIN32) */
    }

 done:
    Trc_SHR_OSC_Sysv_DestroySysVMemoryHelper_Exit(rc);
    return rc;
}

IDATA
OSSharedMemoryCachePolicies::destroySharedSemaphore()
{
    IDATA rc = -1;
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    Trc_SHR_OSC_Sysv_DestroySysVSemHelper_Enter();

    UDATA action;

    action = OMRSH_SYSV_REGULAR_CONTROL_FILE;

    rc = omrshsem_deprecated_destroy(&_cache->_config->_semhandle);

    if (-1 == rc) {
#if !defined(WIN32)
        I_32 errorno = omrerror_last_error_number();
        I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
        I_32 lastSysCall = errorno - lastError;

        if ((OMRPORT_ERROR_SYSV_IPC_SEMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError)) {
            OSC_ERR_TRACE1(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_SEMAPHORE_DESTROY_NOT_PERMITTED,
                           omrshsem_deprecated_getid(_cache->_config->_semhandle));
        } else {
            const char* errormsg = omrerror_last_error_message();

            OSC_ERR_TRACE1(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_DESTROYSEM_ERROR_WITH_SEMID,
                           omrshsem_deprecated_getid(_cache->_config->_semhandle));
            OSC_ERR_TRACE1(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR, errorno);
            Trc_SHR_Assert_True(errormsg != NULL);
            OSC_ERR_TRACE1(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR, errormsg);
        }
#else /* !defined(WIN32) */
        I_32 errorno = omrerror_last_error_number();
        const char * errormsg = omrerror_last_error_message();

        OSC_ERR_TRACE(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_DESTROYSEM_ERROR);
        OSC_ERR_TRACE1(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR, errorno);
        Trc_SHR_Assert_True(errormsg != NULL);
        OSC_ERR_TRACE1(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR, errormsg);
#endif /* !defined(WIN32) */
    }
 done:
    Trc_SHR_OSC_Sysv_DestroySysVSemHelper_Exit(rc);
    return rc;
}

/**
 * This method detects whether the given control file is non-readable, or read-only.
 *
 * @param [in] cacheDirName directory containing cache control file
 * @param [in] filename "semaphore" or "memory" depending on type of control file
 * @param [out] isNotReadable true if the file cannot be read, false otherwise
 * @param [out] isReadOnly true if the file is read-only, false otherwise
 *
 * @return -1 if it fails to get file stats, 0 otherwise.
 */
I_32
OSSharedMemoryCachePolicies::getControlFilePermissions(char *cacheDirName,
                                                       char *filename,
                                                       bool& isNotReadable,
                                                       bool& isReadOnly)
{
    char baseFile[OMRSH_MAXPATH];
    struct J9FileStat statbuf;
    I_32 rc;
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    omrstr_printf(baseFile, OMRSH_MAXPATH, "%s%s", cacheDirName, filename);
    rc = omrfile_stat(baseFile, 0, &statbuf);
    if (0 == rc) {
        UDATA euid = omrsysinfo_get_euid();
        if (euid == statbuf.ownerUid) {
            if (0 == statbuf.perm.isUserReadable) {
                isNotReadable = true;
                isReadOnly = false;
            } else {
                isNotReadable = false;
                if (0 == statbuf.perm.isUserWriteable) {
                    isReadOnly = true;
                } else {
                    isReadOnly = false;
                }
            }
        } else {
            if (0 == statbuf.perm.isGroupReadable) {
                isNotReadable = true;
                isReadOnly = false;
            } else {
                isNotReadable = false;
                if (0 == statbuf.perm.isGroupWriteable) {
                    isReadOnly = true;
                } else {
                    isReadOnly = false;
                }
            }
        }
    }

    return rc;
}

/**
 * This method checks whether the group access of the shared memory is successfully set when a new cache is created with "groupAccess" suboption
 *
 * @param[in] lastErrorInfo     Pointer to store last portable error code and/or message.
 *
 * @return -1 Failed to get the stats of the shared memory.
 *                      0 Group access is not set.
 *                      1 Group access is set.
 */
I_32
OSSharedMemoryCachePolicies::verifySharedMemoryGroupAccess(LastErrorInfo *lastErrorInfo)
{
    I_32 rc = 1;
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
    OMRPortShmemStatistic statBuf;

    memset(&statBuf, 0, sizeof(statBuf));
    if (OMRPORT_INFO_SHMEM_STAT_PASSED != omrshmem_handle_stat(_cache->_config->_shmhandle,
                                                               &statBuf))
    {
        if (NULL != lastErrorInfo) {
            lastErrorInfo->populate(OMRPORTLIB);
        }

        rc = -1;
    } else {
        if ((1 != statBuf.perm.isGroupWriteable) || (1 != statBuf.perm.isGroupReadable)) {
            rc = 0;
        }
    }

    return rc;
}


/**
 * This method checks whether the group access of the semaphore is successfully set when a new cache is created with "groupAccess" suboption
 *
 * @param[in] lastErrorInfo     Pointer to store last portable error code and/or message.
 *
 * @return -1 Failed to get the stats of the semaphore.
 *                      0 Group access is not set.
 *                      1 Group access is set.
 */
I_32
OSSharedMemoryCachePolicies::verifySharedSemaphoreGroupAccess(LastErrorInfo *lastErrorInfo)
{
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    I_32 rc = 1;
    OMRPortShsemStatistic statBuf;

    memset(&statBuf, 0, sizeof(statBuf));
    if (OMRPORT_INFO_SHSEM_STAT_PASSED != omrshsem_deprecated_handle_stat(_cache->_config->_semhandle, &statBuf)) {
        if (NULL != lastErrorInfo) {
            lastErrorInfo->populate(OMRPORTLIB);
        }

        rc = -1;
    } else {
        if ((1 != statBuf.perm.isGroupWriteable) || (1 != statBuf.perm.isGroupReadable)) {
            rc = 0;
        }
    }

    return rc;
}

#endif

/**
 * This method performs additional checks to catch scenarios that are not handled by permission and/or mode settings provided by operating system,
 * to avoid any unintended access to sempahore set.
 *
 * @param[in] lastErrorInfo     Pointer to store last portable error code and/or message
 *
 * @return enum SH_SysvSemAccess indicating if the process can access the semaphore set or not
 */
SH_SysvSemAccess
OSSharedMemoryCachePolicies::checkSharedSemaphoreAccess(LastErrorInfo *lastErrorInfo)
{
    SH_SysvSemAccess semAccess = OMRSH_SEM_ACCESS_ALLOWED;

    if (NULL != lastErrorInfo) {
        lastErrorInfo->_lastErrorCode = 0;
    }

#if !defined(WIN32)
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    if (NULL == _cache->_config->_semhandle) {
        semAccess = OMRSH_SEM_ACCESS_ALLOWED;
    } else {
        IDATA rc;
        OMRPortShsemStatistic statBuf;
        I_32 semid = omrshsem_deprecated_getid(_cache->_config->_semhandle);

        memset(&statBuf, 0, sizeof(statBuf));
        rc = omrshsem_deprecated_handle_stat(_cache->_config->_semhandle, &statBuf);

        if (OMRPORT_INFO_SHSEM_STAT_PASSED == rc) {
            UDATA uid = omrsysinfo_get_euid();

            if (statBuf.cuid != uid) {
                if (statBuf.ouid == uid) {
                    /* Current process is owner but not the creator of the semaphore set.
                     * This implies external entity assigned ownership of the semaphore set to this process.
                     */
                    semAccess = OMRSH_SEM_ACCESS_OWNER_NOT_CREATOR;
                    Trc_SHR_OSC_Sysv_checkSemaphoreAccess_OwnerNotCreator(uid, semid, statBuf.cuid, statBuf.ouid);
                } else {
                    UDATA gid = omrsysinfo_get_egid();
                    bool sameGroup = false;

                    if ((statBuf.cgid == gid) || (statBuf.ogid == gid)) {
                        sameGroup = true;
                        Trc_SHR_OSC_Sysv_checkSemaphoreAccess_GroupIDMatch(gid, semid, statBuf.cgid, statBuf.ogid);
                    } else {
                        /* check supplementary groups */
                        U_32 *list = NULL;
                        IDATA size = 0;
                        IDATA i;

                        size = omrsysinfo_get_groups(&list, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);
                        if (size > 0) {
                            for (i = 0; i < size; i++) {
                                if ((statBuf.ogid == list[i]) || (statBuf.cgid == list[i])) {
                                    sameGroup = true;
                                    Trc_SHR_OSC_Sysv_checkSemaphoreAccess_SupplementaryGroupMatch(list[i], statBuf.cgid, statBuf.ogid, semid);
                                    break;
                                }
                            }
                        } else {
                            semAccess = OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED;

                            if (NULL != lastErrorInfo) {
                                lastErrorInfo->populate(_cache->_portLibrary);
                            }

                            Trc_SHR_OSC_Sysv_checkSemaphoreAccess_GetGroupsFailed();
                            goto _end;
                        }
                        if (NULL != list) {
                            omrmem_free_memory(list);
                        }
                    }
                    if (sameGroup) {
                        /* This process belongs to same group as owner or creator of the semaphore set. */
                        if (0 == _cache->_configOptions->groupPermissions()) {
                            /* If 'groupAccess' option is not set, it implies this process wants to attach to a shared cache that it owns or created.
                             * But this process is neither creator nor owner of the semaphore set.
                             * This implies we should not allow this process to use the cache.
                             */
                            semAccess = OMRSH_SEM_ACCESS_GROUP_ACCESS_REQUIRED;
                            Trc_SHR_OSC_Sysv_checkSemaphoreAccess_GroupAccessRequired(semid);
                        }
                    } else {
                        /* This process does not belong to same group as owner or creator of the semaphore set.
                         * Do not allow access to the cache.
                         */
                        semAccess = OMRSH_SEM_ACCESS_OTHERS_NOT_ALLOWED;
                        Trc_SHR_OSC_Sysv_checkSemaphoreAccess_OthersNotAllowed(semid);
                    }
                }
            }
        } else {
            semAccess = OMRSH_SEM_ACCESS_CANNOT_BE_DETERMINED;
            if (NULL != lastErrorInfo) {
                lastErrorInfo->populate(_cache->_portLibrary);
            }
            Trc_SHR_OSC_Sysv_checkSemaphoreAccess_ShsemStatFailed(semid);
        }
    }

 _end:
#endif /* !defined(WIN32) */

    return semAccess;
}

/**
 * This method performs additional checks to catch scenarios that are not handled by permission and/or mode settings provided by operating system,
 * to avoid any unintended access to shared memory.
 *
 * @param[in] lastErrorInfo     Pointer to store last portable error code and/or message
 *
 * @return enum SH_SysvShmAccess indicating if the process can access the shared memory or not
 */
SH_SysvShmAccess
OSSharedMemoryCachePolicies::checkSharedMemoryAccess(LastErrorInfo *lastErrorInfo)
{
    SH_SysvShmAccess shmAccess = OMRSH_SHM_ACCESS_ALLOWED;

    if (NULL != lastErrorInfo) {
        lastErrorInfo->_lastErrorCode = 0;
    }

#if !defined(WIN32)
    IDATA rc = -1;
    OMRPortShmemStatistic statBuf;
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
    I_32 shmid = omrshmem_getid(_cache->_config->_shmhandle);

    memset(&statBuf, 0, sizeof(statBuf));
    rc = omrshmem_handle_stat(_cache->_config->_shmhandle, &statBuf);
    if (OMRPORT_INFO_SHMEM_STAT_PASSED == rc) {
        UDATA uid = omrsysinfo_get_euid();

        if (statBuf.cuid != uid) {
            if (statBuf.ouid == uid) {
                /* Current process is owner but not the creator of the shared memory.
                 * This implies external entity assigned ownership of the shared memory to this process.
                 */
                shmAccess = OMRSH_SHM_ACCESS_OWNER_NOT_CREATOR;
                Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_OwnerNotCreator(uid, shmid, statBuf.cuid, statBuf.ouid);
            } else {
                UDATA gid = omrsysinfo_get_egid();
                bool sameGroup = false;

                if ((statBuf.ogid == gid) || (statBuf.cgid == gid)) {
                    sameGroup = true;
                    Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_GroupIDMatch(gid, shmid, statBuf.cgid, statBuf.ogid);
                } else {
                    /* check supplementary groups */
                    U_32 *list = NULL;
                    IDATA size = 0;
                    IDATA i;

                    size = omrsysinfo_get_groups(&list, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);
                    if (size > 0) {
                        for (i = 0; i < size; i++) {
                            if ((statBuf.ogid == list[i]) || (statBuf.cgid == list[i])) {
                                sameGroup = true;
                                Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_SupplementaryGroupMatch(list[i], statBuf.cgid, statBuf.ogid, shmid);
                                break;
                            }
                        }
                    } else {
                        shmAccess = OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED;
                        if (NULL != lastErrorInfo) {
                            lastErrorInfo->populate(OMRPORTLIB);
                        }
                        Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_GetGroupsFailed();
                        goto _end;
                    }
                    if (NULL != list) {
                        omrmem_free_memory(list);
                    }
                }
                if (sameGroup) {
                    /* This process belongs to same group as owner or creator of the shared memory. */
                    if (0 == _cache->_configOptions->groupPermissions()) {
                        /* If 'groupAccess' option is not set, it implies this process wants to attach to a shared cache that it owns or created.
                         * But this process is neither creator nor owner of the semaphore set.
                         * This implies we should not allow this process to use the cache.
                         */
                        Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_GroupAccessRequired(shmid);

                        if (statBuf.perm.isGroupWriteable) {
                            /* The shared memory has group read-write permission set, so this process can use it if 'groupAcccess' option is specified. */
                            shmAccess = OMRSH_SHM_ACCESS_GROUP_ACCESS_REQUIRED;
                        } else {
                            /* The shared memory does not have group write permission set, so this process can use it in readonly mode. */
                            shmAccess = OMRSH_SHM_ACCESS_GROUP_ACCESS_READONLY_REQUIRED;
                        }
                    }
                } else {
                    /* This process does not belong to same group as owner or creator of the shared memory.
                     * Do not allow access to the cache.
                     */
                    shmAccess = OMRSH_SHM_ACCESS_OTHERS_NOT_ALLOWED;
                    Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_OthersNotAllowed(shmid);
                }
            }
        }
    } else {
        shmAccess = OMRSH_SHM_ACCESS_CANNOT_BE_DETERMINED;

        if (NULL != lastErrorInfo) {
            lastErrorInfo->populate(OMRPORTLIB);
        }

        Trc_SHR_OSC_Sysv_checkSharedMemoryAccess_ShmemStatFailed(shmid);
    }

 _end:
#endif /* !defined(WIN32) */

    return shmAccess;
}

/**
 * Method to clean up semaphore set and shared memory resources as part of error handling.
 */
void
OSSharedMemoryCachePolicies::cleanupSystemResources(void)
{
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

    /* Setting handles to null prevents further use of this class, only if we haven't finished startup */
    if (NULL != _cache->_config->_shmhandle) {
        /* When ::startup() calls omrshmem_open() the cache is opened attached.
         * So, we must detach if we want clean up to work (see isCacheActive call below)
         */
        omrshmem_detach(&_cache->_config->_shmhandle);
    }

#if !defined(WIN32)
    /*If someone is still attached, don't destroy it*/
    /* isCacheActive isn't really accurate for Win32, so can't check */
    if(_cache->isCacheActive()) {
        if (NULL != _cache->_config->_semhandle) {
            omrshsem_deprecated_close(&_cache->_config->_semhandle);
            OSC_ERR_TRACE(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_CLOSESEM);
        }

        if (NULL != _cache->_config->_shmhandle) {
            omrshmem_close(&_cache->_config->_shmhandle);
            OSC_ERR_TRACE(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_CLOSESM);
        }

        return;
    }
#endif

    if ((NULL != _cache->_config->_semhandle) && (OMRSH_SEM_ACCESS_ALLOWED == _cache->_config->_semAccess)) {
#if defined(WIN32)
        if (omrshsem_deprecated_destroy(&_cache->_config->_semhandle) == 0) {
            OSC_TRACE(_cache->_configOptions, OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SEM);
        } else {
            I_32 errorno = omrerror_last_error_number();
            const char * errormsg = omrerror_last_error_message();

            OSC_ERR_TRACE(_cache->_configOptions,
                          OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_ERROR);
            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);

            Trc_SHR_Assert_True(errormsg != NULL);

            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
        }
#else
        I_32 semid = omrshsem_deprecated_getid(_cache->_config->_semhandle);

        if (omrshsem_deprecated_destroy(&_cache->_config->_semhandle) == 0) {
            OSC_TRACE1(_cache->_configOptions,
                       OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SEM_WITH_SEMID,
                       semid);
        } else {
            I_32 errorno = omrerror_last_error_number();
            const char * errormsg = omrerror_last_error_message();
            I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
            I_32 lastSysCall = errorno - lastError;

            if ((OMRPORT_ERROR_SYSV_IPC_SEMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError)) {
                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_NOT_PERMITTED,
                               semid);
            } else {
                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSEM_ERROR_V1,
                               semid);
                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER,
                               errorno);

                Trc_SHR_Assert_True(errormsg != NULL);

                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER,
                               errormsg);
            }
        }
#endif
    }

    if ((NULL != _cache->_config->_shmhandle) && (OMRSH_SHM_ACCESS_ALLOWED == _cache->_config->_shmAccess)) {
#if defined(WIN32)
        if (omrshmem_destroy(_cacheLocation,
                             _cache->_configOptions->groupPermissions(),
                             &_cache->_config->_shmhandle) == 0)
        {
            OSC_TRACE(_cache->_configOptions,
                      OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SHM);
        } else {
            I_32 errorno = omrerror_last_error_number();
            const char * errormsg = omrerror_last_error_message();

            OSC_ERR_TRACE(_cache->_configOptions,
                          OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSM_ERROR);
            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER, errorno);
            Trc_SHR_Assert_True(errormsg != NULL);
            OSC_ERR_TRACE1(_cache->_configOptions,
                           OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER, errormsg);
        }
#else
        I_32 shmid = omrshmem_getid(_cache->_config->_shmhandle);

        if (omrshmem_destroy(_cache->_cacheLocation,
                             _cache->_configOptions->groupPermissions(),
                             &_cache->_config->_shmhandle) == 0)
        {
            OSC_TRACE1(_cache->_configOptions,
                       OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYED_SHM_WITH_SHMID,
                       shmid);
        } else {
            I_32 errorno = omrerror_last_error_number();
            const char * errormsg = omrerror_last_error_message();

            I_32 lastError = errorno | OMRPORT_ERROR_SYSTEM_CALL_ERRNO_MASK;
            I_32 lastSysCall = errorno - lastError;

            if ((OMRPORT_ERROR_SYSV_IPC_SHMCTL_ERROR == lastSysCall) && (OMRPORT_ERROR_SYSV_IPC_ERRNO_EPERM == lastError))
            {
                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSHM_NOT_PERMITTED,
                               shmid);
            } else {
                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_HANDLE_ERROR_ACTION_DESTROYSM_ERROR_V1,
                               shmid);
                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_PORT_ERROR_NUMBER_SYSV_ERR_RECOVER,
                               errorno);
                Trc_SHR_Assert_True(errormsg != NULL);
                OSC_ERR_TRACE1(_cache->_configOptions,
                               OMRNLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE_SYSV_ERR_RECOVER,
                               errormsg);
            }
        }
#endif
    }
}

IDATA
OSSharedMemoryCachePolicies::openSharedMemoryWrapper(const char *fileName, LastErrorInfo *lastErrorInfo)
{
    OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
    IDATA rc = 0;
    U_32 permissions =
        _cache->_configOptions->readOnlyOpenMode() ? OMRSH_SHMEM_PERM_READ
                                                   : OMRSH_SHMEM_PERM_READ_WRITE;

    LastErrorInfo localLastErrorInfo;
    Trc_SHR_OSC_shmemOpenWrapper_Entry(fileName);
    UDATA flags = _cache->_configOptions->renderCreateOptionsToFlags();

#if !defined(WIN32)
    rc = openSharedMemory(fileName, permissions, &localLastErrorInfo);
#else
    rc = omrshmem_open(_cache->_cacheLocation,
                       _cache->_configOptions->groupPermissions(),
                       &_cache->_config->_shmhandle,
                       fileName,
                       _cache->_configOptions->cacheSize(),
                       permissions,
                       OMRMEM_CATEGORY_CLASSES_SHC_CACHE,
                       flags,
                       NULL);

    localLastErrorInfo.populate(OMRPORTLIB);
#endif

#if defined(OMRZOS390)
    if (OMRPORT_ERROR_SHMEM_ZOS_STORAGE_KEY_READONLY == rc) {
        _cache->_configOptions->setReadOnlyOpenMode();
        permissions = OMRSH_SHMEM_PERM_READ;
        rc = openSharedMemory(fileName, permissions, &localLastErrorInfo);
    }
#endif

    if (OMRPORT_ERROR_SHMEM_OPFAILED == rc) {
        if (_cache->_configOptions->tryReadOnlyOnOpenFailure()) {
            _cache->_configOptions->setReadOnlyOpenMode();

            permissions = OMRSH_SHMEM_PERM_READ;

            rc = omrshmem_open(_cache->_cacheLocation,
                               _cache->_configOptions->groupPermissions(),
                               &_cache->_config->_shmhandle,
                               fileName,
                               _cache->_configOptions->cacheSize(),
                               permissions,
                               OMRMEM_CATEGORY_CLASSES_SHC_CACHE,
                               OMRSHMEM_NO_FLAGS,
                               &_cache->_controlFileStatus);

            /* if omrshmem_open is successful, portable error number is set to 0 */
            localLastErrorInfo.populate(OMRPORTLIB);
        }
    }

    if (((rc == OMRPORT_INFO_SHMEM_OPENED) || (rc == OMRPORT_INFO_SHMEM_OPENED_STALE)) && (permissions == OMRSH_SHMEM_PERM_READ)) {
        Trc_SHR_OSC_Event_OpenReadOnly();
        _cache->_runningReadOnly = true;
    }

    if (NULL != lastErrorInfo) {
        memcpy(lastErrorInfo, &localLastErrorInfo, sizeof(LastErrorInfo));
    }

    Trc_SHR_OSC_shmemOpenWrapper_Exit(rc, _cache->_configOptions->cacheSize());
    return rc;
}
