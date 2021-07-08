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
#if !defined(OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED)
#define OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED

#include "omr.h"

class OSCacheConfigOptions
{
public:
    typedef enum StartupReason {
        StartupReason_Destroy,
        StartupReason_Expired,
        StartupReason_Open,
        StartupReason_Stat,
        StartupReason_EnsureWideOpen = 0x1000000
    } StartupReason;

    typedef enum StatReason {
        StatReason_Destroy,
        StatReason_Expired,
        StatReason_GatherStatistics,
        StatReason_Iterate,
        StatReason_List,
        StatReason_EnsureWideEnum = 0x1000000
    } StatReason;

protected:
    I_32 _openMode;
    StartupReason _startupReason;

public:
    /** 
     * @brief Constructor to set the startupReason to Open
     * @param[in] openMode Open Mode of startupReason enum
     */
    OSCacheConfigOptions(I_32 openMode)
        : _openMode(openMode)
        , _startupReason(StartupReason::StartupReason_Open)
    {}

    /** 
     * @brief Destructor
     */
    virtual ~OSCacheConfigOptions() {}

    /** 
     * @brief Sets the User Home Directory as Cache Directory
     * @return Returns true on success.
     */
    virtual bool omr_useUserHomeDirectoryForCacheDir() = 0;
    
    /** 
     * @brief Check if cache directory is user specified directory
     * @return Returns false
     */
    virtual bool omr_isUserSpecifiedCacheDir() = 0;
    
    /** 
     * @brief Check if the cache being opened in read-only mode
     * @return Returns false
     */
    virtual bool omr_readOnlyOpenMode() = 0;

    /** 
     * @brief Sets the cache file for group access mode
     * @return Returns the mode permission
     */
    virtual I_32 omr_fileMode();

    /** 
     * @brief Check the file open mode
     * @note the open mode flags are defined in omrsharedconsts.h
     * @return Returns the mode
     */
    virtual I_32 omr_openMode() = 0;

    /** 
     * @brief Check group permission of the cache file
     * @return Retruns 1 if group permissions are enabled
     */
    virtual UDATA omr_groupPermissions() = 0;

    /** 
     * @brief Return the permissions of the created cache directory
     * @return Returns OMRSH_DIRPERM_DEFAULT
     */
    virtual IDATA omr_cacheDirPermissions() = 0;

    /** 
     * @brief Detect network cache.
     * @return IDATA
     */
    virtual bool omr_usingNetworkCache() = 0;

    /**
     * @brief Verifies if the restore check is enabled
     * @note the restore check only applies to the shared memory cache,
       so its required to create an OSSharedMemoryCacheConfigOptions
       subclass, and put it there then OSSharedMemoryCache will own a
       reference to an OSSharedMemoryCacheConfigOptions object
       @return bool
     */
    virtual bool omr_restoreCheckEnabled() = 0;

    /** 
     * @brief Opening the cache file but not creating a cache. 
     * @return bool
     */
    virtual bool omr_openButDoNotCreate() = 0;

    /** 
     * @brief Opening the cache in order to destroy it
     * @return IDATA
     */
    virtual bool omr_openToDestroyExistingCache() = 0;
    
    /** 
     * @brief Opening the cache in order to destroy the already expired cache
     * @return bool
     */
    virtual bool omr_openToDestroyExpiredCache() = 0;
    
     /** 
     * @brief Opening an existing cache in order to retrive its stats
     * @return bool
     */ 
    virtual bool omr_openToStatExistingCache() = 0;

    /** 
     * @brief Associated with the StatsReason enum. Returns false for StatReason_Destroy .
     * @return bool
     */ 
    virtual bool omr_statToDestroy() = 0;

    /** 
     * @brief Associated with the StatsReason enum. Returns false for StatReason_Expired.
     * @return bool
     */ 
    virtual bool omr_statExpired() = 0;

    /** 
     * @brief Associated with the StatsReason enum. Returns false for StatReason_Iterate.
     * @return bool
     */ 
    virtual bool omr_statIterate() = 0;

    /** 
     * @brief Associated with the StatsReason enum. Returns false for StatReason_List.
     * @return bool
     */ 
    virtual bool omr_statList() = 0;

    /** 
     * @brief Sets the base directory that needs to be appended to the filename.
     * @param[in] appendBaseDir Append Base Directory
     * @return OSCacheConfigOptions&
     */ 
    virtual OSCacheConfigOptions& omr_setAppendBaseDir(bool appendBaseDir) = 0;

    /** 
     * @brief Checks if semaphore check is enabled
     * @return bool
     */ 
    virtual bool omr_semaphoreCheckEnabled() = 0;

    /** 
     * @brief Sets the StatReason members
     * @param[in] reason Reason value from the StatReason enum 
     * @return OSCacheConfigOptions&
     */ 
    virtual OSCacheConfigOptions& omr_setStatReason(StatReason reason) = 0;

    /** 
     * @brief Sets to StartupReason_Open of StartupReason enum 
     * @param[in] reason Reason value from the StartupReason enum 
     * @return OSCacheConfigOptions&
     */ 
    virtual OSCacheConfigOptions& omr_setOpenReason(StartupReason reason) = 0;

    /** 
     * @brief Sets to the cache to Read-Only open mode
     * @return OSCacheConfigOptions&
     */ 
    virtual OSCacheConfigOptions& omr_setReadOnlyOpenMode() = 0;

    /** 
     * @brief Sets the OS cache to open mode 
     * @param[in] openMode Open Mode
     * @return OSCacheConfigOptions&
     */ 
    virtual OSCacheConfigOptions& omr_setOpenMode(I_32 openMode) = 0;

     /** 
     * @brief Returns the block size of the cache
     * @return OSCacheConfigOptions&
     */ 
    virtual U_32 omr_cacheSize() = 0;

     /** 
     * @brief sets the Cache Size
     * @param[in] size New Size
     * @return OSCacheConfigOptions&
     */ 
    virtual OSCacheConfigOptions& omr_setCacheSize(U_32 size) = 0;

     /** 
     * @brief Gets the maxCRCSamples value from the CacheCRCChecker
     * @return U_32 _maxCRCSamples value
     */ 
    virtual U_32 omr_maxCRCSamples() = 0;

     /** 
     * @brief To check if the cache creates a file
     * @return bool
     */ 
    virtual bool omr_createFile() = 0;

     /** 
     * @brief To check if we try to open the cache read-only when or if we failed to open the cache with write permissions
     * @return bool
     */ 
    virtual bool omr_tryReadOnlyOnOpenFailure() = 0;

     /** 
     * @brief To check if we dumo the contents when we cache is corrupt
     * @return bool
     */ 
    virtual bool omr_disableCorruptCacheDumps() = 0;

     /** 
     * @brief To check if the VERBOSE flag is enabled
     * @return bool
     */ 
    virtual bool omr_verboseEnabled() = 0;

     /** 
     * @brief To check if the group access is enabled
     * @return bool
     */ 
    virtual bool omr_groupAccessEnabled() = 0;

     /** 
     * @brief Renders the options to a bit vector understood by the functions of the OMR port library.
     * @return bool
     */ 
    virtual U_32 omr_renderToFlags() = 0;

    /** 
     * @brief Renders the create options to a bit vector understood by the functions of the OMR port library.
     * @return bool
     */ 
    virtual UDATA omr_renderCreateOptionsToFlags() = 0;

    /** 
     * @brief Renders the verbose options to a bit vector understood by the functions of the OMR port library.
     * @return bool
     */ 
    virtual UDATA omr_renderVerboseOptionsToFlags() = 0;

    /** 
     * @brief Resets the VERBOSE options flags.
     * @return bool
     */ 
    virtual void omr_resetVerboseOptionsFromFlags(UDATA flags) = 0;
};
#endif /* !defined(OS_CACHE_CONFIG_OPTIONS_HPP_INCLUDED) */
