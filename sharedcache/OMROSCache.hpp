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

#if !defined(OSCACHE_HPP_INCLUDED)
#define OSCACHE_HPP_INCLUDED

#include "OSCacheConfigOptions.hpp"
#include "OSCacheRegion.hpp"
#include "OSCacheRegionInitializer.hpp"
#include "OSCacheRegionSerializer.hpp"

#include "omr.h"
#include "omrsharedconsts.h"

/**
 * @brief Class to manage errors 
 * @detail This class stores and sets the last portable error code and error message
 */
class LastErrorInfo
{
public:
    I_32 _lastErrorCode;
    const char* _lastErrorMsg;

    /**
     * @brief Constructor
     */
    LastErrorInfo()
        : _lastErrorCode(0)
        , _lastErrorMsg(NULL)
    {}
    
    /**
     * @brief Sets the error code and error message
     * @param[in] library OMR port library  
     */
    void populate(OMRPortLibrary* library) {
        OMRPORT_ACCESS_FROM_OMRPORT(library);

        _lastErrorCode = omrerror_last_error_number();
        _lastErrorMsg = omrerror_last_error_message();
    }
};

/**
 * @brief A class to manage shared caches on operating system level
 * @detail This class provides abstraction of a shared memory region and its control mutex.
 * @note This class declares virtual functions that are extended and defined by the persistent (Memory Mapped)
 * and non-persistent (Shared Memory) shared cache 
 */
class OMROSCache
{
protected:
    IDATA _corruptionCode;
    UDATA _corruptValue;

    OSCacheConfigOptions* _configOptions;

public:
     /**
     * @brief Constructor
     * param[in] configOptions OSCache config options object
     */
    OMROSCache(OSCacheConfigOptions* configOptions)
        : _configOptions(configOptions)
    {}

    /**
     * @brief Destructor
     */
    virtual ~OMROSCache() {}

    /**
    * @brief Returns the total size of the shared cache
    * @note This method is left for runtime developers to extend and implement it 
    * @return size of shared cache
    */
    virtual U_32 omr_getTotalSize() = 0;

    /**
    * @brief Function to issue an NLS error message when an error situation has occurred  
    * @detail It can optionally issue messages detailing the last error
    * recorded in the Port Library
    * @param [in]  moduleName The message module name
    * @param [in]  id The message id
    * @param [in]  lastErrorInfo Stores portable error number and message
    * @return void
    */
    virtual void omr_errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo) = 0;

    /**
    * @brief Function to find if the cache is accessible by current user or not
    * @return enum OMR_CacheAccess
    */
    virtual OMR_CacheAccess omr_isCacheAccessible(void) const
    {
        return OMR_CACHE_ACCESS_ALLOWED;
    }

    /**
     * @brief Get the corruption context (corruption code and corrupt value)
     * @detail Value returned is interpreted on the basis of _corruptionCode :
     * if corruptionCode = CACHE_CRC_INVALID, corruptValue contains cache CRC returned by getCacheCRC().
     * if corruptionCode = CACHE_HEADER_INCORRECT_DATA_LENGTH, corruptValue contains data length stored in cache header.
     * if corruptionCode = CACHE_HEADER_INCORRECT_DATA_START_ADDRESS, corruptValue is the address of cache data.
     * if corruptionCode = CACHE_HEADER_BAD_EYECATCHER, corruptValue is the address of eye-catcher string in cache header.
     * if corruptionCode = CACHE_HEADER_INCORRECT_CACHE_SIZE, corruptValue is the cache size stored in cache header.
     * if corruptionCode = ACQUIRE_HEADER_WRITE_LOCK_FAILED, corruptValue contains the error code for the failure to acquire lock.
     * if corruptionCode = CACHE_SIZE_INVALID, corruptValue contains invalid cache size.
     * @param [out] corruptionCode  if non-null, it is populated with a code indicating corruption type.
     * @param [out] corruptValue    if non-null, it is populated with a value to be interpreted depending on corruptionCode.
     * @return void
     */
    virtual void omr_getCorruptionContext(IDATA *corruptionCode, UDATA *corruptValue)
    {
        if (NULL != corruptionCode) {
            *corruptionCode = _corruptionCode;
        }

        if (NULL != corruptValue) {
            *corruptValue = _corruptValue;
        }
    }

    /**
    * @briefSets corruption context that specifies a corruption code and the corrupt value
    * @param [in] code	used to set OMROSCache::_corruptionCode
    * @param [in] value used to set OMROSCache::_corruptValue
    * @return void
    */
    virtual void omr_setCorruptionContext(IDATA corruptionCode, UDATA corruptValue)
    {
        _corruptionCode = corruptionCode;
        _corruptValue = corruptValue;
    }

    /**
    * @brief Method to get the instance of OSCacheRegionSerializer
    * @return new OSCacheRegionSerializer
    */
    virtual OSCacheRegionSerializer* omr_constructSerializer() = 0;
    
    /**
    * @brief Method to get the instance of OSCacheRegionInitializer
    * @return new OSCacheRegionInitializer
    */
    virtual OSCacheRegionInitializer* omr_constructInitializer() = 0;

    /**
    * @brief Function to attach the shared cache to a process
    * @return Pointer to the start of the cache on success, NULL on failure
    */
    virtual void* omr_attach() = 0;

    /**
    * @brief Function to create or open a shared cache
    * @param[in] cacheName The name of the cache to be opened/created
    * @param[in] ctrlDirName The directory for the cache file
    * @return true on success, false on failure
    */
    virtual bool omr_startup(const char* cacheName, const char* ctrlDirName) = 0;

    /**
    * @brief Function to clean up the resources and memory held by the object of this class 
    * @details This is virtual function that is extended and defined by the persistent(Memory Mapped) and 
    * non-persistent(Shared Memory) shared cache 
    * @return void
    */
    virtual void omr_cleanup() = 0;

    /**
    * @brief Function to destroy the shared cache
    * @param[in] suppressVerbose suppresses verbose output
    * @param[in] isReset True if reset option is being used, false otherwise
    * @return return 0 for success and -1 for failure
    */
    virtual IDATA omr_destroy(bool suppressVerbose, bool isReset = false) = 0;

    /**
    * @brief Function to return the object's error status code
    * @return return object's error code
    */
    virtual IDATA omr_getError() = 0;

    /**
    * @brief Function to perform processing that is required when the VM/runtime is exiting
    * @return void
    */
    virtual void omr_runExitProcedure() = 0;

    /**
    * @brief Function to return the locking capabilities of this shared cache implementation
    * @return OMROSCACHE_DATA_WRITE_LOCK | OMROSCACHE_DATA_READ_LOCK
    */
    virtual IDATA omr_getLockCapabilities() = 0;

    /**
    * @brief Function to set memory protections to the region
    * @details This function calls omr_setPermissions in order to set memory protections for OSCacheRegion
    * @param[in] region Instance of OSCacheRegion
    * @return 0 if the operations has been successful, -1 if an error has occured
    */
    virtual IDATA omr_setRegionPermissions(OSCacheRegion* region) = 0;

    /**
    * @brief Function to returns the minimum sized region of a shared classes cache on which the process can 
    * set permissions, in the number of bytes
    * @return the minimum size of region on which we can control permissions size or 0 if this is unsupported
    */
    virtual UDATA omr_getPermissionsRegionGranularity() = 0;
};
#endif /* !defined(OSCACHE_HPP_INCLUDED) */
