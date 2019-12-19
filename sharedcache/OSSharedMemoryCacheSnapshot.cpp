
#include "OSCacheConfigOptions.hpp"
#include "OSCacheImpl.hpp"
#include "OSCacheUtils.hpp"
#include "OSSharedMemoryCacheSnapshot.hpp"
#include "OSSharedMemoryCache.hpp"
#include "OSSharedMemoryCacheUtils.hpp"

#include "shrnls.h"

/**
 * This function restore a non-persistent cache from its snapshot file, startup and walk the restored cache to check for corruption.
 *
 * @param[in] vm The current OMR_VM
 * @param[in] cacheName The name of the cache
 * @param[in] numLocks The number of locks to be initialized
 * @param[in] i Pointer to an initializer to be used to initialize the data area of the new cache
 * @param[in, out] cacheExist True if the cache to be restored already exits, false otherwise
 *
 * @return 0 on success and -1 on failure
 */
IDATA
OSSharedMemoryCacheSnapshot::restoreFromSnapshot(IDATA numLocks)
{
  OMRPORT_ACCESS_FROM_OMRPORT(_cache->_portLibrary);

  IDATA rc = 0;
  char cacheDirName[OMRSH_MAXPATH];		/* OMRSH_MAXPATH defined to be EsMaxPath which is 1024 */
  //  char nameWithVGen[CACHE_ROOT_MAXLEN];	/* CACHE_ROOT_MAXLEN defined to be 88 */
  char* snapshotFileName = snapshotName(); // <= CACHE_ROOT_MAXLEN in length.
  char pathFileName[OMRSH_MAXPATH];
  //  J9PortShcVersion versionData;

  // IDATA fd = 0; // replaces by the member variable _fd.

  const char* ctrlDirName = _cache->_cacheLocation; // for: vm->sharedClassConfig->ctrlDirName;

  Trc_SHR_OSC_Sysv_restoreFromSnapshot_Entry();

  // _verboseFlags = vm->sharedClassConfig->verboseFlags;
  //	setCurrentCacheVersion(vm, J2SE_VERSION(vm), &versionData);
  // versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_SNAPSHOT;

  if (-1 == OSCacheUtils::getCacheDirName(OMRPORTLIB, ctrlDirName, cacheDirName, OMRSH_MAXPATH, _cache->_configOptions))//, OMRPORT_SHR_CACHE_TYPE_SNAPSHOT))
    {
      Trc_SHR_OSC_Sysv_restoreFromSnapshot_getCacheDirFailed();
      OSC_ERR_TRACE(_cache->_configOptions, J9NLS_SHRC_GETSNAPSHOTDIR_FAILED);
      rc = -1;
      goto done;
    }

  // SH_OSCache::getCacheVersionAndGen(OMRPORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, cacheName, &versionData, OSCACHE_CURRENT_CACHE_GEN, false);
  /* No check for the return value of getCachePathName() as it always returns 0 */
  OSCacheUtils::getCachePathName(OMRPORTLIB, cacheDirName, pathFileName, OMRSH_MAXPATH, snapshotFileName);
  _fd = omrfile_open(pathFileName, EsOpenRead | EsOpenWrite, 0);

  if (-1 == _fd) {
    I_32 errorno = omrerror_last_error_number();

    if (OMRPORT_ERROR_FILE_NOENT == errorno) {
      Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileNotFound(pathFileName);
      OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_NOT_FOUND, pathFileName);
    } else {
      const char * errormsg = omrerror_last_error_message();

      Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileOpenFailed(pathFileName);
      OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
      Trc_SHR_Assert_True(errormsg != NULL);
      OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
      OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_ERROR_SNAPSHOT_FILE_OPEN, pathFileName);
    }

    rc = -1;
  } else {
    I_64 fileSize = omrfile_flength(_fd);
    LastErrorInfo lastErrorInfo;
    I_32 openMode = _cache->_configOptions->openMode(); // 0;
    SH_CacheFileAccess cacheFileAccess = OMRSH_CACHE_FILE_ACCESS_ALLOWED;

    // we expect that _configOptions has been configured with the
    // restore settingsprior to this function being called. In J9,
    // this function configures the restored cache as it goes about
    // restoring it.
    //    if (_cache->configOptions->groupAccessEnabled()) { //OMR_ARE_ALL_BITS_SET(vm->sharedClassConfig->runtimeFlags, OMRSHR_RUNTIMEFLAG_ENABLE_GROUP_ACCESS)) {
    //      openMode |= J9OSCACHE_OPEN_MODE_GROUPACCESS;
    //      _config->_groupPerm = 1;
    //    } else {
    //      _config->_groupPerm = 0;
    //    }

    cacheFileAccess = OSCacheUtils::checkCacheFileAccess(OMRPORTLIB, _fd, _cache->_configOptions, &lastErrorInfo);

    if (OMRSH_CACHE_FILE_ACCESS_ALLOWED != cacheFileAccess) {
      switch (cacheFileAccess) {
      case OMRSH_CACHE_FILE_ACCESS_GROUP_ACCESS_REQUIRED:
	OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_OSCACHE_SNAPSHOT_GROUPACCESS_REQUIRED, pathFileName);
	break;
      case OMRSH_CACHE_FILE_ACCESS_OTHERS_NOT_ALLOWED:
	OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_OSCACHE_SNAPSHOT_OTHERS_ACCESS_NOT_ALLOWED, pathFileName);
	break;
      case OMRSH_CACHE_FILE_ACCESS_CANNOT_BE_DETERMINED:
	_cache->printErrorMessage(&lastErrorInfo);
	OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_OSCACHE_SNAPSHOT_INTERNAL_ERROR_CHECKING_CACHEFILE_ACCESS, pathFileName);
	break;
      default:
	Trc_SHR_Assert_ShouldNeverHappen();
      }

      omrfile_close(_fd);
      rc = -1;
      Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileAccessNotAllowed(pathFileName);
      goto done;
    }

    if (!fileSizeWithinBounds()) {
      Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileSizeInvalid(pathFileName, fileSize);
      //    OSC_ERR_TRACE4(_cache->_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_LENGTH, pathFileName, fileSize,
      //		   MIN_CC_SIZE, MAX_CC_SIZE);
      rc = -1;
      /* lock the file to prevent reading and writing */
    } else if (omrfile_lock_bytes(_fd, OMRPORT_FILE_WRITE_LOCK | OMRPORT_FILE_WAIT_FOR_LOCK, 0, fileSize) < 0) {
      I_32 errorno = omrerror_last_error_number();
      const char * errormsg = omrerror_last_error_message();

      Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileLockFailed(pathFileName);
      OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
      Trc_SHR_Assert_True(errormsg != NULL);
      OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
      OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_ERROR_SNAPSHOT_FILE_LOCK, pathFileName);
      rc = -1;
    } else {
      // the commented lines in this section are all J9 specific.

      //OMRSharedCachePreinitConfig* piconfig = vm->sharedCachePreinitConfig;
      //OMR_VMThread* currentThread = omr_vmthread_getCurrent(vm); //vm->internalVMFunctions->currentVMThread(vm);
      bool rcStartup = false;

      //piconfig->sharedClassCacheSize = (UDATA)fileSize;
      //versionData.cacheType = OMRPORT_SHR_CACHE_TYPE_NONPERSISTENT;
      //SH_OSCache::getCacheVersionAndGen(OMRPORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, cacheName, &versionData, OSCACHE_CURRENT_CACHE_GEN, true);
      if (1 == OSCacheUtils::statCache(OMRPORTLIB, cacheDirName, snapshotFileName, false)) {//nameWithVGen, false)) {
#if !defined(WIN32)
	OMRPortShmemStatistic statbuf;
	/* The shared memory may be removed without deleting the control files. So check the existence of the shared memory */
	IDATA ret = OSSharedMemoryCacheUtils::StatSysVMemoryHelper(OMRPORTLIB, cacheDirName, _cache->_configOptions->groupPermissions(), snapshotFileName, &statbuf);

	if (0 == ret) {
#endif /* !defined(WIN32) */
	  // Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheExist1(currentThread);
	  OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_OSCACHE_ERROR_RESTORE_EXISTING_CACHE, _cache->_cacheName);
	  _cacheExists = true;
	  omrfile_close(_fd);
	  rc = -1;
	  goto done;
#if !defined(WIN32)
	}
#endif /* !defined(WIN32) */
      }

      // should no longer be necessary:
      // _cache->_configOptions->_openMode = openMode;
      _cache->_numLocks = numLocks;

      rcStartup = _cache->startup(_cache->_cacheName, ctrlDirName);

      if (false == rcStartup) {
	// Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheStartupFailed1(currentThread);
	OSC_ERR_TRACE(_cache->_configOptions, J9NLS_SHRC_OSCACHE_ERROR_STARTUP_CACHE);
	_cache->destroy(false);

	rc = -1;
      } else if (OMRSH_OSCACHE_CREATED != _cache->getError()) {
	/* Another VM has created the cache */
	OSC_ERR_TRACE1(_cache->_configOptions, J9NLS_SHRC_OSCACHE_ERROR_RESTORE_EXISTING_CACHE, _cache->_cacheName);
	// Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheExist2(currentThread);
	_cacheExists = true;
	rc = -1;
      } else {
	// this is where the implementation-specific (ie. subclass of OSSharedMemoryCache) work takes over.
	rc = restoreFromExistingSnapshot();
	//      SH_CacheMap* cm = (SH_CacheMap *)vm->sharedClassConfig->sharedClassCache;
	//      bool cacheHasIntegrity = false;
	//      I_32 semid = 0;
	//      U_16 theVMCntr = 0;
	//      OSCachesysv_header_version_current*  osCacheSysvHeader = NULL;
	//      OMRSharedCacheHeader* theca = (OMRSharedCacheHeader *)attach(currentThread, &versionData);
	//      IDATA nbytes = (IDATA)fileSize;
	//      IDATA fileRc = 0;
	//
	//      if (NULL == theca) {
	//	Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheAttachFailed(currentThread);
	//	OSC_ERR_TRACE(_configOptions, J9NLS_SHRC_OSCACHE_SHMEM_ATTACH);
	//	destroy(false);
	//	omrfile_close(_fd);
	//	rc = -1;
	//	goto done;
	//      }
	//
	//      osCacheSysvHeader = (OSCachesysv_header_version_current *)(_headerStart);
	//      semid = osCacheSysvHeader->attachedSemid;
	//      theVMCntr = theca->vmCntr;
	//
	//      Trc_SHR_Assert_Equals(theVMCntr, 0);
	//
	//      fileRc = omrfile_read(_fd, osCacheSysvHeader, nbytes);
	//      if (fileRc < 0) {
	//	I_32 errorno = omrerror_last_error_number();
	//	const char * errormsg = omrerror_last_error_message();
	//
	//	Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileReadFailed1(currentThread, pathFileName);
	//	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
	//	Trc_SHR_Assert_True(errormsg != NULL);
	//	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
	//	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_READ, pathFileName);
	//	destroy(false);
	//	omrfile_close(_fd);
	//	rc = -1;
	//	goto done;
	//      } else if (nbytes != fileRc) {
	//	Trc_SHR_OSC_Sysv_restoreFromSnapshot_fileReadFailed2(currentThread, pathFileName, nbytes, fileRc);
	//	OSC_ERR_TRACE1(_configOptions, J9NLS_SHRC_OSCACHE_ERROR_SNAPSHOT_FILE_READ, pathFileName);
	//	destroy(false);
	//	omrfile_close(_fd);
	//	rc = -1;
	//	goto done;
	//      }
	//      theca->vmCntr = theVMCntr;
	//      osCacheSysvHeader->attachedSemid = semid;
	//      /* remove OMRSHR_RUNTIMEFLAG_RESTORE and startup the cache again to check for corruption, cache header will be checked in SH_CacheMap::startup() */
	//      vm->sharedClassConfig->runtimeFlags &= ~OMRSHR_RUNTIMEFLAG_RESTORE;
	//      vm->sharedClassConfig->runtimeFlags |= OMRSHR_RUNTIMEFLAG_RESTORE_CHECK;
	//      /* free memory allocated by SH_OSCachesysv::startup() */
	//      cleanup();
	//      rc = cm->startup(currentThread, piconfig, cacheName, ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, NULL, &cacheHasIntegrity);
	//      /* verboseFlags might be set to 0 in cm->startup(), set it again to ensure the NLS can be printed out */
	//      _verboseFlags = vm->sharedClassConfig->verboseFlags;
	//      if (0 == rc) {
	//	IDATA ret = 0;
	//	LastErrorInfo lastErrorInfo;
	//
	//	/* Header mutex is acquired and not released in the first call of SH_OSCachesysv::startup(), release here */
	//	ret = exitHeaderMutex(&lastErrorInfo);
	//	if (0 == ret) {
	//	  /* set osCacheSysvHeader to current _headerStart as it is detached in cleanup() and re-attached in cm->startup() */
	//	  osCacheSysvHeader = (OSCachesysv_header_version_current *)_headerStart;
	//	  /* To prevent the cache being opened by another JVM in read-only mode, osCacheSysvHeader->oscHdr.cacheInitComplete is always 0
	//	   * before the restoring operation is finished. Set it to 1 here
	//	   */
	//	  osCacheSysvHeader->oscHdr.cacheInitComplete = 1;
	//	} else {
	//	  Trc_SHR_OSC_Sysv_restoreFromSnapshot_headerMutexReleaseFailed(currentThread);
	//	  errorHandler(J9NLS_SHRC_OSCACHE_ERROR_EXIT_HDR_MUTEX, &lastErrorInfo);
	//	  cm->destroy(currentThread);
	//	  rc = -1;
	//	}
	//      } else {
	//	/* if the restored cache is corrupted, it is destroyed in SH_CacheMap::startup() */
	//	Trc_SHR_OSC_Sysv_restoreFromSnapshot_cacheStartupFailed2(currentThread);
	//      }
      }
      /* file lock will be released when closed */
      omrfile_close(_fd);
    }
  }

 done:
  Trc_SHR_OSC_Sysv_restoreFromSnapshot_Exit(rc);
  return rc;
}
