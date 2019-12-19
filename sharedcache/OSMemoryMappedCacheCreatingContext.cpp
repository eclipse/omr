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

#include "sharedconsts.h"
#include "shrnls.h"

#include "OSMemoryMappedCache.hpp"
#include "OSMemoryMappedCacheCreatingContext.hpp"
#include "OSMemoryMappedCacheHeader.hpp"
#include "OSMemoryMappedCacheUtils.hpp"

bool OSMemoryMappedCacheCreatingContext::initAttach(void* blockAddress, IDATA&)
{
  // *_cache->_config->getDataLengthFieldLocation() = _cache->_config->getDataSectionSize();
  //  *_cache->_config->getDataSectionLocation() = (void*) _cache->_config->getHeaderLocation() + _cache->_config->getHeaderSize();

  _cache->_config->serializeCacheLayout(_cache, blockAddress, _cache->_configOptions->cacheSize());

  return true;
}

bool OSMemoryMappedCacheCreatingContext::startup(IDATA& errorCode)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
  LastErrorInfo lastErrorInfo;

  // this is accessible through the Config object.
  //OSCachemmap_header_version_current *cacheHeader;
  IDATA rc;

  //creatingNewCache = true;

  /* File is wrong length, so we are creating the cache */
  Trc_SHR_OSC_Mmap_startup_fileCreated();

  /* We can't create the cache when we're running read-only */
  if (_cache->_runningReadOnly) {
    Trc_SHR_OSC_Mmap_startup_runningReadOnlyAndWrongLength();
    _cache->errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_OPENING_CACHE_READONLY, NULL);
    //goto _errorPostHeaderLock;
    return false;
  }

  /* Set cache to the correct length */
  if (!_cache->setCacheLength(&lastErrorInfo)) {
    // Trc_SHR_OSC_Mmap_startup_badSetCacheLength(piconfig->sharedClassCacheSize);
    _cache->errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_SETTING_CACHE_LENGTH, &lastErrorInfo);
    //goto _errorPostHeaderLock;
    return false;
  }

  // Trc_SHR_OSC_Mmap_startup_goodSetCacheLength(piconfig->sharedClassCacheSize);

  /* Verify if the group access has been set */
  if (_cache->_configOptions->groupAccessEnabled()) { //OMR_ARE_ALL_BITS_SET(_openMode, J9OSCACHE_OPEN_MODE_GROUPACCESS)) {
    I_32 groupAccessRc = OSMemoryMappedCacheUtils::verifyCacheFileGroupAccess(_cache->_portLibrary,
									      _cache->_config->_fileHandle,
									      &lastErrorInfo);

    if (0 == groupAccessRc) {
      Trc_SHR_OSC_Mmap_startup_setGroupAccessFailed(_cache->_cachePathName);
      OSC_WARNING_TRACE(_cache->_configOptions, J9NLS_SHRC_OSCACHE_MMAP_SET_GROUPACCESS_FAILED);
    } else if (-1 == groupAccessRc) {
      /* Failed to get stats of the cache file */
      Trc_SHR_OSC_Mmap_startup_badFileStat(_cache->_cachePathName);
      _cache->errorHandler(J9NLS_SHRC_OSCACHE_ERROR_FILE_STAT, &lastErrorInfo);
      // goto _errorPostHeaderLock;
      return false;
    }
  }

  return true;
}

bool OSMemoryMappedCacheCreatingContext::attach(IDATA& errorCode)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->portLibrary());
  IDATA rc = _cache->internalAttach(); 

  if (0 != rc) {
    errorCode = rc;
    Trc_SHR_OSC_Mmap_startup_badAttach();
    // goto _errorPostAttach;
    return false;
  }

  Trc_SHR_OSC_Mmap_startup_goodCreateCacheHeader();

  if (_cache->_configOptions->verboseEnabled()) {
    OSC_TRACE1(_cache->_configOptions, J9NLS_SHRC_OSCACHE_MMAP_STARTUP_CREATED, _cache->_cacheName);
  }

  _startupCompleted = true;
  return true;
}

bool OSMemoryMappedCacheCreatingContext::creatingNewCache() {
  return true;
}

/**
 * Method to create the cache header for a new persistent cache
 *
 * @param [in] cacheHeader  A pointer to the cache header
 * @param [in] versionData  The version data of the cache
 *
 * @return true on success, false on failure
 * THREADING: Pre-req caller holds the cache header write lock
 */
/*
I_32
OSMemoryMappedCacheCreatingContext::createCacheHeader()
{
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);
  //  U_32 headerLen = _cache->_config->getHeaderSize();  // MMAP_CACHEHEADERSIZE;

  OSMemoryMappedCacheHeader* cacheHeader = _cache->_config->_header;

  if(NULL == cacheHeader) {
    return false;
  }

  // commented out because we now lack versionData.
  // Trc_SHR_OSC_Mmap_createCacheHeader_Entry(cacheHeader, headerLen, versionData);
  cacheHeader->init(OMRPORTLIB); //initOSCacheHeader(&(cacheHeader->oscHdr), versionData, headerLen);

// commented out because we now lack some of this data. sort of. some of it is in fact
// contained inside the layout object. It will probably be written to the header. who knows.

//  Trc_SHR_OSC_Mmap_createCacheHeader_header(cacheHeader->eyecatcher,
//					    cacheHeader->oscHdr.size,
//					    cacheHeader->oscHdr.dataStart,
//					    cacheHeader->oscHdr.dataLength,
//					    cacheHeader->createTime,
//					    cacheHeader->lastAttachedTime);

  Trc_SHR_OSC_Mmap_createCacheHeader_Exit();
  return true;
}
*/
