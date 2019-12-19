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

#include "OSCacheLayout.hpp"
#include "OSMemoryMappedCache.hpp"
#include "OSCacheUtils.hpp"
#include "OSMemoryMappedCacheMemoryProtector.hpp"
#include "OSMemoryMappedCacheInitializer.hpp"
#include "OSMemoryMappedCacheSerializer.hpp"
#include "OSMemoryMappedCacheUtils.hpp"

#include "omrcfg.h"
#include "shrnls.h"
#include "ut_omrshr.h"

OSMemoryMappedCache::OSMemoryMappedCache(OMRPortLibrary* library,
					 char* cacheName,
					 char* ctrlDirName,
					 IDATA numLocks,
					 OSMemoryMappedCacheConfig* config,
					 OSCacheConfigOptions* configOptions)
  : OSCacheImpl(library, configOptions, numLocks, cacheName, ctrlDirName)
  , _config(config)
{
  //  an abstract class, so we don't instantiate _config directly.
  initialize();

  // expect the open mode has been set in the configOptions object already.
  // configOptions.setOpenMode(openMode);
}

OSMemoryMappedCache::OSMemoryMappedCache(const OSMemoryMappedCache& cache,
                                         char* cacheName,
                                         char* cacheLocation)
  : OSCacheImpl(cache._portLibrary,
                cache._configOptions,
                cache._numLocks,
                cacheName,
                cacheLocation)
  , _config(cache._config)
{
  commonInit();
}

void OSMemoryMappedCache::initialize()
{
  /* memForConstructor is used in J9 as a place to allocate the cache in. Now
  that we're in OMR, I don't what will take its place, if anything.
  Trc_SHR_OSC_Mmap_initialize_Entry(portLibrary, memForConstructor);
  */

  //TODO: set _portLibrary in commonInit.
  commonInit(); //, generation);

  _config->_fileHandle = -1;
  _config->_actualFileLength = 0;
  _config->_finalised = 0;

  _mapFileHandle = NULL;

  //TODO: shouldn't this be in the OSCache constructor? Seems both universal and benign in
  //its effects.
  _corruptionCode = NO_CORRUPTION;
  _corruptValue = NO_CORRUPTION;

  _config->_cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_ALLOWED;
}

void OSMemoryMappedCache::finalise()
{
  //Trc_SHR_OSC_Mmap_finalise_Entry();

  commonCleanup();

  _config->_fileHandle = -1;
  _config->_actualFileLength = 0;
  _config->_finalised = 1;

  _mapFileHandle = NULL;

  omrthread_t self;
  omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

  for (UDATA i = 0; i < _config->_numLocks; i++) { // OMRSH_OSCACHE_MMAP_LOCK_COUNT; i++) {
    if (NULL != _config->_lockMutex[i]) {
       omrthread_monitor_destroy(_config->_lockMutex[i]);
    }
  }

  omrthread_detach(self);

  Trc_SHR_OSC_Mmap_finalise_Exit();
}

bool OSMemoryMappedCache::startup(const char* cacheName, const char* ctrlDirName)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  I_32 mmapCapabilities = omrmmap_capabilities();
  LastErrorInfo lastErrorInfo;
  IDATA errorCode = OMRSH_OSCACHE_FAILURE;
  struct J9FileStat statBuf;

  // we need both writing and synchronization capabilities in order to
  // have a memory-mapped cache.
  if (!(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_WRITE) || !(mmapCapabilities & OMRPORT_MMAP_CAPABILITY_MSYNC))
  {
    Trc_SHR_OSC_Mmap_startup_nommap(mmapCapabilities);
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_MMAPCAP, NULL);
    goto _errorPreFileOpen;
  }

  // this is how commonStartup was broken up, into these next two blocks.
  if (initCacheDirName(ctrlDirName) != 0) {
     Trc_SHR_OSC_Mmap_startup_commonStartupFailure();
     goto _errorPreFileOpen;
  }

  if (initCacheName(cacheName) != 0) {
     goto _errorPreFileOpen;
  }

  if (!(_cachePathName = (char*)omrmem_allocate_memory(OMRSH_MAXPATH, OMRMEM_CATEGORY_CLASSES))) {
     return false;
  }

  strcpy(_cachePathName, _cacheLocation);
  strcat(_cachePathName, _cacheName);

  Trc_SHR_OSC_Mmap_startup_commonStartupSuccess();

  /* Detect remote filesystem */
  if (_configOptions->usingNetworkCache()) { // openMode & J9OSCACHE_OPEN_MODE_CHECK_NETWORK_CACHE) {
    // _cacheLocation, or _cacheDirName as it was called, is initialized inside commonStartup.
    if (0 == omrfile_stat(_cacheLocation, 0, &statBuf)) {
      if (statBuf.isRemote) {
	Trc_SHR_OSC_Mmap_startup_detectedNetworkCache();
	errorHandler(J9NLS_SHRC_OSCACHE_MMAP_NETWORK_CACHE, NULL);
	goto _errorPreFileOpen;
      }
    }
  }

  if (_configOptions->openToStatExistingCache()) {
     if (OSCacheUtils::statCache(OMRPORTLIB, _cacheLocation, _cacheName, "The SOM shared cache doesn't exist!") != 1)
     {
        goto _errorPreFileOpen;
     }
  }

  /* Open the file */
  if (!openCacheFile(&lastErrorInfo)) { // _createFlags & OMRSH_OSCACHE_CREATE, &lastErrorInfo)) {
    Trc_SHR_OSC_Mmap_startup_badfileopen(_cachePathName);
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_FILEOPEN_ERROR, &lastErrorInfo);  /* TODO: ADD FILE NAME */
    goto _errorPostFileOpen;
  }

  Trc_SHR_OSC_Mmap_startup_goodfileopen(_cachePathName, _config->_fileHandle);

  /* Avoid any checks for cache file access if
   * - user has specified a cache directory, or
   * - destroying an existing cache (if SHR_STARTUP_REASON_DESTROY or SHR_STARTUP_REASON_EXPIRE or OMRSH_OSCACHE_OPEXIST_DESTROY is set)
   */
  if (!_configOptions->isUserSpecifiedCacheDir()
      && !_configOptions->openToDestroyExistingCache()
      && !_configOptions->openToDestroyExpiredCache())
  {
    _config->_cacheFileAccess =
      OSCacheUtils::checkCacheFileAccess(_portLibrary, _config->_fileHandle,
                                         _configOptions, &lastErrorInfo);

    if (_configOptions->openToStatExistingCache() || _config->cacheFileAccessAllowed())
    {
      Trc_SHR_OSC_Mmap_startup_fileaccessallowed(_cachePathName);
    } else {
      // handle cache access errors.
      switch(_config->_cacheFileAccess) {
      case OMRSH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED:
	errorHandler(J9NLS_SHRC_OSCACHE_MMAP_GROUPACCESS_REQUIRED, NULL);
	goto _errorPostFileOpen;
	break;
      case OMRSH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED:
	errorHandler(J9NLS_SHRC_OSCACHE_MMAP_OTHERS_ACCESS_NOT_ALLOWED, NULL);
	goto _errorPostFileOpen;
	break;
      case OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED:
	errorHandler(J9NLS_SHRC_OSCACHE_MMAP_INTERNAL_ERROR_CHECKING_CACHEFILE_ACCESS, &lastErrorInfo);
	goto _errorPostFileOpen;
	break;
      default:
	Trc_SHR_Assert_ShouldNeverHappen();
      }
    }
  }

  if(_configOptions->openToDestroyExistingCache()) {
    Trc_SHR_OSC_Mmap_startup_openCacheForDestroy(_cachePathName);
    goto _exitForDestroy;
  }

  omrthread_attach_ex(&self, J9THREAD_ATTR_DEFAULT);

  for (UDATA i = 0; i < _numLocks; i++) { // OMRSH_OSCACHE_MMAP_LOCK_COUNT; i++) {
    if (omrthread_monitor_init_with_name(&_config->_lockMutex[i], 0, "Persistent shared classes lock mutex")) {
      Trc_SHR_OSC_Mmap_startup_failed_mutex_init(i);
      goto _errorPostFileOpen;
    }
  }

  Trc_SHR_OSC_Mmap_startup_initialized_mutexes();

  /* Get cache header write lock */
  if (-1 == _config->acquireHeaderWriteLock(_portLibrary, _runningReadOnly, &lastErrorInfo)) {
    Trc_SHR_OSC_Mmap_startup_badAcquireHeaderWriteLock();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ACQUIREHEADERWRITELOCK_ERROR, &lastErrorInfo);
    errorCode = OMRSH_OSCACHE_CORRUPT;

    OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_CORRUPT_ACQUIRE_HEADER_WRITE_LOCK_FAILED, lastErrorInfo._lastErrorCode);
    setCorruptionContext(ACQUIRE_HEADER_WRITE_LOCK_FAILED, (UDATA)lastErrorInfo._lastErrorCode);
    goto _errorPostHeaderLock;
  }

  Trc_SHR_OSC_Mmap_startup_goodAcquireHeaderWriteLock();

  errorCode = OMRSH_OSCACHE_FAILURE;

  U_32 cacheSize;

  /* Check the length of the file */
#if defined(WIN32) || defined(WIN64)
  if ((cacheSize = (U_32) omrfile_blockingasync_flength(_config->_fileHandle)) > 0) {
#else
  if ((cacheSize = (U_32) omrfile_flength(_config->_fileHandle)) > 0) {
#endif
    // we are attaching to an existing cache.
    _initContext = new (PERSISTENT_NEW) OSMemoryMappedCacheAttachingContext(this);

    if(!_initContext->startup(errorCode)) {
      goto _errorPostAttach;
    }
  } else {
    _initContext = new (PERSISTENT_NEW) OSMemoryMappedCacheCreatingContext(this);

    if(!_initContext->startup(errorCode)) {
      goto _errorPostHeaderLock;
    } else if(!_initContext->attach(errorCode)) {
      goto _errorPostAttach;
    } else {
      _config->setCacheInitComplete();
	//this line replaces this:
	/*
	if (creatingNewCache) {
	  OSCachemmap_header_version_current *cacheHeader = (OSCachemmap_header_version_current *)_headerStart;

	  cacheHeader->oscHdr.cacheInitComplete = 1;
	}
	*/
    }
  }

  /* Release cache header write lock */
   if (0 != _config->releaseHeaderWriteLock(_portLibrary, _runningReadOnly, &lastErrorInfo)) {
      Trc_SHR_OSC_Mmap_startup_badReleaseHeaderWriteLock();
      errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_RELEASING_HEADER_WRITE_LOCK, &lastErrorInfo);
      goto _errorPostFileOpen;
   }

   Trc_SHR_OSC_Mmap_startup_goodReleaseHeaderWriteLock();

_exitForDestroy:
  _config->_finalised = 0;
  Trc_SHR_OSC_Mmap_startup_Exit();
  return true;
_errorPostAttach :
  internalDetach();//_activeGeneration);
_errorPostHeaderLock :
  _config->releaseHeaderWriteLock(OMRPORTLIB, _runningReadOnly, NULL);//_activeGeneration, NULL);
_errorPostFileOpen :
  closeCacheFile();
  if (_initContext->creatingNewCache()) {
    deleteCacheFile(NULL);
  }
_errorPreFileOpen :
  setError(errorCode);
  return false;
}

bool
OSMemoryMappedCache::setCacheLength(LastErrorInfo* lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  U_32 cacheSize = _configOptions->cacheSize();

  Trc_SHR_OSC_Mmap_setCacheLength_Entry(cacheSize);

  if (NULL != lastErrorInfo) {
    lastErrorInfo->_lastErrorCode = 0;
  }

  if (cacheSize < _config->getHeaderSize()) {
    return false;
  }

#if defined(WIN32) || defined(WIN64)
  if (0 != omrfile_blockingasync_set_length(_config->_fileHandle, cacheSize)) {
#else
  if (0 != omrfile_set_length(_config->_fileHandle, cacheSize)) {
#endif
    LastErrorInfo localLastErrorInfo;
    localLastErrorInfo.populate(OMRPORTLIB);
//    localLastErrorInfo.lastErrorCode = j9error_last_error_number();
//    localLastErrorInfo.lastErrorMsg = j9error_last_error_message();
    Trc_SHR_OSC_Mmap_setCacheLength_badfilesetlength();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_SETCACHELENGTH_FILESETLENGTH, &localLastErrorInfo);
    if (NULL != lastErrorInfo) {
      memcpy(lastErrorInfo, &localLastErrorInfo, sizeof(LastErrorInfo));
    }

    return false;
  }

  Trc_SHR_OSC_Mmap_setCacheLength_goodfilesetlength();

  _configOptions->setCacheSize(cacheSize);

  Trc_SHR_OSC_Mmap_setCacheLength_Exit();
  return true;
}

bool
OSMemoryMappedCache::openCacheFile(LastErrorInfo* lastErrorInfo)
{
  bool result = true;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  //TODO: _openMode should be fed into the ConfigOptions object, subclassed for MemoryMappedCache's.
  //(_openMode & J9OSCACHE_OPEN_MODE_DO_READONLY) ? EsOpenRead : (EsOpenRead | EsOpenWrite);
  I_32 openFlags = _configOptions->readOnlyOpenMode() ? EsOpenRead: (EsOpenRead | EsOpenWrite);
  I_32 fileMode  = _configOptions->fileMode();

  // Trc_SHR_OSC_Mmap_openCacheFile_entry();

  if(_configOptions->createFile() && (openFlags & EsOpenWrite)) {
    openFlags |= EsOpenCreate;
  }

  for(IDATA i = 0; i < 2; i++) {
#if defined(WIN32) || defined(WIN64)
    _config->_fileHandle = omrfile_blockingasync_open(_cachePathName, openFlags, fileMode);
#else
    _config->_fileHandle = omrfile_open(_cachePathName, openFlags, fileMode);
#endif

    if ((_config->_fileHandle == -1) && (openFlags != EsOpenRead) && _configOptions->tryReadOnlyOnOpenFailure()) {
      openFlags &= ~EsOpenWrite;
      continue;
    }

    break;
  }

  if (-1 == _config->_fileHandle) {
    if (NULL != lastErrorInfo) {
      lastErrorInfo->populate(_portLibrary);
    }
    Trc_SHR_OSC_Mmap_openCacheFile_failed();
    result = false;
  } else if ((openFlags & (EsOpenRead | EsOpenWrite)) == EsOpenRead) {
    Trc_SHR_OSC_Event_OpenReadOnly();
    _runningReadOnly = true;
  }

  Trc_SHR_OSC_Mmap_openCacheFile_exit();
  return result;
}

bool OSMemoryMappedCache::closeCacheFile()
{
  bool result = true;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  Trc_SHR_Assert_Equals(_config->getHeaderLocation(), NULL);
  Trc_SHR_Assert_Equals(_config->getDataSectionLocation(), NULL);

  if(-1 == _config->_fileHandle) {
    return true;
  }
  Trc_SHR_OSC_Mmap_closeCacheFile_entry();
#if defined(WIN32) || defined(WIN64)
  if(-1 == omrfile_blockingasync_close(_config->_fileHandle)) {
#else
  if(-1 == omrfile_close(_config->_fileHandle)) {
#endif
    Trc_SHR_OSC_Mmap_closeCacheFile_failed();
    result = false;
  }

  _config->_fileHandle = -1;
  // _startupCompleted = false;

  Trc_SHR_OSC_Mmap_closeCacheFile_exit();
  return result;
}

/*
 * This function performs enough of an attach to start the cache, but nothing more
 * The internalDetach function is the equivalent for detach
 * isNewCache should be true if we're attaching to a completely uninitialized cache, false otherwise
 * THREADING: Pre-req caller holds the cache header write lock
 *
 * Needs to be able to work with all generations
 *
 * @param [in] isNewCache true if the cache is new and we should calculate cache size using the file size;
 * 				false if the cache is pre-existing and we can read the size fields from the cache header
 * @param [in] generation The generation of the cache header to use when calculating the lock offset
 *
 * @return 0 on success, OMRSH_OSCACHE_FAILURE on failure, OMRSH_OSCACHE_CORRUPT for corrupt cache
 */

/* additions: isNewCache is a question answered by the OSMemoryMappedInitializationContext object.
   Similar, the generation is determined by the concrete type of _header, inside _config.. so neither
   of these parameters is needed. */
IDATA OSMemoryMappedCache::internalAttach() //bool isNewCache, UDATA generation)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  U_32 accessFlags = _runningReadOnly ? OMRPORT_MMAP_FLAG_READ : OMRPORT_MMAP_FLAG_WRITE;
  LastErrorInfo lastErrorInfo;
  IDATA rc = OMRSH_OSCACHE_FAILURE;

  Trc_SHR_OSC_Mmap_internalAttach_Entry();

  /* Get current length of file */
  accessFlags |= OMRPORT_MMAP_FLAG_SHARED;

  _config->_actualFileLength = _configOptions->cacheSize();
  Trc_SHR_Assert_True(_config->_actualFileLength > 0);

  if (0 != _config->acquireAttachReadLock(OMRPORTLIB, &lastErrorInfo)) {
    Trc_SHR_OSC_Mmap_internalAttach_badAcquireAttachedReadLock();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_ACQUIRING_ATTACH_READ_LOCK, &lastErrorInfo);
    rc = OMRSH_OSCACHE_FAILURE;
    goto error;
  }

  Trc_SHR_OSC_Mmap_internalAttach_goodAcquireAttachReadLock();

#ifndef WIN32
  {
    J9FileStatFilesystem fileStatFilesystem;
    /* check for free disk space */
    rc = omrfile_stat_filesystem(_cachePathName, 0, &fileStatFilesystem);
    if (0 == rc) {
      if (fileStatFilesystem.freeSizeBytes < (U_64)_config->_actualFileLength) {
	OSC_ERR_TRACE2(_configOptions, J9NLS_SHRC_OSCACHE_MMAP_DISK_FULL, (U_64)fileStatFilesystem.freeSizeBytes, (U_64)_config->_actualFileLength);
	rc = OMRSH_OSCACHE_FAILURE;
	goto error;
      }
    }
  }
#endif

  /* Map the file */
  _mapFileHandle = omrmmap_map_file(_config->_fileHandle, 0, (UDATA)_config->_actualFileLength, _cachePathName, accessFlags, OMRMEM_CATEGORY_CLASSES_SHC_CACHE);
  if ((NULL == _mapFileHandle) || (NULL == _mapFileHandle->pointer)) {
    lastErrorInfo.populate(_portLibrary);
    /*
      lastErrorInfo.lastErrorCode = omrerror_last_error_number();
      lastErrorInfo.lastErrorMsg = omrerror_last_error_message();
    */
    Trc_SHR_OSC_Mmap_internalAttach_badmapfile();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_ATTACH_ERROR_MAPPING_FILE, &lastErrorInfo);
    rc = OMRSH_OSCACHE_FAILURE;
    goto error;
  }

  _configOptions->setCacheSize(_mapFileHandle->size);

  // Trc_SHR_OSC_Mmap_internalAttach_goodmapfile(_layout->_headerStart);

  if(!_initContext->initAttach(_mapFileHandle->pointer, rc)) {
    goto error;
  }

  // Trc_SHR_OSC_Mmap_internalAttach_Exit(_dataStart, sizeof(OSCachemmap_header_version_current));
  return 0;

 error:
  internalDetach();
  return rc;
}

/**
 * Method to detach a persistent cache from the process
 */
void
OSMemoryMappedCache::detach()
{
  if (_config->acquireHeaderWriteLock(_portLibrary, _runningReadOnly, NULL) != -1) {
    _config->updateLastDetachedTime(_portLibrary, _runningReadOnly);

    if (_config->releaseHeaderWriteLock(_portLibrary, _runningReadOnly, NULL) == -1) {
      OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
      I_32 myerror = omrerror_last_error_number();

      Trc_SHR_OSC_Mmap_detach_releaseHeaderWriteLock_Failed(myerror);
      Trc_SHR_Assert_ShouldNeverHappen();
    }
  } else {
    OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
    I_32 myerror = omrerror_last_error_number();

    Trc_SHR_OSC_Mmap_detach_acquireHeaderWriteLock_Failed(myerror);
    Trc_SHR_Assert_ShouldNeverHappen();
  }

  internalDetach();
}

/* Perform enough work to detach from the cache after having called internalAttach */
void OSMemoryMappedCache::internalDetach()
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  Trc_SHR_OSC_Mmap_internalDetach_Entry();

  if (NULL == _config->getHeaderLocation()) { //_layout->_headerStart) {
    Trc_SHR_OSC_Mmap_internalDetach_notattached();
    return;
  }

  if (_mapFileHandle) {
    omrmmap_unmap_file(_mapFileHandle);
    _mapFileHandle = NULL;
  }

  if (0 != _config->releaseAttachReadLock(OMRPORTLIB)) {
    Trc_SHR_OSC_Mmap_internalDetach_badReleaseAttachReadLock();
  }
  Trc_SHR_OSC_Mmap_internalDetach_goodReleaseAttachReadLock();

  _config->detachRegions();
//  _config->_layout->_headerStart = NULL;
//  _config->getDataSectionLocation() = NULL;
//  _config->getDataSectionSize() = 0;
  /* The member variable '_actualFileLength' is not set to zero b/c
   * the cache size may be needed to reset the cache (e.g. in the
   * case of a build id mismatch, the cache may be reset, and
   * ::getTotalSize() may be called to ensure the new cache is the
   * same size).
   */

  Trc_SHR_OSC_Mmap_internalDetach_Exit(_config->getHeaderLocation(),
				       _config->getDataSectionLocation(),
				       _config->getDataSectionSize());
  return;
}

/**
 * Destroy a persistent shared classes cache
 *
 * @param[in] suppressVerbose suppresses verbose output
 * @param[in] isReset True if reset option is being used, false otherwise.
 *
 * This method detaches from the cache, checks whether it is in use by any other
 * processes and if not, deletes it from the filesystem
 *
 * @return 0 for success and -1 for failure
 */
IDATA
OSMemoryMappedCache::destroy(bool suppressVerbose, bool isReset)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  IDATA returnVal = -1; 		/* Assume failure */
  LastErrorInfo lastErrorInfo;

  Trc_SHR_OSC_Mmap_destroy_Entry();

  if (_config->getHeaderLocation() != NULL) {
    detach();
  }

  if (!closeCacheFile()) {
    Trc_SHR_OSC_Mmap_destroy_closefilefailed();
    goto _done;
  }

  _mapFileHandle = 0;
  _config->_actualFileLength = 0;

  Trc_SHR_OSC_Mmap_destroy_deletingfile(_cachePathName);
  if (!deleteCacheFile(&lastErrorInfo)) {
    Trc_SHR_OSC_Mmap_destroy_badunlink();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_DESTROY_ERROR_DELETING_FILE, &lastErrorInfo);
    goto _done;
  }

  Trc_SHR_OSC_Mmap_destroy_goodunlink();

  if (!suppressVerbose && _configOptions->verboseEnabled()) {
    // the isReset flag is only used for toggling trace messages. That's it.
    if (isReset) {
      OSC_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_MMAP_DESTROY_SUCCESS, _cacheName);
    }
  }

  Trc_SHR_OSC_Mmap_destroy_finalising();
  finalise();

  returnVal = 0;
  Trc_SHR_OSC_Mmap_destroy_Exit();

_done :
  return returnVal;
}

/**
 * Method to update the cache's last detached time, detach it from the
 * process and clean up the object's resources.  It is called when the
 * cache is no longer required by the JVM.
 */
void
OSMemoryMappedCache::cleanup()
{
  Trc_SHR_OSC_Mmap_cleanup_Entry();

  if (_config->_finalised) {
    Trc_SHR_OSC_Mmap_cleanup_alreadyfinalised();
    return;
  }

  if (_config->getHeaderLocation()) {
    if (_config->acquireHeaderWriteLock(_portLibrary, _runningReadOnly, NULL) != -1) {//_activeGeneration, NULL) != -1) {
      if (_config->updateLastDetachedTime(_portLibrary, _runningReadOnly)) {
	Trc_SHR_OSC_Mmap_cleanup_goodUpdateLastDetachedTime();
      } else {
	Trc_SHR_OSC_Mmap_cleanup_badUpdateLastDetachedTime();
	errorHandler(J9NLS_SHRC_OSCACHE_MMAP_CLEANUP_ERROR_UPDATING_LAST_DETACHED_TIME, NULL);
      }

      if (_config->releaseHeaderWriteLock(_portLibrary, _runningReadOnly, NULL) == -1) {
	OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
	I_32 myerror = omrerror_last_error_number();

	Trc_SHR_OSC_Mmap_cleanup_releaseHeaderWriteLock_Failed(myerror);
	Trc_SHR_Assert_ShouldNeverHappen();
      }
    } else {
      OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
      I_32 myerror = omrerror_last_error_number();
      Trc_SHR_OSC_Mmap_cleanup_acquireHeaderWriteLock_Failed(myerror);
      Trc_SHR_Assert_ShouldNeverHappen();
    }
  }

  if (_config->getHeaderLocation()) {
    detach();
  }

  if (_config->_fileHandle != -1) {
    closeCacheFile();
  }

  finalise();

  Trc_SHR_OSC_Mmap_cleanup_Exit();
}

/**
 * Function to attach a persistent shared classes cache to the process
 * Function performs version checking on the cache if the version data is provided
 *
 * @param [in] expectedVersionData  If not NULL, function checks the version data of the cache against the values in this struct
 *
 * @return Pointer to the start of the cache data area on success, NULL on failure
 */
/* Again, the currentThread in this function == vestigial by all appearances. */
void *
OSMemoryMappedCache::attach()//OMR_VMThread *currentThread, J9PortShcVersion* expectedVersionData)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
  // Again, a J9 specific thing.
  // OSCachemmap_header_version_current *cacheHeader;

  IDATA headerRc;
  LastErrorInfo lastErrorInfo;
  // OMR_VM *vm = currentThread->_vm;
  bool doRelease = false;
  IDATA rc;

  // Another J9 specific thing.
  // Trc_SHR_OSC_Mmap_attach_Entry1(UnitTest::unitTest);

  /* If we are already attached, just return */
  if (_config->getDataSectionLocation()) {
    Trc_SHR_OSC_Mmap_attach_alreadyattached(_config->getHeaderLocation(),
					    _config->getDataSectionLocation(),
					    _config->getDataSectionSize());
    return _config->getDataSectionLocation();
  }

  if (_config->acquireHeaderWriteLock(_portLibrary, _runningReadOnly, &lastErrorInfo) == -1) { //_activeGeneration, &lastErrorInfo) == -1) {
    Trc_SHR_OSC_Mmap_attach_acquireHeaderLockFailed();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_ATTACH_ACQUIREHEADERWRITELOCK_ERROR, &lastErrorInfo);
    return NULL;
  }

  doRelease = true;

  rc = internalAttach(); //false, _activeGeneration);
  if (0 != rc) {
    setError(rc);
    Trc_SHR_OSC_Mmap_attach_internalAttachFailed2();
    /* We've already detached, so just release the header write lock and exit */
    goto release;
  }

  // cacheHeader = (OSCachemmap_header_version_current *)_headerStart;

  /* Verify the header */
  if ((headerRc = verifyCacheHeader()) != OMRSH_OSCACHE_HEADER_OK) {
    handleCacheHeaderCorruption(headerRc);
    goto detach;
  }

  Trc_SHR_OSC_Mmap_attach_validCacheHeader();

  if (!_config->updateLastAttachedTime(_portLibrary, _runningReadOnly)) {
    Trc_SHR_OSC_Mmap_attach_badupdatelastattachedtime2();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_UPDATING_LAST_ATTACHED_TIME, NULL);
    setError(OMRSH_OSCACHE_FAILURE);

    goto detach;
  }

  Trc_SHR_OSC_Mmap_attach_goodupdatelastattachedtime();

  if (_config->releaseHeaderWriteLock(_portLibrary, _runningReadOnly, &lastErrorInfo) == -1) {//_activeGeneration, &lastErrorInfo) == -1) {
    Trc_SHR_OSC_Mmap_attach_releaseHeaderLockFailed2();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_ATTACH_ERROR_RELEASING_HEADER_WRITE_LOCK, &lastErrorInfo);
    /* doRelease set to false so we do not try to call release more than once which has failed in this block */
    doRelease = false;
    goto detach;
  }

  if (_configOptions->verboseEnabled() && _initContext->startupCompleted()) {
    OSC_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_MMAP_ATTACH_ATTACHED, _cacheName);
  }

  Trc_SHR_OSC_Mmap_attach_Exit(_config->getDataSectionLocation());
  return _config->getDataSectionLocation();

detach:
  internalDetach();//_activeGeneration);
release:
  if ((doRelease) && (_config->releaseHeaderWriteLock(_portLibrary, _runningReadOnly, &lastErrorInfo) == -1)) {
    Trc_SHR_OSC_Mmap_attach_releaseHeaderLockFailed2();
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_ATTACH_ERROR_RELEASING_HEADER_WRITE_LOCK, &lastErrorInfo);
  }

  Trc_SHR_OSC_Mmap_attach_ExitWithError();
  return NULL;
}

// a virtual function to emit trace messages/handle errors in response to detected
// header corruption. it's virtual so that attach, which uses it, becomes a template function.
// that way, the J9 cache can insert the commented out code below.
void OSMemoryMappedCache::handleCacheHeaderCorruption(IDATA headerRc)
{
  if (headerRc == OMRSH_OSCACHE_HEADER_CORRUPT) {
      Trc_SHR_OSC_Mmap_attach_corruptCacheHeader2();
      /* Cache is corrupt, trigger hook to generate a system dump.
       * This is the last chance to get corrupt cache image in system dump.
       * After this point, cache is detached.
       */
      /*
      if (_configOptions->disableCorruptCacheDumps()) {//0 ==(_runtimeFlags & OMRSHR_RUNTIMEFLAG_DISABLE_CORRUPT_CACHE_DUMPS)) {
	//				TRIGGER_J9HOOK_VM_CORRUPT_CACHE(vm->hookInterface, currentThread);
      }
      */
      setError(OMRSH_OSCACHE_CORRUPT);
 // } else if (headerRc == OMRSH_OSCACHE_HEADER_DIFF_BUILDID) {

      //a J9 specific thing:

      //Trc_SHR_OSC_Mmap_attach_differentBuildID();
      //setError(OMRSH_OSCACHE_DIFF_BUILDID);
  } else {
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_STARTUP_ERROR_INVALID_CACHE_HEADER, NULL);
    Trc_SHR_OSC_Mmap_attach_invalidCacheHeader2();
    setError(OMRSH_OSCACHE_FAILURE);
  }
}

/**
 * Method to perform processing required when the JVM is exiting
 *
 * Note:  The VM requires that memory should not be freed during
 * 			exit processing
 */
void
OSMemoryMappedCache::runExitProcedure()
{
  Trc_SHR_OSC_Mmap_runExitCode_Entry();

  if (_config->acquireHeaderWriteLock(_portLibrary, _runningReadOnly, NULL) != -1) {
    if (_config->updateLastDetachedTime(_portLibrary, _runningReadOnly)) {
      Trc_SHR_OSC_Mmap_runExitCode_goodUpdateLastDetachedTime();
    } else {
      Trc_SHR_OSC_Mmap_runExitCode_badUpdateLastDetachedTime();
      errorHandler(J9NLS_SHRC_OSCACHE_MMAP_CLEANUP_ERROR_UPDATING_LAST_DETACHED_TIME, NULL);
    }
    _config->releaseHeaderWriteLock(_portLibrary, _runningReadOnly, NULL);
  } else {
    OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);
    I_32 myerror = omrerror_last_error_number();

    Trc_SHR_OSC_Mmap_runExitCode_acquireHeaderWriteLock_Failed(myerror);
    Trc_SHR_Assert_ShouldNeverHappen();
  }

  Trc_SHR_OSC_Mmap_runExitCode_Exit();
}

//TODO: determine whether this will instead deal with OSCacheRegion's.
#if defined (J9SHR_MSYNC_SUPPORT)
IDATA syncUpdates(void* start, UDATA length, U_32 flags)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  IDATA rc;

  Trc_SHR_OSC_Mmap_syncUpdates_Entry(start, length, flags);

  rc = omrmmap_msync(start, length, flags);
  if (-1 == rc) {
    Trc_SHR_OSC_Mmap_syncUpdates_badmsync();
    return -1;
  }

  Trc_SHR_OSC_Mmap_syncUpdates_goodmsync();

  Trc_SHR_OSC_Mmap_syncUpdates_Exit();
  return 0;
}
#endif

/**
 * Return the locking capabilities of this shared classes cache implementation
 *
 * Read and write locks are supported for this implementation
 *
 * @return OMROSCACHE_DATA_WRITE_LOCK | OMROSCACHE_DATA_READ_LOCK
 */
IDATA
OSMemoryMappedCache::getLockCapabilities()
{
  return J9OSCACHE_DATA_WRITE_LOCK | J9OSCACHE_DATA_READ_LOCK;
}

/**
 * Returns the minimum sized region of a shared classes cache on which the process can set permissions, in the number of bytes.
 *
 * @param[in] portLibrary An instance of portLibrary
 *
 * @return the minimum size of region on which we can control permissions size or 0 if this is unsupported
 */
UDATA
OSMemoryMappedCache::getPermissionsRegionGranularity()
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  /* The code below is used as is in SH_CompositeCacheImpl::initialize()
   * for initializing SH_CompositeCacheImpl::_osPageSize during unit testing.
   */

  /* This call to capabilities is arguably unnecessary, but it is a good check to do */
  if (omrmmap_capabilities() & OMRPORT_MMAP_CAPABILITY_PROTECT) {
    return omrmmap_get_region_granularity(_config->getDataSectionLocation());
  }

  return 0;
}

/**
 * Delete the cache file
 *
 * @return true on success, false on failure
 */
bool
OSMemoryMappedCache::deleteCacheFile(LastErrorInfo *lastErrorInfo)
{
  bool result = true;
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  Trc_SHR_OSC_Mmap_deleteCacheFile_entry();

  if (NULL != lastErrorInfo) {
    lastErrorInfo->_lastErrorCode = 0;
  }

  if (-1 == omrfile_unlink(_cachePathName)) {
    I_32 errorCode = omrerror_last_error_number();

    if (OMRPORT_ERROR_FILE_NOENT != errorCode) {
      if (NULL != lastErrorInfo) {
	lastErrorInfo->populate(OMRPORTLIB);
//	      lastErrorInfo->lastErrorCode = errorCode;
//	      lastErrorInfo->lastErrorMsg = omrerror_last_error_message();
      }
      Trc_SHR_OSC_Mmap_deleteCacheFile_failed();
      result = false;
    }
  }

  Trc_SHR_OSC_Mmap_deleteCacheFile_exit();
  return result;
}

/**
 * Method to set the object's error status code
 *
 * @param [in]  errorCode The error code to be set
 *
 */
void
OSMemoryMappedCache::setError(IDATA errorCode)
{
  Trc_SHR_OSC_Mmap_setError_Entry(errorCode);
  _errorCode = errorCode;
  Trc_SHR_OSC_Mmap_setError_Exit(_errorCode);
}

/**
 * Method to return the object's error status code
 *
 * Note;  This method of error reporting is for consistency with SH_OSCachesysv
 *
 * @return The object's error code
 */
IDATA
OSMemoryMappedCache::getError()
{
  Trc_SHR_OSC_Mmap_getError_Event(_errorCode);
  return _errorCode;
}

/**
 * Method to issue an NLS error message hen an error situation has occurred.  It
 * can optional issue messages detailing the last error recorded in the Port Library.
 *
 * @param [in]  moduleName The message module name
 * @param [in]  id The message id
 * @param [in]  lastErrorInfo Stores portable error number and message
 */
void
OSMemoryMappedCache::errorHandler(U_32 moduleName, U_32 id, LastErrorInfo *lastErrorInfo)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  if ((NULL != lastErrorInfo) && (0 != lastErrorInfo->_lastErrorCode)) {
    Trc_SHR_OSC_Mmap_errorHandler_Entry_V2(moduleName, id, lastErrorInfo->_lastErrorCode, lastErrorInfo->_lastErrorMsg);
  } else {
    Trc_SHR_OSC_Mmap_errorHandler_Entry_V2(moduleName, id, 0, "");
  }

  UDATA verboseFlags = _configOptions->renderVerboseOptionsToFlags();

  if(moduleName && id && verboseFlags) {
    Trc_SHR_OSC_Mmap_errorHandler_printingMessage(verboseFlags);
    omrnls_printf(J9NLS_ERROR, moduleName, id);
    if ((NULL != lastErrorInfo) && (0 != lastErrorInfo->_lastErrorCode)) {
      I_32 errorno = lastErrorInfo->_lastErrorCode;
      const char* errormsg = lastErrorInfo->_lastErrorMsg;

      Trc_SHR_OSC_Mmap_errorHandler_printingPortMessages();
      omrnls_printf(J9NLS_ERROR, J9NLS_SHRC_OSCACHE_PORT_ERROR_NUMBER, errorno);
      Trc_SHR_Assert_True(errormsg != NULL);
      omrnls_printf(J9NLS_ERROR, J9NLS_SHRC_OSCACHE_PORT_ERROR_MESSAGE, errormsg);
    }
  } else {
    Trc_SHR_OSC_Mmap_errorHandler_notPrintingMessage(verboseFlags);
  }

  Trc_SHR_OSC_Mmap_errorHandler_Exit();
}

OSCacheMemoryProtector*
OSMemoryMappedCache::constructMemoryProtector()
{
  return new (PERSISTENT_NEW) OSMemoryMappedCacheMemoryProtector(*this);
}

OSCacheRegionSerializer*
OSMemoryMappedCache::constructSerializer()
{
  return new (PERSISTENT_NEW) OSMemoryMappedCacheSerializer(_portLibrary);
}

OSCacheRegionInitializer*
OSMemoryMappedCache::constructInitializer()
{
  return new (PERSISTENT_NEW) OSMemoryMappedCacheInitializer(_portLibrary);
}

bool
OSMemoryMappedCache::checkTime(U_64 moduleTime)
{
  I_64* cacheTime = _config->getLastAttachTimeLocation();
  return moduleTime < *cacheTime;
}

/**
 * Method to determine whether a persistent cache's header is valid
 *
 * @param [in] headerArg  A pointer to the cache header
 * @param [in] versionData  The expected cache version
 *
 * @return true if header is valid, false if the header is invalid or doesn't match the versionData
 * THREADING: Pre-req caller holds the cache header write lock
 */
IDATA
OSMemoryMappedCache::verifyCacheHeader()
{
  OSMemoryMappedCacheHeaderMapping* headerMapping = _config->_mapping;
  //  Trc_SHR_OSC_Mmap_isCacheHeaderValid_Entry(header);
  IDATA headerRc;
  //   U_32 headerLen = MMAP_CACHEHEADERSIZE;

  OMRPORT_ACCESS_FROM_OMRPORT(_portLibrary);

  if (strncmp(headerMapping->_eyecatcher, OMRSH_OSCACHE_MMAP_EYECATCHER, OMRSH_OSCACHE_MMAP_EYECATCHER_LENGTH)) {
    Trc_SHR_OSC_Mmap_isCacheHeaderValid_eyecatcherFailed(headerMapping->_eyecatcher, OMRSH_OSCACHE_MMAP_EYECATCHER);
    errorHandler(J9NLS_SHRC_OSCACHE_MMAP_ISCACHEHEADERVALID_EYECATCHER, NULL); /* TODO: Add values */

    OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_CORRUPT_CACHE_HEADER_BAD_EYECATCHER, headerMapping->_eyecatcher);
    setCorruptionContext(CACHE_HEADER_BAD_EYECATCHER, (UDATA)(headerMapping->_eyecatcher));
    return OMRSH_OSCACHE_HEADER_CORRUPT;
  }

  //   if (header->oscHdr.size != _cacheSize) {
  //     Trc_SHR_OSC_Mmap_isCacheHeaderValid_wrongSize(header->oscHdr.size, _cacheSize);
  //     OSC_ERR_TRACE1(J9NLS_SHRC_OSCACHE_CORRUPT_CACHE_HEADER_INCORRECT_CACHE_SIZE, header->oscHdr.size);
  //     setCorruptionContext(CACHE_HEADER_INCORRECT_CACHE_SIZE, (UDATA)header->oscHdr.size);
  //     return OMRSH_OSCACHE_HEADER_CORRUPT;
  //   }
  //
  //   if ((headerRc = checkOSCacheHeader(&(header->oscHdr), versionData, headerLen)) != OMRSH_OSCACHE_HEADER_OK) {
  //     Trc_SHR_OSC_Mmap_isCacheHeaderValid_OSCacheHeaderBad(headerRc);
  //     return headerRc;
  //   }

  Trc_SHR_OSC_Mmap_isCacheHeaderValid_Exit();
  return OMRSH_OSCACHE_HEADER_OK;
}
